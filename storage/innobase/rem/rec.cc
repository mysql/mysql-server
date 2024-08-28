/*****************************************************************************

Copyright (c) 1994, 2024, Oracle and/or its affiliates.

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
#include "rem0lrec.h"
#include "rem0rec.h"

/** Initialize offset for each field in a new style record.
@param[in]      rec     physical record
@param[in]      index   record descriptor
@param[in, out] offsets array of offsets */
static void rec_init_offsets_new(const rec_t *rec, const dict_index_t *index,
                                 ulint *offsets) {
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

  /* This is non-leaf record. */
  ut_ad(!rec_new_is_versioned(rec));

  const byte *nulls = rec - (REC_N_NEW_EXTRA_BYTES + 1);
  const size_t nullable_cols = index->get_nullable_before_instant_add_drop();

  const byte *lens = nulls - UT_BITS_IN_BYTES(nullable_cols);
  ulint offs = 0;
  ulint null_mask = 1;

  /* read the lengths of fields 0..n */
  ulint i = 0;
  dict_field_t *field = nullptr;
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
}

/** Initialize offsets for record in REDUNDNT format when each field offsets is
stored in 1 byte.
@param[in]      rec     physical record
@param[in]      index   record descriptor
@param[in,out]  offsets array of offsets
@param[in]      row_version  row version in record */
static void rec_init_offset_old_1byte(const rec_t *rec,
                                      const dict_index_t *index, ulint *offsets,
                                      row_version_t row_version) {
  ut_ad(is_valid_row_version(row_version));

  ulint offs = REC_N_OLD_EXTRA_BYTES;
  /* 1 byte for row version */
  offs += rec_old_is_versioned(rec) ? 1 : 0;
  offs += rec_get_n_fields_old_raw(rec);
  *rec_offs_base(offsets) = offs;

  /* Determine offsets to fields */
  ulint i = 0;
  uint32_t dropped_col_count = 0;
  do {
    if (index->has_instant_cols_or_row_versions()) {
      dict_field_t *dfield = index->get_physical_field(i);
      dict_col_t *col = dfield->col;

      if (rec_old_is_versioned(rec)) { /* rec is in V2 */
        ut_ad(index->has_row_versions() ||
              (index->table->is_upgraded_instant() && row_version == 0));

        /* Based on the row version in record and column information, see if
        this column is there in this record or not. */
        if (col->is_added_after(row_version)) {
          /* This columns is added after this row version. In this case no
          need to store the length. Instead store only if it is NULL or
          DEFAULT value. */

          offs &= ~REC_OFFS_SQL_NULL;
          offs = rec_get_instant_offset(index, i, offs);
        } else if (col->is_dropped_in_or_before(row_version)) {
          /* This columns is dropped before or on this row version so its data
          won't be there on row. So no need to store the length. Instead,
          store offs ORed with REC_OFFS_DROP to indicate the same. */
          offs &= ~REC_OFFS_SQL_NULL;
          offs = offs | REC_OFFS_DROP;
          dropped_col_count++;

          /* NOTE : Existing rows, which have data for this column, would still
          need to process this column, so store the correct length there.
          Though it will be skipped while fetching row. */
        } else {
          // offs = rec_1_get_field_end_info(rec, col->phy_pos);
          /* i is physical pos here */
          offs = rec_1_get_field_end_info_low(rec, i - dropped_col_count);
        }
      } else if (i >= rec_get_n_fields_old_raw(rec)) {
        /* This field is not present in rec */
        offs &= ~REC_OFFS_SQL_NULL;
        offs = rec_get_instant_offset(index, i, offs);
      } else {
        /* This field is present in rec */
        /* i is physical pos here */
        offs = rec_1_get_field_end_info_low(rec, i);
      }
    } else {
      /* i is physical pos here */
      offs = rec_1_get_field_end_info_low(rec, i);
    }

    if (offs & REC_1BYTE_SQL_NULL_MASK) {
      offs &= ~REC_1BYTE_SQL_NULL_MASK;
      offs |= REC_OFFS_SQL_NULL;
    }

    ut_ad(i - dropped_col_count < rec_get_n_fields_old_raw(rec) ||
          (offs & REC_OFFS_SQL_NULL) || (offs & REC_OFFS_DEFAULT) ||
          (offs & REC_OFFS_DROP));
    rec_offs_base(offsets)[1 + i] = offs;
  } while (++i < rec_offs_n_fields(offsets));
}

