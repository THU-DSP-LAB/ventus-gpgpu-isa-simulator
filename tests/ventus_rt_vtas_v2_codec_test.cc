// Copyright 2026
// SPDX-License-Identifier: BSD-3-Clause

#include <assert.h>
#include <stdint.h>

#include <array>
#include <fstream>
#include <iterator>
#include <vector>

#include "ventus_rt_vtas_v2_codec.h"

using namespace ventus_rt::vtas_v2;

static void store_u16(uint8_t *data, uint32_t offset, uint16_t value)
{
  data[offset + 0] = uint8_t(value);
  data[offset + 1] = uint8_t(value >> 8);
}

static void store_u32(uint8_t *data, uint32_t offset, uint32_t value)
{
  data[offset + 0] = uint8_t(value);
  data[offset + 1] = uint8_t(value >> 8);
  data[offset + 2] = uint8_t(value >> 16);
  data[offset + 3] = uint8_t(value >> 24);
}

static std::array<uint8_t, 512> make_tlas()
{
  constexpr uint32_t instance0 = 80;
  constexpr uint32_t instance1 = 208;
  constexpr uint32_t root = 384;
  std::array<uint8_t, 512> image = {};
  store_u32(image.data(), header_magic, magic);
  store_u16(image.data(), header_version, version);
  store_u16(image.data(), header_size_field, header_size);
  store_u32(image.data(), header_type, 2);
  store_u32(image.data(), header_root_ref, root | node_box4);
  store_u32(image.data(), header_node_count, 3);
  store_u32(image.data(), header_region_offset, header_size);
  store_u32(image.data(), header_region_size, image.size() - header_size);
  store_u32(image.data() + instance0, instance_parent_edge, root | 0);
  store_u32(image.data() + instance1, instance_parent_edge, root | 1);

  uint8_t *box = image.data() + root;
  for (uint32_t i = 0; i < 4; i++)
    store_u32(box, box_child_ref + i * 4, invalid_node_ref);
  store_u32(box, box_child_ref + 0 * 4, instance0 | node_instance);
  store_u32(box, box_child_ref + 1 * 4, instance1 | node_instance);
  store_u32(box, box_parent_ref, invalid_node_ref);
  box[box_parent_child_index] = 0xff;
  box[box_node_flags] = box_root_flag;
  return image;
}

static uint64_t encode(const FrontierEntry &entry)
{
  uint64_t raw = 0;
  assert(encode_frontier_entry(entry, raw));
  FrontierEntry decoded;
  assert(decode_frontier_entry(raw, decoded));
  assert(decoded.node_ref == entry.node_ref);
  assert(decoded.kind == entry.kind);
  assert(decoded.domain == entry.domain);
  assert(decoded.child_anchor == entry.child_anchor);
  assert(decoded.anchor_valid == entry.anchor_valid);
  assert(decoded.inclusive == entry.inclusive);
  return raw;
}

static FrontierEntry entry(uint32_t ref, FrontierKind kind, Domain domain,
                           uint8_t anchor, bool anchor_valid, bool inclusive)
{
  FrontierEntry value;
  value.node_ref = ref;
  value.kind = kind;
  value.domain = domain;
  value.child_anchor = anchor;
  value.anchor_valid = anchor_valid;
  value.inclusive = inclusive;
  return value;
}

