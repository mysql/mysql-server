/*
   Copyright (c) 2000, 2018, Oracle and/or its affiliates. All rights reserved.

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
  @file sql/filesort.cc
  Sorts a database.
*/

#include "sql/filesort.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <atomic>
#include <limits>
#include <memory>
#include <new>
#include <type_traits>
#include <vector>

#include "add_with_saturate.h"
#include "binary_log_types.h"
#include "binlog_config.h"
#include "decimal.h"
#include "m_ctype.h"
#include "my_bitmap.h"
#include "my_byteorder.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_loglevel.h"
#include "my_macros.h"
#include "my_pointer_arithmetic.h"
#include "my_sys.h"
#include "mysql/components/services/log_builtins.h"
#include "mysql/components/services/log_shared.h"
#include "mysql/psi/mysql_file.h"
#include "mysql/service_mysql_alloc.h"
#include "mysql/udf_registration_types.h"
#include "mysql_com.h"
#include "mysqld_error.h"
#include "priority_queue.h"
#include "sql/auth/sql_security_ctx.h"
#include "sql/bounded_queue.h"
#include "sql/cmp_varlen_keys.h"
#include "sql/current_thd.h"
#include "sql/debug_sync.h"
#include "sql/derror.h"
#include "sql/error_handler.h"
#include "sql/field.h"
#include "sql/filesort_utils.h"
#include "sql/handler.h"
#include "sql/item.h"
#include "sql/item_subselect.h"
#include "sql/json_dom.h"  // Json_wrapper
#include "sql/key_spec.h"
#include "sql/log.h"
#include "sql/malloc_allocator.h"
#include "sql/merge_many_buff.h"
#include "sql/my_decimal.h"
#include "sql/mysqld.h"  // mysql_tmpdir
#include "sql/opt_costmodel.h"
#include "sql/opt_range.h"  // QUICK
#include "sql/opt_trace.h"
#include "sql/opt_trace_context.h"
#include "sql/psi_memory_key.h"
#include "sql/sort_param.h"
#include "sql/sql_array.h"
#include "sql/sql_base.h"
#include "sql/sql_bitmap.h"
#include "sql/sql_class.h"
#include "sql/sql_const.h"
#include "sql/sql_error.h"
#include "sql/sql_executor.h"  // QEP_TAB
#include "sql/sql_lex.h"
#include "sql/sql_optimizer.h"  // JOIN
#include "sql/sql_sort.h"
#include "sql/system_variables.h"
#include "sql/table.h"
#include "sql/thr_malloc.h"
#include "sql_string.h"
#include "template_utils.h"

using Mysql::Nullable;
using std::max;
using std::min;

namespace {

struct Mem_compare_queue_key {
  Mem_compare_queue_key() : m_compare_length(0), m_param(nullptr) {}

  Mem_compare_queue_key(const Mem_compare_queue_key &that)
      : m_compare_length(that.m_compare_length), m_param(that.m_param) {}

  bool operator()(const uchar *s1, const uchar *s2) const {
    if (m_param)
      return cmp_varlen_keys(m_param->local_sortorder, m_param->use_hash, s1,
                             s2);

    // memcmp(s1, s2, 0) is guaranteed to return zero.
    return memcmp(s1, s2, m_compare_length) < 0;
  }

  size_t m_compare_length;
  Sort_param *m_param;
};

}  // namespace

/* functions defined in this file */

static ha_rows read_all_rows(
    THD *thd, Sort_param *param, QEP_TAB *qep_tab, Filesort_info *fs_info,
    IO_CACHE *buffer_file, IO_CACHE *chunk_file,
    Bounded_queue<uchar *, uchar *, Sort_param, Mem_compare_queue_key> *pq,
    RowIterator *source_iterator, ha_rows *found_rows);
static int write_keys(Sort_param *param, Filesort_info *fs_info, uint count,
                      IO_CACHE *buffer_file, IO_CACHE *tempfile);
static void register_used_fields(Sort_param *param);
static int merge_index(THD *thd, Sort_param *param, Sort_buffer sort_buffer,
                       Merge_chunk_array chunk_array, IO_CACHE *tempfile,
                       IO_CACHE *outfile);
static bool save_index(Sort_param *param, uint count, Filesort_info *table_sort,
                       Sort_result *sort_result);

static bool check_if_pq_applicable(Opt_trace_context *trace, Sort_param *param,
                                   Filesort_info *info, TABLE *table,
                                   ha_rows records, ulong memory_available,
                                   bool keep_addon_fields);

void Sort_param::init_for_filesort(Filesort *file_sort,
                                   Bounds_checked_array<st_sort_field> sf_array,
                                   uint sortlen, TABLE *table,
                                   ulong max_length_for_sort_data,
                                   ha_rows maxrows, bool sort_positions) {
  DBUG_ASSERT(max_rows == 0);  // function should not be called twice
  m_fixed_sort_length = sortlen;
  m_force_stable_sort = file_sort->m_force_stable_sort;
  ref_length = table->file->ref_length;

  local_sortorder = sf_array;

  if (table->file->ha_table_flags() & HA_FAST_KEY_READ)
    m_addon_fields_status = Addon_fields_status::using_heap_table;
  else if (table->fulltext_searched)
    m_addon_fields_status = Addon_fields_status::fulltext_searched;
  else if (sort_positions)
    m_addon_fields_status = Addon_fields_status::keep_rowid;
  else {
    /*
      Get the descriptors of all fields whose values are appended
      to sorted fields and get its total length in m_addon_length.
    */
    addon_fields = file_sort->get_addon_fields(
        max_length_for_sort_data, table->field, m_fixed_sort_length,
        &m_addon_fields_status, &m_addon_length, &m_packable_length);
  }
  if (using_addon_fields()) {
    fixed_res_length = m_addon_length;
  } else {
    fixed_res_length = ref_length;
    /*
      The reference to the record is considered
      as an additional sorted field
    */
    AddWithSaturate(ref_length, &m_fixed_sort_length);
  }

  m_num_varlen_keys = count_varlen_keys();
  m_num_json_keys = count_json_keys();
  if (using_varlen_keys()) {
    AddWithSaturate(size_of_varlength_field, &m_fixed_sort_length);
  }
  /*
    Add hash at the end of sort key to order cut values correctly.
    Needed for GROUPing, rather than for ORDERing.
  */
  if (using_json_keys()) {
    use_hash = true;
    AddWithSaturate(sizeof(ulonglong), &m_fixed_sort_length);
  }

  m_fixed_rec_length = AddWithSaturate(m_fixed_sort_length, m_addon_length);
  max_rows = maxrows;
}

void Sort_param::try_to_pack_addons(ulong max_length_for_sort_data) {
  if (!using_addon_fields() ||  // no addons, or
      using_packed_addons())    // already packed
    return;

  if (!Addon_fields::can_pack_addon_fields(fixed_res_length)) {
    m_addon_fields_status = Addon_fields_status::row_too_large;
    return;
  }
  const uint sz = Addon_fields::size_of_length_field;
  if (m_fixed_rec_length + sz > max_length_for_sort_data) {
    m_addon_fields_status = Addon_fields_status::row_too_large;
    return;
  }

  // Heuristic: skip packing if potential savings are less than 10 bytes.
  if (m_packable_length < (10 + sz)) {
    m_addon_fields_status = Addon_fields_status::skip_heuristic;
    return;
  }

  Addon_fields_array::iterator addonf = addon_fields->begin();
  for (; addonf != addon_fields->end(); ++addonf) {
    addonf->offset += sz;
    addonf->null_offset += sz;
  }
  addon_fields->set_using_packed_addons(true);
  m_using_packed_addons = true;

  m_addon_length += sz;
  fixed_res_length += sz;
  m_fixed_rec_length += sz;
}

int Sort_param::count_varlen_keys() const {
  int retval = 0;
  for (const auto &sf : local_sortorder) {
    if (sf.is_varlen) {
      ++retval;
    }
  }
  return retval;
}

int Sort_param::count_json_keys() const {
  int retval = 0;
  for (const auto &sf : local_sortorder) {
    if (sf.field_type == MYSQL_TYPE_JSON) {
      ++retval;
    }
  }
  return retval;
}

size_t Sort_param::get_record_length(uchar *p) const {
  uchar *start_of_payload = get_start_of_payload(p);
  uint size_of_payload = using_packed_addons()
                             ? Addon_fields::read_addon_length(start_of_payload)
                             : fixed_res_length;
  uchar *end_of_payload = start_of_payload + size_of_payload;
  return end_of_payload - p;
}

void Sort_param::get_rec_and_res_len(uchar *record_start, uint *recl,
                                     uint *resl) {
  if (!using_packed_addons() && !using_varlen_keys()) {
    *recl = m_fixed_rec_length;
    *resl = fixed_res_length;
    return;
  }
  uchar *plen = get_start_of_payload(record_start);
  if (using_packed_addons())
    *resl = Addon_fields::read_addon_length(plen);
  else
    *resl = fixed_res_length;
  DBUG_ASSERT(*resl <= fixed_res_length);
  const uchar *record_end = plen + *resl;
  *recl = static_cast<uint>(record_end - record_start);
}

static void trace_filesort_information(Opt_trace_context *trace,
                                       const st_sort_field *sortorder,
                                       uint s_length) {
  if (!trace->is_started()) return;

  Opt_trace_array trace_filesort(trace, "filesort_information");
  for (; s_length--; sortorder++) {
    Opt_trace_object oto(trace);
    oto.add_alnum("direction", sortorder->reverse ? "desc" : "asc");

    if (sortorder->field) {
      TABLE *t = sortorder->field->table;
      if (strlen(t->alias) != 0)
        oto.add_utf8_table(t->pos_in_table_list);
      else
        oto.add_alnum("table", "intermediate_tmp_table");
      oto.add_alnum("field", sortorder->field->field_name
                                 ? sortorder->field->field_name
                                 : "tmp_table_column");
    } else
      oto.add("expression", sortorder->item);
  }
}

/**
  Sort a table.
  Creates a set of pointers that can be used to read the rows
  in sorted order. This should be done with the functions
  in records.cc.

  Before calling filesort, one must have done
  table->file->info(HA_STATUS_VARIABLE)

  The result set is stored in table->sort.io_cache or
  table->sort.sorted_result, or left in the main filesort buffer.

  @param      thd            Current thread
  @param      filesort       How to sort the table
  @param      sort_positions Set to true if we want to force sorting by position
                             (Needed by UPDATE/INSERT or ALTER TABLE or
                              when rowids are required by executor)
  @param      source_iterator Where to read the rows to be sorted from.
  @param      sort_result    Where to store the sort result.
  @param[out] found_rows     Store the number of found rows here.
                             This is the number of found rows after
                             applying WHERE condition.
  @param[out] returned_rows  Number of rows in the result, could be less than
                             found_rows if LIMIT is provided.

  @note
    If we sort by position (like if sort_positions is 1) filesort() will
    call table->prepare_for_position().

  @returns   False if success, true if error
*/

