# Book Cover Standby / Power-off Screen Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Settings → Power lets standby and power-off each show "Wallpaper" (today) or "Book cover" — the full-screen cover of the most recently opened book, following the reading automatically.

**Architecture:** Two RAM-cached NVS flags live with the existing wallpaper slots. A new module `book_cover_wallpaper` prepares one panel-size cover file (`screen_cover.a2l8` + tag) on a low-priority background task after a book opens, keeps a RAM "ready" flag, and resolves the file for the standby and power-off decode tasks, which try it before their existing paths.

**Tech Stack:** C++17, ESP-IDF v6.1 (esp32s31), LVGL 9, FreeRTOS; host tests with g++ via Python.

**Spec:** `docs/specs/2026-11-21-book-cover-standby-design.md`

## Global Constraints

- Default for both screens is **Wallpaper** = today's behaviour, byte-for-byte unchanged code path.
- Panel 684×1216 (`DISPLAY_WIDTH`, `DISPLAY_HEIGHT`); cover fitted whole, aspect kept, never cropped; enlargement only when the screen cover file is prepared.
- Opening a book is never delayed; power-off never waits for cover preparation.
- No NVS access on the LVGL thread or on PSRAM-stack tasks; NVS writes go through `Application::GetInstance().Schedule(..., Application::TaskDomain::Storage)`.
- SD work in background tasks inside `sd_access::Transaction` (skip when `!access || sd_access::IsQuiescing()`), as in `book_cover_workers.cc:175`.
- Files: `SD_PATH_BOOK_CACHE "/screen_cover.a2l8"` and `SD_PATH_BOOK_CACHE "/screen_cover.txt"` (tag = book absolute path).
- NVS namespace `wallpaper`, keys `stb_cover`, `off_cover` (bool, default false).
- Strings: `SETTINGS_POWER_SCREEN_WALLPAPER` ("壁纸" / "Wallpaper"), `SETTINGS_POWER_SCREEN_COVER` ("书籍封面" / "Book cover").
- Flash: ~49.8 KB free in the OTA slot — no new libraries.
- Build check before each firmware commit: `./tools/idf.ps1 build` exit 0, no new warnings. Host tests: `python tools/tests/test_text_break.py` → `text_break: PASS`.
- Match surrounding style (Chinese `//` comments in these folders); don't touch `components/`.

## Review Focus

1. **Book switched twice quickly** — only the newest book's cover is written; an older job never overwrites it → test `tag_matches` (Task 1) + generation check (Task 3) + device check.
2. **SD card removed after the cover was prepared** — standby/power-off fall back, no hang → device check (Task 6); resolve checks file existence (Task 3).
3. **Book with no cover (TXT, cover-less EPUB) opened while Book cover is on** — tag cleared, fallback shown → Task 3 step + device check.
4. **Cover much wider than tall (landscape art) or tiny (100×150)** — fitted inside the panel with bands, never cropped or zero-sized → `fit_to_panel` cases (Task 1).
5. **Book cover turned on before any book was opened since boot** — nothing ready, fallback shown; opening a book later makes it appear → Task 3/5 device check.

---

### Task 1: Pure cover-fit, resize and tag rules

**Files:**
- Create: `main/reader/screen_cover_rules.h` (header-only, `<cstddef>`, `<cstdint>`, `<cstring>`, `<algorithm>` only)
- Modify: `tools/tests/test_text_break.cpp` (include + tests), `tools/tests/README.md`, `tools/tests/README_zn.md` (one line: also covers screen-cover rules)

**Interfaces — Produces (`namespace reader`, `inline`):**
- `bool FitToPanel(int width, int height, int max_width, int max_height, int* fitted_width, int* fitted_height);` — scales up or down, keeps aspect, result ≥ 1×1 and ≤ max; false on non-positive input or null outputs.
- `void ResizeL8Bilinear(const uint8_t* src, int src_w, int src_h, uint8_t* dst, int dst_w, int dst_h);` — bilinear, corner pixels map to corner pixels.
- `bool ScreenCoverTagMatches(const char* tag, size_t tag_len, const char* book_path);` — true when the tag text, ignoring trailing `\r`/`\n`, equals `book_path` exactly; false for empty tag or null/empty path.

- [ ] **Step 1: Write the failing tests** (add to `main`):
  - `fit_to_panel`: `(600,800,684,1216)→684×912`; `(320,428,684,1216)→684×914`; `(2000,1000,684,1216)→684×342`; `(100,150,684,1216)→684×1026`; `(684,1216,684,1216)→684×1216`; `(0,800,…)` returns false.
  - `resize_bilinear`: 2×2 `{0,255,255,0}` → 4×4: `dst[0]==0`, `dst[3]==255`, `dst[12]==255`, `dst[15]==0`; a uniform 3×3 of 128 → 7×5 all 128; 1×1 `{77}` → 3×2 all 77.
  - `tag_matches`: `("/sdcard/a.epub", "/sdcard/a.epub")` true; `"/sdcard/a.epub\n"` true; `"/sdcard/a.epub\r\n"` true; `"/sdcard/b.epub"` false; prefix `"/sdcard/a.ep"` false; empty tag false; null path false.
