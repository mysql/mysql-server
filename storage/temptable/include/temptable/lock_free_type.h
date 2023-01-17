/* Copyright (c) 2020, 2023, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/** @file storage/temptable/include/temptable/lock_free_type.h
Lock-free type (selection) implementation. */

#ifndef TEMPTABLE_LOCK_FREE_TYPE_H
#define TEMPTABLE_LOCK_FREE_TYPE_H

#include <atomic>
#include <type_traits>

#include "my_config.h"
#include "storage/temptable/include/temptable/constants.h"

namespace temptable {

/** Clang has a bug which causes ATOMIC_LLONG_LOCK_FREE to be defined as 1
 * (or "sometimes lock-free") in 32-bit builds even though
 * __atomic_always_lock_free returns true for the same type on the same
 * platform. This is an inconsistency which can be easily verified by:
 *
 *   % clang -dM -E -x c /dev/null | grep LLONG_LOCK
 *   #define __CLANG_ATOMIC_LLONG_LOCK_FREE 2
 *   #define __GCC_ATOMIC_LLONG_LOCK_FREE 2
 *
 *   % clang -m32 -dM -E -x c /dev/null | grep LLONG_LOCK
 *   #define __CLANG_ATOMIC_LLONG_LOCK_FREE 1
 *   #define __GCC_ATOMIC_LLONG_LOCK_FREE 1
 *
 *   % gcc -dM -E -x c /dev/null | grep LLONG_LOCK
 *   #define __GCC_ATOMIC_LLONG_LOCK_FREE 2
 *
 *   % gcc -m32 -dM -E -x c /dev/null | grep LLONG_LOCK
 *   #define __GCC_ATOMIC_LLONG_LOCK_FREE 2
 *
 * There has been some work towards fixing this issue:
 *  * https://bugs.llvm.org/show_bug.cgi?id=30581
 *      * Introduces the fix for the aforementioned problem but ...
 *  * https://bugs.llvm.org/show_bug.cgi?id=31864
 *      * ... reverts the fix because it breaks some other targets, e.g.
 *        32-bit FreeBSD
 *
 * Some more links:
 *  * https://reviews.llvm.org/D28213
 *  * https://reviews.llvm.org/D29542
 */
#if defined(__clang__) && (SIZEOF_VOIDP == 4) && (ATOMIC_LLONG_LOCK_FREE == 1)
#define WORKAROUND_PR31864_CLANG_BUG (1)
#else
#define WORKAROUND_PR31864_CLANG_BUG (0)
#endif

/** Enum class describing alignment-requirements. */
enum class Alignment { NATURAL, L1_DCACHE_SIZE };

/** Lock-free type selector, a helper utility which evaluates during
 * the compile-time whether the given type T has a property of being
 * always-lock-free for given platform. If true, Lock_free_type_selector::Type
 * will hold T, otherwise Lock_free_type_selector::Type will be inexisting in
 * which case static-assert will be triggered with a hopefully descriptive
 * error-message. In the event of static-assert, one can either try to select
 * another type T or, if one does not care about the actual underlying
 * type representation, simply utilize the `Largest_lock_free_type_selector`
 * utility instead. This utility will work out those details automagically. For
 * more information, see documentation on `Largest_lock_free_type_selector`.
 *
 * In short, reasoning behind this machinery lies in the fact that the standard
 * cannot guarantee that underlying implementation of std::atomic<T> is going
 * to be able to use lock-free atomic CPU instructions. That obviously depends
 * on the given type T but also on the properties of concrete platform.
 * Therefore, actual implementation is mostly platform-dependent and is
 * free to choose any other locking operation (e.g. mutex) as long as it is
 * able to fulfill the atomicity. Lock-freedom is not a pre-requisite. Only
 * exception is std::atomic_flag.
 *
 * For certain types, however, lock-freedom can be claimed upfront during the
 * compile-time phase, and this is where this utiliy kicks in. It is essentially
 * a C++14 rewrite of std::atomic<T>::is_always_lock_free which is only
 * available from C++17 onwards. Once moved to C++17 this utility will become
 * obsolete and shall be replaced with standard-compliant implementation.
 *
 * More details and motivation can be found at:
 *   http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2015/p0152r0.html
 * */
template <typename T, typename V = void>
struct Lock_free_type_selector {
  static_assert(
      !std::is_same<T, T>::value,
      "No always-lock-free property could be found for given type. "
      "Type provided is probably not a built-in (fundamental) type or a "
      "pointer which makes it impossible for this particular check to be "
      "excercised at compile-time.");
};

/** Template-specialization for trivially-copyable classes/structs.
 *
 * Subset of trivially-copyable classes/structs might have always-lock-free
 * property but for this feature to be implemented we would have to go at great
 * lengths to implement cross-platform support. Therefore, for simplicity
 * reasons let's just detect the overload and fail gracefully.
 * */
template <typename T>
struct Lock_free_type_selector<
    T, typename std::enable_if<std::is_class<T>::value and
                               std::is_trivially_copyable<T>::value>::type> {
  static_assert(!std::is_same<T, T>::value,
                "Querying always-lock-free property of trivially-copyable "
                "classes or structs is not yet implemented!");
};

/** Template-specialization for pointer types. */
template <typename T>
struct Lock_free_type_selector<
    T, typename std::enable_if<std::is_pointer<T>::value>::type> {
#if (ATOMIC_POINTER_LOCK_FREE == 2)
  using Type = T;
#else
  static_assert(false,
                "Pointer type on this platform does not have an "
                "always-lock-free property. Bailing out ...");
#endif
};

/** Template-specialization for long long types. */
template <typename T>
struct Lock_free_type_selector<
    T,
    typename std::enable_if<std::is_same<T, long long>::value or
                            std::is_same<T, unsigned long long>::value>::type> {
#if (ATOMIC_LLONG_LOCK_FREE == 2) || (WORKAROUND_PR31864_CLANG_BUG == 1)
  using Type = T;
#else
  static_assert(false,
                "(unsigned) long long type on this platform does not have an "
                "always-lock-free property. Bailing out ...");
#endif
};

/** Template-specialization for long types. */
template <typename T>
struct Lock_free_type_selector<
    T, typename std::enable_if<std::is_same<T, long>::value or
                               std::is_same<T, unsigned long>::value>::type> {
#if (ATOMIC_LONG_LOCK_FREE == 2)
  using Type = T;
#else
  static_assert(false,
                "(unsigned) long type on this platform does not have an "
                "always-lock-free property. Bailing out ...");
#endif
};

/** Template-specialization for int types. */
template <typename T>
struct Lock_free_type_selector<
    T, typename std::enable_if<std::is_same<T, int>::value or
                               std::is_same<T, unsigned int>::value>::type> {
#if (ATOMIC_INT_LOCK_FREE == 2)
  using Type = T;
#else
  static_assert(false,
                "(unsigned) int type on this platform does not have an "
                "always-lock-free property. Bailing out ...");
#endif
};

/** Template-specialization for short types. */
template <typename T>
struct Lock_free_type_selector<
    T, typename std::enable_if<std::is_same<T, short>::value or
                               std::is_same<T, unsigned short>::value>::type> {
#if (ATOMIC_SHORT_LOCK_FREE == 2)
  using Type = T;
#else
  static_assert(false,
                "(unsigned) short type on this platform does not have an "
                "always-lock-free property. Bailing out ...");
#endif
};

/** Template-specialization for char types. */
template <typename T>
struct Lock_free_type_selector<
    T, typename std::enable_if<std::is_same<T, char>::value or
                               std::is_same<T, unsigned char>::value>::type> {
#if (ATOMIC_CHAR_LOCK_FREE == 2)
  using Type = T;
#else
  static_assert(false,
                "(unsigned) char type on this platform does not have an "
                "always-lock-free property. Bailing out ...");
#endif
};

/** Template-specialization for boolean types. */
template <typename T>
struct Lock_free_type_selector<
    T, typename std::enable_if<std::is_same<T, bool>::value>::type> {
#if (ATOMIC_BOOL_LOCK_FREE == 2)
  using Type = T;
#else
  static_assert(false,
                "bool type on this platform does not have an "
                "always-lock-free property. Bailing out ...");
#endif
};

/** Largest lock-free type selector, a helper utility very much similar
 * to Lock_free_type_selector with the difference being that it tries hard
 * not to fail. E.g. it will try to find the largest available T for given
 * platform which has a property of being always-lock-free. T which has been
 * selected is then found in Largest_lock_free_type_selector::Type.
 * Signedness of T is respected.
 * */
template <typename T, typename V = void>
struct Largest_lock_free_type_selector {
  static_assert(
      !std::is_same<T, T>::value,
      "No always-lock-free property could be found for given type. "
      "Type provided is probably not a built-in (fundamental) type or a "
      "pointer which makes it impossible for this particular check to be "
      "excercised at compile-time.");
};

/** Template-specialization for pointer types. */
template <typename T>
struct Largest_lock_free_type_selector<
    T, typename std::enable_if<std::is_pointer<T>::value>::type> {
#if (ATOMIC_POINTER_LOCK_FREE == 2)
  using Type = T;
#else
  static_assert(false,
                "Pointer type on this platform does not have an "
                "always-lock-free property. Bailing out ...");
#endif
};

/** Template-specialization for integral types. */
template <typename T>
struct Largest_lock_free_type_selector<
    T, typename std::enable_if<std::is_integral<T>::value>::type> {
#if (ATOMIC_LLONG_LOCK_FREE == 2) || (WORKAROUND_PR31864_CLANG_BUG == 1)
  using Type = std::conditional_t<std::is_unsigned<T>::value,
                                  unsigned long long, long long>;
#elif (ATOMIC_LONG_LOCK_FREE == 2)
  using Type =
      std::conditional_t<std::is_unsigned<T>::value, unsigned long, long>;
#elif (ATOMIC_INT_LOCK_FREE == 2)
  using Type =
      std::conditional_t<std::is_unsigned<T>::value, unsigned int, int>;
#elif (ATOMIC_SHORT_LOCK_FREE == 2)
  using Type =
      std::conditional_t<std::is_unsigned<T>::value, unsigned short, short>;
#elif (ATOMIC_CHAR_LOCK_FREE == 2)
  using Type =
      std::conditional_t<std::is_unsigned<T>::value, unsigned char, char>;
#elif (ATOMIC_BOOL_LOCK_FREE == 2)
  using Type = bool;
#else
  static_assert(
      false,
      "No suitable always-lock-free type was found for this platform. "
      "Bailing out ...");
#endif
};

/** Representation of an atomic type which is guaranteed to be always-lock-free.
 * In case always-lock-free property cannot be satisfied for given T,
 * Lock_free_type instantiation will fail with the compile-time error.
 *
 * Always-lock-free guarantee is implemented through the means of
 * Lock_free_type_selector or Largest_lock_free_type_selector. User code can
 * opt-in for any of those. By default, Lock_free_type_selector is used.
 *
 * In addition, this type provides an ability to redefine the
 * alignment-requirement of the underlying always-lock-free type, basically
 * making it possible to over-align T to the size of the L1-data cache-line
 * size. By default, T has a natural alignment.
 */
template <typename T, Alignment ALIGN = Alignment::NATURAL,
          template <typename, typename = void> class TypeSelector =
              Lock_free_type_selector>
struct Lock_free_type {
  using Type = typename TypeSelector<T>::Type;
  std::atomic<Type> m_value;
};

/*
 * Template-specialization for Lock_free_type with alignment-requirement set to
 * L1-data cache size.
 * */
template <typename T, template <typename, typename = void> class TypeSelector>
struct Lock_free_type<T, Alignment::L1_DCACHE_SIZE, TypeSelector> {
  using Type = typename TypeSelector<T>::Type;
  alignas(L1_DCACHE_SIZE) std::atomic<Type> m_value;
};

}  // namespace temptable

#endif /* TEMPTABLE_LOCK_FREE_TYPE_H */
