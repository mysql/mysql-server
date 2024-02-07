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

#include "mysql/binlog/event/compression/buffer/managed_buffer_sequence.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <functional>  // std::function
#include <iterator>    // std::distance

#include "mysql/binlog/event/math/math.h"
#include "mysql/binlog/event/string/concat.h"

using mysql::binlog::event::string::concat;

namespace mysql::binlog::event::compression::buffer {
namespace managed_buffer_sequence::unittest {

/// "Token" used for the template argument for Accessor.  In case
/// another unittest needs an Accessor too, the use of different
/// tokens distinguishes them.
class Grow_test_access_token {};

template <>
class Accessor<mysql::binlog::event::compression::buffer::
                   managed_buffer_sequence::unittest::Grow_test_access_token> {
 public:
  /// Export Grow_buffer_sequence::get_boundaries
  template <class MBS, class BS>
  static std::tuple<typename MBS::Iterator_t, typename MBS::Iterator_t,
                    typename MBS::Size_t>
  get_boundaries(BS &bs) {
    return MBS::get_boundaries(bs);
  }

  /// Export Grow_buffer_sequence::m_buffers
  template <class MBS>
  static typename MBS::Container_t &m_buffers(MBS &mbs) {
    return mbs.m_buffers;
  }
};

using Access =
    Accessor<mysql::binlog::event::compression::buffer::
                 managed_buffer_sequence::unittest::Grow_test_access_token>;

using Grow_status_t = mysql::binlog::event::compression::buffer::Grow_status;
using Size_t = mysql::binlog::event::compression::buffer::Buffer_view<>::Size_t;
using Debug_function = const std::function<std::string(std::string)>;
using Difference_t = mysql::binlog::event::compression::buffer::
    Rw_buffer_sequence<>::Difference_t;

// Return the current file and line as a string delimited and ended by
// colons.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define FILELINE() concat(__FILE__, ":", __LINE__, ": ")

// Helper macros to make assertions output the debug info we need, and
// make the program stop with assertion.
[[maybe_unused]] static int n_assertions = 0;
static bool _shall_stop_after_assertion = false;
// NOLINTBEGIN(cppcoreguidelines-macro-usage)
#define ASSERTION_TAIL                                                   \
  << debug_output(fileline) << (_shall_stop_after_assertion = true, ""), \
      assert(!_shall_stop_after_assertion)
#define AEQ(v1, v2)                   \
  do {                                \
    ASSERT_EQ(v1, v2) ASSERTION_TAIL; \
    ++n_assertions;                   \
  } while (0)
#define ANE(v1, v2)                   \
  do {                                \
    ASSERT_NE(v1, v2) ASSERTION_TAIL; \
    ++n_assertions;                   \
  } while (0)
// NOLINTEND(cppcoreguidelines-macro-usage)

// Requirements:
//
// * Resize operations shall satisfy the following requirements:
//
// R1. When the existing or requested capacity is bigger than the max
//     capacity, an error shall be returned. Otherwise, the resize shall
//     succeed.
//
// R2. When the resize operation is successful, the read part shall
//     remain unchanged and the write part shall increase as needed.
//
// R3. When the resize operation is successful, and new space is
//     needed, one new buffer shall be allocated to accomodate the
//     remaining requested capacity.
//
// R4. The buffer sequence shall remain self-consistent after the
//     operation.
//
// * The operation of moving the position shall satisfy the following
//   requirement:
//
// R5. It shall be possible to move the position from A to B, for any
//     A, B between 0 and the capacity.
//
// * The reset operation shall satisfy the following requirements:
//
// R7. The reset operation shall set the read position to 0, leave the
//     specified amount of buffers in the write part, and leave the
//     specified extra capacity in the vector
//
// The exact execution of the resize operation and the operation of
// moving the position depends on the relative order of the following
// quantities:
//
// - capacity
// - position
// - extra container capacity
// - requested capacity
// - max capacity
// - requested new position
//
// The behavior may also depend on whether some of these are 0 or not,
// and whether some are equal or not.
//
// We test all relative orders among these variables, including
// boundary cases where some of them are equal, and including cases
// where the smallest variable is 0 as well as nonzero.  Therefore
// we vary all these quantities between 0 and 4, inclusive.
//
// We expect the following outcome:
// - if capacity > max capacity, or requested capacity > max capacity,
//   exceeds_max_size is returned
// - else if requested capacity <= capacity, capacity is unchanged
// - else, new capacity is requested capacity
template <class Char_t, template <class E, class A> class Container_t>
class Grow_tester {
 public:
  using Managed_buffer_sequence_t =
      buffer::Managed_buffer_sequence<Char_t, Container_t>;
  using Rw_buffer_sequence_t = buffer::Rw_buffer_sequence<Char_t, Container_t>;

