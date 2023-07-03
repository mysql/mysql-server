/*
  Copyright (c) 2019, 2022, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "mysqlrouter/partial_buffer_sequence.h"

#include <gtest/gtest.h>

// PartialBufferSequence of std::vector<net::const_buffer>

struct PartialBufferSequenceParam {
  std::vector<std::string> input;
  size_t consumed;  // bytes to skip before using
  size_t length;    // length of captured slice

  std::vector<std::string> expected;
};

class PartialBufferSequenceTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<PartialBufferSequenceParam> {};

TEST_P(PartialBufferSequenceTest, prepare_all) {
  std::vector<std::string> input = GetParam().input;

  std::vector<net::const_buffer> buf_seq_storage;
  for (const auto &v : input) {
    buf_seq_storage.push_back(net::buffer(v));
  }

  classic_protocol::PartialBufferSequence<std::vector<net::const_buffer>>
      buf_seq(buf_seq_storage);
  buf_seq.consume(GetParam().consumed);

  auto prepared = buf_seq.prepare(GetParam().length);

  std::vector<std::string> stringified;
  {
    auto seq_cur = buffer_sequence_begin(prepared);
    const auto seq_end = buffer_sequence_end(prepared);

    for (; seq_cur != seq_end; ++seq_cur) {
      auto buf = *seq_cur;
      if (buf.size() > 0) {
        stringified.emplace_back(static_cast<const char *>(buf.data()),
                                 buf.size());
      }
    }
  }

  EXPECT_EQ(stringified, GetParam().expected);
}

const PartialBufferSequenceParam partial_buffer_sequence_param[] = {
    {{"0", "12", "345"},
     0,
     std::numeric_limits<size_t>::max(),
     {"0", "12", "345"}},
    {{"", "12", "345"}, 0, std::numeric_limits<size_t>::max(), {"12", "345"}},
    {{"0", "", "345"}, 0, std::numeric_limits<size_t>::max(), {"0", "345"}},
    {{"0", "12", "345"}, 1, std::numeric_limits<size_t>::max(), {"12", "345"}},
    {{"0", "12", "345"}, 2, std::numeric_limits<size_t>::max(), {"2", "345"}},
    {{"0", "12", "345"}, 3, std::numeric_limits<size_t>::max(), {"345"}},
    {{"0", "12", "345"}, 12, std::numeric_limits<size_t>::max(), {}},
    {{"0", "12", "345"}, 0, 0, {}},
    {{"0", "12", "345"}, 0, 1, {"0"}},
    {{"0", "12", "345"}, 0, 2, {"0", "1"}},
    {{"0", "12", "345"}, 0, 3, {"0", "12"}},
    {{"0", "12", "345"}, 0, 4, {"0", "12", "3"}},
    {{"0", "12", "345"}, 0, 5, {"0", "12", "34"}},
    {{"0", "12", "345"}, 0, 6, {"0", "12", "345"}},
    {{"0", "12", "345"}, 1, 4, {"12", "34"}},
};

INSTANTIATE_TEST_SUITE_P(Spec, PartialBufferSequenceTest,
                         ::testing::ValuesIn(partial_buffer_sequence_param));

// PartialBufferSequence with a bare net::const_buffer

struct PartialBufferSequenceSingleParam {
  std::string input;
  size_t consumed;
  size_t length;

  std::vector<std::string> expected;
};

class PartialBufferSequenceSingleTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<PartialBufferSequenceSingleParam> {};

TEST_P(PartialBufferSequenceSingleTest, prepare_all) {
  std::string input = GetParam().input;

  net::const_buffer buf_seq_storage(net::buffer(input));

  classic_protocol::PartialBufferSequence<net::const_buffer> buf_seq(
      buf_seq_storage);
  buf_seq.consume(GetParam().consumed);

  auto prepared = buf_seq.prepare(GetParam().length);

  std::vector<std::string> stringified;
  {
    auto seq_cur = buffer_sequence_begin(prepared);
    const auto seq_end = buffer_sequence_end(prepared);

    for (; seq_cur != seq_end; ++seq_cur) {
      auto buf = *seq_cur;
      if (buf.size() > 0) {
        stringified.emplace_back(static_cast<const char *>(buf.data()),
                                 buf.size());
      }
    }
  }

  EXPECT_EQ(stringified, GetParam().expected);
}

const PartialBufferSequenceSingleParam partial_buffer_sequence_single_param[] =
    {
        {"012345", 0, std::numeric_limits<size_t>::max(), {"012345"}},
        {"012345", 1, std::numeric_limits<size_t>::max(), {"12345"}},
        {"012345", 2, std::numeric_limits<size_t>::max(), {"2345"}},
        {"012345", 3, std::numeric_limits<size_t>::max(), {"345"}},
        {"012345", 12, std::numeric_limits<size_t>::max(), {}},
        {"012345", 0, 0, {}},
        {"012345", 0, 1, {"0"}},
        {"012345", 0, 2, {"01"}},
        {"012345", 0, 3, {"012"}},
        {"012345", 0, 4, {"0123"}},
        {"012345", 0, 5, {"01234"}},
        {"012345", 0, 6, {"012345"}},
        {"012345", 1, 4, {"1234"}},
};

INSTANTIATE_TEST_SUITE_P(
    Spec, PartialBufferSequenceSingleTest,
    ::testing::ValuesIn(partial_buffer_sequence_single_param));

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
