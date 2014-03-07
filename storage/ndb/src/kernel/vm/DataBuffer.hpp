/*
   Copyright (c) 2003, 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef DATA_BUFFER_HPP
#define DATA_BUFFER_HPP

#include "ArrayPool.hpp"

#define JAM_FILE_ID 274


/**
 * @class  DataBuffer
 * @brief  Buffer of data words
 * 
 * @note   The buffer is divided into segments (of size sz)
 */
template <Uint32 sz>
class DataBuffer {
public:
  struct Segment {
    Uint32 nextPool;
    Uint32 data[sz];
    NdbOut& print(NdbOut& out){
      out << "[DataBuffer<" << sz << ">::Segment this=" 
	  << this << dec << " nextPool= "
	  << nextPool << " ]";
      return out;
    }
  };
public:
  typedef ArrayPool<Segment> DataBufferPool;

  /**
   * Head/anchor for data buffer
   */
  struct Head {
    Head() ;

    Uint32 used;       // Words used
    Uint32 firstItem;  // First segment (or RNIL)
    Uint32 lastItem;   // Last segment (or RNIL)

#if defined VM_TRACE || defined ERROR_INSERT
    bool in_use;
#endif

    /** 
     * Get size of databuffer, in words
     */
    Uint32 getSize() const { return used;}
    
    /** 
     * Get segment size in words (template argument) 
     */
    static Uint32 getSegmentSize() { return sz;}
  };

  /** Constructor */
  DataBuffer(DataBufferPool &);

  /** Seize <b>n</b> words, Release */
  bool seize(Uint32 n);
  void release();

  /** 
   * Get size of databuffer, in words
   */
  Uint32 getSize() const;
  
  /** 
   * Check if buffer is empty
   */
  bool isEmpty() const;
  
  /**
   * Get segment size in words (template argument) 
   */
  static Uint32 getSegmentSize();
  
  void print(FILE*) const;

  /* ----------------------------------------------------------------------- */

  struct DataBufferIterator {
    Ptr<Segment>       curr;  // Ptr to current segment
    Uint32*            data;  // Pointer to current data (word)
    Uint32             ind;   // Word index within a segment
    Uint32             pos;   // Absolute word position within DataBuffer

    void print(FILE* out) {
      fprintf(out, "[DataBufferIterator curr.i=%d, data=%p, ind=%d, pos=%d]\n",
	      curr.i, (void*) data, ind, pos);
    };

    inline bool isNull() const { return curr.isNull();}
    inline void setNull() { curr.setNull(); data = 0; ind = pos = RNIL;}
  };
  typedef DataBufferIterator Iterator;

  struct ConstDataBufferIterator {
    ConstPtr<Segment>  curr;
    const Uint32 *     data;
    Uint32             ind;
    Uint32             pos;

    inline bool isNull() const { return curr.isNull();}
    inline void setNull() { curr.setNull(); data = 0; ind = pos = RNIL;}
  };

  /** 
   * Iterator 
   * @parameter hops  Number of words to jump forward
   * @note DataBuffer::next returns false if applied to last word.
   */
  bool first(DataBufferIterator &);
  bool next(DataBufferIterator &);
  bool next(DataBufferIterator &, Uint32 hops);
  bool nextPool(DataBufferIterator &);

  /**
   * Set iterator to position
   */
  bool position(DataBufferIterator& it, Uint32 pos);

  /** Iterator */
  bool first(ConstDataBufferIterator &) const;
  bool next(ConstDataBufferIterator &) const;
  bool next(ConstDataBufferIterator &, Uint32 hops) const;
  bool nextPool(ConstDataBufferIterator &) const;

  /** 
   * Returns true if it is possible to store <em>len</em> 
   * no of words at position given in iterator.
   */
  bool importable(const DataBufferIterator, Uint32 len);

