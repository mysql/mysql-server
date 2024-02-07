/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_BITMASK_H
#define NDB_BITMASK_H

#include "ndb_config.h"
#include "ndb_global.h"

#if defined(HAVE__BITSCANFORWARD) || defined(HAVE__BITSCANREVERSE)
#include <intrin.h>
#endif

/**
 * Bitmask implementation.  Size (in 32-bit words) is given explicitly
 * (as first argument).  All methods are static.
 */
class BitmaskImpl {
 public:
  static constexpr Uint32 NotFound = (unsigned)-1;

  /**
   * get - Check if bit n is set.
   *       assert(n < (32 * size))
   */
  static bool get(unsigned size, const Uint32 data[], unsigned n);

  /**
   * safe_get - Check if bit N is set, accept any value for N
   */
  static bool safe_get(unsigned size, const Uint32 data[], unsigned n);

  /**
   * set - Set bit n to given value (true/false).
   */
  static void set(unsigned size, Uint32 data[], unsigned n, bool value);

  /**
   * set - Set bit n.
   */
  static void set(unsigned size, Uint32 data[], unsigned n);

  /**
   * set - Set all bits.
   */
  static void set(unsigned size, Uint32 data[]);

  /**
   * set <em>len</em> bist from <em>start</em>
   */
  static void setRange(unsigned size, Uint32 data[], unsigned start,
                       unsigned len);

  /**
   * assign - Set all bits in <em>dst</em> to corresponding in <em>src</em>
   */
  static void assign(unsigned size, Uint32 dst[], const Uint32 src[]);

  /**
   * clear - Clear bit n.
   */
  static void clear(unsigned size, Uint32 data[], unsigned n);

  /**
   * clear - Clear all bits.
   */
  static void clear(unsigned size, Uint32 data[]);

  static Uint32 getWord(unsigned size, const Uint32 data[], unsigned word_pos);
  static void setWord(unsigned size, Uint32 data[], unsigned word_pos,
                      Uint32 new_word);
  /**
   * isclear -  Check if all bits are clear.  This is faster
   * than checking count() == 0.
   */
  static bool isclear(unsigned size, const Uint32 data[]);

  /**
   * is_set -  Check if all bits are set.
   */
  static bool is_set(unsigned size, const Uint32 data[]);

  /**
   * count - Count number of set bits.
   */
  static unsigned count(unsigned size, const Uint32 data[]);

  /**
   * return count trailing zero bits inside a word
   * undefined behaviour if non set
   */
  static unsigned ctz(Uint32 x);

  /**
   * return count leading zero bits inside a word
   * undefined behaviour if non set
   */
  static unsigned clz(Uint32 x);

  /**
   * return index of first bit set inside a word
   * undefined behaviour if non set
   */
  static unsigned ffs(Uint32 x);

  /**
   * return index of last bit set inside a word
   * undefined behaviour if non set
   */
  static unsigned fls(Uint32 x);

  /**
   * find - Find first set bit, starting from 0
   * Returns NotFound when not found.
   */
  static unsigned find_first(unsigned size, const Uint32 data[]);

  /**
   * find - Find last set bit, starting from 0
   * Returns NotFound when not found.
   */
  static unsigned find_last(unsigned size, const Uint32 data[]);

  /**
   * find - Find first set bit, starting at given position.
   * Returns NotFound when not found.
   */
  static unsigned find_next(unsigned size, const Uint32 data[], unsigned n);

  /**
   * find - Find last set bit, starting at given position.
   * Returns NotFound when not found.
   */
  static unsigned find_prev(unsigned size, const Uint32 data[], unsigned n);

  /**
   * find - Find first set bit, starting at given position.
   * Returns NotFound when not found.
   */
  static unsigned find(unsigned size, const Uint32 data[], unsigned n);

  /**
   * equal - Bitwise equal.
   */
  static bool equal(unsigned size, const Uint32 data[], const Uint32 data2[]);

  /**
   * bitOR - Bitwise (x | y) into first operand.
   */
  static void bitOR(unsigned size, Uint32 data[], const Uint32 data2[]);

  /**
   * bitAND - Bitwise (x & y) into first operand.
   */
  static void bitAND(unsigned size, Uint32 data[], const Uint32 data2[]);

  /**
   * bitANDC - Bitwise (x & ~y) into first operand.
   */
  static void bitANDC(unsigned size, Uint32 data[], const Uint32 data2[]);

  /**
   * bitXOR - Bitwise (x ^ y) into first operand.
   */
  static void bitXOR(unsigned size, Uint32 data[], const Uint32 data2[]);

  /**
   * bitXORC - Bitwise (x ^ ~y) into first operand.
   */
  static void bitXORC(unsigned size, Uint32 data[], const Uint32 data2[]);

  /**
   * bitNOT - Bitwise (~x) into first operand.
   */
  static void bitNOT(unsigned size, Uint32 data[]);

  /**
   * contains - Check if all bits set in data2 are set in data
   */
  static bool contains(unsigned size, const Uint32 data[],
                       const Uint32 data2[]);

  /**
   * overlaps - Check if any bit set in data is set in data2
   */
  static bool overlaps(unsigned size, const Uint32 data[],
                       const Uint32 data2[]);

  /**
   * getField - Get bitfield at given position and length (max 32 bits)
   */
  static Uint32 getField(unsigned size, const Uint32 data[], unsigned pos,
                         unsigned len);

