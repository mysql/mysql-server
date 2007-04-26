/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef DLFIFOLIST_HPP
#define DLFIFOLIST_HPP

#include <ndb_global.h>
#include <kernel_types.h>
#include "Pool.hpp"

/**
 * Template class used for implementing an
 *   list of object retreived from a pool
 */
template <typename P, typename T, typename U = T>
class DLFifoListImpl 
{
public:
  /**
   * List head
   */
  struct Head 
  {
    Head();
    Uint32 firstItem;
    Uint32 lastItem;

#ifdef VM_TRACE
    bool in_use;
#endif

    inline bool isEmpty() const { return firstItem == RNIL;}
  };
  
  DLFifoListImpl(P & thePool);
  
  bool seizeFirst(Ptr<T> &);
  bool seizeLast(Ptr<T> &);
  bool seize(Ptr<T> & ptr) { return seizeLast(ptr);}
  
  void release(Ptr<T> &);
  void release(); // release all
  
  void addFirst(Ptr<T> &);
  void addLast(Ptr<T> &);
  void add(Ptr<T> & ptr) { addLast(ptr);}

  /**
   * Insert object <em>ptr</ptr> _before_ <em>loc</em>
   */
  void insert(Ptr<T> & ptr, Ptr<T>& loc);
  
  void remove();
  void remove(Ptr<T> &);
  void remove(T*);
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
   * Get next element
   *
   * NOTE ptr must be both p & i
   */
  bool prev(Ptr<T> &) const ;
  
  /**
   * Check if next exists i.e. this is not last
   *
   * NOTE ptr must be both p & i
   */
  bool hasNext(const Ptr<T> &) const;

  /**
   * Check if next exists i.e. this is not last
   *
   * NOTE ptr must be both p & i
   */
  bool hasPrev(const Ptr<T> &) const;
  
  inline bool isEmpty() const { return head.firstItem == RNIL;}

  /**
   * Copy list (head)
   *   Will construct to identical lists
   */
  DLFifoListImpl<P,T,U>& operator=(const DLFifoListImpl<P,T,U>& src){
    assert(&thePool == &src.thePool);
    this->head = src.head;
    return * this;
  }
  
protected:
  Head head;
  P & thePool;
};

template <typename P, typename T, typename U = T>
class LocalDLFifoListImpl : public DLFifoListImpl<P,T,U> 
{
public:
  LocalDLFifoListImpl(P & thePool, typename DLFifoListImpl<P,T,U>::Head &_src)
    : DLFifoListImpl<P,T,U>(thePool), src(_src)
  {
    this->head = src;
#ifdef VM_TRACE
    assert(src.in_use == false);
    src.in_use = true;
#endif
  }
  
  ~LocalDLFifoListImpl(){
#ifdef VM_TRACE
    assert(src.in_use == true);
#endif
    src = this->head;
  }
private:
  typename DLFifoListImpl<P,T,U>::Head & src;
};

template <typename P, typename T, typename U>
inline
DLFifoListImpl<P,T,U>::DLFifoListImpl(P & _pool):
  thePool(_pool)
{
}

template <typename P, typename T, typename U>
inline
DLFifoListImpl<P,T,U>::Head::Head()
{
  firstItem = RNIL;
  lastItem = RNIL;
#ifdef VM_TRACE
  in_use = false;
#endif
}

template <typename P, typename T, typename U>
inline
bool
DLFifoListImpl<P,T,U>::seizeFirst(Ptr<T> & p)
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
DLFifoListImpl<P,T,U>::seizeLast(Ptr<T> & p)
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
DLFifoListImpl<P,T,U>::addFirst(Ptr<T> & p)
{
  Uint32 ff = head.firstItem;
  
  p.p->U::prevList = RNIL;
  p.p->U::nextList = ff;
  head.firstItem = p.i;
  if (ff == RNIL)
  {
    head.lastItem = p.i;
  }
  else
  {
    T * t2 = thePool.getPtr(ff);
    t2->U::prevList = p.i;
  }
}

