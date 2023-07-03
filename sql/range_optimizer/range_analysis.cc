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
#include <string.h>
#include <sys/types.h>

#include "field_types.h"
#include "m_ctype.h"
#include "memory_debugging.h"
#include "mf_wcomp.h"
#include "my_alloc.h"
#include "my_base.h"
#include "my_byteorder.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_table_map.h"
#include "mysql/udf_registration_types.h"
#include "mysql_com.h"
#include "mysqld_error.h"
#include "sql-common/json_dom.h"
#include "sql/current_thd.h"
#include "sql/derror.h"
#include "sql/field.h"
#include "sql/handler.h"
#include "sql/item.h"
#include "sql/item_cmpfunc.h"
#include "sql/item_func.h"
#include "sql/item_json_func.h"
#include "sql/item_row.h"
#include "sql/key.h"
#include "sql/mem_root_array.h"
#include "sql/opt_trace.h"
#include "sql/opt_trace_context.h"
#include "sql/query_options.h"
#include "sql/range_optimizer/internal.h"
#include "sql/range_optimizer/range_optimizer.h"
#include "sql/range_optimizer/tree.h"
#include "sql/sql_bitmap.h"
#include "sql/sql_class.h"
#include "sql/sql_const.h"
#include "sql/sql_error.h"
#include "sql/sql_lex.h"
#include "sql/sql_list.h"
#include "sql/sql_optimizer.h"
#include "sql/system_variables.h"
#include "sql/table.h"
#include "sql_string.h"
#include "template_utils.h"

/*
  A null_sel_tree is used in get_func_mm_tree_from_in_predicate to pass
  as an argument to tree_or. It is used only to influence the return
  value from tree_or function.
*/

static MEM_ROOT null_root;
static SEL_TREE null_sel_tree(SEL_TREE::IMPOSSIBLE, &null_root, 0);

static uchar is_null_string[2] = {1, 0};

static SEL_TREE *get_mm_parts(THD *thd, RANGE_OPT_PARAM *param,
                              table_map prev_tables, table_map read_tables,
                              Item_func *cond_func, Field *field,
                              Item_func::Functype type, Item *value);
static SEL_ROOT *get_mm_leaf(THD *thd, RANGE_OPT_PARAM *param, Item *cond_func,
                             Field *field, KEY_PART *key_part,
                             Item_func::Functype type, Item *value,
                             bool *inexact);
static SEL_TREE *get_full_func_mm_tree(THD *thd, RANGE_OPT_PARAM *param,
                                       table_map prev_tables,
                                       table_map read_tables,
                                       table_map current_table,
                                       bool remove_jump_scans, Item *predicand,
                                       Item_func *op, Item *value, bool inv);
static SEL_ROOT *sel_add(SEL_ROOT *key1, SEL_ROOT *key2);

/**
   If EXPLAIN or if the --safe-updates option is enabled, add a warning that
   the index cannot be used for range access due to either type conversion or
   different collations on the field used for comparison

   @param thd        Thread handle
   @param param      RANGE_OPT_PARAM from test_quick_select
   @param key_num    Key number
   @param field      Field in the predicate
 */
static void warn_index_not_applicable(THD *thd, const RANGE_OPT_PARAM *param,
                                      const uint key_num, const Field *field) {
  if (param->using_real_indexes &&
      (thd->lex->is_explain() ||
       thd->variables.option_bits & OPTION_SAFE_UPDATES))
    push_warning_printf(thd, Sql_condition::SL_WARNING,
                        ER_WARN_INDEX_NOT_APPLICABLE,
                        ER_THD(thd, ER_WARN_INDEX_NOT_APPLICABLE), "range",
                        field->table->key_info[param->real_keynr[key_num]].name,
                        field->field_name);
}

/*
  Build a SEL_TREE for <> or NOT BETWEEN predicate

  SYNOPSIS
    get_ne_mm_tree()
      param       RANGE_OPT_PARAM from test_quick_select
      prev_tables See test_quick_select()
      read_tables See test_quick_select()
      remove_jump_scans See get_mm_tree()
      cond_func   item for the predicate
      field       field in the predicate
      lt_value    constant that field should be smaller
      gt_value    constant that field should be greaterr

  RETURN
    #  Pointer to tree built tree
    0  on error
*/
static SEL_TREE *get_ne_mm_tree(THD *thd, RANGE_OPT_PARAM *param,
                                table_map prev_tables, table_map read_tables,
                                bool remove_jump_scans, Item_func *cond_func,
                                Field *field, Item *lt_value, Item *gt_value) {
  SEL_TREE *tree = nullptr;

  if (param->has_errors()) return nullptr;

  tree = get_mm_parts(thd, param, prev_tables, read_tables, cond_func, field,
                      Item_func::LT_FUNC, lt_value);
  if (tree) {
    tree = tree_or(param, remove_jump_scans, tree,
                   get_mm_parts(thd, param, prev_tables, read_tables, cond_func,
                                field, Item_func::GT_FUNC, gt_value));
  }
  return tree;
}

