// Execute the real instruction handlers, with deliberately different values
// in encoded low registers, extended registers and the other FP domain.
#include "processor.h"

#include <cstring>
#include <iostream>
#include <stdexcept>

static uint32_t bits(float value)
{
  uint32_t result;
  std::memcpy(&result, &value, sizeof(result));
  return result;
}

#define HANDLER(name) extern reg_t rv32i_##name(processor_t *, insn_t, reg_t)
HANDLER(regext);
HANDLER(vfadd_vf);
HANDLER(vfmul_vf);
HANDLER(vfsub_vf);
HANDLER(vfdiv_vf);
HANDLER(vmflt_vf);
HANDLER(vmfle_vf);
HANDLER(vmfgt_vf);
HANDLER(vmfge_vf);
HANDLER(vfwmul_vf);
HANDLER(vfwadd_wf);
#undef HANDLER

static void execute(processor_t &proc, insn_func_t handler, uint32_t encoding)
{
  const insn_t insn(encoding);
  handler(&proc, insn, 0x80000000);
  // Match the extension lifetime in execute.cc; the prefix applies only to
  // the following instruction, rather than leaking into the next test.
  if (proc.get_state()->regext_enable)
    proc.get_state()->regext_enable = false;
  else
    proc.ext_clear();
}

static void check_profile(const char *profile, bool zfinx)
{
  const isa_parser_t isa(profile, "m");
  processor_t proc(&isa, "vlen:1024,elen:64", nullptr, 0, false,
                   stderr, std::cerr);
  proc.put_csr(CSR_MSTATUS, MSTATUS_VS | (zfinx ? 0 : MSTATUS_FS));
  proc.VU.set_vl(1, 1, 4, 2 << 3); // e32, m1
  proc.gpgpu_unit.simt_stack.init_mask_bits(4, 0xb); // lane 2 inactive
  auto *state = proc.get_state();
  struct Operation { uint32_t opcode; insn_func_t handler; uint32_t expected; };
  const Operation operations[] = {
    {MATCH_VFADD_VF, rv32i_vfadd_vf, bits(5)},
    {MATCH_VFMUL_VF, rv32i_vfmul_vf, bits(6)},
    {MATCH_VFSUB_VF, rv32i_vfsub_vf, bits(-1)},
    {MATCH_VFDIV_VF, rv32i_vfdiv_vf, bits(2.f / 3)},
    {MATCH_VMFLT_VF, rv32i_vmflt_vf, 1},
    {MATCH_VMFLE_VF, rv32i_vmfle_vf, 1},
    {MATCH_VMFGT_VF, rv32i_vmfgt_vf, 0},
    {MATCH_VMFGE_VF, rv32i_vmfge_vf, 0},
  };
  for (unsigned bank = 0; bank < 8; ++bank) {
    for (const auto &op : operations) {
      const unsigned scalar = 17 + bank * 32;
      const float low = bank ? -4 : 3;
      state->XPR.write(17, bits(zfinx ? low : -8));
      state->FPR.write(17, freg(f32(bits(zfinx ? -8 : low))));
      state->XPR.write(scalar, bits(zfinx ? 3 : -8));
      state->FPR.write(scalar, freg(f32(bits(zfinx ? -8 : 3))));
      for (unsigned lane = 0; lane < 4; ++lane) {
        proc.VU.elt<uint32_t>(-1, 34, lane) = bits(2);
        proc.VU.elt<uint32_t>(-1, 2, lane) = bits(9);
        proc.VU.elt<uint32_t>(-1, 36, lane) = 0xdeadbeef;
        proc.VU.elt<uint32_t>(-1, 4, lane) = 0x12345678;
      }
      // rd=v36, rs2=v34, rs1=x/f(17+32*bank); all three slots extend.
      execute(proc, rv32i_regext, MATCH_REGEXT | ((1 | (bank << 3) | (1 << 6)) << 20));
      execute(proc, op.handler, op.opcode | (1 << 25) | (2 << 20) | (17 << 15) | (4 << 7));
      for (unsigned lane = 0; lane < 4; ++lane) {
        const uint32_t expected = lane == 2 ? 0xdeadbeef : op.expected;
        if (proc.VU.elt<uint32_t>(-1, 36, lane) != expected ||
            proc.VU.elt<uint32_t>(-1, 4, lane) != 0x12345678)
          throw std::runtime_error(std::string(profile) +
              " VF regext mismatch bank=" + std::to_string(bank) +
              " opcode=" + std::to_string(op.opcode));
      }
      // Without another prefix, the next instruction must use the low bank.
      execute(proc, rv32i_vfadd_vf, MATCH_VFADD_VF | (1 << 25) | (2 << 20) | (17 << 15) | (4 << 7));
      if (proc.VU.elt<uint32_t>(-1, 4, 0) != bits(9 + low))
        throw std::runtime_error("VF regext prefix leaked into following instruction");
    }
  }
}