bool filesort(THD *thd, Filesort *filesort, bool sort_positions,
              RowIterator *source_iterator, Sort_result *sort_result,
              ha_rows *found_rows, ha_rows *returned_rows) {
  int error;
  ulong memory_available = thd->variables.sortbuff_size;
  ha_rows num_rows_found = HA_POS_ERROR;
  ha_rows num_rows_estimate = HA_POS_ERROR;
  IO_CACHE tempfile;    // Temporary file for storing intermediate results.
  IO_CACHE chunk_file;  // For saving Merge_chunk structs.
  IO_CACHE *outfile;    // Contains the final, sorted result.
  Sort_param param;
  Opt_trace_context *const trace = &thd->opt_trace;
  QEP_TAB *const qep_tab = filesort->qep_tab;
  TABLE *const table = qep_tab->table();
  ha_rows max_rows = filesort->limit;
  uint s_length = 0;

  DBUG_ENTER("filesort");

  if (!(s_length = filesort->sort_order_length()))
    DBUG_RETURN(true); /* purecov: inspected */

  /*
    We need a nameless wrapper, since we may be inside the "steps" of
    "join_execution".
  */
  Opt_trace_object trace_wrapper(trace);
  if (qep_tab->join())
    trace_wrapper.add("sorting_table_in_plan_at_position", qep_tab->idx());
  trace_filesort_information(trace, filesort->sortorder, s_length);

  DBUG_ASSERT(!table->reginfo.join_tab);
  DBUG_ASSERT(qep_tab == table->reginfo.qep_tab);
  Item_subselect *const subselect =
      qep_tab->join() ? qep_tab->join()->select_lex->master_unit()->item : NULL;

  DEBUG_SYNC(thd, "filesort_start");

  DBUG_ASSERT(sort_result->sorted_result == NULL);
  sort_result->sorted_result_in_fsbuf = false;

  outfile = sort_result->io_cache;
  my_b_clear(&tempfile);
  my_b_clear(&chunk_file);
  error = 1;

  param.init_for_filesort(filesort, make_array(filesort->sortorder, s_length),
                          sortlength(thd, filesort->sortorder, s_length), table,
                          thd->variables.max_length_for_sort_data, max_rows,
                          sort_positions);

  table->sort.addon_fields = param.addon_fields;

  /*
    TODO: Now that we read from RowIterators, the situation is a lot more
    complicated than just “quick is range scan, everything else is full scan”.
   */
  if (qep_tab->quick())
    thd->inc_status_sort_range();
  else
    thd->inc_status_sort_scan();

  // If number of rows is not known, use as much of sort buffer as possible.
  num_rows_estimate = table->file->estimate_rows_upper_bound();

  Bounded_queue<uchar *, uchar *, Sort_param, Mem_compare_queue_key> pq(
      param.max_record_length(),
      (Malloc_allocator<uchar *>(key_memory_Filesort_info_record_pointers)));

  if (check_if_pq_applicable(trace, &param, &table->sort, table,
                             num_rows_estimate, memory_available,
                             subselect != NULL)) {
    DBUG_PRINT("info", ("filesort PQ is applicable"));
    /*
      For PQ queries (with limit) we know exactly how many pointers/records
      we have in the buffer, so to simplify things, we initialize
      all pointers here. (We cannot pack fields anyways, so there is no
      point in doing incremental allocation).
     */
    if (table->sort.preallocate_records(param.max_rows_per_buffer)) {
      my_error(ER_OUT_OF_SORTMEMORY, ME_FATALERROR);
      LogErr(ERROR_LEVEL, ER_SERVER_OUT_OF_SORTMEMORY);
      goto err;
    }

    if (pq.init(param.max_rows, &param, table->sort.get_sort_keys())) {
      /*
       If we fail to init pq, we have to give up:
       out of memory means my_malloc() will call my_error().
      */
      DBUG_PRINT("info", ("failed to allocate PQ"));
      table->sort.free_sort_buffer();
      DBUG_ASSERT(thd->is_error());
      goto err;
    }
    filesort->using_pq = true;
    param.using_pq = true;
    param.m_addon_fields_status = Addon_fields_status::using_priority_queue;
  } else {
    DBUG_PRINT("info", ("filesort PQ is not applicable"));
    filesort->using_pq = false;
    param.using_pq = false;

    /*
      When sorting using priority queue, we cannot use packed addons.
      Without PQ, we can try.
    */
    param.try_to_pack_addons(thd->variables.max_length_for_sort_data);

    /*
      NOTE: param.max_rows_per_buffer is merely informative (for optimizer
      trace) in this case, not actually used.
    */
    if (num_rows_estimate < MERGEBUFF2) num_rows_estimate = MERGEBUFF2;
    ha_rows keys =
        memory_available / (param.max_record_length() + sizeof(char *));
    param.max_rows_per_buffer =
        min(num_rows_estimate > 0 ? num_rows_estimate : 1, keys);

    table->sort.set_max_size(memory_available, param.max_record_length());
  }

  param.sort_form = table;

  // New scope, because subquery execution must be traced within an array.
  {
    Opt_trace_array ota(trace, "filesort_execution");
    num_rows_found = read_all_rows(
        thd, &param, qep_tab, &table->sort, &chunk_file, &tempfile,
        param.using_pq ? &pq : nullptr, source_iterator, found_rows);
    if (num_rows_found == HA_POS_ERROR) goto err;
  }

  size_t num_chunks, num_initial_chunks;
  if (my_b_inited(&chunk_file)) {
    num_chunks =
        static_cast<size_t>(my_b_tell(&chunk_file)) / sizeof(Merge_chunk);
  } else {
    num_chunks = 0;
  }

  num_initial_chunks = num_chunks;

  if (num_chunks == 0)  // The whole set is in memory
  {
    ha_rows rows_in_chunk = param.using_pq ? pq.num_elements() : num_rows_found;
    if (save_index(&param, rows_in_chunk, &table->sort, sort_result)) goto err;
  } else {
    // We will need an extra buffer in SortFileIndirectIterator
    if (table->sort.addon_fields != nullptr &&
        !(table->sort.addon_fields->allocate_addon_buf(param.m_addon_length)))
      goto err; /* purecov: inspected */

    table->sort.read_chunk_descriptors(&chunk_file, num_chunks);
    if (table->sort.merge_chunks.is_null()) goto err; /* purecov: inspected */

    close_cached_file(&chunk_file);

    /* Open cached file if it isn't open */
    if (!my_b_inited(outfile) &&
        open_cached_file(outfile, mysql_tmpdir, TEMP_PREFIX, READ_RECORD_BUFFER,
                         MYF(MY_WME)))
      goto err;
    if (reinit_io_cache(outfile, WRITE_CACHE, 0L, 0, 0)) goto err;

    param.max_rows_per_buffer = static_cast<uint>(
        table->sort.max_size_in_bytes() / param.max_record_length());

    Bounds_checked_array<uchar> merge_buf = table->sort.get_contiguous_buffer();
    if (merge_buf.array() == nullptr) {
      my_error(ER_OUT_OF_SORTMEMORY, ME_FATALERROR);
      LogErr(ERROR_LEVEL, ER_SERVER_OUT_OF_SORTMEMORY);
      goto err;
    }
    if (merge_many_buff(thd, &param, merge_buf, table->sort.merge_chunks,
                        &num_chunks, &tempfile))
      goto err;
    if (flush_io_cache(&tempfile) ||
        reinit_io_cache(&tempfile, READ_CACHE, 0L, 0, 0))
      goto err;
    if (merge_index(
            thd, &param, merge_buf,
            Merge_chunk_array(table->sort.merge_chunks.begin(), num_chunks),
            &tempfile, outfile))
      goto err;
  }

  if (trace->is_started()) {
    char buffer[100];
    String sort_mode(buffer, sizeof(buffer), &my_charset_bin);
    sort_mode.length(0);
    sort_mode.append("<");
    if (param.using_varlen_keys())
      sort_mode.append("varlen_sort_key");
    else
      sort_mode.append("fixed_sort_key");
    sort_mode.append(", ");
    sort_mode.append(param.using_packed_addons()
                         ? "packed_additional_fields"
                         : param.using_addon_fields() ? "additional_fields"
                                                      : "rowid");
    sort_mode.append(">");

    const char *algo_text[] = {"none", "std::sort", "std::stable_sort"};

    Opt_trace_object filesort_summary(trace, "filesort_summary");
    filesort_summary.add("memory_available", memory_available)
        .add("key_size", param.max_compare_length())
        .add("row_size", param.max_record_length())
        .add("max_rows_per_buffer", param.max_rows_per_buffer)
        .add("num_rows_estimate", num_rows_estimate)
        .add("num_rows_found", num_rows_found)
        .add("num_initial_chunks_spilled_to_disk", num_initial_chunks)
        .add("peak_memory_used", table->sort.peak_memory_used())
        .add_alnum("sort_algorithm", algo_text[param.m_sort_algorithm]);
    if (!param.using_packed_addons())
      filesort_summary.add_alnum(
          "unpacked_addon_fields",
          addon_fields_text(param.m_addon_fields_status));
    filesort_summary.add_alnum("sort_mode", sort_mode.c_ptr());
  }

  if (num_rows_found > param.max_rows) {
    // If read_all_rows() produced more results than the query LIMIT.
    num_rows_found = param.max_rows;
  }
  error = 0;

err:
  if (!subselect || !subselect->is_uncacheable()) {
    if (!sort_result->sorted_result_in_fsbuf) table->sort.free_sort_buffer();
    my_free(table->sort.merge_chunks.array());
    table->sort.merge_chunks = Merge_chunk_array(NULL, 0);
  }
  close_cached_file(&tempfile);
  close_cached_file(&chunk_file);
  if (my_b_inited(outfile)) {
    if (flush_io_cache(outfile)) error = 1;
    {
      my_off_t save_pos = outfile->pos_in_file;
      /* For following reads */
      if (reinit_io_cache(outfile, READ_CACHE, 0L, 0, 0)) error = 1;
      outfile->end_of_file = save_pos;
    }
  }
  if (error) {
    DBUG_ASSERT(thd->is_error() || thd->killed);

    /*
      Guard against Bug#11745656 -- KILL QUERY should not send "server shutdown"
      to client!
    */
    const char *cause = thd->killed ? ((thd->killed == THD::KILL_CONNECTION &&
                                        !connection_events_loop_aborted())
                                           ? ER_THD(thd, THD::KILL_QUERY)
                                           : ER_THD(thd, thd->killed))
                                    : thd->get_stmt_da()->message_text();
    const char *msg = ER_THD(thd, ER_FILESORT_TERMINATED);

    my_printf_error(ER_FILSORT_ABORT, "%s: %s", MYF(0), msg, cause);

    if (thd->is_fatal_error()) {
      LogEvent()
          .type(LOG_TYPE_ERROR)
          .subsys(LOG_SUBSYSTEM_TAG)
          .prio(INFORMATION_LEVEL)
          .errcode(ER_FILESORT_TERMINATED)
          .user(thd->security_context()->priv_user())
          .host(thd->security_context()->host_or_ip())
          .thread_id(thd->thread_id())
          .message(
              "%s, host: %s, user: %s, thread: %u, error: %s, "
              "query: %-.4096s",
              msg, thd->security_context()->host_or_ip().str,
              thd->security_context()->priv_user().str, thd->thread_id(), cause,
              thd->query().str);
    }
  } else
    thd->inc_status_sort_rows(num_rows_found);
  *returned_rows = num_rows_found;

  DBUG_PRINT("exit", ("num_rows: %ld found_rows: %ld",
                      static_cast<long>(num_rows_found),
                      static_cast<long>(*found_rows)));
  DBUG_RETURN(error);
} /* filesort */

