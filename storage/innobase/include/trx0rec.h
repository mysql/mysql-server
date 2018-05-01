/*****************************************************************************

Copyright (c) 1996, 2018, Oracle and/or its affiliates. All Rights Reserved.

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

#include "my_compiler.h"

/** @file include/trx0rec.h
 Transaction undo log record

 Created 3/26/1996 Heikki Tuuri
 *******************************************************/

#ifndef trx0rec_h
#define trx0rec_h

#include "data0data.h"
#include "dict0types.h"
#include "lob0undo.h"
#include "mtr0mtr.h"
#include "page0types.h"
#include "rem0types.h"
#include "row0log.h"
#include "row0types.h"
#include "trx0types.h"
#include "univ.i"

#ifndef UNIV_HOTBACKUP
#include "que0types.h"

/** Copies the undo record to the heap.
@param[in]	undo_rec	undo log record
@param[in]	heap		heap where copied
@return own: copy of undo log record */
UNIV_INLINE
trx_undo_rec_t *trx_undo_rec_copy(const trx_undo_rec_t *undo_rec,
                                  mem_heap_t *heap);

/** Reads the undo log record type.
 @return record type */
UNIV_INLINE
ulint trx_undo_rec_get_type(
    const trx_undo_rec_t *undo_rec); /*!< in: undo log record */
/** Reads from an undo log record the record compiler info.
 @return compiler info */
UNIV_INLINE
ulint trx_undo_rec_get_cmpl_info(
    const trx_undo_rec_t *undo_rec); /*!< in: undo log record */
/** Returns TRUE if an undo log record contains an extern storage field.
 @return true if extern */
UNIV_INLINE
ibool trx_undo_rec_get_extern_storage(
    const trx_undo_rec_t *undo_rec); /*!< in: undo log record */
/** Reads the undo log record number.
 @return undo no */
UNIV_INLINE
undo_no_t trx_undo_rec_get_undo_no(
    const trx_undo_rec_t *undo_rec); /*!< in: undo log record */

/** Returns the start of the undo record data area. */
#define trx_undo_rec_get_ptr(undo_rec, undo_no) \
  ((undo_rec) + trx_undo_rec_get_offset(undo_no))

/** Reads from an undo log record the table ID
@param[in]	undo_rec	Undo log record
@return the table ID */
table_id_t trx_undo_rec_get_table_id(const trx_undo_rec_t *undo_rec)
    MY_ATTRIBUTE((warn_unused_result));

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
    mem_heap_t *heap);   /*!< in: memory heap from which the memory
                         needed is allocated */
/** Reads from an undo log update record the system field values of the old
 version.
 @return remaining part of undo log record after reading these values */
byte *trx_undo_update_rec_get_sys_cols(
    const byte *ptr,      /*!< in: remaining part of undo
                          log record after reading
                          general parameters */
    trx_id_t *trx_id,     /*!< out: trx id */
    roll_ptr_t *roll_ptr, /*!< out: roll ptr */
    ulint *info_bits);    /*!< out: info bits state */

struct type_cmpl_t;

/** Builds an update vector based on a remaining part of an undo log record.
 @return remaining part of the record, NULL if an error detected, which
 means that the record is corrupted. */
byte *trx_undo_update_rec_get_update(
    const byte *ptr,            /*!< in: remaining part in update undo log
                                record, after reading the row reference
                                NOTE that this copy of the undo log record must
                                be preserved as long as the update vector is
                                used, as we do NOT copy the data in the
                                record! */
    dict_index_t *index,        /*!< in: clustered index */
    ulint type,                 /*!< in: TRX_UNDO_UPD_EXIST_REC,
                                TRX_UNDO_UPD_DEL_REC, or
                                TRX_UNDO_DEL_MARK_REC; in the last case,
                                only trx id and roll ptr fields are added to
                                the update vector */
    trx_id_t trx_id,            /*!< in: transaction id from this undorecord */
    roll_ptr_t roll_ptr,        /*!< in: roll pointer from this undo record */
    ulint info_bits,            /*!< in: info bits from this undo record */
    trx_t *trx,                 /*!< in: transaction */
    mem_heap_t *heap,           /*!< in: memory heap from which the memory
                                needed is allocated */
    upd_t **upd,                /*!< out, own: update vector */
    lob::undo_vers_t *lob_undo, /*!< out: LOB undo information. */
    type_cmpl_t &type_cmpl);    /*!< out: type compilation info */

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
    ibool ignore_prefix, /*!< in: flag to indicate if we
                  expect blob prefixes in undo. Used
                  only in the assertion. */
    mem_heap_t *heap)    /*!< in: memory heap from which the memory
                         needed is allocated */
    MY_ATTRIBUTE((warn_unused_result));
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
    const rec_t *rec,            /*!< in: case of an update or delete
                                 marking, the record in the clustered
                                 index, otherwise NULL */
    const ulint *offsets,        /*!< in: rec_get_offsets(rec) */
    roll_ptr_t *roll_ptr)        /*!< out: rollback pointer to the
                                 inserted undo log record,
                                 0 if BTR_NO_UNDO_LOG
                                 flag was specified */
    MY_ATTRIBUTE((warn_unused_result));

