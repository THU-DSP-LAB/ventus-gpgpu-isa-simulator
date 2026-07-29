#ifndef RISCV_VENTUS_RT_H
#define RISCV_VENTUS_RT_H

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <vector>

#ifndef VENTUS_RT_STANDALONE
#include "decode.h"
#include "mmu.h"
#else
using reg_t = uint64_t;
class mmu_t;
#endif

namespace ventus_rt {

constexpr reg_t lanes = 32;

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

constexpr reg_t control_base = 80;
constexpr reg_t control_done = 0;
constexpr reg_t control_incomplete = 1;
constexpr reg_t control_accept_hit = 2;
constexpr reg_t control_ignore_hit = 3;
constexpr reg_t control_terminate_ray = 4;
constexpr reg_t control_skip_closest_hit = 5;

constexpr reg_t committed_hit_record_base = 192;
constexpr reg_t candidate_hit_record_base = 112;
constexpr reg_t hit_attrib_base = 272;
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

constexpr uint32_t rt_status_hit = 2;
constexpr uint32_t rt_status_miss = 3;
constexpr uint32_t rt_status_done = 4;

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
constexpr uint32_t as_version = 1;
constexpr uint32_t as_type_blas = 1;
constexpr uint32_t as_type_tlas = 2;
constexpr uint32_t as_header_size = 0x50;

constexpr uint32_t as_header_magic = 0x00;
constexpr uint32_t as_header_version = 0x04;
constexpr uint32_t as_header_type = 0x08;
constexpr uint32_t as_header_root_node_ref = 0x10;
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
  if (std::fabs(det) < eps)
    return false;

  const float inv_det = 1.0f / det;
  const Vec3 tvec = sub(ray.origin, tri.v0);
  const float u = dot(tvec, p) * inv_det;
  if (u < 0.0f || u > 1.0f)
    return false;

  const Vec3 q = cross(tvec, e1);
  const float v = dot(ray.direction, q) * inv_det;
  if (v < 0.0f || u + v > 1.0f)
    return false;

  const float t = dot(e2, q) * inv_det;
  if (t < ray.tmin || t > ray.tmax || t >= hit.t)
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
  hit.tri.hit_kind = aabb.hit_kind;
  hit.tri.opaque = 0;
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
  for (reg_t word = control_done; word <= control_skip_closest_hit; ++word)
    store_word(mem, slot, control_base, word, 0);
}

template <typename Memory>
inline void write_hit_record(Memory &mem, reg_t slot, reg_t base,
                             const Scene &scene, const Hit &hit,
                             uint32_t status)
{
  const uint32_t sbt_index =
      hit.tri.sbt_index + load_word(mem, slot, 0, slot_sbt_offset);
  const uint64_t shader_record_ptr =
      scene.hit_sbt_base +
      uint64_t(sbt_index) * uint64_t(load_word(mem, slot, 0, slot_sbt_stride)) +
      scene.shader_group_handle_size;

  store_word(mem, slot, base, hit_record_status, status);
  store_word(mem, slot, base, hit_record_hit_t, bit_cast_u32(hit.t));
  store_word(mem, slot, base, hit_record_sbt_index, sbt_index);
  store_word(mem, slot, base, hit_record_shader_record_ptr_lo,
             uint32_t(shader_record_ptr));
  store_word(mem, slot, base, hit_record_shader_record_ptr_hi,
             uint32_t(shader_record_ptr >> 32));
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
  store_word(mem, slot, hit_attrib_base, 0, bit_cast_u32(hit.bary_x));
  store_word(mem, slot, hit_attrib_base, 1, bit_cast_u32(hit.bary_y));
}

