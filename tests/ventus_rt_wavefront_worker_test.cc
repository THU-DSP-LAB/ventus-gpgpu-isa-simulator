#define VENTUS_RT_STANDALONE
#include "ventus_rt_wavefront_worker.h"

#include <cassert>
#include <cstdint>
#include <unordered_map>
#include <vector>

using namespace ventus_rt;
using namespace ventus_rt_wavefront;

struct TestMemory {
  uint32_t load_u32(uint64_t address)
  {
    const auto it = words.find(address);
    return it == words.end() ? 0 : it->second;
  }
  void store_u32(uint64_t address, uint32_t value) { words[address] = value; }
  uint32_t atomic_fetch_add_u32(uint64_t address, uint32_t value)
  {
    const uint32_t previous = load_u32(address);
    store_u32(address, previous + value);
    return previous;
  }
  std::unordered_map<uint64_t, uint32_t> words;
};

static void write_vec3(TestMemory &memory, uint64_t address, float x, float y,
                       float z)
{
  memory.store_u32(address + 0, bit_cast_u32(x));
  memory.store_u32(address + 4, bit_cast_u32(y));
  memory.store_u32(address + 8, bit_cast_u32(z));
}

static void write_triangle(TestMemory &memory, uint64_t address, float z,
                           uint32_t opaque)
{
  write_vec3(memory, address + 0, -1.0f, -1.0f, z);
  write_vec3(memory, address + 12, 1.0f, -1.0f, z);
  write_vec3(memory, address + 24, 0.0f, 1.0f, z);
  memory.store_u32(address + 36, 77);
  memory.store_u32(address + 40, 11);
  memory.store_u32(address + 44, 5);
  memory.store_u32(address + 48, 4);
  memory.store_u32(address + 52, 0xfe);
  memory.store_u32(address + 56, opaque);
}

static void write_scene(TestMemory &memory, uint64_t accel, uint64_t triangles)
{
  memory.store_u32(accel + 0, bvh_magic);
  memory.store_u32(accel + 4, bvh_version);
  memory.store_u32(accel + 8, geometry_triangle_list);
  memory.store_u32(accel + 12, 1);
  memory.store_u32(accel + 16, uint32_t(triangles));
  memory.store_u32(accel + 20, uint32_t(triangles >> 32));
  memory.store_u32(accel + 24, 80);
  memory.store_u32(accel + 28, 0);
  memory.store_u32(accel + 32, 0);
  memory.store_u32(accel + 36, 32);
}

static void write_trace_mailbox(TestMemory &memory, uint64_t mailbox,
                                uint64_t queue_base, uint32_t generation,
                                uint64_t accel)
{
  const auto header = [&](uint32_t word, uint32_t value) {
    memory.store_u32(mailbox + 4u * word, value);
  };
  const auto field = [&](TraceField index, uint32_t value) {
    memory.store_u32(mailbox_field_address(mailbox, index), value);
  };
  header(kMailboxMagicWord, kMailboxMagic);
  header(kMailboxVersionWord, kMailboxAbiVersion);
  header(kMailboxGenerationWord, generation);
  header(kMailboxQueueBaseLoWord, uint32_t(queue_base));
  header(kMailboxQueueBaseHiWord, uint32_t(queue_base >> 32));
  field(TraceField::TlasAddrLo, uint32_t(accel));
  field(TraceField::TlasAddrHi, uint32_t(accel >> 32));
  field(TraceField::CullMask, 0xff);
  field(TraceField::SbtOffset, 2);
  field(TraceField::SbtStride, 96);
  field(TraceField::DirectionZ, bit_cast_u32(1.0f));
  field(TraceField::Tmax, bit_cast_u32(100.0f));
  field(TraceField::PhaseGeneration, generation);
  /* VALID is the producer's final store. */
  header(kMailboxValidWord, 1);
}

int main()
{
  TestMemory memory;
  constexpr uint64_t queue_base = 0x40000;
  constexpr uint64_t mailbox_base = 0x80000;
  constexpr uint32_t generation = 3;
  constexpr uint64_t hit_accel = 0x10000;
  constexpr uint64_t hit_triangles = 0x11000;
  constexpr uint64_t candidate_accel = 0x20000;
  constexpr uint64_t candidate_triangles = 0x21000;

  write_scene(memory, hit_accel, hit_triangles);
  write_triangle(memory, hit_triangles, 5.0f, /* opaque */ 1);
  write_scene(memory, candidate_accel, candidate_triangles);
  write_triangle(memory, candidate_triangles, 6.0f, /* opaque */ 0);

  GlobalLevelQueue<TestMemory> queue(memory, queue_base, /* capacity */ 8,
                                     generation);
  std::vector<uint64_t> mailboxes = {mailbox_base, mailbox_base + 0x100,
                                     mailbox_base + 0x200};
  write_trace_mailbox(memory, mailboxes[0], queue_base, generation, hit_accel);
  write_trace_mailbox(memory, mailboxes[1], queue_base, generation,
                      /* null TLAS is a miss */ 0);
  write_trace_mailbox(memory, mailboxes[2], queue_base, generation,
                      candidate_accel);
  assert(submit_rt_enqueue(queue, mailboxes, 0x7, generation) ==
         LevelQueueResult::Accepted);
  assert(queue.reserve_tail() == 3);
  /* A worker cannot dequeue before the producer level is sealed. */
  assert(dispatch_global_traversal_batch(queue, 32).empty());
  assert(queue.seal_producer_phase());

  const std::vector<TraversalDispatchResult> results =
      dispatch_global_traversal_batch(queue, 32);
  assert(results.size() == 3);
  assert(results[0].ray_ref == 0);
  assert(results[0].traversal_status == traversal_complete_hit);
  assert(results[0].target == TraversalDispatchTarget::ClosestHit);
  assert(results[0].primitive_id == 77 && results[0].instance_id == 11 &&
         results[0].geometry_id == 5 && results[0].sbt_index == 6);
  assert(results[1].ray_ref == 1);
  assert(results[1].traversal_status == traversal_complete_miss);
  assert(results[1].target == TraversalDispatchTarget::Miss);
  assert(results[2].ray_ref == 2);
  assert(results[2].traversal_status == traversal_candidate_non_opaque_triangle);
  assert(results[2].target == TraversalDispatchTarget::AnyHitCandidate);
  /* Candidate output is intentionally not committed/returned as a callback. */
  assert(queue.consume_head() == 3);
  return 0;
}
