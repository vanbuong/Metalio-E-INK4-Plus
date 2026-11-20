# Claude Code kit for Metalio-E-INK4-Plus — setup

Unzip into the **root of the Metalio repo** (next to `CMakeLists.txt`). It adds:

| Path | What |
|---|---|
| `CLAUDE.md` | Metalio-specific rules: understand-before-change, workflow map, README commands, hard rules (sdkconfig, endpoint boundary, vendored `components/`) |
| `.claude/skills/repo-onboarding/` | **New.** Builds/refreshes `docs/agents/CODEBASE_MAP.md` (graphify + parallel area mappers) before any change |
| `.claude/agents/area-mapper.md` | **New.** Read-only subagent that maps one folder with `file:line` citations |
| `.claude/skills/esp-firmware-debugging/`, `firmware-tdd/` | ESP32-S31 / ESP-IDF specifics for debugging and test-first work |
| `.claude/skills/<15 workflow skills>` | From obra/superpowers + mattpocock/skills, unchanged: brainstorming, writing-plans, executing-plans, subagent-driven-development, test-driven-development, systematic-debugging, verification-before-completion, requesting/receiving-code-review, finishing-a-development-branch, using-git-worktrees, dispatching-parallel-agents, using-superpowers, grilling, domain-modeling |
| `.claude/settings.json` + `.claude/hooks/session-start.sh` | Loads `using-superpowers` at session start (needs bash: Git for Windows or Linux/macOS) |
| `docs/CODING_STANDARDS.md` | Standards used when writing and reviewing |
| `tools/idf.ps1` | Runs `idf.py` with the EIM-installed ESP-IDF loaded, from any PowerShell (Claude's too) |
| `THIRD_PARTY_NOTICES.md`, `.claude/LICENSE.*` | MIT notices for the copied skills |

Then, in the Metalio repo:

1. **Check the toolchain**: `./tools/idf.ps1 --version` should print `ESP-IDF v6.1.x`. It picks the
   newest ESP-IDF **6.1.x** among the EIM installs (other versions such as 5.5 are ignored, whatever
   EIM has selected) and runs idf.py in a child process, so your own shell stays untouched. To force
   one install: `$env:GW_IDF_PATH = 'C:\esp\v6.1.1\esp-idf'`. Then `./tools/idf.ps1 build` once yourself. If PowerShell
   says scripts are disabled: `powershell -ExecutionPolicy Bypass -File .\tools\idf.ps1 build`.
   `CLAUDE.md` → "Commands" uses this wrapper; adjust it if your setup differs (e.g. WSL instead).
2. **Install graphify (optional, recommended)** — offline structural graph of the C/C++ code:
   ```
   uv tool install graphifyy        # or: pipx install graphifyy   (Python >= 3.11.5)
   graphify install --project       # writes graphify's own skill into .claude/skills/graphify/
   ```
   - Use **code-only** mode (`graphify extract . --code-only`): tree-sitter parsing, no LLM, no API
     key, nothing leaves the PC. Without `--code-only`, docs/PDFs/images are sent to an LLM backend.
   - This repo mixes C and C++: `.h` files are ambiguous for graphify (a wrong guess fails silently
     with few nodes). Declare them in `.graphifyrc` as its README shows — likely `language.h=cpp`;
     check the exact key/value against graphify's docs for your version.
   - Keep `components/`, `build/`, `main/assets/`, fonts, datasheets out of the graph (check how
     your graphify version excludes paths; it may honour `.gitignore`).
   - Don't enable `--strict` hooks: they block normal Read/Grep, which the skills rely on.
   - Add `graphify-out/` (or whatever output folder it uses) to `.gitignore` unless you want to commit it.
3. **First session**: ask Claude *"run repo-onboarding"*. It writes `docs/agents/CODEBASE_MAP.md`
   (commit it), then every later feature starts from that map.
4. Optional: if you want issue/ticket skills later, mattpocock/skills' `triage`, `to-spec`,
   `to-tickets` need an issue tracker set up with its `setup-matt-pocock-skills` skill.

Not included on purpose: this kit does **not** copy the gateway project's own `CLAUDE.md`,
WSL build scripts or host-test setup — those describe a different codebase and would mislead Claude.
