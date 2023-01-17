/*
   Copyright (c) 2004, 2023, Oracle and/or its affiliates.

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

#include <memory>

#include "consumer_restore.hpp"
#include <kernel/ndb_limits.h>
#include <NdbIndexStat.hpp>
#include <NdbSleep.h>
#include <NdbTick.h>
#include <NdbToolsProgramExitCodes.hpp>
#include <Properties.hpp>
#include <NdbTypesUtil.hpp>
#include <ndb_rand.h>

#include <ndb_internal.hpp>
#include <ndb_logevent.h>
#include "../src/ndbapi/NdbDictionaryImpl.hpp"
#include "../ndb_lib_move_data.hpp"

#define NDB_ANYVALUE_FOR_NOLOGGING 0x8000007f

/**
 * PK mapping index has a known name.
 * Multiple ndb_restore instances can share an index
 */
static const char* PK_MAPPING_IDX_NAME = "NDB$RESTORE_PK_MAPPING";
static const int MAX_RETRIES = 11;

extern FilteredNdbOut err;
extern FilteredNdbOut info;
extern FilteredNdbOut debug;
extern RestoreLogger restoreLogger;

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

  // text to blob promotions (uses staging table)
  // blobs use the BINARY charset, while texts use charsets like UTF8
  // ignore charset diffs by using check_compat_blob_to_blob
  {NDBCOL::Text,           NDBCOL::Blob, check_compat_blob_to_blob,
   NULL},

  // binary to blob promotions (uses staging table)
  {NDBCOL::Binary,         NDBCOL::Blob,           check_compat_binary_to_blob,
   NULL},
  {NDBCOL::Varbinary,      NDBCOL::Blob,           check_compat_binary_to_blob,
   NULL},
  {NDBCOL::Longvarbinary,  NDBCOL::Blob,           check_compat_binary_to_blob,
   NULL},

  // blob to binary promotions (uses staging table)
  {NDBCOL::Blob,           NDBCOL::Binary,         check_compat_blob_to_binary,
   NULL},
  {NDBCOL::Blob,           NDBCOL::Varbinary,      check_compat_blob_to_binary,
   NULL},
  {NDBCOL::Blob,           NDBCOL::Longvarbinary,  check_compat_blob_to_binary,
   NULL},

  // blob to blob promotions (uses staging table)
  // required when part lengths of blob columns are not equal
  {NDBCOL::Blob,           NDBCOL::Blob,           check_compat_blob_to_blob,
   NULL},

  // blob to text promotions (uses staging table)
  // blobs use the BINARY charset, while texts use charsets like UTF8
  // ignore charset diffs by using check_compat_blob_to_blob
  {NDBCOL::Blob,           NDBCOL::Text, check_compat_blob_to_blob,
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

  if (!m_restore && !m_metadata_work_requested && !m_restore_epoch_requested)
    return true;

  m_tableChangesMask = tableChangesMask;

  m_ndb = new Ndb(m_cluster_connection);
  if (m_ndb == NULL)
    return false;
  
  m_ndb->init(1024);
  if (m_ndb->waitUntilReady(30) != 0)
  {
    restoreLogger.log_error("Could not connect to NDB");
    return false;
  }
  restoreLogger.log_info("Connected to NDB");

  m_callback = new restore_callback_t[m_parallelism];

  if (m_callback == 0)
  {
    restoreLogger.log_error("Failed to allocate callback structs");
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
  for (unsigned i = 0; i < m_index_per_table.size(); i++)
  {
    Vector<NdbDictionary::Index*> & list = m_index_per_table[i];
    for (unsigned j = 0; j < list.size(); j++)
      delete list[j];
    list.clear();
  }
  m_index_per_table.clear();

  for (unsigned i = 0; i < m_tablespaces.size(); i++)
  {
    delete m_tablespaces[i];
  }
  m_tablespaces.clear();

  for (unsigned i = 0; i < m_logfilegroups.size(); i++)
  {
    delete m_logfilegroups[i];
  }
  m_logfilegroups.clear();

  for (unsigned i = 0; i < m_hashmaps.size(); i++)
  {
    delete m_hashmaps[i];
  }
  m_hashmaps.clear();

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
    restoreLogger.log_error("Invalid table name format `%s`",
                            qualified_table_name);
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
    restoreLogger.log_error("Invalid index name format `%s`",
                            qualified_index_name);
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
  else if (m_with_apply_status &&
           (strcmp(tab->getName(), NDB_APPLY_TABLE) == 0 ||
            strcmp(tab->getName(), NDB_REP_DB "/def/" NDB_APPLY_TABLE) == 0))
  {
    /*
      Special case needed as ndb_apply_status is a 'system table',
      and so not pre-loaded into the m_new_tables array.
    */
    NdbDictionary::Dictionary* dict = m_ndb->getDictionary();
    m_ndb->setDatabaseName(NDB_REP_DB);
    m_ndb->setSchemaName("def");
    m_cache.m_new_table = dict->getTable(NDB_APPLY_TABLE);
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
  m_ndb->setDatabaseName(db_name.c_str());
  m_ndb->setSchemaName(schema_name.c_str());

  restoreLogger.log_debug("Creating table %s", table_name.c_str());
  if (!ndbapi_dict_operation_retry(
          [stagingTable](NdbDictionary::Dictionary *dict) {
            return dict->createTable(*stagingTable);
          },
          dict)) {
    const NdbError &error = dict->getNdbError();
    restoreLogger.log_error("Error: Failed to create staging source %s: %u: %s",
                            tablename, error.code, error.message);
    return false;
  }
  restoreLogger.log_info("Created staging source %s", tablename);

  const NdbDictionary::Table* tab = dict->getTable(table_name.c_str());
  if (tab == NULL)
  {
    restoreLogger.log_error("Unable to find table '%s' error: %u: %s",
				    tablename, dict->getNdbError().code, dict->getNdbError().message);
  }

  /* Replace real target table with staging table in m_new_tables */
  const Uint32 orig_table_id = tableS.m_dictTable->getTableId();
  assert(m_new_tables[orig_table_id] != NULL);

  m_new_tables[orig_table_id] = tab;
  m_cache.m_old_table = NULL;

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
    restoreLogger.log_error("Failed to find staging source %s: %u: %s",
                            stablename, dict->getNdbError().code, dict->getNdbError().message);
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
    restoreLogger.log_error("Failed to find staging target %s: %u: %s",
                            ttablename, dict->getNdbError().code, dict->getNdbError().message);
    return false;
  }

  Ndb_move_data md;
  const Ndb_move_data::Stat& stat = md.get_stat();

  if (md.init(source, target) != 0)
  {
    const Ndb_move_data::Error& error = md.get_error();
    restoreLogger.log_error("Move data %s to %s : %u %s", stablename, ttablename, error.code, error.message);
    return false;
  }

  md.set_opts_flags(tableS.m_stagingFlags);

  int retries;
  for (retries = 0; retries < MAX_RETRIES; retries++)
  {
    if (md.move_data(m_ndb) != 0)
    {
      const Ndb_move_data::Error& error = md.get_error();

      restoreLogger.log_error("Move data %s to %s %s at try %u at rows moved %llu total %llu error %u %s",
         stablename, ttablename,
         (error.is_temporary() ? "temporary error" : "permanent error"),
         retries, // default is no limit
         stat.rows_moved, stat.rows_total, error.code, error.message);

      if (!error.is_temporary())
        return false;

      int delay = 100 + retries * 300;
      restoreLogger.log_info("Sleeping %u ms", delay);
      NdbSleep_MilliSleep(delay);
      continue;
    }

    restoreLogger.log_info("Successfully staged %s, moved all %llu rows",
        ttablename, stat.rows_total);
    if ((tableS.m_stagingFlags & Ndb_move_data::Opts::MD_ATTRIBUTE_DEMOTION)
        || stat.truncated != 0) // just in case
    restoreLogger.log_info("Truncated %llu attribute values", stat.truncated);
    break;
  }
  if (retries == MAX_RETRIES)
  {
    restoreLogger.log_error("Move data %s to %s: too many temporary errors: %u",
                            stablename, ttablename, MAX_RETRIES);
    return false;
  }

  m_ndb->setDatabaseName(sdb_name.c_str());
  m_ndb->setSchemaName(sschema_name.c_str());
  const char *stableName = stable_name.c_str();
  if (!ndbapi_dict_operation_retry(
          [stableName](NdbDictionary::Dictionary *dict) {
            return dict->dropTable(stableName);
          },
          dict)) {
    restoreLogger.log_error("Error: Failed to drop staging source %s: %u: %s",
                            stablename, dict->getNdbError().code,
                            dict->getNdbError().message);
    return false;
  }
  restoreLogger.log_info("Dropped staging source %s", stablename);

  /* Replace staging table with real target table in m_new_tables */
  const Uint32 orig_table_id = tableS.m_dictTable->getTableId();
  assert(m_new_tables[orig_table_id] == source);
  
  m_new_tables[orig_table_id] = target;
  m_cache.m_old_table = NULL;
  
  return true;
}

bool
BackupRestore::finalize_table(const TableS & table){
  bool ret= true;
  if (!m_restore && !m_restore_meta)
    return ret;
  if (!table.have_auto_inc())
    return ret;

  const Uint32 orig_table_id = table.m_dictTable->getTableId();
  const Uint64 restore_next_val = m_auto_values[orig_table_id];

  for (int retries = 0; retries < MAX_RETRIES; retries++)
  {
    Uint64 db_next_val = ~(Uint64)0;
    int r= m_ndb->readAutoIncrementValue(get_table(table), db_next_val);
    if (r == -1)
    {
      if (m_ndb->getNdbError().status == NdbError::TemporaryError)
      {
        NdbSleep_MilliSleep(100 + retries * 300);
        continue; // retry
      }
      restoreLogger.log_error("Finalize_table failed to read auto increment "
                              "value for table %s.  Error : %u %s",
                              get_table(table)->getName(),
                              m_ndb->getNdbError().code,
                              m_ndb->getNdbError().message);
      return false;
    }
    if (restore_next_val > db_next_val)
    {
      Ndb::TupleIdRange emptyRange;
      emptyRange.reset();
      
      r= m_ndb->setAutoIncrementValue(get_table(table),
                                      emptyRange,
                                      restore_next_val, 
                                      true);
      if (r == -1 &&
            m_ndb->getNdbError().status == NdbError::TemporaryError)
      {
        NdbSleep_MilliSleep(100 + retries * 300);
        continue; // retry
      }
      ret = (r == 0);
    }
    return (ret);
  }
  return (ret);
}

bool
BackupRestore::rebuild_indexes(const TableS& table)
{
  if (!m_rebuild_indexes)
     return true;

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

  /* First drop any support indexes */
  if (!dropPkMappingIndex(&table))
  {
    return false;
  }

  Vector<NdbDictionary::Index*> & indexes = m_index_per_table[id];
  for(unsigned i = 0; i<indexes.size(); i++)
  {
    const NdbDictionary::Index * const idx = indexes[i];
    const char * const idx_name = idx->getName();
    const char * const tab_name = idx->getTable();
    const NDB_TICKS start = NdbTick_getCurrentTicks();
    restoreLogger.log_info("Rebuilding index `%s` on table `%s` ...",
        idx_name, tab_name);
    if (!ndbapi_dict_operation_retry(
            [idx, idx_name, tab_name](NdbDictionary::Dictionary *dict) {
              if (!dict->getIndex(idx_name, tab_name)) {
                return dict->createIndex(*idx, 1);
              } else
                return 0;
            },
            dict)) {
      restoreLogger.log_error(
          "Rebuilding index `%s` on table `%s` failed: %u: %s", idx_name,
          tab_name, dict->getNdbError().code, dict->getNdbError().message);
      return false;
    }
    const NDB_TICKS stop = NdbTick_getCurrentTicks();
    const Uint64 elapsed = NdbTick_Elapsed(start,stop).seconds();
    restoreLogger.log_info("OK (%llu s)", elapsed);
  }

  return true;
}

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
    restoreLogger.log_error("Table will be restored but will not be able to handle the maximum"
                            " amount of rows as requested");
  }
  return reported_parts;
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
      restoreLogger.log_info("Creating tablespace: %s...", old.getName());

      if (!ndbapi_dict_operation_retry(
              [old](NdbDictionary::Dictionary *dict) {
                return dict->createTablespace(old);
              },
              dict)) {
        restoreLogger.log_error("Create tablespace failed: %s: %u: %s",
                                old.getName(), dict->getNdbError().code,
                                dict->getNdbError().message);
        return false;
      }
      restoreLogger.log_info("Successfully created tablespace %s",
                             old.getName());
    }
    
    NdbDictionary::Tablespace curr = dict->getTablespace(old.getName());
    NdbError errobj = dict->getNdbError();
    if ((int) errobj.classification == (int) ndberror_cl_none)
    {
      NdbDictionary::Tablespace* currptr = new NdbDictionary::Tablespace(curr);
      NdbDictionary::Tablespace * null = 0;
      m_tablespaces.set(currptr, id, null);
      restoreLogger.log_debug("Retreived tablespace: %s oldid: %u newid: %u"
          " %p", currptr->getName(), id, currptr->getObjectId(),
	 (void*)currptr);
      m_n_tablespace++;
      return true;
    }
    
    restoreLogger.log_error("Failed to retrieve tablespace \"%s\": %u: %s",
        old.getName(), errobj.code, errobj.message);
    
    return false;
    break;
  }
  case DictTabInfo::LogfileGroup:
  {
    NdbDictionary::LogfileGroup old(*(NdbDictionary::LogfileGroup*)ptr);
    
    Uint32 id = old.getObjectId();
    
    if (!m_no_restore_disk)
    {
      restoreLogger.log_info("Creating logfile group: %s...", old.getName());
      if (!ndbapi_dict_operation_retry(
              [old](NdbDictionary::Dictionary *dict) {
                return dict->createLogfileGroup(old);
              },
              dict)) {
        restoreLogger.log_error("Create logfile group failed: %s: %u: %s",
                                old.getName(), dict->getNdbError().code,
                                dict->getNdbError().message);
        return false;
      }
      restoreLogger.log_info("Successfully created logfile group %s",
                             old.getName());
    }
    
    NdbDictionary::LogfileGroup curr = dict->getLogfileGroup(old.getName());
    NdbError errobj = dict->getNdbError();
    if ((int) errobj.classification == (int) ndberror_cl_none)
    {
      NdbDictionary::LogfileGroup* currptr = 
	new NdbDictionary::LogfileGroup(curr);
      NdbDictionary::LogfileGroup * null = 0;
      m_logfilegroups.set(currptr, id, null);
      restoreLogger.log_debug("Retreived logfile group: %s oldid: %u newid: %u"
            " %p", currptr->getName(), id, currptr->getObjectId(),
            (void*)currptr);
      m_n_logfilegroup++;
      return true;
    }
    
    restoreLogger.log_error("Failed to retrieve logfile group \"%s\": %u: %s",
        old.getName(), errobj.code, errobj.message);
    
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
      restoreLogger.log_debug("Connecting datafile %s to tablespace"
                              "/logfile group: oldid: %u newid: %u",
                              old.getPath(), objid.getObjectId(),
                              ts->getObjectId());
      old.setTablespace(* ts);
      restoreLogger.log_info("Creating datafile \"%s\"...", old.getPath());

      if (!ndbapi_dict_operation_retry(
              [old](NdbDictionary::Dictionary *dict) {
                return dict->createDatafile(old);
              },
              dict)) {
        restoreLogger.log_error("Create datafile failed: %s: %u: %s",
                                old.getPath(), dict->getNdbError().code,
                                dict->getNdbError().message);
        return false;
      }
      restoreLogger.log_info("Successfully created Datafile %s", old.getPath());
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
      restoreLogger.log_debug("Connecting undofile %s to logfile group: oldid:"
          " %u newid: %u %p", old.getPath(), objid.getObjectId(),
          lg->getObjectId(), (void*)lg);
      old.setLogfileGroup(* lg);
      restoreLogger.log_info("Creating undofile \"%s\"...", old.getPath());
      if (!ndbapi_dict_operation_retry(
              [old](NdbDictionary::Dictionary *dict) {
                return dict->createUndofile(old);
              },
              dict)) {
        restoreLogger.log_error("Create undofile failed: %s: %u: %s",
                                old.getPath(), dict->getNdbError().code,
                                dict->getNdbError().message);
        return false;
      }

      restoreLogger.log_info("Successfully created Undo file %s",
                             old.getPath());
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
      int retries;
      for (retries = 0; retries < MAX_RETRIES; retries++) {
        if (dict->createHashMap(old) != 0) {
          const NdbError &error = dict->getNdbError();

          if (error.code == 721) break;  // Ignore schema already exists.

          if (error.status != NdbError::TemporaryError) {
            restoreLogger.log_error("Create hashmap failed: %s: %u: %s",
                                    old.getName(), error.code, error.message);
            return false;
          }
          restoreLogger.log_error(
              "Temporary: Failed to create hashmap %s: %u: %s", old.getName(),
              error.code, error.message);
          int delay = 100 + retries * 300;
          restoreLogger.log_info("Sleeping %u ms", delay);
          NdbSleep_MilliSleep(delay);
          continue;
        }
        restoreLogger.log_info("Successfully created hashmap %s",
                               old.getName());
        break;
      }
      if (retries == MAX_RETRIES) {
        restoreLogger.log_error(
            "Create hashmap %s failed "
            ": too many temporary errors: %u",
            old.getName(), MAX_RETRIES);
        return false;
      }
    }

    NdbDictionary::HashMap curr;
    if (dict->getHashMap(curr, old.getName()) == 0)
    {
      NdbDictionary::HashMap* currptr =
        new NdbDictionary::HashMap(curr);
      NdbDictionary::HashMap * null = 0;
      m_hashmaps.set(currptr, id, null);
      restoreLogger.log_debug("Retreived hashmap: %s oldid %u newid %u %p",
          currptr->getName(), id, currptr->getObjectId(), (void*)currptr);
      return true;
    }

    NdbError errobj = dict->getNdbError();
    restoreLogger.log_error("Failed to retrieve hashmap \"%s\": %u: %s",
        old.getName(), errobj.code, errobj.message);

    return false;
  }
  case DictTabInfo::ForeignKey: // done after tables
  {
    return true;
  }
  default:
  {
    restoreLogger.log_error("Unknown object type: %u", type);
    break;
  }
  }
  return true;
}

