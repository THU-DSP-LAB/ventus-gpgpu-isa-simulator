#include "ventus_custom.h"

#include "trap.h"
#include <array>
#include <cstdint>

namespace {

constexpr reg_t VENTUS_CUSTOM_LANES = 32;
constexpr float VENTUS_CUSTOM_LMUL = 1.0f;

bool lane_active(processor_t *p, reg_t lane)
{
  return ((p->gpgpu_unit.simt_stack.get_mask() >> lane) & 0x1) != 0;
}

void require_ventus_custom_state(processor_t *p, insn_t insn)
{
  if (!p->extension_enabled('V') || p->VU.vsew != e32 ||
      p->VU.vflmul != VENTUS_CUSTOM_LMUL)
    throw trap_illegal_instruction(insn.bits());

  if (p->VU.vl->read() > VENTUS_CUSTOM_LANES)
    throw trap_illegal_instruction(insn.bits());
}

} // namespace

void ventus_exec_packed(processor_t *p, insn_t insn, VentusPackedOp op,
                        VentusPackedType type)
{
  require_ventus_custom_state(p, insn);

  const reg_t vl = p->VU.vl->read();
  const reg_t vd_num = insn.rd();
  const reg_t vs1_num = insn.rs1();
  const reg_t vs2_num = insn.rs2();

  std::array<uint32_t, VENTUS_CUSTOM_LANES> old_vd{};
  for (reg_t lane = 0; lane < old_vd.size(); ++lane)
    old_vd[lane] = p->VU.elt<uint32_t>(0, vd_num, lane);

  for (reg_t lane = p->VU.vstart->read(); lane < vl; ++lane) {
    if (!lane_active(p, lane))
      continue;

    const uint32_t lhs = p->VU.elt<uint32_t>(2, vs2_num, lane);
    const uint32_t rhs = p->VU.elt<uint32_t>(1, vs1_num, lane);
    p->VU.elt<uint32_t>(0, vd_num, lane, true) =
        ventus_custom_arith::do_packed(lhs, rhs, old_vd[lane], op, type);
  }

  p->VU.vstart->write(0);
}

void ventus_exec_vcvt(processor_t *p, insn_t insn, VentusVCvtOp op)
{
  require_ventus_custom_state(p, insn);

  const reg_t vl = p->VU.vl->read();
  const reg_t vd_num = insn.rd();
  const reg_t vs2_num = insn.rs2();

  for (reg_t lane = p->VU.vstart->read(); lane < vl; ++lane) {
    if (!lane_active(p, lane))
      continue;

    const uint32_t input = p->VU.elt<uint32_t>(2, vs2_num, lane);
    p->VU.elt<uint32_t>(0, vd_num, lane, true) =
        ventus_custom_arith::do_vcvt(input, op);
  }

  p->VU.vstart->write(0);
}

void ventus_exec_sfu(processor_t *p, insn_t insn, VentusSFUOp op,
                     VentusSFUMode mode)
{
  require_ventus_custom_state(p, insn);

  const reg_t vl = p->VU.vl->read();
  const reg_t vd_num = insn.rd();
  const reg_t vs2_num = insn.rs2();

  for (reg_t lane = p->VU.vstart->read(); lane < vl; ++lane) {
    if (!lane_active(p, lane))
      continue;

    const uint32_t input = p->VU.elt<uint32_t>(2, vs2_num, lane);
    const uint32_t output = mode == VentusSFUMode::FP32
                                ? ventus_custom_arith::bit_cast_u32(
                                      ventus_custom_arith::apply_sfu(
                                          ventus_custom_arith::bit_cast_f32(input),
                                          op))
                                : ventus_custom_arith::do_packed_sfu(input, op, mode);
    p->VU.elt<uint32_t>(0, vd_num, lane, true) = output;
  }

  p->VU.vstart->write(0);
}
