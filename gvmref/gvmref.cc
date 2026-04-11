#include "gvmref.h"
#include "workgroup.h"

void gvmref_t::release_kernel_range(uint32_t kernel_wg_base, uint32_t kernel_num_workgroup) {
  if (kernel_num_workgroup == 0) {
    return;
  }

  auto base_it = wg.find(kernel_wg_base);
  if (base_it != wg.end()) {
    base_it->second->clear_buffer_data();
  }

  for (uint32_t i = 0; i < kernel_num_workgroup; ++i) {
    wg.erase(kernel_wg_base + i);
  }
}

void gvmref_t::reclaim_finished_kernel_ranges() {
  while (kernel_ranges.size() > 1 && kernel_ranges.front().finished) {
    const auto range = kernel_ranges.front();
    release_kernel_range(range.wg_base, range.num_workgroup);
    kernel_ranges.pop_front();
  }
}

void gvmref_t::on_kernel_started(uint32_t kernel_wg_base, uint32_t kernel_num_workgroup) {
  kernel_ranges.push_back({kernel_wg_base, kernel_num_workgroup, false});
  reclaim_finished_kernel_ranges();
}

void gvmref_t::on_kernel_finished() {
  for (auto& range : kernel_ranges) {
    if (!range.finished) {
      range.finished = true;
      break;
    }
  }
  reclaim_finished_kernel_ranges();
}

gvmref_t::~gvmref_t() {
  for (const auto& range : kernel_ranges) {
    release_kernel_range(range.wg_base, range.num_workgroup);
  }
  kernel_ranges.clear();
  wg.clear();
}
