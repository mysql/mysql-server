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
   * Compare attribute values.  Returns -1, 0, +1 for less, equal,
   * greater, respectively.  Parameters are pointers to values and their
   * lengths in bytes.  The lengths can differ.
   *
   * First value is a full value but second value can be partial.  If
   * the partial value is not enough to determine the result, CmpUnknown
   * will be returned.  A shorter second value is not necessarily
   * partial.  Partial values are allowed only for types where prefix
   * comparison is possible (basically, binary types).
   *
   * First parameter is a pointer to type specific extra info.  Char
   * types receive CHARSET_INFO in it.
   *
   * If a value cannot be parsed, it compares like NULL i.e. less than
   * any valid value.
   */
  typedef int Cmp(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2, bool full);

  enum CmpResult {
    CmpLess = -1,
    CmpEqual = 0,
    CmpGreater = 1,
    CmpUnknown = 2      // insufficient partial data
  };

  struct Type {
    enum Enum {
      Undefined = NDB_TYPE_UNDEFINED,
      Tinyint = NDB_TYPE_TINYINT,
      Tinyunsigned = NDB_TYPE_TINYUNSIGNED,
      Smallint = NDB_TYPE_SMALLINT,
      Smallunsigned = NDB_TYPE_SMALLUNSIGNED,
      Mediumint = NDB_TYPE_MEDIUMINT,
      Mediumunsigned = NDB_TYPE_MEDIUMUNSIGNED,
      Int = NDB_TYPE_INT,
      Unsigned = NDB_TYPE_UNSIGNED,
      Bigint = NDB_TYPE_BIGINT,
      Bigunsigned = NDB_TYPE_BIGUNSIGNED,
      Float = NDB_TYPE_FLOAT,
      Double = NDB_TYPE_DOUBLE,
      Decimal = NDB_TYPE_DECIMAL,
      Char = NDB_TYPE_CHAR,
      Varchar = NDB_TYPE_VARCHAR,
      Binary = NDB_TYPE_BINARY,
      Varbinary = NDB_TYPE_VARBINARY,
      Datetime = NDB_TYPE_DATETIME,
      Timespec = NDB_TYPE_TIMESPEC,
      Blob = NDB_TYPE_BLOB,
      Text = NDB_TYPE_TEXT,
      Bit = NDB_TYPE_BIT,
      Time = NDB_TYPE_TIME
    };
    Enum m_typeId;
    Cmp* m_cmp;         // comparison method
  };

  /**
   * Get type by id.  Can return the Undefined type.
   */
  static const Type& getType(Uint32 typeId);

  /**
   * Get type by id but replace char type by corresponding binary type.
   */
  static const Type& getTypeBinary(Uint32 typeId);

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
  static Cmp cmpDate;
  static Cmp cmpBlob;
  static Cmp cmpText;
  static Cmp cmpTime;
};

#endif
