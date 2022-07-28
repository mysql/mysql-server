/*
   Copyright (c) 2010, 2022, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_SPARSE_BITMASK_H
#define NDB_SPARSE_BITMASK_H

#include <ndb_global.h>
#include <util/Vector.hpp>
#include <util/BaseString.hpp>

class SparseBitmask {
  unsigned m_max_size;
  Vector<unsigned> m_vec;

public:
  static constexpr Uint32 NotFound = (unsigned)-1;

  SparseBitmask(unsigned max_size = NotFound - 1) : m_max_size(max_size) {}

  unsigned max_size() const { return m_max_size; }

  /* Set bit n */
  void set(unsigned n) {
    assert(n <= m_max_size);

    unsigned i = m_vec.size();
    while (i > 0)
    {
      const unsigned j = m_vec[i-1];
      if (n == j)
        return; // Bit n already set

      if (j < n)
        break;
      i--;
    }

    m_vec.push(n, i);
  }

  /* Get bit n */
  bool get(unsigned n) const {
    assert(n <= m_max_size);

    for (unsigned i = 0; i < m_vec.size(); i++)
    {
      const unsigned j = m_vec[i];
      if (j < n)
        continue;

      return (j == n);
    }
    return false;
  }

  /* Clear bit n */
  int clear(unsigned n) {
    assert(n <= m_max_size);
    for (unsigned i = 0; i < m_vec.size(); i++)
    {
      const unsigned j = m_vec[i];
      if (j != n)
        continue;

      m_vec.erase(i);
      return 1;
    }
    return 0;
  }

  /* Clear all bits */
  void clear(void) {
    m_vec.clear();
  }

  /* Find first bit >= n */
  unsigned find(unsigned n) const {
    for (unsigned i = 0; i < m_vec.size(); i++)
    {
      const unsigned j = m_vec[i];
      if (j >= n)
        return j;
    }
    return NotFound;
  }

  /* Number of bits set */
  unsigned count() const { return m_vec.size(); }

  bool isclear() const { return count() == 0; }

  unsigned getBitNo(unsigned n) const {
    assert(n < m_vec.size());
    return m_vec[n];
  }

  void print(void) const {
    for (unsigned i = 0; i < m_vec.size(); i++)
    {
      const unsigned j = m_vec[i];
      printf("[%u]: %u\n", i, j);
    }
  }

  bool equal(const SparseBitmask& obj) const {
    if (obj.count() != count())
      return false;

    for (unsigned i = 0; i<count(); i++)
      if (!obj.get(m_vec[i]))
        return false;

    return true;
  }

  bool overlaps(const SparseBitmask& obj) const {
    for (unsigned i = 0; i<count(); i++)
      if (obj.get(m_vec[i]))
        return true;

    for (unsigned i = 0; i<obj.count(); i++)
      if (get(obj.getBitNo(i)))
        return true;
    return false;
  }

  /**
   * Bitwise OR the content of the passed bitmask
   * into our bitmask
   */
  void bitOR(const SparseBitmask& obj)
  {
    Vector<unsigned> result;
    
    unsigned my_idx = 0;
    unsigned obj_idx = 0;
    bool done = false;

    /* Merge source + obj -> result in bit order */
    while (!done)
    {
      if (my_idx < count())
      {
        const unsigned next_from_me = m_vec[my_idx];
        
        if (obj_idx < obj.count())
        {
          /* Set lowest bit from either bitmask */
          const unsigned next_from_obj = obj.m_vec[obj_idx];
          
          if (next_from_me == next_from_obj)
          {
            /* Same bit set in both */
            result.push_back(next_from_me);
            my_idx++;
            obj_idx++;
          }
          else if (next_from_me < next_from_obj)
          {
            result.push_back(next_from_me);
            my_idx++;
         }
          else
          {
            result.push_back(next_from_obj);
            obj_idx++;
          }
        }
        else
        {
          /* Finished with bits set in obj */
          result.push_back(next_from_me);
          my_idx++;
        }
      }
      else if (obj_idx < obj.count())
      {
        /* Finished with my bits */
        result.push_back(obj.m_vec[obj_idx]);
        obj_idx++;
      }
      else
      {
        done = true;
      }
    }
    /* Overwrite my bitmask with the new value */
    m_vec.assign(result);
  }
      
  BaseString str() const {
    BaseString tmp;
    const char* sep="";
    for (unsigned i = 0; i<m_vec.size(); i++)
    {
      tmp.appfmt("%s%u", sep, m_vec[i]);
      sep=",";
    }
    return tmp;
  }
};

#endif
