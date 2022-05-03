/*
   Copyright (c) 2003, 2022, Oracle and/or its affiliates.

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

#ifndef ARRAY_POOL_HPP
#define ARRAY_POOL_HPP

#include "util/require.h"
#include <ndb_global.h>
#include "ndbd_malloc.hpp"

#include <pc.hpp>
#include "Pool.hpp"
#include <ErrorReporter.hpp>
#include <Bitmask.hpp>
#include <mgmapi.h>

#include <NdbMutex.h>
#include <EventLogger.hpp>

#define JAM_FILE_ID 292


template <class T> class Array;

//#define ARRAY_CHUNK_GUARD

/**
 * Template class used for implementing an
 *   pool of object (in an array with a free list)
 */
template <class T>
class ArrayPool {
public:
  typedef T Type;
  typedef void (CallBack)(ArrayPool<T>& pool);

  /**
    'seizeErrorFunc' is called in case of out of memory errors. Observe
    that the pool is not locked when seizeErrorFunc is called.
    A function pointer rather than a virtual function is used here, because 
    a virtual function would require explicit instantiations of ArrayPool for 
    all T types. That would again require all T types to define the nextChunk, 
    lastChunk and chunkSize fields. This is currently not the case.

    Instead an (abstract) ErrorHandler class is instantiated to store the
    'seizeErrorFunc' pointer. The ErrorHandler class contain the
    virtual 'failure()' function which calls 'seizeErrorFunc' upon
    a seize-failure  This allowes for sub classes of ArrayPool to
    specify error-funcs with different signatures, thus avoiding any
    type cast problems related to this.
  */
  explicit ArrayPool(CallBack* seizeErrorFunc=nullptr)
    : ArrayPool(new ErrorHandlerImpl(seizeErrorFunc, *this))
  {}

  ~ArrayPool();

  class ErrorHandler {
  public:
    virtual ~ErrorHandler() {}
    virtual void failure() const = 0;
  };

  class ErrorHandlerImpl : public ErrorHandler {
  public:
    ErrorHandlerImpl(CallBack* f, ArrayPool<T>& p) :
      func(f), pool(p)
    {}
    ~ErrorHandlerImpl() override {}

    void failure() const override {
      if (func != nullptr) {
        (*func)(pool);
      }
    }

  private:
    CallBack* func;
    ArrayPool<T>& pool;
  };

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