/**
  Factory function to build a SEL_TREE from an @<in predicate@>

  @param thd        Thread handle
  @param param      Information on 'just about everything'.
  @param prev_tables See test_quick_select()
  @param read_tables See test_quick_select()
  @param remove_jump_scans See get_mm_tree()
  @param predicand  The @<in predicate's@> predicand, i.e. the left-hand
                    side of the @<in predicate@> expression.
  @param op         The 'in' operator itself.
  @param is_negated If true, the operator is NOT IN, otherwise IN.
*/
static SEL_TREE *get_func_mm_tree_from_in_predicate(
    THD *thd, RANGE_OPT_PARAM *param, table_map prev_tables,
    table_map read_tables, bool remove_jump_scans, Item *predicand,
    Item_func_in *op, bool is_negated) {
  if (param->has_errors()) return nullptr;

  // Populate array as we need to examine its values here
  if (op->m_const_array != nullptr && !op->m_populated) {
    op->populate_bisection(thd);
  }
  if (is_negated) {
    // We don't support row constructors (multiple columns on lhs) here.
    if (predicand->type() != Item::FIELD_ITEM) return nullptr;

    Field *field = down_cast<Item_field *>(predicand)->field;

    if (op->m_const_array != nullptr && !op->m_const_array->is_row_result()) {
      /*
        We get here for conditions on the form "t.key NOT IN (c1, c2, ...)",
        where c{i} are constants. Our goal is to produce a SEL_TREE that
        represents intervals:

        ($MIN<t.key<c1) OR (c1<t.key<c2) OR (c2<t.key<c3) OR ...    (*)

        where $MIN is either "-inf" or NULL.

        The most straightforward way to produce it is to convert NOT
        IN into "(t.key != c1) AND (t.key != c2) AND ... " and let the
        range analyzer build a SEL_TREE from that. The problem is that
        the range analyzer will use O(N^2) memory (which is probably a
        bug), and people who do use big NOT IN lists (e.g. see
        BUG#15872, BUG#21282), will run out of memory.

        Another problem with big lists like (*) is that a big list is
        unlikely to produce a good "range" access, while considering
        that range access will require expensive CPU calculations (and
        for MyISAM even index accesses). In short, big NOT IN lists
        are rarely worth analyzing.

        Considering the above, we'll handle NOT IN as follows:

        - if the number of entries in the NOT IN list is less than
          NOT_IN_IGNORE_THRESHOLD, construct the SEL_TREE (*)
          manually.

        - Otherwise, don't produce a SEL_TREE.
      */

      const uint NOT_IN_IGNORE_THRESHOLD = 1000;
      // If we have t.key NOT IN (null, null, ...) or the list is too long
      if (op->m_const_array->m_used_size == 0 ||
          op->m_const_array->m_used_size > NOT_IN_IGNORE_THRESHOLD)
        return nullptr;

      /*
        Create one Item_type constant object. We'll need it as
        get_mm_parts only accepts constant values wrapped in Item_Type
        objects.
        We create the Item on thd->mem_root which points to
        per-statement mem_root.
      */
      Item_basic_constant *value_item =
          op->m_const_array->create_item(thd->mem_root);
      if (value_item == nullptr) return nullptr;

      /* Get a SEL_TREE for "(-inf|NULL) < X < c_0" interval.  */
      uint i = 0;
      SEL_TREE *tree = nullptr;
      do {
        op->m_const_array->value_to_item(i, value_item);
        tree = get_mm_parts(thd, param, prev_tables, read_tables, op, field,
                            Item_func::LT_FUNC, value_item);
        if (!tree) break;
        i++;
      } while (i < op->m_const_array->m_used_size &&
               tree->type == SEL_TREE::IMPOSSIBLE);

      if (!tree || tree->type == SEL_TREE::IMPOSSIBLE)
        /* We get here in cases like "t.unsigned NOT IN (-1,-2,-3) */
        return nullptr;
      SEL_TREE *tree2 = nullptr;
      Item_basic_constant *previous_range_value =
          op->m_const_array->create_item(thd->mem_root);
      for (; i < op->m_const_array->m_used_size; i++) {
        // Check if the value stored in the field for the previous range
        // is greater, lesser or equal to the actual value specified in the
        // query. Used further down to set the flags for the current range
        // correctly (as the max value for the previous range will become
        // the min value for the current range).
        op->m_const_array->value_to_item(i - 1, previous_range_value);
        int cmp_value =
            stored_field_cmp_to_item(thd, field, previous_range_value);
        if (op->m_const_array->compare_elems(i, i - 1)) {
          /* Get a SEL_TREE for "-inf < X < c_i" interval */
          op->m_const_array->value_to_item(i, value_item);
          tree2 = get_mm_parts(thd, param, prev_tables, read_tables, op, field,
                               Item_func::LT_FUNC, value_item);
          if (!tree2) {
            tree = nullptr;
            break;
          }

          /* Change all intervals to be "c_{i-1} < X < c_i" */
          for (uint idx = 0; idx < param->keys; idx++) {
            SEL_ARG *last_val;
            if (tree->keys[idx] && tree2->keys[idx] &&
                ((last_val = tree->keys[idx]->root->last()))) {
              SEL_ARG *new_interval = tree2->keys[idx]->root;
              new_interval->min_value = last_val->max_value;
              // We set the max value of the previous range as the beginning
              // for this range interval. However we need values higher than
              // this value:
              // For ex: If the range is "not in (1,2)" we first construct
              // X < 1 before this loop and add 1 < X < 2 in this loop and
              // follow it up with 2 < X below.
              // While fetching values for the second interval, we set
              // "NEAR_MIN" flag so that we fetch values higher than "1".
              // However, when the values specified are not compatible
              // with the field that is being compared to, they are rounded
              // off.
              // For the example above, if the range given was "not in (0.9,
              // 1.9)", range optimizer rounds of the values to (1,2). In such
              // a case, setting the flag to "NEAR_MIN" is not right. Because
              // we need values higher than "0.9" not "1". We check this
              // before we set the flag below.
              if (cmp_value <= 0)
                new_interval->min_flag = NEAR_MIN;
              else
                new_interval->min_flag = 0;

              /*
                If the interval is over a partial keypart, the
                interval must be "c_{i-1} <= X < c_i" instead of
                "c_{i-1} < X < c_i". Reason:

                Consider a table with a column "my_col VARCHAR(3)",
                and an index with definition
                "INDEX my_idx my_col(1)". If the table contains rows
                with my_col values "f" and "foo", the index will not
                distinguish the two rows.

                Note that tree_or() below will effectively merge
                this range with the range created for c_{i-1} and
                we'll eventually end up with only one range:
                "NULL < X".

                Partitioning indexes are never partial.
              */
              if (param->using_real_indexes) {
                const KEY key = param->table->key_info[param->real_keynr[idx]];
                const KEY_PART_INFO *kpi = key.key_part + new_interval->part;

                if (kpi->key_part_flag & HA_PART_KEY_SEG)
                  new_interval->min_flag = 0;
              }
            }
          }
          /*
            The following doesn't try to allocate memory so no need to
            check for NULL.
          */
          tree = tree_or(param, remove_jump_scans, tree, tree2);
        }
      }

      if (tree && tree->type != SEL_TREE::IMPOSSIBLE) {
        /*
          Get the SEL_TREE for the last "c_last < X < +inf" interval
          (value_item contains c_last already)
        */
        tree2 = get_mm_parts(thd, param, prev_tables, read_tables, op, field,
                             Item_func::GT_FUNC, value_item);
        tree = tree_or(param, remove_jump_scans, tree, tree2);
      }
      return tree;
    } else {
      SEL_TREE *tree = get_ne_mm_tree(thd, param, prev_tables, read_tables,
                                      remove_jump_scans, op, field,
                                      op->arguments()[1], op->arguments()[1]);
      if (tree) {
        Item **arg, **end;
        for (arg = op->arguments() + 2, end = arg + op->argument_count() - 2;
             arg < end; arg++) {
          tree = tree_and(
              param, tree,
              get_ne_mm_tree(thd, param, prev_tables, read_tables,
                             remove_jump_scans, op, field, *arg, *arg));
        }
      }
      return tree;
    }
    return nullptr;
  }

  // The expression is IN, not negated.
  if (predicand->type() == Item::FIELD_ITEM) {
    // The expression is (<column>) IN (...)
    Field *field = down_cast<Item_field *>(predicand)->field;
    SEL_TREE *tree =
        get_mm_parts(thd, param, prev_tables, read_tables, op, field,
                     Item_func::EQ_FUNC, op->arguments()[1]);
    if (tree) {
      Item **arg, **end;
      for (arg = op->arguments() + 2, end = arg + op->argument_count() - 2;
           arg < end; arg++) {
        tree = tree_or(param, remove_jump_scans, tree,
                       get_mm_parts(thd, param, prev_tables, read_tables, op,
                                    field, Item_func::EQ_FUNC, *arg));
      }
    }
    return tree;
  }
  if (predicand->type() == Item::ROW_ITEM) {
    /*
      The expression is (<column>,...) IN (...)

      We iterate over the rows on the rhs of the in predicate,
      building an OR tree of ANDs, a.k.a. a DNF expression out of this. E.g:

      (col1, col2) IN ((const1, const2), (const3, const4))
      becomes
      (col1 = const1 AND col2 = const2) OR (col1 = const3 AND col2 = const4)
    */
    SEL_TREE *or_tree = &null_sel_tree;
    Item_row *row_predicand = down_cast<Item_row *>(predicand);

    // Iterate over the rows on the rhs of the in predicate, building an OR.
    for (uint i = 1; i < op->argument_count(); ++i) {
      /*
        We only support row value expressions. Some optimizations rewrite
        the Item tree, and we don't handle that.
      */
      Item *in_list_item = op->arguments()[i];
      if (in_list_item->type() != Item::ROW_ITEM) return nullptr;
      Item_row *row = static_cast<Item_row *>(in_list_item);

      // Iterate over the columns, building an AND tree.
      SEL_TREE *and_tree = nullptr;
      for (uint j = 0; j < row_predicand->cols(); ++j) {
        Item *item = row_predicand->element_index(j);

        // We only support columns in the row on the lhs of the in predicate.
        if (item->type() != Item::FIELD_ITEM) return nullptr;
        Field *field = static_cast<Item_field *>(item)->field;

        Item *value = row->element_index(j);

        SEL_TREE *and_expr = get_mm_parts(thd, param, prev_tables, read_tables,
                                          op, field, Item_func::EQ_FUNC, value);

        and_tree = tree_and(param, and_tree, and_expr);
        /*
          Short-circuit evaluation: If and_expr is NULL then no key part in
          this disjunct can be used as a search key. Or in other words the
          condition is always true. Hence the whole disjunction is always true.
        */
        if (and_tree == nullptr) return nullptr;
      }
      or_tree = tree_or(param, remove_jump_scans, and_tree, or_tree);
    }
    return or_tree;
  }
  return nullptr;
}

/**
  Factory function to build a SEL_TREE from a JSON_OVERLAPS or JSON_CONTAINS
  functions

  \verbatim
    This function builds SEL_TREE out of JSON_OEVRLAPS() of form:
      JSON_OVERLAPS(typed_array_field, "[<val>,...,<val>]")
      JSON_OVERLAPS("[<val>,...,<val>]", typed_array_field)
      JSON_CONTAINS(typed_array_field, "[<val>,...,<val>]")
    where
      typed_array_field is a field which has multi-valued index defined on it
      <val>             each value in the array is coercible to the array's
                        type
    These conditions are pre-checked in substitute_gc().
  \endverbatim
  @param thd        Thread handle
  @param param      Information on 'just about everything'.
  @param prev_tables See test_quick_select()
  @param read_tables See test_quick_select()
  @param remove_jump_scans See get_mm_tree()
  @param predicand  the typed array JSON_CONTAIN's argument
  @param op         The 'JSON_OVERLAPS' operator itself.

  @returns
    non-NULL constructed SEL_TREE
    NULL     in case of any error
*/

