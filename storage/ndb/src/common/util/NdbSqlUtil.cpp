/*
   Copyright (c) 2003, 2019, Oracle and/or its affiliates. All rights reserved.

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

#include <NdbSqlUtil.hpp>

#include <math.h>

#include "my_byteorder.h"
#include "m_ctype.h"

/*
 * Data types.  The entries must be in the numerical order.
 */

const NdbSqlUtil::Type
NdbSqlUtil::m_typeList[] = {
  { // 0
    Type::Undefined,
    NULL,
    NULL,
    NULL
  },
  { // 1
    Type::Tinyint,
    cmpTinyint,
    NULL,
    NULL
  },
  { // 2
    Type::Tinyunsigned,
    cmpTinyunsigned,
    NULL,
    NULL
  },
  { // 3
    Type::Smallint,
    cmpSmallint,
    NULL,
    NULL
  },
  { // 4
    Type::Smallunsigned,
    cmpSmallunsigned,
    NULL,
    NULL
  },
  { // 5
    Type::Mediumint,
    cmpMediumint,
    NULL,
    NULL
  },
  { // 6
    Type::Mediumunsigned,
    cmpMediumunsigned,
    NULL,
    NULL
  },
  { // 7
    Type::Int,
    cmpInt,
    NULL,
    NULL
  },
  { // 8
    Type::Unsigned,
    cmpUnsigned,
    NULL,
    NULL
  },
  { // 9
    Type::Bigint,
    cmpBigint,
    NULL,
    NULL
  },
  { // 10
    Type::Bigunsigned,
    cmpBigunsigned,
    NULL,
    NULL
  },
  { // 11
    Type::Float,
    cmpFloat,
    NULL,
    NULL
  },
  { // 12
    Type::Double,
    cmpDouble,
    NULL,
    NULL
  },
  { // 13
    Type::Olddecimal,
    cmpOlddecimal,
    NULL,
    NULL
  },
  { // 14
    Type::Char,
    cmpChar,
    likeChar,
    NULL
  },
  { // 15
    Type::Varchar,
    cmpVarchar,
    likeVarchar,
    NULL
  },
  { // 16
    Type::Binary,
    cmpBinary,
    likeBinary,
    NULL
  },
  { // 17
    Type::Varbinary,
    cmpVarbinary,
    likeVarbinary,
    NULL
  },
  { // 18
    Type::Datetime,
    cmpDatetime,
    NULL,
    NULL
  },
  { // 19
    Type::Date,
    cmpDate,
    NULL,
    NULL
  },
  { // 20
    Type::Blob,
    NULL,
    NULL,
    NULL
  },
  { // 21
    Type::Text,
    NULL,
    NULL,
    NULL
  },
  { // 22
    Type::Bit,
    cmpBit,
    NULL,
    maskBit
  },
  { // 23
    Type::Longvarchar,
    cmpLongvarchar,
    likeLongvarchar,
    NULL
  },
  { // 24
    Type::Longvarbinary,
    cmpLongvarbinary,
    likeLongvarbinary,
    NULL
  },
  { // 25
    Type::Time,
    cmpTime,
    NULL,
    NULL
  },
  { // 26
    Type::Year,
    cmpYear,
    NULL,
    NULL
  },
  { // 27
    Type::Timestamp,
    cmpTimestamp,
    NULL,
    NULL
  },
  { // 28
    Type::Olddecimalunsigned,
    cmpOlddecimalunsigned,
    NULL,
    NULL
  },
  { // 29
    Type::Decimal,
    cmpDecimal,
    NULL,
    NULL
  },
  { // 30
    Type::Decimalunsigned,
    cmpDecimalunsigned,
    NULL,
    NULL
  },
  { // 31
    Type::Time2,
    cmpTime2,
    NULL,
    NULL
  },
  { // 32
    Type::Datetime2,
    cmpDatetime2,
    NULL,
    NULL
  },
  { // 33
    Type::Timestamp2,
    cmpTimestamp2,
    NULL,
    NULL
  },
};

const NdbSqlUtil::Type&
NdbSqlUtil::getType(Uint32 typeId)
{
  if (typeId < sizeof(m_typeList) / sizeof(m_typeList[0]) &&
      m_typeList[typeId].m_typeId != Type::Undefined) {
    return m_typeList[typeId];
  }
  return m_typeList[Type::Undefined];
}

/*
 * Comparison functions.
 */

int
NdbSqlUtil::cmpTinyint(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2)
{
  assert(info == 0 && n1 == 1 && n2 == 1);
  Int8 v1, v2;
  memcpy(&v1, p1, 1);
  memcpy(&v2, p2, 1);
  int w1 = (int)v1;
  int w2 = (int)v2;
  return w1 - w2;
}

int
NdbSqlUtil::cmpTinyunsigned(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2)
{
  assert(info == 0 && n1 == 1 && n2 == 1);
  Uint8 v1, v2;
  memcpy(&v1, p1, 1);
  memcpy(&v2, p2, 1);
  int w1 = (int)v1;
  int w2 = (int)v2;
  return w1 - w2;
}

int
NdbSqlUtil::cmpSmallint(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2)
{
  assert(info == 0 && n1 == 2 && n2 == 2);
  Int16 v1, v2;
  memcpy(&v1, p1, 2);
  memcpy(&v2, p2, 2);
  int w1 = (int)v1;
  int w2 = (int)v2;
  return w1 - w2;
}

int
NdbSqlUtil::cmpSmallunsigned(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2)
{
  assert(info == 0 && n1 == 2 && n2 == 2);
  Uint16 v1, v2;
  memcpy(&v1, p1, 2);
  memcpy(&v2, p2, 2);
  int w1 = (int)v1;
  int w2 = (int)v2;
  return w1 - w2;
}

int
NdbSqlUtil::cmpMediumint(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2)
{
  assert(info == 0 && n1 == 3 && n2 == 3);
  uchar b1[4];
  uchar b2[4];
  memcpy(b1, p1, 3);
  b1[3] = 0;
  memcpy(b2, p2, 3);
  b2[3] = 0;
  int w1 = (int)sint3korr(b1);
  int w2 = (int)sint3korr(b2);
  return w1 - w2;
}

int
NdbSqlUtil::cmpMediumunsigned(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2)
{
  assert(info == 0 && n1 == 3 && n2 == 3);
  uchar b1[4];
  uchar b2[4];
  memcpy(b1, p1, 3);
  b1[3] = 0;
  memcpy(b2, p2, 3);
  b2[3] = 0;
  int w1 = (int)uint3korr(b1);
  int w2 = (int)uint3korr(b2);
  return w1 - w2;
}

int
NdbSqlUtil::cmpInt(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2)
{
  assert(info == 0 && n1 == 4 && n2 == 4);
  Int32 v1, v2;
  memcpy(&v1, p1, 4);
  memcpy(&v2, p2, 4);
  if (v1 < v2)
    return -1;
  if (v1 > v2)
    return +1;
  return 0;
}

int
NdbSqlUtil::cmpUnsigned(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2)
{
  assert(info == 0 && n1 == 4 && n2 == 4);
  Uint32 v1, v2;
  memcpy(&v1, p1, 4);
  memcpy(&v2, p2, 4);
  if (v1 < v2)
    return -1;
  if (v1 > v2)
    return +1;
  return 0;
}

int
NdbSqlUtil::cmpBigint(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2)
{
  assert(info == 0 && n1 == 8 && n2 == 8);
  Int64 v1, v2;
  memcpy(&v1, p1, 8);
  memcpy(&v2, p2, 8);
  if (v1 < v2)
    return -1;
  if (v1 > v2)
    return +1;
  return 0;
}

