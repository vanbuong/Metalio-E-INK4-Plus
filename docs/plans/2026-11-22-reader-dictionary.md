# Reader Dictionary Lookup Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Long-press a word on a reading page → bottom sheet with its English→Vietnamese and English→English meanings from StarDict dictionaries on the SD card.

**Architecture:** A pure hit-test (`PageWordAt`) re-walks the current page with the same line/glyph positioning the raster uses (shared via two small extracted helpers). A pure-stdio StarDict reader binary-searches prepared dictionaries (`.idx` + our `.pos` offset file). The book screen adds a long-press handler, a `kDict` chrome state, a sheet, and a one-shot worker task that does the SD lookup inside `sd_access::Transaction`.

**Tech Stack:** ESP-IDF v6.1 (esp32s31), C++17, LVGL 9, FreeRTOS; host tests with g++ + a minimal LVGL stub; Python 3 for the prep tool.

**Spec:** `docs/specs/2026-11-22-reader-dictionary-design.md` (read it first).

## Global Constraints

- Firmware build via `./tools/idf.ps1 build` (never a bare `idf.py`); exit 0, no new warnings.
- App must still fit its OTA slot in `partitions/v1/16m.csv` (`./tools/idf.ps1 size`); report growth > 2 %.
- Don't edit `sdkconfig`, `components/`, `patches/esp-idf/`.
- Reader logic under `main/reader/` stays host-buildable (no ESP-IDF headers in new `main/reader/` files except behind `#ifndef EBOOK_EMULATOR`/host guards as `book_storage_paths.h` does).
- Raster pixel output must not change (refactor only).
- Dictionary files: opened and closed per lookup, never held open; every lookup inside one `sd_access::Transaction` (`main/boards/common/storage/sd_access.h`).
- SD root: `SD_MOUNT_POINT "/dict"` (`main/boards/common/storage/sd_paths.h`).
- UI strings: declared in `main/assets/lang_reader.h` (`Lang::Strings`), defined in the `X(NAME, "vi", "en")` table in `main/assets/lang_home.cc`.
- No extra haptic `Pulse` in the long-press handler (`book_nav.cc:420` convention).
- Order of sections in the sheet: EN→VI, then EN→EN. Definition text capped at 8192 bytes per entry.
- Long-press only on horizontal, rasterised pages (`st.reader.text_raster_image != nullptr`, `!session->VerticalWriting()`, `!session->IsPageImagesMode()`).
- New sources under `main/reader/` and `main/display/screen/book_screen/reader/` are picked up by the existing globs in `main/CMakeLists.txt:69,94` — no CMake edits.
- After code changes run `graphify update .`.

## Review Focus

1. Long-press on whitespace, margin, punctuation, or an image → no sheet, and on release no page turn/menu (tap zones must not fire after `LONG_PRESSED`).
2. Words with curly/straight apostrophes and hyphens (`John’s`, `don't`, `well-known`), sentence-initial capitals and ALL-CAPS (`The`, `NASA`) → found via case-folding and `'s` stripping.
3. Long-press on a non-English word (Vietnamese diacritics `người`, CJK) → "not found" sheet or no sheet, never a crash or garbage headword.
4. Page turn, Home key, or opening settings while a lookup is still running → stale result is dropped (generation token), sheet never appears over the wrong page.
5. SD card absent, USB MSC active, or standby entry during lookup → "SD card unavailable" message, no hang, `sd_access::IsIdle()` true afterwards.

(1–3 are pinned by host tests in Tasks 2–3; 4–5 by Task 6 code + the device checklist in Task 7.)

---

### Task 1: Host test harness + shared horizontal glyph pen

**Files:**
- Create: `tools/tests/stubs/lvgl.h` — minimal LVGL for host: `lv_coord_t` (int32_t), `lv_font_glyph_dsc_t` {`adv_w`, `box_w`, `box_h`, `ofs_x`, `ofs_y`, `resolved_font`}, `lv_font_t` {`line_height`, `base_line`, `release_glyph`, plus a `get_glyph_dsc` function pointer}, `lv_font_get_glyph_dsc()`, `lv_font_get_glyph_width()`, plus whatever else `text_spacing.h`/`text_encoding.h`/`reader_types.h` pull in (add only what the compiler asks for).
- Create: `main/reader/text_glyph_pen.h`
- Modify: `main/reader/page_text_raster.cc:126-234` (`DrawUtf8Line` horizontal branch uses the pen)
- Create: `tools/tests/test_reader_dict.cpp`, `tools/tests/test_reader_dict.py` (same shape as `test_text_break.py`, include order: `tools/tests/stubs` first, then `main/reader`; compiles `test_reader_dict.cpp` + later `page_word_hit.cc`, `dictionary.cc`)

