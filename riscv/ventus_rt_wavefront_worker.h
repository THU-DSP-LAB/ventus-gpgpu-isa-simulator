/*
 * Copyright 2026
 * SPDX-License-Identifier: MIT
 *
 * PDS-free functional worker for the global wavefront queue.  The temporary
 * logical slot below exists only while one RTcore worker executes one queued
 * ray; it is neither shader-visible storage nor a PDS allocation.
 */

#ifndef RISCV_VENTUS_RT_WAVEFRONT_WORKER_H
#define RISCV_VENTUS_RT_WAVEFRONT_WORKER_H

#include "ventus_rt.h"
#include "ventus_rt_wavefront_queue.h"

#include <array>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace ventus_rt_wavefront {

enum class TraversalDispatchTarget {
  Miss,
  ClosestHit,
  AnyHitCandidate,
  IntersectionCandidate,
  Terminated,
  Fault,
};

/*
 * The queue record is deliberately only traversal input.  A completion record
 * is keyed by that stable queue ordinal, never by the physical worker lane.
 * This remains a functional Spike model until the device-global completion
 * ABI is shared with Mesa, but it keeps the ownership boundary explicit:
 * RTcore writes hit state, shaders own payload/CPS contents, and a dispatcher
 * selects the next shader from the resulting target.
 */
constexpr uint32_t kHitRecordWordCount =
    ventus_rt::hit_record_instance_sbt_record_offset + 1;
constexpr uint32_t kHitAttributeWordCount = 2;

enum class CompletionAction : uint32_t {
  None,
  Ignore,
  AcceptContinue,
  AcceptTerminate,
};

enum class CompletionState : uint32_t {
  InFlight,
  Candidate,
  CompleteMiss,
  CompleteHit,
  Fault,
};

struct CompletionMetadata {
  uint64_t payload_address = 0;
  uint32_t cps_frame = 0;
  uint32_t parent_frame = 0;
  uint32_t cps_stack_size = 0;
  uint32_t depth = 0;
  uint32_t return_continuation_id = 0;
  uint32_t launch_id_x = 0;
  uint32_t launch_id_y = 0;
  uint32_t launch_id_z = 0;
  /* Vulkan pipeline shader-group index decoded from the terminal SBT record. */
  uint32_t callback_group = 0;
};

struct CompletionRecord {
  CompletionState state = CompletionState::InFlight;
  CompletionAction action = CompletionAction::None;
  uint32_t candidate_status = 0;
  std::array<uint32_t, kHitRecordWordCount> candidate_hit{};
  std::array<uint32_t, kHitRecordWordCount> committed_hit{};
  std::array<uint32_t, kHitAttributeWordCount> candidate_attributes{};
  std::array<uint32_t, kHitAttributeWordCount> committed_attributes{};
  CompletionMetadata metadata;
  FieldMajorRecord trace_input;
};

/*
 * Shader-side any-hit/intersection execution is represented by setting an
 * action then calling resume().  In particular, AcceptTerminate first commits
 * the candidate, then stops traversal and routes its committed result to
 * closest-hit.  Ignore leaves the committed record unchanged.
 */
template <typename Memory>
class CompletionArena {
public:
  CompletionRecord &begin(uint32_t ray_ref, const FieldMajorRecord &input)
  {
    CompletionRecord &record = records_[ray_ref];
    record = {};
    record.state = CompletionState::InFlight;
    record.trace_input = input;
    record.metadata.payload_address =
        uint64_t(input.fields[static_cast<uint32_t>(TraceField::PayloadLo)]) |
        (uint64_t(input.fields[static_cast<uint32_t>(TraceField::PayloadHi)])
         << 32);
    record.metadata.cps_frame =
        input.fields[static_cast<uint32_t>(TraceField::CpsFrame)];
    record.metadata.parent_frame =
        input.fields[static_cast<uint32_t>(TraceField::ParentFrame)];
    record.metadata.depth =
        input.fields[static_cast<uint32_t>(TraceField::Depth)];
    record.metadata.return_continuation_id =
        input.fields[static_cast<uint32_t>(TraceField::ContinuationId)];
    record.metadata.cps_stack_size =
        input.fields[static_cast<uint32_t>(TraceField::CpsStackSize)];
    record.metadata.launch_id_x =
        input.fields[static_cast<uint32_t>(TraceField::LaunchIdX)];
    record.metadata.launch_id_y =
        input.fields[static_cast<uint32_t>(TraceField::LaunchIdY)];
    record.metadata.launch_id_z =
        input.fields[static_cast<uint32_t>(TraceField::LaunchIdZ)];
    return record;
  }

