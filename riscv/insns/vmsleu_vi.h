// vmsleu.vi vd, vs2, simm5
VI_VI_ULOOP_CMP
({
  res = vs2 <= (( (insn.v_simm5() & 31) | p->ext_imm() ) & (UINT64_MAX >> (64 - P.VU.vsew)));
})
