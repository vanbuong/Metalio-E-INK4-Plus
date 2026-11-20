# Reader Word Wrap, Alignment & Symmetric Margins Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** English/Vietnamese text in the e-book reader wraps at word boundaries, justifies only between words, offers a Justified/Left alignment setting, and uses one symmetric 5–40 px side margin — with Chinese layout unchanged.

**Architecture:** The shared, header-only break/spacing state in `main/reader/vertical_text.h` gains a script-aware rule (CJK / Latin / Neutral classes with a line-local context). The three horizontal layout loops in `book_session.cc` pass a "words" flag and let whitespace and combining marks hang. Alignment is a draw-time pref carried in `reader::TextStyle`; margins are enforced symmetric in the prefs layer.

**Tech Stack:** C++17, ESP-IDF v6.1 (esp32s31), LVGL 9; host tests: g++ (MinGW 13) driven by Python 3.

**Spec:** `docs/specs/2026-11-21-reader-word-wrap-design.md`

## Global Constraints

- Vertical writing mode: breaking and spacing exactly as today (only its side margins become symmetric).
- Chinese text runs (incl. their digits, curly quotes, `——`) must break and justify exactly as the old `VerticalCanBreak` rule.
- `main/reader/vertical_text.h` stays free of LVGL/IDF includes (`<cstddef>`, `<cstdint>`, `<algorithm>` only) — the PC emulator and the host test compile it.
- Side margin range 5–40 px; top/bottom keep 0–64.
- Alignment default **Justified**; alignment is never part of any pagination cache key.
- NVS: existing `BookTextOption` indices and keys keep their meaning.
- Match surrounding style: Chinese `//` comments in `main/reader/` and `book_screen/`; no `sdkconfig` edits; don't touch `components/`.
- Build check: `./tools/idf.ps1 build` exit 0 and no new warnings, before each commit that touches firmware code.
- Host tests: `python tools/tests/test_text_break.py` → prints `text_break: PASS`.

## Review Focus

1. **NBSP between words** (`Mr. Smith`, `10 km`) — must not break there → test in Task 1.
2. **Runs of spaces / tab at a line end** — next line must not start with a space; a double space counts as one stretch slot → tests in Tasks 1 and 2.
3. **Vietnamese NFD tone mark at the overflow point** — the mark hangs with its letter, never starts a line → test in Task 1 (`IsHangingGlyph`).
4. **TXT paginator vs page materializer agreement** on an English TXT (jump into the middle of a book) — identical line starts → test in Task 1 (line-local context: same input from a fresh state gives the same breaks) + device check in Task 6.
5. **A word wider than the whole line** (URL, long chemical name) — layout terminates with a hard break, no infinite loop → device check in Task 6 (loop logic unchanged; covered by existing fallback).

---

### Task 1: Script-aware break rule + host test harness

**Files:**
- Modify: `main/reader/vertical_text.h` (add classification + rule; extend `VerticalBreakState`)
- Create: `tools/tests/test_text_break.cpp`, `tools/tests/test_text_break.py`
- Modify: `tools/tests/README.md`, `tools/tests/README_zn.md` (one paragraph each: how to run, needs g++ ≥ 9 on PATH or `CXX`)

