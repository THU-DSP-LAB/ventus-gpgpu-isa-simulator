p->get_sim()->modify_reach_end();
p->gpgpu_unit.w->set_barrier_2(p->get_csr(CSR_WID));
if(p->get_sim()->get_reach_end()){
      std::cout<<"all warps reach the endprg. now proc 0 will end the simulation."<<std::endl;
      p->get_sim()->append_reach_end();
      //return 0;
    }