  CompletionRecord *find(uint32_t ray_ref)
  {
    const auto it = records_.find(ray_ref);
    return it == records_.end() ? nullptr : &it->second;
  }

  const CompletionRecord *find(uint32_t ray_ref) const
  {
    const auto it = records_.find(ray_ref);
    return it == records_.end() ? nullptr : &it->second;
  }

  bool set_action(uint32_t ray_ref, CompletionAction action)
  {
    CompletionRecord *record = find(ray_ref);
    if (!record || record->state != CompletionState::Candidate)
      return false;
    if (action != CompletionAction::Ignore &&
        action != CompletionAction::AcceptContinue &&
        action != CompletionAction::AcceptTerminate)
      return false;
    record->action = action;
    return true;
  }

  /* An intersection shader reports a new procedural candidate in the global
   * completion plane.  Rebind that candidate to the same ray_ref-owned
   * traversal context before any-hit decides whether RTcore may commit it. */
  bool replace_candidate(uint32_t ray_ref,
                         const std::array<uint32_t, kHitRecordWordCount> &hit,
                         const std::array<uint32_t, kHitAttributeWordCount> &attrs)
  {
    CompletionRecord *record = find(ray_ref);
    if (!record || record->state != CompletionState::Candidate)
      return false;
    record->candidate_status = ventus_rt::traversal_candidate_non_opaque_triangle;
    record->candidate_hit = hit;
    record->candidate_attributes = attrs;
    record->action = CompletionAction::None;
    return true;
  }

private:
  std::unordered_map<uint32_t, CompletionRecord> records_;
};

struct TraversalDispatchResult {
  uint32_t ray_ref = 0;
  uint32_t traversal_status = 0;
  TraversalDispatchTarget target = TraversalDispatchTarget::Fault;
  uint32_t primitive_id = 0;
  uint32_t instance_id = 0;
  uint32_t geometry_id = 0;
  uint32_t sbt_index = 0;
  uint32_t hit_t_bits = 0;
};

/*
 * `ventus_rt::traverse` uses its historical logical-slot helpers.  This
 * adapter maps those short logical addresses to per-call host state and sends
 * all other accesses to device-global Memory.  It has no pds_base, lane, or
 * CSR dependency.  The short-address reservation follows the existing
 * HybridMemory convention; Vulkan device allocations do not use that null
 * low-address range.
 */
template <typename Memory>
class WorkerLocalMemory : public ventus_rt::RtMemory {
public:
  WorkerLocalMemory(Memory &global_memory, uint64_t context_id)
      : RtMemory(this, &WorkerLocalMemory::load_callback,
                 &WorkerLocalMemory::store_callback,
                 &WorkerLocalMemory::context_key_callback),
        global_memory_(global_memory), context_id_(context_id) {}

  uint64_t rt_context_key(reg_t slot) const
  {
    return (context_id_ << 32) ^ slot;
  }

  uint32_t load32(reg_t address)
  {
    if (address < ventus_rt::abi_fixed_header_size_bytes) {
      const auto it = local_words_.find(address);
      return it == local_words_.end() ? 0 : it->second;
    }
    return global_memory_.load_u32(address);
  }

