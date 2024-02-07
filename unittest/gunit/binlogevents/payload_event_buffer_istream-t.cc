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

#include <iostream>

#include <array>
#include <random>
#include <vector>

#include <gtest/gtest.h>
#include "my_byteorder.h"
#include "mysql/binlog/event/binary_log.h"
#include "mysql/binlog/event/compression/none_comp.h"
#include "mysql/binlog/event/compression/payload_event_buffer_istream.h"
#include "mysql/binlog/event/compression/zstd_comp.h"
#include "mysql/binlog/event/string/concat.h"

using mysql::binlog::event::string::concat;

namespace mysql::binlog::event::unittests {

/// Wrapper around an Iterator, providing an iterator that restarts
/// from the beginning, whenever the underlying one reaches the end.
template <class IT>
class Cyclic_iterator {
 public:
  using value_type = typename IT::value_type;
  using pointer = typename IT::pointer;
  using reference = typename IT::reference;
  Cyclic_iterator(const IT &begin, const IT &end, const IT &initial)
      : m_begin(begin), m_end(end), m_it(initial) {}
  Cyclic_iterator(const IT &begin, const IT &end)
      : Cyclic_iterator(begin, end, begin) {}
  value_type operator++() {  // ++it
    ++m_it;
    if (m_it == m_end) m_it = m_begin;
    return *m_it;
  }
  value_type operator++(int) {  // it++
    auto ret = *m_it;
    ++m_it;
    if (m_it == m_end) m_it = m_begin;
    return ret;
  }
  reference operator*() { return *m_it; }
  pointer operator->() { return m_it; }

 private:
  const IT m_begin;
  const IT m_end;
  IT m_it;
};

template <class Compressor_tp>
class PayloadEventBufferStreamTest {
 public:
  using Managed_buffer_t =
      mysql::binlog::event::compression::buffer::Managed_buffer<>;
  using Managed_Buffer_sequence_t =
      mysql::binlog::event::compression::buffer::Managed_buffer_sequence<>;
  using Compressor_t = Compressor_tp;
  using Compress_status_t = mysql::binlog::event::compression::Compress_status;
  using Decompress_status_t =
      mysql::binlog::event::compression::Decompress_status;
  using Size_t = typename Compressor_t::Size_t;
  using Char_t = typename Compressor_t::Char_t;
  using String_t = std::basic_string<Char_t>;
  using Stream_t =
      mysql::binlog::event::compression::Payload_event_buffer_istream;
  using Buffer_ptr_t = Stream_t::Buffer_ptr_t;
  static constexpr auto type_code = Compressor_t::type_code;
  using Memory_resource_t = mysql::binlog::event::resource::Memory_resource;

  // Change to true to get more debug info
  static constexpr bool m_trace = false;

  std::random_device::result_type m_seed;
  static constexpr int m_event_timestamp = 4711;
  std::mt19937_64 m_random_number_generator;
  std::vector<Log_event_type> m_type_vector;

  PayloadEventBufferStreamTest() {
    m_type_vector = {QUERY_EVENT, ROWS_QUERY_LOG_EVENT, TABLE_MAP_EVENT,
                     WRITE_ROWS_EVENT, XID_EVENT};
    /// Generate a random seed for the deterministic RNG.
    m_seed = std::random_device()();
    m_random_number_generator.seed(m_seed);
  }

  std::string mock_one_event(Size_t data_size, Log_event_type type) {
    std::string ret(LOG_EVENT_HEADER_LEN + data_size, '\0');
    char *p = ret.data();
    // Store some of the necessary fields in the common-header
    int4store(p, m_event_timestamp);
    p[EVENT_TYPE_OFFSET] = type;
    int4store(p + SERVER_ID_OFFSET, 1);
    int4store(p + EVENT_LEN_OFFSET,
              static_cast<uint32>(LOG_EVENT_HEADER_LEN + data_size));
    // Fill the rest with "deterministic, compressible garbage".  We
    // just repeat the type code.
    std::memset(p + LOG_EVENT_HEADER_LEN, (int)type, data_size);
    return ret;
  }