**Interfaces — Produces (all `namespace reader`, `inline`, in `vertical_text.h`):**
- `enum class BreakClass : uint8_t { kNeutral, kCjk, kLatin };`
- `BreakClass ClassifyBreak(uint32_t cp);` — CJK: U+1100–11FF, U+2E80–2FFF, U+3000–303F, U+3040–30FF, U+3100–31FF, U+3200–9FFF, U+A960–A97F, U+AC00–D7FF, U+F900–FAFF, U+FE30–FE4F, U+FF00–FFEF, U+20000–3FFFF. Latin: Unicode letters outside CJK in U+0041–005A, U+0061–007A, U+00C0–024F (excluding U+00D7, U+00F7), U+0250–02AF, U+0300–036F, U+0370–03FF, U+0400–04FF, U+1E00–1EFF. Everything else Neutral.
- `bool IsBreakingSpace(uint32_t cp);` — `' '` and `'\t'` only.
- `bool IsHangingGlyph(uint32_t cp);` — `IsBreakingSpace(cp) || (cp >= 0x0300 && cp <= 0x036F)`.
- `bool CanBreakBetween(uint32_t before, uint32_t after, bool cjk_context);` — spec §1 rules 1–4 in order.
- `bool NextCjkContext(bool cjk_context, uint32_t cp);` — returns `true` for CJK cp, `false` for Latin cp, unchanged for Neutral.
- `VerticalBreakState` gains `bool cjk_context = true;` and `Observe(uint32_t cp, uint32_t source_offset, size_t bytes, bool words = false)`: when `words`, uses `CanBreakBetween(previous, cp, cjk_context)` then `cjk_context = NextCjkContext(cjk_context, cp)`; when `!words`, behaviour byte-identical to today.

