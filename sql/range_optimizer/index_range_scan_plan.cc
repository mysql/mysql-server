/* Copyright (c) 2000, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql/range_optimizer/index_range_scan_plan.h"

#include <assert.h>
#include <string.h>
#include <algorithm>
#include <memory>

#include "my_alloc.h"
#include "my_base.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "mysql/strings/m_ctype.h"
#include "sql/join_optimizer/access_path.h"
#include "sql/key.h"
#include "sql/mem_root_array.h"
#include "sql/opt_hints.h"
#include "sql/opt_trace.h"
#include "sql/opt_trace_context.h"
#include "sql/range_optimizer/geometry_index_range_scan.h"
#include "sql/range_optimizer/index_range_scan.h"
#include "sql/range_optimizer/internal.h"
#include "sql/range_optimizer/range_opt_param.h"
#include "sql/range_optimizer/range_optimizer.h"
#include "sql/range_optimizer/reverse_index_range_scan.h"
#include "sql/range_optimizer/tree.h"
#include "sql/sql_bitmap.h"
#include "sql/sql_class.h"
#include "sql/sql_lex.h"
#include "sql/sql_select.h"
#include "sql/table.h"
#include "sql/thr_malloc.h"
#include "sql_string.h"

using opt_range::null_element;
using std::max;
using std::min;

static bool is_key_scan_ror(RANGE_OPT_PARAM *param, uint keynr, uint nparts);
static bool eq_ranges_exceeds_limit(const SEL_ROOT *keypart, uint *count,
                                    uint limit);
static bool get_ranges_from_tree_given_base(
    THD *thd, MEM_ROOT *return_mem_root, const KEY *table_key, KEY_PART *key,
    SEL_ROOT *key_tree, uchar *const base_min_key, uchar *min_key,
    uint min_key_flag, uchar *const base_max_key, uchar *max_key,
    uint max_key_flag, bool first_keypart_is_asc, uint num_key_parts,
    uint *used_key_parts, uint *num_exact_key_parts, Quick_ranges *ranges);

/* MRR range sequence, SEL_ARG* implementation: stack entry */
struct RANGE_SEQ_ENTRY {
  /*
    Pointers in min and max keys. They point to right-after-end of key
    images. The 0-th entry has these pointing to key tuple start.
  */
  uchar *min_key, *max_key;

  /*
    Flags, for {keypart0, keypart1, ... this_keypart} subtuple.
    min_key_flag may have NULL_RANGE set.
  */
  uint min_key_flag, max_key_flag;
  enum ha_rkey_function rkey_func_flag;
  /* Number of key parts */
  uint min_key_parts, max_key_parts;
  /**
    Pointer into the R-B tree for this keypart. It points to the
    currently active range for the keypart, so calling next on it will
    get to the next range. sel_arg_range_seq_next() uses this to avoid
    reparsing the R-B range trees each time a new range is fetched.
  */
  SEL_ARG *key_tree;
};

/*
  MRR range sequence, SEL_ARG* implementation: SEL_ARG graph traversal context
*/
class Sel_arg_range_sequence {
 private:
  /**
    Stack of ranges for the curr_kp first keyparts. Used by
    sel_arg_range_seq_next() so that if the next range is equal to the
    previous one for the first x keyparts, stack[x-1] can be
    accumulated with the new range in keyparts > x to quickly form
    the next range to return.

    Notation used below: "x:y" means a range where
    "column_in_keypart_0=x" and "column_in_keypart_1=y". For
    simplicity, only equality (no BETWEEN, < etc) is considered in the
    example but the same principle applies to other range predicate
    operators too.

    Consider a query with these range predicates:
      (kp0=1 and kp1=2 and kp2=3) or
      (kp0=1 and kp1=2 and kp2=4) or
      (kp0=1 and kp1=3 and kp2=5) or
      (kp0=1 and kp1=3 and kp2=6)

    1) sel_arg_range_seq_next() is called the first time
       - traverse the R-B tree (see SEL_ARG) to find the first range
       - returns range "1:2:3"
       - values in stack after this: stack[1, 1:2, 1:2:3]
    2) sel_arg_range_seq_next() is called second time
       - keypart 2 has another range, so the next range in
         keypart 2 is appended to stack[1] and saved
         in stack[2]
       - returns range "1:2:4"
       - values in stack after this: stack[1, 1:2, 1:2:4]
    3) sel_arg_range_seq_next() is called the third time
       - no more ranges in keypart 2, but keypart 1 has
         another range, so the next range in keypart 1 is
         appended to stack[0] and saved in stack[1]. The first
         range in keypart 2 is then appended to stack[1] and
         saved in stack[2]
       - returns range "1:3:5"
       - values in stack after this: stack[1, 1:3, 1:3:5]
    4) sel_arg_range_seq_next() is called the fourth time
       - keypart 2 has another range, see 2)
       - returns range "1:3:6"
       - values in stack after this: stack[1, 1:3, 1:3:6]
  */
  RANGE_SEQ_ENTRY stack[MAX_REF_PARTS];
  /*
    Index of last used element in the above array. A value of -1 means
    that the stack is empty.
  */
  int curr_kp;

 public:
  uint keyno;      /* index of used tree in SEL_TREE structure */
  uint real_keyno; /* Number of the index in tables */

  RANGE_OPT_PARAM *const param;
  bool *is_ror_scan;
  uchar *min_key, *max_key;
  const bool skip_records_in_range;
  SEL_ARG *start; /* Root node of the traversed SEL_ARG* graph */

  /* Number of ranges in the last checked tree->key */
  uint range_count = 0;
  uint max_key_part;

  Sel_arg_range_sequence(RANGE_OPT_PARAM *param_arg, bool *is_ror_scan_arg,
                         uchar *min_key_arg, uchar *max_key_arg,
                         bool skip_records_in_range_arg)
      : param(param_arg),
        is_ror_scan(is_ror_scan_arg),
        min_key(min_key_arg),
        max_key(max_key_arg),
        skip_records_in_range(skip_records_in_range_arg) {
    reset();
  }

  void reset() {
    stack[0].key_tree = nullptr;
    stack[0].min_key = min_key;
    stack[0].min_key_flag = 0;
    stack[0].min_key_parts = 0;
    stack[0].rkey_func_flag = HA_READ_INVALID;

    stack[0].max_key = max_key;
    stack[0].max_key_flag = 0;
    stack[0].max_key_parts = 0;
    curr_kp = -1;
  }

