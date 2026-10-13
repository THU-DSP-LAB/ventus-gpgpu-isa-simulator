#include "ventus_launch_geometry.h"

#include <cassert>
#include <cstdint>
#include <iostream>

using ventus_launch::Geometry;
using ventus_launch::live_lane_mask;

int main()
{
  /* Generic OpenCL semantics: each physical workgroup is one padded row, so
   * every row has lanes 0..15 live.  This deliberately is not Vulkan RT's
   * packed-raygen mapping, where the bridge changes the physical launch to
   * 256x1x1 before it reaches this generic mask calculation. */
  for (uint64_t row = 0; row < 16; ++row) {
    const Geometry geometry = {16, 16, 1, 32, 1, 1, 0, row, 0};
    assert(live_lane_mask(geometry, 0, 32) == UINT64_C(0x0000ffff));
  }

  /* A compact one-dimensional queue tail remains a prefix. */
  const Geometry compact_tail = {111, 1, 1, 32, 1, 1, 3, 0, 0};
  assert(live_lane_mask(compact_tail, 0, 32) == UINT64_C(0x00007fff));

  /* Packed Vulkan raygen reaches this generic mask as a 1-D launch.  A
   * 17x2 logical image is 34 physical rays, so only lanes 0 and 1 of the
   * second warp remain live. */
  const Geometry packed_raygen_tail = {34, 1, 1, 32, 1, 1, 1, 0, 0};
  assert(live_lane_mask(packed_raygen_tail, 0, 32) == UINT64_C(0x00000003));

  /* A two-dimensional edge can be non-prefix within one warp. */
  const Geometry two_dimensional_edge = {10, 5, 1, 8, 4, 1, 1, 0, 0};
  assert(live_lane_mask(two_dimensional_edge, 0, 32) ==
         UINT64_C(0x03030303));

  /* Z clipping must be independent of X/Y clipping. */
  const Geometry three_dimensional_edge = {4, 3, 3, 4, 2, 2, 0, 1, 1};
  assert(live_lane_mask(three_dimensional_edge, 0, 32) ==
         UINT64_C(0x0000000f));

  const Geometry two_warps = {48, 1, 1, 64, 1, 1, 0, 0, 0};
  assert(live_lane_mask(two_warps, 0, 32) == UINT64_C(0xffffffff));
  assert(live_lane_mask(two_warps, 1, 32) == UINT64_C(0x0000ffff));

  const Geometry invalid = {};
  assert(live_lane_mask(invalid, 0, 32) == 0);

  std::cout << "Ventus launch geometry tests passed\n";
  return 0;
}
