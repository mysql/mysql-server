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

/** @file row/row0upd.cc
 Update of a row

 Created 12/27/1996 Heikki Tuuri
 *******************************************************/

#include <sys/types.h>

#include "dict0dict.h"
#include "ha_prototypes.h"

#include "my_byteorder.h"
#include "my_dbug.h"

#include "rem0rec.h"
#include "row0upd.h"
#include "trx0undo.h"
#ifndef UNIV_HOTBACKUP
#include <algorithm>

#include "btr0btr.h"
#include "btr0cur.h"
#include "buf0lru.h"
#include "current_thd.h"
#include "dict0boot.h"
#include "dict0crea.h"
#include "eval0eval.h"
#include "fts0fts.h"
#include "fts0types.h"
#include "lob0lob.h"
#include "lock0lock.h"
#include "log0log.h"
#include "mach0data.h"
#include "pars0sym.h"
#include "que0que.h"
#include "rem0cmp.h"
#endif /* !UNIV_HOTBACKUP */
#include "row0ext.h"
#include "row0ins.h"
#include "row0log.h"
#ifndef UNIV_HOTBACKUP
#include "fts0fts.h"
#include "fts0types.h"
#include "row0row.h"
#include "row0sel.h"
#include "trx0rec.h"
#endif /* !UNIV_HOTBACKUP */
#include <algorithm>
#include "lob0lob.h"
#ifndef UNIV_HOTBACKUP
#include "current_thd.h"
#include "dict0dd.h"
#include "field.h"
#endif /* !UNIV_HOTBACKUP */

#ifndef UNIV_HOTBACKUP
/* What kind of latch and lock can we assume when the control comes to
   -------------------------------------------------------------------
an update node?
--------------
Efficiency of massive updates would require keeping an x-latch on a
clustered index page through many updates, and not setting an explicit
x-lock on clustered index records, as they anyway will get an implicit
x-lock when they are updated. A problem is that the read nodes in the
graph should know that they must keep the latch when passing the control
up to the update node, and not set any record lock on the record which
will be updated. Another problem occurs if the execution is stopped,
as the kernel switches to another query thread, or the transaction must
wait for a lock. Then we should be able to release the latch and, maybe,
acquire an explicit x-lock on the record.
        Because this seems too complicated, we conclude that the less
efficient solution of releasing all the latches when the control is
transferred to another node, and acquiring explicit x-locks, is better. */

/* How is a delete performed? If there is a delete without an
explicit cursor, i.e., a searched delete, there are at least
two different situations:
the implicit select cursor may run on (1) the clustered index or
on (2) a secondary index. The delete is performed by setting
the delete bit in the record and substituting the id of the
deleting transaction for the original trx id, and substituting a
new roll ptr for previous roll ptr. The old trx id and roll ptr
are saved in the undo log record. Thus, no physical changes occur
in the index tree structure at the time of the delete. Only
when the undo log is purged, the index records will be physically
deleted from the index trees.

The query graph executing a searched delete would consist of
a delete node which has as a subtree a select subgraph.
The select subgraph should return a (persistent) cursor
in the clustered index, placed on page which is x-latched.
The delete node should look for all secondary index records for
this clustered index entry and mark them as deleted. When is
the x-latch freed? The most efficient way for performing a
searched delete is obviously to keep the x-latch for several
steps of query graph execution. */

/*************************************************************************
IMPORTANT NOTE: Any operation that generates redo MUST check that there
is enough space in the redo log before for that operation. This is
done by calling log_free_check(). The reason for checking the
availability of the redo log space before the start of the operation is
that we MUST not hold any synchonization objects when performing the
check.
If you make a change in this module make sure that no codepath is
introduced where a call to log_free_check() is bypassed. */

static_assert(DATA_TRX_ID + 1 == DATA_ROLL_PTR, "DATA_TRX_ID invalid value!");

/** Checks if an update vector changes some of the first ordering fields of an
 index record. This is only used in foreign key checks and we can assume
 that index does not contain column prefixes.
 @return true if changes */
static ibool row_upd_changes_first_fields_binary(
    dtuple_t *entry,     /*!< in: old value of index entry */
    dict_index_t *index, /*!< in: index of entry */
    const upd_t *update, /*!< in: update vector for the row */
    ulint n);            /*!< in: how many first fields to check */

/** Checks if index currently is mentioned as a referenced index in a foreign
 key constraint.

 NOTE that since we do not hold dict_operation_lock when leaving the
 function, it may be that the referencing table has been dropped when
 we leave this function: this function is only for heuristic use!

 @return true if referenced */
static ibool row_upd_index_is_referenced(dict_index_t *index, /*!< in: index */
                                         trx_t *trx) /*!< in: transaction */
{
  dict_table_t *table = index->table;
  ibool is_referenced = FALSE;

  if (table->referenced_set.empty()) {
    return (FALSE);
  }

  dict_foreign_set::iterator it =
      std::find_if(table->referenced_set.begin(), table->referenced_set.end(),
                   dict_foreign_with_index(index));

  is_referenced = (it != table->referenced_set.end());

  return (is_referenced);
}

/** Checks if possible foreign key constraints hold after a delete of the record
 under pcur.

 NOTE that this function will temporarily commit mtr and lose the
 pcur position!

 @return DB_SUCCESS or an error code */
static MY_ATTRIBUTE((warn_unused_result)) dberr_t
    row_upd_check_references_constraints(
        upd_node_t *node,    /*!< in: row update node */
        btr_pcur_t *pcur,    /*!< in: cursor positioned on a record; NOTE: the
                             cursor position is lost in this function! */
        dict_table_t *table, /*!< in: table in question */
        dict_index_t *index, /*!< in: index of the cursor */
        ulint *offsets,      /*!< in/out: rec_get_offsets(pcur.rec, index) */
        que_thr_t *thr,      /*!< in: query thread */
        mtr_t *mtr)          /*!< in: mtr */
{
  dict_foreign_t *foreign;
  mem_heap_t *heap;
  dtuple_t *entry;
  trx_t *trx;
  const rec_t *rec;
  ulint n_ext;
  dberr_t err;

  DBUG_ENTER("row_upd_check_references_constraints");

  /* TODO: NEWDD: WL#6049 Ignore FK on DD system tables for now */
  if (table->is_dd_table) {
    DBUG_RETURN(DB_SUCCESS);
  }

  if (table->referenced_set.empty()) {
    DBUG_RETURN(DB_SUCCESS);
  }

  trx = thr_get_trx(thr);

  rec = btr_pcur_get_rec(pcur);
  ut_ad(rec_offs_validate(rec, index, offsets));

  heap = mem_heap_create(500);

  entry = row_rec_to_index_entry(rec, index, offsets, &n_ext, heap);

  mtr_commit(mtr);

  DEBUG_SYNC_C("foreign_constraint_check_for_update");

  mtr_start(mtr);

  for (dict_foreign_set::iterator it = table->referenced_set.begin();
       it != table->referenced_set.end(); ++it) {
    foreign = *it;

    /* Note that we may have an update which updates the index
    record, but does NOT update the first fields which are
    referenced in a foreign key constraint. Then the update does
    NOT break the constraint. */

    if (foreign->referenced_index == index &&
        (node->is_delete ||
         row_upd_changes_first_fields_binary(entry, index, node->update,
                                             foreign->n_fields))) {
      dict_table_t *foreign_table = foreign->foreign_table;

      dict_table_t *ref_table = NULL;

      if (foreign_table == NULL) {
        MDL_ticket *mdl;

        ref_table = dd_table_open_on_name(trx->mysql_thd, &mdl,
                                          foreign->foreign_table_name_lookup,
                                          false, DICT_ERR_IGNORE_NONE);
      }

      /* NOTE that if the thread ends up waiting for a lock
      we will release dict_operation_lock temporarily!
      But the counter on the table protects 'foreign' from
      being dropped while the check is running. */
      if (foreign_table) {
        os_atomic_increment_ulint(&foreign_table->n_foreign_key_checks_running,
                                  1);
      }

      err = row_ins_check_foreign_constraint(FALSE, foreign, table, entry, thr);

      if (foreign_table) {
        os_atomic_decrement_ulint(&foreign_table->n_foreign_key_checks_running,
                                  1);
      }

      if (ref_table != NULL) {
        dict_table_close(ref_table, FALSE, FALSE);
      }

      if (err != DB_SUCCESS) {
        goto func_exit;
      }
    }
  }

  err = DB_SUCCESS;

func_exit:
  mem_heap_free(heap);

  DEBUG_SYNC_C("foreign_constraint_check_for_update_done");
  DBUG_RETURN(err);
}

/** Creates an update node for a query graph.
 @return own: update node */
upd_node_t *upd_node_create(mem_heap_t *heap) /*!< in: mem heap where created */
{
  upd_node_t *node;

  node = static_cast<upd_node_t *>(mem_heap_zalloc(heap, sizeof(upd_node_t)));

  node->common.type = QUE_NODE_UPDATE;
  node->state = UPD_NODE_UPDATE_CLUSTERED;
  node->heap = mem_heap_create(128);
  node->magic_n = UPD_NODE_MAGIC_N;

  return (node);
}
#endif /* !UNIV_HOTBACKUP */

/** Updates the trx id and roll ptr field in a clustered index record in
 database recovery. */
void row_upd_rec_sys_fields_in_recovery(
    rec_t *rec,               /*!< in/out: record */
    page_zip_des_t *page_zip, /*!< in/out: compressed page, or NULL */
    const ulint *offsets,     /*!< in: array returned by rec_get_offsets() */
    ulint pos,                /*!< in: TRX_ID position in rec */
    trx_id_t trx_id,          /*!< in: transaction id */
    roll_ptr_t roll_ptr)      /*!< in: roll ptr of the undo log record */
{
  ut_ad(rec_offs_validate(rec, NULL, offsets));

  if (page_zip) {
    page_zip_write_trx_id_and_roll_ptr(page_zip, rec, offsets, pos, trx_id,
                                       roll_ptr);
  } else {
    byte *field;
    ulint len;

    field = const_cast<byte *>(rec_get_nth_field(rec, offsets, pos, &len));
    ut_ad(len == DATA_TRX_ID_LEN);
    trx_write_trx_id(field, trx_id);
    trx_write_roll_ptr(field + DATA_TRX_ID_LEN, roll_ptr);
  }
}

#ifndef UNIV_HOTBACKUP
/** Sets the trx id or roll ptr field of a clustered index entry. */
void row_upd_index_entry_sys_field(
    dtuple_t *entry,     /*!< in/out: index entry, where the memory
                         buffers for sys fields are already allocated:
                         the function just copies the new values to
                         them */
    dict_index_t *index, /*!< in: clustered index */
    ulint type,          /*!< in: DATA_TRX_ID or DATA_ROLL_PTR */
    ib_uint64_t val)     /*!< in: value to write */
{
  dfield_t *dfield;
  byte *field;
  ulint pos;

  ut_ad(index->is_clustered());

  pos = index->get_sys_col_pos(type);

  dfield = dtuple_get_nth_field(entry, pos);
  field = static_cast<byte *>(dfield_get_data(dfield));

  if (type == DATA_TRX_ID) {
    ut_ad(val > 0);
    trx_write_trx_id(field, val);
  } else {
    ut_ad(type == DATA_ROLL_PTR);
    trx_write_roll_ptr(field, val);
  }
}

/** Returns TRUE if row update changes size of some field in index or if some
 field to be updated is stored externally in rec or update.
 @return true if the update changes the size of some field in index or
 the field is external in rec or update */
ibool row_upd_changes_field_size_or_external(
    dict_index_t *index,  /*!< in: index */
    const ulint *offsets, /*!< in: rec_get_offsets(rec, index) */
    const upd_t *update)  /*!< in: update vector */
{
  const upd_field_t *upd_field;
  const dfield_t *new_val;
  ulint old_len;
  ulint new_len;
  ulint n_fields;
  ulint i;

  ut_ad(rec_offs_validate(NULL, index, offsets));
  ut_ad(!index->table->skip_alter_undo);

  n_fields = upd_get_n_fields(update);

  for (i = 0; i < n_fields; i++) {
    upd_field = upd_get_nth_field(update, i);

    /* We should ignore virtual field if the index is not
    a virtual index */
    if (upd_fld_is_virtual_col(upd_field) &&
        dict_index_has_virtual(index) != DICT_VIRTUAL) {
      continue;
    }

    new_val = &(upd_field->new_val);
    new_len = dfield_get_len(new_val);

    if (dfield_is_null(new_val) && !rec_offs_comp(offsets)) {
      /* A bug fixed on Dec 31st, 2004: we looked at the
      SQL NULL size from the wrong field! We may backport
      this fix also to 4.0. The merge to 5.0 will be made
      manually immediately after we commit this to 4.1. */

      new_len = index->get_col(upd_field->field_no)->get_null_size(0);
    }

    old_len = rec_offs_nth_size(offsets, upd_field->field_no);

    if (rec_offs_comp(offsets)) {
      if (rec_offs_nth_sql_null(offsets, upd_field->field_no)) {
        /* Note that in the compact table format,
        for a variable length field, an SQL NULL
        will use zero bytes in the offset array
        at the start of the physical record, but
        a zero-length value (empty string) will
        use one byte! Thus, we cannot use
        update-in-place if we update an SQL NULL
        varchar to an empty string! */

        old_len = UNIV_SQL_NULL;
      } else if (rec_offs_nth_default(offsets, upd_field->field_no)) {
        /* This will force to do pessimistic update,
        since the default value is not inlined,
        so any update to it will extend the record. */
        old_len = UNIV_SQL_ADD_COL_DEFAULT;
      }
    } else {
      /* REDUNDANT row format, if it updates the field with
      not inlined default value, do it in pessimistic way */
      if (rec_offs_nth_default(offsets, upd_field->field_no)) {
        old_len = UNIV_SQL_ADD_COL_DEFAULT;
      }
    }

    if (dfield_is_ext(new_val) || old_len != new_len ||
        rec_offs_nth_extern(offsets, upd_field->field_no)) {
      return (TRUE);
    }
  }

  return (FALSE);
}

