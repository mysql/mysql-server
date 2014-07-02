/*
   Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef NDB_INTRUSIVE_LIST_HPP
#define NDB_INTRUSIVE_LIST_HPP

/**
 * IntrusiveList implements an family of intrusive list.
 *
 * The following specialisation are defined:
 *
 * SLList - single linked list with only first in head
 * DLList - double linked list with only first in head
 * SLCList - single linked list with first and count in head
 * DLCList - double linked list with first and count in head
 * SLFifoList - single linked list with both first and last in head
 * DLFifoList - double linked list with both first and last in head
 * SLCFifoList - single linked list with first and last and count in head
 * DLCFifoList - double linked list with first and last and count in head
 *
 * For each XXList there are also
 * XXListImpl
 * LocalXXListImpl
 * LocalXXList which as XXList uses ArrayPool<T> as pool type
 * XXHead - XXList::Head
 *
 * The LocalXX classes are used in local scope to tell the compiler
 * that all changes to the head will be through that locally defined
 * object (there will be no aliases to head data).  And so the compiler
 * can possible optimize the generated code further.
 * But be sure to ensure that the head is not accessed but through the
 * local object!
 *
 * Recommended use is to define list type alias:
 *   typedef LocalXXListImpl<PoolClass, NodeClass> YourList;
 * and declare the head as:
 *   YourList::Head head; or
 *   YourList::Head::POD head;
 * and in local scope declare list as:
 *   YourList list(pool, head);
 *
 * For all variants of lists the following methods is available:
 *   void addFirst(Ptr<T> p);
 *   bool first(Ptr<T>& p) const;
 *   bool hasNext(Ptr<T> p) const;
 *   void insertAfter(Ptr<T> p, Ptr<T> loc);
 *   bool isEmpty() const;
 *   bool next(Ptr<T>& p) const;
 *   bool removeFirst(Ptr<T>& p);
 * and pool using methods
 *   Pool& getPool() const;
 *   void getPtr(Ptr<T>& p) const;
 *   void getPtr(Ptr<T>& p, Uint32 i) const;
 *   T* getPtr(Uint32 i) const;
 *   bool releaseFirst();
 *   bool seizeFirst(Ptr<T>& p);
 *
 * These methods needs a prev link in node
 *   void insertBefore(Ptr<T> p, Ptr<T> loc);
 *   void remove(Ptr<T> p);
 *   void remove(T* p);
 *   bool hasPrev(Ptr<T> p) const;
 *   bool prev(Ptr<T>& p) const;
 *
 * These methods needs a last link in head
 *   void addLast(Ptr<T> p);
 *   bool last(Ptr<T>& p) const;
 *   bool seizeLast(Ptr<T>& p);
 * and the for the concatenating methods the OtherHead must
 * have the same or more features as list head have
 *   template<class OtherHead>  void prependList(OtherHead& other);
 *   template<class OtherHead>  void appendList(OtherHead& other);
 *
 * These methods needs both prev link in node and last link in head
 *   bool removeLast(Ptr<T>& p);
 *   bool releaseLast();
 *   void release(Uint32 i);
 *   void release(Ptr<T> p);
 *
 * These methods needs a counter in head
 *   Uint32 count() const;
 **/

#include <ndb_limits.h>
#include <Pool.hpp>
#include "ArrayPool.hpp"

#define JAM_FILE_ID 298


template<class FirstLink, class LastLink, class Count> class ListHeadPOD
: public FirstLink, public LastLink, public Count
{
public:
  void init()
  {
    FirstLink::setFirst(RNIL);
    LastLink::setLast(RNIL);
    Count::setCount(0);
#if defined VM_TRACE || defined ERROR_INSERT
    in_use = false;
#endif
  }
  bool isEmpty() const
  {
    bool empty = FirstLink::getFirst() == RNIL;
#ifdef VM_TRACE
    Count::checkCount(empty);
#endif
    return empty;
  }
#if defined VM_TRACE || defined ERROR_INSERT
  bool in_use;
#endif
private:
};

template<class FirstLink, class LastLink, class Count> class ListHead
: public ListHeadPOD<FirstLink, LastLink, Count>
{
public:
  typedef ListHeadPOD<FirstLink, LastLink, Count> POD;
  ListHead() { POD::init(); }
  ~ListHead() { }
  ListHead(const ListHead& src): POD(src) { }
  ListHead(const typename ListHead::POD& src): POD(src) { }
private:
};

