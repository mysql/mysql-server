#ifndef CONTAINER_HPP
#define CONTAINER_HPP

/*
   Copyright (c) 2013, 2016, Oracle and/or its affiliates. All rights reserved.

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

#define JAM_FILE_ID 342


#define BITSENUMS(name, pos, bits) name##_POS = (pos), name##_BITS = (bits), name##_MASK = (1 << (bits)) - 1
#define GETBITS(name, value) (((value) >> name##_POS) & name##_MASK)
#define SETBITS(name, value, bits) (((value) & ~(name##_MASK << name##_POS)) | ((bits) << name##_POS))
#define CHECKBITS(name, bits) ((bits) <= (name##_MASK))

class Container
{
public:
  class Header;
public:
  enum { CONTAINERS_PER_PAGE = 72,
         MAX_CONTAINER_INDEX = CONTAINERS_PER_PAGE - 1,
         NO_CONTAINER_INDEX = CONTAINERS_PER_PAGE };
  enum { HEADER_SIZE = 2,
         DOWN_LIMIT = 12 ,
         UP_LIMIT = 14,
         CONTAINER_SIZE = 28 };
};

class Container::Header
{
/**
 * A container is a buffer of Container::SIZE words.
 * The container can be used from both ends.
 * An end can have a free or inuse header.
 * The free ends are arranged in a double linked list.
 * If a container end uses more than UP_LIMIT words
 * it tries to reserve the other end if it is free,
 * in this case the other end have no header, and
 * removed from the double linked free list.
 *
 * Common layout of container header
 *
 * llllllh. ........ ........ ........
 * 33222222 22221111 111111
 * 10987654 32109876 54321098 76543210
 *
 * llllll - length of used part of container
 *            0 for free container end
 *            >0 for used container end
 * h      - header marker,
 *            0 - free container end
 *            1 - used container end
 *
 * Layout of a free container ends header
 *
 * llllllh. ........ ........ .nnnnnnn
 * 33222222 22221111 111111
 * 10987654 32109876 54321098 76543210
 *
 * nnnnnnn - index of next free end
 *
 * llllllhP SSSSSSSS SSSS.bse ennnnnnn
 * 33222222 22221111 111111
 * 10987654 32109876 54321098 76543210
 *
 * b - using both ends
 *       0 - other end may be used
 *       1 - other end of container is used by current end
 *
 * s - next container in same page
 *       0 - next container in other page
 *           page i-value in next word
 *       1 - next container in same page
 *
 * ee - end used of next container
 *        00 - no next container
 *        01 - left end
 *        10 - right end
 *        11 - illegal value
 *
 * P - scan in progress, if 1 elements scanbits may have more bits set
 * SSSSSSSSSSSS - scan bits
 *        One bit per scan.  The bit is set if all elements in
 *        container are scanned.
 *
 * nnnnnnn - index of next container
 *
 * Note, bits 11-24 was previously used to keep a double
 * linked list of used containers in a page.  This list
 * was used by LCP sometime in ancient history.  The use
 * of these bits are removed at same time this class is
 * introduced.
 */
public:
  Header();
  Header(Uint32 const header);
  Header(Header const& header);
  operator Uint32 const&() const;
  Header& operator =(Header const& header);

  bool isFree() const;
  bool isInUse() const;
  Uint32 getLength() const;
  Header& setLength(Uint32 length);

  Header& initFree();
  bool haveNextFree() const;
  Uint32& getNextFree() const;
  Header& clearNextFree();
  Header& setNextFree(Uint32 index);

  Header& initInUse();
  bool haveNext() const;
  Uint32 getNextEnd() const;
  Uint32 getNextIndexNumber() const;
  bool isUsingBothEnds() const;
  Uint32 getScanBits() const;
  bool isScanInProgress() const;
  Header& clearUsingBothEnds();
  Header& setUsingBothEnds();
  bool isNextOnSamePage() const;
  Header& setNext(Uint32 end, Uint32 index, bool onsamepage);
  Header& clearNext();
  Header& copyScanBits(Uint32 scanmask);
  Header& setScanBits(Uint32 scanmask);
  Header& clearScanBits(Uint32 scanmask);
  Header& setScanInProgress();
  Header& clearScanInProgress();
private:
  bool isHeader() const;
  Header& setHeader();
  Header& clearHeader();
private:
  /**
   * Defines of helper constants for accessing the specific part of header
   * bits.
   * BITSENUMS(NAME, pos, size) defines constants:
   *   NAME_POS = pos, used to shift down the header bitmask.
   *   NAME_MASK with lowest bits set according to size, used to mask out
   *             the value after shift
   * These constants are used by GETBITS/CHECKBITS/SETBITS macros.
   */
  enum { BITSENUMS(LENGTH, 26, 6) };
  enum { BITSENUMS(HEADER, 25, 1) };
  enum { BITSENUMS(SCAN_IN_PROGRESS, 24, 1) };
  enum { BITSENUMS(SCAN_BITS, 12, 12) };
  enum { BITSENUMS(USING_BOTH_ENDS, 10, 1) };
  enum { BITSENUMS(NEXT_ON_SAME_PAGE, 9, 1) };
  enum { BITSENUMS(NEXT_END, 7, 2) };
  enum { BITSENUMS(NEXT_INDEX, 0, 7) };
  enum { BITSENUMS(NEXT_FREE, 0, 7) };
private:
  Uint32 m_header;
};

/*
 * Implementation: Container::Header
 */

inline
Container::Header::Header()
: m_header(~0U)
{
}

