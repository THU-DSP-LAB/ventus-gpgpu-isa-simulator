# Stage 1 Worker Notes

- Role: Stage 1 worker
- Purpose: Reimplement Ventus reclaimed custom opcode length/decode, packed FPU, conversion, and approximate SFU instructions locally.
- Base commit: `9e4d25df9143a8ade0c6282e8b4f519f603d4f81`
- Start time: `2026-05-20T10:08:33Z`
- Worktree: `/work/ventus-env-gh-spike-extra-instr`

## Owned Files

- `docs/agent_work/stage1_worker.md`
- `tests/ventus_extra_stage1_static.py`
- `riscv/decode.h`
- `riscv/encoding.h`
- `riscv/riscv.mk.in`
- `riscv/insn_template.cc`
- `riscv/ventus_custom.h`
- `riscv/ventus_custom.cc`
- `riscv/insns/v*.h` for the Stage 1 instruction list
- `disasm/disasm.cc`

## Test Plan

- Add a focused static test before implementation and record the expected failure.
- Run the static test with `timeout 60 python3 tests/ventus_extra_stage1_static.py`.
- If build configuration is available or can be generated locally, run a bounded build command with `timeout 60`.
- Run targeted source searches to verify no `mma-sim`, `unfu`, LUT runtime requirement, mock path, or silent fallback was introduced.

## Failing-First Test

- `timeout 60 python3 tests/ventus_extra_stage1_static.py`
- Result before implementation: failed as expected.
- Failure summary: missing reclaimed `0x0a/0x2a/0x5a/0x7a` 32-bit length handling, `ventus_custom` wiring, Stage 1 opcode declarations, instruction headers, build-list entries, disassembly names, and local arithmetic helpers.

## Verification Commands

- `timeout 60 python3 tests/ventus_extra_stage1_static.py`
  - Result: passed after implementation, reviewer fixes, and supervisor P1 fix.
- `mkdir -p /tmp/ventus_stage1_build && cd /tmp/ventus_stage1_build && timeout 60 /work/ventus-env-gh-spike-extra-instr/configure --prefix="$PWD/install" --enable-commitlog`
  - Result: passed. Build products were kept outside the worktree.
- `timeout 60 make -j2`
  - Result: timed out after 60 seconds during broader repository compilation. Before timeout it compiled `riscv/ventus_custom.cc`; rerun after P0 fix showed no Stage 1 errors before timeout.
- `cd /tmp/ventus_stage1_build && timeout 60 make -j2 ventus_custom.o vadd_f16x2.o vmul_f16x2.o vfma_f16x2.o vadd_bf16x2.o vmul_bf16x2.o vfma_bf16x2.o vcvt_fp32_fp16.o vcvt_fp16_fp32.o vcvt_fp32_bf16.o vcvt_bf16_fp32.o vex2_approx_f32.o vlg2_approx_f32.o vrcp_approx_f32.o vsqrt_approx_f32.o vrsqrt_approx_f32.o vsin_approx_f32.o vcos_approx_f32.o vtanh_approx_f32.o vgelu_approx_f32.o vsilu_approx_f32.o vex2_approx_f16x2.o vrcp_approx_f16x2.o vsqrt_approx_f16x2.o vrsqrt_approx_f16x2.o vtanh_approx_f16x2.o vgelu_approx_f16x2.o vsilu_approx_f16x2.o vex2_approx_bf16x2.o vrcp_approx_bf16x2.o vsqrt_approx_bf16x2.o vrsqrt_approx_bf16x2.o vtanh_approx_bf16x2.o vgelu_approx_bf16x2.o vsilu_approx_bf16x2.o`
  - Result: passed for `ventus_custom.o` and all Stage 1 instruction objects.