  void check_one_event(const std::string &debug_string,
                       const std::string &buffer, Size_t expected_data_size,
                       Log_event_type expected_type) {
    const auto *p = buffer.data();
    auto expected_event_size = LOG_EVENT_HEADER_LEN + expected_data_size;
    ASSERT_EQ(buffer.size(), expected_event_size) << debug_string;
    ASSERT_EQ(uint4korr(p), m_event_timestamp) << debug_string;
    ASSERT_EQ(p[EVENT_TYPE_OFFSET], expected_type) << debug_string;
    ASSERT_EQ(uint4korr(p + SERVER_ID_OFFSET), 1) << debug_string;
    ASSERT_EQ(uint4korr(p + EVENT_LEN_OFFSET), expected_event_size)
        << debug_string;
    for (Size_t i = 0; i < expected_data_size; ++i)
      ASSERT_EQ(p[LOG_EVENT_HEADER_LEN + i], (int)expected_type)
          << debug_string;
  }

  /// Create a given number of events, using the specified sizes and
  /// the specified types.
  std::string mock_multiple_events(std::vector<Size_t> size_vector,
                                   std::vector<Log_event_type> type_vector) {
    std::stringstream ss;
    auto type_it = Cyclic_iterator(type_vector.begin(), type_vector.end());
    for (auto size : size_vector) {
      ss << mock_one_event(size, *type_it);
      ++type_it;
    }
    return ss.str();
  }

  std::string compress_event_buffer(std::string event_buffer) {
    Compressor_t compressor;
    compressor.feed(event_buffer.data(), event_buffer.size());
    Managed_Buffer_sequence_t managed_buffer_sequence;
    EXPECT_EQ(compressor.finish(managed_buffer_sequence),
              Compress_status_t::success);
    return managed_buffer_sequence.read_part().str();
  }

  /// Construct a sequence of events, compress it, decompress it, and
  /// verify that the result is as expected
  void test_one_scenario(std::vector<Size_t> size_vector) {
    std::string debug_string = std::string("seed=") + std::to_string(m_seed);
    auto event_buffer = mock_multiple_events(size_vector, m_type_vector);
    auto compressed_data = compress_event_buffer(event_buffer);

    Stream_t stream(compressed_data, type_code);
    int event_count = 0;
    Buffer_ptr_t buffer_ptr;
    auto type_it = Cyclic_iterator(m_type_vector.begin(), m_type_vector.end());
    auto size_it = size_vector.begin();
    while (stream >> buffer_ptr) {
      ASSERT_NE(size_it, size_vector.end()) << debug_string;
      auto type = *type_it;
      auto size = *size_it;
      check_one_event(
          concat(debug_string, " type_it=",
                 std::distance(&*m_type_vector.begin(), &*type_it), " size_it=",
                 std::to_string(std::distance(size_vector.begin(), size_it)),
                 " event_count=", event_count),
          buffer_ptr->str(), size, type);
      ASSERT_EQ(type, buffer_ptr->data()[EVENT_TYPE_OFFSET]) << debug_string;
      ++event_count;
      ++type_it;
      ++size_it;
    }
    ASSERT_EQ(stream.get_status(), Decompress_status_t::end)
        << debug_string << "Error: " << stream.get_error_str();
    ASSERT_EQ(stream.has_error(), false)
        << debug_string << "Error: " << stream.get_error_str();
  }

