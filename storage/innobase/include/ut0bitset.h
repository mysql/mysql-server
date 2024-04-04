/*****************************************************************************

Copyright (c) 1994, 2024, Oracle and/or its affiliates.

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

/** @file include/ut0bitset.h
 Utilities for bitset operations

 Created 11/05/2018 Bin Su
 ***********************************************************************/

#ifndef ut0bitset_h
#define ut0bitset_h
#include <bit>
#include <cstdint>
#include <cstring>
#include <limits>
#include "univ.i"
#include "ut0dbg.h"
#include "ut0math.h"

/** A simple bitset wrapper class, which lets you access an existing range of
bytes (not owned by it!) as if it was a <tt>std::bitset</tt> or
<tt>std::vector<bool></tt>.
NOTE: Because it is a wrapper, its semantics are similar to <tt>std::span</tt>.
For example <tt>const Bitset<></tt> can still let someone modify the bits via
<tt>set()</tt> or <tt>reset()</tt>. If you want to prevent someone from editing
the buffer, you'd need <tt>Bitset&lt;const byte></tt>. For same reason,
<tt>bitset1=bitset2</tt> will just repoint <tt>bitset1</tt> to the same range of
bytes as <tt>bitset2</tt> without copying any bits. If you want to copy the bits
use <tt>bitset1.copy_from(bitset2.bitset())</tt> instead. */
template <typename B = byte>
class Bitset {
  static_assert(std::is_same_v<std::decay_t<B>, byte>,
                "Bitset<B> requires B to be a byte or const byte");

 public:
  /** Constructor */
  Bitset() : m_data{nullptr}, m_size_bytes{} {}
  Bitset(B *data, size_t size_bytes) : m_data{data}, m_size_bytes{size_bytes} {}

  /** Returns a wrapper around [pos,pos+len) fragment of the buffer, where pos
  and len are measured in bytes
  @param[in]    byte_offset   position of first byte of selected slice
  @param[in]    bytes_count   length of the slice in bytes
  @return Bitset wrapping the sub-array */
  Bitset bytes_subspan(size_t byte_offset, size_t bytes_count) const {
    ut_ad(byte_offset + bytes_count <= m_size_bytes);
    return Bitset(m_data + byte_offset, bytes_count);
  }
  /** Returns a wrapper around fragment of the buffer starting at pos, where pos
  is measured in bytes
  @param[in]    byte_offset    position of first byte of selected slice
  @return Bitset wrapping the sub-array */
  Bitset bytes_subspan(size_t byte_offset) const {
    ut_ad(byte_offset <= m_size_bytes);
    return bytes_subspan(byte_offset, m_size_bytes - byte_offset);
  }

  /** Copy a bits from other buffer into this one
  @param[in]    bitset  byte array to bits copy from */
  void copy_from(const byte *bitset) const {
    memcpy(m_data, bitset, m_size_bytes);
  }

  /** Set the specified bit to the value 'bit'
  @param[in]    pos     specified bit
  @param[in]    v       true or false */
  void set(size_t pos, bool v = true) const {
    ut_ad(pos / 8 < m_size_bytes);
    m_data[pos / 8] &= ~(0x1 << (pos & 0x7));
    m_data[pos / 8] |= (static_cast<byte>(v) << (pos & 0x7));
  }

  /** Set all bits to true */
  void set() const { memset(m_data, 0xFF, m_size_bytes); }

  /** Set all bits to false */
  void reset() const { memset(m_data, 0, m_size_bytes); }

  /** Sets the specified bit to false
  @param[in]    pos     specified bit */
  void reset(size_t pos) const { set(pos, false); }

  /** Converts the content of the bitset to an uint64_t value, such that
  (value>>i&1) if and only if test(i).
  The m_size must be at most 8 bytes.
  @return content of the bitset as uint64_t mapping i-th bit to 2^i */
  uint64_t to_uint64() const {
    ut_a(m_size_bytes <= 8);
    byte bytes[8] = {};
    memcpy(bytes, m_data, m_size_bytes);
    return to_uint64(bytes);
  }
  /** Value used by find_set to indicate it could not find a bit set to 1.
  It is guaranteed to be larger than the size of the vector. */
  constexpr static size_t NOT_FOUND = std::numeric_limits<size_t>::max();

