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

#include <SegmentList.hpp>
#include <random.h>

#define JAM_FILE_ID 497

/**
 * sectionAppend
 *
 * Appends all supplied words to section starting with supplied segment
 * IVal, or none at all.
 * Uses the passed SegmentUtils object to seize and release segments.
 *
 * Based on LongSignal.cpp::appendToSection, but using SegmentUtils
 * rather than macro-ed SegmentPool cache args...
 * Easier to encapsulate + use from utilities etc..
 */
bool sectionAppend(SegmentUtils& su, Uint32& firstSegmentIVal, const Uint32* src, Uint32 len)
{
  Ptr<SectionSegment> firstPtr, currPtr;

  if (len == 0)
    return true;

  Uint32 segRemain= SectionSegment::DataLength;
  Uint32 segmentOffset= 0;

  if (firstSegmentIVal == RNIL)
  {
    /* First data to be added to this section */
    bool result= su.seizeSegment(firstPtr);

    if (!result)
      return false;

    firstPtr.p->m_sz= 0;
    firstPtr.p->m_ownerRef= 0;

    currPtr= firstPtr;
  }
  else
  {
    /* Section has at least one segment with data already */
    su.getSegmentPtr(firstPtr, firstSegmentIVal);
    su.getSegmentPtr(currPtr, firstPtr.p->m_lastSegment);

    Uint32 existingLen= firstPtr.p->m_sz;
    assert(existingLen > 0);
    segmentOffset= existingLen % SectionSegment::DataLength;

    /* If existingLen %  SectionSegment::DataLength == 0
     * we assume that the last segment is full
     */
    segmentOffset= segmentOffset == 0 ?
      SectionSegment::DataLength :
      segmentOffset;

    segRemain= SectionSegment::DataLength - segmentOffset;
  }

  Uint32 totalRemain = len;

  while(totalRemain > segRemain) {
    /**
     * Fill this segment, and link in another one
     */
    memcpy(&currPtr.p->theData[segmentOffset], src, segRemain * sizeof(Uint32));
    src += segRemain;
    totalRemain -= segRemain;
    Ptr<SectionSegment> prevPtr= currPtr;

    bool result = su.seizeSegment(currPtr);
    if (!result)
    {
      /**
       * Failed, release any segments allocated so far...
       * The data written to the previous 'last' segment will be ignored.
       */
      Uint32 origLen = firstPtr.p->m_sz;
      Uint32 origOffset = origLen % SectionSegment::DataLength;
      origOffset = (origOffset == 0 ? SectionSegment::DataLength : origOffset);
      Uint32 wordsFitInOrigLastSeg = SectionSegment::DataLength - origOffset;
      Uint32 extraLen = (len - totalRemain);
      if (extraLen > wordsFitInOrigLastSeg)
      {
        /* Have to release some segments.. */
        Uint32 releaseIVal = RNIL;
        if (origLen > 0)
        {
          SectionSegment* lastOrigSeg = su.getSegmentPtr(firstPtr.p->m_lastSegment);
          assert(lastOrigSeg->m_nextSegment != RNIL);
          Uint32 extraSegLen = extraLen - wordsFitInOrigLastSeg;
          assert(extraSegLen > 0);
          releaseIVal = lastOrigSeg->m_nextSegment;
          SectionSegment* firstExtraSeg = su.getSegmentPtr(lastOrigSeg->m_nextSegment);
          lastOrigSeg->m_nextSegment = RNIL;
          firstExtraSeg->m_sz = extraSegLen;
          firstExtraSeg->m_lastSegment = prevPtr.i;
        }
        else
        {
          /* First segment was the first extra segment */
          releaseIVal = firstPtr.i;
          SectionSegment* firstExtraSeg = firstPtr.p;
          firstExtraSeg->m_sz = extraLen;
          firstExtraSeg->m_lastSegment = prevPtr.i;
        }

        /* Release the extra segments we allocated... */
        su.releaseSegmentList(releaseIVal);
      }
      return false;
    }
    prevPtr.p->m_nextSegment = currPtr.i;
    currPtr.p->m_sz= 0;
    currPtr.p->m_ownerRef= 0;

    segmentOffset= 0;
    segRemain= SectionSegment::DataLength;
  }

  /* Data fits in the current last segment */
  currPtr.p->m_nextSegment= RNIL;
  memcpy(&currPtr.p->theData[segmentOffset], src, totalRemain * sizeof(Uint32));

  /* Success - update first segment to reflect new size */
  firstPtr.p->m_sz+= len;
  firstPtr.p->m_lastSegment= currPtr.i;

  firstSegmentIVal= firstPtr.i;

  return true;
}

