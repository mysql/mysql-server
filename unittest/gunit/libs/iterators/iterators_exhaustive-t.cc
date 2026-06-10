// Copyright (c) 2024, 2026, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

#include <gtest/gtest.h>                          // TEST
#include <source_location>                        // source_location
#include "mysql/debugging/my_scoped_trace.h"      // MY_SCOPED_TRACE
#include "mysql/debugging/unittest_assertions.h"  // test_cmp
#include "mysql/iterators/iterator_interface.h"   // Iterator_interface
#include "mysql/iterators/meta.h"                 // Iterator_concept_tag

namespace {

// ==== Requirements ====
//
// This test checks requirements for iterators defined using Iterator_interface.
// The requirements apply to a matrix of iterator types, defined by the
// following axes:
//
// A1. The requirements apply to all iterator categories:
//
//     - Input iterators.
//
//     - Forward iterators.
//
//     - Bidirectional iterators.
//
//     - Random_access iterators.
//
//     - Contiguous iterators.
//
// A2. The requirements apply to:
//
//     - Iterators that return references.
//
//     - Iterators that return values. Except: contiguous iterators are required
//       to return references and not values.
//
// A3. The requirements apply to:
//
//     - Iterators without sentinels.
//
//     - Iterators with sentinels. Except, input iterators are not required to
//       be comparable, so there is no requirement for input iterators with
//       sentinels.
//
// In all the combinations of cases listed above, the following requirements
// apply:
//
// R1. When the derived class does not define an iterator category, the base
//     class must deduce the correct iterator category correctly. This is a
//     compile-time requirement.
//
// R2. When the derived class defines an iterator category, the base class must
//     respect that. This is a compile-time requirement.
//
// R3. The iterator must satisfy the standard library requirements for the
//     deduced/defined iterator category: std::bidirectional_iterator<It> etc.
//     This is a compile-time requirement.
//
// R4. If the iterator has a sentinel, it must satisfy the standard library
//     requirement std::sentinel_for<iterators::Default_sentinel, It>.
//     Otherwise, it must not satisfy the requiremnt. This is a compile-time
//     requirement.
//
// R5. All the operators for the deduced/defined category must work as expected.
//     In particular:
//
//     - input iterators: prefix ++, postfix ++, *, and ->.
//
//     - forward iterators: == and != with operator types. If the iterator has a
//       sentinel type, also with one operand an iterator and the other a
//       sentinel.
//
//     - bidirectional iterators: prefix --, postfix --.
//
//     - random_access iterators and contiguous iterators: +, -, += and -= with
//       right-hand-side of integer type. + with left-hand-side of integer type.
//       All of -, <=, <, >, >=, and <=> with both operands of iterator type. If
//       the iterator has a sentinel type, also the latter set of operators with
//       one operand an iterator and the other a sentinel. These are run-time
//       requirements.

using namespace mysql;

// ==== 1. Test all requirements for a specific iterator ====

// NOLINTNEXTLINE(performance-enum-size): silence clang-tidy's pointless hint
enum class Has_sentinel { no, yes };

/// Check all requirements for the given Iterator_t.
///
/// @tparam Iterator_t Iterator class to check
///
/// @tparam Iterator_category_t Iterator category that we expect Iterator_t to
/// satisfy.
///
/// @tparam has_sentinel If yes, expect that the iterator has a sentinel type.
template <class Iterator_t, class Iterator_concept_t, class Iterator_category_t,
          Has_sentinel has_sentinel>
class Iterator_checker {
// Define static constexpr bool member variables for each category, which are
// true if we expect the iterator to satisfy the requirements for that
// category.
#define DEFINE_CATEGORY_AND_CONCEPT(LEVEL)                               \
  static constexpr bool LEVEL##_category =                               \
      std::derived_from<Iterator_category_t, std::LEVEL##_iterator_tag>; \
  static constexpr bool LEVEL##_concept =                                \
      std::derived_from<Iterator_concept_t, std::LEVEL##_iterator_tag>
  DEFINE_CATEGORY_AND_CONCEPT(input);
  DEFINE_CATEGORY_AND_CONCEPT(forward);
  DEFINE_CATEGORY_AND_CONCEPT(bidirectional);
  DEFINE_CATEGORY_AND_CONCEPT(random_access);
  DEFINE_CATEGORY_AND_CONCEPT(contiguous);

 public:
  // Check the requirements in the constructor.
  Iterator_checker() { check(); }

