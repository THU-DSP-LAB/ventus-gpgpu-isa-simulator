# Supervisor Stage 2 Review Checklist

- Purpose: Review Stage 2 MMA implementation before permitting commit.
- Base commit: `d68c338f0d1d778e6ef48c5b8ef442b942d37a9c`
- Created: `2026-05-20T11:02:00Z`

## Required Checks

- All eight MMA instructions have complete `MATCH_`/`MASK_`, `DECLARE_INSN`, `riscv.mk.in`, header, and disassembly entries.
- MMA opcode space `0x0a` remains explicitly 32-bit in `insn_length`.
- `ventus_exec_mma` uses local arithmetic only and does not include or require `cmodel_mma.h`, `mma-sim`, `unfu`, `sfu_core`, `LUT_PATH`, runtime assets, downloads, or mock paths.
- Shape decode matches the reference shape dimensions and register counts:
  - M8N8K16: 2 A, 2 B, 2 C/D
  - M16N8K16: 4 A, 2 B, 4 C/D
  - M8N16K16: 2 A, 4 B, 4 C/D
  - M16N16K16: 4 A, 4 B, 8 C/D
  - K8 variants use the same M/N register counts with K=8.
- Supported type combinations are explicit:
  - TF32-style inputs: K8 shapes only, FP32 C/D only.
  - FP16 inputs: FP16 or FP32 C/D.
  - BF16 inputs: FP32 C/D only.
  - Unsupported combinations fail explicitly.
- A/B/C/D register packing and layout bits match the reference observable behavior.
- FP16 C/D packed mode reads/writes half as packed pairs with deterministic FP16 conversion.
- Register base plus required register counts are bounds checked against `NVPR`.
- Execution checks vector state, fixed 32-lane assumption, active lanes/vstart behavior, and clears `vstart`.
- Existing Stage 1 instructions and `ventus_shuffle.h` strict checks remain intact.
- Tests include failing-first evidence, static wiring, representative helper semantics, invalid combo coverage, and targeted object build.