/**
 * sectionConsume
 *
 * Consumes the requested number of words, or none at all from the
 * *front* of the long section described by the passed first-segment IVal.
 *
 * The first segment's m_ownerRef member is used to store the offset within
 * the first segment of the first valid word.
 * Segments are released from the front of the long section when all of their
 * words have been consumed.
 * The m_sz parameter in the first segment describes the length of the
 * valid data in the section *including* the offset.
 */
bool sectionConsume(SegmentUtils& su, Uint32& firstSegmentIVal, Uint32* dst, Uint32 len)
{
  if (firstSegmentIVal != RNIL)
  {
    SectionSegment* segment= su.getSegmentPtr(firstSegmentIVal);
    Uint32 sz = segment->m_sz;
    Uint32 offset = segment->m_ownerRef;
    Uint32 queueLen = sz - offset;
    assert(offset <= sz);

    if (len > queueLen)
    {
      /* Insufficient words */
      return false;
    }

    while (len > 0)
    {
      assert(segment != NULL);
      assert(offset < SectionSegment::DataLength);

      Uint32 segmentRemain = SectionSegment::DataLength - offset;
      Uint32 readLen = MIN(segmentRemain, len);
      assert(readLen <= segment->m_sz);

      memcpy(dst, segment->theData + offset, readLen * sizeof(Uint32));
      dst+= readLen;
      offset += readLen;
      len -= readLen;

      /**
       * If we've emptied the segment then release it and update
       * the structure
       */
      if ((offset == segment->m_sz) ||
          (offset == SectionSegment::DataLength))
      {
        Uint32 nextSegmentIVal = segment->m_nextSegment;
        Uint32 oldSegmentIVal = firstSegmentIVal;
        SectionSegment* oldSegment = segment;

        firstSegmentIVal = nextSegmentIVal;

        /* End of segment, free it and fixup next... */
        if (nextSegmentIVal != RNIL)
        {
          /* Move to next segment */
          segment = su.getSegmentPtr(nextSegmentIVal);
          segment->m_sz = oldSegment->m_sz - SectionSegment::DataLength;
          offset = 0;
          segment->m_lastSegment = oldSegment->m_lastSegment;
        }
        else
        {
          segment = NULL;
        }

        /* Release oldSegment */
        oldSegment->m_sz = 1;
        oldSegment->m_lastSegment = oldSegmentIVal;
        oldSegment->m_nextSegment = RNIL;
        su.releaseSegment(oldSegmentIVal);
      }
    }

    if (segment != NULL)
    {
      /* Update offset */
      segment->m_ownerRef = offset;
    }

    return true;
  }
  return false;
}


#define SVASSERT(x) if (!(x)) {assert(false); return false;}

/**
 * sectionVerify
 * Assertion method to check that a segmented section is constructed
 * 'properly' where 'properly' is loosely defined.
 */
bool
sectionVerify(SegmentUtils& su, Uint32 firstIVal)
{
  if (firstIVal == RNIL)
    return true;

  /* Get first section ptr (With assertions in getPtr) */
  SectionSegment* first= su.getSegmentPtr(firstIVal);

  SVASSERT(first != NULL);

  Uint32 totalSize= first->m_sz;
  Uint32 lastSegIVal= first->m_lastSegment;

  /**
   * Hmm, need to be careful of length == 0
   * Nature abhors a segmented section with length 0
   */
  SVASSERT(totalSize != 0);
  SVASSERT(lastSegIVal != RNIL); /* Should never be == RNIL */
  /* We ignore m_ownerRef */

  if (totalSize <= SectionSegment::DataLength)
  {
    /* 1 segment */
    SVASSERT(first->m_lastSegment == firstIVal);
    // m_nextSegment not always set to RNIL on last segment
    SVASSERT(first->m_nextSegment == RNIL);
  }
  else
  {
    /* > 1 segment */
    SVASSERT(first->m_nextSegment != RNIL);
    SVASSERT(first->m_lastSegment != firstIVal);
    Uint32 currIVal= firstIVal;
    SectionSegment* curr= first;

    /* Traverse segments to where we think the end should be */
    while (totalSize > SectionSegment::DataLength)
    {
      currIVal= curr->m_nextSegment;
      curr= su.getSegmentPtr(currIVal);
      totalSize-= SectionSegment::DataLength;
      /* Ignore m_ownerRef, m_sz, m_lastSegment of intermediate
       * Segments
       */
    }

    /* Once we are here, we are on the last Segment of this Section
     * Check that last segment is as stated in the first segment
     */
    SVASSERT(currIVal == lastSegIVal);
    SVASSERT(curr->m_nextSegment == RNIL);
    /* Ignore m_ownerRef, m_sz, m_lastSegment of last segment */
  }

  return true;
}