static SEL_TREE *get_func_mm_tree_from_json_overlaps_contains(
    THD *thd, RANGE_OPT_PARAM *param, table_map prev_tables,
    table_map read_tables, bool remove_jump_scans, Item *predicand,
    Item_func *op) {
  if (param->has_errors()) return nullptr;

  // The expression is JSON_OVERLAPS(<array_field>,<JSON array/scalar>), or
  // The expression is JSON_OVERLAPS(<JSON array/scalar>, <array_field>), or
  // The expression is JSON_CONTAINS(<array_field>, <JSON array/scalar>)
  if (predicand->type() == Item::FIELD_ITEM && predicand->returns_array()) {
    Json_wrapper wr, elt;
    String str;
    uint values;
    if (op->functype() == Item_func::JSON_OVERLAPS) {
      // If the predicand is the 1st arg, then the values arg is 2nd.
      values = (predicand == op->arguments()[0]) ? 1 : 0;
    } else {
      assert(op->functype() == Item_func::JSON_CONTAINS);
      values = 1;
    }
    if (get_json_wrapper(op->arguments(), values, &str, op->func_name(), &wr))
      return nullptr; /* purecov: inspected */

    // Should be pre-checked already
    assert(!(op->arguments()[values])->null_value &&
           wr.type() != enum_json_type::J_OBJECT &&
           wr.type() != enum_json_type::J_ERROR);
    if (wr.length() == 0) return nullptr;

    Field_typed_array *field = down_cast<Field_typed_array *>(
        down_cast<Item_field *>(predicand)->field);
    if (wr.type() == enum_json_type::J_ARRAY)
      wr.remove_duplicates(
          field->type() == MYSQL_TYPE_VARCHAR ? field->charset() : nullptr);
    size_t i = 0;
    const size_t len = (wr.type() == enum_json_type::J_ARRAY) ? wr.length() : 1;
    // Skip leading JSON null values as they can't be indexed and thus doesn't
    // exist in index.
    while (i < len && wr[i].type() == enum_json_type::J_NULL) ++i;
    // No non-null values were found.
    if (i == len) return nullptr;

    // Fake const table for get_mm_parts, as we're using constants from JSON
    // array
    const bool save_const = field->table->const_table;
    field->table->const_table = true;

    field->set_notnull();

    // Get the SEL_ARG tree for the first non-null element..
    elt = wr[i++];
    field->coerce_json_value(&elt, true, nullptr);
    SEL_TREE *tree =
        get_mm_parts(thd, param, prev_tables, read_tables, op, field,
                     Item_func::EQ_FUNC, down_cast<Item_field *>(predicand));
    // .. and OR with others
    if (tree) {
      for (; i < len; i++) {
        elt = wr[i];
        field->coerce_json_value(&elt, true, nullptr);
        tree = tree_or(param, remove_jump_scans, tree,
                       get_mm_parts(thd, param, prev_tables, read_tables, op,
                                    field, Item_func::EQ_FUNC,
                                    down_cast<Item_field *>(predicand)));
        if (!tree)  // OOM
          break;
      }
    }
    field->table->const_table = save_const;
    return tree;
  }
  return nullptr;
}

/**
  Build a SEL_TREE for a simple predicate.

  @param param     RANGE_OPT_PARAM from test_quick_select
  @param remove_jump_scans See get_mm_tree()
  @param predicand field in the predicate
  @param cond_func item for the predicate
  @param value     constant in the predicate
  @param inv       true <> NOT cond_func is considered
                  (makes sense only when cond_func is BETWEEN or IN)

  @return Pointer to the built tree.

  @todo Remove the appaling hack that 'value' can be a 1 cast to an Item*.
*/

static SEL_TREE *get_func_mm_tree(THD *thd, RANGE_OPT_PARAM *param,
                                  table_map prev_tables, table_map read_tables,
                                  bool remove_jump_scans, Item *predicand,
                                  Item_func *cond_func, Item *value, bool inv) {
  SEL_TREE *tree = nullptr;
  DBUG_TRACE;

  if (param->has_errors()) return nullptr;

  switch (cond_func->functype()) {
    case Item_func::XOR_FUNC:
      return nullptr;  // Always true (don't use range access on XOR).
      break;           // See WL#5800

    case Item_func::NE_FUNC:
      if (predicand->type() == Item::FIELD_ITEM) {
        Field *field = down_cast<Item_field *>(predicand)->field;
        tree =
            get_ne_mm_tree(thd, param, prev_tables, read_tables,
                           remove_jump_scans, cond_func, field, value, value);
      }
      break;

    case Item_func::BETWEEN:
      if (predicand->type() == Item::FIELD_ITEM) {
        Field *field = down_cast<Item_field *>(predicand)->field;

        if (!value) {
          if (inv) {
            tree = get_ne_mm_tree(thd, param, prev_tables, read_tables,
                                  remove_jump_scans, cond_func, field,
                                  cond_func->arguments()[1],
                                  cond_func->arguments()[2]);
          } else {
            tree = get_mm_parts(thd, param, prev_tables, read_tables, cond_func,
                                field, Item_func::GE_FUNC,
                                cond_func->arguments()[1]);
            if (tree) {
              tree = tree_and(param, tree,
                              get_mm_parts(thd, param, prev_tables, read_tables,
                                           cond_func, field, Item_func::LE_FUNC,
                                           cond_func->arguments()[2]));
            }
          }
        } else
          tree = get_mm_parts(
              thd, param, prev_tables, read_tables, cond_func, field,
              (inv ? (value == reinterpret_cast<Item *>(1) ? Item_func::GT_FUNC
                                                           : Item_func::LT_FUNC)
                   : (value == reinterpret_cast<Item *>(1)
                          ? Item_func::LE_FUNC
                          : Item_func::GE_FUNC)),
              cond_func->arguments()[0]);
      }
      break;
    case Item_func::IN_FUNC: {
      Item_func_in *in_pred = down_cast<Item_func_in *>(cond_func);
      tree = get_func_mm_tree_from_in_predicate(thd, param, prev_tables,
                                                read_tables, remove_jump_scans,
                                                predicand, in_pred, inv);
    } break;
    case Item_func::JSON_CONTAINS:
    case Item_func::JSON_OVERLAPS: {
      tree = get_func_mm_tree_from_json_overlaps_contains(
          thd, param, prev_tables, read_tables, remove_jump_scans, predicand,
          cond_func);
    } break;

    case Item_func::MEMBER_OF_FUNC:
      if (predicand->type() == Item::FIELD_ITEM && predicand->returns_array()) {
        Field_typed_array *field = down_cast<Field_typed_array *>(
            down_cast<Item_field *>(predicand)->field);
        Item *arg = cond_func->arguments()[0];

        Json_wrapper wr;
        if (arg->val_json(&wr)) {
          break;
        }

        assert(!arg->null_value && wr.type() != enum_json_type::J_ERROR);

        if (wr.type() == enum_json_type::J_NULL) {
          break;
        }

        // Fake const table for get_mm_parts(), as we are using constants from
        // JSON array

        const bool save_const = field->table->const_table;
        field->table->const_table = true;
        field->set_notnull();
        field->coerce_json_value(&wr, true, nullptr);

        tree = get_mm_parts(thd, param, prev_tables, read_tables, cond_func,
                            field, Item_func::EQ_FUNC, predicand);

        field->table->const_table = save_const;
      }
      break;

    default:
      if (predicand->type() == Item::FIELD_ITEM) {
        Field *field = down_cast<Item_field *>(predicand)->field;

        /*
           Here the function for the following predicates are processed:
           <, <=, =, >=, >, LIKE, IS NULL, IS NOT NULL and GIS functions.
           If the predicate is of the form (value op field) it is handled
           as the equivalent predicate (field rev_op value), e.g.
           2 <= a is handled as a >= 2.
        */
        Item_func::Functype func_type =
            (value != cond_func->arguments()[0])
                ? cond_func->functype()
                : ((Item_bool_func2 *)cond_func)->rev_functype();
        tree = get_mm_parts(thd, param, prev_tables, read_tables, cond_func,
                            field, func_type, value);
      }
  }

  return tree;
}

