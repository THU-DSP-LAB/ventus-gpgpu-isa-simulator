#ifndef RISCV_VENTUS_MMA_H
#define RISCV_VENTUS_MMA_H

#include "ventus_custom_arith.h"
#include <array>
#include <cstdint>
#include <stdexcept>

enum class VentusMMAShape {
  M8N8K16 = 0,
  M16N8K16 = 1,
  M8N16K16 = 2,
  M16N16K16 = 3,
  M8N8K8 = 4,
  M16N8K8 = 5,
  M8N16K8 = 6,
  M16N16K8 = 7,
};

enum class VentusMMAInputType { TF32 = 0, FP16 = 1, BF16 = 2 };
enum class VentusMMAOutputType { FP16 = 0, FP32 = 1 };

namespace ventus_mma {

constexpr int LANES = 32;
constexpr int MAX_M = 16;
constexpr int MAX_N = 16;
constexpr int MAX_K = 16;
constexpr int VREGS = 256;
constexpr uint32_t TF32_MANTISSA_MASK = 0xffffe000u;
constexpr uint64_t FULL_WARP_MASK = (uint64_t(1) << LANES) - 1u;

using LaneValues = std::array<uint32_t, LANES>;
using RegisterFile = std::array<LaneValues, VREGS>;
using VentusMMARegisterFile = RegisterFile;
using Matrix = std::array<std::array<float, MAX_N>, MAX_M>;

struct ShapeInfo {
  int m;
  int n;
  int k;
  int a_regs;
  int b_regs;
  int c_regs;
};

struct Options {
  VentusMMAShape shape;
  VentusMMAInputType ab_type;
  VentusMMAOutputType cd_type;
  bool a_column_layout;
  bool b_row_layout;
};

struct ExecutionState {
  uint32_t vl;
  uint32_t vstart;
  uint64_t active_mask;
  uint32_t e32_lanes_per_vreg;
};

inline ShapeInfo get_shape_info(VentusMMAShape shape)
{
  switch (shape) {
  case VentusMMAShape::M8N8K16:
    return {8, 8, 16, 2, 2, 2};
  case VentusMMAShape::M16N8K16:
    return {16, 8, 16, 4, 2, 4};
  case VentusMMAShape::M8N16K16:
    return {8, 16, 16, 2, 4, 4};
  case VentusMMAShape::M16N16K16:
    return {16, 16, 16, 4, 4, 8};
  case VentusMMAShape::M8N8K8:
    return {8, 8, 8, 2, 2, 2};
  case VentusMMAShape::M16N8K8:
    return {16, 8, 8, 4, 2, 4};
  case VentusMMAShape::M8N16K8:
    return {8, 16, 8, 2, 4, 4};
  case VentusMMAShape::M16N16K8:
    return {16, 16, 8, 4, 4, 8};
  }
  throw std::invalid_argument("unsupported Ventus MMA shape");
}

inline bool is_k8_shape(VentusMMAShape shape)
{
  return static_cast<uint32_t>(shape) >=
         static_cast<uint32_t>(VentusMMAShape::M8N8K8);
}

inline int cd_carrier_regs(const ShapeInfo &shape, VentusMMAOutputType cd_type)
{
  return cd_type == VentusMMAOutputType::FP16 ? shape.c_regs / 2 : shape.c_regs;
}

inline void validate_options(const Options &options)
{
  const uint32_t cd_bits = static_cast<uint32_t>(options.cd_type);
  if (cd_bits > static_cast<uint32_t>(VentusMMAOutputType::FP32))
    throw std::invalid_argument("unsupported Ventus MMA output type");

  switch (options.ab_type) {
  case VentusMMAInputType::TF32:
    if (!is_k8_shape(options.shape) ||
        options.cd_type != VentusMMAOutputType::FP32)
      throw std::invalid_argument("unsupported Ventus MMA TF32 type combination");
    return;
  case VentusMMAInputType::FP16:
    return;
  case VentusMMAInputType::BF16:
    if (options.cd_type != VentusMMAOutputType::FP32)
      throw std::invalid_argument("unsupported Ventus MMA BF16 type combination");
    return;
  }
  throw std::invalid_argument("unsupported Ventus MMA input type");
}

inline float decode_ab(uint32_t value, VentusMMAInputType type)
{
  switch (type) {
  case VentusMMAInputType::TF32:
    return ventus_custom_arith::bit_cast_f32(value & TF32_MANTISSA_MASK);
  case VentusMMAInputType::FP16:
    return ventus_custom_arith::fp16_to_float(uint16_t(value & 0xffffu));
  case VentusMMAInputType::BF16:
    return ventus_custom_arith::bf16_to_float(uint16_t(value & 0xffffu));
  }
  throw std::invalid_argument("unsupported Ventus MMA input type");
}

inline uint32_t encode_cd(float value, VentusMMAOutputType type)
{
  switch (type) {
  case VentusMMAOutputType::FP16:
    return ventus_custom_arith::float_to_fp16(value);
  case VentusMMAOutputType::FP32:
    return ventus_custom_arith::bit_cast_u32(value);
  }
  throw std::invalid_argument("unsupported Ventus MMA output type");
}

inline float decode_cd(uint32_t value, VentusMMAOutputType type)
{
  switch (type) {
  case VentusMMAOutputType::FP16:
    return ventus_custom_arith::fp16_to_float(uint16_t(value & 0xffffu));
  case VentusMMAOutputType::FP32:
    return ventus_custom_arith::bit_cast_f32(value);
  }
  throw std::invalid_argument("unsupported Ventus MMA output type");
}

inline void set_ab(Matrix &matrix, int row, int col, uint32_t value,
                   VentusMMAInputType type)
{
  if (row < MAX_M && col < MAX_K)
    matrix[row][col] = decode_ab(value, type);
}

inline void load_a(const RegisterFile &regs, uint32_t base,
                   const ShapeInfo &shape, const Options &options,
                   Matrix &matrix)
{
  const bool wide = options.ab_type == VentusMMAInputType::TF32;
  for (int reg = 0; reg < shape.a_regs; ++reg) {
    for (int lane = 0; lane < LANES; ++lane) {
      const uint32_t value = regs[base + uint32_t(reg)][uint32_t(lane)];
      const int first = wide ? reg * LANES + lane : reg * LANES * 2 + lane * 2;
      const int elems = wide ? 1 : 2;
      for (int part = 0; part < elems; ++part) {
        const int idx = first + part;
        const int m = options.a_column_layout ? idx % shape.m : idx / shape.k;
        const int k = options.a_column_layout ? idx / shape.m : idx % shape.k;
        const uint32_t elem = part == 0 ? value & 0xffffu : value >> 16;
        if (m < shape.m && k < shape.k)
          set_ab(matrix, m, k, wide ? value : elem, options.ab_type);
      }
    }
  }
}

inline void load_b_block(const RegisterFile &regs, uint32_t base,
                         const ShapeInfo &shape, const Options &options,
                         int col_offset, Matrix &matrix)
{
  const bool wide = options.ab_type == VentusMMAInputType::TF32;
  for (int reg = 0; reg < shape.b_regs; ++reg) {
    for (int lane = 0; lane < LANES; ++lane) {
      const uint32_t value = regs[base + uint32_t(reg)][uint32_t(lane)];
      const int first = wide ? reg * LANES + lane : reg * LANES * 2 + lane * 2;
      const int elems = wide ? 1 : 2;
      for (int part = 0; part < elems; ++part) {
        const int idx = first + part;
        const int n = options.b_row_layout ? idx / shape.k : idx % shape.n;
        const int k = options.b_row_layout ? idx % shape.k : idx / shape.n;
        const uint32_t elem = part == 0 ? value & 0xffffu : value >> 16;
        if (n >= col_offset && n < col_offset + 8 && k < shape.k)
          set_ab(matrix, n - col_offset, k, wide ? value : elem, options.ab_type);
      }
    }
  }
}

inline void load_c_block(const RegisterFile &regs, uint32_t base,
                         const ShapeInfo &shape, const Options &options,
                         int col_offset, Matrix &matrix)
{
  const int carrier_regs = cd_carrier_regs(shape, options.cd_type);
  for (int reg = 0; reg < carrier_regs; ++reg) {
    for (int lane = 0; lane < LANES; ++lane) {
      const uint32_t value = regs[base + uint32_t(reg)][uint32_t(lane)];
      if (options.cd_type == VentusMMAOutputType::FP16) {
        const int idx0 = reg * LANES * 2 + lane * 2;
        const int idx1 = idx0 + 1;
        const int m0 = idx0 / shape.n;
        const int n0 = idx0 % shape.n;
        const int m1 = idx1 / shape.n;
        const int n1 = idx1 % shape.n;
        if (m0 < shape.m && n0 >= col_offset && n0 < col_offset + 8)
          matrix[m0][n0 - col_offset] = decode_cd(value & 0xffffu, options.cd_type);
        if (m1 < shape.m && n1 >= col_offset && n1 < col_offset + 8)
          matrix[m1][n1 - col_offset] = decode_cd(value >> 16, options.cd_type);
        continue;
      }

      const int idx = reg * LANES + lane;
      const int m = idx / shape.n;
      const int n = idx % shape.n;
      if (m < shape.m && n >= col_offset && n < col_offset + 8)
        matrix[m][n - col_offset] = decode_cd(value, options.cd_type);
    }
  }
}

inline void multiply_accumulate(const Matrix &a, const Matrix &b,
                                const ShapeInfo &shape, Matrix &d)
{
  for (int m = 0; m < shape.m; ++m)
    for (int n = 0; n < 8; ++n)
      for (int k = 0; k < shape.k; ++k)
        d[m][n] += a[m][k] * b[n][k];
}

inline void store_d(RegisterFile &regs, uint32_t base, const ShapeInfo &shape,
                    const Options &options, const Matrix &matrix)
{
  const int carrier_regs = cd_carrier_regs(shape, options.cd_type);
  for (int reg = 0; reg < carrier_regs; ++reg) {
    for (int lane = 0; lane < LANES; ++lane) {
      if (options.cd_type == VentusMMAOutputType::FP16) {
        const int idx0 = reg * LANES * 2 + lane * 2;
        const int idx1 = idx0 + 1;
        const int m0 = idx0 / shape.n;
        const int n0 = idx0 % shape.n;
        const int m1 = idx1 / shape.n;
        const int n1 = idx1 % shape.n;
        regs[base + uint32_t(reg)][uint32_t(lane)] =
            ventus_custom_arith::pack_halves(uint16_t(encode_cd(matrix[m0][n0], options.cd_type)),
                                             uint16_t(encode_cd(matrix[m1][n1], options.cd_type)));
        continue;
      }

      const int idx = reg * LANES + lane;
      const int m = idx / shape.n;
      const int n = idx % shape.n;
      regs[base + uint32_t(reg)][uint32_t(lane)] = encode_cd(matrix[m][n], options.cd_type);
    }
  }
}

inline void check_register_bounds(uint32_t rd_base, uint32_t rs1_base,
                                  uint32_t rs2_base, const Options &options)
{
  validate_options(options);
  const ShapeInfo shape = get_shape_info(options.shape);
  if (rd_base + uint32_t(cd_carrier_regs(shape, options.cd_type)) > VREGS ||
      rs1_base + uint32_t(shape.a_regs) > VREGS ||
      rs2_base + uint32_t(shape.b_regs) > VREGS)
    throw std::out_of_range("Ventus MMA vector register group exceeds NVPR");
}

inline void check_full_warp_state(const ExecutionState &state)
{
  // MMA is a warp-level operation: reject partial vector/SIMT states instead
  // of mixing full-warp matrix compute with partial-lane writeback.
  if (state.vl != LANES)
    throw std::invalid_argument("Ventus MMA requires vl == 32");
  if (state.vstart != 0)
    throw std::invalid_argument("Ventus MMA requires vstart == 0");
  if ((state.active_mask & FULL_WARP_MASK) != FULL_WARP_MASK)
    throw std::invalid_argument("Ventus MMA requires all 32 SIMT lanes active");
  if (state.e32_lanes_per_vreg < LANES)
    throw std::invalid_argument("Ventus MMA requires 32 e32 lanes per vreg");
}

inline void execute_mma_register_file(RegisterFile &regs, uint32_t rd_base,
                                      uint32_t rs1_base, uint32_t rs2_base,
                                      const Options &options)
{
  check_register_bounds(rd_base, rs1_base, rs2_base, options);
  const ShapeInfo shape = get_shape_info(options.shape);
  Matrix logical_d{};
  Matrix a{};

  load_a(regs, rs1_base, shape, options, a);
  for (int nb = 0; nb < shape.n / 8; ++nb) {
    Matrix b{};
    Matrix block{};
    load_b_block(regs, rs2_base, shape, options, nb * 8, b);
    load_c_block(regs, rd_base, shape, options, nb * 8, block);
    multiply_accumulate(a, b, shape, block);
    for (int m = 0; m < shape.m; ++m)
      for (int n = 0; n < 8; ++n)
        logical_d[m][nb * 8 + n] = block[m][n];
  }

  store_d(regs, rd_base, shape, options, logical_d);
}

} // namespace ventus_mma

#endif
