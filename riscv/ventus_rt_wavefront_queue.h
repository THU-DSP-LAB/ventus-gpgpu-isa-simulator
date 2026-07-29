/*
 * Copyright 2026
 * SPDX-License-Identifier: MIT
 *
 * Functional model for the wavefront queue boundary.  This is deliberately
 * separate from the legacy PDS traversal adapter: shaders submit public
 * mailboxes, while RTcore alone owns the private outqueue and global queue
 * pointers.
 */

#ifndef RISCV_VENTUS_RT_WAVEFRONT_QUEUE_H
#define RISCV_VENTUS_RT_WAVEFRONT_QUEUE_H

#include <algorithm>
#include <array>
#include <cstdint>
#include <deque>
#include <memory>
#include <unordered_map>
#include <vector>

namespace ventus_rt_wavefront {

enum class SubmitResult {
  Accepted,
  Malformed,
  Sealed,
  DispatchFault,
};

/*
 * This is the C++ mirror of vt_rt_global_abi.h.  The two trees cannot include
 * each other, so keep the wire values here and cover them with the mailbox
 * parser test below.  A mailbox is one AoS record written by one shader lane;
 * the global queue remains field-major and is owned exclusively by RTcore.
 */
constexpr uint32_t kMailboxMagic = 0x56545251u; /* "VTRQ" */
constexpr uint32_t kMailboxAbiVersion = 9;
constexpr uint32_t kMailboxHeaderBytes = 32;
constexpr uint32_t kMailboxMagicWord = 0;
constexpr uint32_t kMailboxVersionWord = 1;
constexpr uint32_t kMailboxGenerationWord = 2;
constexpr uint32_t kMailboxValidWord = 3;
constexpr uint32_t kMailboxQueueBaseLoWord = 4;
constexpr uint32_t kMailboxQueueBaseHiWord = 5;
constexpr uint32_t kMailboxReservedWord6 = 6;
constexpr uint32_t kMailboxReservedWord7 = 7;

enum class TraceField : uint32_t {
  TlasAddrLo = 0,
  TlasAddrHi,
  Flags,
  CullMask,
  SbtOffset,
  SbtStride,
  MissIndex,
  OriginX,
  OriginY,
  OriginZ,
  Tmin,
  DirectionX,
  DirectionY,
  DirectionZ,
  Tmax,
  PayloadLo,
  PayloadHi,
  CpsFrame,
  ParentFrame,
  ContinuationId,
  CpsStackSize,
  LaunchIdX,
  LaunchIdY,
  LaunchIdZ,
  PhaseGeneration,
  Depth,
  Ready,
  Count,
};

constexpr uint32_t kTraceFieldCount =
    static_cast<uint32_t>(TraceField::Count);
constexpr uint32_t kQueueHeaderBytes = 128;
constexpr uint32_t kQueueAlignmentBytes = 128;
constexpr uint32_t kQueueCapacityWord = 0;
constexpr uint32_t kQueueReserveTailWord = 1;
constexpr uint32_t kQueueConsumeHeadWord = 2;
constexpr uint32_t kQueueOverflowWord = 3;
constexpr uint32_t kQueueGenerationWord = 4;

enum class MailboxResult {
  Accepted,
  Invalid,
  AbiMismatch,
  GenerationMismatch,
  AddressNotRepresentable,
};

struct Submission {
  uint32_t ray_tag = 0;
  uint32_t parent_frame = 0;
  uint32_t phase_generation = 0;
  uint32_t depth = 0;
  uint64_t queue_base = 0;
  bool valid = false;
};

/*
 * Memory must provide load_u32(uint64_t).  Parsing is intentionally read-only:
 * the caller clears VALID only after QueueModel::submit accepts the complete
 * active-mask transaction.  Thus a malformed mailbox cannot consume a queue
 * slot, and a retry cannot lose a valid ray.
 */
template <typename Memory>
MailboxResult read_submission_mailbox(Memory &memory, uint64_t address,
                                     uint32_t expected_generation,
                                     Submission *submission)
{
  if (memory.load_u32(address + 4u * kMailboxValidWord) == 0)
    return MailboxResult::Invalid;
  if (memory.load_u32(address + 4u * kMailboxMagicWord) != kMailboxMagic ||
      memory.load_u32(address + 4u * kMailboxVersionWord) !=
          kMailboxAbiVersion)
    return MailboxResult::AbiMismatch;
  if (memory.load_u32(address + 4u * kMailboxGenerationWord) !=
      expected_generation)
    return MailboxResult::GenerationMismatch;
  const uint32_t queue_base_lo =
      memory.load_u32(address + 4u * kMailboxQueueBaseLoWord);
  const uint32_t queue_base_hi =
      memory.load_u32(address + 4u * kMailboxQueueBaseHiWord);
  /* Current handler executes RV32 addresses; never silently truncate ABI u64. */
  if (queue_base_hi != 0)
    return MailboxResult::AddressNotRepresentable;

  submission->ray_tag =
      memory.load_u32(address + kMailboxHeaderBytes +
                      4u * static_cast<uint32_t>(TraceField::TlasAddrLo));
  submission->parent_frame =
      memory.load_u32(address + kMailboxHeaderBytes +
                      4u * static_cast<uint32_t>(TraceField::ParentFrame));
  submission->phase_generation = memory.load_u32(
      address + kMailboxHeaderBytes +
      4u * static_cast<uint32_t>(TraceField::PhaseGeneration));
  submission->depth =
      memory.load_u32(address + kMailboxHeaderBytes +
                      4u * static_cast<uint32_t>(TraceField::Depth));
  /* Header and record must describe the same phase; do not admit torn data. */
  if (submission->phase_generation != expected_generation)
    return MailboxResult::GenerationMismatch;
  submission->queue_base = queue_base_lo;
  submission->valid = true;
  return MailboxResult::Accepted;
}

struct GlobalRecord {
  Submission submission;
  bool ready = false;
};

/*
 * This is the wire-format adapter for the global queue defined by
 * vt_rt_global_abi.h.  Memory is device-global memory; no PDS/private-memory
 * address is accepted by this path.  It intentionally supports one producer
 * phase followed by one consumer phase: there is no concurrent dequeue and no
 * ring-buffer reclamation in this model.
 *
 * Required Memory operations are load_u32(), store_u32(), and
 * atomic_fetch_add_u32().  The latter is the RTcore-owned global tail atomic;
 * shader lanes never allocate queue entries directly.
 */
struct FieldMajorRecord {
  std::array<uint32_t, kTraceFieldCount> fields{};
};

/*
 * A queue ordinal remains visible while a record is assigned to a worker.
 * This is deliberately distinct from a physical lane: a later scheduler may
 * bind this ray_ref to any lane without changing the global record identity.
 */
struct IndexedFieldMajorRecord {
  uint32_t ray_ref = 0;
  FieldMajorRecord record;
};

enum class LevelQueueResult {
  Accepted,
  Malformed,
  Sealed,
  DispatchFault,
};

static inline uint64_t
align_queue_bytes(uint64_t value)
{
  return (value + kQueueAlignmentBytes - 1u) &
         ~(uint64_t)(kQueueAlignmentBytes - 1u);
}

static inline uint64_t
queue_field_stride_bytes(uint32_t capacity)
{
  return align_queue_bytes((uint64_t)capacity * sizeof(uint32_t));
}

static inline uint64_t
queue_field_address(uint64_t queue_base, uint64_t field_stride,
                    TraceField field, uint32_t ray_ref)
{
  return queue_base + kQueueHeaderBytes +
         (uint64_t)static_cast<uint32_t>(field) * field_stride +
         (uint64_t)ray_ref * sizeof(uint32_t);
}

static inline uint64_t
mailbox_field_address(uint64_t mailbox_base, TraceField field)
{
  return mailbox_base + kMailboxHeaderBytes +
         (uint64_t)static_cast<uint32_t>(field) * sizeof(uint32_t);
}

template <typename Memory>
class GlobalLevelQueue {
public:
  struct OpenExisting {};

