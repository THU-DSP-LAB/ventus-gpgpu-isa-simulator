#include "ventus.h"
#include <iostream>
using namespace std;

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
int main(){
    // printf("BUFFER SIZE: %d", PRINT_BUFFER_SIZE);
    // printf("BUFFER ADDR: %d", PRINT_BUFFER_ADDR);
    uint64_t num_warp=4;
    uint64_t num_thread=8;
    uint64_t num_workgroups[3]={1,1,1};
    uint64_t num_workgroup=num_workgroups[0]*num_workgroups[1]*num_workgroups[2];
    uint64_t num_processor=num_warp*num_workgroup;
    uint64_t ldssize=0x1000;
    uint64_t pdssize=0x1000;
    uint64_t pdsbase=0x8a000000;
    uint64_t start_pc=0x80000000;
    uint64_t knlbase=0x90000000;
    meta_data meta(0,num_workgroups,num_thread,num_warp,knlbase,ldssize,pdssize,32,32,pdsbase);
    char filename[]="vecadd.riscv";//set elf filename

    uint64_t size_0=0x10000000;
    float data_0[]={1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};//arg_1
    uint64_t vaddr_0;
    uint64_t size_1=0x10000000;
    float data_1[]={1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};//arg_2
    uint64_t vaddr_1,vaddr_2,vaddr_3,vaddr_4;

    uint64_t vaddr_print;
    uint64_t size_print=0x10000000;

    vt_device_h p=nullptr;
    vt_dev_open(&p);

    vt_buf_alloc(p,size_0,&vaddr_0,0,0,0);//allocate for program
    vt_buf_alloc(p,pdssize*num_thread*num_warp*num_workgroup,&pdsbase,0,0,0);//allocate for privatemem
    vt_buf_alloc(p,16*4,&vaddr_1,0,0,0);//allocate arg1 buffer
    vt_buf_alloc(p,16*4,&vaddr_2,0,0,0);//allocate arg2 buffer
    vt_buf_alloc(p,16*4,&vaddr_3,0,0,0);//allocate metadata buffer
    vt_buf_alloc(p,2*4,&vaddr_4,0,0,0);//allocate buffer base

    vt_buf_alloc(p,size_print,&vaddr_print,0,0,0);//allocate buffer base

    vt_copy_to_dev(p,vaddr_1,data_0,16*4,0,0);
    vt_copy_to_dev(p,vaddr_2,data_1,16*4,0,0);
    meta.metaDataBaseAddr=vaddr_3;
    meta.pdsBaseAddr=pdsbase;
    uint32_t data_2[14];//metadata
    for(int i=0;i<14;i++) data_2[i]=1;
    data_2[0]=0x80000098;
    data_2[1]=(uint32_t)vaddr_4;
    data_2[2]=meta.kernel_size[0];
    data_2[6]=num_thread;
    data_2[9]=0;data_2[10]=0;data_2[11]=0;
    data_2[12]=(uint32_t)vaddr_print;data_2[13]=(uint32_t)size_print;

    vt_copy_to_dev(p,vaddr_3,data_2,14*4,0,0);
    uint32_t data_3[2]={(uint32_t)vaddr_1,(uint32_t)vaddr_2};//buffer base
    vt_copy_to_dev(p,vaddr_4,data_3,2*4,0,0);

    vt_upload_kernel_file(p,filename,0);
    vt_start(p,&meta,0);
    cout << "finish running" << endl;
    vt_copy_from_dev(p,vaddr_2,data_1,16*4,0,0);
    vt_copy_from_dev(p,vaddr_1,data_0,16*4,0,0);

    for(int i=0;i<16;i++)
        cout << data_0[i] << " " << data_1[i] << endl;
    uint32_t *print_data=new uint32_t[64];
    vt_copy_from_dev(p,vaddr_print,print_data,64*4,0,0);
    for(int i=0;i<64;i++)
        cout <<  print_data[i] << endl;
    vt_buf_free(p,0,nullptr,0,0);
    delete[] print_data;
    return 0;
}
