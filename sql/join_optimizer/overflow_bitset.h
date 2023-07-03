/* Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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

#ifndef SQL_JOIN_OPTIMIZER_OVERFLOW_BITSET_H
#define SQL_JOIN_OPTIMIZER_OVERFLOW_BITSET_H

/**
  @file

  OverflowBitset is a fixed-size (once allocated) bitmap that is optimized for
  the common case of few elements, yet can support an arbitrary number.
  For 63 bits or fewer, it fits into a simple 64-bit machine word; for more,
  it instead “overflows” to a pointer to externally-allocated storage
  (typically on a MEM_ROOT). In other words, one loses only 1 bit for the common
  (small) case. For small (“inline”) bit sets, most operations are simple
  bit-twiddling operations, adding only a small and easily-predicatable test to
  each of them.

  This is possible because a pointer to external storage of 64-bit values
  will (must) be aligned to 8 bytes, so the lowest bit of the address
  cannot be 1. We can use this to distinguish between inline and non-inline
  sets.

  There are two classes: OverflowBitset is an immutable (const) bitset with
  value semantics; it can be freely assigned, copied, stored in AccessPath
  (which also has value semantics), etc. with no worries, but not modified.
  (The storage is never freed; it is presumed to live on a MEM_ROOT.)
  MutableOverflowBitset can be modified, but it is move-only; this avoids
  the problem where one takes a copy of a (non-inline) bit set and then
  modify one of them, not expecting that modification to also affect
  the other one. (Ie., it avoids the design mistake in String, where the
  copy constructor unexpectedly can become a shallow copy, but not always.)
  MutableOverflowBitset can be converted to OverflowBitset by means of a
  move, at which point it is effectively frozen and cannot be changed further.

  For simplicity, most operations over OverflowBitset require the two
  bit sets to be of the same size. The exception is that an all-zero inline
  bit set can be tested against others in Overlaps(); this is useful so that we
  don't need to initialize bit sets in AccessPath that never have any filters
  set (the default constructor makes them inline and all-zero).
 */

#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <array>
#include <tuple>

#include "my_alloc.h"
#include "sql/join_optimizer/bit_utils.h"

class MutableOverflowBitset;

class OverflowBitset {
 public:
  // Default is to zero-initialize.
  OverflowBitset() : m_bits(1) {}

  explicit OverflowBitset(uint32_t bits) : m_bits((uintptr_t{bits} << 1) | 1) {
    // Check that we didn't overflow on 32-bit platforms.
    assert((m_bits >> 1) == bits);
  }

  // Reset the entire bitset to an inline all-zero bitset.
  // This is distinct from ClearBits(), which only clears a given number
  // of bits, and does not change the capacity.
  void Clear() { m_bits = 1; }

  // Value semantics, so:
  OverflowBitset(const OverflowBitset &) = default;
  OverflowBitset(OverflowBitset &&) = default;
  OverflowBitset &operator=(const OverflowBitset &) = default;
  OverflowBitset &operator=(OverflowBitset &&) = default;

  // Can move-convert MutableOverflowBitset into OverflowBitset.
  inline OverflowBitset(MutableOverflowBitset &&);
  inline OverflowBitset &operator=(MutableOverflowBitset &&);

  bool is_inline() const { return m_bits & 1; }
  bool empty() { return m_bits == 1; }

  size_t capacity() const {
    if (is_inline()) {
      return kInlineBits;
    } else {
      return m_ext->m_num_blocks * sizeof(uint64_t) * CHAR_BIT;
    }
  }

  inline MutableOverflowBitset Clone(MEM_ROOT *mem_root) const;

  bool IsContainedIn(const MEM_ROOT *mem_root) const {
    return !is_inline() && mem_root->Contains(m_ext);
  }

  // NOTE: These could also be made to take in MutableOverflowBitset(),
  // simply by templating them (due to the private inheritance).
  static inline MutableOverflowBitset Or(MEM_ROOT *mem_root, OverflowBitset a,
                                         OverflowBitset b);
  static inline MutableOverflowBitset And(MEM_ROOT *mem_root, OverflowBitset a,
                                          OverflowBitset b);
  static inline MutableOverflowBitset Xor(MEM_ROOT *mem_root, OverflowBitset a,
                                          OverflowBitset b);