  void store32(reg_t address, uint32_t value)
  {
    if (address < ventus_rt::abi_fixed_header_size_bytes) {
      local_words_[address] = value;
      return;
    }
    global_memory_.store_u32(address, value);
  }

private:
  static uint32_t load_callback(void *state, reg_t address)
  {
    return static_cast<WorkerLocalMemory *>(state)->load32(address);
  }

  static void store_callback(void *state, reg_t address, uint32_t value)
  {
    static_cast<WorkerLocalMemory *>(state)->store32(address, value);
  }

  static uint64_t context_key_callback(void *state, reg_t slot)
  {
    return static_cast<WorkerLocalMemory *>(state)->rt_context_key(slot);
  }

  Memory &global_memory_;
  uint64_t context_id_;
  std::unordered_map<reg_t, uint32_t> local_words_;
};

template <typename Memory>
static inline void
seed_worker_slot(WorkerLocalMemory<Memory> &memory,
                 const FieldMajorRecord &record)
{
  using namespace ventus_rt;
  const auto field = [&](TraceField index) {
    return record.fields[static_cast<uint32_t>(index)];
  };
  const auto store_slot = [&](reg_t word, uint32_t value) {
    store_word(memory, /* temporary logical slot */ 0, 0, word, value);
  };

  store_slot(slot_status, slot_status_trace_request);
  store_slot(slot_accel_lo, field(TraceField::TlasAddrLo));
  store_slot(slot_accel_hi, field(TraceField::TlasAddrHi));
  store_slot(slot_flags, field(TraceField::Flags));
  store_slot(slot_cull_mask, field(TraceField::CullMask));
  store_slot(slot_sbt_offset, field(TraceField::SbtOffset));
  store_slot(slot_sbt_stride, field(TraceField::SbtStride));
  store_slot(slot_miss_index, field(TraceField::MissIndex));
  store_slot(slot_origin_x, field(TraceField::OriginX));
  store_slot(slot_origin_y, field(TraceField::OriginY));
  store_slot(slot_origin_z, field(TraceField::OriginZ));
  store_slot(slot_tmin, field(TraceField::Tmin));
  store_slot(slot_direction_x, field(TraceField::DirectionX));
  store_slot(slot_direction_y, field(TraceField::DirectionY));
  store_slot(slot_direction_z, field(TraceField::DirectionZ));
  store_slot(slot_tmax, field(TraceField::Tmax));
  store_slot(slot_payload_ptr_lo, field(TraceField::PayloadLo));
  store_slot(slot_payload_ptr_hi, field(TraceField::PayloadHi));
  store_word(memory, 0, abi_cps_header_base_bytes, cps_frame_base,
             field(TraceField::CpsFrame));
  store_word(memory, 0, abi_cps_header_base_bytes, cps_active_level,
             field(TraceField::Depth));
}

template <typename Memory>
static inline void
restore_completion_slot(WorkerLocalMemory<Memory> &memory,
                        const CompletionRecord &completion)
{
  using namespace ventus_rt;
  seed_worker_slot(memory, completion.trace_input);
  for (uint32_t word = 0; word < kHitRecordWordCount; ++word) {
    store_word(memory, 0, abi_candidate_hit_record_base_bytes, word,
               completion.candidate_hit[word]);
    store_word(memory, 0, abi_committed_hit_record_base_bytes, word,
               completion.committed_hit[word]);
  }
  for (uint32_t word = 0; word < kHitAttributeWordCount; ++word)
    store_word(memory, 0, abi_hit_attrib_base_bytes, word,
               completion.candidate_attributes[word]);
}

template <typename Memory>
static inline void
snapshot_completion_slot(WorkerLocalMemory<Memory> &memory,
                         CompletionRecord &completion, uint32_t status)
{
  using namespace ventus_rt;
  for (uint32_t word = 0; word < kHitRecordWordCount; ++word) {
    completion.candidate_hit[word] =
        load_word(memory, 0, abi_candidate_hit_record_base_bytes, word);
    completion.committed_hit[word] =
        load_word(memory, 0, abi_committed_hit_record_base_bytes, word);
  }
  for (uint32_t word = 0; word < kHitAttributeWordCount; ++word) {
    const uint32_t value = load_word(memory, 0, abi_hit_attrib_base_bytes, word);
    if (status == traversal_candidate_non_opaque_triangle ||
        status == traversal_candidate_procedural_aabb)
      completion.candidate_attributes[word] = value;
    /* The legacy functional traversal has one transient attribute slot.  The
     * committed record is authoritative once a later candidate overwrites it. */
    completion.committed_attributes[word] =
        completion.committed_hit[hit_record_barycentrics_x + word];
  }
}

