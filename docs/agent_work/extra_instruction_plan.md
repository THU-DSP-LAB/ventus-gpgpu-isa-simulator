# Extra Instruction Reimplementation Plan

- Purpose: Stage the missing Ventus custom instruction reimplementation in reviewable commits.
- Base commit: `9e4d25df9143a8ade0c6282e8b4f519f603d4f81`
- Created: `2026-05-20T10:10:00Z`

## Stage 1: Scalar Custom Arithmetic And Decode Wiring

- Implement explicit 32-bit instruction length handling for reclaimed Ventus custom opcode spaces.
- Add opcode, instruction header, build list, and disassembly support for:
  - `vadd/vmul/vfma.f16x2`
  - `vadd/vmul/vfma.bf16x2`
  - `vcvt.fp32.fp16`, `vcvt.fp16.fp32`, `vcvt.fp32.bf16`, `vcvt.bf16.fp32`
  - SFU approximate FP32, FP16x2, and BF16x2 instructions listed in the requirements.
- Add a local Ventus custom arithmetic implementation without `mma-sim`, `unfu`, LUT files, mock paths, or silent fallback behavior.
- Preserve existing `ventus_shuffle.h` strict state checks.
- Add focused failing-first tests for opcode/disassembly and arithmetic helpers before implementation.

## Stage 2: MMA Reimplementation

- Implement `mma.m*` instruction headers, opcode/disassembly/build wiring, and local matrix math.
- Re-create observable shape decode and A/B/C/D vector register packing from the reference branch.
- Support FP16, BF16, and TF32-style input modes with host floating-point arithmetic.
- Let unsupported type/layout/register states throw explicit illegal instruction traps or explicit errors.
- Add focused decode and execution tests covering representative shape/type/layout cases.

## Final Verification

- Build Spike from this worktree.
- Run focused unit/decode tests.
- Run a reasonable number of Ventus simulation cases exercising the new instructions.
- Record exact commands and outcomes in supervisor and final review documents.
