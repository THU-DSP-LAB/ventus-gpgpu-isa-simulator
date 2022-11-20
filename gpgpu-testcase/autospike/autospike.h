//
// Created by lxd21 on 22-11-1.
//

#ifndef AUTOSPIKE_AUTOSPIKE_H
#define AUTOSPIKE_AUTOSPIKE_H

#endif //AUTOSPIKE_AUTOSPIKE_H

#include <string>

void asm_wrapping(std::string &test, std::string &asm_in_path, std::string &asm_out_path, int vl = 32);

void asm_data(std::string &data_in_path, std::string &asm_out_path);

void help();

void makefile_gen(std::string &test, int warp, int thread, int vl);

void execute(std::string &test);

void makelog(std::string &test, int cores);