bool
BackupRestore::has_temp_error(){
  return m_temp_error;
}

struct TransGuard
{
  NdbTransaction* pTrans;
  TransGuard(NdbTransaction* p) : pTrans(p) {}
  ~TransGuard() { if (pTrans) pTrans->close();}
};

bool
BackupRestore::update_apply_status(const RestoreMetaData &metaData, bool snapshotstart)
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
    restoreLogger.log_error("%s: %u: %s", NDB_APPLY_TABLE, dict->getNdbError().code, dict->getNdbError().message);
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
    restoreLogger.log_error("%s has wrong format\n", NDB_APPLY_TABLE);
    return false;
  }

  Uint32 server_id= 0;
  Uint32 version= metaData.getNdbVersion();

  Uint64 epoch= 0;
  if (snapshotstart)
  {
    epoch = Uint64(metaData.getStartGCP());
  }
  else
  {
    epoch = Uint64(metaData.getStopGCP());
  }

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

  int retries;
  for (retries = 0; retries < MAX_RETRIES; retries++)
  {
    if (retries > 0)
      NdbSleep_MilliSleep(100 + retries * 300);

    NdbTransaction * trans= m_ndb->startTransaction();
    if (!trans)
    {
      restoreLogger.log_error("%s : failed to get transaction in --restore-epoch: %u:%s",
          NDB_APPLY_TABLE, m_ndb->getNdbError().code, m_ndb->getNdbError().message);
      if (m_ndb->getNdbError().status == NdbError::TemporaryError)
      {
        continue;
      }
    }

    TransGuard g(trans);
    NdbOperation * op= trans->getNdbOperation(ndbtab);
    if (!op)
    {
      restoreLogger.log_error("%s : failed to get operation in --restore-epoch: %u:%s",
          NDB_APPLY_TABLE, trans->getNdbError().code, trans->getNdbError().message);
      if (trans->getNdbError().status == NdbError::TemporaryError)
      {
        continue;
      }
      return false;
    }
    if (op->writeTuple() ||
        op->equal(0u, (const char *)&server_id, sizeof(server_id)) ||
        op->setValue(1u, (const char *)&epoch, sizeof(epoch)))
    {
      restoreLogger.log_error("%s : failed to set epoch value in --restore-epoch: %u:%s",
          NDB_APPLY_TABLE, op->getNdbError().code, op->getNdbError().message);
      return false;
    }
    if ((apply_table_format == 2) &&
        (op->setValue(2u, (const char *)&empty_string, 1) ||
         op->setValue(3u, (const char *)&zero, sizeof(zero)) ||
         op->setValue(4u, (const char *)&zero, sizeof(zero))))
    {
      restoreLogger.log_error("%s : failed to set values in --restore-epoch: %u:%s",
          NDB_APPLY_TABLE, op->getNdbError().code, op->getNdbError().message);
      return false;
    }

    int res = trans->execute(NdbTransaction::Commit);
    if (res != 0)
    {
      restoreLogger.log_error("%s : failed to commit transaction in --restore-epoch: %u:%s",
          NDB_APPLY_TABLE, trans->getNdbError().code, trans->getNdbError().message);
      if (trans->getNdbError().status == NdbError::TemporaryError)
      {
        continue;
      }
      return false;
    }
    else
    {
      result= true;
      break;
    }
  }
  if (result &&
      retries > 0)
    restoreLogger.log_error("--restore-epoch completed successfully "
                            "after retries");

  return result;
}

