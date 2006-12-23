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

#ifndef ARRAY_HPP
#define ARRAY_HPP

#include "ArrayPool.hpp"

#include <pc.hpp>
#include <ErrorReporter.hpp>

/**
 * Template class used for implementing an
 *   array of object retreived from a pool
 */
template <class T>
class Array {
public:
  Array(ArrayPool<T> & thePool);

  /**
   * Allocate an <b>n</b> objects from pool
   *   These can now be addressed with 0 <= ptr.i < n
   */
  bool seize(Uint32 i);

  /**
   * Release all object from array
   */
  void release();

  /**
   * Return current size of array
   */
  Uint32 getSize() const;

  /**
   * empty
   */
  inline bool empty() const { return sz == 0;}

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
  T * getPtr(Uint32 i) const;
  
private:
  Uint32 base, sz;
  ArrayPool<T> & thePool;
};  

template<class T>
inline
Array<T>::Array(ArrayPool<T> & _pool)
  :  thePool(_pool)
{
  sz = 0;
  base = RNIL;
}

template<class T>
inline
bool
Array<T>::seize(Uint32 n){
  if(base == RNIL && n > 0){
    base = thePool.seizeN(n);
    if(base != RNIL){
      sz = n;
      return true;
    }
    return false;
  }
  ErrorReporter::handleAssert("Array<T>::seize failed", __FILE__, __LINE__);
  return false;
}

template<class T>
inline
void 
Array<T>::release(){
  if(base != RNIL){
    thePool.releaseN(base, sz);
    sz = 0;
    base = RNIL;
    return;
  }
}

template<class T>
inline
Uint32 
Array<T>::getSize() const {
  return sz;
}

template <class T>
inline
void 
Array<T>::getPtr(Ptr<T> & p, Uint32 i) const {
  p.i = i;
#ifdef ARRAY_GUARD
  if(i < sz && base != RNIL){
    p.p = thePool.getPtr(i + base);
    return;
  } else {
  ErrorReporter::handleAssert("Array::getPtr failed", __FILE__, __LINE__);
  }
#endif
  p.p = thePool.getPtr(i + base);
}

template<class T>
inline
void
Array<T>::getPtr(Ptr<T> & ptr) const {
#ifdef ARRAY_GUARD
  if(ptr.i < sz && base != RNIL){
    ptr.p = thePool.getPtr(ptr.i + base);
    return;
  } else {
    ErrorReporter::handleAssert("Array<T>::getPtr failed", __FILE__, __LINE__);
  }
#endif
  ptr.p = thePool.getPtr(ptr.i + base);
}

template<class T>
inline
T * 
Array<T>::getPtr(Uint32 i) const {
#ifdef ARRAY_GUARD
  if(i < sz && base != RNIL){
    return thePool.getPtr(i + base);
  } else {
    ErrorReporter::handleAssert("Array<T>::getPtr failed", __FILE__, __LINE__);
  }
#endif
  return thePool.getPtr(i + base);
}


#endif
