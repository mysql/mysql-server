/*
   Copyright (c) 2010, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef DATA_BUFFER_HPP
#define DATA_BUFFER_HPP

#include <ndb_limits.h>
#include <ErrorReporter.hpp>
#include <NdbOut.hpp>
#include "Pool.hpp"

#define JAM_FILE_ID 274


/**
 * @class  DataBuffer
 * @brief  Buffer of data words
 *
 * @note   The buffer is divided into segments (of size sz)
 */
template<Uint32 sz>
  struct DataBufferSegment {
    Uint32 magic;
    Uint32 nextPool;
    Uint32 data[sz];
    NdbOut& print(NdbOut& out){
      out << "[DataBuffer<" << sz << ">::Segment this="
	  << this << dec << " nextPool= "
	  << nextPool << " ]";
      return out;
    }
  /**
  
 * Head/anchor for data buffer
   */
  struct HeadPOD
  {
    Uint32 used;       // Words used
    Uint32 firstItem;  // First segment (or RNIL)
    Uint32 lastItem;   // Last segment (or RNIL)
#if defined VM_TRACE || defined ERROR_INSERT
    bool in_use;
#endif

    void init() { used = 0; firstItem = lastItem = RNIL;
#if defined VM_TRACE || defined ERROR_INSERT
  in_use = false;
#endif
    }

    /**
     * Get size of databuffer, in words
     */
    Uint32 getSize() const { return used;}

    /**
     * Get segment size in words (template argument)
     */
    static Uint32 getSegmentSize() { return sz;}
  };

  struct Head : public HeadPOD
  {
    Head();

    Head& operator=(const HeadPOD& src) {
      this->used = src.used;
      this->firstItem = src.firstItem;
      this->lastItem = src.lastItem;
#if defined VM_TRACE || defined ERROR_INSERT
require(src.in_use);
      this->in_use = src.in_use;
#endif
      return *this;
    }
  };

  };

template <Uint32 sz, typename Pool>
class DataBuffer {
public:
  typedef DataBufferSegment<sz> Segment;
  typedef typename DataBufferSegment<sz>::HeadPOD HeadPOD;
  typedef typename DataBufferSegment<sz>::Head Head;
public:
  typedef Pool DataBufferPool;

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

  struct ConstDataBufferIterator;
  struct DataBufferIterator {
    Ptr<Segment>       curr;  // Ptr to current segment
    Uint32*            data;  // Pointer to current data (word)
    Uint32             ind;   // Word index within a segment
    Uint32             pos;   // Absolute word position within DataBuffer

    void print(FILE* out) {
      fprintf(out, "[DataBufferIterator curr.i=%d, data=%p, ind=%d, pos=%d]\n",
	      curr.i, (void*) data, ind, pos);
    };

    inline void assign(const ConstDataBufferIterator& src);
    inline bool isNull() const { return curr.isNull();}
    inline void setNull() { curr.setNull(); data = 0; ind = pos = RNIL;}
  };
  typedef DataBufferIterator Iterator;

  struct ConstDataBufferIterator {
    ConstPtr<Segment>  curr;
    const Uint32 *     data;
    Uint32             ind;
    Uint32             pos;

    inline void assign(const DataBufferIterator& src);
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

