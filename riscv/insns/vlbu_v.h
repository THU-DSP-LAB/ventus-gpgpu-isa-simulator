VI_GPU_LD_INDEX(e32,true,({
    reg_t baseAddr = index[i] + insn.v_simm11();
    reg_t baseBias = P.get_csr(CSR_PDS) + (baseAddr & ~3) * P.get_csr(CSR_NUMW) * P.get_csr(CSR_NUMT) + (baseAddr & 3);
    MMU.load_uint8(baseBias+((baseTid + vreg_inx)<<2));}
    ));