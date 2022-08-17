P.gpgpu_unit.simt_stack.pop_join(BRANCH_TARGET);
SET_PC(P.gpgpu_unit.simt_stack.get_npc());
uint64_t &cur_mask = P.VU.elt<uint64_t>(0, 0, true);
SET_MASK(P.gpgpu_unit.simt_stack.get_mask());