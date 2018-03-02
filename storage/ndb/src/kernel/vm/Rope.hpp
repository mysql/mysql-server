/*
   Copyright (c) 2005, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef NDB_ROPE_HPP
#define NDB_ROPE_HPP

#include "ArrayPool.hpp"
#include "DataBuffer.hpp"

#define JAM_FILE_ID 316


typedef DataBuffer<7,ArrayPool<DataBufferSegment<7> > > RopeBase;
typedef RopeBase::DataBufferPool RopePool;

struct RopeHandle {
  RopeHandle() { m_hash = m_length = 0; }
  RopeHandle(RopeBase::Head h, Uint32 l) : m_length(l), m_head(h) {}

  Uint32 m_hash;
  Uint32 m_length;
  RopeBase::Head m_head;

  Uint32 hashValue() const { return m_hash; }
};

class ConstRope : private RopeBase {
public:
  ConstRope(RopePool& thePool, const RopeHandle& handle)  
    : RopeBase(thePool), src(handle)
  {
    this->head = src.m_head;
    m_length = src.m_length;
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
  const char * firstSegment(Ptr<Segment> &) const;
  const char * nextSegment(Ptr<Segment> &) const;

private:
  const RopeHandle & src;
  Uint32 m_length;
};

class LocalRope : private RopeBase {
public:
  LocalRope(RopePool& thePool, RopeHandle& handle)
    : RopeBase(thePool), src(handle)
  {
    this->head = src.m_head;
    m_length = src.m_length;
    m_hash = src.m_hash;
  }
  
  ~LocalRope(){
    src.m_head = this->head;
    src.m_length = m_length;
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
  
  static Uint32 hash(const char * str, Uint32 len, Uint32 starter = 0);

  static Uint32 getSegmentSizeInBytes() { return RopeBase::getSegmentSizeInBytes();}

private:
  char * firstSegment(Ptr<Segment> &) const;
  char * nextSegment(Ptr<Segment> &) const;

private:
  Uint32 m_hash;
  Uint32 m_length;
  RopeHandle & src;
};

inline
Uint32
LocalRope::size() const {
  return m_length;
}

inline
bool
LocalRope::empty() const {
  return m_length == 0;
}

inline
Uint32
ConstRope::size() const {
  return m_length;
}

inline
bool
ConstRope::empty() const {
  return m_length == 0;
}


#undef JAM_FILE_ID

#endif

