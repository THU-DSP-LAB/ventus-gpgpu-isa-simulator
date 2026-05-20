#ifndef RISCV_VENTUS_CUSTOM_ARITH_H
#define RISCV_VENTUS_CUSTOM_ARITH_H

#include <cmath>
#include <cstdint>
#include <cstring>
#include <stdexcept>

enum class VentusPackedOp { Add, Mul, Fma };
enum class VentusPackedType { F16X2, BF16X2 };
enum class VentusVCvtOp { FP32_FP16, FP16_FP32, FP32_BF16, BF16_FP32 };
enum class VentusSFUOp {
  Exp2,
  Log2,
  Rcp,
  Sqrt,
  Rsqrt,
  Sin,
  Cos,
  Tanh,
  Gelu,
  Silu,
};
enum class VentusSFUMode { FP32, F16X2, BF16X2 };

namespace ventus_custom_arith {

constexpr float GELU_SQRT_2_OVER_PI = 0.7978845608028654f;
constexpr float GELU_CUBIC_COEFF = 0.044715f;

inline uint32_t bit_cast_u32(float value)
{
  uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}

inline float bit_cast_f32(uint32_t bits)
{
  float value = 0.0f;
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}

inline float fp16_to_float(uint16_t bits)
{
  const uint32_t sign = uint32_t(bits & 0x8000u) << 16;
  const uint32_t exp = (bits >> 10) & 0x1fu;
  uint32_t mant = bits & 0x03ffu;

  if (exp == 0x1fu)
    return bit_cast_f32(sign | 0x7f800000u | (mant << 13));

  if (exp == 0) {
    if (mant == 0)
      return bit_cast_f32(sign);

    int32_t adjusted_exp = -14;
    while ((mant & 0x0400u) == 0) {
      mant <<= 1;
      --adjusted_exp;
    }
    mant &= 0x03ffu;
    return bit_cast_f32(sign | (uint32_t(adjusted_exp + 127) << 23) |
                        (mant << 13));
  }

  return bit_cast_f32(sign | (uint32_t(exp + 112) << 23) | (mant << 13));
}

inline uint16_t float_to_fp16(float value)
{
  const uint32_t bits = bit_cast_u32(value);
  const uint32_t sign = (bits >> 16) & 0x8000u;
  const uint32_t exp = (bits >> 23) & 0xffu;
  uint32_t mant = bits & 0x7fffffu;

  if (exp == 0xffu) {
    if (mant == 0)
      return uint16_t(sign | 0x7c00u);
    return 0x7e00u;
  }

  const int32_t half_exp = int32_t(exp) - 127 + 15;
  if (half_exp >= 31)
    return uint16_t(sign | 0x7c00u);

  if (half_exp <= 0) {
    if (half_exp < -10)
      return uint16_t(sign);

    mant |= 0x800000u;
    const uint32_t shift = uint32_t(14 - half_exp);
    uint32_t rounded = mant >> shift;
    const uint32_t rem = mant & ((1u << shift) - 1u);
    const uint32_t halfway = 1u << (shift - 1u);
    if (rem > halfway || (rem == halfway && (rounded & 1u)))
      ++rounded;
    return uint16_t(sign | rounded);
  }

  uint32_t rounded = (uint32_t(half_exp) << 10) | (mant >> 13);
  const uint32_t rem = mant & 0x1fffu;
  if (rem > 0x1000u || (rem == 0x1000u && (rounded & 1u)))
    ++rounded;
  return uint16_t(sign | rounded);
}

inline float bf16_to_float(uint16_t bits)
{
  return bit_cast_f32(uint32_t(bits) << 16);
}

inline uint16_t float_to_bf16(float value)
{
  const uint32_t bits = bit_cast_u32(value);
  const uint32_t lsb = (bits >> 16) & 1u;
  const uint32_t rounding_bias = 0x7fffu + lsb;
  return uint16_t((bits + rounding_bias) >> 16);
}

inline uint32_t pack_halves(uint16_t lo, uint16_t hi)
{
  return uint32_t(lo) | (uint32_t(hi) << 16);
}

inline float apply_sfu(float value, VentusSFUOp op)
{
  switch (op) {
  case VentusSFUOp::Exp2:
    return std::exp2(value);
  case VentusSFUOp::Log2:
    return std::log2(value);
  case VentusSFUOp::Rcp:
    return 1.0f / value;
  case VentusSFUOp::Sqrt:
    return std::sqrt(value);
  case VentusSFUOp::Rsqrt:
    return 1.0f / std::sqrt(value);
  case VentusSFUOp::Sin:
    return std::sin(value);
  case VentusSFUOp::Cos:
    return std::cos(value);
  case VentusSFUOp::Tanh:
    return std::tanh(value);
  case VentusSFUOp::Gelu:
    return 0.5f * value *
           (1.0f + std::tanh(GELU_SQRT_2_OVER_PI *
                             (value + GELU_CUBIC_COEFF * value * value * value)));
  case VentusSFUOp::Silu:
    return value / (1.0f + std::exp(-value));
  }
  throw std::invalid_argument("unsupported Ventus SFU operation");
}

inline uint32_t do_packed(uint32_t lhs_bits, uint32_t rhs_bits,
                          uint32_t acc_bits, VentusPackedOp op,
                          VentusPackedType type)
{
  bool is_f16 = false;
  switch (type) {
  case VentusPackedType::F16X2:
    is_f16 = true;
    break;
  case VentusPackedType::BF16X2:
    is_f16 = false;
    break;
  default:
    throw std::invalid_argument("unsupported Ventus packed data type");
  }

  auto unpack = [is_f16](uint16_t bits) {
    return is_f16 ? fp16_to_float(bits) : bf16_to_float(bits);
  };
  auto pack = [is_f16](float value) {
    return is_f16 ? float_to_fp16(value) : float_to_bf16(value);
  };

  const float lhs_lo = unpack(lhs_bits & 0xffffu);
  const float lhs_hi = unpack(lhs_bits >> 16);
  const float rhs_lo = unpack(rhs_bits & 0xffffu);
  const float rhs_hi = unpack(rhs_bits >> 16);
  const float acc_lo = unpack(acc_bits & 0xffffu);
  const float acc_hi = unpack(acc_bits >> 16);

  float out_lo = 0.0f;
  float out_hi = 0.0f;
  switch (op) {
  case VentusPackedOp::Add:
    out_lo = lhs_lo + rhs_lo;
    out_hi = lhs_hi + rhs_hi;
    break;
  case VentusPackedOp::Mul:
    out_lo = lhs_lo * rhs_lo;
    out_hi = lhs_hi * rhs_hi;
    break;
  case VentusPackedOp::Fma:
    out_lo = lhs_lo * rhs_lo + acc_lo;
    out_hi = lhs_hi * rhs_hi + acc_hi;
    break;
  default:
    throw std::invalid_argument("unsupported Ventus packed operation");
  }

  return pack_halves(pack(out_lo), pack(out_hi));
}

inline uint32_t do_vcvt(uint32_t input, VentusVCvtOp op)
{
  switch (op) {
  case VentusVCvtOp::FP32_FP16:
    return bit_cast_u32(fp16_to_float(input & 0xffffu));
  case VentusVCvtOp::FP16_FP32:
    return float_to_fp16(bit_cast_f32(input));
  case VentusVCvtOp::FP32_BF16:
    return bit_cast_u32(bf16_to_float(input & 0xffffu));
  case VentusVCvtOp::BF16_FP32:
    return float_to_bf16(bit_cast_f32(input));
  }
  throw std::invalid_argument("unsupported Ventus conversion operation");
}

inline uint32_t do_packed_sfu(uint32_t input_bits, VentusSFUOp op,
                              VentusSFUMode mode)
{
  bool is_f16 = false;
  switch (mode) {
  case VentusSFUMode::F16X2:
    is_f16 = true;
    break;
  case VentusSFUMode::BF16X2:
    is_f16 = false;
    break;
  default:
    throw std::invalid_argument("unsupported Ventus packed SFU mode");
  }

  auto unpack = [is_f16](uint16_t bits) {
    return is_f16 ? fp16_to_float(bits) : bf16_to_float(bits);
  };
  auto pack = [is_f16](float value) {
    return is_f16 ? float_to_fp16(value) : float_to_bf16(value);
  };

  const float lo = apply_sfu(unpack(input_bits & 0xffffu), op);
  const float hi = apply_sfu(unpack(input_bits >> 16), op);
  return pack_halves(pack(lo), pack(hi));
}

} // namespace ventus_custom_arith

#endif
