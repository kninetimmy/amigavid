# AmigaVid

Native, offline x86 Win32 port of Almathera’s Amiga CD32 Video Creator for Windows 98 and XP. [Video-Creator-Win32-PRD.md](Video-Creator-Win32-PRD.md) is the product authority for scope, milestone order, and release acceptance.

<!-- memhub:managed-block v=1 -->
memhub-primary: true
db: .memhub/project.sqlite
rendered: .memhub/rendered/
config: .memhub/config.toml
<!-- /memhub:managed-block -->

## Session continuity

- Memhub is the source of truth for project memory. At session start, read `.memhub/rendered/PROJECT.md` once unless the hook already supplied its frame. Use `memhub recall` for facts, decisions, and tasks; `memhub locate` for code by intent. `PROJECT_LEDGER.md` is a fallback, not a session-start read.
- Route durable facts and decisions through `/wrap-up` or Memhub proposals and human review. Attribute CLI writes to the current host, for example `--actor opencode:wrap-up`. Keep memory writes in the main session, not in subagents. Refresh the local rendered view with `memhub render`.

## Development and verification

- Start with the pinned archive and source inventory, then qualify the Open Watcom v2 toolchain using the Win32 compatibility probe. Keep core logic in portable C and platform code at the Win32 boundary.
- Keep original input archives and extracted reference files under ignored `inputs/`. Verify the pinned archive hash and audit attribution and third-party redistribution rights before publishing any original code or assets.
- Before milestone 1, no build, test, or run commands existed. The Win32 probe now has proven commands in `README.md`; keep it current and record milestone, test evidence, blockers, and the next action in `PROGRESS.md`. A modern-host smoke test does not qualify Windows 98 or XP.

## Orch

Once `.orchestrator/config.toml` is present, load `orch-architect` before planning tracked-file changes. Ship changes through Delivery only after the human approves the plan. Use only the role agents and model routing selected by Orch.

<!-- orchestrator:managed:start version=1 -->
This file is partially managed by Orch (see `.orchestrator/config.toml`).
- In **Assist** mode, tracked-file changes are mechanically denied; a mutating
  request triggers read-only planning instead.
- In **Delivery** mode, work happens in an isolated per-issue worktree, never in
  this checkout directly.
- Model/effort routing, concurrency, and host plugin setup live in
  `.orchestrator/config.toml` — edit that file, not this block.
- Orch upgrades this block through Delivery. Do not hand-edit it; a hand edit
  blocks the next install/upgrade until reverted or removed.
<!-- orchestrator:managed:end -->
