/*
   Copyright (c) 2004, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <NDBT_ReturnCodes.h>
#include "consumer_restore.hpp"
#include <kernel/ndb_limits.h>
#include <my_sys.h>
#include <NdbSleep.h>
#include <NdbTick.h>
#include <Properties.hpp>
#include <NdbTypesUtil.hpp>

#include <ndb_internal.hpp>
#include <ndb_logevent.h>
#include "../src/ndbapi/NdbDictionaryImpl.hpp"
#include "../ndb_lib_move_data.hpp"

#define NDB_ANYVALUE_FOR_NOLOGGING 0x8000007f

extern FilteredNdbOut err;
extern FilteredNdbOut info;
extern FilteredNdbOut debug;

static void callback(int, NdbTransaction*, void*);
static Uint32 get_part_id(const NdbDictionary::Table *table,
                          Uint32 hash_value);

extern BaseString g_options;
extern unsigned int opt_no_binlog;
extern bool ga_skip_broken_objects;

extern Properties g_rewrite_databases;

bool BackupRestore::m_preserve_trailing_spaces = false;

// ----------------------------------------------------------------------
// conversion handlers
// ----------------------------------------------------------------------

void *
BackupRestore::convert_bitset(const void *source,
                              void *target,
                              bool &truncated)
{
  if (!source || !target)
    return NULL;

  // shortcuts
  const unsigned char * const s = (const unsigned char *)source;
  char_n_padding_struct * const t = (char_n_padding_struct *)target;

  // write data
  if (t->n_new >= t->n_old)
  {
    // clear all bits
    memset(t->new_row, 0, t->n_new);

    memcpy(t->new_row, s, t->n_old);
    truncated = false;
  } else {
    // set all bits, for parity with replication's demotion semantics
    memset(t->new_row, 0xFF, t->n_new);
    truncated = true;
  }

  return t->new_row;
}

template< typename S, typename T >
void *
BackupRestore::convert_array(const void * source,
                             void * target,
                             bool & truncated)
{
  if (!source || !target)
    return NULL;

  // shortcuts (note that all S::... and T::... are compile-time expr)
  const unsigned char * const s = (const unsigned char *)source;
  char_n_padding_struct * const t = (char_n_padding_struct *)target;
  const Uint32 s_prefix_length = S::lengthPrefixSize();
  const Uint32 t_prefix_length = T::lengthPrefixSize();

  // read and adjust length
  Uint32 length = (S::isFixedSized() ? t->n_old : S::readLengthPrefix(s));
  const Uint32 max_length = t->n_new - t_prefix_length;
  if (S::isFixedSized() && !m_preserve_trailing_spaces) {
    const char s_padding_char = (S::isBinary() ? 0x00 : ' ');
    // ignore padding chars for data copying or truncation reporting
    while (length > 0 && s[length - 1] == s_padding_char) {
      length--;
    }
  }
  if (length <= max_length) {
    truncated = false;
  } else {
    length = max_length;
    truncated = true;
  }

  // write length prefix
  if (!T::isFixedSized()) {
    T::writeLengthPrefix(t->new_row, length);
  }

  // write data
  memcpy(t->new_row + t_prefix_length, s + s_prefix_length, length);

  // write padding
  if (T::isFixedSized()) {
    const char t_padding_char = (T::isBinary() ? 0x00 : ' ');
    const Uint32 l = max_length - length;
    memset(t->new_row + t_prefix_length + length, t_padding_char, l);
  }

  return t->new_row;
}

template< typename S, typename T >
void *
BackupRestore::convert_integral(const void * source,
                                void * target,
                                bool & truncated)
{
  if (!source || !target)
    return NULL;

  // read the source value
  typename S::DomainT s;
  S::load(&s, (char *)source);

  // Note: important to correctly handle mixed signedness comparisons.
  //
  // The problem: A straight-forward approach to convert value 's' into
  // type 'T' might be to check into which of these subranges 's' falls
  //    ... < T's lower bound <= ... <= T's upper bound < ...
  // However, this approach is _incorrect_ when applied to generic code
  //    if (s < T::lowest()) ... else if (s > T::highest()) ... else ...
  // since 'S' and 'T' may be types of different signedness.
  //
  // Under ansi (and even more K&R) C promotion rules, if 'T' is unsigned
  // and if there's no larger signed type available, the value 's' gets
  // promoted to unsigned; then, a negative value of 's' becomes (large)
  // positive -- with a wrong comparison outcome.
  //
  // Furthermore, the code should not trigger compiler warnings for any
  // selection of integral types 'S', 'T' ("mixed signedness comparison",
  // "comparison of unsigned expression <0 / >=0 is always false/true").
  //
  // The correct approach: do lower bound comparisons on signed types and
  // upper bound comparisons on unsigned types only; this requires casts.
  // For the casts to be safe, compare the value against the zero literal
  //    if (s <= 0) { check as signed } else { check as unsigned }
  // which is a valid + nontrivial test for signed and unsigned types.
  //
  // This implies that correct, generic conversion code must test into
  // which of these _four_ subranges value 's' falls
  //    ... < T's lower bound <= ... <= 0 < ... <= T's upper bound < ...
  // while handling 's' as signed/unsigned where less-equal/greater zero.
  //
  // Obviously, simplifications are possible if 'S' is unsigned or known
  // to be a subset of 'T'.  This can be accomplished by a few additional
  // compile-time expression tests, which allow code optimization to
  // issue fewer checks for certain specializations of types 'S' and 'T'.

  // write the target value
  typename T::DomainT t;
  if (s <= 0) {

    // check value against lower bound as _signed_, safe since all <= 0
    assert(S::lowest() <= 0 && T::lowest() <= 0 && s <= 0);
    const typename S::SignedT s_l_s = S::asSigned(S::lowest());
    const typename T::SignedT t_l_s = T::asSigned(T::lowest());
    const typename S::SignedT s_s = S::asSigned(s);
    if ((s_l_s < t_l_s)      // compile-time expr
        && (s_s < t_l_s)) {  // lower bound check
      t = T::lowest();
      truncated = true;
    } else {                 // within both bounds
      t = static_cast< typename T::DomainT >(s);
      truncated = false;
    }

  } else { // (s > 0)

    // check value against upper bound as _unsigned_, safe since all > 0
    assert(S::highest() > 0 && T::highest() > 0 && s > 0);
    const typename S::UnsignedT s_h_u = S::asUnsigned(S::highest());
    const typename T::UnsignedT t_h_u = T::asUnsigned(T::highest());
    const typename S::UnsignedT s_u = S::asUnsigned(s);
    if ((s_h_u > t_h_u)      // compile-time expr
        && (s_u > t_h_u)) {  // upper bound check
      t = T::highest();
      truncated = true;
    } else {                 // within both bounds
      t = static_cast< typename T::DomainT >(s);
      truncated = false;
    }

  }
  T::store((char *)target, &t);

  return target;
}

static uint
truncate_fraction(uint f, uint n_old, uint n_new, bool& truncated)
{
  static const uint pow10[1 + 6] = {
    1, 10, 100, 1000, 10000, 100000, 1000000
  };
  assert(n_old <= 6 && n_new <= 6);
  if (n_old <= n_new)
    return f;
  uint k = n_old - n_new;
  uint n = pow10[k];
  uint g = f / n;
  if (g * n != f)
    truncated = true;
  return g;
}

void *
BackupRestore::convert_time_time2(const void * source,
                                  void * target,
                                  bool & truncated)
{
  if (!source || !target)
    return NULL;

  const uchar* s = (const uchar*)source;
  char_n_padding_struct* t = (char_n_padding_struct*)target;
  assert(t->n_old == 0 && t->n_new <= 6);

  NdbSqlUtil::Time ss;
  NdbSqlUtil::Time2 ts;
  truncated = false;

  NdbSqlUtil::unpack_time(ss, s);

  ts.sign = ss.sign;
  ts.interval = 0;
  ts.hour = ss.hour;
  ts.minute = ss.minute;
  ts.second = ss.second;
  ts.fraction = 0;
  NdbSqlUtil::pack_time2(ts, (uchar*)t->new_row, t->n_new);

  return t->new_row;
}

void *
BackupRestore::convert_time2_time(const void * source,
                                  void * target,
                                  bool & truncated)
{
  if (!source || !target)
    return NULL;

  const uchar* s = (const uchar*)source;
  char_n_padding_struct* t = (char_n_padding_struct*)target;
  assert(t->n_old <= 6 && t->n_new == 0);

  NdbSqlUtil::Time2 ss;
  NdbSqlUtil::Time ts;
  truncated = false;

  NdbSqlUtil::unpack_time2(ss, s, t->n_old);
  if (ss.fraction != 0)
    truncated = true;

  ts.sign = ss.sign;
  ts.hour = ss.hour;
  ts.minute = ss.minute;
  ts.second = ss.second;
  NdbSqlUtil::pack_time(ts, (uchar*)t->new_row);

  return t->new_row;
}

void *
BackupRestore::convert_time2_time2(const void * source,
                                   void * target,
                                   bool & truncated)
{
  if (!source || !target)
    return NULL;

  const uchar* s = (const uchar*)source;
  char_n_padding_struct* t = (char_n_padding_struct*)target;
  assert(t->n_old <= 6 && t->n_new <= 6);

  NdbSqlUtil::Time2 ss;
  NdbSqlUtil::Time2 ts;
  truncated = false;

  NdbSqlUtil::unpack_time2(ss, s, t->n_old);
  uint fraction = truncate_fraction(ss.fraction,
                                    t->n_old, t->n_new, truncated);

  ts.sign = ss.sign;
  ts.interval = ss.interval;
  ts.hour = ss.hour;
  ts.minute = ss.minute;
  ts.second = ss.second;
  ts.fraction = fraction;
  NdbSqlUtil::pack_time2(ts, (uchar*)t->new_row, t->n_new);

  return t->new_row;
}

void *
BackupRestore::convert_datetime_datetime2(const void * source,
                                          void * target,
                                          bool & truncated)
{
  if (!source || !target)
    return NULL;

  const uchar* s = (const uchar*)source;
  char_n_padding_struct* t = (char_n_padding_struct*)target;
  assert(t->n_old == 0 && t->n_new <= 6);

  NdbSqlUtil::Datetime ss;
  NdbSqlUtil::Datetime2 ts;
  truncated = false;

  NdbSqlUtil::unpack_datetime(ss, s);

  ts.sign = 1;
  ts.year = ss.year;
  ts.month = ss.month;
  ts.day = ss.day;
  ts.hour = ss.hour;
  ts.minute = ss.minute;
  ts.second = ss.second;
  ts.fraction = 0;
  NdbSqlUtil::pack_datetime2(ts, (uchar*)t->new_row, t->n_new);

  return t->new_row;
}

void *
BackupRestore::convert_datetime2_datetime(const void * source,
                                          void * target,
                                          bool & truncated)
{
  if (!source || !target)
    return NULL;

  const uchar* s = (const uchar*)source;
  char_n_padding_struct* t = (char_n_padding_struct*)target;
  assert(t->n_old <= 6 && t->n_new == 0);

  NdbSqlUtil::Datetime2 ss;
  NdbSqlUtil::Datetime ts;
  truncated = false;

  NdbSqlUtil::unpack_datetime2(ss, s, t->n_old);
  if (ss.fraction != 0)
    truncated = true;
  if (ss.sign != 1) // should not happen
    truncated = true;

  ts.year = ss.year;
  ts.month = ss.month;
  ts.day = ss.day;
  ts.hour = ss.hour;
  ts.minute = ss.minute;
  ts.second = ss.second;
  NdbSqlUtil::pack_datetime(ts, (uchar*)t->new_row);

  return t->new_row;
}

void *
BackupRestore::convert_datetime2_datetime2(const void * source,
                                           void * target,
                                           bool & truncated)
{
  if (!source || !target)
    return NULL;

  const uchar* s = (const uchar*)source;
  char_n_padding_struct* t = (char_n_padding_struct*)target;
  assert(t->n_old <= 6 && t->n_new <= 6);

  NdbSqlUtil::Datetime2 ss;
  NdbSqlUtil::Datetime2 ts;
  truncated = false;

  NdbSqlUtil::unpack_datetime2(ss, s, t->n_old);
  uint fraction = truncate_fraction(ss.fraction,
                                    t->n_old, t->n_new, truncated);

  ts.sign = ss.sign;
  ts.year = ss.year;
  ts.month = ss.month;
  ts.day = ss.day;
  ts.hour = ss.hour;
  ts.minute = ss.minute;
  ts.second = ss.second;
  ts.fraction = fraction;
  NdbSqlUtil::pack_datetime2(ts, (uchar*)t->new_row, t->n_new);

  return t->new_row;
}

void *
BackupRestore::convert_timestamp_timestamp2(const void * source,
                                            void * target,
                                            bool & truncated)
{
  if (!source || !target)
    return NULL;

  const uchar* s = (const uchar*)source;
  char_n_padding_struct* t = (char_n_padding_struct*)target;
  assert(t->n_old == 0 && t->n_new <= 6);

  NdbSqlUtil::Timestamp ss;
  NdbSqlUtil::Timestamp2 ts;
  truncated = false;

  NdbSqlUtil::unpack_timestamp(ss, s);

  ts.second = ss.second;
  ts.fraction = 0;
  NdbSqlUtil::pack_timestamp2(ts, (uchar*)t->new_row, t->n_new);

  return t->new_row;
}

void *
BackupRestore::convert_timestamp2_timestamp(const void * source,
                                            void * target,
                                            bool & truncated)
{
  if (!source || !target)
    return NULL;

  const uchar* s = (const uchar*)source;
  char_n_padding_struct* t = (char_n_padding_struct*)target;
  assert(t->n_old <= 6 && t->n_new == 0);

  NdbSqlUtil::Timestamp2 ss;
  NdbSqlUtil::Timestamp ts;
  truncated = false;

  NdbSqlUtil::unpack_timestamp2(ss, s, t->n_old);
  if (ss.fraction != 0)
    truncated = true;

  ts.second = ss.second;
  NdbSqlUtil::pack_timestamp(ts, (uchar*)t->new_row);

  return t->new_row;
}

void *
BackupRestore::convert_timestamp2_timestamp2(const void * source,
                                             void * target,
                                             bool & truncated)
{
  if (!source || !target)
    return NULL;

  const uchar* s = (const uchar*)source;
  char_n_padding_struct* t = (char_n_padding_struct*)target;
  assert(t->n_old <= 6 && t->n_new <= 6);

  NdbSqlUtil::Timestamp2 ss;
  NdbSqlUtil::Timestamp2 ts;
  truncated = false;

  NdbSqlUtil::unpack_timestamp2(ss, s, t->n_old);
  uint fraction = truncate_fraction(ss.fraction,
                                    t->n_old, t->n_new, truncated);

  ts.second = ss.second;
  ts.fraction = fraction;
  NdbSqlUtil::pack_timestamp2(ts, (uchar*)t->new_row, t->n_new);

  return t->new_row;
}

// ----------------------------------------------------------------------
// conversion rules
// ----------------------------------------------------------------------

const PromotionRules 
BackupRestore::m_allowed_promotion_attrs[] = {
  // bitset promotions/demotions
  {NDBCOL::Bit,            NDBCOL::Bit,            check_compat_sizes,
   convert_bitset},

  // char array promotions/demotions
  {NDBCOL::Char,           NDBCOL::Char,           check_compat_sizes,
   convert_array< Hchar, Hchar >},
  {NDBCOL::Char,           NDBCOL::Varchar,        check_compat_sizes,
   convert_array< Hchar, Hvarchar >},
  {NDBCOL::Char,           NDBCOL::Longvarchar,    check_compat_sizes,
   convert_array< Hchar, Hlongvarchar >},
  {NDBCOL::Varchar,        NDBCOL::Char,           check_compat_sizes,
   convert_array< Hvarchar, Hchar >},
  {NDBCOL::Varchar,        NDBCOL::Varchar,        check_compat_sizes,
   convert_array< Hvarchar, Hvarchar >},
  {NDBCOL::Varchar,        NDBCOL::Longvarchar,    check_compat_sizes,
   convert_array< Hvarchar, Hlongvarchar >},
  {NDBCOL::Longvarchar,    NDBCOL::Char,           check_compat_sizes,
   convert_array< Hlongvarchar, Hchar >},
  {NDBCOL::Longvarchar,    NDBCOL::Varchar,        check_compat_sizes,
   convert_array< Hlongvarchar, Hvarchar >},
  {NDBCOL::Longvarchar,    NDBCOL::Longvarchar,    check_compat_sizes,
   convert_array< Hlongvarchar, Hlongvarchar >},

  // binary array promotions/demotions
  {NDBCOL::Binary,         NDBCOL::Binary,         check_compat_sizes,
   convert_array< Hbinary, Hbinary >},
  {NDBCOL::Binary,         NDBCOL::Varbinary,      check_compat_sizes,
   convert_array< Hbinary, Hvarbinary >},
  {NDBCOL::Binary,         NDBCOL::Longvarbinary,  check_compat_sizes,
   convert_array< Hbinary, Hlongvarbinary >},
  {NDBCOL::Varbinary,      NDBCOL::Binary,         check_compat_sizes,
   convert_array< Hvarbinary, Hbinary >},
  {NDBCOL::Varbinary,      NDBCOL::Varbinary,      check_compat_sizes,
   convert_array< Hvarbinary, Hvarbinary >},
  {NDBCOL::Varbinary,      NDBCOL::Longvarbinary,  check_compat_sizes,
   convert_array< Hvarbinary, Hlongvarbinary >},
  {NDBCOL::Longvarbinary,  NDBCOL::Binary,         check_compat_sizes,
   convert_array< Hlongvarbinary, Hbinary >},
  {NDBCOL::Longvarbinary,  NDBCOL::Varbinary,      check_compat_sizes,
   convert_array< Hlongvarbinary, Hvarbinary >},
  {NDBCOL::Longvarbinary,  NDBCOL::Longvarbinary,  check_compat_sizes,
   convert_array< Hlongvarbinary, Hlongvarbinary >},

  // char to binary promotions/demotions
  {NDBCOL::Char,           NDBCOL::Binary,         check_compat_char_binary,
   convert_array< Hchar, Hbinary >},
  {NDBCOL::Char,           NDBCOL::Varbinary,      check_compat_char_binary,
   convert_array< Hchar, Hvarbinary >},
  {NDBCOL::Char,           NDBCOL::Longvarbinary,  check_compat_char_binary,
   convert_array< Hchar, Hlongvarbinary >},
  {NDBCOL::Varchar,        NDBCOL::Binary,         check_compat_char_binary,
   convert_array< Hvarchar, Hbinary >},
  {NDBCOL::Varchar,        NDBCOL::Varbinary,      check_compat_char_binary,
   convert_array< Hvarchar, Hvarbinary >},
  {NDBCOL::Varchar,        NDBCOL::Longvarbinary,  check_compat_char_binary,
   convert_array< Hvarchar, Hlongvarbinary >},
  {NDBCOL::Longvarchar,    NDBCOL::Binary,         check_compat_char_binary,
   convert_array< Hlongvarchar, Hbinary >},
  {NDBCOL::Longvarchar,    NDBCOL::Varbinary,      check_compat_char_binary,
   convert_array< Hlongvarchar, Hvarbinary >},
  {NDBCOL::Longvarchar,    NDBCOL::Longvarbinary,  check_compat_char_binary,
   convert_array< Hlongvarchar, Hlongvarbinary >},

  // binary to char promotions/demotions
  {NDBCOL::Binary,         NDBCOL::Char,           check_compat_char_binary,
   convert_array< Hbinary, Hchar >},
  {NDBCOL::Binary,         NDBCOL::Varchar,        check_compat_char_binary,
   convert_array< Hbinary, Hvarchar >},
  {NDBCOL::Binary,         NDBCOL::Longvarchar,    check_compat_char_binary,
   convert_array< Hbinary, Hlongvarchar >},
  {NDBCOL::Varbinary,      NDBCOL::Char,           check_compat_char_binary,
   convert_array< Hvarbinary, Hchar >},
  {NDBCOL::Varbinary,      NDBCOL::Varchar,        check_compat_char_binary,
   convert_array< Hvarbinary, Hvarchar >},
  {NDBCOL::Varbinary,      NDBCOL::Longvarchar,    check_compat_char_binary,
   convert_array< Hvarbinary, Hlongvarchar >},
  {NDBCOL::Longvarbinary,  NDBCOL::Char,           check_compat_char_binary,
   convert_array< Hlongvarbinary, Hchar >},
  {NDBCOL::Longvarbinary,  NDBCOL::Varchar,        check_compat_char_binary,
   convert_array< Hlongvarbinary, Hvarchar >},
  {NDBCOL::Longvarbinary,  NDBCOL::Longvarchar,    check_compat_char_binary,
   convert_array< Hlongvarbinary, Hlongvarchar >},
 
  // char to text promotions (uses staging table)
  {NDBCOL::Char,           NDBCOL::Text,           check_compat_char_to_text,
   NULL},
  {NDBCOL::Varchar,        NDBCOL::Text,           check_compat_char_to_text,
   NULL},
  {NDBCOL::Longvarchar,    NDBCOL::Text,           check_compat_char_to_text,
   NULL},
 
  // text to char promotions (uses staging table)
  {NDBCOL::Text,           NDBCOL::Char,           check_compat_text_to_char,
   NULL},
  {NDBCOL::Text,           NDBCOL::Varchar,        check_compat_text_to_char,
   NULL},
  {NDBCOL::Text,           NDBCOL::Longvarchar,    check_compat_text_to_char,
   NULL},

  // text to text promotions (uses staging table)
  // required when part lengths of text columns are not equal 
  {NDBCOL::Text,           NDBCOL::Text,           check_compat_text_to_text,
   NULL},

  // integral promotions
  {NDBCOL::Tinyint,        NDBCOL::Smallint,       check_compat_promotion,
   convert_integral< Hint8, Hint16>},
  {NDBCOL::Tinyint,        NDBCOL::Mediumint,      check_compat_promotion,
   convert_integral< Hint8, Hint24>},
  {NDBCOL::Tinyint,        NDBCOL::Int,            check_compat_promotion,
   convert_integral< Hint8, Hint32>},
  {NDBCOL::Tinyint,        NDBCOL::Bigint,         check_compat_promotion,
   convert_integral< Hint8, Hint64>},
  {NDBCOL::Smallint,       NDBCOL::Mediumint,      check_compat_promotion,
   convert_integral< Hint16, Hint24>},
  {NDBCOL::Smallint,       NDBCOL::Int,            check_compat_promotion,
   convert_integral< Hint16, Hint32>},
  {NDBCOL::Smallint,       NDBCOL::Bigint,         check_compat_promotion,
   convert_integral< Hint16, Hint64>},
  {NDBCOL::Mediumint,      NDBCOL::Int,            check_compat_promotion,
   convert_integral< Hint24, Hint32>},
  {NDBCOL::Mediumint,      NDBCOL::Bigint,         check_compat_promotion,
   convert_integral< Hint24, Hint64>},
  {NDBCOL::Int,            NDBCOL::Bigint,         check_compat_promotion,
   convert_integral< Hint32, Hint64>},
  {NDBCOL::Tinyunsigned,   NDBCOL::Smallunsigned,  check_compat_promotion,
   convert_integral< Huint8, Huint16>},
  {NDBCOL::Tinyunsigned,   NDBCOL::Mediumunsigned, check_compat_promotion,
   convert_integral< Huint8, Huint24>},
  {NDBCOL::Tinyunsigned,   NDBCOL::Unsigned,       check_compat_promotion,
   convert_integral< Huint8, Huint32>},
  {NDBCOL::Tinyunsigned,   NDBCOL::Bigunsigned,    check_compat_promotion,
   convert_integral< Huint8, Huint64>},
  {NDBCOL::Smallunsigned,  NDBCOL::Mediumunsigned, check_compat_promotion,
   convert_integral< Huint16, Huint24>},
  {NDBCOL::Smallunsigned,  NDBCOL::Unsigned,       check_compat_promotion,
   convert_integral< Huint16, Huint32>},
  {NDBCOL::Smallunsigned,  NDBCOL::Bigunsigned,    check_compat_promotion,
   convert_integral< Huint16, Huint64>},
  {NDBCOL::Mediumunsigned, NDBCOL::Unsigned,       check_compat_promotion,
   convert_integral< Huint24, Huint32>},
  {NDBCOL::Mediumunsigned, NDBCOL::Bigunsigned,    check_compat_promotion,
   convert_integral< Huint24, Huint64>},
  {NDBCOL::Unsigned,       NDBCOL::Bigunsigned,    check_compat_promotion,
   convert_integral< Huint32, Huint64>},

  // integral demotions
  {NDBCOL::Smallint,       NDBCOL::Tinyint,        check_compat_lossy,
   convert_integral< Hint16, Hint8>},
  {NDBCOL::Mediumint,      NDBCOL::Tinyint,        check_compat_lossy,
   convert_integral< Hint24, Hint8>},
  {NDBCOL::Mediumint,      NDBCOL::Smallint,       check_compat_lossy,
   convert_integral< Hint24, Hint16>},
  {NDBCOL::Int,            NDBCOL::Tinyint,        check_compat_lossy,
   convert_integral< Hint32, Hint8>},
  {NDBCOL::Int,            NDBCOL::Smallint,       check_compat_lossy,
   convert_integral< Hint32, Hint16>},
  {NDBCOL::Int,            NDBCOL::Mediumint,      check_compat_lossy,
   convert_integral< Hint32, Hint24>},
  {NDBCOL::Bigint,         NDBCOL::Tinyint,        check_compat_lossy,
   convert_integral< Hint64, Hint8>},
  {NDBCOL::Bigint,         NDBCOL::Smallint,       check_compat_lossy,
   convert_integral< Hint64, Hint16>},
  {NDBCOL::Bigint,         NDBCOL::Mediumint,      check_compat_lossy,
   convert_integral< Hint64, Hint24>},
  {NDBCOL::Bigint,         NDBCOL::Int,            check_compat_lossy,
   convert_integral< Hint64, Hint32>},
  {NDBCOL::Smallunsigned,  NDBCOL::Tinyunsigned,   check_compat_lossy,
   convert_integral< Huint16, Huint8>},
  {NDBCOL::Mediumunsigned, NDBCOL::Tinyunsigned,   check_compat_lossy,
   convert_integral< Huint24, Huint8>},
  {NDBCOL::Mediumunsigned, NDBCOL::Smallunsigned,  check_compat_lossy,
   convert_integral< Huint24, Huint16>},
  {NDBCOL::Unsigned,       NDBCOL::Tinyunsigned,   check_compat_lossy,
   convert_integral< Huint32, Huint8>},
  {NDBCOL::Unsigned,       NDBCOL::Smallunsigned,  check_compat_lossy,
   convert_integral< Huint32, Huint16>},
  {NDBCOL::Unsigned,       NDBCOL::Mediumunsigned, check_compat_lossy,
   convert_integral< Huint32, Huint24>},
  {NDBCOL::Bigunsigned,    NDBCOL::Tinyunsigned,   check_compat_lossy,
   convert_integral< Huint64, Huint8>},
  {NDBCOL::Bigunsigned,    NDBCOL::Smallunsigned,  check_compat_lossy,
   convert_integral< Huint64, Huint16>},
  {NDBCOL::Bigunsigned,    NDBCOL::Mediumunsigned, check_compat_lossy,
   convert_integral< Huint64, Huint24>},
  {NDBCOL::Bigunsigned,    NDBCOL::Unsigned,       check_compat_lossy,
   convert_integral< Huint64, Huint32>},

  // integral signedness conversions
  {NDBCOL::Tinyint,        NDBCOL::Tinyunsigned,   check_compat_lossy,
   convert_integral< Hint8, Huint8>},
  {NDBCOL::Smallint,       NDBCOL::Smallunsigned,  check_compat_lossy,
   convert_integral< Hint16, Huint16>},
  {NDBCOL::Mediumint,      NDBCOL::Mediumunsigned, check_compat_lossy,
   convert_integral< Hint24, Huint24>},
  {NDBCOL::Int,            NDBCOL::Unsigned,       check_compat_lossy,
   convert_integral< Hint32, Huint32>},
  {NDBCOL::Bigint,         NDBCOL::Bigunsigned,    check_compat_lossy,
   convert_integral< Hint64, Huint64>},
  {NDBCOL::Tinyunsigned,   NDBCOL::Tinyint,        check_compat_lossy,
   convert_integral< Huint8, Hint8>},
  {NDBCOL::Smallunsigned,  NDBCOL::Smallint,       check_compat_lossy,
   convert_integral< Huint16, Hint16>},
  {NDBCOL::Mediumunsigned, NDBCOL::Mediumint,      check_compat_lossy,
   convert_integral< Huint24, Hint24>},
  {NDBCOL::Unsigned,       NDBCOL::Int,            check_compat_lossy,
   convert_integral< Huint32, Hint32>},
  {NDBCOL::Bigunsigned,    NDBCOL::Bigint,         check_compat_lossy,
   convert_integral< Huint64, Hint64>},

  // integral signedness+promotion conversions
  {NDBCOL::Tinyint,        NDBCOL::Smallunsigned,  check_compat_lossy,
   convert_integral< Hint8, Huint16>},
  {NDBCOL::Tinyint,        NDBCOL::Mediumunsigned, check_compat_lossy,
   convert_integral< Hint8, Huint24>},
  {NDBCOL::Tinyint,        NDBCOL::Unsigned,       check_compat_lossy,
   convert_integral< Hint8, Huint32>},
  {NDBCOL::Tinyint,        NDBCOL::Bigunsigned,    check_compat_lossy,
   convert_integral< Hint8, Huint64>},
  {NDBCOL::Smallint,       NDBCOL::Mediumunsigned, check_compat_lossy,
   convert_integral< Hint16, Huint24>},
  {NDBCOL::Smallint,       NDBCOL::Unsigned,       check_compat_lossy,
   convert_integral< Hint16, Huint32>},
  {NDBCOL::Smallint,       NDBCOL::Bigunsigned,    check_compat_lossy,
   convert_integral< Hint16, Huint64>},
  {NDBCOL::Mediumint,      NDBCOL::Unsigned,       check_compat_lossy,
   convert_integral< Hint24, Huint32>},
  {NDBCOL::Mediumint,      NDBCOL::Bigunsigned,    check_compat_lossy,
   convert_integral< Hint24, Huint64>},
  {NDBCOL::Int,            NDBCOL::Bigunsigned,    check_compat_lossy,
   convert_integral< Hint32, Huint64>},
  {NDBCOL::Tinyunsigned,   NDBCOL::Smallint,       check_compat_lossy,
   convert_integral< Huint8, Hint16>},
  {NDBCOL::Tinyunsigned,   NDBCOL::Mediumint,      check_compat_lossy,
   convert_integral< Huint8, Hint24>},
  {NDBCOL::Tinyunsigned,   NDBCOL::Int,            check_compat_lossy,
   convert_integral< Huint8, Hint32>},
  {NDBCOL::Tinyunsigned,   NDBCOL::Bigint,         check_compat_lossy,
   convert_integral< Huint8, Hint64>},
  {NDBCOL::Smallunsigned,  NDBCOL::Mediumint,      check_compat_lossy,
   convert_integral< Huint16, Hint24>},
  {NDBCOL::Smallunsigned,  NDBCOL::Int,            check_compat_lossy,
   convert_integral< Huint16, Hint32>},
  {NDBCOL::Smallunsigned,  NDBCOL::Bigint,         check_compat_lossy,
   convert_integral< Huint16, Hint64>},
  {NDBCOL::Mediumunsigned, NDBCOL::Int,            check_compat_lossy,
   convert_integral< Huint24, Hint32>},
  {NDBCOL::Mediumunsigned, NDBCOL::Bigint,         check_compat_lossy,
   convert_integral< Huint24, Hint64>},
  {NDBCOL::Unsigned,       NDBCOL::Bigint,         check_compat_lossy,
   convert_integral< Huint32, Hint64>},

  // integral signedness+demotion conversions
  {NDBCOL::Smallint,       NDBCOL::Tinyunsigned,   check_compat_lossy,
   convert_integral< Hint16, Huint8>},
  {NDBCOL::Mediumint,      NDBCOL::Tinyunsigned,   check_compat_lossy,
   convert_integral< Hint24, Huint8>},
  {NDBCOL::Mediumint,      NDBCOL::Smallunsigned,  check_compat_lossy,
   convert_integral< Hint24, Huint16>},
  {NDBCOL::Int,            NDBCOL::Tinyunsigned,   check_compat_lossy,
   convert_integral< Hint32, Huint8>},
  {NDBCOL::Int,            NDBCOL::Smallunsigned,  check_compat_lossy,
   convert_integral< Hint32, Huint16>},
  {NDBCOL::Int,            NDBCOL::Mediumunsigned, check_compat_lossy,
   convert_integral< Hint32, Huint24>},
  {NDBCOL::Bigint,         NDBCOL::Tinyunsigned,   check_compat_lossy,
   convert_integral< Hint64, Huint8>},
  {NDBCOL::Bigint,         NDBCOL::Smallunsigned,  check_compat_lossy,
   convert_integral< Hint64, Huint16>},
  {NDBCOL::Bigint,         NDBCOL::Mediumunsigned, check_compat_lossy,
   convert_integral< Hint64, Huint24>},
  {NDBCOL::Bigint,         NDBCOL::Unsigned,       check_compat_lossy,
   convert_integral< Hint64, Huint32>},
  {NDBCOL::Smallunsigned,  NDBCOL::Tinyint,        check_compat_lossy,
   convert_integral< Huint16, Hint8>},
  {NDBCOL::Mediumunsigned, NDBCOL::Tinyint,        check_compat_lossy,
   convert_integral< Huint24, Hint8>},
  {NDBCOL::Mediumunsigned, NDBCOL::Smallint,       check_compat_lossy,
   convert_integral< Huint24, Hint16>},
  {NDBCOL::Unsigned,       NDBCOL::Tinyint,        check_compat_lossy,
   convert_integral< Huint32, Hint8>},
  {NDBCOL::Unsigned,       NDBCOL::Smallint,       check_compat_lossy,
   convert_integral< Huint32, Hint16>},
  {NDBCOL::Unsigned,       NDBCOL::Mediumint,      check_compat_lossy,
   convert_integral< Huint32, Hint24>},
  {NDBCOL::Bigunsigned,    NDBCOL::Tinyint,        check_compat_lossy,
   convert_integral< Huint64, Hint8>},
  {NDBCOL::Bigunsigned,    NDBCOL::Smallint,       check_compat_lossy,
   convert_integral< Huint64, Hint16>},
  {NDBCOL::Bigunsigned,    NDBCOL::Mediumint,      check_compat_lossy,
   convert_integral< Huint64, Hint24>},
  {NDBCOL::Bigunsigned,    NDBCOL::Int,            check_compat_lossy,
   convert_integral< Huint64, Hint32>},

  // times with fractional seconds
  {NDBCOL::Time,           NDBCOL::Time2,          check_compat_precision,
   convert_time_time2},
  {NDBCOL::Time2,          NDBCOL::Time,           check_compat_precision,
   convert_time2_time},
  {NDBCOL::Time2,          NDBCOL::Time2,          check_compat_precision,
   convert_time2_time2},
  {NDBCOL::Datetime,       NDBCOL::Datetime2,      check_compat_precision,
   convert_datetime_datetime2},
  {NDBCOL::Datetime2,      NDBCOL::Datetime,       check_compat_precision,
   convert_datetime2_datetime},
  {NDBCOL::Datetime2,      NDBCOL::Datetime2,      check_compat_precision,
   convert_datetime2_datetime2},
  {NDBCOL::Timestamp,      NDBCOL::Timestamp2,     check_compat_precision,
   convert_timestamp_timestamp2},
  {NDBCOL::Timestamp2,     NDBCOL::Timestamp,      check_compat_precision,
   convert_timestamp2_timestamp},
  {NDBCOL::Timestamp2,     NDBCOL::Timestamp2,     check_compat_precision,
   convert_timestamp2_timestamp2},

  {NDBCOL::Undefined,      NDBCOL::Undefined,      NULL,                  NULL}
};

bool
BackupRestore::init(Uint32 tableChangesMask)
{
  release();

  if (!m_restore && !m_restore_meta && !m_restore_epoch &&
      !m_rebuild_indexes && !m_disable_indexes)
    return true;

  m_tableChangesMask = tableChangesMask;
  m_cluster_connection = new Ndb_cluster_connection(m_ndb_connectstring,
                                                    m_ndb_nodeid);
  if (m_cluster_connection == NULL)
  {
    err << "Failed to create cluster connection!!" << endl;
    return false;
  }
  m_cluster_connection->set_name(g_options.c_str());
  if(m_cluster_connection->connect(m_ndb_connect_retries, m_ndb_connect_retry_delay, 1) != 0)
  {
    return false;
  }

  m_ndb = new Ndb(m_cluster_connection);

  if (m_ndb == NULL)
    return false;
  
  m_ndb->init(1024);
  if (m_ndb->waitUntilReady(30) != 0)
  {
    err << "Failed to connect to ndb!!" << endl;
    return false;
  }
  info << "Connected to ndb!!" << endl;

  m_callback = new restore_callback_t[m_parallelism];

  if (m_callback == 0)
  {
    err << "Failed to allocate callback structs" << endl;
    return false;
  }

  m_free_callback= m_callback;
  for (Uint32 i= 0; i < m_parallelism; i++) {
    m_callback[i].restore= this;
    m_callback[i].connection= 0;
    if (i > 0)
      m_callback[i-1].next= &(m_callback[i]);
  }
  m_callback[m_parallelism-1].next = 0;

  return true;
}

void BackupRestore::release()
{
  if (m_ndb)
  {
    delete m_ndb;
    m_ndb= 0;
  }

  if (m_callback)
  {
    delete [] m_callback;
    m_callback= 0;
  }

  if (m_cluster_connection)
  {
    delete m_cluster_connection;
    m_cluster_connection= 0;
  }
}

BackupRestore::~BackupRestore()
{
  release();
}

static
int 
match_blob(const char * name){
  int cnt, id1, id2;
  char buf[256];
  if((cnt = sscanf(name, "%[^/]/%[^/]/NDB$BLOB_%d_%d", buf, buf, &id1, &id2)) == 4){
    return id1;
  }
  
  return -1;
}

/**
 * Extracts the database, schema, and table name from an internal table name;
 * prints an error message and returns false in case of a format violation.
 */