/** Returns true if row update contains disowned external fields.
 @return true if the update contains disowned external fields. */
bool row_upd_changes_disowned_external(
    const upd_t *update) /*!< in: update vector */
{
  const upd_field_t *upd_field;
  const dfield_t *new_val;
  ulint n_fields;
  ulint i;

  n_fields = upd_get_n_fields(update);

  for (i = 0; i < n_fields; i++) {
    upd_field = upd_get_nth_field(update, i);
    new_val = &(upd_field->new_val);

    if (!dfield_is_ext(new_val)) {
      continue;
    }

    ut_ad(dfield_get_len(new_val) >= BTR_EXTERN_FIELD_REF_SIZE);

    lob::ref_t ref(new_val->blobref());

    if (!ref.is_owner()) {
      return (true);
    }
  }

  return (false);
}
#endif /* !UNIV_HOTBACKUP */

/** Replaces the new column values stored in the update vector to the
 record given. No field size changes are allowed. This function is
 usually invoked on a clustered index. The only use case for a
 secondary index is row_ins_sec_index_entry_by_modify() or its
 counterpart in ibuf_insert_to_index_page(). */
void row_upd_rec_in_place(
    rec_t *rec,               /*!< in/out: record where replaced */
    dict_index_t *index,      /*!< in: the index the record belongs to */
    const ulint *offsets,     /*!< in: array returned by rec_get_offsets() */
    const upd_t *update,      /*!< in: update vector */
    page_zip_des_t *page_zip) /*!< in: compressed page with enough space
                             available, or NULL */
{
  const upd_field_t *upd_field;
  const dfield_t *new_val;
  ulint n_fields;
  ulint i;

  ut_ad(rec_offs_validate(rec, index, offsets));
  ut_ad(!index->table->skip_alter_undo);

  if (rec_offs_comp(offsets)) {
    bool is_instant = rec_get_instant_flag_new(rec);
    rec_set_info_bits_new(rec, update->info_bits);
    if (is_instant) {
      rec_set_instant_flag_new(rec, true);
    } else {
      rec_set_instant_flag_new(rec, false);
    }
  } else {
    rec_set_info_bits_old(rec, update->info_bits);
  }

  n_fields = upd_get_n_fields(update);

  for (i = 0; i < n_fields; i++) {
    upd_field = upd_get_nth_field(update, i);

    /* No need to update virtual columns for non-virtual index */
    if (upd_fld_is_virtual_col(upd_field) && !dict_index_has_virtual(index)) {
      continue;
    }

    new_val = &(upd_field->new_val);
    ut_ad(!dfield_is_ext(new_val) ==
          !rec_offs_nth_extern(offsets, upd_field->field_no));

    /* Updating default value for instantly added columns
    must not be done in-place. See also
    row_upd_changes_field_size_or_external() */
    ut_ad(!rec_offs_nth_default(offsets, upd_field->field_no));
    rec_set_nth_field(rec, offsets, upd_field->field_no,
                      dfield_get_data(new_val), dfield_get_len(new_val));
  }

  if (page_zip) {
    page_zip_write_rec(page_zip, rec, index, offsets, 0);
  }
}

#ifndef UNIV_HOTBACKUP
/** Writes into the redo log the values of trx id and roll ptr and enough info
 to determine their positions within a clustered index record.
 @return new pointer to mlog */
byte *row_upd_write_sys_vals_to_log(
    dict_index_t *index, /*!< in: clustered index */
    trx_id_t trx_id,     /*!< in: transaction id */
    roll_ptr_t roll_ptr, /*!< in: roll ptr of the undo log record */
    byte *log_ptr,       /*!< pointer to a buffer of size > 20 opened
                         in mlog */
    mtr_t *mtr MY_ATTRIBUTE((unused))) /*!< in: mtr */
{
  ut_ad(index->is_clustered());
  ut_ad(mtr);

  log_ptr +=
      mach_write_compressed(log_ptr, index->get_sys_col_pos(DATA_TRX_ID));

  trx_write_roll_ptr(log_ptr, roll_ptr);
  log_ptr += DATA_ROLL_PTR_LEN;

  log_ptr += mach_u64_write_compressed(log_ptr, trx_id);

  return (log_ptr);
}
#endif /* !UNIV_HOTBACKUP */

/** Parses the log data of system field values.
 @return log data end or NULL */
byte *row_upd_parse_sys_vals(const byte *ptr,     /*!< in: buffer */
                             const byte *end_ptr, /*!< in: buffer end */
                             ulint *pos, /*!< out: TRX_ID position in record */
                             trx_id_t *trx_id,     /*!< out: trx id */
                             roll_ptr_t *roll_ptr) /*!< out: roll ptr */
{
  *pos = mach_parse_compressed(&ptr, end_ptr);

  if (ptr == NULL) {
    return (NULL);
  }

  if (end_ptr < ptr + DATA_ROLL_PTR_LEN) {
    return (NULL);
  }

  *roll_ptr = trx_read_roll_ptr(ptr);
  ptr += DATA_ROLL_PTR_LEN;

  *trx_id = mach_u64_parse_compressed(&ptr, end_ptr);

  return (const_cast<byte *>(ptr));
}

#ifndef UNIV_HOTBACKUP
/** Writes to the redo log the new values of the fields occurring in the index.
 */
void row_upd_index_write_log(
    const upd_t *update, /*!< in: update vector */
    byte *log_ptr,       /*!< in: pointer to mlog buffer: must
                         contain at least MLOG_BUF_MARGIN bytes
                         of free space; the buffer is closed
                         within this function */
    mtr_t *mtr)          /*!< in: mtr into whose log to write */
{
  const upd_field_t *upd_field;
  const dfield_t *new_val;
  ulint len;
  ulint n_fields;
  byte *buf_end;
  ulint i;

  n_fields = upd_get_n_fields(update);

  buf_end = log_ptr + MLOG_BUF_MARGIN;

  mach_write_to_1(log_ptr, update->info_bits);
  log_ptr++;
  log_ptr += mach_write_compressed(log_ptr, n_fields);

  for (i = 0; i < n_fields; i++) {
#if MLOG_BUF_MARGIN <= 30
#error "MLOG_BUF_MARGIN <= 30"
#endif

    if (log_ptr + 30 > buf_end) {
      mlog_close(mtr, log_ptr);

      log_ptr = mlog_open(mtr, MLOG_BUF_MARGIN);
      buf_end = log_ptr + MLOG_BUF_MARGIN;
    }

    upd_field = upd_get_nth_field(update, i);

    new_val = &(upd_field->new_val);

    len = dfield_get_len(new_val);

    /* If this is a virtual column, mark it using special
    field_no */
    ulint field_no = upd_fld_is_virtual_col(upd_field)
                         ? REC_MAX_N_FIELDS + upd_field->field_no
                         : upd_field->field_no;

    log_ptr += mach_write_compressed(log_ptr, field_no);
    log_ptr += mach_write_compressed(log_ptr, len);

    if (len != UNIV_SQL_NULL) {
      if (log_ptr + len < buf_end) {
        memcpy(log_ptr, dfield_get_data(new_val), len);

        log_ptr += len;
      } else {
        mlog_close(mtr, log_ptr);

        mlog_catenate_string(mtr, static_cast<byte *>(dfield_get_data(new_val)),
                             len);

        log_ptr = mlog_open(mtr, MLOG_BUF_MARGIN);
        buf_end = log_ptr + MLOG_BUF_MARGIN;
      }
    }
  }

  mlog_close(mtr, log_ptr);
}
#endif /* !UNIV_HOTBACKUP */

/** Parses the log data written by row_upd_index_write_log.
 @return log data end or NULL */
byte *row_upd_index_parse(
    const byte *ptr,     /*!< in: buffer */
    const byte *end_ptr, /*!< in: buffer end */
    mem_heap_t *heap,    /*!< in: memory heap where update vector is
                         built */
    upd_t **update_out)  /*!< out: update vector */
{
  upd_t *update;
  upd_field_t *upd_field;
  dfield_t *new_val;
  ulint len;
  ulint n_fields;
  ulint info_bits;
  ulint i;

  if (end_ptr < ptr + 1) {
    return (NULL);
  }

  info_bits = mach_read_from_1(ptr);
  ptr++;
  n_fields = mach_parse_compressed(&ptr, end_ptr);

  if (ptr == NULL) {
    return (NULL);
  }

  update = upd_create(n_fields, heap);
  update->info_bits = info_bits;

  for (i = 0; i < n_fields; i++) {
    ulint field_no;
    upd_field = upd_get_nth_field(update, i);
    new_val = &(upd_field->new_val);

    field_no = mach_parse_compressed(&ptr, end_ptr);

    if (ptr == NULL) {
      return (NULL);
    }

    /* Check if this is a virtual column, mark the prtype
    if that is the case */
    if (field_no >= REC_MAX_N_FIELDS) {
      new_val->type.prtype |= DATA_VIRTUAL;
      field_no -= REC_MAX_N_FIELDS;
    }

    upd_field->field_no = field_no;

    len = mach_parse_compressed(&ptr, end_ptr);

    if (ptr == NULL) {
      return (NULL);
    }

    if (len != UNIV_SQL_NULL) {
      if (end_ptr < ptr + len) {
        return (NULL);
      }

      dfield_set_data(new_val, mem_heap_dup(heap, ptr, len), len);
      ptr += len;
    } else {
      dfield_set_null(new_val);
    }
  }

  *update_out = update;

  return (const_cast<byte *>(ptr));
}

/** Get field by field number.
@param[in]	field_no	the field number.
@return the updated field information. */
upd_field_t *upd_t::get_upd_field(ulint field_no) const {
  ulint i;
  for (i = 0; i < n_fields; i++) {
    upd_field_t *uf = upd_get_nth_field(this, i);

    if (uf->field_no == field_no) {
      return (uf);
    }
  }

  return (nullptr);
}

#ifndef UNIV_HOTBACKUP
/** Builds an update vector from those fields which in a secondary index entry
 differ from a record that has the equal ordering fields. NOTE: we compare
 the fields as binary strings!
 @return own: update vector of differing fields */
upd_t *row_upd_build_sec_rec_difference_binary(
    const rec_t *rec,      /*!< in: secondary index record */
    dict_index_t *index,   /*!< in: index */
    const ulint *offsets,  /*!< in: rec_get_offsets(rec, index) */
    const dtuple_t *entry, /*!< in: entry to insert */
    mem_heap_t *heap)      /*!< in: memory heap from which allocated */
{
  upd_field_t *upd_field;
  const dfield_t *dfield;
  const byte *data;
  ulint len;
  upd_t *update;
  ulint n_diff;
  ulint i;

  /* This function is used only for a secondary index */
  ut_a(!index->is_clustered());
  ut_ad(rec_offs_validate(rec, index, offsets));
  ut_ad(rec_offs_n_fields(offsets) == dtuple_get_n_fields(entry));
  ut_ad(!rec_offs_any_extern(offsets));
  ut_ad(!index->table->skip_alter_undo);

  update = upd_create(dtuple_get_n_fields(entry), heap);

  n_diff = 0;

  for (i = 0; i < dtuple_get_n_fields(entry); i++) {
    data = rec_get_nth_field(rec, offsets, i, &len);

    dfield = dtuple_get_nth_field(entry, i);

    /* NOTE that it may be that len != dfield_get_len(dfield) if we
    are updating in a character set and collation where strings of
    different length can be equal in an alphabetical comparison,
    and also in the case where we have a column prefix index
    and the last characters in the index field are spaces; the
    latter case probably caused the assertion failures reported at
    row0upd.cc line 713 in versions 4.0.14 - 4.0.16. */

    /* NOTE: we compare the fields as binary strings!
    (No collation) */

    if (!dfield_data_is_binary_equal(dfield, len, data)) {
      upd_field = upd_get_nth_field(update, n_diff);

      dfield_copy(&(upd_field->new_val), dfield);

      upd_field_set_field_no(upd_field, i, index, NULL);

      n_diff++;
    }
  }

  update->n_fields = n_diff;

  return (update);
}