int main(int argc, char **argv)
{
  auto image = make_tlas();
  Result result;
  assert(validate_image({image.data(), image.size()}, profile_id, &result));
  assert(!validate_image({image.data(), image.size()}, 0, &result));
  assert(result.error == Error::wrong_profile);

  auto bad = image;
  store_u16(bad.data(), header_version, 1);
  assert(!validate_image({bad.data(), bad.size()}, profile_id, &result));
  assert(result.error == Error::wrong_version);
  bad = image;
  bad[384 + box_reserved] = 1;
  assert(!validate_image({bad.data(), bad.size()}, profile_id, &result));
  assert(result.error == Error::nonzero_reserved);
  bad = image;
  store_u32(bad.data() + 80, instance_parent_edge, 384 | 1);
  assert(!validate_image({bad.data(), bad.size()}, profile_id, &result));
  assert(result.error == Error::bad_parent);

  const FrontierEntry direct = entry(
    384 | node_box4, FrontierKind::direct_node, Domain::tlas, 0, false, false);
  const FrontierEntry replay = entry(
    384 | node_box4, FrontierKind::same_node_replay, Domain::blas, 2, true,
    true);
  const FrontierEntry parent = entry(
    384 | node_box4, FrontierKind::parent_resume, Domain::blas, 1, true,
    false);
  const FrontierEntry cross = entry(
    80 | node_instance, FrontierKind::cross_as_return, Domain::tlas, 0, false,
    false);
  encode(direct);
  encode(replay);
  encode(parent);
  encode(cross);
  FrontierEntry decoded;
  assert(!decode_frontier_entry(uint64_t(1) << 39, decoded));

  uint64_t slots[6] = {};
  StackControl control;
  control.count = 6;
  control.top = 2;
  control.cross_as = true;
  control.lost = true;
  for (uint32_t i = 0; i < 5; i++) {
    FrontierEntry entry = direct;
    entry.domain = Domain::blas;
    slots[(control.top + i) % 6] = encode(entry);
  }
  slots[(control.top + 5) % 6] = encode(cross);
  assert(validate_stack(slots, control));
  slots[(control.top + 5) % 6] = encode(direct);
  assert(!validate_stack(slots, control));

  uint16_t raw_control = 0;
  assert(encode_stack_control(control, raw_control));
  StackControl decoded_control;
  assert(decode_stack_control(raw_control, decoded_control));
  assert(decoded_control.count == 6 && decoded_control.top == 2 &&
         decoded_control.cross_as && decoded_control.lost);
  assert(!decode_stack_control(0x0100, decoded_control));
  assert(!decode_stack_control(uint16_t(6) << 3, decoded_control));
  StackControl empty_bad_top;
  empty_bad_top.top = 6;
  assert(!encode_stack_control(empty_bad_top, raw_control));
  assert(!validate_stack(slots, empty_bad_top));

  FixedFrontier fixed;
  assert(fixed.reset_to(direct, false, false));
  assert(fixed.size() == 1 && fixed.ordinary_size() == 1);
  FrontierEntry parent_tlas = parent;
  parent_tlas.domain = Domain::tlas;
  assert(fixed.push_top(parent_tlas));
  FrontierEntry logical;
  assert(fixed.read_logical(0, logical));
  assert(logical.kind == FrontierKind::parent_resume);
  assert(fixed.read_logical(1, logical));
  assert(logical.kind == FrontierKind::direct_node);
  assert(fixed.pop_top(logical));
  assert(logical.kind == FrontierKind::parent_resume);
  assert(fixed.drop_bottom_ordinary());
  assert(fixed.empty());

  FrontierEntry crossed[2] = {direct, cross};
  crossed[0].domain = Domain::blas;
  assert(fixed.rewrite(crossed, 2, true, false));
  assert(fixed.ordinary_size() == 1);
  assert(fixed.drop_bottom_ordinary());
  assert(fixed.size() == 1 && fixed.ordinary_size() == 0);
  assert(fixed.read_logical(0, logical));
  assert(logical.kind == FrontierKind::cross_as_return);
  assert(fixed.valid());

  crossed[0].node_ref = 80 | node_instance;
  assert(!fixed.rewrite(crossed, 2, true, true));

  if (argc == 2) {
    std::ifstream stream(argv[1], std::ios::binary);
    assert(stream.good());
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(stream)),
                               std::istreambuf_iterator<char>());
    assert(validate_image({bytes.data(), bytes.size()}, profile_id, &result));
  }
  return 0;
}