  /// Generate a vector of random length, where each element is a
  /// random integer.
  ///
  /// All random distributions are uniform.
  ///
  /// @param max_count Generate between 0 and this number of elements.
  ///
  /// @param max_value Generate an integer between 0 and this number
  /// and store in each element.
  ///
  /// @return integer vector
  template <class T>
  std::vector<T> generate_random_vector(int max_count, T max_value) {
    std::vector<T> ret;
    std::uniform_int_distribution<int> count_distribution(1, max_count);
    std::uniform_int_distribution<T> value_distribution(0, max_value);
    auto count = count_distribution(m_random_number_generator);
    std::generate_n(std::back_inserter(ret), count, [&] {
      return value_distribution(m_random_number_generator);
    });
    return ret;
  }

  /// Requirement:
  ///
  /// - Compressing and then decompressing should give back the
  ///   original data.
  void test_one_random_scenario(int max_event_count, Size_t max_event_size) {
    auto size_vector = generate_random_vector(max_event_count, max_event_size);
    test_one_scenario(size_vector);
  }

  /// Requirement:
  ///
  /// - Compressing and then decompressing should give back the
  ///   original data.
  ///
  /// We check this by producing random data multiple times.
  void test_multiple_random_scenarios(int scenario_count, int max_event_count,
                                      Size_t max_event_size) {
    for (int i = 0; i < scenario_count; ++i)
      test_one_random_scenario(max_event_count, max_event_size);
  }

  /// Requirement:
  ///
  /// - Payload_event_buffer_istream should fail gracefully and return
  ///   corrupted if it contains an event of type
  ///   TRANSACTION_PAYLOAD_EVENT.
  void test_payload_in_payload() {
    auto event_buffer = mock_multiple_events(
        {10, 20}, {QUERY_EVENT, TRANSACTION_PAYLOAD_EVENT});
    auto compressed_data = compress_event_buffer(event_buffer);
    Stream_t stream(compressed_data, type_code);
    Buffer_ptr_t buffer_ptr;
    stream >> buffer_ptr;
    ASSERT_TRUE((bool)stream);
    check_one_event("payload_in_payload", buffer_ptr->str(), 10, QUERY_EVENT);
    stream >> buffer_ptr;
    ASSERT_FALSE((bool)stream);
    ASSERT_EQ(stream.get_status(), Decompress_status_t::corrupted);
  }

  /// Requirement:
  ///
  /// - Payload_event_buffer_istream should fail gracefully and return
  ///   corrupted if the uncompressed data ends in the middle of an
  ///   event.
  void test_truncated_uncompressed_data() {
    auto event_buffer =
        mock_multiple_events({10, 20}, {QUERY_EVENT, QUERY_EVENT});
    // Test all ways to truncate the second event
    for (Size_t i = LOG_EVENT_HEADER_LEN + 10 + 1; i < event_buffer.size();
         ++i) {
      std::string debug_string =
          std::string("truncated_uncompressed i=") + std::to_string(i);
      auto compressed_data =
          compress_event_buffer(std::string(event_buffer.data(), i));
      Stream_t stream(compressed_data, type_code);
      Buffer_ptr_t buffer_ptr;

      stream >> buffer_ptr;
      ASSERT_TRUE((bool)stream) << debug_string;
      ASSERT_EQ(stream.get_status(), Decompress_status_t::success)
          << debug_string;
      check_one_event(debug_string, buffer_ptr->str(), 10, QUERY_EVENT);

      stream >> buffer_ptr;
      ASSERT_FALSE((bool)stream) << debug_string;
      ASSERT_EQ(stream.get_status(), Decompress_status_t::corrupted)
          << debug_string;
    }
  }

  /// Requirement:
  ///
  /// - Payload_event_buffer_istream should fail gracefully and return
  ///   corrupted if the compressed stream is truncated.
  void test_truncated_compressed_data() {
    // Just one event: if we use multiple events, it's hard to predict
    // which event will observe the error.
    auto event_buffer = mock_one_event(100, QUERY_EVENT);
    auto compressed_data = compress_event_buffer(event_buffer);
    // Test all ways to truncate the event
    for (Size_t i = 1; i < compressed_data.size(); ++i) {
      Stream_t stream(compressed_data.data(), i, type_code);
      Buffer_ptr_t buffer_ptr;
      stream >> buffer_ptr;
      EXPECT_FALSE((bool)stream) << i;
      EXPECT_EQ(stream.get_status(), Decompress_status_t::corrupted) << i;
    }
  }