int
NdbSqlUtil::cmpBigunsigned(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2)
{
  assert(info == 0 && n1 == 8 && n2 == 8);
  Uint64 v1, v2;
  memcpy(&v1, p1, 8);
  memcpy(&v2, p2, 8);
  if (v1 < v2)
    return -1;
  if (v1 > v2)
    return +1;
  return 0;
}

int
NdbSqlUtil::cmpFloat(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2)
{
  assert(info == 0 && n1 == 4 && n2 == 4);
  float v1, v2;
  memcpy(&v1, p1, 4);
  memcpy(&v2, p2, 4);
  require(!isnan(v1) && !isnan(v2));
  if (v1 < v2)
    return -1;
  if (v1 > v2)
    return +1;
  return 0;
}

int
NdbSqlUtil::cmpDouble(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2)
{
  assert(info == 0 && n1 == 8 && n2 == 8);
  double v1, v2;
  memcpy(&v1, p1, 8);
  memcpy(&v2, p2, 8);
  require(!isnan(v1) && !isnan(v2));
  if (v1 < v2)
    return -1;
  if (v1 > v2)
    return +1;
  return 0;
}

int
NdbSqlUtil::cmpOlddecimal(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2)
{
  assert(info == 0 && n1 == n2);
  const uchar* v1 = (const uchar*)p1;
  const uchar* v2 = (const uchar*)p2;
  int sgn = +1;
  unsigned i = 0;
  while (i < n1) {
    int c1 = v1[i];
    int c2 = v2[i];
    if (c1 == c2) {
      if (c1 == '-')
        sgn = -1;
    } else if (c1 == '-') {
      return -1;
    } else if (c2 == '-') {
      return +1;
    } else if (c1 < c2) {
      return -1 * sgn;
    } else {
      return +1 * sgn;
    }
    i++;
  }
  return 0;
}

int
NdbSqlUtil::cmpOlddecimalunsigned(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2)
{
  return cmpOlddecimal(info, p1, n1, p2, n2);
}

int
NdbSqlUtil::cmpDecimal(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2)
{
  return cmpBinary(info, p1, n1, p2, n2);
}

int
NdbSqlUtil::cmpDecimalunsigned(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2)
{
  return cmpBinary(info, p1, n1, p2, n2);
}

int
NdbSqlUtil::cmpChar(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2)
{
  // allow different lengths
  assert(info != 0);
  const uchar* v1 = (const uchar*)p1;
  const uchar* v2 = (const uchar*)p2;
  CHARSET_INFO* cs = (CHARSET_INFO*)info;

  // Comparing with a NO_PAD collation requires trailing spaces to be stripped.
  if (cs->pad_attribute == NO_PAD)
  {
    n1 = cs->cset->lengthsp(cs, (const char *)p1, n1);
    n2 = cs->cset->lengthsp(cs, (const char *)p2, n2);
  }
  return (*cs->coll->strnncollsp)(cs, v1, n1, v2, n2);
}

int
NdbSqlUtil::cmpVarchar(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2)
{
  assert(info != 0);
  const uint lb = 1;
  const uchar* v1 = (const uchar*)p1;
  const uchar* v2 = (const uchar*)p2;
  uint m1 = v1[0];
  uint m2 = v2[0];
  require(lb + m1 <= n1 && lb + m2 <= n2);
  CHARSET_INFO* cs = (CHARSET_INFO*)info;
  // compare with space padding
  int k = (*cs->coll->strnncollsp)(cs, v1 + lb, m1, v2 + lb, m2);
  return k;
}

int
NdbSqlUtil::cmpBinary(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2)
{
  // allow different lengths
  assert(info == 0);
  const uchar* v1 = (const uchar*)p1;
  const uchar* v2 = (const uchar*)p2;
  int k = 0;
  if (n1 < n2) {
    k = memcmp(v1, v2, n1);
    if (k == 0)
      k = -1;
  } else if (n1 > n2) {
    k = memcmp(v1, v2, n2);
    if (k == 0)
      k = +1;
  } else {
    k = memcmp(v1, v2, n1);
  }
  return k;
}

int
NdbSqlUtil::cmpVarbinary(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2)
{
  assert(info == 0);
  const uint lb = 1;
  const uchar* v1 = (const uchar*)p1;
  const uchar* v2 = (const uchar*)p2;
  uint m1 = v1[0];
  uint m2 = v2[0];
  require(lb + m1 <= n1 && lb + m2 <= n2);
  int k = cmpBinary(info, v1 + lb, m1, v2 + lb, m2);
  return k;
}

int
NdbSqlUtil::cmpDatetime(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2)
{
  assert(info == 0 && n1 == 8 && n2 == 8);
  Int64 v1, v2;
  memcpy(&v1, p1, sizeof(Int64));
  memcpy(&v2, p2, sizeof(Int64));
  if (v1 < v2)
    return -1;
  if (v1 > v2)
    return +1;
  return 0;
}

int
NdbSqlUtil::cmpDate(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2)
{
  assert(info == 0 && n1 == 3 && n2 == 3);
  uchar b1[4];
  uchar b2[4];
  memcpy(b1, p1, 3);
  b1[3] = 0;
  memcpy(b2, p2, 3);
  b2[3] = 0;
  // from Field_newdate::val_int
  int w1 = (int)uint3korr(b1);
  int w2 = (int)uint3korr(b2);
  return w1 - w2;
}

// not supported
int
NdbSqlUtil::cmpBlob(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2)
{
  assert(false);
  return 0;
}

// not supported
int
NdbSqlUtil::cmpText(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2)
{
  assert(false);
  return 0;
}

int
NdbSqlUtil::cmpBit(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2)
{ 
  /* Bitfields are stored as 32-bit words
   * This means that a byte-by-byte comparison will not work on all platforms
   * We do a word-wise comparison of the significant bytes.
   * It is assumed that insignificant bits (but not bytes) are zeroed in the
   * passed values.
   */
  const Uint32 bytes= MIN(n1, n2);
  Uint32 words= (bytes + 3) >> 2;

  /* Don't expect either value to be length zero */
  assert(words);

  /* Check ptr alignment */
  if (unlikely(((((UintPtr)p1) & 3) != 0) ||
               ((((UintPtr)p2) & 3) != 0)))
  {
    Uint32 copyP1[ MAX_TUPLE_SIZE_IN_WORDS ];
    Uint32 copyP2[ MAX_TUPLE_SIZE_IN_WORDS ];
    memcpy(copyP1, p1, words << 2);
    memcpy(copyP2, p2, words << 2);

    return cmpBit(info, copyP1, bytes, copyP2, bytes);
  }

  const Uint32* wp1= (const Uint32*) p1;
  const Uint32* wp2= (const Uint32*) p2;
  while (--words)
  {
    if (*wp1 < *wp2)
      return -1;
    if (*(wp1++) > *(wp2++))
      return 1;
  }

  /* For the last word, we mask out any insignificant bytes */
  const Uint32 sigBytes= bytes & 3; // 0..3; 0 == all bytes significant
  const Uint32 mask= sigBytes?
    (1 << (sigBytes *8)) -1 :
    ~0;
  const Uint32 lastWord1= *wp1 & mask;
  const Uint32 lastWord2= *wp2 & mask;
  
  if (lastWord1 < lastWord2)
    return -1;
  if (lastWord1 > lastWord2)
    return 1;

  return 0;
}


