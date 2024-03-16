// See LICENSE for license details.

#include "cfg.h"
#include "sim.h"
#include "mmu.h"
#include "remote_bitbang.h"
#include "cachesim.h"
#include "extension.h"
#include <dlfcn.h>
#include <fesvr/option_parser.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <string>
#include <memory>
#include <fstream>
#include "../VERSION"
#include "spike_main.h"

struct kernel_info{
    std::unordered_map<int, bool> blk_list;
    int kernel_id;
    kernel_info(int input_kernel_id, std::unordered_map<int, bool> input_blk_list):
                blk_list(std::move(input_blk_list)),
                kernel_id(input_kernel_id){}
};

struct vAddr_info{
    uint64_t vAddr;
    uint64_t size;
    vAddr_info(uint64_t vAddr_in, uint64_t size_in):
                vAddr(vAddr_in),
                size(size_in){}
};


static void help(int exit_code = 1){
  fprintf(stderr, "now you are in help function");
  exit(exit_code);
}


static void read_file_bytes(const char *filename,size_t fileoff,
                            mem_t* mem, size_t memoff, size_t read_sz)
{
  std::ifstream in(filename, std::ios::in | std::ios::binary);
  in.seekg(fileoff, std::ios::beg);

  std::vector<char> read_buf(read_sz, 0);
  in.read(&read_buf[0], read_sz);
  mem->store(memoff, read_sz, (uint8_t*)&read_buf[0]);
}

bool sort_mem_region(const mem_cfg_t &a, const mem_cfg_t &b)
{
  if (a.base == b.base)
    return (a.size < b.size);
  else
    return (a.base < b.base);
}

void merge_overlapping_memory_regions(std::vector<mem_cfg_t> &mems)
{
  // check the user specified memory regions and merge the overlapping or
  // eliminate the containing parts
  assert(!mems.empty());

  std::sort(mems.begin(), mems.end(), sort_mem_region);
  for (auto it = mems.begin() + 1; it != mems.end(); ) {
    reg_t start = prev(it)->base;
    reg_t end = prev(it)->base + prev(it)->size;
    reg_t start2 = it->base;
    reg_t end2 = it->base + it->size;

    //contains -> remove
    if (start2 >= start && end2 <= end) {
      it = mems.erase(it);
    //partial overlapped -> extend
    } else if (start2 >= start && start2 < end) {
      prev(it)->size = std::max(end, end2) - start;
      it = mems.erase(it);
    // no overlapping -> keep it
    } else {
      it++;
    }
  }
}

static std::vector<mem_cfg_t> parse_mem_layout(const char* arg)
{
  std::vector<mem_cfg_t> res;

  // handle legacy mem argument
  char* p;
  auto mb = strtoull(arg, &p, 0);
  if (*p == 0) {
    reg_t size = reg_t(mb) << 20;
    if (size != (size_t)size)
      throw std::runtime_error("Size would overflow size_t");
    res.push_back(mem_cfg_t(reg_t(DRAM_BASE), size));
    return res;
  }

  // handle base/size tuples
  while (true) {
    auto base = strtoull(arg, &p, 0);
    if (!*p || *p != ':')
        printf("command line input fromat wrong\n");
    auto size = strtoull(p + 1, &p, 0);

    // page-align base and size
    auto base0 = base, size0 = size;
    size += base0 % PGSIZE;
    base -= base0 % PGSIZE;
    if (size % PGSIZE != 0)
      size += PGSIZE - size % PGSIZE;

    if (base + size < base)
        printf("page size alignmentation failed\n");

    if (size != size0) {
      fprintf(stderr, "Warning: the memory at  [0x%llX, 0x%llX] has been realigned\n"
                      "to the %ld KiB page size: [0x%llX, 0x%llX]\n",
              base0, base0 + size0 - 1, long(PGSIZE / 1024), base, base + size - 1);
    }

    res.push_back(mem_cfg_t(reg_t(base), reg_t(size)));
    if (!*p)
      break;
    if (*p != ',')
      help();
    arg = p + 1;
  }

  merge_overlapping_memory_regions(res);

  return res;
}

