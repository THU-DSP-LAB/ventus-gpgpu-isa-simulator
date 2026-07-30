// vfmv_s_f: vd[0] = rs1 (vs2=0)
require_vector(true);
require_fp;
require((P.VU.vsew == e16 && p->extension_enabled(EXT_ZFH)) ||
        (P.VU.vsew == e32 && VECTOR_FP32_ENABLED) ||
        (P.VU.vsew == e64 && p->extension_enabled('D')));
require(STATE.frm->read() < 0x5);

reg_t vl = P.VU.vl->read();

if (vl > 0 && P.VU.vstart->read() < vl) {
  reg_t rd_num = insn.rd();

  switch(P.VU.vsew) {
    case e16:
      P.VU.elt<uint16_t>(0,rd_num, 0, true) =
          VFP_SCALAR(16, insn.rs1() | p->ext_rs1()).v;
      break;
    case e32:
      P.VU.elt<uint32_t>(0,rd_num, 0, true) =
          VFP_SCALAR(32, insn.rs1() | p->ext_rs1()).v;
      break;
    case e64:
      P.VU.elt<uint64_t>(0,rd_num, 0, true) =
          VFP_SCALAR(64, insn.rs1() | p->ext_rs1()).v;
      break;
  }
}
P.VU.vstart->write(0);
