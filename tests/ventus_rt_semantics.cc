#define VENTUS_RT_STANDALONE
#include "ventus_rtcore_model.h"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <unordered_map>

using namespace ventus_rt;

struct TestMemory {
  std::unordered_map<reg_t, uint32_t> words;
  uint64_t context_id = next_context_id++;
  static uint64_t next_context_id;

  uint64_t rt_context_key(reg_t slot) const
  {
    return (context_id << 32) ^ slot;
  }

  uint32_t load32(reg_t addr)
  {
    auto it = words.find(addr);
    return it == words.end() ? 0 : it->second;
  }

  void store32(reg_t addr, uint32_t value)
  {
    words[addr] = value;
  }
};

uint64_t TestMemory::next_context_id = 1;

struct CanonicalTestPdsMemory {
  TestMemory &memory;
  reg_t pds_address;

  uint32_t load32(reg_t address)
  {
    return memory.load32(address < rt_v5_pds_size_bytes
                             ? pds_address + address
                             : address);
  }

  void store32(reg_t address, uint32_t value)
  {
    memory.store32(address < rt_v5_pds_size_bytes
                       ? pds_address + address
                       : address,
                   value);
  }
};

static void store_slot(TestMemory &mem, reg_t slot, reg_t base, reg_t word,
                       uint32_t value)
{
  store_word(mem, slot, base, word, value);
}

static uint32_t load_slot(TestMemory &mem, reg_t slot, reg_t base, reg_t word)
{
  return load_word(mem, slot, base, word);
}

static void write_vec3(TestMemory &mem, reg_t addr, float x, float y, float z)
{
  mem.store32(addr + 0, bit_cast_u32(x));
  mem.store32(addr + 4, bit_cast_u32(y));
  mem.store32(addr + 8, bit_cast_u32(z));
}

static void write_triangle(TestMemory &mem, reg_t addr, float z,
                           uint32_t primitive_id, uint32_t sbt_index,
                           uint32_t opaque)
{
  write_vec3(mem, addr + 0, -1.0f, -1.0f, z);
  write_vec3(mem, addr + 12, 1.0f, -1.0f, z);
  write_vec3(mem, addr + 24, 0.0f, 1.0f, z);
  mem.store32(addr + 36, primitive_id);
  mem.store32(addr + 40, 7);
  mem.store32(addr + 44, 3);
  mem.store32(addr + 48, sbt_index);
  mem.store32(addr + 52, 0xfe);
  mem.store32(addr + 56, opaque);
  mem.store32(addr + 64, uint32_t(addr));
  mem.store32(addr + 68, 0);
  mem.store32(addr + 72, 0x1234);
  mem.store32(addr + 76, 0);
}

static void write_scene(TestMemory &mem, reg_t accel, reg_t tris,
                        uint32_t tri_count)
{
  mem.store32(accel + 0, bvh_magic);
  mem.store32(accel + 4, bvh_version);
  mem.store32(accel + 8, geometry_triangle_list);
  mem.store32(accel + 12, tri_count);
  mem.store32(accel + 16, uint32_t(tris));
  mem.store32(accel + 20, uint32_t(uint64_t(tris) >> 32));
  mem.store32(accel + 24, 80);
  mem.store32(accel + 28, 0x10000000);
  mem.store32(accel + 32, 1);
  mem.store32(accel + 36, 32);
}

static void write_aabb_scene(TestMemory &mem, reg_t accel, reg_t aabbs,
                             uint32_t aabb_count)
{
  mem.store32(accel + 0, bvh_magic);
  mem.store32(accel + 4, bvh_version);
  mem.store32(accel + 8, geometry_procedural_aabb_list);
  mem.store32(accel + 12, aabb_count);
  mem.store32(accel + 16, uint32_t(aabbs));
  mem.store32(accel + 20, uint32_t(uint64_t(aabbs) >> 32));
  mem.store32(accel + 24, 64);
  mem.store32(accel + 28, 0x10000000);
  mem.store32(accel + 32, 1);
  mem.store32(accel + 36, 32);
}

static void write_aabb(TestMemory &mem, reg_t addr, uint32_t primitive_id,
                       uint32_t sbt_index)
{
  write_vec3(mem, addr + 0, -1.0f, -1.0f, 2.0f);
  write_vec3(mem, addr + 12, 1.0f, 1.0f, 4.0f);
  mem.store32(addr + 24, primitive_id);
  mem.store32(addr + 28, 11);
  mem.store32(addr + 32, 5);
  mem.store32(addr + 36, sbt_index);
  mem.store32(addr + 40, 0xff);
  mem.store32(addr + 48, uint32_t(addr));
  mem.store32(addr + 52, 0);
  mem.store32(addr + 56, 0x5678);
  mem.store32(addr + 60, 0);
}

static void write_ray(TestMemory &mem, reg_t slot, reg_t accel)
{
  store_slot(mem, slot, 0, slot_status, slot_status_trace_request);
  store_slot(mem, slot, 0, slot_accel_lo, uint32_t(accel));
  store_slot(mem, slot, 0, slot_accel_hi, uint32_t(uint64_t(accel) >> 32));
  store_slot(mem, slot, 0, slot_cull_mask, 0xff);
  store_slot(mem, slot, 0, slot_sbt_offset, 2);
  store_slot(mem, slot, 0, slot_sbt_stride, 96);
  store_slot(mem, slot, 0, slot_origin_x, bit_cast_u32(0.0f));
  store_slot(mem, slot, 0, slot_origin_y, bit_cast_u32(0.0f));
  store_slot(mem, slot, 0, slot_origin_z, bit_cast_u32(0.0f));
  store_slot(mem, slot, 0, slot_tmin, bit_cast_u32(0.0f));
  store_slot(mem, slot, 0, slot_direction_x, bit_cast_u32(0.0f));
  store_slot(mem, slot, 0, slot_direction_y, bit_cast_u32(0.0f));
  store_slot(mem, slot, 0, slot_direction_z, bit_cast_u32(1.0f));
  store_slot(mem, slot, 0, slot_tmax, bit_cast_u32(100.0f));
}

static void write_v5_ray(TestMemory &mem, reg_t pds, reg_t accel)
{
  mem.store32(pds + rt_v5_pds_traversable_reference, uint32_t(accel));
  mem.store32(pds + rt_v5_pds_traversable_reference + 4,
              uint32_t(uint64_t(accel) >> 32));
  mem.store32(pds + rt_v5_pds_ray_flags, 0);
  const uint32_t selector = 0xffu | (2u << 8) | (2u << 12);
  mem.store32(pds + rt_v5_pds_trace_selector_control, selector);
  write_vec3(mem, pds + rt_v5_pds_world_origin, 0.0f, 0.0f, 0.0f);
  mem.store32(pds + rt_v5_pds_ray_tmin, bit_cast_u32(0.0f));
  write_vec3(mem, pds + rt_v5_pds_world_direction, 0.0f, 0.0f, 1.0f);
  mem.store32(pds + rt_v5_pds_ray_tmax, bit_cast_u32(100.0f));
  mem.store32(pds + rt_v5_pds_committed_base + rt_v5_hit_t,
              bit_cast_u32(100.0f));
  mem.store32(pds + rt_v5_pds_committed_base + rt_v5_hit_metadata, 0);
}

static void write_v5_private_input(TestMemory &mem, reg_t pds,
                                   reg_t private_address, uint32_t root_ref)
{
  mem.store32(private_address + rt_v5_private_frontier_0, root_ref);
  mem.store32(private_address + rt_v5_private_frontier_0 + 4, 0);
  mem.store32(private_address + rt_v5_private_ray_tmin,
              mem.load32(pds + rt_v5_pds_ray_tmin));
  mem.store32(private_address + rt_v5_private_ray_tmax,
              mem.load32(pds + rt_v5_pds_ray_tmax));
  mem.store32(private_address + rt_v5_private_ray_flags,
              mem.load32(pds + rt_v5_pds_ray_flags));

  uint32_t hot =
      mem.load32(pds + rt_v5_pds_trace_selector_control) & 0xffu;
  for (uint32_t axis = 0; axis < 3; ++axis) {
    const float direction = bit_cast_f32(
        mem.load32(pds + rt_v5_pds_world_direction + axis * 4));
    if (direction == 0.0f)
      hot |= uint32_t{1} << (14 + axis);
    mem.store32(private_address + rt_v5_private_origin + axis * 4,
                mem.load32(pds + rt_v5_pds_world_origin + axis * 4));
    mem.store32(private_address + rt_v5_private_inv_direction + axis * 4,
                bit_cast_u32(vtas_v2_inverse_direction(direction)));
    mem.store32(private_address + rt_v5_private_direction + axis * 4,
                bit_cast_u32(direction));
  }
  mem.store32(private_address + rt_v5_private_hot_control, hot);
  const uint64_t accel =
      uint64_t(mem.load32(pds + rt_v5_pds_traversable_reference)) |
      (uint64_t(mem.load32(pds + rt_v5_pds_traversable_reference + 4)) << 32);
  mem.store32(private_address + rt_v5_private_active_as_base,
              uint32_t(accel));
  mem.store32(private_address + rt_v5_private_active_as_base + 4,
              uint32_t(accel >> 32));
}

