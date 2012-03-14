/*
   Copyright (C) 2003-2006, 2008 MySQL AB, 2008-2010 Sun Microsystems, Inc.
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

#ifndef ARRAY_POOL_HPP
#define ARRAY_POOL_HPP

#include <ndb_global.h>
#include "ndbd_malloc.hpp"

#include <pc.hpp>
#include "Pool.hpp"
#include <ErrorReporter.hpp>
#include <NdbMem.h>
#include <Bitmask.hpp>
#include <mgmapi.h>

#include <NdbMutex.h>

template <class T> class Array;

//#define ARRAY_CHUNK_GUARD

/**
 * Template class used for implementing an
 *   pool of object (in an array with a free list)
 */
template <class T>
class ArrayPool {
public:
  ArrayPool();
  ~ArrayPool();
  
  /**
   * Set the size of the pool
   *
   * Note, can currently only be called once
   */
  bool setSize(Uint32 noOfElements, bool align = false, bool exit_on_error = true, 
               bool guard = true, Uint32 paramId = 0);
  bool set(T*, Uint32 cnt, bool align = false);
  void clear() { theArray = 0; }

  inline Uint32 getNoOfFree() const {
    return noOfFree;
  }

  inline Uint32 getSize() const {
    return size;
  }

  inline Uint32 getUsed() const {
    return size - noOfFree;
  }

  inline Uint32 getUsedHi() const {
    return size - noOfFreeMin;
  }

  inline void updateFreeMin(void) {
    if (noOfFree < noOfFreeMin)
      noOfFreeMin = noOfFree;
  }

  inline void decNoFree(void) {
    assert(noOfFree > 0);
    noOfFree--;
    updateFreeMin();
  }

  inline void decNoFree(Uint32 cnt) {
    assert(noOfFree >= cnt);
    noOfFree -= cnt;
    updateFreeMin();
  }

  inline Uint32 getEntrySize() const {
    return sizeof(T);
  }

  /**
   * Update p value for ptr according to i value 
   */
  void getPtr(Ptr<T> &);
  void getPtr(ConstPtr<T> &) const;
  void getPtr(Ptr<T> &, bool CrashOnBoundaryError);
  void getPtr(ConstPtr<T> &, bool CrashOnBoundaryError) const;
  
  /**
   * Get pointer for i value
   */
  T * getPtr(Uint32 i);
  const T * getConstPtr(Uint32 i) const;
  T * getPtr(Uint32 i, bool CrashOnBoundaryError);
  const T * getConstPtr(Uint32 i, bool CrashOnBoundaryError) const;

  /**
   * Update p & i value for ptr according to <b>i</b> value 
   */
  void getPtr(Ptr<T> &, Uint32 i);
  void getPtr(ConstPtr<T> &, Uint32 i) const;
  void getPtr(Ptr<T> &, Uint32 i, bool CrashOnBoundaryError);
  void getPtr(ConstPtr<T> &, Uint32 i, bool CrashOnBoundaryError) const;

  /**
   * Allocate an object from pool - update Ptr
   *
   * Return i
   */
  bool seize(Ptr<T> &);

  /**
   * Allocate object <b>i</b> from pool - update Ptr
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

#ifdef ARRAY_GUARD
  /**
   *  Checks if i is a correct seized record
   *
   *  @note Since this is either an expensive method, 
   *        or needs bitmask stuff, this method is only 
   *        recommended for debugging.
   *
   */
  bool isSeized(Uint32 i) const {
    if (i>=size) return false;
    return BitmaskImpl::get(bitmaskSz, theAllocatedBitmask, i);
  }
#endif

  /**
   * Cache+LockFun is used to make thread-local caches for ndbmtd
   *   I.e each thread has one cache-instance, and can seize/release on this
   *       wo/ locks
   */
  struct Cache
  {
    Cache(Uint32 a0 = 512, Uint32 a1 = 256) { m_first_free = RNIL; m_free_cnt = 0; m_alloc_cnt = a0; m_max_free_cnt = a1; }
    Uint32 m_first_free;
    Uint32 m_free_cnt;
    Uint32 m_alloc_cnt;
    Uint32 m_max_free_cnt;
  };

  struct LockFun
  {
    void (* lock)(void);
    void (* unlock)(void);
  };

  bool seize(LockFun, Cache&, Ptr<T> &);
  void release(LockFun, Cache&, Uint32 i);
  void release(LockFun, Cache&, Ptr<T> &);
  void releaseList(LockFun, Cache&, Uint32 n, Uint32 first, Uint32 last);

  void setChunkSize(Uint32 sz);
#ifdef ARRAY_CHUNK_GUARD
  void checkChunks();
#endif

protected:
  void releaseChunk(LockFun, Cache&, Uint32 n);

  bool seizeChunk(Uint32 & n, Ptr<T> &);
  void releaseChunk(Uint32 n, Uint32 first, Uint32 last);

protected:
  friend class Array<T>;