SegmentListHead::SegmentListHead():headPtr(RNIL) {};

bool
SegmentListHead::isEmpty() const
{
  return (headPtr == RNIL);
}


LocalSegmentList::LocalSegmentList(SegmentListHead& headRef,
                                   SegmentUtils& segmentUtils)
  :m_headRef(headRef), m_segmentUtils(segmentUtils)
{
  m_headVal = m_headRef.headPtr;
  assert(verify());
}



LocalSegmentList::~LocalSegmentList()
{
  assert(verify());
  m_headRef.headPtr = m_headVal;
}

bool
LocalSegmentList::enqWords(const Uint32* src, Uint32 len)
{
  assert(verify());
  /* Append words on the end of the section */
#ifdef VM_TRACE
  Uint32 offset = 0;
  if (m_headVal != RNIL)
  {
    SectionSegment* firstSeg= m_segmentUtils.getSegmentPtr(m_headVal);
    offset = firstSeg->m_ownerRef;
  }
#endif

  bool res = sectionAppend(m_segmentUtils, m_headVal, src, len);

#ifdef VM_TRACE
  if (res)
  {
    SectionSegment* firstSeg= m_segmentUtils.getSegmentPtr(m_headVal);
    /* Check offset / m_ownerRef not trampled above */
    assert(firstSeg->m_ownerRef == offset);
  }
#endif

  assert(verify());
  return res;
}

bool
LocalSegmentList::deqWords(Uint32* dst, Uint32 len)
{
  assert(verify());
  bool res = sectionConsume(m_segmentUtils, m_headVal, dst, len);
  assert(verify());
  return res;
}

void
LocalSegmentList::empty()
{
  if (m_headVal != RNIL)
  {
    m_segmentUtils.releaseSegmentList(m_headVal);
    m_headVal = RNIL;
  }
  assert(verify());
}

bool
LocalSegmentList::isEmpty() const
{
  return m_headVal == RNIL;
}

Uint32
LocalSegmentList::getLen() const
{
  if (m_headVal != RNIL)
  {
    SectionSegment* firstSeg= m_segmentUtils.getSegmentPtr(m_headVal);
    Uint32 sz = firstSeg->m_sz;
    Uint32 offset = firstSeg->m_ownerRef;
    return sz - offset;
  }
  return 0;
}

bool
LocalSegmentList::verify() const
{
  /**
   * First check that the list is a valid 'long section',
   * then check that it is correct as a list
   */
  if (m_headVal != RNIL)
  {
    assert(sectionVerify(m_segmentUtils, m_headVal));

    /* Above has checked that the length correlates
     * with the number of linked sections etc...
     * We can just check the offset...
     */
    SectionSegment* firstSeg = m_segmentUtils.getSegmentPtr(m_headVal);
    Uint32 sz = firstSeg->m_sz;
    Uint32 offset = firstSeg->m_ownerRef;
    assert(sz >= offset);
    assert(offset < SectionSegment::DataLength);
    /* Suppress warnings in release */
    (void) sz;
    (void) offset;
  }

  return true;
}

SegmentSubPool::SegmentSubPool(SegmentUtils& parentPool):
  m_parentPool(parentPool),
  m_minSegments(0),
  m_maxSegments(0),
  m_numOwned(0),
  m_numAvailable(0),
  m_firstFree(RNIL)
{
  assert(checkInvariants());
}

SegmentSubPool::~SegmentSubPool()
{
  /* Check that all segments were returned to us */
  assert(m_numOwned == m_numAvailable);

  Ptr<SectionSegment> p;

  while (m_firstFree != RNIL)
  {
    m_parentPool.getSegmentPtr(p, m_firstFree);
    m_firstFree = p.p->m_nextSegment;
    m_parentPool.releaseSegment(p.i);
    m_numOwned--;
  }

  assert(m_numOwned == 0);
}