template <typename Memory>
static inline TraversalDispatchResult
route_completion(uint32_t ray_ref, uint32_t status,
                 CompletionRecord &completion)
{
  using namespace ventus_rt;
  TraversalDispatchResult result{.ray_ref = ray_ref,
                                 .traversal_status = status};
  switch (status) {
  case traversal_complete_miss:
    completion.state = CompletionState::CompleteMiss;
    result.target = TraversalDispatchTarget::Miss;
    break;
  case traversal_complete_hit:
    completion.state = CompletionState::CompleteHit;
    result.target = TraversalDispatchTarget::ClosestHit;
    break;
  case traversal_candidate_non_opaque_triangle:
    completion.state = CompletionState::Candidate;
    completion.candidate_status = status;
    result.target = TraversalDispatchTarget::AnyHitCandidate;
    break;
  case traversal_candidate_procedural_aabb:
    completion.state = CompletionState::Candidate;
    completion.candidate_status = status;
    result.target = TraversalDispatchTarget::IntersectionCandidate;
    break;
  case traversal_terminated:
    /* AcceptTerminate commits first; it is a closest-hit result, not a miss. */
    if (completion.committed_hit[hit_record_status] == hit_record_status_valid) {
      completion.state = CompletionState::CompleteHit;
      result.traversal_status = traversal_complete_hit;
      result.target = TraversalDispatchTarget::ClosestHit;
    } else {
      completion.state = CompletionState::CompleteMiss;
      result.target = TraversalDispatchTarget::Terminated;
    }
    break;
  default:
    completion.state = CompletionState::Fault;
    result.target = TraversalDispatchTarget::Fault;
    return result;
  }

  if (result.target == TraversalDispatchTarget::ClosestHit) {
    result.primitive_id = completion.committed_hit[hit_record_primitive_id];
    result.instance_id = completion.committed_hit[hit_record_instance_id];
    result.geometry_id = completion.committed_hit[hit_record_geometry_id];
    result.sbt_index = completion.committed_hit[hit_record_sbt_index];
    result.hit_t_bits = completion.committed_hit[hit_record_hit_t];
  }
  return result;
}

template <typename Memory>
static inline TraversalDispatchResult
run_global_traversal_record(Memory &global_memory,
                            const IndexedFieldMajorRecord &entry,
                            CompletionArena<Memory> &completion_arena)
{
  using namespace ventus_rt;
  CompletionRecord &completion = completion_arena.begin(entry.ray_ref,
                                                          entry.record);
  WorkerLocalMemory<Memory> local_memory(global_memory,
                                          uint64_t(entry.ray_ref) + 1);
  restore_completion_slot(local_memory, completion);
  const uint32_t status = traverse(local_memory, /* temporary slot */ 0);
  snapshot_completion_slot(local_memory, completion, status);
  return route_completion<Memory>(entry.ray_ref, status, completion);
}