  bool stack_empty() const { return (curr_kp == -1); }

  void stack_push_range(SEL_ARG *key_tree);

  void stack_pop_range() {
    assert(!stack_empty());
    if (curr_kp == 0)
      reset();
    else
      curr_kp--;
  }

  int stack_size() const { return curr_kp + 1; }

  RANGE_SEQ_ENTRY *stack_top() {
    return stack_empty() ? nullptr : &stack[curr_kp];
  }
};

/*
  Range sequence interface, SEL_ARG* implementation: Initialize the traversal

  SYNOPSIS
    init()
      init_params  SEL_ARG tree traversal context

  RETURN
    Value of init_param
*/

static range_seq_t sel_arg_range_seq_init(void *init_param, uint, uint) {
  Sel_arg_range_sequence *seq =
      static_cast<Sel_arg_range_sequence *>(init_param);
  seq->reset();
  return init_param;
}

void Sel_arg_range_sequence::stack_push_range(SEL_ARG *key_tree) {
  assert((uint)curr_kp + 1 < MAX_REF_PARTS);

  RANGE_SEQ_ENTRY *push_position = &stack[curr_kp + 1];
  RANGE_SEQ_ENTRY *last_added_kp = stack_top();
  if (stack_empty()) {
    /*
       If we get here this is either
         a) the first time a range sequence is constructed for this
            range access method (in which case stack[0] has not been
            modified since the constructor was called), or
         b) there are multiple ranges for the first keypart in the
            condition (and we have called stack_pop_range() to empty
            the stack).
       In both cases, reset() has been called and all fields in
       push_position have been reset. All we need to do is to copy the
       min/max key flags from the predicate we're about to add to
       stack[0].
    */
    push_position->min_key_flag = key_tree->get_min_flag();
    push_position->max_key_flag = key_tree->get_max_flag();
    push_position->rkey_func_flag = key_tree->rkey_func_flag;
  } else {
    push_position->min_key = last_added_kp->min_key;
    push_position->max_key = last_added_kp->max_key;
    push_position->min_key_parts = last_added_kp->min_key_parts;
    push_position->max_key_parts = last_added_kp->max_key_parts;
    push_position->min_key_flag =
        last_added_kp->min_key_flag | key_tree->get_min_flag();
    push_position->max_key_flag =
        last_added_kp->max_key_flag | key_tree->get_max_flag();
    push_position->rkey_func_flag = key_tree->rkey_func_flag;
  }

  push_position->key_tree = key_tree;
  /* psergey-merge-done:
  key_tree->store(arg->param->key[arg->keyno][key_tree->part].store_length,
                  &cur->min_key, prev->min_key_flag,
                  &cur->max_key, prev->max_key_flag);
  */
  key_tree->store_min_max_values(
      param->key[keyno][key_tree->part].store_length, &push_position->min_key,
      (last_added_kp ? last_added_kp->min_key_flag : 0),
      &push_position->max_key,
      (last_added_kp ? last_added_kp->max_key_flag : 0),
      (int *)&push_position->min_key_parts,
      (int *)&push_position->max_key_parts);
  if (key_tree->is_null_interval()) push_position->min_key_flag |= NULL_RANGE;
  curr_kp++;
}

/*
  Range sequence interface, SEL_ARG* implementation: get the next interval
  in the R-B tree

  SYNOPSIS
    sel_arg_range_seq_next()
      rseq        Value returned from sel_arg_range_seq_init
      range  OUT  Store information about the range here

  DESCRIPTION
    This is "get_next" function for Range sequence interface implementation
    for SEL_ARG* tree.

  IMPLEMENTATION
    The traversal also updates those param members:
      - range_count
      - max_key_part
    as well as:
      - is_ror_scan

  RETURN
    0  Ok
    1  No more ranges in the sequence

  NOTE: append_range_all_keyparts(), which is used to e.g. print
  ranges to Optimizer Trace in a human readable format, mimics the
  behavior of this function.
*/

