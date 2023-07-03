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

#include "field_types.h"
#include "my_alloc.h"
#include "my_base.h"
#include "my_bitmap.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "sql/check_stack.h"
#include "sql/field.h"
#include "sql/handler.h"
#include "sql/item.h"
#include "sql/mem_root_array.h"
#include "sql/partition_info.h"
#include "sql/psi_memory_key.h"
#include "sql/range_optimizer/internal.h"
#include "sql/range_optimizer/range_analysis.h"
#include "sql/range_optimizer/range_optimizer.h"
#include "sql/range_optimizer/tree.h"
#include "sql/sql_class.h"
#include "sql/sql_const.h"
#include "sql/sql_lex.h"
#include "sql/sql_list.h"
#include "sql/sql_partition.h"
#include "sql/system_variables.h"
#include "sql/table.h"
#include "sql/thr_malloc.h"

using opt_range::null_element;

/*
  PartitionPruningModule

  This part of the code does partition pruning. Partition pruning solves the
  following problem: given a query over partitioned tables, find partitions
  that we will not need to access (i.e. partitions that we can assume to be
  empty) when executing the query.
  The set of partitions to prune doesn't depend on which query execution
  plan will be used to execute the query.

  HOW IT WORKS

  Partition pruning module makes use of RangeAnalysisModule. The following
  examples show how the problem of partition pruning can be reduced to the
  range analysis problem:

  EXAMPLE 1
    Consider a query:

      SELECT * FROM t1 WHERE (t1.a < 5 OR t1.a = 10) AND t1.a > 3 AND t1.b='z'

    where table t1 is partitioned using PARTITION BY RANGE(t1.a).  An apparent
    way to find the used (i.e. not pruned away) partitions is as follows:

    1. analyze the WHERE clause and extract the list of intervals over t1.a
       for the above query we will get this list: {(3 < t1.a < 5), (t1.a=10)}

    2. for each interval I
       {
         find partitions that have non-empty intersection with I;
         mark them as used;
       }

  EXAMPLE 2
    Suppose the table is partitioned by HASH(part_func(t1.a, t1.b)). Then
    we need to:

    1. Analyze the WHERE clause and get a list of intervals over (t1.a, t1.b).
       The list of intervals we'll obtain will look like this:
       ((t1.a, t1.b) = (1,'foo')),
       ((t1.a, t1.b) = (2,'bar')),
       ((t1,a, t1.b) > (10,'zz'))

    2. for each interval I
       {
         if (the interval has form "(t1.a, t1.b) = (const1, const2)" )
         {
           calculate HASH(part_func(t1.a, t1.b));
           find which partition has records with this hash value and mark
             it as used;
         }
         else
         {
           mark all partitions as used;
           break;
         }
       }

   For both examples the step #1 is exactly what RangeAnalysisModule could
   be used to do, if it was provided with appropriate index description
   (array of KEY_PART structures).
   In example #1, we need to provide it with description of index(t1.a),
   in example #2, we need to provide it with description of index(t1.a, t1.b).

   These index descriptions are further called "partitioning index
   descriptions". Note that it doesn't matter if such indexes really exist,
   as range analysis module only uses the description.

   Putting it all together, partitioning module works as follows:

   prune_partitions() {
     call create_partition_index_description();

     call get_mm_tree(); // invoke the RangeAnalysisModule

     // analyze the obtained interval list and get used partitions
     call find_used_partitions();
  }

*/

typedef void (*mark_full_part_func)(partition_info *, uint32);

/*
  Partition pruning operation context
*/
struct PART_PRUNE_PARAM {
  RANGE_OPT_PARAM range_param; /* Range analyzer parameters */

  /***************************************************************
   Following fields are filled in based solely on partitioning
   definition and not modified after that:
   **************************************************************/
  partition_info *part_info; /* Copy of table->part_info */
  /* Function to get partition id from partitioning fields only */
  get_part_id_func get_top_partition_id_func;
  /* Function to mark a partition as used (w/all subpartitions if they exist)*/
  mark_full_part_func mark_full_partition_used;

  /* Partitioning 'index' description, array of key parts */
  KEY_PART *key;

  /*
    Number of fields in partitioning 'index' definition created for
    partitioning (0 if partitioning 'index' doesn't include partitioning
    fields)
  */
  uint part_fields;
  uint subpart_fields; /* Same as above for subpartitioning */

  /*
    Number of the last partitioning field keypart in the index, or -1 if
    partitioning index definition doesn't include partitioning fields.
  */
  int last_part_partno;
  int last_subpart_partno; /* Same as above for supartitioning */

  /*
    is_part_keypart[i] == test(keypart #i in partitioning index is a member
                               used in partitioning)
    Used to maintain current values of cur_part_fields and cur_subpart_fields
  */
  bool *is_part_keypart;
  /* Same as above for subpartitioning */
  bool *is_subpart_keypart;

  bool ignore_part_fields; /* Ignore rest of partitioning fields */

  /***************************************************************
   Following fields form find_used_partitions() recursion context:
   **************************************************************/
  SEL_ARG **arg_stack;     /* "Stack" of SEL_ARGs */
  SEL_ARG **arg_stack_end; /* Top of the stack    */
  /* Number of partitioning fields for which we have a SEL_ARG* in arg_stack */
  uint cur_part_fields;
  /* Same as cur_part_fields, but for subpartitioning */
  uint cur_subpart_fields;

  /* Iterator to be used to obtain the "current" set of used partitions */
  PARTITION_ITERATOR part_iter;

  /* Initialized bitmap of num_subparts size */
  MY_BITMAP subparts_bitmap;

  /* Used to store 'current key tuples' */
  uchar min_key[MAX_KEY_LENGTH + MAX_FIELD_WIDTH];
  uchar max_key[MAX_KEY_LENGTH + MAX_FIELD_WIDTH];