bool
SegmentSubPool::init(Uint32 minSegments,
                     Uint32 maxSegments)
{
  if (minSegments > maxSegments)
    return false;

  if (maxSegments == 0)
    return false;

  m_minSegments = minSegments;
  m_maxSegments = maxSegments;

  /* Take minimal allocation from parent pool */
  for (Uint32 i=0; i<m_minSegments; i++)
  {
    Ptr<SectionSegment> p;

    if (!m_parentPool.seizeSegment(p))
    {
      abort();
    }

    p.p->m_nextSegment = m_firstFree;
    m_firstFree = p.i;
  }

  m_numOwned = m_numAvailable = m_minSegments;

  assert(checkInvariants());
  return true;
}

SectionSegment*
SegmentSubPool::getSegmentPtr(Uint32 iVal)
{
  return m_parentPool.getSegmentPtr(iVal);
}

void
SegmentSubPool::getSegmentPtr(Ptr<SectionSegment>& p, Uint32 iVal)
{
  m_parentPool.getSegmentPtr(p, iVal);
}

bool
SegmentSubPool::seizeSegment(Ptr<SectionSegment>& p)
{
  assert(checkInvariants());

  if (m_firstFree != RNIL)
  {
    assert(m_numAvailable > 0);
    getSegmentPtr(p, m_firstFree);
    m_firstFree = p.p->m_nextSegment;
    p.p->m_nextSegment = RNIL;
    m_numAvailable--;
    assert(checkInvariants());
    return true;
  }
  assert(m_numAvailable == 0);
  if (m_numOwned < m_maxSegments)
  {
    bool result = m_parentPool.seizeSegment(p);
    if (result)
    {
      m_numOwned++;
      assert(checkInvariants());
      return true;
    }
  }

  /* Max reached, or parent couldn't seize */
  assert(checkInvariants());
  return false;
}

void
SegmentSubPool::releaseSegment(Uint32 iVal)
{
  assert(m_numAvailable < m_numOwned);
  assert(checkInvariants());
  if (m_numOwned > m_minSegments)
  {
    /* Don't want to sub-pool this, return to parent */
    m_parentPool.releaseSegment(iVal);
    m_numOwned--;
  }
  else
  {
    /* Keep on our free list */
    SectionSegment* seg = m_parentPool.getSegmentPtr(iVal);
    seg->m_nextSegment = m_firstFree;
    m_firstFree = iVal;
    m_numAvailable++;
  }
  assert(checkInvariants());
}

void
SegmentSubPool::releaseSegmentList(Uint32 iVal)
{
  assert(checkInvariants());
  /* Todo : optimise */
  while (iVal != RNIL)
  {
    Ptr<SectionSegment> p;
    m_parentPool.getSegmentPtr(p, iVal);
    iVal = p.p->m_nextSegment;
    releaseSegment(p.i);
  }
  assert(checkInvariants());
}


bool
SegmentSubPool::checkInvariants()
{
  SVASSERT(m_numOwned <= m_maxSegments);
  SVASSERT(m_numOwned >= m_minSegments);
  SVASSERT(m_numAvailable <= m_numOwned);
  SVASSERT(m_firstFree != RNIL ||
           m_numAvailable == 0);
  /**
   * Paranoia for a rainy day
   * Could check length of m_firstFree with iteration against
   * m_numAvailable
   */
  return true;
}

#ifdef TAP_TEST

#undef JAM_FILE_ID

#include <NdbTap.hpp>

#define JAM_FILE_ID 494

/* Redefine ArrayPool dependencies to enable standalone Unit-test compile */
void ErrorReporter::handleAssert(const char* message, const char* file, int line, int ec)
{
  printf("Error :\"%s\" at file : %s line %u ec %u\n",
         message, file, line, ec);
  abort();
}

SectionSegmentPool g_sectionSegmentPool;

#define VERIFY(x) if ((x) == 0) { printf("VERIFY failed at Line %u : %s\n",__LINE__, #x);  abort(); return false; }

#define relSz(x) ((x == 0)? 1 : ((x + SectionSegment::DataLength - 1) / SectionSegment::DataLength))

