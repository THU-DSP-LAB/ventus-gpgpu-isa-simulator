std::cout << "barrier" << std::endl;
SET_BARRIER_1;
if(IS_ALL_TRUE){
    //set_pc(pc+4);
    SET_BARRIER_0;
}
else{
    set_pc(pc);
}
  
