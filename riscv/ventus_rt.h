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

/* Generated from Mesa's canonical VTAS binary ABI.  Spike owns traversal
 * behavior, but not a second handwritten copy of the memory layout. */
#include "ventus_vtas_abi_generated.h"

constexpr uint32_t ray_flag_force_opaque = 1u << 0;
constexpr uint32_t ray_flag_force_non_opaque = 1u << 1;
constexpr uint32_t ray_flag_terminate_on_first_hit = 1u << 2;
constexpr uint32_t instance_flag_force_opaque = 1u << 2;
constexpr uint32_t instance_flag_force_non_opaque = 1u << 3;

inline uint32_t unpack_trace_meta_field(uint32_t word, uint32_t shift,
                                        uint32_t bits)
{
  return (word >> shift) & ((1u << bits) - 1u);
}

inline uint32_t pack_trace_meta0(uint32_t flags, uint32_t cull_mask,
                                 uint32_t sbt_offset, uint32_t sbt_stride)
{
  return ((flags & ((1u << trace_meta0_flags_bits) - 1u))
          << trace_meta0_flags_shift) |
         ((cull_mask & ((1u << trace_meta0_cull_mask_bits) - 1u))
          << trace_meta0_cull_mask_shift) |
         ((sbt_offset & ((1u << trace_meta0_sbt_offset_bits) - 1u))
          << trace_meta0_sbt_offset_shift) |
         ((sbt_stride & ((1u << trace_meta0_sbt_stride_bits) - 1u))
          << trace_meta0_sbt_stride_shift);
}

inline uint32_t pack_trace_meta1(uint32_t miss_index, uint32_t active_level)
{
  return ((miss_index & ((1u << trace_meta1_miss_index_bits) - 1u))
          << trace_meta1_miss_index_shift) |
         ((active_level & ((1u << trace_meta1_active_level_bits) - 1u))
          << trace_meta1_active_level_shift);
}

/* The fixed RT header begins at PDS logical word zero.  `pds_warp_tid_base`
 * is the smallest PDS thread ID in the issuing warp; `lane` is always the
 * local hardware lane (0..31), including for a partially active warp.
 *
 * Every fixed-header field, including candidate and committed hit records, is
 * field-major.  Keep this translation at the PDS boundary: compiler and
 * traversal code continue to use lane-private logical ABI offsets. */
inline reg_t pds_header_word_addr(reg_t pds_base, reg_t num_warps,
                                  reg_t num_threads,
                                  reg_t pds_warp_tid_base, reg_t lane,
                                  reg_t word)
{
  const reg_t pds_thread_count = num_warps * num_threads;
  const reg_t tid = pds_warp_tid_base + lane;
  return pds_base + sizeof(uint32_t) * (word * pds_thread_count + tid);
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

/* The candidate and committed records share this compact representation.
 * `valid` has its own bit so the three-bit status can distinguish the
 * candidate kind without affecting record lifetime. */
inline uint32_t pack_hit_record_meta0(bool valid, uint32_t status,
                                      bool front_face, bool opaque,
                                      bool need_software_opacity_test,
                                      uint32_t instance_custom_index)
{
  constexpr uint32_t custom_index_mask =
      (1u << hit_record_meta0_instance_custom_index_bits) - 1u;
  return (valid ? (1u << hit_record_meta0_valid_shift) : 0u) |
         ((status & ((1u << hit_record_meta0_status_bits) - 1u))
          << hit_record_meta0_status_shift) |
         (front_face ? (1u << hit_record_meta0_front_face_shift) : 0u) |
         (opaque ? (1u << hit_record_meta0_opaque_shift) : 0u) |
         (need_software_opacity_test
              ? (1u << hit_record_meta0_need_software_opacity_test_shift)
              : 0u) |
         ((instance_custom_index & custom_index_mask)
          << hit_record_meta0_instance_custom_index_shift);
}

inline bool hit_record_valid(uint32_t meta0)
{
  return (meta0 & (1u << hit_record_meta0_valid_shift)) != 0;
}

inline uint32_t hit_record_status(uint32_t meta0)
{
  return (meta0 >> hit_record_meta0_status_shift) &
         ((1u << hit_record_meta0_status_bits) - 1u);
}

inline bool hit_record_front_face(uint32_t meta0)
{
  return (meta0 & (1u << hit_record_meta0_front_face_shift)) != 0;
}

inline bool hit_record_opaque(uint32_t meta0)
{
  return (meta0 & (1u << hit_record_meta0_opaque_shift)) != 0;
}

inline bool hit_record_needs_software_opacity_test(uint32_t meta0)
{
  return (meta0 &
          (1u << hit_record_meta0_need_software_opacity_test_shift)) != 0;
}

inline uint32_t hit_record_instance_custom_index(uint32_t meta0)
{
  return (meta0 >> hit_record_meta0_instance_custom_index_shift) &
         ((1u << hit_record_meta0_instance_custom_index_bits) - 1u);
}

inline uint32_t pack_hit_record_meta1(uint32_t instance_sbt_record_offset)
{
  return (instance_sbt_record_offset &
          ((1u << hit_record_meta1_instance_sbt_record_offset_bits) - 1u))
         << hit_record_meta1_instance_sbt_record_offset_shift;
}

inline uint32_t hit_record_instance_sbt_record_offset(uint32_t meta1)
{
  return (meta1 >> hit_record_meta1_instance_sbt_record_offset_shift) &
         ((1u << hit_record_meta1_instance_sbt_record_offset_bits) - 1u);
}

inline uint32_t hit_record_sbt_index(uint32_t meta1, uint32_t geometry_id,
                                     uint32_t trace_sbt_offset)
{
  return hit_record_instance_sbt_record_offset(meta1) + geometry_id +
         trace_sbt_offset;
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
                                      reg_t pds_warp_tid_base, reg_t lane);
VENTUS_RT_API void release_spike(mmu_t &mmu, reg_t pds_base,
                                 reg_t num_warps, reg_t num_threads,
                                 reg_t pds_warp_tid_base, reg_t lane);
VENTUS_RT_API void reset_private_contexts_for_simulation();
#endif

#undef VENTUS_RT_API

} // namespace ventus_rt

#endif
