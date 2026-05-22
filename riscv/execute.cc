// See LICENSE for license details.

#include "processor.h"
#include "mmu.h"
#include "disasm.h"
#include <cassert>

#ifdef RISCV_ENABLE_COMMITLOG
static constexpr int COMMIT_LOG_REG_KIND_MASK = 0xf;
static constexpr int COMMIT_LOG_REG_INDEX_SHIFT = 4;
static constexpr int COMMIT_LOG_XREG = 0;
static constexpr int COMMIT_LOG_FREG = 1;
static constexpr int COMMIT_LOG_VREG = 2;
static constexpr int COMMIT_LOG_VECTOR_HINT = 3;
static constexpr int COMMIT_LOG_CSR = 4;
static constexpr int BITS_PER_VREG_WORD = 32;

static void commit_log_reset(processor_t* p)
{
  p->get_state()->log_reg_write.clear();
  p->get_state()->log_mem_read.clear();
  p->get_state()->log_mem_write.clear();
}

static void commit_log_stash_privilege(processor_t* p)
{
  state_t* state = p->get_state();
  state->last_inst_priv = state->prv;
  state->last_inst_xlen = p->get_xlen();
  state->last_inst_flen = p->get_flen();
}

static void commit_log_print_value(FILE *log_file, int width, const void *data)
{
  assert(log_file);

  switch (width) {
    case 8:
      fprintf(log_file, "0x%01" PRIx8, *(const uint8_t *)data);
      break;
    case 16:
      fprintf(log_file, "0x%04" PRIx16, *(const uint16_t *)data);
      break;
    case 32:
      fprintf(log_file, "0x%08" PRIx32, *(const uint32_t *)data);
      break;
    case 64:
      fprintf(log_file, "0x%016" PRIx64, *(const uint64_t *)data);
      break;
    default:
      // max lengh of vector
      if (((width - 1) & width) == 0) {
        const uint32_t *arr = (const uint32_t *)data;

        fprintf(log_file, "0x");
        for (int idx = width / 32 - 1; idx >= 0; --idx) {
          fprintf(log_file, "%08" PRIx32" ", arr[idx]);
        }
      } else {
        abort();
      }
      break;
  }
}

static void commit_log_print_value(FILE *log_file, int width, uint64_t val)
{
  commit_log_print_value(log_file, width, &val);
}

const char* processor_t::get_symbol(uint64_t addr)
{
  return sim->get_symbol(addr);
}

void commit_log_print_simt_stack(processor_t *p, insn_t insn)
{
  //调用simt_stack的dump()函数，将要打印到cout的内容重定向到string流，然后调用fprintf函数打印到log_file
  FILE *log_file = p->get_log_file();
  std::stringstream ss;
  std::streambuf* oldCoutBuf = std::cout.rdbuf();

  std::cout.rdbuf(ss.rdbuf());
  p->gpgpu_unit.simt_stack.dump();
  std::cout.rdbuf(oldCoutBuf);

  fprintf(log_file, "%s", ss.str().c_str());
  return;
}

static void update_gvmref_vreg_result(processor_t *p, int rd, int size)
{
  p->gvmref_step_ret.insn_result.vreg_result.mask =
      static_cast<uint32_t>(p->gpgpu_unit.simt_stack.get_mask());
  const uint32_t *gvmref_vreg_arr =
      (const uint32_t*)&p->VU.elt<uint8_t>(0, rd, 0);
  int ii = 0;
  for (int idx = 0; idx <= size / BITS_PER_VREG_WORD - 1; ++idx) {
    p->gvmref_step_ret.insn_result.vreg_result.rd[ii] = gvmref_vreg_arr[idx];
    ii++;
  }
}

