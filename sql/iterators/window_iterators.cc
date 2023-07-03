/* Copyright (c) 2000, 2022, Oracle and/or its affiliates.

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

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <algorithm>
#include <atomic>
#include <cinttypes>
#include <cstdint>
#include <limits>
#include <map>
#include <utility>
#include <vector>

#include "my_alloc.h"
#include "my_bitmap.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "sql/field.h"
#include "sql/handler.h"
#include "sql/item.h"
#include "sql/iterators/row_iterator.h"
#include "sql/iterators/window_iterators.h"
#include "sql/mem_root_array.h"
#include "sql/mysqld.h"            // stage_executing
#include "sql/parse_tree_nodes.h"  // PT_frame
#include "sql/sql_base.h"
#include "sql/sql_class.h"
#include "sql/sql_const.h"
#include "sql/sql_executor.h"
#include "sql/sql_optimizer.h"
#include "sql/sql_tmp_table.h"
#include "sql/table.h"
#include "sql/temp_table_param.h"
#include "sql/window.h"
#include "sql/window_lex.h"

using std::min;

namespace {

void SwitchSlice(JOIN *join, int slice_num) {
  if (slice_num != -1 && !join->ref_items[slice_num].is_null()) {
    join->set_ref_item_slice(slice_num);
  }
}

/**
  Minion for reset_framing_wf_states and reset_non_framing_wf_state, q.v.

  @param func_ptr     the set of functions
  @param framing      true if we want to reset for framing window functions
*/
inline void reset_wf_states(Func_ptr_array *func_ptr, bool framing) {
  for (Func_ptr it : *func_ptr) {
    (void)it.func()->walk(&Item::reset_wf_state, enum_walk::POSTFIX,
                          (uchar *)&framing);
  }
}
/**
  Walk the function calls and reset any framing window function's window state.

  @param func_ptr   an array of function call items which might represent
                    or contain window function calls
*/
inline void reset_framing_wf_states(Func_ptr_array *func_ptr) {
  reset_wf_states(func_ptr, true);
}

/**
  Walk the function calls and reset any non-framing window function's window
  state.

  @param func_ptr   an array of function call items which might represent
                    or contain window function calls
 */
inline void reset_non_framing_wf_state(Func_ptr_array *func_ptr) {
  reset_wf_states(func_ptr, false);
}

/**
  Save a window frame buffer to frame buffer temporary table.

  @param thd      The current thread
  @param w        The current window
  @param rowno    The rowno in the current partition (1-based)
*/
bool buffer_record_somewhere(THD *thd, Window *w, int64 rowno) {
  DBUG_TRACE;
  TABLE *const t = w->frame_buffer();
  uchar *record = t->record[0];

  assert(rowno != Window::FBC_FIRST_IN_NEXT_PARTITION);
  assert(t->is_created());

  if (!t->file->inited) {
    /*
      On the frame buffer table, t->file, we do several things in the
      windowing code:
      - read a row by position,
      - read rows after that row,
      - write a row,
      - find the position of a just-written row, if it's first in partition.
      To prepare for reads, we initialize a scan once for all with
      ha_rnd_init(), with argument=true as we'll use ha_rnd_next().
      To read a row, we use ha_rnd_pos() or ha_rnd_next().
      To write, we use ha_write_row().
      To find the position of a just-written row, we are in the following
      conditions:
      - the written row is first of its partition
      - before writing it, we have processed the previous partition, and that
      process ended with a read of the previous partition's last row
      - so, before the write, the read cursor is already positioned on that
      last row.
      Then we do the write; the new row goes after the last row; then
      ha_rnd_next() reads the row after the last row, i.e. reads the written
      row. Then position() gives the position of the written row.
    */
    int rc = t->file->ha_rnd_init(true);
    if (rc != 0) {
      t->file->print_error(rc, MYF(0));
      return true;
    }
  }

  int error = t->file->ha_write_row(record);
  w->set_frame_buffer_total_rows(w->frame_buffer_total_rows() + 1);

  constexpr size_t first_in_partition = static_cast<size_t>(
      Window_retrieve_cached_row_reason::FIRST_IN_PARTITION);

  if (error) {
    /* If this is a duplicate error, return immediately */
    if (t->file->is_ignorable_error(error)) return true;

    /* Other error than duplicate error: Attempt to create a temporary table. */
    bool is_duplicate;
    if (create_ondisk_from_heap(thd, t, error, /*insert_last_record=*/true,
                                /*ignore_last_dup=*/true, &is_duplicate))
      return true;

    assert(t->s->db_type() == innodb_hton);
    if (t->file->ha_rnd_init(true)) return true; /* purecov: inspected */

    if (!w->m_frame_buffer_positions.empty()) {
      /*
        Reset all hints since they all pertain to the in-memory file, not the
        new on-disk one.
      */
      for (size_t i = first_in_partition;
           i < Window::FRAME_BUFFER_POSITIONS_CARD +
                   w->opt_nth_row().m_offsets.size() +
                   w->opt_lead_lag().m_offsets.size();
           i++) {
        void *r = (*THR_MALLOC)->Alloc(t->file->ref_length);
        if (r == nullptr) return true;
        w->m_frame_buffer_positions[i].m_position = static_cast<uchar *>(r);
        w->m_frame_buffer_positions[i].m_rowno = -1;
      }

      if ((w->m_tmp_pos.m_position =
               (uchar *)(*THR_MALLOC)->Alloc(t->file->ref_length)) == nullptr)
        return true;

      w->m_frame_buffer_positions[first_in_partition].m_rowno = 1;
      /* Update the partition offset if we are starting a new partition */
      if (rowno == 1)
        w->set_frame_buffer_partition_offset(w->frame_buffer_total_rows());
      /*
        The auto-generated primary key of the first row is 1. Our offset is
        also one-based, so we can use w->frame_buffer_partition_offset() "as is"
        to construct the position.
      */
      encode_innodb_position(
          w->m_frame_buffer_positions[first_in_partition].m_position,
          t->file->ref_length, w->frame_buffer_partition_offset());

      return is_duplicate ? true : false;
    }
  }
  /* Save position in frame buffer file of first row in a partition */
  if (rowno == 1) {
    if (w->m_frame_buffer_positions.empty()) {
      w->m_frame_buffer_positions.init(thd->mem_root);
      /* lazy initialization of positions remembered */
      for (uint i = 0; i < Window::FRAME_BUFFER_POSITIONS_CARD +
                               w->opt_nth_row().m_offsets.size() +
                               w->opt_lead_lag().m_offsets.size();
           i++) {
        void *r = (*THR_MALLOC)->Alloc(t->file->ref_length);
        if (r == nullptr) return true;
        Window::Frame_buffer_position p(static_cast<uchar *>(r), -1);
        w->m_frame_buffer_positions.push_back(p);
      }

      if ((w->m_tmp_pos.m_position =
               (uchar *)(*THR_MALLOC)->Alloc(t->file->ref_length)) == nullptr)
        return true;
    }

    // Do a read to establish scan position, then get it
    error = t->file->ha_rnd_next(record);
    t->file->position(record);
    std::memcpy(w->m_frame_buffer_positions[first_in_partition].m_position,
                t->file->ref, t->file->ref_length);
    w->m_frame_buffer_positions[first_in_partition].m_rowno = 1;
    w->set_frame_buffer_partition_offset(w->frame_buffer_total_rows());
  }

  return false;
}

