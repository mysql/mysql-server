/*****************************************************************************
Copyright (c) 1995, 2018, Oracle and/or its affiliates. All Rights Reserved.
Copyright (c) 2008, Google Inc.

Portions of this file contain modifications contributed and copyrighted by
Google, Inc. Those modifications are gratefully acknowledged and are described
briefly in the InnoDB documentation. The contributions by Google are
incorporated with their permission, and subject to the conditions contained in
the file COPYING.Google.

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
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file include/os0atomic.h
 Macros for using atomics

 Created 2012-09-23 Sunny Bains (Split from os0sync.h)
 *******************************************************/

#ifndef os0atomic_h
#define os0atomic_h

#include "univ.i"

#ifdef _WIN32

/** On Windows, InterlockedExchange operates on LONG variable */
typedef LONG lock_word_t;

#elif defined(MUTEX_FUTEX)

typedef int lock_word_t;

#else

typedef ulint lock_word_t;

#endif /* _WIN32 */

#if defined __i386__ || defined __x86_64__ || defined _M_IX86 || \
    defined _M_X64 || defined __WIN__

#define IB_STRONG_MEMORY_MODEL

#endif /* __i386__ || __x86_64__ || _M_IX86 || _M_X64 || __WIN__ */

/** Atomic compare-and-swap and increment for InnoDB. */

/** Do an atomic test and set.
@param[in,out]	ptr	Memory location to set
@param[in]	new_val	new value
@return	old value of memory location. */
UNIV_INLINE
lock_word_t os_atomic_test_and_set(volatile lock_word_t *ptr,
                                   lock_word_t new_val);

/** Do an atomic compare and set
@param[in,out]	ptr	Memory location to set
@param[in]	old_val	old value to compare
@param[in]	new_val	new value to set
@return the value of ptr before the operation. */
UNIV_INLINE
lock_word_t os_atomic_val_compare_and_swap(volatile lock_word_t *ptr,
                                           lock_word_t old_val,
                                           lock_word_t new_val);

#ifdef _WIN32

/** Atomic compare and exchange of signed integers (both 32 and 64 bit).
 @return value found before the exchange.
 If it is not equal to old_value the exchange did not happen. */
UNIV_INLINE
lint win_cmp_and_xchg_lint(
    volatile lint *ptr, /*!< in/out: source/destination */
    lint new_val,       /*!< in: exchange value */
    lint old_val);      /*!< in: value to compare to */

/** Atomic addition of signed integers.
 @return Initial value of the variable pointed to by ptr */
UNIV_INLINE
lint win_xchg_and_add(volatile lint *ptr, /*!< in/out: address of destination */
                      lint val);          /*!< in: number to be added */

/** Atomic compare and exchange of unsigned integers.
 @return value found before the exchange.
 If it is not equal to old_value the exchange did not happen. */
UNIV_INLINE
ulint win_cmp_and_xchg_ulint(
    volatile ulint *ptr, /*!< in/out: source/destination */
    ulint new_val,       /*!< in: exchange value */
    ulint old_val);      /*!< in: value to compare to */

/** Atomic compare and exchange of 32 bit unsigned integers.
 @return value found before the exchange.
 If it is not equal to old_value the exchange did not happen. */
UNIV_INLINE
DWORD
win_cmp_and_xchg_dword(volatile DWORD *ptr, /*!< in/out: source/destination */
                       DWORD new_val,       /*!< in: exchange value */
                       DWORD old_val);      /*!< in: value to compare to */

/** Returns true if swapped, ptr is pointer to target, old_val is value to
 compare to, new_val is the value to swap in. */

#define os_compare_and_swap_lint(ptr, old_val, new_val) \
  (win_cmp_and_xchg_lint(ptr, new_val, old_val) == old_val)

#define os_compare_and_swap_ulint(ptr, old_val, new_val) \
  (win_cmp_and_xchg_ulint(ptr, new_val, old_val) == old_val)

#define os_compare_and_swap_uint32(ptr, old_val, new_val)                  \
  (InterlockedCompareExchange(                                             \
       reinterpret_cast<volatile LONG *>(ptr), static_cast<LONG>(new_val), \
       static_cast<LONG>(old_val)) == static_cast<LONG>(old_val))

#define os_compare_and_swap_uint64(ptr, old_val, new_val)                   \
  (InterlockedCompareExchange64(reinterpret_cast<volatile LONGLONG *>(ptr), \
                                static_cast<LONGLONG>(new_val),             \
                                static_cast<LONGLONG>(old_val)) ==          \
   static_cast<LONGLONG>(old_val))

/* Windows thread HANDLEs are of type PVOID */
#define os_compare_and_swap_thread_id(ptr, old_val, new_val) \
  (InterlockedCompareExchangePointer(ptr, new_val, old_val) == old_val)

