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

#define DBTUX_CMP_CPP
#include "Dbtux.hpp"

/*
 * Search key vs node prefix.
 *
 * The comparison starts at given attribute position (in fact 0).  The
 * position is updated by number of equal initial attributes found.  The
 * prefix may be partial in which case CmpUnknown may be returned.
 */
int
Dbtux::cmpSearchKey(const Frag& frag, unsigned& start, TableData searchKey, ConstData entryData, unsigned maxlen)
{
  const unsigned numAttrs = frag.m_numAttrs;
  const DescEnt& descEnt = getDescEnt(frag.m_descPage, frag.m_descOff);
  // number of words of attribute data left
  unsigned len2 = maxlen;
  // skip to right position in search key
  searchKey += start;
  int ret = 0;
  while (start < numAttrs) {
    if (len2 < AttributeHeaderSize) {
      jam();
      ret = NdbSqlUtil::CmpUnknown;
      break;
    }
    len2 -= AttributeHeaderSize;
    if (*searchKey != 0) {
      if (! entryData.ah().isNULL()) {
        jam();
        // current attribute
        const DescAttr& descAttr = descEnt.m_descAttr[start];
        const unsigned typeId = descAttr.m_typeId;
        // full data size
        const unsigned size1 = AttributeDescriptor::getSizeInWords(descAttr.m_attrDesc);
        ndbrequire(size1 != 0 && size1 == entryData.ah().getDataSize());
        const unsigned size2 = min(size1, len2);
        len2 -= size2;
        // compare
        const Uint32* const p1 = *searchKey;
        const Uint32* const p2 = &entryData[AttributeHeaderSize];
        ret = NdbSqlUtil::cmp(typeId, p1, p2, size1, size2);
        if (ret != 0) {
          jam();
          break;
        }
      } else {
        jam();
        // not NULL > NULL
        ret = +1;
        break;
      }
    } else {
      if (! entryData.ah().isNULL()) {
        jam();
        // NULL < not NULL
        ret = -1;
        break;
      }
    }
    searchKey += 1;
    entryData += AttributeHeaderSize + entryData.ah().getDataSize();
    start++;
  }
  // XXX until data format errors are handled
  ndbrequire(ret != NdbSqlUtil::CmpError);
  return ret;
}

/*
 * Search key vs tree entry.
 *
 * Start position is updated as in previous routine.
 */
int
Dbtux::cmpSearchKey(const Frag& frag, unsigned& start, TableData searchKey, TableData entryKey)
{
  const unsigned numAttrs = frag.m_numAttrs;
  const DescEnt& descEnt = getDescEnt(frag.m_descPage, frag.m_descOff);
  // skip to right position
  searchKey += start;
  entryKey += start;
  int ret = 0;
  while (start < numAttrs) {
    if (*searchKey != 0) {
      if (*entryKey != 0) {
        jam();
        // current attribute
        const DescAttr& descAttr = descEnt.m_descAttr[start];
        const unsigned typeId = descAttr.m_typeId;
        // full data size
        const unsigned size1 = AttributeDescriptor::getSizeInWords(descAttr.m_attrDesc);
        // compare
        const Uint32* const p1 = *searchKey;
        const Uint32* const p2 = *entryKey;
        ret = NdbSqlUtil::cmp(typeId, p1, p2, size1, size1);
        if (ret != 0) {
          jam();
          break;
        }
      } else {
        jam();
        // not NULL > NULL
        ret = +1;
        break;
      }
    } else {
      if (*entryKey != 0) {
        jam();
        // NULL < not NULL
        ret = -1;
        break;
      }
    }
    searchKey += 1;
    entryKey += 1;
    start++;
  }
  // XXX until data format errors are handled
  ndbrequire(ret != NdbSqlUtil::CmpError);
  return ret;
}

/*
 * Scan bound vs node prefix.
 *
 * Compare lower or upper bound and index attribute data.  The attribute
 * data may be partial in which case CmpUnknown may be returned.
 * Returns -1 if the boundary is to the left of the compared key and +1
 * if the boundary is to the right of the compared key.
 *
 * To get this behaviour we treat equality a little bit special.  If the
 * boundary is a lower bound then the boundary is to the left of all
 * equal keys and if it is an upper bound then the boundary is to the
 * right of all equal keys.
 *
 * When searching for the first key we are using the lower bound to try
 * to find the first key that is to the right of the boundary.  Then we
 * start scanning from this tuple (including the tuple itself) until we
 * find the first key which is to the right of the boundary.  Then we
 * stop and do not include that key in the scan result.
 */
