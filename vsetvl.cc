// See LICENSE for license details.

#include "insn_template.h"
#include "insn_macros.h"

reg_t rv32i_vsetvl(processor_t* p, insn_t insn, reg_t pc)
{
  #define xlen 32
  reg_t npc = sext_xlen(pc + insn_length( MATCH_VSETVL));
  #include "insns/vsetvl.h"
  trace_opcode(p,  MATCH_VSETVL, insn);
  #undef xlen
  return npc;
}

reg_t rv64i_vsetvl(processor_t* p, insn_t insn, reg_t pc)
{
  #define xlen 64
  reg_t npc = sext_xlen(pc + insn_length( MATCH_VSETVL));
  #include "insns/vsetvl.h"
  trace_opcode(p,  MATCH_VSETVL, insn);
  #undef xlen
  return npc;
}

#undef CHECK_REG
#define CHECK_REG(reg) require((reg) < 16)

reg_t rv32e_vsetvl(processor_t* p, insn_t insn, reg_t pc)
{
  #define xlen 32
  reg_t npc = sext_xlen(pc + insn_length( MATCH_VSETVL));
  #include "insns/vsetvl.h"
  trace_opcode(p,  MATCH_VSETVL, insn);
  #undef xlen
  return npc;
}

reg_t rv64e_vsetvl(processor_t* p, insn_t insn, reg_t pc)
{
  #define xlen 64
  reg_t npc = sext_xlen(pc + insn_length( MATCH_VSETVL));
  #include "insns/vsetvl.h"
  trace_opcode(p,  MATCH_VSETVL, insn);
  #undef xlen
  return npc;
}