// psergey-merge-todo: support check_quick_keys:max_keypart
static uint sel_arg_range_seq_next(range_seq_t rseq, KEY_MULTI_RANGE *range) {
  SEL_ARG *key_tree;
  Sel_arg_range_sequence *seq = static_cast<Sel_arg_range_sequence *>(rseq);

  if (seq->stack_empty()) {
    /*
      This is the first time sel_arg_range_seq_next is called.
      seq->start points to the root of the R-B tree for the first
      keypart
    */
    key_tree = seq->start;

    /*
      Move to the first range for the first keypart. Save this range
      in seq->stack[0] and carry on to ranges in the next keypart if
      any
    */
    key_tree = key_tree->first();
    seq->stack_push_range(key_tree);
  } else {
    /*
      This is not the first time sel_arg_range_seq_next is called, so
      seq->stack is populated with the range the last call to this
      function found. seq->stack[current_keypart].key_tree points to a
      leaf in the R-B tree of the last keypart that was part of the
      former range. This is the starting point for finding the next
      range. @see Sel_arg_range_sequence::stack
    */
    // See if there are more ranges in this or any of the previous keyparts
    while (true) {
      key_tree = seq->stack_top()->key_tree;
      seq->stack_pop_range();
      if (key_tree->next) {
        /* This keypart has more ranges */
        assert(key_tree->next != null_element);
        key_tree = key_tree->next;

        /*
          save the next range for this keypart and carry on to ranges in
          the next keypart if any
        */
        seq->stack_push_range(key_tree);
        *seq->is_ror_scan = false;
        break;
      }

      if (seq->stack_empty()) {
        // There are no more ranges for the first keypart: we're done
        return 1;
      }
      /*
         There are no more ranges for the current keypart. Step back
         to the previous keypart and see if there are more ranges
         there.
      */
    }
  }

  assert(!seq->stack_empty());

  /*
    Add range info for the next keypart if
      1) there is a range predicate for a later keypart
      2) the range predicate is for the next keypart in the index: a
         range predicate on keypartX+1 can only be used if there is a
         range predicate on keypartX.
      3) the range predicate on the next keypart is usable
  */
  while (key_tree->next_key_part &&                                    // 1)
         key_tree->next_key_part->root != null_element &&              // 1)
         key_tree->next_key_part->root->part == key_tree->part + 1 &&  // 2)
         key_tree->next_key_part->type == SEL_ROOT::Type::KEY_RANGE)   // 3)
  {
    {
      DBUG_PRINT("info", ("while(): key_tree->part %d", key_tree->part));
      RANGE_SEQ_ENTRY *cur = seq->stack_top();
      const size_t min_key_total_length = cur->min_key - seq->min_key;
      const size_t max_key_total_length = cur->max_key - seq->max_key;

      /*
        Check if more ranges can be added. This is the case if all
        predicates for keyparts handled so far are equality
        predicates. If either of the following apply, there are
        non-equality predicates in stack[]:

        1) min_key_total_length != max_key_total_length (because
           equality ranges are stored as "min_key = max_key = <value>")
        2) memcmp(<min_key_values>,<max_key_values>) != 0 (same argument as 1)
        3) A min or max flag has been set: Because flags denote ranges
           ('<', '<=' etc), any value but 0 indicates a non-equality
           predicate.
      */

      uchar *min_key_start;
      uchar *max_key_start;
      size_t cur_key_length;

      if (seq->stack_size() == 1) {
        min_key_start = seq->min_key;
        max_key_start = seq->max_key;
        cur_key_length = min_key_total_length;
      } else {
        const RANGE_SEQ_ENTRY prev = cur[-1];
        min_key_start = prev.min_key;
        max_key_start = prev.max_key;
        cur_key_length = cur->min_key - prev.min_key;
      }

      if ((min_key_total_length != max_key_total_length) ||          // 1)
          (memcmp(min_key_start, max_key_start, cur_key_length)) ||  // 2)
          (key_tree->min_flag || key_tree->max_flag))                // 3)
      {
        DBUG_PRINT("info", ("while(): inside if()"));
        /*
          The range predicate up to and including the one in key_tree
          is usable by range access but does not allow subranges made
          up from predicates in later keyparts. This may e.g. be
          because the predicate operator is "<". Since there are range
          predicates on more keyparts, we use those to more closely
          specify the start and stop locations for the range. Example:

                "SELECT * FROM t1 WHERE a >= 2 AND b >= 3":

                t1 content:
                -----------
                1 1
                2 1     <- 1)
                2 2
                2 3     <- 2)
                2 4
                3 1
                3 2
                3 3

          The predicate cannot be translated into something like
             "(a=2 and b>=3) or (a=3 and b>=3) or ..."
          I.e., it cannot be divided into subranges, but by storing
          min/max key below we can at least start the scan from 2)
          instead of 1)

        */
        *seq->is_ror_scan = false;
        key_tree->store_next_min_max_keys(
            seq->param->key[seq->keyno], &cur->min_key, &cur->min_key_flag,
            &cur->max_key, &cur->max_key_flag, (int *)&cur->min_key_parts,
            (int *)&cur->max_key_parts);
        break;
      }
    }

    /*
      There are usable range predicates for the next keypart and the
      range predicate for the current keypart allows us to make use of
      them. Move to the first range predicate for the next keypart.
      Push this range predicate to seq->stack and move on to the next
      keypart (if any). @see Sel_arg_range_sequence::stack
    */
    key_tree = key_tree->next_key_part->root->first();
    seq->stack_push_range(key_tree);
  }

  assert(!seq->stack_empty() && (seq->stack_top() != nullptr));

  // We now have a full range predicate in seq->stack_top()
  RANGE_SEQ_ENTRY *cur = seq->stack_top();
  RANGE_OPT_PARAM *param = seq->param;
  size_t min_key_length = cur->min_key - seq->min_key;

  if (cur->min_key_flag & GEOM_FLAG) {
    range->range_flag = cur->min_key_flag;

    /* Here minimum contains also function code bits, and maximum is +inf */
    range->start_key.key = seq->min_key;
    range->start_key.length = min_key_length;
    range->start_key.keypart_map = make_prev_keypart_map(cur->min_key_parts);
    range->start_key.flag = cur->rkey_func_flag;
    /*
      Spatial operators are only allowed on spatial indexes, and no
      spatial index can at the moment return rows in ROWID order
    */
    assert(!*seq->is_ror_scan);
  } else {
    const KEY *cur_key_info = &param->table->key_info[seq->real_keyno];
    range->range_flag = cur->min_key_flag | cur->max_key_flag;

    range->start_key.key = seq->min_key;
    range->start_key.length = cur->min_key - seq->min_key;
    range->start_key.keypart_map = make_prev_keypart_map(cur->min_key_parts);
    range->start_key.flag =
        (cur->min_key_flag & NEAR_MIN ? HA_READ_AFTER_KEY : HA_READ_KEY_EXACT);

    range->end_key.key = seq->max_key;
    range->end_key.length = cur->max_key - seq->max_key;
    range->end_key.keypart_map = make_prev_keypart_map(cur->max_key_parts);
    range->end_key.flag =
        (cur->max_key_flag & NEAR_MAX ? HA_READ_BEFORE_KEY : HA_READ_AFTER_KEY);
    /*
      This is an equality range (keypart_0=X and ... and keypart_n=Z) if
        1) There are no flags indicating open range (e.g.,
           "keypart_x > y") or GIS.
        2) The lower bound and the upper bound of the range has the
           same value (min_key == max_key).
    */
    const uint is_open_range =
        (NO_MIN_RANGE | NO_MAX_RANGE | NEAR_MIN | NEAR_MAX | GEOM_FLAG);
    const bool is_eq_range_pred =
        !(cur->min_key_flag & is_open_range) &&              // 1)
        !(cur->max_key_flag & is_open_range) &&              // 1)
        range->start_key.length == range->end_key.length &&  // 2)
        !memcmp(seq->min_key, seq->max_key, range->start_key.length);

    if (is_eq_range_pred) {
      range->range_flag = EQ_RANGE;
      /*
        Use statistics instead of index dives for estimates of rows in
        this range if the user requested it
      */
      if (param->use_index_statistics)
        range->range_flag |= SKIP_RECORDS_IN_RANGE;

      /*
        An equality range is a unique range (0 or 1 rows in the range)
        if the index is unique (1) and all keyparts are used (2).
        Note that keys which are extended with PK parts have no
        HA_NOSAME flag. So we can use user_defined_key_parts.
      */
      if (cur_key_info->flags & HA_NOSAME &&  // 1)
          (uint)key_tree->part + 1 ==
              cur_key_info->user_defined_key_parts)  // 2)
        range->range_flag |= UNIQUE_RANGE | (cur->min_key_flag & NULL_RANGE);
    }

    if (*seq->is_ror_scan) {
      const uint key_part_number = key_tree->part + 1;
      /*
        If we get here, the condition on the key was converted to form
        "(keyXpart1 = c1) AND ... AND (keyXpart{key_tree->part - 1} = cN) AND
          somecond(keyXpart{key_tree->part})"
        Check if
          somecond is "keyXpart{key_tree->part} = const" and
          uncovered "tail" of KeyX parts is either empty or is identical to
          first members of clustered primary key.

        If last key part is PK part added to the key as an extension
        and is_key_scan_ror() result is true then it's possible to
        use ROR scan.
      */
      if ((!is_eq_range_pred &&
           key_part_number <= cur_key_info->user_defined_key_parts) ||
          !is_key_scan_ror(param, seq->real_keyno, key_part_number))
        *seq->is_ror_scan = false;
    }
  }

  seq->range_count++;
  seq->max_key_part = max<uint>(seq->max_key_part, key_tree->part);

  if (seq->skip_records_in_range) range->range_flag |= SKIP_RECORDS_IN_RANGE;

  return 0;
}

