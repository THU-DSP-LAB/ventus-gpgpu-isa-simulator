// See LICENSE for license details.

#include "insn_template.h"
#include "insn_macros.h"

reg_t rv32i_fadd_h(processor_t* p, insn_t insn, reg_t pc)
{
  #define xlen 32
  reg_t npc = sext_xlen(pc + insn_length( MATCH_FADD_H));
  #include "insns/fadd_h.h"
  trace_opcode(p,  MATCH_FADD_H, insn);
  #undef xlen
  return npc;
}

reg_t rv64i_fadd_h(processor_t* p, insn_t insn, reg_t pc)
{
  #define xlen 64
  reg_t npc = sext_xlen(pc + insn_length( MATCH_FADD_H));
  #include "insns/fadd_h.h"
  trace_opcode(p,  MATCH_FADD_H, insn);
  #undef xlen
  return npc;
}

#undef CHECK_REG
#define CHECK_REG(reg) require((reg) < 16)

reg_t rv32e_fadd_h(processor_t* p, insn_t insn, reg_t pc)
{
  #define xlen 32
  reg_t npc = sext_xlen(pc + insn_length( MATCH_FADD_H));
  #include "insns/fadd_h.h"
  trace_opcode(p,  MATCH_FADD_H, insn);
  #undef xlen
  return npc;
}

reg_t rv64e_fadd_h(processor_t* p, insn_t insn, reg_t pc)
{
  #define xlen 64
  reg_t npc = sext_xlen(pc + insn_length( MATCH_FADD_H));
  #include "insns/fadd_h.h"
  trace_opcode(p,  MATCH_FADD_H, insn);
  #undef xlen
  return npc;
}
