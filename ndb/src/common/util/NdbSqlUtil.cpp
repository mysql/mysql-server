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
NdbSqlUtil::type(Uint32 typeId)
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
  return cmp(Type::Tinyint, p1, p2, full, size);
}

int
NdbSqlUtil::cmpTinyunsigned(const Uint32* p1, const Uint32* p2, Uint32 full, Uint32 size)
{
  return cmp(Type::Tinyunsigned, p1, p2, full, size);
}

int
NdbSqlUtil::cmpSmallint(const Uint32* p1, const Uint32* p2, Uint32 full, Uint32 size)
{
  return cmp(Type::Smallint, p1, p2, full, size);
}

int
NdbSqlUtil::cmpSmallunsigned(const Uint32* p1, const Uint32* p2, Uint32 full, Uint32 size)
{
  return cmp(Type::Smallunsigned, p1, p2, full, size);
}

int
NdbSqlUtil::cmpMediumint(const Uint32* p1, const Uint32* p2, Uint32 full, Uint32 size)
{
  return cmp(Type::Mediumint, p1, p2, full, size);
}

int
NdbSqlUtil::cmpMediumunsigned(const Uint32* p1, const Uint32* p2, Uint32 full, Uint32 size)
{
  return cmp(Type::Mediumunsigned, p1, p2, full, size);
}

int
NdbSqlUtil::cmpInt(const Uint32* p1, const Uint32* p2, Uint32 full, Uint32 size)
{
  return cmp(Type::Int, p1, p2, full, size);
}

int
NdbSqlUtil::cmpUnsigned(const Uint32* p1, const Uint32* p2, Uint32 full, Uint32 size)
{
  return cmp(Type::Unsigned, p1, p2, full, size);
}

int
NdbSqlUtil::cmpBigint(const Uint32* p1, const Uint32* p2, Uint32 full, Uint32 size)
{
  return cmp(Type::Bigint, p1, p2, full, size);
}

int
NdbSqlUtil::cmpBigunsigned(const Uint32* p1, const Uint32* p2, Uint32 full, Uint32 size)
{
  return cmp(Type::Bigunsigned, p1, p2, full, size);
}

int
NdbSqlUtil::cmpFloat(const Uint32* p1, const Uint32* p2, Uint32 full, Uint32 size)
{
  return cmp(Type::Float, p1, p2, full, size);
}

int
NdbSqlUtil::cmpDouble(const Uint32* p1, const Uint32* p2, Uint32 full, Uint32 size)
{
  return cmp(Type::Double, p1, p2, full, size);
}

int
NdbSqlUtil::cmpDecimal(const Uint32* p1, const Uint32* p2, Uint32 full, Uint32 size)
{
  return cmp(Type::Decimal, p1, p2, full, size);
}

int
NdbSqlUtil::cmpChar(const Uint32* p1, const Uint32* p2, Uint32 full, Uint32 size)
{
  return cmp(Type::Char, p1, p2, full, size);
}

int
NdbSqlUtil::cmpVarchar(const Uint32* p1, const Uint32* p2, Uint32 full, Uint32 size)
{
  return cmp(Type::Varchar, p1, p2, full, size);
}

int
NdbSqlUtil::cmpBinary(const Uint32* p1, const Uint32* p2, Uint32 full, Uint32 size)
{
  return cmp(Type::Binary, p1, p2, full, size);
}

int
NdbSqlUtil::cmpVarbinary(const Uint32* p1, const Uint32* p2, Uint32 full, Uint32 size)
{
  return cmp(Type::Varbinary, p1, p2, full, size);
}

int
NdbSqlUtil::cmpDatetime(const Uint32* p1, const Uint32* p2, Uint32 full, Uint32 size)
{
  return cmp(Type::Datetime, p1, p2, full, size);
}

int
NdbSqlUtil::cmpTimespec(const Uint32* p1, const Uint32* p2, Uint32 full, Uint32 size)
{
  return cmp(Type::Timespec, p1, p2, full, size);
}

int
NdbSqlUtil::cmpBlob(const Uint32* p1, const Uint32* p2, Uint32 full, Uint32 size)
{
  return cmp(Type::Blob, p1, p2, full, size);
}

int
NdbSqlUtil::cmpText(const Uint32* p1, const Uint32* p2, Uint32 full, Uint32 size)
{
  return cmp(Type::Text, p1, p2, full, size);
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