template <typename Memory>
static inline TraversalDispatchResult
resume_global_traversal_record(Memory &global_memory, uint32_t ray_ref,
                               CompletionArena<Memory> &completion_arena)
{
  using namespace ventus_rt;
  CompletionRecord *completion = completion_arena.find(ray_ref);
  if (!completion || completion->state != CompletionState::Candidate ||
      completion->action == CompletionAction::None)
    return {.ray_ref = ray_ref, .target = TraversalDispatchTarget::Fault};

  WorkerLocalMemory<Memory> local_memory(global_memory, uint64_t(ray_ref) + 1);
  restore_completion_slot(local_memory, *completion);
  switch (completion->action) {
  case CompletionAction::Ignore:
    store_word(local_memory, 0, abi_control_base_bytes, control_callback_decision,
               callback_ignore);
    break;
  case CompletionAction::AcceptContinue:
    store_word(local_memory, 0, abi_control_base_bytes, control_callback_decision,
               callback_accept);
    break;
  case CompletionAction::AcceptTerminate:
    store_word(local_memory, 0, abi_control_base_bytes, control_callback_decision,
               callback_terminate);
    break;
  case CompletionAction::None:
    break;
  }
  completion->action = CompletionAction::None;
  const uint32_t status = traverse(local_memory, /* temporary slot */ 0);
  snapshot_completion_slot(local_memory, *completion, status);
  return route_completion<Memory>(ray_ref, status, *completion);
}

/* Any-hit/intersection may leave only a subset of a dispatched wave active.
 * The scheduler supplies that compacted ray_ref list; each ray retains its
 * own traversal context and completion record while inactive candidates stay
 * parked in the arena. */
template <typename Memory>
static inline std::vector<TraversalDispatchResult>
resume_global_traversal_subset(Memory &global_memory,
                               const std::vector<uint32_t> &active_ray_refs,
                               CompletionArena<Memory> &completion_arena)
{
  std::vector<TraversalDispatchResult> results;
  results.reserve(active_ray_refs.size());
  for (uint32_t ray_ref : active_ray_refs)
    results.push_back(
        resume_global_traversal_record(global_memory, ray_ref, completion_arena));
  return results;
}

/*
 * One sealed level becomes a worker batch.  Shader execution remains outside
 * the worker, but it reports its candidate decision through CompletionArena
 * before resume_global_traversal_subset() returns the selected rays to RTcore.
 */
template <typename Memory>
static inline std::vector<TraversalDispatchResult>
dispatch_global_traversal_batch(GlobalLevelQueue<Memory> &queue,
                                uint32_t max_rays,
                                CompletionArena<Memory> &completion_arena)
{
  std::vector<TraversalDispatchResult> results;
  const std::vector<IndexedFieldMajorRecord> batch =
      queue.dequeue_indexed_batch(max_rays);
  results.reserve(batch.size());
  for (const IndexedFieldMajorRecord &entry : batch)
    results.push_back(run_global_traversal_record(queue.memory_for_worker(),
                                                   entry, completion_arena));
  return results;
}

/*
 * This is the host-facing handoff after RTcore has completed traversal.  The
 * consumer must invoke the selected miss/closest-hit stage first; only then
 * may it launch the raygen resume entry identified by continuation_id.  The
 * request carries no physical lane or PDS state, so an external scheduler may
 * compact and rebind it freely.
 */
struct ResumeDispatchRequest {
  uint32_t ray_ref = 0;
  TraversalDispatchTarget completed_stage = TraversalDispatchTarget::Fault;
  uint32_t callback_group = 0;
  uint64_t payload_address = 0;
  uint32_t cps_frame = 0;
  uint32_t cps_stack_size = 0;
  uint32_t continuation_id = 0;
  uint32_t launch_id_x = 0;
  uint32_t launch_id_y = 0;
  uint32_t launch_id_z = 0;
};

/* C++ mirror of the field-major completion plane in vt_rt_global_abi.h. */
enum class CompletionField : uint32_t {
  Status = 0,
  /* v9 exposes the canonical committed record contiguously. */
  CommittedHitBase,
  HitAttributeAddrLo = 1 + kHitRecordWordCount,
  HitAttributeAddrHi,
  PayloadAddrLo,
  PayloadAddrHi,
  CpsFrame,
  ContinuationId,
  LaunchIdX,
  LaunchIdY,
  LaunchIdZ,
  CallbackGroup,
  Ready,
  /* Keep the terminal ABI stable and append the paused candidate planes.
   * Candidate and committed records share ray_ref indexing but never storage. */
  CandidateKind,
  CandidateHitBase,
  CandidateHitAttributeAddrLo = 32 + kHitRecordWordCount,
  CandidateHitAttributeAddrHi,
  CandidateControlBase,
  CandidateControlLast = CandidateControlBase,
  Count,
};

