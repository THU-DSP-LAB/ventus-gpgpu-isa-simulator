#ifndef RISCV_VENTUS_RT_H
#define RISCV_VENTUS_RT_H

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ventus_rt_vtas_v2_codec.h"

#ifndef VENTUS_RT_STANDALONE
#include "decode.h"
#include "mmu.h"
#else
using reg_t = uint64_t;
class mmu_t;
#endif

namespace ventus_rt {

constexpr reg_t lanes = 32;
/* RTcore accesses only the fixed V4 frame header.  Pipeline payload and LLVM
 * private spills follow it but are shader-owned. */
constexpr reg_t rt_pds_base_bytes = 0;
constexpr reg_t rt_region_size_bytes = 276;
constexpr reg_t rt_pds_total_size_bytes =
    rt_pds_base_bytes + rt_region_size_bytes;
constexpr reg_t rt_v5_pds_size_bytes = 384;
constexpr reg_t rt_worker_local_size_bytes = rt_region_size_bytes;

constexpr reg_t rt_v5_private_frontier_0 = 0x00;
constexpr reg_t rt_v5_private_ray_tmin = 0x30;
constexpr reg_t rt_v5_private_ray_tmax = 0x34;
constexpr reg_t rt_v5_private_ray_flags = 0x38;
constexpr reg_t rt_v5_private_hot_control = 0x3c;
constexpr reg_t rt_v5_private_origin = 0x40;
constexpr reg_t rt_v5_private_inv_direction = 0x4c;
constexpr reg_t rt_v5_private_active_as_base = 0x58;
constexpr reg_t rt_v5_private_direction = 0x60;
constexpr reg_t rt_v5_private_instance_sbt_contribution = 0x6c;
constexpr reg_t rt_v5_private_instance_metadata_reference = 0x70;
constexpr reg_t rt_v5_private_instance_index = 0x78;
constexpr reg_t rt_v5_private_instance_custom_index = 0x7c;

constexpr reg_t slot_status = 0;
constexpr reg_t slot_accel_lo = 1;
constexpr reg_t slot_accel_hi = 2;
constexpr reg_t slot_flags = 3;
constexpr reg_t slot_cull_mask = 4;
constexpr reg_t slot_sbt_offset = 5;
constexpr reg_t slot_sbt_stride = 6;
constexpr reg_t slot_miss_index = 7;
constexpr reg_t slot_origin_x = 8;
constexpr reg_t slot_origin_y = 9;
constexpr reg_t slot_origin_z = 10;
constexpr reg_t slot_tmin = 11;
constexpr reg_t slot_direction_x = 12;
constexpr reg_t slot_direction_y = 13;
constexpr reg_t slot_direction_z = 14;
constexpr reg_t slot_tmax = 15;
constexpr reg_t slot_payload_ptr_lo = 16;
constexpr reg_t slot_payload_ptr_hi = 17;
constexpr reg_t slot_hit_t = 18;
constexpr reg_t slot_sbt_index = 19;
constexpr reg_t slot_launch_id_x = 20;
constexpr reg_t slot_launch_id_y = 21;
constexpr reg_t slot_launch_id_z = 22;

constexpr reg_t cps_header_base = 96;
constexpr reg_t cps_frame_base = 0;
constexpr reg_t cps_active_level = 1;
constexpr reg_t cps_fragment_id = 2;
constexpr reg_t cps_flags = 3;

constexpr reg_t control_base = 112;
constexpr reg_t control_callback_decision = 0;
constexpr uint32_t callback_pending = 0;
constexpr uint32_t callback_accept = 1;
constexpr uint32_t callback_ignore = 2;
constexpr uint32_t callback_terminate = 3;

constexpr reg_t committed_hit_record_base = 196;
constexpr reg_t candidate_hit_record_base = 116;
constexpr reg_t hit_attrib_base = 276;
constexpr reg_t hit_record_status = 0;
constexpr reg_t hit_record_hit_t = 1;
constexpr reg_t hit_record_sbt_index = 2;
constexpr reg_t hit_record_shader_record_ptr_lo = 3;
constexpr reg_t hit_record_shader_record_ptr_hi = 4;
constexpr reg_t hit_record_primitive_id = 5;
constexpr reg_t hit_record_instance_id = 6;
constexpr reg_t hit_record_geometry_id = 7;
constexpr reg_t hit_record_hit_kind = 8;
constexpr reg_t hit_record_barycentrics_x = 9;
constexpr reg_t hit_record_barycentrics_y = 10;
constexpr reg_t hit_record_primitive_addr_lo = 11;
constexpr reg_t hit_record_primitive_addr_hi = 12;
constexpr reg_t hit_record_instance_addr_lo = 13;
constexpr reg_t hit_record_instance_addr_hi = 14;
constexpr reg_t hit_record_front_face = 15;
constexpr reg_t hit_record_opaque = 16;
constexpr reg_t hit_record_need_software_opacity_test = 17;
constexpr reg_t hit_record_instance_sbt_record_offset = 18;

constexpr uint32_t slot_status_idle = 0;
constexpr uint32_t slot_status_trace_request = 1;
constexpr uint32_t slot_status_terminated = 2;
constexpr uint32_t hit_record_invalid = 0;
constexpr uint32_t hit_record_valid = 1;

constexpr uint32_t traversal_complete_miss = 0;
constexpr uint32_t traversal_complete_hit = 1;
constexpr uint32_t traversal_candidate_non_opaque_triangle = 2;
constexpr uint32_t traversal_candidate_procedural_aabb = 3;
constexpr uint32_t traversal_terminated = 4;

constexpr uint32_t bvh_magic = 0x56545254; // "VTRT"
constexpr uint32_t bvh_version = 1;
constexpr uint32_t geometry_triangle_list = 1;
constexpr uint32_t geometry_procedural_aabb_list = 2;

constexpr uint32_t as_magic = 0x53415456; // "VTAS"
constexpr uint32_t as_version_v1 = 1;
constexpr uint32_t as_version_v2 = 2;
constexpr uint32_t as_version = as_version_v1;
constexpr uint32_t as_type_blas = 1;
constexpr uint32_t as_type_tlas = 2;
constexpr uint32_t as_header_size = 0x50;

constexpr uint32_t as_header_magic = 0x00;
constexpr uint32_t as_header_version = 0x04;
constexpr uint32_t as_header_type = 0x08;
constexpr uint32_t as_header_root_node_ref = 0x10;
constexpr uint32_t as_header_node_region_offset = 0x18;
constexpr uint32_t as_header_node_region_size = 0x1c;
constexpr uint32_t as_header_root_aabb_min_x = 0x30;
constexpr uint32_t as_header_root_aabb_min_y = 0x34;
constexpr uint32_t as_header_root_aabb_min_z = 0x38;
constexpr uint32_t as_header_root_aabb_max_x = 0x3c;
constexpr uint32_t as_header_root_aabb_max_y = 0x40;
constexpr uint32_t as_header_root_aabb_max_z = 0x44;

constexpr uint32_t node_ref_type_mask = 0x7;
constexpr uint32_t node_ref_offset_mask = ~node_ref_type_mask;
constexpr uint32_t node_box4 = 1;
constexpr uint32_t node_triangle = 2;
constexpr uint32_t node_instance = 3;
constexpr uint32_t node_aabb = 4;
constexpr uint32_t invalid_node_ref = 0xffffffffu;

inline bool is_supported_vtas_version(uint32_t version)
{
  return version == as_version_v1 || version == as_version_v2;
}

constexpr uint32_t box4_child_ref = 0x00;
constexpr uint32_t box4_min_x = 0x10;
constexpr uint32_t box4_min_y = 0x20;
constexpr uint32_t box4_min_z = 0x30;
constexpr uint32_t box4_max_x = 0x40;
constexpr uint32_t box4_max_y = 0x50;
constexpr uint32_t box4_max_z = 0x60;
constexpr uint32_t box4_v2_parent_node_ref = 0x70;
constexpr uint32_t box4_v2_parent_child_index = 0x74;
constexpr uint32_t box4_v2_node_flags = 0x75;
constexpr uint32_t box4_v2_child_flags = 0x76;
constexpr uint32_t box4_v2_reserved = 0x7a;
constexpr uint32_t box4_v2_node_flag_root = 0x01;

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
constexpr uint32_t instance_v2_parent_edge = 0x1c;
constexpr uint32_t instance_object_to_world = 0x20;
constexpr uint32_t instance_world_to_object = 0x50;
constexpr uint32_t instance_v2_root_parent_edge = 0xffffffffu;
constexpr uint32_t instance_v2_parent_offset_mask = 0xffffff80u;
constexpr uint32_t instance_v2_parent_reserved_mask = 0x0000007cu;
constexpr uint32_t instance_v2_parent_child_mask = 0x00000003u;

constexpr uint32_t ray_flag_force_opaque = 1u << 0;
constexpr uint32_t ray_flag_force_non_opaque = 1u << 1;
constexpr uint32_t ray_flag_terminate_on_first_hit = 1u << 2;
constexpr uint32_t ray_flag_cull_back_facing_triangles = 1u << 4;
constexpr uint32_t ray_flag_cull_front_facing_triangles = 1u << 5;
constexpr uint32_t ray_flag_cull_opaque = 1u << 6;
constexpr uint32_t ray_flag_cull_non_opaque = 1u << 7;
constexpr uint32_t ray_flag_skip_triangles = 1u << 8;
constexpr uint32_t ray_flag_skip_aabbs = 1u << 9;

constexpr uint32_t instance_flag_triangle_facing_cull_disable = 1u << 0;
constexpr uint32_t instance_flag_triangle_flip_facing = 1u << 1;
constexpr uint32_t instance_flag_force_opaque = 1u << 2;
constexpr uint32_t instance_flag_force_non_opaque = 1u << 3;

constexpr uint32_t triangle_hit_kind_front_face = 0xfe;
constexpr uint32_t triangle_hit_kind_back_face = 0xff;

constexpr reg_t continuation_state_flags = 0;
constexpr reg_t continuation_stack_count = 1;
constexpr reg_t continuation_current_context = 2;
constexpr reg_t continuation_resume_index = 3;
constexpr uint32_t continuation_active = 1u << 0;
constexpr uint32_t continuation_overflow = 1u << 1;
constexpr uint32_t continuation_done = 1u << 2;

constexpr uint32_t continuation_context_count = 2;
constexpr uint32_t continuation_context_words = 12;
constexpr uint32_t continuation_stack_entry_count = 32;
constexpr uint32_t continuation_stack_entry_words = 4;
constexpr reg_t continuation_context_base_word = 4;
constexpr reg_t continuation_stack_base_word =
    continuation_context_base_word +
    continuation_context_count * continuation_context_words;

constexpr reg_t continuation_context_as_base_lo = 0;
constexpr reg_t continuation_context_as_base_hi = 1;
constexpr reg_t continuation_context_origin_x = 2;
constexpr reg_t continuation_context_origin_y = 3;
constexpr reg_t continuation_context_origin_z = 4;
constexpr reg_t continuation_context_direction_x = 5;
constexpr reg_t continuation_context_direction_y = 6;
constexpr reg_t continuation_context_direction_z = 7;
constexpr reg_t continuation_context_instance_id = 8;
constexpr reg_t continuation_context_instance_sbt_offset = 9;
constexpr reg_t continuation_context_instance_addr_lo = 10;
constexpr reg_t continuation_context_instance_addr_hi = 11;

constexpr reg_t continuation_stack_node_ref = 0;
constexpr reg_t continuation_stack_as_base_lo = 1;
constexpr reg_t continuation_stack_as_base_hi = 2;
constexpr reg_t continuation_stack_context_id = 3;

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

template <typename Memory>
inline uint32_t load_word(Memory &mem, reg_t slot, reg_t byte_base, reg_t word)
{
  return mem.load32(word_addr(slot, byte_base, word));
}

template <typename Memory>
inline void store_word(Memory &mem, reg_t slot, reg_t byte_base, reg_t word,
                       uint32_t value)
{
  mem.store32(word_addr(slot, byte_base, word), value);
}

struct Vec3 {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
};

struct Triangle {
  Vec3 v0;
  Vec3 v1;
  Vec3 v2;
  uint32_t primitive_id = 0;
  uint32_t instance_id = 0;
  uint32_t geometry_id = 0;
  uint32_t sbt_index = 0;
  uint32_t instance_sbt_record_offset = 0;
  uint32_t hit_kind = 0xfe;
  uint32_t opaque = 1;
  uint32_t instance_geometry_flags = 0;
  uint64_t primitive_addr = 0;
  uint64_t instance_addr = 0;
};

struct ProceduralAabb {
  Vec3 min;
  Vec3 max;
  uint32_t primitive_id = 0;
  uint32_t instance_id = 0;
  uint32_t geometry_id = 0;
  uint32_t sbt_index = 0;
  uint32_t instance_sbt_record_offset = 0;
  uint32_t hit_kind = 0xff;
  uint32_t opaque = 0;
  uint32_t instance_geometry_flags = 0;
  uint64_t primitive_addr = 0;
  uint64_t instance_addr = 0;
};

struct Hit {
  bool valid = false;
  float t = std::numeric_limits<float>::infinity();
  float bary_x = 0.0f;
  float bary_y = 0.0f;
  bool front_face = true;
  Triangle tri;
};

struct Ray {
  Vec3 origin;
  Vec3 direction;
  float tmin = 0.0f;
  float tmax = std::numeric_limits<float>::infinity();
  uint32_t flags = 0;
  uint32_t cull_mask = 0xff;
  uint32_t sbt_offset = 0;
  uint32_t sbt_stride = 0;
};

inline bool resolve_candidate_opacity(const Ray &ray,
                                      uint32_t instance_geometry_flags,
                                      bool geometry_opaque)
{
  if (ray.flags & ray_flag_force_non_opaque)
    return false;
  if (ray.flags & ray_flag_force_opaque)
    return true;
  if (instance_geometry_flags & instance_flag_force_non_opaque)
    return false;
  if (instance_geometry_flags & instance_flag_force_opaque)
    return true;
  return geometry_opaque;
}

inline bool candidate_opacity_is_culled(const Ray &ray, bool opaque)
{
  return opaque ? (ray.flags & ray_flag_cull_opaque) != 0
                : (ray.flags & ray_flag_cull_non_opaque) != 0;
}

inline bool prepare_triangle_candidate(const Ray &ray, Hit &candidate)
{
  if (ray.flags & ray_flag_skip_triangles)
    return false;

  const uint32_t instance_flags = candidate.tri.instance_geometry_flags;
  if (instance_flags & instance_flag_triangle_flip_facing)
    candidate.front_face = !candidate.front_face;
  candidate.tri.hit_kind = candidate.front_face
                               ? triangle_hit_kind_front_face
                               : triangle_hit_kind_back_face;

  if (!(instance_flags & instance_flag_triangle_facing_cull_disable)) {
    if (candidate.front_face &&
        (ray.flags & ray_flag_cull_front_facing_triangles))
      return false;
    if (!candidate.front_face &&
        (ray.flags & ray_flag_cull_back_facing_triangles))
      return false;
  }

  candidate.tri.opaque =
      resolve_candidate_opacity(ray, instance_flags,
                                candidate.tri.opaque != 0)
          ? 1
          : 0;
  return !candidate_opacity_is_culled(ray, candidate.tri.opaque != 0);
}

inline bool prepare_aabb_candidate(const Ray &ray, Hit &candidate)
{
  if (ray.flags & ray_flag_skip_aabbs)
    return false;

  candidate.tri.opaque =
      resolve_candidate_opacity(ray, candidate.tri.instance_geometry_flags,
                                candidate.tri.opaque != 0)
          ? 1
          : 0;
  return !candidate_opacity_is_culled(ray, candidate.tri.opaque != 0);
}

struct Scene {
  uint64_t primitive_addr = 0;
  uint32_t primitive_count = 0;
  uint32_t primitive_stride = 0;
  uint64_t hit_sbt_base = 0;
  uint32_t shader_group_handle_size = 0;
};

inline Vec3 sub(Vec3 a, Vec3 b)
{
  return {a.x - b.x, a.y - b.y, a.z - b.z};
}

inline Vec3 cross(Vec3 a, Vec3 b)
{
  return {
      a.y * b.z - a.z * b.y,
      a.z * b.x - a.x * b.z,
      a.x * b.y - a.y * b.x,
  };
}

inline float dot(Vec3 a, Vec3 b)
{
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline bool intersect_triangle(const Ray &ray, const Triangle &tri, Hit &hit)
{
  constexpr float eps = 1.0e-7f;
  const Vec3 e1 = sub(tri.v1, tri.v0);
  const Vec3 e2 = sub(tri.v2, tri.v0);
  const Vec3 p = cross(ray.direction, e2);
  const float det = dot(e1, p);
  if (!std::isfinite(det) || std::fabs(det) < eps)
    return false;

  const float inv_det = 1.0f / det;
  const Vec3 tvec = sub(ray.origin, tri.v0);
  const float u = dot(tvec, p) * inv_det;
  if (!std::isfinite(u) || u < 0.0f || u > 1.0f)
    return false;

  const Vec3 q = cross(tvec, e1);
  const float v = dot(ray.direction, q) * inv_det;
  if (!std::isfinite(v) || v < 0.0f || u + v > 1.0f)
    return false;

  const float t = dot(e2, q) * inv_det;
  /* A NaN bypasses ordinary ordered comparisons.  Treat it as a miss and
   * keep the upper interval exclusive, matching the committed-hit rule. */
  if (!std::isfinite(t) || t < ray.tmin || t >= ray.tmax || t >= hit.t)
    return false;

  hit.valid = true;
  hit.t = t;
  hit.bary_x = u;
  hit.bary_y = v;
  hit.front_face = det < 0.0f;
  hit.tri = tri;
  return true;
}

inline bool intersect_aabb(const Ray &ray, const ProceduralAabb &aabb,
                           float &hit_t)
{
  float tmin = ray.tmin;
  float tmax = ray.tmax;

  const float origin[3] = {ray.origin.x, ray.origin.y, ray.origin.z};
  const float direction[3] = {ray.direction.x, ray.direction.y,
                              ray.direction.z};
  const float bounds_min[3] = {aabb.min.x, aabb.min.y, aabb.min.z};
  const float bounds_max[3] = {aabb.max.x, aabb.max.y, aabb.max.z};

  for (int axis = 0; axis < 3; ++axis) {
    if (std::fabs(direction[axis]) < 1.0e-8f) {
      if (origin[axis] < bounds_min[axis] || origin[axis] > bounds_max[axis])
        return false;
      continue;
    }

    const float inv_dir = 1.0f / direction[axis];
    float t0 = (bounds_min[axis] - origin[axis]) * inv_dir;
    float t1 = (bounds_max[axis] - origin[axis]) * inv_dir;
    if (t0 > t1)
      std::swap(t0, t1);
    tmin = std::max(tmin, t0);
    tmax = std::min(tmax, t1);
    if (tmin > tmax)
      return false;
  }

  hit_t = tmin;
  return true;
}

template <typename Memory>
inline uint64_t load_u64(Memory &mem, reg_t addr)
{
  const uint64_t lo = mem.load32(addr);
  const uint64_t hi = mem.load32(addr + 4);
  return lo | (hi << 32);
}

template <typename Memory>
inline Vec3 load_vec3(Memory &mem, reg_t addr)
{
  return {
      bit_cast_f32(mem.load32(addr + 0)),
      bit_cast_f32(mem.load32(addr + 4)),
      bit_cast_f32(mem.load32(addr + 8)),
  };
}

template <typename Memory>
inline float load_f32(Memory &mem, reg_t addr)
{
  return bit_cast_f32(mem.load32(addr));
}

template <typename Memory>
inline Vec3 transform_instance_point(Memory &mem, reg_t matrix_addr,
                                     Vec3 value)
{
  return {
      load_f32(mem, matrix_addr + 0) * value.x +
          load_f32(mem, matrix_addr + 4) * value.y +
          load_f32(mem, matrix_addr + 8) * value.z +
          load_f32(mem, matrix_addr + 12),
      load_f32(mem, matrix_addr + 16) * value.x +
          load_f32(mem, matrix_addr + 20) * value.y +
          load_f32(mem, matrix_addr + 24) * value.z +
          load_f32(mem, matrix_addr + 28),
      load_f32(mem, matrix_addr + 32) * value.x +
          load_f32(mem, matrix_addr + 36) * value.y +
          load_f32(mem, matrix_addr + 40) * value.z +
          load_f32(mem, matrix_addr + 44),
  };
}

template <typename Memory>
inline Vec3 transform_instance_vector(Memory &mem, reg_t matrix_addr,
                                      Vec3 value)
{
  return {
      load_f32(mem, matrix_addr + 0) * value.x +
          load_f32(mem, matrix_addr + 4) * value.y +
          load_f32(mem, matrix_addr + 8) * value.z,
      load_f32(mem, matrix_addr + 16) * value.x +
          load_f32(mem, matrix_addr + 20) * value.y +
          load_f32(mem, matrix_addr + 24) * value.z,
      load_f32(mem, matrix_addr + 32) * value.x +
          load_f32(mem, matrix_addr + 36) * value.y +
          load_f32(mem, matrix_addr + 40) * value.z,
  };
}

template <typename Memory>
inline ProceduralAabb load_aabb(Memory &mem, reg_t addr)
{
  ProceduralAabb aabb;
  aabb.min = load_vec3(mem, addr + 0);
  aabb.max = load_vec3(mem, addr + 12);
  aabb.primitive_id = mem.load32(addr + 24);
  aabb.instance_id = mem.load32(addr + 28);
  aabb.geometry_id = mem.load32(addr + 32);
  aabb.sbt_index = mem.load32(addr + 36);
  aabb.hit_kind = mem.load32(addr + 40);
  aabb.primitive_addr = load_u64(mem, addr + 48);
  aabb.instance_addr = load_u64(mem, addr + 56);
  return aabb;
}

template <typename Memory>
inline Triangle load_triangle(Memory &mem, reg_t addr)
{
  Triangle tri;
  tri.v0 = load_vec3(mem, addr + 0);
  tri.v1 = load_vec3(mem, addr + 12);
  tri.v2 = load_vec3(mem, addr + 24);
  tri.primitive_id = mem.load32(addr + 36);
  tri.instance_id = mem.load32(addr + 40);
  tri.geometry_id = mem.load32(addr + 44);
  tri.sbt_index = mem.load32(addr + 48);
  tri.hit_kind = mem.load32(addr + 52);
  tri.opaque = mem.load32(addr + 56);
  tri.primitive_addr = load_u64(mem, addr + 64);
  tri.instance_addr = load_u64(mem, addr + 72);
  return tri;
}

inline Hit hit_from_aabb(const ProceduralAabb &aabb, float hit_t)
{
  Hit hit;
  hit.valid = true;
  hit.t = hit_t;
  hit.front_face = true;
  hit.tri.primitive_id = aabb.primitive_id;
  hit.tri.instance_id = aabb.instance_id;
  hit.tri.geometry_id = aabb.geometry_id;
  hit.tri.sbt_index = aabb.sbt_index;
  hit.tri.instance_sbt_record_offset = aabb.instance_sbt_record_offset;
  hit.tri.hit_kind = aabb.hit_kind;
  hit.tri.opaque = aabb.opaque;
  hit.tri.instance_geometry_flags = aabb.instance_geometry_flags;
  hit.tri.primitive_addr = aabb.primitive_addr;
  hit.tri.instance_addr = aabb.instance_addr;
  return hit;
}

template <typename Memory>
inline Ray load_ray(Memory &mem, reg_t slot)
{
  Ray ray;
  ray.flags = load_word(mem, slot, 0, slot_flags);
  ray.cull_mask = load_word(mem, slot, 0, slot_cull_mask);
  ray.sbt_offset = load_word(mem, slot, 0, slot_sbt_offset);
  ray.sbt_stride = load_word(mem, slot, 0, slot_sbt_stride);
  ray.origin = {
      bit_cast_f32(load_word(mem, slot, 0, slot_origin_x)),
      bit_cast_f32(load_word(mem, slot, 0, slot_origin_y)),
      bit_cast_f32(load_word(mem, slot, 0, slot_origin_z)),
  };
  ray.tmin = bit_cast_f32(load_word(mem, slot, 0, slot_tmin));
  ray.direction = {
      bit_cast_f32(load_word(mem, slot, 0, slot_direction_x)),
      bit_cast_f32(load_word(mem, slot, 0, slot_direction_y)),
      bit_cast_f32(load_word(mem, slot, 0, slot_direction_z)),
  };
  ray.tmax = bit_cast_f32(load_word(mem, slot, 0, slot_tmax));
  return ray;
}

template <typename Memory>
inline uint64_t load_accel_addr(Memory &mem, reg_t slot)
{
  const uint64_t lo = load_word(mem, slot, 0, slot_accel_lo);
  const uint64_t hi = load_word(mem, slot, 0, slot_accel_hi);
  return lo | (hi << 32);
}

template <typename Memory>
inline void clear_control(Memory &mem, reg_t slot)
{
  store_word(mem, slot, control_base, control_callback_decision,
             callback_pending);
}

template <typename Memory>
inline void write_hit_record(Memory &mem, reg_t slot, reg_t base,
                             const Scene &, const Hit &hit,
                             uint32_t status)
{
  const uint32_t sbt_index =
      hit.tri.instance_sbt_record_offset + hit.tri.sbt_index +
      load_word(mem, slot, 0, slot_sbt_offset);

  store_word(mem, slot, base, hit_record_status, status);
  store_word(mem, slot, base, hit_record_hit_t, bit_cast_u32(hit.t));
  store_word(mem, slot, base, hit_record_sbt_index, sbt_index);
  /* Compiler/global-consumer code owns SBT region byte addressing. */
  store_word(mem, slot, base, hit_record_shader_record_ptr_lo, 0);
  store_word(mem, slot, base, hit_record_shader_record_ptr_hi, 0);
  store_word(mem, slot, base, hit_record_primitive_id, hit.tri.primitive_id);
  store_word(mem, slot, base, hit_record_instance_id, hit.tri.instance_id);
  store_word(mem, slot, base, hit_record_geometry_id, hit.tri.geometry_id);
  store_word(mem, slot, base, hit_record_hit_kind, hit.tri.hit_kind);
  store_word(mem, slot, base, hit_record_barycentrics_x,
             bit_cast_u32(hit.bary_x));
  store_word(mem, slot, base, hit_record_barycentrics_y,
             bit_cast_u32(hit.bary_y));
  store_word(mem, slot, base, hit_record_primitive_addr_lo,
             uint32_t(hit.tri.primitive_addr));
  store_word(mem, slot, base, hit_record_primitive_addr_hi,
             uint32_t(hit.tri.primitive_addr >> 32));
  store_word(mem, slot, base, hit_record_instance_addr_lo,
             uint32_t(hit.tri.instance_addr));
  store_word(mem, slot, base, hit_record_instance_addr_hi,
             uint32_t(hit.tri.instance_addr >> 32));
  store_word(mem, slot, base, hit_record_front_face, hit.front_face ? 1 : 0);
  store_word(mem, slot, base, hit_record_opaque, hit.tri.opaque ? 1 : 0);
  store_word(mem, slot, base, hit_record_need_software_opacity_test,
             hit.tri.opaque ? 0 : 1);
  store_word(mem, slot, base, hit_record_instance_sbt_record_offset,
             hit.tri.instance_sbt_record_offset);
}

template <typename Memory>
inline void write_hit_attrib(Memory &mem, reg_t slot, const Hit &hit)
{
  /* Triangle barycentrics are canonical words 9/10 of each V4 hit record.
   * Procedural attributes begin at byte 276 and are written by shaders. */
  (void)mem;
  (void)slot;
  (void)hit;
}

template <typename Memory>
inline void copy_hit_record(Memory &mem, reg_t slot, reg_t dst_base,
                            reg_t src_base)
{
  for (reg_t word = hit_record_status; word <= hit_record_instance_sbt_record_offset;
       ++word)
    store_word(mem, slot, dst_base, word, load_word(mem, slot, src_base, word));
}

template <typename Memory>
inline bool committed_hit_valid(Memory &mem, reg_t slot)
{
  return load_word(mem, slot, committed_hit_record_base, hit_record_status) ==
         hit_record_valid;
}

template <typename Memory>
inline float current_tmax(Memory &mem, reg_t slot)
{
  if (committed_hit_valid(mem, slot))
    return bit_cast_f32(load_word(mem, slot, committed_hit_record_base,
                                  hit_record_hit_t));
  return bit_cast_f32(load_word(mem, slot, 0, slot_tmax));
}

#if 0 /* Superseded v1 PDS-resident traversal continuation. */
template <typename Memory>
inline void clear_continuation(Memory &mem, reg_t slot)
{
  const reg_t words = continuation_stack_base_word +
                      continuation_stack_entry_count *
                          continuation_stack_entry_words;
  for (reg_t word = 0; word < words; ++word)
    store_word(mem, slot, continuation_base, word, 0);
}

template <typename Memory>
inline bool continuation_is_active(Memory &mem, reg_t slot)
{
  return (load_word(mem, slot, continuation_base,
                    continuation_state_flags) & continuation_active) != 0;
}

template <typename Memory>
inline void mark_continuation_active(Memory &mem, reg_t slot)
{
  uint32_t flags = load_word(mem, slot, continuation_base,
                             continuation_state_flags);
  flags |= continuation_active;
  flags &= ~continuation_done;
  store_word(mem, slot, continuation_base, continuation_state_flags, flags);
}

template <typename Memory>
inline void mark_continuation_done(Memory &mem, reg_t slot)
{
  uint32_t flags = load_word(mem, slot, continuation_base,
                             continuation_state_flags);
  flags &= ~continuation_active;
  flags |= continuation_done;
  store_word(mem, slot, continuation_base, continuation_state_flags, flags);
  store_word(mem, slot, continuation_base, continuation_stack_count, 0);
  store_word(mem, slot, continuation_base, continuation_resume_index, 0);
}

template <typename Memory>
inline void mark_continuation_overflow(Memory &mem, reg_t slot)
{
  uint32_t flags = load_word(mem, slot, continuation_base,
                             continuation_state_flags);
  flags |= continuation_overflow;
  store_word(mem, slot, continuation_base, continuation_state_flags, flags);
}

template <typename Memory>
inline void commit_candidate_hit(Memory &mem, reg_t slot)
{
  copy_hit_record(mem, slot, committed_hit_record_base,
                  candidate_hit_record_base);
  const uint32_t hit_t =
      load_word(mem, slot, candidate_hit_record_base, hit_record_hit_t);
  const uint32_t sbt_index =
      load_word(mem, slot, candidate_hit_record_base, hit_record_sbt_index);
  store_word(mem, slot, 0, slot_hit_t, hit_t);
  store_word(mem, slot, 0, slot_sbt_index, sbt_index);
  store_word(mem, slot, 0, slot_status, rt_status_hit);
}

template <typename Memory>
inline void commit_hit(Memory &mem, reg_t slot, const Scene &scene,
                       const Hit &hit)
{
  write_hit_record(mem, slot, committed_hit_record_base, scene, hit,
                   hit_record_valid);
  write_hit_attrib(mem, slot, hit);
  store_word(mem, slot, 0, slot_hit_t, bit_cast_u32(hit.t));
  store_word(mem, slot, 0, slot_sbt_index,
             hit.tri.instance_sbt_record_offset + hit.tri.sbt_index +
                 load_word(mem, slot, 0, slot_sbt_offset));
}

template <typename Memory>
inline uint32_t finish_traversal(Memory &mem, reg_t slot)
{
  mark_continuation_done(mem, slot);
  store_word(mem, slot, control_base, control_done, 1);
  if (committed_hit_valid(mem, slot)) {
    store_word(mem, slot, 0, slot_status, rt_status_hit);
    return traversal_complete_hit;
  }

  store_word(mem, slot, 0, slot_status, rt_status_miss);
  store_word(mem, slot, committed_hit_record_base, hit_record_status, 0);
  return traversal_complete_miss;
}

template <typename Memory>
inline void pause_candidate(Memory &mem, reg_t slot, uint32_t status)
{
  mark_continuation_active(mem, slot);
  store_word(mem, slot, control_base, control_incomplete, 1);
  store_word(mem, slot, 0, slot_status, rt_status_hit);
  (void)status;
}

struct ContinuationContext {
  uint64_t as_base = 0;
  Ray ray;
  uint32_t instance_id = 0;
  uint32_t instance_sbt_offset = 0;
  uint64_t instance_addr = 0;
};

struct TraversalEntry {
  uint64_t as_base = 0;
  uint32_t node_ref = invalid_node_ref;
  Ray ray;
  uint32_t instance_id = 0;
  uint32_t instance_sbt_offset = 0;
  uint64_t instance_addr = 0;
};

inline reg_t continuation_context_word(uint32_t context_id, reg_t word)
{
  return continuation_context_base_word +
         reg_t(context_id) * continuation_context_words + word;
}

template <typename Memory>
inline void store_continuation_context(Memory &mem, reg_t slot,
                                       uint32_t context_id,
                                       const ContinuationContext &ctx)
{
  store_word(mem, slot, continuation_base,
             continuation_context_word(context_id,
                                       continuation_context_as_base_lo),
             uint32_t(ctx.as_base));
  store_word(mem, slot, continuation_base,
             continuation_context_word(context_id,
                                       continuation_context_as_base_hi),
             uint32_t(ctx.as_base >> 32));
  store_word(mem, slot, continuation_base,
             continuation_context_word(context_id,
                                       continuation_context_origin_x),
             bit_cast_u32(ctx.ray.origin.x));
  store_word(mem, slot, continuation_base,
             continuation_context_word(context_id,
                                       continuation_context_origin_y),
             bit_cast_u32(ctx.ray.origin.y));
  store_word(mem, slot, continuation_base,
             continuation_context_word(context_id,
                                       continuation_context_origin_z),
             bit_cast_u32(ctx.ray.origin.z));
  store_word(mem, slot, continuation_base,
             continuation_context_word(context_id,
                                       continuation_context_direction_x),
             bit_cast_u32(ctx.ray.direction.x));
  store_word(mem, slot, continuation_base,
             continuation_context_word(context_id,
                                       continuation_context_direction_y),
             bit_cast_u32(ctx.ray.direction.y));
  store_word(mem, slot, continuation_base,
             continuation_context_word(context_id,
                                       continuation_context_direction_z),
             bit_cast_u32(ctx.ray.direction.z));
  store_word(mem, slot, continuation_base,
             continuation_context_word(context_id,
                                       continuation_context_instance_id),
             ctx.instance_id);
  store_word(mem, slot, continuation_base,
             continuation_context_word(
                 context_id, continuation_context_instance_sbt_offset),
             ctx.instance_sbt_offset);
  store_word(mem, slot, continuation_base,
             continuation_context_word(
                 context_id, continuation_context_instance_addr_lo),
             uint32_t(ctx.instance_addr));
  store_word(mem, slot, continuation_base,
             continuation_context_word(
                 context_id, continuation_context_instance_addr_hi),
             uint32_t(ctx.instance_addr >> 32));
}

template <typename Memory>
inline ContinuationContext load_continuation_context(Memory &mem, reg_t slot,
                                                     uint32_t context_id)
{
  ContinuationContext ctx;
  uint64_t lo = load_word(mem, slot, continuation_base,
                          continuation_context_word(
                              context_id, continuation_context_as_base_lo));
  uint64_t hi = load_word(mem, slot, continuation_base,
                          continuation_context_word(
                              context_id, continuation_context_as_base_hi));
  ctx.as_base = lo | (hi << 32);
  ctx.ray = load_ray(mem, slot);
  ctx.ray.origin = {
      bit_cast_f32(load_word(mem, slot, continuation_base,
                             continuation_context_word(
                                 context_id, continuation_context_origin_x))),
      bit_cast_f32(load_word(mem, slot, continuation_base,
                             continuation_context_word(
                                 context_id, continuation_context_origin_y))),
      bit_cast_f32(load_word(mem, slot, continuation_base,
                             continuation_context_word(
                                 context_id, continuation_context_origin_z))),
  };
  ctx.ray.direction = {
      bit_cast_f32(load_word(mem, slot, continuation_base,
                             continuation_context_word(
                                 context_id,
                                 continuation_context_direction_x))),
      bit_cast_f32(load_word(mem, slot, continuation_base,
                             continuation_context_word(
                                 context_id,
                                 continuation_context_direction_y))),
      bit_cast_f32(load_word(mem, slot, continuation_base,
                             continuation_context_word(
                                 context_id,
                                 continuation_context_direction_z))),
  };
  ctx.instance_id = load_word(mem, slot, continuation_base,
                              continuation_context_word(
                                  context_id,
                                  continuation_context_instance_id));
  ctx.instance_sbt_offset =
      load_word(mem, slot, continuation_base,
                continuation_context_word(
                    context_id, continuation_context_instance_sbt_offset));
  lo = load_word(mem, slot, continuation_base,
                 continuation_context_word(
                     context_id, continuation_context_instance_addr_lo));
  hi = load_word(mem, slot, continuation_base,
                 continuation_context_word(
                     context_id, continuation_context_instance_addr_hi));
  ctx.instance_addr = lo | (hi << 32);
  return ctx;
}

inline reg_t continuation_stack_word(uint32_t index, reg_t word)
{
  return continuation_stack_base_word +
         reg_t(index) * continuation_stack_entry_words + word;
}

template <typename Memory>
inline bool push_continuation_entry(Memory &mem, reg_t slot, uint32_t &count,
                                    const TraversalEntry &entry,
                                    uint32_t context_id)
{
  if (count >= continuation_stack_entry_count) {
    mark_continuation_overflow(mem, slot);
    return false;
  }

  store_word(mem, slot, continuation_base,
             continuation_stack_word(count, continuation_stack_node_ref),
             entry.node_ref);
  store_word(mem, slot, continuation_base,
             continuation_stack_word(count, continuation_stack_as_base_lo),
             uint32_t(entry.as_base));
  store_word(mem, slot, continuation_base,
             continuation_stack_word(count, continuation_stack_as_base_hi),
             uint32_t(entry.as_base >> 32));
  store_word(mem, slot, continuation_base,
             continuation_stack_word(count, continuation_stack_context_id),
             context_id);
  ++count;
  store_word(mem, slot, continuation_base, continuation_stack_count, count);
  return true;
}

template <typename Memory>
inline bool pop_continuation_entry(Memory &mem, reg_t slot, uint32_t &count,
                                   TraversalEntry &entry,
                                   uint32_t &context_id)
{
  if (count == 0)
    return false;

  --count;
  entry.node_ref = load_word(mem, slot, continuation_base,
                             continuation_stack_word(
                                 count, continuation_stack_node_ref));
  uint64_t lo = load_word(mem, slot, continuation_base,
                          continuation_stack_word(
                              count, continuation_stack_as_base_lo));
  uint64_t hi = load_word(mem, slot, continuation_base,
                          continuation_stack_word(
                              count, continuation_stack_as_base_hi));
  entry.as_base = lo | (hi << 32);
  context_id = load_word(mem, slot, continuation_base,
                         continuation_stack_word(
                             count, continuation_stack_context_id));
  store_word(mem, slot, continuation_base, continuation_stack_count, count);
  return true;
}
#endif


inline uint32_t node_ref_type(uint32_t ref)
{
  return ref & node_ref_type_mask;
}

inline uint32_t node_ref_offset(uint32_t ref)
{
  return ref & node_ref_offset_mask;
}

template <typename Memory>
inline Vec3 load_vec3_indexed(Memory &mem, reg_t addr, uint32_t index)
{
  return load_vec3(mem, addr + reg_t(index) * 12);
}

inline uint32_t
vtas_geometry_sbt_contribution(uint32_t geometry_id,
                               uint32_t sbt_record_stride)
{
  /* This contribution is trace-time state and cannot be baked into the AS. */
  return geometry_id * sbt_record_stride;
}

template <typename Memory>
inline uint32_t load_node_ref(Memory &mem, uint64_t as_base)
{
  return mem.load32(as_base + as_header_root_node_ref);
}

template <typename Memory>
inline Triangle load_vtas_triangle(Memory &mem, uint64_t as_base,
                                   uint32_t node_ref,
                                   uint32_t instance_id,
                                   uint32_t instance_sbt_offset,
                                   uint64_t instance_addr,
                                   uint32_t sbt_record_stride)
{
  const reg_t addr = as_base + node_ref_offset(node_ref);
  Triangle tri;
  tri.v0 = load_vec3(mem, addr + triangle_v0);
  tri.v1 = load_vec3(mem, addr + triangle_v1);
  tri.v2 = load_vec3(mem, addr + triangle_v2);
  tri.primitive_id = mem.load32(addr + triangle_primitive_id);
  tri.instance_id = instance_id;
  tri.geometry_id = mem.load32(addr + triangle_geometry_id);
  tri.sbt_index =
      vtas_geometry_sbt_contribution(tri.geometry_id, sbt_record_stride);
  tri.instance_sbt_record_offset = instance_sbt_offset;
  tri.hit_kind = 0xfe;
  tri.opaque = mem.load32(addr + triangle_flags) & 0x1;
  if (instance_addr)
    tri.instance_geometry_flags =
        mem.load32(instance_addr + instance_flags);
  tri.primitive_addr = addr;
  tri.instance_addr = instance_addr;
  return tri;
}

template <typename Memory>
inline ProceduralAabb load_vtas_aabb(Memory &mem, uint64_t as_base,
                                     uint32_t node_ref, uint32_t instance_id,
                                     uint32_t instance_sbt_offset,
                                     uint64_t instance_addr,
                                     uint32_t sbt_record_stride)
{
  const reg_t addr = as_base + node_ref_offset(node_ref);
  ProceduralAabb aabb;
  aabb.min = load_vec3(mem, addr + aabb_min);
  aabb.max = load_vec3(mem, addr + aabb_max);
  aabb.primitive_id = mem.load32(addr + aabb_primitive_id);
  aabb.instance_id = instance_id;
  aabb.geometry_id = mem.load32(addr + aabb_geometry_id);
  aabb.sbt_index =
      vtas_geometry_sbt_contribution(aabb.geometry_id, sbt_record_stride);
  aabb.instance_sbt_record_offset = instance_sbt_offset;
  aabb.hit_kind = mem.load32(addr + aabb_hit_kind);
  aabb.opaque = mem.load32(addr + aabb_flags) & 0x1;
  if (instance_addr)
    aabb.instance_geometry_flags =
        mem.load32(instance_addr + instance_flags);
  aabb.primitive_addr = load_u64(mem, addr + aabb_primitive_addr_lo);
  aabb.instance_addr = instance_addr;
  return aabb;
}

template <typename Memory>
inline bool intersect_box4_child(Memory &mem, uint64_t as_base, uint32_t node_ref,
                                 uint32_t child, const Ray &ray,
                                 float &t_near)
{
  const reg_t addr = as_base + node_ref_offset(node_ref);
  float tmin = ray.tmin;
  float tmax = ray.tmax;
  const float origin[3] = {ray.origin.x, ray.origin.y, ray.origin.z};
  const float direction[3] = {ray.direction.x, ray.direction.y, ray.direction.z};
  const uint32_t min_off[3] = {box4_min_x, box4_min_y, box4_min_z};
  const uint32_t max_off[3] = {box4_max_x, box4_max_y, box4_max_z};

  for (uint32_t axis = 0; axis < 3; axis++) {
    const float bmin = bit_cast_f32(mem.load32(addr + min_off[axis] + child * 4));
    const float bmax = bit_cast_f32(mem.load32(addr + max_off[axis] + child * 4));
    if (std::fabs(direction[axis]) < 1.0e-8f) {
      if (origin[axis] < bmin || origin[axis] > bmax)
        return false;
      continue;
    }

    const float inv_dir = 1.0f / direction[axis];
    float t0 = (bmin - origin[axis]) * inv_dir;
    float t1 = (bmax - origin[axis]) * inv_dir;
    if (t0 > t1)
      std::swap(t0, t1);
    tmin = std::max(tmin, t0);
    tmax = std::min(tmax, t1);
    if (tmin > tmax)
      return false;
  }

  t_near = tmin;
  return true;
}

struct ChildHit {
  uint32_t ref = invalid_node_ref;
  float t_near = 0.0f;
};

inline void sort_child_hits(ChildHit (&hits)[4], uint32_t hit_count)
{
  for (uint32_t i = 1; i < hit_count; ++i) {
    const ChildHit hit = hits[i];
    uint32_t j = i;
    while (j > 0 && hit.t_near < hits[j - 1].t_near) {
      hits[j] = hits[j - 1];
      --j;
    }
    hits[j] = hit;
  }
}

/*
 * Legacy one-shot tracing path.
 *
 * Current vt.rt.traverse execution uses the private-RTcore context model below.
 * It keeps paused traversal state out of the shader-visible scratch ABI so an
 * any-hit/intersection callback can accept, ignore, or terminate before the
 * same walk resumes.
 *
 * The functions in this disabled block predate that continuation model.  They
 * either traverse to completion with a local stack or pause at a candidate
 * without enough scratch state to resume the same BVH/list walk.  Keep them as
 * debug/reference material only; do not wire them back into the instruction
 * path without redesigning their state semantics.
 */
#if 0
struct TraversalDebugCounters {
  uint32_t boxes = 0;
  uint32_t box_child_tests = 0;
  uint32_t box_child_hits = 0;
  uint32_t instances = 0;
  uint32_t triangles = 0;
  uint32_t triangle_hits = 0;
};

struct BruteTraceCounters {
  uint32_t tlas_boxes = 0;
  uint32_t blas_boxes = 0;
  uint32_t instances = 0;
  uint32_t triangles = 0;
  uint32_t triangle_hits = 0;
};

template <typename Memory>
inline void brute_trace_blas_node(Memory &mem, uint64_t blas_base,
                                  uint32_t node_ref, const Ray &ray,
                                  uint32_t instance_id,
                                  uint32_t instance_sbt_offset,
                                  uint64_t instance_addr, Hit &closest,
                                  BruteTraceCounters &counters)
{
  if (node_ref == invalid_node_ref)
    return;

  const uint32_t type = node_ref_type(node_ref);
  const reg_t node_addr = blas_base + node_ref_offset(node_ref);

  if (type == node_box4) {
    counters.blas_boxes++;
    for (uint32_t i = 0; i < 4; i++) {
      const uint32_t child = mem.load32(node_addr + box4_child_ref + i * 4);
      brute_trace_blas_node(mem, blas_base, child, ray, instance_id,
                            instance_sbt_offset, instance_addr, closest,
                            counters);
    }
    return;
  }

  if (type != node_triangle)
    return;

  counters.triangles++;
  const Triangle tri = load_vtas_triangle(mem, blas_base, node_ref, instance_id,
                                          instance_sbt_offset, instance_addr,
                                          ray.sbt_stride);
  Hit candidate = closest;
  if (intersect_triangle(ray, tri, candidate)) {
    counters.triangle_hits++;
    closest = candidate;
  }
}

template <typename Memory>
inline void brute_trace_tlas_node(Memory &mem, uint64_t tlas_base,
                                  uint32_t node_ref, const Ray &ray,
                                  Hit &closest,
                                  BruteTraceCounters &counters)
{
  if (node_ref == invalid_node_ref)
    return;

  const uint32_t type = node_ref_type(node_ref);
  const reg_t node_addr = tlas_base + node_ref_offset(node_ref);

  if (type == node_box4) {
    counters.tlas_boxes++;
    for (uint32_t i = 0; i < 4; i++) {
      const uint32_t child = mem.load32(node_addr + box4_child_ref + i * 4);
      brute_trace_tlas_node(mem, tlas_base, child, ray, closest, counters);
    }
    return;
  }

  if (type != node_instance)
    return;

  const uint32_t mask = mem.load32(node_addr + instance_mask);
  if ((ray.cull_mask & mask) == 0)
    return;

  const uint64_t blas = load_u64(mem, node_addr + instance_blas_addr_lo);
  if (!blas || mem.load32(blas + as_header_magic) != as_magic ||
      mem.load32(blas + as_header_type) != as_type_blas)
    return;

  counters.instances++;
  Ray object_ray = ray;
  object_ray.origin =
      transform_instance_point(mem, node_addr + instance_world_to_object,
                               ray.origin);
  object_ray.direction =
      transform_instance_vector(mem, node_addr + instance_world_to_object,
                                ray.direction);

  brute_trace_blas_node(mem, blas, load_node_ref(mem, blas), object_ray,
                        mem.load32(node_addr + instance_instance_id),
                        mem.load32(node_addr + instance_sbt_record_offset),
                        node_addr, closest, counters);
}

template <typename Memory>
inline Hit brute_trace_vtas(Memory &mem, uint64_t tlas_addr, const Ray &ray,
                            BruteTraceCounters &counters)
{
  Hit closest;
  brute_trace_tlas_node(mem, tlas_addr, load_node_ref(mem, tlas_addr), ray,
                        closest, counters);
  return closest;
}

template <typename Memory>
inline uint32_t trace_vtas(Memory &mem, reg_t slot, const Ray &ray,
                           uint64_t tlas_addr, bool skip_non_opaque)
{
  static uint32_t debug_count = 0;
  const bool debug_enabled =
      std::getenv("VENTUS_RT_DEBUG_TRAVERSAL") != nullptr ||
      std::getenv("VENTUS_RT_DEBUG_BRUTE") != nullptr;
  const bool brute_enabled =
      std::getenv("VENTUS_RT_DEBUG_BRUTE") != nullptr;
  const char *debug_start_env = std::getenv("VENTUS_RT_DEBUG_TRAVERSAL_START");
  const uint32_t debug_start =
      debug_start_env ? static_cast<uint32_t>(std::strtoul(debug_start_env, nullptr, 0)) : 0;
  const uint32_t debug_id = debug_enabled ? debug_count++ : 0;
  const bool debug = debug_enabled && debug_id >= debug_start &&
                     debug_id < debug_start + 8;
  TraversalDebugCounters debug_counters;

  if (mem.load32(tlas_addr + as_header_magic) != as_magic ||
      !is_supported_vtas_version(
        mem.load32(tlas_addr + as_header_version) & 0xffffu) ||
      mem.load32(tlas_addr + as_header_type) != as_type_tlas) {
    if (debug) {
      std::fprintf(stderr,
                   "ventus-rt: trace[%u] invalid TLAS addr=0x%llx magic=0x%x "
                   "version=0x%x type=%u\n",
                   debug_id, (unsigned long long)tlas_addr,
                   mem.load32(tlas_addr + as_header_magic),
                   mem.load32(tlas_addr + as_header_version),
                   mem.load32(tlas_addr + as_header_type));
    }
    store_word(mem, slot, 0, slot_status, rt_status_miss);
    store_word(mem, slot, control_base, control_done, 1);
    return traversal_complete_miss;
  }

  if (debug) {
    std::fprintf(stderr,
                 "ventus-rt: trace[%u] tlas=0x%llx root=0x%x ray_o=(%.6g %.6g %.6g) "
                 "ray_d=(%.6g %.6g %.6g) t=[%.6g %.6g] flags=0x%x mask=0x%x\n",
                 debug_id, (unsigned long long)tlas_addr,
                 load_node_ref(mem, tlas_addr), ray.origin.x, ray.origin.y,
                 ray.origin.z, ray.direction.x, ray.direction.y,
                 ray.direction.z, ray.tmin, ray.tmax, ray.flags,
                 ray.cull_mask);
  }

  Scene scene;
  scene.hit_sbt_base = 0;
  scene.shader_group_handle_size = 32;

  Hit closest;
  std::vector<TraversalEntry> stack;
  stack.push_back({tlas_addr, load_node_ref(mem, tlas_addr), ray, 0, 0, 0});

  while (!stack.empty()) {
    TraversalEntry entry = stack.back();
    stack.pop_back();
    if (entry.node_ref == invalid_node_ref)
      continue;

    const uint32_t type = node_ref_type(entry.node_ref);
    const reg_t node_addr = entry.as_base + node_ref_offset(entry.node_ref);

    if (type == node_box4) {
      debug_counters.boxes++;
      ChildHit hits[4];
      uint32_t hit_count = 0;
      for (uint32_t i = 0; i < 4; i++) {
        uint32_t child = mem.load32(node_addr + box4_child_ref + i * 4);
        if (child == invalid_node_ref)
          continue;
        float t_near = 0.0f;
        debug_counters.box_child_tests++;
        const bool child_hit = intersect_box4_child(
            mem, entry.as_base, entry.node_ref, i, entry.ray, t_near);
        if (debug && debug_counters.boxes == 1) {
          std::fprintf(stderr,
                       "ventus-rt: trace[%u] root child[%u] ref=0x%x "
                       "min=(%.6g %.6g %.6g) max=(%.6g %.6g %.6g) hit=%u "
                       "t=%.6g\n",
                       debug_id, i, child,
                       load_f32(mem, node_addr + box4_min_x + i * 4),
                       load_f32(mem, node_addr + box4_min_y + i * 4),
                       load_f32(mem, node_addr + box4_min_z + i * 4),
                       load_f32(mem, node_addr + box4_max_x + i * 4),
                       load_f32(mem, node_addr + box4_max_y + i * 4),
                       load_f32(mem, node_addr + box4_max_z + i * 4),
                       child_hit ? 1u : 0u, t_near);
        }
        if (child_hit) {
          debug_counters.box_child_hits++;
          hits[hit_count++] = {child, t_near};
        }
      }
      sort_child_hits(hits, hit_count);
      for (uint32_t i = hit_count; i > 0; i--)
        stack.push_back({entry.as_base, hits[i - 1].ref, entry.ray,
                         entry.instance_id, entry.instance_sbt_offset,
                         entry.instance_addr});
      continue;
    }

    if (type == node_instance) {
      debug_counters.instances++;
      const uint32_t mask = mem.load32(node_addr + instance_mask);
      if ((ray.cull_mask & mask) == 0)
        continue;

      const uint64_t blas = load_u64(mem, node_addr + instance_blas_addr_lo);
      if (!blas || mem.load32(blas + as_header_magic) != as_magic ||
          mem.load32(blas + as_header_type) != as_type_blas)
        continue;

      Ray object_ray = entry.ray;
      object_ray.origin = transform_instance_point(
          mem, node_addr + instance_world_to_object, entry.ray.origin);
      object_ray.direction = transform_instance_vector(
          mem, node_addr + instance_world_to_object, entry.ray.direction);

      stack.push_back({blas, load_node_ref(mem, blas), object_ray,
                       mem.load32(node_addr + instance_instance_id),
                       mem.load32(node_addr + instance_sbt_record_offset),
                       node_addr});
      continue;
    }

    if (type == node_triangle) {
      debug_counters.triangles++;
      Triangle tri = load_vtas_triangle(mem, entry.as_base, entry.node_ref,
                                        entry.instance_id,
                                        entry.instance_sbt_offset,
                                        entry.instance_addr,
                                        entry.ray.sbt_stride);
      Hit candidate = closest;
      if (!intersect_triangle(entry.ray, tri, candidate))
        continue;
      debug_counters.triangle_hits++;

      const bool force_opaque = (ray.flags & ray_flag_force_opaque) != 0;
      const bool force_non_opaque = (ray.flags & ray_flag_force_non_opaque) != 0;
      const bool opaque = force_opaque || (!force_non_opaque && tri.opaque != 0);
      if (!opaque) {
        if (skip_non_opaque)
          continue;

        write_hit_record(mem, slot, candidate_hit_record_base, scene, candidate,
                         rt_status_hit);
        write_hit_attrib(mem, slot, candidate);
        store_word(mem, slot, control_base, control_incomplete, 1);
        store_word(mem, slot, 0, slot_status, rt_status_hit);
        return traversal_candidate_non_opaque_triangle;
      }

      closest = candidate;
    }
  }

  if (debug && brute_enabled) {
    BruteTraceCounters brute_counters;
    Hit brute = brute_trace_vtas(mem, tlas_addr, ray, brute_counters);
    const bool mismatch =
        closest.valid != brute.valid ||
        (closest.valid && brute.valid &&
         (std::fabs(closest.t - brute.t) > 1.0e-4f ||
          closest.tri.primitive_id != brute.tri.primitive_id));
    std::fprintf(stderr,
                 "ventus-rt: trace[%u] brute_compare mismatch=%u "
                 "bvh_hit=%u brute_hit=%u bvh_t=%.6g brute_t=%.6g "
                 "bvh_prim=%u brute_prim=%u brute={tlas_boxes:%u blas_boxes:%u "
                 "instances:%u triangles:%u tri_hits:%u}\n",
                 debug_id, mismatch ? 1u : 0u, closest.valid ? 1u : 0u,
                 brute.valid ? 1u : 0u, closest.valid ? closest.t : -1.0f,
                 brute.valid ? brute.t : -1.0f,
                 closest.valid ? closest.tri.primitive_id : 0xffffffffu,
                 brute.valid ? brute.tri.primitive_id : 0xffffffffu,
                 brute_counters.tlas_boxes, brute_counters.blas_boxes,
                 brute_counters.instances, brute_counters.triangles,
                 brute_counters.triangle_hits);
  }

  if (!closest.valid) {
    if (debug) {
      std::fprintf(stderr,
                   "ventus-rt: trace[%u] miss boxes=%u box_tests=%u "
                   "box_hits=%u instances=%u triangles=%u tri_hits=%u\n",
                   debug_id, debug_counters.boxes,
                   debug_counters.box_child_tests,
                   debug_counters.box_child_hits, debug_counters.instances,
                   debug_counters.triangles, debug_counters.triangle_hits);
    }
    store_word(mem, slot, 0, slot_status, rt_status_miss);
    store_word(mem, slot, committed_hit_record_base, hit_record_status, 0);
    store_word(mem, slot, control_base, control_done, 1);
    return traversal_complete_miss;
  }

  if (debug) {
    std::fprintf(stderr,
                 "ventus-rt: trace[%u] hit t=%.6g boxes=%u box_tests=%u "
                 "box_hits=%u instances=%u triangles=%u tri_hits=%u prim=%u\n",
                 debug_id, closest.t, debug_counters.boxes,
                 debug_counters.box_child_tests, debug_counters.box_child_hits,
                 debug_counters.instances, debug_counters.triangles,
                 debug_counters.triangle_hits, closest.tri.primitive_id);
  }

  write_hit_record(mem, slot, committed_hit_record_base, scene, closest,
                   rt_status_hit);
  write_hit_attrib(mem, slot, closest);
  store_word(mem, slot, 0, slot_hit_t, bit_cast_u32(closest.t));
  store_word(mem, slot, 0, slot_sbt_index, closest.tri.sbt_index);
  store_word(mem, slot, 0, slot_status, rt_status_hit);
  store_word(mem, slot, control_base, control_done, 1);
  return traversal_complete_hit;
}

template <typename Memory>
inline uint32_t trace_triangle_list(Memory &mem, reg_t slot, const Ray &ray,
                                    const Scene &scene,
                                    bool skip_non_opaque)
{
  Hit closest;
  for (uint32_t i = 0; i < scene.primitive_count; ++i) {
    const Triangle tri =
        load_triangle(mem, scene.primitive_addr + i * scene.primitive_stride);
    Hit candidate = closest;
    if (!intersect_triangle(ray, tri, candidate))
      continue;

    const bool force_opaque = (ray.flags & ray_flag_force_opaque) != 0;
    const bool force_non_opaque = (ray.flags & ray_flag_force_non_opaque) != 0;
    const bool opaque = force_opaque || (!force_non_opaque && tri.opaque != 0);
    if (!opaque) {
      if (skip_non_opaque)
        continue;

      write_hit_record(mem, slot, candidate_hit_record_base, scene, candidate,
                       rt_status_hit);
      write_hit_attrib(mem, slot, candidate);
      store_word(mem, slot, control_base, control_incomplete, 1);
      store_word(mem, slot, 0, slot_status, rt_status_hit);
      return traversal_candidate_non_opaque_triangle;
    }

    closest = candidate;
  }

  if (!closest.valid) {
    store_word(mem, slot, 0, slot_status, rt_status_miss);
    store_word(mem, slot, committed_hit_record_base, hit_record_status, 0);
    store_word(mem, slot, control_base, control_done, 1);
    return traversal_complete_miss;
  }

  write_hit_record(mem, slot, committed_hit_record_base, scene, closest,
                   rt_status_hit);
  write_hit_attrib(mem, slot, closest);
  store_word(mem, slot, 0, slot_hit_t, bit_cast_u32(closest.t));
  store_word(mem, slot, 0, slot_sbt_index, closest.tri.sbt_index);
  store_word(mem, slot, 0, slot_status, rt_status_hit);
  store_word(mem, slot, control_base, control_done, 1);
  return traversal_complete_hit;
}

template <typename Memory>
inline uint32_t trace_aabb_list(Memory &mem, reg_t slot, const Ray &ray,
                                const Scene &scene, bool skip_candidate)
{
  for (uint32_t i = 0; i < scene.primitive_count; ++i) {
    const ProceduralAabb aabb =
        load_aabb(mem, scene.primitive_addr + i * scene.primitive_stride);
    float hit_t = 0.0f;
    if (!intersect_aabb(ray, aabb, hit_t))
      continue;
    if (skip_candidate)
      continue;

    const Hit candidate = hit_from_aabb(aabb, hit_t);
    write_hit_record(mem, slot, candidate_hit_record_base, scene, candidate,
                     rt_status_hit);
    write_hit_attrib(mem, slot, candidate);
    store_word(mem, slot, control_base, control_incomplete, 1);
    store_word(mem, slot, 0, slot_status, rt_status_hit);
    return traversal_candidate_procedural_aabb;
  }

  store_word(mem, slot, 0, slot_status, rt_status_miss);
  store_word(mem, slot, committed_hit_record_base, hit_record_status, 0);
  store_word(mem, slot, control_base, control_done, 1);
  return traversal_complete_miss;
}
#endif

#if 0 /* Superseded v1 PDS-resident traversal paths. */
template <typename Memory>
inline uint32_t trace_triangle_list_cont(Memory &mem, reg_t slot,
                                         const Ray &ray, const Scene &scene)
{
  uint32_t start = 0;
  if (continuation_is_active(mem, slot)) {
    start = load_word(mem, slot, continuation_base,
                      continuation_resume_index);
  } else {
    clear_continuation(mem, slot);
    mark_continuation_active(mem, slot);
  }

  for (uint32_t i = start; i < scene.primitive_count; ++i) {
    Ray current_ray = ray;
    current_ray.tmax = current_tmax(mem, slot);
    const Triangle tri =
        load_triangle(mem, scene.primitive_addr + i * scene.primitive_stride);
    Hit candidate;
    candidate.t = current_ray.tmax;
    if (!intersect_triangle(current_ray, tri, candidate))
      continue;

    const bool force_opaque =
        (current_ray.flags & ray_flag_force_opaque) != 0;
    const bool force_non_opaque =
        (current_ray.flags & ray_flag_force_non_opaque) != 0;
    const bool opaque = force_opaque || (!force_non_opaque && tri.opaque != 0);
    if (!opaque) {
      write_hit_record(mem, slot, candidate_hit_record_base, scene, candidate,
                       rt_status_hit);
      write_hit_attrib(mem, slot, candidate);
      store_word(mem, slot, continuation_base, continuation_resume_index,
                 i + 1);
      pause_candidate(mem, slot, traversal_candidate_non_opaque_triangle);
      return traversal_candidate_non_opaque_triangle;
    }

    commit_hit(mem, slot, scene, candidate);
    if (current_ray.flags & ray_flag_terminate_on_first_hit)
      return finish_traversal(mem, slot);
  }

  return finish_traversal(mem, slot);
}

template <typename Memory>
inline uint32_t trace_aabb_list_cont(Memory &mem, reg_t slot, const Ray &ray,
                                     const Scene &scene)
{
  uint32_t start = 0;
  if (continuation_is_active(mem, slot)) {
    start = load_word(mem, slot, continuation_base,
                      continuation_resume_index);
  } else {
    clear_continuation(mem, slot);
    mark_continuation_active(mem, slot);
  }

  for (uint32_t i = start; i < scene.primitive_count; ++i) {
    Ray current_ray = ray;
    current_ray.tmax = current_tmax(mem, slot);
    const ProceduralAabb aabb =
        load_aabb(mem, scene.primitive_addr + i * scene.primitive_stride);
    float hit_t = 0.0f;
    if (!intersect_aabb(current_ray, aabb, hit_t))
      continue;

    const Hit candidate = hit_from_aabb(aabb, hit_t);
    write_hit_record(mem, slot, candidate_hit_record_base, scene, candidate,
                     rt_status_hit);
    write_hit_attrib(mem, slot, candidate);
    store_word(mem, slot, continuation_base, continuation_resume_index, i + 1);
    pause_candidate(mem, slot, traversal_candidate_procedural_aabb);
    return traversal_candidate_procedural_aabb;
  }

  return finish_traversal(mem, slot);
}

template <typename Memory>
inline uint32_t trace_vtas_cont(Memory &mem, reg_t slot, const Ray &ray,
                                uint64_t tlas_addr)
{
  if (mem.load32(tlas_addr + as_header_magic) != as_magic ||
      !is_supported_vtas_version(
        mem.load32(tlas_addr + as_header_version) & 0xffffu) ||
      mem.load32(tlas_addr + as_header_type) != as_type_tlas)
    return finish_traversal(mem, slot);

  Scene scene;
  scene.hit_sbt_base = 0;
  scene.shader_group_handle_size = 32;

  uint32_t stack_count =
      load_word(mem, slot, continuation_base, continuation_stack_count);
  if (!continuation_is_active(mem, slot)) {
    clear_continuation(mem, slot);
    ContinuationContext ctx;
    ctx.as_base = tlas_addr;
    ctx.ray = ray;
    store_continuation_context(mem, slot, 0, ctx);
    TraversalEntry root = {tlas_addr, load_node_ref(mem, tlas_addr),
                           ray, 0, 0, 0};
    stack_count = 0;
    if (!push_continuation_entry(mem, slot, stack_count, root, 0))
      return finish_traversal(mem, slot);
    mark_continuation_active(mem, slot);
  }

  while (true) {
    TraversalEntry entry;
    uint32_t context_id = 0;
    if (!pop_continuation_entry(mem, slot, stack_count, entry, context_id))
      return finish_traversal(mem, slot);

    if (context_id >= continuation_context_count ||
        entry.node_ref == invalid_node_ref)
      continue;

    ContinuationContext ctx =
        load_continuation_context(mem, slot, context_id);
    entry.ray = ctx.ray;
    entry.ray.tmax = current_tmax(mem, slot);
    entry.instance_id = ctx.instance_id;
    entry.instance_sbt_offset = ctx.instance_sbt_offset;
    entry.instance_addr = ctx.instance_addr;

    const uint32_t type = node_ref_type(entry.node_ref);
    const reg_t node_addr = entry.as_base + node_ref_offset(entry.node_ref);

    if (type == node_box4) {
      ChildHit hits[4];
      uint32_t hit_count = 0;
      for (uint32_t i = 0; i < 4; i++) {
        uint32_t child = mem.load32(node_addr + box4_child_ref + i * 4);
        if (child == invalid_node_ref)
          continue;
        float t_near = 0.0f;
        if (intersect_box4_child(mem, entry.as_base, entry.node_ref, i,
                                 entry.ray, t_near))
          hits[hit_count++] = {child, t_near};
      }

      sort_child_hits(hits, hit_count);
      for (uint32_t i = hit_count; i > 0; i--) {
        TraversalEntry child = {entry.as_base, hits[i - 1].ref, entry.ray,
                                entry.instance_id,
                                entry.instance_sbt_offset,
                                entry.instance_addr};
        if (!push_continuation_entry(mem, slot, stack_count, child,
                                     context_id))
          return finish_traversal(mem, slot);
      }
      continue;
    }

    if (type == node_instance) {
      const uint32_t mask = mem.load32(node_addr + instance_mask);
      if ((entry.ray.cull_mask & mask) == 0)
        continue;

      const uint64_t blas = load_u64(mem, node_addr + instance_blas_addr_lo);
      if (!blas || mem.load32(blas + as_header_magic) != as_magic ||
          mem.load32(blas + as_header_type) != as_type_blas)
        continue;

      Ray object_ray = entry.ray;
      object_ray.origin = transform_instance_point(
          mem, node_addr + instance_world_to_object, entry.ray.origin);
      object_ray.direction = transform_instance_vector(
          mem, node_addr + instance_world_to_object, entry.ray.direction);

      ContinuationContext child_ctx;
      child_ctx.as_base = blas;
      child_ctx.ray = object_ray;
      child_ctx.instance_id = mem.load32(node_addr + instance_instance_id);
      child_ctx.instance_sbt_offset =
          mem.load32(node_addr + instance_sbt_record_offset);
      child_ctx.instance_addr = node_addr;
      store_continuation_context(mem, slot, 1, child_ctx);

      TraversalEntry root = {blas, load_node_ref(mem, blas), object_ray,
                             child_ctx.instance_id,
                             child_ctx.instance_sbt_offset,
                             child_ctx.instance_addr};
      if (!push_continuation_entry(mem, slot, stack_count, root, 1))
        return finish_traversal(mem, slot);
      continue;
    }

    if (type != node_triangle)
      continue;

    Triangle tri = load_vtas_triangle(mem, entry.as_base, entry.node_ref,
                                      entry.instance_id,
                                      entry.instance_sbt_offset,
                                      entry.instance_addr,
                                      entry.ray.sbt_stride);
    Hit candidate;
    candidate.t = current_tmax(mem, slot);
    if (!intersect_triangle(entry.ray, tri, candidate))
      continue;

    const bool force_opaque = (entry.ray.flags & ray_flag_force_opaque) != 0;
    const bool force_non_opaque =
        (entry.ray.flags & ray_flag_force_non_opaque) != 0;
    const bool opaque = force_opaque || (!force_non_opaque && tri.opaque != 0);
    if (!opaque) {
      write_hit_record(mem, slot, candidate_hit_record_base, scene, candidate,
                       rt_status_hit);
      write_hit_attrib(mem, slot, candidate);
      pause_candidate(mem, slot, traversal_candidate_non_opaque_triangle);
      return traversal_candidate_non_opaque_triangle;
    }

    commit_hit(mem, slot, scene, candidate);
    if (entry.ray.flags & ray_flag_terminate_on_first_hit)
      return finish_traversal(mem, slot);
  }
}

template <typename Memory>
inline uint32_t traverse(Memory &mem, reg_t slot)
{
  const bool resuming = continuation_is_active(mem, slot);
  if (!resuming) {
    clear_continuation(mem, slot);
    store_word(mem, slot, committed_hit_record_base, hit_record_status, 0);
    clear_control(mem, slot);
  }

  const bool accept_hit =
      resuming && load_word(mem, slot, control_base, control_accept_hit) != 0;
  const bool terminate_ray =
      resuming && load_word(mem, slot, control_base, control_terminate_ray) != 0;

  if (accept_hit) {
    const float hit_t =
        bit_cast_f32(load_word(mem, slot, candidate_hit_record_base,
                               hit_record_hit_t));
    const Ray ray = load_ray(mem, slot);
    if (hit_t >= ray.tmin && hit_t < current_tmax(mem, slot))
      commit_candidate_hit(mem, slot);
  }

  if (terminate_ray) {
    clear_control(mem, slot);
    store_word(mem, slot, control_base, control_done, 1);
    mark_continuation_done(mem, slot);
    if (committed_hit_valid(mem, slot))
      return traversal_complete_hit;
    store_word(mem, slot, 0, slot_status, rt_status_done);
    return traversal_terminated;
  }

  clear_control(mem, slot);
  const Ray ray = load_ray(mem, slot);
  const uint64_t accel_addr = load_accel_addr(mem, slot);
  if (accel_addr == 0)
    return finish_traversal(mem, slot);

  const uint32_t magic = mem.load32(accel_addr + 0);
  if (magic == as_magic)
    return trace_vtas_cont(mem, slot, ray, accel_addr);

  const uint32_t version = mem.load32(accel_addr + 4);
  const uint32_t geometry_type = mem.load32(accel_addr + 8);
  Scene scene;
  scene.primitive_count = mem.load32(accel_addr + 12);
  scene.primitive_addr = load_u64(mem, accel_addr + 16);
  scene.primitive_stride = mem.load32(accel_addr + 24);
  scene.hit_sbt_base = load_u64(mem, accel_addr + 28);
  scene.shader_group_handle_size = mem.load32(accel_addr + 36);

  if (magic != bvh_magic || version != bvh_version ||
      scene.primitive_count == 0 ||
      scene.primitive_addr == 0 || scene.primitive_stride == 0)
    return finish_traversal(mem, slot);

  if (geometry_type == geometry_triangle_list)
    return trace_triangle_list_cont(mem, slot, ray, scene);

  if (geometry_type == geometry_procedural_aabb_list)
    return trace_aabb_list_cont(mem, slot, ray, scene);

  return finish_traversal(mem, slot);
}

template <typename Memory>
inline void release(Memory &mem, reg_t slot)
{
  clear_control(mem, slot);
  clear_continuation(mem, slot);
}
#endif

struct TraversalEntry {
  uint64_t as_base = 0;
  uint32_t node_ref = invalid_node_ref;
  Ray ray;
  uint32_t instance_id = 0;
  uint32_t instance_sbt_offset = 0;
  uint64_t instance_addr = 0;
};

template <typename Memory>
inline void commit_hit(Memory &mem, reg_t slot, const Scene &scene,
                       const Hit &hit)
{
  write_hit_record(mem, slot, committed_hit_record_base, scene, hit,
                   hit_record_valid);
  write_hit_attrib(mem, slot, hit);
  store_word(mem, slot, 0, slot_hit_t, bit_cast_u32(hit.t));
  store_word(mem, slot, 0, slot_sbt_index,
             hit.tri.instance_sbt_record_offset + hit.tri.sbt_index +
                 load_word(mem, slot, 0, slot_sbt_offset));
}

enum class RtPrivateTraversalKind : uint8_t {
  triangle_list,
  aabb_list,
  vtas,
};

struct RtPrivateContext {
  RtPrivateTraversalKind kind;
  Ray ray;
  Scene scene;
  uint32_t next_primitive = 0;
  uint16_t vtas_version = 0;
  std::vector<TraversalEntry> stack;
  vtas_v2::FixedFrontier v2_frontier;
  uint64_t v2_tlas_base = 0;
  uint64_t v2_active_as_base = 0;
  Ray v2_active_ray;
  uint32_t v2_instance_id = 0;
  uint32_t v2_instance_sbt_offset = 0;
  uint64_t v2_instance_addr = 0;
  uint32_t v2_replay_count = 0;
  uint32_t v2_overflow_count = 0;
  uint32_t v2_parent_bailout_count = 0;
  uint32_t v2_cross_count = 0;
  uint32_t v2_tlas_node_count = 0;
  uint64_t v2_operation_count = 0;
  uint64_t v2_operation_budget = 0;
};

template <typename Memory>
inline auto rt_context_key(Memory &mem, reg_t slot, int)
    -> decltype(mem.rt_context_key(slot))
{
  return mem.rt_context_key(slot);
}

template <typename Memory>
inline uint64_t rt_context_key(Memory &mem, reg_t slot, long)
{
  return uint64_t(reinterpret_cast<uintptr_t>(&mem)) ^ (uint64_t(slot) << 1);
}

class RtPrivateState {
public:
  using ContextMap = std::unordered_map<uint64_t, RtPrivateContext>;

  ContextMap &contexts() { return contexts_; }
  const ContextMap &contexts() const { return contexts_; }
  size_t context_count() const { return contexts_.size(); }
  void reset() { contexts_.clear(); }

private:
  // Production ownership is one RtCoreModel instance per logical SM.  The
  // standalone two-argument helpers below exist only for header-level tests.
  ContextMap contexts_;
};

template <typename Memory>
inline void commit_private_candidate(Memory &mem, reg_t slot)
{
  const float hit_t = bit_cast_f32(load_word(mem, slot,
                                             candidate_hit_record_base,
                                             hit_record_hit_t));
  const Ray ray = load_ray(mem, slot);
  if (hit_t >= ray.tmin && hit_t < current_tmax(mem, slot)) {
    copy_hit_record(mem, slot, committed_hit_record_base,
                    candidate_hit_record_base);
    store_word(mem, slot, 0, slot_hit_t, bit_cast_u32(hit_t));
    store_word(mem, slot, 0, slot_sbt_index,
               load_word(mem, slot, candidate_hit_record_base,
                         hit_record_sbt_index));
  }
}

template <typename Memory>
inline bool private_candidate_is_before_tmax(Memory &mem, reg_t slot,
                                             float hit_t)
{
  const Ray ray = load_ray(mem, slot);
  return std::isfinite(hit_t) && hit_t >= ray.tmin &&
         hit_t < current_tmax(mem, slot);
}

template <typename Memory>
inline uint32_t finish_private_traversal(Memory &mem, reg_t slot,
                                         uint64_t key,
                                         RtPrivateState &private_state)
{
  const bool hit = committed_hit_valid(mem, slot);
  if (!hit)
    store_word(mem, slot, committed_hit_record_base, hit_record_status,
               hit_record_invalid);
  private_state.contexts().erase(key);
  return hit ? traversal_complete_hit : traversal_complete_miss;
}

template <typename Memory>
inline uint32_t pause_private_candidate(Memory &mem, reg_t slot,
                                        const Scene &scene,
                                        const Hit &candidate,
                                        uint32_t status)
{
  write_hit_record(mem, slot, candidate_hit_record_base, scene, candidate,
                   hit_record_valid);
  write_hit_attrib(mem, slot, candidate);
  return status;
}

template <typename Memory>
inline uint32_t trace_private_triangle_list(Memory &mem, reg_t slot,
                                            uint64_t key,
                                            RtPrivateState &private_state)
{
  RtPrivateContext &ctx = private_state.contexts().at(key);
  for (uint32_t i = ctx.next_primitive; i < ctx.scene.primitive_count; ++i) {
    ctx.next_primitive = i + 1;
    Ray ray = ctx.ray;
    ray.tmax = current_tmax(mem, slot);
    const Triangle tri =
        load_triangle(mem, ctx.scene.primitive_addr + i * ctx.scene.primitive_stride);
    Hit candidate;
    candidate.t = ray.tmax;
    if (!intersect_triangle(ray, tri, candidate) ||
        !prepare_triangle_candidate(ray, candidate))
      continue;

    if (!candidate.tri.opaque)
      return pause_private_candidate(mem, slot, ctx.scene, candidate,
                                     traversal_candidate_non_opaque_triangle);

    commit_hit(mem, slot, ctx.scene, candidate);
    if (ray.flags & ray_flag_terminate_on_first_hit)
      return finish_private_traversal(mem, slot, key, private_state);
  }
  return finish_private_traversal(mem, slot, key, private_state);
}

template <typename Memory>
inline uint32_t trace_private_aabb_list(Memory &mem, reg_t slot,
                                        uint64_t key,
                                        RtPrivateState &private_state)
{
  RtPrivateContext &ctx = private_state.contexts().at(key);
  for (uint32_t i = ctx.next_primitive; i < ctx.scene.primitive_count; ++i) {
    ctx.next_primitive = i + 1;
    Ray ray = ctx.ray;
    ray.tmax = current_tmax(mem, slot);
    const ProceduralAabb aabb =
        load_aabb(mem, ctx.scene.primitive_addr + i * ctx.scene.primitive_stride);
    float hit_t = 0.0f;
    if (!intersect_aabb(ray, aabb, hit_t))
      continue;
    if (!private_candidate_is_before_tmax(mem, slot, hit_t))
      continue;
    Hit candidate = hit_from_aabb(aabb, hit_t);
    if (!prepare_aabb_candidate(ray, candidate))
      continue;
    return pause_private_candidate(mem, slot, ctx.scene, candidate,
                                   traversal_candidate_procedural_aabb);
  }
  return finish_private_traversal(mem, slot, key, private_state);
}

struct VtasV2ChildHit {
  uint32_t ref = invalid_node_ref;
  float t_near = 0.0f;
  uint8_t child_slot = 0;
};

struct VtasV2ParentResume {
  bool valid = false;
  bool root = false;
  vtas_v2::FrontierEntry entry;
};

template <typename Memory>
inline auto try_load_private_u64(Memory &mem, reg_t offset, uint64_t &value,
                                 int)
    -> decltype(mem.rt_private_load64(offset), bool())
{
  value = mem.rt_private_load64(offset);
  return true;
}

template <typename Memory>
inline bool try_load_private_u64(Memory &, reg_t, uint64_t &, long)
{
  return false;
}

template <typename Memory>
inline auto try_load_private_u32(Memory &mem, reg_t offset, uint32_t &value,
                                 int)
    -> decltype(mem.rt_private_load32(offset), bool())
{
  value = mem.rt_private_load32(offset);
  return true;
}

template <typename Memory>
inline bool try_load_private_u32(Memory &, reg_t, uint32_t &, long)
{
  return false;
}

template <typename Memory>
inline auto store_private_u64(Memory &mem, reg_t offset, uint64_t value, int)
    -> decltype(mem.rt_private_store64(offset, value), void())
{
  mem.rt_private_store64(offset, value);
}

template <typename Memory>
inline void store_private_u64(Memory &, reg_t, uint64_t, long)
{
}

template <typename Memory>
inline auto store_private_u32(Memory &mem, reg_t offset, uint32_t value, int)
    -> decltype(mem.rt_private_store32(offset, value), void())
{
  mem.rt_private_store32(offset, value);
}

template <typename Memory>
inline void store_private_u32(Memory &, reg_t, uint32_t, long)
{
}

inline uint32_t vtas_v2_parallel_axis_mask(const Ray &ray)
{
  const float direction[3] = {
      ray.direction.x, ray.direction.y, ray.direction.z};
  uint32_t mask = 0;
  for (uint32_t axis = 0; axis < 3; ++axis) {
    if (direction[axis] == 0.0f)
      mask |= uint32_t{1} << (14 + axis);
  }
  return mask;
}

inline float vtas_v2_inverse_direction(float direction)
{
  const float reciprocal = 1.0f / direction;
  if (std::isfinite(reciprocal))
    return reciprocal;
  return direction < 0.0f ? -std::numeric_limits<float>::max()
                          : std::numeric_limits<float>::max();
}

template <typename Memory>
inline void sync_vtas_v2_private_state(Memory &mem, reg_t slot,
                                       const RtPrivateContext &ctx)
{
  for (uint32_t index = 0; index < 6; ++index)
    store_private_u64(mem, reg_t(index) * 8,
                      ctx.v2_frontier.slots()[index], 0);
  store_private_u32(mem, 0x30, bit_cast_u32(ctx.v2_active_ray.tmin), 0);
  store_private_u32(mem, 0x34, bit_cast_u32(current_tmax(mem, slot)), 0);
  store_private_u32(mem, 0x38, ctx.v2_active_ray.flags, 0);
  uint32_t hot_control =
      (ctx.v2_active_ray.cull_mask & 0xffu) |
      (committed_hit_valid(mem, slot) ? uint32_t{1} << 8 : 0) |
      (ctx.v2_instance_addr != 0 ? uint32_t{1} << 9 : 0) |
      vtas_v2_parallel_axis_mask(ctx.v2_active_ray);
  if (ctx.v2_instance_addr != 0)
    hot_control |=
        (mem.load32(ctx.v2_instance_addr + instance_flags) & 0xfu) << 10;
  store_private_u32(mem, 0x3c, hot_control, 0);

  const float origin[3] = {ctx.v2_active_ray.origin.x,
                           ctx.v2_active_ray.origin.y,
                           ctx.v2_active_ray.origin.z};
  const float direction[3] = {ctx.v2_active_ray.direction.x,
                              ctx.v2_active_ray.direction.y,
                              ctx.v2_active_ray.direction.z};
  for (uint32_t axis = 0; axis < 3; ++axis) {
    store_private_u32(mem, rt_v5_private_origin + axis * 4,
                      bit_cast_u32(origin[axis]), 0);
    store_private_u32(mem, rt_v5_private_inv_direction + axis * 4,
                      bit_cast_u32(vtas_v2_inverse_direction(direction[axis])),
                      0);
    store_private_u32(mem, rt_v5_private_direction + axis * 4,
                      bit_cast_u32(direction[axis]), 0);
  }
  store_private_u64(mem, rt_v5_private_active_as_base,
                    ctx.v2_active_as_base, 0);
  store_private_u32(mem, rt_v5_private_instance_sbt_contribution,
                    ctx.v2_instance_sbt_offset, 0);
  store_private_u64(mem, rt_v5_private_instance_metadata_reference,
                    ctx.v2_instance_addr, 0);
  store_private_u32(mem, rt_v5_private_instance_index,
                    ctx.v2_instance_id, 0);
  store_private_u32(
      mem, rt_v5_private_instance_custom_index,
      ctx.v2_instance_addr != 0
          ? mem.load32(ctx.v2_instance_addr + instance_custom_index)
          : 0,
      0);
}

inline void fail_vtas_v2(const char *reason)
{
  throw std::runtime_error(reason);
}

inline uint32_t vtas_v2_node_size(uint32_t type)
{
  switch (type) {
  case node_box4: return vtas_v2::box4_size;
  case node_triangle: return vtas_v2::triangle_size;
  case node_instance: return vtas_v2::instance_size;
  case node_aabb: return vtas_v2::aabb_size;
  default: return 0;
  }
}

template <typename Memory>
inline void validate_vtas_v2_header(Memory &mem, uint64_t as_base,
                                    uint32_t expected_type)
{
  if (as_base == 0)
    fail_vtas_v2("ventus-rt: null VTAS V2 address");
  const uint32_t version_size = mem.load32(as_base + as_header_version);
  if (mem.load32(as_base + as_header_magic) != as_magic ||
      (version_size & 0xffffu) != as_version_v2 ||
      (version_size >> 16) != as_header_size ||
      mem.load32(as_base + as_header_type) != expected_type ||
      mem.load32(as_base + as_header_node_region_offset) != as_header_size)
    fail_vtas_v2("ventus-rt: malformed VTAS V2 header");
}

template <typename Memory>
inline void validate_vtas_v2_node_ref(Memory &mem, uint64_t as_base,
                                      uint32_t ref,
                                      vtas_v2::Domain domain)
{
  if (ref == invalid_node_ref)
    fail_vtas_v2("ventus-rt: invalid VTAS V2 node reference");
  const uint32_t type = node_ref_type(ref);
  const uint32_t size = vtas_v2_node_size(type);
  const uint32_t offset = node_ref_offset(ref);
  const uint32_t region_offset =
      mem.load32(as_base + as_header_node_region_offset);
  const uint32_t region_size =
      mem.load32(as_base + as_header_node_region_size);
  const uint64_t region_end = uint64_t(region_offset) + region_size;
  if (size == 0 || offset < region_offset ||
      uint64_t(offset) + size > region_end ||
      (type == node_box4 && (offset & (vtas_v2::box4_size - 1)) != 0))
    fail_vtas_v2("ventus-rt: out-of-range VTAS V2 node reference");
  const bool domain_type_valid =
      domain == vtas_v2::Domain::tlas
          ? (type == node_box4 || type == node_instance)
          : (type == node_box4 || type == node_triangle ||
             type == node_aabb);
  if (!domain_type_valid)
    fail_vtas_v2("ventus-rt: VTAS V2 node/domain mismatch");
}

template <typename Memory>
inline uint32_t validate_vtas_v2_image(Memory &mem, uint64_t as_base,
                                       uint32_t expected_type)
{
  validate_vtas_v2_header(mem, as_base, expected_type);
  const uint32_t root = load_node_ref(mem, as_base);
  const uint32_t expected_count =
      mem.load32(as_base + vtas_v2::header_node_count);
  const uint32_t region_size =
      mem.load32(as_base + as_header_node_region_size);
  if (root == invalid_node_ref) {
    if (expected_count != 0)
      fail_vtas_v2("ventus-rt: empty VTAS V2 has nonzero node count");
    return 0;
  }
  if (expected_count == 0 || expected_count > region_size / 64u)
    fail_vtas_v2("ventus-rt: impossible VTAS V2 node count");

  struct Work {
    uint32_t ref;
    uint32_t parent_ref;
    uint8_t parent_child;
    bool root;
  };
  std::vector<Work> work = {{root, invalid_node_ref, 0xff, true}};
  std::unordered_set<uint32_t> visited;
  const vtas_v2::Domain domain =
      expected_type == as_type_tlas ? vtas_v2::Domain::tlas
                                    : vtas_v2::Domain::blas;

  while (!work.empty()) {
    const Work item = work.back();
    work.pop_back();
    if (!visited.insert(item.ref).second)
      fail_vtas_v2("ventus-rt: VTAS V2 duplicate or cyclic node reference");
    if (visited.size() > expected_count)
      fail_vtas_v2("ventus-rt: VTAS V2 reachable node count overflow");
    validate_vtas_v2_node_ref(mem, as_base, item.ref, domain);

    const uint32_t type = node_ref_type(item.ref);
    const reg_t node_addr = as_base + node_ref_offset(item.ref);
    if (type == node_box4) {
      const uint32_t parent_ref =
          mem.load32(node_addr + box4_v2_parent_node_ref);
      const uint32_t parent_control =
          mem.load32(node_addr + box4_v2_parent_child_index);
      const uint8_t parent_child = parent_control & 0xffu;
      const uint8_t flags = (parent_control >> 8) & 0xffu;
      if ((parent_control >> 16) != 0 ||
          mem.load32(node_addr + box4_v2_child_flags + 2) != 0 ||
          mem.load32(node_addr + box4_v2_reserved + 2) != 0)
        fail_vtas_v2("ventus-rt: nonzero VTAS V2 Box4 reserved metadata");
      if (item.root) {
        if (item.ref != root || parent_ref != invalid_node_ref ||
            parent_child != 0xff || flags != box4_v2_node_flag_root)
          fail_vtas_v2("ventus-rt: malformed VTAS V2 Box4 root metadata");
      } else if (parent_ref != item.parent_ref ||
                 parent_child != item.parent_child || flags != 0) {
        fail_vtas_v2("ventus-rt: VTAS V2 Box4 parent backlink mismatch");
      }
      for (uint8_t child = 0; child < 4; ++child) {
        const uint32_t child_ref =
            mem.load32(node_addr + box4_child_ref + child * 4);
        if (child_ref != invalid_node_ref)
          work.push_back({child_ref, item.ref, child, false});
      }
      continue;
    }

    if (type == node_instance) {
      const uint32_t edge =
          mem.load32(node_addr + instance_v2_parent_edge);
      if (item.root) {
        if (item.ref != root || edge != instance_v2_root_parent_edge)
          fail_vtas_v2("ventus-rt: malformed VTAS V2 instance root metadata");
      } else {
        const uint32_t expected_edge =
            node_ref_offset(item.parent_ref) | item.parent_child;
        if ((edge & instance_v2_parent_reserved_mask) != 0 ||
            edge != expected_edge)
          fail_vtas_v2("ventus-rt: VTAS V2 instance parent backlink mismatch");
      }
    }
  }

  if (visited.size() != expected_count)
    fail_vtas_v2("ventus-rt: VTAS V2 reachable node count mismatch");
  return expected_count;
}

template <typename Memory>
inline void validate_v5_private_initial_state(Memory &mem, const Ray &ray,
                                              uint64_t accel_addr,
                                              uint32_t root)
{
  uint64_t raw_root = 0;
  if (!try_load_private_u64(mem, rt_v5_private_frontier_0, raw_root, 0))
    return;
  vtas_v2::FrontierEntry compiler_entry;
  if (!vtas_v2::decode_frontier_entry(raw_root, compiler_entry) ||
      compiler_entry.node_ref != root ||
      compiler_entry.kind != vtas_v2::FrontierKind::direct_node ||
      compiler_entry.domain != vtas_v2::Domain::tlas)
    fail_vtas_v2("ventus-rt: compiler VTAS V2 root frontier mismatch");

  const uint32_t expected_hot =
      (ray.cull_mask & 0xffu) | vtas_v2_parallel_axis_mask(ray);
  const reg_t offsets[] = {
      rt_v5_private_ray_tmin,
      rt_v5_private_ray_tmax,
      rt_v5_private_ray_flags,
      rt_v5_private_hot_control,
  };
  const uint32_t expected[] = {
      bit_cast_u32(ray.tmin),
      bit_cast_u32(ray.tmax),
      ray.flags,
      expected_hot,
  };
  for (uint32_t index = 0; index < 4; ++index) {
    uint32_t actual = 0;
    if (!try_load_private_u32(mem, offsets[index], actual, 0) ||
        actual != expected[index])
      fail_vtas_v2("ventus-rt: compiler VTAS V2 hot-state mismatch");
  }

  const float origin[3] = {ray.origin.x, ray.origin.y, ray.origin.z};
  const float direction[3] = {
      ray.direction.x, ray.direction.y, ray.direction.z};
  for (uint32_t axis = 0; axis < 3; ++axis) {
    uint32_t actual_origin = 0;
    uint32_t actual_inverse = 0;
    uint32_t actual_direction = 0;
    if (!try_load_private_u32(mem, rt_v5_private_origin + axis * 4,
                              actual_origin, 0) ||
        !try_load_private_u32(mem,
                              rt_v5_private_inv_direction + axis * 4,
                              actual_inverse, 0) ||
        !try_load_private_u32(mem, rt_v5_private_direction + axis * 4,
                              actual_direction, 0) ||
        actual_origin != bit_cast_u32(origin[axis]) ||
        actual_inverse != bit_cast_u32(
                              vtas_v2_inverse_direction(direction[axis])) ||
        actual_direction != bit_cast_u32(direction[axis]))
      fail_vtas_v2("ventus-rt: compiler VTAS V2 ray projection mismatch");
  }
  uint64_t actual_as = 0;
  if (!try_load_private_u64(mem, rt_v5_private_active_as_base,
                            actual_as, 0) || actual_as != accel_addr)
    fail_vtas_v2("ventus-rt: compiler VTAS V2 active-AS mismatch");
}

template <typename Memory>
inline VtasV2ParentResume resolve_vtas_v2_box_parent(
    Memory &mem, const RtPrivateContext &ctx,
    const vtas_v2::FrontierEntry &current)
{
  validate_vtas_v2_node_ref(mem, ctx.v2_active_as_base, current.node_ref,
                            current.domain);
  if (node_ref_type(current.node_ref) != node_box4)
    fail_vtas_v2("ventus-rt: VTAS V2 bailout target is not Box4");
  const reg_t node_addr =
      ctx.v2_active_as_base + node_ref_offset(current.node_ref);
  const uint32_t parent_ref =
      mem.load32(node_addr + box4_v2_parent_node_ref);
  const uint32_t parent_control =
      mem.load32(node_addr + box4_v2_parent_child_index);
  const uint8_t parent_child = parent_control & 0xffu;
  const uint8_t flags = (parent_control >> 8) & 0xffu;
  if ((parent_control >> 16) != 0 ||
      mem.load32(node_addr + box4_v2_child_flags + 2) != 0 ||
      mem.load32(node_addr + box4_v2_reserved + 2) != 0)
    fail_vtas_v2("ventus-rt: nonzero VTAS V2 Box4 reserved metadata");

  VtasV2ParentResume result;
  if (parent_ref == invalid_node_ref) {
    if (current.node_ref != load_node_ref(mem, ctx.v2_active_as_base) ||
        parent_child != 0xff || flags != box4_v2_node_flag_root)
      fail_vtas_v2("ventus-rt: malformed VTAS V2 Box4 root metadata");
    result.root = true;
    return result;
  }
  if (parent_child >= 4 || flags != 0)
    fail_vtas_v2("ventus-rt: malformed VTAS V2 Box4 parent edge");
  validate_vtas_v2_node_ref(mem, ctx.v2_active_as_base, parent_ref,
                            current.domain);
  if (node_ref_type(parent_ref) != node_box4 ||
      mem.load32(ctx.v2_active_as_base + node_ref_offset(parent_ref) +
                 box4_child_ref + parent_child * 4) != current.node_ref)
    fail_vtas_v2("ventus-rt: VTAS V2 Box4 parent backlink mismatch");

  result.valid = true;
  result.entry.node_ref = parent_ref;
  result.entry.kind = vtas_v2::FrontierKind::parent_resume;
  result.entry.domain = current.domain;
  result.entry.child_anchor = parent_child;
  result.entry.anchor_valid = true;
  return result;
}

template <typename Memory>
inline uint32_t load_vtas_v2_instance_parent_edge(
    Memory &mem, const RtPrivateContext &ctx, uint32_t instance_ref)
{
  validate_vtas_v2_node_ref(mem, ctx.v2_tlas_base, instance_ref,
                            vtas_v2::Domain::tlas);
  if (node_ref_type(instance_ref) != node_instance)
    fail_vtas_v2("ventus-rt: VTAS V2 cross return is not an instance");
  const reg_t instance_addr =
      ctx.v2_tlas_base + node_ref_offset(instance_ref);
  const uint32_t edge = mem.load32(instance_addr + instance_v2_parent_edge);
  if (edge == instance_v2_root_parent_edge) {
    if (instance_ref != load_node_ref(mem, ctx.v2_tlas_base))
      fail_vtas_v2("ventus-rt: non-root VTAS V2 instance claims root");
    return edge;
  }
  if ((edge & instance_v2_parent_reserved_mask) != 0)
    fail_vtas_v2("ventus-rt: malformed VTAS V2 instance parent edge");
  const uint8_t child = edge & instance_v2_parent_child_mask;
  const uint32_t parent_ref =
      (edge & instance_v2_parent_offset_mask) | node_box4;
  validate_vtas_v2_node_ref(mem, ctx.v2_tlas_base, parent_ref,
                            vtas_v2::Domain::tlas);
  if (mem.load32(ctx.v2_tlas_base + node_ref_offset(parent_ref) +
                 box4_child_ref + child * 4) != instance_ref)
    fail_vtas_v2("ventus-rt: VTAS V2 instance parent backlink mismatch");
  return edge;
}

inline bool vtas_v2_child_less(const VtasV2ChildHit &lhs,
                               const VtasV2ChildHit &rhs)
{
  if (lhs.t_near != rhs.t_near)
    return lhs.t_near < rhs.t_near;
  if (lhs.child_slot != rhs.child_slot)
    return lhs.child_slot < rhs.child_slot;
  return lhs.ref < rhs.ref;
}

template <typename Memory>
inline uint32_t collect_vtas_v2_box_hits(
    Memory &mem, const RtPrivateContext &ctx,
    const vtas_v2::FrontierEntry &entry, float committed_tmax,
    VtasV2ChildHit (&filtered)[4])
{
  VtasV2ChildHit all[4];
  uint32_t all_count = 0;
  for (uint8_t child_slot = 0; child_slot < 4; ++child_slot) {
    const reg_t node_addr =
        ctx.v2_active_as_base + node_ref_offset(entry.node_ref);
    const uint32_t child =
        mem.load32(node_addr + box4_child_ref + child_slot * 4);
    if (child == invalid_node_ref)
      continue;
    validate_vtas_v2_node_ref(mem, ctx.v2_active_as_base, child,
                              entry.domain);
    float t_near = 0.0f;
    if (intersect_box4_child(mem, ctx.v2_active_as_base, entry.node_ref,
                             child_slot, ctx.v2_active_ray, t_near))
      all[all_count++] = {child, t_near, child_slot};
  }
  for (uint32_t index = 1; index < all_count; ++index) {
    const VtasV2ChildHit value = all[index];
    uint32_t insert = index;
    while (insert > 0 && vtas_v2_child_less(value, all[insert - 1])) {
      all[insert] = all[insert - 1];
      --insert;
    }
    all[insert] = value;
  }

  int anchor_index = -1;
  if (entry.anchor_valid) {
    for (uint32_t index = 0; index < all_count; ++index) {
      if (all[index].child_slot == entry.child_anchor) {
        anchor_index = int(index);
        break;
      }
    }
    if (anchor_index < 0)
      fail_vtas_v2("ventus-rt: VTAS V2 replay anchor is not reproducible");
  }

  uint32_t filtered_count = 0;
  for (uint32_t index = 0; index < all_count; ++index) {
    const bool selected_by_cursor =
        !entry.anchor_valid || index > uint32_t(anchor_index) ||
        (entry.inclusive && index == uint32_t(anchor_index));
    if (selected_by_cursor && all[index].t_near < committed_tmax)
      filtered[filtered_count++] = all[index];
  }
  return filtered_count;
}

inline bool vtas_v2_internal_ref(uint32_t ref)
{
  return node_ref_type(ref) == node_box4;
}

inline bool push_vtas_v2_node_route(
    RtPrivateContext &ctx, const vtas_v2::FrontierEntry &current,
    const VtasV2ChildHit (&hits)[4], uint32_t hit_count,
    const VtasV2ParentResume &parent,
    vtas_v2::FrontierEntry &selected)
{
  vtas_v2::FrontierEntry old_entries[6];
  const uint8_t old_ordinary = ctx.v2_frontier.ordinary_size();
  for (uint8_t index = 0; index < old_ordinary; ++index) {
    if (!ctx.v2_frontier.read_logical(index, old_entries[index]))
      fail_vtas_v2("ventus-rt: invalid VTAS V2 resident frontier");
  }
  vtas_v2::FrontierEntry protected_return;
  if (ctx.v2_frontier.control().cross_as &&
      !ctx.v2_frontier.read_logical(old_ordinary, protected_return))
    fail_vtas_v2("ventus-rt: missing VTAS V2 cross-AS return");

  const uint8_t capacity =
      ctx.v2_frontier.control().cross_as ? 5 : 6;
  vtas_v2::FrontierEntry merged[10];
  uint8_t merged_count = 0;
  const uint8_t frontier_count = hit_count == 0 ? 0 : hit_count - 1;
  bool replayable = true;
  for (uint8_t index = 0; index < frontier_count; ++index)
    replayable = replayable && vtas_v2_internal_ref(hits[index + 1].ref);
  const bool direct_remainder_fits =
      hit_count != 0 && replayable &&
      unsigned(old_ordinary) + frontier_count + (parent.valid ? 1 : 0) <=
          capacity;

  if (hit_count != 0 && direct_remainder_fits) {
    for (uint8_t index = 0; index < frontier_count; ++index) {
      vtas_v2::FrontierEntry direct;
      direct.node_ref = hits[index + 1].ref;
      direct.domain = current.domain;
      merged[merged_count++] = direct;
    }
  } else if (frontier_count != 0) {
    vtas_v2::FrontierEntry replay;
    replay.node_ref = current.node_ref;
    replay.kind = vtas_v2::FrontierKind::same_node_replay;
    replay.domain = current.domain;
    replay.child_anchor = hits[0].child_slot;
    replay.anchor_valid = true;
    merged[merged_count++] = replay;
    ctx.v2_replay_count++;
  }

  bool parent_deferred = false;
  if (parent.valid) {
    if (merged_count < capacity)
      merged[merged_count++] = parent.entry;
    else
      parent_deferred = true;
  }
  for (uint8_t index = 0; index < old_ordinary; ++index)
    merged[merged_count++] = old_entries[index];

  const bool overflow = merged_count > capacity || parent_deferred;
  if (overflow)
    ctx.v2_overflow_count++;
  const uint8_t retained = merged_count > capacity ? capacity : merged_count;
  vtas_v2::FrontierEntry final_entries[6];
  for (uint8_t index = 0; index < retained; ++index)
    final_entries[index] = merged[index];
  uint8_t final_count = retained;
  if (ctx.v2_frontier.control().cross_as)
    final_entries[final_count++] = protected_return;
  const bool lost = ctx.v2_frontier.control().lost || overflow;
  if (!ctx.v2_frontier.rewrite(final_entries, final_count,
                               ctx.v2_frontier.control().cross_as, lost))
    fail_vtas_v2("ventus-rt: rejected VTAS V2 route transition");

  if (hit_count == 0)
    return false;
  const vtas_v2::Domain selected_domain = current.domain;
  selected = {};
  selected.node_ref = hits[0].ref;
  selected.domain = selected_domain;
  return true;
}

template <typename Memory>
inline uint32_t trace_private_vtas_v1(Memory &mem, reg_t slot, uint64_t key,
                                      RtPrivateState &private_state)
{
  RtPrivateContext &ctx = private_state.contexts().at(key);
  while (!ctx.stack.empty()) {
    TraversalEntry entry = ctx.stack.back();
    ctx.stack.pop_back();
    if (entry.node_ref == invalid_node_ref)
      continue;

    entry.ray.tmax = current_tmax(mem, slot);
    const uint32_t type = node_ref_type(entry.node_ref);
    const reg_t node_addr = entry.as_base + node_ref_offset(entry.node_ref);

    if (type == node_box4) {
      ChildHit hits[4];
      uint32_t hit_count = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        const uint32_t child = mem.load32(node_addr + box4_child_ref + i * 4);
        if (child == invalid_node_ref)
          continue;
        float t_near = 0.0f;
        if (intersect_box4_child(mem, entry.as_base, entry.node_ref, i,
                                 entry.ray, t_near))
          hits[hit_count++] = {child, t_near};
      }
      sort_child_hits(hits, hit_count);
      for (uint32_t i = hit_count; i > 0; --i)
        ctx.stack.push_back({entry.as_base, hits[i - 1].ref, entry.ray,
                             entry.instance_id, entry.instance_sbt_offset,
                             entry.instance_addr});
      continue;
    }

    if (type == node_instance) {
      const uint32_t mask = mem.load32(node_addr + instance_mask);
      if ((entry.ray.cull_mask & mask) == 0)
        continue;
      const uint64_t blas = load_u64(mem, node_addr + instance_blas_addr_lo);
      if (!blas || mem.load32(blas + as_header_magic) != as_magic ||
          mem.load32(blas + as_header_type) != as_type_blas)
        continue;
      Ray object_ray = entry.ray;
      object_ray.origin = transform_instance_point(
          mem, node_addr + instance_world_to_object, entry.ray.origin);
      object_ray.direction = transform_instance_vector(
          mem, node_addr + instance_world_to_object, entry.ray.direction);
      ctx.stack.push_back({blas, load_node_ref(mem, blas), object_ray,
                           mem.load32(node_addr + instance_instance_id),
                           mem.load32(node_addr + instance_sbt_record_offset),
                           node_addr});
      continue;
    }

    if (type == node_aabb) {
      const ProceduralAabb aabb = load_vtas_aabb(
          mem, entry.as_base, entry.node_ref, entry.instance_id,
          entry.instance_sbt_offset, entry.instance_addr,
          entry.ray.sbt_stride);
      float hit_t = 0.0f;
      if (!intersect_aabb(entry.ray, aabb, hit_t) ||
          !private_candidate_is_before_tmax(mem, slot, hit_t))
        continue;
      Hit candidate = hit_from_aabb(aabb, hit_t);
      if (!prepare_aabb_candidate(entry.ray, candidate))
        continue;
      return pause_private_candidate(mem, slot, ctx.scene, candidate,
                                     traversal_candidate_procedural_aabb);
    }

    if (type != node_triangle)
      continue;
    const Triangle tri = load_vtas_triangle(mem, entry.as_base, entry.node_ref,
                                             entry.instance_id,
                                             entry.instance_sbt_offset,
                                             entry.instance_addr,
                                             entry.ray.sbt_stride);
    Hit candidate;
    candidate.t = entry.ray.tmax;
    if (!intersect_triangle(entry.ray, tri, candidate) ||
        !prepare_triangle_candidate(entry.ray, candidate))
      continue;
    if (!candidate.tri.opaque)
      return pause_private_candidate(mem, slot, ctx.scene, candidate,
                                     traversal_candidate_non_opaque_triangle);
    commit_hit(mem, slot, ctx.scene, candidate);
    if (entry.ray.flags & ray_flag_terminate_on_first_hit)
      return finish_private_traversal(mem, slot, key, private_state);
  }
  return finish_private_traversal(mem, slot, key, private_state);
}

template <typename Memory>
inline void enter_vtas_v2_blas(Memory &mem, RtPrivateContext &ctx,
                               const vtas_v2::FrontierEntry &instance_entry)
{
  if (instance_entry.domain != vtas_v2::Domain::tlas ||
      ctx.v2_frontier.control().cross_as)
    fail_vtas_v2("ventus-rt: nested VTAS V2 AS crossing");
  const reg_t instance_addr =
      ctx.v2_tlas_base + node_ref_offset(instance_entry.node_ref);
  (void)load_vtas_v2_instance_parent_edge(mem, ctx,
                                           instance_entry.node_ref);
  const uint64_t blas = load_u64(mem, instance_addr + instance_blas_addr_lo);
  const uint32_t blas_node_count =
      validate_vtas_v2_image(mem, blas, as_type_blas);
  if (ctx.v2_cross_count >= ctx.v2_tlas_node_count)
    fail_vtas_v2("ventus-rt: VTAS V2 cross-AS watchdog exceeded");
  const uint64_t added_budget =
      std::max<uint64_t>(256, uint64_t(blas_node_count) * 64);
  if (ctx.v2_operation_budget >
      std::numeric_limits<uint64_t>::max() - added_budget)
    fail_vtas_v2("ventus-rt: VTAS V2 operation budget overflow");
  ctx.v2_operation_budget += added_budget;

  vtas_v2::FrontierEntry cross_return;
  cross_return.node_ref = instance_entry.node_ref;
  cross_return.kind = vtas_v2::FrontierKind::cross_as_return;
  cross_return.domain = vtas_v2::Domain::tlas;
  if (!ctx.v2_frontier.reset_to(cross_return, true, false))
    fail_vtas_v2("ventus-rt: cannot install VTAS V2 cross-AS return");

  ctx.v2_active_as_base = blas;
  ctx.v2_active_ray = ctx.ray;
  ctx.v2_active_ray.origin = transform_instance_point(
      mem, instance_addr + instance_world_to_object, ctx.ray.origin);
  ctx.v2_active_ray.direction = transform_instance_vector(
      mem, instance_addr + instance_world_to_object, ctx.ray.direction);
  ctx.v2_instance_id = mem.load32(instance_addr + instance_instance_id);
  ctx.v2_instance_sbt_offset =
      mem.load32(instance_addr + instance_sbt_record_offset);
  ctx.v2_instance_addr = instance_addr;
  ctx.v2_cross_count++;

  const uint32_t root = load_node_ref(mem, blas);
  if (root != invalid_node_ref) {
    validate_vtas_v2_node_ref(mem, blas, root, vtas_v2::Domain::blas);
    vtas_v2::FrontierEntry root_entry;
    root_entry.node_ref = root;
    root_entry.domain = vtas_v2::Domain::blas;
    if (!ctx.v2_frontier.push_top(root_entry))
      fail_vtas_v2("ventus-rt: cannot install VTAS V2 BLAS root");
  }
}

template <typename Memory>
inline void return_vtas_v2_to_tlas(Memory &mem, RtPrivateContext &ctx)
{
  if (!ctx.v2_frontier.control().cross_as ||
      ctx.v2_frontier.control().lost ||
      ctx.v2_frontier.ordinary_size() != 0 ||
      ctx.v2_frontier.size() != 1)
    fail_vtas_v2("ventus-rt: invalid VTAS V2 BLAS return state");
  vtas_v2::FrontierEntry return_entry;
  if (!ctx.v2_frontier.read_logical(0, return_entry) ||
      return_entry.kind != vtas_v2::FrontierKind::cross_as_return)
    fail_vtas_v2("ventus-rt: missing VTAS V2 BLAS return entry");

  const uint32_t edge = load_vtas_v2_instance_parent_edge(
      mem, ctx, return_entry.node_ref);
  ctx.v2_active_as_base = ctx.v2_tlas_base;
  ctx.v2_active_ray = ctx.ray;
  ctx.v2_instance_id = 0;
  ctx.v2_instance_sbt_offset = 0;
  ctx.v2_instance_addr = 0;
  if (edge == instance_v2_root_parent_edge) {
    ctx.v2_frontier.clear();
    return;
  }

  vtas_v2::FrontierEntry parent;
  parent.node_ref = (edge & instance_v2_parent_offset_mask) | node_box4;
  parent.kind = vtas_v2::FrontierKind::parent_resume;
  parent.domain = vtas_v2::Domain::tlas;
  parent.child_anchor = edge & instance_v2_parent_child_mask;
  parent.anchor_valid = true;
  if (!ctx.v2_frontier.reset_to(parent, false, true))
    fail_vtas_v2("ventus-rt: cannot install VTAS V2 TLAS parent resume");
}

template <typename Memory>
inline uint32_t trace_private_vtas_v2(Memory &mem, reg_t slot, uint64_t key,
                                      RtPrivateState &private_state)
{
  RtPrivateContext &ctx = private_state.contexts().at(key);
  bool current_valid = false;
  vtas_v2::FrontierEntry current;

  while (true) {
    if (++ctx.v2_operation_count > ctx.v2_operation_budget)
      fail_vtas_v2("ventus-rt: VTAS V2 traversal watchdog exceeded");
    VtasV2ParentResume parent;
    if (!current_valid) {
      if (!ctx.v2_frontier.valid())
        fail_vtas_v2("ventus-rt: invalid VTAS V2 frontier state");
      if (ctx.v2_frontier.empty()) {
        if (ctx.v2_frontier.control().cross_as ||
            ctx.v2_frontier.control().lost)
          fail_vtas_v2("ventus-rt: incomplete VTAS V2 frontier drained");
        sync_vtas_v2_private_state(mem, slot, ctx);
        return finish_private_traversal(mem, slot, key, private_state);
      }
      if (ctx.v2_frontier.control().cross_as &&
          ctx.v2_frontier.ordinary_size() == 0) {
        return_vtas_v2_to_tlas(mem, ctx);
        sync_vtas_v2_private_state(mem, slot, ctx);
        continue;
      }

      const bool parent_bailout =
          ctx.v2_frontier.control().lost &&
          ctx.v2_frontier.ordinary_size() == 1;
      if (!ctx.v2_frontier.pop_top(current))
        fail_vtas_v2("ventus-rt: cannot pop VTAS V2 frontier");
      if (parent_bailout) {
        if (node_ref_type(current.node_ref) != node_box4 ||
            current.kind == vtas_v2::FrontierKind::cross_as_return)
          fail_vtas_v2("ventus-rt: VTAS V2 lost state lacks internal anchor");
        parent = resolve_vtas_v2_box_parent(mem, ctx, current);
        ctx.v2_parent_bailout_count++;
        if (parent.root)
          ctx.v2_frontier.control().lost = false;
      }
    }
    current_valid = false;

    const vtas_v2::Domain active_domain =
        ctx.v2_frontier.control().cross_as ? vtas_v2::Domain::blas
                                           : vtas_v2::Domain::tlas;
    if (current.domain != active_domain ||
        current.kind == vtas_v2::FrontierKind::cross_as_return)
      fail_vtas_v2("ventus-rt: VTAS V2 active-domain mismatch");
    validate_vtas_v2_node_ref(mem, ctx.v2_active_as_base,
                              current.node_ref, current.domain);
    const uint32_t type = node_ref_type(current.node_ref);
    const reg_t node_addr =
        ctx.v2_active_as_base + node_ref_offset(current.node_ref);

    if (type == node_box4) {
      VtasV2ChildHit hits[4];
      const uint32_t hit_count = collect_vtas_v2_box_hits(
          mem, ctx, current, current_tmax(mem, slot), hits);
      current_valid = push_vtas_v2_node_route(
          ctx, current, hits, hit_count, parent, current);
      sync_vtas_v2_private_state(mem, slot, ctx);
      continue;
    }
    if (parent.valid || parent.root)
      fail_vtas_v2("ventus-rt: VTAS V2 parent bailout consumed a leaf");

    Ray ray = ctx.v2_active_ray;
    ray.tmax = current_tmax(mem, slot);
    if (type == node_instance) {
      if (current.domain != vtas_v2::Domain::tlas)
        fail_vtas_v2("ventus-rt: VTAS V2 instance outside TLAS");
      const uint32_t mask = mem.load32(node_addr + instance_mask);
      if ((ray.cull_mask & mask) == 0)
        continue;
      enter_vtas_v2_blas(mem, ctx, current);
      sync_vtas_v2_private_state(mem, slot, ctx);
      continue;
    }

    if (type == node_aabb) {
      const ProceduralAabb aabb = load_vtas_aabb(
          mem, ctx.v2_active_as_base, current.node_ref, ctx.v2_instance_id,
          ctx.v2_instance_sbt_offset, ctx.v2_instance_addr,
          ray.sbt_stride);
      float hit_t = 0.0f;
      if (!intersect_aabb(ray, aabb, hit_t) ||
          !private_candidate_is_before_tmax(mem, slot, hit_t))
        continue;
      Hit candidate = hit_from_aabb(aabb, hit_t);
      if (!prepare_aabb_candidate(ray, candidate))
        continue;
      sync_vtas_v2_private_state(mem, slot, ctx);
      return pause_private_candidate(mem, slot, ctx.scene, candidate,
                                     traversal_candidate_procedural_aabb);
    }

    if (type != node_triangle)
      fail_vtas_v2("ventus-rt: unsupported VTAS V2 leaf type");
    const Triangle tri = load_vtas_triangle(
        mem, ctx.v2_active_as_base, current.node_ref, ctx.v2_instance_id,
        ctx.v2_instance_sbt_offset, ctx.v2_instance_addr, ray.sbt_stride);
    Hit candidate;
    candidate.t = ray.tmax;
    if (!intersect_triangle(ray, tri, candidate) ||
        !prepare_triangle_candidate(ray, candidate))
      continue;
    if (!candidate.tri.opaque) {
      sync_vtas_v2_private_state(mem, slot, ctx);
      return pause_private_candidate(mem, slot, ctx.scene, candidate,
                                     traversal_candidate_non_opaque_triangle);
    }
    commit_hit(mem, slot, ctx.scene, candidate);
    if (ray.flags & ray_flag_terminate_on_first_hit)
      return finish_private_traversal(mem, slot, key, private_state);
  }
}

template <typename Memory>
inline uint32_t trace_private_vtas(Memory &mem, reg_t slot, uint64_t key,
                                   RtPrivateState &private_state)
{
  const RtPrivateContext &ctx = private_state.contexts().at(key);
  return ctx.vtas_version == as_version_v2
             ? trace_private_vtas_v2(mem, slot, key, private_state)
             : trace_private_vtas_v1(mem, slot, key, private_state);
}

template <typename Memory>
inline uint32_t traverse(Memory &mem, reg_t slot,
                         RtPrivateState &private_state)
{
  const uint64_t key = rt_context_key(mem, slot, 0);
  std::unordered_map<uint64_t, RtPrivateContext> &contexts =
      private_state.contexts();
  auto it = contexts.find(key);
  if (it != contexts.end()) {
    const uint32_t decision = load_word(
        mem, slot, control_base, control_callback_decision);
    if (decision == callback_pending || decision > callback_terminate)
      throw std::runtime_error("ventus-rt: invalid V4 callback decision");

    clear_control(mem, slot);
    if (decision == callback_accept || decision == callback_terminate)
      commit_private_candidate(mem, slot);
    store_word(mem, slot, candidate_hit_record_base, hit_record_status,
               hit_record_invalid);

    if (decision == callback_terminate) {
      const bool hit = committed_hit_valid(mem, slot);
      contexts.erase(it);
      return hit ? traversal_complete_hit : traversal_terminated;
    }

    switch (it->second.kind) {
    case RtPrivateTraversalKind::triangle_list:
      return trace_private_triangle_list(mem, slot, key, private_state);
    case RtPrivateTraversalKind::aabb_list:
      return trace_private_aabb_list(mem, slot, key, private_state);
    case RtPrivateTraversalKind::vtas:
      return trace_private_vtas(mem, slot, key, private_state);
    }
  }

  clear_control(mem, slot);
  store_word(mem, slot, candidate_hit_record_base, hit_record_status,
             hit_record_invalid);
  store_word(mem, slot, committed_hit_record_base, hit_record_status,
             hit_record_invalid);
  const Ray ray = load_ray(mem, slot);
  const uint64_t accel_addr = load_accel_addr(mem, slot);
  if (accel_addr == 0)
    return finish_private_traversal(mem, slot, key, private_state);

  if (mem.load32(accel_addr + 0) == as_magic) {
    const uint16_t vtas_version =
        mem.load32(accel_addr + as_header_version) & 0xffffu;
    if (!is_supported_vtas_version(vtas_version) ||
        mem.load32(accel_addr + as_header_type) != as_type_tlas)
      return finish_private_traversal(mem, slot, key, private_state);
    uint32_t v2_tlas_node_count = 0;
    if (vtas_version == as_version_v2)
      v2_tlas_node_count =
          validate_vtas_v2_image(mem, accel_addr, as_type_tlas);
    RtPrivateContext ctx = {};
    ctx.kind = RtPrivateTraversalKind::vtas;
    ctx.vtas_version = vtas_version;
    ctx.ray = ray;
    ctx.scene.hit_sbt_base = 0;
    ctx.scene.shader_group_handle_size = 32;
    const uint32_t root = load_node_ref(mem, accel_addr);
    if (root == invalid_node_ref)
      return finish_private_traversal(mem, slot, key, private_state);
    if (vtas_version == as_version_v2) {
      validate_vtas_v2_node_ref(mem, accel_addr, root,
                                vtas_v2::Domain::tlas);
      vtas_v2::FrontierEntry root_entry;
      root_entry.node_ref = root;
      root_entry.domain = vtas_v2::Domain::tlas;
      validate_v5_private_initial_state(mem, ray, accel_addr, root);
      if (!ctx.v2_frontier.reset_to(root_entry, false, false))
        fail_vtas_v2("ventus-rt: cannot initialize VTAS V2 root frontier");
      ctx.v2_tlas_base = accel_addr;
      ctx.v2_active_as_base = accel_addr;
      ctx.v2_active_ray = ray;
      ctx.v2_tlas_node_count = v2_tlas_node_count;
      ctx.v2_operation_budget =
          std::max<uint64_t>(1024, uint64_t(v2_tlas_node_count) * 64);
    } else {
      ctx.stack.push_back({accel_addr, root, ray, 0, 0, 0});
    }
    contexts.emplace(key, std::move(ctx));
    if (vtas_version == as_version_v2)
      sync_vtas_v2_private_state(mem, slot, contexts.at(key));
    return trace_private_vtas(mem, slot, key, private_state);
  }

  Scene scene;
  const uint32_t magic = mem.load32(accel_addr + 0);
  const uint32_t version = mem.load32(accel_addr + 4);
  const uint32_t geometry_type = mem.load32(accel_addr + 8);
  scene.primitive_count = mem.load32(accel_addr + 12);
  scene.primitive_addr = load_u64(mem, accel_addr + 16);
  scene.primitive_stride = mem.load32(accel_addr + 24);
  scene.hit_sbt_base = load_u64(mem, accel_addr + 28);
  scene.shader_group_handle_size = mem.load32(accel_addr + 36);
  if (magic != bvh_magic || version != bvh_version ||
      scene.primitive_count == 0 || scene.primitive_addr == 0 ||
      scene.primitive_stride == 0)
    return finish_private_traversal(mem, slot, key, private_state);

  RtPrivateContext ctx = {};
  ctx.ray = ray;
  ctx.scene = scene;
  ctx.kind = geometry_type == geometry_triangle_list
                 ? RtPrivateTraversalKind::triangle_list
                 : RtPrivateTraversalKind::aabb_list;
  if (geometry_type != geometry_triangle_list &&
      geometry_type != geometry_procedural_aabb_list)
    return finish_private_traversal(mem, slot, key, private_state);
  contexts.emplace(key, std::move(ctx));
  return geometry_type == geometry_triangle_list
             ? trace_private_triangle_list(mem, slot, key, private_state)
             : trace_private_aabb_list(mem, slot, key, private_state);
}

template <typename Memory>
inline void release(Memory &mem, reg_t slot, RtPrivateState &private_state)
{
  private_state.contexts().erase(rt_context_key(mem, slot, 0));
  clear_control(mem, slot);
}

#ifdef VENTUS_RT_STANDALONE
template <typename Memory>
inline RtPrivateState &standalone_rt_private_state()
{
  static RtPrivateState private_state;
  return private_state;
}

template <typename Memory>
inline uint32_t traverse(Memory &mem, reg_t slot)
{
  return traverse(mem, slot, standalone_rt_private_state<Memory>());
}

template <typename Memory>
inline void release(Memory &mem, reg_t slot)
{
  release(mem, slot, standalone_rt_private_state<Memory>());
}
#endif

#ifndef VENTUS_RT_STANDALONE
struct SpikePdsMemory {
  mmu_t &mmu;
  reg_t pds_base;
  reg_t num_warps;
  reg_t num_threads;
  reg_t tid;
  reg_t lane;