/** status bit used for trx_undo_prev_version_build() */

/** TRX_UNDO_PREV_IN_PURGE tells trx_undo_prev_version_build() that it
is being called purge view and we would like to get the purge record
even it is in the purge view (in normal case, it will return without
fetching the purge record */
#define TRX_UNDO_PREV_IN_PURGE 0x1

/** This tells trx_undo_prev_version_build() to fetch the old value in
the undo log (which is the after image for an update) */
#define TRX_UNDO_GET_OLD_V_VALUE 0x2

/** Build a previous version of a clustered index record. The caller must hold
a latch on the index page of the clustered index record.
@param[in]	index_rec	clustered index record in the index tree
@param[in]	index_mtr	mtr which contains the latch to index_rec page
                                and purge_view
@param[in]	rec		version of a clustered index record
@param[in]	index		clustered index
@param[in,out]	offsets		rec_get_offsets(rec, index)
@param[in]	heap		memory heap from which the memory needed is
                                allocated
@param[out]	old_vers	previous version, or NULL if rec is the first
                                inserted version, or if history data has been
                                deleted
@param[in]	v_heap		memory heap used to create vrow dtuple if it is
                                not yet created. This heap diffs from "heap"
                                above in that it could be
                                prebuilt->old_vers_heap for selection
@param[out]	vrow		virtual column info, if any
@param[in]	v_status	status determine if it is going into this
                                function by purge thread or not. And if we read
                                "after image" of undo log has been rebuilt
@retval false if the previous version is earlier than purge_view, which means
that it may have been removed */
bool trx_undo_prev_version_build(const rec_t *index_rec, mtr_t *index_mtr,
                                 const rec_t *rec, dict_index_t *index,
                                 ulint *offsets, mem_heap_t *heap,
                                 rec_t **old_vers, mem_heap_t *v_heap,
                                 const dtuple_t **vrow, ulint v_status,
                                 lob::undo_vers_t *lob_undo);

#endif /* !UNIV_HOTBACKUP */
/** Parses a redo log record of adding an undo log record.
 @return end of log record or NULL */
byte *trx_undo_parse_add_undo_rec(byte *ptr,     /*!< in: buffer */
                                  byte *end_ptr, /*!< in: buffer end */
                                  page_t *page); /*!< in: page or NULL */
/** Parses a redo log record of erasing of an undo page end.
 @return end of log record or NULL */
byte *trx_undo_parse_erase_page_end(byte *ptr,     /*!< in: buffer */
                                    byte *end_ptr, /*!< in: buffer end */
                                    page_t *page,  /*!< in: page or NULL */
                                    mtr_t *mtr);   /*!< in: mtr or NULL */

/** Read from an undo log record a non-virtual column value.
@param[in,out]	ptr		pointer to remaining part of the undo record
@param[in,out]	field		stored field
@param[in,out]	len		length of the field, or UNIV_SQL_NULL
@param[in,out]	orig_len	original length of the locally stored part
of an externally stored column, or 0
@return remaining part of undo log record after reading these values */
byte *trx_undo_rec_get_col_val(const byte *ptr, const byte **field, ulint *len,
                               ulint *orig_len);

/** Read virtual column value from undo log
@param[in]	table		the table
@param[in]	ptr		undo log pointer
@param[in,out]	row		the dtuple to fill
@param[in]	in_purge        called by purge thread
@param[in]	online		true if this is from online DDL log
@param[in]	col_map		online rebuild column map
@param[in,out]	heap		memory heap to keep value when necessary */
void trx_undo_read_v_cols(const dict_table_t *table, const byte *ptr,
                          const dtuple_t *row, bool in_purge, bool online,
                          const ulint *col_map, mem_heap_t *heap);

