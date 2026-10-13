#ifndef RISCV_VENTUS_LAUNCH_GEOMETRY_H
#define RISCV_VENTUS_LAUNCH_GEOMETRY_H

#include <cstdint>
#include <limits>

namespace ventus_launch {

struct Geometry {
  uint64_t global_x;
  uint64_t global_y;
  uint64_t global_z;
  uint64_t local_x;
  uint64_t local_y;
  uint64_t local_z;
  uint64_t group_x;
  uint64_t group_y;
  uint64_t group_z;
};

inline bool checked_mul(uint64_t left, uint64_t right, uint64_t *result)
{
  if (!result ||
      (right != 0 && left > std::numeric_limits<uint64_t>::max() / right))
    return false;
  *result = left * right;
  return true;
}

inline bool checked_mul_add(uint64_t left, uint64_t right, uint64_t addend,
                            uint64_t *result)
{
  if (!result)
    return false;
  uint64_t product = 0;
  if (!checked_mul(left, right, &product) ||
      addend > std::numeric_limits<uint64_t>::max() - product)
    return false;
  *result = product + addend;
  return true;
}

/* Return the lanes whose three-dimensional global IDs are in bounds.  The
 * warp lane order follows the OpenCL local-ID linearization x + lx*(y + ly*z).
 */
inline uint64_t live_lane_mask(const Geometry &geometry, uint64_t warp_index,
                               uint32_t physical_lane_count)
{
  if (physical_lane_count == 0 || physical_lane_count > 64 ||
      geometry.global_x == 0 || geometry.global_y == 0 ||
      geometry.global_z == 0 || geometry.local_x == 0 ||
      geometry.local_y == 0 || geometry.local_z == 0)
    return 0;

  uint64_t local_xy = 0;
  uint64_t local_threads = 0;
  uint64_t warp_first_thread = 0;
  if (!checked_mul(geometry.local_x, geometry.local_y, &local_xy) ||
      !checked_mul(local_xy, geometry.local_z, &local_threads) ||
      !checked_mul(warp_index, physical_lane_count, &warp_first_thread))
    return 0;

  uint64_t mask = 0;
  for (uint32_t lane = 0; lane < physical_lane_count; ++lane) {
    if (warp_first_thread > std::numeric_limits<uint64_t>::max() - lane)
      break;
    const uint64_t local_linear = warp_first_thread + lane;
    if (local_linear >= local_threads)
      break;

    const uint64_t local_z = local_linear / local_xy;
    const uint64_t local_xy_index = local_linear % local_xy;
    const uint64_t local_y = local_xy_index / geometry.local_x;
    const uint64_t local_x = local_xy_index % geometry.local_x;
    uint64_t global_x = 0;
    uint64_t global_y = 0;
    uint64_t global_z = 0;
    if (!checked_mul_add(geometry.group_x, geometry.local_x, local_x,
                         &global_x) ||
        !checked_mul_add(geometry.group_y, geometry.local_y, local_y,
                         &global_y) ||
        !checked_mul_add(geometry.group_z, geometry.local_z, local_z,
                         &global_z))
      continue;
    if (global_x < geometry.global_x && global_y < geometry.global_y &&
        global_z < geometry.global_z)
      mask |= UINT64_C(1) << lane;
  }
  return mask;
}

} // namespace ventus_launch

#endif