  // Statically assert that the iterator satisfies standard library requirements
  // for the given category, and runtime-assert that the operators defined for
  // the iterator category behave as expected.
  void check() {
    MY_SCOPED_TRACE(typeid(Iterator_t).name());
    MY_SCOPED_TRACE(typeid(Iterator_category_t).name());
    MY_SCOPED_TRACE(has_sentinel == Has_sentinel::yes ? "sentinel"
                                                      : "no sentinel");

    if constexpr (!std::same_as<Iterator_category_t, void>) {
      static_assert(std::derived_from<Iterator_concept_t, Iterator_category_t>);
    }
    static_assert(std::same_as<iterators::Iterator_concept_tag<Iterator_t>,
                               Iterator_concept_t>);

    if constexpr (has_sentinel == Has_sentinel::yes) {
      static_assert(std::sentinel_for<iterators::Default_sentinel, Iterator_t>);
    } else {
      static_assert(
          !std::sentinel_for<iterators::Default_sentinel, Iterator_t>);
    }

    static_assert(std::is_nothrow_default_constructible_v<Iterator_t>);
    static_assert(std::is_nothrow_move_constructible_v<Iterator_t>);
    static_assert(std::is_nothrow_move_assignable_v<Iterator_t>);

    // weakest possible iterator requirement must be satisfied
    static_assert(input_concept);

    if constexpr (input_concept) {
      // Requirements:
      // - operator++ must advance the position.
      // - operator* must return the correct value (checked in
      //   `assert_iterators`).
      Iterator_t it1;
      Iterator_t it2;
      assert_iterators(it1, 0, it2, 0);

      ++it1;
      assert_iterators(it1, 1, it2, 0);
      it2++;
      assert_iterators(it1, 1, it2, 1);
      static_assert(std::same_as<decltype(++it1), Iterator_t &>);

      if constexpr (std::copy_constructible<Iterator_t>) {
        static_assert(std::same_as<decltype(it1++), Iterator_t>);
        static_assert(std::is_copy_assignable_v<Iterator_t>);
        static_assert(std::copyable<Iterator_t>);
      } else {
        // If category was deduced to input, it's because there is no copy
        // constructor or no copy assignment. Then also the postfix increment
        // returns void.
        static_assert(std::same_as<decltype(it1++), void>);
        static_assert(!std::is_copy_assignable_v<Iterator_t>);
        static_assert(!std::copyable<Iterator_t>);
      }
    }

    if constexpr (input_category) {
      // Requirements, besides those checked above:
      // - operator== and operator!= are defined and return the correct values
      //   (the values are tested in calls to assert_iterators above).
      static_assert(input_concept);
      static_assert(std::equality_comparable<Iterator_t>);
    }

    if constexpr (forward_concept) {
      static_assert(input_category);
      // Requirements, besides those checked above:
      // - operator++ must return the expected value.
      Iterator_t it1;
      Iterator_t it2;
      assert_iterators(it1, 0, it2, 0);

      auto &it3 = ++it1;
      assert_iterators(it1, 1, it2, 0);
      auto it4 = it2++;
      assert_iterators(it1, 1, it2, 1);
      assert_iterators(it3, 1, it4, 0);
      static_assert(std::same_as<decltype(it1++), Iterator_t>);
      static_assert(std::copy_constructible<Iterator_t>);
      static_assert(std::is_copy_assignable_v<Iterator_t>);
      static_assert(std::copyable<Iterator_t>);
    }

    if constexpr (forward_category) {
      static_assert(forward_concept);
      static_assert(std::is_reference_v<decltype(*Iterator_t{})>);
    }

    if constexpr (bidirectional_concept) {
      static_assert(forward_concept);
      // Requirements, besides those checked above:
      // - operator-- must advance the position, and return the expected value.
      Iterator_t it1;
      Iterator_t it2;
      ++it1;
      ++it2;
      assert_iterators(it1, 1, it2, 1);

      auto &it3 = --it1;
      assert_iterators(it1, 0, it2, 1);
      auto it4 = it2--;
      assert_iterators(it1, 0, it2, 0);
      assert_iterators(it3, 0, it4, 1);
    }

    if constexpr (bidirectional_category) {
      // Like bidirectional_category, and also requires by-reference
      static_assert(bidirectional_concept);
      static_assert(forward_category);
    }

    if constexpr (random_access_concept) {
      // Requirements, besides those checked above:
      // - operator+, operator-, operator+=, operator-= with numeric right hand
      //   side must return the expected iterator, and the latter two must
      //   advance the position.
      // - operator- with iterator operand must return the expected value
      //   (checked in `assert_iterators`).
      // - operator[] must return the expected value (checked in
      //   `assert_iterators`).
      // - operators <, <=, >, >=, <=> must return the expected result
      //   (checked in `assert_iterators`).
      Iterator_t it1;
      Iterator_t it2;
      assert_iterators(it1, 0, it2, 0);

      auto it3 = (it1 += 2);
      assert_iterators(it1, 2, it2, 0);
      it2 = (it2 + 5);
      assert_iterators(it1, 2, it2, 5);
      it1 = (3 + it1);
      assert_iterators(it1, 5, it2, 5);
      auto it4 = (it1 -= 2);
      assert_iterators(it1, 3, it2, 5);
      it2 = (it2 - 2);
      assert_iterators(it1, 3, it2, 3);
      assert_iterators(it3, 2, it4, 3);
    }

    if constexpr (random_access_category) {
      // Like random_access_category, and also requires by-reference
      static_assert(random_access_concept);
      static_assert(bidirectional_category);
    }

    if constexpr (contiguous_concept) {
      static_assert(random_access_concept);
      static_assert(random_access_category);
    }

    if constexpr (contiguous_category) {
      static_assert(contiguous_concept);
    }

    if constexpr (has_sentinel == Has_sentinel::yes) {
      // Requirements for sentinel:
      // - compares different from sentinel when not at end
      // - compares equal to sentinel when at end
      Iterator_t it;
      for (int i = 0; i < 10; ++i) {
        assert_sentinel(it, i);
        ++it;
      }
      assert_sentinel(it, 10);
    }
  }

