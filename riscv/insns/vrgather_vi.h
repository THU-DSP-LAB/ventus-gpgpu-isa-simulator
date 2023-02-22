// vrgather.vi vd, vs2, zimm5 vm # vd[i] = (zimm5 >= VLMAX) ? 0 : vs2[zimm5];
require_align(insn.rd(), P.VU.vflmul);
require_align(insn.rs2(), P.VU.vflmul);
require(insn.rd() != insn.rs2());
require_vm;

reg_t zimm5 = ( insn.v_zimm5() | p->ext_imm() );

VI_LOOP_BASE

for (reg_t i = P.VU.vstart->read(); i < vl; ++i) {
  VI_LOOP_ELEMENT_SKIP();

  switch (sew) {
  case e8:
    P.VU.elt<uint8_t>(0,rd_num, i, true) = zimm5 >= P.VU.vlmax ? 0 : P.VU.elt<uint8_t>(2,rs2_num, zimm5);
    break;
  case e16:
    P.VU.elt<uint16_t>(0,rd_num, i, true) = zimm5 >= P.VU.vlmax ? 0 : P.VU.elt<uint16_t>(2,rs2_num, zimm5);
    break;
  case e32:
    P.VU.elt<uint32_t>(0,rd_num, i, true) = zimm5 >= P.VU.vlmax ? 0 : P.VU.elt<uint32_t>(2,rs2_num, zimm5);
    break;
  default:
    P.VU.elt<uint64_t>(0,rd_num, i, true) = zimm5 >= P.VU.vlmax ? 0 : P.VU.elt<uint64_t>(2,rs2_num, zimm5);
    break;
  }
}

VI_LOOP_END;
