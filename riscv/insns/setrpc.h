p->gpgpu_unit.rpc->write(RS1 + insn.i_imm());
WRITE_RD(sext_xlen(RS1 + insn.i_imm()));