constexpr uint32_t kCompletionFieldCount =
    static_cast<uint32_t>(CompletionField::Count);
constexpr uint32_t kCompletionHeaderBytes = kQueueHeaderBytes;
constexpr uint32_t kCompletionStatusMiss = 0;
constexpr uint32_t kCompletionStatusHit = 1;

struct CompletionPlaneLayout {
  uint64_t base_address = 0;
  uint32_t capacity = 0;
  /* Two u32 hit attributes per ray, at a caller-owned global stride. */
  uint64_t hit_attribute_base_address = 0;
  uint32_t hit_attribute_stride_bytes = 0;
  uint64_t candidate_hit_attribute_base_address = 0;
  uint32_t candidate_hit_attribute_stride_bytes = 0;
  uint64_t miss_sbt_base_address = 0;
  uint64_t miss_sbt_stride_bytes = 0;
  uint64_t hit_sbt_base_address = 0;
  uint64_t hit_sbt_stride_bytes = 0;
};

static inline uint64_t
completion_field_stride_bytes(uint32_t capacity)
{
  return align_queue_bytes((uint64_t)capacity * sizeof(uint32_t));
}

static inline uint64_t
completion_field_address(const CompletionPlaneLayout &layout,
                         CompletionField field, uint32_t ray_ref)
{
  return layout.base_address + kCompletionHeaderBytes +
         (uint64_t)static_cast<uint32_t>(field) *
             completion_field_stride_bytes(layout.capacity) +
         (uint64_t)ray_ref * sizeof(uint32_t);
}

constexpr uint32_t kShaderGroupHandleIndexOffset = 4;
constexpr uint32_t kShaderGroupHandleSize = 32;

template <typename Memory>
static inline bool
resolve_callback_group(Memory &memory, const CompletionPlaneLayout &layout,
                       const TraversalDispatchResult &result,
                       CompletionRecord &completion, uint32_t *group)
{
  if (!group)
    return false;

  if (result.target == TraversalDispatchTarget::Miss) {
    if (!layout.miss_sbt_base_address || !layout.miss_sbt_stride_bytes)
      return false;
    const uint64_t record = layout.miss_sbt_base_address +
      (uint64_t)completion.trace_input.fields[
         static_cast<uint32_t>(TraceField::MissIndex)] *
         layout.miss_sbt_stride_bytes;
    *group = memory.load_u32(record + kShaderGroupHandleIndexOffset);
    return true;
  }

  const bool candidate =
      result.target == TraversalDispatchTarget::AnyHitCandidate ||
      result.target == TraversalDispatchTarget::IntersectionCandidate;
  if (result.target != TraversalDispatchTarget::ClosestHit && !candidate)
    return false;
  if (!layout.hit_sbt_base_address || !layout.hit_sbt_stride_bytes)
    return false;
  std::array<uint32_t, kHitRecordWordCount> &hit = candidate
      ? completion.candidate_hit : completion.committed_hit;
  const uint32_t sbt_index = hit[ventus_rt::hit_record_sbt_index];
  const uint64_t shader_record = layout.hit_sbt_base_address +
    (uint64_t)sbt_index * layout.hit_sbt_stride_bytes +
    kShaderGroupHandleSize;
  hit[ventus_rt::hit_record_shader_record_ptr_lo] =
    uint32_t(shader_record);
  hit[ventus_rt::hit_record_shader_record_ptr_hi] =
    uint32_t(shader_record >> 32);
  *group = memory.load_u32(shader_record - kShaderGroupHandleSize +
                           kShaderGroupHandleIndexOffset);
  return true;
}

