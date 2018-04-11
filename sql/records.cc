/* Copyright (c) 2000, 2018, Oracle and/or its affiliates. All rights reserved.

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

/**
  @file

  @brief
  Functions for easy reading of records, possible through a cache
*/

#include "sql/records.h"

#include <string.h>
#include <atomic>

#include "my_byteorder.h"
#include "my_dbug.h"
#include "my_pointer_arithmetic.h"
#include "my_sys.h"
#include "my_thread_local.h"
#include "mysql/service_mysql_alloc.h"
#include "sql/field.h"
#include "sql/filesort.h"  // filesort_free_buffers
#include "sql/handler.h"
#include "sql/item.h"
#include "sql/opt_range.h"  // QUICK_SELECT_I
#include "sql/psi_memory_key.h"
#include "sql/sort_param.h"
#include "sql/sql_class.h"  // THD
#include "sql/sql_const.h"
#include "sql/sql_executor.h"  // QEP_TAB
#include "sql/sql_sort.h"
#include "sql/system_variables.h"
#include "sql/table.h"
#include "thr_lock.h"
#include "varlen_sort.h"

static int rr_quick(READ_RECORD *info);
static int rr_from_tempfile(READ_RECORD *info);
template <bool>
static int rr_unpack_from_tempfile(READ_RECORD *info);
template <bool>
static int rr_unpack_from_buffer(READ_RECORD *info);
static int rr_from_pointers(READ_RECORD *info);
static int rr_from_cache(READ_RECORD *info);
static int init_rr_cache(THD *thd, READ_RECORD *info);
static int rr_index_first(READ_RECORD *info);
static int rr_index_last(READ_RECORD *info);
static int rr_index(READ_RECORD *info);
static int rr_index_desc(READ_RECORD *info);
static void end_read_record_unique(READ_RECORD *info);
static void end_read_record_sort(READ_RECORD *info);

/**
  Initialize READ_RECORD structure to perform full index scan in desired
  direction using read_record.read_record() interface

    This function has been added at late stage and is used only by
    UPDATE/DELETE. Other statements perform index scans using
    join_read_first/next functions.

  @param info         READ_RECORD structure to initialize.
  @param thd          Thread handle
  @param table        Table to be accessed
  @param idx          index to scan
  @param reverse      Scan in the reverse direction

  @retval true   error
  @retval false  success
*/

bool init_read_record_idx(READ_RECORD *info, THD *thd, TABLE *table, uint idx,
                          bool reverse) {
  int error;
  empty_record(table);
  new (info) READ_RECORD;
  info->thd = thd;
  info->table = table;
  info->record = table->record[0];
  info->unlock_row = rr_unlock_row;

  if (!table->file->inited && (error = table->file->ha_index_init(idx, 1))) {
    table->file->print_error(error, MYF(0));
    return true;
  }

  /* read_record will be changed to rr_index in rr_index_first */
  info->read_record = reverse ? rr_index_last : rr_index_first;
  return false;
}

