/*****************************************************************************

Copyright (c) 1994, 2018, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file rem/rem0cmp.cc
 Comparison services for records

 Created 7/1/1994 Heikki Tuuri
 ************************************************************************/

#include <gstream.h>
#include <page0cur.h>
#include <spatial.h>
#include <sys/types.h>
#include <algorithm>

#include "ha_prototypes.h"
#include "handler0alter.h"
#include "my_compiler.h"
#include "my_inttypes.h"
#include "rem0cmp.h"
#include "srv0srv.h"

namespace dd {
class Spatial_reference_system;
}

/*		ALPHABETICAL ORDER
                ==================

The records are put into alphabetical order in the following
way: let F be the first field where two records disagree.
If there is a character in some position n where the
records disagree, the order is determined by comparison of
the characters at position n, possibly after
collating transformation. If there is no such character,
but the corresponding fields have different lengths, then
if the data type of the fields is paddable,
shorter field is padded with a padding character. If the
data type is not paddable, longer field is considered greater.
Finally, the SQL null is bigger than any other value.

At the present, the comparison functions return 0 in the case,
where two records disagree only in the way that one
has more fields than the other. */

/** Compare two data fields.
@param[in] prtype precise type
@param[in] a data field
@param[in] a_length length of a, in bytes (not UNIV_SQL_NULL)
@param[in] b data field
@param[in] b_length length of b, in bytes (not UNIV_SQL_NULL)
@return positive, 0, negative, if a is greater, equal, less than b,
respectively */
UNIV_INLINE
int innobase_mysql_cmp(ulint prtype, const byte *a, size_t a_length,
                       const byte *b, size_t b_length) {
#ifdef UNIV_DEBUG
  switch (prtype & DATA_MYSQL_TYPE_MASK) {
    case MYSQL_TYPE_BIT:
    case MYSQL_TYPE_STRING:
    case MYSQL_TYPE_VAR_STRING:
    case MYSQL_TYPE_TINY_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_BLOB:
    case MYSQL_TYPE_LONG_BLOB:
    case MYSQL_TYPE_VARCHAR:
      break;
    default:
      ut_error;
  }
#endif /* UNIV_DEBUG */

  uint cs_num = (uint)dtype_get_charset_coll(prtype);

  if (CHARSET_INFO *cs = get_charset(cs_num, MYF(MY_WME))) {
    if ((prtype & DATA_MYSQL_TYPE_MASK) == MYSQL_TYPE_STRING &&
        cs->pad_attribute == NO_PAD) {
      /* MySQL specifies that CHAR fields are stripped of
      trailing spaces before being returned from the database.
      Normally this is done in Field_string::val_str(),
      but since we don't involve the Field classes for internal
      index comparisons, we need to do the same thing here
      for NO PAD collations. (If not, strnncollsp will ignore
      the spaces for us, so we don't need to do it here.) */
      a_length = cs->cset->lengthsp(cs, (const char *)a, a_length);
      b_length = cs->cset->lengthsp(cs, (const char *)b, b_length);
    }

    return (cs->coll->strnncollsp(cs, a, a_length, b, b_length));
  }

  ib::fatal(ER_IB_MSG_919) << "Unable to find charset-collation " << cs_num;
  return (0);
}

/** Returns TRUE if two columns are equal for comparison purposes.
 @return true if the columns are considered equal in comparisons */
ibool cmp_cols_are_equal(const dict_col_t *col1, /*!< in: column 1 */
                         const dict_col_t *col2, /*!< in: column 2 */
                         ibool check_charsets)
/*!< in: whether to check charsets */
{
  if (dtype_is_non_binary_string_type(col1->mtype, col1->prtype) &&
      dtype_is_non_binary_string_type(col2->mtype, col2->prtype)) {
    /* Both are non-binary string types: they can be compared if
    and only if the charset-collation is the same */

    if (check_charsets) {
      return (dtype_get_charset_coll(col1->prtype) ==
              dtype_get_charset_coll(col2->prtype));
    } else {
      return (TRUE);
    }
  }

  if (dtype_is_binary_string_type(col1->mtype, col1->prtype) &&
      dtype_is_binary_string_type(col2->mtype, col2->prtype)) {
    /* Both are binary string types: they can be compared */

    return (TRUE);
  }

  if (col1->mtype != col2->mtype) {
    return (FALSE);
  }

  if (col1->mtype == DATA_INT &&
      (col1->prtype & DATA_UNSIGNED) != (col2->prtype & DATA_UNSIGNED)) {
    /* The storage format of an unsigned integer is different
    from a signed integer: in a signed integer we OR
    0x8000... to the value of positive integers. */

    return (FALSE);
  }

  return (col1->mtype != DATA_INT || col1->len == col2->len);
}

