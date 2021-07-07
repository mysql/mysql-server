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

#include "sql/range_optimizer/range_scan.h"

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>

#include "field_types.h"
#include "m_ctype.h"
#include "m_string.h"
#include "my_alloc.h"
#include "my_base.h"
#include "my_dbug.h"
#include "my_sys.h"
#include "mysql/components/services/bits/psi_bits.h"
#include "mysql/service_mysql_alloc.h"
#include "sql/current_thd.h"
#include "sql/key.h"
#include "sql/psi_memory_key.h"
#include "sql/range_optimizer/range_scan_desc.h"
#include "sql/sql_bitmap.h"
#include "sql/sql_class.h"
#include "sql/sql_const.h"
#include "sql/sql_select.h"
#include "sql/system_variables.h"
#include "sql/table.h"
#include "sql/thr_malloc.h"
#include "sql_string.h"
#include "template_utils.h"

QUICK_RANGE_SELECT::QUICK_RANGE_SELECT(
    TABLE *table, uint key_nr, MEM_ROOT *return_mem_root, uint mrr_flags,
    uint mrr_buf_size, const KEY_PART *key,
    Bounds_checked_array<QUICK_RANGE *> ranges_arg, uint used_key_parts_arg)
    : ranges(ranges_arg),
      free_file(false),
      cur_range(nullptr),
      last_range(nullptr),
      mrr_flags(mrr_flags),
      mrr_buf_size(mrr_buf_size),
      mrr_buf_desc(nullptr),
      key_parts(key),
      dont_free(false),
      mem_root(return_mem_root) {
  DBUG_TRACE;

  used_key_parts = used_key_parts_arg;
  in_ror_merged_scan = false;
  index = key_nr;
  m_table = table;
  key_part_info = m_table->key_info[index].key_part;

  file = m_table->file;
  record = m_table->record[0];

  for (const QUICK_RANGE *range : ranges) {
    max_used_key_length =
        std::max(max_used_key_length, uint(range->min_length));
    max_used_key_length =
        std::max(max_used_key_length, uint(range->max_length));
  }
}

void QUICK_RANGE_SELECT::need_sorted_output() { mrr_flags |= HA_MRR_SORTED; }

int QUICK_RANGE_SELECT::init() {
  DBUG_TRACE;

  if (column_bitmap.bitmap == nullptr) {
    /* Allocate a bitmap for used columns */
    my_bitmap_map *bitmap =
        (my_bitmap_map *)mem_root->Alloc(m_table->s->column_bitmap_size);
    if (bitmap == nullptr) {
      column_bitmap.bitmap = nullptr;
      return true;
    } else {
      bitmap_init(&column_bitmap, bitmap, m_table->s->fields);
    }
  }

  if (file->inited) file->ha_index_or_rnd_end();
  return false;
}

void QUICK_RANGE_SELECT::range_end() {
  if (file->inited) file->ha_index_or_rnd_end();
}

QUICK_RANGE_SELECT::~QUICK_RANGE_SELECT() {
  DBUG_TRACE;
  if (m_table->key_info[index].flags & HA_MULTI_VALUED_KEY && file)
    file->ha_extra(HA_EXTRA_DISABLE_UNIQUE_RECORD_FILTER);

  if (!dont_free) {
    /* file is NULL for CPK scan on covering ROR-intersection */
    if (file) {
      range_end();
      if (free_file) {
        DBUG_PRINT("info",
                   ("Freeing separate handler %p (free: %d)", file, free_file));
        file->ha_external_lock(current_thd, F_UNLCK);
        file->ha_close();
        destroy(file);
      }
    }
  }
  my_free(mrr_buf_desc);
}