  uchar *cur_min_key;
  uchar *cur_max_key;

  uint cur_min_flag, cur_max_flag;
};

static bool create_partition_index_description(PART_PRUNE_PARAM *prune_par);
static int find_used_partitions(THD *thd, PART_PRUNE_PARAM *ppar,
                                SEL_ROOT *key_tree);
static int find_used_partitions(THD *thd, PART_PRUNE_PARAM *ppar,
                                SEL_ROOT::Type type, SEL_ARG *key_tree);
static int find_used_partitions_imerge(THD *thd, PART_PRUNE_PARAM *ppar,
                                       SEL_IMERGE *imerge);
static int find_used_partitions_imerge_list(THD *thd, PART_PRUNE_PARAM *ppar,
                                            List<SEL_IMERGE> &merges);
static void mark_all_partitions_as_used(partition_info *part_info);

#ifndef NDEBUG
static void print_partitioning_index(KEY_PART *parts, KEY_PART *parts_end);
static void dbug_print_segment_range(SEL_ARG *arg, KEY_PART *part);
static void dbug_print_singlepoint_range(SEL_ARG **start, uint num);
#endif

/**
  Perform partition pruning for a given table and condition.

  @param      thd            Thread handle
  @param      table          Table to perform partition pruning for
  @param      query_block     Query block the table is part of
  @param      pprune_cond    Condition to use for partition pruning

  @note This function assumes that lock_partitions are setup when it
  is invoked. The function analyzes the condition, finds partitions that
  need to be used to retrieve the records that match the condition, and
  marks them as used by setting appropriate bit in part_info->read_partitions
  In the worst case all partitions are marked as used. If the table is not
  yet locked, it will also unset bits in part_info->lock_partitions that is
  not set in read_partitions.

  This function returns promptly if called for non-partitioned table.

  @return Operation status
    @retval true  Failure
    @retval false Success
*/

bool prune_partitions(THD *thd, TABLE *table, Query_block *query_block,
                      Item *pprune_cond) {
  partition_info *part_info = table->part_info;
  DBUG_TRACE;

  /*
    If the prepare stage already have completed pruning successfully,
    it is no use of running prune_partitions() again on the same condition.
    Since it will not be able to prune anything more than the previous call
    from the prepare step.
  */
  if (part_info && part_info->is_pruning_completed) return false;

  table->all_partitions_pruned_away = false;

  if (!part_info) return false; /* not a partitioned table */

  if (table->s->db_type()->partition_flags() & HA_USE_AUTO_PARTITION &&
      part_info->is_auto_partitioned)
    return false; /* Should not prune auto partitioned table */

  if (!pprune_cond) {
    mark_all_partitions_as_used(part_info);
    return false;
  }

  /* No need to continue pruning if there is no more partitions to prune! */
  if (bitmap_is_clear_all(&part_info->lock_partitions))
    bitmap_clear_all(&part_info->read_partitions);
  if (bitmap_is_clear_all(&part_info->read_partitions)) {
    table->all_partitions_pruned_away = true;
    return false;
  }

  PART_PRUNE_PARAM prune_param;
  MEM_ROOT alloc(key_memory_partitions_prune_exec,
                 thd->variables.range_alloc_block_size);
  RANGE_OPT_PARAM *range_par = &prune_param.range_param;
  my_bitmap_map *old_sets[2];

  prune_param.part_info = part_info;
  alloc.set_max_capacity(thd->variables.range_optimizer_max_mem_size);
  alloc.set_error_for_capacity_exceeded(true);
  thd->push_internal_handler(&range_par->error_handler);
  range_par->return_mem_root =
      &alloc;  // We never use the generated AccessPaths, if any.
  range_par->temp_mem_root = &alloc;

  if (create_partition_index_description(&prune_param)) {
    mark_all_partitions_as_used(part_info);
    thd->pop_internal_handler();
    return false;
  }

  dbug_tmp_use_all_columns(table, old_sets, table->read_set, table->write_set);
  range_par->table = table;
  range_par->query_block = query_block;
  /* range_par->cond doesn't need initialization */
  const table_map prev_tables = INNER_TABLE_BIT;
  const table_map read_tables = INNER_TABLE_BIT;
  const table_map current_table = table->pos_in_table_list->map();

  range_par->keys = 1;  // one index
  range_par->using_real_indexes = false;
  unsigned real_keynr = 0;
  range_par->real_keynr = &real_keynr;

  bitmap_clear_all(&part_info->read_partitions);

  prune_param.key = prune_param.range_param.key_parts;
  SEL_TREE *tree;
  int res;

  tree = get_mm_tree(thd, range_par, prev_tables, read_tables, current_table,
                     /*remove_jump_scans=*/false, pprune_cond);
  if (!tree) goto all_used;

  if (tree->type == SEL_TREE::IMPOSSIBLE) {
    /* Cannot improve the pruning any further. */
    part_info->is_pruning_completed = true;
    goto end;
  }

  if (tree->type != SEL_TREE::KEY) goto all_used;

  if (tree->merges.is_empty()) {
    /* Range analysis has produced a single list of intervals. */
    prune_param.arg_stack_end = prune_param.arg_stack;
    prune_param.cur_part_fields = 0;
    prune_param.cur_subpart_fields = 0;

    prune_param.cur_min_key = prune_param.min_key;
    prune_param.cur_max_key = prune_param.max_key;
    prune_param.cur_min_flag = prune_param.cur_max_flag = 0;

    init_all_partitions_iterator(part_info, &prune_param.part_iter);
    if (!tree->keys[0] ||
        (-1 == (res = find_used_partitions(thd, &prune_param, tree->keys[0]))))
      goto all_used;
  } else {
    if (tree->merges.elements == 1) {
      /*
        Range analysis has produced a "merge" of several intervals lists, a
        SEL_TREE that represents an expression in form
          sel_imerge = (tree1 OR tree2 OR ... OR treeN)
        that cannot be reduced to one tree. This can only happen when
        partitioning index has several keyparts and the condition is OR of
        conditions that refer to different key parts. For example, we'll get
        here for "partitioning_field=const1 OR subpartitioning_field=const2"
      */
      if (-1 == (res = find_used_partitions_imerge(thd, &prune_param,
                                                   tree->merges.head())))
        goto all_used;
    } else {
      /*
        Range analysis has produced a list of several imerges, i.e. a
        structure that represents a condition in form
        imerge_list= (sel_imerge1 AND sel_imerge2 AND ... AND sel_imergeN)
        This is produced for complicated WHERE clauses that range analyzer
        can't really analyze properly.
      */
      if (-1 == (res = find_used_partitions_imerge_list(thd, &prune_param,
                                                        tree->merges)))
        goto all_used;
    }
  }

  /*
    Decide if the current pruning attempt is the final one.

    During the prepare phase, before locking, subqueries and stored programs
    are not evaluated. So we need to run prune_partitions() a second time in
    the optimize phase to prune partitions for reading, when subqueries and
    stored programs may be evaluated.

    The upcoming pruning attempt will be the final one when:
    - condition is constant, or
    - condition may vary for every row (so there is nothing to prune) or
    - evaluation is in execution phase.
  */
  if (pprune_cond->const_item() || !pprune_cond->const_for_execution() ||
      thd->lex->is_query_tables_locked())
    part_info->is_pruning_completed = true;
  goto end;

all_used:
  mark_all_partitions_as_used(prune_param.part_info);
end:
  thd->pop_internal_handler();
  dbug_tmp_restore_column_maps(table->read_set, table->write_set, old_sets);

  /* If an error occurred we can return failure after freeing the memroot. */
  if (thd->is_error()) {
    return true;
  }
  /*
    Must be a subset of the locked partitions.
    lock_partitions contains the partitions marked by explicit partition
    selection (... t PARTITION (pX) ...) and we must only use partitions
    within that set.
  */
  bitmap_intersect(&prune_param.part_info->read_partitions,
                   &prune_param.part_info->lock_partitions);
  /*
    If not yet locked, also prune partitions to lock if not UPDATEing
    partition key fields. This will also prune lock_partitions if we are under
    LOCK TABLES, so prune away calls to start_stmt().
    TODO: enhance this prune locking to also allow pruning of
    'UPDATE t SET part_key = const WHERE cond_is_prunable' so it adds
    a lock for part_key partition.
  */
  if (!thd->lex->is_query_tables_locked() &&
      !partition_key_modified(table, table->write_set)) {
    bitmap_copy(&prune_param.part_info->lock_partitions,
                &prune_param.part_info->read_partitions);
  }
  if (bitmap_is_clear_all(&(prune_param.part_info->read_partitions)))
    table->all_partitions_pruned_away = true;
  return false;
}

