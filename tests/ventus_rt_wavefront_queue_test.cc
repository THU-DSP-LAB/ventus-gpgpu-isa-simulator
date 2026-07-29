#include "ventus_rt_wavefront_queue.h"

#include <cassert>
#include <cstdint>
#include <unordered_map>
#include <vector>

using namespace ventus_rt_wavefront;

static Submission submit(uint32_t tag, uint32_t parent, uint32_t phase)
{
  return {.ray_tag = tag, .parent_frame = parent,
          .phase_generation = phase, .depth = 1, .valid = true};
}

struct TestMemory {
  uint32_t load_u32(uint64_t address) { return words[address]; }
  void store_u32(uint64_t address, uint32_t value) { words[address] = value; }
  std::unordered_map<uint64_t, uint32_t> words;
};

static void write_mailbox(TestMemory &memory, uint64_t base, uint32_t tag,
                          uint32_t parent, uint32_t phase, uint32_t depth,
                          uint32_t valid)
{
  memory.store_u32(base + 4u * kMailboxMagicWord, kMailboxMagic);
  memory.store_u32(base + 4u * kMailboxVersionWord, kMailboxAbiVersion);
  memory.store_u32(base + 4u * kMailboxGenerationWord, phase);
  memory.store_u32(base + kMailboxHeaderBytes +
                       4u * static_cast<uint32_t>(TraceField::TlasAddr),
                   tag);
  memory.store_u32(base + kMailboxHeaderBytes +
                       4u * static_cast<uint32_t>(TraceField::ParentFrame),
                   parent);
  memory.store_u32(base + kMailboxHeaderBytes +
                       4u * static_cast<uint32_t>(TraceField::PhaseGeneration),
                   phase);
  memory.store_u32(base + kMailboxHeaderBytes +
                       4u * static_cast<uint32_t>(TraceField::Depth),
                   depth);
  /* Mimic the shader's release-style final publication. */
  memory.store_u32(base + 4u * kMailboxValidWord, valid);
}

