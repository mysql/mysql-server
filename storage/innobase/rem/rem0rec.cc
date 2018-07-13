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

/** @file rem/rem0rec.cc
 Record manager

 Created 5/30/1994 Heikki Tuuri
 *************************************************************************/

#include "rem0rec.h"

#include <sys/types.h>
#ifndef UNIV_HOTBACKUP

#include "fts0fts.h"
#endif /* !UNIV_HOTBACKUP */
#include "gis0geo.h"
#include "mach0data.h"
#include "mtr0log.h"
#include "mtr0mtr.h"
#include "page0page.h"
#include "trx0sys.h"

#include "my_dbug.h"

/*			PHYSICAL RECORD (OLD STYLE)
                        ===========================

The physical record, which is the data type of all the records
found in index pages of the database, has the following format
(lower addresses and more significant bits inside a byte are below
represented on a higher text line):

| offset of the end of the last field of data, the most significant
  bit is set to 1 if and only if the field is SQL-null,
  if the offset is 2-byte, then the second most significant
  bit is set to 1 if the field is stored on another page:
  mostly this will occur in the case of big BLOB fields |
...
| offset of the end of the first field of data + the SQL-null bit |
| 4 bits used to delete mark a record, and mark a predefined
  minimum record in alphabetical order |
| 4 bits giving the number of records owned by this record
  (this term is explained in page0page.h) |
| 13 bits giving the order number of this record in the
  heap of the index page |
| 10 bits giving the number of fields in this record |
| 1 bit which is set to 1 if the offsets above are given in
  one byte format, 0 if in two byte format |
| two bytes giving an absolute pointer to the next record in the page |
ORIGIN of the record
| first field of data |
...
| last field of data |

The origin of the record is the start address of the first field
of data. The offsets are given relative to the origin.
The offsets of the data fields are stored in an inverted
order because then the offset of the first fields are near the
origin, giving maybe a better processor cache hit rate in searches.

The offsets of the data fields are given as one-byte
(if there are less than 127 bytes of data in the record)
or two-byte unsigned integers. The most significant bit
is not part of the offset, instead it indicates the SQL-null
if the bit is set to 1. */

/*			PHYSICAL RECORD (NEW STYLE)
                        ===========================

The physical record, which is the data type of all the records
found in index pages of the database, has the following format
(lower addresses and more significant bits inside a byte are below
represented on a higher text line):

| length of the last non-null variable-length field of data:
  if the maximum length is 255, one byte; otherwise,
  0xxxxxxx (one byte, length=0..127), or 1exxxxxxxxxxxxxx (two bytes,
  length=128..16383, extern storage flag) |
...
| length of first variable-length field of data |
| SQL-null flags (1 bit per nullable field), padded to full bytes |
| 1 or 2 bytes to indicate number of fields in the record if the table
  where the record resides has undergone an instant ADD COLUMN
  before this record gets inserted; If no instant ADD COLUMN ever
  happened, here should be no byte; So parsing this optional number
  requires the index or table information |
| 4 bits used to delete mark a record, and mark a predefined
  minimum record in alphabetical order |
| 4 bits giving the number of records owned by this record
  (this term is explained in page0page.h) |
| 13 bits giving the order number of this record in the
  heap of the index page |
| 3 bits record type: 000=conventional, 001=node pointer (inside B-tree),
  010=infimum, 011=supremum, 1xx=reserved |
| two bytes giving a relative pointer to the next record in the page |
ORIGIN of the record
| first field of data |
...
| last field of data |

The origin of the record is the start address of the first field
of data. The offsets are given relative to the origin.
The offsets of the data fields are stored in an inverted
order because then the offset of the first fields are near the
origin, giving maybe a better processor cache hit rate in searches.

The offsets of the data fields are given as one-byte
(if there are less than 127 bytes of data in the record)
or two-byte unsigned integers. The most significant bit
is not part of the offset, instead it indicates the SQL-null
if the bit is set to 1. */

/* CANONICAL COORDINATES. A record can be seen as a single
string of 'characters' in the following way: catenate the bytes
in each field, in the order of fields. An SQL-null field
is taken to be an empty sequence of bytes. Then after
the position of each field insert in the string
the 'character' <FIELD-END>, except that after an SQL-null field
insert <NULL-FIELD-END>. Now the ordinal position of each
byte in this canonical string is its canonical coordinate.
So, for the record ("AA", SQL-NULL, "BB", ""), the canonical
string is "AA<FIELD_END><NULL-FIELD-END>BB<FIELD-END><FIELD-END>".
We identify prefixes (= initial segments) of a record
with prefixes of the canonical string. The canonical
length of the prefix is the length of the corresponding
prefix of the canonical string. The canonical length of
a record is the length of its canonical string.

For example, the maximal common prefix of records
("AA", SQL-NULL, "BB", "C") and ("AA", SQL-NULL, "B", "C")
is "AA<FIELD-END><NULL-FIELD-END>B", and its canonical
length is 5.

A complete-field prefix of a record is a prefix which ends at the
end of some field (containing also <FIELD-END>).
A record is a complete-field prefix of another record, if
the corresponding canonical strings have the same property. */

/** Validates the consistency of an old-style physical record.
 @return true if ok */
static ibool rec_validate_old(const rec_t *rec); /*!< in: physical record */

/** Determine how many of the first n columns in a compact
 physical record are stored externally.
 @return number of externally stored columns */
ulint rec_get_n_extern_new(
    const rec_t *rec,          /*!< in: compact physical record */
    const dict_index_t *index, /*!< in: record descriptor */
    ulint n)                   /*!< in: number of columns to scan */
{
  const byte *nulls;
  const byte *lens;
  ulint null_mask;
  ulint n_extern;
  ulint i;

  ut_ad(dict_table_is_comp(index->table));
  ut_ad(rec_get_status(rec) == REC_STATUS_ORDINARY);
  ut_ad(n == ULINT_UNDEFINED || n <= dict_index_get_n_fields(index));

  if (n == ULINT_UNDEFINED) {
    n = dict_index_get_n_fields(index);
  }

  nulls = rec - (REC_N_NEW_EXTRA_BYTES + 1);
  lens = nulls - UT_BITS_IN_BYTES(index->n_nullable);
  null_mask = 1;
  n_extern = 0;
  i = 0;

  /* read the lengths of fields 0..n */
  do {
    const dict_field_t *field = index->get_field(i);
    const dict_col_t *col = field->col;
    ulint len;

    if (!(col->prtype & DATA_NOT_NULL)) {
      /* nullable field => read the null flag */

      if (UNIV_UNLIKELY(!(byte)null_mask)) {
        nulls--;
        null_mask = 1;
      }

      if (*nulls & null_mask) {
        null_mask <<= 1;
        /* No length is stored for NULL fields. */
        continue;
      }
      null_mask <<= 1;
    }

    if (UNIV_UNLIKELY(!field->fixed_len)) {
      /* Variable-length field: read the length */
      len = *lens--;
      /* If the maximum length of the field is up
      to 255 bytes, the actual length is always
      stored in one byte. If the maximum length is
      more than 255 bytes, the actual length is
      stored in one byte for 0..127.  The length
      will be encoded in two bytes when it is 128 or
      more, or when the field is stored externally. */
      if (DATA_BIG_COL(col)) {
        if (len & 0x80) {
          /* 1exxxxxxx xxxxxxxx */
          if (len & 0x40) {
            n_extern++;
          }
          lens--;
        }
      }
    }
  } while (++i < n);

  return (n_extern);
}