static void update_gvmref_reg_result(processor_t *p, const std::pair<const reg_t, freg_t>& item)
{
  const int kind = item.first & COMMIT_LOG_REG_KIND_MASK;
  const int rd = item.first >> COMMIT_LOG_REG_INDEX_SHIFT;
  int size = 0;
  bool is_vec = false;
  bool is_vreg = false;
  bool is_csr = false;

  switch (kind) {
  case COMMIT_LOG_XREG:
    size = p->get_state()->last_inst_xlen;
    p->gvmref_step_ret.insn_result.insn_type = XREG;
    break;
  case COMMIT_LOG_FREG:
    size = p->get_state()->last_inst_flen;
    break;
  case COMMIT_LOG_VREG:
    size = p->VU.VLEN;
    p->gvmref_step_ret.insn_result.insn_type = VREG;
    is_vreg = true;
    break;
  case COMMIT_LOG_VECTOR_HINT:
    is_vec = true;
    break;
  case COMMIT_LOG_CSR:
    is_csr = true;
    break;
  default:
    assert("can't been here" && 0);
    break;
  }

  if (is_vec || is_csr)
    return;

  p->gvmref_step_ret.insn_result.xreg_result.reg_idx = rd;
  p->gvmref_step_ret.insn_result.vreg_result.reg_idx = rd;
  if (is_vreg) {
    update_gvmref_vreg_result(p, rd, size);
    return;
  }
  p->gvmref_step_ret.insn_result.xreg_result.rd =
      static_cast<uint32_t>(item.second.v[0]);
}

static void update_gvmref_step_ret(processor_t *p, reg_t pc, insn_t insn)
{
  p->gvmref_step_ret.pc = pc;
  p->gvmref_step_ret.insn = static_cast<uint32_t>(insn.bits());
  p->gvmref_step_ret.insn_result.insn_type = DONT_CARE;

  for (const auto& item : p->get_state()->log_reg_write) {
    if (item.first == 0)
      continue;
    update_gvmref_reg_result(p, item);
  }
}

static void commit_log_print_insn(processor_t *p, reg_t pc, insn_t insn)
{
  FILE *log_file = p->get_log_file();
  auto& reg = p->get_state()->log_reg_write;
  auto& load = p->get_state()->log_mem_read;
  auto& store = p->get_state()->log_mem_write;
  int priv = p->get_state()->last_inst_priv;
  int xlen = p->get_state()->last_inst_xlen;
  int flen = p->get_state()->last_inst_flen;

  // print core id on all lines so it is easy to grep
  fprintf(log_file, "core%4" PRId32 ": ", p->get_id());

  fprintf(log_file, "%1d ", priv);
  commit_log_print_value(log_file, xlen, pc);
  fprintf(log_file, " (");
  commit_log_print_value(log_file, insn.length() * 8, insn.bits());
  fprintf(log_file, ")");
  bool show_vec = false;

  for (auto item : reg) {
    if (item.first == 0)
      continue;

    char prefix = '\0';
    int size = 0;
    int rd = item.first >> COMMIT_LOG_REG_INDEX_SHIFT;
    bool is_vec = false;
    bool is_vreg = false;
    switch (item.first & COMMIT_LOG_REG_KIND_MASK) {
    case COMMIT_LOG_XREG:
      size = xlen;
      prefix = 'x';
      break;
    case COMMIT_LOG_FREG:
      size = flen;
      prefix = 'f';
      break;
    case COMMIT_LOG_VREG:
      size = p->VU.VLEN;
      prefix = 'v';
      is_vreg = true;
      break;
    case COMMIT_LOG_VECTOR_HINT:
      is_vec = true;
      break;
    case COMMIT_LOG_CSR:
      size = xlen;
      prefix = 'c';
      break;
    default:
      assert("can't been here" && 0);
      break;
    }

    if (!show_vec && (is_vreg || is_vec)) {
        fprintf(log_file, " e%ld %s%ld l%ld",
                p->VU.vsew,
                p->VU.vflmul < 1 ? "mf" : "m",
                p->VU.vflmul < 1 ? (reg_t)(1 / p->VU.vflmul) : (reg_t)p->VU.vflmul,
                p->VU.vl->read());
        show_vec = true;
    }
    if (!is_vec) {
      if (prefix == 'c')
        fprintf(log_file, " c%d_%s ", rd, csr_name(rd));
      else {
        fprintf(log_file, " %c%-2d ", prefix, rd);
      }
      if (is_vreg) {
        fprintf(log_file, "%08x ",
                static_cast<uint32_t>(p->gpgpu_unit.simt_stack.get_mask()));
        commit_log_print_value(log_file, size, &p->VU.elt<uint8_t>(0,rd, 0));
      }
      else
        commit_log_print_value(log_file, size, item.second.v);
    } 
  }
//如果是自定义分支相关指令，则打印mask
  if ((insn.bits() & 0x7f) == 0x5b) 
    commit_log_print_simt_stack(p, insn);

  for (auto item : load) {
    fprintf(log_file, " mem ");
    commit_log_print_value(log_file, xlen, std::get<0>(item));
  }

  for (auto item : store) {
    fprintf(log_file, " mem ");
    commit_log_print_value(log_file, xlen, std::get<0>(item));
    fprintf(log_file, " ");
    commit_log_print_value(log_file, std::get<2>(item) << 3, std::get<1>(item));
  }
  fprintf(log_file, "\n");
}

