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

#include <NdbSqlUtil.hpp>

int
NdbSqlUtil::char_compare(const char* s1, unsigned n1,
                         const char* s2, unsigned n2, bool padded)
{
  int c1 = 0;
  int c2 = 0;
  unsigned i = 0;
  while (i < n1 || i < n2) {
    c1 = i < n1 ? s1[i] : padded ? 0x20 : 0;
    c2 = i < n2 ? s2[i] : padded ? 0x20 : 0;
    if (c1 != c2)
      break;
    i++;
  }
  return c1 - c2;
}

bool
NdbSqlUtil::char_like(const char* s1, unsigned n1,
                      const char* s2, unsigned n2, bool padded)
{
  int c1 = 0;
  int c2 = 0;
  unsigned i1 = 0;
  unsigned i2 = 0;
  while (i1 < n1 || i2 < n2) {
    c1 = i1 < n1 ? s1[i1] : padded ? 0x20 : 0;
    c2 = i2 < n2 ? s2[i2] : padded ? 0x20 : 0;
    if (c2 == '%') {
      while (i2 + 1 < n2 && s2[i2 + 1] == '%') {
        i2++;
      }
      unsigned m = 0;
      while (m <= n1 - i1) {
        if (char_like(s1 + i1 + m, n1 -i1 - m,
                      s2 + i2 + 1, n2 - i2 - 1, padded))
        return true;
        m++;
      }
      return false;
    }
    if (c2 == '_') {
      if (c1 == 0)
        return false;
    } else {
      if (c1 != c2)
        return false;
    }
    i1++;
    i2++;
  }
  return i1 == n2 && i2 == n2;
}

/**
 * Data types.
 */