/** The following function is used to get the offset to the nth
 data field in an old-style record.
 @return offset to the field */
ulint rec_get_nth_field_offs_old(const rec_t *rec, /*!< in: record */
                                 ulint n,    /*!< in: index of the field */
                                 ulint *len) /*!< out: length of the field;
                                             UNIV_SQL_NULL if SQL null */
{
  ulint os;
  ulint next_os;

  ut_ad(len);
  ut_a(rec);
  ut_a(n < rec_get_n_fields_old_raw(rec));

  if (rec_get_1byte_offs_flag(rec)) {
    os = rec_1_get_field_start_offs(rec, n);

    next_os = rec_1_get_field_end_info(rec, n);

    if (next_os & REC_1BYTE_SQL_NULL_MASK) {
      *len = UNIV_SQL_NULL;

      return (os);
    }

    next_os = next_os & ~REC_1BYTE_SQL_NULL_MASK;
  } else {
    os = rec_2_get_field_start_offs(rec, n);

    next_os = rec_2_get_field_end_info(rec, n);

    if (next_os & REC_2BYTE_SQL_NULL_MASK) {
      *len = UNIV_SQL_NULL;

      return (os);
    }

    next_os = next_os & ~(REC_2BYTE_SQL_NULL_MASK | REC_2BYTE_EXTERN_MASK);
  }

  *len = next_os - os;

  ut_ad(*len < UNIV_PAGE_SIZE);

  return (os);
}

/** Determines the size of a data tuple prefix in ROW_FORMAT=COMPACT.
 @return total size */
UNIV_INLINE MY_ATTRIBUTE((warn_unused_result)) ulint
    rec_get_converted_size_comp_prefix_low(
        const dict_index_t *index, /*!< in: record descriptor;
                                   dict_table_is_comp() is
                                   assumed to hold, even if
                                   it does not */
        const dfield_t *fields,    /*!< in: array of data fields */
        ulint n_fields,            /*!< in: number of data fields */
        const dtuple_t *v_entry,   /*!< in: dtuple contains virtual column
                                   data */
        ulint *extra,              /*!< out: extra size */
        ulint *status,             /*!< in: status bits of the record,
                                   can be nullptr if unnecessary */
        bool temp)                 /*!< in: whether this is a
                                   temporary file record */
{
  ulint extra_size = 0;
  ulint data_size;
  ulint i;
  ulint n_null = 0;
  ulint n_v_fields;
  ut_ad(n_fields <= dict_index_get_n_fields(index));
  ut_ad(!temp || extra);

  /* At the time being, only temp file record could possible
  store virtual columns */
  ut_ad(!v_entry || (index->is_clustered() && temp));
  n_v_fields = v_entry ? dtuple_get_n_v_fields(v_entry) : 0;

  if (n_fields > 0) {
    n_null = index->has_instant_cols()
                 ? index->get_n_nullable_before(static_cast<uint32_t>(n_fields))
                 : index->n_nullable;
  }

  if (index->has_instant_cols() && status != nullptr) {
    switch (UNIV_EXPECT(*status, REC_STATUS_ORDINARY)) {
      case REC_STATUS_ORDINARY:
        ut_ad(!temp && n_fields > 0);
        extra_size += rec_get_n_fields_length(n_fields);
        break;
      case REC_STATUS_NODE_PTR:
        ut_ad(!temp && n_fields > 0);
        n_null = index->n_instant_nullable;
        break;
      case REC_STATUS_INFIMUM:
      case REC_STATUS_SUPREMUM:
        break;
    }
  }

  extra_size += temp ? UT_BITS_IN_BYTES(n_null)
                     : REC_N_NEW_EXTRA_BYTES + UT_BITS_IN_BYTES(n_null);
  data_size = 0;

  if (temp && dict_table_is_comp(index->table)) {
    /* No need to do adjust fixed_len=0. We only need to
    adjust it for ROW_FORMAT=REDUNDANT. */
    temp = false;
  }

  /* read the lengths of fields 0..n */
  for (i = 0; i < n_fields; i++) {
    const dict_field_t *field;
    ulint len;
    ulint fixed_len;
    const dict_col_t *col;

    field = index->get_field(i);
    len = dfield_get_len(&fields[i]);
    col = field->col;

#ifdef UNIV_DEBUG
    dtype_t *type;

    type = dfield_get_type(&fields[i]);
    if (dict_index_is_spatial(index)) {
      if (DATA_GEOMETRY_MTYPE(col->mtype) && i == 0) {
        ut_ad(type->prtype & DATA_GIS_MBR);
      } else {
        ut_ad(type->mtype == DATA_SYS_CHILD || col->assert_equal(type));
      }
    } else {
      ut_ad(col->assert_equal(type));
    }
#endif

    /* All NULLable fields must be included in the n_null count. */
    ut_ad((col->prtype & DATA_NOT_NULL) || n_null--);

    if (dfield_is_null(&fields[i])) {
      /* No length is stored for NULL fields. */
      ut_ad(!(col->prtype & DATA_NOT_NULL));
      continue;
    }

    ut_ad(len <= col->len || DATA_LARGE_MTYPE(col->mtype) ||
          (DATA_POINT_MTYPE(col->mtype) && len == DATA_MBR_LEN) ||
          (col->len == 0 && col->mtype == DATA_VARCHAR));

    fixed_len = field->fixed_len;
    if (temp && fixed_len && !col->get_fixed_size(temp)) {
      fixed_len = 0;
    }
    /* If the maximum length of a variable-length field
    is up to 255 bytes, the actual length is always stored
    in one byte. If the maximum length is more than 255
    bytes, the actual length is stored in one byte for
    0..127.  The length will be encoded in two bytes when
    it is 128 or more, or when the field is stored externally. */

    if (fixed_len) {
#ifdef UNIV_DEBUG
      ulint mbminlen = DATA_MBMINLEN(col->mbminmaxlen);
      ulint mbmaxlen = DATA_MBMAXLEN(col->mbminmaxlen);

      ut_ad(len <= fixed_len);

      if (dict_index_is_spatial(index)) {
        ut_ad(type->mtype == DATA_SYS_CHILD || !mbmaxlen ||
              len >= mbminlen * (fixed_len / mbmaxlen));
      } else {
        ut_ad(type->mtype != DATA_SYS_CHILD);
        ut_ad(!mbmaxlen || len >= mbminlen * (fixed_len / mbmaxlen));
      }

      /* dict_index_add_col() should guarantee this */
      ut_ad(!field->prefix_len || fixed_len == field->prefix_len);
#endif /* UNIV_DEBUG */
    } else if (dfield_is_ext(&fields[i])) {
      ut_ad(DATA_BIG_COL(col));
      extra_size += 2;
    } else if (len < 128 || !DATA_BIG_COL(col)) {
      extra_size++;
    } else {
      /* For variable-length columns, we look up the
      maximum length from the column itself.  If this
      is a prefix index column shorter than 256 bytes,
      this will waste one byte. */
      extra_size += 2;
    }
    data_size += len;
  }

  if (extra) {
    *extra = extra_size;
  }

  /* Log virtual columns */
  if (n_v_fields != 0) {
    /* length marker */
    data_size += 2;

    for (i = 0; i < n_v_fields; i++) {
      dfield_t *vfield;
      ulint flen;

      const dict_v_col_t *col = dict_table_get_nth_v_col(index->table, i);

      /* Only those indexed needs to be logged */
      if (col->m_col.ord_part || !dict_table_is_comp(index->table)) {
        data_size += mach_get_compressed_size(i + REC_MAX_N_FIELDS);
        vfield = dtuple_get_nth_v_field(v_entry, col->v_pos);

        flen = vfield->len;

        if (flen != UNIV_SQL_NULL) {
          flen = ut_min(flen, static_cast<ulint>(
                                  DICT_MAX_FIELD_LEN_BY_FORMAT(index->table)));
          data_size += flen;
        }

        data_size += mach_get_compressed_size(flen);
      }
    }
  }

  return (extra_size + data_size);
}

