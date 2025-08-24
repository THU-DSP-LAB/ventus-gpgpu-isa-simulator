#pragma once

#include <cstdint>
#include <vector>
#include "gvmref_interface.h"
#include "sim.h"
#include "processor.h"
#include "workgroup.h"

class gvmref_t {
public:
  gvmref_t() = default;
  ~gvmref_t() = default;
  std::vector<std::unique_ptr<workgroup_t>> wg;
  uint32_t num_workgroup; // 工作组数目
  uint32_t num_warp;      // 每个工作组的warp数目

};