 protected:
  struct Ext {
    size_t m_num_blocks;
    uint64_t m_bits[1];
  };
  static_assert(alignof(Ext) % 2 == 0, "The lowest bit must be zero.");

  union {
    uint64_t m_bits;  // Lowest bit must be 1.
    Ext *m_ext;
  };
  static constexpr int kInlineBits = sizeof(m_bits) * CHAR_BIT - 1;

  void InitOverflow(MEM_ROOT *mem_root, size_t capacity);
  static MutableOverflowBitset OrOverflow(MEM_ROOT *mem_root, OverflowBitset a,
                                          OverflowBitset b);
  static MutableOverflowBitset AndOverflow(MEM_ROOT *mem_root, OverflowBitset a,
                                           OverflowBitset b);
  static MutableOverflowBitset XorOverflow(MEM_ROOT *mem_root, OverflowBitset a,
                                           OverflowBitset b);

  friend bool Overlaps(OverflowBitset a, OverflowBitset b);
  friend bool OverlapsOverflow(OverflowBitset a, OverflowBitset b);
  friend bool IsSubset(OverflowBitset a, OverflowBitset b);
  friend bool IsSubsetOverflow(OverflowBitset a, OverflowBitset b);
  friend bool IsBitSet(int bit_num, OverflowBitset x);
  friend bool IsBitSetOverflow(int bit_num, OverflowBitset x);
  friend bool IsEmpty(OverflowBitset x);
  friend int PopulationCount(OverflowBitset x);
  friend int PopulationCountOverflow(OverflowBitset x);
  template <size_t N, class Combine>
  friend class OverflowBitsetBitsIn;
  friend class MutableOverflowBitset;
};
static_assert(
    sizeof(OverflowBitset) <= sizeof(uint64_t),
    "OverflowBitset is intended to be as compact as a regular 64-bit set.");

// Private inheritance, so that the only way of converting to OverflowBitset
// for external callers is by a move-convert.
class MutableOverflowBitset : private OverflowBitset {
 public:
  // NOTE: Will round up the given capacity to either 31/63 (for anything
  // smaller than 32/64), or to the next multiple of 64 (for anything else).
  MutableOverflowBitset(MEM_ROOT *mem_root, size_t capacity) {
    if (capacity <= kInlineBits) {
      m_bits = 1;
    } else {
      InitOverflow(mem_root, capacity);
    }
  }

  // Move-only, so that we don't inadvertently modify aliases.
  MutableOverflowBitset(const MutableOverflowBitset &) = delete;
  MutableOverflowBitset &operator=(const MutableOverflowBitset &) = delete;
  MutableOverflowBitset(MutableOverflowBitset &&other) {
    m_bits = other.m_bits;
    other.m_bits = 1;
  }
  MutableOverflowBitset &operator=(MutableOverflowBitset &&other) {
    m_bits = other.m_bits;
    other.m_bits = 1;
    return *this;
  }

  void SetBit(int bit_num) {
    assert(bit_num >= 0);
    assert(static_cast<size_t>(bit_num) < capacity());
    const unsigned bn = bit_num;  // To avoid sign extension taking time.
    if (is_inline()) {
      assert(bit_num < 63);
      m_bits |= uint64_t{1} << (bn + 1);
    } else {
      m_ext->m_bits[bn / 64] |= uint64_t{1} << (bn % 64);
    }
  }

  void ClearBits(int begin_bit_num, int end_bit_num) {
    assert(begin_bit_num >= 0);
    assert(end_bit_num >= 0);
    assert(begin_bit_num <= end_bit_num);
    assert(static_cast<size_t>(begin_bit_num) <= capacity());
    assert(static_cast<size_t>(end_bit_num) <= capacity());
    if (is_inline()) {
      m_bits &= ~BitsBetween(begin_bit_num + 1, end_bit_num + 1);
    } else {
      ClearBitsOverflow(begin_bit_num, end_bit_num);
    }
  }

  inline void ClearBit(int bit_num) {
    // TODO: Consider a more specialized version here if it starts
    // showing up in the profiles.
    ClearBits(bit_num, bit_num + 1);
  }

  inline MutableOverflowBitset Clone(MEM_ROOT *mem_root) const {
    return OverflowBitset::Clone(mem_root);
  }