/** Compare two DATA_DECIMAL (MYSQL_TYPE_DECIMAL) fields.
TODO: Remove this function. Everything should use MYSQL_TYPE_NEWDECIMAL.
@param[in]	a		data field
@param[in]	a_length	length of a, in bytes (not UNIV_SQL_NULL)
@param[in]	b		data field
@param[in]	b_length	length of b, in bytes (not UNIV_SQL_NULL)
@param[in]	is_asc		true=ascending, false=descending order
@return positive, 0, negative, if a is greater, equal, less than b,
respectively */
static UNIV_COLD int cmp_decimal(const byte *a, unsigned int a_length,
                                 const byte *b, unsigned int b_length,
                                 bool is_asc) {
  int swap_flag = is_asc ? 1 : -1;

  /* Remove preceding spaces */
  for (; a_length && *a == ' '; a++, a_length--) {
  }
  for (; b_length && *b == ' '; b++, b_length--) {
  }

  if (*a == '-') {
    swap_flag = -swap_flag;

    if (*b != '-') {
      return (swap_flag);
    }

    a++;
    b++;
    a_length--;
    b_length--;
  } else {
    if (*b == '-') {
      return (swap_flag);
    }
  }

  while (a_length > 0 && (*a == '+' || *a == '0')) {
    a++;
    a_length--;
  }

  while (b_length > 0 && (*b == '+' || *b == '0')) {
    b++;
    b_length--;
  }

  if (a_length != b_length) {
    if (a_length < b_length) {
      return (-swap_flag);
    }

    return (swap_flag);
  }

  while (a_length > 0 && *a == *b) {
    a++;
    b++;
    a_length--;
  }

  if (a_length == 0) {
    return (0);
  }

  if (*a <= *b) {
    swap_flag = -swap_flag;
  }

  return (swap_flag);
}

/** Innobase uses this function to compare two geometry data fields
 @return	1, 0, -1, if a is greater, equal, less than b, respectively */
static int cmp_geometry_field(ulint mtype,           /*!< in: main type */
                              ulint prtype,          /*!< in: precise type */
                              const byte *a,         /*!< in: data field */
                              unsigned int a_length, /*!< in: data field length,
                                                     not UNIV_SQL_NULL */
                              const byte *b,         /*!< in: data field */
                              unsigned int b_length) /*!< in: data field length,
                                                     not UNIV_SQL_NULL */
{
  double x1, x2;
  double y1, y2;

  ut_ad(prtype & DATA_GIS_MBR);

  if (a_length < sizeof(double) || b_length < sizeof(double)) {
    return (0);
  }

  /* Try to compare mbr left lower corner (xmin, ymin) */
  x1 = mach_double_read(a);
  x2 = mach_double_read(b);
  y1 = mach_double_read(a + sizeof(double) * SPDIMS);
  y2 = mach_double_read(b + sizeof(double) * SPDIMS);

  if (x1 > x2) {
    return (1);
  } else if (x2 > x1) {
    return (-1);
  }

  if (y1 > y2) {
    return (1);
  } else if (y2 > y1) {
    return (-1);
  }

  /* left lower corner (xmin, ymin) overlaps, now right upper corner */
  x1 = mach_double_read(a + sizeof(double));
  x2 = mach_double_read(b + sizeof(double));
  y1 = mach_double_read(a + sizeof(double) * SPDIMS + sizeof(double));
  y2 = mach_double_read(b + sizeof(double) * SPDIMS + sizeof(double));

  if (x1 > x2) {
    return (1);
  } else if (x2 > x1) {
    return (-1);
  }

  if (y1 > y2) {
    return (1);
  } else if (y2 > y1) {
    return (-1);
  }

  return (0);
}
/** Innobase uses this function to compare two gis data fields
 @return	1, 0, -1, if mode == PAGE_CUR_MBR_EQUAL. And return
 1, 0 for rest compare modes, depends on a and b qualifies the
 relationship (CONTAINT, WITHIN etc.) */
static int cmp_gis_field(
    page_cur_mode_t mode,                    /*!< in: compare mode */
    const byte *a,                           /*!< in: data field */
    unsigned int a_length,                   /*!< in: data field length,
                                             not UNIV_SQL_NULL */
    const byte *b,                           /*!< in: data field */
    unsigned int b_length,                   /*!< in: data field length,
                                             not UNIV_SQL_NULL */
    const dd::Spatial_reference_system *srs) /*!< in: SRS of R-tree */
{
  if (mode == PAGE_CUR_MBR_EQUAL) {
    /* TODO: Since the DATA_GEOMETRY is not used in compare
    function, we could pass it instead of a specific type now */
    return (cmp_geometry_field(DATA_GEOMETRY, DATA_GIS_MBR, a, a_length, b,
                               b_length));
  } else {
    return (rtree_key_cmp(mode, a, a_length, b, b_length, srs));
  }
}