#define INNODB_RW_LOCKS_USE_ATOMICS
#define IB_ATOMICS_STARTUP_MSG \
  "Mutexes and rw_locks use Windows interlocked functions"

/** Returns the resulting value, ptr is pointer to target, amount is the
 amount of increment. */

#define os_atomic_increment_lint(ptr, amount) \
  (win_xchg_and_add(ptr, amount) + amount)

#define os_atomic_increment_ulint(ptr, amount)                                 \
  (static_cast<ulint>(win_xchg_and_add(reinterpret_cast<volatile lint *>(ptr), \
                                       static_cast<lint>(amount))) +           \
   static_cast<ulint>(amount))

#define os_atomic_increment_uint32(ptr, amount)                             \
  (static_cast<ulint>(InterlockedExchangeAdd(reinterpret_cast<long *>(ptr), \
                                             static_cast<long>(amount))) +  \
   static_cast<ulint>(amount))

#define os_atomic_increment_uint64(ptr, amount)                             \
  (static_cast<ib_uint64_t>(InterlockedExchangeAdd64(                       \
       reinterpret_cast<LONGLONG *>(ptr), static_cast<LONGLONG>(amount))) + \
   static_cast<ib_uint64_t>(amount))

/** Returns the resulting value, ptr is pointer to target, amount is the
 amount to decrement. There is no atomic substract function on Windows */

#define os_atomic_decrement_lint(ptr, amount) \
  (win_xchg_and_add(ptr, -(static_cast<lint>(amount))) - amount)

#define os_atomic_decrement_ulint(ptr, amount)                                 \
  (static_cast<ulint>(win_xchg_and_add(reinterpret_cast<volatile lint *>(ptr), \
                                       -(static_cast<lint>(amount)))) -        \
   static_cast<ulint>(amount))

#define os_atomic_decrement_uint32(ptr, amount)                        \
  (static_cast<ib_uint32_t>(InterlockedExchangeAdd(                    \
       reinterpret_cast<long *>(ptr), -(static_cast<long>(amount)))) - \
   static_cast<ib_uint32_t>(amount))

#define os_atomic_decrement_uint64(ptr, amount)                                \
  (static_cast<ib_uint64_t>(InterlockedExchangeAdd64(                          \
       reinterpret_cast<LONGLONG *>(ptr), -(static_cast<LONGLONG>(amount)))) - \
   static_cast<ib_uint64_t>(amount))

#else
/* Fall back to GCC-style atomic builtins. */

/** Returns true if swapped, ptr is pointer to target, old_val is value to
 compare to, new_val is the value to swap in. */

#if defined(HAVE_GCC_SYNC_BUILTINS)

#define os_compare_and_swap(ptr, old_val, new_val) \
  __sync_bool_compare_and_swap(ptr, old_val, new_val)

#define os_compare_and_swap_ulint(ptr, old_val, new_val) \
  os_compare_and_swap(ptr, old_val, new_val)

#define os_compare_and_swap_lint(ptr, old_val, new_val) \
  os_compare_and_swap(ptr, old_val, new_val)

#define os_compare_and_swap_uint32(ptr, old_val, new_val) \
  os_compare_and_swap(ptr, old_val, new_val)

#define os_compare_and_swap_uint64(ptr, old_val, new_val) \
  os_compare_and_swap(ptr, old_val, new_val)

#else