static std::vector<std::pair<reg_t, mem_t*>> make_mems(const std::vector<mem_cfg_t> &layout)
{
  std::vector<std::pair<reg_t, mem_t*>> mems;
  mems.reserve(layout.size());
  for (const auto &cfg : layout) {
    mems.push_back(std::make_pair(cfg.base, new mem_t(cfg.size)));
  }
  return mems;
}

static unsigned long atoul_safe(const char* s)
{
  char* e;
  auto res = strtoul(s, &e, 10);
  if (*e)
    help();
  return res;
}

static unsigned long atoul_nonzero_safe(const char* s)
{
  auto res = atoul_safe(s);
  if (!res)
    help();
  return res;
}

static std::vector<int> parse_hartids(const char *s)
{
  std::string const str(s);
  std::stringstream stream(str);
  std::vector<int> hartids;

  int n;
  while (stream >> n) {
    hartids.push_back(n);
    if (stream.peek() == ',') stream.ignore();
  }

  return hartids;
}



#define VBASEADDR   0x70000000

#define ARGBASEADDR 0x90000000


spike_device::spike_device():sim(NULL),buffer(),buffer_data(){
  srcfilename=new char[128];
  logfilename=new char[128];
  uint64_t lds_vaddr;
  uint64_t pc_src_vaddr;
  printf("spike device initialize: allocating local memory: ");
  alloc_const_mem(0x10000000,&lds_vaddr);
  printf("spike device initialize: allocating pc source memory: ");
  alloc_const_mem(0x10000000, &pc_src_vaddr);
};

spike_device::~spike_device(){
  delete sim;delete[] srcfilename,logfilename;
  for (auto& mem : buffer_data)
    if(mem.second!=nullptr) {delete mem.second;mem.second=nullptr;}
  const_buffer.clear();
  for (auto& mem : const_buffer_data)
    if(mem.second!=nullptr) {delete mem.second;mem.second=nullptr;}
  const_buffer_data.clear();
}

int spike_device::alloc_const_mem(uint64_t size, uint64_t *vaddr) {
  uint64_t base;
  #define PGSHIFT 12
  const reg_t PGSIZE = 1 << PGSHIFT;
  if(size <= 0 || vaddr == nullptr )
        return -1;
  if(const_buffer.empty()){
    base=VBASEADDR;
  }
  else{
    base=const_buffer.back().base+const_buffer.back().size;
  }
  

  auto base0 = base, size0 = size;
  size += base0 % PGSIZE;
  base -= base0 % PGSIZE;
  if (size % PGSIZE != 0)
    size += PGSIZE - size % PGSIZE;

  if (base + size < base)
    help();

  if (size != size0) {
    fprintf(stderr, "Warning: the memory at  [0x%lX, 0x%lX] has been realigned\n"
                    "to the %ld KiB page size: [0x%lX, 0x%lX]\n",
            base0, base0 + size0 - 1, long(PGSIZE / 1024), base, base + size - 1);
  }

  printf("to allocate at 0x%lx with %ld bytes \n",base,size);

  const_buffer.push_back(mem_cfg_t(reg_t(base),reg_t(size)));  
  const_buffer_data.push_back(std::pair(base,new mem_t(size)));

  *vaddr = base;
  return 0;
}

