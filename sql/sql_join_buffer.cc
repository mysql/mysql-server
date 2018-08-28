/* Copyright (c) 2010, 2018, Oracle and/or its affiliates. All rights reserved.

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
  join cache optimizations

  @defgroup Query_Optimizer  Query Optimizer
  @{
*/

#include "sql/sql_join_buffer.h"

#include <limits.h>
#include <algorithm>
#include <atomic>
#include <memory>

#include "binary_log_types.h"
#include "my_base.h"
#include "my_bitmap.h"
#include "my_byteorder.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_macros.h"
#include "my_table_map.h"
#include "sql/field.h"
#include "sql/item.h"
#include "sql/key.h"
#include "sql/opt_trace.h"       // Opt_trace_object
#include "sql/psi_memory_key.h"  // key_memory_JOIN_CACHE
#include "sql/records.h"
#include "sql/sql_bitmap.h"
#include "sql/sql_class.h"
#include "sql/sql_const.h"
#include "sql/sql_opt_exec_shared.h"
#include "sql/sql_optimizer.h"  // JOIN
#include "sql/sql_select.h"
#include "sql/system_variables.h"
#include "sql/table.h"
#include "sql/thr_malloc.h"

using std::max;
using std::min;

/*****************************************************************************
 *  Join cache module
 ******************************************************************************/

/*
  Fill in the descriptor of a flag field associated with a join cache

  SYNOPSIS
    add_field_flag_to_join_cache()
      str           position in a record buffer to copy the field from/to
      length        length of the field
      field  IN/OUT pointer to the field descriptor to fill in

  DESCRIPTION
    The function fill in the descriptor of a cache flag field to which
    the parameter 'field' points to. The function uses the first two
    parameters to set the position in the record buffer from/to which
    the field value is to be copied and the length of the copied fragment.
    Before returning the result the function increments the value of
    *field by 1.
    The function ignores the fields 'blob_length' and 'ofset' of the
    descriptor.

  RETURN
    the length of the field
*/

static uint add_flag_field_to_join_cache(uchar *str, uint length,
                                         CACHE_FIELD **field) {
  CACHE_FIELD *copy = *field;
  copy->str = str;
  copy->length = length;
  copy->type = 0;
  copy->field = 0;
  copy->referenced_field_no = 0;
  copy->next_copy_rowid = NULL;
  (*field)++;
  return length;
}

/*
  Fill in the descriptors of table data fields associated with a join cache

  SYNOPSIS
    add_table_data_fields_to_join_cache()
      tab              descriptors of fields from this table are to be filled
      field_set        descriptors for only these fields are to be created
      field_cnt IN/OUT     counter of data fields
      descr  IN/OUT        pointer to the first descriptor to be filled
      field_ptr_cnt IN/OUT counter of pointers to the data fields
      descr_ptr IN/OUT     pointer to the first pointer to blob descriptors

  DESCRIPTION
    The function fills in the descriptors of cache data fields from the table
    'tab'. The descriptors are filled only for the fields marked in the
    bitmap 'field_set'.
    The function fills the descriptors starting from the position pointed
    by 'descr'. If an added field is of a BLOB type then a pointer to the
    its descriptor is added to the array descr_ptr.
    At the return 'descr' points to the position after the last added
    descriptor  while 'descr_ptr' points to the position right after the
    last added pointer.

  RETURN
    the total length of the added fields
*/

static uint add_table_data_fields_to_join_cache(
    QEP_TAB *tab, MY_BITMAP *field_set, uint *field_cnt, CACHE_FIELD **descr,
    uint *field_ptr_cnt, CACHE_FIELD ***descr_ptr) {
  Field **fld_ptr;
  uint len = 0;
  CACHE_FIELD *copy = *descr;
  CACHE_FIELD **copy_ptr = *descr_ptr;
  uint used_fields = bitmap_bits_set(field_set);
  for (fld_ptr = tab->table()->field; used_fields; fld_ptr++) {
    if (bitmap_is_set(field_set, (*fld_ptr)->field_index)) {
      len += (*fld_ptr)->fill_cache_field(copy);
      if (copy->type == CACHE_BLOB) {
        *copy_ptr = copy;
        copy_ptr++;
        (*field_ptr_cnt)++;
      }
      copy->field = *fld_ptr;
      copy->referenced_field_no = 0;
      copy->next_copy_rowid = NULL;
      copy++;
      (*field_cnt)++;
      used_fields--;
    }
  }
  *descr = copy;
  *descr_ptr = copy_ptr;
  return len;
}

/*
  Determine various counters of fields associated with a record in the cache

  SYNOPSIS
    calc_record_fields()

  DESCRIPTION
    The function counts the number of total fields stored in a record
    of the cache and saves this number in the 'fields' member. It also
    determines the number of flag fields and the number of blobs.
    The function sets 'with_match_flag' on if 'join_tab' needs a match flag
    i.e. if it is the first inner table of an outer join, or of a semi-join
    with FirstMatch strategy.

  RETURN
    none
*/

void JOIN_CACHE::calc_record_fields() {
  fields = 0;
  blobs = 0;
  flag_fields = 0;
  data_field_count = 0;
  data_field_ptr_count = 0;
  referenced_fields = 0;

  QEP_TAB *tab = qep_tab - tables;

  for (; tab < qep_tab; tab++) {
    uint used_fields, used_fieldlength, used_blobs;
    calc_used_field_length(
        tab->table(), tab->keep_current_rowid, &used_fields, &used_fieldlength,
        &used_blobs, &tab->used_null_fields, &tab->used_uneven_bit_fields);
    flag_fields += tab->used_null_fields || tab->used_uneven_bit_fields;
    flag_fields += tab->table()->is_nullable();
    fields += used_fields;
    blobs += used_blobs;
  }
  if ((with_match_flag = (qep_tab->is_first_inner_for_outer_join() ||
                          (qep_tab->first_sj_inner() == qep_tab->idx() &&
                           qep_tab->get_sj_strategy() == SJ_OPT_FIRST_MATCH))))
    flag_fields++;
  fields += flag_fields;
}

/*
  Allocate memory for descriptors and pointers to them associated with the cache

  SYNOPSIS
    alloc_fields()

  DESCRIPTION
    The function allocates memory for the array of fields descriptors
    and the array of pointers to the field descriptors used to copy
    join record data from record buffers into the join buffer and
    backward. Some pointers refer to the field descriptor associated
    with previous caches. They are placed at the beginning of the
    array of pointers and its total number is specified by the parameter
    'external fields'.
    The pointer of the first array is assigned to field_descr and the
    number of elements is precalculated by the function calc_record_fields.
    The allocated arrays are adjacent.

  NOTES
    The memory is allocated in join->thd->memroot

  RETURN
    pointer to the first array
*/

int JOIN_CACHE::alloc_fields(uint external_fields) {
  uint ptr_cnt = external_fields + blobs + 1;
  uint fields_size = sizeof(CACHE_FIELD) * fields;
  field_descr =
      (CACHE_FIELD *)sql_alloc(fields_size + sizeof(CACHE_FIELD *) * ptr_cnt);
  blob_ptr = (CACHE_FIELD **)((uchar *)field_descr + fields_size);
  return (field_descr == NULL);
}

/*
  Create descriptors of the record flag fields stored in the join buffer

  SYNOPSIS
    create_flag_fields()

  DESCRIPTION
    The function creates descriptors of the record flag fields stored
    in the join buffer. These are descriptors for:
    - an optional match flag field,
    - table null bitmap fields,
    - table null row fields.
    The match flag field is created when 'join_tab' is the first inner
    table of an outer join our a semi-join. A null bitmap field is
    created for any table whose fields are to be stored in the join
    buffer if at least one of these fields is nullable or is a BIT field
    whose bits are partially stored with null bits. A null row flag
    is created for any table assigned to the cache if it is an inner
    table of an outer join.
    The descriptor for flag fields are placed one after another at the
    beginning of the array of field descriptors 'field_descr' that
    contains 'fields' elements. If there is a match flag field the
    descriptor for it is always first in the sequence of flag fields.
    The descriptors for other flag fields can follow in an arbitrary
    order.
    The flag field values follow in a record stored in the join buffer
    in the same order as field descriptors, with the match flag always
    following first.
    The function sets the value of 'flag_fields' to the total number
    of the descriptors created for the flag fields.
    The function sets the value of 'length' to the total length of the
    flag fields.

  RETURN
    none
*/

void JOIN_CACHE::create_flag_fields() {
  CACHE_FIELD *copy = field_descr;

  length = 0;

  /* If there is a match flag the first field is always used for this flag */
  if (with_match_flag)
    length += add_flag_field_to_join_cache((uchar *)&qep_tab->found,
                                           sizeof(qep_tab->found), &copy);

  /* Create fields for all null bitmaps and null row flags that are needed */
  for (QEP_TAB *tab = qep_tab - tables; tab < qep_tab; tab++) {
    TABLE *table = tab->table();
    /* Create a field for the null bitmap from table if needed */
    if (tab->used_null_fields || tab->used_uneven_bit_fields)
      length += add_flag_field_to_join_cache(table->null_flags,
                                             table->s->null_bytes, &copy);

    /* Create table for the null row flag if needed */
    if (table->is_nullable())
      length += add_flag_field_to_join_cache((uchar *)&table->null_row,
                                             sizeof(table->null_row), &copy);
  }

  /* Theoretically the new value of flag_fields can be less than the old one */
  flag_fields = copy - field_descr;
}

/*
  Create descriptors of all remaining data fields stored in the join buffer

  SYNOPSIS
    create_remaining_fields()
      all_read_fields   indicates that descriptors for all read data fields
                        are to be created

  DESCRIPTION
    The function creates descriptors for all remaining data fields of a
    record from the join buffer. If the parameter 'all_read_fields' is
    true the function creates fields for all read record fields that
    comprise the partial join record joined with join_tab. Otherwise,
    for each table tab, the set of the read fields for which the descriptors
    have to be added is determined as the difference between all read fields
    and and those for which the descriptors have been already created.
    The latter are supposed to be marked in the bitmap tab->table()->tmp_set.
    The function increases the value of 'length' to the total length of
    the added fields.

  NOTES
    If 'all_read_fields' is false the function modifies the value of
    tab->table()->tmp_set for a each table whose fields are stored in the cache.
    The function calls the method Field::fill_cache_field to figure out
    the type of the cache field and the maximal length of its representation
    in the join buffer. If this is a blob field then additionally a pointer
    to this field is added as an element of the array blob_ptr. For a blob
    field only the size of the length of the blob data is taken into account.
    It is assumed that 'data_field_count' contains the number of descriptors
    for data fields that have been already created and 'data_field_ptr_count'
    contains the number of the pointers to such descriptors having been
    stored up to the moment.

  RETURN
    none
*/

void JOIN_CACHE::create_remaining_fields(bool all_read_fields) {
  CACHE_FIELD *copy = field_descr + flag_fields + data_field_count;
  CACHE_FIELD **copy_ptr = blob_ptr + data_field_ptr_count;

  for (QEP_TAB *tab = qep_tab - tables; tab < qep_tab; tab++) {
    MY_BITMAP *rem_field_set;
    TABLE *table = tab->table();

    if (all_read_fields)
      rem_field_set = table->read_set;
    else {
      bitmap_invert(&table->tmp_set);
      bitmap_intersect(&table->tmp_set, table->read_set);
      rem_field_set = &table->tmp_set;
    }

    length += add_table_data_fields_to_join_cache(
        tab, rem_field_set, &data_field_count, &copy, &data_field_ptr_count,
        &copy_ptr);

    /* SemiJoinDuplicateElimination: allocate space for rowid if needed */
    if (tab->keep_current_rowid) {
      copy->str = table->file->ref;
      copy->length = table->file->ref_length;
      copy->type = 0;
      copy->field = 0;
      copy->referenced_field_no = 0;
      copy->next_copy_rowid = NULL;
      // Chain rowid copy objects belonging to same join_tab
      if (tab->copy_current_rowid != NULL)
        copy->next_copy_rowid = tab->copy_current_rowid;
      tab->copy_current_rowid = copy;
      length += copy->length;
      data_field_count++;
      copy++;
    }
  }
}

/*
  Calculate and set all cache constants

  SYNOPSIS
    set_constants()

  DESCRIPTION
    The function calculates and set all precomputed constants that are used
    when writing records into the join buffer and reading them from it.
    It calculates the size of offsets of a record within the join buffer
    and of a field within a record. It also calculates the number of bytes
    used to store record lengths.
    The function also calculates the maximal length of the representation
    of record in the cache excluding blob_data. This value is used when
    making a dicision whether more records should be added into the join
    buffer or not.

  RETURN
    none
*/

void JOIN_CACHE::set_constants() {
  /*
    Any record from a BKA cache is prepended with the record length.
    We use the record length when reading the buffer and building key values
    for each record. The length allows us not to read the fields that are
    not needed for keys.
    If a record has match flag it also may be skipped when the match flag
    is on. It happens if the cache is used for a semi-join operation or
    for outer join when the 'not exist' optimization can be applied.
    If some of the fields are referenced from other caches then
    the record length allows us to easily reach the saved offsets for
    these fields since the offsets are stored at the very end of the record.
    However at this moment we don't know whether we have referenced fields for
    the cache or not. Later when a referenced field is registered for the cache
    we adjust the value of the flag 'with_length'.
  */
  with_length = is_key_access() || with_match_flag;
  /*
     At this moment we don't know yet the value of 'referenced_fields',
     but in any case it can't be greater than the value of 'fields'.
  */
  uint len = length + fields * sizeof(uint) + blobs * sizeof(uchar *) +
             (prev_cache ? prev_cache->get_size_of_rec_offset() : 0) +
             sizeof(ulong) + aux_buffer_min_size();
  buff_size = max<size_t>(join->thd->variables.join_buff_size, 2 * len);
  size_of_rec_ofs = offset_size(buff_size);
  size_of_rec_len = blobs ? size_of_rec_ofs : offset_size(len);
  size_of_fld_ofs = size_of_rec_len;
  /*
    The size of the offsets for referenced fields will be added later.
    The values of 'pack_length' and 'pack_length_with_blob_ptrs' are adjusted
    every time when the first reference to the referenced field is registered.
  */
  pack_length = (with_length ? size_of_rec_len : 0) +
                (prev_cache ? prev_cache->get_size_of_rec_offset() : 0) +
                length;
  pack_length_with_blob_ptrs = pack_length + blobs * sizeof(uchar *);

  check_only_first_match = calc_check_only_first_match(qep_tab);
}

