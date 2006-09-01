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

#ifndef DL_HASHTABLE2_HPP
#define DL_HASHTABLE2_HPP

#include <ndb_global.h>
#include "ArrayPool.hpp"

/**
 * DLHashTable2 is a DLHashTable variant meant for cases where different
 * DLHashTable instances share a common pool (based on a union U).
 *
 * Calls T constructor after seize from pool and T destructor before
 * release (in all forms) into pool.
 */
template <class T, class U>
class DLHashTable2 {
public:
  DLHashTable2(ArrayPool<U> & thePool);
  ~DLHashTable2();
  
  /**
   * Set the no of bucket in the hashtable
   *
   * Note, can currently only be called once
   */
  bool setSize(Uint32 noOfElements);

  /**
   * Seize element from pool - return i
   *
   * Note must be either added using <b>add</b> or released 
   * using <b>release</b>
   */
  bool seize(Ptr<T> &);

  /**
   * Add an object to the hashtable
   */
  void add(Ptr<T> &);
  
  /**
   * Find element key in hashtable update Ptr (i & p) 
   *   (using key.equal(...))
   * @return true if found and false otherwise
   */
  bool find(Ptr<T> &, const T & key) const;

  /**
   * Update i & p value according to <b>i</b>
   */
  void getPtr(Ptr<T> &, Uint32 i) const;

  /**
   * Get element using ptr.i (update ptr.p)
   */
  void getPtr(Ptr<T> &) const;

  /**
   * Get P value for i
   */
  T * getPtr(Uint32 i) const;

  /**
   * Remove element (and set Ptr to removed element)
   * Note does not return to pool
   */
  void remove(Ptr<T> &, const T & key);

  /**
   * Remove element
   * Note does not return to pool
   */
  void remove(Uint32 i);

  /**
   * Remove element
   * Note does not return to pool
   */
  void remove(Ptr<T> &);

  /**
   * Remove all elements, but dont return them to pool
   */
  void removeAll();
  
  /**
   * Remove element (and set Ptr to removed element)
   * And return element to pool
   */
  void release(Ptr<T> &, const T & key);

  /**
   * Remove element and return to pool
   */
  void release(Uint32 i);

  /**
   * Remove element and return to pool
   */
  void release(Ptr<T> &);
  
  class Iterator {
  public:
    Ptr<T> curr;
    Uint32 bucket;
    inline bool isNull() const { return curr.isNull();}
    inline void setNull() { curr.setNull(); }
  };

  /**
   * Sets curr.p according to curr.i
   */
  void getPtr(Iterator & iter) const ;

  /**
   * First element in bucket
   */
  bool first(Iterator & iter) const;
  
  /**
   * Next Element
   *
   * param iter - A "fully set" iterator
   */
  bool next(Iterator & iter) const;

  /**
   * Get next element starting from bucket
   *
   * @param bucket - Which bucket to start from
   * @param iter - An "uninitialized" iterator
   */
  bool next(Uint32 bucket, Iterator & iter) const;

  inline bool isEmpty() const { Iterator iter; return ! first(iter); }
  
private:
  Uint32 mask;
  Uint32 * hashValues;
  ArrayPool<U> & thePool;
};

template<class T, class U>
inline
DLHashTable2<T, U>::DLHashTable2(ArrayPool<U> & _pool)
  : thePool(_pool)
{
  mask = 0;
  hashValues = 0;
}

template<class T, class U>
inline
DLHashTable2<T, U>::~DLHashTable2(){
  if(hashValues != 0)
    delete [] hashValues;
}

template<class T, class U>
inline
bool
DLHashTable2<T, U>::setSize(Uint32 size){
  Uint32 i = 1;
  while(i < size) i *= 2;

  if(mask == (i - 1)){
    /**
     * The size is already set to <b>size</b>
     */
    return true;
  }

  if(mask != 0){
    /**
     * The mask is already set
     */
    return false;
  }
  
  mask = (i - 1);
  hashValues = new Uint32[i];
  for(Uint32 j = 0; j<i; j++)
    hashValues[j] = RNIL;
  
  return true;
}

