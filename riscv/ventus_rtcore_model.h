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
      const uint32_t traversal_status =
          ventus_rt::traverse(view, /* canonical PDS base */ 0,
                              private_state_);
      const uint32_t compact_result =
          compactResult(traversal_status, view.hasCommittedHit());
      results[lane] = compact_result;
      next.lane_result[lane] = compact_result;

      if (isPausedStatus(traversal_status)) {
        next.requests[lane].phase =
            RequestControlPhase::PausedForSoftware;
      } else if (isTerminalStatus(traversal_status)) {
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
    uint32_t callback_effect = 0;

    uint64_t rt_context_key(reg_t) const { return private_state_address; }
    uint32_t load32(reg_t address)
    {
      if (address >= rt_v5_pds_size_bytes)
        return memory.load32(address);

      if (address == word_addr(0, control_base,
                               control_callback_decision)) {
        callback_effect = memory.load32(rt_v5_pds_callback_base);
        if ((callback_effect & ~uint32_t{3}) != 0 ||
            ((callback_effect & uint32_t{2}) != 0 &&
             (callback_effect & uint32_t{1}) == 0))
          return callback_pending;
        if (callback_effect & uint32_t{2})
          return callback_terminate;
        return (callback_effect & uint32_t{1}) ? callback_accept
                                               : callback_ignore;
      }

      if (address == word_addr(0, 0, slot_status))
        return slot_status_trace_request;
      if (address == word_addr(0, 0, slot_accel_lo))
        return memory.load32(rt_v5_pds_traversable_reference);
      if (address == word_addr(0, 0, slot_accel_hi))
        return memory.load32(rt_v5_pds_traversable_reference + 4);
      if (address == word_addr(0, 0, slot_flags))
        return memory.load32(rt_v5_pds_ray_flags);
      if (address == word_addr(0, 0, slot_cull_mask))
        return memory.load32(rt_v5_pds_trace_selector_control) & 0xffu;
      if (address == word_addr(0, 0, slot_sbt_offset))
        return (memory.load32(rt_v5_pds_trace_selector_control) >> 8) & 0xfu;
      if (address == word_addr(0, 0, slot_sbt_stride))
        return (memory.load32(rt_v5_pds_trace_selector_control) >> 12) & 0xfu;
      if (address == word_addr(0, 0, slot_miss_index))
        return memory.load32(rt_v5_pds_trace_selector_control) >> 16;
      if (address >= word_addr(0, 0, slot_origin_x) &&
          address <= word_addr(0, 0, slot_tmax))
        return memory.load32(address);

      reg_t base = 0;
      reg_t word = 0;
      if (legacyHitAddress(address, base, word))
        return loadLegacyHitWord(base, word);
      return memory.load32(address);
    }
    void store32(reg_t address, uint32_t value)
    {
      if (address >= rt_v5_pds_size_bytes) {
        memory.store32(address, value);
        return;
      }
      if (address == word_addr(0, control_base,
                               control_callback_decision)) {
        memory.store32(rt_v5_pds_callback_base, 0);
        return;
      }
      if (address == word_addr(0, 0, slot_status) ||
          address == word_addr(0, 0, slot_hit_t) ||
          address == word_addr(0, 0, slot_sbt_index))
        return;

      reg_t base = 0;
      reg_t word = 0;
      if (legacyHitAddress(address, base, word)) {
        storeLegacyHitWord(base, word, value);
        return;
      }
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
    bool hasCommittedHit()
    {
      return (memory.load32(rt_v5_pds_committed_base + rt_v5_hit_metadata) &
              rt_v5_hit_metadata_committed_valid) != 0;
    }

  private:
    static bool legacyHitAddress(reg_t address, reg_t &base, reg_t &word)
    {
      if (address >= candidate_hit_record_base &&
          address < candidate_hit_record_base + 19 * sizeof(uint32_t)) {
        base = rt_v5_pds_callback_base;
        word = (address - candidate_hit_record_base) / sizeof(uint32_t);
        return true;
      }
      if (address >= committed_hit_record_base &&
          address < committed_hit_record_base + 19 * sizeof(uint32_t)) {
        base = rt_v5_pds_committed_base;
        word = (address - committed_hit_record_base) / sizeof(uint32_t);
        return true;
      }
      return false;
    }

    uint32_t loadLegacyHitWord(reg_t base, reg_t word)
    {
      const uint32_t metadata = memory.load32(base + rt_v5_hit_metadata);
      switch (word) {
      case hit_record_status:
        return base == rt_v5_pds_committed_base
                   ? ((metadata & rt_v5_hit_metadata_committed_valid) != 0)
                   : hit_record_valid;
      case hit_record_hit_t:
        return memory.load32(base + rt_v5_hit_t);
      case hit_record_sbt_index:
      case hit_record_shader_record_ptr_lo:
      case hit_record_shader_record_ptr_hi:
        return 0;
      case hit_record_primitive_id:
        return memory.load32(base + rt_v5_hit_primitive_index);
      case hit_record_instance_id:
        return memory.load32(base + rt_v5_hit_instance_index);
      case hit_record_geometry_id:
        return memory.load32(base + rt_v5_hit_geometry_index);
      case hit_record_hit_kind:
        return (metadata >> rt_v5_hit_metadata_hit_kind_shift) & 0xffu;
      case hit_record_barycentrics_x:
        return memory.load32(base + rt_v5_hit_attributes);
      case hit_record_barycentrics_y:
        return memory.load32(base + rt_v5_hit_attributes + 4);
      case hit_record_primitive_addr_lo:
        return memory.load32(base + rt_v5_hit_primitive_reference);
      case hit_record_primitive_addr_hi:
        return memory.load32(base + rt_v5_hit_primitive_reference + 4);
      case hit_record_instance_addr_lo:
        return memory.load32(base + rt_v5_hit_instance_metadata_reference);
      case hit_record_instance_addr_hi:
        return memory.load32(base + rt_v5_hit_instance_metadata_reference + 4);
      case hit_record_front_face:
        return ((metadata >> rt_v5_hit_metadata_hit_kind_shift) & 0xffu) ==
               triangle_hit_kind_front_face;
      case hit_record_opaque:
        return 0;
      case hit_record_need_software_opacity_test:
        return (metadata & rt_v5_hit_metadata_procedural_any_hit) != 0;
      case hit_record_instance_sbt_record_offset:
        return memory.load32(base + rt_v5_hit_instance_sbt_contribution);
      default:
        return 0;
      }
    }

    void updateMetadata(reg_t base, uint32_t clear_mask, uint32_t set_mask)
    {
      uint32_t metadata = memory.load32(base + rt_v5_hit_metadata);
      metadata = (metadata & ~clear_mask) | set_mask;
      memory.store32(base + rt_v5_hit_metadata, metadata);
    }

    void storeLegacyHitWord(reg_t base, reg_t word, uint32_t value)
    {
      const bool committed = base == rt_v5_pds_committed_base;
      switch (word) {
      case hit_record_status:
        if (committed) {
          updateMetadata(base, rt_v5_hit_metadata_committed_valid,
                         value == hit_record_valid
                             ? rt_v5_hit_metadata_committed_valid
                             : 0);
        }
        return;
      case hit_record_hit_t:
        memory.store32(base + rt_v5_hit_t, value);
        return;
      case hit_record_sbt_index:
      case hit_record_shader_record_ptr_lo:
      case hit_record_shader_record_ptr_hi:
        return;
      case hit_record_primitive_id:
        memory.store32(base + rt_v5_hit_primitive_index, value);
        return;
      case hit_record_instance_id:
        memory.store32(base + rt_v5_hit_instance_index, value);
        return;
      case hit_record_geometry_id:
        memory.store32(base + rt_v5_hit_geometry_index, value);
        return;
      case hit_record_hit_kind: {
        uint32_t metadata = memory.load32(base + rt_v5_hit_metadata);
        if ((metadata & rt_v5_hit_metadata_geometry_mask) == 0) {
          const bool triangle = value == triangle_hit_kind_front_face ||
                                value == triangle_hit_kind_back_face;
          metadata |= triangle
                          ? rt_v5_hit_geometry_triangle |
                                (8u <<
                                 rt_v5_hit_metadata_attribute_count_shift) |
                                (rt_v5_hit_attribute_barycentric_fp32x2
                                 << rt_v5_hit_metadata_attribute_format_shift)
                          : rt_v5_hit_geometry_procedural_aabb;
        }
        metadata &= ~(0xffu << rt_v5_hit_metadata_hit_kind_shift);
        metadata |= (value & 0xffu)
                    << rt_v5_hit_metadata_hit_kind_shift;
        memory.store32(base + rt_v5_hit_metadata, metadata);
        return;
      }
      case hit_record_barycentrics_x:
        memory.store32(base + rt_v5_hit_attributes, value);
        return;
      case hit_record_barycentrics_y:
        memory.store32(base + rt_v5_hit_attributes + 4, value);
        return;
      case hit_record_primitive_addr_lo:
        memory.store32(base + rt_v5_hit_primitive_reference, value);
        return;
      case hit_record_primitive_addr_hi:
        memory.store32(base + rt_v5_hit_primitive_reference + 4, value);
        return;
      case hit_record_instance_addr_lo:
        memory.store32(base + rt_v5_hit_instance_metadata_reference, value);
        return;
      case hit_record_instance_addr_hi:
        memory.store32(base + rt_v5_hit_instance_metadata_reference + 4,
                       value);
        return;
      case hit_record_need_software_opacity_test:
        updateMetadata(base, rt_v5_hit_metadata_procedural_any_hit,
                       value ? rt_v5_hit_metadata_procedural_any_hit : 0);
        return;
      case hit_record_instance_sbt_record_offset:
        memory.store32(base + rt_v5_hit_instance_sbt_contribution, value);
        return;
      case hit_record_front_face:
      case hit_record_opaque:
        return;
      default:
        return;
      }
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

  static uint32_t compactResult(uint32_t status, bool committed_hit)
  {
    uint32_t reason = rt_v5_return_fault;
    switch (status) {
    case traversal_complete_miss:
      reason = rt_v5_return_miss;
      break;
    case traversal_complete_hit:
      reason = rt_v5_return_closest_hit_ready;
      break;
    case traversal_candidate_non_opaque_triangle:
      reason = rt_v5_return_any_hit_required;
      break;
    case traversal_candidate_procedural_aabb:
      reason = rt_v5_return_intersection_required;
      break;
    case traversal_terminated:
      reason = committed_hit ? rt_v5_return_hit_done_no_shader
                             : rt_v5_return_miss;
      break;
    }
    return rt_v5_completion_valid | reason;
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
