# ESP32 AI kit — rules, skills and graphify for Claude Code and Cursor

A drop-in kit for ESP-IDF firmware repositories. It gives an AI agent (Claude Code, or Cursor with
Claude, Grok or other models) a disciplined workflow: map the code before changing it, design →
plan → test-first implementation → review, systematic debugging, and evidence before "done".
After copying it into a project you run **one** skill, `kit-adopt`, which scans the project and
tailors the rules and skills to it.

## 1. What's inside

| Path | What it is | Project-specific? |
|---|---|---|
| `CLAUDE.md` | The rules: understand-before-change, simplicity, surgical changes, workflow map, firmware evidence, commands, hard rules, graphify use | **Yes** — marked `KIT:` sections, filled by `kit-adopt` |
| `AGENTS.md` | Entry point for Cursor and other agents: "read CLAUDE.md, use the skills" | No |
| `.cursor/rules/kit-core.mdc` | Always-applied Cursor rule: the core rules + where things are | No |
| `.cursor/rules/kit-skills.mdc` | Always-applied Cursor rule: index of all skills → open the matching `SKILL.md` | Generated (`tools/kit/gen_cursor_skills.py`) |
| `.cursor/rules/kit-graphify.mdc` | Cursor rule: ask the graph before grepping | No |
| `.claude/skills/kit-adopt/` | **Adapts the kit to the project** (scan → fill project sections → graphify → verify) | — |
| `.claude/skills/repo-onboarding/` + `.claude/agents/area-mapper.md` | Builds `docs/agents/CODEBASE_MAP.md` (graphify + parallel read-only mappers) | No (reads `CLAUDE.md`) |
| `.claude/skills/esp-firmware-debugging/`, `firmware-tdd/` | ESP-IDF debugging and test-first specifics | **Yes** — "Project specifics" / "Project test setup" sections |
| `.claude/skills/<15 workflow skills>` | obra/superpowers + mattpocock/skills, unchanged: brainstorming, writing-plans, executing-plans, subagent-driven-development, test-driven-development, systematic-debugging, verification-before-completion, requesting/receiving-code-review, finishing-a-development-branch, using-git-worktrees, dispatching-parallel-agents, using-superpowers (+ Cursor tool notes), grilling, domain-modeling | No |
| `.claude/settings.json`, `.claude/hooks/session-start.sh` | Claude Code: loads `using-superpowers` at session start (needs bash) | No |
| `docs/CODING_STANDARDS.md` | Standards for writing and reviewing | Partly — "Project conventions" |
| `tools/idf.ps1`, `tools/idf-version.txt` | Windows: run `idf.py` with the right EIM-installed ESP-IDF from any PowerShell | Version file — set by `kit-adopt` |
| `.graphifyrc`, `.graphifyignore` | graphify settings (`.h` language, excluded folders) | **Yes** — tuned by `kit-adopt` |
| `tools/kit/gen_cursor_skills.py` | Regenerates the Cursor skill index after you edit skills | No |
| `THIRD_PARTY_NOTICES.md`, `.claude/LICENSE.*` | MIT notices for the copied skills | No |

## 2. Requirements
- **Claude Code** (CLI, desktop or IDE) and/or **Cursor** (Agent mode). Any capable model works in
  Cursor; Claude models follow long skills most reliably, Grok works well with the always-applied rules.
- **Git for Windows** (provides `bash`, used by the Claude Code start hook and a few skill scripts).
- **Python 3** (graphify, the Cursor index generator).
- **ESP-IDF** installed with the **Espressif Installation Manager (EIM)** for `tools/idf.ps1`
  (other setups — WSL, Docker, `export.sh` — work too; `kit-adopt` rewrites the Commands section).
- Optional, recommended: **graphify** — `uv tool install graphifyy` or `pipx install graphifyy`
  (Python ≥ 3.11.5). Used only in `--code-only` mode: tree-sitter parsing, offline, no API key.

## 3. Install into a project
1. **Save your work.** Commit or stash first, so you can review the kit's changes as one diff.
2. **Pick the folder to unzip into**: the folder you open in Claude Code / Cursor — the repo root, or
   the firmware subfolder of a monorepo (where `apps/`, `main/` or `components/` live).
3. **Keep your own rule files.** If the project already has `CLAUDE.md`, `AGENTS.md`,
   `docs/CODING_STANDARDS.md`, `.claude/settings.json` or `.graphifyrc`, rename yours to
   `<name>.mine` (e.g. `CLAUDE.md.mine`) before unzipping — `kit-adopt` merges them back.
   Existing `.cursor/rules/*.mdc` files with other names are untouched.
4. **Unzip** the kit there (Explorer: *Extract All*; PowerShell:
   `Expand-Archive esp32-ai-kit.zip -DestinationPath . -Force`).