/*
  Build conjunction of all SEL_TREEs for a simple predicate applying equalities

  SYNOPSIS
    get_full_func_mm_tree()
      param       RANGE_OPT_PARAM from test_quick_select
     prev_tables  See test_quick_select()
     read_tables  See test_quick_select()
      remove_jump_scans See get_mm_tree()
      predicand   column or row constructor in the predicate's left-hand side.
      op          Item for the predicate operator
      value       constant in the predicate (or a field already read from
                  a table in the case of dynamic range access)
                  For BETWEEN it contains the number of the field argument.
      inv         If true, the predicate is negated, e.g. NOT IN.
                  (makes sense only when cond_func is BETWEEN or IN)

  DESCRIPTION
    For a simple SARGable predicate of the form (f op c), where f is a field and
    c is a constant, the function builds a conjunction of all SEL_TREES that can
    be obtained by the substitution of f for all different fields equal to f.

  NOTES
    If the WHERE condition contains a predicate (fi op c),
    then not only SELL_TREE for this predicate is built, but
    the trees for the results of substitution of fi for
    each fj belonging to the same multiple equality as fi
    are built as well.
    E.g. for WHERE t1.a=t2.a AND t2.a > 10
    a SEL_TREE for t2.a > 10 will be built for quick select from t2
    and
    a SEL_TREE for t1.a > 10 will be built for quick select from t1.

    A BETWEEN predicate of the form (fi [NOT] BETWEEN c1 AND c2) is treated
    in a similar way: we build a conjunction of trees for the results
    of all substitutions of fi for equal fj.
    Yet a predicate of the form (c BETWEEN f1i AND f2i) is processed
    differently. It is considered as a conjunction of two SARGable
    predicates (f1i <= c) and (f2i <=c) and the function get_full_func_mm_tree
    is called for each of them separately producing trees for
       AND j (f1j <=c ) and AND j (f2j <= c)
    After this these two trees are united in one conjunctive tree.
    It's easy to see that the same tree is obtained for
       AND j,k (f1j <=c AND f2k<=c)
    which is equivalent to
       AND j,k (c BETWEEN f1j AND f2k).
    The validity of the processing of the predicate (c NOT BETWEEN f1i AND f2i)
    which equivalent to (f1i > c OR f2i < c) is not so obvious. Here the
    function get_full_func_mm_tree is called for (f1i > c) and (f2i < c)
    producing trees for AND j (f1j > c) and AND j (f2j < c). Then this two
    trees are united in one OR-tree. The expression
      (AND j (f1j > c) OR AND j (f2j < c)
    is equivalent to the expression
      AND j,k (f1j > c OR f2k < c)
    which is just a translation of
      AND j,k (c NOT BETWEEN f1j AND f2k)

    In the cases when one of the items f1, f2 is a constant c1 we do not create
    a tree for it at all. It works for BETWEEN predicates but does not
    work for NOT BETWEEN predicates as we have to evaluate the expression
    with it. If it is true then the other tree can be completely ignored.
    We do not do it now and no trees are built in these cases for
    NOT BETWEEN predicates.

    As to IN predicates only ones of the form (f IN (c1,...,cn)),
    where f1 is a field and c1,...,cn are constant, are considered as
    SARGable. We never try to narrow the index scan using predicates of
    the form (c IN (c1,...,f,...,cn)).

  RETURN
    Pointer to the tree representing the built conjunction of SEL_TREEs
*/

static SEL_TREE *get_full_func_mm_tree(THD *thd, RANGE_OPT_PARAM *param,
                                       table_map prev_tables,
                                       table_map read_tables,
                                       table_map current_table,
                                       bool remove_jump_scans, Item *predicand,
                                       Item_func *op, Item *value, bool inv) {
  SEL_TREE *tree = nullptr;
  SEL_TREE *ftree = nullptr;
  const table_map param_comp = ~(prev_tables | read_tables | current_table);
  DBUG_TRACE;

  if (param->has_errors()) return nullptr;

  /*
    Here we compute a set of tables that we consider as constants
    suppliers during execution of the SEL_TREE that we produce below.
  */
  table_map ref_tables = 0;
  for (uint i = 0; i < op->arg_count; i++) {
    Item *arg = op->arguments()[i]->real_item();
    if (arg != predicand) ref_tables |= arg->used_tables();
  }
  if (predicand->type() == Item::FIELD_ITEM) {
    Item_field *item_field = static_cast<Item_field *>(predicand);
    Field *field = item_field->field;

    if (!((ref_tables | item_field->table_ref->map()) & param_comp))
      ftree = get_func_mm_tree(thd, param, prev_tables, read_tables,
                               remove_jump_scans, predicand, op, value, inv);
    Item_equal *item_equal = item_field->item_equal;
    if (item_equal != nullptr) {
      for (Item_field &item : item_equal->get_fields()) {
        Field *f = item.field;
        if (!field->eq(f) &&
            !((ref_tables | item.table_ref->map()) & param_comp)) {
          tree = get_func_mm_tree(thd, param, prev_tables, read_tables,
                                  remove_jump_scans, &item, op, value, inv);
          ftree = !ftree ? tree : tree_and(param, ftree, tree);
        }
      }
    }
  } else if (predicand->type() == Item::ROW_ITEM) {
    ftree = get_func_mm_tree(thd, param, prev_tables, read_tables,
                             remove_jump_scans, predicand, op, value, inv);
    return ftree;
  }
  return ftree;
}

/**
  The Range Analysis Module, which finds range access alternatives
  applicable to single or multi-index (UNION) access. The function
  does not calculate or care about the cost of the different
  alternatives.

  get_mm_tree() employs a relaxed boolean algebra where the solution
  may be bigger than what the rules of boolean algebra accept. In
  other words, get_mm_tree() may return range access plans that will
  read more rows than the input conditions dictate. In it's simplest
  form, consider a condition on two fields indexed by two different
  indexes:

     "WHERE fld1 > 'x' AND fld2 > 'y'"

  In this case, there are two single-index range access alternatives.
  No matter which access path is chosen, rows that are not in the
  result set may be read.

  In the case above, get_mm_tree() will create range access
  alternatives for both indexes, so boolean algebra is still correct.
  In other cases, however, the conditions are too complex to be used
  without relaxing the rules. This typically happens when ORing a
  conjunction to a multi-index disjunctions (@see e.g.
  imerge_list_or_tree()). When this happens, the range optimizer may
  choose to ignore conjunctions (any condition connected with AND). The
  effect of this is that the result includes a "bigger" solution than
  necessary. This is OK since all conditions will be used as filters
  after row retrieval.

  @see SEL_TREE::keys and SEL_TREE::merges for details of how single
  and multi-index range access alternatives are stored.

  remove_jump_scans: Aggressively remove "scans" that do not have
  conditions on first keyparts. Such scans are usable when doing partition
  pruning but not regular range optimization.


  A return value of nullptr from get_mm_tree() means that this condition
  could not be represented by a range. Normally, this means that the best
  thing to do is to keep that condition entirely out of the range optimization,
  since ANDing it with other conditions (in tree_and()) would make the entire
  tree inexact and no predicates subsumable (see SEL_TREE::inexact). However,
  the old join optimizer does not care, and always just gives in the entire
  condition (with different parts ANDed together) in one go, since it never
  subsumes anything anyway.
 */