/** Determines the size of a data tuple prefix in ROW_FORMAT=COMPACT.
 @return total size */
ulint rec_get_converted_size_comp_prefix(
    const dict_index_t *index, /*!< in: record descriptor */
    const dfield_t *fields,    /*!< in: array of data fields */
    ulint n_fields,            /*!< in: number of data fields */
    ulint *extra)              /*!< out: extra size */
{
  ut_ad(dict_table_is_comp(index->table));
  return (rec_get_converted_size_comp_prefix_low(index, fields, n_fields, NULL,
                                                 extra, nullptr, false));
}

/** Determines the size of a data tuple in ROW_FORMAT=COMPACT.
 @return total size */
ulint rec_get_converted_size_comp(
    const dict_index_t *index, /*!< in: record descriptor;
                               dict_table_is_comp() is
                               assumed to hold, even if
                               it does not */
    ulint status,              /*!< in: status bits of the record */
    const dfield_t *fields,    /*!< in: array of data fields */
    ulint n_fields,            /*!< in: number of data fields */
    ulint *extra)              /*!< out: extra size */
{
  ulint size;
  ut_ad(n_fields > 0);

  switch (UNIV_EXPECT(status, REC_STATUS_ORDINARY)) {
    case REC_STATUS_ORDINARY:
      /* If this is a record for instant index, it could has
      less fields when it comes from update path */
      ut_ad(n_fields == dict_index_get_n_fields(index) ||
            index->has_instant_cols());
      size = 0;
      break;
    case REC_STATUS_NODE_PTR:
      n_fields--;
      ut_ad(n_fields == dict_index_get_n_unique_in_tree_nonleaf(index));
      ut_ad(dfield_get_len(&fields[n_fields]) == REC_NODE_PTR_SIZE);
      size = REC_NODE_PTR_SIZE; /* child page number */
      break;
    case REC_STATUS_INFIMUM:
    case REC_STATUS_SUPREMUM:
      /* infimum or supremum record, 8 data bytes */
      if (UNIV_LIKELY_NULL(extra)) {
        *extra = REC_N_NEW_EXTRA_BYTES;
      }
      return (REC_N_NEW_EXTRA_BYTES + 8);
    default:
      ut_error;
  }

  return (size + rec_get_converted_size_comp_prefix_low(
                     index, fields, n_fields, NULL, extra, &status, false));
}

/** Sets the value of the ith field SQL null bit of an old-style record. */
void rec_set_nth_field_null_bit(rec_t *rec, /*!< in: record */
                                ulint i,    /*!< in: ith field */
                                ibool val)  /*!< in: value to set */
{
  ulint info;

  if (rec_get_1byte_offs_flag(rec)) {
    info = rec_1_get_field_end_info(rec, i);

    if (val) {
      info = info | REC_1BYTE_SQL_NULL_MASK;
    } else {
      info = info & ~REC_1BYTE_SQL_NULL_MASK;
    }

    rec_1_set_field_end_info(rec, i, info);

    return;
  }

  info = rec_2_get_field_end_info(rec, i);

  if (val) {
    info = info | REC_2BYTE_SQL_NULL_MASK;
  } else {
    info = info & ~REC_2BYTE_SQL_NULL_MASK;
  }

  rec_2_set_field_end_info(rec, i, info);
}

/** Sets an old-style record field to SQL null.
 The physical size of the field is not changed. */
void rec_set_nth_field_sql_null(rec_t *rec, /*!< in: record */
                                ulint n)    /*!< in: index of the field */
{
  ulint offset;

  offset = rec_get_field_start_offs(rec, n);

  data_write_sql_null(rec + offset, rec_get_nth_field_size(rec, n));

  rec_set_nth_field_null_bit(rec, n, TRUE);
}

/** Builds an old-style physical record out of a data tuple and
 stores it beginning from the start of the given buffer.
 @return pointer to the origin of physical record */
static rec_t *rec_convert_dtuple_to_rec_old(
    byte *buf,              /*!< in: start address of the physical record */
    const dtuple_t *dtuple, /*!< in: data tuple */
    ulint n_ext)            /*!< in: number of externally stored columns */
{
  const dfield_t *field;
  ulint n_fields;
  ulint data_size;
  rec_t *rec;
  ulint end_offset;
  ulint ored_offset;
  ulint len;
  ulint i;

  ut_ad(buf && dtuple);
  ut_ad(dtuple_validate(dtuple));
  ut_ad(dtuple_check_typed(dtuple));

  n_fields = dtuple_get_n_fields(dtuple);
  data_size = dtuple_get_data_size(dtuple, 0);

  ut_ad(n_fields > 0);

  /* Calculate the offset of the origin in the physical record */

  rec = buf + rec_get_converted_extra_size(data_size, n_fields, n_ext);
#ifdef UNIV_DEBUG
  /* Suppress Valgrind warnings of ut_ad()
  in mach_write_to_1(), mach_write_to_2() et al. */
  memset(buf, 0xff, rec - buf + data_size);
#endif /* UNIV_DEBUG */
  /* Store the number of fields */
  rec_set_n_fields_old(rec, n_fields);

  /* Set the info bits of the record */
  rec_set_info_bits_old(rec, dtuple_get_info_bits(dtuple) & REC_INFO_BITS_MASK);

  /* Store the data and the offsets */

  end_offset = 0;

  if (!n_ext && data_size <= REC_1BYTE_OFFS_LIMIT) {
    rec_set_1byte_offs_flag(rec, TRUE);

    for (i = 0; i < n_fields; i++) {
      field = dtuple_get_nth_field(dtuple, i);

      if (dfield_is_null(field)) {
        len = dtype_get_sql_null_size(dfield_get_type(field), 0);
        data_write_sql_null(rec + end_offset, len);

        end_offset += len;
        ored_offset = end_offset | REC_1BYTE_SQL_NULL_MASK;
      } else {
        /* If the data is not SQL null, store it */
        len = dfield_get_len(field);

        memcpy(rec + end_offset, dfield_get_data(field), len);

        end_offset += len;
        ored_offset = end_offset;
      }

      rec_1_set_field_end_info(rec, i, ored_offset);
    }
  } else {
    rec_set_1byte_offs_flag(rec, FALSE);

    for (i = 0; i < n_fields; i++) {
      field = dtuple_get_nth_field(dtuple, i);

      if (dfield_is_null(field)) {
        len = dtype_get_sql_null_size(dfield_get_type(field), 0);
        data_write_sql_null(rec + end_offset, len);

        end_offset += len;
        ored_offset = end_offset | REC_2BYTE_SQL_NULL_MASK;
      } else {
        /* If the data is not SQL null, store it */
        len = dfield_get_len(field);

        memcpy(rec + end_offset, dfield_get_data(field), len);

        end_offset += len;
        ored_offset = end_offset;

        if (dfield_is_ext(field)) {
          ored_offset |= REC_2BYTE_EXTERN_MASK;
        }
      }

      rec_2_set_field_end_info(rec, i, ored_offset);
    }
  }

  return (rec);
}

