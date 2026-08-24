#define VENTUS_RT_STANDALONE
#include "ventus_rt_wavefront_worker.h"

#include <cassert>
#include <limits>
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
  address += as_header_size;
  write_vec3(memory, address + triangle_v0, -1.0f, -1.0f, z);
  write_vec3(memory, address + triangle_v1, 1.0f, -1.0f, z);
  write_vec3(memory, address + triangle_v2, 0.0f, 1.0f, z);
  memory.store_u32(address + triangle_primitive_id, 77);
  memory.store_u32(address + triangle_geometry_id, 5);
  memory.store_u32(address + triangle_sbt_record_offset, 4);
  memory.store_u32(address + triangle_flags, opaque);
}

static void write_identity_transform(TestMemory &memory, uint64_t address)
{
  for (uint32_t row = 0; row < 3; ++row)
    for (uint32_t column = 0; column < 4; ++column)
      memory.store_u32(address + 4 * (row * 4 + column),
                       bit_cast_u32(row == column ? 1.0f : 0.0f));
}

static void write_scene(TestMemory &memory, uint64_t accel, uint64_t triangles)
{
  constexpr uint64_t hit_sbt = 0x50000;
  memory.store_u32(accel + as_header_magic, as_magic);
  memory.store_u32(accel + as_header_version, as_version);
  memory.store_u32(accel + as_header_type, as_type_tlas);
  memory.store_u32(accel + as_header_root_node_ref,
                   uint32_t(as_header_size) | node_instance);
  const uint64_t instance = accel + as_header_size;
  memory.store_u32(instance + instance_blas_addr_lo, uint32_t(triangles));
  memory.store_u32(instance + instance_blas_addr_hi, uint32_t(triangles >> 32));
  memory.store_u32(instance + instance_mask, 0xff);
  memory.store_u32(instance + instance_sbt_record_offset, 0);
  memory.store_u32(instance + instance_instance_id, 11);
  write_identity_transform(memory, instance + instance_object_to_world);
  write_identity_transform(memory, instance + instance_world_to_object);

  memory.store_u32(triangles + as_header_magic, as_magic);
  memory.store_u32(triangles + as_header_version, as_version);
  memory.store_u32(triangles + as_header_type, as_type_blas);
  memory.store_u32(triangles + as_header_root_node_ref,
                   uint32_t(as_header_size) | node_triangle);
  /* Trace SBT offset 2 plus triangle SBT index 4 selects record 6. */
  memory.store_u32(hit_sbt + 6 * 96 + 4, 7);
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

static FieldMajorRecord make_trace_record(uint64_t accel)
{
  FieldMajorRecord record = {};
  const auto field = [&](TraceField index, uint32_t value) {
    record.fields[static_cast<uint32_t>(index)] = value;
  };
  field(TraceField::TlasAddrLo, uint32_t(accel));
  field(TraceField::TlasAddrHi, uint32_t(accel >> 32));
  field(TraceField::CullMask, 0xff);
  field(TraceField::SbtOffset, 2);
  field(TraceField::SbtStride, 96);
  field(TraceField::DirectionZ, bit_cast_u32(1.0f));
  field(TraceField::Tmax, bit_cast_u32(100.0f));
  field(TraceField::PayloadLo, 0x23456000);
  field(TraceField::PayloadHi, 0x1);
  field(TraceField::CpsFrame, 0x1000);
  field(TraceField::ParentFrame, 0x0800);
  field(TraceField::ContinuationId, 19);
  field(TraceField::CpsStackSize, 128);
  field(TraceField::LaunchIdX, 4);
  field(TraceField::LaunchIdY, 5);
  field(TraceField::LaunchIdZ, 6);
  field(TraceField::Depth, 3);
  return record;
}

static void check_candidate_actions(TestMemory &memory, uint64_t candidate_accel)
{
  CompletionArena<TestMemory> completion;
  const IndexedFieldMajorRecord candidate = {
      .ray_ref = 10, .record = make_trace_record(candidate_accel)};
  TraversalDispatchResult result =
      run_global_traversal_record(memory, candidate, completion);
  assert(result.target == TraversalDispatchTarget::AnyHitCandidate);
  CompletionRecord *record = completion.find(candidate.ray_ref);
  assert(record && record->state == CompletionState::Candidate);
  assert(record->candidate_hit[hit_record_primitive_id] == 77);
  assert(record->metadata.payload_address == 0x123456000ull);
  assert(record->metadata.cps_frame == 0x1000 &&
         record->metadata.parent_frame == 0x0800 && record->metadata.depth == 3);
  assert(record->metadata.return_continuation_id == 19 &&
         record->metadata.cps_stack_size == 128 &&
         record->metadata.launch_id_x == 4 && record->metadata.launch_id_y == 5 &&
         record->metadata.launch_id_z == 6);

  const IndexedFieldMajorRecord accept = {
      .ray_ref = 11, .record = make_trace_record(candidate_accel)};
  result = run_global_traversal_record(memory, accept, completion);
  assert(result.target == TraversalDispatchTarget::AnyHitCandidate);
  assert(completion.set_action(candidate.ray_ref, CompletionAction::Ignore));
  const std::vector<TraversalDispatchResult> ignored =
      resume_global_traversal_subset(memory, {candidate.ray_ref}, completion);
  assert(ignored.size() == 1);
  result = ignored.front();
  assert(result.target == TraversalDispatchTarget::Miss);
  assert(record->state == CompletionState::CompleteMiss);
  assert(record->committed_hit[hit_record_status] == 0);
  assert(record->metadata.return_continuation_id == 19 &&
         record->metadata.launch_id_x == 4 && record->metadata.launch_id_y == 5 &&
         record->metadata.launch_id_z == 6);
  assert(completion.find(accept.ray_ref)->state == CompletionState::Candidate);
  /* reportIntersectionEXT replaces the provisional candidate before any-hit
   * accepts it.  RTcore must commit report_t, not its paused candidate t. */
  record = completion.find(accept.ray_ref);
  std::array<uint32_t, kHitRecordWordCount> reported_hit = record->candidate_hit;
  std::array<uint32_t, kHitAttributeWordCount> reported_attributes = {{
      bit_cast_u32(0.2f), bit_cast_u32(0.3f)}};
  reported_hit[hit_record_hit_t] = bit_cast_u32(4.0f);
  reported_hit[hit_record_hit_kind] = 3;
  assert(completion.replace_candidate(accept.ray_ref, reported_hit,
                                      reported_attributes));
  assert(completion.set_action(accept.ray_ref, CompletionAction::AcceptContinue));
  result = resume_global_traversal_subset(memory, {accept.ray_ref}, completion)
               .front();
  assert(result.target == TraversalDispatchTarget::ClosestHit);
  record = completion.find(accept.ray_ref);
  assert(record->state == CompletionState::CompleteHit);
  assert(record->committed_hit[hit_record_primitive_id] == 77);
  assert(record->committed_hit[hit_record_hit_t] == bit_cast_u32(4.0f));
  assert(record->committed_attributes[0] ==
         record->committed_hit[hit_record_barycentrics_x]);

  const IndexedFieldMajorRecord terminate = {
      .ray_ref = 12, .record = make_trace_record(candidate_accel)};
  result = run_global_traversal_record(memory, terminate, completion);
  assert(result.target == TraversalDispatchTarget::AnyHitCandidate);
  assert(completion.set_action(terminate.ray_ref,
                               CompletionAction::AcceptTerminate));
  result = resume_global_traversal_record(memory, terminate.ray_ref, completion);
  assert(result.traversal_status == traversal_complete_hit);
  assert(result.target == TraversalDispatchTarget::ClosestHit);
  record = completion.find(terminate.ray_ref);
  assert(record->state == CompletionState::CompleteHit);
  assert(record->committed_hit[hit_record_primitive_id] == 77);
}

static void check_non_finite_triangle_is_miss(TestMemory &memory)
{
  constexpr uint64_t accel = 0x30000;
  constexpr uint64_t triangles = 0x31000;
  write_scene(memory, accel, triangles);
  write_triangle(memory, triangles, std::numeric_limits<float>::quiet_NaN(),
                 /* opaque */ 1);

  CompletionArena<TestMemory> completion;
  const IndexedFieldMajorRecord record = {
      .ray_ref = 99, .record = make_trace_record(accel)};
  const TraversalDispatchResult result =
      run_global_traversal_record(memory, record, completion);
  assert(result.target == TraversalDispatchTarget::Miss);
  const CompletionRecord *completion_record = completion.find(record.ray_ref);
  assert(completion_record &&
         completion_record->state == CompletionState::CompleteMiss);
}

static void check_vtas_aabb_candidate(TestMemory &memory)
{
  constexpr uint64_t tlas = 0x60000;
  constexpr uint64_t blas = 0x61000;
  constexpr uint64_t instance = tlas + as_header_size;
  constexpr uint64_t aabb = blas + as_header_size;
  constexpr uint32_t primitive_id = 19;

  memory.store_u32(tlas + as_header_magic, as_magic);
  memory.store_u32(tlas + as_header_version, as_version);
  memory.store_u32(tlas + as_header_type, as_type_tlas);
  memory.store_u32(tlas + as_header_root_node_ref,
                   uint32_t(instance - tlas) | node_instance);
  memory.store_u32(instance + instance_blas_addr_lo, uint32_t(blas));
  memory.store_u32(instance + instance_mask, 0xff);
  memory.store_u32(instance + instance_sbt_record_offset, 5);
  memory.store_u32(instance + instance_instance_id, 13);
  write_identity_transform(memory, instance + instance_object_to_world);
  write_identity_transform(memory, instance + instance_world_to_object);

  memory.store_u32(blas + as_header_magic, as_magic);
  memory.store_u32(blas + as_header_version, as_version);
  memory.store_u32(blas + as_header_type, as_type_blas);
  memory.store_u32(blas + as_header_root_node_ref,
                   uint32_t(aabb - blas) | node_aabb);
  write_vec3(memory, aabb + aabb_min, -1.0f, -1.0f, 4.0f);
  write_vec3(memory, aabb + aabb_max, 1.0f, 1.0f, 6.0f);
  memory.store_u32(aabb + aabb_primitive_id, primitive_id);
  memory.store_u32(aabb + aabb_geometry_id, 3);
  memory.store_u32(aabb + aabb_sbt_record_offset, 7);
  memory.store_u32(aabb + aabb_flags, 1);
  memory.store_u32(aabb + aabb_primitive_addr_lo, 0x70000);

  CompletionArena<TestMemory> completion;
  const IndexedFieldMajorRecord record = {
      .ray_ref = 123, .record = make_trace_record(tlas)};
  TraversalDispatchResult result =
      run_global_traversal_record(memory, record, completion);
  assert(result.target == TraversalDispatchTarget::IntersectionCandidate);
  CompletionRecord *candidate = completion.find(record.ray_ref);
  assert(candidate && candidate->state == CompletionState::Candidate);
  assert(candidate->candidate_hit[hit_record_primitive_id] == primitive_id);
  assert(candidate->candidate_hit[hit_record_instance_id] == 13);
  assert(candidate->candidate_hit[hit_record_sbt_index] == 14);
  assert(candidate->candidate_hit[hit_record_instance_sbt_record_offset] == 5);
  assert(candidate->candidate_hit[hit_record_opaque] == 1);

  std::array<uint32_t, kHitRecordWordCount> report = candidate->candidate_hit;
  report[hit_record_hit_t] = bit_cast_u32(4.5f);
  report[hit_record_hit_kind] = 0;
  assert(completion.replace_candidate(record.ray_ref, report, {}));
  assert(completion.set_action(record.ray_ref,
                               CompletionAction::AcceptContinue));
  result = resume_global_traversal_record(memory, record.ray_ref, completion);
  assert(result.target == TraversalDispatchTarget::ClosestHit);
  const CompletionRecord *completed = completion.find(record.ray_ref);
  assert(completed && completed->state == CompletionState::CompleteHit);
  assert(completed->committed_hit[hit_record_hit_t] == bit_cast_u32(4.5f));
}

static void check_global_consumer_facade(TestMemory &memory,
                                         uint64_t hit_accel)
{
  constexpr uint64_t queue_base = 0xa0000;
  constexpr uint64_t mailbox = 0xb0000;
  constexpr uint32_t generation = 9;
  GlobalLevelQueue<TestMemory> producer(memory, queue_base, /* capacity */ 4,
                                        generation);
  write_trace_mailbox(memory, mailbox, queue_base, generation, hit_accel);
  const auto field = [&](TraceField index, uint32_t value) {
    memory.store_u32(mailbox_field_address(mailbox, index), value);
  };
  field(TraceField::PayloadLo, 0x34567000);
  field(TraceField::PayloadHi, 0x2);
  field(TraceField::CpsFrame, 0x1234);
  field(TraceField::ContinuationId, 23);
  field(TraceField::CpsStackSize, 96);
  field(TraceField::LaunchIdX, 8);
  field(TraceField::LaunchIdY, 9);
  field(TraceField::LaunchIdZ, 10);
  assert(submit_rt_enqueue(producer, {mailbox}, 0x1, generation) ==
         LevelQueueResult::Accepted);

  GlobalWavefrontConsumer<TestMemory> consumer(memory);
  const std::vector<TraversalDispatchResult> results =
      consumer.consume_producer_phase(queue_base, /* max batch */ 1);
  assert(results.size() == 1);
  assert(results[0].target == TraversalDispatchTarget::ClosestHit);
  const CompletionPlaneLayout completion_layout = {
      .base_address = 0xc0000,
      .capacity = 4,
      .hit_attribute_base_address = 0xd0000,
      .hit_attribute_stride_bytes = 8,
      .hit_sbt_base_address = 0x50000,
      .hit_sbt_stride_bytes = 96,
  };
  assert(write_global_completion(memory, completion_layout, results[0],
                                 consumer.completion_arena()));
  const auto completion_field = [&](CompletionField field) {
    return memory.load_u32(
        completion_field_address(completion_layout, field, results[0].ray_ref));
  };
  const auto committed_hit_field = [&](uint32_t word) {
    return completion_field(static_cast<CompletionField>(
        static_cast<uint32_t>(CompletionField::CommittedHitBase) + word));
  };
  assert(completion_field(CompletionField::Status) == kCompletionStatusHit);
  assert(committed_hit_field(hit_record_primitive_id) == 77);
  assert(committed_hit_field(hit_record_hit_t) ==
         consumer.completion_arena()
             .find(results[0].ray_ref)
             ->committed_hit[hit_record_hit_t]);
  assert(completion_field(CompletionField::PayloadAddrLo) == 0x34567000);
  assert(completion_field(CompletionField::PayloadAddrHi) == 0x2);
  assert(completion_field(CompletionField::CpsFrame) == 0x1234);
  assert(completion_field(CompletionField::ContinuationId) == 23);
  assert(completion_field(CompletionField::LaunchIdX) == 8);
  assert(completion_field(CompletionField::CallbackGroup) == 7);
  assert(completion_field(CompletionField::Ready) == 1);
  assert(memory.load_u32(0xd0000) ==
         consumer.completion_arena()
             .find(results[0].ray_ref)
             ->committed_attributes[0]);

  const std::vector<ResumeDispatchRequest> requests =
      consumer.resume_requests(results);
  assert(requests.size() == 1);
  assert(requests[0].ray_ref == 0 &&
         requests[0].completed_stage == TraversalDispatchTarget::ClosestHit);
  assert(requests[0].callback_group == 7);
  assert(requests[0].payload_address == 0x234567000ull);
  assert(requests[0].cps_frame == 0x1234 &&
         requests[0].cps_stack_size == 96 &&
         requests[0].continuation_id == 23);
  assert(requests[0].launch_id_x == 8 && requests[0].launch_id_y == 9 &&
         requests[0].launch_id_z == 10);
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
  CompletionArena<TestMemory> completion;
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
  assert(dispatch_global_traversal_batch(queue, 32, completion).empty());
  assert(queue.seal_producer_phase());

  const std::vector<TraversalDispatchResult> results =
      dispatch_global_traversal_batch(queue, 32, completion);
  assert(results.size() == 3);
  assert(results[0].ray_ref == 0);
  assert(results[0].traversal_status == traversal_complete_hit);
  assert(results[0].target == TraversalDispatchTarget::ClosestHit);
  assert(results[0].primitive_id == 77 && results[0].instance_id == 11 &&
         results[0].geometry_id == 5 && results[0].sbt_index == 6);
  const CompletionRecord *opaque = completion.find(0);
  assert(opaque && opaque->state == CompletionState::CompleteHit);
  assert(opaque->committed_hit[hit_record_primitive_id] == 77);
  assert(results[1].ray_ref == 1);
  assert(results[1].traversal_status == traversal_complete_miss);
  assert(results[1].target == TraversalDispatchTarget::Miss);
  const CompletionRecord *miss = completion.find(1);
  assert(miss && miss->state == CompletionState::CompleteMiss);
  assert(results[2].ray_ref == 2);
  assert(results[2].traversal_status == traversal_candidate_non_opaque_triangle);
  assert(results[2].target == TraversalDispatchTarget::AnyHitCandidate);
  const CompletionRecord *candidate = completion.find(2);
  assert(candidate && candidate->state == CompletionState::Candidate);
  assert(candidate->candidate_hit[hit_record_primitive_id] == 77);
  const CompletionPlaneLayout candidate_layout = {
      .base_address = 0xc0000,
      .capacity = 8,
      .hit_attribute_base_address = 0xd0000,
      .hit_attribute_stride_bytes = 32,
      .candidate_hit_attribute_base_address = 0xe0000,
      .candidate_hit_attribute_stride_bytes = 32,
      .hit_sbt_base_address = 0x50000,
      .hit_sbt_stride_bytes = 96,
  };
  assert(write_global_completion(memory, candidate_layout, results[2],
                                 completion));
  const auto candidate_field = [&](CompletionField field) {
    return memory.load_u32(completion_field_address(candidate_layout, field, 2));
  };
  assert(candidate_field(CompletionField::Status) ==
         traversal_candidate_non_opaque_triangle);
  assert(candidate_field(CompletionField::CandidateKind) ==
         traversal_candidate_non_opaque_triangle);
  assert(candidate_field(static_cast<CompletionField>(
             static_cast<uint32_t>(CompletionField::CandidateHitBase) +
             hit_record_primitive_id)) == 77);
  assert(candidate_field(CompletionField::CandidateHitAttributeAddrLo) ==
         0xe0000 + 2 * 32);
  assert(candidate_field(CompletionField::CandidateControlBase) ==
         callback_accept);
  assert(candidate_field(CompletionField::Ready) == 1);

  assert(queue.consume_head() == 3);
  check_candidate_actions(memory, candidate_accel);
  check_non_finite_triangle_is_miss(memory);
  check_vtas_aabb_candidate(memory);
  check_global_consumer_facade(memory, hit_accel);
  return 0;
}