int main()
{
  TestMemory memory;
  constexpr uint64_t mailbox = 0x1000;
  write_mailbox(memory, mailbox, 99, 5, 7, 2, 1);
  Submission parsed;
  assert(read_submission_mailbox(memory, mailbox, 7, &parsed) ==
         MailboxResult::Accepted);
  assert(parsed.valid && parsed.ray_tag == 99 && parsed.parent_frame == 5 &&
         parsed.phase_generation == 7 && parsed.depth == 2);
  assert(read_submission_mailbox(memory, mailbox, 8, &parsed) ==
         MailboxResult::GenerationMismatch);
  memory.store_u32(mailbox + kMailboxHeaderBytes +
                       4u * static_cast<uint32_t>(TraceField::PhaseGeneration),
                   8);
  assert(read_submission_mailbox(memory, mailbox, 7, &parsed) ==
         MailboxResult::GenerationMismatch);
  memory.store_u32(mailbox + kMailboxHeaderBytes +
                       4u * static_cast<uint32_t>(TraceField::PhaseGeneration),
                   7);
  memory.store_u32(mailbox + 4u * kMailboxVersionWord, 99);
  assert(read_submission_mailbox(memory, mailbox, 7, &parsed) ==
         MailboxResult::AbiMismatch);
  memory.store_u32(mailbox + 4u * kMailboxVersionWord, kMailboxAbiVersion);
  memory.store_u32(mailbox + 4u * kMailboxValidWord, 0);
  assert(read_submission_mailbox(memory, mailbox, 7, &parsed) ==
         MailboxResult::Invalid);

  QueueModel mailbox_queue(/*global_capacity=*/8, /*outqueue_capacity=*/8,
                           /*flush_quantum=*/2);
  constexpr uint64_t mailbox2 = 0x2000;
  write_mailbox(memory, mailbox, 100, 5, 7, 2, 1);
  write_mailbox(memory, mailbox2, 200, 5, 7, 2, 0);
  assert(submit_mailboxes(mailbox_queue, memory, {mailbox, mailbox2}, 7) ==
         SubmitResult::Malformed);
  assert(mailbox_queue.reserve_tail() == 0);
  assert(memory.load_u32(mailbox + 4u * kMailboxValidWord) == 1);
  write_mailbox(memory, mailbox2, 200, 5, 7, 2, 1);
  assert(submit_mailboxes(mailbox_queue, memory, {mailbox, mailbox2}, 7) ==
         SubmitResult::Accepted);
  assert(mailbox_queue.reserve_tail() == 2);
  assert(memory.load_u32(mailbox + 4u * kMailboxValidWord) == 0);
  assert(memory.load_u32(mailbox2 + 4u * kMailboxValidWord) == 0);

  /* Two SM-local outqueues reserve distinct contiguous global ranges. */
  GlobalQueueStorage shared_global(/*capacity=*/8);
  QueueModel sm0(shared_global, /*outqueue_capacity=*/4, /*flush_quantum=*/2);
  QueueModel sm1(shared_global, /*outqueue_capacity=*/4, /*flush_quantum=*/2);
  assert(sm0.submit({submit(1, 1, 1), submit(2, 1, 1)}) ==
         SubmitResult::Accepted);
  assert(sm1.submit({submit(3, 2, 1), submit(4, 2, 1)}) ==
         SubmitResult::Accepted);
  assert(shared_global.reserve_tail() == 4);
  assert(shared_global.global_records()[0].submission.ray_tag == 1);
  assert(shared_global.global_records()[3].submission.ray_tag == 4);

  /* A later SM must not bypass an earlier reserved-but-unpublished batch. */
  GlobalQueueStorage holes(/*capacity=*/8);
  uint32_t earlier_base;
  uint32_t later_base;
  assert(holes.reserve(2, &earlier_base) && earlier_base == 0);
  holes.write_record(earlier_base, submit(11, 3, 1));
  holes.write_record(earlier_base + 1, submit(12, 3, 1));
  assert(holes.reserve(2, &later_base) && later_base == 2);
  holes.write_record(later_base, submit(21, 4, 1));
  holes.write_record(later_base + 1, submit(22, 4, 1));
  holes.publish(later_base, 2);
  assert(holes.dequeue_batch(4).empty());
  assert(holes.consume_head() == 0);
  holes.publish(earlier_base, 2);
  std::vector<GlobalRecord> hole_batch = holes.dequeue_batch(4);
  assert(hole_batch.size() == 4);
  assert(hole_batch[0].submission.ray_tag == 11);
  assert(hole_batch[3].submission.ray_tag == 22);

  QueueModel queue(/*global_capacity=*/70, /*outqueue_capacity=*/4,
                   /*flush_quantum=*/2);

  assert(queue.submit({submit(10, 1, 7), submit(20, 1, 7)}) ==
         SubmitResult::Accepted);
  assert(queue.reserve_tail() == 2);
  assert(queue.global_records()[0].ready && queue.global_records()[1].ready);
  assert(queue.global_records()[0].submission.ray_tag == 10);
  assert(queue.global_records()[1].submission.ray_tag == 20);

  assert(queue.submit({submit(30, 1, 7), submit(40, 2, 3),
                       submit(50, 1, 7)}) == SubmitResult::Accepted);
  assert(queue.flush(true));
  assert(queue.reserve_tail() == 5);
  for (uint32_t i = 0; i < 5; ++i)
    assert(queue.global_records()[i].ready);

  std::vector<GlobalRecord> first = queue.dequeue_batch(4);
  std::vector<GlobalRecord> tail = queue.dequeue_batch(32);
  assert(first.size() == 4 && tail.size() == 1);
  assert(first[0].submission.ray_tag == 10);
  assert(tail[0].submission.ray_tag == 50);

  assert(queue.seal(1, 7));
  assert(queue.take_resumes().empty());
  assert(queue.complete_child(1, 7));
  assert(queue.complete_child(1, 7));
  assert(queue.complete_child(1, 7));
  assert(queue.complete_child(1, 7));
  std::vector<uint32_t> resumes = queue.take_resumes();
  assert(resumes.size() == 1 && resumes[0] == 1);
  assert(!queue.complete_child(1, 7));
  assert(queue.submit({submit(60, 1, 7)}) == SubmitResult::Sealed);

  assert(queue.submit({Submission{}}) == SubmitResult::Malformed);
  assert(queue.reserve_tail() == 5);

  QueueModel overflow(/*global_capacity=*/2, /*outqueue_capacity=*/4,
                      /*flush_quantum=*/2);
  assert(overflow.submit({submit(1, 9, 1), submit(2, 9, 1)}) ==
         SubmitResult::Accepted);
  assert(overflow.submit({submit(3, 9, 1), submit(4, 9, 1)}) ==
         SubmitResult::DispatchFault);
  assert(overflow.faulted());
  assert(overflow.reserve_tail() == 2);
  assert(overflow.dequeue_batch(32).empty());

  QueueModel batches(/*global_capacity=*/96, /*outqueue_capacity=*/96,
                     /*flush_quantum=*/32);
  std::vector<Submission> many;
  for (uint32_t i = 0; i < 70; ++i)
    many.push_back(submit(100 + i, 77, 1));
  assert(batches.submit(many) == SubmitResult::Accepted);
  assert(batches.flush(true));
  assert(batches.dequeue_batch(32).size() == 32);
  assert(batches.dequeue_batch(32).size() == 32);
  assert(batches.dequeue_batch(32).size() == 6);
  return 0;
}