/*
  Store field key image to table record

  SYNOPSIS
    store_key_image_to_rec()
      field  Field which key image should be stored
      ptr    Field value in key format
      len    Length of the value, in bytes

  DESCRIPTION
    Copy the field value from its key image to the table record. The source
    is the value in key image format, occupying len bytes in buffer pointed
    by ptr. The destination is table record, in "field value in table record"
    format.
*/

void store_key_image_to_rec(Field *field, uchar *ptr, uint len) {
  /* Do the same as print_key_value() does */
  my_bitmap_map *old_map;

  if (field->is_nullable()) {
    if (*ptr) {
      field->set_null();
      return;
    }
    field->set_notnull();
    ptr++;
  }
  old_map = dbug_tmp_use_all_columns(field->table, field->table->write_set);
  field->set_key_image(ptr, len);
  dbug_tmp_restore_column_map(field->table->write_set, old_map);
}

/*
  For SEL_ARG* array, store sel_arg->min values into table record buffer

  SYNOPSIS
    store_selargs_to_rec()
      ppar   Partition pruning context
      start  Array of SEL_ARG* for which the minimum values should be stored
      num    Number of elements in the array

  DESCRIPTION
    For each SEL_ARG* interval in the specified array, store the left edge
    field value (sel_arg->min, key image format) into the table record.
*/

static void store_selargs_to_rec(PART_PRUNE_PARAM *ppar, SEL_ARG **start,
                                 int num) {
  KEY_PART *parts = ppar->range_param.key_parts;
  for (SEL_ARG **end = start + num; start != end; start++) {
    SEL_ARG *sel_arg = (*start);
    store_key_image_to_rec(sel_arg->field, sel_arg->min_value,
                           parts[sel_arg->part].length);
  }
}

/* Mark a partition as used in the case when there are no subpartitions */
static void mark_full_partition_used_no_parts(partition_info *part_info,
                                              uint32 part_id) {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("Mark partition %u as used", part_id));
  bitmap_set_bit(&part_info->read_partitions, part_id);
}

/* Mark a partition as used in the case when there are subpartitions */
static void mark_full_partition_used_with_parts(partition_info *part_info,
                                                uint32 part_id) {
  uint32 start = part_id * part_info->num_subparts;
  uint32 end = start + part_info->num_subparts;
  DBUG_TRACE;

  for (; start != end; start++) {
    DBUG_PRINT("info", ("1:Mark subpartition %u as used", start));
    bitmap_set_bit(&part_info->read_partitions, start);
  }
}

/*
  Find the set of used partitions for List<SEL_IMERGE>
  SYNOPSIS
    find_used_partitions_imerge_list
      ppar      Partition pruning context.
      key_tree  Intervals tree to perform pruning for.

  DESCRIPTION
    List<SEL_IMERGE> represents "imerge1 AND imerge2 AND ...".
    The set of used partitions is an intersection of used partitions sets
    for imerge_{i}.
    We accumulate this intersection in a separate bitmap.

  RETURN
    See find_used_partitions()
*/

