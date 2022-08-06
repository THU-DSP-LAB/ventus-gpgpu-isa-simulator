// See LICENSE for license details.

#include "insn_template.h"
#include "insn_macros.h"

reg_t rv32i_vamomaxuei16_v(processor_t* p, insn_t insn, reg_t pc)
{
  #define xlen 32
  reg_t npc = sext_xlen(pc + insn_length( MATCH_VAMOMAXUEI16_V));
  #include "insns/vamomaxuei16_v.h"
  trace_opcode(p,  MATCH_VAMOMAXUEI16_V, insn);
  #undef xlen
  return npc;
}

reg_t rv64i_vamomaxuei16_v(processor_t* p, insn_t insn, reg_t pc)
{
  #define xlen 64
  reg_t npc = sext_xlen(pc + insn_length( MATCH_VAMOMAXUEI16_V));
  #include "insns/vamomaxuei16_v.h"
  trace_opcode(p,  MATCH_VAMOMAXUEI16_V, insn);
  #undef xlen
  return npc;
}

#undef CHECK_REG
#define CHECK_REG(reg) require((reg) < 16)

reg_t rv32e_vamomaxuei16_v(processor_t* p, insn_t insn, reg_t pc)
{
  #define xlen 32
  reg_t npc = sext_xlen(pc + insn_length( MATCH_VAMOMAXUEI16_V));
  #include "insns/vamomaxuei16_v.h"
  trace_opcode(p,  MATCH_VAMOMAXUEI16_V, insn);
  #undef xlen
  return npc;
}

reg_t rv64e_vamomaxuei16_v(processor_t* p, insn_t insn, reg_t pc)
{
  #define xlen 64
  reg_t npc = sext_xlen(pc + insn_length( MATCH_VAMOMAXUEI16_V));
  #include "insns/vamomaxuei16_v.h"
  trace_opcode(p,  MATCH_VAMOMAXUEI16_V, insn);
  #undef xlen
  return npc;
}
