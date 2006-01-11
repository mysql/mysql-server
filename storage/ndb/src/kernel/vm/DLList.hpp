/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

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
#include <NdbOut.hpp>

/**
 * Template class used for implementing an
 *   list of object retreived from a pool
 */
template <class T, class U = T>
class DLList {
public:
  /**
   * List head
   */
  struct HeadPOD {
    Uint32 firstItem;
    inline bool isEmpty() const { return firstItem == RNIL; }
    inline void init () { firstItem = RNIL; }
  };

  struct Head : public HeadPOD {
    Head();
    Head& operator=(const HeadPOD& src) {
      this->firstItem = src.firstItem;
      return *this;
    }
  };
  
  DLList(ArrayPool<T> & thePool);
  
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
    Uint32 i = head.firstItem;
    while(i != RNIL){
      T * t = thePool.getPtr(i);
      t->print(out); out << " ";
      i = t->U::nextList;
    }
  }

  inline bool isEmpty() const { return head.firstItem == RNIL;}
  
protected:
  Head head;
  ArrayPool<T> & thePool;
};

template <class T, class U = T>
class LocalDLList : public DLList<T,U> {
public:
  LocalDLList(ArrayPool<T> & thePool, typename DLList<T,U>::HeadPOD & _src)
    : DLList<T,U>(thePool), src(_src)
  {
    this->head = src;
  }
  
  ~LocalDLList(){
    src = this->head;
  }
private:
  typename DLList<T,U>::HeadPOD & src;
};

template <class T, class U>
inline
DLList<T,U>::DLList(ArrayPool<T> & _pool):
  thePool(_pool){
}

template <class T, class U>
inline
DLList<T,U>::Head::Head(){
  this->init();
}

/**
 * Allocate an object from pool - update Ptr
 *
 * Return i
 */
template <class T, class U>
inline
bool
DLList<T,U>::seize(Ptr<T> & p){
  if(thePool.seize(p)){
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
template <class T, class U>
inline
bool
DLList<T,U>::seizeId(Ptr<T> & p, Uint32 ir){
  if(thePool.seizeId(p, ir)){
    add(p);
    return true;
  }
  return false;
}

template <class T, class U>
inline
bool
DLList<T,U>::findId(Uint32 i) const {
  return thePool.findId(i);
}

template <class T, class U>
inline
void 
DLList<T,U>::add(Ptr<T> & p){
  T * t = p.p;
  Uint32 ff = head.firstItem;
  
  t->U::nextList = ff;
  t->U::prevList = RNIL;
  head.firstItem = p.i;
  
  if(ff != RNIL){
    T * t2 = thePool.getPtr(ff);
    t2->U::prevList = p.i;
  }
}

template <class T, class U>
inline
void 
DLList<T,U>::add(Uint32 first, Ptr<T> & lastPtr)
{
  Uint32 ff = head.firstItem;

  head.firstItem = first;
  lastPtr.p->U::nextList = ff;
  
  if(ff != RNIL){
    T * t2 = thePool.getPtr(ff);
    t2->U::prevList = lastPtr.i;
  }
}

template <class T, class U>
inline
void 
DLList<T,U>::remove(Ptr<T> & p){
  remove(p.p);
}

template <class T, class U>
inline
void 
DLList<T,U>::remove(T * p){
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
template <class T, class U>
inline
void 
DLList<T,U>::release(Uint32 i){
  Ptr<T> p;
  p.i = i;
  p.p = thePool.getPtr(i);
  release(p);
}
  
/**
 * Return an object to pool
 */
template <class T, class U>
inline
void 
DLList<T,U>::release(Ptr<T> & p){
  remove(p);
  thePool.release(p.i);
}  

template <class T, class U>
inline
void 
DLList<T,U>::release(){
  while(head.firstItem != RNIL){
    const T * t = thePool.getPtr(head.firstItem);
    const Uint32 i = head.firstItem;
    head.firstItem = t->U::nextList;
    thePool.release(i);
  }
}  

template <class T, class U>
inline
void 
DLList<T,U>::remove(){
  head.firstItem = RNIL;
}  

template <class T, class U>
inline
void 
DLList<T,U>::getPtr(Ptr<T> & p, Uint32 i) const {
  p.i = i;
  p.p = thePool.getPtr(i);
}

template <class T, class U>
inline
void 
DLList<T,U>::getPtr(Ptr<T> & p) const {
  thePool.getPtr(p);
}
  
template <class T, class U>
inline
T * 
DLList<T,U>::getPtr(Uint32 i) const {
  return thePool.getPtr(i);
}

/**
 * Update ptr to first element in list
 *
 * Return i
 */
template <class T, class U>
inline
bool
DLList<T,U>::first(Ptr<T> & p) const {
  Uint32 i = head.firstItem;
  p.i = i;
  if(i != RNIL){
    p.p = thePool.getPtr(i);
    return true;
  }
  p.p = NULL;
  return false;
}

template <class T, class U>
inline
bool
DLList<T,U>::next(Ptr<T> & p) const {
  Uint32 i = p.p->U::nextList;
  p.i = i;
  if(i != RNIL){
    p.p = thePool.getPtr(i);
    return true;
  }
  p.p = NULL;
  return false;
}

template <class T, class U>
inline
bool
DLList<T,U>::hasNext(const Ptr<T> & p) const {
  return p.p->U::nextList != RNIL;
}

#endif