static
bool
dissect_table_name(const char * qualified_table_name,
                   BaseString & db_name,
                   BaseString & schema_name,
                   BaseString & table_name) {
  Vector<BaseString> split;
  BaseString tmp(qualified_table_name);
  if (tmp.split(split, "/") != 3) {
    err << "Invalid table name format `" << qualified_table_name
        << "`" << endl;
    return false;
  }
  db_name = split[0];
  schema_name = split[1];
  table_name = split[2];
  return true;
}

/**
 * Similar method for index, only last component is relevant.
 */
static
bool
dissect_index_name(const char * qualified_index_name,
                   BaseString & db_name,
                   BaseString & schema_name,
                   BaseString & index_name) {
  Vector<BaseString> split;
  BaseString tmp(qualified_index_name);
  if (tmp.split(split, "/") != 4) {
    err << "Invalid index name format `" << qualified_index_name
        << "`" << endl;
    return false;
  }
  db_name = split[0];
  schema_name = split[1];
  index_name = split[3];
  return true;
}

/**
 * Assigns the new name for a database, if and only if to be rewritten.
 */
static
void
check_rewrite_database(BaseString & db_name) {
  const char * new_db_name;
  if (g_rewrite_databases.get(db_name.c_str(), &new_db_name))
    db_name.assign(new_db_name);
}