  /**
   * Allocate <b>n</b> consecutive object from pool
   *   return base
   */
  Uint32 seizeN(Uint32 n);

  /**
   * Deallocate <b>n<b> consecutive object to pool
   *  starting from base
   */
  void releaseN(Uint32 base, Uint32 n);

public:
  /**
   * Release a singel linked list in o(1)
   * @param first i-value of first element in list
   * @param last  i-value of last element in list
   * @note nextPool must be used as next pointer in list
   */
  void releaseList(Uint32 n, Uint32 first, Uint32 last);
  //private:

#if 0
  Uint32 getNoOfFree2() const {
    Uint32 c2 = size;
    for(Uint32 i = 0; i<((size + 31)>> 5); i++){
      Uint32 w = theAllocatedBitmask[i];
      for(Uint32 j = 0; j<32; j++){
	if((w & 1) == 1){
	  c2--;
	}
	w >>= 1;
      }
    }
    return c2;
  }

  Uint32 getNoOfFree3() const {
    Uint32 c = 0;
    Ptr<T> p;
    p.i = firstFree;
    while(p.i != RNIL){
      c++;
      p.p = &theArray[p.i];
      p.i = p.p->next;
    }
    return c;
  }
#endif

protected:
  Uint32 firstFree;
  Uint32 size;
  Uint32 noOfFree;
  Uint32 noOfFreeMin;
  T * theArray;
  void * alloc_ptr;
#ifdef ARRAY_GUARD
  Uint32 bitmaskSz;
  Uint32 *theAllocatedBitmask;
  bool chunk;
#endif
};

template <class T>
inline
ArrayPool<T>::ArrayPool(){
  firstFree = RNIL;
  size = 0;
  noOfFree = 0;
  noOfFreeMin = 0;
  theArray = 0;
  alloc_ptr = 0;
#ifdef ARRAY_GUARD
  theAllocatedBitmask = 0;
  chunk = false;
#endif
}