int
NdbSqlUtil::cmpTime(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2)
{
  assert(info == 0 && n1 == 3 && n2 == 3);
  uchar b1[4];
  uchar b2[4];
  memcpy(b1, p1, 3);
  b1[3] = 0;
  memcpy(b2, p2, 3);
  b2[3] = 0;
  // from Field_time::val_int
  int j1 = (int)sint3korr(b1);
  int j2 = (int)sint3korr(b2);
  if (j1 < j2)
    return -1;
  if (j1 > j2)
    return +1;
  return 0;
}

// not yet

int
NdbSqlUtil::cmpLongvarchar(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2)
{
  assert(info != 0);
  const uint lb = 2;
  const uchar* v1 = (const uchar*)p1;
  const uchar* v2 = (const uchar*)p2;
  uint m1 = v1[0] | (v1[1] << 8);
  uint m2 = v2[0] | (v2[1] << 8);
  require(lb + m1 <= n1 && lb + m2 <= n2);
  CHARSET_INFO* cs = (CHARSET_INFO*)info;
  // compare with space padding
  int k = (*cs->coll->strnncollsp)(cs, v1 + lb, m1, v2 + lb, m2);
  return k;
}

int
NdbSqlUtil::cmpLongvarbinary(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2)
{
  assert(info == 0);
  const uint lb = 2;
  const uchar* v1 = (const uchar*)p1;
  const uchar* v2 = (const uchar*)p2;
  uint m1 = v1[0] | (v1[1] << 8);
  uint m2 = v2[0] | (v2[1] << 8);
  require(lb + m1 <= n1 && lb + m2 <= n2);
  int k = cmpBinary(info, v1 + lb, m1, v2 + lb, m2);
  return k;
}

int
NdbSqlUtil::cmpYear(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2)
{
  assert(info == 0 && n1 == 1 && n2 == 1);
  Uint8 v1, v2;
  memcpy(&v1, p1, 1);
  memcpy(&v2, p2, 1);
  int w1 = (int)v1;
  int w2 = (int)v2;
  return w1 - w2;
}

int
NdbSqlUtil::cmpTimestamp(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2)
{
  assert(info == 0 && n1 == 4 && n2 == 4);
  Uint32 v1, v2;
  memcpy(&v1, p1, 4);
  memcpy(&v2, p2, 4);
  if (v1 < v2)
    return -1;
  if (v1 > v2)
    return +1;
  return 0;
}

// times with fractional seconds are big-endian binary-comparable

int
NdbSqlUtil::cmpTime2(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2)
{
  return cmpBinary(info, p1, n1, p2, n2);
}

int
NdbSqlUtil::cmpDatetime2(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2)
{
  return cmpBinary(info, p1, n1, p2, n2);
}

int
NdbSqlUtil::cmpTimestamp2(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2)
{
  return cmpBinary(info, p1, n1, p2, n2);
}

// like

static const int ndb_wild_prefix = '\\';
static const int ndb_wild_one = '_';
static const int ndb_wild_many = '%';

int
NdbSqlUtil::likeChar(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2)
{
  const char* v1 = (const char*)p1;
  const char* v2 = (const char*)p2;
  CHARSET_INFO* cs = (CHARSET_INFO*)(info);
  // strip end spaces to match (incorrect) MySQL behaviour
  n1 = (unsigned)(*cs->cset->lengthsp)(cs, v1, n1);
  int k = (*cs->coll->wildcmp)(cs, v1, v1 + n1, v2, v2 + n2, ndb_wild_prefix, ndb_wild_one, ndb_wild_many);
  return k == 0 ? 0 : +1;
}

int
NdbSqlUtil::likeBinary(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2)
{
  assert(info == 0);
  return likeChar(&my_charset_bin, p1, n1, p2, n2);
}

int
NdbSqlUtil::likeVarchar(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2)
{
  const unsigned lb = 1;
  if (n1 >= lb) {
    const uchar* v1 = (const uchar*)p1;
    const uchar* v2 = (const uchar*)p2;
    unsigned m1 = *v1;
    unsigned m2 = n2;
    if (lb + m1 <= n1) {
      const char* w1 = (const char*)v1 + lb;
      const char* w2 = (const char*)v2;
      CHARSET_INFO* cs = (CHARSET_INFO*)(info);
      int k = (*cs->coll->wildcmp)(cs, w1, w1 + m1, w2, w2 + m2, ndb_wild_prefix, ndb_wild_one, ndb_wild_many);
      return k == 0 ? 0 : +1;
    }
  }
  return -1;
}

int
NdbSqlUtil::likeVarbinary(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2)
{
  assert(info == 0);
  return likeVarchar(&my_charset_bin, p1, n1, p2, n2);
}

int
NdbSqlUtil::likeLongvarchar(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2)
{
  const unsigned lb = 2;
  if (n1 >= lb) {
    const uchar* v1 = (const uchar*)p1;
    const uchar* v2 = (const uchar*)p2;
    unsigned m1 = uint2korr(v1);
    unsigned m2 = n2;
    if (lb + m1 <= n1) {
      const char* w1 = (const char*)v1 + lb;
      const char* w2 = (const char*)v2;
      CHARSET_INFO* cs = (CHARSET_INFO*)(info);
      int k = (*cs->coll->wildcmp)(cs, w1, w1 + m1, w2, w2 + m2, ndb_wild_prefix, ndb_wild_one, ndb_wild_many);
      return k == 0 ? 0 : +1;
    }
  }
  return -1;
}

int
NdbSqlUtil::likeLongvarbinary(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2)
{
  assert(info == 0);
  return likeLongvarchar(&my_charset_bin, p1, n1, p2, n2);
}


// mask Functions

int
NdbSqlUtil::maskBit(const void* data, unsigned dataLen, const void* mask, unsigned maskLen, bool cmpZero)
{
  /* Bitfields are stored in word oriented form, so we must compare them in that
   * style as well
   * It is assumed that insignificant bits (but not bytes) in the passed values
   * are zeroed
   */
  const Uint32 bytes = MIN(dataLen, maskLen);
  Uint32 words = (bytes + 3) >> 2;

  /* Don't expect either value to be length zero */
  assert(words);

  /* Check ptr alignment */
  if (unlikely(((((UintPtr)data) & 3) != 0) ||
               ((((UintPtr)mask) & 3) != 0)))
  {
    Uint32 copydata[ MAX_TUPLE_SIZE_IN_WORDS ];
    Uint32 copymask[ MAX_TUPLE_SIZE_IN_WORDS ];
    memcpy(copydata, data, words << 2);
    memcpy(copymask, mask, words << 2);

    return maskBit(data, bytes, mask, bytes, cmpZero);
  }

  const Uint32* wdata= (const Uint32*) data;
  const Uint32* wmask= (const Uint32*) mask;

  if (cmpZero)
  {
    while (--words)
    {
      if ((*(wdata++) & *(wmask++)) != 0)
        return 1;
    }
    
    /* For the last word, we mask out any insignificant bytes */
    const Uint32 sigBytes= bytes & 3; // 0..3; 0 == all bytes significant
    const Uint32 comparisonMask= sigBytes?
      (1 << (sigBytes *8)) -1 :
      ~0;
    const Uint32 lastDataWord= *wdata & comparisonMask;
    const Uint32 lastMaskWord= *wmask & comparisonMask;
    
    if ((lastDataWord & lastMaskWord) != 0)
      return 1;

    return 0;
  }
  else
  {
    while (--words)
    {
      if ((*(wdata++) & *wmask) != *wmask)
        return 1;
      
      wmask++;
    }

    /* For the last word, we mask out any insignificant bytes */
    const Uint32 sigBytes= bytes & 3; // 0..3; 0 == all bytes significant
    const Uint32 comparisonMask= sigBytes?
      (1 << (sigBytes *8)) -1 :
      ~0;
    const Uint32 lastDataWord= *wdata & comparisonMask;
    const Uint32 lastMaskWord= *wmask & comparisonMask;
    
    if ((lastDataWord & lastMaskWord) != lastMaskWord)
      return 1;

    return 0;
  }
}


