#ifndef RISCV_VENTUS_CUSTOM_H
#define RISCV_VENTUS_CUSTOM_H

#include "processor.h"
#include "ventus_custom_arith.h"
#include "ventus_mma.h"
#include "ventus_rt.h"

void ventus_exec_packed(processor_t *p, insn_t insn, VentusPackedOp op,
                        VentusPackedType type);
void ventus_exec_vcvt(processor_t *p, insn_t insn, VentusVCvtOp op);
void ventus_exec_sfu(processor_t *p, insn_t insn, VentusSFUOp op,
                     VentusSFUMode mode);
void ventus_exec_mma(processor_t *p, insn_t insn);
void ventus_exec_rt_traverse(processor_t *p, insn_t insn);
void ventus_exec_rt_release(processor_t *p, insn_t insn);
void ventus_exec_rt_enqueue(processor_t *p, insn_t insn);
void ventus_exec_rt_local_load(processor_t *p, insn_t insn);
void ventus_exec_rt_local_store(processor_t *p, insn_t insn);

#endif
