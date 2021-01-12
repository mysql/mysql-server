/* Copyright (c) 2014, 2020, Oracle and/or its affiliates.

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

// First include (the generated) my_config.h, to get correct platform defines.
#include "my_config.h"

#include <gtest/gtest.h>
#include <stddef.h>

#include "storage/innobase/include/detail/ut0new.h"
#include "storage/innobase/include/univ.i"
#include "storage/innobase/include/ut0new.h"

namespace innodb_ut0new_unittest {

static void start() { ut_new_boot_safe(); }

using int_types =
    ::testing::Types<short int, unsigned short int, int, unsigned int, long int,
                     unsigned long int, long long int, unsigned long long int>;

using char_types = ::testing::Types<char, unsigned char, wchar_t>;

using floating_point_types = ::testing::Types<float, double, long double>;

/**
 * This is a typed test template, it's instantiated below for all primitive
 * types. This way we can cover all the supported fundamental alignments and
 * sizes.
 */
template <class T>
class ut0new_t : public ::testing::Test {
 protected:
  ut_allocator<T> allocator;
};

template <class T>
struct wrapper {
 public:
  static constexpr T INIT_VAL = std::numeric_limits<T>::min() + 1;
  wrapper(T data = INIT_VAL) : data(data) {}
  T data;
};

template <class T>
constexpr T wrapper<T>::INIT_VAL;

TYPED_TEST_SUITE_P(ut0new_t);

TYPED_TEST_P(ut0new_t, ut_new_fundamental_types) {
  start();
  const auto MAX = std::numeric_limits<TypeParam>::max();
  auto p = UT_NEW_NOKEY(TypeParam(MAX));
  EXPECT_EQ(*p, MAX);
  UT_DELETE(p);

  p = UT_NEW(TypeParam(MAX - 1), mem_key_buf_buf_pool);
  EXPECT_EQ(*p, MAX - 1);
  UT_DELETE(p);

  const int CNT = 5;
  p = UT_NEW_ARRAY_NOKEY(TypeParam, CNT);
  for (int i = 0; i < CNT; ++i) {
    p[i] = MAX;
    EXPECT_EQ(p[i], MAX);
  }
  UT_DELETE_ARRAY(p);

  p = UT_NEW_ARRAY(TypeParam, CNT, mem_key_buf_buf_pool);
  for (int i = 0; i < CNT; ++i) {
    p[i] = MAX - 1;
    EXPECT_EQ(p[i], MAX - 1);
  }
  UT_DELETE_ARRAY(p);
}

TYPED_TEST_P(ut0new_t, ut_new_structs) {
  start();
  const auto MAX = std::numeric_limits<TypeParam>::max();

  using w = wrapper<TypeParam>;

  auto p = UT_NEW_NOKEY(w(TypeParam(MAX)));
  EXPECT_EQ(p->data, MAX);
  UT_DELETE(p);

  p = UT_NEW(w(TypeParam(MAX - 1)), mem_key_buf_buf_pool);
  EXPECT_EQ(p->data, MAX - 1);
  UT_DELETE(p);

  const int CNT = 5;

  p = UT_NEW_ARRAY_NOKEY(w, CNT);
  for (int i = 0; i < CNT; ++i) {
    EXPECT_EQ(w::INIT_VAL, p[i].data);
  }
  UT_DELETE_ARRAY(p);

  p = UT_NEW_ARRAY(w, CNT, mem_key_buf_buf_pool);
  for (int i = 0; i < CNT; ++i) {
    EXPECT_EQ(w::INIT_VAL, p[i].data);
  }
  UT_DELETE_ARRAY(p);
}

