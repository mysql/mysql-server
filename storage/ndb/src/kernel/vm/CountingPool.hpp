/* Copyright (c) 2012, 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
*/

#ifndef COUNTINGPOOL_HPP
#define COUNTINGPOOL_HPP

#include <ndb_global.h>
#include "blocks/diskpage.hpp"
#include "blocks/dbtup/tuppage.hpp"
#include "ndbd_malloc_impl.hpp"
#include "IntrusiveList.hpp"
#include "Pool.hpp"

#define JAM_FILE_ID 332


#ifndef UINT32_MAX
#define UINT32_MAX (4294967295U)
#endif

// Implementation CountingPool

template<typename T, class P>
class CountingPool : public P
{
  Uint32 m_inuse;
  Uint32 m_inuse_high;
  Uint32 m_max_allowed;
protected:
public:
  CountingPool() :m_inuse(0), m_inuse_high(0), m_max_allowed(UINT32_MAX)
    {}

  bool seize(Ptr<T>& ptr)
  {
    if (m_inuse >= m_max_allowed)
    {
      return false;
    }
    bool ok = P::seize(ptr);
    if (!ok)
    {
      return false;
    }
    m_inuse++;
    if (m_inuse_high < m_inuse)
    {
      m_inuse_high++;
    }
    return true;
  }

  void release(Ptr<T> ptr)
  {
    P::release(ptr);
    m_inuse--;
  }

  void release(Uint32 i)
  {
    Ptr<T> p;
    getPtr(p, i);
    release(p);
  }

  T* getPtr(Uint32 i)
  {
    return P::getPtr(i);
  }

  void getPtr(Ptr<T>& p, Uint32 i)
  {
    p.i = i;
    p.p = getPtr(i);
  }

  void getPtr(Ptr<T>& p)
  {
    p.p = getPtr(p.i);
  }

  bool seize(Uint32& i)
  {
    Ptr<T> p;
    p.i = i;
    bool ok = seize(p);
    i = p.i;
    return ok;
  }

public:
  // Extra methods
  void setSize(Uint32 size)
  {
    m_max_allowed = size;
  }

  Uint32 getSize() const
  {
    return m_max_allowed /*m_seized*/;
  }

  Uint32 getEntrySize() const
  {
    return 8 * ((sizeof(T) + 7) / 8);  // Assuming alignment every 8 byte
  }

  Uint32 getNoOfFree() const
  {
    return getSize() - getUsed();
  }

  Uint32 getUsed() const
  {
    return m_inuse;
  }

  Uint32 getUsedHi() const
  {
    return m_inuse_high;
  }
};


#undef JAM_FILE_ID

#endif