  reg_t pds_addr(reg_t logical_addr) const
  {
    return pds_physical_addr(pds_base, num_warps, num_threads, tid, lane,
                             logical_addr);
  }

  uint32_t load32(reg_t logical_addr)
  {
    return uint32_t(mmu.load_uint32(pds_addr(logical_addr)));
  }

  void store32(reg_t logical_addr, uint32_t value)
  {
    mmu.store_uint32(pds_addr(logical_addr), value);
  }
};

struct RawMemory {
  mmu_t &mmu;

  uint32_t load32(reg_t addr)
  {
    return uint32_t(mmu.load_uint32(addr));
  }

  void store32(reg_t addr, uint32_t value)
  {
    mmu.store_uint32(addr, value);
  }
};

struct HybridMemory {
  SpikePdsMemory slot;
  RawMemory raw;

  uint64_t rt_context_key(reg_t logical_slot) const
  {
    return slot.pds_addr(logical_slot);
  }

  uint32_t load32(reg_t addr)
  {
    if (addr < rt_pds_total_size_bytes)
      return slot.load32(addr);
    return raw.load32(addr);
  }

  void store32(reg_t addr, uint32_t value)
  {
    if (addr < rt_pds_total_size_bytes) {
      slot.store32(addr, value);
      return;
    }
    raw.store32(addr, value);
  }
};

// V5 carries the canonical per-lane PDS address directly. Local PDS offsets
// are rebased through that address while AS/SBT pointers remain raw addresses.
struct CanonicalPdsMemory {
  RawMemory raw;
  reg_t pds_address;

  uint32_t load32(reg_t address)
  {
    return raw.load32(address < rt_v5_pds_size_bytes
                          ? pds_address + address
                          : address);
  }

  void store32(reg_t address, uint32_t value)
  {
    raw.store32(address < rt_v5_pds_size_bytes
                    ? pds_address + address
                    : address,
                value);
  }
};

inline HybridMemory make_hybrid_memory(mmu_t &mmu, reg_t pds_base,
                                       reg_t num_warps, reg_t num_threads,
                                       reg_t tid, reg_t lane)
{
  return {{mmu, pds_base, num_warps, num_threads, tid, lane}, {mmu}};
}

inline CanonicalPdsMemory make_canonical_pds_memory(mmu_t &mmu,
                                                     reg_t pds_address)
{
  return {{mmu}, pds_address};
}
#endif

} // namespace ventus_rt

#endif