SEL_TREE *get_mm_tree(THD *thd, RANGE_OPT_PARAM *param, table_map prev_tables,
                      table_map read_tables, table_map current_table,
                      bool remove_jump_scans, Item *cond) {
  SEL_TREE *ftree = nullptr;
  bool inv = false;
  DBUG_TRACE;

  if (param->has_errors()) return nullptr;

  if (cond->type() == Item::COND_ITEM) {
    Item_func::Functype functype = down_cast<Item_cond *>(cond)->functype();

    SEL_TREE *tree = nullptr;
    bool first = true;
    for (Item &item : *down_cast<Item_cond *>(cond)->argument_list()) {
      SEL_TREE *new_tree = get_mm_tree(thd, param, prev_tables, read_tables,
                                       current_table, remove_jump_scans, &item);
      if (param->has_errors()) return nullptr;
      if (first) {
        tree = new_tree;
        first = false;
        continue;
      }
      if (functype == Item_func::COND_AND_FUNC) {
        tree = tree_and(param, tree, new_tree);
        dbug_print_tree("after_and", tree, param);
        if (tree && tree->type == SEL_TREE::IMPOSSIBLE) break;
      } else {  // OR.
        tree = tree_or(param, remove_jump_scans, tree, new_tree);
        dbug_print_tree("after_or", tree, param);
        if (tree == nullptr || tree->type == SEL_TREE::ALWAYS) break;
      }
    }
    dbug_print_tree("tree_returned", tree, param);
    return tree;
  }
  if (cond->const_item() && !cond->is_expensive()) {
    const SEL_TREE::Type type =
        cond->val_int() ? SEL_TREE::ALWAYS : SEL_TREE::IMPOSSIBLE;
    SEL_TREE *tree = new (param->temp_mem_root)
        SEL_TREE(type, param->temp_mem_root, param->keys);

    if (param->has_errors()) return nullptr;
    dbug_print_tree("tree_returned", tree, param);
    return tree;
  }

  // This used to be a guard against predicates like “WHERE x;”. But these are
  // now always rewritten to “x <> 0”, so it does not trigger there.
  // However, it is still relevant for subselects.
  if (cond->type() != Item::FUNC_ITEM) {
    return nullptr;
  }

  Item_func *cond_func = (Item_func *)cond;
  if (cond_func->functype() == Item_func::BETWEEN ||
      cond_func->functype() == Item_func::IN_FUNC)
    inv = ((Item_func_opt_neg *)cond_func)->negated;
  else {
    Item_func::optimize_type opt_type = cond_func->select_optimize(thd);
    if (opt_type == Item_func::OPTIMIZE_NONE) return nullptr;
  }

  /*
    Notice that all fields that are outer references are const during
    the execution and should not be considered for range analysis like
    fields coming from the local query block are.
  */
  switch (cond_func->functype()) {
    case Item_func::BETWEEN: {
      Item *const arg_left = cond_func->arguments()[0];

      if (!arg_left->is_outer_reference() &&
          arg_left->real_item()->type() == Item::FIELD_ITEM) {
        Item_field *field_item = down_cast<Item_field *>(arg_left->real_item());
        ftree = get_full_func_mm_tree(thd, param, prev_tables, read_tables,
                                      current_table, remove_jump_scans,
                                      field_item, cond_func, nullptr, inv);
      }

      /*
        Concerning the code below see the NOTES section in
        the comments for the function get_full_func_mm_tree()
      */
      SEL_TREE *tree = nullptr;
      for (uint i = 1; i < cond_func->arg_count; i++) {
        Item *const arg = cond_func->arguments()[i];

        if (!arg->is_outer_reference() &&
            arg->real_item()->type() == Item::FIELD_ITEM) {
          Item_field *field_item = down_cast<Item_field *>(arg->real_item());
          SEL_TREE *tmp = get_full_func_mm_tree(
              thd, param, prev_tables, read_tables, current_table,
              remove_jump_scans, field_item, cond_func,
              reinterpret_cast<Item *>(i), inv);
          if (inv) {
            tree = !tree ? tmp : tree_or(param, remove_jump_scans, tree, tmp);
            if (tree == nullptr) break;
          } else
            tree = tree_and(param, tree, tmp);
        } else if (inv) {
          tree = nullptr;
          break;
        }
      }

      ftree = tree_and(param, ftree, tree);
      break;
    }  // end case Item_func::BETWEEN

    case Item_func::JSON_CONTAINS:
    case Item_func::JSON_OVERLAPS:
    case Item_func::MEMBER_OF_FUNC:
    case Item_func::IN_FUNC: {
      Item *predicand = cond_func->key_item();
      if (!predicand) return nullptr;
      predicand = predicand->real_item();
      if (predicand->type() != Item::FIELD_ITEM &&
          predicand->type() != Item::ROW_ITEM)
        return nullptr;
      ftree = get_full_func_mm_tree(thd, param, prev_tables, read_tables,
                                    current_table, remove_jump_scans, predicand,
                                    cond_func, nullptr, inv);
      break;
    }  // end case Item_func::IN_FUNC

    case Item_func::MULT_EQUAL_FUNC: {
      Item_equal *item_equal = down_cast<Item_equal *>(cond);
      Item *value = item_equal->const_arg();
      if (value == nullptr) return nullptr;
      table_map ref_tables = value->used_tables();
      for (Item_field &field_item : item_equal->get_fields()) {
        Field *field = field_item.field;
        table_map param_comp = ~(prev_tables | read_tables | current_table);
        if (!((ref_tables | field_item.table_ref->map()) & param_comp)) {
          SEL_TREE *tree =
              get_mm_parts(thd, param, prev_tables, read_tables, item_equal,
                           field, Item_func::EQ_FUNC, value);
          ftree = !ftree ? tree : tree_and(param, ftree, tree);
        }
      }

      dbug_print_tree("tree_returned", ftree, param);
      return ftree;
    }  // end case Item_func::MULT_EQUAL_FUNC

    default: {
      Item *const arg_left = cond_func->arguments()[0];

      assert(!ftree);
      if (!arg_left->is_outer_reference() &&
          arg_left->real_item()->type() == Item::FIELD_ITEM) {
        Item_field *field_item = down_cast<Item_field *>(arg_left->real_item());
        Item *value =
            cond_func->arg_count > 1 ? cond_func->arguments()[1] : nullptr;
        ftree = get_full_func_mm_tree(thd, param, prev_tables, read_tables,
                                      current_table, remove_jump_scans,
                                      field_item, cond_func, value, inv);
      }
      /*
        Even if get_full_func_mm_tree() was executed above and did not
        return a range predicate it may still be possible to create one
        by reversing the order of the operands. Note that this only
        applies to predicates where both operands are fields. Example: A
        query of the form

           WHERE t1.a OP t2.b

        In this case, arguments()[0] == t1.a and arguments()[1] == t2.b.
        When creating range predicates for t2, get_full_func_mm_tree()
        above will return NULL because 'field' belongs to t1 and only
        predicates that applies to t2 are of interest. In this case a
        call to get_full_func_mm_tree() with reversed operands (see
        below) may succeed.
      */
      Item *arg_right;
      if (!ftree && cond_func->have_rev_func() &&
          (arg_right = cond_func->arguments()[1]) &&
          !arg_right->is_outer_reference() &&
          arg_right->real_item()->type() == Item::FIELD_ITEM) {
        Item_field *field_item =
            down_cast<Item_field *>(arg_right->real_item());
        Item *value = arg_left;
        ftree = get_full_func_mm_tree(thd, param, prev_tables, read_tables,
                                      current_table, remove_jump_scans,
                                      field_item, cond_func, value, inv);
      }
    }  // end case default
  }    // end switch

  dbug_print_tree("tree_returned", ftree, param);
  return ftree;
}

/**
  Test whether a comparison operator is a spatial comparison
  operator, i.e. Item_func::SP_*.

  Used to check if range access using operator 'op_type' is applicable
  for a non-spatial index.

  @param   op_type  The comparison operator.
  @return  true if 'op_type' is a spatial comparison operator, false otherwise.

*/
static bool is_spatial_operator(Item_func::Functype op_type) {
  switch (op_type) {
    case Item_func::SP_EQUALS_FUNC:
    case Item_func::SP_DISJOINT_FUNC:
    case Item_func::SP_INTERSECTS_FUNC:
    case Item_func::SP_TOUCHES_FUNC:
    case Item_func::SP_CROSSES_FUNC:
    case Item_func::SP_WITHIN_FUNC:
    case Item_func::SP_CONTAINS_FUNC:
    case Item_func::SP_COVEREDBY_FUNC:
    case Item_func::SP_COVERS_FUNC:
    case Item_func::SP_OVERLAPS_FUNC:
    case Item_func::SP_STARTPOINT:
    case Item_func::SP_ENDPOINT:
    case Item_func::SP_EXTERIORRING:
    case Item_func::SP_POINTN:
    case Item_func::SP_GEOMETRYN:
    case Item_func::SP_INTERIORRINGN:
      return true;
    default:
      return false;
  }
}

