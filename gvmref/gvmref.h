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
  ~gvmref_t();
  std::map<uint32_t, std::unique_ptr<workgroup_t>> wg;
  uint32_t num_workgroup; // 工作组数目
  uint32_t num_warp;      // 每个工作组的warp数目
  uint32_t wg_id_base;    // 每次启动新kernel时，软件wg id的起始值。初值为0 

};