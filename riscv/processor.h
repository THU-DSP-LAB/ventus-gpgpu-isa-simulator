// See LICENSE for license details.
#ifndef _RISCV_PROCESSOR_H
#define _RISCV_PROCESSOR_H

#include "decode.h"
#include "config.h"
#include "trap.h"
#include "abstract_device.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <cassert>
#include "debug_rom_defines.h"
#include "entropy_source.h"
#include "csrs.h"
#include "isa_parser.h"
#include "triggers.h"
#include <iostream>
#include <iomanip>

//怕重复定义了，先写在这
#include <stack>
#include <stdio.h>
//#include <bitset>

class processor_t;
class mmu_t;
typedef reg_t (*insn_func_t)(processor_t*, insn_t, reg_t); //函数
class simif_t;
class trap_t;
class extension_t;
class disassembler_t;


reg_t illegal_instruction(processor_t* p, insn_t insn, reg_t pc);

//warp scheduler for warp and barrier
class warp_schedule_t
{
  public:
    warp_schedule_t(){
      warp_number=0;thread_number=0;workgroup_number=0;workgroup_id=0;is_all_true=false;barrier_counter=0;
      lds_base=0;lds_size=0;pds_base=0;pds_size=0;knl_base=0;
      workgroup_size_x=0;workgroup_size_y=0;workgroup_size_z=0;
    }
    void set_warp_schedule(size_t w,size_t t,size_t wg,size_t wg_id){
      warp_number=w;
      thread_number=t;
      workgroup_number=wg;
      workgroup_id=wg_id;
      barriers.resize(warp_number, 0);
      }
    void init_warp(const char *gpgpuarch);
    void set_barrier_1(uint64_t wid);
    void set_barrier_2(uint64_t wid);
    void set_barrier_0();
    bool get_barrier();
    void parse_gpgpuarch_string(const char *gpgpuarch);
    ~warp_schedule_t(){}
 
    size_t warp_number;
    size_t thread_number;
    size_t workgroup_number;
    size_t workgroup_id;
    size_t workgroup_size_x;
    size_t workgroup_size_y;
    size_t workgroup_size_z;
    std::vector<int> barriers;
    bool is_all_true;
    int barrier_counter;
    uint64_t lds_base,lds_size,pds_base,pds_size,knl_base;
};

struct insn_desc_t  //mask
{
  insn_bits_t match;
  insn_bits_t mask;
  insn_func_t rv32i;
  insn_func_t rv64i;
  insn_func_t rv32e;
  insn_func_t rv64e;

  insn_func_t func(int xlen, bool rve)
  {
    if (rve)
      return xlen == 64 ? rv64e : rv32e;
    else
      return xlen == 64 ? rv64i : rv32i;
  }

  static insn_desc_t illegal()
  {
    return {0, 0, &illegal_instruction, &illegal_instruction, &illegal_instruction, &illegal_instruction};
  }
};

// regnum, data
typedef std::unordered_map<reg_t, freg_t> commit_log_reg_t;

// addr, value, size
typedef std::vector<std::tuple<reg_t, uint64_t, uint8_t>> commit_log_mem_t;

enum VRM{
  RNU = 0,
  RNE,
  RDN,
  ROD,
  INVALID_RM
};

template<uint64_t N>
struct type_usew_t;

template<>
struct type_usew_t<8>
{
  using type=uint8_t;
};

template<>
struct type_usew_t<16>
{
  using type=uint16_t;
};

template<>
struct type_usew_t<32>
{
  using type=uint32_t;
};

template<>
struct type_usew_t<64>
{
  using type=uint64_t;
};

template<uint64_t N>
struct type_sew_t;

template<>
struct type_sew_t<8>
{
  using type=int8_t;
};

template<>
struct type_sew_t<16>
{
  using type=int16_t;
};

template<>
struct type_sew_t<32>
{
  using type=int32_t;
};

template<>
struct type_sew_t<64>
{
  using type=int64_t;
};

// architectural state of a RISC-V hart
struct state_t
{
  void reset(processor_t* const proc, reg_t max_isa);

  reg_t pc;
  regfile_t<reg_t, NXPR, true> XPR;
  regfile_t<freg_t, NFPR, true> FPR;

