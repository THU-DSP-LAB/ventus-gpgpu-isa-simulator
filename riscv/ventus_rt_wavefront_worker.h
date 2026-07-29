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

/* Result routing boundary only.  Invoking shader code and CPS resume are
 * intentionally outside this worker: the current global ABI has no result
 * planes or CPS frame format for either operation. */
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
class WorkerLocalMemory {
public:
  WorkerLocalMemory(Memory &global_memory, uint64_t context_id)
      : global_memory_(global_memory), context_id_(context_id) {}

  uint64_t rt_context_key(reg_t slot) const
  {
    return (context_id_ << 32) ^ slot;
  }

  uint32_t load32(reg_t address)
  {
    if (address < ventus_rt::rt_total_size_bytes) {
      const auto it = local_words_.find(address);
      return it == local_words_.end() ? 0 : it->second;
    }
    return global_memory_.load_u32(address);
  }

  void store32(reg_t address, uint32_t value)
  {
    if (address < ventus_rt::rt_total_size_bytes) {
      local_words_[address] = value;
      return;
    }
    global_memory_.store_u32(address, value);
  }

private:
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
  store_word(memory, 0, cps_header_base, cps_frame_base,
             field(TraceField::ChildCpsFrame));
  store_word(memory, 0, cps_header_base, cps_active_level,
             field(TraceField::Depth));
}

template <typename Memory>
static inline TraversalDispatchResult
run_global_traversal_record(Memory &global_memory,
                            const IndexedFieldMajorRecord &entry,
                            uint64_t worker_context_id)
{
  using namespace ventus_rt;
  WorkerLocalMemory<Memory> local_memory(global_memory, worker_context_id);
  seed_worker_slot(local_memory, entry.record);

  const uint32_t status = traverse(local_memory, /* temporary slot */ 0);
  TraversalDispatchResult result{.ray_ref = entry.ray_ref,
                                 .traversal_status = status};
  switch (status) {
  case traversal_complete_miss:
    result.target = TraversalDispatchTarget::Miss;
    break;
  case traversal_complete_hit:
    result.target = TraversalDispatchTarget::ClosestHit;
    break;
  case traversal_candidate_non_opaque_triangle:
    result.target = TraversalDispatchTarget::AnyHitCandidate;
    break;
  case traversal_candidate_procedural_aabb:
    result.target = TraversalDispatchTarget::IntersectionCandidate;
    break;
  case traversal_terminated:
    result.target = TraversalDispatchTarget::Terminated;
    break;
  default:
    result.target = TraversalDispatchTarget::Fault;
    return result;
  }

  if (result.target == TraversalDispatchTarget::ClosestHit) {
    result.primitive_id = load_word(local_memory, 0, committed_hit_record_base,
                                    hit_record_primitive_id);
    result.instance_id = load_word(local_memory, 0, committed_hit_record_base,
                                   hit_record_instance_id);
    result.geometry_id = load_word(local_memory, 0, committed_hit_record_base,
                                   hit_record_geometry_id);
    result.sbt_index = load_word(local_memory, 0, committed_hit_record_base,
                                 hit_record_sbt_index);
    result.hit_t_bits = load_word(local_memory, 0, committed_hit_record_base,
                                  hit_record_hit_t);
  }
  return result;
}

/*
 * One sealed level becomes a worker batch.  This intentionally has no
 * callback invocation or CPS continuation: it makes the missing ABI work
 * explicit instead of preserving PDS behavior behind the new queue.
 */
template <typename Memory>
static inline std::vector<TraversalDispatchResult>
dispatch_global_traversal_batch(GlobalLevelQueue<Memory> &queue,
                                uint32_t max_rays)
{
  std::vector<TraversalDispatchResult> results;
  const std::vector<IndexedFieldMajorRecord> batch =
      queue.dequeue_indexed_batch(max_rays);
  results.reserve(batch.size());
  uint64_t context_id = 1;
  for (const IndexedFieldMajorRecord &entry : batch)
    results.push_back(run_global_traversal_record(queue.memory_for_worker(),
                                                   entry, context_id++));
  return results;
}

} // namespace ventus_rt_wavefront

#endif