/** Compare two data fields.
@param[in]	mtype		main type
@param[in]	prtype		precise type
@param[in]	is_asc		true=ascending, false=descending order
@param[in]	a		data field
@param[in]	a_length	length of a, in bytes (not UNIV_SQL_NULL)
@param[in]	b		data field
@param[in]	b_length	length of b, in bytes (not UNIV_SQL_NULL)
@return positive, 0, negative, if a is greater, equal, less than b,
respectively */
static int cmp_whole_field(ulint mtype, ulint prtype, bool is_asc,
                           const byte *a, unsigned int a_length, const byte *b,
                           unsigned int b_length) {
  float f_1;
  float f_2;
  double d_1;
  double d_2;
  int cmp;

  switch (mtype) {
    case DATA_DECIMAL:
      return (cmp_decimal(a, a_length, b, b_length, is_asc));
    case DATA_DOUBLE:
      d_1 = mach_double_read(a);
      d_2 = mach_double_read(b);

      if (d_1 > d_2) {
        return (is_asc ? 1 : -1);
      } else if (d_2 > d_1) {
        return (is_asc ? -1 : 1);
      }

      return (0);

    case DATA_FLOAT:
      f_1 = mach_float_read(a);
      f_2 = mach_float_read(b);

      if (f_1 > f_2) {
        return (is_asc ? 1 : -1);
      } else if (f_2 > f_1) {
        return (is_asc ? -1 : 1);
      }

      return (0);
    case DATA_VARCHAR:
    case DATA_CHAR:
      cmp = my_charset_latin1.coll->strnncollsp(&my_charset_latin1, a, a_length,
                                                b, b_length);
      break;
    case DATA_BLOB:
      if (prtype & DATA_BINARY_TYPE) {
        ib::error(ER_IB_MSG_920) << "Comparing a binary BLOB"
                                    " using a character set collation!";
        ut_ad(0);
      }
      /* fall through */
    case DATA_VARMYSQL:
    case DATA_MYSQL:
      cmp = innobase_mysql_cmp(prtype, a, a_length, b, b_length);
      break;
    case DATA_POINT:
    case DATA_VAR_POINT:
    case DATA_GEOMETRY:
      return (cmp_geometry_field(mtype, prtype, a, a_length, b, b_length));
    default:
      ib::fatal(ER_IB_MSG_921) << "Unknown data type number " << mtype;
      cmp = 0;
  }
  if (!is_asc) {
    cmp = -cmp;
  }
  return (cmp);
}

/** Compare two data fields.
@param[in]	mtype	main type
@param[in]	prtype	precise type
@param[in]	is_asc	true=ascending, false=descending order
@param[in]	data1	data field
@param[in]	len1	length of data1 in bytes, or UNIV_SQL_NULL
@param[in]	data2	data field
@param[in]	len2	length of data2 in bytes, or UNIV_SQL_NULL
@return the comparison result of data1 and data2
@retval 0 if data1 is equal to data2
@retval negative if data1 is less than data2
@retval positive if data1 is greater than data2 */
inline int cmp_data(ulint mtype, ulint prtype, bool is_asc, const byte *data1,
                    ulint len1, const byte *data2, ulint len2) {
  if (len1 == UNIV_SQL_NULL || len2 == UNIV_SQL_NULL) {
    if (len1 == len2) {
      return (0);
    }

    /* We define the SQL null to be the smallest possible
    value of a field. */
    return ((len1 == UNIV_SQL_NULL) == is_asc ? -1 : 1);
  }

  ulint pad;

  switch (mtype) {
    case DATA_FIXBINARY:
    case DATA_BINARY:
      if (dtype_get_charset_coll(prtype) != DATA_MYSQL_BINARY_CHARSET_COLL) {
        pad = 0x20;
        break;
      }
      /* fall through */
    case DATA_INT:
    case DATA_SYS_CHILD:
    case DATA_SYS:
      pad = ULINT_UNDEFINED;
      break;
    case DATA_POINT:
    case DATA_VAR_POINT:
      /* Since DATA_POINT has a fixed length of DATA_POINT_LEN,
      currently, pad is not needed. Meanwhile, DATA_VAR_POINT acts
      the same as DATA_GEOMETRY */
    case DATA_GEOMETRY:
      ut_ad(prtype & DATA_BINARY_TYPE);
      pad = ULINT_UNDEFINED;
      if (prtype & DATA_GIS_MBR) {
        return (cmp_whole_field(mtype, prtype, is_asc, data1, (unsigned)len1,
                                data2, (unsigned)len2));
      }
      break;
    case DATA_BLOB:
      if (prtype & DATA_BINARY_TYPE) {
        pad = ULINT_UNDEFINED;
        break;
      }
      /* fall through */
    default:
      return (cmp_whole_field(mtype, prtype, is_asc, data1, (unsigned)len1,
                              data2, (unsigned)len2));
  }

  ulint len;
  int cmp;

  if (len1 < len2) {
    len = len1;
    len2 -= len;
    len1 = 0;
  } else {
    len = len2;
    len1 -= len;
    len2 = 0;
  }

  if (len) {
#if defined __i386__ || defined __x86_64__ || defined _M_IX86 || defined _M_X64
    /* Compare the first bytes with a loop to avoid the call
    overhead of memcmp(). On x86 and x86-64, the GCC built-in
    (repz cmpsb) seems to be very slow, so we will be calling the
    libc version. http://gcc.gnu.org/bugzilla/show_bug.cgi?id=43052
    tracks the slowness of the GCC built-in memcmp().

    We compare up to the first 4..7 bytes with the loop.
    The (len & 3) is used for "normalizing" or
    "quantizing" the len parameter for the memcmp() call,
    in case the whole prefix is equal. On x86 and x86-64,
    the GNU libc memcmp() of equal strings is faster with
    len=4 than with len=3.

    On other architectures than the IA32 or AMD64, there could
    be a built-in memcmp() that is faster than the loop.
    We only use the loop where we know that it can improve
    the performance. */
    for (ulint i = 4 + (len & 3); i > 0; i--) {
      cmp = int(*data1++) - int(*data2++);
      if (cmp) {
        goto func_exit;
      }

      if (!--len) {
        break;
      }
    }

    if (len) {
#endif /* IA32 or AMD64 */
      cmp = memcmp(data1, data2, len);

      if (cmp) {
        goto func_exit;
      }

      data1 += len;
      data2 += len;
#if defined __i386__ || defined __x86_64__ || defined _M_IX86 || defined _M_X64
    }
#endif /* IA32 or AMD64 */
  }

  cmp = (int)(len1 - len2);

  if (!cmp) {
    return (cmp);
  }

  if (pad == ULINT_UNDEFINED) {
    goto func_exit;
  }

  len = 0;

  if (len1) {
    do {
      cmp = static_cast<int>(mach_read_from_1(&data1[len++]) - pad);
    } while (cmp == 0 && len < len1);
  } else {
    ut_ad(len2 > 0);

    do {
      cmp = static_cast<int>(pad - mach_read_from_1(&data2[len++]));
    } while (cmp == 0 && len < len2);
  }

func_exit:
  return (is_asc ? cmp : -cmp);
}

