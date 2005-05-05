/* -*- Mode: C++ -*-

   Copyright (C) 2005 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef BITVECTOR_H
#define BITVECTOR_H

#include <my_global.h>
#include <my_sys.h>
#include <cstring>
#include <climits>

/* Some compile-time checks to ensure the integrity of the implementation. */
#if CHAR_BIT != 8
#  error "This implementation is designed for 8-bit bytes!"
#endif

#ifndef __cplusplus
#  error "This is not a C header file, it's a C++ header file"
#endif

namespace
{
  /* Defining my own swap, since we cannot use std::swap() */
  template <class T>
  inline void my_swap(T& x, T& y)
  { 
    T t(x); x= y; y= t; 
  }
}

/*
  A run-time sized bitvector for storing bits.

  
  CAVEAT

    This class is not designed to be inherited from, please don't do that.

    Right now, the vector cannot change size. It's only used as a replacement
    for using an array of bytes and a counter. If you want to change the size
    of the vector, construct a new bitvector and assign it to the vector,
    e.g.:

      bitvector new_bv(new_size);
      old_bv= new_bv;

    An alternative is to use the swap member function to replace the instance:

      bitvector new_bv(new_size);
      old_bv.swap(new_bv);

    The latter may be more efficient on compilers that are not so good at
    inlining code.
*/

/* Number returned when no bit found */
#define MYSQL_NO_BIT_FOUND 1 << 20
class bitvector
{
private:
  /* Compute the number of bytes required to store 'bits' bits in an array. */
  static inline size_t byte_size(size_t bits)
  { 
    uint const byte_bits = sizeof(byte) * CHAR_BIT;
    return (bits + (byte_bits-1)) / byte_bits; 
  }

  static inline size_t byte_size_word_aligned(size_t bits)
  {
    return ((bits + 31) >> 5) << 2;
  }

  void create_last_word_mask();

  inline void tidy_last_word()
  {
    *last_word_ptr|= last_word_mask;
  }

public:
  bitvector() 
    : m_size(0), m_data(0)
  {
  }

  explicit bitvector(size_t size, bool value= false) 
    : m_size(size),
      m_data((uchar*)my_malloc(byte_size_word_aligned(size), MYF(0)))
  {
    DBUG_ASSERT(size < MYSQL_NO_BIT_FOUND);
    create_last_word_mask();
    if (value)
      set_all();
    else
      clear_all();
  }

  /* Constructor to create a bitvector from data. Observe that 'size' is the
   * number of *bits* in the bitvector. 
   */
  explicit bitvector(byte const* data, size_t size)
    : m_size(size),
      m_data((uchar*)my_malloc(byte_size_word_aligned(size), MYF(0)))
  {
    DBUG_ASSERT(size < MYSQL_NO_BIT_FOUND);
    create_last_word_mask();
    memcpy(m_data, data, byte_size(size));
    tidy_last_word();
  }

  bitvector(bitvector const& other) 
    : m_size(other.size()),
      m_data((uchar*)my_malloc(other.bytes(), MYF(0)))
  {
    DBUG_ASSERT(m_size < MYSQL_NO_BIT_FOUND);
    create_last_word_mask();
    memcpy(m_data, other.data(), other.bytes());
    tidy_last_word();
  }

  ~bitvector() 
  {
    if (m_data)
      my_free((char*)m_data, MYF(0));
  }

  /*
    Allocate memory to the bitvector and create last word mask
    and clear all bits in the bitvector.
  */
  int init(size_t size);

  /* Get number of bits set in the bitvector */
  uint no_bits_set();
  /* Get first bit set/clear in bitvector */
  uint get_first_bit_set();
  uint get_first_bit_clear();
  

  /* Swap the guts of this instance with another instance. */
  void swap(bitvector& other)
  {
    my_swap(m_size, other.m_size);
    my_swap(m_data, other.m_data);
  }

  bitvector &operator=(const bitvector &rhs)
  {
    DBUG_ASSERT(rhs.size() == size());
    memcpy(m_data, rhs.data(), byte_size_word_aligned(m_size));
    return *this;
  }

  bool get_all_bits_set()
  {
    uint32 *data_ptr= (uint32*)&m_data[0];
    for (; data_ptr <= last_word_ptr; data_ptr++)
      if (*data_ptr != 0xFFFFFFFF)
        return FALSE;
    return TRUE;
  }