const NdbSqlUtil::Type
NdbSqlUtil::m_typeList[] = {
  {
    Type::Undefined,
    NULL
  },
  {
    Type::Tinyint,
    cmpTinyint
  },
  {
    Type::Tinyunsigned,
    cmpTinyunsigned
  },
  {
    Type::Smallint,
    cmpSmallint
  },
  {
    Type::Smallunsigned,
    cmpSmallunsigned
  },
  {
    Type::Mediumint,
    cmpMediumint
  },
  {
    Type::Mediumunsigned,
    cmpMediumunsigned
  },
  {
    Type::Int,
    cmpInt
  },
  {
    Type::Unsigned,
    cmpUnsigned
  },
  {
    Type::Bigint,
    cmpBigint
  },
  {
    Type::Bigunsigned,
    cmpBigunsigned
  },
  {
    Type::Float,
    cmpFloat
  },
  {
    Type::Double,
    cmpDouble
  },
  {
    Type::Decimal,
    NULL  // cmpDecimal
  },
  {
    Type::Char,
    cmpChar
  },
  {
    Type::Varchar,
    cmpVarchar
  },
  {
    Type::Binary,
    cmpBinary
  },
  {
    Type::Varbinary,
    cmpVarbinary
  },
  {
    Type::Datetime,
    cmpDatetime
  },
  {
    Type::Timespec,
    cmpTimespec
  },
  {
    Type::Blob,
    cmpBlob
  },
  {
    Type::Text,
    cmpText
  }
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

// compare

int
NdbSqlUtil::cmpTinyint(const Uint32* p1, const Uint32* p2, Uint32 full, Uint32 size)
{
  assert(full >= size && size > 0);
  union { Uint32 p[1]; Int8 v; } u1, u2;
  u1.p[0] = p1[0];
  u2.p[0] = p2[0];
  if (u1.v < u2.v)
    return -1;
  if (u1.v > u2.v)
    return +1;
  return 0;
}

int
NdbSqlUtil::cmpTinyunsigned(const Uint32* p1, const Uint32* p2, Uint32 full, Uint32 size)
{
  assert(full >= size && size > 0);
  union { Uint32 p[1]; Uint8 v; } u1, u2;
  u1.p[0] = p1[0];
  u2.p[0] = p2[0];
  if (u1.v < u2.v)
    return -1;
  if (u1.v > u2.v)
    return +1;
  return 0;
}

int
NdbSqlUtil::cmpSmallint(const Uint32* p1, const Uint32* p2, Uint32 full, Uint32 size)
{
  assert(full >= size && size > 0);
  union { Uint32 p[1]; Int16 v; } u1, u2;
  u1.p[0] = p1[0];
  u2.p[0] = p2[0];
  if (u1.v < u2.v)
    return -1;
  if (u1.v > u2.v)
    return +1;
  return 0;
}

int
NdbSqlUtil::cmpSmallunsigned(const Uint32* p1, const Uint32* p2, Uint32 full, Uint32 size)
{
  assert(full >= size && size > 0);
  union { Uint32 p[1]; Uint16 v; } u1, u2;
  u1.p[0] = p1[0];
  u2.p[0] = p2[0];
  if (u1.v < u2.v)
    return -1;
  if (u1.v > u2.v)
    return +1;
  return 0;
}

int
NdbSqlUtil::cmpMediumint(const Uint32* p1, const Uint32* p2, Uint32 full, Uint32 size)
{
  assert(full >= size && size > 0);
  union { const Uint32* p; const unsigned char* v; } u1, u2;
  u1.p = p1;
  u2.p = p2;
  Int32 v1 = sint3korr(u1.v);
  Int32 v2 = sint3korr(u2.v);
  if (v1 < v2)
    return -1;
  if (v1 > v2)
    return +1;
  return 0;
}

int
NdbSqlUtil::cmpMediumunsigned(const Uint32* p1, const Uint32* p2, Uint32 full, Uint32 size)
{
  assert(full >= size && size > 0);
  union { const Uint32* p; const unsigned char* v; } u1, u2;
  u1.p = p1;
  u2.p = p2;
  Uint32 v1 = uint3korr(u1.v);
  Uint32 v2 = uint3korr(u2.v);
  if (v1 < v2)
    return -1;
  if (v1 > v2)
    return +1;
  return 0;
}

int
NdbSqlUtil::cmpInt(const Uint32* p1, const Uint32* p2, Uint32 full, Uint32 size)
{
  assert(full >= size && size > 0);
  union { Uint32 p[1]; Int32 v; } u1, u2;
  u1.p[0] = p1[0];
  u2.p[0] = p2[0];
  if (u1.v < u2.v)
    return -1;
  if (u1.v > u2.v)
    return +1;
  return 0;
}

int
NdbSqlUtil::cmpUnsigned(const Uint32* p1, const Uint32* p2, Uint32 full, Uint32 size)
{
  assert(full >= size && size > 0);
  union { Uint32 p[1]; Uint32 v; } u1, u2;
  u1.v = p1[0];
  u2.v = p2[0];
  if (u1.v < u2.v)
    return -1;
  if (u1.v > u2.v)
    return +1;
  return 0;
}

int
NdbSqlUtil::cmpBigint(const Uint32* p1, const Uint32* p2, Uint32 full, Uint32 size)
{
  assert(full >= size && size > 0);
  if (size >= 2) {
    union { Uint32 p[2]; Int64 v; } u1, u2;
    u1.p[0] = p1[0];
    u1.p[1] = p1[1];
    u2.p[0] = p2[0];
    u2.p[1] = p2[1];
    if (u1.v < u2.v)
      return -1;
    if (u1.v > u2.v)
      return +1;
    return 0;
  }
  return CmpUnknown;
}

int
NdbSqlUtil::cmpBigunsigned(const Uint32* p1, const Uint32* p2, Uint32 full, Uint32 size)
{
  assert(full >= size && size > 0);
  if (size >= 2) {
    union { Uint32 p[2]; Uint64 v; } u1, u2;
    u1.p[0] = p1[0];
    u1.p[1] = p1[1];
    u2.p[0] = p2[0];
    u2.p[1] = p2[1];
    if (u1.v < u2.v)
      return -1;
    if (u1.v > u2.v)
      return +1;
    return 0;
  }
  return CmpUnknown;
}

int
NdbSqlUtil::cmpFloat(const Uint32* p1, const Uint32* p2, Uint32 full, Uint32 size)
{
  assert(full >= size && size > 0);
  union { Uint32 p[1]; float v; } u1, u2;
  u1.p[0] = p1[0];
  u2.p[0] = p2[0];
  // no format check
  if (u1.v < u2.v)
    return -1;
  if (u1.v > u2.v)
    return +1;
  return 0;
}

int
NdbSqlUtil::cmpDouble(const Uint32* p1, const Uint32* p2, Uint32 full, Uint32 size)
{
  assert(full >= size && size > 0);
  if (size >= 2) {
    union { Uint32 p[2]; double v; } u1, u2;
    u1.p[0] = p1[0];
    u1.p[1] = p1[1];
    u2.p[0] = p2[0];
    u2.p[1] = p2[1];
    // no format check
    if (u1.v < u2.v)
      return -1;
    if (u1.v > u2.v)
      return +1;
    return 0;
  }
  return CmpUnknown;
}

int
NdbSqlUtil::cmpDecimal(const Uint32* p1, const Uint32* p2, Uint32 full, Uint32 size)
{
  assert(full >= size && size > 0);
  // not used by MySQL or NDB
  assert(false);
  return 0;
}

int
NdbSqlUtil::cmpChar(const Uint32* p1, const Uint32* p2, Uint32 full, Uint32 size)
{
  assert(full >= size && size > 0);
  /*
   * Char is blank-padded to length and null-padded to word size.  There
   * is no terminator so we compare the full values.
   */
  union { const Uint32* p; const char* v; } u1, u2;
  u1.p = p1;
  u2.p = p2;
  int k = memcmp(u1.v, u2.v, size << 2);
  return k < 0 ? -1 : k > 0 ? +1 : full == size ? 0 : CmpUnknown;
}

int
NdbSqlUtil::cmpVarchar(const Uint32* p1, const Uint32* p2, Uint32 full, Uint32 size)
{
  assert(full >= size && size > 0);
  /*
   * Varchar is not allowed to contain a null byte and the value is
   * null-padded.  Therefore comparison does not need to use the length.
   */
  union { const Uint32* p; const char* v; } u1, u2;
  u1.p = p1;
  u2.p = p2;
  // skip length in first 2 bytes
  int k = strncmp(u1.v + 2, u2.v + 2, (size << 2) - 2);
  return k < 0 ? -1 : k > 0 ? +1 : full == size ? 0 : CmpUnknown;
}

int
NdbSqlUtil::cmpBinary(const Uint32* p1, const Uint32* p2, Uint32 full, Uint32 size)
{
  assert(full >= size && size > 0);
  /*
   * Binary data of full length.  Compare bytewise.
   */
  union { const Uint32* p; const unsigned char* v; } u1, u2;
  u1.p = p1;
  u2.p = p2;
  int k = memcmp(u1.v, u2.v, size << 2);
  return k < 0 ? -1 : k > 0 ? +1 : full == size ? 0 : CmpUnknown;
}

int
NdbSqlUtil::cmpVarbinary(const Uint32* p1, const Uint32* p2, Uint32 full, Uint32 size)
{
  assert(full >= size && size > 0);
  /*
   * Binary data of variable length padded with nulls.  The comparison
   * does not need to use the length.
   */
  union { const Uint32* p; const unsigned char* v; } u1, u2;
  u1.p = p1;
  u2.p = p2;
  // skip length in first 2 bytes
  int k = memcmp(u1.v + 2, u2.v + 2, (size << 2) - 2);
  return k < 0 ? -1 : k > 0 ? +1 : full == size ? 0 : CmpUnknown;
}

int
NdbSqlUtil::cmpDatetime(const Uint32* p1, const Uint32* p2, Uint32 full, Uint32 size)
{
  assert(full >= size && size > 0);
  /*
   * Datetime is CC YY MM DD hh mm ss \0
   */
  union { const Uint32* p; const unsigned char* v; } u1, u2;
  u1.p = p1;
  u2.p = p2;
  // no format check
  int k = memcmp(u1.v, u2.v, 4);
  if (k != 0)
    return k < 0 ? -1 : +1;
  if (size >= 2) {
    k = memcmp(u1.v + 4, u2.v + 4, 4);
    return k < 0 ? -1 : k > 0 ? +1 : 0;
  }
  return CmpUnknown;
}

int
NdbSqlUtil::cmpTimespec(const Uint32* p1, const Uint32* p2, Uint32 full, Uint32 size)
{
  assert(full >= size && size > 0);
  /*
   * Timespec is CC YY MM DD hh mm ss \0 NN NN NN NN
   */
  union { const Uint32* p; const unsigned char* v; } u1, u2;
  u1.p = p1;
  u2.p = p2;
  // no format check
  int k = memcmp(u1.v, u2.v, 4);
  if (k != 0)
    return k < 0 ? -1 : +1;
  if (size >= 2) {
    k = memcmp(u1.v + 4, u2.v + 4, 4);
    if (k != 0)
      return k < 0 ? -1 : +1;
    if (size >= 3) {
      Uint32 n1 = *(const Uint32*)(u1.v + 8);
      Uint32 n2 = *(const Uint32*)(u2.v + 8);
      if (n1 < n2)
        return -1;
      if (n2 > n1)
        return +1;
      return 0;
    }
  }
  return CmpUnknown;
}

int
NdbSqlUtil::cmpBlob(const Uint32* p1, const Uint32* p2, Uint32 full, Uint32 size)
{
  assert(full >= size && size > 0);
  /*
   * Blob comparison is on the inline bytes.  Except for larger header
   * the format is like Varbinary.
   */
  const unsigned head = NDB_BLOB_HEAD_SIZE;
  // skip blob head
  if (size >= head + 1) {
    union { const Uint32* p; const unsigned char* v; } u1, u2;
    u1.p = p1 + head;
    u2.p = p2 + head;
    int k = memcmp(u1.v, u2.v, (size - head) << 2);
    return k < 0 ? -1 : k > 0 ? +1 : full == size ? 0 : CmpUnknown;
  }
  return CmpUnknown;
}

int
NdbSqlUtil::cmpText(const Uint32* p1, const Uint32* p2, Uint32 full, Uint32 size)
{
  assert(full >= size && size > 0);
  /*
   * Text comparison is on the inline bytes.  Except for larger header
   * the format is like Varchar.
   */
  const unsigned head = NDB_BLOB_HEAD_SIZE;
  // skip blob head
  if (size >= head + 1) {
    union { const Uint32* p; const char* v; } u1, u2;
    u1.p = p1 + head;
    u2.p = p2 + head;
    int k = memcmp(u1.v, u2.v, (size - head) << 2);
    return k < 0 ? -1 : k > 0 ? +1 : full == size ? 0 : CmpUnknown;
  }
  return CmpUnknown;
}

// check charset

bool
NdbSqlUtil::usable_in_pk(Uint32 typeId, const void* info)
{
  const Type& type = getType(typeId);
  switch (type.m_typeId) {
  case Type::Undefined:
    break;
  case Type::Char:
    {
      const CHARSET_INFO *cs = (const CHARSET_INFO*)info;
      return
        cs != 0 &&
        cs->cset != 0 &&
        cs->coll != 0 &&
        cs->coll->strnxfrm != 0 &&
        cs->strxfrm_multiply == 1; // current limitation
    }
    break;
  case Type::Varchar:
    return true; // Varchar not used via MySQL
  case Type::Blob:
  case Type::Text:
    break;
  default:
    return true;
  }
  return false;
}

bool
NdbSqlUtil::usable_in_hash_index(Uint32 typeId, const void* info)
{
  return usable_in_pk(typeId, info);
}

bool
NdbSqlUtil::usable_in_ordered_index(Uint32 typeId, const void* info)
{
  const Type& type = getType(typeId);
  switch (type.m_typeId) {
  case Type::Undefined:
    break;
  case Type::Char:
    {
      const CHARSET_INFO *cs = (const CHARSET_INFO*)info;
      return
        cs != 0 &&
        cs->cset != 0 &&
        cs->coll != 0 &&
        cs->coll->strnxfrm != 0 &&
        cs->coll->strnncollsp != 0 &&
        cs->strxfrm_multiply == 1; // current limitation
    }
    break;
  case Type::Varchar:
    return true; // Varchar not used via MySQL
  case Type::Text:
    {
      const CHARSET_INFO *cs = (const CHARSET_INFO*)info;
      return
        cs != 0 &&
        cs->mbmaxlen == 1 && // extra limitation
        cs->cset != 0 &&
        cs->coll != 0 &&
        cs->coll->strnxfrm != 0 &&
        cs->coll->strnncollsp != 0 &&
        cs->strxfrm_multiply == 1; // current limitation
    }
    break;
  default:
    return true;
  }
  return false;
}

#ifdef NDB_SQL_UTIL_TEST

#include <NdbTick.h>
#include <NdbOut.hpp>

struct Testcase {
  int op;       // 1=compare 2=like
  int res;
  const char* s1;
  const char* s2;
  int pad;
};
const Testcase testcase[] = {
  { 2, 1, "abc", "abc", 0 },
  { 2, 1, "abc", "abc%", 0 },
  { 2, 1, "abcdef", "abc%", 0 },
  { 2, 1, "abcdefabcdefabcdef", "abc%", 0 },
  { 2, 1, "abcdefabcdefabcdef", "abc%f", 0 },
  { 2, 0, "abcdefabcdefabcdef", "abc%z", 0 },
  { 2, 1, "abcdefabcdefabcdef", "%f", 0 },
  { 2, 1, "abcdef", "a%b%c%d%e%f", 0 },
  { 0, 0, 0, 0 }
};

int
main(int argc, char** argv)
{
  unsigned count = argc > 1 ? atoi(argv[1]) : 1000000;
  ndbout_c("count = %u", count);
  assert(count != 0);
  for (const Testcase* t = testcase; t->s1 != 0; t++) {
    ndbout_c("%d = '%s' %s '%s' pad=%d",
        t->res, t->s1, t->op == 1 ? "comp" : "like", t->s2);
    NDB_TICKS x1 = NdbTick_CurrentMillisecond();
    unsigned n1 = strlen(t->s1);
    unsigned n2 = strlen(t->s2);
    for (unsigned i = 0; i < count; i++) {
      if (t->op == 1) {
        int res = NdbSqlUtil::char_compare(t->s1, n1, t->s2, n2, t->pad);
        assert(res == t->res);
        continue;
      }
      if (t->op == 2) {
        int res = NdbSqlUtil::char_like(t->s1, n1, t->s2, n2, t->pad);
        assert(res == t->res);
        continue;
      }
      assert(false);
    }
    NDB_TICKS x2 = NdbTick_CurrentMillisecond();
    if (x2 < x1)
      x2 = x1;
    double usec = 1000000.0 * double(x2 - x1) / double(count);
    ndbout_c("time %.0f usec per call", usec);
  }
  // quick check
  for (unsigned i = 0; i < sizeof(m_typeList) / sizeof(m_typeList[0]); i++) {
    const NdbSqlUtil::Type& t = m_typeList[i];
    assert(t.m_typeId == i);
  }
  return 0;
}

#endif