/**
  If we cannot evaluate all window functions for a window on the fly, buffer the
  current row for later processing by process_buffered_windowing_record.

  @param thd                Current thread
  @param param              The temporary table parameter

  @param[in,out] new_partition If input is not nullptr:
                            sets the bool pointed to to true if a new partition
                            was found and there was a previous partition; if
                            so the buffering of the first row in new
                            partition isn't done and must be repeated
                            later: we save away the row as rowno
                            FBC_FIRST_IN_NEXT_PARTITION, then fetch it back
                            later, cf. end_write_wf.
                            If input is nullptr, this is the "later" call to
                            buffer the first row of the new partition:
                            buffer the row.
  @return true if error.
*/
bool buffer_windowing_record(THD *thd, Temp_table_param *param,
                             bool *new_partition) {
  DBUG_TRACE;
  Window *w = param->m_window;

  if (copy_fields(w->frame_buffer_param(), thd)) return true;

  if (new_partition != nullptr) {
    const bool first_partition = w->partition_rowno() == 0;
    w->check_partition_boundary();

    if (!first_partition && w->partition_rowno() == 1) {
      *new_partition = true;
      w->save_special_row(Window::FBC_FIRST_IN_NEXT_PARTITION,
                          w->frame_buffer());
      return false;
    }
  }

  if (buffer_record_somewhere(thd, w, w->partition_rowno())) return true;

  w->set_last_rowno_in_cache(w->partition_rowno());

  return false;
}

/**
  Read row rowno from frame buffer tmp file using cached row positions to
  minimize positioning work.
*/
bool read_frame_buffer_row(int64 rowno, Window *w,
#ifndef NDEBUG
                           bool for_nth_value)
#else
                           bool for_nth_value [[maybe_unused]])
#endif
{
  int use_idx = 0;  // closest prior position found, a priori 0 (row 1)
  int diff = w->last_rowno_in_cache();  // maximum a priori
  TABLE *t = w->frame_buffer();

  // Find the saved position closest to where we want to go
  for (int i = w->m_frame_buffer_positions.size() - 1; i >= 0; i--) {
    Window::Frame_buffer_position cand = w->m_frame_buffer_positions[i];
    if (cand.m_rowno == -1 || cand.m_rowno > rowno) continue;

    if (rowno - cand.m_rowno < diff) {
      /* closest so far */
      diff = rowno - cand.m_rowno;
      use_idx = i;
    }
  }

  Window::Frame_buffer_position *cand = &w->m_frame_buffer_positions[use_idx];

  int error =
      t->file->ha_rnd_pos(w->frame_buffer()->record[0], cand->m_position);
  if (error) {
    t->file->print_error(error, MYF(0));
    return true;
  }

  if (rowno > cand->m_rowno) {
    /*
      The saved position didn't correspond exactly to where we want to go, but
      is located one or more rows further out on the file, so read next to move
      forward to desired row.
    */
    const int64 cnt = rowno - cand->m_rowno;

    /*
      We should have enough location hints to normally need only one extra read.
      If we have just switched to INNODB due to MEM overflow, a rescan is
      required, so skip assert if we have INNODB.
    */
    assert(w->frame_buffer()->s->db_type()->db_type == DB_TYPE_INNODB ||
           cnt <= 1 ||
           // unless we have a frame beyond the current row, 1. time
           // in which case we need to do some scanning...
           (w->last_row_output() == 0 &&
            w->frame()->m_from->m_border_type == WBT_VALUE_FOLLOWING) ||
           // or unless we are search for NTH_VALUE, which can be in the
           // middle of a frame, and with RANGE frames it can jump many
           // positions from one frame to the next with optimized eval
           // strategy
           for_nth_value);

    for (int i = 0; i < cnt; i++) {
      error = t->file->ha_rnd_next(t->record[0]);
      if (error) {
        t->file->print_error(error, MYF(0));
        return true;
      }
    }
  }

  return false;
}

#if !defined(NDEBUG)
inline void dbug_allow_write_all_columns(
    Temp_table_param *param, std::map<TABLE *, my_bitmap_map *> &map) {
  for (Copy_field &copy_field : param->copy_fields) {
    TABLE *const t = copy_field.from_field()->table;
    if (t != nullptr) {
      auto it = map.find(t);
      if (it == map.end())
        map.insert(it, std::pair<TABLE *, my_bitmap_map *>(
                           t, dbug_tmp_use_all_columns(t, t->write_set)));
    }
  }
}

inline void dbug_restore_all_columns(std::map<TABLE *, my_bitmap_map *> &map) {
  auto func = [](std::pair<TABLE *const, my_bitmap_map *> &e) {
    dbug_tmp_restore_column_map(e.first->write_set, e.second);
  };

  std::for_each(map.begin(), map.end(), func);
}
#endif

/**
  Bring back buffered data to the record of qep_tab-1 [1], and optionally
  execute copy_funcs() to the OUT table.

  [1] This is not always the case. For the first window, if we have no
  PARTITION BY or ORDER BY in the window, and there is more than one table
  in the join, the logical input can consist of more than one table
  (qep_tab-1 .. qep_tab-n), so the record accordingly.

  This method works by temporarily reversing the "normal" direction of the field
  copying.

  Also make a note of the position of the record we retrieved in the window's
  m_frame_buffer_positions to be able to optimize succeeding retrievals.

  @param thd       The current thread
  @param w         The current window
  @param rowno     The row number (in the partition) to set up
  @param reason    What kind of row to retrieve
  @param fno       Used with NTH_VALUE and LEAD/LAG to specify which
                   window function's position cache to use, i.e. what index
                   of m_frame_buffer_positions to update. For the second
                   LEAD/LAG window function in a query, the index would be
                   REA_MISC_POSITIONS (reason) + \<no of NTH functions\> + 2.

  @return true on error
*/
bool bring_back_frame_row(THD *thd, Window *w, int64 rowno,
                          Window_retrieve_cached_row_reason reason,
                          int fno = 0) {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("rowno: %" PRId64 " reason: %d fno: %d", rowno,
                       static_cast<int>(reason), fno));
  assert(reason == Window_retrieve_cached_row_reason::MISC_POSITIONS ||
         fno == 0);
  w->set_rowno_being_visited(rowno);
  uchar *fb_rec = w->frame_buffer()->record[0];

  assert(rowno != 0);

  /*
    If requested row is the last we fetched from FB and copied to OUT, we
    don't need to fetch and copy again.
    Because "reason", "fno" may differ from the last call which fetched the
    row, we still do the updates of w.m_frame_buffer_positions even if
    do_fetch=false.
  */
  bool do_fetch;

  if (rowno == Window::FBC_FIRST_IN_NEXT_PARTITION) {
    do_fetch = true;
    w->restore_special_row(rowno, fb_rec);
  } else {
    assert(reason != Window_retrieve_cached_row_reason::WONT_UPDATE_HINT);
    do_fetch = w->row_has_fields_in_out_table() != rowno;

    if (do_fetch &&
        read_frame_buffer_row(
            rowno, w,
            reason == Window_retrieve_cached_row_reason::MISC_POSITIONS))
      return true;

    /* Got row rowno in record[0], remember position */
    const TABLE *const t = w->frame_buffer();
    t->file->position(fb_rec);
    std::memcpy(
        w->m_frame_buffer_positions[static_cast<int>(reason) + fno].m_position,
        t->file->ref, t->file->ref_length);
    w->m_frame_buffer_positions[static_cast<int>(reason) + fno].m_rowno = rowno;
  }

  if (!do_fetch) return false;

  Temp_table_param *const fb_info = w->frame_buffer_param();

