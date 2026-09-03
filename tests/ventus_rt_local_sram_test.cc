#include "processor.h"

#include <cassert>
#include <cstdint>

int main()
{
  warp_schedule_t schedule;
  schedule.set_warp_schedule(8, 32, 1, 0);

  static_assert(warp_schedule_t::rt_local_word_count * sizeof(uint32_t) ==
                37888, "RT Local SRAM must be 37KiB");

  for (uint64_t warp = 0; warp < warp_schedule_t::rt_local_warp_count;
       ++warp) {
    for (uint64_t lane = 0; lane < warp_schedule_t::rt_local_lane_count;
         ++lane) {
      schedule.rt_local_store(warp, 0, lane,
                              uint32_t(0x1000 * warp + lane));
      schedule.rt_local_store(warp, 36, lane,
                              uint32_t(0x8000 + 0x100 * warp + lane));
    }
  }

  for (uint64_t warp = 0; warp < warp_schedule_t::rt_local_warp_count;
       ++warp) {
    for (uint64_t lane = 0; lane < warp_schedule_t::rt_local_lane_count;
         ++lane) {
      assert(schedule.rt_local_load(warp, 0, lane) == 0x1000 * warp + lane);
      assert(schedule.rt_local_load(warp, 36, lane) ==
             0x8000 + 0x100 * warp + lane);
    }
  }
  return 0;
}