#ifndef NDEBUG
static void print_multiple_key_values(const KEY_PART *key_part,
                                      const uchar *key, uint used_length) {
  char buff[1024];
  const uchar *key_end = key + used_length;
  String tmp(buff, sizeof(buff), &my_charset_bin);
  uint store_length;
  TABLE *table = key_part->field->table;
  my_bitmap_map *old_sets[2];

  dbug_tmp_use_all_columns(table, old_sets, table->read_set, table->write_set);

  for (; key < key_end; key += store_length, key_part++) {
    Field *field = key_part->field;
    if (field->is_array())
      field = (down_cast<Field_typed_array *>(field))->get_conv_field();
    store_length = key_part->store_length;

    if (field->is_nullable()) {
      if (*key) {
        if (fwrite("NULL", sizeof(char), 4, DBUG_FILE) != 4) {
          goto restore_col_map;
        }
        continue;
      }
      key++;  // Skip null byte
      store_length--;
    }
    field->set_key_image(key, key_part->length);
    if (field->type() == MYSQL_TYPE_BIT)
      (void)field->val_int_as_str(&tmp, true);
    else
      field->val_str(&tmp);
    if (fwrite(tmp.ptr(), sizeof(char), tmp.length(), DBUG_FILE) !=
        tmp.length()) {
      goto restore_col_map;
    }
    if (key + store_length < key_end) fputc('/', DBUG_FILE);
  }
restore_col_map:
  dbug_tmp_restore_column_maps(table->read_set, table->write_set, old_sets);
}

void QUICK_RANGE_SELECT::dbug_dump(int indent, bool verbose) {
  /* purecov: begin inspected */
  fprintf(DBUG_FILE, "%*squick range select, key %s, length: %d\n", indent, "",
          m_table->key_info[index].name, max_used_key_length);

  if (verbose) {
    for (size_t ix = 0; ix < ranges.size(); ++ix) {
      fprintf(DBUG_FILE, "%*s", indent + 2, "");
      QUICK_RANGE *range = ranges[ix];
      if (!(range->flag & NO_MIN_RANGE)) {
        print_multiple_key_values(key_parts, range->min_key, range->min_length);
        if (range->flag & NEAR_MIN)
          fputs(" < ", DBUG_FILE);
        else
          fputs(" <= ", DBUG_FILE);
      }
      fputs("X", DBUG_FILE);

      if (!(range->flag & NO_MAX_RANGE)) {
        if (range->flag & NEAR_MAX)
          fputs(" < ", DBUG_FILE);
        else
          fputs(" <= ", DBUG_FILE);
        print_multiple_key_values(key_parts, range->max_key, range->max_length);
      }
      fputs("\n", DBUG_FILE);
    }
  }
  /* purecov: end */
}
#endif

/*
  Return 1 if there is only one range and this uses the whole unique key
*/

bool QUICK_RANGE_SELECT::unique_key_range() {
  if (ranges.size() == 1) {
    QUICK_RANGE *tmp = ranges[0];
    if ((tmp->flag & (EQ_RANGE | NULL_RANGE)) == EQ_RANGE) {
      KEY *key = m_table->key_info + index;
      return (key->flags & HA_NOSAME) && key->key_length == tmp->min_length;
    }
  }
  return false;
}

/*
  Range sequence interface implementation for array<QUICK_RANGE>: initialize

  SYNOPSIS
    quick_range_seq_init()
      init_param  Caller-opaque paramenter: QUICK_RANGE_SELECT* pointer

  RETURN
    Opaque value to be passed to quick_range_seq_next
*/

range_seq_t quick_range_seq_init(void *init_param, uint, uint) {
  QUICK_RANGE_SELECT *quick = static_cast<QUICK_RANGE_SELECT *>(init_param);
  QUICK_RANGE **first = quick->ranges.begin();
  QUICK_RANGE **last = quick->ranges.end();
  quick->qr_traversal_ctx.first = first;
  quick->qr_traversal_ctx.cur = first;
  quick->qr_traversal_ctx.last = last;
  return &quick->qr_traversal_ctx;
}

/*
  Range sequence interface implementation for array<QUICK_RANGE>: get next

  SYNOPSIS
    quick_range_seq_next()
      rseq        Value returned from quick_range_seq_init
      range  OUT  Store information about the range here

  @note This function return next range, and 'next' means next range in the
  array of ranges relatively to the current one when the first keypart has
  ASC sort order, or previous range - when key part has DESC sort order.
  This is needed to preserve correct order of records in case of multiple
  ranges over DESC keypart.

  RETURN
    0  Ok
    1  No more ranges in the sequence
*/