/** Compare a GIS data tuple to a physical record.
@param[in] dtuple data tuple
@param[in] rec B-tree record
@param[in] offsets rec_get_offsets(rec)
@param[in] mode compare mode
@param[in] srs Spatial reference system of R-tree
@retval negative if dtuple is less than rec */
int cmp_dtuple_rec_with_gis(
    const dtuple_t *dtuple, /*!< in: data tuple */
    const rec_t *rec,       /*!< in: physical record which differs from
                            dtuple in some of the common fields, or which
                            has an equal number or more fields than
                            dtuple */
    const ulint *offsets,   /*!< in: array returned by rec_get_offsets() */
    page_cur_mode_t mode,   /*!< in: compare mode */
    const dd::Spatial_reference_system *srs) /*!< in: SRS of R-tree */
{
  const dfield_t *dtuple_field; /* current field in logical record */
  ulint dtuple_f_len;           /* the length of the current field
                                in the logical record */
  ulint rec_f_len;              /* length of current field in rec */
  const byte *rec_b_ptr;        /* pointer to the current byte in
                                rec field */
  int ret = 0;                  /* return value */

  dtuple_field = dtuple_get_nth_field(dtuple, 0);
  dtuple_f_len = dfield_get_len(dtuple_field);

  rec_b_ptr = rec_get_nth_field(rec, offsets, 0, nullptr, &rec_f_len);
  ret = cmp_gis_field(
      mode, static_cast<const byte *>(dfield_get_data(dtuple_field)),
      (unsigned)dtuple_f_len, rec_b_ptr, (unsigned)rec_f_len, srs);

  return (ret);
}

/** Compare a GIS data tuple to a physical record in rtree non-leaf node.
We need to check the page number field, since we don't store pk field in
rtree non-leaf node.
@param[in]	dtuple		data tuple
@param[in]	rec		R-tree record
@param[in]	offsets		rec_get_offsets(rec)
@param[in]	srs	        Spatial reference system of R-tree
@retval negative if dtuple is less than rec */
int cmp_dtuple_rec_with_gis_internal(const dtuple_t *dtuple, const rec_t *rec,
                                     const ulint *offsets,
                                     const dd::Spatial_reference_system *srs) {
  const dfield_t *dtuple_field; /* current field in logical record */
  ulint dtuple_f_len;           /* the length of the current field
                                in the logical record */
  ulint rec_f_len;              /* length of current field in rec */
  const byte *rec_b_ptr;        /* pointer to the current byte in
                                rec field */
  int ret = 0;                  /* return value */

  dtuple_field = dtuple_get_nth_field(dtuple, 0);
  dtuple_f_len = dfield_get_len(dtuple_field);

  rec_b_ptr = rec_get_nth_field(rec, offsets, 0, nullptr, &rec_f_len);
  ret = cmp_gis_field(
      PAGE_CUR_WITHIN, static_cast<const byte *>(dfield_get_data(dtuple_field)),
      (unsigned)dtuple_f_len, rec_b_ptr, (unsigned)rec_f_len, srs);
  if (ret != 0) {
    return (ret);
  }

  dtuple_field = dtuple_get_nth_field(dtuple, 1);
  dtuple_f_len = dfield_get_len(dtuple_field);
  rec_b_ptr = rec_get_nth_field(rec, offsets, 1, nullptr, &rec_f_len);

  return (cmp_data(dtuple_field->type.mtype, dtuple_field->type.prtype, true,
                   static_cast<const byte *>(dtuple_field->data), dtuple_f_len,
                   rec_b_ptr, rec_f_len));
}

/** Compare two data fields.
@param[in]	mtype	main type
@param[in]	prtype	precise type
@param[in]	is_asc	true=ascending, false=descending order
@param[in]	data1	data field
@param[in]	len1	length of data1 in bytes, or UNIV_SQL_NULL
@param[in]	data2	data field
@param[in]	len2	length of data2 in bytes, or UNIV_SQL_NULL
@return the comparison result of data1 and data2
@retval 0 if data1 is equal to data2
@retval negative if data1 is less than data2
@retval positive if data1 is greater than data2 */
int cmp_data_data(ulint mtype, ulint prtype, bool is_asc, const byte *data1,
                  ulint len1, const byte *data2, ulint len2) {
  return (cmp_data(mtype, prtype, is_asc, data1, len1, data2, len2));
}