const NdbDictionary::Table*
BackupRestore::get_table(const TableS & tableS){
  const NdbDictionary::Table * tab = tableS.m_dictTable;
  if(m_cache.m_old_table == tab)
    return m_cache.m_new_table;
  m_cache.m_old_table = tab;

  int cnt, id1, id2;
  char db[256], schema[256];
  if (strcmp(tab->getName(), "SYSTAB_0") == 0 ||
      strcmp(tab->getName(), "sys/def/SYSTAB_0") == 0) {
    /*
      Restore SYSTAB_0 to itself
    */
    m_cache.m_new_table = tab;
  }
  else if((cnt = sscanf(tab->getName(), "%[^/]/%[^/]/NDB$BLOB_%d_%d", 
                        db, schema, &id1, &id2)) == 4){
    m_ndb->setDatabaseName(db);
    m_ndb->setSchemaName(schema);

    assert(tableS.getMainTable() != NULL);
    const TableS & mainTableS = *tableS.getMainTable();

    int mainColumnId = (int)tableS.getMainColumnId();
    assert(mainColumnId >= 0 && mainColumnId < mainTableS.getNoOfAttributes());

    const AttributeDesc & attr_desc =
      *mainTableS.getAttributeDesc(mainColumnId);
    
    BaseString::snprintf(db, sizeof(db), "NDB$BLOB_%d_%d", 
			 m_new_tables[id1]->getTableId(), attr_desc.attrId);
    
    m_cache.m_new_table = m_ndb->getDictionary()->getTable(db);
    
  } else {
    m_cache.m_new_table = m_new_tables[tab->getTableId()];
  }
  assert(m_cache.m_new_table);
  return m_cache.m_new_table;
}

// create staging table
bool
BackupRestore::prepare_staging(const TableS & tableS)
{
  NdbDictionary::Dictionary* dict = m_ndb->getDictionary();

  NdbDictionary::Table* stagingTable = tableS.m_stagingTable;
  const BaseString& stagingName = tableS.m_stagingName;

  const char* tablename = stagingName.c_str();
  BaseString db_name, schema_name, table_name;
  if (!dissect_table_name(tablename, db_name, schema_name, table_name)) {
    return false;
  }
  stagingTable->setName(table_name.c_str());

  // using defaults
  const Ndb_move_data::Opts::Tries ot;
  int createtries = 0;
  int createdelay = 0;
  while (1)
  {
    if (!(ot.maxtries == 0 || createtries < ot.maxtries))
    {
      err << "Create table " << tablename
          << ": too many temporary errors: " << createtries << endl;
      return false;
    }
    createtries++;

    m_ndb->setDatabaseName(db_name.c_str());
    m_ndb->setSchemaName(schema_name.c_str());
    if (dict->createTable(*stagingTable) != 0)
    {
      const NdbError& error = dict->getNdbError();
      if (error.status != NdbError::TemporaryError)
      {
        err << "Error: Failed to create staging source " << tablename
            << ": " << error << endl;
        return false;
      }
      err << "Temporary: Failed to create staging source " << tablename
          << ": " << error << endl;

      createdelay *= 2;
      if (createdelay < ot.mindelay)
        createdelay = ot.mindelay;
      if (createdelay > ot.maxdelay)
        createdelay = ot.maxdelay;

      info << "Sleeping " << createdelay << "ms" << endl;
      NdbSleep_MilliSleep(createdelay);
      continue;
    }
    info << "Created staging source " << tablename << endl;
    break;
  }

  const NdbDictionary::Table* tab = dict->getTable(table_name.c_str());
  if (tab == NULL)
  {
    err << "Unable to find table '" << tablename << "'"
        << " error : " << dict->getNdbError() << endl;
  }

  const NdbDictionary::Table* null = 0;
  m_new_tables.fill(tableS.m_dictTable->getTableId(), null);
  m_new_tables[tableS.m_dictTable->getTableId()] = tab;
  m_n_tables++; //??
  return true;
}

// move rows from staging to real and drop staging
bool
BackupRestore::finalize_staging(const TableS & tableS)
{
  NdbDictionary::Dictionary* dict = m_ndb->getDictionary();

  const NdbDictionary::Table* source = 0;
  const NdbDictionary::Table* target = 0;

  const char* stablename = tableS.m_stagingName.c_str();
  BaseString sdb_name, sschema_name, stable_name;
  if (!dissect_table_name(stablename, sdb_name, sschema_name, stable_name)) {
    return false;
  }
  m_ndb->setDatabaseName(sdb_name.c_str());
  m_ndb->setSchemaName(sschema_name.c_str());
  source = dict->getTable(stable_name.c_str());
  if (source == 0)
  {
    err << "Failed to find staging source " << stablename
        << ": " << dict->getNdbError() << endl;
    return false;
  }

  const char* ttablename = tableS.getTableName();
  BaseString tdb_name, tschema_name, ttable_name;
  if (!dissect_table_name(ttablename, tdb_name, tschema_name, ttable_name)) {
    return false;
  }
  m_ndb->setDatabaseName(tdb_name.c_str());
  m_ndb->setSchemaName(tschema_name.c_str());
  target = dict->getTable(ttable_name.c_str());
  if (target == 0)
  {
    err << "Failed to find staging target " << ttablename
        << ": " << dict->getNdbError() << endl;
    return false;
  }

  Ndb_move_data md;
  const Ndb_move_data::Stat& stat = md.get_stat();

  if (md.init(source, target) != 0)
  {
    const Ndb_move_data::Error& error = md.get_error();
    err << "Move data " << stablename << " to " << ttablename
        << ": " << error << endl;
    return false;
  }

  md.set_opts_flags(tableS.m_stagingFlags);

  // using defaults
  const Ndb_move_data::Opts::Tries ot;
  int tries = 0;
  int delay = 0;
  while (1)
  {
    if (!(ot.maxtries == 0 || tries < ot.maxtries))
    {
      err << "Move data " << stablename << " to " << ttablename
          << ": too many temporary errors: " << tries << endl;
      return false;
    }
    tries++;

    if (md.move_data(m_ndb) != 0)
    {
      const Ndb_move_data::Error& error = md.get_error();
      err
          << "Move data " << stablename << " to " << ttablename << " "
          << (error.is_temporary() ? "temporary error" : "permanent error")
          << " at try " << tries // default is no limit
          << " at rows moved " << stat.rows_moved
          << " total " << stat.rows_total
          << ": " << error << endl;

      if (!error.is_temporary())
        return false;

      if (stat.rows_moved == 0) // this try
        delay *= 2;
      else
        delay /= 2;
      if (delay < ot.mindelay)
        delay = ot.mindelay;
      if (delay > ot.maxdelay)
        delay = ot.maxdelay;

      info << "Sleeping " << delay << "ms" << endl;
      NdbSleep_MilliSleep(delay);
      continue;
    }

    info << "Successfully staged " << ttablename << ","
         << " moved all " << stat.rows_total << " rows" << endl;
    if ((tableS.m_stagingFlags & Ndb_move_data::Opts::MD_ATTRIBUTE_DEMOTION)
        || stat.truncated != 0) // just in case
      info << "Truncated " << stat.truncated << " attribute values" << endl;
    break;
  }

  int droptries = 0;
  int dropdelay = 0;
  while (1)
  {
    if (!(ot.maxtries == 0 || droptries < ot.maxtries))
    {
      err << "Drop table " << stablename
          << ": too many temporary errors: " << droptries << endl;
      return false;
    }
    droptries++;

    // dropTable(const Table&) is not defined ...
    m_ndb->setDatabaseName(sdb_name.c_str());
    m_ndb->setSchemaName(sschema_name.c_str());
    if (dict->dropTable(stable_name.c_str()) != 0)
    {
      const NdbError& error = dict->getNdbError();
      if (error.status != NdbError::TemporaryError)
      {
        err << "Error: Failed to drop staging source " << stablename
            << ": " << error << endl;
        return false;
      }
      err << "Temporary: Failed to drop staging source " << stablename
          << ": " << error << endl;

      dropdelay *= 2;
      if (dropdelay < ot.mindelay)
        dropdelay = ot.mindelay;
      if (dropdelay > ot.maxdelay)
        dropdelay = ot.maxdelay;

      info << "Sleeping " << dropdelay << "ms" << endl;
      NdbSleep_MilliSleep(dropdelay);
      continue;
    }
    info << "Dropped staging source " << stablename << endl;
    break;
  }
  return true;
}

bool
BackupRestore::finalize_table(const TableS & table){
  bool ret= true;
  if (!m_restore && !m_restore_meta)
    return ret;
  if (!table.have_auto_inc())
    return ret;
  // no point for staging table and below code as such would crash
  if (table.m_staging)
    return ret;

  Uint64 max_val= table.get_max_auto_val();
  do
  {
    Uint64 auto_val = ~(Uint64)0;
    int r= m_ndb->readAutoIncrementValue(get_table(table), auto_val);
    if (r == -1 && m_ndb->getNdbError().status == NdbError::TemporaryError)
    {
      NdbSleep_MilliSleep(50);
      continue; // retry
    }
    else if (r == -1 && m_ndb->getNdbError().code != 626)
    {
      ret= false;
    }
    else if ((r == -1 && m_ndb->getNdbError().code == 626) ||
             max_val+1 > auto_val || auto_val == ~(Uint64)0)
    {
      r= m_ndb->setAutoIncrementValue(get_table(table),
                                      max_val+1, false);
      if (r == -1 &&
            m_ndb->getNdbError().status == NdbError::TemporaryError)
      {
        NdbSleep_MilliSleep(50);
        continue; // retry
      }
      ret = (r == 0);
    }
    return (ret);
  } while (1);
}

bool
BackupRestore::rebuild_indexes(const TableS& table)
{
  const char *tablename = table.getTableName();

  const NdbDictionary::Table * tab = get_table(table);
  Uint32 id = tab->getObjectId();
  if (m_index_per_table.size() <= id)
    return true;

  BaseString db_name, schema_name, table_name;
  if (!dissect_table_name(tablename, db_name, schema_name, table_name)) {
    return false;
  }
  check_rewrite_database(db_name);

  m_ndb->setDatabaseName(db_name.c_str());
  m_ndb->setSchemaName(schema_name.c_str());
  NdbDictionary::Dictionary* dict = m_ndb->getDictionary();

  Vector<NdbDictionary::Index*> & indexes = m_index_per_table[id];
  for(unsigned i = 0; i<indexes.size(); i++)
  {
    const NdbDictionary::Index * const idx = indexes[i];
    const char * const idx_name = idx->getName();
    const char * const tab_name = idx->getTable();
    const NDB_TICKS start = NdbTick_getCurrentTicks();
    info << "Rebuilding index `" << idx_name << "` on table `"
      << tab_name << "` ..." << flush;
    bool done = false;
    for(int retries = 0; retries<11; retries++)
    {
      if ((dict->getIndex(idx_name, tab_name) == NULL)
          && (dict->createIndex(* idx, 1) != 0))
      {
        if(dict->getNdbError().status == NdbError::TemporaryError)
        {
          err << "retry sleep 50 ms on error " <<
                      dict->getNdbError().code << endl;
          NdbSleep_MilliSleep(50);
          continue;  // retry on temporary error
        }
        else
        {
          break;
        }
      }
      else
      {
        done = true;
        break;
      }
    }
    if(!done)
    {
      info << "FAIL!" << endl;
      err << "Rebuilding index `" << idx_name << "` on table `"
        << tab_name <<"` failed: ";
      err << dict->getNdbError() << endl;
      return false;
    }
    const NDB_TICKS stop = NdbTick_getCurrentTicks();
    const Uint64 elapsed = NdbTick_Elapsed(start,stop).seconds();
    info << "OK (" << elapsed << "s)" <<endl;
  }

  return true;
}

#ifdef NOT_USED
static bool default_nodegroups(NdbDictionary::Table *table)
{
  Uint16 *node_groups = (Uint16*)table->getFragmentData();
  Uint32 no_parts = table->getFragmentDataLen() >> 1;
  Uint32 i;

  if (node_groups[0] != 0)
    return false; 
  for (i = 1; i < no_parts; i++) 
  {
    if (node_groups[i] != NDB_UNDEF_NODEGROUP)
      return false;
  }
  return true;
}
#endif


static Uint32 get_no_fragments(Uint64 max_rows, Uint32 no_nodes)
{
  Uint32 i = 0;
  Uint32 acc_row_size = 27;
  Uint32 acc_fragment_size = 512*1024*1024;
  Uint32 no_parts= Uint32((max_rows*acc_row_size)/acc_fragment_size + 1);
  Uint32 reported_parts = no_nodes; 
  while (reported_parts < no_parts && ++i < 4 &&
         (reported_parts + no_parts) < MAX_NDB_PARTITIONS)
    reported_parts+= no_nodes;
  if (reported_parts < no_parts)
  {
    err << "Table will be restored but will not be able to handle the maximum";
    err << " amount of rows as requested" << endl;
  }
  return reported_parts;
}


static void set_default_nodegroups(NdbDictionary::Table *table)
{
  Uint32 no_parts = table->getFragmentCount();
  Uint32 node_group[MAX_NDB_PARTITIONS];
  Uint32 i;

  node_group[0] = 0;
  for (i = 1; i < no_parts; i++)
  {
    node_group[i] = NDB_UNDEF_NODEGROUP;
  }
  table->setFragmentData(node_group, no_parts);
}