template <typename Memory>
static inline bool
write_global_completion(Memory &memory, const CompletionPlaneLayout &layout,
                        const TraversalDispatchResult &result,
                        CompletionArena<Memory> &completion_arena)
{
  if (!layout.base_address || !layout.capacity || result.ray_ref >= layout.capacity)
    return false;
  const bool candidate =
      result.target == TraversalDispatchTarget::AnyHitCandidate ||
      result.target == TraversalDispatchTarget::IntersectionCandidate;
  if (result.target != TraversalDispatchTarget::Miss &&
      result.target != TraversalDispatchTarget::ClosestHit && !candidate)
    return false;

  CompletionRecord *completion = completion_arena.find(result.ray_ref);
  if (!completion)
    return false;
  if (!resolve_callback_group(memory, layout, result, *completion,
                              &completion->metadata.callback_group))
    return false;

  uint64_t hit_attribute_address = 0;
  uint64_t candidate_hit_attribute_address = 0;
  if (result.target == TraversalDispatchTarget::ClosestHit || candidate) {
    if (!layout.hit_attribute_base_address ||
        layout.hit_attribute_stride_bytes <
            kHitAttributeWordCount * sizeof(uint32_t))
      return false;
    hit_attribute_address = layout.hit_attribute_base_address +
                            (uint64_t)result.ray_ref *
                                layout.hit_attribute_stride_bytes;
    memory.store_u32(hit_attribute_address, completion->committed_attributes[0]);
    memory.store_u32(hit_attribute_address + sizeof(uint32_t),
                     completion->committed_attributes[1]);
  }
  if (candidate) {
    if (!layout.candidate_hit_attribute_base_address ||
        layout.candidate_hit_attribute_stride_bytes <
            kHitAttributeWordCount * sizeof(uint32_t))
      return false;
    candidate_hit_attribute_address =
        layout.candidate_hit_attribute_base_address +
        (uint64_t)result.ray_ref * layout.candidate_hit_attribute_stride_bytes;
    memory.store_u32(candidate_hit_attribute_address,
                     completion->candidate_attributes[0]);
    memory.store_u32(candidate_hit_attribute_address + sizeof(uint32_t),
                     completion->candidate_attributes[1]);
  }

  const auto store = [&](CompletionField field, uint32_t value) {
    memory.store_u32(completion_field_address(layout, field, result.ray_ref),
                     value);
  };
  const CompletionMetadata &metadata = completion->metadata;
  /* READY is cleared before mutating a reused record and published last. */
  store(CompletionField::Ready, 0);
  store(CompletionField::Status, result.target == TraversalDispatchTarget::Miss
                                      ? kCompletionStatusMiss
                                      : result.target == TraversalDispatchTarget::ClosestHit
                                            ? kCompletionStatusHit
                                            : result.traversal_status);
  for (uint32_t word = 0; word < kHitRecordWordCount; ++word) {
    store(static_cast<CompletionField>(
              static_cast<uint32_t>(CompletionField::CommittedHitBase) + word),
          completion->committed_hit[word]);
  }
  store(CompletionField::HitAttributeAddrLo, uint32_t(hit_attribute_address));
  store(CompletionField::HitAttributeAddrHi,
        uint32_t(hit_attribute_address >> 32));
  store(CompletionField::CandidateKind,
        candidate ? result.traversal_status : 0);
  for (uint32_t word = 0; word < kHitRecordWordCount; ++word) {
    store(static_cast<CompletionField>(
              static_cast<uint32_t>(CompletionField::CandidateHitBase) + word),
          completion->candidate_hit[word]);
  }
  store(CompletionField::CandidateHitAttributeAddrLo,
        uint32_t(candidate_hit_attribute_address));
  store(CompletionField::CandidateHitAttributeAddrHi,
        uint32_t(candidate_hit_attribute_address >> 32));
  store(CompletionField::CandidateControlBase,
        candidate ? ventus_rt::callback_accept : ventus_rt::callback_pending);
  store(CompletionField::PayloadAddrLo, uint32_t(metadata.payload_address));
  store(CompletionField::PayloadAddrHi,
        uint32_t(metadata.payload_address >> 32));
  store(CompletionField::CpsFrame, metadata.cps_frame);
  store(CompletionField::ContinuationId, metadata.return_continuation_id);
  store(CompletionField::LaunchIdX, metadata.launch_id_x);
  store(CompletionField::LaunchIdY, metadata.launch_id_y);
  store(CompletionField::LaunchIdZ, metadata.launch_id_z);
  store(CompletionField::CallbackGroup, metadata.callback_group);
  store(CompletionField::Ready, 1);
  return true;
}

