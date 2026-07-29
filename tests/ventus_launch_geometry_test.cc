#include "ventus_launch_geometry.h"

#include <cassert>
#include <cstdint>
#include <iostream>

using ventus_launch::Geometry;
using ventus_launch::live_lane_mask;

int main()
{
  for (uint64_t row = 0; row < 16; ++row) {
    const Geometry geometry = {
        16, 16, 1, 32, 1, 1, 0, row, 0};
    assert(live_lane_mask(geometry, 0, 32) == UINT64_C(0x0000ffff));
  }

  for (uint64_t row = 0; row < 96; ++row) {
    for (uint64_t column = 0; column < 5; ++column) {
      const Geometry geometry = {
          160, 96, 1, 32, 1, 1, column, row, 0};
      assert(live_lane_mask(geometry, 0, 32) == UINT64_C(0xffffffff));
    }
  }

  const Geometry compact_tail = {
      111, 1, 1, 32, 1, 1, 3, 0, 0};
  assert(live_lane_mask(compact_tail, 0, 32) == UINT64_C(0x00007fff));

  const Geometry two_dimensional_edge = {
      10, 5, 1, 8, 4, 1, 1, 1, 0};
  assert(live_lane_mask(two_dimensional_edge, 0, 32) == UINT64_C(0x00000003));

  const Geometry two_warps = {
      48, 1, 1, 64, 1, 1, 0, 0, 0};
  assert(live_lane_mask(two_warps, 0, 32) == UINT64_C(0xffffffff));
  assert(live_lane_mask(two_warps, 1, 32) == UINT64_C(0x0000ffff));

  const Geometry three_dimensional_edge = {
      3, 2, 2, 4, 2, 2, 0, 0, 0};
  assert(live_lane_mask(three_dimensional_edge, 0, 16) ==
         UINT64_C(0x7777));

  const Geometry three_dimensional_z_tail = {
      3, 2, 3, 4, 2, 2, 0, 0, 1};
  assert(live_lane_mask(three_dimensional_z_tail, 0, 16) ==
         UINT64_C(0x0077));

  const Geometry invalid = {};
  assert(live_lane_mask(invalid, 0, 32) == 0);

  std::cout << "Ventus launch geometry tests passed\n";
  return 0;
}
