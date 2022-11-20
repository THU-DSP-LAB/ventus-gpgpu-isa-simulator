#include <iostream>
#include <fstream>
#include <cstring>
#include "autospike.h"


int main(int argc, char* argv[]) {
    if(argc == 1){

        help();
        return 0;
    }
    std::string asm_in_path, data_in_path;
    std::string name;
    //** log test
//    name = "saxpy";
//    makelog(name, 4);
//    return 0;
    //**end log test
    int num_warp = 0, thread_per_warp = 0, width = 32;
    int i = 1;
    while(i + 1 < argc){
        if(strcmp(argv[i], "-s") == 0){
            asm_in_path = argv[i + 1];
        }
        else if(strcmp(argv[i], "-d") == 0){
            data_in_path = argv[i + 1];
        }
        else if(strcmp(argv[i], "-w") == 0){
            num_warp = std::stoi(std::string(argv[i + 1]));
        }
        else if(strcmp(argv[i], "-t") == 0){
            thread_per_warp = std::stoi(std::string(argv[i + 1]));
        }
        else if(strcmp(argv[i], "-x") == 0){
            width = std::stoi(std::string(argv[i + 1]));
        }
        else{
            std::cerr << "cannot resolve" << std::endl;
            help();
            return 0;
        }
        i = i + 2;
    }

    if(num_warp == 0){
        std::cerr << "wrong value for warp num" << std::endl;
        help();
        return 0;
    }
    if(thread_per_warp == 0){
        std::cerr << "wrong value for thread num per warp" << std::endl;
        help();
        return 0;
    }
    if(width != 32 && width != 64){
        std::cout << "wrong value for data width, set to default(32)" << std::endl;
        width = 32;
    }

    std::string asm_out_path;
    auto pos = asm_in_path.find_last_of('.');
    if(pos != std::string::npos)
        asm_out_path = asm_in_path.substr(0, pos) + ".s";
    else
        asm_out_path = asm_in_path + ".s";
    if(asm_in_path.empty()){
        std::cerr << "empty asm filepath" << std::endl;
        help(); return 0;
    }
    asm_wrapping(name, asm_in_path, asm_out_path, width);
    if(data_in_path.empty()){
        std::cout << "empty data filepath" << std::endl;
        return 0;
    }
    asm_data(data_in_path, asm_out_path);

    std::cout << "*** asm file generated: " + asm_out_path << " ***" << std::endl;

    makefile_gen(name, num_warp, thread_per_warp, width);

    std::cout << "*** makefile generated ***" << std::endl;

    execute(name);

    makelog(name, num_warp);

    return 0;
}

void help(){
    static std::string usage = "Usage:\n"
                               "  -s <asm_file>\n"
                               "  -d <data_file>\n"
                               "  -w <num_warp>\n"
                               "  -t <thread_per_warp>\n"
                               "  -x <width>\t\tdefault=32\n"
    ;
    std::cout << usage << std::endl;
}

void asm_wrapping(std::string &test, std::string &asm_in_path, std::string &asm_out_path, int vl){
    std::ofstream asm_out(asm_out_path);
    std::string equ_str = ".equ CSR_NUMW,\t0x801\n"
                          ".equ CSR_NUMT,\t0x802\n"
                          ".equ CSR_TID,\t0x800\n"
                          ".equ CSR_WID,\t0x805\n"
                          ".equ CSR_GDS,\t0x807\n"
                          ".equ CSR_LDS,\t0x806\n"
    ;
    asm_out << equ_str << std::endl;

    std::size_t pos;
    std::ifstream asm_in(asm_in_path);
    if(!asm_in.is_open()) return;
    std::string line;
    while(!asm_in.eof()){
        std::getline(asm_in, line);
        if(line.substr(0, 6) == ".globl"){
            asm_out << ".section .text" << std::endl;
            line.replace(6, 1, " main, main_end, ");
            break;
        }
        asm_out << line << std::endl;
    }
    asm_out << line << "\n" << std::endl;
    // if(asm_in.eof()) return;
    // std::getline(asm_in, line);
    // if(line[line.length()-1]!=':'){
    //     std::cerr << "expect tag of program" << std::endl;
    //     return;
    // }
    while(!asm_in.eof()){
        std::getline(asm_in, line);
        if(asm_in.eof())
            break;
        pos = line.find_first_of(':');
        if(pos != std::string::npos)
            if(pos > line.find_first_not_of(' ') && pos < line.find_first_of('#'))
                break;
        asm_out << line << std::endl;
    }
    if(asm_in.eof()) return;

    test = line.erase(0, line.find_first_not_of(' '));
    test = line.substr(0, line.find_first_of(':'));

    std::string main_str = "main:\n"
                           "    addi        sp,sp,-16\n"
                           "    sd          s0,8(sp)\n"
                           "    addi        s0,sp,16\n"
                           "    li          t4, {vl}\n"
                           "    vsetvli     t4, t4, e32, ta, ma\n"
                           "    li          t4, 0\n"
                           "    j           {test}\n"
                           "main_end:\n"
                           "    li          a5,0\n"
                           "    mv          a0,a5\n"
                           "    ld          s0,8(sp)\n"
                           "    addi        sp,sp,16\n"
                           "    ret\n"
    ;
    pos = main_str.find("{test}");
    if(pos != std::string::npos)
        main_str.replace(pos, 6, test);
    pos = main_str.find("{vl}");
    if(pos != std::string::npos)
        main_str.replace(pos, 4, std::to_string(vl));
    asm_out << main_str << test << ":" << std::endl;

    while(!asm_in.eof()){
        std::getline(asm_in, line);
        asm_out << line << std::endl;
    }
    asm_out << "    j           main_end\n" << std::endl;
    asm_out.close();

    std::string sed = R"xxx(sed -E -i 's/^\s*csr\w+\s*(\w*),\s?CSR_GDS\s*$/    la          \1, global_data/g' )xxx";
    sed = sed + asm_out_path;
    system(sed.c_str());
}

