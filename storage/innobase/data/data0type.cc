/*****************************************************************************

Copyright (c) 1996, 2024, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file data/data0type.cc
 Data types

 Created 1/16/1996 Heikki Tuuri
 *******************************************************/

#include "data0type.h"
#include "rem0rec.h"

#include <sys/types.h>

#include "ha_prototypes.h"

/* At the database startup we store the default-charset collation number of
this MySQL installation to this global variable. If we have < 4.1.2 format
column definitions, or records in the insert buffer, we use this
charset-collation code for them. */

ulint data_mysql_default_charset_coll;

/** Determine how many bytes the first n characters of the given string occupy.
 If the string is shorter than n characters, returns the number of bytes
 the characters in the string occupy.
 @return length of the prefix, in bytes */
ulint dtype_get_at_most_n_mbchars(
    ulint prtype,      /*!< in: precise type */
    ulint mbminmaxlen, /*!< in: minimum and maximum length of
                       a multi-byte character */
    ulint prefix_len,  /*!< in: length of the requested
                       prefix, in characters, multiplied by
                       dtype_get_mbmaxlen(dtype) */
    ulint data_len,    /*!< in: length of str (in bytes) */
    const char *str)   /*!< in: the string whose prefix
                       length is being determined */
{
  ulint mbminlen = DATA_MBMINLEN(mbminmaxlen);
  ulint mbmaxlen = DATA_MBMAXLEN(mbminmaxlen);

  ut_a(rec_field_not_null_not_add_col_def(data_len));
  ut_ad(!mbmaxlen || !(prefix_len % mbmaxlen));

  if (mbminlen != mbmaxlen) {
    ut_a(!(prefix_len % mbmaxlen));
    return (innobase_get_at_most_n_mbchars(dtype_get_charset_coll(prtype),
                                           prefix_len, data_len, str));
  }

  if (prefix_len < data_len) {
    return (prefix_len);
  }

  return (data_len);
}

/** Checks if a data main type is a string type. Also a BLOB is considered a
 string type.
 @return true if string type */
bool dtype_is_string_type(
    ulint mtype) /*!< in: InnoDB main data type code: DATA_CHAR, ... */
{
  if (mtype <= DATA_BLOB || mtype == DATA_MYSQL || mtype == DATA_VARMYSQL) {
    return true;
  }

  return false;
}

/** Checks if a type is a binary string type. Note that for tables created with
 < 4.0.14, we do not know if a DATA_BLOB column is a BLOB or a TEXT column. For
 those DATA_BLOB columns this function currently returns false.
 @return true if binary string type */
bool dtype_is_binary_string_type(ulint mtype,  /*!< in: main data type */
                                 ulint prtype) /*!< in: precise type */
{
  if ((mtype == DATA_FIXBINARY) || (mtype == DATA_BINARY) ||
      (mtype == DATA_BLOB && (prtype & DATA_BINARY_TYPE))) {
    return true;
  }

  return false;
}

/** Checks if a type is a non-binary string type. That is, dtype_is_string_type
 is true and dtype_is_binary_string_type is false. Note that for tables created
 with < 4.0.14, we do not know if a DATA_BLOB column is a BLOB or a TEXT column.
 For those DATA_BLOB columns this function currently returns true.
 @return true if non-binary string type */
bool dtype_is_non_binary_string_type(ulint mtype,  /*!< in: main data type */
                                     ulint prtype) /*!< in: precise type */
{
  if (dtype_is_string_type(mtype) == true &&
      dtype_is_binary_string_type(mtype, prtype) == false) {
    return true;
  }

  return false;
}

/** Forms a precise type from the < 4.1.2 format precise type plus the
 charset-collation code.
 @return precise type, including the charset-collation code */
ulint dtype_form_prtype(
    ulint old_prtype,   /*!< in: the MySQL type code and the flags
                        DATA_BINARY_TYPE etc. */
    ulint charset_coll) /*!< in: MySQL charset-collation code */
{
  ut_a(old_prtype < 256 * 256);
  ut_a(charset_coll <= MAX_CHAR_COLL_NUM);

  return (old_prtype + (charset_coll << 16));
}

/** Validates a data type structure.
 @return true if ok */
bool dtype_validate(const dtype_t *type) /*!< in: type struct to validate */
{
  ut_a(type);
  ut_a(type->mtype >= DATA_VARCHAR);
  ut_a(type->mtype <= DATA_MTYPE_MAX);

  if (type->mtype == DATA_SYS) {
    ut_a((type->prtype & DATA_MYSQL_TYPE_MASK) < DATA_N_SYS_COLS);
  }

  ut_a(dtype_get_mbminlen(type) <= dtype_get_mbmaxlen(type));

  return true;
}

