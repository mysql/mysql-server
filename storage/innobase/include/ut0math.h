/*****************************************************************************

Copyright (c) 2021, 2024, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file include/ut0math.h
 Math functions.

 ***********************************************************************/

#ifndef ut0math_h
#define ut0math_h

#include <atomic>
#include <cstdint>
#include "ut0class_life_cycle.h"
#include "ut0dbg.h"
#include "ut0seq_lock.h"

namespace ut {

/** Calculates the 128bit result of multiplication of the two specified 64bit
integers. May use CPU native instructions for speed of standard uint64_t
multiplication.
@param[in] x First number to multiply.
@param[in] y Second number to multiply.
@param[out] hi A reference to 64bit integer that will store higher 64bits of the
result.
@return The lower 64bit of the result. */
[[nodiscard]] static inline uint64_t multiply_uint64(uint64_t x, uint64_t y,
                                                     uint64_t &hi);

/*Calculates the 64bit result of division of the specified 128bit integer by the
specified 64bit integer. The result must fit in 64bit or else the behavior is
undefined. Currently does not use native CPU instructions and can be quite slow.
@param[in] high High 64bits of the number to divide.
@param[in] low Low 64bits of the number to divide.
@param[in] div The number to divide by.
@return The lower 64bit of the result. */
[[nodiscard]] static inline uint64_t divide_128(uint64_t high, uint64_t low,
                                                uint64_t div);
class fast_modulo_t;

/** Looks for a prime number slightly greater than the given argument.
The prime is chosen so that it is not near any power of 2.
@param[in]	n  positive number > 100
@return prime */
[[nodiscard]] uint64_t find_prime(uint64_t n);

namespace detail {
/** Calculates the 128bit result of multiplication of the two specified 64bit
integers.
@param[in] x First number to multiply.
@param[in] y Second number to multiply.
@param[out] hi A reference to 64bit integer that will store higher 64bits of the
result.
@return The lower 64bit of the result. */
[[nodiscard]] constexpr uint64_t multiply_uint64_portable(uint64_t x,
                                                          uint64_t y,
                                                          uint64_t &hi) {
  uint32_t x_hi = static_cast<uint32_t>(x >> 32);
  uint32_t x_lo = static_cast<uint32_t>(x);
  uint32_t y_hi = static_cast<uint32_t>(y >> 32);
  uint32_t y_lo = static_cast<uint32_t>(y);

  uint64_t hi_lo = static_cast<uint64_t>(x_hi) * y_lo;

  uint64_t low = static_cast<uint64_t>(x_lo) * y_lo;
  /* This will not overflow, as (2^32 -1)^2 = 2^64 - 1 - 2 * 2^32, so there is
  still a place for two 32bit integers to be added. */
  uint64_t mid = (low >> 32) + static_cast<uint64_t>(x_lo) * y_hi +
                 static_cast<uint32_t>(hi_lo);
  hi = (mid >> 32) + static_cast<uint64_t>(x_hi) * y_hi + (hi_lo >> 32);
  return static_cast<uint32_t>(low) + (mid << 32);
}
}  // namespace detail

#if defined(_MSC_VER) && defined(_M_X64) && !defined(_M_ARM64EC)
/* MSVC x86 supports native uint64_t -> uint128_t multiplication */
#include <intrin.h>
#pragma intrinsic(_umul128)
[[nodiscard]] static inline uint64_t multiply_uint64(uint64_t x, uint64_t y,
                                                     uint64_t &hi) {
  return _umul128(x, y, &hi);
}
#elif defined(__SIZEOF_INT128__)
/* Compiler supports 128-bit values (GCC/Clang) */

[[nodiscard]] static inline uint64_t multiply_uint64(uint64_t x, uint64_t y,
                                                     uint64_t &hi) {
  unsigned __int128 res = (unsigned __int128)x * y;
  hi = static_cast<uint64_t>(res >> 64);
  return static_cast<uint64_t>(res);
}
#else
[[nodiscard]] static inline uint64_t multiply_uint64(uint64_t x, uint64_t y,
                                                     uint64_t &hi) {
  return detail::multiply_uint64_portable(x, y, hi);
}
#endif

[[nodiscard]] static inline uint64_t divide_128(uint64_t high, uint64_t low,
                                                uint64_t div) {
  uint64_t res = 0;
  for (auto current_bit = 63; current_bit >= 0; current_bit--) {
    const auto div_hi = current_bit ? (div >> (64 - current_bit)) : 0;
    const auto div_lo = div << current_bit;
    if (div_hi < high || (div_hi == high && div_lo <= low)) {
      high -= div_hi;
      if (low < div_lo) {
        high--;
      }
      low -= div_lo;
      res += 1ULL << current_bit;
    }
  }
  return res;
}

/** Allows to execute x % mod for a specified mod in a fast way, without using a
slow operation of division. The additional cost is hidden in constructor to
preprocess the mod constant. */
class fast_modulo_t {
  /* Idea behind this implementation is following: (division sign in all
  equations below is to be treated as mathematical division on reals)

      x  % mod =  x - floor(x/mod)*mod

  and...

      x / mod  =  x * 1/mod =  (x *  (BIG/mod)) /BIG

  and..

      floor(x/mod) =  x / mod  - epsilon, where 0<=epsilon<1

  Now, lets define:

      M = floor(BIG/mod)

  And take a look at the value of following expression:

      floor( x*M / BIG) * mod =

          floor(x * floor(BIG/mod) / BIG) * mod =
          floor(x * ((BIG/mod)-epsilon1) / BIG) * mod =
          ((x*((BIG/mod)-epsilon1)/BIG - epsilon2) * mod

  This sure looks ugly, but it has interesting properties:
    (1) is divisible by mod, which you can see, because it has a form (...)*
  mod
    (2) is smaller or equal to x, which you can see by setting epsilons to 0
    (3) assuming BIG>x, the expression is strictly larger than x - 2*mod,
  because it must be larger than the value for epsilons=1, which is:
          ((x*((BIG/mod)-1))/BIG - 1) * mod  =
             ((x*BIG/mod - x)/BIG -1) * mod =
             ((x/mod - x/BIG) - 1) * mod =
             (x - x/BIG*mod - mod)
    (4) we can compute it without using division at all, if BIG is 1<<k,
       as it simplifies to
       (( x * M ) >> k ) * mod

  So, assuming BIG>x, and is a power of two (say BIG=1<<64), we get an
  expression, which is divisible by mod, and if we subtract it from x, we get
  something in the range [0...,2mod). What is left is to compare against mod,
  and subtract it if it is higher.
  */

