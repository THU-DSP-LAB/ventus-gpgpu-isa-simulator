#ifndef RISCV_VENTUS_RT_H
#define RISCV_VENTUS_RT_H

#include <cstdint>
#include <cstring>

#ifndef VENTUS_RT_STANDALONE
#include "decode.h"
#include "mmu.h"
#else
using reg_t = uint64_t;
class mmu_t;
#endif
namespace ventus_rt {

#if defined(__GNUC__)
#define VENTUS_RT_API __attribute__((visibility("default")))
#else
#define VENTUS_RT_API
#endif

constexpr reg_t lanes = 32;

/* Generated from Mesa's canonical fixed PDS-header ABI.  Shader payload and
 * compiler continuation storage remain shader-owned and are not constants. */
#include "ventus_rt_abi_generated.h"

constexpr uint32_t bvh_magic = 0x56545254; // "VTRT"
constexpr uint32_t bvh_version = 1;
constexpr uint32_t geometry_triangle_list = 1;
constexpr uint32_t geometry_procedural_aabb_list = 2;

constexpr uint32_t as_magic = 0x53415456; // "VTAS"
constexpr uint32_t as_version = 1;
constexpr uint32_t as_type_blas = 1;
constexpr uint32_t as_type_tlas = 2;
constexpr uint32_t as_header_size = 0x50;
constexpr uint32_t as_header_magic = 0x00;
constexpr uint32_t as_header_version = 0x04;
constexpr uint32_t as_header_type = 0x08;
constexpr uint32_t as_header_root_node_ref = 0x10;

constexpr uint32_t node_ref_type_mask = 0x7;
constexpr uint32_t node_ref_offset_mask = ~node_ref_type_mask;
constexpr uint32_t node_box4 = 1;
constexpr uint32_t node_triangle = 2;
constexpr uint32_t node_instance = 3;
constexpr uint32_t node_aabb = 4;
constexpr uint32_t invalid_node_ref = 0xffffffffu;

constexpr uint32_t box4_child_ref = 0x00;
constexpr uint32_t box4_min_x = 0x10;
constexpr uint32_t box4_min_y = 0x20;
constexpr uint32_t box4_min_z = 0x30;
constexpr uint32_t box4_max_x = 0x40;
constexpr uint32_t box4_max_y = 0x50;
constexpr uint32_t box4_max_z = 0x60;

constexpr uint32_t triangle_v0 = 0x00;
constexpr uint32_t triangle_v1 = 0x0c;
constexpr uint32_t triangle_v2 = 0x18;
constexpr uint32_t triangle_primitive_id = 0x24;
constexpr uint32_t triangle_geometry_id = 0x28;
constexpr uint32_t triangle_sbt_record_offset = 0x2c;
constexpr uint32_t triangle_flags = 0x30;
constexpr uint32_t triangle_instance_id = 0x34;
constexpr uint32_t triangle_primitive_addr_lo = 0x38;

constexpr uint32_t aabb_min = 0x00;
constexpr uint32_t aabb_max = 0x0c;
constexpr uint32_t aabb_primitive_id = 0x18;
constexpr uint32_t aabb_geometry_id = 0x1c;
constexpr uint32_t aabb_sbt_record_offset = 0x20;
constexpr uint32_t aabb_hit_kind = 0x24;
constexpr uint32_t aabb_flags = 0x28;
constexpr uint32_t aabb_primitive_addr_lo = 0x30;
constexpr uint32_t aabb_instance_addr_lo = 0x38;

constexpr uint32_t instance_blas_addr_lo = 0x00;
constexpr uint32_t instance_custom_index = 0x08;
constexpr uint32_t instance_mask = 0x0c;
constexpr uint32_t instance_sbt_record_offset = 0x10;
constexpr uint32_t instance_flags = 0x14;
constexpr uint32_t instance_instance_id = 0x18;
constexpr uint32_t instance_object_to_world = 0x20;
constexpr uint32_t instance_world_to_object = 0x50;

constexpr uint32_t ray_flag_force_opaque = 1u << 0;
constexpr uint32_t ray_flag_force_non_opaque = 1u << 1;
constexpr uint32_t ray_flag_terminate_on_first_hit = 1u << 2;
constexpr uint32_t instance_flag_force_opaque = 1u << 2;
constexpr uint32_t instance_flag_force_non_opaque = 1u << 3;

inline reg_t pds_physical_addr(reg_t pds_base, reg_t num_warps,
                               reg_t num_threads, reg_t tid, reg_t lane,
                               reg_t logical_addr)
{
  const reg_t aligned = (logical_addr >> 2) << 2;
  return pds_base + num_warps * num_threads * aligned + ((tid + lane) << 2);
}

inline reg_t word_addr(reg_t slot, reg_t byte_base, reg_t word)
{
  return slot + byte_base + word * sizeof(uint32_t);
}

inline uint32_t bit_cast_u32(float value)
{
  uint32_t bits = 0;
  static_assert(sizeof(bits) == sizeof(value), "float must be 32-bit");
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}

inline float bit_cast_f32(uint32_t bits)
{
  float value = 0.0f;
  static_assert(sizeof(bits) == sizeof(value), "float must be 32-bit");
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}

/* A stable, non-template bridge into libspike_main's RTcore implementation.
 * It intentionally exposes only memory transactions and a caller-owned
 * continuation key; BVH state and STL containers stay in ventus_rt.cc. */
class RtMemory {
public:
  using Load32 = uint32_t (*)(void *state, reg_t address);
  using Store32 = void (*)(void *state, reg_t address, uint32_t value);
  using ContextKey = uint64_t (*)(void *state, reg_t slot);

  RtMemory(void *state, Load32 load32, Store32 store32, ContextKey context_key)
      : state_(state), load32_(load32), store32_(store32),
        context_key_(context_key) {}

  uint32_t load32(reg_t address) { return load32_(state_, address); }
  void store32(reg_t address, uint32_t value)
  {
    store32_(state_, address, value);
  }
  uint64_t rt_context_key(reg_t slot) const
  {
    return context_key_(state_, slot);
  }

private:
  void *state_;
  Load32 load32_;
  Store32 store32_;
  ContextKey context_key_;
};

inline uint32_t load_word(RtMemory &memory, reg_t slot, reg_t byte_base,
                          reg_t word)
{
  return memory.load32(word_addr(slot, byte_base, word));
}

inline void store_word(RtMemory &memory, reg_t slot, reg_t byte_base,
                       reg_t word, uint32_t value)
{
  memory.store32(word_addr(slot, byte_base, word), value);
}

VENTUS_RT_API uint32_t traverse(RtMemory &memory, reg_t slot);
VENTUS_RT_API void release(RtMemory &memory, reg_t slot);

#ifndef VENTUS_RT_STANDALONE
VENTUS_RT_API uint32_t traverse_spike(mmu_t &mmu, reg_t pds_base,
                                      reg_t num_warps, reg_t num_threads,
                                      reg_t tid, reg_t lane, reg_t slot);
VENTUS_RT_API void release_spike(mmu_t &mmu, reg_t pds_base,
                                 reg_t num_warps, reg_t num_threads,
                                 reg_t tid, reg_t lane, reg_t slot);
VENTUS_RT_API void reset_private_contexts_for_simulation();
#endif

#undef VENTUS_RT_API

} // namespace ventus_rt

#endif
