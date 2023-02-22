// vrgather.vv vd, vs2, vs1, vm # vd[i] = (vs1[i] >= VLMAX) ? 0 : vs2[vs1[i]];
require_align(insn.rd(), P.VU.vflmul);
require_align(insn.rs2(), P.VU.vflmul);
require_align(insn.rs1(), P.VU.vflmul);
require(insn.rd() != insn.rs2() && insn.rd() != insn.rs1());
require_vm;

VI_LOOP_BASE
  switch (sew) {
  case e8: {
    auto vs1 = P.VU.elt<uint8_t>(1,rs1_num, i);
    //if (i > 255) continue;
    P.VU.elt<uint8_t>(0,rd_num, i, true) = vs1 >= P.VU.vlmax ? 0 : P.VU.elt<uint8_t>(2,rs2_num, vs1);
    break;
  }
  case e16: {
    auto vs1 = P.VU.elt<uint16_t>(1,rs1_num, i);
    P.VU.elt<uint16_t>(0,rd_num, i, true) = vs1 >= P.VU.vlmax ? 0 : P.VU.elt<uint16_t>(2,rs2_num, vs1);
    break;
  }
  case e32: {
    auto vs1 = P.VU.elt<uint32_t>(1,rs1_num, i);
    P.VU.elt<uint32_t>(0,rd_num, i, true) = vs1 >= P.VU.vlmax ? 0 : P.VU.elt<uint32_t>(2,rs2_num, vs1);
    break;
  }
  default: {
    auto vs1 = P.VU.elt<uint64_t>(1,rs1_num, i);
    P.VU.elt<uint64_t>(0,rd_num, i, true) = vs1 >= P.VU.vlmax ? 0 : P.VU.elt<uint64_t>(2,rs2_num, vs1);
    break;
  }
  }
VI_LOOP_END;