  /**
   * setField - Set bitfield at given position and length (max 32 bits)
   * Note : length == 0 not supported.
   */
  static void setField(unsigned size, Uint32 data[], unsigned pos, unsigned len,
                       Uint32 val);

  /**
   * getField - Get bitfield at given position and length
   * Note : length == 0 not supported.
   */
  static void getField(unsigned size, const Uint32 data[], unsigned pos,
                       unsigned len, Uint32 dst[]);

  /**
   * setField - Set bitfield at given position and length
   */
  static void setField(unsigned size, Uint32 data[], unsigned pos, unsigned len,
                       const Uint32 src[]);

  /**
   * copyField - Copy bitfield from one position and length
   * to another position and length.
   * Undefined for overlapping bitfields
   */
  static void copyField(Uint32 dst[], unsigned destPos, const Uint32 src[],
                        unsigned srcPos, unsigned len);

  /**
   * getText - Return as hex-digits (only for debug routines).
   */
  static char *getText(unsigned size, const Uint32 data[], char *buf);

  /* Fast bit counting (16 instructions on x86_64, gcc -O3). */
  static inline Uint32 count_bits(Uint32 x);

  /**
   * store each set bit in <em>dst</em> and return bits found
   */
  static Uint32 toArray(Uint8 *dst, Uint32 len, unsigned size,
                        const Uint32 data[]);

 private:
  static void getFieldImpl(const Uint32 data[], unsigned, unsigned, Uint32[]);
  static void setFieldImpl(Uint32 data[], unsigned, unsigned, const Uint32[]);
};

inline bool BitmaskImpl::get(unsigned size [[maybe_unused]],
                             const Uint32 data[], unsigned n) {
  assert(n < (size << 5));
  return (data[n >> 5] & (1 << (n & 31))) != 0;
}

inline bool BitmaskImpl::safe_get(unsigned size, const Uint32 data[],
                                  unsigned n) {
  if (n < (size << 5)) {
    return (data[n >> 5] & (1 << (n & 31))) != 0;
  }
  return false;
}

inline void BitmaskImpl::set(unsigned size, Uint32 data[], unsigned n,
                             bool value) {
  value ? set(size, data, n) : clear(size, data, n);
}

inline void BitmaskImpl::set(unsigned size [[maybe_unused]], Uint32 data[],
                             unsigned n) {
  assert(n < (size << 5));
  data[n >> 5] |= (1 << (n & 31));
}

inline void BitmaskImpl::set(unsigned size, Uint32 data[]) {
  for (unsigned i = 0; i < size; i++) {
    data[i] = ~0;
  }
}

inline void BitmaskImpl::setRange(unsigned size [[maybe_unused]], Uint32 data[],
                                  unsigned start, unsigned len) {
  if (len == 0) {
    return;
  }

  assert(start < (size << 5));
  Uint32 last = start + len - 1;
  Uint32 *ptr = data + (start >> 5);
  Uint32 *end = data + (last >> 5);
  assert(start <= last);
  assert(last < (size << 5));

  Uint32 tmp_word = ~(Uint32)0 << (start & 31);

  if (ptr < end) {
    *ptr++ |= tmp_word;

    for (; ptr < end;) {
      *ptr++ = ~(Uint32)0;
    }

    tmp_word = ~(Uint32)0;
  }

  tmp_word &= ~(~(Uint32)1 << (last & 31));

  *ptr |= tmp_word;
}

inline void BitmaskImpl::assign(unsigned size, Uint32 dst[],
                                const Uint32 src[]) {
  for (unsigned i = 0; i < size; i++) {
    dst[i] = src[i];
  }
}

inline void BitmaskImpl::clear(unsigned size [[maybe_unused]], Uint32 data[],
                               unsigned n) {
  assert(n < (size << 5));
  data[n >> 5] &= ~(1 << (n & 31));
}

inline void BitmaskImpl::clear(unsigned size, Uint32 data[]) {
  for (unsigned i = 0; i < size; i++) {
    data[i] = 0;
  }
}

inline Uint32 BitmaskImpl::getWord(unsigned size [[maybe_unused]],
                                   const Uint32 data[], unsigned word_pos) {
  return data[word_pos];
}

inline void BitmaskImpl::setWord(unsigned size [[maybe_unused]], Uint32 data[],
                                 unsigned word_pos, Uint32 new_word) {
  data[word_pos] = new_word;
  return;
}

inline bool BitmaskImpl::isclear(unsigned size, const Uint32 data[]) {
  for (unsigned i = 0; i < size; i++) {
    if (data[i] != 0) return false;
  }
  return true;
}

inline bool BitmaskImpl::is_set(unsigned size, const Uint32 data[]) {
  for (unsigned i = 0; i < size; i++) {
    if (~data[i] != 0) return false;
  }
  return true;
}

inline unsigned BitmaskImpl::count(unsigned size, const Uint32 data[]) {
  unsigned cnt = 0;
  for (unsigned i = 0; i < size; i++) {
    cnt += count_bits(data[i]);
  }
  return cnt;
}

/**
 * return count trailing zero bits inside a word
 * undefined behaviour if non set
 */
inline Uint32 BitmaskImpl::ctz(Uint32 x) { return ffs(x); }

/**
 * return count leading bits inside a word
 * undefined behaviour if non set
 */
