#ifndef __VT_SPIKE_DEVICE_H__
#define __VT_SPIKE_DEVICE_H__

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <queue>


#include "cfg.h"
#include "sim.h"
#include "mmu.h"
#include "remote_bitbang.h"
#include "cachesim.h"
#include "extension.h"
#include <dlfcn.h>
#include <fesvr/option_parser.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <string>
#include <memory>
#include <fstream>
#include "../VERSION"

struct meta_data{  // 这个metadata是供驱动使用的，而不是给硬件的
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
    meta_data(uint64_t arg0,uint64_t arg1[],uint64_t arg2,uint64_t arg3,uint64_t arg4,uint64_t arg5,\
      uint64_t arg6,uint64_t arg7,uint64_t arg8,uint64_t arg9) \
      :kernel_id(arg0),wf_size(arg2),wg_size(arg3),metaDataBaseAddr(arg4),ldsSize(arg5),pdsSize(arg6),\
      sgprUsage(arg7),vgprUsage(arg8),pdsBaseAddr(arg9)
      {
        kernel_size[0]=arg1[0];kernel_size[1]=arg1[1];kernel_size[2]=arg1[2];
      }
};
/*struct meta_data{  // 这个metadata是供驱动使用的，而不是给硬件的
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
    uint64_t num_buffer; ///> buffer的数目，包括pc
    uint64_t* buffer_base;//各buffer的虚拟地址基址。第一块buffer是给硬件用的metadata
    uint64_t* buffer_size;//各buffer的size，以Bytes为单位 = new uint64_t[num_buffer]
    meta_data(uint64_t arg0,uint64_t arg1[],uint64_t arg2,uint64_t arg3,uint64_t arg4,uint64_t arg5,\
      uint64_t arg6,uint64_t arg7,uint64_t arg8,uint64_t arg9,uint64_t arg10) \
      :kernel_id(arg0),wf_size(arg2),wg_size(arg3),metaDataBaseAddr(arg4),ldsSize(arg5),pdsSize(arg6),\
      sgprUsage(arg7),vgprUsage(arg8),pdsBaseAddr(arg9),num_buffer(arg10)
      {
        kernel_size[0]=arg1[0];kernel_size[1]=arg1[1];kernel_size[2]=arg1[2];
        buffer_base=new uint64_t[arg10];
        buffer_size=new uint64_t[arg10];
      }
    ~meta_data(){
      delete []buffer_base;
      delete []buffer_size;
    }  
};*/


class spike_device{
  public:
    spike_device();
    ~spike_device();
    int alloc_const_mem(uint64_t size, uint64_t *dev_maddr);
    int alloc_local_mem(uint64_t size, uint64_t *dev_maddr);
    int free_local_mem();
    int copy_to_dev(uint64_t dev_maddr, uint64_t size,const void* data);
    int copy_from_dev(uint64_t dev_maddr, uint64_t size,void* data);
    int run(meta_data* knl_data,uint64_t knl_start_pc);
    int set_filename(const char* filename,const char* logname=nullptr);
  private:
    sim_t* sim;
    std::vector<mem_cfg_t> buffer; //可以在分配时让buffer地址对齐4k
    std::vector<std::pair<reg_t, mem_t*>> buffer_data;
    std::vector<mem_cfg_t> const_buffer;
    std::vector<std::pair<reg_t, mem_t*>> const_buffer_data;
    char* srcfilename;
    char* logfilename;
};

#endif // __VT_SPIKE_DEVICE_H__