/*
   Copyright (c) 2003, 2010, 2011 Oracle and/or its affiliates. All rights reserved.

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

#ifndef DLLIST_HPP
#define DLLIST_HPP

#include "ArrayPool.hpp"

/**
 * DLMList implements a intrusive list using chaining
 *   (with a double links)
 *
 * The entries in the (uninstansiated) meta class passed to the
 * list must have the following methods:
 *
 *  -# nextList(U&) returning a reference to the next link
 *  -# prevList(U&) returning a reference to the prev link
 */

template <typename T, typename U = T> struct DLListDefaultMethods {
static inline Uint32& nextList(U& t) { return t.nextList; }
static inline Uint32& prevList(U& t) { return t.prevList; }
};

template <typename P, typename T, typename M = DLListDefaultMethods<T> >
class DLMList
{
public:
  explicit DLMList(P& thePool);
  ~DLMList() { }
private:
  DLMList(const DLMList&);
  DLMList&  operator=(const DLMList&);

public:
  /**
   * List head
   */
  struct HeadPOD {
    Uint32 firstItem;
    inline bool isEmpty() const { return firstItem == RNIL; }
    inline void init () { 
      firstItem = RNIL; 
#ifdef VM_TRACE
      in_use = false;
#endif
    }

#ifdef VM_TRACE
    bool in_use;
#endif
  };

  struct Head : public HeadPOD 
  {
    Head();
    Head& operator=(const HeadPOD& src) {
      this->firstItem = src.firstItem;
      return *this;
    }
  };
  
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

template <typename P, typename T, typename M = DLListDefaultMethods<T> >
class LocalDLMList : public DLMList<P, T, M>
{
public:
  LocalDLMList(P& thePool, typename DLMList<P, T, M>::HeadPOD& _src)
    : DLMList<P, T, M>(thePool), src(_src)
  {
    this->head = src;
#ifdef VM_TRACE
    assert(src.in_use == false);
    src.in_use = true;
#endif
  }
  