/*
  init_read_record is used to scan by using a number of different methods.
  Which method to use is set-up in this call so that later calls to
  the info->read_record will call the appropriate method using a function
  pointer.

  There are five methods that relate completely to the sort function
  filesort. The result of a filesort is retrieved using read_record
  calls. The other two methods are used for normal table access.

  The filesort will produce references to the records sorted, these
  references can be stored in memory or in a temporary file.

  The temporary file is normally used when the references doesn't fit into
  a properly sized memory buffer. For most small queries the references
  are stored in the memory buffer.
  SYNOPSIS
    init_read_record()
      info              OUT read structure
      thd               Thread handle
      table             Table the data [originally] comes from; if NULL,
      'table' is inferred from 'qep_tab'; if non-NULL, 'qep_tab' must be NULL.
      qep_tab           QEP_TAB for 'table', if there is one; we may use
      qep_tab->quick() as data source
      disable_rr_cache  Don't use rr_from_cache (used by sort-union
                        index-merge which produces rowid sequences that
                        are already ordered)

  DESCRIPTION
    This function sets up reading data via one of the methods:

  The temporary file is also used when performing an update where a key is
  modified.

  Methods used when ref's are in memory (using rr_from_pointers):
    rr_unpack_from_buffer:
    ----------------------
      This method is used when table->sort.addon_field is allocated.
      This is allocated for most SELECT queries not involving any BLOB's.
      In this case the records are fetched from a memory buffer.
    rr_from_pointers:
    -----------------
      Used when the above is not true, UPDATE, DELETE and so forth and
      SELECT's involving BLOB's. It is also used when the addon_field
      buffer is not allocated due to that its size was bigger than the
      session variable max_length_for_sort_data. Finally, it is used for
      the result of Unique, which returns row IDs in the same format as
      filesort.
      In this case the record data is fetched from the handler using the
      saved reference using the rnd_pos handler call.

  Methods used when ref's are in a temporary file (using rr_from_tempfile)
    rr_unpack_from_tempfile:
    ------------------------
      Same as rr_unpack_from_buffer except that references are fetched from
      temporary file. Should obviously not really happen other than in
      strange configurations.

    rr_from_tempfile:
    -----------------
      Same as rr_from_pointers except that references are fetched from
      temporary file instead of from
    rr_from_cache:
    --------------
      This is a special variant of rr_from_tempfile that can be used for
      handlers that is not using the HA_FAST_KEY_READ table flag. Instead
      of reading the references one by one from the temporary file it reads
      a set of them, sorts them and reads all of them into a buffer which
      is then used for a number of subsequent calls to rr_from_cache.
      It is only used for SELECT queries and a number of other conditions
      on table size.

  All other accesses use either index access methods (rr_quick) or a full
  table scan (rr_sequential).
  rr_quick:
  ---------
    rr_quick uses one of the QUICK_SELECT classes in opt_range.cc to
    perform an index scan. There are loads of functionality hidden
    in these quick classes. It handles all index scans of various kinds.
  rr_sequential:
  --------------
    This is the most basic access method of a table using rnd_init,
    ha_rnd_next and rnd_end. No indexes are used.

  @retval true   error
  @retval false  success
*/
bool init_read_record(READ_RECORD *info, THD *thd, TABLE *table,
                      QEP_TAB *qep_tab, bool disable_rr_cache) {
  int error = 0;
  DBUG_ENTER("init_read_record");

  // If only 'table' is given, assume no quick, no condition.
  DBUG_ASSERT(!(table && qep_tab));
  if (!table) table = qep_tab->table();

  new (info) READ_RECORD;
  info->thd = thd;
  info->table = table;
  info->forms = &info->table; /* Only one table */

  if (table->sort_result.has_result() && table->sort.using_addon_fields()) {
    info->rec_buf = table->sort.addon_fields->get_addon_buf();
    info->ref_length = table->sort.addon_fields->get_addon_buf_length();
  } else {
    empty_record(table);
    info->record = table->record[0];
    info->ref_length = table->file->ref_length;
  }
  info->quick = qep_tab ? qep_tab->quick() : NULL;
  info->unlock_row = rr_unlock_row;
  info->ignore_not_found_rows = 0;

  IO_CACHE *tempfile = nullptr;
  if (info->quick && info->quick->clustered_pk_range()) {
    /*
      In case of QUICK_INDEX_MERGE_SELECT with clustered pk range we have to
      use its own access method(i.e QUICK_INDEX_MERGE_SELECT::get_next()) as
      sort file does not contain rowids which satisfy clustered pk range.
    */
  }
  /*
    We test for a Unique result before a filesort result, because on
    any given table, we can have Unique sending its result to filesort
    (in which case filesort would be half-initialized at this point),
    but not the other way round. It's possible that we should actually
    have a “finished” flag instead, though.
  */
  else if (table->unique_result.io_cache &&
           my_b_inited(table->unique_result.io_cache)) {
    tempfile = table->unique_result.io_cache;
    info->read_record = rr_from_tempfile;
  } else if (table->sort_result.io_cache &&
             my_b_inited(table->sort_result.io_cache)) {
    tempfile = table->sort_result.io_cache;

    // Test if ref-records was used
    if (table->sort.using_addon_fields()) {
      DBUG_PRINT("info", ("using rr_unpack_from_tempfile"));
      if (table->sort.addon_fields->using_packed_addons())
        info->read_record = rr_unpack_from_tempfile<true>;
      else
        info->read_record = rr_unpack_from_tempfile<false>;
    } else {
      DBUG_PRINT("info", ("using rr_from_tempfile"));
      info->read_record = rr_from_tempfile;
    }
  }

  if (tempfile) {
    info->io_cache = tempfile;
    reinit_io_cache(info->io_cache, READ_CACHE, 0L, 0, 0);
    info->ref_pos = table->file->ref;
    if (!table->file->inited && (error = table->file->ha_rnd_init(0))) goto err;

    /*
      table->sort.addon_field is checked because if we use addon fields,
      it doesn't make sense to use cache - we don't read from the table
      and table->sort.io_cache is read sequentially
    */
    if (!disable_rr_cache && !table->sort.using_addon_fields() &&
        thd->variables.read_rnd_buff_size &&
        !(table->file->ha_table_flags() & HA_FAST_KEY_READ) &&
        (table->db_stat & HA_READ_ONLY ||
         table->reginfo.lock_type <= TL_READ_NO_INSERT) &&
        (ulonglong)table->s->reclength *
                (table->file->stats.records + table->file->stats.deleted) >
            (ulonglong)MIN_FILE_LENGTH_TO_USE_ROW_CACHE &&
        info->io_cache->end_of_file / info->ref_length * table->s->reclength >
            (my_off_t)MIN_ROWS_TO_USE_TABLE_CACHE &&
        !table->s->blob_fields && info->ref_length <= MAX_REFLENGTH) {
      if (init_rr_cache(thd, info)) goto skip_caching;
      DBUG_PRINT("info", ("using rr_from_cache"));
      info->read_record = rr_from_cache;
    }
  } else if (info->quick) {
    DBUG_PRINT("info", ("using rr_quick"));
    info->read_record = rr_quick;
  }
  /*
    See further up in the function for why we test for Unique before filesort.
  */
  else if (table->unique_result.has_result_in_memory()) {
    /*
      The Unique class never puts its results into table->sort's
      Filesort_buffer.
    */
    DBUG_ASSERT(!table->unique_result.sorted_result_in_fsbuf);

    if ((error = table->file->ha_rnd_init(0))) goto err;

    info->cache_pos = table->unique_result.sorted_result.get();
    DBUG_PRINT("info", ("using rr_from_pointers (unique)"));
    info->read_record = rr_from_pointers;
    info->cleanup = end_read_record_unique;
    info->cache_end =
        info->cache_pos + table->unique_result.found_records * info->ref_length;
  }
  // See save_index(), which stores the filesort result set.
  else if (table->sort_result.has_result_in_memory()) {
    if ((error = table->file->ha_rnd_init(0))) goto err;

    info->cache_pos = table->sort_result.sorted_result.get();
    if (table->sort.using_addon_fields()) {
      DBUG_PRINT("info", ("using rr_unpack_from_buffer (sort)"));
      DBUG_ASSERT(table->sort_result.sorted_result_in_fsbuf);
      info->unpack_counter = 0;
      if (table->sort.addon_fields->using_packed_addons())
        info->read_record = rr_unpack_from_buffer<true>;
      else
        info->read_record = rr_unpack_from_buffer<false>;
      info->cache_end = table->sort_result.sorted_result_end;
    } else {
      DBUG_PRINT("info", ("using rr_from_pointers"));
      info->read_record = rr_from_pointers;
      info->cache_end =
          info->cache_pos + table->sort_result.found_records * info->ref_length;
    }
    info->cleanup = end_read_record_sort;
  } else {
    DBUG_PRINT("info", ("using rr_sequential"));
    info->read_record = rr_sequential;
    if ((error = table->file->ha_rnd_init(1))) goto err;
  }

skip_caching:
  /*
    Do condition pushdown for UPDATE/DELETE.
    TODO: Remove this from here as it causes two condition pushdown calls
    when we're running a SELECT and the condition cannot be pushed down.
    Some temporary tables do not have a TABLE_LIST object, and it is never
    needed to push down conditions (ECP) for such tables.
  */
  if (thd->optimizer_switch_flag(OPTIMIZER_SWITCH_ENGINE_CONDITION_PUSHDOWN) &&
      qep_tab && qep_tab->condition() && table->pos_in_table_list &&
      (qep_tab->condition()->used_tables() & table->pos_in_table_list->map()) &&
      !table->file->pushed_cond)
    table->file->cond_push(qep_tab->condition());

  DBUG_RETURN(false);

err:
  table->file->print_error(error, MYF(0));
  DBUG_RETURN(true);
} /* init_read_record */

