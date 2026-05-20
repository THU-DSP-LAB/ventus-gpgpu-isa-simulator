#include "ventus_custom_arith.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <stdexcept>

using namespace ventus_custom_arith;

static void check_fp16_rne_is_deterministic()
{
  const float halfway_input = 1.0f + fp16_to_float(0x1100);
  const uint16_t direct_result = float_to_fp16(halfway_input);
  const uint32_t vcvt_result = do_vcvt(bit_cast_u32(halfway_input),
                                       VentusVCvtOp::FP16_FP32);

  const uint32_t f16_lhs = pack_halves(0x3c00, 0x4000);
  const uint32_t f16_rhs = pack_halves(0x1100, 0x1400);
  const uint32_t packed_result = do_packed(f16_lhs, f16_rhs, 0,
                                           VentusPackedOp::Add,
                                           VentusPackedType::F16X2);
  const uint32_t packed_sfu_result = do_packed_sfu(pack_halves(0x4200, 0x4500),
                                                   VentusSFUOp::Sqrt,
                                                   VentusSFUMode::F16X2);
  const float quiet_nan = bit_cast_f32(0x7fa00000);
  const uint16_t direct_nan_result = float_to_fp16(quiet_nan);
  const uint32_t vcvt_nan_result = do_vcvt(0x7f80302c,
                                           VentusVCvtOp::FP16_FP32);
  const uint32_t packed_nan_result = do_packed(pack_halves(0x7e00, 0x3c00),
                                               pack_halves(0x3c00, 0x7e00),
                                               0, VentusPackedOp::Add,
                                               VentusPackedType::F16X2);
  const uint32_t packed_sfu_nan_result = do_packed_sfu(pack_halves(0x7e00, 0x7e00),
                                                       VentusSFUOp::Sqrt,
                                                       VentusSFUMode::F16X2);

  assert(direct_result == 0x3c01);
  assert(vcvt_result == 0x3c01);
  assert(packed_result == pack_halves(0x3c01, 0x4000));
  assert(packed_sfu_result == pack_halves(0x3eee, 0x4079));
  assert(direct_nan_result == 0x7e00);
  assert(vcvt_nan_result == 0x7e00);
  assert(packed_nan_result == pack_halves(0x7e00, 0x7e00));
  assert(packed_sfu_nan_result == pack_halves(0x7e00, 0x7e00));
}

int main()
{
  check_fp16_rne_is_deterministic();

  const uint32_t f16_lhs = pack_halves(0x3c00, 0x4000);
  const uint32_t f16_rhs = pack_halves(0x4000, 0x4200);
  const uint32_t f16_acc = pack_halves(0x3c00, 0x3c00);
  assert(do_packed(f16_lhs, f16_rhs, 0, VentusPackedOp::Add,
                   VentusPackedType::F16X2) == pack_halves(0x4200, 0x4500));
  assert(do_packed(f16_lhs, f16_rhs, 0, VentusPackedOp::Mul,
                   VentusPackedType::F16X2) == pack_halves(0x4000, 0x4600));
  assert(do_packed(f16_lhs, f16_rhs, f16_acc, VentusPackedOp::Fma,
                   VentusPackedType::F16X2) == pack_halves(0x4200, 0x4700));

  const uint32_t bf16_lhs = pack_halves(0x3f80, 0x4000);
  const uint32_t bf16_rhs = pack_halves(0x4000, 0x4040);
  const uint32_t bf16_acc = pack_halves(0x3f80, 0x3f80);
  assert(do_packed(bf16_lhs, bf16_rhs, 0, VentusPackedOp::Add,
                   VentusPackedType::BF16X2) == pack_halves(0x4040, 0x40a0));
  assert(do_packed(bf16_lhs, bf16_rhs, 0, VentusPackedOp::Mul,
                   VentusPackedType::BF16X2) == pack_halves(0x4000, 0x40c0));
  assert(do_packed(bf16_lhs, bf16_rhs, bf16_acc, VentusPackedOp::Fma,
                   VentusPackedType::BF16X2) == pack_halves(0x4040, 0x40e0));

  assert(do_vcvt(0x3c00, VentusVCvtOp::FP32_FP16) == 0x3f800000);
  assert(do_vcvt(0x3f800000, VentusVCvtOp::FP16_FP32) == 0x3c00);
  assert(do_vcvt(0x3f80, VentusVCvtOp::FP32_BF16) == 0x3f800000);
  assert(do_vcvt(0x3f800000, VentusVCvtOp::BF16_FP32) == 0x3f80);

  assert(bit_cast_u32(apply_sfu(1.0f, VentusSFUOp::Exp2)) == 0x40000000);
  assert(bit_cast_u32(apply_sfu(8.0f, VentusSFUOp::Log2)) == 0x40400000);
  assert(bit_cast_u32(apply_sfu(4.0f, VentusSFUOp::Rcp)) == 0x3e800000);
  assert(bit_cast_u32(apply_sfu(4.0f, VentusSFUOp::Sqrt)) == 0x40000000);
  assert(bit_cast_u32(apply_sfu(4.0f, VentusSFUOp::Rsqrt)) == 0x3f000000);
  assert(std::fabs(apply_sfu(0.5f, VentusSFUOp::Sin) - std::sin(0.5f)) < 0.000001f);
  assert(std::fabs(apply_sfu(0.5f, VentusSFUOp::Cos) - std::cos(0.5f)) < 0.000001f);
  assert(std::fabs(apply_sfu(0.5f, VentusSFUOp::Tanh) - std::tanh(0.5f)) < 0.000001f);
  assert(std::fabs(apply_sfu(0.5f, VentusSFUOp::Gelu) - 0.345714f) < 0.000001f);
  assert(do_packed_sfu(pack_halves(0x3c00, 0x4000), VentusSFUOp::Rcp,
                       VentusSFUMode::F16X2) == pack_halves(0x3c00, 0x3800));
  assert(do_packed_sfu(pack_halves(0x4000, 0x4080), VentusSFUOp::Sqrt,
                       VentusSFUMode::BF16X2) == pack_halves(0x3fb5, 0x4000));

  const float silu = apply_sfu(1.0f, VentusSFUOp::Silu);
  assert(std::fabs(silu - 0.7310586f) < 0.000001f);
  try {
    (void)do_packed_sfu(0, VentusSFUOp::Exp2, VentusSFUMode::FP32);
    assert(false);
  } catch (const std::invalid_argument&) {
  }
  return 0;
}
