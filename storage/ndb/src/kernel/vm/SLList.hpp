/*
   Copyright (C) 2003-2006 MySQL AB
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

#ifndef SLLIST_HPP
#define SLLIST_HPP

#include "ArrayPool.hpp"
#include <NdbOut.hpp>

/**
 * Template class used for implementing an
 *   list of object retreived from a pool
 */
template <typename P, typename T, typename U = T>
class SLListImpl 
{
public:
  /**
   * List head
   */
  struct HeadPOD {
    Uint32 firstItem;
    void init() { firstItem = RNIL;}
  };

  struct Head : public HeadPOD {
    Head();
    Head& operator= (const HeadPOD& src) { 
      this->firstItem = src.firstItem;
      return *this;
    }
  };
  
  SLListImpl(P & thePool);
  
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
   * Allocate <b>n</b>objects from pool
   *
   * Return i value of first object allocated or RNIL if fails
   */
  bool seizeN(Ptr<T> &, Uint32 n);
  
  /**
   * Return all objects to the pool
   */
  void release();

  /**
   * Remove all object from list but don't return to pool
   */
  void remove();

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
   * Get next element
   *
   * NOTE ptr must be both p & i
   */
  bool next(Ptr<T> &) const ;
  
  /**
   * Check if next exists
   *
   * NOTE ptr must be both p & i
   */
  bool hasNext(const Ptr<T> &) const;

  /**
   * Add
   */
  void add(Ptr<T> & p){
    p.p->U::nextList = head.firstItem;
    head.firstItem = p.i;
  }

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
  bool remove_front(Ptr<T> &);

  Uint32 noOfElements() const {
    Uint32 c = 0;
    Uint32 i = head.firstItem;
    while(i != RNIL){
      c++;
      const T * t = thePool.getPtr(i);
      i = t->U::nextList;
    }
    return c;
  }

  /**
   * Print
   * (Run operator NdbOut<< on every element)
   */
  void print(NdbOut & out) {
    out << "firstItem = " << head.firstItem << endl;
    Uint32 i = head.firstItem;
    while(i != RNIL){
      T * t = thePool.getPtr(i);
      t->print(out); out << " ";
      i = t->next;
    }
  }

  inline bool empty() const { return head.firstItem == RNIL;}

protected:
  Head head;
  P & thePool;
};

template <typename P, typename T, typename U = T>
class LocalSLListImpl : public SLListImpl<P, T, U> 
{
public:
  LocalSLListImpl(P & thePool, typename SLListImpl<P, T, U>::HeadPOD & _src)
    : SLListImpl<P, T, U>(thePool), src(_src)
  {
    this->head = src;
  }
  
  ~LocalSLListImpl(){
    src = this->head;
  }
private:
  typename SLListImpl<P, T, U>::HeadPOD & src;
};

template <typename P, typename T, typename U>
inline
SLListImpl<P, T, U>::SLListImpl(P & _pool):
  thePool(_pool)
{
}

template <typename P, typename T, typename U>
inline
SLListImpl<P, T, U>::Head::Head()
{
  this->init();
}

template <typename P, typename T, typename U>
inline
bool
SLListImpl<P, T, U>::seize(Ptr<T> & p)
{
  thePool.seize(p);
  T * t = p.p;
  Uint32 ff = head.firstItem;
  if(p.i != RNIL)
  {
    t->U::nextList = ff;
    head.firstItem = p.i;
    return true;
  }
  return false;
}

template <typename P, typename T, typename U>
inline
bool
SLListImpl<P, T, U>::seizeId(Ptr<T> & p, Uint32 ir)
{
  thePool.seizeId(p, ir);
  T * t = p.p;
  Uint32 ff = head.firstItem;
  if(p.i != RNIL)
  {
    t->U::nextList = ff;
    head.firstItem = p.i;
    return true;
  }
  return false;
}

template <typename P, typename T, typename U>
inline
bool
SLListImpl<P, T, U>::seizeN(Ptr<T> & p, Uint32 n)
{
  for(Uint32 i = 0; i < n; i++)
  {
    if(seize(p) == RNIL)
    {
      /**
       * Failure
       */
      for(; i > 0; i--)
      {
	p.i = head.firstItem;
	thePool.getPtr(p);
	head.firstItem = p.p->U::nextList;
	thePool.release(p);
      }
      return false;
    }
  }    

  /**
   * Success
   */
  p.i = head.firstItem;
  p.p = thePool.getPtr(head.firstItem);
  
  return true;
}


template <typename P, typename T, typename U>
inline
void 
SLListImpl<P, T, U>::remove()
{
  head.firstItem = RNIL;
}  

template <typename P, typename T, typename U>
inline
bool
SLListImpl<P, T, U>::remove_front(Ptr<T> & p)
{
  p.i = head.firstItem;
  if (p.i != RNIL)
  {
    p.p = thePool.getPtr(p.i);
    head.firstItem = p.p->U::nextList;
    return true;
  }
  return false;
}  

template <typename P, typename T, typename U>
inline
void
SLListImpl<P, T, U>::add(Uint32 first, Ptr<T> & last)
{
  last.p->U::nextList = head.firstItem;
  head.firstItem = first;
}  

template <typename P, typename T, typename U>
inline
void 
SLListImpl<P, T, U>::release()
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
SLListImpl<P, T, U>::getPtr(Ptr<T> & p, Uint32 i) const 
{
  p.i = i;
  p.p = thePool.getPtr(i);
}

template <typename P, typename T, typename U>
inline
void 
SLListImpl<P, T, U>::getPtr(Ptr<T> & p) const 
{
  thePool.getPtr(p);
}
  
template <typename P, typename T, typename U>
inline
T * 
SLListImpl<P, T, U>::getPtr(Uint32 i) const 
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
SLListImpl<P, T, U>::first(Ptr<T> & p) const 
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
SLListImpl<P, T, U>::next(Ptr<T> & p) const 
{
  Uint32 i = p.p->U::nextList;
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
SLListImpl<P, T, U>::hasNext(const Ptr<T> & p) const 
{
  return p.p->U::nextList != RNIL;
}

// Specializations

template <typename T, typename U = T>
class SLList : public SLListImpl<ArrayPool<T>, T, U>
{
public:
  SLList(ArrayPool<T> & p) : SLListImpl<ArrayPool<T>, T, U>(p) {}
};

template <typename T, typename U = T>
class LocalSLList : public LocalSLListImpl<ArrayPool<T>,T,U> {
public:
  LocalSLList(ArrayPool<T> & p, typename SLList<T,U>::Head & _src)
    : LocalSLListImpl<ArrayPool<T>,T,U>(p, _src) {}
};


#endif
