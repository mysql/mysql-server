/* Copyright (C) 2003 MySQL AB

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

#ifndef LINEAR_POOL_HPP
#define LINEAR_POOL_HPP

#include <SuperPool.hpp>

/*
 * LinearPool - indexed record pool
 *
 * LinearPool implements a pool where each record has a 0-based index.
 * Any index value (up to 2^32-1) is allowed.  Normal efficient usage is
 * to assign index values in sequence and to re-use any values which
 * have become free.  This is default seize/release behaviour.
 *
 * LinearPool has 2 internal RecordPool instances:
 *
 * (a) record pool of T (the template argument class)
 * (b) record pool of "maps" (array of Uint32)
 *
 * The maps translate an index into an i-value in (a).  Each map has
 * a level.  Level 0 maps point to i-values.  Level N+1 maps point to
 * level N maps.  There is a unique "root map" at top.
 *
 * This works exactly like numbers in a given base.  Each map has base
 * size entries.  For implementation convenience the base must be power
 * of 2 and is given as its log2 value.
 *
 * There is a doubly linked list of available maps (some free entries)
 * on each level.  There is a singly linked free list within each map.
 *
 * Level 0 free entry has space for one record.  Level N free entry
 * implies space for base^N records.  The implied levels are created and
 * removed on demand.  Completely empty maps are removed.
 *
 * Default base is 256 (log2 = 8) which requires maximum 4 levels or
 * "digits" (similar to ip address).
 *
 * TODO
 *
 * - move most of the inline code to LinearPool.cpp
 * - add methods to check / seize user-specified index
 */

#include "SuperPool.hpp"

template <class T, Uint32 LogBase = 8>
class LinearPool {
  typedef SuperPool::PtrI PtrI;

  // Base.
  STATIC_CONST( Base = 1 << LogBase );

  // Digit mask.
  STATIC_CONST( DigitMask = Base - 1 );

  // Max possible levels (0 to max root level).
  STATIC_CONST( MaxLevels = (32 + LogBase - 1) / LogBase );

  // Map.
  struct Map {
    Uint32 m_level;
    Uint32 m_occup;     // number of used entries
    Uint32 m_firstfree; // position of first free entry
    PtrI m_parent;      // parent map
    Uint32 m_index;     // from root to here
    PtrI m_nextavail;
    PtrI m_prevavail;
    PtrI m_entry[Base];
  };

public:

  // Constructor.
  LinearPool(GroupPool& gp);

  // Destructor.
  ~LinearPool();

  // Update pointer ptr.p according to index value ptr.i.
  void getPtr(Ptr<T>& ptr);

  // Allocate record from the pool.  Reuses free index if possible.
  bool seize(Ptr<T>& ptr);

  // Return record to the pool.
  void release(Ptr<T>& ptr);

  // Verify (debugging).
  void verify();

private:

  // Given index find the bottom map.
  void get_map(Ptr<Map>& map_ptr, Uint32 index);

  // Add new root map and increase level
  bool add_root();

  // Add new non-root map.
  bool add_map(Ptr<Map>& map_ptr, Ptr<Map> parent_ptr, Uint32 digit);

  // Remove entry and map if it becomes empty.
  void remove_entry(Ptr<Map> map_ptr, Uint32 digit);

  // Remove map and all parents which become empty.
  void remove_map(Ptr<Map> map_ptr);

  // Add map to available list.
  void add_avail(Ptr<Map> map_ptr);

  // Remove map from available list.
  void remove_avail(Ptr<Map> map_ptr);

  // Verify map (recursive).
  void verify(Ptr<Map> map_ptr, Uint32 level);

  RecordPool<T> m_records;
  RecordPool<Map> m_maps;
  Uint32 m_levels;              // 0 means empty pool
  PtrI m_root;
  PtrI m_avail[MaxLevels];
};

