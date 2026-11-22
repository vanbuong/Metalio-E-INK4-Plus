// Host tests for the reader's horizontal line-break and justification rules.
// Run from the repository root: python tools/tests/test_text_break.py
#include <cstdio>
#include <string>
#include <vector>

#include "vertical_text.h"
#include "book_margin_rules.h"
#include "screen_cover_rules.h"
#include "system_font_rules.h"

using namespace reader;

static int g_failures = 0;
#define CHECK(cond)                                                             \
    do {                                                                        \
        if (!(cond)) {                                                          \
            std::printf("%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
            ++g_failures;                                                       \
        }                                                                       \
    } while (0)

struct Cp {
    uint32_t cp;
    uint32_t offset;
};

// Minimal UTF-8 decoder for well-formed test strings.
static std::vector<Cp> Decode(const std::string& s) {
    std::vector<Cp> out;
    for (size_t i = 0; i < s.size();) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        uint32_t cp = c;
        size_t n = 1;
        if (c >= 0xF0) { cp = c & 0x07; n = 4; }
        else if (c >= 0xE0) { cp = c & 0x0F; n = 3; }
        else if (c >= 0xC0) { cp = c & 0x1F; n = 2; }
        for (size_t k = 1; k < n; ++k) cp = (cp << 6) | (static_cast<unsigned char>(s[i + k]) & 0x3F);
        out.push_back({cp, static_cast<uint32_t>(i)});
        i += n;
    }
    return out;
}

// Offsets at which a fresh VerticalBreakState records a new legal break.
static std::vector<uint32_t> Breaks(const std::string& text, bool words) {
    std::vector<uint32_t> found;
    VerticalBreakState state;
    for (const Cp& c : Decode(text)) {
        const bool had = state.valid;
        const uint32_t before = state.offset;
        state.Observe(c.cp, c.offset, c.offset, words);
        if (state.valid && (!had || state.offset != before)) found.push_back(state.offset);
    }
    return found;
}

static bool Contains(const std::vector<uint32_t>& v, uint32_t x) {
    for (uint32_t y : v) if (y == x) return true;
    return false;
}

static void english_words() {
    const std::vector<uint32_t> b = Breaks("the quick brown", true);
    CHECK(b.size() == 2);
    CHECK(Contains(b, 4));   // before "quick"
    CHECK(Contains(b, 10));  // before "brown"
}

static void no_break_inside_word() {
    const std::string w = "TemperatureSensor";
    for (size_t i = 0; i + 1 < w.size(); ++i) CHECK(!CanBreakBetween(w[i], w[i + 1], false));
    CHECK(Breaks(w, true).empty());
}

static void hyphen_and_dash() {
    CHECK(CanBreakBetween('-', 'k', false));        // well-|known
    CHECK(CanBreakBetween(0x2014, 'a', false));     // em dash then word
    CHECK(!CanBreakBetween('-', ' ', false));
    CHECK(!CanBreakBetween(0x2014, 0x2014, true));  // Chinese ——
}

static void joined_marks() {
    CHECK(!CanBreakBetween('n', 0x2019, false));  // don’t
    CHECK(!CanBreakBetween(0x2019, 't', false));
    CHECK(!CanBreakBetween('3', '.', false));     // 3.14
    CHECK(!CanBreakBetween('.', '1', false));
    CHECK(!CanBreakBetween('U', '.', false));     // U.S.
    CHECK(!CanBreakBetween('.', 'S', false));
}

static void nbsp() {
    CHECK(!CanBreakBetween(0x00A0, 'S', false));  // Mr. Smith
    CHECK(!CanBreakBetween('.', 0x00A0, false));
    CHECK(Breaks("Mr.\xC2\xA0Smith", true).empty());
}

static void spaces() {
    CHECK(CanBreakBetween(' ', 'a', false));
    CHECK(!CanBreakBetween(' ', ' ', false));
    CHECK(CanBreakBetween('\t', 'a', false));
    CHECK(IsBreakingSpace(' ') && IsBreakingSpace('\t') && !IsBreakingSpace(0x00A0));
}