bool
BackupRestore::delete_epoch_tuple()
{
  /**
   * Make sure only 1 thread in which m_delete_epoch_tuple flag
   * is set executes this method.
   */
  if (!m_delete_epoch_tuple)
    return true;

  bool result= false;

  m_ndb->setDatabaseName(NDB_REP_DB);
  m_ndb->setSchemaName("def");

  NdbDictionary::Dictionary *dict= m_ndb->getDictionary();
  const NdbDictionary::Table *ndbtab= dict->getTable(NDB_APPLY_TABLE);
  if (!ndbtab)
  {
    restoreLogger.log_error("%s: %u: %s", NDB_APPLY_TABLE, dict->getNdbError().code, dict->getNdbError().message);
    return false;
  }
  restoreLogger.log_info("[with_apply_status] Deleting tuple with server_id=0 from ndb_apply_status");

  int retries;
  for (retries = 0; retries < MAX_RETRIES; retries++)
  {
    if (retries > 0)
      NdbSleep_MilliSleep(100 + retries * 300);

    NdbTransaction * trans= m_ndb->startTransaction();
    if (!trans)
    {
      restoreLogger.log_error("%s : failed to get transaction in --with-apply-status: %u:%s",
          NDB_APPLY_TABLE, m_ndb->getNdbError().code, m_ndb->getNdbError().message);
      if (m_ndb->getNdbError().status == NdbError::TemporaryError)
      {
        continue;
      }
    }

    TransGuard g(trans);
    NdbOperation * op= trans->getNdbOperation(ndbtab);
    if (!op)
    {
      restoreLogger.log_error("%s : failed to get operation in --with-apply-status: %u:%s",
          NDB_APPLY_TABLE, trans->getNdbError().code, trans->getNdbError().message);
      if (trans->getNdbError().status == NdbError::TemporaryError)
      {
        continue;
      }
      return false;
    }

    Uint32 server_id= 0;
    if (op->deleteTuple() ||
        op->equal(0u, (const char *)&server_id, sizeof(server_id)))
    {
      restoreLogger.log_error("%s : failed to delete tuple with server_id=0 in --with-apply-status: %u: %s",
          NDB_APPLY_TABLE, op->getNdbError().code, op->getNdbError().message);
      return false;
    }

    int res = trans->execute(NdbTransaction::Commit);
    if (res != 0)
    {
      if(trans->getNdbError().code == 626)
      {
        result= true;
        break;
      }
      restoreLogger.log_error("%s : failed to commit transaction in --with-apply-status: %u:%s",
          NDB_APPLY_TABLE, trans->getNdbError().code, trans->getNdbError().message);
      if (trans->getNdbError().status == NdbError::TemporaryError)
      {
        continue;
      }
      return false;
    }
    else
    {
      result= true;
      break;
    }
  }
  if (result &&
      retries > 0)
    restoreLogger.log_error("--with-apply-status completed successfully "
                            "after retries");

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
    restoreLogger.log_info("Column %s.%s "
        "has different name in DB(%s)",
        tableName, backupCol->getName(), dbCol->getName());
    similarEnough = false;
  }
  
  if (backupCol->getType() != dbCol->getType())
  {
    restoreLogger.log_info("Column %s.%s "
        "%s has different type in DB; promotion or lossy type conversion"
        " (demotion, signed/unsigned) may be required.",
        tableName, backupCol->getName(), dbCol->getName());

    similarEnough = false;
  }

  if (backupCol->getPrimaryKey() != dbCol->getPrimaryKey())
  {
    restoreLogger.log_info("Column %s.%s "
        "%s a primary key in the DB", tableName, backupCol->getName(),
        (dbCol->getPrimaryKey()?" is":" is not"));
    /* If --allow-pk-changes is set, this may be ok */
  }
  else
  {
    if (backupCol->getPrimaryKey())
    {
      if (backupCol->getDistributionKey() != dbCol->getDistributionKey())
      {
        restoreLogger.log_info("Column %s.%s "
            "%s a distribution key in the DB", tableName, backupCol->getName(),
            (dbCol->getDistributionKey()?" is":" is not"));
        /* Not a problem for restore though */
      }
    }
  }

  if (backupCol->getNullable() != dbCol->getNullable())
  {
    restoreLogger.log_info("Column %s.%s "
        "%s nullable in the DB", tableName, backupCol->getName(),
        (dbCol->getNullable()?" is":" is not"));
    if (dbCol->getNullable()) // nullable -> not null conversion
      similarEnough = ((m_tableChangesMask & TCM_ATTRIBUTE_PROMOTION) != 0);
    else if (backupCol->getNullable()) // not null -> nullable conversion
      similarEnough = ((m_tableChangesMask & TCM_ATTRIBUTE_DEMOTION) != 0);
    if (!similarEnough)
    {
      if (backupCol->getNullable())
      {
        restoreLogger.log_error("Conversion of nullable column in backup to non-nullable column"
            " in DB is possible, but cannot be done because option"
            " --lossy-conversions is not specified");
      }
      else
      {
        restoreLogger.log_error("Conversion of non-nullable column in backup to nullable column"
            " in DB is possible, but cannot be done because option "
            "--promote-attributes is not specified");
      }
    }
  }

  if (backupCol->getPrecision() != dbCol->getPrecision())
  {
    restoreLogger.log_info("Column %s.%s "
        "precision is different in the DB",
        tableName, backupCol->getName());
    similarEnough = false;
  }

  if (backupCol->getScale() != dbCol->getScale())
  {
    restoreLogger.log_info("Column %s.%s "
        "scale is different in the DB",
        tableName, backupCol->getName());
    similarEnough = false;
  }

  if (backupCol->getLength() != dbCol->getLength())
  {
    restoreLogger.log_info("Column %s.%s "
        "length is different in the DB",
        tableName, backupCol->getName());
    similarEnough = false;
  }

  if (backupCol->getCharset() != dbCol->getCharset())
  {
    restoreLogger.log_info("Column %s.%s "
        "charset is different in the DB",
        tableName, backupCol->getName());
    similarEnough = false;
  }
  
  if (backupCol->getAutoIncrement() != dbCol->getAutoIncrement())
  {
    restoreLogger.log_info("Column %s.%s "
        "%s AutoIncrementing in the DB", tableName, backupCol->getName(),
        (dbCol->getAutoIncrement()?" is":" is not"));
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
      restoreLogger.log_info("Column %s.%s "
          "Default value is different in the DB",
          tableName, backupCol->getName());
      /* This doesn't matter */
    }
  }

  if (backupCol->getArrayType() != dbCol->getArrayType())
  {
    restoreLogger.log_info("Column %s.%s "
        "ArrayType is different in the DB",
        tableName, backupCol->getName());
    similarEnough = false;
  }

  if (backupCol->getStorageType() != dbCol->getStorageType())
  {
    restoreLogger.log_info("Column %s.%s "
        "Storagetype is different in the DB",
        tableName, backupCol->getName());
    /* This doesn't matter */
  }

  if (backupCol->getBlobVersion() != dbCol->getBlobVersion())
  {
    restoreLogger.log_info("Column %s.%s "
        "Blob version is different in the DB",
        tableName, backupCol->getName());
    similarEnough = false;
  }

  if (backupCol->getDynamic() != dbCol->getDynamic())
  {
    restoreLogger.log_info("Column %s.%s "
        "%s Dynamic in the DB", tableName, backupCol->getName(),
        (dbCol->getDynamic()?" is":" is not"));
    /* This doesn't matter */
  }

  if (similarEnough)
    restoreLogger.log_info("  Difference(s) will be ignored during restore.");
  else
    restoreLogger.log_info("  Difference(s) cannot be ignored.  Column requires conversion to restore.");

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
   * If a staging table is used, there may only be a partial conversion done
   * during data + log restore
   */
  if(match_blob(tableS.getTableName()) == -1)
    return true;

  int mainColumnId = tableS.getMainColumnId();
  const TableS *mainTableS = tableS.getMainTable();
  if(mainTableS->m_dictTable->getColumn(mainColumnId)->getBlobVersion() == NDB_BLOB_V1)
    return true; /* only to make old ndb_restore_compat* tests on v1 blobs pass */

  /**
   * Loop over columns in Backup schema for Blob parts table.
   * v2 Blobs have e.g. <Main table PK col(s)>, NDB$PART, NDB$PKID, NDB$DATA
   */
  for(int i=0; i<tableS.m_dictTable->getNoOfColumns(); i++)
  {
    NDBCOL *col = tableS.m_dictTable->getColumn(i);
    AttributeDesc *attrDesc = tableS.getAttributeDesc(col->getAttrId());
  
    /* get corresponding pk column in main table, backup + kernel versions */
    NDBCOL *backupMainCol = mainTableS->m_dictTable->getColumn(col->getName());
    const NdbDictionary::Table* ndbMainTab = get_table(*mainTableS);
    const NdbDictionary::Column* ndbMainCol = ndbMainTab->getColumn(col->getName());
    const NdbDictionary::Table* ndbTab = get_table(tableS);
    const NdbDictionary::Column* ndbCol = ndbTab->getColumn(col->getName());

    if(!backupMainCol || !backupMainCol->getPrimaryKey())
    {
      /* Finished with Blob part table's pk columns shared with main table
       * (Blob parts table always has main table PKs first)
       * Now just setting attrId values to match kernel table
       */
      assert(ndbCol != NULL);
      attrDesc->attrId = ndbCol->getColumnNo();
      continue;
    }

    int mainTableAttrId = backupMainCol->getAttrId();
    AttributeDesc *mainTableAttrDesc = mainTableS->getAttributeDesc(mainTableAttrId);

    if (mainTableAttrDesc->m_exclude)
    {
      /**
       * This column is gone from the main table pk, remove it from the
       * Blob part table pk here
       */
      restoreLogger.log_debug("Column excluded from main table, "
                              "exclude from blob parts pk");
      attrDesc->m_exclude = true;
      continue;
    }

    /* Column is part of main table pk in backup, check DB */
    if (!ndbMainCol->getPrimaryKey())
    {
      /* This column is still in the main table, but no longer
       * as part of the primary key
       */
      restoreLogger.log_debug("Column moved from pk in main table, "
                              "exclude from blob parts pk");
      attrDesc->m_exclude = true;
      continue;
    }

    attrDesc->attrId = ndbCol->getColumnNo();

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
    restoreLogger.log_error("Table %s has no m_dictTable", tablename);
    return false;
  }
  /**
   * Ignore blob tables
   */
  if(match_blob(tablename) >= 0)
    return true;

  const NdbTableImpl & tmptab = NdbTableImpl::getImpl(* tableS.m_dictTable);
  if ((int)tmptab.m_indexType != (int)NdbDictionary::Index::Undefined) {
    return true;
  }

  BaseString db_name, schema_name, table_name;
  if (!dissect_table_name(tablename, db_name, schema_name, table_name)) {
    restoreLogger.log_error("Failed to dissect table name %s", tablename);
    return false;
  }
  check_rewrite_database(db_name);

  m_ndb->setDatabaseName(db_name.c_str());
  m_ndb->setSchemaName(schema_name.c_str());

  NdbDictionary::Dictionary* dict = m_ndb->getDictionary();
  const NdbDictionary::Table* tab = dict->getTable(table_name.c_str());
  if(tab == 0){
    restoreLogger.log_error("Unable to find table: %s error: %u: %s",
        table_name.c_str(), dict->getNdbError().code, dict->getNdbError().message);
    return false;
  }

  /**
   * Check if target table is restored with --disable-indexes in previous steps.
   * If it already has indexes, it indicates that --disable-indexes isn't used.
   * In that case, display a warning that it could lead to duplicate key errors
   * if the indexes already restored are unique indexes.
   */
  {
    NdbDictionary::Dictionary::List index_list;
    if (dict->listIndexes(index_list, *tab) != 0) {
      restoreLogger.log_error("Failed to list indexes due to NDB error %u: %s",
                              dict->getNdbError().code,
                              dict->getNdbError().message);
      return false;
    }

    bool contains_unique_indexes = false;
    for (unsigned i = 0; i < index_list.count; i++) {
      const char *index_name = index_list.elements[i].name;
      const NdbDictionary::Index *index =
          dict->getIndexGlobal(index_name, *tab);
      if (!index) {
        restoreLogger.log_error(
            "Failed to open index %s from NDB due to error %u: %s", index_name,
            dict->getNdbError().code, dict->getNdbError().message);
        return false;
      }
      if ((int)index->getType() == (int)NdbDictionary::Index::UniqueHashIndex) {
        if (!contains_unique_indexes) {
          restoreLogger.log_error("Unique indexes: ");
        }
        restoreLogger.log_error("%s", index_name);
        contains_unique_indexes = true;
      }
    }

    if (contains_unique_indexes) {
      restoreLogger.log_error(
          "WARNING: Table %s contains unique indexes. "
          "This can cause ndb_restore failures with duplicate key errors "
          "while restoring data. To avoid duplicate key errors, use "
          "--disable-indexes before restoring data and --rebuild-indexes "
          "after data is restored.",
          tab->getName());
    }
  }

  /**
   * Allowed primary key modifications
   *
   * Extend pk
   *   a) Using existing non-pk non-nullable column(s)
   *   b) NOT SUPPORTED Using new defaulted columns
   *
   * Contract pk
   *   c) Leaving columns in the table
   *   d) Removing columns entirely
   *
   * b) not currently supported as
   *   - NdbApi does not represent default-valued pk
   *     columns
   *   - NdbApi does not have a concept of a default-init
   *     value for a type like MySQLD
   *   In future these concepts could be added to NdbApi
   *   or even to ndb_restore.
   *   An autoincrement column could also be considered a
   *   type of defaulted column in a future extension.
   *
   * Note that
   *   a) + c) are symmetric
   *   b) + d) are symmetric
   *
   * Since b) is not supported, d) must be used with care
   * as it is not 'reversible' in e.g. a rollback / replication
   * use case.
   *
   * Reducing or demoting the pk columns has the risk that
   * the reduced pk is no longer unique across the set of
   * key values in the backup.
   * This is a user responsibility to avoid, as it is today
   * when a pk column undergoes a lossy type demotion.
   *
   * When INSERTing rows (from .Data or .Log), all column
   * values are present, so support is trivial.
   *
   * PK mapping index
   *
   * For UPDATE and DELETE, c) and d) are trivial, but
   * a) requires some way to identify which row to
   * update or delete.  This is managed using an optional
   * secondary index on the old primary key column(s).
   *
   * Changes to PK columns in log
   *
   * For case a), it is possible that a backup log contains
   * UPDATEs to the columns which are becoming part
   * of the primary key.  When applying those to the new
   * table schema, they are mapped to separate DELETE + INSERT
   * operations.
   *
   * Blobs
   *
   * Blob columns have part tables which share the primary key of
   * the main table, but do not have all of the other columns.
   *
   * For a), this would require that a column from the main table row
   * is found and used when inserting/updating/deleting a part table
   * row.
   *
   * This is not practical for ndb_restore to do inline in a single
   * pass, so for pk changes to tables with Blobs, we require the
   * use of a staging table to achieve this transform.
   */
  bool full_pk_present_in_kernel = true;
  bool pk_extended_in_kernel = false;
  bool table_has_blob_parts = false;


  /**
   * remap column(s) based on column-names
   * Loop over columns recorded in the Backup
   */
  for (int i = 0; i<tableS.m_dictTable->getNoOfColumns(); i++)
  {
    AttributeDesc * attr_desc = tableS.getAttributeDesc(i);
    const NDBCOL * col_in_backup = tableS.m_dictTable->getColumn(i);
    const NDBCOL * col_in_kernel = tab->getColumn(col_in_backup->getName());
    const bool col_in_backup_pk = col_in_backup->getPrimaryKey();

    if (col_in_kernel == 0)
    {
      /* Col in backup does not exist in kernel */

      if ((m_tableChangesMask & TCM_EXCLUDE_MISSING_COLUMNS) == 0)
      {
        restoreLogger.log_error( "Missing column(%s.%s) in DB and "
            "exclude-missing-columns not specified",
            tableS.m_dictTable->getName(), col_in_backup->getName());
        return false;
      }

      restoreLogger.log_info("Column in backup (%s.%s) missing in DB."
          " Excluding column from restore.",
          tableS.m_dictTable->getName(), col_in_backup->getName());

      attr_desc->m_exclude = true;

      if (col_in_backup_pk)
      {
        restoreLogger.log_info("  Missing column (%s.%s) in DB was "
                               "part of primary key in Backup.  "
                               "Risk of row loss or merge if remaining "
                               "key(s) not unique.",
                               tableS.m_dictTable->getName(),
                               col_in_backup->getName());

        full_pk_present_in_kernel = false;
      }
    }
    else
    {
      /* Col in backup exists in kernel */
      attr_desc->attrId = col_in_kernel->getColumnNo();

      {
        const bool col_in_kernel_pk = col_in_kernel->getPrimaryKey();

        if (col_in_backup_pk)
        {
          if (!col_in_kernel_pk)
          {
            restoreLogger.log_info("Column (%s.%s) is part of "
                                   "primary key in Backup but "
                                   "not part of primary key in DB. "
                                   " Risk of row loss or merge if remaining "
                                   " key(s) not unique.",
                                   tableS.m_dictTable->getName(),
                                   col_in_backup->getName());

            full_pk_present_in_kernel = false;
          }
        }
        else
        {
          if (col_in_kernel_pk)
          {
            restoreLogger.log_info("Column (%s.%s) is not part of "
                                   "primary key in Backup but "
                                   "changed to be part of primary "
                                   "key in DB.",
                                   tableS.m_dictTable->getName(),
                                   col_in_backup->getName());

            pk_extended_in_kernel = true;
          }
        }

        /* Check for blobs with part tables */
        switch (col_in_kernel->getType())
        {
        case NDB_TYPE_BLOB:
        case NDB_TYPE_TEXT:
          if (col_in_kernel->getPartSize() > 0)
          {
            table_has_blob_parts = true;
          }
        default:
          break;
        }
      }
    }
  }

  /* Loop over columns present in the DB */
  for (int i = 0; i<tab->getNoOfColumns(); i++)
  {
    const NDBCOL * col_in_kernel = tab->getColumn(i);
    const NDBCOL * col_in_backup =
      tableS.m_dictTable->getColumn(col_in_kernel->getName());

    if (col_in_backup == 0)
    {
      /* New column in database */
      if ((m_tableChangesMask & TCM_EXCLUDE_MISSING_COLUMNS) == 0)
      {
        restoreLogger.log_error( "Missing column(%s.%s) in backup and "
            "exclude-missing-columns not specified",
             tableS.m_dictTable->getName(), col_in_kernel->getName());
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
        restoreLogger.log_error( "Missing column(%s.%s) in backup "
            " is primary key or not nullable or defaulted in DB",
            tableS.m_dictTable->getName(), col_in_kernel->getName());
        return false;
      }

      restoreLogger.log_info("Column in DB (%s.%s) missing in Backup."
          " Will be set to %s.",
          tableS.m_dictTable->getName(), col_in_kernel->getName(),
           ((col_in_kernel->getDefaultValue() == NULL) ?
                                            "Null" : "Default value"));
    }
  }

  /* Check pk changes against flags */

  if (pk_extended_in_kernel)
  {
    if ((m_tableChangesMask & TCM_ALLOW_PK_CHANGES) == 0)
    {
      restoreLogger.log_error("Error : Primary key extended in DB without "
                              "allow-pk-changes.");
      return false;
    }

    if (m_restore && !m_disable_indexes)
    {
      /**
       * Prefer to use disable_indexes here as it supports safer use of
       * a single shared mapping index rather than per
       * ndb_restore / slice / thread indices
       */
      restoreLogger.log_info("Warning : Primary key extended in DB with "
                             "allow-pk-changes, and --restore-data but without "
                             "--disable-indexes.  A final --rebuild-indexes step "
                             "is required to drop any mapping indices created.");
      /**
       * This could be a hard error (requiring --disable-indexes), but
       * for now it is a warning, allowing serialised use of ndb_restore
       * without --disable-indexes and --rebuild-indexes
       */
      //return false;
    }

    if (table_has_blob_parts)
    {
      /**
       * Problem as the blob parts tables will not have the
       * non-pk column(s) required to do a 1-pass reformat.
       * This requires staging tables.
       */
      restoreLogger.log_info("Table %s has Blob/Text columns with part tables "
                             "and an extended primary key.  This requires "
                             "staging.", tableS.getTableName());
      tableS.m_staging = true;
    }
  }

  if (!full_pk_present_in_kernel)
  {
    if ((m_tableChangesMask & TCM_ALLOW_PK_CHANGES) == 0)
    {
      restoreLogger.log_error("Error : Primary key reduced in DB without "
                              "allow-pk-changes.");
      return false;
    }
    if ((m_tableChangesMask & TCM_ATTRIBUTE_DEMOTION) == 0)
    {
      restoreLogger.log_error("Error : Primary key reduced in DB without "
                              "lossy-conversions.");
      return false;
    }
  }

  if (pk_extended_in_kernel ||
      !full_pk_present_in_kernel)
  {
    if (tab->getFragmentType() == NdbDictionary::Object::UserDefined)
    {
      /**
       * Note
       *
       * 1.  Type promotion/demotion on distribution keys may also
       *     affect stored hash for user defined partitioning
       *     As we don't know the function mapping we cannot allow
       *     this.
       *
       * 2.  Could allow changes to non-distribution primary keys
       *     if there are any, but not for now.
       */
      restoreLogger.log_error("Error : Primary key changes to table with "
                              "user-defined partitioning not supported as "
                              "new value of stored distribution keys "
                              "potentially unknown.");
      return false;
    }
  }

  tableS.m_pk_extended = pk_extended_in_kernel;

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
    const bool col_in_kernel_pk = col_in_kernel->getPrimaryKey();
    attrCheckCompatFunc = get_attr_check_compatability(type_in_backup,
                                                       type_in_kernel);
    AttrConvType compat
      = (attrCheckCompatFunc == NULL ? ACT_UNSUPPORTED
         : attrCheckCompatFunc(*col_in_backup, *col_in_kernel));
    switch (compat) {
    case ACT_UNSUPPORTED:
      {
        restoreLogger.log_error("Table: %s column: %s"
            " incompatible with kernel's definition. "
            "Conversion not possible",
            tablename, col_in_backup->getName());
        return false;
      }
    case ACT_PRESERVING:
      if ((m_tableChangesMask & TCM_ATTRIBUTE_PROMOTION) == 0)
      {
        restoreLogger.log_error("Table: %s column: %s"
            " promotable to kernel's definition but option"
            " promote-attributes not specified",
            tablename, col_in_backup->getName());
        return false;
      }
      break;
    case ACT_LOSSY:
      if ((m_tableChangesMask & TCM_ATTRIBUTE_DEMOTION) == 0)
      {
        restoreLogger.log_error("Table: %s column: %s"
            " convertable to kernel's definition but option"
            " lossy-conversions not specified",
            tablename, col_in_backup->getName());
        return false;
      }
      if (col_in_kernel_pk)
      {
        restoreLogger.log_info("Warning : Table: %s column: %s "
                               "is part of primary key and involves "
                               "a lossy conversion.  Risk of row loss "
                               "or merge if demoted key(s) not unique.",
                               tablename, col_in_backup->getName());
      }
      break;
    case ACT_STAGING_PRESERVING:
      if ((m_tableChangesMask & TCM_ATTRIBUTE_PROMOTION) == 0)
      {
        restoreLogger.log_error("Table: %s column: %s"
            " promotable to kernel's definition via staging but option"
            " promote-attributes not specified",
            tablename, col_in_backup->getName());
        return false;
      }
      /**
       * Staging lossy conversions should be safe w.r.t pk uniqueness
       * as staging conversion rejects duplicate keys
       */
      attr_desc->staging = true;
      tableS.m_staging = true;
      tableS.m_stagingFlags |= Ndb_move_data::Opts::MD_ATTRIBUTE_PROMOTION;
      break;
    case ACT_STAGING_LOSSY:
      if ((m_tableChangesMask & TCM_ATTRIBUTE_DEMOTION) == 0)
      {
        restoreLogger.log_error("Table: %s column: %s"
           " convertable to kernel's definition via staging but option"
           " lossy-conversions not specified",
            tablename, col_in_backup->getName());
        return false;
      }
      attr_desc->staging = true;
      tableS.m_staging = true;
      tableS.m_stagingFlags |= Ndb_move_data::Opts::MD_ATTRIBUTE_DEMOTION;
      break;
    default:
      restoreLogger.log_error("internal error: illegal value of compat = %u", compat);
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
        restoreLogger.log_error("No more memory available!");
        return false;
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
        restoreLogger.log_error("No more memory available!");
        return false;
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
        restoreLogger.log_error("No more memory available!");
        return false;
      }
      memset(attr_desc->parameter, 0, size + 2);
      attr_desc->parameterSz = size + 2;
    }

    restoreLogger.log_info("Data for column %s.%s"
        " will be converted from Backup type into DB type.",
        tablename, col_in_backup->getName());
  }

  if (tableS.m_staging)
  {
    // fully qualified name, dissected at createTable()
    // For mt-restore, each thread creates its own staging table.
    // To ensure that each thread has a unique staging table name,
    // the tablename contains m_instance_name=nodeID.threadID
    BaseString& stagingName = tableS.m_stagingName;
    stagingName.assfmt("%s%s%s", tableS.getTableName(),
                       NDB_RESTORE_STAGING_SUFFIX, m_instance_name);
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
        restoreLogger.log_error("Kernel table %s: "
            "Failed to fetch tablespace id=%u: %u:%s",
            tablename, ts_id, dict->getNdbError().code, dict->getNdbError().message);
        return false;
      }
      restoreLogger.log_info("Kernel table %s tablespace %s",
          tablename, ts_name);
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
          attr_desc->staging = true;
          tableS.m_stagingFlags |= Ndb_move_data::Opts::MD_ATTRIBUTE_PROMOTION;
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
  if (!m_restore && !m_metadata_work_requested)
    return true;
  const char *tablename = tables.getTableName();

  if( strcmp(tablename, NDB_REP_DB "/def/" NDB_APPLY_TABLE) != 0 &&
      strcmp(tablename, NDB_REP_DB "/def/" NDB_SCHEMA_TABLE) != 0 )
  {
    // Dont restore any other system table than those listed above
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
BackupRestore::handle_index_stat_tables() {
  if (!m_restore_meta) return true;

  m_ndb->setDatabaseName(NDB_REP_DB);
  m_ndb->setSchemaName("def");

  NdbIndexStat index_stat;

  if (index_stat.check_systables(m_ndb) == 0) {
    restoreLogger.log_debug("Index stat tables exist");
    return true;
  }

  if (index_stat.create_systables(m_ndb) == 0) {
    restoreLogger.log_debug("Index stat tables created");
    return true;
  }

  restoreLogger.log_error("Creation of index stat tables failed: %d: %s",
                          index_stat.getNdbError().code,
                          index_stat.getNdbError().message);
  return false;
}

bool
BackupRestore::table(const TableS & table){
  if (!m_restore && !m_metadata_work_requested)
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
      NdbDictionary::Tablespace* ts = m_tablespaces[id];
      restoreLogger.log_debug("Connecting %s to tablespace oldid: %u newid: %u",
                              name, id, ts->getObjectId());
      copy.setTablespace(* ts);
    }

    NdbDictionary::Object::PartitionBalance part_bal;
    part_bal = copy.getPartitionBalance();
    assert(part_bal != 0);
    if (part_bal == NdbDictionary::Object::PartitionBalance_ForRPByLDM)
    {
      /**
       * For backups created by versions prior to the introduction of
       * PartitionBalance, we may have picked up the default partition
       * balance member, but we should have a specific setting.
       */
      if (!copy.getDefaultNoPartitionsFlag())
      {
        /* This is actually a specifically partitioned table, check that
         * it has a specific fragment count we can reuse
         */
        assert(copy.getFragmentCount() != 0);
        part_bal = NdbDictionary::Object::PartitionBalance_Specific;
        copy.setPartitionBalance(part_bal);
        restoreLogger.log_info("Setting %s to specific partition balance with "
                               "%u fragments.",
                               name, copy.getFragmentCount());
      }
    }
    if (part_bal != NdbDictionary::Object::PartitionBalance_Specific)
    {
      /* Let the partition balance decide partition count */
      copy.setFragmentCount(0);
    }
    if (copy.getFragmentType() == NdbDictionary::Object::HashMapPartition)
    {
      /**
       * The only specific information we have in specific hash map
       * partitions is really the number of fragments. Other than
       * that we can use a new hash map. We won't be able to restore
       * in exactly the same distribution anyways. So we set the
       * hash map to be non-existing and thus it will be created
       * as part of creating the table. The fragment count is already
       * set in the copy object.
       *
       * Use the PartitionBalance to resize table for this cluster...
       *   set "null" hashmap
       */
      NdbDictionary::HashMap nullMap;
      assert(Uint32(nullMap.getObjectId()) == RNIL);
      assert(Uint32(nullMap.getObjectVersion()) == ~Uint32(0));
      copy.setHashMap(nullMap);
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

      // Build default nodegroups array
      const Uint32 frag_count = copy.getFragmentCount();
      auto node_groups = std::make_unique<Uint32[]>(frag_count);
      node_groups[0] = 0;
      for (Uint32 i = 1; i < frag_count; i++)
      {
        node_groups[i] = NDB_UNDEF_NODEGROUP;
      }
      copy.setFragmentData(node_groups.get(), frag_count);
    }
    else
    {
      /*
        Table was defined with specific number of partitions. It should be
        restored with the same partitions as when backup was taken.
      */
    }

    /**
     * Force of varpart was introduced in 5.1.18, telco 6.1.7 and 6.2.1
     * Since default from mysqld is to add force of varpart (disable with
     * ROW_FORMAT=FIXED) we force varpart onto tables when they are restored
     * from backups taken with older versions. This will be wrong if
     * ROW_FORMAT=FIXED was used on original table, however the likelihood of
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

    int retries;
    for (retries = 0; retries < MAX_RETRIES; retries++) {
      if (dict->createTable(copy) == -1) {
        const NdbError &error = dict->getNdbError();
        if (error.status != NdbError::TemporaryError) {
          restoreLogger.log_error("Create table `%s` failed: %u: %s",
                                  table.getTableName(), error.code,
                                  error.message);
          if (dict->getNdbError().code == 771) {
            /*
              The user on the cluster where the backup was created had specified
              specific node groups for partitions. Some of these node groups
              didn't exist on this cluster.
            */
            restoreLogger.log_error(
                "The node groups defined in the table didn't exist in this"
                " cluster.");
          }
          return false;
        }
        restoreLogger.log_error("Temporary: Failed to create table %s: %u: %s",
                                table.getTableName(), error.code,
                                error.message);
        int delay = 100 + retries * 300;
        restoreLogger.log_info("Sleeping %u ms", delay);
        NdbSleep_MilliSleep(delay);
        continue;
      }
      restoreLogger.log_info("Successfully created table %s",
                             table.getTableName());
      break;
    }
    if (retries == MAX_RETRIES) {
      restoreLogger.log_error(
          "Create table %s failed "
          ": too many temporary errors: %u",
          table.getTableName(), MAX_RETRIES);
      return false;
    }
    info.setLevel(254);
    restoreLogger.log_info("Successfully restored table `%s`",
        table.getTableName());
  }  

  // In mt-restore, many restore-threads may be querying DICT for the
  // same table at one time, which could result in failures. Add retries.
  const NdbDictionary::Table* tab = 0;
  for (int retries = 0; retries < MAX_RETRIES; retries++)
  {
    tab = dict->getTable(table_name.c_str());
    if (tab)
      break;
    else
    {
      const NdbError& error = dict->getNdbError();
      if (error.status != NdbError::TemporaryError)
        NdbSleep_MilliSleep(100 + retries * 300);
      else
        break;
    }
  }
  if(tab == 0)
  {
    restoreLogger.log_error("Unable to find table: `%s` error : %u: %s",
        table_name.c_str(), dict->getNdbError().code, dict->getNdbError().message);
    return false;
  }
  if (m_restore_meta)
  {
    if (tab->getNoOfAutoIncrementColumns())
    {
      // Ensure that auto-inc metadata is created in database
      for (int retries = 0; retries < MAX_RETRIES; retries++)
      {
        int res = m_ndb->setAutoIncrementValue(tab,
                                               Uint64(1),
                                               false);
        if (res == 0)
        {
          break;
        }

        if (m_ndb->getNdbError().status == NdbError::TemporaryError)
        {
          NdbSleep_MilliSleep(100 + retries * 300);
          continue;
        }
        restoreLogger.log_error("Failed to create auto increment value "
                                "for table : %s error : %u %s.",
                                table_name.c_str(),
                                m_ndb->getNdbError().code,
                                m_ndb->getNdbError().message);
        return false;
      }
    }
  }
  const Uint32 orig_table_id = table.m_dictTable->getTableId();
  const NdbDictionary::Table* null = 0;
  m_new_tables.fill(orig_table_id + 1, null);
  m_new_tables[orig_table_id] = tab;
  Uint64 zeroAutoVal = 0;
  m_auto_values.fill(orig_table_id + 1, zeroAutoVal);

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
        restoreLogger.log_error("Foreign key %s "
            "parent table %s.%s not found: %u: %s",
            fk_ptr->getName(),
            db_name.c_str(),
            table_name.c_str(),
            dict->getNdbError().code, dict->getNdbError().message);
        return false;
      }
      m_fks.push_back(fk_ptr);
      restoreLogger.log_info("Save FK %s", fk_ptr->getName());

      if (m_disable_indexes)
      {
        // Extract foreign key name from format
        // like 10/14/fk1 where 10,14 are old table ids
        const char *fkname = 0;
        Vector<BaseString> splitname;
        BaseString tmpname(fk_ptr->getName());
        int n = tmpname.split(splitname, "/");
        if (n == 3)
        {
          fkname = splitname[2].c_str();
        }
        else
        {
          restoreLogger.log_error("Invalid foreign key name %s",
                                  tmpname.c_str());
          return false;
        }
        NdbDictionary::ForeignKey fk;
        char fullname[MAX_TAB_NAME_SIZE];
        sprintf(fullname, "%d/%d/%s", parent->getObjectId(),
                child->getObjectId(), fkname);

        // Drop foreign keys if they exist
        if (dict->getForeignKey(fk, fullname) == 0)
        {
          restoreLogger.log_info("Dropping Foreign key %s", fkname);
          if (!ndbapi_dict_operation_retry(
                  [fk](NdbDictionary::Dictionary *dict) {
                    return dict->dropForeignKey(fk);
                  },
                  dict)) {
            restoreLogger.log_error(
                "Error: Failed to drop foreign key %s: %u: %s", fkname,
                dict->getNdbError().code, dict->getNdbError().message);
            return false;
          }
          restoreLogger.log_info("Successfully dropped foreign key %s", fkname);
        }
      }
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
      restoreLogger.log_error("Unable to find base table `%s` for index `%s`",
          table_name.c_str(), indtab.getName());
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
        restoreLogger.log_error("Invalid index name format `%s`",
            indtab.getName());
        return false;
      }
    }
    if(NdbDictInterface::create_index_obj_from_table(&idx, &indtab, &base))
    {
      restoreLogger.log_error("Failed to create index `%s` on `%s`",
          split_idx[3].c_str(), table_name.c_str());
	return false;
    }
    idx->setName(split_idx[3].c_str());
    if (m_restore_meta && !m_disable_indexes && !m_rebuild_indexes)
    {
      if (!ndbapi_dict_operation_retry(
              [idx](NdbDictionary::Dictionary *dict) {
                return dict->createIndex(*idx);
              },
              dict)) {
        restoreLogger.log_error("Failed to create index `%s` on `%s`: %u: %s",
                                split_idx[3].c_str(), table_name.c_str(),
                                dict->getNdbError().code,
                                dict->getNdbError().message);
        delete idx;
        return false;
      }
      restoreLogger.log_info("Successfully created index `%s` on `%s`",
            split_idx[3].c_str(), table_name.c_str());
    }
    else if (m_disable_indexes)
    {
      // Drop indexes if they exist
      if(dict->getIndex(idx->getName(), prim->getName()))
      {
        restoreLogger.log_info("Dropping Index %s", split_idx[3].c_str());
        if (!ndbapi_dict_operation_retry(
                [idx, prim](NdbDictionary::Dictionary *dict) {
                  return dict->dropIndex(idx->getName(), prim->getName());
                },
                dict)) {
          restoreLogger.log_info("Failed to drop index `%s` on `%s`: %u %s",
                                 split_idx[3].c_str(), table_name.c_str(),
                                 dict->getNdbError().code,
                                 dict->getNdbError().message);
          return false;
        }
        restoreLogger.log_info("Dropped index `%s` on `%s`",
                               split_idx[3].c_str(), table_name.c_str());
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
  restoreLogger.log_info("Create foreign keys");
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
      restoreLogger.log_error("Invalid foreign key name %s",tmpname.c_str());
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
        restoreLogger.log_error("Foreign key %s"
            " parent table %s.%s not found: %u: %s",
            fkname, db_name.c_str(), table_name.c_str(), dict->getNdbError().code, dict->getNdbError().message);
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
          restoreLogger.log_error("Foreign key %s"
              " parent index %s.%s not found: %u: %s",
              fkname, db_name.c_str(), table_name.c_str(), dict->getNdbError().code, dict->getNdbError().message);
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
        restoreLogger.log_error("Foreign key %s"
            " child table %s.%s not found: %u: %s",
            fkname, db_name.c_str(), table_name.c_str(), dict->getNdbError().code, dict->getNdbError().message);
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
          restoreLogger.log_error("Foreign key %s"
              " child index %s.%s not found: %u: %s",
              fkname, db_name.c_str(), table_name.c_str(), dict->getNdbError().code, dict->getNdbError().message);
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
          restoreLogger.log_error("Foreign key %s fk column %u"
              " parent column %u out of range",
              fkname, i, j);
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
          restoreLogger.log_error("Foreign key %s fk column %u"
              " child column %u out of range",
              fkname, i, j);
          return false;
        }
        cols[i++] = cCol;
      }
      cols[i] = 0;
      fk.setChild(*cTab, cInd, cols);
    }
    fk.setOnUpdateAction(fkinfo.getOnUpdateAction());
    fk.setOnDeleteAction(fkinfo.getOnDeleteAction());

    restoreLogger.log_info("Creating foreign key: %s", fkname);
    if (!ndbapi_dict_operation_retry(
            [fk](NdbDictionary::Dictionary *dict) {
              return dict->createForeignKey(fk);
            },
            dict)) {
      restoreLogger.log_error(
          "Failed to create foreign key %s"
          " parent %s child %s : %u: %s",
          fkname, pInfo, cInfo, dict->getNdbError().code,
          dict->getNdbError().message);
      return false;
    }
    restoreLogger.log_info(
        "Successfully created foreign key %s parent %s child %s", fkname, pInfo,
        cInfo);
  }
  restoreLogger.log_info("Create foreign keys done");
  return true;
}

