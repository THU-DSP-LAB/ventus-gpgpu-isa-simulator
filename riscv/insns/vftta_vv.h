// vmpopc rd, vs2, vm
require(P.VU.vsew >= e8 && P.VU.vsew <= e64);
require_vector(true);
reg_t vl = P.VU.vl->read();
reg_t sew = P.VU.vsew;
reg_t rd_num = insn.rd();
reg_t rs1_num = insn.rs1();
reg_t rs2_num = insn.rs2();
require_align(rd_num, P.VU.vflmul);
require_vm;

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
    if(i>=vl/2)
    P.VU.elt<float32_t>(0,rd_num, i, true) = f32(i+7);
    else{
	P.VU.elt<float32_t>(0,rd_num, i, true)=P.VU.elt<float32_t>(0,rd_num, i, true);
	//float32_t vd=P.VU.elt<float32_t>(0,rd_num,i,true);
	//float32_t vs1=P.VU.elt<float32_t>(1,rs1_num, i);
	//float32_t vs2=P.VU.elt<float32_t>(2,rs2_num, i);
	for(reg_t j=0;j<(vl/4);++j){
		//vd=f32_mulAdd(vs1,vs2,vd);
		P.VU.elt<float32_t>(0,rd_num, i, true)=f32_mulAdd(P.VU.elt<float32_t>(1,rs1_num, ((i>>2)<<3)+j),P.VU.elt<float32_t>(2,rs2_num, ((i&3)<<3)+j, true),P.VU.elt<float32_t>(0,rd_num, i, true));
	}
    //P.VU.elt<float32_t>(0,rd_num,i,true)=vd;
    }
    break;
  default:
    P.VU.elt<uint64_t>(0,rd_num, i, true) = i;
    break;
  }
}

P.VU.vstart->write(0);
