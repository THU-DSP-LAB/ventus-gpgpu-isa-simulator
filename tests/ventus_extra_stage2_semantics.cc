#include "ventus_mma.h"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <stdexcept>

using namespace ventus_mma;
using namespace ventus_custom_arith;

static void write_fp16(RegisterFile &regs, uint32_t base, int idx, uint16_t bits)
{
  uint32_t &slot = regs[base + uint32_t(idx / 64)][uint32_t((idx % 64) / 2)];
  if ((idx & 1) == 0)
    slot = (slot & 0xffff0000u) | bits;
  else
    slot = (slot & 0x0000ffffu) | (uint32_t(bits) << 16);
}

static void write_f32(RegisterFile &regs, uint32_t base, int idx, float value)
{
  regs[base + uint32_t(idx / 32)][uint32_t(idx % 32)] = bit_cast_u32(value);
}

static float read_f32(const RegisterFile &regs, uint32_t base, int idx)
{
  return bit_cast_f32(regs[base + uint32_t(idx / 32)][uint32_t(idx % 32)]);
}

static uint16_t read_fp16(const RegisterFile &regs, uint32_t base, int idx)
{
  const uint32_t slot = regs[base + uint32_t(idx / 64)][uint32_t((idx % 64) / 2)];
  return (idx & 1) == 0 ? uint16_t(slot & 0xffffu) : uint16_t(slot >> 16);
}

static void fill_fp16_ab(RegisterFile &regs, uint32_t a_base, uint32_t b_base,
                         const ShapeInfo &shape)
{
  for (int m = 0; m < shape.m; ++m)
    for (int k = 0; k < shape.k; ++k)
      write_fp16(regs, a_base, m * shape.k + k, float_to_fp16(float(m + 1)));

  for (int n = 0; n < shape.n; ++n)
    for (int k = 0; k < shape.k; ++k)
      write_fp16(regs, b_base, n * shape.k + k, float_to_fp16(float(k + 1)));
}

static void fill_fp16_one_ab(RegisterFile &regs, uint32_t a_base,
                             uint32_t b_base, const ShapeInfo &shape)
{
  for (int m = 0; m < shape.m; ++m)
    for (int k = 0; k < shape.k; ++k)
      write_fp16(regs, a_base, m * shape.k + k, float_to_fp16(1.0f));

  for (int n = 0; n < shape.n; ++n)
    for (int k = 0; k < shape.k; ++k)
      write_fp16(regs, b_base, n * shape.k + k, float_to_fp16(1.0f));
}

static void fill_fp16_ab_column_layout(RegisterFile &regs, uint32_t a_base,
                                       uint32_t b_base, const ShapeInfo &shape)
{
  for (int m = 0; m < shape.m; ++m)
    for (int k = 0; k < shape.k; ++k)
      write_fp16(regs, a_base, k * shape.m + m, float_to_fp16(float(m + 1)));

  for (int n = 0; n < shape.n; ++n)
    for (int k = 0; k < shape.k; ++k)
      write_fp16(regs, b_base, k * shape.n + n, float_to_fp16(float(k + 1)));
}

static void check_fp16_to_fp32_m8n8k16()
{
  VentusMMARegisterFile regs{};
  const ShapeInfo shape = get_shape_info(VentusMMAShape::M8N8K16);
  fill_fp16_ab(regs, 8, 16, shape);
  for (int m = 0; m < shape.m; ++m)
    for (int n = 0; n < shape.n; ++n)
      write_f32(regs, 0, m * shape.n + n, float(n));

  execute_mma_register_file(regs, 0, 8, 16,
                            {VentusMMAShape::M8N8K16, VentusMMAInputType::FP16,
                             VentusMMAOutputType::FP32, false, true});

  assert(std::fabs(read_f32(regs, 0, 0) - 136.0f) < 0.001f);
  assert(std::fabs(read_f32(regs, 0, 8) - 272.0f) < 0.001f);
  assert(std::fabs(read_f32(regs, 0, 63) - 1095.0f) < 0.001f);
}