/* Static function */
void
SectionSegmentPool::handleOutOfSegments(ArrayPool<SectionSegment>& pool)
{
  printf("SectionSegmentPool::handleOutOfSegments called");
}

class TestSegmentUtils : public SegmentUtils
{
public:
  TestSegmentUtils() {};

  SectionSegment*
  getSegmentPtr(Uint32 iVal)
  {
    return g_sectionSegmentPool.getPtr(iVal);
  }

  void getSegmentPtr(Ptr<SectionSegment>& ptr, Uint32 iVal)
  {
    g_sectionSegmentPool.getPtr(ptr, iVal);
  }

  bool
  seizeSegment(Ptr<SectionSegment>& p)
  {
    return g_sectionSegmentPool.seize(p);
  };

  void
  releaseSegment(Uint32 iVal)
  {
    return g_sectionSegmentPool.release(iVal);
  };

  void
  releaseSegmentList(Uint32 firstSegmentIVal)
  {
    if (firstSegmentIVal != RNIL)
    {
      SectionSegment* p = g_sectionSegmentPool.getPtr(firstSegmentIVal);

      g_sectionSegmentPool.releaseList(relSz(p->m_sz),
                                       firstSegmentIVal,
                                       p->m_lastSegment);
    }
  }
};


struct TestVariant
{
  bool useSubPool;
  Uint32 minAlloc;
  Uint32 maxAlloc;
};

static const Uint32 NUM_SEGMENTS = 1024;

static const TestVariant testVariants[] =
{
  {false, 0,                1},
  {true,  0,                ~Uint32(0)},
  {true,  10,               ~Uint32(0)},
  {true,  NUM_SEGMENTS,     NUM_SEGMENTS}
};


static Uint32 getActualUsed(SegmentSubPool& ssp)
{
  return g_sectionSegmentPool.getUsed() - ssp.getNumAvailable();
};

bool testBasicFillAndDrain()
{
  TestSegmentUtils tsu;
  SegmentListHead slh;
  SegmentUtils* su = &tsu;

  printf("testBasicFillAndDrain()\n");

  const Uint32 NumVariants = sizeof(testVariants)/sizeof(TestVariant);

  for (Uint32 i = 0; i < NumVariants; i++)
  {
    printf("Variant %u\n", i);
    printf("SectionPool used : %u \n", g_sectionSegmentPool.getUsed());

    SegmentSubPool ssp(tsu);
    VERIFY(ssp.init(testVariants[i].minAlloc,
                    testVariants[i].maxAlloc));
    if (testVariants[i].useSubPool)
    {
      printf("Using subpool with min=%u and max=%u\n",
             testVariants[i].minAlloc,
             testVariants[i].maxAlloc);
      su = &ssp;
    }
    printf("SectionPool used : %u \n", g_sectionSegmentPool.getUsed());
    printf("SubPool used : %u \n", ssp.getNumOwned() - ssp.getNumAvailable());
    printf("Actual used : %u \n", getActualUsed(ssp));

    /* Test SegmentList */
    for (Uint32 t=0; t < 100; t++)
    {
      LocalSegmentList lsl(slh, *su);

      printf("Enqueueing...\n");
      const Uint32 totalLen = 10000;
      Uint32 e = 0;

      while (e < totalLen)
      {
        Uint32 enqSize = MIN((totalLen - e), (e % 129) + 1);
        Uint32 buff[130];
        for (Uint32 f=0; f < enqSize; f++)
        {
          buff[f] = e + f;
        }

        VERIFY(lsl.enqWords(buff, enqSize));
        e+= enqSize;
      }

      VERIFY(!lsl.isEmpty());
      VERIFY(lsl.getLen() == 10000);
      printf("SectionPool used : %u \n", g_sectionSegmentPool.getUsed());
      printf("SubPool owned : %u \n", ssp.getNumOwned());
      printf("SubPool available : %u \n", ssp.getNumAvailable());
      printf("Actual used : %u \n", getActualUsed(ssp));

      printf("\nDequeueing...\n");
      Uint32 elementCount = 0;

      while(!lsl.isEmpty())
      {
        Uint32 deqSize = MIN((elementCount % 128) + 1, lsl.getLen());
        Uint32 buff[130];
        VERIFY(lsl.deqWords(buff, deqSize));

        for (Uint32 c=0; c < deqSize; c++)
        {
          VERIFY(buff[c] == elementCount + c);
        }

        elementCount+= deqSize;
      }
      VERIFY(elementCount == 10000);

      printf("SectionPool used : %u \n", g_sectionSegmentPool.getUsed());
      printf("SubPool owned : %u \n", ssp.getNumOwned());
      printf("SubPool available : %u \n", ssp.getNumAvailable());
      printf("Actual used : %u \n", getActualUsed(ssp));

      VERIFY(lsl.isEmpty());
      VERIFY(lsl.getLen() == 0);
      VERIFY(getActualUsed(ssp) == 0);
    }
  }

  VERIFY(slh.headPtr == RNIL);

  return true;
};

