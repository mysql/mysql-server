/*****************************************************************************

Copyright (c) 1994, 2022, Oracle and/or its affiliates.

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

/** @file include/ut0bitset.h
 Utilities for bitset operations

 Created 11/05/2018 Bin Su
 ***********************************************************************/

#ifndef ut0bitset_h
#define ut0bitset_h

/** A simple bitset wrapper class, whose size can be specified
after the object has been defined */
class Bitset {
 public:
  /** Constructor */
  Bitset() : m_bitset(nullptr), m_size(0) {}

  /** Destructor */
  ~Bitset() = default;

  /** Initialize the bitset with a byte array and size
  @param[in]    bitset  byte array for this bitset
  @param[in]    size   size of the byte array */
  void init(byte *bitset, size_t size) {
    m_bitset = bitset;
    m_size = size;
    m_capacity = size;
  }

  /** Copy a byte array and size to current bitmap
  @param[in]    bitset  byte array for this bitset
  @param[in]    size   size of the byte array */
  void copy(const byte *bitset, size_t size) {
    ut_ad(m_capacity >= size);
    memcpy(m_bitset, bitset, size);
    m_size = size;
  }

  /** Set the specified bit to the value 'bit'
  @param[in]    pos     specified bit
  @param[in]    v       true or false */
  void set(size_t pos, bool v = true) {
    ut_ad(pos / 8 < m_size);
    m_bitset[pos / 8] &= ~(0x1 << (pos & 0x7));
    m_bitset[pos / 8] |= (static_cast<uint>(v) << (pos & 0x7));
  }

  /** Set all bits to true */
  void set() { memset(m_bitset, 0xFF, m_size); }

  /** Set all bits to false */
  void reset() { memset(m_bitset, 0, m_size); }

  /** Test if the specified bit is set or not
  @param[in]    pos     the specified bit
  @return True if this bit is set, otherwise false */
  bool test(size_t pos) const {
    ut_ad(pos / 8 < m_size);
    return ((m_bitset[pos / 8] >> (pos & 0x7)) & 0x1);
  }

  /** Get the size of current bitset
  @return the size of the bitset */
  size_t size() const { return (m_size); }

  /** Get the capacity of current bitset
  @return the capacity of the bitset */
  size_t capacity() const { return (m_capacity); }

  /** Get the bitset, don't allow to modify it!
  @return current bitset */
  const byte *bitset() const { return (m_bitset); }

  /** Set current bitset with specified one. Current bitset should have
  called init() to allocate its own bitmap memory which should be big
  enough for the assignment.
  @param[in]    from    set the bitset from this one
  @return current bitset object */
  Bitset &operator=(const Bitset &from) {
    ut_ad(m_capacity >= from.m_size);
    memcpy(m_bitset, from.m_bitset, from.m_size);
    m_size = from.m_size;

    return (*this);
  }

 private:
  /** Bitset bytes */
  byte *m_bitset;

  /** Bitset size in bytes */
  size_t m_size;

  /** Bitset capacity, could be bigger than m_size if one smaller bitmap
  has been assigned to it, after a copy() called */
  size_t m_capacity;
};

#endif