bool BackupRestore::ndbapi_dict_operation_retry(
    const std::function<int(NdbDictionary::Dictionary *)> &func,
    NdbDictionary::Dictionary *dict) {
  int retries;
  for (retries = 0; retries < MAX_RETRIES; retries++) {
    if (func(dict) != 0) {
      if (dict->getNdbError().status != NdbError::TemporaryError) {
        return false;
      }
      restoreLogger.log_error("Failed with temporary error: %d %s",
                              dict->getNdbError().code,
                              dict->getNdbError().message);
      int delay = 100 + retries * 300;
      restoreLogger.log_info("Sleeping %u ms and retrying...", delay);
      NdbSleep_MilliSleep(delay);
      continue;
    }
    break;
  }

  if (retries == MAX_RETRIES) {
    restoreLogger.log_error("Failed with too many temporary errors: %u",
                            MAX_RETRIES);
    return false;
  }
  return true;
}

static Uint64 extract_auto_val(const char *data,
                               int size,
                               NdbDictionary::Column::Type type)
{
  union {
    Int8  i8;
    Int16 i16;
    Int32 i32;
  } val;
  Int64 v; /* Get sign-extension on assignment */
  switch(size){
  case 64:
    memcpy(&v,data,8);
    break;
  case 32:
    memcpy(&val.i32,data,4);
    v= val.i32;
    break;
  case 24:
    v= sint3korr((unsigned char*)data);
    break;
  case 16:
    memcpy(&val.i16,data,2);
    v= val.i16;
    break;
  case 8:
    memcpy(&val.i8,data,1);
    v= val.i8;
    break;
  default:
    return 0;
  };

  /* Don't return negative signed values */
  if (unlikely(v & 0x80000000))
  {
    if (type == NdbDictionary::Column::Bigint ||
        type == NdbDictionary::Column::Int ||
        type == NdbDictionary::Column::Mediumint ||
        type == NdbDictionary::Column::Smallint ||
        type == NdbDictionary::Column::Tinyint)
    {
      /* Negative signed value */
      v = 0;
    }
  }

  return (Uint64) v;
}