class FirstLink
{
public:
  Uint32 getFirst() const { return m_first; }
  void setFirst(Uint32 first) { m_first = first; }
  template<class H> void copyFirst(H& h) { setFirst(h.getFirst()); }
private:
  Uint32 m_first;
};

class LastLink
{
public:
  Uint32 getLast() const { return m_last; }
  void setLast(Uint32 last) { m_last = last; }
  template<class H> void copyLast(H& h) { setLast(h.getLast()); }
private:
  Uint32 m_last;
};

class NoLastLink
{
public:
  void setLast(Uint32 /* last */) { }
  template<class H> void copyLast(H& /* h */) { }
private:
};

class Count
{
public:
  Uint32 getCount() const { return m_count; }
  void setCount(Uint32 count) { m_count = count; }
  void incrCount() { m_count ++ ; }
  void decrCount() { assert(m_count > 0); m_count -- ; }
  template<class H> void transferCount(H& h) { m_count += h.getCount(); h.setCount(0); }
#ifdef VM_TRACE
  void checkCount(bool empty) const { if (empty) assert(getCount() == 0); else assert(getCount() > 0); }
#endif
private:
  Uint32 m_count;
};

class NoCount
{
public:
  void setCount(Uint32 /* count */) { }
  void incrCount() { }
  void decrCount() { }
  template<class H> void transferCount(H& h) { h.setCount(0); }
#ifdef VM_TRACE
  void checkCount(bool /* empty */) const { }
#endif
private:
};

template <typename T, typename U = T> struct DefaultSingleLinkMethods
{
static bool hasNext(U& t) { return getNext(t) != RNIL; }
static Uint32 getNext(U& t) { return t.nextList; }
static void setNext(U& t, Uint32 v) { t.nextList = v; }
template<class T2> static void copyNext(T& t, T2& t2) { setNext(t, getNext(t2)); }
static void setPrev(U& /* t */, Uint32 /* v */) { }
template<class T2> static void copyPrev(T& /* t */, T2& /* t2 */) { }
};

template <typename T, typename U = T> struct DefaultDoubleLinkMethods
{
static bool hasNext(U& t) { return getNext(t) != RNIL; }
static Uint32 getNext(U& t) { return t.nextList; }
static void setNext(U& t, Uint32 v) { t.nextList = v; }
template<class T2> static void copyNext(T& t, T2& t2) { setNext(t, getNext(t2)); }
static bool hasPrev(U& t) { return getPrev(t) != RNIL; }
static Uint32 getPrev(U& t) { return t.prevList; }
static void setPrev(U& t, Uint32 v) { t.prevList = v; }
template<class T2> static void copyPrev(T& t, T2& t2) { setPrev(t, getPrev(t2)); }
};

template<typename T, class Pool, typename THead, class LM = DefaultDoubleLinkMethods<T> > class IntrusiveList
{
public:
typedef THead Head;
typedef typename THead::POD HeadPOD;
class Local;
public:
  explicit IntrusiveList(Pool& pool, typename THead::POD head): m_pool(pool), m_head(head) { }
  explicit IntrusiveList(Pool& pool): m_pool(pool) { m_head.init(); }
  ~IntrusiveList() { }
private:
  IntrusiveList&  operator=(const IntrusiveList& src) {
    assert(&this->m_pool == &src.m_pool);
    this->m_head = src.m_head;
    return *this;
  }
private:
  IntrusiveList(const IntrusiveList&); // Not to be implemented
public:
  void addFirst(Ptr<T> p);
  void addLast(Ptr<T> p);
  void insertBefore(Ptr<T> p, Ptr<T> loc);
  void insertAfter(Ptr<T> p, Ptr<T> loc);
  bool removeFirst(Ptr<T>& p);
  bool removeLast(Ptr<T>& p);
  void remove(Ptr<T> p);
  void remove(T* p);
  bool hasNext(Ptr<T> p) const;
  bool next(Ptr<T>& p) const;
  bool hasPrev(Ptr<T> p) const;
  bool prev(Ptr<T>& p) const;
  template<class OtherList>  void prependList(OtherList& other);
  template<class OtherList>  void appendList(OtherList& other);
  bool isEmpty() const;
  Uint32 count() const;
  bool first(Ptr<T>& p) const;
  bool last(Ptr<T>& p) const;
public:
  Pool& getPool() const;
  void getPtr(Ptr<T>& p) const { if (p.i == RNIL) p.p = NULL ; else m_pool.getPtr(p); }
  void getPtr(Ptr<T>& p, Uint32 i) const { p.i=i; getPtr(p); }
  T* getPtr(Uint32 i) const { Ptr<T> p; p.i = i; getPtr(p); return p.p; }
  bool seizeFirst(Ptr<T>& p);
  bool seizeLast(Ptr<T>& p);
  bool releaseFirst();
  bool releaseLast();
  void release(Uint32 i);
  void release(Ptr<T> p);
protected:
  Pool& m_pool;
  THead m_head;
};