**Interfaces:**
- Produces:
```cpp
namespace reader {
// Horizontal pen: mirrors DrawUtf8Line's x math exactly.
struct HorizontalGlyphPen {
    HorizontalGlyphPen(const lv_font_t* font, const char* utf8, size_t size,
                       int x, const TextLineSpacing& spacing, int letter_gap);
    // Advances over one code point. out_x = left edge of its advance cell
    // (pen_x + shift, without ofs_x), out_adv = advance incl. letter_gap.
    void Step(uint32_t cp, const lv_font_glyph_dsc_t& glyph, bool known, int* out_x, int* out_adv);
    int EndX() const;   // == DrawUtf8Line's *out_end_x for horizontal lines
};
}
```
- Test helper in `test_reader_dict.cpp`: `MakeMonoFont(int adv, int line_height)` returning an `lv_font_t` whose every glyph has `adv_w = adv`, `box_w = adv-1`.

- [ ] **Step 1:** Write `test_glyph_pen_matches_spacing_width`: for `"Hello world"` with mono font adv 10, `MakeTextLineSpacing(font, s, 200, /*justify*/true)`, walking every cp through the pen gives monotonically increasing `out_x`, first `out_x == x`, and `EndX() - x == spacing.width`. Also justify=false case: `out_x` of `'w'` == x + 6*10.
- [ ] **Step 2:** Run `python tools/tests/test_reader_dict.py` → FAIL (`text_glyph_pen.h` missing).
- [ ] **Step 3:** Implement `text_glyph_pen.h` by lifting the horizontal `spacing_state`/`shift`/`pen_x` logic out of `DrawUtf8Line`; make `DrawUtf8Line`'s non-vertical path call it (vertical path untouched).
- [ ] **Step 4:** `python tools/tests/test_reader_dict.py` → PASS; `python tools/tests/test_text_break.py` → `text_break: PASS`.
- [ ] **Step 5:** `./tools/idf.ps1 build` exit 0, no new warnings.
- [ ] **Step 6:** Commit `reader: extract horizontal glyph pen; host test harness`.

### Task 2: Page line walk + `PageWordAt`

**Files:**
- Create: `main/reader/page_line_walk.h` — header-only walk over `page.items` reproducing `RasterPageTextToL8`'s row/para-gap/indent/chapter-title/centering logic (`page_text_raster.cc:301-375`).
- Modify: `main/reader/page_text_raster.cc:276-378` — `RasterPageTextToL8` iterates lines via the walk (draw/underline calls stay in the raster).
- Create: `main/reader/page_word_hit.h`, `main/reader/page_word_hit.cc`
- Test: `tools/tests/test_reader_dict.cpp`

**Interfaces:**
- Consumes: `HorizontalGlyphPen` (Task 1).
- Produces:
```cpp
namespace reader {
struct PageTextLine {
    const PageItem* item; std::string_view text;   // one drawn line (chapter titles split on '\n')
    int x, line_y, cell_top, cell_h;               // raster coords
    TextLineSpacing spacing; bool faux_bold;
};
// Returns false where RasterPageTextToL8 would return false (overflow etc.).
template <class Fn> bool ForEachPageTextLine(const Page&, const lv_font_t*, int spacing_percent,
        lv_coord_t para_gap, int page_w, int page_h, const TextStyle&, Fn&& on_line);
struct PageWordHit { std::string word; };
bool PageWordAt(const Page& page, const lv_font_t* font, int spacing_percent, lv_coord_t para_gap,
                int page_w, int page_h, const TextStyle& style, int x, int y, PageWordHit& out);
// Pure: expand a byte offset in a UTF-8 line to the surrounding word; "" if not on a letter.
std::string WordAround(std::string_view line, size_t byte_offset);
}
```
- Word rules (`WordAround`): letters = ASCII letters + U+00C0–U+024F + U+1E00–U+1EFF; `'`, U+2019 and `-` are kept only when both neighbours are letters; result has U+2019 normalised to `'`.
- Hit slack: a point within 4 px left/right of a glyph cell on the hit row counts as that glyph; rows match on `[cell_top, cell_top+cell_h)`.