static void check_widening()
{
  const isa_parser_t isa("rv32gcv_zfh", "m");
  processor_t proc(&isa, "vlen:1024,elen:64", nullptr, 0, false,
                   stderr, std::cerr);
  proc.put_csr(CSR_MSTATUS, MSTATUS_VS | MSTATUS_FS);
  proc.gpgpu_unit.simt_stack.init_mask_bits(4, 0xb);
  for (unsigned width : {16U, 32U}) {
    proc.VU.set_vl(1, 1, 4, (width == 16 ? 1 : 2) << 3);
    for (unsigned bank = 0; bank < 8; ++bank) {
      auto *state = proc.get_state();
      const float low = bank ? -4 : 3;
      if (width == 16) {
        state->FPR.write(17, freg(f16(bank ? 0xc400 : 0x4200)));
        state->FPR.write(17 + bank * 32, freg(f16(0x4200)));
      } else {
        state->FPR.write(17, freg(f32(bits(low))));
        state->FPR.write(17 + bank * 32, freg(f32(bits(3))));
      }
      for (bool wide_source : {false, true}) {
        for (unsigned lane = 0; lane < 4; ++lane) {
          if (width == 16) {
            if (wide_source)
              proc.VU.elt<uint32_t>(-1, 34, lane) = bits(2);
            else
              proc.VU.elt<uint16_t>(-1, 34, lane) = 0x4000;
            proc.VU.elt<uint32_t>(-1, 36, lane) = 0xdeadbeef;
          } else {
            if (wide_source)
              proc.VU.elt<float64_t>(-1, 34, lane) = f64(0x4000000000000000);
            else
              proc.VU.elt<uint32_t>(-1, 34, lane) = bits(2);
            proc.VU.elt<uint64_t>(-1, 36, lane) = 0xdeadbeef;
          }
        }
        execute(proc, rv32i_regext, MATCH_REGEXT | ((1 | (bank << 3) | (1 << 6)) << 20));
        const uint32_t opcode = wide_source ? MATCH_VFWADD_WF : MATCH_VFWMUL_VF;
        execute(proc, wide_source ? rv32i_vfwadd_wf : rv32i_vfwmul_vf,
                opcode | (1 << 25) | (2 << 20) | (17 << 15) | (4 << 7));
        for (unsigned lane = 0; lane < 4; ++lane) {
          const uint64_t value = width == 16 ?
              proc.VU.elt<uint32_t>(-1, 36, lane) : proc.VU.elt<uint64_t>(-1, 36, lane);
          const uint64_t expected = lane == 2 ? 0xdeadbeef : width == 16 ?
              bits(wide_source ? 5 : 6) : wide_source ?
              UINT64_C(0x4014000000000000) : UINT64_C(0x4018000000000000);
          if (value != expected)
            throw std::runtime_error("VF/WF widening regext mismatch width=" +
                std::to_string(width) + " bank=" + std::to_string(bank));
        }
      }
    }
  }
}

int main()
{
  try {
    check_profile("rv32ima_zfinx_zve32f", true);
    check_profile("rv32if_zve32f", false);
    check_widening();
    std::cout << "PASS VF regext: 2 FP domains, 8 banks, 8 operations, 16/32-bit widening, inactive lanes and prefix lifetime\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
  }
}
