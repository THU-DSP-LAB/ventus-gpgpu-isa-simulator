// vsuxe32.v
if (std::getenv("VENTUS_DEBUG_VSUXEI32")) {
  fprintf(stderr,
          "vsuxei32 pc=0x%08" PRIx64 " mask=0x%08" PRIx64 " stack=%d",
          (uint64_t)pc,
          (uint64_t)P.gpgpu_unit.simt_stack.get_mask(),
          P.gpgpu_unit.simt_stack.size());
  if (!P.gpgpu_unit.simt_stack.stack_empty()) {
    const auto &entry = P.gpgpu_unit.simt_stack.top();
    fprintf(stderr, " top.r_pc=0x%08" PRIx64 " top.else_pc=0x%08" PRIx64
                    " top.else_mask=0x%08" PRIx64,
            (uint64_t)entry.r_pc, (uint64_t)entry.else_pc,
            (uint64_t)entry.else_mask);
  }
  fprintf(stderr, "\n");
}
VI_ST_INDEX(e32, true);
if (std::getenv("VENTUS_DEBUG_VSUXEI32")) {
  const reg_t debug_lanes = P.VU.vl->read() < 8 ? P.VU.vl->read() : 8;
  const reg_t debug_base = RS1;
  const reg_t debug_vs3 = insn.rd();
  VI_DUPLICATE_VREG(2, insn.rs2(), e32);
  for (reg_t debug_i = 0; debug_i < debug_lanes; ++debug_i) {
    const bool active =
      ((P.gpgpu_unit.simt_stack.get_mask() >> debug_i) & 0x1) != 0;
    fprintf(stderr,
            "  lane=%" PRIu64 " active=%d addr=0x%08" PRIx64
            " value=0x%08" PRIx32 "\n",
            (uint64_t)debug_i, active ? 1 : 0,
            (uint64_t)(debug_base + index[debug_i]),
            (uint32_t)P.VU.elt<uint32_t>(3, debug_vs3, debug_i));
  }
}