/** Builds a ROW_FORMAT=COMPACT record out of a data tuple.
@param[in]	rec		origin of record
@param[in]	index		record descriptor
@param[in]	fields		array of data fields
@param[in]	n_fields	number of data fields
@param[in]	v_entry		dtuple contains virtual column data
@param[in]	status		status bits of the record
@param[in]	temp		whether to use the format for temporary
                                files in index creation
@return	true	if this record is an instant record on leaf page
@retval	false	if not an instant record */
UNIV_INLINE
bool rec_convert_dtuple_to_rec_comp(rec_t *rec, const dict_index_t *index,
                                    const dfield_t *fields, ulint n_fields,
                                    const dtuple_t *v_entry, ulint status,
                                    bool temp) {
  const dfield_t *field;
  const dtype_t *type;
  byte *end;
  byte *nulls;
  byte *lens = NULL;
  ulint len;
  ulint i;
  ulint n_node_ptr_field;
  ulint fixed_len;
  ulint null_mask = 1;
  ulint n_null = 0;
  ulint num_v = v_entry ? dtuple_get_n_v_fields(v_entry) : 0;
  bool instant = false;

  ut_ad(temp || dict_table_is_comp(index->table));

  if (n_fields != 0) {
    n_null = index->has_instant_cols()
                 ? index->get_n_nullable_before(static_cast<uint32_t>(n_fields))
                 : index->n_nullable;
  }

  if (temp) {
    ut_ad(status == REC_STATUS_ORDINARY);
    ut_ad(n_fields <= dict_index_get_n_fields(index));
    n_node_ptr_field = ULINT_UNDEFINED;
    nulls = rec - 1;
    if (dict_table_is_comp(index->table)) {
      /* No need to do adjust fixed_len=0. We only
      need to adjust it for ROW_FORMAT=REDUNDANT. */
      temp = false;
    }
  } else {
    ut_ad(v_entry == NULL);
    ut_ad(num_v == 0);
    nulls = rec - (REC_N_NEW_EXTRA_BYTES + 1);

    switch (UNIV_EXPECT(status, REC_STATUS_ORDINARY)) {
      case REC_STATUS_ORDINARY:
        ut_ad(n_fields <= dict_index_get_n_fields(index));
        n_node_ptr_field = ULINT_UNDEFINED;

        if (index->has_instant_cols()) {
          uint32_t n_fields_len;
          n_fields_len = rec_set_n_fields(rec, n_fields);
          nulls -= n_fields_len;
          instant = true;
        }
        break;
      case REC_STATUS_NODE_PTR:
        ut_ad(n_fields == dict_index_get_n_unique_in_tree_nonleaf(index) + 1);
        n_node_ptr_field = n_fields - 1;
        n_null = index->n_instant_nullable;
        break;
      case REC_STATUS_INFIMUM:
      case REC_STATUS_SUPREMUM:
        ut_ad(n_fields == 1);
        n_node_ptr_field = ULINT_UNDEFINED;
        break;
      default:
        ut_error;
        return (instant);
    }
  }

  end = rec;

  if (n_fields != 0) {
    lens = nulls - UT_BITS_IN_BYTES(n_null);
    /* clear the SQL-null flags */
    memset(lens + 1, 0, nulls - lens);
  }

  /* Store the data and the offsets */

  for (i = 0; i < n_fields; i++) {
    const dict_field_t *ifield;
    dict_col_t *col = NULL;

    field = &fields[i];

    type = dfield_get_type(field);
    len = dfield_get_len(field);

    if (UNIV_UNLIKELY(i == n_node_ptr_field)) {
      ut_ad(dtype_get_prtype(type) & DATA_NOT_NULL);
      ut_ad(len == REC_NODE_PTR_SIZE);
      memcpy(end, dfield_get_data(field), len);
      end += REC_NODE_PTR_SIZE;
      break;
    }

    if (!(dtype_get_prtype(type) & DATA_NOT_NULL)) {
      /* nullable field */
      ut_ad(n_null--);

      if (UNIV_UNLIKELY(!(byte)null_mask)) {
        nulls--;
        null_mask = 1;
      }

      ut_ad(*nulls < null_mask);

      /* set the null flag if necessary */
      if (dfield_is_null(field)) {
        *nulls |= null_mask;
        null_mask <<= 1;
        continue;
      }

      null_mask <<= 1;
    }
    /* only nullable fields can be null */
    ut_ad(!dfield_is_null(field));

    ifield = index->get_field(i);
    fixed_len = ifield->fixed_len;
    col = ifield->col;
    if (temp && fixed_len && !col->get_fixed_size(temp)) {
      fixed_len = 0;
    }

    /* If the maximum length of a variable-length field
    is up to 255 bytes, the actual length is always stored
    in one byte. If the maximum length is more than 255
    bytes, the actual length is stored in one byte for
    0..127.  The length will be encoded in two bytes when
    it is 128 or more, or when the field is stored externally. */
    if (fixed_len) {
#ifdef UNIV_DEBUG
      ulint mbminlen = DATA_MBMINLEN(col->mbminmaxlen);
      ulint mbmaxlen = DATA_MBMAXLEN(col->mbminmaxlen);

      ut_ad(len <= fixed_len);
      ut_ad(!mbmaxlen || len >= mbminlen * (fixed_len / mbmaxlen));
      ut_ad(!dfield_is_ext(field));
#endif /* UNIV_DEBUG */
    } else if (dfield_is_ext(field)) {
      ut_ad(DATA_BIG_COL(col));
      ut_ad(len <= REC_ANTELOPE_MAX_INDEX_COL_LEN + BTR_EXTERN_FIELD_REF_SIZE);
      *lens-- = (byte)(len >> 8) | 0xc0;
      *lens-- = (byte)len;
    } else {
      /* DATA_POINT would have a fixed_len */
      ut_ad(dtype_get_mtype(type) != DATA_POINT);
#ifndef UNIV_HOTBACKUP
      ut_ad(len <= dtype_get_len(type) ||
            DATA_LARGE_MTYPE(dtype_get_mtype(type)) ||
            !strcmp(index->name, FTS_INDEX_TABLE_IND_NAME));
#endif /* !UNIV_HOTBACKUP */
      if (len < 128 ||
          !DATA_BIG_LEN_MTYPE(dtype_get_len(type), dtype_get_mtype(type))) {
        *lens-- = (byte)len;
      } else {
        ut_ad(len < 16384);
        *lens-- = (byte)(len >> 8) | 0x80;
        *lens-- = (byte)len;
      }
    }
    if (len > 0) memcpy(end, dfield_get_data(field), len);
    end += len;
  }

  if (!num_v) {
    return (instant);
  }

  /* reserve 2 bytes for writing length */
  byte *ptr = end;
  ptr += 2;

  /* Now log information on indexed virtual columns */
  for (ulint col_no = 0; col_no < num_v; col_no++) {
    dfield_t *vfield;
    ulint flen;

    const dict_v_col_t *col = dict_table_get_nth_v_col(index->table, col_no);

    if (col->m_col.ord_part || !dict_table_is_comp(index->table)) {
      ulint pos = col_no;

      pos += REC_MAX_N_FIELDS;

      ptr += mach_write_compressed(ptr, pos);

      vfield = dtuple_get_nth_v_field(v_entry, col->v_pos);

      flen = vfield->len;

      if (flen != UNIV_SQL_NULL) {
        /* The virtual column can only be in sec
        index, and index key length is bound by
        DICT_MAX_FIELD_LEN_BY_FORMAT */
        flen = ut_min(flen, static_cast<ulint>(
                                DICT_MAX_FIELD_LEN_BY_FORMAT(index->table)));
      }

      ptr += mach_write_compressed(ptr, flen);

      if (flen != UNIV_SQL_NULL) {
        ut_memcpy(ptr, dfield_get_data(vfield), flen);
        ptr += flen;
      }
    }
  }

  mach_write_to_2(end, ptr - end);

  return (instant);
}