  struct regext_t{
  //regext_t():ext_rd(0),ext_rs1(0),ext_rs2(0),ext_rs3(0),ext_imm(0),valid(0){}
  uint64_t ext_rd,ext_rs1,ext_rs2,ext_rs3,ext_imm,valid;
  };
  regext_t regext_info;
  bool regext_enable;
  // control and status registers
  std::unordered_map<reg_t, csr_t_p> csrmap;
  reg_t prv;    // TODO: Can this be an enum instead?
  bool v;
  misa_csr_t_p misa;
  mstatus_csr_t_p mstatus;
  csr_t_p mstatush;
  csr_t_p mepc;
  csr_t_p mtval;
  csr_t_p mtvec;
  csr_t_p mcause;
  wide_counter_csr_t_p minstret;
  wide_counter_csr_t_p mcycle;
  mie_csr_t_p mie;
  mip_csr_t_p mip;
  csr_t_p medeleg;
  csr_t_p mideleg;
  csr_t_p mcounteren;
  csr_t_p scounteren;
  csr_t_p sepc;
  csr_t_p stval;
  csr_t_p stvec;
  virtualized_csr_t_p satp;
  csr_t_p scause;

  csr_t_p mtval2;
  csr_t_p mtinst;
  csr_t_p hstatus;
  csr_t_p hideleg;
  csr_t_p hedeleg;
  csr_t_p hcounteren;
  csr_t_p htval;
  csr_t_p htinst;
  csr_t_p hgatp;
  sstatus_csr_t_p sstatus;
  vsstatus_csr_t_p vsstatus;
  csr_t_p vstvec;
  csr_t_p vsepc;
  csr_t_p vscause;
  csr_t_p vstval;
  csr_t_p vsatp;

  csr_t_p dpc;
  dcsr_csr_t_p dcsr;
  csr_t_p tselect;
  tdata2_csr_t_p tdata2;
  bool debug_mode;

  mseccfg_csr_t_p mseccfg;

  static const int max_pmp = 16;
  pmpaddr_csr_t_p pmpaddr[max_pmp];

  float_csr_t_p fflags;
  float_csr_t_p frm;

  csr_t_p menvcfg;
  csr_t_p senvcfg;
  csr_t_p henvcfg;

  csr_t_p mstateen[4];
  csr_t_p sstateen[4];
  csr_t_p hstateen[4];

  bool serialized; // whether timer CSRs are in a well-defined state

  // When true, execute a single instruction and then enter debug mode.  This
  // can only be set by executing dret.
  enum {
      STEP_NONE,
      STEP_STEPPING,
      STEP_STEPPED
  } single_step;

#ifdef RISCV_ENABLE_COMMITLOG
  commit_log_reg_t log_reg_write;
  commit_log_mem_t log_mem_read;
  commit_log_mem_t log_mem_write;
  reg_t last_inst_priv;
  int last_inst_xlen;
  int last_inst_flen;
#endif
};

typedef enum {
  OPERATION_EXECUTE,
  OPERATION_STORE,
  OPERATION_LOAD,
} trigger_operation_t;

// Count number of contiguous 1 bits starting from the LSB.
static int cto(reg_t val)
{
  int res = 0;
  while ((val & 1) == 1)
    val >>= 1, res++;
  return res;
}
int count_ones(uint64_t n);
// this class represents one processor in a RISC-V machine.
class processor_t : public abstract_device_t
{
public:
  processor_t(const isa_parser_t *isa, const char* varch,
              simif_t* sim, uint32_t id, bool halt_on_reset,
              FILE *log_file, std::ostream& sout_); // because of command line option --log and -s we need both
  ~processor_t();

  const isa_parser_t &get_isa() { return *isa; }

