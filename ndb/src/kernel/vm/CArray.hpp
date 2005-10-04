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

#ifndef CARRAY_HPP
#define CARRAY_HPP

/**
 * Template class used for implementing an c - array
 */
template <class T>
class CArray {
public:
  CArray();
  ~CArray();
  
  /**
   * Set the size of the pool
   *
   * Note, can currently only be called once
   */
  bool setSize(Uint32 noOfElements, bool exit_on_error = true);

  /**
   * Get size
   */
  Uint32 getSize() const;
  
  /**
   * Update p value for ptr according to i value 
   */
  void getPtr(Ptr<T> &) const;
  
  /**
   * Get pointer for i value
   */
  T * getPtr(Uint32 i) const;

  /**
   * Update p & i value for ptr according to <b>i</b> value 
   */
  void getPtr(Ptr<T> &, Uint32 i) const;

private:
  Uint32 size;
  T * theArray;
};

template <class T>
inline
CArray<T>::CArray(){
  size = 0;
  theArray = 0;
}

template <class T>
inline
CArray<T>::~CArray(){
  if(theArray != 0){
    NdbMem_Free(theArray);
    theArray = 0;
  }
}

/**
 * Set the size of the pool
 *
 * Note, can currently only be called once
 */
template <class T>
inline
bool
CArray<T>::setSize(Uint32 noOfElements, bool exit_on_error){
  if(size == noOfElements)
    return true;
  
  theArray = (T *)NdbMem_Allocate(noOfElements * sizeof(T));
  if(theArray == 0)
  {
    if (!exit_on_error)
      return false;
    ErrorReporter::handleAssert("CArray<T>::setSize malloc failed",
				__FILE__, __LINE__, NDBD_EXIT_MEMALLOC);
    return false; // not reached
  }
  size = noOfElements;
  return true;
}

template<class T>
inline
Uint32
CArray<T>::getSize() const {
  return size;
}

template <class T>
inline
void
CArray<T>::getPtr(Ptr<T> & ptr) const {
  const Uint32 i = ptr.i;
  if(i < size){
    ptr.p = &theArray[i];
    return;
  } else {
    ErrorReporter::handleAssert("CArray<T>::getPtr", __FILE__, __LINE__);
  }
}
  
template <class T>
inline
T * 
CArray<T>::getPtr(Uint32 i) const {
  if(i < size){
    return &theArray[i];
  } else {
    ErrorReporter::handleAssert("CArray<T>::getPtr", __FILE__, __LINE__);
    return 0;
  }
}

template <class T>
inline
void
CArray<T>::getPtr(Ptr<T> & ptr, Uint32 i) const {
  ptr.i = i;
  if(i < size){
    ptr.p = &theArray[i];
    return;
  } else {
    ErrorReporter::handleAssert("CArray<T>::getPtr", __FILE__, __LINE__);
  }
}

#endif
