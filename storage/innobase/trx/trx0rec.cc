/*****************************************************************************

Copyright (c) 1996, 2022, Oracle and/or its affiliates.

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

/** @file trx/trx0rec.cc
 Transaction undo log record

 Created 3/26/1996 Heikki Tuuri
 *******************************************************/

#include "trx0rec.h"

#include <sys/types.h>

#include "fsp0fsp.h"
#include "mach0data.h"
#include "mtr0log.h"
#include "trx0undo.h"
#include "ut0dbg.h"
#ifndef UNIV_HOTBACKUP
#include "dict0dict.h"
#include "fsp0sysspace.h"
#include "lob0index.h"
#include "lob0inf.h"
#include "lob0lob.h"
#include "que0que.h"
#include "read0read.h"
#include "row0ext.h"
#include "row0mysql.h"
#include "row0row.h"
#include "row0upd.h"
#include "trx0purge.h"
#include "trx0rseg.h"
#include "ut0mem.h"

#include "my_dbug.h"

namespace dd {
class Spatial_reference_system;
}

/*=========== UNDO LOG RECORD CREATION AND DECODING ====================*/

/** Writes the mtr log entry of the inserted undo log record on the undo log
 page. */
static inline void trx_undof_page_add_undo_rec_log(
    page_t *undo_page, /*!< in: undo log page */
    ulint old_free,    /*!< in: start offset of the inserted entry */
    ulint new_free,    /*!< in: end offset of the entry */
    mtr_t *mtr)        /*!< in: mtr */
{
  byte *log_ptr = nullptr;
  const byte *log_end;
  ulint len;

  if (!mlog_open(mtr, 11 + 13 + MLOG_BUF_MARGIN, log_ptr)) {
    return;
  }

  log_end = &log_ptr[11 + 13 + MLOG_BUF_MARGIN];
  log_ptr = mlog_write_initial_log_record_fast(undo_page, MLOG_UNDO_INSERT,
                                               log_ptr, mtr);
  len = new_free - old_free - 4;

  mach_write_to_2(log_ptr, len);
  log_ptr += 2;

  if (log_ptr + len <= log_end) {
    memcpy(log_ptr, undo_page + old_free + 2, len);
    mlog_close(mtr, log_ptr + len);
  } else {
    mlog_close(mtr, log_ptr);
    mlog_catenate_string(mtr, undo_page + old_free + 2, len);
  }
}
#endif /* !UNIV_HOTBACKUP */

/** Parses a redo log record of adding an undo log record.
 @return end of log record or NULL */
byte *trx_undo_parse_add_undo_rec(byte *ptr,     /*!< in: buffer */
                                  byte *end_ptr, /*!< in: buffer end */
                                  page_t *page)  /*!< in: page or NULL */
{
  ulint len;
  byte *rec;
  ulint first_free;

  if (end_ptr < ptr + 2) {
    return (nullptr);
  }

  len = mach_read_from_2(ptr);
  ptr += 2;

  if (end_ptr < ptr + len) {
    return (nullptr);
  }

  if (page == nullptr) {
    return (ptr + len);
  }

  first_free = mach_read_from_2(page + TRX_UNDO_PAGE_HDR + TRX_UNDO_PAGE_FREE);
  rec = page + first_free;

  mach_write_to_2(rec, first_free + 4 + len);
  mach_write_to_2(rec + 2 + len, first_free);

  mach_write_to_2(page + TRX_UNDO_PAGE_HDR + TRX_UNDO_PAGE_FREE,
                  first_free + 4 + len);
  ut_memcpy(rec + 2, ptr, len);

  return (ptr + len);
}

#ifndef UNIV_HOTBACKUP
/** Calculates the free space left for extending an undo log record.
 @return bytes left */
static inline ulint trx_undo_left(const page_t *page, /*!< in: undo log page */
                                  const byte *ptr) /*!< in: pointer to page */
{
  /* The '- 10' is a safety margin, in case we have some small
  calculation error below */

#ifdef UNIV_DEBUG
  ut_ad(ptr >= page);
  size_t diff = ptr - page;
  size_t max_free = UNIV_PAGE_SIZE - 10 - FIL_PAGE_DATA_END;
  ut_ad(diff < UNIV_PAGE_SIZE);
  ut_ad(diff <= max_free);
#endif /* UNIV_DEBUG */

  return (UNIV_PAGE_SIZE - (ptr - page) - 10 - FIL_PAGE_DATA_END);
}

size_t trx_undo_max_free_space() {
  /* Starting from an empty undo page. The following calculation is based
  on what free space is got from trx_undo_reuse_cached(), trx_undo_create()
  and trx_undo_left(). Current simplified free_space would be
  UNIV_PAGE_SIZE - 290. */
  size_t free_space =
      UNIV_PAGE_SIZE - (TRX_UNDO_SEG_HDR + TRX_UNDO_SEG_HDR_SIZE +
                        TRX_UNDO_LOG_XA_HDR_SIZE + FIL_PAGE_DATA_END + 10);

  /* Undo number, table id, undo log type and pointer to next.
  Also refer to the beginning of trx_undo_page_report_insert() */
  free_space -= (11 + 11 + 1 + 2);

  /* For simplification, the max record length should be
  UNIV_PAGE_SIZE - 290 - 25 = UNIV_PAGE_SIZE - 315. */

  return (free_space);
}

/** Set the next and previous pointers in the undo page for the undo record
 that was written to ptr. Update the first free value by the number of bytes
 written for this undo record.
 @return offset of the inserted entry on the page if succeeded, 0 if fail */
static ulint trx_undo_page_set_next_prev_and_add(
    page_t *undo_page, /*!< in/out: undo log page */
    byte *ptr,         /*!< in: ptr up to where data has been
                       written on this undo page. */
    mtr_t *mtr)        /*!< in: mtr */
{
  ulint first_free; /*!< offset within undo_page */
  ulint end_of_rec; /*!< offset within undo_page */
  byte *ptr_to_first_free;
  /* pointer within undo_page
  that points to the next free
  offset value within undo_page.*/

  ut_ad(ptr > undo_page);
  ut_ad(ptr < undo_page + UNIV_PAGE_SIZE);

  if (UNIV_UNLIKELY(trx_undo_left(undo_page, ptr) < 2)) {
    return (0);
  }

  ptr_to_first_free = undo_page + TRX_UNDO_PAGE_HDR + TRX_UNDO_PAGE_FREE;

  first_free = mach_read_from_2(ptr_to_first_free);

  /* Write offset of the previous undo log record */
  mach_write_to_2(ptr, first_free);
  ptr += 2;

  end_of_rec = ptr - undo_page;

  /* Write offset of the next undo log record */
  mach_write_to_2(undo_page + first_free, end_of_rec);

  /* Update the offset to first free undo record */
  mach_write_to_2(ptr_to_first_free, end_of_rec);

  /* Write this log entry to the UNDO log */
  trx_undof_page_add_undo_rec_log(undo_page, first_free, end_of_rec, mtr);

  return (first_free);
}

/** Virtual column undo log version. To distinguish it from a length value
in 5.7.8 undo log, it starts with 0xF1 */
static const ulint VIRTUAL_COL_UNDO_FORMAT_1 = 0xF1;

/** Decide if the following undo log record is a multi-value virtual column
@param[in]      undo_rec        undo log record
@return true if this is a multi-value virtual column log, otherwise false */
bool trx_undo_rec_is_multi_value(const byte *undo_rec) {
  return (Multi_value_logger::is_multi_value_log(undo_rec));
}

/** Write virtual column index info (index id and column position in index)
to the undo log
@param[in,out]  undo_page       undo log page
@param[in]      table           the table
@param[in]      pos             the virtual column position
@param[in]      ptr             undo log record being written
@param[in]      first_v_col     whether this is the first virtual column
                                which could start with a version marker
@return new undo log pointer */
static byte *trx_undo_log_v_idx(page_t *undo_page, const dict_table_t *table,
                                ulint pos, byte *ptr, bool first_v_col) {
  ut_ad(pos < table->n_v_def);
  dict_v_col_t *vcol = dict_table_get_nth_v_col(table, pos);

  ulint n_idx = vcol->v_indexes->size();
  byte *old_ptr;

  ut_ad(n_idx > 0);

  /* Size to reserve, max 5 bytes for each index id and position, plus
  5 bytes for num of indexes, 2 bytes for write total length.
  1 byte for undo log record format version marker */
  ulint size = n_idx * (5 + 5) + 5 + 2 + (first_v_col ? 1 : 0);

  if (trx_undo_left(undo_page, ptr) < size) {
    return (nullptr);
  }

  if (first_v_col) {
    /* write the version marker */
    mach_write_to_1(ptr, VIRTUAL_COL_UNDO_FORMAT_1);

    ptr += 1;
  }

  old_ptr = ptr;

  ptr += 2;

  ptr += mach_write_compressed(ptr, n_idx);

  dict_v_idx_list::iterator it;

  for (it = vcol->v_indexes->begin(); it != vcol->v_indexes->end(); ++it) {
    dict_v_idx_t v_index = *it;

    ptr += mach_write_compressed(ptr, static_cast<ulint>(v_index.index->id));

    ptr += mach_write_compressed(ptr, v_index.nth_field);
  }

  mach_write_to_2(old_ptr, ptr - old_ptr);

  return (ptr);
}

/** Read virtual column index from undo log, and verify the column is still
indexed, and return its position
@param[in]      table           the table
@param[in]      ptr             undo log pointer
@param[out]     col_pos         the column number or ULINT_UNDEFINED
                                if the column is not indexed any more
@return remaining part of undo log record after reading these values */
static const byte *trx_undo_read_v_idx_low(const dict_table_t *table,
                                           const byte *ptr, ulint *col_pos) {
  ulint len = mach_read_from_2(ptr);
  const byte *old_ptr = ptr;

  *col_pos = ULINT_UNDEFINED;

  ptr += 2;

  ulint num_idx = mach_read_next_compressed(&ptr);

  ut_ad(num_idx > 0);

  const dict_index_t *clust_index = table->first_index();

  for (ulint i = 0; i < num_idx; i++) {
    space_index_t id = mach_read_next_compressed(&ptr);
    ulint pos = mach_read_next_compressed(&ptr);
    const dict_index_t *index = clust_index->next();

    while (index != nullptr) {
      /* Return if we find a matching index.
      TODO: in the future, it might be worth to add
      checks on other indexes */
      if (index->id == id) {
        const dict_col_t *col = index->get_col(pos);
        ut_ad(col->is_virtual());
        const dict_v_col_t *vcol = reinterpret_cast<const dict_v_col_t *>(col);
        *col_pos = vcol->v_pos;
        return (old_ptr + len);
      }

      index = index->next();
    }
  }

  return (old_ptr + len);
}

/** Read virtual column index from undo log or online log if the log
contains such info, and in the undo log case, verify the column is
still indexed, and output its position
@param[in]      table           the table
@param[in]      ptr             undo log pointer
@param[in]      first_v_col     if this is the first virtual column, which
                                has the version marker
@param[in,out]  is_undo_log     this function is used to parse both undo log,
                                and online log for virtual columns. So
                                check to see if this is undo log. When
                                first_v_col is true, is_undo_log is output,
                                when first_v_col is false, is_undo_log is input
@param[in,out]  field_no        the column number
@return remaining part of undo log record after reading these values */
const byte *trx_undo_read_v_idx(const dict_table_t *table, const byte *ptr,
                                bool first_v_col, bool *is_undo_log,
                                ulint *field_no) {
  /* Version marker only put on the first virtual column */
  if (first_v_col) {
    /* Undo log has the virtual undo log marker */
    *is_undo_log = (mach_read_from_1(ptr) == VIRTUAL_COL_UNDO_FORMAT_1);

    if (*is_undo_log) {
      ptr += 1;
    }
  }

  if (*is_undo_log) {
    ptr = trx_undo_read_v_idx_low(table, ptr, field_no);
  } else {
    *field_no -= REC_MAX_N_FIELDS;
  }

  return (ptr);
}

