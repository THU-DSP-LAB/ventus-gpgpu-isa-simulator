VI_GPU_LD_INDEX(e32,true,({
    MMU.load_uint8(baseBias+((baseTid + vreg_inx)<<2) + (baseAddr & 3));}
    ));