int spike_device::alloc_local_mem(uint64_t size, uint64_t *vaddr){
  uint64_t base;
  #define PGSHIFT 12
  const reg_t PGSIZE = 1 << PGSHIFT;
  if(size <= 0 || vaddr == nullptr )
        return -1;
  if(buffer.empty()){
    base=ARGBASEADDR;
  }
  else{
    base=buffer.back().base+buffer.back().size;
  }
  

  auto base0 = base, size0 = size;
  size += base0 % PGSIZE;
  base -= base0 % PGSIZE;
  if (size % PGSIZE != 0)
    size += PGSIZE - size % PGSIZE;

  if (base + size < base)
    help();

  if (size != size0) {
    fprintf(stderr, "Warning: the memory at  [0x%lX, 0x%lX] has been realigned\n"
                    "to the %ld KiB page size: [0x%lX, 0x%lX]\n",
            base0, base0 + size0 - 1, long(PGSIZE / 1024), base, base + size - 1);
  }

  printf("to allocate at 0x%lx with %ld bytes \n",base,size);

  buffer.push_back(mem_cfg_t(reg_t(base),reg_t(size)));  
  buffer_data.push_back(std::pair(base,new mem_t(size)));

  *vaddr = base;
  return 0;
}

int spike_device::free_local_mem(){
  buffer.clear();
  for (auto& mem : buffer_data)
    if(mem.second!=nullptr) {delete mem.second;mem.second=nullptr;}
  buffer_data.clear();  
  return 0; 
}

// todo: the memory management should be rewrite
int spike_device::free_local_mem(uint64_t paddr) {
  for (std::vector<mem_cfg_t>::iterator it = buffer.begin(); it != buffer.end(); it++ )
    if(it->base == paddr) {
      it = buffer.erase(it);
      break;
    }
  for (std::vector<std::pair<reg_t, mem_t*>>::iterator it = buffer_data.begin(); it != buffer_data.end(); it++ )
    if(it->first == paddr) {
      delete it->second;
      it = buffer_data.erase(it);
      break;
    }
  return 0;
}

int spike_device::copy_to_dev(uint64_t vaddr, uint64_t size,const void *data){
  uint64_t i=0;
  printf("to copy to 0x%lx with %ld bytes\n",vaddr,size);
  for (i=0; i<buffer.size(); ++i)
    if(vaddr>=buffer[i].base && vaddr<buffer[i].base +buffer[i].size){
      if( vaddr+size > buffer[i].base +buffer[i].size)
        fprintf(stderr,"cannot copy to %#lx with size %lx\n",vaddr,size);
      buffer_data[i].second->store(vaddr-buffer_data[i].first,size,(const uint8_t*)data);
      break;  
    } 
  if(i==buffer.size()) fprintf(stderr,"vaddr do not fit buffer allocated.");  
  return 0;
}

int spike_device::copy_from_dev(uint64_t vaddr, uint64_t size, void *data){
  uint64_t i=0;
  printf("to copy from 0x%lx with %ld bytes\n",vaddr,size);
  for (i=0; i<buffer.size(); ++i)
    if(vaddr>=buffer[i].base && vaddr<buffer[i].base +buffer[i].size){
      if( vaddr+size > buffer[i].base +buffer[i].size)
        fprintf(stderr,"cannot copy to %#lx with size %lx\n",vaddr,size);
      buffer_data[i].second->load(vaddr-buffer_data[i].first,size,(uint8_t*)data);
      break;  
    } 
  if(i==buffer.size()) fprintf(stderr,"vaddr do not fit buffer allocated.");  
  return 0;
}

int spike_device::set_filename(const char* filename,const char* logname){
  sprintf(srcfilename,"%s",filename);
  if(logname==nullptr)
    sprintf(logfilename,"%s.log",filename);
  else
    sprintf(logfilename,"%s",logname);  
  return 0;  
}