Uint32 BackupRestore::map_ng(Uint32 ng)
{
  NODE_GROUP_MAP *ng_map = m_nodegroup_map;

  if (ng == NDB_UNDEF_NODEGROUP ||
      ng_map[ng].map_array[0] == NDB_UNDEF_NODEGROUP)
  {
    return ng;
  }
  else
  {
    Uint32 new_ng;
    Uint32 curr_inx = ng_map[ng].curr_index;
    Uint32 new_curr_inx = curr_inx + 1;

    assert(ng < MAX_NDB_PARTITIONS);
    assert(curr_inx < MAX_MAPS_PER_NODE_GROUP);
    assert(new_curr_inx < MAX_MAPS_PER_NODE_GROUP);

    if (new_curr_inx >= MAX_MAPS_PER_NODE_GROUP)
      new_curr_inx = 0;
    else if (ng_map[ng].map_array[new_curr_inx] == NDB_UNDEF_NODEGROUP)
      new_curr_inx = 0;
    new_ng = ng_map[ng].map_array[curr_inx];
    ng_map[ng].curr_index = new_curr_inx;
    return new_ng;
  }
}


bool BackupRestore::map_nodegroups(Uint32 *ng_array, Uint32 no_parts)
{
  Uint32 i;
  bool mapped = FALSE;
  DBUG_ENTER("map_nodegroups");

  assert(no_parts < MAX_NDB_PARTITIONS);
  for (i = 0; i < no_parts; i++)
  {
    Uint32 ng;
    ng = map_ng(ng_array[i]);
    if (ng != ng_array[i])
      mapped = TRUE;
    ng_array[i] = ng;
  }
  DBUG_RETURN(mapped);
}


static void copy_byte(const char **data, char **new_data, uint *len)
{
  **new_data = **data;
  (*data)++;
  (*new_data)++;
  (*len)++;
}


bool BackupRestore::search_replace(char *search_str, char **new_data,
                                   const char **data, const char *end_data,
                                   uint *new_data_len)
{
  uint search_str_len = (uint)strlen(search_str);
  uint inx = 0;
  bool in_delimiters = FALSE;
  bool escape_char = FALSE;
  char start_delimiter = 0;
  DBUG_ENTER("search_replace");

  do
  {
    char c = **data;
    copy_byte(data, new_data, new_data_len);
    if (escape_char)
    {
      escape_char = FALSE;
    }
    else if (in_delimiters)
    {
      if (c == start_delimiter)
        in_delimiters = FALSE;
    }
    else if (c == '\'' || c == '\"')
    {
      in_delimiters = TRUE;
      start_delimiter = c;
    }
    else if (c == '\\')
    {
      escape_char = TRUE;
    }
    else if (c == search_str[inx])
    {
      inx++;
      if (inx == search_str_len)
      {
        bool found = FALSE;
        uint number = 0;
        while (*data != end_data)
        {
          if (isdigit(**data))
          {
            found = TRUE;
            number = (10 * number) + (**data);
            if (number > MAX_NDB_NODES)
              break;
          }
          else if (found)
          {
            /*
               After long and tedious preparations we have actually found
               a node group identifier to convert. We'll use the mapping
               table created for node groups and then insert the new number
               instead of the old number.
            */
            uint temp = map_ng(number);
            int no_digits = 0;
            char digits[10];
            while (temp != 0)
            {
              digits[no_digits] = temp % 10;
              no_digits++;
              temp/=10;
            }
            for (no_digits--; no_digits >= 0; no_digits--)
            {
              **new_data = digits[no_digits];
              *new_data_len+=1;
            }
            DBUG_RETURN(FALSE); 
          }
          else
            break;
          (*data)++;
        }
        DBUG_RETURN(TRUE);
      }
    }
    else
      inx = 0;
  } while (*data < end_data);
  DBUG_RETURN(FALSE);
}

bool BackupRestore::map_in_frm(char *new_data, const char *data,
                                       uint data_len, uint *new_data_len)
{
  const char *end_data= data + data_len;
  const char *end_part_data;
  const char *part_data;
  char *extra_ptr;
  uint start_key_definition_len = uint2korr(data + 6);
  uint key_definition_len = uint4korr(data + 47);
  uint part_info_len;
  DBUG_ENTER("map_in_frm");

  if (data_len < 4096) goto error;
  extra_ptr = (char*)data + start_key_definition_len + key_definition_len;
  if ((int)data_len < ((extra_ptr - data) + 2)) goto error;
  extra_ptr = extra_ptr + 2 + uint2korr(extra_ptr);
  if ((int)data_len < ((extra_ptr - data) + 2)) goto error;
  extra_ptr = extra_ptr + 2 + uint2korr(extra_ptr);
  if ((int)data_len < ((extra_ptr - data) + 4)) goto error;
  part_info_len = uint4korr(extra_ptr);
  part_data = extra_ptr + 4;
  if ((int)data_len < ((part_data + part_info_len) - data)) goto error;
 
  do
  {
    copy_byte(&data, &new_data, new_data_len);
  } while (data < part_data);
  end_part_data = part_data + part_info_len;
  do
  {
    if (search_replace((char*)" NODEGROUP = ", &new_data, &data,
                       end_part_data, new_data_len))
      goto error;
  } while (data != end_part_data);
  do
  {
    copy_byte(&data, &new_data, new_data_len);
  } while (data < end_data);
  DBUG_RETURN(FALSE);
error:
  DBUG_RETURN(TRUE);
}


bool BackupRestore::translate_frm(NdbDictionary::Table *table)
{
  uchar *pack_data, *data, *new_pack_data;
  char *new_data;
  uint new_data_len;
  size_t data_len, new_pack_len;
  uint no_parts, extra_growth;
  DBUG_ENTER("translate_frm");

  pack_data = (uchar*) table->getFrmData();
  no_parts = table->getFragmentCount();
  /*
    Add max 4 characters per partition to handle worst case
    of mapping from single digit to 5-digit number.
    Fairly future-proof, ok up to 99999 node groups.
  */
  extra_growth = no_parts * 4;
  if (unpackfrm(&data, &data_len, pack_data))
  {
    DBUG_RETURN(TRUE);
  }
  if ((new_data = (char*) malloc(data_len + extra_growth)))
  {
    DBUG_RETURN(TRUE);
  }
  if (map_in_frm(new_data, (const char*)data, (uint)data_len, &new_data_len))
  {
    free(new_data);
    DBUG_RETURN(TRUE);
  }
  if (packfrm((uchar*) new_data, new_data_len,
              &new_pack_data, &new_pack_len))
  {
    free(new_data);
    DBUG_RETURN(TRUE);
  }
  table->setFrm(new_pack_data, (Uint32)new_pack_len);
  DBUG_RETURN(FALSE);
}

#include <signaldata/DictTabInfo.hpp>

bool
BackupRestore::object(Uint32 type, const void * ptr)
{
  if (!m_restore_meta)
    return true;
  
  NdbDictionary::Dictionary* dict = m_ndb->getDictionary();
  switch(type){
  case DictTabInfo::Tablespace:
  {
    NdbDictionary::Tablespace old(*(NdbDictionary::Tablespace*)ptr);

    Uint32 id = old.getObjectId();

    if (!m_no_restore_disk)
    {
      NdbDictionary::LogfileGroup * lg = m_logfilegroups[old.getDefaultLogfileGroupId()];
      old.setDefaultLogfileGroup(* lg);
      info << "Creating tablespace: " << old.getName() << "..." << flush;
      int ret = dict->createTablespace(old);
      if (ret)
      {
	NdbError errobj= dict->getNdbError();
	info << "FAILED" << endl;
        err << "Create tablespace failed: " << old.getName() << ": " << errobj << endl;
	return false;
      }
      info << "done" << endl;
    }
    
    NdbDictionary::Tablespace curr = dict->getTablespace(old.getName());
    NdbError errobj = dict->getNdbError();
    if ((int) errobj.classification == (int) ndberror_cl_none)
    {
      NdbDictionary::Tablespace* currptr = new NdbDictionary::Tablespace(curr);
      NdbDictionary::Tablespace * null = 0;
      m_tablespaces.set(currptr, id, null);
      debug << "Retreived tablespace: " << currptr->getName() 
	    << " oldid: " << id << " newid: " << currptr->getObjectId() 
	    << " " << (void*)currptr << endl;
      m_n_tablespace++;
      return true;
    }
    
    err << "Failed to retrieve tablespace \"" << old.getName() << "\": "
	<< errobj << endl;
    
    return false;
    break;
  }
  case DictTabInfo::LogfileGroup:
  {
    NdbDictionary::LogfileGroup old(*(NdbDictionary::LogfileGroup*)ptr);
    
    Uint32 id = old.getObjectId();
    
    if (!m_no_restore_disk)
    {
      info << "Creating logfile group: " << old.getName() << "..." << flush;
      int ret = dict->createLogfileGroup(old);
      if (ret)
      {
	NdbError errobj= dict->getNdbError();
	info << "FAILED" << endl;
        err << "Create logfile group failed: " << old.getName() << ": " << errobj << endl;
	return false;
      }
      info << "done" << endl;
    }
    
    NdbDictionary::LogfileGroup curr = dict->getLogfileGroup(old.getName());
    NdbError errobj = dict->getNdbError();
    if ((int) errobj.classification == (int) ndberror_cl_none)
    {
      NdbDictionary::LogfileGroup* currptr = 
	new NdbDictionary::LogfileGroup(curr);
      NdbDictionary::LogfileGroup * null = 0;
      m_logfilegroups.set(currptr, id, null);
      debug << "Retreived logfile group: " << currptr->getName() 
	    << " oldid: " << id << " newid: " << currptr->getObjectId() 
	    << " " << (void*)currptr << endl;
      m_n_logfilegroup++;
      return true;
    }
    
    err << "Failed to retrieve logfile group \"" << old.getName() << "\": "
	<< errobj << endl;
    
    return false;
    break;
  }
  case DictTabInfo::Datafile:
  {
    if (!m_no_restore_disk)
    {
      NdbDictionary::Datafile old(*(NdbDictionary::Datafile*)ptr);
      NdbDictionary::ObjectId objid;
      old.getTablespaceId(&objid);
      NdbDictionary::Tablespace * ts = m_tablespaces[objid.getObjectId()];
      debug << "Connecting datafile " << old.getPath() 
	    << " to tablespace: oldid: " << objid.getObjectId()
	    << " newid: " << ts->getObjectId() << endl;
      old.setTablespace(* ts);
      info << "Creating datafile \"" << old.getPath() << "\"..." << flush;
      if (dict->createDatafile(old))
      {
	NdbError errobj= dict->getNdbError();
	info << "FAILED" << endl;
        err << "Create datafile failed: " << old.getPath() << ": " << errobj << endl;
	return false;
      }
      info << "done" << endl;
      m_n_datafile++;
    }
    return true;
    break;
  }
  case DictTabInfo::Undofile:
  {
    if (!m_no_restore_disk)
    {
      NdbDictionary::Undofile old(*(NdbDictionary::Undofile*)ptr);
      NdbDictionary::ObjectId objid;
      old.getLogfileGroupId(&objid);
      NdbDictionary::LogfileGroup * lg = m_logfilegroups[objid.getObjectId()];
      debug << "Connecting undofile " << old.getPath() 
	    << " to logfile group: oldid: " << objid.getObjectId()
	    << " newid: " << lg->getObjectId() 
	    << " " << (void*)lg << endl;
      old.setLogfileGroup(* lg);
      info << "Creating undofile \"" << old.getPath() << "\"..." << flush;
      if (dict->createUndofile(old))
      {
	NdbError errobj= dict->getNdbError();
	info << "FAILED" << endl;
        err << "Create undofile failed: " << old.getPath() << ": " << errobj << endl;
	return false;
      }
      info << "done" << endl;
      m_n_undofile++;
    }
    return true;
    break;
  }
  case DictTabInfo::HashMap:
  {
    NdbDictionary::HashMap old(*(NdbDictionary::HashMap*)ptr);

    Uint32 id = old.getObjectId();

    if (m_restore_meta)
    {
      int ret = dict->createHashMap(old);
      if (ret == 0)
      {
        info << "Created hashmap: " << old.getName() << endl;
      }
      else
      {
        NdbError errobj = dict->getNdbError();
        // We ignore schema already exists, this is fine
        if (errobj.code != 721)
        {
          err << "Could not create hashmap \"" << old.getName() << "\": "
              << errobj << endl;
          return false;
        }
      }
    }

    NdbDictionary::HashMap curr;
    if (dict->getHashMap(curr, old.getName()) == 0)
    {
      NdbDictionary::HashMap* currptr =
        new NdbDictionary::HashMap(curr);
      NdbDictionary::HashMap * null = 0;
      m_hashmaps.set(currptr, id, null);
      debug << "Retreived hashmap: " << currptr->getName()
            << " oldid: " << id << " newid: " << currptr->getObjectId()
            << " " << (void*)currptr << endl;
      return true;
    }

    NdbError errobj = dict->getNdbError();
    err << "Failed to retrieve hashmap \"" << old.getName() << "\": "
	<< errobj << endl;

    return false;
  }
  case DictTabInfo::ForeignKey: // done after tables
  {
    return true;
  }
  default:
  {
    err << "Unknown object type: " << type << endl;
    break;
  }
  }
  return true;
}

bool
BackupRestore::has_temp_error(){
  return m_temp_error;
}

bool
BackupRestore::update_apply_status(const RestoreMetaData &metaData)
{
  if (!m_restore_epoch)
    return true;

  bool result= false;
  unsigned apply_table_format= 0;

  m_ndb->setDatabaseName(NDB_REP_DB);
  m_ndb->setSchemaName("def");

  NdbDictionary::Dictionary *dict= m_ndb->getDictionary();
  const NdbDictionary::Table *ndbtab= dict->getTable(NDB_APPLY_TABLE);
  if (!ndbtab)
  {
    err << NDB_APPLY_TABLE << ": "
	<< dict->getNdbError() << endl;
    return false;
  }
  if (ndbtab->getColumn(0)->getType() == NdbDictionary::Column::Unsigned &&
      ndbtab->getColumn(1)->getType() == NdbDictionary::Column::Bigunsigned)
  {
    if (ndbtab->getNoOfColumns() == 2)
    {
      apply_table_format= 1;
    }
    else if
      (ndbtab->getColumn(2)->getType() == NdbDictionary::Column::Varchar &&
       ndbtab->getColumn(3)->getType() == NdbDictionary::Column::Bigunsigned &&
       ndbtab->getColumn(4)->getType() == NdbDictionary::Column::Bigunsigned)
    {
      apply_table_format= 2;
    }
  }
  if (apply_table_format == 0)
  {
    err << NDB_APPLY_TABLE << " has wrong format\n";
    return false;
  }

  Uint32 server_id= 0;
  Uint64 epoch= Uint64(metaData.getStopGCP());
  Uint32 version= metaData.getNdbVersion();

  /**
   * Bug#XXX, stopGCP is not really stop GCP, but stopGCP - 1
   */
  epoch += 1;

  if (version >= NDBD_MICRO_GCP_63 ||
      (version >= NDBD_MICRO_GCP_62 && getMinor(version) == 2))
  {
    epoch<<= 32; // Only gci_hi is saved...

    /**
     * Backup contains all epochs with those top bits,
     * so we indicate that with max setting
     */
    epoch += (Uint64(1) << 32) - 1;
  }

  Uint64 zero= 0;
  char empty_string[1];
  empty_string[0]= 0;
  NdbTransaction * trans= m_ndb->startTransaction();
  if (!trans)
  {
    err << NDB_APPLY_TABLE << ": "
	<< m_ndb->getNdbError() << endl;
    return false;
  }
  NdbOperation * op= trans->getNdbOperation(ndbtab);
  if (!op)
  {
    err << NDB_APPLY_TABLE << ": "
	<< trans->getNdbError() << endl;
    goto err;
  }
  if (op->writeTuple() ||
      op->equal(0u, (const char *)&server_id, sizeof(server_id)) ||
      op->setValue(1u, (const char *)&epoch, sizeof(epoch)))
  {
    err << NDB_APPLY_TABLE << ": "
	<< op->getNdbError() << endl;
    goto err;
  }
  if ((apply_table_format == 2) &&
      (op->setValue(2u, (const char *)&empty_string, 1) ||
       op->setValue(3u, (const char *)&zero, sizeof(zero)) ||
       op->setValue(4u, (const char *)&zero, sizeof(zero))))
  {
    err << NDB_APPLY_TABLE << ": "
	<< op->getNdbError() << endl;
    goto err;
  }
  if (trans->execute(NdbTransaction::Commit))
  {
    err << NDB_APPLY_TABLE << ": "
	<< trans->getNdbError() << endl;
    goto err;
  }
  result= true;
err:
  m_ndb->closeTransaction(trans);
  return result;
}

bool
BackupRestore::report_started(unsigned backup_id, unsigned node_id)
{
  if (m_ndb)
  {
    Uint32 data[3];
    data[0]= NDB_LE_RestoreStarted;
    data[1]= backup_id;
    data[2]= node_id;
    Ndb_internal::send_event_report(false /* has lock */, m_ndb, data, 3);
  }
  return true;
}

bool
BackupRestore::report_meta_data(unsigned backup_id, unsigned node_id)
{
  if (m_ndb)
  {
    Uint32 data[8];
    data[0]= NDB_LE_RestoreMetaData;
    data[1]= backup_id;
    data[2]= node_id;
    data[3]= m_n_tables;
    data[4]= m_n_tablespace;
    data[5]= m_n_logfilegroup;
    data[6]= m_n_datafile;
    data[7]= m_n_undofile;
    Ndb_internal::send_event_report(false /* has lock */, m_ndb, data, 8);
  }
  return true;
}
bool
BackupRestore::report_data(unsigned backup_id, unsigned node_id)
{
  if (m_ndb)
  {
    Uint32 data[7];
    data[0]= NDB_LE_RestoreData;
    data[1]= backup_id;
    data[2]= node_id;
    data[3]= m_dataCount & 0xFFFFFFFF;
    data[4]= 0;
    data[5]= (Uint32)(m_dataBytes & 0xFFFFFFFF);
    data[6]= (Uint32)((m_dataBytes >> 32) & 0xFFFFFFFF);
    Ndb_internal::send_event_report(false /* has lock */, m_ndb, data, 7);
  }
  return true;
}