ha_rows check_quick_select(THD *thd, RANGE_OPT_PARAM *param, uint idx,
                           bool index_only, SEL_ROOT *tree,
                           bool update_tbl_stats, enum_order order_direction,
                           bool skip_records_in_range, uint *mrr_flags,
                           uint *bufsize, Cost_estimate *cost,
                           bool *is_ror_scan, bool *is_imerge_scan) {
  uchar min_key[MAX_KEY_LENGTH + MAX_FIELD_WIDTH];
  uchar max_key[MAX_KEY_LENGTH + MAX_FIELD_WIDTH];

  Sel_arg_range_sequence seq(param, is_ror_scan, min_key, max_key,
                             skip_records_in_range);
  RANGE_SEQ_IF seq_if = {sel_arg_range_seq_init, sel_arg_range_seq_next,
                         nullptr};
  handler *file = param->table->file;
  ha_rows rows;
  uint keynr = param->real_keynr[idx];
  DBUG_TRACE;

  /* Handle cases when we don't have a valid non-empty list of range */
  if (!tree) return HA_POS_ERROR;
  if (tree->type == SEL_ROOT::Type::IMPOSSIBLE) return 0L;
  if (tree->type != SEL_ROOT::Type::KEY_RANGE || tree->root->part != 0)
    return HA_POS_ERROR;  // Don't use tree

  seq.keyno = idx;
  seq.real_keyno = keynr;
  seq.start = tree->root;
  seq.range_count = 0;
  seq.max_key_part = 0;

  /*
    If there are more equality ranges than specified by the
    eq_range_index_dive_limit variable we switches from using index
    dives to use statistics.
  */
  uint range_count = 0;
  param->use_index_statistics = eq_ranges_exceeds_limit(
      tree, &range_count, thd->variables.eq_range_index_dive_limit);
  *is_imerge_scan = true;
  *is_ror_scan = !(file->index_flags(keynr, 0, true) & HA_KEY_SCAN_NOT_ROR);

  *mrr_flags = (order_direction == ORDER_DESC) ? HA_MRR_USE_DEFAULT_IMPL : 0;
  *mrr_flags |= HA_MRR_NO_ASSOCIATION;
  /*
    Pass HA_MRR_SORTED to see if MRR implementation can handle sorting.
  */
  if (order_direction != ORDER_NOT_RELEVANT) *mrr_flags |= HA_MRR_SORTED;

  bool pk_is_clustered = file->primary_key_is_clustered();
  if (index_only &&
      (file->index_flags(keynr, seq.max_key_part, true) & HA_KEYREAD_ONLY) &&
      !(pk_is_clustered && keynr == param->table->s->primary_key))
    *mrr_flags |= HA_MRR_INDEX_ONLY;

  if (thd->lex->sql_command != SQLCOM_SELECT)
    *mrr_flags |= HA_MRR_SORTED;  // Assumed to give faster ins/upd/del

  *bufsize = thd->variables.read_rnd_buff_size;
  // Sets is_ror_scan to false for some queries, e.g. multi-ranges
  bool force_default_mrr = false;
  rows = file->multi_range_read_info_const(keynr, &seq_if, (void *)&seq, 0,
                                           bufsize, mrr_flags,
                                           &force_default_mrr, cost);
  if (rows != HA_POS_ERROR) {
    param->table->quick_rows[keynr] = rows;
    if (update_tbl_stats) {
      param->table->quick_keys.set_bit(keynr);
      param->table->quick_key_parts[keynr] = seq.max_key_part + 1;
      param->table->quick_n_ranges[keynr] = seq.range_count;
      param->table->quick_condition_rows =
          min(param->table->quick_condition_rows, rows);
    }
    param->table->possible_quick_keys.set_bit(keynr);
  }
  /*
    Check whether ROR scan could be used. It cannot be used if
    1. Index algo is not HA_KEY_ALG_BTREE or HA_KEY_ALG_SE_SPECIFIC
       (this mostly covers engines like Archive/Federated.)
       TODO: Don't have this logic here, make table engines return
       appropriate flags instead.
    2. Any of the keyparts in the index chosen is descending. Desc
       indexes do not work well for ROR scans, except for clustered PK.
    3. SE states the index can't be used for ROR. We need 2nd check
       here to avoid enabling it for a non-ROR PK.
    4. Index contains virtual columns. RowIDIntersectionIterator
       and RowIDUnionIterator do read_set manipulations in reset(),
       which breaks virtual generated column's computation logic, which
       is used when reading index values. So, disable index merge
       intersection/union for any index on such column.
       @todo lift this implementation restriction
  */
  // Get the index key algorithm
  enum ha_key_alg key_alg = param->table->key_info[seq.real_keyno].algorithm;

  // Check if index has desc keypart
  KEY_PART_INFO *key_part = param->table->key_info[keynr].key_part;
  KEY_PART_INFO *key_part_end =
      key_part + param->table->key_info[keynr].user_defined_key_parts;
  for (; key_part != key_part_end; ++key_part) {
    if (key_part->key_part_flag & HA_REVERSE_SORT) {
      // ROR will be enabled again for clustered PK, see 'else if' below.
      *is_ror_scan = false;  // 2
      *is_imerge_scan = false;
      break;
    }
  }
  if (((key_alg != HA_KEY_ALG_BTREE) &&
       (key_alg != HA_KEY_ALG_SE_SPECIFIC)) ||                      // 1
      (file->index_flags(keynr, 0, true) & HA_KEY_SCAN_NOT_ROR) ||  // 3
      param->table->index_contains_some_virtual_gcol(keynr))        // 4
  {
    *is_ror_scan = false;
  } else if (param->table->s->primary_key == keynr && pk_is_clustered) {
    /*
      Clustered PK scan is always a ROR scan (TODO: same as above).
      This can enable ROR back if it was disabled by multi_range_read_info_const
      call.
    */
    *is_ror_scan = true;
  }

  DBUG_PRINT("exit", ("Records: %lu", (ulong)rows));
  return rows;
}

