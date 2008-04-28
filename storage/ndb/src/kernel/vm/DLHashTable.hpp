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

#ifndef DL_HASHTABLE_HPP
#define DL_HASHTABLE_HPP

#include <ndb_global.h>
#include "ArrayPool.hpp"

/**
 * DLHashTable implements a hashtable using chaining
 *   (with a double linked list)
 *
 * The entries in the hashtable must have the following methods:
 *  -# bool equal(const class T &) const;
 *     Which should return equal if the to objects have the same key
 *  -# Uint32 hashValue() const;
 *     Which should return a 32 bit hashvalue
 */
template <typename P, typename T, typename U = T>
class DLHashTableImpl 
{
public:
  DLHashTableImpl(P & thePool);
  ~DLHashTableImpl();
  
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
  
private:
  Uint32 mask;
  Uint32 * hashValues;
  P & thePool;
};

template <typename P, typename T, typename U>
inline
DLHashTableImpl<P, T, U>::DLHashTableImpl(P & _pool)
  : thePool(_pool)
{
  mask = 0;
  hashValues = 0;
}

template <typename P, typename T, typename U>
inline
DLHashTableImpl<P, T, U>::~DLHashTableImpl()
{
  if(hashValues != 0)
    delete [] hashValues;
}

template <typename P, typename T, typename U>
inline
bool
DLHashTableImpl<P, T, U>::setSize(Uint32 size)
{
  Uint32 i = 1;
  while(i < size) i *= 2;

  if(mask == (i - 1))
  {
    /**
     * The size is already set to <b>size</b>
     */
    return true;
  }

  if(mask != 0)
  {
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

template <typename P, typename T, typename U>
inline
void
DLHashTableImpl<P, T, U>::add(Ptr<T> & obj)
{
  const Uint32 hv = obj.p->hashValue() & mask;
  const Uint32 i  = hashValues[hv];
  
  if(i == RNIL)
  {
    hashValues[hv] = obj.i;
    obj.p->U::nextHash = RNIL;
    obj.p->U::prevHash = RNIL;
  } 
  else 
  {
    T * tmp = thePool.getPtr(i);
    tmp->U::prevHash = obj.i;
    obj.p->U::nextHash = i;
    obj.p->U::prevHash = RNIL;
    
    hashValues[hv] = obj.i;
  }
}

/**
 * First element
 */
template <typename P, typename T, typename U>
inline
bool
DLHashTableImpl<P, T, U>::first(Iterator & iter) const 
{
  Uint32 i = 0;
  while(i <= mask && hashValues[i] == RNIL) i++;
  if(i <= mask)
  {
    iter.bucket = i;
    iter.curr.i = hashValues[i];
    iter.curr.p = thePool.getPtr(iter.curr.i);
    return true;
  }
  else 
  {
    iter.curr.i = RNIL;
  }
  return false;
}

template <typename P, typename T, typename U>
inline
bool
DLHashTableImpl<P, T, U>::next(Iterator & iter) const 
{
  if(iter.curr.p->U::nextHash == RNIL)
  {
    Uint32 i = iter.bucket + 1;
    while(i <= mask && hashValues[i] == RNIL) i++;
    if(i <= mask)
    {
      iter.bucket = i;
      iter.curr.i = hashValues[i];
      iter.curr.p = thePool.getPtr(iter.curr.i);
      return true;
    }
    else 
    {
      iter.curr.i = RNIL;
      return false;
    }
  }
  
  iter.curr.i = iter.curr.p->U::nextHash;
  iter.curr.p = thePool.getPtr(iter.curr.i);
  return true;
}

template <typename P, typename T, typename U>
inline
void
DLHashTableImpl<P, T, U>::remove(Ptr<T> & ptr, const T & key)
{
  const Uint32 hv = key.hashValue() & mask;  
  
  Uint32 i;
  T * p;
  Ptr<T> prev;
  LINT_INIT(prev.p);
  prev.i = RNIL;

  i = hashValues[hv];
  while(i != RNIL)
  {
    p = thePool.getPtr(i);
    if(key.equal(* p))
    {
      const Uint32 next = p->U::nextHash;
      if(prev.i == RNIL)
      {
	hashValues[hv] = next;
      } 
      else 
      {
	prev.p->U::nextHash = next;
      }
      
      if(next != RNIL)
      {
	T * nextP = thePool.getPtr(next);
	nextP->U::prevHash = prev.i;
      }
      
      ptr.i = i;
      ptr.p = p;
      return;
    }
    prev.p = p;
    prev.i = i;
    i = p->U::nextHash;
  }
  ptr.i = RNIL;
}

template <typename P, typename T, typename U>
inline
void
DLHashTableImpl<P, T, U>::remove(Uint32 i)
{
  Ptr<T> tmp;
  tmp.i = i;
  tmp.p = thePool.getPtr(i);
  remove(tmp);
}

template <typename P, typename T, typename U>
inline
void
DLHashTableImpl<P, T, U>::release(Uint32 i)
{
  Ptr<T> tmp;
  tmp.i = i;
  tmp.p = thePool.getPtr(i);
  release(tmp);
}

template <typename P, typename T, typename U>
inline
void 
DLHashTableImpl<P, T, U>::remove(Ptr<T> & ptr)
{
  const Uint32 next = ptr.p->U::nextHash;
  const Uint32 prev = ptr.p->U::prevHash;

  if(prev != RNIL)
  {
    T * prevP = thePool.getPtr(prev);
    prevP->U::nextHash = next;
  } 
  else 
  {
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
  
  if(next != RNIL)
  {
    T * nextP = thePool.getPtr(next);
    nextP->U::prevHash = prev;
  }
}

template <typename P, typename T, typename U>
inline
void 
DLHashTableImpl<P, T, U>::release(Ptr<T> & ptr)
{
  const Uint32 next = ptr.p->U::nextHash;
  const Uint32 prev = ptr.p->U::prevHash;

  if(prev != RNIL)
  {
    T * prevP = thePool.getPtr(prev);
    prevP->U::nextHash = next;
  } 
  else 
  {
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
  
  if(next != RNIL)
  {
    T * nextP = thePool.getPtr(next);
    nextP->U::prevHash = prev;
  }
  
  thePool.release(ptr);
}

template <typename P, typename T, typename U>
inline
void 
DLHashTableImpl<P, T, U>::removeAll()
{
  for(Uint32 i = 0; i<=mask; i++)
    hashValues[i] = RNIL;
}

template <typename P, typename T, typename U>
inline
bool
DLHashTableImpl<P, T, U>::next(Uint32 bucket, Iterator & iter) const 
{
  while (bucket <= mask && hashValues[bucket] == RNIL) 
    bucket++; 
  
  if (bucket > mask) 
  {
    iter.bucket = bucket;
    iter.curr.i = RNIL;
    return false;
  }
  
  iter.bucket = bucket;
  iter.curr.i = hashValues[bucket];
  iter.curr.p = thePool.getPtr(iter.curr.i);
  return true;
}

template <typename P, typename T, typename U>
inline
bool
DLHashTableImpl<P, T, U>::seize(Ptr<T> & ptr)
{
  if(thePool.seize(ptr)){
    ptr.p->U::nextHash = ptr.p->U::prevHash = RNIL;
    return true;
  }
  return false;
}

template <typename P, typename T, typename U>
inline
void
DLHashTableImpl<P, T, U>::getPtr(Ptr<T> & ptr, Uint32 i) const 
{
  ptr.i = i;
  ptr.p = thePool.getPtr(i);
}

template <typename P, typename T, typename U>
inline
void
DLHashTableImpl<P, T, U>::getPtr(Ptr<T> & ptr) const 
{
  thePool.getPtr(ptr);
}

template <typename P, typename T, typename U>
inline
T * 
DLHashTableImpl<P, T, U>::getPtr(Uint32 i) const 
{
  return thePool.getPtr(i);
}

template <typename P, typename T, typename U>
inline
bool
DLHashTableImpl<P, T, U>::find(Ptr<T> & ptr, const T & key) const 
{
  const Uint32 hv = key.hashValue() & mask;  
  
  Uint32 i;
  T * p;

  i = hashValues[hv];
  while(i != RNIL)
  {
    p = thePool.getPtr(i);
    if(key.equal(* p))
    {
      ptr.i = i;
      ptr.p = p;
      return true;
    }
    i = p->U::nextHash;
  }
  ptr.i = RNIL;
  ptr.p = NULL;
  return false;
}

// Specializations

template <typename T, typename U = T>
class DLHashTable : public DLHashTableImpl<ArrayPool<T>, T, U>
{
public:
  DLHashTable(ArrayPool<T> & p) : DLHashTableImpl<ArrayPool<T>, T, U>(p) {}
};

#endif