static void publish_v5_shader_accept(TestMemory &mem, reg_t pds,
                                     bool terminate = false)
{
  for (uint32_t offset = 0; offset < 96; offset += sizeof(uint32_t)) {
    mem.store32(pds + rt_v5_pds_committed_base + offset,
                mem.load32(pds + rt_v5_pds_callback_base + offset));
  }
  uint32_t metadata =
      mem.load32(pds + rt_v5_pds_committed_base + rt_v5_hit_metadata);
  mem.store32(pds + rt_v5_pds_committed_base + rt_v5_hit_metadata,
              metadata | rt_v5_hit_metadata_committed_valid);

  const uint32_t accepted_t =
      mem.load32(pds + rt_v5_pds_committed_base + rt_v5_hit_t);
  for (uint32_t offset = 8; offset < 32; offset += sizeof(uint32_t))
    mem.store32(pds + rt_v5_pds_callback_base + offset, 0);
  mem.store32(pds + rt_v5_pds_callback_base + 4, accepted_t);
  mem.store32(pds + rt_v5_pds_callback_base,
              terminate ? uint32_t{3} : uint32_t{1});
}

static void write_vtas_header(TestMemory &mem, reg_t addr, uint32_t type,
                              uint32_t root_ref)
{
  mem.store32(addr + as_header_magic, as_magic);
  mem.store32(addr + as_header_version, as_version);
  mem.store32(addr + as_header_type, type);
  mem.store32(addr + as_header_root_node_ref, root_ref);
}

static void write_vtas_v2_header(TestMemory &mem, reg_t addr, uint32_t type,
                                 uint32_t root_ref, uint32_t node_count,
                                 uint32_t region_size)
{
  mem.store32(addr + as_header_magic, as_magic);
  mem.store32(addr + as_header_version,
              as_version_v2 | (as_header_size << 16));
  mem.store32(addr + as_header_type, type);
  mem.store32(addr + as_header_root_node_ref, root_ref);
  mem.store32(addr + 0x14, node_count);
  mem.store32(addr + as_header_node_region_offset, as_header_size);
  mem.store32(addr + as_header_node_region_size, region_size);
}

static uint32_t make_node_ref(uint32_t offset, uint32_t type)
{
  return (offset & node_ref_offset_mask) | type;
}

static void write_vtas_triangle(TestMemory &mem, reg_t addr, float z,
                                uint32_t primitive_id, uint32_t sbt_offset,
                                uint32_t opaque)
{
  write_vec3(mem, addr + triangle_v0, -1.0f, -1.0f, z);
  write_vec3(mem, addr + triangle_v1, 1.0f, -1.0f, z);
  write_vec3(mem, addr + triangle_v2, 0.0f, 1.0f, z);
  mem.store32(addr + triangle_primitive_id, primitive_id);
  mem.store32(addr + triangle_geometry_id, 5);
  mem.store32(addr + triangle_sbt_record_offset, sbt_offset);
  mem.store32(addr + triangle_flags, opaque);
  mem.store32(addr + triangle_primitive_addr_lo, uint32_t(addr));
  mem.store32(addr + triangle_primitive_addr_lo + 4, 0);
}

static void write_vtas_instance(TestMemory &mem, reg_t addr, reg_t blas,
                                uint32_t instance_id,
                                uint32_t instance_sbt_offset,
                                uint32_t flags = 0)
{
  mem.store32(addr + instance_blas_addr_lo, uint32_t(blas));
  mem.store32(addr + instance_blas_addr_lo + 4, uint32_t(uint64_t(blas) >> 32));
  mem.store32(addr + instance_mask, 0xff);
  mem.store32(addr + instance_sbt_record_offset, instance_sbt_offset);
  mem.store32(addr + instance_flags, flags);
  mem.store32(addr + instance_instance_id, instance_id);
  for (uint32_t row = 0; row < 3; ++row) {
    for (uint32_t col = 0; col < 4; ++col) {
      const float value = row == col ? 1.0f : 0.0f;
      mem.store32(addr + instance_world_to_object + (row * 4 + col) * 4,
                  bit_cast_u32(value));
    }
  }
}

static void write_vtas_v2_box4(TestMemory &mem, reg_t as_base,
                               uint32_t node_offset,
                               const std::array<uint32_t, 4> &children,
                               uint32_t parent_ref, uint8_t parent_child,
                               bool root)
{
  const reg_t addr = as_base + node_offset;
  for (uint32_t child = 0; child < 4; ++child) {
    mem.store32(addr + box4_child_ref + child * 4, children[child]);
    if (children[child] == invalid_node_ref)
      continue;
    mem.store32(addr + box4_min_x + child * 4, bit_cast_u32(-1.0f));
    mem.store32(addr + box4_min_y + child * 4, bit_cast_u32(-1.0f));
    mem.store32(addr + box4_min_z + child * 4, bit_cast_u32(1.0f));
    mem.store32(addr + box4_max_x + child * 4, bit_cast_u32(1.0f));
    mem.store32(addr + box4_max_y + child * 4, bit_cast_u32(1.0f));
    mem.store32(addr + box4_max_z + child * 4, bit_cast_u32(9.0f));
  }
  mem.store32(addr + box4_v2_parent_node_ref, parent_ref);
  mem.store32(addr + box4_v2_parent_child_index,
              uint32_t(parent_child) |
                  (uint32_t(root ? box4_v2_node_flag_root : 0) << 8));
  mem.store32(addr + box4_v2_child_flags + 2, 0);
  mem.store32(addr + box4_v2_reserved + 2, 0);
}

static void write_vtas_aabb(TestMemory &mem, reg_t addr,
                            uint32_t primitive_id, uint32_t opaque)
{
  write_vec3(mem, addr + aabb_min, -1.0f, -1.0f, 2.0f);
  write_vec3(mem, addr + aabb_max, 1.0f, 1.0f, 4.0f);
  mem.store32(addr + aabb_primitive_id, primitive_id);
  mem.store32(addr + aabb_geometry_id, 6);
  mem.store32(addr + aabb_sbt_record_offset, 0);
  mem.store32(addr + aabb_hit_kind, 0xff);
  mem.store32(addr + aabb_flags, opaque);
  mem.store32(addr + aabb_primitive_addr_lo, uint32_t(addr));
  mem.store32(addr + aabb_primitive_addr_lo + 4, 0);
}

static void write_vtas_triangle_scene(TestMemory &mem, reg_t tlas, reg_t blas,
                                      uint32_t opaque,
                                      uint32_t instance_geometry_flags)
{
  write_vtas_header(mem, tlas, as_type_tlas,
                    make_node_ref(0x50, node_instance));
  write_vtas_instance(mem, tlas + 0x50, blas, 13, 4,
                      instance_geometry_flags);
  write_vtas_header(mem, blas, as_type_blas,
                    make_node_ref(0x50, node_triangle));
  write_vtas_triangle(mem, blas + 0x50, 5.0f, 77, 2, opaque);
}

static void write_vtas_aabb_scene(TestMemory &mem, reg_t tlas, reg_t blas,
                                  uint32_t opaque,
                                  uint32_t instance_geometry_flags)
{
  write_vtas_header(mem, tlas, as_type_tlas,
                    make_node_ref(0x50, node_instance));
  write_vtas_instance(mem, tlas + 0x50, blas, 17, 0,
                      instance_geometry_flags);
  write_vtas_header(mem, blas, as_type_blas,
                    make_node_ref(0x50, node_aabb));
  write_vtas_aabb(mem, blas + 0x50, 88, opaque);
}