void filesort_free_buffers(TABLE *table, bool full) {
  DBUG_ENTER("filesort_free_buffers");

  table->unique_result.sorted_result.reset();
  DBUG_ASSERT(!table->unique_result.sorted_result_in_fsbuf);
  table->unique_result.sorted_result_in_fsbuf = false;

  if (full) {
    table->sort.free_sort_buffer();
    my_free(table->sort.merge_chunks.array());
    table->sort.merge_chunks = Merge_chunk_array(NULL, 0);
    table->sort.addon_fields = NULL;
  }

  DBUG_VOID_RETURN;
}

Filesort::Filesort(QEP_TAB *tab_arg, ORDER *order, ha_rows limit_arg,
                   bool force_stable_sort)
    : qep_tab(tab_arg),
      limit(limit_arg),
      sortorder(NULL),
      using_pq(false),
      m_force_stable_sort(
          force_stable_sort),  // keep relative order of equiv. elts
      addon_fields(NULL) {
  // Switch to the right slice if applicable, so that we fetch out the correct
  // items from order_arg.
  if (qep_tab->join() != nullptr) {
    DBUG_ASSERT(qep_tab->join()->m_ordered_index_usage !=
                (order == qep_tab->join()->order
                     ? JOIN::ORDERED_INDEX_ORDER_BY
                     : JOIN::ORDERED_INDEX_GROUP_BY));
    Switch_ref_item_slice slice_switch(qep_tab->join(),
                                       qep_tab->ref_item_slice);
    m_sort_order_length = make_sortorder(order);
  } else {
    m_sort_order_length = make_sortorder(order);
  }
}

uint Filesort::make_sortorder(ORDER *order) {
  uint count;
  st_sort_field *sort, *pos;
  ORDER *ord;
  DBUG_ENTER("Filesort::make_sortorder");

  count = 0;
  for (ord = order; ord; ord = ord->next) count++;
  DBUG_ASSERT(count > 0);

  const size_t sortorder_size = sizeof(*sortorder) * (count + 1);
  if (sortorder == nullptr)
    sortorder = static_cast<st_sort_field *>(sql_alloc(sortorder_size));
  if (sortorder == nullptr) DBUG_RETURN(0); /* purecov: inspected */
  memset(sortorder, 0, sortorder_size);

  pos = sort = sortorder;
  for (ord = order; ord; ord = ord->next, pos++) {
    Item *const item = ord->item[0], *const real_item = item->real_item();
    if (real_item->type() == Item::FIELD_ITEM) {
      /*
        Could be a field, or Item_view_ref/Item_ref wrapping a field
        If it is an Item_outer_ref, only_full_group_by has been switched off.
      */
      DBUG_ASSERT(
          item->type() == Item::FIELD_ITEM ||
          (item->type() == Item::REF_ITEM &&
           (down_cast<Item_ref *>(item)->ref_type() == Item_ref::VIEW_REF ||
            down_cast<Item_ref *>(item)->ref_type() == Item_ref::OUTER_REF ||
            down_cast<Item_ref *>(item)->ref_type() == Item_ref::REF)));
      pos->field = down_cast<Item_field *>(real_item)->field;
    } else if (real_item->type() == Item::SUM_FUNC_ITEM &&
               !real_item->const_item()) {
      // Aggregate, or Item_aggregate_ref
      DBUG_ASSERT(item->type() == Item::SUM_FUNC_ITEM ||
                  (item->type() == Item::REF_ITEM &&
                   static_cast<Item_ref *>(item)->ref_type() ==
                       Item_ref::AGGREGATE_REF));
      pos->field = item->get_tmp_table_field();
    } else if (real_item->type() == Item::COPY_STR_ITEM) {  // Blob patch
      pos->item = static_cast<Item_copy *>(real_item)->get_item();
    } else
      pos->item = item;
    pos->reverse = (ord->direction == ORDER_DESC);
    DBUG_ASSERT(pos->field != NULL || pos->item != NULL);
    DBUG_PRINT("info", ("sorting on %s: %s", (pos->field ? "field" : "item"),
                        (pos->field ? pos->field->field_name : "")));
  }
  DBUG_RETURN(count);
}

void Filesort_info::read_chunk_descriptors(IO_CACHE *chunk_file, uint count) {
  DBUG_ENTER("Filesort_info::read_chunk_descriptors");

  // If we already have a chunk array, we're doing sort in a subquery.
  if (!merge_chunks.is_null() && merge_chunks.size() < count) {
    my_free(merge_chunks.array());             /* purecov: inspected */
    merge_chunks = Merge_chunk_array(NULL, 0); /* purecov: inspected */
  }

  void *rawmem = merge_chunks.array();
  const size_t length = sizeof(Merge_chunk) * count;
  if (NULL == rawmem) {
    rawmem = my_malloc(key_memory_Filesort_info_merge, length, MYF(MY_WME));
    if (rawmem == NULL) DBUG_VOID_RETURN; /* purecov: inspected */
  }

  if (reinit_io_cache(chunk_file, READ_CACHE, 0L, 0, 0) ||
      my_b_read(chunk_file, static_cast<uchar *>(rawmem), length)) {
    my_free(rawmem); /* purecov: inspected */
    rawmem = NULL;   /* purecov: inspected */
    count = 0;       /* purecov: inspected */
  }

  merge_chunks = Merge_chunk_array(static_cast<Merge_chunk *>(rawmem), count);
  DBUG_VOID_RETURN;
}

#ifndef DBUG_OFF
/*
  Print a text, SQL-like record representation into dbug trace.

  Note: this function is a work in progress: at the moment
   - column read bitmap is ignored (can print garbage for unused columns)
   - there is no quoting
*/
static void dbug_print_record(TABLE *table, bool print_rowid) {
  char buff[1024];
  Field **pfield;
  String tmp(buff, sizeof(buff), &my_charset_bin);
  DBUG_LOCK_FILE;

  fprintf(DBUG_FILE, "record (");
  for (pfield = table->field; *pfield; pfield++)
    fprintf(DBUG_FILE, "%s%s", (*pfield)->field_name, (pfield[1]) ? ", " : "");
  fprintf(DBUG_FILE, ") = ");

  fprintf(DBUG_FILE, "(");
  for (pfield = table->field; *pfield; pfield++) {
    Field *field = *pfield;

    if (field->is_null()) {
      if (fwrite("NULL", sizeof(char), 4, DBUG_FILE) != 4) {
        goto unlock_file_and_quit;
      }
    }

    if (field->type() == MYSQL_TYPE_BIT)
      (void)field->val_int_as_str(&tmp, 1);
    else
      field->val_str(&tmp);

    if (fwrite(tmp.ptr(), sizeof(char), tmp.length(), DBUG_FILE) !=
        tmp.length()) {
      goto unlock_file_and_quit;
    }

    if (pfield[1]) {
      if (fwrite(", ", sizeof(char), 2, DBUG_FILE) != 2) {
        goto unlock_file_and_quit;
      }
    }
  }
  fprintf(DBUG_FILE, ")");
  if (print_rowid) {
    fprintf(DBUG_FILE, " rowid ");
    for (uint i = 0; i < table->file->ref_length; i++) {
      fprintf(DBUG_FILE, "%x", table->file->ref[i]);
    }
  }
  fprintf(DBUG_FILE, "\n");
unlock_file_and_quit:
  DBUG_UNLOCK_FILE;
}
#endif

/// Error handler for filesort.
class Filesort_error_handler : public Internal_error_handler {
  THD *m_thd;                 ///< The THD in which filesort is executed.
  bool m_seen_not_supported;  ///< Has a not supported warning has been seen?
 public:
  /**
    Create an error handler and push it onto the error handler
    stack. The handler will be automatically popped from the error
    handler stack when it is destroyed.
  */
  Filesort_error_handler(THD *thd) : m_thd(thd), m_seen_not_supported(false) {
    thd->push_internal_handler(this);
  }

  /**
    Pop the error handler from the error handler stack, and destroy
    it.
  */
  ~Filesort_error_handler() { m_thd->pop_internal_handler(); }

  /**
    Handle a condition.

    The handler will make sure that no more than a single
    ER_NOT_SUPPORTED_YET warning will be seen by the higher
    layers. This warning is generated by Json_wrapper::make_sort_key()
    for every value that it doesn't know how to create a sort key
    for. It is sufficient for the higher layers to report this warning
    only once per sort.
  */
  virtual bool handle_condition(THD *, uint sql_errno, const char *,
                                Sql_condition::enum_severity_level *level,
                                const char *) {
    if (*level == Sql_condition::SL_WARNING &&
        sql_errno == ER_NOT_SUPPORTED_YET) {
      if (m_seen_not_supported) return true;
      m_seen_not_supported = true;
    }

    return false;
  }
};

static bool alloc_and_make_sortkey(Sort_param *param, Filesort_info *fs_info,
                                   uchar *ref_pos) {
  size_t min_bytes = 1;
  for (;;) {  // Termination condition within loop.
    Bounds_checked_array<uchar> sort_key_buf =
        fs_info->get_next_record_pointer(min_bytes);
    if (sort_key_buf.array() == nullptr) return true;
    const uint rec_sz = param->make_sortkey(sort_key_buf, ref_pos);
    if (rec_sz > sort_key_buf.size()) {
      // The record wouldn't fit. Try again, asking for a larger buffer.
      min_bytes = sort_key_buf.size() + 1;
    } else {
      fs_info->commit_used_memory(rec_sz);
      return false;
    }
  }
}

static const Item::enum_walk walk_subquery =
    Item::enum_walk(Item::WALK_POSTFIX | Item::WALK_SUBQUERY);

/**
  Read all rows, and write them into a temporary file
  (if we run out of space in the sort buffer).
  All produced sequences are guaranteed to be non-empty.

  @param thd               Thread handle
  @param param             Sorting parameter
  @param qep_tab           Parameters for which data to read (see
                           source_iterator).
  @param fs_info           Struct containing sort buffer etc.
  @param chunk_file        File to write Merge_chunks describing sorted segments
                           in tempfile.
  @param tempfile          File to write sorted sequences of sortkeys to.
  @param pq                If !NULL, use it for keeping top N elements
  @param source_iterator   Where to read the rows to be sorted from.
  @param [out] found_rows  The number of FOUND_ROWS().
                           For a query with LIMIT, this value will typically
                           be larger than the function return value.

  @note
    Basic idea:
    @verbatim
     while (get_next_sortkey())
     {
       if (using priority queue)
         push sort key into queue
       else
       {
         try to put sort key into buffer;
         if (no free space in sort buffer)
         {
           do {
             allocate new, larger buffer;
             retry putting sort key into buffer;
           } until (record fits or no space for new buffer)
           if (no space for new buffer)
           {
             sort record pointers (all buffers);
             dump sorted sequence to 'tempfile';
             dump Merge_chunk describing sequence location into 'chunk_file';
           }
         }
         if (key was packed)
           tell sort buffer the actual number of bytes used;
       }
     }
     if (buffer has some elements && dumped at least once)
       sort-dump-dump as above;
     else
       don't sort, leave sort buffer to be sorted by caller.
  @endverbatim

  @returns
    Number of records written on success.
  @returns
    HA_POS_ERROR on error.
*/