5. **Optional — graphify** (once per PC): `uv tool install graphifyy`.
6. **Adopt.** Open the folder in your tool and ask:
   - Claude Code: *"run kit-adopt"*
   - Cursor (Agent mode): *"Follow .claude/skills/kit-adopt/SKILL.md to adapt the kit to this project."*

   The agent scans the project (IDF version, target, apps, own vs vendored code, tests, external
   chips, build tooling), fills every `KIT:` section, merges your `*.mine` files, sets
   `tools/idf-version.txt`, tunes `.graphifyrc`/`.graphifyignore`, builds the graph, and reports what
   it couldn't determine. It commits nothing.
7. **Review** the diff (`git diff`, plus new files). Fix anything wrong in `CLAUDE.md` yourself — it is
   the most important file. Then delete the `*.mine` files and commit.
8. **Map the code**: ask *"run repo-onboarding"* (Cursor: *"follow .claude/skills/repo-onboarding/SKILL.md"*).
   It writes `docs/agents/CODEBASE_MAP.md`; commit it.

Check the toolchain: `./tools/idf.ps1 --version` → `ESP-IDF v<your version>`. If PowerShell blocks
scripts: `powershell -ExecutionPolicy Bypass -File .\tools\idf.ps1 --version`.

## 4. Daily use
Talk normally; the rules make the agent pick the right skill. Useful phrasings:

| You want | Say |
|---|---|
| A new feature | "I want X" → brainstorming → design doc → plan → you pick *Native* (inline) or *Subagent-driven* |
| A bug fixed | Paste the **whole** serial log from boot + the steps → systematic-debugging |
| To understand code | "How does X work?" — answered from the graph + map, with file:line citations |
| A review | "review the branch" → requesting-code-review |
| Finish a branch | "merge locally" / "push" → finishing-a-development-branch |

Hardware: the agent cannot see your board. It gives you the exact flash/monitor command and what to
look for, and says "not tested on hardware" until you paste the log.

### Cursor notes
- `.cursor/rules/kit-*.mdc` are always applied; they replace Claude Code's start hook. Keep Agent mode on.
- When a skill says "invoke skill X" or `superpowers:X`, Cursor opens `.claude/skills/X/SKILL.md`.
  Tool names are mapped in `.claude/skills/using-superpowers/references/cursor-tools.md`.
- Without subagents, prefer *Native* plan execution; `repo-onboarding` then maps areas one by one.
- If a model ignores the skills, start the prompt with "Follow the kit rules" or name the skill file.

## 5. Tailoring skills to a project
- Each project-specific file has `<!-- KIT:… -->` markers. Edit **inside** them so a newer kit version
  can be merged without losing your changes. Re-run `kit-adopt` any time the project changes a lot
  (new app, new IDF version, new test setup); it updates the marked sections.
- To add project knowledge to any other skill, append a section at its end:
  `<!-- KIT:PROJECT --> … <!-- /KIT:PROJECT -->`. Keep the generic body unchanged.
- A skill's frontmatter `description` decides when it fires. After adding, renaming or re-describing
  a skill, run `python tools/kit/gen_cursor_skills.py` to refresh Cursor's index.
- New project skill: create `.claude/skills/<name>/SKILL.md` with `name` + `description`
  frontmatter, then regenerate the index.

## 6. Updating the kit in a project later
Unzip a newer kit into a scratch folder, copy over the generic files (`.claude/skills/*` except the
`KIT:PROJECT` sections, `.cursor/rules/kit-*.mdc`, `tools/idf.ps1`, `KIT-README.md`), then ask
"run kit-adopt" to re-check the project sections.

## 7. Troubleshooting
| Symptom | Fix |
|---|---|
| `idf.ps1`: "No ESP-IDF X.Y.x install found" | Install that version with EIM, or set `tools/idf-version.txt`, or `$env:GW_IDF_PATH` |
| Claude Code start hook error | Install Git for Windows (`bash` on PATH) |
| graphify finds few nodes / wrong top nodes | `.h` language wrong in `.graphifyrc`, or vendored code not in `.graphifyignore`; rebuild |
| Cursor agent ignores skills | Check `.cursor/rules/kit-*.mdc` exist and are "Always"; mention the skill file in the prompt |
| Agent edits `sdkconfig` / vendored code | Point it at `CLAUDE.md` → Hard rules; make the rule more specific in `KIT:PROJECT-RULES` |

## 8. Privacy and licences
graphify runs offline in `--code-only` mode; the kit never sends code anywhere by itself (your AI tool
of course sends what it reads to its model provider). Workflow skills are © obra/superpowers and
mattpocock/skills, MIT — see `THIRD_PARTY_NOTICES.md` and `.claude/LICENSE.*`.