#if !defined(NDEBUG)
  /*
    Since we are copying back a row from the frame buffer to the output table's
    buffer, we will be copying into fields that are not necessarily marked as
    writeable. To eliminate problems with ASSERT_COLUMN_MARKED_FOR_WRITE, we
    set all fields writeable. This is only
    applicable in debug builds, since ASSERT_COLUMN_MARKED_FOR_WRITE is debug
    only.
  */
  std::map<TABLE *, my_bitmap_map *> saved_map;
  dbug_allow_write_all_columns(fb_info, saved_map);
#endif

  /*
    Do the inverse of copy_fields to get the row's fields back to the input
    table from the frame buffer.
  */
  bool rc = copy_fields(fb_info, thd, true);

#if !defined(NDEBUG)
  dbug_restore_all_columns(saved_map);
#endif

  if (!rc) {
    // fields are in OUT
    if (rowno >= 1) w->set_row_has_fields_in_out_table(rowno);
  }
  return rc;
}

}  // namespace

/**
  Save row special_rowno in table t->record[0] to an in-memory copy for later
  restoration.
*/
void Window::save_special_row(uint64 special_rowno, TABLE *t) {
  DBUG_PRINT("info", ("save_special_row: %" PRIu64, special_rowno));
  size_t l = t->s->reclength;
  assert(m_special_rows_cache_max_length >= l);  // check room.
  // From negative enum, get proper array index:
  int idx = FBC_FIRST_KEY - special_rowno;
  m_special_rows_cache_length[idx] = l;
  std::memcpy(m_special_rows_cache + idx * m_special_rows_cache_max_length,
              t->record[0], l);
}

/**
  Restore row special_rowno into record from in-memory copy. Any fields not
  the result of window functions are not used, but they do tag along here
  (unnecessary copying..). BLOBs: have storage in result_field of Item
  for the window function although the pointer is copied here. The
  result field storage is stable across reads from the frame buffer, so safe.
*/
void Window::restore_special_row(uint64 special_rowno, uchar *record) {
  DBUG_PRINT("info", ("restore_special_row: %" PRIu64, special_rowno));
  int idx = FBC_FIRST_KEY - special_rowno;
  size_t l = m_special_rows_cache_length[idx];
  std::memcpy(record,
              m_special_rows_cache + idx * m_special_rows_cache_max_length, l);
  // Sometimes, "record" points to IN record
  set_row_has_fields_in_out_table(0);
}