bool testMixedEnqAndDeq()
{
  printf("testMixedEnqAndDeq()\n");

  TestSegmentUtils tsu;
  SegmentListHead slh;
  SegmentUtils* su = &tsu;

  const Uint32 NumVariants = sizeof(testVariants)/sizeof(TestVariant);
  const Uint32 maxLen = 10000;
  for (Uint32 i = 0; i < NumVariants; i++)
  {
    printf("Variant %u\n", i);
    printf("SectionPool used : %u \n", g_sectionSegmentPool.getUsed());

    SegmentSubPool ssp(tsu);
    VERIFY(ssp.init(testVariants[i].minAlloc,
                    testVariants[i].maxAlloc));
    if (testVariants[i].useSubPool)
    {
      printf("Using subpool with min=%u and max=%u\n",
             testVariants[i].minAlloc,
             testVariants[i].maxAlloc);
      su = &ssp;
    }
    printf("SectionPool used : %u \n", g_sectionSegmentPool.getUsed());
    printf("SubPool used : %u \n", ssp.getNumOwned() - ssp.getNumAvailable());
    printf("Actual used : %u \n", getActualUsed(ssp));

    Uint32 headVal = 0;
    Uint32 tailVal = 0;

    for (Uint32 j = 0; j < 4000; j++)
    {
      LocalSegmentList lsl(slh, *su);

      Uint32 len = lsl.getLen();
      Uint32 remain = maxLen - len;

      printf("Queue length is %u\n", len);
      printf("Actual used : %u \n", getActualUsed(ssp));
      Uint32 bestCaseUsed = len / SectionSegment::DataLength;
      Uint32 worstCaseUsed = 1 + ((len + (SectionSegment::DataLength - 1)) / SectionSegment::DataLength);

      VERIFY(getActualUsed(ssp) >= bestCaseUsed);
      VERIFY(getActualUsed(ssp) <= worstCaseUsed);

      if (remain)
      {
        Uint32 rp = myRandom48(remain) + 1;
        Uint32 enqSize = MIN(rp, 217);
        Uint32 buff[217];
        for (Uint32 k = 0; k < enqSize; k++)
        {
          buff[k] = headVal + k;
        }
        VERIFY(lsl.enqWords(buff, enqSize));
        VERIFY(lsl.getLen() == len + enqSize);
        len = lsl.getLen();
        headVal+= enqSize;
        printf("Queue length is %u\n", len);
        printf("Actual used : %u \n", getActualUsed(ssp));
      }

      if (len)
      {
        Uint32 r = myRandom48(len) + 1;
        Uint32 deqSize = MIN(r, 217);
        Uint32 buff[217];
        VERIFY(lsl.deqWords(buff, deqSize));
        VERIFY(lsl.getLen() == len - deqSize);

        for (Uint32 k = 0; k < deqSize; k++)
        {
          VERIFY(buff[k] == tailVal + k);
        }
        tailVal += deqSize;
      }

      if (myRandom48(20) == 1)
      {
        printf("Emptying queue of len %u\n", lsl.getLen());

        lsl.empty();

        tailVal = headVal;

        VERIFY(lsl.getLen() == 0);
        VERIFY(getActualUsed(ssp) == 0);
      }
    }

    {
      LocalSegmentList lsl(slh, *su);

      while (lsl.getLen() > 0)
      {
        Uint32 space;
        lsl.deqWords(&space, 1);
      }
    }
  }

  VERIFY(g_sectionSegmentPool.getUsed() == 0);

  return true;
}