UNIV_INLINE
bool os_compare_and_swap_ulint(volatile ulint *ptr, ulint old_val,
                               ulint new_val) {
  return __atomic_compare_exchange_n(ptr, &old_val, new_val, 0,
                                     __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

UNIV_INLINE
bool os_compare_and_swap_lint(volatile lint *ptr, lint old_val, lint new_val) {
  return __atomic_compare_exchange_n(ptr, &old_val, new_val, 0,
                                     __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

UNIV_INLINE
bool os_compare_and_swap_uint32(volatile ib_uint32_t *ptr, ib_uint32_t old_val,
                                ib_uint32_t new_val) {
  return __atomic_compare_exchange_n(ptr, &old_val, new_val, 0,
                                     __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

UNIV_INLINE
bool os_compare_and_swap_uint64(volatile ib_uint64_t *ptr, ib_uint64_t old_val,
                                ib_uint64_t new_val) {
  return __atomic_compare_exchange_n(ptr, &old_val, new_val, 0,
                                     __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

#endif /* HAVE_GCC_SYNC_BUILTINS */

#ifdef HAVE_IB_ATOMIC_PTHREAD_T_GCC
#if defined(HAVE_GCC_SYNC_BUILTINS)
#define os_compare_and_swap_thread_id(ptr, old_val, new_val) \
  os_compare_and_swap(ptr, old_val, new_val)
#else
UNIV_INLINE
bool os_compare_and_swap_thread_id(volatile os_thread_id_t *ptr,
                                   os_thread_id_t old_val,
                                   os_thread_id_t new_val) {
  return __atomic_compare_exchange_n(ptr, &old_val, new_val, 0,
                                     __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}
#endif /* HAVE_GCC_SYNC_BUILTINS */
#define INNODB_RW_LOCKS_USE_ATOMICS
#define IB_ATOMICS_STARTUP_MSG "Mutexes and rw_locks use GCC atomic builtins"
#else /* HAVE_IB_ATOMIC_PTHREAD_T_GCC */
#define IB_ATOMICS_STARTUP_MSG \
  "Mutexes use GCC atomic builtins, rw_locks do not"
#endif /* HAVE_IB_ATOMIC_PTHREAD_T_GCC */

/** Same functions with no return value. These may have optimized
implementations on some architectures. */

#if defined(__aarch64__) && defined(HAVE_ARM64_LSE_ATOMICS)

#define ARM64_LSE_ATOMIC_STADD(ptr, amount, w, r)   \
  do {                                              \
    __asm__ __volatile__("stadd" w " %" r "1, %0\n" \
                         : "+Q"(*ptr)               \
                         : "r"(amount)              \
                         : "memory");               \
  } while (0)

#define os_atomic_increment_nr(ptr, amount)              \
  do {                                                   \
    switch (sizeof(*ptr)) {                              \
      case 1:                                            \
        ARM64_LSE_ATOMIC_STADD(ptr, amount, "b", "w");   \
        break;                                           \
      case 2:                                            \
        ARM64_LSE_ATOMIC_STADD(ptr, amount, "h", "w");   \
        break;                                           \
      case 4:                                            \
        ARM64_LSE_ATOMIC_STADD(ptr, amount, "", "w");    \
        break;                                           \
      case 8:                                            \
        ARM64_LSE_ATOMIC_STADD(ptr, amount, "", "");     \
        break;                                           \
      default:                                           \
        static_assert(true, "unsupported operand size"); \
    }                                                    \
  } while (0)
#else
#define os_atomic_increment_nr(ptr, amount) os_atomic_increment(ptr, amount)
#endif

#define os_atomic_increment_lint_nr(ptr, amount) \
  os_atomic_increment_nr(ptr, amount)

#define os_atomic_increment_ulint_nr(ptr, amount) \
  os_atomic_increment_nr(ptr, amount)

#define os_atomic_increment_uint32_nr(ptr, amount) \
  os_atomic_increment_nr(ptr, amount)

#define os_atomic_increment_uint64_nr(ptr, amount) \
  os_atomic_increment_nr(ptr, amount)

/* Non-atomic version of the functions with no return value. */

#if defined(__aarch64__) && defined(HAVE_ARM64_LSE_ATOMICS)
/* Atomic increment without fetching the original value is faster than nonatomic
one with fetching. */
#define os_nonatomic_increment_nr(ptr, amount) \
  os_atomic_increment_nr(ptr, amount)
#else
#define os_nonatomic_increment_nr(ptr, amount) (*(ptr) += (amount))
#endif

#define os_nonatomic_increment_lint_nr(ptr, amount) \
  os_nonatomic_increment_nr(ptr, amount)

#define os_nonatomic_increment_ulint_nr(ptr, amount) \
  os_nonatomic_increment_nr(ptr, amount)

#define os_nonatomic_increment_uint32_nr(ptr, amount) \
  os_nonatomic_increment_nr(ptr, amount)

#define os_nonatomic_increment_uint64_nr(ptr, amount) \
  os_nonatomic_increment_nr(ptr, amount)

/** Returns the resulting value, ptr is pointer to target, amount is the
 amount of increment. */

#if defined(HAVE_GCC_SYNC_BUILTINS)
#define os_atomic_increment(ptr, amount) __sync_add_and_fetch(ptr, amount)
#else
#define os_atomic_increment(ptr, amount) \
  __atomic_add_fetch(ptr, amount, __ATOMIC_SEQ_CST)
#endif /* HAVE_GCC_SYNC_BUILTINS */

#define os_atomic_increment_lint(ptr, amount) os_atomic_increment(ptr, amount)

#define os_atomic_increment_ulint(ptr, amount) os_atomic_increment(ptr, amount)

#define os_atomic_increment_uint32(ptr, amount) os_atomic_increment(ptr, amount)

#define os_atomic_increment_uint64(ptr, amount) os_atomic_increment(ptr, amount)

/* Returns the resulting value, ptr is pointer to target, amount is the
amount to decrement. */

#if defined(HAVE_GCC_SYNC_BUILTINS)
#define os_atomic_decrement(ptr, amount) __sync_sub_and_fetch(ptr, amount)
#else
#define os_atomic_decrement(ptr, amount) \
  __atomic_sub_fetch(ptr, amount, __ATOMIC_SEQ_CST)
#endif /* HAVE_GCC_SYNC_BUILTINS */

#define os_atomic_decrement_lint(ptr, amount) os_atomic_decrement(ptr, amount)

#define os_atomic_decrement_ulint(ptr, amount) os_atomic_decrement(ptr, amount)

#define os_atomic_decrement_uint32(ptr, amount) os_atomic_decrement(ptr, amount)

#define os_atomic_decrement_uint64(ptr, amount) os_atomic_decrement(ptr, amount)

/** Same functions with no return value. These may have optimized
implementations on some architectures. */

#if defined(__aarch64__) && defined(HAVE_ARM64_LSE_ATOMICS)
#define os_atomic_decrement_nr(ptr, amount) os_atomic_increment_nr(ptr, -amount)
#else
#define os_atomic_decrement_nr(ptr, amount) os_atomic_decrement(ptr, amount)
#endif

#define os_atomic_decrement_lint_nr(ptr, amount) \
  os_atomic_decrement_nr(ptr, amount)

#define os_atomic_decrement_ulint_nr(ptr, amount) \
  os_atomic_decrement_nr(ptr, amount)

#define os_atomic_decrement_uint32_nr(ptr, amount) \
  os_atomic_decrement_nr(ptr, amount)

#define os_atomic_decrement_uint64_nr(ptr, amount) \
  os_atomic_decrement_nr(ptr, amount)

/* Non-atomic version of the functions with no return value. */

#if defined(__aarch64__) && defined(HAVE_ARM64_LSE_ATOMICS)
/* Atomic increment without fetching the original value is faster than nonatomic
one with fetching. */
#define os_nonatomic_decrement_nr(ptr, amount) \
  os_atomic_decrement_nr(ptr, amount)
#else
#define os_nonatomic_decrement_nr(ptr, amount) (*(ptr) -= (amount))
#endif

#define os_nonatomic_decrement_lint_nr(ptr, amount) \
  os_nonatomic_decrement_nr(ptr, amount)

#define os_nonatomic_decrement_ulint_nr(ptr, amount) \
  os_nonatomic_decrement_nr(ptr, amount)

#define os_nonatomic_decrement_uint32_nr(ptr, amount) \
  os_nonatomic_decrement_nr(ptr, amount)

#define os_nonatomic_decrement_uint64_nr(ptr, amount) \
  os_nonatomic_decrement_nr(ptr, amount)

#endif

#define os_atomic_inc_ulint(m, v, d) os_atomic_increment_ulint(v, d)
#define os_atomic_dec_ulint(m, v, d) os_atomic_decrement_ulint(v, d)
#define TAS(l, n) os_atomic_test_and_set((l), (n))
#define CAS(l, o, n) os_atomic_val_compare_and_swap((l), (o), (n))

/** barrier definitions for memory ordering */
#ifdef HAVE_IB_GCC_ATOMIC_THREAD_FENCE
#define HAVE_MEMORY_BARRIER
#define os_rmb __atomic_thread_fence(__ATOMIC_ACQUIRE)
#define os_wmb __atomic_thread_fence(__ATOMIC_RELEASE)
#define IB_MEMORY_BARRIER_STARTUP_MSG \
  "GCC builtin __atomic_thread_fence() is used for memory barrier"

#elif defined(HAVE_IB_GCC_SYNC_SYNCHRONISE)
#define HAVE_MEMORY_BARRIER
#define os_rmb __sync_synchronize()
#define os_wmb __sync_synchronize()
#define IB_MEMORY_BARRIER_STARTUP_MSG \
  "GCC builtin __sync_synchronize() is used for memory barrier"

#elif defined(HAVE_IB_MACHINE_BARRIER_SOLARIS)
#define HAVE_MEMORY_BARRIER
#include <mbarrier.h>
#define os_rmb __machine_r_barrier()
#define os_wmb __machine_w_barrier()
#define IB_MEMORY_BARRIER_STARTUP_MSG \
  "Solaris memory ordering functions are used for memory barrier"

#elif defined(HAVE_WINDOWS_MM_FENCE) && defined(_WIN64)
#define HAVE_MEMORY_BARRIER
#include <mmintrin.h>
#define os_rmb _mm_lfence()
#define os_wmb _mm_sfence()
#define IB_MEMORY_BARRIER_STARTUP_MSG \
  "_mm_lfence() and _mm_sfence() are used for memory barrier"

#else
#define os_rmb
#define os_wmb
#define IB_MEMORY_BARRIER_STARTUP_MSG "Memory barrier is not used"
#endif

#include "os0atomic.ic"

#endif /* !os0atomic_h */
