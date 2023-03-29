/**
 * @file ventus.cpp
 * @brief 设备和OpenCL程序的交互功能的实现
 * 
 * 1. `/include/ventus.h`中声明的函数
 * 2. `spike_device`类，表示spike设备
 */

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <future>
#include <list>
#include <chrono>
// driver/page_table
#include "ventus.h"
#include "spike_main.h"


/// open the device and connect to it
extern int vt_dev_open(vt_device_h* hdevice){
    if(hdevice == nullptr)
        return -1;
    PCOUT_INFO << "vt_dev_open : hello world from ventus.cpp" << std::endl;
    *hdevice = new spike_device();
    return 0;
}
/// Close the device when all the operations are done
extern int vt_dev_close(vt_device_h hdevice){
    if(hdevice == nullptr)
        return -1;
    auto* device = (spike_device*) hdevice;
    delete device;
    return 0;
}
extern int vt_dev_caps(vt_device_h* hdevice, host_port_t* input_sig){
    // if(hdevice == nullptr)
    //     return -1;
    // vt_device* device = (vt_device*) hdevice;
    // //set spike_device id to 1
    return 0;
}
extern int vt_buf_alloc(vt_device_h hdevice, uint64_t size, uint64_t *vaddr, int BUF_TYPE, uint64_t taskID, uint64_t kernelID) {
    if(size <= 0 || hdevice == nullptr)
        return -1;
    auto device = ((spike_device*) hdevice);
    return device->alloc_local_mem(size, vaddr);

}
extern int vt_buf_free(vt_buffer_h hdevice, uint64_t size, uint64_t *vaddr, uint64_t taskID, uint64_t kernelID) {
    if(hdevice == nullptr)
        return -1;
    auto device = ((spike_device*) hdevice);
    return device->free_local_mem();

}

/**
 * @brief  为设备分配内存，返回根页表的地址
 * @param  hdevice           
 * @param  size              
 * @param  dev_vaddr    申请物理地址时的虚拟地址         
 * @return int 
 */
extern int vt_root_mem_alloc(vt_device_h hdevice, int taskID) {
    return -1;
}

/**
 * 释放taskID（对应context）的根页表
 * @param hdevice
 * @param taskID
 * @return
 */
extern int vt_root_mem_free(vt_device_h hdevice, int taskID) {
    return -1;
}

//extern int vt_create_kernel(vt_device_h hdevice, int taskID, int kernelID) {
//    if(hdevice == nullptr)
//        return -1;
//    auto device = (vt_device*) hdevice;
//    return device->push_kernel(taskID, kernelID);
//}

extern int vt_copy_to_dev(vt_device_h hdevice, uint64_t dev_vaddr, void *src_addr, uint64_t size, uint64_t taskID, uint64_t kernelID) {
    if(size <= 0)
        return -1;
    auto device = (spike_device*) hdevice;
    return device->copy_to_dev(dev_vaddr, size, src_addr);
}

extern int vt_copy_from_dev(vt_device_h hdevice, uint64_t dev_vaddr, void *dst_addr, uint64_t size, uint64_t taskID, uint64_t kernelID) {
    if(size <= 0)
        return -1;
    auto device = (spike_device*) hdevice;
    return device->copy_from_dev(dev_vaddr, size, dst_addr);
}

extern int vt_start(vt_device_h hdevice, void* metaData, uint64_t taskID) {
    if(hdevice == nullptr)
        return -1;
    auto device = (spike_device *) hdevice;
    auto knl_data = (meta_data *) metaData;
    device->run(knl_data,0x80000000);
    return 0;
}
extern int vt_ready_wait(vt_device_h hdevice, uint64_t timeout) {
    return 0;
}

extern int vt_finish_all_kernel(vt_device_h hdevice, std::queue<int> *finished_kernel_list) {
    return 0;
}

extern int vt_upload_kernel_file(vt_device_h hdevice, const char* filename, int taskID) {
  /*std::ifstream ifs(filename);
  if (!ifs) {
    std::cout << "error: " << filename << " not found" << std::endl;
    return -1;
  }
  ifs.close();*/
  if(hdevice == nullptr)
        return -1;
    auto device = (spike_device *) hdevice;
    device->set_filename(filename);
  return 0;
}

/*
extern int vt_upload_kernel_bytes(vt_device_h device, const void* content, uint64_t size, int taskID) {
  int err = 0;

  if (NULL == content || 0 == size)
    return -1;

  uint32_t buffer_transfer_size = 65536; ///< 64 KB
  uint64_t kernel_base_addr = GLOBALMEM_BASE;
//   err = vt_dev_caps(device, VT_CAPS_KERNEL_BASE_ADDR, &kernel_base_addr);
//   if (err != 0)
//     return -1;

  // allocate device buffer
  vt_buffer_h buffer;
  err = vt_buf_alloc(device, buffer_transfer_size, &buffer);
  if (err != 0)
    return -1;

  // get buffer address
  auto buf_ptr = (uint8_t*)vt_host_ptr(buffer);

  //
  // upload content
  //

  uint64_t offset = 0;
  while (offset < size) {
    auto chunk_size = std::min<uint64_t>(buffer_transfer_size, size - offset);
    std::memcpy(buf_ptr, (uint8_t*)content + offset, chunk_size);

    */
/*printf("***  Upload Kernel to 0x%0x: data=", kernel_base_addr + offset);
    for (int i = 0, n = ((chunk_size+7)/8); i < n; ++i) {
      printf("%08x", ((uint64_t*)((uint8_t*)content + offset))[n-1-i]);
    }
    printf("\n");*//*



    err = vt_copy_to_dev(buffer, kernel_base_addr + offset, chunk_size, taskID);
    if (err != 0) {
      vt_buf_free(buffer);
      return err;
    }
    offset += chunk_size;
  }

  vt_buf_free(buffer);

  return 0;
}

extern int vt_upload_kernel_file(vt_device_h device, const char* filename, int taskID) {
  std::ifstream ifs(filename);
  if (!ifs) {
    std::cout << "error: " << filename << " not found" << std::endl;
    return -1;
  }

  // read file content
  ifs.seekg(0, ifs.end);
  auto size = ifs.tellg();
  auto content = new char [size];
  ifs.seekg(0, ifs.beg);
  ifs.read(content, size);

  // upload
  int err = vt_upload_kernel_bytes(device, content, size, taskID);

  // release buffer
  delete[] content;

  return err;
}
*/
