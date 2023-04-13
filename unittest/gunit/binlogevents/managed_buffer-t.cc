/* Copyright (c) 2023, Oracle and/or its affiliates.

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

#include "libbinlogevents/include/buffer/managed_buffer.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <functional>  // std::function
#include <iterator>    // std::distance

#include "libbinlogevents/include/math/math.h"
#include "libbinlogevents/include/string/concat.h"

using mysqlns::string::concat;

namespace mysqlns::buffer::managed_buffer::unittest {

using Grow_calculator_t = mysqlns::buffer::Grow_calculator;
using Grow_status_t = mysqlns::buffer::Grow_status;
using Size_t = mysqlns::buffer::Buffer_view<>::Size_t;
using Debug_function = const std::function<std::string(std::string)>;
using Difference_t = mysqlns::buffer::Rw_buffer<>::Difference_t;

// Return the current file and line as a string delimited and ended by
// colons.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define FILELINE() concat(__FILE__, ":", __LINE__, ": ")

// Helper macros to make assertions output the debug info we need, and
// make the program stop with assertion.
static int n_assertions = 0;
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
// R1. When the existing or requested size is bigger than the max
//     size, an error shall be returned. Otherwise, the resize shall
//     succeed.
//
// R2. When the resize operation is successful, the read part shall
//     remain unchanged and the write part shall increase as needed.
//
// R3. When the resize operation is successful, and new space is
//     needed, one new buffer shall be allocated to accomodate the
//     remaining requested size.
//
// R4. The buffer sequence shall remain self-consistent after the
//     operation.
//
// * The operation of moving the position shall satisfy the following
//   requirement:
//
// R5. It shall be possible to move the position from A to B, for any
//     A, B between 0 and the size.
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
// - size
// - position
// - extra container capacity
// - requested size
// - max size
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
// - if size > max size, or requested size > max size,
//   exceeds_max_size is returned
// - else if requested size <= size, size is unchanged
// - else, new size is requested size
template <class Char_t>
class Grow_tester {
 public:
  using Managed_buffer_t = buffer::Managed_buffer<Char_t>;
  using Rw_buffer_t = buffer::Rw_buffer<Char_t>;

  int m_scenario_count = 0;

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
  // maximum sizes in the Grow_calculator, and by moving the position
  // to different places.  These quantities can be zero or nonzero,
  // and can coincide or be different.
  void combinatorial_grow_test() {
    std::vector<Size_t> sizes = {0, 1, 2, 10, 100, 1000, 10000};
    for (Size_t size : sizes) {
      for (Size_t position : sizes) {
        if (position <= size) {
          for (Size_t max_size : sizes) {
            for (Size_t requested_size : sizes) {
              for (Size_t requested_position : sizes) {
                grow_test_helper<0>(size, position, max_size, requested_size,
                                    requested_position);
                grow_test_helper<1>(size, position, max_size, requested_size,
                                    requested_position);
                grow_test_helper<2>(size, position, max_size, requested_size,
                                    requested_position);
                grow_test_helper<10>(size, position, max_size, requested_size,
                                     requested_position);
                grow_test_helper<100>(size, position, max_size, requested_size,
                                      requested_position);
                grow_test_helper<1000>(size, position, max_size, requested_size,
                                       requested_position);
                grow_test_helper<10000>(size, position, max_size,
                                        requested_size, requested_position);
              }
            }
          }
        }
      }
    }
    {
      // Corner cases: we exercise the code path where the destructor
      // is used while the buffer is null and verify that there is no
      // crash.  Tests above already checked that the sizes are 0.
      Managed_buffer_t mb0(0);
      Managed_buffer_t mb1(1);
      Preallocated_managed_buffer<Char_t, 0> pmb0;
      Preallocated_managed_buffer<Char_t, 1> pmb1;
    }

    // Uncomment for debugging.
    // std::cout << "scenarios: " << m_scenario_count
    //           << " assertions: " << n_assertions << "\n";
  }

  template <Size_t default_capacity>
  void grow_test_helper(Size_t capacity, Size_t position, Size_t max_capacity,
                        Size_t requested_capacity, Size_t requested_position) {
    // The 'if constexpr' is needed to silence some old/buggy
    // compilers that would otherwise warn about pointless comparison
    // when default_capacity is 0.
    if constexpr (default_capacity != 0) {
      if (default_capacity > capacity) return;
    }
    Size_t new_capacity = compute_new_size(capacity, default_capacity,
                                           max_capacity, requested_capacity);
    if (requested_position > new_capacity) return;

    ++m_scenario_count;
    Managed_buffer_t mb(default_capacity);
    grow_test(mb, capacity, position, default_capacity, max_capacity,
              requested_capacity, requested_position, 0);

    ++m_scenario_count;
    Preallocated_managed_buffer<Char_t, default_capacity> pmb;
    grow_test(pmb, capacity, position, default_capacity, max_capacity,
              requested_capacity, requested_position, default_capacity);
  }

  Size_t compute_new_size(Size_t capacity,
                          [[maybe_unused]] Size_t default_capacity,
                          Size_t max_capacity, Size_t requested_capacity) {
    // In the first two cases, either the existing capacity or the
    // requested capacity exceeds the maximum capacity configured in the
    // Grow_calculator. Therefore it refuses to grow and leaves the buffer
    // unchanged.  In the third case, the request is for a smaller
    // capacity than the existing capacity, so it succeeds and leaves the
    // buffer unchanged.
    if (capacity > max_capacity || requested_capacity > max_capacity ||
        requested_capacity <= capacity)
      return capacity;
    return requested_capacity;
  }

  void grow_test(Managed_buffer_t &mb, Size_t capacity, Size_t position,
                 Size_t default_capacity, Size_t max_capacity,
                 Size_t requested_capacity, Size_t requested_position,
                 Size_t initial_capacity) {
    // NOLINTBEGIN(cppcoreguidelines-macro-usage)
#define CHECK_SIZES(POSITION, CAPACITY) \
  check_sizes(FILELINE(), debug_output, mb, POSITION, CAPACITY)
    // NOLINTEND(cppcoreguidelines-macro-usage)

    // This does the following:
    //
    // 1. Create empty buffer
    // 2. Grow to the required total capacity
    // 3. Set the position as required
    // 4. Set Grow_calculator.max_size and grow to the requested capacity
    // 5. Move position to the requested new position

    Size_t new_capacity = compute_new_size(capacity, default_capacity,
                                           max_capacity, requested_capacity);

    // Prepare Grow_calculator with a high max size
    {
      Grow_calculator_t gc;
      gc.set_block_size(1);
      gc.set_grow_factor(1);
      gc.set_grow_increment(1);
      mb.set_grow_calculator(gc);
    }

    std::string fileline;
    Debug_function debug_output = [&](const std::string &fileline_arg) {
      // clang-format off
      return concat(
          fileline_arg,
          "\nposition=", position,
          ",\ncapacity=", capacity,
          ",\nmax_capacity=", max_capacity,
          ",\nrequested_capacity=", requested_capacity,
          ",\nrequested_position=", requested_position,
          ",\ndefault_capacity=", default_capacity,
          ",\nnew_capacity=", new_capacity,
          ",\ninitial_capacity=", initial_capacity);
      // clang-format on
    };
    auto work = [&](bool use_increase_position_api) {
      CHECK_SIZES(0, initial_capacity);
      // Set the initial capacity.
      {
        auto gc = mb.get_grow_calculator();
        gc.set_max_size(1000000);
        mb.set_grow_calculator(gc);
        auto status = mb.reserve_total_size(capacity);
        AEQ(status, Grow_status_t::success);
        CHECK_SIZES(0, capacity);
      }
      // Set the initial position.
      mb.set_position(position);
      CHECK_SIZES(position, capacity);
      // Grow to the requested capacity.
      {
        auto gc = mb.get_grow_calculator();
        gc.set_max_size(max_capacity);
        mb.set_grow_calculator(gc);
        auto status = mb.reserve_total_size(requested_capacity);
        auto expected_status = Grow_status_t::success;
        if (capacity > max_capacity || requested_capacity > max_capacity)
          expected_status = Grow_status_t::exceeds_max_size;
        AEQ(status, expected_status);
        CHECK_SIZES(position, new_capacity);
      }
      // Move to the requested position.  Use different methods to
      // move the position, just to exercise all the API entries.
      if (requested_position >= position && use_increase_position_api)
        mb.increase_position(requested_position - position);
      else
        mb.move_position(Difference_t(requested_position) -
                         Difference_t(position));
      CHECK_SIZES(requested_position, new_capacity);
      // Reset.
      mb.reset();
      // If we have allocated a default buffer, the initial capacity is
      // now the default capacity.
      if (new_capacity > 0 && capacity <= default_capacity)
        initial_capacity = default_capacity;
      CHECK_SIZES(0, initial_capacity);
    };

    // Run twice, to verify that everything works the same way also
    // after `reset`, and also to exercise different members that move
    // the position.
    work(false);
    work(true);
  }

  // Check that the sizes of the parts, and the number of buffers per
  // part, is as expected.
  void check_sizes(const std::string &fileline, Debug_function &debug_prefix,
                   Managed_buffer_t &mb, Size_t position, Size_t capacity) {
    auto w_size = capacity - position;

    auto debug_output = [&](std::string fileline_arg) {
      // clang-format off
      return concat(debug_prefix(std::move(fileline_arg)),
                    ", position=", position,
                    ", capacity=", capacity,
                    ", w_size=", w_size);
      // clang-format on
    };

    auto &r = mb.read_part();
    auto &w = mb.write_part();

    AEQ(r.size(), position);
    AEQ(w.size(), w_size);
    AEQ(mb.capacity(), capacity);

    AEQ(std::distance(r.begin(), r.end()), position);
    AEQ(std::distance(w.begin(), w.end()), w_size);
  }
};

TEST(ManagedBufferTest, CombinatorialGrowTestChar) {
  Grow_tester<char>().combinatorial_grow_test();
}

TEST(ManagedBufferTest, CombinatorialGrowTestUchar) {
  Grow_tester<unsigned char>().combinatorial_grow_test();
}

}  // namespace mysqlns::buffer::managed_buffer::unittest
