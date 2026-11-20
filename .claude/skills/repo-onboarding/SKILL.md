---
name: repo-onboarding
description: Build or refresh an understanding of this firmware repo before planning or changing code. Use at the start of work in an unfamiliar area, when docs/agents/CODEBASE_MAP.md is missing or stale, or when the user asks how the project is structured or how something works.
---

# Repo onboarding: map before you change

Goal: a short, accurate, *cited* map in `docs/agents/CODEBASE_MAP.md` that later sessions read
instead of re-exploring. Facts come from files you opened, never from names alone.

## 0. Is a map already there?
Read `docs/agents/CODEBASE_MAP.md`. Check its `Mapped at: <commit>` line against `git log -1`.
- Fresh and covers your area → read it, open the 2–3 files it points to for your task, stop here.
- Stale for your area → refresh only those sections (steps 2–4 for that area).

## 1. Structural graph (fast, offline) — if graphify is installed
`graphify --version` succeeds → build a code-only graph (tree-sitter, no LLM, nothing leaves the PC):
```
graphify extract . --code-only
```
- Ensure `.h` is parsed as C++ in this mixed C/C++ repo (see SETUP notes in `docs/agents/`).
- Exclude vendored/generated trees from the graph: `components/`, `build/`, `managed_components/`,
  `main/assets/`, `tools/ttf-fonts/`, `docs/datasheets/`, `docs/images/`.
- Use it for structure: `graphify query "who calls <fn>"`, `graphify path A B`, `graphify explain X`,
  and the communities in `GRAPH_REPORT.md` as a first module split.
- The graph shows *structure*, not intent: confirm every claim you write by opening the code.
Not installed → skip; steps 2–4 work without it (slower).

## 2. Skeleton (you, ~15 min)
Read, in order: `README.md`, top-level `CMakeLists.txt`, `main/CMakeLists.txt`,
`main/Kconfig.projbuild`, `partitions/v1/16m.csv`, `sdkconfig.defaults*`, `main/app_main.cc`,
`main/application.cc/.h`, the board folder `main/boards/metalio-e-ink4-plus/` and `main/boards/common/`.
Write down: boot sequence, main tasks/threads and their priorities, how the board object is created,
which Kconfig options switch features.

## 3. Areas in parallel (area-mapper agents)
Split `main/` into areas (e.g. `display/`, `audio/`, `protocols/`, `reader/`, `boards/`,
`app_store/`, `workbuddy/`, `yunxin_binding/`, top-level services like `ota.cc`, `mcp_server.cc`,
`settings.cc`) and `tools/`. Dispatch one `area-mapper` agent per area (read-only, parallel; up to
4 at a time), each with: the area path, the skeleton notes from step 2, and the graphify output for
that area if any. Each returns a ≤ 300-word report in its fixed format.

## 4. Write the map
`docs/agents/CODEBASE_MAP.md`, at most ~400 lines:
```
# Codebase map            Mapped at: <git short sha> (<date>)
## Build & run            exact commands that worked (or "unverified")
## Boot & runtime         boot sequence, tasks (name, prio, core, stack), event/queue flow
## Modules                one block per area: purpose, entry points (file:function), owns, depends on
## Cross-cutting          logging, settings/NVS keys, Kconfig switches, memory (PSRAM vs internal), OTA
## Hardware               peripherals → driver file → bus/pins (link config.h)
## Conventions            C vs C++, naming, error handling, how a new feature is usually added
## Danger zones           timing-sensitive code, ISR/IRAM code, vendored code, upstream-shared code
## Open questions         things the code didn't answer
```
Cite `path:line` for every non-obvious claim. Commit the map with the user's OK.

## 5. Before any change
State which map sections and files you read, the call path you will touch, and what could break
(callers, other boards/variants, memory). Then hand off to `brainstorming` / `writing-plans`.