template <typename Memory>
inline void copy_hit_record(Memory &mem, reg_t slot, reg_t dst_base,
                            reg_t src_base)
{
  for (reg_t word = hit_record_status; word <= hit_record_instance_sbt_record_offset;
       ++word)
    store_word(mem, slot, dst_base, word, load_word(mem, slot, src_base, word));
}


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
                                   uint64_t instance_addr)
{
  const reg_t addr = as_base + node_ref_offset(node_ref);
  Triangle tri;
  tri.v0 = load_vec3(mem, addr + triangle_v0);
  tri.v1 = load_vec3(mem, addr + triangle_v1);
  tri.v2 = load_vec3(mem, addr + triangle_v2);
  tri.primitive_id = mem.load32(addr + triangle_primitive_id);
  tri.instance_id = instance_id;
  tri.geometry_id = mem.load32(addr + triangle_geometry_id);
  tri.sbt_index = mem.load32(addr + triangle_sbt_record_offset);
  tri.instance_sbt_record_offset = instance_sbt_offset;
  tri.hit_kind = 0xfe;
  tri.opaque = mem.load32(addr + triangle_flags) & 0x1;
  tri.primitive_addr = load_u64(mem, addr + triangle_primitive_addr_lo);
  tri.instance_addr = instance_addr;
  return tri;
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

struct TraversalEntry {
  uint64_t as_base = 0;
  uint32_t node_ref = invalid_node_ref;
  Ray ray;
  uint32_t instance_id = 0;
  uint32_t instance_sbt_offset = 0;
  uint64_t instance_addr = 0;
};

struct ChildHit {
  uint32_t ref = invalid_node_ref;
  float t_near = 0.0f;
};

struct TraversalDebugCounters {
  uint32_t boxes = 0;
  uint32_t box_child_tests = 0;
  uint32_t box_child_hits = 0;
  uint32_t instances = 0;
  uint32_t triangles = 0;
  uint32_t triangle_hits = 0;
};

template <typename Memory>
inline uint32_t trace_vtas(Memory &mem, reg_t slot, const Ray &ray,
                           uint64_t tlas_addr, bool skip_non_opaque)
{
  static uint32_t debug_count = 0;
  const bool debug_enabled = std::getenv("VENTUS_RT_DEBUG_TRAVERSAL") != nullptr;
  const char *debug_start_env = std::getenv("VENTUS_RT_DEBUG_TRAVERSAL_START");
  const uint32_t debug_start =
      debug_start_env ? static_cast<uint32_t>(std::strtoul(debug_start_env, nullptr, 0)) : 0;
  const uint32_t debug_id = debug_enabled ? debug_count++ : 0;
  const bool debug = debug_enabled && debug_id >= debug_start &&
                     debug_id < debug_start + 8;
  TraversalDebugCounters debug_counters;

  if (mem.load32(tlas_addr + as_header_magic) != as_magic ||
      (mem.load32(tlas_addr + as_header_version) & 0xffffu) != as_version ||
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
      std::sort(hits, hits + hit_count,
                [](const ChildHit &a, const ChildHit &b) { return a.t_near < b.t_near; });
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
                                        entry.instance_addr);
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

template <typename Memory>
inline uint32_t traverse(Memory &mem, reg_t slot)
{
  if (load_word(mem, slot, control_base, control_terminate_ray)) {
    store_word(mem, slot, 0, slot_status, rt_status_done);
    store_word(mem, slot, control_base, control_done, 1);
    return traversal_terminated;
  }

  if (load_word(mem, slot, control_base, control_accept_hit)) {
    copy_hit_record(mem, slot, committed_hit_record_base,
                    candidate_hit_record_base);
    const uint32_t hit_t =
        load_word(mem, slot, candidate_hit_record_base, hit_record_hit_t);
    const uint32_t sbt_index =
        load_word(mem, slot, candidate_hit_record_base, hit_record_sbt_index);
    store_word(mem, slot, 0, slot_hit_t, hit_t);
    store_word(mem, slot, 0, slot_sbt_index, sbt_index);
    store_word(mem, slot, 0, slot_status, rt_status_hit);
    clear_control(mem, slot);
    store_word(mem, slot, control_base, control_done, 1);
    return traversal_complete_hit;
  }

  const bool skip_non_opaque =
      load_word(mem, slot, control_base, control_ignore_hit) != 0;
  clear_control(mem, slot);
  const Ray ray = load_ray(mem, slot);
  const uint64_t accel_addr = load_accel_addr(mem, slot);
  if (accel_addr == 0) {
    store_word(mem, slot, 0, slot_status, rt_status_miss);
    store_word(mem, slot, control_base, control_done, 1);
    return traversal_complete_miss;
  }

  const uint32_t magic = mem.load32(accel_addr + 0);
  if (magic == as_magic)
    return trace_vtas(mem, slot, ray, accel_addr, skip_non_opaque);

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
      scene.primitive_addr == 0 || scene.primitive_stride == 0) {
    store_word(mem, slot, 0, slot_status, rt_status_miss);
    store_word(mem, slot, control_base, control_done, 1);
    return traversal_complete_miss;
  }

  if (geometry_type == geometry_triangle_list)
    return trace_triangle_list(mem, slot, ray, scene, skip_non_opaque);

  if (geometry_type == geometry_procedural_aabb_list)
    return trace_aabb_list(mem, slot, ray, scene, skip_non_opaque);

  store_word(mem, slot, 0, slot_status, rt_status_miss);
  store_word(mem, slot, control_base, control_done, 1);
  return traversal_complete_miss;
}

template <typename Memory>
inline void release(Memory &mem, reg_t slot)
{
  clear_control(mem, slot);
}

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

  uint32_t load32(reg_t addr)
  {
    return addr < 4096 ? slot.load32(addr) : raw.load32(addr);
  }

  void store32(reg_t addr, uint32_t value)
  {
    if (addr < 4096)
      slot.store32(addr, value);
    else
      raw.store32(addr, value);
  }
};

inline HybridMemory make_hybrid_memory(mmu_t &mmu, reg_t pds_base,
                                       reg_t num_warps, reg_t num_threads,
                                       reg_t tid, reg_t lane)
{
  return {{mmu, pds_base, num_warps, num_threads, tid, lane}, {mmu}};
}
#endif

} // namespace ventus_rt

#endif
