#include "ventus.h"
#include "utils.h"

#include <cstddef>
#include <cstring>
#include <iostream>
#include <gtest/gtest.h>

using namespace std;

void GEMM_cpu(
        float* mat_1, float* mat_2, float* mat_res, 
        const int M, const int N, const int K
    ) {
    for(int i=0;i<M;i++) {
        for(int j=0;j<N;j++) {
            float sum = 0.0f;
            for(int k=0;k<K;k++)
                sum += mat_1[i*K+k]*mat_2[k*N+j];
            mat_res[i*N+j] = sum;
        }
    }
}

void GEMM_gpu(        
        float* mat_1, float* mat_2, float* mat_res, 
        int M, int N, int K
    ) {
    uint64_t vaddr_1, vaddr_2, vaddr_res, vaddr_M, vaddr_N, vaddr_K;

    vt_device_h p=nullptr;
    vt_dev_open(&p);

    vt_buf_alloc(p,(M*K)*sizeof(float),&vaddr_1,0,0,0);//allocate arg1 buffer
    vt_buf_alloc(p,(K*N)*sizeof(float),&vaddr_2,0,0,0);//allocate arg2 buffer
    vt_buf_alloc(p,(M*N)*sizeof(float),&vaddr_res,0,0,0);//allocate arg3 buffer

    vt_buf_alloc(p,sizeof(int),&vaddr_M,0,0,0);//allocate arg4 buffer
    vt_buf_alloc(p,sizeof(int),&vaddr_N,0,0,0);//allocate arg5 buffer
    vt_buf_alloc(p,sizeof(int),&vaddr_K,0,0,0);//allocate arg6 buffer

    vt_copy_to_dev(p,vaddr_1,mat_1,(M*K)*sizeof(float),0,0);
    vt_copy_to_dev(p,vaddr_2,mat_2,(K*N)*sizeof(float),0,0);

    vt_copy_to_dev(p,vaddr_M,&M,sizeof(int),0,0);
    vt_copy_to_dev(p,vaddr_N,&N,sizeof(int),0,0);
    vt_copy_to_dev(p,vaddr_K,&K,sizeof(int),0,0);

    uint32_t data_3[6]={
        (uint32_t)vaddr_1,
        (uint32_t)vaddr_2,
        (uint32_t)vaddr_res,
        (uint32_t)vaddr_M,
        (uint32_t)vaddr_N,
        (uint32_t)vaddr_K
    };//buffer base

    char elf_name[] = "GEMM.riscv";
    launch_kernel(p, data_3, 6, elf_name, 1, 32);
    cout << "finish running" << endl;

    vt_copy_from_dev(p,vaddr_res,mat_res,(M*N)*sizeof(float),0,0);

    vt_buf_free(p,0,nullptr,0,0);
}

TEST(vecadd, 0) {
    srand(time(0));

    int M = 4;
    int N = 7;
    int K = 9;

    const float tol = 1E-4;

    float* mat_a = vec_generate_random(M*K);
    float* mat_b = vec_generate_random(K*N);

    float* mat_res_cpu = new float[M*N]; 
    float* mat_res_gpu = new float[M*N];

    mat_print(mat_a, M, K);
    mat_print(mat_b, K, N);

    GEMM_cpu(mat_a, mat_b, mat_res_cpu, M, N, K);
    GEMM_gpu(mat_a, mat_b, mat_res_gpu, M, N, K);

    mat_print(mat_res_cpu, M, N);
    mat_print(mat_res_gpu, M, N);

    std::cout<<"Difference sum between two vector: "<<vec_diff_sum(mat_res_cpu, mat_res_gpu, M*N)<<std::endl;

    EXPECT_TRUE(vec_diff_sum(mat_res_cpu, mat_res_gpu, M*N)<=tol);

    delete[] mat_a;
    delete[] mat_b;
    delete[] mat_res_cpu;
    delete[] mat_res_gpu;
}
