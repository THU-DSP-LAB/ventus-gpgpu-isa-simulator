#ifndef RISCV_VENTUS_SHUFFLE_H
#define RISCV_VENTUS_SHUFFLE_H

#include "processor.h"
#include <array>

enum class VentusShuffleOp { Idx, Up, Down, Bfly };

constexpr reg_t VENTUS_SHUFFLE_LANES = 32;
constexpr float VENTUS_SHUFFLE_LMUL = 1.0f;

inline void require_ventus_shuffle_state(processor_t *p, insn_t insn)
{
  if (!p->extension_enabled('V') || p->VU.vsew != e32 ||
      p->VU.vflmul != VENTUS_SHUFFLE_LMUL)
    throw trap_illegal_instruction(insn.bits());

  if (p->VU.vl->read() > VENTUS_SHUFFLE_LANES)
    throw trap_illegal_instruction(insn.bits());
}

inline bool ventus_shuffle_lane_active(processor_t *p, reg_t lane)
{
  return ((p->gpgpu_unit.simt_stack.get_mask() >> lane) & 0x1) != 0;
}

inline void ventus_exec_shuffle(processor_t *p, insn_t insn,
                                VentusShuffleOp op)
{
  require_ventus_shuffle_state(p, insn);

  const reg_t vl = p->VU.vl->read();
  const reg_t vd_num = insn.rd();
  const reg_t vs2_num = insn.rs2();
  const reg_t imm5 = insn.v_zimm5() & 0x1f;

  std::array<uint32_t, VENTUS_SHUFFLE_LANES> src{};
  for (reg_t lane = 0; lane < src.size(); ++lane)
    src[lane] = p->VU.elt<uint32_t>(2, vs2_num, lane);

  for (reg_t lane = p->VU.vstart->read(); lane < vl; ++lane) {
    if (!ventus_shuffle_lane_active(p, lane))
      continue;

    int src_lane = static_cast<int>(lane);
    switch (op) {
    case VentusShuffleOp::Idx:
      src_lane = static_cast<int>(imm5);
      break;
    case VentusShuffleOp::Up:
      src_lane = static_cast<int>(lane) - static_cast<int>(imm5);
      break;
    case VentusShuffleOp::Down:
      src_lane = static_cast<int>(lane) + static_cast<int>(imm5);
      break;
    case VentusShuffleOp::Bfly:
      src_lane = static_cast<int>(lane ^ imm5);
      break;
    }

    const bool valid_src =
      src_lane >= 0 && src_lane < static_cast<int>(vl) &&
      ventus_shuffle_lane_active(p, static_cast<reg_t>(src_lane));
    const uint32_t result = valid_src ? src[src_lane] : src[lane];
    p->VU.elt<uint32_t>(0, vd_num, lane, true) = result;
  }

  p->VU.vstart->write(0);
}

#endif
