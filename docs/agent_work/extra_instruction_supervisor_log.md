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
- `2026-05-20T10:59:00Z`: Stage 1 worker committed `d68c338f0d1d778e6ef48c5b8ef442b942d37a9c` (`Implement Ventus packed custom instructions`) and reported a clean worktree.
- `2026-05-20T11:02:00Z`: Dispatched Stage 2 worker `Ptolemy` for MMA opcode/disassembly/build wiring, local MMA arithmetic and packing implementation, failing-first tests, and internal reviewer rounds. Added supervisor Stage 2 review checklist.
- `2026-05-20T11:52:00Z`: Stage 2 supervisor reviewer `Copernicus` failed review with P1: MMA reads/computes full 32-lane warp state but only partially writes back according to `vl/vstart/active mask`, creating mixed warp-level semantics. Returned to worker to make MMA state semantics explicit and tested.
- `2026-05-20T12:13:00Z`: Stage 2 worker fixed MMA full-warp policy, all-lane writeback, TF32 truncation, vector-state validation, and MMA-specific disassembly. Worker completed five internal reviewer rounds; final reviewer passed.
- `2026-05-20T12:16:00Z`: Supervisor reran Stage 1/2 static and semantic helper tests, targeted object build, forbidden dependency search, and diff check. Supervisor reviewer `Aristotle` passed Stage 2. Residual risk: full Spike program-level execution still pending for final verification.
- `2026-05-20T12:33:00Z`: Final program-level verification found a real Stage 2 bug: `spike-dasm` decoded base encoding `0x0000000a` as `mma.m8n8k16`, but Spike execution trapped illegal because encoded `abtype=0, cdtype=0` mapped to an unsupported helper type combination.
- `2026-05-20T12:43:00Z`: Dispatched rework worker `Ampere` for explicit MMA encoding decode, tests for all eight base MMA encodings, and internal reviewer confirmation.
- `2026-05-20T12:56:00Z`: MMA rework completed and internal reviewer passed. Supervisor reran Stage 1/2 static tests, Stage 1/2 semantic helper tests, targeted build checks, forbidden dependency search, `git diff --check`, and `spike-dasm` decode checks; all passed.
- `2026-05-20T12:58:48Z`: Supervisor reviewer `Hume` passed the MMA rework. Program-level Spike checks passed for `mma.m8n8k16` alone and for a mixed bare-metal program executing `vadd.f16x2`, `vmul.f16x2`, `vcvt.fp32.fp16`, `vsqrt.approx.f32`, and `mma.m8n8k16`; the existing OpenCL `vadd.f16x2` program-level check also passed.
