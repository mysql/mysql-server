/*
   Copyright (c) 2005, 2016, Oracle and/or its affiliates. All rights reserved.

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

#ifndef NDB_ROPE_HPP
#define NDB_ROPE_HPP

#include "ArrayPool.hpp"
#include "DataBuffer.hpp"

#define JAM_FILE_ID 316


typedef DataBuffer<7,ArrayPool<DataBufferSegment<7> > > RopeBase;
typedef RopeBase::DataBufferPool RopePool;

struct RopeHandle {
  RopeHandle() { m_hash = 0; }

  Uint32 m_hash;
  RopeBase::Head m_head; 

  Uint32 hashValue() const { return m_hash; }
};

class ConstRope : private RopeBase {
public:
  ConstRope(RopePool& thePool, const RopeHandle& handle)  
    : RopeBase(thePool), src(handle)
  {
    this->head = src.m_head;
  }
  
  ~ConstRope(){
  }

  Uint32 size() const;
  bool empty() const;

  void copy(char* buf) const;
  
  int compare(const char * s) const { return compare(s, (Uint32)strlen(s) + 1);}
  int compare(const char *, Uint32 len) const; 

  bool equal(const ConstRope& r2) const;
private:
  const RopeHandle & src;
};

class LocalRope : private RopeBase {
public:
  LocalRope(RopePool& thePool, RopeHandle& handle)
    : RopeBase(thePool), src(handle)
  {
    this->head = src.m_head;
    m_hash = src.m_hash;
  }
  
  ~LocalRope(){
    src.m_head = this->head;
    src.m_hash = m_hash;
  }

  Uint32 size() const;
  bool empty() const;

  void copy(char* buf) const;
  
  int compare(const char * s) const { return compare(s, Uint32(strlen(s) + 1));}
  int compare(const char *, Uint32 len) const; 
  
  bool assign(const char * s) { return assign(s, Uint32(strlen(s) + 1));}
  bool assign(const char * s, Uint32 l) { return assign(s, l, hash(s, l));}
  bool assign(const char *, Uint32 len, Uint32 hash);

  void erase();
  
  static Uint32 hash(const char * str, Uint32 len);

  static Uint32 getSegmentSize() { return RopeBase::getSegmentSize();}
private:
  Uint32 m_hash;
  RopeHandle & src;
};

inline
Uint32
LocalRope::size() const {
  return head.used;
}

inline
bool
LocalRope::empty() const {
  return head.used == 0;
}

inline
Uint32
ConstRope::size() const {
  return head.used;
}

inline
bool
ConstRope::empty() const {
  return head.used == 0;
}


#undef JAM_FILE_ID

#endif