bool
BackupRestore::report_log(unsigned backup_id, unsigned node_id)
{
  if (m_ndb)
  {
    Uint32 data[7];
    data[0]= NDB_LE_RestoreLog;
    data[1]= backup_id;
    data[2]= node_id;
    data[3]= m_logCount & 0xFFFFFFFF;
    data[4]= 0;
    data[5]= (Uint32)(m_logBytes & 0xFFFFFFFF);
    data[6]= (Uint32)((m_logBytes >> 32) & 0xFFFFFFFF);
    Ndb_internal::send_event_report(false /* has lock */, m_ndb, data, 7);
  }
  return true;
}

bool
BackupRestore::report_completed(unsigned backup_id, unsigned node_id)
{
  if (m_ndb)
  {
    Uint32 data[3];
    data[0]= NDB_LE_RestoreCompleted;
    data[1]= backup_id;
    data[2]= node_id;
    Ndb_internal::send_event_report(false /* has lock */, m_ndb, data, 3);
  }
  return true;
}

bool
BackupRestore::column_compatible_check(const char* tableName, 
                                       const NDBCOL* backupCol, 
                                       const NDBCOL* dbCol)
{
  if (backupCol->equal(*dbCol))
    return true;

  /* Something is different between the columns, but some differences don't
   * matter.
   * Investigate which parts are different, and inform user
   */
  bool similarEnough = true;

  /* We check similar things to NdbColumnImpl::equal() here */
  if (strcmp(backupCol->getName(), dbCol->getName()) != 0)
  {
    info << "Column " << tableName << "." << backupCol->getName()
         << " has different name in DB (" << dbCol->getName() << ")"
         << endl;
    similarEnough = false;
  }
  
  if (backupCol->getType() != dbCol->getType())
  {
    info << "Column " << tableName << "." << backupCol->getName()
         << (" has different type in DB; promotion or lossy type conversion"
             " (demotion, signed/unsigned) may be required.") << endl;
    similarEnough = false;
  }

  if (backupCol->getPrimaryKey() != dbCol->getPrimaryKey())
  {
    info << "Column " << tableName << "." << backupCol->getName()
         << (dbCol->getPrimaryKey()?" is":" is not")
         << " a primary key in the DB." << endl;
    similarEnough = false;
  }
  else
  {
    if (backupCol->getPrimaryKey())
    {
      if (backupCol->getDistributionKey() != dbCol->getDistributionKey())
      {
        info << "Column " << tableName << "." << backupCol->getName()
             << (dbCol->getDistributionKey()?" is":" is not")
             << " a distribution key in the DB." << endl;
        /* Not a problem for restore though */
      }
    }
  }

  if (backupCol->getNullable() != dbCol->getNullable())
  {
    info << "Column " << tableName << "." << backupCol->getName()
         << (dbCol->getNullable()?" is":" is not")
         << " nullable in the DB." << endl;
    similarEnough = false;
  }

  if (backupCol->getPrecision() != dbCol->getPrecision())
  {
    info << "Column " << tableName << "." << backupCol->getName()
         << " precision is different in the DB" << endl;
    similarEnough = false;
  }

  if (backupCol->getScale() != dbCol->getScale())
  {
    info <<  "Column " << tableName << "." << backupCol->getName()
         << " scale is different in the DB" << endl;
    similarEnough = false;
  }

  if (backupCol->getLength() != dbCol->getLength())
  {
    info <<  "Column " << tableName << "." << backupCol->getName()
         << " length is different in the DB" << endl;
    similarEnough = false;
  }

  if (backupCol->getCharset() != dbCol->getCharset())
  {
    info <<  "Column " << tableName << "." << backupCol->getName()
         << " charset is different in the DB" << endl;
    similarEnough = false;
  }
  
  if (backupCol->getAutoIncrement() != dbCol->getAutoIncrement())
  {
    info << "Column " << tableName << "." << backupCol->getName()
         << (dbCol->getAutoIncrement()?" is":" is not")
         << " AutoIncrementing in the DB" << endl;
    /* TODO : Can this be ignored? */
    similarEnough = false;
  }
  
  {
    unsigned int backupDefaultLen, dbDefaultLen;
    const void *backupDefaultPtr, *dbDefaultPtr;
    backupDefaultPtr = backupCol->getDefaultValue(&backupDefaultLen);
    dbDefaultPtr = dbCol->getDefaultValue(&dbDefaultLen);
    
    if ((backupDefaultLen != dbDefaultLen) ||
        (memcmp(backupDefaultPtr, dbDefaultPtr, backupDefaultLen) != 0))
    {
      info << "Column " << tableName << "." << backupCol->getName()
           << " Default value is different in the DB" << endl;
      /* This doesn't matter */
    }
  }

  if (backupCol->getArrayType() != dbCol->getArrayType())
  {
    info << "Column " << tableName << "." << backupCol->getName()
         << " ArrayType is different in the DB" << endl;
    similarEnough = false;
  }

  if (backupCol->getStorageType() != dbCol->getStorageType())
  {
    info << "Column " << tableName << "." << backupCol->getName()
         << " Storagetype is different in the DB" << endl;
    /* This doesn't matter */
  }

  if (backupCol->getBlobVersion() != dbCol->getBlobVersion())
  {
    info << "Column " << tableName << "." << backupCol->getName()
         << " Blob version is different in the DB" << endl;
    similarEnough = false;
  }

  if (backupCol->getDynamic() != dbCol->getDynamic())
  {
    info << "Column " << tableName << "." << backupCol->getName()
         << (dbCol->getDynamic()?" is":" is not")
         << " Dynamic in the DB" << endl;
    /* This doesn't matter */
  }

  if (similarEnough)
    info << "  Difference(s) will be ignored during restore." << endl;
  else
    info << "  Difference(s) cannot be ignored.  Cannot restore this column as is." << endl;

  return similarEnough;
}

bool is_array(NDBCOL::Type type)
{
  if (type == NDBCOL::Char ||
      type == NDBCOL::Binary ||
      type == NDBCOL::Varchar ||
      type == NDBCOL::Varbinary ||
      type == NDBCOL::Longvarchar ||
      type == NDBCOL::Longvarbinary)
  {
    return true;
  }
  return false;
 
}

bool
BackupRestore::check_blobs(TableS & tableS)
{
   /**
   * Nothing to check when printing data
   */
  if (!m_restore) {
    return true;
  }

  /**
   * For blob tables, check if there is a conversion on any PK of the main table.
   * If there is, the blob table PK needs the same conversion as the main table PK.
   * Copy the conversion to the blob table. 
   */
  if(match_blob(tableS.getTableName()) == -1)
    return true;

  int mainColumnId = tableS.getMainColumnId();
  const TableS *mainTableS = tableS.getMainTable();
  if(mainTableS->m_dictTable->getColumn(mainColumnId)->getBlobVersion() == NDB_BLOB_V1)
    return true; /* only to make old ndb_restore_compat* tests on v1 blobs pass */

  /* check all PK columns in v2 blob table */
  for(int i=0; i<tableS.m_dictTable->getNoOfColumns(); i++)
  {
    NDBCOL *col = tableS.m_dictTable->getColumn(i);
    AttributeDesc *attrDesc = tableS.getAttributeDesc(col->getAttrId());
  
    /* get corresponding pk column in main table */
    NDBCOL *mainCol = mainTableS->m_dictTable->getColumn(col->getName());
    if(!mainCol || !mainCol->getPrimaryKey()) 
      return true; /* no more PKs */

    int mainTableAttrId = mainCol->getAttrId();
    AttributeDesc *mainTableAttrDesc = mainTableS->getAttributeDesc(mainTableAttrId);
    if(mainTableAttrDesc->convertFunc)
    {
      /* copy convertFunc from main table PK to blob table PK */
      attrDesc->convertFunc = mainTableAttrDesc->convertFunc;     
      attrDesc->parameter = malloc(mainTableAttrDesc->parameterSz);
      memcpy(attrDesc->parameter, mainTableAttrDesc->parameter, mainTableAttrDesc->parameterSz);
    }
  }
  return true;
}

bool
BackupRestore::table_compatible_check(TableS & tableS)
{
  if (!m_restore)
    return true;

  const char *tablename = tableS.getTableName();

  if(tableS.m_dictTable == NULL){
    ndbout<<"Table %s has no m_dictTable " << tablename << endl;
    return false;
  }
  /**
   * Ignore blob tables
   */
  if(match_blob(tablename) >= 0)
    return true;

  const NdbTableImpl & tmptab = NdbTableImpl::getImpl(* tableS.m_dictTable);
  if ((int) tmptab.m_indexType != (int) NdbDictionary::Index::Undefined)
  {
    if((int) tmptab.m_indexType == (int) NdbDictionary::Index::UniqueHashIndex)
    {
      BaseString dummy1, dummy2, indexname;
      dissect_index_name(tablename, dummy1, dummy2, indexname);
      ndbout << "WARNING: Table " << tmptab.m_primaryTable.c_str() << " contains unique index " << indexname.c_str() << ". ";
      ndbout << "This can cause ndb_restore failures with duplicate key errors while restoring data. ";
      ndbout << "To avoid duplicate key errors, use --disable-indexes before restoring data ";
      ndbout << "and --rebuild-indexes after data is restored." << endl;
    }
    return true;
  }

  BaseString db_name, schema_name, table_name;
  if (!dissect_table_name(tablename, db_name, schema_name, table_name)) {
    return false;
  }
  check_rewrite_database(db_name);

  m_ndb->setDatabaseName(db_name.c_str());
  m_ndb->setSchemaName(schema_name.c_str());

  NdbDictionary::Dictionary* dict = m_ndb->getDictionary();
  const NdbDictionary::Table* tab = dict->getTable(table_name.c_str());
  if(tab == 0){
    err << "Unable to find table: " << table_name 
        << " error: " << dict->getNdbError().code << endl;
    return false;
  }

  /**
   * remap column(s) based on column-names
   */
  for (int i = 0; i<tableS.m_dictTable->getNoOfColumns(); i++)
  {
    AttributeDesc * attr_desc = tableS.getAttributeDesc(i);
    const NDBCOL * col_in_backup = tableS.m_dictTable->getColumn(i);
    const NDBCOL * col_in_kernel = tab->getColumn(col_in_backup->getName());

    if (col_in_kernel == 0)
    {
      if ((m_tableChangesMask & TCM_EXCLUDE_MISSING_COLUMNS) == 0)
      {
        ndbout << "Missing column("
               << tableS.m_dictTable->getName() << "."
               << col_in_backup->getName()
               << ") in DB and exclude-missing-columns not specified" << endl;
        return false;
      }

      info << "Column in backup ("
           << tableS.m_dictTable->getName() << "."
           << col_in_backup->getName()
           << ") missing in DB.  Excluding column from restore." << endl;

      attr_desc->m_exclude = true;
    }
    else
    {
      attr_desc->attrId = col_in_kernel->getColumnNo();
    }
  }

  for (int i = 0; i<tab->getNoOfColumns(); i++)
  {
    const NDBCOL * col_in_kernel = tab->getColumn(i);
    const NDBCOL * col_in_backup =
      tableS.m_dictTable->getColumn(col_in_kernel->getName());

    if (col_in_backup == 0)
    {
      if ((m_tableChangesMask & TCM_EXCLUDE_MISSING_COLUMNS) == 0)
      {
        ndbout << "Missing column("
               << tableS.m_dictTable->getName() << "."
               << col_in_kernel->getName()
               << ") in backup and exclude-missing-columns not specified"
               << endl;
        return false;
      }

      /**
       * only nullable or defaulted non primary key columns can be missing from backup
       *
       */
      if (col_in_kernel->getPrimaryKey() ||
          ((col_in_kernel->getNullable() == false) &&
           (col_in_kernel->getDefaultValue() == NULL)))
      {
        ndbout << "Missing column("
               << tableS.m_dictTable->getName() << "."
               << col_in_kernel->getName()
               << ") in backup is primary key or not nullable or defaulted in DB"
               << endl;
        return false;
      }

      info << "Column in DB ("
           << tableS.m_dictTable->getName() << "."
           << col_in_kernel->getName()
           << ") missing in Backup.  Will be set to "
           << ((col_in_kernel->getDefaultValue() == NULL)?"Null":"Default value")
           << "." << endl;
    }
  }

  AttrCheckCompatFunc attrCheckCompatFunc = NULL;
  for(int i = 0; i<tableS.m_dictTable->getNoOfColumns(); i++)
  {
    AttributeDesc * attr_desc = tableS.getAttributeDesc(i);
    attr_desc->staging = false;
    if (attr_desc->m_exclude)
      continue;

    const NDBCOL * col_in_kernel = tab->getColumn(attr_desc->attrId);
    const NDBCOL * col_in_backup = tableS.m_dictTable->getColumn(i);

    if(column_compatible_check(tablename,
                               col_in_backup, 
                               col_in_kernel))
    {
      continue;
    }

    NDBCOL::Type type_in_backup = col_in_backup->getType();
    NDBCOL::Type type_in_kernel = col_in_kernel->getType();
    attrCheckCompatFunc = get_attr_check_compatability(type_in_backup,
                                                       type_in_kernel);
    AttrConvType compat
      = (attrCheckCompatFunc == NULL ? ACT_UNSUPPORTED
         : attrCheckCompatFunc(*col_in_backup, *col_in_kernel));
    switch (compat) {
    case ACT_UNSUPPORTED:
      {
        err << "Table: "<< tablename
            << " column: " << col_in_backup->getName()
            << " incompatible with kernel's definition" << endl;
        return false;
      }
    case ACT_PRESERVING:
      if ((m_tableChangesMask & TCM_ATTRIBUTE_PROMOTION) == 0)
      {
        err << "Table: "<< tablename
            << " column: " << col_in_backup->getName()
            << " promotable to kernel's definition but option"
            << " promote-attributes not specified" << endl;
        return false;
      }
      break;
    case ACT_LOSSY:
      if ((m_tableChangesMask & TCM_ATTRIBUTE_DEMOTION) == 0)
      {
        err << "Table: "<< tablename
            << " column: " << col_in_backup->getName()
            << " convertable to kernel's definition but option"
            << " lossy-conversions not specified" << endl;
        return false;
      }
      break;
    case ACT_STAGING_PRESERVING:
      if ((m_tableChangesMask & TCM_ATTRIBUTE_PROMOTION) == 0)
      {
        err << "Table: "<< tablename
            << " column: " << col_in_backup->getName()
            << " promotable to kernel's definition via staging but option"
            << " promote-attributes not specified" << endl;
        return false;
      }
      attr_desc->staging = true;
      tableS.m_staging = true;
      tableS.m_stagingFlags |= Ndb_move_data::Opts::MD_ATTRIBUTE_PROMOTION;
      break;
    case ACT_STAGING_LOSSY:
      if ((m_tableChangesMask & TCM_ATTRIBUTE_DEMOTION) == 0)
      {
        err << "Table: "<< tablename
            << " column: " << col_in_backup->getName()
            << " convertable to kernel's definition via staging but option"
            << " lossy-conversions not specified" << endl;
        return false;
      }
      attr_desc->staging = true;
      tableS.m_staging = true;
      tableS.m_stagingFlags |= Ndb_move_data::Opts::MD_ATTRIBUTE_DEMOTION;
      break;
    default:
      err << "internal error: illegal value of compat = " << compat << endl;
      assert(false);
      return false;
    };

    attr_desc->convertFunc = get_convert_func(type_in_backup,
                                              type_in_kernel);
    Uint32 m_attrSize = NdbColumnImpl::getImpl(*col_in_kernel).m_attrSize;
    Uint32 m_arraySize = NdbColumnImpl::getImpl(*col_in_kernel).m_arraySize;

    // use a char_n_padding_struct to pass length information to convert()
    if (type_in_backup == NDBCOL::Char ||
        type_in_backup == NDBCOL::Binary ||
        type_in_backup == NDBCOL::Bit ||
        type_in_backup == NDBCOL::Varchar ||
        type_in_backup == NDBCOL::Longvarchar ||
        type_in_backup == NDBCOL::Varbinary ||
        type_in_backup == NDBCOL::Longvarbinary)
    {
      unsigned int size = sizeof(struct char_n_padding_struct) +
        m_attrSize * m_arraySize;
      struct char_n_padding_struct *s = (struct char_n_padding_struct *)
        malloc(size +2);
      if (!s)
      {
        err << "No more memory available!" << endl;
        exitHandler();
      }
      s->n_old = (attr_desc->size * attr_desc->arraySize) / 8;
      s->n_new = m_attrSize * m_arraySize;
      memset(s->new_row, 0 , m_attrSize * m_arraySize + 2);
      attr_desc->parameter = s;
      attr_desc->parameterSz = size + 2;
    }
    else if (type_in_backup == NDBCOL::Time ||
             type_in_backup == NDBCOL::Datetime ||
             type_in_backup == NDBCOL::Timestamp ||
             type_in_backup == NDBCOL::Time2 ||
             type_in_backup == NDBCOL::Datetime2 ||
             type_in_backup == NDBCOL::Timestamp2)
    {
      const unsigned int maxdata = 8;
      unsigned int size = sizeof(struct char_n_padding_struct) + maxdata;
      struct char_n_padding_struct *s = (struct char_n_padding_struct *)
        malloc(size);
      if (!s)
      {
        err << "No more memory available!" << endl;
        exitHandler();
      }
      s->n_old = col_in_backup->getPrecision();
      s->n_new = col_in_kernel->getPrecision();
      memset(s->new_row, 0 , maxdata);
      attr_desc->parameter = s;
    }
    else
    {
      unsigned int size = m_attrSize * m_arraySize;
      attr_desc->parameter = malloc(size + 2);
      if (!attr_desc->parameter)
      {
        err << "No more memory available!" << endl;
        exitHandler();
      }
      memset(attr_desc->parameter, 0, size + 2);
      attr_desc->parameterSz = size + 2;
    }

    info << "Data for column "
         << tablename << "."
         << col_in_backup->getName()
         << " will be converted from Backup type into DB type." << endl;
  }

  if (tableS.m_staging)
  {
    // fully qualified name, dissected at createTable()
    BaseString& stagingName = tableS.m_stagingName;
    stagingName.assfmt("%s%s%d", tableS.getTableName(),
                       NDB_RESTORE_STAGING_SUFFIX, m_backup_nodeid);

    NdbDictionary::Table* stagingTable = new NdbDictionary::Table;

    // handle very many rows
    stagingTable->setFragmentType(tab->getFragmentType());
    // XXX not sure about this
    if (tab->getFragmentType() == NdbDictionary::Table::HashMapPartition &&
        !tab->getDefaultNoPartitionsFlag())
    {
      stagingTable->setDefaultNoPartitionsFlag(false);
      stagingTable->setFragmentCount(tab->getFragmentCount());
      stagingTable->setFragmentData(0, 0);
    }

    // if kernel is DD, staging will be too
    bool kernel_is_dd = false;
    Uint32 ts_id = ~(Uint32)0;
    if (tab->getTablespace(&ts_id))
    {
      // must be an initialization
      NdbDictionary::Tablespace ts = dict->getTablespace(ts_id);
      const char* ts_name = ts.getName();
      // how to detect error?
      if (strlen(ts_name) == 0)
      {
        err << "Kernel table " << tablename
            << ": Failed to fetch tablespace id=" << ts_id
            << ": " << dict->getNdbError() << endl;
        return false;
      }
      info << "Kernel table " << tablename
           << " tablespace " << ts_name << endl;
      stagingTable->setTablespaceName(ts_name);
      kernel_is_dd = true;
    }

    /*
     * Staging table is the table in backup, omit excluded columns.
     * Reset column mappings and convert methods.
     */
    int j = 0;
    for (int i = 0; i < tableS.m_dictTable->getNoOfColumns(); i++)
    {
      AttributeDesc * attr_desc = tableS.getAttributeDesc(i);
      const NDBCOL * col_in_backup = tableS.m_dictTable->getColumn(i);
      if (attr_desc->m_exclude)
        continue;
      attr_desc->attrId = (uint32)(j++);
      if(attr_desc->convertFunc)
      {
        const NDBCOL * col_in_kernel = tab->getColumn(col_in_backup->getName());
       
        // Skip built-in conversions from smaller array types 
        // to larger array types so that they are handled by staging.
        // This prevents staging table from growing too large and
        // causing ndb_restore to fail with error 738: record too big.
        NDBCOL::Type type_in_backup = col_in_backup->getType();
        NDBCOL::Type type_in_kernel = col_in_kernel->getType();
        if(is_array(type_in_backup) && is_array(type_in_kernel) && 
           col_in_kernel->getLength() > col_in_backup->getLength()) 
        {
          stagingTable->addColumn(*col_in_backup);
          attr_desc->convertFunc = NULL;
        }
        else
        {
          // Add column of destination type to staging table so that
          // built-in conversion is done while loading data into
          // staging table. 
          stagingTable->addColumn(*col_in_kernel);
        }
      } 
      else 
      {
        stagingTable->addColumn(*col_in_backup);
        attr_desc->convertFunc = NULL;
      }
    }

    if (m_tableChangesMask & TCM_EXCLUDE_MISSING_COLUMNS)
      tableS.m_stagingFlags |= Ndb_move_data::Opts::MD_EXCLUDE_MISSING_COLUMNS;

    tableS.m_stagingTable = stagingTable;
  }

  return true;  
}