static void check_base_encoding_defaults_execute()
{
  constexpr uint32_t RD_BASE = 0;
  constexpr uint32_t A_BASE = 16;
  constexpr uint32_t B_BASE = 32;
  constexpr std::array<uint32_t, 8> base_encodings = {
      0x0000000a, 0x0200000a, 0x0400000a, 0x0600000a,
      0x0800000a, 0x0a00000a, 0x0c00000a, 0x0e00000a,
  };
  constexpr std::array<VentusMMAShape, 8> shapes = {
      VentusMMAShape::M8N8K16,
      VentusMMAShape::M16N8K16,
      VentusMMAShape::M8N16K16,
      VentusMMAShape::M16N16K16,
      VentusMMAShape::M8N8K8,
      VentusMMAShape::M16N8K8,
      VentusMMAShape::M8N16K8,
      VentusMMAShape::M16N16K8,
  };

  for (size_t i = 0; i < base_encodings.size(); ++i) {
    const Options options = decode_mma_options(base_encodings[i]);
    assert(options.shape == shapes[i]);
    assert(options.ab_type == VentusMMAInputType::FP16);
    assert(options.cd_type == VentusMMAOutputType::FP16);
    assert(!options.a_column_layout);
    assert(!options.b_row_layout);

    VentusMMARegisterFile regs{};
    const ShapeInfo shape = get_shape_info(options.shape);
    fill_fp16_one_ab(regs, A_BASE, B_BASE, shape);
    execute_mma_register_file(regs, RD_BASE, A_BASE, B_BASE, options);
    assert(read_fp16(regs, RD_BASE, 0) == float_to_fp16(float(shape.k)));
  }
}

static void check_fp16_column_layouts()
{
  VentusMMARegisterFile regs{};
  const ShapeInfo shape = get_shape_info(VentusMMAShape::M8N8K16);
  fill_fp16_ab_column_layout(regs, 8, 16, shape);

  execute_mma_register_file(regs, 0, 8, 16,
                            {VentusMMAShape::M8N8K16, VentusMMAInputType::FP16,
                             VentusMMAOutputType::FP32, true, false});

  assert(std::fabs(read_f32(regs, 0, 0) - 136.0f) < 0.001f);
  assert(std::fabs(read_f32(regs, 0, 8) - 272.0f) < 0.001f);
}

static void check_fp16_to_fp32_m16n16k16()
{
  VentusMMARegisterFile regs{};
  const ShapeInfo shape = get_shape_info(VentusMMAShape::M16N16K16);
  fill_fp16_ab(regs, 16, 32, shape);
  for (int m = 0; m < shape.m; ++m)
    for (int n = 0; n < shape.n; ++n)
      write_f32(regs, 0, m * shape.n + n, float(n));

  execute_mma_register_file(regs, 0, 16, 32,
                            {VentusMMAShape::M16N16K16, VentusMMAInputType::FP16,
                             VentusMMAOutputType::FP32, false, true});

  assert(std::fabs(read_f32(regs, 0, 0) - 136.0f) < 0.001f);
  assert(std::fabs(read_f32(regs, 0, 15) - 151.0f) < 0.001f);
  assert(std::fabs(read_f32(regs, 0, 240) - 2176.0f) < 0.001f);
  assert(std::fabs(read_f32(regs, 0, 255) - 2191.0f) < 0.001f);
}

static void check_fp16_to_packed_fp16()
{
  VentusMMARegisterFile regs{};
  const ShapeInfo shape = get_shape_info(VentusMMAShape::M8N8K16);
  fill_fp16_ab(regs, 8, 16, shape);
  write_fp16(regs, 0, 0, float_to_fp16(0.5f));
  write_fp16(regs, 0, 1, float_to_fp16(1.0f));

  execute_mma_register_file(regs, 0, 8, 16,
                            {VentusMMAShape::M8N8K16, VentusMMAInputType::FP16,
                             VentusMMAOutputType::FP16, false, true});

  assert(read_fp16(regs, 0, 0) == float_to_fp16(136.5f));
  assert(read_fp16(regs, 0, 1) == float_to_fp16(137.0f));
}