namespace {

/**
  Process window functions that need partition cardinality
*/
bool process_wfs_needing_partition_cardinality(
    THD *thd, Temp_table_param *param, const Window::st_nth &have_nth_value,
    const Window::st_lead_lag &have_lead_lag, const int64 current_row,
    Window *w, Window_retrieve_cached_row_reason current_row_reason) {
  // Reset state for LEAD/LAG functions
  if (!have_lead_lag.m_offsets.empty()) w->reset_lead_lag();

  // This also handles LEAD(.., 0)
  if (copy_funcs(param, thd, CFT_WF_NEEDS_PARTITION_CARDINALITY)) return true;

  if (!have_lead_lag.m_offsets.empty()) {
    int fno = 0;
    const int nths = have_nth_value.m_offsets.size();

    for (const Window::st_ll_offset &ll : have_lead_lag.m_offsets) {
      const int64 rowno_to_visit = current_row - ll.m_rowno;

      if (rowno_to_visit == current_row)
        continue;  // Already processed above above

      /*
        Note that this value can be outside partition, even negative: if so,
        the default will applied, if any is provided.
      */
      w->set_rowno_being_visited(rowno_to_visit);

      if (rowno_to_visit >= 1 && rowno_to_visit <= w->last_rowno_in_cache()) {
        if (bring_back_frame_row(
                thd, w, rowno_to_visit,
                Window_retrieve_cached_row_reason::MISC_POSITIONS,
                nths + fno++))
          return true;
      }

      if (copy_funcs(param, thd, CFT_WF_NEEDS_PARTITION_CARDINALITY))
        return true;
    }
    /* Bring back the fields for the output row */
    if (bring_back_frame_row(thd, w, current_row, current_row_reason))
      return true;
  }

  return false;
}

/**
  While there are more unprocessed rows ready to process given the current
  partition/frame state, process such buffered rows by evaluating/aggregating
  the window functions defined over this window on the current frame, moving
  the frame if required.

  This method contains the main execution time logic of the evaluation
  window functions if we need buffering for one or more of the window functions
  defined on the window.

  Moving (sliding) frames can be executed using a naive or optimized strategy
  for aggregate window functions, like SUM or AVG (but not MAX, or MIN).
  In the naive approach, for each row considered for processing from the buffer,
  we visit all the rows defined in the frame for that row, essentially leading
  to N*M complexity, where N is the number of rows in the result set, and M is
  the number for rows in the frame. This can be slow for large frames,
  obviously, so we can choose an optimized evaluation strategy using inversion.
  This means that when rows leave the frame as we move it forward, we re-use
  the previous aggregate state, but compute the *inverse* function to eliminate
  the contribution to the aggregate by the row(s) leaving the frame, and then
  use the normal aggregate function to add the contribution of the rows moving
  into the frame. The present method contains code paths for both strategies.

  For integral data types, this is safe in the sense that the result will be the
  same if no overflow occurs during normal evaluation. For floating numbers,
  optimizing in this way may lead to different results, so it is not done by
  default, cf the session variable "windowing_use_high_precision".

  Since the evaluation strategy is chosen based on the "most difficult" window
  function defined on the window, we must also be able to evaluate
  non-aggregates like ROW_NUMBER, NTILE, FIRST_VALUE in the code path of the
  optimized aggregates, so there is redundant code for those in the naive and
  optimized code paths. Note that NTILE forms a class of its own of the
  non-aggregates: it needs two passes over the partition's rows since the
  cardinality is needed to compute it. Furthermore, FIRST_VALUE and LAST_VALUE
  heed the frames, but they are not aggregates.

  The is a special optimized code path for *static aggregates*: when the window
  frame is the default, e.g. the entire partition and there is no ORDER BY
  specified, the value of the framing window functions, i.e. SUM, AVG,
  FIRST_VALUE, LAST_VALUE can be evaluated once and for all and saved when
  we visit and evaluate the first row of the partition. For later rows we
  restore the aggregate values and just fill in the other fields and evaluate
  non-framing window functions for the row.

  The code paths both for naive execution and optimized execution differ
  depending on whether we have ROW or RANGE boundaries in a explicit frame.

  A word on BLOBs. Below we make copies of rows into the frame buffer.
  This is a temporary table, so BLOBs get copied in the normal way.

  Sometimes we save records containing already computed framing window
  functions away into memory only: is the lifetime of the referenced BLOBs long
  enough? We have two cases:

  BLOB results from wfs: Any BLOB results will reside in the copies in result
  fields of the Items ready for the out file, so they no longer need any BLOB
  memory read from the frame buffer tmp file.

  BLOB fields not evaluated by wfs: Any other BLOB field will be copied as
  well, and would not have life-time past the next read from the frame buffer,
  but they are never used since we fill in the fields from the current row
  after evaluation of the window functions, so we don't need to make special
  copies of such BLOBs. This can be (and was) tested by shredding any BLOBs
  deallocated by InnoDB at the next read.

  We also save away in memory the next record of the next partition while
  processing the current partition. Any blob there will have its storage from
  the read of the input file, but we won't be touching that for reading again
  until after we start processing the next partition and save the saved away
  next partition row to the frame buffer.

  Note that the logic of this function is centered around the window, not
  around the window function. It is about putting rows in a partition,
  in a frame, in a set of peers, and passing this information to all window
  functions attached to this window; each function looks at the partition,
  frame, or peer set in its own particular way (for example RANK looks at the
  partition, SUM looks at the frame).

  @param thd                    Current thread
  @param param                  Current temporary table
  @param new_partition_or_eof   True if (we are about to start a new partition
                                and there was a previous partition) or eof
  @param[out] output_row_ready  True if there is a row record ready to write
                                to the out table

  @return true if error
*/
bool process_buffered_windowing_record(THD *thd, Temp_table_param *param,
                                       const bool new_partition_or_eof,
                                       bool *output_row_ready) {
  DBUG_TRACE;
  /**
    The current window
  */
  Window &w = *param->m_window;

  /**
    The frame
  */
  const PT_frame *f = w.frame();

  *output_row_ready = false;

  /**
    This is the row we are currently considering for processing and getting
    ready for output, cf. output_row_ready.
  */
  const int64 current_row = w.last_row_output() + 1;

  /**
    This is the row number of the last row we have buffered so far.
  */
  const int64 last_rowno_in_cache = w.last_rowno_in_cache();

  if (current_row > last_rowno_in_cache)  // already sent all buffered rows
    return false;

  /**
    If true, use code path for static aggregates
  */
  const bool static_aggregate = w.static_aggregates();

  /**
    If true, use code path for ROW bounds with optimized strategy
  */
  const bool row_optimizable = w.optimizable_row_aggregates();

  /**
    If true, use code path for RANGE bounds with optimized strategy
  */
  const bool range_optimizable = w.optimizable_range_aggregates();

  // These three strategies are mutually exclusive:
  assert((static_aggregate + row_optimizable + range_optimizable) <= 1);

  /**
    We need to evaluate FIRST_VALUE, or optimized MIN/MAX
  */
  const bool have_first_value = w.opt_first_row();

  /**
    We need to evaluate LAST_VALUE, or optimized MIN/MAX
  */
  const bool have_last_value = w.opt_last_row();

  /**
    We need to evaluate NTH_VALUE
  */
  const Window::st_nth &have_nth_value = w.opt_nth_row();

  /**
    We need to evaluate LEAD/LAG rows
  */

  const Window::st_lead_lag &have_lead_lag = w.opt_lead_lag();

  /**
    True if an inversion optimization strategy is used. For common
    code paths.
  */
  const bool optimizable = (row_optimizable || range_optimizable);

  /**
    RANGE was specified as the bounds unit for the frame
  */
  const bool range_frame = f->m_query_expression == WFU_RANGE;

  const bool range_to_current_row =
      range_frame && f->m_to->m_border_type == WBT_CURRENT_ROW;

  const bool range_from_first_to_current_row =
      range_to_current_row &&
      f->m_from->m_border_type == WBT_UNBOUNDED_PRECEDING;
  /**
    UNBOUNDED FOLLOWING was specified for the frame
  */
  bool unbounded_following = false;

  /**
    Row_number of the first row in the frame. Invariant: lower_limit >= 1
    after initialization.
  */
  int64 lower_limit = 1;

  /**
    Row_number of the logically last row to be computed in the frame, may be
    higher than the number of rows in the partition. The actual highest row
    number is computed later, see upper below.
  */
  int64 upper_limit = 0;

  /**
    needs peerset of current row to evaluate a wf for the current row.
  */
  bool needs_peerset = w.needs_peerset();

  /**
    needs the last peer of the current row within a frame.
  */
  const bool needs_last_peer_in_frame = w.needs_last_peer_in_frame();

  DBUG_PRINT("enter", ("current_row: %" PRId64 ", new_partition_or_eof: %d",
                       current_row, new_partition_or_eof));

  /* Compute lower_limit, upper_limit and possibly unbounded_following */
  if (f->m_query_expression == WFU_RANGE) {
    lower_limit = w.first_rowno_in_range_frame();
    /*
      For RANGE frame, we first buffer all the rows in the partition due to the
      need to find last peer before first can be processed. This can be
      optimized,
      FIXME.
    */
    upper_limit = INT64_MAX;
  } else {
    assert(f->m_query_expression == WFU_ROWS);
    bool lower_within_limits = true;
    // Determine lower border, handle wraparound for unsigned value:
    int64 border =
        f->m_from->border() != nullptr ? f->m_from->border()->val_int() : 0;
    if (border < 0) {
      border = INT64_MAX;
    }
    switch (f->m_from->m_border_type) {
      case WBT_CURRENT_ROW:
        lower_limit = current_row;
        break;
      case WBT_VALUE_PRECEDING:
        /*
          Example: 1 PRECEDING and current row== 2 => 1
                                   current row== 1 => 1
                                   current row== 3 => 2
        */
        lower_limit = std::max<int64>(current_row - border, 1);
        break;
      case WBT_VALUE_FOLLOWING:
        /*
          Example: 1 FOLLOWING and current row== 2 => 3
                                   current row== 1 => 2
                                   current row== 3 => 4
        */
        if (border <= (std::numeric_limits<int64>::max() - current_row))
          lower_limit = current_row + border;
        else {
          lower_within_limits = false;
          lower_limit = INT64_MAX;
        }
        break;
      case WBT_UNBOUNDED_PRECEDING:
        lower_limit = 1;
        break;
      case WBT_UNBOUNDED_FOLLOWING:
        assert(false);
        break;
    }

    // Determine upper border, handle wraparound for unsigned value:
    border = f->m_to->border() != nullptr ? f->m_to->border()->val_int() : 0;
    if (border < 0) {
      border = INT64_MAX;
    }
    {
      switch (f->m_to->m_border_type) {
        case WBT_CURRENT_ROW:
          // we always have enough cache
          upper_limit = current_row;
          break;
        case WBT_VALUE_PRECEDING:
          upper_limit = current_row - border;
          break;
        case WBT_VALUE_FOLLOWING:
          if (border <= (std::numeric_limits<longlong>::max() - current_row))
            upper_limit = current_row + border;
          else {
            upper_limit = INT64_MAX;
            /*
              If both the border specifications are beyond numeric limits,
              the window frame is empty.
            */
            if (f->m_from->m_border_type == WBT_VALUE_FOLLOWING &&
                !lower_within_limits) {
              lower_limit = INT64_MAX;
              upper_limit = INT64_MAX - 1;
            }
          }
          break;
        case WBT_UNBOUNDED_FOLLOWING:
          unbounded_following = true;
          upper_limit = INT64_MAX;  // need whole partition
          break;
        case WBT_UNBOUNDED_PRECEDING:
          assert(false);
          break;
      }
    }
  }

  /*
    Determine if, given our current read and buffering state, we have enough
    buffered rows to compute an output row.

    Example: ROWS BETWEEN 1 PRECEDING and 3 FOLLOWING

    State:
    +---+-------------------------------+
    |   | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
    +---+-------------------------------+
    ^    1?         ^
    lower      last_rowno_in_cache
    (0)             (4)

    This state means:

    We have read 4 rows, cf. value of last_rowno_in_cache.
    We can now process row 1 since both lower (1-1=0) and upper (1+3=4) are less
    than or equal to 4, the last row in the cache so far.

    We can not process row 2 since: !(4 >= 2 + 3) and we haven't seen the last
    row in partition which means that the frame may not be full yet.

    If we have a window function that needs to know the partition cardinality,
    we also must buffer all records of the partition before processing.
  */

  if (!((lower_limit <= last_rowno_in_cache &&
         upper_limit <= last_rowno_in_cache &&
         !w.needs_partition_cardinality()) || /* we have cached enough rows */
        new_partition_or_eof /* we have cached all rows */))
    return false;  // We haven't read enough rows yet, so return

  w.set_rowno_in_partition(current_row);

  /*
    By default, we must:
    - if we are the first row of a partition, reset values for both
    non-framing and framing WFs
    - reset values for framing WFs (new current row = new frame = new
    values for WFs).

    Both resettings require restoring the row from the FB. And, as we have
    restored this row, we use this opportunity to compute non-framing
    does-not-need-partition-cardinality functions.

    The meaning of if statements below is that in some cases, we can avoid
    this default behaviour.

    For example, if we have static framing WFs, and this is not the
    partition's first row: the previous row's framing-WF values should be
    reused without change, so all the above resetting must be skipped;
    so row restoration isn't immediately needed; that and the computation of
    non-framing functions is then done in another later block of code.
    Likewise, if we have framing WFs with inversion, and it's not the
    first row of the partition, we must skip the resetting of framing WFs.
  */
  if (!static_aggregate || current_row == 1) {
    /*
      We need to reset functions. As part of it, their comparators need to
      update themselves to use the new row as base line. So, restore it:
    */
    if (bring_back_frame_row(thd, &w, current_row,
                             Window_retrieve_cached_row_reason::CURRENT))
      return true;

    if (current_row == 1)  // new partition
      reset_non_framing_wf_state(param->items_to_copy);
    if (!optimizable || current_row == 1)  // new frame
    {
      reset_framing_wf_states(param->items_to_copy);
    }  // else we remember state and update it for row 2..N

    /* E.g. ROW_NUMBER, RANK, DENSE_RANK */
    if (copy_funcs(param, thd, CFT_WF_NON_FRAMING)) return true;
    if (!optimizable || current_row == 1) {
      /*
        So far frame is empty; set up a flag which makes framing WFs set
        themselves to NULL in OUT.
      */
      w.set_do_copy_null(true);
      if (copy_funcs(param, thd, CFT_WF_FRAMING)) return true;
      w.set_do_copy_null(false);
    }  // else aggregates keep value of previous row, and we'll do inversion
  }

  if (range_frame) {
    /* establish current row as base-line for RANGE computation */
    w.reset_order_by_peer_set();
  }

  bool first_row_in_range_frame_seen = false;

  /**
    For optimized strategy we want to save away the previous aggregate result
    and reuse in later round by inversion. This keeps track of whether we
    managed to compute results for this current row (result are "primed"), so we
    can use inversion in later rows. Cf Window::m_aggregates_primed.
  */
  bool optimizable_primed = false;

  /**
    Possible adjustment of the logical upper_limit: no rows exist beyond
    last_rowno_in_cache.
  */
  const int64 upper = min(upper_limit, last_rowno_in_cache);

  /*
    Optimization: we evaluate the peer set of the current row potentially
    several times. Window functions like CUME_DIST sets needs_peerset and is
    evaluated last, so if any other wf evaluation led to finding the peer set
    of the current row, make a note of it, so we can skip doing it twice.
  */
  bool have_peers_current_row = false;

  if ((static_aggregate && current_row == 1) ||   // skip for row > 1
      (optimizable && !w.aggregates_primed()) ||  // skip for 2..N in frame
      (!static_aggregate && !optimizable))        // normal: no skip
  {
    // Compute and output current_row.
    int64 rowno;        ///< iterates over rows in a frame
    int64 skipped = 0;  ///< RANGE: # of visited rows seen before the frame

    for (rowno = lower_limit; rowno <= upper; rowno++) {
      if (optimizable) optimizable_primed = true;

      /*
        Set window frame state before computing framing window function.
        'n' is the number of row #rowno relative to the beginning of the
        frame, 1-based.
      */
      const int64 n = rowno - lower_limit + 1 - skipped;

      w.set_rowno_in_frame(n);

      const Window_retrieve_cached_row_reason reason =
          (n == 1 ? Window_retrieve_cached_row_reason::FIRST_IN_FRAME
                  : Window_retrieve_cached_row_reason::LAST_IN_FRAME);
      /*
        Hint maintenance: we will normally read past last row in frame, so
        prepare to resurrect that hint once we do.
      */
      w.save_pos(reason);

      /* Set up the non-wf fields for aggregating to the output row. */
      if (bring_back_frame_row(thd, &w, rowno, reason)) return true;

      if (range_frame) {
        if (w.before_frame()) {
          skipped++;
          continue;
        }
        if (w.after_frame()) {
          w.set_last_rowno_in_range_frame(rowno - 1);

          if (!first_row_in_range_frame_seen)
            // empty frame, optimize starting point for next row
            w.set_first_rowno_in_range_frame(rowno);
          w.restore_pos(reason);
          break;
        }  // else: row is within range, process

        if (!first_row_in_range_frame_seen) {
          /*
            Optimize starting point for next row: monotonic increase in frame
            bounds
          */
          first_row_in_range_frame_seen = true;
          w.set_first_rowno_in_range_frame(rowno);
        }
      }

      /*
        Compute framing WFs. For ROWS frame, "upper" is exactly the frame's
        last row; but for the case of RANGE
        we can't be sure that this is indeed the last row, but we must make a
        pessimistic assumption. If it is not the last, the final row
        calculation, if any, as for AVG, will be repeated for the next peer
        row(s).
        For optimized MIN/MAX [1], we do this to make sure we have a non-NULL
        last value (if one exists) for the initial frame.
      */
      const bool setstate =
          (rowno == upper || range_frame || have_last_value /* [1] */);
      if (setstate)
        w.set_is_last_row_in_frame(true);  // temporary state for next call

      // Accumulate frame's row into WF's value for current_row:
      if (copy_funcs(param, thd, CFT_WF_FRAMING)) return true;

      if (setstate) w.set_is_last_row_in_frame(false);  // undo temporary state
    }

    if (range_frame || rowno > upper)  // no more rows in partition
    {
      if (range_frame) {
        if (!first_row_in_range_frame_seen) {
          /*
            Empty frame: optimize starting point for next row: monotonic
            increase in frame bounds
          */
          w.set_first_rowno_in_range_frame(rowno);
        }
      }
      w.set_last_rowno_in_range_frame(rowno - 1);
      if (range_to_current_row) {
        w.set_last_rowno_in_peerset(w.last_rowno_in_range_frame());
        have_peers_current_row = true;
      }
    }  // else: we already set it before breaking out of loop
  }

  /*
    While the block above was for the default execution method, below we have
    alternative blocks for optimized methods: static framing WFs and
    inversion, when current_row isn't first; i.e. we can use the previous
    row's value of framing WFs as a base.
    In the row buffer of OUT, after the previous row was emitted, these values
    of framing WFs are still present, as no copy_funcs(CFT_WF_FRAMING) was run
    for our new row yet.
  */
  if (static_aggregate && current_row != 1) {
    /* Set up the correct non-wf fields for copying to the output row */
    if (bring_back_frame_row(thd, &w, current_row,
                             Window_retrieve_cached_row_reason::CURRENT))
      return true;

    /* E.g. ROW_NUMBER, RANK, DENSE_RANK */
    if (copy_funcs(param, thd, CFT_WF_NON_FRAMING)) return true;
  } else if (row_optimizable && w.aggregates_primed()) {
    /*
      Rows 2..N in partition: we still have state from previous current row's
      frame computation, now adjust by subtracting row 1 in frame (lower_limit)
      and adding new, if any, final frame row
    */
    const bool remove_previous_first_row =
        (lower_limit > 1 && lower_limit - 1 <= last_rowno_in_cache);
    const bool new_last_row =
        (upper_limit <= upper &&
         !unbounded_following /* all added when primed */);
    const int64 rows_in_frame = upper - lower_limit + 1;
    w.set_first_rowno_in_rows_frame(lower_limit);

    /* possibly subtract: early in partition there may not be any */
    if (remove_previous_first_row) {
      /*
        Check if the row leaving the frame is the last row in the peerset
        within a frame. If true, set is_last_row_in_peerset_within_frame
        to true.
        Used by JSON_OBJECTAGG to remove the key/value pair only
        when it is the last row having that key value.
      */
      if (needs_last_peer_in_frame) {
        int64 rowno = lower_limit - 1;
        bool is_last_row_in_peerset = true;
        if (rowno < upper) {
          if (bring_back_frame_row(
                  thd, &w, rowno,
                  Window_retrieve_cached_row_reason::LAST_IN_PEERSET))
            return true;
          // Establish current row as base-line for peer set.
          w.reset_order_by_peer_set();
          /*
            Check if the next row is a peer to this row. If not
            set current row as the last row in peerset within
            frame.
          */
          rowno++;
          if (rowno < upper) {
            if (bring_back_frame_row(
                    thd, &w, rowno,
                    Window_retrieve_cached_row_reason::LAST_IN_PEERSET))
              return true;
            // Compare only the first order by item.
            if (!w.in_new_order_by_peer_set(false))
              is_last_row_in_peerset = false;
          }
        }
        if (is_last_row_in_peerset)
          w.set_is_last_row_in_peerset_within_frame(true);
      }

      if (bring_back_frame_row(
              thd, &w, lower_limit - 1,
              Window_retrieve_cached_row_reason::FIRST_IN_FRAME))
        return true;

      w.set_inverse(true);
      if (!new_last_row) {
        w.set_rowno_in_frame(current_row - lower_limit + 1);
        if (rows_in_frame > 0)
          w.set_is_last_row_in_frame(true);  // do final comp., e.g. div in AVG

        if (copy_funcs(param, thd, CFT_WF_FRAMING)) return true;

        w.set_is_last_row_in_frame(false);  // undo temporary states
      } else {
        if (copy_funcs(param, thd, CFT_WF_FRAMING)) return true;
      }

      w.set_is_last_row_in_peerset_within_frame(false);
      w.set_inverse(false);
    }

    if (have_first_value && (lower_limit <= last_rowno_in_cache)) {
      // We have seen first row of frame, FIRST_VALUE can be computed:
      if (bring_back_frame_row(
              thd, &w, lower_limit,
              Window_retrieve_cached_row_reason::FIRST_IN_FRAME))
        return true;

      w.set_rowno_in_frame(1);

      /*
        Framing WFs which accumulate (SUM, COUNT, AVG) shouldn't accumulate
        this row again as they have done so already. Evaluate only
        X_VALUE/MIN/MAX.
      */
      if (copy_funcs(param, thd, CFT_WF_USES_ONLY_ONE_ROW)) return true;
    }

    if (have_last_value && !new_last_row) {
      // We have seen last row of frame, LAST_VALUE can be computed:
      if (bring_back_frame_row(
              thd, &w, upper, Window_retrieve_cached_row_reason::LAST_IN_FRAME))
        return true;

      w.set_rowno_in_frame(current_row - lower_limit + 1);
      if (rows_in_frame > 0) w.set_is_last_row_in_frame(true);

      if (copy_funcs(param, thd, CFT_WF_USES_ONLY_ONE_ROW)) return true;

      w.set_is_last_row_in_frame(false);
    }

    if (!have_nth_value.m_offsets.empty()) {
      int fno = 0;
      for (Window::st_offset nth : have_nth_value.m_offsets) {
        if (lower_limit + nth.m_rowno - 1 <= upper) {
          if (bring_back_frame_row(
                  thd, &w, lower_limit + nth.m_rowno - 1,
                  Window_retrieve_cached_row_reason::MISC_POSITIONS, fno++))
            return true;

          w.set_rowno_in_frame(nth.m_rowno);

          if (copy_funcs(param, thd, CFT_WF_USES_ONLY_ONE_ROW)) return true;
        }
      }
    }

    if (new_last_row)  // Add new last row to framing WF's value
    {
      if (bring_back_frame_row(
              thd, &w, upper, Window_retrieve_cached_row_reason::LAST_IN_FRAME))
        return true;

      w.set_rowno_in_frame(upper - lower_limit + 1)
          .set_is_last_row_in_frame(true);  // temporary states for next copy

      if (copy_funcs(param, thd, CFT_WF_FRAMING)) return true;

      w.set_is_last_row_in_frame(false);  // undo temporary states
    }
  } else if (range_optimizable && w.aggregates_primed()) {
    /*
      Peer sets 2..N in partition: we still have state from previous current
      row's frame computation, now adjust by possibly subtracting rows no
      longer in frame and possibly adding new rows now within range.
    */
    const int64 prev_last_rowno_in_frame = w.last_rowno_in_range_frame();
    const int64 prev_first_rowno_in_frame = w.first_rowno_in_range_frame();

    /*
      As an optimization, if:
      - RANGE frame specification ends at CURRENT ROW and
      - current_row belongs to frame of previous row,
      then both rows are peers, so have the same frame: nothing changes.
    */
    if (range_to_current_row && current_row >= prev_first_rowno_in_frame &&
        current_row <= prev_last_rowno_in_frame) {
      // Peer set should already have been determined:
      assert(w.last_rowno_in_peerset() >= current_row);
      have_peers_current_row = true;
    } else {
      /**
         Whether we know the start of the frame yet. The a priori setting is
         inherited from the previous current row.
      */
      bool found_first =
          (prev_first_rowno_in_frame <= prev_last_rowno_in_frame);
      int64 new_first_rowno_in_frame = prev_first_rowno_in_frame;  // a priori

      int64 inverted = 0;  // Number of rows inverted when moving frame
      int64 rowno;         // Partition relative, loop counter

      if (range_from_first_to_current_row) {
        /*
          No need to locate frame's start, it's first row of partition. No
          need to recompute FIRST_VALUE, it's same as for previous row.
          So we just have to accumulate new rows.
        */
        assert(current_row > prev_last_rowno_in_frame && lower_limit == 1 &&
               prev_first_rowno_in_frame == 1 && found_first);
      } else {
        for (rowno = lower_limit;
             (rowno <= upper &&
              prev_first_rowno_in_frame <= prev_last_rowno_in_frame);
             rowno++) {
          /* Set up the non-wf fields for aggregating to the output row. */
          if (bring_back_frame_row(
                  thd, &w, rowno,
                  Window_retrieve_cached_row_reason::FIRST_IN_FRAME))
            return true;

          if (w.before_frame()) {
            w.set_inverse(true)
                .
                /*
                  The next setting sets the logical last row number in the frame
                  after inversion, so that final actions can do the right thing,
                  e.g.  AVG needs to know the updated cardinality. The
                  aggregates consults m_rowno_in_frame for that, so set it
                  accordingly.
                */
                set_rowno_in_frame(prev_last_rowno_in_frame -
                                   prev_first_rowno_in_frame + 1 - ++inverted)
                .set_is_last_row_in_frame(true);  // pessimistic assumption

            // Set the current row as the last row in the peerset.
            w.set_is_last_row_in_peerset_within_frame(true);

            /*
              It may be that rowno is not in previous frame; for example if
              column id contains 1, 3, 4 and 5 and frame is RANGE BETWEEN 2
              FOLLOWING AND 2 FOLLOWING: we process id=1, frame of id=1 is
              id=3; then we process id=3: id=3 is before frame (and was in
              previous frame), id=4 is before frame too (and was not in
              previous frame); so id=3 only should be inverted:
            */
            if (rowno >= prev_first_rowno_in_frame &&
                rowno <= prev_last_rowno_in_frame) {
              if (copy_funcs(param, thd, CFT_WF_FRAMING)) return true;
            }

            w.set_inverse(false).set_is_last_row_in_frame(false);
            w.set_is_last_row_in_peerset_within_frame(false);
            found_first = false;
          } else {
            if (w.after_frame()) {
              found_first = false;
            } else {
              w.set_first_rowno_in_range_frame(rowno);
              found_first = true;
              new_first_rowno_in_frame = rowno;
              w.set_rowno_in_frame(1);
            }

            break;
          }
        }

        // Empty frame
        if (rowno > upper && !found_first) {
          w.set_first_rowno_in_range_frame(rowno);
          w.set_last_rowno_in_range_frame(rowno - 1);
        }

        if ((have_first_value || have_last_value) &&
            (rowno <= last_rowno_in_cache) && found_first) {
          /*
             We have FIRST_VALUE or LAST_VALUE and have a new first row; make it
             last also until we find something better.
          */
          w.set_is_last_row_in_frame(true);

          if (copy_funcs(param, thd, CFT_WF_USES_ONLY_ONE_ROW)) return true;
          w.set_is_last_row_in_frame(false);

          if (have_last_value && w.last_rowno_in_range_frame() > rowno) {
            /* Set up the non-wf fields for aggregating to the output row. */
            if (bring_back_frame_row(
                    thd, &w, w.last_rowno_in_range_frame(),
                    Window_retrieve_cached_row_reason::LAST_IN_FRAME))
              return true;

            w.set_rowno_in_frame(w.last_rowno_in_range_frame() -
                                 w.first_rowno_in_range_frame() + 1)
                .set_is_last_row_in_frame(true);
            if (copy_funcs(param, thd, CFT_WF_USES_ONLY_ONE_ROW)) return true;
            w.set_is_last_row_in_frame(false);
          }
        }
      }

      /*
        We last evaluated last_rowno_in_range_frame for the previous current
        row. Now evaluate over any new rows within range of the current row.
      */
      const int64 first = w.last_rowno_in_range_frame() + 1;
      const bool empty =
          w.last_rowno_in_range_frame() < w.first_rowno_in_range_frame();
      bool row_added = false;

      for (rowno = first; rowno <= upper; rowno++) {
        w.save_pos(Window_retrieve_cached_row_reason::LAST_IN_FRAME);
        if (bring_back_frame_row(
                thd, &w, rowno,
                Window_retrieve_cached_row_reason::LAST_IN_FRAME))
          return true;

        if (w.before_frame()) {
          if (!found_first) new_first_rowno_in_frame++;
          continue;
        } else if (w.after_frame()) {
          w.set_last_rowno_in_range_frame(rowno - 1);
          if (!found_first) w.set_first_rowno_in_range_frame(rowno);
          /*
            We read one row too far, so reinstate previous hint for last in
            frame. We will likely be reading the last row in frame
            again in for next current row, and then we will need the hint.
          */
          w.restore_pos(Window_retrieve_cached_row_reason::LAST_IN_FRAME);
          break;
        }  // else: row is within range, process

        const int64 rowno_in_frame = rowno - new_first_rowno_in_frame + 1;

        if (rowno_in_frame == 1 && !found_first) {
          found_first = true;
          w.set_first_rowno_in_range_frame(rowno);
          // Found the first row in this range frame. Make a note in the hint.
          w.copy_pos(Window_retrieve_cached_row_reason::LAST_IN_FRAME,
                     Window_retrieve_cached_row_reason::FIRST_IN_FRAME);
        }
        w.set_rowno_in_frame(rowno_in_frame)
            .set_is_last_row_in_frame(true);  // pessimistic assumption

        if (copy_funcs(param, thd, CFT_WF_FRAMING)) return true;

        w.set_is_last_row_in_frame(false);  // undo temporary states
        row_added = true;
      }

      if (w.before_frame() && empty) {
        assert(!row_added && !found_first);
        // This row's value is too low to fit in frame. We already had an empty
        // set of frame rows when evaluating for the previous row, and the set
        // is still empty.  So, we can move the possible boundaries for the
        // set of frame rows for the next row to be evaluated one row ahead.
        // We need only update last_rowno_in_range_frame here, first_row
        // no_in_range_frame will be adjusted below to be one higher, cf.
        // "maintain invariant" comment.
        w.set_last_rowno_in_range_frame(
            min(w.last_rowno_in_range_frame() + 1, upper));
      }

      if (rowno > upper && row_added)
        w.set_last_rowno_in_range_frame(rowno - 1);

      if (range_to_current_row) {
        w.set_last_rowno_in_peerset(w.last_rowno_in_range_frame());
        have_peers_current_row = true;
      }

      if (found_first && !have_nth_value.m_offsets.empty()) {
        // frame is non-empty, so we might find NTH_VALUE
        assert(w.first_rowno_in_range_frame() <= w.last_rowno_in_range_frame());
        int fno = 0;
        for (Window::st_offset nth : have_nth_value.m_offsets) {
          const int64 row_to_get =
              w.first_rowno_in_range_frame() + nth.m_rowno - 1;
          if (row_to_get <= w.last_rowno_in_range_frame()) {
            if (bring_back_frame_row(
                    thd, &w, row_to_get,
                    Window_retrieve_cached_row_reason::MISC_POSITIONS, fno++))
              return true;

            w.set_rowno_in_frame(nth.m_rowno);

            if (copy_funcs(param, thd, CFT_WF_USES_ONLY_ONE_ROW)) return true;
          }
        }
      }

      // We have empty frame, maintain invariant
      if (!found_first) {
        assert(!row_added);
        w.set_first_rowno_in_range_frame(w.last_rowno_in_range_frame() + 1);
      }
    }
  }

  /* We need the peer of the current row to evaluate the row. */
  if (needs_peerset && !have_peers_current_row) {
    int64 first = current_row;

    if (current_row != 1) first = w.last_rowno_in_peerset() + 1;

    if (current_row >= first) {
      int64 rowno;
      for (rowno = current_row; rowno <= last_rowno_in_cache; rowno++) {
        if (bring_back_frame_row(
                thd, &w, rowno,
                Window_retrieve_cached_row_reason::LAST_IN_PEERSET))
          return true;

        if (rowno == current_row) {
          /* establish current row as base-line for peer set */
          w.reset_order_by_peer_set();
          w.set_last_rowno_in_peerset(current_row);
        } else if (w.in_new_order_by_peer_set()) {
          w.set_last_rowno_in_peerset(rowno - 1);
          break;  // we have accumulated all rows in the peer set
        }
      }
      if (rowno > last_rowno_in_cache)
        w.set_last_rowno_in_peerset(last_rowno_in_cache);
    }
  }

  if (optimizable && optimizable_primed) w.set_aggregates_primed(true);

  if (bring_back_frame_row(thd, &w, current_row,
                           Window_retrieve_cached_row_reason::CURRENT))
    return true;

  /* NTILE and other non-framing wfs */
  if (w.needs_partition_cardinality()) {
    /* Set up the non-wf fields for aggregating to the output row. */
    if (process_wfs_needing_partition_cardinality(
            thd, param, have_nth_value, have_lead_lag, current_row, &w,
            Window_retrieve_cached_row_reason::CURRENT))
      return true;
  }

  if (w.is_last() && copy_funcs(param, thd, CFT_HAS_WF)) return true;
  *output_row_ready = true;
  w.set_last_row_output(current_row);
  DBUG_PRINT("info", ("sent row: %" PRId64, current_row));

  return false;
}

}  // namespace

