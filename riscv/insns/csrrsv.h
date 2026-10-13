require_extension('V');

reg_t vd = insn.rd();
uint32_t csr_id = insn.csr();
auto csr_base = p->get_state()->csrmap.find(csr_id);
if (csr_base == p->get_state()->csrmap.end()) {
  throw trap_illegal_instruction(insn.bits());
}

vec_csr_t* vcsr = dynamic_cast<vec_csr_t*>(csr_base->second.get());
reg_t vl = P.VU.vl->read();
if (vcsr) {
  for(uint32_t i = 0; i < vl; i++) {
    P.VU.elt<uint32_t>(0,vd,i,true) = vcsr->get_lane(i);
  }
} else {
  /* Some target intrinsics are per-warp rather than per-lane (for example
   * mhartid used as the RT submission worker ID).  csrr.v represents those
   * as a uniform VGPR value, while the GPGPU vector CSRs above retain their
   * lane-specific values. */
  const reg_t value = csr_base->second->read();
  for(uint32_t i = 0; i < vl; i++) {
    P.VU.elt<uint32_t>(0,vd,i,true) = value;
  }
}