  static void createRecordInfo(Record_info & ri, Uint32 type_id);
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

template<Uint32 sz, typename Pool>
class LocalDataBuffer : public DataBuffer<sz, Pool> {
public:
  LocalDataBuffer(typename DataBuffer<sz, Pool>::DataBufferPool & thePool,
                   typename DataBuffer<sz, Pool>::HeadPOD & _src)
    : DataBuffer<sz, Pool>(thePool), src(_src)
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
  typename DataBuffer<sz, Pool>::HeadPOD & src;
};

template<Uint32 sz>
inline
DataBufferSegment<sz>::Head::Head(){
  this->init();
}

template<Uint32 sz, typename Pool>
inline
bool DataBuffer<sz, Pool>::importable(const DataBufferIterator it, Uint32 len){
  return (it.pos + len < head.used);
}

template<Uint32 sz, typename Pool>
inline
bool DataBuffer<sz, Pool>::position(DataBufferIterator& it, Uint32 p){

  // TODO: The current implementation is not the most effective one.
  //       A more effective implementation would start at the current
  //       position of the iterator.

  if(!first(it)){
    return false;
  }
  return next(it, p);
}

template<Uint32 sz, typename Pool>
inline
bool
DataBuffer<sz, Pool>::import(const DataBufferIterator & it,
                              const Uint32* src, Uint32 len)
{
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
}

template<Uint32 sz, typename Pool>
inline
bool
DataBuffer<sz, Pool>::append(const Uint32* src, Uint32 len){
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

template<Uint32 sz, typename Pool>
inline
void DataBuffer<sz, Pool>::print(FILE* out) const {
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
    ptr.p = (Segment*)thePool.getPtr(ptr.i);
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

template<Uint32 sz, typename Pool>
inline
DataBuffer<sz, Pool>::DataBuffer(DataBufferPool & p) : thePool(p){
}

template<Uint32 sz, typename Pool>
inline
bool
DataBuffer<sz, Pool>::seize(Uint32 n){
  Uint32 rest; // Free space in last segment (currently)

  if(head.firstItem == RNIL)
  {
    rest = 0;
  }
  else
  {
    rest = (sz - (head.used % sz)) % sz;
  }

  if (0)
    ndbout_c("seize(%u) used: %u rest: %u firstItem: 0x%x",
             n, head.used, rest, head.firstItem);

  if (rest >= n)
  {
    head.used += n;
    return true;
  }

  Uint32 used = head.used + n;
  Segment first;
  Ptr<Segment> currPtr;
  currPtr.p = &first;
  first.nextPool = RNIL;

  while (n >= sz)
  {
    Ptr<Segment> tmp;
    if (thePool.seize(tmp))
    {
      currPtr.p->nextPool = tmp.i;
      currPtr.i = tmp.i;
      currPtr.p = static_cast<Segment*>(tmp.p);
    }
    else
    {
      goto error;
    }
    n -= sz;
  }

  if(n > rest)
  {
    Ptr<Segment> tmp;
    if (thePool.seize(tmp))
    {
      currPtr.p->nextPool = tmp.i;
      currPtr.i = tmp.i;
      currPtr.p = static_cast<Segment*>(tmp.p);
    }
    else
    {
      goto error;
    }
  }

  if (head.firstItem == RNIL)
  {
    head.firstItem = first.nextPool;
  }
  else
  {
    Segment* lastPtr = static_cast<Segment*>(thePool.getPtr(head.lastItem));
    lastPtr->nextPool = first.nextPool;
  }

  head.used = used;
  head.lastItem = currPtr.i;
  currPtr.p->nextPool = RNIL;
  return true;

error:
  currPtr.i = first.nextPool;
  while (currPtr.i != RNIL)
  {
    currPtr.p = static_cast<Segment*>(thePool.getPtr(currPtr.i));
    Ptr<Segment> tmp;
    tmp.i = currPtr.i;
    tmp.p = currPtr.p;
    currPtr.i = currPtr.p->nextPool;
    thePool.release(tmp);
  }
  return false;
}

#ifdef DATA_BUFFER_RELEASE_ARRAY_POOL_SPECIALIZATION_NOT_WORKING
template<Uint32 sz>
inline
void
DataBuffer<sz,ArrayPool<DataBufferSegment<sz> > >::release()
{
  Uint32 used = head.used + sz - 1;
  if(head.firstItem != RNIL){
    thePool.releaseList(used / sz, head.firstItem, head.lastItem);
    head.used = 0;
    head.firstItem = RNIL;
    head.lastItem = RNIL;
  }
}
#endif

template<Uint32 sz, typename Pool>
inline
void
DataBuffer<sz, Pool>::release(){
  Ptr<Segment> tmp;
  tmp.i = head.firstItem;
  while (tmp.i != RNIL)
  {
    tmp.p = thePool.getPtr(tmp.i);
    Uint32 next = static_cast<Segment*>(tmp.p)->nextPool;
    thePool.release(tmp);
    tmp.i = next;
  }
head.firstItem = head.lastItem = RNIL;
head.used = 0;
}

template<Uint32 sz, typename Pool>
inline
Uint32
DataBuffer<sz, Pool>::getSegmentSize(){
  return sz;
}

template<Uint32 sz, typename Pool>
inline
bool
DataBuffer<sz, Pool>::first(DataBufferIterator & it){
  ConstDataBufferIterator tmp;
  bool ret = first(tmp);
  it.assign(tmp);
  return ret;
}

template<Uint32 sz, typename Pool>
inline
bool
DataBuffer<sz, Pool>::next(DataBufferIterator & it){
  ConstDataBufferIterator tmp;
  tmp.assign(it);
  bool ret = next(tmp);
  it.assign(tmp);
  return ret;
}

template<Uint32 sz, typename Pool>
inline
bool
DataBuffer<sz, Pool>::next(DataBufferIterator & it, Uint32 hops){
  ConstDataBufferIterator tmp;
  tmp.assign(it);
  bool ret = next(tmp, hops);
  it.assign(tmp);
  return ret;
}

template<Uint32 sz, typename Pool>
inline
bool
DataBuffer<sz, Pool>::first(ConstDataBufferIterator & it) const {
  it.curr.i = head.firstItem;
  if(it.curr.i == RNIL){
    it.setNull();
    return false;
  }
  it.curr.p = static_cast<Segment*>(thePool.getPtr(it.curr.i));
  it.data = &it.curr.p->data[0];
  it.ind = 0;
  it.pos = 0;
  return true;
}

template<Uint32 sz, typename Pool>
inline
bool
DataBuffer<sz, Pool>::next(ConstDataBufferIterator & it) const {
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
      ErrorReporter::handleAssert("DataBuffer<sz, Pool>::next", __FILE__, __LINE__);
    }
#endif
    it.curr.p = static_cast<Segment*>(thePool.getPtr(it.curr.i));
    it.data = &it.curr.p->data[0];
    it.ind = 0;
    return true;
  }
  it.setNull();
  return false;
}

template<Uint32 sz, typename Pool>
inline
bool
DataBuffer<sz, Pool>::next(ConstDataBufferIterator & it, Uint32 hops) const {
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
      it.curr.p = static_cast<Segment*>(thePool.getPtr(it.curr.i));
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
    it.curr.p = static_cast<Segment*>(thePool.getPtr(it.curr.i));
    it.ind -= sz;
    it.data = &it.curr.p->data[it.ind];
    return true;
  }
  it.setNull();
  return false;
#endif
}

template<Uint32 sz, typename Pool>
inline
Uint32
DataBuffer<sz, Pool>::getSize() const {
  return head.used;
}

template<Uint32 sz, typename Pool>
inline
bool
DataBuffer<sz, Pool>::isEmpty() const {
  return (head.used == 0);
}

template<Uint32 sz, typename Pool>
inline
void
DataBuffer<sz, Pool>::createRecordInfo(Record_info & ri, Uint32 type_id)
{
  Segment tmp;
  const char * off_base = (char*)&tmp;
  const char * off_next = (char*)&tmp.nextPool;
  const char * off_magic = (char*)&tmp.magic;

  ri.m_size = sizeof(tmp);
  ri.m_offset_next_pool = Uint32(off_next - off_base);
  ri.m_offset_magic = Uint32(off_magic - off_base);
  ri.m_type_id = type_id;
}

template<Uint32 sz, typename Pool>
inline
void
DataBuffer<sz, Pool>::DataBufferIterator::assign(const ConstDataBufferIterator & src)
{
  this->curr.i = src.curr.i;
  this->curr.p = const_cast<Segment*>(src.curr.p);
  this->data = const_cast<Uint32*>(src.data);
  this->ind = src.ind;
  this->pos = src.pos;
}

template<Uint32 sz, typename Pool>
inline
void
DataBuffer<sz, Pool>::ConstDataBufferIterator::assign(const DataBufferIterator & src)
{
  this->curr.i = src.curr.i;
  this->curr.p = src.curr.p;
  this->data = src.data;
  this->ind = src.ind;
  this->pos = src.pos;
}


#undef JAM_FILE_ID

#endif