void end_read_record(READ_RECORD *info) {
  if (info->cleanup) {
    info->cleanup(info);
  }
  /* free cache if used */
  if (info->cache) {
    my_free(info->cache);
    info->cache = 0;
  }
  if (info->table && info->table->key_read) {
    info->table->set_keyread(false);
  }
  if (info->table && info->table->is_created()) {
    if (info->read_record != rr_quick)  // otherwise quick_range does it
      (void)info->table->file->ha_index_or_rnd_end();
    info->table = 0;
  }
}

static int rr_handle_error(READ_RECORD *info, int error) {
  if (info->thd->killed) {
    info->thd->send_kill_message();
    return 1;
  }

  if (error == HA_ERR_END_OF_FILE)
    error = -1;
  else {
    info->table->file->print_error(error, MYF(0));
    if (error < 0)  // Fix negative BDB errno
      error = 1;
  }
  return error;
}

/** Read a record from head-database. */

static int rr_quick(READ_RECORD *info) {
  int tmp;
  while ((tmp = info->quick->get_next())) {
    if (info->thd->killed || (tmp != HA_ERR_RECORD_DELETED)) {
      tmp = rr_handle_error(info, tmp);
      break;
    }
  }

  return tmp;
}

/**
  Reads first row in an index scan.

  @param info  	Scan info

  @retval
    0   Ok
  @retval
    -1   End of records
  @retval
    1   Error
*/