/**
  Allocate memory for a join buffer.

  The function allocates a lump of memory for the join buffer. The
  size of the allocated memory is 'buff_size' bytes.

  @returns false if success, otherwise true.
*/
bool JOIN_CACHE::alloc_buffer() {
  DBUG_EXECUTE_IF("jb_alloc_fail", buff = NULL; DBUG_SET("-d,jb_alloc_fail");
                  return true;);

  DBUG_EXECUTE_IF("jb_alloc_100MB",
                  buff = (uchar *)my_malloc(key_memory_JOIN_CACHE,
                                            100 * 1024 * 1024, MYF(0));
                  return buff == NULL;);

  buff = (uchar *)my_malloc(key_memory_JOIN_CACHE, buff_size, MYF(0));
  return buff == NULL;
}

/**
  Filter base columns of virtual generated columns that might not be read
  by a dynamic range scan.

  A dynamic range scan will read the data from a table using either a
  table scan, a range scan on a covering index, or a range scan on a
  non-covering index. The table's read set contains all columns that
  will be read by the table scan. This might be base columns that are
  used to evaluate virtual column values that are part of an
  index. When the table is read using a table scan, these base columns
  will be read from the storage engine, but when a index/range scan on
  a covering index is used, the base columns will not be read by the
  storage engine. To avoid that these potentially un-read columns are
  inserted into the join buffer, we need to adjust the read set to
  only contain columns that are read independently of which access
  method that is used: these are the only columns needed in the join
  buffer for the query.

  This function does the following manipulations of table's read_set:

  * if one or more of the alternative range scan indexes are covering,
    then the table's read_set is intersected with the read_set for
    each of the covering indexes.

  For potential range indexes that are not covering, no adjustment to
  the read_set is done.

  @note The table->read_set will be changed by this function. It is
  the caller's responsibility to save a copy of this in
  table->tmp_set.

  @param tab the query execution tab
*/

static void filter_gcol_for_dynamic_range_scan(QEP_TAB *const tab) {
  TABLE *table = tab->table();
  DBUG_ASSERT(tab->dynamic_range() && table->vfield);

  for (uint key = 0; key < table->s->keys; ++key) {
    /*
      We only need to consider indexes that are:
      1. Candidates for being used for range scan.
      2. A covering index for the query.
    */
    if (tab->keys().is_set(key) && table->covering_keys.is_set(key)) {
      my_bitmap_map
          bitbuf[(bitmap_buffer_size(MAX_FIELDS) / sizeof(my_bitmap_map)) + 1];
      MY_BITMAP range_read_set;
      bitmap_init(&range_read_set, bitbuf, table->s->fields, false);

      // Make a bitmap of which fields this covering index can read
      table->mark_columns_used_by_index_no_reset(key, &range_read_set,
                                                 UINT_MAX);

      // Compute the minimal read_set that must be included in the join buffer
      bitmap_intersect(table->read_set, &range_read_set);
    }
  }
}

/**
  Filter the base columns of virtual generated columns if using a covering index
  scan.

  When setting up the join buffer, adjust read_set temporarily so that
  only contains the columns that are needed in the join operation and
  afterwards. Afterwards, the regular contents are restored (the
  columns to be read from input tables).

  For a virtual generated column, all base columns are added to the read_set
  of the table. The storage engine will then copy all base column values so
  that the value of the GC can be calculated inside the executor.
  But when a virtual GC is fetched using a covering index, the actual GC
  value is fetched by the storage engine and the base column values are not
  needed. Join buffering code must not try to copy them (in
  create_remaining_fields()).
  So, we eliminate from read_set those columns that are available from the
  covering index.
*/

void JOIN_CACHE::filter_virtual_gcol_base_cols() {
  for (QEP_TAB *tab = qep_tab - tables; tab < qep_tab; tab++) {
    TABLE *table = tab->table();
    if (table->vfield == NULL) continue;

    const uint index = tab->effective_index();
    if (index != MAX_KEY && table->index_contains_some_virtual_gcol(index) &&
        /*
          There are two cases:
          - If the table scan uses covering index scan, we can get the value
            of virtual generated column from index
          - If not, JOIN_CACHE only needs the value of virtual generated
            columns (This is why the index can be chosen as a covering index).
            After restore the base columns, the value of virtual generated
            columns can be calculated correctly.
        */
        table->covering_keys.is_set(index)) {
      DBUG_ASSERT(bitmap_is_clear_all(&table->tmp_set));
      // Keep table->read_set in tmp_set so that it can be restored
      bitmap_copy(&table->tmp_set, table->read_set);
      bitmap_clear_all(table->read_set);
      table->mark_columns_used_by_index_no_reset(index, table->read_set);
      if (table->s->primary_key != MAX_KEY)
        table->mark_columns_used_by_index_no_reset(table->s->primary_key,
                                                   table->read_set);
      bitmap_intersect(table->read_set, &table->tmp_set);
    } else if (tab->dynamic_range()) {
      DBUG_ASSERT(bitmap_is_clear_all(&table->tmp_set));
      // Keep table->read_set in tmp_set so that it can be restored
      bitmap_copy(&table->tmp_set, table->read_set);

      filter_gcol_for_dynamic_range_scan(tab);
    }
  }
}

/**
  After JOIN_CACHE initialization, the table->read_set is restored so that the
  virtual generated column can be calculated during later time.
*/

void JOIN_CACHE::restore_virtual_gcol_base_cols() {
  for (QEP_TAB *tab = qep_tab - tables; tab < qep_tab; tab++) {
    TABLE *table = tab->table();
    if (table->vfield == NULL) continue;

    if (!bitmap_is_clear_all(&table->tmp_set)) {
      bitmap_copy(table->read_set, &table->tmp_set);
      bitmap_clear_all(&table->tmp_set);
    }
  }
}

/*
  Initialize a BNL cache

  SYNOPSIS
    init()

  DESCRIPTION
    The function initializes the cache structure. It supposed to be called
    right after a constructor for the JOIN_CACHE_BNL.
    The function allocates memory for the join buffer and for descriptors of
    the record fields stored in the buffer.

  NOTES
    The code of this function should have been included into the constructor
    code itself. However the new operator for the class JOIN_CACHE_BNL would
    never fail while memory allocation for the join buffer is not absolutely
    unlikely to fail. That's why this memory allocation has to be placed in a
    separate function that is called in a couple with a cache constructor.
    It is quite natural to put almost all other constructor actions into
    this function.

  RETURN
    0   initialization with buffer allocations has been succeeded
    1   otherwise
*/

int JOIN_CACHE_BNL::init() {
  DBUG_ENTER("JOIN_CACHE::init");

  /*
    If there is a previous cache, start with the corresponding table, otherwise:
    - if in a regular execution, start with the first non-const table.
    - if in a materialized subquery, start with the first table of the subquery.
  */
  QEP_TAB *tab = prev_cache
                     ? prev_cache->qep_tab
                     : sj_is_materialize_strategy(qep_tab->get_sj_strategy())
                           ? &QEP_AT(qep_tab, first_sj_inner())
                           : &join->qep_tab[join->const_tables];

  tables = qep_tab - tab;

  filter_virtual_gcol_base_cols();

  calc_record_fields();

  if (alloc_fields(0)) DBUG_RETURN(1);

  create_flag_fields();

  create_remaining_fields(true);

  restore_virtual_gcol_base_cols();

  set_constants();

  if (alloc_buffer()) DBUG_RETURN(1);

  reset_cache(true);

  if (qep_tab->condition() && qep_tab->first_inner() == NO_PLAN_IDX) {
    /*
      When we read a record from qep_tab->table(), we can filter it by testing
      conditions which depend only on this table. Note that such condition
      must not depend on previous tables (except const ones) as the record is
      going to be joined with all buffered records of the previous tables.
    */
    const table_map available = join->best_ref[qep_tab->idx()]->added_tables();
    Item *const tmp = make_cond_for_table(join->thd, qep_tab->condition(),
                                          join->const_table_map | available,
                                          available, false);
    if (tmp) {
      Opt_trace_object(&join->thd->opt_trace)
          .add("constant_condition_in_bnl", tmp);
      const_cond = tmp;
    }
  }

  DBUG_RETURN(0);
}

/*
  Initialize a BKA cache

  SYNOPSIS
    init()

  DESCRIPTION
    The function initializes the cache structure. It supposed to be called
    right after a constructor for the JOIN_CACHE_BKA.
    The function allocates memory for the join buffer and for descriptors of
    the record fields stored in the buffer.

  NOTES
    The code of this function should have been included into the constructor
    code itself. However the new operator for the class JOIN_CACHE_BKA would
    never fail while memory allocation for the join buffer is not absolutely
    unlikely to fail. That's why this memory allocation has to be placed in a
    separate function that is called in a couple with a cache constructor.
    It is quite natural to put almost all other constructor actions into
    this function.

  RETURN
    0   initialization with buffer allocations has been succeeded
    1   otherwise
*/

int JOIN_CACHE_BKA::init() {
  DBUG_ENTER("JOIN_CACHE_BKA::init");
#ifndef DBUG_OFF
  m_read_only = false;
#endif
  local_key_arg_fields = 0;
  external_key_arg_fields = 0;

  /*
    Reference JOIN_CACHE_BNL::init() for details.
  */
  QEP_TAB *tab = prev_cache
                     ? prev_cache->qep_tab
                     : sj_is_materialize_strategy(qep_tab->get_sj_strategy())
                           ? &QEP_AT(qep_tab, first_sj_inner())
                           : &join->qep_tab[join->const_tables];

  tables = qep_tab - tab;

  filter_virtual_gcol_base_cols();
  calc_record_fields();

  /* Mark all fields that can be used as arguments for this key access */
  TABLE_REF *ref = &qep_tab->ref();
  JOIN_CACHE *cache = this;
  do {
    /*
      Traverse the ref expressions and find the occurrences of fields in them
      for each table 'tab' whose fields are to be stored in the 'cache' join
      buffer. Mark these fields in the bitmap tab->table()->tmp_set. For these
      fields count the number of them stored in this cache and the total number
      of them stored in the previous caches. Save the result of the counting 'in
      local_key_arg_fields' and 'external_key_arg_fields' respectively.
    */
    for (QEP_TAB *tab = cache->qep_tab - cache->tables; tab < cache->qep_tab;
         tab++) {
      uint key_args;
      bitmap_clear_all(&tab->table()->tmp_set);
      for (uint i = 0; i < ref->key_parts; i++) {
        Item *ref_item = ref->items[i];
        if (!(tab->table_ref->map() & ref_item->used_tables())) continue;
        ref_item->walk(
            &Item::add_field_to_set_processor,
            Item::enum_walk(Item::WALK_POSTFIX | Item::WALK_SUBQUERY),
            (uchar *)tab->table());
      }
      if ((key_args = bitmap_bits_set(&tab->table()->tmp_set))) {
        if (cache == this)
          local_key_arg_fields += key_args;
        else
          external_key_arg_fields += key_args;
      }
    }
    cache = cache->prev_cache;
  } while (cache);

  if (alloc_fields(external_key_arg_fields)) DBUG_RETURN(1);

  create_flag_fields();

  /*
    Save pointers to the cache fields in previous caches
    that  are used to build keys for this key access.
  */
  cache = this;
  uint ext_key_arg_cnt = external_key_arg_fields;
  CACHE_FIELD *copy;
  CACHE_FIELD **copy_ptr = blob_ptr;
  while (ext_key_arg_cnt) {
    cache = cache->prev_cache;
    for (QEP_TAB *tab = cache->qep_tab - cache->tables; tab < cache->qep_tab;
         tab++) {
      CACHE_FIELD *copy_end;
      MY_BITMAP *key_read_set = &tab->table()->tmp_set;
      /* key_read_set contains the bitmap of tab's fields referenced by ref */
      if (bitmap_is_clear_all(key_read_set)) continue;
      copy_end = cache->field_descr + cache->fields;
      for (copy = cache->field_descr + cache->flag_fields; copy < copy_end;
           copy++) {
        /*
          (1) - when we store rowids for DuplicateWeedout, they have
                copy->field==NULL
        */
        if (copy->field &&  // (1)
            copy->field->table == tab->table() &&
            bitmap_is_set(key_read_set, copy->field->field_index)) {
          *copy_ptr++ = copy;
          ext_key_arg_cnt--;
          if (!copy->referenced_field_no) {
            /*
              Register the referenced field 'copy':
              - set the offset number in copy->referenced_field_no,
              - adjust the value of the flag 'with_length',
              - adjust the values of 'pack_length' and
                of 'pack_length_with_blob_ptrs'.
            */
            copy->referenced_field_no = ++cache->referenced_fields;
            cache->with_length = true;
            cache->pack_length += cache->get_size_of_fld_offset();
            cache->pack_length_with_blob_ptrs +=
                cache->get_size_of_fld_offset();
          }
        }
      }
    }
  }
  /* After this 'blob_ptr' shall not be be changed */
  blob_ptr = copy_ptr;

  /* Now create local fields that are used to build ref for this key access */
  copy = field_descr + flag_fields;
  for (QEP_TAB *tab = qep_tab - tables; tab < qep_tab; tab++) {
    length += add_table_data_fields_to_join_cache(
        tab, &tab->table()->tmp_set, &data_field_count, &copy,
        &data_field_ptr_count, &copy_ptr);
  }

  use_emb_key = check_emb_key_usage();

  create_remaining_fields(false);
  restore_virtual_gcol_base_cols();
  bitmap_clear_all(&qep_tab->table()->tmp_set);

  set_constants();

  if (alloc_buffer()) DBUG_RETURN(1);

  reset_cache(true);

  DBUG_RETURN(0);
}

