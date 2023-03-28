#ifndef __VT_DRIVER_H__
#define __VT_DRIVER_H__

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

//#ifdef __cplusplus
//extern "C" {
//#endif

typedef void* vt_device_h; ///< 类型定义，指向vt_device类的指针

typedef void* vt_buffer_h; ///< 类型定义，指向vt_buffer类的指针

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
    int alloc_local_mem(uint64_t size, uint64_t *dev_maddr);
    int free_local_mem();
    int copy_to_dev(uint64_t dev_maddr, uint64_t size, void* data);
    int copy_from_dev(uint64_t dev_maddr, uint64_t size,void* data);
    int run(meta_data knl_data,uint64_t knl_start_pc);
  private:
    sim_t* sim;
    std::vector<mem_cfg_t> buffer; //可以在分配时让buffer地址对齐4k
    std::vector<std::pair<reg_t, mem_t*>> buffer_data;
};

#define MAX_TIMEOUT               (60*60*1000)   // 1hr 


/// @brief 【已实现】打开并连接一个GPGPU设备
/// @param hdevice 指向设备的指针
/// @return 若无错误则返回0，否则返回-1
int vt_dev_open(vt_device_h hdevice);

/// @brief 【已实现】当所有操作完成时，关闭设备
/// @param hdevice 指向设备的指针
/// @return 若无错误则返回0，否则返回-1
int vt_dev_close(vt_device_h hdevice);

/// return device configurations
// int vt_dev_caps(vt_device_h* hdevice, uint32_t caps_id, uint64_t *value);


/// @brief 【已实现】为设备分配buffer
/// @param hdevice 指向设备的指针
/// @param size buffer大小
/// @param vaddr 保存设备端内存地址的指针，函数在分配好内存空间后，修改该指针的值为分配的内存的地址
/// @param taskID context ID
/// @param kernelID kernel ID
/// @return 若无错误则返回0，否则返回-1
int vt_buf_alloc(vt_device_h hdevice, uint64_t size, uint64_t *vaddr, int BUF_TYPE, uint64_t taskID, uint64_t kernelID);

/// @brief 【已实现】释放buffer
/// @param hbuffer 指向设备的指针
/// @param size buffer大小
/// @param vaddr 要释放的设备端内存的起始地址
/// @param taskID context ID
/// @param kernelID kernel IDD
/// @return 若无错误则返回0，否则返回-1
int vt_buf_free(vt_device_h hdevice, uint64_t size, uint64_t *vaddr, uint64_t taskID, uint64_t kernelID);


/// @brief 【已实现】以任务为单位，在GPGPU设备上分配虚拟内存空间（创建根页表）
/// @param hdevice 指向设备的指针
/// @param taskID context ID，需要从0开始迭加分配
/// @return 若无错误则返回0，否则返回-1
int vt_root_mem_alloc(vt_device_h hdevice, int taskID);


/// @brief 【已实现】释放任务对应的虚拟内存空间和已分配的物理空间（删除根页表）
/// @param hdevice 指向设备的指针
/// @param taskID context ID
/// @return 若无错误则返回0，否则返回-1
int vt_root_mem_free(vt_device_h hdevice, int taskID);

int vt_create_kernel(vt_device_h hdevice, int taskID, int kernelID);


/// @brief 【已实现】将数据从buffer复制到设备内存
/// @param hbuffer 指向类vt_buffer的指针
/// @param dev_vaddr 设备端保存数据的起始虚拟地址
/// @param src_addr 源数据的起始地址
/// @param size 数据大小
/// @param taskID 任务ID
/// @param kernelID kernel ID
/// @return 若无错误则返回0，否则返回-1
int vt_copy_to_dev(vt_device_h hdevice, uint64_t dev_vaddr, void *src_addr, uint64_t size, uint64_t taskID, uint64_t kernelID);


/// @brief 【已实现】将数据从设备内存复制到buffer
/// @param hbuffer 指向类vt_buffer的指针
/// @param dev_vaddr 设备端保存数据的起始虚拟地址
/// @param dst_addr 数据要保存的地址
/// @param size 数据大小
/// @param taskID 任务ID
/// @return 若无错误则返回0，否则返回-1
int vt_copy_from_dev(vt_device_h hdevice, uint64_t dev_vaddr, void *dst_addr, uint64_t size, uint64_t taskID, uint64_t kernelID);


/// @brief 【已实现】设备开始执行任务
/// @param hdevice 指向设备的指针
/// @param kernelID kernel ID
/// @param num_blocks 该任务需要分配的block数量
/// @param input_port GPGPU硬件输入信号
/// @return 若无错误则返回0，否则返回-1
int vt_start(vt_device_h hdevice, void* metaData, uint64_t taskID);

/// @brief 【已实现】等待设备执行完成
/// @param hdevice 指向设备的指针
/// @param timeout 等待时间，单位为毫秒
/// @return 若无错误则返回0，否则返回-1
int vt_ready_wait(vt_device_h hdevice, uint64_t timeout);

/// @brief 【已实现】执行所有的kernel，并返回kernel ID
/// @param hdevice 指向设备的指针
/// @param finished_list 指向已完成kernel的list
/// @return 若无错误则返回0，否则返回-1
int vt_finish_all_kernel(vt_device_h hdevice, std::queue<int> *finished_list);

////////////////////////////// UTILITY FUNCIONS ///////////////////////////////

/// @brief 【已实现】上传kernel文件的一部分到设备，由vt_upload_kernel_file调用
/// @param device 指向设备的指针
/// @param content 数据内容
/// @param size 数据大小
/// @param taskID 对应的任务ID
/// @return 若无错误则返回0，否则返回-1
int vt_upload_kernel_bytes(vt_device_h device, const void* content, uint64_t size, int taskID);


/// @brief 【已实现】上传kernel文件到设备
/// @param device 指向设备的指针
/// @param filename kernel文件的名称
/// @param kernelID 对应的kernel ID
/// @return 若无错误则返回0，否则返回-1
int vt_upload_kernel_file(vt_device_h device, const char* filename, int kernelID);

/// dump performance counters

/// @brief 【未实现】性能计数
/// @param device 指向设备的指针
/// @param stream 
/// @return 
int vt_dump_perf(vt_device_h device, FILE* stream);


//#ifdef __cplusplus
//}
//#endif

#endif // __VT_DRIVER_H__