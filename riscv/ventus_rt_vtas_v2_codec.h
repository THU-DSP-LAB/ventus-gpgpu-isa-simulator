// Copyright 2026
// SPDX-License-Identifier: BSD-3-Clause

#ifndef VENTUS_RT_VTAS_V2_CODEC_H
#define VENTUS_RT_VTAS_V2_CODEC_H

#include <stddef.h>
#include <stdint.h>

#include <unordered_set>
#include <vector>

namespace ventus_rt {
namespace vtas_v2 {

constexpr uint32_t profile_id = 0x00020001u;
constexpr uint32_t magic = 0x53415456u;
constexpr uint16_t version = 2;
constexpr uint16_t header_size = 80;
constexpr uint32_t invalid_node_ref = 0xffffffffu;
constexpr uint32_t box4_size = 128;
constexpr uint32_t triangle_size = 64;
constexpr uint32_t aabb_size = 64;
constexpr uint32_t instance_size = 128;
constexpr uint32_t node_type_mask = 0x7u;
constexpr uint32_t node_offset_mask = ~node_type_mask;
constexpr uint32_t node_box4 = 1;
constexpr uint32_t node_triangle = 2;
constexpr uint32_t node_instance = 3;
constexpr uint32_t node_aabb = 4;

constexpr uint32_t header_magic = 0x00;
constexpr uint32_t header_version = 0x04;
constexpr uint32_t header_size_field = 0x06;
constexpr uint32_t header_type = 0x08;
constexpr uint32_t header_root_ref = 0x10;
constexpr uint32_t header_node_count = 0x14;
constexpr uint32_t header_region_offset = 0x18;
constexpr uint32_t header_region_size = 0x1c;

constexpr uint32_t box_child_ref = 0x00;
constexpr uint32_t box_parent_ref = 0x70;
constexpr uint32_t box_parent_child_index = 0x74;
constexpr uint32_t box_node_flags = 0x75;
constexpr uint32_t box_child_flags = 0x76;
constexpr uint32_t box_reserved = 0x7a;
constexpr uint8_t box_root_flag = 1;

constexpr uint32_t instance_parent_edge = 0x1c;
constexpr uint32_t instance_parent_offset_mask = 0xffffff80u;
constexpr uint32_t instance_parent_reserved_mask = 0x0000007cu;
constexpr uint32_t instance_parent_child_mask = 0x3u;

enum class Error {
  none,
  truncated,
  wrong_profile,
  bad_magic,
  wrong_version,
  bad_header,
  bad_root,
  bad_node_ref,
  bad_node_type,
  bad_parent,
  duplicate_or_cycle,
  nonzero_reserved,
  node_count_mismatch,
  invalid_frontier_entry,
  invalid_stack_control,
};

struct Result {
  Error error = Error::none;
  uint32_t node_ref = 0;
  uint32_t detail = 0;
};

struct ByteSpan {
  const uint8_t *data;
  size_t size;

  bool u16(size_t offset, uint16_t &value) const
  {
    if (offset > size || sizeof(value) > size - offset)
      return false;
    value = uint16_t(data[offset]) | (uint16_t(data[offset + 1]) << 8);
    return true;
  }