/** Builds a new-style physical record out of a data tuple and
 stores it beginning from the start of the given buffer.
 @return pointer to the origin of physical record */
static rec_t *rec_convert_dtuple_to_rec_new(
    byte *buf,                 /*!< in: start address of
                               the physical record */
    const dict_index_t *index, /*!< in: record descriptor */
    const dtuple_t *dtuple)    /*!< in: data tuple */
{
  ulint extra_size;
  ulint status;
  rec_t *rec;
  bool instant;

  status = dtuple_get_info_bits(dtuple) & REC_NEW_STATUS_MASK;
  rec_get_converted_size_comp(index, status, dtuple->fields, dtuple->n_fields,
                              &extra_size);
  rec = buf + extra_size;

  instant = rec_convert_dtuple_to_rec_comp(
      rec, index, dtuple->fields, dtuple->n_fields, NULL, status, false);

  /* Set the info bits of the record */
  rec_set_info_and_status_bits(rec, dtuple_get_info_bits(dtuple));

  if (instant) {
    ut_ad(index->has_instant_cols());
    rec_set_instant_flag_new(rec, true);
  } else {
    rec_set_instant_flag_new(rec, false);
  }

  return (rec);
}

/** Builds a physical record out of a data tuple and
 stores it beginning from the start of the given buffer.
 @return pointer to the origin of physical record */
rec_t *rec_convert_dtuple_to_rec(
    byte *buf,                 /*!< in: start address of the
                               physical record */
    const dict_index_t *index, /*!< in: record descriptor */
    const dtuple_t *dtuple,    /*!< in: data tuple */
    ulint n_ext)               /*!< in: number of
                               externally stored columns */
{
  rec_t *rec;

  ut_ad(buf != NULL);
  ut_ad(index != NULL);
  ut_ad(dtuple != NULL);
  ut_ad(dtuple_validate(dtuple));
  ut_ad(dtuple_check_typed(dtuple));

  if (dict_table_is_comp(index->table)) {
    rec = rec_convert_dtuple_to_rec_new(buf, index, dtuple);
  } else {
    rec = rec_convert_dtuple_to_rec_old(buf, dtuple, n_ext);
  }

#ifdef UNIV_DEBUG
  /* Can't check this if it's an index with instantly added columns,
  because if it comes from UPDATE, the fields of dtuple may be less than
  the on from index itself. */
  if (!index->has_instant_cols()) {
    mem_heap_t *heap = NULL;
    ulint offsets_[REC_OFFS_NORMAL_SIZE];
    const ulint *offsets;
    ulint i;
    rec_offs_init(offsets_);

    offsets = rec_get_offsets(rec, index, offsets_, ULINT_UNDEFINED, &heap);
    ut_ad(rec_validate(rec, offsets));
    ut_ad(dtuple_get_n_fields(dtuple) == rec_offs_n_fields(offsets));

    for (i = 0; i < rec_offs_n_fields(offsets); i++) {
      ut_ad(!dfield_is_ext(dtuple_get_nth_field(dtuple, i)) ==
            !rec_offs_nth_extern(offsets, i));
    }

    if (UNIV_LIKELY_NULL(heap)) {
      mem_heap_free(heap);
    }
  }
#endif /* UNIV_DEBUG */
  return (rec);
}

#ifndef UNIV_HOTBACKUP
/** Determines the size of a data tuple prefix in ROW_FORMAT=COMPACT.
 @return total size */
ulint rec_get_converted_size_temp(
    const dict_index_t *index, /*!< in: record descriptor */
    const dfield_t *fields,    /*!< in: array of data fields */
    ulint n_fields,            /*!< in: number of data fields */
    const dtuple_t *v_entry,   /*!< in: dtuple contains virtual column
                               data */
    ulint *extra)              /*!< out: extra size */
{
  return (rec_get_converted_size_comp_prefix_low(
      index, fields, n_fields, v_entry, extra, nullptr, true));
}

/** Determine the offset to each field in temporary file.
 @see rec_convert_dtuple_to_temp() */
void rec_init_offsets_temp(
    const rec_t *rec,          /*!< in: temporary file record */
    const dict_index_t *index, /*!< in: record descriptor */
    ulint *offsets)            /*!< in/out: array of offsets;
                               in: n=rec_offs_n_fields(offsets) */
{
  rec_init_offsets_comp_ordinary(rec, true, index, offsets);
}

/** Builds a temporary file record out of a data tuple.
 @see rec_init_offsets_temp() */
void rec_convert_dtuple_to_temp(
    rec_t *rec,                /*!< out: record */
    const dict_index_t *index, /*!< in: record descriptor */
    const dfield_t *fields,    /*!< in: array of data fields */
    ulint n_fields,            /*!< in: number of fields */
    const dtuple_t *v_entry)   /*!< in: dtuple contains
                               virtual column data */
{
  rec_convert_dtuple_to_rec_comp(rec, index, fields, n_fields, v_entry,
                                 REC_STATUS_ORDINARY, true);
}

/** Copies the first n fields of a physical record to a data tuple. The fields
 are copied to the memory heap. */
void rec_copy_prefix_to_dtuple(
    dtuple_t *tuple,           /*!< out: data tuple */
    const rec_t *rec,          /*!< in: physical record */
    const dict_index_t *index, /*!< in: record descriptor */
    ulint n_fields,            /*!< in: number of fields
                               to copy */
    mem_heap_t *heap)          /*!< in: memory heap */
{
  ulint i;
  ulint offsets_[REC_OFFS_NORMAL_SIZE];
  ulint *offsets = offsets_;
  rec_offs_init(offsets_);

  offsets = rec_get_offsets(rec, index, offsets, n_fields, &heap);

  ut_ad(rec_validate(rec, offsets));
  ut_ad(dtuple_check_typed(tuple));

  dtuple_set_info_bits(
      tuple, rec_get_info_bits(rec, dict_table_is_comp(index->table)));

  for (i = 0; i < n_fields; i++) {
    dfield_t *field;
    const byte *data;
    ulint len;

    field = dtuple_get_nth_field(tuple, i);
    data = rec_get_nth_field_instant(rec, offsets, i, index, &len);

    if (len != UNIV_SQL_NULL) {
      dfield_set_data(field, mem_heap_dup(heap, data, len), len);
      ut_ad(!rec_offs_nth_extern(offsets, i));
    } else {
      dfield_set_null(field);
    }
  }
}

