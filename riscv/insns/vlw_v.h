VI_GPU_LD_INDEX(e32,true,({
    reg_t baseAddr = index[i] + insn.v_s_simm11();
    // reg_t baseBias = P.get_csr(CSR_PDS) + (baseAddr & ~3) * P.get_csr(CSR_NUMW) * P.get_csr(CSR_NUMT) + (baseAddr & 3);
    // MMU.load_int32(baseBias+((baseTid + vreg_inx)<<2));}

    reg_t baseBias = P.get_csr(CSR_PDS) + (P.get_csr(CSR_NUMW) * P.get_csr(CSR_NUMT) * ((baseAddr >> 2)<<2));
    MMU.load_int32(baseBias+((baseTid + vreg_inx)<<2));}
));

