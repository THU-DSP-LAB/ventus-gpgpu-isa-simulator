#define VENTUS_RT_STANDALONE
#include "ventus_rt.h"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <unordered_map>

using namespace ventus_rt;

struct TestMemory {
  std::unordered_map<reg_t, uint32_t> words;

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

static void write_vtas_header(TestMemory &mem, reg_t addr, uint32_t type,
                              uint32_t root_ref)
{
  mem.store32(addr + as_header_magic, as_magic);
  mem.store32(addr + as_header_version, as_version);
  mem.store32(addr + as_header_type, type);
  mem.store32(addr + as_header_root_node_ref, root_ref);
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
                                uint32_t instance_sbt_offset)
{
  mem.store32(addr + instance_blas_addr_lo, uint32_t(blas));
  mem.store32(addr + instance_blas_addr_lo + 4, uint32_t(uint64_t(blas) >> 32));
  mem.store32(addr + instance_mask, 0xff);
  mem.store32(addr + instance_sbt_record_offset, instance_sbt_offset);
  mem.store32(addr + instance_instance_id, instance_id);
  for (uint32_t row = 0; row < 3; ++row) {
    for (uint32_t col = 0; col < 4; ++col) {
      const float value = row == col ? 1.0f : 0.0f;
      mem.store32(addr + instance_world_to_object + (row * 4 + col) * 4,
                  bit_cast_u32(value));
    }
  }
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
  write_ray(mem, slot, tlas);

  assert(traverse(mem, slot) == traversal_complete_hit);
  assert(load_slot(mem, slot, committed_hit_record_base,
                   hit_record_primitive_id) == 77);
  assert(load_slot(mem, slot, committed_hit_record_base,
                   hit_record_instance_id) == 13);
  assert(load_slot(mem, slot, committed_hit_record_base,
                   hit_record_geometry_id) == 5);
  assert(load_slot(mem, slot, committed_hit_record_base,
                   hit_record_sbt_index) == 8);
  assert(std::fabs(bit_cast_f32(load_slot(mem, slot, committed_hit_record_base,
                                          hit_record_hit_t)) -
                   5.0f) < 0.001f);
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
                   hit_record_shader_record_ptr_lo) == 0x10000260);
  assert(load_slot(mem, slot, committed_hit_record_base,
                   hit_record_shader_record_ptr_hi) == 1);
  assert(std::fabs(bit_cast_f32(load_slot(mem, slot, committed_hit_record_base,
                                          hit_record_hit_t)) -
                   5.0f) < 0.001f);
  assert(load_slot(mem, slot, control_base, control_done) == 1);
}

static void check_miss()
{
  {
    TestMemory mem;
    constexpr reg_t slot = 0;
    write_ray(mem, slot, 0);

    assert(traverse(mem, slot) == traversal_complete_miss);
    assert(load_slot(mem, slot, 0, slot_status) == rt_status_miss);
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
    assert(load_slot(mem, slot, 0, slot_status) == rt_status_miss);
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
    assert(load_slot(mem, slot, control_base, control_incomplete) == 1);

    store_slot(mem, slot, control_base, control_accept_hit, 1);
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
    store_slot(mem, slot, control_base, control_ignore_hit, 1);
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
    store_slot(mem, slot, control_base, control_ignore_hit, 1);
    assert(traverse(mem, slot) == traversal_candidate_non_opaque_triangle);
    assert(load_slot(mem, slot, candidate_hit_record_base,
                     hit_record_primitive_id) == 10);
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
    store_slot(mem, slot, control_base, control_accept_hit, 1);
    assert(traverse(mem, slot) == traversal_candidate_non_opaque_triangle);
    assert(load_slot(mem, slot, candidate_hit_record_base,
                     hit_record_primitive_id) == 12);
    store_slot(mem, slot, control_base, control_accept_hit, 1);
    assert(traverse(mem, slot) == traversal_complete_hit);
    assert(load_slot(mem, slot, committed_hit_record_base,
                     hit_record_primitive_id) == 12);
  }
}

static void check_terminate_and_release()
{
  TestMemory mem;
  constexpr reg_t slot = 0;

  store_slot(mem, slot, control_base, control_terminate_ray, 1);
  assert(traverse(mem, slot) == traversal_terminated);
  assert(load_slot(mem, slot, control_base, control_done) == 1);

  release(mem, slot);
  assert(load_slot(mem, slot, control_base, control_done) == 0);
  assert(load_slot(mem, slot, control_base, control_terminate_ray) == 0);

  release(mem, slot);
  assert(load_slot(mem, slot, control_base, control_done) == 0);
  assert(load_slot(mem, slot, control_base, control_terminate_ray) == 0);
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
               rt_status_hit);
    store_slot(mem, slot, candidate_hit_record_base, hit_record_hit_t,
               bit_cast_u32(2.5f));
    store_slot(mem, slot, candidate_hit_record_base, hit_record_hit_kind, 0xff);
    store_slot(mem, slot, candidate_hit_record_base, hit_record_front_face, 0);
    store_slot(mem, slot, candidate_hit_record_base, hit_record_opaque, 0);
    store_slot(mem, slot, candidate_hit_record_base,
               hit_record_need_software_opacity_test, 1);
    store_slot(mem, slot, control_base, control_accept_hit, 1);

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
    store_slot(mem, slot, control_base, control_ignore_hit, 1);
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

int main()
{
  check_opaque_hit();
  check_miss();
  check_closest_hit_wins();
  check_non_opaque_candidate_accept_and_ignore();
  check_terminate_and_release();
  check_procedural_candidate_report_accept();
  check_vtas_tlas_blas_triangle_hit();
  check_pds_formula();
  return 0;
}
