#ifndef RISCV_VENTUS_RTCORE_MODEL_H
#define RISCV_VENTUS_RTCORE_MODEL_H

#include "ventus_rt.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <unordered_map>

namespace ventus_rt {

constexpr std::size_t v5_warp_lanes = 32;

enum class ResidentWarpState : uint8_t {
  Unbound,
  Active,
  TerminalResident,
};

enum class RequestControlPhase : uint8_t {
  Free,
  Traversing,
  PausedForSoftware,
};

// Functional, software-contract view of one lane request. RTL queue, bank,
// target-unit, and commit-pipeline state deliberately do not appear here.
struct RequestControlEntry {
  reg_t hardware_warp_id = 0;
  uint32_t lane_id = 0;
  reg_t private_state_address = 0;
  RequestControlPhase phase = RequestControlPhase::Free;
};

// V5 resident authority is frozen by the first RT_TRAVERSE active mask and is
// retained until one whole-original-mask RT_RELEASE succeeds.
struct ResidentWarpRecord {
  ResidentWarpState state = ResidentWarpState::Unbound;
  uint32_t original_bound_mask = 0;
  uint32_t live_request_mask = 0;
  uint32_t terminal_mask = 0;
  std::array<reg_t, v5_warp_lanes> software_pds_address{};
  std::array<reg_t, v5_warp_lanes> private_state_address{};
  std::array<uint32_t, v5_warp_lanes> lane_result{};
  std::array<RequestControlEntry, v5_warp_lanes> requests{};
};

struct RtCoreDebugSnapshot {
  uint64_t sm_generation = 1;
  uint64_t traverse_command_count = 0;
  uint64_t release_command_count = 0;
  uint64_t private_context_count = 0;
  bool command_active = false;
};

// One instance is owned by one functional Spike execution partition (SM0 in
// the current single-partition mapping). The model owns traversal
// cursor/frontier/stack state so it cannot leak through a template-static
// global. Trace input, candidate/committed records, and control words remain
// in the remote branch's current PDS ABI.
class RtCoreModel {
public:
  template <typename Memory>
  uint32_t executeTraverse(Memory &memory, reg_t slot)
  {
    CommandScope command(*this);
    const uint32_t status =
        ventus_rt::traverse(memory, slot, private_state_);
    ++traverse_command_count_;
    return status;
  }

  template <typename MemoryFactory>
  std::array<uint32_t, v5_warp_lanes> executeTraverseV5(
      reg_t hardware_warp_id, uint32_t issue_mask,
      const std::array<reg_t, v5_warp_lanes> &pds_addresses,
      const std::array<reg_t, v5_warp_lanes> &private_state_addresses,
      MemoryFactory memory_factory)
  {
    if (issue_mask == 0)
      throw std::runtime_error("ventus-rt: empty V5 RT_TRAVERSE mask");

    CommandScope command(*this);
    auto resident = resident_warps_.find(hardware_warp_id);
    const bool new_admission =
        resident == resident_warps_.end() ||
        resident->second.state == ResidentWarpState::Unbound;

    ResidentWarpRecord next =
        new_admission ? ResidentWarpRecord{} : resident->second;
    if (new_admission) {
      validateNewAdmission(hardware_warp_id, issue_mask, pds_addresses,
                           private_state_addresses);
      next.state = ResidentWarpState::Active;
      next.original_bound_mask = issue_mask;
    } else {
      validateResume(next, issue_mask, pds_addresses,
                     private_state_addresses);
    }

    std::array<uint32_t, v5_warp_lanes> results{};
    for (std::size_t lane = 0; lane < v5_warp_lanes; ++lane) {
      const uint32_t bit = laneBit(lane);
      if ((issue_mask & bit) == 0)
        continue;

      if (new_admission) {
        next.software_pds_address[lane] = pds_addresses[lane];
        next.private_state_address[lane] = private_state_addresses[lane];
        next.requests[lane].hardware_warp_id = hardware_warp_id;
        next.requests[lane].lane_id = uint32_t(lane);
        next.requests[lane].private_state_address =
            private_state_addresses[lane];
      }

      next.requests[lane].phase = RequestControlPhase::Traversing;
      next.live_request_mask |= bit;

      auto memory = memory_factory(reg_t(lane), pds_addresses[lane]);
      V5PrivateMemoryView<decltype(memory)> view{
          memory, private_state_addresses[lane]};
      const uint32_t status =
          ventus_rt::traverse(view, /* canonical PDS base */ 0,
                              private_state_);
      results[lane] = status;
      next.lane_result[lane] = status;

      if (isPausedStatus(status)) {
        next.requests[lane].phase =
            RequestControlPhase::PausedForSoftware;
      } else if (isTerminalStatus(status)) {
        next.live_request_mask &= ~bit;
        next.terminal_mask |= bit;
        next.requests[lane] = RequestControlEntry{};
      } else {
        throw std::runtime_error("ventus-rt: invalid V5 traversal result");
      }
    }

    if (next.terminal_mask == next.original_bound_mask) {
      if (next.live_request_mask != 0)
        throw std::logic_error("ventus-rt: terminal V5 warp has live lanes");
      next.state = ResidentWarpState::TerminalResident;
    }
    validateResidentInvariants(next);
    resident_warps_[hardware_warp_id] = next;
    ++traverse_command_count_;
    return results;
  }