  GlobalLevelQueue(Memory &memory, uint64_t queue_base, uint32_t capacity,
                   uint32_t generation)
      : memory_(memory), queue_base_(queue_base), capacity_(capacity),
        field_stride_(queue_field_stride_bytes(capacity)), generation_(generation)
  {
    memory_.store_u32(header_address(kQueueCapacityWord), capacity_);
    memory_.store_u32(header_address(kQueueReserveTailWord), 0);
    memory_.store_u32(header_address(kQueueConsumeHeadWord), 0);
    memory_.store_u32(header_address(kQueueOverflowWord), 0);
    memory_.store_u32(header_address(kQueueGenerationWord), generation_);
  }

  /* Open queue storage initialized by the driver without resetting its tail. */
  GlobalLevelQueue(Memory &memory, uint64_t queue_base, OpenExisting)
      : memory_(memory), queue_base_(queue_base),
        capacity_(memory.load_u32(queue_base +
                                  kQueueCapacityWord * sizeof(uint32_t))),
        field_stride_(queue_field_stride_bytes(capacity_)),
        generation_(memory.load_u32(queue_base +
                                    kQueueGenerationWord * sizeof(uint32_t)))
  {}

  uint64_t queue_base() const { return queue_base_; }
  Memory &memory_for_worker() { return memory_; }
  uint32_t capacity() const { return capacity_; }
  uint32_t generation() const { return generation_; }
  uint64_t field_stride_bytes() const { return field_stride_; }
  uint32_t reserve_tail() const
  {
    return memory_.load_u32(header_address(kQueueReserveTailWord));
  }
  uint32_t consume_head() const
  {
    return memory_.load_u32(header_address(kQueueConsumeHeadWord));
  }
  bool sealed() const { return sealed_; }
  bool faulted() const
  {
    return memory_.load_u32(header_address(kQueueOverflowWord)) != 0;
  }