// check charset

uint
NdbSqlUtil::check_column_for_pk(Uint32 typeId, const void* info)
{
  const Type& type = getType(typeId);
  switch (type.m_typeId) {
  case Type::Char:
  case Type::Varchar:
  case Type::Longvarchar:
    {
      const CHARSET_INFO *cs = (const CHARSET_INFO*)info;
      if(cs != 0 &&
         cs->cset != 0 &&
         cs->coll != 0)
      {
	/**
         * Check that we can produce a hash value
         * - NO_PAD collations use the builtin hash_sort function
         *   creating a single 'ulong' value.
         */
        if (cs->pad_attribute == NO_PAD)
        {
          if (cs->coll->hash_sort != NULL)
            return 0;
        }
        /**
         * 'Old' NO_PAD collations will 'multiply' the size of the
         * frm'ed result. Check that it is within supported limits.
         */
        else if (cs->strxfrm_multiply > 0 &&
                 cs->strxfrm_multiply <= MAX_XFRM_MULTIPLY)
        {
          return 0;
        }
      }
      // Fall through; can't 'hash' this charset. 
      return 743;
    }
    break;
  case Type::Undefined:
  case Type::Blob:
  case Type::Text:
  case Type::Bit:
    break;
  default:
    return 0;
  }
  return 906;
}

uint
NdbSqlUtil::check_column_for_hash_index(Uint32 typeId, const void* info)
{
  return check_column_for_pk(typeId, info);
}

uint
NdbSqlUtil::check_column_for_ordered_index(Uint32 typeId, const void* info)
{
  const Type& type = getType(typeId);
  if (type.m_cmp == NULL)
    return false;
  switch (type.m_typeId) {
  case Type::Char:
  case Type::Varchar:
  case Type::Longvarchar:
    {
      // Note: Only strnncollsp used for compare - no strnxfrm! 
      const CHARSET_INFO *cs = (const CHARSET_INFO*)info;
      if (cs != 0 &&
          cs->cset != 0 &&
          cs->coll != 0 &&
          cs->coll->strnncollsp != 0)
        return 0;
      else
        return 743;
    }
    break;
  case Type::Undefined:
  case Type::Blob:
  case Type::Text:
  case Type::Bit:       // can be fixed
    break;
  default:
    return 0;
  }
  return 906;
}

// utilities

bool
NdbSqlUtil::get_var_length(Uint32 typeId, const void* p, unsigned attrlen, Uint32& lb, Uint32& len)
{
  const unsigned char* const src = (const unsigned char*)p;
  switch (typeId) {
  case NdbSqlUtil::Type::Varchar:
  case NdbSqlUtil::Type::Varbinary:
    lb = 1;
    if (attrlen >= lb) {
      len = src[0];
      if (attrlen >= lb + len)
        return true;
    }
    break;
  case NdbSqlUtil::Type::Longvarchar:
  case NdbSqlUtil::Type::Longvarbinary:
    lb = 2;
    if (attrlen >= lb) {
      len = src[0] + (src[1] << 8);
      if (attrlen >= lb + len)
        return true;
    }
    break;
  default:
    lb = 0;
    len = attrlen;
    return true;
    break;
  }
  return false;
}

/**
 * Normalize string for **hashing**. 
 * To compare strings, use the NdbSqlUtil::cmp*() methods.
 *
 * xfrm'ed strings are guaranteed to be binary equal for
 * strings defined as equal by the specified charset collation.
 *
 * However, the opposite is not true: unequal strings may
 * be xfrm'ed into the same binary representation.
 */

/**
 * Backward bug compatible implementation of strnxfrm.
 *
 * Even if bug#7284: 'strnxfrm generates different results for equal strings'
 * is fixed long ago, we still have to keep this method:
 * The suggested fix for that bug was:
 *
 *   'Pad the result not with 0x00 character, but with weight
 *    corresponding to the space character'.
 *
 * However, only PAD SPACE collations do this; NO PAD collations
 * are likely to return a 'dst' not being completely padded
 * unless the MY_STRXFRM_PAD_TO_MAXLEN flag is given.
 *
 * So we still have to handle the 'unlikely' case 'n3 < (int)dstLen'.
 */

/**
 * Used to verify that the strnxfrm is only used for hashing:
 * zero-fill xfrm'ed string, which will give a valid hash pattern,
 * but break any misuse in strings compare
 */
static const bool verify_hash_only_usage = false;

static inline int
strnxfrm_bug7284(const CHARSET_INFO* cs,
                 uchar* dst, unsigned dstLen,
                 const uchar*src, unsigned srcLen)
{
  // strxfrm argument string - returns no error indication
  const int n3 = (int)(*cs->coll->strnxfrm)(cs,
                                dst, dstLen, (uint)dstLen,
                                src, srcLen,
				0);

  if (unlikely(n3 < (int)dstLen))
  {
    unsigned char nsp[20]; // native space char
    unsigned char xsp[20]; // strxfrm-ed space char
#ifdef VM_TRACE
    memset(nsp, 0x1f, sizeof(nsp));
    memset(xsp, 0x1f, sizeof(xsp));
#endif
    // convert from unicode codepoint for space
    const int n1 = (*cs->cset->wc_mb)(cs, (my_wc_t)0x20, nsp, nsp + sizeof(nsp));
    if (n1 <= 0)
      return -1;
    // strxfrm to binary
    const int n2 = (int)(*cs->coll->strnxfrm)(cs, 
                                xsp, sizeof(xsp), (uint)sizeof(xsp),
                                nsp, n1,
				0);
    if (n2 <= 0)
      return -1;

    // pad with strxfrm-ed space chars
    int n4 = n3;
    while (n4 < (int)dstLen) {
      dst[n4] = xsp[(n4 - n3) % n2];
      n4++;
    }
  }

  if (verify_hash_only_usage)
    memset(dst, 0, dstLen);

  // no check for partial last
  return dstLen;
}

int
NdbSqlUtil::strnxfrm_hash(const CHARSET_INFO* cs,
                          Uint32 typeId,
                          uchar* dst, unsigned bufLen,
                          const uchar* src, unsigned srcLen,
                          unsigned maxLen)
{
  /**
   * The NO_PAD Unicode-9.0 collations were introduced in MySQL-9.0.
   * As we dont have to be bug-compatible when these (new) collations
   * are used, we use the hash function provided by the collation
   * directly, and use the calculated hash value as our contribution
   * to the xfrm'ed hash string.
   */
  if (cs->pad_attribute == NO_PAD && cs != &my_charset_bin)
  {
    assert(typeId == NdbSqlUtil::Type::Char ||
           typeId == NdbSqlUtil::Type::Varchar ||
           typeId == NdbSqlUtil::Type::Longvarchar);

    // Fixed length char need trailing spaces to be stripped if NO_PAD
    if (typeId == NdbSqlUtil::Type::Char)
      srcLen = cs->cset->lengthsp(cs, reinterpret_cast<const char*>(src), srcLen);

    // Hash the string using the collations hash function.
    uint64 hash = 0, n2 = 0;
    (*cs->coll->hash_sort)(cs, src, srcLen, &hash, &n2);

    if (verify_hash_only_usage)  //Debug only
      hash = 0;

    // Store the hash as part of the normalized hash string:
    if (likely(sizeof(hash) <= bufLen))
    {
      memcpy(dst, &hash, sizeof(hash));
      return sizeof(hash);
    }
  }
  /**
   * Need to be bug- and feature-compatible with older collations.
   * Produce the fully xfrm'ed and space padded string.
   * May unfortunately become quite huge, adding significant
   * overhead when later md5_hash'ing (the space padding)
   */
  else if (likely(cs->strxfrm_multiply > 0))
  {
    /**
     * Old transformation, pre-8.0 unicode-9.0 charset:
     *
     * Varchar end-spaces are ignored in comparisons.  To get same hash
     * we blank-pad to maximum 'dstLen' via strnxfrm.
     */
    const Uint32 dstLen = cs->strxfrm_multiply * maxLen;

    // Sufficient buffer space should always be provided.
    if (likely(dstLen <= bufLen))
    {
      return strnxfrm_bug7284(cs, dst, dstLen, src, srcLen);
    }
  }

  // Fall through, should never happen
  return -1;
}

