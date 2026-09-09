#include "ventus_custom.h"
#include "ventus_rtcore_model.h"
#include "ventus_rt_replay_capture.h"

#include "trap.h"
#include <array>
#include <cstdint>

namespace {

constexpr reg_t VENTUS_CUSTOM_LANES = 32;
constexpr float VENTUS_CUSTOM_LMUL = 1.0f;

bool lane_active(processor_t *p, reg_t lane)
{
  return p->gpgpu_unit.simt_stack.lane_active(lane);
}

reg_t resident_hardware_warp_id(processor_t *p)
{
  // CSR_TID is workgroup-local and repeats for each one-warp RT workgroup.
  // The Spike hart ID is the unique resident warp slot within this SM model.
  return p->get_id();
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
  if (vl > ventus_rt::v5_warp_lanes)
    throw trap_illegal_instruction(insn.bits());
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

  ventus_mma::Options options{};
  try {
    options = ventus_mma::decode_mma_options(insn.bits());
  } catch (const std::exception&) {
    throw trap_illegal_instruction(insn.bits());
  }

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

bool ventus_exec_amo_w_if_warp(processor_t *p, insn_t insn,
                               VentusAmoWOp op)
{
  if (p->get_csr(CSR_NUMT) == 0)
    return false;

  require_ventus_custom_state(p, insn);
  const reg_t vl = p->VU.vl->read();
  const reg_t vd_num = insn.rd();
  const reg_t vs1_num = insn.rs1();
  const reg_t vs2_num = insn.rs2();

  for (reg_t lane = p->VU.vstart->read(); lane < vl; ++lane) {
    if (!lane_active(p, lane))
      continue;

    const reg_t address = p->VU.elt<uint32_t>(1, vs1_num, lane);
    const uint32_t rhs = p->VU.elt<uint32_t>(2, vs2_num, lane);
    const uint32_t previous =
        p->get_mmu()->amo_uint32(address, [rhs, op](uint32_t lhs) {
          if (op == VentusAmoWOp::Add)
            return lhs + rhs;
          if (op == VentusAmoWOp::And)
            return lhs & rhs;
          return lhs | rhs;
        });
    p->VU.elt<uint32_t>(0, vd_num, lane, true) = previous;
  }

  p->VU.vstart->write(0);
  return true;
}

void ventus_exec_rt_traverse(processor_t *p, insn_t insn)
{
  require_ventus_custom_state(p, insn);

  const reg_t vl = p->VU.vl->read();
  const reg_t vd_num = insn.rd();
  const reg_t vs2_num = insn.rs2();
  ventus_rt::RtCoreModel *model = p->get_sim()->get_rtcore_model();
  if (model == nullptr)
    throw trap_illegal_instruction(insn.bits());

  for (reg_t lane = p->VU.vstart->read(); lane < vl; ++lane) {
    if (!lane_active(p, lane))
      continue;

    auto mem = ventus_rt::make_hybrid_memory(
        *p->get_mmu(), p->get_csr(CSR_PDS), p->get_csr(CSR_NUMW),
        p->get_csr(CSR_NUMT), p->get_csr(CSR_TID), lane);
    const reg_t slot = p->VU.elt<uint32_t>(2, vs2_num, lane);
    p->VU.elt<uint32_t>(0, vd_num, lane, true) =
        model->executeTraverse(mem, slot);
  }

  p->VU.vstart->write(0);
}

void ventus_exec_rt_traverse_v5(processor_t *p, insn_t insn)
{
  require_ventus_custom_state(p, insn);

  const reg_t vl = p->VU.vl->read();
  const reg_t vd_num = insn.rd();
  const reg_t pds_num = insn.rs2();
  const reg_t private_num = insn.rs1();
  ventus_rt::RtCoreModel *model = p->get_sim()->get_rtcore_model();
  if (model == nullptr)
    throw trap_illegal_instruction(insn.bits());

  uint32_t issue_mask = 0;
  std::array<reg_t, ventus_rt::v5_warp_lanes> pds_addresses{};
  std::array<reg_t, ventus_rt::v5_warp_lanes> private_state_addresses{};
  for (reg_t lane = p->VU.vstart->read(); lane < vl; ++lane) {
    if (!lane_active(p, lane))
      continue;

    issue_mask |= uint32_t{1} << lane;
    pds_addresses[lane] = p->VU.elt<uint32_t>(2, pds_num, lane);
    private_state_addresses[lane] =
        p->VU.elt<uint32_t>(1, private_num, lane);
  }

  if (issue_mask != 0) {
    auto &capture = ventus_rt::RtReplayCapture::instance();
    const uint64_t capture_command = capture.beginTraverse(
        resident_hardware_warp_id(p), issue_mask, pds_addresses,
        private_state_addresses);
    const auto results = model->executeTraverseV5(
        resident_hardware_warp_id(p), issue_mask, pds_addresses,
        private_state_addresses,
        [p, &capture, capture_command](reg_t lane, reg_t pds_address) {
          return ventus_rt::make_capturing_canonical_pds_memory(
              *p->get_mmu(), pds_address, capture, capture_command,
              uint32_t(lane));
        });
    capture.endTraverse(capture_command, issue_mask, results);
    for (reg_t lane = p->VU.vstart->read(); lane < vl; ++lane) {
      if ((issue_mask & (uint32_t{1} << lane)) != 0)
        p->VU.elt<uint32_t>(0, vd_num, lane, true) = results[lane];
    }
  }

  p->VU.vstart->write(0);
}

void ventus_exec_rt_release(processor_t *p, insn_t insn)
{
  require_ventus_custom_state(p, insn);

  const reg_t vl = p->VU.vl->read();
  if (vl > ventus_rt::v5_warp_lanes)
    throw trap_illegal_instruction(insn.bits());
  const reg_t vs2_num = insn.rs2();
  ventus_rt::RtCoreModel *model = p->get_sim()->get_rtcore_model();
  if (model == nullptr)
    throw trap_illegal_instruction(insn.bits());

  uint32_t active_mask = 0;
  bool operand_exceeds_legacy_slot = false;
  std::array<reg_t, ventus_rt::v5_warp_lanes> operands{};
  for (reg_t lane = p->VU.vstart->read(); lane < vl; ++lane) {
    if (!lane_active(p, lane))
      continue;

    const reg_t slot = p->VU.elt<uint32_t>(2, vs2_num, lane);
    active_mask |= uint32_t{1} << lane;
    operands[lane] = slot;
    operand_exceeds_legacy_slot |=
        slot >= ventus_rt::rt_pds_total_size_bytes;
  }

  const reg_t hardware_warp_id = resident_hardware_warp_id(p);
  const auto *resident = model->residentWarpRecord(hardware_warp_id);
  const bool v5_release =
      resident != nullptr &&
      resident->state != ventus_rt::ResidentWarpState::Unbound;
  if (v5_release || operand_exceeds_legacy_slot) {
    model->executeReleaseV5(
        hardware_warp_id, active_mask, operands,
        [p](reg_t, reg_t pds_address) {
          return ventus_rt::make_canonical_pds_memory(*p->get_mmu(),
                                                       pds_address);
        });
  } else if (active_mask != 0) {
    for (reg_t lane = p->VU.vstart->read(); lane < vl; ++lane) {
      if ((active_mask & (uint32_t{1} << lane)) == 0)
        continue;
      auto mem = ventus_rt::make_hybrid_memory(
          *p->get_mmu(), p->get_csr(CSR_PDS), p->get_csr(CSR_NUMW),
          p->get_csr(CSR_NUMT), p->get_csr(CSR_TID), lane);
      model->executeRelease(mem, operands[lane]);
    }
  }

  p->VU.vstart->write(0);
}
