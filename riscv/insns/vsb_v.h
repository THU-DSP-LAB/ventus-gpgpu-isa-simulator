VI_GPU_ST_INDEX(e32,true,({
    reg_t baseAddr = index[i] + insn.v_s_simm11();
    reg_t baseBias = P.get_csr(CSR_PDS) + (baseAddr & ~3) * P.get_csr(CSR_NUMW) * P.get_csr(CSR_NUMT) + (baseAddr & 3);
    MMU.store_uint8(baseBias+((baseTid + vreg_inx)<<2),P.VU.elt<uint32_t>(2,vs2, vreg_inx));}
    ));


  