TYPED_TEST_P(ut0new_t, ut_malloc) {
  start();
  TypeParam *p;
  const auto MAX = std::numeric_limits<TypeParam>::max();
  const auto MIN = std::numeric_limits<TypeParam>::min();

  p = static_cast<TypeParam *>(ut_malloc_nokey(sizeof(TypeParam)));
  *p = MIN;
  ut_free(p);

  p = static_cast<TypeParam *>(
      ut_malloc(sizeof(TypeParam), mem_key_buf_buf_pool));
  *p = MAX;
  ut_free(p);

  p = static_cast<TypeParam *>(ut_zalloc_nokey(sizeof(TypeParam)));
  EXPECT_EQ(0, *p);
  *p = MAX;
  ut_free(p);

  p = static_cast<TypeParam *>(
      ut_zalloc(sizeof(TypeParam), mem_key_buf_buf_pool));
  EXPECT_EQ(0, *p);
  *p = MAX;
  ut_free(p);

  p = static_cast<TypeParam *>(ut_malloc_nokey(sizeof(TypeParam)));
  *p = MAX - 1;
  p = static_cast<TypeParam *>(ut_realloc(p, 2 * sizeof(TypeParam)));
  EXPECT_EQ(MAX - 1, p[0]);
  p[1] = MAX;
  ut_free(p);
}

/* test ut_allocator() */
TYPED_TEST_P(ut0new_t, ut_vector) {
  start();

  typedef ut_allocator<TypeParam> vec_allocator_t;
  typedef std::vector<TypeParam, vec_allocator_t> vec_t;
  const auto MAX = std::numeric_limits<TypeParam>::max();
  const auto MIN = std::numeric_limits<TypeParam>::min();

  vec_t v1;
  v1.push_back(MIN);
  v1.push_back(MIN + 1);
  v1.push_back(MAX);
  EXPECT_EQ(MIN, v1[0]);
  EXPECT_EQ(MIN + 1, v1[1]);
  EXPECT_EQ(MAX, v1[2]);

  /* We use "new" instead of "UT_NEW()" for simplicity here. Real InnoDB
  code should use UT_NEW(). */

  /* This could of course be written as:
  std::vector<int, ut_allocator<int> >*	v2
  = new std::vector<int, ut_allocator<int> >(ut_allocator<int>(
  mem_key_buf_buf_pool)); */
  vec_t *v2 = new vec_t(vec_allocator_t(mem_key_buf_buf_pool));
  v2->push_back(MIN);
  v2->push_back(MIN + 1);
  v2->push_back(MAX);
  EXPECT_EQ(MIN, v2->at(0));
  EXPECT_EQ(MIN + 1, v2->at(1));
  EXPECT_EQ(MAX, v2->at(2));
  delete v2;
}

REGISTER_TYPED_TEST_SUITE_P(ut0new_t, ut_new_fundamental_types, ut_new_structs,
                            ut_malloc, ut_vector);

INSTANTIATE_TYPED_TEST_SUITE_P(int_types, ut0new_t, int_types);
INSTANTIATE_TYPED_TEST_SUITE_P(float_types, ut0new_t, floating_point_types);
INSTANTIATE_TYPED_TEST_SUITE_P(char_types, ut0new_t, char_types);
INSTANTIATE_TYPED_TEST_SUITE_P(bool, ut0new_t, bool);

static int n_construct = 0;

class cc_t {
 public:
  cc_t() {
    n_construct++;
    if (n_construct % 4 == 0) {
      throw(1);
    }
  }
};

struct big_t {
  char x[128];
};