template<typename T, class Pool, typename THead, class LM>
class IntrusiveList<T, Pool, THead, LM>::Local: public IntrusiveList<T, Pool, THead, LM>
{
public:
  explicit Local(Pool& pool, typename THead::POD& head)
  : IntrusiveList<T, Pool, THead, LM>(pool, head), m_src(head) {
#if defined VM_TRACE || defined ERROR_INSERT
    assert(!m_src.in_use);
    m_src.in_use = true;
#endif
  }
  ~Local() {
#if defined VM_TRACE || defined ERROR_INSERT
    assert(m_src.in_use);
#endif
    m_src = this->m_head;
#if defined VM_TRACE || defined ERROR_INSERT
    assert(!m_src.in_use);
#endif
  }
private:
  Local(const Local&); // Not to be implemented
  Local&  operator=(const Local&); // Not to be implemented
private:
  typename THead::POD& m_src;
};

/* Specialisations */

#define INTRUSIVE_LIST_COMPAT(prefix, links) \
template <typename P, typename T, typename U = T, typename LM = Default##links##LinkMethods<T, U> > \
class prefix##ListImpl : public IntrusiveList<T, P, prefix##Head, LM> { \
public: prefix##ListImpl(P& pool): IntrusiveList<T, P, prefix##Head, LM>(pool) { } \
}; \
 \
template <typename P, typename T, typename U = T, typename LM = Default##links##LinkMethods<T, U> > \
class Local##prefix##ListImpl : public IntrusiveList<T, P, prefix##Head, LM>::Local { \
public: Local##prefix##ListImpl(P& pool, prefix##Head::POD& head): IntrusiveList<T, P, prefix##Head, LM>::Local(pool, head) { } \
}; \
 \
template <typename T, typename U = T, typename LM = Default##links##LinkMethods<T, U> > \
class prefix##List : public IntrusiveList<T, ArrayPool<T>, prefix##Head, LM> { \
public: prefix##List(ArrayPool<T>& pool): IntrusiveList<T, ArrayPool<T>, prefix##Head, LM>(pool) { } \
}; \
 \
template <typename T, typename U = T, typename LM = Default##links##LinkMethods<T, U> > \
class Local##prefix##List : public IntrusiveList<T, ArrayPool<T>, prefix##Head, LM>::Local { \
public: Local##prefix##List(ArrayPool<T>& pool, prefix##Head::POD& head): IntrusiveList<T, ArrayPool<T>, prefix##Head, LM>::Local(pool, head) { } \
}

typedef ListHead<FirstLink, NoLastLink, NoCount> SLHead;
typedef ListHead<FirstLink, NoLastLink, NoCount> DLHead;
typedef ListHead<FirstLink, NoLastLink, Count> SLCHead;
typedef ListHead<FirstLink, NoLastLink, Count> DLCHead;
typedef ListHead<FirstLink, LastLink, NoCount> SLFifoHead;
typedef ListHead<FirstLink, LastLink, NoCount> DLFifoHead;
typedef ListHead<FirstLink, LastLink, Count> SLCFifoHead;
typedef ListHead<FirstLink, LastLink, Count> DLCFifoHead;

INTRUSIVE_LIST_COMPAT(SL, Single);
INTRUSIVE_LIST_COMPAT(DL, Double);
INTRUSIVE_LIST_COMPAT(SLC, Single);
INTRUSIVE_LIST_COMPAT(DLC, Double);
INTRUSIVE_LIST_COMPAT(SLFifo, Single);
INTRUSIVE_LIST_COMPAT(DLFifo, Double);
INTRUSIVE_LIST_COMPAT(SLCFifo, Single);
INTRUSIVE_LIST_COMPAT(DLCFifo, Double);

/**
 * Implementation IntrusiveList
 **/

template<typename T, class Pool, typename THead, class LM>
inline void IntrusiveList<T, Pool, THead, LM>::addFirst(Ptr<T> p)
{
  Ptr<T> firstItem;
  if (first(firstItem))
  {
    LM::setPrev(*firstItem.p, p.i);
  }
  else
  {
    m_head.setLast(p.i);
  }
  LM::setPrev(*p.p, RNIL);
  LM::setNext(*p.p, firstItem.i);
  m_head.setFirst(p.i);
  m_head.incrCount();
}