/** Compare a data tuple to a physical record.
@param[in]	dtuple		data tuple
@param[in]	rec		record
@param[in]	index		index
@param[in]	offsets		rec_get_offsets(rec)
@param[in]	n_cmp		number of fields to compare
@param[in,out]	matched_fields	number of completely matched fields
@return the comparison result of dtuple and rec
@retval 0 if dtuple is equal to rec
@retval negative if dtuple is less than rec
@retval positive if dtuple is greater than rec */
int cmp_dtuple_rec_with_match_low(const dtuple_t *dtuple, const rec_t *rec,
                                  const dict_index_t *index,
                                  const ulint *offsets, ulint n_cmp,
                                  ulint *matched_fields) {
  ulint cur_field; /* current field number */
  int ret;         /* return value */

  ut_ad(dtuple_check_typed(dtuple));
  ut_ad(rec_offs_validate(rec, index, offsets));

  cur_field = *matched_fields;

  ut_ad(n_cmp > 0);
  ut_ad(n_cmp <= dtuple_get_n_fields(dtuple));
  ut_ad(cur_field <= n_cmp);
  ut_ad(cur_field <= rec_offs_n_fields(offsets));

  if (cur_field == 0) {
    ulint rec_info = rec_get_info_bits(rec, rec_offs_comp(offsets));
    ulint tup_info = dtuple_get_info_bits(dtuple);

    /* The leftmost node pointer record is defined as
    smaller than any other node pointer, independent of
    any ASC/DESC flags. It is an "infimum node pointer". */
    if (UNIV_UNLIKELY(rec_info & REC_INFO_MIN_REC_FLAG)) {
      ret = !(tup_info & REC_INFO_MIN_REC_FLAG);
      goto order_resolved;
    } else if (UNIV_UNLIKELY(tup_info & REC_INFO_MIN_REC_FLAG)) {
      ret = -1;
      goto order_resolved;
    }
  }

  /* Match fields in a loop */

  for (; cur_field < n_cmp; cur_field++) {
    const byte *rec_b_ptr;
    const dfield_t *dtuple_field = dtuple_get_nth_field(dtuple, cur_field);
    const byte *dtuple_b_ptr =
        static_cast<const byte *>(dfield_get_data(dtuple_field));
    const dtype_t *type = dfield_get_type(dtuple_field);
    ulint dtuple_f_len = dfield_get_len(dtuple_field);
    ulint rec_f_len;

    /* We should never compare against an externally
    stored field.  Only clustered index records can
    contain externally stored fields, and the first fields
    (primary key fields) should already differ. */
    ut_ad(!rec_offs_nth_extern(offsets, cur_field));
    /* So does the field with default value */
    ut_ad(!rec_offs_nth_default(offsets, cur_field));

    rec_b_ptr = rec_get_nth_field(rec, offsets, cur_field, nullptr, &rec_f_len);

    ut_ad(!dfield_is_ext(dtuple_field));

    /* For now, change buffering is only supported on
    indexes with ascending order on the columns. */
    ret = cmp_data(
        type->mtype, type->prtype,
        dict_index_is_ibuf(index) || index->get_field(cur_field)->is_ascending,
        dtuple_b_ptr, dtuple_f_len, rec_b_ptr, rec_f_len);
    if (ret) {
      goto order_resolved;
    }
  }

  ret = 0; /* If we ran out of fields, dtuple was equal to rec
           up to the common fields */
order_resolved:
  *matched_fields = cur_field;
  return (ret);
}

/** Get the pad character code point for a type.
@param[in]	type
@return		pad character code point
@retval		ULINT_UNDEFINED if no padding is specified */
UNIV_INLINE
ulint cmp_get_pad_char(const dtype_t *type) {
  switch (type->mtype) {
    case DATA_FIXBINARY:
    case DATA_BINARY:
      if (dtype_get_charset_coll(type->prtype) ==
          DATA_MYSQL_BINARY_CHARSET_COLL) {
        /* Starting from 5.0.18, do not pad
        VARBINARY or BINARY columns. */
        return (ULINT_UNDEFINED);
      }
      /* Fall through */
    case DATA_CHAR:
    case DATA_VARCHAR:
    case DATA_MYSQL:
    case DATA_VARMYSQL:
      /* Space is the padding character for all char and binary
      strings, and starting from 5.0.3, also for TEXT strings. */
      return (0x20);
    case DATA_GEOMETRY:
      /* DATA_GEOMETRY is binary data, not ASCII-based. */
      return (ULINT_UNDEFINED);
    case DATA_BLOB:
      if (!(type->prtype & DATA_BINARY_TYPE)) {
        return (0x20);
      }
      /* Fall through */
    default:
      /* No padding specified */
      return (ULINT_UNDEFINED);
  }
}