  /// Requirement:
  ///
  /// - Payload_event_buffer_istream should fail gracefully and return
  ///   exceeds_max_capacity in case the output is bigger than the
  ///   configured max size.
  void test_exceeds_max_size() {
    auto event_buffer = mock_multiple_events({10, 100, 10}, {QUERY_EVENT});
    auto compressed_data = compress_event_buffer(event_buffer);
    Stream_t stream(compressed_data, type_code);
    {
      auto gc = stream.get_grow_calculator();
      gc.set_max_size(99);
      stream.set_grow_calculator(gc);
    }
    auto debug_func = [&] {
      return std::string("max_size=") +
             std::to_string(stream.get_grow_calculator().get_max_size());
    };
    Buffer_ptr_t buffer_ptr;
    // First event is within max size
    stream >> buffer_ptr;
    ASSERT_TRUE((bool)stream) << debug_func();
    check_one_event(debug_func(), buffer_ptr->str(), 10, QUERY_EVENT);
    // Second event exceeds max size
    stream >> buffer_ptr;
    ASSERT_FALSE((bool)stream) << debug_func();
    ASSERT_EQ(stream.get_status(), Decompress_status_t::exceeds_max_size)
        << debug_func();
    // Once we have failed, any subsequent read from the stream will
    // fail the same way, even if the max size has increased.
    {
      auto gc = stream.get_grow_calculator();
      gc.set_max_size(10000);
      stream.set_grow_calculator(gc);
    }
    stream >> buffer_ptr;
    ASSERT_FALSE((bool)stream) << debug_func();
    ASSERT_EQ(stream.get_status(), Decompress_status_t::exceeds_max_size)
        << debug_func();
  }

  /// Requirement:
  ///
  /// - Payload_event_buffer_istream should fail gracefully and return
  ///   out_of_memory in case allocation fails.
  void test_allocation_failure() {
    // Allocator that fails after `success_count_before_failure`
    // successful allocations.
    int allocation_failure_number = 0;
    int allocation_number = 0;
    auto failing_allocator = [&](Size_t n) -> Memory_resource_t::Ptr_t {
      ++allocation_number;
      if (allocation_number == allocation_failure_number) return nullptr;
      // "nolint": as a general rule, malloc should not be used, so
      // clang-tidy warns about it. But this is an allocator so it is
      // appropriate to use malloc and therefore we suppress the check.
      // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
      return std::malloc(n);
    };
    Memory_resource_t failing_memory_resource(failing_allocator, std::free);
    auto debug_func = [&] {
      return std::string("allocation_failure_number=") +
             std::to_string(allocation_failure_number) +
             " allocation_number=" + std::to_string(allocation_number);
    };

    std::vector<Size_t> size_vector({10, 500, 500, 500, 100000});
    auto event_buffer = mock_multiple_events(size_vector, {QUERY_EVENT});
    auto compressed_data = compress_event_buffer(event_buffer);

    // Repeat the test incrementing `allocation_failure_number` in
    // each iteration.  This forces the allocation_failure_number'th
    // lower-level allocation to fail.
    while (true) {
      Stream_t stream(compressed_data, type_code, 0, failing_memory_resource);
      Buffer_ptr_t buffer_ptr;
      auto size_it = size_vector.begin();
      int event_count = 0;
      ++allocation_failure_number;
      allocation_number = 0;
      while (stream >> buffer_ptr) {
        ASSERT_TRUE((bool)stream) << debug_func();
        ASSERT_EQ(stream.get_status(), Decompress_status_t::success)
            << debug_func();
        check_one_event(debug_func(), buffer_ptr->str(), *size_it, QUERY_EVENT);
        ++size_it;
        ++event_count;
      }
      ASSERT_FALSE((bool)stream) << debug_func();
      // Eventually allocation_failure_number is so big that the
      // lower-level functions don't observe any error.  Then we are
      // done with the test.
      if (stream.get_status() == Decompress_status_t::end) {
        ASSERT_EQ(size_it, size_vector.end()) << debug_func();
        if (m_trace)
          std::cout << "Got all " << event_count << " events with less than "
                    << allocation_failure_number << " allocations.\n";
        return;
      }
      // If there is an error, it should be out-of-memory or corrupted.
      ASSERT_TRUE((stream.get_status() == Decompress_status_t::out_of_memory) ||
                  (stream.get_status() == Decompress_status_t::corrupted))
          << debug_func();
      if (m_trace)
        std::cout << "Got " << event_count << " events before allocation "
                  << "number " << allocation_failure_number << " failed.\n";
      // Retrying doesn't help
      stream >> buffer_ptr;
      ASSERT_FALSE((bool)stream) << debug_func();
      ASSERT_TRUE((stream.get_status() == Decompress_status_t::out_of_memory) ||
                  (stream.get_status() == Decompress_status_t::corrupted))
          << debug_func();
    }
  }

