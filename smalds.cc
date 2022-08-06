// See LICENSE for license details.

#include "insn_template.h"
#include "insn_macros.h"

reg_t rv32i_smalds(processor_t* p, insn_t insn, reg_t pc)
{
  #define xlen 32
  reg_t npc = sext_xlen(pc + insn_length( MATCH_SMALDS));
  #include "insns/smalds.h"
  trace_opcode(p,  MATCH_SMALDS, insn);
  #undef xlen
  return npc;
}

reg_t rv64i_smalds(processor_t* p, insn_t insn, reg_t pc)
{
  #define xlen 64
  reg_t npc = sext_xlen(pc + insn_length( MATCH_SMALDS));
  #include "insns/smalds.h"
  trace_opcode(p,  MATCH_SMALDS, insn);
  #undef xlen
  return npc;
}

#undef CHECK_REG
#define CHECK_REG(reg) require((reg) < 16)

reg_t rv32e_smalds(processor_t* p, insn_t insn, reg_t pc)
{
  #define xlen 32
  reg_t npc = sext_xlen(pc + insn_length( MATCH_SMALDS));
  #include "insns/smalds.h"
  trace_opcode(p,  MATCH_SMALDS, insn);
  #undef xlen
  return npc;
}

reg_t rv64e_smalds(processor_t* p, insn_t insn, reg_t pc)
{
  #define xlen 64
  reg_t npc = sext_xlen(pc + insn_length( MATCH_SMALDS));
  #include "insns/smalds.h"
  trace_opcode(p,  MATCH_SMALDS, insn);
  #undef xlen
  return npc;
}
