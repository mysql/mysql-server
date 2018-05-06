/*
   Copyright (c) 2003, 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef ARRAY_HPP
#define ARRAY_HPP

#include "ArrayPool.hpp"

#include <pc.hpp>
#include <ErrorReporter.hpp>

#define JAM_FILE_ID 227


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



#undef JAM_FILE_ID

#endif