/** Builds an update vector from those fields, excluding the roll ptr and
trx id fields, which in an index entry differ from a record that has
the equal ordering fields. NOTE: we compare the fields as binary strings!
@param[in]	index		clustered index
@param[in]	entry		clustered index entry to insert
@param[in]	rec		clustered index record
@param[in]	offsets		rec_get_offsets(rec,index), or NULL
@param[in]	no_sys		skip the system columns
                                DB_TRX_ID and DB_ROLL_PTR
@param[in]	trx		transaction (for diagnostics),
                                or NULL
@param[in]	heap		memory heap from which allocated
@param[in]	mysql_table	NULL, or mysql table object when
                                user thread invokes dml
@return own: update vector of differing fields, excluding roll ptr and
trx id */
upd_t *row_upd_build_difference_binary(dict_index_t *index,
                                       const dtuple_t *entry, const rec_t *rec,
                                       const ulint *offsets, bool no_sys,
                                       trx_t *trx, mem_heap_t *heap,
                                       TABLE *mysql_table) {
  upd_field_t *upd_field;
  dfield_t *dfield;
  const byte *data;
  ulint len;
  upd_t *update;
  ulint n_diff;
  ulint trx_id_pos;
  ulint i;
  ulint offsets_[REC_OFFS_NORMAL_SIZE];
  ulint n_fld = dtuple_get_n_fields(entry);
  ulint n_v_fld = dtuple_get_n_v_fields(entry);
  rec_offs_init(offsets_);

  /* This function is used only for a clustered index */
  ut_a(index->is_clustered());
  ut_ad(!index->table->skip_alter_undo);

  update = upd_create(n_fld + n_v_fld, heap);
  update->table = index->table;

  n_diff = 0;

  trx_id_pos = index->get_sys_col_pos(DATA_TRX_ID);
  ut_ad(index->table->is_intrinsic() ||
        (index->get_sys_col_pos(DATA_ROLL_PTR) == trx_id_pos + 1));

  if (!offsets) {
    offsets = rec_get_offsets(rec, index, offsets_, ULINT_UNDEFINED, &heap);
  } else {
    ut_ad(rec_offs_validate(rec, index, offsets));
  }

  for (i = 0; i < n_fld; i++) {
    data = rec_get_nth_field_instant(rec, offsets, i, index, &len);

    dfield = dtuple_get_nth_field(entry, i);

    /* NOTE: we compare the fields as binary strings!
    (No collation) */
    if (no_sys) {
      /* TRX_ID */
      if (i == trx_id_pos) {
        continue;
      }

      /* DB_ROLL_PTR */
      if (i == trx_id_pos + 1 && !index->table->is_intrinsic()) {
        continue;
      }
    }

    if (!dfield_is_ext(dfield) != !rec_offs_nth_extern(offsets, i) ||
        !dfield_data_is_binary_equal(dfield, len, data)) {
      upd_field = upd_get_nth_field(update, n_diff);

      dfield_copy(&(upd_field->new_val), dfield);

      upd_field_set_field_no(upd_field, i, index, trx);

      n_diff++;
    }
  }

  /* Check the virtual columns updates. Even if there is no non-virtual
  column (base columns) change, we will still need to build the
  indexed virtual column value so that undo log would log them (
  for purge/mvcc purpose) */
  if (n_v_fld > 0) {
    row_ext_t *ext;
    mem_heap_t *v_heap = NULL;
    THD *thd;

    if (trx == NULL) {
      thd = current_thd;
    } else {
      thd = trx->mysql_thd;
    }

    ut_ad(!update->old_vrow);

    for (i = 0; i < n_v_fld; i++) {
      const dict_v_col_t *col = dict_table_get_nth_v_col(index->table, i);

      if (!col->m_col.ord_part) {
        continue;
      }

      if (update->old_vrow == NULL) {
        update->old_vrow = row_build(ROW_COPY_POINTERS, index, rec, offsets,
                                     index->table, NULL, NULL, &ext, heap);
      }

      dfield = dtuple_get_nth_v_field(entry, i);

      dfield_t *vfield = innobase_get_computed_value(
          update->old_vrow, col, index, &v_heap, heap, NULL, thd, mysql_table,
          NULL, NULL, NULL);

      if (!dfield_data_is_binary_equal(dfield, vfield->len,
                                       static_cast<byte *>(vfield->data))) {
        upd_field = upd_get_nth_field(update, n_diff);

        upd_field->old_v_val = static_cast<dfield_t *>(
            mem_heap_alloc(heap, sizeof *upd_field->old_v_val));

        dfield_copy(upd_field->old_v_val, vfield);

        dfield_copy(&(upd_field->new_val), dfield);

        upd_field_set_v_field_no(upd_field, i, index);

        n_diff++;
      }
    }

    if (v_heap) {
      mem_heap_free(v_heap);
    }
  }

  update->n_fields = n_diff;
  ut_ad(update->validate());

  return (update);
}

#ifdef UNIV_DEBUG
#define row_upd_ext_fetch(clust_index, data, local_len, page_size, len,        \
                          is_sdi, heap)                                        \
  row_upd_ext_fetch_func(clust_index, data, local_len, page_size, len, is_sdi, \
                         heap)

#define row_upd_index_replace_new_col_val(index, dfield, field, col, uf, heap, \
                                          is_sdi, page_size)                   \
  row_upd_index_replace_new_col_val_func(index, dfield, field, col, uf, heap,  \
                                         is_sdi, page_size)
#else /* UNIV_DEBUG */
#define row_upd_ext_fetch(clust_index, data, local_len, page_size, len, \
                          is_sdi, heap)                                 \
  row_upd_ext_fetch_func(clust_index, data, local_len, page_size, len, heap)

#define row_upd_index_replace_new_col_val(index, dfield, field, col, uf, heap, \
                                          is_sdi, page_size)                   \
  row_upd_index_replace_new_col_val_func(index, dfield, field, col, uf, heap,  \
                                         page_size)
#endif /* UNIV_DEBUG */

/** Fetch a prefix of an externally stored column.
This is similar to row_ext_lookup(), but the row_ext_t holds the old values
of the column and must not be poisoned with the new values.
@param[in]	clust_index	the clustered index.
@param[in]	data		'internally' stored part of the field
containing also the reference to the external part
@param[in]	local_len	length of data, in bytes
@param[in]	page_size	BLOB page size
@param[in,out]	len		input - length of prefix to
fetch; output: fetched length of the prefix
@param[in]	is_sdi		true for SDI indexes
@param[in,out]	heap		heap where to allocate
@return BLOB prefix */
static byte *row_upd_ext_fetch_func(dict_index_t *clust_index, const byte *data,
                                    ulint local_len,
                                    const page_size_t &page_size, ulint *len,
#ifdef UNIV_DEBUG
                                    bool is_sdi,
#endif /* UNIV_DEBUG */
                                    mem_heap_t *heap) {
  byte *buf = static_cast<byte *>(mem_heap_alloc(heap, *len));

  *len = lob::btr_copy_externally_stored_field_prefix(
      nullptr, clust_index, buf, *len, page_size, data, is_sdi, local_len);

  /* We should never update records containing a half-deleted BLOB. */
  ut_a(*len);

  return (buf);
}

/** Replaces the new column value stored in the update vector in
the given index entry field.
@param[in]	index		index dictionary object.
@param[in,out]	dfield		data field of the index entry
@param[in]	field		index field
@param[in]	col		field->col
@param[in]	uf		update field
@param[in,out]	heap		memory heap for allocating and copying
the new value
@param[in]	is_sdi		true for SDI indexes
@param[in]	page_size	page size */
static void row_upd_index_replace_new_col_val_func(
    dict_index_t *index, dfield_t *dfield, const dict_field_t *field,
    const dict_col_t *col, const upd_field_t *uf, mem_heap_t *heap,
#ifdef UNIV_DEBUG
    bool is_sdi,
#endif /* UNIV_DEBUG */
    const page_size_t &page_size) {
  DBUG_ENTER("row_upd_index_replace_new_col_val_func");

  ulint len;
  const byte *data;

  dfield_copy_data(dfield, &uf->new_val);

  if (dfield_is_null(dfield)) {
    DBUG_VOID_RETURN;
  }

  len = dfield_get_len(dfield);
  data = static_cast<const byte *>(dfield_get_data(dfield));

  if (field->prefix_len > 0) {
    ibool fetch_ext =
        dfield_is_ext(dfield) &&
        len < (ulint)field->prefix_len + BTR_EXTERN_FIELD_REF_SIZE;

    if (fetch_ext) {
      ulint l = len;

      len = field->prefix_len;

      data = row_upd_ext_fetch(index->table->first_index(), data, l, page_size,
                               &len, is_sdi, heap);
    }

    len =
        dtype_get_at_most_n_mbchars(col->prtype, col->mbminmaxlen,
                                    field->prefix_len, len, (const char *)data);

    dfield_set_data(dfield, data, len);

    if (!fetch_ext) {
      dfield_dup(dfield, heap);
    }

    DBUG_VOID_RETURN;
  }

  switch (uf->orig_len) {
    byte *buf;
    case BTR_EXTERN_FIELD_REF_SIZE:
      /* Restore the original locally stored
      part of the column.  In the undo log,
      InnoDB writes a longer prefix of externally
      stored columns, so that column prefixes
      in secondary indexes can be reconstructed. */
      dfield_set_data(dfield, data + len - BTR_EXTERN_FIELD_REF_SIZE,
                      BTR_EXTERN_FIELD_REF_SIZE);
      dfield_set_ext(dfield);
      /* fall through */
    case 0:
      dfield_dup(dfield, heap);
      break;
    default:
      /* Reconstruct the original locally
      stored part of the column.  The data
      will have to be copied. */
      ut_a(uf->orig_len > BTR_EXTERN_FIELD_REF_SIZE);
      buf = static_cast<byte *>(mem_heap_alloc(heap, uf->orig_len));

      /* Copy the locally stored prefix. */
      memcpy(buf, data, uf->orig_len - BTR_EXTERN_FIELD_REF_SIZE);

      /* Copy the BLOB pointer. */
      memcpy(buf + uf->orig_len - BTR_EXTERN_FIELD_REF_SIZE,
             data + len - BTR_EXTERN_FIELD_REF_SIZE, BTR_EXTERN_FIELD_REF_SIZE);

      dfield_set_data(dfield, buf, uf->orig_len);
      dfield_set_ext(dfield);
      break;
  }

  DBUG_VOID_RETURN;
}

/** Replaces the new column values stored in the update vector to the index
 entry given. */
void row_upd_index_replace_new_col_vals_index_pos(
    dtuple_t *entry,     /*!< in/out: index entry where replaced;
                         the clustered index record must be
                         covered by a lock or a page latch to
                         prevent deletion (rollback or purge) */
    dict_index_t *index, /*!< in: index; NOTE that this may also be a
                         non-clustered index */
    const upd_t *update, /*!< in: an update vector built for the index so
                         that the field number in an upd_field is the
                         index position */
    ibool order_only,
    /*!< in: if TRUE, limit the replacement to
    ordering fields of index; note that this
    does not work for non-clustered indexes. */
    mem_heap_t *heap) /*!< in: memory heap for allocating and
                      copying the new values */
{
  DBUG_ENTER("row_upd_index_replace_new_col_vals_index_pos");

  ulint i;
  ulint n_fields;
  const page_size_t &page_size = dict_table_page_size(index->table);

  ut_ad(index);
  ut_ad(!index->table->skip_alter_undo);

  dtuple_set_info_bits(entry, update->info_bits);

  if (order_only) {
    n_fields = dict_index_get_n_unique(index);
  } else {
    n_fields = dict_index_get_n_fields(index);
  }

  for (i = 0; i < n_fields; i++) {
    const dict_field_t *field;
    const dict_col_t *col;
    const upd_field_t *uf;

    field = index->get_field(i);
    col = field->col;
    if (col->is_virtual()) {
      const dict_v_col_t *vcol = reinterpret_cast<const dict_v_col_t *>(col);

      uf = upd_get_field_by_field_no(update, vcol->v_pos, true);
    } else {
      uf = upd_get_field_by_field_no(update, i, false);
    }

    if (uf) {
      upd_field_t *tmp = const_cast<upd_field_t *>(uf);
      dfield_t *dfield = dtuple_get_nth_field(entry, i);
      tmp->ext_in_old = dfield_is_ext(dfield);

      dfield_copy(&tmp->old_val, dfield);

      if (dfield_is_ext(dfield)) {
        byte *data = static_cast<byte *>(dfield_get_data(dfield));
        ulint len = dfield_get_len(dfield);
        lob::ref_t ref(data + len - lob::ref_t::SIZE);
      }

      row_upd_index_replace_new_col_val(index, dfield, field, col, uf, heap,
                                        dict_index_is_sdi(index), page_size);
    }
  }

  DBUG_VOID_RETURN;
}

/** Replaces the new column values stored in the update vector to the index
 entry given. */
void row_upd_index_replace_new_col_vals(
    dtuple_t *entry,     /*!< in/out: index entry where replaced;
                         the clustered index record must be
                         covered by a lock or a page latch to
                         prevent deletion (rollback or purge) */
    dict_index_t *index, /*!< in: index; NOTE that this may also be a
                         non-clustered index */
    const upd_t *update, /*!< in: an update vector built for the
                         CLUSTERED index so that the field number in
                         an upd_field is the clustered index position */
    mem_heap_t *heap)    /*!< in: memory heap for allocating and
                         copying the new values */
{
  ulint i;
  const dict_index_t *clust_index = index->table->first_index();
  const page_size_t &page_size = dict_table_page_size(index->table);

  ut_ad(!index->table->skip_alter_undo);

  dtuple_set_info_bits(entry, update->info_bits);

  for (i = 0; i < dict_index_get_n_fields(index); i++) {
    const dict_field_t *field;
    const dict_col_t *col;
    const upd_field_t *uf;

    field = index->get_field(i);
    col = field->col;
    if (col->is_virtual()) {
      const dict_v_col_t *vcol = reinterpret_cast<const dict_v_col_t *>(col);

      uf = upd_get_field_by_field_no(update, vcol->v_pos, true);
    } else {
      uf = upd_get_field_by_field_no(
          update, dict_col_get_clust_pos(col, clust_index), false);
    }

    if (uf) {
      row_upd_index_replace_new_col_val(index, dtuple_get_nth_field(entry, i),
                                        field, col, uf, heap,
                                        dict_index_is_sdi(index), page_size);
    }
  }
}

