// See LICENSE for license details.

#include "insn_template.h"
#include "insn_macros.h"

reg_t rv32i_vmsle_vi(processor_t* p, insn_t insn, reg_t pc)
{
  #define xlen 32
  reg_t npc = sext_xlen(pc + insn_length( MATCH_VMSLE_VI));
  #include "insns/vmsle_vi.h"
  trace_opcode(p,  MATCH_VMSLE_VI, insn);
  #undef xlen
  return npc;
}

reg_t rv64i_vmsle_vi(processor_t* p, insn_t insn, reg_t pc)
{
  #define xlen 64
  reg_t npc = sext_xlen(pc + insn_length( MATCH_VMSLE_VI));
  #include "insns/vmsle_vi.h"
  trace_opcode(p,  MATCH_VMSLE_VI, insn);
  #undef xlen
  return npc;
}

#undef CHECK_REG
#define CHECK_REG(reg) require((reg) < 16)

reg_t rv32e_vmsle_vi(processor_t* p, insn_t insn, reg_t pc)
{
  #define xlen 32
  reg_t npc = sext_xlen(pc + insn_length( MATCH_VMSLE_VI));
  #include "insns/vmsle_vi.h"
  trace_opcode(p,  MATCH_VMSLE_VI, insn);
  #undef xlen
  return npc;
}

reg_t rv64e_vmsle_vi(processor_t* p, insn_t insn, reg_t pc)
{
  #define xlen 64
  reg_t npc = sext_xlen(pc + insn_length( MATCH_VMSLE_VI));
  #include "insns/vmsle_vi.h"
  trace_opcode(p,  MATCH_VMSLE_VI, insn);
  #undef xlen
  return npc;
}
