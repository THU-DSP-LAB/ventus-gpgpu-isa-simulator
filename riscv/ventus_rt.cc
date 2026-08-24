#include "ventus_rt.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <vector>

namespace ventus_rt {

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
  uint32_t instance_custom_index = 0;
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
  uint32_t instance_custom_index = 0;
  uint32_t geometry_id = 0;
  uint32_t sbt_index = 0;
  uint32_t instance_sbt_record_offset = 0;
  uint32_t hit_kind = 0xff;
  uint32_t opaque = 0;
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
  uint64_t hit_sbt_base = 0;
  uint32_t shader_group_handle_size = 0;
};

inline bool effective_opaque(const Ray &ray, uint32_t geometry_opaque,
                             uint32_t instance_flags = 0)
{
  const bool force_opaque =
      (ray.flags & ray_flag_force_opaque) != 0 ||
      (instance_flags & instance_flag_force_opaque) != 0;
  const bool force_non_opaque =
      (ray.flags & ray_flag_force_non_opaque) != 0 ||
      (instance_flags & instance_flag_force_non_opaque) != 0;
  return force_opaque || (!force_non_opaque && geometry_opaque != 0);
}

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

inline Hit hit_from_aabb(const ProceduralAabb &aabb, float hit_t)
{
  Hit hit;
  hit.valid = true;
  hit.t = hit_t;
  hit.front_face = true;
  hit.tri.primitive_id = aabb.primitive_id;
  hit.tri.instance_id = aabb.instance_id;
  hit.tri.instance_custom_index = aabb.instance_custom_index;
  hit.tri.geometry_id = aabb.geometry_id;
  hit.tri.sbt_index = aabb.sbt_index;
  hit.tri.instance_sbt_record_offset = aabb.instance_sbt_record_offset;
  hit.tri.hit_kind = aabb.hit_kind;
  hit.tri.opaque = aabb.opaque;
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
  store_word(mem, slot, abi_control_base_bytes, control_callback_decision, callback_pending);
}

template <typename Memory>
inline void write_hit_record(Memory &mem, reg_t slot, reg_t base,
                             const Scene &scene, const Hit &hit,
                             uint32_t status, bool opaque)
{
  const uint32_t sbt_index =
      hit.tri.instance_sbt_record_offset + hit.tri.sbt_index +
      load_word(mem, slot, 0, slot_sbt_offset);
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
  store_word(mem, slot, base, hit_record_instance_custom_index,
             hit.tri.instance_custom_index);
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
             opaque ? 0 : 1);
  store_word(mem, slot, base, hit_record_instance_sbt_record_offset,
             hit.tri.instance_sbt_record_offset);
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
  return load_word(mem, slot, abi_committed_hit_record_base_bytes, hit_record_status) ==
         hit_record_status_valid;
}