template <class T>
inline
ArrayPool<T>::~ArrayPool(){
  if(theArray != 0){
    ndbd_free(alloc_ptr, size * sizeof(T));
    theArray = 0;
    alloc_ptr = 0;
#ifdef ARRAY_GUARD
    if (theAllocatedBitmask)
      delete []theAllocatedBitmask;
    theAllocatedBitmask = 0;
#endif
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
ArrayPool<T>::setSize(Uint32 noOfElements, 
		      bool align, bool exit_on_error, bool guard, Uint32 paramId){
  if(size == 0){
    if(noOfElements == 0)
      return true;
  Uint64 real_size = (Uint64)noOfElements * sizeof(T);
  size_t req_size = (size_t)real_size;
  Uint64 real_size_align = real_size + sizeof(T);
  size_t req_size_align = (size_t)real_size_align;

    if(align)
    {
      if((Uint64)req_size_align == real_size_align && req_size_align > 0)
        alloc_ptr = ndbd_malloc(req_size_align);  
      UintPtr p = (UintPtr)alloc_ptr;
      UintPtr mod = p % sizeof(T);
      if (mod)
      {
	p += sizeof(T) - mod;
      }
      theArray = (T *)p;
    }
    else if((Uint64)req_size == real_size && req_size > 0)
      theArray = (T *)(alloc_ptr = ndbd_malloc(req_size));

    if(theArray == 0)
    {
      char errmsg[255] = "ArrayPool<T>::setSize malloc failed";
      struct ndb_mgm_param_info param_info;
      size_t tsize = sizeof(ndb_mgm_param_info);
      if (!exit_on_error)
	return false;

      if(0 != paramId && 0 == ndb_mgm_get_db_parameter_info(paramId, &param_info, &tsize)) {
        BaseString::snprintf(errmsg, sizeof(errmsg), 
                "Malloc memory for %s failed", param_info.m_name);
      }

      ErrorReporter::handleAssert(errmsg,
				  __FILE__, __LINE__, NDBD_EXIT_MEMALLOC);
      return false; // not reached
    }
    size = noOfElements;
    noOfFree = noOfElements;
    noOfFreeMin = noOfElements;

    /**
     * Set next pointers
     */
    T * t = &theArray[0];
    for(Uint32 i = 0; i<size; i++){
      t->nextPool = (i + 1);
      t++;
    }
    theArray[size-1].nextPool = RNIL;
    firstFree = 0;

#ifdef ARRAY_GUARD
    if (guard)
    {
      bitmaskSz = (noOfElements + 31) >> 5;
      theAllocatedBitmask = new Uint32[bitmaskSz];
      BitmaskImpl::clear(bitmaskSz, theAllocatedBitmask);
    }
#endif
    
    return true;
  }
  if (!exit_on_error)
    return false;

  ErrorReporter::handleAssert("ArrayPool<T>::setSize called twice", __FILE__, __LINE__);
  return false; // not reached
}

template <class T>
inline
void
ArrayPool<T>::setChunkSize(Uint32 sz)
{
  Uint32 i;
  for (i = 0; i + sz < size; i += sz)
  {
    theArray[i].chunkSize = sz;
    theArray[i].lastChunk = i + sz - 1;
    theArray[i].nextChunk = i + sz;
  }

  theArray[i].chunkSize = size - i;
  theArray[i].lastChunk = size - 1;
  theArray[i].nextChunk = RNIL;

#ifdef ARRAY_GUARD
  chunk = true;
#endif

#ifdef ARRAY_CHUNK_GUARD
  checkChunks();
#endif
}

template <class T>
inline
bool
ArrayPool<T>::set(T* ptr, Uint32 cnt, bool align){
  if (size == 0)
  {
    alloc_ptr = ptr;
    if(align)
    {
      UintPtr p = (UintPtr)alloc_ptr;
      UintPtr mod = p % sizeof(T);
      if (mod)
      {
	p += sizeof(T) - mod;
	cnt --;
      }
      theArray = (T *)p;
    }
    else
    {
      theArray = (T *)alloc_ptr;
    }
    
    size = cnt;
    noOfFree = 0;
    noOfFreeMin = 0;
    return true;
  }
  ErrorReporter::handleAssert("ArrayPool<T>::set called twice", 
			      __FILE__, __LINE__);
  return false; // not reached
}
  
template <class T>
inline
void
ArrayPool<T>::getPtr(Ptr<T> & ptr){
  Uint32 i = ptr.i;
  if(likely (i < size)){
    ptr.p = &theArray[i];
#ifdef ARRAY_GUARD
    if (theAllocatedBitmask)
    {
      if(BitmaskImpl::get(bitmaskSz, theAllocatedBitmask, i))
	return;
      /**
       * Getting a non-seized element
       */
      ErrorReporter::handleAssert("ArrayPool<T>::getPtr", __FILE__, __LINE__);
    }
#endif
  } else {
    ErrorReporter::handleAssert("ArrayPool<T>::getPtr", __FILE__, __LINE__);
  }
}

template <class T>
inline
void
ArrayPool<T>::getPtr(ConstPtr<T> & ptr) const {
  Uint32 i = ptr.i;
  if(likely(i < size)){
    ptr.p = &theArray[i];
#ifdef ARRAY_GUARD
    if (theAllocatedBitmask)
    {
      if(BitmaskImpl::get(bitmaskSz, theAllocatedBitmask, i))
	return;
      /**
       * Getting a non-seized element
       */
      ErrorReporter::handleAssert("ArrayPool<T>::getPtr", __FILE__, __LINE__);
    }
#endif
  } else {
    ErrorReporter::handleAssert("ArrayPool<T>::getPtr", __FILE__, __LINE__);
  }
}

template <class T>
inline
void
ArrayPool<T>::getPtr(Ptr<T> & ptr, Uint32 i){
  ptr.i = i;
  if(likely(i < size)){
    ptr.p = &theArray[i];
#ifdef ARRAY_GUARD
    if (theAllocatedBitmask)
    {
      if(BitmaskImpl::get(bitmaskSz, theAllocatedBitmask, i))
	return;
      /**
       * Getting a non-seized element
       */
      ErrorReporter::handleAssert("ArrayPool<T>::getPtr", __FILE__, __LINE__);
    }
#endif
  } else {
    ErrorReporter::handleAssert("ArrayPool<T>::getPtr", __FILE__, __LINE__);
  }
}

template <class T>
inline
void
ArrayPool<T>::getPtr(ConstPtr<T> & ptr, Uint32 i) const {
  ptr.i = i;
  if(likely(i < size)){
    ptr.p = &theArray[i];
#ifdef ARRAY_GUARD
    if (theAllocatedBitmask)
    {
      if(BitmaskImpl::get(bitmaskSz, theAllocatedBitmask, i))
	return;
      /**
       * Getting a non-seized element
       */
      ErrorReporter::handleAssert("ArrayPool<T>::getPtr", __FILE__, __LINE__);
    }
#endif
  } else {
    ErrorReporter::handleAssert("ArrayPool<T>::getPtr", __FILE__, __LINE__);
  }
}
  
template <class T>
inline
T * 
ArrayPool<T>::getPtr(Uint32 i){
  if(likely(i < size)){
#ifdef ARRAY_GUARD
    if (theAllocatedBitmask)
    {
      if(BitmaskImpl::get(bitmaskSz, theAllocatedBitmask, i))
	return &theArray[i];
      /**
       * Getting a non-seized element
       */
      ErrorReporter::handleAssert("ArrayPool<T>::getPtr", __FILE__, __LINE__);
      return 0;
    }
#endif
    return &theArray[i];
  } else {
    ErrorReporter::handleAssert("ArrayPool<T>::getPtr", __FILE__, __LINE__);
    return 0;
  }
}

template <class T>
inline
const T * 
ArrayPool<T>::getConstPtr(Uint32 i) const {
  if(likely(i < size)){
#ifdef ARRAY_GUARD
    if (theAllocatedBitmask)
    {
      if(BitmaskImpl::get(bitmaskSz, theAllocatedBitmask, i))
	return &theArray[i];
      /**
       * Getting a non-seized element
       */
      ErrorReporter::handleAssert("ArrayPool<T>::getPtr", __FILE__, __LINE__);
      return 0;
    }
#endif
    return &theArray[i];
  } else {
    ErrorReporter::handleAssert("ArrayPool<T>::getPtr", __FILE__, __LINE__);
    return 0;
  }
}

template <class T>
inline
void
ArrayPool<T>::getPtr(Ptr<T> & ptr, bool CrashOnBoundaryError){
  Uint32 i = ptr.i;
  if(likely(i < size)){
    ptr.p = &theArray[i];
#ifdef ARRAY_GUARD
    if (theAllocatedBitmask)
    {
      if(BitmaskImpl::get(bitmaskSz, theAllocatedBitmask, i))
	return;
      /**
       * Getting a non-seized element
       */
      ErrorReporter::handleAssert("ArrayPool<T>::getPtr", __FILE__, __LINE__);
    }
#endif
  } else {
    ptr.i = RNIL;
  }
}

template <class T>
inline
void
ArrayPool<T>::getPtr(ConstPtr<T> & ptr, bool CrashOnBoundaryError) const {
  Uint32 i = ptr.i;
  if(likely(i < size)){
    ptr.p = &theArray[i];
#ifdef ARRAY_GUARD
    if (theAllocatedBitmask)
    {
      if(BitmaskImpl::get(bitmaskSz, theAllocatedBitmask, i))
	return;
      /**
       * Getting a non-seized element
       */
      ErrorReporter::handleAssert("ArrayPool<T>::getPtr", __FILE__, __LINE__);
    }
#endif
  } else {
    ptr.i = RNIL;
  }
}

template <class T>
inline
void
ArrayPool<T>::getPtr(Ptr<T> & ptr, Uint32 i, bool CrashOnBoundaryError){
  ptr.i = i;
  if(likely(i < size)){
    ptr.p = &theArray[i];
#ifdef ARRAY_GUARD
    if (theAllocatedBitmask)
    {
      if(BitmaskImpl::get(bitmaskSz, theAllocatedBitmask, i))
	return;
      /**
       * Getting a non-seized element
       */
      ErrorReporter::handleAssert("ArrayPool<T>::getPtr", __FILE__, __LINE__);
    }
#endif
  } else {
    ptr.i = RNIL;
  }
}

template <class T>
inline
void
ArrayPool<T>::getPtr(ConstPtr<T> & ptr, Uint32 i, 
		     bool CrashOnBoundaryError) const {
  ptr.i = i;
  if(likely(i < size)){
    ptr.p = &theArray[i];
#ifdef ARRAY_GUARD
    if (theAllocatedBitmask)
    {
      if(BitmaskImpl::get(bitmaskSz, theAllocatedBitmask, i))
	return;
      /**
       * Getting a non-seized element
       */
      ErrorReporter::handleAssert("ArrayPool<T>::getPtr", __FILE__, __LINE__);
    }
#endif
  } else {
    ptr.i = RNIL;
  }
}
  
template <class T>
inline
T * 
ArrayPool<T>::getPtr(Uint32 i, bool CrashOnBoundaryError){
  if(likely(i < size)){
#ifdef ARRAY_GUARD
    if (theAllocatedBitmask)
    {
      if(BitmaskImpl::get(bitmaskSz, theAllocatedBitmask, i))
	return &theArray[i];
      /**
       * Getting a non-seized element
       */
      ErrorReporter::handleAssert("ArrayPool<T>::getPtr", __FILE__, __LINE__);
      return 0;
    }
#endif
    return &theArray[i];
  } else {
    return 0;
  }
}

template <class T>
inline
const T * 
ArrayPool<T>::getConstPtr(Uint32 i, bool CrashOnBoundaryError) const {
  if(likely(i < size)){
#ifdef ARRAY_GUARD
    if (theAllocatedBitmask)
    {
      if(BitmaskImpl::get(bitmaskSz, theAllocatedBitmask, i))
	return &theArray[i];
      /**
       * Getting a non-seized element
       */
      ErrorReporter::handleAssert("ArrayPool<T>::getConstPtr", __FILE__,__LINE__);
      return 0;
    }
#endif
    return &theArray[i];
  } else {
    return 0;
  }
}
  
/**
 * Allocate an object from pool - update Ptr
 *
 * Return i
 */
template <class T>
inline
bool
ArrayPool<T>::seize(Ptr<T> & ptr){
#ifdef ARRAY_GUARD
  assert(chunk == false);
#endif

  Uint32 ff = firstFree;
  if(ff != RNIL){
    firstFree = theArray[ff].nextPool;
    
    ptr.i = ff;
    ptr.p = &theArray[ff];
#ifdef ARRAY_GUARD
    if (theAllocatedBitmask)
    {
      if(!BitmaskImpl::get(bitmaskSz, theAllocatedBitmask, ff)){
	BitmaskImpl::set(bitmaskSz, theAllocatedBitmask, ff);
	decNoFree();
	return true;
      } else {
	/**
	 * Seizing an already seized element
	 */
	ErrorReporter::handleAssert("ArrayPool<T>::seize", __FILE__, __LINE__);
	return false;
      }
    }
#endif
    decNoFree();
    return true;
  }
  ptr.i = RNIL;
  ptr.p = NULL;
  return false;
}

template <class T>
inline
bool
ArrayPool<T>::seizeId(Ptr<T> & ptr, Uint32 i){
#ifdef ARRAY_GUARD
  assert(chunk == false);
#endif

  Uint32 ff = firstFree;
  Uint32 prev = RNIL;
  while(ff != i && ff != RNIL){
    prev = ff;
    ff = theArray[ff].nextPool;
  }
  
  if(ff != RNIL){
    if(prev == RNIL)
      firstFree = theArray[ff].nextPool;
    else
      theArray[prev].nextPool = theArray[ff].nextPool;
    
    ptr.i = ff;
    ptr.p = &theArray[ff];
#ifdef ARRAY_GUARD
    if (theAllocatedBitmask)
    {
      if(!BitmaskImpl::get(bitmaskSz, theAllocatedBitmask, ff)){
	BitmaskImpl::set(bitmaskSz, theAllocatedBitmask, ff);
	decNoFree();
	return true;
      } else {
	/**
	 * Seizing an already seized element
	 */
	ErrorReporter::handleAssert("ArrayPool<T>::seizeId", __FILE__, __LINE__);
	return false;
      }
    }
#endif
    decNoFree();
    return true;
  }
  ptr.i = RNIL;
  ptr.p = NULL;
  return false;
}

template <class T>
inline
bool
ArrayPool<T>::findId(Uint32 i) const {
  if (i >= size)
    return false;
  Uint32 ff = firstFree;
  while(ff != i && ff != RNIL){
    ff = theArray[ff].nextPool;
  }
  return (ff == RNIL);
}

template<class T>
Uint32
ArrayPool<T>::seizeN(Uint32 n){
#ifdef ARRAY_GUARD
  assert(chunk == false);
#endif

  Uint32 curr = firstFree;
  Uint32 prev = RNIL;
  Uint32 sz = 0;
  while(sz < n && curr != RNIL){
    if(theArray[curr].nextPool == (curr + 1)){
      sz++;
    } else {
      sz = 0;
      prev = curr;
    }
    curr = theArray[curr].nextPool;
  }
  if(sz != n){
    return RNIL;
  }
  const Uint32 base = curr - n;
  if(base == firstFree){
    firstFree = curr;
  } else {
    theArray[prev].nextPool = curr;
  }
  
  decNoFree(n);
#ifdef ARRAY_GUARD
  if (theAllocatedBitmask)
  {
    for(Uint32 j = base; j<curr; j++){
      if(!BitmaskImpl::get(bitmaskSz, theAllocatedBitmask, j)){
	BitmaskImpl::set(bitmaskSz, theAllocatedBitmask, j);
      } else {
	/**
	 * Seizing an already seized element
	 */
	ErrorReporter::handleAssert("ArrayPool<T>::seize", __FILE__, __LINE__);
	return RNIL;
      }
    }
  }
#endif
  return base;
}

template<class T>
inline
void 
ArrayPool<T>::releaseN(Uint32 base, Uint32 n){
#ifdef ARRAY_GUARD
  assert(chunk == false);
#endif

  Uint32 curr = firstFree;
  Uint32 prev = RNIL;
  while(curr < base){
    prev = curr;
    curr = theArray[curr].nextPool;
  }
  if(curr == firstFree){
    firstFree = base;
  } else {
    theArray[prev].nextPool = base;
  }
  const Uint32 end = base + n;
  for(Uint32 i = base; i<end; i++){
#ifdef ARRAY_GUARD
    if (theAllocatedBitmask)
    {
      if(BitmaskImpl::get(bitmaskSz, theAllocatedBitmask, i)){
	BitmaskImpl::clear(bitmaskSz, theAllocatedBitmask, i);
      } else {
	/**
	 * Relesing a already released element
	 */
	ErrorReporter::handleAssert("ArrayPool<T>::release", __FILE__, __LINE__);
	return;
      }
    }
#endif
    theArray[i].nextPool = i + 1;
  }
  theArray[end-1].nextPool = curr;
  noOfFree += n;
}

template<class T>
inline
void 
ArrayPool<T>::releaseList(Uint32 n, Uint32 first, Uint32 last){

#ifdef ARRAY_GUARD
  assert(chunk == false);
#endif

  assert( n != 0 );

  if(first < size && last < size){
    Uint32 ff = firstFree;
    firstFree = first;
    theArray[last].nextPool = ff;
    noOfFree += n;

#ifdef ARRAY_GUARD
    if (theAllocatedBitmask)
    {
      Uint32 tmp = first;
      for(Uint32 i = 0; i<n; i++){
	if(BitmaskImpl::get(bitmaskSz, theAllocatedBitmask, tmp)){
	  BitmaskImpl::clear(bitmaskSz, theAllocatedBitmask, tmp);
	} else {
	  /**
	   * Relesing a already released element
	   */
	  ErrorReporter::handleAssert("ArrayPool<T>::releaseList", 
				      __FILE__, __LINE__);
	  return;
	}
	tmp = theArray[tmp].nextPool;
      }
    }
#endif
    return;
  }
  ErrorReporter::handleAssert("ArrayPool<T>::releaseList", __FILE__, __LINE__);
}

/**
 * Return an object to pool
 */
template <class T>
inline
void
ArrayPool<T>::release(Uint32 _i){

#ifdef ARRAY_GUARD
  assert(chunk == false);
#endif

  const Uint32 i = _i;
  if(likely(i < size)){
    Uint32 ff = firstFree;
    theArray[i].nextPool = ff;
    firstFree = i;

#ifdef ARRAY_GUARD
    if (theAllocatedBitmask)
    {
      if(BitmaskImpl::get(bitmaskSz, theAllocatedBitmask, i)){
	BitmaskImpl::clear(bitmaskSz, theAllocatedBitmask, i);
	noOfFree++;
	return;
      }
      /**
       * Relesing a already released element
       */
      ErrorReporter::handleAssert("ArrayPool<T>::release", __FILE__, __LINE__);
    }
#endif
    noOfFree++;
    return;
  }
  ErrorReporter::handleAssert("ArrayPool<T>::release", __FILE__, __LINE__);
}

/**
 * Return an object to pool
 */
template <class T>
inline
void
ArrayPool<T>::release(Ptr<T> & ptr){

#ifdef ARRAY_GUARD
  assert(chunk == false);
#endif

  Uint32 i = ptr.i;
  if(likely(i < size)){
    Uint32 ff = firstFree;
    theArray[i].nextPool = ff;
    firstFree = i;

#ifdef ARRAY_GUARD
    if (theAllocatedBitmask)
    {
      if(BitmaskImpl::get(bitmaskSz, theAllocatedBitmask, i)){
	BitmaskImpl::clear(bitmaskSz, theAllocatedBitmask, i);
	//assert(noOfFree() == noOfFree2());
	noOfFree++;
	return;
      }
      /**
       * Relesing a already released element
       */
      ErrorReporter::handleAssert("ArrayPool<T>::release", __FILE__, __LINE__);
    }
#endif
    noOfFree++;
    return;
  }
  ErrorReporter::handleAssert("ArrayPool<T>::release", __FILE__, __LINE__);
}

#if 0
#define DUMP(a,b) do { printf("%s c.m_first_free: %u c.m_free_cnt: %u %s", a, c.m_first_free, c.m_free_cnt, b); fflush(stdout); } while(0)
#else
#define DUMP(a,b)
#endif

#ifdef ARRAY_CHUNK_GUARD
template <class T>
inline
void
ArrayPool<T>::checkChunks()
{
#ifdef ARRAY_GUARD
  assert(chunk == true);
#endif

  Uint32 ff = firstFree;
  Uint32 sum = 0;
  while (ff != RNIL)
  {
    sum += theArray[ff].chunkSize;
    Uint32 last = theArray[ff].lastChunk;
    assert(theArray[last].nextPool == theArray[ff].nextChunk);
    if (theAllocatedBitmask)
    {
      Uint32 tmp = ff;
      while(tmp != last)
      {
        assert(!BitmaskImpl::get(bitmaskSz, theAllocatedBitmask, tmp));
        tmp = theArray[tmp].nextPool;
      }
      assert(!BitmaskImpl::get(bitmaskSz, theAllocatedBitmask, tmp));
    }

    ff = theArray[ff].nextChunk;
  }
  assert(sum == noOfFree);
}
#endif

template <class T>
inline
bool
ArrayPool<T>::seizeChunk(Uint32 & cnt, Ptr<T> & ptr)
{
#ifdef ARRAY_GUARD
  assert(chunk == true);
#endif

#ifdef ARRAY_CHUNK_GUARD
  checkChunks();
#endif

  Uint32 save = cnt;
  int tmp = save;
  Uint32 ff = firstFree;
  ptr.i = ff;
  ptr.p = theArray + ff;

  if (ff != RNIL)
  {
    Uint32 prev;
    do 
    {
      if (0)
        ndbout_c("seizeChunk(%u) ff: %u tmp: %d chunkSize: %u lastChunk: %u nextChunk: %u",
                 save, ff, tmp, 
                 theArray[ff].chunkSize, 
                 theArray[ff].lastChunk,
                 theArray[ff].nextChunk);
      
      tmp -= theArray[ff].chunkSize;
      prev = theArray[ff].lastChunk;
      assert(theArray[ff].nextChunk == theArray[prev].nextPool);
      ff = theArray[ff].nextChunk;
    } while (tmp > 0 && ff != RNIL);
    
    cnt = (save - tmp);
    decNoFree(save - tmp);
    firstFree = ff;
    theArray[prev].nextPool = RNIL;
    
#ifdef ARRAY_GUARD
    if (theAllocatedBitmask)
    {
      Uint32 tmpI = ptr.i;
      for(Uint32 i = 0; i<cnt; i++){
        if(BitmaskImpl::get(bitmaskSz, theAllocatedBitmask, tmpI)){
          /**
           * Seizing an already seized element
           */
          ErrorReporter::handleAssert("ArrayPool<T>::seizeChunk", 
                                      __FILE__, __LINE__);
        } else {
          BitmaskImpl::set(bitmaskSz, theAllocatedBitmask, tmpI);
        }
        tmpI = theArray[tmpI].nextPool;
      }
    }
#endif

#ifdef ARRAY_CHUNK_GUARD
    checkChunks();
#endif

    return true;
  }

  ptr.p = NULL;
  return false;
}

template <class T>
inline
void
ArrayPool<T>::releaseChunk(Uint32 cnt, Uint32 first, Uint32 last)
{
#ifdef ARRAY_GUARD
  assert(chunk == true);
#endif

#ifdef ARRAY_CHUNK_GUARD
  checkChunks();
#endif

  Uint32 ff = firstFree;
  firstFree = first;
  theArray[first].nextChunk = ff;
  theArray[last].nextPool = ff;
  noOfFree += cnt;

  assert(theArray[first].chunkSize == cnt);
  assert(theArray[first].lastChunk == last);
  
#ifdef ARRAY_GUARD
  if (theAllocatedBitmask)
  {
    Uint32 tmp = first;
    for(Uint32 i = 0; i<cnt; i++){
      if(BitmaskImpl::get(bitmaskSz, theAllocatedBitmask, tmp)){
        BitmaskImpl::clear(bitmaskSz, theAllocatedBitmask, tmp);
      } else {
        /**
         * Relesing a already released element
         */
        ErrorReporter::handleAssert("ArrayPool<T>::releaseList", 
                                    __FILE__, __LINE__);
        return;
      }
      tmp = theArray[tmp].nextPool;
    }
  }
#endif

#ifdef ARRAY_CHUNK_GUARD
  checkChunks();
#endif
}


template <class T>
inline
bool
ArrayPool<T>::seize(LockFun l, Cache& c, Ptr<T> & p)
{
  DUMP("seize", "-> ");

  Uint32 ff = c.m_first_free;
  if (ff != RNIL)
  {
    c.m_first_free = theArray[ff].nextPool;
    c.m_free_cnt--;
    p.i = ff;
    p.p = theArray + ff;
    DUMP("LOCAL ", "\n");
    return true;
  }

  Uint32 tmp = c.m_alloc_cnt;
  l.lock();
  bool ret = seizeChunk(tmp, p);
  l.unlock();

  if (ret)
  {
    c.m_first_free = theArray[p.i].nextPool;
    c.m_free_cnt = tmp - 1;
    DUMP("LOCKED", "\n");
    return true;
  }
  return false;
}

template <class T>
inline
void
ArrayPool<T>::release(LockFun l, Cache& c, Uint32 i)
{
  Ptr<T> tmp;
  getPtr(tmp, i);
  release(l, c, tmp);
}

template <class T>
inline
void
ArrayPool<T>::release(LockFun l, Cache& c, Ptr<T> & p)
{
  p.p->nextPool = c.m_first_free;
  c.m_first_free = p.i;
  c.m_free_cnt ++;

  if (c.m_free_cnt > 2 * c.m_max_free_cnt)
  {
    releaseChunk(l, c, c.m_alloc_cnt);
  }
}

template <class T>
inline
void
ArrayPool<T>::releaseList(LockFun l, Cache& c,
                          Uint32 n, Uint32 first, Uint32 last)
{
  theArray[last].nextPool = c.m_first_free;
  c.m_first_free = first;
  c.m_free_cnt += n;

  if (c.m_free_cnt > 2 * c.m_max_free_cnt)
  {
    releaseChunk(l, c, c.m_alloc_cnt);
  }
}

template <class T>
inline
void
ArrayPool<T>::releaseChunk(LockFun l, Cache& c, Uint32 n)
{
  DUMP("releaseListImpl", "-> ");
  Uint32 ff = c.m_first_free;
  Uint32 prev = ff;
  Uint32 curr = ff;
  Uint32 i;
  for (i = 0; i < n && curr != RNIL; i++)
  {
    prev = curr;
    curr = theArray[curr].nextPool;
  }
  c.m_first_free = curr;
  c.m_free_cnt -= i;

  DUMP("", "\n");

  theArray[ff].chunkSize = i;
  theArray[ff].lastChunk = prev;

  l.lock();
  releaseChunk(i, ff, prev);
  l.unlock();
}

template <class T>
class UnsafeArrayPool : public ArrayPool<T> {
public:
  /**
   * Update p value for ptr according to i value 
   *   ignore if it's allocated or not
   */
  void getPtrForce(Ptr<T> &);
  void getPtrForce(ConstPtr<T> &) const;
  T * getPtrForce(Uint32 i);
  const T * getConstPtrForce(Uint32 i) const;
  void getPtrForce(Ptr<T> &, Uint32 i);
  void getPtrForce(ConstPtr<T> &, Uint32 i) const;
};

template <class T>
inline
void 
UnsafeArrayPool<T>::getPtrForce(Ptr<T> & ptr){
  Uint32 i = ptr.i;
  if(likely(i < this->size)){
    ptr.p = &this->theArray[i];
  } else {
    ErrorReporter::handleAssert("UnsafeArrayPool<T>::getPtr", 
				__FILE__, __LINE__);
  }
}

template <class T>
inline
void 
UnsafeArrayPool<T>::getPtrForce(ConstPtr<T> & ptr) const{
  Uint32 i = ptr.i;
  if(likely(i < this->size)){
    ptr.p = &this->theArray[i];
  } else {
    ErrorReporter::handleAssert("UnsafeArrayPool<T>::getPtr", 
				__FILE__, __LINE__);
  }
}

template <class T>
inline
T * 
UnsafeArrayPool<T>::getPtrForce(Uint32 i){
  if(likely(i < this->size)){
    return &this->theArray[i];
  } else {
    ErrorReporter::handleAssert("UnsafeArrayPool<T>::getPtr", 
				__FILE__, __LINE__);
    return 0;
  }
}

template <class T>
inline
const T * 
UnsafeArrayPool<T>::getConstPtrForce(Uint32 i) const {
  if(likely(i < this->size)){
    return &this->theArray[i];
  } else {
    ErrorReporter::handleAssert("UnsafeArrayPool<T>::getPtr",
				__FILE__, __LINE__);
    return 0;
  }
}

template <class T>
inline
void 
UnsafeArrayPool<T>::getPtrForce(Ptr<T> & ptr, Uint32 i){
  ptr.i = i;
  if(likely(i < this->size)){
    ptr.p = &this->theArray[i];
    return ;
  } else {
    ErrorReporter::handleAssert("UnsafeArrayPool<T>::getPtr", 
				__FILE__, __LINE__);
  }
}

template <class T>
inline
void 
UnsafeArrayPool<T>::getPtrForce(ConstPtr<T> & ptr, Uint32 i) const{
  ptr.i = i;
  if(likely(i < this->size)){
    ptr.p = &this->theArray[i];
    return ;
  } else {
    ErrorReporter::handleAssert("UnsafeArrayPool<T>::getPtr", 
				__FILE__, __LINE__);
  }
}

// SafeArrayPool

template <class T>
class SafeArrayPool : public ArrayPool<T> {
public:
  SafeArrayPool(NdbMutex* mutex = 0);
  ~SafeArrayPool();
  int lock();
  int unlock();
  bool seize(Ptr<T>&);
  void release(Uint32 i);
  void release(Ptr<T>&);

private:
  NdbMutex* m_mutex;
  bool m_mutex_owner;

  bool seizeId(Ptr<T>&);
  bool findId(Uint32) const;
};

template <class T>
inline
SafeArrayPool<T>::SafeArrayPool(NdbMutex* mutex)
{
  if (mutex != 0) {
    m_mutex = mutex;
    m_mutex_owner = false;
  } else {
    m_mutex = NdbMutex_Create();
    assert(m_mutex != 0);
    m_mutex_owner = true;
  }
}

template <class T>
inline
SafeArrayPool<T>::~SafeArrayPool()
{
  if (m_mutex_owner)
    NdbMutex_Destroy(m_mutex);
}

template <class T>
inline
int
SafeArrayPool<T>::lock()
{
  return NdbMutex_Lock(m_mutex);
}

template <class T>
inline
int
SafeArrayPool<T>::unlock()
{
  return NdbMutex_Unlock(m_mutex);
}

template <class T>
inline
bool
SafeArrayPool<T>::seize(Ptr<T>& ptr)
{
  bool ok = false;
  if (lock() == 0) {
    ok = ArrayPool<T>::seize(ptr);
    unlock();
  }
  return ok;
}

template <class T>
inline
void
SafeArrayPool<T>::release(Uint32 i)
{
  int ret = lock();
  assert(ret == 0);
  ArrayPool<T>::release(i);
  unlock();
}

template <class T>
inline
void
SafeArrayPool<T>::release(Ptr<T>& ptr)
{
  int ret = lock();
  assert(ret == 0);
  ArrayPool<T>::release(ptr);
  unlock();
}

#endif