#ifdef UNIV_DEBUG
/** Print a data type structure.
@param[in]      type    data type */
void dtype_print(const dtype_t *type) {
  ulint mtype;
  ulint prtype;
  ulint len;

  ut_a(type);

  mtype = type->mtype;
  prtype = type->prtype;

  switch (mtype) {
    case DATA_VARCHAR:
      fputs("DATA_VARCHAR", stderr);
      break;

    case DATA_CHAR:
      fputs("DATA_CHAR", stderr);
      break;

    case DATA_BINARY:
      fputs("DATA_BINARY", stderr);
      break;

    case DATA_FIXBINARY:
      fputs("DATA_FIXBINARY", stderr);
      break;

    case DATA_BLOB:
      fputs("DATA_BLOB", stderr);
      break;

    case DATA_POINT:
      fputs("DATA_POINT", stderr);
      break;

    case DATA_VAR_POINT:
      fputs("DATA_VAR_POINT", stderr);
      break;

    case DATA_GEOMETRY:
      fputs("DATA_GEOMETRY", stderr);
      break;

    case DATA_INT:
      fputs("DATA_INT", stderr);
      break;

    case DATA_MYSQL:
      fputs("DATA_MYSQL", stderr);
      break;

    case DATA_SYS:
      fputs("DATA_SYS", stderr);
      break;

    case DATA_FLOAT:
      fputs("DATA_FLOAT", stderr);
      break;

    case DATA_DOUBLE:
      fputs("DATA_DOUBLE", stderr);
      break;

    case DATA_DECIMAL:
      fputs("DATA_DECIMAL", stderr);
      break;

    case DATA_VARMYSQL:
      fputs("DATA_VARMYSQL", stderr);
      break;

    default:
      fprintf(stderr, "type %lu", (ulong)mtype);
      break;
  }

  len = type->len;

  if ((type->mtype == DATA_SYS) || (type->mtype == DATA_VARCHAR) ||
      (type->mtype == DATA_CHAR)) {
    putc(' ', stderr);
    if (prtype == DATA_ROW_ID) {
      fputs("DATA_ROW_ID", stderr);
      len = DATA_ROW_ID_LEN;
    } else if (prtype == DATA_ROLL_PTR) {
      fputs("DATA_ROLL_PTR", stderr);
      len = DATA_ROLL_PTR_LEN;
    } else if (prtype == DATA_TRX_ID) {
      fputs("DATA_TRX_ID", stderr);
      len = DATA_TRX_ID_LEN;
    } else if (prtype == DATA_ENGLISH) {
      fputs("DATA_ENGLISH", stderr);
    } else {
      fprintf(stderr, "prtype %lu", (ulong)prtype);
    }
  } else {
    if (prtype & DATA_UNSIGNED) {
      fputs(" DATA_UNSIGNED", stderr);
    }

    if (prtype & DATA_BINARY_TYPE) {
      fputs(" DATA_BINARY_TYPE", stderr);
    }

    if (prtype & DATA_NOT_NULL) {
      fputs(" DATA_NOT_NULL", stderr);
    }
  }

  fprintf(stderr, " len %lu", (ulong)len);
}
#endif /* UNIV_DEBUG */

std::ostream &dtype_t::print(std::ostream &out) const {
  out << "[dtype_t: mtype=" << mtype << " (";

  switch (mtype) {
    case DATA_VARCHAR:
      out << "DATA_VARCHAR";
      break;

    case DATA_CHAR:
      out << "DATA_CHAR";
      break;

    case DATA_BINARY:
      out << "DATA_BINARY";
      break;

    case DATA_FIXBINARY:
      out << "DATA_FIXBINARY";
      break;

    case DATA_BLOB:
      out << "DATA_BLOB";
      break;

    case DATA_POINT:
      out << "DATA_POINT";
      break;

    case DATA_VAR_POINT:
      out << "DATA_VAR_POINT";
      break;

    case DATA_GEOMETRY:
      out << "DATA_GEOMETRY";
      break;

    case DATA_INT:
      out << "DATA_INT";
      break;

    case DATA_MYSQL:
      out << "DATA_MYSQL";
      break;

    case DATA_SYS:
      out << "DATA_SYS";
      break;

    case DATA_FLOAT:
      out << "DATA_FLOAT";
      break;

    case DATA_DOUBLE:
      out << "DATA_DOUBLE";
      break;

    case DATA_DECIMAL:
      out << "DATA_DECIMAL";
      break;

    case DATA_VARMYSQL:
      out << "DATA_VARMYSQL";
      break;

    default:
      out << "UKNOWN: " << mtype;
      break;
  }

  out << "), prtype=" << prtype << " (";

  if ((mtype == DATA_SYS) || (mtype == DATA_VARCHAR) || (mtype == DATA_CHAR)) {
    out << ' ';
    if (prtype == DATA_ROW_ID) {
      out << "DATA_ROW_ID";
    } else if (prtype == DATA_ROLL_PTR) {
      out << "DATA_ROLL_PTR";
    } else if (prtype == DATA_TRX_ID) {
      out << "DATA_TRX_ID";
    } else if (prtype == DATA_ENGLISH) {
      out << "DATA_ENGLISH";
    } else {
      out << "UNKNOWN: " << prtype;
    }
  } else {
    if (prtype & DATA_UNSIGNED) {
      out << " DATA_UNSIGNED";
    }

    if (prtype & DATA_BINARY_TYPE) {
      out << " DATA_BINARY_TYPE";
    }

    if (prtype & DATA_NOT_NULL) {
      out << " DATA_NOT_NULL";
    }

    if (prtype & DATA_VIRTUAL) {
      out << " DATA_VIRTUAL";
    }

    const auto other_flags = prtype & ~(DATA_UNSIGNED | DATA_BINARY_TYPE |
                                        DATA_NOT_NULL | DATA_VIRTUAL);

    if (other_flags != 0) {
      out << " UNKNOWN:" << other_flags;
    }
  }

  out << "), len=" << len << "]";

  return (out);
}