  /*
   * RTcore scans only currently active lanes.  An inactive or invalid mailbox
   * contributes no child; a valid malformed mailbox faults before tail
   * allocation, so a bad warp cannot leave a partially committed range.
   */
  LevelQueueResult submit_sparse_mailboxes(
      const std::vector<uint64_t> &mailbox_addresses, uint32_t active_mask,
      uint32_t expected_generation, uint32_t *submitted_count = nullptr)
  {
    if (submitted_count)
      *submitted_count = 0;
    if (sealed_)
      return LevelQueueResult::Sealed;
    if (faulted())
      return LevelQueueResult::DispatchFault;
    if (expected_generation != generation_)
      return LevelQueueResult::Malformed;
    if (mailbox_addresses.size() > 32)
      return fail_dispatch();

    std::vector<uint64_t> accepted_mailboxes;
    accepted_mailboxes.reserve(mailbox_addresses.size());
    for (uint32_t lane = 0; lane < mailbox_addresses.size(); ++lane) {
      if ((active_mask & (1u << lane)) == 0)
        continue;

      Submission submission;
      const MailboxResult parsed = read_submission_mailbox(
          memory_, mailbox_addresses[lane], expected_generation, &submission);
      if (parsed == MailboxResult::Invalid)
        continue;
      if (parsed != MailboxResult::Accepted)
        return LevelQueueResult::Malformed;
      if (submission.queue_base != queue_base_)
        return LevelQueueResult::Malformed;
      accepted_mailboxes.push_back(mailbox_addresses[lane]);
    }

    const uint32_t count = accepted_mailboxes.size();
    if (count == 0)
      return LevelQueueResult::Accepted;

    /* Atomic ticket allocation is the only cross-SM synchronization here. */
    const uint32_t base = memory_.atomic_fetch_add_u32(
        header_address(kQueueReserveTailWord), count);
    if (base > capacity_ || count > capacity_ - base)
      return fail_dispatch();

    for (uint32_t i = 0; i < count; ++i) {
      copy_mailbox_to_record(accepted_mailboxes[i], base + i);
      /* Consume only after the field-major record was fully published. */
      memory_.store_u32(accepted_mailboxes[i] +
                            4u * kMailboxValidWord,
                        0);
    }
    if (submitted_count)
      *submitted_count = count;
    return LevelQueueResult::Accepted;
  }