inline Uint32 BitmaskImpl::clz(Uint32 x) {
#if defined HAVE___BUILTIN_CLZ
  return __builtin_clz(x);
#elif defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
  asm("bsr %1,%0" : "=r"(x) : "rm"(x));
  return 31 - x;
#elif defined HAVE__BITSCANREVERSE
  unsigned long r;
  unsigned char res [[maybe_unused]] = _BitScanReverse(&r, (unsigned long)x);
  assert(res > 0);
  return 31 - (Uint32)r;
#else
  int b = 0;
  if (!(x & 0xffff0000)) {
    x <<= 16;
    b += 16;
  }
  if (!(x & 0xff000000)) {
    x <<= 8;
    b += 8;
  }
  if (!(x & 0xf0000000)) {
    x <<= 4;
    b += 4;
  }
  if (!(x & 0xc0000000)) {
    x <<= 2;
    b += 2;
  }
  if (!(x & 0x80000000)) {
    x <<= 1;
    b += 1;
  }
  return b;
#endif
}

/**
 * return index of first bit set inside a word
 * undefined behaviour if non set
 */
inline Uint32 BitmaskImpl::ffs(Uint32 x) {
#if defined HAVE___BUILTIN_CTZ
  return __builtin_ctz(x);
#elif defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
  asm("bsf %1,%0" : "=r"(x) : "rm"(x));
  return x;
#elif defined HAVE___BUILTIN_FFS
  /**
   * gcc defined ffs(0) == 0, and returned indexes 1-32
   */
  return __builtin_ffs(x) - 1;
#elif defined HAVE__BITSCANFORWARD
  unsigned long r;
  unsigned char res [[maybe_unused]] = _BitScanForward(&r, (unsigned long)x);
  assert(res > 0);
  return (Uint32)r;
#elif defined HAVE_FFS
  return ::ffs(x) - 1;
#else
  int b = 0;
  if (!(x & 0xffff)) {
    x >>= 16;
    b += 16;
  }
  if (!(x & 0xff)) {
    x >>= 8;
    b += 8;
  }
  if (!(x & 0xf)) {
    x >>= 4;
    b += 4;
  }
  if (!(x & 3)) {
    x >>= 2;
    b += 2;
  }
  if (!(x & 1)) {
    x >>= 1;
    b += 1;
  }
  return b;
#endif
}

/**
 * return index of last bit set inside a word
 * undefined behaviour if non set
 */
inline Uint32 BitmaskImpl::fls(Uint32 x) {
#if defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
  asm("bsr %1,%0" : "=r"(x) : "rm"(x));
  return x;
#elif defined HAVE___BUILTIN_CLZ
  return 31 - __builtin_clz(x);
#elif defined HAVE__BITSCANREVERSE
  unsigned long r;
  unsigned char res [[maybe_unused]] = _BitScanReverse(&r, (unsigned long)x);
  assert(res > 0);
  return (Uint32)r;
#else
  int b = 31;
  if (!(x & 0xffff0000)) {
    x <<= 16;
    b -= 16;
  }
  if (!(x & 0xff000000)) {
    x <<= 8;
    b -= 8;
  }
  if (!(x & 0xf0000000)) {
    x <<= 4;
    b -= 4;
  }
  if (!(x & 0xc0000000)) {
    x <<= 2;
    b -= 2;
  }
  if (!(x & 0x80000000)) {
    x <<= 1;
    b -= 1;
  }
  return b;
#endif
}

inline unsigned BitmaskImpl::find_first(unsigned size, const Uint32 data[]) {
  Uint32 n = 0;
  while (n < (size << 5)) {
    Uint32 val = data[n >> 5];
    if (val) {
      return n + ffs(val);
    }
    n += 32;
  }
  return NotFound;
}

inline unsigned BitmaskImpl::find_last(unsigned size, const Uint32 data[]) {
  if (size == 0) return NotFound;
  Uint32 n = (size << 5) - 1;
  do {
    Uint32 val = data[n >> 5];
    if (val) {
      return n - clz(val);
    }
    n -= 32;
  } while (n != 0xffffffff);
  return NotFound;
}

inline unsigned BitmaskImpl::find_next(unsigned size, const Uint32 data[],
                                       unsigned n) {
  assert(n <= (size << 5));
  if (n == (size << 5))  // allow one step utside for easier use
    return NotFound;
  Uint32 val = data[n >> 5];
  Uint32 b = n & 31;
  if (b) {
    val >>= b;
    if (val) {
      return n + ffs(val);
    }
    n += 32 - b;
  }

  while (n < (size << 5)) {
    val = data[n >> 5];
    if (val) {
      return n + ffs(val);
    }
    n += 32;
  }
  return NotFound;
}

inline unsigned BitmaskImpl::find_prev(unsigned size [[maybe_unused]],
                                       const Uint32 data[], unsigned n) {
  if (n >= (Uint32)0xffffffff /* -1 */)  // allow one bit outside array for
                                         // easier use
    return NotFound;
  assert(n < (size << 5));
  Uint32 val = data[n >> 5];
  Uint32 b = n & 31;
  if (b < 31) {
    val <<= 31 - b;
    if (val) {
      return n - clz(val);
    }
    n -= b + 1;
  }

  while (n != NotFound) {
    val = data[n >> 5];
    if (val) {
      return n - clz(val);
    }
    n -= 32;
  }
  return NotFound;
}