/** Replaces the virtual column values stored in the update vector.
@param[in,out]	row	row whose column to be set
@param[in]	field	data to set
@param[in]	len	data length
@param[in]	vcol	virtual column info */
static void row_upd_set_vcol_data(dtuple_t *row, const byte *field, ulint len,
                                  dict_v_col_t *vcol) {
  dfield_t *dfield = dtuple_get_nth_v_field(row, vcol->v_pos);

  if (dfield_get_type(dfield)->mtype == DATA_MISSING) {
    vcol->m_col.copy_type(dfield_get_type(dfield));

    dfield_set_data(dfield, field, len);
  }
}

/** Replaces the virtual column values stored in a dtuple with that of
a update vector.
@param[in,out]	row	row whose column to be updated
@param[in]	table	table
@param[in]	update	an update vector built for the clustered index
@param[in]	upd_new	update to new or old value
@param[in,out]	undo_row undo row (if needs to be updated)
@param[in]	ptr	remaining part in update undo log */
void row_upd_replace_vcol(dtuple_t *row, const dict_table_t *table,
                          const upd_t *update, bool upd_new, dtuple_t *undo_row,
                          const byte *ptr) {
  ulint col_no;
  ulint i;
  ulint n_cols;

  ut_ad(!table->skip_alter_undo);

  n_cols = dtuple_get_n_v_fields(row);
  for (col_no = 0; col_no < n_cols; col_no++) {
    dfield_t *dfield;

    const dict_v_col_t *col = dict_table_get_nth_v_col(table, col_no);

    /* If there is no index on the column, do not bother for
    value update */
    if (!col->m_col.ord_part) {
      const dict_index_t *clust_index = table->first_index();

      /* Skip the column if there is no online alter
      table in progress or it is not being indexed
      in new table */
      if (!dict_index_is_online_ddl(clust_index) ||
          !row_log_col_is_indexed(clust_index, col_no)) {
        continue;
      }
    }

    dfield = dtuple_get_nth_v_field(row, col_no);

    for (i = 0; i < upd_get_n_fields(update); i++) {
      const upd_field_t *upd_field = upd_get_nth_field(update, i);
      if (!upd_fld_is_virtual_col(upd_field) ||
          upd_field->field_no != col->v_pos) {
        continue;
      }

      if (upd_new) {
        dfield_copy_data(dfield, &upd_field->new_val);
      } else {
        dfield_copy_data(dfield, upd_field->old_v_val);
      }

      dfield_get_type(dfield)->mtype = upd_field->new_val.type.mtype;
      dfield_get_type(dfield)->prtype = upd_field->new_val.type.prtype;
      dfield_get_type(dfield)->mbminmaxlen =
          upd_field->new_val.type.mbminmaxlen;
      break;
    }
  }

  bool first_v_col = true;
  bool is_undo_log = true;

  /* We will read those unchanged (but indexed) virtual columns in */
  if (ptr != NULL) {
    const byte *end_ptr;

    end_ptr = ptr + mach_read_from_2(ptr);
    ptr += 2;

    while (ptr != end_ptr) {
      const byte *field;
      ulint field_no;
      ulint len;
      ulint orig_len;
      bool is_v;

      field_no = mach_read_next_compressed(&ptr);

      is_v = (field_no >= REC_MAX_N_FIELDS);

      if (is_v) {
        ptr = trx_undo_read_v_idx(table, ptr, first_v_col, &is_undo_log,
                                  &field_no);
        first_v_col = false;
      }

      ptr = trx_undo_rec_get_col_val(ptr, &field, &len, &orig_len);

      if (field_no == ULINT_UNDEFINED) {
        ut_ad(is_v);
        continue;
      }

      if (is_v) {
        dict_v_col_t *vcol = dict_table_get_nth_v_col(table, field_no);

        row_upd_set_vcol_data(row, field, len, vcol);

        if (undo_row) {
          row_upd_set_vcol_data(undo_row, field, len, vcol);
        }
      }
      ut_ad(ptr <= end_ptr);
    }
  }
}

/** Replaces the new column values stored in the update vector. */
void row_upd_replace(
    trx_t *trx,                /*!< in: transaction object. */
    dtuple_t *row,             /*!< in/out: row where replaced,
                               indexed by col_no;
                               the clustered index record must be
                               covered by a lock or a page latch to
                               prevent deletion (rollback or purge) */
    row_ext_t **ext,           /*!< out, own: NULL, or externally
                               stored column prefixes */
    const dict_index_t *index, /*!< in: clustered index */
    const upd_t *update,       /*!< in: an update vector built for the
                               clustered index */
    mem_heap_t *heap)          /*!< in: memory heap */
{
  ulint col_no;
  ulint i;
  ulint n_cols;
  ulint n_ext_cols;
  ulint *ext_cols;
  const dict_table_t *table;

  ut_ad(row);
  ut_ad(ext);
  ut_ad(index);
  ut_ad(index->is_clustered());
  ut_ad(update);
  ut_ad(heap);
  ut_ad(update->validate());
  ut_ad(!index->table->skip_alter_undo);

  n_cols = dtuple_get_n_fields(row);
  table = index->table;
  ut_ad(n_cols == table->get_n_cols());

  ext_cols =
      static_cast<ulint *>(mem_heap_alloc(heap, n_cols * sizeof *ext_cols));

  n_ext_cols = 0;

  dtuple_set_info_bits(row, update->info_bits);

  for (col_no = 0; col_no < n_cols; col_no++) {
    const dict_col_t *col = table->get_col(col_no);
    const ulint clust_pos = dict_col_get_clust_pos(col, index);
    dfield_t *dfield;

    if (UNIV_UNLIKELY(clust_pos == ULINT_UNDEFINED)) {
      continue;
    }

    dfield = dtuple_get_nth_field(row, col_no);

    for (i = 0; i < upd_get_n_fields(update); i++) {
      const upd_field_t *upd_field = upd_get_nth_field(update, i);

      if (upd_field->field_no != clust_pos ||
          upd_fld_is_virtual_col(upd_field)) {
        continue;
      }

      dfield_copy_data(dfield, &upd_field->new_val);
      break;
    }

    if (dfield_is_ext(dfield) && col->ord_part) {
      ext_cols[n_ext_cols++] = col_no;
    }
  }

  if (n_ext_cols) {
    *ext = row_ext_create(index, n_ext_cols, ext_cols, table->flags, row,
                          dict_index_is_sdi(index), heap);
  } else {
    *ext = NULL;
  }

  row_upd_replace_vcol(row, table, update, true, NULL, NULL);
}

/** Checks if an update vector changes an ordering field of an index record.

 This function is fast if the update vector is short or the number of ordering
 fields in the index is small. Otherwise, this can be quadratic.
 NOTE: we compare the fields as binary strings!
 @return true if update vector changes an ordering field in the index record */
ibool row_upd_changes_ord_field_binary_func(
    dict_index_t *index, /*!< in: index of the record */
    const upd_t *update, /*!< in: update vector for the row; NOTE: the
                         field numbers in this MUST be clustered index
                         positions! */
#ifdef UNIV_DEBUG
    const que_thr_t *thr, /*!< in: query thread, or NULL */
#endif                    /* UNIV_DEBUG */
    const dtuple_t *row,  /*!< in: old value of row, or NULL if the
                          row and the data values in update are not
                          known when this function is called, e.g., at
                          compile time */
    const row_ext_t *ext, /*!< NULL, or prefixes of the externally
                          stored columns in the old row */
    ulint flag)           /*!< in: ROW_BUILD_NORMAL,
                          ROW_BUILD_FOR_PURGE or ROW_BUILD_FOR_UNDO */
{
  ulint n_unique;
  ulint i;
  const dict_index_t *clust_index;

  ut_ad(index);
  ut_ad(update);

  ut_ad(!index->table->skip_alter_undo);

  n_unique = dict_index_get_n_unique(index);

  clust_index = index->table->first_index();

  for (i = 0; i < n_unique; i++) {
    const dict_field_t *ind_field;
    const dict_col_t *col;
    ulint col_no;
    const upd_field_t *upd_field;
    const dfield_t *dfield;
    dfield_t dfield_ext;
    ulint dfield_len;
    const byte *buf;
    const dict_v_col_t *vcol = NULL;

    ind_field = index->get_field(i);
    col = ind_field->col;
    col_no = dict_col_get_no(col);

    if (col->is_virtual()) {
      vcol = reinterpret_cast<const dict_v_col_t *>(col);

      upd_field = upd_get_field_by_field_no(update, vcol->v_pos, true);
    } else {
      upd_field = upd_get_field_by_field_no(
          update, dict_col_get_clust_pos(col, clust_index), false);
    }

    if (upd_field == NULL) {
      continue;
    }

    if (row == NULL) {
      ut_ad(ext == NULL);
      return (TRUE);
    }

    if (col->is_virtual()) {
      dfield = dtuple_get_nth_v_field(row, vcol->v_pos);
    } else {
      dfield = dtuple_get_nth_field(row, col_no);
    }

    /* For spatial index update, since the different geometry
    data could generate same MBR, so, if the new index entry is
    same as old entry, which means the MBR is not changed, we
    don't need to do anything. */
    if (dict_index_is_spatial(index) && i == 0) {
      double mbr1[SPDIMS * 2];
      double mbr2[SPDIMS * 2];
      rtr_mbr_t *old_mbr;
      rtr_mbr_t *new_mbr;
      uchar *dptr = NULL;
      ulint flen = 0;
      ulint dlen = 0;
      mem_heap_t *temp_heap = NULL;
      const dfield_t *new_field = &upd_field->new_val;

      const page_size_t page_size =
          (ext != NULL) ? ext->page_size : dict_table_page_size(index->table);

      ut_ad(dfield->data != NULL && dfield->len > GEO_DATA_HEADER_SIZE);
      ut_ad(col->get_spatial_status() != SPATIAL_NONE);

      /* Get the old mbr. */
      if (dfield_is_ext(dfield)) {
        /* For off-page stored data, we
        need to read the whole field data. */
        flen = dfield_get_len(dfield);
        dptr = static_cast<byte *>(dfield_get_data(dfield));
        temp_heap = mem_heap_create(1000);

        const dict_index_t *clust_index =
            (ext == nullptr ? index->table->first_index() : ext->index);
        dptr = lob::btr_copy_externally_stored_field(
            nullptr, clust_index, &dlen, nullptr, dptr, page_size, flen, false,
            temp_heap);
      } else {
        dptr = static_cast<uchar *>(dfield->data);
        dlen = dfield->len;
      }

      uint32_t srid;

      get_mbr_from_store(index->rtr_srs.get(), dptr, static_cast<uint>(dlen),
                         SPDIMS, mbr1, &srid);
      old_mbr = reinterpret_cast<rtr_mbr_t *>(mbr1);

      /* Get the new mbr. */
      if (dfield_is_ext(new_field)) {
        if (flag == ROW_BUILD_FOR_UNDO &&
            dict_table_has_atomic_blobs(index->table)) {
          /* For undo, and the table is
          Barrcuda, we need to skip the
          prefix data. */
          flen = BTR_EXTERN_FIELD_REF_SIZE;
          ut_ad(dfield_get_len(new_field) >= BTR_EXTERN_FIELD_REF_SIZE);
          dptr = static_cast<byte *>(dfield_get_data(new_field)) +
                 dfield_get_len(new_field) - BTR_EXTERN_FIELD_REF_SIZE;
        } else {
          flen = dfield_get_len(new_field);
          dptr = static_cast<byte *>(dfield_get_data(new_field));
        }

        if (temp_heap == NULL) {
          temp_heap = mem_heap_create(1000);
        }

        const dict_index_t *clust_index =
            (ext == nullptr ? index->table->first_index() : ext->index);
        dptr = lob::btr_copy_externally_stored_field(
            nullptr, clust_index, &dlen, nullptr, dptr, page_size, flen,
            dict_table_is_sdi(index->table->id), temp_heap);
      } else {
        dptr = static_cast<uchar *>(upd_field->new_val.data);
        dlen = upd_field->new_val.len;
      }

      uint32_t new_srid;
      get_mbr_from_store(index->rtr_srs.get(), dptr, static_cast<uint>(dlen),
                         SPDIMS, mbr2, &new_srid);

      new_mbr = reinterpret_cast<rtr_mbr_t *>(mbr2);

      if (new_srid != srid) {
        return true;
      }

      if (temp_heap) {
        mem_heap_free(temp_heap);
      }

      if (!mbr_equal_cmp(index->rtr_srs.get(), old_mbr, new_mbr)) {
        return (TRUE);
      } else {
        continue;
      }
    }

    /* This treatment of column prefix indexes is loosely
    based on row_build_index_entry(). */

    if (UNIV_LIKELY(ind_field->prefix_len == 0) || dfield_is_null(dfield)) {
      /* do nothing special */
    } else if (ext) {
      /* Silence a compiler warning without
      silencing a Valgrind error. */
      dfield_len = 0;
      UNIV_MEM_INVALID(&dfield_len, sizeof dfield_len);
      /* See if the column is stored externally. */
      buf = row_ext_lookup(ext, col_no, &dfield_len);

      ut_ad(col->ord_part);

      if (UNIV_LIKELY_NULL(buf)) {
        if (UNIV_UNLIKELY(buf == field_ref_zero)) {
          /* The externally stored field
          was not written yet. This
          record should only be seen by
          trx_rollback_or_clean_all_recovered(),
          when the server had crashed before
          storing the field. */
          ut_ad(thr == NULL || thr->graph->trx->is_recovered);
          ut_ad(thr == NULL || trx_is_recv(thr->graph->trx));
          return (TRUE);
        }

        goto copy_dfield;
      }
    } else if (dfield_is_ext(dfield)) {
      dfield_len = dfield_get_len(dfield);
      ut_a(dfield_len > BTR_EXTERN_FIELD_REF_SIZE);
      dfield_len -= BTR_EXTERN_FIELD_REF_SIZE;
      ut_a(index->is_clustered() || ind_field->prefix_len <= dfield_len);

      buf = static_cast<byte *>(dfield_get_data(dfield));
    copy_dfield:
      ut_a(dfield_len > 0);
      dfield_copy(&dfield_ext, dfield);
      dfield_set_data(&dfield_ext, buf, dfield_len);
      dfield = &dfield_ext;
    }

    if (!dfield_datas_are_binary_equal(dfield, &upd_field->new_val,
                                       ind_field->prefix_len)) {
      return (TRUE);
    }
  }

  return (FALSE);
}

