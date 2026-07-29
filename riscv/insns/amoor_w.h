require_extension('A');
if (!ventus_exec_amo_w_if_warp(p, insn, VentusAmoWOp::Or))
  WRITE_RD(sext32(MMU.amo_uint32(RS1,
                                 [&](uint32_t lhs) { return lhs | RS2; })));
