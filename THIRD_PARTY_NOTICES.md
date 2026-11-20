# Third-party notices (Claude Code skills)

## Copied unchanged (only `agents/openai.yaml` files removed)
- `.claude/skills/{brainstorming, dispatching-parallel-agents, executing-plans,
  finishing-a-development-branch, receiving-code-review, requesting-code-review,
  subagent-driven-development, systematic-debugging, test-driven-development, using-git-worktrees,
  using-superpowers, verification-before-completion, writing-plans}` — obra/superpowers @ 22f0b463
  (2026-11-03), Copyright (c) 2025 Jesse Vincent, MIT. License: `.claude/LICENSE.superpowers`.
- `.claude/skills/{grilling, domain-modeling}` — mattpocock/skills @ abeeaf95 (2026-11-19),
  Copyright (c) 2026 Matt Pocock, MIT. License: `.claude/LICENSE.mattpocock-skills`.
- `.claude/hooks/session-start.sh` — adapted from obra/superpowers `hooks/session-start`.

## Written for this repo (ideas adapted from the above, MIT)
`CLAUDE.md` (four principles adapted from multica-ai/andrej-karpathy-skills, MIT),
`.claude/skills/{repo-onboarding, esp-firmware-debugging, firmware-tdd}`, `.claude/agents/area-mapper.md`,
`docs/CODING_STANDARDS.md`.

To update a copied skill: re-clone its repository and copy the same folder over, then review the diff.