uint quick_range_seq_next(range_seq_t rseq, KEY_MULTI_RANGE *range) {
  QUICK_RANGE_SEQ_CTX *ctx = (QUICK_RANGE_SEQ_CTX *)rseq;

  if (ctx->cur == ctx->last) return 1; /* no more ranges */

  QUICK_RANGE *cur = *(ctx->cur);
  key_range *start_key = &range->start_key;
  key_range *end_key = &range->end_key;

  start_key->key = cur->min_key;
  start_key->length = cur->min_length;
  start_key->keypart_map = cur->min_keypart_map;
  start_key->flag =
      ((cur->flag & NEAR_MIN)
           ? HA_READ_AFTER_KEY
           : (cur->flag & EQ_RANGE) ? HA_READ_KEY_EXACT : HA_READ_KEY_OR_NEXT);
  end_key->key = cur->max_key;
  end_key->length = cur->max_length;
  end_key->keypart_map = cur->max_keypart_map;
  /*
    We use HA_READ_AFTER_KEY here because if we are reading on a key
    prefix. We want to find all keys with this prefix.
  */
  end_key->flag =
      (cur->flag & NEAR_MAX ? HA_READ_BEFORE_KEY : HA_READ_AFTER_KEY);
  range->range_flag = cur->flag;
  ctx->cur++;
  assert(ctx->cur <= ctx->last);
  return 0;
}

int QUICK_RANGE_SELECT::reset() {
  uint buf_size;
  uchar *mrange_buff = nullptr;
  int error;
  HANDLER_BUFFER empty_buf;
  DBUG_TRACE;
  last_range = nullptr;
  cur_range = ranges.begin();

  /* set keyread to true if index is covering */
  if (!m_table->no_keyread && m_table->covering_keys.is_set(index))
    m_table->set_keyread(true);
  else
    m_table->set_keyread(false);
  if (!file->inited) {
    /*
      read_set is set to the correct value for ror_merge_scan here as a
      subquery execution during optimization might result in innodb not
      initializing the read set in index_read() leading to wrong
      results while merging.
    */
    MY_BITMAP *const save_read_set = m_table->read_set;
    MY_BITMAP *const save_write_set = m_table->write_set;
    const bool sorted = (mrr_flags & HA_MRR_SORTED);
    DBUG_EXECUTE_IF("bug14365043_2", DBUG_SET("+d,ha_index_init_fail"););

    /* Pass index specifc read set for ror_merged_scan */
    if (in_ror_merged_scan) {
      /*
        We don't need to signal the bitmap change as the bitmap is always the
        same for this m_table->file
      */
      m_table->column_bitmaps_set_no_signal(&column_bitmap, &column_bitmap);
    }
    if ((error = file->ha_index_init(index, sorted))) {
      file->print_error(error, MYF(0));
      return error;
    }
    if (in_ror_merged_scan) {
      /* Restore bitmaps set on entry */
      m_table->column_bitmaps_set_no_signal(save_read_set, save_write_set);
    }
  }
  // Enable & reset unique record filter for multi-valued index
  if (m_table->key_info[index].flags & HA_MULTI_VALUED_KEY) {
    file->ha_extra(HA_EXTRA_ENABLE_UNIQUE_RECORD_FILTER);
    // Add PK's fields to read_set as unique filter uses rowid to skip dups
    if (m_table->s->primary_key != MAX_KEY)
      m_table->mark_columns_used_by_index_no_reset(m_table->s->primary_key,
                                                   m_table->read_set);
  }

  /* Allocate buffer if we need one but haven't allocated it yet */
  if (mrr_buf_size && !mrr_buf_desc) {
    buf_size = mrr_buf_size;
    while (buf_size &&
           !my_multi_malloc(key_memory_QUICK_RANGE_SELECT_mrr_buf_desc,
                            MYF(MY_WME), &mrr_buf_desc, sizeof(*mrr_buf_desc),
                            &mrange_buff, buf_size, NullS)) {
      /* Try to shrink the buffers until both are 0. */
      buf_size /= 2;
    }
    if (!mrr_buf_desc) return HA_ERR_OUT_OF_MEM;

    /* Initialize the handler buffer. */
    mrr_buf_desc->buffer = mrange_buff;
    mrr_buf_desc->buffer_end = mrange_buff + buf_size;
    mrr_buf_desc->end_of_used_area = mrange_buff;
  }

  if (!mrr_buf_desc)
    empty_buf.buffer = empty_buf.buffer_end = empty_buf.end_of_used_area =
        nullptr;

  RANGE_SEQ_IF seq_funcs = {quick_range_seq_init, quick_range_seq_next,
                            nullptr};
  error =
      file->multi_range_read_init(&seq_funcs, this, ranges.size(), mrr_flags,
                                  mrr_buf_desc ? mrr_buf_desc : &empty_buf);
  return error;
}