template<typename T, class Pool, typename THead, class LM>
inline void IntrusiveList<T, Pool, THead, LM>::addLast(Ptr<T> p)
{
  Ptr<T> lastItem;
  if (last(lastItem))
  {
    LM::setNext(*lastItem.p, p.i);
  }
  else
  {
    m_head.setFirst(p.i);
  }
  LM::setPrev(*p.p, lastItem.i);
  LM::setNext(*p.p, RNIL);
  m_head.setLast(p.i);
  m_head.incrCount();
}

template<typename T, class Pool, typename THead, class LM>
inline void IntrusiveList<T, Pool, THead, LM>::insertBefore(Ptr<T> p, Ptr<T> loc)
{
  assert(!loc.isNull());
  Ptr<T> prevItem = loc;
  if (prev(prevItem))
  {
    LM::setNext(*prevItem.p, p.i);
  }
  else
  {
    m_head.setFirst(p.i);
  }
  LM::setPrev(*loc.p, p.i);
  LM::setPrev(*p.p, prevItem.i);
  LM::setNext(*p.p, loc.i);
  m_head.incrCount();
}

template<typename T, class Pool, typename THead, class LM>
inline void IntrusiveList<T, Pool, THead, LM>::insertAfter(Ptr<T> p, Ptr<T> loc)
{
  assert(!loc.isNull());
  Ptr<T> nextItem = loc;
  if (next(nextItem))
  {
    LM::setPrev(*nextItem.p, p.i);
  }
  else
  {
    m_head.setLast(p.i);
  }
  LM::setNext(*loc.p, p.i);
  LM::setPrev(*p.p, loc.i);
  LM::setNext(*p.p, nextItem.i);
  m_head.incrCount();
}

template<typename T, class Pool, typename THead, class LM>
inline bool IntrusiveList<T, Pool, THead, LM>::removeFirst(Ptr<T>& p)
{
  if (!first(p))
    return false;
  Ptr<T> nextItem = p;
  if (next(nextItem))
  {
    LM::setPrev(*nextItem.p, RNIL);
  }
  else
  {
    m_head.setLast(RNIL);
  }
  LM::setNext(*p.p, RNIL);
  m_head.setFirst(nextItem.i);
  m_head.decrCount();
  return true;
}

template<typename T, class Pool, typename THead, class LM>
inline bool IntrusiveList<T, Pool, THead, LM>::removeLast(Ptr<T>& p)
{
  if (!last(p))
    return false;
  Ptr<T> prevItem = p;
  if (prev(prevItem))
  {
    LM::setNext(*prevItem.p, RNIL);
  }
  else
  {
    m_head.setFirst(RNIL);
  }
  LM::setPrev(*p.p, RNIL);
  m_head.setLast(prevItem.i);
  m_head.decrCount();
  return true;
}

template<typename T, class Pool, typename THead, class LM>
inline void IntrusiveList<T, Pool, THead, LM>::remove(Ptr<T> p)
{
  remove(p.p);
}

template<typename T, class Pool, typename THead, class LM>
inline void IntrusiveList<T, Pool, THead, LM>::remove(T* p)
{
  Ptr<T> prevItem;
  Ptr<T> nextItem;
  prevItem.p = p;
  nextItem.p = p;
  prev(prevItem);
  next(nextItem);
  if (!prevItem.isNull())
  {
    LM::setNext(*prevItem.p, nextItem.i);
  }
  else
  {
    m_head.setFirst(nextItem.i);
  }
  if (!nextItem.isNull())
  {
    LM::setPrev(*nextItem.p, prevItem.i);
  }
  else
  {
    m_head.setLast(prevItem.i);
  }
  LM::setPrev(*p, RNIL);
  LM::setNext(*p, RNIL);
  m_head.decrCount();
}

template<typename T, class Pool, typename THead, class LM>
inline bool IntrusiveList<T, Pool, THead, LM>::hasNext(Ptr<T> p) const
{
  return LM::hasNext(*p.p);
}

template<typename T, class Pool, typename THead, class LM>
inline bool IntrusiveList<T, Pool, THead, LM>::next(Ptr<T>& p) const
{
  p.i = LM::getNext(*p.p);
  if (p.i == RNIL)
    return false;
  getPtr(p);
  return true;
}

