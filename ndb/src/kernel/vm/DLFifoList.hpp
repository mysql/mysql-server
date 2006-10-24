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

#ifndef DLFIFOLIST_HPP
#define DLFIFOLIST_HPP

#include "ArrayPool.hpp"
#include <NdbOut.hpp>

/**
 * Template class used for implementing an
 *   list of object retreived from a pool
 */
template <class T>
class DLFifoList {
public:
  /**
   * List head
   */
  struct Head {
    Head();
    Uint32 firstItem;
    Uint32 lastItem;

    inline bool isEmpty() const { return firstItem == RNIL;}
  };
  
  DLFifoList(ArrayPool<T> & thePool);
  
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
   * Add object to list 
   * 
   * @NOTE MUST be seized from correct pool
   */
  void add(Ptr<T> &);

  /**
   * Remove from list 
   */
  void remove(Ptr<T> &);

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
   * Check if prev exists i.e. this is not first
   *
   * NOTE ptr must be both p & i
   */
  bool hasPrev(const Ptr<T> &) const;

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
      out << (unsigned int) t << "[" << i << "]:"; 
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
class LocalDLFifoList : public DLFifoList<T> {
public:
  LocalDLFifoList(ArrayPool<T> & thePool, typename DLFifoList<T>::Head & _src)
    : DLFifoList<T>(thePool), src(_src)
  {
    this->head = src;
  }
  
  ~LocalDLFifoList(){
    src = this->head;
  }
private:
  typename DLFifoList<T>::Head & src;
};

template <class T>
inline
DLFifoList<T>::DLFifoList(ArrayPool<T> & _pool):
  thePool(_pool){
}

template<class T>
inline
DLFifoList<T>::Head::Head(){
  firstItem = RNIL;
  lastItem = RNIL;
}

/**
 * Allocate an object from pool - update Ptr
 *
 * Return i
 */
template <class T>
inline
bool
DLFifoList<T>::seize(Ptr<T> & p){
  thePool.seize(p);
  if (p.i != RNIL) {
    add(p);
    return true;
  }
  p.p = NULL;
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
DLFifoList<T>::seizeId(Ptr<T> & p, Uint32 ir){
  thePool.seizeId(p, ir);
  if(p.i != RNIL){
    add(p);
    return true;
  }
  p.p = NULL;
  return false;
}

template <class T>
inline
void
DLFifoList<T>::add(Ptr<T> & p){
  T * t = p.p;
  Uint32 last = head.lastItem;

  if(p.i == RNIL)
    ErrorReporter::handleAssert("DLFifoList<T>::add", __FILE__, __LINE__);
  
  t->nextList = RNIL;
  t->prevList = last;
  if (head.firstItem == RNIL)
    head.firstItem = p.i;
  head.lastItem = p.i;    
  
  if(last != RNIL){
    T * t2 = thePool.getPtr(last);
    t2->nextList = p.i;
  }
}

/**
 * Return an object to pool
 */
template <class T>
inline
void 
DLFifoList<T>::release(Uint32 i){
  Ptr<T> p;
  p.i = i;
  p.p = thePool.getPtr(i);
  release(p);
}

template <class T>
inline
void 
DLFifoList<T>::remove(Ptr<T> & p){
  T * t = p.p;
  Uint32 ni = t->nextList;
  Uint32 pi = t->prevList;

  if(ni != RNIL){
    T * t = thePool.getPtr(ni);
    t->prevList = pi;
  } else {
    // We are releasing last
    head.lastItem = pi;
  }
  
  if(pi != RNIL){
    T * t = thePool.getPtr(pi);
    t->nextList = ni;
  } else {
    // We are releasing first
    head.firstItem = ni;
  }
}
  
/**
 * Return an object to pool
 */
template <class T>
inline
void 
DLFifoList<T>::release(Ptr<T> & p){
  remove(p);
  thePool.release(p.i);
}  

template <class T>
inline
void 
DLFifoList<T>::release(){
  Ptr<T> p;
  while(head.firstItem != RNIL){
    p.i = head.firstItem;  
    p.p = thePool.getPtr(head.firstItem);    
    T * t = p.p;
    head.firstItem = t->nextList;
    release(p);
  }
}  

template <class T>
inline
void 
DLFifoList<T>::getPtr(Ptr<T> & p, Uint32 i) const {
  p.i = i;
  p.p = thePool.getPtr(i);
}

template <class T>
inline
void 
DLFifoList<T>::getPtr(Ptr<T> & p) const {
  thePool.getPtr(p);
}
  
template <class T>
inline
T * 
DLFifoList<T>::getPtr(Uint32 i) const {
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
DLFifoList<T>::first(Ptr<T> & p) const {
  p.i = head.firstItem;
  if(p.i != RNIL){
    p.p = thePool.getPtr(p.i);
    return true;
  }
  p.p = NULL;
  return false;
}

template <class T>
inline
bool
DLFifoList<T>::next(Ptr<T> & p) const {
  p.i = p.p->nextList;
  if(p.i != RNIL){
    p.p = thePool.getPtr(p.i);
    return true;
  }
  p.p = NULL;
  return false;
}

template <class T>
inline
bool
DLFifoList<T>::hasNext(const Ptr<T> & p) const {
  return p.p->nextList != RNIL;
}

template <class T>
inline
bool
DLFifoList<T>::hasPrev(const Ptr<T> & p) const {
  return p.p->prevList != RNIL;
}

#endif