  void set_debug(bool value);
  void set_histogram(bool value);
#ifdef RISCV_ENABLE_COMMITLOG
  void enable_log_commits();
  bool get_log_commits_enabled() const { return log_commits_enabled; }
#endif
  void reset();
  void step(size_t n); // run for n cycles
  void put_csr(int which, reg_t val);
  uint32_t get_id() const { return id; }
  simif_t* get_sim() const{return sim;}
  reg_t get_csr(int which, insn_t insn, bool write, bool peek = 0);
  reg_t get_csr(int which) { return get_csr(which, insn_t(0), false, true); }
  mmu_t* get_mmu() { return mmu; }
  state_t* get_state() { return &state; }
  unsigned get_xlen() const { return xlen; }
  unsigned get_const_xlen() const {
    // Any code that assumes a const xlen should use this method to
    // document that assumption. If Spike ever changes to allow
    // variable xlen, this method should be removed.
    return xlen;
  }
  unsigned get_flen() const {
    return extension_enabled('Q') ? 128 :
           extension_enabled('D') ? 64 :
           extension_enabled('F') ? 32 : 0;
  }
  extension_t* get_extension();
  extension_t* get_extension(const char* name);
  bool any_custom_extensions() const {
    return !custom_extensions.empty();
  }
  bool extension_enabled(unsigned char ext) const {
    if (ext >= 'A' && ext <= 'Z')
      return state.misa->extension_enabled(ext);
    else
      return isa->extension_enabled(ext);
  }
  // Is this extension enabled? and abort if this extension can
  // possibly be disabled dynamically. Useful for documenting
  // assumptions about writable misa bits.
  bool extension_enabled_const(unsigned char ext) const {
    if (ext >= 'A' && ext <= 'Z')
      return state.misa->extension_enabled_const(ext);
    else
      return isa->extension_enabled(ext);  // assume this can't change
  }
  void set_impl(uint8_t impl, bool val) { impl_table[impl] = val; }
  bool supports_impl(uint8_t impl) const {
    return impl_table[impl];
  }
  reg_t pc_alignment_mask() {
    return ~(reg_t)(extension_enabled('C') ? 0 : 2);
  }
  void check_pc_alignment(reg_t pc) {
    if (unlikely(pc & ~pc_alignment_mask()))
      throw trap_instruction_address_misaligned(state.v, pc, 0, 0);
  }
  reg_t legalize_privilege(reg_t);
  void set_privilege(reg_t);
  void set_virt(bool);
  void update_histogram(reg_t pc);
  const disassembler_t* get_disassembler() { return disassembler; }

  FILE *get_log_file() { return log_file; }

  void register_insn(insn_desc_t);
  void register_extension(extension_t*);

  // MMIO slave interface
  bool load(reg_t addr, size_t len, uint8_t* bytes);
  bool store(reg_t addr, size_t len, const uint8_t* bytes);

  // When true, display disassembly of each instruction that's executed.
  bool debug;
  // When true, take the slow simulation path.
  bool slow_path();
  bool halted() { return state.debug_mode; }
  enum {
    HR_NONE,    /* Halt request is inactive. */
    HR_REGULAR, /* Regular halt request/debug interrupt. */
    HR_GROUP    /* Halt requested due to halt group. */
  } halt_request;

  void trigger_updated(const std::vector<triggers::trigger_t *> &triggers);

  void set_pmp_num(reg_t pmp_num);
  void set_pmp_granularity(reg_t pmp_granularity);
  void set_mmu_capability(int cap);

  const char* get_symbol(uint64_t addr);


  void ext_set(int64_t imm,bool is_i){
    state.regext_info.ext_rd = imm & 7;
    state.regext_info.valid = 1;
    if(is_i){
      state.regext_info.ext_rs1 = 0;
      state.regext_info.ext_rs2 = (imm>>3)&7;
      state.regext_info.ext_rs3 = 0;
      state.regext_info.ext_imm = (imm>>6)&63;
    }
    else{
      state.regext_info.ext_rs1 = (imm>>3)&7;
      state.regext_info.ext_rs2 = (imm>>6)&7;
      state.regext_info.ext_rs3 = (imm>>9)&7;
      state.regext_info.ext_imm = 0;
    }
  }
  void ext_clear(){
    state.regext_info.valid=0;
    state.regext_info.ext_imm=0;
    state.regext_info.ext_rd=0;
    state.regext_info.ext_rs1 = 0;
    state.regext_info.ext_rs2 = 0;
    state.regext_info.ext_rs3 = 0;
  }
  uint64_t ext_rs1(){ return state.regext_info.valid ? (state.regext_info.ext_rs1<<5)  : 0;}
  uint64_t ext_rs2(){ return state.regext_info.valid ? (state.regext_info.ext_rs2<<5) : 0;}
  uint64_t ext_rs3(){ return state.regext_info.valid ? (state.regext_info.ext_rs3<<5) : 0;}
  uint64_t ext_rd(){ return state.regext_info.valid ? (state.regext_info.ext_rd<<5) : 0;}
  int64_t ext_imm(){ return state.regext_info.valid ? ( (state.regext_info.ext_imm) << 58 >> 53) : 0;}

private:
  const isa_parser_t * const isa;

  simif_t* sim;
  mmu_t* mmu; // main memory is always accessed via the mmu
  std::unordered_map<std::string, extension_t*> custom_extensions;
  disassembler_t* disassembler;
  state_t state;
  uint32_t id;
  unsigned xlen;
  bool histogram_enabled;
  bool log_commits_enabled;
  FILE *log_file;
  std::ostream sout_; // needed for socket command interface -s, also used for -d and -l, but not for --log
  bool halt_on_reset;
  std::vector<bool> impl_table;

  
  std::vector<insn_desc_t> instructions;
  std::map<reg_t,uint64_t> pc_histogram;