static ha_rows read_all_rows(
    THD *thd, Sort_param *param, QEP_TAB *qep_tab, Filesort_info *fs_info,
    IO_CACHE *chunk_file, IO_CACHE *tempfile,
    Bounded_queue<uchar *, uchar *, Sort_param, Mem_compare_queue_key> *pq,
    RowIterator *source_iterator, ha_rows *found_rows) {
  /*
    Set up an error handler for filesort. It is automatically pushed
    onto the internal error handler stack upon creation, and will be
    popped off the stack automatically when the handler goes out of
    scope.
  */
  Filesort_error_handler error_handler(thd);

  DBUG_ENTER("read_all_rows");
  DBUG_PRINT("info", ("using: %s", (qep_tab->condition()
                                        ? qep_tab->quick() ? "ranges" : "where"
                                        : "every row")));

  int error = 0;
  TABLE *sort_form = param->sort_form;
  handler *file = sort_form->file;
  *found_rows = 0;
  uchar *ref_pos = &file->ref[0];

  DBUG_EXECUTE_IF("bug14365043_1", DBUG_SET("+d,ha_rnd_init_fail"););
  if (source_iterator->Init()) {
    DBUG_RETURN(HA_POS_ERROR);
  }

  // Now modify the read bitmaps, so that we are sure to get the rows
  // that we need for the sort (ie., the fields to sort on) as well as
  // the actual fields we want to return. We need to do this after Init()
  // has run, as Init() may want to set its own bitmaps and we don't want
  // it to overwrite ours. This is fairly ugly, though; we could end up
  // setting fields that the access method doesn't actually need (e.g.
  // if we set a condition that the access method can satisfy using an
  // index only), and in theory also clear fields it _would_ need, although
  // the latter should never happen in practice. A better solution would
  // involve communicating which extra fields we need down to the
  // RowIterator, instead of just overwriting the read set.

  /* Remember original bitmaps */
  MY_BITMAP *save_read_set = sort_form->read_set;
  MY_BITMAP *save_write_set = sort_form->write_set;
  /*
    Set up temporary column read map for columns used by sort and verify
    it's not used
  */
  DBUG_ASSERT(sort_form->tmp_set.n_bits == 0 ||
              bitmap_is_clear_all(&sort_form->tmp_set));

  // Temporary set for register_used_fields and mark_field_in_map()
  sort_form->read_set = &sort_form->tmp_set;
  // Include fields used for sorting in the read_set.
  register_used_fields(param);

  // Include fields used by conditions in the read_set.
  if (qep_tab->condition()) {
    Mark_field mf(sort_form, MARK_COLUMNS_TEMP);
    qep_tab->condition()->walk(&Item::mark_field_in_map, walk_subquery,
                               (uchar *)&mf);
  }
  // Include fields used by pushed conditions in the read_set.
  if (qep_tab->table()->file->pushed_idx_cond) {
    Mark_field mf(sort_form, MARK_COLUMNS_TEMP);
    qep_tab->table()->file->pushed_idx_cond->walk(&Item::mark_field_in_map,
                                                  walk_subquery, (uchar *)&mf);
  }
  sort_form->column_bitmaps_set(&sort_form->tmp_set, &sort_form->tmp_set);

  DEBUG_SYNC(thd, "after_index_merge_phase1");
  ha_rows num_total_records = 0, num_records_this_chunk = 0;
  uint num_written_chunks = 0;
  if (pq == nullptr) {
    fs_info->reset();
    fs_info->clear_peak_memory_used();
  }
  for (;;) {
    DBUG_EXECUTE_IF("bug19656296", DBUG_SET("+d,ha_rnd_next_deadlock"););
    if ((error = source_iterator->Read())) {
      break;
    }
    // Note where we are, for the case where we are not using addon fields.
    file->position(sort_form->record[0]);
    DBUG_EXECUTE_IF("debug_filesort", dbug_print_record(sort_form, true););

    if (thd->killed) {
      DBUG_PRINT("info", ("Sort killed by user"));
      num_total_records = HA_POS_ERROR;
      goto cleanup;
    }

    bool skip_record;
    if (!qep_tab->skip_record(thd, &skip_record) && !skip_record) {
      ++(*found_rows);
      num_total_records++;
      if (pq)
        pq->push(ref_pos);
      else {
        bool out_of_mem = alloc_and_make_sortkey(param, fs_info, ref_pos);
        if (out_of_mem) {
          // Out of room, so flush chunk to disk (if there's anything to flush).
          if (num_records_this_chunk > 0) {
            if (write_keys(param, fs_info, num_records_this_chunk, chunk_file,
                           tempfile)) {
              num_total_records = HA_POS_ERROR;
              goto cleanup;
            }
            num_records_this_chunk = 0;
            num_written_chunks++;
            fs_info->reset();

            // Now we should have room for a new row.
            out_of_mem = alloc_and_make_sortkey(param, fs_info, ref_pos);
          }

          // If we're still out of memory after flushing to disk, give up.
          if (out_of_mem) {
            my_error(ER_OUT_OF_SORTMEMORY, ME_FATALERROR);
            LogErr(ERROR_LEVEL, ER_SERVER_OUT_OF_SORTMEMORY);
            num_total_records = HA_POS_ERROR;
            goto cleanup;
          }
        }

        num_records_this_chunk++;
      }
    }
    /*
      Don't try unlocking the row if skip_record reported an error since in
      this case the transaction might have been rolled back already.
    */
    else if (!thd->is_error())
      file->unlock_row();
    /* It does not make sense to read more keys in case of a fatal error */
    if (thd->is_error()) break;
  }

  if (thd->is_error()) {
    num_total_records = HA_POS_ERROR;
    goto cleanup;
  }

  /* Signal we should use orignal column read and write maps */
  sort_form->column_bitmaps_set(save_read_set, save_write_set);

  DBUG_PRINT("test",
             ("error: %d  num_written_chunks: %d", error, num_written_chunks));
  if (error == 1) {
    num_total_records = HA_POS_ERROR;
    goto cleanup;
  }
  if (num_written_chunks != 0 && num_records_this_chunk != 0 &&
      write_keys(param, fs_info, num_records_this_chunk, chunk_file,
                 tempfile)) {
    num_total_records = HA_POS_ERROR;  // purecov: inspected
    goto cleanup;
  }

cleanup:
  // Clear tmp_set so it can be used elsewhere
  bitmap_clear_all(&sort_form->tmp_set);

  DBUG_PRINT("info", ("read_all_rows return %lu", (ulong)num_total_records));

  DBUG_RETURN(num_total_records);
} /* read_all_rows */

/**
  @details
  Sort the buffer and write:
  -# the sorted sequence to tempfile
  -# a Merge_chunk describing the sorted sequence position to chunk_file

  @param param          Sort parameters
  @param fs_info        Contains the buffer to be sorted and written.
  @param count          Number of records to write.
  @param chunk_file     One 'Merge_chunk' struct will be written into this file.
                        The Merge_chunk::{file_pos, count} will indicate where
                        the sorted data was stored.
  @param tempfile       The sorted sequence will be written into this file.

  @returns
    0 OK
  @returns
    1 Error
*/

static int write_keys(Sort_param *param, Filesort_info *fs_info, uint count,
                      IO_CACHE *chunk_file, IO_CACHE *tempfile) {
  Merge_chunk merge_chunk;
  DBUG_ENTER("write_keys");

  fs_info->sort_buffer(param, count);

  if (!my_b_inited(chunk_file) &&
      open_cached_file(chunk_file, mysql_tmpdir, TEMP_PREFIX, DISK_BUFFER_SIZE,
                       MYF(MY_WME)))
    DBUG_RETURN(1);

  if (!my_b_inited(tempfile) &&
      open_cached_file(tempfile, mysql_tmpdir, TEMP_PREFIX, DISK_BUFFER_SIZE,
                       MYF(MY_WME)))
    DBUG_RETURN(1); /* purecov: inspected */

  // Check that we wont have more chunks than we can possibly keep in memory.
  if (my_b_tell(chunk_file) + sizeof(Merge_chunk) > (ulonglong)UINT_MAX)
    DBUG_RETURN(1); /* purecov: inspected */

  merge_chunk.set_file_position(my_b_tell(tempfile));
  if (static_cast<ha_rows>(count) > param->max_rows) {
    // Write only SELECT LIMIT rows to the file
    count = static_cast<uint>(param->max_rows); /* purecov: inspected */
  }
  merge_chunk.set_rowcount(static_cast<ha_rows>(count));

  for (uint ix = 0; ix < count; ++ix) {
    uchar *record = fs_info->get_sorted_record(ix);
    size_t rec_length = param->get_record_length(record);

    if (my_b_write(tempfile, record, rec_length))
      DBUG_RETURN(1); /* purecov: inspected */
  }

  if (my_b_write(chunk_file, &merge_chunk, sizeof(merge_chunk)))
    DBUG_RETURN(1); /* purecov: inspected */

  DBUG_RETURN(0);
} /* write_keys */

#ifdef WORDS_BIGENDIAN
const bool Is_big_endian = true;
#else
const bool Is_big_endian = false;
#endif
static void copy_native_longlong(uchar *to, size_t to_length, longlong val,
                                 bool is_unsigned) {
  copy_integer<Is_big_endian>(to, to_length,
                              static_cast<uchar *>(static_cast<void *>(&val)),
                              sizeof(longlong), is_unsigned);
}

/**
  Make a sort key for the JSON value in an Item.

  This function is called by Sort_param::make_sortkey(). We don't want
  it to be inlined, since that seemed to have a negative impact on
  some performance tests.

  @param[in]     item    The item for which to create a sort key.

  @param[out]    to      Pointer into the buffer to which the sort key should
                         be written. It will point to where the data portion
                         of the key should start.

  @param[out]    null_indicator
                         For nullable items, the NULL indicator byte.
                         (Ignored otherwise.) Should be initialized by the
                         caller to a value that indicates not NULL.

  @param[in]     length  The length of the sort key, not including the NULL
                         indicator byte at the beginning of the sort key for
                         nullable items.

  @param[in,out] hash    The hash key of the JSON values in the current row.

  @returns
    length of the key stored
*/
NO_INLINE
static uint make_json_sort_key(Item *item, uchar *to, uchar *null_indicator,
                               size_t length, ulonglong *hash) {
  DBUG_ASSERT(!item->maybe_null || *null_indicator == 1);

  Json_wrapper wr;
  if (item->val_json(&wr)) {
    // An error occurred, no point to continue making key, set it to null.
    if (item->maybe_null) *null_indicator = 0;
    return 0;
  }

  if (item->null_value) {
    /*
      Got NULL. The sort key should be all zeros. The caller has
      already tentatively set the NULL indicator byte at *null_indicator to
      not-NULL, so we need to clear that byte too.
    */
    if (item->maybe_null) {
      // Don't store anything but null flag.
      *null_indicator = 0;
      return 0;
    }
    /* purecov: begin inspected */
    DBUG_PRINT("warning", ("Got null on something that shouldn't be null"));
    DBUG_ASSERT(false);
    return 0;
    /* purecov: end */
  }

  size_t actual_length = wr.make_sort_key(to, length);
  *hash = wr.make_hash_key(hash);
  return actual_length;
}