static SEL_TREE *get_mm_parts(THD *thd, RANGE_OPT_PARAM *param,
                              table_map prev_tables, table_map read_tables,
                              Item_func *cond_func, Field *field,
                              Item_func::Functype type, Item *value) {
  DBUG_TRACE;

  if (param->has_errors()) return nullptr;

  if (field->table != param->table) return nullptr;

  KEY_PART *key_part = param->key_parts;
  KEY_PART *end = param->key_parts_end;
  SEL_TREE *tree = nullptr;
  if (value && value->used_tables() & ~(prev_tables | read_tables))
    return nullptr;
  for (; key_part != end; key_part++) {
    if (field->eq(key_part->field)) {
      /*
        Cannot do range access for spatial operators when a
        non-spatial index is used.
      */
      if (key_part->image_type != Field::itMBR &&
          is_spatial_operator(cond_func->functype()))
        continue;

      SEL_ROOT *sel_root = nullptr;
      if (!tree && !(tree = new (param->temp_mem_root)
                         SEL_TREE(param->temp_mem_root, param->keys)))
        return nullptr;  // OOM
      if (!value || !(value->used_tables() & ~read_tables)) {
        sel_root = get_mm_leaf(thd, param, cond_func, key_part->field, key_part,
                               type, value, &tree->inexact);
        if (!sel_root) continue;
        if (sel_root->type == SEL_ROOT::Type::IMPOSSIBLE) {
          tree->type = SEL_TREE::IMPOSSIBLE;
          return tree;
        }
      } else {
        /*
          The index may not be used by dynamic range access unless
          'field' and 'value' are comparable.
        */
        if (!comparable_in_index(cond_func, key_part->field,
                                 key_part->image_type, type, value)) {
          warn_index_not_applicable(thd, param, key_part->key, field);
          return nullptr;
        }

        if (!(sel_root = new (param->temp_mem_root)
                  SEL_ROOT(param->temp_mem_root, SEL_ROOT::Type::MAYBE_KEY)))
          return nullptr;  // OOM
      }
      sel_root->root->part = (uchar)key_part->part;
      tree->set_key(key_part->key,
                    sel_add(tree->release_key(key_part->key), sel_root));
      tree->keys_map.set_bit(key_part->key);
    }
  }

  if (tree && tree->merges.is_empty() && tree->keys_map.is_clear_all())
    tree = nullptr;
  return tree;
}

/**
  Saves 'value' in 'field' and handles potential type conversion
  problems.

  @param [out] tree                 The SEL_ROOT leaf under construction. If
                                    an always false predicate is found it is
                                    modified to point to a SEL_ROOT with
                                    type == SEL_ROOT::Type::IMPOSSIBLE.
  @param value                      The Item that contains a value that shall
                                    be stored in 'field'.
  @param comp_op                    Comparison operator: >, >=, <=> etc.
  @param field                      The field that 'value' is stored into.
  @param [out] impossible_cond_cause Set to a descriptive string if an
                                    impossible condition is found.
  @param memroot                    Memroot for creation of new SEL_ARG.
  @param query_block                Query block the field is part of
  @param inexact                    Set to true on lossy conversion

  @retval false  if saving went fine and it makes sense to continue
                 optimizing for this predicate.
  @retval true   if always true/false predicate was found, in which
                 case 'tree' has been modified to reflect this: NULL
                 pointer if always true, SEL_ARG with type IMPOSSIBLE
                 if always false.
*/
static bool save_value_and_handle_conversion(
    SEL_ROOT **tree, Item *value, const Item_func::Functype comp_op,
    Field *field, const char **impossible_cond_cause, MEM_ROOT *memroot,
    Query_block *query_block, bool *inexact) {
  // A SEL_ARG should not have been created for this predicate yet.
  assert(*tree == nullptr);

  THD *const thd = current_thd;

  if (!(value->const_item() || thd->lex->is_query_tables_locked())) {
    /*
      We cannot evaluate the value yet (i.e. required tables are not yet
      locked.)
      This is the case of prune_partitions() called during
      Query_block::prepare().
    */
    return true;
  }

  /*
    Don't evaluate subqueries during optimization if they are disabled. This
    function can be called during execution when doing dynamic range access, and
    we only want to disable subquery evaluation during optimization, so check if
    we're in the optimization phase by calling Query_expression::is_optimized().
  */
  if (!query_block->master_query_expression()->is_optimized() &&
      !evaluate_during_optimization(value, query_block))
    return true;

  // For comparison purposes allow invalid dates like 2000-01-32
  const sql_mode_t orig_sql_mode = thd->variables.sql_mode;
  thd->variables.sql_mode |= MODE_INVALID_DATES;

  /*
    We want to change "field > value" to "field OP V"
    where:
    * V is what is in "field" after we stored "value" in it via
    save_in_field_no_warning() (such store operation may have done
    rounding...)
    * OP is > or >=, depending on what's correct.
    For example, if c is an INT column,
    "c > 2.9" is changed to "c OP 3"
    where OP is ">=" (">" would not be correct, as 3 > 2.9, a comparison
    done with stored_field_cmp_to_item()). And
    "c > 3.1" is changed to "c OP 3" where OP is ">" (3 < 3.1...).
  */

  // Note that value may be a stored function call, executed here.
  const type_conversion_status err =
      value->save_in_field_no_warnings(field, true);
  thd->variables.sql_mode = orig_sql_mode;

  switch (err) {
    case TYPE_NOTE_TRUNCATED:
    case TYPE_WARN_TRUNCATED:
      *inexact = true;
      [[fallthrough]];
    case TYPE_OK:
      return false;
    case TYPE_WARN_INVALID_STRING:
      /*
        An invalid string does not produce any rows when used with
        equality operator.
      */
      if (comp_op == Item_func::EQUAL_FUNC || comp_op == Item_func::EQ_FUNC) {
        *impossible_cond_cause = "invalid_characters_in_string";
        goto impossible_cond;
      }
      /*
        For other operations on invalid strings, we assume that the range
        predicate is always true and let evaluate_join_record() decide
        the outcome.
      */
      *inexact = true;
      return true;
    case TYPE_ERR_BAD_VALUE:
      /*
        In the case of incompatible values, MySQL's SQL dialect has some
        strange interpretations. For example,

            "int_col > 'foo'" is interpreted as "int_col > 0"

        instead of always false. Because of this, we assume that the
        range predicate is always true instead of always false and let
        evaluate_join_record() decide the outcome.
      */
      *inexact = true;
      return true;
    case TYPE_ERR_NULL_CONSTRAINT_VIOLATION:
      // Checking NULL value on a field that cannot contain NULL.
      *impossible_cond_cause = "null_field_in_non_null_column";
      goto impossible_cond;
    case TYPE_WARN_OUT_OF_RANGE:
      /*
        value to store was either higher than field::max_value or lower
        than field::min_value. The field's max/min value has been stored
        instead.
      */
      if (comp_op == Item_func::EQUAL_FUNC || comp_op == Item_func::EQ_FUNC) {
        /*
          Independent of data type, "out_of_range_value =/<=> field" is
          always false.
        */
        *impossible_cond_cause = "value_out_of_range";
        goto impossible_cond;
      }

      // If the field is numeric, we can interpret the out of range value.
      if ((field->type() != FIELD_TYPE_BIT) &&
          (field->result_type() == REAL_RESULT ||
           field->result_type() == INT_RESULT ||
           field->result_type() == DECIMAL_RESULT)) {
        /*
          value to store was higher than field::max_value if
             a) field has a value greater than 0, or
             b) if field is unsigned and has a negative value (which, when
                cast to unsigned, means some value higher than LLONG_MAX).
        */
        if ((field->val_int() > 0) ||                        // a)
            (field->is_unsigned() && field->val_int() < 0))  // b)
        {
          if (comp_op == Item_func::LT_FUNC || comp_op == Item_func::LE_FUNC) {
            /*
              '<' or '<=' compared to a value higher than the field
              can store is always true.
            */
            return true;
          }
          if (comp_op == Item_func::GT_FUNC || comp_op == Item_func::GE_FUNC) {
            /*
              '>' or '>=' compared to a value higher than the field can
              store is always false.
            */
            *impossible_cond_cause = "value_out_of_range";
            goto impossible_cond;
          }
        } else  // value is lower than field::min_value
        {
          if (comp_op == Item_func::GT_FUNC || comp_op == Item_func::GE_FUNC) {
            /*
              '>' or '>=' compared to a value lower than the field
              can store is always true.
            */
            return true;
          }
          if (comp_op == Item_func::LT_FUNC || comp_op == Item_func::LE_FUNC) {
            /*
              '<' or '=' compared to a value lower than the field can
              store is always false.
            */
            *impossible_cond_cause = "value_out_of_range";
            goto impossible_cond;
          }
        }
      }
      /*
        Value is out of range on a datatype where it can't be decided if
        it was underflow or overflow. It is therefore not possible to
        determine whether or not the condition is impossible or always
        true and we have to assume always true.
      */
      return true;
    case TYPE_NOTE_TIME_TRUNCATED:
      if (field->type() == FIELD_TYPE_DATE &&
          (comp_op == Item_func::GT_FUNC || comp_op == Item_func::GE_FUNC ||
           comp_op == Item_func::LT_FUNC || comp_op == Item_func::LE_FUNC)) {
        /*
          We were saving DATETIME into a DATE column, the conversion went ok
          but a non-zero time part was cut off.

          In MySQL's SQL dialect, DATE and DATETIME are compared as datetime
          values. Index over a DATE column uses DATE comparison. Changing
          from one comparison to the other is possible:

          datetime(date_col)< '2007-12-10 12:34:55' -> date_col<='2007-12-10'
          datetime(date_col)<='2007-12-10 12:34:55' -> date_col<='2007-12-10'

          datetime(date_col)> '2007-12-10 12:34:55' -> date_col>='2007-12-10'
          datetime(date_col)>='2007-12-10 12:34:55' -> date_col>='2007-12-10'

          but we'll need to convert '>' to '>=' and '<' to '<='. This will
          be done together with other types at the end of get_mm_leaf()
          (grep for stored_field_cmp_to_item)
        */
        return false;
      }
      if (comp_op == Item_func::EQ_FUNC || comp_op == Item_func::EQUAL_FUNC) {
        // Equality comparison is always false when time info has been
        // truncated.
        goto impossible_cond;
      }
      return true;
    case TYPE_ERR_OOM:
      return true;
      /*
        No default here to avoid adding new conversion status codes that are
        unhandled in this function.
      */
  }

  assert(false);  // Should never get here.

impossible_cond:
  *tree = new (memroot) SEL_ROOT(memroot, SEL_ROOT::Type::IMPOSSIBLE);
  return true;
}

