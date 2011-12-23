/*
   Copyright (C) 2003-2006, 2011 Oracle and/or its affiliates. All rights reserved.

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
 * SLMList implements a intrusive list using chaining
 *   (with a single link)
 *
 * The entries in the (uninstansiated) meta class passed to the
 * list must have the following methods:
 *
 *  -# nextList(U&) returning a reference to the next link
 */

template <typename T, typename U = T> struct SLListDefaultMethods {
static inline Uint32& nextList(U& t) { return t.nextList; }
};

template <typename P, typename T, typename M = SLListDefaultMethods<T> >
class SLMList
{
public:
  explicit SLMList(P& thePool);
  ~SLMList() { }
private:
  SLMList(const SLMList&);
  SLMList&  operator=(const SLMList&);

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
    M::nextList(*p.p) = head.firstItem;
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
      i = M::nextList(*t);
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

template <typename P, typename T, typename M = SLListDefaultMethods<T> >
class LocalSLMList : public SLMList<P, T, M>
{
public:
  LocalSLMList(P& thePool, typename SLMList<P, T, M>::HeadPOD& _src)
    : SLMList<P, T, M>(thePool), src(_src)
  {
    this->head = src;
  }
  
  ~LocalSLMList(){
    src = this->head;
  }
private:
  typename SLMList<P, T, M>::HeadPOD& src;
};

template <typename P, typename T, typename M>
inline
SLMList<P, T, M>::SLMList(P& _pool):
  thePool(_pool)
{
}

template <typename P, typename T, typename M>
inline
SLMList<P, T, M>::Head::Head()
{
  this->init();
}

template <typename P, typename T, typename M>
inline
bool
SLMList<P, T, M>::seize(Ptr<T>& p)
{
  thePool.seize(p);
  T * t = p.p;
  Uint32 ff = head.firstItem;
  if(p.i != RNIL)
  {
    M::nextList(*t) = ff;
    head.firstItem = p.i;
    return true;
  }
  return false;
}

template <typename P, typename T, typename M>
inline
bool
SLMList<P, T, M>::seizeId(Ptr<T>& p, Uint32 ir)
{
  thePool.seizeId(p, ir);
  T * t = p.p;
  Uint32 ff = head.firstItem;
  if(p.i != RNIL)
  {
    M::nextList(*t) = ff;
    head.firstItem = p.i;
    return true;
  }
  return false;
}

template <typename P, typename T, typename M>
inline
bool
SLMList<P, T, M>::seizeN(Ptr<T>& p, Uint32 n)
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
         head.firstItem = M::nextList(*p.p);
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


template <typename P, typename T, typename M>
inline
void 
SLMList<P, T, M>::remove()
{
  head.firstItem = RNIL;
}  

template <typename P, typename T, typename M>
inline
bool
SLMList<P, T, M>::remove_front(Ptr<T>& p)
{
  p.i = head.firstItem;
  if (p.i != RNIL)
  {
    p.p = thePool.getPtr(p.i);
    head.firstItem = M::nextList(*p.p);
    return true;
  }
  return false;
}  

template <typename P, typename T, typename M>
inline
void
SLMList<P, T, M>::add(Uint32 first, Ptr<T>& last)
{
  M::nextList(*last.p) = head.firstItem;
  head.firstItem = first;
}  

template <typename P, typename T, typename M>
inline
void 
SLMList<P, T, M>::release()
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
SLMList<P, T, M>::getPtr(Ptr<T>& p, Uint32 i) const
{
  p.i = i;
  p.p = thePool.getPtr(i);
}

template <typename P, typename T, typename M>
inline
void 
SLMList<P, T, M>::getPtr(Ptr<T>& p) const
{
  thePool.getPtr(p);
}
  
template <typename P, typename T, typename M>
inline
T * 
SLMList<P, T, M>::getPtr(Uint32 i) const
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
SLMList<P, T, M>::first(Ptr<T>& p) const
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
SLMList<P, T, M>::next(Ptr<T>& p) const
{
  Uint32 i = M::nextList(*p.p);
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
SLMList<P, T, M>::hasNext(const Ptr<T>& p) const
{
  return M::nextList(*p.p) != RNIL;
}

// Specializations

template <typename P, typename T, typename U = T, typename M = SLListDefaultMethods<T, U> >
class SLListImpl : public SLMList<P, T, M>
{
public:
  SLListImpl(P& p) : SLMList<P, T, M>(p) {}
};

template <typename P, typename T, typename U = T, typename M = SLListDefaultMethods<T, U> >
class LocalSLListImpl : public LocalSLMList<P, T, M> {
public:
  LocalSLListImpl(P& p, typename SLMList<P, T, M>::Head& _src)
    : LocalSLMList<P, T, M>(p, _src) {}
};

//

template <typename T, typename U = T, typename M = SLListDefaultMethods<T, U> >
class SLList : public SLMList<ArrayPool<T>, T, M>
{
public:
  SLList(ArrayPool<T>& p) : SLMList<ArrayPool<T>, T, M>(p) {}
};

template <typename T, typename U = T, typename M = SLListDefaultMethods<T, U> >
class LocalSLList : public LocalSLMList<ArrayPool<T>, T, M> {
public:
  LocalSLList(ArrayPool<T>& p, typename SLMList<ArrayPool<T>, T, M>::Head& _src)
    : LocalSLMList<ArrayPool<T>, T, M>(p, _src) {}
};


#endif
