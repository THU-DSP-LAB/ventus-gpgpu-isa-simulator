VENTUS_INSTALL_PREFIX ?= ./

SOURCE_PATH ?= ./

KERNEL_FUNC ?= None
TARGET ?= ${KERNEL_FUNC}

all: ${TARGET}.riscv

${TARGET}_llvm.ll: ${SOURCE_PATH}/${TARGET}.cl
	${VENTUS_INSTALL_PREFIX}/bin/clang -S -cl-std=CL2.0 -target riscv32 -mcpu=ventus-gpgpu $^ -emit-llvm -o $@
	
${TARGET}_llvm.o: ${TARGET}_llvm.ll
	${VENTUS_INSTALL_PREFIX}/bin/llc -mtriple=riscv32 -mcpu=ventus-gpgpu --filetype=obj $^ -o $@

${TARGET}.riscv: ${TARGET}_llvm.o
	${VENTUS_INSTALL_PREFIX}/bin/ld.lld -o $@ \
		-T ${VENTUS_INSTALL_PREFIX}/lib/ldscripts/ventus/elf32lriscv.ld $^ \
		${VENTUS_INSTALL_PREFIX}/lib/crt0.o ${VENTUS_INSTALL_PREFIX}/lib/riscv32clc.o \
		-L${VENTUS_INSTALL_PREFIX}/lib -lworkitem --gc-sections --init ${KERNEL_FUNC}

clean:
	rm -f ${TARGET}_llvm.*
	rm ${TARGET}.riscv