  /**
   * Stores <em>len</em> no of words starting at location <em>src</em> in
   * databuffer at position given in iterator.
   * 
   * @return true if success, false otherwise.
   * @note Iterator is not advanced.
   */
  bool import(const DataBufferIterator &, const Uint32* src, Uint32 len);

  /**
   * Increases size with appends <em>len</em> words
   * @return true if success, false otherwise.
   */
  bool append(const Uint32* src, Uint32 len);

protected:
  Head head;
  DataBufferPool &  thePool;

private:
  /**
   * This is NOT a public method, since the intension is that the import
   * method using iterators will be more effective in the future 
   */
  bool import(Uint32 pos, const Uint32* src, Uint32 len);
};

template<Uint32 sz>
class LocalDataBuffer : public DataBuffer<sz> {
public:
  LocalDataBuffer(typename DataBuffer<sz>::DataBufferPool & thePool, 
		  typename DataBuffer<sz>::Head & _src)
    : DataBuffer<sz>(thePool), src(_src)
  {
#if defined VM_TRACE || defined ERROR_INSERT
    if (src.in_use == true)
      abort();
    src.in_use = true;
#endif
    this->head = src;
  }

  ~LocalDataBuffer(){
    src = this->head;
#if defined VM_TRACE || defined ERROR_INSERT
    if (src.in_use == false)
      abort();
    src.in_use = false;
#endif
  }
private:
  typename DataBuffer<sz>::Head & src;
};

template<Uint32 sz>
inline
DataBuffer<sz>::Head::Head(){
  used = 0;
  firstItem = RNIL;
  lastItem = RNIL;
#if defined VM_TRACE || defined ERROR_INSERT
  in_use = false;
#endif
}

template<Uint32 sz>
inline
bool DataBuffer<sz>::importable(const DataBufferIterator it, Uint32 len){
  return (it.pos + len < head.used);
}

template<Uint32 sz>
inline
bool DataBuffer<sz>::position(DataBufferIterator& it, Uint32 p){

  // TODO: The current implementation is not the most effective one.
  //       A more effective implementation would start at the current 
  //       position of the iterator.

  if(!first(it)){
    return false;
  }
  return next(it, p);
}
  
template<Uint32 sz>
inline
bool 
DataBuffer<sz>::import(const DataBufferIterator & it, 
		       const Uint32* src, Uint32 len){

#if 0
  DataBufferIterator it;
  position(it, _it.pos);
  
  for(; len > 0; len--){
    Uint32 s = * src;
    * it.data = s;
    next(it);
    src++;
  }
  return true;
#else
  Uint32 ind = (it.pos % sz);
  Uint32 left = sz  - ind;
  Segment * p = it.curr.p;

  while(len > left){
    memcpy(&p->data[ind], src, 4 * left);
    src += left;
    len -= left;
    ind = 0;
    left = sz;
    p = thePool.getPtr(p->nextPool);
  }
  
  memcpy(&p->data[ind], src, 4 * len);      
  return true;
#endif
}

template<Uint32 sz>
inline
bool 
DataBuffer<sz>::append(const Uint32* src, Uint32 len){
  if(len == 0)
    return true;
  
  Uint32 pos = head.used;
  if(!seize(len)){
    return false;
  }
  DataBufferIterator it;

  bool ok = position(it, pos);
  require(ok);
  ok = import(it, src, len);
  require(ok);
  return true;
}

template<Uint32 sz>
inline
void DataBuffer<sz>::print(FILE* out) const {
  fprintf(out, "[DataBuffer used=%d words, segmentsize=%d words",
	  head.used, sz);

  if (head.firstItem == RNIL) {
    fprintf(out, ": No segments seized.]\n");
    return;
  } else {
    fprintf(out, "\n");
  }

  Ptr<Segment> ptr;
  ptr.i = head.firstItem;
  
  Uint32 acc = 0;
  for(; ptr.i != RNIL; ){
    thePool.getPtr(ptr);
    const Uint32 * rest = ptr.p->data;
    for(Uint32 i = 0; i<sz; i++){
      fprintf(out, " H'%.8x", rest[i]);
      if(acc++ == 6){
	acc = 0;
	fprintf(out, "\n");
      }
    }
    ptr.i = ptr.p->nextPool;
  }
  fprintf(out, " ]\n");
}

