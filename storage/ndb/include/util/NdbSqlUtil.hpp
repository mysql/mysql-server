/*
   Copyright (c) 2003, 2016, Oracle and/or its affiliates. All rights reserved.

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

#ifndef NDB_SQL_UTIL_HPP
#define NDB_SQL_UTIL_HPP

#include <ndb_global.h>
#include <kernel/ndb_limits.h>

struct charset_info_st;
typedef struct charset_info_st CHARSET_INFO;

/**
 * Helper class with comparison functions on NDB (column) data types.
 *
 * Notes: this Helper class
 * - is used by kernel code
 * - provides non-elementary functions
 * - is not generic, template-based code
 * - has link/library dependencies upon MySQL code
 * (in contrast to other type utility classes, like ./NdbTypesUtil).
 */
class NdbSqlUtil {
public:
  /**
   * Compare attribute values.  Returns negative, zero, positive for
   * less, equal, greater.  We trust DBTUP to validate all data and
   * mysql upgrade to not invalidate them.  Bad values (such as NaN)
   * causing undefined results crash here always (require, not assert)
   * since they are likely to cause a more obscure crash in DBTUX.
   * wl4163_todo: API probably should not crash.
   *
   * Parameters are pointers to values (no alignment requirements) and
   * their lengths in bytes.  First parameter is a pointer to type
   * specific extra info.  Char types receive CHARSET_INFO in it.
   */
  typedef int Cmp(const void* info, const void* p1, uint n1, const void* p2, uint n2);

  /**
   * Prototype for "like" comparison.  Defined for string types.  First
   * argument can be fixed or var* type, second argument is fixed.
   * Returns 0 on match, +1 on no match, and -1 on bad data.
   *
   * Uses default special chars ( \ % _ ).
   */
  typedef int Like(const void* info, const void* p1, unsigned n1, const void* p2, unsigned n2);

  /**
   * Prototype for mask comparisons.  Defined for bit type.
   *
   * If common portion of data AND Mask is equal to mask
   * return 0, else return 1.
   * If cmpZero, compare data AND Mask to zero.
   */
  typedef int AndMask(const void* data, unsigned dataLen, const void* mask, unsigned maskLen, bool cmpZero); 

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
      Decimalunsigned = NDB_TYPE_DECIMALUNSIGNED,
      Time2 = NDB_TYPE_TIME2,
      Datetime2 = NDB_TYPE_DATETIME2,
      Timestamp2 = NDB_TYPE_TIMESTAMP2
    };
    Enum m_typeId;      // redundant
    Cmp* m_cmp;         // comparison method
    Like* m_like;       // "like" comparison method
    AndMask* m_mask;    // Mask comparison method
  };

  /**
   * Get type by id.  Can return the Undefined type.
   */
  static const Type& getType(Uint32 typeId);

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
   * Convert attribute data to/from network byte order
   * This method converts the passed data of the passed type
   * between host and network byte order.
   * On little-endian (network order) hosts, it has no effect.
   */
  static void convertByteOrder(Uint32 typeId, 
                               Uint32 typeLog2Size, 
                               Uint32 arrayType, 
                               Uint32 arraySize,
                               uchar* data,
                               Uint32 dataByteSize);

  /**
   * Unpack and pack date/time types.  There is no check that the data
   * is valid for MySQL.  Random input gives equally random output.
   * Fractional seconds wl#946 introduce new formats (type names with
   * suffix 2).  The methods for these take an extra precision argument
   * with range 0-6 which translates to 0-3 bytes.
   */
  struct Year {
    uint year;
  };
  struct Date {
    uint year, month, day;
  };
  struct Time {
    uint sign; // as in Time2
    uint hour, minute, second;
  };
  struct Datetime {
    uint year, month, day;
    uint hour, minute, second;
  };
  struct Timestamp {
    uint second;
  };
  struct Time2 {
    uint sign;
    uint interval;
    uint hour, minute, second;
    uint fraction;
  };
  struct Datetime2 {
    uint sign;
    uint year, month, day;
    uint hour, minute, second;
    uint fraction;
  };
  struct Timestamp2 {
    uint second;
    uint fraction;
  };
  // bytes to struct
  static void unpack_year(Year&, const uchar*);
  static void unpack_date(Date&, const uchar*);
  static void unpack_time(Time&, const uchar*);
  static void unpack_datetime(Datetime&, const uchar*);
  static void unpack_timestamp(Timestamp&, const uchar*);
  static void unpack_time2(Time2&, const uchar*, uint prec);
  static void unpack_datetime2(Datetime2&, const uchar*, uint prec);
  static void unpack_timestamp2(Timestamp2&, const uchar*, uint prec);
  // struct to bytes
  static void pack_year(const Year&, uchar*);
  static void pack_date(const Date&, uchar*);
  static void pack_time(const Time&, uchar*);
  static void pack_datetime(const Datetime&, uchar*);
  static void pack_timestamp(const Timestamp&, uchar*);
  static void pack_time2(const Time2&, uchar*, uint prec);
  static void pack_datetime2(const Datetime2&, uchar*, uint prec);
  static void pack_timestamp2(const Timestamp2&, uchar*, uint prec);

private:
  friend class NdbPack;
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
  static Cmp cmpTime2;
  static Cmp cmpDatetime2;
  static Cmp cmpTimestamp2;
  //
  static Like likeChar;
  static Like likeBinary;
  static Like likeVarchar;
  static Like likeVarbinary;
  static Like likeLongvarchar;
  static Like likeLongvarbinary;
  //
  static AndMask maskBit;
};

#endif