/**
 * Get maximum length needed by the xfrm'ed string
 * as produced by strnxfrm_hash().
 *
 *  cs:     The Character set definition
 *  maxLen: The maximim (padded) length of the string
 */
Uint32
NdbSqlUtil::strnxfrm_hash_len(const CHARSET_INFO* cs,
                              unsigned maxLen)
{
  if (cs->pad_attribute == NO_PAD && cs != &my_charset_bin)
  {
    //The hash_sort() value, see strnxfrm_hash
    return sizeof(uint64);
  }
  else if (likely(cs->strxfrm_multiply > 0))
  {
    // The full space-padded string will be produced
    return cs->strxfrm_multiply * maxLen;
  }

  // Fall through, should never happen.
  return 0;
}

#if defined(WORDS_BIGENDIAN) || defined (VM_TRACE)

static
void determineParams(Uint32 typeId,
                     Uint32 typeLog2Size,
                     Uint32 arrayType,
                     Uint32 arraySize,
                     Uint32 dataByteSize,
                     Uint32& convSize,
                     Uint32& convLen)
{
  /* Some types need 'normal behaviour' over-ridden in
   * terms of endian-ness conversions
   */
  convSize = 0;
  convLen = 0;
  switch(typeId)
  {
  case NdbSqlUtil::Type::Datetime:
  {
    // Datetime is stored as 8x8, should be twiddled as 64 bit
    assert(typeLog2Size == 3);
    assert(arraySize == 8);
    assert(dataByteSize == 8);
    convSize = 64;
    convLen = 1;
    break;
  }
  case NdbSqlUtil::Type::Timestamp:
  {
    // Timestamp is stored as 4x8, should be twiddled as 32 bit
    assert(typeLog2Size == 3);
    assert(arraySize == 4);
    assert(dataByteSize == 4);
    convSize = 32;
    convLen = 1;
    break;
  }
  case NdbSqlUtil::Type::Bit:
  {
    // Bit is stored as bits, should be twiddled as 32 bit
    assert(typeLog2Size == 0);
    convSize = 32;
    convLen = (arraySize + 31)/32;
    break;
  }
  case NdbSqlUtil::Type::Blob:
  case NdbSqlUtil::Type::Text:
  {
    if (arrayType == NDB_ARRAYTYPE_FIXED)
    {
      // Length of fixed size blob which is stored in first 64 bit's
      // has to be twiddled, the remaining byte stream left as is
      assert(typeLog2Size == 3);
      assert(arraySize > 8);
      assert(dataByteSize > 8);
      convSize = 64;
      convLen = 1;
      break;
    }
  }
  // Fall through - for Blob v2
  default:
    /* Default determined by meta-info */
    convSize = 1 << typeLog2Size;
    convLen = arraySize;
    break;
  }

  const Uint32 unitBytes = (convSize >> 3);
  
  if (dataByteSize < (unitBytes * convLen))
  {
    /* Actual data is shorter than expected, could
     * be VAR type or bad FIXED data (which some other
     * code should detect + handle)
     * We reduce convLen to avoid trampling
     */
    assert((dataByteSize % unitBytes) == 0);
    convLen = dataByteSize / unitBytes;
  }

  assert(convSize);
  assert(convLen);
  assert(dataByteSize >= (unitBytes * convLen));
}

static
void doConvert(Uint32 convSize,
               Uint32 convLen,
               uchar* data)
{
  switch(convSize)
  {
  case 8:
    /* Nothing to swap */
    break;
  case 16:
  {
    Uint16* ptr = (Uint16*)data;
    for (Uint32 i = 0; i < convLen; i++){
      Uint16 val = 
        ((*ptr & 0xFF00) >> 8) |
        ((*ptr & 0x00FF) << 8);
      *ptr = val;
      ptr++;
    }
    break;
  }
  case 32:
  {
    Uint32* ptr = (Uint32*)data;
    for (Uint32 i = 0; i < convLen; i++){
      Uint32 val = 
        ((*ptr & 0xFF000000) >> 24) |
        ((*ptr & 0x00FF0000) >> 8)  |
        ((*ptr & 0x0000FF00) << 8)  |
        ((*ptr & 0x000000FF) << 24);
      *ptr = val;
      ptr++;
    }
    break;
  }
  case 64:
  {
    Uint64* ptr = (Uint64*)data;
    for (Uint32 i = 0; i < convLen; i++){
      Uint64 val = 
        ((*ptr & (Uint64)0xFF00000000000000LL) >> 56) |
        ((*ptr & (Uint64)0x00FF000000000000LL) >> 40) |
        ((*ptr & (Uint64)0x0000FF0000000000LL) >> 24) |
        ((*ptr & (Uint64)0x000000FF00000000LL) >> 8)  |
        ((*ptr & (Uint64)0x00000000FF000000LL) << 8)  |
        ((*ptr & (Uint64)0x0000000000FF0000LL) << 24) |
        ((*ptr & (Uint64)0x000000000000FF00LL) << 40) |
        ((*ptr & (Uint64)0x00000000000000FFLL) << 56);
      *ptr = val;
      ptr++;
    }
    break;
  }
  default:
    abort();
    break;
  }
}
#endif

/**
 * Convert attribute byte order if necessary
 */
void
NdbSqlUtil::convertByteOrder(Uint32 typeId, 
                             Uint32 typeLog2Size, 
                             Uint32 arrayType, 
                             Uint32 arraySize, 
                             uchar* data,
                             Uint32 dataByteSize)
{
#if defined(WORDS_BIGENDIAN) || defined (VM_TRACE)
  Uint32 convSize;
  Uint32 convLen;
  determineParams(typeId,
                  typeLog2Size,
                  arrayType,
                  arraySize,
                  dataByteSize,
                  convSize,
                  convLen);

  size_t mask = (((size_t) convSize) >> 3) -1; // Bottom 0,1,2,3 bits set
  bool aligned = (((size_t) data) & mask) == 0;
  uchar* dataPtr = data;
  const Uint32 bufSize = (MAX_TUPLE_SIZE_IN_WORDS + 1)/2;
  Uint64 alignedBuf[bufSize];
  
  if (!aligned)
  {
    assert(dataByteSize <= 4 * MAX_TUPLE_SIZE_IN_WORDS);
    memcpy(alignedBuf, data, dataByteSize);
    dataPtr = (uchar*) alignedBuf;
  }

  /* Now have convSize and convLen, do the conversion */
  doConvert(convSize,
            convLen,
            dataPtr);

#ifdef VM_TRACE
#ifndef WORDS_BIGENDIAN
  // VM trace on little-endian performs double-convert
  doConvert(convSize,
            convLen,
            dataPtr);
#endif
#endif

  if (!aligned)
  {
    memcpy(data, alignedBuf, dataByteSize);
  }  
#endif
}

