#ifndef RISCV_VENTUS_RT_REPLAY_CAPTURE_H
#define RISCV_VENTUS_RT_REPLAY_CAPTURE_H

#include "ventus_rt.h"

#include <array>
#include <cerrno>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>

namespace ventus_rt {

constexpr std::size_t rt_replay_capture_lanes = 32;

// Opt-in, workload-facing capture of the exact memory contract observed by
// RT_TRAVERSE_V5.  The compact text format is deliberately independent of
// Spike's instruction log so a cycle-accurate RTL test can replay one command
// without retaining a hundreds-of-megabytes whole-kernel trace.
//
//   VENTUS_RT_RTL_CAPTURE=<path>
//   VENTUS_RT_RTL_CAPTURE_LIMIT=<maximum traverse commands, optional>
//
// Records contain a sorted, deduplicated 128-byte command pre-image and final
// word writes.  Full lines match the current RTL memory packet width and avoid
// inventing zeroes for words that the functional model did not individually
// inspect:
//   T command hardware-warp issue-mask
//   L command lane canonical-pds-address private-state-address
//   B command 128-byte-block-address word31...word0
//   W command lane committed-record-word-address value
//   F command physical-address final-value
//   O command lane compact-result
//   E command read-count write-count initial-blocks final-writes
class RtReplayCapture {
public:
  static constexpr uint64_t inactive_command =
      std::numeric_limits<uint64_t>::max();

  static RtReplayCapture &instance()
  {
    static RtReplayCapture capture;
    return capture;
  }

  uint64_t beginTraverse(
      reg_t hardware_warp_id, uint32_t issue_mask,
      const std::array<reg_t, rt_replay_capture_lanes> &pds_addresses,
      const std::array<reg_t, rt_replay_capture_lanes>
          &private_state_addresses)
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (stream_ == nullptr || captured_commands_ >= command_limit_)
      return inactive_command;

    const uint64_t command = next_command_++;
    ++captured_commands_;
    auto inserted = command_records_.emplace(command, CommandRecord{});
    inserted.first->second.issue_mask = issue_mask;
    inserted.first->second.pds_addresses = pds_addresses;
    std::fprintf(stream_, "T %" PRIu64 " %" PRIu64 " %08" PRIx32 "\n",
                 command, uint64_t(hardware_warp_id), issue_mask);
    for (std::size_t lane = 0; lane < rt_replay_capture_lanes; ++lane) {
      if ((issue_mask & (uint32_t{1} << lane)) == 0)
        continue;
      std::fprintf(stream_,
                   "L %" PRIu64 " %zu %016" PRIx64 " %016" PRIx64 "\n",
                   command, lane, uint64_t(pds_addresses[lane]),
                   uint64_t(private_state_addresses[lane]));
    }
    return command;
  }

  void recordRead(uint64_t command, uint32_t lane, reg_t address,
                  uint32_t value)
  {
    if (command == inactive_command)
      return;
    std::lock_guard<std::mutex> lock(mutex_);
    auto record = command_records_.find(command);
    if (record == command_records_.end())
      return;
    ++record->second.read_count;
  }

  void recordWrite(uint64_t command, uint32_t lane, reg_t address,
                   uint32_t new_value)
  {
    if (command == inactive_command)
      return;
    std::lock_guard<std::mutex> lock(mutex_);
    auto record = command_records_.find(command);
    if (record == command_records_.end())
      return;
    ++record->second.write_count;
    record->second.final_writes[address] = new_value;
    const reg_t pds = record->second.pds_addresses[lane];
    if ((record->second.issue_mask & (uint32_t{1} << lane)) != 0 &&
        address >= pds + rt_v5_pds_committed_base &&
        address < pds + rt_v5_pds_committed_base + 0x60) {
      record->second.committed_writes.push_back(
          {lane, address, new_value});
    }
  }

  bool needsInitialBlock(uint64_t command, reg_t block_address)
  {
    if (command == inactive_command)
      return false;
    std::lock_guard<std::mutex> lock(mutex_);
    auto record = command_records_.find(command);
    return record != command_records_.end() &&
           record->second.initial_blocks.count(block_address) == 0;
  }

  void recordInitialBlock(uint64_t command, reg_t block_address,
                          const std::array<uint32_t, 32> &words)
  {
    if (command == inactive_command)
      return;
    std::lock_guard<std::mutex> lock(mutex_);
    auto record = command_records_.find(command);
    if (record != command_records_.end())
      record->second.initial_blocks.emplace(block_address, words);
  }

