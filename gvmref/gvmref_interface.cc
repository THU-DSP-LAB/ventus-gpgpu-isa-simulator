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
  ref->wg.insert({0, std::make_unique<workgroup_t>()});
  return 0;
}
int gvmref_vt_dev_close() {
  if (ref != nullptr) {
    ref->wg[ref->wg_id_base]->clear_buffer_data();
    delete ref;
  }
  return 0;
}
int gvmref_vt_buf_alloc(uint64_t size, uint64_t *vaddr, int BUF_TYPE, uint64_t taskID, uint64_t kernelID) {
  if(size <= 0) return -1;
  int ret = 0;
  ret = ref->wg[ref->wg_id_base]->alloc_local_mem(size, vaddr);
  return ret;
}
int gvmref_vt_buf_free(uint64_t size, uint64_t *vaddr, uint64_t taskID, uint64_t kernelID) {
  if(size <= 0) return -1;
  int ret = 0;
  ret = ref->wg[ref->wg_id_base]->free_local_mem();
  return ret;
}
int gvmref_vt_one_buf_free(uint64_t size, uint64_t *vaddr, uint64_t taskID, uint64_t kernelID) {
  if(size <= 0) return -1;
  int ret = 0;
  ret = ref->wg[ref->wg_id_base]->free_local_mem(*vaddr);
  return ret;
}
int gvmref_vt_copy_to_dev(uint64_t dev_vaddr, const void *src_addr, uint64_t size, uint64_t taskID, uint64_t kernelID) {
  if(size <= 0) return -1;
  int ret = 0;
  ret = ref->wg[ref->wg_id_base]->copy_to_dev(dev_vaddr, size, src_addr);
  return ret;
}
int gvmref_vt_upload_kernel_file(const char* filename, int taskID) {
  ref->wg[ref->wg_id_base]->set_filename(filename);
  return 0;
}
int gvmref_vt_start(void* metaData, uint64_t taskID) {
  if(ref->num_workgroup != 0){
    // 移动 base_wg
    ref->wg.insert({ref->wg_id_base + ref->num_workgroup, std::make_unique<workgroup_t>(*ref->wg[ref->wg_id_base], true)});
  }
  ref->wg_id_base = ref->wg_id_base + ref->num_workgroup;
  auto knl_data = (gvmref_meta_data *) metaData;
  ref->num_workgroup = (knl_data->kernel_size[0]) * (knl_data->kernel_size[1]) * (knl_data->kernel_size[2]);
  ref->num_warp = knl_data->wg_size;
  // 调用拷贝构造函数
  for (int i = ref->wg_id_base + 1; i < ref->wg_id_base + ref->num_workgroup; i++) {
    ref->wg.insert({i, std::make_unique<workgroup_t>(*ref->wg[ref->wg_id_base], false)});
  }
  for (int i = ref->wg_id_base; i < ref->wg_id_base + ref->num_workgroup; i++) {
    ref->wg[i]->init_sim(knl_data, 0x80000000, i - ref->wg_id_base);
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
  return;
}

void gvmref_get_xreg(gvmref_xreg_t* ret, uint32_t wg_id, uint32_t warp_id) {
  gvmref_xreg_t result;
  std::array<uint64_t, 256> warp_xpr;
  for (int i = 0; i < 256; i++) {
    warp_xpr[i] = ref->wg[wg_id]->state[warp_id]->XPR[i];
  }
  result.xpr = warp_xpr;
  *ret = result;
  return;
}

} // extern "C"