  /**
    Set the low water mark equal to the current value, so that we can track
    the lowest value seen after this call.
   */
  void resetFreeMin()
  {
    noOfFreeMin = noOfFree;
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
  void getPtr(Ptr<T> &) const;
  void getPtr(ConstPtr<T> &) const;
  void getPtrIgnoreAlloc(Ptr<T> &) const;

  /**
   * Get pointer for i value
   */
  [[nodiscard]] T * getPtr(Uint32 i) const;

  /**
   * Update p & i value for ptr according to <b>i</b> value 
   */
  [[nodiscard]] bool getPtr(Ptr<T> &, Uint32 i) const;

  /**
   * Allocate an object from pool - update Ptr
   *
   * Return i
   */
  [[nodiscard]] bool seize(Ptr<T> &);

  /**
   * Allocate object <b>i</b> from pool - update Ptr
   */
  [[nodiscard]] bool seizeId(Ptr<T> &, Uint32 i);

  /**
   * Check if <b>i</b> is allocated.
   */
  [[nodiscard]] bool findId(Uint32 i) const;

  /**
   * Return an object to pool
   * release releases the object and places it first in free list
   */
  void release(Uint32 i);

  /**
   * Return an object to pool
   * release releases the object and places it first in free list
   * releaseLast releases the object and places it last in free list
   */
  void release(Ptr<T> &);
  void releaseLast(Ptr<T> &);

protected:
  friend class Array<T>;
  explicit ArrayPool(const ErrorHandler* errorHandler);

  /**
   * Allocate <b>n</b> consecutive object from pool
   *   return base
   */
  [[nodiscard]] Uint32 seizeN(Uint32 n);

  /**
   * Deallocate <b>n</b> consecutive object to pool
   *  starting from base
   */
  void releaseN(Uint32 base, Uint32 n);

public:
  /**
   * Release a single linked list in o(1)
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
  T * theArray;
  Uint32 size;
  /*
   * Protect size and theArray which are very seldom updated from
   * updates of often updated variables such as firstFree, noOfFree.
   * Protect here means to have them on separate CPU cache lines to
   * avoid false CPU cache line sharing.
   */
  char protect_read_var[NDB_CL_PADSZ(sizeof(Uint32) + sizeof(void*))];
  Uint32 firstFree;
  Uint32 lastFree;
  Uint32 noOfFree;
  Uint32 noOfFreeMin;
#ifdef ARRAY_GUARD
  bool chunk;
  Uint32 bitmaskSz;
  Uint32 *theAllocatedBitmask;
#endif
  void * alloc_ptr;
  // Call this function if a seize request fails.
  const ErrorHandler* const seizeErrHand;
public:
  T* getArrayPtr()
  {
    return theArray;
  }
  void setArrayPtr(T* newArray)
  {
    theArray = newArray;
  }
  Uint32 getSize()
  {
    return size;
  }
  void setNewSize(Uint32 newSize)
  {
    size = newSize;
  }
};

template <class T>
inline
ArrayPool<T>::ArrayPool(const ErrorHandler* seizeErrorHandler):
  seizeErrHand(seizeErrorHandler)
{
  firstFree = RNIL;
  lastFree = RNIL;
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
  delete seizeErrHand;
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
		      bool align,
                      bool exit_on_error,
                      bool guard,
                      Uint32 paramId)
{
  if(size == 0)
  {
    if(noOfElements == 0)
    {
      return true;
    }
    Uint64 real_size = (Uint64)noOfElements * sizeof(T);
    size_t req_size = (size_t)real_size;
    Uint64 real_size_align = real_size + sizeof(T);
    size_t req_size_align = (size_t)real_size_align;

    if(align)
    {
      if((Uint64)req_size_align == real_size_align && req_size_align > 0)
      {
        alloc_ptr = ndbd_malloc(req_size_align);
      }
      UintPtr p = (UintPtr)alloc_ptr;
      UintPtr mod = p % sizeof(T);
      if (mod)
      {
	p += sizeof(T) - mod;
      }
      theArray = (T *)p;
    }
    else if((Uint64)req_size == real_size && req_size > 0)
    {
      theArray = (T *)(alloc_ptr = ndbd_malloc(req_size));
    }

    if(theArray == 0)
    {
      char errmsg[255] = "ArrayPool<T>::setSize malloc failed";
      struct ndb_mgm_param_info param_info;
      size_t tsize = sizeof(ndb_mgm_param_info);
      if (!exit_on_error)
	return false;

      if(0 != paramId &&
         0 == ndb_mgm_get_db_parameter_info(paramId, &param_info, &tsize))
      {
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
    for(Uint32 i = 0; i<size; i++)
    {
      t->nextPool = (i + 1);
      t++;
    }
    theArray[size-1].nextPool = RNIL;
    firstFree = 0;
    lastFree = size - 1;

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
  {
    return false;
  }

  ErrorReporter::handleAssert("ArrayPool<T>::setSize called twice", __FILE__, __LINE__);
  return false; // not reached
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
ArrayPool<T>::getPtr(Ptr<T> & ptr) const
{
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
ArrayPool<T>::getPtr(ConstPtr<T> & ptr) const
{
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
bool
ArrayPool<T>::getPtr(Ptr<T> & ptr, Uint32 i) const
{
  if (likely(i < size))
  {
    ptr.i = i;
    ptr.p = &theArray[i];
#ifdef ARRAY_GUARD
    if (theAllocatedBitmask)
    {
      if (BitmaskImpl::get(bitmaskSz, theAllocatedBitmask, i))
        return true;
      /**
       * Getting a non-seized element
       */
      return false;
    }
#endif
    return true;
  }
  return false;
}

template <class T>
inline
T * 
ArrayPool<T>::getPtr(Uint32 i) const
{
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

/**
   getPtrIgnoreAlloc

   getPtr, without array_guard /theAllocatedBitmask checks
   Useful when looking at elements in the pool which may or may not
   be allocated.
   Retains the range check.
*/
template <class T>
inline
void
ArrayPool<T>::getPtrIgnoreAlloc(Ptr<T> & ptr) const
{
  Uint32 i = ptr.i;
  if(likely (i < size)){
    ptr.p = &theArray[i];
  } else {
    ErrorReporter::handleAssert("ArrayPool<T>::getPtr", __FILE__, __LINE__);
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
    if (firstFree == RNIL)
    {
      assert(lastFree == ff);
      lastFree = RNIL;
    }
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
  seizeErrHand->failure();
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
    if (ff == lastFree)
    {
      assert(firstFree == RNIL);
      lastFree = prev;
    }
    
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
  seizeErrHand->failure();
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
    seizeErrHand->failure();
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
	 * Releasing an already released element
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
	   * Releasing an already released element
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
    if (ff == RNIL)
    {
      assert(lastFree == RNIL);
      lastFree = i;
    }

#ifdef ARRAY_GUARD
    if (theAllocatedBitmask)
    {
      if(BitmaskImpl::get(bitmaskSz, theAllocatedBitmask, i)){
	BitmaskImpl::clear(bitmaskSz, theAllocatedBitmask, i);
	noOfFree++;
	return;
      }
      /**
       * Releasing an already released element
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
    if (ff == RNIL)
    {
      assert(lastFree == RNIL);
      lastFree = i;
    }

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
       * Releasing an already released element
       */
      ErrorReporter::handleAssert("ArrayPool<T>::release", __FILE__, __LINE__);
    }
#endif
    noOfFree++;
    return;
  }
  ErrorReporter::handleAssert("ArrayPool<T>::release", __FILE__, __LINE__);
}

template <class T>
inline
void
ArrayPool<T>::releaseLast(Ptr<T> & ptr)
{

#ifdef ARRAY_GUARD
  assert(chunk == false);
#endif

  Uint32 i = ptr.i;
  Uint32 lf = lastFree;
  if(likely(i < size))
  {
    lastFree = i;
    theArray[i].nextPool = RNIL;
    if (lf < size)
    {
      theArray[lf].nextPool = i;
    }
    else if (lf == RNIL)
    {
      assert(firstFree == RNIL);
      firstFree = i;
    }
    else
    {
      goto error;
    }

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
       * Releasing an already released element
       */
      ErrorReporter::handleAssert("ArrayPool<T>::releaseLast", __FILE__, __LINE__);
    }
#endif
    noOfFree++;
    return;
  }
error:
  ErrorReporter::handleAssert("ArrayPool<T>::release", __FILE__, __LINE__);
}

#if 0
#define DUMP(a,b) do { printf("%s c.m_first_free: %u c.m_free_cnt: %u %s", a, c.m_first_free, c.m_free_cnt, b); fflush(stdout); } while(0)
#else
#define DUMP(a,b)
#endif


template <class T>
class CachedArrayPool : public ArrayPool<T>
{
public:
  typedef void (CallBack)(CachedArrayPool<T>& pool);
  explicit CachedArrayPool(CallBack* seizeErrorFunc=nullptr)
    : ArrayPool<T>(new ErrorHandlerImpl(seizeErrorFunc, *this))
  {}

  class ErrorHandlerImpl : public ArrayPool<T>::ErrorHandler {
  public:
    ErrorHandlerImpl(CallBack* f, CachedArrayPool<T>& p) :
      func(f), pool(p)
    {}
    ~ErrorHandlerImpl() override {}

    void failure() const override {
      if (func != nullptr) {
        (*func)(pool);
      }
    }

  private:
    CallBack* func;
    CachedArrayPool<T>& pool;
  };

  void setChunkSize(Uint32 sz);
#ifdef ARRAY_CHUNK_GUARD
  void checkChunks();
#endif

/***/
  [[nodiscard]] bool seize(Ptr<T> &p) { return ArrayPool<T>::seize(p); }
  void release(Uint32 i) { return ArrayPool<T>::release(i); }
  void releaseList(Uint32 n, Uint32 first, Uint32 last) { ArrayPool<T>::releaseList(n,first,last); }

  /**
   * Cache+LockFun is used to make thread-local caches for ndbmtd
   *   I.e each thread has one cache-instance, and can seize/release on this
   *       wo/ locks
   */
  struct Cache
  {
    Cache(Uint32 a0 = 512, Uint32 a1 = 256)
    { m_first_free = RNIL; m_free_cnt = 0; m_alloc_cnt = a0; m_max_free_cnt = a1; }
    void init_cache(Uint32 a0, Uint32 a1)
    {
      m_alloc_cnt = a0;
      m_max_free_cnt = a1;
    }
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

  [[nodiscard]] bool seize(LockFun, Cache&, Ptr<T> &);
  void release(LockFun, Cache&, Uint32 i);
  void release(LockFun, Cache&, Ptr<T> &);
  void releaseList(LockFun, Cache&, Uint32 n, Uint32 first, Uint32 last);

protected:
  void releaseChunk(LockFun, Cache&, Uint32 n);

  [[nodiscard]] bool seizeChunk(Uint32 & n, Ptr<T> &);
  void releaseChunk(Uint32 n, Uint32 first, Uint32 last);
};

template <class T>
inline
void
CachedArrayPool<T>::setChunkSize(Uint32 sz)
{
  Uint32 const& size = this->size;
  T * const& theArray = this->theArray;
#ifdef ARRAY_GUARD
  bool& chunk = this->chunk;
#endif

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

#ifdef ARRAY_CHUNK_GUARD
template <class T>
inline
void
CachedArrayPool<T>::checkChunks()
{
  T * const& theArray = this->theArray;
  const Uint32 firstFree = this->firstFree;
  Uint32 * const& theAllocatedBitmask = this->theAllocatedBitmask;
#ifdef ARRAY_GUARD
  bool const& chunk = this->chunk;
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
CachedArrayPool<T>::seizeChunk(Uint32 & cnt, Ptr<T> & ptr)
{
  Uint32 & firstFree = this->firstFree;
  T* const& theArray = this->theArray;
#ifdef ARRAY_GUARD
  Uint32 & bitmaskSz = this->bitmaskSz;
  Uint32 * const& theAllocatedBitmask = this->theAllocatedBitmask;
  bool const& chunk = this->chunk;
  assert(chunk == true);
  (void)chunk; //Avoid 'unused warning'
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
        g_eventLogger->info(
            "seizeChunk(%u) ff: %u tmp: %d chunkSize: %u lastChunk: %u "
            "nextChunk: %u",
            save, ff, tmp, theArray[ff].chunkSize, theArray[ff].lastChunk,
            theArray[ff].nextChunk);

      tmp -= theArray[ff].chunkSize;
      prev = theArray[ff].lastChunk;
      assert(theArray[ff].nextChunk == theArray[prev].nextPool);
      ff = theArray[ff].nextChunk;
    } while (tmp > 0 && ff != RNIL);
    
    cnt = (save - tmp);
this->    decNoFree(save - tmp);
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
  this->seizeErrHand->failure();
  return false;
}

template <class T>
inline
void
CachedArrayPool<T>::releaseChunk(Uint32 cnt, Uint32 first, Uint32 last)
{
  Uint32 & firstFree = this->firstFree;
  T * const& theArray = this->theArray;
  Uint32 & noOfFree = this->noOfFree;
#ifdef ARRAY_GUARD
  Uint32 & bitmaskSz = this->bitmaskSz;
  Uint32 * const& theAllocatedBitmask = this->theAllocatedBitmask;
#endif
#ifdef ARRAY_GUARD
  bool const& chunk = this->chunk;
  assert(chunk == true);
  (void)chunk; //Avoid 'unused warning'
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
         * Releasing an already released element
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
CachedArrayPool<T>::seize(LockFun l, Cache& c, Ptr<T> & p)
{
  DUMP("seize", "-> ");

  Uint32 ff = c.m_first_free;
  if (ff != RNIL)
  {
    c.m_first_free = this->theArray[ff].nextPool;
    c.m_free_cnt--;
    p.i = ff;
    p.p = this->theArray + ff;
    DUMP("LOCAL ", "\n");
    return true;
  }

  Uint32 tmp = c.m_alloc_cnt;
  l.lock();
  bool ret = seizeChunk(tmp, p);
  l.unlock();

  if (ret)
  {
    c.m_first_free = this->theArray[p.i].nextPool;
    c.m_free_cnt = tmp - 1;
    DUMP("LOCKED", "\n");
    return true;
  }
  this->seizeErrHand->failure();
  return false;
}

template <class T>
inline
void
CachedArrayPool<T>::release(LockFun l, Cache& c, Uint32 i)
{
  Ptr<T> tmp;
  require(this->getPtr(tmp, i));
  release(l, c, tmp);
}

template <class T>
inline
void
CachedArrayPool<T>::release(LockFun l, Cache& c, Ptr<T> & p)
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
CachedArrayPool<T>::releaseList(LockFun l, Cache& c,
                          Uint32 n, Uint32 first, Uint32 last)
{
  this->theArray[last].nextPool = c.m_first_free;
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
CachedArrayPool<T>::releaseChunk(LockFun l, Cache& c, Uint32 n)
{
  T * const& theArray = this->theArray;
  DUMP("releaseList", "-> ");
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

// SafeArrayPool

template <class T>
class SafeArrayPool : public ArrayPool<T> {
public:
  SafeArrayPool();
  ~SafeArrayPool();
  int lock();
  int unlock();
  [[nodiscard]] bool seize(Ptr<T>&);
  void release(Uint32 i);
  void release(Ptr<T>&);

  void setMutex(NdbMutex* mutex = 0);

private:
  NdbMutex* m_mutex;
  bool m_mutex_owner;

  [[nodiscard]] bool seizeId(Ptr<T>&);
  [[nodiscard]] bool findId(Uint32) const;
};

template <class T>
inline
SafeArrayPool<T>::SafeArrayPool()
{
  m_mutex = 0;
  m_mutex_owner = false;
}

template <class T>
inline
void
SafeArrayPool<T>::setMutex(NdbMutex* mutex)
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
  require(ret == 0);
  ArrayPool<T>::release(i);
  unlock();
}

template <class T>
inline
void
SafeArrayPool<T>::release(Ptr<T>& ptr)
{
  int ret = lock();
  require(ret == 0);
  ArrayPool<T>::release(ptr);
  unlock();
}


#undef JAM_FILE_ID

#endif