WindowIterator::WindowIterator(THD *thd,
                               unique_ptr_destroy_only<RowIterator> source,
                               Temp_table_param *temp_table_param, JOIN *join,
                               int output_slice)
    : RowIterator(thd),
      m_source(std::move(source)),
      m_temp_table_param(temp_table_param),
      m_window(temp_table_param->m_window),
      m_join(join),
      m_output_slice(output_slice) {
  assert(!m_window->needs_buffering());
}

bool WindowIterator::Init() {
  if (m_source->Init()) {
    return true;
  }
  m_window->reset_round();

  // Store which slice we will be reading from.
  m_input_slice = m_join->get_ref_item_slice();

  return false;
}

int WindowIterator::Read() {
  SwitchSlice(m_join, m_input_slice);

  int err = m_source->Read();

  SwitchSlice(m_join, m_output_slice);

  if (err != 0) {
    return err;
  }

  if (copy_funcs(m_temp_table_param, thd(), CFT_HAS_NO_WF)) return 1;

  m_window->check_partition_boundary();

  if (copy_funcs(m_temp_table_param, thd(), CFT_WF)) return 1;

  if (m_window->is_last() && copy_funcs(m_temp_table_param, thd(), CFT_HAS_WF))
    return 1;

  return 0;
}

BufferingWindowIterator::BufferingWindowIterator(
    THD *thd, unique_ptr_destroy_only<RowIterator> source,
    Temp_table_param *temp_table_param, JOIN *join, int output_slice)
    : RowIterator(thd),
      m_source(std::move(source)),
      m_temp_table_param(temp_table_param),
      m_window(temp_table_param->m_window),
      m_join(join),
      m_output_slice(output_slice) {
  assert(m_window->needs_buffering());
}