/*
  Get next possible record using quick-struct.

  SYNOPSIS
    QUICK_RANGE_SELECT::get_next()

  NOTES
    Record is read into table->record[0]

  RETURN
    0			Found row
    HA_ERR_END_OF_FILE	No (more) rows in range
    #			Error code
*/

int QUICK_RANGE_SELECT::get_next() {
  char *dummy;
  MY_BITMAP *const save_read_set = m_table->read_set;
  MY_BITMAP *const save_write_set = m_table->write_set;
  DBUG_TRACE;

  if (in_ror_merged_scan) {
    /*
      We don't need to signal the bitmap change as the bitmap is always the
      same for this m_table->file
    */
    m_table->column_bitmaps_set_no_signal(&column_bitmap, &column_bitmap);
  }

  int result = file->ha_multi_range_read_next(&dummy);

  if (in_ror_merged_scan) {
    /* Restore bitmaps set on entry */
    m_table->column_bitmaps_set_no_signal(save_read_set, save_write_set);
  }
  return result;
}

/*
  Get the next record with a different prefix.

  @param prefix_length   length of cur_prefix
  @param group_key_parts The number of key parts in the group prefix
  @param cur_prefix      prefix of a key to be searched for

  Each subsequent call to the method retrieves the first record that has a
  prefix with length prefix_length and which is different from cur_prefix,
  such that the record with the new prefix is within the ranges described by
  this->ranges. The record found is stored into the buffer pointed by
  this->record. The method is useful for GROUP-BY queries with range
  conditions to discover the prefix of the next group that satisfies the range
  conditions.

  @todo

    This method is a modified copy of QUICK_RANGE_SELECT::get_next(), so both
    methods should be unified into a more general one to reduce code
    duplication.

  @retval 0                  on success
  @retval HA_ERR_END_OF_FILE if returned all keys
  @retval other              if some error occurred
*/

int QUICK_RANGE_SELECT::get_next_prefix(uint prefix_length,
                                        uint group_key_parts,
                                        uchar *cur_prefix) {
  DBUG_TRACE;
  const key_part_map keypart_map = make_prev_keypart_map(group_key_parts);

  for (;;) {
    int result;
    if (last_range) {
      /* Read the next record in the same range with prefix after cur_prefix. */
      assert(cur_prefix != nullptr);
      result = file->ha_index_read_map(record, cur_prefix, keypart_map,
                                       HA_READ_AFTER_KEY);
      if (result || last_range->max_keypart_map == 0) return result;

      key_range previous_endpoint;
      last_range->make_max_endpoint(&previous_endpoint, prefix_length,
                                    keypart_map);
      if (file->compare_key(&previous_endpoint) <= 0) return 0;
    }

    const size_t count = ranges.size() - (cur_range - ranges.begin());
    if (count == 0) {
      /* Ranges have already been used up before. None is left for read. */
      last_range = nullptr;
      return HA_ERR_END_OF_FILE;
    }
    last_range = *(cur_range++);

    key_range start_key, end_key;
    last_range->make_min_endpoint(&start_key, prefix_length, keypart_map);
    last_range->make_max_endpoint(&end_key, prefix_length, keypart_map);

    const bool sorted = (mrr_flags & HA_MRR_SORTED);
    result = file->ha_read_range_first(
        last_range->min_keypart_map ? &start_key : nullptr,
        last_range->max_keypart_map ? &end_key : nullptr,
        last_range->flag & EQ_RANGE, sorted);
    if ((last_range->flag & (UNIQUE_RANGE | EQ_RANGE)) ==
        (UNIQUE_RANGE | EQ_RANGE))
      last_range = nullptr;  // Stop searching

    if (result != HA_ERR_END_OF_FILE) return result;
    last_range = nullptr;  // No matching rows; go to next range
  }
}

