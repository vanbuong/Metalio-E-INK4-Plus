// Host tests for the reader's horizontal line-break and justification rules.
// Run from the repository root: python tools/tests/test_text_break.py
#include <cstdio>
#include <string>
#include <vector>

#include "vertical_text.h"
#include "book_margin_rules.h"

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

static void margin_rules() {
    CHECK(ReaderSymmetricSideMargin(16, 24) == 24);
    CHECK(ReaderSymmetricSideMargin(56, 56) == 40);
    CHECK(ReaderSymmetricSideMargin(0, 3) == 5);
    CHECK(ReaderSymmetricSideMargin(30, 30) == 30);
}

int main() {
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