static void check_vtas_tlas_blas_triangle_hit()
{
  TestMemory mem;
  constexpr reg_t slot = 0;
  constexpr reg_t tlas = 0x20000;
  constexpr reg_t tlas_instance = tlas + 0x50;
  constexpr reg_t blas = 0x21000;
  constexpr reg_t blas_triangle = blas + 0x50;

  write_vtas_header(mem, tlas, as_type_tlas,
                    make_node_ref(0x50, node_instance));
  write_vtas_instance(mem, tlas_instance, blas, 13, 4);
  write_vtas_header(mem, blas, as_type_blas,
                    make_node_ref(0x50, node_triangle));
  write_vtas_triangle(mem, blas_triangle, 5.0f, 77, 2, 1);
  mem.store32(blas_triangle + triangle_primitive_addr_lo, 0xdeadbeef);
  mem.store32(blas_triangle + triangle_primitive_addr_lo + 4, 0);
  write_ray(mem, slot, tlas);
  store_slot(mem, slot, 0, slot_sbt_stride, 3);

  assert(traverse(mem, slot) == traversal_complete_hit);
  assert(load_slot(mem, slot, committed_hit_record_base,
                   hit_record_primitive_id) == 77);
  assert(load_slot(mem, slot, committed_hit_record_base,
                   hit_record_instance_id) == 13);
  assert(load_slot(mem, slot, committed_hit_record_base,
                   hit_record_geometry_id) == 5);
  assert(load_slot(mem, slot, committed_hit_record_base,
                   hit_record_sbt_index) == 21);
  assert(load_slot(mem, slot, committed_hit_record_base,
                   hit_record_primitive_addr_lo) == blas_triangle);
  assert(load_slot(mem, slot, committed_hit_record_base,
                   hit_record_primitive_addr_hi) == 0);
  assert(std::fabs(bit_cast_f32(load_slot(mem, slot, committed_hit_record_base,
                                          hit_record_hit_t)) -
                   5.0f) < 0.001f);
}

static void check_vtas_v2_blas_internal_node_domain()
{
  TestMemory mem;
  constexpr reg_t slot = 0;
  constexpr reg_t tlas = 0x40000;
  constexpr reg_t blas = 0x50000;
  constexpr uint32_t instance_ref = 0x50 | node_instance;
  constexpr uint32_t box_ref = 0x80 | node_box4;
  constexpr uint32_t triangle_ref = 0x100 | node_triangle;

  write_vtas_v2_header(mem, tlas, as_type_tlas, instance_ref, 1, 128);
  write_vtas_instance(mem, tlas + 0x50, blas, 31, 2);
  mem.store32(tlas + 0x50 + instance_v2_parent_edge,
              instance_v2_root_parent_edge);

  write_vtas_v2_header(mem, blas, as_type_blas, box_ref, 2,
                       0x140 - as_header_size);
  write_vtas_v2_box4(
      mem, blas, 0x80,
      {triangle_ref, invalid_node_ref, invalid_node_ref, invalid_node_ref},
      invalid_node_ref, 0xff, true);
  write_vtas_triangle(mem, blas + 0x100, 5.0f, 91, 3, 0);
  write_ray(mem, slot, tlas);

  RtPrivateState state;
  assert(traverse(mem, slot, state) ==
         traversal_candidate_non_opaque_triangle);
  assert(load_slot(mem, slot, candidate_hit_record_base,
                   hit_record_primitive_id) == 91);
  store_slot(mem, slot, control_base, control_callback_decision,
             callback_ignore);
  assert(traverse(mem, slot, state) == traversal_complete_miss);
}

