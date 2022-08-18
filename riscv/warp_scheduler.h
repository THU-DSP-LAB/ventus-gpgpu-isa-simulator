#ifndef _RISCV_SIM_H
#define _RISCV_SIM_H

#include <vector>

class warp_scheduler_t {

    public:
        void parser_gpgpuarch_string(const char*);
        void init_warps();
        int warp_number;
        int thread_number;
    private:
        std::vector<bool> barriers;
};



#endif