static bool checkSegFootprint(Uint32 headVal,
                              Uint32 len,
                              Uint32 segsUsed)
{
  Uint32 offset = headVal % SectionSegment::DataLength;
  Uint32 realLen = offset + len;

  Uint32 expectedSegs = (realLen + (SectionSegment::DataLength - 1))/
    SectionSegment::DataLength;

  return (expectedSegs == segsUsed);
}

bool testSubPoolLimit()
{
  printf("testSubPoolLimit()\n");

  TestSegmentUtils tsu;
  SegmentListHead slh;

  for (Uint32 i=0; i < 10; i++)
  {
    const Uint32 maxSegs = myRandom48(1020) + 1;
    const Uint32 maxWords = maxSegs * SectionSegment::DataLength;

    VERIFY(g_sectionSegmentPool.getUsed() == 0);

    SegmentSubPool ssp(tsu);
    VERIFY(ssp.init(maxSegs, maxSegs));
    LocalSegmentList lsl(slh, ssp);

    printf("Iteration %u maxSegs %u maxWords %u\n",
           i, maxSegs, maxWords);

    VERIFY(lsl.getLen() == 0);
    Uint32 buff[250];
    Uint32 headVal = 0;
    Uint32 tailVal = 0;

    for (int j=0; j < 10; j++)
    {
      /* Fill up to beyond the limit */
      while (true)
      {
        Uint32 len = lsl.getLen();

        /* Check that our segment footprint is ok */
        checkSegFootprint(headVal, len, getActualUsed(ssp));
        Uint32 extra = myRandom48(249) + 1;

        for (Uint32 k=0; k < extra; k++)
        {
          buff[k] = tailVal + k;
        }

        bool result = lsl.enqWords(buff, extra);

        if (result)
        {
          VERIFY((len + extra) <= maxWords);
          VERIFY(lsl.getLen() == len + extra);
          tailVal += extra;
        }
        else
        {
          printf("Enqueue failed at length %u plus %u words\n",
                 len, extra);
          Uint32 offset = headVal % SectionSegment::DataLength;
          Uint32 realNewLen = offset + len + extra;
          if (realNewLen <= maxWords) {abort();};
          VERIFY(realNewLen > maxWords);
          VERIFY(lsl.getLen() == len);
          break;
        }
      }

      checkSegFootprint(headVal, lsl.getLen(), getActualUsed(ssp));
 
      /**
       * Now drain partially or fully, checking data
       */
      Uint32 segsToDrain = myRandom48(maxSegs) + 1;

      do
      {
        Uint32 drainLen = myRandom48(249) + 1;
        Uint32 len = lsl.getLen();

        bool result = lsl.deqWords(buff, drainLen);

        if (!result)
        {
          VERIFY(drainLen > len);
          VERIFY(lsl.getLen() == len);
          continue;
        }

        for (Uint32 k=0; k < drainLen; k++)
        {
          VERIFY(buff[k] == headVal + k);
        }
        VERIFY(lsl.getLen() == len - drainLen);
        headVal += drainLen;
      } while (ssp.getNumAvailable() < segsToDrain);

      printf("Dequeued down to %u words with %u segments available\n",
             lsl.getLen(), ssp.getNumAvailable());

      /* Now iterate and fill again */
    }
 
    printf("Mai testSubPoolLimit: verify2: used %u max seg %u actual %u \n",
           g_sectionSegmentPool.getUsed(), maxSegs, getActualUsed(ssp));
    VERIFY(g_sectionSegmentPool.getUsed() == maxSegs);

    checkSegFootprint(headVal, lsl.getLen(), getActualUsed(ssp));

    lsl.empty();
  }

  return true;
}

TAPTEST(SegmentList)
{
  /* Test SegmentList
   * -------------------
   * To run this code :
   *   cd storage/ndb/src/kernel/vm
   *   ./SegmentList-t
   * 
   * Will print "OK" in success case and return 0
   */

  g_sectionSegmentPool.setSize(NUM_SEGMENTS);

  printf("g_sectionSegmentPool size is %u\n",
         g_sectionSegmentPool.getSize());

  printf("Testing SegmentList\n");

  do
  {
    if (!testBasicFillAndDrain())
    {
      break;
    }

    if (!testMixedEnqAndDeq())
    {
      break;
    }

    if (!testSubPoolLimit())
    {
      break;
    }

    printf("\nOK\n");

    return 1;
  } while(0);

  printf("\nFAILED\n");
  return 0;
}

#endif