// unpack and pack date/time types

// Year

void
NdbSqlUtil::unpack_year(Year& s, const uchar* d)
{
  s.year = (uint)(1900 + d[0]);
}

void
NdbSqlUtil::pack_year(const Year& s, uchar* d)
{
  d[0] = (uchar)(s.year - 1900);
}

// Date

void
NdbSqlUtil::unpack_date(Date& s, const uchar* d)
{
  uchar b[4];
  memcpy(b, d, 3);
  b[3] = 0;
  uint w = (uint)uint3korr(b);
  s.day = (w & 31);
  w >>= 5;
  s.month = (w & 15);
  w >>= 4;
  s.year = w;
}

void
NdbSqlUtil::pack_date(const Date& s, uchar* d)
{
  uint w = 0;
  w |= s.year;
  w <<= 4;
  w |= s.month;
  w <<= 5;
  w |= s.day;
  int3store(d, w);
}

// Time

void
NdbSqlUtil::unpack_time(Time& s, const uchar* d)
{
  uchar b[4];
  memcpy(b, d, 3);
  b[3] = 0;
  uint w = 0;
  int v = (int)sint3korr(b);
  if (v >= 0)
  {
    s.sign = 1;
    w = (uint)v;
  }
  else
  {
    s.sign = 0;
    w = (uint)(-v);
  }
  const uint f = (uint)100;
  s.second = (w % f);
  w /= f;
  s.minute = (w % f);
  w /= f;
  s.hour = w;
}

void
NdbSqlUtil::pack_time(const Time& s, uchar* d)
{
  const uint f = (uint)100;
  uint w = 0;
  w += s.hour;
  w *= f;
  w += s.minute;
  w *= f;
  w += s.second;
  int v = 0;
  if (s.sign == 1)
  {
    v = (int)w;
  }
  else
  {
    v = (int)w;
    v = -v;
  }
  int3store(d, v);
}

// Datetime

void
NdbSqlUtil::unpack_datetime(Datetime& s, const uchar* d)
{
  uint64 w;
  memcpy(&w, d, 8);
  const uint64 f = (uint64)100;
  s.second = (w % f);
  w /= f;
  s.minute = (w % f);
  w /= f;
  s.hour = (w % f);
  w /= f;
  s.day = (w % f);
  w /= f;
  s.month = (w % f);
  w /= f;
  s.year = (uint)w;
}

void
NdbSqlUtil::pack_datetime(const Datetime& s, uchar* d)
{
  const uint64 f = (uint64)100;
  uint64 w = 0;
  w += s.year;
  w *= f;
  w += s.month;
  w *= f;
  w += s.day;
  w *= f;
  w += s.hour;
  w *= f;
  w += s.minute;
  w *= f;
  w += s.second;
  memcpy(d, &w, 8);
}

// Timestamp

void
NdbSqlUtil::unpack_timestamp(Timestamp& s, const uchar* d)
{
  uint32 w;
  memcpy(&w, d, 4);
  s.second = (uint)w;
}

void
NdbSqlUtil::pack_timestamp(const Timestamp& s, uchar* d)
{
  uint32 w = s.second;
  memcpy(d, &w, 4);
}

// types with fractional seconds

static uint64
unpack_bigendian(const uchar* d, uint len)
{
  assert(len <= 8);
  uint64 val = 0;
  int i = (int)len;
  int s = 0;
  while (i != 0)
  {
    i--;
    uint64 v = d[i];
    val += (v << s);
    s += 8;
  }
  return val;
}

static void
pack_bigendian(uint64 val, uchar* d, uint len)
{
  uchar b[8];
  uint i = 0;
  while (i  < len)
  {
    b[i] = (uchar)(val & 255);
    val >>= 8;
    i++;
  }
  uint j = 0;
  while (i != 0)
  {
    i--;
    d[i] = b[j];
    j++;
  }
}

// Time2 : big-endian time(3 bytes).fraction(0-3 bytes)

void
NdbSqlUtil::unpack_time2(Time2& s, const uchar* d, uint prec)
{
  const uint64 one = (uint64)1;
  uint flen = (1 + prec) / 2;
  uint fbit = 8 * flen;
  uint64 val = unpack_bigendian(&d[0], 3 + flen);
  uint spos = 23 + fbit;
  uint sign = (uint)((val & (one << spos)) >> spos);
  if (sign == 0) // negative
    val = (one << spos) - val;
  uint64 w = (val >> fbit);
  s.second = (uint)(w & 63);
  w >>= 6;
  s.minute = (uint)(w & 63);
  w >>= 6;
  s.hour = (uint)(w & 1023);
  w >>= 10;
  s.interval = (uint)(w & 1);
  w >>= 1;
  s.sign = sign;
  uint f = (uint)(val & ((one << fbit) - 1));
  if (prec % 2 != 0)
    f /= 10;
  s.fraction = f;
}

void
NdbSqlUtil::pack_time2(const Time2& s, uchar* d, uint prec)
{
  const uint64 one = (uint64)1;
  uint flen = (1 + prec) / 2;
  uint fbit = 8 * flen;
  uint spos = 23 + fbit;
  uint64 w = 0;
  w |= s.sign;
  w <<= 1;
  w |= s.interval;
  w <<= 10;
  w |= s.hour;
  w <<= 6;
  w |= s.minute;
  w <<= 6;
  w |= s.second;
  uint f = s.fraction;
  if (prec % 2 != 0)
    f *= 10;
  uint64 val = (w << fbit) | f;
  if (s.sign == 0)
    val = (one << spos) - val;
  pack_bigendian(val, &d[0], 3 + flen);
}

// Datetime2 : big-endian date(5 bytes).fraction(0-3 bytes)

void
NdbSqlUtil::unpack_datetime2(Datetime2& s, const uchar* d, uint prec)
{
  const uint64 one = (uint64)1;
  uint flen = (1 + prec) / 2;
  uint fbit = 8 * flen;
  uint64 val = unpack_bigendian(&d[0], 5 + flen);
  uint spos = 39 + fbit;
  uint sign = (uint)((val & (one << spos)) >> spos);
  if (sign == 0) // negative
    val = (one << spos) - val;
  uint64 w = (val >> fbit);
  s.second = (uint)(w & 63);
  w >>= 6;
  s.minute = (uint)(w & 63);
  w >>= 6;
  s.hour = (uint)(w & 31);
  w >>= 5;
  s.day = (uint)(w & 31);
  w >>= 5;
  uint year_month = (uint)(w & ((1 << 17) - 1));
  s.month = year_month % 13;
  s.year = year_month / 13;
  w >>= 17;
  s.sign = sign;
  uint f = (uint)(val & ((one << fbit) - 1));
  if (prec % 2 != 0)
    f /= 10;
  s.fraction = f;
}

void
NdbSqlUtil::pack_datetime2(const Datetime2& s, uchar* d, uint prec)
{
  const uint64 one = (uint64)1;
  uint flen = (1 + prec) / 2;
  uint fbit = 8 * flen;
  uint spos = 39 + fbit;
  uint64 w = 0;
  w |= s.sign;
  w <<= 17;
  w |= (s.year * 13 + s.month);
  w <<= 5;
  w |= s.day;
  w <<= 5;
  w |= s.hour;
  w <<= 6;
  w |= s.minute;
  w <<= 6;
  w |= s.second;
  uint f = s.fraction;
  if (prec % 2 != 0)
    f *= 10;
  uint64 val = (w << fbit) | f;
  if (s.sign == 0)
    val = (one << spos) - val;
  pack_bigendian(val, &d[0], 5 + flen);
}