/** Copies the first n fields of an old-style physical record
 to a new physical record in a buffer.
 @return own: copied record */
static rec_t *rec_copy_prefix_to_buf_old(
    const rec_t *rec, /*!< in: physical record */
    ulint n_fields,   /*!< in: number of fields to copy */
    ulint area_end,   /*!< in: end of the prefix data */
    byte **buf,       /*!< in/out: memory buffer for
                      the copied prefix, or NULL */
    ulint *buf_size)  /*!< in/out: buffer size */
{
  rec_t *copy_rec;
  ulint area_start;
  ulint prefix_len;

  if (rec_get_1byte_offs_flag(rec)) {
    area_start = REC_N_OLD_EXTRA_BYTES + n_fields;
  } else {
    area_start = REC_N_OLD_EXTRA_BYTES + 2 * n_fields;
  }

  prefix_len = area_start + area_end;

  if ((*buf == NULL) || (*buf_size < prefix_len)) {
    ut_free(*buf);
    *buf_size = prefix_len;
    *buf = static_cast<byte *>(ut_malloc_nokey(prefix_len));
  }

  ut_memcpy(*buf, rec - area_start, prefix_len);

  copy_rec = *buf + area_start;

  rec_set_n_fields_old(copy_rec, n_fields);

  return (copy_rec);
}

/** Copies the first n fields of a physical record to a new physical record in
 a buffer.
 @return own: copied record */
rec_t *rec_copy_prefix_to_buf(
    const rec_t *rec,          /*!< in: physical record */
    const dict_index_t *index, /*!< in: record descriptor */
    ulint n_fields,            /*!< in: number of fields
                               to copy */
    byte **buf,                /*!< in/out: memory buffer
                               for the copied prefix,
                               or NULL */
    ulint *buf_size)           /*!< in/out: buffer size */
{
  const byte *nulls;
  const byte *lens;
  uint16_t n_null;
  ulint i;
  ulint prefix_len;
  ulint null_mask;
  ulint status;
  bool is_rtr_node_ptr = false;

  UNIV_PREFETCH_RW(*buf);

  if (!dict_table_is_comp(index->table)) {
    ut_ad(rec_validate_old(rec));
    return (rec_copy_prefix_to_buf_old(
        rec, n_fields, rec_get_field_start_offs(rec, n_fields), buf, buf_size));
  }

  status = rec_get_status(rec);

  switch (status) {
    case REC_STATUS_ORDINARY:
      ut_ad(n_fields <= dict_index_get_n_fields(index));
      break;
    case REC_STATUS_NODE_PTR:
      /* For R-tree, we need to copy the child page number field. */
      if (dict_index_is_spatial(index)) {
        ut_ad(n_fields == DICT_INDEX_SPATIAL_NODEPTR_SIZE + 1);
        is_rtr_node_ptr = true;
      } else {
        /* it doesn't make sense to copy the child page number
        field */
        ut_ad(n_fields <= dict_index_get_n_unique_in_tree_nonleaf(index));
      }
      break;
    case REC_STATUS_INFIMUM:
    case REC_STATUS_SUPREMUM:
      /* infimum or supremum record: no sense to copy anything */
    default:
      ut_error;
      return (NULL);
  }

  ut_d(uint16_t non_default_fields =)
      rec_init_null_and_len_comp(rec, index, &nulls, &lens, &n_null);
  ut_ad(!rec_get_instant_flag_new(rec) || n_fields <= non_default_fields);

  UNIV_PREFETCH_R(lens);
  prefix_len = 0;
  null_mask = 1;

  /* read the lengths of fields 0..n */
  for (i = 0; i < n_fields; i++) {
    const dict_field_t *field;
    const dict_col_t *col;

    field = index->get_field(i);
    col = field->col;

    if (!(col->prtype & DATA_NOT_NULL)) {
      /* nullable field => read the null flag */
      if (UNIV_UNLIKELY(!(byte)null_mask)) {
        nulls--;
        null_mask = 1;
      }

      if (*nulls & null_mask) {
        null_mask <<= 1;
        continue;
      }

      null_mask <<= 1;
    }

    if (is_rtr_node_ptr && i == 1) {
      /* For rtree node ptr rec, we need to
      copy the page no field with 4 bytes len. */
      prefix_len += 4;
    } else if (field->fixed_len) {
      prefix_len += field->fixed_len;
    } else {
      ulint len = *lens--;
      /* If the maximum length of the column is up
      to 255 bytes, the actual length is always
      stored in one byte. If the maximum length is
      more than 255 bytes, the actual length is
      stored in one byte for 0..127.  The length
      will be encoded in two bytes when it is 128 or
      more, or when the column is stored externally. */
      if (DATA_BIG_COL(col)) {
        if (len & 0x80) {
          /* 1exxxxxx */
          len &= 0x3f;
          len <<= 8;
          len |= *lens--;
          UNIV_PREFETCH_R(lens);
        }
      }
      prefix_len += len;
    }
  }

  UNIV_PREFETCH_R(rec + prefix_len);

  prefix_len += rec - (lens + 1);

  if ((*buf == NULL) || (*buf_size < prefix_len)) {
    ut_free(*buf);
    *buf_size = prefix_len;
    *buf = static_cast<byte *>(ut_malloc_nokey(prefix_len));
  }

  memcpy(*buf, lens + 1, prefix_len);

  return (*buf + (rec - (lens + 1)));
}
#endif /* UNIV_HOTBACKUP */

/** Validates the consistency of an old-style physical record.
 @return true if ok */
static ibool rec_validate_old(const rec_t *rec) /*!< in: physical record */
{
  ulint len;
  ulint n_fields;
  ulint len_sum = 0;
  ulint i;

  ut_a(rec);
  n_fields = rec_get_n_fields_old_raw(rec);

  if ((n_fields == 0) || (n_fields > REC_MAX_N_FIELDS)) {
    ib::error(ER_IB_MSG_922) << "Record has " << n_fields << " fields";
    return (FALSE);
  }

  for (i = 0; i < n_fields; i++) {
    rec_get_nth_field_offs_old(rec, i, &len);

    if (!((len < UNIV_PAGE_SIZE) || (len == UNIV_SQL_NULL))) {
      ib::error(ER_IB_MSG_923) << "Record field " << i << " len " << len;
      return (FALSE);
    }

    if (len != UNIV_SQL_NULL) {
      len_sum += len;
    } else {
      len_sum += rec_get_nth_field_size(rec, i);
    }
  }

  if (len_sum != rec_get_data_size_old(rec)) {
    ib::error(ER_IB_MSG_924) << "Record len should be " << len_sum << ", len "
                             << rec_get_data_size_old(rec);
    return (FALSE);
  }

  return (TRUE);
}

/** Validates the consistency of a physical record.
 @return true if ok */