/** Checks if an update vector changes an ordering field of an index record.
 NOTE: we compare the fields as binary strings!
 @return true if update vector may change an ordering field in an index
 record */
ibool row_upd_changes_some_index_ord_field_binary(
    const dict_table_t *table, /*!< in: table */
    const upd_t *update)       /*!< in: update vector for the row */
{
  upd_field_t *upd_field;
  const dict_index_t *index;
  ulint i;

  index = table->first_index();

  for (i = 0; i < upd_get_n_fields(update); i++) {
    upd_field = upd_get_nth_field(update, i);

    if (upd_fld_is_virtual_col(upd_field)) {
      if (dict_table_get_nth_v_col(index->table, upd_field->field_no)
              ->m_col.ord_part) {
        return (TRUE);
      }
    } else {
      if (index->get_field(upd_field->field_no)->col->ord_part) {
        return (TRUE);
      }
    }
  }

  return (FALSE);
}

/** Checks if an FTS Doc ID column is affected by an UPDATE.
 @return whether the Doc ID column is changed */
bool row_upd_changes_doc_id(dict_table_t *table,    /*!< in: table */
                            upd_field_t *upd_field) /*!< in: field to check */
{
  ulint col_no;
  dict_index_t *clust_index;
  fts_t *fts = table->fts;

  ut_ad(!table->skip_alter_undo);

  clust_index = table->first_index();

  /* Convert from index-specific column number to table-global
  column number. */
  col_no = clust_index->get_col_no(upd_field->field_no);

  return (col_no == fts->doc_col);
}
/** Checks if an FTS indexed column is affected by an UPDATE.
 @return offset within fts_t::indexes if FTS indexed column updated else
 ULINT_UNDEFINED */
ulint row_upd_changes_fts_column(
    dict_table_t *table,    /*!< in: table */
    upd_field_t *upd_field) /*!< in: field to check */
{
  ulint col_no;
  dict_index_t *clust_index;
  fts_t *fts = table->fts;

  ut_ad(!table->skip_alter_undo);

  if (upd_fld_is_virtual_col(upd_field)) {
    col_no = upd_field->field_no;
    return (dict_table_is_fts_column(fts->indexes, col_no, true));
  } else {
    clust_index = table->first_index();

    /* Convert from index-specific column number to table-global
    column number. */
    col_no = clust_index->get_col_no(upd_field->field_no);
    return (dict_table_is_fts_column(fts->indexes, col_no, false));
  }
}

/** Checks if an update vector changes some of the first ordering fields of an
 index record. This is only used in foreign key checks and we can assume
 that index does not contain column prefixes.
 @return true if changes */
static ibool row_upd_changes_first_fields_binary(
    dtuple_t *entry,     /*!< in: index entry */
    dict_index_t *index, /*!< in: index of entry */
    const upd_t *update, /*!< in: update vector for the row */
    ulint n)             /*!< in: how many first fields to check */
{
  ulint n_upd_fields;
  ulint i, j;
  dict_index_t *clust_index;

  ut_ad(update && index);
  ut_ad(n <= dict_index_get_n_fields(index));

  n_upd_fields = upd_get_n_fields(update);
  clust_index = index->table->first_index();

  for (i = 0; i < n; i++) {
    const dict_field_t *ind_field;
    const dict_col_t *col;
    ulint col_pos;

    ind_field = index->get_field(i);
    col = ind_field->col;
    col_pos = dict_col_get_clust_pos(col, clust_index);

    ut_a(ind_field->prefix_len == 0);

    for (j = 0; j < n_upd_fields; j++) {
      upd_field_t *upd_field = upd_get_nth_field(update, j);

      if (col_pos == upd_field->field_no &&
          !dfield_datas_are_binary_equal(dtuple_get_nth_field(entry, i),
                                         &upd_field->new_val, 0)) {
        return (TRUE);
      }
    }
  }

  return (FALSE);
}

/** Copies the column values from a record.
@param[in]	rec	record in a clustered index
@param[in]	offsets	array returned by rec_get_offsets()
@param[in]	index	clustered index where record resides
@param[in]	column	first column in a column list, or nullptr */
UNIV_INLINE
void row_upd_copy_columns(rec_t *rec, const ulint *offsets,
                          const dict_index_t *index, sym_node_t *column) {
  const byte *data;
  ulint len;

  ut_ad(index->is_clustered());

  while (column) {
    data = rec_get_nth_field_instant(
        rec, offsets, column->field_nos[SYM_CLUST_FIELD_NO], index, &len);
    eval_node_copy_and_alloc_val(column, data, len);

    column = UT_LIST_GET_NEXT(col_var_list, column);
  }
}

/** Calculates the new values for fields to update. Note that
 row_upd_copy_columns must have been called first. */
UNIV_INLINE
void row_upd_eval_new_vals(upd_t *update) /*!< in/out: update vector */
{
  que_node_t *exp;
  upd_field_t *upd_field;
  ulint n_fields;
  ulint i;

  n_fields = upd_get_n_fields(update);

  for (i = 0; i < n_fields; i++) {
    upd_field = upd_get_nth_field(update, i);

    exp = upd_field->exp;

    eval_exp(exp);

    dfield_copy_data(&(upd_field->new_val), que_node_get_val(exp));
  }
}

/** Stores to the heap the virtual columns that need for any indexes
@param[in,out]	node		row update node
@param[in]	update		an update vector if it is update
@param[in]	thd		mysql thread handle
@param[in,out]	mysql_table	mysql table object */
static void row_upd_store_v_row(upd_node_t *node, const upd_t *update, THD *thd,
                                TABLE *mysql_table) {
  mem_heap_t *heap = NULL;
  dict_index_t *index = node->table->first_index();

  for (ulint col_no = 0; col_no < dict_table_get_n_v_cols(node->table);
       col_no++) {
    const dict_v_col_t *col = dict_table_get_nth_v_col(node->table, col_no);

    if (col->m_col.ord_part) {
      dfield_t *dfield = dtuple_get_nth_v_field(node->row, col_no);
      ulint n_upd = update ? upd_get_n_fields(update) : 0;
      ulint i = 0;

      /* Check if the value is already in update vector */
      for (i = 0; i < n_upd; i++) {
        const upd_field_t *upd_field = upd_get_nth_field(update, i);
        if (!(upd_field->new_val.type.prtype & DATA_VIRTUAL) ||
            upd_field->field_no != col->v_pos) {
          continue;
        }

        dfield_copy_data(dfield, upd_field->old_v_val);
        break;
      }

      /* Not updated */
      if (i >= n_upd) {
        /* If this is an update, then the value
        should be in update->old_vrow */
        if (update) {
          if (update->old_vrow == NULL) {
            /* This only happens in
            cascade update. And virtual
            column can't be affected,
            so it is Ok to set it to NULL */
            dfield_set_null(dfield);
          } else {
            dfield_t *vfield = dtuple_get_nth_v_field(update->old_vrow, col_no);
            dfield_copy_data(dfield, vfield);
            dfield_dup(dfield, node->heap);
          }
        } else {
          /* Need to compute, this happens when
          deleting row */
          innobase_get_computed_value(node->row, col, index, &heap, node->heap,
                                      NULL, thd, mysql_table, NULL, NULL, NULL);
        }
      }
    }
  }

  if (heap) {
    mem_heap_free(heap);
  }
}

/** Stores to the heap the row on which the node->pcur is positioned.
@param[in]	trx		the transaction object
@param[in]	node		row update node
@param[in]	thd		mysql thread handle
@param[in,out]	mysql_table	NULL, or mysql table object when
                                user thread invokes dml */
void row_upd_store_row(trx_t *trx, upd_node_t *node, THD *thd,
                       TABLE *mysql_table) {
  dict_index_t *clust_index;
  rec_t *rec;
  mem_heap_t *heap = NULL;
  row_ext_t **ext;
  ulint offsets_[REC_OFFS_NORMAL_SIZE];
  const ulint *offsets;
  rec_offs_init(offsets_);

  ut_ad(node->pcur->latch_mode != BTR_NO_LATCHES);

  if (node->row != NULL) {
    mem_heap_empty(node->heap);
  }

  clust_index = node->table->first_index();

  rec = btr_pcur_get_rec(node->pcur);

  offsets = rec_get_offsets(rec, clust_index, offsets_, ULINT_UNDEFINED, &heap);

  if (dict_table_has_atomic_blobs(node->table)) {
    /* There is no prefix of externally stored columns in
    the clustered index record. Build a cache of column
    prefixes. */
    ext = &node->ext;
  } else {
    /* REDUNDANT and COMPACT formats store a local
    768-byte prefix of each externally stored column.
    No cache is needed. */
    ext = NULL;
    node->ext = NULL;
  }

  node->row = row_build(ROW_COPY_DATA, clust_index, rec, offsets, NULL, NULL,
                        NULL, ext, node->heap);

  if (node->table->n_v_cols) {
    row_upd_store_v_row(node, node->is_delete ? NULL : node->update, thd,
                        mysql_table);
  }

  if (node->is_delete) {
    node->upd_row = NULL;
    node->upd_ext = NULL;
  } else {
    node->upd_row = dtuple_copy(node->row, node->heap);
    row_upd_replace(trx, node->upd_row, &node->upd_ext, clust_index,
                    node->update, node->heap);
  }

  if (UNIV_LIKELY_NULL(heap)) {
    mem_heap_free(heap);
  }
}

/** Print a MBR data from disk */
static void srv_mbr_print(const byte *data) {
  double a, b, c, d;
  a = mach_double_read(data);
  data += sizeof(double);
  b = mach_double_read(data);
  data += sizeof(double);
  c = mach_double_read(data);
  data += sizeof(double);
  d = mach_double_read(data);

  ib::info(ER_IB_MSG_1043) << "GIS MBR INFO: " << a << " and " << b << ", " << c
                           << ", " << d << "\n";
}

/** Updates a secondary index entry of a row.
 @return DB_SUCCESS if operation successfully completed, else error
 code or DB_LOCK_WAIT */
