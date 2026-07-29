#ifndef RISCV_VENTUS_RTCORE_MODEL_H
#define RISCV_VENTUS_RTCORE_MODEL_H

#include "ventus_rt.h"

#include <cstdint>
#include <stdexcept>

namespace ventus_rt {

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

  template <typename Memory>
  void executeRelease(Memory &memory, reg_t slot)
  {
    CommandScope command(*this);
    ventus_rt::release(memory, slot, private_state_);
    ++release_command_count_;
  }

  void resetSm()
  {
    if (command_active_)
      throw std::logic_error("cannot reset an active RTCore model command");

    ++sm_generation_;
    traverse_command_count_ = 0;
    release_command_count_ = 0;
    private_state_.reset();
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
  bool command_active_ = false;
};

} // namespace ventus_rt

#endif
