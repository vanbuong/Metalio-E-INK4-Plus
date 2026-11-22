// Host tests for the reader dictionary feature.
// Run from the repository root: python tools/tests/test_reader_dict.py
#include <cstdio>
#include <string>
#include <vector>

#include "text_encoding.h"
#include "text_spacing.h"
#include "text_glyph_pen.h"
#include "page_line_walk.h"
#include "page_word_hit.h"

using namespace reader;

static int g_failures = 0;
#define CHECK(cond)                                                             \
    do {                                                                        \
        if (!(cond)) {                                                          \
            std::printf("%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
            ++g_failures;                                                       \
        }                                                                       \
    } while (0)

// ---- Monospace test font: every glyph advances `adv`, ink box is adv-1 wide. ----
static int g_mono_adv = 10;
static bool MonoGlyph(const lv_font_t* font, lv_font_glyph_dsc_t* dsc, uint32_t cp, uint32_t) {
    dsc->adv_w = static_cast<uint16_t>(g_mono_adv);
    const bool blank = cp == ' ' || cp == '\t';
    dsc->box_w = blank ? 0 : static_cast<uint16_t>(g_mono_adv - 1);
    dsc->box_h = blank ? 0 : static_cast<uint16_t>(font->line_height / 2);
    dsc->ofs_x = 0;
    dsc->ofs_y = 0;
    return true;
}

static lv_font_t MakeMonoFont(int adv, int line_height) {
    g_mono_adv = adv;
    lv_font_t font;
    font.get_glyph_dsc = MonoGlyph;
    font.line_height = line_height;
    font.base_line = line_height / 4;
    return font;
}

struct PenGlyph {
    uint32_t cp;
    int x;
    int adv;
};

static std::vector<PenGlyph> WalkPen(const lv_font_t& font, const std::string& s, int x,
                                     const TextLineSpacing& spacing, int letter_gap, int* end_x) {
    std::vector<PenGlyph> out;
    HorizontalGlyphPen pen(&font, s.data(), s.size(), x, spacing, letter_gap);
    for (size_t i = 0; i < s.size();) {
        uint32_t cp = 0;
        const size_t n = Utf8Next(reinterpret_cast<const uint8_t*>(s.data()) + i, s.size() - i, &cp);
        if (n == 0) break;
        i += n;
        lv_font_glyph_dsc_t g{};
        const bool known = lv_font_get_glyph_dsc(&font, &g, cp, 0);
        PenGlyph pg{cp, 0, 0};
        pen.Step(cp, g, known, &pg.x, &pg.adv);
        out.push_back(pg);
    }
    *end_x = pen.EndX();
    return out;
}

static void test_glyph_pen_matches_spacing_width() {
    const lv_font_t font = MakeMonoFont(10, 20);
    const std::string s = "Hello world";
    // Left-aligned: no justification, 'w' sits after "Hello " = 6 cells.
    {
        const auto spacing = MakeTextLineSpacing(&font, s, 200, false);
        int end_x = 0;
        const auto glyphs = WalkPen(font, s, 7, spacing, 0, &end_x);
        CHECK(glyphs.size() == s.size());
        CHECK(glyphs.front().x == 7);
        CHECK(glyphs[6].cp == 'w');
        CHECK(glyphs[6].x == 7 + 6 * 10);
        CHECK(end_x - 7 == spacing.width);
    }
    // Justified to 200 px: positions grow monotonically and the line ends at the spacing width.
    {
        const auto spacing = MakeTextLineSpacing(&font, s, 200, true);
        CHECK(spacing.width == 200);
        int end_x = 0;
        const auto glyphs = WalkPen(font, s, 0, spacing, 0, &end_x);
        CHECK(glyphs.front().x == 0);
        for (size_t i = 1; i < glyphs.size(); ++i) CHECK(glyphs[i].x > glyphs[i - 1].x);
        CHECK(glyphs[6].x > 6 * 10);  // the space gap was stretched
        CHECK(end_x == spacing.width);
    }
    // Letter gap is part of every advance.
    {
        const auto spacing = MakeTextLineSpacing(&font, s, 400, false, false, 2);
        int end_x = 0;
        const auto glyphs = WalkPen(font, s, 0, spacing, 2, &end_x);
        CHECK(glyphs[1].x == 12);
        CHECK(glyphs[1].adv == 12);
    }
}

static size_t OffsetOf(const std::string& s, const char* needle, size_t extra = 0) {
    return s.find(needle) + extra;
}

static void test_word_around() {
    const std::string quote = "He said, \xE2\x80\x9C" "don\xE2\x80\x99t go.\xE2\x80\x9D";
    CHECK(WordAround(quote, OffsetOf(quote, "don", 1)) == "don't");
    CHECK(WordAround(quote, OffsetOf(quote, "t go")) == "don't");
    CHECK(WordAround(quote, OffsetOf(quote, ",")) == "");
    CHECK(WordAround(quote, OffsetOf(quote, " go")) == "");
    const std::string hyphen = "a well-known end-";
    CHECK(WordAround(hyphen, OffsetOf(hyphen, "well")) == "well-known");
    CHECK(WordAround(hyphen, OffsetOf(hyphen, "known", 2)) == "well-known");
    CHECK(WordAround(hyphen, OffsetOf(hyphen, "end")) == "end");
    CHECK(WordAround(hyphen, OffsetOf(hyphen, "-", 0)) == "well-known");  // the inner hyphen itself
    const std::string vi = "m\xE1\xBB\x99t ng\xC6\xB0\xE1\xBB\x9Di";  // "một người"
    CHECK(WordAround(vi, OffsetOf(vi, "ng")) == "ng\xC6\xB0\xE1\xBB\x9Di");
    const std::string cjk = "\xE4\xB8\xAD\xE6\x96\x87";  // 中文
    CHECK(WordAround(cjk, 0) == "");
    CHECK(WordAround("John's", 0) == "John's");
    CHECK(WordAround("", 0) == "");
    CHECK(WordAround("abc", 99) == "");
}

static Page ThreeLinePage() {
    Page page;
    PageItem first;
    first.text = "The quick brown fox";
    first.para_indent = true;
    first.justify = true;
    PageItem second;
    second.text = "jumps over the lazy";
    second.justify = true;
    PageItem third;
    third.text = "dog.";
    third.para_gap_before = true;
    page.items = {first, second, third};
    return page;
}

// Mono font adv 10, line height 20, page 300x400, spacing 10 %, para gap 56:
// 16 rows of 25 px, text offset 0, indent 20 px; rows at y 0, 25, then 50 + 56 = 106.
static void test_line_walk_rows_match_raster_rules() {
    const lv_font_t font = MakeMonoFont(10, 20);
    const Page page = ThreeLinePage();
    std::vector<PageTextLine> lines;
    const bool ok = ForEachPageTextLine(page, &font, 10, 56, 300, 400, TextStyle{},
        [&](const PageTextLine& line) { lines.push_back(line); return true; });
    CHECK(ok);
    CHECK(lines.size() == 3);
    if (lines.size() != 3) return;
    CHECK(lines[0].cell_top == 0 && lines[0].cell_h == 25 && lines[0].line_y == 0);
    CHECK(lines[1].cell_top == 25 && lines[1].line_y == 25);
    CHECK(lines[2].cell_top == 106 && lines[2].line_y == 106);
    CHECK(lines[0].x == 20);  // paragraph indent
    CHECK(lines[1].x == 0);
    CHECK(*lines[1].text == "jumps over the lazy");
    CHECK(lines[1].spacing.width == 300);  // justified
    CHECK(lines[0].spacing.width == 280);  // justified within the indent
    CHECK(!lines[0].faux_bold);
}

static int GlyphX(const lv_font_t& font, const PageTextLine& line, size_t byte_offset) {
    int end_x = 0;
    const auto glyphs = WalkPen(font, *line.text, line.x, line.spacing, 0, &end_x);
    size_t at = 0;
    for (const auto& g : glyphs) {
        if (at == byte_offset) return g.x;
        at += g.cp < 0x80 ? 1 : g.cp < 0x800 ? 2 : g.cp < 0x10000 ? 3 : 4;
    }
    return -1;
}

static void test_page_word_at() {
    const lv_font_t font = MakeMonoFont(10, 20);
    const Page page = ThreeLinePage();
    std::vector<PageTextLine> lines;
    ForEachPageTextLine(page, &font, 10, 56, 300, 400, TextStyle{},
        [&](const PageTextLine& line) { lines.push_back(line); return true; });
    if (lines.size() != 3) { CHECK(lines.size() == 3); return; }
    auto at = [&](int x, int y) {
        PageWordHit hit;
        PageWordAt(page, &font, 10, 56, 300, 400, TextStyle{}, x, y, hit);
        return hit.word;
    };
    const int q = GlyphX(font, lines[0], 4);
    CHECK(at(q + 3, 10) == "quick");
    const int space = GlyphX(font, lines[0], 9);
    const int brown = GlyphX(font, lines[0], 10);
    CHECK(space >= 0 && brown > space);
    CHECK(at(space + 1, 10) == "");  // on the stretched gap between words
    CHECK(at(brown + 1, 10) == "brown");
    const int lazy = GlyphX(font, lines[1], 15);
    CHECK(at(lazy + 2, 30) == "lazy");
    CHECK(at(lines[0].x - 3, 10) == "The");   // within slack before the line start
    CHECK(at(lines[0].x - 9, 10) == "");      // beyond slack
    CHECK(at(15, 80) == "");                  // paragraph gap
    CHECK(at(5, 110) == "dog");
    CHECK(at(-1, 10) == "");
    CHECK(at(10, 401) == "");
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    test_glyph_pen_matches_spacing_width();
    test_word_around();
    test_line_walk_rows_match_raster_rules();
    test_page_word_at();
    if (g_failures) {
        std::printf("%d check(s) failed\n", g_failures);
        return 1;
    }
    return 0;
}
