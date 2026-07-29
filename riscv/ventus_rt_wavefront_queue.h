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
#include <cstdint>
#include <deque>
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
constexpr uint32_t kMailboxAbiVersion = 3;
constexpr uint32_t kMailboxHeaderBytes = 32;
constexpr uint32_t kMailboxMagicWord = 0;
constexpr uint32_t kMailboxVersionWord = 1;
constexpr uint32_t kMailboxGenerationWord = 2;
constexpr uint32_t kMailboxValidWord = 3;

enum class TraceField : uint32_t {
  TlasAddr = 0,
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
  ChildCpsFrame,
  ParentFrame,
  PhaseGeneration,
  Depth,
  Ready,
};

enum class MailboxResult {
  Accepted,
  Invalid,
  AbiMismatch,
  GenerationMismatch,
};

struct Submission {
  uint32_t ray_tag = 0;
  uint32_t parent_frame = 0;
  uint32_t phase_generation = 0;
  uint32_t depth = 0;
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

  submission->ray_tag =
      memory.load_u32(address + kMailboxHeaderBytes +
                      4u * static_cast<uint32_t>(TraceField::TlasAddr));
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
  submission->valid = true;
  return MailboxResult::Accepted;
}

struct GlobalRecord {
  Submission submission;
  bool ready = false;
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