inline unsigned BitmaskImpl::find(unsigned size, const Uint32 data[],
                                  unsigned n) {
  return find_next(size, data, n);
}

inline bool BitmaskImpl::equal(unsigned size, const Uint32 data[],
                               const Uint32 data2[]) {
  for (unsigned i = 0; i < size; i++) {
    if (data[i] != data2[i]) return false;
  }
  return true;
}

inline void BitmaskImpl::bitOR(unsigned size, Uint32 data[],
                               const Uint32 data2[]) {
  for (unsigned i = 0; i < size; i++) {
    data[i] |= data2[i];
  }
}

inline void BitmaskImpl::bitAND(unsigned size, Uint32 data[],
                                const Uint32 data2[]) {
  for (unsigned i = 0; i < size; i++) {
    data[i] &= data2[i];
  }
}

inline void BitmaskImpl::bitANDC(unsigned size, Uint32 data[],
                                 const Uint32 data2[]) {
  for (unsigned i = 0; i < size; i++) {
    data[i] &= ~data2[i];
  }
}

inline void BitmaskImpl::bitXOR(unsigned size, Uint32 data[],
                                const Uint32 data2[]) {
  for (unsigned i = 0; i < size; i++) {
    data[i] ^= data2[i];
  }
}

inline void BitmaskImpl::bitXORC(unsigned size, Uint32 data[],
                                 const Uint32 data2[]) {
  for (unsigned i = 0; i < size; i++) {
    data[i] ^= ~data2[i];
  }
}

inline void BitmaskImpl::bitNOT(unsigned size, Uint32 data[]) {
  for (unsigned i = 0; i < size; i++) {
    data[i] = ~data[i];
  }
}

inline bool BitmaskImpl::contains(unsigned size, const Uint32 data[],
                                  const Uint32 data2[]) {
  for (unsigned int i = 0; i < size; i++)
    if ((data[i] & data2[i]) != data2[i]) return false;
  return true;
}

inline bool BitmaskImpl::overlaps(unsigned size, const Uint32 data[],
                                  const Uint32 data2[]) {
  for (unsigned int i = 0; i < size; i++)
    if ((data[i] & data2[i]) != 0) return true;
  return false;
}

inline Uint32 BitmaskImpl::getField(unsigned size, const Uint32 data[],
                                    unsigned pos, unsigned len) {
  Uint32 val = 0;
  for (unsigned i = 0; i < len; i++) val |= get(size, data, pos + i) << i;
  return val;
}

inline void BitmaskImpl::setField(unsigned size, Uint32 data[], unsigned pos,
                                  unsigned len, Uint32 val) {
  for (unsigned i = 0; i < len; i++) set(size, data, pos + i, val & (1 << i));
}

inline char *BitmaskImpl::getText(unsigned size, const Uint32 data[],
                                  char *buf) {
  char *org = buf;
  const char *const hex = "0123456789abcdef";
  for (int i = (size - 1); i >= 0; i--) {
    Uint32 x = data[i];
    for (unsigned j = 0; j < 8; j++) {
      buf[7 - j] = hex[x & 0xf];
      x >>= 4;
    }
    buf += 8;
  }
  *buf = 0;
  return org;
}

inline Uint32 BitmaskImpl::count_bits(Uint32 x) {
  x = x - ((x >> 1) & 0x55555555);
  x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
  x = (x + (x >> 4)) & 0x0f0f0f0f;
  x = (x * 0x01010101) >> 24;
  return x;
}

inline Uint32 BitmaskImpl::toArray(Uint8 *dst, Uint32 len [[maybe_unused]],
                                   unsigned size, const Uint32 *data) {
  assert(len >= size * 32);
  assert(32 * size <= 256);  // Uint8
  Uint8 *save = dst;
  for (Uint32 i = 0; i < size; i++) {
    Uint32 val = *data++;
    Uint32 bit = 0;
    while (val) {
      if (val & (1 << bit)) {
        *dst++ = 32 * i + bit;
        val &= ~(1U << bit);
      }
      bit++;
    }
  }
  return (Uint32)(dst - save);
}

/**
 * Bitmasks.  The size is number of 32-bit words (Uint32).
 * Unused bits in the last word must be zero.
 *
 * XXX replace size by length in bits
 */
template <unsigned size>
struct BitmaskPOD {
 public:
  /**
   * POD data representation
   */
  struct Data {
    Uint32 data[size];
#if 0
    Data & operator=(const BitmaskPOD<size> & src) {
      src.copyto(size, data);
      return *this;
    }
#endif
  };

  Data rep;

 public:
  static constexpr Uint32 Size = size;
  static constexpr Uint32 NotFound = BitmaskImpl::NotFound;
  static constexpr Uint32 TextLength = size * 8;

  /**
   * Return the length- number of words required to store the bitmask.
   * i.e the index of last non-zero word plus one.
   */
  Uint32 getPackedLengthInWords() const {
    Uint32 packed_length = 0;
    for (Uint32 i = 0; i < size; i++) {
      if (rep.data[i] != 0) {
        packed_length = i + 1;
      }
    }
    return packed_length;
  }

  static Uint32 getPackedLengthInWords(const Uint32 bitmaskarray[]) {
    Uint32 packed_length = 0;
    for (Uint32 i = 0; i < size; i++) {
      if (bitmaskarray[i] != 0) {
        packed_length = i + 1;
      }
    }
    return packed_length;
  }

