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
  if (!p->get_state()->sstatus->enabled(SSTATUS_VS) ||
      !p->extension_enabled('V') || p->VU.vill || p->VU.vsew != e32 ||
      p->VU.vflmul != VENTUS_CUSTOM_LMUL)
    throw trap_illegal_instruction(insn.bits());

  if (p->VU.vl->read() > VENTUS_CUSTOM_LANES)
    throw trap_illegal_instruction(insn.bits());
}

void require_ventus_mma_state(processor_t *p, insn_t insn)
{
  require_ventus_custom_state(p, insn);

  const ventus_mma::ExecutionState state{
      uint32_t(p->VU.vl->read()),
      uint32_t(p->VU.vstart->read()),
      p->gpgpu_unit.simt_stack.get_mask(),
      uint32_t((p->VU.VLEN / 8) / sizeof(uint32_t)),
  };

  try {
    ventus_mma::check_full_warp_state(state);
  } catch (const std::exception&) {
    throw trap_illegal_instruction(insn.bits());
  }
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

void ventus_exec_mma(processor_t *p, insn_t insn)
{
  require_ventus_mma_state(p, insn);

  const auto shape = VentusMMAShape((insn.bits() >> 25) & 0x7u);
  const auto ab_type = VentusMMAInputType((insn.bits() >> 28) & 0xfu);
  const auto cd_type = VentusMMAOutputType((insn.bits() >> 12) & 0x1u);
  const ventus_mma::Options options{
      shape,
      ab_type,
      cd_type,
      static_cast<bool>((insn.bits() >> 14) & 0x1u),
      static_cast<bool>((insn.bits() >> 13) & 0x1u),
  };

  const uint32_t rd_base = uint32_t(insn.rd() | p->ext_rd());
  const uint32_t rs1_base = uint32_t(insn.rs1() | p->ext_rs1());
  const uint32_t rs2_base = uint32_t(insn.rs2() | p->ext_rs2());
  ventus_mma::ShapeInfo shape_info{};

  try {
    ventus_mma::check_register_bounds(rd_base, rs1_base, rs2_base, options);
    shape_info = ventus_mma::get_shape_info(options.shape);
  } catch (const std::exception&) {
    throw trap_illegal_instruction(insn.bits());
  }

  ventus_mma::RegisterFile regs{};
  const int carrier_regs =
      ventus_mma::cd_carrier_regs(shape_info, options.cd_type);
  for (int reg = 0; reg < carrier_regs; ++reg)
    for (reg_t lane = 0; lane < VENTUS_CUSTOM_LANES; ++lane)
      regs[rd_base + uint32_t(reg)][lane] =
          p->VU.elt<uint32_t>(-1, rd_base + reg, lane);
  for (int reg = 0; reg < shape_info.a_regs; ++reg)
    for (reg_t lane = 0; lane < VENTUS_CUSTOM_LANES; ++lane)
      regs[rs1_base + uint32_t(reg)][lane] =
          p->VU.elt<uint32_t>(-1, rs1_base + reg, lane);
  for (int reg = 0; reg < shape_info.b_regs; ++reg)
    for (reg_t lane = 0; lane < VENTUS_CUSTOM_LANES; ++lane)
      regs[rs2_base + uint32_t(reg)][lane] =
          p->VU.elt<uint32_t>(-1, rs2_base + reg, lane);

  try {
    ventus_mma::execute_mma_register_file(regs, rd_base, rs1_base, rs2_base,
                                          options);
  } catch (const std::exception&) {
    throw trap_illegal_instruction(insn.bits());
  }

  for (int reg = 0; reg < carrier_regs; ++reg) {
    for (reg_t lane = 0; lane < VENTUS_CUSTOM_LANES; ++lane)
      p->VU.elt<uint32_t>(-1, rd_base + reg, lane, true) =
          regs[rd_base + uint32_t(reg)][lane];
  }

  p->VU.vstart->write(0);
}