- [ ] **Step 1:** Write tests:
  - `test_word_around`: `WordAround("He said, “don’t go.”", offset of 'n')` == `"don't"`; offset on `,` → `""`; `"well-known"` from either half → `"well-known"`; `"end-"` at line end → `"end"`; `"người"` → `"người"`; CJK `"中文"` → `""`.
  - `test_page_word_at`: page = 3 items (`para_indent` line `"The quick brown fox"`, justified line `"jumps over the lazy"`, `para_gap_before` line `"dog."`), mono font adv 10, line_height 20, page 300×400, default style: a point inside `quick` → `"quick"`; a point between `quick` and `brown` beyond slack → `""`; a point inside the 2nd row at x of `lazy` (computed through `ForEachPageTextLine` + pen) → `"lazy"`; a point in the paragraph gap → `""`; `x<0`/`y>page_h` → `""`.
  - `test_line_walk_rows_match_raster_rules`: the `line_y` of each row equals `y + MakeTextLineLayout(...).TextOffset(row)` accumulated with `ReaderParagraphGap` — i.e. assert concrete numbers for the 3-line page above.
- [ ] **Step 2:** Run `python tools/tests/test_reader_dict.py` → FAIL (missing headers).
- [ ] **Step 3:** Implement `page_line_walk.h`, refactor `RasterPageTextToL8` onto it, implement `page_word_hit.cc` (walk lines → find row → pen through glyphs to the byte offset → `WordAround`).
- [ ] **Step 4:** Host tests PASS (both runners).
- [ ] **Step 5:** Raster unchanged check: `./tools/idf.ps1 build` exit 0; on device (user) open the same EPUB page before/after — identical layout. If the emulator builds on this machine, compare a screenshot instead; otherwise record "raster parity verified by shared code + host tests only".
- [ ] **Step 6:** Commit `reader: PageWordAt hit-test sharing the raster line walk`.

### Task 3: Lookup candidates (case + lemma rules)

**Files:**
- Create: `main/reader/dict_word_rules.h` (header-only, pure)
- Test: `tools/tests/test_reader_dict.cpp`

**Interfaces:**
- Produces: `std::vector<std::string> reader::DictLookupCandidates(std::string_view word);` — ordered, de-duplicated: as-is, ASCII-lowercased, then on the lowercase form: strip `'s`/`s'`; `ies→y`; `ied→y`; `es`; `s` (not after `s`); doubled consonant + `ed`/`ing` → single; `ed`; `ed→e`; `ing`; `ing→e`; `er`; `est`. Stems shorter than 2 letters are dropped.

- [ ] **Step 1:** Tests: `"Running"` contains `"run"` and its first element is `"Running"`; `"studied"` contains `"study"`; `"books"` → `"book"`; `"John's"` → `"john"`; `"making"` → `"make"`; `"NASA"` → first `"NASA"`, then `"nasa"`; `"is"` → no stem `"i"`; `"người"` → exactly `{"người"}` (non-ASCII lower-casing untouched, no ASCII suffix rules break UTF-8).
- [ ] **Step 2:** Run → FAIL.
- [ ] **Step 3:** Implement.
- [ ] **Step 4:** Run → PASS.
- [ ] **Step 5:** Commit `reader: dictionary lookup candidate rules`.

### Task 4: StarDict prep tool

**Files:**
- Create: `tools/dict/prepare_stardict.py`, `tools/dict/README.md`
- Test: `tools/tests/test_prepare_stardict.py` (builds a tiny StarDict in a temp dir, runs the tool, checks output)

**Interfaces:**
- CLI: `python tools/dict/prepare_stardict.py <src.ifo> --lang en-vi|en-en --out <dir>` → writes `<out>/<name>/<name>.{ifo,idx,dict,pos}`.
- Output contract (consumed by Task 5): `.ifo` keeps `bookname`, sets `wordcount`, `idxfilesize`, `sametypesequence=m`, adds `lang=<lang>`; `.idx` StarDict 2.4.2 entries (`word\0`, u32 BE offset, u32 BE size) sorted by StarDict order (key = `(ascii_casefold(word), word bytes)`); `.dict` uncompressed UTF-8 plain text; `.pos` = u32 **LE** byte offset of each `.idx` entry, in order. Handles `.idx.gz`, `.dict.dz`/gzip, `idxoffsetbits=32` only (reject 64 with a clear error), types `m`,`t`(drop),`h`,`g`,`x` → plain text (strip tags, `<br>`/`</p>`/`</div>` → newline, unescape entities, collapse blank lines). Entries with no type data or empty text are dropped. Multi-type `sametypesequence` and per-entry type prefixes both supported.