  static const size_t OPCODE_CACHE_SIZE = 8191;
  insn_desc_t opcode_cache[OPCODE_CACHE_SIZE];

  void take_pending_interrupt() { take_interrupt(state.mip->read() & state.mie->read()); }
  void take_interrupt(reg_t mask); // take first enabled interrupt in mask
  void take_trap(trap_t& t, reg_t epc); // take an exception
  void disasm(insn_t insn); // disassemble and print an instruction
  int paddr_bits();

  void enter_debug_mode(uint8_t cause);

  void debug_output_log(std::stringstream *s); // either output to interactive user or write to log file

  friend class mmu_t;
  friend class clint_t;
  friend class extension_t;

  void parse_varch_string(const char*);
  void parse_priv_string(const char*);
  void build_opcode_map();
  void register_base_instructions();
  insn_func_t decode_insn(insn_t insn);

  // Track repeated executions for processor_t::disasm()
  uint64_t last_pc, last_bits, executions;
public:
  entropy_source es; // Crypto ISE Entropy source.

  reg_t n_pmp;
  reg_t lg_pmp_granularity;
  reg_t pmp_tor_mask() { return -(reg_t(1) << (lg_pmp_granularity - PMP_SHIFT)); }

  class vectorUnit_t {
    public:
      processor_t* p;
      void *reg_file;
      char reg_referenced[NVPR];
      int setvl_count;
      reg_t vlmax;
      reg_t vlenb;
      csr_t_p vxsat;
      vector_csr_t_p vxrm, vstart, vl, vtype;
      reg_t vma, vta;
      reg_t vsew;
      float vflmul;
      reg_t ELEN, VLEN;
      bool vill;
      bool vstart_alu;

      // vector element for varies SEW
      template<class T>
        T& elt(int reg_ext,reg_t vReg_to_ext, reg_t n, bool is_write = false){
          reg_t vReg = vReg_to_ext;
          if(reg_ext==0) vReg = vReg | p->ext_rd();
          else if(reg_ext==1) vReg = vReg | p->ext_rs1();
          else if(reg_ext==2) vReg = vReg | p->ext_rs2();
          else if(reg_ext==3) vReg = vReg | p->ext_rs3();
          assert(vsew != 0);
          assert((VLEN >> 3)/sizeof(T) > 0);
          reg_t elts_per_reg = (VLEN >> 3) / (sizeof(T));
          vReg += n / elts_per_reg;
          n = n % elts_per_reg;
#ifdef WORDS_BIGENDIAN
          // "V" spec 0.7.1 requires lower indices to map to lower significant
          // bits when changing SEW, thus we need to index from the end on BE.
          n ^= elts_per_reg - 1;
#endif
          reg_referenced[vReg] = 1;

#ifdef RISCV_ENABLE_COMMITLOG
          if (is_write)
            p->get_state()->log_reg_write[((vReg) << 4) | 2] = {0, 0};
#endif

          T *regStart = (T*)((char*)reg_file + vReg * (VLEN >> 3));
          return regStart[n];
        }
    public:

      void reset();

      vectorUnit_t():
        p(0),
        reg_file(0),
        reg_referenced{0},
        setvl_count(0),
        vlmax(0),
        vlenb(0),
        vxsat(0),
        vxrm(0),
        vstart(0),
        vl(0),
        vtype(0),
        vma(0),
        vta(0),
        vsew(0),
        vflmul(0),
        ELEN(0),
        VLEN(0),
        vill(false),
        vstart_alu(false) {
      }

      ~vectorUnit_t(){
        free(reg_file);
        reg_file = 0;
      }

      reg_t set_vl(int rd, int rs1, reg_t reqVL, reg_t newType);

      reg_t get_vlen() { return VLEN; }
      reg_t get_elen() { return ELEN; }
      reg_t get_slen() { return VLEN; }

      VRM get_vround_mode() {
        return (VRM)(vxrm->read());
      }
  };

  vectorUnit_t VU;
  triggers::module_t TM;

  
  //TODO simt struct

  class gpgpu_unit_t{
    public:
      warp_schedule_t *w;
      csr_t_p rpc;
    private:
      
