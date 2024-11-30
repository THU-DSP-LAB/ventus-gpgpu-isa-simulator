require_extension('A');
// WRITE_RD(sext32(MMU.amo_uint32(RS1, [&](uint32_t lhs) { return lhs + RS2; })));

// TODO: check: 64位时，amoadd.w缺少sext操作，原版代码中也没有
VI_GPU_AMO({ return lhs + vs2; }, uint, e32);
