/*
   Copyright (c) 2003, 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA */

#ifndef SEGMENT_LIST_HPP
#define SEGMENT_LIST_HPP

#include <ndb_types.h>
#include <LongSignal.hpp>

#define JAM_FILE_ID 496

/* Head of a List implemented using Segmented Sections */
struct SegmentListHead
{
  SegmentListHead();

  Uint32 headPtr;

  bool isEmpty() const;
};

/**
 * LocalSegmentList
 *
 * Util class for working with Segment List
 *
 * Segments from the global segment pool can be linked in a list - this
 * is heavily used for passing 'long sections' around within a data node
 * without copying.
 *
 * Segments internally have 4 words of header data, and 60 words of payload
 * The header data includes a size in words, a next ptr, a prev ptr and an
 * ownerRef
 *
 * Typically the existing 'long signal' code uses the size member of the first
 * segment in the list as the valid size in words of the whole list.
 * The next pointer is used, but the prev pointer and m_ownerRef are not.
 *
 * Sections (segment lists) are typically built up over time, and then freed
 * at one time.
 *
 * The underlying Segments support more flexible usage, e.g. as a FIFO, or
 * Stack or double-ended list.  This class aims to support that usage, in
 * a clean-ish way so that it is easy to use.
 *
 * Initially, support for using a Segment List as a FIFO is implemented -
 * words can be 'enqueued' at the tail of a 'word list' and 'dequeued' from
 * the head.  The tail is the end of a segment list (long section), and the
 * head is the start of the list...  The m_ownerRef value in the first section's
 * header is used as an offset within that section to the head of the word list.
 *
 * This can be extended over time with :
 *   enqWordsAtHead()
 *     Can be O(1) with just nextPtrs...
 *
 *   deqWordsFromTail()
 *     Requires setting of prevPtrs when building list...
 *
 * A 'normal' long section IValue can be used as the head of a list, as long
 * as it's m_ownerRef is set to 0 initially (should be the case).
 * A list can be treated as a long section, as the size value includes the valid
 * data and the offset.
 *
 * The complexities of per-thread segment caches are hidden using the SegmentUtils
 * abstraction.
 */
class LocalSegmentList
{
private:
  SegmentListHead& m_headRef;
  SegmentUtils& m_segmentUtils;

  Uint32 m_headVal;

public:
  /* This is a 'local handle' class which is used to work on
   * a SegmentList, which is normally represented with just
   * a single segment IVal.
   * Only one LocalSegmentList instance should be 'active' at a time
   * on a single queue.
   * The destructor writes the (new) queue head back into the
   * SegmentListHead object.
   */
  LocalSegmentList(SegmentListHead& headRef,
                   SegmentUtils& segmentUtils);

  ~LocalSegmentList();

  /* Enqueue len 32 bit words onto the tail of the queue from
   * the src pointer.
   */
  bool enqWords(const Uint32* src, Uint32 len);

  /* Dequeue len 32 bit words from the head of the queue to
   * the dst pointer.
   */
  bool deqWords(Uint32* dst, Uint32 len);

  /**
   * TODO
   * - bool enqWordsAtHead(const Uint32* src, Uint32 len);
   * - bool deqWordsFromTail(Uint32& dst, Uint32 len);
   *
   * Peek, trim, truncate etc...
   */

  /* Empty the queue, releasing all segments */
  void empty();

  /* Test whether the queue is empty */
  bool isEmpty() const;

  /* Get the length of the queue in 32 bit words */
  Uint32 getLen() const;

private:
  bool verify() const;
  // copy ops are private to prevent copying
  LocalSegmentList(const LocalSegmentList&); // no implementation
  LocalSegmentList& operator= (const LocalSegmentList&); // no implementation
};


/**
 * SegmentSubPool
 *
 * This is an implementation of the SegmentUtils Api which can be
 * used to create a subpool of segments, from a parent pool.
 * This can be useful for 'reserving' a certain number of segments
 * for a particular usage.
 */
class SegmentSubPool :
  public SegmentUtils /* Implements SegmentUtils Api */
{
public:
  explicit SegmentSubPool(SegmentUtils& parentPool);
  ~SegmentSubPool();

  /**
   * init
   *
   * Initialise the sub-pool, allocating the minSegments from
   * the parent Pool.
   * Separate from the constructor to allow delayed initialisation
   * of the parent pool.
   */
  bool init(Uint32 minSegments,
            Uint32 maxSegments);

  /* SegmentUtils Api */
  virtual SectionSegment* getSegmentPtr(Uint32 iVal);
  virtual void getSegmentPtr(Ptr<SectionSegment>& p, Uint32 iVal);
  virtual bool seizeSegment(Ptr<SectionSegment>& p);
  virtual void releaseSegment(Uint32 iVal);

  /* Release a section (ll of segments with size) */
  virtual void releaseSegmentList(Uint32 iVal);

  /* SegmentSubPool information : */
  /**
   * getNumOwned
   *
   * Returns number of segments owned by this pool
   * (seized from parent, and in freelist or given
   * to pool users)
   */
  Uint32 getNumOwned() const
  {
    return m_numOwned;
  }

  /**
   * getNumAvailable
   *
   * Returns number of segments available without
   * requiring seize from the parent pool
   */
  Uint32 getNumAvailable() const
  {
    return m_numAvailable;
  }

private:
  bool checkInvariants();

  SegmentUtils& m_parentPool;
  Uint32 m_minSegments;
  Uint32 m_maxSegments;
  Uint32 m_numOwned;
  Uint32 m_numAvailable;
  Uint32 m_firstFree;

  // copy ops are private to prevent copying
  SegmentSubPool(SegmentSubPool&); // no implementation
  SegmentSubPool & operator= (const SegmentSubPool&); // no implementation
};


#undef JAM_FILE_ID

#endif