static void vietnamese() {
    // "Tiếng Việt" precomposed: only one break, before "V".
    const std::string text = "Ti\xE1\xBA\xBFng Vi\xE1\xBB\x87t";
    const std::vector<uint32_t> b = Breaks(text, true);
    CHECK(b.size() == 1);
    CHECK(b.size() == 1 && text[b[0]] == 'V');
    CHECK(ClassifyBreak(0x1EC7) == BreakClass::kLatin);
    // NFD: e + combining acute stays together and the mark may hang.
    CHECK(!CanBreakBetween('e', 0x0301, false));
    CHECK(!CanBreakBetween(0x0301, 'n', false));
    CHECK(IsHangingGlyph(0x0301));
    CHECK(IsHangingGlyph(' '));
    CHECK(!IsHangingGlyph('a'));
}

static void chinese_unchanged() {
    const std::vector<Cp> cps = Decode(
        "\xE4\xB8\xAD\xE6\x96\x87\xEF\xBC\x8C\xE2\x80\x9C\xE5\xBC\x95\xE5\x8F\xB7\xE2\x80\x9D"  // 中文，“引号”
        "\xE2\x80\x94\xE2\x80\x94\xE6\xB5\x8B\xE8\xAF\x95\xE3\x80\x82\xE2\x80\x9D\xE2\x80\x9C"  // ——测试。”“
        "\xE4\xBD\xA0\xE5\xA5\xBD" "2023" "\xE5\xB9\xB4\xE2\x80\xA6\xEF\xBC\x88\xE6\x8B\xAC"    // 你好2023年…（括
        "\xE5\x8F\xB7\xEF\xBC\x89");                                                             // 号）
    bool context = true;
    for (size_t i = 0; i + 1 < cps.size(); ++i) {
        context = NextCjkContext(context, cps[i].cp);
        CHECK(CanBreakBetween(cps[i].cp, cps[i + 1].cp, context) == VerticalCanBreak(cps[i].cp, cps[i + 1].cp));
    }
    // Vertical mode (words=false) keeps the old per-character behaviour.
    CHECK(!Breaks("TemperatureSensor", false).empty());
    CHECK(ClassifyBreak(0x4E2D) == BreakClass::kCjk);
    CHECK(ClassifyBreak(0xFF0C) == BreakClass::kCjk);
    CHECK(ClassifyBreak(0x201C) == BreakClass::kNeutral);
    CHECK(ClassifyBreak('7') == BreakClass::kNeutral);
}

static void mixed() {
    // 中文English中文: E at byte 6, second 中 at byte 13.
    const std::vector<uint32_t> b = Breaks("\xE4\xB8\xAD\xE6\x96\x87" "English" "\xE4\xB8\xAD\xE6\x96\x87", true);
    CHECK(Contains(b, 6));
    CHECK(Contains(b, 13));
    for (uint32_t off = 7; off < 13; ++off) CHECK(!Contains(b, off));
}

static void line_local() {
    CHECK(VerticalBreakState{}.cjk_context);
    CHECK(Breaks("so humans could get it", true) == Breaks("so humans could get it", true));
}

// Counts stretch slots the way ObserveTextGlyph does: decide with the context before cp.
static int Slots(const std::string& text) {
    int slots = 0;
    uint32_t previous = 0;
    bool context = true;
    for (const Cp& c : Decode(text)) {
        if (previous && JustifySlot(previous, c.cp, context)) ++slots;
        context = NextCjkContext(context, c.cp);
        previous = c.cp;
    }
    return slots;
}

static void justify_slots_latin() {
    CHECK(Slots("a  quick brown") == 2);  // the double space counts once
    CHECK(Slots("a quick word ") == 2);   // a trailing space adds nothing
    CHECK(Slots("TemperatureSensor") == 0);
}