static int rr_index_first(READ_RECORD *info) {
  int tmp = info->table->file->ha_index_first(info->record);
  info->read_record = rr_index;
  if (tmp) tmp = rr_handle_error(info, tmp);
  return tmp;
}

/**
  Reads last row in an index scan.

  @param info  	Scan info

  @retval
    0   Ok
  @retval
    -1   End of records
  @retval
    1   Error
*/

static int rr_index_last(READ_RECORD *info) {
  int tmp = info->table->file->ha_index_last(info->record);
  info->read_record = rr_index_desc;
  if (tmp) tmp = rr_handle_error(info, tmp);
  return tmp;
}

static void end_read_record_sort(READ_RECORD *info) {
  if (info->table) {
    info->table->sort_result.sorted_result.reset();
    info->table->sort_result.sorted_result_in_fsbuf = false;
  }
}

static void end_read_record_unique(READ_RECORD *info) {
  if (info->table) {
    info->table->unique_result.sorted_result.reset();
    DBUG_ASSERT(!info->table->unique_result.sorted_result_in_fsbuf);
    info->table->unique_result.sorted_result_in_fsbuf = false;
  }
}

/**
  Reads index sequentially after first row.

  Read the next index record (in forward direction) and translate return
  value.

  @param info  Scan info

  @retval
    0   Ok
  @retval
    -1   End of records
  @retval
    1   Error
*/

static int rr_index(READ_RECORD *info) {
  int tmp = info->table->file->ha_index_next(info->record);
  if (tmp) tmp = rr_handle_error(info, tmp);
  return tmp;
}

/**
  Reads index sequentially from the last row to the first.

  Read the prev index record (in backward direction) and translate return
  value.

  @param info  Scan info

  @retval
    0   Ok
  @retval
    -1   End of records
  @retval
    1   Error
*/

static int rr_index_desc(READ_RECORD *info) {
  int tmp = info->table->file->ha_index_prev(info->record);
  if (tmp) tmp = rr_handle_error(info, tmp);
  return tmp;
}

int rr_sequential(READ_RECORD *info) {
  int tmp;
  while ((tmp = info->table->file->ha_rnd_next(info->record))) {
    /*
      ha_rnd_next can return RECORD_DELETED for MyISAM when one thread is
      reading and another deleting without locks.
    */
    if (info->thd->killed || (tmp != HA_ERR_RECORD_DELETED)) {
      tmp = rr_handle_error(info, tmp);
      break;
    }
  }
  return tmp;
}

static int rr_from_tempfile(READ_RECORD *info) {
  int tmp;
  for (;;) {
    if (my_b_read(info->io_cache, info->ref_pos, info->ref_length))
      return -1; /* End of file */
    if (!(tmp = info->table->file->ha_rnd_pos(info->record, info->ref_pos)))
      break;
    /* The following is extremely unlikely to happen */
    if (tmp == HA_ERR_RECORD_DELETED ||
        (tmp == HA_ERR_KEY_NOT_FOUND && info->ignore_not_found_rows))
      continue;
    tmp = rr_handle_error(info, tmp);
    break;
  }
  return tmp;
} /* rr_from_tempfile */