/*
  Check the possibility to read the access keys directly from the join buffer

  SYNOPSIS
    check_emb_key_usage()

  DESCRIPTION
    The function checks some conditions at which the key values can be read
    directly from the join buffer. This is possible when the key values can be
    composed by concatenation of the record fields stored in the join buffer.
    Sometimes when the access key is multi-component the function has to
  re-order the fields written into the join buffer to make keys embedded. If key
    values for the key access are detected as embedded then 'use_emb_key'
    is set to true.

  EXAMPLE
    Let table t2 has an index defined on the columns a,b . Let's assume also
    that the columns t2.a, t2.b as well as the columns t1.a, t1.b are all
    of the integer type. Then if the query
      SELECT COUNT(*) FROM t1, t2 WHERE t1.a=t2.a and t1.b=t2.b
    is executed with a join cache in such a way that t1 is the driving
    table then the key values to access table t2 can be read directly
    from the join buffer.

  NOTES
    In some cases key values could be read directly from the join buffer but
    we still do not consider them embedded. In the future we'll expand the
    class of keys which we identify as embedded.

  RETURN
    true  - key values will be considered as embedded,
    false - otherwise.
*/

bool JOIN_CACHE_BKA::check_emb_key_usage() {
  uint i;
  Item *item;
  KEY_PART_INFO *key_part;
  CACHE_FIELD *copy;
  CACHE_FIELD *copy_end;
  uint len = 0;
  TABLE *table = qep_tab->table();
  TABLE_REF *ref = &qep_tab->ref();
  KEY *keyinfo = table->key_info + ref->key;

  /*
    If some of the key arguments are not from the local cache the key
    is not considered as embedded.
    TODO:
    Expand it to the case when ref->key_parts=1 and local_key_arg_fields=0.
  */
  if (external_key_arg_fields != 0) return false;
  /*
    If the number of the local key arguments is not equal to the number
    of key parts the key value cannot be read directly from the join buffer.
  */
  if (local_key_arg_fields != ref->key_parts) return false;

  /*
    A key is not considered embedded if one of the following is true:
    - one of its key parts is not equal to a field
    - it is a partial key
    - definition of the argument field does not coincide with the
      definition of the corresponding key component
    - the argument field has different byte ordering from the target table
    - some of the key components are nullable
  */
  for (i = 0; i < ref->key_parts; i++) {
    item = ref->items[i]->real_item();
    if (item->type() != Item::FIELD_ITEM) return false;
    key_part = keyinfo->key_part + i;
    if (key_part->key_part_flag & HA_PART_KEY_SEG) return false;
    if (!key_part->field->eq_def(((Item_field *)item)->field)) return false;
    if (((Item_field *)item)->field->table->s->db_low_byte_first !=
        table->s->db_low_byte_first) {
      return false;
    }
    if (key_part->field->maybe_null()) {
      return false;
      /*
        If this is changed so that embedded keys may contain nullable
        components, get_next_key() and put_record() will have to test
        ref->null_rejecting in the "embedded keys" case too.
      */
    }
  }

  copy = field_descr + flag_fields;
  copy_end = copy + local_key_arg_fields;
  for (; copy < copy_end; copy++) {
    /*
      If some of the key arguments are of variable length the key
      is not considered as embedded.
    */
    if (copy->type != 0) return false;
    /*
      If some of the key arguments are bit fields whose bits are partially
      stored with null bits the key is not considered as embedded.
    */
    if (copy->field->type() == MYSQL_TYPE_BIT &&
        ((Field_bit *)(copy->field))->bit_len)
      return false;
    len += copy->length;
  }

  emb_key_length = len;

  /*
    Make sure that key fields follow the order of the corresponding
    key components these fields are equal to. For this the descriptors
    of the fields that comprise the key might be re-ordered.
  */
  for (i = 0; i < ref->key_parts; i++) {
    uint j;
    Item *item = ref->items[i]->real_item();
    Field *fld = ((Item_field *)item)->field;
    CACHE_FIELD *init_copy = field_descr + flag_fields + i;
    for (j = i, copy = init_copy; i < local_key_arg_fields; i++, copy++) {
      if (fld->eq(copy->field)) {
        if (j != i) {
          CACHE_FIELD key_part_copy = *copy;
          *copy = *init_copy;
          *init_copy = key_part_copy;
        }
        break;
      }
    }
  }

  return true;
}

uint JOIN_CACHE_BKA::aux_buffer_incr() {
  uint incr = 0;
  TABLE_REF *ref = &qep_tab->ref();
  TABLE *tab = qep_tab->table();

  if (records == 1) incr = ref->key_length + tab->file->ref_length;
  /*
    When adding a new record to the join buffer this can match
    multiple keys in this table. We use "records per key" as estimate for
    the number of records that will match and reserve space in the
    DS-MRR sort buffer for this many record references.
  */
  rec_per_key_t rec_per_key =
      tab->key_info[ref->key].records_per_key(ref->key_parts - 1);
  set_if_bigger(rec_per_key, 1.0f);
  incr += static_cast<uint>(tab->file->stats.mrr_length_per_rec * rec_per_key);
  return incr;
}

/**
  Calculate the minimum size for the MRR buffer.

  @return The minumum size that must be allocated for the MRR buffer
*/

uint JOIN_CACHE_BKA::aux_buffer_min_size() const {
  /*
    For DS-MRR to work, the sort buffer must have space to store the
    reference (or primary key) for at least one record.
  */
  DBUG_ASSERT(qep_tab->table()->file->stats.mrr_length_per_rec > 0);
  return qep_tab->table()->file->stats.mrr_length_per_rec;
}

/*
  Check if the record combination matches the index condition

  SYNOPSIS
    JOIN_CACHE_BKA::skip_index_tuple()
      rseq             Value returned by bka_range_seq_init()
      range_info       MRR range association data

  DESCRIPTION
    This function is invoked from MRR implementation to check if an index
    tuple matches the index condition. It is used in the case where the index
    condition actually depends on both columns of the used index and columns
    from previous tables.

    Accessing columns of the previous tables requires special handling with
    BKA. The idea of BKA is to collect record combinations in a buffer and
    then do a batch of ref access lookups, i.e. by the time we're doing a
    lookup its previous-records-combination is not in prev_table->record[0]
    but somewhere in the join buffer.

    We need to get it from there back into prev_table(s)->record[0] before we
    can evaluate the index condition, and that's why we need this function
    instead of regular IndexConditionPushdown.

  NOTE
    Possible optimization:
    Before we unpack the record from a previous table
    check if this table is used in the condition.
    If so then unpack the record otherwise skip the unpacking.
    This should be done by a special virtual method
    get_partial_record_by_pos().

  RETURN
    0    The record combination satisfies the index condition
    1    Otherwise
*/

bool JOIN_CACHE_BKA::skip_index_tuple(range_seq_t rseq, char *range_info) {
  DBUG_ENTER("JOIN_CACHE_BKA::skip_index_tuple");
  JOIN_CACHE_BKA *cache = (JOIN_CACHE_BKA *)rseq;
  cache->get_record_by_pos((uchar *)range_info);
  DBUG_RETURN(!qep_tab->cache_idx_cond->val_int());
}

/*
  Check if the record combination matches the index condition

  SYNOPSIS
    bka_skip_index_tuple()
      rseq             Value returned by bka_range_seq_init()
      range_info       MRR range association data

  DESCRIPTION
    This is wrapper for JOIN_CACHE_BKA::skip_index_tuple method,
    see comments there.

  NOTE
    This function is used as a RANGE_SEQ_IF::skip_index_tuple callback.

  RETURN
    0    The record combination satisfies the index condition
    1    Otherwise
*/

static bool bka_skip_index_tuple(range_seq_t rseq, char *range_info) {
  DBUG_ENTER("bka_skip_index_tuple");
  JOIN_CACHE_BKA *cache = (JOIN_CACHE_BKA *)rseq;
  DBUG_RETURN(cache->skip_index_tuple(rseq, range_info));
}

/**
  Write record fields and their required offsets into the join cache buffer.

  @param      link    a reference to the associated info in the previous cache
  @param[out] is_full whether it has been decided that no more records will be
                      added to the join buffer
  @return length of the written record data

  @details
    This function put into the cache buffer the following info that it reads
    from the join record buffers or computes somehow:
    (1) the length of all fields written for the record (optional)
    (2) an offset to the associated info in the previous cache (if there is any)
        determined by the link parameter
    (3) all flag fields of the tables whose data field are put into the cache:
        - match flag (optional),
        - null bitmaps for all tables,
        - null row flags for all tables
    (4) values of all data fields including
        - full images of those fixed legth data fields that cannot have
          trailing spaces
        - significant part of fixed length fields that can have trailing spaces
          with the prepended length
        - data of non-blob variable length fields with the prepended data length
        - blob data from blob fields with the prepended data length
    (5) record offset values for the data fields that are referred to from
        other caches

    The record is written at the current position stored in the field 'pos'.
    At the end of the function 'pos' points at the position right after the
    written record data.
    The function increments the number of records in the cache that is stored
    in the 'records' field by 1. The function also modifies the values of
    'curr_rec_pos' and 'last_rec_pos' to point to the written record.
    The 'end_pos' cursor is modified accordingly.
    The 'last_rec_blob_data_is_in_rec_buff' is set on if the blob data
    remains in the record buffers and not copied to the join buffer. It may
    happen only to the blob data from the last record added into the cache.
*/

uint JOIN_CACHE::write_record_data(uchar *link, bool *is_full) {
  DBUG_ASSERT(!m_read_only);
  uchar *cp = pos;
  uchar *init_pos = cp;

  records++; /* Increment the counter of records in the cache */

  reserve_aux_buffer();

  auto len = pack_length;

  /*
    For each blob to be put into cache save its length and a pointer
    to the value in the corresponding element of the blob_ptr array.
    Blobs with null values are skipped.
    Increment 'len' by the total length of all these blobs.
  */
  if (blobs) {
    CACHE_FIELD **copy_ptr = blob_ptr;
    CACHE_FIELD **copy_ptr_end = copy_ptr + blobs;
    for (; copy_ptr < copy_ptr_end; copy_ptr++) {
      Field_blob *blob_field = (Field_blob *)(*copy_ptr)->field;
      if (!blob_field->is_null()) {
        uint blob_len = blob_field->get_length();
        (*copy_ptr)->blob_length = blob_len;
        len += blob_len;
        blob_field->get_ptr(&(*copy_ptr)->str);
      }
    }
  }

  /*
    Check whether we won't be able to add any new record into the cache after
    this one because the cache will be full. Set last_record to true if it's so.
    The assume that the cache will be full after the record has been written
    into it if either the remaining space of the cache is not big enough for the
    record's blob values or if there is a chance that not all non-blob fields
    of the next record can be placed there.
    This function is called only in the case when there is enough space left in
    the cache to store at least non-blob parts of the current record.
  */
  bool last_record = (len + pack_length_with_blob_ptrs) > rem_space();

  /*
    Save the position for the length of the record in the cache if it's needed.
    The length of the record will be inserted here when all fields of the record
    are put into the cache.
  */
  uchar *rec_len_ptr = NULL;
  if (with_length) {
    rec_len_ptr = cp;
    cp += size_of_rec_len;
  }

  /*
    Put a reference to the fields of the record that are stored in the previous
    cache if there is any. This reference is passed by the 'link' parameter.
  */
  if (prev_cache) {
    cp += prev_cache->get_size_of_rec_offset();
    prev_cache->store_rec_ref(cp, link);
  }

  curr_rec_pos = cp;

  /* If there is a match flag set its value to 0 */
  CACHE_FIELD *copy = field_descr;
  if (with_match_flag) *copy[0].str = 0;

  /* First put into the cache the values of all flag fields */
  CACHE_FIELD *copy_end = field_descr + flag_fields;
  for (; copy < copy_end; copy++) {
    memcpy(cp, copy->str, copy->length);
    cp += copy->length;
  }

  /* Now put the values of the remaining fields as soon as they are not nulls */
  copy_end = field_descr + fields;
  for (; copy < copy_end; copy++) {
    Field *field = copy->field;
    if (field && field->maybe_null() && field->is_null()) {
      /* Do not copy a field if its value is null */
      if (copy->referenced_field_no) copy->offset = 0;
      continue;
    }
    /* Save the offset of the field to put it later at the end of the record */
    if (copy->referenced_field_no) copy->offset = cp - curr_rec_pos;

    if (copy->type == CACHE_BLOB) {
      Field_blob *blob_field = (Field_blob *)copy->field;
      if (last_record) {
        last_rec_blob_data_is_in_rec_buff = 1;
        /* Put down the length of the blob and the pointer to the data */
        blob_field->get_image(cp, copy->length + sizeof(char *),
                              blob_field->charset());
        cp += copy->length + sizeof(char *);
      } else {
        /* First put down the length of the blob and then copy the data */
        blob_field->get_image(cp, copy->length, blob_field->charset());
        if (copy->blob_length > 0)
          memcpy(cp + copy->length, copy->str, copy->blob_length);
        cp += copy->length + copy->blob_length;
      }
    } else {
      switch (copy->type) {
        case CACHE_VARSTR1:
          /* Copy the significant part of the short varstring field */
          len = (uint)copy->str[0] + 1;
          memcpy(cp, copy->str, len);
          cp += len;
          break;
        case CACHE_VARSTR2:
          /* Copy the significant part of the long varstring field */
          len = uint2korr(copy->str) + 2;
          memcpy(cp, copy->str, len);
          cp += len;
          break;
        case CACHE_STRIPPED: {
          /*
            Put down the field value stripping all trailing spaces off.
            After this insert the length of the written sequence of bytes.
          */
          uchar *str, *end;
          for (str = copy->str, end = str + copy->length;
               end > str && end[-1] == ' '; end--)
            ;
          len = (uint)(end - str);
          int2store(cp, len);
          memcpy(cp + 2, str, len);
          cp += len + 2;
          break;
        }
        default:
          /* Copy the entire image of the field from the record buffer */
          memcpy(cp, copy->str, copy->length);
          cp += copy->length;
      }
    }
  }

  /* Add the offsets of the fields that are referenced from other caches */
  if (referenced_fields) {
    uint cnt = 0;
    for (copy = field_descr + flag_fields; copy < copy_end; copy++) {
      if (copy->referenced_field_no) {
        store_fld_offset(cp + size_of_fld_ofs * (copy->referenced_field_no - 1),
                         copy->offset);
        cnt++;
      }
    }
    cp += size_of_fld_ofs * cnt;
  }

  if (rec_len_ptr)
    store_rec_length(rec_len_ptr, (ulong)(cp - rec_len_ptr - size_of_rec_len));
  last_rec_pos = curr_rec_pos;
  end_pos = pos = cp;
  *is_full = last_record;
  return (uint)(cp - init_pos);
}