/*
  Check if key scan on given index with equality conditions on first n key
  parts is a ROR scan.

  SYNOPSIS
    is_key_scan_ror()
      param  Parameter from test_quick_select
      keynr  Number of key in the table. The key must not be a clustered
             primary key.
      nparts Number of first key parts for which equality conditions
             are present.

  NOTES
    ROR (Rowid Ordered Retrieval) key scan is a key scan that produces
    ordered sequence of rowids (ha_xxx::cmp_ref is the comparison function)

    This function is needed to handle a practically-important special case:
    an index scan is a ROR scan if it is done using a condition in form

        "key1_1=c_1 AND ... AND key1_n=c_n"

    where the index is defined on (key1_1, ..., key1_N [,a_1, ..., a_n])

    and the table has a clustered Primary Key defined as

      PRIMARY KEY(a_1, ..., a_n, b1, ..., b_k)

    i.e. the first key parts of it are identical to uncovered parts of the
    key being scanned. This function assumes that the index flags do not
    include HA_KEY_SCAN_NOT_ROR flag (that is checked elsewhere).

    Check (1) is made in quick_range_seq_next()

  RETURN
    true   The scan is ROR-scan
    false  Otherwise
*/

static bool is_key_scan_ror(RANGE_OPT_PARAM *param, uint keynr, uint nparts) {
  KEY *table_key = param->table->key_info + keynr;

  /*
    Range predicates on hidden key parts do not change the fact
    that a scan is rowid ordered, so we only care about user
    defined keyparts
  */
  const uint user_defined_nparts =
      std::min<uint>(nparts, table_key->user_defined_key_parts);

  KEY_PART_INFO *key_part = table_key->key_part + user_defined_nparts;
  KEY_PART_INFO *key_part_end =
      (table_key->key_part + table_key->user_defined_key_parts);
  uint pk_number;

  for (KEY_PART_INFO *kp = table_key->key_part; kp < key_part; kp++) {
    uint16 fieldnr = param->table->key_info[keynr]
                         .key_part[kp - table_key->key_part]
                         .fieldnr -
                     1;
    if (param->table->field[fieldnr]->key_length() != kp->length) return false;
  }

  if (key_part == key_part_end) return true;

  key_part = table_key->key_part + user_defined_nparts;
  pk_number = param->table->s->primary_key;
  if (!param->table->file->primary_key_is_clustered() || pk_number == MAX_KEY)
    return false;

  KEY_PART_INFO *pk_part = param->table->key_info[pk_number].key_part;
  KEY_PART_INFO *pk_part_end =
      pk_part + param->table->key_info[pk_number].user_defined_key_parts;
  for (; (key_part != key_part_end) && (pk_part != pk_part_end);
       ++key_part, ++pk_part) {
    if ((key_part->field != pk_part->field) ||
        (key_part->length != pk_part->length))
      return false;
  }
  return (key_part == key_part_end);
}

bool get_ranges_from_tree(MEM_ROOT *return_mem_root, TABLE *table,
                          KEY_PART *key, uint keyno, SEL_ROOT *key_tree,
                          uint num_key_parts, unsigned *used_key_parts,
                          unsigned *num_exact_key_parts, Quick_ranges *ranges) {
  *used_key_parts = 0;
  if (key_tree->type != SEL_ROOT::Type::KEY_RANGE) {
    return false;
  }
  const bool first_keypart_is_asc = key_tree->root->is_ascending;
  uchar min_key[MAX_KEY_LENGTH + MAX_FIELD_WIDTH];
  uchar max_key[MAX_KEY_LENGTH + MAX_FIELD_WIDTH];
  *num_exact_key_parts = num_key_parts;
  if (get_ranges_from_tree_given_base(
          current_thd, return_mem_root, &table->key_info[keyno], key, key_tree,
          min_key, min_key, 0, max_key, max_key, 0, first_keypart_is_asc,
          num_key_parts, used_key_parts, num_exact_key_parts, ranges)) {
    return true;
  }
  *num_exact_key_parts = std::min(*num_exact_key_parts, *used_key_parts);
  return false;
}

void trace_basic_info_index_range_scan(THD *thd, const AccessPath *path,
                                       const RANGE_OPT_PARAM *param,
                                       Opt_trace_object *trace_object) {
  assert(param->using_real_indexes);
  const uint keynr_in_table = path->index_range_scan().index;

  const KEY &cur_key = param->table->key_info[keynr_in_table];
  const KEY_PART_INFO *key_part = cur_key.key_part;

  trace_object->add_alnum("type", "range_scan")
      .add_utf8("index", cur_key.name)
      .add("rows", path->num_output_rows());

  Opt_trace_array trace_range(&thd->opt_trace, "ranges");

  String range_info;
  range_info.set_charset(system_charset_info);
  for (QUICK_RANGE *range :
       Bounds_checked_array{path->index_range_scan().ranges,
                            path->index_range_scan().num_ranges}) {
    append_range_to_string(range, key_part, &range_info);
    trace_range.add_utf8(range_info.ptr(), range_info.length());
    range_info.length(0);
  }
}

