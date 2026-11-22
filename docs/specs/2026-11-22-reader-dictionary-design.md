# Reader dictionary lookup (EN→EN, EN→VI) — design

Date: 2026-11-22 · Status: draft for approval · Branch context: `feature/system-font`

## Intent

While reading an English book, the user long-presses a word on the page and gets its meaning
in English (English–English) and in Vietnamese (English–Vietnamese), offline, without leaving
the page. Target user: a Vietnamese reader of English books. The Literata system font already
covers Latin + Vietnamese, so definitions render without font work.

## Goals

- Long-press on a word in the reading page → a bottom sheet shows the word and its EN→VI and
  EN→EN entries.
- Fully offline; dictionaries live on the SD card and can be replaced by the user.
- Inflected forms still resolve (`running` → `run`, `books` → `book`, `studied` → `study`).
- Normal taps keep working exactly as today (tap zones: prev / menu / next …).

## Non-goals

- Typing a word / a standalone dictionary app.
- Vertical text pages (CJK vertical layout); long-press there does nothing.
- Multi-word phrases, text selection handles, highlight/vocabulary list, audio pronunciation.
- Shipping dictionary data in firmware or in the repo (size + licensing).
- Rich HTML rendering of entries (entries are shown as plain text).

## Anti-goals (failure even if it "works")

- A long-press also turns the page or opens the menu on release.
- Lookup stalls the LVGL task (SD reads must not run on the UI thread for long).
- Holding a per-page word list or a whole dictionary index in RAM.
- Any change to how pages are laid out or paginated.

## Constraints (from the code)

- Pages are drawn into one L8 bitmap by `reader::RasterPageTextToL8`
  (`main/reader/page_text_raster.cc:276`, called from `book_screen/reader/book_page_work.cc:91`);
  no per-word objects exist. Label rendering (`CreatePageTextLabel`, `book_reader_body.cc:416`) is
  only a fallback when rasterising fails.
- Touch on the page: `AttachReadBodyInput` (`book_screen/book_nav.cc:415`) registers
  `OnReadContentClicked` for `LV_EVENT_CLICKED`, which hit-tests the 3×3 tap zones
  (`book_layout_worker.cc:310`). No long-press handler exists on the page body.
  In LVGL `CLICKED` still fires on release after a long press; `SHORT_CLICKED` does not
  (already relied on in `book_settings_sheet.cc:1647`).
- 16 MB flash is full of OTA slots; spiffs partitions are 400 KB → dictionaries go on SD.
  SD paths are built in `main/boards/common/storage/sd_data_layout.h`.
- Reader code is also compiled by the PC emulator (`tools/ebook/emulator`, `EBOOK_EMULATOR`) and
  host tests (`tools/tests/test_text_break.py`); new pure logic must stay host-buildable.
- UI strings go through `assets/lang_config.h` (English default, Vietnamese).

## Approaches considered

**Word hit-testing**
- **A (chosen).** On long-press, recompute which word lies under the point with a pure function
  that walks the page using the *same* line/glyph positioning rules as the raster. No memory
  between presses, cannot drift from what is drawn, host-testable.
- B. Raster records word boxes per page while drawing. Slightly faster lookup, but every page
  carries hundreds of boxes and the raster function grows. Rejected: long-press is rare.

**Dictionary data**
- **1 (chosen).** StarDict dictionaries on SD, prepared by a host script into a simple,
  uncompressed, plain-text form the device can binary-search. Users can bring any StarDict
  EN-EN / EN-VI dictionary.
- 2. Our own compact binary format. Smaller, but users can't drop in dictionaries they find.

## Design

### 1. Word hit-test — `main/reader/page_word_hit.{h,cc}` (pure, host-testable)

```cpp
namespace reader {
struct PageWordHit { std::string word; };   // empty word = nothing under the point
// x,y relative to the raster's top-left (same origin RasterPageTextToL8 uses).
bool PageWordAt(const Page& page, const lv_font_t* font, int spacing_percent,
                lv_coord_t para_gap, int page_w, int page_h, const TextStyle& style,
                int x, int y, PageWordHit& out);
}
```