/* test edge cases */
TEST(ut0new, edgecases) {
  ut_allocator<byte> alloc1(mem_key_buf_buf_pool);
  ut_new_pfx_t pfx;
  void *ret;
  const void *null_ptr = nullptr;

  ret = alloc1.allocate_large(0, &pfx);
  EXPECT_EQ(null_ptr, ret);

#ifdef UNIV_PFS_MEMORY
  ret = alloc1.allocate(16);
  ASSERT_TRUE(ret != nullptr);
  ret = alloc1.reallocate(ret, 0, UT_NEW_THIS_FILE_PSI_KEY);
  EXPECT_EQ(null_ptr, ret);

  ret = UT_NEW_ARRAY_NOKEY(byte, 0);
  EXPECT_EQ(null_ptr, ret);
#endif /* UNIV_PFS_MEMORY */

  ut_allocator<big_t> alloc2(mem_key_buf_buf_pool);

  const ut_allocator<big_t>::size_type too_many_elements =
      std::numeric_limits<ut_allocator<big_t>::size_type>::max() /
          sizeof(big_t) +
      1;

#ifdef UNIV_PFS_MEMORY
  ret = alloc2.allocate(16);
  ASSERT_TRUE(ret != nullptr);
  void *ret2 =
      alloc2.reallocate(ret, too_many_elements, UT_NEW_THIS_FILE_PSI_KEY);
  EXPECT_EQ(null_ptr, ret2);
  /* If reallocate fails due to too many elements,
  memory is still allocated. Do explicit deallocate do avoid mem leak. */
  alloc2.deallocate(static_cast<big_t *>(ret));
#endif /* UNIV_PFS_MEMORY */

  bool threw = false;

  try {
    ret = alloc2.allocate(too_many_elements);
  } catch (...) {
    threw = true;
  }
  EXPECT_TRUE(threw);

  ret = alloc2.allocate(too_many_elements, nullptr, PSI_NOT_INSTRUMENTED, false,
                        false);
  EXPECT_EQ(null_ptr, ret);

  threw = false;
  try {
    cc_t *cc = UT_NEW_ARRAY_NOKEY(cc_t, 16);
    /* Not reached, but silence a compiler warning
    about unused 'cc': */
    ASSERT_TRUE(cc != nullptr);
  } catch (...) {
    threw = true;
  }
  EXPECT_TRUE(threw);
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
      : x(_x), y(_y), s(_s), sum(std::make_unique<My_fancy_sum>(x, y)) {}
  int x;
  int y;
  std::string s;
  std::unique_ptr<My_fancy_sum> sum;
};
struct Default_constructible_pod {
  Default_constructible_pod() : x(0), y(1) {}
  int x, y;
};
struct Default_constructible_non_pod {
  Default_constructible_non_pod() : x(0), y(1), s("non-pod-string") {}
  int x, y;
  std::string s;
};

using all_fundamental_types =
    ::testing::Types<char, unsigned char, wchar_t, short int,
                     unsigned short int, int, unsigned int, long int,
                     unsigned long int, long long int, unsigned long long int,
                     float, double, long double>;
using all_pod_types = ::testing::Types<Pod_type>;
using all_non_pod_types = ::testing::Types<Non_pod_type>;