  /* The scheduler calls this only after all SM RTcores have produced. */
  bool seal_producer_phase()
  {
    if (faulted() || sealed_)
      return false;
    sealed_ = true;
    return true;
  }

  std::vector<FieldMajorRecord> dequeue_batch(uint32_t max_rays)
  {
    std::vector<FieldMajorRecord> records;
    const std::vector<IndexedFieldMajorRecord> indexed =
        dequeue_indexed_batch(max_rays);
    records.reserve(indexed.size());
    for (const IndexedFieldMajorRecord &entry : indexed)
      records.push_back(entry.record);
    return records;
  }

  /*
   * Consumer-side worker interface.  READY is checked before advancing the
   * global head; callers therefore cannot observe or skip a reserved hole.
   */
  std::vector<IndexedFieldMajorRecord> dequeue_indexed_batch(uint32_t max_rays)
  {
    std::vector<IndexedFieldMajorRecord> batch;
    if (!sealed_ || faulted() || max_rays == 0)
      return batch;

    const uint32_t head = consume_head();
    const uint32_t tail = reserve_tail();
    if (head >= tail)
      return batch;

    const uint32_t count = std::min(max_rays, tail - head);
    for (uint32_t i = 0; i < count; ++i) {
      const uint32_t ray_ref = head + i;
      if (memory_.load_u32(queue_field_address(queue_base_, field_stride_,
                                                TraceField::Ready, ray_ref)) == 0)
        return {};
      batch.push_back({ray_ref, read_record(ray_ref)});
    }
    memory_.store_u32(header_address(kQueueConsumeHeadWord), head + count);
    return batch;
  }

private:
  uint64_t header_address(uint32_t word) const
  {
    return queue_base_ + (uint64_t)word * sizeof(uint32_t);
  }

  LevelQueueResult fail_dispatch()
  {
    memory_.store_u32(header_address(kQueueOverflowWord), 1);
    return LevelQueueResult::DispatchFault;
  }

  void copy_mailbox_to_record(uint64_t mailbox_base, uint32_t ray_ref)
  {
    /* READY is RTcore-owned and must be the final destination store. */
    for (uint32_t field = 0;
         field < static_cast<uint32_t>(TraceField::Ready); ++field) {
      const TraceField trace_field = static_cast<TraceField>(field);
      memory_.store_u32(queue_field_address(queue_base_, field_stride_,
                                             trace_field, ray_ref),
                        memory_.load_u32(mailbox_field_address(mailbox_base,
                                                               trace_field)));
    }
    memory_.store_u32(queue_field_address(queue_base_, field_stride_,
                                           TraceField::Ready, ray_ref),
                      1);
  }

  FieldMajorRecord read_record(uint32_t ray_ref) const
  {
    FieldMajorRecord record;
    for (uint32_t field = 0; field < kTraceFieldCount; ++field)
      record.fields[field] = memory_.load_u32(queue_field_address(
          queue_base_, field_stride_, static_cast<TraceField>(field), ray_ref));
    return record;
  }

