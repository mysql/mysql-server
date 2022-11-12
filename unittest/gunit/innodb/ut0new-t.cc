/* Copyright (c) 2014, 2022, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/* See http://code.google.com/p/googletest/wiki/Primer */

#include <gtest/gtest.h>
#include <stddef.h>
#include <memory>
#include <stdexcept>
#include <vector>

#include "my_config.h"
#include "storage/innobase/include/univ.i"
#include "storage/innobase/include/ut0new.h"

namespace innodb_ut0new_unittest {

static auto pfs_key = 12345;

inline bool ptr_is_suitably_aligned(void *ptr) {
#if (defined(LINUX_RHEL6) || (defined(LINUX_RHEL7))) && (SIZEOF_VOIDP == 4)
  // This is a "workaround" for 32-bit OL6/7 platform which do not respect
  // the alignment requirements. TL;DR 32-bit OL6/7/8 have a violation so that
  // the pointer returned by malloc is not suitably aligned
  // (ptr % alignof(max_align_t) is _not_ 0). This was found out through the
  // suite of ut0new* unit-tests and since then these unit-tests are failing
  // on those platforms.
  //
  // For more details see
  // https://mybug.mysql.oraclecorp.com/orabugs/bug.php?id=33137030.
  //
  // As it stands, issue is identified and confirmed but OL6 and OL7 platforms
  // will not receive a backport of a fix that has been deployed to OL8. By
  // returning true, we allow the unit-tests to continue running in faith that
  // nothing will become broken.
  return true;
#else
  return reinterpret_cast<std::uintptr_t>(ptr) % alignof(max_align_t) == 0;
#endif
}

/* test edge cases */
TEST(ut0new, edgecases) {
#ifdef UNIV_PFS_MEMORY
  auto ptr = ut::new_arr<byte>(ut::Count{0});
  EXPECT_NE(nullptr, ptr);
  ut::delete_arr((byte *)ptr);
#endif /* UNIV_PFS_MEMORY */
}

struct Pod_type {
  Pod_type(int _x, int _y) : x(_x), y(_y) {}
  int x;
  int y;
};

struct My_fancy_sum {
  My_fancy_sum(int x, int y) : result(x + y) {}
  int result;
};
struct Non_pod_type {
  Non_pod_type(int _x, int _y, std::string _s)
      : x(_x), y(_y), s(_s), sum(x, y) {
    s_count++;
  }
  Non_pod_type(const Non_pod_type &other)
      : x(other.x), y(other.y), s(other.s), sum(other.sum) {
    s_count++;
  }
  ~Non_pod_type() { s_count--; }
  int x;
  int y;
  std::string s;
  My_fancy_sum sum;
  static size_t get_count() { return s_count; }

