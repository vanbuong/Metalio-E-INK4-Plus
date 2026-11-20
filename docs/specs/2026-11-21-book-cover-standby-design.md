# Book cover as standby / power-off screen — design

Status: draft for approval · 2026-11-21 · areas: `main/display/screen/wallpaper_screen/`,
`main/display/screen/standby_screen/`, `main/display/lv_adapter_epdiy.cc`,
`main/display/screen/settings_screen/settings_power_tab.cc`, `main/reader/book_cover_sidecar.*`,
`main/display/screen/book_screen/reader/book_open_worker.cc`

## Intent
The owner wants the e-paper device, while asleep or switched off, to show the cover of the book
they are currently reading — like a physical book lying on the desk. The cover must follow the
reading automatically: opening a different book changes the picture with no extra steps.

## Goals
1. Settings → Power gets two choices, one per screen:
   `Standby screen [ Wallpaper | Book cover ]` and `Power-off screen [ Wallpaper | Book cover ]`.
   Default **Wallpaper** = exactly today's behaviour.
2. **Book cover** shows the cover of the most recently opened book (the first entry of the
   home-screen recent list), updated automatically when another book is opened.
3. The whole cover is shown, scaled as large as fits 684×1216 (portrait panel), centred, white
   bands above/below (or at the sides) — never cropped.
4. The cover is sharp and fills the panel: a screen-size cover is prepared after a book is
   opened, scaled up or down to fit 684×1216 (*decided from the review*: enlargement happens
   only in this preparation step; the existing decode/blit path never enlarges and stays
   unchanged, so existing wallpapers look the same).
5. Opening a book is never slower because of this feature.
6. Fallback, never blank: if there is no usable cover (TXT book, EPUB/.ebook without cover, no
   book opened yet, file missing, decode failure), the screen shows what **Wallpaper** would have
   shown — standby: the selected standby wallpaper or else the classic clock/weather standby;
   power-off: the selected power-off wallpaper or else the built-in image.

## Non-goals
- A designed "now reading" screen with title/progress (approach C) — later, if wanted.
- Overlay mode for the cover (cover drawn over the current UI). Book cover is always full screen.
- A cover choice inside the wallpaper library screen; the setting lives only in Settings → Power.
- Picking a specific book's cover (that would be a static wallpaper — out of scope).
- Landscape-specific layout beyond "fit and centre" using the existing display size.

## Anti-goals
- Power-off getting slower or hanging: the power-off path must never wait on cover extraction;
  it only decodes a ready file, with the existing built-in fallback.
- NVS or slow SD work on the LVGL thread or on PSRAM-stack decode tasks (existing rule in
  `wallpaper_active.h` / `standby_wallpaper.cc`).
- Breaking today's wallpaper behaviour when the option is **Wallpaper**.
- Noticeable flash growth: the app has ~49.8 KB free in its OTA slot.

## Constraints (verified in code)
- Standby/power-off images are resolved from RAM caches in `wallpaper_active.cc`
  (`TryResolveSlotPath`) because decode tasks run on PSRAM stacks and must not read NVS.
- Standby picks its variant up front: `Wallpaper` if `Wallpaper_HasStandbySelection()`,
  else `Classic` (`standby_screen.cc:95-115`); the image is decoded later on a task
  (`standby_wallpaper.cc:89-118`, `DecodeImageFileToL8`).
- Power-off decodes on a task (`lv_adapter_epdiy.cc:970-1090`): SD file via
  `Wallpaper_TryResolveActiveWallpaperPath`, else `TryDecodeShutdownBuiltin`.
- `reader::book_home_snapshot::LoadLatestAbsPath()` reads a mutex-guarded RAM list — safe from
  the decode tasks.
- `DecodeImageFileToL8` reads A2L8, A2I1 and JPEG (`image_util.cc:183-193`); shelf cover
  sidecars are A2L8 at 320×428 (`reader_image_budget.h:9-10`), created in
  `book_open_worker.cc:242-250` via `reader::SaveBookCoverSidecar` (EPUB/.ebook only).