 private:
  friend bool IsBitSet(int bit_num, const MutableOverflowBitset &x);
  friend bool Overlaps(OverflowBitset a, const MutableOverflowBitset &b);
  friend bool Overlaps(const MutableOverflowBitset &a,
                       const MutableOverflowBitset &b);
  friend bool Overlaps(const MutableOverflowBitset &a, OverflowBitset b);
  friend bool IsSubset(OverflowBitset a, const MutableOverflowBitset &b);
  friend bool IsSubset(const MutableOverflowBitset &a,
                       const MutableOverflowBitset &b);
  friend bool IsSubset(const MutableOverflowBitset &a, OverflowBitset b);
  friend bool IsEmpty(const MutableOverflowBitset &x);
  friend int PopulationCount(const MutableOverflowBitset &x);
  void SetBitOverflow(int bit_num);
  void ClearBitsOverflow(int begin_bit_num, int end_bit_num);

  friend class OverflowBitset;
};

inline OverflowBitset::OverflowBitset(MutableOverflowBitset &&other) {
  m_bits = other.m_bits;
  other.m_bits = 1;
}

inline OverflowBitset &OverflowBitset::operator=(
    MutableOverflowBitset &&other) {
  m_bits = other.m_bits;
  other.m_bits = 1;
  return *this;
}

inline MutableOverflowBitset OverflowBitset::Clone(MEM_ROOT *mem_root) const {
  MutableOverflowBitset ret(mem_root, capacity());
  if (is_inline()) {
    ret.m_bits = m_bits;
  } else {
    memcpy(ret.m_ext, m_ext,
           sizeof(m_ext->m_num_blocks) +
               m_ext->m_num_blocks * sizeof(m_ext->m_bits));
  }
  return ret;
}

inline MutableOverflowBitset OverflowBitset::Or(MEM_ROOT *mem_root,
                                                OverflowBitset a,
                                                OverflowBitset b) {
  assert(a.is_inline() == b.is_inline());
  assert(a.capacity() == b.capacity());
  if (a.is_inline()) {
    MutableOverflowBitset ret{mem_root, 63};
    ret.m_bits = a.m_bits | b.m_bits;
    return ret;
  } else {
    return OrOverflow(mem_root, a, b);
  }
}

inline MutableOverflowBitset OverflowBitset::And(MEM_ROOT *mem_root,
                                                 OverflowBitset a,
                                                 OverflowBitset b) {
  assert(a.is_inline() == b.is_inline());
  assert(a.capacity() == b.capacity());
  if (a.is_inline()) {
    MutableOverflowBitset ret{mem_root, 63};
    ret.m_bits = a.m_bits & b.m_bits;
    return ret;
  } else {
    return AndOverflow(mem_root, a, b);
  }
}

inline MutableOverflowBitset OverflowBitset::Xor(MEM_ROOT *mem_root,
                                                 OverflowBitset a,
                                                 OverflowBitset b) {
  assert(a.is_inline() == b.is_inline());
  assert(a.capacity() == b.capacity());
  if (a.is_inline()) {
    MutableOverflowBitset ret{mem_root, 63};
    ret.m_bits = (a.m_bits ^ b.m_bits) | 1;
    return ret;
  } else {
    return XorOverflow(mem_root, a, b);
  }
}

// Definitions overloading utility functions in bit_utils.h, making it generally
// possible to use OverflowBitset as we use regular uint64_t bitsets
// (e.g. NodeMap).
//
// Since one cannot easily combine non-inline OverflowBitsets without allocating
// memory, the BitsSetIn() overload supports combining state as-we-go.
// For instance, where you would normally write (for uint64_t)
//
//   for (int bit_idx : BitsSetIn(x & y))
//
// you would use this variation for OverflowBitsets:
//
//   for (int bit_idx : BitsSetInBoth(x, y))
//
// Under the hood, BitsSetInBoth() calls a Combine functor that ANDs the two
// uint64_t bitwise (for inline bitsets only once, but for overflow bitsets
// multiple times, on-demand as we iterate), which can potentially save
// on a lot of bitscan operations and loop iterations versus trying to test
// one-by-one. This can be extended to any number of arguments.
//
// Combine::operator() must be const, Combine must be movable (but can have
// state).

template <size_t N, class Combine>
class OverflowBitsetBitsIn {
 public:
  class iterator {
   private:
    const Combine *m_combine;
    uint64_t m_state;