  Memory &memory_;
  uint64_t queue_base_;
  uint32_t capacity_;
  uint64_t field_stride_;
  uint32_t generation_;
  bool sealed_ = false;
};

/*
 * Handler-facing RT_ENQUEUE adapter.  lane_mailbox_addresses is the i32 VGPR
 * source widened to host addresses; the caller supplies only active lanes in
 * active_mask.  It has no PDS/current-slot operand.
 */
template <typename Memory>
LevelQueueResult submit_rt_enqueue(
    GlobalLevelQueue<Memory> &queue,
    const std::vector<uint64_t> &lane_mailbox_addresses,
    uint32_t active_mask, uint32_t generation,
    uint32_t *submitted_count = nullptr)
{
  return queue.submit_sparse_mailboxes(lane_mailbox_addresses, active_mask,
                                       generation, submitted_count);
}

/*
 * Queue identity is shader-provided in the ABI mailbox, not inferred from a
 * PDS slot or a CSR.  The registry keeps a queue object per global base so a
 * later enqueue observes the same producer-phase state and tail.
 */
template <typename Memory>
class GlobalLevelQueueRegistry {
public:
  explicit GlobalLevelQueueRegistry(Memory &memory) : memory_(memory) {}

  LevelQueueResult submit_rt_enqueue_batch(
      const std::vector<uint64_t> &lane_mailbox_addresses,
      uint32_t active_mask, uint32_t generation,
      uint32_t *submitted_count = nullptr)
  {
    if (submitted_count)
      *submitted_count = 0;
    if (lane_mailbox_addresses.size() > 32)
      return LevelQueueResult::DispatchFault;

    bool have_queue = false;
    uint64_t queue_base = 0;
    for (uint32_t lane = 0; lane < lane_mailbox_addresses.size(); ++lane) {
      if ((active_mask & (1u << lane)) == 0)
        continue;
      Submission submission;
      const MailboxResult parsed = read_submission_mailbox(
          memory_, lane_mailbox_addresses[lane], generation, &submission);
      if (parsed == MailboxResult::Invalid)
        continue;
      if (parsed != MailboxResult::Accepted)
        return LevelQueueResult::Malformed;
      if (!have_queue) {
        queue_base = submission.queue_base;
        have_queue = true;
      } else if (queue_base != submission.queue_base) {
        return LevelQueueResult::Malformed;
      }
    }

    /* A sparse warp with no child has no queue identity and no side effect. */
    if (!have_queue)
      return LevelQueueResult::Accepted;

    GlobalLevelQueue<Memory> *queue = find_or_open(queue_base);
    if (!queue)
      return LevelQueueResult::DispatchFault;
    return submit_rt_enqueue(*queue, lane_mailbox_addresses, active_mask,
                             generation, submitted_count);
  }

  GlobalLevelQueue<Memory> *find(uint64_t queue_base)
  {
    const auto it = queues_.find(queue_base);
    return it == queues_.end() ? nullptr : it->second.get();
  }

private:
  GlobalLevelQueue<Memory> *find_or_open(uint64_t queue_base)
  {
    if (GlobalLevelQueue<Memory> *queue = find(queue_base))
      return queue;
    auto queue = std::make_unique<GlobalLevelQueue<Memory>>(
        memory_, queue_base, typename GlobalLevelQueue<Memory>::OpenExisting{});
    if (queue->capacity() == 0)
      return nullptr;
    GlobalLevelQueue<Memory> *result = queue.get();
    queues_.emplace(queue_base, std::move(queue));
    return result;
  }

  Memory &memory_;
  std::unordered_map<uint64_t, std::unique_ptr<GlobalLevelQueue<Memory>>> queues_;
};

struct ParentState {
  uint32_t generation = 0;
  uint32_t pending_children = 0;
  bool sealed = false;
  bool resume_enqueued = false;
};

/*
 * Device-global queue state.  One instance is shared by every SM-local
 * QueueModel.  Spike invokes it synchronously, so these operations model the
 * serialization points that RTL will implement with atomics.
 */
class GlobalQueueStorage {
public:
  explicit GlobalQueueStorage(uint32_t capacity) : capacity_(capacity) {}

