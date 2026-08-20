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
  uint32_t executeTraverseV5(Memory &memory, reg_t pds_address,
                             reg_t private_state_address)
  {
    if ((pds_address & 0x3) != 0 || (private_state_address & 0x7f) != 0)
      throw std::runtime_error("ventus-rt: invalid V5 RT address alignment");

    CommandScope command(*this);
    bindV5Addresses(pds_address, private_state_address);
    V5PrivateMemoryView<Memory> view{memory, private_state_address};
    const uint32_t status =
        ventus_rt::traverse(view, /* canonical PDS base */ 0, private_state_);
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

  template <typename Memory>
  void executeReleaseV5(Memory &memory, reg_t pds_address)
  {
    CommandScope command(*this);
    auto binding = v5_pds_to_private_.find(pds_address);
    const reg_t private_state_address =
        binding == v5_pds_to_private_.end() ? pds_address : binding->second;
    V5PrivateMemoryView<Memory> view{memory, private_state_address};
    ventus_rt::release(view, /* canonical PDS base */ 0, private_state_);
    if (binding != v5_pds_to_private_.end()) {
      v5_private_to_pds_.erase(binding->second);
      v5_pds_to_private_.erase(binding);
    }
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
    v5_pds_to_private_.clear();
    v5_private_to_pds_.clear();
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
  };

  void bindV5Addresses(reg_t pds_address, reg_t private_state_address)
  {
    auto pds_binding = v5_pds_to_private_.find(pds_address);
    if (pds_binding != v5_pds_to_private_.end() &&
        pds_binding->second != private_state_address)
      throw std::runtime_error("ventus-rt: V5 PDS/private binding changed");

    auto private_binding = v5_private_to_pds_.find(private_state_address);
    if (private_binding != v5_private_to_pds_.end() &&
        private_binding->second != pds_address)
      throw std::runtime_error("ventus-rt: V5 private/PDS binding changed");

    v5_pds_to_private_[pds_address] = private_state_address;
    v5_private_to_pds_[private_state_address] = pds_address;
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
  std::unordered_map<reg_t, reg_t> v5_pds_to_private_;
  std::unordered_map<reg_t, reg_t> v5_private_to_pds_;
  bool command_active_ = false;
};

} // namespace ventus_rt

#endif
