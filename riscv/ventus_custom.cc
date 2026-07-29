#include "ventus_custom.h"
#include "ventus_rt_wavefront_queue.h"

#include "trap.h"
#include <array>
#include <cstdint>
#include <vector>

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

/* Adapter for the global wavefront ABI.  It deliberately exposes only
 * device-global u32 operations: enqueue must not be able to fall back to the
 * legacy PDS address transform.  Spike currently executes one SM at a time,
 * so this read-modify-write is the functional model of the RTcore tail atomic;
 * the global queue ABI itself remains valid when RTL supplies a real atomic. */
class GlobalRTQueueMemory {
public:
  explicit GlobalRTQueueMemory(mmu_t &mmu) : mmu_(mmu) {}

  uint32_t load_u32(uint64_t address)
  {
    return mmu_.load_uint32(address);
  }

  void store_u32(uint64_t address, uint32_t value)
  {
    mmu_.store_uint32(address, value);
  }

  uint32_t atomic_fetch_add_u32(uint64_t address, uint32_t value)
  {
    const uint32_t previous = load_u32(address);
    store_u32(address, previous + value);
    return previous;
  }

private:
  mmu_t &mmu_;
};

uint32_t active_lanes_from_vstart(processor_t *p)
{
  const reg_t vstart = p->VU.vstart->read();
  if (vstart >= VENTUS_CUSTOM_LANES)
    return 0;
  const uint32_t from_vstart =
      vstart == 0 ? UINT32_MAX : UINT32_MAX << vstart;
  return static_cast<uint32_t>(p->gpgpu_unit.simt_stack.get_mask()) &
         from_vstart;
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

void ventus_exec_rt_enqueue(processor_t *p, insn_t insn)
{
  require_ventus_custom_state(p, insn);

  const reg_t vl = p->VU.vl->read();
  const reg_t vs2_num = insn.rs2();
  const uint32_t active_mask = active_lanes_from_vstart(p);
  std::vector<uint64_t> mailbox_addresses(vl);
  GlobalRTQueueMemory memory(*p->get_mmu());
  bool have_valid_mailbox = false;
  uint32_t generation = 0;

  for (reg_t lane = p->VU.vstart->read(); lane < vl; ++lane) {
    if (!lane_active(p, lane))
      continue;

    /* This is a global staging-record address, not a PDS slot. */
    const uint32_t mailbox_addr = p->VU.elt<uint32_t>(2, vs2_num, lane);
    mailbox_addresses[lane] = mailbox_addr;
    if (!have_valid_mailbox &&
        memory.load_u32((uint64_t)mailbox_addr +
                        4u * ventus_rt_wavefront::kMailboxValidWord)) {
      generation = memory.load_u32((uint64_t)mailbox_addr +
                                   4u * ventus_rt_wavefront::kMailboxGenerationWord);
      have_valid_mailbox = true;
    }
  }

  if (have_valid_mailbox) {
    /* Queue identity comes from mailbox words 4/5.  No CSR or PDS state is
     * used to infer it, so sparse child emission remains per-lane correct. */
    ventus_rt_wavefront::GlobalLevelQueueRegistry<GlobalRTQueueMemory> queues(
        memory);
    const auto result = queues.submit_rt_enqueue_batch(mailbox_addresses,
                                                         active_mask, generation);
    if (result != ventus_rt_wavefront::LevelQueueResult::Accepted)
      throw trap_illegal_instruction(insn.bits());
  }

  p->VU.vstart->write(0);
}