/** Store the multi-value column information for undo log
@param[in,out]  undo_page       undo page to store the information
@param[in]      vfield          multi-value field information
@param[in,out]  ptr             pointer where to store the information
@return true if stored successfully, false if space is not enough */
static bool trx_undo_store_multi_value(page_t *undo_page,
                                       const dfield_t *vfield, byte **ptr) {
  Multi_value_logger mv_logger(
      static_cast<multi_value_data *>(dfield_get_data(vfield)),
      dfield_get_len(vfield));
  uint32_t log_len = mv_logger.get_log_len(false);

  if (trx_undo_left(undo_page, *ptr) < log_len) {
    return (false);
  }

  mv_logger.log(ptr);

  return (true);
}

/** Reports in the undo log of an insert of virtual columns.
@param[in]      undo_page       undo log page
@param[in]      table           the table
@param[in]      row             dtuple contains the virtual columns
@param[in,out]  ptr             log ptr
@return true if write goes well, false if out of space */
static bool trx_undo_report_insert_virtual(page_t *undo_page,
                                           dict_table_t *table,
                                           const dtuple_t *row, byte **ptr) {
  byte *start = *ptr;
  bool first_v_col = true;

  if (trx_undo_left(undo_page, *ptr) < 2) {
    return (false);
  }

  /* Reserve 2 bytes to write the number
  of bytes the stored fields take in this
  undo record */
  *ptr += 2;

  for (ulint col_no = 0; col_no < dict_table_get_n_v_cols(table); col_no++) {
    dfield_t *vfield = nullptr;

    const dict_v_col_t *col = dict_table_get_nth_v_col(table, col_no);

    if (col->m_col.ord_part) {
      /* make sure enough space to write the length */
      if (trx_undo_left(undo_page, *ptr) < 5) {
        return (false);
      }

      ulint pos = col_no;
      pos += REC_MAX_N_FIELDS;
      *ptr += mach_write_compressed(*ptr, pos);

      *ptr = trx_undo_log_v_idx(undo_page, table, col_no, *ptr, first_v_col);
      first_v_col = false;

      if (*ptr == nullptr) {
        return (false);
      }

      vfield = dtuple_get_nth_v_field(row, col->v_pos);

      ulint flen = vfield->len;

      if (col->m_col.is_multi_value()) {
        bool suc = trx_undo_store_multi_value(undo_page, vfield, ptr);

        if (!suc) {
          return (false);
        }
      } else if (flen != UNIV_SQL_NULL) {
        ulint max_len = dict_max_v_field_len_store_undo(table, col_no);

        if (flen > max_len) {
          flen = max_len;
        }

        if (trx_undo_left(undo_page, *ptr) < flen + 5) {
          return (false);
        }
        *ptr += mach_write_compressed(*ptr, flen);

        ut_memcpy(*ptr, vfield->data, flen);
        *ptr += flen;
      } else {
        if (trx_undo_left(undo_page, *ptr) < 5) {
          return (false);
        }

        *ptr += mach_write_compressed(*ptr, flen);
      }
    }
  }

  /* Always mark the end of the log with 2 bytes length field */
  mach_write_to_2(start, *ptr - start);

  return (true);
}

/** Reports in the undo log of an insert of a clustered index record.
 @return offset of the inserted entry on the page if succeed, 0 if fail */
static ulint trx_undo_page_report_insert(
    page_t *undo_page,           /*!< in: undo log page */
    trx_t *trx,                  /*!< in: transaction */
    dict_index_t *index,         /*!< in: clustered index */
    const dtuple_t *clust_entry, /*!< in: index entry which will be
                                 inserted to the clustered index */
    mtr_t *mtr)                  /*!< in: mtr */
{
  ulint first_free;
  byte *ptr;
  ulint i;

  ut_ad(index->is_clustered());
  ut_ad(mach_read_from_2(undo_page + TRX_UNDO_PAGE_HDR + TRX_UNDO_PAGE_TYPE) ==
        TRX_UNDO_INSERT);

  first_free =
      mach_read_from_2(undo_page + TRX_UNDO_PAGE_HDR + TRX_UNDO_PAGE_FREE);
  ptr = undo_page + first_free;

  ut_ad(first_free <= UNIV_PAGE_SIZE);

  if (trx_undo_left(undo_page, ptr) < 2 + 1 + 11 + 11) {
    /* Not enough space for writing the general parameters */

    return (0);
  }

  /* Reserve 2 bytes for the pointer to the next undo log record */
  ptr += 2;

  /* Store first some general parameters to the undo log */
  *ptr++ = TRX_UNDO_INSERT_REC;
  ptr += mach_u64_write_much_compressed(ptr, trx->undo_no);
  ptr += mach_u64_write_much_compressed(ptr, index->table->id);
  /*----------------------------------------*/
  /* Store then the fields required to uniquely determine the record
  to be inserted in the clustered index */

  for (i = 0; i < dict_index_get_n_unique(index); i++) {
    const dfield_t *field = dtuple_get_nth_field(clust_entry, i);
    ulint flen = dfield_get_len(field);

    if (trx_undo_left(undo_page, ptr) < 5) {
      return (0);
    }

    ptr += mach_write_compressed(ptr, flen);

    if (flen != UNIV_SQL_NULL && flen != 0) {
      if (trx_undo_left(undo_page, ptr) < flen) {
        return (0);
      }

      ut_memcpy(ptr, dfield_get_data(field), flen);
      ptr += flen;
    }
  }

  if (index->table->n_v_cols) {
    if (!trx_undo_report_insert_virtual(undo_page, index->table, clust_entry,
                                        &ptr)) {
      return (0);
    }
  }

  return (trx_undo_page_set_next_prev_and_add(undo_page, ptr, mtr));
}

/** Reads from an undo log record the general parameters.
 @return remaining part of undo log record after reading these values */
byte *trx_undo_rec_get_pars(
    trx_undo_rec_t *undo_rec, /*!< in: undo log record */
    ulint *type,              /*!< out: undo record type:
                              TRX_UNDO_INSERT_REC, ... */
    ulint *cmpl_info,         /*!< out: compiler info, relevant only
                              for update type records */
    bool *updated_extern,     /*!< out: true if we updated an
                              externally stored fild */
    undo_no_t *undo_no,       /*!< out: undo log record number */
    table_id_t *table_id,     /*!< out: table id */
    type_cmpl_t &type_cmpl)   /*!< out: type compilation info */
{
  const byte *ptr;

  ptr = undo_rec + 2;
  ptr = type_cmpl.read(ptr);

  *updated_extern = type_cmpl.is_lob_updated();
  *type = type_cmpl.type_info();
  *cmpl_info = type_cmpl.cmpl_info();

  if (type_cmpl.is_lob_undo()) {
    /* Reading the new 1-byte undo record flag. */
    uint8_t undo_rec_flags = 0x00;

    undo_rec_flags = mach_read_from_1(ptr);
    ptr++;

    ut_a(undo_rec_flags == 0x00);
  }

  *undo_no = mach_read_next_much_compressed(&ptr);
  *table_id = mach_read_next_much_compressed(&ptr);

  return (const_cast<byte *>(ptr));
}

/** Reads from an undo log record the table ID
@param[in]      undo_rec        Undo log record
@return the table ID */
table_id_t trx_undo_rec_get_table_id(const trx_undo_rec_t *undo_rec) {
  const byte *ptr = undo_rec + 2;
  uint8_t type_cmpl = mach_read_from_1(ptr);

  const bool blob_undo = type_cmpl & TRX_UNDO_MODIFY_BLOB;

  if (blob_undo) {
    /* The next record offset takes 2 bytes + 1 byte for
    type_cmpl flag + 1 byte for the new flag. Total 4 bytes.
    The new flag is currently unused and is available for
    future use. */
    ptr = undo_rec + 4;
  } else {
    ptr = undo_rec + 3;
  }

  /* Skip the UNDO number */
  mach_read_next_much_compressed(&ptr);

  /* Read the table ID */
  return (mach_read_next_much_compressed(&ptr));
}

/** Read from an undo log record of a multi-value virtual column.
@param[in]      ptr     pointer to remaining part of the undo record
@param[in,out]  field   stored field, nullptr if the col is no longer
                        indexed or existing, in the latter case,
                        this function will only skip the log
@param[in,out]  heap    memory heap
@return remaining part of undo log record after reading these values */
const byte *trx_undo_rec_get_multi_value(const byte *ptr, dfield_t *field,
                                         mem_heap_t *heap) {
  if (field == nullptr) {
    return (ptr + Multi_value_logger::read_log_len(ptr));
  }

  return (Multi_value_logger::read(ptr, field, heap));
}

/** Read from an undo log record a non-virtual column value.
@param[in,out]  ptr             pointer to remaining part of the undo record
@param[in,out]  field           stored field
@param[in,out]  len             length of the field, or UNIV_SQL_NULL
@param[in,out]  orig_len        original length of the locally stored part
of an externally stored column, or 0
@return remaining part of undo log record after reading these values */
byte *trx_undo_rec_get_col_val(const byte *ptr, const byte **field, ulint *len,
                               ulint *orig_len) {
  *len = mach_read_next_compressed(&ptr);
  *orig_len = 0;

  switch (*len) {
    case UNIV_SQL_NULL:
      *field = nullptr;
      break;
    case UNIV_EXTERN_STORAGE_FIELD:
      *orig_len = mach_read_next_compressed(&ptr);
      *len = mach_read_next_compressed(&ptr);
      *field = ptr;
      ptr += *len & ~SPATIAL_STATUS_MASK;

      ut_ad(*orig_len >= BTR_EXTERN_FIELD_REF_SIZE);
      ut_ad(*len > *orig_len);
      /* @see dtuple_convert_big_rec() */
      ut_ad(*len >= BTR_EXTERN_FIELD_REF_SIZE);

      /* we do not have access to index->table here
      ut_ad(dict_table_has_atomic_blobs(index->table)
            || *len >= col->max_prefix
            + BTR_EXTERN_FIELD_REF_SIZE);
      */

      *len += UNIV_EXTERN_STORAGE_FIELD;
      break;
    default:
      *field = ptr;
      if (*len >= UNIV_EXTERN_STORAGE_FIELD) {
        ptr += (*len - UNIV_EXTERN_STORAGE_FIELD) & ~SPATIAL_STATUS_MASK;
      } else {
        ptr += *len;
      }
  }

  return (const_cast<byte *>(ptr));
}

/** Builds a row reference from an undo log record.
 @return pointer to remaining part of undo record */
byte *trx_undo_rec_get_row_ref(
    byte *ptr,           /*!< in: remaining part of a copy of an undo log
                         record, at the start of the row reference;
                         NOTE that this copy of the undo log record must
                         be preserved as long as the row reference is
                         used, as we do NOT copy the data in the
                         record! */
    dict_index_t *index, /*!< in: clustered index */
    dtuple_t **ref,      /*!< out, own: row reference */
    mem_heap_t *heap)    /*!< in: memory heap from which the memory
                         needed is allocated */
{
  ulint ref_len;
  ulint i;

  ut_ad(index && ptr && ref && heap);
  ut_a(index->is_clustered());

  ref_len = dict_index_get_n_unique(index);

  *ref = dtuple_create(heap, ref_len);

  dict_index_copy_types(*ref, index, ref_len);

  for (i = 0; i < ref_len; i++) {
    dfield_t *dfield;
    const byte *field;
    ulint len;
    ulint orig_len;

    dfield = dtuple_get_nth_field(*ref, i);

    ptr = trx_undo_rec_get_col_val(ptr, &field, &len, &orig_len);

    dfield_set_data(dfield, field, len);
  }

  return (ptr);
}

/** Skips a row reference from an undo log record.
 @return pointer to remaining part of undo record */
static byte *trx_undo_rec_skip_row_ref(
    byte *ptr,                 /*!< in: remaining part in update undo log
                               record, at the start of the row reference */
    const dict_index_t *index) /*!< in: clustered index */
{
  ulint ref_len;
  ulint i;

  ut_ad(index && ptr);
  ut_a(index->is_clustered());

  ref_len = dict_index_get_n_unique(index);

  for (i = 0; i < ref_len; i++) {
    const byte *field;
    ulint len;
    ulint orig_len;

    ptr = trx_undo_rec_get_col_val(ptr, &field, &len, &orig_len);
  }

  return (ptr);
}

