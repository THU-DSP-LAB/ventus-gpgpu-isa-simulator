# Stage 2 Worker Review Round 3

- Reviewer: Dirac
- Reviewed diff base: `d68c338f0d1d778e6ef48c5b8ef442b942d37a9c`
- Status: Fail

## Findings

- P1: `ventus_exec_mma` assumes 32 e32 lanes fit in each LMUL=1 vector register, but did not explicitly reject configurations where `VLEN/e32 < 32`. In those states `VU.elt(..., lane 0..31)` crosses into following physical vector registers.

## Response

- Added a testable MMA execution policy helper:
  - `vl == 32`
  - `vstart == 0`
  - all lower 32 SIMT mask bits active
  - at least 32 e32 lanes per LMUL=1 vector register
- `ventus_exec_mma` now calls this policy before reading A/B/C/D fragments.
- MMA writeback now writes all 32 lanes for each participating D carrier register after a full-warp compute; no partial `vstart..vl` or active-mask writeback remains in the MMA executor.
- Added Stage 2 semantic tests for rejecting non-full-warp states.
- Added Stage 2 static checks for the policy helper and real executor boundary.
- Re-ran Stage 1/Stage 2 static and helper tests, targeted object build, and forbidden dependency search; all passed.

## Notes

- This documents and enforces the explicit Stage 2 policy requested by the supervisor: MMA is full-warp only. Non-full-warp states fail loudly with illegal instruction in the real executor or `std::invalid_argument` in the helper boundary.
