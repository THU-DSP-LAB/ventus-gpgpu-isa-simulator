VI_GPU_ST_INDEX(e32,true,({
    MMU.store_uint32(baseBias+((baseTid + vreg_inx)<<2 + (baseAddr & 3)),P.VU.elt<uint32_t>(2,vs2, vreg_inx));
    }
    ));


  