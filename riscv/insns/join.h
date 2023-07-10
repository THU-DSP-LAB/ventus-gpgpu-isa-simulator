if(!(P.gpgpu_unit.simt_stack.stack_empty()))
    if( pc ==  P.get_csr(CSR_RPC)){
        SET_PC(P.gpgpu_unit.simt_stack.top().else_pc);
        P.gpgpu_unit.simt_stack.pop_join();}