 public:
  fast_modulo_t() = default;
  explicit fast_modulo_t(uint64_t mod)
      : m_mod(mod), m_inv(precompute_inv(mod)) {}
  explicit fast_modulo_t(uint64_t mod, uint64_t inv) : m_mod(mod), m_inv(inv) {}

  /** Computes the value of x % mod. */
  uint64_t compute(uint64_t x) const {
    uint64_t hi;
    (void)multiply_uint64(x, m_inv, hi);

    const uint64_t guess = hi * m_mod;
    const uint64_t rest = x - guess;

    return rest - (m_mod <= rest) * m_mod;
  }

  /** Gets the precomputed value of inverse. */
  uint64_t get_inverse() const { return m_inv; }

  /** Gets the modulo value. */
  uint64_t get_mod() const { return m_mod; }

  /** Precomputes the inverse needed for fast modulo operations. */
  static uint64_t precompute_inv(uint64_t mod) {
    /* pedantic matter: for mod=1 -- you can remove it if you never plan to use
    it for 1. */
    if (mod == 1) {
      /* According to equations we want M to be 1<<64, but this overflows
      uint64_t, so, let's do the second best thing we can, which is 1<<64-1,
      this means that our `guess` will be  ((x<<64 - x) >> 64)*mod, which for
      x=0, is 0 (good), and for x>0 is (x-1)*mod = (x-1)*1 = x-1, and then
      rest=1, which is also good enough (<2*mod). */
      return ~uint64_t{0};
    } else {
      return divide_128(1, 0, mod);
    }
  }

 private:
  uint64_t m_mod{0};
  uint64_t m_inv{0};
};

/** A class that allows to atomically set new modulo value for fast modulo
computations. */
class mt_fast_modulo_t : private Non_copyable {
 public:
  mt_fast_modulo_t() : m_data{0ULL, 0ULL} {}
  explicit mt_fast_modulo_t(uint64_t mod)
      : m_data{mod, fast_modulo_t::precompute_inv(mod)} {}
  /* This class can be made copyable, but this requires additional constructors.
   */

  fast_modulo_t load() {
    return m_data.read([](const data_t &stored_data) {
      return fast_modulo_t{stored_data.m_mod.load(std::memory_order_relaxed),
                           stored_data.m_inv.load(std::memory_order_relaxed)};
    });
  }

  void store(uint64_t new_mod) {
    const fast_modulo_t new_fast_modulo{new_mod};
    const auto inv = new_fast_modulo.get_inverse();
    m_data.write([&](data_t &data) {
      data.m_mod.store(new_mod, std::memory_order_relaxed);
      data.m_inv.store(inv, std::memory_order_relaxed);
    });
  }

 private:
  struct data_t {
    std::atomic<uint64_t> m_mod;
    std::atomic<uint64_t> m_inv;
  };

  Seq_lock<data_t> m_data;
};

}  // namespace ut

static inline uint64_t operator%(uint64_t x, const ut::fast_modulo_t &fm) {
  return fm.compute(x);
}

#endif