  template <typename Memory>
  void executeRelease(Memory &memory, reg_t slot)
  {
    CommandScope command(*this);
    ventus_rt::release(memory, slot, private_state_);
    ++release_command_count_;
  }

  template <typename MemoryFactory>
  void executeReleaseV5(
      reg_t hardware_warp_id, uint32_t release_mask,
      const std::array<reg_t, v5_warp_lanes> &pds_addresses,
      MemoryFactory memory_factory)
  {
    auto resident = resident_warps_.find(hardware_warp_id);
    if (resident == resident_warps_.end() ||
        resident->second.state == ResidentWarpState::Unbound)
      throw std::runtime_error(
          "ventus-rt: duplicate or stale V5 RT_RELEASE");

    const ResidentWarpRecord &record = resident->second;
    if (record.state != ResidentWarpState::TerminalResident)
      throw std::runtime_error("ventus-rt: early V5 RT_RELEASE");
    if (release_mask != record.original_bound_mask)
      throw std::runtime_error("ventus-rt: partial V5 RT_RELEASE mask");

    for (std::size_t lane = 0; lane < v5_warp_lanes; ++lane) {
      const uint32_t bit = laneBit(lane);
      if ((release_mask & bit) == 0)
        continue;
      if (pds_addresses[lane] != record.software_pds_address[lane])
        throw std::runtime_error("ventus-rt: wrong-PDS V5 RT_RELEASE");
    }

    CommandScope command(*this);
    for (std::size_t lane = 0; lane < v5_warp_lanes; ++lane) {
      const uint32_t bit = laneBit(lane);
      if ((release_mask & bit) == 0)
        continue;
      auto memory = memory_factory(reg_t(lane), pds_addresses[lane]);
      V5PrivateMemoryView<decltype(memory)> view{
          memory, record.private_state_address[lane]};
      ventus_rt::release(view, /* canonical PDS base */ 0, private_state_);
    }

    resident->second = ResidentWarpRecord{};
    ++release_command_count_;
  }

  const ResidentWarpRecord *residentWarpRecord(reg_t hardware_warp_id) const
  {
    auto resident = resident_warps_.find(hardware_warp_id);
    return resident == resident_warps_.end() ? nullptr : &resident->second;
  }

  void resetSm()
  {
    if (command_active_)
      throw std::logic_error("cannot reset an active RTCore model command");

    ++sm_generation_;
    traverse_command_count_ = 0;
    release_command_count_ = 0;
    private_state_.reset();
    resident_warps_.clear();
  }

  RtCoreDebugSnapshot debugSnapshot() const
  {
    return {
        sm_generation_,
        traverse_command_count_,
        release_command_count_,
        private_state_.context_count(),
        command_active_,
    };
  }

private:
  template <typename Memory> struct V5PrivateMemoryView {
    Memory &memory;
    reg_t private_state_address;

    uint64_t rt_context_key(reg_t) const { return private_state_address; }
    uint32_t load32(reg_t address) { return memory.load32(address); }
    void store32(reg_t address, uint32_t value)
    {
      memory.store32(address, value);
    }
    uint64_t rt_private_load64(reg_t offset)
    {
      const uint64_t lo = memory.load32(private_state_address + offset);
      const uint64_t hi = memory.load32(private_state_address + offset + 4);
      return lo | (hi << 32);
    }
    uint32_t rt_private_load32(reg_t offset)
    {
      return memory.load32(private_state_address + offset);
    }
    void rt_private_store32(reg_t offset, uint32_t value)
    {
      memory.store32(private_state_address + offset, value);
    }
    void rt_private_store64(reg_t offset, uint64_t value)
    {
      memory.store32(private_state_address + offset, uint32_t(value));
      memory.store32(private_state_address + offset + 4,
                     uint32_t(value >> 32));
    }
  };

  static uint32_t laneBit(std::size_t lane)
  {
    return uint32_t{1} << lane;
  }

  static bool isPausedStatus(uint32_t status)
  {
    return status == traversal_candidate_non_opaque_triangle ||
           status == traversal_candidate_procedural_aabb;
  }

  static bool isTerminalStatus(uint32_t status)
  {
    return status == traversal_complete_miss ||
           status == traversal_complete_hit || status == traversal_terminated;
  }