/** Fetch a prefix of an externally stored column, for writing to the undo
log of an update or delete marking of a clustered index record.
@param[in]      trx             transaction object
@param[in]      index           the clustered index object
@param[out]     ext_buf         buffer to hold the prefix data and BLOB pointer
@param[in]      prefix_len      prefix size to store in the undo log
@param[in]      page_size       page size
@param[in]      field           an externally stored column
@param[in]      is_sdi          true for SDI indexes
@param[in,out]  len             input: length of field; output: used length of
ext_buf
@return ext_buf */
static byte *trx_undo_page_fetch_ext(trx_t *trx, dict_index_t *index,
                                     byte *ext_buf, ulint prefix_len,
                                     const page_size_t &page_size,
                                     const byte *field,
                                     IF_DEBUG(bool is_sdi, ) ulint *len) {
  /* Fetch the BLOB. */
  ulint ext_len = lob::btr_copy_externally_stored_field_prefix_func(
      trx, index, ext_buf, prefix_len, page_size, field,
      IF_DEBUG(is_sdi, ) * len);

#ifdef UNIV_DEBUG
  if (ext_len == 0) {
    byte *field_ref = const_cast<byte *>(field) + (*len) - lob::ref_t::SIZE;
    lob::ref_t ref(field_ref);
    lob::ref_mem_t ref_mem;
    ref.parse(ref_mem);
    lob::print(trx, index, std::cout, ref, true);
  }
#endif /* UNIV_DEBUG */

  /* BLOBs should always be nonempty. */
  ut_a(ext_len > 0);
  /* Append the BLOB pointer to the prefix. */
  memcpy(ext_buf + ext_len, field + *len - BTR_EXTERN_FIELD_REF_SIZE,
         BTR_EXTERN_FIELD_REF_SIZE);
  *len = ext_len + BTR_EXTERN_FIELD_REF_SIZE;
  return (ext_buf);
}

/** Writes to the undo log a prefix of an externally stored column.
@param[in]      trx             transaction object
@param[in]      index           the clustered index object
@param[out]     ptr             undo log position, at least 15 bytes must be
                                available
@param[out]     ext_buf         a buffer of DICT_MAX_FIELD_LEN_BY_FORMAT()
                                size, or NULL when should not fetch a longer
                                prefix
@param[in]      prefix_len      prefix size to store in the undo log
@param[in]      page_size       page size
@param[in,out]  field           the locally stored part of the externally
stored column
@param[in,out]  len             length of field, in bytes
@param[in]      is_sdi          true for SDI indexes
@param[in]      spatial_status  whether the column is used by spatial index or
                                regular index
@return undo log position */
static byte *trx_undo_page_report_modify_ext_func(
    trx_t *trx, dict_index_t *index, byte *ptr, byte *ext_buf, ulint prefix_len,
    const page_size_t &page_size, const byte **field, ulint *len,
    IF_DEBUG(bool is_sdi, ) spatial_status_t spatial_status) {
  ulint spatial_len = 0;

  switch (spatial_status) {
    case SPATIAL_UNKNOWN:
    case SPATIAL_NONE:
      break;

    case SPATIAL_MIXED:
    case SPATIAL_ONLY:
      spatial_len = DATA_MBR_LEN;
      break;
  }

  /* Encode spatial status into length. */
  spatial_len |= spatial_status << SPATIAL_STATUS_SHIFT;

  if (spatial_status == SPATIAL_ONLY) {
    /* If the column is only used by gis index, log its
    MBR is enough.*/
    ptr += mach_write_compressed(ptr, UNIV_EXTERN_STORAGE_FIELD + spatial_len);

    return (ptr);
  }

  if (ext_buf) {
    ut_a(prefix_len > 0);

    /* If an ordering column is externally stored, we will
    have to store a longer prefix of the field.  In this
    case, write to the log a marker followed by the
    original length and the real length of the field. */
    ptr += mach_write_compressed(ptr, UNIV_EXTERN_STORAGE_FIELD);

    ptr += mach_write_compressed(ptr, *len);

    *field = trx_undo_page_fetch_ext(trx, index, ext_buf, prefix_len, page_size,
                                     *field, IF_DEBUG(is_sdi, ) len);

    ptr += mach_write_compressed(ptr, *len + spatial_len);
  } else {
    ptr += mach_write_compressed(
        ptr, UNIV_EXTERN_STORAGE_FIELD + *len + spatial_len);
  }

  return (ptr);
}

static inline byte *trx_undo_page_report_modify_ext(
    trx_t *trx, dict_index_t *index, byte *ptr, byte *ext_buf, ulint prefix_len,
    const page_size_t &page_size, const byte **field, ulint *len,
    bool is_sdi [[maybe_unused]], spatial_status_t spatial_status) {
  return trx_undo_page_report_modify_ext_func(
      trx, index, ptr, ext_buf, prefix_len, page_size, field, len,
      IF_DEBUG(is_sdi, ) spatial_status);
}

/** Get MBR from a Geometry column stored externally
@param[in]      trx             transaction object
@param[in]      index           the clustered index object
@param[out]     mbr             MBR to fill
@param[in]      page_size       table pagesize
@param[in]      field           field contain the geometry data
@param[in,out]  len             length of field, in bytes
@param[in]      srs             Spatial reference system of R-tree.
*/
static void trx_undo_get_mbr_from_ext(trx_t *trx, dict_index_t *index,
                                      double *mbr, const page_size_t &page_size,
                                      const byte *field, ulint *len,
                                      const dd::Spatial_reference_system *srs) {
  uchar *dptr = nullptr;
  ulint dlen;
  mem_heap_t *heap = mem_heap_create(100, UT_LOCATION_HERE);

  dptr = lob::btr_copy_externally_stored_field(
      trx, index, &dlen, nullptr, field, page_size, *len, false, heap);

  if (dlen <= GEO_DATA_HEADER_SIZE) {
    for (uint i = 0; i < SPDIMS; ++i) {
      mbr[i * 2] = DBL_MAX;
      mbr[i * 2 + 1] = -DBL_MAX;
    }
  } else {
    get_mbr_from_store(srs, dptr, static_cast<uint>(dlen), SPDIMS, mbr,
                       nullptr);
  }

  mem_heap_free(heap);
}

static const byte *trx_undo_read_blob_update(const byte *undo_ptr,
                                             upd_field_t *uf,
                                             lob::undo_vers_t *lob_undo) {
  DBUG_TRACE;

  /* Read one byte of flags. */
  uint8_t flag = *undo_ptr;
  ut_a(flag == 0x00);
  undo_ptr++;

  const ulint field_no = uf->field_no;

  /* Read the size of the vector. */
  ulint N = mach_read_next_compressed(&undo_ptr);

  if (N == 0) {
    return undo_ptr;
  }

  /* Read the LOB first page number*/
  uf->lob_first_page_no = mach_read_next_compressed(&undo_ptr);
  uf->lob_version = mach_read_next_compressed(&undo_ptr);
  uf->last_trx_id = mach_read_next_compressed(&undo_ptr);
  uf->last_undo_no = mach_read_next_compressed(&undo_ptr);

  for (size_t i = 0; i < N; ++i) {
    Lob_diff lob_diff(uf->heap);
    lob::undo_seq_t *lob_seq = nullptr;
    lob::undo_data_t lob_undo_data;

    if (lob_undo != nullptr) {
      lob_seq = lob_undo->get_undo_sequence(field_no);
    }

    /* Read the offset. */
    undo_ptr = lob_diff.read_offset(undo_ptr);
    lob_undo_data.m_offset = lob_diff.m_offset;

    /* Read the length. */
    undo_ptr = lob_diff.read_length(undo_ptr);

    /* Read the old data. */
    lob_diff.set_old_data(undo_ptr);

    /* Copy the data only if the lob_undo is not null. */
    if (lob_seq != nullptr) {
      undo_ptr = lob_undo_data.copy_old_data(undo_ptr, lob_diff.m_length);
    } else {
      undo_ptr += lob_diff.m_length;
    }

    lob_undo_data.m_version = uf->lob_version;
    lob_undo_data.m_page_no = uf->lob_first_page_no;

    if (lob_seq != nullptr) {
      lob_seq->m_field_no = field_no;
      lob_seq->push_back(lob_undo_data);
    }

    /* Read the number of LOB index entries modified. */
    ulint n_entry = mach_read_next_compressed(&undo_ptr);

    ut_ad(n_entry == 1 || n_entry == 2);

    for (size_t i = 0; i < n_entry; ++i) {
      lob_index_diff_t idx_diff;

      /* Read the modifier trx id of the LOB index entry. */
      idx_diff.m_modifier_trxid = mach_read_next_compressed(&undo_ptr);

      /* Write the modifier trx undo_no of the LOB index entry. */
      idx_diff.m_modifier_undo_no = mach_read_next_compressed(&undo_ptr);

      lob_diff.m_idx_diffs->push_back(idx_diff);
    }

    uf->push_lob_diff(lob_diff);
    DBUG_LOG("lob", lob_diff);
  }

  return undo_ptr;
}