static int find_used_partitions_imerge_list(THD *thd, PART_PRUNE_PARAM *ppar,
                                            List<SEL_IMERGE> &merges) {
  MY_BITMAP all_merges;
  uint bitmap_bytes;
  my_bitmap_map *bitmap_buf;
  uint n_bits = ppar->part_info->read_partitions.n_bits;
  bitmap_bytes = bitmap_buffer_size(n_bits);
  if (!(bitmap_buf = (my_bitmap_map *)ppar->range_param.temp_mem_root->Alloc(
            bitmap_bytes))) {
    /*
      Fallback, process just the first SEL_IMERGE. This can leave us with more
      partitions marked as used then actually needed.
    */
    return find_used_partitions_imerge(thd, ppar, merges.head());
  }
  bitmap_init(&all_merges, bitmap_buf, n_bits);
  bitmap_set_prefix(&all_merges, n_bits);

  List_iterator<SEL_IMERGE> it(merges);
  SEL_IMERGE *imerge;
  while ((imerge = it++)) {
    int res = find_used_partitions_imerge(thd, ppar, imerge);
    if (!res) {
      /* no used partitions on one ANDed imerge => no used partitions at all */
      return 0;
    }

    if (res != -1)
      bitmap_intersect(&all_merges, &ppar->part_info->read_partitions);

    if (bitmap_is_clear_all(&all_merges)) return 0;

    bitmap_clear_all(&ppar->part_info->read_partitions);
  }
  memcpy(ppar->part_info->read_partitions.bitmap, all_merges.bitmap,
         bitmap_bytes);
  return 1;
}

/*
  Find the set of used partitions for SEL_IMERGE structure
  SYNOPSIS
    find_used_partitions_imerge()
      ppar      Partition pruning context.
      key_tree  Intervals tree to perform pruning for.

  DESCRIPTION
    SEL_IMERGE represents "tree1 OR tree2 OR ...". The implementation is
    trivial - just use mark used partitions for each tree and bail out early
    if for some tree_{i} all partitions are used.

  RETURN
    See find_used_partitions().
*/

static int find_used_partitions_imerge(THD *thd, PART_PRUNE_PARAM *ppar,
                                       SEL_IMERGE *imerge) {
  int res = 0;
  for (SEL_TREE *ptree : imerge->trees) {
    ppar->arg_stack_end = ppar->arg_stack;
    ppar->cur_part_fields = 0;
    ppar->cur_subpart_fields = 0;

    ppar->cur_min_key = ppar->min_key;
    ppar->cur_max_key = ppar->max_key;
    ppar->cur_min_flag = ppar->cur_max_flag = 0;

    init_all_partitions_iterator(ppar->part_info, &ppar->part_iter);
    SEL_ROOT *key_tree = ptree->keys[0];
    if (!key_tree || (-1 == (res |= find_used_partitions(thd, ppar, key_tree))))
      return -1;
  }
  return res;
}

/*
  Collect partitioning ranges for the SEL_ARG tree and mark partitions as used

  SYNOPSIS
    find_used_partitions()
      ppar      Partition pruning context.
      key_tree  SEL_ARG range (sub)tree to perform pruning for

  DESCRIPTION
    This function
      * recursively walks the SEL_ARG* tree collecting partitioning "intervals"
      * finds the partitions one needs to use to get rows in these intervals
      * marks these partitions as used.
    The next session describes the process in greater detail.

  IMPLEMENTATION
    TYPES OF RESTRICTIONS THAT WE CAN OBTAIN PARTITIONS FOR
    We can find out which [sub]partitions to use if we obtain restrictions on
    [sub]partitioning fields in the following form:
    1.  "partition_field1=const1 AND ... AND partition_fieldN=constN"
    1.1  Same as (1) but for subpartition fields

    If partitioning supports interval analysis (i.e. partitioning is a
    function of a single table field, and partition_info::
    get_part_iter_for_interval != NULL), then we can also use condition in
    this form:
    2.  "const1 <=? partition_field <=? const2"
    2.1  Same as (2) but for subpartition_field

    INFERRING THE RESTRICTIONS FROM SEL_ARG TREE

    The below is an example of what SEL_ARG tree may represent:

    (start)
     |                           $
     |   Partitioning keyparts   $  subpartitioning keyparts
     |                           $
     |     ...          ...      $
     |      |            |       $
     | +---------+  +---------+  $  +-----------+  +-----------+
     \-| par1=c1 |--| par2=c2 |-----| subpar1=c3|--| subpar2=c5|
       +---------+  +---------+  $  +-----------+  +-----------+
            |                    $        |             |
            |                    $        |        +-----------+
            |                    $        |        | subpar2=c6|
            |                    $        |        +-----------+
            |                    $        |
            |                    $  +-----------+  +-----------+
            |                    $  | subpar1=c4|--| subpar2=c8|
            |                    $  +-----------+  +-----------+
            |                    $
            |                    $
       +---------+               $  +------------+  +------------+
       | par1=c2 |------------------| subpar1=c10|--| subpar2=c12|
       +---------+               $  +------------+  +------------+
            |                    $
           ...                   $

    The up-down connections are connections via SEL_ARG::left and
    SEL_ARG::right. A horizontal connection to the right is the
    SEL_ARG::next_key_part connection.

    find_used_partitions() traverses the entire tree via recursion on
     * SEL_ARG::next_key_part (from left to right on the picture)
     * SEL_ARG::left|right (up/down on the pic). Left-right recursion is
       performed for each depth level.

    Recursion descent on SEL_ARG::next_key_part is used to accumulate (in
    ppar->arg_stack) constraints on partitioning and subpartitioning fields.
    For the example in the above picture, one of stack states is:
      in find_used_partitions(key_tree = "subpar2=c5") (***)
      in find_used_partitions(key_tree = "subpar1=c3")
      in find_used_partitions(key_tree = "par2=c2")   (**)
      in find_used_partitions(key_tree = "par1=c1")
      in prune_partitions(...)
    We apply partitioning limits as soon as possible, e.g. when we reach the
    depth (**), we find which partition(s) correspond to "par1=c1 AND par2=c2",
    and save them in ppar->part_iter.
    When we reach the depth (***), we find which subpartition(s) correspond to
    "subpar1=c3 AND subpar2=c5", and then mark appropriate subpartitions in
    appropriate subpartitions as used.

    It is possible that constraints on some partitioning fields are missing.
    For the above example, consider this stack state:
      in find_used_partitions(key_tree = "subpar2=c12") (***)
      in find_used_partitions(key_tree = "subpar1=c10")
      in find_used_partitions(key_tree = "par1=c2")
      in prune_partitions(...)
    Here we don't have constraints for all partitioning fields. Since we've
    never set the ppar->part_iter to contain used set of partitions, we use
    its default "all partitions" value.  We get  subpartition id for
    "subpar1=c3 AND subpar2=c5", and mark that subpartition as used in every
    partition.

    The inverse is also possible: we may get constraints on partitioning
    fields, but not constraints on subpartitioning fields. In that case,
    calls to find_used_partitions() with depth below (**) will return -1,
    and we will mark entire partition as used.

  TODO
    Replace recursion on SEL_ARG::left and SEL_ARG::right with a loop

  RETURN
    1   OK, one or more [sub]partitions are marked as used.
    0   The passed condition doesn't match any partitions
   -1   Couldn't infer any partition pruning "intervals" from the passed
        SEL_ARG* tree (which means that all partitions should be marked as
        used) Marking partitions as used is the responsibility of the caller.
*/

