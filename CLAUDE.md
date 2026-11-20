# Metalio E-Ink4-Plus — agent rules

ESP-IDF **v6.1** firmware for the Metalio E-Ink4-Plus (target **esp32s31**, RISC-V, 16 MB flash,
octal PSRAM), derived from **xiaozhi-esp32**. C and C++ (`.cc`), CMake, Kconfig. Board:
`main/boards/metalio-e-ink4-plus/` (pins in `config.h`), shared board code in `main/boards/common/`.
Hardware: 4.7" ED047TC2 e-paper via EPDiy + TPS65185, LVGL UI, FT6336U touch, I2S audio,
external BT audio chip (UART), GC2145 camera, SD card, TinyUSB MSC, Wi-Fi (+4G modem), RTC,
fuel gauge, charger, IO expander. Features: AI voice assistant, e-book reader, OTA, MCP server.

These rules take precedence over any skill. Skills take precedence over default behaviour.

## 0. Understand before changing (this repo is large)
- Before planning or changing anything, read `docs/agents/CODEBASE_MAP.md`. If it doesn't exist or
  is stale for the area you're touching, run the `repo-onboarding` skill first.
- Read the code you will change *and its callers* before proposing a design. Name the files.
- Upstream heritage: much of `main/` comes from xiaozhi-esp32. Prefer changes that keep it easy to
  merge upstream fixes (extend via the board layer, don't rewrite shared code without reason).

## 1. Think before coding
- State assumptions. If a request has more than one reading, list them and ask.
- If a simpler approach exists, say so. If something is unclear (an IDF API, an S31 capability,
  a peripheral's behaviour), check headers/datasheets in `docs/` instead of guessing.

## 2. Simplicity first
- Minimum code that meets the request; no speculative options or abstractions.
- Every `esp_err_t` that can fail is checked; no error handling for impossible cases.

## 3. Surgical changes
- Touch only what the task needs; match the surrounding style (C vs C++, naming, logging).
- Don't reformat or refactor neighbouring code. Mention dead code you notice; don't delete it.

## 4. Goal-driven execution
- Turn each task into a verifiable goal. Never claim "done/fixed/builds" without fresh output.

## Workflow (skills in `.claude/skills/`)
`superpowers:<name>` in a skill means the project skill `<name>`.

| Stage | Skill |
|---|---|
| First time in an area / stale map | `repo-onboarding` (uses graphify if installed + the `area-mapper` agent) |
| New feature / design | `brainstorming` → spec in `docs/specs/YYYY-MM-DD-<topic>-design.md` |
| Stress-test a design | `grilling`, `domain-modeling` (`GLOSSARY.md` + `docs/adr/`) |
| Plan | `writing-plans` → `docs/plans/YYYY-MM-DD-<feature>.md` |
| Isolate work | `using-git-worktrees` |
| Build | `executing-plans` (default) or `subagent-driven-development` (only when asked — costly) |
| Code | `test-driven-development` + `firmware-tdd` |
| Bugs | `systematic-debugging` + `esp-firmware-debugging` |
| Done? | `verification-before-completion` + firmware evidence below |
| Review | `requesting-code-review` / `receiving-code-review`, standards in `docs/CODING_STANDARDS.md` |
| Integrate | `finishing-a-development-branch` |

### Firmware evidence (for `verification-before-completion`)
| Claim | Evidence |
|---|---|
| Builds | `idf.py build` exit 0, no new warnings |
| Fits | `idf.py size` — the app still fits its OTA slot (`partitions/v1/16m.csv`); note growth > 2 % |
| Logic works | host tests (see `firmware-tdd`) run now, 0 failures |
| Works on device | the user's serial log / screen result — otherwise say "not tested on hardware" |
| Memory | for UI/audio/network changes: free internal heap + PSRAM before/after (log or `heap_caps_*`) |

## Commands (Windows, native ESP-IDF installed with EIM)
ESP-IDF v6.1.x is installed with the Espressif Installation Manager (EIM), possibly next to other
versions. A plain PowerShell — including yours — does NOT have the IDF environment, so always go
through `tools/idf.ps1`: it picks the newest 6.1.x install listed in `C:\Espressif\tools\eim_idf.json`
(override: `$env:GW_IDF_PATH`), runs `idf.py` with your arguments in a child process, and returns
its exit code. Never load an ESP-IDF activation script into your own shell.
If `idf.py` is still not found, say so; never guess build results.
- Check toolchain: `./tools/idf.ps1 --version` (expect `ESP-IDF v6.1.x`)
- Build: `./tools/idf.ps1 build` — slow the first time; run it in the background and read the tail.
- Size: `./tools/idf.ps1 size`
- Board variant: `./tools/idf.ps1 menuconfig` → Board Type (interactive: the user runs it)
- Flash + monitor (user runs it, or you with explicit permission for that port):
  `./tools/idf.ps1 -p COM<n> flash monitor` (Ctrl+] quits the monitor)
- Release image: `tools/firmware/merge_firmware.sh` is a bash script — run it from Git Bash or WSL
  with IDF loaded (unverified on Windows; check and update this line on first use).
- Never use `idf.py set-target` casually: it wipes `build/` and regenerates `sdkconfig`; the repo is
  already configured for esp32s31.
- Hardware steps: you cannot flash or watch the board unless the user lets you use the serial
  port. Give the exact command and what to look for; wait for their log before claiming success.

## Hard rules
- Don't edit `sdkconfig` by hand or commit incidental `sdkconfig` churn; change defaults in
  `sdkconfig.defaults*` or Kconfig and say why.
- Cloud endpoints: call sites only use `main/api_endpoints.h` to build URLs; hosts live only in
  `main/cloudzao_endpoints.c` (README boundary rule).
- `components/` is vendored third-party code and `patches/esp-idf/` patches ESP-IDF: don't modify
  them unless the task is about them; say so explicitly when you do.
- Never commit secrets, keys, tokens, Wi-Fi passwords or device credentials; never log them.
- Target-specific code stays behind `CONFIG_IDF_TARGET_*` / `SOC_*` caps or the board layer.
- Generated/large inputs (`main/assets/`, fonts, `docs/datasheets/`, `build/`) are not source:
  regenerate them with the scripts in `tools/` instead of hand-editing.

## graphify

This project has a knowledge graph at graphify-out/ with god nodes, community structure, and cross-file relationships.

Rules:
- When graphify-out/graph.json exists, run `graphify query "<question>"` before your first grep, search or source-file read for a codebase question, and put this rule in every subagent prompt that explores code. Read raw files only for lines the graph or an earlier read already located. Use `graphify path "<A>" "<B>"` for relationships and `graphify explain "<concept>"` for focused concepts. When the answer is under a Markdown heading, `graphify section "<heading>"` prints that section's text, so the .md need not be opened. These return a scoped subgraph, usually much smaller than GRAPH_REPORT.md or raw grep output.
- If graphify-out/wiki/index.md exists, use it for broad navigation instead of raw source browsing.
- Read graphify-out/GRAPH_REPORT.md only for broad architecture review or when query/path/explain do not surface enough context.
- After modifying code, run `graphify update .` to keep the graph current (AST-only, no API cost).
- graphify-out/ is git-ignored: the graph is local to each machine and never committed. If it is missing or stale, rebuild it offline with `graphify extract . --code-only` then `graphify cluster-only . --no-viz --no-label`. Settings live in `.graphifyrc` (`.h` parsed as C++) and `.graphifyignore` (vendored/generated trees).
