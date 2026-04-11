#pragma once

#include <cstdint>
#include <deque>
#include <vector>
#include "gvmref_interface.h"
#include "sim.h"
#include "processor.h"
#include "workgroup.h"

class gvmref_t {
public:
  gvmref_t() = default;
  ~gvmref_t();
  void release_kernel_range(uint32_t kernel_wg_base, uint32_t kernel_num_workgroup);
  void on_kernel_started(uint32_t kernel_wg_base, uint32_t kernel_num_workgroup);
  void on_kernel_finished();
  std::map<uint32_t, std::unique_ptr<workgroup_t>> wg;
  uint32_t num_workgroup = 0; // 工作组数目
  uint32_t num_warp = 0;      // 每个工作组的warp数目
  uint32_t wg_id_base = 0;    // 每次启动新kernel时，软件wg id的起始值。初值为0

private:
  struct kernel_range_t {
    uint32_t wg_base;
    uint32_t num_workgroup;
    bool finished;
  };
  void reclaim_finished_kernel_ranges();

  std::deque<kernel_range_t> kernel_ranges;
};