 private:
  /// Exercise all the operators whose result type is not an iterator, and
  /// check that they evaluate to the values we expect.
  ///
  /// @param it1 First iterator.
  /// @param v1 Expected value of first iterator.
  /// @param it2 Second iterator.
  /// @param v2 Expected value of second iterator.
  void assert_iterators(const Iterator_t &it1, int v1, const Iterator_t &it2,
                        int v2,
                        [[maybe_unused]] std::source_location source_location =
                            std::source_location::current()) const {
    MY_SCOPED_TRACE(source_location);
    assert_iterators_one_way(it1, v1, it2, v2);
    // NOLINTNEXTLINE(readability-suspicious-call-argument): args are in order
    assert_iterators_one_way(it2, v2, it1, v1);
  }

  /// Exercise all the operators whose result type is not an iterator. Assert
  /// that they evaluate to the values we expect.
  ///
  /// This function only asserts unary operators for it1, and binary operators
  /// with it1 on the LHS; call it twice with reversed parameters for a complete
  /// check.
  ///
  /// @param it1 First iterator.
  /// @param v1 Expected value of first iterator.
  /// @param it2 Second iterator.
  /// @param v2 Expected value of second iterator.
  void assert_iterators_one_way(const Iterator_t &it1, int v1,
                                const Iterator_t &it2, int v2) const {
    ASSERT_EQ((*it1).m_value, v1);
    ASSERT_EQ(it1->m_value, v1);
    if constexpr (input_category) {
      debugging::test_eq_one_way(it1, it2, v1 == v2);
    }
    if constexpr (random_access_concept) {
      debugging::test_cmp_one_way(it1, it2, v1 <=> v2);
      ASSERT_EQ(it1 - it2, v1 - v2);
      ASSERT_EQ(it1[0].m_value, v1);
      ASSERT_EQ(it1[1].m_value, v1 + 1);
      ASSERT_EQ(it1[-1].m_value, v1 - 1);
    }
  }