static void justify_slots_cjk_unchanged() {
    const std::vector<Cp> cps = Decode("\xE4\xB8\xAD\xE6\x96\x87\xEF\xBC\x8C\xE6\xB5\x8B\xE8\xAF\x95\xE3\x80\x82");  // 中文，测试。
    for (size_t i = 1; i < cps.size(); ++i) {
        CHECK(JustifySlot(cps[i - 1].cp, cps[i].cp, true) == !VerticalClosing(cps[i].cp));
    }
}

static void squeeze_latin_off() {
    TextSpacingState s;
    s.capacity = 10;
    s.tail_blank = 2;
    s.cjk_context = false;
    CHECK(!CanFitTrailingPunctuation('.', 101, 100, s));
    s.cjk_context = true;
    CHECK(CanFitTrailingPunctuation('.', 101, 100, s));
}

// Context a drawer seeds from the whole line before measuring it.
static bool SeedContext(const std::string& text) {
    const std::vector<Cp> cps = Decode(text);
    size_t i = 0;
    return LineCjkContext([&](uint32_t& cp) {
        if (i >= cps.size()) return false;
        cp = cps[i++].cp;
        return true;
    });
}

static int SeededSlots(const std::string& text) {
    int slots = 0;
    uint32_t previous = 0;
    bool context = SeedContext(text);
    for (const Cp& c : Decode(text)) {
        if (previous && JustifySlot(previous, c.cp, context)) ++slots;
        context = NextCjkContext(context, c.cp);
        previous = c.cp;
    }
    return slots;
}

static void zhuyin_tone_marks_neutral() {
    for (uint32_t cp : {0x02C7u, 0x02CAu, 0x02CBu, 0x02D9u}) CHECK(ClassifyBreak(cp) == BreakClass::kNeutral);
    CHECK(ClassifyBreak(0x0259) == BreakClass::kLatin);  // ə stays Latin
}

static void hanging_only_in_latin_context() {
    VerticalBreakState s;
    CHECK(!HangsAtLineEnd(s, ' '));  // CJK context: space breaks as before
    s.cjk_context = false;
    CHECK(HangsAtLineEnd(s, ' '));
    CHECK(HangsAtLineEnd(s, 0x0301));
    CHECK(!HangsAtLineEnd(s, 'a'));
}

static void line_start_digits() {
    CHECK(!SeedContext("1990 was a year"));
    CHECK(SeedContext("2023\xE5\xB9\xB4"));  // 2023年
    CHECK(SeedContext("1234"));              // no strong character: CJK default
    CHECK(SeededSlots("1990 was a year") == 3);
}

static void sysfont_ttf_names() {
    using sysfont::TtfStyle;
    auto r = sysfont::ParseTtfName("Bookerly.ttf");
    CHECK(r.ok && r.family == "Bookerly" && r.style == TtfStyle::kRegular);
    r = sysfont::ParseTtfName("Bookerly Bold.ttf");
    CHECK(r.ok && r.family == "Bookerly" && r.style == TtfStyle::kBold);
    r = sysfont::ParseTtfName("Bookerly-Regular.TTF");
    CHECK(r.ok && r.family == "Bookerly" && r.style == TtfStyle::kRegular);
    r = sysfont::ParseTtfName("Bookerly-BoldItalic.ttf");
    CHECK(r.ok && r.style == TtfStyle::kOther);
    r = sysfont::ParseTtfName("Bookerly Bold Italic.ttf");
    CHECK(r.ok && r.style == TtfStyle::kOther);
    r = sysfont::ParseTtfName("Bookerly Italic.otf");
    CHECK(r.ok && r.style == TtfStyle::kOther);
    r = sysfont::ParseTtfName("Bookerly Display Bold.ttf");
    CHECK(r.ok && r.family == "Bookerly Display" && r.style == TtfStyle::kBold);
    r = sysfont::ParseTtfName("Noto Sans-SemiBold.ttf");
    CHECK(r.ok && r.family == "Noto Sans" && r.style == TtfStyle::kBold);
    CHECK(!sysfont::ParseTtfName("Bookerly.zip").ok);
    CHECK(!sysfont::ParseTtfName(".ttf").ok);
}