  /// Requirement:
  ///
  /// - Payload_event_buffer_istream should return `corrupted` if the
  ///   input is not valid ZSTD-compressor output.
  void test_corrupted_compressed_data() {
    // By chance, "Hello world!" is not valid zstd compressor output.
    std::string compressed_data = "Hello world!";
    Stream_t stream(compressed_data, type_code);
    Buffer_ptr_t buffer_ptr;
    stream >> buffer_ptr;
    EXPECT_FALSE((bool)stream);
    EXPECT_EQ(stream.get_status(), Decompress_status_t::corrupted);
  }

  /// Requirement:
  ///
  /// - Payload_event_buffer_istream should work transparently even if
  ///   there are compression frame boundaries in the middle of the
  ///   compressed data.
  void test_frame_boundaries() {
    std::vector<Size_t> size_vector({5, 0, 5});
    auto event_buffer = mock_multiple_events(size_vector, {QUERY_EVENT});
    for (Size_t f1 = 0; f1 <= event_buffer.size(); ++f1) {
      auto frame_1 =
          compress_event_buffer(std::string(event_buffer.data(), f1));
      for (Size_t f2 = f1; f2 <= event_buffer.size(); ++f2) {
        auto debug_func = [&] {
          return std::string("f1=") + std::to_string(f1) + std::string(" f2=") +
                 std::to_string(f2);
        };
        auto frame_2 = compress_event_buffer(
            std::string(event_buffer.data() + f1, f2 - f1));
        auto frame_3 = compress_event_buffer(
            std::string(event_buffer.data() + f2, event_buffer.size() - f2));
        auto compressed_data = frame_1;
        compressed_data += frame_2;
        compressed_data += frame_3;
        Stream_t stream(compressed_data, type_code);
        Buffer_ptr_t buffer_ptr;
        stream >> buffer_ptr;
        EXPECT_TRUE((bool)stream) << debug_func();
        EXPECT_EQ(stream.get_status(), Decompress_status_t::success)
            << debug_func();
        stream >> buffer_ptr;
        EXPECT_TRUE((bool)stream) << debug_func();
        EXPECT_EQ(stream.get_status(), Decompress_status_t::success)
            << debug_func();
        stream >> buffer_ptr;
        EXPECT_TRUE((bool)stream) << debug_func();
        EXPECT_EQ(stream.get_status(), Decompress_status_t::success)
            << debug_func();
        stream >> buffer_ptr;
        EXPECT_FALSE((bool)stream) << debug_func();
        EXPECT_EQ(stream.get_status(), Decompress_status_t::end)
            << debug_func();
      }
    }
  }

