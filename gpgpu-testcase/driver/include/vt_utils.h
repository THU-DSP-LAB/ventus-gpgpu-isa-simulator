
#pragma once
#include <cstdint>
#include <initializer_list>
#include <functional>


#define DEBUG

#ifdef DEBUG
#define PCOUT_ERROR std::cout<<"[ERROR]: "<<__FUNCTION__<<": "
#define PCOUT_INFO std::cout<<"[INFO]: "<<__FUNCTION__<<": "
#endif

#define INSTSIZE64
#ifdef INSTSIZE64
typedef uint64_t inst_len;
#else
typedef uint32_t inst_len;
#endif

typedef struct {
    inst_len host_req_wg_id;
    inst_len host_req_num_wf;
    inst_len host_req_wf_size;
    inst_len host_req_start_pc;
    inst_len host_req_vgpr_size_total;
    inst_len host_req_sgpr_size_total;
    inst_len host_req_lds_size_total;
    inst_len host_req_gds_size_total;
    inst_len host_req_vgpr_size_per_wf;
    inst_len host_req_sgpr_size_per_wf;
    inst_len host_req_gds_baseaddr;
    inst_len host_req_pds_baseaddr;
    inst_len host_req_csr_knl;
    inst_len host_req_kernel_size_3d_0;
    inst_len host_req_kernel_size_3d_1;
    inst_len host_req_kernel_size_3d_2;

} host_port_t;///< GPGPU和主机之间进行配置参数传递的接口

/**
 * 将size对齐
 * @param size 要对齐的大小
 * @param align_block 和该大小对齐
 * @return 对齐后的大小
 */
uint64_t aligned_size(uint64_t size, uint64_t align_block);
/**
 * 判断是否对齐
 * @param addr 要判断的大小
 * @param align_block 和该大小对齐
 * @return true if 已对齐 else false
 */
bool is_aligned(uint64_t addr, uint64_t align_block);

