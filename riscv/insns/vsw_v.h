#include "ventus_rt.h"

VI_GPU_ST_INDEX(e32,true,({
    const reg_t baseAddr = index[i] + insn.v_s_simm11();
    const reg_t threadCount = P.get_csr(CSR_NUMW) * P.get_csr(CSR_NUMT);
    const reg_t tid = baseTid + vreg_inx;
    const reg_t fieldMajorAddr = P.get_csr(CSR_PDS) +
        (threadCount * (baseAddr & ~reg_t(3))) + (tid << 2);
    MMU.store_uint32(fieldMajorAddr, P.VU.elt<uint32_t>(2,vs2, vreg_inx));
}));


  
