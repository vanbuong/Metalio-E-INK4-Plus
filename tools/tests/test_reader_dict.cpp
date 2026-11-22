// Host tests for the reader dictionary feature.
// Run from the repository root: python tools/tests/test_reader_dict.py
#include <cstdio>
#include <string>
#include <vector>

#include "text_encoding.h"
#include "text_spacing.h"
#include "text_glyph_pen.h"

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

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    test_glyph_pen_matches_spacing_width();
    if (g_failures) {
        std::printf("%d check(s) failed\n", g_failures);
        return 1;
    }
    return 0;
}
