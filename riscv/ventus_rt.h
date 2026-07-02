#ifndef RISCV_VENTUS_RT_H
#define RISCV_VENTUS_RT_H

#include "decode.h"
#include "mmu.h"

namespace ventus_rt {

constexpr reg_t lanes = 32;

constexpr reg_t slot_status = 0;

constexpr reg_t control_base = 80;
constexpr reg_t control_done = 0;
constexpr reg_t control_incomplete = 1;
constexpr reg_t control_accept_hit = 2;
constexpr reg_t control_ignore_hit = 3;
constexpr reg_t control_terminate_ray = 4;
constexpr reg_t control_skip_closest_hit = 5;

constexpr reg_t committed_hit_record_base = 192;
constexpr reg_t hit_record_status = 0;

constexpr uint32_t rt_status_hit = 2;
constexpr uint32_t rt_status_miss = 3;
constexpr uint32_t rt_status_done = 4;

constexpr uint32_t traversal_complete_miss = 0;
constexpr uint32_t traversal_complete_hit = 1;
constexpr uint32_t traversal_terminated = 4;

inline reg_t word_addr(reg_t slot, reg_t byte_base, reg_t word)
{
  return slot + byte_base + word * sizeof(uint32_t);
}

inline uint32_t load_word(mmu_t &mmu, reg_t slot, reg_t byte_base, reg_t word)
{
  return uint32_t(mmu.load_uint32(word_addr(slot, byte_base, word)));
}

inline void store_word(mmu_t &mmu, reg_t slot, reg_t byte_base, reg_t word,
                       uint32_t value)
{
  mmu.store_uint32(word_addr(slot, byte_base, word), value);
}

} // namespace ventus_rt

#endif
