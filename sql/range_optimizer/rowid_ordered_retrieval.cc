/* Copyright (c) 2000, 2021, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql/range_optimizer/rowid_ordered_retrieval.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <new>

#include "lex_string.h"
#include "m_string.h"
#include "my_base.h"
#include "my_bitmap.h"
#include "my_dbug.h"
#include "my_sys.h"
#include "mysql/components/services/bits/psi_bits.h"
#include "mysqld_error.h"
#include "sql/current_thd.h"
#include "sql/key.h"
#include "sql/psi_memory_key.h"
#include "sql/sql_class.h"
#include "sql/sql_const.h"
#include "sql/system_variables.h"
#include "sql/thr_malloc.h"
#include "sql_string.h"

QUICK_ROR_INTERSECT_SELECT::QUICK_ROR_INTERSECT_SELECT(
    TABLE *table, bool retrieve_full_rows, MEM_ROOT *return_mem_root)
    : mem_root(return_mem_root),
      cpk_quick(nullptr),
      need_to_fetch_row(retrieve_full_rows),
      scans_inited(false) {
  index = MAX_KEY;
  m_table = table;
  record = m_table->record[0];
  last_rowid = (uchar *)mem_root->Alloc(m_table->file->ref_length);
}

/*
  Do post-constructor initialization.
  SYNOPSIS
    QUICK_ROR_INTERSECT_SELECT::init()

  RETURN
    0      OK
    other  Error code
*/

int QUICK_ROR_INTERSECT_SELECT::init() {
  DBUG_TRACE;
  /* Check if last_rowid was successfully allocated in ctor */
  return !last_rowid;
}

/*
  Initialize this quick select to be a ROR-merged scan.

  SYNOPSIS
    QUICK_RANGE_SELECT::init_ror_merged_scan()
      reuse_handler If true, use m_table->file, otherwise create a separate
                    handler object

  NOTES
    This function creates and prepares for subsequent use a separate handler
    object if it can't reuse m_table->file. The reason for this is that during
    ROR-merge several key scans are performed simultaneously, and a single
    handler is only capable of preserving context of a single key scan.

    In ROR-merge the quick select doing merge does full records retrieval,
    merged quick selects read only keys.

  RETURN
    0  ROR child scan initialized, ok to use.
    1  error
*/

int QUICK_RANGE_SELECT::init_ror_merged_scan(bool reuse_handler) {
  handler *save_file = file, *org_file;
  MY_BITMAP *const save_read_set = m_table->read_set;
  MY_BITMAP *const save_write_set = m_table->write_set;
  DBUG_TRACE;

  THD *thd = current_thd;

  in_ror_merged_scan = true;
  mrr_flags |= HA_MRR_SORTED;
  if (reuse_handler) {
    DBUG_PRINT("info", ("Reusing handler %p", file));
    if (init() || reset()) {
      return 1;
    }
    m_table->column_bitmaps_set(&column_bitmap, &column_bitmap);
    file->ha_extra(HA_EXTRA_SECONDARY_SORT_ROWID);
    goto end;
  }

  /* Create a separate handler object for this quick select */
  if (free_file) {
    /* already have own 'handler' object. */
    return 0;
  }

  if (!(file =
            m_table->file->clone(m_table->s->normalized_path.str, mem_root))) {
    /*
      Manually set the error flag. Note: there seems to be quite a few
      places where a failure could cause the server to "hang" the client by
      sending no response to a query. ATM those are not real errors because
      the storage engine calls in question happen to never fail with the
      existing storage engines.
    */
    my_error(ER_OUT_OF_RESOURCES, MYF(0)); /* purecov: inspected */
    /* Caller will free the memory */
    goto failure; /* purecov: inspected */
  }

  m_table->column_bitmaps_set(&column_bitmap, &column_bitmap);

  if (file->ha_external_lock(thd, m_table->file->get_lock_type())) goto failure;

  if (init() || reset()) {
    file->ha_external_lock(thd, F_UNLCK);
    file->ha_close();
    goto failure;
  }
  free_file = true;
  last_rowid = file->ref;
  file->ha_extra(HA_EXTRA_SECONDARY_SORT_ROWID);

end:
  /*
    We are only going to read key fields and call position() on 'file'
    The following sets m_table->tmp_set to only use this key and then updates
    m_table->read_set and m_table->write_set to use this bitmap.
    The now bitmap is stored in 'column_bitmap' which is used in ::get_next()
  */
  org_file = m_table->file;
  m_table->file = file;
  /* We don't have to set 'm_table->keyread' here as the 'file' is unique */
  if (!m_table->no_keyread) m_table->mark_columns_used_by_index(index);
  m_table->prepare_for_position();
  m_table->file = org_file;
  bitmap_copy(&column_bitmap, m_table->read_set);

  /*
    We have prepared a column_bitmap which get_next() will use. To do this we
    used TABLE::read_set/write_set as playground; restore them to their
    original value to not pollute other scans.
  */
  m_table->column_bitmaps_set(save_read_set, save_write_set);
  bitmap_clear_all(&m_table->tmp_set);

  return 0;

failure:
  m_table->column_bitmaps_set(save_read_set, save_write_set);
  destroy(file);
  file = save_file;
  return 1;
}