/**
  @brief Reset the join buffer for reading/writing: default implementation

  @param for_writing  if it's true the function reset the buffer for writing

  @details
    This default implementation of the virtual function reset_cache() resets
    the join buffer for reading or writing.
    If the buffer is reset for reading only the 'pos' value is reset
    to point to the very beginning of the join buffer. If the buffer is
    reset for writing additionally:
    - the counter of the records in the buffer is set to 0,
    - the value of 'last_rec_pos' gets pointing at the position just
      before the buffer,
    - 'end_pos' is set to point to the beginning of the join buffer,
    - the size of the auxiliary buffer is reset to 0,
    - the flag 'last_rec_blob_data_is_in_rec_buff' is set to 0.
*/

void JOIN_CACHE::reset_cache(bool for_writing) {
  pos = buff;
  curr_rec_link = 0;
  if (for_writing) {
#ifndef DBUG_OFF
    m_read_only = false;
#endif
    records = 0;
    last_rec_pos = buff;
    end_pos = pos;
    last_rec_blob_data_is_in_rec_buff = 0;
  }
}

/*
  Add a record into the join buffer: the default implementation

  SYNOPSIS
    put_record_in_cache()

  DESCRIPTION
    This default implementation of the virtual function put_record writes
    the next matching record into the join buffer.
    It also links the record having been written into the join buffer with
    the matched record in the previous cache if there is any.
    The implementation assumes that the function get_curr_link()
    will return exactly the pointer to this matched record.

  RETURN
    true    if it has been decided that it should be the last record
            in the join buffer,
    false   otherwise
*/

bool JOIN_CACHE::put_record_in_cache() {
  bool is_full;
  uchar *link = 0;
  if (prev_cache) link = prev_cache->get_curr_rec_link();
  write_record_data(link, &is_full);
  return (is_full);
}

/**
  Read the next record from the join buffer.

  Read the fields of the next record from the join buffer of this cache.
  Also read any other fields associated with this record from the join
  buffers of the previous caches. The fields are read into the
  corresponding record buffers.

  It is supposed that 'pos' points to the position in the buffer right
  after the previous record when the function is called.  Upon return,
  'pos' will point to the position after the record that was read.
  The value of 'curr_rec_pos' is also updated to point to the beginning
  of the first field of the record in the join buffer.

  @return whether there are no more records to read from the join buffer
*/

bool JOIN_CACHE::get_record() {
  bool res;
  uchar *prev_rec_ptr = 0;
  if (with_length) pos += size_of_rec_len;
  if (prev_cache) {
    pos += prev_cache->get_size_of_rec_offset();
    prev_rec_ptr = prev_cache->get_rec_ref(pos);
  }
  curr_rec_pos = pos;
  res = (read_some_record_fields() == -1);
  if (!res) {  // There are more records to read
    pos += referenced_fields * size_of_fld_ofs;
    if (prev_cache) {
      /*
        read_some_record_fields() didn't read fields stored in previous
        buffers, read them now:
      */
      prev_cache->get_record_by_pos(prev_rec_ptr);
    }
  }
  return res;
}

/**
  Read a positioned record from the join buffer.

  Also read all other fields associated with this record from the
  join buffers of the previous caches. The fields are read into the
  corresponding record buffers.

  @param rec_ptr  record in the join buffer
*/

void JOIN_CACHE::get_record_by_pos(uchar *rec_ptr) {
  uchar *save_pos = pos;
  pos = rec_ptr;
  read_some_record_fields();
  pos = save_pos;
  if (prev_cache) {
    uchar *prev_rec_ptr = prev_cache->get_rec_ref(rec_ptr);
    prev_cache->get_record_by_pos(prev_rec_ptr);
  }
}

/**
  Read the match flag of a record.

  If this buffer has a match flag, that match flag is returned.
  Otherwise, the match flag of a preceding buffer is returned.
  A match flag must be present in at least one of the buffers.

  @param rec_ptr  position of the first field of the record in the join buffer

  @return the match flag
*/

bool JOIN_CACHE::get_match_flag_by_pos(uchar *rec_ptr) {
  if (with_match_flag) return *rec_ptr != 0;
  if (prev_cache) {
    auto prev_rec_ptr = prev_cache->get_rec_ref(rec_ptr);
    return prev_cache->get_match_flag_by_pos(prev_rec_ptr);
  }
  DBUG_ASSERT(false);
  return false;
}

/**
  Read some flag and data fields of a record from the join buffer.

  Reads all fields (flag and data fields) stored in this join buffer, for the
  current record (at 'pos'). If the buffer is incremental, fields of this
  record which are stored in previous join buffers are _not_ read so remain
  unknown: caller must then make sure to call this function on previous
  buffers too.

  The fields are read starting from the position 'pos' which is
  supposed to point to the beginning of the first record field.
  The function increments the value of 'pos' by the length of the
  read data.

  Flag fields are copied back to their source; data fields are copied to the
  record's buffer.

  @retval (-1)   if there are no more records in the join buffer
  @retval <>(-1) length of the data read from the join buffer
*/

int JOIN_CACHE::read_some_record_fields() {
  uchar *init_pos = pos;

  if (pos > last_rec_pos || !records) return -1;

  // First match flag, read null bitmaps and null_row flag
  read_some_flag_fields();

  /* Now read the remaining table fields if needed */
  CACHE_FIELD *copy = field_descr + flag_fields;
  CACHE_FIELD *copy_end = field_descr + fields;
  bool blob_in_rec_buff = blob_data_is_in_rec_buff(init_pos);
  for (; copy < copy_end; copy++) read_record_field(copy, blob_in_rec_buff);

  return (uint)(pos - init_pos);
}

/**
  Read some flag fields of a record from the join buffer.

  Reads all flag fields stored in this join buffer, for the current record (at
  'pos'). If the buffer is incremental, flag fields of this record which are
  stored in previous join buffers are _not_ read so remain unknown: caller
  must then make sure to call this function on previous buffers too.

  The flag fields are read starting from the position 'pos'.
  The function increments the value of 'pos' by the length of the
  read data.

  Flag fields are copied back to their source.
*/
void JOIN_CACHE::read_some_flag_fields() {
  CACHE_FIELD *copy = field_descr;
  CACHE_FIELD *copy_end = copy + flag_fields;
  for (; copy < copy_end; copy++) {
    memcpy(copy->str, pos, copy->length);
    pos += copy->length;
  }
}

/*
  Read a data record field from the join buffer

  SYNOPSIS
    read_record_field()
      copy             the descriptor of the data field to be read
      blob_in_rec_buff indicates whether this is the field from the record
                       whose blob data are in record buffers

  DESCRIPTION
    The function reads the data field specified by the parameter copy
    from the join buffer into the corresponding record buffer.
    The field is read starting from the position 'pos'.
    The data of blob values is not copied from the join buffer.
    The function increments the value of 'pos' by the length of the
    read data.

  RETURN
    length of the data read from the join buffer
*/

uint JOIN_CACHE::read_record_field(CACHE_FIELD *copy, bool blob_in_rec_buff) {
  uint len;
  /* Do not copy the field if its value is null */
  if (copy->field && copy->field->maybe_null() && copy->field->is_null())
    return 0;
  if (copy->type == CACHE_BLOB) {
    Field_blob *blob_field = (Field_blob *)copy->field;
    /*
      Copy the length and the pointer to data but not the blob data
      itself to the record buffer
    */
    if (blob_in_rec_buff) {
      blob_field->set_image(pos, copy->length + sizeof(char *),
                            blob_field->charset());
      len = copy->length + sizeof(char *);
    } else {
      blob_field->set_ptr(pos, pos + copy->length);
      len = copy->length + blob_field->get_length();
    }
  } else {
    switch (copy->type) {
      case CACHE_VARSTR1:
        /* Copy the significant part of the short varstring field */
        len = (uint)pos[0] + 1;
        memcpy(copy->str, pos, len);
        break;
      case CACHE_VARSTR2:
        /* Copy the significant part of the long varstring field */
        len = uint2korr(pos) + 2;
        memcpy(copy->str, pos, len);
        break;
      case CACHE_STRIPPED:
        /* Pad the value by spaces that has been stripped off */
        len = uint2korr(pos);
        memcpy(copy->str, pos + 2, len);
        memset(copy->str + len, ' ', copy->length - len);
        len += 2;
        break;
      default:
        /* Copy the entire image of the field from the record buffer */
        len = copy->length;
        memcpy(copy->str, pos, len);
    }
  }
  pos += len;
  return len;
}

/*
  Read a referenced field from the join buffer

  SYNOPSIS
    read_referenced_field()
      copy         pointer to the descriptor of the referenced field
      rec_ptr      pointer to the record that may contain this field
      len  IN/OUT  total length of the record fields

  DESCRIPTION
    The function checks whether copy points to a data field descriptor
    for this cache object. If it does not then the function returns
    false. Otherwise the function reads the field of the record in
    the join buffer pointed by 'rec_ptr' into the corresponding record
    buffer and returns true.
    If the value of *len is 0 then the function sets it to the total
    length of the record fields including possible trailing offset
    values. Otherwise *len is supposed to provide this value that
    has been obtained earlier.

  RETURN
    true   'copy' points to a data descriptor of this join cache
    false  otherwise
*/

bool JOIN_CACHE::read_referenced_field(CACHE_FIELD *copy, uchar *rec_ptr,
                                       uint *len) {
  uchar *ptr;
  uint offset;
  if (copy < field_descr || copy >= field_descr + fields) return false;
  if (!*len) {
    /* Get the total length of the record fields */
    uchar *len_ptr = rec_ptr;
    if (prev_cache) len_ptr -= prev_cache->get_size_of_rec_offset();
    *len = get_rec_length(len_ptr - size_of_rec_len);
  }

  ptr = rec_ptr - (prev_cache ? prev_cache->get_size_of_rec_offset() : 0);
  offset = get_fld_offset(
      ptr + *len -
      size_of_fld_ofs * (referenced_fields + 1 - copy->referenced_field_no));
  bool is_null = false;
  if (offset == 0 && flag_fields) is_null = true;
  if (is_null)
    copy->field->set_null();
  else {
    uchar *save_pos = pos;
    copy->field->set_notnull();
    pos = rec_ptr + offset;
    read_record_field(copy, blob_data_is_in_rec_buff(rec_ptr));
    pos = save_pos;
  }
  return true;
}

/*
  Skip record from join buffer if its match flag is on: default implementation

  SYNOPSIS
    skip_record_if_match()

  DESCRIPTION
    This default implementation of the virtual function skip_record_if_match
    skips the next record from the join buffer if its  match flag is set on.
    If the record is skipped the value of 'pos' is set to points to the position
    right after the record.

  RETURN
    true  - the match flag is on and the record has been skipped
    false - the match flag is off
*/

bool JOIN_CACHE::skip_record_if_match() {
  DBUG_ASSERT(with_match_flag && with_length);
  uint offset = size_of_rec_len;
  if (prev_cache) offset += prev_cache->get_size_of_rec_offset();
  /* Check whether the match flag is on */
  if (*(pos + offset) != 0) {
    pos += size_of_rec_len + get_rec_length(pos);
    return true;
  }
  return false;
}

/*
  Restore the fields of the last record from the join buffer

  SYNOPSIS
    restore_last_record()

  DESCRIPTION
    This function restore the values of the fields of the last record put
    into join buffer in record buffers. The values most probably have been
    overwritten by the field values from other records when they were read
    from the join buffer into the record buffer in order to check pushdown
    predicates.

  RETURN
    none
*/

void JOIN_CACHE::restore_last_record() {
  if (records) get_record_by_pos(last_rec_pos);
}

/*
  Join records from the join buffer with records from the next join table

  SYNOPSIS
    join_records()
      skip_last    do not find matches for the last record from the buffer

  DESCRIPTION
    The functions extends all records from the join buffer by the matched
    records from join_tab. In the case of outer join operation it also
    adds null complementing extensions for the records from the join buffer
    that have no match.
    No extensions are generated for the last record from the buffer if
    skip_last is true.

  NOTES
    The function must make sure that if linked join buffers are used then
    a join buffer cannot be refilled again until all extensions in the
    buffers chained to this one are generated.
    Currently an outer join operation with several inner tables always uses
    at least two linked buffers with the match join flags placed in the
    first buffer. Any record composed of rows of the inner tables that
    matches a record in this buffer must refer to the position of the
    corresponding match flag.

  IMPLEMENTATION
    When generating extensions for outer tables of an outer join operation
    first we generate all extensions for those records from the join buffer
    that have matches, after which null complementing extension for all
    unmatched records from the join buffer are generated.

  RETURN
    return one of enum_nested_loop_state, except NESTED_LOOP_NO_MORE_ROWS.
*/

