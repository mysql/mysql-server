/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

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

struct charset_info_st;
typedef struct charset_info_st CHARSET_INFO;

class NdbSqlUtil {
public:
  /**
   * Compare attribute values.  Returns -1, 0, +1 for less, equal,
   * greater, respectively.  Parameters are pointers to values and their
   * lengths in bytes.  The lengths can differ.
   *
   * First value is a full value but second value can be partial.  If
   * the partial value is not enough to determine the result, CmpUnknown
   * will be returned.  A shorter second value is not necessarily
   * partial.  Partial values are allowed only for types where prefix
   * comparison is possible (basically, binary strings).
   *
   * First parameter is a pointer to type specific extra info.  Char
   * types receive CHARSET_INFO in it.
   *
   * If a value cannot be parsed, it compares like NULL i.e. less than
   * any valid value.
   */
  typedef int Cmp(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2, bool full);

  /**
   * Prototype for "like" comparison.  Defined for string types.  First
   * argument can be fixed or var* type, second argument is fixed.
   * Returns 0 on match, +1 on no match, and -1 on bad data.
   *
   * Uses default special chars ( \ % _ ).
   */
  typedef int Like(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2);

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
      Olddecimal = NDB_TYPE_OLDDECIMAL,
      Char = NDB_TYPE_CHAR,
      Varchar = NDB_TYPE_VARCHAR,
      Binary = NDB_TYPE_BINARY,
      Varbinary = NDB_TYPE_VARBINARY,
      Datetime = NDB_TYPE_DATETIME,
      Date = NDB_TYPE_DATE,
      Blob = NDB_TYPE_BLOB,
      Text = NDB_TYPE_TEXT,
      Bit = NDB_TYPE_BIT,
      Longvarchar = NDB_TYPE_LONGVARCHAR,
      Longvarbinary = NDB_TYPE_LONGVARBINARY,
      Time = NDB_TYPE_TIME,
      Year = NDB_TYPE_YEAR,
      Timestamp = NDB_TYPE_TIMESTAMP,
      Olddecimalunsigned = NDB_TYPE_OLDDECIMALUNSIGNED,
      Decimal = NDB_TYPE_DECIMAL,
      Decimalunsigned = NDB_TYPE_DECIMALUNSIGNED
    };
    Enum m_typeId;      // redundant
    Cmp* m_cmp;         // comparison method
    Like* m_like;       // "like" comparison method
  };

  /**
   * Get type by id.  Can return the Undefined type.
   */
  static const Type& getType(Uint32 typeId);

  /**
   * Get the normalized type used in hashing and key comparisons.
   * Maps all string types to Binary.  This includes Var* strings
   * because strxfrm result is padded to fixed (maximum) length.
   */
  static const Type& getTypeBinary(Uint32 typeId);

  /**
   * Check character set.
   */
  static uint check_column_for_pk(Uint32 typeId, const void* info);
  static uint check_column_for_hash_index(Uint32 typeId, const void* info);
  static uint check_column_for_ordered_index(Uint32 typeId, const void* info);

  /**
   * Get number of length bytes and length from variable length string.
   * Returns false on error (invalid data).  For other types returns
   * zero length bytes and the fixed attribute length.
   */
  static bool get_var_length(Uint32 typeId, const void* p, unsigned attrlen, Uint32& lb, Uint32& len);

  /**
   * Temporary workaround for bug#7284.
   */
  static int strnxfrm_bug7284(CHARSET_INFO* cs, unsigned char* dst, unsigned dstLen, const unsigned char*src, unsigned srcLen);

  /**
   * Compare decimal numbers.
   */
  static int cmp_olddecimal(const uchar* s1, const uchar* s2, unsigned n);

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
  static Cmp cmpOlddecimal;
  static Cmp cmpChar;
  static Cmp cmpVarchar;
  static Cmp cmpBinary;
  static Cmp cmpVarbinary;
  static Cmp cmpDatetime;
  static Cmp cmpDate;
  static Cmp cmpBlob;
  static Cmp cmpText;
  static Cmp cmpBit;
  static Cmp cmpLongvarchar;
  static Cmp cmpLongvarbinary;
  static Cmp cmpTime;
  static Cmp cmpYear;
  static Cmp cmpTimestamp;
  static Cmp cmpOlddecimalunsigned;
  static Cmp cmpDecimal;
  static Cmp cmpDecimalunsigned;
  //
  static Like likeChar;
  static Like likeBinary;
  static Like likeVarchar;
  static Like likeVarbinary;
  static Like likeLongvarchar;
  static Like likeLongvarbinary;
};

#endif