  bool faulted() const { return dispatch_fault_; }
  void fail_dispatch() { dispatch_fault_ = true; }
  uint32_t reserve_tail() const { return reserve_tail_; }
  uint32_t consume_head() const { return consume_head_; }
  const std::vector<GlobalRecord> &global_records() const { return global_; }

  bool reserve(uint32_t count, uint32_t *base)
  {
    if (dispatch_fault_ || count > capacity_ - reserve_tail_) {
      dispatch_fault_ = true;
      return false;
    }
    *base = reserve_tail_;
    reserve_tail_ += count;
    if (global_.size() < reserve_tail_)
      global_.resize(reserve_tail_);
    return true;
  }

  void write_record(uint32_t index, const Submission &submission)
  {
    global_[index].submission = submission;
  }

  /* Release-style publication: record data must have been written first. */
  void publish(uint32_t base, uint32_t count)
  {
    for (uint32_t i = 0; i < count; ++i)
      global_[base + i].ready = true;
  }

  std::vector<GlobalRecord> dequeue_batch(uint32_t max_lanes)
  {
    std::vector<GlobalRecord> batch;
    if (dispatch_fault_ || consume_head_ == reserve_tail_)
      return batch;

    const uint32_t count = std::min(max_lanes, reserve_tail_ - consume_head_);
    /*
     * Strict FIFO: do not claim a prefix until the entire requested worker
     * batch is published.  In particular never skip an earlier unready hole.
     */
    for (uint32_t i = 0; i < count; ++i) {
      if (!global_[consume_head_ + i].ready)
        return {};
    }
    batch.insert(batch.end(), global_.begin() + consume_head_,
                 global_.begin() + consume_head_ + count);
    consume_head_ += count;
    return batch;
  }

private:
  uint32_t capacity_;
  uint32_t reserve_tail_ = 0;
  uint32_t consume_head_ = 0;
  bool dispatch_fault_ = false;
  std::vector<GlobalRecord> global_;
};

class QueueModel {
public:
  QueueModel(uint32_t global_capacity, uint32_t outqueue_capacity,
             uint32_t flush_quantum)
      : owned_global_(global_capacity), global_(&owned_global_),
        outqueue_capacity_(outqueue_capacity), flush_quantum_(flush_quantum) {}

  QueueModel(GlobalQueueStorage &global, uint32_t outqueue_capacity,
             uint32_t flush_quantum)
      : global_(&global), outqueue_capacity_(outqueue_capacity),
        flush_quantum_(flush_quantum) {}

  bool faulted() const { return global_->faulted(); }
  uint32_t reserve_tail() const { return global_->reserve_tail(); }
  uint32_t consume_head() const { return global_->consume_head(); }
  uint32_t outqueue_count() const { return outqueue_.size(); }
  const std::vector<GlobalRecord> &global_records() const
  {
    return global_->global_records();
  }

  SubmitResult submit(const std::vector<Submission> &submissions)
  {
    if (global_->faulted())
      return SubmitResult::DispatchFault;

    /* Admission is warp-transactional: reject before changing any state. */
    for (const Submission &submission : submissions) {
      if (!submission.valid)
        return SubmitResult::Malformed;
      const auto parent = parents_.find(submission.parent_frame);
      if (parent != parents_.end() &&
          (parent->second.sealed ||
           parent->second.generation != submission.phase_generation))
        return SubmitResult::Sealed;
    }

    if (submissions.size() > outqueue_capacity_) {
      /* A single oversized SM admission is a dispatch-wide fault. */
      global_->fail_dispatch();
      return SubmitResult::DispatchFault;
    }

    while (outqueue_.size() + submissions.size() > outqueue_capacity_) {
      if (!flush(true))
        return SubmitResult::DispatchFault;
    }

    for (const Submission &submission : submissions) {
      ParentState &parent = parents_[submission.parent_frame];
      parent.generation = submission.phase_generation;
      parent.pending_children++;
      outqueue_.push_back(submission);
    }

    if (outqueue_.size() >= flush_quantum_)
      flush(false);
    return global_->faulted() ? SubmitResult::DispatchFault : SubmitResult::Accepted;
  }

