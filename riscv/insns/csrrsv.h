require_extension('V');

reg_t vd = insn.rd();
uint32_t csr_id = insn.csr();
auto csr_base = p->get_state()->csrmap.find(csr_id);
if (csr_base == p->get_state()->csrmap.end()) {
  throw trap_illegal_instruction(insn.bits());
}

vec_csr_t* vcsr = dynamic_cast<vec_csr_t*>(csr_base->second.get());
if (!vcsr) {
  throw trap_illegal_instruction(insn.bits());
}

reg_t vl = P.VU.vl->read();
// reg_t vl = P.gpgpu_unit.w->local_size_x * P.gpgpu_unit.w->local_size_y * P.gpgpu_unit.w->local_size_z;
for(uint32_t i = 0; i < vl; i++) {
  P.VU.elt<uint64_t>(0,vd , i, true) = vcsr->get_lane(i);
}