/* Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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

#ifndef SQL_JOIN_OPTIMIZER_BIT_UTILS_H
#define SQL_JOIN_OPTIMIZER_BIT_UTILS_H 1

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "my_compiler.h"
#ifdef _MSC_VER
#include <intrin.h>
#pragma intrinsic(_BitScanForward64)
#pragma intrinsic(_BitScanReverse64)
#endif

// Wraps iteration over interesting states (based on the given policy) over a
// single uint64_t into an STL-style adapter.
template <class Policy>
class BitIteratorAdaptor {
 public:
  class iterator {
   private:
    uint64_t m_state;

   public:
    explicit iterator(uint64_t state) : m_state(state) {}
    bool operator==(const iterator &other) const {
      return m_state == other.m_state;
    }
    bool operator!=(const iterator &other) const {
      return m_state != other.m_state;
    }
    size_t operator*() const { return Policy::NextValue(m_state); }
    iterator &operator++() {
      m_state = Policy::AdvanceState(m_state);
      return *this;
    }
  };

  explicit BitIteratorAdaptor(uint64_t state) : m_initial_state(state) {}

  iterator begin() const { return iterator{m_initial_state}; }
  iterator end() const { return iterator{0}; }

 private:
  const uint64_t m_initial_state;
};

inline size_t FindLowestBitSet(uint64_t x) {
  assert(x != 0);
#ifdef _MSC_VER
  unsigned long idx;
  _BitScanForward64(&idx, x);
  return idx;
#elif defined(__GNUC__) && defined(__x86_64__)
  // Using this instead of ffsll() (which maps to the same instruction,
  // but has an extra zero test and returns an int value) helps
  // a whopping 10% on some of the microbenchmarks! (GCC 9.2, Skylake.)
  // Evidently, the test for zero is rewritten into a conditional move,
  // which turns out to be add a lot of latency into these hot loops.
  size_t idx;
  asm("bsfq %1,%q0" : "=r"(idx) : "rm"(x));
  return idx;
#else
  // The cast to unsigned at least gets rid of the sign extension.
  return static_cast<unsigned>(ffsll(x)) - 1u;
#endif
}

// A policy for BitIteratorAdaptor that gives out the index of each set bit in
// the value, ascending.
class CountBitsAscending {
 public:
  static size_t NextValue(uint64_t state) {
    // Find the lowest set bit.
    return FindLowestBitSet(state);
  }

  static uint64_t AdvanceState(uint64_t state) {
    // Clear the lowest set bit.
    assert(state != 0);
    return state & (state - 1);
  }
};

// Same as CountBitsAscending, just descending.
class CountBitsDescending {
 public:
  static size_t NextValue(uint64_t state) {
    // Find the lowest set bit.
    assert(state != 0);
#ifdef _MSC_VER
    unsigned long idx;
    _BitScanReverse64(&idx, state);
    return idx;
#else
    return __builtin_clzll(state) ^ 63u;
#endif
  }

  static uint64_t AdvanceState(uint64_t state) {
    // Clear the highest set bit. (This is fewer operations
    // then the standard bit-fiddling trick, especially given
    // that NextValue() is probably already computed.)
    return state & ~(uint64_t{1} << NextValue(state));
  }
};

inline BitIteratorAdaptor<CountBitsAscending> BitsSetIn(uint64_t state) {
  return BitIteratorAdaptor<CountBitsAscending>{state};
}
inline BitIteratorAdaptor<CountBitsDescending> BitsSetInDescending(
    uint64_t state) {
  return BitIteratorAdaptor<CountBitsDescending>{state};
}

// An iterator (for range-based for loops) that returns all non-zero subsets of
// a given set. This includes the set itself.
//
// In the database literature, this algorithm is often attributed to
// a 1995 paper of Vance and Maier, but it is known to be older than
// that. In particular, here is a 1994 reference from Marcel van Kervinck:
//
//   https://groups.google.com/forum/#!msg/rec.games.chess/KnJvBnhgDKU/yCi5yBx18PQJ
class NonzeroSubsetsOf {
 public:
  class iterator {
   private:
    uint64_t m_state;
    uint64_t m_set;

   public:
    iterator(uint64_t state, uint64_t set) : m_state(state), m_set(set) {}
    bool operator==(const iterator &other) const {
      assert(m_set == other.m_set);
      return m_state == other.m_state;
    }
    bool operator!=(const iterator &other) const {
      assert(m_set == other.m_set);
      return m_state != other.m_state;
    }
    uint64_t operator*() const { return m_state; }
    iterator &operator++() {
      m_state = (m_state - m_set) & m_set;
      return *this;
    }
  };

  explicit NonzeroSubsetsOf(uint64_t set) : m_set(set) {}

  MY_COMPILER_DIAGNOSTIC_PUSH()
  // Suppress warning C4146 unary minus operator applied to unsigned type,
  // result still unsigned
  MY_COMPILER_MSVC_DIAGNOSTIC_IGNORE(4146)
  iterator begin() const { return {(-m_set) & m_set, m_set}; }
  MY_COMPILER_DIAGNOSTIC_POP()
  iterator end() const { return {0, m_set}; }

 private:
  const uint64_t m_set;
};

// Returns a bitmap representing a single table.
inline uint64_t TableBitmap(unsigned x) { return uint64_t{1} << x; }

// Returns a bitmap representing the semi-open interval [start, end).
MY_COMPILER_DIAGNOSTIC_PUSH()
// Suppress warning C4146 unary minus operator applied to unsigned type,
// result still unsigned
MY_COMPILER_MSVC_DIAGNOSTIC_IGNORE(4146)
inline uint64_t BitsBetween(unsigned start, unsigned end) {
  assert(end >= start);
  assert(end <= 64);
  if (end == 64) {
    if (start == 64) {
      return 0;
    } else {
      return -(uint64_t{1} << start);
    }
  } else {
    return (uint64_t{1} << end) - (uint64_t{1} << start);
  }
}
MY_COMPILER_DIAGNOSTIC_POP()

// The same, just with a different name for clarity.
inline uint64_t TablesBetween(unsigned start, unsigned end) {
  return BitsBetween(start, end);
}

// Isolates the LSB of x. Ie., if x = 0b110001010, returns 0b000000010.
// Zero input gives zero output.
MY_COMPILER_DIAGNOSTIC_PUSH()
// Suppress warning C4146 unary minus operator applied to unsigned type,
// result still unsigned
MY_COMPILER_MSVC_DIAGNOSTIC_IGNORE(4146)
inline uint64_t IsolateLowestBit(uint64_t x) { return x & (-x); }
MY_COMPILER_DIAGNOSTIC_POP()

// Returns whether X is a subset of Y.
inline bool IsSubset(uint64_t x, uint64_t y) { return (x & y) == x; }

/// Returns whether X is a proper subset of Y.
inline bool IsProperSubset(uint64_t x, uint64_t y) {
  return IsSubset(x, y) && x != y;
}

// Returns whether X and Y overlap. Symmetric.
inline bool Overlaps(uint64_t x, uint64_t y) { return (x & y) != 0; }

// Returns whether X has more than one bit set.
inline bool AreMultipleBitsSet(uint64_t x) { return (x & (x - 1)) != 0; }

// Returns whether X has exactly one bit set.
inline bool IsSingleBitSet(uint64_t x) {
  return x != 0 && !AreMultipleBitsSet(x);
}

// Returns whether the given bit is set in X.
inline bool IsBitSet(int bit_num, uint64_t x) {
  return Overlaps(x, uint64_t{1} << bit_num);
}

// Fairly slow implementation of population count (number of bits set).
inline int PopulationCount(uint64_t x) {
  int count = 0;
  while (x != 0) {
    x &= x - 1;
    ++count;
  }
  return count;
}

#endif  // SQL_JOIN_OPTIMIZER_BIT_UTILS_H
