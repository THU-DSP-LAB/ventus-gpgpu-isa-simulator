VI_GPU_ST_INDEX(e32,true,({
    reg_t baseAddr = index[i] + insn.v_s_simm11();
    // reg_t baseBias = P.get_csr(CSR_PDS) + (baseAddr & ~3) * P.get_csr(CSR_NUMW) * P.get_csr(CSR_NUMT) + (baseAddr & 3);
    // MMU.store_uint32(baseBias+((baseTid + vreg_inx)<<2),P.VU.elt<uint32_t>(2,vs2, vreg_inx));}

    reg_t baseBias = P.get_csr(CSR_PDS) + (P.get_csr(CSR_NUMW) * P.get_csr(CSR_NUMT) * (baseAddr >> 2));
    std::cout << "insn:"<<insn.bits()<<";index="<<index[i]<<";imm="<<insn.v_s_simm11()<<";baseaddr="<<baseAddr<<";basebias="<<baseBias<<";vreg_inx="<<vreg_inx<<std::endl;
    MMU.load_int32(baseBias + vreg_inx, P.VU.elt<uint32_t>(2,vs2, vreg_inx));}

    ));


  