bool
BackupRestore::createSystable(const TableS & tables){
  if (!m_restore && !m_restore_meta && !m_restore_epoch)
    return true;
  const char *tablename = tables.getTableName();

  if( strcmp(tablename, NDB_REP_DB "/def/" NDB_APPLY_TABLE) != 0 &&
      strcmp(tablename, NDB_REP_DB "/def/" NDB_SCHEMA_TABLE) != 0 )
  {
    return true;
  }

  BaseString db_name, schema_name, table_name;
  if (!dissect_table_name(tablename, db_name, schema_name, table_name)) {
    return false;
  }
  // do not rewrite database for system tables:
  // check_rewrite_database(db_name);

  m_ndb->setDatabaseName(db_name.c_str());
  m_ndb->setSchemaName(schema_name.c_str());

  NdbDictionary::Dictionary* dict = m_ndb->getDictionary();
  if( dict->getTable(table_name.c_str()) != NULL ){
    return true;
  }
  return table(tables);
}

bool
BackupRestore::table(const TableS & table){
  if (!m_restore && !m_restore_meta && !m_rebuild_indexes && !m_disable_indexes)
    return true;

  const char * name = table.getTableName();
 
  /**
   * Ignore blob tables
   */
  if(match_blob(name) >= 0)
    return true;
  
  const NdbTableImpl & tmptab = NdbTableImpl::getImpl(* table.m_dictTable);
  if ((int) tmptab.m_indexType != (int) NdbDictionary::Index::Undefined){
    m_indexes.push_back(table.m_dictTable);
    return true;
  }
  
  BaseString db_name, schema_name, table_name;
  if (!dissect_table_name(name, db_name, schema_name, table_name)) {
    return false;
  }
  check_rewrite_database(db_name);

  m_ndb->setDatabaseName(db_name.c_str());
  m_ndb->setSchemaName(schema_name.c_str());
  
  NdbDictionary::Dictionary* dict = m_ndb->getDictionary();
  if(m_restore_meta)
  {
    NdbDictionary::Table* tab = table.m_dictTable;
    NdbDictionary::Table copy(*tab);

    copy.setName(table_name.c_str());
    Uint32 id;
    if (copy.getTablespace(&id))
    {
      debug << "Connecting " << name << " to tablespace oldid: " << id << flush;
      NdbDictionary::Tablespace* ts = m_tablespaces[id];
      debug << " newid: " << ts->getObjectId() << endl;
      copy.setTablespace(* ts);
    }

    if (copy.getFragmentType() == NdbDictionary::Object::HashMapPartition)
    {
      Uint32 id;
      if (copy.getHashMap(&id))
      {
        NdbDictionary::HashMap * hm = m_hashmaps[id];
        copy.setHashMap(* hm);
      }
    }
    else if (copy.getDefaultNoPartitionsFlag())
    {
      /*
        Table was defined with default number of partitions. We can restore
        it with whatever is the default in this cluster.
        We use the max_rows parameter in calculating the default number.
      */
      Uint32 no_nodes = m_cluster_connection->no_db_nodes();
      copy.setFragmentCount(get_no_fragments(copy.getMaxRows(),
                            no_nodes));
      set_default_nodegroups(&copy);
    }
    else
    {
      /*
        Table was defined with specific number of partitions. It should be
        restored with the same number of partitions. It will either be
        restored in the same node groups as when backup was taken or by
        using a node group map supplied to the ndb_restore program.
      */
      Vector<Uint32> new_array;
      Uint16 no_parts = copy.getFragmentCount();
      new_array.assign(copy.getFragmentData(), no_parts);
      if (map_nodegroups(new_array.getBase(), no_parts))
      {
        if (translate_frm(&copy))
        {
          err << "Create table " << table.getTableName() << " failed: ";
          err << "Translate frm error" << endl;
          return false;
        }
      }
      copy.setFragmentData(new_array.getBase(), no_parts);
    }

    /**
     * Force of varpart was introduced in 5.1.18, telco 6.1.7 and 6.2.1
     * Since default from mysqld is to add force of varpart (disable with
     * ROW_FORMAT=FIXED) we force varpart onto tables when they are restored
     * from backups taken with older versions. This will be wrong if
     * ROW_FORMAT=FIXED was used on original table, however the likelyhood of
     * this is low, since ROW_FORMAT= was a NOOP in older versions.
     */

    if (table.getBackupVersion() < MAKE_VERSION(5,1,18))
      copy.setForceVarPart(true);
    else if (getMajor(table.getBackupVersion()) == 6 &&
             (table.getBackupVersion() < MAKE_VERSION(6,1,7) ||
              table.getBackupVersion() == MAKE_VERSION(6,2,0)))
      copy.setForceVarPart(true);

    /*
      update min and max rows to reflect the table, this to
      ensure that memory is allocated properly in the ndb kernel
    */
    copy.setMinRows(table.getNoOfRecords());
    if (tab->getMaxRows() != 0 &&
        table.getNoOfRecords() > copy.getMaxRows())
    {
      copy.setMaxRows(table.getNoOfRecords());
    }
    
    NdbTableImpl &tableImpl = NdbTableImpl::getImpl(copy);
    if (table.getBackupVersion() < MAKE_VERSION(5,1,0) && !m_no_upgrade){
      for(int i= 0; i < copy.getNoOfColumns(); i++)
      {
        NdbDictionary::Column::Type t = copy.getColumn(i)->getType();

        if (t == NdbDictionary::Column::Varchar ||
          t == NdbDictionary::Column::Varbinary)
          tableImpl.getColumn(i)->setArrayType(NdbDictionary::Column::ArrayTypeShortVar);
        if (t == NdbDictionary::Column::Longvarchar ||
          t == NdbDictionary::Column::Longvarbinary)
          tableImpl.getColumn(i)->setArrayType(NdbDictionary::Column::ArrayTypeMediumVar);
      }
    }

    if (dict->createTable(copy) == -1) 
    {
      err << "Create table `" << table.getTableName() << "` failed: "
          << dict->getNdbError() << endl;
      if (dict->getNdbError().code == 771)
      {
        /*
          The user on the cluster where the backup was created had specified
          specific node groups for partitions. Some of these node groups
          didn't exist on this cluster. We will warn the user of this and
          inform him of his option.
        */
        err << "The node groups defined in the table didn't exist in this";
        err << " cluster." << endl << "There is an option to use the";
        err << " the parameter ndb-nodegroup-map to define a mapping from";
        err << endl << "the old nodegroups to new nodegroups" << endl; 
      }
      return false;
    }
    info.setLevel(254);
    info << "Successfully restored table `"
         << table.getTableName() << "`" << endl;
  }  
  
  const NdbDictionary::Table* tab = dict->getTable(table_name.c_str());
  if(tab == 0){
    err << "Unable to find table: `" << table_name << "`" 
        << " error : " << dict->getNdbError().code << endl;
    return false;
  }
  if(m_restore_meta)
  {
    if (tab->getFrmData())
    {
      // a MySQL Server table is restored, thus an event should be created
      BaseString event_name("REPL$");
      event_name.append(db_name.c_str());
      event_name.append("/");
      event_name.append(table_name.c_str());

      NdbDictionary::Event my_event(event_name.c_str());
      my_event.setTable(*tab);
      my_event.addTableEvent(NdbDictionary::Event::TE_ALL);
      my_event.setReport(NdbDictionary::Event::ER_DDL);

      // add all columns to the event
      bool has_blobs = false;
      for(int a= 0; a < tab->getNoOfColumns(); a++)
      {
	my_event.addEventColumn(a);
        NdbDictionary::Column::Type t = tab->getColumn(a)->getType();
        if (t == NdbDictionary::Column::Blob ||
            t == NdbDictionary::Column::Text)
          has_blobs = true;
      }
      if (has_blobs)
        my_event.mergeEvents(true);

      while ( dict->createEvent(my_event) ) // Add event to database
      {
	if (dict->getNdbError().classification == NdbError::SchemaObjectExists)
	{
	  info << "Event for table " << table.getTableName()
	       << " already exists, removing.\n";
	  if (!dict->dropEvent(my_event.getName(), 1))
	    continue;
	}
	err << "Create table event for " << table.getTableName() << " failed: "
	    << dict->getNdbError() << endl;
	dict->dropTable(table_name.c_str());
	return false;
      }
      info.setLevel(254);
      info << "Successfully restored table event " << event_name << endl ;
    }
  }
  const NdbDictionary::Table* null = 0;
  m_new_tables.fill(table.m_dictTable->getTableId(), null);
  m_new_tables[table.m_dictTable->getTableId()] = tab;

  m_n_tables++;

  return true;
}

bool
BackupRestore::fk(Uint32 type, const void * ptr)
{
  if (!m_restore_meta && !m_rebuild_indexes && !m_disable_indexes)
    return true;

  // only record FKs, create in endOfTables()
  switch (type){
  case DictTabInfo::ForeignKey:
  {
    const NdbDictionary::ForeignKey* fk_ptr =
      (const NdbDictionary::ForeignKey*)ptr;
    const NdbDictionary::Table *child = NULL, *parent=NULL;
    BaseString db_name, dummy, table_name;
    //check if the child table is a part of the restoration
    if (!dissect_table_name(fk_ptr->getChildTable(),
                       db_name, dummy, table_name))
      return false;
    for(unsigned i = 0; i < m_new_tables.size(); i++)
    {
      if(m_new_tables[i] == NULL)
        continue;
      BaseString new_table_name(m_new_tables[i]->getMysqlName());
      //table name in format db-name/table-name
      Vector<BaseString> split;
      if (new_table_name.split(split, "/") != 2) {
        continue;
      }
      if(db_name == split[0] && table_name == split[1])
      {
        child = m_new_tables[i];
        break;
      }
    }
    if(child)
    {
      //check if parent exists
      if (!dissect_table_name(fk_ptr->getParentTable(),
                              db_name, dummy, table_name))
        return false;
      m_ndb->setDatabaseName(db_name.c_str());
      NdbDictionary::Dictionary* dict = m_ndb->getDictionary();
      parent = dict->getTable(table_name.c_str());
      if (parent == 0)
      {
        err << "Foreign key " << fk_ptr->getName() << " parent table "
            << db_name.c_str() << "." << table_name.c_str()
            << " not found: " << dict->getNdbError() << endl;
        return false;
      }
      m_fks.push_back(fk_ptr);
      info << "Save FK " << fk_ptr->getName() << endl;
    }
    return true;
    break;
  }
  default:
  {
    break;
  }
  }
  return true;
}

bool
BackupRestore::endOfTables(){
  if(!m_restore_meta && !m_rebuild_indexes && !m_disable_indexes)
    return true;

  NdbDictionary::Dictionary* dict = m_ndb->getDictionary();
  for(unsigned i = 0; i<m_indexes.size(); i++){
    NdbTableImpl & indtab = NdbTableImpl::getImpl(* m_indexes[i]);

    BaseString db_name, schema_name, table_name;
    if (!dissect_table_name(indtab.m_primaryTable.c_str(),
                            db_name, schema_name, table_name)) {
      return false;
    }
    check_rewrite_database(db_name);

    m_ndb->setDatabaseName(db_name.c_str());
    m_ndb->setSchemaName(schema_name.c_str());

    const NdbDictionary::Table * prim = dict->getTable(table_name.c_str());
    if(prim == 0){
      err << "Unable to find base table `" << table_name
	  << "` for index `"
	  << indtab.getName() << "`" << endl;
      if (ga_skip_broken_objects)
      {
        continue;
      }
      return false;
    }
    NdbTableImpl& base = NdbTableImpl::getImpl(*prim);
    NdbIndexImpl* idx;
    Vector<BaseString> split_idx;
    {
      BaseString tmp(indtab.getName());
      if (tmp.split(split_idx, "/") != 4)
      {
        err << "Invalid index name format `" << indtab.getName() << "`" << endl;
        return false;
      }
    }
    if(NdbDictInterface::create_index_obj_from_table(&idx, &indtab, &base))
    {
      err << "Failed to create index `" << split_idx[3]
	  << "` on " << table_name << endl;
	return false;
    }
    idx->setName(split_idx[3].c_str());
    if (m_restore_meta && !m_disable_indexes && !m_rebuild_indexes)
    {
      bool done = false;
      for(unsigned int retries = 0; retries < 11; retries++)
      {
        if(dict->createIndex(* idx) == 0)
        {
          done = true;  // success
          break;
        }
        else if(dict->getNdbError().status == NdbError::TemporaryError)
        {
          err << "retry sleep 50 ms on error " <<
                      dict->getNdbError().code << endl;
          NdbSleep_MilliSleep(50);
          continue;  // retry on temporary error
        }
        else
        {
          break; // error out on permanent error
        }
      }
      if(!done)
      {
        delete idx;
        err << "Failed to create index `" << split_idx[3].c_str()
            << "` on `" << table_name << "`" << endl
            << dict->getNdbError() << endl;
        return false;
      }
      info << "Successfully created index `" << split_idx[3].c_str()
          << "` on `" << table_name << "`" << endl;
    }
    else if (m_disable_indexes)
    {
      int res = dict->dropIndex(idx->getName(), prim->getName());
      if (res == 0)
      {
        info << "Dropped index `" << split_idx[3].c_str()
            << "` on `" << table_name << "`" << endl;
      }
    }
    Uint32 id = prim->getObjectId();
    if (m_index_per_table.size() <= id)
    {
      Vector<NdbDictionary::Index*> tmp;
      m_index_per_table.fill(id + 1, tmp);
    }
    Vector<NdbDictionary::Index*> & list = m_index_per_table[id];
    list.push_back(idx);
  }
  return true;
}