// aligned alloc/free - fundamental types
template <typename T>
class aligned_alloc_free_fundamental_types : public ::testing::Test {};
TYPED_TEST_SUITE_P(aligned_alloc_free_fundamental_types);
TYPED_TEST_P(aligned_alloc_free_fundamental_types, fundamental_types) {
  using type = TypeParam;
  for (auto alignment = 2 * alignof(std::max_align_t);
       alignment < 1024 * 1024 + 1; alignment *= 2) {
    type *ptr = static_cast<type *>(ut::aligned_alloc(sizeof(type), alignment));
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
  using type = TypeParam;
  auto alignment = 4 * 1024;
  type *ptr = static_cast<type *>(ut::aligned_alloc(sizeof(type), alignment));
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
  using type = TypeParam;
  auto alignment = 4 * 1024;
  type *ptr = static_cast<type *>(ut::aligned_alloc(sizeof(type), alignment));
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
INSTANTIATE_TYPED_TEST_SUITE_P(PodTypes, aligned_alloc_free_non_pod_types,
                               all_non_pod_types);

// aligned alloc/free - array specialization for fundamental types
template <typename T>
class aligned_alloc_free_fundamental_types_arr : public ::testing::Test {};
TYPED_TEST_SUITE_P(aligned_alloc_free_fundamental_types_arr);
TYPED_TEST_P(aligned_alloc_free_fundamental_types_arr, fundamental_types) {
  using type = TypeParam;
  size_t n_elements = 1234;
  for (auto alignment = 2 * alignof(std::max_align_t);
       alignment < 1024 * 1024 + 1; alignment *= 2) {
    type *ptr = static_cast<type *>(ut::detail::Aligned_alloc_arr::alloc(
        sizeof(type), n_elements, alignment));
    EXPECT_TRUE(reinterpret_cast<std::uintptr_t>(ptr) % alignment == 0);
    ut::aligned_free(ptr);
  }
}
REGISTER_TYPED_TEST_SUITE_P(aligned_alloc_free_fundamental_types_arr,
                            fundamental_types);
INSTANTIATE_TYPED_TEST_SUITE_P(My, aligned_alloc_free_fundamental_types_arr,
                               all_fundamental_types);

// aligned alloc/free - array specialization for pod types
template <typename T>
class aligned_alloc_free_pod_types_arr : public ::testing::Test {};
TYPED_TEST_SUITE_P(aligned_alloc_free_pod_types_arr);
TYPED_TEST_P(aligned_alloc_free_pod_types_arr, pod_types) {
  using type = TypeParam;
  size_t n_elements = 1234;
  for (auto alignment = 2 * alignof(std::max_align_t);
       alignment < 1024 * 1024 + 1; alignment *= 2) {
    type *ptr = static_cast<type *>(ut::detail::Aligned_alloc_arr::alloc(
        sizeof(type), n_elements, alignment));
    EXPECT_TRUE(reinterpret_cast<std::uintptr_t>(ptr) % alignment == 0);
    ut::aligned_free(ptr);
  }
}
REGISTER_TYPED_TEST_SUITE_P(aligned_alloc_free_pod_types_arr, pod_types);
INSTANTIATE_TYPED_TEST_SUITE_P(My, aligned_alloc_free_pod_types_arr,
                               all_pod_types);

// aligned alloc/free - array specialization for non-pod types
template <typename T>
class aligned_alloc_free_non_pod_types_arr : public ::testing::Test {};
TYPED_TEST_SUITE_P(aligned_alloc_free_non_pod_types_arr);
TYPED_TEST_P(aligned_alloc_free_non_pod_types_arr, non_pod_types) {
  using type = TypeParam;
  size_t n_elements = 1234;
  for (auto alignment = 2 * alignof(std::max_align_t);
       alignment < 1024 * 1024 + 1; alignment *= 2) {
    type *ptr = static_cast<type *>(ut::detail::Aligned_alloc_arr::alloc(
        sizeof(type), n_elements, alignment));
    // Referencing non-pod type members through returned pointer is UB.
    // Solely releasing it is ok.
    //
    // Using it otherwise is UB because aligned_alloc_* functions are raw
    // memory management functions which do not invoke constructors neither
    // they know which type they are operating with. That is why we would be end
    // up accessing memory of not yet instantiated object (UB).
    EXPECT_TRUE(reinterpret_cast<std::uintptr_t>(ptr) % alignment == 0);
    ut::aligned_free(ptr);
  }
}
REGISTER_TYPED_TEST_SUITE_P(aligned_alloc_free_non_pod_types_arr,
                            non_pod_types);
INSTANTIATE_TYPED_TEST_SUITE_P(My, aligned_alloc_free_non_pod_types_arr,
                               all_non_pod_types);

// aligned new/delete - fundamental types
template <typename T>
class aligned_new_delete_fundamental_types : public ::testing::Test {};
TYPED_TEST_SUITE_P(aligned_new_delete_fundamental_types);
TYPED_TEST_P(aligned_new_delete_fundamental_types, fundamental_types) {
  using type = TypeParam;
  for (auto alignment = 2 * alignof(std::max_align_t);
       alignment < 1024 * 1024 + 1; alignment *= 2) {
    type *ptr = ut::aligned_new<type>(alignment, 1);
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
  using type = TypeParam;
  for (auto alignment = 2 * alignof(std::max_align_t);
       alignment < 1024 * 1024 + 1; alignment *= 2) {
    type *ptr = ut::aligned_new<type>(alignment, 2, 5);
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
  using type = TypeParam;
  for (auto alignment = 2 * alignof(std::max_align_t);
       alignment < 1024 * 1024 + 1; alignment *= 2) {
    type *ptr = ut::aligned_new<type>(alignment, 2, 5, std::string("non-pod"));
    EXPECT_TRUE(reinterpret_cast<std::uintptr_t>(ptr) % alignment == 0);
    EXPECT_EQ(ptr->x, 2);
    EXPECT_EQ(ptr->y, 5);
    EXPECT_EQ(ptr->sum->result, 7);
    EXPECT_EQ(ptr->s, std::string("non-pod"));
    ut::aligned_delete(ptr);
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
  using type = TypeParam;
  constexpr size_t n_elements = 10;
  for (auto alignment = 2 * alignof(std::max_align_t);
       alignment < 1024 * 1024 + 1; alignment *= 2) {
    type *ptr = ut::aligned_new_arr<type, n_elements>(alignment, 0, 1, 2, 3, 4,
                                                      5, 6, 7, 8, 9);

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
  using type = TypeParam;
  constexpr size_t n_elements = 5;
  for (auto alignment = 2 * alignof(std::max_align_t);
       alignment < 1024 * 1024 + 1; alignment *= 2) {
    type *ptr = ut::aligned_new_arr<type, n_elements>(alignment, 0, 1, 2, 3, 4,
                                                      5, 6, 7, 8, 9);

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
  using type = TypeParam;
  constexpr size_t n_elements = 5;
  for (auto alignment = 2 * alignof(std::max_align_t);
       alignment < 1024 * 1024 + 1; alignment *= 2) {
    type *ptr = ut::aligned_new_arr<type, n_elements>(
        alignment, 1, 2, std::string("a"), 3, 4, std::string("b"), 5, 6,
        std::string("c"), 7, 8, std::string("d"), 9, 10, std::string("e"));

    EXPECT_TRUE(reinterpret_cast<std::uintptr_t>(ptr) % alignment == 0);

    EXPECT_EQ(ptr[0].x, 1);
    EXPECT_EQ(ptr[0].y, 2);
    EXPECT_TRUE(ptr[0].s == std::string("a"));
    EXPECT_EQ(ptr[0].sum->result, 3);

    EXPECT_EQ(ptr[1].x, 3);
    EXPECT_EQ(ptr[1].y, 4);
    EXPECT_TRUE(ptr[1].s == std::string("b"));
    EXPECT_EQ(ptr[1].sum->result, 7);

    EXPECT_EQ(ptr[2].x, 5);
    EXPECT_EQ(ptr[2].y, 6);
    EXPECT_TRUE(ptr[2].s == std::string("c"));
    EXPECT_EQ(ptr[2].sum->result, 11);

    EXPECT_EQ(ptr[3].x, 7);
    EXPECT_EQ(ptr[3].y, 8);
    EXPECT_TRUE(ptr[3].s == std::string("d"));
    EXPECT_EQ(ptr[3].sum->result, 15);

    EXPECT_EQ(ptr[4].x, 9);
    EXPECT_EQ(ptr[4].y, 10);
    EXPECT_TRUE(ptr[4].s == std::string("e"));
    EXPECT_EQ(ptr[4].sum->result, 19);

    ut::aligned_delete_arr(ptr);
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
  using type = TypeParam;
  constexpr size_t n_elements = 5;
  for (auto alignment = 2 * alignof(std::max_align_t);
       alignment < 1024 * 1024 + 1; alignment *= 2) {
    type *ptr = ut::aligned_new_arr<type>(alignment, n_elements);

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
  using type = TypeParam;
  constexpr size_t n_elements = 5;
  for (auto alignment = 2 * alignof(std::max_align_t);
       alignment < 1024 * 1024 + 1; alignment *= 2) {
    type *ptr = ut::aligned_new_arr<type>(alignment, n_elements);

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
    Default_constructible_pod);

// aligned new/delete - array specialization for default constructible non-pod
// types
template <typename T>
class aligned_new_delete_default_constructible_non_pod_types_arr
    : public ::testing::Test {};
TYPED_TEST_SUITE_P(aligned_new_delete_default_constructible_non_pod_types_arr);
TYPED_TEST_P(aligned_new_delete_default_constructible_non_pod_types_arr,
             pod_types) {
  using type = TypeParam;
  constexpr size_t n_elements = 5;
  for (auto alignment = 2 * alignof(std::max_align_t);
       alignment < 1024 * 1024 + 1; alignment *= 2) {
    type *ptr = ut::aligned_new_arr<type>(alignment, n_elements);

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
    Default_constructible_non_pod);

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
  constexpr auto n_elements = 5;
  constexpr auto alignment = 4 * 1024;
  struct Aligned_int_arr_deleter {
    void operator()(int *p) {
      std::cout << "Hello from custom deleter!\n";
      ut::aligned_delete_arr(p);
    }
  };
  std::unique_ptr<int, Aligned_int_arr_deleter> ptr(
      ut::aligned_new_arr<int, n_elements>(alignment, 1, 2, 3, 4, 5),
      Aligned_int_arr_deleter{});
}

}  // namespace innodb_ut0new_unittest
