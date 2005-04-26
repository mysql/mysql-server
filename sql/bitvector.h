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

class bitvector
{
private:
  /* Helper classes */
  struct flip_bit_op
  { 
    void operator()(byte* p, byte m) { *p^= m; }
  };

  struct set_bit_op
  { 
    void operator()(byte* p, byte m) { *p|= m; }
  };

  struct clear_bit_op
  { 
    void operator()(byte* p, byte m) { *p&= ~m; }
  };

  struct test_bit_op
  { 
    bool operator()(byte* p, byte m) { return *p & m; }
  };

  /* Compute the number of bytes required to store 'bits' bits in an array. */
  static inline size_t byte_size(size_t bits)
  { 
    int const byte_bits = sizeof(byte) * CHAR_BIT;
    return (bits + (byte_bits-1)) / byte_bits; 
  }

  /* Tidy the last byte (by clearing the unused bits) of the bitvector to make
   * comparison easy.  This code is assuming that we're working with 8-bit
   * bytes.
   */
  void tidy_last_byte()
  {
    byte* const last_byte= m_data + bytes() - 1;

    /* Get the number of used bits (1..8) in the last byte */
    unsigned int const used= 1U + ((size()-1U) & 0x7U);

    /* Create a mask with the upper 'unused' bits clear and the lower 'used'
     * bits set. The bits within each byte is stored in big-endian order.
     */
    unsigned int const mask= ((1 << used) - 1);

    /* Mask the last byte */
    *last_byte&= mask;
  }

  template <class ReturnType, class Func>
  inline ReturnType apply_to_byte(size_t const pos, Func op) const
  {
    /* Here I'm assuming that we're working with 8-bit bytes. */
    ptrdiff_t const byte_pos= pos >> 3;
    byte const mask= (1 << (pos & 0x7U));
    return op(&m_data[byte_pos], mask);
  }

public:
  bitvector() 
    : m_size(0), m_data(0)
  {
  }

  explicit bitvector(size_t size, bool value= false) 
    : m_size(size), m_data(my_malloc(byte_size(size), MYF(0)))
  {
    if (value)
      set_all();
    else
      clear_all();
  }

  /* Constructor to create a bitvector from data. Observe that 'size' is the
   * number of *bits* in the bitvector. 
   */
  explicit bitvector(byte const* data, size_t size)
    : m_size(size), m_data(my_malloc(byte_size(size), MYF(0)))
  {
    /* std::copy(data, data + byte_size(size), m_data); */
    memcpy(m_data, data, byte_size(size));
    tidy_last_byte();
  }

  bitvector(bitvector const& other) 
    : m_size(other.size()), m_data(my_malloc(other.bytes(), MYF(0)))
  {
    /* std::copy(other.m_data, other.m_data + other.bytes(), m_data); */
    memcpy(m_data, other.data(), other.bytes());
    tidy_last_byte();           /* Just a precaution */
  }

  /* Assignment operator */
  bitvector& operator=(bitvector other)
  {
    swap(other);
    return *this;
  }

  ~bitvector() 
  {
    if (m_data)
      my_free(m_data, MYF(0));
  }

  /* Swap the guts of this instance with another instance. */
  void swap(bitvector& other)
  {
    my_swap(m_size, other.m_size);
    my_swap(m_data, other.m_data);
  }

  /* A pointer to the bytes representing the bits */
  byte const *data() const { return m_data; }

  /* The size of the data in *bytes* */
  size_t bytes() const { return byte_size(m_size); }

  /* The number of bits in the bit vector */
  size_t size() const { return m_size; }

  /* Set all bits in the vector */
  void set_all()
  { 
    /* std::fill_n(m_data, bytes(), 255); */
    memset(m_data, 255, bytes()); 
    tidy_last_byte();
  }

  /* Set a bit to a value */
  void set_bit(size_t pos)
  {
    apply_to_byte<void>(pos, set_bit_op());
  }

  /* Reset (clear) all bits in the vector */
  void clear_all()
  { 
    /* std::fill_n(m_data, bytes(), 0); */
    memset(m_data, 0, bytes()); 
    tidy_last_byte();
  }

  /* Reset one bit in the vector */
  void clear_bit(size_t pos)
  {
    apply_to_byte<void>(pos, clear_bit_op());
  }

  void flip_bit(size_t pos)
  {
    apply_to_byte<void>(pos, flip_bit_op());
  }

  bool get_bit(size_t pos) const
  {
    return apply_to_byte<bool>(pos, test_bit_op());
  };

  bool operator==(bitvector const& rhs) const
  {
    if (size() != rhs.size())
      return false;
    /* This works since I have ensured that the last byte of the array contain
     * sensible data.
     */
    if (memcmp(data(), rhs.data(), bytes()) != 0)
      return false;
    return true;
  }

  bool operator!=(bitvector const& rhs) const
  {
    return !(*this == rhs);
  }

private:
  size_t m_size;
  byte *m_data;
};

#endif /* BITVECTOR_H */