- [ ] **Step 2:** `python tools/tests/test_text_break.py` → compile error (`FitToPanel` not declared).
- [ ] **Step 3:** Implement the three functions.
- [ ] **Step 4:** `python tools/tests/test_text_break.py` → `text_break: PASS`.
- [ ] **Step 5: Commit** — `git commit -m "reader: add screen cover fit, resize and tag rules"`.

### Task 2: Book-cover flags in the wallpaper state

**Files:**
- Modify: `main/display/screen/wallpaper_screen/wallpaper_active.h`, `wallpaper_active.cc` (`LoadAllFromNvsIntoCache` :135, keys near :22)

**Interfaces — Produces:**
- `bool Wallpaper_BookCoverEnabled(bool for_standby);` — RAM atomic read, any task.
- `void Wallpaper_SetBookCoverEnabled(bool for_standby, bool enabled);` — updates RAM then NVS (`Settings(kNvsNs, true).SetBool(...)`); non-LVGL, internal-RAM-stack task only.
- Hydrate: `LoadAllFromNvsIntoCache()` also loads `stb_cover` / `off_cover` (default false) into two `std::atomic<bool>`.

- [ ] **Step 1:** Implement (state glue; no host test — NVS-bound).
- [ ] **Step 2:** `./tools/idf.ps1 build` → exit 0, no new warnings.
- [ ] **Step 3: Commit** — `git commit -m "wallpaper: persist book-cover choice for standby and power-off"`.

### Task 3: Screen cover preparation and resolution

**Files:**
- Modify: `main/reader/book_cover_sidecar.{h,cc}` — extract the cover-extraction branch of `SaveBookCoverSidecar` (:178-232) into a public `DecodeBookCoverToL8`, and make `SaveBookCoverSidecar` use it (behaviour unchanged).
- Modify: `main/boards/common/storage/sd_paths.h` — `SD_PATH_SCREEN_COVER`, `SD_PATH_SCREEN_COVER_TAG` next to `SD_PATH_BOOK_CACHE` (:97).
- Create: `main/display/screen/wallpaper_screen/book_cover_wallpaper.{h,cc}`
- Modify: `main/CMakeLists.txt` only if `wallpaper_screen/*.cc` is not already globbed (check first).

**Interfaces:**
- Consumes: Task 1 rules; Task 2 `Wallpaper_BookCoverEnabled`.
- Produces:
  - `bool reader::DecodeBookCoverToL8(const char* book_path, int max_w, int max_h, RasterImage& out);` — EPUB via `EpubDocument::DecodeCoverImageToL8`, `.ebook` via `EbookDocument::PeekCoverImageBytes` + `DecodeImageToL8`; false for other formats or no cover. Shrinks only (existing decoders).
  - `void BookCoverWallpaper_RequestPrepare(const char* book_abs_path);` — any task; copies the path, bumps a generation, starts task `"scr_cover"` (`xTaskCreatePinnedToCoreWithCaps`, stack 8192 PSRAM, priority `tskIDLE_PRIORITY + 1`, core 0). No-op when both flags are off. If a job is already running, the running job sees the new generation, stops, and the newest request runs next (keep one pending path).
  - `bool BookCoverWallpaper_Ready();` — RAM flag: tag matches the latest book and the file was written/validated.
  - `bool BookCoverWallpaper_TryResolvePath(char* out, size_t len);` — decode tasks only: requires `Ready()`, latest book from `reader::book_home_snapshot::LoadLatestAbsPath()` equal to the book the flag was set for, and the file exists; writes `SD_PATH_SCREEN_COVER`.
- Job rules (inside `sd_access::Transaction`): if the tag file matches the book and the cover file exists → set ready, done. Else `DecodeBookCoverToL8(book, DISPLAY_WIDTH, DISPLAY_HEIGHT, l8)`; on false → delete tag, ready=false, done. Else `FitToPanel` → `ResizeL8Bilinear` into a PSRAM `RasterImage` when the size differs → `EncodeL8ToA2l8` → `WriteFileAtomic` cover → `WriteFileAtomic` tag → ready=true for that book. Before each write, abort if the generation changed. Log one line per outcome (no paths of user files beyond the book basename).

- [ ] **Step 1:** Refactor `DecodeBookCoverToL8` out of `SaveBookCoverSidecar`; build → exit 0 (shelf covers unchanged).
- [ ] **Step 2:** Implement `book_cover_wallpaper.{h,cc}` and the two paths.
- [ ] **Step 3:** Hook in `book_open_worker.cc` after the existing sidecar block (:242-250), still inside `if (ok && token matches)`, for every format: `BookCoverWallpaper_RequestPrepare(ctx->info.path.c_str());` (TXT → job clears the tag).
- [ ] **Step 4:** Hook at boot in `Application::Start` right after `reader::book_home_snapshot::HydrateFromNvs()` (`application.cc:1823`): request prepare for `LoadLatestAbsPath()` when non-empty (job only verifies the tag on the common path).
- [ ] **Step 5:** `python tools/tests/test_text_break.py` → PASS; `./tools/idf.ps1 build` → exit 0, no new warnings.
- [ ] **Step 6: Commit** — `git commit -m "wallpaper: prepare a full-screen cover for the current book"`.