 private:
  static size_t s_count;
};
size_t Non_pod_type::s_count{0};

struct Default_constructible_pod {
  Default_constructible_pod() : x(0), y(1) {}
  int x, y;
};
struct Default_constructible_non_pod {
  Default_constructible_non_pod() : x(0), y(1), s("non-pod-string") {}
  int x, y;
  std::string s;
};

template <typename T, bool With_pfs>
struct Ut0new_test_param_wrapper {
  using type = T;
  static constexpr bool with_pfs = With_pfs;
};

using all_fundamental_types = ::testing::Types<
    // with PFS
    Ut0new_test_param_wrapper<char, true>,
    Ut0new_test_param_wrapper<unsigned char, true>,
    Ut0new_test_param_wrapper<wchar_t, true>,
    Ut0new_test_param_wrapper<short int, true>,
    Ut0new_test_param_wrapper<unsigned short int, true>,
    Ut0new_test_param_wrapper<int, true>,
    Ut0new_test_param_wrapper<unsigned int, true>,
    Ut0new_test_param_wrapper<long int, true>,
    Ut0new_test_param_wrapper<unsigned long int, true>,
    Ut0new_test_param_wrapper<long long int, true>,
    Ut0new_test_param_wrapper<unsigned long long int, true>,
    Ut0new_test_param_wrapper<float, true>,
    Ut0new_test_param_wrapper<double, true>,
    Ut0new_test_param_wrapper<long double, true>,
    // no PFS
    Ut0new_test_param_wrapper<char, false>,
    Ut0new_test_param_wrapper<unsigned char, false>,
    Ut0new_test_param_wrapper<wchar_t, false>,
    Ut0new_test_param_wrapper<short int, false>,
    Ut0new_test_param_wrapper<unsigned short int, false>,
    Ut0new_test_param_wrapper<int, false>,
    Ut0new_test_param_wrapper<unsigned int, false>,
    Ut0new_test_param_wrapper<long int, false>,
    Ut0new_test_param_wrapper<unsigned long int, false>,
    Ut0new_test_param_wrapper<long long int, false>,
    Ut0new_test_param_wrapper<unsigned long long int, false>,
    Ut0new_test_param_wrapper<float, false>,
    Ut0new_test_param_wrapper<double, false>,
    Ut0new_test_param_wrapper<long double, false>>;

using all_pod_types = ::testing::Types<
    // with PFS
    Ut0new_test_param_wrapper<Pod_type, true>,
    // no PFS
    Ut0new_test_param_wrapper<Pod_type, false>>;

using all_default_constructible_pod_types = ::testing::Types<
    // with PFS
    Ut0new_test_param_wrapper<Default_constructible_pod, true>,
    // no PFS
    Ut0new_test_param_wrapper<Default_constructible_pod, false>>;

using all_non_pod_types = ::testing::Types<
    // with PFS
    Ut0new_test_param_wrapper<Non_pod_type, true>,
    // no PFS
    Ut0new_test_param_wrapper<Non_pod_type, false>>;

using all_default_constructible_non_pod_types = ::testing::Types<
    // with PFS
    Ut0new_test_param_wrapper<Default_constructible_non_pod, true>,
    // no PFS
    Ut0new_test_param_wrapper<Default_constructible_non_pod, false>>;

// malloc/free - fundamental types
template <typename T>
class ut0new_malloc_free_fundamental_types : public ::testing::Test {};
TYPED_TEST_SUITE_P(ut0new_malloc_free_fundamental_types);
TYPED_TEST_P(ut0new_malloc_free_fundamental_types, fundamental_types) {
  using type = typename TypeParam::type;
  auto with_pfs = TypeParam::with_pfs;
  type *ptr = with_pfs ? static_cast<type *>(ut::malloc_withkey(
                             ut::make_psi_memory_key(pfs_key), sizeof(type)))
                       : static_cast<type *>(ut::malloc(sizeof(type)));
  EXPECT_TRUE(ptr_is_suitably_aligned(ptr));
  ut::free(ptr);
}
REGISTER_TYPED_TEST_SUITE_P(ut0new_malloc_free_fundamental_types,
                            fundamental_types);
INSTANTIATE_TYPED_TEST_SUITE_P(FundamentalTypes,
                               ut0new_malloc_free_fundamental_types,
                               all_fundamental_types);

// malloc/free - pod types
template <typename T>
class ut0new_malloc_free_pod_types : public ::testing::Test {};
TYPED_TEST_SUITE_P(ut0new_malloc_free_pod_types);
TYPED_TEST_P(ut0new_malloc_free_pod_types, pod_types) {
  using type = typename TypeParam::type;
  auto with_pfs = TypeParam::with_pfs;
  type *ptr = with_pfs ? static_cast<type *>(ut::malloc_withkey(
                             ut::make_psi_memory_key(pfs_key), sizeof(type)))
                       : static_cast<type *>(ut::malloc(sizeof(type)));
  EXPECT_TRUE(ptr_is_suitably_aligned(ptr));
  ut::free(ptr);
}
REGISTER_TYPED_TEST_SUITE_P(ut0new_malloc_free_pod_types, pod_types);
INSTANTIATE_TYPED_TEST_SUITE_P(PodTypes, ut0new_malloc_free_pod_types,
                               all_pod_types);

// malloc/free - non-pod types
template <typename T>
class ut0new_malloc_free_non_pod_types : public ::testing::Test {};
TYPED_TEST_SUITE_P(ut0new_malloc_free_non_pod_types);
TYPED_TEST_P(ut0new_malloc_free_non_pod_types, non_pod_types) {
  using type = typename TypeParam::type;
  auto with_pfs = TypeParam::with_pfs;
  type *ptr = with_pfs ? static_cast<type *>(ut::malloc_withkey(
                             ut::make_psi_memory_key(pfs_key), sizeof(type)))
                       : static_cast<type *>(ut::malloc(sizeof(type)));
  EXPECT_TRUE(ptr_is_suitably_aligned(ptr));
  ut::free(ptr);
}
REGISTER_TYPED_TEST_SUITE_P(ut0new_malloc_free_non_pod_types, non_pod_types);
INSTANTIATE_TYPED_TEST_SUITE_P(NonPodTypes, ut0new_malloc_free_non_pod_types,
                               all_non_pod_types);

// zalloc/free - fundamental types
template <typename T>
class ut0new_zalloc_free_fundamental_types : public ::testing::Test {};
TYPED_TEST_SUITE_P(ut0new_zalloc_free_fundamental_types);
TYPED_TEST_P(ut0new_zalloc_free_fundamental_types, fundamental_types) {
  using type = typename TypeParam::type;
  auto with_pfs = TypeParam::with_pfs;
  type *ptr = with_pfs ? static_cast<type *>(ut::zalloc_withkey(
                             ut::make_psi_memory_key(pfs_key), sizeof(type)))
                       : static_cast<type *>(ut::zalloc(sizeof(type)));
  EXPECT_TRUE(ptr_is_suitably_aligned(ptr));
  EXPECT_EQ(*ptr, 0);
  ut::free(ptr);
}
REGISTER_TYPED_TEST_SUITE_P(ut0new_zalloc_free_fundamental_types,
                            fundamental_types);
INSTANTIATE_TYPED_TEST_SUITE_P(FundamentalTypes,
                               ut0new_zalloc_free_fundamental_types,
                               all_fundamental_types);

// realloc - fundamental types
template <typename T>
class ut0new_realloc_fundamental_types : public ::testing::Test {};
TYPED_TEST_SUITE_P(ut0new_realloc_fundamental_types);
TYPED_TEST_P(ut0new_realloc_fundamental_types, fundamental_types) {
  using T = typename TypeParam::type;
  auto with_pfs = TypeParam::with_pfs;
  {
    // Allocating through realloc and release through free should work fine
    auto p = with_pfs
                 ? static_cast<T *>(ut::realloc_withkey(
                       ut::make_psi_memory_key(pfs_key), nullptr, sizeof(T)))
                 : static_cast<T *>(ut::realloc(nullptr, sizeof(T)));
    EXPECT_TRUE(ptr_is_suitably_aligned(p));
    ut::free(p);
  }

  {
    // Allocating through realloc and release through realloc should work fine
    auto p = with_pfs
                 ? static_cast<T *>(ut::realloc_withkey(
                       ut::make_psi_memory_key(pfs_key), nullptr, sizeof(T)))
                 : static_cast<T *>(ut::realloc(nullptr, sizeof(T)));
    EXPECT_TRUE(ptr_is_suitably_aligned(p));
    ut::realloc(p, 0);
  }

  {
    // Allocating through malloc and then upsizing the memory region through
    // realloc should work fine
    auto p = with_pfs ? static_cast<T *>(ut::malloc_withkey(
                            ut::make_psi_memory_key(pfs_key), sizeof(T)))
                      : static_cast<T *>(ut::malloc(sizeof(T)));
    EXPECT_TRUE(ptr_is_suitably_aligned(p));

    // Let's write something into the memory so we can afterwards check if
    // ut::realloc_* is handling the copying/moving the element(s) properly
    *p = 0xA;

    // Enlarge to 10x through realloc
    p = with_pfs ? static_cast<T *>(ut::realloc_withkey(
                       ut::make_psi_memory_key(pfs_key), p, 10 * sizeof(T)))
                 : static_cast<T *>(ut::realloc(p, 10 * sizeof(T)));
    EXPECT_TRUE(ptr_is_suitably_aligned(p));

    // Make sure that the contents of memory are untouched after reallocation
    EXPECT_EQ(p[0], 0xA);

    // Write some more stuff to the memory
    for (size_t i = 1; i < 10; i++) p[i] = 0xB;

    // Enlarge to 100x through realloc
    p = with_pfs ? static_cast<T *>(ut::realloc_withkey(
                       ut::make_psi_memory_key(pfs_key), p, 100 * sizeof(T)))
                 : static_cast<T *>(ut::realloc(p, 100 * sizeof(T)));
    EXPECT_TRUE(ptr_is_suitably_aligned(p));

    // Make sure that the contents of memory are untouched after reallocation
    EXPECT_EQ(p[0], 0xA);
    for (size_t i = 1; i < 10; i++) EXPECT_EQ(p[i], 0xB);

    // Write some more stuff to the memory
    for (size_t i = 10; i < 100; i++) p[i] = 0xC;

    // Enlarge to 1000x through realloc
    p = with_pfs ? static_cast<T *>(ut::realloc_withkey(
                       ut::make_psi_memory_key(pfs_key), p, 1000 * sizeof(T)))
                 : static_cast<T *>(ut::realloc(p, 1000 * sizeof(T)));
    EXPECT_TRUE(ptr_is_suitably_aligned(p));

    // Make sure that the contents of memory are untouched after reallocation
    EXPECT_EQ(p[0], 0xA);
    for (size_t i = 1; i < 10; i++) EXPECT_EQ(p[i], 0xB);
    for (size_t i = 10; i < 100; i++) EXPECT_EQ(p[i], 0xC);

    ut::free(p);
  }

  {
    // Allocating through malloc and then downsizing the memory region through
    // realloc should also work fine
    auto p = with_pfs ? static_cast<T *>(ut::malloc_withkey(
                            ut::make_psi_memory_key(pfs_key), 10 * sizeof(T)))
                      : static_cast<T *>(ut::malloc(10 * sizeof(T)));
    EXPECT_TRUE(ptr_is_suitably_aligned(p));

    // Write some stuff to the memory
    for (size_t i = 0; i < 10; i++) p[i] = 0xA;

    // Downsize the array to only half of the elements
    p = with_pfs ? static_cast<T *>(ut::realloc_withkey(
                       ut::make_psi_memory_key(pfs_key), p, 5 * sizeof(T)))
                 : static_cast<T *>(ut::realloc(p, 5 * sizeof(T)));
    EXPECT_TRUE(ptr_is_suitably_aligned(p));

    // Make sure that the respective contents of memory (only half of the
    // elements) are untouched after reallocation
    for (size_t i = 0; i < 5; i++) EXPECT_EQ(p[i], 0xA);

    ut::free(p);
  }
}
REGISTER_TYPED_TEST_SUITE_P(ut0new_realloc_fundamental_types,
                            fundamental_types);
INSTANTIATE_TYPED_TEST_SUITE_P(FundamentalTypes,
                               ut0new_realloc_fundamental_types,
                               all_fundamental_types);

// new/delete - fundamental types
template <typename T>
class ut0new_new_delete_fundamental_types : public ::testing::Test {};
TYPED_TEST_SUITE_P(ut0new_new_delete_fundamental_types);
TYPED_TEST_P(ut0new_new_delete_fundamental_types, fundamental_types) {
  using type = typename TypeParam::type;
  auto with_pfs = TypeParam::with_pfs;
  type *ptr = with_pfs
                  ? ut::new_withkey<type>(ut::make_psi_memory_key(pfs_key), 1)
                  : ut::new_<type>(1);
  EXPECT_TRUE(ptr_is_suitably_aligned(ptr));
  EXPECT_EQ(*ptr, 1);
  ut::delete_(ptr);
}
REGISTER_TYPED_TEST_SUITE_P(ut0new_new_delete_fundamental_types,
                            fundamental_types);
INSTANTIATE_TYPED_TEST_SUITE_P(My, ut0new_new_delete_fundamental_types,
                               all_fundamental_types);

// new/delete - pod types
template <typename T>
class ut0new_new_delete_pod_types : public ::testing::Test {};
TYPED_TEST_SUITE_P(ut0new_new_delete_pod_types);
TYPED_TEST_P(ut0new_new_delete_pod_types, pod_types) {
  using type = typename TypeParam::type;
  auto with_pfs = TypeParam::with_pfs;

  type *ptr =
      with_pfs ? ut::new_withkey<type>(ut::make_psi_memory_key(pfs_key), 2, 5)
               : ut::new_<type>(2, 5);
  EXPECT_TRUE(ptr_is_suitably_aligned(ptr));
  EXPECT_EQ(ptr->x, 2);
  EXPECT_EQ(ptr->y, 5);
  ut::delete_(ptr);
}
REGISTER_TYPED_TEST_SUITE_P(ut0new_new_delete_pod_types, pod_types);
INSTANTIATE_TYPED_TEST_SUITE_P(My, ut0new_new_delete_pod_types, all_pod_types);

// new/delete - non-pod types
template <typename T>
class ut0new_new_delete_non_pod_types : public ::testing::Test {};
TYPED_TEST_SUITE_P(ut0new_new_delete_non_pod_types);
TYPED_TEST_P(ut0new_new_delete_non_pod_types, non_pod_types) {
  using type = typename TypeParam::type;
  auto with_pfs = TypeParam::with_pfs;
  EXPECT_EQ(type::get_count(), 0);
  type *ptr = with_pfs ? ut::new_withkey<type>(ut::make_psi_memory_key(pfs_key),
                                               2, 5, std::string("non-pod"))
                       : ut::new_<type>(2, 5, std::string("non-pod"));
  EXPECT_TRUE(ptr_is_suitably_aligned(ptr));
  EXPECT_EQ(ptr->x, 2);
  EXPECT_EQ(ptr->y, 5);
  EXPECT_EQ(ptr->sum.result, 7);
  EXPECT_EQ(ptr->s, std::string("non-pod"));
  EXPECT_EQ(type::get_count(), 1);
  ut::delete_(ptr);
  EXPECT_EQ(type::get_count(), 0);
}
REGISTER_TYPED_TEST_SUITE_P(ut0new_new_delete_non_pod_types, non_pod_types);
INSTANTIATE_TYPED_TEST_SUITE_P(My, ut0new_new_delete_non_pod_types,
                               all_non_pod_types);

// new/delete - array specialization for fundamental types
template <typename T>
class ut0new_new_delete_fundamental_types_arr : public ::testing::Test {};
TYPED_TEST_SUITE_P(ut0new_new_delete_fundamental_types_arr);
TYPED_TEST_P(ut0new_new_delete_fundamental_types_arr, fundamental_types) {
  using type = typename TypeParam::type;
  auto with_pfs = TypeParam::with_pfs;
  type *ptr =
      with_pfs
          ? ut::new_arr_withkey<type>(
                ut::make_psi_memory_key(pfs_key),
                std::forward_as_tuple((type)0), std::forward_as_tuple((type)1),
                std::forward_as_tuple((type)2), std::forward_as_tuple((type)3),
                std::forward_as_tuple((type)4), std::forward_as_tuple((type)5),
                std::forward_as_tuple((type)6), std::forward_as_tuple((type)7),
                std::forward_as_tuple((type)8), std::forward_as_tuple((type)9))
          : ut::new_arr<type>(
                std::forward_as_tuple((type)0), std::forward_as_tuple((type)1),
                std::forward_as_tuple((type)2), std::forward_as_tuple((type)3),
                std::forward_as_tuple((type)4), std::forward_as_tuple((type)5),
                std::forward_as_tuple((type)6), std::forward_as_tuple((type)7),
                std::forward_as_tuple((type)8), std::forward_as_tuple((type)9));

  EXPECT_TRUE(ptr_is_suitably_aligned(ptr));

  for (size_t elem = 0; elem < 10; elem++) {
    EXPECT_EQ(ptr[elem], elem);
  }
  ut::delete_arr(ptr);
}
REGISTER_TYPED_TEST_SUITE_P(ut0new_new_delete_fundamental_types_arr,
                            fundamental_types);
INSTANTIATE_TYPED_TEST_SUITE_P(My, ut0new_new_delete_fundamental_types_arr,
                               all_fundamental_types);

// new/delete - array specialization for pod types
template <typename T>
class ut0new_new_delete_pod_types_arr : public ::testing::Test {};
TYPED_TEST_SUITE_P(ut0new_new_delete_pod_types_arr);
TYPED_TEST_P(ut0new_new_delete_pod_types_arr, pod_types) {
  using type = typename TypeParam::type;
  auto with_pfs = TypeParam::with_pfs;
  type *ptr =
      with_pfs
          ? ut::new_arr_withkey<type>(
                ut::make_psi_memory_key(pfs_key), std::forward_as_tuple(0, 1),
                std::forward_as_tuple(2, 3), std::forward_as_tuple(4, 5),
                std::forward_as_tuple(6, 7), std::forward_as_tuple(8, 9))
          : ut::new_arr<type>(
                std::forward_as_tuple(0, 1), std::forward_as_tuple(2, 3),
                std::forward_as_tuple(4, 5), std::forward_as_tuple(6, 7),
                std::forward_as_tuple(8, 9));

  EXPECT_TRUE(ptr_is_suitably_aligned(ptr));

  for (size_t elem = 0; elem < 5; elem++) {
    EXPECT_EQ(ptr[elem].x, 2 * elem);
    EXPECT_EQ(ptr[elem].y, 2 * elem + 1);
  }
  ut::delete_arr(ptr);
}
REGISTER_TYPED_TEST_SUITE_P(ut0new_new_delete_pod_types_arr, pod_types);
INSTANTIATE_TYPED_TEST_SUITE_P(My, ut0new_new_delete_pod_types_arr,
                               all_pod_types);

// new/delete - array specialization for non-pod types
template <typename T>
class ut0new_new_delete_non_pod_types_arr : public ::testing::Test {};
TYPED_TEST_SUITE_P(ut0new_new_delete_non_pod_types_arr);
TYPED_TEST_P(ut0new_new_delete_non_pod_types_arr, non_pod_types) {
  using type = typename TypeParam::type;
  auto with_pfs = TypeParam::with_pfs;
  EXPECT_EQ(type::get_count(), 0);
  type *ptr = with_pfs ? ut::new_arr_withkey<type>(
                             ut::make_psi_memory_key(pfs_key),
                             std::forward_as_tuple(1, 2, std::string("a")),
                             std::forward_as_tuple(3, 4, std::string("b")),
                             std::forward_as_tuple(5, 6, std::string("c")),
                             std::forward_as_tuple(7, 8, std::string("d")),
                             std::forward_as_tuple(9, 10, std::string("e")))
                       : ut::new_arr<type>(
                             std::forward_as_tuple(1, 2, std::string("a")),
                             std::forward_as_tuple(3, 4, std::string("b")),
                             std::forward_as_tuple(5, 6, std::string("c")),
                             std::forward_as_tuple(7, 8, std::string("d")),
                             std::forward_as_tuple(9, 10, std::string("e")));

  EXPECT_TRUE(ptr_is_suitably_aligned(ptr));

  EXPECT_EQ(ptr[0].x, 1);
  EXPECT_EQ(ptr[0].y, 2);
  EXPECT_TRUE(ptr[0].s == std::string("a"));
  EXPECT_EQ(ptr[0].sum.result, 3);

  EXPECT_EQ(ptr[1].x, 3);
  EXPECT_EQ(ptr[1].y, 4);
  EXPECT_TRUE(ptr[1].s == std::string("b"));
  EXPECT_EQ(ptr[1].sum.result, 7);

  EXPECT_EQ(ptr[2].x, 5);
  EXPECT_EQ(ptr[2].y, 6);
  EXPECT_TRUE(ptr[2].s == std::string("c"));
  EXPECT_EQ(ptr[2].sum.result, 11);

  EXPECT_EQ(ptr[3].x, 7);
  EXPECT_EQ(ptr[3].y, 8);
  EXPECT_TRUE(ptr[3].s == std::string("d"));
  EXPECT_EQ(ptr[3].sum.result, 15);

  EXPECT_EQ(ptr[4].x, 9);
  EXPECT_EQ(ptr[4].y, 10);
  EXPECT_TRUE(ptr[4].s == std::string("e"));
  EXPECT_EQ(ptr[4].sum.result, 19);

  EXPECT_EQ(type::get_count(), 5);

  ut::delete_arr(ptr);
  EXPECT_EQ(type::get_count(), 0);
}
REGISTER_TYPED_TEST_SUITE_P(ut0new_new_delete_non_pod_types_arr, non_pod_types);
INSTANTIATE_TYPED_TEST_SUITE_P(My, ut0new_new_delete_non_pod_types_arr,
                               all_non_pod_types);

// new/delete - array specialization for default constructible
// fundamental types
template <typename T>
class ut0new_new_delete_default_constructible_fundamental_types_arr
    : public ::testing::Test {};
TYPED_TEST_SUITE_P(
    ut0new_new_delete_default_constructible_fundamental_types_arr);
TYPED_TEST_P(ut0new_new_delete_default_constructible_fundamental_types_arr,
             fundamental_types) {
  using type = typename TypeParam::type;
  auto with_pfs = TypeParam::with_pfs;
  constexpr size_t n_elements = 5;
  type *ptr = with_pfs
                  ? ut::new_arr_withkey<type>(ut::make_psi_memory_key(pfs_key),
                                              ut::Count{n_elements})
                  : ut::new_arr<type>(ut::Count{n_elements});

  EXPECT_TRUE(ptr_is_suitably_aligned(ptr));

  for (size_t elem = 0; elem < n_elements; elem++) {
    EXPECT_EQ(ptr[elem], type{});
  }
  ut::delete_arr(ptr);
}
REGISTER_TYPED_TEST_SUITE_P(
    ut0new_new_delete_default_constructible_fundamental_types_arr,
    fundamental_types);
INSTANTIATE_TYPED_TEST_SUITE_P(
    My, ut0new_new_delete_default_constructible_fundamental_types_arr,
    all_fundamental_types);

// new/delete - array specialization for default constructible pod types
template <typename T>
class ut0new_new_delete_default_constructible_pod_types_arr
    : public ::testing::Test {};
TYPED_TEST_SUITE_P(ut0new_new_delete_default_constructible_pod_types_arr);
TYPED_TEST_P(ut0new_new_delete_default_constructible_pod_types_arr, pod_types) {
  using type = typename TypeParam::type;
  auto with_pfs = TypeParam::with_pfs;
  constexpr size_t n_elements = 5;
  type *ptr = with_pfs
                  ? ut::new_arr_withkey<type>(ut::make_psi_memory_key(pfs_key),
                                              ut::Count{n_elements})
                  : ut::new_arr<type>(ut::Count{n_elements});

  EXPECT_TRUE(ptr_is_suitably_aligned(ptr));

  for (size_t elem = 0; elem < n_elements; elem++) {
    EXPECT_EQ(ptr[elem].x, 0);
    EXPECT_EQ(ptr[elem].y, 1);
  }
  ut::delete_arr(ptr);
}
REGISTER_TYPED_TEST_SUITE_P(
    ut0new_new_delete_default_constructible_pod_types_arr, pod_types);
INSTANTIATE_TYPED_TEST_SUITE_P(
    My, ut0new_new_delete_default_constructible_pod_types_arr,
    all_default_constructible_pod_types);

// new/delete - array specialization for default constructible non-pod
// types
template <typename T>
class ut0new_new_delete_default_constructible_non_pod_types_arr
    : public ::testing::Test {};
TYPED_TEST_SUITE_P(ut0new_new_delete_default_constructible_non_pod_types_arr);
TYPED_TEST_P(ut0new_new_delete_default_constructible_non_pod_types_arr,
             pod_types) {
  using type = typename TypeParam::type;
  auto with_pfs = TypeParam::with_pfs;
  constexpr size_t n_elements = 5;
  type *ptr = with_pfs
                  ? ut::new_arr_withkey<type>(ut::make_psi_memory_key(pfs_key),
                                              ut::Count{n_elements})
                  : ut::new_arr<type>(ut::Count{n_elements});

  EXPECT_TRUE(ptr_is_suitably_aligned(ptr));

  for (size_t elem = 0; elem < n_elements; elem++) {
    EXPECT_EQ(ptr[elem].x, 0);
    EXPECT_EQ(ptr[elem].y, 1);
    EXPECT_TRUE(ptr[elem].s == std::string("non-pod-string"));
  }
  ut::delete_arr(ptr);
}
REGISTER_TYPED_TEST_SUITE_P(
    ut0new_new_delete_default_constructible_non_pod_types_arr, pod_types);
INSTANTIATE_TYPED_TEST_SUITE_P(
    My, ut0new_new_delete_default_constructible_non_pod_types_arr,
    all_default_constructible_non_pod_types);

TEST(ut0new_new_delete, unique_ptr_demo) {
  struct Int_deleter {
    void operator()(int *p) {
      std::cout << "Hello from custom deleter!\n";
      ut::delete_(p);
    }
  };
  std::unique_ptr<int, Int_deleter> ptr(ut::new_<int>(1), Int_deleter{});
}

TEST(ut0new_new_delete_arr, unique_ptr_demo) {
  struct Int_arr_deleter {
    void operator()(int *p) {
      std::cout << "Hello from custom deleter!\n";
      ut::delete_arr(p);
    }
  };
  std::unique_ptr<int, Int_arr_deleter> ptr(
      ut::new_arr<int>(std::forward_as_tuple(1), std::forward_as_tuple(2),
                       std::forward_as_tuple(3), std::forward_as_tuple(4),
                       std::forward_as_tuple(5)),
      Int_arr_deleter{});
}

TEST(ut0new_new_delete_arr, demo_with_non_default_constructible_types) {
  auto ptr = ut::new_arr_withkey<Pod_type>(
      ut::make_psi_memory_key(pfs_key), std::forward_as_tuple(1, 2),
      std::forward_as_tuple(3, 4), std::forward_as_tuple(5, 6),
      std::forward_as_tuple(7, 8), std::forward_as_tuple(9, 10));

  EXPECT_EQ(ptr[0].x, 1);
  EXPECT_EQ(ptr[0].y, 2);
  EXPECT_EQ(ptr[1].x, 3);
  EXPECT_EQ(ptr[1].y, 4);
  EXPECT_EQ(ptr[2].x, 5);
  EXPECT_EQ(ptr[2].y, 6);
  EXPECT_EQ(ptr[3].x, 7);
  EXPECT_EQ(ptr[3].y, 8);
  EXPECT_EQ(ptr[4].x, 9);
  EXPECT_EQ(ptr[4].y, 10);

  ut::delete_arr(ptr);
}

TEST(ut0new_new_delete_arr,
     demo_with_explicit_N_default_constructible_instances) {
  constexpr auto n_elements = 5;
  auto ptr = ut::new_arr_withkey<Default_constructible_pod>(
      ut::make_psi_memory_key(pfs_key), std::forward_as_tuple(),
      std::forward_as_tuple(), std::forward_as_tuple(), std::forward_as_tuple(),
      std::forward_as_tuple());

  for (size_t elem = 0; elem < n_elements; elem++) {
    EXPECT_EQ(ptr[elem].x, 0);
    EXPECT_EQ(ptr[elem].y, 1);
  }
  ut::delete_arr(ptr);
}

TEST(ut0new_new_delete_arr,
     demo_with_N_default_constructible_instances_through_ut_count) {
  constexpr auto n_elements = 5;
  auto ptr = ut::new_arr_withkey<Default_constructible_pod>(
      ut::make_psi_memory_key(pfs_key), ut::Count(n_elements));

  for (size_t elem = 0; elem < n_elements; elem++) {
    EXPECT_EQ(ptr[elem].x, 0);
    EXPECT_EQ(ptr[elem].y, 1);
  }
  ut::delete_arr(ptr);
}

TEST(
    ut0new_new_delete_arr,
    demo_with_type_that_is_both_default_constructible_and_constructible_through_user_provided_ctr) {
  struct Type {
    Type() : x(10), y(15) {}
    Type(int _x, int _y) : x(_x), y(_y) {}
    int x, y;
  };

  auto ptr = ut::new_arr_withkey<Type>(
      ut::make_psi_memory_key(pfs_key), std::forward_as_tuple(1, 2),
      std::forward_as_tuple(), std::forward_as_tuple(3, 4),
      std::forward_as_tuple(5, 6), std::forward_as_tuple());

  EXPECT_EQ(ptr[0].x, 1);
  EXPECT_EQ(ptr[0].y, 2);
  EXPECT_EQ(ptr[1].x, 10);
  EXPECT_EQ(ptr[1].y, 15);
  EXPECT_EQ(ptr[2].x, 3);
  EXPECT_EQ(ptr[2].y, 4);
  EXPECT_EQ(ptr[3].x, 5);
  EXPECT_EQ(ptr[3].y, 6);
  EXPECT_EQ(ptr[4].x, 10);
  EXPECT_EQ(ptr[4].y, 15);

  ut::delete_arr(ptr);
}

TEST(
    ut0new_new_delete_arr,
    destructors_of_successfully_instantiated_trivially_constructible_elements_are_invoked_when_one_of_the_element_constructors_throws) {
  static int n_constructors = 0;
  static int n_destructors = 0;

  struct Type_that_may_throw {
    Type_that_may_throw() {
      n_constructors++;
      if (n_constructors % 4 == 0) {
        throw std::runtime_error("cannot construct");
      }
    }
    ~Type_that_may_throw() { ++n_destructors; }
  };

  bool exception_thrown_and_caught = false;
  try {
    auto ptr = ut::new_arr_withkey<Type_that_may_throw>(
        ut::make_psi_memory_key(pfs_key), ut::Count(7));
    ASSERT_FALSE(ptr);
  } catch (std::runtime_error &) {
    exception_thrown_and_caught = true;
  }
  EXPECT_TRUE(exception_thrown_and_caught);
  EXPECT_EQ(n_constructors, 4);
  EXPECT_EQ(n_destructors, 3);
}

TEST(
    ut0new_new_delete_arr,
    destructors_of_successfully_instantiated_non_trivially_constructible_elements_are_invoked_when_one_of_the_element_constructors_throws) {
  static int n_constructors = 0;
  static int n_destructors = 0;

  struct Type_that_may_throw {
    Type_that_may_throw(int x, int y) : _x(x), _y(y) {
      n_constructors++;
      if (n_constructors % 4 == 0) {
        throw std::runtime_error("cannot construct");
      }
    }
    ~Type_that_may_throw() { ++n_destructors; }
    int _x, _y;
  };

  bool exception_thrown_and_caught = false;
  try {
    auto ptr = ut::new_arr_withkey<Type_that_may_throw>(
        ut::make_psi_memory_key(pfs_key), std::forward_as_tuple(0, 1),
        std::forward_as_tuple(2, 3), std::forward_as_tuple(4, 5),
        std::forward_as_tuple(6, 7), std::forward_as_tuple(8, 9));
    ASSERT_FALSE(ptr);
  } catch (std::runtime_error &) {
    exception_thrown_and_caught = true;
  }
  EXPECT_TRUE(exception_thrown_and_caught);
  EXPECT_EQ(n_constructors, 4);
  EXPECT_EQ(n_destructors, 3);
}

TEST(
    ut0new_new_delete_arr,
    no_destructors_are_invoked_when_first_trivially_constructible_element_constructor_throws) {
  static int n_constructors = 0;
  static int n_destructors = 0;

  struct Type_that_always_throws {
    Type_that_always_throws() {
      n_constructors++;
      throw std::runtime_error("cannot construct");
    }
    ~Type_that_always_throws() { ++n_destructors; }
  };

  bool exception_thrown_and_caught = false;
  try {
    auto ptr = ut::new_arr_withkey<Type_that_always_throws>(
        ut::make_psi_memory_key(pfs_key), ut::Count(7));
    ASSERT_FALSE(ptr);
  } catch (std::runtime_error &) {
    exception_thrown_and_caught = true;
  }
  EXPECT_TRUE(exception_thrown_and_caught);
  EXPECT_EQ(n_constructors, 1);
  EXPECT_EQ(n_destructors, 0);
}

TEST(
    ut0new_new_delete_arr,
    no_destructors_are_invoked_when_first_non_trivially_constructible_element_constructor_throws) {
  static int n_constructors = 0;
  static int n_destructors = 0;

  struct Type_that_always_throws {
    Type_that_always_throws(int x, int y) : _x(x), _y(y) {
      n_constructors++;
      throw std::runtime_error("cannot construct");
    }
    ~Type_that_always_throws() { ++n_destructors; }
    int _x, _y;
  };

  bool exception_thrown_and_caught = false;
  try {
    auto ptr = ut::new_arr_withkey<Type_that_always_throws>(
        ut::make_psi_memory_key(pfs_key), std::forward_as_tuple(0, 1),
        std::forward_as_tuple(2, 3), std::forward_as_tuple(4, 5),
        std::forward_as_tuple(6, 7), std::forward_as_tuple(8, 9));
    ASSERT_FALSE(ptr);
  } catch (std::runtime_error &) {
    exception_thrown_and_caught = true;
  }
  EXPECT_TRUE(exception_thrown_and_caught);
  EXPECT_EQ(n_constructors, 1);
  EXPECT_EQ(n_destructors, 0);
}

TEST(ut0new_new_delete,
     no_destructor_is_invoked_when_no_object_is_successfully_constructed) {
  static int n_constructors = 0;
  static int n_destructors = 0;

  struct Type_that_always_throws {
    Type_that_always_throws() {
      n_constructors++;
      throw std::runtime_error("cannot construct");
    }
    ~Type_that_always_throws() { ++n_destructors; }
  };

  bool exception_thrown_and_caught = false;
  try {
    auto ptr = ut::new_withkey<Type_that_always_throws>(
        ut::make_psi_memory_key(pfs_key));
    ASSERT_FALSE(ptr);
  } catch (std::runtime_error &) {
    exception_thrown_and_caught = true;
  }
  EXPECT_TRUE(exception_thrown_and_caught);
  EXPECT_EQ(n_constructors, 1);
  EXPECT_EQ(n_destructors, 0);
}

TEST(ut0new_new_delete, zero_sized_allocation_returns_valid_ptr) {
  auto ptr = ut::new_<byte>(0);
  EXPECT_NE(ptr, nullptr);
  ut::delete_(ptr);
}

TEST(ut0new_new_delete_arr, zero_sized_allocation_returns_valid_ptr) {
  auto ptr = ut::new_arr<byte>(ut::Count{0});
  EXPECT_NE(ptr, nullptr);
  ut::delete_arr(ptr);
}

TEST(ut0new_new_delete_arr,
     reference_types_are_automagically_handled_through_forward_as_tuple) {
  struct My_ref {
    My_ref(int &ref) : _ref(ref) {}
    int &_ref;
  };

  int y = 10;
  int x = 20;
  auto ptr = ut::new_arr_withkey<My_ref>(ut::make_psi_memory_key(pfs_key),
                                         std::forward_as_tuple(x),
                                         std::forward_as_tuple(y));
  EXPECT_EQ(ptr[0]._ref, x);
  EXPECT_EQ(ptr[1]._ref, y);
  x = 30;
  y = 40;
  EXPECT_EQ(ptr[0]._ref, x);
  EXPECT_EQ(ptr[1]._ref, y);

  ut::delete_arr(ptr);
}

TEST(ut0new_new_delete_arr,
     reference_types_are_automagically_handled_through_make_tuple_and_ref) {
  struct My_ref {
    My_ref(int &ref) : _ref(ref) {}
    int &_ref;
  };

  int y = 10;
  int x = 20;
  auto ptr = ut::new_arr_withkey<My_ref>(ut::make_psi_memory_key(pfs_key),
                                         std::make_tuple(std::ref(x)),
                                         std::make_tuple(std::ref(y)));
  EXPECT_EQ(ptr[0]._ref, x);
  EXPECT_EQ(ptr[1]._ref, y);
  x = 30;
  y = 40;
  EXPECT_EQ(ptr[0]._ref, x);
  EXPECT_EQ(ptr[1]._ref, y);

  ut::delete_arr(ptr);
}

TEST(ut0new_new_delete_arr, proper_overload_resolution_is_selected) {
  auto ptr = ut::new_arr_withkey<Pod_type>(ut::make_psi_memory_key(pfs_key),
                                           std::forward_as_tuple(1, 2));
  EXPECT_EQ(ptr[0].x, 1);
  EXPECT_EQ(ptr[0].y, 2);
  ut::delete_arr(ptr);
}

// Page-aligned alloc/free - fundamental types
template <typename T>
class ut0new_page_malloc_free_fundamental_types : public ::testing::Test {};
TYPED_TEST_SUITE_P(ut0new_page_malloc_free_fundamental_types);
TYPED_TEST_P(ut0new_page_malloc_free_fundamental_types, fundamental_types) {
  using type = typename TypeParam::type;
  auto with_pfs = TypeParam::with_pfs;
  type *ptr = with_pfs ? static_cast<type *>(ut::malloc_page_withkey(
                             ut::make_psi_memory_key(pfs_key), sizeof(type)))
                       : static_cast<type *>(ut::malloc_page(sizeof(type)));
  EXPECT_TRUE(ptr_is_suitably_aligned(ptr));
  EXPECT_GE(ut::page_allocation_size(ptr), sizeof(type));
  ut::free_page(ptr);
}
REGISTER_TYPED_TEST_SUITE_P(ut0new_page_malloc_free_fundamental_types,
                            fundamental_types);
INSTANTIATE_TYPED_TEST_SUITE_P(FundamentalTypes,
                               ut0new_page_malloc_free_fundamental_types,
                               all_fundamental_types);

// Page-aligned alloc/free - pod types
template <typename T>
class ut0new_page_malloc_free_pod_types : public ::testing::Test {};
TYPED_TEST_SUITE_P(ut0new_page_malloc_free_pod_types);
TYPED_TEST_P(ut0new_page_malloc_free_pod_types, pod_types) {
  using type = typename TypeParam::type;
  auto with_pfs = TypeParam::with_pfs;
  type *ptr = with_pfs ? static_cast<type *>(ut::malloc_page_withkey(
                             ut::make_psi_memory_key(pfs_key), sizeof(type)))
                       : static_cast<type *>(ut::malloc_page(sizeof(type)));
  EXPECT_TRUE(ptr_is_suitably_aligned(ptr));
  EXPECT_GE(ut::page_allocation_size(ptr), sizeof(type));
  ut::free_page(ptr);
}
REGISTER_TYPED_TEST_SUITE_P(ut0new_page_malloc_free_pod_types, pod_types);
INSTANTIATE_TYPED_TEST_SUITE_P(PodTypes, ut0new_page_malloc_free_pod_types,
                               all_pod_types);

// Page-aligned alloc/free - non-pod types
template <typename T>
class ut0new_page_malloc_free_non_pod_types : public ::testing::Test {};
TYPED_TEST_SUITE_P(ut0new_page_malloc_free_non_pod_types);
TYPED_TEST_P(ut0new_page_malloc_free_non_pod_types, non_pod_types) {
  using type = typename TypeParam::type;
  auto with_pfs = TypeParam::with_pfs;
  type *ptr = with_pfs ? static_cast<type *>(ut::malloc_page_withkey(
                             ut::make_psi_memory_key(pfs_key), sizeof(type)))
                       : static_cast<type *>(ut::malloc_page(sizeof(type)));
  EXPECT_TRUE(ptr_is_suitably_aligned(ptr));
  EXPECT_GE(ut::page_allocation_size(ptr), sizeof(type));
  // Referencing non-pod type members through returned pointer is UB.
  // Solely releasing it is ok.
  //
  // Using it otherwise is UB because ut::malloc_* functions are raw
  // memory management functions which do not invoke constructors neither
  // they know which type they are operating with. That is why we would be end
  // up accessing memory of not yet instantiated object (UB).
  ut::free_page(ptr);
}
REGISTER_TYPED_TEST_SUITE_P(ut0new_page_malloc_free_non_pod_types,
                            non_pod_types);
INSTANTIATE_TYPED_TEST_SUITE_P(NonPodTypes,
                               ut0new_page_malloc_free_non_pod_types,
                               all_non_pod_types);

// Allocating memory backed by huge (large) pages generally requires some prior
// system admin setup. When there's no such setup, we don't want to make our CI
// systems red. We skip the test instead and by doing it we are denoting that
// test was neither failed nor successful with the message explaining possible
// reasons. Solaris and OSX for example do not require any prior setup whereas
// Linux and Windows do.
#define SKIP_TEST_IF_HUGE_PAGE_SUPPORT_IS_NOT_AVAILABLE(ptr)       \
  if (!ptr)                                                        \
    GTEST_SKIP()                                                   \
        << "Huge-page support seems not to be enabled on this "    \
           "platform or underlying huge-page allocation function " \
           "was not used correctly. Please check. Skipping the test ...";

// Large-page alloc/free - fundamental types
template <typename T>
class ut0new_large_malloc_free_fundamental_types : public ::testing::Test {};
TYPED_TEST_SUITE_P(ut0new_large_malloc_free_fundamental_types);
TYPED_TEST_P(ut0new_large_malloc_free_fundamental_types, fundamental_types) {
  using type = typename TypeParam::type;
  auto with_pfs = TypeParam::with_pfs;
  type *ptr = with_pfs
                  ? static_cast<type *>(ut::malloc_large_page_withkey(
                        ut::make_psi_memory_key(pfs_key), sizeof(type)))
                  : static_cast<type *>(ut::malloc_large_page(sizeof(type)));
  SKIP_TEST_IF_HUGE_PAGE_SUPPORT_IS_NOT_AVAILABLE(ptr)
  EXPECT_TRUE(ptr_is_suitably_aligned(ptr));
  EXPECT_GE(ut::large_page_allocation_size(ptr), sizeof(type));
  EXPECT_GT(ut::large_page_low_level_info(ptr).allocation_size,
            ut::large_page_allocation_size(ptr));
  EXPECT_EQ(reinterpret_cast<std::uintptr_t>(
                ut::large_page_low_level_info(ptr).base_ptr) %
                CPU_PAGE_SIZE,
            0);
  ut::free_large_page(ptr);
}
REGISTER_TYPED_TEST_SUITE_P(ut0new_large_malloc_free_fundamental_types,
                            fundamental_types);
INSTANTIATE_TYPED_TEST_SUITE_P(FundamentalTypes,
                               ut0new_large_malloc_free_fundamental_types,
                               all_fundamental_types);

// Large-page alloc/free - pod types
template <typename T>
class ut0new_large_malloc_free_pod_types : public ::testing::Test {};
TYPED_TEST_SUITE_P(ut0new_large_malloc_free_pod_types);
TYPED_TEST_P(ut0new_large_malloc_free_pod_types, pod_types) {
  using type = typename TypeParam::type;
  auto with_pfs = TypeParam::with_pfs;
  type *ptr = with_pfs
                  ? static_cast<type *>(ut::malloc_large_page_withkey(
                        ut::make_psi_memory_key(pfs_key), sizeof(type)))
                  : static_cast<type *>(ut::malloc_large_page(sizeof(type)));
  SKIP_TEST_IF_HUGE_PAGE_SUPPORT_IS_NOT_AVAILABLE(ptr)
  EXPECT_TRUE(ptr_is_suitably_aligned(ptr));
  EXPECT_GE(ut::large_page_allocation_size(ptr), sizeof(type));
  EXPECT_GT(ut::large_page_low_level_info(ptr).allocation_size,
            ut::large_page_allocation_size(ptr));
  EXPECT_EQ(reinterpret_cast<std::uintptr_t>(
                ut::large_page_low_level_info(ptr).base_ptr) %
                CPU_PAGE_SIZE,
            0);
  ut::free_large_page(ptr);
}
REGISTER_TYPED_TEST_SUITE_P(ut0new_large_malloc_free_pod_types, pod_types);
INSTANTIATE_TYPED_TEST_SUITE_P(PodTypes, ut0new_large_malloc_free_pod_types,
                               all_pod_types);

// Large-page alloc/free - non-pod types
template <typename T>
class ut0new_large_malloc_free_non_pod_types : public ::testing::Test {};
TYPED_TEST_SUITE_P(ut0new_large_malloc_free_non_pod_types);
TYPED_TEST_P(ut0new_large_malloc_free_non_pod_types, non_pod_types) {
  using type = typename TypeParam::type;
  auto with_pfs = TypeParam::with_pfs;
  type *ptr = with_pfs
                  ? static_cast<type *>(ut::malloc_large_page_withkey(
                        ut::make_psi_memory_key(pfs_key), sizeof(type)))
                  : static_cast<type *>(ut::malloc_large_page(sizeof(type)));
  SKIP_TEST_IF_HUGE_PAGE_SUPPORT_IS_NOT_AVAILABLE(ptr)
  EXPECT_TRUE(ptr_is_suitably_aligned(ptr));
  EXPECT_GE(ut::large_page_allocation_size(ptr), sizeof(type));
  EXPECT_GT(ut::large_page_low_level_info(ptr).allocation_size,
            ut::large_page_allocation_size(ptr));
  EXPECT_EQ(reinterpret_cast<std::uintptr_t>(
                ut::large_page_low_level_info(ptr).base_ptr) %
                CPU_PAGE_SIZE,
            0);
  // Referencing non-pod type members through returned pointer is UB.
  // Solely releasing it is ok.
  //
  // Using it otherwise is UB because ut::large_malloc_* functions are raw
  // memory management functions which do not invoke constructors neither
  // they know which type they are operating with. That is why we would be end
  // up accessing memory of not yet instantiated object (UB).
  ut::free_large_page(ptr);
}
REGISTER_TYPED_TEST_SUITE_P(ut0new_large_malloc_free_non_pod_types,
                            non_pod_types);
INSTANTIATE_TYPED_TEST_SUITE_P(NonPodTypes,
                               ut0new_large_malloc_free_non_pod_types,
                               all_non_pod_types);

// Large-page alloc/free with page-aligned fallback - fundamental types
template <typename T>
class ut0new_large_malloc_free_fallback_fundamental_types
    : public ::testing::Test {};
TYPED_TEST_SUITE_P(ut0new_large_malloc_free_fallback_fundamental_types);
TYPED_TEST_P(ut0new_large_malloc_free_fallback_fundamental_types,
             fundamental_types) {
  using type = typename TypeParam::type;
  auto with_pfs = TypeParam::with_pfs;
  for (auto default_cfg : {true, false}) {
    auto ret = with_pfs ? ut::malloc_large_page_withkey(
                              ut::make_psi_memory_key(pfs_key), sizeof(type),
                              ut::fallback_to_normal_page_t{}, default_cfg)
                        : ut::malloc_large_page(sizeof(type),
                                                ut::fallback_to_normal_page_t{},
                                                default_cfg);
    EXPECT_TRUE(reinterpret_cast<std::uintptr_t>(ret) % CPU_PAGE_SIZE == 0);
    EXPECT_GE(
        ut::large_page_allocation_size(ret, ut::fallback_to_normal_page_t{}),
        sizeof(type));
    EXPECT_GT(
        ut::large_page_low_level_info(ret, ut::fallback_to_normal_page_t{})
            .allocation_size,
        ut::large_page_allocation_size(ret, ut::fallback_to_normal_page_t{}));
    EXPECT_EQ(
        reinterpret_cast<std::uintptr_t>(
            ut::large_page_low_level_info(ret, ut::fallback_to_normal_page_t{})
                .base_ptr) %
            CPU_PAGE_SIZE,
        0);
    ut::free_large_page(ret, ut::fallback_to_normal_page_t{});
  }
}
REGISTER_TYPED_TEST_SUITE_P(ut0new_large_malloc_free_fallback_fundamental_types,
                            fundamental_types);
INSTANTIATE_TYPED_TEST_SUITE_P(
    FundamentalTypes, ut0new_large_malloc_free_fallback_fundamental_types,
    all_fundamental_types);

// Large-page alloc/free with page-aligned fallback - pod types
template <typename T>
class ut0new_large_malloc_free_fallback_pod_types : public ::testing::Test {};
TYPED_TEST_SUITE_P(ut0new_large_malloc_free_fallback_pod_types);
TYPED_TEST_P(ut0new_large_malloc_free_fallback_pod_types, pod_types) {
  using type = typename TypeParam::type;
  auto with_pfs = TypeParam::with_pfs;
  for (auto default_cfg : {true, false}) {
    auto ret = with_pfs ? ut::malloc_large_page_withkey(
                              ut::make_psi_memory_key(pfs_key), sizeof(type),
                              ut::fallback_to_normal_page_t{}, default_cfg)
                        : ut::malloc_large_page(sizeof(type),
                                                ut::fallback_to_normal_page_t{},
                                                default_cfg);
    EXPECT_TRUE(reinterpret_cast<std::uintptr_t>(ret) % CPU_PAGE_SIZE == 0);
    EXPECT_GE(
        ut::large_page_allocation_size(ret, ut::fallback_to_normal_page_t{}),
        sizeof(type));
    EXPECT_GT(
        ut::large_page_low_level_info(ret, ut::fallback_to_normal_page_t{})
            .allocation_size,
        ut::large_page_allocation_size(ret, ut::fallback_to_normal_page_t{}));
    EXPECT_EQ(
        reinterpret_cast<std::uintptr_t>(
            ut::large_page_low_level_info(ret, ut::fallback_to_normal_page_t{})
                .base_ptr) %
            CPU_PAGE_SIZE,
        0);
    ut::free_large_page(ret, ut::fallback_to_normal_page_t{});
  }
}
REGISTER_TYPED_TEST_SUITE_P(ut0new_large_malloc_free_fallback_pod_types,
                            pod_types);
INSTANTIATE_TYPED_TEST_SUITE_P(PodTypes,
                               ut0new_large_malloc_free_fallback_pod_types,
                               all_pod_types);

// Large-page alloc/free with page-aligned fallback - non-pod types
template <typename T>
class ut0new_large_malloc_free_fallback_non_pod_types : public ::testing::Test {
};
TYPED_TEST_SUITE_P(ut0new_large_malloc_free_fallback_non_pod_types);
TYPED_TEST_P(ut0new_large_malloc_free_fallback_non_pod_types, non_pod_types) {
  using type = typename TypeParam::type;
  auto with_pfs = TypeParam::with_pfs;
  for (auto default_cfg : {true, false}) {
    auto ret = with_pfs ? ut::malloc_large_page_withkey(
                              ut::make_psi_memory_key(pfs_key), sizeof(type),
                              ut::fallback_to_normal_page_t{}, default_cfg)
                        : ut::malloc_large_page(sizeof(type),
                                                ut::fallback_to_normal_page_t{},
                                                default_cfg);
    EXPECT_TRUE(reinterpret_cast<std::uintptr_t>(ret) % CPU_PAGE_SIZE == 0);
    EXPECT_GE(
        ut::large_page_allocation_size(ret, ut::fallback_to_normal_page_t{}),
        sizeof(type));
    EXPECT_GT(
        ut::large_page_low_level_info(ret, ut::fallback_to_normal_page_t{})
            .allocation_size,
        ut::large_page_allocation_size(ret, ut::fallback_to_normal_page_t{}));
    EXPECT_EQ(
        reinterpret_cast<std::uintptr_t>(
            ut::large_page_low_level_info(ret, ut::fallback_to_normal_page_t{})
                .base_ptr) %
            CPU_PAGE_SIZE,
        0);
    // Referencing non-pod type members through returned pointer is UB.
    // Solely releasing it is ok.
    //
    // Using it otherwise is UB because ut::large_malloc_* functions are raw
    // memory management functions which do not invoke constructors neither
    // they know which type they are operating with. That is why we would be end
    // up accessing memory of not yet instantiated object (UB).
    ut::free_large_page(ret, ut::fallback_to_normal_page_t{});
  }
}
REGISTER_TYPED_TEST_SUITE_P(ut0new_large_malloc_free_fallback_non_pod_types,
                            non_pod_types);
INSTANTIATE_TYPED_TEST_SUITE_P(NonPodTypes,
                               ut0new_large_malloc_free_fallback_non_pod_types,
                               all_non_pod_types);

// aligned alloc/free - fundamental types
template <typename T>
class aligned_alloc_free_fundamental_types : public ::testing::Test {};
TYPED_TEST_SUITE_P(aligned_alloc_free_fundamental_types);
TYPED_TEST_P(aligned_alloc_free_fundamental_types, fundamental_types) {
  using type = typename TypeParam::type;
  auto with_pfs = TypeParam::with_pfs;
  for (auto alignment = 2 * alignof(std::max_align_t);
       alignment < 1024 * 1024 + 1; alignment *= 2) {
    type *ptr =
        with_pfs
            ? static_cast<type *>(ut::aligned_alloc_withkey(
                  ut::make_psi_memory_key(pfs_key), sizeof(type), alignment))
            : static_cast<type *>(ut::aligned_alloc(sizeof(type), alignment));
    EXPECT_TRUE(reinterpret_cast<std::uintptr_t>(ptr) % alignment == 0);
    ut::aligned_free(ptr);
  }
}
REGISTER_TYPED_TEST_SUITE_P(aligned_alloc_free_fundamental_types,
                            fundamental_types);
INSTANTIATE_TYPED_TEST_SUITE_P(FundamentalTypes,
                               aligned_alloc_free_fundamental_types,
                               all_fundamental_types);

// aligned alloc/free - pod types
template <typename T>
class aligned_alloc_free_pod_types : public ::testing::Test {};
TYPED_TEST_SUITE_P(aligned_alloc_free_pod_types);
TYPED_TEST_P(aligned_alloc_free_pod_types, pod_types) {
  using type = typename TypeParam::type;
  auto with_pfs = TypeParam::with_pfs;
  auto alignment = 4 * 1024;
  type *ptr =
      with_pfs
          ? static_cast<type *>(ut::aligned_alloc_withkey(
                ut::make_psi_memory_key(pfs_key), sizeof(type), alignment))
          : static_cast<type *>(ut::aligned_alloc(sizeof(type), alignment));
  EXPECT_TRUE(reinterpret_cast<std::uintptr_t>(ptr) % alignment == 0);
  ut::aligned_free(ptr);
}
REGISTER_TYPED_TEST_SUITE_P(aligned_alloc_free_pod_types, pod_types);
INSTANTIATE_TYPED_TEST_SUITE_P(PodTypes, aligned_alloc_free_pod_types,
                               all_pod_types);

// aligned alloc/free - non-pod types
template <typename T>
class aligned_alloc_free_non_pod_types : public ::testing::Test {};
TYPED_TEST_SUITE_P(aligned_alloc_free_non_pod_types);
TYPED_TEST_P(aligned_alloc_free_non_pod_types, non_pod_types) {
  using type = typename TypeParam::type;
  auto with_pfs = TypeParam::with_pfs;
  auto alignment = 4 * 1024;
  type *ptr =
      with_pfs
          ? static_cast<type *>(ut::aligned_alloc_withkey(
                ut::make_psi_memory_key(pfs_key), sizeof(type), alignment))
          : static_cast<type *>(ut::aligned_alloc(sizeof(type), alignment));
  EXPECT_TRUE(reinterpret_cast<std::uintptr_t>(ptr) % alignment == 0);
  // Referencing non-pod type members through returned pointer is UB.
  // Solely releasing it is ok.
  //
  // Using it otherwise is UB because aligned_alloc_* functions are raw
  // memory management functions which do not invoke constructors neither
  // they know which type they are operating with. That is why we would be end
  // up accessing memory of not yet instantiated object (UB).
  ut::aligned_free(ptr);
}
REGISTER_TYPED_TEST_SUITE_P(aligned_alloc_free_non_pod_types, non_pod_types);
INSTANTIATE_TYPED_TEST_SUITE_P(NonPodTypes, aligned_alloc_free_non_pod_types,
                               all_non_pod_types);

// aligned new/delete - fundamental types
template <typename T>
class aligned_new_delete_fundamental_types : public ::testing::Test {};
TYPED_TEST_SUITE_P(aligned_new_delete_fundamental_types);
TYPED_TEST_P(aligned_new_delete_fundamental_types, fundamental_types) {
  using type = typename TypeParam::type;
  auto with_pfs = TypeParam::with_pfs;
  for (auto alignment = 2 * alignof(std::max_align_t);
       alignment < 1024 * 1024 + 1; alignment *= 2) {
    type *ptr = with_pfs ? ut::aligned_new_withkey<type>(
                               ut::make_psi_memory_key(pfs_key), alignment, 1)
                         : ut::aligned_new<type>(alignment, 1);
    EXPECT_TRUE(reinterpret_cast<std::uintptr_t>(ptr) % alignment == 0);
    EXPECT_EQ(*ptr, 1);
    ut::aligned_delete(ptr);
  }
}
REGISTER_TYPED_TEST_SUITE_P(aligned_new_delete_fundamental_types,
                            fundamental_types);
INSTANTIATE_TYPED_TEST_SUITE_P(My, aligned_new_delete_fundamental_types,
                               all_fundamental_types);

// aligned new/delete - pod types
template <typename T>
class aligned_new_delete_pod_types : public ::testing::Test {};
TYPED_TEST_SUITE_P(aligned_new_delete_pod_types);
TYPED_TEST_P(aligned_new_delete_pod_types, pod_types) {
  using type = typename TypeParam::type;
  auto with_pfs = TypeParam::with_pfs;

  for (auto alignment = 2 * alignof(std::max_align_t);
       alignment < 1024 * 1024 + 1; alignment *= 2) {
    type *ptr = with_pfs
                    ? ut::aligned_new_withkey<type>(
                          ut::make_psi_memory_key(pfs_key), alignment, 2, 5)
                    : ut::aligned_new<type>(alignment, 2, 5);
    EXPECT_TRUE(reinterpret_cast<std::uintptr_t>(ptr) % alignment == 0);
    EXPECT_EQ(ptr->x, 2);
    EXPECT_EQ(ptr->y, 5);
    ut::aligned_delete(ptr);
  }
}
REGISTER_TYPED_TEST_SUITE_P(aligned_new_delete_pod_types, pod_types);
INSTANTIATE_TYPED_TEST_SUITE_P(My, aligned_new_delete_pod_types, all_pod_types);

// aligned new/delete - non-pod types
template <typename T>
class aligned_new_delete_non_pod_types : public ::testing::Test {};
TYPED_TEST_SUITE_P(aligned_new_delete_non_pod_types);
TYPED_TEST_P(aligned_new_delete_non_pod_types, non_pod_types) {
  using type = typename TypeParam::type;
  auto with_pfs = TypeParam::with_pfs;
  for (auto alignment = 2 * alignof(std::max_align_t);
       alignment < 1024 * 1024 + 1; alignment *= 2) {
    EXPECT_EQ(type::get_count(), 0);
    type *ptr =
        with_pfs
            ? ut::aligned_new_withkey<type>(ut::make_psi_memory_key(pfs_key),
                                            alignment, 2, 5,
                                            std::string("non-pod"))
            : ut::aligned_new<type>(alignment, 2, 5, std::string("non-pod"));
    EXPECT_TRUE(reinterpret_cast<std::uintptr_t>(ptr) % alignment == 0);
    EXPECT_EQ(ptr->x, 2);
    EXPECT_EQ(ptr->y, 5);
    EXPECT_EQ(ptr->sum.result, 7);
    EXPECT_EQ(ptr->s, std::string("non-pod"));
    EXPECT_EQ(type::get_count(), 1);
    ut::aligned_delete(ptr);
    EXPECT_EQ(type::get_count(), 0);
  }
}
REGISTER_TYPED_TEST_SUITE_P(aligned_new_delete_non_pod_types, non_pod_types);
INSTANTIATE_TYPED_TEST_SUITE_P(My, aligned_new_delete_non_pod_types,
                               all_non_pod_types);

// aligned new/delete - array specialization for fundamental types
template <typename T>
class aligned_new_delete_fundamental_types_arr : public ::testing::Test {};
TYPED_TEST_SUITE_P(aligned_new_delete_fundamental_types_arr);
TYPED_TEST_P(aligned_new_delete_fundamental_types_arr, fundamental_types) {
  using type = typename TypeParam::type;
  auto with_pfs = TypeParam::with_pfs;
  constexpr size_t n_elements = 10;
  for (auto alignment = 2 * alignof(std::max_align_t);
       alignment < 1024 * 1024 + 1; alignment *= 2) {
    type *ptr = with_pfs ? ut::aligned_new_arr_withkey<type>(
                               ut::make_psi_memory_key(pfs_key), alignment,
                               std::forward_as_tuple((type)0),
                               std::forward_as_tuple((type)1),
                               std::forward_as_tuple((type)2),
                               std::forward_as_tuple((type)3),
                               std::forward_as_tuple((type)4),
                               std::forward_as_tuple((type)5),
                               std::forward_as_tuple((type)6),
                               std::forward_as_tuple((type)7),
                               std::forward_as_tuple((type)8),
                               std::forward_as_tuple((type)9))
                         : ut::aligned_new_arr<type>(
                               alignment, std::forward_as_tuple((type)0),
                               std::forward_as_tuple((type)1),
                               std::forward_as_tuple((type)2),
                               std::forward_as_tuple((type)3),
                               std::forward_as_tuple((type)4),
                               std::forward_as_tuple((type)5),
                               std::forward_as_tuple((type)6),
                               std::forward_as_tuple((type)7),
                               std::forward_as_tuple((type)8),
                               std::forward_as_tuple((type)9));
    EXPECT_TRUE(reinterpret_cast<std::uintptr_t>(ptr) % alignment == 0);

    for (size_t elem = 0; elem < n_elements; elem++) {
      EXPECT_EQ(ptr[elem], elem);
    }
    ut::aligned_delete_arr(ptr);
  }
}
REGISTER_TYPED_TEST_SUITE_P(aligned_new_delete_fundamental_types_arr,
                            fundamental_types);
INSTANTIATE_TYPED_TEST_SUITE_P(My, aligned_new_delete_fundamental_types_arr,
                               all_fundamental_types);

// aligned new/delete - array specialization for pod types
template <typename T>
class aligned_new_delete_pod_types_arr : public ::testing::Test {};
TYPED_TEST_SUITE_P(aligned_new_delete_pod_types_arr);
TYPED_TEST_P(aligned_new_delete_pod_types_arr, pod_types) {
  using type = typename TypeParam::type;
  auto with_pfs = TypeParam::with_pfs;
  constexpr size_t n_elements = 5;
  for (auto alignment = 2 * alignof(std::max_align_t);
       alignment < 1024 * 1024 + 1; alignment *= 2) {
    type *ptr =
        with_pfs
            ? ut::aligned_new_arr_withkey<type>(
                  ut::make_psi_memory_key(pfs_key), alignment,
                  std::forward_as_tuple(0, 1), std::forward_as_tuple(2, 3),
                  std::forward_as_tuple(4, 5), std::forward_as_tuple(6, 7),
                  std::forward_as_tuple(8, 9))
            : ut::aligned_new_arr<type>(
                  alignment, std::forward_as_tuple(0, 1),
                  std::forward_as_tuple(2, 3), std::forward_as_tuple(4, 5),
                  std::forward_as_tuple(6, 7), std::forward_as_tuple(8, 9));

    EXPECT_TRUE(reinterpret_cast<std::uintptr_t>(ptr) % alignment == 0);

    for (size_t elem = 0; elem < n_elements; elem++) {
      EXPECT_EQ(ptr[elem].x, 2 * elem);
      EXPECT_EQ(ptr[elem].y, 2 * elem + 1);
    }
    ut::aligned_delete_arr(ptr);
  }
}
REGISTER_TYPED_TEST_SUITE_P(aligned_new_delete_pod_types_arr, pod_types);
INSTANTIATE_TYPED_TEST_SUITE_P(My, aligned_new_delete_pod_types_arr,
                               all_pod_types);

// aligned new/delete - array specialization for non-pod types
template <typename T>
class aligned_new_delete_non_pod_types_arr : public ::testing::Test {};
TYPED_TEST_SUITE_P(aligned_new_delete_non_pod_types_arr);
TYPED_TEST_P(aligned_new_delete_non_pod_types_arr, non_pod_types) {
  using type = typename TypeParam::type;
  auto with_pfs = TypeParam::with_pfs;
  for (auto alignment = 2 * alignof(std::max_align_t);
       alignment < 1024 * 1024 + 1; alignment *= 2) {
    EXPECT_EQ(type::get_count(), 0);
    type *ptr =
        with_pfs ? ut::aligned_new_arr_withkey<type>(
                       ut::make_psi_memory_key(pfs_key), alignment,
                       std::forward_as_tuple(1, 2, std::string("a")),
                       std::forward_as_tuple(3, 4, std::string("b")),
                       std::forward_as_tuple(5, 6, std::string("c")),
                       std::forward_as_tuple(7, 8, std::string("d")),
                       std::forward_as_tuple(9, 10, std::string("e")))
                 : ut::aligned_new_arr<type>(
                       alignment, std::forward_as_tuple(1, 2, std::string("a")),
                       std::forward_as_tuple(3, 4, std::string("b")),
                       std::forward_as_tuple(5, 6, std::string("c")),
                       std::forward_as_tuple(7, 8, std::string("d")),
                       std::forward_as_tuple(9, 10, std::string("e")));

    EXPECT_TRUE(reinterpret_cast<std::uintptr_t>(ptr) % alignment == 0);

    EXPECT_EQ(ptr[0].x, 1);
    EXPECT_EQ(ptr[0].y, 2);
    EXPECT_TRUE(ptr[0].s == std::string("a"));
    EXPECT_EQ(ptr[0].sum.result, 3);

    EXPECT_EQ(ptr[1].x, 3);
    EXPECT_EQ(ptr[1].y, 4);
    EXPECT_TRUE(ptr[1].s == std::string("b"));
    EXPECT_EQ(ptr[1].sum.result, 7);

    EXPECT_EQ(ptr[2].x, 5);
    EXPECT_EQ(ptr[2].y, 6);
    EXPECT_TRUE(ptr[2].s == std::string("c"));
    EXPECT_EQ(ptr[2].sum.result, 11);

    EXPECT_EQ(ptr[3].x, 7);
    EXPECT_EQ(ptr[3].y, 8);
    EXPECT_TRUE(ptr[3].s == std::string("d"));
    EXPECT_EQ(ptr[3].sum.result, 15);

    EXPECT_EQ(ptr[4].x, 9);
    EXPECT_EQ(ptr[4].y, 10);
    EXPECT_TRUE(ptr[4].s == std::string("e"));
    EXPECT_EQ(ptr[4].sum.result, 19);

    EXPECT_EQ(type::get_count(), 5);
    ut::aligned_delete_arr(ptr);
    EXPECT_EQ(type::get_count(), 0);
  }
}
REGISTER_TYPED_TEST_SUITE_P(aligned_new_delete_non_pod_types_arr,
                            non_pod_types);
INSTANTIATE_TYPED_TEST_SUITE_P(My, aligned_new_delete_non_pod_types_arr,
                               all_non_pod_types);

// aligned new/delete - array specialization for default constructible
// fundamental types
template <typename T>
class aligned_new_delete_default_constructible_fundamental_types_arr
    : public ::testing::Test {};
TYPED_TEST_SUITE_P(
    aligned_new_delete_default_constructible_fundamental_types_arr);
TYPED_TEST_P(aligned_new_delete_default_constructible_fundamental_types_arr,
             fundamental_types) {
  using type = typename TypeParam::type;
  auto with_pfs = TypeParam::with_pfs;
  constexpr size_t n_elements = 5;
  for (auto alignment = 2 * alignof(std::max_align_t);
       alignment < 1024 * 1024 + 1; alignment *= 2) {
    type *ptr =
        with_pfs ? ut::aligned_new_arr_withkey<type>(
                       ut::make_psi_memory_key(pfs_key), alignment,
                       ut::Count{n_elements})
                 : ut::aligned_new_arr<type>(alignment, ut::Count{n_elements});

    EXPECT_TRUE(reinterpret_cast<std::uintptr_t>(ptr) % alignment == 0);

    for (size_t elem = 0; elem < n_elements; elem++) {
      EXPECT_EQ(ptr[elem], type{});
    }
    ut::aligned_delete_arr(ptr);
  }
}
REGISTER_TYPED_TEST_SUITE_P(
    aligned_new_delete_default_constructible_fundamental_types_arr,
    fundamental_types);
INSTANTIATE_TYPED_TEST_SUITE_P(
    My, aligned_new_delete_default_constructible_fundamental_types_arr,
    all_fundamental_types);

// aligned new/delete - array specialization for default constructible pod types
template <typename T>
class aligned_new_delete_default_constructible_pod_types_arr
    : public ::testing::Test {};
TYPED_TEST_SUITE_P(aligned_new_delete_default_constructible_pod_types_arr);
TYPED_TEST_P(aligned_new_delete_default_constructible_pod_types_arr,
             pod_types) {
  using type = typename TypeParam::type;
  auto with_pfs = TypeParam::with_pfs;
  constexpr size_t n_elements = 5;
  for (auto alignment = 2 * alignof(std::max_align_t);
       alignment < 1024 * 1024 + 1; alignment *= 2) {
    type *ptr =
        with_pfs ? ut::aligned_new_arr_withkey<type>(
                       ut::make_psi_memory_key(pfs_key), alignment,
                       ut::Count{n_elements})
                 : ut::aligned_new_arr<type>(alignment, ut::Count{n_elements});

    EXPECT_TRUE(reinterpret_cast<std::uintptr_t>(ptr) % alignment == 0);

    for (size_t elem = 0; elem < n_elements; elem++) {
      EXPECT_EQ(ptr[elem].x, 0);
      EXPECT_EQ(ptr[elem].y, 1);
    }
    ut::aligned_delete_arr(ptr);
  }
}
REGISTER_TYPED_TEST_SUITE_P(
    aligned_new_delete_default_constructible_pod_types_arr, pod_types);
INSTANTIATE_TYPED_TEST_SUITE_P(
    My, aligned_new_delete_default_constructible_pod_types_arr,
    all_default_constructible_pod_types);

// aligned new/delete - array specialization for default constructible non-pod
// types
template <typename T>
class aligned_new_delete_default_constructible_non_pod_types_arr
    : public ::testing::Test {};
TYPED_TEST_SUITE_P(aligned_new_delete_default_constructible_non_pod_types_arr);
TYPED_TEST_P(aligned_new_delete_default_constructible_non_pod_types_arr,
             pod_types) {
  using type = typename TypeParam::type;
  auto with_pfs = TypeParam::with_pfs;
  constexpr size_t n_elements = 5;
  for (auto alignment = 2 * alignof(std::max_align_t);
       alignment < 1024 * 1024 + 1; alignment *= 2) {
    type *ptr =
        with_pfs ? ut::aligned_new_arr_withkey<type>(
                       ut::make_psi_memory_key(pfs_key), alignment,
                       ut::Count{n_elements})
                 : ut::aligned_new_arr<type>(alignment, ut::Count{n_elements});

    EXPECT_TRUE(reinterpret_cast<std::uintptr_t>(ptr) % alignment == 0);

    for (size_t elem = 0; elem < n_elements; elem++) {
      EXPECT_EQ(ptr[elem].x, 0);
      EXPECT_EQ(ptr[elem].y, 1);
      EXPECT_TRUE(ptr[elem].s == std::string("non-pod-string"));
    }
    ut::aligned_delete_arr(ptr);
  }
}
REGISTER_TYPED_TEST_SUITE_P(
    aligned_new_delete_default_constructible_non_pod_types_arr, pod_types);
INSTANTIATE_TYPED_TEST_SUITE_P(
    My, aligned_new_delete_default_constructible_non_pod_types_arr,
    all_default_constructible_non_pod_types);

TEST(aligned_new_delete, unique_ptr_demo) {
  constexpr auto alignment = 4 * 1024;
  struct Aligned_int_deleter {
    void operator()(int *p) {
      std::cout << "Hello from custom deleter!\n";
      ut::aligned_delete(p);
    }
  };
  std::unique_ptr<int, Aligned_int_deleter> ptr(
      ut::aligned_new<int>(alignment, 1), Aligned_int_deleter{});
}

TEST(aligned_new_delete_arr, unique_ptr_demo) {
  constexpr auto alignment = 4 * 1024;
  struct Aligned_int_arr_deleter {
    void operator()(int *p) {
      std::cout << "Hello from custom deleter!\n";
      ut::aligned_delete_arr(p);
    }
  };
  std::unique_ptr<int, Aligned_int_arr_deleter> ptr(
      ut::aligned_new_arr<int>(
          alignment, std::forward_as_tuple(1), std::forward_as_tuple(2),
          std::forward_as_tuple(3), std::forward_as_tuple(4),
          std::forward_as_tuple(5)),
      Aligned_int_arr_deleter{});
}

TEST(aligned_new_delete_arr, distance_between_elements_in_arr) {
  using type = Default_constructible_pod;
  constexpr size_t n_elements = 5;
  for (auto alignment = 2 * alignof(std::max_align_t);
       alignment < 1024 * 1024 + 1; alignment *= 2) {
    type *ptr = ut::aligned_new_arr<type>(alignment, ut::Count{n_elements});

    EXPECT_TRUE(reinterpret_cast<std::uintptr_t>(ptr) % alignment == 0);

    for (size_t elem = 1; elem < n_elements; elem++) {
      auto addr_curr = reinterpret_cast<std::uintptr_t>(&ptr[elem]);
      auto addr_prev = reinterpret_cast<std::uintptr_t>(&ptr[elem - 1]);
      auto distance = addr_curr - addr_prev;
      EXPECT_EQ(distance, sizeof(type));
    }
    ut::aligned_delete_arr(ptr);
  }
}

TEST(
    ut0new_aligned_new_delete_arr,
    destructors_of_successfully_instantiated_trivially_constructible_elements_are_invoked_when_one_of_the_element_constructors_throws) {
  static int n_constructors = 0;
  static int n_destructors = 0;

  struct Type_that_may_throw {
    Type_that_may_throw() {
      n_constructors++;
      if (n_constructors % 4 == 0) {
        throw std::runtime_error("cannot construct");
      }
    }
    ~Type_that_may_throw() { ++n_destructors; }
  };

  bool exception_thrown_and_caught = false;
  try {
    auto ptr = ut::aligned_new_arr_withkey<Type_that_may_throw>(
        ut::make_psi_memory_key(pfs_key), 2 * alignof(std::max_align_t),
        ut::Count(7));
    ASSERT_FALSE(ptr);
  } catch (std::runtime_error &) {
    exception_thrown_and_caught = true;
  }
  EXPECT_TRUE(exception_thrown_and_caught);
  EXPECT_EQ(n_constructors, 4);
  EXPECT_EQ(n_destructors, 3);
}

TEST(
    ut0new_aligned_new_delete_arr,
    destructors_of_successfully_instantiated_non_trivially_constructible_elements_are_invoked_when_one_of_the_element_constructors_throws) {
  static int n_constructors = 0;
  static int n_destructors = 0;

  struct Type_that_may_throw {
    Type_that_may_throw(int x, int y) : _x(x), _y(y) {
      n_constructors++;
      if (n_constructors % 4 == 0) {
        throw std::runtime_error("cannot construct");
      }
    }
    ~Type_that_may_throw() { ++n_destructors; }
    int _x, _y;
  };

  bool exception_thrown_and_caught = false;
  try {
    auto ptr = ut::aligned_new_arr_withkey<Type_that_may_throw>(
        ut::make_psi_memory_key(pfs_key), 2 * alignof(std::max_align_t),
        std::forward_as_tuple(0, 1), std::forward_as_tuple(2, 3),
        std::forward_as_tuple(4, 5), std::forward_as_tuple(6, 7),
        std::forward_as_tuple(8, 9));
    ASSERT_FALSE(ptr);
  } catch (std::runtime_error &) {
    exception_thrown_and_caught = true;
  }
  EXPECT_TRUE(exception_thrown_and_caught);
  EXPECT_EQ(n_constructors, 4);
  EXPECT_EQ(n_destructors, 3);
}

TEST(
    ut0new_aligned_new_delete_arr,
    no_destructors_are_invoked_when_first_trivially_constructible_element_constructor_throws) {
  static int n_constructors = 0;
  static int n_destructors = 0;

  struct Type_that_always_throws {
    Type_that_always_throws() {
      n_constructors++;
      throw std::runtime_error("cannot construct");
    }
    ~Type_that_always_throws() { ++n_destructors; }
  };

  bool exception_thrown_and_caught = false;
  try {
    auto ptr = ut::aligned_new_arr_withkey<Type_that_always_throws>(
        ut::make_psi_memory_key(pfs_key), 2 * alignof(std::max_align_t),
        ut::Count(7));
    ASSERT_FALSE(ptr);
  } catch (std::runtime_error &) {
    exception_thrown_and_caught = true;
  }
  EXPECT_TRUE(exception_thrown_and_caught);
  EXPECT_EQ(n_constructors, 1);
  EXPECT_EQ(n_destructors, 0);
}

TEST(
    ut0new_aligned_new_delete_arr,
    no_destructors_are_invoked_when_first_non_trivially_constructible_element_constructor_throws) {
  static int n_constructors = 0;
  static int n_destructors = 0;

  struct Type_that_always_throws {
    Type_that_always_throws(int x, int y) : _x(x), _y(y) {
      n_constructors++;
      throw std::runtime_error("cannot construct");
    }
    ~Type_that_always_throws() { ++n_destructors; }
    int _x, _y;
  };

  bool exception_thrown_and_caught = false;
  try {
    auto ptr = ut::aligned_new_arr_withkey<Type_that_always_throws>(
        ut::make_psi_memory_key(pfs_key), 2 * alignof(std::max_align_t),
        std::forward_as_tuple(0, 1), std::forward_as_tuple(2, 3),
        std::forward_as_tuple(4, 5), std::forward_as_tuple(6, 7),
        std::forward_as_tuple(8, 9));
    ASSERT_FALSE(ptr);
  } catch (std::runtime_error &) {
    exception_thrown_and_caught = true;
  }
  EXPECT_TRUE(exception_thrown_and_caught);
  EXPECT_EQ(n_constructors, 1);
  EXPECT_EQ(n_destructors, 0);
}

TEST(ut0new_aligned_new_delete,
     no_destructor_is_invoked_when_no_object_is_successfully_constructed) {
  static int n_constructors = 0;
  static int n_destructors = 0;

  struct Type_that_always_throws {
    Type_that_always_throws() {
      n_constructors++;
      throw std::runtime_error("cannot construct");
    }
    ~Type_that_always_throws() { ++n_destructors; }
  };

  bool exception_thrown_and_caught = false;
  try {
    auto ptr = ut::aligned_new_withkey<Type_that_always_throws>(
        ut::make_psi_memory_key(pfs_key), 2 * alignof(std::max_align_t));
    ASSERT_FALSE(ptr);
  } catch (std::runtime_error &) {
    exception_thrown_and_caught = true;
  }
  EXPECT_TRUE(exception_thrown_and_caught);
  EXPECT_EQ(n_constructors, 1);
  EXPECT_EQ(n_destructors, 0);
}

TEST(aligned_pointer, access_data_through_implicit_conversion_operator) {
  constexpr auto alignment = 4 * 1024;
  ut::aligned_pointer<int, alignment> ptr;
  ptr.alloc();

  int *data = ptr;
  EXPECT_TRUE(reinterpret_cast<std::uintptr_t>(data) % alignment == 0);
  EXPECT_EQ(*data, int{});

  ptr.dealloc();
}

TEST(aligned_array_pointer, access_data_through_subscript_operator) {
  constexpr auto n_elements = 5;
  constexpr auto alignment = 4 * 1024;
  ut::aligned_array_pointer<Default_constructible_pod, alignment> ptr;
  ptr.alloc(ut::Count{n_elements});

  EXPECT_TRUE(reinterpret_cast<std::uintptr_t>(&ptr[0]) % alignment == 0);
  for (size_t elem = 0; elem < n_elements; elem++) {
    EXPECT_EQ(ptr[elem].x, 0);
    EXPECT_EQ(ptr[elem].y, 1);
  }

  ptr.dealloc();
}

TEST(aligned_array_pointer, initialize_an_array_of_non_pod_types) {
  constexpr auto alignment = 4 * 1024;
  ut::aligned_array_pointer<Non_pod_type, alignment> ptr;
  EXPECT_EQ(Non_pod_type::get_count(), 0);
  ptr.alloc(std::forward_as_tuple(1, 2, std::string("a")),
            std::forward_as_tuple(3, 4, std::string("b")),
            std::forward_as_tuple(5, 6, std::string("c")),
            std::forward_as_tuple(7, 8, std::string("d")),
            std::forward_as_tuple(9, 10, std::string("e")));

  EXPECT_TRUE(reinterpret_cast<std::uintptr_t>(&ptr[0]) % alignment == 0);

  EXPECT_EQ(ptr[0].x, 1);
  EXPECT_EQ(ptr[0].y, 2);
  EXPECT_TRUE(ptr[0].s == std::string("a"));

  EXPECT_EQ(ptr[1].x, 3);
  EXPECT_EQ(ptr[1].y, 4);
  EXPECT_TRUE(ptr[1].s == std::string("b"));

  EXPECT_EQ(ptr[2].x, 5);
  EXPECT_EQ(ptr[2].y, 6);
  EXPECT_TRUE(ptr[2].s == std::string("c"));

  EXPECT_EQ(ptr[3].x, 7);
  EXPECT_EQ(ptr[3].y, 8);
  EXPECT_TRUE(ptr[3].s == std::string("d"));

  EXPECT_EQ(ptr[4].x, 9);
  EXPECT_EQ(ptr[4].y, 10);
  EXPECT_TRUE(ptr[4].s == std::string("e"));

  EXPECT_EQ(Non_pod_type::get_count(), 5);
  ptr.dealloc();
  EXPECT_EQ(Non_pod_type::get_count(), 0);
}

TEST(aligned_array_pointer, distance_between_elements_in_arr) {
  constexpr auto n_elements = 5;
  constexpr auto alignment = 4 * 1024;
  ut::aligned_array_pointer<Default_constructible_pod, alignment> ptr;
  ptr.alloc(ut::Count{n_elements});

  EXPECT_TRUE(reinterpret_cast<std::uintptr_t>(&ptr[0]) % alignment == 0);

  for (size_t elem = 1; elem < n_elements; elem++) {
    auto addr_curr = reinterpret_cast<std::uintptr_t>(&ptr[elem]);
    auto addr_prev = reinterpret_cast<std::uintptr_t>(&ptr[elem - 1]);
    auto distance = addr_curr - addr_prev;
    EXPECT_EQ(distance, sizeof(Default_constructible_pod));
  }

  ptr.dealloc();
}

template <bool With_pfs, typename T>
struct select_allocator_variant {
  using type = ut::detail::allocator_base<T>;
};
template <typename T>
struct select_allocator_variant<false, T> {
  using type = ut::detail::allocator_base_pfs<T>;
};
template <bool With_pfs, typename T>
using select_allocator_variant_t =
    typename select_allocator_variant<With_pfs, T>::type;

// allocator - fundamental types
template <typename T>
class ut0new_allocator_fundamental_types : public ::testing::Test {};
TYPED_TEST_SUITE_P(ut0new_allocator_fundamental_types);
TYPED_TEST_P(ut0new_allocator_fundamental_types, fundamental_types) {
  using T = typename TypeParam::type;
  using allocator_variant = select_allocator_variant_t<TypeParam::with_pfs, T>;

  auto n_elements = 100;
  ut::allocator<T, allocator_variant> a(pfs_key);

  auto ptr = a.allocate(n_elements);

  EXPECT_TRUE(ptr_is_suitably_aligned(ptr));

  ptr[0] = std::numeric_limits<T>::max();
  EXPECT_EQ(ptr[0], std::numeric_limits<T>::max());
  ptr[n_elements - 1] = std::numeric_limits<T>::min();
  EXPECT_EQ(ptr[n_elements - 1], std::numeric_limits<T>::min());

  a.deallocate(ptr);
}
REGISTER_TYPED_TEST_SUITE_P(ut0new_allocator_fundamental_types,
                            fundamental_types);
INSTANTIATE_TYPED_TEST_SUITE_P(FundamentalTypes,
                               ut0new_allocator_fundamental_types,
                               all_fundamental_types);

// allocator - pod types
template <typename T>
class ut0new_allocator_pod_types : public ::testing::Test {};
TYPED_TEST_SUITE_P(ut0new_allocator_pod_types);
TYPED_TEST_P(ut0new_allocator_pod_types, pod_types) {
  using T = typename TypeParam::type;
  using allocator_variant = select_allocator_variant_t<TypeParam::with_pfs, T>;

  auto n_elements = 100;
  ut::allocator<T, allocator_variant> a(pfs_key);

  auto ptr = a.allocate(n_elements);

  EXPECT_TRUE(ptr_is_suitably_aligned(ptr));

  a.deallocate(ptr);
}
REGISTER_TYPED_TEST_SUITE_P(ut0new_allocator_pod_types, pod_types);
INSTANTIATE_TYPED_TEST_SUITE_P(PodTypes, ut0new_allocator_pod_types,
                               all_pod_types);

// allocator - non_pod types
template <typename T>
class ut0new_allocator_non_pod_types : public ::testing::Test {};
TYPED_TEST_SUITE_P(ut0new_allocator_non_pod_types);
TYPED_TEST_P(ut0new_allocator_non_pod_types, non_pod_types) {
  using T = typename TypeParam::type;
  using allocator_variant = select_allocator_variant_t<TypeParam::with_pfs, T>;

  auto n_elements = 100;
  ut::allocator<T, allocator_variant> a(pfs_key);

  auto ptr = a.allocate(n_elements);

  EXPECT_TRUE(ptr_is_suitably_aligned(ptr));

  a.deallocate(ptr);
}
REGISTER_TYPED_TEST_SUITE_P(ut0new_allocator_non_pod_types, non_pod_types);
INSTANTIATE_TYPED_TEST_SUITE_P(NonPodTypes, ut0new_allocator_non_pod_types,
                               all_non_pod_types);

// allocator - std::vector with fundamental types
template <typename T>
class ut0new_allocator_std_vector_with_fundamental_types
    : public ::testing::Test {};
TYPED_TEST_SUITE_P(ut0new_allocator_std_vector_with_fundamental_types);
TYPED_TEST_P(ut0new_allocator_std_vector_with_fundamental_types,
             std_vector_with_fundamental_types) {
  using T = typename TypeParam::type;
  using allocator_variant = select_allocator_variant_t<TypeParam::with_pfs, T>;

  std::vector<T, ut::allocator<T, allocator_variant>> vec;
  vec.push_back(std::numeric_limits<T>::max());
  vec.push_back(std::numeric_limits<T>::min());
  vec.push_back(std::numeric_limits<T>::max());
  vec.push_back(std::numeric_limits<T>::min());

  EXPECT_TRUE(ptr_is_suitably_aligned(&vec[0]));

  EXPECT_EQ(vec[0], std::numeric_limits<T>::max());
  EXPECT_EQ(vec[1], std::numeric_limits<T>::min());
  EXPECT_EQ(vec[2], std::numeric_limits<T>::max());
  EXPECT_EQ(vec[3], std::numeric_limits<T>::min());
}
REGISTER_TYPED_TEST_SUITE_P(ut0new_allocator_std_vector_with_fundamental_types,
                            std_vector_with_fundamental_types);
INSTANTIATE_TYPED_TEST_SUITE_P(
    FundamentalTypes, ut0new_allocator_std_vector_with_fundamental_types,
    all_fundamental_types);

// allocator - std::vector with pod types
template <typename T>
class ut0new_allocator_std_vector_with_pod_types : public ::testing::Test {};
TYPED_TEST_SUITE_P(ut0new_allocator_std_vector_with_pod_types);
TYPED_TEST_P(ut0new_allocator_std_vector_with_pod_types,
             std_vector_with_pod_types) {
  using T = typename TypeParam::type;
  using allocator_variant = select_allocator_variant_t<TypeParam::with_pfs, T>;

  constexpr auto min = std::numeric_limits<int>::min();
  constexpr auto max = std::numeric_limits<int>::max();
  std::vector<T, ut::allocator<T, allocator_variant>> vec;
  vec.push_back({min, min});
  vec.push_back({min, max});
  vec.push_back({max, min});
  vec.push_back({max, max});

  EXPECT_TRUE(ptr_is_suitably_aligned(&vec[0]));

  EXPECT_EQ(vec[0].x, min);
  EXPECT_EQ(vec[0].y, min);
  EXPECT_EQ(vec[1].x, min);
  EXPECT_EQ(vec[1].y, max);
  EXPECT_EQ(vec[2].x, max);
  EXPECT_EQ(vec[2].y, min);
  EXPECT_EQ(vec[3].x, max);
  EXPECT_EQ(vec[3].y, max);
}
REGISTER_TYPED_TEST_SUITE_P(ut0new_allocator_std_vector_with_pod_types,
                            std_vector_with_pod_types);
INSTANTIATE_TYPED_TEST_SUITE_P(PodTypes,
                               ut0new_allocator_std_vector_with_pod_types,
                               all_pod_types);

// allocator - std::vector with non_pod types
template <typename T>
class ut0new_allocator_std_vector_with_non_pod_types : public ::testing::Test {
};
TYPED_TEST_SUITE_P(ut0new_allocator_std_vector_with_non_pod_types);
TYPED_TEST_P(ut0new_allocator_std_vector_with_non_pod_types,
             std_vector_with_non_pod_types) {
  using T = typename TypeParam::type;
  using allocator_variant = select_allocator_variant_t<TypeParam::with_pfs, T>;
  {
    std::vector<T, ut::allocator<T, allocator_variant>> vec;
    EXPECT_EQ(T::get_count(), 0);
    vec.push_back({1, 2, std::string("a")});
    vec.push_back({3, 4, std::string("b")});
    vec.push_back({5, 6, std::string("c")});
    vec.push_back({7, 8, std::string("d")});
    vec.push_back({9, 10, std::string("e")});

    EXPECT_TRUE(ptr_is_suitably_aligned(&vec[0]));

    EXPECT_EQ(vec[0].x, 1);
    EXPECT_EQ(vec[0].y, 2);
    EXPECT_TRUE(vec[0].s == std::string("a"));

    EXPECT_EQ(vec[1].x, 3);
    EXPECT_EQ(vec[1].y, 4);
    EXPECT_TRUE(vec[1].s == std::string("b"));

    EXPECT_EQ(vec[2].x, 5);
    EXPECT_EQ(vec[2].y, 6);
    EXPECT_TRUE(vec[2].s == std::string("c"));

    EXPECT_EQ(vec[3].x, 7);
    EXPECT_EQ(vec[3].y, 8);
    EXPECT_TRUE(vec[3].s == std::string("d"));

    EXPECT_EQ(vec[4].x, 9);
    EXPECT_EQ(vec[4].y, 10);
    EXPECT_TRUE(vec[4].s == std::string("e"));
    EXPECT_EQ(T::get_count(), 5);
  }
  EXPECT_EQ(T::get_count(), 0);
}
REGISTER_TYPED_TEST_SUITE_P(ut0new_allocator_std_vector_with_non_pod_types,
                            std_vector_with_non_pod_types);
INSTANTIATE_TYPED_TEST_SUITE_P(NonPodTypes,
                               ut0new_allocator_std_vector_with_non_pod_types,
                               all_non_pod_types);

// allocator - std::vector with default_constructible_pod types
template <typename T>
class ut0new_allocator_std_vector_with_default_constructible_pod_types
    : public ::testing::Test {};
TYPED_TEST_SUITE_P(
    ut0new_allocator_std_vector_with_default_constructible_pod_types);
TYPED_TEST_P(ut0new_allocator_std_vector_with_default_constructible_pod_types,
             std_vector_with_default_constructible_pod_types) {
  using T = typename TypeParam::type;
  using allocator_variant = select_allocator_variant_t<TypeParam::with_pfs, T>;

  std::vector<T, ut::allocator<T, allocator_variant>> vec;
  vec.push_back({});
  vec.push_back({});
  vec.push_back({});
  vec.push_back({});

  EXPECT_TRUE(ptr_is_suitably_aligned(&vec[0]));

  EXPECT_EQ(vec[0].x, 0);
  EXPECT_EQ(vec[0].y, 1);
  EXPECT_EQ(vec[1].x, 0);
  EXPECT_EQ(vec[1].y, 1);
  EXPECT_EQ(vec[2].x, 0);
  EXPECT_EQ(vec[2].y, 1);
  EXPECT_EQ(vec[3].x, 0);
  EXPECT_EQ(vec[3].y, 1);
}
REGISTER_TYPED_TEST_SUITE_P(
    ut0new_allocator_std_vector_with_default_constructible_pod_types,
    std_vector_with_default_constructible_pod_types);
INSTANTIATE_TYPED_TEST_SUITE_P(
    DefaultConstructiblePodTypes,
    ut0new_allocator_std_vector_with_default_constructible_pod_types,
    all_default_constructible_pod_types);

// allocator - std::vector with default_non_pod_constructible types
template <typename T>
class ut0new_allocator_std_vector_with_default_non_pod_constructible_types
    : public ::testing::Test {};
TYPED_TEST_SUITE_P(
    ut0new_allocator_std_vector_with_default_non_pod_constructible_types);
TYPED_TEST_P(
    ut0new_allocator_std_vector_with_default_non_pod_constructible_types,
    std_vector_with_default_non_pod_constructible_types) {
  using T = typename TypeParam::type;
  using allocator_variant = select_allocator_variant_t<TypeParam::with_pfs, T>;
  {
    std::vector<T, ut::allocator<T, allocator_variant>> vec;

    vec.push_back({});
    vec.push_back({});
    vec.push_back({});
    vec.push_back({});
    vec.push_back({});

    EXPECT_TRUE(ptr_is_suitably_aligned(&vec[0]));

    EXPECT_EQ(vec[0].x, 0);
    EXPECT_EQ(vec[0].y, 1);
    EXPECT_TRUE(vec[0].s == std::string("non-pod-string"));

    EXPECT_EQ(vec[1].x, 0);
    EXPECT_EQ(vec[1].y, 1);
    EXPECT_TRUE(vec[1].s == std::string("non-pod-string"));

    EXPECT_EQ(vec[2].x, 0);
    EXPECT_EQ(vec[2].y, 1);
    EXPECT_TRUE(vec[2].s == std::string("non-pod-string"));

    EXPECT_EQ(vec[3].x, 0);
    EXPECT_EQ(vec[3].y, 1);
    EXPECT_TRUE(vec[3].s == std::string("non-pod-string"));
  }
}
REGISTER_TYPED_TEST_SUITE_P(
    ut0new_allocator_std_vector_with_default_non_pod_constructible_types,
    std_vector_with_default_non_pod_constructible_types);
INSTANTIATE_TYPED_TEST_SUITE_P(
    NonPodTypes,
    ut0new_allocator_std_vector_with_default_non_pod_constructible_types,
    all_default_constructible_non_pod_types);

TEST(ut0new_allocator, throws_bad_array_new_length_when_max_size_is_exceeded) {
  struct big_t {
    char x[128];
  };

  ut::allocator<big_t> alloc(mem_key_buf_buf_pool);

  const ut::allocator<big_t>::size_type too_many_elements =
      std::numeric_limits<ut::allocator<big_t>::size_type>::max() /
          sizeof(big_t) +
      1;

  big_t *ptr = nullptr;
  bool threw = false;
  try {
    ptr = alloc.allocate(too_many_elements);
  } catch (std::bad_array_new_length &) {
    threw = true;
  }
  EXPECT_TRUE(threw);
  EXPECT_FALSE(ptr);
}

// make_unique - fundamental types
template <typename T>
class ut0new_make_unique_ptr_fundamental_types : public ::testing::Test {};
TYPED_TEST_SUITE_P(ut0new_make_unique_ptr_fundamental_types);
TYPED_TEST_P(ut0new_make_unique_ptr_fundamental_types, fundamental_types) {
  using type = typename TypeParam::type;
  auto with_pfs = TypeParam::with_pfs;
  auto ptr = with_pfs
                 ? ut::make_unique<type>(ut::make_psi_memory_key(pfs_key), 1)
                 : ut::make_unique<type>(1);
  EXPECT_TRUE(ptr_is_suitably_aligned(ptr.get()));
  EXPECT_EQ(*ptr, 1);
}
REGISTER_TYPED_TEST_SUITE_P(ut0new_make_unique_ptr_fundamental_types,
                            fundamental_types);
INSTANTIATE_TYPED_TEST_SUITE_P(My, ut0new_make_unique_ptr_fundamental_types,
                               all_fundamental_types);

// make_unique - pod types
template <typename T>
class ut0new_make_unique_ptr_pod_types : public ::testing::Test {};
TYPED_TEST_SUITE_P(ut0new_make_unique_ptr_pod_types);
TYPED_TEST_P(ut0new_make_unique_ptr_pod_types, pod_types) {
  using type = typename TypeParam::type;
  auto with_pfs = TypeParam::with_pfs;

  auto ptr = with_pfs
                 ? ut::make_unique<type>(ut::make_psi_memory_key(pfs_key), 2, 5)
                 : ut::make_unique<type>(2, 5);
  EXPECT_TRUE(ptr_is_suitably_aligned(ptr.get()));
  EXPECT_EQ(ptr->x, 2);
  EXPECT_EQ(ptr->y, 5);
}
REGISTER_TYPED_TEST_SUITE_P(ut0new_make_unique_ptr_pod_types, pod_types);
INSTANTIATE_TYPED_TEST_SUITE_P(My, ut0new_make_unique_ptr_pod_types,
                               all_pod_types);

// make_unique - default constructible pod types
template <typename T>
class ut0new_make_unique_ptr_default_constructible_pod_types
    : public ::testing::Test {};
TYPED_TEST_SUITE_P(ut0new_make_unique_ptr_default_constructible_pod_types);
TYPED_TEST_P(ut0new_make_unique_ptr_default_constructible_pod_types,
             default_constructible_pod_types) {
  using type = typename TypeParam::type;
  auto with_pfs = TypeParam::with_pfs;

  auto ptr = with_pfs ? ut::make_unique<type>(ut::make_psi_memory_key(pfs_key))
                      : ut::make_unique<type>();
  EXPECT_TRUE(ptr_is_suitably_aligned(ptr.get()));
  EXPECT_EQ(ptr->x, 0);
  EXPECT_EQ(ptr->y, 1);
}
REGISTER_TYPED_TEST_SUITE_P(
    ut0new_make_unique_ptr_default_constructible_pod_types,
    default_constructible_pod_types);
INSTANTIATE_TYPED_TEST_SUITE_P(
    My, ut0new_make_unique_ptr_default_constructible_pod_types,
    all_default_constructible_pod_types);

// make_unique - non-pod types
template <typename T>
class ut0new_make_unique_ptr_non_pod_types : public ::testing::Test {};
TYPED_TEST_SUITE_P(ut0new_make_unique_ptr_non_pod_types);
TYPED_TEST_P(ut0new_make_unique_ptr_non_pod_types, non_pod_types) {
  using type = typename TypeParam::type;
  auto with_pfs = TypeParam::with_pfs;
  EXPECT_EQ(type::get_count(), 0);
  {
    auto ptr = with_pfs
                   ? ut::make_unique<type>(ut::make_psi_memory_key(pfs_key), 2,
                                           5, std::string("non-pod"))
                   : ut::make_unique<type>(2, 5, std::string("non-pod"));
    EXPECT_TRUE(ptr_is_suitably_aligned(ptr.get()));
    EXPECT_EQ(ptr->x, 2);
    EXPECT_EQ(ptr->y, 5);
    EXPECT_EQ(ptr->sum.result, 7);
    EXPECT_EQ(ptr->s, std::string("non-pod"));
    EXPECT_EQ(type::get_count(), 1);
  }
  EXPECT_EQ(type::get_count(), 0);
}
REGISTER_TYPED_TEST_SUITE_P(ut0new_make_unique_ptr_non_pod_types,
                            non_pod_types);
INSTANTIATE_TYPED_TEST_SUITE_P(My, ut0new_make_unique_ptr_non_pod_types,
                               all_non_pod_types);

// make_unique - default constructible non-pod types
template <typename T>
class ut0new_make_unique_ptr_default_constructible_non_pod_types
    : public ::testing::Test {};
TYPED_TEST_SUITE_P(ut0new_make_unique_ptr_default_constructible_non_pod_types);
TYPED_TEST_P(ut0new_make_unique_ptr_default_constructible_non_pod_types,
             default_constructible_non_pod_types) {
  using type = typename TypeParam::type;
  auto with_pfs = TypeParam::with_pfs;
  {
    auto ptr = with_pfs
                   ? ut::make_unique<type>(ut::make_psi_memory_key(pfs_key))
                   : ut::make_unique<type>();
    EXPECT_TRUE(ptr_is_suitably_aligned(ptr.get()));
    EXPECT_EQ(ptr->x, 0);
    EXPECT_EQ(ptr->y, 1);
    EXPECT_EQ(ptr->s, std::string("non-pod-string"));
  }
}
REGISTER_TYPED_TEST_SUITE_P(
    ut0new_make_unique_ptr_default_constructible_non_pod_types,
    default_constructible_non_pod_types);
INSTANTIATE_TYPED_TEST_SUITE_P(
    My, ut0new_make_unique_ptr_default_constructible_non_pod_types,
    all_default_constructible_non_pod_types);

// make_unique - array specialization for fundamental types
template <typename T>
class ut0new_make_unique_ptr_fundamental_types_arr : public ::testing::Test {};
TYPED_TEST_SUITE_P(ut0new_make_unique_ptr_fundamental_types_arr);
TYPED_TEST_P(ut0new_make_unique_ptr_fundamental_types_arr, fundamental_types) {
  using type = typename TypeParam::type;
  auto with_pfs = TypeParam::with_pfs;
  auto ptr = with_pfs
                 ? ut::make_unique<type[]>(ut::make_psi_memory_key(pfs_key), 3)
                 : ut::make_unique<type[]>(3);
  EXPECT_TRUE(ptr_is_suitably_aligned(ptr.get()));
  EXPECT_EQ(ptr[0], type{});
  EXPECT_EQ(ptr[1], type{});
  EXPECT_EQ(ptr[2], type{});
}
REGISTER_TYPED_TEST_SUITE_P(ut0new_make_unique_ptr_fundamental_types_arr,
                            fundamental_types);
INSTANTIATE_TYPED_TEST_SUITE_P(My, ut0new_make_unique_ptr_fundamental_types_arr,
                               all_fundamental_types);

// make_unique - array specialization for default_constructible POD types
template <typename T>
class ut0new_make_unique_ptr_default_constructible_pod_types_arr
    : public ::testing::Test {};
TYPED_TEST_SUITE_P(ut0new_make_unique_ptr_default_constructible_pod_types_arr);
TYPED_TEST_P(ut0new_make_unique_ptr_default_constructible_pod_types_arr,
             default_constructible_pod_types) {
  using type = typename TypeParam::type;
  auto with_pfs = TypeParam::with_pfs;

  auto ptr = with_pfs
                 ? ut::make_unique<type[]>(ut::make_psi_memory_key(pfs_key), 3)
                 : ut::make_unique<type[]>(3);
  EXPECT_TRUE(ptr_is_suitably_aligned(ptr.get()));
  EXPECT_EQ(ptr[0].x, 0);
  EXPECT_EQ(ptr[0].y, 1);
  EXPECT_EQ(ptr[1].x, 0);
  EXPECT_EQ(ptr[1].y, 1);
  EXPECT_EQ(ptr[2].x, 0);
  EXPECT_EQ(ptr[2].y, 1);
}
REGISTER_TYPED_TEST_SUITE_P(
    ut0new_make_unique_ptr_default_constructible_pod_types_arr,
    default_constructible_pod_types);
INSTANTIATE_TYPED_TEST_SUITE_P(
    My, ut0new_make_unique_ptr_default_constructible_pod_types_arr,
    all_default_constructible_pod_types);

// make_unique_aligned - fundamental types
template <typename T>
class ut0new_make_unique_aligned_fundamental_types : public ::testing::Test {};
TYPED_TEST_SUITE_P(ut0new_make_unique_aligned_fundamental_types);
TYPED_TEST_P(ut0new_make_unique_aligned_fundamental_types, fundamental_types) {
  using type = typename TypeParam::type;
  auto with_pfs = TypeParam::with_pfs;
  auto alignment = 4 * alignof(std::max_align_t);
  auto ptr = with_pfs ? ut::make_unique_aligned<type>(
                            ut::make_psi_memory_key(pfs_key), alignment, 1)
                      : ut::make_unique_aligned<type>(alignment, 1);
  EXPECT_TRUE(reinterpret_cast<std::uintptr_t>(ptr.get()) % alignment == 0);
  EXPECT_EQ(*ptr, 1);
}
REGISTER_TYPED_TEST_SUITE_P(ut0new_make_unique_aligned_fundamental_types,
                            fundamental_types);
INSTANTIATE_TYPED_TEST_SUITE_P(My, ut0new_make_unique_aligned_fundamental_types,
                               all_fundamental_types);

// make_unique_aligned - pod types
template <typename T>
class ut0new_make_unique_aligned_pod_types : public ::testing::Test {};
TYPED_TEST_SUITE_P(ut0new_make_unique_aligned_pod_types);
TYPED_TEST_P(ut0new_make_unique_aligned_pod_types, pod_types) {
  using type = typename TypeParam::type;
  auto with_pfs = TypeParam::with_pfs;
  auto alignment = 4 * alignof(std::max_align_t);
  auto ptr = with_pfs ? ut::make_unique_aligned<type>(
                            ut::make_psi_memory_key(pfs_key), alignment, 2, 5)
                      : ut::make_unique_aligned<type>(alignment, 2, 5);
  EXPECT_TRUE(reinterpret_cast<std::uintptr_t>(ptr.get()) % alignment == 0);
  EXPECT_EQ(ptr->x, 2);
  EXPECT_EQ(ptr->y, 5);
}
REGISTER_TYPED_TEST_SUITE_P(ut0new_make_unique_aligned_pod_types, pod_types);
INSTANTIATE_TYPED_TEST_SUITE_P(My, ut0new_make_unique_aligned_pod_types,
                               all_pod_types);

// make_unique_aligned - default constructible pod types
template <typename T>
class ut0new_make_unique_aligned_default_constructible_pod_types
    : public ::testing::Test {};
TYPED_TEST_SUITE_P(ut0new_make_unique_aligned_default_constructible_pod_types);
TYPED_TEST_P(ut0new_make_unique_aligned_default_constructible_pod_types,
             default_constructible_pod_types) {
  using type = typename TypeParam::type;
  auto with_pfs = TypeParam::with_pfs;
  auto alignment = 4 * alignof(std::max_align_t);
  auto ptr = with_pfs ? ut::make_unique_aligned<type>(
                            ut::make_psi_memory_key(pfs_key), alignment)
                      : ut::make_unique_aligned<type>(alignment);
  EXPECT_TRUE(reinterpret_cast<std::uintptr_t>(ptr.get()) % alignment == 0);
  EXPECT_EQ(ptr->x, 0);
  EXPECT_EQ(ptr->y, 1);
}
REGISTER_TYPED_TEST_SUITE_P(
    ut0new_make_unique_aligned_default_constructible_pod_types,
    default_constructible_pod_types);
INSTANTIATE_TYPED_TEST_SUITE_P(
    My, ut0new_make_unique_aligned_default_constructible_pod_types,
    all_default_constructible_pod_types);

// make_unique_aligned - non-pod types
template <typename T>
class ut0new_make_unique_aligned_non_pod_types : public ::testing::Test {};
TYPED_TEST_SUITE_P(ut0new_make_unique_aligned_non_pod_types);
TYPED_TEST_P(ut0new_make_unique_aligned_non_pod_types, non_pod_types) {
  using type = typename TypeParam::type;
  auto with_pfs = TypeParam::with_pfs;
  auto alignment = 4 * alignof(std::max_align_t);
  EXPECT_EQ(type::get_count(), 0);
  {
    auto ptr = with_pfs ? ut::make_unique_aligned<type>(
                              ut::make_psi_memory_key(pfs_key), alignment, 2, 5,
                              std::string("non-pod"))
                        : ut::make_unique_aligned<type>(alignment, 2, 5,
                                                        std::string("non-pod"));
    EXPECT_TRUE(reinterpret_cast<std::uintptr_t>(ptr.get()) % alignment == 0);
    EXPECT_EQ(ptr->x, 2);
    EXPECT_EQ(ptr->y, 5);
    EXPECT_EQ(ptr->sum.result, 7);
    EXPECT_EQ(ptr->s, std::string("non-pod"));
    EXPECT_EQ(type::get_count(), 1);
  }
  EXPECT_EQ(type::get_count(), 0);
}
REGISTER_TYPED_TEST_SUITE_P(ut0new_make_unique_aligned_non_pod_types,
                            non_pod_types);
INSTANTIATE_TYPED_TEST_SUITE_P(My, ut0new_make_unique_aligned_non_pod_types,
                               all_non_pod_types);

// make_unique_aligned - default constructible non-pod types
template <typename T>
class ut0new_make_unique_aligned_default_constructible_non_pod_types
    : public ::testing::Test {};
TYPED_TEST_SUITE_P(
    ut0new_make_unique_aligned_default_constructible_non_pod_types);
TYPED_TEST_P(ut0new_make_unique_aligned_default_constructible_non_pod_types,
             default_constructible_non_pod_types) {
  using type = typename TypeParam::type;
  auto with_pfs = TypeParam::with_pfs;
  auto alignment = 4 * alignof(std::max_align_t);
  {
    auto ptr = with_pfs ? ut::make_unique_aligned<type>(
                              ut::make_psi_memory_key(pfs_key), alignment)
                        : ut::make_unique_aligned<type>(alignment);
    EXPECT_TRUE(reinterpret_cast<std::uintptr_t>(ptr.get()) % alignment == 0);
    EXPECT_EQ(ptr->x, 0);
    EXPECT_EQ(ptr->y, 1);
    EXPECT_EQ(ptr->s, std::string("non-pod-string"));
  }
}
REGISTER_TYPED_TEST_SUITE_P(
    ut0new_make_unique_aligned_default_constructible_non_pod_types,
    default_constructible_non_pod_types);
INSTANTIATE_TYPED_TEST_SUITE_P(
    My, ut0new_make_unique_aligned_default_constructible_non_pod_types,
    all_default_constructible_non_pod_types);

// make_unique_aligned - array specialization for fundamental types
template <typename T>
class ut0new_make_unique_aligned_fundamental_types_arr
    : public ::testing::Test {};
TYPED_TEST_SUITE_P(ut0new_make_unique_aligned_fundamental_types_arr);
TYPED_TEST_P(ut0new_make_unique_aligned_fundamental_types_arr,
             fundamental_types) {
  using type = typename TypeParam::type;
  auto with_pfs = TypeParam::with_pfs;
  auto alignment = 4 * alignof(std::max_align_t);
  auto ptr = with_pfs ? ut::make_unique_aligned<type[]>(
                            ut::make_psi_memory_key(pfs_key), alignment, 3)
                      : ut::make_unique_aligned<type[]>(alignment, 3);
  EXPECT_TRUE(reinterpret_cast<std::uintptr_t>(ptr.get()) % alignment == 0);
  EXPECT_EQ(ptr[0], type{});
  EXPECT_EQ(ptr[1], type{});
  EXPECT_EQ(ptr[2], type{});
}
REGISTER_TYPED_TEST_SUITE_P(ut0new_make_unique_aligned_fundamental_types_arr,
                            fundamental_types);
INSTANTIATE_TYPED_TEST_SUITE_P(My,
                               ut0new_make_unique_aligned_fundamental_types_arr,
                               all_fundamental_types);

// make_unique_aligned - array specialization for default_constructible POD
// types
template <typename T>
class ut0new_make_unique_aligned_default_constructible_pod_types_arr
    : public ::testing::Test {};
TYPED_TEST_SUITE_P(
    ut0new_make_unique_aligned_default_constructible_pod_types_arr);
TYPED_TEST_P(ut0new_make_unique_aligned_default_constructible_pod_types_arr,
             default_constructible_pod_types) {
  using type = typename TypeParam::type;
  auto with_pfs = TypeParam::with_pfs;
  auto alignment = 4 * alignof(std::max_align_t);
  auto ptr = with_pfs ? ut::make_unique_aligned<type[]>(
                            ut::make_psi_memory_key(pfs_key), alignment, 3)
                      : ut::make_unique_aligned<type[]>(alignment, 3);
  EXPECT_TRUE(reinterpret_cast<std::uintptr_t>(ptr.get()) % alignment == 0);
  EXPECT_EQ(ptr[0].x, 0);
  EXPECT_EQ(ptr[0].y, 1);
  EXPECT_EQ(ptr[1].x, 0);
  EXPECT_EQ(ptr[1].y, 1);
  EXPECT_EQ(ptr[2].x, 0);
  EXPECT_EQ(ptr[2].y, 1);
}
REGISTER_TYPED_TEST_SUITE_P(
    ut0new_make_unique_aligned_default_constructible_pod_types_arr,
    default_constructible_pod_types);
INSTANTIATE_TYPED_TEST_SUITE_P(
    My, ut0new_make_unique_aligned_default_constructible_pod_types_arr,
    all_default_constructible_pod_types);

// make_shared - fundamental types
template <typename T>
class ut0new_make_shared_ptr_fundamental_types : public ::testing::Test {};
TYPED_TEST_SUITE_P(ut0new_make_shared_ptr_fundamental_types);
TYPED_TEST_P(ut0new_make_shared_ptr_fundamental_types, fundamental_types) {
  using type = typename TypeParam::type;
  auto with_pfs = TypeParam::with_pfs;
  auto ptr = with_pfs
                 ? ut::make_shared<type>(ut::make_psi_memory_key(pfs_key), 1)
                 : ut::make_shared<type>(1);
  EXPECT_TRUE(ptr_is_suitably_aligned(ptr.get()));
  EXPECT_EQ(*ptr, 1);
}
REGISTER_TYPED_TEST_SUITE_P(ut0new_make_shared_ptr_fundamental_types,
                            fundamental_types);
INSTANTIATE_TYPED_TEST_SUITE_P(My, ut0new_make_shared_ptr_fundamental_types,
                               all_fundamental_types);

// make_shared - pod types
template <typename T>
class ut0new_make_shared_ptr_pod_types : public ::testing::Test {};
TYPED_TEST_SUITE_P(ut0new_make_shared_ptr_pod_types);
TYPED_TEST_P(ut0new_make_shared_ptr_pod_types, pod_types) {
  using type = typename TypeParam::type;
  auto with_pfs = TypeParam::with_pfs;

  auto ptr = with_pfs
                 ? ut::make_shared<type>(ut::make_psi_memory_key(pfs_key), 2, 5)
                 : ut::make_shared<type>(2, 5);
  EXPECT_TRUE(ptr_is_suitably_aligned(ptr.get()));
  EXPECT_EQ(ptr->x, 2);
  EXPECT_EQ(ptr->y, 5);
}
REGISTER_TYPED_TEST_SUITE_P(ut0new_make_shared_ptr_pod_types, pod_types);
INSTANTIATE_TYPED_TEST_SUITE_P(My, ut0new_make_shared_ptr_pod_types,
                               all_pod_types);

// make_shared - default constructible pod types
template <typename T>
class ut0new_make_shared_ptr_default_constructible_pod_types
    : public ::testing::Test {};
TYPED_TEST_SUITE_P(ut0new_make_shared_ptr_default_constructible_pod_types);
TYPED_TEST_P(ut0new_make_shared_ptr_default_constructible_pod_types,
             default_constructible_pod_types) {
  using type = typename TypeParam::type;
  auto with_pfs = TypeParam::with_pfs;

  auto ptr = with_pfs ? ut::make_shared<type>(ut::make_psi_memory_key(pfs_key))
                      : ut::make_shared<type>();
  EXPECT_TRUE(ptr_is_suitably_aligned(ptr.get()));
  EXPECT_EQ(ptr->x, 0);
  EXPECT_EQ(ptr->y, 1);
}
REGISTER_TYPED_TEST_SUITE_P(
    ut0new_make_shared_ptr_default_constructible_pod_types,
    default_constructible_pod_types);
INSTANTIATE_TYPED_TEST_SUITE_P(
    My, ut0new_make_shared_ptr_default_constructible_pod_types,
    all_default_constructible_pod_types);

// make_shared - non-pod types
template <typename T>
class ut0new_make_shared_ptr_non_pod_types : public ::testing::Test {};
TYPED_TEST_SUITE_P(ut0new_make_shared_ptr_non_pod_types);
TYPED_TEST_P(ut0new_make_shared_ptr_non_pod_types, non_pod_types) {
  using type = typename TypeParam::type;
  auto with_pfs = TypeParam::with_pfs;
  EXPECT_EQ(type::get_count(), 0);
  {
    auto ptr = with_pfs
                   ? ut::make_shared<type>(ut::make_psi_memory_key(pfs_key), 2,
                                           5, std::string("non-pod"))
                   : ut::make_shared<type>(2, 5, std::string("non-pod"));
    EXPECT_TRUE(ptr_is_suitably_aligned(ptr.get()));
    EXPECT_EQ(ptr->x, 2);
    EXPECT_EQ(ptr->y, 5);
    EXPECT_EQ(ptr->sum.result, 7);
    EXPECT_EQ(ptr->s, std::string("non-pod"));
    EXPECT_EQ(type::get_count(), 1);
  }
  EXPECT_EQ(type::get_count(), 0);
}
REGISTER_TYPED_TEST_SUITE_P(ut0new_make_shared_ptr_non_pod_types,
                            non_pod_types);
INSTANTIATE_TYPED_TEST_SUITE_P(My, ut0new_make_shared_ptr_non_pod_types,
                               all_non_pod_types);

// make_shared - default constructible non-pod types
template <typename T>
class ut0new_make_shared_ptr_default_constructible_non_pod_types
    : public ::testing::Test {};
TYPED_TEST_SUITE_P(ut0new_make_shared_ptr_default_constructible_non_pod_types);
TYPED_TEST_P(ut0new_make_shared_ptr_default_constructible_non_pod_types,
             default_constructible_non_pod_types) {
  using type = typename TypeParam::type;
  auto with_pfs = TypeParam::with_pfs;
  {
    auto ptr = with_pfs
                   ? ut::make_shared<type>(ut::make_psi_memory_key(pfs_key))
                   : ut::make_shared<type>();
    EXPECT_TRUE(ptr_is_suitably_aligned(ptr.get()));
    EXPECT_EQ(ptr->x, 0);
    EXPECT_EQ(ptr->y, 1);
    EXPECT_EQ(ptr->s, std::string("non-pod-string"));
  }
}
REGISTER_TYPED_TEST_SUITE_P(
    ut0new_make_shared_ptr_default_constructible_non_pod_types,
    default_constructible_non_pod_types);
INSTANTIATE_TYPED_TEST_SUITE_P(
    My, ut0new_make_shared_ptr_default_constructible_non_pod_types,
    all_default_constructible_non_pod_types);

// TODO macOS build of clang on PB2 currently exhibits a bug in which
// std::shared_ptr with array types is not usable.
//
// For example,
//   std::shared_ptr<int[]> ptr(new int[3]);
//   ptr[0]; ptr[1]; ptr[2];
//
// will give the following error:
//   type 'std::__1::shared_ptr<int []>' does not provide a subscript operator
//
// This can be worked around by reinterpreting it as pointer to int:
//   ((int*)ptr.get())[0];
//   ((int*)ptr.get())[1];
//   ((int*)ptr.get())[2];
//
// But this leads to another issue:
//   no matching constructor for initialization of 'std::shared_ptr<int[]>'
//
// Interestingly enough, issue is _not_ reproducible on Linux build of clang
// (Fedora) which implied that there's something wrong with the Apple deployment
// of clang. And indeed, issue is reproducible only if code is compiled with
// libc++, an alternative (and default to Apple) implementation of C++ standard
// library. clang on Linux OTOH is using libstdc++ by default and libstdc++ does
// not suffer from this problem. So this is a bug in libc++ implementation.
//
// For now I am disabling std::shared_ptr-with-arrays subset of unit-tests from
// running on PB2 macOS jobs so that the build is not broken and so that the
// other unit-tests keep running. If and when this becomes a real issue (when
// somebody actually tries to use this approach in production code), a
// workaround will be needed to be found. It looks like
//   std::shared_ptr<std::array<int, 3>> p(new std::array<int, 3>{});
// will do or FWIW ut::make_shared version of it.
#if !defined(__APPLE__)
// make_shared - array specialization for fundamental types
template <typename T>
class ut0new_make_shared_ptr_fundamental_types_arr : public ::testing::Test {};
TYPED_TEST_SUITE_P(ut0new_make_shared_ptr_fundamental_types_arr);
TYPED_TEST_P(ut0new_make_shared_ptr_fundamental_types_arr, fundamental_types) {
  using type = typename TypeParam::type;
  auto with_pfs = TypeParam::with_pfs;
  auto ptr = with_pfs
                 ? ut::make_shared<type[]>(ut::make_psi_memory_key(pfs_key), 3)
                 : ut::make_shared<type[]>(3);
  EXPECT_TRUE(ptr_is_suitably_aligned(ptr.get()));
  EXPECT_EQ(ptr[0], type{});
  EXPECT_EQ(ptr[1], type{});
  EXPECT_EQ(ptr[2], type{});
}
REGISTER_TYPED_TEST_SUITE_P(ut0new_make_shared_ptr_fundamental_types_arr,
                            fundamental_types);
INSTANTIATE_TYPED_TEST_SUITE_P(My, ut0new_make_shared_ptr_fundamental_types_arr,
                               all_fundamental_types);

// make_shared - bounded array specialization for fundamental types
template <typename T>
class ut0new_make_shared_ptr_fundamental_types_bounded_arr
    : public ::testing::Test {};
TYPED_TEST_SUITE_P(ut0new_make_shared_ptr_fundamental_types_bounded_arr);
TYPED_TEST_P(ut0new_make_shared_ptr_fundamental_types_bounded_arr,
             fundamental_types) {
  using type = typename TypeParam::type;
  auto with_pfs = TypeParam::with_pfs;
  auto ptr = with_pfs
                 ? ut::make_shared<type[3]>(ut::make_psi_memory_key(pfs_key))
                 : ut::make_shared<type[3]>();
  EXPECT_TRUE(ptr_is_suitably_aligned(ptr.get()));
  EXPECT_EQ(ptr[0], type{});
  EXPECT_EQ(ptr[1], type{});
  EXPECT_EQ(ptr[2], type{});
}
REGISTER_TYPED_TEST_SUITE_P(
    ut0new_make_shared_ptr_fundamental_types_bounded_arr, fundamental_types);
INSTANTIATE_TYPED_TEST_SUITE_P(
    My, ut0new_make_shared_ptr_fundamental_types_bounded_arr,
    all_fundamental_types);

// make_shared - array specialization for default_constructible POD types
template <typename T>
class ut0new_make_shared_ptr_default_constructible_pod_types_arr
    : public ::testing::Test {};
TYPED_TEST_SUITE_P(ut0new_make_shared_ptr_default_constructible_pod_types_arr);
TYPED_TEST_P(ut0new_make_shared_ptr_default_constructible_pod_types_arr,
             default_constructible_pod_types) {
  using type = typename TypeParam::type;
  auto with_pfs = TypeParam::with_pfs;
  auto ptr = with_pfs
                 ? ut::make_shared<type[]>(ut::make_psi_memory_key(pfs_key), 3)
                 : ut::make_shared<type[]>(3);
  EXPECT_TRUE(ptr_is_suitably_aligned(ptr.get()));
  EXPECT_EQ(ptr[0].x, 0);
  EXPECT_EQ(ptr[0].y, 1);
  EXPECT_EQ(ptr[1].x, 0);
  EXPECT_EQ(ptr[1].y, 1);
  EXPECT_EQ(ptr[2].x, 0);
  EXPECT_EQ(ptr[2].y, 1);
}
REGISTER_TYPED_TEST_SUITE_P(
    ut0new_make_shared_ptr_default_constructible_pod_types_arr,
    default_constructible_pod_types);
INSTANTIATE_TYPED_TEST_SUITE_P(
    My, ut0new_make_shared_ptr_default_constructible_pod_types_arr,
    all_default_constructible_pod_types);

// make_shared - bounded array specialization for default_constructible POD
// types
template <typename T>
class ut0new_make_shared_ptr_default_constructible_pod_types_bounded_arr
    : public ::testing::Test {};
TYPED_TEST_SUITE_P(
    ut0new_make_shared_ptr_default_constructible_pod_types_bounded_arr);
TYPED_TEST_P(ut0new_make_shared_ptr_default_constructible_pod_types_bounded_arr,
             default_constructible_pod_types) {
  using type = typename TypeParam::type;
  auto with_pfs = TypeParam::with_pfs;
  auto ptr = with_pfs
                 ? ut::make_shared<type[3]>(ut::make_psi_memory_key(pfs_key))
                 : ut::make_shared<type[3]>();
  EXPECT_TRUE(ptr_is_suitably_aligned(ptr.get()));
  EXPECT_EQ(ptr[0].x, 0);
  EXPECT_EQ(ptr[0].y, 1);
  EXPECT_EQ(ptr[1].x, 0);
  EXPECT_EQ(ptr[1].y, 1);
  EXPECT_EQ(ptr[2].x, 0);
  EXPECT_EQ(ptr[2].y, 1);
}
REGISTER_TYPED_TEST_SUITE_P(
    ut0new_make_shared_ptr_default_constructible_pod_types_bounded_arr,
    default_constructible_pod_types);
INSTANTIATE_TYPED_TEST_SUITE_P(
    My, ut0new_make_shared_ptr_default_constructible_pod_types_bounded_arr,
    all_default_constructible_pod_types);
#endif

// make_shared_aligned - fundamental types
template <typename T>
class ut0new_make_shared_aligned_fundamental_types : public ::testing::Test {};
TYPED_TEST_SUITE_P(ut0new_make_shared_aligned_fundamental_types);
TYPED_TEST_P(ut0new_make_shared_aligned_fundamental_types, fundamental_types) {
  using type = typename TypeParam::type;
  auto with_pfs = TypeParam::with_pfs;
  auto alignment = 4 * alignof(std::max_align_t);
  auto ptr = with_pfs ? ut::make_shared_aligned<type>(
                            ut::make_psi_memory_key(pfs_key), alignment, 1)
                      : ut::make_shared_aligned<type>(alignment, 1);
  EXPECT_TRUE(reinterpret_cast<std::uintptr_t>(ptr.get()) % alignment == 0);
  EXPECT_EQ(*ptr, 1);
}
REGISTER_TYPED_TEST_SUITE_P(ut0new_make_shared_aligned_fundamental_types,
                            fundamental_types);
INSTANTIATE_TYPED_TEST_SUITE_P(My, ut0new_make_shared_aligned_fundamental_types,
                               all_fundamental_types);

// make_shared_aligned - pod types
template <typename T>
class ut0new_make_shared_aligned_pod_types : public ::testing::Test {};
TYPED_TEST_SUITE_P(ut0new_make_shared_aligned_pod_types);
TYPED_TEST_P(ut0new_make_shared_aligned_pod_types, pod_types) {
  using type = typename TypeParam::type;
  auto with_pfs = TypeParam::with_pfs;
  auto alignment = 4 * alignof(std::max_align_t);
  auto ptr = with_pfs ? ut::make_shared_aligned<type>(
                            ut::make_psi_memory_key(pfs_key), alignment, 2, 5)
                      : ut::make_shared_aligned<type>(alignment, 2, 5);
  EXPECT_TRUE(reinterpret_cast<std::uintptr_t>(ptr.get()) % alignment == 0);
  EXPECT_EQ(ptr->x, 2);
  EXPECT_EQ(ptr->y, 5);
}
REGISTER_TYPED_TEST_SUITE_P(ut0new_make_shared_aligned_pod_types, pod_types);
INSTANTIATE_TYPED_TEST_SUITE_P(My, ut0new_make_shared_aligned_pod_types,
                               all_pod_types);

// make_shared_aligned - default constructible pod types
template <typename T>
class ut0new_make_shared_aligned_default_constructible_pod_types
    : public ::testing::Test {};
TYPED_TEST_SUITE_P(ut0new_make_shared_aligned_default_constructible_pod_types);
TYPED_TEST_P(ut0new_make_shared_aligned_default_constructible_pod_types,
             default_constructible_pod_types) {
  using type = typename TypeParam::type;
  auto with_pfs = TypeParam::with_pfs;
  auto alignment = 4 * alignof(std::max_align_t);
  auto ptr = with_pfs ? ut::make_shared_aligned<type>(
                            ut::make_psi_memory_key(pfs_key), alignment)
                      : ut::make_shared_aligned<type>(alignment);
  EXPECT_TRUE(reinterpret_cast<std::uintptr_t>(ptr.get()) % alignment == 0);
  EXPECT_EQ(ptr->x, 0);
  EXPECT_EQ(ptr->y, 1);
}
REGISTER_TYPED_TEST_SUITE_P(
    ut0new_make_shared_aligned_default_constructible_pod_types,
    default_constructible_pod_types);
INSTANTIATE_TYPED_TEST_SUITE_P(
    My, ut0new_make_shared_aligned_default_constructible_pod_types,
    all_default_constructible_pod_types);

// make_shared_aligned - non-pod types
template <typename T>
class ut0new_make_shared_aligned_non_pod_types : public ::testing::Test {};
TYPED_TEST_SUITE_P(ut0new_make_shared_aligned_non_pod_types);
TYPED_TEST_P(ut0new_make_shared_aligned_non_pod_types, non_pod_types) {
  using type = typename TypeParam::type;
  auto with_pfs = TypeParam::with_pfs;
  auto alignment = 4 * alignof(std::max_align_t);
  EXPECT_EQ(type::get_count(), 0);
  {
    auto ptr = with_pfs ? ut::make_shared_aligned<type>(
                              ut::make_psi_memory_key(pfs_key), alignment, 2, 5,
                              std::string("non-pod"))
                        : ut::make_shared_aligned<type>(alignment, 2, 5,
                                                        std::string("non-pod"));
    EXPECT_TRUE(reinterpret_cast<std::uintptr_t>(ptr.get()) % alignment == 0);
    EXPECT_EQ(ptr->x, 2);
    EXPECT_EQ(ptr->y, 5);
    EXPECT_EQ(ptr->sum.result, 7);
    EXPECT_EQ(ptr->s, std::string("non-pod"));
    EXPECT_EQ(type::get_count(), 1);
  }
  EXPECT_EQ(type::get_count(), 0);
}
REGISTER_TYPED_TEST_SUITE_P(ut0new_make_shared_aligned_non_pod_types,
                            non_pod_types);
INSTANTIATE_TYPED_TEST_SUITE_P(My, ut0new_make_shared_aligned_non_pod_types,
                               all_non_pod_types);

// make_shared_aligned - default constructible non-pod types
template <typename T>
class ut0new_make_shared_aligned_default_constructible_non_pod_types
    : public ::testing::Test {};
TYPED_TEST_SUITE_P(
    ut0new_make_shared_aligned_default_constructible_non_pod_types);
TYPED_TEST_P(ut0new_make_shared_aligned_default_constructible_non_pod_types,
             default_constructible_non_pod_types) {
  using type = typename TypeParam::type;
  auto with_pfs = TypeParam::with_pfs;
  auto alignment = 4 * alignof(std::max_align_t);

  {
    auto ptr = with_pfs ? ut::make_shared_aligned<type>(
                              ut::make_psi_memory_key(pfs_key), alignment)
                        : ut::make_shared_aligned<type>(alignment);
    EXPECT_TRUE(reinterpret_cast<std::uintptr_t>(ptr.get()) % alignment == 0);
    EXPECT_EQ(ptr->x, 0);
    EXPECT_EQ(ptr->y, 1);
    EXPECT_EQ(ptr->s, std::string("non-pod-string"));
  }
}
REGISTER_TYPED_TEST_SUITE_P(
    ut0new_make_shared_aligned_default_constructible_non_pod_types,
    default_constructible_non_pod_types);
INSTANTIATE_TYPED_TEST_SUITE_P(
    My, ut0new_make_shared_aligned_default_constructible_non_pod_types,
    all_default_constructible_non_pod_types);

// TODO macOS build of clang on PB2 currently exhibits a bug in which
// std::shared_ptr with array types is not usable. Please see above for more in
// depth explanation.
#if !defined(__APPLE__)
// make_shared_aligned - array specialization for fundamental types
template <typename T>
class ut0new_make_shared_aligned_fundamental_types_arr
    : public ::testing::Test {};
TYPED_TEST_SUITE_P(ut0new_make_shared_aligned_fundamental_types_arr);
TYPED_TEST_P(ut0new_make_shared_aligned_fundamental_types_arr,
             fundamental_types) {
  using type = typename TypeParam::type;
  auto with_pfs = TypeParam::with_pfs;
  auto alignment = 4 * alignof(std::max_align_t);
  auto ptr = with_pfs ? ut::make_shared_aligned<type[]>(
                            ut::make_psi_memory_key(pfs_key), alignment, 3)
                      : ut::make_shared_aligned<type[]>(alignment, 3);
  EXPECT_TRUE(reinterpret_cast<std::uintptr_t>(ptr.get()) % alignment == 0);
  EXPECT_EQ(ptr[0], type{});
  EXPECT_EQ(ptr[1], type{});
  EXPECT_EQ(ptr[2], type{});
}
REGISTER_TYPED_TEST_SUITE_P(ut0new_make_shared_aligned_fundamental_types_arr,
                            fundamental_types);
INSTANTIATE_TYPED_TEST_SUITE_P(My,
                               ut0new_make_shared_aligned_fundamental_types_arr,
                               all_fundamental_types);

// make_shared_aligned - array specialization for default_constructible POD
// types
template <typename T>
class ut0new_make_shared_aligned_default_constructible_pod_types_arr
    : public ::testing::Test {};
TYPED_TEST_SUITE_P(
    ut0new_make_shared_aligned_default_constructible_pod_types_arr);
TYPED_TEST_P(ut0new_make_shared_aligned_default_constructible_pod_types_arr,
             default_constructible_pod_types) {
  using type = typename TypeParam::type;
  auto with_pfs = TypeParam::with_pfs;
  auto alignment = 4 * alignof(std::max_align_t);
  auto ptr = with_pfs ? ut::make_shared_aligned<type[]>(
                            ut::make_psi_memory_key(pfs_key), alignment, 3)
                      : ut::make_shared_aligned<type[]>(alignment, 3);
  EXPECT_TRUE(reinterpret_cast<std::uintptr_t>(ptr.get()) % alignment == 0);
  EXPECT_EQ(ptr[0].x, 0);
  EXPECT_EQ(ptr[0].y, 1);
  EXPECT_EQ(ptr[1].x, 0);
  EXPECT_EQ(ptr[1].y, 1);
  EXPECT_EQ(ptr[2].x, 0);
  EXPECT_EQ(ptr[2].y, 1);
}
REGISTER_TYPED_TEST_SUITE_P(
    ut0new_make_shared_aligned_default_constructible_pod_types_arr,
    default_constructible_pod_types);
INSTANTIATE_TYPED_TEST_SUITE_P(
    My, ut0new_make_shared_aligned_default_constructible_pod_types_arr,
    all_default_constructible_pod_types);
#endif

}  // namespace innodb_ut0new_unittest