/** Read virtual column index from undo log if the undo log contains such
info, and verify the column is still indexed, and output its position
@param[in]	table		the table
@param[in]	ptr		undo log pointer
@param[in]	first_v_col	if this is the first virtual column, which
                                has the version marker
@param[in,out]	is_undo_log	his function is used to parse both undo log,
                                and online log for virtual columns. So
                                check to see if this is undo log
@param[out]	field_no	the column number
@return remaining part of undo log record after reading these values */
const byte *trx_undo_read_v_idx(const dict_table_t *table, const byte *ptr,
                                bool first_v_col, bool *is_undo_log,
                                ulint *field_no);

#ifndef UNIV_HOTBACKUP

/* Types of an undo log record: these have to be smaller than 16, as the
compilation info multiplied by 16 is ORed to this value in an undo log
record */

#define TRX_UNDO_INSERT_REC 11 /* fresh insert into clustered index */
#define TRX_UNDO_UPD_EXIST_REC        \
  12 /* update of a non-delete-marked \
     record */
#define TRX_UNDO_UPD_DEL_REC                \
  13 /* update of a delete marked record to \
     a not delete marked record; also the   \
     fields of the record can change */
#define TRX_UNDO_DEL_MARK_REC              \
  14 /* delete marking of a record; fields \
     do not change */
#define TRX_UNDO_CMPL_INFO_MULT           \
  16 /* compilation info is multiplied by \
     this and ORed to the type above */

#define TRX_UNDO_MODIFY_BLOB              \
  64 /* If this bit is set in type_cmpl,  \
     then the undo log record has support \
     for partial update of BLOBs. Also to \
     make the undo log format extensible, \
     introducing a new flag next to the   \
     type_cmpl flag. */

#define TRX_UNDO_UPD_EXTERN                \
  128 /* This bit can be ORed to type_cmpl \
      to denote that we updated external   \
      storage fields: used by purge to     \
      free the external storage */

/* Operation type flags used in trx_undo_report_row_operation */
#define TRX_UNDO_INSERT_OP 1
#define TRX_UNDO_MODIFY_OP 2

/** The type and compilation info flag in the undo record for update.
For easier understanding let the 8 bits be numbered as
7, 6, 5, 4, 3, 2, 1, 0. */
struct type_cmpl_t {
  type_cmpl_t() : m_flag(0) {}

  const byte *read(const byte *ptr) {
    m_flag = mach_read_from_1(ptr);
    return (ptr + 1);
  }

  ulint type_info() {
    /* Get 0-3 */
    return (m_flag & 0x0F);
  }

  ulint cmpl_info() {
    /* Get bits 5 and 4 */
    return ((m_flag >> 4) & 0x03);
  }

  /** Is an LOB updated by this update operation.
  @return true if LOB is updated, false otherwise. */
  bool is_lob_updated() {
    /* Check if bit 7 is set. */
    return (m_flag & TRX_UNDO_UPD_EXTERN);
  }

  /** Does the undo log record contains information about LOB partial
  update vector.
  @return true if undo contains LOB update info. */
  bool is_lob_undo() const {
    /* Check if bit 6 is set. */
    return (m_flag & TRX_UNDO_MODIFY_BLOB);
  }

 private:
  uint8_t m_flag;
};

/*******************************************************************/ /**
 Builds an update vector based on a remaining part of an undo log record.
 @return remaining part of the record, NULL if an error detected, which
 means that the record is corrupted */
byte *trx_undo_update_rec_get_update(
    /*===========================*/
    const byte *ptr,     /*!< in: remaining part in update undo log
                         record, after reading the row reference
                         NOTE that this copy of the undo log record must
                         be preserved as long as the update vector is
                         used, as we do NOT copy the data in the
                         record! */
    dict_index_t *index, /*!< in: clustered index */
    ulint type,          /*!< in: TRX_UNDO_UPD_EXIST_REC,
                         TRX_UNDO_UPD_DEL_REC, or
                         TRX_UNDO_DEL_MARK_REC; in the last case,
                         only trx id and roll ptr fields are added to
                         the update vector */
    trx_id_t trx_id,     /*!< in: transaction id from this undorecord */
    roll_ptr_t roll_ptr, /*!< in: roll pointer from this undo record */
    ulint info_bits,     /*!< in: info bits from this undo record */
    trx_t *trx,          /*!< in: transaction */
    mem_heap_t *heap,    /*!< in: memory heap from which the memory
                         needed is allocated */
    upd_t **upd,         /*!< out, own: update vector */
    lob::undo_vers_t *lob_undo, type_cmpl_t &type_cmpl);

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
    type_cmpl_t &type_cmpl);  /*!< out: type compilation info. */

#include "trx0rec.ic"

#endif /* !UNIV_HOTBACKUP */

#endif /* trx0rec_h */