  Uint32 getSizeInWords() const { return Size; }

  unsigned max_size() const { return (size * 32) - 1; }

  /**
   * assign - Set all bits in <em>dst</em> to corresponding in <em>src</em>
   */
  void assign(const typename BitmaskPOD<size>::Data &src);

  /**
   * assign - Set all bits in <em>dst</em> to corresponding in <em>src</em>
   */
  static void assign(Uint32 dst[], const Uint32 src[]);
  static void assign(Uint32 dst[], const BitmaskPOD<size> &src);
  void assign(const BitmaskPOD<size> &src);

  /**
   * copy this to <em>dst</em>
   */
  void copyto(unsigned sz, Uint32 dst[]) const;

  /**
   * assign <em>this</em> according to <em>src</em>
   */
  void assign(unsigned sz, const Uint32 src[]);

  /**
   * start of static members
   */

  /**
   * get - Check if bit n is set.
   */
  static bool get(const Uint32 data[], unsigned n);
  bool get(unsigned n) const;

  /**
   * safe_get - Check if bit N is set, accept any value for N
   */
  static bool safe_get(const Uint32 data[], unsigned n);
  bool safe_get(unsigned n) const;

  /**
   * set - Set bit n to given value (true/false).
   */
  static void set(Uint32 data[], unsigned n, bool value);
  void set(unsigned n, bool value);

  /**
   * set - Set bit n.
   */
  static void set(Uint32 data[], unsigned n);
  void set(unsigned n);

  /**
   * set - set all bits.
   */
  static void set(Uint32 data[]);
  void set();

  /**
   * set - set a range of bits
   */
  static void setRange(Uint32 data[], Uint32 pos, Uint32 len);
  void setRange(Uint32 pos, Uint32 len);

  /**
   * clear - Clear bit n.
   */
  static void clear(Uint32 data[], unsigned n);
  void clear(unsigned n);

  /**
   * clear - Clear all bits.
   */
  static void clear(Uint32 data[]);
  void clear();

  /**
   * Get and set words of bits
   */
  Uint32 getWord(unsigned word_pos) const;
  void setWord(unsigned word_pos, Uint32 new_word);

  /**
   * isclear -  Check if all bits are clear.  This is faster
   * than checking count() == 0.
   */
  static bool isclear(const Uint32 data[]);
  bool isclear() const;

  /**
   * is_set -  Check if all bits are set.
   */
  static bool is_set(const Uint32 data[]);
  bool is_set() const;

  /**
   * count - Count number of set bits.
   */
  static unsigned count(const Uint32 data[]);
  unsigned count() const;

  /**
   * find - Find first set bit, starting at 0
   * Returns NotFound when not found.
   */
  static unsigned find_first(const Uint32 data[]);
  unsigned find_first() const;

  /**
   * find - Find first set bit, starting at 0
   * Returns NotFound when not found.
   */
  static unsigned find_next(const Uint32 data[], unsigned n);
  unsigned find_next(unsigned n) const;

  /**
   * find - Find last set bit, starting at 0
   * Returns NotFound when not found.
   */
  static unsigned find_last(const Uint32 data[]);
  unsigned find_last() const;

  /**
   * find - Find previous set bit, starting at n
   * Returns NotFound when not found.
   */
  static unsigned find_prev(const Uint32 data[], unsigned n);
  unsigned find_prev(unsigned n) const;

  /**
   * find - Find first set bit, starting at given position.
   * Returns NotFound when not found.
   */
  static unsigned find(const Uint32 data[], unsigned n);
  unsigned find(unsigned n) const;

  /**
   * equal - Bitwise equal.
   */
  static bool equal(const Uint32 data[], const Uint32 data2[]);
  bool equal(const BitmaskPOD<size> &mask2) const;

  /**
   * bitOR - Bitwise (x | y) into first operand.
   */
  static void bitOR(Uint32 data[], const Uint32 data2[]);
  BitmaskPOD<size> &bitOR(const BitmaskPOD<size> &mask2);

  /**
   * bitAND - Bitwise (x & y) into first operand.
   */
  static void bitAND(Uint32 data[], const Uint32 data2[]);
  BitmaskPOD<size> &bitAND(const BitmaskPOD<size> &mask2);

  /**
   * bitANDC - Bitwise (x & ~y) into first operand.
   */
  static void bitANDC(Uint32 data[], const Uint32 data2[]);
  BitmaskPOD<size> &bitANDC(const BitmaskPOD<size> &mask2);

  /**
   * bitXOR - Bitwise (x ^ y) into first operand.
   */
  static void bitXOR(Uint32 data[], const Uint32 data2[]);
  BitmaskPOD<size> &bitXOR(const BitmaskPOD<size> &mask2);

  /**
   * bitXORC - Bitwise (x ^ ~y) into first operand.
   */
  static void bitXORC(Uint32 data[], const Uint32 data2[]);
  BitmaskPOD<size> &bitXORC(const BitmaskPOD<size> &mask2);

  /**
   * bitNOT - Bitwise (~x) in first operand.
   */
  static void bitNOT(Uint32 data[]);
  BitmaskPOD<size> &bitNOT();

  /**
   * contains - Check if all bits set in data2 (that) are also set in data
   * (this)
   */
  static bool contains(const Uint32 data[], const Uint32 data2[]);
  bool contains(BitmaskPOD<size> that) const;