// Timestamp2 : big-endian non-negative unix time

void
NdbSqlUtil::unpack_timestamp2(Timestamp2& s, const uchar* d, uint prec)
{
  uint flen = (1 + prec) / 2;
  uint w = (uint)unpack_bigendian(&d[0], 4);
  s.second = w;
  uint f = (uint)unpack_bigendian(&d[4], flen);
  if (prec % 2 != 0)
    f /= 10;
  s.fraction = f;
}

void
NdbSqlUtil::pack_timestamp2(const Timestamp2& s, uchar* d, uint prec)
{
  uint flen = (1 + prec) / 2;
  uint w = s.second;
  pack_bigendian(w, &d[0], 4);
  uint f = s.fraction;
  if (prec % 2 != 0)
    f *= 10;
  pack_bigendian(f, &d[4], flen);
}

#ifdef TEST_NDBSQLUTIL

/*
 * Before using the pack/unpack test one must verify correctness
 * of unpack methods via sql and ndb_select_all.  Otherwise we are
 * just testing pack/unpack of some fantasy formats.
 */

#include <util/NdbTap.hpp>
#include <ndb_rand.h>
#include <NdbOut.hpp>
#include <NdbEnv.h>
#include <NdbHost.h>

#define chk1(x) \
  do { if (x) break; ndbout << "line " << __LINE__ << ": " << #x << endl; \
       require(false); } while (0)

#define lln(x, n) \
  do { if (verbose < n) break; ndbout << x << endl; } while (0)

#define ll0(x) lln(x, 0)
#define ll1(x) lln(x, 1)

static int seed = -1; // random
static int loops = 10;
static int subloops = 100000;
static int verbose = 0;

static const uint maxprec = 6;

static uint maxfrac[1 + maxprec] = {
  0, 9, 99, 999, 9999, 99999, 999999
};

static uint
getrand(uint m)
{
  assert(m != 0);
  uint n = ndb_rand();
  return n % m;
}

static uint
getrand(uint m1, uint  m2, bool& nz)
{
  assert(m1 <= m2);
  // test zero and min and max more often
  uint n = 0;
  switch (getrand(10)) {
  case 0:
    n = 0;
    break;
  case 1:
    n = m1;
    break;
  case 2:
    n = m2;
    break;
  default:
    n =  m1 + getrand(m2 - m1 + 1);
    break;
  }
  if (n != 0)
    nz = true;
  return n;
}

static void
cmpyear(const NdbSqlUtil::Year& s1,
        const NdbSqlUtil::Year& s2)
{
  chk1(s1.year == s2.year);
}

static void
testyear()
{
  NdbSqlUtil::Year s1;
  NdbSqlUtil::Year s2;
  uchar d1[20];
  uchar d2[20];
  memset(&s1, 0x1f, sizeof(s1));
  memset(&s2, 0x1f, sizeof(s2));
  memset(d1, 0x1f, sizeof(d1));
  memset(d2, 0x1f, sizeof(d2));
  bool nz = false;
  s1.year = getrand(1900, 2155, nz);
  if (!nz)
    s1.year = 1900;
  NdbSqlUtil::pack_year(s1, d1);
  NdbSqlUtil::unpack_year(s2, d1);
  cmpyear(s1, s2);
  NdbSqlUtil::pack_year(s2, d2);
  chk1(memcmp(d1, d2, sizeof(d1)) == 0);
}

static void
loopyear()
{
  for (int i = 0; i < subloops; i++) {
    testyear();
  }
}

static void
cmpdate(const NdbSqlUtil::Date& s1,
        const NdbSqlUtil::Date& s2)
{
  chk1(s1.year == s2.year);
  chk1(s1.month == s2.month);
  chk1(s1.day == s2.day);
}

static void
testdate()
{
  NdbSqlUtil::Date s1;
  NdbSqlUtil::Date s2;
  uchar d1[20];
  uchar d2[20];
  memset(&s1, 0x1f, sizeof(s1));
  memset(&s2, 0x1f, sizeof(s2));
  memset(d1, 0x1f, sizeof(d1));
  memset(d2, 0x1f, sizeof(d2));
  bool nz = false;
  s1.year = getrand(1000, 9999, nz);
  s1.month = getrand(1, 12, nz);
  s1.day = getrand(1, 31, nz);
  if (!nz)
    s1.year = 1900;
  NdbSqlUtil::pack_date(s1, d1);
  NdbSqlUtil::unpack_date(s2, d1);
  cmpdate(s1, s2);
  NdbSqlUtil::pack_date(s2, d2);
  chk1(memcmp(d1, d2, sizeof(d1)) == 0);
}

static void
loopdate()
{
  for (int i = 0; i < subloops; i++) {
    testdate();
  }
}

static void
cmptime(const NdbSqlUtil::Time& s1,
        const NdbSqlUtil::Time& s2)
{
  chk1(s1.sign == s2.sign);
  chk1(s1.hour == s2.hour);
  chk1(s1.minute == s2.minute);
  chk1(s1.second == s2.second);
}

static void
testtime()
{
  NdbSqlUtil::Time s1;
  NdbSqlUtil::Time s2;
  uchar d1[20];
  uchar d2[20];
  memset(&s1, 0x1f, sizeof(s1));
  memset(&s2, 0x1f, sizeof(s2));
  memset(d1, 0x1f, sizeof(d1));
  memset(d2, 0x1f, sizeof(d2));
  bool nz = false;
  s1.sign = getrand(0, 1, nz);
  s1.hour = getrand(0, 838, nz);
  s1.minute = getrand(0, 59, nz);
  s1.second = getrand(0, 59, nz);
  if (!nz)
    s1.sign = 1;
  NdbSqlUtil::pack_time(s1, d1);
  NdbSqlUtil::unpack_time(s2, d1);
  cmptime(s1, s2);
  NdbSqlUtil::pack_time(s2, d2);
  chk1(memcmp(d1, d2, sizeof(d1)) == 0);
}

static void
looptime()
{
  for (int i = 0; i < subloops; i++) {
    testtime();
  }
}

static void
cmpdatetime(const NdbSqlUtil::Datetime& s1,
            const NdbSqlUtil::Datetime& s2)
{
  chk1(s1.year == s2.year);
  chk1(s1.month == s2.month);
  chk1(s1.day == s2.day);
  chk1(s1.hour == s2.hour);
  chk1(s1.minute == s2.minute);
  chk1(s1.second == s2.second);
}

static void
testdatetime()
{
  NdbSqlUtil::Datetime s1;
  NdbSqlUtil::Datetime s2;
  uchar d1[20];
  uchar d2[20];
  memset(&s1, 0x1f, sizeof(s1));
  memset(&s2, 0x1f, sizeof(s2));
  memset(d1, 0x1f, sizeof(d1));
  memset(d2, 0x1f, sizeof(d2));
  bool nz = false;
  s1.year = getrand(1000, 9999, nz);
  s1.month = getrand(1, 12, nz);
  s1.day = getrand(1, 31, nz);
  s1.hour = getrand(0, 23, nz);
  s1.minute = getrand(0, 59, nz);
  s1.second = getrand(0, 59, nz);
  NdbSqlUtil::pack_datetime(s1, d1);
  NdbSqlUtil::unpack_datetime(s2, d1);
  cmpdatetime(s1, s2);
  NdbSqlUtil::pack_datetime(s2, d2);
  chk1(memcmp(d1, d2, sizeof(d1)) == 0);
}

static void
loopdatetime()
{
  for (int i = 0; i < subloops; i++) {
    testdatetime();
  }
}

