#include "ventus.h"
#include "utils.h"

#include <cstddef>
#include <cstring>
#include <iostream>
#include <gtest/gtest.h>

using namespace std;

void vecadd_cpu(float* vec_1, float* vec_2, size_t size) {
    for(size_t i=0 ; i<size ; i++)
        vec_1[i] += vec_2[i];
}

void vecadd_gpu(float* vec_1, float* vec_2, size_t size) {
    uint64_t vaddr_1,vaddr_2;

    vt_device_h p=nullptr;
    vt_dev_open(&p);

    vt_buf_alloc(p,size*sizeof(float),&vaddr_1,0,0,0);//allocate arg1 buffer
    vt_buf_alloc(p,size*sizeof(float),&vaddr_2,0,0,0);//allocate arg2 buffer

    vt_copy_to_dev(p,vaddr_1,vec_1,size*sizeof(float),0,0);
    vt_copy_to_dev(p,vaddr_2,vec_2,size*sizeof(float),0,0);

    uint32_t data_3[2]={(uint32_t)vaddr_1,(uint32_t)vaddr_2};//buffer base

    char elf_name[] = "vecadd.riscv";
    launch_kernel(p, data_3, 2, elf_name, 4, 8);
    cout << "finish running" << endl;

    vt_copy_from_dev(p,vaddr_1,vec_1,size*sizeof(float),0,0);

    vt_buf_free(p,0,nullptr,0,0);
}

TEST(vecadd, 0) {
    srand(time(0));

    const size_t vec_size = 8;
    const float tol = 1E-4;

    float* vec_a = vec_generate_random(vec_size);
    float* vec_b = vec_generate_random(vec_size);

    float* vec_a_cpu = new float[vec_size]; 
    float* vec_a_gpu = new float[vec_size];

    memcpy(vec_a_cpu, vec_a, vec_size);
    memcpy(vec_a_gpu, vec_a, vec_size);

    vec_print(vec_a, vec_size);
    vec_print(vec_b, vec_size);

    vecadd_cpu(vec_a_cpu, vec_b, vec_size);
    vecadd_gpu(vec_a_gpu, vec_b, vec_size);

    std::cout<<"Difference sum between two vector: "<<vec_diff_sum(vec_a_cpu, vec_a_gpu, vec_size)<<std::endl;

    vec_print(vec_a_cpu, vec_size);
    vec_print(vec_a_gpu, vec_size);

    EXPECT_TRUE(vec_diff_sum(vec_a_cpu, vec_a_gpu, vec_size)<=tol);

    delete[] vec_a;
    delete[] vec_b;
    delete[] vec_a_cpu;
    delete[] vec_a_gpu;
}