ibool rec_validate(
    const rec_t *rec,     /*!< in: physical record */
    const ulint *offsets) /*!< in: array returned by rec_get_offsets() */
{
  ulint len;
  uint16_t n_fields;
  ulint len_sum = 0;
  ulint i;
  uint16_t n_defaults = 0;

  ut_a(rec);
  n_fields = static_cast<uint16_t>(rec_offs_n_fields(offsets));

  if ((n_fields == 0) || (n_fields > REC_MAX_N_FIELDS)) {
    ib::error(ER_IB_MSG_925) << "Record has " << n_fields << " fields";
    return (FALSE);
  }

  for (i = 0; i < n_fields; i++) {
    rec_get_nth_field_offs(offsets, i, &len);

    switch (len) {
      case UNIV_SQL_ADD_COL_DEFAULT:
        ++n_defaults;
        break;
      case UNIV_SQL_NULL:
        if (!rec_offs_comp(offsets)) {
          /* If a default value is NULL, it will come
          here, however, this is not inlined, so
          don't count it */
          if (i < rec_get_n_fields_old_raw(rec)) {
            len_sum += rec_get_nth_field_size(rec, i);
          } else {
            ++n_defaults;
          }
        }
        break;
      default:
        ut_a(n_defaults == 0);
        if (len >= UNIV_PAGE_SIZE) {
          ib::error(ER_IB_MSG_926) << "Record field " << i << " len " << len;
          return (FALSE);
        }

        len_sum += len;
        break;
    }
  }

  ut_a(rec_offs_comp(offsets) ||
       n_fields <= rec_get_n_fields_old_raw(rec) + n_defaults);

  if (len_sum != rec_offs_data_size(offsets)) {
    ib::error(ER_IB_MSG_927) << "Record len should be " << len_sum << ", len "
                             << rec_offs_data_size(offsets);
    return (FALSE);
  }

  if (!rec_offs_comp(offsets)) {
    ut_a(rec_validate_old(rec));
  }

  return (TRUE);
}

/** Prints an old-style physical record. */
void rec_print_old(FILE *file,       /*!< in: file where to print */
                   const rec_t *rec) /*!< in: physical record */
{
  const byte *data;
  ulint len;
  ulint n;
  ulint i;

  ut_ad(rec);

  n = rec_get_n_fields_old_raw(rec);

  fprintf(file,
          "PHYSICAL RECORD: n_fields %lu;"
          " %u-byte offsets; info bits %lu\n",
          (ulong)n, rec_get_1byte_offs_flag(rec) ? 1 : 2,
          (ulong)rec_get_info_bits(rec, FALSE));

  for (i = 0; i < n; i++) {
    data = rec_get_nth_field_old(rec, i, &len);

    fprintf(file, " %lu:", (ulong)i);

    if (len != UNIV_SQL_NULL) {
      if (len <= 30) {
        ut_print_buf(file, data, len);
      } else {
        ut_print_buf(file, data, 30);

        fprintf(file, " (total %lu bytes)", (ulong)len);
      }
    } else {
      fprintf(file, " SQL NULL, size " ULINTPF " ",
              rec_get_nth_field_size(rec, i));
    }

    putc(';', file);
    putc('\n', file);
  }

  rec_validate_old(rec);
}

#ifndef UNIV_HOTBACKUP
/** Prints a physical record in ROW_FORMAT=COMPACT.  Ignores the
 record header. */
static void rec_print_comp(
    FILE *file,           /*!< in: file where to print */
    const rec_t *rec,     /*!< in: physical record */
    const ulint *offsets) /*!< in: array returned by rec_get_offsets() */
{
  ulint i;

  for (i = 0; i < rec_offs_n_fields(offsets); i++) {
    const byte *data = nullptr;
    ulint len;

    if (rec_offs_nth_default(offsets, i)) {
      len = UNIV_SQL_ADD_COL_DEFAULT;
    } else {
      data = rec_get_nth_field(rec, offsets, i, &len);
    }

    fprintf(file, " %lu:", (ulong)i);

    switch (len) {
      case UNIV_SQL_NULL:
        fputs(" SQL NULL", file);
        break;
      case UNIV_SQL_ADD_COL_DEFAULT:
        fputs(" SQL DEFAULT", file);
        break;
      default:
        if (len <= 30) {
          ut_print_buf(file, data, len);
        } else if (rec_offs_nth_extern(offsets, i)) {
          ut_print_buf(file, data, 30);
          fprintf(file, " (total %lu bytes, external)", (ulong)len);
          ut_print_buf(file, data + len - BTR_EXTERN_FIELD_REF_SIZE,
                       BTR_EXTERN_FIELD_REF_SIZE);
        } else {
          ut_print_buf(file, data, 30);

          fprintf(file, " (total %lu bytes)", (ulong)len);
        }
    }
    putc(';', file);
    putc('\n', file);
  }
}

/** Prints an old-style spatial index record. */
static void rec_print_mbr_old(FILE *file,       /*!< in: file where to print */
                              const rec_t *rec) /*!< in: physical record */
{
  const byte *data;
  ulint len;
  ulint n;
  ulint i;

  ut_ad(rec);

  n = rec_get_n_fields_old_raw(rec);

  fprintf(file,
          "PHYSICAL RECORD: n_fields %lu;"
          " %u-byte offsets; info bits %lu\n",
          (ulong)n, rec_get_1byte_offs_flag(rec) ? 1 : 2,
          (ulong)rec_get_info_bits(rec, FALSE));

  for (i = 0; i < n; i++) {
    data = rec_get_nth_field_old(rec, i, &len);

    fprintf(file, " %lu:", (ulong)i);

    if (len != UNIV_SQL_NULL) {
      if (i == 0) {
        fprintf(file, " MBR:");
        for (; len > 0; len -= sizeof(double)) {
          double d = mach_double_read(data);

          if (len != sizeof(double)) {
            fprintf(file, "%.2lf,", d);
          } else {
            fprintf(file, "%.2lf", d);
          }

          data += sizeof(double);
        }
      } else {
        if (len <= 30) {
          ut_print_buf(file, data, len);
        } else {
          ut_print_buf(file, data, 30);

          fprintf(file, " (total %lu bytes)", (ulong)len);
        }
      }
    } else {
      fprintf(file, " SQL NULL, size " ULINTPF " ",
              rec_get_nth_field_size(rec, i));
    }

    putc(';', file);
    putc('\n', file);
  }

  if (rec_get_deleted_flag(rec, false)) {
    fprintf(file, " Deleted");
  }

  if (rec_get_info_bits(rec, true) & REC_INFO_MIN_REC_FLAG) {
    fprintf(file, " First rec");
  }

  rec_validate_old(rec);
}

