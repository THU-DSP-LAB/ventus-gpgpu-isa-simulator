# Stage 2 Worker Review Round 1

- Reviewer: Euler
- Reviewed diff base: `d68c338f0d1d778e6ef48c5b8ef442b942d37a9c`
- Status: Fail

## Findings

- P1: TF32 input decode used full FP32 mantissa rather than TF32-style mantissa truncation.
- P1: Arithmetic uses host `float` and is not bit-exact with the external reference model for special values and detailed rounding.
- P1: Core implementation files were untracked, so a tracked-only diff would be incomplete.
- P2: Helper validation did not reject an invalid `VentusMMAOutputType` enum before treating it like FP32.
- P2: Tests did not cover TF32 truncation-sensitive values, additional invalid combos, or N16 split packing.

## Response

- Fixed TF32 decode by masking input bits with `0xffffe000` before converting to host float.
- Added validation for invalid output type enums.
- Added tests for:
  - TF32 truncation-sensitive input.
  - BF16+FP16 invalid combo.
  - TF32+FP16 invalid combo.
  - invalid helper `ab_type`.
  - invalid helper `cd_type`.
  - `m16n16k16` FP16->FP32 with two N blocks.
- Re-ran Stage 1/Stage 2 static and helper tests and the targeted object build; all passed.

## Notes

- The untracked-file finding is expected while following the no-commit workflow. The supervisor should review the whole working tree, not only tracked `git diff`.
- Full external-model bit-exact FDA arithmetic was not implemented because the Stage 2 requirement forbids the external model dependencies and explicitly allows host-float approximation for TF32. The implementation targets local arithmetic plus reference-compatible shape/register packing and explicit invalid-combo failure.