static SEL_ROOT *get_mm_leaf(THD *thd, RANGE_OPT_PARAM *param, Item *cond_func,
                             Field *field, KEY_PART *key_part,
                             Item_func::Functype type, Item *value,
                             bool *inexact) {
  const size_t null_bytes = field->is_nullable() ? 1 : 0;
  bool optimize_range;
  SEL_ROOT *tree = nullptr;
  MEM_ROOT *const alloc = param->temp_mem_root;
  uchar *str;
  const char *impossible_cond_cause = nullptr;
  DBUG_TRACE;

  if (param->has_errors()) goto end;

  if (!value)  // IS NULL or IS NOT NULL
  {
    if (field->table->pos_in_table_list->outer_join)
      /*
        Range scan cannot be used to scan the inner table of an outer
        join if the predicate is IS NULL.
      */
      goto end;
    if (!field->is_nullable())  // NOT NULL column
    {
      if (type == Item_func::ISNULL_FUNC)
        tree = new (alloc) SEL_ROOT(alloc, SEL_ROOT::Type::IMPOSSIBLE);
      goto end;
    }
    uchar *null_string =
        static_cast<uchar *>(alloc->Alloc(key_part->store_length + 1));
    if (!null_string) goto end;  // out of memory

    TRASH(null_string, key_part->store_length + 1);
    memcpy(null_string, is_null_string, sizeof(is_null_string));

    SEL_ARG *root;
    if (!(root = new (alloc) SEL_ARG(field, null_string, null_string,
                                     !(key_part->flag & HA_REVERSE_SORT))))
      goto end;                                          // out of memory
    if (!(tree = new (alloc) SEL_ROOT(root))) goto end;  // out of memory
    if (type == Item_func::ISNOTNULL_FUNC) {
      root->min_flag = NEAR_MIN; /* IS NOT NULL ->  X > NULL */
      root->max_flag = NO_MAX_RANGE;
    }
    goto end;
  }

  /*
    The range access method cannot be used unless 'field' and 'value'
    are comparable in the index. Examples of non-comparable
    field/values: different collation, DATETIME vs TIME etc.
  */
  if (!comparable_in_index(cond_func, field, key_part->image_type, type,
                           value)) {
    warn_index_not_applicable(thd, param, key_part->key, field);
    goto end;
  }

  if (key_part->image_type == Field::itMBR) {
    // @todo: use is_spatial_operator() instead?
    switch (type) {
      case Item_func::SP_EQUALS_FUNC:
      case Item_func::SP_DISJOINT_FUNC:
      case Item_func::SP_INTERSECTS_FUNC:
      case Item_func::SP_TOUCHES_FUNC:
      case Item_func::SP_CROSSES_FUNC:
      case Item_func::SP_WITHIN_FUNC:
      case Item_func::SP_CONTAINS_FUNC:
      case Item_func::SP_OVERLAPS_FUNC:
        break;
      default:
        /*
          We cannot involve spatial indexes for queries that
          don't use MBREQUALS(), MBRDISJOINT(), etc. functions.
        */
        goto end;
    }
  }

  if (param->using_real_indexes)
    optimize_range =
        field->optimize_range(param->real_keynr[key_part->key], key_part->part);
  else
    optimize_range = true;

  if (type == Item_func::LIKE_FUNC) {
    bool like_error;
    char buff1[MAX_FIELD_WIDTH];
    uchar *min_str, *max_str;
    String tmp(buff1, sizeof(buff1), value->collation.collation), *res;
    size_t length, offset, min_length, max_length;
    size_t field_length = field->pack_length() + null_bytes;

    if (!optimize_range) goto end;
    if (!(res = value->val_str(&tmp))) {
      tree = new (alloc) SEL_ROOT(alloc, SEL_ROOT::Type::IMPOSSIBLE);
      goto end;
    }

    /*
      TODO:
      Check if this was a function. This should have be optimized away
      in the sql_select.cc
    */
    if (res != &tmp) {
      tmp.copy(*res);  // Get own copy
      res = &tmp;
    }
    if (field->cmp_type() != STRING_RESULT)
      goto end;  // Can only optimize strings

    offset = null_bytes;
    length = key_part->store_length;

    if (length != key_part->length + null_bytes) {
      /* key packed with length prefix */
      offset += HA_KEY_BLOB_LENGTH;
      field_length = length - HA_KEY_BLOB_LENGTH;
    } else {
      if (unlikely(length < field_length)) {
        /*
          This can only happen in a table created with UNIREG where one key
          overlaps many fields
        */
        length = field_length;
      } else
        field_length = length;
    }
    length += offset;
    if (!(min_str = (uchar *)alloc->Alloc(length * 2))) goto end;

    max_str = min_str + length;
    if (field->is_nullable()) max_str[0] = min_str[0] = 0;

    Item_func_like *like_func = down_cast<Item_func_like *>(cond_func);

    // We can only optimize with LIKE if the escape string is known.
    if (!like_func->escape_is_evaluated()) goto end;

    field_length -= null_bytes;
    like_error = my_like_range(
        field->charset(), res->ptr(), res->length(), like_func->escape(),
        wild_one, wild_many, field_length, (char *)min_str + offset,
        (char *)max_str + offset, &min_length, &max_length);
    if (like_error)  // Can't optimize with LIKE
      goto end;

    // LIKE is tricky to get 100% exact, especially with Unicode collations
    // (which can have contractions etc.), and will frequently be a bit too
    // broad. To be safe, we currently always set that LIKE range scans are
    // inexact and must be rechecked by means of a filter afterwards.
    *inexact = true;

    if (offset != null_bytes)  // BLOB or VARCHAR
    {
      int2store(min_str + null_bytes, static_cast<uint16>(min_length));
      int2store(max_str + null_bytes, static_cast<uint16>(max_length));
    }
    SEL_ARG *root = new (alloc)
        SEL_ARG(field, min_str, max_str, !(key_part->flag & HA_REVERSE_SORT));
    if (!root || !(tree = new (alloc) SEL_ROOT(root)))
      goto end;  // out of memory
    goto end;
  }

  if (!optimize_range && type != Item_func::EQ_FUNC &&
      type != Item_func::EQUAL_FUNC)
    goto end;  // Can't optimize this

  /*
    Geometry operations may mix geometry types, e.g., we may be
    checking ST_Contains(<polygon field>, <point>). In such cases,
    field->geom_type will be a different type than the value we're
    trying to store in it, and the conversion will fail. Therefore,
    set the most general geometry type while saving, and revert to the
    original geometry type afterwards.
  */
  {
    const Field::geometry_type save_geom_type =
        (field->type() == MYSQL_TYPE_GEOMETRY) ? field->get_geometry_type()
                                               : Field::GEOM_GEOMETRY;
    if (field->type() == MYSQL_TYPE_GEOMETRY) {
      down_cast<Field_geom *>(field)->geom_type = Field::GEOM_GEOMETRY;

      // R-tree queries are based on bounds, and must be rechecked.
      *inexact = true;
    }

    bool always_true_or_false = save_value_and_handle_conversion(
        &tree, value, type, field, &impossible_cond_cause, alloc,
        param->query_block, inexact);

    if (field->type() == MYSQL_TYPE_GEOMETRY &&
        save_geom_type != Field::GEOM_GEOMETRY) {
      down_cast<Field_geom *>(field)->geom_type = save_geom_type;
    }

    if (always_true_or_false) goto end;
  }

  /*
    Any sargable predicate except "<=>" involving NULL as a constant is always
    false
  */
  if (type != Item_func::EQUAL_FUNC && field->is_real_null()) {
    impossible_cond_cause = "comparison_with_null_always_false";
    tree = new (alloc) SEL_ROOT(alloc, SEL_ROOT::Type::IMPOSSIBLE);
    goto end;
  }

  str = (uchar *)alloc->Alloc(key_part->store_length + 1);
  if (!str) goto end;
  if (field->is_nullable())
    *str = (uchar)field->is_real_null();  // Set to 1 if null
  field->get_key_image(str + null_bytes, key_part->length,
                       key_part->image_type);
  SEL_ARG *root;
  root =
      new (alloc) SEL_ARG(field, str, str, !(key_part->flag & HA_REVERSE_SORT));
  if (!root || !(tree = new (alloc) SEL_ROOT(root))) goto end;  // out of memory
  /*
    Check if we are comparing an UNSIGNED integer with a negative constant.
    In this case we know that:
    (a) (unsigned_int [< | <=] negative_constant) == false
    (b) (unsigned_int [> | >=] negative_constant) == true
    In case (a) the condition is false for all values, and in case (b) it
    is true for all values, so we can avoid unnecessary retrieval and condition
    testing, and we also get correct comparison of unsigned integers with
    negative integers (which otherwise fails because at query execution time
    negative integers are cast to unsigned if compared with unsigned).
  */
  if (field->result_type() == INT_RESULT &&
      value->result_type() == INT_RESULT &&
      ((field->type() == FIELD_TYPE_BIT || field->is_unsigned()) &&
       !(value)->unsigned_flag)) {
    longlong item_val = value->val_int();
    if (item_val < 0) {
      if (type == Item_func::LT_FUNC || type == Item_func::LE_FUNC) {
        impossible_cond_cause = "unsigned_int_cannot_be_negative";
        tree->type = SEL_ROOT::Type::IMPOSSIBLE;
        goto end;
      }
      if (type == Item_func::GT_FUNC || type == Item_func::GE_FUNC) {
        tree = nullptr;
        goto end;
      }
    }
  }

  switch (type) {
    case Item_func::LT_FUNC:
    case Item_func::LE_FUNC:
      /* Don't use open ranges for partial key_segments */
      if (!(key_part->flag & HA_PART_KEY_SEG)) {
        /*
          Set NEAR_MAX to read values lesser than the stored value.
        */
        const int cmp_value = stored_field_cmp_to_item(thd, field, value);
        if ((type == Item_func::LT_FUNC && cmp_value >= 0) ||
            (type == Item_func::LE_FUNC && cmp_value > 0))
          tree->root->max_flag = NEAR_MAX;
      }
      if (!field->is_nullable())
        tree->root->min_flag = NO_MIN_RANGE; /* From start */
      else {                                 // > NULL
        if (!(tree->root->min_value = static_cast<uchar *>(
                  alloc->Alloc(key_part->store_length + 1))))
          goto end;
        TRASH(tree->root->min_value, key_part->store_length + 1);
        memcpy(tree->root->min_value, is_null_string, sizeof(is_null_string));
        tree->root->min_flag = NEAR_MIN;
      }
      break;
    case Item_func::GT_FUNC:
    case Item_func::GE_FUNC:
      /* Don't use open ranges for partial key_segments */
      if (!(key_part->flag & HA_PART_KEY_SEG)) {
        /*
          Set NEAR_MIN to read values greater than the stored value.
        */
        const int cmp_value = stored_field_cmp_to_item(thd, field, value);
        if ((type == Item_func::GT_FUNC && cmp_value <= 0) ||
            (type == Item_func::GE_FUNC && cmp_value < 0))
          tree->root->min_flag = NEAR_MIN;
      }
      tree->root->max_flag = NO_MAX_RANGE;
      break;
    case Item_func::SP_EQUALS_FUNC:
      tree->root->set_gis_index_read_function(HA_READ_MBR_EQUAL);
      break;
    case Item_func::SP_DISJOINT_FUNC:
      tree->root->set_gis_index_read_function(HA_READ_MBR_DISJOINT);
      break;
    case Item_func::SP_INTERSECTS_FUNC:
      tree->root->set_gis_index_read_function(HA_READ_MBR_INTERSECT);
      break;
    case Item_func::SP_TOUCHES_FUNC:
      tree->root->set_gis_index_read_function(HA_READ_MBR_INTERSECT);
      break;

    case Item_func::SP_CROSSES_FUNC:
      tree->root->set_gis_index_read_function(HA_READ_MBR_INTERSECT);
      break;
    case Item_func::SP_WITHIN_FUNC:
      /*
        Adjust the rkey_func_flag as it's assumed and observed that both
        MyISAM and Innodb implement this function in reverse order.
      */
      tree->root->set_gis_index_read_function(HA_READ_MBR_CONTAIN);
      break;

    case Item_func::SP_CONTAINS_FUNC:
      /*
        Adjust the rkey_func_flag as it's assumed and observed that both
        MyISAM and Innodb implement this function in reverse order.
      */
      tree->root->set_gis_index_read_function(HA_READ_MBR_WITHIN);
      break;
    case Item_func::SP_OVERLAPS_FUNC:
      tree->root->set_gis_index_read_function(HA_READ_MBR_INTERSECT);
      break;

    default:
      break;
  }

end:
  if (impossible_cond_cause != nullptr) {
    Opt_trace_object wrapper(&thd->opt_trace);
    Opt_trace_object(&thd->opt_trace, "impossible_condition",
                     Opt_trace_context::RANGE_OPTIMIZER)
        .add_alnum("cause", impossible_cond_cause);
  }
  return tree;
}

/**
  Add a new key test to a key when scanning through all keys
  This will never be called for same key parts.

  @param key1 Old root of key
  @param key2 Element to insert (must be a single element)
  @return New root of key
*/
static SEL_ROOT *sel_add(SEL_ROOT *key1, SEL_ROOT *key2) {
  if (!key1) return key2;
  if (!key2) return key1;

  // key2 is assumed to be a single element.
  assert(!key2->root->next_key_part);

  if (key2->root->part < key1->root->part) {
    // key2 fits in the start of the list.
    key2->root->set_next_key_part(key1);
    return key2;
  }

  // Find out where in the chain in key1 to put key2.
  SEL_ARG *node = key1->root;
  while (node->next_key_part &&
         node->next_key_part->root->part > key2->root->part)
    node = node->next_key_part->root;

  key2->root->set_next_key_part(node->release_next_key_part());
  node->set_next_key_part(key2);

  return key1;
}