/*
  Initialize this quick select to be a part of a ROR-merged scan.
  SYNOPSIS
    QUICK_ROR_INTERSECT_SELECT::init_ror_merged_scan()
      reuse_handler If true, use m_table->file, otherwise create separate
                    handler object.
  RETURN
    0     OK
    other error code
*/
int QUICK_ROR_INTERSECT_SELECT::init_ror_merged_scan(bool reuse_handler) {
  List_iterator_fast<QUICK_RANGE_SELECT> quick_it(quick_selects);
  QUICK_RANGE_SELECT *quick;
  DBUG_TRACE;

  /* Initialize all merged "children" quick selects */
  assert(!need_to_fetch_row || reuse_handler);
  if (!need_to_fetch_row && reuse_handler) {
    quick = quick_it++;
    /*
      There is no use of this->file. Use it for the first of merged range
      selects.
    */
    int error = quick->init_ror_merged_scan(true);
    if (error) return error;
    quick->file->ha_extra(HA_EXTRA_KEYREAD_PRESERVE_FIELDS);
  }
  while ((quick = quick_it++)) {
#ifndef NDEBUG
    const MY_BITMAP *const save_read_set = quick->m_table->read_set;
    const MY_BITMAP *const save_write_set = quick->m_table->write_set;
#endif
    int error;
    if ((error = quick->init_ror_merged_scan(false))) return error;
    quick->file->ha_extra(HA_EXTRA_KEYREAD_PRESERVE_FIELDS);
    // Sets are shared by all members of "quick_selects" so must not change
    assert(quick->m_table->read_set == save_read_set);
    assert(quick->m_table->write_set == save_write_set);
    /* All merged scans share the same record buffer in intersection. */
    quick->record = m_table->record[0];
  }

  /* Prepare for ha_rnd_pos calls if needed. */
  int error;
  if (need_to_fetch_row && (error = m_table->file->ha_rnd_init(false))) {
    DBUG_PRINT("error", ("ROR index_merge rnd_init call failed"));
    return error;
  }
  return 0;
}

/*
  Initialize quick select for row retrieval.
  SYNOPSIS
    reset()
  RETURN
    0      OK
    other  Error code
*/

int QUICK_ROR_INTERSECT_SELECT::reset() {
  DBUG_TRACE;
  if (!scans_inited && init_ror_merged_scan(true)) return 1;
  scans_inited = true;
  List_iterator_fast<QUICK_RANGE_SELECT> it(quick_selects);
  QUICK_RANGE_SELECT *quick;
  while ((quick = it++)) quick->reset();
  return 0;
}

/*
  Add a merged quick select to this ROR-intersection quick select.

  SYNOPSIS
    QUICK_ROR_INTERSECT_SELECT::push_quick_back()
      quick Quick select to be added. The quick select must return
            rows in rowid order.
  NOTES
    This call can only be made before init() is called.

  RETURN
    false OK
    true  Out of memory.
*/

bool QUICK_ROR_INTERSECT_SELECT::push_quick_back(QUICK_RANGE_SELECT *quick) {
  return quick_selects.push_back(quick);
}

QUICK_ROR_INTERSECT_SELECT::~QUICK_ROR_INTERSECT_SELECT() {
  DBUG_TRACE;
  quick_selects.destroy_elements();
  destroy(cpk_quick);
  if (need_to_fetch_row && m_table->file->inited) m_table->file->ha_rnd_end();
}

