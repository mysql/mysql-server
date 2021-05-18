/* Copyright (c) 2021, Oracle and/or its affiliates.

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

#include "sql/join_optimizer/bit_utils.h"

struct MEM_ROOT;
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
    uintptr_t m_bits;  // Lowest bit must be 1.
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
  friend bool IsBitSet(int bit_num, OverflowBitset x);
  friend bool IsBitSetOverflow(int bit_num, OverflowBitset x);
  friend bool IsEmpty(OverflowBitset x);
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
    if (is_inline()) {
      assert(bit_num < 63);
      m_bits |= uint64_t{1} << (bit_num + 1);
    } else {
      SetBitOverflow(bit_num);
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

  inline MutableOverflowBitset Clone(MEM_ROOT *mem_root) const {
    return OverflowBitset::Clone(mem_root);
  }

 private:
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

// Definitions overloading utility functions in bit_utils.h,
// making it generally possible to use OverflowBitset as we use
// regular uint64_t bitsets (e.g. NodeMap).

class OverflowBitsetBitsIn {
 public:
  class iterator {
   private:
    uint64_t m_state;
    const uint64_t *m_next;
    const uint64_t *const m_end;
    int m_base;

   public:
    // For inline bitsets.
    explicit iterator(uint64_t state)
        : m_state(state), m_next(nullptr), m_end(nullptr), m_base(0) {}

    // For overflow bitsets.
    iterator(const uint64_t *begin, const uint64_t *end)
        : m_state(0), m_next(begin), m_end(end), m_base(-64) {
      while (m_state == 0 && m_next != m_end) {
        m_state = *m_next++;
        m_base += 64;
      }
    }

    bool operator==(const iterator &other) const {
      assert(m_end == other.m_end);
      return m_state == other.m_state && m_next == other.m_next;
    }
    bool operator!=(const iterator &other) const {
      assert(m_end == other.m_end);
      return m_state != other.m_state || m_next != other.m_next;
    }
    size_t operator*() const { return FindLowestBitSet(m_state) + m_base; }
    iterator &operator++() {
      // Clear the lowest set bit.
      assert(m_state != 0);
      m_state = m_state & (m_state - 1);

      while (m_state == 0 && m_next != m_end) {
        m_state = *m_next++;
        m_base += 64;
      }
      return *this;
    }
  };

  explicit OverflowBitsetBitsIn(OverflowBitset bitset) : m_bitset(bitset) {}

  iterator begin() const {
    if (m_bitset.is_inline()) {
      return iterator{m_bitset.m_bits >> 1};
    } else {
      const uint64_t *end =
          m_bitset.m_ext->m_bits + m_bitset.m_ext->m_num_blocks;
      return iterator{m_bitset.m_ext->m_bits, end};
    }
  }
  iterator end() const {
    if (m_bitset.is_inline()) {
      return iterator{0};
    } else {
      const uint64_t *end =
          m_bitset.m_ext->m_bits + m_bitset.m_ext->m_num_blocks;
      return iterator{end, end};
    }
  }

 private:
  const OverflowBitset m_bitset;
};

inline OverflowBitsetBitsIn BitsSetIn(OverflowBitset bitset) {
  return OverflowBitsetBitsIn{bitset};
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

bool IsBitSetOverflow(int bit_num, OverflowBitset x);

inline bool IsBitSet(int bit_num, OverflowBitset x) {
  assert(bit_num >= 0);
  assert(static_cast<size_t>(bit_num) < x.capacity());
  if (x.is_inline()) {
    return Overlaps(x.m_bits, uint64_t{1} << (bit_num + 1));
  } else {
    return IsBitSetOverflow(bit_num, x);
  }
}

// This is used only to guard a few asserts, so it's better that it's
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

#endif  // SQL_JOIN_OPTIMIZER_OVERFLOW_BITSET_H