  void endTraverse(
      uint64_t command, uint32_t issue_mask,
      const std::array<uint32_t, rt_replay_capture_lanes> &results)
  {
    if (command == inactive_command)
      return;
    std::lock_guard<std::mutex> lock(mutex_);
    auto record = command_records_.find(command);
    if (record == command_records_.end())
      return;
    for (const auto &entry : record->second.initial_blocks) {
      std::fprintf(stream_, "B %" PRIu64 " %016" PRIx64 " ", command,
                   uint64_t(entry.first));
      for (std::size_t word = entry.second.size(); word > 0; --word)
        std::fprintf(stream_, "%08" PRIx32, entry.second[word - 1]);
      std::fputc('\n', stream_);
    }
    for (const auto &entry : record->second.committed_writes) {
      std::fprintf(stream_, "W %" PRIu64 " %" PRIu32 " %016" PRIx64
                            " %08" PRIx32 "\n",
                   command, entry.lane, uint64_t(entry.address), entry.value);
    }
    for (const auto &entry : record->second.final_writes) {
      std::fprintf(stream_, "F %" PRIu64 " %016" PRIx64 " %08" PRIx32
                            "\n",
                   command, uint64_t(entry.first), entry.second);
    }
    for (std::size_t lane = 0; lane < rt_replay_capture_lanes; ++lane) {
      if ((issue_mask & (uint32_t{1} << lane)) == 0)
        continue;
      std::fprintf(stream_, "O %" PRIu64 " %zu %08" PRIx32 "\n",
                   command, lane, results[lane]);
    }
    std::fprintf(stream_,
                 "E %" PRIu64 " %" PRIu64 " %" PRIu64 " %zu %zu\n",
                 command, record->second.read_count, record->second.write_count,
                 record->second.initial_blocks.size(),
                 record->second.final_writes.size());
    command_records_.erase(record);
    std::fflush(stream_);
  }

private:
  RtReplayCapture()
  {
    const char *path = std::getenv("VENTUS_RT_RTL_CAPTURE");
    if (path == nullptr || path[0] == '\0')
      return;

    const char *limit_text = std::getenv("VENTUS_RT_RTL_CAPTURE_LIMIT");
    if (limit_text != nullptr && limit_text[0] != '\0') {
      errno = 0;
      char *end = nullptr;
      const unsigned long long parsed = std::strtoull(limit_text, &end, 0);
      if (errno != 0 || end == limit_text || *end != '\0')
        throw std::runtime_error(
            "ventus-rt: invalid VENTUS_RT_RTL_CAPTURE_LIMIT");
      command_limit_ = uint64_t(parsed);
    }

    stream_ = std::fopen(path, "w");
    if (stream_ == nullptr)
      throw std::runtime_error(
          std::string("ventus-rt: cannot open RTL replay capture: ") + path);
    std::fprintf(stream_, "VENTUS_RT_REPLAY_V4\n");
    std::fflush(stream_);
  }

  ~RtReplayCapture()
  {
    if (stream_ != nullptr)
      std::fclose(stream_);
  }

  RtReplayCapture(const RtReplayCapture &) = delete;
  RtReplayCapture &operator=(const RtReplayCapture &) = delete;

  struct CommandRecord {
    struct CommittedWrite {
      uint32_t lane;
      reg_t address;
      uint32_t value;
    };

    uint64_t read_count = 0;
    uint64_t write_count = 0;
    uint32_t issue_mask = 0;
    std::array<reg_t, rt_replay_capture_lanes> pds_addresses{};
    std::map<reg_t, std::array<uint32_t, 32>> initial_blocks;
    std::map<reg_t, uint32_t> final_writes;
    std::vector<CommittedWrite> committed_writes;
  };

  std::mutex mutex_;
  FILE *stream_ = nullptr;
  uint64_t next_command_ = 0;
  uint64_t captured_commands_ = 0;
  uint64_t command_limit_ = std::numeric_limits<uint64_t>::max();
  std::map<uint64_t, CommandRecord> command_records_;
};

// Preserves the production CanonicalPdsMemory behavior while exposing the
// physical address/value sequence that the traversal model actually saw.
struct CapturingCanonicalPdsMemory {
  CanonicalPdsMemory memory;
  RtReplayCapture *capture;
  uint64_t command;
  uint32_t lane;

  reg_t physicalAddress(reg_t address) const
  {
    return address < rt_v5_pds_size_bytes ? memory.pds_address + address
                                          : address;
  }

  void captureInitialBlock(reg_t address)
  {
    if (command == RtReplayCapture::inactive_command)
      return;
    const reg_t block_address = physicalAddress(address) & ~reg_t{0x7f};
    if (!capture->needsInitialBlock(command, block_address))
      return;

    std::array<uint32_t, 32> words{};
    for (std::size_t word = 0; word < words.size(); ++word)
      words[word] = memory.raw.load32(block_address + word * sizeof(uint32_t));
    capture->recordInitialBlock(command, block_address, words);
  }

  uint32_t load32(reg_t address)
  {
    captureInitialBlock(address);
    const uint32_t value = memory.load32(address);
    capture->recordRead(command, lane, physicalAddress(address), value);
    return value;
  }

  void store32(reg_t address, uint32_t value)
  {
    captureInitialBlock(address);
    if (command != RtReplayCapture::inactive_command) {
      capture->recordWrite(command, lane, physicalAddress(address), value);
    }
    memory.store32(address, value);
  }
};

inline CapturingCanonicalPdsMemory make_capturing_canonical_pds_memory(
    mmu_t &mmu, reg_t pds_address, RtReplayCapture &capture,
    uint64_t command, uint32_t lane)
{
  return {make_canonical_pds_memory(mmu, pds_address), &capture, command,
          lane};
}

} // namespace ventus_rt

#endif