enum_nested_loop_state JOIN_CACHE::join_records(bool skip_last) {
  enum_nested_loop_state rc = NESTED_LOOP_OK;
  DBUG_ENTER("JOIN_CACHE::join_records");

  table_map saved_status_bits[3] = {0, 0, 0};
  for (int cnt = 1; cnt <= static_cast<int>(tables); cnt++) {
    /*
      We may have hit EOF on previous tables; this has set
      STATUS_NOT_FOUND in their status. However, now we are going to load
      table->record[0] from the join buffer so have to declare that there is a
      record. @See convert_constant_item().
      We first need to save bits of table status; STATUS_DELETED and
      STATUS_UPDATED cannot be on as multi-table DELETE/UPDATE never use join
      buffering. So we only have three bits to save.
    */
    TABLE_LIST *const tr = qep_tab[-cnt].table_ref;
    TABLE *const table = tr->table;
    const table_map map = tr->map();
    DBUG_ASSERT(!table->has_updated_row() && !table->has_deleted_row());
    if (!table->is_started()) saved_status_bits[0] |= map;
    if (!table->has_row()) saved_status_bits[1] |= map;
    if (table->has_null_row()) saved_status_bits[2] |= map;
    table->set_found_row();  // Record exists.
  }

  const bool outer_join_first_inner = qep_tab->is_first_inner_for_outer_join();
  if (outer_join_first_inner && qep_tab->first_unmatched == NO_PLAN_IDX)
    qep_tab->not_null_compl = true;

  /*
    We're going to read records of previous tables from our buffer, and also
    records of our table; none of these can be a group-by/window tmp table, so
    we should still be on the join's first slice.
  */
  DBUG_ASSERT(qep_tab->join()->get_ref_item_slice() == REF_SLICE_SAVED_BASE);

  if (qep_tab->first_unmatched == NO_PLAN_IDX) {
    const bool pfs_batch_update = qep_tab->pfs_batch_update(join);
    if (pfs_batch_update) qep_tab->table()->file->start_psi_batch_mode();
    /* Find all records from join_tab that match records from join buffer */
    rc = join_matching_records(skip_last);
    if (pfs_batch_update) qep_tab->table()->file->end_psi_batch_mode();

    if (rc != NESTED_LOOP_OK) goto finish;
    if (outer_join_first_inner) {
      /*
        If the inner-most outer join has a single inner table, all matches for
        outer table's record from join buffer is already found by
        join_matching_records. There is no need to call
        next_cache->join_records now. The full extensions of matched and null
        extended rows will be generated together at once by calling
        next_cache->join_records at the end of this function.
      */
      if (!qep_tab->is_single_inner_for_outer_join() && next_cache) {
        /*
          Ensure that all matches for outer records from join buffer are to be
          found. Now we ensure that all full records are found for records from
          join buffer. Generally this is an overkill.
          TODO: Ensure that only matches of the inner table records have to be
          found for the records from join buffer.
        */
        rc = next_cache->join_records(skip_last);
        if (rc != NESTED_LOOP_OK) goto finish;
      }
      qep_tab->not_null_compl = false;
      /* Prepare for generation of null complementing extensions */
      for (plan_idx i = qep_tab->first_inner(); i <= qep_tab->last_inner(); ++i)
        join->qep_tab[i].first_unmatched = qep_tab->first_inner();
    }
  }
  if (qep_tab->first_unmatched != NO_PLAN_IDX) {
    if (is_key_access()) restore_last_record();

    /*
      Generate all null complementing extensions for the records from
      join buffer that don't have any matching rows from the inner tables.
    */
    reset_cache(false);
    rc = join_null_complements(skip_last);
    if (rc != NESTED_LOOP_OK) goto finish;
  }
  if (next_cache) {
    /*
      When using linked caches we must ensure the records in the next caches
      that refer to the records in the join buffer are fully extended.
      Otherwise we could have references to the records that have been
      already erased from the join buffer and replaced for new records.
    */
    rc = next_cache->join_records(skip_last);
    if (rc != NESTED_LOOP_OK) goto finish;
  }

  if (skip_last) {
    DBUG_ASSERT(!is_key_access());
    /*
       Restore the last record from the join buffer to generate
       all extensions for it.
    */
    get_record();
  }

finish:
  if (outer_join_first_inner) {
    /*
      All null complemented rows have been already generated for all
      outer records from join buffer. Restore the state of the
      first_unmatched values to 0 to avoid another null complementing.
    */
    for (plan_idx i = qep_tab->first_inner(); i <= qep_tab->last_inner(); ++i)
      join->qep_tab[i].first_unmatched = NO_PLAN_IDX;
  }
  for (int cnt = 1; cnt <= static_cast<int>(tables); cnt++) {
    /*
      We must restore the status of outer tables as it was before entering
      this function.
    */
    TABLE_LIST *const tr = qep_tab[-cnt].table_ref;
    TABLE *const table = tr->table;
    const table_map map = tr->map();
    if (saved_status_bits[0] & map) table->set_not_started();
    if (saved_status_bits[1] & map) table->set_no_row();
    if (saved_status_bits[2] & map) table->set_null_row();
  }
  restore_last_record();
  reset_cache(true);
  DBUG_RETURN(rc);
}

/*
  Using BNL find matches from the next table for records from the join buffer

  SYNOPSIS
    join_matching_records()
      skip_last    do not look for matches for the last partial join record

  DESCRIPTION
    The function retrieves all rows of the join_tab table and check whether
    they match partial join records from the join buffer. If a match is found
    the function will call the sub_select function trying to look for matches
    for the remaining join operations.
    This function currently is called only from the function join_records.
    If the value of skip_last is true the function writes the partial join
    record from the record buffer into the join buffer to save its value for
    the future processing in the caller function.

  NOTES
    The function produces all matching extensions for the records in the
    join buffer following the path of the Blocked Nested Loops algorithm.
    When an outer join operation is performed all unmatched records from
    the join buffer must be extended by null values. The function
    'join_null_complements' serves this purpose.

  RETURN
    return one of enum_nested_loop_state.
*/

enum_nested_loop_state JOIN_CACHE_BNL::join_matching_records(bool skip_last) {
  int error;
  enum_nested_loop_state rc = NESTED_LOOP_OK;

  /* Return at once if there are no records in the join buffer */
  if (!records) return NESTED_LOOP_OK;

  /*
    When joining we read records from the join buffer back into record buffers.
    If matches for the last partial join record are found through a call to
    the sub_select function then this partial join record must be saved in the
    join buffer in order to be restored just before the sub_select call.
  */
  if (skip_last) put_record_in_cache();

  // See setup_join_buffering(=: dynamic range => no cache.
  DBUG_ASSERT(!(qep_tab->dynamic_range() && qep_tab->quick()));

  /* Start retrieving all records of the joined table */
  if (qep_tab->read_record.iterator->Init()) return NESTED_LOOP_ERROR;
  if ((error = qep_tab->read_record->Read()))
    return error < 0 ? NESTED_LOOP_OK : NESTED_LOOP_ERROR;

  READ_RECORD *info = &qep_tab->read_record;
  do {
    if (qep_tab->keep_current_rowid)
      qep_tab->table()->file->position(qep_tab->table()->record[0]);

    if (join->thd->killed) {
      /* The user has aborted the execution of the query */
      join->thd->send_kill_message();
      return NESTED_LOOP_KILLED;
    }

    /*
      Do not look for matches if the last read record of the joined table
      does not meet the conditions that have been pushed to this table
    */
    if (rc == NESTED_LOOP_OK) {
      if (const_cond) {
        const bool consider_record = const_cond->val_int() != false;
        if (join->thd->is_error())  // error in condition evaluation
          return NESTED_LOOP_ERROR;
        if (!consider_record) continue;
      }
      {
        /* Prepare to read records from the join buffer */
        reset_cache(false);

        /* Read each record from the join buffer and look for matches */
        for (uint cnt = records - skip_last; cnt; cnt--) {
          /*
            If only the first match is needed and it has been already found for
            the next record read from the join buffer then the record is
            skipped.
          */
          if (!check_only_first_match || !skip_record_if_match()) {
            get_record();
            rc = generate_full_extensions(get_curr_rec());
            if (rc != NESTED_LOOP_OK) return rc;
          }
        }
      }
    }
  } while (!(error = info->iterator->Read()));

  if (error > 0)  // Fatal error
    rc = NESTED_LOOP_ERROR;
  return rc;
}

bool JOIN_CACHE::calc_check_only_first_match(const QEP_TAB *t) const {
  if ((t->last_sj_inner() == t->idx() &&
       t->get_sj_strategy() == SJ_OPT_FIRST_MATCH))
    return true;
  if (t->first_inner() != NO_PLAN_IDX &&
      QEP_AT(t, first_inner()).last_inner() == t->idx() &&
      t->table()->reginfo.not_exists_optimize)
    return true;
  return false;
}

/*
  Set match flag for a record in join buffer if it has not been set yet

  SYNOPSIS
    set_match_flag_if_none()
      first_inner     the join table to which this flag is attached to
      rec_ptr         pointer to the record in the join buffer

  DESCRIPTION
    If the records of the table are accumulated in a join buffer the function
    sets the match flag for the record in the buffer that is referred to by
    the record from this cache positioned at 'rec_ptr'.
    The function also sets the match flag 'found' of the table first inner
    if it has not been set before.

  NOTES
    The function assumes that the match flag for any record in any cache
    is placed in the first byte occupied by the record fields.

  RETURN
    true   the match flag is set by this call for the first time
    false  the match flag has been set before this call
*/

bool JOIN_CACHE::set_match_flag_if_none(QEP_TAB *first_inner, uchar *rec_ptr) {
  if (!first_inner->op) {
    /*
      Records of the first inner table to which the flag is attached to
      are not accumulated in a join buffer.
    */
    if (first_inner->found)
      return false;
    else {
      first_inner->found = true;
      return true;
    }
  }
  JOIN_CACHE *cache = this;
  while (cache->qep_tab != first_inner) {
    cache = cache->prev_cache;
    DBUG_ASSERT(cache);
    rec_ptr = cache->get_rec_ref(rec_ptr);
  }
  if (rec_ptr[0] == 0) {
    rec_ptr[0] = 1;
    first_inner->found = true;
    return true;
  }
  return false;
}

/*
  Generate all full extensions for a partial join record in the buffer

  SYNOPSIS
    generate_full_extensions()
      rec_ptr     pointer to the record from join buffer to generate extensions

  DESCRIPTION
    The function first checks whether the current record of 'join_tab' matches
    the partial join record from join buffer located at 'rec_ptr'. If it is the
    case the function calls the join_tab->next_select method to generate
    all full extension for this partial join match.

  RETURN
    return one of enum_nested_loop_state.
*/

enum_nested_loop_state JOIN_CACHE::generate_full_extensions(uchar *rec_ptr) {
  enum_nested_loop_state rc = NESTED_LOOP_OK;
  /*
    Check whether the extended partial join record meets
    the pushdown conditions.
  */
  if (check_match(rec_ptr)) {
    int res = 0;
    if (!qep_tab->check_weed_out_table ||
        !(res = do_sj_dups_weedout(join->thd, qep_tab->check_weed_out_table))) {
      set_curr_rec_link(rec_ptr);
      rc = (qep_tab->next_select)(join, qep_tab + 1, 0);
      if (rc != NESTED_LOOP_OK) {
        reset_cache(true);
        return rc;
      }
    }
    if (res == -1) {
      rc = NESTED_LOOP_ERROR;
      return rc;
    }
  }
  // error in condition evaluation
  if (join->thd->is_error()) rc = NESTED_LOOP_ERROR;
  return rc;
}

/*
  Check matching to a partial join record from the join buffer

  SYNOPSIS
    check_match()
      rec_ptr     pointer to the record from join buffer to check matching to

  DESCRIPTION
    The function checks whether the current record of 'join_tab' matches
    the partial join record from join buffer located at 'rec_ptr'. If this is
    the case and 'join_tab' is the last inner table of a semi-join or an outer
    join the function turns on the match flag for the 'rec_ptr' record unless
    it has been already set.

  NOTES
    Setting the match flag on can trigger re-evaluation of pushdown conditions
    for the record when join_tab is the last inner table of an outer join.

  RETURN
    true   there is a match
    false  there is no match
*/

bool JOIN_CACHE::check_match(uchar *rec_ptr) {
  bool skip_record;
  /* Check whether pushdown conditions are satisfied */
  if (qep_tab->skip_record(join->thd, &skip_record) || skip_record)
    return false;

  if (!((qep_tab->first_inner() != NO_PLAN_IDX &&
         QEP_AT(qep_tab, first_inner()).last_inner() == qep_tab->idx()) ||
        (qep_tab->last_sj_inner() == qep_tab->idx() &&
         qep_tab->get_sj_strategy() == SJ_OPT_FIRST_MATCH)))
    return true;  // not the last inner table

  /*
     This is the last inner table of an outer join,
     and maybe of other embedding outer joins, or
     this is the last inner table of a semi-join.
  */
  plan_idx f_i = qep_tab->first_inner() != NO_PLAN_IDX
                     ? qep_tab->first_inner()
                     : ((qep_tab->get_sj_strategy() == SJ_OPT_FIRST_MATCH)
                            ? qep_tab->first_sj_inner()
                            : NO_PLAN_IDX);

  QEP_TAB *first_inner = &join->qep_tab[f_i];

  for (;;) {
    set_match_flag_if_none(first_inner, rec_ptr);
    if (calc_check_only_first_match(first_inner) &&
        qep_tab->first_inner() == NO_PLAN_IDX)
      return true;
    /*
      This is the first match for the outer table row.
      The function set_match_flag_if_none has turned the flag
      first_inner->found on. The pushdown predicates for
      inner tables must be re-evaluated with this flag on.
      Note that, if first_inner is the first inner table
      of a semi-join, but is not an inner table of an outer join
      such that 'not exists' optimization can  be applied to it,
      the re-evaluation of the pushdown predicates is not needed.
    */
    for (QEP_TAB *tab = first_inner; tab <= qep_tab; tab++) {
      if (tab->skip_record(join->thd, &skip_record) || skip_record)
        return false;
    }
    f_i = first_inner->first_upper();
    if (f_i == NO_PLAN_IDX) break;
    first_inner = &join->qep_tab[f_i];
    if (first_inner->last_inner() != qep_tab->idx()) break;
  }

  return true;
}

