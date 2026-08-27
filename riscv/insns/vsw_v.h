/* See vlw_v.h: tagged hit-record accesses use lane-major PDS storage. */
#include "ventus_rt.h"

VI_GPU_ST_INDEX(e32,true,(
    {
    const reg_t rawBaseAddr = index[i] + insn.v_s_simm11();
    const bool taggedHitRecord =
        (rawBaseAddr & ventus_rt::abi_hit_record_addr_tag) != 0;
    const reg_t baseAddr = rawBaseAddr & ~ventus_rt::abi_hit_record_addr_tag;
    const reg_t threadCount = P.get_csr(CSR_NUMW) * P.get_csr(CSR_NUMT);
    const reg_t logicalWord = baseAddr >> 2;
    const reg_t candidateWord =
        ventus_rt::abi_candidate_hit_record_base_bytes / sizeof(uint32_t);
    const reg_t committedWord =
        ventus_rt::abi_committed_hit_record_base_bytes / sizeof(uint32_t);
    const reg_t recordWords = ventus_rt::abi_pds_lane_major_hit_record_word_count;
    const bool candidate = logicalWord >= candidateWord &&
        logicalWord < candidateWord + recordWords;
    const bool committed = logicalWord >= committedWord &&
        logicalWord < committedWord + recordWords;
    const bool laneMajor = taggedHitRecord && (candidate || committed);
    const reg_t recordBase = candidate
        ? ventus_rt::abi_pds_field_major_prefix_word_count
        : ventus_rt::abi_pds_field_major_prefix_word_count + recordWords;
    const reg_t recordWord = candidate ? logicalWord - candidateWord
                                       : logicalWord - committedWord;
    const reg_t baseBias = laneMajor
        ? P.get_csr(CSR_PDS) + ((recordBase * threadCount) << 2)
        : P.get_csr(CSR_PDS) + (threadCount * (baseAddr & ~reg_t(3)));
    MMU.store_uint32(baseBias + (laneMajor
        ? ((baseTid + vreg_inx) * recordWords + recordWord) << 2
        : (baseTid + vreg_inx) << 2),
        P.VU.elt<uint32_t>(2,vs2, vreg_inx));
    }
    ));


  
