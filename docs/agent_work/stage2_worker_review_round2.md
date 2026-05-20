# Stage 2 Worker Review Round 2

- Reviewer: Jason
- Reviewed diff base: `d68c338f0d1d778e6ef48c5b8ef442b942d37a9c`
- Status: Fail

## Findings

- P1: `ventus_exec_mma` did not fully validate RVV vector state. The shared custom state check verified extension, `vsew`, `vflmul`, and `vl`, but missed `sstatus.VS` enabled and `VU.vill` rejection.
- P2: Helper tests do not construct a full `processor_t`, so they do not directly exercise `ventus_exec_mma` state, mask, `vstart`, and writeback behavior.

## Response

- Fixed shared `require_ventus_custom_state()` to reject disabled `sstatus.VS` and `VU.vill`. This applies to Stage 2 MMA and Stage 1 custom arithmetic helpers using the same policy.
- Added static coverage requiring `get_state()->sstatus->enabled(SSTATUS_VS)` and `p->VU.vill` checks.
- Re-ran Stage 1/Stage 2 static and helper tests and targeted object build; all passed.

## Notes

- Full `processor_t` executor tests remain a residual integration-test gap. The current targeted object build verifies the real executor compiles; helper tests cover packing/arithmetic semantics without requiring a full Spike harness.