static bool should_record_insn_result(processor_t *p)
{
  return p->get_log_commits_enabled() || p->get_gvmref_step_ret_enabled();
}

static bool has_vector_hint_write(processor_t *p)
{
  for (const auto& item : p->get_state()->log_reg_write) {
    if ((item.first & COMMIT_LOG_REG_KIND_MASK) == COMMIT_LOG_VECTOR_HINT)
      return true;
  }
  return false;
}

static void record_insn_result(processor_t *p, reg_t pc, insn_t insn)
{
  if (p->get_gvmref_step_ret_enabled())
    update_gvmref_step_ret(p, pc, insn);
  if (p->get_log_commits_enabled())
    commit_log_print_insn(p, pc, insn);
}
#else
static void commit_log_reset(processor_t* p) {}
static void commit_log_stash_privilege(processor_t* p) {}
#endif

inline void processor_t::update_histogram(reg_t pc)
{
#ifdef RISCV_ENABLE_HISTOGRAM
  pc_histogram[pc]++;
#endif
}

// This is expected to be inlined by the compiler so each use of execute_insn
// includes a duplicated body of the function to get separate fetch.func
// function calls.
static inline reg_t execute_insn(processor_t* p, reg_t pc, insn_fetch_t fetch)
{
  commit_log_reset(p);
  commit_log_stash_privilege(p);
  reg_t npc;

  try {
    npc = fetch.func(p, fetch.insn, pc);
    if (npc != PC_SERIALIZE_BEFORE) {

#ifdef RISCV_ENABLE_COMMITLOG
      if (should_record_insn_result(p)) {
        record_insn_result(p, pc, fetch.insn);
      }
#endif

     }
#ifdef RISCV_ENABLE_COMMITLOG
  } catch (wait_for_interrupt_t &t) {
      if (should_record_insn_result(p)) {
        record_insn_result(p, pc, fetch.insn);
      }
      throw;
  } catch(mem_trap_t& t) {
      //handle segfault in midlle of vector load/store
      if (should_record_insn_result(p) && has_vector_hint_write(p)) {
        record_insn_result(p, pc, fetch.insn);
      }
      throw;
#endif
  } catch(...) {
    throw;
  }
  p->update_histogram(pc);
  if(p->get_state()->regext_enable){
    p->get_state()->regext_enable=false;
  }
  else{
    p->ext_clear();
  }
  return npc;
}

bool processor_t::slow_path()
{
  return debug || state.single_step != state.STEP_NONE || state.debug_mode;
}

// fetch/decode/execute loop