## Approaches
- **A. "Book cover" mode resolved at draw time (chosen).** A per-screen flag; when on, the draw
  task resolves the latest book → its screen-size cover file → decode; otherwise or on failure,
  today's path. No files in the wallpaper library, always current.
- **B. Copy the cover into the wallpaper folder on every book open.** Simple drawing, but an SD
  write per open, a changing file in the user's wallpaper list, can lag. Rejected.
- **C. Designed "now reading" screen.** New layout work; deferred.

## Design (decided)

### 1. Settings and state
- Two booleans, persisted in NVS namespace `wallpaper` (keys `stb_cover`, `off_cover`, default 0),
  loaded into the existing RAM cache by `Wallpaper_HydrateFromNvsNow()` / hydrate, written by
  setters that run off the LVGL thread (same rule as `Wallpaper_Set*FromFile`).
- API added to `wallpaper_active.h`:
  `bool Wallpaper_BookCoverEnabled(bool for_standby);` (RAM read, any task) and
  `void Wallpaper_SetBookCoverEnabled(bool for_standby, bool enabled);` (non-LVGL task).
- Settings → Power: a two-choice row under the Standby section and one under the Power-off
  section, labels `Wallpaper` / `Book cover` (zh: `壁纸` / `书籍封面`).

### 2. Screen-size cover for the current book
- One file holds the current book's full-screen cover: `screen_cover.a2l8` in the book cover
  cache directory (next to the shelf sidecars), plus a tag naming the book it was made from
  (*my call*: a small companion file `screen_cover.txt` holding the book's absolute path).
- Prepared by a separate low-priority background task that the book-open worker *starts* after
  a successful open (it does not wait for it, so the first page is not delayed), only when either
  option is on and the book is EPUB/.ebook. If the tag already names this book and the file
  exists, nothing is done; otherwise extract the embedded cover, decode it and scale it (up or
  down, aspect kept) to fit 684×1216 L8, write file and tag atomically (temp file + rename).
  A book without a cover clears the tag so the fallback is used.
- Latest wins *(my call, from the review)*: a generation counter; a job whose book is no longer
  the latest opened book stops early and never writes. SD work goes through the existing
  `sd_access` admission like other background SD jobs, so standby/power-off pausing still works.
- Turning an option on in Settings starts the same job for the latest book.
- A RAM flag records "screen cover ready for <latest book>" so the standby variant decision on
  the UI thread needs no SD access.

### 3. Resolution at draw time
`bool Wallpaper_TryResolveBookCoverPath(char* out, size_t len)` (decode tasks only): latest book
from `LoadLatestAbsPath()`; the screen cover file if its tag matches; else false
(*my call, from the review*: no shelf-thumbnail fallback — it would show as a small 320×428
picture, so the wallpaper fallback is better).
- Standby: if `Wallpaper_BookCoverEnabled(true)` and a cover is ready, the variant is
  `Wallpaper` and the load task decodes the cover (never overlay); on any failure it decodes the
  selected standby wallpaper if there is one, otherwise the standby shows the classic screen.
- Power-off: if `Wallpaper_BookCoverEnabled(false)`, try the cover first (full screen, no overlay),
  then the existing SD wallpaper, then the built-in image.

### 4. Placement
The screen cover file is already panel-fit, so `DecodeImageFileToL8(path, DISPLAY_WIDTH,
DISPLAY_HEIGHT, img)` returns it at full fitted size and the existing blit
(`BlitL8ToEpdFullScreen`) centres it on white; no cropping.

## Verification
- Host tests for any pure helper extracted (e.g. tag match / fallback choice) in
  `tools/tests/` with the existing g++ runner.
- Build + size (`./tools/idf.ps1 build`, `size`); report growth against the ~49.8 KB free.
- On device: each combination of the two options; EPUB with cover, TXT book, book switch, option
  toggled before any book was opened, SD card removed, power-off timing unchanged.

## Left to the builder
- Exact cache directory constant and atomic-write helper to reuse.
- How the standby variant decision reads the RAM "ready" flag and how the load task falls back.
- The extraction refactor in `book_cover_sidecar.cc` (decode the embedded cover at a given size).
- String IDs and the exact row widget used in `settings_power_tab.cc`.
