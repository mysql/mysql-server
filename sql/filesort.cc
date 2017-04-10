/*
   Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */


/**
  @file sql/filesort.cc
  Sorts a database.
*/

#include "filesort.h"

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <new>
#include <vector>

#include "binary_log_types.h"
#include "binlog_config.h"
#include "bounded_queue.h"
#include "cmp_varlen_keys.h"
#include "debug_sync.h"
#include "decimal.h"
#include "derror.h"
#include "error_handler.h"
#include "field.h"
#include "filesort_utils.h"
#include "handler.h"
#include "item.h"
#include "item_subselect.h"
#include "json_dom.h"                   // Json_wrapper
#include "lex_string.h"
#include "log.h"
#include "m_ctype.h"
#include "malloc_allocator.h"
#include "merge_many_buff.h"
#include "my_bitmap.h"
#include "my_byteorder.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_decimal.h"
#include "my_macros.h"
#include "my_pointer_arithmetic.h"
#include "my_sys.h"
#include "myisampack.h"
#include "mysql/psi/mysql_file.h"
#include "mysql/service_mysql_alloc.h"
#include "mysql_com.h"
#include "mysqld.h"                             // mysql_tmpdir
#include "mysqld_error.h"
#include "opt_costmodel.h"
#include "opt_range.h"                          // QUICK
#include "opt_trace.h"
#include "opt_trace_context.h"
#include "priority_queue.h"
#include "psi_memory_key.h"
#include "session_tracker.h"
#include "sort_param.h"
#include "sql_array.h"
#include "sql_base.h"
#include "sql_bitmap.h"
#include "sql_class.h"
#include "sql_const.h"
#include "sql_error.h"
#include "sql_executor.h"               // QEP_TAB
#include "sql_lex.h"
#include "sql_optimizer.h"              // JOIN
#include "sql_plugin.h"
#include "sql_security_ctx.h"
#include "sql_sort.h"
#include "sql_string.h"
#include "system_variables.h"
#include "table.h"
#include "template_utils.h"
#include "thr_malloc.h"

using std::max;
using std::min;

namespace {

struct Mem_compare_queue_key
{
  Mem_compare_queue_key() : m_compare_length(0), m_param(nullptr) {}

  Mem_compare_queue_key(const Mem_compare_queue_key &that)
    : m_compare_length(that.m_compare_length), m_param(that.m_param)
  {
  }

  bool operator()(const uchar *s1, const uchar *s2) const
  {
    if (m_param)
      return
        cmp_varlen_keys(m_param->local_sortorder, s1, s2);

    // memcmp(s1, s2, 0) is guaranteed to return zero.
    return memcmp(s1, s2, m_compare_length) < 0;
  }

  size_t m_compare_length;
  Sort_param *m_param;
};
}


	/* functions defined in this file */

static ha_rows find_all_keys(THD *thd, Sort_param *param, QEP_TAB *qep_tab,
                             Filesort_info *fs_info,
                             IO_CACHE *buffer_file,
                             IO_CACHE *chunk_file,
                             Bounded_queue<uchar *, uchar *, Sort_param,
                                           Mem_compare_queue_key> *pq,
                             ha_rows *found_rows);
static int write_keys(Sort_param *param, Filesort_info *fs_info,
                      uint count, IO_CACHE *buffer_file, IO_CACHE *tempfile);
static void register_used_fields(Sort_param *param);
static int merge_index(THD *thd,
                       Sort_param *param,
                       Sort_buffer sort_buffer,
                       Merge_chunk_array chunk_array,
                       IO_CACHE *tempfile,
                       IO_CACHE *outfile);
static bool save_index(Sort_param *param, uint count,
                       Filesort_info *table_sort);
static uint suffix_length(ulong string_length);

static bool check_if_pq_applicable(Opt_trace_context *trace,
                                   Sort_param *param, Filesort_info *info,
                                   TABLE *table,
                                   ha_rows records, ulong memory_available,
                                   bool keep_addon_fields);


void Sort_param::init_for_filesort(Filesort *file_sort,
                                   Bounds_checked_array<st_sort_field> sf_array,
                                   uint sortlen, TABLE *table,
                                   ulong max_length_for_sort_data,
                                   ha_rows maxrows, bool sort_positions)
{
  DBUG_ASSERT(max_rows == 0);   // function should not be called twice
  m_fixed_sort_length= sortlen;
  ref_length= table->file->ref_length;

  local_sortorder= sf_array;

  if (table->file->ha_table_flags() & HA_FAST_KEY_READ)
    m_addon_fields_status= Addon_fields_status::using_heap_table;
  else if (table->fulltext_searched)
    m_addon_fields_status= Addon_fields_status::fulltext_searched;
  else if (sort_positions)
    m_addon_fields_status= Addon_fields_status::keep_rowid;
  else
  {
    /* 
      Get the descriptors of all fields whose values are appended 
      to sorted fields and get its total length in m_addon_length.
    */
    addon_fields=
      file_sort->get_addon_fields(max_length_for_sort_data,
                                  table->field, m_fixed_sort_length,
                                  &m_addon_fields_status,
                                  &m_addon_length,
                                  &m_packable_length);
  }
  if (using_addon_fields())
  {
    fixed_res_length= m_addon_length;
  }
  else
  {
    fixed_res_length= ref_length;
    /* 
      The reference to the record is considered 
      as an additional sorted field
    */
    m_fixed_sort_length+= ref_length;
  }

  m_num_varlen_keys= count_varlen_keys();
  if (using_varlen_keys())
  {
    use_hash= true;
    m_fixed_sort_length+= size_of_varlength_field;
  }
  /*
    Add hash at the end of sort key to order cut values correctly.
    Needed for GROUPing, rather than for ORDERing.
  */
  if (use_hash)
    m_fixed_sort_length+= sizeof(ulonglong);

  m_fixed_rec_length= m_fixed_sort_length + m_addon_length;
  max_rows= maxrows;
}


void Sort_param::try_to_pack_addons(ulong max_length_for_sort_data)
{
  if (!using_addon_fields() ||                  // no addons, or
      using_packed_addons())                    // already packed
    return;

  if (!Addon_fields::can_pack_addon_fields(fixed_res_length))
  {
    m_addon_fields_status= Addon_fields_status::row_too_large;
    return;
  }
  const uint sz= Addon_fields::size_of_length_field;
  if (m_fixed_rec_length + sz > max_length_for_sort_data)
  {
    m_addon_fields_status= Addon_fields_status::row_too_large;
    return;
  }

  // Heuristic: skip packing if potential savings are less than 10 bytes.
  if (m_packable_length < (10 + sz))
  {
    m_addon_fields_status= Addon_fields_status::skip_heuristic;
    return;
  }

  Addon_fields_array::iterator addonf= addon_fields->begin();
  for ( ; addonf != addon_fields->end(); ++addonf)
  {
    addonf->offset+= sz;
    addonf->null_offset+= sz;
  }
  addon_fields->set_using_packed_addons(true);
  m_using_packed_addons= true;

  m_addon_length+= sz;
  fixed_res_length+= sz;
  m_fixed_rec_length+= sz;
}


int Sort_param::count_varlen_keys() const
{
  int retval= 0;
  for (const auto &sf : local_sortorder)
  {
    if (sf.is_varlen)
    {
      ++retval;
    }
  }
  return retval;
}


size_t Sort_param::get_record_length(uchar *p) const
{
  uchar *start_of_payload= get_start_of_payload(p);
  uint size_of_payload= using_packed_addons() ?
    Addon_fields::read_addon_length(start_of_payload) : fixed_res_length;
  uchar *end_of_payload= start_of_payload + size_of_payload;
  return end_of_payload - p;
}

void Sort_param::get_rec_and_res_len(uchar *record_start, uint *recl, uint *resl)
{
  if (!using_packed_addons() && !using_varlen_keys())
  {
    *recl= m_fixed_rec_length;
    *resl= fixed_res_length;
    return;
  }
  uchar *plen= get_start_of_payload(record_start);
  if (using_packed_addons())
    *resl= Addon_fields::read_addon_length(plen);
  else
    *resl= fixed_res_length;
  DBUG_ASSERT(*resl <= fixed_res_length);
  const uchar *record_end= plen + *resl;
  *recl= static_cast<uint>(record_end - record_start);
}