bool BufferingWindowIterator::Init() {
  if (m_source->Init()) {
    return true;
  }
  m_window->reset_round();
  m_possibly_buffered_rows = false;
  m_last_input_row_started_new_partition = false;
  m_eof = false;

  // Store which slice we will be reading from.
  m_input_slice = m_join->get_ref_item_slice();
  assert(m_input_slice >= 0);

  return false;
}

int BufferingWindowIterator::Read() {
  SwitchSlice(m_join, m_output_slice);

  if (m_eof) {
    return ReadBufferedRow(/*new_partition_or_eof=*/true);
  }

  // The previous call to Read() may have caused multiple rows to be ready
  // for output, but could only return one of them. See if there are more
  // to be output.
  if (m_possibly_buffered_rows) {
    int err = ReadBufferedRow(m_last_input_row_started_new_partition);
    if (err != -1) {
      return err;
    }
  }

  for (;;) {
    if (m_last_input_row_started_new_partition) {
      /*
        We didn't really buffer this row yet since, we found a partition
        change so we had to finalize the previous partition first.
        Bring back saved row for next partition.
      */
      if (bring_back_frame_row(
              thd(), m_window, Window::FBC_FIRST_IN_NEXT_PARTITION,
              Window_retrieve_cached_row_reason::WONT_UPDATE_HINT)) {
        return 1;
      }

      /*
        copy_funcs(CFT_HAS_NO_WF) is not necessary: a non-WF function was
        calculated and saved in OUT, then this OUT column was copied to
        special row, then restored to OUT column.
      */

      m_window->reset_partition_state();
      if (buffer_windowing_record(thd(), m_temp_table_param,
                                  nullptr /* first in new partition */)) {
        return 1;
      }

      m_last_input_row_started_new_partition = false;
    } else {
      // Read a new input row, if it exists. This needs to be done under
      // the input slice, so that any expressions in sub-iterators are
      // evaluated correctly.
      int err;
      {
        Switch_ref_item_slice slice_switch(m_join, m_input_slice);
        err = m_source->Read();
      }
      if (err == 1) {
        return 1;  // Error.
      }
      if (err == -1) {
        // EOF. Read any pending buffered rows, and then that's it.
        m_eof = true;
        return ReadBufferedRow(/*new_partition_or_eof=*/true);
      }

      /*
        This saves the values of non-WF functions for the row. For
        example, 1+t.a. But also 1+LEAD. Even though at this point we lack
        data to compute LEAD; the saved value is thus incorrect; later,
        when the row is fully computable, we will re-evaluate the
        CFT_NON_WF to get a correct value for 1+LEAD. We haven't copied fields
        yet, so use input file slice: referenced fields are present in input
        file record.
      */
      {
        Switch_ref_item_slice slice_switch(m_join, m_input_slice);
        if (copy_funcs(m_temp_table_param, thd(), CFT_HAS_NO_WF)) return 1;
      }

      bool new_partition = false;
      if (buffer_windowing_record(thd(), m_temp_table_param, &new_partition)) {
        return 1;
      }
      m_last_input_row_started_new_partition = new_partition;
    }

    int err = ReadBufferedRow(m_last_input_row_started_new_partition);
    if (err == 1) {
      return 1;
    }

    if (err == 0) {
      return 0;
    }

    // This input row didn't generate an output row right now, so we'll just
    // continue the loop.
  }
}

int BufferingWindowIterator::ReadBufferedRow(bool new_partition_or_eof) {
  bool output_row_ready;
  if (process_buffered_windowing_record(
          thd(), m_temp_table_param, new_partition_or_eof, &output_row_ready)) {
    return 1;
  }
  if (thd()->killed) {
    thd()->send_kill_message();
    return 1;
  }
  if (output_row_ready) {
    // Return the buffered row, and there are possibly more.
    // These will be checked on the next call to Read().
    m_possibly_buffered_rows = true;
    return 0;
  } else {
    // No more buffered rows.
    m_possibly_buffered_rows = false;
    return -1;
  }
}
