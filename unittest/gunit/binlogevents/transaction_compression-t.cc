/* Copyright (c) 2019, 2023, Oracle and/or its affiliates.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <array>

#include <gtest/gtest.h>
#include "libbinlogevents/include/binary_log.h"
#include "libbinlogevents/include/compression/none_comp.h"
#include "libbinlogevents/include/compression/none_dec.h"
#include "libbinlogevents/include/compression/zstd_comp.h"
#include "libbinlogevents/include/compression/zstd_dec.h"
#include "libbinlogevents/include/string/concat.h"

using mysqlns::string::concat;

namespace binary_log {
namespace transaction {
namespace unittests {

static std::size_t MAX_BUFFER_SIZE = 1024 * 1024 * 512;

static std::array<std::size_t, 10> buffer_sizes{
    0, 1, 2, 3, 4, 5, 128, 256, 512, MAX_BUFFER_SIZE / 2};

class TransactionPayloadCompressionTest : public ::testing::Test {
 public:
  using Compressor_t = binary_log::transaction::compression::Compressor;
  using Managed_buffer_sequence_t = Compressor_t::Managed_buffer_sequence_t;
  using Decompressor_t = binary_log::transaction::compression::Decompressor;
  using Managed_buffer_t = Decompressor_t::Managed_buffer_t;
  using Size_t = Decompressor_t::Size_t;
  using Char_t = Decompressor_t::Char_t;
  using String_t = std::basic_string<Char_t>;
  using Decompress_status_t =
      binary_log::transaction::compression::Decompress_status;
  using Compress_status_t =
      binary_log::transaction::compression::Compress_status;

  static String_t constant_data(Size_t size) {
    return String_t(size, (Char_t)'a');
  }

 protected:
  TransactionPayloadCompressionTest() = default;

  void SetUp() override {}

  void TearDown() override {}

  static void compression_idempotency_test(Compressor_t &c, Decompressor_t &d,
                                           String_t data) {
    auto debug_string = concat(
        binary_log::transaction::compression::type_to_string(c.get_type_code()),
        " ", data.size());
    Managed_buffer_sequence_t managed_buffer_sequence;
    c.feed(data.data(), data.size());
    ASSERT_EQ(Compress_status_t::success, c.finish(managed_buffer_sequence))
        << debug_string;

    // assert that it is smaller than the uncompressed size
    auto compressed_size = managed_buffer_sequence.read_part().size();
    if (c.get_type_code() != binary_log::transaction::compression::NONE) {
      // For very small data the constant overhead results in size inflation
      if (data.size() > 100) {
        ASSERT_LT(compressed_size, data.size()) << debug_string;
      }
    } else {
      ASSERT_EQ(compressed_size, data.size()) << debug_string;
    }

    // Get contiguous buffer.
    const auto compressed_data = managed_buffer_sequence.read_part().str();

    // Decompress
    d.feed(compressed_data.data(), compressed_data.size());
    Managed_buffer_t managed_buffer;
    auto ret = d.decompress(managed_buffer, data.size());
    auto expected_status = Decompress_status_t::success;
    if (data.empty() &&
        c.get_type_code() == binary_log::transaction::compression::NONE)
      expected_status = Decompress_status_t::end;
    ASSERT_EQ(ret, expected_status) << debug_string;

    // Check decompressed data
    ASSERT_EQ(managed_buffer.read_part().size(), data.size()) << debug_string;
    ASSERT_EQ(data, String_t(managed_buffer.read_part().begin(),
                             managed_buffer.read_part().end()))
        << debug_string;

    // Check that we reached EOF
    ASSERT_EQ(d.decompress(managed_buffer, 1), Decompress_status_t::end)
        << debug_string;
  }
};

TEST_F(TransactionPayloadCompressionTest, CompressDecompressZstdTest) {
  for (auto size : buffer_sizes) {
    binary_log::transaction::compression::Zstd_dec d;
    binary_log::transaction::compression::Zstd_comp c;
    String_t data{TransactionPayloadCompressionTest::constant_data(size)};
    TransactionPayloadCompressionTest::compression_idempotency_test(c, d, data);
    c.set_compression_level(22);
    TransactionPayloadCompressionTest::compression_idempotency_test(c, d, data);
  }
}

TEST_F(TransactionPayloadCompressionTest, CompressDecompressNoneTest) {
  for (auto size : buffer_sizes) {
    binary_log::transaction::compression::None_dec d;
    binary_log::transaction::compression::None_comp c;
    String_t data{TransactionPayloadCompressionTest::constant_data(size)};
    TransactionPayloadCompressionTest::compression_idempotency_test(c, d, data);
  }
}

}  // namespace unittests
}  // namespace transaction
}  // namespace binary_log