/** Write the partial update information about LOBs to the undo log record.
@param[in]      undo_page       the undo page
@param[in]      index           the clustered index where LOBs are modified.
@param[in]      undo_ptr        the location within undo page where next
                                part of undo record is to be written.
@param[in]      field           the LOB data
@param[in]      flen            length of LOB data in bytes
@param[in]      update          the update vector containing partial update
                                information on LOBs.
@param[in]      fld             the field to which the LOB belongs.
@param[in]      mtr             the mini-transaction context.
@return the undo record pointer where new data can be written.
@return nullptr when there is not enough space in undo page. */
static byte *trx_undo_report_blob_update(page_t *undo_page, dict_index_t *index,
                                         byte *undo_ptr, const byte *field,
                                         ulint flen, const upd_t *update,
                                         upd_field_t *fld, mtr_t *mtr) {
  DBUG_TRACE;

  /* Access the LOB reference object. */
  byte *field_ref = const_cast<byte *>(field) + flen - lob::ref_t::SIZE;

  lob::ref_t ref(field_ref);

  /* Check if enough space for flag and vector length. */
  if (trx_undo_left(undo_page, undo_ptr) < 6) {
    return nullptr;
  }

  /* Write one byte of flags. */
  *undo_ptr = 0x00;
  undo_ptr++;

  if (fld == nullptr || update == nullptr) {
    /* Write the size of the vector as 0. */
    undo_ptr += mach_write_compressed(undo_ptr, 0);
    return undo_ptr;
  }

  /* Find the Binary_diff object */
  const Binary_diff_vector *bdiff_v =
      update->get_binary_diff_by_field_no(fld->field_no);

  if (bdiff_v == nullptr || !update->is_partially_updated(fld->field_no)) {
    /* Write the size of the vector as 0. */
    undo_ptr += mach_write_compressed(undo_ptr, 0);
    return undo_ptr;
  }

  const ulint bytes_changed = upd_t::get_total_modified_bytes(*bdiff_v);

  /* Whether the update to the LOB can be considered as a small change. */
  const bool small_change =
      (bytes_changed <= lob::ref_t::LOB_SMALL_CHANGE_THRESHOLD);

  if (!small_change) {
    /* This is not a small change.  So write the size of the vector as
    0 and bailout. */
    undo_ptr += mach_write_compressed(undo_ptr, 0);
    return undo_ptr;
  }

  const page_size_t page_size = dict_table_page_size(index->table);
  if (page_size.is_compressed()) {
    /* This is compressed LOB. Not yet supporting. */
    undo_ptr += mach_write_compressed(undo_ptr, 0);
    return undo_ptr;
  }

  trx_id_t last_trx_id;
  undo_no_t last_undo_no;
  ulint lob_version;
  page_type_t f_page_type;

  /* Obtain LOB info. */
  lob::get_info(ref, index, lob_version, last_trx_id, last_undo_no, f_page_type,
                mtr);

  /* Only the page type FIL_PAGE_TYPE_LOB_FIRST is supported here. */
  if (f_page_type != FIL_PAGE_TYPE_LOB_FIRST) {
    undo_ptr += mach_write_compressed(undo_ptr, 0);
    return undo_ptr;
  }

  /* Only for small changes to the BLOB, we do regular undo logging. */
  size_t N = bdiff_v->size();

  /* Write the size of the vector. */
  undo_ptr += mach_write_compressed(undo_ptr, N);

  if (N == 0) {
    return undo_ptr;
  }

  /* Check if there is enough space for lob_version, last_trx_id
  and last_undo_no. */
  if (trx_undo_left(undo_page, undo_ptr) < 20) {
    return nullptr;
  }

  /* Write the LOB first page number*/
  undo_ptr += mach_write_compressed(undo_ptr, ref.page_no());

  /* Write the lob version number */
  undo_ptr += mach_write_compressed(undo_ptr, lob_version);

  /* Write the last trx id */
  undo_ptr += mach_write_compressed(undo_ptr, last_trx_id);

  /* Write the last undo_no */
  undo_ptr += mach_write_compressed(undo_ptr, last_undo_no);

  for (size_t i = 0; i < N; ++i) {
    const Binary_diff &bdiff = bdiff_v->at(i);

    if (trx_undo_left(undo_page, undo_ptr) < 10) {
      return nullptr;
    }

    /* Write the offset. */
    undo_ptr += mach_write_compressed(undo_ptr, bdiff.offset());

    /* Write the length. */
    undo_ptr += mach_write_compressed(undo_ptr, bdiff.length());

    if (trx_undo_left(undo_page, undo_ptr) < bdiff.length()) {
      return nullptr;
    }

    /* Write the old data. */
    ut_memcpy(undo_ptr, bdiff.old_data(fld->mysql_field), bdiff.length());
    undo_ptr += bdiff.length();

    lob::List_iem_t entries;

    /* Find the affected LOB index entries. */
    lob::get_affected_index_entries(ref, index, bdiff, entries, mtr);

    ulint n_entry = entries.size();

    ut_ad(n_entry == 1 || n_entry == 2);

    /* Check if there is enough space for n_entry */
    if (trx_undo_left(undo_page, undo_ptr) < 5) {
      return nullptr;
    }

    /* Write the number of LOB index entries modified. */
    undo_ptr += mach_write_compressed(undo_ptr, n_entry);

    for (lob::List_iem_t::iterator iter = entries.begin();
         iter != entries.end(); ++iter) {
      if (trx_undo_left(undo_page, undo_ptr) < 10) {
        return nullptr;
      }

      /* Write the modifier trx id of the LOB index entry. */
      undo_ptr += mach_write_compressed(undo_ptr, iter->m_trx_id_modifier);

      /* Write the modifier trx undo_no of the LOB index entry. */
      undo_ptr += mach_write_compressed(undo_ptr, iter->m_undo_no_modifier);
    }
  }

  return undo_ptr;
}

/**********************************************************************/ /**
 Reports in the undo log of an update or delete marking of a clustered index
 record.
 @return byte offset of the inserted undo log entry on the page if
 succeed, 0 if fail */