AccessPath *get_key_scans_params(THD *thd, RANGE_OPT_PARAM *param,
                                 SEL_TREE *tree, bool index_read_must_be_used,
                                 bool update_tbl_stats,
                                 enum_order order_direction,
                                 bool skip_records_in_range,
                                 const double cost_est, bool ror_only,
                                 Key_map *needed_reg) {
  uint idx, best_idx = 0;
  SEL_ROOT *key, *key_to_read = nullptr;
  ha_rows best_records = 0; /* protected by key_to_read */
  uint best_mrr_flags = 0, best_buf_size = 0;
  double read_cost = cost_est;
  DBUG_TRACE;
  Opt_trace_context *const trace = &thd->opt_trace;
  /*
    Note that there may be trees that have type SEL_TREE::KEY but contain no
    key reads at all, e.g. tree for expression "key1 is not null" where key1
    is defined as "not null".
  */
  DBUG_EXECUTE("info",
               print_sel_tree(param, tree, &tree->keys_map, "tree scans"););
  Opt_trace_array ota(trace, "range_scan_alternatives");

  tree->ror_scans_map.clear_all();
  tree->n_ror_scans = 0;
  bool is_best_idx_imerge_scan = true;
  bool use_cheapest_index_merge = false;
  bool force_index_merge =
      idx_merge_hint_state(thd, param->table, &use_cheapest_index_merge);

  for (idx = 0; idx < param->keys; idx++) {
    key = tree->keys[idx];
    if (key) {
      ha_rows found_records;
      Cost_estimate cost;
      uint mrr_flags = 0, buf_size = 0;
      uint keynr = param->real_keynr[idx];
      if (key->type == SEL_ROOT::Type::MAYBE_KEY || key->root->maybe_flag)
        needed_reg->set_bit(keynr);

      bool read_index_only =
          index_read_must_be_used
              ? true
              : (bool)param->table->covering_keys.is_set(keynr);

      Opt_trace_object trace_idx(trace);
      trace_idx.add_utf8("index", param->table->key_info[keynr].name);
      bool is_ror_scan, is_imerge_scan;
      found_records = check_quick_select(
          thd, param, idx, read_index_only, key, update_tbl_stats,
          order_direction, skip_records_in_range, &mrr_flags, &buf_size, &cost,
          &is_ror_scan, &is_imerge_scan);
      if (found_records != HA_POS_ERROR && ror_only && !is_ror_scan) {
        trace_idx.add("chosen", false).add_alnum("cause", "not_rowid_ordered");
        continue;
      }
      if (!compound_hint_key_enabled(param->table, keynr,
                                     INDEX_MERGE_HINT_ENUM)) {
        trace_idx.add("chosen", false).add_alnum("cause", "index_merge_hint");
        continue;
      }

      // check_quick_select() says don't use range if it returns HA_POS_ERROR
      if (found_records != HA_POS_ERROR && thd->opt_trace.is_started()) {
        Opt_trace_array trace_range(&thd->opt_trace, "ranges");

        const KEY &cur_key = param->table->key_info[keynr];
        const KEY_PART_INFO *key_part = cur_key.key_part;

        String range_info;
        range_info.set_charset(system_charset_info);
        append_range_all_keyparts(&trace_range, nullptr, &range_info, key,
                                  key_part, false);
        trace_range.end();  // NOTE: ends the tracing scope

        /// No cost calculation when index dive is skipped.
        if (skip_records_in_range)
          trace_idx.add_alnum("index_dives_for_range_access",
                              "skipped_due_to_force_index");
        else
          trace_idx.add("index_dives_for_eq_ranges",
                        !param->use_index_statistics);

        trace_idx.add("rowid_ordered", is_ror_scan)
            .add("using_mrr", !(mrr_flags & HA_MRR_USE_DEFAULT_IMPL))
            .add("index_only", read_index_only)
            .add("in_memory", cur_key.in_memory_estimate());

        if (skip_records_in_range) {
          trace_idx.add_alnum("rows", "not applicable")
              .add_alnum("cost", "not applicable");
        } else {
          trace_idx.add("rows", found_records).add("cost", cost);
        }
      }

      if ((found_records != HA_POS_ERROR) && is_ror_scan) {
        tree->n_ror_scans++;
        tree->ror_scans_map.set_bit(idx);
      }

      if (found_records != HA_POS_ERROR &&
          (read_cost > cost.total_cost() ||
           /*
             Ignore cost check if INDEX_MERGE hint is used with
             explicitly specified indexes or if INDEX_MERGE hint
             is used without any specified indexes and no best
             index is chosen yet.
           */
           (force_index_merge &&
            (!use_cheapest_index_merge || !key_to_read)))) {
        trace_idx.add("chosen", true);
        read_cost = cost.total_cost();
        best_records = found_records;
        key_to_read = key;
        best_idx = idx;
        best_mrr_flags = mrr_flags;
        best_buf_size = buf_size;
        is_best_idx_imerge_scan = is_imerge_scan;
      } else {
        trace_idx.add("chosen", false);
        if (found_records == HA_POS_ERROR)
          if (key->type == SEL_ROOT::Type::MAYBE_KEY)
            trace_idx.add_alnum("cause", "depends_on_unread_values");
          else
            trace_idx.add_alnum("cause", "no_valid_range_for_this_index");
        else
          trace_idx.add_alnum("cause", "cost");
      }
    }
  }

  DBUG_EXECUTE("info",
               print_sel_tree(param, tree, &tree->ror_scans_map, "ROR scans"););

  if (key_to_read == nullptr) {
    DBUG_PRINT("info", ("No 'range' table read plan found"));
    return nullptr;
  }

  Quick_ranges ranges(param->return_mem_root);
  unsigned used_key_parts, num_exact_key_parts;
  if (get_ranges_from_tree(param->return_mem_root, param->table,
                           param->key[best_idx], param->real_keynr[best_idx],
                           key_to_read, MAX_REF_PARTS, &used_key_parts,
                           &num_exact_key_parts, &ranges)) {
    return nullptr;
  }

  KEY *used_key = &param->table->key_info[param->real_keynr[best_idx]];

  AccessPath *path = new (param->return_mem_root) AccessPath;
  path->type = AccessPath::INDEX_RANGE_SCAN;
  path->set_cost(read_cost);
  path->set_num_output_rows(best_records);
  path->index_range_scan().index = param->real_keynr[best_idx];
  path->index_range_scan().num_used_key_parts = used_key_parts;
  path->index_range_scan().used_key_part = param->key[best_idx];
  path->index_range_scan().ranges = &ranges[0];
  path->index_range_scan().num_ranges = ranges.size();
  path->index_range_scan().mrr_flags = best_mrr_flags;
  path->index_range_scan().mrr_buf_size = best_buf_size;
  path->index_range_scan().can_be_used_for_ror =
      tree->ror_scans_map.is_set(best_idx);
  path->index_range_scan().need_rows_in_rowid_order =
      false;  // May be changed by callers later.
  path->index_range_scan().can_be_used_for_imerge = is_best_idx_imerge_scan;
  path->index_range_scan().reuse_handler = false;
  path->index_range_scan().geometry = (used_key->flags & HA_SPATIAL);
  path->index_range_scan().reverse =
      false;  // May be changed by make_reverse() later.
  DBUG_PRINT("info", ("Returning range plan for key %s, cost %g, records %g",
                      used_key->name, path->cost(), path->num_output_rows()));
  return path;
}