static void check_bf16_to_fp32()
{
  VentusMMARegisterFile regs{};
  const ShapeInfo shape = get_shape_info(VentusMMAShape::M8N8K16);
  for (int k = 0; k < shape.k; ++k) {
    write_fp16(regs, 8, k, float_to_bf16(2.0f));
    write_fp16(regs, 16, k, float_to_bf16(0.5f));
  }

  execute_mma_register_file(regs, 0, 8, 16,
                            {VentusMMAShape::M8N8K16, VentusMMAInputType::BF16,
                             VentusMMAOutputType::FP32, false, true});

  assert(std::fabs(read_f32(regs, 0, 0) - 16.0f) < 0.001f);
}

static void check_tf32_k8_to_fp32()
{
  VentusMMARegisterFile regs{};
  const ShapeInfo shape = get_shape_info(VentusMMAShape::M8N8K8);
  for (int k = 0; k < shape.k; ++k) {
    write_f32(regs, 8, k, bit_cast_f32(0x3f800fff));
    write_f32(regs, 16, k, 2.0f);
  }

  execute_mma_register_file(regs, 0, 8, 16,
                            {VentusMMAShape::M8N8K8, VentusMMAInputType::TF32,
                             VentusMMAOutputType::FP32, false, true});

  assert(read_f32(regs, 0, 0) == 16.0f);
}

static void check_invalid_type_combo()
{
  VentusMMARegisterFile regs{};
  try {
    (void)decode_mma_options(0x3000000a);
    assert(false);
  } catch (const std::invalid_argument&) {
  }

  try {
    execute_mma_register_file(regs, 0, 8, 16,
                              {VentusMMAShape::M8N8K16, VentusMMAInputType::TF32,
                               VentusMMAOutputType::FP32, false, true});
    assert(false);
  } catch (const std::invalid_argument&) {
  }

  try {
    execute_mma_register_file(regs, 0, 8, 16,
                              {VentusMMAShape::M8N8K16, VentusMMAInputType::BF16,
                               VentusMMAOutputType::FP16, false, true});
    assert(false);
  } catch (const std::invalid_argument&) {
  }

  try {
    execute_mma_register_file(regs, 0, 8, 16,
                              {VentusMMAShape::M8N8K8, VentusMMAInputType::TF32,
                               VentusMMAOutputType::FP16, false, true});
    assert(false);
  } catch (const std::invalid_argument&) {
  }

  try {
    execute_mma_register_file(regs, 0, 8, 16,
                              {VentusMMAShape::M8N8K16,
                               static_cast<VentusMMAInputType>(3),
                               VentusMMAOutputType::FP32, false, true});
    assert(false);
  } catch (const std::invalid_argument&) {
  }

  try {
    execute_mma_register_file(regs, 0, 8, 16,
                              {VentusMMAShape::M8N8K16, VentusMMAInputType::FP16,
                               static_cast<VentusMMAOutputType>(2), false, true});
    assert(false);
  } catch (const std::invalid_argument&) {
  }
}

static void check_register_bounds_failure()
{
  try {
    check_register_bounds(255, 8, 16,
                          {VentusMMAShape::M8N8K16, VentusMMAInputType::FP16,
                           VentusMMAOutputType::FP32, false, true});
    assert(false);
  } catch (const std::out_of_range&) {
  }
}

static void check_full_warp_policy()
{
  check_full_warp_state({32, 0, FULL_WARP_MASK, 32});

  try {
    check_full_warp_state({31, 0, FULL_WARP_MASK, 32});
    assert(false);
  } catch (const std::invalid_argument&) {
  }

  try {
    check_full_warp_state({32, 1, FULL_WARP_MASK, 32});
    assert(false);
  } catch (const std::invalid_argument&) {
  }

  try {
    check_full_warp_state({32, 0, FULL_WARP_MASK ^ 0x2u, 32});
    assert(false);
  } catch (const std::invalid_argument&) {
  }

  try {
    check_full_warp_state({32, 0, FULL_WARP_MASK, 8});
    assert(false);
  } catch (const std::invalid_argument&) {
  }
}

int main()
{
  check_full_warp_policy();
  check_base_encoding_defaults_execute();
  check_fp16_to_fp32_m8n8k16();
  check_fp16_column_layouts();
  check_fp16_to_fp32_m16n16k16();
  check_fp16_to_packed_fp16();
  check_bf16_to_fp32();
  check_tf32_k8_to_fp32();
  check_invalid_type_combo();
  check_register_bounds_failure();
  return 0;
}