static MY_ATTRIBUTE((warn_unused_result)) dberr_t
    row_upd_sec_index_entry(upd_node_t *node, /*!< in: row update node */
                            que_thr_t *thr)   /*!< in: query thread */
{
  mtr_t mtr;
  const rec_t *rec;
  btr_pcur_t pcur;
  mem_heap_t *heap;
  dtuple_t *entry;
  dict_index_t *index;
  btr_cur_t *btr_cur;
  ibool referenced;
  dberr_t err = DB_SUCCESS;
  trx_t *trx = thr_get_trx(thr);
  ulint mode;
  ulint flags = 0;
  enum row_search_result search_result;

  ut_ad(trx->id != 0);

  index = node->index;

  referenced = row_upd_index_is_referenced(index, trx);

  heap = mem_heap_create(1024);

  if (!node->is_delete && dict_index_is_spatial(index) &&
      index->srid_is_valid) {
    const dict_col_t *col = index->get_field(0)->col;
    ulint col_no = dict_col_get_no(col);
    const dfield_t *dfield = dtuple_get_nth_field(node->upd_row, col_no);
    uchar *dptr = static_cast<uchar *>(dfield_get_data(dfield));
    uint32_t srid = uint4korr(dptr);
    if (index->srid != srid) {
      return DB_CANT_CREATE_GEOMETRY_OBJECT;
    }
  }

  /* Build old index entry */
  entry = row_build_index_entry(node->row, node->ext, index, heap);
  ut_a(entry);

  if (!index->table->is_intrinsic()) {
    log_free_check();
  }

  DEBUG_SYNC_C_IF_THD(trx->mysql_thd, "before_row_upd_sec_index_entry");

  mtr_start(&mtr);

  /* Disable REDO logging as lifetime of temp-tables is limited to
  server or connection lifetime and so REDO information is not needed
  on restart for recovery.
  Disable locking as temp-tables are not shared across connection. */
  if (index->table->is_temporary()) {
    flags |= BTR_NO_LOCKING_FLAG;
    mtr.set_log_mode(MTR_LOG_NO_REDO);

    if (index->table->is_intrinsic()) {
      flags |= BTR_NO_UNDO_LOG_FLAG;
    }
  }

  if (!index->is_committed()) {
    /* The index->online_status may change if the index is
    or was being created online, but not committed yet. It
    is protected by index->lock. */

    mtr_s_lock(dict_index_get_lock(index), &mtr);

    switch (dict_index_get_online_status(index)) {
      case ONLINE_INDEX_COMPLETE:
        /* This is a normal index. Do not log anything.
        Perform the update on the index tree directly. */
        break;
      case ONLINE_INDEX_CREATION:
        /* Log a DELETE and optionally INSERT. */
        row_log_online_op(index, entry, 0);

        if (!node->is_delete) {
          mem_heap_empty(heap);
          entry =
              row_build_index_entry(node->upd_row, node->upd_ext, index, heap);
          ut_a(entry);
          row_log_online_op(index, entry, trx->id);
        }
        /* fall through */
      case ONLINE_INDEX_ABORTED:
      case ONLINE_INDEX_ABORTED_DROPPED:
        mtr_commit(&mtr);
        goto func_exit;
    }

    /* We can only buffer delete-mark operations if there
    are no foreign key constraints referring to the index.
    Change buffering is disabled for temporary tables and
    spatial index. */
    mode = (referenced || index->table->is_temporary() ||
            dict_index_is_spatial(index))
               ? BTR_MODIFY_LEAF | BTR_ALREADY_S_LATCHED
               : BTR_MODIFY_LEAF | BTR_ALREADY_S_LATCHED | BTR_DELETE_MARK;
  } else {
    /* For secondary indexes,
    index->online_status==ONLINE_INDEX_COMPLETE if
    index->is_committed(). */
    ut_ad(!dict_index_is_online_ddl(index));

    /* We can only buffer delete-mark operations if there
    are no foreign key constraints referring to the index.
    Change buffering is disabled for temporary tables and
    spatial index. */
    mode = (referenced || index->table->is_temporary() ||
            dict_index_is_spatial(index))
               ? BTR_MODIFY_LEAF
               : BTR_MODIFY_LEAF | BTR_DELETE_MARK;
  }

  if (dict_index_is_spatial(index)) {
    ut_ad(mode & BTR_MODIFY_LEAF);
    mode |= BTR_RTREE_DELETE_MARK;
  }

  /* Set the query thread, so that ibuf_insert_low() will be
  able to invoke thd_get_trx(). */
  btr_pcur_get_btr_cur(&pcur)->thr = thr;

  search_result = row_search_index_entry(index, entry, mode, &pcur, &mtr);

  btr_cur = btr_pcur_get_btr_cur(&pcur);

  rec = btr_cur_get_rec(btr_cur);

  switch (search_result) {
    case ROW_NOT_DELETED_REF: /* should only occur for BTR_DELETE */
      ut_error;
      break;
    case ROW_BUFFERED:
      /* Entry was delete marked already. */
      break;

    case ROW_NOT_FOUND:
      if (!index->is_committed()) {
        /* When online CREATE INDEX copied the update
        that we already made to the clustered index,
        and completed the secondary index creation
        before we got here, the old secondary index
        record would not exist. The CREATE INDEX
        should be waiting for a MySQL meta-data lock
        upgrade at least until this UPDATE returns.
        After that point, set_committed(true) would be
        invoked by commit_inplace_alter_table(). */
        break;
      }

      if (dict_index_is_spatial(index) && btr_cur->rtr_info->fd_del) {
        /* We found the record, but a delete marked */
        break;
      }

      ib::error(ER_IB_MSG_1044)
          << "Record in index " << index->name << " of table "
          << index->table->name << " was not found on update: " << *entry
          << " at: " << rec_index_print(rec, index);
      srv_mbr_print((unsigned char *)entry->fields[0].data);
#ifdef UNIV_DEBUG
      mtr_commit(&mtr);
      mtr_start(&mtr);
      ut_ad(btr_validate_index(index, 0, false));
      ut_ad(0);
#endif /* UNIV_DEBUG */
      break;
    case ROW_FOUND:
      ut_ad(err == DB_SUCCESS);

      /* Delete mark the old index record; it can already be
      delete marked if we return after a lock wait in
      row_ins_sec_index_entry() below */
      if (!rec_get_deleted_flag(rec, dict_table_is_comp(index->table))) {
        err = btr_cur_del_mark_set_sec_rec(flags, btr_cur, TRUE, thr, &mtr);
        if (err != DB_SUCCESS) {
          break;
        }
      }

      ut_ad(err == DB_SUCCESS);

      if (referenced) {
        ulint *offsets;

        offsets = rec_get_offsets(rec, index, NULL, ULINT_UNDEFINED, &heap);

        /* NOTE that the following call loses
        the position of pcur ! */
        err = row_upd_check_references_constraints(node, &pcur, index->table,
                                                   index, offsets, thr, &mtr);
      }
      break;
  }

  btr_pcur_close(&pcur);
  mtr_commit(&mtr);

  if (node->is_delete || err != DB_SUCCESS) {
    goto func_exit;
  }

  mem_heap_empty(heap);

  /* Build a new index entry */
  entry = row_build_index_entry(node->upd_row, node->upd_ext, index, heap);
  ut_a(entry);

  /* Insert new index entry */
  err = row_ins_sec_index_entry(index, entry, thr, false);

func_exit:
  mem_heap_free(heap);

  return (err);
}

/** Updates the secondary index record if it is changed in the row update or
 deletes it if this is a delete.
 @return DB_SUCCESS if operation successfully completed, else error
 code or DB_LOCK_WAIT */
static MY_ATTRIBUTE((warn_unused_result)) dberr_t
    row_upd_sec_step(upd_node_t *node, /*!< in: row update node */
                     que_thr_t *thr)   /*!< in: query thread */
{
  ut_ad((node->state == UPD_NODE_UPDATE_ALL_SEC) ||
        (node->state == UPD_NODE_UPDATE_SOME_SEC));
  ut_ad(!node->index->is_clustered());

  if (node->state == UPD_NODE_UPDATE_ALL_SEC ||
      row_upd_changes_ord_field_binary(node->index, node->update, thr,
                                       node->row, node->ext)) {
    return (row_upd_sec_index_entry(node, thr));
  }

  return (DB_SUCCESS);
}

#ifdef UNIV_DEBUG
#define row_upd_clust_rec_by_insert_inherit(rec, offsets, entry, update) \
  row_upd_clust_rec_by_insert_inherit_func(rec, offsets, entry, update)
#else /* UNIV_DEBUG */
#define row_upd_clust_rec_by_insert_inherit(rec, offsets, entry, update) \
  row_upd_clust_rec_by_insert_inherit_func(rec, entry, update)
#endif /* UNIV_DEBUG */
/** Mark non-updated off-page columns inherited when the primary key is
 updated. We must mark them as inherited in entry, so that they are not
 freed in a rollback. A limited version of this function used to be
 called btr_cur_mark_dtuple_inherited_extern().
 @return whether any columns were inherited */
static bool row_upd_clust_rec_by_insert_inherit_func(
    const rec_t *rec, /*!< in: old record, or NULL */
#ifdef UNIV_DEBUG
    const ulint *offsets, /*!< in: rec_get_offsets(rec), or NULL */
#endif                    /* UNIV_DEBUG */
    dtuple_t *entry,      /*!< in/out: updated entry to be
                          inserted into the clustered index */
    const upd_t *update)  /*!< in: update vector */
{
  bool inherit = false;
  ulint i;

  ut_ad(!rec == !offsets);
  ut_ad(!rec || rec_offs_any_extern(offsets));

  for (i = 0; i < dtuple_get_n_fields(entry); i++) {
    dfield_t *dfield = dtuple_get_nth_field(entry, i);
    ulint len;

    ut_ad(!offsets ||
          !rec_offs_nth_extern(offsets, i) == !dfield_is_ext(dfield) ||
          upd_get_field_by_field_no(update, i, false));
    if (!dfield_is_ext(dfield) || upd_get_field_by_field_no(update, i, false)) {
      continue;
    }

    lob::ref_t ref(dfield->blobref());

#ifdef UNIV_DEBUG
    if (UNIV_LIKELY(rec != NULL)) {
      const byte *rec_data = rec_get_nth_field(rec, offsets, i, &len);
      ut_ad(len == dfield_get_len(dfield));
      ut_ad(len != UNIV_SQL_NULL);
      ut_ad(len >= BTR_EXTERN_FIELD_REF_SIZE);

      rec_data += len - BTR_EXTERN_FIELD_REF_SIZE;

      /* The pointer must not be zero. */
      ut_ad(!ref.is_null());

      /* The BLOB must be owned. */
      ut_ad(ref.is_owner());
    }
#endif /* UNIV_DEBUG */

    len = dfield_get_len(dfield);
    ut_a(len != UNIV_SQL_NULL);
    ut_a(len >= BTR_EXTERN_FIELD_REF_SIZE);

    /* The pointer must not be zero. */
    ut_a(!ref.is_null());

    /* The BLOB must be owned, unless we are resuming from
    a lock wait and we already had disowned the BLOB. */
    ut_a(rec == NULL || ref.is_owner());

    ref.set_owner(true, NULL);
    ref.set_inherited(true, NULL);

    /* The BTR_EXTERN_INHERITED_FLAG only matters in
    rollback of a fresh insert (insert_undo log).
    Purge (operating on update_undo log) will always free
    the extern fields of a delete-marked row. */

    inherit = true;
  }

  return (inherit);
}

/** Marks the clustered index record deleted and inserts the updated version
 of the record to the index. This function should be used when the ordering
 fields of the clustered index record change. This should be quite rare in
 database applications.
 @return DB_SUCCESS if operation successfully completed, else error
 code or DB_LOCK_WAIT */
static MY_ATTRIBUTE((warn_unused_result)) dberr_t row_upd_clust_rec_by_insert(
    ulint flags,         /*!< in: undo logging and locking flags */
    upd_node_t *node,    /*!< in/out: row update node */
    dict_index_t *index, /*!< in: clustered index of the record */
    que_thr_t *thr,      /*!< in: query thread */
    ibool referenced,    /*!< in: TRUE if index may be referenced in
                      a foreign key constraint */
    mtr_t *mtr)          /*!< in/out: mtr; gets committed here */
{
  mem_heap_t *heap;
  btr_pcur_t *pcur;
  btr_cur_t *btr_cur;
  trx_t *trx;
  dict_table_t *table;
  dtuple_t *entry;
  dberr_t err;
  rec_t *rec;
  ulint *offsets = NULL;

  ut_ad(node);
  ut_ad(index->is_clustered());

  trx = thr_get_trx(thr);
  table = node->table;
  pcur = node->pcur;
  btr_cur = btr_pcur_get_btr_cur(pcur);

  heap = mem_heap_create(1000);

  entry = row_build_index_entry_low(node->upd_row, node->upd_ext, index, heap,
                                    ROW_BUILD_FOR_INSERT);
  ut_ad(dtuple_get_info_bits(entry) == 0);

  row_upd_index_entry_sys_field(entry, index, DATA_TRX_ID, trx->id);

  switch (node->state) {
    default:
      ut_error;
    case UPD_NODE_INSERT_CLUSTERED:
      /* A lock wait occurred in row_ins_clust_index_entry() in
      the previous invocation of this function. */
      row_upd_clust_rec_by_insert_inherit(NULL, NULL, entry, node->update);
      break;
    case UPD_NODE_UPDATE_CLUSTERED:
      /* This is the first invocation of the function where
      we update the primary key.  Delete-mark the old record
      in the clustered index and prepare to insert a new entry. */
      rec = btr_cur_get_rec(btr_cur);
      offsets = rec_get_offsets(rec, index, NULL, ULINT_UNDEFINED, &heap);
      ut_ad(page_rec_is_user_rec(rec));

      if (rec_get_deleted_flag(rec, rec_offs_comp(offsets))) {
        /* If the clustered index record is already delete
        marked, then we are here after a DB_LOCK_WAIT.
        Skip delete marking clustered index and disowning
        its blobs. */
        ut_ad(rec_get_trx_id(rec, index) == trx->id);
        ut_ad(!trx_undo_roll_ptr_is_insert(
            row_get_rec_roll_ptr(rec, index, offsets)));
        goto check_fk;
      }

      err =
          btr_cur_del_mark_set_clust_rec(flags, btr_cur_get_block(btr_cur), rec,
                                         index, offsets, thr, node->row, mtr);
      if (err != DB_SUCCESS) {
      err_exit:
        mtr_commit(mtr);
        mem_heap_free(heap);
        return (err);
      }

      /* If the the new row inherits externally stored
      fields (off-page columns a.k.a. BLOBs) from the
      delete-marked old record, mark them disowned by the
      old record and owned by the new entry. */

      if (rec_offs_any_extern(offsets)) {
        if (row_upd_clust_rec_by_insert_inherit(rec, offsets, entry,
                                                node->update)) {
          /* The blobs are disowned here, expecting the
          insert down below to inherit them.  But if the
          insert fails, then this disown will be undone
          when the operation is rolled back. */

          lob::BtrContext btr_ctx(mtr, pcur, index, rec, offsets,
                                  btr_cur_get_block(btr_cur));

          btr_ctx.disown_inherited_fields(node->update);
        }
      }
    check_fk:
      if (referenced) {
        /* NOTE that the following call loses
        the position of pcur ! */

        err = row_upd_check_references_constraints(node, pcur, table, index,
                                                   offsets, thr, mtr);

        if (err != DB_SUCCESS) {
          goto err_exit;
        }
      }
  }

  mtr_commit(mtr);

  err = row_ins_clust_index_entry(
      index, entry, thr, node->upd_ext ? node->upd_ext->n_ext : 0, false);
  node->state = UPD_NODE_INSERT_CLUSTERED;

  mem_heap_free(heap);

  return (err);
}