- Parameters mirror every layout-affecting input of `RasterPageTextToL8`. `underline_mode` and
  `antialias` only change ink, not positions (row frames always reserve the underline,
  `MakeTextLineLayout`); image heights come from `PageItem::image_h`, already in `page`. The
  host test asserting hit-test/raster agreement guards this.
- Only pages that were actually rasterised support lookup. If the page fell back to label
  rendering (raster failed, `book_page_work.cc`), long-press does nothing (my call).

- Walks `page.items` exactly like `RasterPageTextToL8` (rows from `MakeTextLineLayout`,
  paragraph gaps, indent, chapter-title lines, justify via `MakeTextLineSpacing`, `letter_gap`).
  To guarantee identical positions, the per-line glyph x walk is extracted from
  `page_text_raster.cc` into a shared internal helper used by both raster and hit-test
  (refactor only; raster output must stay byte-identical — host test checks it).
- Finds the row whose cell contains `y`, then the glyph whose advance span contains `x`
  (a few px of slack to the nearest glyph on the line), then expands to the word:
  letters (ASCII + Latin-1/Latin Extended letters), with inner `'`/`’` and `-` kept when
  between letters. Returns nothing on whitespace/punctuation/image/vertical pages.
- Screen → raster coordinates: the caller subtracts `text_top` and the column's left
  (`text_left + (width - column_width)/2`, as in `book_page_work.cc`). Those values come from
  the current page work result / reader geometry.

### 2. Dictionary reader — `main/reader/dictionary.{h,cc}` (host-buildable, file I/O only)

On-SD layout (per dictionary, one folder each):

```
<SD>/dict/<name>/<name>.ifo    StarDict info (bookname, wordcount, idxfilesize)
<SD>/dict/<name>/<name>.idx    StarDict index, sorted, uncompressed
<SD>/dict/<name>/<name>.dict   entry data, uncompressed, plain text ('m')
<SD>/dict/<name>/<name>.pos    uint32 LE offsets of each .idx entry (built by our script)
```

- `.pos` gives fixed-width random access into the variable-length `.idx`, so lookup is a
  binary search with ~17 probes of (read 4 B from `.pos`, read ≤ 256+8 B from `.idx`); nothing
  is held in RAM besides the `.ifo` fields (parsed once, cached).
- **SD access rules:** files are opened per lookup and closed before returning — never kept
  open — because standby / low-power entry wait for `sd_access::IsIdle()` (zero open files) and
  mount/USB-MSC transitions wait for open accesses (`power_hw.cc`, `power_policy_standby_lp.cc`).
  Each lookup runs inside one `sd_access` scope (`Permit`/`Transaction`, see
  `main/boards/common/storage/sd_access.h`) and fails gracefully ("SD card busy") if access is
  refused (e.g. USB MSC active). Cost: ~3 opens per dictionary per lemma candidate; acceptable
  on the worker (target < 300 ms per long-press for two dictionaries).
- Comparison follows StarDict's order (`g_ascii_strcasecmp`, then `strcmp`); match is
  case-insensitive, preferring an exact-case hit.
- API: `Dictionary::Open(dir)`, `bool Lookup(std::string_view word, std::string& definition)`,
  `name()`. `DictionarySet` discovers folders under `<SD>/dict/`, classifies by a
  `lang=en-vi` / `lang=en-en` line we add to the `.ifo` (fallback: folder order), and returns
  entries in the order EN→VI, then EN→EN. Definition text is capped (e.g. 8 KB per entry).
- Lemma fallback (pure function, host-tested): try the word as-is, lower-case, then strip
  `'s`, `s`, `es`, `ies→y`, `ed`, `ied→y`, doubled consonant + `ed`/`ing`, `ing`, `ing→e`,
  `er`, `est`; first hit wins; the sheet shows the headword actually found.
- SD root path: `SD_MOUNT_POINT "/dict"` (`SD_MOUNT_POINT` from
  `main/boards/common/storage/sd_paths.h`), exposed via a small `sd_data::DictRoot()` added to
  `sd_data_layout.h` (which today only has per-file path builders); the emulator uses a host
  directory.
- No book-language gating: there is no language signal in the book session today, and a
  "not found" on a non-English book is harmless (my call).