/** Compare a data tuple to a physical record.
@param[in]	dtuple		data tuple
@param[in]	rec		B-tree or R-tree index record
@param[in]	index		index tree
@param[in]	offsets		rec_get_offsets(rec)
@param[in,out]	matched_fields	number of completely matched fields
@param[in,out]	matched_bytes	number of matched bytes in the first
field that is not matched
@return the comparison result of dtuple and rec
@retval 0 if dtuple is equal to rec
@retval negative if dtuple is less than rec
@retval positive if dtuple is greater than rec */
int cmp_dtuple_rec_with_match_bytes(const dtuple_t *dtuple, const rec_t *rec,
                                    const dict_index_t *index,
                                    const ulint *offsets, ulint *matched_fields,
                                    ulint *matched_bytes) {
  ulint n_cmp = dtuple_get_n_fields_cmp(dtuple);
  ulint cur_field; /* current field number */
  ulint cur_bytes;
  int ret; /* return value */

  ut_ad(dtuple_check_typed(dtuple));
  ut_ad(rec_offs_validate(rec, index, offsets));
  // ut_ad(page_is_leaf(page_align(rec)));
  ut_ad(!(REC_INFO_MIN_REC_FLAG & dtuple_get_info_bits(dtuple)));
  ut_ad(!(REC_INFO_MIN_REC_FLAG &
          rec_get_info_bits(rec, rec_offs_comp(offsets))));

  cur_field = *matched_fields;
  cur_bytes = *matched_bytes;

  ut_ad(n_cmp <= dtuple_get_n_fields(dtuple));
  ut_ad(cur_field <= n_cmp);
  ut_ad(cur_field + (cur_bytes > 0) <= rec_offs_n_fields(offsets));

  /* Match fields in a loop; stop if we run out of fields in dtuple
  or find an externally stored field */

  while (cur_field < n_cmp) {
    const dfield_t *dfield = dtuple_get_nth_field(dtuple, cur_field);
    const dtype_t *type = dfield_get_type(dfield);
    ulint dtuple_f_len = dfield_get_len(dfield);
    const byte *dtuple_b_ptr;
    const byte *rec_b_ptr;
    ulint rec_f_len;

    ut_ad(!rec_offs_nth_default(offsets, cur_field));

    /* For now, change buffering is only supported on
    indexes with ascending order on the columns. */
    const bool is_ascending =
        dict_index_is_ibuf(index) || index->fields[cur_field].is_ascending;

    dtuple_b_ptr = static_cast<const byte *>(dfield_get_data(dfield));
    rec_b_ptr = rec_get_nth_field(rec, offsets, cur_field, nullptr, &rec_f_len);
    ut_ad(!rec_offs_nth_extern(offsets, cur_field));

    /* If we have matched yet 0 bytes, it may be that one or
    both the fields are SQL null, or the record or dtuple may be
    the predefined minimum record. */
    if (cur_bytes == 0) {
      if (dtuple_f_len == UNIV_SQL_NULL) {
        if (rec_f_len == UNIV_SQL_NULL) {
          goto next_field;
        }

        ret = is_ascending ? -1 : 1;
        goto order_resolved;
      } else if (rec_f_len == UNIV_SQL_NULL) {
        /* We define the SQL null to be the
        smallest possible value of a field
        in the alphabetical order */

        ret = is_ascending ? 1 : -1;
        goto order_resolved;
      }
    }

    switch (type->mtype) {
      case DATA_FIXBINARY:
      case DATA_BINARY:
      case DATA_INT:
      case DATA_SYS_CHILD:
      case DATA_SYS:
        break;
      case DATA_BLOB:
        if (type->prtype & DATA_BINARY_TYPE) {
          break;
        }
        /* fall through */
      default:
        ret = cmp_data(type->mtype, type->prtype, is_ascending, dtuple_b_ptr,
                       dtuple_f_len, rec_b_ptr, rec_f_len);

        if (!ret) {
          goto next_field;
        }

        cur_bytes = 0;
        goto order_resolved;
    }

    /* Set the pointers at the current byte */

    rec_b_ptr += cur_bytes;
    dtuple_b_ptr += cur_bytes;
    /* Compare then the fields */

    for (const ulint pad = cmp_get_pad_char(type);; cur_bytes++) {
      ulint rec_byte = pad;
      ulint dtuple_byte = pad;

      if (rec_f_len <= cur_bytes) {
        if (dtuple_f_len <= cur_bytes) {
          goto next_field;
        }

        if (rec_byte == ULINT_UNDEFINED) {
          ret = is_ascending ? 1 : -1;
          goto order_resolved;
        }
      } else {
        rec_byte = *rec_b_ptr++;
      }

      if (dtuple_f_len <= cur_bytes) {
        if (dtuple_byte == ULINT_UNDEFINED) {
          ret = is_ascending ? -1 : 1;
          goto order_resolved;
        }
      } else {
        dtuple_byte = *dtuple_b_ptr++;
      }

      if (dtuple_byte < rec_byte) {
        ret = is_ascending ? -1 : 1;
        goto order_resolved;
      } else if (dtuple_byte > rec_byte) {
        ret = is_ascending ? 1 : -1;
        goto order_resolved;
      }
    }

  next_field:
    cur_field++;
    cur_bytes = 0;
  }

  ut_ad(cur_bytes == 0);

  ret = 0; /* If we ran out of fields, dtuple was equal to rec
           up to the common fields */
order_resolved:
  *matched_fields = cur_field;
  *matched_bytes = cur_bytes;

  return (ret);
}

/** Compare a data tuple to a physical record.
@see cmp_dtuple_rec_with_match
@param[in]	dtuple	data tuple
@param[in]	rec	record
@param[in]	index	index
@param[in]	offsets	rec_get_offsets(rec)
@return the comparison result of dtuple and rec
@retval 0 if dtuple is equal to rec
@retval negative if dtuple is less than rec
@retval positive if dtuple is greater than rec */
int cmp_dtuple_rec(const dtuple_t *dtuple, const rec_t *rec,
                   const dict_index_t *index, const ulint *offsets) {
  ulint matched_fields = 0;

  return (
      cmp_dtuple_rec_with_match(dtuple, rec, index, offsets, &matched_fields));
}