template <typename Memory>
inline float current_tmax(Memory &mem, reg_t slot)
{
  if (committed_hit_valid(mem, slot))
    return bit_cast_f32(load_word(mem, slot, abi_committed_hit_record_base_bytes,
                                  hit_record_hit_t));
  return bit_cast_f32(load_word(mem, slot, 0, slot_tmax));
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
inline uint32_t load_node_ref(Memory &mem, uint64_t as_base)
{
  return mem.load32(as_base + as_header_root_node_ref);
}

template <typename Memory>
inline Triangle load_vtas_triangle(Memory &mem, uint64_t as_base,
                                   uint32_t node_ref,
                                   uint32_t instance_id,
                                   uint32_t instance_custom_index,
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
  tri.instance_custom_index = instance_custom_index;
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
inline ProceduralAabb load_vtas_aabb(Memory &mem, uint64_t as_base,
                                     uint32_t node_ref, uint32_t instance_id,
                                     uint32_t instance_custom_index,
                                     uint32_t instance_sbt_offset,
                                     uint64_t instance_addr)
{
  const reg_t addr = as_base + node_ref_offset(node_ref);
  ProceduralAabb aabb;
  aabb.min = load_vec3(mem, addr + aabb_min);
  aabb.max = load_vec3(mem, addr + aabb_max);
  aabb.primitive_id = mem.load32(addr + aabb_primitive_id);
  aabb.instance_id = instance_id;
  aabb.instance_custom_index = instance_custom_index;
  aabb.geometry_id = mem.load32(addr + aabb_geometry_id);
  aabb.sbt_index = mem.load32(addr + aabb_sbt_record_offset);
  aabb.instance_sbt_record_offset = instance_sbt_offset;
  aabb.hit_kind = mem.load32(addr + aabb_hit_kind);
  aabb.opaque = mem.load32(addr + aabb_flags) & 0x1;
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


struct TraversalEntry {
  uint64_t as_base = 0;
  uint32_t node_ref = invalid_node_ref;
  Ray ray;
  uint32_t instance_id = 0;
  uint32_t instance_custom_index = 0;
  uint32_t instance_sbt_offset = 0;
  uint64_t instance_addr = 0;
  uint32_t instance_flags = 0;
};

template <typename Memory>
inline void commit_hit(Memory &mem, reg_t slot, const Scene &scene,
                       const Hit &hit, bool opaque)
{
  write_hit_record(mem, slot, abi_committed_hit_record_base_bytes, scene, hit,
                   hit_record_status_valid, opaque);
  store_word(mem, slot, 0, slot_hit_t, bit_cast_u32(hit.t));
  store_word(mem, slot, 0, slot_sbt_index,
             hit.tri.instance_sbt_record_offset + hit.tri.sbt_index +
                 load_word(mem, slot, 0, slot_sbt_offset));
}

enum class RtPrivateTraversalKind : uint8_t {
  vtas,
};

/* This is RTcore-private simulator state, not a shader-visible PDS stack.
 * A four-way BVH needs only O(depth) pending entries; cap it so a corrupt
 * host-side context can never turn into unbounded allocation or traversal. */
constexpr uint32_t rt_private_stack_limit = 4096;

struct RtPrivateContext {
  RtPrivateTraversalKind kind;
  Ray ray;
  Scene scene;
  std::vector<TraversalEntry> stack;
  uint32_t stack_entries = 0;
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

template <typename Memory>
inline std::unordered_map<uint64_t, RtPrivateContext> &rt_private_contexts()
{
  static std::unordered_map<uint64_t, RtPrivateContext> contexts;
  return contexts;
}

/* Private traversal state belongs to one simulated execution, not to the
 * shader-visible PDS storage.  A simulator may abort before a shader emits
 * vt.rt.release; its retained state must therefore be discarded before that
 * PDS address is reused by a different sim_t instance. */
template <typename Memory>
inline void clear_private_contexts()
{
  rt_private_contexts<Memory>().clear();
}

inline bool private_stack_consistent(const RtPrivateContext &ctx)
{
  return ctx.stack_entries <= rt_private_stack_limit &&
         ctx.stack.size() == ctx.stack_entries;
}

inline bool push_private_entry(RtPrivateContext &ctx,
                               const TraversalEntry &entry)
{
  if (!private_stack_consistent(ctx) ||
      ctx.stack_entries == rt_private_stack_limit)
    return false;
  ctx.stack.push_back(entry);
  ++ctx.stack_entries;
  return private_stack_consistent(ctx);
}

template <typename Memory>
inline uint32_t abort_private_traversal(Memory &mem, reg_t slot,
                                        uint64_t key)
{
  rt_private_contexts<Memory>().erase(key);
  clear_control(mem, slot);
  return traversal_terminated;
}

template <typename Memory>
inline void commit_private_candidate(Memory &mem, reg_t slot)
{
  const float hit_t = bit_cast_f32(load_word(mem, slot,
                                             abi_candidate_hit_record_base_bytes,
                                             hit_record_hit_t));
  const Ray ray = load_ray(mem, slot);
  const float prior_tmax = current_tmax(mem, slot);
  const bool closer = hit_t >= ray.tmin && hit_t < prior_tmax;
  bool compiler_committed = std::isfinite(hit_t) && hit_t >= ray.tmin &&
                           committed_hit_valid(mem, slot) &&
                           hit_t == prior_tmax;
  if (compiler_committed) {
    /* reportIntersectionEXT commits the reported candidate before traversal
     * resumes.  Treat that commit as authoritative only when every record
     * word matches; an equal-distance stale candidate must not be accepted. */
    for (reg_t word = hit_record_status;
         word <= hit_record_instance_sbt_record_offset; ++word) {
      if (load_word(mem, slot, abi_candidate_hit_record_base_bytes, word) !=
          load_word(mem, slot, abi_committed_hit_record_base_bytes, word)) {
        compiler_committed = false;
        break;
      }
    }
  }
  if (closer || compiler_committed) {
    /* RTcore owns a normal any-hit commit.  A procedural report already
     * copied its candidate into the committed record so that subsequent
     * reports compare against the real intersection t, not the AABB t. */
    if (closer) {
      copy_hit_record(mem, slot, abi_committed_hit_record_base_bytes,
                      abi_candidate_hit_record_base_bytes);
    }
    store_word(mem, slot, 0, slot_hit_t, bit_cast_u32(hit_t));
    store_word(mem, slot, 0, slot_sbt_index,
               load_word(mem, slot, abi_candidate_hit_record_base_bytes,
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
                                         uint64_t key)
{
  const bool hit = committed_hit_valid(mem, slot);
  if (!hit)
    store_word(mem, slot, abi_committed_hit_record_base_bytes, hit_record_status, 0);
  clear_control(mem, slot);
  rt_private_contexts<Memory>().erase(key);
  return hit ? traversal_complete_hit : traversal_complete_miss;
}

template <typename Memory>
inline uint32_t pause_private_candidate(Memory &mem, reg_t slot,
                                        const Scene &scene,
                                        const Hit &candidate,
                                        uint32_t status, bool opaque)
{
  write_hit_record(mem, slot, abi_candidate_hit_record_base_bytes, scene, candidate,
                   hit_record_status_valid, opaque);
  clear_control(mem, slot);
  return status;
}

template <typename Memory>
inline uint32_t trace_private_vtas(Memory &mem, reg_t slot, uint64_t key)
{
  RtPrivateContext &ctx = rt_private_contexts<Memory>().at(key);
  while (ctx.stack_entries != 0) {
    if (!private_stack_consistent(ctx))
      return abort_private_traversal(mem, slot, key);
    TraversalEntry entry = ctx.stack.back();
    ctx.stack.pop_back();
    --ctx.stack_entries;
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
      std::sort(hits, hits + hit_count,
                [](const ChildHit &a, const ChildHit &b) {
                  return a.t_near < b.t_near;
                });
      for (uint32_t i = hit_count; i > 0; --i)
        if (!push_private_entry(
                ctx, {entry.as_base, hits[i - 1].ref, entry.ray,
                      entry.instance_id, entry.instance_custom_index,
                      entry.instance_sbt_offset,
                      entry.instance_addr, entry.instance_flags}))
          return abort_private_traversal(mem, slot, key);
      continue;
    }

    if (type == node_instance) {
      const uint32_t mask = mem.load32(node_addr + instance_mask);
      if ((entry.ray.cull_mask & mask) == 0)
        continue;
      const uint64_t blas = load_u64(mem, node_addr + instance_blas_addr_lo);
      if (!blas || mem.load32(blas + as_header_magic) != as_magic ||
          (mem.load32(blas + as_header_version) & 0xffffu) != as_version ||
          mem.load32(blas + as_header_type) != as_type_blas)
        continue;
      Ray object_ray = entry.ray;
      object_ray.origin = transform_instance_point(
          mem, node_addr + instance_world_to_object, entry.ray.origin);
      object_ray.direction = transform_instance_vector(
          mem, node_addr + instance_world_to_object, entry.ray.direction);
      if (!push_private_entry(
              ctx, {blas, load_node_ref(mem, blas), object_ray,
                    mem.load32(node_addr + instance_instance_id),
                    mem.load32(node_addr + instance_custom_index),
                    mem.load32(node_addr + instance_sbt_record_offset),
                    node_addr, mem.load32(node_addr + instance_flags)}))
        return abort_private_traversal(mem, slot, key);
      continue;
    }

    if (type == node_aabb) {
      const ProceduralAabb aabb = load_vtas_aabb(
          mem, entry.as_base, entry.node_ref, entry.instance_id,
          entry.instance_custom_index,
          entry.instance_sbt_offset, entry.instance_addr);
      float hit_t = 0.0f;
      if (!intersect_aabb(entry.ray, aabb, hit_t) ||
          !private_candidate_is_before_tmax(mem, slot, hit_t))
        continue;
      const bool opaque = effective_opaque(entry.ray, aabb.opaque,
                                           entry.instance_flags);
      return pause_private_candidate(mem, slot, ctx.scene,
                                     hit_from_aabb(aabb, hit_t),
                                     traversal_candidate_procedural_aabb, opaque);
    }

    if (type != node_triangle)
      continue;
    const Triangle tri = load_vtas_triangle(mem, entry.as_base, entry.node_ref,
                                             entry.instance_id,
                                             entry.instance_custom_index,
                                             entry.instance_sbt_offset,
                                             entry.instance_addr);
    Hit candidate;
    candidate.t = entry.ray.tmax;
    if (!intersect_triangle(entry.ray, tri, candidate))
      continue;
    const bool opaque = effective_opaque(entry.ray, tri.opaque,
                                         entry.instance_flags);
    if (!opaque)
      return pause_private_candidate(mem, slot, ctx.scene, candidate,
                                     traversal_candidate_non_opaque_triangle, opaque);
    commit_hit(mem, slot, ctx.scene, candidate, opaque);
    if (entry.ray.flags & ray_flag_terminate_on_first_hit)
      return finish_private_traversal(mem, slot, key);
  }
  return finish_private_traversal(mem, slot, key);
}

template <typename Memory>
inline uint32_t traverse(Memory &mem, reg_t slot)
{
  const uint64_t key = rt_context_key(mem, slot, 0);
  std::unordered_map<uint64_t, RtPrivateContext> &contexts =
      rt_private_contexts<Memory>();
  auto it = contexts.find(key);
  if (it != contexts.end()) {
    const uint32_t callback =
        load_word(mem, slot, abi_control_base_bytes, control_callback_decision);
    if (callback == callback_accept || callback == callback_terminate)
      commit_private_candidate(mem, slot);
    if (callback == callback_terminate) {
      clear_control(mem, slot);
      contexts.erase(it);
      return traversal_terminated;
    }
    store_word(mem, slot, abi_candidate_hit_record_base_bytes, hit_record_status, 0);
    clear_control(mem, slot);
    return trace_private_vtas(mem, slot, key);
  }

  if (load_word(mem, slot, 0, slot_status) != slot_status_trace_request)
    return traversal_terminated;

  clear_control(mem, slot);
  store_word(mem, slot, abi_candidate_hit_record_base_bytes, hit_record_status, 0);
  store_word(mem, slot, abi_committed_hit_record_base_bytes, hit_record_status, 0);
  const Ray ray = load_ray(mem, slot);
  const uint64_t accel_addr = load_accel_addr(mem, slot);
  if (accel_addr == 0)
    return finish_private_traversal(mem, slot, key);

  if (mem.load32(accel_addr + as_header_magic) != as_magic ||
      (mem.load32(accel_addr + as_header_version) & 0xffffu) != as_version ||
      mem.load32(accel_addr + as_header_type) != as_type_tlas)
    return finish_private_traversal(mem, slot, key);

  RtPrivateContext ctx = {};
  ctx.ray = ray;
  ctx.scene.hit_sbt_base = 0;
  ctx.scene.shader_group_handle_size = 32;
  ctx.kind = RtPrivateTraversalKind::vtas;
  const uint32_t root_ref = load_node_ref(mem, accel_addr);
  if (!push_private_entry(ctx, {accel_addr, root_ref, ray, 0, 0, 0, 0, 0})) {
    clear_control(mem, slot);
    return traversal_terminated;
  }
  contexts.emplace(key, std::move(ctx));
  return trace_private_vtas(mem, slot, key);
}

template <typename Memory>
inline void release(Memory &mem, reg_t slot)
{
  rt_private_contexts<Memory>().erase(rt_context_key(mem, slot, 0));
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

  uint64_t rt_context_key(reg_t logical_slot) const
  {
    return slot.pds_addr(logical_slot);
  }

  uint32_t load32(reg_t addr)
  {
    if (addr < abi_fixed_header_size_bytes)
      return slot.load32(addr);
    return raw.load32(addr);
  }

  void store32(reg_t addr, uint32_t value)
  {
    if (addr < abi_fixed_header_size_bytes) {
      slot.store32(addr, value);
      return;
    }
    raw.store32(addr, value);
  }
};

/* Megakernel traversal contexts are keyed by a physical PDS slot.  The same
 * slot address is valid again for each sim_t batch, so its RTcore-private
 * state must not survive the simulator that created it. */
void reset_private_contexts_for_simulation()
{
  clear_private_contexts<HybridMemory>();
}

inline HybridMemory make_hybrid_memory(mmu_t &mmu, reg_t pds_base,
                                       reg_t num_warps, reg_t num_threads,
                                       reg_t tid, reg_t lane)
{
  return {{mmu, pds_base, num_warps, num_threads, tid, lane}, {mmu}};
}
#endif


namespace {

class CallbackMemory {
public:
  explicit CallbackMemory(RtMemory &memory) : memory_(memory) {}

  uint32_t load32(reg_t address) { return memory_.load32(address); }
  void store32(reg_t address, uint32_t value) { memory_.store32(address, value); }
  uint64_t rt_context_key(reg_t slot) const { return memory_.rt_context_key(slot); }

private:
  RtMemory &memory_;
};

} // namespace

uint32_t traverse(RtMemory &memory, reg_t slot)
{
  CallbackMemory adapter(memory);
  return traverse(adapter, slot);
}

void release(RtMemory &memory, reg_t slot)
{
  CallbackMemory adapter(memory);
  release(adapter, slot);
}

#ifndef VENTUS_RT_STANDALONE
uint32_t traverse_spike(mmu_t &mmu, reg_t pds_base, reg_t num_warps,
                        reg_t num_threads, reg_t tid, reg_t lane, reg_t slot)
{
  HybridMemory memory =
      make_hybrid_memory(mmu, pds_base, num_warps, num_threads, tid, lane);
  return traverse(memory, slot);
}

void release_spike(mmu_t &mmu, reg_t pds_base, reg_t num_warps,
                   reg_t num_threads, reg_t tid, reg_t lane, reg_t slot)
{
  HybridMemory memory =
      make_hybrid_memory(mmu, pds_base, num_warps, num_threads, tid, lane);
  release(memory, slot);
}
#endif

} // namespace ventus_rt