static void sysfont_pick_files() {
    const std::vector<std::string> names = {"Bookerly Italic.ttf", "bookerly bold.ttf", "Bookerly.ttf",
                                            "Literata-Bold.ttf"};
    auto f = sysfont::PickTtfFiles(names, "Bookerly");
    CHECK(f.regular == "Bookerly.ttf" && f.bold == "bookerly bold.ttf");
    f = sysfont::PickTtfFiles(names, "Literata");
    CHECK(f.regular.empty());  // no regular file: family not usable
    f = sysfont::PickTtfFiles({"Bookerly.ttf"}, "BOOKERLY");
    CHECK(f.regular == "Bookerly.ttf" && f.bold.empty());
}

static void sysfont_ef_names() {
    auto e = sysfont::ParseEfName("literata_30_2.ef");
    CHECK(e.ok && e.family == "literata" && e.px == 30 && e.bpp == 2);
    e = sysfont::ParseEfName("font_misans_regular_25_2.ef");
    CHECK(e.ok && e.family == "font_misans_regular" && !sysfont::IsBuiltinFamily(e.family));
    CHECK(sysfont::IsBuiltinFamily("Literata") && sysfont::IsBuiltinFamily("LiterataBook"));
    CHECK(!sysfont::IsBuiltinFamily("MiSans") && !sysfont::IsBuiltinFamily("Bookerly"));
    CHECK(!sysfont::ParseEfName("literata.ef").ok);
    CHECK(!sysfont::ParseEfName("literata_30_2.ttf").ok);
    CHECK(sysfont::EfRoleFile("literata", 30, 4) == "literata_30_4.ef");
}

static void sysfont_roles() {
    CHECK(sysfont::RoleIndex(30, 2) == 0 && sysfont::RoleIndex(30, 4) == 1);
    CHECK(sysfont::RoleIndex(25, 2) == 2 && sysfont::RoleIndex(18, 2) == 3);
    CHECK(sysfont::RoleIndex(28, 2) < 0 && sysfont::RoleIndex(36, 2) < 0 && sysfont::RoleIndex(30, 1) < 0);
}

static void sysfont_coverage_choice() {
    CHECK(sysfont::InCoverage('A') && sysfont::InCoverage(0x7E) && !sysfont::InCoverage(0x7F));
    CHECK(sysfont::InCoverage(0x1EA0) && sysfont::InCoverage(0x1EF9) && !sysfont::InCoverage(0x1EFA));
    CHECK(sysfont::InCoverage(0x01B0) && !sysfont::InCoverage(0x4E2D));  // ư yes, 中 no
    CHECK(sysfont::InClockCoverage('0') && sysfont::InClockCoverage('9') && sysfont::InClockCoverage(':'));
    CHECK(sysfont::InClockCoverage(' ') && !sysfont::InClockCoverage('A') && !sysfont::InClockCoverage('/'));
    CHECK(sysfont::kClockSize == 160);
    auto c = sysfont::ParseChoice("");
    CHECK(c.kind == sysfont::FontKind::kBuiltin);  // absent → built-in Literata
    c = sysfont::ParseChoice("ttf:Bookerly");
    CHECK(c.kind == sysfont::FontKind::kTtf && c.family == "Bookerly");
    c = sysfont::ParseChoice("builtin");
    CHECK(c.kind == sysfont::FontKind::kBuiltin);
    c = sysfont::ParseChoice("ef:literata");
    CHECK(c.kind == sysfont::FontKind::kEf && c.family == "literata");
    c = sysfont::ParseChoice("ttf:");
    CHECK(c.kind == sysfont::FontKind::kBuiltin);  // malformed → default
    CHECK(sysfont::ParseChoice("junk").kind == sysfont::FontKind::kBuiltin);
    CHECK(sysfont::FormatChoice({sysfont::FontKind::kTtf, "Bookerly"}) == "ttf:Bookerly");
    CHECK(sysfont::FormatChoice({sysfont::FontKind::kBuiltin, ""}) == "builtin");
}