template<class T, class U>
inline
void
DLHashTable2<T, U>::add(Ptr<T> & obj){
  const Uint32 hv = obj.p->hashValue() & mask;
  const Uint32 i  = hashValues[hv];
  
  if(i == RNIL){
    hashValues[hv] = obj.i;
    obj.p->nextHash = RNIL;
    obj.p->prevHash = RNIL;
  } else {
    
    T * tmp = (T*)thePool.getPtr(i);    // cast
    tmp->prevHash = obj.i;
    obj.p->nextHash = i;
    obj.p->prevHash = RNIL;
    
    hashValues[hv] = obj.i;
  }
}

/**
 * First element
 */
template<class T, class U>
inline
bool
DLHashTable2<T, U>::first(Iterator & iter) const {
  Uint32 i = 0;
  while(i <= mask && hashValues[i] == RNIL) i++;
  if(i <= mask){
    iter.bucket = i;
    iter.curr.i = hashValues[i];
    iter.curr.p = (T*)thePool.getPtr(iter.curr.i);      // cast
    return true;
  } else {
    iter.curr.i = RNIL;
  }
  return false;
}

template<class T, class U>
inline
bool
DLHashTable2<T, U>::next(Iterator & iter) const {
  if(iter.curr.p->nextHash == RNIL){
    Uint32 i = iter.bucket + 1;
    while(i <= mask && hashValues[i] == RNIL) i++;
    if(i <= mask){
      iter.bucket = i;
      iter.curr.i = hashValues[i];
      iter.curr.p = (T*)thePool.getPtr(iter.curr.i);    // cast
      return true;
    } else {
      iter.curr.i = RNIL;
      return false;
    }
  }
  
  iter.curr.i = iter.curr.p->nextHash;
  iter.curr.p = (T*)thePool.getPtr(iter.curr.i);        // cast
  return true;
}

template<class T, class U>
inline
void
DLHashTable2<T, U>::remove(Ptr<T> & ptr, const T & key){
  const Uint32 hv = key.hashValue() & mask;  
  
  Uint32 i;
  T * p;
  Ptr<T> prev;
  prev.i = RNIL;

  i = hashValues[hv];
  while(i != RNIL){
    p = (T*)thePool.getPtr(i);  // cast
    if(key.equal(* p)){
      const Uint32 next = p->nextHash;
      if(prev.i == RNIL){
	hashValues[hv] = next;
      } else {
	prev.p->nextHash = next;
      }
      
      if(next != RNIL){
	T * nextP = (T*)thePool.getPtr(next);   // cast
	nextP->prevHash = prev.i;
      }

      ptr.i = i;
      ptr.p = p;
      return;
    }
    prev.p = p;
    prev.i = i;
    i = p->nextHash;
  }
  ptr.i = RNIL;
}

template<class T, class U>
inline
void
DLHashTable2<T, U>::release(Ptr<T> & ptr, const T & key){
  const Uint32 hv = key.hashValue() & mask;  
  
  Uint32 i;
  T * p;
  Ptr<T> prev;
  prev.i = RNIL;

  i = hashValues[hv];
  while(i != RNIL){
    p = (T*)thePool.getPtr(i);  // cast
    if(key.equal(* p)){
      const Uint32 next = p->nextHash;
      if(prev.i == RNIL){
	hashValues[hv] = next;
      } else {
	prev.p->nextHash = next;
      }
      
      if(next != RNIL){
	T * nextP = (T*)thePool.getPtr(next);   // cast
	nextP->prevHash = prev.i;
      }

      p->~T();  // dtor
      thePool.release(i);
      ptr.i = i;
      ptr.p = p;        // invalid
      return;
    }
    prev.p = p;
    prev.i = i;
    i = p->nextHash;
  }
  ptr.i = RNIL;
}

template<class T, class U>
inline
void
DLHashTable2<T, U>::remove(Uint32 i){
  Ptr<T> tmp;
  tmp.i = i;
  tmp.p = (T*)thePool.getPtr(i);        // cast
  remove(tmp);
}

