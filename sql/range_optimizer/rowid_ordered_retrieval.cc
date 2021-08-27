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
    THD *thd, TABLE *table_arg, ha_rows *examined_rows, bool retrieve_full_rows,
    bool need_rows_in_rowid_order, MEM_ROOT *return_mem_root)
    : QUICK_SELECT_I(thd, table_arg, examined_rows),
      mem_root(return_mem_root),
      cpk_quick(nullptr),
      retrieve_full_rows(retrieve_full_rows),
      scans_inited(false),
      need_rows_in_rowid_order(need_rows_in_rowid_order) {
  last_rowid = mem_root->ArrayAlloc<uchar>(table()->file->ref_length);
}

/*
  Initialize this quick select to be a ROR-merged scan.

  SYNOPSIS
    QUICK_RANGE_SELECT::init_ror_merged_scan()

  NOTES
    This function creates and prepares for subsequent use a separate handler
    object if it can't reuse table()->file. The reason for this is that during
    ROR-merge several key scans are performed simultaneously, and a single
    handler is only capable of preserving context of a single key scan.

    In ROR-merge the quick select doing merge does full records retrieval,
    merged quick selects read only keys.

  RETURN
    true if error
 */
bool QUICK_RANGE_SELECT::init_ror_merged_scan() {
  handler *save_file = file, *org_file;
  MY_BITMAP *const save_read_set = table()->read_set;
  MY_BITMAP *const save_write_set = table()->write_set;
  DBUG_TRACE;

  THD *thd = current_thd;

  in_ror_merged_scan = true;
  mrr_flags |= HA_MRR_SORTED;
  if (reuse_handler) {
    DBUG_PRINT("info", ("Reusing handler %p", file));
    if (shared_init()) {
      return true;
    }
    if (shared_reset()) {
      return true;
    }
    table()->column_bitmaps_set(&column_bitmap, &column_bitmap);
    file->ha_extra(HA_EXTRA_SECONDARY_SORT_ROWID);
    goto end;
  }

  /* Create a separate handler object for this quick select */
  if (free_file) {
    /* already have own 'handler' object. */
    return false;
  }

  if (!(file =
            table()->file->clone(table()->s->normalized_path.str, mem_root))) {
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

  table()->column_bitmaps_set(&column_bitmap, &column_bitmap);

  if (file->ha_external_lock(thd, table()->file->get_lock_type())) goto failure;

  if (shared_init() || shared_reset()) {
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
    The following sets table()->tmp_set to only use this key and then updates
    table()->read_set and table()->write_set to use this bitmap.
    The new bitmap is stored in 'column_bitmap' which is used in ::Read()
  */
  org_file = table()->file;
  table()->file = file;
  /* We don't have to set 'table()->keyread' here as the 'file' is unique */
  if (!table()->no_keyread) table()->mark_columns_used_by_index(index);
  table()->prepare_for_position();
  table()->file = org_file;
  bitmap_copy(&column_bitmap, table()->read_set);

  /*
    We have prepared a column_bitmap which Read() will use. To do this we
    used TABLE::read_set/write_set as playground; restore them to their
    original value to not pollute other scans.
  */
  table()->column_bitmaps_set(save_read_set, save_write_set);
  bitmap_clear_all(&table()->tmp_set);

  return false;

failure:
  table()->column_bitmaps_set(save_read_set, save_write_set);
  destroy(file);
  file = save_file;
  return true;
}

/*
  Initialize this quick select to be a part of a ROR-merged scan.
  Returns true if error.
 */
bool QUICK_ROR_INTERSECT_SELECT::init_ror_merged_scan() {
  DBUG_TRACE;

#ifndef NDEBUG
  /* Initialize all merged "children" quick selects */
  for (QUICK_RANGE_SELECT &quick : quick_selects) {
    const MY_BITMAP *const save_read_set = quick.table()->read_set;
    const MY_BITMAP *const save_write_set = quick.table()->write_set;
    // Sets are shared by all members of "quick_selects" so must not change
    assert(quick.table()->read_set == save_read_set);
    assert(quick.table()->write_set == save_write_set);
    /* All merged scans share the same record buffer in intersection. */
    assert(quick.table() == table());
    assert(quick.table()->record[0] == table()->record[0]);
  }
#endif

  /* Prepare for ha_rnd_pos calls if needed. */
  int error;
  if (retrieve_full_rows && (error = table()->file->ha_rnd_init(false))) {
    DBUG_PRINT("error", ("ROR index_merge rnd_init call failed"));
    table()->file->print_error(error, MYF(0));
    return true;
  }
  return false;
}

bool QUICK_ROR_INTERSECT_SELECT::Init() {
  DBUG_TRACE;
  if (!inited) {
    /* Check if last_rowid was successfully allocated in ctor */
    if (last_rowid == nullptr) {
      table()->file->print_error(HA_ERR_OUT_OF_MEM, MYF(0));
      return true;
    }

    if (need_rows_in_rowid_order) {
      if (init_ror_merged_scan()) {
        return true;
      }
    }
    inited = true;
  }

  if (!scans_inited && init_ror_merged_scan()) return true;
  scans_inited = true;
  for (QUICK_RANGE_SELECT &quick : quick_selects) {
    if (quick.Init()) {
      return true;
    }
  }
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
  if (retrieve_full_rows && table()->file->inited) table()->file->ha_rnd_end();
}

QUICK_ROR_UNION_SELECT::QUICK_ROR_UNION_SELECT(MEM_ROOT *return_mem_root,
                                               THD *thd, TABLE *table,
                                               ha_rows *examined_rows)
    : QUICK_SELECT_I(thd, table, examined_rows),
      queue(Quick_ror_union_less(table->file),
            Malloc_allocator<PSI_memory_key>(PSI_INSTRUMENT_ME)),
      mem_root(return_mem_root),
      scans_inited(false) {
  rowid_length = table->file->ref_length;
}

bool QUICK_ROR_UNION_SELECT::Init() {
  if (!inited) {
    if (queue.reserve(quick_selects.size())) {
      table()->file->print_error(HA_ERR_OUT_OF_MEM, MYF(0));
      return true;
    }

    if (!(cur_rowid =
              mem_root->ArrayAlloc<uchar>(2 * table()->file->ref_length))) {
      table()->file->print_error(HA_ERR_OUT_OF_MEM, MYF(0));
      return true;
    }
    prev_rowid = cur_rowid + table()->file->ref_length;
    inited = true;
  }

  int error;
  DBUG_TRACE;
  have_prev_rowid = false;
  if (!scans_inited) {
    scans_inited = true;
  }
  queue.clear();
  /*
    Initialize scans for merged quick selects and put all merged quick
    selects into the queue.
  */
  for (QUICK_SELECT_I &quick : quick_selects) {
    if (quick.Init()) return true;
    int result = quick.Read();
    if (result == 1) {
      return true;
    } else if (result == 0) {
      queue.push(&quick);
    }
  }

  /* Prepare for ha_rnd_pos calls. */
  if (table()->file->inited && (error = table()->file->ha_rnd_end())) {
    DBUG_PRINT("error", ("ROR index_merge rnd_end call failed"));
    table()->file->print_error(error, MYF(0));
    return true;
  }
  if ((error = table()->file->ha_rnd_init(false))) {
    DBUG_PRINT("error", ("ROR index_merge rnd_init call failed"));
    table()->file->print_error(error, MYF(0));
    return true;
  }

  return false;
}

bool QUICK_ROR_UNION_SELECT::push_quick_back(QUICK_SELECT_I *quick_sel_range) {
  return quick_selects.push_back(quick_sel_range);
}

QUICK_ROR_UNION_SELECT::~QUICK_ROR_UNION_SELECT() {
  DBUG_TRACE;
  quick_selects.destroy_elements();
  if (table()->file->inited) table()->file->ha_rnd_end();
}

/*
  Retrieve next record.
  SYNOPSIS
     QUICK_ROR_INTERSECT_SELECT::Read()

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
    See RowIterator::Read()
 */
int QUICK_ROR_INTERSECT_SELECT::Read() {
  List_iterator_fast<QUICK_RANGE_SELECT> quick_it(quick_selects);

  DBUG_TRACE;

  for (;;) {  // Termination condition within loop.
    /* Get a rowid for first quick and save it as a 'candidate' */
    QUICK_RANGE_SELECT *quick = quick_it++;
    if (int error = quick->Read(); error != 0) {
      return error;
    }
    if (cpk_quick) {
      while (!cpk_quick->row_in_ranges()) {
        quick->UnlockRow(); /* row not in range; unlock */
        if (int error = quick->Read(); error != 0) {
          return error;
        }
      }
    }

    memcpy(last_rowid, quick->file->ref, table()->file->ref_length);

    /* quick that reads the given rowid first. This is needed in order
    to be able to unlock the row using the same handler object that locked
    it */
    QUICK_RANGE_SELECT *quick_with_last_rowid = quick;

    uint last_rowid_count = 1;
    while (last_rowid_count < quick_selects.size()) {
      if (!(quick = quick_it++)) {
        quick_it.rewind();
        quick = quick_it++;
      }

      int cmp;
      do {
        DBUG_EXECUTE_IF("innodb_quick_report_deadlock",
                        DBUG_SET("+d,innodb_report_deadlock"););
        if (int error = quick->Read(); error != 0) {
          /* On certain errors like deadlock, trx might be rolled back.*/
          if (!current_thd->transaction_rollback_request)
            quick_with_last_rowid->UnlockRow();
          return error;
        }
        cmp = table()->file->cmp_ref(quick->file->ref, last_rowid);
        if (cmp < 0) {
          /* This row is being skipped.  Release lock on
           * it. */
          quick->UnlockRow();
        }
      } while (cmp < 0);

      /* Ok, current select 'caught up' and returned ref >= cur_ref */
      if (cmp > 0) {
        /* Found a row with ref > cur_ref. Make it a new 'candidate' */
        if (cpk_quick) {
          while (!cpk_quick->row_in_ranges()) {
            quick->UnlockRow(); /* row not in range; unlock */
            if (int error = quick->Read(); error != 0) {
              /* On certain errors like deadlock, trx might be rolled back.*/
              if (!current_thd->transaction_rollback_request)
                quick_with_last_rowid->UnlockRow();
              return error;
            }
          }
        }
        memcpy(last_rowid, quick->file->ref, table()->file->ref_length);
        quick_with_last_rowid->UnlockRow();
        last_rowid_count = 1;
        quick_with_last_rowid = quick;
      } else {
        /* current 'candidate' row confirmed by this select */
        last_rowid_count++;
      }
    }

    /* We get here if we got the same row ref in all scans. */
    if (retrieve_full_rows) {
      int error = table()->file->ha_rnd_pos(table()->record[0], last_rowid);
      if (error == HA_ERR_RECORD_DELETED) {
        // The row was deleted, so we need to loop back.
        continue;
      }
      if (error == 0) {
        return 0;
      }
      return HandleError(error);
    } else {
      return 0;
    }
  }
}

/*
  Retrieve next record.
  SYNOPSIS
    QUICK_ROR_UNION_SELECT::Read()

  NOTES
    Enter/exit invariant:
    For each quick select in the queue a {key,rowid} tuple has been
    retrieved but the corresponding row hasn't been passed to output.

  RETURN
    See RowIterator::Read()
 */
int QUICK_ROR_UNION_SELECT::Read() {
  DBUG_TRACE;

  for (;;) {  // Termination condition within loop.
    bool dup_row;
    do {
      if (queue.empty()) return -1;
      /* Ok, we have a queue with >= 1 scans */

      QUICK_SELECT_I *quick = queue.top();
      memcpy(cur_rowid, quick->last_rowid, rowid_length);

      /* put into queue rowid from the same stream as top element */
      if (int ret = quick->Read(); ret != 0) {
        if (ret != -1) return ret;
        queue.pop();
      } else {
        queue.update_top();
      }

      if (!have_prev_rowid) {
        /* No rows have been returned yet */
        dup_row = false;
        have_prev_rowid = true;
      } else
        dup_row = !table()->file->cmp_ref(cur_rowid, prev_rowid);
    } while (dup_row);

    using std::swap;
    swap(cur_rowid, prev_rowid);

    int error = table()->file->ha_rnd_pos(table()->record[0], prev_rowid);
    if (error == HA_ERR_RECORD_DELETED) {
      // The row was deleted, so we need to loop back.
      continue;
    }
    if (error == 0) {
      return 0;
    }
    return HandleError(error);
  }
}