static ulint trx_undo_page_report_modify(
    /*========================*/
    page_t *undo_page,    /*!< in: undo log page */
    trx_t *trx,           /*!< in: transaction */
    dict_index_t *index,  /*!< in: clustered index where update or
                          delete marking is done */
    const rec_t *rec,     /*!< in: clustered index record which
                          has NOT yet been modified */
    const ulint *offsets, /*!< in: rec_get_offsets(rec, index) */
    const upd_t *update,  /*!< in: update vector which tells the
                          columns to be updated; in the case of
                          a delete, this should be set to NULL */
    ulint cmpl_info,      /*!< in: compiler info on secondary
                          index updates */
    const dtuple_t *row,  /*!< in: clustered index row contains
                          virtual column info */
    mtr_t *mtr)           /*!< in: mtr */
{
  DBUG_TRACE;

  dict_table_t *table;
  ulint first_free;
  byte *ptr;
  const byte *field;
  ulint flen;
  ulint col_no;
  ulint type_cmpl;
  byte *type_cmpl_ptr;
  ulint i;
  trx_id_t trx_id;
  trx_undo_ptr_t *undo_ptr;
  bool ignore_prefix = false;
  byte ext_buf[REC_VERSION_56_MAX_INDEX_COL_LEN + BTR_EXTERN_FIELD_REF_SIZE];
  bool first_v_col = true;

  ut_a(index->is_clustered());
  ut_ad(rec_offs_validate(rec, index, offsets));
  ut_ad(mach_read_from_2(undo_page + TRX_UNDO_PAGE_HDR + TRX_UNDO_PAGE_TYPE) ==
        TRX_UNDO_UPDATE);
  table = index->table;

  /* If table instance is temporary then select noredo rseg as changes
  to undo logs don't need REDO logging given that they are not
  restored on restart as corresponding object doesn't exist on restart.*/
  undo_ptr =
      index->table->is_temporary() ? &trx->rsegs.m_noredo : &trx->rsegs.m_redo;

  first_free =
      mach_read_from_2(undo_page + TRX_UNDO_PAGE_HDR + TRX_UNDO_PAGE_FREE);
  ptr = undo_page + first_free;

  ut_ad(first_free <= UNIV_PAGE_SIZE);

  if (trx_undo_left(undo_page, ptr) < 50) {
    /* NOTE: the value 50 must be big enough so that the general
    fields written below fit on the undo log page */

    return 0;
  }

  /* Reserve 2 bytes for the pointer to the next undo log record */
  ptr += 2;

  /* Store first some general parameters to the undo log */

  if (!update) {
    ut_ad(!rec_get_deleted_flag(rec, dict_table_is_comp(table)));
    type_cmpl = TRX_UNDO_DEL_MARK_REC;
  } else if (rec_get_deleted_flag(rec, dict_table_is_comp(table))) {
    type_cmpl = TRX_UNDO_UPD_DEL_REC;
    /* We are about to update a delete marked record.
    We don't typically need the prefix in this case unless
    the delete marking is done by the same transaction
    (which we check below). */
    ignore_prefix = true;
  } else {
    type_cmpl = TRX_UNDO_UPD_EXIST_REC;
  }

  type_cmpl |= cmpl_info * TRX_UNDO_CMPL_INFO_MULT;
  type_cmpl_ptr = ptr;

  *ptr++ = (byte)type_cmpl;

  /* Introducing a change in undo log format. */
  *type_cmpl_ptr |= TRX_UNDO_MODIFY_BLOB;

  /* Introducing a new 1-byte flag. */
  *ptr++ = 0x00;

  ptr += mach_u64_write_much_compressed(ptr, trx->undo_no);

  ptr += mach_u64_write_much_compressed(ptr, table->id);

  /*----------------------------------------*/
  /* Store the state of the info bits */

  *ptr++ = (byte)rec_get_info_bits(rec, dict_table_is_comp(table));

  /* Store the values of the system columns */
  field = rec_get_nth_field(nullptr, rec, offsets,
                            index->get_sys_col_pos(DATA_TRX_ID), &flen);
  ut_ad(flen == DATA_TRX_ID_LEN);

  trx_id = trx_read_trx_id(field);

  /* If it is an update of a delete marked record, then we are
  allowed to ignore blob prefixes if the delete marking was done
  by some other trx as it must have committed by now for us to
  allow an over-write. */
  if (ignore_prefix) {
    ignore_prefix = (trx_id != trx->id);
  }
  ptr += mach_u64_write_compressed(ptr, trx_id);

  field = rec_get_nth_field(nullptr, rec, offsets,
                            index->get_sys_col_pos(DATA_ROLL_PTR), &flen);
  ut_ad(flen == DATA_ROLL_PTR_LEN);

  ptr += mach_u64_write_compressed(ptr, trx_read_roll_ptr(field));

  /*----------------------------------------*/
  /* Store then the fields required to uniquely determine the
  record which will be modified in the clustered index */

  for (i = 0; i < dict_index_get_n_unique(index); i++) {
    field = rec_get_nth_field(index, rec, offsets, i, &flen);

    /* The ordering columns must not be stored externally. */
    ut_ad(!rec_offs_nth_extern(index, offsets, i));
    ut_ad(!rec_offs_nth_default(index, offsets, i));
    ut_ad(index->get_col(i)->ord_part);

    if (trx_undo_left(undo_page, ptr) < 5) {
      return 0;
    }

    ptr += mach_write_compressed(ptr, flen);

    if (flen != UNIV_SQL_NULL) {
      if (trx_undo_left(undo_page, ptr) < flen) {
        return 0;
      }

      ut_memcpy(ptr, field, flen);
      ptr += flen;
    }
  }

  /*----------------------------------------*/
  /* Save to the undo log the old values of the columns to be updated. */

  if (update) {
    if (trx_undo_left(undo_page, ptr) < 5) {
      return 0;
    }

    ulint n_updated = upd_get_n_fields(update);

    /* If this is an online update while an inplace alter table
    is in progress and the table has virtual column, we will
    need to double check if there are any non-indexed columns
    being registered in update vector in case they will be indexed
    in new table */
    if (dict_index_is_online_ddl(index) && index->table->n_v_cols > 0) {
      for (i = 0; i < upd_get_n_fields(update); i++) {
        upd_field_t *fld = upd_get_nth_field(update, i);
        ulint pos = fld->field_no;

        /* These columns must not have an index
        on them */
        if (upd_fld_is_virtual_col(fld) &&
            dict_table_get_nth_v_col(table, pos)->v_indexes->empty()) {
          n_updated--;
        }
      }
    }

    ptr += mach_write_compressed(ptr, n_updated);

    for (i = 0; i < upd_get_n_fields(update); i++) {
      upd_field_t *fld = upd_get_nth_field(update, i);

      bool is_virtual = upd_fld_is_virtual_col(fld);
      bool is_multi_val = upd_fld_is_multi_value_col(fld);
      ulint max_v_log_len = 0;

      ulint pos = fld->field_no;

      /* Write field number to undo log */
      if (trx_undo_left(undo_page, ptr) < 5) {
        return 0;
      }

      if (is_virtual) {
        /* Skip the non-indexed column, during
        an online alter table */
        if (dict_index_is_online_ddl(index) &&
            dict_table_get_nth_v_col(table, pos)->v_indexes->empty()) {
          continue;
        }

        /* add REC_MAX_N_FIELDS to mark this
        is a virtual col */
        pos += REC_MAX_N_FIELDS;
      }

      if (index->has_row_versions() && !is_virtual) {
        /* Write physical position of field in UNDO */
        auto phy_pos = index->get_field(pos)->col->get_col_phy_pos();
        ut_ad(phy_pos == fld->field_phy_pos);
        ut_ad(!index->get_field(pos)->col->is_instant_dropped());
        ptr += mach_write_compressed(ptr, phy_pos);
      } else {
        ptr += mach_write_compressed(ptr, pos);
      }

      /* Save the old value of field */
      if (is_virtual) {
        ut_ad(fld->field_no < table->n_v_def);

        ptr = trx_undo_log_v_idx(undo_page, table, fld->field_no, ptr,
                                 first_v_col);
        if (ptr == nullptr) {
          return 0;
        }
        first_v_col = false;

        max_v_log_len = dict_max_v_field_len_store_undo(table, fld->field_no);

        field = static_cast<byte *>(fld->old_v_val->data);
        flen = fld->old_v_val->len;

        /* Only log sufficient bytes for index
        record update */
        if (flen != UNIV_SQL_NULL) {
          flen = std::min(flen, max_v_log_len);
        }
      } else {
        field = rec_get_nth_field_instant(rec, offsets, pos, index, &flen);
      }

      if (trx_undo_left(undo_page, ptr) < 15) {
        return 0;
      }

      if (!is_virtual && rec_offs_nth_extern(index, offsets, pos)) {
        ut_ad(!is_multi_val);
        const dict_col_t *col = index->get_col(pos);
        ulint prefix_len = dict_max_field_len_store_undo(table, col);

        ut_ad(prefix_len + BTR_EXTERN_FIELD_REF_SIZE <= sizeof ext_buf);

        ptr = trx_undo_page_report_modify_ext(
            trx, index, ptr,
            col->ord_part && !ignore_prefix &&
                    flen < REC_ANTELOPE_MAX_INDEX_COL_LEN
                ? ext_buf
                : nullptr,
            prefix_len, dict_table_page_size(table), &field, &flen,
            dict_table_is_sdi(table->id), SPATIAL_UNKNOWN);

        /* Notify purge that it eventually has to
        free the old externally stored field */

        undo_ptr->update_undo->del_marks = true;

        *type_cmpl_ptr |= TRX_UNDO_UPD_EXTERN;
      } else if (!is_multi_val) {
        ptr += mach_write_compressed(ptr, flen);
      }

      if (is_multi_val) {
        bool suc = trx_undo_store_multi_value(undo_page, fld->old_v_val, &ptr);
        if (!suc) {
          return 0;
        }
      } else if (flen != UNIV_SQL_NULL) {
        if (trx_undo_left(undo_page, ptr) < flen) {
          return 0;
        }

        ut_memcpy(ptr, field, flen);
        ptr += flen;

        if (!is_virtual && rec_offs_nth_extern(index, offsets, pos)) {
          ptr = trx_undo_report_blob_update(undo_page, index, ptr, field, flen,
                                            update, fld, mtr);

          if (ptr == nullptr) {
            return 0;
          }
        }
      }

      /* Also record the new value for virtual column */
      if (is_virtual) {
        field = static_cast<byte *>(fld->new_val.data);
        flen = fld->new_val.len;
        if (flen != UNIV_SQL_NULL) {
          flen = std::min(flen, max_v_log_len);
        }

        if (trx_undo_left(undo_page, ptr) < 15) {
          return 0;
        }

        if (is_multi_val) {
          bool suc = trx_undo_store_multi_value(undo_page, &fld->new_val, &ptr);
          if (!suc) {
            return 0;
          }
        } else {
          ptr += mach_write_compressed(ptr, flen);

          if (flen != UNIV_SQL_NULL) {
            if (trx_undo_left(undo_page, ptr) < flen) {
              return 0;
            }

            ut_memcpy(ptr, field, flen);
            ptr += flen;
          }
        }
      }
    }
  }

  /* Reset the first_v_col, so to put the virtual column undo
  version marker again, when we log all the indexed columns */
  first_v_col = true;

  /*----------------------------------------*/
  /* In the case of a delete marking, and also in the case of an update
  where any ordering field of any index changes, store the values of all
  columns which occur as ordering fields in any index. This info is used
  in the purge of old versions where we use it to build and search the
  delete marked index records, to look if we can remove them from the
  index tree. Note that starting from 4.0.14 also externally stored
  fields can be ordering in some index. Starting from 5.2, we no longer
  store REC_MAX_INDEX_COL_LEN first bytes to the undo log record,
  but we can construct the column prefix fields in the index by
  fetching the first page of the BLOB that is pointed to by the
  clustered index. This works also in crash recovery, because all pages
  (including BLOBs) are recovered before anything is rolled back. */

  if (!update || !(cmpl_info & UPD_NODE_NO_ORD_CHANGE)) {
    byte *old_ptr = ptr;
    double mbr[SPDIMS * 2];
    mem_heap_t *row_heap = nullptr;

    undo_ptr->update_undo->del_marks = true;

    if (trx_undo_left(undo_page, ptr) < 5) {
      return 0;
    }

    /* Reserve 2 bytes to write the number of bytes the stored
    fields take in this undo record */

    ptr += 2;

    for (col_no = 0; col_no < table->get_n_cols(); col_no++) {
      const dict_col_t *col = table->get_col(col_no);

      if (col->ord_part) {
        ulint pos;
        spatial_status_t spatial_status;

        spatial_status = SPATIAL_NONE;

        /* Write field number to undo log */
        if (trx_undo_left(undo_page, ptr) < 5 + 15) {
          return 0;
        }

        pos = index->get_col_pos(col_no);
        if (index->has_row_versions()) {
          /* Write physical position of field in UNDO */
          ut_ad(!col->is_virtual());
          ut_ad(!col->is_instant_dropped());

          auto phy_pos = col->get_col_phy_pos();
          ut_ad(phy_pos < REC_MAX_N_FIELDS);

          ptr += mach_write_compressed(ptr, phy_pos);
        } else {
          ptr += mach_write_compressed(ptr, pos);
        }

        /* Save the old value of field */
        field = rec_get_nth_field_instant(rec, offsets, pos, index, &flen);

        if (rec_offs_nth_extern(index, offsets, pos)) {
          const dict_col_t *col = index->get_col(pos);
          ulint prefix_len = dict_max_field_len_store_undo(table, col);

          ut_a(prefix_len < sizeof ext_buf);

          spatial_status = col->get_spatial_status();

          /* If there is a spatial index on it,
          log its MBR */
          if (spatial_status != SPATIAL_NONE) {
            ut_ad(DATA_GEOMETRY_MTYPE(col->mtype));

            trx_undo_get_mbr_from_ext(trx, index, mbr,
                                      dict_table_page_size(table), field, &flen,
                                      index->rtr_srs.get());
          }

          ptr = trx_undo_page_report_modify_ext(
              trx, index, ptr,
              flen < REC_ANTELOPE_MAX_INDEX_COL_LEN && !ignore_prefix ? ext_buf
                                                                      : nullptr,
              prefix_len, dict_table_page_size(table), &field, &flen,
              dict_table_is_sdi(table->id), spatial_status);
        } else {
          ptr += mach_write_compressed(ptr, flen);
        }

        if (flen != UNIV_SQL_NULL && spatial_status != SPATIAL_ONLY) {
          if (trx_undo_left(undo_page, ptr) < flen) {
            return 0;
          }

          ut_memcpy(ptr, field, flen);
          ptr += flen;
        }

        if (spatial_status != SPATIAL_NONE) {
          if (trx_undo_left(undo_page, ptr) < DATA_MBR_LEN) {
            return 0;
          }

          for (uint i = 0; i < SPDIMS * 2; i++) {
            mach_double_write(ptr, mbr[i]);
            ptr += sizeof(double);
          }
        }
      }
    }

    for (col_no = 0; col_no < dict_table_get_n_v_cols(table); col_no++) {
      dfield_t *vfield = nullptr;

      const dict_v_col_t *col = dict_table_get_nth_v_col(table, col_no);

      if (col->m_col.ord_part) {
        ulint pos = col_no;
        ulint max_v_log_len = dict_max_v_field_len_store_undo(table, pos);

        /* Write field number to undo log.
        Make sure there is enough space in log */
        if (trx_undo_left(undo_page, ptr) < 5) {
          return 0;
        }

        pos += REC_MAX_N_FIELDS;
        ptr += mach_write_compressed(ptr, pos);

        ut_ad(col_no < table->n_v_def);
        ptr = trx_undo_log_v_idx(undo_page, table, col_no, ptr, first_v_col);
        first_v_col = false;

        if (!ptr) {
          return 0;
        }

        if (update) {
          ut_ad(!row);
          if (update->old_vrow == nullptr) {
            flen = UNIV_SQL_NULL;
          } else {
            vfield = dtuple_get_nth_v_field(update->old_vrow, col->v_pos);
          }
        } else if (row) {
          vfield = dtuple_get_nth_v_field(row, col->v_pos);
        } else {
          ut_d(ut_error);
        }

        if (vfield) {
          field = static_cast<byte *>(vfield->data);
          flen = vfield->len;
        } else {
          ut_ad(flen == UNIV_SQL_NULL);
        }

        /* Prepare to write the field length and field data */
        if (flen != UNIV_SQL_NULL) {
          flen = std::min(flen, max_v_log_len);

          if (trx_undo_left(undo_page, ptr) < 5 + flen) {
            return 0;
          }
        } else if (trx_undo_left(undo_page, ptr) < 5) {
          return 0;
        }

        if (col->m_col.is_multi_value()) {
          bool suc = trx_undo_store_multi_value(undo_page, vfield, &ptr);
          if (!suc) {
            return 0;
          }
        } else {
          ptr += mach_write_compressed(ptr, flen);

          if (flen != UNIV_SQL_NULL) {
            ut_memcpy(ptr, field, flen);
            ptr += flen;
          }
        }
      }
    }

    mach_write_to_2(old_ptr, ptr - old_ptr);

    if (row_heap) {
      mem_heap_free(row_heap);
    }
  }

  /*----------------------------------------*/
  /* Write pointers to the previous and the next undo log records */
  if (trx_undo_left(undo_page, ptr) < 2) {
    return 0;
  }

  mach_write_to_2(ptr, first_free);
  ptr += 2;
  mach_write_to_2(undo_page + first_free, ptr - undo_page);

  mach_write_to_2(undo_page + TRX_UNDO_PAGE_HDR + TRX_UNDO_PAGE_FREE,
                  ptr - undo_page);

  /* Write to the REDO log about this change in the UNDO log */

  trx_undof_page_add_undo_rec_log(undo_page, first_free, ptr - undo_page, mtr);
  return first_free;
}

/** Reads from an undo log update record the system field values of the old
 version.
 @return remaining part of undo log record after reading these values */
byte *trx_undo_update_rec_get_sys_cols(
    const byte *ptr,      /*!< in: remaining part of undo
                          log record after reading
                          general parameters */
    trx_id_t *trx_id,     /*!< out: trx id */
    roll_ptr_t *roll_ptr, /*!< out: roll ptr */
    ulint *info_bits)     /*!< out: info bits state */
{
  /* Read the state of the info bits */
  *info_bits = mach_read_from_1(ptr);
  ptr += 1;

  /* Read the values of the system columns */

  *trx_id = mach_u64_read_next_compressed(&ptr);
  *roll_ptr = mach_u64_read_next_compressed(&ptr);

  return (const_cast<byte *>(ptr));
}