  /** Finds the smallest position which is set and is not smaller than start_pos
  @param[in]     start_pos   The position from which to start the search.
  @return Smallest pos for which test(pos)==true and start_pos<=pos. In case
  there's no such pos, returns NOT_FOUND */
  size_t find_set(size_t start_pos) const {
    /* The reason this function is so complicated is because it is meant to be
    fast for long sparse bitsets, so it's main part is to iterate over whole
    uint64_t words, ignoring all that are equal to 0. Alas, in general m_bitset
    doesn't have to be aligned to word boundary, neither m_bitse + m_size must
    end at word boundary, worse still m_size could be below 8. Thus we consider
    following cases:
    a) start_pos out of bounds -> return NOT_FOUND
    b) m_size <=8 -> convert the few bytes into uint64_t and use countr_zero
    c) m_bitset aligned -> iter over whole words, handle unfinished word
    recursively (a or b)
    d) unaligned m_bitset -> handle partial word recursively (b), and the
    aligned rest recursively (a, b or c).
    Note that in most important usages of this class m_bitset is aligned. */
    if (m_size_bytes * 8 <= start_pos) {
      return NOT_FOUND;
    }
    if (m_size_bytes <= 8) {
      const uint64_t all = to_uint64();
      const uint64_t earlier = (uint64_t{1} << start_pos) - 1;
      const uint64_t unseen = all & ~earlier;
      if (unseen) {
        return std::countr_zero(unseen);
      }
      return NOT_FOUND;
    }
    const auto start_addr = reinterpret_cast<uintptr_t>(m_data);
    const size_t start_word_byte_idx =
        ut::div_ceil(start_addr, uintptr_t{8}) * 8 - start_addr;
    const auto translate_result = [&start_pos, this](size_t offset) {
      auto found = bytes_subspan(offset / 8).find_set(start_pos - offset);
      return found == NOT_FOUND ? found : found + offset;
    };
    if (start_word_byte_idx == 0) {
      // the middle of the m_bitset consists of uint64_t elements
      auto *words = reinterpret_cast<const uint64_t *>(m_data);
      size_t word_idx = start_pos / 64;
      const auto full_words_count = m_size_bytes / 8;
      if (word_idx < full_words_count) {
        const uint64_t earlier = (uint64_t{1} << start_pos % 64) - 1;
        const uint64_t unseen = to_uint64(m_data + word_idx * 8) & ~earlier;
        if (unseen) {
          return word_idx * 64 + std::countr_zero(unseen);
        }
        // otherwise find first non-empty word
        ++word_idx;
        while (word_idx < full_words_count) {
          if (words[word_idx]) {
            return word_idx * 64 +
                   std::countr_zero(to_uint64(m_data + word_idx * 8));
          }
          ++word_idx;
        }
        start_pos = full_words_count * 64;
      }
      return translate_result(full_words_count * 64);
    }
    // this only occurs for m_bitset not aligned to 64-bit
    if (start_pos < start_word_byte_idx * 8) {
      const auto found =
          bytes_subspan(0, start_word_byte_idx).find_set(start_pos);
      if (found < NOT_FOUND) {
        return found;
      }
      start_pos = start_word_byte_idx * 8;
    }
    return translate_result(start_word_byte_idx * 8);
  }

  /** Test if the specified bit is set or not
  @param[in]    pos     the specified bit
  @return True if this bit is set, otherwise false */
  bool test(size_t pos) const {
    ut_ad(pos / 8 < m_size_bytes);
    return ((m_data[pos / 8] >> (pos & 0x7)) & 0x1);
  }

  /** Get the size of current bitset in bytes
  @return the size of the bitset */
  size_t size_bytes() const { return (m_size_bytes); }

  /** Get the bitset's bytes buffer
  @return current bitset */
  B *data() const { return (m_data); }

 private:
  /** Converts 8 bytes to uint64_t value, such that
  (value>>i&1) equals the i-th bit, i.e. (bytes[i/8]>>i%8 & 1).
  For example, the returned value equals bytes[0] modulo 256.
  @param[in]    bytes   the bytes to convert
  @return uint64_t created by concatenating the bytes in the right order:
  on Little-Endian it's an identity, on Big-Endian it's std::byteswap. */
  static constexpr uint64_t to_uint64(const byte *bytes) {
    /* This pattern this gets recognized by gcc as identity on Little-Endian.
    The benefit of writing it this way is not only that it works on Big-Endian,
    but also that it doesn't require the address to be aligned. */
    return ((uint64_t)(bytes[7]) << 7 * 8) | ((uint64_t)(bytes[6]) << 6 * 8) |
           ((uint64_t)(bytes[5]) << 5 * 8) | ((uint64_t)(bytes[4]) << 4 * 8) |
           ((uint64_t)(bytes[3]) << 3 * 8) | ((uint64_t)(bytes[2]) << 2 * 8) |
           ((uint64_t)(bytes[1]) << 1 * 8) | ((uint64_t)(bytes[0]) << 0 * 8);
  }

  /** The buffer containing the bitmap. First bit is the lowest bit of the first
  byte of this buffer. */
  B *m_data;

  /** The length of the buffer containing the bitmap in bytes. Number of bits is
  8 times larger than this */
  size_t m_size_bytes;
};

#endif