namespace {

/*
  Returns true if writing the given uint8_t would overflow <to> past <to_end>.
  Writes the value and advances <to> otherwise.
*/
inline bool write_uint8_overflows(uint8_t val, uchar *to_end, uchar **to) {
  if (to_end - *to < 1) return true;
  **to = val;
  (*to)++;
  return false;
}

/*
  Returns true if writing <num_bytes> zeros would overflow <to> past <to_end>.
  Writes the zeros and advances <to> otherwise.
*/
inline bool clear_overflows(size_t num_bytes, uchar *to_end, uchar **to) {
  if (static_cast<size_t>(to_end - *to) < num_bytes) return true;
  memset(*to, 0, num_bytes);
  *to += num_bytes;
  return false;
}

/*
  Returns true if advancing <to> by <num_bytes> would put it past <to_end>.
  Advances <to> otherwise (does not write anything to the buffer).
*/
inline bool advance_overflows(size_t num_bytes, uchar *to_end, uchar **to) {
  if (static_cast<size_t>(to_end - *to) < num_bytes) return true;
  *to += num_bytes;
  return false;
}

/*
  Writes a NULL indicator byte (if the field may be NULL), leaves space for a
  varlength prefix (if varlen and not NULL), and then the actual sort key.
  Returns the length of the key, sans NULL indicator byte and varlength prefix,
  or UINT_MAX if the value would not provably fit within the given bounds.
*/
size_t make_sortkey_from_field(Field *field, Nullable<size_t> dst_length,
                               uchar *to, uchar *to_end, bool *maybe_null) {
  bool is_varlen = !dst_length.has_value();

  *maybe_null = field->maybe_null();
  if (field->maybe_null()) {
    if (write_uint8_overflows(field->is_null() ? 0 : 1, to_end, &to))
      return UINT_MAX;
    if (field->is_null()) {
      if (is_varlen) {
        // Don't store anything except the NULL flag.
        return 0;
      }
      if (clear_overflows(dst_length.value(), to_end, &to)) return UINT_MAX;
      return dst_length.value();
    }
  }

  size_t actual_length;
  if (is_varlen) {
    if (advance_overflows(VARLEN_PREFIX, to_end, &to)) return UINT_MAX;
    size_t max_length = to_end - to;
    if (max_length % 2 != 0) {
      // Heed the contract that strnxfrm needs an even number of bytes.
      --max_length;
    }
    actual_length = field->make_sort_key(to, max_length);
    if (actual_length >= max_length) {
      /*
        The sort key either fit perfectly, or overflowed; we can't distinguish
        between the two, so we have to count it as overflow.
      */
      return UINT_MAX;
    }
  } else {
    if (static_cast<size_t>(to_end - to) < dst_length.value()) return UINT_MAX;
    actual_length = field->make_sort_key(to, dst_length.value());
    DBUG_ASSERT(actual_length == dst_length.value());
  }
  return actual_length;
}

/*
  Writes a NULL indicator byte (if the field may be NULL), leaves space for a
  varlength prefix (if varlen and not NULL), and then the actual sort key.
  Returns the length of the key, sans NULL indicator byte and varlength prefix,
  or UINT_MAX if the value would not provably fit within the given bounds.
*/
size_t make_sortkey_from_item(Item *item, Item_result result_type,
                              Nullable<size_t> dst_length, String *tmp_buffer,
                              uchar *to, uchar *to_end, bool *maybe_null,
                              ulonglong *hash) {
  bool is_varlen = !dst_length.has_value();

  uchar *null_indicator = nullptr;
  *maybe_null = item->maybe_null;
  if (item->maybe_null) {
    null_indicator = to;
    /*
      Assume not NULL by default. Will be overwritten if needed.
      Note that we can't check item->null_value at this time,
      because it will only get properly set after a call to val_*().
    */
    if (write_uint8_overflows(1, to_end, &to)) return UINT_MAX;
  }

  if (is_varlen) {
    // Check that there is room for the varlen prefix, and advance past it.
    if (advance_overflows(VARLEN_PREFIX, to_end, &to)) return UINT_MAX;
  } else {
    // Check that there is room for the fixed-size value.
    if (static_cast<size_t>(to_end - to) < dst_length.value()) return UINT_MAX;
  }

  switch (result_type) {
    case STRING_RESULT: {
      if (item->data_type() == MYSQL_TYPE_JSON) {
        DBUG_ASSERT(is_varlen);
        return make_json_sort_key(item, to, null_indicator, to_end - to, hash);
      }

      const CHARSET_INFO *cs = item->collation.collation;

      String *res = item->val_str(tmp_buffer);
      if (res == nullptr)  // Value is NULL.
      {
        DBUG_ASSERT(item->maybe_null);
        if (is_varlen) {
          // Don't store anything except the NULL flag.
          return 0;
        }
        *null_indicator = 0;
        memset(to, 0, dst_length.value());
        return dst_length.value();
      }

      uint src_length = static_cast<uint>(res->length());
      char *from = (char *)res->ptr();

      size_t actual_length;
      if (is_varlen) {
        size_t max_length = to_end - to;
        if (max_length % 2 != 0) {
          // Heed the contract that strnxfrm needs an even number of bytes.
          --max_length;
        }
        actual_length =
            cs->coll->strnxfrm(cs, to, max_length, item->max_char_length(),
                               (uchar *)from, src_length, 0);
        if (actual_length == max_length) {
          /*
            The sort key eithen fit perfectly, or overflowed; we can't
            distinguish between the two, so we have to count it as overflow.
          */
          return UINT_MAX;
        }
      } else {
        actual_length = cs->coll->strnxfrm(
            cs, to, dst_length.value(), item->max_char_length(), (uchar *)from,
            src_length, MY_STRXFRM_PAD_TO_MAXLEN);
        DBUG_ASSERT(actual_length == dst_length.value());
      }
      DBUG_ASSERT(to + actual_length <= to_end);
      return actual_length;
    }
    case INT_RESULT: {
      DBUG_ASSERT(!is_varlen);
      longlong value = item->data_type() == MYSQL_TYPE_TIME
                           ? item->val_time_temporal()
                           : item->is_temporal_with_date()
                                 ? item->val_date_temporal()
                                 : item->val_int();
      /*
        Note: item->null_value can't be trusted alone here; there are cases
        (for the DATE data type in particular) where we can have
        item->null_value set without maybe_null being set! This really should be
        cleaned up, but until that happens, we need to have a more conservative
        check.
      */
      if (item->maybe_null && item->null_value) {
        *null_indicator = 0;
        memset(to, 0, dst_length.value());
      } else
        copy_native_longlong(to, dst_length.value(), value,
                             item->unsigned_flag);
      return dst_length.value();
    }
    case DECIMAL_RESULT: {
      DBUG_ASSERT(!is_varlen);
      my_decimal dec_buf, *dec_val = item->val_decimal(&dec_buf);
      /*
        Note: item->null_value can't be trusted alone here; there are cases
        where we can have item->null_value set without maybe_null being set!
        (There are also cases where dec_val can return non-nullptr even in
        the case of a NULL result.) This really should be cleaned up, but until
        that happens, we need to have a more conservative check.
      */
      if (item->maybe_null && item->null_value) {
        *null_indicator = 0;
        memset(to, 0, dst_length.value());
      } else if (dst_length.value() < DECIMAL_MAX_FIELD_SIZE) {
        uchar buf[DECIMAL_MAX_FIELD_SIZE];
        my_decimal2binary(E_DEC_FATAL_ERROR, dec_val, buf,
                          item->max_length - (item->decimals ? 1 : 0),
                          item->decimals);
        memcpy(to, buf, dst_length.value());
      } else {
        my_decimal2binary(E_DEC_FATAL_ERROR, dec_val, to,
                          item->max_length - (item->decimals ? 1 : 0),
                          item->decimals);
      }
      return dst_length.value();
    }
    case REAL_RESULT: {
      DBUG_ASSERT(!is_varlen);
      double value = item->val_real();
      if (item->null_value) {
        DBUG_ASSERT(item->maybe_null);
        *null_indicator = 0;
        memset(to, 0, dst_length.value());
      } else if (dst_length.value() < sizeof(double)) {
        uchar buf[sizeof(double)];
        change_double_for_sort(value, buf);
        memcpy(to, buf, dst_length.value());
      } else {
        change_double_for_sort(value, to);
      }
      return dst_length.value();
    }
    case ROW_RESULT:
    default:
      // This case should never be choosen
      DBUG_ASSERT(0);
      return dst_length.value();
  }
}

}  // namespace

uint Sort_param::make_sortkey(Bounds_checked_array<uchar> dst,
                              const uchar *ref_pos) {
  uchar *to = dst.array();
  uchar *to_end = dst.array() + dst.size();
  uchar *orig_to = to;
  const st_sort_field *sort_field;
  ulonglong hash = 0;

  if (using_varlen_keys()) {
    to += size_of_varlength_field;
    if (to >= to_end) return UINT_MAX;
  }
  for (sort_field = local_sortorder.begin();
       sort_field != local_sortorder.end(); sort_field++) {
    if (to >= to_end ||
        (!sort_field->is_varlen &&
         static_cast<size_t>(to_end - to) < sort_field->length)) {
      return UINT_MAX;
    }

    bool maybe_null;
    Nullable<size_t> dst_length;
    if (!sort_field->is_varlen) dst_length = sort_field->length;
    uint actual_length;
    if (sort_field->field) {
      Field *field = sort_field->field;
      DBUG_ASSERT(sort_field->field_type == field->type());

      actual_length =
          make_sortkey_from_field(field, dst_length, to, to_end, &maybe_null);

      if (sort_field->field_type == MYSQL_TYPE_JSON) {
        DBUG_ASSERT(use_hash);
        unique_hash(field, &hash);
      }
    } else {  // Item
      Item *item = sort_field->item;
      DBUG_ASSERT(sort_field->field_type == item->data_type());

      actual_length =
          make_sortkey_from_item(item, sort_field->result_type, dst_length,
                                 &tmp_buffer, to, to_end, &maybe_null, &hash);
    }

    if (actual_length == UINT_MAX) {
      // Overflow.
      return UINT_MAX;
    }

    /*
      Now advance past the key that was just written, reversing the parts that
      we need to reverse.
    */

    bool is_null = maybe_null && *to == 0;
    if (maybe_null) {
      DBUG_ASSERT(*to == 0 || *to == 1);
      if (sort_field->reverse && is_null) {
        *to = 0xff;
      }
      ++to;
    }

    // Fill out the varlen prefix if it exists.
    if (sort_field->is_varlen && !is_null) {
      int4store(to, actual_length + VARLEN_PREFIX);
      to += VARLEN_PREFIX;
    }

    // Reverse the key if needed.
    if (sort_field->reverse) {
      while (actual_length--) {
        *to = (uchar)(~*to);
        to++;
      }
    } else {
      to += actual_length;
    }
  }

  if (use_hash) {
    if (to_end - to < 8) return UINT_MAX;
    int8store(to, hash);
    to += 8;
  }

  if (using_varlen_keys()) {
    // Store the length of the record as a whole.
    Sort_param::store_varlen_key_length(orig_to,
                                        static_cast<uint>(to - orig_to));
  }

  if (using_addon_fields()) {
    /*
      Save field values appended to sorted fields.
      First null bit indicators are appended then field values follow.
    */
    uchar *nulls = to;
    uchar *p_len = to;

    Addon_fields_array::const_iterator addonf = addon_fields->begin();
    if (clear_overflows(addonf->offset, to_end, &to)) return UINT_MAX;
    if (addon_fields->using_packed_addons()) {
      for (; addonf != addon_fields->end(); ++addonf) {
        Field *field = addonf->field;
        if (addonf->null_bit && field->is_null()) {
          nulls[addonf->null_offset] |= addonf->null_bit;
        } else {
          to = field->pack(to, field->ptr, to_end - to,
                           field->table->s->db_low_byte_first);
          if (to >= to_end) return UINT_MAX;
        }
      }
      Addon_fields::store_addon_length(p_len, to - p_len);
    } else {
      for (; addonf != addon_fields->end(); ++addonf) {
        Field *field = addonf->field;
        if (static_cast<size_t>(to_end - to) < addonf->max_length) {
          return UINT_MAX;
        }
        if (addonf->null_bit && field->is_null()) {
          nulls[addonf->null_offset] |= addonf->null_bit;
        } else {
          uchar *ptr MY_ATTRIBUTE((unused)) = field->pack(
              to, field->ptr, to_end - to, field->table->s->db_low_byte_first);
          DBUG_ASSERT(ptr <= to + addonf->max_length);
        }
        to += addonf->max_length;
      }
    }
    DBUG_PRINT("info", ("make_sortkey %p %u", orig_to,
                        static_cast<unsigned>(to - p_len)));
  } else {
    if (static_cast<size_t>(to_end - to) < ref_length) {
      return UINT_MAX;
    }

    /* Save filepos last */
    memcpy(to, ref_pos, ref_length);
    to += ref_length;
  }
  return to - orig_to;
}