static void check_vtas_v2_fixed_stack_replay()
{
  TestMemory mem;
  constexpr reg_t slot = 0;
  constexpr reg_t tlas = 0x60000;
  constexpr reg_t blas = 0x70000;
  constexpr uint32_t depth = 8;
  constexpr uint32_t box_start = 0x100;
  constexpr uint32_t instance_start = box_start + depth * 0x80;
  constexpr uint32_t instance_count = depth + 1;
  constexpr uint32_t region_end =
      instance_start + instance_count * 0x80;

  write_vtas_v2_header(mem, blas, as_type_blas,
                       make_node_ref(0x50, node_triangle), 1, 64);
  write_vtas_triangle(mem, blas + 0x50, 5.0f, 177, 2, 0);

  write_vtas_v2_header(mem, tlas, as_type_tlas,
                       make_node_ref(box_start, node_box4),
                       depth + instance_count, region_end - as_header_size);
  for (uint32_t level = 0; level < depth; ++level) {
    const uint32_t box_offset = box_start + level * 0x80;
    const uint32_t deferred_instance = instance_start + level * 0x80;
    const uint32_t selected =
        level + 1 < depth
            ? make_node_ref(box_offset + 0x80, node_box4)
            : make_node_ref(instance_start + depth * 0x80, node_instance);
    const std::array<uint32_t, 4> children = {
        selected,
        make_node_ref(deferred_instance, node_instance),
        invalid_node_ref,
        invalid_node_ref,
    };
    write_vtas_v2_box4(
        mem, tlas, box_offset, children,
        level == 0 ? invalid_node_ref
                   : make_node_ref(box_offset - 0x80, node_box4),
        level == 0 ? 0xff : 0, level == 0);
  }
  for (uint32_t index = 0; index < instance_count; ++index) {
    const uint32_t instance_offset = instance_start + index * 0x80;
    write_vtas_instance(mem, tlas + instance_offset, blas, 100 + index, 4);
    const uint32_t parent_level = index == depth ? depth - 1 : index;
    const uint32_t child_index = index == depth ? 0 : 1;
    mem.store32(tlas + instance_offset + instance_v2_parent_edge,
                box_start + parent_level * 0x80 + child_index);
  }

  write_ray(mem, slot, tlas);
  RtPrivateState state;
  uint32_t status = traverse(mem, slot, state);
  assert(status == traversal_candidate_non_opaque_triangle);
  assert(state.context_count() == 1);
  const uint64_t key = rt_context_key(mem, slot, 0);
  const RtPrivateContext &paused = state.contexts().at(key);
  assert(paused.vtas_version == as_version_v2);
  assert(paused.stack.empty());
  assert(paused.v2_frontier.valid());
  assert(paused.v2_frontier.size() <= 6);
  assert(paused.v2_replay_count > 0);
  assert(paused.v2_overflow_count > 0);
  assert(paused.v2_cross_count > 0);

  uint32_t candidate_count = 0;
  uint32_t max_parent_bailouts = 0;
  std::array<bool, instance_count> seen_instances{};
  while (status == traversal_candidate_non_opaque_triangle) {
    const uint32_t instance_id = load_slot(
        mem, slot, candidate_hit_record_base, hit_record_instance_id);
    assert(instance_id >= 100 && instance_id < 100 + instance_count);
    const uint32_t instance_index = instance_id - 100;
    assert(!seen_instances[instance_index]);
    seen_instances[instance_index] = true;
    candidate_count++;
    max_parent_bailouts = std::max(
        max_parent_bailouts,
        state.contexts().at(key).v2_parent_bailout_count);
    store_slot(mem, slot, control_base, control_callback_decision,
               callback_ignore);
    status = traverse(mem, slot, state);
  }
  assert(status == traversal_complete_miss);
  assert(candidate_count == instance_count);
  for (bool seen : seen_instances)
    assert(seen);
  assert(max_parent_bailouts > 0);
  assert(state.context_count() == 0);

  const uint32_t deepest_instance = instance_start + depth * 0x80;
  mem.store32(tlas + deepest_instance + instance_v2_parent_edge,
              box_start + (depth - 1) * 0x80 + 4);
  write_ray(mem, slot, tlas);
  RtPrivateState malformed_state;
  bool rejected = false;
  try {
    (void)traverse(mem, slot, malformed_state);
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  assert(rejected);
  release(mem, slot, malformed_state);

  mem.store32(tlas + deepest_instance + instance_v2_parent_edge,
              box_start + (depth - 1) * 0x80);
  const uint32_t nonroot_box = box_start + 0x80;
  mem.store32(tlas + nonroot_box + box4_v2_parent_node_ref,
              invalid_node_ref);
  mem.store32(tlas + nonroot_box + box4_v2_parent_child_index,
              0xffu | (box4_v2_node_flag_root << 8));
  write_ray(mem, slot, tlas);
  RtPrivateState false_root_state;
  rejected = false;
  try {
    (void)traverse(mem, slot, false_root_state);
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  assert(rejected);

  mem.store32(tlas + nonroot_box + box4_v2_parent_node_ref,
              make_node_ref(box_start, node_box4));
  mem.store32(tlas + nonroot_box + box4_v2_parent_child_index, 0);
  mem.store32(tlas + box_start + box4_child_ref,
              make_node_ref(box_start, node_box4));
  write_ray(mem, slot, tlas);
  RtPrivateState cyclic_state;
  rejected = false;
  try {
    (void)traverse(mem, slot, cyclic_state);
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  assert(rejected);
}

static void check_rtcore_model_v5_vtas_v2_private_frontier()
{
  TestMemory mem;
  constexpr reg_t hardware_warp_id = 17;
  constexpr reg_t pds = 0x3000;
  constexpr reg_t private_address = 0x9000;
  constexpr reg_t tlas = 0x80000;
  constexpr reg_t blas = 0x81000;
  constexpr uint32_t instance_ref = 0x50 | node_instance;
  constexpr uint32_t triangle_ref = 0x50 | node_triangle;

  write_vtas_v2_header(mem, tlas, as_type_tlas, instance_ref, 1, 128);
  write_vtas_instance(mem, tlas + 0x50, blas, 55, 4);
  mem.store32(tlas + 0x50 + instance_v2_parent_edge,
              instance_v2_root_parent_edge);
  write_vtas_v2_header(mem, blas, as_type_blas, triangle_ref, 1, 64);
  write_vtas_triangle(mem, blas + 0x50, 5.0f, 188, 2, 0);
  write_v5_ray(mem, pds, tlas);
  write_v5_private_input(mem, pds, private_address, instance_ref);

  std::array<reg_t, v5_warp_lanes> pds_addresses{};
  std::array<reg_t, v5_warp_lanes> private_addresses{};
  pds_addresses[0] = pds;
  private_addresses[0] = private_address;
  auto make_memory = [&mem](reg_t, reg_t pds_address) {
    return CanonicalTestPdsMemory{mem, pds_address};
  };

  RtCoreModel model;
  auto results = model.executeTraverseV5(
      hardware_warp_id, 1, pds_addresses, private_addresses, make_memory);
  assert(results[0] ==
         (rt_v5_completion_valid | rt_v5_return_any_hit_required));
  const uint32_t candidate_metadata =
      mem.load32(pds + rt_v5_pds_callback_base + rt_v5_hit_metadata);
  assert((candidate_metadata & rt_v5_hit_metadata_geometry_mask) ==
         rt_v5_hit_geometry_triangle);
  assert(((candidate_metadata >> rt_v5_hit_metadata_hit_kind_shift) & 0xffu) ==
         triangle_hit_kind_front_face);
  assert((candidate_metadata & rt_v5_hit_metadata_committed_valid) == 0);
  const uint64_t raw_frontier =
      uint64_t(mem.load32(private_address)) |
      (uint64_t(mem.load32(private_address + 4)) << 32);
  vtas_v2::FrontierEntry frontier;
  assert(vtas_v2::decode_frontier_entry(raw_frontier, frontier));
  assert(frontier.kind == vtas_v2::FrontierKind::cross_as_return);
  assert(frontier.node_ref == instance_ref);
  for (uint32_t frontier_slot = 1; frontier_slot < 6; ++frontier_slot) {
    assert(mem.load32(private_address + frontier_slot * 8) == 0);
    assert(mem.load32(private_address + frontier_slot * 8 + 4) == 0);
  }
  assert(mem.load32(private_address + rt_v5_private_ray_tmin) ==
         bit_cast_u32(0.0f));
  assert(mem.load32(private_address + rt_v5_private_ray_tmax) ==
         bit_cast_u32(100.0f));
  assert(mem.load32(private_address + rt_v5_private_ray_flags) == 0);
  assert(mem.load32(private_address + rt_v5_private_hot_control) ==
         (0xffu | (uint32_t{1} << 9) | (uint32_t{1} << 14) |
          (uint32_t{1} << 15)));
  assert(mem.load32(private_address + rt_v5_private_inv_direction) ==
         bit_cast_u32(std::numeric_limits<float>::max()));
  assert(mem.load32(private_address + rt_v5_private_active_as_base) == blas);
  assert(mem.load32(private_address + rt_v5_private_active_as_base + 4) == 0);
  assert(mem.load32(private_address + rt_v5_private_instance_sbt_contribution)
         == 4);
  assert(mem.load32(private_address +
                    rt_v5_private_instance_metadata_reference) ==
         tlas + 0x50);
  assert(mem.load32(private_address + rt_v5_private_instance_index) == 55);
  assert(mem.load32(private_address + rt_v5_private_instance_custom_index) ==
         0);

  mem.store32(pds + rt_v5_pds_callback_base, 0);
  results = model.executeTraverseV5(
      hardware_warp_id, 1, pds_addresses, private_addresses, make_memory);
  assert(results[0] == (rt_v5_completion_valid | rt_v5_return_miss));
  model.executeReleaseV5(hardware_warp_id, 1, pds_addresses, make_memory);

  write_v5_ray(mem, pds, tlas);
  write_v5_private_input(mem, pds, private_address, instance_ref);
  results = model.executeTraverseV5(
      hardware_warp_id, 1, pds_addresses, private_addresses, make_memory);
  assert(results[0] ==
         (rt_v5_completion_valid | rt_v5_return_any_hit_required));
  publish_v5_shader_accept(mem, pds);
  results = model.executeTraverseV5(
      hardware_warp_id, 1, pds_addresses, private_addresses, make_memory);
  assert(results[0] ==
         (rt_v5_completion_valid | rt_v5_return_closest_hit_ready));
  const uint32_t committed_metadata =
      mem.load32(pds + rt_v5_pds_committed_base + rt_v5_hit_metadata);
  assert((committed_metadata & rt_v5_hit_metadata_committed_valid) != 0);
  assert(mem.load32(pds + rt_v5_pds_committed_base +
                    rt_v5_hit_primitive_index) == 188);
  assert(mem.load32(private_address + rt_v5_private_ray_tmax) ==
         bit_cast_u32(5.0f));
  assert((mem.load32(private_address + rt_v5_private_hot_control) &
          (uint32_t{1} << 8)) != 0);
  model.executeReleaseV5(hardware_warp_id, 1, pds_addresses, make_memory);

  write_v5_ray(mem, pds, tlas);
  write_v5_private_input(mem, pds, private_address, instance_ref);
  mem.store32(private_address + rt_v5_private_ray_tmin,
              bit_cast_u32(1.0f));
  bool rejected = false;
  try {
    (void)model.executeTraverseV5(
        hardware_warp_id, 1, pds_addresses, private_addresses, make_memory);
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  assert(rejected);
}

static void check_rtcore_model_v5_procedural_callback()
{
  TestMemory mem;
  constexpr reg_t hardware_warp_id = 18;
  constexpr reg_t pds = 0x5000;
  constexpr reg_t private_address = 0xa000;
  constexpr reg_t tlas = 0x82000;
  constexpr reg_t blas = 0x83000;
  constexpr uint32_t instance_ref = 0x50 | node_instance;
  constexpr uint32_t aabb_ref = 0x50 | node_aabb;

  write_vtas_v2_header(mem, tlas, as_type_tlas, instance_ref, 1, 128);
  write_vtas_instance(mem, tlas + 0x50, blas, 55, 4);
  mem.store32(tlas + 0x50 + instance_v2_parent_edge,
              instance_v2_root_parent_edge);
  write_vtas_v2_header(mem, blas, as_type_blas, aabb_ref, 1, 64);
  write_vtas_aabb(mem, blas + 0x50, 188, 0);
  write_v5_ray(mem, pds, tlas);
  write_v5_private_input(mem, pds, private_address, instance_ref);

  std::array<reg_t, v5_warp_lanes> pds_addresses{};
  std::array<reg_t, v5_warp_lanes> private_addresses{};
  pds_addresses[0] = pds;
  private_addresses[0] = private_address;
  auto make_memory = [&mem](reg_t, reg_t pds_address) {
    return CanonicalTestPdsMemory{mem, pds_address};
  };

  RtCoreModel model;
  auto results = model.executeTraverseV5(
      hardware_warp_id, 1, pds_addresses, private_addresses, make_memory);
  assert(results[0] ==
         (rt_v5_completion_valid | rt_v5_return_intersection_required));
  const reg_t callback = pds + rt_v5_pds_callback_base;
  assert(mem.load32(callback + rt_v5_hit_primitive_index) == 188);
  assert(mem.load32(callback + rt_v5_hit_instance_index) == 55);
  assert(mem.load32(callback + rt_v5_hit_geometry_index) == 6);
  const uint32_t candidate_metadata =
      mem.load32(callback + rt_v5_hit_metadata);
  assert((candidate_metadata & rt_v5_hit_metadata_geometry_mask) ==
         rt_v5_hit_geometry_procedural_aabb);
  assert((candidate_metadata & rt_v5_hit_metadata_committed_valid) == 0);

  mem.store32(callback + rt_v5_hit_t, bit_cast_u32(3.0f));
  publish_v5_shader_accept(mem, pds);
  results = model.executeTraverseV5(
      hardware_warp_id, 1, pds_addresses, private_addresses, make_memory);
  assert(results[0] ==
         (rt_v5_completion_valid | rt_v5_return_closest_hit_ready));
  const reg_t committed = pds + rt_v5_pds_committed_base;
  assert(mem.load32(committed + rt_v5_hit_primitive_index) == 188);
  assert(mem.load32(committed + rt_v5_hit_instance_index) == 55);
  assert(mem.load32(committed + rt_v5_hit_geometry_index) == 6);
  assert(mem.load32(committed + rt_v5_hit_t) == bit_cast_u32(3.0f));
  assert((mem.load32(committed + rt_v5_hit_metadata) &
          rt_v5_hit_metadata_committed_valid) != 0);
  assert(mem.load32(private_address + rt_v5_private_ray_tmax) ==
         bit_cast_u32(3.0f));
  model.executeReleaseV5(hardware_warp_id, 1, pds_addresses, make_memory);
}

static void check_triangle_facing_culling()
{
  constexpr reg_t slot = 0;
  constexpr reg_t tlas = 0x30000;
  constexpr reg_t blas = 0x31000;

  {
    TestMemory mem;
    write_vtas_triangle_scene(mem, tlas, blas, 1, 0);
    write_ray(mem, slot, tlas);
    store_slot(mem, slot, 0, slot_flags,
               ray_flag_cull_front_facing_triangles);

    assert(traverse(mem, slot) == traversal_complete_miss);
  }

  {
    TestMemory mem;
    write_vtas_triangle_scene(mem, tlas, blas, 1,
                              instance_flag_triangle_flip_facing);
    write_ray(mem, slot, tlas);
    store_slot(mem, slot, 0, slot_flags,
               ray_flag_cull_front_facing_triangles);

    assert(traverse(mem, slot) == traversal_complete_hit);
    assert(load_slot(mem, slot, committed_hit_record_base,
                     hit_record_front_face) == 0);
    assert(load_slot(mem, slot, committed_hit_record_base,
                     hit_record_hit_kind) ==
           triangle_hit_kind_back_face);
  }

  {
    TestMemory mem;
    write_vtas_triangle_scene(mem, tlas, blas, 1,
                              instance_flag_triangle_flip_facing);
    write_ray(mem, slot, tlas);
    store_slot(mem, slot, 0, slot_flags,
               ray_flag_cull_back_facing_triangles);

    assert(traverse(mem, slot) == traversal_complete_miss);
  }

  {
    TestMemory mem;
    write_vtas_triangle_scene(
        mem, tlas, blas, 1,
        instance_flag_triangle_flip_facing |
            instance_flag_triangle_facing_cull_disable);
    write_ray(mem, slot, tlas);
    store_slot(mem, slot, 0, slot_flags,
               ray_flag_cull_back_facing_triangles);

    assert(traverse(mem, slot) == traversal_complete_hit);
    assert(load_slot(mem, slot, committed_hit_record_base,
                     hit_record_front_face) == 0);
    assert(load_slot(mem, slot, committed_hit_record_base,
                     hit_record_hit_kind) ==
           triangle_hit_kind_back_face);
  }
}

static void check_effective_opacity_culling()
{
  constexpr reg_t slot = 0;

  {
    TestMemory mem;
    constexpr reg_t accel = 0x32000;
    constexpr reg_t tris = 0x33000;
    write_scene(mem, accel, tris, 1);
    write_triangle(mem, tris, 5.0f, 42, 0, 1);
    write_ray(mem, slot, accel);
    store_slot(mem, slot, 0, slot_flags, ray_flag_cull_opaque);

    assert(traverse(mem, slot) == traversal_complete_miss);
  }

  {
    TestMemory mem;
    constexpr reg_t accel = 0x34000;
    constexpr reg_t tris = 0x35000;
    write_scene(mem, accel, tris, 1);
    write_triangle(mem, tris, 5.0f, 42, 0, 0);
    write_ray(mem, slot, accel);
    store_slot(mem, slot, 0, slot_flags, ray_flag_cull_non_opaque);

    assert(traverse(mem, slot) == traversal_complete_miss);
  }

  {
    TestMemory mem;
    constexpr reg_t accel = 0x36000;
    constexpr reg_t aabbs = 0x37000;
    write_aabb_scene(mem, accel, aabbs, 1);
    write_aabb(mem, aabbs, 17, 0);
    write_ray(mem, slot, accel);
    store_slot(mem, slot, 0, slot_flags, ray_flag_cull_non_opaque);

    assert(traverse(mem, slot) == traversal_complete_miss);
  }

  {
    TestMemory mem;
    constexpr reg_t tlas = 0x38000;
    constexpr reg_t blas = 0x39000;
    write_vtas_triangle_scene(mem, tlas, blas, 1,
                              instance_flag_force_non_opaque);
    write_ray(mem, slot, tlas);

    assert(traverse(mem, slot) ==
           traversal_candidate_non_opaque_triangle);
    assert(load_slot(mem, slot, candidate_hit_record_base,
                     hit_record_opaque) == 0);
    release(mem, slot);
  }

  {
    TestMemory mem;
    constexpr reg_t tlas = 0x3a000;
    constexpr reg_t blas = 0x3b000;
    write_vtas_triangle_scene(mem, tlas, blas, 1,
                              instance_flag_force_non_opaque);
    write_ray(mem, slot, tlas);
    store_slot(mem, slot, 0, slot_flags,
               ray_flag_force_opaque | ray_flag_cull_opaque);

    assert(traverse(mem, slot) == traversal_complete_miss);
  }

  {
    TestMemory mem;
    constexpr reg_t tlas = 0x3c000;
    constexpr reg_t blas = 0x3d000;
    write_vtas_triangle_scene(mem, tlas, blas, 0,
                              instance_flag_force_opaque);
    write_ray(mem, slot, tlas);
    store_slot(mem, slot, 0, slot_flags,
               ray_flag_force_non_opaque | ray_flag_cull_non_opaque);

    assert(traverse(mem, slot) == traversal_complete_miss);
  }

  {
    TestMemory mem;
    constexpr reg_t tlas = 0x3e000;
    constexpr reg_t blas = 0x3f000;
    write_vtas_triangle_scene(mem, tlas, blas, 0,
                              instance_flag_force_opaque);
    write_ray(mem, slot, tlas);

    assert(traverse(mem, slot) == traversal_complete_hit);
    assert(load_slot(mem, slot, committed_hit_record_base,
                     hit_record_opaque) == 1);
  }

  {
    TestMemory mem;
    constexpr reg_t tlas = 0x40000;
    constexpr reg_t blas = 0x41000;
    write_vtas_aabb_scene(mem, tlas, blas, 1,
                          instance_flag_force_non_opaque);
    write_ray(mem, slot, tlas);
    store_slot(mem, slot, 0, slot_flags, ray_flag_cull_non_opaque);

    assert(traverse(mem, slot) == traversal_complete_miss);
  }
}

static void check_primitive_type_culling()
{
  constexpr reg_t slot = 0;

  {
    TestMemory mem;
    constexpr reg_t accel = 0x43000;
    constexpr reg_t tris = 0x44000;
    write_scene(mem, accel, tris, 1);
    write_triangle(mem, tris, 5.0f, 42, 0, 1);
    write_ray(mem, slot, accel);
    store_slot(mem, slot, 0, slot_flags, ray_flag_skip_triangles);

    assert(traverse(mem, slot) == traversal_complete_miss);
  }

  {
    TestMemory mem;
    constexpr reg_t accel = 0x45000;
    constexpr reg_t aabbs = 0x46000;
    write_aabb_scene(mem, accel, aabbs, 1);
    write_aabb(mem, aabbs, 17, 0);
    write_ray(mem, slot, accel);
    store_slot(mem, slot, 0, slot_flags, ray_flag_skip_aabbs);

    assert(traverse(mem, slot) == traversal_complete_miss);
  }

  {
    TestMemory mem;
    constexpr reg_t tlas = 0x47000;
    constexpr reg_t blas = 0x48000;
    write_vtas_triangle_scene(mem, tlas, blas, 1, 0);
    write_ray(mem, slot, tlas);
    store_slot(mem, slot, 0, slot_flags, ray_flag_skip_triangles);

    assert(traverse(mem, slot) == traversal_complete_miss);
  }

  {
    TestMemory mem;
    constexpr reg_t tlas = 0x49000;
    constexpr reg_t blas = 0x4a000;
    write_vtas_aabb_scene(mem, tlas, blas, 1, 0);
    write_ray(mem, slot, tlas);
    store_slot(mem, slot, 0, slot_flags, ray_flag_skip_aabbs);

    assert(traverse(mem, slot) == traversal_complete_miss);
  }
}

static void check_opaque_hit()
{
  TestMemory mem;
  constexpr reg_t slot = 0;
  constexpr reg_t accel = 0x10000;
  constexpr reg_t tris = 0x11000;

  write_scene(mem, accel, tris, 1);
  write_triangle(mem, tris, 5.0f, 42, 4, 1);
  write_ray(mem, slot, accel);

  assert(traverse(mem, slot) == traversal_complete_hit);
  assert(load_slot(mem, slot, committed_hit_record_base,
                   hit_record_primitive_id) == 42);
  assert(load_slot(mem, slot, committed_hit_record_base,
                   hit_record_sbt_index) == 6);
  assert(load_slot(mem, slot, committed_hit_record_base,
                   hit_record_shader_record_ptr_lo) == 0);
  assert(load_slot(mem, slot, committed_hit_record_base,
                   hit_record_shader_record_ptr_hi) == 0);
  assert(std::fabs(bit_cast_f32(load_slot(mem, slot, committed_hit_record_base,
                                          hit_record_hit_t)) -
                   5.0f) < 0.001f);
  assert(load_slot(mem, slot, committed_hit_record_base,
                   hit_record_status) == hit_record_valid);
}

static void check_miss()
{
  {
    TestMemory mem;
    constexpr reg_t slot = 0;
    write_ray(mem, slot, 0);

    assert(traverse(mem, slot) == traversal_complete_miss);
    assert(load_slot(mem, slot, 0, slot_status) == slot_status_trace_request);
  }

  {
    TestMemory mem;
    constexpr reg_t slot = 0;
    constexpr reg_t accel = 0x18000;
    constexpr reg_t tris = 0x19000;

    write_scene(mem, accel, tris, 1);
    write_triangle(mem, tris, 5.0f, 42, 4, 1);
    write_ray(mem, slot, accel);
    store_slot(mem, slot, 0, slot_origin_x, bit_cast_u32(4.0f));

    assert(traverse(mem, slot) == traversal_complete_miss);
    assert(load_slot(mem, slot, 0, slot_status) == slot_status_trace_request);
  }
}

static void check_closest_hit_wins()
{
  TestMemory mem;
  constexpr reg_t slot = 0;
  constexpr reg_t accel = 0x12000;
  constexpr reg_t tris = 0x13000;

  write_scene(mem, accel, tris, 2);
  write_triangle(mem, tris, 7.0f, 1, 0, 1);
  write_triangle(mem, tris + 80, 3.0f, 2, 0, 1);
  write_ray(mem, slot, accel);

  assert(traverse(mem, slot) == traversal_complete_hit);
  assert(load_slot(mem, slot, committed_hit_record_base,
                   hit_record_primitive_id) == 2);
}

static void check_candidate_replaces_farther_opaque_hit()
{
  TestMemory mem;
  constexpr reg_t slot = 0;
  constexpr reg_t accel = 0x13200;
  constexpr reg_t tris = 0x13300;

  /* The opaque hit is committed while traversing.  A nearer non-opaque
   * candidate must replace it only after the callback accepts it. */
  write_scene(mem, accel, tris, 2);
  write_triangle(mem, tris, 7.0f, 41, 0, 1);
  write_triangle(mem, tris + 80, 3.0f, 42, 0, 0);
  write_ray(mem, slot, accel);

  assert(traverse(mem, slot) == traversal_candidate_non_opaque_triangle);
  assert(load_slot(mem, slot, committed_hit_record_base,
                   hit_record_primitive_id) == 41);
  assert(load_slot(mem, slot, candidate_hit_record_base,
                   hit_record_primitive_id) == 42);

  store_slot(mem, slot, control_base, control_callback_decision,
             callback_accept);
  assert(traverse(mem, slot) == traversal_complete_hit);
  assert(load_slot(mem, slot, committed_hit_record_base,
                   hit_record_primitive_id) == 42);
  assert(std::fabs(bit_cast_f32(load_slot(mem, slot,
                                          committed_hit_record_base,
                                          hit_record_hit_t)) -
                   3.0f) < 0.001f);
}

static void check_non_opaque_candidate_accept_and_ignore()
{
  {
    TestMemory mem;
    constexpr reg_t slot = 0;
    constexpr reg_t accel = 0x14000;
    constexpr reg_t tris = 0x15000;

    write_scene(mem, accel, tris, 1);
    write_triangle(mem, tris, 2.0f, 9, 1, 0);
    write_ray(mem, slot, accel);

    assert(traverse(mem, slot) == traversal_candidate_non_opaque_triangle);
    assert(load_slot(mem, slot, candidate_hit_record_base,
                     hit_record_primitive_id) == 9);
    assert(load_slot(mem, slot, control_base, control_callback_decision) ==
           callback_pending);

    store_slot(mem, slot, control_base, control_callback_decision,
               callback_accept);
    assert(traverse(mem, slot) == traversal_complete_hit);
    assert(load_slot(mem, slot, committed_hit_record_base,
                     hit_record_primitive_id) == 9);
  }

  {
    TestMemory mem;
    constexpr reg_t slot = 0;
    constexpr reg_t accel = 0x16000;
    constexpr reg_t tris = 0x17000;

    write_scene(mem, accel, tris, 1);
    write_triangle(mem, tris, 2.0f, 9, 1, 0);
    write_ray(mem, slot, accel);

    assert(traverse(mem, slot) == traversal_candidate_non_opaque_triangle);
    store_slot(mem, slot, control_base, control_callback_decision,
               callback_ignore);
    assert(traverse(mem, slot) == traversal_complete_miss);
  }

  {
    TestMemory mem;
    constexpr reg_t slot = 0;
    constexpr reg_t accel = 0x18000;
    constexpr reg_t tris = 0x19000;

    write_scene(mem, accel, tris, 2);
    write_triangle(mem, tris, 2.0f, 9, 1, 0);
    write_triangle(mem, tris + 80, 3.0f, 10, 2, 0);
    write_ray(mem, slot, accel);

    assert(traverse(mem, slot) == traversal_candidate_non_opaque_triangle);
    assert(load_slot(mem, slot, candidate_hit_record_base,
                     hit_record_primitive_id) == 9);
    store_slot(mem, slot, control_base, control_callback_decision,
               callback_ignore);
    assert(traverse(mem, slot) == traversal_candidate_non_opaque_triangle);
    assert(load_slot(mem, slot, candidate_hit_record_base,
                     hit_record_primitive_id) == 10);
    release(mem, slot);
  }

  {
    TestMemory mem;
    constexpr reg_t slot = 0;
    constexpr reg_t accel = 0x1c000;
    constexpr reg_t tris = 0x1d000;

    write_scene(mem, accel, tris, 2);
    write_triangle(mem, tris, 7.0f, 11, 1, 0);
    write_triangle(mem, tris + 80, 3.0f, 12, 2, 0);
    write_ray(mem, slot, accel);

    assert(traverse(mem, slot) == traversal_candidate_non_opaque_triangle);
    assert(load_slot(mem, slot, candidate_hit_record_base,
                     hit_record_primitive_id) == 11);
    store_slot(mem, slot, control_base, control_callback_decision,
               callback_accept);
    assert(traverse(mem, slot) == traversal_candidate_non_opaque_triangle);
    assert(load_slot(mem, slot, candidate_hit_record_base,
                     hit_record_primitive_id) == 12);
    store_slot(mem, slot, control_base, control_callback_decision,
               callback_accept);
    assert(traverse(mem, slot) == traversal_complete_hit);
    assert(load_slot(mem, slot, committed_hit_record_base,
                     hit_record_primitive_id) == 12);
  }
}

static void check_terminate_and_release()
{
  TestMemory mem;
  constexpr reg_t slot = 0;
  constexpr reg_t accel = 0x22000;
  constexpr reg_t tris = 0x23000;

  write_scene(mem, accel, tris, 1);
  write_triangle(mem, tris, 2.0f, 31, 0, 0);
  write_ray(mem, slot, accel);
  assert(traverse(mem, slot) == traversal_candidate_non_opaque_triangle);
  store_slot(mem, slot, control_base, control_callback_decision,
             callback_terminate);
  assert(traverse(mem, slot) == traversal_complete_hit);
  assert(load_slot(mem, slot, committed_hit_record_base,
                   hit_record_status) == hit_record_valid);

  release(mem, slot);
  assert(load_slot(mem, slot, control_base, control_callback_decision) ==
         callback_pending);

  release(mem, slot);
  assert(load_slot(mem, slot, control_base, control_callback_decision) ==
         callback_pending);
}

static void check_invalid_callback_decision_fails_closed()
{
  TestMemory mem;
  constexpr reg_t slot = 0;
  constexpr reg_t accel = 0x26000;
  constexpr reg_t tris = 0x27000;

  write_scene(mem, accel, tris, 1);
  write_triangle(mem, tris, 2.0f, 33, 0, 0);
  write_ray(mem, slot, accel);

  RtCoreModel model;
  assert(model.executeTraverse(mem, slot) ==
         traversal_candidate_non_opaque_triangle);
  assert(model.debugSnapshot().private_context_count == 1);

  bool rejected = false;
  try {
    (void)model.executeTraverse(mem, slot);
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  assert(rejected);
  assert(model.debugSnapshot().private_context_count == 1);
  assert(load_slot(mem, slot, candidate_hit_record_base,
                   hit_record_status) == hit_record_valid);

  store_slot(mem, slot, control_base, control_callback_decision,
             callback_terminate + 1);
  rejected = false;
  try {
    (void)model.executeTraverse(mem, slot);
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  assert(rejected);
  assert(model.debugSnapshot().private_context_count == 1);

  store_slot(mem, slot, control_base, control_callback_decision,
             callback_accept);
  assert(model.executeTraverse(mem, slot) == traversal_complete_hit);
  assert(model.debugSnapshot().private_context_count == 0);
  assert(load_slot(mem, slot, control_base, control_callback_decision) ==
         callback_pending);
}

static void check_procedural_candidate_report_accept()
{
  {
    TestMemory mem;
    constexpr reg_t slot = 0;
    constexpr reg_t accel = 0x1a000;
    constexpr reg_t aabbs = 0x1b000;

    write_aabb_scene(mem, accel, aabbs, 1);
    write_aabb(mem, aabbs, 17, 3);
    write_ray(mem, slot, accel);

    assert(traverse(mem, slot) == traversal_candidate_procedural_aabb);
    assert(load_slot(mem, slot, candidate_hit_record_base,
                     hit_record_primitive_id) == 17);
    assert(load_slot(mem, slot, candidate_hit_record_base,
                     hit_record_need_software_opacity_test) == 1);

    store_slot(mem, slot, candidate_hit_record_base, hit_record_status,
               hit_record_valid);
    store_slot(mem, slot, candidate_hit_record_base, hit_record_hit_t,
               bit_cast_u32(2.5f));
    store_slot(mem, slot, candidate_hit_record_base, hit_record_hit_kind, 0xff);
    store_slot(mem, slot, candidate_hit_record_base, hit_record_front_face, 0);
    store_slot(mem, slot, candidate_hit_record_base, hit_record_opaque, 0);
    store_slot(mem, slot, candidate_hit_record_base,
               hit_record_need_software_opacity_test, 1);
    store_slot(mem, slot, control_base, control_callback_decision,
               callback_accept);

    assert(traverse(mem, slot) == traversal_complete_hit);
    assert(load_slot(mem, slot, committed_hit_record_base,
                     hit_record_primitive_id) == 17);
    assert(load_slot(mem, slot, committed_hit_record_base,
                     hit_record_hit_kind) == 0xff);
    assert(std::fabs(bit_cast_f32(load_slot(mem, slot, committed_hit_record_base,
                                            hit_record_hit_t)) -
                     2.5f) < 0.001f);
  }

  {
    TestMemory mem;
    constexpr reg_t slot = 0;
    constexpr reg_t accel = 0x1e000;
    constexpr reg_t aabbs = 0x1f000;

    write_aabb_scene(mem, accel, aabbs, 2);
    write_aabb(mem, aabbs, 21, 3);
    write_aabb(mem, aabbs + 64, 22, 4);
    write_ray(mem, slot, accel);

    assert(traverse(mem, slot) == traversal_candidate_procedural_aabb);
    assert(load_slot(mem, slot, candidate_hit_record_base,
                     hit_record_primitive_id) == 21);
    store_slot(mem, slot, control_base, control_callback_decision,
               callback_ignore);
    assert(traverse(mem, slot) == traversal_candidate_procedural_aabb);
    assert(load_slot(mem, slot, candidate_hit_record_base,
                     hit_record_primitive_id) == 22);
  }
}

static void check_pds_formula()
{
  constexpr reg_t pds = 0x80000000;
  constexpr reg_t num_warps = 8;
  constexpr reg_t num_threads = 32;
  constexpr reg_t tid = 64;
  constexpr reg_t lane = 3;
  constexpr reg_t logical = 112;
  constexpr reg_t expected =
      pds + num_warps * num_threads * logical + ((tid + lane) << 2);

  assert(pds_physical_addr(pds, num_warps, num_threads, tid, lane, logical) ==
         expected);

  TestMemory mem;
  const reg_t physical =
      pds_physical_addr(pds, num_warps, num_threads, tid, lane, logical);
  mem.store32(physical, 0xabcdef01);
  assert(mem.load32(physical) == 0xabcdef01);
}

static void check_rt_private_context_abi()
{
  TestMemory mem;
  constexpr reg_t slot = 0;
  constexpr reg_t accel = 0x24000;
  constexpr reg_t tris = 0x25000;

  write_scene(mem, accel, tris, 1);
  write_triangle(mem, tris, 2.0f, 41, 0, 0);
  write_ray(mem, slot, accel);
  assert(traverse(mem, slot) == traversal_candidate_non_opaque_triangle);
  store_slot(mem, slot, control_base, control_callback_decision,
             callback_ignore);
  assert(traverse(mem, slot) == traversal_complete_miss);

  assert(rt_region_size_bytes == 276);
  assert(cps_header_base == 96);
  assert(control_base == 112);
  assert(candidate_hit_record_base == 116);
  assert(committed_hit_record_base == 196);
  assert(hit_attrib_base == 276);
  for (const auto &word : mem.words)
    assert(word.first < rt_region_size_bytes || word.first >= accel);
}

static void check_rtcore_model_owns_private_state()
{
  TestMemory mem;
  constexpr reg_t slot0 = 0;
  constexpr reg_t slot2 = 0x1000;
  constexpr reg_t accel = 0x28000;
  constexpr reg_t tris = 0x29000;

  write_scene(mem, accel, tris, 1);
  write_triangle(mem, tris, 2.0f, 51, 0, 0);
  write_ray(mem, slot0, accel);
  write_ray(mem, slot2, accel);

  RtCoreModel model;
  assert(model.executeTraverse(mem, slot0) ==
         traversal_candidate_non_opaque_triangle);
  assert(model.executeTraverse(mem, slot2) ==
         traversal_candidate_non_opaque_triangle);

  RtCoreDebugSnapshot snapshot = model.debugSnapshot();
  assert(snapshot.traverse_command_count == 2);
  assert(snapshot.private_context_count == 2);

  model.executeRelease(mem, slot0);
  model.executeRelease(mem, slot2);
  snapshot = model.debugSnapshot();
  assert(snapshot.release_command_count == 2);
  assert(snapshot.private_context_count == 0);

  write_ray(mem, slot0, accel);
  assert(model.executeTraverse(mem, slot0) ==
         traversal_candidate_non_opaque_triangle);
  snapshot = model.debugSnapshot();
  assert(snapshot.private_context_count == 1);

  const uint64_t generation = snapshot.sm_generation;
  model.resetSm();
  snapshot = model.debugSnapshot();
  assert(snapshot.sm_generation == generation + 1);
  assert(snapshot.traverse_command_count == 0);
  assert(snapshot.release_command_count == 0);
}

static void check_rtcore_model_v5_lifecycle()
{
  TestMemory mem;
  constexpr reg_t hardware_warp_id = 9;
  constexpr reg_t accel = 0x2a000;
  constexpr reg_t tris = 0x2b000;
  constexpr reg_t empty_accel = 0x2c000;
  constexpr uint32_t original_mask = 0x7;

  std::array<reg_t, v5_warp_lanes> pds_addresses{};
  std::array<reg_t, v5_warp_lanes> private_state_addresses{};
  pds_addresses[0] = 0x1000;
  pds_addresses[1] = 0x1200;
  pds_addresses[2] = 0x1400;
  pds_addresses[3] = 0x1600;
  private_state_addresses[0] = 0x8000;
  private_state_addresses[1] = 0x8080;
  private_state_addresses[2] = 0x8100;
  private_state_addresses[3] = 0x8180;

  auto make_memory = [&mem](reg_t, reg_t pds_address) {
    return CanonicalTestPdsMemory{mem, pds_address};
  };

  write_scene(mem, accel, tris, 1);
  write_triangle(mem, tris, 2.0f, 61, 0, 0);
  write_scene(mem, empty_accel, tris, 0);
  write_v5_ray(mem, pds_addresses[0], accel);
  write_v5_ray(mem, pds_addresses[1], accel);
  write_v5_ray(mem, pds_addresses[2], empty_accel);

  RtCoreModel model;
  auto results = model.executeTraverseV5(
      hardware_warp_id, original_mask, pds_addresses,
      private_state_addresses, make_memory);
  assert(results[0] ==
         (rt_v5_completion_valid | rt_v5_return_any_hit_required));
  assert(results[1] ==
         (rt_v5_completion_valid | rt_v5_return_any_hit_required));
  assert(results[2] == (rt_v5_completion_valid | rt_v5_return_miss));

  const ResidentWarpRecord *record =
      model.residentWarpRecord(hardware_warp_id);
  assert(record != nullptr);
  assert(record->state == ResidentWarpState::Active);
  assert(record->original_bound_mask == original_mask);
  assert(record->live_request_mask == 0x3);
  assert(record->terminal_mask == 0x4);
  assert(record->software_pds_address[0] == pds_addresses[0]);
  assert(record->private_state_address[1] == private_state_addresses[1]);
  assert(record->requests[0].phase ==
         RequestControlPhase::PausedForSoftware);
  assert(record->requests[1].phase ==
         RequestControlPhase::PausedForSoftware);
  assert(record->requests[2].phase == RequestControlPhase::Free);
  assert(model.debugSnapshot().private_context_count == 2);

  bool rejected = false;
  try {
    model.executeReleaseV5(hardware_warp_id, original_mask, pds_addresses,
                           make_memory);
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  assert(rejected);
  assert(model.debugSnapshot().release_command_count == 0);

  rejected = false;
  try {
    (void)model.executeTraverseV5(hardware_warp_id, 0x8, pds_addresses,
                                  private_state_addresses, make_memory);
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  assert(rejected);

  auto wrong_private_addresses = private_state_addresses;
  wrong_private_addresses[0] += 128;
  rejected = false;
  try {
    (void)model.executeTraverseV5(hardware_warp_id, 0x1, pds_addresses,
                                  wrong_private_addresses, make_memory);
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  assert(rejected);

  mem.store32(pds_addresses[0] + rt_v5_pds_callback_base, 0);
  results = model.executeTraverseV5(hardware_warp_id, 0x1, pds_addresses,
                                    private_state_addresses, make_memory);
  assert(results[0] == (rt_v5_completion_valid | rt_v5_return_miss));
  record = model.residentWarpRecord(hardware_warp_id);
  assert(record->state == ResidentWarpState::Active);
  assert(record->original_bound_mask == original_mask);
  assert(record->live_request_mask == 0x2);
  assert(record->terminal_mask == 0x5);
  assert(record->requests[0].phase == RequestControlPhase::Free);
  assert(record->requests[1].phase ==
         RequestControlPhase::PausedForSoftware);

  mem.store32(pds_addresses[1] + rt_v5_pds_callback_base, 0);
  results = model.executeTraverseV5(hardware_warp_id, 0x2, pds_addresses,
                                    private_state_addresses, make_memory);
  assert(results[1] == (rt_v5_completion_valid | rt_v5_return_miss));
  record = model.residentWarpRecord(hardware_warp_id);
  assert(record->state == ResidentWarpState::TerminalResident);
  assert(record->original_bound_mask == original_mask);
  assert(record->live_request_mask == 0);
  assert(record->terminal_mask == original_mask);
  assert(model.debugSnapshot().private_context_count == 0);

  rejected = false;
  try {
    model.executeReleaseV5(hardware_warp_id, 0x3, pds_addresses,
                           make_memory);
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  assert(rejected);

  auto wrong_pds_addresses = pds_addresses;
  wrong_pds_addresses[1] += 0x200;
  rejected = false;
  try {
    model.executeReleaseV5(hardware_warp_id, original_mask,
                           wrong_pds_addresses, make_memory);
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  assert(rejected);
  record = model.residentWarpRecord(hardware_warp_id);
  assert(record->state == ResidentWarpState::TerminalResident);
  assert(record->terminal_mask == original_mask);

  model.executeReleaseV5(hardware_warp_id, original_mask, pds_addresses,
                         make_memory);
  record = model.residentWarpRecord(hardware_warp_id);
  assert(record->state == ResidentWarpState::Unbound);
  assert(record->original_bound_mask == 0);
  assert(model.debugSnapshot().traverse_command_count == 3);
  assert(model.debugSnapshot().release_command_count == 1);

  rejected = false;
  try {
    model.executeReleaseV5(hardware_warp_id, original_mask, pds_addresses,
                           make_memory);
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  assert(rejected);

  rejected = false;
  try {
    auto unaligned_private_addresses = private_state_addresses;
    unaligned_private_addresses[0] += 4;
    (void)model.executeTraverseV5(hardware_warp_id, 0x1, pds_addresses,
                                  unaligned_private_addresses, make_memory);
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  assert(rejected);
}

int main()
{
  check_opaque_hit();
  check_miss();
  check_closest_hit_wins();
  check_candidate_replaces_farther_opaque_hit();
  check_non_opaque_candidate_accept_and_ignore();
  check_terminate_and_release();
  check_invalid_callback_decision_fails_closed();
  check_procedural_candidate_report_accept();
  check_vtas_tlas_blas_triangle_hit();
  check_vtas_v2_blas_internal_node_domain();
  check_vtas_v2_fixed_stack_replay();
  check_triangle_facing_culling();
  check_effective_opacity_culling();
  check_primitive_type_culling();
  check_pds_formula();
  check_rt_private_context_abi();
  check_rtcore_model_owns_private_state();
  check_rtcore_model_v5_lifecycle();
  check_rtcore_model_v5_vtas_v2_private_frontier();
  check_rtcore_model_v5_procedural_callback();
  return 0;
}