/** Checks if a dtuple is a prefix of a record. The last field in dtuple is
allowed to be a prefix of the corresponding field in the record.
@param[in]	dtuple	data tuple
@param[in]	rec	B-tree record
@param[in]	index	B-tree index
@param[in]	offsets	rec_get_offsets(rec)
@return true if prefix */
ibool cmp_dtuple_is_prefix_of_rec(const dtuple_t *dtuple, const rec_t *rec,
                                  const dict_index_t *index,
                                  const ulint *offsets) {
  ut_ad(!dict_index_is_spatial(index));

  ulint n_fields;
  ulint matched_fields = 0;

  n_fields = dtuple_get_n_fields(dtuple);

  if (n_fields > rec_offs_n_fields(offsets)) {
    ut_ad(0);
    return (FALSE);
  }

  return (
      !cmp_dtuple_rec_with_match(dtuple, rec, index, offsets, &matched_fields));
}

/** Compare two physical record fields.
 @retval positive if rec1 field is greater than rec2
 @retval negative if rec1 field is less than rec2
 @retval 0 if rec1 field equals to rec2 */
static MY_ATTRIBUTE((warn_unused_result)) int cmp_rec_rec_simple_field(
    const rec_t *rec1,         /*!< in: physical record */
    const rec_t *rec2,         /*!< in: physical record */
    const ulint *offsets1,     /*!< in: rec_get_offsets(rec1, ...) */
    const ulint *offsets2,     /*!< in: rec_get_offsets(rec2, ...) */
    const dict_index_t *index, /*!< in: data dictionary index */
    ulint n)                   /*!< in: field to compare */
{
  const byte *rec1_b_ptr;
  const byte *rec2_b_ptr;
  ulint rec1_f_len;
  ulint rec2_f_len;
  const dict_col_t *col = index->get_col(n);
  const dict_field_t *field = index->get_field(n);

  ut_ad(!rec_offs_nth_extern(offsets1, n));
  ut_ad(!rec_offs_nth_extern(offsets2, n));

  rec1_b_ptr = rec_get_nth_field(rec1, offsets1, n, index, &rec1_f_len);
  rec2_b_ptr = rec_get_nth_field(rec2, offsets2, n, index, &rec2_f_len);

  return (cmp_data(col->mtype, col->prtype, field->is_ascending, rec1_b_ptr,
                   rec1_f_len, rec2_b_ptr, rec2_f_len));
}

/** Compare two physical records that contain the same number of columns,
none of which are stored externally.
@retval positive if rec1 (including non-ordering columns) is greater than rec2
@retval negative if rec1 (including non-ordering columns) is less than rec2
@retval 0 if rec1 is a duplicate of rec2 */
int cmp_rec_rec_simple(
    const rec_t *rec1,         /*!< in: physical record */
    const rec_t *rec2,         /*!< in: physical record */
    const ulint *offsets1,     /*!< in: rec_get_offsets(rec1, ...) */
    const ulint *offsets2,     /*!< in: rec_get_offsets(rec2, ...) */
    const dict_index_t *index, /*!< in: data dictionary index */
    struct TABLE *table)       /*!< in: MySQL table, for reporting
                               duplicate key value if applicable,
                               or NULL */
{
  ulint n;
  ulint n_uniq = dict_index_get_n_unique(index);
  bool null_eq = false;

  ut_ad(rec_offs_n_fields(offsets1) >= n_uniq);
  ut_ad(rec_offs_n_fields(offsets2) == rec_offs_n_fields(offsets2));

  ut_ad(rec_offs_comp(offsets1) == rec_offs_comp(offsets2));

  for (n = 0; n < n_uniq; n++) {
    int cmp =
        cmp_rec_rec_simple_field(rec1, rec2, offsets1, offsets2, index, n);

    if (cmp) {
      return (cmp);
    }

    /* If the fields are internally equal, they must both
    be NULL or non-NULL. */
    ut_ad(rec_offs_nth_sql_null(offsets1, n) ==
          rec_offs_nth_sql_null(offsets2, n));

    if (rec_offs_nth_sql_null(offsets1, n)) {
      ut_ad(!(index->get_col(n)->prtype & DATA_NOT_NULL));
      null_eq = true;
    }
  }

  /* If we ran out of fields, the ordering columns of rec1 were
  equal to rec2. Issue a duplicate key error if needed. */

  if (!null_eq && table && dict_index_is_unique(index)) {
    /* Report erroneous row using new version of table. */
    innobase_rec_to_mysql(table, rec1, index, offsets1);
    return (0);
  }

  /* Else, keep comparing so that we have the full internal
  order. */
  for (; n < dict_index_get_n_fields(index); n++) {
    int cmp =
        cmp_rec_rec_simple_field(rec1, rec2, offsets1, offsets2, index, n);

    if (cmp) {
      return (cmp);
    }

    /* If the fields are internally equal, they must both
    be NULL or non-NULL. */
    ut_ad(rec_offs_nth_sql_null(offsets1, n) ==
          rec_offs_nth_sql_null(offsets2, n));
  }

  /* This should never be reached. Internally, an index must
  never contain duplicate entries. */
  ut_ad(0);
  return (0);
}

