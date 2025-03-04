#include <cmath>
#include <cstddef>
#include <iostream>
#include <ctime>
#include "ventus.h"
#include "utils.h"

float* vec_generate_random(size_t length) {
    float* random_vec = new float[length];
    for(size_t i=0; i<length; i++)
        random_vec[i] = rand()%100;
    return random_vec;
}

float vec_diff_sum(float* vec_1, float* vec_2, size_t length) {
    float diff_sum = 0;
    for(size_t i=0; i<length; i++)
        diff_sum += std::fabs(vec_1[i]-vec_2[i]);
    return diff_sum;
}

void vec_print(float* vec, size_t length) {
    for(size_t i=0; i<length; i++)
        std::cout<<vec[i]<<' ';
    std::cout<<std::endl;
}

void mat_print(float* mat, size_t M, size_t N) {
    std::cout<<"[ ";
    for(size_t i=0; i<M; i++) {
        for(size_t j=0; j<N; j++) {
            std::cout<<mat[i*N+j]<<' ';
        }
        if (i==M-1)
            std::cout<<']';
        std::cout<<std::endl;
    }
}

// Still in progress
void launch_kernel(vt_device_h hdevice, uint32_t* arg_buffer, size_t arg_num, char* elf_name, uint64_t num_warp, uint64_t num_thread) {
    uint64_t num_workgroups[3]={1,1,1};
    uint64_t num_workgroup=num_workgroups[0]*num_workgroups[1]*num_workgroups[2];
    uint64_t num_processor=num_warp*num_workgroup;
    uint64_t ldssize=0x1000;
    uint64_t pdssize=0x1000;
    uint64_t pdsbase=0x8a000000;
    uint64_t start_pc=0x80000000;
    uint64_t knlbase=0x90000000;
    meta_data meta(0,num_workgroups,num_thread,num_warp,knlbase,ldssize,pdssize,32,32,pdsbase);

    uint64_t size_0=0x10000000;

    uint64_t vaddr_0;
    uint64_t size_1=0x10000000;

    uint64_t vaddr_3,vaddr_4;

    uint64_t vaddr_print;
    uint64_t size_print=0x10000000;

    vt_device_h p=hdevice;

    vt_buf_alloc(p,size_0,&vaddr_0,0,0,0);//allocate for program
    vt_buf_alloc(p,pdssize*num_thread*num_warp*num_workgroup,&pdsbase,0,0,0);//allocate for privatemem

    vt_buf_alloc(p,16*4,&vaddr_3,0,0,0);//allocate metadata buffer
    vt_buf_alloc(p,arg_num*sizeof(uint32_t),&vaddr_4,0,0,0);//allocate buffer base

    vt_buf_alloc(p,size_print,&vaddr_print,0,0,0);//allocate buffer base

    meta.metaDataBaseAddr=vaddr_3;
    meta.pdsBaseAddr=pdsbase;
    uint32_t data_2[14];//metadata
    for(int i=0;i<14;i++) data_2[i]=1;
    data_2[0]=KERNEL_ADDRESS;
    data_2[1]=(uint32_t)vaddr_4;
    data_2[2]=meta.kernel_size[0];
    data_2[6]=num_thread;
    data_2[9]=0;data_2[10]=0;data_2[11]=0;
    data_2[12]=(uint32_t)vaddr_print;data_2[13]=(uint32_t)size_print;

    vt_copy_to_dev(p,vaddr_3,data_2,14*sizeof(uint32_t),0,0);
    uint32_t* data_3=arg_buffer;//buffer base
    vt_copy_to_dev(p,vaddr_4,data_3,arg_num*sizeof(uint32_t),0,0);

    vt_upload_kernel_file(p,elf_name,0);
    vt_start(p,&meta,0);
}