static int find_used_partitions(THD *thd, PART_PRUNE_PARAM *ppar,
                                SEL_ROOT::Type key_tree_type,
                                SEL_ARG *key_tree) {
  int res, left_res = 0, right_res = 0;
  int key_tree_part = (int)key_tree->part;
  bool set_full_part_if_bad_ret = false;
  bool ignore_part_fields = ppar->ignore_part_fields;
  bool did_set_ignore_part_fields = false;

  if (check_stack_overrun(thd, 3 * STACK_MIN_SIZE, nullptr)) return -1;

  if (key_tree->left != null_element) {
    if (-1 == (left_res = find_used_partitions(thd, ppar, key_tree_type,
                                               key_tree->left)))
      return -1;
  }

  /* Push SEL_ARG's to stack to enable looking backwards as well */
  ppar->cur_part_fields += ppar->is_part_keypart[key_tree_part];
  ppar->cur_subpart_fields += ppar->is_subpart_keypart[key_tree_part];
  *(ppar->arg_stack_end++) = key_tree;

  if (ignore_part_fields) {
    /*
      We come here when a condition on the first partitioning
      fields led to evaluating the partitioning condition
      (due to finding a condition of the type a < const or
      b > const). Thus we must ignore the rest of the
      partitioning fields but we still want to analyse the
      subpartitioning fields.
    */
    if (key_tree->next_key_part)
      res = find_used_partitions(thd, ppar, key_tree->next_key_part);
    else
      res = -1;
    goto pop_and_go_right;
  }

  /*
    TODO: It seems that key_tree_type is _always_ KEY_RANGE in practice,
    so maybe this if is redundant and should be replaced with an assert?
  */
  if (key_tree_type == SEL_ROOT::Type::KEY_RANGE) {
    if (ppar->part_info->get_part_iter_for_interval &&
        key_tree->part <= ppar->last_part_partno) {
      /* Collect left and right bound, their lengths and flags */
      uchar *min_key = ppar->cur_min_key;
      uchar *max_key = ppar->cur_max_key;
      uchar *tmp_min_key = min_key;
      uchar *tmp_max_key = max_key;
      key_tree->store_min_value(ppar->key[key_tree->part].store_length,
                                &tmp_min_key, ppar->cur_min_flag);
      key_tree->store_max_value(ppar->key[key_tree->part].store_length,
                                &tmp_max_key, ppar->cur_max_flag);
      uint flag;
      if (key_tree->next_key_part &&
          key_tree->next_key_part->root->part == key_tree->part + 1 &&
          key_tree->next_key_part->root->part <= ppar->last_part_partno &&
          key_tree->next_key_part->type == SEL_ROOT::Type::KEY_RANGE) {
        /*
          There are more key parts for partition pruning to handle
          This mainly happens when the condition is an equality
          condition.
        */
        if ((tmp_min_key - min_key) == (tmp_max_key - max_key) &&
            (memcmp(min_key, max_key, (uint)(tmp_max_key - max_key)) == 0) &&
            !key_tree->min_flag && !key_tree->max_flag) {
          /* Set 'parameters' */
          ppar->cur_min_key = tmp_min_key;
          ppar->cur_max_key = tmp_max_key;
          uint save_min_flag = ppar->cur_min_flag;
          uint save_max_flag = ppar->cur_max_flag;

          ppar->cur_min_flag |= key_tree->min_flag;
          ppar->cur_max_flag |= key_tree->max_flag;

          res = find_used_partitions(thd, ppar, key_tree->next_key_part);

          /* Restore 'parameters' back */
          ppar->cur_min_key = min_key;
          ppar->cur_max_key = max_key;

          ppar->cur_min_flag = save_min_flag;
          ppar->cur_max_flag = save_max_flag;
          goto pop_and_go_right;
        }
        /* We have arrived at the last field in the partition pruning */
        uint tmp_min_flag = key_tree->min_flag,
             tmp_max_flag = key_tree->max_flag;
        if (!tmp_min_flag)
          key_tree->next_key_part->store_min_key(ppar->key, &tmp_min_key,
                                                 &tmp_min_flag,
                                                 ppar->last_part_partno, true);
        if (!tmp_max_flag)
          key_tree->next_key_part->store_max_key(ppar->key, &tmp_max_key,
                                                 &tmp_max_flag,
                                                 ppar->last_part_partno, false);
        flag = tmp_min_flag | tmp_max_flag;
      } else
        flag = key_tree->min_flag | key_tree->max_flag;

      if (tmp_min_key != ppar->min_key)
        flag &= ~NO_MIN_RANGE;
      else
        flag |= NO_MIN_RANGE;
      if (tmp_max_key != ppar->max_key)
        flag &= ~NO_MAX_RANGE;
      else
        flag |= NO_MAX_RANGE;

      /*
        We need to call the interval mapper if we have a condition which
        makes sense to prune on. In the example of COLUMNS on a and
        b it makes sense if we have a condition on a, or conditions on
        both a and b. If we only have conditions on b it might make sense
        but this is a harder case we will solve later. For the harder case
        this clause then turns into use of all partitions and thus we
        simply set res= -1 as if the mapper had returned that.
        TODO: What to do here is defined in WL#4065.
      */
      if (ppar->arg_stack[0]->part == 0) {
        uint32 i;
        uint32 store_length_array[MAX_KEY];
        uint32 num_keys = ppar->part_fields;

        for (i = 0; i < num_keys; i++)
          store_length_array[i] = ppar->key[i].store_length;
        res = ppar->part_info->get_part_iter_for_interval(
            ppar->part_info, false, store_length_array, ppar->min_key,
            ppar->max_key, tmp_min_key - ppar->min_key,
            tmp_max_key - ppar->max_key, flag, &ppar->part_iter);
        if (!res)
          goto pop_and_go_right; /* res==0 --> no satisfying partitions */
      } else
        res = -1;

      if (res == -1) {
        /* get a full range iterator */
        init_all_partitions_iterator(ppar->part_info, &ppar->part_iter);
      }
      /*
        Save our intent to mark full partition as used if we will not be able
        to obtain further limits on subpartitions
      */
      if (key_tree_part < ppar->last_part_partno) {
        /*
          We need to ignore the rest of the partitioning fields in all
          evaluations after this
        */
        did_set_ignore_part_fields = true;
        ppar->ignore_part_fields = true;
      }
      set_full_part_if_bad_ret = true;
      goto process_next_key_part;
    }

    if (key_tree_part == ppar->last_subpart_partno &&
        (nullptr != ppar->part_info->get_subpart_iter_for_interval)) {
      PARTITION_ITERATOR subpart_iter;
      DBUG_EXECUTE("info", dbug_print_segment_range(
                               key_tree, ppar->range_param.key_parts););
      res = ppar->part_info->get_subpart_iter_for_interval(
          ppar->part_info, true, nullptr, /* Currently not used here */
          key_tree->min_value, key_tree->max_value, 0,
          0, /* Those are ignored here */
          key_tree->min_flag | key_tree->max_flag, &subpart_iter);
      if (res == 0) {
        /*
           The only case where we can get "no satisfying subpartitions"
           returned from the above call is when an error has occurred.
        */
        assert(thd->is_error());
        return 0;
      }

      if (res == -1) goto pop_and_go_right; /* all subpartitions satisfy */

      uint32 subpart_id;
      bitmap_clear_all(&ppar->subparts_bitmap);
      while ((subpart_id = subpart_iter.get_next(&subpart_iter)) !=
             NOT_A_PARTITION_ID)
        bitmap_set_bit(&ppar->subparts_bitmap, subpart_id);

      /* Mark each partition as used in each subpartition.  */
      uint32 part_id;
      while ((part_id = ppar->part_iter.get_next(&ppar->part_iter)) !=
             NOT_A_PARTITION_ID) {
        for (uint i = 0; i < ppar->part_info->num_subparts; i++)
          if (bitmap_is_set(&ppar->subparts_bitmap, i))
            bitmap_set_bit(&ppar->part_info->read_partitions,
                           part_id * ppar->part_info->num_subparts + i);
      }
      goto pop_and_go_right;
    }

    if (key_tree->is_singlepoint()) {
      if (key_tree_part == ppar->last_part_partno &&
          ppar->cur_part_fields == ppar->part_fields &&
          ppar->part_info->get_part_iter_for_interval == nullptr) {
        /*
          Ok, we've got "fieldN<=>constN"-type SEL_ARGs for all partitioning
          fields. Save all constN constants into table record buffer.
        */
        store_selargs_to_rec(ppar, ppar->arg_stack, ppar->part_fields);
        DBUG_EXECUTE("info", dbug_print_singlepoint_range(ppar->arg_stack,
                                                          ppar->part_fields););
        uint32 part_id;
        longlong func_value;
        /* Find in which partition the {const1, ...,constN} tuple goes */
        if (ppar->get_top_partition_id_func(ppar->part_info, &part_id,
                                            &func_value)) {
          res = 0; /* No satisfying partitions */
          goto pop_and_go_right;
        }
        /* Remember the limit we got - single partition #part_id */
        init_single_partition_iterator(part_id, &ppar->part_iter);

        /*
          If there are no subpartitions/we fail to get any limit for them,
          then we'll mark full partition as used.
        */
        set_full_part_if_bad_ret = true;
        goto process_next_key_part;
      }

      if (key_tree_part == ppar->last_subpart_partno &&
          ppar->cur_subpart_fields == ppar->subpart_fields) {
        /*
          Ok, we've got "fieldN<=>constN"-type SEL_ARGs for all subpartitioning
          fields. Save all constN constants into table record buffer.
        */
        store_selargs_to_rec(ppar, ppar->arg_stack_end - ppar->subpart_fields,
                             ppar->subpart_fields);
        DBUG_EXECUTE("info", dbug_print_singlepoint_range(
                                 ppar->arg_stack_end - ppar->subpart_fields,
                                 ppar->subpart_fields););
        /* Find the subpartition (it's HASH/KEY so we always have one) */
        partition_info *part_info = ppar->part_info;
        uint32 part_id, subpart_id;

        if (part_info->get_subpartition_id(part_info, &subpart_id)) return 0;

        /* Mark this partition as used in each subpartition. */
        while ((part_id = ppar->part_iter.get_next(&ppar->part_iter)) !=
               NOT_A_PARTITION_ID) {
          bitmap_set_bit(&part_info->read_partitions,
                         part_id * part_info->num_subparts + subpart_id);
        }
        res = 1; /* Some partitions were marked as used */
        goto pop_and_go_right;
      }
    } else {
      /*
        Can't handle condition on current key part. If we're that deep that
        we're processing subpartititoning's key parts, this means we'll not be
        able to infer any suitable condition, so bail out.
      */
      if (key_tree_part >= ppar->last_part_partno) {
        res = -1;
        goto pop_and_go_right;
      }
      /*
        No meaning in continuing with rest of partitioning key parts.
        Will try to continue with subpartitioning key parts.
      */
      ppar->ignore_part_fields = true;
      did_set_ignore_part_fields = true;
      goto process_next_key_part;
    }
  }

process_next_key_part:
  if (key_tree->next_key_part)
    res = find_used_partitions(thd, ppar, key_tree->next_key_part);
  else
    res = -1;

  if (did_set_ignore_part_fields) {
    /*
      We have returned from processing all key trees linked to our next
      key part. We are ready to be moving down (using right pointers) and
      this tree is a new evaluation requiring its own decision on whether
      to ignore partitioning fields.
    */
    ppar->ignore_part_fields = false;
  }
  if (set_full_part_if_bad_ret) {
    if (res == -1) {
      /* Got "full range" for subpartitioning fields */
      uint32 part_id;
      bool found = false;
      while ((part_id = ppar->part_iter.get_next(&ppar->part_iter)) !=
             NOT_A_PARTITION_ID) {
        ppar->mark_full_partition_used(ppar->part_info, part_id);
        found = true;
      }
      res = found;
    }
    /*
      Restore the "used partitions iterator" to the default setting that
      specifies iteration over all partitions.
    */
    init_all_partitions_iterator(ppar->part_info, &ppar->part_iter);
  }

pop_and_go_right:
  /* Pop this key part info off the "stack" */
  ppar->arg_stack_end--;
  ppar->cur_part_fields -= ppar->is_part_keypart[key_tree_part];
  ppar->cur_subpart_fields -= ppar->is_subpart_keypart[key_tree_part];

  if (res == -1) return -1;
  if (key_tree->right != null_element) {
    if (-1 == (right_res = find_used_partitions(thd, ppar, key_tree_type,
                                                key_tree->right)))
      return -1;
  }
  return (left_res || right_res || res);
}