/*
  Register fields used by sorting in the sorted table's read set
*/

static void register_used_fields(Sort_param *param) {
  Bounds_checked_array<st_sort_field>::const_iterator sort_field;
  TABLE *table = param->sort_form;
  MY_BITMAP *bitmap = table->read_set;
  Mark_field mf(table, MARK_COLUMNS_TEMP);

  for (sort_field = param->local_sortorder.begin();
       sort_field != param->local_sortorder.end(); sort_field++) {
    Field *field;
    if ((field = sort_field->field)) {
      if (field->table == table) {
        bitmap_set_bit(bitmap, field->field_index);
        if (field->is_virtual_gcol()) table->mark_gcol_in_maps(field);
      }
    } else {  // Item
      sort_field->item->walk(&Item::mark_field_in_map, walk_subquery,
                             (uchar *)&mf);
    }
  }

  if (param->using_addon_fields()) {
    Addon_fields_array::const_iterator addonf = param->addon_fields->begin();
    for (; addonf != param->addon_fields->end(); ++addonf) {
      Field *field = addonf->field;
      bitmap_set_bit(bitmap, field->field_index);
      if (field->is_virtual_gcol()) table->mark_gcol_in_maps(field);
    }
  } else {
    /* Save filepos last */
    table->prepare_for_position();
  }
}

/**
  This function is used only if the entire result set fits in memory.

  For addon fields, we keep the result in the filesort buffer.
  This saves us a lot of memcpy calls.

  For row references, we copy the final sorted result into a buffer,
  but we do not copy the actual sort-keys, as they are no longer needed.
  We could have kept the result in the sort buffere here as well,
  but the new buffer - containing only row references - is probably a
  lot smaller.

  The result data will be unpacked by SortBufferIterator
  or SortBufferIndirectIterator

  Note that SortBufferIterator does not have access to a Sort_param.
  It does however have access to a Filesort_info, which knows whether
  we have variable sized keys or not.
  TODO: consider templatizing SortBufferIterator on is_varlen or not.

  @param [in]     param      Sort parameters.
  @param          count      Number of records
  @param [in,out] table_sort Information used by SortBufferIterator /
                             SortBufferIndirectIterator
  @param [out]    sort_result Where to store the actual result
 */
static bool save_index(Sort_param *param, uint count, Filesort_info *table_sort,
                       Sort_result *sort_result) {
  uchar *to;
  DBUG_ENTER("save_index");

  table_sort->set_sort_length(param->max_compare_length(),
                              param->using_varlen_keys());

  table_sort->sort_buffer(param, count);

  if (param->using_addon_fields()) {
    sort_result->sorted_result_in_fsbuf = true;
    DBUG_RETURN(0);
  }

  sort_result->sorted_result_in_fsbuf = false;
  const size_t buf_size = param->fixed_res_length * count;

  DBUG_ASSERT(sort_result->sorted_result == NULL);
  sort_result->sorted_result.reset(static_cast<uchar *>(my_malloc(
      key_memory_Filesort_info_record_pointers, buf_size, MYF(MY_WME))));
  if (!(to = sort_result->sorted_result.get()))
    DBUG_RETURN(1); /* purecov: inspected */
  sort_result->sorted_result_end = sort_result->sorted_result.get() + buf_size;

  uint res_length = param->fixed_res_length;
  for (uint ix = 0; ix < count; ++ix) {
    uchar *record = table_sort->get_sorted_record(ix);
    uchar *start_of_payload = param->get_start_of_payload(record);
    memcpy(to, start_of_payload, res_length);
    to += res_length;
  }
  DBUG_RETURN(0);
}

/**
  Test whether priority queue is worth using to get top elements of an
  ordered result set. If it is, then allocates buffer for required amount of
  records

  @param trace            Current trace context.
  @param param            Sort parameters.
  @param filesort_info    Filesort information.
  @param table            Table to sort.
  @param num_rows         Estimate of number of rows in source record set.
  @param memory_available Memory available for sorting.
  @param keep_addon_fields Do not try to strip off addon fields.

  DESCRIPTION
    Given a query like this:
      SELECT ... FROM t ORDER BY a1,...,an LIMIT max_rows;
    This function tests whether a priority queue should be used to keep
    the result. Necessary conditions are:
    - estimate that it is actually cheaper than merge-sort
    - enough memory to store the @<max_rows@> records.

    If we don't have space for @<max_rows@> records, but we *do* have
    space for @<max_rows@> keys, we may rewrite 'table' to sort with
    references to records instead of additional data.
    (again, based on estimates that it will actually be cheaper).

   @returns
    true  - if it's ok to use PQ
    false - PQ will be slower than merge-sort, or there is not enough memory.
*/

bool check_if_pq_applicable(Opt_trace_context *trace, Sort_param *param,
                            Filesort_info *filesort_info, TABLE *table,
                            ha_rows num_rows, ulong memory_available,
                            bool keep_addon_fields) {
  DBUG_ENTER("check_if_pq_applicable");

  /*
    How much Priority Queue sort is slower than qsort.
    Measurements (see unit test) indicate that PQ is roughly 3 times slower.
  */
  const double PQ_slowness = 3.0;

  Opt_trace_object trace_filesort(trace,
                                  "filesort_priority_queue_optimization");
  if (param->max_rows == HA_POS_ERROR) {
    trace_filesort.add("usable", false)
        .add_alnum("cause", "not applicable (no LIMIT)");
    DBUG_RETURN(false);
  }

  trace_filesort.add("limit", param->max_rows);

  if (param->max_rows + 2 >= UINT_MAX) {
    trace_filesort.add("usable", false).add_alnum("cause", "limit too large");
    DBUG_RETURN(false);
  }
  if (param->max_record_length() >= 0xFFFFFFFFu) {
    trace_filesort.add("usable", false)
        .add_alnum("cause", "contains records of unbounded length");
    DBUG_RETURN(false);
  }

  ulong num_available_keys =
      memory_available / (param->max_record_length() + sizeof(char *));
  // We need 1 extra record in the buffer, when using PQ.
  param->max_rows_per_buffer = (uint)param->max_rows + 1;

  if (num_rows < num_available_keys) {
    // The whole source set fits into memory.
    if (param->max_rows < num_rows / PQ_slowness) {
      filesort_info->set_max_size(memory_available, param->max_record_length());
      trace_filesort.add("chosen", true);
      DBUG_RETURN(filesort_info->max_size_in_bytes() > 0);
    } else {
      // PQ will be slower.
      trace_filesort.add("chosen", false).add_alnum("cause", "sort_is_cheaper");
      DBUG_RETURN(false);
    }
  }

  // Do we have space for LIMIT rows in memory?
  if (param->max_rows_per_buffer < num_available_keys) {
    filesort_info->set_max_size(memory_available, param->max_record_length());
    trace_filesort.add("chosen", true);
    DBUG_RETURN(filesort_info->max_size_in_bytes() > 0);
  }

  // Try to strip off addon fields.
  if (!keep_addon_fields && param->using_addon_fields()) {
    const ulong row_length =
        param->max_compare_length() + param->ref_length + sizeof(char *);
    num_available_keys = memory_available / row_length;

    Opt_trace_object trace_addon(trace, "strip_additional_fields");
    trace_addon.add("row_size", row_length);

    // Can we fit all the keys in memory?
    if (param->max_rows_per_buffer >= num_available_keys) {
      trace_addon.add("chosen", false).add_alnum("cause", "not_enough_space");
    } else {
      const Cost_model_table *cost_model = table->cost_model();
      const double sort_merge_cost = get_merge_many_buffs_cost_fast(
          num_rows, num_available_keys, row_length, cost_model);
      trace_addon.add("sort_merge_cost", sort_merge_cost);
      /*
        PQ has cost:
        (insert + qsort) * log(queue size) * key_compare_cost() +
        cost of file lookup afterwards.
        The lookup cost is a bit pessimistic: we take table scan cost and
        assume that on average we find the row after scanning half of the file.
        A better estimate would be lookup cost, but note that we are doing
        random lookups here, rather than sequential scan.
      */
      const double pq_cpu_cost =
          (PQ_slowness * num_rows + param->max_rows_per_buffer) *
          cost_model->key_compare_cost(log((double)param->max_rows_per_buffer));
      const Cost_estimate scan_cost = table->file->table_scan_cost();
      const double pq_io_cost = param->max_rows * scan_cost.total_cost() / 2.0;
      const double pq_cost = pq_cpu_cost + pq_io_cost;
      trace_addon.add("priority_queue_cost", pq_cost);

      if (sort_merge_cost < pq_cost) {
        trace_addon.add("chosen", false);
        DBUG_RETURN(false);
      }

      trace_addon.add("chosen", true);
      filesort_info->set_max_size(
          memory_available, param->max_compare_length() + param->ref_length);
      if (filesort_info->max_size_in_bytes() > 0) {
        // Make attached data to be references instead of fields.
        filesort_info->addon_fields = NULL;
        param->addon_fields = NULL;

        param->fixed_res_length = param->ref_length;
        param->set_max_compare_length(param->max_compare_length() +
                                      param->ref_length);
        param->set_max_record_length(param->max_compare_length());

        DBUG_RETURN(true);
      }
    }
  }
  DBUG_RETURN(false);
}

