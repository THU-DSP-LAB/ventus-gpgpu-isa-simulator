require_extension('A');
// WRITE_RD(sext32(MMU.amo_uint32(RS1, [&](int32_t lhs) { return std::max(lhs, int32_t(RS2)); })));

// signed/unsigned type is handled by VI_GPU_AMO
VI_GPU_AMO({ return lhs >= vs2 ? lhs : vs2; }, int, e32);