template<Uint32 sz>
inline
DataBuffer<sz>::DataBuffer(DataBufferPool & p) : thePool(p){
}

template<Uint32 sz>
inline
bool
DataBuffer<sz>::seize(Uint32 n)
{
  Uint32 req = n;
  Uint32 used = head.used;
  Uint32 last = used % sz;            // (almost) used in last segment
  Uint32 rest = last ? sz - last : 0; // Free in last segment

  if (rest >= n)
  {
    /**
     * No extra allocation needed
     */
    head.used = used + n;
    return true;
  }

  n -= rest;

  /**
   * Check for space
   */
  Uint32 free = thePool.getNoOfFree() * sz;
  if (n > free)
  {
    return false;
  }
  
  Ptr<Segment> firstPtr;
  thePool.seize(firstPtr);
  Ptr<Segment> lastPtr = firstPtr;
  
  while (n > sz)
  {
    Ptr<Segment> tmp;
    thePool.seize(tmp);
    lastPtr.p->nextPool = tmp.i;
    lastPtr = tmp;
    n -= sz;
  }
  lastPtr.p->nextPool = RNIL;
  
  head.used = used + req;
  if (head.firstItem == RNIL)
  {
    head.firstItem = firstPtr.i;
    assert(head.lastItem == RNIL);
  }
  else
  {
    Segment* tail = thePool.getPtr(head.lastItem);
    assert(tail->nextPool == RNIL);
    tail->nextPool = firstPtr.i;
  }
  head.lastItem = lastPtr.i;
  
#if 0
  {
    ndbout_c("Before validate - %d", head.used);
    if(head.used == 0){
      assert(head.firstItem == RNIL);
      assert(head.lastItem == RNIL);
    } else {
      Ptr<Segment> tmp;
      tmp.i = head.firstItem;
      for(Uint32 i = head.used; i > sz; i -= sz){
	ndbout << tmp.i << " ";
	tmp.p = thePool.getPtr(tmp.i);
	tmp.i = tmp.p->nextPool;
      }
      ndbout_c("%d", tmp.i);
      assert(head.lastItem == tmp.i);
    }
    ndbout_c("After validate");
  }
#endif
  return true;
}

template<Uint32 sz>
inline
void
DataBuffer<sz>::release(){
  Uint32 used = head.used + sz - 1;
  if(head.firstItem != RNIL){
    thePool.releaseList(used / sz, head.firstItem, head.lastItem);
    head.used = 0;
    head.firstItem = RNIL;
    head.lastItem = RNIL;
  }
}

template<Uint32 sz>
inline
Uint32
DataBuffer<sz>::getSegmentSize(){
  return sz;
}

template<Uint32 sz>
inline
bool
DataBuffer<sz>::first(DataBufferIterator & it){
  it.curr.i = head.firstItem;
  if(it.curr.i == RNIL){
    it.setNull();
    return false;
  }
  thePool.getPtr(it.curr);
  it.data = &it.curr.p->data[0];
  it.ind = 0;
  it.pos = 0;
  return true;
}

template<Uint32 sz>
inline
bool
DataBuffer<sz>::next(DataBufferIterator & it){
  it.ind ++;
  it.data ++;
  it.pos ++;
  if(it.ind < sz && it.pos < head.used){
    return true;
  }

  if(it.pos < head.used){
    it.curr.i = it.curr.p->nextPool;
#ifdef ARRAY_GUARD
    if(it.curr.i == RNIL){
      /**
       * This is actually "internal error"
       * pos can't be less than head.used and at the same time we can't
       * find next segment
       *
       * Note this must not "really" be checked since thePool.getPtr will
       *  abort when trying to get RNIL. That's why the check is within
       *  ARRAY_GUARD
       */
      ErrorReporter::handleAssert("DataBuffer<sz>::next", __FILE__, __LINE__);
    }
#endif
    thePool.getPtr(it.curr);
    it.data = &it.curr.p->data[0];
    it.ind = 0;
    return true;
  }
  it.setNull();
  return false;
}