  /**
   * overlaps - Check if any bit set in this BitmaskPOD (data) is also set in
   * that (data2)
   */
  static bool overlaps(const Uint32 data[], const Uint32 data2[]);
  bool overlaps(BitmaskPOD<size> that) const;

  /**
   * getText - Return as hex-digits (only for debug routines).
   */
  static char *getText(const Uint32 data[], char *buf);
  char *getText(char *buf) const;

  static Uint32 toArray(Uint8 *dst, Uint32 len, const Uint32 data[]);
  Uint32 toArray(Uint8 *dst, Uint32 len) const;
};

template <unsigned size>
inline void BitmaskPOD<size>::assign(Uint32 dst[], const Uint32 src[]) {
  BitmaskImpl::assign(size, dst, src);
}

template <unsigned size>
inline void BitmaskPOD<size>::assign(Uint32 dst[],
                                     const BitmaskPOD<size> &src) {
  BitmaskImpl::assign(size, dst, src.rep.data);
}

template <unsigned size>
inline void BitmaskPOD<size>::assign(
    const typename BitmaskPOD<size>::Data &src) {
  BitmaskPOD<size>::assign(rep.data, src.data);
}

template <unsigned size>
inline void BitmaskPOD<size>::assign(const BitmaskPOD<size> &src) {
  BitmaskPOD<size>::assign(rep.data, src.rep.data);
}

template <unsigned size>
inline void BitmaskPOD<size>::copyto(unsigned sz, Uint32 dst[]) const {
  BitmaskImpl::assign(sz, dst, rep.data);
}

template <unsigned size>
inline void BitmaskPOD<size>::assign(unsigned sz, const Uint32 src[]) {
  BitmaskImpl::assign(sz, rep.data, src);
}

template <unsigned size>
inline bool BitmaskPOD<size>::get(const Uint32 data[], unsigned n) {
  return BitmaskImpl::get(size, data, n);
}

template <unsigned size>
inline bool BitmaskPOD<size>::get(unsigned n) const {
  return BitmaskPOD<size>::get(rep.data, n);
}

template <unsigned size>
inline bool BitmaskPOD<size>::safe_get(const Uint32 data[], unsigned n) {
  return BitmaskImpl::safe_get(size, data, n);
}

template <unsigned size>
inline bool BitmaskPOD<size>::safe_get(unsigned n) const {
  return BitmaskPOD<size>::safe_get(rep.data, n);
}

template <unsigned size>
inline void BitmaskPOD<size>::set(Uint32 data[], unsigned n, bool value) {
  BitmaskImpl::set(size, data, n, value);
}

template <unsigned size>
inline void BitmaskPOD<size>::set(unsigned n, bool value) {
  BitmaskPOD<size>::set(rep.data, n, value);
}

template <unsigned size>
inline void BitmaskPOD<size>::set(Uint32 data[], unsigned n) {
  BitmaskImpl::set(size, data, n);
}

template <unsigned size>
inline void BitmaskPOD<size>::set(unsigned n) {
  BitmaskPOD<size>::set(rep.data, n);
}

template <unsigned size>
inline void BitmaskPOD<size>::set(Uint32 data[]) {
  BitmaskImpl::set(size, data);
}

template <unsigned size>
inline void BitmaskPOD<size>::set() {
  BitmaskPOD<size>::set(rep.data);
}

template <unsigned size>
inline void BitmaskPOD<size>::setRange(Uint32 data[], Uint32 pos, Uint32 len) {
  BitmaskImpl::setRange(size, data, pos, len);
}

template <unsigned size>
inline void BitmaskPOD<size>::setRange(Uint32 pos, Uint32 len) {
  BitmaskPOD<size>::setRange(rep.data, pos, len);
}

template <unsigned size>
inline void BitmaskPOD<size>::clear(Uint32 data[], unsigned n) {
  BitmaskImpl::clear(size, data, n);
}

template <unsigned size>
inline void BitmaskPOD<size>::clear(unsigned n) {
  BitmaskPOD<size>::clear(rep.data, n);
}

template <unsigned size>
inline void BitmaskPOD<size>::clear(Uint32 data[]) {
  BitmaskImpl::clear(size, data);
}

template <unsigned size>
inline void BitmaskPOD<size>::clear() {
  BitmaskPOD<size>::clear(rep.data);
}

template <unsigned size>
inline Uint32 BitmaskPOD<size>::getWord(unsigned word_pos) const {
  return BitmaskImpl::getWord(size, rep.data, word_pos);
}

template <unsigned size>
inline void BitmaskPOD<size>::setWord(unsigned word_pos, Uint32 new_word) {
  BitmaskImpl::setWord(size, rep.data, word_pos, new_word);
}

template <unsigned size>
inline bool BitmaskPOD<size>::isclear(const Uint32 data[]) {
  return BitmaskImpl::isclear(size, data);
}

template <unsigned size>
inline bool BitmaskPOD<size>::isclear() const {
  return BitmaskPOD<size>::isclear(rep.data);
}

template <unsigned size>
inline bool BitmaskPOD<size>::is_set(const Uint32 data[]) {
  return BitmaskImpl::is_set(size, data);
}

template <unsigned size>
inline bool BitmaskPOD<size>::is_set() const {
  return BitmaskPOD<size>::is_set(rep.data);
}