- [ ] **Step 1: Write the failing test** `tools/tests/test_text_break.cpp` — plain `main()` with a `CHECK(cond)` macro that prints file:line and returns 1; `#include "vertical_text.h"`. Cases (each a named function called from `main`):
  - `english_words`: feed "the quick brown" through `VerticalBreakState` with `words=true`; collect offsets where `valid` toggles to a new offset → exactly the offsets of `q` and `b`.
  - `no_break_inside_word`: for every adjacent pair in "TemperatureSensor", `CanBreakBetween(a,b,false) == false`.
  - `hyphen_and_dash`: `CanBreakBetween('-', 'k', false)` true ("well-known"); `CanBreakBetween(0x2014, 'a', false)` true; `CanBreakBetween('-', ' ', false)` false; `CanBreakBetween(0x2014, 0x2014, true)` false.
  - `joined_marks`: false for `('n',0x2019)`, `(0x2019,'t')` (don't), `('3','.')`, `('.','1')` (3.14), `('U','.')`, `('.','S')` — all with `cjk_context=false`.
  - `nbsp`: `CanBreakBetween(0x00A0, 'S', false)` false; `CanBreakBetween('.', 0x00A0, false)` false.
  - `spaces`: `CanBreakBetween(' ', 'a', false)` true; `CanBreakBetween(' ', ' ', false)` false; `CanBreakBetween('\t', 'a', false)` true.
  - `vietnamese`: "Tiếng Việt" precomposed (U+1EBF, U+1EC7) breaks only before `V`; NFD "é" → `CanBreakBetween('e', 0x0301, false)` false and `IsHangingGlyph(0x0301)` true; `ClassifyBreak(0x1EC7) == BreakClass::kLatin`.
  - `chinese_unchanged`: for every adjacent pair in a corpus string containing `中文，“引号”——测试。”“你好2023年…（括号）` evaluated with the context a fresh `VerticalBreakState` would have (CJK), `CanBreakBetween(a,b,true) == VerticalCanBreak(a,b)`; and `VerticalBreakState::Observe(..., words=false)` over "TemperatureSensor" still breaks between letters (old behaviour preserved for vertical).
  - `mixed`: "中文English中文" with `words=true` → breaks allowed before `E` and before the second `中`, none inside `English`.
  - `line_local`: two fresh `VerticalBreakState`s fed the same Latin line produce identical `offset`/`valid` sequences, and a default-constructed state has `cjk_context == true`.

- [ ] **Step 2: Write the runner** `tools/tests/test_text_break.py`: compile with `os.environ.get("CXX", "g++")`, args `-std=c++17 -Wall -Werror -I main/reader tools/tests/test_text_break.cpp -o <tempdir>/text_break.exe`, run it, `check=True`, print `text_break: PASS`. Run from repo root.

- [ ] **Step 3: Run test to verify it fails**
  Run: `python tools/tests/test_text_break.py`
  Expected: compile error (`CanBreakBetween` not declared).

- [ ] **Step 4: Implement** the Produces list in `vertical_text.h`, above `VerticalBreakState`, with Chinese comments matching the file.

- [ ] **Step 5: Run test to verify it passes**
  Run: `python tools/tests/test_text_break.py` → `text_break: PASS`

- [ ] **Step 6: Commit**
```bash
git add main/reader/vertical_text.h tools/tests/test_text_break.cpp tools/tests/test_text_break.py tools/tests/README.md tools/tests/README_zn.md
git commit -m "reader: add script-aware line break rule with host tests"
```

### Task 2: Word-gap justification and Latin trailing-punctuation rule

**Files:**
- Modify: `main/reader/vertical_text.h` (`TextSpacingState`, `CanFitTrailingPunctuation`)
- Modify: `main/reader/text_spacing.h` (`ObserveTextGlyph`, `MakeTextLineSpacing`)
- Test: `tools/tests/test_text_break.cpp`

**Interfaces:**
- Consumes: `ClassifyBreak`, `IsBreakingSpace`, `NextCjkContext` (Task 1).
- Produces:
  - `bool JustifySlot(uint32_t before, uint32_t cp, bool cjk_context);` (in `vertical_text.h`) — CJK branch (either side CJK, or both Neutral with `cjk_context`) → `!VerticalClosing(cp)`; else `IsBreakingSpace(before) && !IsBreakingSpace(cp)`; else `false`.
  - `TextSpacingState` gains `uint32_t previous = 0; bool cjk_context = true;`.
  - `CanFitTrailingPunctuation(...)` additionally requires `spacing.cjk_context`.
  - `ObserveTextGlyph(...)` (both overloads, `text_spacing.h`): `expand = JustifySlot(spacing.previous, cp, spacing.cjk_context)`; after `Observe`, set `previous = cp` and `cjk_context = NextCjkContext(cjk_context, cp)`.
  - `MakeTextLineSpacing(...)` horizontal: track the advance of trailing `IsBreakingSpace` glyphs; when the final `state.cjk_context` is false, subtract it from `spacing.width` before computing `need`/`extra`.

- [ ] **Step 1: Write the failing tests** (add to `main`):
  - `justify_slots_latin`: count `JustifySlot` over "a  quick brown" with a context tracked by `NextCjkContext` → 2 (the double space counts once); trailing "word " adds 0.
  - `justify_slots_cjk_unchanged`: over `中文，测试。` each `JustifySlot(prev,cp,true) == !VerticalClosing(cp)`.
  - `squeeze_latin_off`: a `TextSpacingState` with `capacity = 10, tail_blank = 2, cjk_context = false` → `CanFitTrailingPunctuation('.', 101, 100, s)` false; same with `cjk_context = true` → true.
- [ ] **Step 2:** `python tools/tests/test_text_break.py` → fails to compile (`JustifySlot` missing).
- [ ] **Step 3:** Implement the Produces list.
- [ ] **Step 4:** `python tools/tests/test_text_break.py` → `text_break: PASS`; `./tools/idf.ps1 build` → exit 0, no new warnings.
- [ ] **Step 5: Commit** — `git commit -m "reader: justify Latin text between words only"` (the three files).

### Task 3: Wire word breaking into the three layout loops + cache version

**Files:**
- Modify: `main/reader/book_session.cc` — TXT paginate (~2015-2070), EPUB/HTML (~2476-2512), TXT materialize (~3484-3526), `FontFingerprint()` (:1132)

**Interfaces:**
- Consumes: `VerticalBreakState::Observe(cp, off, bytes, bool words)`, `IsHangingGlyph` (Task 1); `CanFitTrailingPunctuation` context rule (Task 2).
- Produces: `constexpr uint32_t kLayoutRulesVersion = 2;` (file-scope in `book_session.cc`), mixed into the start of `FontFingerprint()`'s hash.

- [ ] **Step 1:** In all seven `Observe` calls (`candidate.Observe` and `breaks.Observe`, incl. the EPUB tail re-observe loop) pass `!vertical_writing_` as `words`.
- [ ] **Step 2:** In each of the three overflow conditions, an overflow is ignored when `!vertical_writing_ && IsHangingGlyph(cp)` (the glyph is appended and hangs; the next non-hanging glyph triggers the break at its own offset, so the next line never starts with whitespace or a mark). Keep the `kMaxLineSourceBytes` clause unconditional.
- [ ] **Step 3:** Hash `kLayoutRulesVersion` first in `FontFingerprint()` so `.idx`/`.cpg`/EPUB page caches rebuild once.
- [ ] **Step 4:** `python tools/tests/test_text_break.py` → PASS; `./tools/idf.ps1 build` → exit 0, no new warnings.
- [ ] **Step 5: Commit** — `git commit -m "reader: wrap horizontal text at word boundaries"`.

### Task 4: Alignment setting (Justified / Left)

**Files:**
- Modify: `main/reader/reader_types.h` (`TextStyle`)
- Modify: `main/reader/page_text_raster.cc:351`, `main/reader/reader_page_render.h:108-120,194`
- Modify: `main/display/screen/book_screen/reader/book_reader_body.cc:416` (pass alignment)
- Modify: `main/display/screen/book_screen/reader/book_reader_prefs.{h,cc}` (pref + persistence, modelled on `BookReaderPrefsAntialias`, `BookPrefsPersistKind::kAntialias`)
- Modify: `main/display/screen/book_screen/settings/book_settings_sheet.cc` (switch row next to `BOOK_ANTIALIAS` at :1070, `RefreshSwitchRows` :943, :966, bindings :1895)
- Modify: `main/assets/lang_home.cc` (X entries near :1081), `main/assets/lang_config.h` (externs near :571)

**Interfaces:**
- Produces:
  - `reader::TextStyle::justify` — `bool justify = true; // 两端对齐；false 为左对齐，仅影响横排绘制`
  - `int BookReaderPrefsJustify(void);` / `void BookReaderPrefsSetJustify(int enabled);` — default 1, NVS key `"rd_justify"` in the reader prefs namespace; `BookReaderPrefsTextStyle()` sets `style.justify`.
  - `CreatePageTextLabel(..., int indent_chars = 2, bool justify_enabled = true)` — uses `item.justify && justify_enabled` to pick `DrawJustifiedTextLabel`.
  - Strings: `BOOK_ALIGNMENT` ("对齐" / "Alignment"), `BOOK_ALIGN_JUSTIFY` ("两端对齐" / "Justified"), `BOOK_ALIGN_LEFT` ("左对齐" / "Left").
- Rules: horizontal raster line 351 uses `item.justify && style.justify && !item.chapter_title`; vertical line 405 unchanged; `PageTextColumnWidth` unchanged (uses raw `item.justify`, so Left does not narrow or re-centre the column). The sheet handler follows `SheetAntialias`: set pref, `RequestSettingsSheetPaint()`, `CancelReaderPageWork()`, `RequestRenderReaderPage()` — no re-pagination. `BookReaderPrefsResetReading()` restores Justified. The switch row shows `BOOK_ALIGN_JUSTIFY` / `BOOK_ALIGN_LEFT` instead of On/Off.

- [ ] **Step 1:** Implement the pref, `TextStyle` field, draw-time masking and sheet row.
- [ ] **Step 2:** `./tools/idf.ps1 build` → exit 0, no new warnings; `python tools/tests/test_text_break.py` → PASS.
- [ ] **Step 3: Commit** — `git commit -m "reader: add Justified/Left alignment setting"`.

### Task 5: Symmetric side margins

**Files:**
- Modify: `main/display/screen/book_screen/reader/book_reader_prefs.cc` — `kTextMin`/`kTextMax` (:81-82), `kMarginTable` (:51-56), load (:182-190), `MarginValue` (:488), `BookReaderPrefsSetTextValue` (:771), `BookReaderPrefsMarginLeft/Right` (:503-512)
- Modify: `main/display/screen/book_screen/settings/book_settings_sheet.cc` — margin picker (:1081-1089), `TextValue` (:732-736), refresh (:1879-1881)
- Modify: `main/assets/lang_home.cc`, `main/assets/lang_config.h` — `BOOK_MARGIN_SIDES` ("左右" / "Sides")

**Interfaces:**
- Produces: `static int ClampSideMargin(int px)` → `std::clamp(px, 5, 40)`; effective `BookReaderPrefsMarginLeft()` and `BookReaderPrefsMarginRight()` always return the same clamped value.
- Rules (spec §6):
  - `kTextMin`/`kTextMax` for MarginLeft/MarginRight → 5 / 40; `kMarginTable[3]` sides 56 → 40.
  - `BookReaderPrefsSetTextValue(MarginLeft or MarginRight, v)` writes both indices, one `SaveTextValues()`.
  - Migration in load, on effective values: both sides `-1` and legacy `Margin` `-1` → untouched; otherwise if effective left ≠ right or outside 5–40 → store both as `ClampSideMargin(max(left, right))`, persist once.
  - Sheet picker: three choices Top / Bottom / Sides (`MarginLeft` option id, label `BOOK_MARGIN_SIDES`); numeric row bounds 5–40 when Sides is selected, 0–64 otherwise; a stored `s_margin_option == MarginRight` maps to `MarginLeft`.
  - `ReadContentArea()` (`book_geom.cc:115-128`) needs no change.

- [ ] **Step 1: Write the failing test** — the prefs code is NVS/LVGL-bound, so the pure rule lives in a new header `main/display/screen/book_screen/reader/book_margin_rules.h`: `inline int ReaderSymmetricSideMargin(int left, int right)` returning `std::clamp(std::max(left, right), 5, 40)`. Test `margin_rules`: `(16,24)→24`, `(56,56)→40`, `(0,3)→5`, `(30,30)→30`. Add `-I main/display/screen/book_screen/reader` to the runner. `ClampSideMargin` and the migration call this helper.
- [ ] **Step 2:** `python tools/tests/test_text_break.py` → fails (header missing).
- [ ] **Step 3:** Create the header; implement the prefs and sheet rules above using it.
- [ ] **Step 4:** `python tools/tests/test_text_break.py` → PASS; `./tools/idf.ps1 build` → exit 0, no new warnings.
- [ ] **Step 5: Commit** — `git commit -m "reader: single symmetric side margin (5-40 px)"`.

### Task 6: Verification and hand-off

**Files:**
- Modify: `docs/agents/CODEBASE_MAP.md` (Build & run: host test command + result; Reader: break rule and alignment notes)

- [ ] **Step 1:** `python tools/tests/test_text_break.py` → PASS (paste output).
- [ ] **Step 2:** `./tools/idf.ps1 build` → exit 0; `./tools/idf.ps1 size` → app fits the 5 MB `ota_0` slot; note growth vs `fd3fa03` if > 2 %.
- [ ] **Step 3:** Update `CODEBASE_MAP.md`; run `graphify update .`.
- [ ] **Step 4: Commit** — `git commit -m "docs: record reader word-wrap verification"`.
- [ ] **Step 5: Device checklist for the owner** (flash: `./tools/idf.ps1 -p COM<n> flash monitor`):
  1. The book in the photo (English EPUB): no word split across lines; justified lines stretch only between words; log shows `txt idx stale, will rebuild` / cache rebuild once.
  2. A Vietnamese TXT and EPUB: no syllable split, tone marks attached (also confirms the reading font has the glyphs).
  3. A Chinese book: page count and line breaks identical to before (compare a screenshot of page 1 and a middle page).
  4. Settings → Alignment Left/Justified: page redraws instantly, page number unchanged.
  5. Settings → Margins → Sides at 5 and 40: both sides equal; Top/Bottom still 0–64.
  6. An English TXT: open, jump to ~50 % via progress, page forward/back 5 pages — no repeated or missing lines.
  7. A line with a very long URL: layout completes, URL hard-breaks.
