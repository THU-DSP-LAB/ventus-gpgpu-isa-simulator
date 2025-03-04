VENTUS_INSTALL_PREFIX ?= /root/ventus/llvm-project/install

SOURCE_PATH ?= ./

KERNEL_FUNC = vectorAdd

all: vecadd.riscv

vecadd_llvm.ll: ${SOURCE_PATH}/vecadd.cl
	${VENTUS_INSTALL_PREFIX}/bin/clang -S -cl-std=CL2.0 -target riscv32 -mcpu=ventus-gpgpu $^ -emit-llvm -o $@
	
vecadd_llvm.o: vecadd_llvm.ll
	${VENTUS_INSTALL_PREFIX}/bin/llc -mtriple=riscv32 -mcpu=ventus-gpgpu --filetype=obj $^ -o $@

vecadd.riscv: vecadd_llvm.o
	${VENTUS_INSTALL_PREFIX}/bin/ld.lld -o $@ \
		-T ${VENTUS_INSTALL_PREFIX}/lib/ldscripts/ventus/elf32lriscv.ld $^ \
		${VENTUS_INSTALL_PREFIX}/lib/crt0.o ${VENTUS_INSTALL_PREFIX}/lib/riscv32clc.o \
		-L${VENTUS_INSTALL_PREFIX}/lib -lworkitem --gc-sections --init ${KERNEL_FUNC}

clean:
	rm -f vecadd_llvm.*
	rm vecadd.riscv