static void trace_filesort_information(Opt_trace_context *trace,
                                       const st_sort_field *sortorder,
                                       uint s_length)
{
  if (!trace->is_started())
    return;

  Opt_trace_array trace_filesort(trace, "filesort_information");
  for (; s_length-- ; sortorder++)
  {
    Opt_trace_object oto(trace);
    oto.add_alnum("direction", sortorder->reverse ? "desc" : "asc");

    if (sortorder->field)
    {
      if (strlen(sortorder->field->table->alias) != 0)
        oto.add_utf8_table(sortorder->field->table->pos_in_table_list);
      else
        oto.add_alnum("table", "intermediate_tmp_table");
      oto.add_alnum("field", sortorder->field->field_name ?
                    sortorder->field->field_name : "tmp_table_column");
    }
    else
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
  @param      sort_positions Set to TRUE if we want to force sorting by position
                             (Needed by UPDATE/INSERT or ALTER TABLE or
                              when rowids are required by executor)
  @param[out] examined_rows  Store number of examined rows here
                             This is the number of found rows before
                             applying WHERE condition.
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
              ha_rows *examined_rows, ha_rows *found_rows,
              ha_rows *returned_rows)
{
  int error;
  ulong memory_available= thd->variables.sortbuff_size;
  size_t num_chunks;
  ha_rows num_rows_found= HA_POS_ERROR;
  ha_rows num_rows_estimate= HA_POS_ERROR;
  IO_CACHE tempfile;   // Temporary file for storing intermediate results.
  IO_CACHE chunk_file; // For saving Merge_chunk structs.
  IO_CACHE *outfile;   // Contains the final, sorted result.
  Sort_param param;
  bool multi_byte_charset;
  Bounded_queue<uchar *, uchar *, Sort_param, Mem_compare_queue_key>
    pq((Malloc_allocator<uchar*>
        (key_memory_Filesort_info_record_pointers)));
  Opt_trace_context * const trace= &thd->opt_trace;
  QEP_TAB *const tab= filesort->tab;
  TABLE *const table= tab->table();
  ha_rows max_rows= filesort->limit;
  uint s_length= 0;

  DBUG_ENTER("filesort");

  if (!(s_length= filesort->make_sortorder()))
    DBUG_RETURN(true);  /* purecov: inspected */

  /*
    We need a nameless wrapper, since we may be inside the "steps" of
    "join_execution".
  */
  Opt_trace_object trace_wrapper(trace);
  trace_filesort_information(trace, filesort->sortorder, s_length);

  DBUG_ASSERT(!table->reginfo.join_tab);
  DBUG_ASSERT(tab == table->reginfo.qep_tab);
  Item_subselect *const subselect= tab && tab->join() ?
    tab->join()->select_lex->master_unit()->item : NULL;

  DEBUG_SYNC(thd, "filesort_start");

  /* 
    Don't use table->sort in filesort as it is also used by 
    QUICK_INDEX_MERGE_SELECT. Work with a copy and put it back at the end 
    when index_merge select has finished with it.
  */
  Filesort_info table_sort= table->sort;
  table->sort.io_cache= NULL;
  DBUG_ASSERT(table_sort.sorted_result == NULL);
  table_sort.sorted_result_in_fsbuf= false;

  outfile= table_sort.io_cache;
  my_b_clear(&tempfile);
  my_b_clear(&chunk_file);
  error= 1;

  param.init_for_filesort(filesort,
                          make_array(filesort->sortorder, s_length),
                          sortlength(thd, filesort->sortorder, s_length,
                                     &multi_byte_charset),
                          table,
                          thd->variables.max_length_for_sort_data,
                          max_rows, sort_positions);

  table_sort.addon_fields= param.addon_fields;

  if (tab->quick())
    thd->inc_status_sort_range();
  else
    thd->inc_status_sort_scan();

  // If number of rows is not known, use as much of sort buffer as possible. 
  num_rows_estimate= table->file->estimate_rows_upper_bound();

  if (multi_byte_charset &&
      !(param.tmp_buffer= (char*)
        my_malloc(key_memory_Sort_param_tmp_buffer,
                  param.max_compare_length(), MYF(MY_WME))))
    goto err;

  if (check_if_pq_applicable(trace, &param, &table_sort,
                             table, num_rows_estimate, memory_available,
                             subselect != NULL))
  {
    DBUG_PRINT("info", ("filesort PQ is applicable"));
    /*
      For PQ queries (with limit) we know exactly how many pointers/records
      we have in the buffer, so to simplify things, we initialize
      all pointers here. (We cannot pack fields anyways, so there is no
      point in doing lazy initialization).
     */
    table_sort.init_record_pointers();

    if (pq.init(param.max_rows,
                &param, table_sort.get_sort_keys()))
    {
      /*
       If we fail to init pq, we have to give up:
       out of memory means my_malloc() will call my_error().
      */
      DBUG_PRINT("info", ("failed to allocate PQ"));
      table_sort.free_sort_buffer();
      DBUG_ASSERT(thd->is_error());
      goto err;
    }
    filesort->using_pq= true;
    param.using_pq= true;
    param.m_addon_fields_status= Addon_fields_status::using_priority_queue;
  }
  else
  {
    DBUG_PRINT("info", ("filesort PQ is not applicable"));
    filesort->using_pq= false;
    param.using_pq= false;

    /*
      When sorting using priority queue, we cannot use packed addons.
      Without PQ, we can try.
    */
    param.try_to_pack_addons(thd->variables.max_length_for_sort_data);

    /*
      We need space for at least one record from each merge chunk, i.e.
        param->max_rows_per_buffer >= MERGEBUFF2
      See merge_buffers()),
      memory_available must be large enough for
        param->max_rows_per_buffer * (record + record pointer) bytes
      (the main sort buffer, see alloc_sort_buffer()).
      Hence this minimum:
    */
    const ulong min_sort_memory=
      max<ulong>(MIN_SORT_MEMORY,
                 ALIGN_SIZE(MERGEBUFF2 *
                            (param.max_record_length() + sizeof(uchar*))));
    /*
      Cannot depend on num_rows. For external sort, space for upto MERGEBUFF2
      rows is required.
    */
    if (num_rows_estimate < MERGEBUFF2)
      num_rows_estimate= MERGEBUFF2;

    while (memory_available >= min_sort_memory)
    {
      ha_rows keys=
        memory_available / (param.max_record_length() + sizeof(char*));
      // If the table is empty, allocate space for one row.
      param.max_rows_per_buffer=
        min(num_rows_estimate > 0 ? num_rows_estimate : 1, keys);

      table_sort.alloc_sort_buffer(param.max_rows_per_buffer,
                                   param.max_record_length());
      if (table_sort.sort_buffer_size() > 0)
        break;
      ulong old_memory_available= memory_available;
      memory_available= memory_available/4*3;
      if (memory_available < min_sort_memory &&
          old_memory_available > min_sort_memory)
        memory_available= min_sort_memory;
    }
    if (memory_available < min_sort_memory)
    {
      my_error(ER_OUT_OF_SORTMEMORY,MYF(ME_ERRORLOG + ME_FATALERROR));
      goto err;
    }
  }

  if (open_cached_file(&chunk_file,mysql_tmpdir,TEMP_PREFIX,
		       DISK_BUFFER_SIZE, MYF(MY_WME)))
    goto err;

  param.sort_form= table;

  // New scope, because subquery execution must be traced within an array.
  {
    Opt_trace_array ota(trace, "filesort_execution");
    num_rows_found= find_all_keys(thd, &param, tab,
                                  &table_sort,
                                  &chunk_file,
                                  &tempfile,
                                  param.using_pq ? &pq : NULL,
                                  found_rows);
    if (num_rows_found == HA_POS_ERROR)
      goto err;
  }

  num_chunks= static_cast<size_t>(my_b_tell(&chunk_file)) /
    sizeof(Merge_chunk);

  if (num_chunks == 0)                   // The whole set is in memory
  {
    if (save_index(&param, num_rows_found, &table_sort))
      goto err;
  }
  else
  {
    // We will need an extra buffer in rr_unpack_from_tempfile()
    if (table_sort.addon_fields != nullptr &&
        !(table_sort.addon_fields->allocate_addon_buf(param.m_addon_length)))
      goto err;                                 /* purecov: inspected */

    table_sort.read_chunk_descriptors(&chunk_file, num_chunks);
    if (table_sort.merge_chunks.is_null())
      goto err;                                 /* purecov: inspected */

    close_cached_file(&chunk_file);

    /* Open cached file if it isn't open */
    if (! my_b_inited(outfile) &&
	open_cached_file(outfile,mysql_tmpdir,TEMP_PREFIX,READ_RECORD_BUFFER,
			  MYF(MY_WME)))
      goto err;
    if (reinit_io_cache(outfile,WRITE_CACHE,0L,0,0))
      goto err;

    /*
      Use also the space previously used by string pointers in sort_buffer
      for temporary key storage.
    */
    param.max_rows_per_buffer=
      static_cast<uint>(table_sort.sort_buffer_size() /
                        param.max_record_length());

    if (merge_many_buff(thd, &param,
                        table_sort.get_raw_buf(),
                        table_sort.merge_chunks,
                        &num_chunks,
                        &tempfile))
      goto err;
    if (flush_io_cache(&tempfile) ||
	reinit_io_cache(&tempfile,READ_CACHE,0L,0,0))
      goto err;
    if (merge_index(thd, &param,
                    table_sort.get_raw_buf(),
                    Merge_chunk_array(table_sort.merge_chunks.begin(),
                                      num_chunks),
                    &tempfile,
                    outfile))
      goto err;
  }

  if (trace->is_started())
  {
    char buffer[100];
    String sort_mode(buffer, sizeof(buffer), &my_charset_bin);
    sort_mode.length(0);
    sort_mode.append("<");
    if (param.using_varlen_keys())
      sort_mode.append("varlen_sort_key");
    else
      sort_mode.append("fixed_sort_key");
    sort_mode.append(", ");
    sort_mode.append(param.using_packed_addons() ?
                     "packed_additional_fields" :
                     param.using_addon_fields() ?
                     "additional_fields" : "rowid");
    sort_mode.append(">");

    const char *algo_text[]= {
      "none", "radix", "std::sort", "std::stable_sort"
    };

    Opt_trace_object filesort_summary(trace, "filesort_summary");
    filesort_summary
      .add("memory_available", memory_available)
      .add("key_size", param.max_compare_length())
      .add("row_size", param.max_record_length())
      .add("max_rows_per_buffer", param.max_rows_per_buffer)
      .add("num_rows_estimate", num_rows_estimate)
      .add("num_rows_found", num_rows_found)
      .add("num_examined_rows", param.num_examined_rows)
      .add("num_tmp_files", num_chunks)
      .add("sort_buffer_size", table_sort.sort_buffer_size())
      .add_alnum("sort_algorithm", algo_text[param.m_sort_algorithm]);
    if (!param.using_packed_addons())
      filesort_summary.add_alnum("unpacked_addon_fields",
                                 addon_fields_text(param.
                                                   m_addon_fields_status));
    filesort_summary
      .add_alnum("sort_mode", sort_mode.c_ptr());
  }

  if (num_rows_found > param.max_rows)
  {
    // If find_all_keys() produced more results than the query LIMIT.
    num_rows_found= param.max_rows;
  }
  error= 0;

 err:
  my_free(param.tmp_buffer);
  if (!subselect || !subselect->is_uncacheable())
  {
    if (!table_sort.sorted_result_in_fsbuf)
      table_sort.free_sort_buffer();
    my_free(table_sort.merge_chunks.array());
    table_sort.merge_chunks= Merge_chunk_array(NULL, 0);
  }
  close_cached_file(&tempfile);
  close_cached_file(&chunk_file);
  if (my_b_inited(outfile))
  {
    if (flush_io_cache(outfile))
      error=1;
    {
      my_off_t save_pos=outfile->pos_in_file;
      /* For following reads */
      if (reinit_io_cache(outfile,READ_CACHE,0L,0,0))
	error=1;
      outfile->end_of_file=save_pos;
    }
  }
  if (error)
  {
    DBUG_ASSERT(thd->is_error() || thd->killed);

    /*
      We replace the table->sort at the end.
      Hence calling free_io_cache to make sure table->sort.io_cache
      used for QUICK_INDEX_MERGE_SELECT is free.
    */
    free_io_cache(table);

    /*
      Guard against Bug#11745656 -- KILL QUERY should not send "server shutdown"
      to client!
    */
    const char *cause= thd->killed
                       ? ((thd->killed == THD::KILL_CONNECTION
                         && !connection_events_loop_aborted())
                          ? ER_THD(thd, THD::KILL_QUERY)
                          : ER_THD(thd, thd->killed))
                       : thd->get_stmt_da()->message_text();
    const char *msg=   ER_THD(thd, ER_FILSORT_ABORT);

    my_printf_error(ER_FILSORT_ABORT,
                    "%s: %s",
                    MYF(0),
                    msg,
                    cause);

    if (thd->is_fatal_error)
      sql_print_information("%s, host: %s, user: %s, "
                            "thread: %u, error: %s, query: %-.4096s",
                            msg,
                            thd->security_context()->host_or_ip().str,
                            thd->security_context()->priv_user().str,
                            thd->thread_id(),
                            cause,
                            thd->query().str);
  }
  else
    thd->inc_status_sort_rows(num_rows_found);
  *examined_rows= param.num_examined_rows;
  *returned_rows= num_rows_found;

  /* table->sort.io_cache should be free by this time */
  DBUG_ASSERT(NULL == table->sort.io_cache);

  // Assign the copy back!
  table->sort= table_sort;

  DBUG_PRINT("exit",
             ("num_rows: %ld examined_rows: %ld found_rows: %ld",
              static_cast<long>(num_rows_found),
              static_cast<long>(*examined_rows),
              static_cast<long>(*found_rows)));
  DBUG_RETURN(error);
} /* filesort */


void filesort_free_buffers(TABLE *table, bool full)
{
  DBUG_ENTER("filesort_free_buffers");
  my_free(table->sort.sorted_result);
  table->sort.sorted_result= NULL;
  table->sort.sorted_result_in_fsbuf= false;

  if (full)
  {
    table->sort.free_sort_buffer();
    my_free(table->sort.merge_chunks.array());
    table->sort.merge_chunks= Merge_chunk_array(NULL, 0);
  }

  table->sort.addon_fields= NULL;
  DBUG_VOID_RETURN;
}


uint Filesort::make_sortorder()
{
  uint count;
  st_sort_field *sort,*pos;
  ORDER *ord;
  DBUG_ENTER("make_sortorder");

  count=0;
  for (ord = order; ord; ord= ord->next)
    count++;
  DBUG_ASSERT(count > 0);

  const size_t sortorder_size= sizeof(*sortorder) * (count + 1);
  if (sortorder == nullptr)
    sortorder= static_cast<st_sort_field*>(sql_alloc(sortorder_size));
  if (sortorder == nullptr)
    DBUG_RETURN(0); /* purecov: inspected */
  memset(sortorder, 0, sortorder_size);

  pos= sort= sortorder;
  for (ord= order; ord; ord= ord->next, pos++)
  {
    Item *const item= ord->item[0], *const real_item= item->real_item();
    if (real_item->type() == Item::FIELD_ITEM)
    {
      /*
        Could be a field, or Item_direct_view_ref/Item_ref wrapping a field
        If it is an Item_outer_ref, only_full_group_by has been switched off.
      */
      DBUG_ASSERT
        (item->type() == Item::FIELD_ITEM ||
         (item->type() == Item::REF_ITEM &&
          (down_cast<Item_ref*>(item)->ref_type() == Item_ref::VIEW_REF
           || down_cast<Item_ref*>(item)->ref_type() == Item_ref::OUTER_REF
           || down_cast<Item_ref*>(item)->ref_type() == Item_ref::REF)
          ));
      pos->field= down_cast<Item_field*>(real_item)->field;
    }
    else if (real_item->type() == Item::SUM_FUNC_ITEM &&
             !real_item->const_item())
    {
      // Aggregate, or Item_aggregate_ref
      DBUG_ASSERT(item->type() == Item::SUM_FUNC_ITEM ||
                  (item->type() == Item::REF_ITEM &&
                   static_cast<Item_ref*>(item)->ref_type() ==
                   Item_ref::AGGREGATE_REF));
      pos->field= item->get_tmp_table_field();
    }
    else if (real_item->type() == Item::COPY_STR_ITEM)
    {						// Blob patch
      pos->item= static_cast<Item_copy*>(real_item)->get_item();
    }
    else
      pos->item= item;
    pos->reverse= (ord->direction == ORDER_DESC);
    DBUG_ASSERT(pos->field != NULL || pos->item != NULL);
  }
  DBUG_RETURN(count);
}


void Filesort_info::read_chunk_descriptors(IO_CACHE *chunk_file, uint count)
{
  DBUG_ENTER("Filesort_info::read_chunk_descriptors");

  // If we already have a chunk array, we're doing sort in a subquery.
  if (!merge_chunks.is_null() &&
      merge_chunks.size() < count)
  {
    my_free(merge_chunks.array());              /* purecov: inspected */
    merge_chunks= Merge_chunk_array(NULL, 0);   /* purecov: inspected */
  }

  void *rawmem= merge_chunks.array();
  const size_t length= sizeof(Merge_chunk) * count;
  if (NULL == rawmem)
  {
    rawmem= my_malloc(key_memory_Filesort_info_merge, length, MYF(MY_WME));
    if (rawmem == NULL)
      DBUG_VOID_RETURN;                         /* purecov: inspected */
  }

  if (reinit_io_cache(chunk_file, READ_CACHE, 0L, 0, 0) ||
      my_b_read(chunk_file, static_cast<uchar*>(rawmem), length))
  {
    my_free(rawmem);                            /* purecov: inspected */
    rawmem= NULL;                               /* purecov: inspected */
    count= 0;                                   /* purecov: inspected */
  }

  merge_chunks= Merge_chunk_array(static_cast<Merge_chunk*>(rawmem), count);
  DBUG_VOID_RETURN;
}

#ifndef DBUG_OFF
/*
  Print a text, SQL-like record representation into dbug trace.

  Note: this function is a work in progress: at the moment
   - column read bitmap is ignored (can print garbage for unused columns)
   - there is no quoting
*/
static void dbug_print_record(TABLE *table, bool print_rowid)
{
  char buff[1024];
  Field **pfield;
  String tmp(buff,sizeof(buff),&my_charset_bin);
  DBUG_LOCK_FILE;
  
  fprintf(DBUG_FILE, "record (");
  for (pfield= table->field; *pfield ; pfield++)
    fprintf(DBUG_FILE, "%s%s", (*pfield)->field_name, (pfield[1])? ", ":"");
  fprintf(DBUG_FILE, ") = ");

  fprintf(DBUG_FILE, "(");
  for (pfield= table->field; *pfield ; pfield++)
  {
    Field *field=  *pfield;

    if (field->is_null()) {
      if (fwrite("NULL", sizeof(char), 4, DBUG_FILE) != 4) {
        goto unlock_file_and_quit;
      }
    }
   
    if (field->type() == MYSQL_TYPE_BIT)
      (void) field->val_int_as_str(&tmp, 1);
    else
      field->val_str(&tmp);

    if (fwrite(tmp.ptr(),sizeof(char),tmp.length(),DBUG_FILE) != tmp.length()) {
      goto unlock_file_and_quit;
    }

    if (pfield[1]) {
      if (fwrite(", ", sizeof(char), 2, DBUG_FILE) != 2) {
        goto unlock_file_and_quit;
      }
    }
  }
  fprintf(DBUG_FILE, ")");
  if (print_rowid)
  {
    fprintf(DBUG_FILE, " rowid ");
    for (uint i=0; i < table->file->ref_length; i++)
    {
      fprintf(DBUG_FILE, "%x", table->file->ref[i]);
    }
  }
  fprintf(DBUG_FILE, "\n");
unlock_file_and_quit:
  DBUG_UNLOCK_FILE;
}
#endif 


/// Error handler for filesort.
class Filesort_error_handler : public Internal_error_handler
{
  THD *m_thd;                ///< The THD in which filesort is executed.
  bool m_seen_not_supported; ///< Has a not supported warning has been seen?
public:
  /**
    Create an error handler and push it onto the error handler
    stack. The handler will be automatically popped from the error
    handler stack when it is destroyed.
  */
  Filesort_error_handler(THD *thd)
    : m_thd(thd), m_seen_not_supported(false)
  {
    thd->push_internal_handler(this);
  }

  /**
    Pop the error handler from the error handler stack, and destroy
    it.
  */
  ~Filesort_error_handler()
  {
    m_thd->pop_internal_handler();
  }

  /**
    Handle a condition.

    The handler will make sure that no more than a single
    ER_NOT_SUPPORTED_YET warning will be seen by the higher
    layers. This warning is generated by Json_wrapper::make_sort_key()
    for every value that it doesn't know how to create a sort key
    for. It is sufficient for the higher layers to report this warning
    only once per sort.
  */
  virtual bool handle_condition(THD*,
                                uint sql_errno,
                                const char*,
                                Sql_condition::enum_severity_level *level,
                                const char*)
  {
    if (*level == Sql_condition::SL_WARNING &&
        sql_errno == ER_NOT_SUPPORTED_YET)
    {
      if (m_seen_not_supported)
        return true;
      m_seen_not_supported= true;
    }

    return false;
  }

};


static const Item::enum_walk walk_subquery=
  Item::enum_walk(Item::WALK_POSTFIX | Item::WALK_SUBQUERY);

/**
  Search after sort_keys, and write them into tempfile
  (if we run out of space in the sort buffer).
  All produced sequences are guaranteed to be non-empty.

  @param thd               Thread handle
  @param param             Sorting parameter
  @param qep_tab            Use this to get source data
  @param fs_info           Struct containing sort buffer etc.
  @param chunk_file        File to write Merge_chunks describing sorted segments
                           in tempfile.
  @param tempfile          File to write sorted sequences of sortkeys to.
  @param pq                If !NULL, use it for keeping top N elements
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
         if (no free space in sort buffer)
         {
           sort buffer;
           dump sorted sequence to 'tempfile';
           dump Merge_chunk describing sequence location into 'chunk_file';
         }
         put sort key into buffer;
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

static ha_rows find_all_keys(THD *thd, Sort_param *param, QEP_TAB *qep_tab,
                             Filesort_info *fs_info,
                             IO_CACHE *chunk_file,
                             IO_CACHE *tempfile,
                             Bounded_queue<uchar *, uchar *, Sort_param,
                                           Mem_compare_queue_key> *pq,
                             ha_rows *found_rows)
{
  int error,flag;
  uint idx,indexpos,ref_length;
  uchar *ref_pos,*next_pos,ref_buff[MAX_REFLENGTH];
  my_off_t record;
  TABLE *sort_form;
  volatile THD::killed_state *killed= &thd->killed;
  handler *file;
  MY_BITMAP *save_read_set, *save_write_set;
  bool skip_record;
  ha_rows num_records= 0;
  const bool packed_addon_fields= param->using_packed_addons();
  const bool using_varlen_keys= param->using_varlen_keys();

  /*
    Set up an error handler for filesort. It is automatically pushed
    onto the internal error handler stack upon creation, and will be
    popped off the stack automatically when the handler goes out of
    scope.
  */
  Filesort_error_handler error_handler(thd);

  DBUG_ENTER("find_all_keys");
  DBUG_PRINT("info",("using: %s",
                     (qep_tab->condition() ? qep_tab->quick() ? "ranges" : "where":
                      "every row")));

  idx=indexpos=0;
  error= 0;
  sort_form=param->sort_form;
  file=sort_form->file;
  ref_length=param->ref_length;
  ref_pos= ref_buff;
  const bool quick_select= qep_tab->quick() != NULL;
  record=0;
  *found_rows= 0;
  flag= ((file->ha_table_flags() & HA_REC_NOT_IN_SEQ) || quick_select);
  if (flag)
    ref_pos= &file->ref[0];
  next_pos=ref_pos;
  if (!quick_select)
  {
    next_pos=(uchar*) 0;			/* Find records in sequence */
    DBUG_EXECUTE_IF("bug14365043_1",
                    DBUG_SET("+d,ha_rnd_init_fail"););
    if ((error= file->ha_rnd_init(1)))
    {
      file->print_error(error, MYF(0));
      DBUG_RETURN(HA_POS_ERROR);
    }
    file->extra_opt(HA_EXTRA_CACHE,
		    thd->variables.read_buff_size);
  }

  if (quick_select)
  {
    if ((error= qep_tab->quick()->reset()))
    {
      file->print_error(error, MYF(0));
      DBUG_RETURN(HA_POS_ERROR);
    }
  }

  /* Remember original bitmaps */
  save_read_set=  sort_form->read_set;
  save_write_set= sort_form->write_set;
  /*
    Set up temporary column read map for columns used by sort and verify
    it's not used
  */
  DBUG_ASSERT(sort_form->tmp_set.n_bits == 0 ||
              bitmap_is_clear_all(&sort_form->tmp_set));

  // Temporary set for register_used_fields and mark_field_in_map()
  sort_form->read_set= &sort_form->tmp_set;
  // Include fields used for sorting in the read_set.
  register_used_fields(param); 

  // Include fields used by conditions in the read_set.
  if (qep_tab->condition())
  {
    Mark_field mf(sort_form, MARK_COLUMNS_TEMP);
    qep_tab->condition()->walk(&Item::mark_field_in_map,
                               walk_subquery, (uchar*) &mf);
  }
  // Include fields used by pushed conditions in the read_set.
  if (qep_tab->table()->file->pushed_idx_cond)
  {
    Mark_field mf(sort_form, MARK_COLUMNS_TEMP);
    qep_tab->table()->file->pushed_idx_cond->walk(&Item::mark_field_in_map,
                                                  walk_subquery,
                                                  (uchar*) &mf);
  }
  sort_form->column_bitmaps_set(&sort_form->tmp_set, &sort_form->tmp_set);

  DEBUG_SYNC(thd, "after_index_merge_phase1");
  for (;;)
  {
    if (quick_select)
    {
      if ((error= qep_tab->quick()->get_next()))
        break;
      file->position(sort_form->record[0]);
      DBUG_EXECUTE_IF("debug_filesort", dbug_print_record(sort_form, TRUE););
    }
    else					/* Not quick-select */
    {
      DBUG_EXECUTE_IF("bug19656296", DBUG_SET("+d,ha_rnd_next_deadlock"););
      {
	error= file->ha_rnd_next(sort_form->record[0]);
	if (!flag)
	{
	  my_store_ptr(ref_pos,ref_length,record); // Position to row
	  record+= sort_form->s->db_record_offset;
	}
	else if (!error)
	  file->position(sort_form->record[0]);
      }
      if (error && error != HA_ERR_RECORD_DELETED)
	break;
    }

    if (*killed)
    {
      DBUG_PRINT("info",("Sort killed by user"));
      if (!quick_select)
      {
        (void) file->extra(HA_EXTRA_NO_CACHE);
        file->ha_rnd_end();
      }
      num_records= HA_POS_ERROR;
      goto cleanup;
    }
    if (error == 0)
      param->num_examined_rows++;
    if (!error && !qep_tab->skip_record(thd, &skip_record) && !skip_record)
    {
      ++(*found_rows);
      if (pq)
        pq->push(ref_pos);
      else
      {
        if (fs_info->isfull())
        {
          if (write_keys(param, fs_info, idx, chunk_file, tempfile))
          {
            num_records= HA_POS_ERROR;
            goto cleanup;
          }
          idx= 0;
          indexpos++;
        }
        if (idx == 0)
          fs_info->init_next_record_pointer();
        uchar *start_of_rec= fs_info->get_next_record_pointer();

        const uint rec_sz= param->make_sortkey(start_of_rec, ref_pos);
        if ((packed_addon_fields || using_varlen_keys) &&
            rec_sz != param->max_record_length())
          fs_info->adjust_next_record_pointer(rec_sz);

        idx++;
        num_records++;
      }
    }
    /*
      Don't try unlocking the row if skip_record reported an error since in
      this case the transaction might have been rolled back already.
    */
    else if (!thd->is_error())
      file->unlock_row();
    /* It does not make sense to read more keys in case of a fatal error */
    if (thd->is_error())
      break;
  }
  if (!quick_select)
  {
    (void) file->extra(HA_EXTRA_NO_CACHE);	/* End cacheing of records */
    if (!next_pos)
      file->ha_rnd_end();
  }

  if (thd->is_error())
  {
    num_records= HA_POS_ERROR;
    goto cleanup;
  }
  
  /* Signal we should use orignal column read and write maps */
  sort_form->column_bitmaps_set(save_read_set, save_write_set);

  DBUG_PRINT("test",("error: %d  indexpos: %d",error,indexpos));
  if (error != HA_ERR_END_OF_FILE)
  {
    myf my_flags;
    switch (error) {
    case HA_ERR_LOCK_DEADLOCK:
    case HA_ERR_LOCK_WAIT_TIMEOUT:
      my_flags= MYF(0);
      break;
    default:
      my_flags= MYF(ME_ERRORLOG);
    }
    file->print_error(error, my_flags);
    num_records= HA_POS_ERROR;
    goto cleanup;
  }
  if (indexpos && idx &&
      write_keys(param, fs_info, idx, chunk_file, tempfile))
  {
    num_records= HA_POS_ERROR;                            // purecov: inspected
    goto cleanup;
  }

  if (pq)
    num_records= pq->num_elements();

cleanup:
  // Clear tmp_set so it can be used elsewhere
  bitmap_clear_all(&sort_form->tmp_set);

  DBUG_PRINT("info", ("find_all_keys return %lu", (ulong) num_records));

  DBUG_RETURN(num_records);
} /* find_all_keys */


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

