#ifndef RISCV_VENTUS_CUSTOM_H
#define RISCV_VENTUS_CUSTOM_H

#include "processor.h"
#include "ventus_custom_arith.h"

void ventus_exec_packed(processor_t *p, insn_t insn, VentusPackedOp op,
                        VentusPackedType type);
void ventus_exec_vcvt(processor_t *p, insn_t insn, VentusVCvtOp op);
void ventus_exec_sfu(processor_t *p, insn_t insn, VentusSFUOp op,
                     VentusSFUMode mode);

#endif
