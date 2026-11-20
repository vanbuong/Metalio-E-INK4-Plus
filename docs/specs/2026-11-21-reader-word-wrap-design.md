# Reader word wrapping, alignment and symmetric margins — design

Status: draft for approval · 2026-11-21 · area: `main/reader/`, `main/display/screen/book_screen/`

## Intent

The owner reads English and Vietnamese books on the device. Today the reader splits Latin words
at any character ("TemperatureSen / sor", "ta / ke"), which makes those books hard to read.
The goal is typography comparable to CrossPoint Reader for space-separated text, without
changing how Chinese text lays out.

Root cause (verified): every horizontal layout path asks one break rule,
`VerticalCanBreak` (`main/reader/vertical_text.h:43`) via `VerticalBreakState::Observe`
(`:90-106`). The rule was written for CJK and allows a break between *any* two code points
except around opening/closing punctuation. Justification likewise counts every inter-character
gap as a stretch slot (`TextSpacingState::Observe`, expand = `!VerticalClosing(cp)`), so
justified English lines get letter-spaced.

## Goals
1. Word-boundary wrapping for horizontal text: never break inside a word; move the whole word
   to the next line when it does not fit the text width (page width minus left and right margin).
2. Justification that distributes extra space only between words in Latin text.
3. A reader setting **Alignment: Justified / Left**.
4. Equal left and right margins, one **Sides** value, 5–40 px.