static int
write_keys(Sort_param *param, Filesort_info *fs_info, uint count,
           IO_CACHE *chunk_file, IO_CACHE *tempfile)
{
  Merge_chunk merge_chunk;
  DBUG_ENTER("write_keys");

  fs_info->sort_buffer(param, count);

  if (!my_b_inited(tempfile) &&
      open_cached_file(tempfile, mysql_tmpdir, TEMP_PREFIX, DISK_BUFFER_SIZE,
                       MYF(MY_WME)))
    DBUG_RETURN(1);                             /* purecov: inspected */

  // Check that we wont have more chunks than we can possibly keep in memory.
  if (my_b_tell(chunk_file) + sizeof(Merge_chunk) > (ulonglong)UINT_MAX)
    DBUG_RETURN(1);                             /* purecov: inspected */

  merge_chunk.set_file_position(my_b_tell(tempfile));
  if (static_cast<ha_rows>(count) > param->max_rows)
  {
    // Write only SELECT LIMIT rows to the file
    count= static_cast<uint>(param->max_rows);  /* purecov: inspected */
  }
  merge_chunk.set_rowcount(static_cast<ha_rows>(count));

  for (uint ix= 0; ix < count; ++ix)
  {
    uchar *record= fs_info->get_sorted_record(ix);
    size_t rec_length= param->get_record_length(record);

    if (my_b_write(tempfile, record, rec_length))
      DBUG_RETURN(1);                           /* purecov: inspected */
  }

  if (my_b_write(chunk_file, &merge_chunk, sizeof(merge_chunk)))
    DBUG_RETURN(1);                             /* purecov: inspected */

  DBUG_RETURN(0);
} /* write_keys */