byte *trx_undo_update_rec_get_update(const byte *ptr, const dict_index_t *index,
                                     ulint type, trx_id_t trx_id,
                                     roll_ptr_t roll_ptr, ulint info_bits,
                                     mem_heap_t *heap, upd_t **upd,
                                     lob::undo_vers_t *lob_undo,
                                     type_cmpl_t &type_cmpl) {
  DBUG_TRACE;

  upd_field_t *upd_field;
  upd_t *update;
  ulint n_fields;
  byte *buf;
  ulint i;
  bool first_v_col = true;
  bool is_undo_log = true;
  ulint n_skip_field = 0;

  ut_a(index->is_clustered());

  if (type != TRX_UNDO_DEL_MARK_REC) {
    n_fields = mach_read_next_compressed(&ptr);
  } else {
    n_fields = 0;
  }

  update = upd_create(n_fields + 2, heap);

  update->table = index->table;

  update->info_bits = info_bits;

  /* Store first trx id and roll ptr to update vector */

  upd_field = upd_get_nth_field(update, n_fields);

  buf = static_cast<byte *>(mem_heap_alloc(heap, DATA_TRX_ID_LEN));

  trx_write_trx_id(buf, trx_id);

  upd_field_set_field_no(upd_field, index->get_sys_col_pos(DATA_TRX_ID), index);
  dfield_set_data(&(upd_field->new_val), buf, DATA_TRX_ID_LEN);

  upd_field = upd_get_nth_field(update, n_fields + 1);

  buf = static_cast<byte *>(mem_heap_alloc(heap, DATA_ROLL_PTR_LEN));

  trx_write_roll_ptr(buf, roll_ptr);

  upd_field_set_field_no(upd_field, index->get_sys_col_pos(DATA_ROLL_PTR),
                         index);
  dfield_set_data(&(upd_field->new_val), buf, DATA_ROLL_PTR_LEN);

  /* Store then the updated ordinary columns to the update vector */

  for (i = 0; i < n_fields; i++) {
    const byte *field;
    ulint len;
    ulint field_no;
    ulint orig_len;
    bool is_virtual;
    dict_v_col_t *vcol = nullptr;

    field_no = mach_read_next_compressed(&ptr);

    is_virtual = (field_no >= REC_MAX_N_FIELDS);

    if (is_virtual) {
      /* If new version, we need to check index list to figure
      out the correct virtual column position */
      ptr = trx_undo_read_v_idx(index->table, ptr, first_v_col, &is_undo_log,
                                &field_no);
      first_v_col = false;
    } else if (field_no >= dict_index_get_n_fields(index)) {
      ib::error(ER_IB_MSG_1184)
          << "Trying to access update undo rec"
             " field "
          << field_no << " in index " << index->name << " of table "
          << index->table->name << " but index has only "
          << dict_index_get_n_fields(index) << " fields " << BUG_REPORT_MSG
          << ". Run also CHECK TABLE " << index->table->name
          << "."
             " n_fields = "
          << n_fields << ", i = " << i << ", ptr " << ptr;

      ut_d(ut_error);
      ut_o(*upd = nullptr);
      ut_o(return nullptr);
    }

    upd_field = upd_get_nth_field(update, i);

    if (is_virtual) {
      /* This column could be dropped or no longer indexed */
      if (field_no == ULINT_UNDEFINED) {
        /* Mark this is no longer needed */
        upd_field->field_no = REC_MAX_N_FIELDS;

        if (trx_undo_rec_is_multi_value(ptr)) {
          ptr = trx_undo_rec_get_multi_value(ptr, nullptr, heap);
          ut_ad(trx_undo_rec_is_multi_value(ptr));
          ptr = trx_undo_rec_get_multi_value(ptr, nullptr, heap);
        } else {
          ptr = trx_undo_rec_get_col_val(ptr, &field, &len, &orig_len);
          ptr = trx_undo_rec_get_col_val(ptr, &field, &len, &orig_len);
        }
        n_skip_field++;
        continue;
      } else {
        vcol = dict_table_get_nth_v_col(index->table, field_no);
      }

      upd_field_set_v_field_no(upd_field, field_no, index);
    } else {
      if (index->has_row_versions()) {
        auto log_pos = index->fields_array[field_no];
        upd_field_set_field_no(upd_field, log_pos, index);
        IF_DEBUG(upd_field->field_phy_pos = field_no;)
      } else {
        upd_field_set_field_no(upd_field, field_no, index);
      }
    }

    if (vcol != nullptr && vcol->m_col.is_multi_value()) {
      ptr = trx_undo_rec_get_multi_value(ptr, &upd_field->new_val, heap);
    } else {
      ptr = trx_undo_rec_get_col_val(ptr, &field, &len, &orig_len);

      upd_field->orig_len = orig_len;

      if (len == UNIV_SQL_NULL) {
        dfield_set_null(&upd_field->new_val);
      } else if (len < UNIV_EXTERN_STORAGE_FIELD) {
        dfield_set_data(&upd_field->new_val, field, len);
      } else {
        len -= UNIV_EXTERN_STORAGE_FIELD;

        dfield_set_data(&upd_field->new_val, field, len);
        dfield_set_ext(&upd_field->new_val);

        if (type_cmpl.is_lob_undo() && type_cmpl.is_lob_updated()) {
          /* Read the partial update on LOB */
          ptr = trx_undo_read_blob_update(ptr, upd_field, lob_undo);
        }
      }
    }

    if (is_virtual) {
      upd_field->old_v_val = static_cast<dfield_t *>(
          mem_heap_zalloc(heap, sizeof *upd_field->old_v_val));

      if (vcol != nullptr && vcol->m_col.is_multi_value()) {
        ptr = trx_undo_rec_get_multi_value(ptr, upd_field->old_v_val, heap);
      } else {
        ptr = trx_undo_rec_get_col_val(ptr, &field, &len, &orig_len);
        if (len == UNIV_SQL_NULL) {
          dfield_set_null(upd_field->old_v_val);
        } else if (len < UNIV_EXTERN_STORAGE_FIELD) {
          dfield_set_data(upd_field->old_v_val, field, len);
        } else {
          ut_d(ut_error);
        }
      }
    }
  }

  /* In rare scenario, we could have skipped virtual column (as they
  are dropped. We will regenerate a update vector and skip them */
  if (n_skip_field > 0) {
    ulint n = 0;
    ut_ad(n_skip_field <= n_fields);

    upd_t *new_update = upd_create(n_fields + 2 - n_skip_field, heap);

    for (i = 0; i < n_fields + 2; i++) {
      upd_field = upd_get_nth_field(update, i);

      if (upd_field->field_no == REC_MAX_N_FIELDS) {
        continue;
      }

      upd_field_t *new_upd_field = upd_get_nth_field(new_update, n);
      *new_upd_field = *upd_field;
      n++;
    }
    ut_ad(n == n_fields + 2 - n_skip_field);
    *upd = new_update;
  } else {
    *upd = update;
  }

  return const_cast<byte *>(ptr);
}

/** Builds a partial row from an update undo log record, for purge.
 It contains the columns which occur as ordering in any index of the table.
 Any missing columns are indicated by col->mtype == DATA_MISSING.
 @return pointer to remaining part of undo record */
byte *trx_undo_rec_get_partial_row(
    const byte *ptr,     /*!< in: remaining part in update undo log
                         record of a suitable type, at the start of
                         the stored index columns;
                         NOTE that this copy of the undo log record must
                         be preserved as long as the partial row is
                         used, as we do NOT copy the data in the
                         record! */
    dict_index_t *index, /*!< in: clustered index */
    dtuple_t **row,      /*!< out, own: partial row */
    bool ignore_prefix,  /*!< in: flag to indicate if we
                   expect blob prefixes in undo. Used
                   only in the assertion. */
    mem_heap_t *heap)    /*!< in: memory heap from which the memory
                         needed is allocated */
{
  const byte *end_ptr;
  bool first_v_col = true;
  bool is_undo_log = true;

  ut_ad(index);
  ut_ad(ptr);
  ut_ad(row);
  ut_ad(heap);
  ut_ad(index->is_clustered());

  *row = dtuple_create_with_vcol(heap, index->table->get_n_cols(),
                                 dict_table_get_n_v_cols(index->table));

  /* Mark all columns in the row uninitialized, so that
  we can distinguish missing fields from fields that are SQL NULL. */
  for (ulint i = 0; i < index->table->get_n_cols(); i++) {
    dfield_get_type(dtuple_get_nth_field(*row, i))->mtype = DATA_MISSING;
    /* In case a multi-value field checking read uninitialized value */
    dfield_get_type(dtuple_get_nth_field(*row, i))->prtype = 0;
  }

  dtuple_init_v_fld(*row);

  end_ptr = ptr + mach_read_from_2(ptr);
  ptr += 2;

  while (ptr != end_ptr) {
    dfield_t *dfield = nullptr;
    const byte *field;
    ulint field_no = ULINT_UNDEFINED;
    const dict_col_t *col = nullptr;
    ulint col_no;
    ulint len;
    ulint orig_len;
    bool is_virtual;
    dict_v_col_t *vcol = nullptr;

    field_no = mach_read_next_compressed(&ptr);

    is_virtual = (field_no >= REC_MAX_N_FIELDS);

    if (is_virtual) {
      ptr = trx_undo_read_v_idx(index->table, ptr, first_v_col, &is_undo_log,
                                &field_no);
      first_v_col = false;
      if (field_no != ULINT_UNDEFINED) {
        vcol = dict_table_get_nth_v_col(index->table, field_no);
        col = &vcol->m_col;
        col_no = dict_col_get_no(col);
        dfield = dtuple_get_nth_v_field(*row, vcol->v_pos);
        vcol->m_col.copy_type(dfield_get_type(dfield));
      }
    }

    if ((vcol != nullptr && vcol->m_col.is_multi_value()) ||
        trx_undo_rec_is_multi_value(ptr)) {
      ut_ad(is_virtual);
      ut_ad(vcol != nullptr || field_no == ULINT_UNDEFINED);
      ut_ad(dfield != nullptr || field_no == ULINT_UNDEFINED);
      ptr = trx_undo_rec_get_multi_value(ptr, dfield, heap);
      continue;
    } else {
      ptr = trx_undo_rec_get_col_val(ptr, &field, &len, &orig_len);
    }

    /* This column could be dropped or no longer indexed */
    if (field_no == ULINT_UNDEFINED) {
      ut_ad(is_virtual);
      continue;
    }

    if (!is_virtual) {
      if (index->has_row_versions()) {
        /* This field_no is physical pos */
        col = index->get_physical_field(field_no)->col;
      } else {
        col = index->get_col(field_no);
      }

      /* This column shouldn't be dropped unless index on this column is
      dropped. */
      ut_ad(!col->is_instant_dropped() || !col->ord_part);
      if (col->is_instant_dropped()) {
        continue;
      }
      col_no = dict_col_get_no(col);
      dfield = dtuple_get_nth_field(*row, col_no);
      index->table->get_col(col_no)->copy_type(dfield_get_type(dfield));
    }

    dfield_set_data(dfield, field, len);

    if (len != UNIV_SQL_NULL && len >= UNIV_EXTERN_STORAGE_FIELD) {
      spatial_status_t spatial_status;

      /* Decode spatial status. */
      spatial_status = static_cast<spatial_status_t>(
          (len & SPATIAL_STATUS_MASK) >> SPATIAL_STATUS_SHIFT);
      len &= ~SPATIAL_STATUS_MASK;

      /* Keep compatible with 5.7.9 format. */
      if (spatial_status == SPATIAL_UNKNOWN) {
        spatial_status = col->get_spatial_status();
      }

      switch (spatial_status) {
        case SPATIAL_ONLY:
          ut_ad(len - UNIV_EXTERN_STORAGE_FIELD == DATA_MBR_LEN);
          dfield_set_len(dfield, len - UNIV_EXTERN_STORAGE_FIELD);
          break;

        case SPATIAL_MIXED:
          dfield_set_len(dfield,
                         len - UNIV_EXTERN_STORAGE_FIELD - DATA_MBR_LEN);
          break;

        case SPATIAL_NONE:
          dfield_set_len(dfield, len - UNIV_EXTERN_STORAGE_FIELD);
          break;

        case SPATIAL_UNKNOWN:
          ut_d(ut_error);
          ut_o(break);
      }

      dfield_set_ext(dfield);
      dfield_set_spatial_status(dfield, spatial_status);

      /* If the prefix of this column is indexed,
      ensure that enough prefix is stored in the
      undo log record. */
      if (!ignore_prefix && col->ord_part && spatial_status != SPATIAL_ONLY) {
        ut_a(dfield_get_len(dfield) >= BTR_EXTERN_FIELD_REF_SIZE);
        ut_a(dict_table_has_atomic_blobs(index->table) ||
             dfield_get_len(dfield) >=
                 REC_ANTELOPE_MAX_INDEX_COL_LEN + BTR_EXTERN_FIELD_REF_SIZE);
      }
    }
  }

  return (const_cast<byte *>(ptr));
}
#endif /* !UNIV_HOTBACKUP */

/** Erases the unused undo log page end.
 @return true if the page contained something, false if it was empty */
static bool trx_undo_erase_page_end(
    page_t *undo_page, /*!< in/out: undo page whose end to erase */
    mtr_t *mtr)        /*!< in/out: mini-transaction */
{
  ulint first_free;

  first_free =
      mach_read_from_2(undo_page + TRX_UNDO_PAGE_HDR + TRX_UNDO_PAGE_FREE);
  memset(undo_page + first_free, 0xff,
         (UNIV_PAGE_SIZE - FIL_PAGE_DATA_END) - first_free);

  mlog_write_initial_log_record(undo_page, MLOG_UNDO_ERASE_END, mtr);
  return (first_free != TRX_UNDO_PAGE_HDR + TRX_UNDO_PAGE_HDR_SIZE);
}

