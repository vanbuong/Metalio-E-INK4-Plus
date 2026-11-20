# Coding standards (for writing and reviewing changes)

The existing code is the first standard: match the file you're in (C vs C++, naming, logging,
brace style). Where the code doesn't decide, use these.

## Structure
- New hardware or board-specific behaviour goes through the board layer
  (`main/boards/metalio-e-ink4-plus/`, `main/boards/common/`), not `#ifdef`s spread across `main/`.
- Keep logic that doesn't touch hardware in functions/classes without IDF driver calls, so it can be
  tested on a PC (see the `firmware-tdd` skill).
- Prefer extending upstream (xiaozhi-esp32) structures over rewriting them; note upstream-shared
  files you change in the PR description.
- Cloud URLs only via `main/api_endpoints.h`; hosts only in `main/cloudzao_endpoints.c`.

## C / C++
- Check every fallible `esp_err_t` (`ESP_RETURN_ON_ERROR`, `ESP_GOTO_ON_ERROR`, or explicit handling
  with a log); log failures with `esp_err_to_name`.
- Bounded string handling only (`snprintf`, `strlcpy`, `std::string`); validate lengths of everything
  from the network, files on SD, BLE, or NVS.
- Memory: say where big buffers live (PSRAM vs internal/DMA); free on every path; prefer RAII in C++.
- Concurrency: LVGL only from its task or under its lock; no blocking calls in ISRs or event
  callbacks; state shared between tasks is protected (mutex/queue/atomic).
- No magic numbers: `constexpr`/`#define`/Kconfig with a short reason.

## Security
- Never log tokens, keys, passwords or personal data; no secrets in the repo.
- Treat downloaded files (OTA, e-books, fonts) as untrusted: check sizes, signatures/checksums
  where the protocol provides them.

## Review smells (judgement calls)
Mysterious names · duplicated code · long functions (> ~80 lines) · shotgun surgery across many
modules · speculative options · repeated `if (board == X)` switches · copy-pasted driver code.