/*
  Add null complements for unmatched outer records from join buffer

  SYNOPSIS
    join_null_complements()
      skip_last    do not add null complements for the last record

  DESCRIPTION
    This function is called only for inner tables of outer joins.
    The function retrieves all rows from the join buffer and adds null
    complements for those of them that do not have matches for outer
    table records.
    If the 'join_tab' is the last inner table of the embedding outer
    join and the null complemented record satisfies the outer join
    condition then the corresponding match flag is turned on
    unless it has been set earlier. This setting may trigger
    re-evaluation of pushdown conditions for the record.

  NOTES
    The same implementation of the virtual method join_null_complements
    is used for JOIN_CACHE_BNL and JOIN_CACHE_BKA.

  RETURN
    return one of enum_nested_loop_state.
*/

enum_nested_loop_state JOIN_CACHE::join_null_complements(bool skip_last) {
  uint cnt;
  enum_nested_loop_state rc = NESTED_LOOP_OK;
  bool is_first_inner = qep_tab->idx() == qep_tab->first_unmatched;
  DBUG_ENTER("JOIN_CACHE::join_null_complements");

  /* Return at once if there are no records in the join buffer */
  if (!records) DBUG_RETURN(NESTED_LOOP_OK);

  cnt = records - (is_key_access() ? 0 : skip_last);

  /* This function may be called only for inner tables of outer joins */
  DBUG_ASSERT(qep_tab->first_inner() != NO_PLAN_IDX);

  // Make sure that the rowid buffer is bound, duplicates weedout needs it
  if (qep_tab->copy_current_rowid &&
      !qep_tab->copy_current_rowid->buffer_is_bound())
    qep_tab->copy_current_rowid->bind_buffer(qep_tab->table()->file->ref);

  for (; cnt; cnt--) {
    if (join->thd->killed) {
      /* The user has aborted the execution of the query */
      join->thd->send_kill_message();
      rc = NESTED_LOOP_KILLED;
      goto finish;
    }
    /* Just skip the whole record if a match for it has been already found */
    if (!is_first_inner || !skip_record_if_match()) {
      get_record();
      /* The outer row is complemented by nulls for each inner table */
      restore_record(qep_tab->table(), s->default_values);
      qep_tab->table()->set_null_row();
      rc = generate_full_extensions(get_curr_rec());
      qep_tab->table()->reset_null_row();
      if (rc != NESTED_LOOP_OK) goto finish;
    }
  }

finish:
  DBUG_RETURN(rc);
}

/*
  Initialize retrieval of range sequence for BKA algorithm

  SYNOPSIS
    bka_range_seq_init()
     init_params   pointer to the BKA join cache object

  DESCRIPTION
    The function interprets init_param as a pointer to a JOIN_CACHE_BKA
    object. The function prepares for an iteration over the join keys
    built for all records from the cache join buffer.

  NOTE
    This function are used only as a callback function.

  RETURN
    init_param value that is to be used as a parameter of bka_range_seq_next()
*/

static range_seq_t bka_range_seq_init(void *init_param, uint, uint) {
  DBUG_ENTER("bka_range_seq_init");
  JOIN_CACHE_BKA *cache = (JOIN_CACHE_BKA *)init_param;
  cache->reset_cache(false);
  DBUG_RETURN((range_seq_t)init_param);
}

/*
  Get the key over the next record from the join buffer used by BKA

  SYNOPSIS
    bka_range_seq_next()
      seq    the value returned by  bka_range_seq_init
      range  OUT reference to the next range

  DESCRIPTION
    The function interprets seq as a pointer to a JOIN_CACHE_BKA
    object. The function returns a pointer to the range descriptor
    for the key built over the next record from the join buffer.

  NOTE
    This function are used only as a callback function.

  RETURN
    0   ok, the range structure filled with info about the next key
    1   no more ranges
*/

static uint bka_range_seq_next(range_seq_t rseq, KEY_MULTI_RANGE *range) {
  DBUG_ENTER("bka_range_seq_next");
  JOIN_CACHE_BKA *cache = (JOIN_CACHE_BKA *)rseq;
  TABLE_REF *ref = &cache->qep_tab->ref();
  key_range *start_key = &range->start_key;
  if ((start_key->length = cache->get_next_key((uchar **)&start_key->key))) {
    start_key->keypart_map = (1 << ref->key_parts) - 1;
    start_key->flag = HA_READ_KEY_EXACT;
    range->end_key = *start_key;
    range->end_key.flag = HA_READ_AFTER_KEY;
    range->ptr = (char *)cache->get_curr_rec();
    range->range_flag = EQ_RANGE;
    DBUG_RETURN(0);
  }
  DBUG_RETURN(1);
}

/*
  Check whether range_info orders to skip the next record from BKA buffer

  SYNOPSIS
    bka_range_seq_skip_record()
      seq              value returned by bka_range_seq_init()
      range_info       information about the next range


  DESCRIPTION
    The function interprets seq as a pointer to a JOIN_CACHE_BKA object.
    The function interprets seq as a pointer to the JOIN_CACHE_BKA_UNIQUE
    object. The function returns true if the record with this range_info
    is to be filtered out from the stream of records returned by
    ha_multi_range_read_next().

  NOTE
    This function are used only as a callback function.

  RETURN
    1    record with this range_info is to be filtered out from the stream
         of records returned by ha_multi_range_read_next()
    0    the record is to be left in the stream
*/

static bool bka_range_seq_skip_record(range_seq_t rseq, char *range_info,
                                      uchar *) {
  DBUG_ENTER("bka_range_seq_skip_record");
  JOIN_CACHE_BKA *cache = (JOIN_CACHE_BKA *)rseq;
  bool res = cache->get_match_flag_by_pos((uchar *)range_info);
  DBUG_RETURN(res);
}

/*
  Using BKA find matches from the next table for records from the join buffer

  SYNOPSIS
    join_matching_records()
      skip_last    do not look for matches for the last partial join record

  DESCRIPTION
    This function can be used only when the table join_tab can be accessed
    by keys built over the fields of previous join tables.
    The function retrieves all partial join records from the join buffer and
    for each of them builds the key value to access join_tab, performs index
    look-up with this key and selects matching records yielded by this look-up
    If a match is found the function will call the sub_select function trying
    to look for matches for the remaining join operations.
    This function currently is called only from the function join_records.
    It's assumed that this function is always called with the skip_last
    parameter equal to false.

  NOTES
    The function produces all matching extensions for the records in the
    join buffer following the path of the Batched Key Access algorithm.
    When an outer join operation is performed all unmatched records from
    the join buffer must be extended by null values. The function
    join_null_complements serves this purpose.
    The Batched Key Access algorithm assumes that key accesses are batched.
    In other words it assumes that, first, either keys themselves or the
    corresponding rowids (primary keys) are accumulated in a buffer, then
    data rows from  join_tab are fetched for all of them. When a row is
    fetched it is always returned with a reference to the key by which it
    has been accessed.
    When key values are batched we can save on the number of the server
    requests for index lookups. For the remote engines, like NDB cluster, it
    essentially reduces the number of round trips between the server and
    the engine when performing a join operation.
    When the rowids for the keys are batched we can optimize the order
    in what we fetch the data for this rowids. The performance benefits of
    this optimization can be significant for such engines as MyISAM, InnoDB.
    What is exactly batched are hidden behind implementations of
    MRR handler interface that is supposed to be appropriately chosen
    for each engine. If for a engine no specific implementation of the MRR
    interface is supllied then the default implementation is used. This
    implementation actually follows the path of Nested Loops Join algorithm.
    In this case BKA join surely will demonstrate a worse performance than
    NL join.

  RETURN
    return one of enum_nested_loop_state
*/

enum_nested_loop_state JOIN_CACHE_BKA::join_matching_records(
    bool skip_last MY_ATTRIBUTE((unused))) {
  /* The value of skip_last must be always false when this function is called */
  DBUG_ASSERT(!skip_last);

  /* Return at once if there are no records in the join buffer */
  if (!records) return NESTED_LOOP_OK;

  /* Set functions to iterate over keys in the join buffer */
  RANGE_SEQ_IF seq_funcs = {
      bka_range_seq_init, bka_range_seq_next,
      check_only_first_match ? bka_range_seq_skip_record : 0,
      qep_tab->cache_idx_cond ? bka_skip_index_tuple : 0};

  if (init_join_matching_records(&seq_funcs, records)) return NESTED_LOOP_ERROR;

  int error;
  handler *file = qep_tab->table()->file;
  enum_nested_loop_state rc = NESTED_LOOP_OK;
  uchar *rec_ptr = NULL;

  while (!(error = file->ha_multi_range_read_next((char **)&rec_ptr))) {
    if (join->thd->killed) {
      /* The user has aborted the execution of the query */
      join->thd->send_kill_message();
      return NESTED_LOOP_KILLED;
    }
    if (qep_tab->keep_current_rowid)
      qep_tab->table()->file->position(qep_tab->table()->record[0]);
    /*
      If only the first match is needed and it has been already found
      for the associated partial join record then the returned candidate
      is discarded.
    */
    if (rc == NESTED_LOOP_OK &&
        (!check_only_first_match || !get_match_flag_by_pos(rec_ptr))) {
      get_record_by_pos(rec_ptr);
      rc = generate_full_extensions(rec_ptr);
      if (rc != NESTED_LOOP_OK) return rc;
    }
  }

  if (error > 0 && error != HA_ERR_END_OF_FILE) return NESTED_LOOP_ERROR;
  return rc;
}

/*
  Prepare to search for records that match records from the join buffer

  SYNOPSIS
    init_join_matching_records()
      seq_funcs    structure of range sequence interface
      ranges       number of keys/ranges in the sequence

  DESCRIPTION
    This function calls the multi_range_read_init function to set up
    the BKA process of generating the keys from the records in the join
    buffer and looking for matching records from the table to be joined.
    The function passes as a parameter a structure of functions that
    implement the range sequence interface. This interface is used to
    enumerate all generated keys and optionally to filter the matching
    records returned by the ha_multi_range_read_next calls from the
    intended invocation of the join_matching_records method. The
    multi_range_read_init function also receives the parameters for
    MRR buffer to be used and flags specifying the mode in which
    this buffer will be functioning.
    The number of keys in the sequence expected by multi_range_read_init
    is passed through the parameter ranges.

  RETURN
    False if ok, True otherwise.
*/

bool JOIN_CACHE_BKA::init_join_matching_records(RANGE_SEQ_IF *seq_funcs,
                                                uint ranges) {
  handler *file = qep_tab->table()->file;

  /* Dynamic range access is never used with BKA */
  DBUG_ASSERT(!qep_tab->dynamic_range());

  init_mrr_buff();

  /*
    Prepare to iterate over keys from the join buffer and to get
    matching candidates obtained with MMR handler functions.
  */
  if (!file->inited) {
    const int error = file->ha_index_init(qep_tab->ref().key, 1);
    if (error) {
      file->print_error(error, MYF(0));
      return error;
    }
  }
  return file->multi_range_read_init(seq_funcs, (void *)this, ranges, mrr_mode,
                                     &mrr_buff);
}

/**
  Reads all flag fields of a positioned record from the join buffer.
  Including all flag fields (of this record) stored in the previous join
  buffers.

  @param rec_ptr  position of the first field of the record in the join buffer
 */
void JOIN_CACHE::read_all_flag_fields_by_pos(uchar *rec_ptr) {
  uchar *const save_pos = pos;
  pos = rec_ptr;
  read_some_flag_fields();  // moves 'pos'...
  pos = save_pos;           // ... so we restore it.
  if (prev_cache) {
    // position of this record in previous join buffer:
    rec_ptr = prev_cache->get_rec_ref(rec_ptr);
    // recurse into previous buffer to read missing flag fields
    prev_cache->read_all_flag_fields_by_pos(rec_ptr);
  }
}

/*
  Get the key built over the next record from BKA join buffer

  SYNOPSIS
    get_next_key()
      key    pointer to the buffer where the key value is to be placed

  DESCRIPTION
    The function reads key fields from the current record in the join buffer.
    and builds the key value out of these fields that will be used to access
    the 'join_tab' table. Some of key fields may belong to previous caches.
    They are accessed via record references to the record parts stored in the
    previous join buffers. The other key fields always are placed right after
    the flag fields of the record.
    If the key is embedded, which means that its value can be read directly
    from the join buffer, then *key is set to the beginning of the key in
    this buffer. Otherwise the key is built in the join_tab->ref()->key_buff.
    The function returns the length of the key if it succeeds ro read it.
    If is assumed that the functions starts reading at the position of
    the record length which is provided for each records in a BKA cache.
    After the key is built the 'pos' value points to the first position after
    the current record.
    The function returns 0 if the initial position is after the beginning
    of the record fields for last record from the join buffer.

  RETURN
    length of the key value - if the starting value of 'pos' points to
    the position before the fields for the last record,
    0 - otherwise.
*/