/** Initialize offsets for record in REDUNDNT format when each field offsets is
stored in 2 byte.
@param[in]      rec     physical record
@param[in]      index   record descriptor
@param[in, out] offsets array of offsets
@param[in]      row_version     row version in record */
static void rec_init_offset_old_2byte(const rec_t *rec,
                                      const dict_index_t *index, ulint *offsets,
                                      row_version_t row_version) {
  ut_ad(is_valid_row_version(row_version));

  ulint offs = REC_N_OLD_EXTRA_BYTES;
  /* 1 byte for row version */
  offs += rec_old_is_versioned(rec) ? 1 : 0;
  offs += 2 * rec_get_n_fields_old_raw(rec);
  *rec_offs_base(offsets) = offs;

  /* Determine offsets to fields */
  ulint i = 0;
  uint32_t dropped_col_count = 0;
  do {
    if (index->has_instant_cols_or_row_versions()) {
      dict_field_t *dfield = index->get_physical_field(i);
      dict_col_t *col = dfield->col;

      if (rec_old_is_versioned(rec)) { /* rec is in V2 */
        ut_ad(index->has_row_versions() ||
              (index->table->is_upgraded_instant() && row_version == 0));

        /* Based on the row version in record and column information, see if
        this column is there in this record or not. */
        if (col->is_added_after(row_version)) {
          /* This columns is added after this row version. In this case no
          need to store the length. Instead store only if it is NULL or
          DEFAULT value. */

          offs &= ~(REC_OFFS_SQL_NULL | REC_OFFS_EXTERNAL);
          offs = rec_get_instant_offset(index, i, offs);
        } else if (col->is_dropped_in_or_before(row_version)) {
          /* This columns is dropped before or on this row version so its data
          won't be there on row. So no need to store the length. Instead,
          store offs ORed with REC_OFFS_DROP to indicate the same. */
          offs &= ~(REC_OFFS_SQL_NULL | REC_OFFS_EXTERNAL);
          offs = offs | REC_OFFS_DROP;
          dropped_col_count++;

          /* NOTE : Existing rows, which have data for this column, would still
          need to process this column, so store the correct length there.
          Though it will be skipped while fetching row. */
        } else {
          // offs = rec_2_get_field_end_info(rec, col->phy_pos);
          /* i is physical pos here */
          offs = rec_2_get_field_end_info_low(rec, i - dropped_col_count);
        }
      } else if (i >= rec_get_n_fields_old_raw(rec)) {
        /* This field is not present in rec */
        offs &= ~(REC_OFFS_SQL_NULL | REC_OFFS_EXTERNAL);
        offs = rec_get_instant_offset(index, i, offs);
      } else {
        /* This field is present in rec */
        /* i is physical pos here */
        offs = rec_2_get_field_end_info_low(rec, i);
      }
    } else {
      /* i is physical pos here */
      offs = rec_2_get_field_end_info_low(rec, i);
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

    ut_ad(i - dropped_col_count < rec_get_n_fields_old_raw(rec) ||
          (offs & REC_OFFS_SQL_NULL) || (offs & REC_OFFS_DEFAULT) ||
          (offs & REC_OFFS_DROP));
    rec_offs_base(offsets)[1 + i] = offs;
  } while (++i < rec_offs_n_fields(offsets));
}

/** Initialize offset for each field in an old style record.
@param[in]      rec     physical record
@param[in]      index   record descriptor
@param[in, out] offsets array of offsets */
static void rec_init_offsets_old(const rec_t *rec, const dict_index_t *index,
                                 ulint *offsets) {
  /* Old-style record: determine extra size and end offsets */

  uint32_t row_version = 0;
  if (rec_old_is_versioned(rec)) {
    ut_ad(index->is_clustered());
    /* Read the version information */
    row_version = rec_get_instant_row_version_old(rec);
    ut_ad(is_valid_row_version(row_version));
    ut_ad(index->has_row_versions() ||
          (index->table->is_upgraded_instant() && row_version == 0));
  }

  if (rec_get_1byte_offs_flag(rec)) {
    rec_init_offset_old_1byte(rec, index, offsets, row_version);
  } else {
    rec_init_offset_old_2byte(rec, index, offsets, row_version);
  }
}

void rec_init_offsets(const rec_t *rec, const dict_index_t *index,
                      ulint *offsets) {
  rec_offs_make_valid(rec, index, offsets);

  if (dict_table_is_comp(index->table)) {
    rec_init_offsets_new(rec, index, offsets);
  } else {
    rec_init_offsets_old(rec, index, offsets);
  }
}

ulint *rec_get_offsets(const rec_t *rec, const dict_index_t *index,
                       ulint *offsets, ulint n_fields, ut::Location location,
                       mem_heap_t **heap) {
  ulint n;

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
    }
  } else {
    n = rec_get_n_fields_old(rec, index);
  }

  if (UNIV_UNLIKELY(n_fields < n)) {
    n = n_fields;
  }

  /* The offsets header consists of the allocation size at
  offsets[0] and the REC_OFFS_HEADER_SIZE bytes. */
  ulint size = n + (1 + REC_OFFS_HEADER_SIZE);

  if (UNIV_UNLIKELY(!offsets) ||
      UNIV_UNLIKELY(rec_offs_get_n_alloc(offsets) < size)) {
    if (UNIV_UNLIKELY(!*heap)) {
      *heap = mem_heap_create(size * sizeof(ulint), location);
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

#ifdef UNIV_DEBUG
/** Check if the given two record offsets are identical.
@param[in]  offsets1  field offsets of a record
@param[in]  offsets2  field offsets of a record
@return true if they are identical, false otherwise. */
bool rec_offs_cmp(ulint *offsets1, ulint *offsets2) {
  ulint n1 = rec_offs_n_fields(offsets1);
  ulint n2 = rec_offs_n_fields(offsets2);

  if (n1 != n2) {
    return (false);
  }

  for (ulint i = 0; i < n1; ++i) {
    ulint len_1;
    /* nullptr for index as we compare all the fields, so it doesn't matter */
    ulint field_offset_1 = rec_get_nth_field_offs(nullptr, offsets1, i, &len_1);

    ulint len_2;
    /* nullptr for index as we compare all the fields, so it doesn't matter */
    ulint field_offset_2 = rec_get_nth_field_offs(nullptr, offsets2, i, &len_2);

    if (field_offset_1 != field_offset_2) {
      return (false);
    }

    if (len_1 != len_2) {
      return (false);
    }
  }
  return (true);
}

/** Print the record offsets.
@param[in]    out         the output stream to which offsets are printed.
@param[in]    offsets     the field offsets of the record.
@return the output stream. */
std::ostream &rec_offs_print(std::ostream &out, const ulint *offsets) {
  ulint n = rec_offs_n_fields(offsets);

  out << "[rec offsets: &offsets[0]=" << (void *)&offsets[0] << ", n=" << n
      << std::endl;
  for (ulint i = 0; i < n; ++i) {
    ulint len;
    /* nullptr for index as we print all the fields, so it doesn't matter */
    ulint field_offset = rec_get_nth_field_offs(nullptr, offsets, i, &len);
    out << "i=" << i << ", offsets[" << i << "]=" << offsets[i]
        << ", field_offset=" << field_offset << ", len=" << len << std::endl;
  }
  out << "]" << std::endl;
  return (out);
}

#endif /* UNIV_DEBUG */
