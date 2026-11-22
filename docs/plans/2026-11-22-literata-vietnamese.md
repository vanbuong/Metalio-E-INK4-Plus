# Literata built-in font + Vietnamese UI — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Built-in UI font becomes a pre-rasterised Literata fontpack in flash (no Chinese), clock digits
become Literata, the system font defaults to built-in, and the Chinese UI language becomes Vietnamese.

**Architecture:** Reuse `tools/fontpack` (FreeType → EFNT fontpack, mmapped from `font_data`), the existing
system-font role layer (fallback = built-in pack), and the X-macro string table in `lang_home.cc`
(the zh column becomes vi).

**Tech Stack:** ESP-IDF 6.1, LVGL 9.5, Python (freetype-py, fonttools), lv_font_conv (npx), g++ host tests.

**Spec:** `docs/specs/2026-11-22-literata-vietnamese-design.md`

## Global Constraints
- Build with `$env:IDF_PY_BUILD_JOBS='1'; ./tools/idf.ps1 build` (PC memory); no new warnings.
- Do not commit `fonts/`; Literata OTFs + OFL.txt go to `tools/ttf-fonts/Literata/` (OFL permits it).
- Language codes: `en-US` (default) and `vi-VN`; anything else → `en-US`.
- Built-in system font choice string stays `"builtin"`; absent/malformed NVS → `builtin`.
- Fontpack roles: Book 18@2, 25@2, 30@2; Semibold 30@4; plus Noto Emoji 30@2 and math 18/28/36@2.

## Review Focus
- Saved NVS `language=zh-CN` from an older firmware → English, no crash (Task 4 host-free: code review of `NormalizeCode`).
- Saved NVS `sys_font=ttf:Bookerly` still loads Bookerly (unchanged path; `ParseChoice` test).
- Vietnamese glyph missing in the pack → tofu; charset test checks all U+1EA0–1EF9 present in the pack (Task 1 verify).
- 18@2 role: before, only math glyphs existed at 18@2; now Literata text must render (Task 1 verify lists 'A' at 18@2).
- Clock "slash" variant copies line_height/base_line from the 160 font — must still look aligned (device check).

---

### Task 1: Literata fontpack

**Files:** Create `tools/ttf-fonts/Literata/{LiterataBook.otf,LiterataBook-Semibold.otf,OFL.txt}`,
`tools/fontpack/charset_latin_vi.txt`, `tools/fontpack/build_literata.sh`; replace `main/assets/fonts/font.fontpack`;
update `tools/fontpack/README.md` (production default line).

- [ ] Create venv `tools/fontpack/.venv` and install `requirements.txt`.
- [ ] Charset: every code point in `sysfont::InCoverage` ranges (0x20–0x7E, 0xA0–0x17F, 0x1A0–0x1A1, 0x1AF–0x1B0, 0x1EA0–0x1EF9, 0x2010–0x2027, 0x2030–0x203A, 0x20AC, 0x2122) plus `…` `•` `←→↑↓` `°` `✓`.
- [ ] Script mirrors `build_misans_mixed.sh`: `--chars-file charset_latin_vi.txt --variant-map "18:2=Book,25:2=Book,30:2=Book,30:4=Semibold"`, then emoji and math merges; copies result to `main/assets/fonts/font.fontpack`.
- [ ] Verify: `--verify` passes; a Python check reads the pack and finds 'A' at 18@2/25@2/30@2/30@4, every U+1EA0–1EF9 at 30@2, no U+4E2D. Pack size ≤ 5 MB (expected < 1.5 MB).
- [ ] Commit.

### Task 2: Built-in default and labels

**Files:** `main/display/font/system_font_rules.h`, `main/display/font/fontpack_lvgl.cc:363`,
`main/assets/lang_home.cc` (SETTINGS_SYSTEM_FONT_BUILTIN), `tools/tests/test_text_break.cpp`.

- [ ] Tests: `ParseChoice("")` and `ParseChoice("junk")` → kind `kBuiltin`; `ParseChoice("ttf:Bookerly")` → ttf/Bookerly; `IsBuiltinFamily("Literata")` and `("LiterataBook")` true; `("Bookerly")` false.
- [ ] Run `python tools/tests/test_text_break.py` → those fail.
- [ ] Implement: `ParseChoice` default `{kBuiltin,""}`; `IsBuiltinFamily` also matches "literata"; remove `kDefaultFamily` if unused. Smoke glyph in `fontpack_lv_font_builtin`: try 'A' (0x41) only. Builtin label "Built-in (Literata)".
- [ ] Tests pass; commit.

### Task 3: Literata clock font

**Files:** Create `main/display/font/font_literata_160_2.c`; delete `font_misans_regular_160_2.c`;
modify `main/CMakeLists.txt:162`, `home_hero_priv.h:12`, `home_hero.cc`, `home_hero_build.cc:68`,
`settings_test_aging_screen.cc`.

- [ ] Generate: `npx lv_font_conv@1.5.3 --font LiterataBook.otf -r 0x20,0x30-0x3A --size 160 --bpp 2 --format lvgl --no-compress --lv-include lvgl.h -o font_literata_160_2.c` (symbol `font_literata_160_2`); confirm it compiles with LVGL 9 (fix header if needed).
- [ ] Replace symbol everywhere; `home_hero.cc` slash copy keeps its line_height/base_line override rule.
- [ ] Build passes; commit.

### Task 4: Vietnamese replaces Chinese

**Files:** `main/assets/lang_home.cc`, `main/assets/lang_config.h:4,8,13`, `settings_language_tab.cc`, and the
`zh-CN` call sites: `app_store.cc:293`, `lv_adapter_epdiy.cc:996`, `app_store_screen.cc:584,692,702,704,818`,
`assistant_idle.cc:18`, `ap_transfer.cc:339`, `home_hero.cc:198`, `home_screen.cc:105`,
`settings_conversation_tab.cc:67`, `standby_classic_weather_fetch.cc:242`, `standby_classic_weather_store.cc:140`,
`book_bookmarks.cc:22`.

- [ ] Table: rename macro params `zh`→`vi`, `kZh`→`kVi`; every zh literal replaced by Vietnamese (format specifiers and `\n` preserved, same order). Script check: count of `%` specifiers equal between vi and en per row; no CJK code point left in `lang_home.cc`.
- [ ] `LANG_DEFAULT_CODE "en-US"`; `NormalizeCode`/`LiteralsFor` accept `vi-VN`; `SETTINGS_LANG_ZH_CN`→ `SETTINGS_LANG_VI_VN` "Tiếng Việt", current label likewise; `BookHomeLabel(bool)` param renamed.
- [ ] Call sites: data with only zh/en fields (app store, weather text, bookmarks, conversation) → use English unconditionally where the zh branch would show Chinese data, Vietnamese literal where the site picks between two literals; shutdown picture → English; AP page `Content-Language` `vi-VN`/`en-US`.
- [ ] `grep -rn "zh-CN" main` → none left (except comments explaining fallback).
- [ ] Build passes; commit.

### Task 5: Verify and finish
- [ ] Host tests 0 failures; `idf.py build` exit 0 no new warnings; `idf.py size` slot free noted (clock shrink expected).
- [ ] `graphify update .`; final reviewer; give the user flash commands (app + `font_data`) and device checklist.