/**
  Store length as suffix in high-byte-first order.
*/

static inline void store_length(uchar *to, size_t length, uint pack_length)
{
  switch (pack_length) {
  case 1:
    *to= (uchar) length;
    break;
  case 2:
    mi_int2store(to, length);
    break;
  case 3:
    mi_int3store(to, length);
    break;
  default:
    mi_int4store(to, length);
    break;
  }
}


#ifdef WORDS_BIGENDIAN
const bool Is_big_endian= true;
#else
const bool Is_big_endian= false;
#endif
static void copy_native_longlong(uchar *to, size_t to_length,
                                 longlong val, bool is_unsigned)
{
  copy_integer<Is_big_endian>(to, to_length,
                              static_cast<uchar*>(static_cast<void*>(&val)),
                              sizeof(longlong),
                              is_unsigned);
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
static uint MY_ATTRIBUTE((noinline))
make_json_sort_key(Item *item, uchar *to, uchar *null_indicator,
                   size_t length, ulonglong *hash)
{
  DBUG_ASSERT(!item->maybe_null || *null_indicator == 1);

  Json_wrapper wr;
  if (item->val_json(&wr))
  {
    // An error occurred, no point to continue making key, set it to null.
    if (item->maybe_null)
      *null_indicator= 0;
    return 0;
  }

  if (item->null_value)
  {
    /*
      Got NULL. The sort key should be all zeros. The caller has
      already tentatively set the NULL indicator byte at *null_indicator to
      not-NULL, so we need to clear that byte too.
    */
    if (item->maybe_null)
    {
      // Don't store anything but null flag.
      *null_indicator= 0;
      return 0;
    }
    /* purecov: begin inspected */
    DBUG_PRINT("warning",
               ("Got null on something that shouldn't be null"));
    DBUG_ABORT();
    return 0;
    /* purecov: end */
  }

  size_t actual_length= wr.make_sort_key(to, length);
  *hash= wr.make_hash_key(hash);
  return actual_length;
}


uint Sort_param::make_sortkey(uchar *to, const uchar *ref_pos)
{
  uchar *orig_to= to;
  const st_sort_field *sort_field;
  ulonglong hash= 0;

  if (using_varlen_keys())
  {
    to+= size_of_varlength_field;
  }
  for (sort_field= local_sortorder.begin() ;
       sort_field != local_sortorder.end() ;
       sort_field++)
  {
    bool maybe_null= false;
    uint actual_length= sort_field->length;

    if (sort_field->field)
    {
      Field *field= sort_field->field;
      DBUG_ASSERT(sort_field->field_type == field->type());
      if (field->maybe_null())
      {
	if (field->is_null())
	{
          if (sort_field->is_varlen)
          {
            // Don't store anything, except NULL flag, invert it here
            *to++= sort_field->reverse ? 0xff : 0;
            continue;
          }
	  if (sort_field->reverse)
	    memset(to, 255, sort_field->length+1);
	  else
	    memset(to, 0, sort_field->length+1);
	  to+= sort_field->length+1;
	  continue;
	}
	else
	  *to++=1;
      }
      if (sort_field->is_varlen)
      {
        DBUG_ASSERT(sort_field->length >= VARLEN_PREFIX);
        actual_length= field->make_sort_key(
          to + VARLEN_PREFIX, sort_field->length - VARLEN_PREFIX);
        DBUG_ASSERT(actual_length <= sort_field->length - VARLEN_PREFIX);
        int4store(to, actual_length + VARLEN_PREFIX);
      }
      else
      {
        actual_length= field->make_sort_key(to, sort_field->length);
        DBUG_ASSERT(actual_length == sort_field->length);
      }

      if (sort_field->field_type == MYSQL_TYPE_JSON)
      {
        DBUG_ASSERT(use_hash);
        unique_hash(field, &hash);
      }
    }
    else
    {						// Item
      Item *item=sort_field->item;
      maybe_null= item->maybe_null;
      DBUG_ASSERT(sort_field->field_type == item->data_type());
      switch (sort_field->result_type) {
      case STRING_RESULT:
      {
        uchar* null_indicator= nullptr;
        if (maybe_null)
        {
          null_indicator= to++;
          *null_indicator= 1;
        }

        if (sort_field->field_type == MYSQL_TYPE_JSON)
        {
          DBUG_ASSERT(use_hash);
          DBUG_ASSERT(sort_field->is_varlen);
          DBUG_ASSERT(sort_field->length >= VARLEN_PREFIX);
          /*
            We don't want the code for creating JSON sort keys to be
            inlined here, as increasing the size of the surrounding
            "else" branch seems to have a negative impact on some
            performance tests, even if those tests never execute the
            "else" branch.
          */
          actual_length= make_json_sort_key(
            item, to + VARLEN_PREFIX, null_indicator,
            sort_field->length - VARLEN_PREFIX, &hash);
          int4store(to, actual_length + VARLEN_PREFIX);
          break;
        }

        const CHARSET_INFO *cs=item->collation.collation;
        char fill_char= ((cs->state & MY_CS_BINSORT) ? (char) 0 : ' ');

        /* All item->str() to use some extra byte for end null.. */
        String tmp((char*) to,sort_field->length+4,cs);
        String *res= item->str_result(&tmp);
        if (!res)
        {
          if (maybe_null)
            memset(to-1, 0, sort_field->length+1);
          else      // The return value is null but the result may NOT be null.
          {
            /* purecov: begin deadcode */
            /*
              This assert should only trigger if we have an item marked
              as null when in fact it cannot be null.
              (ret_value == nullptr, null_value == true
              and maybe_null == false).
            */
            DBUG_ASSERT(0);
            DBUG_PRINT("warning",
                       ("Got null on something that shouldn't be null"));
            /*
               Avoid a crash by filling the field with zeroes
               and break as the error will be reported later in find_all_keys.
            */
            memset(to, 0, sort_field->length);
            /* purecov: end */
          }
          break;
        }
        uint length= static_cast<uint>(res->length());
        if (sort_field->need_strnxfrm)
        {
          char *from=(char*) res->ptr();
          size_t tmp_length MY_ATTRIBUTE((unused));
          if ((uchar*) from == to)
          {
            DBUG_ASSERT(sort_field->length >= length);
            set_if_smaller(length,sort_field->length);
            memcpy(tmp_buffer, from, length);
            from= tmp_buffer;
          }
          tmp_length=
            cs->coll->strnxfrm(cs, to, sort_field->length,
                               item->max_char_length(),
                               (uchar*) from, length,
                               MY_STRXFRM_PAD_TO_MAXLEN);
          DBUG_ASSERT(tmp_length == sort_field->length);
        }
        else
        {
          size_t diff;
          uint sort_field_length= sort_field->length -
            sort_field->suffix_length;
          if (sort_field_length < length)
          {
            diff= 0;
            length= sort_field_length;
          }
          else
            diff= sort_field_length - length;
          if (sort_field->suffix_length)
          {
            /* Store length last in result_string */
            store_length(to + sort_field_length, length,
                         sort_field->suffix_length);
          }

          my_strnxfrm(cs, to,length,(const uchar*)res->ptr(),length);
          cs->cset->fill(cs, (char *)to+length,diff,fill_char);
        }
        break;
      }
      case INT_RESULT:
	{
          longlong value= item->data_type() == MYSQL_TYPE_TIME ?
                          item->val_time_temporal_result() :
                          item->is_temporal_with_date() ?
                          item->val_date_temporal_result() :
                          item->val_int_result();
          if (maybe_null)
          {
	    *to++=1;				/* purecov: inspected */
            if (item->null_value)
            {
              if (maybe_null)
                memset(to-1, 0, sort_field->length+1);
              else
              {
                DBUG_PRINT("warning",
                           ("Got null on something that shouldn't be null"));
                memset(to, 0, sort_field->length);
              }
              break;
            }
          }
          copy_native_longlong(to, sort_field->length,
                               value, item->unsigned_flag);
	  break;
	}
      case DECIMAL_RESULT:
        {
          my_decimal dec_buf, *dec_val= item->val_decimal_result(&dec_buf);
          if (maybe_null)
          {
            if (item->null_value)
            { 
              memset(to, 0, sort_field->length+1);
              to++;
              break;
            }
            *to++=1;
          }
          if (sort_field->length < DECIMAL_MAX_FIELD_SIZE)
          {
            uchar buf[DECIMAL_MAX_FIELD_SIZE];
            my_decimal2binary(E_DEC_FATAL_ERROR, dec_val, buf,
                              item->max_length - (item->decimals ? 1:0),
                              item->decimals);
            memcpy(to, buf, sort_field->length);
          }
          else
          {
            my_decimal2binary(E_DEC_FATAL_ERROR, dec_val, to,
                              item->max_length - (item->decimals ? 1:0),
                              item->decimals);
          }
         break;
        }
      case REAL_RESULT:
	{
          double value= item->val_result();
	  if (maybe_null)
          {
            if (item->null_value)
            {
              memset(to, 0, sort_field->length+1);
              to++;
              break;
            }
	    *to++=1;
          }
          if (sort_field->length < sizeof(double))
          {
            uchar buf[sizeof(double)];
            change_double_for_sort(value, buf);
            memcpy(to, buf, sort_field->length);
          }
          else
          {
            change_double_for_sort(value, to);
          }
	  break;
	}
      case ROW_RESULT:
      default: 
	// This case should never be choosen
	DBUG_ASSERT(0);
	break;
      }
    }
    if (sort_field->reverse)
    {							/* Reverse key */
      if (maybe_null)
        to[-1]= ~to[-1];
      if (sort_field->is_varlen && actual_length)
      {
        to+= VARLEN_PREFIX;
      }
      while (actual_length--)
      {
	*to = (uchar) (~ *to);
	to++;
      }
    }
    else
    {
      if (sort_field->is_varlen && actual_length)
      {
        to+= VARLEN_PREFIX;
      }
      to+= actual_length;
    }
  }

  if (use_hash)
  {
    int8store(to, hash);
    to+= 8;
  }

  if (using_varlen_keys())
  {
    // Store the length of the record as a whole.
    Sort_param::store_varlen_key_length(orig_to,
                                        static_cast<uint>(to - orig_to));
  }

  if (using_addon_fields())
  {
    /* 
      Save field values appended to sorted fields.
      First null bit indicators are appended then field values follow.
    */
    uchar *nulls= to;
    uchar *p_len= to;

    Addon_fields_array::const_iterator addonf= addon_fields->begin();
    uint32 res_len= addonf->offset;
    const bool packed_addon_fields= addon_fields->using_packed_addons();
    memset(nulls, 0, addonf->offset);
    to+= addonf->offset;
    for ( ; addonf != addon_fields->end(); ++addonf)
    {
      Field *field= addonf->field;
      if (addonf->null_bit && field->is_null())
      {
        nulls[addonf->null_offset]|= addonf->null_bit;
        if (!packed_addon_fields)
          to+= addonf->max_length;
      }
      else
      {
        uchar *ptr= field->pack(to, field->ptr);
        int sz= static_cast<int>(ptr - to);
        res_len += sz;
        if (packed_addon_fields)
          to+= sz;
        else
          to+= addonf->max_length;
      }
    }
    if (packed_addon_fields)
      Addon_fields::store_addon_length(p_len, res_len);
    DBUG_PRINT("info", ("make_sortkey %p %u", orig_to, res_len));
  }
  else
  {
    /* Save filepos last */
    memcpy(to, ref_pos, ref_length);
    to+= ref_length;
  }
  return to - orig_to;
}


/*
  Register fields used by sorting in the sorted table's read set
*/

static void register_used_fields(Sort_param *param)
{
  Bounds_checked_array<st_sort_field>::const_iterator sort_field;
  TABLE *table=param->sort_form;
  MY_BITMAP *bitmap= table->read_set;
  Mark_field mf(table, MARK_COLUMNS_TEMP);

  for (sort_field= param->local_sortorder.begin() ;
       sort_field != param->local_sortorder.end() ;
       sort_field++)
  {
    Field *field;
    if ((field= sort_field->field))
    {
      if (field->table == table)
      {
        bitmap_set_bit(bitmap, field->field_index);
        if (field->is_virtual_gcol())
          table->mark_gcol_in_maps(field);
      }
    }
    else
    {						// Item
      sort_field->item->walk(&Item::mark_field_in_map, walk_subquery,
                             (uchar *)&mf);
    }
  }

  if (param->using_addon_fields())
  {
    Addon_fields_array::const_iterator addonf= param->addon_fields->begin();
    for ( ; addonf != param->addon_fields->end(); ++addonf)
    {
      Field *field= addonf->field;
      bitmap_set_bit(bitmap, field->field_index);
      if (field->is_virtual_gcol())
          table->mark_gcol_in_maps(field);
    }
  }
  else
  {
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

  The result data will be unpacked by rr_unpack_from_buffer()
  or rr_from_pointers()

  Note that rr_unpack_from_buffer() does not have access to a Sort_param.
  It does however have access to a Filesort_info, which knows whether
  we have variable sized keys or not.
  TODO: consider templatizing rr_unpack_from_buffer on is_varlen or not.

  @param [in]     param      Sort parameters.
  @param          count      Number of records
  @param [in,out] table_sort Information used by rr_unpack_from_buffer() /
                             rr_from_pointers()
 */
static bool save_index(Sort_param *param, uint count, Filesort_info *table_sort)
{
  uchar *to;
  DBUG_ENTER("save_index");

  table_sort->set_sort_length(param->max_compare_length(),
                              param->using_varlen_keys());

  table_sort->sort_buffer(param, count);

  if (param->using_addon_fields())
  {
    table_sort->sorted_result_in_fsbuf= true;
    DBUG_RETURN(0);
  }

  table_sort->sorted_result_in_fsbuf= false;
  const size_t buf_size= param->fixed_res_length * count;

  DBUG_ASSERT(table_sort->sorted_result == NULL);
  if (!(to= table_sort->sorted_result=
        static_cast<uchar*>(my_malloc(key_memory_Filesort_info_record_pointers,
                                      buf_size, MYF(MY_WME)))))
    DBUG_RETURN(1);                 /* purecov: inspected */
  table_sort->sorted_result_end=
    table_sort->sorted_result + buf_size;

  uint res_length= param->fixed_res_length;
  for (uint ix= 0; ix < count; ++ix)
  {
    uchar *record= table_sort->get_sorted_record(ix);
    uchar *start_of_payload= param->get_start_of_payload(record);
    memcpy(to, start_of_payload, res_length);
    to+= res_length;
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

bool check_if_pq_applicable(Opt_trace_context *trace,
                            Sort_param *param,
                            Filesort_info *filesort_info,
                            TABLE *table, ha_rows num_rows,
                            ulong memory_available,
                            bool keep_addon_fields)
{
  DBUG_ENTER("check_if_pq_applicable");

  /*
    How much Priority Queue sort is slower than qsort.
    Measurements (see unit test) indicate that PQ is roughly 3 times slower.
  */
  const double PQ_slowness= 3.0;

  Opt_trace_object trace_filesort(trace,
                                  "filesort_priority_queue_optimization");
  if (param->max_rows == HA_POS_ERROR)
  {
    trace_filesort
      .add("usable", false)
      .add_alnum("cause", "not applicable (no LIMIT)");
    DBUG_RETURN(false);
  }

  trace_filesort
    .add("limit", param->max_rows);

  if (param->max_rows + 2 >= UINT_MAX)
  {
    trace_filesort.add("usable", false).add_alnum("cause", "limit too large");
    DBUG_RETURN(false);
  }

  ulong num_available_keys=
    memory_available / (param->max_record_length() + sizeof(char*));
  // We need 1 extra record in the buffer, when using PQ.
  param->max_rows_per_buffer= (uint) param->max_rows + 1;

  if (num_rows < num_available_keys)
  {
    // The whole source set fits into memory.
    if (param->max_rows < num_rows/PQ_slowness )
    {
      filesort_info->
        alloc_sort_buffer(param->max_rows_per_buffer,
                          param->max_record_length());
      trace_filesort.add("chosen", true);
      DBUG_RETURN(filesort_info->sort_buffer_size() > 0);
    }
    else
    {
      // PQ will be slower.
      trace_filesort.add("chosen", false)
        .add_alnum("cause", "sort_is_cheaper");
      DBUG_RETURN(false);
    }
  }

  // Do we have space for LIMIT rows in memory?
  if (param->max_rows_per_buffer < num_available_keys)
  {
    filesort_info->alloc_sort_buffer(param->max_rows_per_buffer,
                                     param->max_record_length());
    trace_filesort.add("chosen", true);
    DBUG_RETURN(filesort_info->sort_buffer_size() > 0);
  }

  // Try to strip off addon fields.
  if (!keep_addon_fields && param->using_addon_fields())
  {
    const ulong row_length=
      param->max_compare_length() + param->ref_length + sizeof(char*);
    num_available_keys= memory_available / row_length;

    Opt_trace_object trace_addon(trace, "strip_additional_fields");
    trace_addon.add("row_size", row_length);

    // Can we fit all the keys in memory?
    if (param->max_rows_per_buffer >= num_available_keys)
    {
      trace_addon.add("chosen", false).add_alnum("cause", "not_enough_space");
    }
    else
    {
      const Cost_model_table *cost_model= table->cost_model();
      const double sort_merge_cost=
        get_merge_many_buffs_cost_fast(num_rows,
                                       num_available_keys,
                                       row_length, cost_model);
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
      const double pq_cpu_cost= 
        (PQ_slowness * num_rows + param->max_rows_per_buffer) *
        cost_model->key_compare_cost(log((double) param->max_rows_per_buffer));
      const Cost_estimate scan_cost= table->file->table_scan_cost();
      const double pq_io_cost=
        param->max_rows * scan_cost.total_cost() / 2.0;
      const double pq_cost= pq_cpu_cost + pq_io_cost;
      trace_addon.add("priority_queue_cost", pq_cost);

      if (sort_merge_cost < pq_cost)
      {
        trace_addon.add("chosen", false);
        DBUG_RETURN(false);
      }

      trace_addon.add("chosen", true);
      filesort_info->
        alloc_sort_buffer(param->max_rows_per_buffer,
                          param->max_compare_length() + param->ref_length);
      if (filesort_info->sort_buffer_size() > 0)
      {
        // Make attached data to be references instead of fields.
        filesort_info->addon_fields= NULL;
        param->addon_fields= NULL;

        param->fixed_res_length= param->ref_length;
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
  Read data to buffer.

  @returns
    (uint)-1 if something goes wrong
*/
static
uint read_to_buffer(IO_CACHE *fromfile,
                    Merge_chunk *merge_chunk,
                    Sort_param *param)
{
  DBUG_ENTER("read_to_buffer");
  uint rec_length= param->max_record_length();
  ha_rows count;

  const bool packed_addon_fields= param->using_packed_addons();
  const bool using_varlen_keys= param->using_varlen_keys();

  if ((count= min(merge_chunk->max_keys(), merge_chunk->rowcount())))
  {
    size_t bytes_to_read;
    if (packed_addon_fields || using_varlen_keys)
    {
      count= merge_chunk->rowcount();
      bytes_to_read=
        min(merge_chunk->buffer_size(),
            static_cast<size_t>(fromfile->end_of_file -
                                merge_chunk->file_position()));
    }
    else
      bytes_to_read= rec_length * static_cast<size_t>(count);

    DBUG_PRINT("info", ("read_to_buffer %p at file_pos %llu bytes %llu",
                        merge_chunk,
                        static_cast<ulonglong>(merge_chunk->file_position()),
                        static_cast<ulonglong>(bytes_to_read)));
    if (mysql_file_pread(fromfile->file,
                         merge_chunk->buffer_start(),
                         bytes_to_read,
                         merge_chunk->file_position(), MYF_RW))
      DBUG_RETURN((uint) -1);			/* purecov: inspected */

    size_t num_bytes_read;
    if (packed_addon_fields || using_varlen_keys)
    {
      /*
        The last record read is most likely not complete here.
        We need to loop through all the records, reading the length fields,
        and then "chop off" the final incomplete record.
       */
      uchar *record= merge_chunk->buffer_start();
      uint ix= 0;
      for (; ix < count; ++ix)
      {
        if (using_varlen_keys &&
            (record + Sort_param::size_of_varlength_field)
            >= merge_chunk->buffer_end())
          break;                                // Incomplete record.

        uchar *start_of_payload= param->get_start_of_payload(record);
        if (start_of_payload >= merge_chunk->buffer_end())
          break;                                // Incomplete record.

        if (packed_addon_fields &&
            start_of_payload + Addon_fields::size_of_length_field >=
            merge_chunk->buffer_end())
          break;                                // Incomplete record.

        const uint res_length= packed_addon_fields ?
          Addon_fields::read_addon_length(start_of_payload) :
          param->fixed_res_length;

        if (start_of_payload + res_length >= merge_chunk->buffer_end())
          break;                                // Incomplete record.

        DBUG_ASSERT(res_length > 0);
        record= start_of_payload + res_length;
      }
      DBUG_ASSERT(ix > 0);
      count= ix;
      num_bytes_read= record - merge_chunk->buffer_start();
      DBUG_PRINT("info", ("read %llu bytes of complete records",
                          static_cast<ulonglong>(bytes_to_read)));
    }
    else
      num_bytes_read= bytes_to_read;

    merge_chunk->init_current_key();
    merge_chunk->advance_file_position(num_bytes_read);
    merge_chunk->decrement_rowcount(count);
    merge_chunk->set_mem_count(count);
    DBUG_RETURN(num_bytes_read);
  }

  DBUG_RETURN (0);
} /* read_to_buffer */


namespace {

/**
  This struct is used for merging chunks for filesort()
  For filesort() with fixed-size keys we use memcmp to compare rows.
  For variable length keys, we use cmp_varlen_keys to compare rows.
 */
struct Merge_chunk_greater
{
  size_t m_len;
  Sort_param *m_param;

  // CTOR for filesort() with fixed-size keys
  explicit Merge_chunk_greater(size_t len)
    : m_len(len), m_param(nullptr)
  {}

  // CTOR for filesort() with varlen keys
  explicit Merge_chunk_greater(Sort_param *param)
    : m_len(0), m_param(param)
  {}

  bool operator()(Merge_chunk *a, Merge_chunk *b)
  {
    uchar *key1= a->current_key();
    uchar *key2= b->current_key();
    // Fixed len keys
    if (m_len)
      return memcmp(key1, key2, m_len) > 0;

    if (m_param)
      return !cmp_varlen_keys(m_param->local_sortorder, key1, key2);

    // We can actually have zero-length sort key for filesort().
    return false;
  }
};

} // namespace


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
static
int merge_buffers(THD *thd, Sort_param *param, IO_CACHE *from_file,
                  IO_CACHE *to_file, Sort_buffer sort_buffer,
                  Merge_chunk *last_chunk,
                  Merge_chunk_array chunk_array,
                  int flag)
{
  int error= 0;
  uint rec_length,res_length;
  size_t sort_length;
  ha_rows maxcount;
  ha_rows max_rows,org_max_rows;
  my_off_t to_start_filepos;
  uchar *strpos;
  Merge_chunk *merge_chunk;
  volatile THD::killed_state *killed= &thd->killed;
  THD::killed_state not_killable;
  DBUG_ENTER("merge_buffers");

  thd->inc_status_sort_merge_passes();
  if (param->not_killable)
  {
    killed= &not_killable;
    not_killable= THD::NOT_KILLED;
  }

  rec_length= param->max_record_length();
  res_length= param->fixed_res_length;
  sort_length= param->max_compare_length();
  uint offset= (flag == 0) ? 0 : (rec_length - res_length);
  maxcount= (param->max_rows_per_buffer / chunk_array.size());
  to_start_filepos= my_b_tell(to_file);
  strpos= sort_buffer.array();
  org_max_rows= max_rows= param->max_rows;

  /* The following will fire if there is not enough space in sort_buffer */
  DBUG_ASSERT(maxcount != 0);

  Merge_chunk_greater mcl=
    param->using_varlen_keys() ? Merge_chunk_greater(param) :
    Merge_chunk_greater(sort_length);
  Priority_queue<Merge_chunk*,
                 std::vector<Merge_chunk*, Malloc_allocator<Merge_chunk*> >,
                 Merge_chunk_greater>
    queue(mcl,
          Malloc_allocator<Merge_chunk*>(key_memory_Filesort_info_merge));

  if (queue.reserve(chunk_array.size()))
    DBUG_RETURN(1);

  for (merge_chunk= chunk_array.begin() ;
       merge_chunk != chunk_array.end() ; merge_chunk++)
  {
    const size_t chunk_sz= sort_buffer.size() / chunk_array.size();
    merge_chunk->set_buffer(strpos, strpos + chunk_sz);

    merge_chunk->set_max_keys(maxcount);
    strpos+= chunk_sz;
    error= static_cast<int>(read_to_buffer(from_file, merge_chunk, param));

    if (error == -1)
      DBUG_RETURN(error);     /* purecov: inspected */
    // If less data in buffers than expected
    merge_chunk->set_max_keys(merge_chunk->mem_count());
    (void) queue.push(merge_chunk);
  }

  while (queue.size() > 1)
  {
    if (*killed)
    {
      DBUG_RETURN(1);                         /* purecov: inspected */
    }
    for (;;)
    {
      merge_chunk= queue.top();
      {
        param->get_rec_and_res_len(merge_chunk->current_key(),
                                   &rec_length, &res_length);
        const uint bytes_to_write= (flag == 0) ? rec_length : res_length;

        if (flag && param->using_varlen_keys())
          offset= rec_length - res_length;

        DBUG_PRINT("info", ("write record at %llu len %u",
                            my_b_tell(to_file), bytes_to_write));
        if (my_b_write(to_file,
                       merge_chunk->current_key() + offset, bytes_to_write))
        {
          DBUG_RETURN(1);                     /* purecov: inspected */
        }
        if (!--max_rows)
        {
          error= 0;                             /* purecov: inspected */
          goto end;                             /* purecov: inspected */
        }
      }

      merge_chunk->advance_current_key(rec_length);
      merge_chunk->decrement_mem_count();
      if (0 == merge_chunk->mem_count())
      {
        if (!(error= (int) read_to_buffer(from_file, merge_chunk, param)))
        {
          queue.pop();
          reuse_freed_buff(merge_chunk, &queue);
          break;                        /* One buffer have been removed */
        }
        else if (error == -1)
          DBUG_RETURN(error);                 /* purecov: inspected */
      }
      /*
        The Merge_chunk at the queue's top had one of its keys consumed, thus
        it may now rank differently in the comparison order of the queue, so:
      */
      queue.update_top();
    }
  }
  merge_chunk= queue.top();
  merge_chunk->set_buffer(sort_buffer.array(),
                          sort_buffer.array() + sort_buffer.size());
  merge_chunk->set_max_keys(param->max_rows_per_buffer);

  do
  {
    if (merge_chunk->mem_count() > max_rows)
    {
      merge_chunk->set_mem_count(max_rows); /* Don't write too many records */
      merge_chunk->set_rowcount(0);         /* Don't read more */
    }
    max_rows-= merge_chunk->mem_count();

    for (uint ix= 0; ix < merge_chunk->mem_count(); ++ix)
    {
      param->get_rec_and_res_len(merge_chunk->current_key(),
                                 &rec_length, &res_length);
      const uint bytes_to_write= (flag == 0) ? rec_length : res_length;
 
      if (flag && param->using_varlen_keys())
        offset= rec_length - res_length;

      if (my_b_write(to_file,
                     merge_chunk->current_key() + offset,
                     bytes_to_write))
      {
        DBUG_RETURN(1);                       /* purecov: inspected */
      }
      merge_chunk->advance_current_key(rec_length);
    }
  }
  while ((error=(int) read_to_buffer(from_file, merge_chunk, param))
         != -1 && error != 0);

end:
  last_chunk->set_rowcount(min(org_max_rows-max_rows, param->max_rows));
  last_chunk->set_file_position(to_start_filepos);

  DBUG_RETURN(error);
} /* merge_buffers */


	/* Do a merge to output-file (save only positions) */

static int merge_index(THD *thd, Sort_param *param, Sort_buffer sort_buffer,
                       Merge_chunk_array chunk_array,
                       IO_CACHE *tempfile, IO_CACHE *outfile)
{
  DBUG_ENTER("merge_index");
  if (merge_buffers(thd,
                    param,                    // param
                    tempfile,                 // from_file
                    outfile,                  // to_file
                    sort_buffer,              // sort_buffer
                    chunk_array.begin(),      // last_chunk [out]
                    chunk_array,
                    1))                       // flag
    DBUG_RETURN(1);                           /* purecov: inspected */
  DBUG_RETURN(0);
} /* merge_index */


static uint suffix_length(ulong string_length)
{
  if (string_length < 256)
    return 1;
  if (string_length < 256L*256L)
    return 2;
  if (string_length < 256L*256L*256L)
    return 3;
  return 4;                                     // Can't sort longer than 4G
}



/**
  Calculate length of sort key.

  @param thd			  Thread handler
  @param sortorder		  Order of items to sort
  @param s_length	          Number of items to sort
  @param[out] multi_byte_charset Set to 1 if we are using multi-byte charset
                                 (In which case we have to use strnxfrm())

  @note
    sortorder->length is updated for each sort item.
  @n
    sortorder->need_strnxfrm is set 1 if we have to use strnxfrm

  @return
    Total length of sort buffer in bytes
*/

uint
sortlength(THD *thd, st_sort_field *sortorder, uint s_length,
           bool *multi_byte_charset)
{
  uint total_length= 0;
  *multi_byte_charset= false;

  // Heed the contract that strnxfrm() needs an even number of bytes.
  const uint max_sort_length_even=
    (thd->variables.max_sort_length + 1) & ~1;

  for (; s_length-- ; sortorder++)
  {
    DBUG_ASSERT(!sortorder->need_strnxfrm);
    DBUG_ASSERT(sortorder->suffix_length == 0);
    if (sortorder->field)
    {
      const Field *field= sortorder->field;
      const CHARSET_INFO *cs= field->sort_charset();
      sortorder->length= field->sort_length();
      sortorder->is_varlen= field->sort_key_is_varlen();

      if (use_strnxfrm(cs))
      {
        // How many bytes do we need (including sort weights) for strnxfrm()?
        sortorder->length= cs->coll->strnxfrmlen(cs, sortorder->length);
        sortorder->need_strnxfrm= true;
        *multi_byte_charset= 1;
      }
      /*
        NOTE: The corresponding test below also has a check for
        cs == &my_charset_bin to sort truncated blobs deterministically;
        however, that part is dealt by in Field_blob/Field_varstring,
        so we don't need it here.
      */
      if (sortorder->is_varlen)
        sortorder->length+= VARLEN_PREFIX;
      sortorder->maybe_null= field->maybe_null();
      if (field->result_type() == STRING_RESULT &&
          !field->is_temporal())
      {
        set_if_smaller(sortorder->length, max_sort_length_even);
      }

      sortorder->field_type= field->type();
    }
    else
    {
      const Item *item= sortorder->item;
      sortorder->result_type= item->result_type();
      sortorder->field_type= item->data_type();
      if (sortorder->field_type == MYSQL_TYPE_JSON)
        sortorder->is_varlen= true;
      else
        sortorder->is_varlen= false;
      if (item->is_temporal())
        sortorder->result_type= INT_RESULT;
      switch (sortorder->result_type) {
      case STRING_RESULT: {
        const CHARSET_INFO *cs= item->collation.collation;
	sortorder->length= item->max_length;
        set_if_smaller(sortorder->length, max_sort_length_even);
	if (use_strnxfrm(cs))
	{ 
          // How many bytes do we need (including sort weights) for strnxfrm()?
          sortorder->length= cs->coll->strnxfrmlen(cs, sortorder->length);
	  sortorder->need_strnxfrm= true;
	  *multi_byte_charset= 1;
	}
        else if (cs->pad_attribute == NO_PAD)
        {
          /* Store length last to be able to sort blob/varbinary */
          sortorder->suffix_length= suffix_length(sortorder->length);
          sortorder->length+= sortorder->suffix_length;
        }
	break;
      }
      case INT_RESULT:
#if SIZEOF_LONG_LONG > 4
	sortorder->length=8;			// Size of intern longlong
#else
	sortorder->length=4;
#endif
	break;
      case DECIMAL_RESULT:
        sortorder->length=
          my_decimal_get_binary_size(item->max_length -
                                     (item->decimals ? 1 : 0),
                                     item->decimals);
        break;
      case REAL_RESULT:
	sortorder->length=sizeof(double);
	break;
      case ROW_RESULT:
      default: 
	// This case should never be choosen
	DBUG_ASSERT(0);
	break;
      }
      sortorder->maybe_null= item->maybe_null;
    }
    if (sortorder->maybe_null)
      total_length++;                       // Place for NULL marker
    total_length+= sortorder->length;
  }
  sortorder->field= NULL;                       // end marker
  DBUG_PRINT("info",("sort_length: %u", total_length));
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

Addon_fields *
Filesort::get_addon_fields(ulong max_length_for_sort_data,
                           Field **ptabfield, uint sortlength,
                           Addon_fields_status *addon_fields_status,
                           uint *plength,
                           uint *ppackable_length)
{
  Field **pfield;
  Field *field;
  uint total_length= 0;
  uint packable_length= 0;
  uint num_fields= 0;
  uint null_fields= 0;
  TABLE *const table= tab->table();
  MY_BITMAP *read_set= table->read_set;

  // Locate the effective index for the table to be sorted (if any)
  const uint index= tab->effective_index();
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
  const bool filter_covering=
    index != MAX_KEY &&
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
  *plength= *ppackable_length= 0;
  *addon_fields_status= Addon_fields_status::unknown_status;

  for (pfield= ptabfield; (field= *pfield) ; pfield++)
  {
    if (!bitmap_is_set(read_set, field->field_index))
      continue;
    // part_of_key is empty for a BLOB, so apply this check before the next.
    if (field->flags & BLOB_FLAG)
    {
      DBUG_ASSERT(addon_fields == NULL);
      *addon_fields_status= Addon_fields_status::row_contains_blob;
      return NULL;
    }
    if (filter_covering && !field->part_of_key.is_set(index))
      continue;                   // See explanation above filter_covering

    const uint field_length= field->max_packed_col_length();
    total_length+= field_length;

    const enum_field_types field_type= field->type();
    if (field->maybe_null() ||
        field_type == MYSQL_TYPE_STRING ||
        field_type == MYSQL_TYPE_VARCHAR ||
        field_type == MYSQL_TYPE_VAR_STRING)
      packable_length+= field_length;
    if (field->maybe_null())
      null_fields++;
    num_fields++;
  }
  if (0 == num_fields)
    return NULL;

  total_length+= (null_fields + 7) / 8;

  *ppackable_length= packable_length;

  if (total_length + sortlength > max_length_for_sort_data)
  {
    DBUG_ASSERT(addon_fields == NULL);
    *addon_fields_status= Addon_fields_status::max_length_for_sort_data;
    return NULL;
  }

  if (addon_fields == NULL)
  {
    void *rawmem1= sql_alloc(sizeof(Addon_fields));
    void *rawmem2= sql_alloc(sizeof(Sort_addon_field) * num_fields);
    if (rawmem1 == NULL || rawmem2 == NULL)
      return NULL;                            /* purecov: inspected */
    Addon_fields_array
      addon_array(static_cast<Sort_addon_field*>(rawmem2), num_fields);
    addon_fields= new (rawmem1) Addon_fields(addon_array);
  }
  else
  {
    /*
      Allocate memory only once, reuse descriptor array and buffer.
      Set using_packed_addons here, and size/offset details below.
     */
    DBUG_ASSERT(num_fields == addon_fields->num_field_descriptors());
    addon_fields->set_using_packed_addons(false);
  }

  *plength= total_length;

  uint length= (null_fields + 7) / 8;
  null_fields= 0;
  Addon_fields_array::iterator addonf= addon_fields->begin();
  for (pfield= ptabfield; (field= *pfield) ; pfield++)
  {
    if (!bitmap_is_set(read_set, field->field_index))
      continue;
    if (filter_covering && !field->part_of_key.is_set(index))
      continue;
    DBUG_ASSERT(addonf != addon_fields->end());

    addonf->field= field;
    addonf->offset= length;
    if (field->maybe_null())
    {
      addonf->null_offset= null_fields / 8;
      addonf->null_bit= 1 << (null_fields & 7);
      null_fields++;
    }
    else
    {
      addonf->null_offset= 0;
      addonf->null_bit= 0;
    }
    addonf->max_length= field->max_packed_col_length();
    DBUG_PRINT("info", ("addon_field %s max_length %u",
                        addonf->field->field_name, addonf->max_length));

    length+= addonf->max_length;
    addonf++;
  }

  DBUG_PRINT("info",("addon_length: %d",length));
  return addon_fields;
}


/*
** functions to change a double or float to a sortable string
** The following should work for IEEE
*/

void change_double_for_sort(double nr, uchar *to)
{
  /*
    -0.0 and +0.0 compare identically, so make sure they use exactly the same
    bit pattern.
  */
  if (nr == 0.0) nr= 0.0;

  /*
    Positive doubles sort exactly as ints; negative doubles need
    bit flipping. The bit flipping sets the upper bit to 0
    unconditionally, so put 1 in there for positive numbers
    (so they sort later for our unsigned comparison).
    NOTE: This does not sort infinities or NaN correctly.
  */
  int64 nr_int;
  memcpy(&nr_int, &nr, sizeof(nr));
  nr_int= (nr_int ^ (nr_int >> 63)) | ((~nr_int) & 0x8000000000000000ULL);

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