bool
BackupRestore::endOfTablesFK()
{
  if (!m_restore_meta && !m_rebuild_indexes && !m_disable_indexes)
    return true;

  NdbDictionary::Dictionary* dict = m_ndb->getDictionary();
  info << "Create foreign keys" << endl;
  for (unsigned i = 0; i < m_fks.size(); i++)
  {
    const NdbDictionary::ForeignKey& fkinfo = *m_fks[i];

    // full name is e.g. 10/14/fk1 where 10,14 are old table ids
    const char* fkname = 0;
    Vector<BaseString> splitname;
    BaseString tmpname(fkinfo.getName());
    int n = tmpname.split(splitname, "/");
    // may get these from ndbapi-created FKs prior to bug#18824753
    if (n == 1)
      fkname = splitname[0].c_str();
    else if (n == 3)
      fkname = splitname[2].c_str();
    else
    {
      err << "Invalid foreign key name " << tmpname.c_str() << endl;
      return false;
    }

    // retrieve fk parent and child
    const NdbDictionary::Table* pTab = 0;
    const NdbDictionary::Index* pInd = 0;
    const NdbDictionary::Table* cTab = 0;
    const NdbDictionary::Index* cInd = 0;
    // parent and child info - db.table.index
    char pInfo[512] = "?";
    char cInfo[512] = "?";
    {
      BaseString db_name, dummy2, table_name;
      if (!dissect_table_name(fkinfo.getParentTable(),
                              db_name, dummy2, table_name))
        return false;
      m_ndb->setDatabaseName(db_name.c_str());
      pTab = dict->getTable(table_name.c_str());
      if (pTab == 0)
      {
        err << "Foreign key " << fkname << " parent table "
            << db_name.c_str() << "." << table_name.c_str()
            << " not found: " << dict->getNdbError() << endl;
        return false;
      }
      if (fkinfo.getParentIndex() != 0)
      {
        BaseString dummy1, dummy2, index_name;
        if (!dissect_index_name(fkinfo.getParentIndex(),
                                dummy1, dummy2, index_name))
          return false;
        pInd = dict->getIndex(index_name.c_str(), table_name.c_str());
        if (pInd == 0)
        {
          err << "Foreign key " << fkname << " parent index "
              << db_name.c_str() << "." << table_name.c_str()
              << "." << index_name.c_str()
              << " not found: " << dict->getNdbError() << endl;
          return false;
        }
      }
      BaseString::snprintf(pInfo, sizeof(pInfo), "%s.%s.%s",
          db_name.c_str(), table_name.c_str(),
          pInd ? pInd->getName() : "PK");
    }
    {
      BaseString db_name, dummy2, table_name;
      if (!dissect_table_name(fkinfo.getChildTable(),
                              db_name, dummy2, table_name))
        return false;
      m_ndb->setDatabaseName(db_name.c_str());
      cTab = dict->getTable(table_name.c_str());
      if (cTab == 0)
      {
        err << "Foreign key " << fkname << " child table "
            << db_name.c_str() << "." << table_name.c_str()
            << " not found: " << dict->getNdbError() << endl;
        return false;
      }
      if (fkinfo.getChildIndex() != 0)
      {
        BaseString dummy1, dummy2, index_name;
        if (!dissect_index_name(fkinfo.getChildIndex(),
                                dummy1, dummy2, index_name))
          return false;
        cInd = dict->getIndex(index_name.c_str(), table_name.c_str());
        if (cInd == 0)
        {
          err << "Foreign key " << fkname << " child index "
              << db_name.c_str() << "." << table_name.c_str()
              << "." << index_name.c_str()
              << " not found: " << dict->getNdbError() << endl;
          return false;
        }
      }
      BaseString::snprintf(cInfo, sizeof(cInfo), "%s.%s.%s",
          db_name.c_str(), table_name.c_str(),
          cInd ? cInd->getName() : "PK");
    }

    // define the fk
    NdbDictionary::ForeignKey fk;
    fk.setName(fkname);
    static const int MaxAttrs = MAX_ATTRIBUTES_IN_INDEX;
    {
      const NdbDictionary::Column* cols[MaxAttrs+1]; // NULL terminated
      const int n = fkinfo.getParentColumnCount();
      int i = 0;
      while (i < n)
      {
        int j = fkinfo.getParentColumnNo(i);
        const NdbDictionary::Column* pCol = pTab->getColumn(j);
        if (pCol == 0)
        {
          err << "Foreign key " << fkname << " fk column " << i
              << " parent column " << j << " out of range" << endl;
          return false;
        }
        cols[i++] = pCol;
      }
      cols[i] = 0;
      fk.setParent(*pTab, pInd, cols);
    }
    {
      const NdbDictionary::Column* cols[MaxAttrs+1]; // NULL terminated
      const int n = fkinfo.getChildColumnCount();
      int i = 0;
      while (i < n)
      {
        int j = fkinfo.getChildColumnNo(i);
        const NdbDictionary::Column* cCol = cTab->getColumn(j);
        if (cCol == 0)
        {
          err << "Foreign key " << fkname << " fk column " << i
              << " child column " << j << " out of range" << endl;
          return false;
        }
        cols[i++] = cCol;
      }
      cols[i] = 0;
      fk.setChild(*cTab, cInd, cols);
    }
    fk.setOnUpdateAction(fkinfo.getOnUpdateAction());
    fk.setOnDeleteAction(fkinfo.getOnDeleteAction());

    // create
    if (dict->createForeignKey(fk) != 0)
    {
      err << "Failed to create foreign key " << fkname
          << " parent " << pInfo
          << " child " << cInfo
          << ": " << dict->getNdbError() << endl;
      return false;
    }
    info << "Successfully created foreign key " << fkname
         << " parent " << pInfo
         << " child " << cInfo
         << endl;
  }
  info << "Create foreign keys done" << endl;
  return true;
}

void BackupRestore::tuple(const TupleS & tup, Uint32 fragmentId)
{
  const TableS * tab = tup.getTable();

  if (!m_restore) 
    return;

  while (m_free_callback == 0)
  {
    assert(m_transactions == m_parallelism);
    // send-poll all transactions
    // close transaction is done in callback
    m_ndb->sendPollNdb(3000, 1);
  }
  
  restore_callback_t * cb = m_free_callback;
  
  if (cb == 0)
    assert(false);
  
  cb->retries = 0;
  cb->fragId = fragmentId;
  cb->tup = tup; // must do copy!

  if (tab->isSYSTAB_0())
  {
    tuple_SYSTAB_0(cb, *tab);
    return;
  }

  m_free_callback = cb->next;

  tuple_a(cb);
}

void BackupRestore::tuple_a(restore_callback_t *cb)
{
  Uint32 partition_id = cb->fragId;
  Uint32 n_bytes;
  while (cb->retries < 10) 
  {
    /**
     * start transactions
     */
    cb->connection = m_ndb->startTransaction();
    if (cb->connection == NULL) 
    {
      if (errorHandler(cb)) 
      {
	m_ndb->sendPollNdb(3000, 1);
	continue;
      }
      err << "Cannot start transaction" << endl;
      exitHandler();
    } // if
    
    const TupleS &tup = cb->tup;
    const NdbDictionary::Table * table = get_table(*tup.getTable());

    NdbOperation * op = cb->connection->getNdbOperation(table);
    
    if (op == NULL) 
    {
      if (errorHandler(cb)) 
	continue;
      err << "Cannot get operation: " << cb->connection->getNdbError() << endl;
      exitHandler();
    } // if
    
    if (op->writeTuple() == -1) 
    {
      if (errorHandler(cb))
	continue;
      err << "Error defining op: " << cb->connection->getNdbError() << endl;
      exitHandler();
    } // if

    // XXX until NdbRecord is used
    op->set_disable_fk();

    n_bytes= 0;

    if (table->getFragmentType() == NdbDictionary::Object::UserDefined)
    {
      if (table->getDefaultNoPartitionsFlag())
      {
        /*
          This can only happen for HASH partitioning with
          user defined hash function where user hasn't
          specified the number of partitions and we
          have to calculate it. We use the hash value
          stored in the record to calculate the partition
          to use.
        */
        int i = tup.getNoOfAttributes() - 1;
	const AttributeData  *attr_data = tup.getData(i);
        Uint32 hash_value =  *attr_data->u_int32_value;
        op->setPartitionId(get_part_id(table, hash_value));
      }
      else
      {
        /*
          Either RANGE or LIST (with or without subparts)
          OR HASH partitioning with user defined hash
          function but with fixed set of partitions.
        */
        op->setPartitionId(partition_id);
      }
    }
    int ret = 0;
    for (int j = 0; j < 2; j++)
    {
      for (int i = 0; i < tup.getNoOfAttributes(); i++) 
      {
	AttributeDesc * attr_desc = tup.getDesc(i);
	const AttributeData * attr_data = tup.getData(i);
	int size = attr_desc->size;
	int arraySize = attr_desc->arraySize;
	char * dataPtr = attr_data->string_value;
	Uint32 length = 0;

        if (attr_desc->m_exclude)
          continue;
       
        if (!attr_data->null)
        {
          const unsigned char * src = (const unsigned char *)dataPtr;
          switch(attr_desc->m_column->getType()){
          case NdbDictionary::Column::Varchar:
          case NdbDictionary::Column::Varbinary:
            length = src[0] + 1;
            break;
          case NdbDictionary::Column::Longvarchar:
          case NdbDictionary::Column::Longvarbinary:
            length = src[0] + (src[1] << 8) + 2;
            break;
          default:
            length = attr_data->size;
            break;
          }
        }
	if (j == 0 && tup.getTable()->have_auto_inc(i))
	  tup.getTable()->update_max_auto_val(dataPtr,size*arraySize);
	
        if (attr_desc->convertFunc)
        {
          if ((attr_desc->m_column->getPrimaryKey() && j == 0) ||
              (j == 1 && !attr_data->null))
          {
            bool truncated = true; // assume data truncation until overridden
            dataPtr = (char*)attr_desc->convertFunc(dataPtr,
                                                    attr_desc->parameter,
                                                    truncated);
            if (!dataPtr)
            {
              const char* tabname = tup.getTable()->m_dictTable->getName();
              err << "Error: Convert data failed when restoring tuples!"
                  << " Data part, table " << tabname << endl;
              exitHandler();
            }
            if (truncated)
            {
              // wl5421: option to report data truncation on tuple of desired
              //err << "======  data truncation detected for column: "
              //    << attr_desc->m_column->getName() << endl;
              attr_desc->truncation_detected = true;
            }
          }            
        }

	if (attr_desc->m_column->getPrimaryKey())
	{
	  if (j == 1) continue;
	  ret = op->equal(attr_desc->attrId, dataPtr, length);
	}
	else
	{
	  if (j == 0) continue;
	  if (attr_data->null) 
	    ret = op->setValue(attr_desc->attrId, NULL, 0);
	  else
	    ret = op->setValue(attr_desc->attrId, dataPtr, length);
	}
	if (ret < 0) {
	  ndbout_c("Column: %d type %d %d %d %d",i,
		   attr_desc->m_column->getType(),
		   size, arraySize, length);
	  break;
	}
        n_bytes+= length;
      }
      if (ret < 0)
	break;
    }
    if (ret < 0)
    {
      if (errorHandler(cb)) 
	continue;
      err << "Error defining op: " << cb->connection->getNdbError() << endl;
      exitHandler();
    }

    if (opt_no_binlog)
    {
      op->setAnyValue(NDB_ANYVALUE_FOR_NOLOGGING);
    }

    // Prepare transaction (the transaction is NOT yet sent to NDB)
    cb->n_bytes= n_bytes;
    cb->connection->executeAsynchPrepare(NdbTransaction::Commit,
					 &callback, cb);
    m_transactions++;
    return;
  }
  err << "Retried transaction " << cb->retries << " times.\nLast error"
      << m_ndb->getNdbError(cb->error_code) << endl
      << "...Unable to recover from errors. Exiting..." << endl;
  exitHandler();
}

void BackupRestore::tuple_SYSTAB_0(restore_callback_t *cb,
                                   const TableS & tab)
{
  const TupleS & tup = cb->tup;
  Uint32 syskey;
  Uint64 nextid;

  if (tab.get_auto_data(tup, &syskey, &nextid))
  {
    /*
      We found a valid auto_increment value in SYSTAB_0
      where syskey is a table_id and nextid is next auto_increment
      value.
     */
    if (restoreAutoIncrement(cb, syskey, nextid) ==  -1)
      exitHandler();
  }
}

int BackupRestore::restoreAutoIncrement(restore_callback_t *cb,
                                        Uint32 tableId, Uint64 value)
{
  /*
    Restore the auto_increment value found in SYSTAB_0 from
    backup. First map the old table id to the new table while
    also checking that it is an actual table will some auto_increment
    column. Note that the SYSTAB_0 table in the backup can contain
    stale information from dropped tables.
   */
  int result = 0;
  const NdbDictionary::Table* tab = (tableId < m_new_tables.size())? m_new_tables[tableId] : NULL;
  if (tab && tab->getNoOfAutoIncrementColumns() > 0)
  {
    /*
      Write the auto_increment value back into SYSTAB_0.
      This is done in a separate transaction and could possibly
      fail, so we retry if a temporary error is received.
     */
    while (cb->retries < 10)
    {
      if ((result = m_ndb->setAutoIncrementValue(tab, value, false) == -1))
      {
        if (errorHandler(cb)) 
        {
          continue;
        }
      }
      break;
    }
  }
  return result;
}

bool BackupRestore::isMissingTable(const TableS& table)
{
  NdbDictionary::Dictionary* dict = m_ndb->getDictionary();
  const char* tablename = table.getTableName();
  BaseString db_name, schema_name, table_name;
  Vector<BaseString> split;
  BaseString tmp(tablename);
  if (tmp.split(split, "/") != 3) {
    return false;
  }
  db_name = split[0];
  schema_name = split[1];
  table_name = split[2];
  m_ndb->setDatabaseName(db_name.c_str());
  m_ndb->setSchemaName(schema_name.c_str());

  const NdbDictionary::Table* tab = dict->getTable(table_name.c_str());

  /* 723 == NoSuchTableExisted */
  return ((tab == NULL) && (dict->getNdbError().code == 723));
}

void BackupRestore::cback(int result, restore_callback_t *cb)
{
  m_transactions--;

  if (result < 0)
  {
    /**
     * Error. temporary or permanent?
     */
    if (errorHandler(cb))
      tuple_a(cb); // retry
    else
    {
      err << "Restore: Failed to restore data due to a unrecoverable error. Exiting..." << endl;
      exitHandler();
    }
  }
  else
  {
    /**
     * OK! close transaction
     */
    m_ndb->closeTransaction(cb->connection);
    cb->connection= 0;
    cb->next= m_free_callback;
    m_free_callback= cb;
    m_dataBytes+= cb->n_bytes;
    m_dataCount++;
  }
}

/**
 * returns true if is recoverable,
 * Error handling based on hugo
 *  false if it is an  error that generates an abort.
 */
bool BackupRestore::errorHandler(restore_callback_t *cb) 
{
  NdbError error;
  if(cb->connection)
  {
    error= cb->connection->getNdbError();
    m_ndb->closeTransaction(cb->connection);
    cb->connection= 0;
  }
  else
  {
    error= m_ndb->getNdbError();
  } 

  Uint32 sleepTime = 100 + cb->retries * 300;
  
  cb->retries++;
  cb->error_code = error.code;

  switch(error.status)
  {
  case NdbError::Success:
    err << "Success error: " << error << endl;
    return false;
    // ERROR!
    
  case NdbError::TemporaryError:
    err << "Temporary error: " << error << endl;
    m_temp_error = true;
    NdbSleep_MilliSleep(sleepTime);
    return true;
    // RETRY
    
  case NdbError::UnknownResult:
    err << "Unknown: " << error << endl;
    return false;
    // ERROR!
    
  default:
  case NdbError::PermanentError:
    //ERROR
    err << "Permanent: " << error << endl;
    return false;
  }
  err << "No error status" << endl;
  return false;
}

void BackupRestore::exitHandler() 
{
  release();
  NDBT_ProgramExit(NDBT_FAILED);
  exit(NDBT_FAILED);
}


void
BackupRestore::tuple_free()
{
  if (!m_restore)
    return;

  // Poll all transactions
  while (m_transactions)
  {
    m_ndb->sendPollNdb(3000);
  }
}

void
BackupRestore::endOfTuples()
{
  tuple_free();
}

#ifdef NOT_USED
static bool use_part_id(const NdbDictionary::Table *table)
{
  if (table->getDefaultNoPartitionsFlag() &&
      (table->getFragmentType() == NdbDictionary::Object::UserDefined))
    return false;
  else
    return true;
}
#endif

static Uint32 get_part_id(const NdbDictionary::Table *table,
                          Uint32 hash_value)
{
  Uint32 no_frags = table->getFragmentCount();
  
  if (table->getLinearFlag())
  {
    Uint32 part_id;
    Uint32 mask = 1;
    while (no_frags > mask) mask <<= 1;
    mask--;
    part_id = hash_value & mask;
    if (part_id >= no_frags)
      part_id = hash_value & (mask >> 1);
    return part_id;
  }
  else
    return (hash_value % no_frags);
}

struct TransGuard
{
  NdbTransaction* pTrans;
  TransGuard(NdbTransaction* p) : pTrans(p) {}
  ~TransGuard() { if (pTrans) pTrans->close();}
};