static int find_used_partitions(THD *thd, PART_PRUNE_PARAM *ppar,
                                SEL_ROOT *key_tree) {
  return find_used_partitions(thd, ppar, key_tree->type, key_tree->root);
}

static void mark_all_partitions_as_used(partition_info *part_info) {
  bitmap_copy(&(part_info->read_partitions), &(part_info->lock_partitions));
}

/*
  Check if field types allow to construct partitioning index description

  SYNOPSIS
    fields_ok_for_partition_index()
      pfield  NULL-terminated array of pointers to fields.

  DESCRIPTION
    For an array of fields, check if we can use all of the fields to create
    partitioning index description.

    We can't process GEOMETRY fields - for these fields singlepoint intervals
    can't be generated, and non-singlepoint are "special" kinds of intervals
    to which our processing logic can't be applied.

    It is not known if we could process ENUM fields, so they are disabled to be
    on the safe side.

  RETURN
    true   Yes, fields can be used in partitioning index
    false  Otherwise
*/

static bool fields_ok_for_partition_index(Field **pfield) {
  if (!pfield) return false;
  for (; (*pfield); pfield++) {
    enum_field_types ftype = (*pfield)->real_type();
    if (ftype == MYSQL_TYPE_ENUM || ftype == MYSQL_TYPE_GEOMETRY) return false;
  }
  return true;
}