  bool u32(size_t offset, uint32_t &value) const
  {
    if (offset > size || sizeof(value) > size - offset)
      return false;
    value = uint32_t(data[offset]) | (uint32_t(data[offset + 1]) << 8) |
            (uint32_t(data[offset + 2]) << 16) |
            (uint32_t(data[offset + 3]) << 24);
    return true;
  }
};

inline uint32_t node_type(uint32_t ref) { return ref & node_type_mask; }
inline uint32_t node_offset(uint32_t ref) { return ref & node_offset_mask; }

inline size_t node_bytes(uint32_t type)
{
  switch (type) {
  case node_box4: return box4_size;
  case node_triangle: return triangle_size;
  case node_instance: return instance_size;
  case node_aabb: return aabb_size;
  default: return 0;
  }
}

inline bool fail(Result *result, Error error, uint32_t ref, uint32_t detail)
{
  if (result)
  {
    result->error = error;
    result->node_ref = ref;
    result->detail = detail;
  }
  return false;
}

inline bool validate_image(ByteSpan image, uint32_t selected_profile,
                           Result *result = nullptr)
{
  if (result)
    *result = {};
  if (selected_profile != profile_id)
    return fail(result, Error::wrong_profile, 0, selected_profile);
  if (!image.data || image.size < header_size)
    return fail(result, Error::truncated, 0, 0);

  uint32_t value32;
  uint16_t value16;
  if (!image.u32(header_magic, value32) || value32 != magic)
    return fail(result, Error::bad_magic, 0, value32);
  if (!image.u16(header_version, value16) || value16 != version)
    return fail(result, Error::wrong_version, 0, value16);
  if (!image.u16(header_size_field, value16) || value16 != header_size)
    return fail(result, Error::bad_header, 0, header_size_field);

  uint32_t as_type;
  uint32_t root_ref;
  uint32_t expected_count;
  uint32_t region_offset;
  uint32_t region_size;
  if (!image.u32(header_type, as_type) ||
      (as_type != 1 && as_type != 2) ||
      !image.u32(header_root_ref, root_ref) ||
      !image.u32(header_node_count, expected_count) ||
      !image.u32(header_region_offset, region_offset) ||
      region_offset != header_size ||
      !image.u32(header_region_size, region_size))
    return fail(result, Error::bad_header, 0, 0);
  if (region_size > image.size - header_size)
    return fail(result, Error::truncated, 0, region_size);
  const size_t used_size = header_size + size_t(region_size);

  if (root_ref == invalid_node_ref) {
    if (expected_count != 0)
      return fail(result, Error::node_count_mismatch, root_ref,
                  expected_count);
    return true;
  }
  if (expected_count == 0)
    return fail(result, Error::bad_root, root_ref, 0);

  struct Work {
    uint32_t ref;
    uint32_t parent_ref;
    uint8_t parent_child;
    bool root;
  };
  std::vector<Work> work = {{root_ref, invalid_node_ref, 0xff, true}};
  std::unordered_set<uint32_t> visited;
  uint32_t reachable = 0;

  while (!work.empty()) {
    const Work item = work.back();
    work.pop_back();
    if (!visited.insert(item.ref).second)
      return fail(result, Error::duplicate_or_cycle, item.ref, 0);

    const uint32_t type = node_type(item.ref);
    const uint32_t offset = node_offset(item.ref);
    const size_t bytes = node_bytes(type);
    if (!bytes)
      return fail(result, Error::bad_node_type, item.ref, type);
    if (offset < header_size || offset > used_size || bytes > used_size - offset)
      return fail(result, Error::bad_node_ref, item.ref, offset);
    if (type == node_box4 && (offset & (box4_size - 1)) != 0)
      return fail(result, Error::bad_node_ref, item.ref, offset);

    ByteSpan node = {image.data + offset, bytes};
    reachable++;
    if (type == node_box4) {
      uint32_t parent_ref;
      if (!node.u32(box_parent_ref, parent_ref))
        return fail(result, Error::truncated, item.ref, 0);
      const uint8_t parent_child = node.data[box_parent_child_index];
      const uint8_t flags = node.data[box_node_flags];
      if (flags & ~box_root_flag)
        return fail(result, Error::nonzero_reserved, item.ref, flags);
      if (item.root) {
        if (parent_ref != invalid_node_ref || parent_child != 0xff ||
            flags != box_root_flag)
          return fail(result, Error::bad_root, item.ref, parent_ref);
      } else if (parent_ref != item.parent_ref ||
                 parent_child != item.parent_child || flags != 0) {
        return fail(result, Error::bad_parent, item.ref, parent_ref);
      }
      for (uint32_t i = 0; i < 4; i++) {
        if (node.data[box_child_flags + i] != 0)
          return fail(result, Error::nonzero_reserved, item.ref,
                      box_child_flags + i);
      }
      for (uint32_t i = 0; i < 6; i++) {
        if (node.data[box_reserved + i] != 0)
          return fail(result, Error::nonzero_reserved, item.ref,
                      box_reserved + i);
      }
      for (uint32_t i = 0; i < 4; i++) {
        uint32_t child;
        node.u32(box_child_ref + i * 4, child);
        if (child != invalid_node_ref)
          work.push_back({child, item.ref, uint8_t(i), false});
      }
      continue;
    }

    if (type == node_instance) {
      if (as_type != 2)
        return fail(result, Error::bad_node_type, item.ref, type);
      uint32_t edge;
      node.u32(instance_parent_edge, edge);
      if (item.root) {
        if (edge != invalid_node_ref)
          return fail(result, Error::bad_root, item.ref, edge);
      } else if ((edge & instance_parent_reserved_mask) != 0 ||
                 (edge & instance_parent_offset_mask) !=
                     node_offset(item.parent_ref) ||
                 (edge & instance_parent_child_mask) != item.parent_child) {
        return fail(result, Error::bad_parent, item.ref, edge);
      }
      continue;
    }

    if (as_type != 1 || (item.root && as_type == 2))
      return fail(result, Error::bad_node_type, item.ref, type);
  }

  if (reachable != expected_count)
    return fail(result, Error::node_count_mismatch, root_ref, reachable);
  return true;
}

enum class FrontierKind : uint8_t {
  direct_node = 0,
  same_node_replay = 1,
  parent_resume = 2,
  cross_as_return = 3,
};

enum class Domain : uint8_t { tlas = 0, blas = 1 };

struct FrontierEntry {
  uint32_t node_ref = invalid_node_ref;
  FrontierKind kind = FrontierKind::direct_node;
  Domain domain = Domain::tlas;
  uint8_t child_anchor = 0;
  bool anchor_valid = false;
  bool inclusive = false;
};

inline bool validate_frontier_entry(const FrontierEntry &entry)
{
  if (entry.child_anchor > 3)
    return false;
  switch (entry.kind) {
  case FrontierKind::direct_node:
    return !entry.anchor_valid && !entry.inclusive && entry.child_anchor == 0;
  case FrontierKind::same_node_replay:
    return node_type(entry.node_ref) == node_box4 &&
           (entry.anchor_valid ||
            (!entry.inclusive && entry.child_anchor == 0));
  case FrontierKind::parent_resume:
    return node_type(entry.node_ref) == node_box4 && entry.anchor_valid &&
           !entry.inclusive;
  case FrontierKind::cross_as_return:
    return entry.domain == Domain::tlas &&
           node_type(entry.node_ref) == node_instance &&
           entry.child_anchor == 0 && !entry.anchor_valid && !entry.inclusive;
  }
  return false;
}

inline bool encode_frontier_entry(const FrontierEntry &entry, uint64_t &raw)
{
  if (!validate_frontier_entry(entry))
    return false;
  raw = uint64_t(entry.node_ref) |
        (uint64_t(entry.kind) << 32) |
        (uint64_t(entry.domain) << 34) |
        (uint64_t(entry.child_anchor) << 35) |
        (uint64_t(entry.anchor_valid) << 37) |
        (uint64_t(entry.inclusive) << 38);
  return true;
}

inline bool decode_frontier_entry(uint64_t raw, FrontierEntry &entry)
{
  if ((raw >> 39) != 0)
    return false;
  entry.node_ref = uint32_t(raw);
  entry.kind = FrontierKind((raw >> 32) & 0x3);
  entry.domain = Domain((raw >> 34) & 0x1);
  entry.child_anchor = uint8_t((raw >> 35) & 0x3);
  entry.anchor_valid = ((raw >> 37) & 0x1) != 0;
  entry.inclusive = ((raw >> 38) & 0x1) != 0;
  return validate_frontier_entry(entry);
}

struct StackControl {
  uint8_t count = 0;
  uint8_t top = 0;
  bool cross_as = false;
  bool lost = false;
};

inline bool decode_stack_control(uint16_t raw, StackControl &control)
{
  if ((raw & 0xff00u) != 0)
    return false;
  control.count = raw & 0x7;
  control.top = (raw >> 3) & 0x7;
  control.cross_as = ((raw >> 6) & 1) != 0;
  control.lost = ((raw >> 7) & 1) != 0;
  return control.count <= 6 && control.top < 6 &&
         (!control.cross_as || control.count >= 1);
}

inline bool encode_stack_control(const StackControl &control, uint16_t &raw)
{
  if (control.count > 6 || control.top >= 6 ||
      (control.cross_as && control.count == 0))
    return false;
  raw = uint16_t(control.count) | (uint16_t(control.top) << 3) |
        (uint16_t(control.cross_as) << 6) |
        (uint16_t(control.lost) << 7);
  return true;
}

inline bool validate_stack(const uint64_t slots[6], const StackControl &control)
{
  if (control.count > 6 || control.top >= 6 ||
      (control.cross_as && control.count == 0))
    return false;
  for (uint32_t i = 0; i < control.count; i++) {
    FrontierEntry entry;
    if (!decode_frontier_entry(slots[(control.top + i) % 6], entry))
      return false;
    const bool bottom = i + 1 == control.count;
    if (control.cross_as) {
      if (bottom) {
        if (entry.kind != FrontierKind::cross_as_return ||
            entry.domain != Domain::tlas)
          return false;
      } else if (entry.kind == FrontierKind::cross_as_return ||
                 entry.domain != Domain::blas) {
        return false;
      }
    } else if (entry.kind == FrontierKind::cross_as_return ||
               entry.domain != Domain::tlas) {
      return false;
    }
  }
  if (control.lost) {
    const uint8_t ordinary_count =
        control.count - (control.cross_as ? 1 : 0);
    if (ordinary_count == 0)
      return false;
    FrontierEntry recovery;
    if (!decode_frontier_entry(
            slots[(control.top + ordinary_count - 1) % 6], recovery) ||
        node_type(recovery.node_ref) != node_box4 ||
        recovery.kind == FrontierKind::cross_as_return)
      return false;
  }
  return true;
}

class FixedFrontier {
public:
  const StackControl &control() const { return control_; }
  StackControl &control() { return control_; }
  const uint64_t *slots() const { return slots_; }