int
Dbtux::cmpScanBound(const Frag& frag, unsigned dir, ConstData boundInfo, unsigned boundCount, ConstData entryData, unsigned maxlen)
{
  const DescEnt& descEnt = getDescEnt(frag.m_descPage, frag.m_descOff);
  // direction 0-lower 1-upper
  ndbrequire(dir <= 1);
  // number of words of data left
  unsigned len2 = maxlen;
  /*
   * No boundary means full scan, low boundary is to the right of all
   * keys.  Thus we should always return -1.  For upper bound we are to
   * the right of all keys, thus we should always return +1.  We achieve
   * this behaviour by initializing type to 4.
   */
  unsigned type = 4;
  while (boundCount != 0) {
    if (len2 < AttributeHeaderSize) {
      jam();
      return NdbSqlUtil::CmpUnknown;
    }
    len2 -= AttributeHeaderSize;
    // get and skip bound type
    type = boundInfo[0];
    boundInfo += 1;
    if (! boundInfo.ah().isNULL()) {
      if (! entryData.ah().isNULL()) {
        jam();
        // current attribute
        const unsigned index = boundInfo.ah().getAttributeId();
        const DescAttr& descAttr = descEnt.m_descAttr[index];
        const unsigned typeId = descAttr.m_typeId;
        ndbrequire(entryData.ah().getAttributeId() == descAttr.m_primaryAttrId);
        // full data size
        const unsigned size1 = boundInfo.ah().getDataSize();
        ndbrequire(size1 != 0 && size1 == entryData.ah().getDataSize());
        const unsigned size2 = min(size1, len2);
        len2 -= size2;
        // compare
        const Uint32* const p1 = &boundInfo[AttributeHeaderSize];
        const Uint32* const p2 = &entryData[AttributeHeaderSize];
        int ret = NdbSqlUtil::cmp(typeId, p1, p2, size1, size2);
        // XXX until data format errors are handled
        ndbrequire(ret != NdbSqlUtil::CmpError);
        if (ret != 0) {
          jam();
          return ret;
        }
      } else {
        jam();
        // not NULL > NULL
        return +1;
      }
    } else {
      jam();
      if (! entryData.ah().isNULL()) {
        jam();
        // NULL < not NULL
        return -1;
      }
    }
    boundInfo += AttributeHeaderSize + boundInfo.ah().getDataSize();
    entryData += AttributeHeaderSize + entryData.ah().getDataSize();
    boundCount -= 1;
  }
  if (dir == 0) {
    jam();
    /*
     * Looking for the lower bound.  If strict lower bound then the
     * boundary is to the right of the compared key and otherwise (equal
     * included in range) then the boundary is to the left of the key.
     */
    if (type == 1) {
      jam();
      return +1;
    }
    return -1;
  } else {
    jam();
    /*
     * Looking for the upper bound.  If strict upper bound then the
     * boundary is to the left of all equal keys and otherwise (equal
     * included in the range) then the boundary is to the right of all
     * equal keys.
     */
    if (type == 3) {
      jam();
      return -1;
    }
    return +1;
  }
}

/*
 * Scan bound vs tree entry.
 */
int
Dbtux::cmpScanBound(const Frag& frag, unsigned dir, ConstData boundInfo, unsigned boundCount, TableData entryKey)
{
  const DescEnt& descEnt = getDescEnt(frag.m_descPage, frag.m_descOff);
  // direction 0-lower 1-upper
  ndbrequire(dir <= 1);
  // initialize type to equality
  unsigned type = 4;
  while (boundCount != 0) {
    // get and skip bound type
    type = boundInfo[0];
    boundInfo += 1;
    if (! boundInfo.ah().isNULL()) {
      if (*entryKey != 0) {
        jam();
        // current attribute
        const unsigned index = boundInfo.ah().getAttributeId();
        const DescAttr& descAttr = descEnt.m_descAttr[index];
        const unsigned typeId = descAttr.m_typeId;
        // full data size
        const unsigned size1 = AttributeDescriptor::getSizeInWords(descAttr.m_attrDesc);
        // compare
        const Uint32* const p1 = &boundInfo[AttributeHeaderSize];
        const Uint32* const p2 = *entryKey;
        int ret = NdbSqlUtil::cmp(typeId, p1, p2, size1, size1);
        // XXX until data format errors are handled
        ndbrequire(ret != NdbSqlUtil::CmpError);
        if (ret != 0) {
          jam();
          return ret;
        }
      } else {
        jam();
        // not NULL > NULL
        return +1;
      }
    } else {
      jam();
      if (*entryKey != 0) {
        jam();
        // NULL < not NULL
        return -1;
      }
    }
    boundInfo += AttributeHeaderSize + boundInfo.ah().getDataSize();
    entryKey += 1;
    boundCount -= 1;
  }
  if (dir == 0) {
    // lower bound
    jam();
    if (type == 1) {
      jam();
      return +1;
    }
    return -1;
  } else {
    // upper bound
    jam();
    if (type == 3) {
      jam();
      return -1;
    }
    return +1;
  }
}
