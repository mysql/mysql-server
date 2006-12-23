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

#ifndef DLLIST_HPP
#define DLLIST_HPP

#include "ArrayPool.hpp"

/**
 * Template class used for implementing an
 *   list of object retreived from a pool
 */
template <typename P, typename T, typename U = T>
class DLListImpl 
{
public:
  /**
   * List head
   */
  struct HeadPOD {
    Uint32 firstItem;
    inline bool isEmpty() const { return firstItem == RNIL; }
    inline void init () { firstItem = RNIL; }
  };

  struct Head : public HeadPOD 
  {
    Head();
    Head& operator=(const HeadPOD& src) {
      this->firstItem = src.firstItem;
      return *this;
    }
  };
  
  DLListImpl(P& thePool);
  
  /**
   * Allocate an object from pool - update Ptr
   *
   * Return i
   */
  bool seize(Ptr<T> &);

  /**
   * Allocate object <b>i</b> from pool - update Ptr
   *
   * Return i
   */
  bool seizeId(Ptr<T> &, Uint32 i);

  /**
   * Check if <b>i</b> is allocated.
   */
  bool findId(Uint32 i) const;
  
  /**
   * Return an object to pool
   */
  void release(Uint32 i);
  
  /**
   * Return an object to pool
   */
  void release(Ptr<T> &);

  /**
   * Return all objects to the pool
   */
  void release();

  /**
   * Remove all objects from list
   */
  void remove();
  
  /**
   * Add object to list 
   * 
   * @NOTE MUST be seized from correct pool
   */
  void add(Ptr<T> &);

  /**
   * Add a list to list
   * @NOTE all elements _must_ be correctly initilized correctly wrt next/prev
   */
  void add(Uint32 first, Ptr<T> & last);
  
  /**
   * Remove object from list
   *
   * @NOTE Does not return it to pool
   */
  void remove(Ptr<T> &);

  /**
   * Remove object from list
   *
   * @NOTE Does not return it to pool
   */
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
   * @return True if element exists, false otherwise
   */
  bool first(Ptr<T> &) const ;

  /**
   * Get next element
   *
   * @note ptr must have both p & i values
   * 
   * @return True if element exists, false otherwise
   */
  bool next(Ptr<T> &) const ;
  
  /**
   * Check if next exists
   *
   * @note ptr must have both p & i values
   * @return True if element exists, false otherwise
   */
  bool hasNext(const Ptr<T> &) const;

  inline bool isEmpty() const { return head.firstItem == RNIL;}
  
protected:
  Head head;
  P & thePool;
};

template <typename P, typename T, typename U = T>
class LocalDLListImpl : public DLListImpl<P,T,U> 
{
public:
  LocalDLListImpl(P & thePool, typename DLListImpl<P,T,U>::HeadPOD & _src)
    : DLListImpl<P,T,U>(thePool), src(_src)
  {
    this->head = src;
  }
  
  ~LocalDLListImpl(){
    src = this->head;
  }
private:
  typename DLListImpl<P,T,U>::HeadPOD & src;
};

template <typename P, typename T, typename U>
inline
DLListImpl<P,T,U>::DLListImpl(P & _pool)
  : thePool(_pool)
{
}

template <typename P, typename T, typename U>
inline
DLListImpl<P,T,U>::Head::Head()
{
  this->init();
}

/**
 * Allocate an object from pool - update Ptr
 *
 * Return i
 */
template <typename P, typename T, typename U>
inline
bool
DLListImpl<P,T,U>::seize(Ptr<T> & p)
{
  if (likely(thePool.seize(p)))
  {
    add(p);
    return true;
  }
  return false;
}

/**
 * Allocate an object from pool - update Ptr
 *
 * Return i
 */
template <typename P, typename T, typename U>
inline
bool
DLListImpl<P,T,U>::seizeId(Ptr<T> & p, Uint32 ir)
{
  if (likely(thePool.seizeId(p, ir)))
  {
    add(p);
    return true;
  }
  return false;
}

template <typename P, typename T, typename U>
inline
bool
DLListImpl<P,T,U>::findId(Uint32 i) const 
{
  return thePool.findId(i);
}

