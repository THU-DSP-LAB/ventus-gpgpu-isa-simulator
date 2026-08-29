/* The high address bit is reserved for the existing lane-major hit-record
 * body path. hit_kind is an ordinary untagged field-major PDS access. */
#include "ventus_rt.h"

VI_GPU_LD_INDEX(e32,true,({
    const reg_t rawBaseAddr = index[i] + insn.v_simm11();
    const bool taggedHitRecord =
        (rawBaseAddr & ventus_rt::abi_hit_record_addr_tag) != 0;
    const reg_t baseAddr = rawBaseAddr & ~ventus_rt::abi_hit_record_addr_tag;
    const reg_t threadCount = P.get_csr(CSR_NUMW) * P.get_csr(CSR_NUMT);
    const reg_t logicalWord = baseAddr >> 2;
    const reg_t candidateWord =
        ventus_rt::abi_candidate_hit_record_base_bytes / sizeof(uint32_t);
    const reg_t committedWord =
        ventus_rt::abi_committed_hit_record_base_bytes / sizeof(uint32_t);
    const reg_t bodyWords =
        ventus_rt::abi_pds_lane_major_hit_record_body_word_count;
    const reg_t bodyRecordCount =
        ventus_rt::abi_pds_lane_major_hit_record_body_record_count;
    const bool candidateBody = logicalWord >= candidateWord &&
        logicalWord < candidateWord + bodyWords;
    const bool committedBody = logicalWord >= committedWord &&
        logicalWord < committedWord + bodyWords;
    const bool body = candidateBody || committedBody;
    const reg_t tid = baseTid + vreg_inx;
    const reg_t bodyWord = candidateBody ? logicalWord - candidateWord
                                         : logicalWord - committedWord;
    const reg_t bodyAddr = P.get_csr(CSR_PDS) +
        ((ventus_rt::abi_pds_field_major_prefix_word_count * threadCount +
          tid * bodyWords * bodyRecordCount +
          (committedBody ? bodyWords : 0) + bodyWord) << 2);
    const reg_t fieldMajorAddr = P.get_csr(CSR_PDS) +
        (threadCount * (baseAddr & ~reg_t(3))) + (tid << 2);
    MMU.load_int32(taggedHitRecord && body ? bodyAddr : fieldMajorAddr);}
));