template <class T, Uint32 LogBase>
inline
LinearPool<T, LogBase>::LinearPool(GroupPool& gp) :
  m_records(gp),
  m_maps(gp),
  m_levels(0),
  m_root(RNIL)
{
  Uint32 n;
  for (n = 0; n < MaxLevels; n++)
    m_avail[n] = RNIL;
}

template <class T, Uint32 LogBase>
inline
LinearPool<T, LogBase>::~LinearPool()
{
}

template <class T, Uint32 LogBase>
inline void
LinearPool<T, LogBase>::getPtr(Ptr<T>& ptr)
{
  Uint32 index = ptr.i;
  // get level 0 map
  Ptr<Map> map_ptr;
  get_map(map_ptr, index);
  // get record
  Ptr<T> rec_ptr;
  Uint32 digit = index & DigitMask;
  rec_ptr.i = map_ptr.p->m_entry[digit];
  m_records.getPtr(rec_ptr);
  ptr.p = rec_ptr.p;
}

template <class T, Uint32 LogBase>
inline bool
LinearPool<T, LogBase>::seize(Ptr<T>& ptr)
{
  // look for free list on some level
  Ptr<Map> map_ptr;
  map_ptr.i = RNIL;
  Uint32 n = 0;
  while (n < m_levels) {
    if ((map_ptr.i = m_avail[n]) != RNIL)
      break;
    n++;
  }
  if (map_ptr.i == RNIL) {
    // add new level with available maps
    if (! add_root())
      return false;
    assert(n < m_levels);
    map_ptr.i = m_avail[n];
  }
  m_maps.getPtr(map_ptr);
  // walk down creating missing levels and using an entry on each
  Uint32 firstfree;
  while (true) {
    assert(map_ptr.p->m_occup < Base);
    map_ptr.p->m_occup++;
    firstfree = map_ptr.p->m_firstfree;
    assert(firstfree < Base);
    map_ptr.p->m_firstfree = map_ptr.p->m_entry[firstfree];
    if (map_ptr.p->m_occup == Base) {
      assert(map_ptr.p->m_firstfree == Base);
      // remove from available list
      remove_avail(map_ptr);
    }
    if (n == 0)
      break;
    Ptr<Map> child_ptr;
    if (! add_map(child_ptr, map_ptr, firstfree)) {
      remove_entry(map_ptr, firstfree);
      return false;
    }
    map_ptr.p->m_entry[firstfree] = child_ptr.i;
    map_ptr = child_ptr;
    n--;
  }
  // now on level 0
  assert(map_ptr.p->m_level == 0);
  Ptr<T> rec_ptr;
  if (! m_records.seize(rec_ptr)) {
    remove_entry(map_ptr, firstfree);
    return false;
  }
  map_ptr.p->m_entry[firstfree] = rec_ptr.i;
  ptr.i = firstfree + (map_ptr.p->m_index << LogBase);
  ptr.p = rec_ptr.p;
  return true;
}

template <class T, Uint32 LogBase>
inline void
LinearPool<T, LogBase>::release(Ptr<T>& ptr)
{
  Uint32 index = ptr.i;
  // get level 0 map
  Ptr<Map> map_ptr;
  get_map(map_ptr, index);
  // release record
  Ptr<T> rec_ptr;
  Uint32 digit = index & DigitMask;
  rec_ptr.i = map_ptr.p->m_entry[digit];
  m_records.release(rec_ptr);
  // remove entry
  remove_entry(map_ptr, digit);
  // null pointer
  ptr.i = RNIL;
  ptr.p = 0;
}

template <class T, Uint32 LogBase>
inline void
LinearPool<T, LogBase>::verify()
{
  if (m_root == RNIL)
    return;
  assert(m_levels != 0);
  Ptr<Map> map_ptr;
  map_ptr.i = m_root;
  m_maps.getPtr(map_ptr);
  verify(map_ptr, m_levels - 1);
}

// private methods