  bool get_all_bits_clear()
  {
    uint32 *data_ptr= (uint32*)m_data;
    if (*last_word_ptr != last_word_mask)
      return FALSE;
    for (; data_ptr < last_word_ptr; data_ptr++)
      if (*data_ptr)
        return FALSE;
    return TRUE;
  }

  /* A pointer to the bytes representing the bits */
  uchar const *data() const { return m_data; }

  /* The size of the data in *bytes* */
  size_t bytes() const { return byte_size(m_size); }

  /* The number of bits in the bit vector */
  size_t size() const { return m_size; }

  /* Set all bits in the vector */
  void set_all()
  { 
    memset(m_data, 255, byte_size_word_aligned(m_size)); 
  }

  /* Set a bit to a value */
  void set_bit(size_t pos)
  {
    DBUG_ASSERT(pos < m_size);
    m_data[pos>>3]|= (uchar)(1 << (pos & 0x7U));
  }

  /* Reset (clear) all bits in the vector */
  void clear_all()
  { 
    memset(m_data, 0, bytes()); 
    tidy_last_word();
  }

  /* Reset one bit in the vector */
  void clear_bit(size_t pos)
  {
    DBUG_ASSERT(pos < m_size);
    m_data[pos>>3]&= ~(uchar)(1 << (pos & 0x7U));
  }

  void flip_bit(size_t pos)
  {
    DBUG_ASSERT(pos < m_size);
    m_data[pos>>3]^= (uchar)(1 << (pos & 0x7U));
  }

  bool get_bit(size_t pos) const
  {
    DBUG_ASSERT(pos < m_size);
    /*
      !! provides the most effective implementation of conversion to
      bool
    */
    uchar *byte_word= m_data + (pos >> 3);
    uchar mask= 1 << (pos & 0x7U);
    bool ret_value= !!(*byte_word & mask);
    return ret_value;
  };

  bool operator==(bitvector const& rhs) const
  {
    if (size() != rhs.size())
      return false;
    /* This works since I have ensured that the last byte of the array
     * contain sensible data.
     */
    if (memcmp(data(), rhs.data(), bytes()) != 0)
      return false;
    return true;
  }

  bool operator!=(bitvector const& rhs) const
  {
    return !(*this == rhs);
  }

  bitvector &operator&=(bitvector const& rhs)
  {
    DBUG_ASSERT(size() == rhs.size());
    uint32 *data_ptr=(uint32*)data(), *rhs_data_ptr=(uint32*)rhs.data();
    uint32 *last_ptr= last_word_ptr;
    for (; data_ptr <= last_ptr; data_ptr++, rhs_data_ptr++)
      *data_ptr&=*rhs_data_ptr;
    return *this;
  }

  bitvector &operator|=(bitvector const& rhs)
  {
    DBUG_ASSERT(size() == rhs.size());
    uint32 *data_ptr=(uint32*)data(), *rhs_data_ptr=(uint32*)rhs.data();
    uint32 *last_ptr= last_word_ptr;
    for (; data_ptr <= last_ptr; data_ptr++, rhs_data_ptr++)
      *data_ptr|=*rhs_data_ptr;
    return *this;
  }

  bitvector &operator^=(bitvector const& rhs)
  {
    DBUG_ASSERT(size() == rhs.size());
    uint32 *data_ptr=(uint32*)data(), *rhs_data_ptr=(uint32*)rhs.data();
    uint32 *last_ptr= last_word_ptr;
    for (; data_ptr <= last_ptr; data_ptr++, rhs_data_ptr++)
      *data_ptr^=*rhs_data_ptr;
    tidy_last_word();
    return *this;
  }

  bitvector &operator~()
  {
    uint32 *data_ptr= (uint32*)data();
    uint32 *last_ptr= last_word_ptr;
    for (; data_ptr <= last_ptr; data_ptr++)
      *data_ptr^=0xFFFFFFFF;
    tidy_last_word();
    return *this;
  }

private:
  size_t m_size;
  uint32 last_word_mask;
  uchar *m_data;
  uint32 *last_word_ptr;
};

#endif /* BITVECTOR_H */