template <typename P, typename T, typename U>
inline
void
DLFifoListImpl<P,T,U>::addLast(Ptr<T> & p)
{
  T * t = p.p;
  Uint32 last = head.lastItem;
  head.lastItem = p.i;    
  
  t->U::nextList = RNIL;
  t->U::prevList = last;
  
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
DLFifoListImpl<P,T,U>::insert(Ptr<T> & ptr, Ptr<T> & loc)
{
  Uint32 prev= loc.p->U::prevList;
  if(loc.i == head.firstItem)
  {
    head.firstItem = ptr.i;
    assert(prev == RNIL);
  }
  else
  {
    T* t2 = thePool.getPtr(prev);
    t2->U::nextList = ptr.i;
  }
  
  loc.p->U::prevList = ptr.i;
  ptr.p->U::prevList = prev;
  ptr.p->U::nextList = loc.i;
}

template <typename P, typename T, typename U>
inline
void
DLFifoListImpl<P,T,U>::remove()
{
  head.firstItem = RNIL;
  head.lastItem = RNIL;
}

template <typename P, typename T, typename U>
inline
void
DLFifoListImpl<P,T,U>::remove(Ptr<T> & p)
{
  remove(p.p);
}

template <typename P, typename T, typename U>
inline
void
DLFifoListImpl<P,T,U>::remove(T * t)
{
  Uint32 ni = t->U::nextList;
  Uint32 pi = t->U::prevList;

  if(ni != RNIL)
  {
    T * t = thePool.getPtr(ni);
    t->U::prevList = pi;
  } 
  else 
  {
    // We are releasing last
    head.lastItem = pi;
  }
  
  if(pi != RNIL)
  {
    T * t = thePool.getPtr(pi);
    t->U::nextList = ni;
  } 
  else 
  {
    // We are releasing first
    head.firstItem = ni;
  }
}

template <typename P, typename T, typename U>
inline
void 
DLFifoListImpl<P,T,U>::release()
{
  Ptr<T> ptr;
  Uint32 curr = head.firstItem;
  while(curr != RNIL)
  {
    thePool.getPtr(ptr, curr);
    curr = ptr.p->U::nextList;
    thePool.release(ptr);
  }
  head.firstItem = RNIL;
  head.lastItem = RNIL;
}

template <typename P, typename T, typename U>
inline
void 
DLFifoListImpl<P,T,U>::release(Ptr<T> & p)
{
  remove(p.p);
  thePool.release(p);
}

template <typename P, typename T, typename U>
inline
void 
DLFifoListImpl<P,T,U>::getPtr(Ptr<T> & p, Uint32 i) const 
{
  p.i = i;
  p.p = thePool.getPtr(i);
}

template <typename P, typename T, typename U>
inline
void 
DLFifoListImpl<P,T,U>::getPtr(Ptr<T> & p) const 
{
  thePool.getPtr(p);
}
  
template <typename P, typename T, typename U>
inline
T * 
DLFifoListImpl<P,T,U>::getPtr(Uint32 i) const 
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
DLFifoListImpl<P,T,U>::first(Ptr<T> & p) const 
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
DLFifoListImpl<P,T,U>::last(Ptr<T> & p) const 
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
DLFifoListImpl<P,T,U>::next(Ptr<T> & p) const 
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
DLFifoListImpl<P,T,U>::prev(Ptr<T> & p) const 
{
  p.i = p.p->U::prevList;
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
DLFifoListImpl<P,T,U>::hasNext(const Ptr<T> & p) const 
{
  return p.p->U::nextList != RNIL;
}

template <typename P, typename T, typename U>
inline
bool
DLFifoListImpl<P,T,U>::hasPrev(const Ptr<T> & p) const 
{
  return p.p->U::prevList != RNIL;
}

// Specializations

template <typename T, typename U = T>
class DLFifoList : public DLFifoListImpl<ArrayPool<T>, T, U>
{
public:
  DLFifoList(ArrayPool<T> & p) : DLFifoListImpl<ArrayPool<T>, T, U>(p) {}
};

template <typename T, typename U = T>
class LocalDLFifoList : public LocalDLFifoListImpl<ArrayPool<T>,T,U> {
public:
  LocalDLFifoList(ArrayPool<T> & p, typename DLFifoList<T,U>::Head & _src)
    : LocalDLFifoListImpl<ArrayPool<T>,T,U>(p, _src) {}
};

#endif
