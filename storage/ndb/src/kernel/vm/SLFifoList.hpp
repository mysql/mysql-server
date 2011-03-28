/*
   Copyright (c) 2006, 2010, Oracle and/or its affiliates. All rights reserved.

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

#ifndef SLFIFOLIST_HPP
#define SLFIFOLIST_HPP

#include <ndb_global.h>
#include <kernel_types.h>
#include "Pool.hpp"

/**
 * Template class used for implementing an
 *   list of object retreived from a pool
 */
template <typename P, typename T, typename U = T>
class SLFifoListImpl 
{
public:
  /**
   * List head
   */
  struct HeadPOD
  {
    Uint32 firstItem;
    Uint32 lastItem;

#ifdef VM_TRACE
    bool in_use;
#endif
    void init();
    inline bool isEmpty() const { return firstItem == RNIL;}
  };
  
  struct Head : public HeadPOD
  {
    Head() { this->init();}

    Head& operator=(const HeadPOD& src) {
      this->firstItem = src.firstItem;
      this->lastItem = src.lastItem;
#ifdef VM_TRACE
      this->in_use = src.in_use;
#endif
      return *this;
    }
  };
  SLFifoListImpl(P & thePool);
  
  bool seizeFirst(Ptr<T> &);
  bool seizeLast(Ptr<T> &);
  bool seize(Ptr<T> & ptr) { return seizeLast(ptr);}
  
  void releaseFirst(Ptr<T> &);
  
  void addFirst(Ptr<T> &);
  void addLast(Ptr<T> &);
  
  void removeFirst(Ptr<T> &);
  void remove() { head.init(); }

  /**
   *  Update i & p value according to <b>i</b>
   */
  void getPtr(Ptr<T> &, Uint32 i) const;
  
  /**
   * Update p value for ptr according to i value 
   */
  void getPtr(Ptr<T> &) const ;
  
  /**
   * Get pointer for i value
   */
  T * getPtr(Uint32 i) const ;

  /**
   * Update ptr to first element in list
   *
   * Return i
   */
  bool first(Ptr<T> &) const ;

  /**
   * Update ptr to first element in list
   *
   * Return i
   */
  bool last(Ptr<T> &) const ;

  /**
   * Get next element
   *
   * NOTE ptr must be both p & i
   */
  bool next(Ptr<T> &) const ;
  
  /**
   * Check if next exists i.e. this is not last
   *
   * NOTE ptr must be both p & i
   */
  bool hasNext(const Ptr<T> &) const;
  
  inline bool isEmpty() const { return head.firstItem == RNIL;}
  
protected:
  Head head;
  P & thePool;
};

template <typename P, typename T, typename U = T>
class LocalSLFifoListImpl : public SLFifoListImpl<P,T,U> 
{
public:
  LocalSLFifoListImpl(P & thePool, typename SLFifoListImpl<P,T,U>::HeadPOD&_src)
    : SLFifoListImpl<P,T,U>(thePool), src(_src)
  {
    this->head = src;
#ifdef VM_TRACE
    assert(src.in_use == false);
    src.in_use = true;
#endif
  }
  
  ~LocalSLFifoListImpl(){
#ifdef VM_TRACE
    assert(src.in_use == true);
#endif
    src = this->head;
  }
private:
  typename SLFifoListImpl<P,T,U>::HeadPOD & src;
};

template <typename P, typename T, typename U>
inline
SLFifoListImpl<P,T,U>::SLFifoListImpl(P & _pool):
  thePool(_pool)
{
}

template <typename P, typename T, typename U>
inline
void
SLFifoListImpl<P,T,U>::HeadPOD::init()
{
  this->firstItem = RNIL;
  this->lastItem = RNIL;
#ifdef VM_TRACE
  this->in_use = false;
#endif
}

template <typename P, typename T, typename U>
inline
bool
SLFifoListImpl<P,T,U>::seizeFirst(Ptr<T> & p)
{
  if (likely(thePool.seize(p)))
  {
    addFirst(p);
    return true;
  }
  p.p = NULL;
  return false;
}

