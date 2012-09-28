/*
   Copyright (c) 2012 Oracle and/or its affiliates. All rights reserved.

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

#ifndef NDB_LHLEVEL_H
#define NDB_LHLEVEL_H

/**
 * LHLevel keeps level information for linear hashing,
 **/

/**
 * Supports up to UINT32_MAX number of bucket addresses.
 * If support for more is needed, one also have to
 * increase hash key size.
 **/

#include <assert.h>
#include "Bitmask.hpp"

class LHLevel
{
public:
  explicit LHLevel();
  explicit LHLevel(Uint32 size);
  ~LHLevel() {}
private:
  LHLevel(LHLevel const&); // Not to be implemented
  LHLevel&  operator=(LHLevel const&); // Not to be implemented
public:
  void clear();
  bool isEmpty() const;
  bool isFull() const;
  Uint32 getSize() const;
  void setSize(Uint32 size);
  Uint32 getTop() const;
public:
  Uint32 getBucketNumber(Uint32 hash_value) const;
  bool getSplitBucket(Uint32& from, Uint32& to) const; // true if data move needed
  bool shouldMoveBeforeExpand(Uint32 hash_value) const;
  void expand();
  bool getMergeBuckets(Uint32& from, Uint32& to) const; // true if data move needed
  void shrink();
private:
  enum {
    ADDR_MAX = 0xFFFFFFFEU,
    MAX_SIZE = 0xFFFFFFFFU,
    MAXP_EMPTY = 0xFFFFFFFFU,
  };
protected:
  Uint32 max_size() const;
  Uint8 hashcheckbit() const { return m_hashcheckbit; }
  Uint32 maxp() const { return m_maxp; }
  Uint32 p() const { return m_p; }
private:
  Uint32 m_maxp;
  Uint32 m_p;
  Uint8 m_hashcheckbit;
};

class LocalLHLevel : public LHLevel
{
public:
  explicit LocalLHLevel(Uint32& size): LHLevel(size), m_src(size) {}
  ~LocalLHLevel() { m_src = getSize(); }
private:
  LocalLHLevel(const LocalLHLevel&); // Not to be implemented
  LocalLHLevel&  operator=(const LocalLHLevel&); // Not to be implemented
  Uint32& m_src;
};

/**
 * Implementation
 **/

inline LHLevel::LHLevel()
{
  setSize(0);
}

inline LHLevel::LHLevel(Uint32 _size)
{
  setSize(_size);
}

inline void LHLevel::clear()
{
  m_maxp = MAXP_EMPTY;
  m_p = 0;
}

inline bool LHLevel::isEmpty() const
{
  return maxp() == MAXP_EMPTY;
}

inline bool LHLevel::isFull() const
{
  return !isEmpty() && (getTop() == ADDR_MAX);
}

inline Uint32 LHLevel::max_size() const
{
  return MAX_SIZE;
}

inline Uint32 LHLevel::getSize() const
{
  assert(!isEmpty() || (maxp() + 1 + p() == 0));
  return maxp() + 1 + p();
}

inline void LHLevel::setSize(Uint32 size)
{
  assert(size <= max_size());
  if (size == 0)
    clear();
  else
  {
    m_hashcheckbit = BitmaskImpl::fls(size);
    m_maxp = (1 << hashcheckbit()) - 1;
    m_p = size - 1 - maxp();
  }
}

inline Uint32 LHLevel::getTop() const
{
  assert(!isEmpty());
  return maxp() + p();
}

inline Uint32 LHLevel::getBucketNumber(Uint32 hash_value) const
{
  assert(!isEmpty());
  Uint32 addr = hash_value & maxp();
  if (addr < p())
  {
    addr |= hash_value & (maxp() + 1);
  }
  return addr;
}

inline bool LHLevel::getSplitBucket(Uint32& from, Uint32& to) const
{
  assert(!isFull());

  from = p();
  to = getSize(); // == getTop() + 1

  // true if data move needed, that is, it was not empty
  return to > 0;
}

inline void LHLevel::expand()
{
  assert(!isFull());

  if (isEmpty())
  {
    m_p = 0;
    m_hashcheckbit = 0;
    m_maxp = 0;
  }
  else if (p() == maxp())
  {
    m_maxp = (maxp() << 1) | 1;
    m_hashcheckbit ++;
    m_p = 0;
  }
  else
  {
    m_p ++;
  }
}

inline bool LHLevel::shouldMoveBeforeExpand(Uint32 hash_value) const
{
  return hash_value & (1 << hashcheckbit());
}

inline bool LHLevel::getMergeBuckets(Uint32& from, Uint32& to) const
{
  assert(!isEmpty());

  from = getTop();

  if (likely(p() != 0))
  {
    to = p() - 1;
  }
  else
  {
    to = maxp() >> 1;
  }

  // true if data move needed, that is, all buckets disappeared
  return from > 0;
}

inline void LHLevel::shrink()
{
  assert(!isEmpty());

  if (p() != 0)
  {
    m_p --;
  }
  else if (maxp() == 0)
  {
    m_maxp --; // == MAXP_EMPTY
  }
  else
  {
    m_maxp >>= 1;
    m_hashcheckbit --;
    m_p = m_maxp;
  }
}

#endif