/*
  Return true if any part of the key is NULL

  SYNOPSIS
    null_part_in_key()
      key_part  Array of key parts (index description)
      key       Key values tuple
      length    Length of key values tuple in bytes.

  RETURN
    true   The tuple has at least one "keypartX is NULL"
    false  Otherwise
*/

static bool null_part_in_key(KEY_PART *key_part, const uchar *key,
                             uint length) {
  for (const uchar *end = key + length; key < end;
       key += key_part++->store_length) {
    if (key_part->null_bit && *key) return true;
  }
  return false;
}

// TODO(sgunders): This becomes a bit simpler with C++20's string_view
// constructors.
static inline std::basic_string_view<uchar> make_string_view(const uchar *start,
                                                             const uchar *end) {
  return {start, static_cast<size_t>(end - start)};
}

/**
  Generate key values for range select from given sel_arg tree

  SYNOPSIS
    get_ranges_from_tree_given_base()

  @param thd            THD object
  @param return_mem_root MEM_ROOT to use for allocating the data
  @param key            Generate key values for this key
  @param key_tree       SEL_ARG tree
  @param base_min_key   Start of min key buffer
  @param min_key        Current append place in min key buffer
  @param min_key_flag   Min key's flags
  @param base_max_key   Start of max key buffer
  @param max_key        Current append place in max key buffer
  @param max_key_flag   Max key's flags
  @param first_keypart_is_asc  Whether first keypart is ascending or not
  @param num_key_parts  Number of key parts that should be used for
                        creating ranges
  @param used_key_parts Number of key parts that were ever used in some form
  @param num_exact_key_parts
                        The number of key parts for which we were able to
                        apply the ranges fully (never higher than
                        used_key_parts), subsuming conditions touching
                        that key part.
  @param ranges         The ranges to scan

  @note Fix this to get all possible sub_ranges

  @returns
    true    OOM
    false   Ok
*/

static bool get_ranges_from_tree_given_base(
    THD *thd, MEM_ROOT *return_mem_root, const KEY *table_key, KEY_PART *key,
    SEL_ROOT *key_tree, uchar *const base_min_key, uchar *min_key,
    uint min_key_flag, uchar *const base_max_key, uchar *max_key,
    uint max_key_flag, bool first_keypart_is_asc, uint num_key_parts,
    uint *used_key_parts, uint *num_exact_key_parts, Quick_ranges *ranges) {
  const uint part = key_tree->root->part;
  const bool asc = key_tree->root->is_ascending;

  for (SEL_ARG *node = asc ? key_tree->root->first() : key_tree->root->last();
       node != nullptr && node != null_element;
       node = asc ? node->next : node->prev) {
    int min_part = part - 1,  // # of keypart values in min_key buffer
        max_part = part - 1;  // # of keypart values in max_key buffer
    uchar *tmp_min_key = min_key, *tmp_max_key = max_key;
    node->store_min_max_values(key[part].store_length, &tmp_min_key,
                               min_key_flag, &tmp_max_key, max_key_flag,
                               &min_part, &max_part);

    uint flag;

    // See if we have a range tree for the next keypart.
    if (num_key_parts > 1 && node->next_key_part != nullptr &&
        node->next_key_part->type == SEL_ROOT::Type::KEY_RANGE &&
        node->next_key_part->root->part == part + 1) {
      if (node->min_flag == 0 && node->max_flag == 0 &&
          make_string_view(min_key, tmp_min_key) ==
              make_string_view(max_key, tmp_max_key)) {
        // This range was an equality predicate, and we have more
        // keyparts to scan, so use its range as a base for ranges on
        // the next keypart(s). E.g. if we have (a = 3) on this keypart,
        // and (b < 1 OR b >= 5) on the next one (connected to a = 3),
        // we can use that predicate to build ranges (3,-inf) <= (a,b) < (3,1)
        // and (3,5) <= (a,b) <= (3,+inf). And if so, we don't add a range for
        // (a=3) in itself (which is what the rest of the function is doing),
        // so skip to the next range after processing this one.
        if (get_ranges_from_tree_given_base(
                thd, return_mem_root, table_key, key, node->next_key_part,
                base_min_key, tmp_min_key, min_key_flag | node->get_min_flag(),
                base_max_key, tmp_max_key, max_key_flag | node->get_max_flag(),
                first_keypart_is_asc, num_key_parts - 1, used_key_parts,
                num_exact_key_parts, ranges)) {
          return true;
        }
        continue;
      }

      // We have more keyparts, but we didn't have an equality range.
      // This means we're essentially dropping predicates on those,
      // keyparts, since we cannot express them using simple ranges.
      // However, we can do a last-ditch effort to at least cut off
      // part of the ranges whenever possible.
      //
      // E.g. if we have a >= 3 and the next keypart is on b, we would
      // normally have a range a >= 3 (set up by the call to
      // store_min_max_values() above) with the key not extended to b;
      // effectively, the same as (a,b) >= (3,-inf). However, we can look
      // through the range tree for b and limit our sub-range to the smallest
      // value it could have. So e.g. for (a >= 3) AND (b IN (4, 9, 10)),
      // we would start scan over (a,b) >= (3,4) instead. (Sometimes, this would
      // include adjusting min/max flags.) We work similarly for the upper end
      // of the range.
      uint tmp_min_flag = node->get_min_flag();
      uint tmp_max_flag = node->get_max_flag();
      node->store_next_min_max_keys(key, &tmp_min_key, &tmp_min_flag,
                                    &tmp_max_key, &tmp_max_flag, &min_part,
                                    &max_part);
      flag = tmp_min_flag | tmp_max_flag;
    } else if (node->min_flag & GEOM_FLAG) {
      assert(asc);
      flag = node->min_flag;
    } else if (asc) {
      flag = node->min_flag | node->max_flag;
    } else {
      // Invert flags for DESC keypart
      flag = invert_min_flag(node->min_flag) | invert_max_flag(node->max_flag);
    }

    if (node->next_key_part != nullptr &&
        part + num_key_parts >= node->next_key_part->root->part) {
      // We necessarily skipped something in the next keypart (see above),
      // so note that. The caller can use this information to know that it
      // cannot subsume any predicates that touch on that (or any later)
      // keyparts, but must recheck them using a filter. (The old join optimizer
      // always checks, but the hypergraph join optimizer is more precise.)
      *num_exact_key_parts =
          std::min<uint>(*num_exact_key_parts, node->next_key_part->root->part);
    }

    /*
      Ensure that some part of min_key and max_key are used.  If not,
      regard this as no lower/upper range
    */
    if ((flag & GEOM_FLAG) == 0) {
      if (tmp_min_key != base_min_key)
        flag &= ~NO_MIN_RANGE;
      else
        flag |= NO_MIN_RANGE;
      if (tmp_max_key != base_max_key)
        flag &= ~NO_MAX_RANGE;
      else
        flag |= NO_MAX_RANGE;
    }
    if (flag == 0 && make_string_view(base_min_key, tmp_min_key) ==
                         make_string_view(base_max_key, tmp_max_key)) {
      flag |= EQ_RANGE;
      /*
        Note that keys which are extended with PK parts have no
        HA_NOSAME flag. So we can use user_defined_key_parts.
      */
      if ((table_key->flags & HA_NOSAME) &&
          part == table_key->user_defined_key_parts - 1) {
        if ((table_key->flags & HA_NULL_PART_KEY) &&
            null_part_in_key(key, base_min_key,
                             (uint)(tmp_min_key - base_min_key)))
          flag |= NULL_RANGE;
        else
          flag |= UNIQUE_RANGE;
      }
    }

    /*
      Set DESC flag. We need this flag set according to the first keypart.
      Depending on it, key values will be scanned either forward or backward,
      preserving the order or records in the index along multiple ranges.
    */
    if (!first_keypart_is_asc) {
      flag |= DESC_FLAG;
    }

    assert(!thd->m_mem_cnt.is_error());
    /* Get range for retrieving rows in RowIterator::Read() */
    QUICK_RANGE *range = new (return_mem_root) QUICK_RANGE(
        return_mem_root, base_min_key, (uint)(tmp_min_key - base_min_key),
        min_part >= 0 ? make_keypart_map(min_part) : 0, base_max_key,
        (uint)(tmp_max_key - base_max_key),
        max_part >= 0 ? make_keypart_map(max_part) : 0, flag,
        node->rkey_func_flag);
    if (range == nullptr || thd->killed) {
      return true;  // out of memory
    }

    *used_key_parts = std::max(*used_key_parts, part + 1);
    if (ranges->push_back(range)) return true;
  }
  return false;
}

