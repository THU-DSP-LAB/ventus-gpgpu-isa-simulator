#ifndef RISCV_VENTUS_RTCORE_MODEL_H
#define RISCV_VENTUS_RTCORE_MODEL_H

#include "ventus_rt.h"

#include <array>
#include <cstdint>
#include <stdexcept>
#include <utility>

namespace ventus_rt {

// Transitional command accepted by the P1 compatibility boundary.  The slot
// values and their interpretation remain the legacy shader-visible PDS ABI;
// they are deliberately not named handoff addresses.
struct LegacyWarpIssue {
  uint32_t active_mask = 0;
  uint32_t first_lane = 0;
  uint32_t lane_count = 0;
  std::array<reg_t, lanes> lane_slots{};
};

struct LegacyWarpResult {
  uint32_t valid_mask = 0;
  std::array<uint32_t, lanes> lane_status{};
};

struct RtCoreDebugSnapshot {
  uint64_t sm_generation = 1;
  uint64_t traverse_issue_count = 0;
  uint64_t release_issue_count = 0;
  uint64_t private_context_count = 0;
  bool command_active = false;
};

// The only place where the P1 model may interpret the legacy PDS contract.
// A later native handoff adapter replaces this class without changing the
// SM-owned model/instruction boundary established by this phase.
class LegacyPdsAdapter {
public:
  template <typename MemoryFactory>
  static LegacyWarpResult traverse(RtPrivateState &private_state,
                                   const LegacyWarpIssue &issue,
                                   MemoryFactory &&make_memory)
  {
    LegacyWarpResult result;
    const uint32_t mask = effective_mask(issue);

    for (uint32_t lane = 0; lane < issue.lane_count; ++lane) {
      const uint32_t lane_bit = uint32_t{1} << lane;
      if ((mask & lane_bit) == 0)
        continue;

      auto memory = make_memory(lane);
      result.lane_status[lane] =
          ventus_rt::traverse(memory, issue.lane_slots[lane], private_state);
      result.valid_mask |= lane_bit;
    }

    return result;
  }

  template <typename MemoryFactory>
  static uint32_t release(RtPrivateState &private_state,
                          const LegacyWarpIssue &issue,
                          MemoryFactory &&make_memory)
  {
    const uint32_t mask = effective_mask(issue);

    for (uint32_t lane = 0; lane < issue.lane_count; ++lane) {
      const uint32_t lane_bit = uint32_t{1} << lane;
      if ((mask & lane_bit) == 0)
        continue;

      auto memory = make_memory(lane);
      ventus_rt::release(memory, issue.lane_slots[lane], private_state);
    }

    return mask;
  }

private:
  static uint32_t effective_mask(const LegacyWarpIssue &issue)
  {
    if (issue.lane_count > lanes)
      throw std::invalid_argument("legacy RT warp lane count exceeds 32");

    const uint32_t lane_limit =
        issue.lane_count == lanes
            ? ~uint32_t{0}
            : (issue.lane_count == 0
                   ? uint32_t{0}
                   : (uint32_t{1} << issue.lane_count) - uint32_t{1});
    const uint32_t first_lane_mask =
        issue.first_lane >= lanes
            ? ~uint32_t{0}
            : (issue.first_lane == 0
                   ? uint32_t{0}
                   : (uint32_t{1} << issue.first_lane) - uint32_t{1});
    return issue.active_mask & lane_limit & ~first_lane_mask;
  }
};

// One instance is owned by one functional Spike execution partition (SM0 in
// the current single-partition mapping).  P1 also owns the compatibility
// traversal cursor/frontier/stack so it cannot leak through a template-static
// global.  Legacy trace input, candidate/committed records, and control words
// remain in PDS until the native identity/lifecycle model replaces the adapter.
class RtCoreModel {
public:
  template <typename MemoryFactory>
  LegacyWarpResult executeLegacyTraverse(const LegacyWarpIssue &issue,
                                         MemoryFactory &&make_memory)
  {
    CommandScope command(*this);
    LegacyWarpResult result = LegacyPdsAdapter::traverse(
        private_state_, issue, std::forward<MemoryFactory>(make_memory));
    ++traverse_issue_count_;
    return result;
  }

  template <typename MemoryFactory>
  uint32_t executeLegacyRelease(const LegacyWarpIssue &issue,
                                MemoryFactory &&make_memory)
  {
    CommandScope command(*this);
    const uint32_t released_mask = LegacyPdsAdapter::release(
        private_state_, issue, std::forward<MemoryFactory>(make_memory));
    ++release_issue_count_;
    return released_mask;
  }

  void resetSm()
  {
    if (command_active_)
      throw std::logic_error("cannot reset an active RTCore model command");

    ++sm_generation_;
    traverse_issue_count_ = 0;
    release_issue_count_ = 0;
    private_state_.reset();
  }

  RtCoreDebugSnapshot debugSnapshot() const
  {
    return {
        sm_generation_,
        traverse_issue_count_,
        release_issue_count_,
        private_state_.context_count(),
        command_active_,
    };
  }

private:
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
  uint64_t traverse_issue_count_ = 0;
  uint64_t release_issue_count_ = 0;
  RtPrivateState private_state_;
  bool command_active_ = false;
};

} // namespace ventus_rt

#endif
