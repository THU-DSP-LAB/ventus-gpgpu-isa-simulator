// See LICENSE for license details.

#include "insn_template.h"
#include "insn_macros.h"

reg_t rv32i_vredminu_vs(processor_t* p, insn_t insn, reg_t pc)
{
  #define xlen 32
  reg_t npc = sext_xlen(pc + insn_length( MATCH_VREDMINU_VS));
  #include "insns/vredminu_vs.h"
  trace_opcode(p,  MATCH_VREDMINU_VS, insn);
  #undef xlen
  return npc;
}

reg_t rv64i_vredminu_vs(processor_t* p, insn_t insn, reg_t pc)
{
  #define xlen 64
  reg_t npc = sext_xlen(pc + insn_length( MATCH_VREDMINU_VS));
  #include "insns/vredminu_vs.h"
  trace_opcode(p,  MATCH_VREDMINU_VS, insn);
  #undef xlen
  return npc;
}

#undef CHECK_REG
#define CHECK_REG(reg) require((reg) < 16)

reg_t rv32e_vredminu_vs(processor_t* p, insn_t insn, reg_t pc)
{
  #define xlen 32
  reg_t npc = sext_xlen(pc + insn_length( MATCH_VREDMINU_VS));
  #include "insns/vredminu_vs.h"
  trace_opcode(p,  MATCH_VREDMINU_VS, insn);
  #undef xlen
  return npc;
}

reg_t rv64e_vredminu_vs(processor_t* p, insn_t insn, reg_t pc)
{
  #define xlen 64
  reg_t npc = sext_xlen(pc + insn_length( MATCH_VREDMINU_VS));
  #include "insns/vredminu_vs.h"
  trace_opcode(p,  MATCH_VREDMINU_VS, insn);
  #undef xlen
  return npc;
}
