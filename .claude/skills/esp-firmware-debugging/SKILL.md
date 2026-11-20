---
name: esp-firmware-debugging
description: ESP32-S31 / ESP-IDF specifics for debugging this firmware. Use together with systematic-debugging for any crash/panic, boot loop, watchdog, stack overflow, heap or PSRAM exhaustion, e-paper/LVGL glitch, audio dropout, Wi-Fi/4G/BLE/WebSocket/MQTT problem, OTA failure, or build error.
---

# ESP firmware debugging (Metalio, ESP32-S31)

Use with `systematic-debugging` (no fix before root cause). This adds the board specifics.
Adapted from obra/superpowers `systematic-debugging` and mattpocock/skills `diagnosing-bugs` (MIT).

## 1. Get a feedback loop first
1. Build error → exact `idf.py build` command + the first error (not the last).
2. Host test or tool in `tools/tests/` that reproduces it (see `firmware-tdd`).
3. Device: full serial log from boot (`idf.py -p <PORT> monitor`), with the steps taken. You can't see
   the board: ask the user for the whole log, not a snippet. Redact keys/tokens/passwords.
4. Network features: reproduce against the server with a script/curl where possible.

## 2. Read the evidence
- **Panics**: ESP32-S31 is RISC-V → decode with the monitor, or
  `riscv32-esp-elf-addr2line -pfiaC -e build/<project>.elf <addresses>`.
  `Load access fault`/`Store access fault` = bad pointer; `Stack protection fault` / canary =
  task stack; `Interrupt wdt` = blocking in an ISR or with interrupts off; `Task wdt` = a task
  starved others (long LVGL/EPD refresh, busy loop, missing `vTaskDelay`).
- **Coredump**: if enabled, `idf.py coredump-info` after a crash.
- **Memory**: log `heap_caps_get_free_size(MALLOC_CAP_INTERNAL)` and `(MALLOC_CAP_SPIRAM)` plus
  `heap_caps_get_minimum_free_size` around the failing step. DMA buffers (I2S, SDMMC, LCD/EPD)
  must be internal/DMA-capable; large UI/audio buffers belong in PSRAM.
- **Stacks**: `uxTaskGetStackHighWaterMark(NULL)` in the suspect task before enlarging anything.
- **E-paper/LVGL**: check refresh mode (full vs partial), dither/waveform, VCOM, PMIC (TPS65185)
  power-up sequence and temperature; LVGL calls only from the LVGL task (or under its lock).
- **Audio**: sample rate/format mismatch between codec, I2S and the BT chip; ring-buffer
  underruns show as clicks; check task priorities against the network task.
- **Network**: TLS error codes are negative hex (map them, don't guess); time not set → TLS
  date failures; reconnect loops → look at the backoff and event handler order.
- **Config**: what changed last — `git diff` of `sdkconfig*`, Kconfig, partitions, component versions.

## 3. Hypothesise, then test one variable
1–3 falsifiable hypotheses, most likely first, shared with the user. One change at a time.
Temporary logs tagged `[DBG-xxxx]` so they can be found and removed.

## 4. Fix and clean up
Failing test first where the logic is host-testable; one fix at the root cause; re-run the
original loop; `verification-before-completion`; remove every `[DBG-` line.

## 5. Three strikes
Three failed fixes → stop, report what was ruled out, and question the design with the user.