/*
  Create partition index description and fill related info in the context
  struct

  SYNOPSIS
    create_partition_index_description()
      prune_par  INOUT Partition pruning context

  DESCRIPTION
    Create partition index description. Partition index description is:

      part_index(used_fields_list(part_expr), used_fields_list(subpart_expr))

    If partitioning/sub-partitioning uses BLOB or Geometry fields, then
    corresponding fields_list(...) is not included into index description
    and we don't perform partition pruning for partitions/subpartitions.

  RETURN
    true   Out of memory or can't do partition pruning at all
    false  OK
*/

static bool create_partition_index_description(PART_PRUNE_PARAM *ppar) {
  RANGE_OPT_PARAM *range_par = &(ppar->range_param);
  partition_info *part_info = ppar->part_info;
  uint used_part_fields, used_subpart_fields;

  used_part_fields = fields_ok_for_partition_index(part_info->part_field_array)
                         ? part_info->num_part_fields
                         : 0;
  used_subpart_fields =
      fields_ok_for_partition_index(part_info->subpart_field_array)
          ? part_info->num_subpart_fields
          : 0;

  uint total_parts = used_part_fields + used_subpart_fields;

  ppar->ignore_part_fields = false;
  ppar->part_fields = used_part_fields;
  ppar->last_part_partno = (int)used_part_fields - 1;

  ppar->subpart_fields = used_subpart_fields;
  ppar->last_subpart_partno =
      used_subpart_fields ? (int)(used_part_fields + used_subpart_fields - 1)
                          : -1;

  if (part_info->is_sub_partitioned()) {
    ppar->mark_full_partition_used = mark_full_partition_used_with_parts;
    ppar->get_top_partition_id_func = part_info->get_part_partition_id;
  } else {
    ppar->mark_full_partition_used = mark_full_partition_used_no_parts;
    ppar->get_top_partition_id_func = part_info->get_partition_id;
  }

  KEY_PART *key_part;
  MEM_ROOT *alloc = range_par->temp_mem_root;
  if (!total_parts ||
      !(key_part = (KEY_PART *)alloc->Alloc(sizeof(KEY_PART) * total_parts)) ||
      !(ppar->arg_stack =
            (SEL_ARG **)alloc->Alloc(sizeof(SEL_ARG *) * total_parts)) ||
      !(ppar->is_part_keypart =
            (bool *)alloc->Alloc(sizeof(bool) * total_parts)) ||
      !(ppar->is_subpart_keypart =
            (bool *)alloc->Alloc(sizeof(bool) * total_parts)))
    return true;

  if (ppar->subpart_fields) {
    my_bitmap_map *buf;
    uint32 bufsize = bitmap_buffer_size(ppar->part_info->num_subparts);
    if (!(buf = (my_bitmap_map *)alloc->Alloc(bufsize))) return true;
    bitmap_init(&ppar->subparts_bitmap, buf, ppar->part_info->num_subparts);
  }
  range_par->key_parts = key_part;
  Field **field = (ppar->part_fields) ? part_info->part_field_array
                                      : part_info->subpart_field_array;
  bool in_subpart_fields = false;
  for (uint part = 0; part < total_parts; part++, key_part++) {
    key_part->key = 0;
    key_part->part = part;
    key_part->length = (uint16)(*field)->key_length();
    key_part->store_length = (uint16)get_partition_field_store_length(*field);

    DBUG_PRINT("info", ("part %u length %u store_length %u", part,
                        key_part->length, key_part->store_length));

    key_part->field = (*field);
    key_part->image_type = Field::itRAW;
    /*
      We set keypart flag to 0 here as the only HA_PART_KEY_SEG is checked
      in the RangeAnalysisModule.
    */
    key_part->flag = 0;
    /* We don't set key_parts->null_bit as it will not be used */

    ppar->is_part_keypart[part] = !in_subpart_fields;
    ppar->is_subpart_keypart[part] = in_subpart_fields;

    /*
      Check if this was last field in this array, in this case we
      switch to subpartitioning fields. (This will only happens if
      there are subpartitioning fields to cater for).
    */
    if (!*(++field)) {
      field = part_info->subpart_field_array;
      in_subpart_fields = true;
    }
  }
  range_par->key_parts_end = key_part;

  DBUG_EXECUTE("info", print_partitioning_index(range_par->key_parts,
                                                range_par->key_parts_end););
  return false;
}