uint JOIN_CACHE_BKA::get_next_key(uchar **key) {
  uint len;
  uint32 rec_len;
  uchar *init_pos;
  JOIN_CACHE *cache;

  if (records == 0) return 0;

  /* Any record in a BKA cache is prepended with its length, which we need */
  DBUG_ASSERT(with_length);

  /*
    Read keys until find non-ignorable one or EOF.
    Unlike in JOIN_CACHE::read_some_record_fields()), pos>=last_rec_pos means
    EOF, because we are not at fields' start, and previous record's fields
    might be empty.
  */
  for (len = 0; (len == 0) && pos < last_rec_pos; pos = init_pos + rec_len) {
    /* Read the length of the record */
    rec_len = get_rec_length(pos);
    pos += size_of_rec_len;
    init_pos = pos;

    /* Read a reference to the previous cache if any */
    uchar *prev_rec_ptr = NULL;
    if (prev_cache) {
      pos += prev_cache->get_size_of_rec_offset();
      // position of this record in previous buffer:
      prev_rec_ptr = prev_cache->get_rec_ref(pos);
    }

    curr_rec_pos = pos;

    // Read all flag fields of the record, in two steps:
    read_some_flag_fields();  // 1) flag fields stored in this buffer
    if (prev_cache)           // 2) flag fields stored in previous buffers
      prev_cache->read_all_flag_fields_by_pos(prev_rec_ptr);

    if (use_emb_key) {
      /* An embedded key is taken directly from the join buffer */
      *key = pos;
      len = emb_key_length;
      DBUG_ASSERT(len != 0);
    } else {
      /*
        Read key arguments from previous caches if there are any such
        fields
      */
      if (external_key_arg_fields) {
        uchar *rec_ptr = curr_rec_pos;
        uint key_arg_count = external_key_arg_fields;
        CACHE_FIELD **copy_ptr = blob_ptr - key_arg_count;
        for (cache = prev_cache; key_arg_count; cache = cache->prev_cache) {
          uint len2 = 0;
          DBUG_ASSERT(cache);
          rec_ptr = cache->get_rec_ref(rec_ptr);
          while (!cache->referenced_fields) {
            cache = cache->prev_cache;
            DBUG_ASSERT(cache);
            rec_ptr = cache->get_rec_ref(rec_ptr);
          }
          while (key_arg_count &&
                 cache->read_referenced_field(*copy_ptr, rec_ptr, &len2)) {
            copy_ptr++;
            --key_arg_count;
          }
        }
      }

      /*
         Read the other key arguments from the current record. The fields for
         these arguments are always first in the sequence of the record's
         fields.
      */
      CACHE_FIELD *copy = field_descr + flag_fields;
      CACHE_FIELD *copy_end = copy + local_key_arg_fields;
      bool blob_in_rec_buff = blob_data_is_in_rec_buff(curr_rec_pos);
      for (; copy < copy_end; copy++) read_record_field(copy, blob_in_rec_buff);

      TABLE_REF *ref = &qep_tab->ref();
      if (ref->impossible_null_ref()) {
        DBUG_PRINT("info", ("JOIN_CACHE_BKA::get_next_key null_rejected"));
        /* this key cannot give a match, don't collect it, go read next key */
        len = 0;
      } else {
        /* Build the key over the fields read into the record buffers */
        cp_buffer_from_ref(join->thd, qep_tab->table(), ref);
        *key = ref->key_buff;
        len = ref->key_length;
        DBUG_ASSERT(len != 0);
      }
    }
  }
  return len;
}

/*
  Initialize a BKA_UNIQUE cache

  SYNOPSIS
    init()

  DESCRIPTION
    The function initializes the cache structure. It supposed to be called
    right after a constructor for the JOIN_CACHE_BKA_UNIQUE.
    The function allocates memory for the join buffer and for descriptors of
    the record fields stored in the buffer.
    The function also estimates the number of hash table entries in the hash
    table to be used and initializes this hash table.

  NOTES
    The code of this function should have been included into the constructor
    code itself. However the new operator for the class JOIN_CACHE_BKA_UNIQUE
    would never fail while memory allocation for the join buffer is not
    absolutely unlikely to fail. That's why this memory allocation has to be
    placed in a separate function that is called in a couple with a cache
    constructor.
    It is quite natural to put almost all other constructor actions into
    this function.

  RETURN
    0   initialization with buffer allocations has been succeeded
    1   otherwise
*/

int JOIN_CACHE_BKA_UNIQUE::init() {
  int rc = 0;
  TABLE_REF *ref = &qep_tab->ref();

  DBUG_ENTER("JOIN_CACHE_BKA_UNIQUE::init");

  hash_table = 0;
  key_entries = 0;

  if ((rc = JOIN_CACHE_BKA::init())) DBUG_RETURN(rc);

  key_length = ref->key_length;

  /* Take into account a reference to the next record in the key chain */
  pack_length += get_size_of_rec_offset();

  /* Calculate the minimal possible value of size_of_key_ofs greater than 1 */
  uint max_size_of_key_ofs = max(2U, get_size_of_rec_offset());
  for (size_of_key_ofs = 2; size_of_key_ofs <= max_size_of_key_ofs;
       size_of_key_ofs += 2) {
    key_entry_length = get_size_of_rec_offset() +  // key chain header
                       size_of_key_ofs +           // reference to the next key
                       (use_emb_key ? get_size_of_rec_offset() : key_length);

    uint n = buff_size / (pack_length + key_entry_length + size_of_key_ofs);

    /*
      TODO: Make a better estimate for this upper bound of
            the number of records in in the join buffer.
    */
    uint max_n =
        buff_size / (pack_length - length + key_entry_length + size_of_key_ofs);

    hash_entries = (uint)(n / 0.7);

    if (offset_size(max_n * key_entry_length) <= size_of_key_ofs) break;
  }

  /* Initialize the hash table */
  hash_table = buff + (buff_size - hash_entries * size_of_key_ofs);
  cleanup_hash_table();
  curr_key_entry = hash_table;

  pack_length += key_entry_length;
  pack_length_with_blob_ptrs += get_size_of_rec_offset() + key_entry_length;

  rec_fields_offset = get_size_of_rec_offset() + get_size_of_rec_length() +
                      (prev_cache ? prev_cache->get_size_of_rec_offset() : 0);

  data_fields_offset = 0;
  if (use_emb_key) {
    CACHE_FIELD *copy = field_descr;
    CACHE_FIELD *copy_end = copy + flag_fields;
    for (; copy < copy_end; copy++) data_fields_offset += copy->length;
  }

  DBUG_RETURN(rc);
}

void JOIN_CACHE_BKA_UNIQUE::reset_cache(bool for_writing) {
  JOIN_CACHE_BKA::reset_cache(for_writing);
  if (for_writing && hash_table) cleanup_hash_table();
  curr_key_entry = hash_table;
}

/*
  Add a record into the JOIN_CACHE_BKA_UNIQUE buffer

  SYNOPSIS
    put_record()

  DESCRIPTION
    This implementation of the virtual function put_record writes the next
    matching record into the join buffer of the JOIN_CACHE_BKA_UNIQUE class.
    Additionally to what the default implementation does this function
    performs the following.
    It extracts from the record the key value used in lookups for matching
    records and searches for this key in the hash tables from the join cache.
    If it finds the key in the hash table it joins the record to the chain
    of records with this key. If the key is not found in the hash table the
    key is placed into it and a chain containing only the newly added record
    is attached to the key entry. The key value is either placed in the hash
    element added for the key or, if the use_emb_key flag is set, remains in
    the record from the partial join.

  RETURN
    true    if it has been decided that it should be the last record
            in the join buffer,
    false   otherwise
*/

bool JOIN_CACHE_BKA_UNIQUE::put_record_in_cache() {
  uchar *key;
  uint key_len = key_length;
  uchar *key_ref_ptr;
  TABLE_REF *ref = &qep_tab->ref();
  uchar *next_ref_ptr = pos;
  pos += get_size_of_rec_offset();

  // Write record to join buffer
  bool is_full = JOIN_CACHE::put_record_in_cache();

  if (use_emb_key) {
    key = get_curr_emb_key();
    // Embedded is not used if one of the key columns is nullable
  } else {
    /* Build the key over the fields read into the record buffers */
    cp_buffer_from_ref(join->thd, qep_tab->table(), ref);
    key = ref->key_buff;
    if (ref->impossible_null_ref()) {
      /*
        The row just put into the buffer has a NULL-value for one of
        the ref-columns and the ref access is NULL-rejecting, this key cannot
        give a match. So we don't insert it into the hash table.
        We still stored the record into the buffer (put_record() call above),
        or we would later miss NULL-complementing of this record.
      */
      DBUG_PRINT("info", ("JOIN_CACHE_BKA_UNIQUE::put_record null_rejected"));
      return is_full;
    }
  }

  /* Look for the key in the hash table */
  if (key_search(key, key_len, &key_ref_ptr)) {
    uchar *last_next_ref_ptr;
    /*
      The key is found in the hash table.
      Add the record to the circular list of the records attached to this key.
      Below 'rec' is the record to be added into the record chain for the found
      key, 'key_ref' points to a flatten representation of the st_key_entry
      structure that contains the key and the head of the record chain.
    */
    last_next_ref_ptr =
        get_next_rec_ref(key_ref_ptr + get_size_of_key_offset());
    /* rec->next_rec= key_entry->last_rec->next_rec */
    memcpy(next_ref_ptr, last_next_ref_ptr, get_size_of_rec_offset());
    /* key_entry->last_rec->next_rec= rec */
    store_next_rec_ref(last_next_ref_ptr, next_ref_ptr);
    /* key_entry->last_rec= rec */
    store_next_rec_ref(key_ref_ptr + get_size_of_key_offset(), next_ref_ptr);
  } else {
    /*
      The key is not found in the hash table.
      Put the key into the join buffer linking it with the keys for the
      corresponding hash entry. Create a circular list with one element
      referencing the record and attach the list to the key in the buffer.
    */
    uchar *cp = last_key_entry;
    cp -= get_size_of_rec_offset() + get_size_of_key_offset();
    store_next_key_ref(key_ref_ptr, cp);
    store_null_key_ref(cp);
    store_next_rec_ref(next_ref_ptr, next_ref_ptr);
    store_next_rec_ref(cp + get_size_of_key_offset(), next_ref_ptr);
    if (use_emb_key) {
      cp -= get_size_of_rec_offset();
      store_emb_key_ref(cp, key);
    } else {
      cp -= key_len;
      memcpy(cp, key, key_len);
    }
    last_key_entry = cp;
    /* Increment the counter of key_entries in the hash table */
    key_entries++;
  }
  return is_full;
}

/*
  Read the next record from the JOIN_CACHE_BKA_UNIQUE buffer

  SYNOPSIS
    get_record()

  DESCRIPTION
    Additionally to what the default implementation of the virtual
    function get_record does this implementation skips the link element
    used to connect the records with the same key into a chain.

  RETURN
    true  - there are no more records to read from the join buffer
    false - otherwise
*/

bool JOIN_CACHE_BKA_UNIQUE::get_record() {
  pos += get_size_of_rec_offset();
  return this->JOIN_CACHE::get_record();
}

/*
  Skip record from the JOIN_CACHE_BKA_UNIQUE join buffer if its match flag is on

  SYNOPSIS
    skip_record_if_match()

  DESCRIPTION
    This implementation of the virtual function skip_record_if_match does
    the same as the default implementation does, but it takes into account
    the link element used to connect the records with the same key into a chain.

  RETURN
    true  - the match flag is on and the record has been skipped
    false - the match flag is off
*/

bool JOIN_CACHE_BKA_UNIQUE::skip_record_if_match() {
  uchar *save_pos = pos;
  pos += get_size_of_rec_offset();
  if (!this->JOIN_CACHE::skip_record_if_match()) {
    pos = save_pos;
    return false;
  }
  return true;
}

/**
  Search for a key in the hash table of the join buffer.

  @param key              pointer to the key value
  @param key_len          key value length
  @param[out] key_ref_ptr position of the reference to the next key from
                          the hash element for the found key, or a
                          position where the reference to the hash
                          element for the key is to be added in the
                          case when the key has not been found
  @details
    The function looks for a key in the hash table of the join buffer.
    If the key is found the functionreturns the position of the reference
    to the next key from  to the hash element for the given key.
    Otherwise the function returns the position where the reference to the
    newly created hash element for the given key is to be added.

  @return whether the key is found in the hash table
*/

bool JOIN_CACHE_BKA_UNIQUE::key_search(uchar *key, uint key_len,
                                       uchar **key_ref_ptr) {
  bool is_found = false;
  uint idx = get_hash_idx(key, key_length);
  uchar *ref_ptr = hash_table + size_of_key_ofs * idx;
  while (!is_null_key_ref(ref_ptr)) {
    uchar *next_key;
    ref_ptr = get_next_key_ref(ref_ptr);
    next_key = use_emb_key ? get_emb_key(ref_ptr - get_size_of_rec_offset())
                           : ref_ptr - key_length;

    if (memcmp(next_key, key, key_len) == 0) {
      is_found = true;
      break;
    }
  }
  *key_ref_ptr = ref_ptr;
  return is_found;
}

/*
  Calclulate hash value for a key in the hash table of the join buffer

  SYNOPSIS
    get_hash_idx()
      key             pointer to the key value
      key_len         key value length

  DESCRIPTION
    The function calculates an index of the hash entry in the hash table
    of the join buffer for the given key

  RETURN
    the calculated index of the hash entry for the given key.
*/

uint JOIN_CACHE_BKA_UNIQUE::get_hash_idx(uchar *key, uint key_len) {
  ulong nr = 1;
  ulong nr2 = 4;
  uchar *position = key;
  uchar *end = key + key_len;
  for (; position < end; position++) {
    nr ^= (ulong)((((uint)nr & 63) + nr2) * ((uint)*position)) + (nr << 8);
    nr2 += 3;
  }
  return nr % hash_entries;
}

/*
  Clean up the hash table of the join buffer

  SYNOPSIS
    cleanup_hash_table()
      key             pointer to the key value
      key_len         key value length

  DESCRIPTION
    The function cleans up the hash table in the join buffer removing all
    hash elements from the table.

  RETURN
    none
*/

