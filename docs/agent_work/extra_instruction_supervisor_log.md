# Extra Instruction Reimplementation Supervisor Log

- Purpose: Coordinate reimplementation of missing Ventus custom instructions without external `mma-sim` or `unfu` dependencies.
- Base commit: `9e4d25df9143a8ade0c6282e8b4f519f603d4f81`
- Worktree: `/work/ventus-env-gh-spike-extra-instr`
- Started: `2026-05-20T10:05:54Z`

## Documentation Protocol

- Supervisor and subagents record scoped notes under `docs/agent_work/`.
- Each worker document must include: role, purpose, base commit, start time, owned files, tests written before implementation, verification commands, reviewer rounds, and final status.
- Reviewer notes must include: reviewed commit or diff base, target requirements, findings ordered by severity, and pass/fail status.
- No agent may introduce silent fallback behavior, mock success, or external dependency requirements for `mma-sim` or `unfu`.

## Log

- `2026-05-20T10:05:54Z`: Created independent Spike worktree at `/work/ventus-env-gh-spike-extra-instr` on branch `codex/extra-instructions`, based on `9e4d25df9143a8ade0c6282e8b4f519f603d4f81`.
- `2026-05-20T10:08:00Z`: Read requirement file and reference `dev-sfu-mma` implementation. Reference opcode/disasm/header wiring is usable; arithmetic code depends on forbidden `mma-sim` and `unfu`, so worker implementation must replace those pieces locally.
- `2026-05-20T10:12:00Z`: Dispatched Stage 1 worker `Darwin` for custom opcode length/decode, packed FPU, conversion, and SFU approximate instructions. Worker was instructed to write failing-first tests, run two internal reviewer rounds, and avoid committing until supervisor review.
- `2026-05-20T10:40:00Z`: Stage 1 worker reported completion after three internal reviewer rounds. Supervisor static test, semantic helper test, forbidden dependency search, diff check, and targeted object build passed.
- `2026-05-20T10:43:00Z`: Supervisor reviewer `Ohm` found P1 deterministic FP16 issue: `float_to_fp16()` used SoftFloat global rounding mode. Returned Stage 1 to worker for local RNE conversion and a regression test covering rounding-mode contamination.
- `2026-05-20T10:54:00Z`: Stage 1 worker fixed the FP16 determinism issue, added rounding-mode and NaN coverage, and passed two more internal reviewer rounds. Supervisor reran static test, semantic helper test, forbidden dependency search, diff check, and targeted object build; all passed.
- `2026-05-20T10:56:00Z`: Supervisor reviewer `Cicero` passed the Stage 1 fix review. Residual risk: Stage 1 tests are static/helper/object-build focused and do not yet run full Spike instruction execution paths. Approved Stage 1 for worker commit.