## Non-goals
- Hyphenation (Liang / libhyphen patterns). Deferred by the owner; the design must not block
  adding it later (it would add break opportunities, not change the rule's shape).
- Soft hyphen (U+00AD) handling — left as today.
- Vertical writing mode: its breaking and spacing behave exactly as today (its side margins
  become symmetric, see §6).
- Other text layouts (assistant chat `assistant_paginate.cc`, WorkBuddy, A2UI).
- Changing the PC `.ebook` converter or file format.
- Full Unicode UAX #14 conformance.

## Anti-goals (a failure even if the above works)
- Any visible change to Chinese text runs — including their digits, curly quotes and `——`
  (break positions, punctuation hanging, justification). Latin words inside Chinese books
  stopping mid-word splits is the one accepted change (§1).
- A Vietnamese tone mark separated from its letter (NFD text: base letter + U+0300–U+036F).
- The TXT paginator and the TXT page materializer disagreeing on line breaks (page drift).
- Old page caches being reused with the new rules.

## Constraints
- Three horizontal layout loops share the rule and must stay consistent:
  TXT paginate `book_session.cc` ~1846-2070, EPUB/HTML ~2382-2515, TXT page materialize
  ~3368-3525 (must reproduce the paginator's breaks exactly).
- The 512-byte per-line source cap (`kMaxLineSourceBytes`) and the "word wider than the line"
  case must still terminate (hard break at capacity, as today).
- `vertical_text.h` / `text_spacing.h` are header-only and also compiled by the PC emulator
  (`tools/ebook/emulator`, `EBOOK_EMULATOR`) — keep them free of IDF dependencies.
- NVS persistence of reading prefs is by option index (`s_text_values`, `kTextKeys` in
  `book_reader_prefs.cc`); existing indices must keep their meaning.
- Repo rules: surgical changes, match surrounding style (Chinese comments in this area), no
  `sdkconfig` edits.

## Approaches considered
- **A. Script-aware break rule in the existing shared state (chosen).** Change
  `VerticalBreakState`/`VerticalCanBreak` and the spacing slot rule so Latin-like text breaks
  only at spaces and existing hyphens/dashes, while any boundary touching CJK keeps today's
  rule. One place, all three paths benefit, pure logic → host-testable, mixed CJK/Latin books
  keep working.
- **B. Vendor a UAX #14 library (libunibreak).** Correct for all scripts, but a new vendored
  component, replaces hand-tuned CJK punctuation rules, more flash and risk; little gain for
  English + Vietnamese. Rejected.
- **C. Pre-wrap in the PC `.ebook` converter.** Fixes only `.ebook`, not TXT/EPUB, and wrapping
  would no longer follow on-device font/margin changes. Rejected.

## Design (decided)

### 1. Break rule (horizontal only) — context-sensitive *(my call, from the review)*
Classify each code point into three groups:
- **CJK** (strong): CJK ideographs incl. extensions, kana, Hangul, fullwidth forms
  (U+FF00–FFEF), CJK symbols & punctuation (U+3000–303F), CJK compatibility forms.
- **Latin** (strong): letters of any non-CJK script (Latin incl. Latin-1/Extended-A/B and
  Latin Extended Additional U+1E00–U+1EFF for Vietnamese, Greek, Cyrillic) and combining
  marks U+0300–U+036F.
- **Neutral**: everything else — digits, ASCII punctuation, curly quotes U+2018–201D,
  dashes U+2010–2014, ellipsis U+2026, whitespace, symbols.

`VerticalBreakState` additionally remembers the **context** = group of the most recent
strong code point on the current line. It resets to CJK at every line start (the state is
already reset per line), so pure-Chinese behaviour is the default and the TXT page
materializer, which starts mid-book, reaches the same decisions as the paginator
*(my call, for determinism)*.

A break *before* `after` (following `before`) is allowed when:
1. `before` or `after` is CJK, **or** both are neutral and the context is CJK →
   today's `VerticalCanBreak(before, after)` exactly (so `——`, `。”“你`, `2023年` behave as today);
2. else `before` is breaking whitespace (space, tab; **not** NBSP U+00A0) and `after` is not
   whitespace;
3. else `before` is `-`, U+2010, U+2013 or U+2014, the context is Latin, and `after` is a
   strong Latin letter or a digit;
4. else **no break** (letters, digits, combining marks, apostrophes "don’t", "3.14", "U.S."
   stay joined in Latin context).

Consequence (accepted): a Latin word embedded in a Chinese book (e.g. "iPhone") no longer
splits mid-word; pure-Chinese runs, including their digits and punctuation, are unchanged.

Vertical writing keeps calling the old rule unchanged (pass a mode or keep a separate entry).

### 2. Whitespace at a break
Breaking whitespace never triggers an overflow break: it may hang past the right edge. The
line therefore ends after its trailing spaces and the next line starts with the next word
(no leading space). Trailing whitespace is excluded from the measured line width and from
justification slots.

### 3. Oversized words
If a line holds no legal break when it overflows (a single word longer than the text width,
or the 512-byte cap), break at the overflowing character as today. No hyphen is inserted.
Combining marks U+0300–U+036F, like breaking whitespace, never trigger an overflow (they
hang), so a hard break never separates a tone mark from its letter.

### 3b. Trailing punctuation squeeze *(my call, from the review)*
`CanFitTrailingPunctuation` (fitting an overflowing closing mark by tightening blank space)
stays for CJK context only. In Latin context an overflowing "end." moves the whole word to
the next line, as CrossPoint does; word spacing is never narrowed to fit it.

### 4. Justification
A gap counts as a stretch slot when the code point before it is breaking whitespace, or
(CJK case, unchanged) when either neighbour is CJK-like and the current glyph is not closing
punctuation. Gaps between Latin letters/digits/marks never stretch. Last line of a paragraph
and chapter titles stay unstretched (existing `justify` flag logic). The same slot rule must be
used by pagination (`ObserveTextGlyph`), the label renderer (`reader_page_render.h`) and the
bitmap rasterizer (`page_text_raster.cc`) so measured and drawn widths agree.

### 5. Alignment setting
New reading pref **Alignment: Justified (default) / Left**, persisted in NVS with the other
reader prefs, shown in the reader settings sheet as a two-choice switch like the existing
ones. It is applied at draw time: a line is stretched only if `item.justify` **and** the pref
is Justified (carried in `reader::TextStyle` as a new field). Line breaks do not depend on it,
so toggling never re-paginates and it must not be part of any cache key. *(my call)*
Alignment affects horizontal text only; vertical columns keep today's justification.

### 6. Symmetric side margins
- Settings sheet margin picker becomes **Top / Bottom / Sides** (was Top / Bottom / Left /
  Right). Sides edits one value, range 5–40 px, written to both `MarginLeft` and `MarginRight`.
- Keep the `BookTextOption::MarginLeft/MarginRight` enum values and NVS keys; enforce
  left == right in the prefs layer.
- *(my call, from the review)* The effective side margin (whatever `MarginValue` resolves:
  explicit value → legacy single margin → preset) is clamped to 5–40 when read, everywhere.
  Top and bottom keep their current range.
- Migration on load, on **effective** values: if both sides are on the preset (-1) and no
  legacy margin is set, leave them on the preset (preset buttons keep working). Otherwise, if
  effective left ≠ right or either is outside 5–40, store both as max(left, right) clamped to
  5–40, once. Explicit values overriding the preset is existing behaviour and stays.
- Presets: cap side margins at 40 (preset 3 `{56, 56, …}` → `{40, 40, …}`); top/bottom
  unchanged.
- *(my call)* Sides applies in both writing modes, because the two modes share one margin
  pair; vertical mode only becomes symmetric, its breaking and spacing are unchanged.

### 7. Cache invalidation
Add a layout-rules version constant to `BookSession::FontFingerprint()` so `.idx`, `.cpg` and
EPUB page caches built with the old rule are rebuilt once. Margin changes already change the
viewport and therefore the caches, as today.

### 8. Verification
- Host tests (pure C++, no LVGL) for the break rule: English words, hyphenated compounds,
  em dash, numbers with periods, apostrophes, NBSP, Vietnamese precomposed (U+1E00–U+1EFF)
  and NFD sequences, mixed "中文English中文", and a Chinese corpus (incl. `——`, `。”“`,
  `2023年`) whose break positions equal the old rule's; oversized word; trailing
  punctuation in Latin vs CJK context. `tools/tests/` has only a Python test today, so the C++
  harness is new.
- Host tests for the slot rule (which gaps stretch).
- Build, size, and on-device check by the owner: an English EPUB, a Vietnamese TXT/EPUB, a
  Chinese book (no visible change), Alignment toggle, Sides margin at 5 and 40.

## Left to the builder
- Exact code-point ranges for "CJK-like" (must at least cover what `VerticalOpening/Closing`
  and the current CJK fonts cover).
- How the vertical-mode path keeps the old rule (flag vs separate function).
- Where the migration runs inside `book_reader_prefs.cc` load, and the string IDs/labels for
  "Alignment", "Justified", "Left", "Sides" (add to the existing language tables).
- Host-test harness shape (follow the `firmware-tdd` skill; `tools/tests/` is the current
  home of host tests).
