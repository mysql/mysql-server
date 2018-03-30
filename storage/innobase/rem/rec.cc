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

/** @file rem/rec.cc
 Record manager

 Created 5/30/1994 Heikki Tuuri
 *************************************************************************/

/** NOTE: The functions in this file should only use functions from
other files in library. The code in this file is used to make a library for
external tools. */

#include <stddef.h>

#include "dict0dict.h"
#include "mem0mem.h"
#include "rem/rec.h"

/** The following function determines the offsets to each field in the
 record.	 The offsets are written to a previously allocated array of
 ulint, where rec_offs_n_fields(offsets) has been initialized to the
 number of fields in the record.	 The rest of the array will be
 initialized by this function.  rec_offs_base(offsets)[0] will be set
 to the extra size (if REC_OFFS_COMPACT is set, the record is in the
 new format; if REC_OFFS_EXTERNAL is set, the record contains externally
 stored columns), and rec_offs_base(offsets)[1..n_fields] will be set to
 offsets past the end of fields 0..n_fields, or to the beginning of
 fields 1..n_fields+1.  When the high-order bit of the offset at [i+1]
 is set (REC_OFFS_SQL_NULL), the field i is NULL.  When the second
 high-order bit of the offset at [i+1] is set (REC_OFFS_EXTERNAL), the
 field i is being stored externally. */
void rec_init_offsets(const rec_t *rec,          /*!< in: physical record */
                      const dict_index_t *index, /*!< in: record descriptor */
                      ulint *offsets)            /*!< in/out: array of offsets;
                                                 in: n=rec_offs_n_fields(offsets) */
{
  ulint i = 0;
  ulint offs;

  rec_offs_make_valid(rec, index, offsets);

  if (dict_table_is_comp(index->table)) {
    const byte *nulls;
    const byte *lens;
    dict_field_t *field;
    ulint null_mask;
    ulint status = rec_get_status(rec);
    ulint n_node_ptr_field = ULINT_UNDEFINED;

    switch (UNIV_EXPECT(status, REC_STATUS_ORDINARY)) {
      case REC_STATUS_INFIMUM:
      case REC_STATUS_SUPREMUM:
        /* the field is 8 bytes long */
        rec_offs_base(offsets)[0] = REC_N_NEW_EXTRA_BYTES | REC_OFFS_COMPACT;
        rec_offs_base(offsets)[1] = 8;
        return;
      case REC_STATUS_NODE_PTR:
        n_node_ptr_field = dict_index_get_n_unique_in_tree_nonleaf(index);
        break;
      case REC_STATUS_ORDINARY:
        rec_init_offsets_comp_ordinary(rec, false, index, offsets);
        return;
    }

    ut_ad(!rec_get_instant_flag_new(rec));

    nulls = rec - (REC_N_NEW_EXTRA_BYTES + 1);
    lens = nulls - UT_BITS_IN_BYTES(index->n_instant_nullable);
    offs = 0;
    null_mask = 1;

    /* read the lengths of fields 0..n */
    do {
      ulint len;
      if (UNIV_UNLIKELY(i == n_node_ptr_field)) {
        len = offs += REC_NODE_PTR_SIZE;
        goto resolved;
      }

      field = index->get_field(i);
      if (!(field->col->prtype & DATA_NOT_NULL)) {
        /* nullable field => read the null flag */

        if (UNIV_UNLIKELY(!(byte)null_mask)) {
          nulls--;
          null_mask = 1;
        }

        if (*nulls & null_mask) {
          null_mask <<= 1;
          /* No length is stored for NULL fields.
          We do not advance offs, and we set
          the length to zero and enable the
          SQL NULL flag in offsets[]. */
          len = offs | REC_OFFS_SQL_NULL;
          goto resolved;
        }
        null_mask <<= 1;
      }

      if (UNIV_UNLIKELY(!field->fixed_len)) {
        const dict_col_t *col = field->col;
        /* DATA_POINT should always be a fixed
        length column. */
        ut_ad(col->mtype != DATA_POINT);
        /* Variable-length field: read the length */
        len = *lens--;
        /* If the maximum length of the field
        is up to 255 bytes, the actual length
        is always stored in one byte. If the
        maximum length is more than 255 bytes,
        the actual length is stored in one
        byte for 0..127.  The length will be
        encoded in two bytes when it is 128 or
        more, or when the field is stored
        externally. */
        if (DATA_BIG_COL(col)) {
          if (len & 0x80) {
            /* 1exxxxxxx xxxxxxxx */

            len <<= 8;
            len |= *lens--;

            /* B-tree node pointers
            must not contain externally
            stored columns.  Thus
            the "e" flag must be 0. */
            ut_a(!(len & 0x4000));
            offs += len & 0x3fff;
            len = offs;

            goto resolved;
          }
        }

        len = offs += len;
      } else {
        len = offs += field->fixed_len;
      }
    resolved:
      rec_offs_base(offsets)[i + 1] = len;
    } while (++i < rec_offs_n_fields(offsets));

    *rec_offs_base(offsets) = (rec - (lens + 1)) | REC_OFFS_COMPACT;
  } else {
    /* Old-style record: determine extra size and end offsets */
    offs = REC_N_OLD_EXTRA_BYTES;
    if (rec_get_1byte_offs_flag(rec)) {
      offs += rec_get_n_fields_old_raw(rec);
      *rec_offs_base(offsets) = offs;
      /* Determine offsets to fields */
      do {
        if (index->has_instant_cols() && i >= rec_get_n_fields_old_raw(rec)) {
          offs &= ~REC_OFFS_SQL_NULL;
          offs = rec_get_instant_offset(index, i, offs);
        } else {
          offs = rec_1_get_field_end_info(rec, i);
        }

        if (offs & REC_1BYTE_SQL_NULL_MASK) {
          offs &= ~REC_1BYTE_SQL_NULL_MASK;
          offs |= REC_OFFS_SQL_NULL;
        }

        ut_ad(i < rec_get_n_fields_old_raw(rec) || (offs & REC_OFFS_SQL_NULL) ||
              (offs & REC_OFFS_DEFAULT));
        rec_offs_base(offsets)[1 + i] = offs;
      } while (++i < rec_offs_n_fields(offsets));
    } else {
      offs += 2 * rec_get_n_fields_old_raw(rec);
      *rec_offs_base(offsets) = offs;
      /* Determine offsets to fields */
      do {
        if (index->has_instant_cols() && i >= rec_get_n_fields_old_raw(rec)) {
          offs &= ~(REC_OFFS_SQL_NULL | REC_OFFS_EXTERNAL);
          offs = rec_get_instant_offset(index, i, offs);
        } else {
          offs = rec_2_get_field_end_info(rec, i);
        }

        if (offs & REC_2BYTE_SQL_NULL_MASK) {
          offs &= ~REC_2BYTE_SQL_NULL_MASK;
          offs |= REC_OFFS_SQL_NULL;
        }
        if (offs & REC_2BYTE_EXTERN_MASK) {
          offs &= ~REC_2BYTE_EXTERN_MASK;
          offs |= REC_OFFS_EXTERNAL;
          *rec_offs_base(offsets) |= REC_OFFS_EXTERNAL;
        }

        ut_ad(i < rec_get_n_fields_old_raw(rec) || (offs & REC_OFFS_SQL_NULL) ||
              (offs & REC_OFFS_DEFAULT));
        rec_offs_base(offsets)[1 + i] = offs;
      } while (++i < rec_offs_n_fields(offsets));
    }
  }
}