template <class T, Uint32 LogBase>
inline void
LinearPool<T, LogBase>::get_map(Ptr<Map>& map_ptr, Uint32 index)
{
  // root map must exist
  Ptr<Map> tmp_ptr;
  tmp_ptr.i = m_root;
  m_maps.getPtr(tmp_ptr);
  assert(tmp_ptr.p->m_level + 1 == m_levels);
  // extract index digits up to current root level
  Uint32 digits[MaxLevels];
  Uint32 n = 0;
  do {
    digits[n] = index & DigitMask;
    index >>= LogBase;
  } while (++n < m_levels);
  assert(index == 0);
  // walk down indirect levels
  while (--n > 0) {
    tmp_ptr.i = tmp_ptr.p->m_entry[digits[n]];
    m_maps.getPtr(tmp_ptr);
  }
  // level 0 map
  assert(tmp_ptr.p->m_level == 0);
  map_ptr = tmp_ptr;
}

template <class T, Uint32 LogBase>
inline bool
LinearPool<T, LogBase>::add_root()
{
  // new root
  Ptr<Map> map_ptr;
  if (! m_maps.seize(map_ptr))
    return false;
  Uint32 n = m_levels++;
  assert(n < MaxLevels);
  // set up
  map_ptr.p->m_level = n;
  if (n == 0) {
    map_ptr.p->m_occup = 0;
    map_ptr.p->m_firstfree = 0;
  } else {
    // on level > 0 digit 0 points to old root
    map_ptr.p->m_occup = 1;
    map_ptr.p->m_firstfree = 1;
    Ptr<Map> old_ptr;
    old_ptr.i = m_root;
    m_maps.getPtr(old_ptr);
    assert(old_ptr.p->m_parent == RNIL);
    old_ptr.p->m_parent = map_ptr.i;
    map_ptr.p->m_entry[0] = old_ptr.i;
  }
  // set up free list with Base as terminator
  for (Uint32 j = map_ptr.p->m_firstfree; j < Base; j++)
    map_ptr.p->m_entry[j] = j + 1;
  map_ptr.p->m_parent = RNIL;
  map_ptr.p->m_index = 0;
  add_avail(map_ptr);
  // set new root
  m_root = map_ptr.i;
  return true;
}

template <class T, Uint32 LogBase>
inline bool
LinearPool<T, LogBase>::add_map(Ptr<Map>& map_ptr, Ptr<Map> parent_ptr, Uint32 digit)
{
  if (! m_maps.seize(map_ptr))
    return false;
  assert(parent_ptr.p->m_level != 0);
  // set up
  map_ptr.p->m_level = parent_ptr.p->m_level - 1;
  map_ptr.p->m_occup = 0;
  map_ptr.p->m_firstfree = 0;
  // set up free list with Base as terminator
  for (Uint32 j = map_ptr.p->m_firstfree; j < Base; j++)
    map_ptr.p->m_entry[j] = j + 1;
  map_ptr.p->m_parent = parent_ptr.i;
  map_ptr.p->m_index = digit + (parent_ptr.p->m_index << LogBase);
  add_avail(map_ptr);
  return true;
}

template <class T, Uint32 LogBase>
inline void
LinearPool<T, LogBase>::remove_entry(Ptr<Map> map_ptr, Uint32 digit)
{
  assert(map_ptr.p->m_occup != 0 && digit < Base);
  map_ptr.p->m_occup--;
  map_ptr.p->m_entry[digit] = map_ptr.p->m_firstfree;
  map_ptr.p->m_firstfree = digit;
  if (map_ptr.p->m_occup + 1 == Base)
    add_avail(map_ptr);
  else if (map_ptr.p->m_occup == 0)
    remove_map(map_ptr);
}