template <typename P, typename T, typename U>
inline
void 
DLListImpl<P,T,U>::add(Ptr<T> & p)
{
  T * t = p.p;
  Uint32 ff = head.firstItem;
  
  t->U::nextList = ff;
  t->U::prevList = RNIL;
  head.firstItem = p.i;
  
  if(ff != RNIL)
  {
    T * t2 = thePool.getPtr(ff);
    t2->U::prevList = p.i;
  }
}

template <typename P, typename T, typename U>
inline
void 
DLListImpl<P,T,U>::add(Uint32 first, Ptr<T> & lastPtr)
{
  Uint32 ff = head.firstItem;

  head.firstItem = first;
  lastPtr.p->U::nextList = ff;
  
  if(ff != RNIL)
  {
    T * t2 = thePool.getPtr(ff);
    t2->U::prevList = lastPtr.i;
  }
}

template <typename P, typename T, typename U>
inline
void 
DLListImpl<P,T,U>::remove(Ptr<T> & p)
{
  remove(p.p);
}

template <typename P, typename T, typename U>
inline
void 
DLListImpl<P,T,U>::remove(T * p)
{
  T * t = p;
  Uint32 ni = t->U::nextList;
  Uint32 pi = t->U::prevList;

  if(ni != RNIL){
    T * tn = thePool.getPtr(ni);
    tn->U::prevList = pi;
  }
  
  if(pi != RNIL){
    T * tp = thePool.getPtr(pi);
    tp->U::nextList = ni;
  } else {
    head.firstItem = ni;
  }
}

/**
 * Return an object to pool
 */
template <typename P, typename T, typename U>
inline
void 
DLListImpl<P,T,U>::release(Uint32 i)
{
  Ptr<T> p;
  p.i = i;
  p.p = thePool.getPtr(i);
  release(p);
}
  
/**
 * Return an object to pool
 */
template <typename P, typename T, typename U>
inline
void 
DLListImpl<P,T,U>::release(Ptr<T> & p)
{
  remove(p);
  thePool.release(p);
}  

template <typename P, typename T, typename U>
inline
void 
DLListImpl<P,T,U>::release()
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
}

template <typename P, typename T, typename U>
inline
void 
DLListImpl<P,T,U>::remove()
{
  head.firstItem = RNIL;
}  

template <typename P, typename T, typename U>
inline
void 
DLListImpl<P,T,U>::getPtr(Ptr<T> & p, Uint32 i) const 
{
  p.i = i;
  p.p = thePool.getPtr(i);
}

template <typename P, typename T, typename U>
inline
void 
DLListImpl<P,T,U>::getPtr(Ptr<T> & p) const 
{
  thePool.getPtr(p);
}
  
template <typename P, typename T, typename U>
inline
T * 
DLListImpl<P,T,U>::getPtr(Uint32 i) const 
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
DLListImpl<P,T,U>::first(Ptr<T> & p) const 
{
  Uint32 i = head.firstItem;
  p.i = i;
  if(i != RNIL)
  {
    p.p = thePool.getPtr(i);
    return true;
  }
  p.p = NULL;
  return false;
}

template <typename P, typename T, typename U>
inline
bool
DLListImpl<P,T,U>::next(Ptr<T> & p) const 
{
  Uint32 i = p.p->U::nextList;
  p.i = i;
  if(i != RNIL){
    p.p = thePool.getPtr(i);
    return true;
  }
  p.p = NULL;
  return false;
}

template <typename P, typename T, typename U>
inline
bool
DLListImpl<P,T,U>::hasNext(const Ptr<T> & p) const 
{
  return p.p->U::nextList != RNIL;
}

// Specializations

template <typename T, typename U = T>
class DLList : public DLListImpl<ArrayPool<T>, T, U>
{
public:
  DLList(ArrayPool<T> & p) : DLListImpl<ArrayPool<T>, T, U>(p) {}
};

template <typename T, typename U = T>
class LocalDLList : public LocalDLListImpl<ArrayPool<T>, T, U> {
public:
  LocalDLList(ArrayPool<T> & p, typename DLList<T,U>::HeadPOD & _src)
    : LocalDLListImpl<ArrayPool<T>, T, U>(p, _src) {}
};

#endif
