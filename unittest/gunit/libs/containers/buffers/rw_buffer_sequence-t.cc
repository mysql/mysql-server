/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "mysql/containers/buffers/rw_buffer_sequence.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "mysql/utils/concat.h"

using mysql::utils::concat;

namespace mysql::containers::buffers {
namespace rw_buffer_sequence::unittest {

template <class Rw_buffer_sequence_tp>
class Reposition_tester {
 public:
  using Rw_buffer_sequence_t = Rw_buffer_sequence_tp;
  using Char_t = typename Rw_buffer_sequence_t::Char_t;
  using Difference_t = typename Rw_buffer_sequence_t::Difference_t;
  using Size_t = typename Rw_buffer_sequence_t::Size_t;
  using Container_t = typename Rw_buffer_sequence_t::Container_t;

  /// Verify that the buffer sequence is as expected
  ///
  /// @param debug_string Extra debug info to print in case of failure.
  ///
  /// @param bs Rw_buffer_sequence to check
  ///
  /// @param contents Expected contents of all buffers
  ///
  /// @param underlying The underlying container
  ///
  /// @param read_size The expected size of the read part
  ///
  /// @param write_size The expected size of the write part
  void expect_rw_buffer_sequence(
      const std::string &debug_string, Rw_buffer_sequence_t &bs,
      const std::string &contents, Container_t &underlying,
      std::vector<std::pair<Char_t *, Size_t>> &expected, Size_t read_size,
      Size_t write_size) {
    EXPECT_EQ(bs.capacity(), read_size + write_size) << debug_string;
    EXPECT_EQ(bs.read_part().size(), read_size) << debug_string;
    EXPECT_EQ(bs.read_part().begin(), underlying.begin()) << debug_string;
    EXPECT_EQ(bs.write_part().size(), write_size) << debug_string;
    EXPECT_EQ(bs.write_part().end(), underlying.end()) << debug_string;
    Size_t i = 0;
    auto it = bs.read_part().begin();
    for (; it != bs.write_part().end(); ++i, ++it) {
      auto [base, size] = expected[i];
      EXPECT_EQ(it->size(), size) << debug_string;
      EXPECT_EQ(it->begin(), base) << debug_string;
      EXPECT_EQ(it->end(), base + size) << debug_string;
    }

    EXPECT_EQ(bs.read_part().str(), contents.substr(0, read_size))
        << debug_string;
  }

  void test() {
    std::string str1(30, 'a');
    std::string str2(30, 'b');
    std::string str3(30, 'c');
    std::string contents = concat(str1, str2, str3);
    Char_t *buf1 = reinterpret_cast<Char_t *>(str1.data());
    Char_t *buf2 = reinterpret_cast<Char_t *>(str2.data());
    Char_t *buf3 = reinterpret_cast<Char_t *>(str3.data());
    Container_t underlying;
    underlying.emplace_back();
    underlying.emplace_back(buf1, 30);
    underlying.emplace_back(buf2, 30);
    underlying.emplace_back(buf3, 30);
    Rw_buffer_sequence_t bs(underlying.begin(), underlying.end());
    std::vector<std::vector<std::pair<Char_t *, Size_t>>> expected = {
        {{nullptr, 0}, {buf1, 30}, {buf2, 30}, {buf3, 30}},
        {{buf1, 10}, {buf1 + 10, 20}, {buf2, 30}, {buf3, 30}},
        {{buf1, 20}, {buf1 + 20, 10}, {buf2, 30}, {buf3, 30}},
        {{buf1, 30}, {nullptr, 0}, {buf2, 30}, {buf3, 30}},
        {{buf1, 30}, {buf2, 10}, {buf2 + 10, 20}, {buf3, 30}},
        {{buf1, 30}, {buf2, 20}, {buf2 + 20, 10}, {buf3, 30}},
        {{buf1, 30}, {buf2, 30}, {nullptr, 0}, {buf3, 30}},
        {{buf1, 30}, {buf2, 30}, {buf3, 10}, {buf3 + 10, 20}},
        {{buf1, 30}, {buf2, 30}, {buf3, 20}, {buf3 + 20, 10}},
        {{buf1, 30}, {buf2, 30}, {buf3, 30}, {nullptr, 0}},
    };

    for (Size_t from = 0; from <= 9; ++from) {
      for (Size_t to = 0; to <= 9; ++to) {
        // Can only use a macro since we need __FILE__ and __LINE__.
        // NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define DEBUG_STRING()                                        \
  concat(__FILE__, ":", __LINE__, " from=", from, " to=", to, \
         " bs=", bs.debug_string())

        bs.set_position(from * 10);
        expect_rw_buffer_sequence(DEBUG_STRING(), bs, contents, underlying,
                                  expected[from], from * 10, 90 - from * 10);
        bs.set_position(to * 10);
        expect_rw_buffer_sequence(DEBUG_STRING(), bs, contents, underlying,
                                  expected[to], to * 10, 90 - to * 10);

        bs.set_position(from * 10);
        expect_rw_buffer_sequence(DEBUG_STRING(), bs, contents, underlying,
                                  expected[from], from * 10, 90 - from * 10);
        bs.move_position((Difference_t(to) - Difference_t(from)) * 10);
        expect_rw_buffer_sequence(DEBUG_STRING(), bs, contents, underlying,
                                  expected[to], to * 10, 90 - to * 10);
#undef DEBUG_STRING
      }
    }
  }
};

TEST(RwBufferSequenceTest, CombinatorialRepositionTestCharVector) {
  Reposition_tester<Rw_buffer_sequence<char, std::vector>>().test();
}

TEST(RwBufferSequenceTest, CombinatorialRepositionTestUcharVector) {
  Reposition_tester<Rw_buffer_sequence<unsigned char, std::vector>>().test();
}

TEST(RwBufferSequenceTest, CombinatorialRepositionTestCharList) {
  Reposition_tester<Rw_buffer_sequence<char, std::list>>().test();
}

TEST(RwBufferSequenceTest, CombinatorialRepositionTestUcharList) {
  Reposition_tester<Rw_buffer_sequence<unsigned char, std::list>>().test();
}

}  // namespace rw_buffer_sequence::unittest
}  // namespace mysql::containers::buffers
