---
name: firmware-tdd
description: How to apply test-first development in this ESP-IDF firmware. Use with test-driven-development when adding or changing logic (parsers, protocol messages, settings, reader/pagination, OTA decisions, state machines) or fixing a bug in such logic.
---

# Firmware TDD (Metalio)

Use with `test-driven-development` (the iron law: failing test first). This adds where and how
tests can run for this firmware. Adapted from obra/superpowers and mattpocock/skills (MIT).

## Where tests run here — find out before the first test
1. Look at `tools/tests/` (README calls it host-side regression tests): what framework, what it
   covers, how it's run. Run it once and record the command + result in
   `docs/agents/CODEBASE_MAP.md` (Build & run).
2. If it fits the code you're changing, add your test there, in its style.
3. If not, and the logic is pure (no IDF/FreeRTOS/driver calls), propose a **host-test seam** to
   the user before building one: the pure function moves to its own file, and a small host build
   (ESP-IDF `linux` target on Linux/WSL, or plain CMake + Unity with a desktop gcc) tests it.
   Agree on it first — it's new infrastructure in someone else's project.
4. Things only the hardware can show (EPD refresh, audio, radio, touch) get a written device
   check (steps + expected log/screen) instead of fake tests.

## The loop — one behaviour at a time
1. RED: one small test named for the behaviour, real inputs (e.g. a real EPUB snippet, a real
   server JSON message captured from logs with secrets removed).
2. Run it: it must fail for the expected reason.
3. GREEN: simplest code that passes. 4. Whole suite passes; firmware still builds.
5. Refactor only while green.
Bugs: a failing test that reproduces the bug comes first.

## Don't
- Write all tests first, then all code. Assert on internals. Recompute expected values with the
  code under test. Keep code written before its test. Skip RED because "I tested it on the board".