/**
  Read from a disk file into the merge chunk's buffer. We generally read as
  many complete rows as we can, except when bounded by max_keys() or rowcount().
  Incomplete rows will be left in the file.

  @returns
    Number of bytes read, or (uint)-1 if something went wrong.
*/
static uint read_to_buffer(IO_CACHE *fromfile, Merge_chunk *merge_chunk,
                           Sort_param *param) {
  DBUG_ENTER("read_to_buffer");
  uint rec_length = param->max_record_length();
  ha_rows count;

  const bool packed_addon_fields = param->using_packed_addons();
  const bool using_varlen_keys = param->using_varlen_keys();

  if (merge_chunk->rowcount() > 0) {
    size_t bytes_to_read;
    if (packed_addon_fields || using_varlen_keys) {
      count = merge_chunk->rowcount();
      bytes_to_read = min(merge_chunk->buffer_size(),
                          static_cast<size_t>(fromfile->end_of_file -
                                              merge_chunk->file_position()));
    } else {
      count = min(merge_chunk->max_keys(), merge_chunk->rowcount());
      bytes_to_read = rec_length * static_cast<size_t>(count);
      if (count == 0) {
        // Not even room for the first row.
        my_error(ER_OUT_OF_SORTMEMORY, ME_FATALERROR);
        LogErr(ERROR_LEVEL, ER_SERVER_OUT_OF_SORTMEMORY);
        DBUG_RETURN((uint)-1);
      }
    }

    DBUG_PRINT("info",
               ("read_to_buffer %p at file_pos %llu bytes %llu", merge_chunk,
                static_cast<ulonglong>(merge_chunk->file_position()),
                static_cast<ulonglong>(bytes_to_read)));
    if (mysql_file_pread(fromfile->file, merge_chunk->buffer_start(),
                         bytes_to_read, merge_chunk->file_position(), MYF_RW))
      DBUG_RETURN((uint)-1); /* purecov: inspected */

    size_t num_bytes_read;
    if (packed_addon_fields || using_varlen_keys) {
      /*
        The last record read is most likely not complete here.
        We need to loop through all the records, reading the length fields,
        and then "chop off" the final incomplete record.
       */
      uchar *record = merge_chunk->buffer_start();
      uint ix = 0;
      for (; ix < count; ++ix) {
        if (using_varlen_keys &&
            (record + Sort_param::size_of_varlength_field) >=
                merge_chunk->buffer_end())
          break;  // Incomplete record.

        uchar *start_of_payload = param->get_start_of_payload(record);
        if (start_of_payload >= merge_chunk->buffer_end())
          break;  // Incomplete record.

        if (packed_addon_fields &&
            start_of_payload + Addon_fields::size_of_length_field >=
                merge_chunk->buffer_end())
          break;  // Incomplete record.

        const uint res_length =
            packed_addon_fields
                ? Addon_fields::read_addon_length(start_of_payload)
                : param->fixed_res_length;

        if (start_of_payload + res_length >= merge_chunk->buffer_end())
          break;  // Incomplete record.

        DBUG_ASSERT(res_length > 0);
        record = start_of_payload + res_length;
      }
      if (ix == 0) {
        // Not even room for the first row.
        my_error(ER_OUT_OF_SORTMEMORY, ME_FATALERROR);
        LogErr(ERROR_LEVEL, ER_SERVER_OUT_OF_SORTMEMORY);
        DBUG_RETURN((uint)-1);
      }
      count = ix;
      num_bytes_read = record - merge_chunk->buffer_start();
      DBUG_PRINT("info", ("read %llu bytes of complete records",
                          static_cast<ulonglong>(bytes_to_read)));
    } else
      num_bytes_read = bytes_to_read;

    merge_chunk->init_current_key();
    merge_chunk->advance_file_position(num_bytes_read);
    merge_chunk->decrement_rowcount(count);
    merge_chunk->set_mem_count(count);
    DBUG_RETURN(num_bytes_read);
  }

  DBUG_RETURN(0);
} /* read_to_buffer */

namespace {

/**
  This struct is used for merging chunks for filesort()
  For filesort() with fixed-size keys we use memcmp to compare rows.
  For variable length keys, we use cmp_varlen_keys to compare rows.
 */
struct Merge_chunk_greater {
  size_t m_len;
  Sort_param *m_param;

  // CTOR for filesort() with fixed-size keys
  explicit Merge_chunk_greater(size_t len) : m_len(len), m_param(nullptr) {}

  // CTOR for filesort() with varlen keys
  explicit Merge_chunk_greater(Sort_param *param) : m_len(0), m_param(param) {}

  bool operator()(Merge_chunk *a, Merge_chunk *b) {
    uchar *key1 = a->current_key();
    uchar *key2 = b->current_key();
    // Fixed len keys
    if (m_len) return memcmp(key1, key2, m_len) > 0;

    if (m_param)
      return !cmp_varlen_keys(m_param->local_sortorder, m_param->use_hash, key1,
                              key2);

    // We can actually have zero-length sort key for filesort().
    return false;
  }
};

}  // namespace

/**
  Merge buffers to one buffer.

  @param thd
  @param param          Sort parameter
  @param from_file      File with source data (Merge_chunks point to this file)
  @param to_file        File to write the sorted result data.
  @param sort_buffer    Buffer for data to store up to MERGEBUFF2 sort keys.
  @param [out] last_chunk Store here Merge_chunk describing data written to
                        to_file.
  @param chunk_array    Array of chunks to merge.
  @param flag           0 - write full record, 1 - write addon/ref

  @returns
    0      OK
  @returns
    other  error
*/
static int merge_buffers(THD *thd, Sort_param *param, IO_CACHE *from_file,
                         IO_CACHE *to_file, Sort_buffer sort_buffer,
                         Merge_chunk *last_chunk, Merge_chunk_array chunk_array,
                         int flag) {
  int error = 0;
  uint rec_length, res_length;
  size_t sort_length;
  ha_rows max_rows, org_max_rows;
  my_off_t to_start_filepos;
  uchar *strpos;
  Merge_chunk *merge_chunk;
  std::atomic<THD::killed_state> *killed = &thd->killed;
  std::atomic<THD::killed_state> not_killable{THD::NOT_KILLED};
  DBUG_ENTER("merge_buffers");

  thd->inc_status_sort_merge_passes();
  if (param->not_killable) {
    killed = &not_killable;
    not_killable = THD::NOT_KILLED;
  }

  rec_length = param->max_record_length();
  res_length = param->fixed_res_length;
  sort_length = param->max_compare_length();
  uint offset = (flag == 0) ? 0 : (rec_length - res_length);
  to_start_filepos = my_b_tell(to_file);
  strpos = sort_buffer.array();
  org_max_rows = max_rows = param->max_rows;

  // Only relevant for fixed-length rows.
  ha_rows maxcount = param->max_rows_per_buffer / chunk_array.size();

  Merge_chunk_greater mcl = param->using_varlen_keys()
                                ? Merge_chunk_greater(param)
                                : Merge_chunk_greater(sort_length);
  Priority_queue<Merge_chunk *,
                 std::vector<Merge_chunk *, Malloc_allocator<Merge_chunk *>>,
                 Merge_chunk_greater>
  queue(mcl, Malloc_allocator<Merge_chunk *>(key_memory_Filesort_info_merge));

  if (queue.reserve(chunk_array.size())) DBUG_RETURN(1);

  for (merge_chunk = chunk_array.begin(); merge_chunk != chunk_array.end();
       merge_chunk++) {
    const size_t chunk_sz = sort_buffer.size() / chunk_array.size();
    merge_chunk->set_buffer(strpos, strpos + chunk_sz);

    merge_chunk->set_max_keys(maxcount);
    strpos += chunk_sz;
    error = static_cast<int>(read_to_buffer(from_file, merge_chunk, param));

    if (error == -1) DBUG_RETURN(error); /* purecov: inspected */
    // If less data in buffers than expected
    merge_chunk->set_max_keys(merge_chunk->mem_count());
    (void)queue.push(merge_chunk);
  }

  while (queue.size() > 1) {
    if (*killed) {
      DBUG_RETURN(1); /* purecov: inspected */
    }
    for (;;) {
      merge_chunk = queue.top();
      {
        param->get_rec_and_res_len(merge_chunk->current_key(), &rec_length,
                                   &res_length);
        const uint bytes_to_write = (flag == 0) ? rec_length : res_length;

        if (flag && param->using_varlen_keys())
          offset = rec_length - res_length;

        DBUG_PRINT("info", ("write record at %llu len %u", my_b_tell(to_file),
                            bytes_to_write));
        if (my_b_write(to_file, merge_chunk->current_key() + offset,
                       bytes_to_write)) {
          DBUG_RETURN(1); /* purecov: inspected */
        }
        if (!--max_rows) {
          error = 0; /* purecov: inspected */
          goto end;  /* purecov: inspected */
        }
      }

      merge_chunk->advance_current_key(rec_length);
      merge_chunk->decrement_mem_count();
      if (0 == merge_chunk->mem_count()) {
        if (!(error = (int)read_to_buffer(from_file, merge_chunk, param))) {
          queue.pop();
          reuse_freed_buff(merge_chunk, &queue);
          break; /* One buffer have been removed */
        } else if (error == -1)
          DBUG_RETURN(error); /* purecov: inspected */
      }
      /*
        The Merge_chunk at the queue's top had one of its keys consumed, thus
        it may now rank differently in the comparison order of the queue, so:
      */
      queue.update_top();
    }
  }
  merge_chunk = queue.top();
  merge_chunk->set_buffer(sort_buffer.array(),
                          sort_buffer.array() + sort_buffer.size());
  merge_chunk->set_max_keys(param->max_rows_per_buffer);

  do {
    if (merge_chunk->mem_count() > max_rows) {
      merge_chunk->set_mem_count(max_rows); /* Don't write too many records */
      merge_chunk->set_rowcount(0);         /* Don't read more */
    }
    max_rows -= merge_chunk->mem_count();

    for (uint ix = 0; ix < merge_chunk->mem_count(); ++ix) {
      param->get_rec_and_res_len(merge_chunk->current_key(), &rec_length,
                                 &res_length);
      const uint bytes_to_write = (flag == 0) ? rec_length : res_length;

      if (flag && param->using_varlen_keys()) offset = rec_length - res_length;

      if (my_b_write(to_file, merge_chunk->current_key() + offset,
                     bytes_to_write)) {
        DBUG_RETURN(1); /* purecov: inspected */
      }
      merge_chunk->advance_current_key(rec_length);
    }
  } while ((error = (int)read_to_buffer(from_file, merge_chunk, param)) != -1 &&
           error != 0);

end:
  last_chunk->set_rowcount(min(org_max_rows - max_rows, param->max_rows));
  last_chunk->set_file_position(to_start_filepos);

  DBUG_RETURN(error);
} /* merge_buffers */

/* Do a merge to output-file (save only positions) */