#define SPIKE_RUN_WG_NUM 1
int spike_device::run(meta_data* knl_data,uint64_t knl_start_pc){
  uint64_t num_warp=knl_data->wg_size;
  uint64_t num_thread=knl_data->wf_size;
  uint64_t num_workgroup_x=knl_data->kernel_size[0];
  uint64_t num_workgroup_y=knl_data->kernel_size[1];
  uint64_t num_workgroup_z=knl_data->kernel_size[2];
  uint64_t num_workgroup=num_workgroup_x*num_workgroup_y*num_workgroup_z;
  uint64_t num_processor=num_warp*num_workgroup;
  uint64_t ldssize=knl_data->ldsSize;
  //uint64_t pdssize=knl_data->pdsSize * num_thread;
  uint64_t pdssize = 0x10000000;
  uint64_t pdsbase=knl_data->pdsBaseAddr;
  uint64_t start_pc=knl_start_pc;
  uint64_t knlbase=knl_data->metaDataBaseAddr;
  uint64_t currwgid = 0;

  if ((ldssize)>0x10000000) {
        fprintf(stderr, "lds size is too large. please modify VBASEADDR");
        exit(-1);
     }

  cfg_t cfg(/*default_initrd_bounds=*/std::make_pair((reg_t)0, (reg_t)0),
            /*default_bootargs=*/nullptr,
            /*default_isa=*/"RV32GV",
            /*default_priv=*/DEFAULT_PRIV,
            /*default_varch=*/DEFAULT_VARCH,
            /*default_mem_layout=*/parse_mem_layout("2048"),
            /*default_hartids=*/std::vector<int>(),
            /*default_real_time_clint=*/false
  );


  bool debug = false;
  bool halted = false;
  bool histogram = false;
  bool log = false;
  bool socket = false;  // command line option -s
  bool dump_dts = false;
  bool dtb_enabled = true;
  const char* kernel = NULL;
  reg_t kernel_offset, kernel_size;
  std::vector<std::pair<reg_t, abstract_device_t*>> plugin_devices;
  std::unique_ptr<icache_sim_t> ic;
  std::unique_ptr<dcache_sim_t> dc;
  std::unique_ptr<cache_sim_t> l2;
  bool log_cache = false;
  bool log_commits = false;
  const char *log_path = nullptr;
  std::vector<std::function<extension_t*()>> extensions;
  const char* initrd = NULL;
  const char* dtb_file = NULL;
  uint16_t rbb_port = 0;
  bool use_rbb = false;
  unsigned dmi_rti = 0;
  reg_t blocksz = 64;
  debug_module_config_t dm_config = {
    .progbufsize = 2,
    .max_sba_data_width = 0,
    .require_authentication = false,
    .abstract_rti = 0,
    .support_hasel = true,
    .support_abstract_csr_access = true,
    .support_haltgroups = true,
    .support_impebreak = true
  };
  cfg_arg_t<size_t> nprocs(1);

  auto const device_parser = [&plugin_devices](const char *s) {
    const std::string str(s);
    std::istringstream stream(str);

    // We are parsing a string like name,base,args.

    // Parse the name, which is simply all of the characters leading up to the
    // first comma. The validity of the plugin name will be checked later.
    std::string name;
    std::getline(stream, name, ',');
    if (name.empty()) {
      throw std::runtime_error("Plugin name is empty.");
    }

    // Parse the base address. First, get all of the characters up to the next
    // comma (or up to the end of the string if there is no comma). Then try to
    // parse that string as an integer according to the rules of strtoull. It
    // could be in decimal, hex, or octal. Fail if we were able to parse a
    // number but there were garbage characters after the valid number. We must
    // consume the entire string between the commas.
    std::string base_str;
    std::getline(stream, base_str, ',');
    if (base_str.empty()) {
      throw std::runtime_error("Device base address is empty.");
    }
    char* end;
    reg_t base = static_cast<reg_t>(std::strtoull(base_str.c_str(), &end, 0));
    if (end != &*base_str.cend()) {
      throw std::runtime_error("Error parsing device base address.");
    }

    // The remainder of the string is the arguments. We could use getline, but
    // that could ignore newline characters in the arguments. That should be
    // rare and discouraged, but handle it here anyway with this weird in_avail
    // technique. The arguments are optional, so if there were no arguments
    // specified we could end up with an empty string here. That's okay.
    auto avail = stream.rdbuf()->in_avail();
    std::string args(avail, '\0');
    stream.readsome(&args[0], avail);

    plugin_devices.emplace_back(base, new mmio_plugin_device_t(name, args));
  };


  // command analyze
  option_parser_t parser;
  parser.option('d', 0, 0, [&](const char* s){debug = true;});
  parser.option('g', 0, 0, [&](const char* s){histogram = true;});
  parser.option('l', 0, 0, [&](const char* s){log = true;});
#ifdef HAVE_BOOST_ASIO
  parser.option('s', 0, 0, [&](const char* s){socket = true;});
#endif
  parser.option('p', 0, 1, [&](const char* s){nprocs = atoul_nonzero_safe(s);});
  parser.option('m', 0, 1, [&](const char* s){cfg.mem_layout = parse_mem_layout(s);});
  // I wanted to use --halted, but for some reason that doesn't work.
  parser.option('H', 0, 0, [&](const char* s){halted = true;});
  parser.option(0, "rbb-port", 1, [&](const char* s){use_rbb = true; rbb_port = atoul_safe(s);});
  parser.option(0, "pc", 1, [&](const char* s){cfg.start_pc = strtoull(s, 0, 0);});
  parser.option(0, "hartids", 1, [&](const char* s){
    cfg.hartids = parse_hartids(s);
    cfg.explicit_hartids = true;
  });
  parser.option(0, "ic", 1, [&](const char* s){ic.reset(new icache_sim_t(s));});
  parser.option(0, "dc", 1, [&](const char* s){dc.reset(new dcache_sim_t(s));});
  parser.option(0, "l2", 1, [&](const char* s){l2.reset(cache_sim_t::construct(s, "L2$"));});
  parser.option(0, "log-cache-miss", 0, [&](const char* s){log_cache = true;});
  parser.option(0, "isa", 1, [&](const char* s){cfg.isa = s;});
  parser.option(0, "priv", 1, [&](const char* s){cfg.priv = s;});
  parser.option(0, "varch", 1, [&](const char* s){cfg.varch = s;});
  parser.option(0, "gpgpuarch",1,  [&](const char* s){cfg.gpgpuarch = s;});
  parser.option(0, "device", 1, device_parser);
  parser.option(0, "extension", 1, [&](const char* s){extensions.push_back(find_extension(s));});
  parser.option(0, "dump-dts", 0, [&](const char *s){dump_dts = true;});
  parser.option(0, "disable-dtb", 0, [&](const char *s){dtb_enabled = false;});
  parser.option(0, "dtb", 1, [&](const char *s){dtb_file = s;});
  parser.option(0, "kernel", 1, [&](const char* s){kernel = s;});
  parser.option(0, "initrd", 1, [&](const char* s){initrd = s;});
  parser.option(0, "bootargs", 1, [&](const char* s){cfg.bootargs = s;});
  parser.option(0, "real-time-clint", 0, [&](const char *s){cfg.real_time_clint = true;});
  parser.option(0, "extlib", 1, [&](const char *s){
    void *lib = dlopen(s, RTLD_NOW | RTLD_GLOBAL);
    if (lib == NULL) {
      fprintf(stderr, "Unable to load extlib '%s': %s\n", s, dlerror());
      exit(-1);
    }
  });
  parser.option(0, "dm-progsize", 1,
      [&](const char* s){dm_config.progbufsize = atoul_safe(s);});
  parser.option(0, "dm-no-impebreak", 0,
      [&](const char* s){dm_config.support_impebreak = false;});
  parser.option(0, "dm-sba", 1,
      [&](const char* s){dm_config.max_sba_data_width = atoul_safe(s);});
  parser.option(0, "dm-auth", 0,
      [&](const char* s){dm_config.require_authentication = true;});
  parser.option(0, "dmi-rti", 1,
      [&](const char* s){dmi_rti = atoul_safe(s);});
  parser.option(0, "dm-abstract-rti", 1,
      [&](const char* s){dm_config.abstract_rti = atoul_safe(s);});
  parser.option(0, "dm-no-hasel", 0,
      [&](const char* s){dm_config.support_hasel = false;});
  parser.option(0, "dm-no-abstract-csr", 0,
      [&](const char* s){dm_config.support_abstract_csr_access = false;});
  parser.option(0, "dm-no-halt-groups", 0,
      [&](const char* s){dm_config.support_haltgroups = false;});
  parser.option(0, "log-commits", 0,
                [&](const char* s){log_commits = true;});
  parser.option(0, "log", 1,
                [&](const char* s){log_path = s;});
  FILE *cmd_file = NULL;
  parser.option(0, "debug-cmd", 1, [&](const char* s){
     if ((cmd_file = fopen(s, "r"))==NULL) {
        fprintf(stderr, "Unable to open command file '%s'\n", s);
        exit(-1);
     }
  });
  parser.option(0, "blocksz", 1, [&](const char* s){
    blocksz = strtoull(s, 0, 0);
    if (((blocksz & (blocksz - 1))) != 0) {
      fprintf(stderr, "--blocksz should be power of 2\n");
      exit(-1);
    }
  });
  //const char* argv[] = " -d -l --log-commits -p1 --isa rv64gv_zfh --varch vlen:256,elen:32 --gpgpuarch numw:1,numt:8,numwg:1 build/my.riscv > log/my.log 2>&1";
  int argc=14;
  //mem的和sim的config可以直接赋值，但htif的只能通过命令行传
  //为了减少出错的可能，用spike默认模式转为字符串传递。
  
  char arg_num_core[16];
  char arg_vlen_elen[32];
  char arg_mem_scope[64];
  char arg_gpgpu[256];
  char arg_start_pc[32];;
  char arg_logfilename[64];
  sprintf(arg_logfilename,"--log=%s",logfilename);
  sprintf(arg_num_core,"-p%ld",num_processor);
  sprintf(arg_gpgpu,"numw:%ld,numt:%ld,numwg:%ld,kernelx:%ld,kernely:%ld,kernelz:%ld,ldssize:0x%lx,pdssize:0x%lx,pdsbase:0x%lx,knlbase:0x%lx,currwgid:%lx",\
        num_warp,num_thread,num_workgroup,num_workgroup_x,num_workgroup_y,num_workgroup_z,ldssize,pdssize,pdsbase,knlbase,currwgid);
  printf("arg gpgpu is %s\n",arg_gpgpu);
  sprintf(arg_vlen_elen,"vlen:%ld,elen:%d",num_thread*32,32);
  sprintf(arg_mem_scope,"-m0x70000000:0x%lx",buffer.back().base+buffer.back().size);
  printf("vaddr mem scope is %s\n",arg_mem_scope);
  sprintf(arg_start_pc,"--pc=0x%lx",start_pc);
  //strcat(arg_mem_scope,temp);
  //--------------------------------------------------num_core-------------------pc------mem_scope   //mem_scope is unused now.
  //-------------vlen_elen-----------gpgpu-------------------log_file_output
  char strings[][32]={"spike","-l", "--log-commits", " ","--isa", "rv32gcv_zfh", " ","-m0x70000000:0x90000000",\
       "--varch", " ","--gpgpuarch","numw:1,numt:8,numwg:1"," "," "};

  char** argv=new char*[argc];
  for(int i=0;i<argc;i++){
    argv[i]=strings[i];
  }
  argv[11]=arg_gpgpu;
  argv[3]=arg_num_core;
  argv[12]=arg_logfilename;
  printf("src file is %s, run log is written to %s\n",srcfilename,logfilename);
  argv[13]=srcfilename;
  argv[9]=arg_vlen_elen;
  argv[7]=arg_mem_scope;
  argv[6]=arg_start_pc;
  for(int i=0;i<argc;i++){
    printf("%s ",argv[i]);
  }
  printf("\n");

  auto argv1=parser.parse(argv); 

  std::vector<std::string> htif_args(argv1, (const char*const*)argv + argc);
  //std::vector<std::pair<reg_t, mem_t*>> mems = make_mems(cfg.mem_layout());


  //buffer_data = make_mems(cfg.mem_layout());

  //初始化文件使用的是这个方法
  /*for (auto& m: mems){
    if(true){
      std::vector<char> read_buf(read_sz,0);//创建一块待使用的数据。然后用&read_buf[0]来放进去，即mem->store(memoff,read_sz,(uint8_t*)&read_buf[0])来放入
      //mem例化时是reg_t和mem_t*的pair，所以second指向的才是真正的数，reg_t提供的是基址。cfg.base, new mem_t(cfg.size)
      //注意mem.store是只对mem_t使用的，而不是对vec<pair(base,mem_t)>使用的。
    }
  }*/


  if (cfg.explicit_hartids) {
    if (nprocs.overridden() && (nprocs() != cfg.nprocs())) {
      std::cerr << "Number of specified hartids ("
                << cfg.nprocs()
                << ") doesn't match specified number of processors ("
                << nprocs() << ").\n";
      exit(1);
    }
  } else {
    // Set default set of hartids based on nprocs, but don't set the
    // explicit_hartids flag (which means that downstream code can know that
    // we've only set the number of harts, not explicitly chosen their IDs).
    std::vector<int> default_hartids;
    default_hartids.reserve(nprocs());
    for (size_t i = 0; i < num_warp * SPIKE_RUN_WG_NUM; ++i) {
      default_hartids.push_back(i);
    }
    cfg.hartids = default_hartids;
  }

  std::vector<std::pair<reg_t, mem_t*>> all_buffer_data(const_buffer_data);
  for(auto ele : buffer_data) {
      all_buffer_data.push_back(ele);
    }


  auto return_code = 0;
//  char log_name[256] = {0};
  for (uint64_t i = 0; i < num_workgroup / SPIKE_RUN_WG_NUM; i++)
  {
      sim=new sim_t(&cfg, halted,
              all_buffer_data, plugin_devices, htif_args, dm_config, log_path, dtb_enabled, dtb_file,
#ifdef HAVE_BOOST_ASIO
              nullptr, nullptr,
#endif
              cmd_file);
      std::unique_ptr<remote_bitbang_t> remote_bitbang((remote_bitbang_t *) NULL);
      /*std::unique_ptr<jtag_dtm_t> jtag_dtm(new jtag_dtm_t(sim->debug_module, dmi_rti));
        if (use_rbb) {
        remote_bitbang.reset(new remote_bitbang_t(rbb_port, &(*jtag_dtm)));
        sim->set_remote_bitbang(&(*remote_bitbang));
        }*/

      if (dump_dts) {
          printf("%s", sim->get_dts());
          return 0;
      }

      if (ic && l2) ic->set_miss_handler(&*l2);
      if (dc && l2) dc->set_miss_handler(&*l2);
      if (ic) ic->set_log(log_cache);
      if (dc) dc->set_log(log_cache);

      for (size_t i = 0; i < num_warp; i++)
      {
          if (ic) sim->get_core(i)->get_mmu()->register_memtracer(&*ic);
          if (dc) sim->get_core(i)->get_mmu()->register_memtracer(&*dc);
          for (auto e : extensions)
              sim->get_core(i)->register_extension(e());
          sim->get_core(i)->get_mmu()->set_cache_blocksz(blocksz);
      }

      sim->set_debug(debug);
      sim->configure_log(log, log_commits);
      sim->set_histogram(histogram);

      return_code = sim->run();
      currwgid++;
      sprintf(arg_gpgpu,"numw:%ld,numt:%ld,numwg:%ld,kernelx:%ld,kernely:%ld,kernelz:%ld,ldssize:0x%lx,pdssize:0x%lx,pdsbase:0x%lx,knlbase:0x%lx,currwgid:%lx",\
          num_warp,num_thread,num_workgroup,num_workgroup_x,num_workgroup_y,num_workgroup_z,ldssize,pdssize,pdsbase,knlbase,currwgid);
  //    sprintf(log_name, "object_%ld.riscv.log", currwgid);
  //    log_path = log_name;

      delete sim;
  }

  for (auto& plugin_device : plugin_devices)
    delete plugin_device.second;
  delete[] argv;
//  delete sim;
  return return_code;    
}