template<typename T, class Pool, typename THead, class LM>
inline bool IntrusiveList<T, Pool, THead, LM>::hasPrev(Ptr<T> p) const
{
  return LM::hasPrev(*p.p);
}

template<typename T, class Pool, typename THead, class LM>
inline bool IntrusiveList<T, Pool, THead, LM>::prev(Ptr<T>& p) const
{
  p.i = LM::getPrev(*p.p);
  if (p.i == RNIL)
    return false;
  getPtr(p);
  return true;
}

template<typename T, class Pool, typename THead, class LM>
template<class OtherHead> inline void IntrusiveList<T, Pool, THead, LM>::prependList(OtherHead& other)
{
  if (other.isEmpty())
    return;

  Ptr<T> firstItem;
  first(firstItem);

  Ptr<T> otherLastItem;
  otherLastItem.i = other.getLast();
  getPtr(otherLastItem);

  if (firstItem.i != RNIL)
  {
    LM::setPrev(*firstItem.p, otherLastItem.i);
  }
  else
  {
    m_head.copyLast(other);
  }
  LM::setNext(*otherLastItem.p, firstItem.i);
  m_head.copyFirst(other);
  m_head.transferCount(other);
  other.setFirst(RNIL);
  other.setLast(RNIL);
}

template<typename T, class Pool, typename THead, class LM>
template<class OtherHead> inline void IntrusiveList<T, Pool, THead, LM>::appendList(OtherHead& other)
{
  if (other.isEmpty())
    return;

  Ptr<T> lastItem;
  last(lastItem);

  Ptr<T> otherFirstItem;
  otherFirstItem.i = other.getFirst();
  getPtr(otherFirstItem);

  if (lastItem.i != RNIL)
  {
    LM::setNext(*lastItem.p, otherFirstItem.i);
  }
  else
  {
    m_head.copyFirst(other);
  }
  LM::setPrev(*otherFirstItem.p, lastItem.i);
  m_head.copyLast(other);
  m_head.transferCount(other);
  other.setFirst(RNIL);
  other.setLast(RNIL);
}

template<typename T, class Pool, typename THead, class LM>
inline bool IntrusiveList<T, Pool, THead, LM>::isEmpty() const
{
  return m_head.isEmpty();
}

template<typename T, class Pool, typename THead, class LM>
inline Uint32 IntrusiveList<T, Pool, THead, LM>::count() const
{
  return m_head.getCount();
}

template<typename T, class Pool, typename THead, class LM>
inline bool IntrusiveList<T, Pool, THead, LM>::first(Ptr<T>& p) const
{
  p.i = m_head.getFirst();
  getPtr(p);
  return !p.isNull();
}

template<typename T, class Pool, typename THead, class LM>
inline bool IntrusiveList<T, Pool, THead, LM>::last(Ptr<T>& p) const
{
  p.i = m_head.getLast();
  getPtr(p);
  return !p.isNull();
}

template<typename T, class Pool, typename THead, class LM>
inline Pool& IntrusiveList<T, Pool, THead, LM>::getPool() const
{
  return m_pool;
}

template<typename T, class Pool, typename THead, class LM>
inline bool IntrusiveList<T, Pool, THead, LM>::seizeFirst(Ptr<T>& p)
{
  if (!getPool().seize(p))
    return false;
  addFirst(p);
  return true;
}

template<typename T, class Pool, typename THead, class LM>
inline bool IntrusiveList<T, Pool, THead, LM>::seizeLast(Ptr<T>& p)
{
  if (!getPool().seize(p))
    return false;
  addLast(p);
  return true;
}

template<typename T, class Pool, typename THead, class LM>
inline bool IntrusiveList<T, Pool, THead, LM>::releaseFirst()
{
  Ptr<T> p;
  if (!removeFirst(p))
    return false;
  getPool().release(p);
  return true;
}

template<typename T, class Pool, typename THead, class LM>
inline bool IntrusiveList<T, Pool, THead, LM>::releaseLast()
{
  Ptr<T> p;
  if (!removeLast(p))
    return false;
  getPool().release(p);
  return true;
}

template<typename T, class Pool, typename THead, class LM>
inline void IntrusiveList<T, Pool, THead, LM>::release(Ptr<T> p)
{
  remove(p);
  getPool().release(p);
}

template<typename T, class Pool, typename THead, class LM>
inline void IntrusiveList<T, Pool, THead, LM>::release(Uint32 i)
{
  Ptr<T> p;
  getPtr(p, i);
  remove(p);
  getPool().release(p);
}


#undef JAM_FILE_ID

#endif
