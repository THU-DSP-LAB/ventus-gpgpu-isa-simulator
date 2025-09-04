#include "gvmref.h"
#include "workgroup.h"

gvmref_t::~gvmref_t() {
  // Destructor implementation
  wg[wg_id_base]->clear_buffer_data();
  wg.clear();
}
