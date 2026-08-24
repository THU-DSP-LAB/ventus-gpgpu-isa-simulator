#define VENTUS_RT_STANDALONE
#include "ventus_rt.h"

#include <cassert>
#include <cstdint>
#include <unordered_map>

using namespace ventus_rt;

struct TestMemory : RtMemory {
  TestMemory()
      : RtMemory(this, &TestMemory::load_callback, &TestMemory::store_callback,
                 &TestMemory::context_key_callback) {}

  std::unordered_map<reg_t, uint32_t> words;
  uint64_t context_id = next_context_id++;
  static uint64_t next_context_id;

  uint64_t rt_context_key(reg_t slot) const { return (context_id << 32) ^ slot; }
  uint32_t load32(reg_t addr)
  {
    const auto it = words.find(addr);
    return it == words.end() ? 0 : it->second;
  }
  void store32(reg_t addr, uint32_t value) { words[addr] = value; }

private:
  static uint32_t load_callback(void *state, reg_t addr)
  {
    return static_cast<TestMemory *>(state)->load32(addr);
  }
  static void store_callback(void *state, reg_t addr, uint32_t value)
  {
    static_cast<TestMemory *>(state)->store32(addr, value);
  }
  static uint64_t context_key_callback(void *state, reg_t slot)
  {
    return static_cast<TestMemory *>(state)->rt_context_key(slot);
  }
};

uint64_t TestMemory::next_context_id = 1;

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

static uint32_t make_node_ref(uint32_t offset, uint32_t type)
{
  return (offset & node_ref_offset_mask) | type;
}

static void write_header(TestMemory &mem, reg_t addr, uint32_t type,
                         uint32_t root_ref)
{
  mem.store32(addr + as_header_magic, as_magic);
  mem.store32(addr + as_header_version, as_version);
  mem.store32(addr + as_header_type, type);
  mem.store32(addr + as_header_root_node_ref, root_ref);
}

static void write_identity_transform(TestMemory &mem, reg_t addr)
{
  for (uint32_t row = 0; row < 3; ++row)
    for (uint32_t column = 0; column < 4; ++column)
      mem.store32(addr + 4 * (row * 4 + column),
                  bit_cast_u32(row == column ? 1.0f : 0.0f));
}

static void write_instance(TestMemory &mem, reg_t addr, reg_t blas,
                           uint32_t instance_id, uint32_t sbt_offset)
{
  mem.store32(addr + instance_blas_addr_lo, uint32_t(blas));
  mem.store32(addr + instance_blas_addr_hi, uint32_t(blas >> 32));
  mem.store32(addr + instance_mask, 0xff);
  mem.store32(addr + instance_sbt_record_offset, sbt_offset);
  mem.store32(addr + instance_instance_id, instance_id);
  write_identity_transform(mem, addr + instance_object_to_world);
  write_identity_transform(mem, addr + instance_world_to_object);
}

static void write_triangle(TestMemory &mem, reg_t addr, float z,
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
}

static void write_aabb(TestMemory &mem, reg_t addr, uint32_t primitive_id,
                       uint32_t sbt_offset, uint32_t opaque)
{
  write_vec3(mem, addr + aabb_min, -1.0f, -1.0f, 2.0f);
  write_vec3(mem, addr + aabb_max, 1.0f, 1.0f, 4.0f);
  mem.store32(addr + aabb_primitive_id, primitive_id);
  mem.store32(addr + aabb_geometry_id, 5);
  mem.store32(addr + aabb_sbt_record_offset, sbt_offset);
  mem.store32(addr + aabb_hit_kind, 0xff);
  mem.store32(addr + aabb_flags, opaque);
  mem.store32(addr + aabb_primitive_addr_lo, uint32_t(addr));
}

static void write_ray(TestMemory &mem, reg_t slot, reg_t tlas)
{
  store_slot(mem, slot, 0, slot_accel_lo, uint32_t(tlas));
  store_slot(mem, slot, 0, slot_accel_hi, uint32_t(tlas >> 32));
  store_slot(mem, slot, 0, slot_cull_mask, 0xff);
  store_slot(mem, slot, 0, slot_sbt_offset, 2);
  store_slot(mem, slot, 0, slot_sbt_stride, 96);
  store_slot(mem, slot, 0, slot_direction_z, bit_cast_u32(1.0f));
  store_slot(mem, slot, 0, slot_tmax, bit_cast_u32(100.0f));
  store_slot(mem, slot, 0, slot_status, slot_status_trace_request);
}

static void write_tlas_blas(TestMemory &mem, reg_t tlas, reg_t blas,
                            uint32_t leaf_type)
{
  write_header(mem, tlas, as_type_tlas,
               make_node_ref(as_header_size, node_instance));
  write_instance(mem, tlas + as_header_size, blas, 13, 4);
  write_header(mem, blas, as_type_blas,
               make_node_ref(as_header_size, leaf_type));
}

static void check_triangle_hit_and_candidate()
{
  TestMemory mem;
  constexpr reg_t slot = 0;
  constexpr reg_t tlas = 0x20000;
  constexpr reg_t blas = 0x21000;
  write_tlas_blas(mem, tlas, blas, node_triangle);
  write_triangle(mem, blas + as_header_size, 5.0f, 77, 2, 1);
  write_ray(mem, slot, tlas);

  assert(traverse(mem, slot) == traversal_complete_hit);
  assert(load_slot(mem, slot, abi_committed_hit_record_base_bytes,
                   hit_record_primitive_id) == 77);
  assert(load_slot(mem, slot, abi_committed_hit_record_base_bytes,
                   hit_record_instance_id) == 13);
  assert(load_slot(mem, slot, abi_committed_hit_record_base_bytes,
                   hit_record_sbt_index) == 8);

  TestMemory candidate;
  write_tlas_blas(candidate, tlas, blas, node_triangle);
  write_triangle(candidate, blas + as_header_size, 5.0f, 19, 1, 0);
  write_ray(candidate, slot, tlas);
  assert(traverse(candidate, slot) == traversal_candidate_non_opaque_triangle);
  store_slot(candidate, slot, abi_control_base_bytes, control_callback_decision,
             callback_ignore);
  assert(traverse(candidate, slot) == traversal_complete_miss);
}

static void check_aabb_candidate()
{
  TestMemory mem;
  constexpr reg_t slot = 0;
  constexpr reg_t tlas = 0x30000;
  constexpr reg_t blas = 0x31000;
  write_tlas_blas(mem, tlas, blas, node_aabb);
  write_aabb(mem, blas + as_header_size, 31, 6, 1);
  write_ray(mem, slot, tlas);

  assert(traverse(mem, slot) == traversal_candidate_procedural_aabb);
  assert(load_slot(mem, slot, abi_candidate_hit_record_base_bytes,
                   hit_record_primitive_id) == 31);
}

static void check_invalid_tlas_is_miss()
{
  TestMemory mem;
  constexpr reg_t slot = 0;
  write_ray(mem, slot, 0x40000);
  assert(traverse(mem, slot) == traversal_complete_miss);
}

static void check_pds_formula()
{
  constexpr reg_t pds = 0x80000000;
  assert(pds_physical_addr(pds, 8, 32, 64, 3, 112) ==
         pds + 8 * 32 * 112 + ((64 + 3) << 2));
}

int main()
{
  check_triangle_hit_and_candidate();
  check_aabb_candidate();
  check_invalid_tlas_is_miss();
  check_pds_formula();
  return 0;
}