void asm_data(std::string &data_in_path, std::string &asm_out_path){
    std::ifstream data_in(data_in_path);
    if(!data_in.is_open()) return;
    if(!std::ifstream(asm_out_path).good()) return;
    std::ofstream asm_out(asm_out_path, std::ios::app);
    asm_out << ".section .data\nglobal_data:" << std::endl;
    if(data_in.eof()) return;
    std::string line;
    while(!data_in.eof()){
        std::getline(data_in, line);
        if(!line.empty())
            asm_out << "    .word 0x" << line << std::endl;
    }
}

void makefile_gen(std::string &test, int warp, int thread, int vl){
    static std::string makefile =
            R"xxx(CC = riscv64-unknown-elf-gcc
CFLAG_C = -fno-common -fno-builtin-printf -specs=htif_nano.specs -Wl,--defsym=__main=main -c
CFLAG_LD = -static -specs=htif_nano.specs -Wl,--defsym=__main=main

TEST ?
SRC = $(TEST).s
OBJ = build/$(TEST).o
TARGET = build/$(TEST).riscv

P ?
VARCH ?
GPGPUARCH ?
DEBUG ?= -d
ISA ?= rv64gv
SPIKE_ARGS = $(DEBUG) -l --log-commits -p$(P) --isa $(ISA) --varch $(VARCH) --gpgpuarch $(GPGPUARCH)

all: $(TARGET)

spike-sim: $(TARGET)
	spike $(SPIKE_ARGS) $(TARGET) > log/$(TEST).log 2>&1

$(OBJ): $(SRC)
	$(CC) $(CFLAG_C) $(SRC) -o $(OBJ)

$(TARGET): $(OBJ)
	$(CC) $(CFLAG_LD) $(OBJ) -o $(TARGET)

clean:
	rm build/*
	rm log/*)xxx";
    int vlen = thread * vl;
    std::string VARCH = "vlen:" + std::to_string(vlen) + ",elen:" + std::to_string(vl);
    std::string P = std::to_string(warp);
    std::string GPGPUARCH = "numw:" + P + ",numt:" + std::to_string(thread);
    std::string tmp;
    std::ofstream makefile_output("./Makefile");
    makefile_output << makefile << std::endl;
    makefile_output.close();
    tmp = R"xxx(sed -E -i "/^TEST \?/ s/$/= )xxx" + test + R"xxx(/g" ./Makefile)xxx";
    system(tmp.c_str());
    tmp = R"xxx(sed -E -i "/^P \?/ s/$/= )xxx" + P + R"xxx(/g" ./Makefile)xxx";
    system(tmp.c_str());
    tmp = R"xxx(sed -E -i "/^VARCH \?/ s/$/= )xxx" + VARCH + R"xxx(/g" ./Makefile)xxx";
    system(tmp.c_str());
    tmp = R"xxx(sed -E -i "/^GPGPUARCH \?/ s/$/= )xxx" + GPGPUARCH + R"xxx(/g" ./Makefile)xxx";
    system(tmp.c_str());
}

void execute(std::string &test){
    system("mkdir -p build log");
    system("make spike-sim");
}

void makelog(std::string &test, int cores){
    // sed -E '/core\s*1:.*$/!d; />>>>\s*saxpy\s*$/,$!d; />>>>\s*exit\s*$/q;' saxpy.log
    std::string stri, extractlog, sublog;

    static std::string awk = R"y(awk -nM ' BEGIN { flagX = 0; X = 0; }
/^[\s\S]*core[^x]+0x[0-9a-f]{16}/ {
    flagC = 0;
    for(C = 1; C < NF; C++){
        if(flagC == 0 && $C ~ /0x[0-9a-f]{16}/){
            if( flagX == 0 ) {
                X = $C;
                flagX = 1;
            }
            Y = $C - X;
            flagC = 1;
            $C = sprintf("0x%016x", Y);
        }
    }
}
{
    print;
} ' )y";

    for(int i = 0; i < cores; i++){
        stri = std::to_string(i);
        sublog = "log/" + test + "." + stri + ".log";
        extractlog = R"x(sed -E '/core\s*)x"+ stri + R"x(\s*:.*$/!d; />>>>\s*)x" + test +
                     R"x(\s*$/,$!d; />>>>\s*main_end\s*$/q;' log/)x" + test + ".log > " + sublog;
        system(extractlog.c_str());
        extractlog = R"y(sed -E -i 's/^(: )?core\s*\w+\s*:\s*0x[0-9a-f]{16} \(\w+\)([^$]*?)$/\1                                         \2/g; s/^(: )//g' )y" + sublog;
        system(extractlog.c_str());
        extractlog = awk + sublog + " > log/tmp && mv log/tmp " + sublog;
        system(extractlog.c_str());
        std::cout << "*** " << sublog << " generated ***"<< std::endl;
    }
}