QUICK_ROR_UNION_SELECT::QUICK_ROR_UNION_SELECT(MEM_ROOT *return_mem_root,
                                               TABLE *table)
    : queue(Quick_ror_union_less(this),
            Malloc_allocator<PSI_memory_key>(PSI_INSTRUMENT_ME)),
      mem_root(return_mem_root),
      scans_inited(false) {
  index = MAX_KEY;
  m_table = table;
  rowid_length = table->file->ref_length;
  record = m_table->record[0];
}

/*
  Do post-constructor initialization.
  SYNOPSIS
    QUICK_ROR_UNION_SELECT::init()

  RETURN
    0      OK
    other  Error code
*/

int QUICK_ROR_UNION_SELECT::init() {
  DBUG_TRACE;
  if (queue.reserve(quick_selects.elements)) {
    return 1;
  }

  if (!(cur_rowid = (uchar *)mem_root->Alloc(2 * m_table->file->ref_length)))
    return 1;
  prev_rowid = cur_rowid + m_table->file->ref_length;
  return 0;
}

/*
  Initialize quick select for row retrieval.
  SYNOPSIS
    reset()

  RETURN
    0      OK
    other  Error code
*/

int QUICK_ROR_UNION_SELECT::reset() {
  QUICK_SELECT_I *quick;
  int error;
  DBUG_TRACE;
  have_prev_rowid = false;
  if (!scans_inited) {
    List_iterator_fast<QUICK_SELECT_I> it(quick_selects);
    while ((quick = it++)) {
      error = quick->init_ror_merged_scan(false);
      if (error) return 1;
    }
    scans_inited = true;
  }
  queue.clear();
  /*
    Initialize scans for merged quick selects and put all merged quick
    selects into the queue.
  */
  List_iterator_fast<QUICK_SELECT_I> it(quick_selects);
  while ((quick = it++)) {
    if ((error = quick->reset())) return error;
    if ((error = quick->get_next())) {
      if (error == HA_ERR_END_OF_FILE) continue;
      return error;
    }
    quick->save_last_pos();
    queue.push(quick);
  }

  /* Prepare for ha_rnd_pos calls. */
  if (m_table->file->inited && (error = m_table->file->ha_rnd_end())) {
    DBUG_PRINT("error", ("ROR index_merge rnd_end call failed"));
    return error;
  }
  if ((error = m_table->file->ha_rnd_init(false))) {
    DBUG_PRINT("error", ("ROR index_merge rnd_init call failed"));
    return error;
  }

  return 0;
}

bool QUICK_ROR_UNION_SELECT::push_quick_back(QUICK_SELECT_I *quick_sel_range) {
  return quick_selects.push_back(quick_sel_range);
}

QUICK_ROR_UNION_SELECT::~QUICK_ROR_UNION_SELECT() {
  DBUG_TRACE;
  quick_selects.destroy_elements();
  if (m_table->file->inited) m_table->file->ha_rnd_end();
}

bool QUICK_ROR_INTERSECT_SELECT::is_keys_used(const MY_BITMAP *fields) {
  QUICK_RANGE_SELECT *quick;
  List_iterator_fast<QUICK_RANGE_SELECT> it(quick_selects);
  while ((quick = it++)) {
    if (is_key_used(m_table, quick->index, fields)) return true;
  }
  return false;
}

bool QUICK_ROR_UNION_SELECT::is_keys_used(const MY_BITMAP *fields) {
  QUICK_SELECT_I *quick;
  List_iterator_fast<QUICK_SELECT_I> it(quick_selects);
  while ((quick = it++)) {
    if (quick->is_keys_used(fields)) return true;
  }
  return false;
}

/*
  Retrieve next record.
  SYNOPSIS
     QUICK_ROR_INTERSECT_SELECT::get_next()

  NOTES
    Invariant on enter/exit: all intersected selects have retrieved all index
    records with rowid <= some_rowid_val and no intersected select has
    retrieved any index records with rowid > some_rowid_val.
    We start fresh and loop until we have retrieved the same rowid in each of
    the key scans or we got an error.

    If a Clustered PK scan is present, it is used only to check if row
    satisfies its condition (and never used for row retrieval).

    Locking: to ensure that exclusive locks are only set on records that
    are included in the final result we must release the lock
    on all rows we read but do not include in the final result. This
    must be done on each index that reads the record and the lock
    must be released using the same handler (the same quick object) as
    used when reading the record.

  RETURN
   0     - Ok
   other - Error code if any error occurred.
*/