/** Get the new autoinc counter from the update vector when there is
an autoinc field defined in this table.
@param[in]	update			update vector for the clustered index
@param[in]	autoinc_field_no	autoinc field's order in clustered index
@return the new counter if we find it in the update vector, otherwise 0.
We don't mind that the new counter happens to be 0, we just care about
non-zero counters. */
ib_uint64_t row_upd_get_new_autoinc_counter(const upd_t *update,
                                            ulint autoinc_field_no) {
  ulint n_fields = update->n_fields;
  dfield_t *field = NULL;

  for (ulint i = 0; i < n_fields; ++i) {
    upd_field_t *upd_field = upd_get_nth_field(update, i);

    if (upd_field->field_no == autoinc_field_no &&
        !upd_fld_is_virtual_col(upd_field)) {
      /* We should double check the field to see if this
      is a virtual column, which is on virtual index
      instead of clustered index */
      field = &upd_field->new_val;
      break;
    }
  }

  if (field != NULL) {
    return (row_parse_int_from_field(field));
  }

  return (0);
}

/** If the table has autoinc column and the counter is updated to
some bigger value, we need to log the new autoinc counter. We will
use the given mtr to do logging for performance reasons.
@param[in]	node	row update node
@param[in,out]	mtr	mini-transaction */
static void row_upd_check_autoinc_counter(const upd_node_t *node, mtr_t *mtr) {
  dict_table_t *table = node->table;

  if (!dict_table_has_autoinc_col(table) || table->is_temporary() ||
      node->row == NULL) {
    return;
  }

  /* If the node->row hasn't been prepared, there must
  no order field change and autoinc field should keep
  as is. Otherwise, we need to check if autoinc field
  would be changed to a bigger number. */
  ib_uint64_t new_counter;

  new_counter =
      row_upd_get_new_autoinc_counter(node->update, table->autoinc_field_no);

  if (new_counter == 0) {
    return;
  }

  ib_uint64_t old_counter;
  const dict_index_t *index;

  index = table->first_index();

  /* The autoinc field order in row is not the
  same as in clustered index, we need to get
  the column number in the table instead. */
  old_counter = row_get_autoinc_counter(
      node->row, index->get_col_no(table->autoinc_field_no));

  /* We just check if the updated counter is bigger than
  the old one, which may result in more redo logs, since
  this is safer than checking with the counter in table
  object. */
  if (new_counter > old_counter) {
    dict_table_autoinc_log(table, new_counter, mtr);
  }
}

/** Updates a clustered index record of a row when the ordering fields do
 not change.
 @return DB_SUCCESS if operation successfully completed, else error
 code or DB_LOCK_WAIT */
static MY_ATTRIBUTE((warn_unused_result)) dberr_t row_upd_clust_rec(
    ulint flags,         /*!< in: undo logging and locking flags */
    upd_node_t *node,    /*!< in: row update node */
    dict_index_t *index, /*!< in: clustered index */
    ulint *offsets,      /*!< in: rec_get_offsets() on node->pcur */
    mem_heap_t **offsets_heap,
    /*!< in/out: memory heap, can be emptied */
    que_thr_t *thr, /*!< in: query thread */
    mtr_t *mtr)     /*!< in: mtr; gets committed here */
{
  mem_heap_t *heap = NULL;
  big_rec_t *big_rec = NULL;
  btr_pcur_t *pcur;
  btr_cur_t *btr_cur;
  dberr_t err;
  const dtuple_t *rebuilt_old_pk = NULL;
  trx_id_t trx_id = thr_get_trx(thr)->id;
  trx_t *trx = thr_get_trx(thr);

  ut_ad(node);
  ut_ad(index->is_clustered());
  ut_ad(!thr_get_trx(thr)->in_rollback);

  pcur = node->pcur;
  btr_cur = btr_pcur_get_btr_cur(pcur);

  ut_ad(btr_cur_get_index(btr_cur) == index);
  ut_ad(!rec_get_deleted_flag(btr_cur_get_rec(btr_cur),
                              dict_table_is_comp(index->table)));
  ut_ad(rec_offs_validate(btr_cur_get_rec(btr_cur), index, offsets));

  if (dict_index_is_online_ddl(index)) {
    rebuilt_old_pk = row_log_table_get_pk(trx, btr_cur_get_rec(btr_cur), index,
                                          offsets, NULL, &heap);
  }

  /* Check and log if necessary at the beginning, to prevent any
  further potential deadlock */
  row_upd_check_autoinc_counter(node, mtr);

  /* Try optimistic updating of the record, keeping changes within
  the page; we do not check locks because we assume the x-lock on the
  record to update */

  if (node->cmpl_info & UPD_NODE_NO_SIZE_CHANGE) {
    err = btr_cur_update_in_place(flags | BTR_NO_LOCKING_FLAG, btr_cur, offsets,
                                  node->update, node->cmpl_info, thr,
                                  thr_get_trx(thr)->id, mtr);
  } else {
    err = btr_cur_optimistic_update(
        flags | BTR_NO_LOCKING_FLAG, btr_cur, &offsets, offsets_heap,
        node->update, node->cmpl_info, thr, thr_get_trx(thr)->id, mtr);
  }

  if (err == DB_SUCCESS) {
    goto success;
  }

  mtr->commit();

  if (buf_LRU_buf_pool_running_out()) {
    err = DB_LOCK_TABLE_FULL;
    goto func_exit;
  }
  /* We may have to modify the tree structure: do a pessimistic descent
  down the index tree */

  mtr->start();

  /* Disable REDO logging as lifetime of temp-tables is limited to
  server or connection lifetime and so REDO information is not needed
  on restart for recovery.
  Disable locking as temp-tables are not shared across connection. */
  if (index->table->is_temporary()) {
    flags |= BTR_NO_LOCKING_FLAG;
    mtr->set_log_mode(MTR_LOG_NO_REDO);

    if (index->table->is_intrinsic()) {
      flags |= BTR_NO_UNDO_LOG_FLAG;
    }
  }

  /* NOTE: this transaction has an s-lock or x-lock on the record and
  therefore other transactions cannot modify the record when we have no
  latch on the page. In addition, we assume that other query threads of
  the same transaction do not modify the record in the meantime.
  Therefore we can assert that the restoration of the cursor succeeds. */

  ut_a(btr_pcur_restore_position(BTR_MODIFY_TREE, pcur, mtr));

  ut_ad(!rec_get_deleted_flag(btr_pcur_get_rec(pcur),
                              dict_table_is_comp(index->table)));

  if (!heap) {
    heap = mem_heap_create(1024);
  }

  err = btr_cur_pessimistic_update(
      flags | BTR_NO_LOCKING_FLAG | BTR_KEEP_POS_FLAG, btr_cur, &offsets,
      offsets_heap, heap, &big_rec, node->update, node->cmpl_info, thr, trx_id,
      trx->undo_no, mtr);
  if (big_rec) {
    ut_a(err == DB_SUCCESS);

    DEBUG_SYNC_C("before_row_upd_extern");
    err = lob::btr_store_big_rec_extern_fields(
        trx, pcur, node->update, offsets, big_rec, mtr, lob::OPCODE_UPDATE);
    DEBUG_SYNC_C("after_row_upd_extern");
  }

  if (err == DB_SUCCESS) {
  success:
    if (dict_index_is_online_ddl(index)) {
      dtuple_t *new_v_row = NULL;
      dtuple_t *old_v_row = NULL;

      if (!(node->cmpl_info & UPD_NODE_NO_ORD_CHANGE)) {
        new_v_row = node->upd_row;
        old_v_row = node->update->old_vrow;
      }

      row_log_table_update(btr_cur_get_rec(btr_cur), index, offsets,
                           rebuilt_old_pk, new_v_row, old_v_row);
    }
  }

  mtr->commit();

func_exit:
  if (heap) {
    mem_heap_free(heap);
  }

  if (big_rec) {
    dtuple_big_rec_free(big_rec);
  }

  return (err);
}

/** Delete marks a clustered index record.
 @return DB_SUCCESS if operation successfully completed, else error code */
static MY_ATTRIBUTE((warn_unused_result)) dberr_t row_upd_del_mark_clust_rec(
    ulint flags,         /*!< in: undo logging and locking flags */
    upd_node_t *node,    /*!< in: row update node */
    dict_index_t *index, /*!< in: clustered index */
    ulint *offsets,      /*!< in/out: rec_get_offsets() for the
                         record under the cursor */
    que_thr_t *thr,      /*!< in: query thread */
    ibool referenced,
    /*!< in: TRUE if index may be referenced in
    a foreign key constraint */
    mtr_t *mtr) /*!< in: mtr; gets committed here */
{
  btr_pcur_t *pcur;
  btr_cur_t *btr_cur;
  dberr_t err;
  trx_t *trx = thr_get_trx(thr);

  ut_ad(node);
  ut_ad(index->is_clustered());
  ut_ad(node->is_delete);

  pcur = node->pcur;
  btr_cur = btr_pcur_get_btr_cur(pcur);

  /* Store row because we have to build also the secondary index
  entries */

  row_upd_store_row(trx, node, thr_get_trx(thr)->mysql_thd,
                    thr->prebuilt ? thr->prebuilt->m_mysql_table : NULL);

  /* Mark the clustered index record deleted; we do not have to check
  locks, because we assume that we have an x-lock on the record */

  err = btr_cur_del_mark_set_clust_rec(flags, btr_cur_get_block(btr_cur),
                                       btr_cur_get_rec(btr_cur), index, offsets,
                                       thr, node->row, mtr);
  if (err == DB_SUCCESS && referenced) {
    /* NOTE that the following call loses the position of pcur ! */

    err = row_upd_check_references_constraints(node, pcur, index->table, index,
                                               offsets, thr, mtr);
  }

  mtr_commit(mtr);

  return (err);
}

/** Updates the clustered index record.
 @return DB_SUCCESS if operation successfully completed, DB_LOCK_WAIT
 in case of a lock wait, else error code */
static MY_ATTRIBUTE((warn_unused_result)) dberr_t
    row_upd_clust_step(upd_node_t *node, /*!< in: row update node */
                       que_thr_t *thr)   /*!< in: query thread */
{
  dict_index_t *index;
  btr_pcur_t *pcur;
  ibool success;
  dberr_t err;
  mtr_t mtr;
  rec_t *rec;
  mem_heap_t *heap = NULL;
  ulint offsets_[REC_OFFS_NORMAL_SIZE];
  ulint *offsets;
  ibool referenced;
  ulint flags = 0;
  trx_t *trx = thr_get_trx(thr);
  rec_offs_init(offsets_);

  index = node->table->first_index();

  referenced = row_upd_index_is_referenced(index, trx);

  pcur = node->pcur;

  /* We have to restore the cursor to its position */

  mtr_start(&mtr);

  /* Disable REDO logging as lifetime of temp-tables is limited to
  server or connection lifetime and so REDO information is not needed
  on restart for recovery.
  Disable locking as temp-tables are not shared across connection. */
  if (index->table->is_temporary()) {
    flags |= BTR_NO_LOCKING_FLAG;
    mtr.set_log_mode(MTR_LOG_NO_REDO);

    if (index->table->is_intrinsic()) {
      flags |= BTR_NO_UNDO_LOG_FLAG;
    }
  }

  /* If the restoration does not succeed, then the same
  transaction has deleted the record on which the cursor was,
  and that is an SQL error. If the restoration succeeds, it may
  still be that the same transaction has successively deleted
  and inserted a record with the same ordering fields, but in
  that case we know that the transaction has at least an
  implicit x-lock on the record. */

  ut_a(pcur->rel_pos == BTR_PCUR_ON);

  ulint mode;

  DEBUG_SYNC_C_IF_THD(thr_get_trx(thr)->mysql_thd,
                      "innodb_row_upd_clust_step_enter");

  if (dict_index_is_online_ddl(index)) {
    ut_ad(node->table->id != DICT_INDEXES_ID);
    mode = BTR_MODIFY_LEAF | BTR_ALREADY_S_LATCHED;
    mtr_s_lock(dict_index_get_lock(index), &mtr);
  } else {
    mode = BTR_MODIFY_LEAF;
  }

  success = btr_pcur_restore_position(mode, pcur, &mtr);

  if (!success) {
    err = DB_RECORD_NOT_FOUND;

    mtr_commit(&mtr);

    return (err);
  }

  rec = btr_pcur_get_rec(pcur);
  offsets = rec_get_offsets(rec, index, offsets_, ULINT_UNDEFINED, &heap);

  if (!node->has_clust_rec_x_lock) {
    err = lock_clust_rec_modify_check_and_lock(flags, btr_pcur_get_block(pcur),
                                               rec, index, offsets, thr);
    if (err != DB_SUCCESS) {
      mtr_commit(&mtr);
      goto exit_func;
    }
  }

  ut_ad(lock_trx_has_rec_x_lock(thr_get_trx(thr), index->table,
                                btr_pcur_get_block(pcur),
                                page_rec_get_heap_no(rec)));

  /* NOTE: the following function calls will also commit mtr */

  if (node->is_delete) {
    err = row_upd_del_mark_clust_rec(flags, node, index, offsets, thr,
                                     referenced, &mtr);

    if (err == DB_SUCCESS) {
      node->state = UPD_NODE_UPDATE_ALL_SEC;
      node->index = index->next();
    }

    goto exit_func;
  }

  /* If the update is made for MySQL, we already have the update vector
  ready, else we have to do some evaluation: */

  if (UNIV_UNLIKELY(!node->in_mysql_interface)) {
    /* Copy the necessary columns from clust_rec and calculate the
    new values to set */
    row_upd_copy_columns(rec, offsets, index, UT_LIST_GET_FIRST(node->columns));
    row_upd_eval_new_vals(node->update);
  }

  if (node->cmpl_info & UPD_NODE_NO_ORD_CHANGE) {
    err = row_upd_clust_rec(flags, node, index, offsets, &heap, thr, &mtr);
    goto exit_func;
  }

  row_upd_store_row(trx, node, trx->mysql_thd,
                    thr->prebuilt ? thr->prebuilt->m_mysql_table : NULL);

  if (row_upd_changes_ord_field_binary(index, node->update, thr, node->row,
                                       node->ext)) {
    /* Update causes an ordering field (ordering fields within
    the B-tree) of the clustered index record to change: perform
    the update by delete marking and inserting.

    TODO! What to do to the 'Halloween problem', where an update
    moves the record forward in index so that it is again
    updated when the cursor arrives there? Solution: the
    read operation must check the undo record undo number when
    choosing records to update. MySQL solves now the problem
    externally! */

    err =
        row_upd_clust_rec_by_insert(flags, node, index, thr, referenced, &mtr);

    if (err != DB_SUCCESS) {
      goto exit_func;
    }

    node->state = UPD_NODE_UPDATE_ALL_SEC;
  } else {
    err = row_upd_clust_rec(flags, node, index, offsets, &heap, thr, &mtr);

    if (err != DB_SUCCESS) {
      goto exit_func;
    }

    node->state = UPD_NODE_UPDATE_SOME_SEC;
  }

  node->index = index->next();

exit_func:
  if (heap) {
    mem_heap_free(heap);
  }
  return (err);
}