  /// Exercise all the operators that accept a sentinel operand, and whose
  /// result type is not an iterator. Assert that they evaluate to the values
  /// we expect.
  ///
  /// @param it The iterator.
  /// @param v Expected value of the iterator.
  void assert_sentinel(Iterator_t &it, int v,
                       [[maybe_unused]] std::source_location source_location =
                           std::source_location::current()) {
    MY_SCOPED_TRACE(source_location);
    const auto &s = iterators::default_sentinel;
    debugging::test_eq(it, s, v == 10);
    if constexpr (random_access_concept) {
      debugging::test_cmp(it, s, v <=> 10);
      ASSERT_EQ(it - s, v - 10);
      ASSERT_EQ(s - it, 10 - v);
    }
  }
};

// ==== 2. Test with and without sentinel ====

/// Invoke Wrapped_iterator_checker for an iterator specialized first without
/// sentinel, and then with sentinel.
template <template <Has_sentinel> class Iterator_t, class Iterator_concept,
          class Iterator_category>
class With_and_without_sentinel_checker {
  Iterator_checker<Iterator_t<Has_sentinel::no>, Iterator_concept,
                   Iterator_category, Has_sentinel::no>
      without_sentinel;
  Iterator_checker<Iterator_t<Has_sentinel::yes>, Iterator_concept,
                   Iterator_category, Has_sentinel::yes>
      with_sentinel;
};

// ==== 3. Specialize the checker to each iterator concept and category ====

/// Check the requirements for an iterator satisfying input iterator concept but
/// not more.
template <template <Has_sentinel> class Iterator_t>
using Input_iterator_concept_checker =
    With_and_without_sentinel_checker<Iterator_t, std::input_iterator_tag,
                                      void /*not a legacy input iterator*/>;

/// Check the requirements for an iterator satisfying input iterator category
/// but not more.
template <template <Has_sentinel> class Iterator_t>
using Input_iterator_category_checker =
    With_and_without_sentinel_checker<Iterator_t, std::input_iterator_tag,
                                      std::input_iterator_tag>;

/// Check the requirements for an iterator satisfying forward iterator concept
/// but not more.
template <template <Has_sentinel> class Iterator_t>
using Forward_iterator_concept_checker =
    With_and_without_sentinel_checker<Iterator_t, std::forward_iterator_tag,
                                      std::input_iterator_tag>;

/// Check the requirements for an iterator satisfying forward iterator category
/// but not more.
template <template <Has_sentinel> class Iterator_t>
using Forward_iterator_category_checker =
    With_and_without_sentinel_checker<Iterator_t, std::forward_iterator_tag,
                                      std::forward_iterator_tag>;

/// Check the requirements for an iterator satisfying bidirectional iterator
/// concept but not more.
template <template <Has_sentinel> class Iterator_t>
using Bidirectional_iterator_concept_checker =
    With_and_without_sentinel_checker<
        Iterator_t, std::bidirectional_iterator_tag, std::input_iterator_tag>;

/// Check the requirements for an iterator satisfying bidirectional iterator
/// category but not more.
template <template <Has_sentinel> class Iterator_t>
using Bidirectional_iterator_category_checker =
    With_and_without_sentinel_checker<Iterator_t,
                                      std::bidirectional_iterator_tag,
                                      std::bidirectional_iterator_tag>;

/// Check the requirements for an iterator satisfying random access iterator
/// concept but not more.
template <template <Has_sentinel> class Iterator_t>
using Random_access_iterator_concept_checker =
    With_and_without_sentinel_checker<
        Iterator_t, std::random_access_iterator_tag, std::input_iterator_tag>;

/// Check the requirements for an iterator satisfying random access iterator
/// category but not more.
template <template <Has_sentinel> class Iterator_t>
using Random_access_iterator_category_checker =
    With_and_without_sentinel_checker<Iterator_t,
                                      std::random_access_iterator_tag,
                                      std::random_access_iterator_tag>;

/// Check the requirements for an iterator satisfying contiguos iterator
/// category and concept.
template <template <Has_sentinel> class Iterator_t>
using Contiguous_iterator_checker =
    With_and_without_sentinel_checker<Iterator_t, std::contiguous_iterator_tag,
                                      std::contiguous_iterator_tag>;

// ==== 4. "Container" that iterators in our scenarios iterate over ====

// The element type.
//
// We just want a type that is as simple as possible. `int` would be fine,
// except the type needs to be a struct or class in order to test `operator->`.
// Therefore, we define a minimal class having just an int member, and implicit
// conversions from and to int.
struct Int_wrapper {
  // intentionally implicit
  // NOLINTNEXTLINE(google-explicit-constructor, hicpp-explicit-conversions)
  Int_wrapper(int v) noexcept : m_value(v) {}
  // intentionally implicit
  // NOLINTNEXTLINE(google-explicit-constructor, hicpp-explicit-conversions)
  operator int() const { return m_value; }
  int m_value;
};

// The "container": an array of 10 elements.
//
// The assertions will inspect one element left and right of each iterator
// position, hence populating it with extra elements at each end.
Int_wrapper array_data[] = {-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
Int_wrapper *array_base = &array_data[1];

// ==== 5. Helper macros to define test scenarios ====

// We will define a number of iterator classes below. All are similar, but we
// expose different sets of members for them, in order to test the iterator
// category deduction in Iterator_interface. To reduce the amount of copy-pasted
// boilerplate code, we use these macros to define members.

// Begin the declaration of an iterator class that does not have a template
// argument for a sentinel type.
//
// @param NAME Suffix of the class name.
#define BEGIN_NOSENTINEL(NAME) \
  class Iterator_##NAME : public iterators::Iterator_interface<Iterator_##NAME>

// Begin the declaration of an iterator having a template argument for a
// sentinel type.
//
// @param NAME Suffix of the class name.
#define BEGIN(NAME)                       \
  template <Has_sentinel has_sentinel_tp> \
  class Iterator_##NAME                   \
      : public iterators::Iterator_interface<Iterator_##NAME<has_sentinel_tp>>

// This must follow the BEGIN[_NOSENTINEL]. It defines the member that hodls
// the postion, and
#define HEAD(NAME)                \
  using This_t = Iterator_##NAME; \
  int m_position = 0;             \
                                  \
 public:

// Define the iterator category as a member type.
//
// @param NAME Iterator category: one of `input`, `random_access`, etc.
#define CATEGORY(NAME) using Iterator_category_t = std::NAME##_iterator_tag;

// Define the iterator concept as a member type.
//
// @param NAME Iterator category: one of `input`, `random_access`, etc.
#define CONCEPT(NAME) using Iterator_concept_t = std::NAME##_iterator_tag;

// Delete the copy constructor and copy assignment operator (and define the
// default constructor, move constructor, move assignment operator, and
// destructor), making the iterator single-pass and an input iterator.
//
// @param NAME Suffix of the class name.
#define DELETE_COPY(NAME)                                       \
  Iterator_##NAME() = default;                                  \
  Iterator_##NAME(const Iterator_##NAME &) = delete;            \
  Iterator_##NAME(Iterator_##NAME &&) = default;                \
  Iterator_##NAME &operator=(const Iterator_##NAME &) = delete; \
  Iterator_##NAME &operator=(Iterator_##NAME &&) = default;     \
  ~Iterator_##NAME() = default;

// Define a `get` member returning by value.
#define GET \
  auto get() const { return array_base[m_position]; }

// Define a `get` member returning by reference.
#define GET_REF \
  auto &get() const { return array_base[m_position]; }

// Define a `get` member returning by value.
#define GET_POINTER \
  auto *get_pointer() const { return array_base + m_position; }

// Define a `next` member.
#define NEXT \
  void next() { ++m_position; }

// Define a `prev` member.
#define PREV \
  void prev() { --m_position; }

// Define an `advance` member.
#define ADVANCE \
  void advance(std::ptrdiff_t delta) { m_position += delta; }

// Define an `is_equal` member.
//
// @param NAME Suffix of the class name.
#define IS_EQUAL                             \
  bool is_equal(const This_t &other) const { \
    return m_position == other.m_position;   \
  }

// Define an `is_sentinel` member.
#define IS_SENTINEL                                \
  bool is_sentinel() const                         \
    requires(has_sentinel_tp == Has_sentinel::yes) \
  {                                                \
    return m_position == 10;                       \
  }

// Define a `distance_from` member.
//
// @param NAME Suffix of the class name.
#define DISTANCE_FROM                                       \
  std::ptrdiff_t distance_from(const This_t &other) const { \
    return m_position - other.m_position;                   \
  }

// Define a `distance_from_sentinel` member.
#define DISTANCE_FROM_SENTINEL                     \
  std::ptrdiff_t distance_from_sentinel() const    \
    requires(has_sentinel_tp == Has_sentinel::yes) \
  {                                                \
    return m_position - 10;                        \
  }

// ==== 6. Test scenarios ====

// Each subsection contains iterators of a given category. We use the macros
// above to define the iterators succinctly. Then we instantiate the checker for
// the given category, specialized to each iterator.

// ---- 6.1. Input iterator concept scenarios ----

// Deduced concept input_iterator; category none.
// Defined by omitting equality comparison.
BEGIN(ix1){HEAD(ix1) DELETE_COPY(ix1) GET NEXT IS_SENTINEL};
BEGIN(ix2){HEAD(ix2) GET NEXT IS_SENTINEL};
BEGIN(ix3){HEAD(ix3) GET NEXT PREV IS_SENTINEL};
BEGIN(ix4){HEAD(ix4) GET ADVANCE IS_SENTINEL};
BEGIN(ix5){HEAD(ix5) GET NEXT PREV ADVANCE IS_SENTINEL};

BEGIN(ix6){HEAD(ix6) DELETE_COPY(ix6) GET_REF NEXT IS_SENTINEL};
BEGIN(ix7){HEAD(ix7) GET_REF NEXT IS_SENTINEL};
BEGIN(ix8){HEAD(ix8) GET_REF NEXT PREV IS_SENTINEL};
BEGIN(ix9){HEAD(ix9) GET_REF ADVANCE IS_SENTINEL};
BEGIN(ix10){HEAD(ix10) GET_REF NEXT PREV ADVANCE IS_SENTINEL};

BEGIN(ix11){HEAD(ix11) DELETE_COPY(ix11) GET_POINTER NEXT IS_SENTINEL};
BEGIN(ix12){HEAD(ix12) GET_POINTER NEXT IS_SENTINEL};
BEGIN(ix13){HEAD(ix13) GET_POINTER NEXT PREV IS_SENTINEL};
BEGIN(ix14){HEAD(ix14) GET_POINTER ADVANCE IS_SENTINEL};
BEGIN(ix15){HEAD(ix15) GET_POINTER NEXT PREV ADVANCE IS_SENTINEL};

// ---- 6.2 Input iterator category scenarios ----

// Deduced concept and category input_iterator.
// Defined by omitting copy constructor.
BEGIN(ii1){HEAD(ii1) DELETE_COPY(ii1) GET NEXT IS_EQUAL IS_SENTINEL};
BEGIN(ii2){HEAD(ii2) DELETE_COPY(ii2) GET NEXT PREV IS_EQUAL IS_SENTINEL};
BEGIN(ii3){HEAD(ii3) DELETE_COPY(ii3) GET ADVANCE IS_EQUAL IS_SENTINEL};
BEGIN(ii4){HEAD(ii4) DELETE_COPY(ii4)
               GET NEXT PREV ADVANCE IS_EQUAL IS_SENTINEL};
BEGIN(ii5){HEAD(ii5) DELETE_COPY(ii5) GET ADVANCE DISTANCE_FROM IS_SENTINEL};
BEGIN(ii6){HEAD(ii6) DELETE_COPY(ii6)
               GET NEXT PREV ADVANCE DISTANCE_FROM IS_SENTINEL};

BEGIN(ii7){HEAD(ii7) DELETE_COPY(ii7) GET_REF NEXT IS_EQUAL IS_SENTINEL};
BEGIN(ii8){HEAD(ii8) DELETE_COPY(ii8) GET_REF NEXT PREV IS_EQUAL IS_SENTINEL};
BEGIN(ii9){HEAD(ii9) DELETE_COPY(ii9) GET_REF ADVANCE IS_EQUAL IS_SENTINEL};
BEGIN(ii10){HEAD(ii10) DELETE_COPY(ii10)
                GET_REF NEXT PREV ADVANCE IS_EQUAL IS_SENTINEL};
BEGIN(ii11){HEAD(ii11) DELETE_COPY(ii11)
                GET_REF ADVANCE DISTANCE_FROM IS_SENTINEL};
BEGIN(ii12){HEAD(ii12) DELETE_COPY(ii12)
                GET_REF NEXT PREV ADVANCE DISTANCE_FROM IS_SENTINEL};

BEGIN(ii13){HEAD(ii13) DELETE_COPY(ii13) GET_POINTER NEXT IS_EQUAL IS_SENTINEL};
BEGIN(ii14){HEAD(ii14) DELETE_COPY(ii14)
                GET_POINTER NEXT PREV IS_EQUAL IS_SENTINEL};
BEGIN(ii15){HEAD(ii15) DELETE_COPY(ii15)
                GET_POINTER ADVANCE IS_EQUAL IS_SENTINEL};
BEGIN(ii16){HEAD(ii16) DELETE_COPY(ii16)
                GET_POINTER NEXT PREV ADVANCE IS_EQUAL IS_SENTINEL};
BEGIN(ii17){HEAD(ii17) DELETE_COPY(ii17)
                GET_POINTER ADVANCE DISTANCE_FROM IS_SENTINEL};
BEGIN(ii18){HEAD(ii18) DELETE_COPY(ii18)
                GET_POINTER NEXT PREV ADVANCE DISTANCE_FROM IS_SENTINEL};

// ---- 6.3 Forward iterator concept scenarios ----

// Deduced concept forward_iterator; category input_iterator.
// Defined by returning by value.
BEGIN(fi1){HEAD(fi1) GET NEXT IS_EQUAL IS_SENTINEL};
BEGIN(fi2){HEAD(fi2) GET NEXT DISTANCE_FROM IS_SENTINEL};

// ---- 6.4 Forward iterator category scenarios ----

// Deduced concept and category forward_iterator.
// Defined by returning by value.
BEGIN(ff1){HEAD(ff1) GET_REF NEXT IS_EQUAL IS_SENTINEL};
BEGIN(ff2){HEAD(ff2) GET_REF NEXT DISTANCE_FROM IS_SENTINEL};

BEGIN(ff3){HEAD(ff3) GET_POINTER NEXT IS_EQUAL IS_SENTINEL};
BEGIN(ff4){HEAD(ff4) GET_POINTER NEXT DISTANCE_FROM IS_SENTINEL};

// ---- 6.5 Bidirectional iterator concept scenarios ----

// Deduced concept bidirectional_iterator; category input_iterator.
// Defined by returning by value.
BEGIN(bi1){HEAD(bi1) GET NEXT PREV IS_EQUAL IS_SENTINEL};
BEGIN(bi2){HEAD(bi2) GET ADVANCE IS_EQUAL IS_SENTINEL};
BEGIN(bi3){HEAD(bi3) GET NEXT PREV DISTANCE_FROM IS_SENTINEL};

// ---- 6.6 Bidirectional iterator category scenarios ----

// Deduced concept and category bidirectional_iterator.
BEGIN(bb1){HEAD(bb1) GET_REF NEXT PREV IS_EQUAL IS_SENTINEL};
BEGIN(bb2){HEAD(bb2) GET_REF ADVANCE IS_EQUAL IS_SENTINEL};
BEGIN(bb3){HEAD(bb3) GET_REF NEXT PREV DISTANCE_FROM IS_SENTINEL};

BEGIN(bb4){HEAD(bb4) GET_POINTER NEXT PREV IS_EQUAL IS_SENTINEL};
BEGIN(bb5){HEAD(bb5) GET_POINTER ADVANCE IS_EQUAL IS_SENTINEL};
BEGIN(bb6){HEAD(bb6) GET_POINTER NEXT PREV DISTANCE_FROM IS_SENTINEL};

// ---- 6.7 Random_access iterator concept scenarios ----

BEGIN(ri1){HEAD(ri1) GET ADVANCE DISTANCE_FROM DISTANCE_FROM_SENTINEL};
BEGIN(ri2){
    HEAD(ri2)
        GET NEXT PREV ADVANCE IS_EQUAL DISTANCE_FROM DISTANCE_FROM_SENTINEL};

// ---- 6.8 Random_access iterator category scenarios ----

BEGIN(rr1){HEAD(rr1) GET_REF ADVANCE DISTANCE_FROM DISTANCE_FROM_SENTINEL};
BEGIN(rr2){HEAD(rr2) GET_REF NEXT PREV ADVANCE IS_EQUAL DISTANCE_FROM
               DISTANCE_FROM_SENTINEL};

// ---- 6.9 Contiguous iterator category&concept scenarios ----

BEGIN(cc1){HEAD(cc1) GET_POINTER ADVANCE DISTANCE_FROM DISTANCE_FROM_SENTINEL};
BEGIN(cc2){HEAD(cc2) GET_POINTER NEXT PREV ADVANCE IS_EQUAL DISTANCE_FROM
               DISTANCE_FROM_SENTINEL};

// Test all the input concept iterators
TEST(LibsMysqlIteratorsExhaustive, InputConceptIterators) {
  Input_iterator_concept_checker<Iterator_ix1> ix1;
  Input_iterator_concept_checker<Iterator_ix2> ix2;
  Input_iterator_concept_checker<Iterator_ix3> ix3;
  Input_iterator_concept_checker<Iterator_ix4> ix4;
  Input_iterator_concept_checker<Iterator_ix5> ix5;
  Input_iterator_concept_checker<Iterator_ix6> ix6;
  Input_iterator_concept_checker<Iterator_ix7> ix7;
  Input_iterator_concept_checker<Iterator_ix8> ix8;
  Input_iterator_concept_checker<Iterator_ix9> ix9;
  Input_iterator_concept_checker<Iterator_ix10> ix10;
  Input_iterator_concept_checker<Iterator_ix11> ix11;
  Input_iterator_concept_checker<Iterator_ix12> ix12;
  Input_iterator_concept_checker<Iterator_ix13> ix13;
  Input_iterator_concept_checker<Iterator_ix14> ix14;
  Input_iterator_concept_checker<Iterator_ix15> ix15;
}

// Test all the input concept iterators
TEST(LibsMysqlIteratorsExhaustive, InputCategoryIterators) {
  Input_iterator_category_checker<Iterator_ii1> ii1;
  Input_iterator_category_checker<Iterator_ii2> ii2;
  Input_iterator_category_checker<Iterator_ii3> ii3;
  Input_iterator_category_checker<Iterator_ii4> ii4;
  Input_iterator_category_checker<Iterator_ii5> ii5;
  Input_iterator_category_checker<Iterator_ii6> ii6;
  Input_iterator_category_checker<Iterator_ii7> ii7;
  Input_iterator_category_checker<Iterator_ii8> ii8;
  Input_iterator_category_checker<Iterator_ii9> ii9;
  Input_iterator_category_checker<Iterator_ii10> ii10;
  Input_iterator_category_checker<Iterator_ii11> ii11;
  Input_iterator_category_checker<Iterator_ii12> ii12;
  Input_iterator_category_checker<Iterator_ii13> ii13;
  Input_iterator_category_checker<Iterator_ii14> ii14;
  Input_iterator_category_checker<Iterator_ii15> ii15;
  Input_iterator_category_checker<Iterator_ii16> ii16;
  Input_iterator_category_checker<Iterator_ii17> ii17;
  Input_iterator_category_checker<Iterator_ii18> ii18;
}

// Test all the forward concept iterators
TEST(LibsMysqlIteratorsExhaustive, ForwardConceptIterators) {
  Forward_iterator_concept_checker<Iterator_fi1> fi1;
  Forward_iterator_concept_checker<Iterator_fi2> fi2;
}

// Test all the forward category iterators
TEST(LibsMysqlIteratorsExhaustive, ForwardCategoryIterators) {
  Forward_iterator_category_checker<Iterator_ff1> ff1;
  Forward_iterator_category_checker<Iterator_ff2> ff2;
  Forward_iterator_category_checker<Iterator_ff3> ff3;
  Forward_iterator_category_checker<Iterator_ff4> ff4;
}

// Test all the bidirectional concept iterators
TEST(LibsMysqlIteratorsExhaustive, BidirectionalConceptIterators) {
  Bidirectional_iterator_concept_checker<Iterator_bi1> bi1;
  Bidirectional_iterator_concept_checker<Iterator_bi2> bi2;
  Bidirectional_iterator_concept_checker<Iterator_bi3> bi3;
}

// Test all the bidirectional category iterators
TEST(LibsMysqlIteratorsExhaustive, BidirectionalCategoryIterators) {
  Bidirectional_iterator_category_checker<Iterator_bb1> bb1;
  Bidirectional_iterator_category_checker<Iterator_bb2> bb2;
  Bidirectional_iterator_category_checker<Iterator_bb3> bb3;
  Bidirectional_iterator_category_checker<Iterator_bb4> bb4;
  Bidirectional_iterator_category_checker<Iterator_bb5> bb5;
  Bidirectional_iterator_category_checker<Iterator_bb6> bb6;
}

// Test all the random_access concept iterators
TEST(LibsMysqlIteratorsExhaustive, RandomAccessConceptIterators) {
  Random_access_iterator_concept_checker<Iterator_ri1> ri1;
  Random_access_iterator_concept_checker<Iterator_ri2> ri2;
}

// Test all the random_access category iterators
TEST(LibsMysqlIteratorsExhaustive, RandomAccessCategoryIterators) {
  Random_access_iterator_category_checker<Iterator_rr1> rr1;
  Random_access_iterator_category_checker<Iterator_rr2> rr2;
}

// Test all the contiguous category&concept iterators
TEST(LibsMysqlIteratorsExhaustive, ContiguousIterators) {
  Contiguous_iterator_checker<Iterator_cc1> cc1;
  Contiguous_iterator_checker<Iterator_cc2> cc2;
}

}  // namespace
