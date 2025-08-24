#pragma once

#include <cstdint>
#include <vector>
#include "gvmref_interface.h"
#include "sim.h"
#include "processor.h"
#include "workgroup.h"
#include "gvmref.h"

static gvmref_t* ref = nullptr;

extern "C" {

int gvmref_vt_dev_open() {
  if (ref != nullptr) {
    delete ref;
  }
  ref = new gvmref_t();
  // 使用 std::make_unique 创建 workgroup_t 对象
  ref->wg.push_back(std::make_unique<workgroup_t>());
  return 0;
}
int gvmref_vt_dev_close() {
  if (ref != nullptr) {
    delete ref;
  }
  return 0;
}
int gvmref_vt_buf_alloc(uint64_t size, uint64_t *vaddr, int BUF_TYPE, uint64_t taskID, uint64_t kernelID) {
  if(size <= 0) return -1;
  int ret = 0;
  for (uint32_t wg_id = 0; wg_id < ref->wg.size(); wg_id++) {
    ret = ref->wg[wg_id]->alloc_local_mem(size, vaddr);
  }
  return ret;
}
int gvmref_vt_buf_free(uint64_t size, uint64_t *vaddr, uint64_t taskID, uint64_t kernelID) {
  if(size <= 0) return -1;
  int ret = 0;
  for (uint32_t wg_id = 0; wg_id < ref->wg.size(); wg_id++) {
    ret = ref->wg[wg_id]->free_local_mem();
  }
  return ret;
}
int gvmref_vt_one_buf_free(uint64_t size, uint64_t *vaddr, uint64_t taskID, uint64_t kernelID) {
  if(size <= 0) return -1;
  int ret = 0;
  for (uint32_t wg_id = 0; wg_id < ref->wg.size(); wg_id++) {
    ret = ref->wg[wg_id]->free_local_mem(*vaddr);
  }
  return ret;
}
int gvmref_vt_copy_to_dev(uint64_t dev_vaddr, const void *src_addr, uint64_t size, uint64_t taskID, uint64_t kernelID) {
  if(size <= 0) return -1;
  int ret = 0;
  for (uint32_t wg_id = 0; wg_id < ref->wg.size(); wg_id++) {
    ret = ref->wg[wg_id]->copy_to_dev(dev_vaddr, size, src_addr);
  }
  return ret;
}
int gvmref_vt_upload_kernel_file(const char* filename, int taskID) {
  for (uint32_t wg_id = 0; wg_id < ref->wg.size(); wg_id++) {
    ref->wg[wg_id]->set_filename(filename);
  }
  return 0;
}
int gvmref_vt_start(void* metaData, uint64_t taskID) {
  assert(ref->wg.size() == 1);
  auto knl_data = (gvmref_meta_data *) metaData;
  ref->num_workgroup = (knl_data->kernel_size[0]) * (knl_data->kernel_size[1]) * (knl_data->kernel_size[2]);
  ref->num_warp = knl_data->wg_size;
  // 改为使用 std::make_unique 并调用深拷贝构造函数
  for (int i = 1; i < ref->num_workgroup; i++) {
    ref->wg.push_back(std::make_unique<workgroup_t>(*ref->wg[0]));
  }
  for (int i = 0; i < ref->num_workgroup; i++) {
    ref->wg[i]->init_sim(knl_data, 0x80000000, i);
  }
  return 0;
}
  
// 以下是 GVM 需要使用的 API
int gvmref_set_warp_xreg(uint32_t software_wg_id, uint32_t software_warp_id, uint32_t xreg_usage, gvmref_warp_xreg_t xreg_data) {
  ref->wg[software_wg_id]->set_warp_xreg(software_warp_id, xreg_usage, xreg_data);
  return 0;
}

uint32_t gvmref_get_next_pc(uint32_t software_wg_id, uint32_t software_warp_id) {
  return ref->wg[software_wg_id]->get_next_pc(software_warp_id);
}

void gvmref_step(uint32_t software_wg_id, uint32_t software_warp_id, gvmref_step_return_info_t* ret) {
  ref->wg[software_wg_id]->step(software_warp_id);
  gvmref_step_return_info_t ret_info;
  ret_info = ref->wg[software_wg_id]->proc[software_warp_id]->gvmref_step_ret;
  ret_info.wg_done = ref->wg[software_wg_id]->done();
  *ret = ret_info;
}

void gvmref_get_xreg(gvmref_xreg_t* ret) {
  gvmref_xreg_t result;
  for (uint32_t wg_id = 0; wg_id < ref->num_workgroup; wg_id++) {
    std::vector<std::array<uint64_t, 256>> wg_xpr; // const int NXPR = 256;
    for (uint32_t warp_id = 0; warp_id < ref->num_warp; warp_id++) {
      std::array<uint64_t, 256> warp_xpr;
      for (int i = 0; i < 256; i++) {
        warp_xpr[i] = ref->wg[wg_id]->state[warp_id]->XPR[i];
      }
      wg_xpr.push_back(warp_xpr);
    }
    result.xpr.push_back(wg_xpr);
  }
  *ret = result;
}

} // extern "C"