  void clear()
  {
    for (uint64_t &slot : slots_)
      slot = 0;
    control_ = {};
  }

  bool empty() const { return control_.count == 0; }
  uint8_t size() const { return control_.count; }
  uint8_t ordinary_size() const
  {
    return control_.count - (control_.cross_as ? 1 : 0);
  }

  bool peek_top(FrontierEntry &entry) const
  {
    return control_.count != 0 &&
           decode_frontier_entry(slots_[control_.top], entry);
  }

  bool read_logical(uint8_t logical_index, FrontierEntry &entry) const
  {
    if (logical_index >= control_.count)
      return false;
    return decode_frontier_entry(
        slots_[(control_.top + logical_index) % 6], entry);
  }

  bool push_top(const FrontierEntry &entry)
  {
    uint64_t raw;
    if (control_.count >= 6 || !encode_frontier_entry(entry, raw))
      return false;
    control_.top = control_.count == 0
                       ? 0
                       : uint8_t((control_.top + 5) % 6);
    slots_[control_.top] = raw;
    control_.count++;
    return validate_stack(slots_, control_);
  }

  bool pop_top(FrontierEntry &entry)
  {
    if (!peek_top(entry))
      return false;
    slots_[control_.top] = 0;
    control_.count--;
    control_.top = control_.count == 0
                       ? 0
                       : uint8_t((control_.top + 1) % 6);
    return true;
  }