  static void validateAddressAlignment(reg_t pds_address,
                                       reg_t private_state_address)
  {
    if ((pds_address & 0x3) != 0 || (private_state_address & 0x7f) != 0)
      throw std::runtime_error("ventus-rt: invalid V5 RT address alignment");
  }

  void validateNewAdmission(
      reg_t hardware_warp_id, uint32_t issue_mask,
      const std::array<reg_t, v5_warp_lanes> &pds_addresses,
      const std::array<reg_t, v5_warp_lanes> &private_state_addresses) const
  {
    for (std::size_t lane = 0; lane < v5_warp_lanes; ++lane) {
      const uint32_t bit = laneBit(lane);
      if ((issue_mask & bit) == 0)
        continue;

      validateAddressAlignment(pds_addresses[lane],
                               private_state_addresses[lane]);
      for (std::size_t previous = 0; previous < lane; ++previous) {
        if ((issue_mask & laneBit(previous)) == 0)
          continue;
        if (pds_addresses[previous] == pds_addresses[lane] ||
            private_state_addresses[previous] ==
                private_state_addresses[lane])
          throw std::runtime_error(
              "ventus-rt: duplicate V5 lane address binding");
      }

      for (const auto &owner : resident_warps_) {
        if (owner.first == hardware_warp_id ||
            owner.second.state == ResidentWarpState::Unbound)
          continue;
        const ResidentWarpRecord &record = owner.second;
        for (std::size_t owner_lane = 0; owner_lane < v5_warp_lanes;
             ++owner_lane) {
          if ((record.original_bound_mask & laneBit(owner_lane)) == 0)
            continue;
          if (record.software_pds_address[owner_lane] == pds_addresses[lane] ||
              record.private_state_address[owner_lane] ==
                  private_state_addresses[lane])
            throw std::runtime_error(
                "ventus-rt: V5 address already resident");
        }
      }
    }
  }

  static void validateResume(
      const ResidentWarpRecord &record, uint32_t issue_mask,
      const std::array<reg_t, v5_warp_lanes> &pds_addresses,
      const std::array<reg_t, v5_warp_lanes> &private_state_addresses)
  {
    if (record.state != ResidentWarpState::Active)
      throw std::runtime_error("ventus-rt: V5 traverse after rendezvous");
    if ((issue_mask & ~record.original_bound_mask) != 0)
      throw std::runtime_error("ventus-rt: V5 resume changed frozen mask");

    for (std::size_t lane = 0; lane < v5_warp_lanes; ++lane) {
      const uint32_t bit = laneBit(lane);
      if ((issue_mask & bit) == 0)
        continue;
      validateAddressAlignment(pds_addresses[lane],
                               private_state_addresses[lane]);
      if (record.requests[lane].phase !=
          RequestControlPhase::PausedForSoftware)
        throw std::runtime_error("ventus-rt: V5 lane is not resumable");
      if (pds_addresses[lane] != record.software_pds_address[lane] ||
          private_state_addresses[lane] !=
              record.private_state_address[lane])
        throw std::runtime_error("ventus-rt: V5 resume address mismatch");
    }
  }

  static void validateResidentInvariants(const ResidentWarpRecord &record)
  {
    const uint32_t outside =
        (record.live_request_mask | record.terminal_mask) &
        ~record.original_bound_mask;
    if (outside != 0 ||
        (record.live_request_mask & record.terminal_mask) != 0)
      throw std::logic_error("ventus-rt: invalid V5 resident masks");

    for (std::size_t lane = 0; lane < v5_warp_lanes; ++lane) {
      const uint32_t bit = laneBit(lane);
      const bool live = (record.live_request_mask & bit) != 0;
      const bool paused = record.requests[lane].phase ==
                          RequestControlPhase::PausedForSoftware;
      if (live != paused)
        throw std::logic_error("ventus-rt: invalid V5 lane request state");
    }
  }

  class CommandScope {
  public:
    explicit CommandScope(RtCoreModel &model) : model_(model)
    {
      if (model_.command_active_)
        throw std::logic_error("RTCore model command re-entry");
      model_.command_active_ = true;
    }

    ~CommandScope()
    {
      model_.command_active_ = false;
    }

    CommandScope(const CommandScope &) = delete;
    CommandScope &operator=(const CommandScope &) = delete;

  private:
    RtCoreModel &model_;
  };

  uint64_t sm_generation_ = 1;
  uint64_t traverse_command_count_ = 0;
  uint64_t release_command_count_ = 0;
  RtPrivateState private_state_;
  std::unordered_map<reg_t, ResidentWarpRecord> resident_warps_;
  bool command_active_ = false;
};

} // namespace ventus_rt

#endif