### 3. Host tool — `tools/dict/prepare_stardict.py`

Input: any StarDict dictionary (`.ifo` + `.idx`(.gz) + `.dict`(.dz) or `.dict`), plus
`--lang en-vi|en-en`. Output: the folder above. It decompresses, converts entry types
`h`/`g`/`x` (HTML/markup) to plain text (`m`), strips tags, keeps line breaks, re-sorts the
index in StarDict order, writes `.pos`, and adds `lang=` to `.ifo`. Documented in
`docs/` with recommended free sources (WordNet-based EN-EN is permissive; EN-VI licences
vary and are the user's responsibility).

### 4. UI — `main/display/screen/book_screen/reader/book_dict_sheet.{h,cc}`

- `AttachReadBodyInput`: switch the tap handler from `LV_EVENT_CLICKED` to
  `LV_EVENT_SHORT_CLICKED`, and add `OnReadContentLongPressed` for `LV_EVENT_LONG_PRESSED`
  (only when `read_chrome == kReading`, session open, not opening, horizontal text page).
  `DetachReadBodyInput` removes both.
- Long-press: no extra haptic pulse (the body's `HapticAttachClick` already buzzes on press,
  and business callbacks must not `Pulse` again — `book_nav.cc:420`); map point → raster coords, run `PageWordAt` on the UI thread
  (pure CPU, one page) → if a word is found, show the sheet with "Looking up…" and post the SD
  lookup to a worker (reuse an existing reader worker pattern — `book_font` / `reader_page`
  style task with internal-RAM stack, see CODEBASE_MAP "Stacks"); result is delivered back via
  `BookLvAsync`.
- Sheet: bottom sheet over the page (same style as the reading settings sheet), ~50 % height:
  headword (bold), then a section per dictionary with its `bookname` as title, scrollable.
  Closes on tap outside, `vk_home`/`vk_prev`, or page turn. While open, tap zones are inert
  (new `ReadChrome::kDict` state: added to `IsReadOverlayChrome` in `book_geom.cc:88` and
  handled in `book_vk.cc` like `kSettings`; audit every `read_chrome` switch/compare).
- Messages (lang_config, EN + VI): no word under finger → no sheet; no dictionaries on SD →
  "No dictionary found. Copy dictionaries to /dict on the SD card."; not found →
  "‘word’ not found".

```
┌──────────────────────────────────────────┐
│  …page text…                             │
├──────────────────────────────────────────┤
│ running → run                            │
│ ── Anh–Việt ──                           │
│ động từ: chạy, chạy trốn, vận hành …     │
│ ── WordNet (EN) ──                       │
│ v. move fast by using one's feet …       │
└──────────────────────────────────────────┘
```

### 5. Tests / evidence

- Host tests (added to `tools/tests/`): `PageWordAt` against a synthetic page + a host font
  (word boundaries, apostrophes, hyphens, justified lines, indent, paragraph gaps, misses);
  raster output unchanged after the refactor; dictionary binary search on a tiny generated
  StarDict; lemma rules.
- `idf.py build` clean, `idf.py size` within OTA slot.
- On device (user): long-press in an English EPUB → sheet; short taps still page; heap/PSRAM
  before/after one lookup logged.

## Decided by the user

- Interaction: long-press a word while reading (no standalone app).
- Hit-test approach A; dictionary approach 1 (StarDict on SD).

## My calls (please check)

- EN→VI shown before EN→EN.
- `.pos` offset file instead of loading the `.idx` into PSRAM.
- Entries rendered as plain text; markup stripped by the host script, not on device.
- Vertical-text pages excluded.
- Tap handler switched to `SHORT_CLICKED` so a long-press never also triggers a tap action.
- Dictionary files opened and closed per lookup inside an `sd_access` scope (sleep/USB safety).
- Long-press disabled on label-fallback pages and vertical pages.
- No book-language gating; no second haptic pulse.

## Left to the builder

- Exact sheet styling/height, entry cap size, worker task reuse vs. a small new one.
- How the current page's raster origin (`text_top`, column left) is exposed to the handler.
- Emulator wiring for the host dictionary directory.