/** The following function determines the offsets to each field
 in the record.	It can reuse a previously returned array.
 Note that after instant ADD COLUMN, if this is a record
 from clustered index, fields in the record may be less than
 the fields defined in the clustered index. So the offsets
 size is allocated according to the clustered index fields.
 @return the new offsets */
ulint *rec_get_offsets_func(
    const rec_t *rec,          /*!< in: physical record */
    const dict_index_t *index, /*!< in: record descriptor */
    ulint *offsets,            /*!< in/out: array consisting of
                               offsets[0] allocated elements,
                               or an array from rec_get_offsets(),
                               or NULL */
    ulint n_fields,            /*!< in: maximum number of
                              initialized fields
                               (ULINT_UNDEFINED if all fields) */
#ifdef UNIV_DEBUG
    const char *file,  /*!< in: file name where called */
    ulint line,        /*!< in: line number where called */
#endif                 /* UNIV_DEBUG */
    mem_heap_t **heap) /*!< in/out: memory heap */
{
  ulint n;
  ulint size;

  ut_ad(rec);
  ut_ad(index);
  ut_ad(heap);

  if (dict_table_is_comp(index->table)) {
    switch (UNIV_EXPECT(rec_get_status(rec), REC_STATUS_ORDINARY)) {
      case REC_STATUS_ORDINARY:
        n = dict_index_get_n_fields(index);
        break;
      case REC_STATUS_NODE_PTR:
        /* Node pointer records consist of the
        uniquely identifying fields of the record
        followed by a child page number field. */
        n = dict_index_get_n_unique_in_tree_nonleaf(index) + 1;
        break;
      case REC_STATUS_INFIMUM:
      case REC_STATUS_SUPREMUM:
        /* infimum or supremum record */
        n = 1;
        break;
      default:
        ut_error;
        return (NULL);
    }
  } else {
    n = rec_get_n_fields_old(rec, index);
  }

  if (UNIV_UNLIKELY(n_fields < n)) {
    n = n_fields;
  }

  /* The offsets header consists of the allocation size at
  offsets[0] and the REC_OFFS_HEADER_SIZE bytes. */
  size = n + (1 + REC_OFFS_HEADER_SIZE);

  if (UNIV_UNLIKELY(!offsets) ||
      UNIV_UNLIKELY(rec_offs_get_n_alloc(offsets) < size)) {
    if (UNIV_UNLIKELY(!*heap)) {
      *heap = mem_heap_create_at(size * sizeof(ulint), file, line);
    }
    offsets = static_cast<ulint *>(mem_heap_alloc(*heap, size * sizeof(ulint)));

    rec_offs_set_n_alloc(offsets, size);
  }

  rec_offs_set_n_fields(offsets, n);
  rec_init_offsets(rec, index, offsets);
  return (offsets);
}