void JOIN_CACHE_BKA_UNIQUE::cleanup_hash_table() {
  last_key_entry = hash_table;
  memset(hash_table, 0, (buff + buff_size) - hash_table);
  key_entries = 0;
}

/*
  Initialize retrieval of range sequence for BKA_UNIQUE algorithm

  SYNOPSIS
    bka_range_seq_init()
      init_params   pointer to the BKA_INIQUE join cache object

  DESCRIPTION
    The function interprets init_param as a pointer to a JOIN_CACHE_BKA_UNIQUE
    object. The function prepares for an iteration over the unique join keys
    built over the records from the cache join buffer.

  NOTE
    This function are used only as a callback function.

  RETURN
    init_param    value that is to be used as a parameter of
                  bka_unique_range_seq_next()
*/

static range_seq_t bka_unique_range_seq_init(void *init_param, uint, uint) {
  DBUG_ENTER("bka_unique_range_seq_init");
  JOIN_CACHE_BKA_UNIQUE *cache = (JOIN_CACHE_BKA_UNIQUE *)init_param;
  cache->reset_cache(false);
  DBUG_RETURN((range_seq_t)init_param);
}

/*
  Get the key over the next record from the join buffer used by BKA_UNIQUE

  SYNOPSIS
    bka_unique_range_seq_next()
      seq        value returned by  bka_unique_range_seq_init()
      range  OUT reference to the next range

  DESCRIPTION
    The function interprets seq as a pointer to the JOIN_CACHE_BKA_UNIQUE
    object. The function returns a pointer to the range descriptor
    for the next unique key built over records from the join buffer.

  NOTE
    This function are used only as a callback function.

  RETURN
    0    ok, the range structure filled with info about the next key
    1    no more ranges
*/

static uint bka_unique_range_seq_next(range_seq_t rseq,
                                      KEY_MULTI_RANGE *range) {
  DBUG_ENTER("bka_unique_range_seq_next");
  JOIN_CACHE_BKA_UNIQUE *cache = (JOIN_CACHE_BKA_UNIQUE *)rseq;
  TABLE_REF *ref = &cache->qep_tab->ref();
  key_range *start_key = &range->start_key;
  if ((start_key->length = cache->get_next_key((uchar **)&start_key->key))) {
    start_key->keypart_map = (1 << ref->key_parts) - 1;
    start_key->flag = HA_READ_KEY_EXACT;
    range->end_key = *start_key;
    range->end_key.flag = HA_READ_AFTER_KEY;
    range->ptr = (char *)cache->get_curr_key_chain();
    range->range_flag = EQ_RANGE;
    DBUG_RETURN(0);
  }
  DBUG_RETURN(1);
}

/*
  Check whether range_info orders to skip the next record from BKA_UNIQUE buffer

  SYNOPSIS
    bka_unique_range_seq_skip_record()
      seq              value returned by bka_unique_range_seq_init()
      range_info       information about the next range

  DESCRIPTION
    The function interprets seq as a pointer to the JOIN_CACHE_BKA_UNIQUE
    object. The function returns true if the record with this range_info
    is to be filtered out from the stream of records returned by
    ha_multi_range_read_next().

  NOTE
    This function are used only as a callback function.

  RETURN
    1    record with this range_info is to be filtered out from the stream
         of records returned by ha_multi_range_read_next()
    0    the record is to be left in the stream
*/

static bool bka_unique_range_seq_skip_record(range_seq_t rseq, char *range_info,
                                             uchar *) {
  DBUG_ENTER("bka_unique_range_seq_skip_record");
  JOIN_CACHE_BKA_UNIQUE *cache = (JOIN_CACHE_BKA_UNIQUE *)rseq;
  bool res = cache->check_all_match_flags_for_key((uchar *)range_info);
  DBUG_RETURN(res);
}

/**
  Check if the record combination matches the index condition

  @param  rseq             Value returned by bka_range_seq_init()
  @param  range_info       MRR range association data

  @sa JOIN_CACHE_BKA::skip_index_tuple().
  This function is the variant for use with
  JOIN_CACHE_BKA_UNIQUE. The difference from JOIN_CACHE_BKA case is that
  there may be multiple previous table record combinations that share the
  same key, i.e. they map to the same MRR range. And for all of those
  records, we have just done one single key lookup in the current table,
  found an index tuple. If in this function we discard this index tuple, all
  those records will be eliminated from the result. Thus, in this function
  we can discard the index tuple only if _all_ those cached records and the
  index tuple don't match the pushed index condition. It's a "group-wide
  decision".
  Thus we must here loop through all previous table records combinations
  that match the given MRR range key range_info, searching for a single one
  matching the index condition.
  If we find none, we can safely discard the index tuple here, which avoids
  retrieving the record from the current table.
  If we instead find one, we cannot discard the index tuple here; later in
  execution, in join_matching_records(), we can finally take one
  "case-by-case decision" per cached record, by checking again the index
  condition (@sa JOIN_CACHE_BKA_UNIQUE::check_match).

  @note
  Possible optimization:
  Before we unpack the record from a previous table
  check if this table is used in the condition.
  If so then unpack the record otherwise skip the unpacking.
  This should be done by a special virtual method
  get_partial_record_by_pos().

  @retval false  The record combination satisfies the index condition
  @retval true   Otherwise


*/

bool JOIN_CACHE_BKA_UNIQUE::skip_index_tuple(range_seq_t rseq,
                                             char *range_info) {
  DBUG_ENTER("JOIN_CACHE_BKA_UNIQUE::skip_index_tuple");
  JOIN_CACHE_BKA_UNIQUE *cache = (JOIN_CACHE_BKA_UNIQUE *)rseq;
  uchar *last_rec_ref_ptr = cache->get_next_rec_ref((uchar *)range_info);
  uchar *next_rec_ref_ptr = last_rec_ref_ptr;
  do {
    next_rec_ref_ptr = cache->get_next_rec_ref(next_rec_ref_ptr);
    uchar *rec_ptr = next_rec_ref_ptr + cache->rec_fields_offset;
    cache->get_record_by_pos(rec_ptr);
    if (qep_tab->cache_idx_cond->val_int()) DBUG_RETURN(false);
  } while (next_rec_ref_ptr != last_rec_ref_ptr);
  DBUG_RETURN(true);
}

/*
  Check if the record combination matches the index condition

  SYNOPSIS
    bka_unique_skip_index_tuple()
      rseq             Value returned by bka_range_seq_init()
      range_info       MRR range association data

  DESCRIPTION
    This is wrapper for JOIN_CACHE_BKA_UNIQUE::skip_index_tuple method,
    see comments there.

  NOTE
    This function is used as a RANGE_SEQ_IF::skip_index_tuple callback.

  RETURN
    0    The record combination satisfies the index condition
    1    Otherwise
*/

static bool bka_unique_skip_index_tuple(range_seq_t rseq, char *range_info) {
  DBUG_ENTER("bka_unique_skip_index_tuple");
  JOIN_CACHE_BKA_UNIQUE *cache = (JOIN_CACHE_BKA_UNIQUE *)rseq;
  DBUG_RETURN(cache->skip_index_tuple(rseq, range_info));
}

/*
  Using BKA_UNIQUE find matches from the next table for records from join buffer

  SYNOPSIS
    join_matching_records()
      skip_last    do not look for matches for the last partial join record

  DESCRIPTION
    This function can be used only when the table join_tab can be accessed
    by keys built over the fields of previous join tables.
    The function retrieves all keys from the hash table of the join buffer
    built for partial join records from the buffer. For each of these keys
    the function performs an index lookup and tries to match records yielded
    by this lookup with records from the join buffer attached to the key.
    If a match is found the function will call the sub_select function trying
    to look for matches for the remaining join operations.
    This function does not assume that matching records are necessarily
    returned with references to the keys by which they were found. If the call
    of the function multi_range_read_init returns flags with
    HA_MRR_NO_ASSOCIATION then a search for the key built from the returned
    record is carried on. The search is performed by probing in in the hash
    table of the join buffer.
    This function currently is called only from the function join_records.
    It's assumed that this function is always called with the skip_last
    parameter equal to false.

  RETURN
    return one of enum_nested_loop_state
*/

enum_nested_loop_state JOIN_CACHE_BKA_UNIQUE::join_matching_records(
    bool skip_last MY_ATTRIBUTE((unused))) {
  /* The value of skip_last must be always false when this function is called */
  DBUG_ASSERT(!skip_last);

  /* Return at once if there are no records in the join buffer */
  if (!records) return NESTED_LOOP_OK;

  const bool no_association = mrr_mode & HA_MRR_NO_ASSOCIATION;
  /* Set functions to iterate over keys in the join buffer */
  RANGE_SEQ_IF seq_funcs = {
      bka_unique_range_seq_init, bka_unique_range_seq_next,
      check_only_first_match && !no_association
          ? bka_unique_range_seq_skip_record
          : 0,
      qep_tab->cache_idx_cond ? bka_unique_skip_index_tuple : 0};

  if (init_join_matching_records(&seq_funcs, key_entries))
    return NESTED_LOOP_ERROR;

  int error;
  uchar *key_chain_ptr;
  handler *file = qep_tab->table()->file;
  enum_nested_loop_state rc = NESTED_LOOP_OK;

  while (!(error = file->ha_multi_range_read_next((char **)&key_chain_ptr))) {
    TABLE *table = qep_tab->table();
    if (no_association) {
      uchar *key_ref_ptr;
      TABLE_REF *ref = &qep_tab->ref();
      KEY *keyinfo = table->key_info + ref->key;
      /*
        Build the key value out of  the record returned by the call of
        ha_multi_range_read_next in the record buffer
      */
      key_copy(ref->key_buff, table->record[0], keyinfo, ref->key_length);
      /* Look for this key in the join buffer */
      if (!key_search(ref->key_buff, ref->key_length, &key_ref_ptr)) continue;
      key_chain_ptr = key_ref_ptr + get_size_of_key_offset();
    }

    if (qep_tab->keep_current_rowid) table->file->position(table->record[0]);

    uchar *last_rec_ref_ptr = get_next_rec_ref(key_chain_ptr);
    uchar *next_rec_ref_ptr = last_rec_ref_ptr;
    do {
      next_rec_ref_ptr = get_next_rec_ref(next_rec_ref_ptr);
      uchar *rec_ptr = next_rec_ref_ptr + rec_fields_offset;

      if (join->thd->killed) {
        /* The user has aborted the execution of the query */
        join->thd->send_kill_message();
        return NESTED_LOOP_KILLED;
      }
      /*
        If only the first match is needed and it has been already found
        for the associated partial join record then the returned candidate
        is discarded.
      */
      if (rc == NESTED_LOOP_OK &&
          (!check_only_first_match || !get_match_flag_by_pos(rec_ptr))) {
        get_record_by_pos(rec_ptr);
        rc = generate_full_extensions(rec_ptr);
        if (rc != NESTED_LOOP_OK) return rc;
      }
    } while (next_rec_ref_ptr != last_rec_ref_ptr);
  }

  if (error > 0 && error != HA_ERR_END_OF_FILE) return NESTED_LOOP_ERROR;
  return rc;
}

/**
  Check whether all records in a key chain are flagged as matches.

  @param key_chain_ptr key chain
  @return whether each record in the key chain has been flagged as a match

  @details
    This function retrieves records in the given circular chain and checks
    whether their match flags are set on. The parameter key_chain_ptr shall
    point to the position in the join buffer storing the reference to the
    last element of this chain.
*/

bool JOIN_CACHE_BKA_UNIQUE::check_all_match_flags_for_key(
    uchar *key_chain_ptr) {
  uchar *last_rec_ref_ptr = get_next_rec_ref(key_chain_ptr);
  uchar *next_rec_ref_ptr = last_rec_ref_ptr;
  do {
    next_rec_ref_ptr = get_next_rec_ref(next_rec_ref_ptr);
    uchar *rec_ptr = next_rec_ref_ptr + rec_fields_offset;
    if (!get_match_flag_by_pos(rec_ptr)) return false;
  } while (next_rec_ref_ptr != last_rec_ref_ptr);
  return true;
}

/*
  Get the next key built for the records from BKA_UNIQUE join buffer

  SYNOPSIS
    get_next_key()
      key    pointer to the buffer where the key value is to be placed

  DESCRIPTION
    The function reads the next key value stored in the hash table of the
    join buffer. Depending on the value of the use_emb_key flag of the
    join cache the value is read either from the table itself or from
    the record field where it occurs.

  RETURN
    length of the key value - if the starting value of 'cur_key_entry' refers
    to the position after that referred by the value of 'last_key_entry'
    0 - otherwise.
*/

uint JOIN_CACHE_BKA_UNIQUE::get_next_key(uchar **key) {
  if (curr_key_entry == last_key_entry) return 0;

  curr_key_entry -= key_entry_length;

  *key = use_emb_key ? get_emb_key(curr_key_entry) : curr_key_entry;

  DBUG_ASSERT(*key >= buff && *key < hash_table);

  return key_length;
}

/**
  Check matching to a partial join record from the join buffer, an
  implementation specialized for JOIN_CACHE_BKA_UNIQUE.
  Only JOIN_CACHE_BKA_UNIQUE needs that, because it's the only cache using
  distinct keys.
  JOIN_CACHE_BKA, on the other hand, does one key lookup per cached
  record, so can take a per-record individualized decision for the pushed
  index condition as soon as it has the index tuple.
  @sa JOIN_CACHE_BKA_UNIQUE::skip_index_tuple
  @sa JOIN_CACHE::check_match
 */
bool JOIN_CACHE_BKA_UNIQUE::check_match(uchar *rec_ptr) {
  /* recheck pushed down index condition */
  if (qep_tab->cache_idx_cond != NULL && !qep_tab->cache_idx_cond->val_int())
    return false;
  /* continue with generic tests */
  return JOIN_CACHE_BKA::check_match(rec_ptr);
}

/**
  @} (end of group Query_Optimizer)
*/

/****************************************************************************
 * Join cache module end
 ****************************************************************************/