/**
  Traverse the R-B range tree for this and later keyparts to see if
  there are at least as many equality ranges as defined by the limit.

  @param keypart        The R-B tree of ranges for a given keypart.
  @param [in,out] count The number of equality ranges found so far
  @param limit          The number of ranges

  @retval true if limit > 0 and 'limit' or more equality ranges have been
          found in the range R-B trees
  @retval false otherwise

*/

static bool eq_ranges_exceeds_limit(const SEL_ROOT *keypart, uint *count,
                                    uint limit) {
  // "Statistics instead of index dives" feature is turned off
  if (limit == 0) return false;

  /*
    Optimization: if there is at least one equality range, index
    statistics will be used when limit is 1. It's safe to return true
    even without checking that there is an equality range because if
    there are none, index statistics will not be used anyway.
  */
  if (limit == 1) return true;

  for (SEL_ARG *keypart_range = keypart->root->first(); keypart_range;
       keypart_range = keypart_range->next) {
    /*
      This is an equality range predicate and should be counted if:
      1) the range for this keypart does not have a min/max flag
         (which indicates <, <= etc), and
      2) the lower and upper range boundaries have the same value
         (it's not a "x BETWEEN a AND b")

      Note, however, that if this is an "x IS NULL" condition we don't
      count it because the number of NULL-values is likely to be off
      the index statistics we plan to use.
    */
    if (!keypart_range->min_flag && !keypart_range->max_flag &&  // 1)
        !keypart_range->cmp_max_to_min(keypart_range) &&         // 2)
        !keypart_range->is_null_interval())                      // "x IS NULL"
    {
      /*
         Count predicates in the next keypart, but only if that keypart
         is the next in the index.
      */
      if (keypart_range->next_key_part &&
          keypart_range->next_key_part->root->part == keypart_range->part + 1)
        eq_ranges_exceeds_limit(keypart_range->next_key_part, count, limit);
      else
        // We've found a path of equlity predicates down to a keypart leaf
        (*count)++;

      if (*count >= limit) return true;
    }
  }
  return false;
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

void dbug_dump_range(int indent, bool verbose, TABLE *table, int index,
                     KEY_PART *used_key_part,
                     Bounds_checked_array<QUICK_RANGE *> ranges) {
  /* purecov: begin inspected */
  int max_used_key_length = 0;
  for (const QUICK_RANGE *range : ranges) {
    max_used_key_length = std::max<int>(max_used_key_length, range->min_length);
    max_used_key_length = std::max<int>(max_used_key_length, range->max_length);
  }
  fprintf(DBUG_FILE, "%*squick range select, key %s, length: %d\n", indent, "",
          table->key_info[index].name, max_used_key_length);

  if (verbose) {
    for (size_t ix = 0; ix < ranges.size(); ++ix) {
      fprintf(DBUG_FILE, "%*s", indent + 2, "");
      QUICK_RANGE *range = ranges[ix];
      if (!(range->flag & NO_MIN_RANGE)) {
        print_multiple_key_values(used_key_part, range->min_key,
                                  range->min_length);
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
        print_multiple_key_values(used_key_part, range->max_key,
                                  range->max_length);
      }
      fputs("\n", DBUG_FILE);
    }
  }
  /* purecov: end */
}

#endif