template <class T, Uint32 LogBase>
inline void
LinearPool<T, LogBase>::remove_map(Ptr<Map> map_ptr)
{
  assert(map_ptr.p->m_occup == 0);
  remove_avail(map_ptr);
  Ptr<Map> parent_ptr;
  parent_ptr.i = map_ptr.p->m_parent;
  Uint32 digit = map_ptr.p->m_index & DigitMask;
  PtrI map_ptr_i = map_ptr.i;
  m_maps.release(map_ptr);
  if (m_root == map_ptr_i) {
    assert(parent_ptr.i == RNIL);
    Uint32 used = m_maps.m_superPool.getRecUseCount(m_maps.m_recInfo);
    assert(used == 0);
    m_root = RNIL;
    m_levels = 0;
  }
  if (parent_ptr.i != RNIL) {
    m_maps.getPtr(parent_ptr);
    // remove child entry (recursive)
    remove_entry(parent_ptr, digit);
  }
}

template <class T, Uint32 LogBase>
inline void
LinearPool<T, LogBase>::add_avail(Ptr<Map> map_ptr)
{
  Uint32 n = map_ptr.p->m_level;
  assert(n < m_levels);
  map_ptr.p->m_nextavail = m_avail[n];
  if (map_ptr.p->m_nextavail != RNIL) {
    Ptr<Map> next_ptr;
    next_ptr.i = map_ptr.p->m_nextavail;
    m_maps.getPtr(next_ptr);
    next_ptr.p->m_prevavail = map_ptr.i;
  }
  map_ptr.p->m_prevavail = RNIL;
  m_avail[n] = map_ptr.i;
}

template <class T, Uint32 LogBase>
inline void
LinearPool<T, LogBase>::remove_avail(Ptr<Map> map_ptr)
{
  Uint32 n = map_ptr.p->m_level;
  assert(n < m_levels);
  if (map_ptr.p->m_nextavail != RNIL) {
    Ptr<Map> next_ptr;
    next_ptr.i = map_ptr.p->m_nextavail;
    m_maps.getPtr(next_ptr);
    next_ptr.p->m_prevavail = map_ptr.p->m_prevavail;
  }
  if (map_ptr.p->m_prevavail != RNIL) {
    Ptr<Map> prev_ptr;
    prev_ptr.i = map_ptr.p->m_prevavail;
    m_maps.getPtr(prev_ptr);
    prev_ptr.p->m_nextavail = map_ptr.p->m_nextavail;
  }
  if (map_ptr.p->m_prevavail == RNIL) {
    m_avail[n] = map_ptr.p->m_nextavail;
  }
  map_ptr.p->m_nextavail = RNIL;
  map_ptr.p->m_prevavail = RNIL;
}

template <class T, Uint32 LogBase>
inline void
LinearPool<T, LogBase>::verify(Ptr<Map> map_ptr, Uint32 level)
{
  assert(level < MaxLevels);
  assert(map_ptr.p->m_level == level);
  Uint32 j = 0;
  while (j < Base) {
    bool free = false;
    Uint32 j2 = map_ptr.p->m_firstfree;
    while (j2 != Base) {
      if (j2 == j) {
        free = true;
        break;
      }
      assert(j2 < Base);
      j2 = map_ptr.p->m_entry[j2];
    }
    if (! free) {
      if (level != 0) {
        Ptr<Map> child_ptr;
        child_ptr.i = map_ptr.p->m_entry[j];
        m_maps.getPtr(child_ptr);
        assert(child_ptr.p->m_parent == map_ptr.i);
        assert(child_ptr.p->m_index == j + (map_ptr.p->m_index << LogBase));
        verify(child_ptr, level - 1);
      } else {
        Ptr<T> rec_ptr;
        rec_ptr.i = map_ptr.p->m_entry[j];
        m_records.getPtr(rec_ptr);
      }
      Ptr<Map> avail_ptr;
      avail_ptr.i = m_avail[map_ptr.p->m_level];
      bool found = false;
      while (avail_ptr.i != RNIL) {
        if (avail_ptr.i == map_ptr.i) {
          found = true;
          break;
        }
        m_maps.getPtr(avail_ptr);
        avail_ptr.i = avail_ptr.p->m_nextavail;
      }
      assert(found == (map_ptr.p->m_occup < Base));
    }
    j++;
  }
}

#endif
