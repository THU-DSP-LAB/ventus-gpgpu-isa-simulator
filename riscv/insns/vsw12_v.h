VI_GPU_ST12_INDEX(e32,true,{
  const reg_t addr = baseAddr + index[i] + fn * 4;
  const uint32_t value = P.VU.elt<uint32_t>(2,vs2, vreg_inx);
  const char *debug_addr = std::getenv("VENTUS_SPIKE_DEBUG_VSW12_ADDR");
  if (debug_addr && addr >= std::strtoull(debug_addr, nullptr, 0) &&
      addr < std::strtoull(debug_addr, nullptr, 0) + 16)
    std::fprintf(stderr, "ventus: vsw12 pc=0x%lx lane=%lu addr=0x%lx value=0x%x\\n",
                 (unsigned long)STATE.pc, (unsigned long)i,
                 (unsigned long)addr, value);
  MMU.store_uint32(addr, value);
});