inline
Container::Header::Header(Uint32 const header)
: m_header(header)
{
}

inline
Container::Header::Header(Header const& header)
: m_header(header)
{
}

inline
Container::Header& Container::Header::operator =(Header const& header)
{
  m_header = header.m_header;
  return *this;
}

inline
Container::Header::operator Uint32 const& () const
{
  return m_header;
}

inline
Container::Header& Container::Header::setHeader()
{
  m_header = SETBITS(HEADER, m_header, 1);
  return *this;
}

inline
Container::Header& Container::Header::clearHeader()
{
  m_header = SETBITS(HEADER, m_header, 0);
  return *this;
}

inline
bool Container::Header::isInUse() const
{
  return isHeader();
}

inline
bool Container::Header::isFree() const
{
  return !isHeader();
}

inline
bool Container::Header::isHeader() const
{
  bool isheader = GETBITS(HEADER, m_header);
#ifdef VM_TRACE
  if (isheader) assert(getLength() > 0);
  else assert(getLength() == 0);
#endif
  return isheader;
}

inline
Uint32 Container::Header::getLength() const
{
  return GETBITS(LENGTH, m_header);
}

inline
Uint32 Container::Header::getNextEnd() const
{
  assert(isInUse());
  return GETBITS(NEXT_END, m_header);
}

inline
bool Container::Header::haveNext() const
{
  assert(isInUse());
  Uint32 end = getNextEnd();
  assert(end < 3);
  return end != 0;
}

inline
Uint32 Container::Header::getNextIndexNumber() const
{
  assert(isInUse());
  return GETBITS(NEXT_INDEX, m_header);
}

inline
bool Container::Header::isUsingBothEnds() const
{
  assert(isInUse());
  return GETBITS(USING_BOTH_ENDS, m_header);
}

inline
bool Container::Header::isNextOnSamePage() const
{
  assert(isInUse());
  return GETBITS(NEXT_ON_SAME_PAGE, m_header);
}

inline
Uint32 Container::Header::getScanBits() const
{
  assert(isInUse());
  return GETBITS(SCAN_BITS, m_header);
}

inline
bool Container::Header::isScanInProgress() const
{
  assert(isInUse());
  return GETBITS(SCAN_IN_PROGRESS, m_header);
}

inline
Container::Header& Container::Header::clearUsingBothEnds()
{
  assert(isInUse());
  m_header = SETBITS(USING_BOTH_ENDS, m_header, 0);
  return *this;
}

inline
Container::Header& Container::Header::setUsingBothEnds()
{
  assert(isInUse());
  assert(!isUsingBothEnds());
  m_header = SETBITS(USING_BOTH_ENDS, m_header, 1);
  return *this;
}

inline
Container::Header& Container::Header::initFree()
{
  m_header = 0; // clear all (unused) bits
  clearHeader();
  setLength(0);
  clearNextFree();
  return *this;
}

inline
Container::Header& Container::Header::initInUse()
{
  m_header = 0; // clear all (unused) bits
  setHeader();
  setLength(HEADER_SIZE);
  clearUsingBothEnds();
  clearNext();
  return *this;
}

inline
Container::Header& Container::Header::setLength(Uint32 length)
{
  assert(length <= CONTAINER_SIZE);
  m_header = SETBITS(LENGTH, m_header, length);
  return *this;
}

inline
Container::Header& Container::Header::setNext(Uint32 end, Uint32 index, bool onsamepage)
{
  assert(isInUse());
  assert(end < 3);
  assert(index <= NEXT_INDEX_MASK);
  m_header = SETBITS(NEXT_ON_SAME_PAGE, m_header, onsamepage);
  m_header = SETBITS(NEXT_END, m_header, end);
  m_header = SETBITS(NEXT_INDEX, m_header, index);
  return *this;
}

inline
Container::Header& Container::Header::clearNext()
{
  assert(isInUse());
  return setNext(0, 0, false);
}

inline
bool Container::Header::haveNextFree() const
{
  assert(isFree());
  return getNextFree() <= MAX_CONTAINER_INDEX;
}

inline
Container::Header& Container::Header::copyScanBits(Uint32 scanmask)
{
  assert(isInUse());
  assert(CHECKBITS(SCAN_BITS, scanmask));
  m_header = SETBITS(SCAN_BITS, m_header, scanmask);
  return *this;
}

inline
Container::Header& Container::Header::setScanBits(Uint32 scanmask)
{
  assert(isInUse());
  assert((getScanBits() & scanmask) == 0);
  assert(CHECKBITS(SCAN_BITS, scanmask));
  scanmask |= getScanBits();
  m_header = SETBITS(SCAN_BITS, m_header, scanmask);
  return *this;
}

inline
Container::Header& Container::Header::clearScanBits(Uint32 scanmask)
{
  assert(isInUse());
  assert((getScanBits() & scanmask) == scanmask);
  assert(CHECKBITS(SCAN_BITS, scanmask));
  scanmask = getScanBits() & ~scanmask;
  m_header = SETBITS(SCAN_BITS, m_header, scanmask);
  return *this;
}

inline
Container::Header& Container::Header::setScanInProgress()
{
  assert(isInUse());
  assert(!isScanInProgress());
  m_header = SETBITS(SCAN_IN_PROGRESS, m_header, 1);
  return *this;
}

inline
Container::Header& Container::Header::clearScanInProgress()
{
  assert(isInUse());
  assert(isScanInProgress());
  m_header = SETBITS(SCAN_IN_PROGRESS, m_header, 0);
  return *this;
}

#undef JAM_FILE_ID

#endif