static void
cmptimestamp(const NdbSqlUtil::Timestamp& s1,
             const NdbSqlUtil::Timestamp& s2)
{
  chk1(s1.second == s2.second);
}

static void
testtimestamp()
{
  NdbSqlUtil::Timestamp s1;
  NdbSqlUtil::Timestamp s2;
  uchar d1[20];
  uchar d2[20];
  memset(&s1, 0x1f, sizeof(s1));
  memset(&s2, 0x1f, sizeof(s2));
  memset(d1, 0x1f, sizeof(d1));
  memset(d2, 0x1f, sizeof(d2));
  bool nz = false;
  s1.second = getrand(0, 59, nz);
  NdbSqlUtil::pack_timestamp(s1, d1);
  NdbSqlUtil::unpack_timestamp(s2, d1);
  cmptimestamp(s1, s2);
  NdbSqlUtil::pack_timestamp(s2, d2);
  chk1(memcmp(d1, d2, sizeof(d1)) == 0);
}

static void
looptimestamp()
{
  for (int i = 0; i < subloops; i++) {
    testtimestamp();
  }
}

static void
cmptime2(const NdbSqlUtil::Time2& s1,
         const NdbSqlUtil::Time2& s2)
{
  chk1(s1.sign == s2.sign);
  chk1(s1.interval == s2.interval);
  chk1(s1.hour == s2.hour);
  chk1(s1.minute == s2.minute);
  chk1(s1.second == s2.second);
  chk1(s1.fraction == s2.fraction);
}

static void
testtime2(uint prec)
{
  NdbSqlUtil::Time2 s1;
  NdbSqlUtil::Time2 s2;
  uchar d1[20];
  uchar d2[20];
  memset(&s1, 0x1f, sizeof(s1));
  memset(&s2, 0x1f, sizeof(s2));
  memset(d1, 0x1f, sizeof(d1));
  memset(d2, 0x1f, sizeof(d2));
  bool nz = false;
  s1.sign = getrand(0, 1, nz);
  s1.interval = 0;
  s1.hour = getrand(0, 838, nz);
  s1.minute = getrand(0, 59, nz);
  s1.second = getrand(0, 59, nz);
  s1.fraction = getrand(0, maxfrac[prec], nz);
  if (!nz)
    s1.sign = 1;
  NdbSqlUtil::pack_time2(s1, d1, prec);
  NdbSqlUtil::unpack_time2(s2, d1, prec);
  cmptime2(s1, s2);
  NdbSqlUtil::pack_time2(s2, d2, prec);
  chk1(memcmp(d1, d2, sizeof(d1)) == 0);
}

static void
looptime2()
{
  for (uint prec = 0; prec <= maxprec; prec++) {
    for (int i = 0; i < subloops; i++) {
      testtime2(prec);
    }
  }
}

static void
cmpdatetime2(const NdbSqlUtil::Datetime2& s1,
             const NdbSqlUtil::Datetime2& s2)
{
  chk1(s1.sign == s2.sign);
  chk1(s1.year == s2.year);
  chk1(s1.month == s2.month);
  chk1(s1.day == s2.day);
  chk1(s1.hour == s2.hour);
  chk1(s1.minute == s2.minute);
  chk1(s1.second == s2.second);
  chk1(s1.fraction == s2.fraction);
}

static void
testdatetime2(uint prec)
{
  NdbSqlUtil::Datetime2 s1;
  NdbSqlUtil::Datetime2 s2;
  uchar d1[20];
  uchar d2[20];
  memset(&s1, 0x1f, sizeof(s1));
  memset(&s2, 0x1f, sizeof(s2));
  memset(d1, 0x1f, sizeof(d1));
  memset(d2, 0x1f, sizeof(d2));
  bool nz = false;
  s1.sign = getrand(0, 1, nz); // negative not yet in MySQL
  s1.year = getrand(0, 9999, nz);
  s1.month = getrand(1, 12, nz);
  s1.day = getrand(1, 31, nz);
  s1.hour = getrand(0, 23, nz);
  s1.minute = getrand(0, 59, nz);
  s1.second = getrand(0, 59, nz);
  s1.fraction = getrand(0, maxfrac[prec], nz);
  if (!nz)
    s1.sign = 1;
  NdbSqlUtil::pack_datetime2(s1, d1, prec);
  NdbSqlUtil::unpack_datetime2(s2, d1, prec);
  cmpdatetime2(s1, s2);
  NdbSqlUtil::pack_datetime2(s2, d2, prec);
  chk1(memcmp(d1, d2, sizeof(d1)) == 0);
}

static void
loopdatetime2()
{
  for (uint prec = 0; prec <= maxprec; prec++) {
    for (int i = 0; i < subloops; i++) {
      testdatetime2(prec);
    }
  }
}

static void
cmptimestamp2(const NdbSqlUtil::Timestamp2& s1,
              const NdbSqlUtil::Timestamp2& s2)
{
  chk1(s1.second == s2.second);
  chk1(s1.fraction == s2.fraction);
}

static void
testtimestamp2(uint prec)
{
  NdbSqlUtil::Timestamp2 s1;
  NdbSqlUtil::Timestamp2 s2;
  uchar d1[20];
  uchar d2[20];
  memset(&s1, 0x1f, sizeof(s1));
  memset(&s2, 0x1f, sizeof(s2));
  memset(d1, 0x1f, sizeof(d1));
  memset(d2, 0x1f, sizeof(d2));
  bool nz = false;
  s1.second = getrand(0, 59, nz);
  s1.fraction = getrand(0, maxfrac[prec], nz);
  NdbSqlUtil::pack_timestamp2(s1, d1, prec);
  NdbSqlUtil::unpack_timestamp2(s2, d1, prec);
  cmptimestamp2(s1, s2);
  NdbSqlUtil::pack_timestamp2(s2, d2, prec);
  chk1(memcmp(d1, d2, sizeof(d1)) == 0);
}

static void
looptimestamp2()
{
  for (uint prec = 0; prec <= maxprec; prec++) {
    for (int i = 0; i < subloops; i++) {
      testtimestamp2(prec);
    }
  }
}

static void
testrun()
{
  loopyear();
  loopdate();
  looptime();
  loopdatetime();
  looptimestamp();
  looptime2();
  loopdatetime2();
  looptimestamp2();
}

static int
testmain()
{
  ndb_init();
#ifdef NDB_USE_GET_ENV
  struct { const char* env; int* val; } opt[] = {
    { "TEST_NDB_SQL_UTIL_SEED", &seed },
    { "TEST_NDB_SQL_UTIL_LOOPS", &loops },
    { "TEST_NDB_SQL_UTIL_VERBOSE", &verbose },
    { 0, 0 }
  };
  for (int i = 0; opt[i].env != 0; i++) {
    const char* p = NdbEnv_GetEnv(opt[i].env, (char*)0, 0);
    if (p != 0)
      *opt[i].val = atoi(p);
  }
#endif
#ifdef VM_TRACE
  signal(SIGABRT, SIG_DFL);
#endif
  if (seed == 0)
    ll0("random seed: loop number");
  else {
    if (seed < 0)
      seed = NdbHost_GetProcessId();
    ll0("random seed " << seed);
    ndb_srand(seed);
  }
  for (int i = 0; i < loops; i++) {
    ll0("loop:" << i << "/" << loops);
    if (seed == 0)
      ndb_srand(seed);
    testrun(); // aborts on any error
  }
  ll0("passed");
  return 0;
}

TAPTEST(NdbSqlUtil)
{
  int ret = testmain();
  return (ret == 0);
}

#endif