/** The following function determines the offsets to each field
 in the record.  It can reuse a previously allocated array. */
void rec_get_offsets_reverse(
    const byte *extra,         /*!< in: the extra bytes of a
                               compact record in reverse order,
                               excluding the fixed-size
                               REC_N_NEW_EXTRA_BYTES */
    const dict_index_t *index, /*!< in: record descriptor */
    ulint node_ptr,            /*!< in: nonzero=node pointer,
                              0=leaf node */
    ulint *offsets)            /*!< in/out: array consisting of
                               offsets[0] allocated elements */
{
  ulint n;
  ulint i;
  ulint offs;
  ulint any_ext;
  const byte *nulls;
  const byte *lens;
  dict_field_t *field;
  ulint null_mask;
  ulint n_node_ptr_field;

  ut_ad(extra);
  ut_ad(index);
  ut_ad(offsets);
  ut_ad(dict_table_is_comp(index->table));

  if (UNIV_UNLIKELY(node_ptr)) {
    n_node_ptr_field = dict_index_get_n_unique_in_tree_nonleaf(index);
    n = n_node_ptr_field + 1;
  } else {
    n_node_ptr_field = ULINT_UNDEFINED;
    n = dict_index_get_n_fields(index);
  }

  ut_a(rec_offs_get_n_alloc(offsets) >= n + (1 + REC_OFFS_HEADER_SIZE));
  rec_offs_set_n_fields(offsets, n);

  nulls = extra;
  lens = nulls + UT_BITS_IN_BYTES(index->n_nullable);
  i = offs = 0;
  null_mask = 1;
  any_ext = 0;

  /* read the lengths of fields 0..n */
  do {
    ulint len;
    if (UNIV_UNLIKELY(i == n_node_ptr_field)) {
      len = offs += REC_NODE_PTR_SIZE;
      goto resolved;
    }

    field = index->get_field(i);
    if (!(field->col->prtype & DATA_NOT_NULL)) {
      /* nullable field => read the null flag */

      if (UNIV_UNLIKELY(!(byte)null_mask)) {
        nulls++;
        null_mask = 1;
      }

      if (*nulls & null_mask) {
        null_mask <<= 1;
        /* No length is stored for NULL fields.
        We do not advance offs, and we set
        the length to zero and enable the
        SQL NULL flag in offsets[]. */
        len = offs | REC_OFFS_SQL_NULL;
        goto resolved;
      }
      null_mask <<= 1;
    }

    if (UNIV_UNLIKELY(!field->fixed_len)) {
      /* Variable-length field: read the length */
      const dict_col_t *col = field->col;
      len = *lens++;
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
          len <<= 8;
          len |= *lens++;

          offs += len & 0x3fff;
          if (UNIV_UNLIKELY(len & 0x4000)) {
            any_ext = REC_OFFS_EXTERNAL;
            len = offs | REC_OFFS_EXTERNAL;
          } else {
            len = offs;
          }

          goto resolved;
        }
      }

      len = offs += len;
    } else {
      len = offs += field->fixed_len;
    }
  resolved:
    rec_offs_base(offsets)[i + 1] = len;
  } while (++i < rec_offs_n_fields(offsets));

  ut_ad(lens >= extra);
  *rec_offs_base(offsets) =
      (lens - extra + REC_N_NEW_EXTRA_BYTES) | REC_OFFS_COMPACT | any_ext;
}