void
BackupRestore::update_next_auto_val(Uint32 orig_table_id,
                                    Uint64 next_val)
{
  if (orig_table_id < m_auto_values.size())
  {
    if (next_val > m_auto_values[orig_table_id])
    {
      m_auto_values[orig_table_id] = next_val;
    }
  }
}

bool BackupRestore::get_fatal_error()
{
  return m_fatal_error;
}

void BackupRestore::set_fatal_error(bool err)
{
  m_fatal_error = err;
}

bool BackupRestore::tuple(const TupleS & tup, Uint32 fragmentId)
{
  set_fatal_error(false);
  const TableS * tab = tup.getTable();

  if (!m_restore) 
    return true;

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
    return true;
  }

  m_free_callback = cb->next;

  tuple_a(cb);
 /*
  * A single thread may have multiple INSERT operations in flight, each in
  * its own transaction, with a callback pending. When not defining ops,
  * the thread is reading more data to INSERT, or waiting for callback
  * invocations to occur inside a call to sendPollNdb(). If any one of those
  * operations encounters a fatal error (in definition, or in its callback,
  * after 0..n retries) then it will set a 'global' variable m_fatal_error
  * to state that there has been a fatal error. All other operations check
  * this variable whenever they have their callbacks run next and will
  * quickly finish processing. No new operations will be defined. This will
  * allow the normal polling of transaction completion to finish and return
  * NdbApi to a stable state. Then process cleanup and exit can occur.
  */
  return (!get_fatal_error());
}