/** Prints a spatial index record. */
void rec_print_mbr_rec(
    FILE *file,           /*!< in: file where to print */
    const rec_t *rec,     /*!< in: physical record */
    const ulint *offsets) /*!< in: array returned by rec_get_offsets() */
{
  ut_ad(rec);
  ut_ad(offsets);
  ut_ad(rec_offs_validate(rec, NULL, offsets));

  if (!rec_offs_comp(offsets)) {
    rec_print_mbr_old(file, rec);
    return;
  }

  for (ulint i = 0; i < rec_offs_n_fields(offsets); i++) {
    const byte *data;
    ulint len;

    ut_ad(!rec_offs_nth_default(offsets, i));
    data = rec_get_nth_field(rec, offsets, i, &len);

    if (i == 0) {
      fprintf(file, " MBR:");
      for (; len > 0; len -= sizeof(double)) {
        double d = mach_double_read(data);

        if (len != sizeof(double)) {
          fprintf(file, "%.2lf,", d);
        } else {
          fprintf(file, "%.2lf", d);
        }

        data += sizeof(double);
      }
    } else {
      fprintf(file, " %lu:", (ulong)i);

      if (len != UNIV_SQL_NULL) {
        if (len <= 30) {
          ut_print_buf(file, data, len);
        } else {
          ut_print_buf(file, data, 30);

          fprintf(file, " (total %lu bytes)", (ulong)len);
        }
      } else {
        fputs(" SQL NULL", file);
      }
    }
    putc(';', file);
  }

  if (rec_get_info_bits(rec, true) & REC_INFO_DELETED_FLAG) {
    fprintf(file, " Deleted");
  }

  if (rec_get_info_bits(rec, true) & REC_INFO_MIN_REC_FLAG) {
    fprintf(file, " First rec");
  }

  rec_validate(rec, offsets);
}

/** Prints a physical record. */
/** Prints a physical record. */
void rec_print_new(
    FILE *file,           /*!< in: file where to print */
    const rec_t *rec,     /*!< in: physical record */
    const ulint *offsets) /*!< in: array returned by rec_get_offsets() */
{
  ut_ad(rec);
  ut_ad(offsets);
  ut_ad(rec_offs_validate(rec, NULL, offsets));

#ifdef UNIV_DEBUG
  if (rec_get_deleted_flag(rec, rec_offs_comp(offsets))) {
    DBUG_PRINT("info", ("deleted "));
  } else {
    DBUG_PRINT("info", ("not-deleted "));
  }
#endif /* UNIV_DEBUG */

  if (!rec_offs_comp(offsets)) {
    rec_print_old(file, rec);
    return;
  }

  fprintf(file,
          "PHYSICAL RECORD: n_fields %lu;"
          " compact format; info bits %lu\n",
          (ulong)rec_offs_n_fields(offsets),
          (ulong)rec_get_info_bits(rec, TRUE));

  rec_print_comp(file, rec, offsets);
  rec_validate(rec, offsets);
}

/** Prints a physical record. */
void rec_print(FILE *file,                /*!< in: file where to print */
               const rec_t *rec,          /*!< in: physical record */
               const dict_index_t *index) /*!< in: record descriptor */
{
  ut_ad(index);

  if (!dict_table_is_comp(index->table)) {
    rec_print_old(file, rec);
    return;
  } else {
    mem_heap_t *heap = NULL;
    ulint offsets_[REC_OFFS_NORMAL_SIZE];
    rec_offs_init(offsets_);

    rec_print_new(
        file, rec,
        rec_get_offsets(rec, index, offsets_, ULINT_UNDEFINED, &heap));
    if (UNIV_LIKELY_NULL(heap)) {
      mem_heap_free(heap);
    }
  }
}

/** Pretty-print a record.
@param[in,out]	o	output stream
@param[in]	rec	physical record
@param[in]	info	rec_get_info_bits(rec)
@param[in]	offsets	rec_get_offsets(rec) */
void rec_print(std::ostream &o, const rec_t *rec, ulint info,
               const ulint *offsets) {
  const ulint comp = rec_offs_comp(offsets);
  const ulint n = rec_offs_n_fields(offsets);

  ut_ad(rec_offs_validate(rec, NULL, offsets));

  o << (comp ? "COMPACT RECORD" : "RECORD") << "(info_bits=" << info << ", "
    << n << " fields): {";

  for (ulint i = 0; i < n; i++) {
    const byte *data;
    ulint len;

    if (i) {
      o << ',';
    }

    if (rec_offs_nth_default(offsets, i)) {
      o << "DEFAULT";
      continue;
    }

    data = rec_get_nth_field(rec, offsets, i, &len);

    if (len == UNIV_SQL_NULL) {
      o << "NULL";
      continue;
    }

    if (rec_offs_nth_extern(offsets, i)) {
      ulint local_len = len - BTR_EXTERN_FIELD_REF_SIZE;
      ut_ad(len >= BTR_EXTERN_FIELD_REF_SIZE);

      o << '[' << local_len << '+' << BTR_EXTERN_FIELD_REF_SIZE << ']';
      ut_print_buf(o, data, local_len);
      ut_print_buf_hex(o, data + local_len, BTR_EXTERN_FIELD_REF_SIZE);
    } else {
      o << '[' << len << ']';
      ut_print_buf(o, data, len);
    }
  }

  o << "}";
}

/** Display a record.
@param[in,out]	o	output stream
@param[in]	r	record to display
@return	the output stream */
std::ostream &operator<<(std::ostream &o, const rec_index_print &r) {
  mem_heap_t *heap = NULL;
  ulint *offsets =
      rec_get_offsets(r.m_rec, r.m_index, NULL, ULINT_UNDEFINED, &heap);
  rec_print(o, r.m_rec, rec_get_info_bits(r.m_rec, rec_offs_comp(offsets)),
            offsets);
  mem_heap_free(heap);
  return (o);
}

/** Display a record.
@param[in,out]	o	output stream
@param[in]	r	record to display
@return	the output stream */
std::ostream &operator<<(std::ostream &o, const rec_offsets_print &r) {
  rec_print(o, r.m_rec, rec_get_info_bits(r.m_rec, rec_offs_comp(r.m_offsets)),
            r.m_offsets);
  return (o);
}

/** Reads the DB_TRX_ID of a clustered index record.
 @return the value of DB_TRX_ID */
trx_id_t rec_get_trx_id(const rec_t *rec,          /*!< in: record */
                        const dict_index_t *index) /*!< in: clustered index */
{
  ulint trx_id_col = index->get_sys_col_pos(DATA_TRX_ID);
  const byte *trx_id;
  ulint len;
  mem_heap_t *heap = NULL;
  ulint offsets_[REC_OFFS_NORMAL_SIZE];
  ulint *offsets = offsets_;
  rec_offs_init(offsets_);

  ut_ad(index->is_clustered());
  ut_ad(trx_id_col > 0);
  ut_ad(trx_id_col != ULINT_UNDEFINED);

#ifdef UNIV_DEBUG
  const page_t *page = page_align(rec);
  if (fil_page_index_page_check(page)) {
    ut_ad(mach_read_from_8(page + PAGE_HEADER + PAGE_INDEX_ID) == index->id);
  }
#endif /* UNIV_DEBUG */

  offsets = rec_get_offsets(rec, index, offsets, trx_id_col + 1, &heap);

  trx_id = rec_get_nth_field(rec, offsets, trx_id_col, &len);

  ut_ad(len == DATA_TRX_ID_LEN);

  if (heap) {
    mem_heap_free(heap);
  }

  return (trx_read_trx_id(trx_id));
}
#endif /* !UNIV_HOTBACKUP */

/** Mark the nth field as externally stored.
@param[in]	offsets		array returned by rec_get_offsets()
@param[in]	n		nth field */
void rec_offs_make_nth_extern(ulint *offsets, const ulint n) {
  ut_ad(!rec_offs_nth_sql_null(offsets, n));
  rec_offs_base(offsets)[1 + n] |= REC_OFFS_EXTERNAL;
}