void processor_t::step(size_t n)
{
  if (!state.debug_mode) {
    if (halt_request == HR_REGULAR) {
      enter_debug_mode(DCSR_CAUSE_DEBUGINT);
    } else if (halt_request == HR_GROUP) {
      enter_debug_mode(DCSR_CAUSE_GROUP);
    } // !!!The halt bit in DCSR is deprecated.
    else if (state.dcsr->halt) {
      enter_debug_mode(DCSR_CAUSE_HALT);
    }
  }

  while (n > 0) {
    size_t instret = 0;
    reg_t pc = state.pc;
    mmu_t* _mmu = mmu;

    #define advance_pc() \
     if (unlikely(invalid_pc(pc))) { \
       switch (pc) { \
         case PC_SERIALIZE_BEFORE: state.serialized = true; break; \
         case PC_SERIALIZE_AFTER: ++instret; break; \
         default: abort(); \
       } \
       pc = state.pc; \
       break; \
     } else { \
       state.pc = pc; \
       instret++; \
     }

    try
    {
      take_pending_interrupt();

      if (unlikely(slow_path()))
      {
        // Main simulation loop, slow path.
        while (instret < n)
        {
          if (unlikely(!state.serialized && state.single_step == state.STEP_STEPPED)) {
            state.single_step = state.STEP_NONE;
            if (!state.debug_mode) {
              enter_debug_mode(DCSR_CAUSE_STEP);
              // enter_debug_mode changed state.pc, so we can't just continue.
              break;
            }
          }

          if (unlikely(state.single_step == state.STEP_STEPPING)) {
            state.single_step = state.STEP_STEPPED;
          }

          insn_fetch_t fetch = mmu->load_insn(pc);  //取指令 load 解码

          if (debug && !state.serialized)
            disasm(fetch.insn);
          pc = execute_insn(this, pc, fetch);
          advance_pc();
        }
      }
      else while (instret < n)
      {
        // Main simulation loop, fast path.
        for (auto ic_entry = _mmu->access_icache(pc); ; ) {
          auto fetch = ic_entry->data;
          pc = execute_insn(this, pc, fetch);
          ic_entry = ic_entry->next;
          if (unlikely(ic_entry->tag != pc))
            break;
          if (unlikely(instret + 1 == n))
            break;
          instret++;
          state.pc = pc;
        }

        advance_pc();
      }
    }
    catch(trap_t& t)
    {
      take_trap(t, pc);
      n = instret;

      if (unlikely(state.single_step == state.STEP_STEPPED)) {
        state.single_step = state.STEP_NONE;
        enter_debug_mode(DCSR_CAUSE_STEP);
      }
    }
    catch (triggers::matched_t& t)
    {
      if (mmu->matched_trigger) {
        // This exception came from the MMU. That means the instruction hasn't
        // fully executed yet. We start it again, but this time it won't throw
        // an exception because matched_trigger is already set. (All memory
        // instructions are idempotent so restarting is safe.)

        insn_fetch_t fetch = mmu->load_insn(pc);
        pc = execute_insn(this, pc, fetch);
        advance_pc();

        delete mmu->matched_trigger;
        mmu->matched_trigger = NULL;
      }
      switch (t.action) {
        case triggers::ACTION_DEBUG_MODE:
          enter_debug_mode(DCSR_CAUSE_HWBP);
          break;
        case triggers::ACTION_DEBUG_EXCEPTION: {
          trap_breakpoint trap(state.v, t.address);
          take_trap(trap, pc);
          break;
        }
        default:
          abort();
      }
    }
    catch(trap_debug_mode&)
    {
      enter_debug_mode(DCSR_CAUSE_SWBP);
    }
    catch (wait_for_interrupt_t &t)
    {
      // Return to the outer simulation loop, which gives other devices/harts a
      // chance to generate interrupts.
      //
      // In the debug ROM this prevents us from wasting time looping, but also
      // allows us to switch to other threads only once per idle loop in case
      // there is activity.
      n = ++instret;
    }

    state.minstret->bump(instret);

    // Model a hart whose CPI is 1.
    state.mcycle->bump(instret);

    n -= instret;
  }
}