### Task 4: Standby and power-off show the cover

**Files:**
- Modify: `main/display/screen/standby_screen/standby_screen.cc:94-96` (`PreferWallpaperVariant`)
- Modify: `main/display/screen/standby_screen/standby_wallpaper.cc` — `StandbyWallpaper_Build` overlay decision and `LoadTask` (:89-118)
- Modify: `main/display/lv_adapter_epdiy.cc` — power-off overlay decision and task (~:1060-1135)

**Interfaces — Consumes:** `Wallpaper_BookCoverEnabled`, `BookCoverWallpaper_Ready`, `BookCoverWallpaper_TryResolvePath`.

- Rules:
  - `const bool use_cover = Wallpaper_BookCoverEnabled(true) && BookCoverWallpaper_Ready();` decided on the UI thread (RAM only).
  - Standby variant = Wallpaper when `Wallpaper_HasStandbySelection() || use_cover`; overlay = `!use_cover && (existing overlay rule)`.
  - Standby `LoadTask`: when cover mode, decode `BookCoverWallpaper_TryResolvePath` → `DecodeImageFileToL8(path, size.x, size.y, img)`; on failure fall through to the existing standby-wallpaper decode; if that also has nothing, keep today's error label (*plan decision*: switching to the classic variant after an async failure needs a remount path; the ready flag makes this case rare — SD removed after preparation; recorded for the reviewer).
  - Power-off: overlay = `!cover && existing`; task order: cover (`DecodeImageFileToL8(path, DISPLAY_WIDTH, DISPLAY_HEIGHT, img)`) → `TryDecodeShutdownFromSd` → `TryDecodeShutdownBuiltin`.

- [ ] **Step 1:** Implement.
- [ ] **Step 2:** `./tools/idf.ps1 build` → exit 0, no new warnings.
- [ ] **Step 3: Commit** — `git commit -m "standby: show the current book cover when enabled"`.

### Task 5: Settings → Power choices

**Files:**
- Modify: `main/display/screen/settings_screen/settings_power_tab.cc` (`PowerUi` :56, `RefreshOptions` :63, Build :198-204)
- Modify: `main/assets/lang_home.cc` (near :704-707), `main/assets/lang_config.h`

**Interfaces — Consumes:** `Wallpaper_BookCoverEnabled`, `Wallpaper_SetBookCoverEnabled`, `BookCoverWallpaper_RequestPrepare`, `reader::book_home_snapshot::LoadLatestAbsPath`.

- Rules: one `CreateOptionRow` with two `CreateCompactOption`s (Wallpaper = 0, Book cover = 1) after each section hint (standby; power-off). `PowerUi` gains `stb_btns[2]`, `off_btns[2]`; `RefreshOptions` styles them with `SettingsStyleSelectable`. Click: if unchanged return; else update UI optimistically and `Application::GetInstance().Schedule([for_standby, on] { Wallpaper_SetBookCoverEnabled(for_standby, on); if (on) BookCoverWallpaper_RequestPrepare(latest.c_str()); }, Application::TaskDomain::Storage)` (capture `latest` from `LoadLatestAbsPath()` before scheduling); if `Schedule` returns false, show `ScreenShowToast(Lang::Strings::WALLPAPER_ENABLE_FAIL)` and refresh from state.

- [ ] **Step 1:** Implement strings, rows, handlers.
- [ ] **Step 2:** `./tools/idf.ps1 build` → exit 0, no new warnings; `python tools/tests/test_text_break.py` → PASS.
- [ ] **Step 3: Commit** — `git commit -m "settings: choose wallpaper or book cover for standby and power-off"`.

### Task 6: Verification and hand-off

**Files:** Modify `docs/agents/CODEBASE_MAP.md` (Services/Display: one paragraph on book-cover standby; Build: new size).

- [ ] **Step 1:** `python tools/tests/test_text_break.py` → PASS.
- [ ] **Step 2:** `./tools/idf.ps1 build` → exit 0; `./tools/idf.ps1 size` → fits `ota_0`; record free bytes vs 0xc730 before this plan.
- [ ] **Step 3:** Update the map; `graphify update .`; commit `docs: record book-cover standby verification`.
- [ ] **Step 4: Device checklist for the owner:**
  1. Both options Wallpaper: standby and power-off exactly as before.
  2. Standby = Book cover, open an EPUB with a cover, wait for standby: full-screen cover, centred, sharp.
  3. Power-off = Book cover: cover shown at power-off; power-off not slower.
  4. Open a different book, then standby: new cover.
  5. Open a TXT book: standby/power-off show the wallpaper fallback.
  6. Turn Book cover on right after boot before opening a book: fallback, then cover after opening one.
  7. Remove the SD card after preparing a cover: standby/power-off fall back without hanging.
  8. Opening a book feels as fast as before (log shows `scr_cover` running after the first page).