      processor_t *p;
      // custom csr
      csr_t_p numw;
      csr_t_p numt;
      csr_t_p tid;
      csr_t_p wgid;
      csr_t_p wid;//warp id
      csr_t_p pds;
      csr_t_p lds;
      csr_t_p knl;
      csr_t_p gidx;
      csr_t_p gidy;
      csr_t_p gidz;

      // int warp_id;

    public:
      // clear simt-stack, map and intialize csr
      gpgpu_unit_t() :
        p(0),
        numw(0),
        numt(0),
        tid(0),
        wgid(0),
        wid(0),
        pds(0),
        lds(0),
        knl(0),
        gidx(0),
        gidy(0),
        gidz(0),
        simt_stack() {}

      void reset(processor_t *const proc);
      void set_warp(warp_schedule_t* w);
      

      void init_warp(uint64_t _numw, uint64_t _numt, uint64_t _tid, uint64_t _wgid, uint64_t _wid,uint64_t _pds, uint64_t _lds,uint64_t _knl,uint64_t _gidx,uint64_t _gidy,uint64_t _gidz);

      struct simt_stack_entry_t
      {
        bool      is_part;  //选择输出栈顶 0:else路径信息, 1:汇合点
        reg_t     r_pc;     //汇合点pc
        uint64_t  r_mask;   //汇合点mask，（嵌套情况为上一分支的if mask
        reg_t     else_pc;  //else部分的地址
        uint64_t  else_mask;//else部分的mask
        bool      pair;     //else路径掩码是否为0
        simt_stack_entry_t() :is_part(), r_pc(), r_mask(), else_pc(), else_mask(), pair(){}
        simt_stack_entry_t(bool a, reg_t b, uint64_t c, reg_t d, uint64_t e, bool f) :is_part(a), r_pc(b), r_mask(c), else_pc(d), else_mask(e), pair(f){}
        void dump() {
          std::cout //<< "[is_part] " << is_part << " "
                    << "[r_pc] " << std::hex << std::setw(8) << std::setfill('0') << static_cast<uint32_t>(r_pc) << " " 
                    << "[else_pc] " << std::hex << std::setw(8) << std::setfill('0') << static_cast<uint32_t>(else_pc) << " " 
                    << "[else_mask] " << std::hex << std::setw(8) << std::setfill('0') << else_mask << " ";
                    //<< "[pair] " << pair 
                    // << std::endl;
        }
      };

      class simt_stack_t {
        public:
          //void print(simt_stack_entry_t &entry); 有必要再加
          //void push(simt_stack_entry_t &entry);
          void pop();
          simt_stack_entry_t& top();
          int size();

          void push_branch(reg_t r_pc,reg_t if_pc, uint64_t if_mask, 
                          uint64_t r_mask, reg_t else_pc, uint64_t else_mask); // push r_mask else_pc else_mask
          void pop_join();   // 

          reg_t get_npc() { return npc; };
          uint64_t get_mask() { return mask; };

          void reset();

          void dump() {
            // std::cout << std::endl << "current mask:\t" << std::hex << std::setw(8) << std::setfill('0') << mask << std::endl;
            // std::cout << "current npc:\t" << std::hex << std::setw(8) << std::setfill('0') << static_cast<uint32_t>(npc) << std::endl;
            // std::cout << "stack size: " << _stack.size() << std::endl;

            std::cout << " current mask:\t" << std::hex << std::setw(8) << std::setfill('0') << mask  \
            << " current npc:\t" << std::hex << std::setw(8) << std::setfill('0') << static_cast<uint32_t>(npc) \
            << " stack size: " << _stack.size(); 
            for(auto it = _stack.begin(); it != _stack.end(); it ++) {
              std::cout << std::endl;
              it->dump();
            }
          }

          void init_mask(int numt) {
            mask_width = numt;
            width_mask = 1;
            for(int i = 0; i< numt - 1; i ++) 
              width_mask = width_mask | (width_mask << 1);
            mask = 0xffffffffffffffff & width_mask;
          }
          bool stack_empty(){
            return _stack.empty();
          }

        private:
          std::vector<simt_stack_entry_t> _stack;
          reg_t npc;
          uint64_t mask;

          int mask_width;
          uint64_t width_mask;

          // bool all_one(uint64_t val) { return (val & width_mask) == width_mask; }
          bool all_zero(uint64_t val) { return (val & width_mask) == 0; }
      };

      simt_stack_t simt_stack;
  };

  // simt_stack_t simt_stack;
  gpgpu_unit_t gpgpu_unit;
};

#endif