  ~LocalDLMList()
  {
#ifdef VM_TRACE
    assert(src.in_use == true);
#endif
    src = this->head;
  }
private:
  typename DLMList<P, T, M>::HeadPOD& src;
};

template <typename P, typename T, typename M>
inline
DLMList<P, T, M>::DLMList(P& _pool)
  : thePool(_pool)
{
  // Require user defined constructor on T since we fiddle
  // with T's members
  ASSERT_TYPE_HAS_CONSTRUCTOR(T);
}

template <typename P, typename T, typename M>
inline
DLMList<P, T, M>::Head::Head()
{
  this->init();
}

/**
 * Allocate an object from pool - update Ptr
 *
 * Return i
 */
template <typename P, typename T, typename M>
inline
bool
DLMList<P, T, M>::seize(Ptr<T>& p)
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
template <typename P, typename T, typename M>
inline
bool
DLMList<P, T, M>::seizeId(Ptr<T>& p, Uint32 ir)
{
  if (likely(thePool.seizeId(p, ir)))
  {
    add(p);
    return true;
  }
  return false;
}

template <typename P, typename T, typename M>
inline
bool
DLMList<P, T, M>::findId(Uint32 i) const
{
  return thePool.findId(i);
}

template <typename P, typename T, typename M>
inline
void 
DLMList<P, T, M>::add(Ptr<T>& p)
{
  T * t = p.p;
  Uint32 ff = head.firstItem;
  
  M::nextList(*t) = ff;
  M::prevList(*t) = RNIL;
  head.firstItem = p.i;
  
  if(ff != RNIL)
  {
    T * t2 = thePool.getPtr(ff);
    M::prevList(*t2) = p.i;
  }
}

template <typename P, typename T, typename M>
inline
void 
DLMList<P, T, M>::add(Uint32 first, Ptr<T>& lastPtr)
{
  Uint32 ff = head.firstItem;

  head.firstItem = first;
  M::nextList(*lastPtr.p) = ff;
  
  if(ff != RNIL)
  {
    T * t2 = thePool.getPtr(ff);
    M::prevList(*t2) = lastPtr.i;
  }
}

template <typename P, typename T, typename M>
inline
void 
DLMList<P, T, M>::remove(Ptr<T>& p)
{
  remove(p.p);
}

template <typename P, typename T, typename M>
inline
void 
DLMList<P, T, M>::remove(T * p)
{
  T * t = p;
  Uint32 ni = M::nextList(*t);
  Uint32 pi = M::prevList(*t);

  if(ni != RNIL){
    T * tn = thePool.getPtr(ni);
    M::prevList(*tn) = pi;
  }
  
  if(pi != RNIL){
    T * tp = thePool.getPtr(pi);
    M::nextList(*tp) = ni;
  } else {
    head.firstItem = ni;
  }
}

/**
 * Return an object to pool
 */
template <typename P, typename T, typename M>
inline
void 
DLMList<P, T, M>::release(Uint32 i)
{
  Ptr<T> p;
  p.i = i;
  p.p = thePool.getPtr(i);
  release(p);
}
  
/**
 * Return an object to pool
 */
template <typename P, typename T, typename M>
inline
void 
DLMList<P, T, M>::release(Ptr<T>& p)
{
  remove(p);
  thePool.release(p);
}  

template <typename P, typename T, typename M>
inline
void 
DLMList<P, T, M>::release()
{
  Ptr<T> ptr;
  Uint32 curr = head.firstItem;
  while(curr != RNIL)
  {
    thePool.getPtr(ptr, curr);
    curr = M::nextList(*ptr.p);
    thePool.release(ptr);
  }
  head.firstItem = RNIL;
}

template <typename P, typename T, typename M>
inline
void 
DLMList<P, T, M>::remove()
{
  head.firstItem = RNIL;
}  

template <typename P, typename T, typename M>
inline
void 
DLMList<P, T, M>::getPtr(Ptr<T>& p, Uint32 i) const
{
  p.i = i;
  p.p = thePool.getPtr(i);
}

template <typename P, typename T, typename M>
inline
void 
DLMList<P, T, M>::getPtr(Ptr<T>& p) const
{
  thePool.getPtr(p);
}
  
template <typename P, typename T, typename M>
inline
T * 
DLMList<P, T, M>::getPtr(Uint32 i) const
{
  return thePool.getPtr(i);
}

/**
 * Update ptr to first element in list
 *
 * Return i
 */
template <typename P, typename T, typename M>
inline
bool
DLMList<P, T, M>::first(Ptr<T>& p) const
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

template <typename P, typename T, typename M>
inline
bool
DLMList<P, T, M>::next(Ptr<T>& p) const
{
  Uint32 i = M::nextList(*p.p);
  p.i = i;
  if(i != RNIL){
    p.p = thePool.getPtr(i);
    return true;
  }
  p.p = NULL;
  return false;
}

template <typename P, typename T, typename M>
inline
bool
DLMList<P, T, M>::hasNext(const Ptr<T>& p) const
{
  return M::nextList(*p.p) != RNIL;
}

// Specializations

template <typename P, typename T, typename U = T, typename M = DLListDefaultMethods<T, U> >
class DLListImpl : public DLMList<P, T, M >
{
public:
  DLListImpl(P& p) : DLMList<P, T, M >(p) {}
};

template <typename P, typename T, typename U = T, typename M = DLListDefaultMethods<T, U> >
class LocalDLListImpl : public LocalDLMList<P, T, M > {
public:
  LocalDLListImpl(P& p, typename DLMList<P, T, M>::HeadPOD& _src)
    : LocalDLMList<P, T, M>(p, _src) {}
};

template <typename T, typename U = T, typename M = DLListDefaultMethods<T, U> >
class DLList : public DLMList<ArrayPool<T>, T, M >
{
public:
  DLList(ArrayPool<T>& p) : DLMList<ArrayPool<T>, T, M >(p) {}
};

template <typename T, typename U = T, typename M = DLListDefaultMethods<T, U> >
class LocalDLList : public LocalDLMList<ArrayPool<T>, T, M > {
public:
  LocalDLList(ArrayPool<T>& p, typename DLList<T, U, M>::HeadPOD& _src)
    : LocalDLMList<ArrayPool<T>, T, M>(p, _src) {}
};

#endif