template<Uint32 sz>
inline
bool
DataBuffer<sz>::next(DataBufferIterator & it, Uint32 hops){
#if 0
  for (Uint32 i=0; i<hops; i++) {
    if (!this->next(it))
      return false;
  }
  return true;
#else
  if(it.pos + hops < head.used){
    while(hops >= sz){
      it.curr.i = it.curr.p->nextPool;
      thePool.getPtr(it.curr);
      hops -= sz;
      it.pos += sz;
    }

    it.ind += hops;
    it.pos += hops;
    if(it.ind < sz){
      it.data = &it.curr.p->data[it.ind];
      return true;
    }

    it.curr.i = it.curr.p->nextPool;
    thePool.getPtr(it.curr);
    it.ind -= sz;
    it.data = &it.curr.p->data[it.ind];
    return true;
  }
  it.setNull();
  return false;
#endif
}

template<Uint32 sz>
inline
bool
DataBuffer<sz>::first(ConstDataBufferIterator & it) const {
  it.curr.i = head.firstItem;
  if(it.curr.i == RNIL){
    it.setNull();
    return false;
  }
  thePool.getPtr(it.curr);
  it.data = &it.curr.p->data[0];
  it.ind = 0;
  it.pos = 0;
  return true;
}

template<Uint32 sz>
inline
bool
DataBuffer<sz>::next(ConstDataBufferIterator & it) const {
  it.ind ++;
  it.data ++;
  it.pos ++;
  if(it.ind < sz && it.pos < head.used){
    return true;
  }

  if(it.pos < head.used){
    it.curr.i = it.curr.p->nextPool;
#ifdef ARRAY_GUARD    
    if(it.curr.i == RNIL){
      /**
       * This is actually "internal error"
       * pos can't be less than head.used and at the same time we can't 
       * find next segment
       * 
       * Note this must not "really" be checked since thePool.getPtr will
       *  abort when trying to get RNIL. That's why the check is within
       *  ARRAY_GUARD
       */
      ErrorReporter::handleAssert("DataBuffer<sz>::next", __FILE__, __LINE__);
    }
#endif
    thePool.getPtr(it.curr);
    it.data = &it.curr.p->data[0];
    it.ind = 0;
    return true;
  }
  it.setNull();
  return false;
}

template<Uint32 sz>
inline
bool
DataBuffer<sz>::next(ConstDataBufferIterator & it, Uint32 hops) const {
#if 0
  for (Uint32 i=0; i<hops; i++) {
    if (!this->next(it))
      return false;
  }
  return true;
#else
  if(it.pos + hops < head.used){
    while(hops >= sz){
      it.curr.i = it.curr.p->nextPool;
      thePool.getPtr(it.curr);
      hops -= sz;
      it.pos += sz;
    }

    it.ind += hops;
    it.pos += hops;
    if(it.ind < sz){
      it.data = &it.curr.p->data[it.ind];
      return true;
    }
    
    it.curr.i = it.curr.p->nextPool;
    thePool.getPtr(it.curr);
    it.ind -= sz;
    it.data = &it.curr.p->data[it.ind];
    return true;
  }
  it.setNull();
  return false;
#endif
}

template<Uint32 sz>
inline
Uint32
DataBuffer<sz>::getSize() const {
  return head.used;
}

template<Uint32 sz>
inline
bool
DataBuffer<sz>::isEmpty() const {
  return (head.used == 0);
}


#undef JAM_FILE_ID

#endif

