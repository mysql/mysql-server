/*
   Copyright (C) 2005, 2006 MySQL AB
    All rights reserved. Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef LINEAR_POOL_HPP
#define LINEAR_POOL_HPP

#include <Bitmask.hpp>
#include "SuperPool.hpp"

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
 * of 2 between 2^1 and 2^15.  It is given by its log2 value (1-15).
 *
 * A position in a map is also called a "digit".
 *
 * There is a doubly linked list of available maps (some free entries)
 * on each level.  There is a doubly linked freelist within each map.
 * There is also a bitmask of used entries in each map.
 *
 * Level 0 free entry has space for one record.  Level N free entry
 * implies space for base^N records.  The implied levels are created and
 * removed on demand.  Empty maps are usually removed.
 *
 * Default base is 256 (log2 = 8) which requires maximum 4 levels or
 * digits (similar to ip address).
 *
 * TODO
 *
 * - move most of the inline code to LinearPool.cpp
 * - optimize for common case
 * - add optimized 2-level implementation (?)
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

  // Number of words in map used bit mask.
  STATIC_CONST( BitmaskSize = (Base + 31) / 32 );

  // Map.
  struct Map {
    Uint32 m_level;
    Uint32 m_occup;     // number of used entries
    Uint32 m_firstfree; // position of first free entry
    PtrI m_parent;      // parent map
    Uint32 m_index;     // from root to here
    PtrI m_nextavail;
    PtrI m_prevavail;
    Uint32 m_bitmask[BitmaskSize];
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

  // Allocate given index.  Like seize but returns -1 if in use.
  int seize_index(Ptr<T>& ptr, Uint32 index);

  // Return record to the pool.
  void release(Ptr<T>& ptr);

  // Return number of used records (may require 1 page scan).
  Uint32 count();

  // Verify (debugging).
  void verify();

private:

  // Given index find the bottom map.
  void get_map(Ptr<Map>& map_ptr, Uint32 index);

  // Add new root map and increase level
  bool add_root();

  // Add new non-root map.
  bool add_map(Ptr<Map>& map_ptr, Ptr<Map> parent_ptr, Uint32 digit);

  // Subroutine to initialize map free lists.
  void init_free(Ptr<Map> map_ptr);

  // Add entry at given free position.
  void add_entry(Ptr<Map> map_ptr, Uint32 digit, PtrI ptr_i);

  // Remove entry and map if it becomes empty.
  void remove_entry(Ptr<Map> map_ptr, Uint32 digit);

  // Remove map and all parents which become empty.
  void remove_map(Ptr<Map> map_ptr);

  // Add map to available list.
  void add_avail(Ptr<Map> map_ptr);

  // Remove map from available list.
  void remove_avail(Ptr<Map> map_ptr);

  // Verify available lists
  void verify_avail();

  // Verify map (recursive).
  void verify_map(Ptr<Map> map_ptr, Uint32 level, Uint32* count);

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
  assert(BitmaskImpl::get(BitmaskSize, map_ptr.p->m_bitmask, digit));
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
  Uint32 digit;
  Ptr<Map> new_ptr;
  new_ptr.i = RNIL;
  while (true) {
    digit = map_ptr.p->m_firstfree;
    if (n == 0)
      break;
    Ptr<Map> child_ptr;
    if (! add_map(child_ptr, map_ptr, digit)) {
      if (new_ptr.i != RNIL)
        remove_map(new_ptr);
      return false;
    }
    new_ptr = child_ptr;
    map_ptr = child_ptr;
    n--;
  }
  // now on level 0
  assert(map_ptr.p->m_level == 0);
  Ptr<T> rec_ptr;
  if (! m_records.seize(rec_ptr)) {
    if (new_ptr.i != RNIL)
      remove_map(new_ptr);
    return false;
  }
  add_entry(map_ptr, digit, rec_ptr.i);
  ptr.i = digit + (map_ptr.p->m_index << LogBase);
  ptr.p = rec_ptr.p;
  return true;
}

template <class T, Uint32 LogBase>
inline int
LinearPool<T, LogBase>::seize_index(Ptr<T>& ptr, Uint32 index)
{
  // extract all digits at least up to current root level
  Uint32 digits[MaxLevels];
  Uint32 n = 0;
  Uint32 tmp = index;
  do {
    digits[n] = tmp & DigitMask;
    tmp >>= LogBase;
  } while (++n < m_levels || tmp != 0);
  // add any new root levels
  while (n > m_levels) {
    if (! add_root())
      return false;
  }
  // start from root
  Ptr<Map> map_ptr;
  map_ptr.i = m_root;
  m_maps.getPtr(map_ptr);
  // walk down creating or re-using existing levels
  Uint32 digit;
  bool used;
  Ptr<Map> new_ptr;
  new_ptr.i = RNIL;
  while (true) {
    digit = digits[--n];
    used = BitmaskImpl::get(BitmaskSize, map_ptr.p->m_bitmask, digit);
    if (n == 0)
      break;
    if (used) {
      map_ptr.i = map_ptr.p->m_entry[digit];
      m_maps.getPtr(map_ptr);
    } else {
      Ptr<Map> child_ptr;
      if (! add_map(child_ptr, map_ptr, digit)) {
        if (new_ptr.i != RNIL)
          remove_map(new_ptr);
      }
      new_ptr = child_ptr;
      map_ptr = child_ptr;
    }
  }
  // now at level 0
  assert(map_ptr.p->m_level == 0);
  Ptr<T> rec_ptr;
  if (used || ! m_records.seize(rec_ptr)) {
    if (new_ptr.i != RNIL)
      remove_map(new_ptr);
    return used ? -1 : false;
  }
  add_entry(map_ptr, digit, rec_ptr.i);
  assert(index == digit + (map_ptr.p->m_index << LogBase));
  ptr.i = index;
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
inline Uint32
LinearPool<T, LogBase>::count()
{
  SuperPool& sp = m_records.m_superPool;
  Uint32 count1 = sp.getRecUseCount(m_records.m_recInfo);
  return count1;
}

template <class T, Uint32 LogBase>
inline void
LinearPool<T, LogBase>::verify()
{
  verify_avail();
  if (m_root == RNIL) {
    assert(m_levels == 0);
    return;
  }
  assert(m_levels != 0);
  Ptr<Map> map_ptr;
  map_ptr.i = m_root;
  m_maps.getPtr(map_ptr);
  Uint32 count1 = count();
  Uint32 count2 = 0;
  verify_map(map_ptr, m_levels - 1, &count2);
  assert(count1 == count2);
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
  map_ptr.p->m_parent = RNIL;
  map_ptr.p->m_index = 0;
  init_free(map_ptr);
  // on level > 0 digit 0 points to old root
  if (n > 0) {
    Ptr<Map> old_ptr;
    old_ptr.i = m_root;
    m_maps.getPtr(old_ptr);
    assert(old_ptr.p->m_parent == RNIL);
    old_ptr.p->m_parent = map_ptr.i;
    add_entry(map_ptr, 0, old_ptr.i);
  }
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
  map_ptr.p->m_parent = parent_ptr.i;
  map_ptr.p->m_index = digit + (parent_ptr.p->m_index << LogBase);
  init_free(map_ptr);
  add_entry(parent_ptr, digit, map_ptr.i);
  return true;
}

template <class T, Uint32 LogBase>
inline void
LinearPool<T, LogBase>::init_free(Ptr<Map> map_ptr)
{
  map_ptr.p->m_occup = 0;
  map_ptr.p->m_firstfree = 0;
  // freelist
  Uint32 j;
  Uint16 back = ZNIL;
  for (j = 0; j < Base - 1; j++) {
    map_ptr.p->m_entry[j] = back | ((j + 1) << 16);
    back = j;
  }
  map_ptr.p->m_entry[j] = back | (ZNIL << 16);
  // bitmask
  BitmaskImpl::clear(BitmaskSize, map_ptr.p->m_bitmask);
  // add to available
  add_avail(map_ptr);
}

template <class T, Uint32 LogBase>
inline void
LinearPool<T, LogBase>::add_entry(Ptr<Map> map_ptr, Uint32 digit, PtrI ptr_i)
{
  assert(map_ptr.p->m_occup < Base && digit < Base);
  assert(! BitmaskImpl::get(BitmaskSize, map_ptr.p->m_bitmask, digit));
  // unlink from freelist
  Uint32 val = map_ptr.p->m_entry[digit];
  Uint16 back = val & ZNIL;
  Uint16 forw = val >> 16;
  if (back != ZNIL) {
    assert(back < Base);
    map_ptr.p->m_entry[back] &= ZNIL;
    map_ptr.p->m_entry[back] |= (forw << 16);
  }
  if (forw != ZNIL) {
    assert(forw < Base);
    map_ptr.p->m_entry[forw] &= (ZNIL << 16);
    map_ptr.p->m_entry[forw] |= back;
  }
  if (back == ZNIL) {
    map_ptr.p->m_firstfree = forw;
  }
  // set new value
  map_ptr.p->m_entry[digit] = ptr_i;
  map_ptr.p->m_occup++;
  BitmaskImpl::set(BitmaskSize, map_ptr.p->m_bitmask, digit);
  if (map_ptr.p->m_occup == Base)
    remove_avail(map_ptr);
}

template <class T, Uint32 LogBase>
inline void
LinearPool<T, LogBase>::remove_entry(Ptr<Map> map_ptr, Uint32 digit)
{
  assert(map_ptr.p->m_occup != 0 && digit < Base);
  assert(BitmaskImpl::get(BitmaskSize, map_ptr.p->m_bitmask, digit));
  // add to freelist
  Uint32 firstfree = map_ptr.p->m_firstfree;
  map_ptr.p->m_entry[digit] = ZNIL | (firstfree << 16);
  if (firstfree != ZNIL) {
    assert(firstfree < Base);
    map_ptr.p->m_entry[firstfree] &= (ZNIL << 16);
    map_ptr.p->m_entry[firstfree] |= digit;
  }
  map_ptr.p->m_firstfree = digit;
  map_ptr.p->m_occup--;
  BitmaskImpl::clear(BitmaskSize, map_ptr.p->m_bitmask, digit);
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
    Uint32 used = count();
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
LinearPool<T, LogBase>::verify_avail()
{
  // check available lists
  for (Uint32 n = 0; n < MaxLevels; n++) {
    Ptr<Map> map_ptr;
    map_ptr.i = m_avail[n];
    Uint32 back = RNIL;
    while (map_ptr.i != RNIL) {
      m_maps.getPtr(map_ptr);
      assert(map_ptr.p->m_occup < Base);
      assert(back == map_ptr.p->m_prevavail);
      back = map_ptr.i;
      map_ptr.i = map_ptr.p->m_nextavail;
    }
  }
}

template <class T, Uint32 LogBase>
inline void
LinearPool<T, LogBase>::verify_map(Ptr<Map> map_ptr, Uint32 level, Uint32* count)
{
  assert(level < MaxLevels);
  assert(map_ptr.p->m_level == level);
  // check freelist
  {
    Uint32 nused = BitmaskImpl::count(BitmaskSize, map_ptr.p->m_bitmask);
    assert(nused <= Base);
    assert(map_ptr.p->m_occup == nused);
    Uint32 nfree = 0;
    Uint32 j = map_ptr.p->m_firstfree;
    Uint16 back = ZNIL;
    while (j != ZNIL) {
      assert(j < Base);
      assert(! BitmaskImpl::get(BitmaskSize, map_ptr.p->m_bitmask, j));
      Uint32 val = map_ptr.p->m_entry[j];
      assert(back == (val & ZNIL));
      back = j;
      j = (val >> 16);
      nfree++;
    }
    assert(nused + nfree == Base);
  }
  // check entries
  {
    for (Uint32 j = 0; j < Base; j++) {
      bool free = ! BitmaskImpl::get(BitmaskSize, map_ptr.p->m_bitmask, j);
      if (free)
        continue;
      if (level != 0) {
        Ptr<Map> child_ptr;
        child_ptr.i = map_ptr.p->m_entry[j];
        m_maps.getPtr(child_ptr);
        assert(child_ptr.p->m_parent == map_ptr.i);
        assert(child_ptr.p->m_index == j + (map_ptr.p->m_index << LogBase));
        verify_map(child_ptr, level - 1, count);
      } else {
        Ptr<T> rec_ptr;
        rec_ptr.i = map_ptr.p->m_entry[j];
        m_records.getPtr(rec_ptr);
        (*count)++;
      }
    }
  }
  // check membership on available list
  {
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
}

#endif