template<class T, class U>
inline
void
DLHashTable2<T, U>::release(Uint32 i){
  Ptr<T> tmp;
  tmp.i = i;
  tmp.p = (T*)thePool.getPtr(i);        // cast
  release(tmp);
}

template<class T, class U>
inline
void 
DLHashTable2<T, U>::remove(Ptr<T> & ptr){
  const Uint32 next = ptr.p->nextHash;
  const Uint32 prev = ptr.p->prevHash;

  if(prev != RNIL){
    T * prevP = (T*)thePool.getPtr(prev);       // cast
    prevP->nextHash = next;
  } else {
    const Uint32 hv = ptr.p->hashValue() & mask;  
    hashValues[hv] = next;
  }
  
  if(next != RNIL){
    T * nextP = (T*)thePool.getPtr(next);       // cast
    nextP->prevHash = prev;
  }
}

template<class T, class U>
inline
void 
DLHashTable2<T, U>::release(Ptr<T> & ptr){
  const Uint32 next = ptr.p->nextHash;
  const Uint32 prev = ptr.p->prevHash;

  if(prev != RNIL){
    T * prevP = (T*)thePool.getPtr(prev);       // cast
    prevP->nextHash = next;
  } else {
    const Uint32 hv = ptr.p->hashValue() & mask;  
    hashValues[hv] = next;
  }
  
  if(next != RNIL){
    T * nextP = (T*)thePool.getPtr(next);       // cast
    nextP->prevHash = prev;
  }
  
  thePool.release(ptr.i);
}

template<class T, class U>
inline
void 
DLHashTable2<T, U>::removeAll(){
  for(Uint32 i = 0; i<=mask; i++)
    hashValues[i] = RNIL;
}

template<class T, class U>
inline
bool
DLHashTable2<T, U>::next(Uint32 bucket, Iterator & iter) const {
  while (bucket <= mask && hashValues[bucket] == RNIL) 
    bucket++; 
  
  if (bucket > mask) {
    iter.bucket = bucket;
    iter.curr.i = RNIL;
    return false;
  }
  
  iter.bucket = bucket;
  iter.curr.i = hashValues[bucket];
  iter.curr.p = (T*)thePool.getPtr(iter.curr.i);        // cast
  return true;
}

template<class T, class U>
inline
bool
DLHashTable2<T, U>::seize(Ptr<T> & ptr){
  Ptr<U> ptr2;
  thePool.seize(ptr2);
  ptr.i = ptr2.i;
  ptr.p = (T*)ptr2.p;   // cast
  if (ptr.p != NULL){
    ptr.p->nextHash = RNIL;
    ptr.p->prevHash = RNIL;
    new (ptr.p) T;      // ctor
  }
  return !ptr.isNull();
}

template<class T, class U>
inline
void
DLHashTable2<T, U>::getPtr(Ptr<T> & ptr, Uint32 i) const {
  ptr.i = i;
  ptr.p = (T*)thePool.getPtr(i);        // cast
}

template<class T, class U>
inline
void
DLHashTable2<T, U>::getPtr(Ptr<T> & ptr) const {
  Ptr<U> ptr2;
  thePool.getPtr(ptr2);
  ptr.i = ptr2.i;
  ptr.p = (T*)ptr2.p;   // cast
}

template<class T, class U>
inline
T * 
DLHashTable2<T, U>::getPtr(Uint32 i) const {
  return (T*)thePool.getPtr(i); // cast
}

template<class T, class U>
inline
bool
DLHashTable2<T, U>::find(Ptr<T> & ptr, const T & key) const {
  const Uint32 hv = key.hashValue() & mask;  
  
  Uint32 i;
  T * p;

  i = hashValues[hv];
  while(i != RNIL){
    p = (T*)thePool.getPtr(i);  // cast
    if(key.equal(* p)){
      ptr.i = i;
      ptr.p = p;
      return true;
    }
    i = p->nextHash;
  }
  ptr.i = RNIL;
  ptr.p = NULL;
  return false;
}
#endif
