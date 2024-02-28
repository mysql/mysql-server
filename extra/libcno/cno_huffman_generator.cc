/*
  Copyright (c) 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <list>
#include <memory>
#include <vector>

// This source code was created to implement same functionality as
// is supplied by:
//
//    ../cno/hpack-data.py
//
// The output of this application, is and C header file, which
// is used by 'libcno' internally. The file is not included directly
// in MySQL source code, because its big.

namespace generic {

template <typename Container>
bool contains(const Container &c, const typename Container::value_type &v) {
  auto it = std::find(c.begin(), c.end(), v);

  return it != c.end();
}

const size_t k_not_found = std::numeric_limits<size_t>::max();

template <typename Container>
size_t index_of(const Container &c, const typename Container::value_type &v) {
  auto it = std::find(c.begin(), c.end(), v);

  if (it == c.end()) return k_not_found;

  return std::distance(c.begin(), it);
}

}  // namespace generic

namespace data {

struct HuffmanCode {
  HuffmanCode() {}
  HuffmanCode(uint32_t pcode, int pbit_length)
      : code{pcode}, bit_length{pbit_length} {}

  uint32_t code;
  int bit_length;
};

struct Node {
  Node(std::shared_ptr<Node> l, std::shared_ptr<Node> r) : left{l}, right{r} {}
  Node(uint64_t c) : ch{c} {}

  std::shared_ptr<Node> left;
  std::shared_ptr<Node> right;

  std::shared_ptr<Node> get(int idx) {
    switch (idx) {
      case 0:
        return left;
      case 1:
        return right;
      default:
        return {};
    }
  }

  bool has_int_value() { return !left && !right; }

  uint64_t ch{0};
};

struct InnerData {
  InnerData() {}
  InnerData(uint64_t pch, uint32_t pcode, uint64_t pmsb)
      : ch(pch), code(pcode), msb(pmsb) {}

  uint64_t ch{0};
  uint32_t code;
  uint64_t msb;
};

using NodePtr = std::shared_ptr<Node>;

struct HuffmanState {
  HuffmanState() {}
  HuffmanState(int p_next, int p_accept, int p_emit1, int p_emit2, int p_byte1,
               int p_byte2)
      : next{p_next},
        accept{p_accept},
        emit1{p_emit1},
        emit2{p_emit2},
        byte1{p_byte1},
        byte2{p_byte2} {}

  int next;
  int accept;
  int emit1;
  int emit2;
  int byte1;
  int byte2;
};

struct HeaderValue {
  std::string key;
  std::string value;
};

}  // namespace data

const std::vector<data::HeaderValue> k_static_table{
    {":authority", ""},
    {":method", "GET"},
    {":method", "POST"},
    {":path", "/"},
    {":path", "/index.html"},
    {":scheme", "http"},
    {":scheme", "https"},
    {":status", "200"},
    {":status", "204"},
    {":status", "206"},
    {":status", "304"},
    {":status", "400"},
    {":status", "404"},
    {":status", "500"},
    {"accept-charset", ""},
    {"accept-encoding", "gzip, deflate"},
    {"accept-language", ""},
    {"accept-ranges", ""},
    {"accept", ""},
    {"access-control-allow-origin", ""},
    {"age", ""},
    {"allow", ""},
    {"authorization", ""},
    {"cache-control", ""},
    {"content-disposition", ""},
    {"content-encoding", ""},
    {"content-language", ""},
    {"content-length", ""},
    {"content-location", ""},
    {"content-range", ""},
    {"content-type", ""},
    {"cookie", ""},
    {"date", ""},
    {"etag", ""},
    {"expect", ""},
    {"expires", ""},
    {"from", ""},
    {"host", ""},
    {"if-match", ""},
    {"if-modified-since", ""},
    {"if-none-match", ""},
    {"if-range", ""},
    {"if-unmodified-since", ""},
    {"last-modified", ""},
    {"link", ""},
    {"location", ""},
    {"max-forwards", ""},
    {"proxy-authenticate", ""},
    {"proxy-authorization", ""},
    {"range", ""},
    {"referer", ""},
    {"refresh", ""},
    {"retry-after", ""},
    {"server", ""},
    {"set-cookie", ""},
    {"strict-transport-security", ""},
    {"transfer-encoding", ""},
    {"user-agent", ""},
    {"vary", ""},
    {"via", ""},
    {"www-authenticate", ""}};

const std::vector<data::HuffmanCode> k_huffman{
    {0x1ff8, 13},    {0x7fffd8, 23},   {0xfffffe2, 28},  {0xfffffe3, 28},
    {0xfffffe4, 28}, {0xfffffe5, 28},  {0xfffffe6, 28},  {0xfffffe7, 28},
    {0xfffffe8, 28}, {0xffffea, 24},   {0x3ffffffc, 30}, {0xfffffe9, 28},
    {0xfffffea, 28}, {0x3ffffffd, 30}, {0xfffffeb, 28},  {0xfffffec, 28},
    {0xfffffed, 28}, {0xfffffee, 28},  {0xfffffef, 28},  {0xffffff0, 28},
    {0xffffff1, 28}, {0xffffff2, 28},  {0x3ffffffe, 30}, {0xffffff3, 28},
    {0xffffff4, 28}, {0xffffff5, 28},  {0xffffff6, 28},  {0xffffff7, 28},
    {0xffffff8, 28}, {0xffffff9, 28},  {0xffffffa, 28},  {0xffffffb, 28},
    {0x14, 6},       {0x3f8, 10},      {0x3f9, 10},      {0xffa, 12},
    {0x1ff9, 13},    {0x15, 6},        {0xf8, 8},        {0x7fa, 11},
    {0x3fa, 10},     {0x3fb, 10},      {0xf9, 8},        {0x7fb, 11},
    {0xfa, 8},       {0x16, 6},        {0x17, 6},        {0x18, 6},
    {0x0, 5},        {0x1, 5},         {0x2, 5},         {0x19, 6},
    {0x1a, 6},       {0x1b, 6},        {0x1c, 6},        {0x1d, 6},
    {0x1e, 6},       {0x1f, 6},        {0x5c, 7},        {0xfb, 8},
    {0x7ffc, 15},    {0x20, 6},        {0xffb, 12},      {0x3fc, 10},
    {0x1ffa, 13},    {0x21, 6},        {0x5d, 7},        {0x5e, 7},
    {0x5f, 7},       {0x60, 7},        {0x61, 7},        {0x62, 7},
    {0x63, 7},       {0x64, 7},        {0x65, 7},        {0x66, 7},
    {0x67, 7},       {0x68, 7},        {0x69, 7},        {0x6a, 7},
    {0x6b, 7},       {0x6c, 7},        {0x6d, 7},        {0x6e, 7},
    {0x6f, 7},       {0x70, 7},        {0x71, 7},        {0x72, 7},
    {0xfc, 8},       {0x73, 7},        {0xfd, 8},        {0x1ffb, 13},
    {0x7fff0, 19},   {0x1ffc, 13},     {0x3ffc, 14},     {0x22, 6},
    {0x7ffd, 15},    {0x3, 5},         {0x23, 6},        {0x4, 5},
    {0x24, 6},       {0x5, 5},         {0x25, 6},        {0x26, 6},
    {0x27, 6},       {0x6, 5},         {0x74, 7},        {0x75, 7},
    {0x28, 6},       {0x29, 6},        {0x2a, 6},        {0x7, 5},
    {0x2b, 6},       {0x76, 7},        {0x2c, 6},        {0x8, 5},
    {0x9, 5},        {0x2d, 6},        {0x77, 7},        {0x78, 7},
    {0x79, 7},       {0x7a, 7},        {0x7b, 7},        {0x7ffe, 15},
    {0x7fc, 11},     {0x3ffd, 14},     {0x1ffd, 13},     {0xffffffc, 28},
    {0xfffe6, 20},   {0x3fffd2, 22},   {0xfffe7, 20},    {0xfffe8, 20},
    {0x3fffd3, 22},  {0x3fffd4, 22},   {0x3fffd5, 22},   {0x7fffd9, 23},
    {0x3fffd6, 22},  {0x7fffda, 23},   {0x7fffdb, 23},   {0x7fffdc, 23},
    {0x7fffdd, 23},  {0x7fffde, 23},   {0xffffeb, 24},   {0x7fffdf, 23},
    {0xffffec, 24},  {0xffffed, 24},   {0x3fffd7, 22},   {0x7fffe0, 23},
    {0xffffee, 24},  {0x7fffe1, 23},   {0x7fffe2, 23},   {0x7fffe3, 23},
    {0x7fffe4, 23},  {0x1fffdc, 21},   {0x3fffd8, 22},   {0x7fffe5, 23},
    {0x3fffd9, 22},  {0x7fffe6, 23},   {0x7fffe7, 23},   {0xffffef, 24},
    {0x3fffda, 22},  {0x1fffdd, 21},   {0xfffe9, 20},    {0x3fffdb, 22},
    {0x3fffdc, 22},  {0x7fffe8, 23},   {0x7fffe9, 23},   {0x1fffde, 21},
    {0x7fffea, 23},  {0x3fffdd, 22},   {0x3fffde, 22},   {0xfffff0, 24},
    {0x1fffdf, 21},  {0x3fffdf, 22},   {0x7fffeb, 23},   {0x7fffec, 23},
    {0x1fffe0, 21},  {0x1fffe1, 21},   {0x3fffe0, 22},   {0x1fffe2, 21},
    {0x7fffed, 23},  {0x3fffe1, 22},   {0x7fffee, 23},   {0x7fffef, 23},
    {0xfffea, 20},   {0x3fffe2, 22},   {0x3fffe3, 22},   {0x3fffe4, 22},
    {0x7ffff0, 23},  {0x3fffe5, 22},   {0x3fffe6, 22},   {0x7ffff1, 23},
    {0x3ffffe0, 26}, {0x3ffffe1, 26},  {0xfffeb, 20},    {0x7fff1, 19},
    {0x3fffe7, 22},  {0x7ffff2, 23},   {0x3fffe8, 22},   {0x1ffffec, 25},
    {0x3ffffe2, 26}, {0x3ffffe3, 26},  {0x3ffffe4, 26},  {0x7ffffde, 27},
    {0x7ffffdf, 27}, {0x3ffffe5, 26},  {0xfffff1, 24},   {0x1ffffed, 25},
    {0x7fff2, 19},   {0x1fffe3, 21},   {0x3ffffe6, 26},  {0x7ffffe0, 27},
    {0x7ffffe1, 27}, {0x3ffffe7, 26},  {0x7ffffe2, 27},  {0xfffff2, 24},
    {0x1fffe4, 21},  {0x1fffe5, 21},   {0x3ffffe8, 26},  {0x3ffffe9, 26},
    {0xffffffd, 28}, {0x7ffffe3, 27},  {0x7ffffe4, 27},  {0x7ffffe5, 27},
    {0xfffec, 20},   {0xfffff3, 24},   {0xfffed, 20},    {0x1fffe6, 21},
    {0x3fffe9, 22},  {0x1fffe7, 21},   {0x1fffe8, 21},   {0x7ffff3, 23},
    {0x3fffea, 22},  {0x3fffeb, 22},   {0x1ffffee, 25},  {0x1ffffef, 25},
    {0xfffff4, 24},  {0xfffff5, 24},   {0x3ffffea, 26},  {0x7ffff4, 23},
    {0x3ffffeb, 26}, {0x7ffffe6, 27},  {0x3ffffec, 26},  {0x3ffffed, 26},
    {0x7ffffe7, 27}, {0x7ffffe8, 27},  {0x7ffffe9, 27},  {0x7ffffea, 27},
    {0x7ffffeb, 27}, {0xffffffe, 28},  {0x7ffffec, 27},  {0x7ffffed, 27},
    {0x7ffffee, 27}, {0x7ffffef, 27},  {0x7fffff0, 27},  {0x3ffffee, 26},
};

data::NodePtr branch(const std::vector<data::InnerData> &data) {
  if (data.empty()) return {};

  std::vector<data::InnerData> b1, b2;
  for (auto [ch, code, msb] : data) {
    if (0 == msb) {
      return std::make_shared<data::Node>(ch);
    }

    if (code & msb)
      b2.emplace_back(ch, code, msb >> 1);
    else
      b1.emplace_back(ch, code, msb >> 1);
  }

  return std::make_shared<data::Node>(branch(b1), branch(b2));
}

auto huffman_dfa(const std::vector<data::HuffmanCode> &table) {
  std::vector<data::InnerData> new_table;

  auto ch{0};
  for (auto [code, blength] : table) {
    new_table.emplace_back(ch++, code, 1 << (blength - 1));
  }

  auto root = branch(new_table);
  auto tree = root;

  // List is used, because we are dynamically expanding the container
  // while iterating through it. It eunsures that iterators are still
  // valid after expnastion.
  std::list<data::NodePtr> accept{};

  for (int i = 0; i < 8; ++i) {
    accept.push_back(tree);
    tree = tree->get(1);
  }

  assert(std::all_of(accept.begin(), accept.end(),
                     [](auto &ptr) -> bool { return ptr.get(); }));

  std::list<data::NodePtr> states{root};
  std::vector<data::HuffmanState> result;

  for (auto state : states) {
    for (int bits = 0; bits < 256; ++bits) {
      data::NodePtr char1{}, char2{}, next{state};

      for (int bit_index = 7; bit_index >= 0; --bit_index) {
        auto bit = (bits >> bit_index) & 1;

        if (!next) break;

        next = next->get(bit);

        if (next && next->has_int_value()) {
          assert(!char1 && "a single step would yield > 2 characters");
          char1 = char2;
          char2 = next;
          next = root;
        }
      }

      if (!generic::contains(states, next)) {
        states.push_back(next);
      }

      result.emplace_back(generic::index_of(states, next),
                          generic::contains(accept, next) ? 1 : 0,
                          (char1 ? 1 : 0), (char2 ? 1 : 0),
                          (char1 ? char1->ch : 0), (char2 ? char2->ch : 0));
    }
  }

  return result;
}

int main(int argc, const char *argv[]) {
  auto result = huffman_dfa(k_huffman);
  auto min_bits_per_char =
      min_element(k_huffman.begin(), k_huffman.end(),
                  [](const auto &value, const auto &smallest) {
                    return value.bit_length < smallest.bit_length;
                  })
          ->bit_length;

  if (argc != 2) {
    std::cerr << "This application requires an argument:\n";
    std::cerr << "\tPATH_OUTPUT_FILE - location of theoutput-file\n\n";
    std::cerr << "\tcno_huffman_generator PATH_OUTPUT_FILE\n";
    return EXIT_FAILURE;
  }

  std::ofstream file{argv[1], std::ofstream::out | std::ofstream::trunc};

  if (!file.is_open()) {
    std::cerr << "Can't create file \"" << argv[1]
              << "\", specified as argument.\n";
    return EXIT_FAILURE;
  }

  file << "#pragma once\n";
  file << "// make cno/hpack-data.h\n";
  file << "enum {\n";
  file << "    CNO_HPACK_STATIC_TABLE_SIZE = " << k_static_table.size()
       << ",\n";
  file << "    CNO_HUFFMAN_MIN_BITS_PER_CHAR = " << min_bits_per_char << ",\n";
  file << "};\n";

  file << "struct cno_huffman_state_t { uint32_t next : 13, accept : 1, "
          "emit1 : 1, emit2 : 1, byte1 : 8, byte2 : 8; };\n";

  file << "static const struct cno_header_t CNO_HPACK_STATIC_TABLE[] = {";
  bool st_first = true;
  for (auto &[k, v] : k_static_table) {
    if (!st_first) file << ",";
    file << "{{\"" << k << "\"," << k.length() << "},{\"" << v << "\","
         << v.length() << "},0}";
    st_first = false;
  }
  file << "};\n";

  file << "static const uint32_t CNO_HUFFMAN_ENC[] = {";
  st_first = true;
  for (auto [code, blength] : k_huffman) {
    if (!st_first) file << ",";
    file << code << "UL";
    st_first = false;
  }
  file << "};\n";

  file << "static const uint8_t CNO_HUFFMAN_LEN[] = {";
  st_first = true;
  for (auto [code, blength] : k_huffman) {
    if (!st_first) file << ",";
    file << blength;
    st_first = false;
  }
  file << "};\n";

  file << "static const struct cno_huffman_state_t CNO_HUFFMAN_STATE[] = {";
  st_first = true;
  for (auto &el : result) {
    if (!st_first) file << ",";
    file << "{" << el.next << "," << el.accept << "," << el.emit1 << ","
         << el.emit2 << "," << el.byte1 << "," << el.byte2 << "}";
    st_first = false;
  }

  file << "};\n";

  file << "static const struct cno_huffman_state_t "
          "CNO_HUFFMAN_STATE_INIT = {.accept = 1};\n";

  return EXIT_SUCCESS;
}