    // m_next holds, for each bitset array, the pointer to the next
    // uint64_t to be processed/read (once m_state is zero, ie.,
    // there are no more bits in the current state). When m_next[0] == m_end,
    // iteration is over. For inline bitsets, m_next[0] == m_end == nullptr,
    // so once the first 64-bit group is processed, we are done.
    // (We assume all arrays have the same length, so we only need one
    // end pointer.)
    std::array<const uint64_t *, N> m_next;
    const uint64_t *const m_end;
    int m_base;

   public:
    // For inline bitsets.
    iterator(uint64_t state, const Combine *combine)
        : m_combine(combine), m_state(state), m_end(nullptr), m_base(0) {
      m_next[0] = nullptr;
    }

    // For overflow bitsets.
    iterator(const std::array<const uint64_t *, N> begin, const uint64_t *end,
             const Combine *combine)
        : m_combine(combine),
          m_state(0),
          m_next(begin),
          m_end(end),
          m_base(-64) {
      while (m_state == 0 && m_next[0] != m_end) {
        m_state = ReadAndCombine(m_next, m_combine);
        for (size_t i = 0; i < N; ++i) {
          ++m_next[i];
        }
        m_base += 64;
      }
    }

    bool operator==(const iterator &other) const {
      assert(m_end == other.m_end);
      return m_state == other.m_state && m_next[0] == other.m_next[0];
    }
    bool operator!=(const iterator &other) const {
      assert(m_end == other.m_end);
      return m_state != other.m_state || m_next[0] != other.m_next[0];
    }
    size_t operator*() const { return FindLowestBitSet(m_state) + m_base; }
    iterator &operator++() {
      // Clear the lowest set bit.
      assert(m_state != 0);
      m_state = m_state & (m_state - 1);

      while (m_state == 0 && m_next[0] != m_end) {
        m_state = ReadAndCombine(m_next, m_combine);
        for (size_t i = 0; i < N; ++i) {
          ++m_next[i];
        }
        m_base += 64;
      }
      return *this;
    }
  };

  OverflowBitsetBitsIn(std::array<OverflowBitset, N> bitsets, Combine combine)
      : m_bitsets(bitsets), m_combine(std::move(combine)) {}

  iterator begin() const {
    if (m_bitsets[0].is_inline()) {
      std::array<uint64_t, N> bits;
      for (size_t i = 0; i < N; ++i) {
        assert(m_bitsets[i].is_inline());
        bits[i] = m_bitsets[i].m_bits;
      }
      uint64_t state = std::apply(m_combine, bits);
      return iterator{state >> 1, &m_combine};
    } else {
      std::array<const uint64_t *, N> ptrs;
      for (size_t i = 0; i < N; ++i) {
        assert(!m_bitsets[i].is_inline());
        assert(m_bitsets[i].capacity() == m_bitsets[0].capacity());
        ptrs[i] = m_bitsets[i].m_ext->m_bits;
      }
      const uint64_t *end =
          m_bitsets[0].m_ext->m_bits + m_bitsets[0].m_ext->m_num_blocks;
      return iterator{ptrs, end, &m_combine};
    }
  }
  iterator end() const {
    if (m_bitsets[0].is_inline()) {
      return iterator{0, &m_combine};
    } else {
      std::array<const uint64_t *, N> ptrs;
      for (size_t i = 0; i < N; ++i) {
        assert(m_bitsets[i].is_inline() == m_bitsets[0].is_inline());
        assert(m_bitsets[i].capacity() == m_bitsets[0].capacity());
        ptrs[i] = m_bitsets[i].m_ext->m_bits + m_bitsets[i].m_ext->m_num_blocks;
      }
      return iterator{ptrs, ptrs[0], &m_combine};
    }
  }

 private:
  static inline uint64_t ReadAndCombine(
      const std::array<const uint64_t *, N> &ptrs, const Combine *combine) {
    std::array<uint64_t, N> bits;
    for (size_t i = 0; i < N; ++i) {
      bits[i] = *ptrs[i];
    }
    return std::apply(*combine, bits);
  }

  const std::array<OverflowBitset, N> m_bitsets;
  const Combine m_combine;
};

struct IdentityCombine {
  uint64_t operator()(uint64_t x) const { return x; }
};
inline auto BitsSetIn(OverflowBitset bitset) {
  return OverflowBitsetBitsIn<1, IdentityCombine>{{bitset}, IdentityCombine()};
}

