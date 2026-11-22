#include "page_word_hit.h"

#include <vector>

#include "page_line_walk.h"
#include "text_encoding.h"
#include "text_glyph_pen.h"

namespace reader {
namespace {

constexpr int kHitSlackPx = 4; // 行首前、行尾后仍算命中首末字的像素

struct Cp {
    uint32_t cp;
    size_t offset;
    size_t size;
};

bool IsWordLetter(uint32_t cp) {
    if ((cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z')) return true;
    if (cp >= 0x00C0 && cp <= 0x024F) return cp != 0x00D7 && cp != 0x00F7;
    if (cp >= 0x0300 && cp <= 0x036F) return true; // 组合附加符（分解形式的越南文）
    return cp >= 0x1E00 && cp <= 0x1EFF;
}

bool IsWordJoiner(uint32_t cp) { return cp == '\'' || cp == 0x2019 || cp == '-'; }

std::vector<Cp> DecodeLine(std::string_view line) {
    std::vector<Cp> out;
    const auto* p = reinterpret_cast<const uint8_t*>(line.data());
    for (size_t i = 0; i < line.size();) {
        uint32_t cp = 0;
        const size_t n = Utf8Next(p + i, line.size() - i, &cp);
        if (n == 0) break;
        out.push_back({cp, i, n});
        i += n;
    }
    return out;
}

bool IsInnerJoiner(const std::vector<Cp>& cps, size_t i) {
    return IsWordJoiner(cps[i].cp) && i > 0 && i + 1 < cps.size() &&
        IsWordLetter(cps[i - 1].cp) && IsWordLetter(cps[i + 1].cp);
}

}  // namespace

std::string WordAround(std::string_view line, size_t byte_offset) {
    const auto cps = DecodeLine(line);
    size_t at = cps.size();
    for (size_t i = 0; i < cps.size(); ++i) {
        if (byte_offset >= cps[i].offset && byte_offset < cps[i].offset + cps[i].size) { at = i; break; }
    }
    if (at == cps.size()) return {};
    if (!IsWordLetter(cps[at].cp) && !IsInnerJoiner(cps, at)) return {};
    size_t first = at, last = at;
    while (first > 0 && (IsWordLetter(cps[first - 1].cp) || IsInnerJoiner(cps, first - 1))) --first;
    while (last + 1 < cps.size() && (IsWordLetter(cps[last + 1].cp) || IsInnerJoiner(cps, last + 1))) ++last;
    std::string word;
    for (size_t i = first; i <= last; ++i) {
        if (cps[i].cp == 0x2019) word += '\'';
        else word.append(line.substr(cps[i].offset, cps[i].size));
    }
    return word;
}

bool PageWordAt(const Page& page, const lv_font_t* font, int spacing_percent, lv_coord_t para_gap,
                int page_w, int page_h, const TextStyle& style, int x, int y, PageWordHit& out) {
    out.word.clear();
    if (x < 0 || y < 0 || x >= page_w || y >= page_h) return false;
    ForEachPageTextLine(page, font, spacing_percent, para_gap, page_w, page_h, style,
        [&](const PageTextLine& line) {
            if (y < line.cell_top || y >= line.cell_top + line.cell_h) return true;
            const std::string& text = *line.text;
            HorizontalGlyphPen pen(font, text.data(), text.size(), line.x, line.spacing, style.letter_gap);
            size_t hit = std::string::npos, first = std::string::npos, last = std::string::npos;
            int first_x = 0, last_end = 0;
            const auto* p = reinterpret_cast<const uint8_t*>(text.data());
            for (size_t i = 0; i < text.size();) {
                uint32_t cp = 0;
                const size_t n = Utf8Next(p + i, text.size() - i, &cp);
                if (n == 0) break;
                lv_font_glyph_dsc_t glyph{};
                const bool known = lv_font_get_glyph_dsc(font, &glyph, cp, 0);
                int gx = 0, adv = 0;
                pen.Step(cp, glyph, known, &gx, &adv);
                if (adv > 0) {
                    if (first == std::string::npos) { first = i; first_x = gx; }
                    last = i;
                    last_end = gx + adv;
                    if (x >= gx && x < gx + adv) { hit = i; break; }
                }
                i += n;
            }
            if (hit == std::string::npos && first != std::string::npos) {
                if (x < first_x && first_x - x <= kHitSlackPx) hit = first;
                else if (x >= last_end && x - last_end < kHitSlackPx) hit = last;
            }
            if (hit != std::string::npos) out.word = WordAround(text, hit);
            return false; // 只有一行覆盖该 y
        });
    return !out.word.empty();
}

}  // namespace reader