  bool drop_bottom_ordinary()
  {
    if (ordinary_size() == 0)
      return false;
    if (!control_.cross_as) {
      const uint8_t bottom = uint8_t((control_.top + control_.count - 1) % 6);
      slots_[bottom] = 0;
      control_.count--;
      if (control_.count == 0)
        control_.top = 0;
      return validate_stack(slots_, control_);
    }

    const uint8_t ordinary_bottom =
        uint8_t((control_.top + control_.count - 2) % 6);
    const uint8_t return_bottom =
        uint8_t((control_.top + control_.count - 1) % 6);
    slots_[ordinary_bottom] = slots_[return_bottom];
    slots_[return_bottom] = 0;
    control_.count--;
    return validate_stack(slots_, control_);
  }

  bool reset_to(const FrontierEntry &entry, bool cross_as, bool lost)
  {
    clear();
    uint64_t raw;
    if (!encode_frontier_entry(entry, raw))
      return false;
    slots_[0] = raw;
    control_.count = 1;
    control_.top = 0;
    control_.cross_as = cross_as;
    control_.lost = lost;
    return validate_stack(slots_, control_);
  }

  bool rewrite(const FrontierEntry *entries, uint8_t count,
               bool cross_as, bool lost)
  {
    if (count > 6 || (count != 0 && entries == nullptr))
      return false;
    clear();
    control_.count = count;
    control_.top = 0;
    control_.cross_as = cross_as;
    control_.lost = lost;
    for (uint8_t index = 0; index < count; ++index) {
      if (!encode_frontier_entry(entries[index], slots_[index])) {
        clear();
        return false;
      }
    }
    if (!validate_stack(slots_, control_)) {
      clear();
      return false;
    }
    return true;
  }

  bool valid() const { return validate_stack(slots_, control_); }

private:
  uint64_t slots_[6] = {};
  StackControl control_;
};

} // namespace vtas_v2
} // namespace ventus_rt

#endif