int QUICK_ROR_INTERSECT_SELECT::get_next() {
  List_iterator_fast<QUICK_RANGE_SELECT> quick_it(quick_selects);
  QUICK_RANGE_SELECT *quick;

  /* quick that reads the given rowid first. This is needed in order
  to be able to unlock the row using the same handler object that locked
  it */
  QUICK_RANGE_SELECT *quick_with_last_rowid;

  int error, cmp;
  uint last_rowid_count = 0;
  DBUG_TRACE;

  do {
    /* Get a rowid for first quick and save it as a 'candidate' */
    quick = quick_it++;
    error = quick->get_next();
    if (cpk_quick) {
      while (!error && !cpk_quick->row_in_ranges()) {
        quick->file->unlock_row(); /* row not in range; unlock */
        error = quick->get_next();
      }
    }
    if (error) return error;

    quick->file->position(quick->record);
    memcpy(last_rowid, quick->file->ref, m_table->file->ref_length);
    last_rowid_count = 1;
    quick_with_last_rowid = quick;

    while (last_rowid_count < quick_selects.elements) {
      if (!(quick = quick_it++)) {
        quick_it.rewind();
        quick = quick_it++;
      }

      do {
        DBUG_EXECUTE_IF("innodb_quick_report_deadlock",
                        DBUG_SET("+d,innodb_report_deadlock"););
        if ((error = quick->get_next())) {
          /* On certain errors like deadlock, trx might be rolled back.*/
          if (!current_thd->transaction_rollback_request)
            quick_with_last_rowid->file->unlock_row();
          return error;
        }
        quick->file->position(quick->record);
        cmp = m_table->file->cmp_ref(quick->file->ref, last_rowid);
        if (cmp < 0) {
          /* This row is being skipped.  Release lock on it. */
          quick->file->unlock_row();
        }
      } while (cmp < 0);

      /* Ok, current select 'caught up' and returned ref >= cur_ref */
      if (cmp > 0) {
        /* Found a row with ref > cur_ref. Make it a new 'candidate' */
        if (cpk_quick) {
          while (!cpk_quick->row_in_ranges()) {
            quick->file->unlock_row(); /* row not in range; unlock */
            if ((error = quick->get_next())) {
              /* On certain errors like deadlock, trx might be rolled back.*/
              if (!current_thd->transaction_rollback_request)
                quick_with_last_rowid->file->unlock_row();
              return error;
            }
          }
          quick->file->position(quick->record);
        }
        memcpy(last_rowid, quick->file->ref, m_table->file->ref_length);
        quick_with_last_rowid->file->unlock_row();
        last_rowid_count = 1;
        quick_with_last_rowid = quick;
      } else {
        /* current 'candidate' row confirmed by this select */
        last_rowid_count++;
      }
    }

    /* We get here if we got the same row ref in all scans. */
    if (need_to_fetch_row)
      error = m_table->file->ha_rnd_pos(m_table->record[0], last_rowid);
  } while (error == HA_ERR_RECORD_DELETED);
  return error;
}

/*
  Retrieve next record.
  SYNOPSIS
    QUICK_ROR_UNION_SELECT::get_next()

  NOTES
    Enter/exit invariant:
    For each quick select in the queue a {key,rowid} tuple has been
    retrieved but the corresponding row hasn't been passed to output.

  RETURN
   0     - Ok
   other - Error code if any error occurred.
*/

int QUICK_ROR_UNION_SELECT::get_next() {
  int error, dup_row;
  QUICK_SELECT_I *quick;
  uchar *tmp;
  DBUG_TRACE;

  do {
    do {
      if (queue.empty()) return HA_ERR_END_OF_FILE;
      /* Ok, we have a queue with >= 1 scans */

      quick = queue.top();
      memcpy(cur_rowid, quick->last_rowid, rowid_length);

      /* put into queue rowid from the same stream as top element */
      if ((error = quick->get_next())) {
        if (error != HA_ERR_END_OF_FILE) return error;
        queue.pop();
      } else {
        quick->save_last_pos();
        queue.update_top();
      }

      if (!have_prev_rowid) {
        /* No rows have been returned yet */
        dup_row = false;
        have_prev_rowid = true;
      } else
        dup_row = !m_table->file->cmp_ref(cur_rowid, prev_rowid);
    } while (dup_row);

    tmp = cur_rowid;
    cur_rowid = prev_rowid;
    prev_rowid = tmp;

    error = m_table->file->ha_rnd_pos(quick->record, prev_rowid);
  } while (error == HA_ERR_RECORD_DELETED);
  return error;
}