/*
  Check if current row will be retrieved by this QUICK_RANGE_SELECT

  NOTES
    It is assumed that currently a scan is being done on another index
    which reads all necessary parts of the index that is scanned by this
    quick select.
    The implementation does a binary search on sorted array of disjoint
    ranges, without taking size of range into account.

    This function is used to filter out clustered PK scan rows in
    index_merge quick select.

  RETURN
    true  if current row will be retrieved by this quick select
    false if not
*/

bool QUICK_RANGE_SELECT::row_in_ranges() {
  if (ranges.empty()) return false;

  QUICK_RANGE *res;
  size_t min = 0;
  size_t max = ranges.size() - 1;
  size_t mid = (max + min) / 2;

  while (min != max) {
    if (cmp_next(ranges[mid])) {
      /* current row value > mid->max */
      min = mid + 1;
    } else
      max = mid;
    mid = (min + max) / 2;
  }
  res = ranges[mid];
  return (!cmp_next(res) && !cmp_prev(res));
}

/**
  Create a compatible quick select with the result ordered in an opposite way

  @param used_key_parts_arg  Number of used key parts

  @retval NULL in case of errors (OOM etc)
  @retval pointer to a newly created QUICK_SELECT_DESC if success
*/

QUICK_SELECT_I *QUICK_RANGE_SELECT::make_reverse(uint used_key_parts_arg) {
  return new (mem_root) QUICK_SELECT_DESC(std::move(*this), used_key_parts_arg);
}

/*
  Compare if found key is over max-value
  Returns 0 if key <= range->max_key
  TODO: Figure out why can't this function be as simple as cmp_prev().
  At least it could use key_cmp() from key.cc, it's almost identical.
*/

int QUICK_RANGE_SELECT::cmp_next(QUICK_RANGE *range_arg) {
  int cmp;
  if (range_arg->flag & NO_MAX_RANGE) return 0; /* key can't be to large */

  cmp = key_cmp(key_part_info, range_arg->max_key, range_arg->max_length);
  if (cmp < 0 || (cmp == 0 && !(range_arg->flag & NEAR_MAX))) return 0;
  return 1;  // outside of range
}

/*
  Returns 0 if found key is inside range (found key >= range->min_key).
*/

int QUICK_RANGE_SELECT::cmp_prev(QUICK_RANGE *range_arg) {
  int cmp;
  if (range_arg->flag & NO_MIN_RANGE) return 0; /* key can't be to small */

  cmp = key_cmp(key_part_info, range_arg->min_key, range_arg->min_length);
  if (cmp > 0 || (cmp == 0 && !(range_arg->flag & NEAR_MIN))) return 0;
  return 1;  // outside of range
}

void QUICK_RANGE_SELECT::add_info_string(String *str) {
  KEY *key_info = m_table->key_info + index;
  str->append(key_info->name);
}

void QUICK_RANGE_SELECT::add_keys_and_lengths(String *key_names,
                                              String *used_lengths) {
  char buf[64];
  size_t length;
  KEY *key_info = m_table->key_info + index;
  key_names->append(key_info->name);
  length = longlong10_to_str(max_used_key_length, buf, 10) - buf;
  used_lengths->append(buf, length);
}