  // Execute "grow_test" in many configurations.
  //
  // The goal is to exercise as many potential corner cases as
  // possible related to growing the buffer sequence or moving the
  // position.
  //
  // In each scenario, we will setup the buffer in an "initial state",
  // verify that it is as we expect, attempt to grow it, verify that
  // it worked or didn't work according to expectation, attempt to
  // move the position, verify that it worked, reset, verify that it
  // worked.
  //
  // We vary the scenarios by using different initial states, by using
  // different requestes for more capacity, by setting different
  // maximum sizes in the Grow_calculator, and by moving the position to
  // different places.
  //
  // In general, the state of a managed buffer sequence is described
  // by the size of the read part, the total capacity of the read+write
  // parts, and the number of extra null buffers.  To test many types
  // of initial states, we should exercise cases where the position
  // and the capacity are 0, or on a buffer boundary, or in the middle of
  // a buffer; when they coincide, or are both within the same buffer,
  // or are farther apart; when there is no extra capacity in the
  // container, when there is one extra buffer, or when there are two
  // extra buffers.
  //
  // We use up to 3 buffers, and vary all the positions from 0 up to
  // the end of the last buffer, in steps of 1/N buffer, where
  // N=parts_per_buffer.
  //
  // We use a Grow_calculator that gives us exactly as many bytes as we
  // ask for.
  void combinatorial_grow_test() {
    [[maybe_unused]] static int n_scenarios = 0;
    for (Size_t parts_per_buffer : {1, 4}) {
      for (Size_t part_size : {1, 100}) {
        Size_t buffer_size = parts_per_buffer * part_size;
        for (Size_t extra_container_capacity : {0, 1, 2}) {
          for (Size_t capacity = 0; capacity <= buffer_size * 3;
               capacity += buffer_size) {
            std::set<Size_t> requests;
            requests.insert(0);
            if (capacity > part_size) requests.insert(capacity - part_size);
            requests.insert(capacity);
            requests.insert(capacity + part_size);
            if (parts_per_buffer >= 2)
              requests.insert(capacity + 2 * part_size);
            for (Size_t position = 0; position <= capacity;
                 position += part_size) {
              for (Size_t max_capacity : requests) {
                for (Size_t requested_capacity : requests) {
                  auto new_size = compute_new_size(capacity, max_capacity,
                                                   requested_capacity);
                  for (Size_t requested_position = 0;
                       requested_position <= new_size;
                       requested_position += part_size) {
                    ++n_scenarios;
                    grow_test(part_size, parts_per_buffer,
                              extra_container_capacity, position, capacity,
                              max_capacity, requested_capacity,
                              requested_position);
                  }
                }
              }
            }
          }
        }
      }
    }
    // Uncomment for debugging.
    // std::cout << "scenarios: " << n_scenarios
    //           << " assertions: " << n_assertions
    //           << "\n";
  }

  Size_t compute_new_size(Size_t capacity, Size_t max_capacity,
                          Size_t requested_capacity) {
    // In the first two cases, either the existing size or the
    // requested size exceeds the maximum size configured in the
    // Grow_calculator. Therefore it refuses to grow and leaves the buffer
    // unchanged.  In the third case, the request is for a smaller
    // size than the existing size, so it succeeds and leaves the
    // buffer sequence unchanged.
    if (capacity > max_capacity || requested_capacity > max_capacity ||
        requested_capacity <= capacity)
      return capacity;
    return requested_capacity;
  }