void BackupRestore::tuple_a(restore_callback_t *cb)
{
  Uint32 partition_id = cb->fragId;
  Uint32 n_bytes;
  while (cb->retries < MAX_RETRIES)
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
      restoreLogger.log_error("Cannot start transaction");
      set_fatal_error(true);
      return;
    } // if
    
    const TupleS &tup = cb->tup;
    const NdbDictionary::Table * table = get_table(*tup.getTable());

    NdbOperation * op = cb->connection->getNdbOperation(table);
    
    if (op == NULL) 
    {
      if (errorHandler(cb)) 
	continue;
      restoreLogger.log_error("Cannot get operation: %u: %s", cb->error_code,
                              m_ndb->getNdbError(cb->error_code).message);
      set_fatal_error(true);
      return;
    } // if
    
    if (op->writeTuple() == -1) 
    {
      if (errorHandler(cb))
	continue;
      restoreLogger.log_error("Error defining op: %u: %s", cb->error_code,
                              m_ndb->getNdbError(cb->error_code).message);
      set_fatal_error(true);
      return;
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
        {
          Uint64 usedAutoVal = extract_auto_val(dataPtr,
                                                size * arraySize,
                                                attr_desc->m_column->getType());
          Uint32 orig_table_id = tup.getTable()->m_dictTable->getTableId();
          update_next_auto_val(orig_table_id, usedAutoVal + 1);
        }

        /* Use column's DB pk status to decide whether it is a key or data */
        const bool col_pk_in_kernel =
          table->getColumn(attr_desc->attrId)->getPrimaryKey();
	
        if (attr_desc->convertFunc)
        {
          if ((col_pk_in_kernel && j == 0) ||
              (j == 1 && !attr_data->null))
          {
            bool truncated = true; // assume data truncation until overridden
            dataPtr = (char*)attr_desc->convertFunc(dataPtr,
                                                    attr_desc->parameter,
                                                    truncated);
            if (!dataPtr)
            {
              const char* tabname = tup.getTable()->m_dictTable->getName();
              restoreLogger.log_error("Error: Convert data failed when restoring tuples!"
                 " Data part, table %s", tabname);
              set_fatal_error(true);
              m_ndb->closeTransaction(cb->connection);
              cb->connection = NULL;
              return;
            }
            if (truncated)
            {
              // wl5421: option to report data truncation on tuple of desired
              //restoreLogger.log_error("======  data truncation detected for column: "
              //    << attr_desc->m_column->getName());
              attr_desc->truncation_detected = true;
            }
          }            
        }

	if (col_pk_in_kernel)
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
      restoreLogger.log_error("Error defining op: %u: %s", cb->error_code,
                              m_ndb->getNdbError(cb->error_code).message);
      set_fatal_error(true);
      return;
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
  restoreLogger.log_error("Retried transaction %u times.\nLast error %u %s"
      "...Unable to recover from errors. Exiting...",
      cb->retries, m_ndb->getNdbError(cb->error_code).code, m_ndb->getNdbError(cb->error_code).message);
  set_fatal_error(true);
  m_ndb->closeTransaction(cb->connection);
  cb->connection = NULL;
  cb->next = m_free_callback;
  m_free_callback = cb;
  return;
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
      Update next auto val metadata
     */
    update_next_auto_val(syskey, nextid);
  }
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
  check_rewrite_database(db_name);
  m_ndb->setDatabaseName(db_name.c_str());
  m_ndb->setSchemaName(schema_name.c_str());

  const NdbDictionary::Table* tab = dict->getTable(table_name.c_str());

  /* 723 == NoSuchTableExisted */
  return ((tab == NULL) && (dict->getNdbError().code == 723));
}