/** Updates the affected index records of a row. When the control is transferred
 to this node, we assume that we have a persistent cursor which was on a
 record, and the position of the cursor is stored in the cursor.
 @return DB_SUCCESS if operation successfully completed, else error
 code or DB_LOCK_WAIT */
static dberr_t row_upd(upd_node_t *node, /*!< in: row update node */
                       que_thr_t *thr)   /*!< in: query thread */
{
  dberr_t err = DB_SUCCESS;
  DBUG_ENTER("row_upd");

  ut_ad(node != NULL);
  ut_ad(thr != NULL);
  ut_ad(!thr_get_trx(thr)->in_rollback);
  ut_ad(!node->table->skip_alter_undo);

  DBUG_PRINT("row_upd", ("table: %s", node->table->name.m_name));
  DBUG_PRINT("row_upd", ("info bits in update vector: 0x%lx",
                         node->update ? node->update->info_bits : 0));
  DBUG_PRINT("row_upd",
             ("foreign_id: %s", node->foreign ? node->foreign->id : "NULL"));

  if (UNIV_LIKELY(node->in_mysql_interface)) {
    /* We do not get the cmpl_info value from the MySQL
    interpreter: we must calculate it on the fly: */

    if (node->is_delete || row_upd_changes_some_index_ord_field_binary(
                               node->table, node->update)) {
      node->cmpl_info = 0;
    } else {
      node->cmpl_info = UPD_NODE_NO_ORD_CHANGE;
    }
  }

  switch (node->state) {
    case UPD_NODE_UPDATE_CLUSTERED:
    case UPD_NODE_INSERT_CLUSTERED:
      if (!node->table->is_intrinsic()) {
        log_free_check();
      }
      err = row_upd_clust_step(node, thr);

      if (err != DB_SUCCESS) {
        DBUG_RETURN(err);
      }
  }

  DEBUG_SYNC_C_IF_THD(thr_get_trx(thr)->mysql_thd, "after_row_upd_clust");

  if (node->index == NULL ||
      (!node->is_delete && (node->cmpl_info & UPD_NODE_NO_ORD_CHANGE))) {
    DBUG_RETURN(DB_SUCCESS);
  }

  DBUG_EXECUTE_IF("row_upd_skip_sec", node->index = NULL;);

  do {
    /* Skip corrupted index */
    dict_table_skip_corrupt_index(node->index);

    if (!node->index) {
      break;
    }

    if (node->index->type != DICT_FTS) {
      err = row_upd_sec_step(node, thr);

      if (err != DB_SUCCESS) {
        DBUG_RETURN(err);
      }
    }

    node->index = node->index->next();
  } while (node->index != NULL);

  ut_ad(err == DB_SUCCESS);

  /* Do some cleanup */

  if (node->row != NULL) {
    node->row = NULL;
    node->ext = NULL;
    node->upd_row = NULL;
    node->upd_ext = NULL;
    mem_heap_empty(node->heap);
  }

  node->state = UPD_NODE_UPDATE_CLUSTERED;

  DBUG_RETURN(err);
}

/** Updates a row in a table. This is a high-level function used in SQL
 execution graphs.
 @return query thread to run next or NULL */
que_thr_t *row_upd_step(que_thr_t *thr) /*!< in: query thread */
{
  upd_node_t *node;
  sel_node_t *sel_node;
  que_node_t *parent;
  dberr_t err = DB_SUCCESS;
  trx_t *trx;
  DBUG_ENTER("row_upd_step");

  ut_ad(thr);

  trx = thr_get_trx(thr);

  trx_start_if_not_started_xa(trx, true);

  node = static_cast<upd_node_t *>(thr->run_node);

  sel_node = node->select;

  parent = que_node_get_parent(node);

  ut_ad(que_node_get_type(node) == QUE_NODE_UPDATE);

  if (thr->prev_node == parent) {
    node->state = UPD_NODE_SET_IX_LOCK;
  }

  if (node->state == UPD_NODE_SET_IX_LOCK) {
    if (!node->has_clust_rec_x_lock) {
      /* It may be that the current session has not yet
      started its transaction, or it has been committed: */

      err = lock_table(0, node->table, LOCK_IX, thr);

      if (err != DB_SUCCESS) {
        goto error_handling;
      }
    }

    node->state = UPD_NODE_UPDATE_CLUSTERED;

    if (node->searched_update) {
      /* Reset the cursor */
      sel_node->state = SEL_NODE_OPEN;

      /* Fetch a row to update */

      thr->run_node = sel_node;

      DBUG_RETURN(thr);
    }
  }

  /* sel_node is NULL if we are in the MySQL interface */

  if (sel_node && (sel_node->state != SEL_NODE_FETCH)) {
    if (!node->searched_update) {
      /* An explicit cursor should be positioned on a row
      to update */

      ut_error;
    }

    ut_ad(sel_node->state == SEL_NODE_NO_MORE_ROWS);

    /* No more rows to update, or the select node performed the
    updates directly in-place */

    thr->run_node = parent;

    DBUG_RETURN(thr);
  }

  /* DO THE CHECKS OF THE CONSISTENCY CONSTRAINTS HERE */

  err = row_upd(node, thr);

error_handling:
  trx->error_state = err;

  if (err != DB_SUCCESS) {
    DBUG_RETURN(NULL);
  }

  /* DO THE TRIGGER ACTIONS HERE */

  if (node->searched_update) {
    /* Fetch next row to update */

    thr->run_node = sel_node;
  } else {
    /* It was an explicit cursor update */

    thr->run_node = parent;
  }

  node->state = UPD_NODE_UPDATE_CLUSTERED;

  DBUG_RETURN(thr);
}

std::ostream &upd_field_t::print(std::ostream &out) const {
  out << "[upd_field_t: field_no=" << field_no << ", orig_len=" << orig_len
      << ", old_val=" << old_val << ", new_val=" << new_val
      << ", ext_in_old=" << ext_in_old;

  if (lob_diffs != nullptr) {
    for (auto iter = lob_diffs->begin(); iter != lob_diffs->end(); ++iter) {
      out << *iter;
    }
  }

  out << "]";

  return (out);
}

std::ostream &upd_t::print(std::ostream &out) const {
  out << "[upd_t: n_fields=" << n_fields << ", ";
  for (ulint i = 0; i < n_fields; ++i) {
    out << fields[i];
    print_puvect(out, &fields[i]);
  }
  out << "]";
  return (out);
}

/** Print the given binary diff into the given output stream.
@param[in]	out	the output stream
@param[in]	uf	the update vector of concerned field.
@param[in]	bdiff	binary diff to be printed.
@param[in]	table	the table dictionary object.
@param[in]	field	mysql field object.
@return the output stream */
static std::ostream &print_binary_diff(std::ostream &out, upd_field_t *uf,
                                       const Binary_diff *bdiff,
                                       const dict_table_t *table,
                                       const Field *field) {
  ulint field_no = 0;
  if (table != nullptr) {
    dict_col_t *col = table->get_col(field->field_index);
    field_no = dict_col_get_clust_pos(col, table->first_index());
  }

  const char *to = bdiff->new_data(const_cast<Field *>(field));
  size_t len = bdiff->length();

  const char *from = bdiff->old_data(const_cast<Field *>(field));

  out << "[Binary_diff: field_index=" << field->field_index
      << ", field_no=" << field_no << ", offset=" << bdiff->offset()
      << ", length=" << len << ", new_data=" << PrintBuffer(to, len)
      << ", old_data=" << PrintBuffer(from, len) << "]";

  return (out);
}

/** Print the given binary diff into the given output stream.
@param[in]	out	the output stream
@param[in]	bdiff	binary diff to be printed.
@param[in]	table	the table dictionary object.
@param[in]	field	mysql field object.
@return the output stream */
std::ostream &print_binary_diff(std::ostream &out, const Binary_diff *bdiff,
                                const dict_table_t *table, const Field *field) {
  ulint field_no = 0;
  if (table != nullptr) {
    dict_col_t *col = table->get_col(field->field_index);
    field_no = dict_col_get_clust_pos(col, table->first_index());
  }

  const char *to = bdiff->new_data(const_cast<Field *>(field));
  size_t len = bdiff->length();

  out << "[Binary_diff: field_index=" << field->field_index
      << ", field_no=" << field_no << ", offset=" << bdiff->offset()
      << ", length=" << len << ", new_data=" << PrintBuffer(to, len) << "]";

  return (out);
}

std::ostream &print_binary_diff(std::ostream &out, const Binary_diff *bdiff,
                                Field *fld) {
  const char *to = bdiff->new_data(fld);
  size_t len = bdiff->length();

  out << "[Binary_diff: field_index=" << fld->field_index
      << ", offset=" << bdiff->offset() << ", length=" << bdiff->length()
      << ", new_data=" << PrintBuffer(to, len) << "]";
  return (out);
}

std::ostream &upd_t::print_puvect(std::ostream &out, upd_field_t *uf) const {
  if (!is_partially_updated(uf->field_no)) {
    return (out);
  }

  Field *fld = uf->mysql_field;

  const Binary_diff_vector *dv = mysql_table->get_binary_diffs(fld);

  for (Binary_diff_vector::const_iterator iter = dv->begin(); iter != dv->end();
       ++iter) {
    const Binary_diff *bdiff = iter;
    print_binary_diff(out, uf, bdiff, table, fld);
  }

  return (out);
}

upd_field_t *upd_t::get_field_by_field_no(ulint field_no,
                                          dict_index_t *index) const {
  const upd_field_t *uf;

  dict_field_t *field = index->get_field(field_no);
  dict_col_t *col = field->col;

  if (col->is_virtual()) {
    const dict_v_col_t *vcol = reinterpret_cast<const dict_v_col_t *>(col);

    uf = upd_get_field_by_field_no(this, vcol->v_pos, true);
  } else {
    uf = upd_get_field_by_field_no(this, field_no, false);
  }

  return (const_cast<upd_field_t *>(uf));
}

/** Check if the given field number is partially updated.
@param[in]	field_no	the field number.
@return true if partially updated, false otherwise. */
bool upd_t::is_partially_updated(ulint field_no) const {
  if (mysql_table == nullptr || !mysql_table->has_binary_diff_columns()) {
    return (false);
  }

  upd_field_t *uf = get_field_by_field_no(field_no, table->first_index());

  if (uf == nullptr || uf->mysql_field == nullptr) {
    return (false);
  }

  ut_ad(mysql_table == uf->mysql_field->table);

  if (!mysql_table->is_binary_diff_enabled(uf->mysql_field)) {
    return (false);
  }

  if (dict_table_has_atomic_blobs(table)) {
    return (true);
  }

#ifdef UNIV_DEBUG
  rec_format_t format = dict_tf_get_rec_format(table->flags);
  ut_ad(format == REC_FORMAT_REDUNDANT || format == REC_FORMAT_COMPACT);
#endif /* UNIV_DEBUG */

  /* In compact and redundant row format, partially updating the LOB prefix
  is not yet supported. */

  const Binary_diff_vector *bdiff_vector =
      get_binary_diff_by_field_no(field_no);

  for (Binary_diff_vector::const_iterator iter = bdiff_vector->begin();
       iter != bdiff_vector->end(); ++iter) {
    const Binary_diff *bdiff = iter;

    if (bdiff->offset() < DICT_ANTELOPE_MAX_INDEX_COL_LEN) {
      return (false);
    }
  }

  return (true);
}

const Binary_diff_vector *upd_t::get_binary_diff_by_field_no(
    ulint field_no) const {
  ut_ad(table != nullptr);

  upd_field_t *uf = get_field_by_field_no(field_no, table->first_index());
  ut_ad(uf != nullptr);

  Field *fld = uf->mysql_field;

  if (fld == nullptr) {
    return (nullptr);
  }

  return (mysql_table->get_binary_diffs(fld));
}
#endif /* !UNIV_HOTBACKUP */