  void test_api_assertions() {
    EXPECT_DEBUG_DEATH(
        {
          // Stream destructor should raise assertion if stream is in
          // an error state, but caller has not checked if the reason
          // that the stream ended is EOF or error.
          std::string compressed_data = "Hello world!";
          Stream_t stream(compressed_data, type_code);
          Buffer_ptr_t buffer_ptr;
          stream >> buffer_ptr;
          EXPECT_FALSE((bool)stream);
          EXPECT_TRUE(!stream);
        },
        "!m_outstanding_error");
    EXPECT_DEBUG_DEATH(
        {
          // Stream destructor should raise assertion if stream has
          // reached EOF, but caller has not checked if the reason
          // that the stream ended is EOF or error.
          auto event_buffer = mock_one_event(100, QUERY_EVENT);
          auto compressed_data = compress_event_buffer(event_buffer);
          Stream_t stream(compressed_data, type_code);
          Buffer_ptr_t buffer_ptr;
          stream >> buffer_ptr;
          EXPECT_TRUE((bool)stream);
          stream >> buffer_ptr;
          EXPECT_FALSE((bool)stream);
          EXPECT_TRUE(!stream);
        },
        "!m_outstanding_error");
    // Non-assertion cases
    auto event_buffer = mock_one_event(100, QUERY_EVENT);
    auto compressed_data = compress_event_buffer(event_buffer);
    {
      Stream_t stream(compressed_data, type_code);
      // No assertion in the initial state
    }
    {
      Stream_t stream(compressed_data, type_code);
      Buffer_ptr_t buffer_ptr;
      stream >> buffer_ptr;
      // No assertion if no error occurred
    }
    {
      Stream_t stream(compressed_data, type_code);
      Buffer_ptr_t buffer_ptr;
      stream >> buffer_ptr;
      stream >> buffer_ptr;
      stream.has_error();
      // No assertion if error was checked
    }
    {
      Stream_t stream(compressed_data, type_code);
      Buffer_ptr_t buffer_ptr;
      stream >> buffer_ptr;
      stream >> buffer_ptr;
      stream.get_error_str();
      // No assertion if error was checked
    }
    {
      Stream_t stream(compressed_data, type_code);
      Buffer_ptr_t buffer_ptr;
      stream >> buffer_ptr;
      stream >> buffer_ptr;
      stream.get_status();
      // No assertion if status was checked
    }
  }

  void test_error_cases() {
    test_payload_in_payload();
    test_truncated_uncompressed_data();
    test_truncated_compressed_data();
    test_exceeds_max_size();
    test_allocation_failure();
    if (type_code != mysql::binlog::event::compression::NONE)
      // "NONE" can't be corrupted
      test_corrupted_compressed_data();
    test_frame_boundaries();
    test_api_assertions();
  }
};

TEST(PayloadEventBufferStreamTest, NoneCompressDecompressTest) {
  // 50 trials, each time creating up to 20 events, each of size up to 65536.
  PayloadEventBufferStreamTest<mysql::binlog::event::compression::None_comp>()
      .test_multiple_random_scenarios(50, 20, 65536);
}

TEST(PayloadEventBufferStreamTest, ZstdCompressDecompressTest) {
  // 50 trials, each time creating up to 20 events, each of size up to 65536.
  PayloadEventBufferStreamTest<mysql::binlog::event::compression::Zstd_comp>()
      .test_multiple_random_scenarios(50, 20, 65536);
}

TEST(PayloadEventBufferStreamTest, NoneDecompressErrorTest) {
  PayloadEventBufferStreamTest<mysql::binlog::event::compression::None_comp>()
      .test_error_cases();
}

TEST(PayloadEventBufferStreamTest, ZstdDecompressErrorTest) {
  PayloadEventBufferStreamTest<mysql::binlog::event::compression::Zstd_comp>()
      .test_error_cases();
}

}  // namespace mysql::binlog::event::unittests