byte *trx_undo_parse_erase_page_end(byte *ptr, byte *end_ptr [[maybe_unused]],
                                    page_t *page, mtr_t *mtr) {
  if (page == nullptr) {
    return (ptr);
  }

  trx_undo_erase_page_end(page, mtr);

  return (ptr);
}

#ifndef UNIV_HOTBACKUP
/** Writes information to an undo log about an insert, update, or a delete
 marking of a clustered index record. This information is used in a rollback of
 the transaction and in consistent reads that must look to the history of this
 transaction.
 @return DB_SUCCESS or error code */
dberr_t trx_undo_report_row_operation(
    ulint flags,                 /*!< in: if BTR_NO_UNDO_LOG_FLAG bit is
                                 set, does nothing */
    ulint op_type,               /*!< in: TRX_UNDO_INSERT_OP or
                                 TRX_UNDO_MODIFY_OP */
    que_thr_t *thr,              /*!< in: query thread */
    dict_index_t *index,         /*!< in: clustered index */
    const dtuple_t *clust_entry, /*!< in: in the case of an insert,
                                 index entry to insert into the
                                 clustered index, otherwise NULL */
    const upd_t *update,         /*!< in: in the case of an update,
                                 the update vector, otherwise NULL */
    ulint cmpl_info,             /*!< in: compiler info on secondary
                                 index updates */
    const rec_t *rec,            /*!< in: in case of an update or delete
                                 marking, the record in the clustered
                                 index, otherwise NULL */
    const ulint *offsets,        /*!< in: rec_get_offsets(rec) */
    roll_ptr_t *roll_ptr)        /*!< out: rollback pointer to the
                                 inserted undo log record,
                                 0 if BTR_NO_UNDO_LOG
                                 flag was specified */
{
  trx_t *trx;
  trx_undo_t *undo;
  page_no_t page_no;
  buf_block_t *undo_block;
  trx_undo_ptr_t *undo_ptr;
  mtr_t mtr;
  dberr_t err = DB_SUCCESS;
#ifdef UNIV_DEBUG
  int loop_count = 0;
#endif /* UNIV_DEBUG */

  ut_a(index->is_clustered());
  ut_ad(!rec || rec_offs_validate(rec, index, offsets));

  if (flags & BTR_NO_UNDO_LOG_FLAG) {
    *roll_ptr = 0;

    return (DB_SUCCESS);
  }

  ut_ad(thr);
  ut_ad(!srv_read_only_mode);
  ut_ad((op_type != TRX_UNDO_INSERT_OP) || (clust_entry && !update && !rec));

  trx = thr_get_trx(thr);

  bool is_temp_table = index->table->is_temporary();

  /* Temporary tables do not go into INFORMATION_SCHEMA.TABLES,
  so do not bother adding it to the list of modified tables by
  the transaction - this list is only used for maintaining
  INFORMATION_SCHEMA.TABLES.UPDATE_TIME. */
  if (!is_temp_table) {
    trx->mod_tables.insert(index->table);
  }

  /* If trx is read-only then only temp-tables can be written. */
  ut_ad(!trx->read_only || is_temp_table);

  /* If this is a temp-table then we assign temporary rseg. */
  if (is_temp_table && trx->rsegs.m_noredo.rseg == nullptr) {
    trx_assign_rseg_temp(trx);
  }

  mtr_start(&mtr);

  if (is_temp_table) {
    /* If object is temporary, disable REDO logging that
    is done to track changes done to UNDO logs. This is
    feasible given that temporary tables and temporary
    undo logs are not restored on restart. */
    undo_ptr = &trx->rsegs.m_noredo;
    mtr.set_log_mode(MTR_LOG_NO_REDO);
  } else {
    undo_ptr = &trx->rsegs.m_redo;
  }

  mutex_enter(&trx->undo_mutex);

#ifdef UNIV_DEBUG
  if (srv_inject_too_many_concurrent_trxs) {
    err = DB_TOO_MANY_CONCURRENT_TRXS;
    goto err_exit;
  }
#endif /* UNIV_DEBUG */

  switch (op_type) {
    case TRX_UNDO_INSERT_OP:
      undo = undo_ptr->insert_undo;

      if (undo == nullptr) {
        err = trx_undo_assign_undo(trx, undo_ptr, TRX_UNDO_INSERT);
        undo = undo_ptr->insert_undo;

        if (undo == nullptr) {
          /* Did not succeed */
          ut_ad(err != DB_SUCCESS);
          goto err_exit;
        }
      }

      ut_ad(err == DB_SUCCESS);
      break;
    default:
      ut_ad(op_type == TRX_UNDO_MODIFY_OP);

      undo = undo_ptr->update_undo;

      if (undo == nullptr) {
        err = trx_undo_assign_undo(trx, undo_ptr, TRX_UNDO_UPDATE);
        undo = undo_ptr->update_undo;

        if (undo == nullptr) {
          /* Did not succeed */
          ut_ad(err != DB_SUCCESS);
          goto err_exit;
        }
      }

      ut_ad(err == DB_SUCCESS);
      break;
  }

  page_no = undo->last_page_no;
  undo_block = buf_page_get_gen(page_id_t(undo->space, page_no),
                                undo->page_size, RW_X_LATCH, undo->guess_block,
                                Page_fetch::NORMAL, UT_LOCATION_HERE, &mtr);

  buf_block_dbg_add_level(undo_block, SYNC_TRX_UNDO_PAGE);

  do {
    page_t *undo_page;
    ulint offset;

    undo_page = buf_block_get_frame(undo_block);
    ut_ad(page_no == undo_block->page.id.page_no());

    switch (op_type) {
      case TRX_UNDO_INSERT_OP:
        offset = trx_undo_page_report_insert(undo_page, trx, index, clust_entry,
                                             &mtr);
        break;
      default:
        ut_ad(op_type == TRX_UNDO_MODIFY_OP);
        offset =
            trx_undo_page_report_modify(undo_page, trx, index, rec, offsets,
                                        update, cmpl_info, clust_entry, &mtr);
    }

    if (UNIV_UNLIKELY(offset == 0)) {
      /* The record did not fit on the page. We erase the
      end segment of the undo log page and write a log
      record of it: this is to ensure that in the debug
      version the replicate page constructed using the log
      records stays identical to the original page */

      if (!trx_undo_erase_page_end(undo_page, &mtr)) {
        /* The record did not fit on an empty
        undo page. Discard the freshly allocated
        page and return an error. */

        /* When we remove a page from an undo
        log, this is analogous to a
        pessimistic insert in a B-tree, and we
        must reserve the counterpart of the
        tree latch, which is the rseg
        mutex. We must commit the mini-transaction
        first, because it may be holding lower-level
        latches, such as SYNC_FSP and SYNC_FSP_PAGE. */

        mtr_commit(&mtr);
        mtr_start(&mtr);

        if (index->table->is_temporary()) {
          mtr.set_log_mode(MTR_LOG_NO_REDO);
        }

        undo_ptr->rseg->latch();
        trx_undo_free_last_page(trx, undo, &mtr);
        undo_ptr->rseg->unlatch();

        err = DB_UNDO_RECORD_TOO_BIG;
        goto err_exit;
      }

      mtr_commit(&mtr);
    } else {
      /* Success */
      undo->guess_block = undo_block;
      mtr_commit(&mtr);

      undo->empty = false;
      undo->top_page_no = page_no;
      undo->top_offset = offset;
      undo->top_undo_no = trx->undo_no;

      trx->undo_no++;
      trx->undo_rseg_space = undo_ptr->rseg->space_id;

      mutex_exit(&trx->undo_mutex);

      *roll_ptr =
          trx_undo_build_roll_ptr(op_type == TRX_UNDO_INSERT_OP,
                                  undo_ptr->rseg->space_id, page_no, offset);
      return (DB_SUCCESS);
    }

    ut_ad(page_no == undo->last_page_no);

    /* We have to extend the undo log by one page */

    ut_ad(++loop_count < 2);

    mtr_start(&mtr);

    if (index->table->is_temporary()) {
      mtr.set_log_mode(MTR_LOG_NO_REDO);
    }

    /* When we add a page to an undo log, this is analogous to
    a pessimistic insert in a B-tree, and we must reserve the
    counterpart of the tree latch, which is the rseg mutex. */

    undo_ptr->rseg->latch();
    undo_block = trx_undo_add_page(trx, undo, undo_ptr, &mtr);
    undo_ptr->rseg->unlatch();

    page_no = undo->last_page_no;

    DBUG_EXECUTE_IF("ib_err_ins_undo_page_add_failure", undo_block = nullptr;);
  } while (undo_block != nullptr);

  ib_errf(
      trx->mysql_thd, IB_LOG_LEVEL_ERROR, ER_INNODB_UNDO_LOG_FULL,
      "No more space left over in %s tablespace for allocating UNDO"
      " log pages. Please add new data file to the tablespace or"
      " check if filesystem is full or enable auto-extension for"
      " the tablespace",
      ((undo->space == TRX_SYS_SPACE)
           ? "system"
           : ((fsp_is_system_temporary(undo->space)) ? "temporary" : "undo")));

  /* Did not succeed: out of space */
  err = DB_OUT_OF_FILE_SPACE;

err_exit:
  mutex_exit(&trx->undo_mutex);
  mtr_commit(&mtr);
  return (err);
}

/*============== BUILDING PREVIOUS VERSION OF A RECORD ===============*/

/** Copies an undo record to heap. This function can be called if we know that
 the undo log record exists.
 @return own: copy of the record */
[[nodiscard]] static trx_undo_rec_t *trx_undo_get_undo_rec_low(
    roll_ptr_t roll_ptr, /*!< in: roll pointer to record */
    mem_heap_t *heap,    /*!< in: memory heap where copied */
    bool is_temp)        /*!< in: true if temp undo rec. */
{
  trx_undo_rec_t *undo_rec;
  ulint rseg_id;
  space_id_t space_id;
  page_no_t page_no;
  ulint offset;
  const page_t *undo_page;
  bool is_insert;
  mtr_t mtr;

  trx_undo_decode_roll_ptr(roll_ptr, &is_insert, &rseg_id, &page_no, &offset);
  space_id = trx_rseg_id_to_space_id(rseg_id, is_temp);

  bool found;
  const page_size_t &page_size = fil_space_get_page_size(space_id, &found);
  ut_ad(found);

  mtr_start(&mtr);

  undo_page = trx_undo_page_get_s_latched(page_id_t(space_id, page_no),
                                          page_size, &mtr);

  undo_rec = trx_undo_rec_copy(undo_page, static_cast<uint32_t>(offset), heap);

  mtr_commit(&mtr);

  return (undo_rec);
}

/** Copies an undo record to heap.
 @param[in]     roll_ptr        roll pointer to record
 @param[in]     trx_id          id of the trx that generated
                                 the roll pointer: it points to an
                                 undo log of this transaction
 @param[in]     heap            memory heap where copied
 @param[in]     is_temp         true if temporary, no-redo rseg.
 @param[in]     name            table name
 @param[out]    undo_rec        own: copy of the record
 @retval true if the undo log has been
 truncated and we cannot fetch the old version
 @retval false if the undo log record is available
 NOTE: the caller must have latches on the clustered index page. */
[[nodiscard]] static bool trx_undo_get_undo_rec(roll_ptr_t roll_ptr,
                                                trx_id_t trx_id,
                                                mem_heap_t *heap, bool is_temp,
                                                const table_name_t &name,
                                                trx_undo_rec_t **undo_rec) {
  bool missing_history;

  rw_lock_s_lock(&purge_sys->latch, UT_LOCATION_HERE);

  missing_history = purge_sys->view.changes_visible(trx_id, name);
  if (!missing_history) {
    *undo_rec = trx_undo_get_undo_rec_low(roll_ptr, heap, is_temp);
  }

  rw_lock_s_unlock(&purge_sys->latch);

  return (missing_history);
}

#ifdef UNIV_DEBUG
#define ATTRIB_USED_ONLY_IN_DEBUG
#else /* UNIV_DEBUG */
#define ATTRIB_USED_ONLY_IN_DEBUG [[maybe_unused]]
#endif /* UNIV_DEBUG */

