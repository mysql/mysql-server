/*
   Copyright (C) 2005, 2006 MySQL AB, 2009 Sun Microsystems, Inc.
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

#ifndef DLC_FIFOLIST_HPP
#define DLC_FIFOLIST_HPP

#include "DLFifoList.hpp"
#include <NdbOut.hpp>

// Adds "count" to DLFifoList
template <class T, class U = T>
class DLCFifoList : protected DLFifoList<T, U> {
public:
  // List head
  struct Head : public DLFifoList<T, U>::Head {
    Head() : m_count(0) {}
    Uint32 m_count;
  };
  
  // Ctor
  DLCFifoList(ArrayPool<T> & thePool) :
    DLFifoList<T, U>(thePool)
  {}

  // Get count
  Uint32 count() const { return head.m_count; }

  // Redefine methods which do add or remove
  
  bool seize(Ptr<T>& ptr) {
    if (DLFifoList<T, U>::seize(ptr)) {
      head.m_count++;
      return true;
    }
    return false;
  }

  bool seizeFirst(Ptr<T> & ptr) {
    if (DLFifoList<T, U>::seizeFirst(ptr)) {
      head.m_count++;
      return true;
    }
    return false;
  }
  bool seizeLast(Ptr<T> & ptr) {
    if (DLFifoList<T, U>::seizeLast(ptr)) {
      head.m_count++;
      return true;
    }
    return false;
  }

  bool seizeId(Ptr<T>& ptr, Uint32 i) {
    if (DLFifoList<T, U>::seizeId(ptr)) {
      head.m_count++;
      return true;
    }
    return false;
  }
  
  void add(Ptr<T>& ptr) {
    DLFifoList<T, U>::add(ptr);
    head.m_count++;
  }

  void addFirst(Ptr<T> & ptr) {
    DLFifoList<T, U>::addFirst(ptr);
    head.m_count++;
  }

  void addLast(Ptr<T> & ptr) {
    DLFifoList<T, U>::addLast(ptr);
    head.m_count++;
  }

  void insert(Ptr<T> & ptr, Ptr<T>& loc) {
    DLFifoList<T, U>::insert(ptr, loc);
    head.m_count++;
  }

  void remove(T* t) {
    DLFifoList<T, U>::remove(t);
    head.m_count--;
  }

  void remove(Ptr<T>& ptr) {
    DLFifoList<T, U>::remove(ptr);
    head.m_count--;
  }

  void release(Uint32 i) {
    DLFifoList<T, U>::release(i);
    head.m_count--;
  }
  
  void release(Ptr<T>& ptr) {
    DLFifoList<T, U>::release(ptr);
    head.m_count--;
  }

  void release() {
    DLFifoList<T, U>::release();
    head.m_count = 0;
  }

  void getPtr(Ptr<T> & ptr, Uint32 i) const {
    DLFifoList<T, U>::getPtr(ptr, i);
  }

  void getPtr(Ptr<T> & ptr) const {
    DLFifoList<T, U>::getPtr(ptr);
  }

  T * getPtr(Uint32 i) const {
    return DLFifoList<T, U>::getPtr(i);
  }

  bool first(Ptr<T> & ptr) const {
    return DLFifoList<T, U>::first(ptr);
  }

  bool last(Ptr<T> & ptr) const {
    return DLFifoList<T, U>::last(ptr);
  }

  bool next(Ptr<T> & ptr) const {
    return DLFifoList<T, U>::next(ptr);
  }

  bool prev(Ptr<T> & ptr) const {
    return DLFifoList<T, U>::prev(ptr);
  }

  bool hasNext(const Ptr<T> & ptr) const {
    return DLFifoList<T, U>::hasNext(ptr);
  }

  bool hasPrev(const Ptr<T> & ptr) const {
    return DLFifoList<T, U>::hasPrev(ptr);
  }

  inline bool isEmpty() const {
    return DLFifoList<T, U>::isEmpty();
  }

  DLCFifoList<T>& operator=(const DLCFifoList<T>& src){
    assert(&this->thePool == &src.thePool);
    this->head = src.head;
    return * this;
  }

protected:
  Head head;
};

// Local variant
template <class T, class U = T>
class LocalDLCFifoList : public DLCFifoList<T, U> {
public:
  LocalDLCFifoList(ArrayPool<T> & thePool,
                   typename DLCFifoList<T, U>::Head &_src)
    : DLCFifoList<T, U>(thePool), src(_src)
  {
    this->head = src;
#ifdef VM_TRACE
    assert(src.in_use == false);
    src.in_use = true;
#endif
  }
  
  ~LocalDLCFifoList() {
#ifdef VM_TRACE
    assert(src.in_use == true);
#endif
    src = this->head;
  }
private:
  typename DLCFifoList<T, U>::Head & src;
};

#endif
