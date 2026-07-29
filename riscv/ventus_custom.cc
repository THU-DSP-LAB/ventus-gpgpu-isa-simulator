#include "ventus_custom.h"

#include "trap.h"
#include <array>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

constexpr reg_t VENTUS_CUSTOM_LANES = 32;
constexpr float VENTUS_CUSTOM_LMUL = 1.0f;

bool lane_active(processor_t *p, reg_t lane)
{
  return ((p->gpgpu_unit.simt_stack.get_mask() >> lane) & 0x1) != 0;
}

bool debug_mma_enabled()
{
  const char *env = std::getenv("VENTUS_DEBUG_MMA");
  return env && env[0] && env[0] != '0';
}

const char *mma_shape_name(VentusMMAShape shape)
{
  switch (shape) {
  case VentusMMAShape::M8N8K16:
    return "m8n8k16";
  case VentusMMAShape::M16N8K16:
    return "m16n8k16";
  case VentusMMAShape::M8N16K16:
    return "m8n16k16";
  case VentusMMAShape::M16N16K16:
    return "m16n16k16";
  case VentusMMAShape::M8N8K8:
    return "m8n8k8";
  case VentusMMAShape::M16N8K8:
    return "m16n8k8";
  case VentusMMAShape::M8N16K8:
    return "m8n16k8";
  case VentusMMAShape::M16N16K8:
    return "m16n16k8";
  }
  return "unknown";
}

const char *mma_input_type_name(VentusMMAInputType type)
{
  switch (type) {
  case VentusMMAInputType::TF32:
    return "tf32";
  case VentusMMAInputType::FP16:
    return "fp16";
  case VentusMMAInputType::BF16:
    return "bf16";
  }
  return "unknown";
}

const char *mma_output_type_name(VentusMMAOutputType type)
{
  switch (type) {
  case VentusMMAOutputType::FP16:
    return "fp16";
  case VentusMMAOutputType::FP32:
    return "fp32";
  }
  return "unknown";
}