struct AndCombine {
  uint64_t operator()(uint64_t x, uint64_t y) const { return x & y; }
};
inline auto BitsSetInBoth(OverflowBitset bitset_a, OverflowBitset bitset_b) {
  return OverflowBitsetBitsIn<2, AndCombine>{{bitset_a, bitset_b},
                                             AndCombine()};
}

bool OverlapsOverflow(OverflowBitset a, OverflowBitset b);

inline bool Overlaps(OverflowBitset a, OverflowBitset b) {
  if (a.m_bits == 1 || b.m_bits == 1) {
    // Empty and inline, so nothing overlaps.
    // This is a special case that does not require the two
    // sets to be of the same size (see file comment).
    return false;
  }
  assert(a.is_inline() == b.is_inline());
  assert(a.capacity() == b.capacity());
  if (a.is_inline()) {
    return (a.m_bits & b.m_bits) != 1;
  } else {
    return OverlapsOverflow(a, b);
  }
}

inline bool Overlaps(OverflowBitset a, const MutableOverflowBitset &b) {
  return Overlaps(a, static_cast<const OverflowBitset &>(b));
}

inline bool Overlaps(const MutableOverflowBitset &a,
                     const MutableOverflowBitset &b) {
  return Overlaps(static_cast<const OverflowBitset &>(a),
                  static_cast<const OverflowBitset &>(b));
}

inline bool Overlaps(const MutableOverflowBitset &a, OverflowBitset b) {
  return Overlaps(static_cast<const OverflowBitset &>(a), b);
}

inline bool IsSubset(OverflowBitset a, OverflowBitset b) {
  assert(a.is_inline() == b.is_inline());
  assert(a.capacity() == b.capacity());
  if (a.is_inline()) {
    return IsSubset(a.m_bits, b.m_bits);
  } else {
    return IsSubsetOverflow(a, b);
  }
}

inline bool IsBitSet(int bit_num, OverflowBitset x) {
  assert(bit_num >= 0);
  assert(static_cast<size_t>(bit_num) < x.capacity());
  const unsigned bn = bit_num;  // To avoid sign extension taking time.
  if (x.is_inline()) {
    return Overlaps(x.m_bits, uint64_t{1} << (bn + 1));
  } else {
    return Overlaps(x.m_ext->m_bits[bn / 64], uint64_t{1} << (bn % 64));
  }
}

inline bool IsBitSet(int bit_num, const MutableOverflowBitset &x) {
  return IsBitSet(bit_num, static_cast<const OverflowBitset &>(x));
}

inline bool IsSubset(OverflowBitset a, const MutableOverflowBitset &b) {
  return IsSubset(a, static_cast<const OverflowBitset &>(b));
}

inline bool IsSubset(const MutableOverflowBitset &a,
                     const MutableOverflowBitset &b) {
  return IsSubset(static_cast<const OverflowBitset &>(a),
                  static_cast<const OverflowBitset &>(b));
}

inline bool IsSubset(const MutableOverflowBitset &a, OverflowBitset b) {
  return IsSubset(static_cast<const OverflowBitset &>(a), b);
}

// This is mostly used to guard a few asserts, so it's better that it's
// completely visible, so that the compiler can remove it totally
// in optimized mode.
inline bool IsEmpty(OverflowBitset x) {
  if (x.is_inline()) {
    return x.m_bits == 1;
  } else {
    for (unsigned i = 0; i < x.m_ext->m_num_blocks; ++i) {
      if (x.m_ext->m_bits[i] != 0) {
        return false;
      }
    }
    return true;
  }
}

inline bool IsEmpty(const MutableOverflowBitset &x) {
  return IsEmpty(static_cast<const OverflowBitset &>(x));
}

inline int PopulationCount(OverflowBitset x) {
  if (x.is_inline()) {
    return PopulationCount(x.m_bits) - 1;
  } else {
    return PopulationCountOverflow(x);
  }
}

/// Find the nuber of bits set in 'x'.
inline int PopulationCount(const MutableOverflowBitset &x) {
  return PopulationCount(static_cast<const OverflowBitset &>(x));
}

#endif  // SQL_JOIN_OPTIMIZER_OVERFLOW_BITSET_H
