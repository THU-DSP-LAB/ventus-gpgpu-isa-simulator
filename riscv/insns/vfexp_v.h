// vfexp.v rd, vs2, vm
require(P.VU.vsew >= e8 && P.VU.vsew <= e64);
require_vector(true);
reg_t vl = P.VU.vl->read();
reg_t sew = P.VU.vsew;
reg_t rd_num = insn.rd();
reg_t rs2_num = insn.rs2();
require_align(rd_num, P.VU.vflmul);
require_vm;
float32_t vfexp_src_f;float* vfexp_src_fp;
float vfexp_dst_f;uint32_t vfexp_dst_i;

for (reg_t i = P.VU.vstart->read() ; i < P.VU.vl->read(); ++i) {
  VI_LOOP_ELEMENT_SKIP();
  switch (sew) {
  case e8:
    P.VU.elt<uint8_t>(0,rd_num, i, true) = i;
    break;
  case e16:
    P.VU.elt<uint16_t>(0,rd_num, i, true) = i;
    break;
  case e32:
    vfexp_src_f=P.VU.elt<float32_t>(2,rs2_num, i);
    vfexp_src_fp=(float*)(&(vfexp_src_f.v));
    vfexp_dst_f=exp(*vfexp_src_fp);
    vfexp_dst_i=*((uint32_t*)(&vfexp_dst_f));
		P.VU.elt<float32_t>(0,rd_num, i, true)= f32(vfexp_dst_i);
    break;
  default:
    P.VU.elt<uint64_t>(0,rd_num, i, true) = i;
    break;
  }
}

P.VU.vstart->write(0);