void
BackupRestore::logEntry(const LogEntry & tup)
{
  if (!m_restore)
    return;


  Uint32 retries = 0;
  NdbError errobj;
retry:
  if (retries == 11)
  {
    err << "execute failed: " << errobj << endl;
    exitHandler();
  }
  else if (retries > 0)
  {
    NdbSleep_MilliSleep(100 + (retries - 1) * 100);
  }
  
  retries++;

  NdbTransaction * trans = m_ndb->startTransaction();
  if (trans == NULL) 
  {
    errobj = m_ndb->getNdbError();
    if (errobj.status == NdbError::TemporaryError)
    {
      goto retry;
    }
    err << "Cannot start transaction: " << errobj << endl;
    exitHandler();
  } // if
  
  TransGuard g(trans);
  const NdbDictionary::Table * table = get_table(*tup.m_table);
  NdbOperation * op = trans->getNdbOperation(table);
  if (op == NULL) 
  {
    err << "Cannot get operation: " << trans->getNdbError() << endl;
    exitHandler();
  } // if
  
  int check = 0;
  switch(tup.m_type)
  {
  case LogEntry::LE_INSERT:
    check = op->insertTuple();
    break;
  case LogEntry::LE_UPDATE:
    check = op->updateTuple();
    break;
  case LogEntry::LE_DELETE:
    check = op->deleteTuple();
    break;
  default:
    err << "Log entry has wrong operation type."
	   << " Exiting...";
    exitHandler();
  }

  if (check != 0) 
  {
    err << "Error defining op: " << trans->getNdbError() << endl;
    exitHandler();
  } // if

  op->set_disable_fk();

  if (table->getFragmentType() == NdbDictionary::Object::UserDefined)
  {
    if (table->getDefaultNoPartitionsFlag())
    {
      const AttributeS * attr = tup[tup.size()-1];
      Uint32 hash_value = *(Uint32*)attr->Data.string_value;
      op->setPartitionId(get_part_id(table, hash_value));
    }
    else
      op->setPartitionId(tup.m_frag_id);
  }

  Bitmask<4096> keys;
  Uint32 n_bytes= 0;
  for (Uint32 i= 0; i < tup.size(); i++) 
  {
    const AttributeS * attr = tup[i];
    int size = attr->Desc->size;
    int arraySize = attr->Desc->arraySize;
    const char * dataPtr = attr->Data.string_value;

    if (attr->Desc->m_exclude)
      continue;
    
    if (tup.m_table->have_auto_inc(attr->Desc->attrId))
      tup.m_table->update_max_auto_val(dataPtr,size*arraySize);

    const Uint32 length = (size / 8) * arraySize;
    n_bytes+= length;

    if (attr->Desc->convertFunc &&
        dataPtr != NULL) // NULL will not be converted
    {
      bool truncated = true; // assume data truncation until overridden
      dataPtr = (char*)attr->Desc->convertFunc(dataPtr,
                                               attr->Desc->parameter,
                                               truncated);
      if (!dataPtr)
      {
        const char* tabname = tup.m_table->m_dictTable->getName();
        err << "Error: Convert data failed when restoring tuples!"
            << " Log part, table " << tabname
            << ", entry type " << tup.m_type << endl;
        exitHandler();
      }            
      if (truncated)
      {
        // wl5421: option to report data truncation on tuple of desired
        //err << "******  data truncation detected for column: "
        //    << attr->Desc->m_column->getName() << endl;
        attr->Desc->truncation_detected = true;
      }
    } 
 
    if (attr->Desc->m_column->getPrimaryKey())
    {
      if(!keys.get(attr->Desc->attrId))
      {
	keys.set(attr->Desc->attrId);
	check= op->equal(attr->Desc->attrId, dataPtr, length);
      }
    }
    else
      check= op->setValue(attr->Desc->attrId, dataPtr, length);
    
    if (check != 0) 
    {
      err << "Error defining op: " << trans->getNdbError() << endl;
      exitHandler();
    } // if
  }
  
  if (opt_no_binlog)
  {
    op->setAnyValue(NDB_ANYVALUE_FOR_NOLOGGING);
  }
  const int ret = trans->execute(NdbTransaction::Commit);
  if (ret != 0)
  {
    // Both insert update and delete can fail during log running
    // and it's ok
    bool ok= false;
    errobj= trans->getNdbError();
    if (errobj.status == NdbError::TemporaryError)
      goto retry;

    switch(tup.m_type)
    {
    case LogEntry::LE_INSERT:
      if(errobj.status == NdbError::PermanentError &&
	 errobj.classification == NdbError::ConstraintViolation)
	ok= true;
      break;
    case LogEntry::LE_UPDATE:
    case LogEntry::LE_DELETE:
      if(errobj.status == NdbError::PermanentError &&
	 errobj.classification == NdbError::NoDataFound)
	ok= true;
      break;
    }
    if (!ok)
    {
      err << "execute failed: " << errobj << endl;
      exitHandler();
    }
  }
  
  m_logBytes+= n_bytes;
  m_logCount++;
}

void
BackupRestore::endOfLogEntrys()
{
  if (!m_restore)
    return;

  info.setLevel(254);
  info << "Restored " << m_dataCount << " tuples and "
       << m_logCount << " log entries" << endl;
}

/*
 *   callback : This is called when the transaction is polled
 *              
 *   (This function must have three arguments: 
 *   - The result of the transaction, 
 *   - The NdbTransaction object, and 
 *   - A pointer to an arbitrary object.)
 */

static void
callback(int result, NdbTransaction* trans, void* aObject)
{
  restore_callback_t *cb = (restore_callback_t *)aObject;
  (cb->restore)->cback(result, cb);
}


AttrCheckCompatFunc 
BackupRestore::get_attr_check_compatability(const NDBCOL::Type &old_type, 
                                            const NDBCOL::Type &new_type) 
{
  int i = 0;
  NDBCOL::Type first_item = m_allowed_promotion_attrs[0].old_type;
  NDBCOL::Type second_item = m_allowed_promotion_attrs[0].new_type;

  while (first_item != old_type || second_item != new_type) 
  {
    if (first_item == NDBCOL::Undefined)
      break;

    i++;
    first_item = m_allowed_promotion_attrs[i].old_type;
    second_item = m_allowed_promotion_attrs[i].new_type;
  }
  if (first_item == old_type && second_item == new_type)
    return m_allowed_promotion_attrs[i].attr_check_compatability;
  return  NULL;
}

AttrConvertFunc
BackupRestore::get_convert_func(const NDBCOL::Type &old_type, 
                                const NDBCOL::Type &new_type) 
{
  int i = 0;
  NDBCOL::Type first_item = m_allowed_promotion_attrs[0].old_type;
  NDBCOL::Type second_item = m_allowed_promotion_attrs[0].new_type;

  while (first_item != old_type || second_item != new_type)
  {
    if (first_item == NDBCOL::Undefined)
      break;
    i++;
    first_item = m_allowed_promotion_attrs[i].old_type;
    second_item = m_allowed_promotion_attrs[i].new_type;
  }
  if (first_item == old_type && second_item == new_type)
    return m_allowed_promotion_attrs[i].attr_convert;

  return  NULL;

}

AttrConvType
BackupRestore::check_compat_promotion(const NDBCOL &old_col,
                                      const NDBCOL &new_col)
{
  return ACT_PRESERVING;
}

AttrConvType
BackupRestore::check_compat_lossy(const NDBCOL &old_col,
                                  const NDBCOL &new_col)
{
  return ACT_LOSSY;
}

AttrConvType
BackupRestore::check_compat_sizes(const NDBCOL &old_col,
                                  const NDBCOL &new_col)
{
  // the size (width) of the element type
  Uint32 new_size = new_col.getSize();
  Uint32 old_size = old_col.getSize();
  // the fixed/max array length (1 for scalars)
  Uint32 new_length = new_col.getLength();
  Uint32 old_length = old_col.getLength();

  // identity conversions have been handled by column_compatible_check()
  assert(new_size != old_size
         || new_length != old_length
         || new_col.getArrayType() != old_col.getArrayType());

  // test for loss of element width or array length
  if (new_size < old_size || new_length < old_length) {
    return ACT_LOSSY;
  }

  // not tested: conversions varying in both, array length and element width
  if (new_size != old_size && new_length != old_length) {
    return ACT_UNSUPPORTED;
  }

  assert(new_size >= old_size && new_length >= old_length);
  return ACT_PRESERVING;
}

AttrConvType
BackupRestore::check_compat_precision(const NDBCOL &old_col,
                                      const NDBCOL &new_col)
{
  Uint32 new_prec = new_col.getPrecision();
  Uint32 old_prec = old_col.getPrecision();

  if (new_prec < old_prec)
    return ACT_LOSSY;
  return ACT_PRESERVING;
}

AttrConvType
BackupRestore::check_compat_char_binary(const NDBCOL &old_col,
                                           const NDBCOL &new_col)
{
  // as in check_compat_sizes
  assert(old_col.getSize() == 1 && new_col.getSize() == 1);
  Uint32 new_length = new_col.getLength();
  Uint32 old_length = old_col.getLength();

  if (new_length < old_length) {
    return ACT_LOSSY;
  }
  return ACT_PRESERVING;
}

AttrConvType
BackupRestore::check_compat_char_to_text(const NDBCOL &old_col,
                                         const NDBCOL &new_col)
{
  if (new_col.getPrimaryKey()) {
    // staging will refuse this so detect early
    info << "convert of TEXT to primary key column not supported" << endl;
    return ACT_UNSUPPORTED;
  }
  return ACT_STAGING_PRESERVING;
}

AttrConvType
BackupRestore::check_compat_text_to_char(const NDBCOL &old_col,
                                         const NDBCOL &new_col)
{
  if (old_col.getPrimaryKey()) {
    // staging will refuse this so detect early
    info << "convert of primary key column to TEXT not supported" << endl;
    return ACT_UNSUPPORTED;
  }
  return ACT_STAGING_LOSSY;
}

AttrConvType
BackupRestore::check_compat_text_to_text(const NDBCOL &old_col,
                                         const NDBCOL &new_col)
{
  if(old_col.getCharset() != new_col.getCharset()) 
  {
    info << "convert to field with different charset not supported" << endl; 
    return ACT_UNSUPPORTED;
  }
  if(old_col.getPartSize() > new_col.getPartSize()) 
  {
   // TEXT/MEDIUMTEXT/LONGTEXT to TINYTEXT conversion is potentially lossy at the 
   // Ndb level because there is a hard limit on the TINYTEXT size.
   // TEXT/MEDIUMTEXT/LONGTEXT is not lossy at the Ndb level, but can be at the 
   // MySQL level.
   // Both conversions require the lossy switch, but they are not lossy in the same way.
    return ACT_STAGING_LOSSY;
  }
  return ACT_STAGING_PRESERVING;
}


// ----------------------------------------------------------------------
// explicit template instantiations
// ----------------------------------------------------------------------

template class Vector<NdbDictionary::Table*>;
template class Vector<const NdbDictionary::Table*>;
template class Vector<NdbDictionary::Tablespace*>;
template class Vector<NdbDictionary::LogfileGroup*>;
template class Vector<NdbDictionary::HashMap*>;
template class Vector<NdbDictionary::Index*>;
template class Vector<Vector<NdbDictionary::Index*> >;

// char array promotions/demotions
template void * BackupRestore::convert_array< Hchar, Hchar >(const void *, void *, bool &);
template void * BackupRestore::convert_array< Hchar, Hvarchar >(const void *, void *, bool &);
template void * BackupRestore::convert_array< Hchar, Hlongvarchar >(const void *, void *, bool &);
template void * BackupRestore::convert_array< Hvarchar, Hchar >(const void *, void *, bool &);
template void * BackupRestore::convert_array< Hvarchar, Hvarchar >(const void *, void *, bool &);
template void * BackupRestore::convert_array< Hvarchar, Hlongvarchar >(const void *, void *, bool &);
template void * BackupRestore::convert_array< Hlongvarchar, Hchar >(const void *, void *, bool &);
template void * BackupRestore::convert_array< Hlongvarchar, Hvarchar >(const void *, void *, bool &);
template void * BackupRestore::convert_array< Hlongvarchar, Hlongvarchar >(const void *, void *, bool &);

// binary array promotions/demotions
template void * BackupRestore::convert_array< Hbinary, Hbinary >(const void *, void *, bool &);
template void * BackupRestore::convert_array< Hbinary, Hvarbinary >(const void *, void *, bool &);
template void * BackupRestore::convert_array< Hbinary, Hlongvarbinary >(const void *, void *, bool &);
template void * BackupRestore::convert_array< Hvarbinary, Hbinary >(const void *, void *, bool &);
template void * BackupRestore::convert_array< Hvarbinary, Hvarbinary >(const void *, void *, bool &);
template void * BackupRestore::convert_array< Hvarbinary, Hlongvarbinary >(const void *, void *, bool &);
template void * BackupRestore::convert_array< Hlongvarbinary, Hbinary >(const void *, void *, bool &);
template void * BackupRestore::convert_array< Hlongvarbinary, Hvarbinary >(const void *, void *, bool &);
template void * BackupRestore::convert_array< Hlongvarbinary, Hlongvarbinary >(const void *, void *, bool &);

// char to binary promotions/demotions
template void * BackupRestore::convert_array< Hchar, Hbinary >(const void *, void *, bool &);
template void * BackupRestore::convert_array< Hchar, Hvarbinary >(const void *, void *, bool &);
template void * BackupRestore::convert_array< Hchar, Hlongvarbinary >(const void *, void *, bool &);
template void * BackupRestore::convert_array< Hvarchar, Hbinary >(const void *, void *, bool &);
template void * BackupRestore::convert_array< Hvarchar, Hvarbinary >(const void *, void *, bool &);
template void * BackupRestore::convert_array< Hvarchar, Hlongvarbinary >(const void *, void *, bool &);
template void * BackupRestore::convert_array< Hlongvarchar, Hbinary >(const void *, void *, bool &);
template void * BackupRestore::convert_array< Hlongvarchar, Hvarbinary >(const void *, void *, bool &);
template void * BackupRestore::convert_array< Hlongvarchar, Hlongvarbinary >(const void *, void *, bool &);

// binary array to char array promotions/demotions
template void * BackupRestore::convert_array< Hbinary, Hchar >(const void *, void *, bool &);
template void * BackupRestore::convert_array< Hbinary, Hvarchar >(const void *, void *, bool &);
template void * BackupRestore::convert_array< Hbinary, Hlongvarchar >(const void *, void *, bool &);
template void * BackupRestore::convert_array< Hvarbinary, Hchar >(const void *, void *, bool &);
template void * BackupRestore::convert_array< Hvarbinary, Hvarchar >(const void *, void *, bool &);
template void * BackupRestore::convert_array< Hvarbinary, Hlongvarchar >(const void *, void *, bool &);
template void * BackupRestore::convert_array< Hlongvarbinary, Hchar >(const void *, void *, bool &);
template void * BackupRestore::convert_array< Hlongvarbinary, Hvarchar >(const void *, void *, bool &);
template void * BackupRestore::convert_array< Hlongvarbinary, Hlongvarchar >(const void *, void *, bool &);

// integral promotions
template void * BackupRestore::convert_integral<Hint8, Hint16>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Hint8, Hint24>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Hint8, Hint32>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Hint8, Hint64>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Hint16, Hint24>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Hint16, Hint32>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Hint16, Hint64>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Hint24, Hint32>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Hint24, Hint64>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Hint32, Hint64>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Huint8, Huint16>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Huint8, Huint24>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Huint8, Huint32>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Huint8, Huint64>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Huint16, Huint24>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Huint16, Huint32>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Huint16, Huint64>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Huint24, Huint32>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Huint24, Huint64>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Huint32, Huint64>(const void *, void *, bool &);

// integral demotions
template void * BackupRestore::convert_integral<Hint16, Hint8>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Hint24, Hint8>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Hint24, Hint16>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Hint32, Hint8>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Hint32, Hint16>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Hint32, Hint24>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Hint64, Hint8>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Hint64, Hint16>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Hint64, Hint24>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Hint64, Hint32>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Huint16, Huint8>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Huint24, Huint8>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Huint24, Huint16>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Huint32, Huint8>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Huint32, Huint16>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Huint32, Huint24>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Huint64, Huint8>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Huint64, Huint16>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Huint64, Huint24>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Huint64, Huint32>(const void *, void *, bool &);

// integral signedness BackupRestore::conversions
template void * BackupRestore::convert_integral<Hint8, Huint8>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Hint16, Huint16>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Hint24, Huint24>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Hint32, Huint32>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Hint64, Huint64>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Huint8, Hint8>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Huint16, Hint16>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Huint24, Hint24>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Huint32, Hint32>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Huint64, Hint64>(const void *, void *, bool &);

// integral signedness+promotion BackupRestore::conversions
template void * BackupRestore::convert_integral<Hint8, Huint16>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Hint8, Huint24>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Hint8, Huint32>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Hint8, Huint64>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Hint16, Huint24>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Hint16, Huint32>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Hint16, Huint64>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Hint24, Huint32>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Hint24, Huint64>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Hint32, Huint64>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Huint8, Hint16>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Huint8, Hint24>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Huint8, Hint32>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Huint8, Hint64>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Huint16, Hint24>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Huint16, Hint32>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Huint16, Hint64>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Huint24, Hint32>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Huint24, Hint64>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Huint32, Hint64>(const void *, void *, bool &);

// integral signedness+demotion BackupRestore::conversions
template void * BackupRestore::convert_integral<Hint16, Huint8>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Hint24, Huint8>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Hint24, Huint16>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Hint32, Huint8>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Hint32, Huint16>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Hint32, Huint24>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Hint64, Huint8>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Hint64, Huint16>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Hint64, Huint24>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Hint64, Huint32>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Huint16, Hint8>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Huint24, Hint8>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Huint24, Hint16>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Huint32, Hint8>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Huint32, Hint16>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Huint32, Hint24>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Huint64, Hint8>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Huint64, Hint16>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Huint64, Hint24>(const void *, void *, bool &);
template void * BackupRestore::convert_integral<Huint64, Hint32>(const void *, void *, bool &);