static void fit_to_panel() {
    int w = 0, h = 0;
    CHECK(FitToPanel(600, 800, 684, 1216, &w, &h) && w == 684 && h == 912);
    CHECK(FitToPanel(320, 428, 684, 1216, &w, &h) && w == 684 && h == 914);
    CHECK(FitToPanel(2000, 1000, 684, 1216, &w, &h) && w == 684 && h == 342);
    CHECK(FitToPanel(100, 150, 684, 1216, &w, &h) && w == 684 && h == 1026);
    CHECK(FitToPanel(684, 1216, 684, 1216, &w, &h) && w == 684 && h == 1216);
    CHECK(!FitToPanel(0, 800, 684, 1216, &w, &h));
    CHECK(!FitToPanel(600, 800, 684, 1216, nullptr, &h));
}

static void resize_bilinear() {
    const uint8_t checker[4] = {0, 255, 255, 0};
    uint8_t big[16] = {};
    ResizeL8Bilinear(checker, 2, 2, big, 4, 4);
    CHECK(big[0] == 0 && big[3] == 255 && big[12] == 255 && big[15] == 0);
    uint8_t flat[9];
    for (uint8_t& p : flat) p = 128;
    uint8_t out[35] = {};
    ResizeL8Bilinear(flat, 3, 3, out, 7, 5);
    bool all = true;
    for (uint8_t p : out) all = all && p == 128;
    CHECK(all);
    const uint8_t one[1] = {77};
    uint8_t six[6] = {};
    ResizeL8Bilinear(one, 1, 1, six, 3, 2);
    bool same = true;
    for (uint8_t p : six) same = same && p == 77;
    CHECK(same);
}

static void tag_matches() {
    const char* book = "/sdcard/a.epub";
    CHECK(ScreenCoverTagMatches("/sdcard/a.epub", 14, book));
    CHECK(ScreenCoverTagMatches("/sdcard/a.epub\n", 15, book));
    CHECK(ScreenCoverTagMatches("/sdcard/a.epub\r\n", 16, book));
    CHECK(!ScreenCoverTagMatches("/sdcard/b.epub", 14, book));
    CHECK(!ScreenCoverTagMatches("/sdcard/a.ep", 12, book));
    CHECK(!ScreenCoverTagMatches("", 0, book));
    CHECK(!ScreenCoverTagMatches("/sdcard/a.epub", 14, nullptr));
}

static void cover_usable() {
    const char* book = "/sdcard/a.epub";
    CHECK(ScreenCoverUsable(book, book, true, 3, 3));
    CHECK(!ScreenCoverUsable(book, "/sdcard/b.epub", true, 3, 3));  // a newer book is open
    CHECK(!ScreenCoverUsable(book, book, false, 3, 3));             // card removed
    CHECK(!ScreenCoverUsable(book, book, true, 3, 4));              // card re-mounted / swapped
    CHECK(!ScreenCoverUsable("", "", true, 3, 3));                  // nothing prepared
    CHECK(!ScreenCoverUsable(nullptr, book, true, 3, 3));
}

static void margin_rules() {
    CHECK(ReaderSymmetricSideMargin(16, 24) == 24);
    CHECK(ReaderSymmetricSideMargin(56, 56) == 40);
    CHECK(ReaderSymmetricSideMargin(0, 3) == 5);
    CHECK(ReaderSymmetricSideMargin(30, 30) == 30);
}

int main() {
    sysfont_ttf_names();
    sysfont_pick_files();
    sysfont_ef_names();
    sysfont_roles();
    sysfont_coverage_choice();
    fit_to_panel();
    resize_bilinear();
    tag_matches();
    cover_usable();
    zhuyin_tone_marks_neutral();
    hanging_only_in_latin_context();
    line_start_digits();
    margin_rules();
    justify_slots_latin();
    justify_slots_cjk_unchanged();
    squeeze_latin_off();
    english_words();
    no_break_inside_word();
    hyphen_and_dash();
    joined_marks();
    nbsp();
    spaces();
    vietnamese();
    chinese_unchanged();
    mixed();
    line_local();
    if (g_failures) {
        std::printf("%d check(s) failed\n", g_failures);
        return 1;
    }
    return 0;
}