void BackupRestore::cback(int result, restore_callback_t *cb)
{
#ifdef ERROR_INSERT
    if (m_error_insert == NDB_RESTORE_ERROR_INSERT_FAIL_RESTORE_TUPLE && m_transactions > 10)
    {
      restoreLogger.log_error("Error insert NDB_RESTORE_ERROR_INSERT_FAIL_RESTORE_TUPLE");
      m_error_insert = 0;
      set_fatal_error(true);
    }
#endif

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
      restoreLogger.log_error("Restore: Failed to restore data due to a unrecoverable error. Exiting...");
      cb->next = m_free_callback;
      m_free_callback = cb;
      return;
    }
  }
  else if (get_fatal_error()) // fatal error in other restore-thread
  {
    restoreLogger.log_error("Restore: Failed to restore data due to a unrecoverable error. Exiting...");
    cb->next = m_free_callback;
    m_free_callback = cb;
    return;
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
    restoreLogger.log_error("Success error: %u %s", error.code, error.message);
    return false;
    // ERROR!
    
  case NdbError::TemporaryError:
    restoreLogger.log_error("Temporary error: %u %s", error.code, error.message);
    m_temp_error = true;
    NdbSleep_MilliSleep(sleepTime);
    return true;
    // RETRY
    
  case NdbError::UnknownResult:
    restoreLogger.log_error("Unknown: %u %s", error.code, error.message);
    return false;
    // ERROR!
    
  default:
  case NdbError::PermanentError:
    //ERROR
    restoreLogger.log_error("Permanent: %u %s", error.code, error.message);
    return false;
  }
  restoreLogger.log_error("No error status");
  return false;
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

bool
BackupRestore::tryCreatePkMappingIndex(TableS* table,
                                       const char* short_table_name)
{
  NdbDictionary::Dictionary* dict = m_ndb->getDictionary();
  const NdbDictionary::Table* ndbtab = dict->getTable(short_table_name);

  if (ndbtab == NULL)
  {
    restoreLogger.log_error("Failed to find table %s in DB.  Error : %u %s.",
                            table->getTableName(),
                            dict->getNdbError().code,
                            dict->getNdbError().message);
    return false;
  }
  NdbDictionary::Index idx(PK_MAPPING_IDX_NAME);

  if (idx.setTable(short_table_name) != 0)
  {
    restoreLogger.log_error("Error in idx::setTable.");
    return false;
  }

  idx.setType(NdbDictionary::Index::UniqueHashIndex);
  idx.setLogging(false); /* Save on redo + lcp */

  Uint32 oldPkColsAvailable = 0;

  for (int i=0; i<table->getNoOfAttributes(); i++)
  {
    const AttributeDesc* attrDesc = table->getAttributeDesc(i);
    if (attrDesc->m_column->getPrimaryKey())
    {
      /* This was a primary key before.
       * If it's still in the table then add as
       * an index key
       */
      const NdbDictionary::Column* col =
        ndbtab->getColumn(attrDesc->m_column->getName());

      if (col != NULL)
      {
        restoreLogger.log_info("Adding column (%s) DB(%s) to "
                               "PK mapping index for table %s.",
                               attrDesc->m_column->getName(),
                               col->getName(),
                               table->getTableName());

        if (idx.addColumn(*col) != 0)
        {
          restoreLogger.log_error("Problem adding column %s to index",
                                  col->getName());
          return false;
        }

        oldPkColsAvailable++;
      }
      else
      {
        restoreLogger.log_info("Warning : Table %s primary key column %s "
                               "no longer exists in DB.",
                               table->getTableName(),
                               attrDesc->m_column->getName());
      }
    }
  }

  if (oldPkColsAvailable == 0)
  {
    restoreLogger.log_error("Table %s has update or delete backup log "
                            "entries and no columns from the old "
                            "primary key are available. "
                            "Restore using backup schema then ALTER to "
                            "new schema.",
                            table->getTableName());
    return false;
  }

  if (dict->createIndex(idx) == 0)
  {
    restoreLogger.log_info("Built PK mapping index on table %s.",
                           table->getTableName());

    restoreLogger.log_info("Remember to run ndb_restore --rebuild-indexes "
                           "after all ndb_restore --restore-data steps as this "
                           "will also drop this PK mapping index.");
    return true;
  }


  /* Potential errors :
     - Index now exists - someone else created it
     - System busy with other operation
     - Temp error
     - Permanent error
  */
  NdbError createError = dict->getNdbError();

  if (createError.code == 721)
  {
    /* Index now exists - we will use it */
    return true;
  } else if (createError.code == 701)
  {
    /**
     * System busy with other (schema) operation
     *
     * This could be e.g. another ndb_restore instance building
     * the index, or something else
     */
    restoreLogger.log_info("Build PK mapping index : System busy with "
                           "other schema operation, retrying.");
    return true;
  }
  else if (createError.status == NdbError::TemporaryError)
  {
    return true;
  }
  else
  {
    restoreLogger.log_error("Failed to create pk mapping index on "
                            "table %s %u %s.",
                            table->getTableName(),
                            createError.code,
                            createError.message);
    return false;
  }
}

bool
BackupRestore::getPkMappingIndex(TableS* table)
{
  /**
   * A table can have more pk columns in the DB than
   * in the Backup.
   * For UPDATE and DELETE log events, where the full
   * DB pk is not available, we need some means to
   * identify which row to modify.
   * This is done using a PkMappingIndex, on the
   * available primary keys from the Backup schema.
   *
   * Optimisations :
   *  - A mapping index is only built if needed
   *    (e.g. pk extension + UPDATE/DELETE log
   *    event must be applied)
   *  - A mapping index can be shared between
   *    multiple ndb_restore instances
   *    - It is created when the first
   *      ndb_restore instance to need one
   *      creates one
   *    - It is dropped as part of the
   *      --rebuild-indexes step
   */
  const NdbDictionary::Index* dbIdx = NULL;
  Uint32 retry_count = 0;

  NdbDictionary::Dictionary* dict = m_ndb->getDictionary();

  /* Set database, schema */
  BaseString db_name, schema_name, table_name;
  if (!dissect_table_name(table->getTableName(),
                          db_name, schema_name, table_name))
  {
    restoreLogger.log_error("Failed to dissect table name : %s",
                            table->getTableName());
    return false;
  }

  check_rewrite_database(db_name);
  m_ndb->setDatabaseName(db_name.c_str());
  m_ndb->setSchemaName(schema_name.c_str());
  const char* short_table_name = table_name.c_str();

  do
  {
    dbIdx = dict->getIndex(PK_MAPPING_IDX_NAME,
                           short_table_name);

    if (dbIdx)
    {
      /* Found index, use it */
      table->m_pk_index = dbIdx;
      return true;
    }
    else
    {
      NdbError getErr = dict->getNdbError();

      if (getErr.code == 701)
      {
        /**
         * System busy with other (schema) operation
         *
         * This could be e.g. another ndb_restore instance building
         * the index, or some other DDL.
         */
        restoreLogger.log_info("Build PK mapping index : System busy with "
                               "other schema operation, retrying.");
        NdbSleep_MilliSleep(100 + retry_count * 300);
        continue;
      }

      if (getErr.code == 4243)
      {
        /**
         * Index not found
         * Let's try to create it
         */
        if (!tryCreatePkMappingIndex(table,
                                     short_table_name))
        {
          /* Hard failure */
          return false;
        }
        retry_count = 0;
        NdbSleep_MilliSleep(100 + retry_count * 300);
        /* Retry lookup */
        continue;
      }
      else if (getErr.status == NdbError::TemporaryError)
      {
        NdbSleep_MilliSleep(100 + retry_count * 300);
        /* Retry lookup */
        continue;
      }
      else
      {
        restoreLogger.log_error("Failure looking up PK mapping index on "
                                "table %s %u %s.",
                                table->getTableName(),
                                getErr.code,
                                getErr.message);
        return false;
      }
    }
  } while (retry_count++ < MAX_RETRIES);

  restoreLogger.log_error("Failure to lookup / create PK mapping "
                          "index after %u attempts.",
                          MAX_RETRIES);
  return false;
}

