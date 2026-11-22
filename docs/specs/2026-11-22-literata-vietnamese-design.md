# Literata built-in font + Vietnamese UI — design

## Intent
Boot with the Bookerly system font from SD takes ~10 s (FreeType rasterisation every boot). The user
reads English and Vietnamese only, so: drop Chinese, replace built-in MiSans with Literata stored in
flash (instant, zero rasterisation), and replace the Chinese UI language with Vietnamese (Tiếng Việt).

## Goals
- Built-in UI font = Literata (OFL), pre-rasterised fontpack in `font_data` flash; no boot font work.
- Home 160 px clock digits = Literata (compiled C font).
- System font picker kept (option B): "Built-in (Literata)" is the default; SD fonts (Bookerly…) still
  selectable and still load in the background after boot.
- UI languages: English (default) and Tiếng Việt; Chinese removed.

## Non-goals
- Reader body font (own SD font picker) unchanged.
- `epd_font24` (unused Chinese 24 px bitmap) not removed — reported as dead code.
- Server-side assistant language.

## Approaches
1. **Pre-rasterised Literata fontpack in `font_data` (chosen)** — mmap, instant, existing loader.
2. Literata OTF in flash + FreeType at boot — still ~seconds of rasterisation; rejected.
3. Literata compiled into the app as LVGL C fonts — app slot has ~25 KB free; rejected.

## Design
- `tools/ttf-fonts/Literata/`: LiterataBook.otf, LiterataBook-Semibold.otf, OFL.txt.
- `tools/fontpack/build_literata.sh` + charset file (ASCII, Latin-1, Latin Ext-A, Vietnamese,
  punctuation): Book → 18@2, 25@2, 30@2; Semibold → 30@4; merge Noto Emoji 30@2 and Latin Modern Math
  18/28/36@2 as today. Output → `main/assets/fonts/font.fontpack`.
- `fontpack_lv_font_builtin` smoke test glyph: 'A' first (no 中).
- `font_literata_160_2.c` (digits, ':', ' ') replaces `font_misans_regular_160_2.c`.
- System font: `ParseChoice` absent/malformed → `builtin`; label "Built-in (Literata)"; `IsBuiltinFamily`
  hides "literata" too. Saved `ttf:Bookerly` keeps working.
- Lang: `zh-CN` column → `vi-VN` Vietnamese; `LANG_DEFAULT_CODE "en-US"`; unknown codes (incl. saved
  zh-CN) → en-US. Call sites that tested `zh-CN` either use Vietnamese data if present or English.
  Shutdown background: English image for both.

## Left to the builder
Exact Vietnamese wording; how each `zh-CN` call site maps (data with only zh/en fields → English).