void QUICK_ROR_INTERSECT_SELECT::add_info_string(String *str) {
  bool first = true;
  QUICK_RANGE_SELECT *quick;
  List_iterator_fast<QUICK_RANGE_SELECT> it(quick_selects);
  str->append(STRING_WITH_LEN("intersect("));
  while ((quick = it++)) {
    KEY *key_info = m_table->key_info + quick->index;
    if (!first)
      str->append(',');
    else
      first = false;
    str->append(key_info->name);
  }
  if (cpk_quick) {
    KEY *key_info = m_table->key_info + cpk_quick->index;
    str->append(',');
    str->append(key_info->name);
  }
  str->append(')');
}

void QUICK_ROR_UNION_SELECT::add_info_string(String *str) {
  bool first = true;
  QUICK_SELECT_I *quick;
  List_iterator_fast<QUICK_SELECT_I> it(quick_selects);
  str->append(STRING_WITH_LEN("union("));
  while ((quick = it++)) {
    if (!first)
      str->append(',');
    else
      first = false;
    quick->add_info_string(str);
  }
  str->append(')');
}

void QUICK_ROR_INTERSECT_SELECT::add_keys_and_lengths(String *key_names,
                                                      String *used_lengths) {
  char buf[64];
  size_t length;
  bool first = true;
  QUICK_RANGE_SELECT *quick;
  List_iterator_fast<QUICK_RANGE_SELECT> it(quick_selects);
  while ((quick = it++)) {
    KEY *key_info = m_table->key_info + quick->index;
    if (first)
      first = false;
    else {
      key_names->append(',');
      used_lengths->append(',');
    }
    key_names->append(key_info->name);
    length = longlong10_to_str(quick->max_used_key_length, buf, 10) - buf;
    used_lengths->append(buf, length);
  }

  if (cpk_quick) {
    KEY *key_info = m_table->key_info + cpk_quick->index;
    key_names->append(',');
    key_names->append(key_info->name);
    length = longlong10_to_str(cpk_quick->max_used_key_length, buf, 10) - buf;
    used_lengths->append(',');
    used_lengths->append(buf, length);
  }
}

void QUICK_ROR_UNION_SELECT::add_keys_and_lengths(String *key_names,
                                                  String *used_lengths) {
  bool first = true;
  QUICK_SELECT_I *quick;
  List_iterator_fast<QUICK_SELECT_I> it(quick_selects);
  while ((quick = it++)) {
    if (first)
      first = false;
    else {
      used_lengths->append(',');
      key_names->append(',');
    }
    quick->add_keys_and_lengths(key_names, used_lengths);
  }
}

#ifndef NDEBUG
void QUICK_ROR_INTERSECT_SELECT::dbug_dump(int indent, bool verbose) {
  List_iterator_fast<QUICK_RANGE_SELECT> it(quick_selects);
  QUICK_RANGE_SELECT *quick;
  fprintf(DBUG_FILE, "%*squick ROR-intersect select, %scovering\n", indent, "",
          need_to_fetch_row ? "" : "non-");
  fprintf(DBUG_FILE, "%*smerged scans {\n", indent, "");
  while ((quick = it++)) quick->dbug_dump(indent + 2, verbose);
  if (cpk_quick) {
    fprintf(DBUG_FILE, "%*sclustered PK quick:\n", indent, "");
    cpk_quick->dbug_dump(indent + 2, verbose);
  }
  fprintf(DBUG_FILE, "%*s}\n", indent, "");
}

void QUICK_ROR_UNION_SELECT::dbug_dump(int indent, bool verbose) {
  List_iterator_fast<QUICK_SELECT_I> it(quick_selects);
  QUICK_SELECT_I *quick;
  fprintf(DBUG_FILE, "%*squick ROR-union select\n", indent, "");
  fprintf(DBUG_FILE, "%*smerged scans {\n", indent, "");
  while ((quick = it++)) quick->dbug_dump(indent + 2, verbose);
  fprintf(DBUG_FILE, "%*s}\n", indent, "");
}
#endif
