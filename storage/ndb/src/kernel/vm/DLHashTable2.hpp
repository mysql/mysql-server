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

#ifndef DL_HASHTABLE2_HPP
#define DL_HASHTABLE2_HPP

#include <ndb_global.h>

#define JAM_FILE_ID 307


/**
 * DLHashTable2 is a DLHashTable variant meant for cases where different
 * DLHashTable instances share a common pool (based on a union U).
 *
 * Calls T constructor after seize from pool and T destructor before
 * release (in all forms) into pool.
 */
template <class P, class T = typename P::Type>
class DLHashTable2 {
  typedef typename P::Type U;
public:
  DLHashTable2(P & thePool);
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
   * Note *must* be added using <b>add</b> (even before hash.release)
   *             or be released using pool
   */
  [[nodiscard]] bool seize(Ptr<T> &);

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
  P & thePool;
};

template<class P, class T>
inline
DLHashTable2<P, T>::DLHashTable2(P & _pool)
  : thePool(_pool)
{
  mask = 0;
  hashValues = 0;
}

template<class P, class T>
inline
DLHashTable2<P, T>::~DLHashTable2(){
  if(hashValues != 0)
    delete [] hashValues;
}

template<class P, class T>
inline
bool
DLHashTable2<P, T>::setSize(Uint32 size){
  Uint32 i = 1;
  while(i < size) i *= 2;

  if (hashValues != NULL)
  {
    /*
      If setSize() is called twice with different size values then this is 
      most likely a bug.
    */
    assert(mask == i-1); 
    // Return true if size already set to 'size', false otherwise.
    return mask == i-1;
  }
  
  mask = (i - 1);
  hashValues = new Uint32[i];
  for(Uint32 j = 0; j<i; j++)
    hashValues[j] = RNIL;
  
  return true;
}

template<class P, class T>
inline
void
DLHashTable2<P, T>::add(Ptr<T> & obj){
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
template<class P, class T>
inline
bool
DLHashTable2<P, T>::first(Iterator & iter) const {
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

template<class P, class T>
inline
bool
DLHashTable2<P, T>::next(Iterator & iter) const {
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

template<class P, class T>
inline
void
DLHashTable2<P, T>::remove(Ptr<T> & ptr, const T & key){
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

template<class P, class T>
inline
void
DLHashTable2<P, T>::release(Ptr<T> & ptr, const T & key){
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

template<class P, class T>
inline
void
DLHashTable2<P, T>::remove(Uint32 i){
  Ptr<T> tmp;
  tmp.i = i;
  tmp.p = (T*)thePool.getPtr(i);        // cast
  remove(tmp);
}

template<class P, class T>
inline
void
DLHashTable2<P, T>::release(Uint32 i){
  Ptr<T> tmp;
  tmp.i = i;
  tmp.p = (T*)thePool.getPtr(i);        // cast
  release(tmp);
}

template<class P, class T>
inline
void 
DLHashTable2<P, T>::remove(Ptr<T> & ptr){
  const Uint32 next = ptr.p->nextHash;
  const Uint32 prev = ptr.p->prevHash;

  if(prev != RNIL){
    T * prevP = (T*)thePool.getPtr(prev);       // cast
    prevP->nextHash = next;
  } else {
    const Uint32 hv = ptr.p->hashValue() & mask;  
    if (hashValues[hv] == ptr.i)
    {
      hashValues[hv] = next;
    }
    else
    {
      // Will add assert in 5.1
    }
  }
  
  if(next != RNIL){
    T * nextP = (T*)thePool.getPtr(next);       // cast
    nextP->prevHash = prev;
  }
}

template<class P, class T>
inline
void 
DLHashTable2<P, T>::release(Ptr<T> & ptr){
  const Uint32 next = ptr.p->nextHash;
  const Uint32 prev = ptr.p->prevHash;

  if(prev != RNIL){
    T * prevP = (T*)thePool.getPtr(prev);       // cast
    prevP->nextHash = next;
  } else {
    const Uint32 hv = ptr.p->hashValue() & mask;  
    if (hashValues[hv] == ptr.i)
    {
      hashValues[hv] = next;
    }
    else
    {
      // Will add assert in 5.1
    }
  }
  
  if(next != RNIL){
    T * nextP = (T*)thePool.getPtr(next);       // cast
    nextP->prevHash = prev;
  }
  
  thePool.release(ptr.i);
}

template<class P, class T>
inline
void 
DLHashTable2<P, T>::removeAll(){
  for(Uint32 i = 0; i<=mask; i++)
    hashValues[i] = RNIL;
}

template<class P, class T>
inline
bool
DLHashTable2<P, T>::next(Uint32 bucket, Iterator & iter) const {
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

template<class P, class T>
inline
bool
DLHashTable2<P, T>::seize(Ptr<T> & ptr){
  Ptr<U> ptr2;
  if (!thePool.seize(ptr2))
    return false;

  ptr.i = ptr2.i;
  ptr.p = (T*)ptr2.p;   // cast
  require(ptr.p != NULL);

  ptr.p->nextHash = RNIL;
  ptr.p->prevHash = RNIL;
  new (ptr.p) T;      // ctor
  return true;
}

template<class P, class T>
inline
void
DLHashTable2<P, T>::getPtr(Ptr<T> & ptr, Uint32 i) const {
  ptr.i = i;
  ptr.p = (T*)thePool.getPtr(i);        // cast
}

template<class P, class T>
inline
void
DLHashTable2<P, T>::getPtr(Ptr<T> & ptr) const {
  Ptr<U> ptr2;
  thePool.getPtr(ptr2);
  ptr.i = ptr2.i;
  ptr.p = (T*)ptr2.p;   // cast
}

template<class P, class T>
inline
T * 
DLHashTable2<P, T>::getPtr(Uint32 i) const {
  return (T*)thePool.getPtr(i); // cast
}

template<class P, class T>
inline
bool
DLHashTable2<P, T>::find(Ptr<T> & ptr, const T & key) const {
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

#undef JAM_FILE_ID

#endif