  void grow_test(Size_t part_size, Size_t parts_per_buffer,
                 Size_t extra_container_capacity, Size_t position,
                 Size_t capacity, Size_t max_capacity,
                 Size_t requested_capacity, Size_t requested_position) {
    // NOLINTBEGIN(cppcoreguidelines-macro-usage)
#define CHECK_SIZES(POSITION, CAPACITY) \
  check_sizes(FILELINE(), debug_output, mbs, buffer_size, POSITION, CAPACITY)
    // NOLINTEND(cppcoreguidelines-macro-usage)

    // This does the following:
    //
    // 1. Create empty buffer sequence
    // 2. Allocate the required container capacity
    // 3. Reset to the required used buffer count
    // 4. Grow to the required total size
    // 5. Set the position as required
    // 6. Set Grow_calculator.max_size and grow to the requested size
    // 7. Move position to the requested new position

    Size_t buffer_size = part_size * parts_per_buffer;
    Size_t new_capacity =
        compute_new_size(capacity, max_capacity, requested_capacity);

    // **** Empty buffer ****

    // Prepare initial buffer sequence with given default size and old size
    Managed_buffer_sequence_t mbs;

    auto work = [&](bool use_increase_position_api) {
      // Prepare grow Grow_calculator with a high max size
      {
        auto gc = mbs.get_grow_calculator();
        gc.set_max_size(1000000);
        gc.set_block_size(1);
        gc.set_grow_factor(1);
        gc.set_grow_increment(1);
        mbs.set_grow_calculator(gc);
      }

      std::string fileline;
      Debug_function debug_output = [&](std::string fileline_arg) {
        // clang-format off
        return concat(
            fileline_arg,
            "\npart_size=", part_size,
            ",\nparts_per_buffer=", parts_per_buffer,
            ",\nextra_container_capacity=", extra_container_capacity,
            ",\nposition=", position,
            ",\ncapacity=", capacity,
            ",\nmax_capacity=", max_capacity,
            ",\nrequested_capacity=", requested_capacity,
            ",\nrequested_position=", requested_position,
            ",\nnew_capacity=", new_capacity,
            ",\nmbs=", mbs.debug_string());
        // clang-format on
      };

      CHECK_SIZES(0, 0);

      // **** allocate vector capacity ****

      // Allocate the buffers we need
      Size_t accumulated_size = 0;
      while (mbs.capacity() <
             capacity + buffer_size * extra_container_capacity) {
        // Grow
        accumulated_size += buffer_size;
        auto status = mbs.reserve_write_size(accumulated_size);
        AEQ(status, Grow_status::success);
        CHECK_SIZES(0, accumulated_size);
      }
      Size_t rw_count = capacity / buffer_size;

      mbs.reset(rw_count, rw_count + 1 + extra_container_capacity);

      CHECK_SIZES(0, capacity);

      // **** move position ****

      mbs.set_position(position);
      CHECK_SIZES(position, capacity);

      // **** grow ****
      {
        auto gc = mbs.get_grow_calculator();
        gc.set_max_size(max_capacity);
        mbs.set_grow_calculator(gc);

        auto status = mbs.reserve_total_size(requested_capacity);

        auto expected_status = Grow_status_t::success;
        if (capacity > max_capacity || requested_capacity > max_capacity)
          expected_status = Grow_status_t::exceeds_max_size;
        AEQ(status, expected_status);

        CHECK_SIZES(position, new_capacity);
      }

      // *** move position ****

      // Use different methods to move the position, just to exercise
      // all the API entries.
      if (requested_position >= position && use_increase_position_api)
        mbs.increase_position(requested_position - position);
      else
        mbs.move_position(Difference_t(requested_position) -
                          Difference_t(position));

      CHECK_SIZES(requested_position, new_capacity);

      // **** reset ****
      mbs.reset();

      Size_t first_buffer_size = std::min(buffer_size, new_capacity);
      CHECK_SIZES(0, first_buffer_size);

      mbs.reset(0, 0);
      CHECK_SIZES(0, 0);
    };

    // Execute twice, just to verify that the sequence is still
    // functional after `reset()`, and also to exercise all API
    // entries for moving the position.
    work(false);
    work(true);
  }