bool trx_undo_prev_version_build(
    const rec_t *index_rec ATTRIB_USED_ONLY_IN_DEBUG,
    mtr_t *index_mtr ATTRIB_USED_ONLY_IN_DEBUG, const rec_t *rec,
    const dict_index_t *const index, ulint *offsets, mem_heap_t *heap,
    rec_t **old_vers, mem_heap_t *v_heap, const dtuple_t **vrow, ulint v_status,
    lob::undo_vers_t *lob_undo) {
  DBUG_TRACE;

  trx_undo_rec_t *undo_rec = nullptr;
  dtuple_t *entry;
  trx_id_t rec_trx_id;
  ulint type;
  undo_no_t undo_no;
  table_id_t table_id;
  trx_id_t trx_id;
  roll_ptr_t roll_ptr;
  upd_t *update = nullptr;
  byte *ptr;
  ulint info_bits;
  ulint cmpl_info;
  bool dummy_extern;
  byte *buf;

  ut_ad(!rw_lock_own(&purge_sys->latch, RW_LOCK_S));
  ut_ad(mtr_memo_contains_page(index_mtr, index_rec, MTR_MEMO_PAGE_S_FIX) ||
        mtr_memo_contains_page(index_mtr, index_rec, MTR_MEMO_PAGE_X_FIX));
  ut_ad(rec_offs_validate(rec, index, offsets));
  ut_a(index->is_clustered());

  roll_ptr = row_get_rec_roll_ptr(rec, index, offsets);

  *old_vers = nullptr;

  if (trx_undo_roll_ptr_is_insert(roll_ptr)) {
    /* The record rec is the first inserted version */
    return true;
  }

  rec_trx_id = row_get_rec_trx_id(rec, index, offsets);

  /* REDO rollback segments are used only for non-temporary objects.
  For temporary objects NON-REDO rollback segments are used. */
  bool is_temp = index->table->is_temporary();

  ut_ad(!index->table->skip_alter_undo);

  if (trx_undo_get_undo_rec(roll_ptr, rec_trx_id, heap, is_temp,
                            index->table->name, &undo_rec)) {
    if (v_status & TRX_UNDO_PREV_IN_PURGE) {
      /* We are fetching the record being purged */
      undo_rec = trx_undo_get_undo_rec_low(roll_ptr, heap, is_temp);
    } else {
      /* The undo record may already have been purged,
      during purge or semi-consistent read. */
      return false;
    }
  }

  type_cmpl_t type_cmpl;
  ptr = trx_undo_rec_get_pars(undo_rec, &type, &cmpl_info, &dummy_extern,
                              &undo_no, &table_id, type_cmpl);

  if (table_id != index->table->id) {
    /* The table should have been rebuilt, but purge has
    not yet removed the undo log records for the
    now-dropped old table (table_id). */
    return true;
  }

  ptr = trx_undo_update_rec_get_sys_cols(ptr, &trx_id, &roll_ptr, &info_bits);

  /* (a) If a clustered index record version is such that the
  trx id stamp in it is bigger than purge_sys->view, then the
  BLOBs in that version are known to exist (the purge has not
  progressed that far);

  (b) if the version is the first version such that trx id in it
  is less than purge_sys->view, and it is not delete-marked,
  then the BLOBs in that version are known to exist (the purge
  cannot have purged the BLOBs referenced by that version
  yet).

  This function does not fetch any BLOBs.  The callers might, by
  possibly invoking row_ext_create() via row_build().  However,
  they should have all needed information in the *old_vers
  returned by this function.  This is because *old_vers is based
  on the transaction undo log records.  The function
  trx_undo_page_fetch_ext() will write BLOB prefixes to the
  transaction undo log that are at least as long as the longest
  possible column prefix in a secondary index.  Thus, secondary
  index entries for *old_vers can be constructed without
  dereferencing any BLOB pointers. */

  ptr = trx_undo_rec_skip_row_ref(ptr, index);

  ptr = trx_undo_update_rec_get_update(ptr, index, type, trx_id, roll_ptr,
                                       info_bits, heap, &update, lob_undo,
                                       type_cmpl);
  ut_a(ptr);

  if (row_upd_changes_field_size_or_external(index, offsets, update)) {
    /* We should confirm the existence of disowned external data,
    if the previous version record is delete marked. If the trx_id
    of the previous record is seen by purge view, we should treat
    it as missing history, because the disowned external data
    might be purged already.

    The inherited external data (BLOBs) can be freed (purged)
    after trx_id was committed, provided that no view was started
    before trx_id. If the purge view can see the committed
    delete-marked record by trx_id, no transactions need to access
    the BLOB. */

    /* the row_upd_changes_disowned_external(update) call could be
    omitted, but the synchronization on purge_sys->latch is likely
    more expensive. */

    if ((update->info_bits & REC_INFO_DELETED_FLAG) &&
        row_upd_changes_disowned_external(update)) {
      bool missing_extern;

      rw_lock_s_lock(&purge_sys->latch, UT_LOCATION_HERE);

      missing_extern =
          purge_sys->view.changes_visible(trx_id, index->table->name);

      rw_lock_s_unlock(&purge_sys->latch);

      if (missing_extern) {
        /* treat as a fresh insert, not to
        cause assertion error at the caller. */
        if (update != nullptr) {
          update->reset();
        }
        return true;
      }
    }

    /* We have to set the appropriate extern storage bits in the
    old version of the record: the extern bits in rec for those
    fields that update does NOT update, as well as the bits for
    those fields that update updates to become externally stored
    fields. Store the info: */

    entry = row_rec_to_index_entry(rec, index, offsets, heap);
    /* The page containing the clustered index record
    corresponding to entry is latched in mtr.  Thus the
    following call is safe. */
    row_upd_index_replace_new_col_vals(entry, index, update, heap);

    buf = static_cast<byte *>(
        mem_heap_alloc(heap, rec_get_converted_size(index, entry)));

    *old_vers = rec_convert_dtuple_to_rec(buf, index, entry);
  } else {
    buf = static_cast<byte *>(mem_heap_alloc(heap, rec_offs_size(offsets)));

    *old_vers = rec_copy(buf, rec, offsets);
    rec_offs_make_valid(*old_vers, index, offsets);
    row_upd_rec_in_place(*old_vers, index, offsets, update, nullptr);
  }

  /* Set the old value (which is the after image of an update) in the
  update vector to dtuple vrow */
  if (v_status & TRX_UNDO_GET_OLD_V_VALUE) {
    row_upd_replace_vcol((dtuple_t *)*vrow, index->table, update, false,
                         nullptr, nullptr);
  }

#if defined UNIV_DEBUG || defined UNIV_BLOB_LIGHT_DEBUG
  ut_a(!rec_offs_any_null_extern(
      index, *old_vers,
      rec_get_offsets(*old_vers, index, nullptr, ULINT_UNDEFINED,
                      UT_LOCATION_HERE, &heap)));
#endif  // defined UNIV_DEBUG || defined UNIV_BLOB_LIGHT_DEBUG

  /* If vrow is not NULL it means that the caller is interested in the values of
  the virtual columns for this version.
  If the UPD_NODE_NO_ORD_CHANGE flag is set on cmpl_info, it means that the
  change which created this entry in undo log did not affect any column of any
  secondary index (in particular: virtual), and thus the values of virtual
  columns were not recorded in undo. In such case the caller may assume that the
  values of (virtual) columns present in secondary index are exactly the same as
  they are in the next (more recent) version.
  If on the other hand the UPD_NODE_NO_ORD_CHANGE flag is not set, then we will
  make sure that *vrow points to a properly allocated memory and contains the
  values of virtual columns for this version recovered from undo log.
  This implies that if the caller has provided a non-NULL vrow, and the *vrow is
  still NULL after the call, (and old_vers is not NULL) it must be because the
  UPD_NODE_NO_ORD_CHANGE flag was set for this version.
  This last statement is an important assumption made by the
  row_vers_impl_x_locked_low() function. */
  if (vrow && !(cmpl_info & UPD_NODE_NO_ORD_CHANGE)) {
    if (!(*vrow)) {
      *vrow = dtuple_create_with_vcol(v_heap ? v_heap : heap,
                                      index->table->get_n_cols(),
                                      dict_table_get_n_v_cols(index->table));
      dtuple_init_v_fld(*vrow);
    }

    ut_ad(index->table->n_v_cols);
    trx_undo_read_v_cols(index->table, ptr, *vrow,
                         v_status & TRX_UNDO_PREV_IN_PURGE, false, nullptr,
                         (v_heap != nullptr ? v_heap : heap));
  }

  if (update != nullptr) {
    update->reset();
  }

  return true;
}

/** Read virtual column value from undo log
@param[in]      table           the table
@param[in]      ptr             undo log pointer
@param[in,out]  row             the dtuple to fill
@param[in]      in_purge        called by purge thread
@param[in]      online          true if this is from online DDL log
@param[in]      col_map         online rebuild column map
@param[in,out]  heap            memory heap to keep value when necessary */
void trx_undo_read_v_cols(const dict_table_t *table, const byte *ptr,
                          const dtuple_t *row, bool in_purge, bool online,
                          const ulint *col_map, mem_heap_t *heap) {
  const byte *end_ptr;
  bool first_v_col = true;
  bool is_undo_log = true;

  end_ptr = ptr + mach_read_from_2(ptr);
  ptr += 2;
  while (ptr < end_ptr) {
    dfield_t *dfield;
    dfield_t multi_value_field;
    const byte *field = nullptr;
    ulint field_no;
    ulint len = 0;
    ulint orig_len = 0;
    bool is_virtual;
    dict_v_col_t *vcol = nullptr;
    ulint col_no;

    field_no = mach_read_next_compressed(const_cast<const byte **>(&ptr));

    is_virtual = (field_no >= REC_MAX_N_FIELDS);

    if (is_virtual) {
      ptr =
          trx_undo_read_v_idx(table, ptr, first_v_col, &is_undo_log, &field_no);
      first_v_col = false;
    }

    if (!is_virtual || field_no == ULINT_UNDEFINED) {
      /* The virtual column is no longer indexed or does not exist.
      "continue" needs to put after ptr gets advanced */
      if (trx_undo_rec_is_multi_value(ptr)) {
        ptr = trx_undo_rec_get_multi_value(ptr, nullptr, heap);
      } else {
        ptr = trx_undo_rec_get_col_val(ptr, &field, &len, &orig_len);
      }
      continue;
    }

    vcol = dict_table_get_nth_v_col(table, field_no);

    if (!col_map) {
      col_no = vcol->v_pos;
    } else {
      col_no = col_map[vcol->v_pos];
    }

    if (col_no == ULINT_UNDEFINED) {
      if (trx_undo_rec_is_multi_value(ptr)) {
        ptr = trx_undo_rec_get_multi_value(ptr, nullptr, heap);
      } else {
        ptr = trx_undo_rec_get_col_val(ptr, &field, &len, &orig_len);
      }
      continue;
    }

    dfield = dtuple_get_nth_v_field(row, col_no);

    if (trx_undo_rec_is_multi_value(ptr)) {
      ut_ad(vcol->m_col.is_multi_value());
      ptr = trx_undo_rec_get_multi_value(ptr, &multi_value_field, heap);
    } else {
      ut_ad(!vcol->m_col.is_multi_value());
      ptr = trx_undo_rec_get_col_val(ptr, &field, &len, &orig_len);
    }

    if (!in_purge || dfield_get_type(dfield)->mtype == DATA_MISSING) {
      vcol->m_col.copy_type(dfield_get_type(dfield));
      if (online && !vcol->m_col.is_multi_value()) {
        dfield->adjust_v_data_mysql(vcol, dict_table_is_comp(table), field, len,
                                    heap);
      } else if (!vcol->m_col.is_multi_value()) {
        dfield_set_data(dfield, field, len);
      } else {
        dfield_copy_data(dfield, &multi_value_field);
      }
    }
  }

  ut_ad(ptr == end_ptr);
}
#endif /* !UNIV_HOTBACKUP */