template <typename P, typename T, typename U>
inline
bool
SLFifoListImpl<P,T,U>::seizeLast(Ptr<T> & p)
{
  if (likely(thePool.seize(p)))
  {
    addLast(p);
    return true;
  }
  p.p = NULL;
  return false;
}

template <typename P, typename T, typename U>
inline
void
SLFifoListImpl<P,T,U>::addFirst(Ptr<T> & p)
{
  Uint32 first = head.firstItem;
  head.firstItem = p.i;
  if (first == RNIL)
  {
    head.lastItem = p.i;
  }
  p.p->U::nextList = first;
}

template <typename P, typename T, typename U>
inline
void
SLFifoListImpl<P,T,U>::addLast(Ptr<T> & p)
{
  T * t = p.p;
  Uint32 last = head.lastItem;
  
  t->U::nextList = RNIL;
  head.lastItem = p.i;    
  
  if(last != RNIL)
  {
    T * t2 = thePool.getPtr(last);
    t2->U::nextList = p.i;
  }
  else
  {
    head.firstItem = p.i;
  }
}

template <typename P, typename T, typename U>
inline
void
SLFifoListImpl<P,T,U>::removeFirst(Ptr<T> & p)
{
  Uint32 first = head.firstItem;
  Uint32 last = head.lastItem;
  assert(p.i == first);
  if (first != last)
  {
    head.firstItem = p.p->U::nextList;
  }
  else
  {
    head.firstItem = head.lastItem = RNIL;
  }
}

template <typename P, typename T, typename U>
inline
void 
SLFifoListImpl<P,T,U>::releaseFirst(Ptr<T> & p)
{
  removeFirst(p);
  thePool.release(p);
}

template <typename P, typename T, typename U>
inline
void 
SLFifoListImpl<P,T,U>::getPtr(Ptr<T> & p, Uint32 i) const 
{
  p.i = i;
  p.p = thePool.getPtr(i);
}

template <typename P, typename T, typename U>
inline
void 
SLFifoListImpl<P,T,U>::getPtr(Ptr<T> & p) const 
{
  thePool.getPtr(p);
}
  
template <typename P, typename T, typename U>
inline
T * 
SLFifoListImpl<P,T,U>::getPtr(Uint32 i) const 
{
  return thePool.getPtr(i);
}

/**
 * Update ptr to first element in list
 *
 * Return i
 */
template <typename P, typename T, typename U>
inline
bool
SLFifoListImpl<P,T,U>::first(Ptr<T> & p) const 
{
  p.i = head.firstItem;
  if(p.i != RNIL)
  {
    p.p = thePool.getPtr(p.i);
    return true;
  }
  p.p = NULL;
  return false;
}

template <typename P, typename T, typename U>
inline
bool
SLFifoListImpl<P,T,U>::last(Ptr<T> & p) const 
{
  p.i = head.lastItem;
  if(p.i != RNIL)
  {
    p.p = thePool.getPtr(p.i);
    return true;
  }
  p.p = NULL;
  return false;
}

template <typename P, typename T, typename U>
inline
bool
SLFifoListImpl<P,T,U>::next(Ptr<T> & p) const 
{
  p.i = p.p->U::nextList;
  if(p.i != RNIL)
  {
    p.p = thePool.getPtr(p.i);
    return true;
  }
  p.p = NULL;
  return false;
}

template <typename P, typename T, typename U>
inline
bool
SLFifoListImpl<P,T,U>::hasNext(const Ptr<T> & p) const 
{
  return p.p->U::nextList != RNIL;
}

// Specializations

template <typename T, typename U = T>
class SLFifoList : public SLFifoListImpl<ArrayPool<T>, T, U>
{
public:
  SLFifoList(ArrayPool<T> & p) : SLFifoListImpl<ArrayPool<T>, T, U>(p) {}
};

template <typename T, typename U = T>
class LocalSLFifoList : public LocalSLFifoListImpl<ArrayPool<T>,T,U> {
public:
  LocalSLFifoList(ArrayPool<T> & p, typename SLFifoList<T,U>::Head & _src)
    : LocalSLFifoListImpl<ArrayPool<T>,T,U>(p, _src) {}
};

#endif