/** Compare two B-tree records.
@param[in] rec1 B-tree record
@param[in] rec2 B-tree record
@param[in] offsets1 rec_get_offsets(rec1, index)
@param[in] offsets2 rec_get_offsets(rec2, index)
@param[in] index B-tree index
@param[in] nulls_unequal true if this is for index cardinality
statistics estimation, and innodb_stats_method=nulls_unequal
or innodb_stats_method=nulls_ignored
@param[out] matched_fields number of completely matched fields
within the first field not completely matched
@return the comparison result
@retval 0 if rec1 is equal to rec2
@retval negative if rec1 is less than rec2
@retval positive if rec2 is greater than rec2 */
int cmp_rec_rec_with_match(const rec_t *rec1, const rec_t *rec2,
                           const ulint *offsets1, const ulint *offsets2,
                           const dict_index_t *index, bool nulls_unequal,
                           ulint *matched_fields) {
  ulint rec1_n_fields;    /* the number of fields in rec */
  ulint rec1_f_len;       /* length of current field in rec */
  const byte *rec1_b_ptr; /* pointer to the current byte
                          in rec field */
  ulint rec2_n_fields;    /* the number of fields in rec */
  ulint rec2_f_len;       /* length of current field in rec */
  const byte *rec2_b_ptr; /* pointer to the current byte
                          in rec field */
  ulint cur_field = 0;    /* current field number */
  int ret = 0;            /* return value */
  ulint comp;

  ut_ad(rec1 != NULL);
  ut_ad(rec2 != NULL);
  ut_ad(index != NULL);
  ut_ad(rec_offs_validate(rec1, index, offsets1));
  ut_ad(rec_offs_validate(rec2, index, offsets2));
  ut_ad(rec_offs_comp(offsets1) == rec_offs_comp(offsets2));

  comp = rec_offs_comp(offsets1);
  rec1_n_fields = rec_offs_n_fields(offsets1);
  rec2_n_fields = rec_offs_n_fields(offsets2);

  /* Test if rec is the predefined minimum record */
  if (UNIV_UNLIKELY(rec_get_info_bits(rec1, comp) & REC_INFO_MIN_REC_FLAG)) {
    /* There should only be one such record. */
    ut_ad(!(rec_get_info_bits(rec2, comp) & REC_INFO_MIN_REC_FLAG));
    ret = -1;
    goto order_resolved;
  } else if (UNIV_UNLIKELY(rec_get_info_bits(rec2, comp) &
                           REC_INFO_MIN_REC_FLAG)) {
    ret = 1;
    goto order_resolved;
  }

  /* Match fields in a loop */

  for (; cur_field < rec1_n_fields && cur_field < rec2_n_fields; cur_field++) {
    ulint mtype;
    ulint prtype;
    bool is_asc;

    /* If this is node-ptr records then avoid comparing node-ptr
    field. Only key field needs to be compared. */
    if (cur_field == dict_index_get_n_unique_in_tree(index)) {
      break;
    }

    if (dict_index_is_ibuf(index)) {
      /* This is for the insert buffer B-tree. */
      mtype = DATA_BINARY;
      prtype = 0;
      is_asc = true;
    } else {
      const dict_col_t *col;
      const dict_field_t *field = index->get_field(cur_field);

      col = index->get_col(cur_field);

      mtype = col->mtype;
      prtype = col->prtype;
      is_asc = field->is_ascending;

      /* If the index is spatial index, we mark the
      prtype of the first field as MBR field. */
      if (cur_field == 0 && dict_index_is_spatial(index)) {
        ut_ad(DATA_GEOMETRY_MTYPE(mtype));
        prtype |= DATA_GIS_MBR;
      }
    }

    /* We should never encounter an externally stored field.
    Externally stored fields only exist in clustered index
    leaf page records. These fields should already differ
    in the primary key columns already, before DB_TRX_ID,
    DB_ROLL_PTR, and any externally stored columns. */
    ut_ad(!rec_offs_nth_extern(offsets1, cur_field));
    ut_ad(!rec_offs_nth_extern(offsets2, cur_field));

    rec1_b_ptr =
        rec_get_nth_field(rec1, offsets1, cur_field, index, &rec1_f_len);
    rec2_b_ptr =
        rec_get_nth_field(rec2, offsets2, cur_field, index, &rec2_f_len);

    if (nulls_unequal && rec1_f_len == UNIV_SQL_NULL &&
        rec2_f_len == UNIV_SQL_NULL) {
      ret = -1;
      goto order_resolved;
    }

    ret = cmp_data(mtype, prtype, is_asc, rec1_b_ptr, rec1_f_len, rec2_b_ptr,
                   rec2_f_len);
    if (ret) {
      goto order_resolved;
    }
  }

  /* If we ran out of fields, rec1 was equal to rec2 up
  to the common fields */
  ut_ad(ret == 0);
order_resolved:
  *matched_fields = cur_field;
  return (ret);
}

#ifdef UNIV_COMPILE_TEST_FUNCS

#ifdef HAVE_UT_CHRONO_T

void test_cmp_data_data(ulint len) {
  int i;
  static byte zeros[64];

  if (len > sizeof zeros) {
    len = sizeof zeros;
  }

  ut_chrono_t ch(__func__);

  for (i = 1000000; i > 0; i--) {
    i += cmp_data(DATA_INT, 0, zeros, len, zeros, len);
  }
}

#endif /* HAVE_UT_CHRONO_T */

#endif /* UNIV_COMPILE_TEST_FUNCS */