template <bool Packed_addon_fields>
inline void Filesort_info::unpack_addon_fields(uchar *buff) {
  Sort_addon_field *addonf = addon_fields->begin();

  const uchar *start_of_record = buff + addonf->offset;

  for (; addonf != addon_fields->end(); ++addonf) {
    Field *field = addonf->field;
    if (addonf->null_bit && (addonf->null_bit & buff[addonf->null_offset])) {
      field->set_null();
      continue;
    }
    field->set_notnull();
    if (Packed_addon_fields)
      start_of_record = field->unpack(field->ptr, start_of_record);
    else
      field->unpack(field->ptr, buff + addonf->offset);
  }
}

/**
  Read a result set record from a temporary file after sorting.

  The function first reads the next sorted record from the temporary file.
  into a buffer. If a success it calls a callback function that unpacks
  the fields values use in the result set from this buffer into their
  positions in the regular record buffer.

  @param info          Reference to the context including record descriptors
  @tparam Packed_addon_fields Are the addon fields packed?
     This is a compile-time constant, to avoid if (....) tests during execution.
  @retval
    0   Record successfully read.
  @retval
    -1   There is no record to be read anymore.
*/
template <bool Packed_addon_fields>
static int rr_unpack_from_tempfile(READ_RECORD *info) {
  uchar *destination = info->rec_buf;
#ifndef DBUG_OFF
  my_off_t where = my_b_tell(info->io_cache);
#endif
  if (Packed_addon_fields) {
    const uint len_sz = Addon_fields::size_of_length_field;

    // First read length of the record.
    if (my_b_read(info->io_cache, destination, len_sz)) return -1;
    uint res_length = Addon_fields::read_addon_length(destination);
    DBUG_PRINT("info",
               ("rr_unpack from %llu to %p sz %u",
                static_cast<ulonglong>(where), destination, res_length));
    DBUG_ASSERT(res_length > len_sz);
    DBUG_ASSERT(info->table->sort.using_addon_fields());

    // Then read the rest of the record.
    if (my_b_read(info->io_cache, destination + len_sz, res_length - len_sz))
      return -1; /* purecov: inspected */
  } else {
    if (my_b_read(info->io_cache, destination, info->ref_length)) return -1;
  }

  info->table->sort.unpack_addon_fields<Packed_addon_fields>(destination);

  return 0;
}

static int rr_from_pointers(READ_RECORD *info) {
  int tmp;
  uchar *cache_pos;

  for (;;) {
    if (info->cache_pos == info->cache_end) return -1; /* End of file */
    cache_pos = info->cache_pos;
    info->cache_pos += info->ref_length;

    if (!(tmp = info->table->file->ha_rnd_pos(info->record, cache_pos))) break;

    /* The following is extremely unlikely to happen */
    if (tmp == HA_ERR_RECORD_DELETED ||
        (tmp == HA_ERR_KEY_NOT_FOUND && info->ignore_not_found_rows))
      continue;
    tmp = rr_handle_error(info, tmp);
    break;
  }
  return tmp;
}

/**
  Read a result set record from a buffer after sorting.

  Get the next record from the filesort buffer,
  then unpack the fields into their positions in the regular record buffer.

  @param info          Reference to the context including record descriptors
  @tparam Packed_addon_fields Are the addon fields packed?
     This is a compile-time constant, to avoid if (....) tests during execution.

  TODO: consider templatizing on is_varlen as well.
  Variable / Fixed size key is currently handled by
  Filesort_info::get_start_of_payload

  @retval
    0   Record successfully read.
  @retval
    -1   There is no record to be read anymore.
*/
template <bool Packed_addon_fields>
static int rr_unpack_from_buffer(READ_RECORD *info) {
  if (info->unpack_counter == info->table->sort_result.found_records)
    return -1; /* End of buffer */

  uchar *record = info->table->sort.get_sorted_record(
      static_cast<uint>(info->unpack_counter));
  uchar *payload = get_start_of_payload(&info->table->sort, record);
  info->table->sort.unpack_addon_fields<Packed_addon_fields>(payload);
  info->unpack_counter++;
  return 0;
}
/* cacheing of records from a database */

