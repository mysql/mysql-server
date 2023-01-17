/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.

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

#ifndef FOR_LIB_POOL_H
#define FOR_LIB_POOL_H

#define JAM_FILE_ID 398


 
//===========================================================================
//
// .PUBLIC
//
//===========================================================================
 
////////////////////////////////////////////////////////////////
//
// enum { defInitSize = 256, defIncSize  = 64 }; 
// Description: type to store initial and incremental size in.
//
////////////////////////////////////////////////////////////////
//
// Pool(int anInitSize = defInitSize, int anIncSize = defIncSize);
// Description:
//   Constructor. Allocates anInitSize of objects <template argument>.
//   When the pool runs out of elements, anIncSize elements are added to the
//   pool. (When the pool is not optimized to allocate multiple elements
//   more efficient, the anIncSize MUST be set to 1 to get the best
//   performance...
//
// Parameters:
//   defInitSize: Initial size of the pool (# of elements in the pool)
//   defIncSize:  # of elements added to the pool when a request to an empty
//    pool is made.
// Return value:
//      _ 
// Errors:
//      -
// Asserts:
//   _
//
////////////////////////////////////////////////////////////////
//
// virtual ~Pool();
// Description:
//   Elements in the pool are all deallocated.
// Parameters:
//   _
// Return value:
//   _
// Errors:
//      -
// Asserts:
//   theEmptyNodeList==0. No elements are in still in use.
//
////////////////////////////////////////////////////////////////
//
// T& get();
// Description:
//   gets an element from the Pool.
// Parameters:
//   _
// Return value:
//   T& the element extracted from the Pool. (element must be cleared to
//   mimic newly created element)
// Errors:
//      -
// Asserts:
//   _
//
////////////////////////////////////////////////////////////////
//
// void put(T& aT);
// Description:
//   Returns an element to the pool.
// Parameters:
//   aT The element to put back in the pool
// Return value:
//   void 
// Errors:
//      -
// Asserts:
//   The pool has "empty" elements, to put element back in...
//
//===========================================================================
//
// .PRIVATE
//
//===========================================================================
 
////////////////////////////////////////////////////////////////
//
// void allocate(int aSize);
// Description:
//   add aSize elements to the pool
// Parameters:
//   aSize: # of elements to add to the pool
// Return value:
//   void 
// Errors:
//      -
// Asserts:
//   _
//
////////////////////////////////////////////////////////////////
//
// void deallocate();
// Description:
//   frees all elements kept in the pool.
// Parameters:
//   _
// Return value:
//   void 
// Errors:
//      -
// Asserts:
//   No elements are "empty" i.e. in use.
//
//===========================================================================
//
// .PRIVATE
//
//===========================================================================
 
////////////////////////////////////////////////////////////////
//
// Pool<T>& operator=(const Pool<T>& cp);
// Description:
//   Prohibit use of assignment operator.
// Parameters:
//   cp
// Return value:
//   Pool<T>& 
// Asserts:
//   _
//
////////////////////////////////////////////////////////////////
//
// Pool(const Pool<T>& cp);
// Description:
//   Prohibit use of default copy constructor.
// Parameters:
//   cp
// Return value:
//   _
// Errors:
//      -
// Asserts:
//   _
//
////////////////////////////////////////////////////////////////
//
// int initSize;
// Description: size of the initial size of the pool
//
////////////////////////////////////////////////////////////////
//
// int incSize;
// Description: # of elements added to the pool when pool is exhausted.
//
////////////////////////////////////////////////////////////////
//
// PoolElement<T>* theFullNodeList;
// Description: List to contain all "unused" elements in the pool
//
////////////////////////////////////////////////////////////////
//
// PoolElement<T>* theEmptyNodeList;
// Description: List to contain all "in use" elements in the pool
//
//-------------------------------------------------------------------------

template <class T>
class Pool
{
public:
  enum { defInitSize = 256, defIncSize  = 64 }; 
   
  Pool(int anInitSize = defInitSize, int anIncSize = defIncSize) :
    theIncSize(anIncSize),
    theTop(0),
    theCurrentSize(0),   
    theList(0)
  {
    allocate(anInitSize);
  }
  
  virtual ~Pool(void)
  {
    for (int i=0; i <theTop ; ++i)
      delete theList[i];
    
    delete []theList;
  }
  
  T* get();
  void put(T* aT);

  // size() : Return number free items in pool
  unsigned size(){ return theTop; }

  // inuse() : Return number items taken from pool
  unsigned inuse() const { return theCurrentSize - theTop; }

  const T* peekInuseItem(unsigned idx) const;
  
protected:
  void allocate(int aSize)
  {
    T** tList = theList;
    int i;
    theList = new T*[aSize+theCurrentSize];
    // allocate full list
    for (i = 0; i < theTop; i++) {
      theList[i] = tList[i];
    }
    delete []tList;
    for (; (theTop < aSize); theTop++){
      theList[theTop] = (T*)new T;
    }
    theCurrentSize += aSize;
  }
  
private:
  Pool<T>& operator=(const Pool<T>& cp);
  Pool(const Pool<T>& cp);
  
  int theIncSize;
  int theTop;
  int theCurrentSize;
  
  T** theList;
};

//******************************************************************************
template <class T> inline T* Pool<T>::get()
{
   T* tmp;
   if( theTop == 0 )
   {
      allocate(theIncSize);
   }
   --theTop;
   tmp = theList[theTop];
   tmp->atGet();
   return tmp;
}

//
//******************************************************************************
template <class T> inline void Pool<T>::put(T* aT)
{
   theList[theTop]= aT;
   ++theTop;
}

template <class T> const T* Pool<T>::peekInuseItem(unsigned idx) const
{
  assert(idx <= inuse());
  // theTop is index of the first item in use
  return theList[theTop + idx];
}




#undef JAM_FILE_ID

#endif
