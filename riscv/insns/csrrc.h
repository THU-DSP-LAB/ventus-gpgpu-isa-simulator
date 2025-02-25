bool write = insn.rs1() != 0;
int csr = validate_csr(insn.csr(), write);
if (csr < CSR_GL_ID_X || csr > CSR_LC_ID_Z) {
reg_t old = p->get_csr(csr, insn, write);
if (write) {
  p->put_csr(csr, old & ~RS1);
}
WRITE_RD(sext_xlen(old));
} else {
  std::vector<reg_t> read_gpuvec_csr = p->get_gpuvec_csr(csr, insn, write);
  std::vector<reg_t> old_vector = read_gpuvec_csr;
  if (write) {
    // for (size_t i = 0; i < read_gpuvec_csr.size(); ++i) {
    //   read_gpuvec_csr[i] &= ~RS1;
    // }
    // p->put_gpuvec_csr(csr, read_gpuvec_csr);
    assert(false && "Write operation is not allowed for gpuvec_csr");
  }
  VI_VV_LOOP({
    vd = sext_xlen(old_vector[i]);
  })
}
serialize();