template <unsigned size>
inline unsigned BitmaskPOD<size>::count(const Uint32 data[]) {
  return BitmaskImpl::count(size, data);
}

template <unsigned size>
inline unsigned BitmaskPOD<size>::count() const {
  return BitmaskPOD<size>::count(rep.data);
}

template <unsigned size>
inline unsigned BitmaskPOD<size>::find_first(const Uint32 data[]) {
  return BitmaskImpl::find_first(size, data);
}

template <unsigned size>
inline unsigned BitmaskPOD<size>::find_first() const {
  return BitmaskPOD<size>::find_first(rep.data);
}

template <unsigned size>
inline unsigned BitmaskPOD<size>::find_next(const Uint32 data[], unsigned n) {
  return BitmaskImpl::find_next(size, data, n);
}

template <unsigned size>
inline unsigned BitmaskPOD<size>::find_next(unsigned n) const {
  return BitmaskPOD<size>::find_next(rep.data, n);
}

template <unsigned size>
inline unsigned BitmaskPOD<size>::find_last(const Uint32 data[]) {
  return BitmaskImpl::find_last(size, data);
}

template <unsigned size>
inline unsigned BitmaskPOD<size>::find_last() const {
  return BitmaskPOD<size>::find_last(rep.data);
}

template <unsigned size>
inline unsigned BitmaskPOD<size>::find_prev(const Uint32 data[], unsigned n) {
  return BitmaskImpl::find_prev(size, data, n);
}

template <unsigned size>
inline unsigned BitmaskPOD<size>::find_prev(unsigned n) const {
  return BitmaskPOD<size>::find_prev(rep.data, n);
}

template <unsigned size>
inline unsigned BitmaskPOD<size>::find(const Uint32 data[], unsigned n) {
  return find_next(data, n);
}

template <unsigned size>
inline unsigned BitmaskPOD<size>::find(unsigned n) const {
  return find_next(n);
}

template <unsigned size>
inline bool BitmaskPOD<size>::equal(const Uint32 data[], const Uint32 data2[]) {
  return BitmaskImpl::equal(size, data, data2);
}

template <unsigned size>
inline bool BitmaskPOD<size>::equal(const BitmaskPOD<size> &mask2) const {
  return BitmaskPOD<size>::equal(rep.data, mask2.rep.data);
}

template <unsigned size>
inline void BitmaskPOD<size>::bitOR(Uint32 data[], const Uint32 data2[]) {
  BitmaskImpl::bitOR(size, data, data2);
}

template <unsigned size>
inline BitmaskPOD<size> &BitmaskPOD<size>::bitOR(
    const BitmaskPOD<size> &mask2) {
  BitmaskPOD<size>::bitOR(rep.data, mask2.rep.data);
  return *this;
}

template <unsigned size>
inline void BitmaskPOD<size>::bitAND(Uint32 data[], const Uint32 data2[]) {
  BitmaskImpl::bitAND(size, data, data2);
}

template <unsigned size>
inline BitmaskPOD<size> &BitmaskPOD<size>::bitAND(
    const BitmaskPOD<size> &mask2) {
  BitmaskPOD<size>::bitAND(rep.data, mask2.rep.data);
  return *this;
}

template <unsigned size>
inline void BitmaskPOD<size>::bitANDC(Uint32 data[], const Uint32 data2[]) {
  BitmaskImpl::bitANDC(size, data, data2);
}

template <unsigned size>
inline BitmaskPOD<size> &BitmaskPOD<size>::bitANDC(
    const BitmaskPOD<size> &mask2) {
  BitmaskPOD<size>::bitANDC(rep.data, mask2.rep.data);
  return *this;
}

template <unsigned size>
inline void BitmaskPOD<size>::bitXOR(Uint32 data[], const Uint32 data2[]) {
  BitmaskImpl::bitXOR(size, data, data2);
}

template <unsigned size>
inline BitmaskPOD<size> &BitmaskPOD<size>::bitXOR(
    const BitmaskPOD<size> &mask2) {
  BitmaskPOD<size>::bitXOR(rep.data, mask2.rep.data);
  return *this;
}

template <unsigned size>
inline void BitmaskPOD<size>::bitXORC(Uint32 data[], const Uint32 data2[]) {
  BitmaskImpl::bitXORC(size, data, data2);
}

template <unsigned size>
inline BitmaskPOD<size> &BitmaskPOD<size>::bitXORC(
    const BitmaskPOD<size> &mask2) {
  BitmaskPOD<size>::bitXORC(rep.data, mask2.rep.data);
  return *this;
}

template <unsigned size>
inline void BitmaskPOD<size>::bitNOT(Uint32 data[]) {
  BitmaskImpl::bitNOT(size, data);
}

template <unsigned size>
inline BitmaskPOD<size> &BitmaskPOD<size>::bitNOT() {
  BitmaskPOD<size>::bitNOT(rep.data);
  return *this;
}

template <unsigned size>
inline char *BitmaskPOD<size>::getText(const Uint32 data[], char *buf) {
  return BitmaskImpl::getText(size, data, buf);
}

template <unsigned size>
inline char *BitmaskPOD<size>::getText(char *buf) const {
  return BitmaskPOD<size>::getText(rep.data, buf);
}

template <unsigned size>
inline bool BitmaskPOD<size>::contains(const Uint32 data[],
                                       const Uint32 data2[]) {
  return BitmaskImpl::contains(size, data, data2);
}