- `timeout 60 g++ -std=c++17 -I/tmp/ventus_stage1_build -I. -Iriscv -Isoftfloat tests/ventus_extra_stage1_semantics.cc softfloat/f16_to_f32.c softfloat/f32_to_f16.c softfloat/s_normSubnormalF16Sig.c softfloat/s_roundPackToF16.c softfloat/s_roundPackToF32.c softfloat/s_shiftRightJam32.c softfloat/s_shortShiftRightJam64.c softfloat/s_countLeadingZeros32.c softfloat/s_countLeadingZeros8.c softfloat/softfloat_state.c softfloat/softfloat_raiseFlags.c -o /tmp/ventus_extra_stage1_semantics && timeout 60 /tmp/ventus_extra_stage1_semantics`
  - Result: passed after adding semantic coverage for packed add/mul/fma, conversion, FP32 SFU, packed SFU, invalid packed SFU mode, deterministic FP32-to-FP16 RNE behavior under non-default `softfloat_roundingMode`, and canonical FP32 NaN-to-FP16 behavior.
- `rg -n "mma-sim|dependencies/unfu|sfu_core|cmodel_mma|LUT_PATH|return \\(mock\\)|fallback" riscv/ventus_custom.* riscv/insns/v*.h -S`
  - Result: no implementation matches.
- `git status --short --untracked-files=all`
  - Result: no `build-stage1/` or other generated build products in the worktree.

## Reviewer Rounds

- Round 1 reviewer: failed initial review.
  - P0: `do_packed_sfu` used `insn` out of scope after the explicit illegal-instruction cleanup. Fixed by passing `insn_t` through that helper, then later by moving arithmetic helpers into `ventus_custom_arith.h`.
  - P1: static test did not cover arithmetic semantics. Fixed by adding `tests/ventus_extra_stage1_semantics.cc`.
  - P2: worker document was stale. Updated here.
  - P2: temporary `build-stage1/` was untracked. Removed after verification.
- Round 2 reviewer: failed review.
  - P1: `build-stage1/` generated files were still untracked and contradicted this document. Fixed by removing `build-stage1/` and moving final configure/build verification to `/tmp/ventus_stage1_build`.
  - P2: arithmetic helper semantic coverage was narrow. Fixed by adding packed `Mul`, BF16 `Mul/Fma`, more FP32 SFU operations, BF16x2 packed SFU coverage, and invalid packed SFU mode coverage.
- Round 2 follow-up fix: removed silent unreachable returns in `ventus_custom_arith.h`; invalid enum/mode paths now throw `std::invalid_argument` explicitly.
- Round 3 reviewer: passed.
  - Confirmed reclaimed opcode length, explicit helper errors, semantic coverage, no forbidden dependencies/mock paths, no worktree build artifacts, and Stage 1 target object verification.
- Supervisor review: failed with P1.
  - P1: `float_to_fp16()` called SoftFloat `f32_to_f16()`, so Stage 1 FP16 results could depend on prior `softfloat_roundingMode`.
  - Fixed by replacing Stage 1 FP32-to-FP16 conversion with a local deterministic round-to-nearest-even implementation that does not read or write SoftFloat rounding state.
  - Extended `tests/ventus_extra_stage1_semantics.cc` to set `softfloat_roundingMode = softfloat_round_minMag` before checking `float_to_fp16`, `do_vcvt(...FP16_FP32)`, one F16x2 packed op, and one F16x2 packed SFU path; the test restores the saved rounding mode before assertions.
- Round 4 reviewer: failed review.
  - P1: local `float_to_fp16()` preserved FP32 NaN sign/payload and could produce half signaling NaNs, while this repository's SoftFloat canonicalizes FP32 NaN-to-FP16 to `0x7e00`.
  - Fixed by returning canonical `0x7e00` for all FP32 NaN inputs and adding direct, conversion, packed F16x2, and F16x2 SFU NaN coverage to the semantic test.
- Round 5 reviewer: passed.
  - Confirmed local deterministic FP32-to-FP16 RNE conversion no longer depends on SoftFloat rounding state, NaN canonicalization matches this repository's SoftFloat behavior, tests cover the supervisor P1 and NaN regression, and no worktree build products were introduced.

## Final Status

- Latest local verification completed at `2026-05-20T10:52:21Z`.
- Stage 1 worker changes are ready for supervisor review after supervisor P1 fix.
- No commit was created.