bool
BackupRestore::dropPkMappingIndex(const TableS* table)
{
  const char *tablename = table->getTableName();

  BaseString db_name, schema_name, table_name;
  if (!dissect_table_name(tablename, db_name, schema_name, table_name)) {
    return false;
  }
  check_rewrite_database(db_name);

  m_ndb->setDatabaseName(db_name.c_str());
  m_ndb->setSchemaName(schema_name.c_str());
  NdbDictionary::Dictionary* dict = m_ndb->getDictionary();

  /* Drop any support indexes */
  bool dropped = false;
  int attempts = MAX_RETRIES;
  while (!dropped && attempts--)
  {
    dict->dropIndex(PK_MAPPING_IDX_NAME,
                    table_name.c_str());
    const NdbError dropErr = dict->getNdbError();
    switch (dropErr.status)
    {
    case NdbError::Success:
      restoreLogger.log_info("Dropped PK mapping index on %s.",
                             tablename);
      dropped = true;
      break;
    case NdbError::TemporaryError:
      restoreLogger.log_error("Temporary error: %u %s.",
                              dropErr.code,
                              dropErr.message);
      NdbSleep_MilliSleep(100 + attempts * 300);
      continue;
    case NdbError::PermanentError:
      if (dropErr.code == 723 ||
          dropErr.code == 4243)
      {
        // No such table exists
        dropped = true;
        break;
      }
      [[fallthrough]];
    default:
      restoreLogger.log_error("Error dropping mapping index on %s %u %s",
                              tablename,
                              dropErr.code,
                              dropErr.message);
      return false;
    }
  }

  return dropped;
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

static void
callback_logentry(int result, NdbTransaction* trans, void* aObject)
{
  restore_callback_t *cb = (restore_callback_t *)aObject;
  (cb->restore)->cback_logentry(result, cb);
}

bool
BackupRestore::logEntry(const LogEntry &le)
{
  if (!m_restore)
    return true;

  if (le.m_table->isSYSTAB_0())
  {
    /* We don't restore from SYSTAB_0 log entries */
    return true;
  }

  restore_callback_t * cb = m_free_callback;

  if (cb == 0)
    abort();

  cb->retries = 0;
  cb->le = &le;
  logEntry_a(cb);

  // Poll existing logentry transaction
  while (m_transactions > 0)
  {
    m_ndb->sendPollNdb(3000);
  }

  return (!get_fatal_error());
}

void
BackupRestore::logEntry_a(restore_callback_t *cb)
{
  bool use_mapping_idx = false;

  const LogEntry &tup = *(cb->le);
  if (unlikely((tup.m_table->m_pk_extended) &&
               (tup.m_type != LogEntry::LE_INSERT) &&
               (!tup.m_table->m_staging)))
  {
    /**
     * We will need to find a row to operate on, using
     * a secondary unique index on the remains of the
     * old PK
     */
    if (unlikely(tup.m_table->m_pk_index == NULL))
    {
      /* Need to get/build an index for this purpose */
      if (!getPkMappingIndex(tup.m_table))
      {
        restoreLogger.log_error("Build of PK mapping index failed "
                                "on table %s.",
                                tup.m_table->getTableName());
        set_fatal_error(true);
        return;
      }
      assert(tup.m_table->m_pk_index != NULL);

      restoreLogger.log_info("Using PK mapping index on table %s.",
                             tup.m_table->getTableName());
    }

    use_mapping_idx = true;
  }

retry:
  Uint32 mapping_idx_key_count = 0;
#ifdef ERROR_INSERT
  if (m_error_insert == NDB_RESTORE_ERROR_INSERT_FAIL_REPLAY_LOG && m_logCount == 25)
  {
    restoreLogger.log_error("Error insert NDB_RESTORE_ERROR_INSERT_FAIL_REPLAY_LOG");
    m_error_insert = 0;
    cb->retries = MAX_RETRIES;
  }
#endif

  if (cb->retries == MAX_RETRIES)
  {
    restoreLogger.log_error("execute failed");
    set_fatal_error(true);
    return;
  }

  cb->connection = m_ndb->startTransaction();
  NdbTransaction * trans = cb->connection;
  if (trans == NULL) 
  {
    if (errorHandler(cb)) // temp error, retry
      goto retry;
    set_fatal_error(true);
    restoreLogger.log_error("Cannot start transaction: %u: %s", cb->error_code,
                            m_ndb->getNdbError(cb->error_code).message);
    return;
  }

  const NdbDictionary::Table * table = get_table(*tup.m_table);
  NdbOperation * op = NULL;

  if (unlikely(use_mapping_idx))
  {
    /* UI access */
    op = trans->getNdbIndexOperation(tup.m_table->m_pk_index,
                                     table);
  }
  else
  {
    /* Normal pk access */
    op = trans->getNdbOperation(table);
  }
  if (op == NULL) 
  {
    if (errorHandler(cb)) // temp error, retry
      goto retry;
    set_fatal_error(true);
    restoreLogger.log_error("Cannot get operation: %u: %s", cb->error_code,
                            m_ndb->getNdbError(cb->error_code).message);
    return;
  }

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
    restoreLogger.log_error("Log entry has wrong operation type %u"
	  " Exiting...", tup.m_type);
    m_ndb->closeTransaction(trans);
    set_fatal_error(true);
    return;
  }

  if (check != 0) 
  {
    restoreLogger.log_error("Error defining op: %u: %s",
              trans->getNdbError().code, trans->getNdbError().message);
    if (errorHandler(cb)) // temp error, retry
      goto retry;
    set_fatal_error(true);
    return;
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
  for (Uint32 pass= 0; pass < 2; pass++)  // Keys then Values
  {
    for (Uint32 i= 0; i < tup.size(); i++)
    {
      const AttributeS * attr = tup[i];
      int size = attr->Desc->size;
      int arraySize = attr->Desc->arraySize;
      const char * dataPtr = attr->Data.string_value;
      const bool col_pk_in_backup = attr->Desc->m_column->getPrimaryKey();

      if (attr->Desc->m_exclude)
        continue;

      const bool col_pk_in_kernel =
        table->getColumn(attr->Desc->attrId)->getPrimaryKey();
      bool col_is_key = col_pk_in_kernel;
      Uint32 keyAttrId = attr->Desc->attrId;

      if (unlikely(use_mapping_idx))
      {
        if (col_pk_in_backup)
        {
          /* Using a secondary UI to map non-excluded
           * backup keys to kernel rows.
           * Backup pks are UI keys, using key
           * AttrIds in declaration order.
           * Therefore we set the attrId here.
           */
          col_is_key = true;
          keyAttrId = mapping_idx_key_count++;
        }
        else
        {
          col_is_key = false;
        }
      }

      if ((!col_is_key && pass == 0) ||  // Keys
          (col_is_key && pass == 1))     // Values
      {
        continue;
      }

      /* Check for unsupported PK update */
      if (unlikely(!col_pk_in_backup && col_pk_in_kernel))
     {
        if (unlikely(tup.m_type == LogEntry::LE_UPDATE))
        {
          if ((m_tableChangesMask & TCM_IGNORE_EXTENDED_PK_UPDATES) != 0)
          {
            /* Ignore it as requested */
            m_pk_update_warning_count++;
            continue;
          }
          else
          {
            /**
             * Problem as a non-pk column has become part of
             * the table's primary key, but is updated in
             * the backup - which would require DELETE + INSERT
             * to represent
             */
            restoreLogger.log_error("Error : Primary key remapping failed "
                                    "during log apply for table %s which " 
                                    "UPDATEs column(s) now included in the "
                                    "table's primary key.  "
                                    "Perhaps the --ignore-extended-pk-updates "
                                    "switch is missing?",
                                    tup.m_table->m_dictTable->getName());
            m_ndb->closeTransaction(trans);
            set_fatal_error(true);
            return;
          }
        }
     }
      if (tup.m_table->have_auto_inc(attr->Desc->attrId))
      {
        Uint64 usedAutoVal = extract_auto_val(dataPtr,
                                              size * arraySize,
                                              attr->Desc->m_column->getType());
        Uint32 orig_table_id = tup.m_table->m_dictTable->getTableId();
        update_next_auto_val(orig_table_id, usedAutoVal + 1);
      }

      const Uint32 length = (size / 8) * arraySize;
      cb->n_bytes+= length;

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
          restoreLogger.log_error("Error: Convert data failed when restoring tuples! "
                                  "Log part, table %s, entry type %u.",
                                  tabname, tup.m_type);
          m_ndb->closeTransaction(trans);
          set_fatal_error(true);
          return;
        }
        if (truncated)
        {
          // wl5421: option to report data truncation on tuple of desired
          //err << "******  data truncation detected for column: "
          //    << attr->Desc->m_column->getName() << endl;
          attr->Desc->truncation_detected = true;
        }
      }

      if (col_is_key)
      {
        assert(pass == 0);

        if(!keys.get(keyAttrId))
        {
          keys.set(keyAttrId);
          check= op->equal(keyAttrId, dataPtr, length);
        }
      }
      else
      {
        assert(pass == 1);
        if (tup.m_type != LogEntry::LE_DELETE)
        {
          check= op->setValue(attr->Desc->attrId, dataPtr, length);
        }
      }

      if (check != 0)
      {
        restoreLogger.log_error("Error defining log op: %u %s.",
              trans->getNdbError().code, trans->getNdbError().message);
        if (errorHandler(cb)) // temp error, retry
          goto retry;
        set_fatal_error(true);
        return;
      } // if
    }
  }
  
  if (opt_no_binlog)
  {
    op->setAnyValue(NDB_ANYVALUE_FOR_NOLOGGING);
  }

  trans->executeAsynchPrepare(NdbTransaction::Commit,
                                 &callback_logentry, cb);
  m_transactions++;
  return;
}

void BackupRestore::cback_logentry(int result, restore_callback_t *cb)
{
  m_transactions--;
  const NdbError errobj = cb->connection->getNdbError();
  m_ndb->closeTransaction(cb->connection);
  cb->connection = NULL;

#ifndef NDEBUG
  /* Test retry path */
  if ((m_logCount % 100000) == 3)
  {
    if (cb->retries++ < 3)
    {
      restoreLogger.log_info("Testing log retry path");
      logEntry_a(cb);
      return;
    }
  }
#endif

  if (result < 0)
  {
    // Ignore errors and continue if
    // - insert fails with ConstraintViolation or
    // - update/delete fails with NoDataFound
    bool ok= false;
    if (errobj.status == NdbError::TemporaryError)
    {
      logEntry_a(cb);
      return;
    }
    switch(cb->le->m_type)
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
      set_fatal_error(true);
      return;
    }
  }
  m_logBytes+= cb->n_bytes;
  m_logCount++;
}

void
BackupRestore::endOfLogEntrys()
{
  if (!m_restore)
    return;

  if (m_pk_update_warning_count > 0)
  {
    restoreLogger.log_info("Warning : --ignore-extended-pk-updates resulted in %llu "
                           "modifications to extended primary key columns being "
                           "ignored.",
                           m_pk_update_warning_count);
  }

  info.setLevel(254);
  restoreLogger.log_info("Restored %u tuples and "
      "%u log entries", m_dataCount, m_logCount);
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
    restoreLogger.log_info("convert of TEXT to primary key column not supported");
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
    restoreLogger.log_info("convert of primary key column to TEXT not supported");
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
    restoreLogger.log_info("convert to field with different charset not supported"); 
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

AttrConvType
BackupRestore::check_compat_binary_to_blob(const NDBCOL &old_col,
                                           const NDBCOL &new_col)
{
  return ACT_STAGING_PRESERVING;
}

AttrConvType
BackupRestore::check_compat_blob_to_binary(const NDBCOL &old_col,
                                           const NDBCOL &new_col)
{
  return ACT_STAGING_LOSSY;
}

AttrConvType
BackupRestore::check_compat_blob_to_blob(const NDBCOL &old_col,
                                         const NDBCOL &new_col)
{
  if(old_col.getPartSize() > new_col.getPartSize())
  {
   // BLOB/MEDIUMBLOB/LONGBLOB to TINYBLOB conversion is potentially lossy at the
   // Ndb level because there is a hard limit on the TINYBLOB size.
   // BLOB/MEDIUMBLOB/LONGBLOB is not lossy at the Ndb level, but can be at the
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
