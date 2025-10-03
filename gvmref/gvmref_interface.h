// Reference 头文件
// 包括 Reference API，以及使用 API 时需要的结构体定义
// 在 sim-verilator 仿真环境中，该头文件只用于 API 函数声明，具体函数功能通过 .so 动态库调用

#pragma once

#include <cstdint>
#include <vector>
#include <array>
#include <map>

extern "C" {

struct gvmref_warp_xreg_t {
  std::vector<uint64_t> xreg;
}; // 用于 gvmref_set_warp_xreg API

struct gvmref_xreg_t {
  std::array<uint64_t, 256> xpr; // const int NXPR = 256;
};

enum gvmref_insn_type {
  DONT_CARE, // 其他指令
  XREG, // 写回标量寄存器的指令
  VREG // 写回向量寄存器的指令
};

struct gvmref_xreg_result_t {
  uint32_t rd; // 写回数据
  uint32_t reg_idx; // 写回地址
};
struct gvmref_vreg_result_t {
  std::array<uint32_t, 32> rd; // 写回数据
  uint32_t reg_idx; // 写回地址
  uint32_t mask;
  // TBD
};

struct gvmref_insn_result_t {
  gvmref_insn_type insn_type;
  gvmref_xreg_result_t xreg_result;
  gvmref_vreg_result_t vreg_result;
};

struct gvmref_step_return_info_t {
  // 每次 step 时，返回 step 的结果
  // 包括指令类型、写回寄存器地址、写回寄存器数据、向量寄存器 mask 等
  int wg_done; // 该 workgroup 是否已完成
  uint64_t pc;
  uint32_t insn;
  gvmref_insn_result_t insn_result;
};



// 以下是 spike 初始化 API，可参考 spike driver
int gvmref_vt_dev_open();
int gvmref_vt_dev_close();
int gvmref_vt_buf_alloc(uint64_t size, uint64_t *vaddr, int BUF_TYPE, uint64_t taskID, uint64_t kernelID);
int gvmref_vt_buf_free(uint64_t size, uint64_t *vaddr, uint64_t taskID, uint64_t kernelID);
int gvmref_vt_one_buf_free(uint64_t size, uint64_t *vaddr, uint64_t taskID, uint64_t kernelID);
int gvmref_vt_copy_to_dev(uint64_t dev_vaddr,const void *src_addr, uint64_t size, uint64_t taskID, uint64_t kernelID);
int gvmref_vt_start(void* metaData, uint64_t taskID);
int gvmref_vt_upload_kernel_file(const char* filename, int taskID);

// 以下是 GVM 需要使用的 API
int gvmref_set_warp_xreg(uint32_t software_wg_id, uint32_t software_warp_id, uint32_t xreg_usage, gvmref_warp_xreg_t xreg_data);
// 将一个指定 warp 的标量寄存器堆的前 xreg_usage 个值依次设置为 xreg_data 中的值
// 这个函数是为了解决以下问题：
// REF 对每一个 warp 的标量寄存器都是零初始化的；
// 但 RTL DUT 的 CTA 调度器在向 SM 分派新 warp 时，只会在寄存器堆中分配一块空间，但不会零初始化，寄存器中仍是之前残留的旧数据
// 导致大量寄存器不匹配。
// 因此这里在 DUT 的 CTA 调度器向 SM 分派新 warp 时，将 DUT 的该 warp 的寄存器数据同步到 REF 的对应 warp
uint32_t gvmref_get_next_pc(uint32_t software_wg_id, uint32_t software_warp_id);
// 返回指定 warp 的即将执行的指令 PC
void gvmref_step(uint32_t software_wg_id, uint32_t software_warp_id, gvmref_step_return_info_t* ret);
// 将指定 warp 步进一条指令
// 并且，如果该指令是 1.会写回标量寄存器的指令 或者 2.会写回向量寄存器的指令
// 则将该指令的执行结果返回
void gvmref_get_xreg(gvmref_xreg_t* ret, uint32_t wg_id, uint32_t warp_id);
// 获取 spike 的所有 warp 的标量寄存器堆

}

struct gvmref_meta_data{  // 这个metadata是供驱动使用的，而不是给硬件的
  uint64_t kernel_id;
  uint64_t kernel_size[3];///> 每个kernel的workgroup三维数目
  uint64_t wf_size; ///> 每个warp的thread数目
  uint64_t wg_size; ///> 每个workgroup的warp数目
  uint64_t metaDataBaseAddr;///> CSR_KNL的值，
  uint64_t ldsSize;///> 每个workgroup使用的local memory的大小
  uint64_t pdsSize;///> 每个thread用到的private memory大小
  uint64_t sgprUsage;///> 每个workgroup使用的标量寄存器数目
  uint64_t vgprUsage;///> 每个thread使用的向量寄存器数目
  uint64_t pdsBaseAddr;///> private memory的基址，要转成每个workgroup的基地址， wf_size*wg_size*pdsSize
  gvmref_meta_data(uint64_t arg0,uint64_t arg1[],uint64_t arg2,uint64_t arg3,uint64_t arg4,uint64_t arg5,\
    uint64_t arg6,uint64_t arg7,uint64_t arg8,uint64_t arg9) \
    :kernel_id(arg0),wf_size(arg2),wg_size(arg3),metaDataBaseAddr(arg4),ldsSize(arg5),pdsSize(arg6),\
    sgprUsage(arg7),vgprUsage(arg8),pdsBaseAddr(arg9)
    {
      kernel_size[0]=arg1[0];kernel_size[1]=arg1[1];kernel_size[2]=arg1[2];
    }
};