#ifndef NDEBUG

static void print_partitioning_index(KEY_PART *parts, KEY_PART *parts_end) {
  DBUG_TRACE;
  DBUG_LOCK_FILE;
  fprintf(DBUG_FILE, "partitioning INDEX(");
  for (KEY_PART *p = parts; p != parts_end; p++) {
    fprintf(DBUG_FILE, "%s%s", p == parts ? "" : " ,", p->field->field_name);
  }
  fputs(");\n", DBUG_FILE);
  DBUG_UNLOCK_FILE;
}

/* Print a "c1 < keypartX < c2" - type interval into debug trace. */
static void dbug_print_segment_range(SEL_ARG *arg, KEY_PART *part) {
  DBUG_TRACE;
  DBUG_LOCK_FILE;
  if (!(arg->min_flag & NO_MIN_RANGE)) {
    store_key_image_to_rec(part->field, arg->min_value, part->length);
    part->field->dbug_print();
    if (arg->min_flag & NEAR_MIN)
      fputs(" < ", DBUG_FILE);
    else
      fputs(" <= ", DBUG_FILE);
  }

  fprintf(DBUG_FILE, "%s", part->field->field_name);

  if (!(arg->max_flag & NO_MAX_RANGE)) {
    if (arg->max_flag & NEAR_MAX)
      fputs(" < ", DBUG_FILE);
    else
      fputs(" <= ", DBUG_FILE);
    store_key_image_to_rec(part->field, arg->max_value, part->length);
    part->field->dbug_print();
  }
  fputs("\n", DBUG_FILE);
  DBUG_UNLOCK_FILE;
}

/*
  Print a singlepoint multi-keypart range interval to debug trace

  SYNOPSIS
    dbug_print_singlepoint_range()
      start  Array of SEL_ARG* ptrs representing conditions on key parts
      num    Number of elements in the array.

  DESCRIPTION
    This function prints a "keypartN=constN AND ... AND keypartK=constK"-type
    interval to debug trace.
*/

static void dbug_print_singlepoint_range(SEL_ARG **start, uint num) {
  DBUG_TRACE;
  DBUG_LOCK_FILE;
  SEL_ARG **end = start + num;

  for (SEL_ARG **arg = start; arg != end; arg++) {
    Field *field = (*arg)->field;
    fprintf(DBUG_FILE, "%s%s=", (arg == start) ? "" : ", ", field->field_name);
    field->dbug_print();
  }
  fputs("\n", DBUG_FILE);
  DBUG_UNLOCK_FILE;
}
#endif

/****************************************************************************
 * Partition pruning code ends
 ****************************************************************************/
