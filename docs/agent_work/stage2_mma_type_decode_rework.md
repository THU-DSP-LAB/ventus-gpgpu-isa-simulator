# Stage 2 MMA Type Decode Rework

- Role: Stage 2 rework worker
- Purpose: Fix program-level illegal instruction on base MMA encoding `0x0000000a`.
- Base commit: `f44d9b1a77584353cb8d294059491161950ad944`
- Worktree: `/work/ventus-env-gh-spike-extra-instr`
- Started: `2026-05-20T12:47:36Z`

## Reproduction

Supervisor found that `spike-dasm` decoded `0x0000000a` as
`mma.m8n8k16 v0, v0, v0`, but Spike execution threw
`trap_illegal_instruction` after executing the same instruction word in
`/tmp/ventus_extra_mma_only.riscv`.

The root cause was that `ventus_exec_mma` cast encoded bits directly into the
helper enums. Encoded `abtype=0, cdtype=0` became helper `TF32/FP16`, which is
intentionally invalid, even though the base instruction encoding must be an
executable MMA form.

## Fix

- Added explicit MMA instruction-bit decode helpers in `riscv/ventus_mma.h`.
- Mapped encoded `abtype=0, cdtype=0` to helper `FP16/FP16` so base MMA
  encodings are executable.
- Preserved encoded `abtype=0, cdtype=1` as `TF32/FP32`.
- Kept unknown encoded input types explicitly invalid; there is no fallback or
  mock success path.
- Changed `ventus_exec_mma` to call `ventus_mma::decode_mma_options`.

## Tests

- Added helper semantic coverage that decodes and executes all eight base MMA
  shape encodings:
  - `0x0000000a`
  - `0x0200000a`
  - `0x0400000a`
  - `0x0600000a`
  - `0x0800000a`
  - `0x0a00000a`
  - `0x0c00000a`
  - `0x0e00000a`
- Added coverage that an unknown encoded input type still fails explicitly.
- Added static checks that `ventus_exec_mma` uses explicit MMA decode and that
  the base encoded type maps to an executable FP16/FP16 helper option.

## Verification

- `timeout 60 python3 tests/ventus_extra_stage2_static.py`
  - Result: passed.
- `timeout 60 g++ -std=c++17 -Iriscv tests/ventus_extra_stage2_semantics.cc -o /tmp/ventus_stage2_semantics_fix && timeout 60 /tmp/ventus_stage2_semantics_fix`
  - Result: passed.
- `timeout 60 make -j2 ventus_custom.o mma_m8n8k16.o mma_m16n8k16.o mma_m8n16k16.o mma_m16n16k16.o mma_m8n8k8.o mma_m16n8k8.o mma_m8n16k8.o mma_m16n16k8.o`
  - Result: passed in `/tmp/ventus_stage2_build`.
- `git diff --check`
  - Result: passed.

## Program-Level Status

- `timeout 120 make -j2 spike`
  - Result: timed out after compiling many common Spike instruction objects;
    no compiler error was observed before timeout.
- `timeout 600 make -j2 spike`
  - Result: passed in `/tmp/ventus_stage2_build`.
- Program-level reproduction:
  - Command: `timeout 2 /tmp/ventus_stage2_build/spike -l -p1 -m0x10000:0x1000,0x80000000:0x100000 --isa=rv64gcv_zfh --varch=vlen:1024,elen:32 --gpgpuarch numw:1,numt:32,numwg:1,kernelx:1,kernely:1,kernelz:1,lsx:32,lsy:1,lsz:1 /tmp/ventus_extra_mma_only.riscv > /tmp/ventus_mma_only_stdout.fix.log 2>&1`
  - Result: exit code 0.
  - Log check: `0x80000008 (0x0000000a) mma.m8n8k16 v0, v0, v0` executed
    without `trap_illegal_instruction`.