static int merge_index(THD *thd, Sort_param *param, Sort_buffer sort_buffer,
                       Merge_chunk_array chunk_array, IO_CACHE *tempfile,
                       IO_CACHE *outfile) {
  DBUG_ENTER("merge_index");
  if (merge_buffers(thd,
                    param,                // param
                    tempfile,             // from_file
                    outfile,              // to_file
                    sort_buffer,          // sort_buffer
                    chunk_array.begin(),  // last_chunk [out]
                    chunk_array,
                    1))  // flag
    DBUG_RETURN(1);      /* purecov: inspected */
  DBUG_RETURN(0);
} /* merge_index */

/**
  Calculate length of sort key.

  @param thd			  Thread handler
  @param sortorder		  Order of items to sort
  @param s_length	          Number of items to sort

  @note
    sortorder->length is updated for each sort item.

  @return
    Total length of sort buffer in bytes
*/

uint sortlength(THD *thd, st_sort_field *sortorder, uint s_length) {
  uint total_length = 0;

  // Heed the contract that strnxfrm() needs an even number of bytes.
  const uint max_sort_length_even = (thd->variables.max_sort_length + 1) & ~1;

  for (; s_length--; sortorder++) {
    bool is_string_type = false;
    if (sortorder->field) {
      const Field *field = sortorder->field;
      const CHARSET_INFO *cs = field->sort_charset();
      sortorder->length = field->sort_length();
      sortorder->is_varlen = field->sort_key_is_varlen();

      // How many bytes do we need (including sort weights) for strnxfrm()?
      if (sortorder->length < (10 << 20)) {  // 10 MB.
        sortorder->length = cs->coll->strnxfrmlen(cs, sortorder->length);
      } else {
        /*
          If over 10 MB, just set the length as effectively infinite, so we
          don't get overflows in strnxfrmlen().
         */
        sortorder->length = 0xFFFFFFFFu;
      }

      sortorder->maybe_null = field->maybe_null();
      sortorder->field_type = field->type();
      is_string_type =
          field->result_type() == STRING_RESULT && !field->is_temporal();
    } else {
      const Item *item = sortorder->item;
      sortorder->result_type = item->result_type();
      sortorder->field_type = item->data_type();
      if (sortorder->field_type == MYSQL_TYPE_JSON)
        sortorder->is_varlen = true;
      else
        sortorder->is_varlen = false;
      if (item->is_temporal()) sortorder->result_type = INT_RESULT;
      switch (sortorder->result_type) {
        case STRING_RESULT: {
          const CHARSET_INFO *cs = item->collation.collation;
          sortorder->length = item->max_length;

          if (cs->pad_attribute == NO_PAD) {
            sortorder->is_varlen = true;
          }

          if (sortorder->length < (10 << 20)) {  // 10 MB.
            // How many bytes do we need (including sort weights) for
            // strnxfrm()?
            sortorder->length = cs->coll->strnxfrmlen(cs, sortorder->length);
          } else {
            /*
              If over 10 MB, just set the length as effectively infinite, so we
              don't get overflows in strnxfrmlen().
             */
            sortorder->length = 0xFFFFFFFFu;
          }
          is_string_type = true;
          break;
        }
        case INT_RESULT:
#if SIZEOF_LONG_LONG > 4
          sortorder->length = 8;  // Size of intern longlong
#else
          sortorder->length = 4;
#endif
          break;
        case DECIMAL_RESULT:
          sortorder->length = my_decimal_get_binary_size(
              item->max_length - (item->decimals ? 1 : 0), item->decimals);
          break;
        case REAL_RESULT:
          sortorder->length = sizeof(double);
          break;
        case ROW_RESULT:
        default:
          // This case should never be choosen
          DBUG_ASSERT(0);
          break;
      }
      sortorder->maybe_null = item->maybe_null;
    }
    if (!sortorder->is_varlen && is_string_type) {
      /*
        We would love to never have to care about max_sort_length anymore,
        but that would make it impossible for us to sort blobs (TEXT) with
        PAD SPACE collations, since those are not variable-length (the padding
        is serialized as part of the sort key) and thus require infinite space.
        Thus, as long as we need to sort such fields by storing their sort
        keys, we need to heed max_sort_length for such fields.
      */
      sortorder->length = std::min(sortorder->length, max_sort_length_even);
    }

    if (sortorder->maybe_null)
      AddWithSaturate(1u, &total_length);  // Place for NULL marker
    if (sortorder->is_varlen)
      AddWithSaturate(VARLEN_PREFIX, &sortorder->length);
    AddWithSaturate(sortorder->length, &total_length);
  }
  sortorder->field = NULL;  // end marker
  DBUG_PRINT("info", ("sort_length: %u", total_length));
  return total_length;
}

/**
  Get descriptors of fields appended to sorted fields and
  calculate their total length.

  The function first finds out what fields are used in the result set.
  Then it calculates the length of the buffer to store the values of
  these fields together with the value of sort values.
  If the calculated length is not greater than max_length_for_sort_data
  the function allocates memory for an array of descriptors containing
  layouts for the values of the non-sorted fields in the buffer and
  fills them.

  @param max_length_for_sort_data Value of session variable.
  @param ptabfield             Array of references to the table fields
  @param sortlength            Total length of sorted fields
  @param[out] addon_fields_status Reason for *not* using packed addon fields
  @param[out] plength          Total length of appended fields
  @param[out] ppackable_length Total length of appended fields having a
                               packable type

  @note
    The null bits for the appended values are supposed to be put together
    and stored into the buffer just ahead of the value of the first field.

  @return
    Pointer to the layout descriptors for the appended fields, if any
  @returns
    NULL   if we do not store field values with sort data.
*/

Addon_fields *Filesort::get_addon_fields(
    ulong max_length_for_sort_data, Field **ptabfield, uint sortlength,
    Addon_fields_status *addon_fields_status, uint *plength,
    uint *ppackable_length) {
  Field **pfield;
  Field *field;
  uint total_length = 0;
  uint packable_length = 0;
  uint num_fields = 0;
  uint null_fields = 0;
  TABLE *const table = qep_tab->table();
  MY_BITMAP *read_set = table->read_set;

  // Locate the effective index for the table to be sorted (if any)
  const uint index = qep_tab->effective_index();
  /*
    filter_covering is true if access is via an index that is covering,
    regardless of whether the access is by the covering index or by
    index and base table, since the query has to be fulfilled with fields
    from that index only.
    This information is later used to filter out base columns for virtual
    generated columns, since these are only needed when reading the table.
    During sorting, trust that values for all generated columns have been
    materialized, which means that base columns are no longer necessary.
  */
  const bool filter_covering = index != MAX_KEY &&
                               table->covering_keys.is_set(index) &&
                               table->index_contains_some_virtual_gcol(index);

  /*
    If there is a reference to a field in the query add it
    to the the set of appended fields.
    Note for future refinement:
    This this a too strong condition.
    Actually we need only the fields referred in the
    result set. And for some of them it makes sense to use
    the values directly from sorted fields.
  */
  *plength = *ppackable_length = 0;
  *addon_fields_status = Addon_fields_status::unknown_status;

  for (pfield = ptabfield; (field = *pfield); pfield++) {
    if (!bitmap_is_set(read_set, field->field_index)) continue;
    // part_of_key is empty for a BLOB, so apply this check before the next.
    if (field->flags & BLOB_FLAG) {
      DBUG_ASSERT(addon_fields == NULL);
      *addon_fields_status = Addon_fields_status::row_contains_blob;
      return NULL;
    }
    if (filter_covering && !field->part_of_key.is_set(index))
      continue;  // See explanation above filter_covering

    const uint field_length = field->max_packed_col_length();
    total_length += field_length;

    const enum_field_types field_type = field->type();
    if (field->maybe_null() || field_type == MYSQL_TYPE_STRING ||
        field_type == MYSQL_TYPE_VARCHAR || field_type == MYSQL_TYPE_VAR_STRING)
      packable_length += field_length;
    if (field->maybe_null()) null_fields++;
    num_fields++;
  }
  if (0 == num_fields) return NULL;

  total_length += (null_fields + 7) / 8;

  *ppackable_length = packable_length;

  if (total_length + sortlength > max_length_for_sort_data) {
    DBUG_ASSERT(addon_fields == NULL);
    *addon_fields_status = Addon_fields_status::max_length_for_sort_data;
    return NULL;
  }

  if (addon_fields == NULL) {
    void *rawmem1 = sql_alloc(sizeof(Addon_fields));
    void *rawmem2 = sql_alloc(sizeof(Sort_addon_field) * num_fields);
    if (rawmem1 == NULL || rawmem2 == NULL)
      return NULL; /* purecov: inspected */
    Addon_fields_array addon_array(static_cast<Sort_addon_field *>(rawmem2),
                                   num_fields);
    addon_fields = new (rawmem1) Addon_fields(addon_array);
  } else {
    /*
      Allocate memory only once, reuse descriptor array and buffer.
      Set using_packed_addons here, and size/offset details below.
     */
    DBUG_ASSERT(num_fields == addon_fields->num_field_descriptors());
    addon_fields->set_using_packed_addons(false);
  }

  *plength = total_length;

  uint length = (null_fields + 7) / 8;
  null_fields = 0;
  Addon_fields_array::iterator addonf = addon_fields->begin();
  for (pfield = ptabfield; (field = *pfield); pfield++) {
    if (!bitmap_is_set(read_set, field->field_index)) continue;
    if (filter_covering && !field->part_of_key.is_set(index)) continue;
    DBUG_ASSERT(addonf != addon_fields->end());

    addonf->field = field;
    addonf->offset = length;
    if (field->maybe_null()) {
      addonf->null_offset = null_fields / 8;
      addonf->null_bit = 1 << (null_fields & 7);
      null_fields++;
    } else {
      addonf->null_offset = 0;
      addonf->null_bit = 0;
    }
    addonf->max_length = field->max_packed_col_length();
    DBUG_PRINT("info", ("addon_field %s max_length %u",
                        addonf->field->field_name, addonf->max_length));

    length += addonf->max_length;
    addonf++;
  }

  DBUG_PRINT("info", ("addon_length: %d", length));
  return addon_fields;
}

/*
** functions to change a double or float to a sortable string
** The following should work for IEEE
*/

void change_double_for_sort(double nr, uchar *to) {
  /*
    -0.0 and +0.0 compare identically, so make sure they use exactly the same
    bit pattern.
  */
  if (nr == 0.0) nr = 0.0;

  /*
    Positive doubles sort exactly as ints; negative doubles need
    bit flipping. The bit flipping sets the upper bit to 0
    unconditionally, so put 1 in there for positive numbers
    (so they sort later for our unsigned comparison).
    NOTE: This does not sort infinities or NaN correctly.
  */
  int64 nr_int;
  memcpy(&nr_int, &nr, sizeof(nr));
  nr_int = (nr_int ^ (nr_int >> 63)) | ((~nr_int) & 0x8000000000000000ULL);

  // TODO: Make store64be() or similar.
  memcpy(to, &nr_int, sizeof(nr_int));
#if !defined(WORDS_BIGENDIAN)
  using std::swap;
  swap(to[0], to[7]);
  swap(to[1], to[6]);
  swap(to[2], to[5]);
  swap(to[3], to[4]);
#endif
}