template <typename Memory>
static inline bool
make_resume_dispatch_request(const TraversalDispatchResult &result,
                             const CompletionArena<Memory> &completion_arena,
                             ResumeDispatchRequest *request)
{
  if (!request || (result.target != TraversalDispatchTarget::Miss &&
                   result.target != TraversalDispatchTarget::ClosestHit &&
                   result.target != TraversalDispatchTarget::AnyHitCandidate &&
                   result.target != TraversalDispatchTarget::IntersectionCandidate))
    return false;
  const CompletionRecord *completion = completion_arena.find(result.ray_ref);
  if (!completion)
    return false;
  const CompletionMetadata &metadata = completion->metadata;
  *request = {
      .ray_ref = result.ray_ref,
      .completed_stage = result.target,
      .callback_group = metadata.callback_group,
      .payload_address = metadata.payload_address,
      .cps_frame = metadata.cps_frame,
      .cps_stack_size = metadata.cps_stack_size,
      .continuation_id = metadata.return_continuation_id,
      .launch_id_x = metadata.launch_id_x,
      .launch_id_y = metadata.launch_id_y,
      .launch_id_z = metadata.launch_id_z,
  };
  return true;
}

/*
 * A host/bridge calls consume_producer_phase() only after the raygen producer
 * has halted all waves in its level.  Queue sealing is intentionally here,
 * not in vt.rt.enqueue: a doorbell admits sparse records but cannot know that
 * other producer waves have finished.  The caller supplies the executable
 * stage dispatch and the continuation_id -> resume-entry mapping.
 */
template <typename Memory>
class GlobalWavefrontConsumer {
public:
  explicit GlobalWavefrontConsumer(Memory &memory) : memory_(memory) {}

  std::vector<TraversalDispatchResult>
  consume_producer_phase(uint64_t queue_base, uint32_t max_batch_rays)
  {
    std::vector<TraversalDispatchResult> results;
    GlobalLevelQueue<Memory> queue(
        memory_, queue_base, typename GlobalLevelQueue<Memory>::OpenExisting{});
    if (queue.capacity() == 0 || !queue.seal_producer_phase())
      return results;

    const uint32_t batch_size = max_batch_rays ? max_batch_rays : queue.capacity();
    while (true) {
      std::vector<TraversalDispatchResult> batch =
          dispatch_global_traversal_batch(queue, batch_size, completion_);
      if (batch.empty())
        break;
      results.insert(results.end(), batch.begin(), batch.end());
    }
    return results;
  }

  std::vector<TraversalDispatchResult>
  resume_candidate_subset(const std::vector<uint32_t> &active_ray_refs)
  {
    return resume_global_traversal_subset(memory_, active_ray_refs, completion_);
  }

  std::vector<ResumeDispatchRequest>
  resume_requests(const std::vector<TraversalDispatchResult> &results) const
  {
    std::vector<ResumeDispatchRequest> requests;
    requests.reserve(results.size());
    for (const TraversalDispatchResult &result : results) {
      ResumeDispatchRequest request;
      if (make_resume_dispatch_request(result, completion_, &request))
        requests.push_back(request);
    }
    return requests;
  }

  CompletionArena<Memory> &completion_arena() { return completion_; }
  const CompletionArena<Memory> &completion_arena() const { return completion_; }

private:
  Memory &memory_;
  CompletionArena<Memory> completion_;
};

} // namespace ventus_rt_wavefront

#endif
