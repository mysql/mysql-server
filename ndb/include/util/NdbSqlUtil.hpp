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

#ifndef NDB_SQL_UTIL_HPP
#define NDB_SQL_UTIL_HPP

#include <ndb_global.h>
#include <kernel/ndb_limits.h>

class NdbSqlUtil {
public:
  /**
   * Compare strings, optionally with padded semantics.  Returns
   * negative (less), zero (equal), or positive (greater).
   */
  static int char_compare(const char* s1, unsigned n1,
                          const char* s2, unsigned n2, bool padded);

  /**
   * Like operator, optionally with padded semantics.  Returns true or
   * false.
   */
  static bool char_like(const char* s1, unsigned n1,
                        const char* s2, unsigned n2, bool padded);

  /**
   * Compare kernel attribute values.  Returns -1, 0, +1 for less,
   * equal, greater, respectively.  Parameters are pointers to values,
   * full attribute size in words, and size of available data in words.
   * If available size is less than full size, CmpUnknown may be
   * returned.  If a value cannot be parsed, it compares like NULL i.e.
   * less than any valid value.
   */
  typedef int Cmp(const Uint32* p1, const Uint32* p2, Uint32 full, Uint32 size);

  enum CmpResult {
    CmpLess = -1,
    CmpEqual = 0,
    CmpGreater = 1,
    CmpUnknown = 2      // insufficient partial data
  };

  /**
   * Kernel data types.  Must match m_typeList in NdbSqlUtil.cpp.
   */
  struct Type {
    enum Enum {
      Undefined = 0,    // Undefined 
      Tinyint,          // 8 bit
      Tinyunsigned,     // 8 bit
      Smallint,         // 16 bit
      Smallunsigned,    // 16 bit
      Mediumint,        // 24 bit
      Mediumunsigned,   // 24 bit
      Int,              // 32 bit
      Unsigned,         // 32 bit
      Bigint,           // 64 bit
      Bigunsigned,      // 64 Bit
      Float,            // 32-bit float
      Double,           // 64-bit float
      Decimal,          // Precision, Scale
      Char,             // Len
      Varchar,          // Max len
      Binary,           // Len
      Varbinary,        // Max len
      Datetime,         // Precision down to 1 sec  (size 8 bytes)
      Timespec,         // Precision down to 1 nsec (size 12 bytes)
      Blob,             // Blob
      Text              // Text blob
    };
    Enum m_typeId;
    Cmp* m_cmp;         // comparison method
  };

  /**
   * Get type by id.  Can return the Undefined type.
   */
  static const Type& getType(Uint32 typeId);

  /**
   * Check character set.
   */
  static bool usable_in_pk(Uint32 typeId, const void* cs);
  static bool usable_in_hash_index(Uint32 typeId, const void* cs);
  static bool usable_in_ordered_index(Uint32 typeId, const void* cs);

private:
  /**
   * List of all types.  Must match Type::Enum.
   */
  static const Type m_typeList[];
  /**
   * Comparison methods.
   */
  static Cmp cmpTinyint;
  static Cmp cmpTinyunsigned;
  static Cmp cmpSmallint;
  static Cmp cmpSmallunsigned;
  static Cmp cmpMediumint;
  static Cmp cmpMediumunsigned;
  static Cmp cmpInt;
  static Cmp cmpUnsigned;
  static Cmp cmpBigint;
  static Cmp cmpBigunsigned;
  static Cmp cmpFloat;
  static Cmp cmpDouble;
  static Cmp cmpDecimal;
  static Cmp cmpChar;
  static Cmp cmpVarchar;
  static Cmp cmpBinary;
  static Cmp cmpVarbinary;
  static Cmp cmpDatetime;
  static Cmp cmpTimespec;
  static Cmp cmpBlob;
  static Cmp cmpText;
};

#endif