template <unsigned size>
inline bool BitmaskPOD<size>::contains(BitmaskPOD<size> that) const {
  return BitmaskPOD<size>::contains(this->rep.data, that.rep.data);
}

template <unsigned size>
inline bool BitmaskPOD<size>::overlaps(const Uint32 data[],
                                       const Uint32 data2[]) {
  return BitmaskImpl::overlaps(size, data, data2);
}

template <unsigned size>
inline bool BitmaskPOD<size>::overlaps(BitmaskPOD<size> that) const {
  return BitmaskPOD<size>::overlaps(this->rep.data, that.rep.data);
}

template <unsigned size>
inline Uint32 BitmaskPOD<size>::toArray(Uint8 *dst, Uint32 len,
                                        const Uint32 data[]) {
  return BitmaskImpl::toArray(dst, len, size, data);
}

template <unsigned size>
inline Uint32 BitmaskPOD<size>::toArray(Uint8 *dst, Uint32 len) const {
  return BitmaskImpl::toArray(dst, len, size, this->rep.data);
}

template <unsigned size>
class Bitmask : public BitmaskPOD<size> {
 public:
  Bitmask() { this->clear(); }
  Bitmask(bool v) { (void)v; }

  template <unsigned sz2>
  Bitmask &operator=(const Bitmask<sz2> &src) {
    if (size >= sz2) {
      for (unsigned i = 0; i < sz2; i++) {
        this->rep.data[i] = src.rep.data[i];
      }
    } else {
      assert(src.find(32 * size + 1) == BitmaskImpl::NotFound);
      for (unsigned i = 0; i < size; i++) {
        this->rep.data[i] = src.rep.data[i];
      }
    }
    return *this;
  }

  template <unsigned sz2>
  Bitmask &operator=(const BitmaskPOD<sz2> &src) {
    if (size >= sz2) {
      for (unsigned i = 0; i < sz2; i++) {
        this->rep.data[i] = src.rep.data[i];
      }
    } else {
      assert(src.find(32 * size + 1) == BitmaskImpl::NotFound);
      for (unsigned i = 0; i < size; i++) {
        this->rep.data[i] = src.rep.data[i];
      }
    }
    return *this;
  }
};

inline void BitmaskImpl::getField(unsigned size [[maybe_unused]],
                                  const Uint32 src[], unsigned pos,
                                  unsigned len, Uint32 dst[]) {
  assert(pos + len <= (size << 5));
  assert(len != 0);
  if (len == 0) return;

  src += (pos >> 5);
  Uint32 offset = pos & 31;
  *dst = (*src >> offset) & (len >= 32 ? ~0 : (1 << len) - 1);

  if (offset + len <= 32) {
    return;
  }
  Uint32 used = (32 - offset);
  assert(len > used);
  getFieldImpl(src + 1, used & 31, len - used, dst + (used >> 5));
}

inline void BitmaskImpl::setField(unsigned size [[maybe_unused]], Uint32 dst[],
                                  unsigned pos, unsigned len,
                                  const Uint32 src[]) {
  assert(pos + len <= (size << 5));
  assert(len != 0);
  if (len == 0) return;

  dst += (pos >> 5);
  Uint32 offset = pos & 31;
  Uint32 mask = (len >= 32 ? ~0 : (1 << len) - 1) << offset;

  *dst = (*dst & ~mask) | ((*src << offset) & mask);

  if (offset + len <= 32) {
    return;
  }
  Uint32 used = (32 - offset);
  assert(len > used);
  setFieldImpl(dst + 1, used & 31, len - used, src + (used >> 5));
}

/* Three way min utiltiy for copyField below */
inline unsigned minLength(unsigned a, unsigned b, unsigned c) {
  return (a < b ? (a < c ? a : c) : (b < c ? b : c));
}

inline void BitmaskImpl::copyField(Uint32 _dst[], unsigned dstPos,
                                   const Uint32 _src[], unsigned srcPos,
                                   unsigned len) {
  /* Algorithm
   * While (len > 0)
   *  - Find the longest bit length we can copy from one 32-bit word
   *    to another (which is the minimum of remaining length,
   *    space in current src word and space in current dest word)
   *  - Extract that many bits from src, and shift them to the correct
   *    position to insert into dest
   *  - Mask out the to-be-written words from dest (and any irrelevant
   *    words in src) and or them together
   *  - Move onto next chunk
   */
  while (len > 0) {
    const Uint32 *src = _src + (srcPos >> 5);
    Uint32 *dst = _dst + (dstPos >> 5);
    unsigned srcOffset = srcPos & 31;
    unsigned dstOffset = dstPos & 31;
    unsigned srcBitsInWord = 32 - srcOffset;
    unsigned dstBitsInWord = 32 - dstOffset;

    /* How many bits can we copy at once? */
    unsigned bits = minLength(dstBitsInWord, srcBitsInWord, len);

    /* Create mask for affected bits in dest */
    Uint32 destMask = (~(Uint32)0 >> (32 - bits) << dstOffset);

    /* Grab source data and shift to dest offset */
    Uint32 data = ((*src) >> srcOffset) << dstOffset;

    /* Mask out affected bits in dest and irrelevant bits in source
     * and combine
     */
    *dst = (*dst & ~destMask) | (data & destMask);

    srcPos += bits;
    dstPos += bits;
    len -= bits;
  }

  return;
}

#endif