void print_mma_options(const ventus_mma::Options &options)
{
  std::fprintf(stderr,
               " shape=%s ab=%s cd=%s a_col=%u b_row=%u",
               mma_shape_name(options.shape),
               mma_input_type_name(options.ab_type),
               mma_output_type_name(options.cd_type),
               options.a_column_layout ? 1u : 0u,
               options.b_row_layout ? 1u : 0u);
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
  } catch (const std::exception &e) {
    if (debug_mma_enabled()) {
      std::fprintf(stderr,
                   "[VENTUS_DEBUG_MMA] state rejected bits=0x%08" PRIx64
                   " vl=%u vstart=%u mask=0x%08" PRIx64
                   " e32_lanes=%u reason=%s\n",
                   uint64_t(insn.bits()), state.vl, state.vstart,
                   state.active_mask, state.e32_lanes_per_vreg, e.what());
    }
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
  static bool printed_mma_entry = false;
  if (!printed_mma_entry) {
    printed_mma_entry = true;
    std::fprintf(stderr,
                 "[VENTUS_DEBUG_MMA] unconditional enter bits=0x%08" PRIx64
                 " rd=%lu rs1=%lu rs2=%lu\n",
                 uint64_t(insn.bits()), insn.rd(), insn.rs1(), insn.rs2());
  }

  if (debug_mma_enabled()) {
    std::fprintf(stderr,
                 "[VENTUS_DEBUG_MMA] enter bits=0x%08" PRIx64
                 " rd=%lu rs1=%lu rs2=%lu ext_rd=%u ext_rs1=%u ext_rs2=%u\n",
                 uint64_t(insn.bits()), insn.rd(), insn.rs1(), insn.rs2(),
                 p->ext_rd(), p->ext_rs1(), p->ext_rs2());
  }

  require_ventus_mma_state(p, insn);

  ventus_mma::Options options{};
  try {
    options = ventus_mma::decode_mma_options(insn.bits());
  } catch (const std::exception &e) {
    if (debug_mma_enabled()) {
      std::fprintf(stderr,
                   "[VENTUS_DEBUG_MMA] decode rejected bits=0x%08" PRIx64
                   " rd=%lu rs1=%lu rs2=%lu ext_rd=%u ext_rs1=%u ext_rs2=%u"
                   " reason=%s\n",
                   uint64_t(insn.bits()), insn.rd(), insn.rs1(), insn.rs2(),
                   p->ext_rd(), p->ext_rs1(), p->ext_rs2(), e.what());
    }
    throw trap_illegal_instruction(insn.bits());
  }

  const uint32_t rd_base = uint32_t(insn.rd() | p->ext_rd());
  const uint32_t rs1_base = uint32_t(insn.rs1() | p->ext_rs1());
  const uint32_t rs2_base = uint32_t(insn.rs2() | p->ext_rs2());
  ventus_mma::ShapeInfo shape_info{};

  try {
    ventus_mma::check_register_bounds(rd_base, rs1_base, rs2_base, options);
    shape_info = ventus_mma::get_shape_info(options.shape);
  } catch (const std::exception &e) {
    if (debug_mma_enabled()) {
      std::fprintf(stderr,
                   "[VENTUS_DEBUG_MMA] register/options rejected bits=0x%08"
                   PRIx64 " rd=%u rs1=%u rs2=%u",
                   uint64_t(insn.bits()), rd_base, rs1_base, rs2_base);
      print_mma_options(options);
      std::fprintf(stderr, " reason=%s\n", e.what());
    }
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
  } catch (const std::exception &e) {
    if (debug_mma_enabled()) {
      std::fprintf(stderr,
                   "[VENTUS_DEBUG_MMA] execute rejected bits=0x%08" PRIx64
                   " rd=%u rs1=%u rs2=%u",
                   uint64_t(insn.bits()), rd_base, rs1_base, rs2_base);
      print_mma_options(options);
      std::fprintf(stderr, " reason=%s\n", e.what());
    }
    throw trap_illegal_instruction(insn.bits());
  }

  for (int reg = 0; reg < carrier_regs; ++reg) {
    for (reg_t lane = 0; lane < VENTUS_CUSTOM_LANES; ++lane)
      p->VU.elt<uint32_t>(-1, rd_base + reg, lane, true) =
          regs[rd_base + uint32_t(reg)][lane];
  }

  p->VU.vstart->write(0);
}

void ventus_exec_rt_traverse(processor_t *p, insn_t insn)
{
  require_ventus_custom_state(p, insn);

  const reg_t vl = p->VU.vl->read();
  const reg_t vd_num = insn.rd();
  const reg_t vs2_num = insn.rs2();

  for (reg_t lane = p->VU.vstart->read(); lane < vl; ++lane) {
    if (!lane_active(p, lane))
      continue;

    auto mem = ventus_rt::make_hybrid_memory(
        *p->get_mmu(), p->get_csr(CSR_PDS), p->get_csr(CSR_NUMW),
        p->get_csr(CSR_NUMT), p->get_csr(CSR_TID), lane);
    const reg_t slot = p->VU.elt<uint32_t>(2, vs2_num, lane);
    const uint32_t status = ventus_rt::traverse(mem, slot);
    p->VU.elt<uint32_t>(0, vd_num, lane, true) = status;
  }

  p->VU.vstart->write(0);
}

void ventus_exec_rt_release(processor_t *p, insn_t insn)
{
  require_ventus_custom_state(p, insn);

  const reg_t vl = p->VU.vl->read();
  const reg_t vs2_num = insn.rs2();

  for (reg_t lane = p->VU.vstart->read(); lane < vl; ++lane) {
    if (!lane_active(p, lane))
      continue;

    auto mem = ventus_rt::make_hybrid_memory(
        *p->get_mmu(), p->get_csr(CSR_PDS), p->get_csr(CSR_NUMW),
        p->get_csr(CSR_NUMT), p->get_csr(CSR_TID), lane);
    const reg_t slot = p->VU.elt<uint32_t>(2, vs2_num, lane);
    ventus_rt::release(mem, slot);
  }

  p->VU.vstart->write(0);
}
