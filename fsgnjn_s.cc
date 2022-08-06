// See LICENSE for license details.

#include "insn_template.h"
#include "insn_macros.h"

reg_t rv32i_fsgnjn_s(processor_t* p, insn_t insn, reg_t pc)
{
  #define xlen 32
  reg_t npc = sext_xlen(pc + insn_length( MATCH_FSGNJN_S));
  #include "insns/fsgnjn_s.h"
  trace_opcode(p,  MATCH_FSGNJN_S, insn);
  #undef xlen
  return npc;
}

reg_t rv64i_fsgnjn_s(processor_t* p, insn_t insn, reg_t pc)
{
  #define xlen 64
  reg_t npc = sext_xlen(pc + insn_length( MATCH_FSGNJN_S));
  #include "insns/fsgnjn_s.h"
  trace_opcode(p,  MATCH_FSGNJN_S, insn);
  #undef xlen
  return npc;
}

#undef CHECK_REG
#define CHECK_REG(reg) require((reg) < 16)

reg_t rv32e_fsgnjn_s(processor_t* p, insn_t insn, reg_t pc)
{
  #define xlen 32
  reg_t npc = sext_xlen(pc + insn_length( MATCH_FSGNJN_S));
  #include "insns/fsgnjn_s.h"
  trace_opcode(p,  MATCH_FSGNJN_S, insn);
  #undef xlen
  return npc;
}

reg_t rv64e_fsgnjn_s(processor_t* p, insn_t insn, reg_t pc)
{
  #define xlen 64
  reg_t npc = sext_xlen(pc + insn_length( MATCH_FSGNJN_S));
  #include "insns/fsgnjn_s.h"
  trace_opcode(p,  MATCH_FSGNJN_S, insn);
  #undef xlen
  return npc;
}
