# Supervisor Stage 1 Review Checklist

- Purpose: Review Stage 1 worker output before permitting commit.
- Base commit: `9e4d25df9143a8ade0c6282e8b4f519f603d4f81`
- Created: `2026-05-20T10:18:00Z`

## Required Checks

- `insn_length` treats Ventus reclaimed opcode spaces `0x0a`, `0x2a`, `0x42`, `0x5a`, `0x72`, and `0x7a` as 32-bit instructions.
- Existing `riscv/ventus_shuffle.h` strict checks remain intact.
- New Stage 1 instruction names have complete `MATCH_`/`MASK_`, `DECLARE_INSN`, `riscv.mk.in`, header, and disassembly entries.
- Local arithmetic code does not include or require `cmodel_mma.h`, `sfu_core.h`, `mma-sim`, `unfu`, `LUT_PATH`, or dependency downloads.
- Vector execution checks `V` extension and `vsew == e32`, respects `vstart`, writes `vstart = 0`, and does not silently skip unsupported states.
- Packed FP16/BF16 and conversion helpers are deterministic and covered by tests.
- SFU uses host math functions directly and has explicit behavior for unsupported modes.
- Failing-first test evidence is recorded in the worker document.
- All verification commands use a hard timeout where applicable.