  // Check that the sizes of the parts, and the number of buffers per
  // part, is as expected.
  void check_sizes(const std::string &fileline, Debug_function &debug_prefix,
                   Managed_buffer_sequence_t &mbs, Size_t buffer_size,
                   Size_t position, Size_t capacity) {
    // size of each part
    auto w_size = capacity - position;

    // offset of each part boundary from beginning of container
    auto end_distance = [&](Size_t s) {
      return mysql::binlog::event::math::ceil_div(s, buffer_size);
    };
    auto begin_distance = [&](Size_t s) {
      if (s == capacity)
        return mysql::binlog::event::math::ceil_div(s, buffer_size);
      return s / buffer_size;
    };
    auto r_end = end_distance(position);
    auto w_begin = 1 + begin_distance(position);
    auto w_end = 1 + end_distance(capacity);

    auto debug_output = [&](std::string fileline_arg) {
      // clang-format off
      return concat(debug_prefix(std::move(fileline_arg)),
                    ", position=", position,
                    ", w_size=", w_size,
                    ", r_end=", r_end,
                    ", w_begin=", w_begin,
                    ", w_end=", w_end);
      // clang-format on
    };
    check_self_consistent(fileline, debug_output, mbs);

    auto &r = mbs.read_part();
    auto &w = mbs.write_part();

    AEQ(r.size(), position);
    AEQ(w.size(), w_size);
    AEQ(mbs.capacity(), capacity);

    AEQ(std::distance(r.begin(), r.end()), r_end);
    AEQ(std::distance(r.begin(), w.begin()), w_begin);
    AEQ(std::distance(r.begin(), w.end()), w_end);
  }

  // Check that the Managed_buffer_sequence is self-consistent.
  //
  // Requirements:
  //
  // R1. The sum of buffer sizes should equal the result from size().
  //     This should hold for the read part, for the write part, and
  //     for the managed buffer sequence as a whole.
  //
  // R2. No buffer in the read part or in the write part should be a
  //     null buffer. All buffers after the write part should be null
  //     buffers.  Any buffer between the read and write part should
  //     be a null buffer.
  //
  // R3. If the position splits a buffer, the second half should begin
  //     where the first half ends.
  //
  // R4. If the position does not split a buffer, there should be a
  //     null buffer between the read part and the write part.
  void check_self_consistent(const std::string &fileline,
                             Debug_function &debug_output,
                             Managed_buffer_sequence_t &mbs) {
    using Iterator_t = typename Managed_buffer_sequence_t::Iterator_t;
    using Size_t = typename Managed_buffer_sequence_t::Size_t;

    auto [read_begin, read_end, read_size] =
        Access::get_boundaries<Managed_buffer_sequence_t>(mbs.read_part());
    auto [write_begin, write_end, write_size] =
        Access::get_boundaries<Managed_buffer_sequence_t>(mbs.write_part());

    // R1. reported size matching actual size
    auto get_size = [&](Iterator_t begin, Iterator_t end) {
      Size_t size{0};
      for (auto it = begin; it != end; ++it) size += it->size();
      return size;
    };
    AEQ(get_size(read_begin, read_end), read_size);
    AEQ(get_size(write_begin, write_end), write_size);
    AEQ(get_size(read_begin, write_end), mbs.capacity());

    // R2. null / not-null buffers where expected
    auto check_no_null_buffer = [&](Iterator_t begin, Iterator_t end) {
      for (auto it = begin; it != end; ++it) {
        ANE(it->data(), nullptr);
        ANE(it->size(), 0);
      }
    };
    auto check_null_buffer = [&](Iterator_t begin, Iterator_t end) {
      for (auto it = begin; it != end; ++it) {
        AEQ(it->data(), nullptr);
        AEQ(it->size(), 0);
      }
    };
    check_no_null_buffer(read_begin, read_end);
    check_null_buffer(read_end, write_begin);
    check_no_null_buffer(write_begin, write_end);
    check_null_buffer(write_end, Access::m_buffers(mbs).end());

    // boundary between read and write part
    auto before_write = std::next(write_begin, -1);
    if (read_end == write_begin)
      // R3. split buffer: right half should begin where left half ends
      AEQ(before_write->end(), write_begin->begin());
    else
      // R4. no split buffer: should be exactly one buffer between
      AEQ(before_write, read_end);
  }
};

TEST(ManagedBufferSequenceTest, CombinatorialGrowTestCharVector) {
  Grow_tester<char, std::vector>().combinatorial_grow_test();
}

TEST(ManagedBufferSequenceTest, CombinatorialGrowTestUcharVector) {
  Grow_tester<unsigned char, std::vector>().combinatorial_grow_test();
}

TEST(ManagedBufferSequenceTest, CombinatorialGrowTestCharList) {
  Grow_tester<char, std::list>().combinatorial_grow_test();
}

TEST(ManagedBufferSequenceTest, CombinatorialGrowTestUcharList) {
  Grow_tester<unsigned char, std::list>().combinatorial_grow_test();
}

}  // namespace managed_buffer_sequence::unittest
}  // namespace mysql::binlog::event::compression::buffer