/**
  Initialize caching of records from temporary file.

  @retval
    0 OK, use caching.
    1 Buffer is too small, or cannot be allocated.
      Skip caching, and read records directly from temporary file.
 */
static int init_rr_cache(THD *thd, READ_RECORD *info) {
  uint rec_cache_size;
  DBUG_ENTER("init_rr_cache");

  READ_RECORD info_copy = *info;
  info->struct_length = 3 + MAX_REFLENGTH;
  info->reclength = ALIGN_SIZE(info->table->s->reclength + 1);
  if (info->reclength < info->struct_length)
    info->reclength = ALIGN_SIZE(info->struct_length);

  info->error_offset = info->table->s->reclength;
  info->cache_records = (thd->variables.read_rnd_buff_size /
                         (info->reclength + info->struct_length));
  rec_cache_size = info->cache_records * info->reclength;
  info->rec_cache_size = info->cache_records * info->ref_length;

  if (info->cache_records <= 2 ||
      !(info->cache = (uchar *)my_malloc(
            key_memory_READ_RECORD_cache,
            rec_cache_size + info->cache_records * info->struct_length,
            MYF(0)))) {
    *info = info_copy;
    DBUG_RETURN(1);
  }
  DBUG_PRINT("info", ("Allocated buffert for %d records", info->cache_records));
  info->read_positions = info->cache + rec_cache_size;
  info->cache_pos = info->cache_end = info->cache;
  DBUG_RETURN(0);
} /* init_rr_cache */

static int rr_from_cache(READ_RECORD *info) {
  uint i;
  ulong length;
  my_off_t rest_of_file;
  int16 error;
  uchar *position, *ref_position, *record_pos;
  ulong record;

  for (;;) {
    if (info->cache_pos != info->cache_end) {
      if (info->cache_pos[info->error_offset]) {
        shortget(&error, info->cache_pos);
        info->table->file->print_error(error, MYF(0));
      } else {
        error = 0;
        memcpy(info->record, info->cache_pos,
               (size_t)info->table->s->reclength);
      }
      info->cache_pos += info->reclength;
      return ((int)error);
    }
    length = info->rec_cache_size;
    rest_of_file = info->io_cache->end_of_file - my_b_tell(info->io_cache);
    if ((my_off_t)length > rest_of_file) length = (ulong)rest_of_file;
    if (!length || my_b_read(info->io_cache, info->cache, length)) {
      DBUG_PRINT("info", ("Found end of file"));
      return -1; /* End of file */
    }

    length /= info->ref_length;
    position = info->cache;
    ref_position = info->read_positions;
    for (i = 0; i < length; i++, position += info->ref_length) {
      memcpy(ref_position, position, (size_t)info->ref_length);
      ref_position += MAX_REFLENGTH;
      int3store(ref_position, (long)i);
      ref_position += 3;
    }
    size_t ref_length = info->ref_length;
    DBUG_ASSERT(ref_length <= MAX_REFLENGTH);
    varlen_sort(info->read_positions,
                info->read_positions + length * info->struct_length,
                info->struct_length,
                [ref_length](const uchar *a, const uchar *b) {
                  return memcmp(a, b, ref_length) < 0;
                });

    position = info->read_positions;
    for (i = 0; i < length; i++) {
      memcpy(info->ref_pos, position, (size_t)info->ref_length);
      position += MAX_REFLENGTH;
      record = uint3korr(position);
      position += 3;
      record_pos = info->cache + record * info->reclength;
      error = (int16)info->table->file->ha_rnd_pos(record_pos, info->ref_pos);
      if (error) {
        record_pos[info->error_offset] = 1;
        shortstore(record_pos, error);
        DBUG_PRINT("error", ("Got error: %d:%d when reading row", my_errno(),
                             (int)error));
      } else
        record_pos[info->error_offset] = 0;
    }
    info->cache_end =
        (info->cache_pos = info->cache) + length * info->reclength;
  }
} /* rr_from_cache */

/**
  The default implementation of unlock-row method of READ_RECORD,
  used in all access methods.
*/

void rr_unlock_row(QEP_TAB *tab) {
  READ_RECORD *info = &tab->read_record;
  info->table->file->unlock_row();
}
