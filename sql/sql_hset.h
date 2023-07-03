#ifndef SQL_HSET_INCLUDED
#define SQL_HSET_INCLUDED
/* Copyright (c) 2010, 2023, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

#include "my_global.h"
#include "hash.h"


/**
  A type-safe wrapper around mysys HASH.
*/

template <typename T, my_hash_get_key K>
class Hash_set
{
public:
  typedef T Value_type;
  enum { START_SIZE= 8 };
  /**
    Constructs an empty hash. Does not allocate memory, it is done upon
    the first insert. Thus does not cause or return errors.
  */
  Hash_set(PSI_memory_key psi_key)
  {
    m_psi_key= psi_key;
    my_hash_clear(&m_hash);
  }
  /**
    Destroy the hash by freeing the buckets table. Does
    not call destructors for the elements.
  */
  ~Hash_set()
  {
    my_hash_free(&m_hash);
  }
  /**
    Insert a single value into a hash. Does not tell whether
    the value was inserted -- if an identical value existed,
    it is not replaced.

    @retval TRUE  Out of memory.
    @retval FALSE OK. The value either was inserted or existed
                  in the hash.
  */
  bool insert(T *value)
  {
    my_hash_init_opt(&m_hash, &my_charset_bin, START_SIZE, 0, 0, K, 0, MYF(0),
                     m_psi_key);
    size_t key_len;
    const uchar *key= K(reinterpret_cast<uchar*>(value), &key_len, FALSE);
    if (my_hash_search(&m_hash, key, key_len) == NULL)
      return my_hash_insert(&m_hash, reinterpret_cast<uchar *>(value));
    return FALSE;
  }
  /** Is this hash set empty? */
  bool is_empty() const { return m_hash.records == 0; }
  /** Returns the number of unique elements. */
  size_t size() const { return static_cast<size_t>(m_hash.records); }
  /** An iterator over hash elements. Is not insert-stable. */
  class Iterator
  {
  public:
    Iterator(Hash_set &hash_set)
      : m_hash(&hash_set.m_hash),
        m_idx(0)
    {}
    /**
      Return the current element and reposition the iterator to the next
      element.
    */
    inline T *operator++(int)
    {
      if (m_idx < m_hash->records)
        return reinterpret_cast<T*>(my_hash_element(m_hash, m_idx++));
      return NULL;
    }
    void rewind() { m_idx= 0; }
  private:
    HASH *m_hash;
    uint m_idx;
  };
private:
  HASH m_hash;
  PSI_memory_key m_psi_key;
};

#endif // SQL_HSET_INCLUDED