  bool seal(uint32_t parent_frame, uint32_t phase_generation)
  {
    if (global_->faulted())
      return false;
    ParentState &parent = parents_[parent_frame];
    if (parent.generation != phase_generation && parent.pending_children != 0)
      return false;
    parent.generation = phase_generation;
    parent.sealed = true;
    enqueue_resume_if_complete(parent_frame, parent);
    return true;
  }

  bool flush(bool force)
  {
    if (global_->faulted())
      return false;
    if (outqueue_.empty() || (!force && outqueue_.size() < flush_quantum_))
      return true;

    const uint32_t count = force ? outqueue_.size() : flush_quantum_;
    uint32_t base;
    if (!global_->reserve(count, &base))
      return false;

    /* Copy every field before the release-style READY publication. */
    for (uint32_t i = 0; i < count; ++i) {
      global_->write_record(base + i, outqueue_.front());
      outqueue_.pop_front();
    }
    global_->publish(base, count);
    return true;
  }

  std::vector<GlobalRecord> dequeue_batch(uint32_t max_lanes)
  {
    return global_->dequeue_batch(max_lanes);
  }

  bool complete_child(uint32_t parent_frame, uint32_t phase_generation)
  {
    const auto it = parents_.find(parent_frame);
    if (it == parents_.end() || it->second.generation != phase_generation ||
        it->second.pending_children == 0)
      return false;
    it->second.pending_children--;
    enqueue_resume_if_complete(parent_frame, it->second);
    return true;
  }

  std::vector<uint32_t> take_resumes()
  {
    std::vector<uint32_t> resumes;
    resumes.swap(resumes_);
    return resumes;
  }

private:
  void enqueue_resume_if_complete(uint32_t parent_frame, ParentState &parent)
  {
    if (parent.sealed && parent.pending_children == 0 && !parent.resume_enqueued) {
      parent.resume_enqueued = true;
      resumes_.push_back(parent_frame);
    }
  }

  GlobalQueueStorage owned_global_{0};
  GlobalQueueStorage *global_;
  uint32_t outqueue_capacity_;
  uint32_t flush_quantum_;
  std::deque<Submission> outqueue_;
  std::unordered_map<uint32_t, ParentState> parents_;
  std::vector<uint32_t> resumes_;
};

/*
 * This is the RT_TRAVERSE wavefront adapter contract.  Memory additionally
 * provides store_u32(uint64_t, uint32_t).  It deliberately parses every
 * active lane before calling QueueModel::submit, then consumes VALID only on
 * successful warp-transactional admission.
 */
template <typename Memory>
SubmitResult submit_mailboxes(QueueModel &queue, Memory &memory,
                              const std::vector<uint64_t> &mailbox_addresses,
                              uint32_t expected_generation)
{
  std::vector<Submission> submissions;
  submissions.reserve(mailbox_addresses.size());
  for (uint64_t address : mailbox_addresses) {
    Submission submission;
    if (read_submission_mailbox(memory, address, expected_generation,
                                &submission) != MailboxResult::Accepted)
      return SubmitResult::Malformed;
    submissions.push_back(submission);
  }

  const SubmitResult result = queue.submit(submissions);
  if (result != SubmitResult::Accepted)
    return result;

  for (uint64_t address : mailbox_addresses)
    memory.store_u32(address + 4u * kMailboxValidWord, 0);
  return SubmitResult::Accepted;
}

} // namespace ventus_rt_wavefront

#endif
