int csr = validate_csr(insn.csr(), true);
if (csr < CSR_GL_ID_X || csr > CSR_LC_ID_Z) {
reg_t old = p->get_csr(csr, insn, true);
p->put_csr(csr, RS1);
WRITE_RD(sext_xlen(old));
} else {
  assert(false && "Write operation is not allowed for gpuvec_csr");
}
serialize();