- [ ] **Step 1:** Write test: source dict with words `["apple","Apple","banana","run","zebra"]` (unsorted, `.dict.dz` gzip, type `h` with `<b>fruit</b><br>red`) → output `.idx` order `apple, Apple, banana, run, zebra`; `.pos[i]` points at entry `i`; definition of `apple` == `"fruit\nred"`; `.ifo` has `lang=en-vi`, `sametypesequence=m`, correct `wordcount=5`.
- [ ] **Step 2:** `python tools/tests/test_prepare_stardict.py` → FAIL.
- [ ] **Step 3:** Implement (stdlib only: `gzip`, `struct`, `html`, `re`).
- [ ] **Step 4:** Run → PASS.
- [ ] **Step 5:** README: SD layout (`/dict/<name>/`), usage, licence note (WordNet permissive; EN-VI sources vary, user's responsibility). Commit `tools: StarDict prep for on-device dictionary`.

### Task 5: Dictionary reader (pure stdio)

**Files:**
- Create: `main/reader/dictionary.h`, `main/reader/dictionary.cc`
- Test: `tools/tests/test_reader_dict.cpp` (+ `test_reader_dict.py` generates fixtures by calling `prepare_stardict.py` into a temp dir and passes the path as argv[1])

**Interfaces:**
- Consumes: `DictLookupCandidates` (Task 3), file contract (Task 4).
- Produces:
```cpp
namespace reader {
enum class DictLang : uint8_t { kEnVi, kEnEn, kOther };
struct DictInfo { std::string dir, name, bookname; DictLang lang; uint32_t wordcount; };
struct DictEntry { std::string bookname, headword, definition; };   // definition ≤ 8192 B
// Lists <root>/*/ with a valid prepared .ifo; sorted EN-VI, EN-EN, other, then by name.
std::vector<DictInfo> ScanDictionaries(const std::string& root);
// Exact (StarDict-order) binary search via .pos; opens/closes its files. Case-insensitive
// match, exact-case preferred. false on miss or I/O error.
bool DictLookupExact(const DictInfo& dict, std::string_view word, DictEntry& out);
// Tries DictLookupCandidates(word) in order per dictionary; one entry per dictionary max.
std::vector<DictEntry> DictLookup(const std::vector<DictInfo>& dicts, std::string_view word);
}
```
- No `sd_access` here (caller holds the Transaction). Read sizes: `.pos` 4 B; `.idx` ≤ 256+9 B per probe.

- [ ] **Step 1:** Tests on fixture dict (Task 4 fixture + a second `en-en` dict): `ScanDictionaries` returns 2, EN-VI first; `DictLookupExact("Apple")` → headword `Apple` (exact case preferred); `DictLookupExact("APPLE")` → found; `"zzz"` → false; `DictLookup(…, "Running")` → entry with headword `run` from each dict that has it; a folder missing `.pos` is skipped; truncated `.dict` → `DictLookupExact` false, no crash.
- [ ] **Step 2:** Run → FAIL. **Step 3:** Implement. **Step 4:** Run → PASS.
- [ ] **Step 5:** `./tools/idf.ps1 build` exit 0. Commit `reader: StarDict dictionary reader`.

### Task 6: Long-press UI, sheet, worker, strings

**Files:**
- Modify: `main/display/screen/book_screen/book_screen_priv.h:126-130` — add `kDict` to `ReadChrome`; add `uint32_t dict_generation` to reader state.
- Modify: `main/display/screen/book_screen/book_geom.cc:88` — `IsReadOverlayChrome` includes `kDict`.
- Modify: `main/display/screen/book_screen/book_nav.cc:406-424` — `AttachReadBodyInput` registers `OnReadContentClicked` on `LV_EVENT_SHORT_CLICKED` and `OnReadContentLongPressed` on `LV_EVENT_LONG_PRESSED`; `DetachReadBodyInput` removes both.
- Modify: `main/display/screen/book_screen/reader/book_layout_worker.cc:310` — `OnReadContentClicked`: when `read_chrome == kDict`, close the sheet and return.
- Modify: `main/display/screen/book_screen/book_vk.cc` — in `kScreenRead`: `kDict` + `vk_home`/`vk_prev` → close sheet; `vk_next` ignored. Audit every `read_chrome` compare (`grep -rn read_chrome main/display/screen/book_screen`) and handle `kDict` like `kSettings` where it is an overlay check; page turn/navigation paths close the sheet and bump `dict_generation`.
- Create: `main/display/screen/book_screen/reader/book_dict_sheet.h`, `book_dict_sheet.cc`
- Modify: `main/boards/common/storage/sd_data_layout.h` (+ its `.cc`) — `std::string DictRoot();` → `SD_MOUNT_POINT "/dict"`.
- Modify: `main/assets/lang_reader.h`, `main/assets/lang_home.cc` — strings below.

**Interfaces:**
- Consumes: `PageWordAt` (Task 2), `ScanDictionaries`/`DictLookup` (Task 5), `sd_data::DictRoot()`, `sd_access::Transaction`, `BookLvAsync` (`book_nav.h:34`), `ReaderWorkersBusy()`, `ReaderPageWorkPending()`.
- Produces (`book_dict_sheet.h`): `void OnReadContentLongPressed(lv_event_t* e);` `void CloseDictSheet();` `bool DictSheetVisible();`
- Strings (`X(NAME, vi, en)`):
  - `BOOK_DICT_LOOKING_UP`: "Đang tra từ...", "Looking up..."
  - `BOOK_DICT_NOT_FOUND`: "Không tìm thấy “%s”", "“%s” not found"
  - `BOOK_DICT_NONE`: "Chưa có từ điển. Chép từ điển vào thư mục /dict trên thẻ SD.", "No dictionary found. Copy dictionaries to /dict on the SD card."
  - `BOOK_DICT_SD_UNAVAILABLE`: "Thẻ SD đang bận", "SD card unavailable"
- Behaviour:
  - Long-press guards: `read_chrome == kReading`, session open, `!opening`, `!ReaderWorkersBusy()`, `!ReaderPageWorkPending()`, horizontal rasterised page (Global Constraints). Point → raster coords via `lv_obj_get_coords(st.reader.text_raster_image)`; page params from `session->CurrentPageData()`, `LineSpacingPercent()`, `ParaGap()`, style built as in `book_page_work.cc:128-131`, `page_w = area width`, `page_h = st.reader.viewport_h`. No word → return silently.
  - Word found → `read_chrome = kDict`, show sheet with headword + `BOOK_DICT_LOOKING_UP`, `++dict_generation`, spawn one-shot task `book_dict` (8 KB stack, `MALLOC_CAP_SPIRAM`, priority `tskIDLE_PRIORITY + 1`, pattern of `book_page_image.cc:340`) with `{word, generation}`. If a lookup task is already running, ignore the long-press.
  - Worker: `sd_access::Transaction tx; if (!tx) → SD_UNAVAILABLE;` else `ScanDictionaries(DictRoot())` (empty → `BOOK_DICT_NONE`) then `DictLookup`; post result via `BookLvAsync`; UI applies only if `generation == dict_generation && read_chrome == kDict`.
  - Sheet: child of the read screen, bottom-anchored, 50 % of screen height, same style as the reading settings sheet (`book_reader_body.cc:519-522`: clickable, stops bubbling); scrollable column: headword (bold, with "word → headword" when they differ), then per entry a section title = `bookname` and the definition label (wrap). No results → `BOOK_DICT_NOT_FOUND`.

- [ ] **Step 1:** Implement strings, `DictRoot()`, state + `IsReadOverlayChrome`, input wiring (SHORT_CLICKED + LONG_PRESSED), sheet + worker, key handling.
- [ ] **Step 2:** `./tools/idf.ps1 build` exit 0, no new warnings; `./tools/idf.ps1 size` — app fits slot, note % growth.
- [ ] **Step 3:** All host tests still PASS (`test_reader_dict.py`, `test_text_break.py`, `test_prepare_stardict.py`).
- [ ] **Step 4:** Commit `book: long-press dictionary lookup sheet`.

### Task 7: Docs + device verification

**Files:**
- Modify: `docs/agents/CODEBASE_MAP.md` — Reader engine section: `page_word_hit`, `dictionary`, `dict_word_rules`, `text_glyph_pen`, `page_line_walk`; book screen: `book_dict_sheet`, `ReadChrome::kDict`; Tests line: new host test commands.
- Run: `graphify update .`

- [ ] **Step 1:** Update the map; run `graphify update .`. Commit `docs: map reader dictionary`.
- [ ] **Step 2:** Hand the user this device checklist (flash: `./tools/idf.ps1 -p COM<n> flash monitor`), and record results; until then report "not tested on hardware":
  1. Prepare dictionaries with the tool, copy to `/dict` on SD.
  2. Long-press a word in an English EPUB → sheet with EN-VI then EN-EN; release does not turn the page.
  3. Short taps still turn pages / open the menu; Home and Prev close the sheet.
  4. Long-press whitespace/margin → nothing. Long-press `running` → shows `run`.
  5. Start a lookup then immediately turn the page → no sheet on the new page.
  6. No `/dict` folder → "No dictionary found…" message. USB MSC connected → "SD card unavailable".
  7. Let the device go to standby after a lookup → enters standby normally.
  8. Log free internal heap + PSRAM (`heap_caps_get_free_size`) before and after 5 lookups — no steady loss.
