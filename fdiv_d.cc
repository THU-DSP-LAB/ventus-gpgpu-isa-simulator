// See LICENSE for license details.

#include "insn_template.h"
#include "insn_macros.h"

reg_t rv32i_fdiv_d(processor_t* p, insn_t insn, reg_t pc)
{
  #define xlen 32
  reg_t npc = sext_xlen(pc + insn_length( MATCH_FDIV_D));
  #include "insns/fdiv_d.h"
  trace_opcode(p,  MATCH_FDIV_D, insn);
  #undef xlen
  return npc;
}

reg_t rv64i_fdiv_d(processor_t* p, insn_t insn, reg_t pc)
{
  #define xlen 64
  reg_t npc = sext_xlen(pc + insn_length( MATCH_FDIV_D));
  #include "insns/fdiv_d.h"
  trace_opcode(p,  MATCH_FDIV_D, insn);
  #undef xlen
  return npc;
}

#undef CHECK_REG
#define CHECK_REG(reg) require((reg) < 16)

reg_t rv32e_fdiv_d(processor_t* p, insn_t insn, reg_t pc)
{
  #define xlen 32
  reg_t npc = sext_xlen(pc + insn_length( MATCH_FDIV_D));
  #include "insns/fdiv_d.h"
  trace_opcode(p,  MATCH_FDIV_D, insn);
  #undef xlen
  return npc;
}

reg_t rv64e_fdiv_d(processor_t* p, insn_t insn, reg_t pc)
{
  #define xlen 64
  reg_t npc = sext_xlen(pc + insn_length( MATCH_FDIV_D));
  #include "insns/fdiv_d.h"
  trace_opcode(p,  MATCH_FDIV_D, insn);
  #undef xlen
  return npc;
}
