// Exercise the production VF operand macro without constructing a full core.
#include <array>
#include <cassert>
#include <cstdint>
#include "v_ext_macros.h"

struct float32_t { uint32_t v; };
static float32_t f32(uint64_t bits) { return {uint32_t(bits)}; }

struct TestVectorUnit {
  std::array<float32_t, 4> regs{};
  template <typename T>
  T &elt(int, unsigned reg, unsigned, bool = false) {
    return regs.at(reg);
  }
};

struct TestProcessor {
  std::array<uint64_t, 64> scalar{};
  TestVectorUnit VU;
  unsigned extension = 0;
  unsigned ext_rs1() const { return extension; }
};

#define P (*p)
#define READ_REG(reg) p->scalar.at(reg)

static void check_scalar_bank(unsigned extension, uint32_t expected) {
  TestProcessor core;
  TestProcessor *p = &core;
  core.extension = extension;
  // Different low/high values expose an ignored register extension.
  core.scalar[5] = 0x3f800000;
  core.scalar[37] = 0x40000000;
  const unsigned rd_num = 0, rs1_num = 5, rs2_num = 1, i = 0;
  VFP_VF_PARAMS(32);
  assert(rs1.v == expected);
  vd.v = rs1.v;
  assert(core.VU.regs[0].v == expected);
  (void)vs2;
}

int main() {
  check_scalar_bank(0, 0x3f800000);
  check_scalar_bank(32, 0x40000000);
}
