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
template <class T>
class DLList {
public:
  /**
   * List head
   */
  struct Head {
    Head();
    Uint32 firstItem;
    inline bool isEmpty() const { return firstItem == RNIL; }
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
   * Allocate <b>n</b>objects from pool
   *
   * Return i value of first object allocated or RNIL if fails
   */
  bool seizeN(Ptr<T> &, Uint32 n);

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
   * Add object to list 
   * 
   * @NOTE MUST be seized from correct pool
   */
  void add(Ptr<T> &);
  
  /**
   * Remove object from list
   *
   * @NOTE Does not return it to pool
   */
  void remove(Ptr<T> &);
  
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
      i = t->nextList;
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
      i = t->nextList;
    }
  }

  inline bool isEmpty() const { return head.firstItem == RNIL;}
  
protected:
  Head head;
  ArrayPool<T> & thePool;
};

template<class T>
class LocalDLList : public DLList<T> {
public:
  LocalDLList(ArrayPool<T> & thePool, typename DLList<T>::Head & _src)
    : DLList<T>(thePool), src(_src)
  {
    this->head = src;
  }
  
  ~LocalDLList(){
    src = this->head;
  }
private:
  typename DLList<T>::Head & src;
};

template <class T>
inline
DLList<T>::DLList(ArrayPool<T> & _pool):
  thePool(_pool){
}

template<class T>
inline
DLList<T>::Head::Head(){
  firstItem = RNIL;
}

/**
 * Allocate an object from pool - update Ptr
 *
 * Return i
 */
template <class T>
inline
bool
DLList<T>::seize(Ptr<T> & p){
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
template <class T>
inline
bool
DLList<T>::seizeId(Ptr<T> & p, Uint32 ir){
  if(thePool.seizeId(p, ir)){
    add(p);
    return true;
  }
  return false;
}

template <class T>
inline
bool
DLList<T>::findId(Uint32 i) const {
  return thePool.findId(i);
}

template <class T>
inline
void 
DLList<T>::add(Ptr<T> & p){
  T * t = p.p;
  Uint32 ff = head.firstItem;
  
  t->nextList = ff;
  t->prevList = RNIL;
  head.firstItem = p.i;
  
  if(ff != RNIL){
    T * t2 = thePool.getPtr(ff);
    t2->prevList = p.i;
  }
}

template <class T>
inline
void 
DLList<T>::remove(Ptr<T> & p){
  T * t = p.p;
  Uint32 ni = t->nextList;
  Uint32 pi = t->prevList;

  if(ni != RNIL){
    T * t = thePool.getPtr(ni);
    t->prevList = pi;
  }
  
  if(pi != RNIL){
    T * t = thePool.getPtr(pi);
    t->nextList = ni;
  } else {
    head.firstItem = ni;
  }
}

/**
 * Return an object to pool
 */
template <class T>
inline
void 
DLList<T>::release(Uint32 i){
  Ptr<T> p;
  p.i = i;
  p.p = thePool.getPtr(i);
  release(p);
}
  
/**
 * Return an object to pool
 */
template <class T>
inline
void 
DLList<T>::release(Ptr<T> & p){
  remove(p);
  thePool.release(p.i);
}  

template <class T>
inline
void 
DLList<T>::release(){
  while(head.firstItem != RNIL){
    const T * t = thePool.getPtr(head.firstItem);
    const Uint32 i = head.firstItem;
    head.firstItem = t->nextList;
    thePool.release(i);
  }
}  

template <class T>
inline
void 
DLList<T>::getPtr(Ptr<T> & p, Uint32 i) const {
  p.i = i;
  p.p = thePool.getPtr(i);
}

template <class T>
inline
void 
DLList<T>::getPtr(Ptr<T> & p) const {
  thePool.getPtr(p);
}
  
template <class T>
inline
T * 
DLList<T>::getPtr(Uint32 i) const {
  return thePool.getPtr(i);
}

/**
 * Update ptr to first element in list
 *
 * Return i
 */
template <class T>
inline
bool
DLList<T>::first(Ptr<T> & p) const {
  Uint32 i = head.firstItem;
  p.i = i;
  if(i != RNIL){
    p.p = thePool.getPtr(i);
    return true;
  }
  p.p = NULL;
  return false;
}

template <class T>
inline
bool
DLList<T>::next(Ptr<T> & p) const {
  Uint32 i = p.p->nextList;
  p.i = i;
  if(i != RNIL){
    p.p = thePool.getPtr(i);
    return true;
  }
  p.p = NULL;
  return false;
}

template <class T>
inline
bool
DLList<T>::hasNext(const Ptr<T> & p) const {
  return p.p->nextList != RNIL;
}

#endif
