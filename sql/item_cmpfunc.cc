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

/**
  @file sql/item_cmpfunc.cc

  @brief
  This file defines all Items that compare values (e.g. >=, ==, LIKE, etc.)
*/

#include "sql/item_cmpfunc.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <utility>

#include "decimal.h"
#include "field_types.h"
#include "mf_wcomp.h"  // wild_one, wild_many
#include "my_alloc.h"
#include "my_bitmap.h"
#include "my_dbug.h"
#include "my_sqlcommand.h"
#include "my_sys.h"
#include "mysql/strings/dtoa.h"
#include "mysql/strings/m_ctype.h"
#include "mysql/udf_registration_types.h"
#include "mysql_com.h"
#include "mysql_time.h"
#include "mysqld_error.h"
#include "sql-common/json_dom.h"  // Json_scalar_holder
#include "sql/aggregate_check.h"  // Distinct_check
#include "sql/check_stack.h"
#include "sql/current_thd.h"  // current_thd
#include "sql/derror.h"       // ER_THD
#include "sql/error_handler.h"
#include "sql/field.h"
#include "sql/histograms/histogram.h"
#include "sql/item.h"
#include "sql/item_func.h"
#include "sql/item_json_func.h"  // json_value, get_json_atom_wrapper
#include "sql/item_subselect.h"  // Item_subselect
#include "sql/item_sum.h"        // Item_sum_hybrid
#include "sql/item_timefunc.h"   // Item_typecast_date
#include "sql/join_optimizer/bit_utils.h"
#include "sql/key.h"
#include "sql/mysqld.h"  // log_10
#include "sql/nested_join.h"
#include "sql/opt_trace.h"  // Opt_trace_object
#include "sql/opt_trace_context.h"
#include "sql/parse_tree_helpers.h"    // PT_item_list
#include "sql/parse_tree_node_base.h"  // Parse_context
#include "sql/query_options.h"
#include "sql/sql_array.h"
#include "sql/sql_base.h"
#include "sql/sql_bitmap.h"
#include "sql/sql_class.h"  // THD
#include "sql/sql_const.h"
#include "sql/sql_error.h"
#include "sql/sql_executor.h"
#include "sql/sql_lex.h"
#include "sql/sql_opt_exec_shared.h"
#include "sql/sql_optimizer.h"  // JOIN
#include "sql/sql_select.h"
#include "sql/sql_time.h"  // str_to_datetime
#include "sql/system_variables.h"
#include "sql/thd_raii.h"
#include "string_with_len.h"

using std::max;
using std::min;

static const enum_walk walk_options =
    enum_walk::PREFIX | enum_walk::POSTFIX | enum_walk::SUBQUERY;

static bool convert_constant_item(THD *, Item_field *, Item **, bool *);
static longlong get_year_value(THD *thd, Item ***item_arg, Item **cache_arg,
                               const Item *warn_item, bool *is_null);
static Item **cache_converted_constant(THD *thd, Item **value,
                                       Item **cache_item, Item_result type);

/**
  Compare row signature of two expressions

  @param item1   first expression
  @param item2   second expression

  @returns true if row types are compatible, false otherwise.

  The function checks that two expressions have compatible row signatures
  i.e. that the number of columns they return are the same and that if they
  are both row expressions then each component from the first expression has
  a row signature compatible with the signature of the corresponding component
  of the second expression.
*/

static bool row_types_are_compatible(Item *item1, Item *item2) {
  const uint n = item1->cols();
  if (item2->check_cols(n)) return false;
  for (uint i = 0; i < n; i++) {
    if (item2->element_index(i)->check_cols(item1->element_index(i)->cols()) ||
        (item1->element_index(i)->result_type() == ROW_RESULT &&
         !row_types_are_compatible(item1->element_index(i),
                                   item2->element_index(i))))
      return false;
  }
  return true;
}

/**
  Aggregates result types from the array of items.

  This function aggregates result types from the array of items. Found type
  supposed to be used later for comparison of values of these items.
  Aggregation itself is performed by the item_cmp_type() function.

  @param      items        array of items to aggregate the type from
  @param      nitems       number of items in the array

  @returns    the aggregated type

*/

static Item_result agg_cmp_type(Item **items, uint nitems) {
  Item_result type = items[0]->result_type();
  for (uint i = 1; i < nitems; i++) {
    type = item_cmp_type(type, items[i]->result_type());
  }
  return type;
}

static void write_histogram_to_trace(THD *thd, const Item_func *item,
                                     const double selectivity) {
  Opt_trace_object obj(&thd->opt_trace, "histogram_selectivity");
  obj.add("condition", item).add("histogram_selectivity", selectivity);
}

/**
  @brief Aggregates field types from the array of items.

  @param[in] items  array of items to aggregate the type from
  @param[in] nitems number of items in the array

  @details This function aggregates field types from the array of items.
    Found type is supposed to be used later as the result field type
    of a multi-argument function.
    Aggregation itself is performed by the Field::field_type_merge()
    function.

  @note The term "aggregation" is used here in the sense of inferring the
    result type of a function from its argument types.

  @return aggregated field type.
*/

enum_field_types agg_field_type(Item **items, uint nitems) {
  assert(nitems > 0 && items[0]->result_type() != ROW_RESULT);
  enum_field_types res = items[0]->data_type();
  for (uint i = 1; i < nitems; i++)
    res = Field::field_type_merge(res, items[i]->data_type());
  return real_type_to_type(res);
}

/**
  Collects different types for comparison of first item with each other items

  @param items             Array of items to collect types from
  @param nitems            Number of items in the array
  @param skip_nulls        Don't collect types of NULL items if true

  @note
    This function collects different result types for comparison of the first
    item in the list with each of the remaining items in the 'items' array.

  @retval 0 Error, row type incompatibility has been detected
  @retval <> 0 Bitmap of collected types - otherwise
*/

static uint collect_cmp_types(Item **items, uint nitems,
                              bool skip_nulls = false) {
  const Item_result left_result = items[0]->result_type();
  assert(nitems > 1);
  uint found_types = 0;
  for (uint i = 1; i < nitems; i++) {
    if (skip_nulls && items[i]->type() == Item::NULL_ITEM)
      continue;  // Skip NULL constant items
    if ((left_result == ROW_RESULT || items[i]->result_type() == ROW_RESULT) &&
        !row_types_are_compatible(items[0], items[i]))
      return 0;
    found_types |=
        1U << (uint)item_cmp_type(left_result, items[i]->result_type());
  }
  /*
   Even if all right-hand items are NULLs and we are skipping them all, we need
   at least one type bit in the found_type bitmask.
  */
  if (skip_nulls && !found_types) found_types = 1U << (uint)left_result;
  return found_types;
}

static void my_coll_agg_error(DTCollation &c1, DTCollation &c2,
                              const char *fname) {
  my_error(ER_CANT_AGGREGATE_2COLLATIONS, MYF(0), c1.collation->m_coll_name,
           c1.derivation_name(), c2.collation->m_coll_name,
           c2.derivation_name(), fname);
}

/// This is used to indicate that the selectivity of a predicate has
/// not been determined.
static constexpr double kUndefinedSelectivity{-1.0};

/**
  Try to find the selectivity of an Item_func (predicate) using a
  histogram.
  @param thd The current thread.
  @param field The field for which we will look for a histogram.
  @param op The comparison operator of item_func.
  @param item_func The predicate.
  @return The selectivity if a histogram was found and the arguments
    of item_func allowed use of a histogram. Otherwise, kUndefinedSelectivity.
*/
static double get_histogram_selectivity(THD *thd, const Field &field,
                                        histograms::enum_operator op,
                                        const Item_func &item_func) {
  const histograms::Histogram *histogram =
      field.table->find_histogram(field.field_index());
  if (histogram != nullptr) {
    double selectivity;
    if (!histogram->get_selectivity(item_func.arguments(),
                                    item_func.argument_count(), op,
                                    &selectivity)) {
      if (unlikely(thd->opt_trace.is_started()))
        write_histogram_to_trace(thd, &item_func, selectivity);
      return selectivity;
    }
  }

  return kUndefinedSelectivity;
}

/**
   Estimate the selectivity of a predicate of type field=expression,
   using an index containing 'field'. ('expression' is assumed to be
   independent of the table that 'field' belongs to, meaning that this
   function should not be called for e.g. "t1.f1=t1.f2+1").
   @param field The field for which we estimate the selectivity.
   @returns The selectivity estimate, or kUndefinedSelectivity if no
   suitable index was found.
*/
static double IndexSelectivityOfUnknownValue(const Field &field) {
  const ha_rows row_count{field.table->file->stats.records};
  int contributing_keys{0};
  double selectivity_product{-1.0};

  if (row_count == 0) {
    return kUndefinedSelectivity;
  }

  uint shortest_prefix{UINT_MAX};

  // Loop over the keys containing 'field'.
  for (uint key_no = field.part_of_key.get_first_set(); key_no != MY_BIT_NONE;
       key_no = field.part_of_key.get_next_set(key_no)) {
    const KEY &key{field.table->key_info[key_no]};

    // Loop over the fields of 'key'.
    for (uint part_no = 0; part_no < key.user_defined_key_parts; part_no++) {
      if (!key.has_records_per_key(part_no)) {
        break;
      }

      const Field &key_field{*key.key_part[part_no].field};

      // Find (the square of) a selectivity estimate for a field that is part of
      // an index, but not the first field of that index.
      const auto subsequent_field_selectivity_squared = [&]() {
        assert(part_no > 0);
        /*
          For a field that is the first part (zero-indexed) of a key we
          can obtain the number of distinct values directly from the
          records_per_key statistic, but if the field is the k'th > 0
          part we have to make an estimate. Let d_k denote the number of
          distinct values in the k-part prefix of the key. Given that we
          only have information about d_k and d_(k-1) the number of
          distinct values in the field can be anywhere between d_k and
          d_k / d_(k-1), so we use the geometric mean of these two
          values as our estimate.
        */

        // Case 1: key field 'part_no' and the preceding fields are
        // uncorrelated.
        const double uncorrelated_estimate{
            double{key.records_per_key(part_no)} /
            key.records_per_key(part_no - 1)};

        // Case 2: The preceding fields are functionally dependent on
        // key field 'part_no'.
        const double correlated_estimate{
            std::min(1.0, double{key.records_per_key(part_no)} / row_count)};

        // Use the geometric mean of case 1 and 2.
        return uncorrelated_estimate * correlated_estimate;
      };

      if (&field == &key_field) {
        if (part_no == 0) {
          // We need std::min() since records_per_key() and stats.records
          // may be updated at different points in time.
          return std::min(1.0, double{key.records_per_key(0)} / row_count);

        } else if (part_no < shortest_prefix) {
          shortest_prefix = part_no;
          selectivity_product = subsequent_field_selectivity_squared();
          contributing_keys = 1;
          break;

        } else if (part_no == shortest_prefix) {
          // If 'field' is the n'th part of several indexes, we calculate the
          // geometric mean of the estimate from each of them.
          selectivity_product *= subsequent_field_selectivity_squared();
          contributing_keys++;
          break;
        }
      }
    }
  }

  switch (contributing_keys) {
    case 0:
      return kUndefinedSelectivity;

    case 1:
      return std::sqrt(
          selectivity_product);  // Minor optimization for the most common case.

    default:
      return std::pow(selectivity_product, 0.5 / contributing_keys);
  }
}

/**
  This implementation of the factory method also implements flattening of
  row constructors. Examples of flattening are:

  - ROW(a, b) op ROW(x, y) => a op x P b op y.
  - ROW(a, ROW(b, c) op ROW(x, ROW(y, z))) => a op x P b op y P c op z.

  P is either AND or OR, depending on the comparison operation, and this
  detail is left for combine().

  The actual operator @c op is created by the concrete subclass in
  create_scalar_predicate().
*/
Item_bool_func *Linear_comp_creator::create(Item *a, Item *b) const {
  /*
    Test if the arguments are row constructors and thus can be flattened into
    a list of ANDs or ORs.
  */
  if (a->type() == Item::ROW_ITEM && b->type() == Item::ROW_ITEM) {
    if (a->cols() != b->cols()) {
      my_error(ER_OPERAND_COLUMNS, MYF(0), a->cols());
      return nullptr;
    }
    assert(a->cols() > 1);
    List<Item> list;
    for (uint i = 0; i < a->cols(); ++i)
      list.push_back(create(a->element_index(i), b->element_index(i)));
    return combine(list);
  }
  return create_scalar_predicate(a, b);
}

Item_bool_func *Eq_creator::create_scalar_predicate(Item *a, Item *b) const {
  assert(a->type() != Item::ROW_ITEM || b->type() != Item::ROW_ITEM);
  return new Item_func_eq(a, b);
}

Item_bool_func *Eq_creator::combine(List<Item> list) const {
  return new Item_cond_and(list);
}

Item_bool_func *Equal_creator::create_scalar_predicate(Item *a, Item *b) const {
  assert(a->type() != Item::ROW_ITEM || b->type() != Item::ROW_ITEM);
  return new Item_func_equal(a, b);
}

Item_bool_func *Equal_creator::combine(List<Item> list) const {
  return new Item_cond_and(list);
}

Item_bool_func *Ne_creator::create_scalar_predicate(Item *a, Item *b) const {
  assert(a->type() != Item::ROW_ITEM || b->type() != Item::ROW_ITEM);
  return new Item_func_ne(a, b);
}

Item_bool_func *Ne_creator::combine(List<Item> list) const {
  return new Item_cond_or(list);
}

Item_bool_func *Gt_creator::create(Item *a, Item *b) const {
  return new Item_func_gt(a, b);
}

Item_bool_func *Lt_creator::create(Item *a, Item *b) const {
  return new Item_func_lt(a, b);
}

Item_bool_func *Ge_creator::create(Item *a, Item *b) const {
  return new Item_func_ge(a, b);
}

Item_bool_func *Le_creator::create(Item *a, Item *b) const {
  return new Item_func_le(a, b);
}

float Item_func_not::get_filtering_effect(THD *thd, table_map filter_for_table,
                                          table_map read_tables,
                                          const MY_BITMAP *fields_to_ignore,
                                          double rows_in_table) {
  const float filter = args[0]->get_filtering_effect(
      thd, filter_for_table, read_tables, fields_to_ignore, rows_in_table);

  /*
    If the predicate that will be negated has COND_FILTER_ALLPASS
    filtering it means that some dependent tables have not been
    read, that the predicate is of a type that filtering effect is
    not calculated for or something similar. In any case, the
    filtering effect of the inverted predicate should also be
    COND_FILTER_ALLPASS.
  */
  if (filter == COND_FILTER_ALLPASS) return COND_FILTER_ALLPASS;

  return 1.0f - filter;
}

/*
  Test functions
  Most of these  returns 0LL if false and 1LL if true and
  NULL if some arg is NULL.
*/

longlong Item_func_not::val_int() {
  assert(fixed);
  const bool value = args[0]->val_bool();
  null_value = args[0]->null_value;
  /*
    If NULL, return 0 because some higher layers like
    evaluate_join_record() just test for !=0 to implement IS TRUE.
    If not NULL, return inverted value.
  */
  return ((!null_value && value == 0) ? 1 : 0);
}

/*
  We put any NOT expression into parenthesis to avoid
  possible problems with internal view representations where
  any '!' is converted to NOT. It may cause a problem if
  '!' is used in an expression together with other operators
  whose precedence is lower than the precedence of '!' yet
  higher than the precedence of NOT.
*/

void Item_func_not::print(const THD *thd, String *str,
                          enum_query_type query_type) const {
  str->append('(');
  Item_func::print(thd, str, query_type);
  str->append(')');
}

/**
  special NOT for ALL subquery.
*/

longlong Item_func_not_all::val_int() {
  assert(fixed);
  const bool value = args[0]->val_bool();

  /*
    return TRUE if there was no record in underlying select in max/min
    optimization (ALL subquery)
  */
  if (empty_underlying_subquery()) return 1;

  null_value = args[0]->null_value;
  return ((!null_value && value == 0) ? 1 : 0);
}

bool Item_func_not_all::empty_underlying_subquery() {
  assert(subselect != nullptr ||
         !(test_sum_item != nullptr || test_sub_item != nullptr));
  /*
   When outer argument is NULL the subquery has not yet been evaluated, we
   need to evaluate it to get to know whether it returns any rows to return
   the correct result. 'ANY' subqueries are an exception because the
   result would be false or null which for a top level item always mean false.
   The subselect->unit->item->... chain should be used instead of
   subselect->... to workaround subquery transformation which could make
   subselect->engine unusable.
  */
  if (subselect != nullptr &&
      subselect->subquery_type() != Item_subselect::ANY_SUBQUERY &&
      subselect->query_expr()->item != nullptr &&
      !subselect->query_expr()->item->is_evaluated())
    subselect->query_expr()->item->exec(current_thd);
  return (test_sum_item != nullptr && !test_sum_item->has_values()) ||
         (test_sub_item != nullptr && !test_sub_item->has_values());
}

void Item_func_not_all::print(const THD *thd, String *str,
                              enum_query_type query_type) const {
  if (show)
    Item_func::print(thd, str, query_type);
  else
    args[0]->print(thd, str, query_type);
}

/**
  Special NOP (No OPeration) for ALL subquery. It is like
  Item_func_not_all.

  @return
    (return TRUE if underlying subquery do not return rows) but if subquery
    returns some rows it return same value as argument (TRUE/FALSE).
*/

longlong Item_func_nop_all::val_int() {
  assert(fixed);
  const longlong value = args[0]->val_int();

  /*
    return FALSE if there was records in underlying select in max/min
    optimization (SAME/ANY subquery)
  */
  if (empty_underlying_subquery()) return 0;

  null_value = args[0]->null_value;
  return (null_value || value == 0) ? 0 : 1;
}

/**
  Return an an unsigned Item_int containing the value of the year as stored in
  field. The item is typed as a YEAR.
  @param field   the field containign the year value

  @return the year wrapped in an Item in as described above, or nullptr on
          error.
*/
static Item *make_year_constant(Field *field) {
  Item_int *year = new Item_int(field->val_int());
  if (year == nullptr) return nullptr;
  year->unsigned_flag = field->is_flag_set(UNSIGNED_FLAG);
  year->set_data_type(MYSQL_TYPE_YEAR);
  return year;
}

/**
  Convert a constant item to an int and replace the original item.

    The function converts a constant expression or string to an integer.
    On successful conversion the original item is substituted for the
    result of the item evaluation.
    This is done when comparing DATE/TIME of different formats and
    also when comparing bigint to strings (in which case strings
    are converted to bigints).

  @param  thd             thread handle
  @param  field_item      item will be converted using the type of this field
  @param[in,out] item     reference to the item to convert
  @param[out] converted   True if a replacement was done.

  @note
    This function may be called both at prepare and optimize stages.
    When called at optimize stage, ensure that we record transient changes.

  @returns false if success, true if error
*/

static bool convert_constant_item(THD *thd, Item_field *field_item, Item **item,
                                  bool *converted) {
  Field *field = field_item->field;

  *converted = false;

  if ((*item)->may_evaluate_const(thd) &&
      /*
        In case of GC it's possible that this func will be called on an
        already converted constant. Don't convert it again.
      */
      !((*item)->data_type() == field_item->data_type() &&
        (*item)->basic_const_item())) {
    TABLE *table = field->table;
    const sql_mode_t orig_sql_mode = thd->variables.sql_mode;
    const enum_check_fields orig_check_for_truncated_fields =
        thd->check_for_truncated_fields;
    my_bitmap_map *old_maps[2];
    ulonglong orig_field_val = 0; /* original field value if valid */

    old_maps[0] = nullptr;
    old_maps[1] = nullptr;

    if (table)
      dbug_tmp_use_all_columns(table, old_maps, table->read_set,
                               table->write_set);
    /* For comparison purposes allow invalid dates like 2000-01-32 */
    thd->variables.sql_mode =
        (orig_sql_mode & ~MODE_NO_ZERO_DATE) | MODE_INVALID_DATES;
    thd->check_for_truncated_fields = CHECK_FIELD_IGNORE;

    /*
      Store the value of the field/constant if it references an outer field
      because the call to save_in_field below overrides that value.
      Don't save field value if no data has been read yet.
      Outer constant values are always saved.
    */
    bool save_field_value =
        field_item->depended_from &&
        (field_item->const_item() || field->table->has_row());
    if (save_field_value) orig_field_val = field->val_int();
    int rc;
    if (!(*item)->is_null() &&
        (((rc = (*item)->save_in_field(field, true)) == TYPE_OK) ||
         rc == TYPE_NOTE_TIME_TRUNCATED))  // TS-TODO
    {
      int field_cmp = 0;
      /*
        If item is a decimal value, we must reject it if it was truncated.
        TODO: consider doing the same for MYSQL_TYPE_YEAR,.
        However: we have tests which assume that things '1999' and
        '1991-01-01 01:01:01' can be converted to year.
        Testing for MYSQL_TYPE_YEAR here, would treat such literals
        as 'incorrect DOUBLE value'.
        See Bug#13580652 YEAR COLUMN CAN BE EQUAL TO 1999.1
      */
      if (field->type() == MYSQL_TYPE_LONGLONG) {
        field_cmp = stored_field_cmp_to_item(thd, field, *item);
        DBUG_PRINT("info", ("convert_constant_item %d", field_cmp));
      }

      // @todo it is not correct, in time_col = datetime_const_function,
      // to convert the latter to Item_time_with_ref below. Time_col should
      // rather be cast to datetime. WL#6570 check if the "fix temporals"
      // patch fixes this.
      if (0 == field_cmp) {
        Item *tmp =
            field->type() == MYSQL_TYPE_TIME
                ?
#define OLD_CMP
#ifdef OLD_CMP
                new Item_time_with_ref(field->decimals(),
                                       field->val_time_temporal(), *item)
                :
#else
                new Item_time_with_ref(
                    max((*item)->time_precision(), field->decimals()),
                    (*item)->val_time_temporal(), *item)
                :
#endif
                is_temporal_type_with_date(field->type())
                    ?
#ifdef OLD_CMP
                    new Item_datetime_with_ref(field->type(), field->decimals(),
                                               field->val_date_temporal(),
                                               *item)
                    :
#else
                    new Item_datetime_with_ref(
                        field->type(),
                        max((*item)->datetime_precision(), field->decimals()),
                        (*item)->val_date_temporal(), *item)
                    :
#endif
                    field->type() == MYSQL_TYPE_YEAR
                        ? make_year_constant(field)
                        : new Item_int_with_ref(
                              field->type(), field->val_int(), *item,
                              field->is_flag_set(UNSIGNED_FLAG));
        if (tmp == nullptr) return true;

        if (thd->lex->is_exec_started())
          thd->change_item_tree(item, tmp);
        else
          *item = tmp;
        *converted = true;  // Item was replaced
      }
    }
    /* Restore the original field value. */
    if (save_field_value) {
      *converted = field->store(orig_field_val, true);
      /* orig_field_val must be a valid value that can be restored back. */
      assert(!*converted);
    }
    thd->variables.sql_mode = orig_sql_mode;
    thd->check_for_truncated_fields = orig_check_for_truncated_fields;
    if (table)
      dbug_tmp_restore_column_maps(table->read_set, table->write_set, old_maps);
  }
  return false;
}

bool Item_bool_func2::convert_constant_arg(THD *thd, Item *field, Item **item,
                                           bool *converted) {
  *converted = false;
  if (field->real_item()->type() != FIELD_ITEM) return false;

  Item_field *field_item = (Item_field *)(field->real_item());
  if (field_item->field->can_be_compared_as_longlong() &&
      !(field_item->is_temporal_with_date() &&
        (*item)->result_type() == STRING_RESULT)) {
    if (convert_constant_item(thd, field_item, item, converted)) return true;
    if (*converted) {
      if (cmp.set_cmp_func(this, args, args + 1, INT_RESULT)) return true;
      field->cmp_context = (*item)->cmp_context = INT_RESULT;
    }
  }
  return false;
}

bool Item_bool_func2::resolve_type(THD *thd) {
  DBUG_TRACE;

  // Both arguments are needed for type resolving
  assert(args[0] && args[1]);

  Item_bool_func::resolve_type(thd);
  /*
    See agg_item_charsets() in item.cc for comments
    on character set and collation aggregation.
    Charset comparison is skipped for SHOW CREATE VIEW
    statements since the join fields are not resolved
    during SHOW CREATE VIEW.
  */
  if (thd->lex->sql_command != SQLCOM_SHOW_CREATE &&
      args[0]->result_type() == STRING_RESULT &&
      args[1]->result_type() == STRING_RESULT &&
      agg_arg_charsets_for_comparison(cmp.cmp_collation, args, 2))
    return true;

  args[0]->cmp_context = args[1]->cmp_context =
      item_cmp_type(args[0]->result_type(), args[1]->result_type());

  /*
    Geometry item cannot participate in an arithmetic or string comparison or
    a full text search, except in equal/not equal comparison.
    We allow geometry arguments in equal/not equal, since such
    comparisons are used now and are meaningful, although it simply compares
    the GEOMETRY byte string rather than doing a geometric equality comparison.
  */
  const Functype func_type = functype();

  uint nvector_args = num_vector_args();
  if (func_type == EQ_FUNC && nvector_args != 0 && nvector_args != arg_count) {
    my_error(ER_WRONG_ARGUMENTS, MYF(0), func_name());
    return true;
  }

  if ((func_type == LT_FUNC || func_type == LE_FUNC || func_type == GE_FUNC ||
       func_type == GT_FUNC || func_type == FT_FUNC) &&
      (reject_geometry_args() || reject_vector_args()))
    return true;

  // Make a special case of compare with fields to get nicer DATE comparisons
  if (!(thd->lex->is_view_context_analysis())) {
    bool cvt1, cvt2;
    if (convert_constant_arg(thd, args[0], &args[1], &cvt1) ||
        convert_constant_arg(thd, args[1], &args[0], &cvt2))
      return true;
    if (cvt1 || cvt2) return false;
  }

  if (marker == MARKER_IMPLICIT_NE_ZERO) {  // Results may surprise
    if (args[1]->result_type() == STRING_RESULT &&
        args[1]->data_type() == MYSQL_TYPE_JSON)
      push_warning(thd, Sql_condition::SL_WARNING,
                   ER_IMPLICIT_COMPARISON_FOR_JSON,
                   ER_THD(thd, ER_IMPLICIT_COMPARISON_FOR_JSON));
  }

  return (thd->lex->sql_command != SQLCOM_SHOW_CREATE) ? set_cmp_func() : false;
}

bool Item_func_like::resolve_type(THD *thd) {
  // Function returns 0 or 1
  max_length = 1;

  // Determine the common character set for all arguments
  if (agg_arg_charsets_for_comparison(cmp.cmp_collation, args, arg_count))
    return true;

  for (uint i = 0; i < arg_count; i++) {
    if (args[i]->data_type() == MYSQL_TYPE_INVALID &&
        args[i]->propagate_type(
            thd,
            Type_properties(MYSQL_TYPE_VARCHAR, cmp.cmp_collation.collation))) {
      return true;
    }
  }

  if (reject_geometry_args()) return true;
  if (reject_vector_args()) return true;

  // LIKE is always carried out as a string operation
  args[0]->cmp_context = STRING_RESULT;
  args[1]->cmp_context = STRING_RESULT;

  if (arg_count > 2) {
    args[2]->cmp_context = STRING_RESULT;

    // ESCAPE clauses that vary per row are not valid:
    if (!args[2]->const_for_execution()) {
      my_error(ER_WRONG_ARGUMENTS, MYF(0), "ESCAPE");
      return true;
    }
  }
  /*
    If the escape item is const, evaluate it now, so that the range optimizer
    can try to optimize LIKE 'foo%' into a range query.

    TODO: If we move this into escape_is_evaluated(), which is called later,
          we might be able to optimize more cases.
  */
  if (!escape_was_used_in_parsing() || args[2]->const_item()) {
    escape_is_const = true;
    if (!(thd->lex->context_analysis_only & CONTEXT_ANALYSIS_ONLY_VIEW)) {
      if (eval_escape_clause(thd)) return true;
      if (check_covering_prefix_keys(thd)) return true;
    }
  }

  return false;
}

Item *Item_func_like::replace_scalar_subquery(uchar *) {
  // Replacing a scalar subquery with a reference to a column in a derived table
  // could change the constness. Check that the ESCAPE clause is still
  // const_for_execution().
  if (escape_was_used_in_parsing() && !args[2]->const_for_execution()) {
    my_error(ER_WRONG_ARGUMENTS, MYF(0), "ESCAPE");
    return nullptr;
  }
  return this;
}

Item *Item_bool_func2::replace_scalar_subquery(uchar *) {
  if (set_cmp_func()) {
    return nullptr;
  }
  return this;
}

void Arg_comparator::cleanup() {
  if (comparators != nullptr) {
    /*
      We cannot rely on (*left)->cols(), since *left may be deallocated
      at this point, so use comparator_count to loop.
    */
    for (size_t i = 0; i < comparator_count; i++) {
      comparators[i].cleanup();
    }
  }
  if (json_scalar != nullptr) {
    ::destroy_at(json_scalar);
    json_scalar = nullptr;
  }
  value1.mem_free();
  value2.mem_free();
}

bool Arg_comparator::set_compare_func(Item_func *item, Item_result type) {
  m_compare_type = type;
  owner = item;
  func = comparator_matrix[type];

  switch (type) {
    case ROW_RESULT: {
      const uint n = (*left)->cols();
      if (n != (*right)->cols()) {
        my_error(ER_OPERAND_COLUMNS, MYF(0), n);
        comparators = nullptr;
        return true;
      }
      if (!(comparators = new (*THR_MALLOC) Arg_comparator[n])) return true;
      comparator_count = n;

      for (uint i = 0; i < n; i++) {
        if ((*left)->element_index(i)->cols() !=
            (*right)->element_index(i)->cols()) {
          my_error(ER_OPERAND_COLUMNS, MYF(0),
                   (*left)->element_index(i)->cols());
          return true;
        }
        if (comparators[i].set_cmp_func(owner, (*left)->addr(i),
                                        (*right)->addr(i), set_null))
          return true;
      }
      break;
    }
    case STRING_RESULT: {
      /*
        We must set cmp_charset here as we may be called from for an automatic
        generated item, like in natural join
      */
      if (cmp_collation.set((*left)->collation, (*right)->collation,
                            MY_COLL_CMP_CONV) ||
          cmp_collation.derivation == DERIVATION_NONE) {
        const char *func_name = owner != nullptr ? owner->func_name() : "";
        my_coll_agg_error((*left)->collation, (*right)->collation, func_name);
        return true;
      }
      if (cmp_collation.collation == &my_charset_bin) {
        /*
          We are using BLOB/BINARY/VARBINARY, change to compare byte by byte,
          without removing end space
        */
        if (func == &Arg_comparator::compare_string)
          func = &Arg_comparator::compare_binary_string;
      }
      /*
        If the comparison's and arguments' collations differ, prevent column
        substitution. Otherwise we would get into trouble with comparisons
        like:
        WHERE col = 'j' AND col = BINARY 'j'
        which would be transformed to:
        WHERE col = 'j' AND 'j' = BINARY 'j', then to:
        WHERE col = 'j'. That would be wrong, if col contains 'J'.
      */
      if ((*left)->collation.collation != cmp_collation.collation)
        (*left)->walk(&Item::disable_constant_propagation, enum_walk::POSTFIX,
                      nullptr);
      if ((*right)->collation.collation != cmp_collation.collation)
        (*right)->walk(&Item::disable_constant_propagation, enum_walk::POSTFIX,
                       nullptr);

      break;
    }
    case INT_RESULT: {
      if ((*left)->is_temporal() && (*right)->is_temporal()) {
        func = &Arg_comparator::compare_time_packed;
      } else if (func == &Arg_comparator::compare_int_signed) {
        if ((*left)->unsigned_flag)
          func = (((*right)->unsigned_flag)
                      ? &Arg_comparator::compare_int_unsigned
                      : &Arg_comparator::compare_int_unsigned_signed);
        else if ((*right)->unsigned_flag)
          func = &Arg_comparator::compare_int_signed_unsigned;
      }
      break;
    }
    case DECIMAL_RESULT:
      break;
    case REAL_RESULT: {
      if ((*left)->decimals < DECIMAL_NOT_SPECIFIED &&
          (*right)->decimals < DECIMAL_NOT_SPECIFIED) {
        precision = 5 / log_10[max((*left)->decimals, (*right)->decimals) + 1];
        if (func == &Arg_comparator::compare_real)
          func = &Arg_comparator::compare_real_fixed;
      }
      break;
    }
    default:
      assert(0);
  }
  return false;
}

/**
  A minion of get_mysql_time_from_str, see its description.
  This version doesn't issue any warnings, leaving that to its parent.
  This method has one extra argument which return warnings.

  @param[in]   thd           Thread handle
  @param[in]   str           A string to convert
  @param[out]  l_time        The MYSQL_TIME objects is initialized.
  @param[in, out] status     Any warnings given are returned here
  @returns true if error
*/
bool get_mysql_time_from_str_no_warn(THD *thd, String *str, MYSQL_TIME *l_time,
                                     MYSQL_TIME_STATUS *status) {
  my_time_flags_t flags = TIME_FUZZY_DATE | TIME_INVALID_DATES;

  if (thd->variables.sql_mode & MODE_NO_ZERO_IN_DATE)
    flags |= TIME_NO_ZERO_IN_DATE;
  if (thd->variables.sql_mode & MODE_NO_ZERO_DATE) flags |= TIME_NO_ZERO_DATE;
  if (thd->is_fsp_truncate_mode()) flags |= TIME_FRAC_TRUNCATE;
  return str_to_datetime(str, l_time, flags, status);
}
/**
  Parse date provided in a string to a MYSQL_TIME.

  @param[in]   thd           Thread handle
  @param[in]   str           A string to convert
  @param[in]   warn_type     Type of the timestamp for issuing the warning
  @param[in]   warn_name     Field name for issuing the warning
  @param[out]  l_time        The MYSQL_TIME objects is initialized.

  Parses a date provided in the string str into a MYSQL_TIME object. If the
  string contains an incorrect date or doesn't correspond to a date at all
  then a warning is issued. The warn_type and the warn_name arguments are used
  as the name and the type of the field when issuing the warning. If any input
  was discarded (trailing or non-timestamp-y characters), return value will be
  true.

  @return Status flag
  @retval false Success.
  @retval True Indicates failure.
*/

bool get_mysql_time_from_str(THD *thd, String *str,
                             enum_mysql_timestamp_type warn_type,
                             const char *warn_name, MYSQL_TIME *l_time) {
  bool value;
  MYSQL_TIME_STATUS status;
  my_time_flags_t flags = TIME_FUZZY_DATE;
  if (thd->variables.sql_mode & MODE_NO_ZERO_IN_DATE)
    flags |= TIME_NO_ZERO_IN_DATE;
  if (thd->variables.sql_mode & MODE_NO_ZERO_DATE) flags |= TIME_NO_ZERO_DATE;
  if (thd->is_fsp_truncate_mode()) flags |= TIME_FRAC_TRUNCATE;
  if (thd->variables.sql_mode & MODE_INVALID_DATES) flags |= TIME_INVALID_DATES;

  if (!propagate_datetime_overflow(
          thd, &status.warnings,
          str_to_datetime(str, l_time, flags, &status)) &&
      (l_time->time_type == MYSQL_TIMESTAMP_DATETIME ||
       l_time->time_type == MYSQL_TIMESTAMP_DATETIME_TZ ||
       l_time->time_type == MYSQL_TIMESTAMP_DATE)) {
    /*
      Do not return yet, we may still want to throw a "trailing garbage"
      warning.
    */
    check_deprecated_datetime_format(thd, str->charset(), status);
    value = false;
  } else {
    value = true;
    status.warnings = MYSQL_TIME_WARN_TRUNCATED; /* force warning */
  }

  if (status.warnings > 0) {
    if (make_truncated_value_warning(thd, Sql_condition::SL_WARNING,
                                     ErrConvString(str), warn_type, warn_name))
      return true;
  }

  return value;
}

/**
  @brief Convert date provided in a string
  to its packed temporal int representation.

  @param[in]   thd        thread handle
  @param[in]   str        a string to convert
  @param[in]   warn_type  type of the timestamp for issuing the warning
  @param[in]   warn_name  field name for issuing the warning
  @param[out]  error_arg  could not extract a DATE or DATETIME

  @details Convert date provided in the string str to the int
    representation.  If the string contains wrong date or doesn't
    contain it at all then a warning is issued.  The warn_type and
    the warn_name arguments are used as the name and the type of the
    field when issuing the warning.

  @return
    converted value. 0 on error and on zero-dates -- check 'failure'
*/
static ulonglong get_date_from_str(THD *thd, String *str,
                                   enum_mysql_timestamp_type warn_type,
                                   const char *warn_name, bool *error_arg) {
  MYSQL_TIME l_time;
  *error_arg = get_mysql_time_from_str(thd, str, warn_type, warn_name, &l_time);

  if (*error_arg) return 0;
  return TIME_to_longlong_datetime_packed(l_time);
}

/**
  Check if str_arg is a constant and convert it to datetime packed value.
  Note, const_value may stay untouched, so the caller is responsible to
  initialize it.

  @param         date_arg    date argument, its name is used for error
                             reporting.
  @param         str_arg     string argument to get datetime value from.
  @param[in,out] const_value If not nullptr, the converted value is stored
                             here. To detect that conversion was not possible,
                             the caller is responsible for initializing this
                             value to MYSQL_TIMESTAMP_ERROR before calling
                             and checking the value has changed after the call.

  @return true on error, false on success, false if str_arg is not a const.
*/
bool Arg_comparator::get_date_from_const(Item *date_arg, Item *str_arg,
                                         ulonglong *const_value) {
  THD *thd = current_thd;
  assert(str_arg->result_type() == STRING_RESULT);
  /*
    Don't use cache while in the context analysis mode only (i.e. for
    EXPLAIN/CREATE VIEW and similar queries). Cache is useless in such
    cases and can cause problems. For example evaluating subqueries can
    confuse storage engines since in context analysis mode tables
    aren't locked.
  */
  if (!(thd->lex->context_analysis_only & CONTEXT_ANALYSIS_ONLY_VIEW) &&
      str_arg->may_evaluate_const(thd)) {
    ulonglong value;
    if (str_arg->data_type() == MYSQL_TYPE_TIME) {
      // Convert from TIME to DATETIME numeric packed value
      value = str_arg->val_date_temporal();
      if (str_arg->null_value) return true;
    } else {
      // Convert from string to DATETIME numeric packed value
      const enum_field_types date_arg_type = date_arg->data_type();
      const enum_mysql_timestamp_type t_type =
          (date_arg_type == MYSQL_TYPE_DATE ? MYSQL_TIMESTAMP_DATE
                                            : MYSQL_TIMESTAMP_DATETIME);
      String tmp;
      String *str_val = str_arg->val_str(&tmp);
      if (str_arg->null_value) return true;
      bool error;
      value = get_date_from_str(thd, str_val, t_type, date_arg->item_name.ptr(),
                                &error);
      if (error) {
        const char *typestr = (date_arg_type == MYSQL_TYPE_DATE) ? "DATE"
                              : (date_arg_type == MYSQL_TYPE_DATETIME)
                                  ? "DATETIME"
                                  : "TIMESTAMP";

        const ErrConvString err(str_val->ptr(), str_val->length(),
                                thd->variables.character_set_client);
        my_error(ER_WRONG_VALUE, MYF(0), typestr, err.ptr());

        return true;
      }
    }
    if (const_value) *const_value = value;
  }
  return false;
}

/**
  Checks whether compare_datetime() can be used to compare items.

  SYNOPSIS
    Arg_comparator::can_compare_as_dates()
    left, right          [in]  items to be compared

  DESCRIPTION
    Checks several cases when the DATETIME comparator should be used.
    The following cases are accepted:
      1. Both left and right is a DATE/DATETIME/TIMESTAMP field/function
         returning string or int result.
      2. Only left or right is a DATE/DATETIME/TIMESTAMP field/function
         returning string or int result and the other item (right or left) is an
         item with string result.
  This doesn't mean that the string can necessarily be successfully converted to
  a datetime value. But if it cannot this will lead to an error later,
  @see Arg_comparator::get_date_from_const

      In all other cases (date-[int|real|decimal]/[int|real|decimal]-date)
      the comparison is handled by other comparators.

  @return true if the Arg_comparator::compare_datetime should be used,
          false otherwise
*/

bool Arg_comparator::can_compare_as_dates(const Item *left, const Item *right) {
  if (left->type() == Item::ROW_ITEM || right->type() == Item::ROW_ITEM)
    return false;

  if (left->is_temporal_with_date() &&
      (right->result_type() == STRING_RESULT || right->is_temporal_with_date()))
    return true;
  else
    return left->result_type() == STRING_RESULT &&
           right->is_temporal_with_date();
}

/**
  Retrieves correct TIME value from the given item.

  @param [in,out] item_arg    item to retrieve TIME value from
  @param [out] is_null        true <=> the item_arg is null

  @returns obtained value

  Retrieves the correct TIME value from given item for comparison by the
  compare_datetime() function.
  If item's result can be compared as longlong then its int value is used
  and a value returned by get_time function is used otherwise.
*/

static longlong get_time_value(THD *, Item ***item_arg, Item **, const Item *,
                               bool *is_null) {
  longlong value = 0;
  Item *item = **item_arg;
  String buf, *str = nullptr;

  if (item->data_type() == MYSQL_TYPE_TIME ||
      item->data_type() == MYSQL_TYPE_NULL) {
    value = item->val_time_temporal();
    *is_null = item->null_value;
  } else {
    str = item->val_str(&buf);
    *is_null = item->null_value;
  }
  if (*is_null) return ~(ulonglong)0;

  /*
    Convert strings to the integer TIME representation.
  */
  if (str) {
    MYSQL_TIME l_time;
    if (str_to_time_with_warn(str, &l_time)) {
      *is_null = true;
      return ~(ulonglong)0;
    }
    value = TIME_to_longlong_datetime_packed(l_time);
  }

  return value;
}

/**
  Sets compare functions for various datatypes.

  It additionally sets up Item_cache objects for caching any constant values
  that need conversion to a type compatible with the comparator type, to avoid
  the need for performing the conversion again each time the comparator is
  invoked.

  NOTE
    The result type of a comparison is chosen by item_cmp_type().
    Here we override the chosen result type for certain expression
    containing date or time or decimal expressions.
 */
bool Arg_comparator::set_cmp_func(Item_func *owner_arg, Item **left_arg,
                                  Item **right_arg, Item_result type) {
  m_compare_type = type;
  owner = owner_arg;
  set_null = set_null && owner_arg;
  left = left_arg;
  right = right_arg;

  if (type != ROW_RESULT && (((*left)->result_type() == STRING_RESULT &&
                              (*left)->data_type() == MYSQL_TYPE_JSON) ||
                             ((*right)->result_type() == STRING_RESULT &&
                              (*right)->data_type() == MYSQL_TYPE_JSON))) {
    // Use the JSON comparator if at least one of the arguments is JSON.
    func = &Arg_comparator::compare_json;
    m_compare_type = STRING_RESULT;
    // Convention: Immediate dynamic parameters are handled as scalars:
    (*left)->mark_json_as_scalar();
    (*right)->mark_json_as_scalar();
    return false;
  }

  /*
    Checks whether at least one of the arguments is DATE/DATETIME/TIMESTAMP
    and the other one is also DATE/DATETIME/TIMESTAMP or a constant string.
  */
  if (can_compare_as_dates(*left, *right)) {
    left_cache = nullptr;
    right_cache = nullptr;
    ulonglong numeric_datetime = static_cast<ulonglong>(MYSQL_TIMESTAMP_ERROR);

    /*
      If one of the arguments is constant string, try to convert it
      to DATETIME and cache it.
    */
    if (!(*left)->is_temporal_with_date()) {
      if (!get_date_from_const(*right, *left, &numeric_datetime) &&
          numeric_datetime != static_cast<ulonglong>(MYSQL_TIMESTAMP_ERROR)) {
        auto *cache = new Item_cache_datetime(MYSQL_TYPE_DATETIME);
        // OOM
        if (!cache) return true; /* purecov: inspected */
        cache->store_value((*left), numeric_datetime);
        // Mark the cache as non-const to prevent re-caching.
        cache->set_used_tables(1);
        left_cache = cache;
        left = &left_cache;
      }
    } else if (!(*right)->is_temporal_with_date()) {
      if (!get_date_from_const(*left, *right, &numeric_datetime) &&
          numeric_datetime != static_cast<ulonglong>(MYSQL_TIMESTAMP_ERROR)) {
        auto *cache = new Item_cache_datetime(MYSQL_TYPE_DATETIME);
        // OOM
        if (!cache) return true; /* purecov: inspected */
        cache->store_value((*right), numeric_datetime);
        // Mark the cache as non-const to prevent re-caching.
        cache->set_used_tables(1);
        right_cache = cache;
        right = &right_cache;
      }
    }
    if (current_thd->is_error()) return true;
    func = &Arg_comparator::compare_datetime;
    get_value_a_func = &get_datetime_value;
    get_value_b_func = &get_datetime_value;
    cmp_collation.set(&my_charset_numeric);
    set_cmp_context_for_datetime();
    return false;
  } else if ((type == STRING_RESULT ||
              // When comparing time field and cached/converted time constant
              type == REAL_RESULT) &&
             (*left)->data_type() == MYSQL_TYPE_TIME &&
             (*right)->data_type() == MYSQL_TYPE_TIME) {
    /* Compare TIME values as integers. */
    left_cache = nullptr;
    right_cache = nullptr;
    func = &Arg_comparator::compare_datetime;
    get_value_a_func = &get_time_value;
    get_value_b_func = &get_time_value;
    set_cmp_context_for_datetime();
    return false;
  } else if (type == STRING_RESULT && (*left)->result_type() == STRING_RESULT &&
             (*right)->result_type() == STRING_RESULT) {
    DTCollation coll;
    coll.set((*left)->collation, (*right)->collation, MY_COLL_CMP_CONV);
    /*
      DTCollation::set() may have chosen a charset that is a superset of both
      and "left" and "right", so both items may need conversion.
      Note this may be considered redundant for non-row arguments but necessary
      for row arguments.
     */
    if (convert_const_strings(coll, left, 1, 1)) {
      return true;
    }
    if (convert_const_strings(coll, right, 1, 1)) {
      return true;
    }
  } else if (try_year_cmp_func(type)) {
    return false;
  } else if (type == REAL_RESULT &&
             (((*left)->result_type() == DECIMAL_RESULT &&
               !(*left)->const_item() &&
               (*right)->result_type() == STRING_RESULT &&
               (*right)->const_item()) ||
              ((*right)->result_type() == DECIMAL_RESULT &&
               !(*right)->const_item() &&
               (*left)->result_type() == STRING_RESULT &&
               (*left)->const_item()))) {
    /*
     <non-const decimal expression> <cmp> <const string expression>
     or
     <const string expression> <cmp> <non-const decimal expression>

     Do comparison as decimal rather than float, in order not to lose precision.
    */
    type = DECIMAL_RESULT;
  }

  THD *thd = current_thd;
  left = cache_converted_constant(thd, left, &left_cache, type);
  right = cache_converted_constant(thd, right, &right_cache, type);
  return set_compare_func(owner_arg, type);
}

bool Arg_comparator::set_cmp_func(Item_func *owner_arg, Item **left_arg,
                                  Item **right_arg, bool set_null_arg) {
  set_null = set_null_arg;
  const Item_result item_result =
      item_cmp_type((*left_arg)->result_type(), (*right_arg)->result_type());
  return set_cmp_func(owner_arg, left_arg, right_arg, item_result);
}

bool Arg_comparator::set_cmp_func(Item_func *owner_arg, Item **left_arg,
                                  Item **right_arg, bool set_null_arg,
                                  Item_result type) {
  set_null = set_null_arg;
  return set_cmp_func(owner_arg, left_arg, right_arg, type);
}

/**
   Wraps the item into a CAST function to the type provided as argument
   @param item - the item to be wrapped
   @param type - the type to wrap the item to
   @returns true if error (OOM), false otherwise.
 */
inline bool wrap_in_cast(Item **item, enum_field_types type) {
  THD *thd = current_thd;
  Item *cast = nullptr;
  switch (type) {
    case MYSQL_TYPE_DATETIME: {
      cast = new Item_typecast_datetime(*item, false);
      break;
    }
    case MYSQL_TYPE_DATE: {
      cast = new Item_typecast_date(*item, false);
      break;
    }
    case MYSQL_TYPE_TIME: {
      cast = new Item_typecast_time(*item);
      break;
    }
    case MYSQL_TYPE_DOUBLE: {
      cast = new Item_typecast_real(*item);
      break;
    }
    default: {
      assert(false);
      return true;
    }
  }
  if (cast == nullptr) return true;

  if (cast->fix_fields(thd, item)) return true;
  thd->change_item_tree(item, cast);

  return false;
}

/**
 * Checks that the argument is an aggregation function, window function, a
 * built-in non-constant function or a non-constant field.
 * WL#12108: it excludes stored procedures and functions, user defined
 * functions and also does not update the content of expressions
 * inside Value_generator since Optimize is not called after the expression
 * is unpacked.
 * @param item to be checked
 * @return  true for non-const field or functions, false otherwise
 */
inline bool is_non_const_field_or_function(const Item &item) {
  return !item.const_for_execution() &&
         (item.type() == Item::FIELD_ITEM || item.type() == Item::FUNC_ITEM ||
          item.type() == Item::SUM_FUNC_ITEM);
}

bool Arg_comparator::inject_cast_nodes() {
  // If the comparator is set to one that compares as floating point numbers.
  if (func == &Arg_comparator::compare_real ||
      func == &Arg_comparator::compare_real_fixed) {
    Item *aa = (*left)->real_item();
    Item *bb = (*right)->real_item();

    // No cast nodes are injected if both arguments are numeric
    // (that includes YEAR data type)
    if (!((aa->result_type() == STRING_RESULT &&
           (bb->result_type() == INT_RESULT ||
            bb->result_type() == REAL_RESULT ||
            bb->result_type() == DECIMAL_RESULT)) ||
          (bb->result_type() == STRING_RESULT &&
           (aa->result_type() == INT_RESULT ||
            aa->result_type() == REAL_RESULT ||
            aa->result_type() == DECIMAL_RESULT))))
      return false;

    // No CAST nodes are injected in comparisons with YEAR
    if ((aa->data_type() == MYSQL_TYPE_YEAR &&
         (bb->data_type() == MYSQL_TYPE_TIME ||
          bb->data_type() == MYSQL_TYPE_TIME2)) ||
        (bb->data_type() == MYSQL_TYPE_YEAR &&
         (aa->data_type() == MYSQL_TYPE_TIME ||
          aa->data_type() == MYSQL_TYPE_TIME2)))
      return false;

    // Check that both arguments are fields or functions
    if (!is_non_const_field_or_function(*aa) ||
        !is_non_const_field_or_function(*bb))
      return false;

    // If any of the arguments is not floating point number, wrap it in a CAST
    if (aa->result_type() != REAL_RESULT &&
        wrap_in_cast(left, MYSQL_TYPE_DOUBLE))
      return true; /* purecov: inspected */
    if (bb->result_type() != REAL_RESULT &&
        wrap_in_cast(right, MYSQL_TYPE_DOUBLE))
      return true; /* purecov: inspected */
  } else if (func == &Arg_comparator::compare_datetime) {
    Item *aa = (*left)->real_item();
    Item *bb = (*right)->real_item();
    // Check that none of the arguments are of type YEAR
    if (aa->data_type() == MYSQL_TYPE_YEAR ||
        bb->data_type() == MYSQL_TYPE_YEAR)
      return false;

    // Check that both arguments are fields or functions and that they have
    // different data types
    if (!is_non_const_field_or_function(*aa) ||
        !is_non_const_field_or_function(*bb) ||
        aa->data_type() == bb->data_type())
      return false;

    const bool left_is_datetime = aa->is_temporal_with_date_and_time();
    const bool left_is_date = aa->is_temporal_with_date();
    const bool left_is_time = aa->is_temporal_with_time();

    const bool right_is_datetime = bb->is_temporal_with_date_and_time();
    const bool right_is_date = bb->is_temporal_with_date();
    const bool right_is_time = bb->is_temporal_with_time();

    // When one of the arguments is_temporal_with_date_and_time() or one
    // argument is DATE and the other one is TIME
    if (left_is_datetime || right_is_datetime ||
        (left_is_date && right_is_time) || (left_is_time && right_is_date)) {
      if (!left_is_datetime && !right_is_datetime) {
        // one is DATE, the other one is TIME so wrap both in CAST to DATETIME
        return wrap_in_cast(left, MYSQL_TYPE_DATETIME) ||
               wrap_in_cast(right, MYSQL_TYPE_DATETIME);
      }
      if (left_is_datetime && right_is_datetime) {
        // E.g., DATETIME = TIMESTAMP. We allow this (we could even produce it
        // ourselves by the logic below).
        return false;
      }
      // one is DATETIME the other one is not
      return left_is_datetime ? wrap_in_cast(right, MYSQL_TYPE_DATETIME)
                              : wrap_in_cast(left, MYSQL_TYPE_DATETIME);
    }

    // One of the arguments is DATE, wrap the other in CAST to DATE
    if (left_is_date || right_is_date) {
      return left_is_date ? wrap_in_cast(right, MYSQL_TYPE_DATE)
                          : wrap_in_cast(left, MYSQL_TYPE_DATE);
    }

    assert(left_is_time || right_is_time);
    // one of the arguments is TIME, wrap the other one in CAST to TIME
    return left_is_time ? wrap_in_cast(right, MYSQL_TYPE_TIME)
                        : wrap_in_cast(left, MYSQL_TYPE_TIME);
  }

  return false;
}

/*
  Helper function to call from Arg_comparator::set_cmp_func()
*/

bool Arg_comparator::try_year_cmp_func(Item_result type) {
  if (type == ROW_RESULT) return false;

  const bool a_is_year = (*left)->data_type() == MYSQL_TYPE_YEAR;
  const bool b_is_year = (*right)->data_type() == MYSQL_TYPE_YEAR;

  if (!a_is_year && !b_is_year) return false;

  if (a_is_year && b_is_year) {
    get_value_a_func = &get_year_value;
    get_value_b_func = &get_year_value;
  } else if (a_is_year && (*right)->is_temporal_with_date()) {
    get_value_a_func = &get_year_value;
    get_value_b_func = &get_datetime_value;
  } else if (b_is_year && (*left)->is_temporal_with_date()) {
    get_value_b_func = &get_year_value;
    get_value_a_func = &get_datetime_value;
  } else
    return false;

  func = &Arg_comparator::compare_datetime;
  set_cmp_context_for_datetime();

  return true;
}

/**
  Convert and cache a constant.

  @param thd   The current session.
  @param value An item to cache
  @param[out]  cache_item Placeholder for the cache item
  @param type  Comparison type

  @details
    When given item is a constant and its type differs from comparison type
    then cache its value to avoid type conversion of this constant on each
    evaluation. In this case the value is cached and the reference to the cache
    is returned.
    Original value is returned otherwise.

  @return cache item or original value.
*/

static Item **cache_converted_constant(THD *thd, Item **value,
                                       Item **cache_item, Item_result type) {
  // Don't need cache if doing context analysis only.
  if (!(thd->lex->context_analysis_only & CONTEXT_ANALYSIS_ONLY_VIEW) &&
      (*value)->const_for_execution() && type != (*value)->result_type()) {
    Item_cache *cache = Item_cache::get_cache(*value, type);
    cache->setup(*value);
    *cache_item = cache;
    return cache_item;
  }
  return value;
}

void Arg_comparator::set_datetime_cmp_func(Item_func *owner_arg,
                                           Item **left_arg, Item **right_arg) {
  owner = owner_arg;
  left = left_arg;
  right = right_arg;
  left_cache = nullptr;
  right_cache = nullptr;
  func = &Arg_comparator::compare_datetime;
  get_value_a_func = &get_datetime_value;
  get_value_b_func = &get_datetime_value;
  set_cmp_context_for_datetime();
}

/**
  Retrieve correct DATETIME value from given item.

  @param thd           thread handle
  @param item_arg      item to retrieve DATETIME value from
  @param warn_item     item for issuing the conversion warning
  @param[out] is_null  true <=> the item_arg is null

  Retrieves the correct DATETIME value from given item for comparison by the
  compare_datetime() function.
  If item's result can be compared as longlong then its int value is used
  and its string value is used otherwise. Strings are always parsed and
  converted to int values by the get_date_from_str() function.
  This allows us to compare correctly string dates with missed insignificant
  zeros. In order to compare correctly DATE and DATETIME items the result
  of the former are treated as a DATETIME with zero time (00:00:00).

  @returns the DATETIME value, all ones if Item is NULL
*/

longlong get_datetime_value(THD *thd, Item ***item_arg, Item **,
                            const Item *warn_item, bool *is_null) {
  longlong value = 0;
  String buf, *str = nullptr;

  Item *item = **item_arg;
  if (item->is_temporal() && item->data_type() != MYSQL_TYPE_YEAR) {
    value = item->val_date_temporal();
    *is_null = item->null_value;
  } else {
    str = item->val_str(&buf);
    *is_null = item->null_value;
  }
  if (*is_null) return ~(ulonglong)0;
  /*
    Convert strings to the integer DATE/DATETIME representation.
    Even if both dates provided in strings we can't compare them directly as
    strings as there is no warranty that they are correct and do not miss
    some insignificant zeros.
  */
  if (str) {
    bool error;
    const enum_field_types f_type = warn_item->data_type();
    const enum_mysql_timestamp_type t_type = f_type == MYSQL_TYPE_DATE
                                                 ? MYSQL_TIMESTAMP_DATE
                                                 : MYSQL_TIMESTAMP_DATETIME;
    value = (longlong)get_date_from_str(thd, str, t_type,
                                        warn_item->item_name.ptr(), &error);
    /*
      If str did not contain a valid date according to the current
      SQL_MODE, get_date_from_str() has already thrown a warning,
      and we don't want to throw NULL on invalid date (see 5.2.6
      "SQL modes" in the manual), so we're done here.
    */
  }

  // @todo WL#6570: restore caching of datetime values here,
  // this should affect the count of warnings in mtr test
  // engines.funcs.update_delete_calendar.

  return value;
}

/*
  Retrieves YEAR value of 19XX-00-00 00:00:00 form from given item.

  SYNOPSIS
    get_year_value()
    item_arg   [in/out] item to retrieve YEAR value from
    is_null    [out]    true <=> the item_arg is null

  DESCRIPTION
    Retrieves the YEAR value of 19XX form from given item for comparison by the
    compare_datetime() function.
    Converts year to DATETIME of form YYYY-00-00 00:00:00 for the compatibility
    with the get_datetime_value function result.

  RETURN
    obtained value
*/

static longlong get_year_value(THD *, Item ***item_arg, Item **, const Item *,
                               bool *is_null) {
  longlong value = 0;
  Item *item = **item_arg;

  value = item->val_int();
  *is_null = item->null_value;
  if (*is_null) return ~(ulonglong)0;

  /* Convert year to DATETIME packed format */
  return year_to_longlong_datetime_packed(static_cast<long>(value));
}

/**
  Compare item values as dates.

  Compare items values as DATE/DATETIME for regular comparison functions.
  The correct DATETIME values are obtained with help of
  the get_datetime_value() function.

  @returns
    -1   left < right or at least one item is null
     0   left == right
     1   left > right
    See the table:
    left_is_null    | 1 | 0 | 1 | 0 |
    right_is_null   | 1 | 1 | 0 | 0 |
    result          |-1 |-1 |-1 |-1/0/1|
*/

int Arg_comparator::compare_datetime() {
  bool left_is_null, right_is_null;
  longlong left_value, right_value;
  THD *thd = current_thd;

  /* Get DATE/DATETIME/TIME value of the 'left' item. */
  left_value =
      (*get_value_a_func)(thd, &left, &left_cache, *right, &left_is_null);
  if (left_is_null) {
    if (set_null) owner->null_value = true;
    return -1;
  }

  /* Get DATE/DATETIME/TIME value of the 'right' item. */
  right_value =
      (*get_value_b_func)(thd, &right, &right_cache, *left, &right_is_null);
  if (right_is_null) {
    if (set_null) owner->null_value = true;
    return -1;
  }

  /* Here we have two not-NULL values. */
  if (set_null) owner->null_value = false;

  /* Compare values. */
  return left_value < right_value ? -1 : (left_value > right_value ? 1 : 0);
}

/**
  Get one of the arguments to the comparator as a JSON value.

  @param[in]     arg     pointer to the argument
  @param[in,out] value   buffer used for reading the JSON value
  @param[in,out] tmp     buffer used for converting string values to the
                         correct charset, if necessary
  @param[out]    result  where to store the result
  @param[in,out] scalar  pointer to a location with pre-allocated memory
                         used for JSON scalars that are converted from
                         SQL scalars

  @retval false on success
  @retval true on failure
*/
static bool get_json_arg(Item *arg, String *value, String *tmp,
                         Json_wrapper *result, Json_scalar_holder **scalar) {
  Json_scalar_holder *holder = nullptr;

  /*
    If the argument is a non-JSON type, it gets converted to a JSON
    scalar. Use the pre-allocated memory passed in via the "scalar"
    argument. Note, however, that geometry types are not converted
    to scalars. They are converted to JSON objects by get_json_atom_wrapper().
  */
  if ((arg->data_type() != MYSQL_TYPE_JSON) &&
      (arg->data_type() != MYSQL_TYPE_GEOMETRY)) {
    /*
      If it's a constant item, and we've already read it, just return
      the value that's cached in the pre-allocated memory.
    */
    if (*scalar && arg->const_item()) {
      *result = Json_wrapper((*scalar)->get());
      /*
        The DOM object lives in memory owned by the Json_scalar_holder. Tell
        the wrapper that it's not the owner.
      */
      result->set_alias();
      return false;
    }

    /*
      Allocate memory to hold the scalar, if we haven't already done
      so. Otherwise, we reuse the previously allocated memory.
    */
    if (*scalar == nullptr) *scalar = new (*THR_MALLOC) Json_scalar_holder();

    holder = *scalar;
  }

  return get_json_atom_wrapper(&arg, 0, "<=", value, tmp, result, holder, true);
}

/**
  Compare two Item objects as JSON.

  If one of the arguments is NULL, and the owner is not EQUAL_FUNC,
  the null_value flag of the owner will be set to true.

  @return -1 if at least one of the items is NULL or if the first item is
             less than the second item,
           0 if the two items are equal
           1 if the first item is greater than the second item.
*/
int Arg_comparator::compare_json() {
  char buf[STRING_BUFFER_USUAL_SIZE];
  String tmp(buf, sizeof(buf), &my_charset_bin);

  // Get the JSON value in the left Item.
  Json_wrapper aw;
  if (get_json_arg(*left, &value1, &tmp, &aw, &json_scalar)) {
    if (set_null) owner->null_value = true;
    return 1;
  }

  const bool a_is_null = (*left)->null_value;
  if (a_is_null) {
    if (set_null) owner->null_value = true;
    return -1;
  }

  // Get the JSON value in the right Item.
  Json_wrapper bw;
  if (get_json_arg(*right, &value1, &tmp, &bw, &json_scalar)) {
    if (set_null) owner->null_value = true;
    return 1;
  }

  const bool b_is_null = (*right)->null_value;
  if (b_is_null) {
    if (set_null) owner->null_value = true;
    return -1;
  }

  if (set_null) owner->null_value = false;

  return aw.compare(bw);
}

int Arg_comparator::compare_string() {
  const CHARSET_INFO *cs = cmp_collation.collation;
  String *res1 = eval_string_arg(cs, *left, &value1);
  if (res1 == nullptr) {
    if (set_null) owner->null_value = true;
    return -1;
  }
  String *res2 = eval_string_arg(cs, *right, &value2);
  if (res2 == nullptr) {
    if (set_null) owner->null_value = true;
    return -1;
  }

  if (set_null) owner->null_value = false;
  const size_t l1 = res1->length();
  const size_t l2 = res2->length();
  // Compare the two strings
  return cs->coll->strnncollsp(cs, pointer_cast<const uchar *>(res1->ptr()), l1,
                               pointer_cast<const uchar *>(res2->ptr()), l2);
}

/**
  Compare strings byte by byte. End spaces are also compared.

  @retval
    <0  *left < *right
  @retval
     0  *right == *right
  @retval
    >0  *left > *right
*/

int Arg_comparator::compare_binary_string() {
  String *res1, *res2;
  if ((res1 = (*left)->val_str(&value1))) {
    if ((res2 = (*right)->val_str(&value2))) {
      if (set_null) owner->null_value = false;
      const size_t len1 = res1->length();
      const size_t len2 = res2->length();
      const size_t min_length = min(len1, len2);
      const int cmp =
          min_length == 0 ? 0 : memcmp(res1->ptr(), res2->ptr(), min_length);
      auto rc = cmp ? cmp : (int)(len1 - len2);
      return rc;
    }
  }
  if (set_null) owner->null_value = true;
  return -1;
}

int Arg_comparator::compare_real() {
  double val1, val2;
  val1 = (*left)->val_real();
  if (current_thd->is_error()) return 0;
  if (!(*left)->null_value) {
    val2 = (*right)->val_real();
    if (current_thd->is_error()) return 0;
    if (!(*right)->null_value) {
      if (set_null) owner->null_value = false;
      if (val1 < val2) return -1;
      if (val1 == val2) return 0;
      return 1;
    }
  }
  if (set_null) owner->null_value = true;
  return -1;
}

int Arg_comparator::compare_decimal() {
  my_decimal decimal1;
  my_decimal *val1 = (*left)->val_decimal(&decimal1);
  if (current_thd->is_error()) return 0;
  if (!(*left)->null_value) {
    my_decimal decimal2;
    my_decimal *val2 = (*right)->val_decimal(&decimal2);
    if (current_thd->is_error()) return 0;
    if (!(*right)->null_value) {
      if (set_null) owner->null_value = false;
      return my_decimal_cmp(val1, val2);
    }
  }
  if (set_null) owner->null_value = true;
  return -1;
}

int Arg_comparator::compare_real_fixed() {
  double val1, val2;
  val1 = (*left)->val_real();
  if (current_thd->is_error()) return 0;
  if (!(*left)->null_value) {
    val2 = (*right)->val_real();
    if (current_thd->is_error()) return 0;
    if (!(*right)->null_value) {
      if (set_null) owner->null_value = false;
      if (val1 == val2 || fabs(val1 - val2) < precision) return 0;
      if (val1 < val2) return -1;
      return 1;
    }
  }
  if (set_null) owner->null_value = true;
  return -1;
}

int Arg_comparator::compare_int_signed() {
  const longlong val1 = (*left)->val_int();
  if (current_thd->is_error()) {
    if (set_null) owner->null_value = true;
    return 0;
  }
  if (!(*left)->null_value) {
    const longlong val2 = (*right)->val_int();
    if (current_thd->is_error()) {
      if (set_null) owner->null_value = true;
      return 0;
    }
    if (!(*right)->null_value) {
      if (set_null) owner->null_value = false;
      if (val1 < val2) return -1;
      if (val1 == val2) return 0;
      return 1;
    }
  }
  if (set_null) owner->null_value = true;
  return -1;
}

/**
  Compare arguments using numeric packed temporal representation.
*/
int Arg_comparator::compare_time_packed() {
  /*
    Note, we cannot do this:
    assert((*left)->data_type() == MYSQL_TYPE_TIME);
    assert((*right)->data_type() == MYSQL_TYPE_TIME);

    SELECT col_time_key FROM t1
    WHERE
      col_time_key != UTC_DATE()
    AND
      col_time_key = MAKEDATE(43, -2852);

    is rewritten to:

    SELECT col_time_key FROM t1
    WHERE
      MAKEDATE(43, -2852) != UTC_DATE()
    AND
      col_time_key = MAKEDATE(43, -2852);
  */
  const longlong val1 = (*left)->val_time_temporal();
  if (!(*left)->null_value) {
    const longlong val2 = (*right)->val_time_temporal();
    if (!(*right)->null_value) {
      if (set_null) owner->null_value = false;
      return val1 < val2 ? -1 : val1 > val2 ? 1 : 0;
    }
  }
  if (set_null) owner->null_value = true;
  return -1;
}

/**
  Compare values as BIGINT UNSIGNED.
*/

int Arg_comparator::compare_int_unsigned() {
  const ulonglong val1 = (*left)->val_int();
  if (current_thd->is_error()) {
    if (set_null) owner->null_value = true;
    return 0;
  }
  if (!(*left)->null_value) {
    const ulonglong val2 = (*right)->val_int();
    if (current_thd->is_error()) {
      if (set_null) owner->null_value = true;
      return 0;
    }
    if (!(*right)->null_value) {
      if (set_null) owner->null_value = false;
      if (val1 < val2) return -1;
      if (val1 == val2) return 0;
      return 1;
    }
  }
  if (set_null) owner->null_value = true;
  return -1;
}

/**
  Compare signed (*left) with unsigned (*B)
*/

int Arg_comparator::compare_int_signed_unsigned() {
  const longlong sval1 = (*left)->val_int();
  if (current_thd->is_error()) return 0;
  if (!(*left)->null_value) {
    const ulonglong uval2 = static_cast<ulonglong>((*right)->val_int());
    if (current_thd->is_error()) return 0;
    if (!(*right)->null_value) {
      if (set_null) owner->null_value = false;
      if (sval1 < 0 || (ulonglong)sval1 < uval2) return -1;
      if ((ulonglong)sval1 == uval2) return 0;
      return 1;
    }
  }
  if (set_null) owner->null_value = true;
  return -1;
}

/**
  Compare unsigned (*left) with signed (*B)
*/

int Arg_comparator::compare_int_unsigned_signed() {
  const ulonglong uval1 = static_cast<ulonglong>((*left)->val_int());
  if (current_thd->is_error()) return 0;
  if (!(*left)->null_value) {
    const longlong sval2 = (*right)->val_int();
    if (current_thd->is_error()) return 0;
    if (!(*right)->null_value) {
      if (set_null) owner->null_value = false;
      if (sval2 < 0) return 1;
      if (uval1 < (ulonglong)sval2) return -1;
      if (uval1 == (ulonglong)sval2) return 0;
      return 1;
    }
  }
  if (set_null) owner->null_value = true;
  return -1;
}

int Arg_comparator::compare_row() {
  int res = 0;
  bool was_null = false;
  (*left)->bring_value();
  (*right)->bring_value();

  if ((*left)->null_value || (*right)->null_value) {
    owner->null_value = true;
    return -1;
  }

  const uint n = (*left)->cols();
  for (uint i = 0; i < n; i++) {
    res = comparators[i].compare();
    /* Aggregate functions don't need special null handling. */
    if (owner->null_value && owner->type() == Item::FUNC_ITEM) {
      // NULL was compared
      switch (owner->functype()) {
        case Item_func::NE_FUNC:
          break;  // NE never aborts on NULL even if abort_on_null is set
        case Item_func::LT_FUNC:
        case Item_func::LE_FUNC:
        case Item_func::GT_FUNC:
        case Item_func::GE_FUNC:
          return -1;  // <, <=, > and >= always fail on NULL
        default:      // EQ_FUNC
          if (down_cast<Item_bool_func2 *>(owner)->ignore_unknown())
            return -1;  // We do not need correct NULL returning
      }
      was_null = true;
      owner->null_value = false;
      res = 0;  // continue comparison (maybe we will meet explicit difference)
    } else if (res)
      return res;
  }
  if (was_null) {
    /*
      There was NULL(s) in comparison in some parts, but there was no
      explicit difference in other parts, so we have to return NULL.
    */
    owner->null_value = true;
    return -1;
  }
  return 0;
}

/**
  Compare two argument items, or a pair of elements from two argument rows,
  for NULL values.

  @param a First item
  @param b Second item
  @param[out] result True if both items are NULL, false otherwise,
                     when return value is true.

  @returns true if at least one of the items is NULL
*/
static bool compare_pair_for_nulls(Item *a, Item *b, bool *result) {
  if (a->result_type() == ROW_RESULT) {
    a->bring_value();
    b->bring_value();
    /*
     Compare matching array elements. If only one element in a pair is NULL,
     result is false, otherwise move to next pair. If the values from all pairs
     are NULL, result is true.
    */
    bool have_null_items = false;
    for (uint i = 0; i < a->cols(); i++) {
      if (compare_pair_for_nulls(a->element_index(i), b->element_index(i),
                                 result)) {
        have_null_items = true;
        if (!*result) return true;
      }
    }
    return have_null_items;
  }
  const bool a_null = a->is_nullable() && a->is_null();
  if (current_thd->is_error()) return false;
  const bool b_null = b->is_nullable() && b->is_null();
  if (current_thd->is_error()) return false;
  if (a_null || b_null) {
    *result = a_null == b_null;
    return true;
  }
  *result = false;
  return false;
}

/**
  Compare NULL values for two arguments. When called, we know that at least
  one argument contains a NULL value.

  @returns true if both arguments are NULL, false if one argument is NULL
*/
bool Arg_comparator::compare_null_values() {
  bool result;
  (void)compare_pair_for_nulls(*left, *right, &result);
  if (current_thd->is_error()) return false;
  return result;
}

void Item_bool_func::set_created_by_in2exists() {
  m_created_by_in2exists = true;
  // When a condition is created by IN to EXISTS transformation,
  // it re-uses the expressions that are part of the query. As a
  // result we need to increment the reference count
  // for these expressions.
  WalkItem(this, enum_walk::PREFIX | enum_walk::SUBQUERY, [](Item *inner_item) {
    // Reference counting matters only for referenced items.
    if (inner_item->type() == REF_ITEM) {
      down_cast<Item_ref *>(inner_item)->ref_item()->increment_ref_count();
    }
    return false;
  });
}

const char *Item_bool_func::bool_transform_names[10] = {"is true",
                                                        "is false",
                                                        "is null",
                                                        "is not true",
                                                        "is not false",
                                                        "is not null",
                                                        "",
                                                        "",
                                                        "",
                                                        ""};

const Item::Bool_test Item_bool_func::bool_transform[10][8] = {
    {BOOL_IS_TRUE, BOOL_NOT_TRUE, BOOL_ALWAYS_FALSE, BOOL_NOT_TRUE,
     BOOL_IS_TRUE, BOOL_ALWAYS_TRUE, BOOL_IS_TRUE, BOOL_NOT_TRUE},
    {BOOL_IS_FALSE, BOOL_NOT_FALSE, BOOL_ALWAYS_FALSE, BOOL_NOT_FALSE,
     BOOL_IS_FALSE, BOOL_ALWAYS_TRUE, BOOL_IS_FALSE, BOOL_NOT_FALSE},
    {BOOL_IS_UNKNOWN, BOOL_NOT_UNKNOWN, BOOL_ALWAYS_FALSE, BOOL_NOT_UNKNOWN,
     BOOL_IS_UNKNOWN, BOOL_ALWAYS_TRUE, BOOL_IS_UNKNOWN, BOOL_NOT_UNKNOWN},
    {BOOL_NOT_TRUE, BOOL_IS_TRUE, BOOL_ALWAYS_FALSE, BOOL_IS_TRUE,
     BOOL_NOT_TRUE, BOOL_ALWAYS_TRUE, BOOL_NOT_TRUE, BOOL_IS_TRUE},
    {BOOL_NOT_FALSE, BOOL_IS_FALSE, BOOL_ALWAYS_FALSE, BOOL_IS_FALSE,
     BOOL_NOT_FALSE, BOOL_ALWAYS_TRUE, BOOL_NOT_FALSE, BOOL_IS_FALSE},
    {BOOL_NOT_UNKNOWN, BOOL_IS_UNKNOWN, BOOL_ALWAYS_FALSE, BOOL_IS_UNKNOWN,
     BOOL_NOT_UNKNOWN, BOOL_ALWAYS_TRUE, BOOL_NOT_UNKNOWN, BOOL_IS_UNKNOWN},
    {BOOL_IS_TRUE, BOOL_IS_FALSE, BOOL_IS_UNKNOWN, BOOL_NOT_TRUE,
     BOOL_NOT_FALSE, BOOL_NOT_UNKNOWN, BOOL_IDENTITY, BOOL_NEGATED},
    {BOOL_IS_FALSE, BOOL_IS_TRUE, BOOL_IS_UNKNOWN, BOOL_NOT_FALSE,
     BOOL_NOT_TRUE, BOOL_NOT_UNKNOWN, BOOL_NEGATED, BOOL_IDENTITY},
    {BOOL_ALWAYS_TRUE, BOOL_ALWAYS_FALSE, BOOL_ALWAYS_FALSE, BOOL_ALWAYS_FALSE,
     BOOL_ALWAYS_TRUE, BOOL_ALWAYS_TRUE, BOOL_ALWAYS_TRUE, BOOL_ALWAYS_FALSE},
    {BOOL_ALWAYS_FALSE, BOOL_ALWAYS_TRUE, BOOL_ALWAYS_FALSE, BOOL_ALWAYS_TRUE,
     BOOL_ALWAYS_FALSE, BOOL_ALWAYS_TRUE, BOOL_ALWAYS_FALSE, BOOL_ALWAYS_TRUE}};

bool Item_func_truth::resolve_type(THD *thd) {
  set_nullable(false);
  null_value = false;
  return Item_bool_func::resolve_type(thd);
}

void Item_func_truth::print(const THD *thd, String *str,
                            enum_query_type query_type) const {
  str->append('(');
  args[0]->print(thd, str, query_type);
  str->append(STRING_WITH_LEN(" "));
  str->append(func_name());
  assert(func_name()[0]);
  str->append(')');
}

longlong Item_func_truth::val_int() {
  const bool val = args[0]->val_bool();
  if (args[0]->null_value) {
    /*
      NULL val IS {TRUE, FALSE} --> FALSE
      NULL val IS NOT {TRUE, FALSE} --> TRUE
    */
    switch (truth_test) {
      case BOOL_IS_TRUE:
      case BOOL_IS_FALSE:
        return false;
      case BOOL_NOT_TRUE:
      case BOOL_NOT_FALSE:
        return true;
      default:
        assert(false);
        return false;
    }
  }

  switch (truth_test) {
    case BOOL_IS_TRUE:
    case BOOL_NOT_FALSE:
      return val;
    case BOOL_IS_FALSE:
    case BOOL_NOT_TRUE:
      return !val;
    default:
      assert(false);
      return false;
  }
}

bool Item_in_optimizer::fix_left(THD *thd) {
  Item *left = down_cast<Item_in_subselect *>(args[0])->left_expr;
  /*
    Because get_cache() depends on type of left arg, if this arg is a PS param
    we must decide of its type now. We cannot wait until we know the type of
    the subquery's SELECT list.
    @todo: This may actually be changed later, INSPECT.
  */
  if (left->propagate_type(thd, MYSQL_TYPE_VARCHAR)) return true;

  assert(cache == nullptr);
  cache = Item_cache::get_cache(left);
  if (cache == nullptr) return true;

  cache->setup(left);
  used_tables_cache = left->used_tables();

  /*
    Propagate used tables information to the cache objects.
    Since the cache objects will be used in synthesized predicates that are
    added to the subquery's query expression, we need to add extra references
    to them, since on removal these will be decremented twice.
  */
  if (cache->cols() == 1) {
    left->real_item()->increment_ref_count();
    cache->set_used_tables(used_tables_cache);
  } else {
    uint n = cache->cols();
    for (uint i = 0; i < n; i++) {
      Item_cache *const element =
          down_cast<Item_cache *>(cache->element_index(i));
      element->set_used_tables(left->element_index(i)->used_tables());
      element->real_item()->increment_ref_count();
    }
  }
  not_null_tables_cache = left->not_null_tables();
  add_accum_properties(left);
  if (const_item()) cache->store(left);

  return false;
}

bool Item_in_optimizer::fix_fields(THD *, Item **) {
  assert(!fixed);
  Item_in_subselect *subqpred = down_cast<Item_in_subselect *>(args[0]);

  assert(subqpred->fixed);
  if (subqpred->is_nullable()) set_nullable(true);
  add_accum_properties(subqpred);
  used_tables_cache |= subqpred->used_tables();
  not_null_tables_cache |= subqpred->not_null_tables();

  /*
    not_null_tables_cache is to hold any table which, if its row is NULL,
    causes the result of the complete Item to be NULL.
    This can never be guaranteed, as the complete Item will return FALSE if
    the subquery's result is empty.
    But, if the Item's owner previously called top_level_item(), a FALSE
    result is equivalent to a NULL result from the owner's POV.
    A NULL value in the left argument will surely lead to a NULL or FALSE
    result for the naked IN. If the complete item is:
    plain IN, or IN IS TRUE, then it will return NULL or FALSE. Otherwise it
    won't and we must remove the left argument from not_null_tables().
    Right argument doesn't need to be handled, as
    Item_subselect::not_null_tables() is always 0.
  */
  if (subqpred->abort_on_null && subqpred->value_transform == BOOL_IS_TRUE) {
  } else {
    not_null_tables_cache &= ~subqpred->left_expr->not_null_tables();
  }
  fixed = true;
  return false;
}

void Item_in_optimizer::fix_after_pullout(Query_block *parent_query_block,
                                          Query_block *removed_query_block) {
  used_tables_cache = get_initial_pseudo_tables();
  not_null_tables_cache = 0;

  args[0]->fix_after_pullout(parent_query_block, removed_query_block);

  used_tables_cache |= args[0]->used_tables();
  not_null_tables_cache |= args[0]->not_null_tables();
}

bool Item_in_optimizer::split_sum_func(THD *thd, Ref_item_array ref_item_array,
                                       mem_root_deque<Item *> *fields) {
  if (args[0]->split_sum_func2(thd, ref_item_array, fields, args, true)) {
    return true;
  }
  Item **left = &down_cast<Item_in_subselect *>(args[0])->left_expr;
  if ((*left)->split_sum_func2(thd, ref_item_array, fields, left, true)) {
    return true;
  }
  return false;
}

void Item_in_optimizer::print(const THD *thd, String *str,
                              enum_query_type query_type) const {
  str->append(func_name());
  str->append('(');
  down_cast<Item_in_subselect *>(args[0])->left_expr->print(thd, str,
                                                            query_type);
  str->append(',');
  print_args(thd, str, 0, query_type);
  str->append(')');
}

/**
   The implementation of optimized @<outer expression@> [NOT] IN @<subquery@>
   predicates. It applies to predicates which have gone through the IN->EXISTS
   transformation in in_to_exists_transformer functions; not to subquery
   materialization (which has no triggered conditions).

   The implementation works as follows.
   For the current value of the outer expression

   - If it contains only NULL values, the original (before rewrite by the
     Item_in_subselect rewrite methods) inner subquery is non-correlated and
     was previously executed, there is no need to re-execute it, and the
     previous return value is returned.

   - If it contains NULL values, check if there is a partial match for the
     inner query block by evaluating it. For clarity we repeat here the
     transformation previously performed on the sub-query. The expression

     <tt>
     ( oc_1, ..., oc_n )
     @<in predicate@>
     ( SELECT ic_1, ..., ic_n
       FROM @<table@>
       WHERE @<inner where@>
     )
     </tt>

     was transformed into

     <tt>
     ( oc_1, ..., oc_n )
     \@in predicate@>
     ( SELECT ic_1, ..., ic_n
       FROM @<table@>
       WHERE @<inner where@> AND ... ( ic_k = oc_k OR ic_k IS NULL )
       HAVING ... NOT ic_k IS NULL
     )
     </tt>

     The evaluation will now proceed according to special rules set up
     elsewhere. These rules include:

     - The HAVING NOT @<inner column@> IS NULL conditions added by the
       aforementioned rewrite methods will detect whether they evaluated (and
       rejected) a NULL value and if so, will cause the subquery to evaluate
       to NULL.

     - The added WHERE and HAVING conditions are present only for those inner
       columns that correspond to outer column that are not NULL at the moment.

     - If there is an eligible index for executing the subquery, the special
       access method "Full scan on NULL key" is employed which ensures that
       the inner query will detect if there are NULL values resulting from the
       inner query. This access method will quietly resort to table scan if it
       needs to find NULL values as well.

     - Under these conditions, the sub-query need only be evaluated in order to
       find out whether it produced any rows.

       - If it did, we know that there was a partial match since there are
         NULL values in the outer row expression.

       - If it did not, the result is FALSE or UNKNOWN. If at least one of the
         HAVING sub-predicates rejected a NULL value corresponding to an outer
         non-NULL, and hence the inner query block returns UNKNOWN upon
         evaluation, there was a partial match and the result is UNKNOWN.

   - If it contains no NULL values, the call is forwarded to the inner query
     block.

     @see Item_in_subselect::val_bool_naked()
     @see Item_is_not_null_test::val_int()
 */

longlong Item_in_optimizer::val_int() {
  assert(fixed);
  Item_in_subselect *const subqpred = down_cast<Item_in_subselect *>(args[0]);

  cache->store(subqpred->left_expr);
  cache->cache_value();

  if (cache->null_value) {
    /*
      We're evaluating
      "<outer_value_list> [NOT] IN (SELECT <inner_value_list>...)"
      where one or more of the outer values is NULL.
    */
    if (subqpred->abort_on_null) {
      /*
        We're evaluating a top level item, e.g.
        "<outer_value_list> IN (SELECT <inner_value_list>...)",
        and in this case a NULL value in the outer_value_list means
        that the result shall be NULL/FALSE (makes no difference for
        top level items). The cached value is NULL, so just return
        NULL.
      */
      null_value = true;
    } else {
      /*
        We're evaluating an item where a NULL value in either the
        outer or inner value list does not automatically mean that we
        can return NULL/FALSE. An example of such a query is
        "<outer_value_list> NOT IN (SELECT <inner_value_list>...)"
        where <*_list> may be a scalar or a ROW.
        The result when there is at least one NULL value in <outer_value_list>
        is: NULL if the SELECT evaluated over the non-NULL values produces at
        least one row, FALSE otherwise
      */
      bool all_left_cols_null = true;
      const uint ncols = cache->cols();

      /*
        Turn off the predicates that are based on column compares for
        which the left part is currently NULL
      */
      for (uint i = 0; i < ncols; i++) {
        if (cache->element_index(i)->null_value)
          subqpred->set_cond_guard_var(i, false);
        else
          all_left_cols_null = false;
      }

      if (all_left_cols_null && result_for_null_param != UNKNOWN &&
          !subqpred->dependent_before_in2exists()) {
        /*
           This subquery was originally not correlated. The IN->EXISTS
           transformation may have made it correlated, but only to the left
           expression. All values in the left expression are NULL, and we have
           already evaluated the subquery for all NULL values: return the same
           result we did last time without evaluating the subquery.
        */
        null_value = result_for_null_param;
      } else {
        /* The subquery has to be evaluated */
        (void)subqpred->val_bool_naked();
        if (!subqpred->m_value)
          null_value = subqpred->null_value;
        else
          null_value = true;
        if (all_left_cols_null) result_for_null_param = null_value;
      }

      /* Turn all predicates back on */
      for (uint i = 0; i < ncols; i++) subqpred->set_cond_guard_var(i, true);
    }
    cache->store(subqpred->left_expr);
    return subqpred->translate(null_value, false);
  }
  const bool result = subqpred->val_bool_naked();
  null_value = subqpred->null_value;
  cache->store(subqpred->left_expr);
  return subqpred->translate(null_value, result);
}

void Item_in_optimizer::cleanup() {
  Item_bool_func::cleanup();
  result_for_null_param = UNKNOWN;
  // Restore the changes done to the cached object during execution.
  // E.g. constant expressions in "left_expr" might have been
  // replaced with cached items (cache_const_expr_transformer())
  // which live only for one execution and these cached items
  // replace the original items in "cache" during execution.
  if (cache != nullptr) {
    cache->store(down_cast<Item_in_subselect *>(args[0])->left_expr);
  }
}

bool Item_in_optimizer::is_null() {
  val_int();
  return null_value;
}

void Item_in_optimizer::update_used_tables() {
  Item_func::update_used_tables();

  // See explanation for this logic in Item_in_optimizer::fix_fields
  Item_in_subselect *subqpred = down_cast<Item_in_subselect *>(args[0]);
  if (subqpred->abort_on_null && subqpred->value_transform == BOOL_IS_TRUE) {
  } else {
    not_null_tables_cache &= subqpred->left_expr->not_null_tables();
  }
}

longlong Item_func_eq::val_int() {
  assert(fixed);
  const int value = cmp.compare();
  return value == 0 ? 1 : 0;
}

/** Same as Item_func_eq, but NULL = NULL. */

bool Item_func_equal::resolve_type(THD *thd) {
  if (Item_bool_func2::resolve_type(thd)) return true;
  uint nvector_args = num_vector_args();
  if (nvector_args != 0 && nvector_args != arg_count) {
    my_error(ER_WRONG_ARGUMENTS, MYF(0), func_name());
    return true;
  }
  set_nullable(false);
  null_value = false;
  return false;
}

longlong Item_func_equal::val_int() {
  assert(fixed);
  // Perform regular equality check first:
  const int value = cmp.compare();
  if (current_thd->is_error()) return 0;
  // If comparison is not NULL, we have a result:
  if (!null_value) return value == 0 ? 1 : 0;
  null_value = false;
  // Check NULL values for both arguments
  return longlong(cmp.compare_null_values());
}

float Item_func_ne::get_filtering_effect(THD *thd, table_map filter_for_table,
                                         table_map read_tables,
                                         const MY_BITMAP *fields_to_ignore,
                                         double rows_in_table) {
  const Item_field *fld = contributes_to_filter(
      thd, read_tables, filter_for_table, fields_to_ignore);
  if (!fld) return COND_FILTER_ALLPASS;

  // Find selectivity from histogram or index.
  const double selectivity = [&]() {
    // The index calculation might be useful for the original optimizer too,
    // but we are loth to change existing plans and therefore restrict
    // it to Hypergraph.
    const auto index_selectivity = [&]() {
      const double reverse_selectivity =
          IndexSelectivityOfUnknownValue(*fld->field);

      if (reverse_selectivity == kUndefinedSelectivity) {
        return kUndefinedSelectivity;
      } else {
        // Even if all rows have the same value for 'fld', we should avoid
        // returning a selectivity estimate of zero, as that can give
        // a distorted view of the cost of a plan if the estimate should
        // be wrong (even by a small margin).
        return std::max(1.0 - reverse_selectivity,
                        Item_func_ne::kMinSelectivityForUnknownValue);
      }
    };

    if (!thd->lex->using_hypergraph_optimizer()) {
      return get_histogram_selectivity(
          thd, *fld->field, histograms::enum_operator::NOT_EQUALS_TO, *this);

    } else if (args[0]->const_item() || args[1]->const_item() ||
               fld->field->key_start.is_clear_all()) {
      // We prefer histograms over indexes if:
      // 1) We are comparing a field to a constant, since histograms will
      //    give the frequency of that constant value.
      // 2) If no index starts with fld->field, as index estimates will then
      //    be less accurate, since we do not know if that field is correlated
      //    with the preceding fields of the index.
      const double histogram_selectivity = get_histogram_selectivity(
          thd, *fld->field, histograms::enum_operator::NOT_EQUALS_TO, *this);

      return histogram_selectivity == kUndefinedSelectivity
                 ? index_selectivity()
                 : histogram_selectivity;
    } else {
      const double idx_sel = index_selectivity();

      return idx_sel == kUndefinedSelectivity
                 ? get_histogram_selectivity(
                       thd, *fld->field,
                       histograms::enum_operator::NOT_EQUALS_TO, *this)
                 : idx_sel;
    }
  }();

  return selectivity == kUndefinedSelectivity
             ? 1.0 - fld->get_cond_filter_default_probability(
                         rows_in_table, COND_FILTER_EQUALITY)
             : selectivity;
}

longlong Item_func_ne::val_int() {
  assert(fixed);
  const int value = cmp.compare();
  return value != 0 && !null_value ? 1 : 0;
}

/**
   Compute selectivity for field=expression and field<=>expression, where
   'expression' is not Item_null.
   @param thd The current thread.
   @param equal The '=' or '<=>' term.
   @param field The field we compare with 'expression'.
   @param rows_in_table Number of rows in the table of 'field'.
   @returns Selectivity estimate.
 */
static double GetEqualSelectivity(THD *thd, Item_eq_base *equal,
                                  const Item_field &field,
                                  double rows_in_table) {
  assert(equal->argument_count() == 2);
  assert(std::none_of(
      equal->arguments(), equal->arguments() + equal->argument_count(),
      [](const Item *item) { return item->type() == Item::NULL_ITEM; }));

  const double selectivity = [&]() {
    // The index calculation might be useful for the original optimizer too,
    // but we are loth to change existing plans and therefore restrict
    // it to Hypergraph.
    if (!thd->lex->using_hypergraph_optimizer()) {
      return get_histogram_selectivity(
          thd, *field.field, histograms::enum_operator::EQUALS_TO, *equal);

    } else if (equal->arguments()[0]->const_item() ||
               equal->arguments()[1]->const_item() ||
               field.field->key_start.is_clear_all()) {
      // We prefer histograms over indexes if:
      // 1) We are comparing a field to a constant, since histograms will
      //    give the frequency of that constant value.
      // 2) If no index starts with field.field, as index estimates will then
      //    be less accurate, since we do not know if that field is correlated
      //    with the preceding fields of the index.
      const double histogram_selectivity = get_histogram_selectivity(
          thd, *field.field, histograms::enum_operator::EQUALS_TO, *equal);

      return histogram_selectivity == kUndefinedSelectivity
                 ? IndexSelectivityOfUnknownValue(*field.field)
                 : histogram_selectivity;

    } else {
      const double index_selectivity =
          IndexSelectivityOfUnknownValue(*field.field);

      return index_selectivity == kUndefinedSelectivity
                 ? get_histogram_selectivity(
                       thd, *field.field, histograms::enum_operator::EQUALS_TO,
                       *equal)
                 : index_selectivity;
    }
  }();

  return selectivity == kUndefinedSelectivity
             ? field.get_cond_filter_default_probability(rows_in_table,
                                                         COND_FILTER_EQUALITY)
             : selectivity;
}

float Item_func_equal::get_filtering_effect(THD *thd,
                                            table_map filter_for_table,
                                            table_map read_tables,
                                            const MY_BITMAP *fields_to_ignore,
                                            double rows_in_table) {
  const Item_field *fld = contributes_to_filter(
      thd, read_tables, filter_for_table, fields_to_ignore);
  if (!fld) return COND_FILTER_ALLPASS;

  for (int i : {0, 1}) {
    if (arguments()[i]->type() == NULL_ITEM) {
      if (!fld->field->is_nullable()) {
        return 0.0;
      }

      const Item_func *is_null =
          new (thd->mem_root) Item_func_isnull(arguments()[(i + 1) % 2]);

      const double histogram_selectivity = get_histogram_selectivity(
          thd, *fld->field, histograms::enum_operator::IS_NULL, *is_null);

      if (histogram_selectivity >= 0.0) {
        return histogram_selectivity;
      } else {
        return fld->get_cond_filter_default_probability(rows_in_table,
                                                        COND_FILTER_EQUALITY);
      }
    }
  }

  return GetEqualSelectivity(thd, this, *fld, rows_in_table);
}

float Item_func_comparison::get_filtering_effect(
    THD *thd, table_map filter_for_table, table_map read_tables,
    const MY_BITMAP *fields_to_ignore, double rows_in_table) {
  // For comparing MATCH(...), generally reuse the same selectivity as for
  // MATCH(...), which is generally COND_FILTER_BETWEEN. This is wrong
  // in a number of cases (the equivalence only holds for MATCH(...) > 0
  // or 0 < MATCH(...)) but usually less wrong than the default down below,
  // which is COND_FILTER_ALLPASS (1.0).
  //
  // Ideally, of course, we should have had a real estimation of MATCH(...)
  // selectivity in the form of some sort of histogram, and then read out
  // that histogram here. However, that is a larger job.
  if (is_function_of_type(args[0], Item_func::FT_FUNC) &&
      args[1]->const_item()) {
    return args[0]->get_filtering_effect(thd, filter_for_table, read_tables,
                                         fields_to_ignore, rows_in_table);
  }
  if (is_function_of_type(args[1], Item_func::FT_FUNC) &&
      args[0]->const_item()) {
    return args[1]->get_filtering_effect(thd, filter_for_table, read_tables,
                                         fields_to_ignore, rows_in_table);
  }

  const Item_field *fld = contributes_to_filter(
      thd, read_tables, filter_for_table, fields_to_ignore);
  if (!fld) return COND_FILTER_ALLPASS;

  const histograms::enum_operator comp_op = [&]() {
    switch (functype()) {
      case GT_FUNC:
        return histograms::enum_operator::GREATER_THAN;

      case LT_FUNC:
        return histograms::enum_operator::LESS_THAN;

      case GE_FUNC:
        return histograms::enum_operator::GREATER_THAN_OR_EQUAL;

      case LE_FUNC:
        return histograms::enum_operator::LESS_THAN_OR_EQUAL;

      default:
        assert(false);
        return histograms::enum_operator::GREATER_THAN;
    };
  }();

  const double selectivity =
      get_histogram_selectivity(thd, *fld->field, comp_op, *this);

  return selectivity == kUndefinedSelectivity
             ? fld->get_cond_filter_default_probability(rows_in_table,
                                                        COND_FILTER_INEQUALITY)
             : selectivity;
}

longlong Item_func_ge::val_int() {
  assert(fixed);
  const int value = cmp.compare();
  return value >= 0 ? 1 : 0;
}

longlong Item_func_gt::val_int() {
  assert(fixed);
  const int value = cmp.compare();
  return value > 0 ? 1 : 0;
}

longlong Item_func_le::val_int() {
  assert(fixed);
  const int value = cmp.compare();
  return value <= 0 && !null_value ? 1 : 0;
}

longlong Item_func_reject_if::val_int() {
  const longlong result = args[0]->val_int();
  if (result == 1) {
    my_error(ER_SUBQUERY_NO_1_ROW, MYF(0));
  }
  return !result;
}
float Item_func_reject_if::get_filtering_effect(
    THD *thd, table_map filter_for_table, table_map read_tables,
    const MY_BITMAP *fields_to_ignore, double rows_in_table) {
  return args[0]->get_filtering_effect(thd, filter_for_table, read_tables,
                                       fields_to_ignore, rows_in_table);
}

longlong Item_func_lt::val_int() {
  assert(fixed);
  const int value = cmp.compare();
  return value < 0 && !null_value ? 1 : 0;
}

longlong Item_func_strcmp::val_int() {
  assert(fixed);
  const CHARSET_INFO *cs = cmp.cmp_collation.collation;
  String *a = eval_string_arg(cs, args[0], &cmp.value1);
  if (a == nullptr) {
    if (current_thd->is_error()) return error_int();
    null_value = true;
    return 0;
  }

  String *b = eval_string_arg(cs, args[1], &cmp.value2);
  if (b == nullptr) {
    if (current_thd->is_error()) return error_int();
    null_value = true;
    return 0;
  }
  const int value = sortcmp(a, b, cs);
  null_value = false;
  return value == 0 ? 0 : value < 0 ? -1 : 1;
}

bool Item_func_opt_neg::eq_specific(const Item *item) const {
  if (negated != down_cast<const Item_func_opt_neg *>(item)->negated)
    return false;
  return true;
}

bool Item_func_interval::do_itemize(Parse_context *pc, Item **res) {
  if (skip_itemize(res)) return false;
  if (row == nullptr ||  // OOM in constructor
      super::do_itemize(pc, res))
    return true;
  assert(row == args[0]);  // row->itemize() is not needed
  return false;
}

Item_row *Item_func_interval::alloc_row(const POS &pos, MEM_ROOT *mem_root,
                                        Item *expr1, Item *expr2,
                                        PT_item_list *opt_expr_list) {
  mem_root_deque<Item *> *list =
      opt_expr_list ? &opt_expr_list->value
                    : new (mem_root) mem_root_deque<Item *>(mem_root);
  if (list == nullptr) return nullptr;
  list->push_front(expr2);
  Item_row *tmprow = new (mem_root) Item_row(pos, expr1, *list);
  return tmprow;
}

bool Item_func_interval::resolve_type(THD *thd) {
  const uint rows = row->cols();

  // The number of columns in one argument is limited to one
  for (uint i = 0; i < rows; i++) {
    if (row->element_index(i)->check_cols(1)) return true;
    if (row->element_index(i)->propagate_type(thd, MYSQL_TYPE_LONGLONG))
      return true;
  }

  use_decimal_comparison =
      ((row->element_index(0)->result_type() == DECIMAL_RESULT) ||
       (row->element_index(0)->result_type() == INT_RESULT));
  if (rows > 8) {
    bool not_null_consts = true;

    for (uint i = 1; not_null_consts && i < rows; i++) {
      Item *el = row->element_index(i);
      not_null_consts = el->const_item() && !el->is_null();
    }

    if (not_null_consts) {
      intervals = static_cast<interval_range *>(
          (*THR_MALLOC)->Alloc(sizeof(interval_range) * (rows - 1)));
      if (intervals == nullptr) return true;
      if (use_decimal_comparison) {
        for (uint i = 1; i < rows; i++) {
          Item *el = row->element_index(i);
          interval_range *range = intervals + (i - 1);
          if ((el->result_type() == DECIMAL_RESULT) ||
              (el->result_type() == INT_RESULT)) {
            range->type = DECIMAL_RESULT;
            range->dec.init();
            my_decimal *dec = el->val_decimal(&range->dec);
            if (dec != &range->dec) {
              range->dec = *dec;
            }
          } else {
            range->type = REAL_RESULT;
            range->dbl = el->val_real();
          }
        }
      } else {
        for (uint i = 1; i < rows; i++) {
          intervals[i - 1].dbl = row->element_index(i)->val_real();
        }
      }
    }
  }
  set_nullable(false);
  max_length = 2;
  used_tables_cache |= row->used_tables();
  not_null_tables_cache = row->not_null_tables();
  add_accum_properties(row);

  return false;
}

void Item_func_interval::update_used_tables() {
  Item_func::update_used_tables();
  not_null_tables_cache = row->not_null_tables();
}

/**
  Appends function name and arguments list to the String str.

  @note
    Arguments of INTERVAL function are stored in "Item_row" object. Function
    print_args calls print function of "Item_row" class. Item_row::print
    function append "(", "argument_list" and ")" to String str.

  @param thd               Thread handle
  @param [in,out] str      String to which the func_name and argument list
                                should be appended.
  @param query_type        Query type
*/

void Item_func_interval::print(const THD *thd, String *str,
                               enum_query_type query_type) const {
  str->append(func_name());
  print_args(thd, str, 0, query_type);
}

/**
  Execute Item_func_interval().

  @note
    If we are doing a decimal comparison, we are evaluating the first
    item twice.

  @return
    - -1 if null value,
    - 0 if lower than lowest
    - 1 - arg_count-1 if between args[n] and args[n+1]
    - arg_count if higher than biggest argument
*/

longlong Item_func_interval::val_int() {
  assert(fixed);
  double value;
  my_decimal dec_buf, *dec = nullptr;
  uint i;

  if (use_decimal_comparison) {
    dec = row->element_index(0)->val_decimal(&dec_buf);
    if (row->element_index(0)->null_value) return -1;
    my_decimal2double(E_DEC_FATAL_ERROR, dec, &value);
  } else {
    value = row->element_index(0)->val_real();
    if (row->element_index(0)->null_value) return -1;
  }

  if (intervals) {  // Use binary search to find interval
    uint start, end;
    start = 0;
    end = row->cols() - 2;
    while (start != end) {
      const uint mid = (start + end + 1) / 2;
      interval_range *range = intervals + mid;
      bool cmp_result;
      /*
        The values in the range interval may have different types,
        Only do a decimal comparison of the first argument is a decimal
        and we are comparing against a decimal
      */
      if (dec && range->type == DECIMAL_RESULT)
        cmp_result = my_decimal_cmp(&range->dec, dec) <= 0;
      else
        cmp_result = (range->dbl <= value);
      if (cmp_result)
        start = mid;
      else
        end = mid - 1;
    }
    interval_range *range = intervals + start;
    return ((dec && range->type == DECIMAL_RESULT)
                ? my_decimal_cmp(dec, &range->dec) < 0
                : value < range->dbl)
               ? 0
               : start + 1;
  }

  for (i = 1; i < row->cols(); i++) {
    Item *el = row->element_index(i);
    if (use_decimal_comparison && ((el->result_type() == DECIMAL_RESULT) ||
                                   (el->result_type() == INT_RESULT))) {
      my_decimal e_dec_buf, *e_dec = el->val_decimal(&e_dec_buf);
      /* Skip NULL ranges. */
      if (el->null_value) continue;
      if (my_decimal_cmp(e_dec, dec) > 0) return i - 1;
    } else {
      const double val = el->val_real();
      /* Skip NULL ranges. */
      if (el->null_value) continue;
      if (val > value) return i - 1;
    }
  }
  return i - 1;
}

/**
  Perform context analysis of a BETWEEN item tree.

    This function performs context analysis (name resolution) and calculates
    various attributes of the item tree with Item_func_between as its root.
    The function saves in ref the pointer to the item or to a newly created
    item that is considered as a replacement for the original one.

  @param thd     reference to the global context of the query thread
  @param ref     pointer to Item* variable where pointer to resulting "fixed"
                 item is to be assigned

  @note
    Let T0(e)/T1(e) be the value of not_null_tables(e) when e is used on
    a predicate/function level. Then it's easy to show that:
    @verbatim
      T0(e BETWEEN e1 AND e2)     = union(T1(e),T1(e1),T1(e2))
      T1(e BETWEEN e1 AND e2)     = union(T1(e),intersection(T1(e1),T1(e2)))
      T0(e NOT BETWEEN e1 AND e2) = union(T1(e),intersection(T1(e1),T1(e2)))
      T1(e NOT BETWEEN e1 AND e2) = union(T1(e),intersection(T1(e1),T1(e2)))
    @endverbatim

  @retval
    0   ok
  @retval
    1   got error
*/

bool Item_func_between::fix_fields(THD *thd, Item **ref) {
  if (Item_func_opt_neg::fix_fields(thd, ref)) return true;

  thd->lex->current_query_block()->between_count++;

  update_not_null_tables();

  // if 'high' and 'low' are same, convert this to a _eq function
  if (negated || !args[1]->const_item() || !args[2]->const_item()) {
    return false;
  }
  // Ensure that string values are compared using BETWEEN's effective collation
  if (args[1]->result_type() == STRING_RESULT &&
      args[2]->result_type() == STRING_RESULT) {
    if (!args[1]->eq_by_collation(args[2], args[0]->collation.collation))
      return false;
  } else {
    if (!args[1]->eq(args[2])) return false;
  }
  Item *item = new (thd->mem_root) Item_func_eq(args[0], args[1]);
  if (item == nullptr) return true;
  item->item_name = item_name;
  if (item->fix_fields(thd, ref)) return true;
  *ref = item;

  return false;
}

void Item_func_between::fix_after_pullout(Query_block *parent_query_block,
                                          Query_block *removed_query_block) {
  Item_func_opt_neg::fix_after_pullout(parent_query_block, removed_query_block);
  update_not_null_tables();
}

bool Item_func_between::resolve_type(THD *thd) {
  max_length = 1;
  int datetime_items_found = 0;
  int time_items_found = 0;
  compare_as_dates_with_strings = false;
  compare_as_temporal_times = compare_as_temporal_dates = false;

  // All three arguments are needed for type resolving
  assert(args[0] && args[1] && args[2]);

  if (Item_func_opt_neg::resolve_type(thd)) return true;

  cmp_type = agg_cmp_type(args, 3);

  if (cmp_type == STRING_RESULT &&
      agg_arg_charsets_for_comparison(cmp_collation, args, 3))
    return true;

  /*
    See comments for the code block doing similar checks in
    Item_bool_func2::resolve_type().
  */
  if (reject_geometry_args()) return true;
  if (reject_vector_args()) return true;

  /*
    JSON values will be compared as strings, and not with the JSON
    comparator as one might expect. Raise a warning if one of the
    arguments is JSON.
  */
  unsupported_json_comparison(arg_count, args,
                              "comparison of JSON in the BETWEEN operator");

  /*
    Detect the comparison of DATE/DATETIME items.
    At least one of items should be a DATE/DATETIME item and other items
    should return the STRING result.
  */
  if (cmp_type == STRING_RESULT) {
    for (int i = 0; i < 3; i++) {
      if (args[i]->is_temporal_with_date())
        datetime_items_found++;
      else if (args[i]->data_type() == MYSQL_TYPE_TIME)
        time_items_found++;
    }
  }

  if (datetime_items_found + time_items_found == 3) {
    if (time_items_found == 3) {
      // All items are TIME
      cmp_type = INT_RESULT;
      compare_as_temporal_times = true;
    } else {
      /*
        There is at least one DATE or DATETIME item,
        all other items are DATE, DATETIME or TIME.
      */
      cmp_type = INT_RESULT;
      compare_as_temporal_dates = true;
    }
  } else if (datetime_items_found > 0) {
    /*
       There is at least one DATE or DATETIME item.
       All other items are DATE, DATETIME or strings.
    */
    compare_as_dates_with_strings = true;
    ge_cmp.set_datetime_cmp_func(this, args, args + 1);
    le_cmp.set_datetime_cmp_func(this, args, args + 2);
  } else if (args[0]->real_item()->type() == FIELD_ITEM &&
             thd->lex->sql_command != SQLCOM_CREATE_VIEW &&
             thd->lex->sql_command != SQLCOM_SHOW_CREATE) {
    Item_field *field_item = (Item_field *)(args[0]->real_item());
    if (field_item->field->can_be_compared_as_longlong()) {
      /*
        The following can't be recoded with || as convert_constant_item
        changes the argument
      */
      bool cvt_arg1, cvt_arg2;
      if (convert_constant_item(thd, field_item, &args[1], &cvt_arg1))
        return true;
      if (convert_constant_item(thd, field_item, &args[2], &cvt_arg2))
        return true;

      if (args[0]->is_temporal()) {  // special handling of date/time etc.
        if (cvt_arg1 || cvt_arg2) cmp_type = INT_RESULT;
      } else {
        if (cvt_arg1 && cvt_arg2) cmp_type = INT_RESULT;
      }

      if (args[0]->is_temporal() && args[1]->is_temporal() &&
          args[2]->is_temporal() && args[0]->data_type() != MYSQL_TYPE_YEAR &&
          args[1]->data_type() != MYSQL_TYPE_YEAR &&
          args[2]->data_type() != MYSQL_TYPE_YEAR) {
        /*
          An expression:
            time_or_datetime_field
              BETWEEN const_number_or_time_or_datetime_expr1
              AND     const_number_or_time_or_datetime_expr2
          was rewritten to:
            time_field
              BETWEEN Item_time_with_ref1
              AND     Item_time_with_ref2
          or
            datetime_field
              BETWEEN Item_datetime_with_ref1
              AND     Item_datetime_with_ref2
        */
        if (field_item->data_type() == MYSQL_TYPE_TIME)
          compare_as_temporal_times = true;
        else if (field_item->is_temporal_with_date())
          compare_as_temporal_dates = true;
      }
    }
  }

  return false;
}

void Item_func_between::update_used_tables() {
  Item_func::update_used_tables();
  update_not_null_tables();
}

float Item_func_between::get_filtering_effect(THD *thd,
                                              table_map filter_for_table,
                                              table_map read_tables,
                                              const MY_BITMAP *fields_to_ignore,
                                              double rows_in_table) {
  const Item_field *fld = contributes_to_filter(
      thd, read_tables, filter_for_table, fields_to_ignore);
  if (!fld) return COND_FILTER_ALLPASS;

  const histograms::enum_operator op =
      (negated ? histograms::enum_operator::NOT_BETWEEN
               : histograms::enum_operator::BETWEEN);

  const double selectivity =
      get_histogram_selectivity(thd, *fld->field, op, *this);

  if (selectivity == kUndefinedSelectivity) {
    const float filter = fld->get_cond_filter_default_probability(
        rows_in_table, COND_FILTER_BETWEEN);

    return negated ? 1.0f - filter : filter;
  } else {
    return selectivity;
  }
}

/**
  A helper function for Item_func_between::val_int() to avoid
  over/underflow when comparing large values.

  @tparam LLorULL ulonglong or longlong

  @param  compare_as_temporal_dates copy of Item_func_between member variable
  @param  compare_as_temporal_times copy of Item_func_between member variable
  @param  negated                   copy of Item_func_between member variable
  @param  args                      copy of Item_func_between member variable
  @param [out] null_value           set to true if result is not true/false

  @retval true if: args[1] <= args[0] <= args[2]
 */
template <typename LLorULL>
static inline longlong compare_between_int_result(
    bool compare_as_temporal_dates, bool compare_as_temporal_times,
    bool negated, Item **args, bool *null_value) {
  {
    LLorULL a, b, value;
    value = compare_as_temporal_times   ? args[0]->val_time_temporal()
            : compare_as_temporal_dates ? args[0]->val_date_temporal()
                                        : args[0]->val_int();
    if ((*null_value = args[0]->null_value)) return 0; /* purecov: inspected */
    if (compare_as_temporal_times) {
      a = args[1]->val_time_temporal();
      b = args[2]->val_time_temporal();
    } else if (compare_as_temporal_dates) {
      a = args[1]->val_date_temporal();
      b = args[2]->val_date_temporal();
    } else {
      a = args[1]->val_int();
      b = args[2]->val_int();
    }

    if (std::is_unsigned<LLorULL>::value) {
      /*
        Comparing as unsigned.
        value BETWEEN <some negative number> AND <some number>
        rewritten to
        value BETWEEN 0 AND <some number>
      */
      if (!args[1]->unsigned_flag && static_cast<longlong>(a) < 0) a = 0;
      /*
        Comparing as unsigned.
        value BETWEEN <some number> AND <some negative number>
        rewritten to
        1 BETWEEN <some number> AND 0
      */
      if (!args[2]->unsigned_flag && static_cast<longlong>(b) < 0) {
        b = 0;
        value = 1;
      }
    } else {
      // Comparing as signed, but a is unsigned and > LLONG_MAX.
      if (args[1]->unsigned_flag && static_cast<longlong>(a) < 0) {
        if (value < 0) {
          /*
            value BETWEEN <large number> AND b
            rewritten to
            value BETWEEN 0 AND b
          */
          a = 0;
        } else {
          /*
            value BETWEEN <large number> AND b
            rewritten to
            value BETWEEN LLONG_MAX AND b
          */
          a = LLONG_MAX;
          // rewrite to: (value-1) BETWEEN LLONG_MAX AND b
          if (value == LLONG_MAX) value -= 1;
        }
      }

      // Comparing as signed, but b is unsigned, and really large
      if (args[2]->unsigned_flag && static_cast<longlong>(b) < 0) b = LLONG_MAX;
    }

    if (!args[1]->null_value && !args[2]->null_value)
      return (longlong)((value >= a && value <= b) != negated);
    if (args[1]->null_value && args[2]->null_value)
      *null_value = true;
    else if (args[1]->null_value) {
      *null_value = value <= b;  // not null if false range.
    } else {
      *null_value = value >= a;
    }
    return value;
  }
}

longlong Item_func_between::val_int() {  // ANSI BETWEEN
  assert(fixed);
  THD *thd = current_thd;
  if (compare_as_dates_with_strings) {
    const int ge_res = ge_cmp.compare();
    if ((null_value = args[0]->null_value)) return 0;
    const int le_res = le_cmp.compare();

    if (!args[1]->null_value && !args[2]->null_value)
      return (longlong)((ge_res >= 0 && le_res <= 0) != negated);
    else if (args[1]->null_value) {
      null_value = le_res <= 0;  // not null if false range.
    } else {
      null_value = ge_res >= 0;
    }
  } else if (cmp_type == STRING_RESULT) {
    const CHARSET_INFO *cs = cmp_collation.collation;

    String *value = eval_string_arg(cs, args[0], &value0);
    null_value = args[0]->null_value;
    if (value == nullptr) {
      null_value = true;
      return 0;
    }
    String *a = eval_string_arg(cs, args[1], &value1);
    if (thd->is_error()) {
      return error_int();
    }
    String *b = eval_string_arg(cs, args[2], &value2);
    if (thd->is_error()) {
      return error_int();
    }
    if (!args[1]->null_value && !args[2]->null_value)
      return (longlong)((sortcmp(value, a, cmp_collation.collation) >= 0 &&
                         sortcmp(value, b, cmp_collation.collation) <= 0) !=
                        negated);
    if (args[1]->null_value && args[2]->null_value)
      null_value = true;
    else if (args[1]->null_value) {
      // Set to not null if false range.
      null_value = sortcmp(value, b, cmp_collation.collation) <= 0;
    } else {
      // Set to not null if false range.
      null_value = sortcmp(value, a, cmp_collation.collation) >= 0;
    }
  } else if (cmp_type == INT_RESULT) {
    longlong value;
    if (args[0]->unsigned_flag)
      value = compare_between_int_result<ulonglong>(compare_as_temporal_dates,
                                                    compare_as_temporal_times,
                                                    negated, args, &null_value);
    else
      value = compare_between_int_result<longlong>(compare_as_temporal_dates,
                                                   compare_as_temporal_times,
                                                   negated, args, &null_value);
    if (args[0]->null_value) return 0; /* purecov: inspected */
    if (!args[1]->null_value && !args[2]->null_value) return value;
  } else if (cmp_type == DECIMAL_RESULT) {
    my_decimal dec_buf, *dec = args[0]->val_decimal(&dec_buf), a_buf, *a_dec,
                        b_buf, *b_dec;
    if ((null_value = args[0]->null_value)) return 0; /* purecov: inspected */
    a_dec = args[1]->val_decimal(&a_buf);
    b_dec = args[2]->val_decimal(&b_buf);
    if (!args[1]->null_value && !args[2]->null_value)
      return (longlong)((my_decimal_cmp(dec, a_dec) >= 0 &&
                         my_decimal_cmp(dec, b_dec) <= 0) != negated);
    if (args[1]->null_value && args[2]->null_value)
      null_value = true;
    else if (args[1]->null_value)
      null_value = (my_decimal_cmp(dec, b_dec) <= 0);
    else
      null_value = (my_decimal_cmp(dec, a_dec) >= 0);
  } else {
    const double value = args[0]->val_real();
    double a, b;
    if (thd->is_error()) return false;
    if ((null_value = args[0]->null_value)) return 0; /* purecov: inspected */
    a = args[1]->val_real();
    if (thd->is_error()) return false;
    b = args[2]->val_real();
    if (thd->is_error()) return false;
    if (!args[1]->null_value && !args[2]->null_value)
      return (longlong)((value >= a && value <= b) != negated);
    if (args[1]->null_value && args[2]->null_value)
      null_value = true;
    else if (args[1]->null_value) {
      null_value = value <= b;  // not null if false range.
    } else {
      null_value = value >= a;
    }
  }
  return (longlong)(!null_value && negated);
}

void Item_func_between::print(const THD *thd, String *str,
                              enum_query_type query_type) const {
  str->append('(');
  args[0]->print(thd, str, query_type);
  if (negated) str->append(STRING_WITH_LEN(" not"));
  str->append(STRING_WITH_LEN(" between "));
  args[1]->print(thd, str, query_type);
  str->append(STRING_WITH_LEN(" and "));
  args[2]->print(thd, str, query_type);
  str->append(')');
}

Field *Item_func_ifnull::tmp_table_field(TABLE *table) {
  return tmp_table_field_from_field_type(table, false);
}

double Item_func_ifnull::real_op() {
  assert(fixed);
  double value = args[0]->val_real();
  if (current_thd->is_error()) return error_real();
  if (!args[0]->null_value) {
    null_value = false;
    return value;
  }
  value = args[1]->val_real();
  if (current_thd->is_error()) return error_real();
  if ((null_value = args[1]->null_value)) return 0.0;
  return value;
}

longlong Item_func_ifnull::int_op() {
  assert(fixed);
  longlong value = args[0]->val_int();
  if (current_thd->is_error()) return error_int();
  if (!args[0]->null_value) {
    null_value = false;
    return value;
  }
  value = args[1]->val_int();
  if (current_thd->is_error()) return error_int();
  if ((null_value = args[1]->null_value)) return 0;
  return value;
}

my_decimal *Item_func_ifnull::decimal_op(my_decimal *decimal_value) {
  assert(fixed);
  my_decimal *value = args[0]->val_decimal(decimal_value);
  if (current_thd->is_error()) return error_decimal(decimal_value);
  if (!args[0]->null_value) {
    null_value = false;
    return value;
  }
  value = args[1]->val_decimal(decimal_value);
  if (current_thd->is_error()) return error_decimal(decimal_value);
  if ((null_value = args[1]->null_value)) return nullptr;
  return value;
}

bool Item_func_ifnull::val_json(Json_wrapper *result) {
  null_value = false;
  bool has_value;
  if (json_value(args[0], result, &has_value)) return error_json();
  assert(!current_thd->is_error() && has_value);

  if (!args[0]->null_value) return false;

  if (json_value(args[1], result, &has_value)) return error_json();
  assert(!current_thd->is_error() && has_value);

  null_value = args[1]->null_value;
  return false;
}

bool Item_func_ifnull::date_op(MYSQL_TIME *ltime, my_time_flags_t fuzzydate) {
  assert(fixed);
  if (!args[0]->get_date(ltime, fuzzydate)) return (null_value = false);
  return (null_value = args[1]->get_date(ltime, fuzzydate));
}

bool Item_func_ifnull::time_op(MYSQL_TIME *ltime) {
  assert(fixed);
  if (!args[0]->get_time(ltime)) return (null_value = false);
  return (null_value = args[1]->get_time(ltime));
}

String *Item_func_ifnull::str_op(String *str) {
  assert(fixed);
  String *res = eval_string_arg(collation.collation, args[0], str);
  if (current_thd->is_error()) return error_str();
  if (!args[0]->null_value) {
    null_value = false;
    return res;
  }
  res = eval_string_arg(collation.collation, args[1], str);
  if (current_thd->is_error()) return error_str();

  if ((null_value = args[1]->null_value)) return nullptr;

  return res;
}

/**
  Perform context analysis of an IF item tree.

    This function performs context analysis (name resolution) and calculates
    various attributes of the item tree with Item_func_if as its root.
    The function saves in ref the pointer to the item or to a newly created
    item that is considered as a replacement for the original one.

  @param thd     reference to the global context of the query thread
  @param ref     pointer to Item* variable where pointer to resulting "fixed"
                 item is to be assigned

  @note
    Let T0(e)/T1(e) be the value of not_null_tables(e) when e is used on
    a predicate/function level. Then it's easy to show that:
    @verbatim
      T0(IF(e,e1,e2)  = T1(IF(e,e1,e2))
      T1(IF(e,e1,e2)) = intersection(T1(e1),T1(e2))
    @endverbatim

  @retval
    0   ok
  @retval
    1   got error
*/

bool Item_func_if::fix_fields(THD *thd, Item **ref) {
  assert(!fixed);
  args[0]->apply_is_true();

  if (Item_func::fix_fields(thd, ref)) return true;

  update_not_null_tables();

  return false;
}

void Item_func_if::fix_after_pullout(Query_block *parent_query_block,
                                     Query_block *removed_query_block) {
  Item_func::fix_after_pullout(parent_query_block, removed_query_block);
  update_not_null_tables();
}

void Item_func_if::update_used_tables() {
  Item_func::update_used_tables();
  update_not_null_tables();
}

bool Item_func_if::resolve_type(THD *thd) {
  // Assign type to the condition argument, if necessary
  if (param_type_is_default(thd, 0, 1, MYSQL_TYPE_LONGLONG)) return true;
  /*
   If none of the return arguments have type, type of this operator cannot
   be determined yet
  */
  if (args[1]->data_type() == MYSQL_TYPE_INVALID &&
      args[2]->data_type() == MYSQL_TYPE_INVALID)
    return false;

  return resolve_type_inner(thd);
}

bool Item_func_if::resolve_type_inner(THD *thd) {
  args++;
  arg_count--;
  if (param_type_uses_non_param(thd)) return true;
  args--;
  arg_count++;

  set_nullable(args[1]->is_nullable() || args[2]->is_nullable());
  if (aggregate_type(func_name(), args + 1, 2)) return true;

  cached_result_type = Field::result_merge_type(data_type());

  return false;
}

TYPELIB *Item_func_if::get_typelib() const {
  if (data_type() != MYSQL_TYPE_ENUM && data_type() != MYSQL_TYPE_SET) {
    return nullptr;
  }
  assert((args[1]->data_type() == MYSQL_TYPE_NULL) ^
         (args[2]->data_type() == MYSQL_TYPE_NULL));
  TYPELIB *typelib = args[1]->data_type() != MYSQL_TYPE_NULL
                         ? args[1]->get_typelib()
                         : args[2]->get_typelib();
  assert(typelib != nullptr);
  return typelib;
}

double Item_func_if::val_real() {
  assert(fixed);
  Item *arg = args[0]->val_bool() ? args[1] : args[2];
  if (current_thd->is_error()) return error_real();
  const double value = arg->val_real();
  null_value = arg->null_value;
  return value;
}

longlong Item_func_if::val_int() {
  assert(fixed);
  Item *arg = args[0]->val_bool() ? args[1] : args[2];
  if (current_thd->is_error()) return error_int();
  const longlong value = arg->val_int();
  null_value = arg->null_value;
  return value;
}

String *Item_func_if::val_str(String *str) {
  assert(fixed);

  switch (data_type()) {
    case MYSQL_TYPE_DATETIME:
    case MYSQL_TYPE_TIMESTAMP:
      return val_string_from_datetime(str);
    case MYSQL_TYPE_DATE:
      return val_string_from_date(str);
    case MYSQL_TYPE_TIME:
      return val_string_from_time(str);
    default: {
      Item *item = args[0]->val_bool() ? args[1] : args[2];
      if (current_thd->is_error()) return error_str();
      String *res = eval_string_arg(collation.collation, item, str);
      if (res == nullptr) return error_str();
      null_value = false;
      return res;
    }
  }
  null_value = true;
  return nullptr;
}

my_decimal *Item_func_if::val_decimal(my_decimal *decimal_value) {
  assert(fixed);
  Item *arg = args[0]->val_bool() ? args[1] : args[2];
  if (current_thd->is_error()) return error_decimal(decimal_value);
  my_decimal *value = arg->val_decimal(decimal_value);
  null_value = arg->null_value;
  return value;
}

bool Item_func_if::val_json(Json_wrapper *wr) {
  assert(fixed);
  Item *arg = args[0]->val_bool() ? args[1] : args[2];
  if (current_thd->is_error()) return error_json();
  bool has_value;
  const bool ok = json_value(arg, wr, &has_value);
  assert(has_value);
  null_value = arg->null_value;
  return ok;
}

bool Item_func_if::get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzydate) {
  assert(fixed);
  Item *arg = args[0]->val_bool() ? args[1] : args[2];
  if (arg->get_date(ltime, fuzzydate)) return error_date();
  null_value = arg->null_value;
  return false;
}

bool Item_func_if::get_time(MYSQL_TIME *ltime) {
  assert(fixed);
  Item *arg = args[0]->val_bool() ? args[1] : args[2];
  if (arg->get_time(ltime)) return error_time();
  null_value = arg->null_value;
  return false;
}

bool Item_func_nullif::resolve_type(THD *thd) {
  // If no arguments have a type, type of this operator cannot be determined yet
  if (args[0]->data_type() == MYSQL_TYPE_INVALID &&
      args[1]->data_type() == MYSQL_TYPE_INVALID) {
    /*
      Due to inheritance from Item_bool_func2, data_type() is LONGLONG.
      Ensure propagate_type() is called for this class:
    */
    set_data_type(MYSQL_TYPE_INVALID);
    return false;
  }
  return resolve_type_inner(thd);
}

bool Item_func_nullif::resolve_type_inner(THD *thd) {
  if (Item_bool_func2::resolve_type(thd)) return true;

  set_nullable(true);
  set_data_type_from_item(args[0]);
  cached_result_type = args[0]->result_type();

  // This class does not implement temporal data types
  if (is_temporal()) {
    set_data_type_string(args[0]->max_length);
    if (agg_arg_charsets_for_comparison(cmp.cmp_collation, args, arg_count))
      return true;
    cached_result_type = STRING_RESULT;
  }
  return false;
}

TYPELIB *Item_func_nullif::get_typelib() const {
  return args[0]->get_typelib();
}

/**
  @note
  Note that we have to evaluate the first argument twice as the compare
  may have been done with a different type than return value
  @return
    NULL  if arguments are equal
  @return
    the first argument if not equal
*/

double Item_func_nullif::val_real() {
  assert(fixed);
  double value;
  if (!cmp.compare()) {
    null_value = true;
    return 0.0;
  }
  value = args[0]->val_real();
  null_value = args[0]->null_value;
  return value;
}

longlong Item_func_nullif::val_int() {
  assert(fixed);
  longlong value;
  if (!cmp.compare()) {
    null_value = true;
    return 0;
  }
  value = args[0]->val_int();
  null_value = args[0]->null_value;
  return value;
}

String *Item_func_nullif::val_str(String *str) {
  assert(fixed);
  String *res;
  if (!cmp.compare()) {
    null_value = true;
    return nullptr;
  }
  if (current_thd->is_error()) return error_str();
  res = args[0]->val_str(str);
  null_value = args[0]->null_value;
  return res;
}

my_decimal *Item_func_nullif::val_decimal(my_decimal *decimal_value) {
  assert(fixed);
  my_decimal *res;
  if (!cmp.compare()) {
    null_value = true;
    return nullptr;
  }
  res = args[0]->val_decimal(decimal_value);
  null_value = args[0]->null_value;
  return res;
}

bool Item_func_nullif::val_json(Json_wrapper *wr) {
  assert(fixed);
  const int cmp_result = cmp.compare();
  // compare() calls val functions and may raise errors.
  if (current_thd->is_error()) {
    return error_json();
  }
  if (cmp_result == 0) {
    null_value = true;
    return false;
  }
  const bool res = args[0]->val_json(wr);
  null_value = args[0]->null_value;
  return res;
}

bool Item_func_nullif::is_null() {
  const int result = cmp.compare();
  if (current_thd->is_error()) {
    null_value = true;
    return true;
  }
  return (null_value = result == 0 ? true : args[0]->null_value);
}

/**
    Find and return matching items for CASE or ELSE item if all compares
    are failed or NULL if ELSE item isn't defined.

  IMPLEMENTATION
    In order to do correct comparisons of the CASE expression (the expression
    between CASE and the first WHEN) with each WHEN expression several
    comparators are used. One for each result type. CASE expression can be
    evaluated up to # of different result types are used. To check whether
    the CASE expression already was evaluated for a particular result type
    a bit mapped variable value_added_map is used. Result types are mapped
    to it according to their int values i.e. STRING_RESULT is mapped to bit
    0, REAL_RESULT to bit 1, so on.

  @retval
    NULL  Nothing found and there is no ELSE expression defined
  @retval
    item  Found item or ELSE item if defined and all comparisons are
           failed
*/

Item *Item_func_case::find_item(String *) {
  uint value_added_map = 0;

  if (first_expr_num == -1) {
    for (uint i = 0; i < ncases; i += 2) {
      // No expression between CASE and the first WHEN
      if (args[i]->val_bool()) return args[i + 1];
      if (current_thd->is_error()) return nullptr;
      continue;
    }
  } else {
    /* Compare every WHEN argument with it and return the first match */
    for (uint i = 0; i < ncases; i += 2) {
      if (args[i]->real_item()->type() == NULL_ITEM) continue;
      cmp_type = item_cmp_type(left_result_type, args[i]->result_type());
      assert(cmp_type != ROW_RESULT);
      assert(cmp_items[(uint)cmp_type]);
      if (!(value_added_map & (1U << (uint)cmp_type))) {
        cmp_items[(uint)cmp_type]->store_value(args[first_expr_num]);
        if (current_thd->is_error()) {
          return nullptr;
        }
        if ((null_value = args[first_expr_num]->null_value))
          return else_expr_num != -1 ? args[else_expr_num] : nullptr;
        value_added_map |= 1U << (uint)cmp_type;
      }
      if (cmp_items[(uint)cmp_type]->cmp(args[i]) == false) return args[i + 1];
    }
  }
  // No, WHEN clauses all missed, return ELSE expression
  return else_expr_num != -1 ? args[else_expr_num] : nullptr;
}

String *Item_func_case::val_str(String *str) {
  assert(fixed);
  switch (data_type()) {
    case MYSQL_TYPE_DATETIME:
    case MYSQL_TYPE_TIMESTAMP:
      return val_string_from_datetime(str);
    case MYSQL_TYPE_DATE:
      return val_string_from_date(str);
    case MYSQL_TYPE_TIME:
      return val_string_from_time(str);
    default: {
      Item *item = find_item(str);
      if (item == nullptr) return error_str();
      String *res = eval_string_arg(collation.collation, item, str);
      if (res == nullptr) return error_str();
      null_value = false;
      return res;
    }
  }
  if (current_thd->is_error()) {
    return error_str();
  } else {
    return null_return_str();
  }
}

longlong Item_func_case::val_int() {
  assert(fixed);
  StringBuffer<MAX_FIELD_WIDTH> dummy_str(default_charset());
  Item *item = find_item(&dummy_str);

  if (item != nullptr) {
    const longlong res = item->val_int();
    null_value = item->null_value;
    return res;
  }

  if (current_thd->is_error()) {
    return error_int();
  }

  null_value = true;
  return 0;
}

double Item_func_case::val_real() {
  assert(fixed);
  StringBuffer<MAX_FIELD_WIDTH> dummy_str(default_charset());
  Item *item = find_item(&dummy_str);

  if (item != nullptr) {
    const double res = item->val_real();
    null_value = item->null_value;
    return res;
  }

  if (current_thd->is_error()) {
    return error_real();
  }

  null_value = true;
  return 0.0;
}

my_decimal *Item_func_case::val_decimal(my_decimal *decimal_value) {
  assert(fixed);
  StringBuffer<MAX_FIELD_WIDTH> dummy_str(default_charset());
  Item *item = find_item(&dummy_str);

  if (item != nullptr) {
    my_decimal *res = item->val_decimal(decimal_value);
    null_value = item->null_value;
    return res;
  }

  if (current_thd->is_error()) {
    return error_decimal(decimal_value);
  }

  null_value = true;
  return nullptr;
}

bool Item_func_case::val_json(Json_wrapper *wr) {
  assert(fixed);
  char buff[MAX_FIELD_WIDTH];
  String dummy_str(buff, sizeof(buff), default_charset());
  Item *item = find_item(&dummy_str);

  // Make sure that calling find_item did not result in error
  if (current_thd->is_error()) return error_json();

  if (item == nullptr) {
    null_value = true;
    return false;
  }

  bool has_value;
  if (json_value(item, wr, &has_value)) return error_json();
  assert(!current_thd->is_error() && has_value);
  null_value = item->null_value;
  return false;
}

bool Item_func_case::get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzydate) {
  assert(fixed);
  char buff[MAX_FIELD_WIDTH];
  String dummy_str(buff, sizeof(buff), default_charset());
  Item *item = find_item(&dummy_str);
  if (!item) {
    null_value = is_nullable();
    return true;
  }
  if (item->get_date(ltime, fuzzydate)) return error_date();
  null_value = item->null_value;
  return false;
}

bool Item_func_case::get_time(MYSQL_TIME *ltime) {
  assert(fixed);
  char buff[MAX_FIELD_WIDTH];
  String dummy_str(buff, sizeof(buff), default_charset());
  Item *item = find_item(&dummy_str);
  if (!item) {
    null_value = is_nullable();
    return true;
  }
  if (item->get_time(ltime)) return error_time();
  null_value = item->null_value;
  return false;
}

bool Item_func_case::fix_fields(THD *thd, Item **ref) {
  /*
    buff should match stack usage from
    Item_func_case::val_int() -> Item_func_case::find_item()
  */
  uchar buff[MAX_FIELD_WIDTH * 2 + sizeof(String) * 2 + sizeof(String *) * 2 +
             sizeof(double) * 2 + sizeof(longlong) * 2];
  bool res = Item_func::fix_fields(thd, ref);
  /*
    Call check_stack_overrun after fix_fields to be sure that stack variable
    is not optimized away
  */
  if (check_stack_overrun(thd, STACK_MIN_SIZE, buff))
    return true;  // Fatal error flag is set!
  return res;
}

/**
  Check if (*place) and new_value points to different Items and call
  THD::change_item_tree() if needed.

  This function is a workaround for implementation deficiency in
  Item_func_case. The problem there is that the 'args' attribute contains
  Items from different expressions.

  The function must not be used elsewhere and will be remove eventually.
*/

static void change_item_tree_if_needed(Item **place, Item *new_value) {
  if (*place == new_value) return;

  *place = new_value;
  // WL#6570 remove-after-qa
  assert(current_thd->stmt_arena->is_regular() ||
         !current_thd->lex->is_exec_started());
}

bool Item_func_case::resolve_type(THD *thd) {
  Item **agg = (Item **)thd->mem_root->Alloc(sizeof(Item *) * (ncases + 1));
  if (agg == nullptr) return true;

  /*
    Choose types for dynamic parameters.
    1) CASE value WHEN [compare_value] THEN result [WHEN [compare_value] THEN
    result ...] [ELSE result] END

    If ? is in value/WHEN then infer from other WHENs/value. If ? if in
    THEN/ELSE then infer from other THENs/ELSE. If can't infer, use VARCHAR
    for value/WHEN, but determine type from outer context for THEN/ELSE.

    2) CASE WHEN [condition] THEN result [WHEN [condition] THEN result ...]
    [ELSE result] END
    If ? is in condition then do as for WHENs in (1).
  */
  // value/WHEN
  uint nagg;
  for (nagg = 0; nagg < ncases / 2; nagg++) agg[nagg] = args[nagg * 2];
  if (first_expr_num != -1) agg[nagg++] = args[first_expr_num];
  std::swap(args, agg);
  std::swap(arg_count, nagg);
  if (param_type_uses_non_param(thd)) return true;
  std::swap(args, agg);
  std::swap(arg_count, nagg);

  /*
   If none of the return arguments have type, type of this operator cannot
   be determined yet
  */
  bool all_types_invalid = true;
  for (uint i = 0; i < ncases / 2; i++)
    if (args[i * 2 + 1]->data_type() != MYSQL_TYPE_INVALID)
      all_types_invalid = false;
  if (else_expr_num != -1 &&
      args[else_expr_num]->data_type() != MYSQL_TYPE_INVALID)
    all_types_invalid = false;
  if (all_types_invalid) return false;

  // THEN/ELSE
  for (nagg = 0; nagg < ncases / 2; nagg++) agg[nagg] = args[nagg * 2 + 1];
  if (else_expr_num != -1) agg[nagg++] = args[else_expr_num];
  std::swap(args, agg);
  std::swap(arg_count, nagg);
  if (param_type_uses_non_param(thd)) return true;
  std::swap(args, agg);
  std::swap(arg_count, nagg);

  return resolve_type_inner(thd);
}

bool Item_func_case::resolve_type_inner(THD *thd) {
  /*
    @todo notice that both resolve_type() and resolve_type_inner() allocate
    an "agg" vector. One of the allocations is redundant and should be
    eliminated. This might be done when refactoring all CASE-derived operators
    to have a common base class.
  */
  Item **agg = (Item **)thd->mem_root->Alloc(sizeof(Item *) * (ncases + 1));
  if (agg == nullptr) return true;
  // Determine nullability based on THEN and ELSE expressions:

  bool nullable = else_expr_num == -1 || args[else_expr_num]->is_nullable();

  for (Item **arg = args + 1; arg < args + arg_count; arg += 2)
    nullable |= (*arg)->is_nullable();
  set_nullable(nullable);
  /*
    Aggregate all THEN and ELSE expression types
    and collations when string result
  */

  uint nagg;
  for (nagg = 0; nagg < ncases / 2; nagg++) agg[nagg] = args[nagg * 2 + 1];

  if (else_expr_num != -1) agg[nagg++] = args[else_expr_num];

  if (aggregate_type(func_name(), agg, nagg)) return true;

  cached_result_type = Field::result_merge_type(data_type());
  if (cached_result_type == STRING_RESULT) {
    /*
      Copy all THEN and ELSE items back to args[] array.
      Some of the items might have been changed to Item_func_conv_charset.
    */
    for (nagg = 0; nagg < ncases / 2; nagg++)
      change_item_tree_if_needed(&args[nagg * 2 + 1], agg[nagg]);

    if (else_expr_num != -1)
      change_item_tree_if_needed(&args[else_expr_num], agg[nagg++]);
  }
  /*
    Aggregate first expression and all WHEN expression types
    and collations when string comparison
  */
  if (first_expr_num != -1) {
    agg[0] = args[first_expr_num];
    left_result_type = agg[0]->result_type();

    /*
      As the first expression and WHEN expressions
      are intermixed in args[] array THEN and ELSE items,
      extract the first expression and all WHEN expressions into
      a temporary array, to process them easier.
    */
    for (nagg = 0; nagg < ncases / 2; nagg++) agg[nagg + 1] = args[nagg * 2];
    nagg++;
    const uint found_types = collect_cmp_types(agg, nagg);
    if (found_types == 0) return true;
    if (found_types & (1U << STRING_RESULT)) {
      /*
        If we'll do string comparison, we also need to aggregate
        character set and collation for first/WHEN items and
        install converters for some of them to cmp_collation when necessary.
        This is done because cmp_item comparators cannot compare
        strings in two different character sets.
        Some examples when we install converters:

        1. Converter installed for the first expression:

           CASE         latin1_item              WHEN utf16_item THEN ... END

        is replaced to:

           CASE CONVERT(latin1_item USING utf16) WHEN utf16_item THEN ... END

        2. Converter installed for the left WHEN item:

          CASE utf16_item WHEN         latin1_item              THEN ... END

        is replaced to:

           CASE utf16_item WHEN CONVERT(latin1_item USING utf16) THEN ... END
      */
      if (agg_arg_charsets_for_comparison(cmp_collation, agg, nagg))
        return true;
      /*
        Now copy first expression and all WHEN expressions back to args[]
        array, because some of the items might have been changed to converters
        (e.g. Item_func_conv_charset, or Item_string for constants).
      */
      change_item_tree_if_needed(&args[first_expr_num], agg[0]);

      for (nagg = 0; nagg < ncases / 2; nagg++)
        change_item_tree_if_needed(&args[nagg * 2], agg[nagg + 1]);
    }
    for (uint i = 0; i <= (uint)DECIMAL_RESULT; i++) {
      // @todo - for time being, fill in ALL cmp_items slots
      if (found_types & (1U << i) && !cmp_items[i]) {
        assert((Item_result)i != ROW_RESULT);
        cmp_items[i] = cmp_item::new_comparator(
            thd, static_cast<Item_result>(i), args[first_expr_num],
            cmp_collation.collation);
        if (cmp_items[i] == nullptr) return true;
      }
    }
    /*
      Set cmp_context of all WHEN arguments. This prevents
      Item_field::equal_fields_propagator() from transforming a
      zerofill argument into a string constant. Such a change would
      require rebuilding cmp_items.
    */
    for (uint i = 0; i < ncases; i += 2)
      args[i]->cmp_context =
          item_cmp_type(left_result_type, args[i]->result_type());
  }
  return false;
}

TYPELIB *Item_func_case::get_typelib() const {
  if (data_type() != MYSQL_TYPE_ENUM && data_type() != MYSQL_TYPE_SET) {
    return nullptr;
  }
  TYPELIB *typelib = nullptr;
  for (uint i = 0; i < ncases; i += 2) {
    if (typelib == nullptr) {
      typelib = args[i + 1]->get_typelib();
    } else {
      assert(args[i + 1]->get_typelib() == nullptr);
    }
  }
  if (else_expr_num != -1 && typelib == nullptr) {
    typelib = args[else_expr_num]->get_typelib();
  }
  assert(typelib != nullptr);
  return typelib;
}

/**
  @todo
    Fix this so that it prints the whole CASE expression
*/

void Item_func_case::print(const THD *thd, String *str,
                           enum_query_type query_type) const {
  str->append(STRING_WITH_LEN("(case "));
  if (first_expr_num != -1) {
    args[first_expr_num]->print(thd, str, query_type);
    str->append(' ');
  }
  for (uint i = 0; i < ncases; i += 2) {
    str->append(STRING_WITH_LEN("when "));
    args[i]->print(thd, str, query_type);
    str->append(STRING_WITH_LEN(" then "));
    args[i + 1]->print(thd, str, query_type);
    str->append(' ');
  }
  if (else_expr_num != -1) {
    str->append(STRING_WITH_LEN("else "));
    args[else_expr_num]->print(thd, str, query_type);
    str->append(' ');
  }
  str->append(STRING_WITH_LEN("end)"));
}

Item_func_case::~Item_func_case() {
  for (uint i = 0; i <= (uint)DECIMAL_RESULT; i++) {
    if (cmp_items[i] != nullptr) {
      ::destroy_at(cmp_items[i]);
      cmp_items[i] = nullptr;
    }
  }
}

/**
  Coalesce - return first not NULL argument.
*/

String *Item_func_coalesce::str_op(String *str) {
  assert(fixed);
  null_value = false;
  for (uint i = 0; i < arg_count; i++) {
    String *res = eval_string_arg(collation.collation, args[i], str);
    if (current_thd->is_error()) return error_str();
    if (res != nullptr) return res;
  }
  null_value = true;
  return error_str();
}

bool Item_func_coalesce::val_json(Json_wrapper *wr) {
  assert(fixed);
  null_value = false;
  for (uint i = 0; i < arg_count; i++) {
    bool has_value;
    if (json_value(args[i], wr, &has_value)) return error_json();
    assert(!current_thd->is_error() && has_value);
    if (!args[i]->null_value) return false;
  }

  null_value = true;
  return false;
}

longlong Item_func_coalesce::int_op() {
  assert(fixed);
  null_value = false;
  for (uint i = 0; i < arg_count; i++) {
    const longlong res = args[i]->val_int();
    if (current_thd->is_error()) return error_int();
    if (!args[i]->null_value) return res;
  }
  null_value = true;
  return 0;
}

double Item_func_coalesce::real_op() {
  assert(fixed);
  null_value = false;
  for (uint i = 0; i < arg_count; i++) {
    const double res = args[i]->val_real();
    if (current_thd->is_error()) return 0.0E0;
    if (!args[i]->null_value) return res;
  }
  null_value = true;
  return 0;
}

my_decimal *Item_func_coalesce::decimal_op(my_decimal *decimal_value) {
  assert(fixed);
  null_value = false;
  for (uint i = 0; i < arg_count; i++) {
    my_decimal *res = args[i]->val_decimal(decimal_value);
    if (current_thd->is_error()) return error_decimal(decimal_value);
    if (!args[i]->null_value) return res;
  }
  null_value = true;
  return nullptr;
}

bool Item_func_coalesce::date_op(MYSQL_TIME *ltime, my_time_flags_t fuzzydate) {
  assert(fixed);
  for (uint i = 0; i < arg_count; i++) {
    if (!args[i]->get_date(ltime, fuzzydate)) return (null_value = false);
  }
  return (null_value = true);
}

bool Item_func_coalesce::time_op(MYSQL_TIME *ltime) {
  assert(fixed);
  for (uint i = 0; i < arg_count; i++) {
    if (!args[i]->get_time(ltime)) return (null_value = false);
  }
  return (null_value = true);
}

bool Item_func_coalesce::resolve_type(THD *thd) {
  // If no arguments have type, type of this operator cannot be determined yet
  bool all_types_invalid = true;
  for (uint i = 0; i < arg_count; i++)
    if (args[i]->data_type() != MYSQL_TYPE_INVALID) all_types_invalid = false;
  if (all_types_invalid) return false;
  return resolve_type_inner(thd);
}

bool Item_func_coalesce::resolve_type_inner(THD *thd) {
  if (param_type_uses_non_param(thd)) return true;
  if (aggregate_type(func_name(), args, arg_count)) return true;

  hybrid_type = Field::result_merge_type(data_type());
  for (uint i = 0; i < arg_count; i++) {
    // A non-nullable argument guarantees a non-NULL result
    if (!args[i]->is_nullable()) {
      set_nullable(false);
      break;
    }
  }
  return false;
}

TYPELIB *Item_func_coalesce::get_typelib() const {
  if (data_type() != MYSQL_TYPE_ENUM && data_type() != MYSQL_TYPE_SET) {
    return nullptr;
  }
  TYPELIB *typelib = nullptr;
  for (uint i = 0; i < arg_count; i++) {
    if (typelib == nullptr) {
      typelib = args[i]->get_typelib();
    } else {
      assert(args[i]->get_typelib() == nullptr);
    }
  }
  assert(typelib != nullptr);
  return typelib;
}

/****************************************************************************
 Classes and function for the IN operator
****************************************************************************/

bool in_vector::fill(Item **items, uint item_count) {
  m_used_size = 0;
  for (uint i = 0; i < item_count; i++) {
    set(m_used_size, items[i]);
    if (current_thd->is_error()) return true;
    /*
      We don't put NULL values in array, to avoid erroneous matches in
      bisection.
    */
    if (!items[i]->null_value) m_used_size++;  // include this cell in array.
  }
  assert(m_used_size <= m_size);

  sort_array();

  return m_used_size < item_count;  // True = at least one null value found.
}

bool in_row::allocate(MEM_ROOT *mem_root, Item *lhs, uint arg_count) {
  for (uint i = 0; i < arg_count; i++) {
    if (base_pointers[i]->allocate_value_comparators(mem_root, tmp.get(),
                                                     lhs)) {
      return true;
    }
  }
  return false;
}

/*
  Determine which of the signed longlong arguments is bigger

  SYNOPSIS
    cmp_longs()
      a_val     left argument
      b_val     right argument

  DESCRIPTION
    This function will compare two signed longlong arguments
    and will return -1, 0, or 1 if left argument is smaller than,
    equal to or greater than the right argument.

  RETURN VALUE
    -1          left argument is smaller than the right argument.
    0           left argument is equal to the right argument.
    1           left argument is greater than the right argument.
*/
static inline int cmp_longs(longlong a_val, longlong b_val) {
  return a_val < b_val ? -1 : a_val == b_val ? 0 : 1;
}

/*
  Determine which of the unsigned longlong arguments is bigger

  SYNOPSIS
    cmp_ulongs()
      a_val     left argument
      b_val     right argument

  DESCRIPTION
    This function will compare two unsigned longlong arguments
    and will return -1, 0, or 1 if left argument is smaller than,
    equal to or greater than the right argument.

  RETURN VALUE
    -1          left argument is smaller than the right argument.
    0           left argument is equal to the right argument.
    1           left argument is greater than the right argument.
*/
static inline int cmp_ulongs(ulonglong a_val, ulonglong b_val) {
  return a_val < b_val ? -1 : a_val == b_val ? 0 : 1;
}

/*
  Compare two integers in IN value list format (packed_longlong)

  SYNOPSIS
    cmp_longlong()
      a         left argument
      b         right argument

  DESCRIPTION
    This function will compare two integer arguments in the IN value list
    format and will return -1, 0, or 1 if left argument is smaller than,
    equal to or greater than the right argument.
    It's used in sorting the IN values list and finding an element in it.
    Depending on the signedness of the arguments cmp_longlong() will
    compare them as either signed (using cmp_longs()) or unsigned (using
    cmp_ulongs()).

  RETURN VALUE
    -1          left argument is smaller than the right argument.
    0           left argument is equal to the right argument.
    1           left argument is greater than the right argument.
*/
static int cmp_longlong(const in_longlong::packed_longlong *a,
                        const in_longlong::packed_longlong *b) {
  if (a->unsigned_flag != b->unsigned_flag) {
    /*
      One of the args is unsigned and is too big to fit into the
      positive signed range. Report no match.
    */
    if ((a->unsigned_flag && ((ulonglong)a->val) > (ulonglong)LLONG_MAX) ||
        (b->unsigned_flag && ((ulonglong)b->val) > (ulonglong)LLONG_MAX))
      return a->unsigned_flag ? 1 : -1;
    /*
      Although the signedness differs both args can fit into the signed
      positive range. Make them signed and compare as usual.
    */
    return cmp_longs(a->val, b->val);
  }
  if (a->unsigned_flag)
    return cmp_ulongs((ulonglong)a->val, (ulonglong)b->val);
  else
    return cmp_longs(a->val, b->val);
}

class Cmp_longlong {
 public:
  bool operator()(const in_longlong::packed_longlong &a,
                  const in_longlong::packed_longlong &b) {
    return cmp_longlong(&a, &b) < 0;
  }
};

void in_longlong::sort_array() {
  std::sort(base.begin(), base.begin() + m_used_size, Cmp_longlong());
}

bool in_longlong::find_item(Item *item) {
  if (m_used_size == 0) return false;
  packed_longlong result;
  val_item(item, &result);
  if (item->null_value) return false;
  return std::binary_search(base.begin(), base.begin() + m_used_size, result,
                            Cmp_longlong());
}

bool in_longlong::compare_elems(uint pos1, uint pos2) const {
  return cmp_longlong(&base[pos1], &base[pos2]) != 0;
}

class Cmp_row {
 public:
  bool operator()(const cmp_item_row *a, const cmp_item_row *b) {
    return a->compare(b) < 0;
  }
};

void in_row::sort_array() {
  std::sort(base_pointers.begin(), base_pointers.begin() + m_used_size,
            Cmp_row());
}

bool in_row::find_item(Item *item) {
  if (m_used_size == 0) return false;
  tmp->store_value(item);
  if (item->null_value) return false;
  return std::binary_search(base_pointers.begin(),
                            base_pointers.begin() + m_used_size, tmp.get(),
                            Cmp_row());
}

bool in_row::compare_elems(uint pos1, uint pos2) const {
  return base_pointers[pos1]->compare(base_pointers[pos2]) != 0;
}

in_string::in_string(MEM_ROOT *mem_root, uint elements, const CHARSET_INFO *cs)
    : in_vector(elements),
      tmp(buff, sizeof(buff), &my_charset_bin),
      base_objects(mem_root, elements),
      base_pointers(mem_root, elements),
      collation(cs) {
  for (uint ix = 0; ix < elements; ++ix) {
    base_pointers[ix] = &base_objects[ix];
  }
}

void in_string::cleanup() {
  // Clear reference pointers and free any memory allocated for holding data.
  for (uint i = 0; i < m_used_size; i++) {
    String *str = base_pointers[i];
    str->set(static_cast<const char *>(nullptr), 0, str->charset());
  }
}

void in_string::set(uint pos, Item *item) {
  String *str = base_pointers[pos];
  String *res = eval_string_arg(collation, item, str);
  if (res == nullptr || res == str) return;

  if (res->uses_buffer_owned_by(str)) res->copy();
  if (item->type() == Item::FUNC_ITEM)
    str->copy(*res);
  else
    *str = *res;
}

static int srtcmp_in(const CHARSET_INFO *cs, const String *x, const String *y) {
  return cs->coll->strnncollsp(
      cs, pointer_cast<const uchar *>(x->ptr()), x->length(),
      pointer_cast<const uchar *>(y->ptr()), y->length());
}

namespace {
class Cmp_string {
 public:
  explicit Cmp_string(const CHARSET_INFO *cs) : collation(cs) {}
  bool operator()(const String *a, const String *b) const {
    return srtcmp_in(collation, a, b) < 0;
  }

 private:
  const CHARSET_INFO *collation;
};
}  // namespace

// Sort string pointers, not string objects.
void in_string::sort_array() {
  std::sort(base_pointers.begin(), base_pointers.begin() + m_used_size,
            Cmp_string(collation));
}

bool in_string::find_item(Item *item) {
  if (m_used_size == 0) return false;
  const String *str = eval_string_arg(collation, item, &tmp);
  if (str == nullptr) return false;
  if (current_thd->is_error()) return false;
  return std::binary_search(base_pointers.begin(),
                            base_pointers.begin() + m_used_size, str,
                            Cmp_string(collation));
}

bool in_string::compare_elems(uint pos1, uint pos2) const {
  return srtcmp_in(collation, base_pointers[pos1], base_pointers[pos2]) != 0;
}

in_row::in_row(MEM_ROOT *mem_root, uint elements, cmp_item_row *cmp)
    : in_vector(elements),
      tmp(cmp),
      base_objects(mem_root, elements),
      base_pointers(mem_root, elements) {
  for (uint ix = 0; ix < elements; ++ix) {
    base_pointers[ix] = &base_objects[ix];
  }
}

void in_row::set(uint pos, Item *item) {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("pos: %u  item: %p", pos, item));
  base_pointers[pos]->store_value_by_template(tmp.get(), item);
}

void in_longlong::val_item(Item *item, packed_longlong *result) {
  result->val = item->val_int();
  result->unsigned_flag = item->unsigned_flag;
}

void in_time_as_longlong::val_item(Item *item, packed_longlong *result) {
  result->val = item->val_time_temporal();
  result->unsigned_flag = item->unsigned_flag;
}

void in_datetime_as_longlong::val_item(Item *item, packed_longlong *result) {
  result->val = item->val_date_temporal();
  result->unsigned_flag = item->unsigned_flag;
}

void in_datetime::set(uint pos, Item *item) {
  Item **p = &item;
  bool is_null;
  struct packed_longlong *buff = &base[pos];

  buff->val = get_datetime_value(current_thd, &p, nullptr, warn_item, &is_null);
  buff->unsigned_flag = true;
}

void in_datetime::val_item(Item *item, packed_longlong *result) {
  bool is_null;
  Item **p = &item;
  result->val =
      get_datetime_value(current_thd, &p, nullptr, warn_item, &is_null);
  result->unsigned_flag = true;
}

void in_double::set(uint pos, Item *item) { base[pos] = item->val_real(); }

void in_double::sort_array() {
  std::sort(base.begin(), base.begin() + m_used_size);
}

bool in_double::find_item(Item *item) {
  if (m_used_size == 0) return false;
  const double dbl = item->val_real();
  if (item->null_value) return false;
  return std::binary_search(base.begin(), base.begin() + m_used_size, dbl);
}

bool in_double::compare_elems(uint pos1, uint pos2) const {
  return base[pos1] != base[pos2];
}

void in_decimal::set(uint pos, Item *item) {
  /* as far as 'item' is constant, we can store reference on my_decimal */
  my_decimal *dec = &base[pos];
  my_decimal *res = item->val_decimal(dec);
  /* if item->val_decimal() is evaluated to NULL then res == 0 */
  if (!item->null_value && res != dec) my_decimal2decimal(res, dec);
}

void in_decimal::sort_array() {
  std::sort(base.begin(), base.begin() + m_used_size);
}

bool in_decimal::find_item(Item *item) {
  if (m_used_size == 0) return false;
  my_decimal val;
  const my_decimal *dec = item->val_decimal(&val);
  if (item->null_value) return false;
  return std::binary_search(base.begin(), base.begin() + m_used_size, *dec);
}

bool in_decimal::compare_elems(uint pos1, uint pos2) const {
  return base[pos1] != base[pos2];
}

bool cmp_item::allocate_value_comparators(MEM_ROOT *, cmp_item *, Item *) {
  return false;
}

cmp_item *cmp_item::new_comparator(THD *thd, Item_result result_type,
                                   Item *item, const CHARSET_INFO *cs) {
  switch (result_type) {
    case STRING_RESULT:
      /*
        Temporal types shouldn't be compared as strings. Since date/time formats
        may be different, e.g. '20000102' == '2000-01-02'."
      */
      if (item->is_temporal())
        return new (*THR_MALLOC) cmp_item_datetime(item);
      else
        return new (*THR_MALLOC) cmp_item_string(cs);
    case INT_RESULT:
      return new (*THR_MALLOC) cmp_item_int;
    case REAL_RESULT:
      return new (*THR_MALLOC) cmp_item_real;
    case ROW_RESULT:
      return new (*THR_MALLOC) cmp_item_row(thd, item);
    case DECIMAL_RESULT:
      return new (*THR_MALLOC) cmp_item_decimal;
    default:
      assert(false);
      break;
  }
  return nullptr;  // to satisfy compiler :)
}

cmp_item *cmp_item_string::make_same() {
  return new (*THR_MALLOC) cmp_item_string(cmp_charset);
}

int cmp_item_string::cmp(Item *arg) {
  if (m_null_value) return UNKNOWN;
  StringBuffer<STRING_BUFFER_USUAL_SIZE> tmp(cmp_charset);
  String *res = eval_string_arg(cmp_charset, arg, &tmp);
  if (res == nullptr) return UNKNOWN;
  return sortcmp(value_res, res, cmp_charset) != 0;
}

cmp_item *cmp_item_int::make_same() { return new (*THR_MALLOC) cmp_item_int(); }

cmp_item *cmp_item_real::make_same() {
  return new (*THR_MALLOC) cmp_item_real();
}

cmp_item *cmp_item_row::make_same() { return new (*THR_MALLOC) cmp_item_row(); }

cmp_item_json::cmp_item_json(unique_ptr_destroy_only<Json_wrapper> wrapper,
                             unique_ptr_destroy_only<Json_scalar_holder> holder)
    : m_value(std::move(wrapper)), m_holder(std::move(holder)) {}

cmp_item_json::~cmp_item_json() = default;

/// Create a cmp_item_json object on a MEM_ROOT.
static cmp_item_json *make_cmp_item_json(MEM_ROOT *mem_root) {
  auto wrapper = make_unique_destroy_only<Json_wrapper>(mem_root);
  if (wrapper == nullptr) return nullptr;
  auto holder = make_unique_destroy_only<Json_scalar_holder>(mem_root);
  if (holder == nullptr) return nullptr;
  return new (mem_root) cmp_item_json(std::move(wrapper), std::move(holder));
}

cmp_item *cmp_item_json::make_same() { return make_cmp_item_json(*THR_MALLOC); }

int cmp_item_json::compare(const cmp_item *ci) const {
  const cmp_item_json *l_cmp = down_cast<const cmp_item_json *>(ci);
  return m_value->compare(*l_cmp->m_value);
}

void cmp_item_json::store_value(Item *item) {
  bool err = false;
  if (item->data_type() == MYSQL_TYPE_JSON)
    err = item->val_json(m_value.get());
  else {
    String tmp;
    err = get_json_atom_wrapper(&item, 0, "IN", &m_str_value, &tmp,
                                m_value.get(), m_holder.get(), true);
  }
  set_null_value(err || item->null_value);
}

int cmp_item_json::cmp(Item *arg) {
  Json_scalar_holder holder;
  Json_wrapper wr;

  if (m_null_value) return UNKNOWN;

  if (arg->data_type() == MYSQL_TYPE_JSON) {
    if (arg->val_json(&wr) || arg->null_value) return UNKNOWN;
  } else {
    String tmp, str;
    if (get_json_atom_wrapper(&arg, 0, "IN", &str, &tmp, &wr, &holder, true) ||
        arg->null_value)
      return UNKNOWN; /* purecov: inspected */
  }
  return m_value->compare(wr) ? 1 : 0;
}

cmp_item_row::~cmp_item_row() {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("this: %p", this));
  if (comparators) {
    for (uint i = 0; i < n; i++) {
      if (comparators[i] != nullptr) ::destroy_at(comparators[i]);
    }
  }
}

bool cmp_item_row::allocate_template_comparators(THD *thd, Item *item) {
  assert(n == item->cols());
  n = item->cols();
  assert(comparators == nullptr);
  comparators = thd->mem_root->ArrayAlloc<cmp_item *>(n);
  if (comparators == nullptr) return true;

  for (uint i = 0; i < n; i++) {
    assert(comparators[i] == nullptr);
    Item *item_i = item->element_index(i);
    comparators[i] = cmp_item::new_comparator(
        thd, item_i->result_type(), item_i, item_i->collation.collation);
    if (comparators[i] == nullptr) return true;  // Allocation failed
  }
  return false;
}

void cmp_item_row::store_value(Item *item) {
  DBUG_TRACE;
  assert(comparators != nullptr);
  item->bring_value();
  item->null_value = false;
  for (uint i = 0; i < n; i++) {
    comparators[i]->store_value(item->element_index(i));
    item->null_value |= item->element_index(i)->null_value;
  }
}

bool cmp_item_row::allocate_value_comparators(MEM_ROOT *mem_root,
                                              cmp_item *tmpl, Item *item) {
  cmp_item_row *row_template = down_cast<cmp_item_row *>(tmpl);
  assert(row_template->n == item->cols());
  n = row_template->n;
  assert(comparators == nullptr);
  comparators = (cmp_item **)mem_root->Alloc(sizeof(cmp_item *) * n);
  if (comparators == nullptr) return true;

  for (uint i = 0; i < n; i++) {
    comparators[i] = row_template->comparators[i]->make_same();
    if (comparators[i] == nullptr) return true;
    if (comparators[i]->allocate_value_comparators(
            mem_root, row_template->comparators[i], item->element_index(i))) {
      return true;
    }
  }
  return false;
}

void cmp_item_row::store_value_by_template(cmp_item *t, Item *item) {
  cmp_item_row *tmpl = (cmp_item_row *)t;
  item->bring_value();
  item->null_value = false;
  for (uint i = 0; i < n; i++) {
    comparators[i]->store_value_by_template(tmpl->comparators[i],
                                            item->element_index(i));
    item->null_value |= item->element_index(i)->null_value;
  }
}

int cmp_item_row::cmp(Item *arg) {
  arg->null_value = false;
  if (arg->cols() != n) {
    my_error(ER_OPERAND_COLUMNS, MYF(0), n);
    return 1;
  }
  bool was_null = false;
  arg->bring_value();
  for (uint i = 0; i < n; i++) {
    const int rc = comparators[i]->cmp(arg->element_index(i));
    switch (rc) {
      case UNKNOWN:
        was_null = true;
        break;
      case true:
        return true;
      case false:
        break;  // elements #i are equal
    }
    arg->null_value |= arg->element_index(i)->null_value;
  }
  return was_null ? UNKNOWN : false;
}

int cmp_item_row::compare(const cmp_item *c) const {
  const cmp_item_row *l_cmp = down_cast<const cmp_item_row *>(c);
  for (uint i = 0; i < n; i++) {
    int res;
    if ((res = comparators[i]->compare(l_cmp->comparators[i]))) return res;
  }
  return 0;
}

void cmp_item_decimal::store_value(Item *item) {
  my_decimal *val = item->val_decimal(&value);
  /* val may be zero if item is nnull */
  if (val && val != &value) my_decimal2decimal(val, &value);
  set_null_value(item->null_value);
}

int cmp_item_decimal::cmp(Item *arg) {
  my_decimal tmp_buf, *tmp = arg->val_decimal(&tmp_buf);
  return (m_null_value || arg->null_value) ? UNKNOWN
                                           : (my_decimal_cmp(&value, tmp) != 0);
}

int cmp_item_decimal::compare(const cmp_item *arg) const {
  const cmp_item_decimal *l_cmp = down_cast<const cmp_item_decimal *>(arg);
  return my_decimal_cmp(&value, &l_cmp->value);
}

cmp_item *cmp_item_decimal::make_same() {
  return new (*THR_MALLOC) cmp_item_decimal();
}

cmp_item_datetime::cmp_item_datetime(const Item *warn_item_arg)
    : warn_item(warn_item_arg),
      has_date(warn_item_arg->is_temporal_with_date()) {}

void cmp_item_datetime::store_value(Item *item) {
  bool is_null;
  Item **p = &item;
  if (has_date)
    value = get_datetime_value(current_thd, &p, nullptr, warn_item, &is_null);
  else
    value = get_time_value(current_thd, &p, nullptr, nullptr, &is_null);
  set_null_value(item->null_value);
}

int cmp_item_datetime::cmp(Item *item) {
  bool is_null;
  longlong value2 = 0;
  Item **p = &item;
  if (has_date)
    value2 = get_datetime_value(current_thd, &p, nullptr, warn_item, &is_null);
  else
    value2 = get_time_value(current_thd, &p, nullptr, nullptr, &is_null);

  const bool rc = (value != value2);
  return (m_null_value || item->null_value) ? UNKNOWN : rc;
}

int cmp_item_datetime::compare(const cmp_item *ci) const {
  const cmp_item_datetime *l_cmp = down_cast<const cmp_item_datetime *>(ci);
  return (value < l_cmp->value) ? -1 : ((value == l_cmp->value) ? 0 : 1);
}

cmp_item *cmp_item_datetime::make_same() {
  return new (*THR_MALLOC) cmp_item_datetime(warn_item);
}

float Item_func_in::get_single_col_filtering_effect(
    Item_ident *fieldref, table_map filter_for_table,
    const MY_BITMAP *fields_to_ignore, double rows_in_table) {
  /*
    Does not contribute to filtering effect if
    1) This field belongs to another table.
    2) Filter effect for this field has already been taken into
       account. 'fieldref' may be a field or a reference to a field
       (through a view, to an outer table etc)
  */
  if ((fieldref->used_tables() != filter_for_table) ||  // 1)
      bitmap_is_set(fields_to_ignore,
                    static_cast<Item_field *>(fieldref->real_item())
                        ->field->field_index()))  // 2)
    return COND_FILTER_ALLPASS;

  const Item_field *fld = (Item_field *)fieldref->real_item();
  return fld->get_cond_filter_default_probability(rows_in_table,
                                                  COND_FILTER_EQUALITY);
}

float Item_func_in::get_filtering_effect(THD *thd, table_map filter_for_table,
                                         table_map read_tables,
                                         const MY_BITMAP *fields_to_ignore,
                                         double rows_in_table) {
  assert((read_tables & filter_for_table) == 0);
  /*
    To contribute to filtering effect, the condition must refer to
    exactly one unread table: the table filtering is currently
    calculated for.

    Dependent subqueries are not considered available values and no
    filtering should be calculated for this item if the IN list
    contains one. dep_subq_in_list is 'true' if the IN list contains a
    dependent subquery.
  */
  if ((used_tables() & ~read_tables) != filter_for_table || dep_subq_in_list)
    return COND_FILTER_ALLPASS;

  /*
    No matter how many row values are input the filtering effect
    shall not be higher than in_max_filter (currently 0.5).
  */
  const float in_max_filter = 0.5f;

  float filter = COND_FILTER_ALLPASS;
  if (args[0]->type() == Item::ROW_ITEM) {
    /*
      This is a row value IN predicate:
         "WHERE (col1, col2, ...) IN ((1,2,..), ...)"
      which can be rewritten to:
         "WHERE (col1=1 AND col2=2...) OR (col1=.. AND col2=...) OR ..."

      The filtering effect is:
        filter= #row_values * filter(<single_row_value>)

      where filter(<single_row_value>) = filter(col1) * filter(col2) * ...

      In other words, we ignore the fact that there could be identical
      row values since writing "WHERE (a,b) IN ((1,1), (1,1), ...)" is
      not expected input from a user.
    */
    Item_row *lhs_row = static_cast<Item_row *>(args[0]);
    // For all items in the left row
    float single_rowval_filter = COND_FILTER_ALLPASS;
    for (uint i = 0; i < lhs_row->cols(); i++) {
      /*
        May contribute to condition filtering only if
        lhs_row->element_index(i) is a field or a reference to a field
        (through a view, to an outer table etc)
      */
      if (lhs_row->element_index(i)->real_item()->type() == Item::FIELD_ITEM) {
        Item_ident *fieldref =
            static_cast<Item_ident *>(lhs_row->element_index(i));

        const float tmp_filt = get_single_col_filtering_effect(
            fieldref, filter_for_table, fields_to_ignore, rows_in_table);
        single_rowval_filter *= tmp_filt;
      }
    }

    /*
      If single_rowval_filter == COND_FILTER_ALLPASS, the filtering
      effect of this field should be ignored. If not, selectivity
      should not be higher than 'in_max_filter' even if there are a
      lot of values on the right hand side

      arg_count includes the left hand side item
    */
    if (single_rowval_filter != COND_FILTER_ALLPASS)
      filter = min((arg_count - 1) * single_rowval_filter, in_max_filter);
  } else if (args[0]->real_item()->type() == Item::FIELD_ITEM) {
    /*
      This is a single-column IN predicate:
        "WHERE col IN (1, 2, ...)"
      which can be rewritten to:
        "WHERE col=1 OR col1=2 OR ..."

      The filtering effect is: #values_right_hand_side * selectivity(=)

      As for row values, it is assumed that no values on the right
      hand side are identical.
    */
    assert(args[0]->type() == FIELD_ITEM || args[0]->type() == REF_ITEM);

    if (args[0]->type() == FIELD_ITEM) {
      const Item_field *item_field = down_cast<const Item_field *>(args[0]);
      const histograms::enum_operator op =
          (negated ? histograms::enum_operator::NOT_IN_LIST
                   : histograms::enum_operator::IN_LIST);

      const double selectivity =
          get_histogram_selectivity(thd, *item_field->field, op, *this);

      if (selectivity != kUndefinedSelectivity) {
        return selectivity;
      }
    }

    Item_ident *fieldref = static_cast<Item_ident *>(args[0]);
    const float tmp_filt = get_single_col_filtering_effect(
        fieldref, filter_for_table, fields_to_ignore, rows_in_table);
    /*
      If tmp_filt == COND_FILTER_ALLPASS, the filtering effect of this
      field should be ignored. If not, selectivity should not be
      higher than 'in_max_filter' even if there are a lot of values on
      the right hand side

      arg_count includes the left hand side item
    */
    if (tmp_filt != COND_FILTER_ALLPASS)
      filter = min((arg_count - 1) * tmp_filt, in_max_filter);
  }

  if (negated && filter != COND_FILTER_ALLPASS) filter = 1.0f - filter;

  assert(filter >= 0.0f && filter <= 1.0f);
  return filter;
}

bool Item_func_in::list_contains_null() {
  Item **arg, **arg_end;
  for (arg = args + 1, arg_end = args + arg_count; arg != arg_end; arg++) {
    if ((*arg)->null_inside()) return true;
  }
  return false;
}

/**
  Perform context analysis of an IN item tree.

    This function performs context analysis (name resolution) and calculates
    various attributes of the item tree with Item_func_in as its root.
    The function saves in ref the pointer to the item or to a newly created
    item that is considered as a replacement for the original one.

  @param thd     reference to the global context of the query thread
  @param ref     pointer to Item* variable where pointer to resulting "fixed"
                 item is to be assigned

  @note
    Let T0(e)/T1(e) be the value of not_null_tables(e) when e is used on
    a predicate/function level. Then it's easy to show that:
    @verbatim
      T0(e IN(e1,...,en))     = union(T1(e),intersection(T1(ei)))
      T1(e IN(e1,...,en))     = union(T1(e),intersection(T1(ei)))
      T0(e NOT IN(e1,...,en)) = union(T1(e),union(T1(ei)))
      T1(e NOT IN(e1,...,en)) = union(T1(e),intersection(T1(ei)))
    @endverbatim

  @retval
    0   ok
  @retval
    1   got error
*/

bool Item_func_in::fix_fields(THD *thd, Item **ref) {
  if (Item_func_opt_neg::fix_fields(thd, ref)) return true;
  update_not_null_tables();
  return false;
}

void Item_func_in::fix_after_pullout(Query_block *parent_query_block,
                                     Query_block *removed_query_block) {
  Item_func_opt_neg::fix_after_pullout(parent_query_block, removed_query_block);
  update_not_null_tables();
}

bool Item_func_in::resolve_type(THD *thd) {
  if (Item_func_opt_neg::resolve_type(thd)) return true;
  /* true <=> arguments values will be compared as DATETIMEs. */
  bool compare_as_datetime = false;
  Item *date_arg = nullptr;
  bool compare_as_json = (args[0]->data_type() == MYSQL_TYPE_JSON);

  left_result_type = args[0]->result_type();
  Item_result cmp_type = STRING_RESULT;

  const uint found_types = collect_cmp_types(args, arg_count, true);
  if (found_types == 0) return true;

  m_values_are_const = true;
  m_need_populate = false;
  Item **arg_end = args + arg_count;
  for (Item **arg = args + 1; arg != arg_end; arg++) {
    compare_as_json |= (arg[0]->data_type() == MYSQL_TYPE_JSON);

    if (!(*arg)->const_for_execution()) {
      m_values_are_const = false;
      // @todo - rewrite as has_subquery() ???
      if ((*arg)->real_item()->type() == Item::SUBQUERY_ITEM)
        dep_subq_in_list = true;
      break;
    } else {
      // Some items may change per execution - trigger repopulation
      if (!(*arg)->const_item()) {
        m_need_populate = true;
      }
    }
  }
  if (compare_as_json) {
    for (Item **arg = args + 1; arg != arg_end; arg++) {
      (*arg)->mark_json_as_scalar();
    }
  }
  uint type_cnt = 0;
  for (uint i = 0; i <= (uint)DECIMAL_RESULT; i++) {
    if (found_types & (1U << i)) {
      (type_cnt)++;
      cmp_type = (Item_result)i;
    }
  }

  /*
    Set cmp_context of all arguments. This prevents
    Item_field::equal_fields_propagator() from transforming a zerofill integer
    argument into a string constant. Such a change would require rebuilding
    cmp_items.
   */
  for (Item **arg = args + 1; arg != arg_end; arg++) {
    (*arg)->cmp_context =
        item_cmp_type(left_result_type, arg[0]->result_type());
  }
  max_length = 1;

  if (m_const_array != nullptr) {
    /*
      A previously allocated const array exists; so we are now allocating in the
      execution MEM_ROOT a new array only for this execution; delete the old
      one now; take note to delete the new one in cleanup().
      @see substitute_gc_expression().
    */
    first_resolve_call = false;
    m_need_populate = true;
    cleanup_arrays();
  } else {
    for (uint i = 0; i <= (uint)DECIMAL_RESULT + 1; i++) {
      if (cmp_items[i]) {  // Same thing
        first_resolve_call = false;
        m_need_populate = true;
        cleanup_arrays();
        break;
      }
    }
  }
  /*
    First conditions for bisection to be possible:
     1. All types are similar, and
     2. All expressions in <in value list> are const (for execution)
     3. No JSON is compared (in such case universal JSON comparator is used)
  */
  bool bisection_possible = type_cnt == 1 &&       // 1
                            m_values_are_const &&  // 2
                            !compare_as_json;      // 3
  if (bisection_possible) {
    /*
      In the presence of NULLs, the correct result of evaluating this item
      must be UNKNOWN or FALSE. To achieve that:
      - If type is scalar, we can use bisection and the "have_null" boolean.
      - If type is ROW, we will need to scan all of <in value list> when
        searching, so bisection is impossible. Unless:
        3. UNKNOWN and FALSE are equivalent results
        4. Neither left expression nor <in value list> contain any NULL value
      */

    if (cmp_type == ROW_RESULT &&
        !((ignore_unknown() && !negated) ||                     // 3
          (!list_contains_null() && !args[0]->is_nullable())))  // 4
      bisection_possible = false;
  }

  if (type_cnt == 1 && !compare_as_json) {
    if (cmp_type == STRING_RESULT &&
        agg_arg_charsets_for_comparison(cmp_collation, args, arg_count))
      return true;
    /*
      When comparing rows create the row comparator object beforehand to ease
      the DATETIME comparison detection procedure.
    */
    if (cmp_type == ROW_RESULT) {
      assert(first_resolve_call);
      cmp_item_row *cmp = new (thd->mem_root) cmp_item_row(thd, args[0]);
      if (cmp == nullptr) return true;
      if (bisection_possible) {
        m_const_array =
            new (thd->mem_root) in_row(thd->mem_root, arg_count - 1, cmp);
        if (m_const_array == nullptr) return true;
        if (down_cast<in_row *>(m_const_array)
                ->allocate(thd->mem_root, args[0], arg_count - 1)) {
          return true;
        }
      } else {
        cmp_items[ROW_RESULT] = cmp;
      }
    }
    /* All DATE/DATETIME fields/functions has the STRING result type. */
    if (cmp_type == STRING_RESULT || cmp_type == ROW_RESULT) {
      bool datetime_found = false;
      const uint num_cols = args[0]->cols();
      // Proper JSON comparison isn't yet supported if JSON is within a ROW
      bool json_row_warning_printed = (num_cols == 1);

      for (uint col = 0; col < num_cols; col++) {
        /*
          Check that all items to be compared has the STRING result type and at
          least one of them is a DATE/DATETIME item.
        */
        for (Item **arg = args; arg != arg_end; arg++) {
          Item *itm =
              ((cmp_type == STRING_RESULT) ? arg[0]
                                           : arg[0]->element_index(col));
          if (itm->data_type() == MYSQL_TYPE_JSON &&
              !json_row_warning_printed) {
            json_row_warning_printed = true;
            push_warning_printf(
                current_thd, Sql_condition::SL_WARNING, ER_NOT_SUPPORTED_YET,
                ER_THD(current_thd, ER_NOT_SUPPORTED_YET),
                "comparison of JSON within a ROW in the IN operator");
          }
          if (itm->result_type() != STRING_RESULT) {
            // If the warning wasn't printed yet, we need to continue scanning
            // through args to check whether one of them is JSON
            if (json_row_warning_printed)
              break;
            else
              continue;
          } else if (itm->is_temporal_with_date()) {
            datetime_found = true;
            /*
              Internally all DATE/DATETIME values are converted to the DATETIME
              type. So try to find a DATETIME item to issue correct warnings.
            */
            if (!date_arg)
              date_arg = itm;
            else if (itm->data_type() == MYSQL_TYPE_DATETIME) {
              date_arg = itm;
              /* All arguments are already checked to have the STRING result. */
              if (cmp_type == STRING_RESULT) break;
            }
          }
        }
      }
      compare_as_datetime = (datetime_found && cmp_type != ROW_RESULT);
    }
  }

  if (bisection_possible) {
    if (compare_as_datetime) {
      m_const_array = new (thd->mem_root)
          in_datetime(thd->mem_root, date_arg, arg_count - 1);
      if (m_const_array == nullptr) return true;
    } else {
      /*
        IN must compare INT columns and constants as int values (the same
        way as equality does).
        So we must check here if the column on the left and all the constant
        values on the right can be compared as integers and adjust the
        comparison type accordingly.
      */
      bool datetime_as_longlong = false;
      if (args[0]->real_item()->type() == FIELD_ITEM &&
          thd->lex->sql_command != SQLCOM_CREATE_VIEW &&
          thd->lex->sql_command != SQLCOM_SHOW_CREATE &&
          cmp_type != INT_RESULT) {
        Item_field *field_item = (Item_field *)(args[0]->real_item());
        if (field_item->field->can_be_compared_as_longlong()) {
          bool all_converted = true;
          for (Item **arg = args + 1; arg != arg_end; arg++) {
            bool converted;
            if (convert_constant_item(thd, field_item, &arg[0], &converted))
              return true;
            all_converted &= converted;
          }
          if (all_converted) {
            cmp_type = INT_RESULT;
            datetime_as_longlong = field_item->is_temporal() &&
                                   field_item->data_type() != MYSQL_TYPE_YEAR;
          }
        }
      }
      switch (cmp_type) {
        case STRING_RESULT:
          m_const_array = new (thd->mem_root)
              in_string(thd->mem_root, arg_count - 1, cmp_collation.collation);
          break;
        case INT_RESULT:
          m_const_array =
              datetime_as_longlong
                  ? args[0]->data_type() == MYSQL_TYPE_TIME
                        ? static_cast<in_vector *>(
                              new (thd->mem_root) in_time_as_longlong(
                                  thd->mem_root, arg_count - 1))
                        : static_cast<in_vector *>(
                              new (thd->mem_root) in_datetime_as_longlong(
                                  thd->mem_root, arg_count - 1))
                  : static_cast<in_vector *>(new (thd->mem_root) in_longlong(
                        thd->mem_root, arg_count - 1));
          break;
        case REAL_RESULT:
          m_const_array =
              new (thd->mem_root) in_double(thd->mem_root, arg_count - 1);
          break;
        case ROW_RESULT:
          /*
            The row comparator was created at the beginning.
          */
          break;
        case DECIMAL_RESULT:
          m_const_array =
              new (thd->mem_root) in_decimal(thd->mem_root, arg_count - 1);
          break;
        default:
          assert(0);
      }
      if (m_const_array == nullptr) return true;
    }
    /*
      convert_constant_item() or one of its descendants might set an error
      without correct propagation of return value. Bail out if error.
      (Should be an assert).
    */
    if (thd->is_error()) return true;
  } else {
    if (compare_as_json) {
      // Use JSON comparator for all comparison types
      for (uint i = 0; i <= (uint)DECIMAL_RESULT; i++) {
        if (/* (found_types & (1U << i) && */ !cmp_items[i]) {
          cmp_items[i] = make_cmp_item_json(thd->mem_root);
          if (cmp_items[i] == nullptr) return true; /* purecov: inspected */
        }
      }
    } else if (compare_as_datetime) {
      if (!(cmp_items[STRING_RESULT] =
                new (thd->mem_root) cmp_item_datetime(date_arg)))
        return true;
    } else {
      for (uint i = 0; i <= (uint)DECIMAL_RESULT; i++) {
        if (found_types & (1U << i) && !cmp_items[i]) {
          if ((Item_result)i == STRING_RESULT &&
              agg_arg_charsets_for_comparison(cmp_collation, args, arg_count))
            return true;
          if (!cmp_items[i] &&
              !(cmp_items[i] = cmp_item::new_comparator(
                    thd, (Item_result)i, args[0], cmp_collation.collation)))
            return true;
        }
      }
    }
  }
  if (thd->lex->is_view_context_analysis()) return false;

  if (m_const_array != nullptr && m_values_are_const && !m_need_populate) {
    have_null = m_const_array->fill(args + 1, arg_count - 1);
    m_populated = true;
  }
  Opt_trace_object(&thd->opt_trace)
      .add("IN_uses_bisection", bisection_possible);
  return false;
}

void Item_func_in::update_used_tables() {
  Item_func::update_used_tables();
  update_not_null_tables();
}

void Item_func_in::print(const THD *thd, String *str,
                         enum_query_type query_type) const {
  str->append('(');
  args[0]->print(thd, str, query_type);
  if (negated) str->append(STRING_WITH_LEN(" not"));
  str->append(STRING_WITH_LEN(" in ("));
  print_args(thd, str, 1, query_type);
  str->append(STRING_WITH_LEN("))"));
}

/*
  Evaluate the function and return its value.

  SYNOPSIS
    val_int()

  DESCRIPTION
    Evaluate the function and return its value.

  IMPLEMENTATION
    If the array object is defined then the value of the function is
    calculated by means of this array.
    Otherwise several cmp_item objects are used in order to do correct
    comparison of left expression and an expression from the values list.
    One cmp_item object correspond to one used comparison type. Left
    expression can be evaluated up to number of different used comparison
    types. A bit mapped variable value_added_map is used to check whether
    the left expression already was evaluated for a particular result type.
    Result types are mapped to it according to their integer values i.e.
    STRING_RESULT is mapped to bit 0, REAL_RESULT to bit 1, so on.

  RETURN
    Value of the function
*/

longlong Item_func_in::val_int() {
  cmp_item *in_item;
  assert(fixed);
  uint value_added_map = 0;
  if (m_const_array != nullptr) {
    if (!m_populated) {
      have_null = m_const_array->fill(args + 1, arg_count - 1);
      if (current_thd->is_error()) return error_int();
      m_populated = true;
    }

    const bool tmp = m_const_array->find_item(args[0]);
    /*
      NULL on left -> UNKNOWN.
      Found no match, and NULL on right -> UNKNOWN.
      NULL on right can never give a match, as it is not stored in
      array.
      See also the 'bisection_possible' variable in resolve_type().
    */
    null_value = args[0]->null_value || (!tmp && have_null);
    return (longlong)(!null_value && tmp != negated);
  }

  if ((null_value = args[0]->real_item()->type() == NULL_ITEM)) return 0;

  have_null = false;
  for (uint i = 1; i < arg_count; i++) {
    if (args[i]->real_item()->type() == NULL_ITEM) {
      have_null = true;
      continue;
    }
    const Item_result cmp_type =
        item_cmp_type(left_result_type, args[i]->result_type());
    in_item = cmp_items[(uint)cmp_type];
    assert(in_item);
    if (!(value_added_map & (1U << (uint)cmp_type))) {
      in_item->store_value(args[0]);
      value_added_map |= 1U << (uint)cmp_type;
      if (current_thd->is_error()) return error_int();
    }
    const int rc = in_item->cmp(args[i]);
    if (rc == false) return (longlong)(!negated);
    have_null |= (rc == UNKNOWN);
    if (current_thd->is_error()) return error_int();
  }

  null_value = have_null;
  return (longlong)(!null_value && negated);
}

bool Item_func_in::populate_bisection(THD *) {
  assert(!m_populated);
  have_null = m_const_array->fill(args + 1, arg_count - 1);
  m_populated = true;
  return false;
}

void Item_func_in::cleanup_arrays() {
  m_populated = false;
  if (m_const_array != nullptr) ::destroy_at(m_const_array);
  m_const_array = nullptr;
  for (uint i = 0; i <= (uint)DECIMAL_RESULT + 1; i++) {
    if (cmp_items[i] != nullptr) {
      ::destroy_at(cmp_items[i]);
      cmp_items[i] = nullptr;
    }
  }
}

void Item_func_in::cleanup() {
  DBUG_TRACE;
  Item_int_func::cleanup();
  // Trigger re-population in next execution (if bisection is used)
  if (m_need_populate) {
    if (m_const_array != nullptr) m_const_array->cleanup();
    m_populated = false;
  }

  if (!first_resolve_call) {
    /*
      2nd and next calls to resolve_type() allocated in execution MEM_ROOT; at
      the end of this execution we must delete the objects, as their storage
      will soon be freed.
      On the opposite, the objects allocated by the first call are in the
      persistent MEM_ROOT and, if they have not been deleted and replaced by
      some 2nd call, they are to be deleted by the destructor, no earlier -
      they may serve for multiple executions.
    */
    cleanup_arrays();
  }
}

Item_func_in::~Item_func_in() { cleanup_arrays(); }

Item_cond::Item_cond(THD *thd, Item_cond *item)
    : Item_bool_func(thd, item), abort_on_null(item->abort_on_null) {
  /*
    item->list will be copied by copy_andor_arguments() call
  */
}

/**
  Ensure that all expressions involved in conditions are boolean functions.
  Specifically, change <non-bool-expr> to (0 <> <non-bool-expr>)

  @param pc    Parse context, including memroot for Item construction
  @param item  Any expression, if not a boolean expression, convert it

  @returns = NULL  Error
           <> NULL A boolean expression, possibly constructed as described above

  @note Due to the special conditions of a MATCH expression (it is both a
        function returning a floating point value and it may be used
        standalone in the WHERE clause), it is wrapped inside a special
        Item_func_match_predicate, instead of forming a non-equality.
*/
Item *make_condition(Parse_context *pc, Item *item) {
  assert(!item->is_bool_func());

  Item *predicate;
  if (!is_function_of_type(item, Item_func::FT_FUNC)) {
    Item *const item_zero = new (pc->mem_root) Item_int(0);
    if (item_zero == nullptr) return nullptr;
    predicate = new (pc->mem_root) Item_func_ne(item_zero, item);
    predicate->marker = Item::MARKER_IMPLICIT_NE_ZERO;
  } else {
    predicate = new (pc->mem_root) Item_func_match_predicate(item);
  }
  return predicate;
}

/**
  Contextualization for Item_cond functional items

  Item_cond successors use Item_cond::list instead of Item_func::args
  and Item_func::arg_count, so we can't itemize parse-time Item_cond
  objects by forwarding a contextualization process to the parent Item_func
  class: we need to overload this function to run a contextualization
  the Item_cond::list items.
*/
bool Item_cond::do_itemize(Parse_context *pc, Item **res) {
  if (skip_itemize(res)) return false;
  if (super::do_itemize(pc, res)) return true;

  List_iterator<Item> li(list);
  Item *item;
  while ((item = li++)) {
    if (item->itemize(pc, &item)) return true;
    if (!item->is_bool_func()) {
      item = make_condition(pc, item);
      if (item == nullptr) return true;
    }
    li.replace(item);
  }
  return false;
}

void Item_cond::copy_andor_arguments(THD *thd, Item_cond *item) {
  List_iterator_fast<Item> li(item->list);
  while (Item *it = li++) {
    assert(it->real_item());  // Sanity check (no dangling 'ref')
    list.push_back(it->copy_andor_structure(thd));
  }
}

bool Item_cond::fix_fields(THD *thd, Item **ref) {
  assert(!fixed);
  List_iterator<Item> li(list);
  Item *item;
  Query_block *select = thd->lex->current_query_block();

  auto func_type = functype();
  assert(func_type == COND_AND_FUNC || func_type == COND_OR_FUNC);
  // For semi-join flattening, indicate that we're traversing an AND, or an OR:
  Condition_context CCT(select, func_type == COND_AND_FUNC
                                    ? enum_condition_context::ANDS
                                    : enum_condition_context::ANDS_ORS);

  uchar buff[sizeof(char *)];  // Max local vars in function
  used_tables_cache = 0;

  if (func_type == COND_AND_FUNC && ignore_unknown())
    not_null_tables_cache = 0;
  else
    not_null_tables_cache = ~(table_map)0;

  if (check_stack_overrun(thd, STACK_MIN_SIZE, buff))
    return true;  // Fatal error flag is set!
  Item *new_item = nullptr;
  bool remove_condition = false, can_remove_cond = true;

  /*
    The following optimization reduces the depth of an AND-OR tree.
    E.g. a WHERE clause like
      F1 AND (F2 AND (F2 AND F4))
    is parsed into a tree with the same nested structure as defined
    by braces. This optimization will transform such tree into
      AND (F1, F2, F3, F4).
    Trees of OR items are flattened as well:
      ((F1 OR F2) OR (F3 OR F4))   =>   OR (F1, F2, F3, F4)
    Items for removed AND/OR levels will dangle until the death of the
    entire statement.
    The optimization is currently prepared statements and stored procedures
    friendly as it doesn't allocate any memory and its effects are durable
    (i.e. do not depend on PS/SP arguments).
  */
  while ((item = li++)) {
    Item_cond *cond;
    while (item->type() == Item::COND_ITEM &&
           (cond = down_cast<Item_cond *>(item)) &&
           cond->functype() == func_type &&
           !cond->list.is_empty()) {  // Identical function
      li.replace(cond->list);
      cond->list.clear();
      item = *li.ref();  // new current item
    }
    if (ignore_unknown()) item->apply_is_true();

    // item can be substituted in fix_fields
    if ((!item->fixed && item->fix_fields(thd, li.ref())) ||
        (item = *li.ref())->check_cols(1))
      return true; /* purecov: inspected */

    /*
      We optimize away the basic constant items here. If an AND condition
      has "cond AND FALSE", then the entire condition is collapsed and
      replaced with an ALWAYS FALSE item. Similarly, if an OR
      condition has "cond OR TRUE", then the entire condition is replaced
      with an ALWAYS TRUE item. Else only the const item is removed.
    */
    /*
      Make a note if the expression has been created by IN to EXISTS
      transformation. If so we cannot remove the entire condition.
     */
    if (item->created_by_in2exists()) {
      remove_condition = false;
      can_remove_cond = false;
    }
    /*
      If it is indicated that we can remove the condition because
      of a possible ALWAYS FALSE or ALWAYS TRUE condition, continue to
      just call fix_fields on the items.
    */
    if (remove_condition) continue;

    /*
      Do this optimization if fix_fields is allowed to change the condition
      and if this is the first execution.
      Check if the const item does not contain param's, SP args etc.  We also
      cannot optimize conditions if it's a view. The condition has to be a
      top_level_item to get optimized as they can have only two return values,
      true or false. A non-top_level_item can have true, false and NULL return.
      Fulltext funcs cannot be removed as ftfunc_list stores the list
      of pointers to these functions. The list gets accessed later
      in the call to init_ftfuncs() from JOIN::reset.
      TODO: Lift this restriction once init_ft_funcs gets moved to JOIN::exec
    */
    if (ref != nullptr && select->first_execution && item->const_item() &&
        !item->walk(&Item::is_non_const_over_literals, enum_walk::POSTFIX,
                    nullptr) &&
        !thd->lex->is_view_context_analysis() && ignore_unknown() &&
        !select->has_ft_funcs() && can_remove_cond) {
      if (remove_const_conds(thd, item, &new_item)) return true;
      /*
        If a new_item is returned, indicate that all the items can be removed
        from the list.
        Else remove only the current element in the list.
      */
      if (new_item != nullptr) {
        remove_condition = true;
        continue;
      }
      Cleanup_after_removal_context ctx(select);
      item->walk(&Item::clean_up_after_removal, walk_options,
                 pointer_cast<uchar *>(&ctx));
      li.remove();
      continue;
    }
    // AND/OR take booleans
    if (item->propagate_type(thd, MYSQL_TYPE_LONGLONG)) return true;

    used_tables_cache |= item->used_tables();

    if (func_type == COND_AND_FUNC && ignore_unknown())
      not_null_tables_cache |= item->not_null_tables();
    else
      not_null_tables_cache &= item->not_null_tables();
    add_accum_properties(item);
    set_nullable(is_nullable() || item->is_nullable());
  }

  /*
    Remove all the items from the list if it was indicated that we have
    an ALWAYS TRUE or an ALWAYS FALSE condition. Replace with the new
    TRUE or FALSE condition.
  */
  if (remove_condition) {
    new_item->fix_fields(thd, ref);
    used_tables_cache = 0;
    not_null_tables_cache = 0;
    li.rewind();
    while ((item = li++)) {
      Cleanup_after_removal_context ctx(select);
      item->walk(&Item::clean_up_after_removal, walk_options,
                 pointer_cast<uchar *>(&ctx));
      li.remove();
    }
    const Prepared_stmt_arena_holder ps_arena_holder(thd);
    list.push_front(new_item);
  }

  select->cond_count += list.elements;

  if (resolve_type(thd)) return true;

  fixed = true;
  return false;
}

/**

  Remove constant conditions over literals.

  If an item is a trivial condition like a literal or an operation
  on literal(s), we evaluate the item and based on the result, decide
  if the entire condition can be replaced with an ALWAYS TRUE or
  ALWAYS FALSE item.
  For every constant condition, if the result is true, then
  for an OR condition we return an ALWAYS TRUE item. For an AND
  condition we return NULL if its not the only argument in the
  condition.
  If the result is false, for an AND condition we return
  an ALWAYS FALSE item and for an OR condition we return NULL if
  its not the only argument in the condition.

  @param thd                  Current thread
  @param item                 Item which needs to be evaluated
  @param[out] new_item        return new_item, if created

  @return               true, if error
                        false, on success
*/

bool Item_cond::remove_const_conds(THD *thd, Item *item, Item **new_item) {
  assert(item->const_item());

  const bool and_condition = functype() == Item_func::COND_AND_FUNC;

  bool cond_value = true;

  /* Push ignore / strict error handler */
  Ignore_error_handler ignore_handler;
  Strict_error_handler strict_handler;
  if (thd->lex->is_ignore())
    thd->push_internal_handler(&ignore_handler);
  else if (thd->is_strict_mode())
    thd->push_internal_handler(&strict_handler);

  const bool err = eval_const_cond(thd, item, &cond_value);
  /* Pop ignore / strict error handler */
  if (thd->lex->is_ignore() || thd->is_strict_mode())
    thd->pop_internal_handler();

  if (err) return true;

  if (cond_value) {
    if (!and_condition || (argument_list()->elements == 1)) {
      const Prepared_stmt_arena_holder ps_arena_holder(thd);
      *new_item = new Item_func_true();
      if (*new_item == nullptr) return true;
    }
    return false;
  } else {
    if (and_condition || (argument_list()->elements == 1)) {
      const Prepared_stmt_arena_holder ps_arena_holder(thd);
      *new_item = new Item_func_false();
      if (*new_item == nullptr) return true;
    }
    return false;
  }
}

void Item_cond::fix_after_pullout(Query_block *parent_query_block,
                                  Query_block *removed_query_block) {
  List_iterator<Item> li(list);
  Item *item;

  used_tables_cache = get_initial_pseudo_tables();

  if (functype() == COND_AND_FUNC && ignore_unknown())
    not_null_tables_cache = 0;
  else
    not_null_tables_cache = ~(table_map)0;

  while ((item = li++)) {
    item->fix_after_pullout(parent_query_block, removed_query_block);
    used_tables_cache |= item->used_tables();
    if (functype() == COND_AND_FUNC && ignore_unknown())
      not_null_tables_cache |= item->not_null_tables();
    else
      not_null_tables_cache &= item->not_null_tables();
  }
}

bool Item_cond::eq(const Item *item) const {
  if (this == item) return true;
  if (item->type() != COND_ITEM) return false;
  const Item_cond *item_cond = down_cast<const Item_cond *>(item);
  if (functype() != item_cond->functype() ||
      list.elements != item_cond->list.elements ||
      strcmp(func_name(), item_cond->func_name()) != 0)
    return false;
  // Item_cond never uses "args". Inspect "list" instead.
  assert(arg_count == 0 && item_cond->arg_count == 0);
  return std::equal(
      list.begin(), list.end(), item_cond->list.begin(),
      [](const Item &i1, const Item &i2) { return ItemsAreEqual(&i1, &i2); });
}

bool Item_cond::walk(Item_processor processor, enum_walk walk, uchar *arg) {
  if ((walk & enum_walk::PREFIX) && (this->*processor)(arg)) return true;

  List_iterator_fast<Item> li(list);
  Item *item;
  while ((item = li++)) {
    if (item->walk(processor, walk, arg)) return true;
  }
  return (walk & enum_walk::POSTFIX) && (this->*processor)(arg);
}

/**
  Transform an Item_cond object with a transformer callback function.

    The function recursively applies the transform method to each
    member item of the condition list.
    If the call of the method for a member item returns a new item
    the old item is substituted for a new one.
    After this the transformer is applied to the root node
    of the Item_cond object.
*/

Item *Item_cond::transform(Item_transformer transformer, uchar *arg) {
  List_iterator<Item> li(list);
  Item *item;
  while ((item = li++)) {
    Item *new_item = item->transform(transformer, arg);
    if (new_item == nullptr) return nullptr; /* purecov: inspected */
    if (new_item != item) li.replace(new_item);
  }
  return Item_func::transform(transformer, arg);
}

/**
  Compile Item_cond object with a processor and a transformer
  callback functions.

    First the function applies the analyzer to the root node of
    the Item_func object. Then if the analyzer succeeeds (returns true)
    the function recursively applies the compile method to member
    item of the condition list.
    If the call of the method for a member item returns a new item
    the old item is substituted for a new one.
    After this the transformer is applied to the root node
    of the Item_cond object.
*/

Item *Item_cond::compile(Item_analyzer analyzer, uchar **arg_p,
                         Item_transformer transformer, uchar *arg_t) {
  if (!(this->*analyzer)(arg_p)) return this;

  List_iterator<Item> li(list);
  Item *item;
  while ((item = li++)) {
    /*
      The same parameter value of arg_p must be passed
      to analyze any argument of the condition formula.
    */
    uchar *arg_v = *arg_p;
    Item *new_item = item->compile(analyzer, &arg_v, transformer, arg_t);
    if (new_item == nullptr) return nullptr;
    if (new_item != item) current_thd->change_item_tree(li.ref(), new_item);
  }
  // strange to call transform(): each argument will thus have the transformer
  // called twice on it (in compile() above and Item_func::transform below)??
  return Item_func::transform(transformer, arg_t);
}

void Item_cond::traverse_cond(Cond_traverser traverser, void *arg,
                              traverse_order order) {
  List_iterator<Item> li(list);
  Item *item;

  switch (order) {
    case (PREFIX):
      (*traverser)(this, arg);
      while ((item = li++)) {
        item->traverse_cond(traverser, arg, order);
      }
      (*traverser)(nullptr, arg);
      break;
    case (POSTFIX):
      while ((item = li++)) {
        item->traverse_cond(traverser, arg, order);
      }
      (*traverser)(this, arg);
  }
}

/**
  Move SUM items out from item tree and replace with reference.

  The split is done to get a unique item for each SUM function
  so that we can easily find and calculate them.
  (Calculation done by update_sum_func() and copy_sum_funcs() in
  sql_select.cc)

  @note
    This function is run on all expression (SELECT list, WHERE, HAVING etc)
    that have or refer (HAVING) to a SUM expression.
*/

bool Item_cond::split_sum_func(THD *thd, Ref_item_array ref_item_array,
                               mem_root_deque<Item *> *fields) {
  List_iterator<Item> li(list);
  Item *item;
  while ((item = li++)) {
    if (item->split_sum_func2(thd, ref_item_array, fields, li.ref(), true)) {
      return true;
    }
  }
  return false;
}

void Item_cond::update_used_tables() {
  List_iterator_fast<Item> li(list);
  Item *item;

  used_tables_cache = 0;
  m_accum_properties = 0;

  if (functype() == COND_AND_FUNC && ignore_unknown())
    not_null_tables_cache = 0;
  else
    not_null_tables_cache = ~(table_map)0;

  while ((item = li++)) {
    item->update_used_tables();
    used_tables_cache |= item->used_tables();
    add_accum_properties(item);
    if (functype() == COND_AND_FUNC && ignore_unknown())
      not_null_tables_cache |= item->not_null_tables();
    else
      not_null_tables_cache &= item->not_null_tables();
  }
}

void Item_cond::print(const THD *thd, String *str,
                      enum_query_type query_type) const {
  str->append('(');
  bool first = true;
  for (auto &item : list) {
    if (!first) {
      str->append(' ');
      str->append(func_name());
      str->append(' ');
    }
    item.print(thd, str, query_type);
    first = false;
  }
  str->append(')');
}

bool Item_cond::truth_transform_arguments(THD *thd, Bool_test test) {
  assert(test == BOOL_NEGATED);
  List_iterator<Item> li(list);
  Item *item;
  while ((item = li++)) /* Apply not transformation to the arguments */
  {
    Item *new_item = item->truth_transformer(thd, test);
    if (!new_item) {
      if (!(new_item = new Item_func_not(item))) return true;
    }
    li.replace(new_item);
  }
  return false;
}

float Item_cond_and::get_filtering_effect(THD *thd, table_map filter_for_table,
                                          table_map read_tables,
                                          const MY_BITMAP *fields_to_ignore,
                                          double rows_in_table) {
  if (!(used_tables() & filter_for_table))
    return COND_FILTER_ALLPASS;  // No conditions below this apply to the table

  float filter = COND_FILTER_ALLPASS;
  List_iterator<Item> it(list);
  Item *item;

  /*
    Calculated as "Conjunction of independent events":
       P(A and B ...) = P(A) * P(B) * ...
  */
  while ((item = it++))
    filter *= item->get_filtering_effect(thd, filter_for_table, read_tables,
                                         fields_to_ignore, rows_in_table);
  return filter;
}

/**
  Evaluation of AND(expr, expr, expr ...).

  @note
    abort_if_null is set for AND expressions for which we don't care if the
    result is NULL or 0. This is set for:
    - WHERE clause
    - HAVING clause
    - IF(expression)

  @retval
    1  If all expressions are true
  @retval
    0  If all expressions are false or if we find a NULL expression and
       'abort_on_null' is set.
  @retval
    NULL if all expression are either 1 or NULL
*/

longlong Item_cond_and::val_int() {
  assert(fixed);
  List_iterator_fast<Item> li(list);
  Item *item;
  null_value = false;
  while ((item = li++)) {
    if (!item->val_bool()) {
      if (ignore_unknown() || !(null_value = item->null_value))
        return 0;  // return false
    }
    if (current_thd->is_error()) return error_int();
  }
  return null_value ? 0 : 1;
}

float Item_cond_or::get_filtering_effect(THD *thd, table_map filter_for_table,
                                         table_map read_tables,
                                         const MY_BITMAP *fields_to_ignore,
                                         double rows_in_table) {
  if (!(used_tables() & filter_for_table))
    return COND_FILTER_ALLPASS;  // No conditions below this apply to the table

  float filter = 0.0f;
  List_iterator<Item> it(list);
  Item *item;
  while ((item = it++)) {
    const float cur_filter = item->get_filtering_effect(
        thd, filter_for_table, read_tables, fields_to_ignore, rows_in_table);
    /*
      Calculated as "Disjunction of independent events":
      P(A or B)  = P(A) + P(B) - P(A) * P(B)

      If any of the ORed predicates has a filtering effect of
      COND_FILTER_ALLPASS, the end result is COND_FILTER_ALLPASS. This is as
      expected since COND_FILTER_ALLPASS means that a) the predicate has
      no filtering effect at all, or b) the predicate's filtering
      effect is unknown. In both cases, the only meaningful result is
      for OR to produce COND_FILTER_ALLPASS.
    */
    filter = filter + cur_filter - (filter * cur_filter);
  }
  return filter;
}

longlong Item_cond_or::val_int() {
  assert(fixed);
  List_iterator_fast<Item> li(list);
  Item *item;
  null_value = false;
  while ((item = li++)) {
    if (item->val_bool()) {
      null_value = false;
      return 1;
    }
    if (item->null_value) null_value = true;
    if (current_thd->is_error()) return error_int();
  }
  return 0;
}

void Item_func_isnull::update_used_tables() {
  args[0]->update_used_tables();
  set_accum_properties(args[0]);
  if (!args[0]->is_nullable()) {
    used_tables_cache = 0;
  } else {
    used_tables_cache = args[0]->used_tables();
    if (!const_item()) cache_used = false;
  }

  not_null_tables_cache = 0;
  if (null_on_null && !const_item())
    not_null_tables_cache |= args[0]->not_null_tables();
}

float Item_func_isnull::get_filtering_effect(THD *thd,
                                             table_map filter_for_table,
                                             table_map read_tables,
                                             const MY_BITMAP *fields_to_ignore,
                                             double rows_in_table) {
  if (cache_used) {
    return cached_value ? COND_FILTER_ALLPASS : 0.0f;
  }

  const Item_field *fld = contributes_to_filter(
      thd, read_tables, filter_for_table, fields_to_ignore);
  if (!fld) return COND_FILTER_ALLPASS;

  const double selectivity = get_histogram_selectivity(
      thd, *fld->field, histograms::enum_operator::IS_NULL, *this);

  return selectivity == kUndefinedSelectivity
             ? fld->get_cond_filter_default_probability(rows_in_table,
                                                        COND_FILTER_EQUALITY)
             : selectivity;
}

bool Item_func_isnull::fix_fields(THD *thd, Item **ref) {
  if (super::fix_fields(thd, ref)) return true;
  if (args[0]->type() == Item::FIELD_ITEM) {
    Field *const field = down_cast<Item_field *>(args[0])->field;
    /*
      Fix to replace 'NULL' dates with '0' (shreeve@uci.edu)
      See BUG#12594011
      Documentation says that
      SELECT datetime_notnull d FROM t1 WHERE d IS NULL
      shall return rows where d=='0000-00-00'

      Thus, for DATE and DATETIME columns defined as NOT NULL,
      "date_notnull IS NULL" has to be modified to
      "date_notnull IS NULL OR date_notnull == 0" (if outer join)
      "date_notnull == 0"                         (otherwise)

      It's a legacy convenience of the user, but it also causes problems as
      it's not SQL-compliant. So, to keep it confined to the above type of
      query only, we do not enable this behaviour when IS NULL
      - is internally created (it must really mean IS NULL)
        * IN-to-EXISTS creates IS NULL items but either they wrap Item_ref (so
        the if() above skips them) or are not created if not nullable.
        * fold_or_simplify() creates IS NULL items but not if not nullable.
      - is not in WHERE (e.g. is in ON)
      - isn't reachable from top of WHERE through a chain of AND
      - is IS NOT NULL (Item_func_isnotnull doesn't use this fix_fields).
      - is inside expressions (except the AND case above).

      Moreover, we do this transformation at first resolution only, and
      permanently. Indeed, at second resolution, it's not necessary and it would
      even cause a problem (as we can't distinguish JOIN ON from WHERE
      anymore).
    */
    if (thd->lex->current_query_block()->resolve_place ==
            Query_block::RESOLVE_CONDITION &&
        thd->lex->current_query_block()->condition_context ==
            enum_condition_context::ANDS &&
        thd->lex->current_query_block()->first_execution &&
        (field->type() == MYSQL_TYPE_DATE ||
         field->type() == MYSQL_TYPE_DATETIME) &&
        field->is_flag_set(NOT_NULL_FLAG)) {
      const Prepared_stmt_arena_holder ps_arena_holder(thd);
      Item *item0 = new Item_int(0);
      if (item0 == nullptr) return true;
      Item *new_cond = new Item_func_eq(args[0], item0);
      if (new_cond == nullptr) return true;

      if (args[0]->is_outer_field()) {
        // outer join: transform "col IS NULL" to "col IS NULL or col=0"
        new_cond = new Item_cond_or(new_cond, this);
        if (new_cond == nullptr) return true;
      } else {
        // not outer join: transform "col IS NULL" to "col=0" (don't add the
        // OR IS NULL part: it wouldn't change result but prevent index use)
      }
      *ref = new_cond;
      return new_cond->fix_fields(thd, ref);
    }

    /*
      Handles this special case for some ODBC applications:
      They are requesting the row that was just updated with an auto_increment
      value with this construct:

      SELECT * FROM table_name WHERE auto_increment_column IS NULL

      This will be changed to:

      SELECT * FROM table_name WHERE auto_increment_column = LAST_INSERT_ID()
    */
    if (thd->lex->current_query_block()->where_cond() == this &&
        (thd->variables.option_bits & OPTION_AUTO_IS_NULL) != 0 &&
        field->is_flag_set(AUTO_INCREMENT_FLAG) &&
        !field->table->is_nullable()) {
      const Prepared_stmt_arena_holder ps_arena_holder(thd);
      const auto last_insert_id_func = new Item_func_last_insert_id();
      if (last_insert_id_func == nullptr) return true;
      *ref = new Item_func_eq(args[0], last_insert_id_func);
      return *ref == nullptr || (*ref)->fix_fields(thd, ref);
    }
  }

  return false;
}

bool Item_func_isnull::resolve_type(THD *thd) {
  set_nullable(false);
  if (Item_bool_func::resolve_type(thd)) return true;

  cache_used = false;
  if (!args[0]->is_nullable()) {
    used_tables_cache = 0;
    cached_value = false;
    cache_used = true;
  } else {
    used_tables_cache = args[0]->used_tables();

    // If const, remember if value is always NULL or never NULL
    if (const_item() && !thd->lex->is_view_context_analysis()) {
      cached_value = args[0]->is_null();
      if (thd->is_error()) return true;
      cache_used = true;
    }
  }

  not_null_tables_cache = 0;
  if (null_on_null && !const_item())
    not_null_tables_cache |= args[0]->not_null_tables();

  return false;
}

longlong Item_func_isnull::val_int() {
  assert(fixed);
  if (cache_used) return cached_value;
  return args[0]->is_null() ? 1 : 0;
}

void Item_func_isnull::print(const THD *thd, String *str,
                             enum_query_type query_type) const {
  str->append('(');
  args[0]->print(thd, str, query_type);
  str->append(STRING_WITH_LEN(" is null)"));
}

longlong Item_is_not_null_test::val_int() {
  assert(fixed);
  assert(used_tables_cache != 0);
  DBUG_TRACE;
  if (args[0]->is_null()) {
    DBUG_PRINT("info", ("null"));
    owner->m_was_null |= 1;
    return 0;
  } else
    return 1;
}

bool Item_is_not_null_test::resolve_type(THD *thd) {
  set_nullable(false);
  if (Item_bool_func::resolve_type(thd)) return true;
  not_null_tables_cache = 0;
  if (null_on_null && !const_item())
    not_null_tables_cache |= args[0]->not_null_tables();
  return false;
}

void Item_is_not_null_test::update_used_tables() {
  const table_map initial_pseudo_tables = get_initial_pseudo_tables();
  used_tables_cache = initial_pseudo_tables;
  args[0]->update_used_tables();
  set_accum_properties(args[0]);
  used_tables_cache |= args[0]->used_tables();
  not_null_tables_cache = 0;
  if (null_on_null) not_null_tables_cache |= args[0]->not_null_tables();
}

float Item_func_isnotnull::get_filtering_effect(
    THD *thd, table_map filter_for_table, table_map read_tables,
    const MY_BITMAP *fields_to_ignore, double rows_in_table) {
  const Item_field *fld = contributes_to_filter(
      thd, read_tables, filter_for_table, fields_to_ignore);
  if (!fld) return COND_FILTER_ALLPASS;

  const double selectivity = get_histogram_selectivity(
      thd, *fld->field, histograms::enum_operator::IS_NOT_NULL, *this);

  return selectivity == kUndefinedSelectivity
             ? 1.0f - fld->get_cond_filter_default_probability(
                          rows_in_table, COND_FILTER_EQUALITY)
             : selectivity;
}

longlong Item_func_isnotnull::val_int() {
  assert(fixed);
  return args[0]->is_null() ? 0 : 1;
}

void Item_func_isnotnull::print(const THD *thd, String *str,
                                enum_query_type query_type) const {
  str->append('(');
  args[0]->print(thd, str, query_type);
  str->append(STRING_WITH_LEN(" is not null)"));
}

float Item_func_like::get_filtering_effect(THD *thd, table_map filter_for_table,
                                           table_map read_tables,
                                           const MY_BITMAP *fields_to_ignore,
                                           double rows_in_table) {
  const Item_field *fld = contributes_to_filter(
      thd, read_tables, filter_for_table, fields_to_ignore);
  if (!fld) return COND_FILTER_ALLPASS;

  /*
    Filtering effect is similar to that of BETWEEN because

    * "col like abc%" is similar to
      "col between abc and abd"
      The same applies for 'abc_'
    * "col like %abc" can be seen as
      "reverse(col) like cba%"" (see above)
    * "col like "abc%def%..." is also similar

    Now we're left with "col like <string_no_wildcards>" which should
    have filtering effect like equality, but it would be costly to
    look through the whole string searching for wildcards and since
    LIKE is mostly used for wildcards this isn't checked.
  */
  return fld->get_cond_filter_default_probability(rows_in_table,
                                                  COND_FILTER_BETWEEN);
}

longlong Item_func_like::val_int() {
  assert(fixed);

  if (!escape_evaluated && eval_escape_clause(current_thd)) return error_int();

  const CHARSET_INFO *cs = cmp.cmp_collation.collation;

  String *res = eval_string_arg(cs, args[0], &cmp.value1);
  if (args[0]->null_value) {
    null_value = true;
    return 0;
  }
  String *res2 = eval_string_arg(cs, args[1], &cmp.value2);
  if (args[1]->null_value) {
    null_value = true;
    return 0;
  }
  null_value = false;
  if (current_thd->is_error()) return 0;

  return my_wildcmp(cs, res->ptr(), res->ptr() + res->length(), res2->ptr(),
                    res2->ptr() + res2->length(), escape(),
                    (escape() == wild_one) ? -1 : wild_one,
                    (escape() == wild_many) ? -1 : wild_many)
             ? 0
             : 1;
}

/**
  We can optimize a where if first character isn't a wildcard
*/

Item_func::optimize_type Item_func_like::select_optimize(const THD *thd) {
  /*
    Can be called both during preparation (from prune_partitions()) and
    optimization. Check if the pattern can be evaluated in the current phase.
  */
  if (!args[1]->may_evaluate_const(thd)) return OPTIMIZE_NONE;

  // Don't evaluate the pattern if evaluation during optimization is disabled.
  if (!evaluate_during_optimization(args[1], thd->lex->current_query_block()))
    return OPTIMIZE_NONE;

  String *res2 = args[1]->val_str(&cmp.value2);
  if (!res2) return OPTIMIZE_NONE;

  if (!res2->length())  // Can optimize empty wildcard: column LIKE ''
    return OPTIMIZE_OP;

  assert(res2->ptr());
  const char first = res2->ptr()[0];
  return (first == wild_many || first == wild_one) ? OPTIMIZE_NONE
                                                   : OPTIMIZE_OP;
}

bool Item_func_like::check_covering_prefix_keys(THD *thd) {
  Item *first_arg = args[0]->real_item();
  Item *second_arg = args[1]->real_item();
  if (first_arg->type() == Item::FIELD_ITEM) {
    Field *field = down_cast<Item_field *>(first_arg)->field;
    Key_map covering_keys = field->get_covering_prefix_keys();
    if (covering_keys.is_clear_all()) return false;
    if (second_arg->const_item()) {
      size_t prefix_length = 0;
      String *wild_str = second_arg->val_str(&cmp.value2);
      if (thd->is_error()) return true;
      if (second_arg->null_value) return false;
      if (my_is_prefixidx_cand(wild_str->charset(), wild_str->ptr(),
                               wild_str->ptr() + wild_str->length(), escape(),
                               wild_many, &prefix_length))
        field->table->update_covering_prefix_keys(field, prefix_length,
                                                  &covering_keys);
      else
        // Not comparing to a prefix, remove all prefix indexes
        field->table->covering_keys.subtract(field->part_of_prefixkey);
    } else
      // Second argument is not a const
      field->table->covering_keys.subtract(field->part_of_prefixkey);
  }
  return false;
}

bool Item_func_like::fix_fields(THD *thd, Item **ref) {
  assert(!fixed);

  args[0]->real_item()->set_can_use_prefix_key();

  if (Item_bool_func::fix_fields(thd, ref)) {
    return true;
  }

  return false;
}

void Item_func_like::cleanup() {
  if (!escape_is_const) escape_evaluated = false;
  Item_bool_func2::cleanup();
}

/**
  Evaluate the expression in the escape clause.

  @param thd  thread handler
  @return false on success, true on failure
 */
bool Item_func_like::eval_escape_clause(THD *thd) {
  assert(!escape_evaluated);
  escape_evaluated = true;

  const bool no_backslash_escapes =
      thd->variables.sql_mode & MODE_NO_BACKSLASH_ESCAPES;

  // No ESCAPE clause is specified. The default escape character is backslash,
  // unless NO_BACKSLASH_ESCAPES mode is enabled.
  if (!escape_was_used_in_parsing()) {
    m_escape = no_backslash_escapes ? 0 : '\\';
    return false;
  }

  Item *escape_item = args[2];
  String buf;
  const String *escape_str = escape_item->val_str(&buf);
  if (thd->is_error()) return true;

  // Use backslash as escape character if the escape clause evaluates to NULL.
  // (For backward compatibility. The SQL standard says the LIKE expression
  // should evaluate to NULL in this case.)
  if (escape_item->null_value) {
    m_escape = '\\';
    return false;
  }

  // An empty escape sequence means there is no escape character. An empty
  // escape sequence is not accepted in NO_BACKSLASH_ESCAPES mode.
  if (escape_str->is_empty()) {
    if (no_backslash_escapes) {
      my_error(ER_WRONG_ARGUMENTS, MYF(0), "ESCAPE");
      return true;
    }
    m_escape = 0;
    return false;
  }

  // Accept at most one character.
  if (escape_str->numchars() > 1) {
    my_error(ER_WRONG_ARGUMENTS, MYF(0), "ESCAPE");
    return true;
  }

  const char *escape_str_ptr = escape_str->ptr();

  // For multi-byte character sets, we store the Unicode code point of the
  // escape character.
  if (use_mb(cmp.cmp_collation.collation)) {
    const CHARSET_INFO *cs = escape_str->charset();
    my_wc_t wc;
    int rc = cs->cset->mb_wc(
        cs, &wc, pointer_cast<const uchar *>(escape_str_ptr),
        pointer_cast<const uchar *>(escape_str_ptr) + escape_str->length());
    if (rc <= 0) {
      my_error(ER_WRONG_ARGUMENTS, MYF(0), "ESCAPE");
      return true;
    }
    m_escape = wc;
    return false;
  }

  // For single-byte character sets, we store the native code instead of the
  // Unicode code point. The escape character is converted to the character set
  // of the comparator if they differ.
  const CHARSET_INFO *cs = cmp.cmp_collation.collation;
  size_t unused;
  if (escape_str->needs_conversion(escape_str->length(), escape_str->charset(),
                                   cs, &unused)) {
    char ch;
    uint errors;
    const size_t cnvlen =
        copy_and_convert(&ch, 1, cs, escape_str_ptr, escape_str->length(),
                         escape_str->charset(), &errors);
    if (cnvlen == 0) {
      my_error(ER_WRONG_ARGUMENTS, MYF(0), "ESCAPE");
      return true;
    }
    m_escape = static_cast<uchar>(ch);
  } else {
    m_escape = static_cast<uchar>(escape_str_ptr[0]);
  }

  return false;
}

void Item_func_like::print(const THD *thd, String *str,
                           enum_query_type query_type) const {
  str->append('(');
  args[0]->print(thd, str, query_type);
  str->append(STRING_WITH_LEN(" like "));
  args[1]->print(thd, str, query_type);
  if (arg_count > 2) {
    str->append(STRING_WITH_LEN(" escape "));
    args[2]->print(thd, str, query_type);
  }
  str->append(')');
}

bool Item_func_xor::do_itemize(Parse_context *pc, Item **res) {
  if (skip_itemize(res)) return false;
  if (super::do_itemize(pc, res)) return true;

  if (!args[0]->is_bool_func()) {
    args[0] = make_condition(pc, args[0]);
    if (args[0] == nullptr) return true;
  }
  if (!args[1]->is_bool_func()) {
    args[1] = make_condition(pc, args[1]);
    if (args[1] == nullptr) return true;
  }

  return false;
}
float Item_func_xor::get_filtering_effect(THD *thd, table_map filter_for_table,
                                          table_map read_tables,
                                          const MY_BITMAP *fields_to_ignore,
                                          double rows_in_table) {
  assert(arg_count == 2);

  const float filter0 = args[0]->get_filtering_effect(
      thd, filter_for_table, read_tables, fields_to_ignore, rows_in_table);
  if (filter0 == COND_FILTER_ALLPASS) return COND_FILTER_ALLPASS;

  const float filter1 = args[1]->get_filtering_effect(
      thd, filter_for_table, read_tables, fields_to_ignore, rows_in_table);

  if (filter1 == COND_FILTER_ALLPASS) return COND_FILTER_ALLPASS;

  /*
    Calculated as "exactly one of independent events":
    P(A and not B) + P(B and not A) = P(A) + P(B) - 2 * P(A) * P(B)
  */
  return filter0 + filter1 - (2 * filter0 * filter1);
}

/**
  Make a logical XOR of the arguments.

  If either operator is NULL, return NULL.

  @todo
    (low priority) Change this to be optimized as: @n
    A XOR B   ->  (A) == 1 AND (B) <> 1) OR (A <> 1 AND (B) == 1) @n
    To be able to do this, we would however first have to extend the MySQL
    range optimizer to handle OR better.

  @note
    As we don't do any index optimization on XOR this is not going to be
    very fast to use.
*/

longlong Item_func_xor::val_int() {
  assert(fixed);
  int result = 0;
  null_value = false;
  for (uint i = 0; i < arg_count; i++) {
    result ^= (args[i]->val_int() != 0);
    if (args[i]->null_value) {
      null_value = true;
      return 0;
    }
    if (current_thd->is_error()) return error_int();
  }
  return result;
}

/**
  Apply NOT transformation to the item and return a new one.


    Transform the item using next rules:
    @verbatim
       a AND b AND ...    -> NOT(a) OR NOT(b) OR ...
       a OR b OR ...      -> NOT(a) AND NOT(b) AND ...
       NOT(a)             -> a
       a = b              -> a != b
       a != b             -> a = b
       a < b              -> a >= b
       a >= b             -> a < b
       a > b              -> a <= b
       a <= b             -> a > b
       IS NULL(a)         -> IS NOT NULL(a)
       IS NOT NULL(a)     -> IS NULL(a)
       EXISTS(subquery)   -> same EXISTS but with an internal mark of negation
       IN(subquery)       -> as above
    @endverbatim

  @return
    New item or
    NULL if we cannot apply NOT transformation (see Item::truth_transformer()).
*/

Item *Item_func_not::truth_transformer(THD *,
                                       Bool_test test)  // NOT(x)  ->  x
{
  return (test == BOOL_NEGATED) ? args[0] : nullptr;
}

Item *Item_func_comparison::truth_transformer(THD *, Bool_test test) {
  if (test != BOOL_NEGATED) return nullptr;
  Item *item = negated_item();
  return item;
}

/**
  XOR can be negated by negating one of the operands:

  NOT (a XOR b)  => (NOT a) XOR b
                 => a       XOR (NOT b)
*/
Item *Item_func_xor::truth_transformer(THD *thd, Bool_test test) {
  if (test != BOOL_NEGATED) return nullptr;
  Item *neg_operand;
  Item_func_xor *new_item;
  if ((neg_operand = args[0]->truth_transformer(thd, test)))
    // args[0] has truth_tranformer
    new_item = new (thd->mem_root) Item_func_xor(neg_operand, args[1]);
  else if ((neg_operand = args[1]->truth_transformer(thd, test)))
    // args[1] has truth_tranformer
    new_item = new (thd->mem_root) Item_func_xor(args[0], neg_operand);
  else {
    neg_operand = new (thd->mem_root) Item_func_not(args[0]);
    new_item = new (thd->mem_root) Item_func_xor(neg_operand, args[1]);
  }
  return new_item;
}

/**
  a IS NULL  ->  a IS NOT NULL.
*/
Item *Item_func_isnull::truth_transformer(THD *, Bool_test test) {
  return (test == BOOL_NEGATED) ? new Item_func_isnotnull(args[0]) : nullptr;
}

/**
  a IS NOT NULL  ->  a IS NULL.
*/
Item *Item_func_isnotnull::truth_transformer(THD *, Bool_test test) {
  return (test == BOOL_NEGATED) ? new Item_func_isnull(args[0]) : nullptr;
}

Item *Item_cond_and::truth_transformer(THD *thd, Bool_test test)
// NOT(a AND b AND ...)  ->  NOT a OR NOT b OR ...
{
  if (test != BOOL_NEGATED) return nullptr;
  if (truth_transform_arguments(thd, test)) return nullptr;
  Item *item = new Item_cond_or(list);
  return item;
}

Item *Item_cond_or::truth_transformer(THD *thd, Bool_test test)
// NOT(a OR b OR ...)  ->  NOT a AND NOT b AND ...
{
  if (test != BOOL_NEGATED) return nullptr;
  if (truth_transform_arguments(thd, test)) return nullptr;
  Item *item = new Item_cond_and(list);
  return item;
}

Item *Item_func_nop_all::truth_transformer(THD *, Bool_test test) {
  if (test != BOOL_NEGATED) return nullptr;
  // "NOT (e $cmp$ ANY (SELECT ...)) -> e $rev_cmp$" ALL (SELECT ...)
  Item_func_not_all *new_item = new Item_func_not_all(args[0]);
  Item_allany_subselect *allany = down_cast<Item_allany_subselect *>(args[0]);
  allany->m_all = !allany->m_all;
  allany->m_upper_item = new_item;
  return new_item;
}

Item *Item_func_not_all::truth_transformer(THD *, Bool_test test) {
  if (test != BOOL_NEGATED) return nullptr;
  // "NOT (e $cmp$ ALL (SELECT ...)) -> e $rev_cmp$" ANY (SELECT ...)
  Item_func_nop_all *new_item = new Item_func_nop_all(args[0]);
  Item_allany_subselect *allany = down_cast<Item_allany_subselect *>(args[0]);
  allany->m_all = !allany->m_all;
  allany->m_upper_item = new_item;
  return new_item;
}

Item *Item_func_eq::negated_item() /* a = b  ->  a != b */
{
  auto *i = new Item_func_ne(args[0], args[1]);
  if (i != nullptr) i->marker = marker;  // forward MARKER_IMPLICIT_NE_ZERO
  return i;
}

Item *Item_func_ne::negated_item() /* a != b  ->  a = b */
{
  auto *i = new Item_func_eq(args[0], args[1]);
  if (i != nullptr) i->marker = marker;  // forward MARKER_IMPLICIT_NE_ZERO
  return i;
}

Item *Item_func_lt::negated_item() /* a < b  ->  a >= b */
{
  return new Item_func_ge(args[0], args[1]);
}

Item *Item_func_ge::negated_item() /* a >= b  ->  a < b */
{
  return new Item_func_lt(args[0], args[1]);
}

Item *Item_func_gt::negated_item() /* a > b  ->  a <= b */
{
  return new Item_func_le(args[0], args[1]);
}

Item *Item_func_le::negated_item() /* a <= b  ->  a > b */
{
  return new Item_func_gt(args[0], args[1]);
}

/**
  just fake method, should never be called.
*/
Item *Item_func_comparison::negated_item() {
  assert(0);
  return nullptr;
}

bool Item_func_comparison::is_null() {
  assert(args[0]->cols() == args[1]->cols());

  // Fast path: If the operands are scalar, the result of the comparison is NULL
  // if and only if at least one of the operands is NULL.
  if (args[0]->cols() == 1) {
    return (null_value = args[0]->is_null() || args[1]->is_null());
  }

  // If the operands are rows, we need to evaluate the comparison operator to
  // find out if it is NULL. Fall back to the implementation in Item_func, which
  // calls update_null_value() to evaluate the operator.
  return Item_func::is_null();
}

bool Item_func_comparison::cast_incompatible_args(uchar *) {
  return cmp.inject_cast_nodes();
}

Item_multi_eq::Item_multi_eq(Item_field *lhs_field, Item_field *rhs_field)
    : Item_bool_func() {
  fields.push_back(lhs_field);
  fields.push_back(rhs_field);
}

Item_multi_eq::Item_multi_eq(Item *const_item, Item_field *field)
    : Item_bool_func(),
      m_const_arg(const_item),
      compare_as_dates(field->is_temporal_with_date()) {
  fields.push_back(field);
}

Item_multi_eq::Item_multi_eq(Item_multi_eq *item_multi_eq) : Item_bool_func() {
  List_iterator_fast<Item_field> li(item_multi_eq->fields);
  Item_field *item;
  while ((item = li++)) {
    fields.push_back(item);
  }
  m_const_arg = item_multi_eq->m_const_arg;
  compare_as_dates = item_multi_eq->compare_as_dates;
  m_always_false = item_multi_eq->m_always_false;
}

bool Item_multi_eq::compare_const(THD *thd, Item *const_item) {
  if (compare_as_dates) {
    cmp.set_datetime_cmp_func(this, &const_item, &m_const_arg);
    m_always_false = (cmp.compare() != 0);
  } else {
    Item_func_eq *eq_func = new Item_func_eq(const_item, m_const_arg);
    if (eq_func == nullptr) return true;
    if (eq_func->set_cmp_func()) return true;
    eq_func->quick_fix_field();
    m_always_false = (eq_func->val_int() == 0);
  }
  if (thd->is_error()) return true;
  if (m_always_false) used_tables_cache = 0;

  return false;
}

bool Item_multi_eq::add(THD *thd, Item *const_item, Item_field *field) {
  if (m_always_false) return false;
  if (m_const_arg == nullptr) {
    assert(field != nullptr);
    m_const_arg = const_item;
    compare_as_dates = field->is_temporal_with_date();
    return false;
  }
  return compare_const(thd, const_item);
}

bool Item_multi_eq::add(THD *thd, Item *const_item) {
  if (m_always_false) return false;
  if (m_const_arg == nullptr) {
    m_const_arg = const_item;
    return false;
  }
  return compare_const(thd, const_item);
}

void Item_multi_eq::add(Item_field *field) { fields.push_back(field); }

uint Item_multi_eq::members() { return fields.elements; }

/**
  Check whether a field is referred in the multiple equality.

  The function checks whether field is occurred in the Item_multi_eq object .

  @param field   Item field whose occurrence is to be checked

  @returns true if multiple equality contains a reference to field, false
  otherwise.
*/

bool Item_multi_eq::contains(const Item_field *field) const {
  for (const Item_field &item : fields) {
    if (field->eq(&item)) return true;
  }
  return false;
}

/**
  Add members of another Item_multi_eq object.

    The function merges two multiple equalities.
    After this operation the Item_multi_eq object additionally contains
    the field items of another item of the type Item_multi_eq.
    If the optional constant items are not equal the m_always_false flag is
    set to true.

  @param thd     thread handler
  @param item    multiple equality whose members are to be joined

  @returns false if success, true if error
*/

bool Item_multi_eq::merge(THD *thd, Item_multi_eq *item) {
  fields.concat(&item->fields);
  Item *c = item->m_const_arg;
  if (c) {
    /*
      The flag m_always_false will be set to true after this, if
      the multiple equality already contains a constant and its
      value is not equal to the value of c.
    */
    if (add(thd, c)) return true;
  }
  m_always_false |= item->m_always_false;
  if (m_always_false) used_tables_cache = 0;

  return false;
}

/**
  Check appearance of new constant items in the multiple equality object.

  The function checks appearance of new constant items among
  the members of multiple equalities. Each new constant item is
  compared with the designated constant item if there is any in the
  multiple equality. If there is none the first new constant item
  becomes designated.

  @param thd      thread handler

  @returns false if success, true if error
*/

bool Item_multi_eq::update_const(THD *thd) {
  List_iterator<Item_field> it(fields);
  Item *item;
  while ((item = it++)) {
    if (item->const_item() &&
        /*
          Don't propagate constant status of outer-joined column.
          Such a constant status here is a result of:
            a) empty outer-joined table: in this case such a column has a
               value of NULL; but at the same time other arguments of
               Item_multi_eq don't have to be NULLs and the value of the whole
               multiple equivalence expression doesn't have to be NULL or FALSE
               because of the outer join nature;
          or
            b) outer-joined table contains only 1 row: the result of
               this column is equal to a row field value *or* NULL.
          Both values are inacceptable as Item_multi_eq constants.
        */
        !item->is_outer_field()) {
      it.remove();
      if (add(thd, item)) return true;
    }
  }
  return false;
}

bool Item_multi_eq::fix_fields(THD *thd, Item **) {
  List_iterator_fast<Item_field> li(fields);
  Item *item;
  not_null_tables_cache = used_tables_cache = 0;
  bool nullable = false;
  while ((item = li++)) {
    used_tables_cache |= item->used_tables();
    not_null_tables_cache |= item->not_null_tables();
    nullable |= item->is_nullable();
  }
  set_nullable(nullable);
  if (resolve_type(thd)) return true;

  fixed = true;
  return false;
}

/**
  Get filtering effect for multiple equalities, i.e.
  "tx.col = value_1 = ... = value_n" where value_i may be a
  constant, a column etc.

  The multiple equality only contributes to the filtering effect for
  'filter_for_table' if
    a) A column in 'filter_for_table' is referred to
    b) at least one value_i is a constant or a column in a table
       already read

  If this multiple equality refers to more than one column in
  'filter_for_table', the predicates on all these fields will
  contribute to the filtering effect.
*/
float Item_multi_eq::get_filtering_effect(THD *thd, table_map filter_for_table,
                                          table_map read_tables,
                                          const MY_BITMAP *fields_to_ignore,
                                          double rows_in_table) {
  // This predicate does not refer to a column in 'filter_for_table'
  if (!(used_tables() & filter_for_table)) return COND_FILTER_ALLPASS;

  float filter = COND_FILTER_ALLPASS;
  /*
    Keep track of whether or not a usable value that is either a
    constant or a column in an already read table has been found.
  */
  bool found_comparable = false;

  // Is there a constant that this multiple equality is equal to?
  if (m_const_arg != nullptr) found_comparable = true;

  List_iterator<Item_field> it(fields);

  Item_field *cur_field;
  /*
    Calculate filtering effect for all applicable fields. If this
    item has multiple fields from 'filter_for_table', each of these
    fields will contribute to the filtering effect.
  */
  while ((cur_field = it++)) {
    if (cur_field->used_tables() & read_tables) {
      // cur_field is a field in a table earlier in the join sequence.
      found_comparable = true;
    } else if (cur_field->used_tables() == filter_for_table) {
      if (bitmap_is_set(fields_to_ignore, cur_field->field->field_index())) {
        /*
          cur_field is a field in 'filter_for_table', but it is a
          field which already contributes to the filtering effect.
          Its value can still be used as a constant if another column
          in the same table is referred to in this multiple equality.
        */
        found_comparable = true;
      } else {
        /*
          cur_field is a field in 'filter_for_table', and it's not one
          of the fields that must be ignored
        */
        float cur_filter = cur_field->get_cond_filter_default_probability(
            rows_in_table, COND_FILTER_EQUALITY);

        // Use index statistics if available for this field
        if (!cur_field->field->key_start.is_clear_all()) {
          // cur_field is indexed - there may be statistics for it.
          const TABLE *tab = cur_field->field->table;

          for (uint j = 0; j < tab->s->keys; j++) {
            if (cur_field->field->key_start.is_set(j) &&
                tab->key_info[j].has_records_per_key(0)) {
              cur_filter = static_cast<float>(
                  tab->key_info[j].records_per_key(0) / rows_in_table);
              break;
            }
          }
          /*
            Since rec_per_key and rows_per_table are calculated at
            different times, their values may not be in synch and thus
            it is possible that cur_filter is greater than 1.0 if
            rec_per_key is outdated. Force the filter to 1.0 in such
            cases.
          */
          if (cur_filter >= 1.0) cur_filter = 1.0f;
        } else if (m_const_arg != nullptr) {
          /*
            If index statistics is not available, see if we can use any
            available histogram statistics.
          */
          const histograms::Histogram *histogram =
              cur_field->field->table->find_histogram(
                  cur_field->field->field_index());
          if (histogram != nullptr) {
            std::array<Item *, 2> items{{cur_field, m_const_arg}};
            double selectivity;
            if (!histogram->get_selectivity(
                    items.data(), items.size(),
                    histograms::enum_operator::EQUALS_TO, &selectivity)) {
              if (unlikely(thd->opt_trace.is_started())) {
                Item_func_eq *eq_func =
                    new (thd->mem_root) Item_func_eq(cur_field, m_const_arg);
                write_histogram_to_trace(thd, eq_func, selectivity);
              }
              cur_filter = static_cast<float>(selectivity);
            }
          }
        }

        filter *= cur_filter;
      }
    }
  }
  return found_comparable ? filter : COND_FILTER_ALLPASS;
}

void Item_multi_eq::update_used_tables() {
  List_iterator_fast<Item_field> li(fields);
  Item *item;
  not_null_tables_cache = used_tables_cache = 0;
  if (m_always_false) return;
  m_accum_properties = 0;
  while ((item = li++)) {
    item->update_used_tables();
    used_tables_cache |= item->used_tables();
    not_null_tables_cache |= item->not_null_tables();
    add_accum_properties(item);
  }
  if (m_const_arg != nullptr) used_tables_cache |= m_const_arg->used_tables();
}

longlong Item_multi_eq::val_int() {
  Item_field *item_field;
  if (m_always_false) return 0;
  List_iterator_fast<Item_field> it(fields);
  Item *item = m_const_arg != nullptr ? m_const_arg : it++;
  eval_item->store_value(item);
  if ((null_value = item->null_value)) return 0;
  while ((item_field = it++)) {
    /* Skip fields of non-const tables. They haven't been read yet */
    if (item_field->field->table->const_table) {
      const int rc = eval_item->cmp(item_field);
      if ((rc == static_cast<int>(true)) || (null_value = (rc == UNKNOWN)))
        return 0;
    }
  }
  return 1;
}

Item_multi_eq::~Item_multi_eq() {
  if (eval_item != nullptr) {
    ::destroy_at(eval_item);
    eval_item = nullptr;
  }
}

bool Item_multi_eq::resolve_type(THD *thd) {
  Item *item;
  // As such item is created during optimization, types of members are known:
#ifndef NDEBUG
  List_iterator_fast<Item_field> it(fields);
  while ((item = it++)) {
    assert(item->data_type() != MYSQL_TYPE_INVALID);
  }
#endif

  item = get_first();
  eval_item = cmp_item::new_comparator(thd, item->result_type(), item,
                                       item->collation.collation);
  return eval_item == nullptr;
}

bool Item_multi_eq::walk(Item_processor processor, enum_walk walk, uchar *arg) {
  if ((walk & enum_walk::PREFIX) && (this->*processor)(arg)) return true;

  List_iterator_fast<Item_field> it(fields);
  Item *item;
  while ((item = it++)) {
    if (item->walk(processor, walk, arg)) return true;
  }

  return (walk & enum_walk::POSTFIX) && (this->*processor)(arg);
}

void Item_multi_eq::print(const THD *thd, String *str,
                          enum_query_type query_type) const {
  str->append(func_name());
  str->append('(');

  if (m_const_arg != nullptr) m_const_arg->print(thd, str, query_type);

  bool first = (m_const_arg == nullptr);
  for (auto &item_field : fields) {
    if (!first) str->append(STRING_WITH_LEN(", "));
    item_field.print(thd, str, query_type);
    first = false;
  }
  str->append(')');
}

bool Item_multi_eq::eq_specific(const Item *item) const {
  const Item_multi_eq *item_eq = down_cast<const Item_multi_eq *>(item);
  if ((m_const_arg != nullptr) != (item_eq->m_const_arg != nullptr)) {
    return false;
  }
  if (m_const_arg != nullptr && !m_const_arg->eq(item_eq->m_const_arg)) {
    return false;
  }

  // NOTE: We assume there are no duplicates in either list.
  if (fields.size() != item_eq->fields.size()) {
    return false;
  }
  for (const Item_field &field : get_fields()) {
    if (!item_eq->contains(&field)) {
      return false;
    }
  }

  return true;
}

longlong Item_func_match_predicate::val_int() {
  // Reimplement Item_func_match::val_int() instead of forwarding to it. Even
  // though args[0] is usually an Item_func_match, it could in some situations
  // be replaced with a reference to a field in a temporary table holding the
  // result of the MATCH function. And since the conversion from double to
  // integer in Field_double::val_int() is different from the conversion in
  // Item_func_match::val_int(), just returning args[0]->val_int() would give
  // wrong results when the argument has been materialized.
  return args[0]->val_real() != 0;
}

longlong Item_func_trig_cond::val_int() {
  if (trig_var == nullptr) {
    // We don't use trigger conditions for IS_NOT_NULL_COMPL / FOUND_MATCH in
    // the iterator executor (except for figuring out which conditions are join
    // conditions and which are from WHERE), so we remove them whenever we can.
    // However, we don't prune them entirely from the query tree, so they may be
    // left within e.g. sub-conditions of ORs. Open up the conditions so
    // that we don't have conditions that are disabled during execution.
    assert(trig_type == IS_NOT_NULL_COMPL || trig_type == FOUND_MATCH);
    return args[0]->val_int();
  }
  return *trig_var ? args[0]->val_int() : 1;
}

void Item_func_trig_cond::get_table_range(Table_ref **first_table,
                                          Table_ref **last_table) const {
  *first_table = nullptr;
  *last_table = nullptr;
  if (m_join == nullptr) return;

  // There may be a JOIN_TAB or a QEP_TAB.
  plan_idx last_inner;
  if (m_join->qep_tab) {
    QEP_TAB *qep_tab = &m_join->qep_tab[m_idx];
    *first_table = qep_tab->table_ref;
    last_inner = qep_tab->last_inner();
    *last_table = m_join->qep_tab[last_inner].table_ref;
  } else {
    JOIN_TAB *join_tab = m_join->best_ref[m_idx];
    *first_table = join_tab->table_ref;
    last_inner = join_tab->last_inner();
    *last_table = m_join->best_ref[last_inner]->table_ref;
  }
}

table_map Item_func_trig_cond::get_inner_tables() const {
  table_map inner_tables(0);
  if (m_join != nullptr) {
    if (m_join->qep_tab) {
      const plan_idx last_idx = m_join->qep_tab[m_idx].last_inner();
      plan_idx ix = m_idx;
      do {
        inner_tables |= m_join->qep_tab[ix++].table_ref->map();
      } while (ix <= last_idx);
    } else {
      const plan_idx last_idx = m_join->best_ref[m_idx]->last_inner();
      plan_idx ix = m_idx;
      do {
        inner_tables |= m_join->best_ref[ix++]->table_ref->map();
      } while (ix <= last_idx);
    }
  }
  return inner_tables;
}

void Item_func_trig_cond::print(const THD *thd, String *str,
                                enum_query_type query_type) const {
  /*
    Print:
    <if>(<property><(optional list of source tables)>, condition, TRUE)
    which means: if a certain property (<property>) is true, then return
    the value of <condition>, else return TRUE. If source tables are
    present, they are the owner of the property.
  */
  str->append(func_name());
  str->append("(");
  switch (trig_type) {
    case IS_NOT_NULL_COMPL:
      str->append("is_not_null_compl");
      break;
    case FOUND_MATCH:
      str->append("found_match");
      break;
    case OUTER_FIELD_IS_NOT_NULL:
      str->append("outer_field_is_not_null");
      break;
    default:
      assert(0);
  }
  if (m_join != nullptr) {
    Table_ref *first_table, *last_table;
    get_table_range(&first_table, &last_table);
    str->append("(");
    str->append(first_table->table->alias);
    if (first_table != last_table) {
      /* case of t1 LEFT JOIN (t2,t3,...): print range of inner tables */
      str->append("..");
      str->append(last_table->table->alias);
    }
    str->append(")");
  }
  str->append(", ");
  args[0]->print(thd, str, query_type);
  str->append(", true)");
}

/**
  Get item that can be substituted for the supplied item.

  @param field  field item to get substitution field for, which must be
                present within the multiple equality itself.

  @retval Found substitution item in the multiple equality.

  @details Get the first item of multiple equality that can be substituted
  for the given field item. In order to make semijoin materialization strategy
  work correctly we can't propagate equal fields between a materialized
  semijoin and the outer query (or any other semijoin) unconditionally.
  Thus the field is returned according to the following rules:

  1) If the given field belongs to a materialized semijoin then the
     first field in the multiple equality which belongs to the same semijoin
     is returned.
  2) If the given field doesn't belong to a materialized semijoin then
     the first field in the multiple equality is returned.
*/

Item_field *Item_multi_eq::get_subst_item(const Item_field *field) {
  assert(field != nullptr);

  const JOIN_TAB *field_tab = field->field->table->reginfo.join_tab;

  /*
    field_tab is NULL if this function was not called from
    JOIN::optimize() but from e.g. mysql_delete() or mysql_update().
    In these cases there is only one table and no semijoin
  */
  if (field_tab && sj_is_materialize_strategy(field_tab->get_sj_strategy())) {
    /*
      It's a field from a materialized semijoin. We can substitute it only
      with a field from the same semijoin.

      Example: suppose we have a join_tab order:

       ot1 ot2 <subquery> ot3 SJM(it1  it2  it3)

      <subquery> is the temporary table that is materialized from the join
      of it1, it2 and it3.

      and equality ot2.col = <subquery>.col = it1.col = it2.col

      If we're looking for best substitute for 'it2.col', we must pick it1.col
      and not ot2.col. it2.col is evaluated while performing materialization,
      when the outer tables are not available in the execution.

      Note that subquery materialization does not have the same problem:
      even though IN->EXISTS has injected equalities involving outer query's
      expressions, it has wrapped those expressions in variants of Item_ref,
      never Item_field, so they can be part of an Item_multi_eq only if they are
      constant (in which case there is no problem with choosing them below);
      @see check_simple_equality().
    */
    List_iterator<Item_field> it(fields);
    Item_field *item;
    const plan_idx first = field_tab->first_sj_inner(),
                   last = field_tab->last_sj_inner();

    while ((item = it++)) {
      const plan_idx idx = item->field->table->reginfo.join_tab->idx();
      if (idx >= first && idx <= last) return item;
    }
  } else {
    /*
      The field is not in a materialized semijoin nest. We can return
      the first field in the multiple equality.

      Example: suppose we have a join_tab order with MaterializeLookup:

        ot1 ot2 <subquery> SJM(it1 it2)

      Here we should always pick the first field in the multiple equality,
      as this will be present before all other dependent fields.

      Example: suppose we have a join_tab order with MaterializeScan:

        <subquery> ot1 ot2 SJM(it1 it2)

      and equality <subquery>.col = ot2.col = ot1.col = it2.col.

      When looking for best substitute for ot2.col, we should pick
      <subquery>.col, because column values from the inner materialized tables
      are copied to the temporary table <subquery>, and when we run the scan,
      field values are read into this table's field buffers.
    */
    return fields.head();
  }
  assert(false);  // Should never get here.
  return nullptr;
}

/**
  Transform an Item_multi_eq object after having added a table that
  represents a materialized semi-join.

  @details
    If the multiple equality represented by the Item_multi_eq object contains
    a field from the subquery that was used to create the materialized table,
    add the corresponding key field from the materialized table to the
    multiple equality.
    @see JOIN::update_equalities_for_sjm() for the reason.
*/

Item *Item_multi_eq::equality_substitution_transformer(uchar *arg) {
  Table_ref *sj_nest = reinterpret_cast<Table_ref *>(arg);
  List_iterator<Item_field> it(fields);
  List<Item_field> added_fields;
  Item_field *item;
  // Iterate over the fields in the multiple equality
  while ((item = it++)) {
    // Skip fields that do not come from materialized subqueries
    const JOIN_TAB *tab = item->field->table->reginfo.join_tab;
    if (!tab || !sj_is_materialize_strategy(tab->get_sj_strategy())) continue;

    // Iterate over the fields selected from the subquery
    uint fieldno = 0;
    for (Item *existing : sj_nest->nested_join->sj_inner_exprs) {
      if (existing->real_item()->eq(item))
        added_fields.push_back(sj_nest->nested_join->sjm.mat_fields[fieldno]);
      fieldno++;
    }
  }
  fields.concat(&added_fields);

  return this;
}

/**
  Replace arg of Item_func_eq object after having added a table that
  represents a materialized semi-join.

  @details
    The right argument of an injected semi-join equality (which comes from
    the select list of the subquery) is replaced with the corresponding
    column from the materialized temporary table, if the left and right
    arguments are not from the same semi-join nest.
    @see JOIN::update_equalities_for_sjm() for why this is needed.
*/
Item *Item_func_eq::equality_substitution_transformer(uchar *arg) {
  Table_ref *sj_nest = reinterpret_cast<Table_ref *>(arg);

  // Skip if equality can be processed during materialization
  if (((used_tables() & ~INNER_TABLE_BIT) & ~sj_nest->sj_inner_tables) == 0) {
    return this;
  }
  // Iterate over the fields selected from the subquery
  uint fieldno = 0;
  for (Item *existing : sj_nest->nested_join->sj_inner_exprs) {
    if (existing->real_item()->eq(args[1]) &&
        (args[0]->used_tables() & ~sj_nest->sj_inner_tables))
      current_thd->change_item_tree(
          args + 1, sj_nest->nested_join->sjm.mat_fields[fieldno]);
    fieldno++;
  }
  return this;
}

float Item_func_eq::get_filtering_effect(THD *thd, table_map filter_for_table,
                                         table_map read_tables,
                                         const MY_BITMAP *fields_to_ignore,
                                         double rows_in_table) {
  if (arguments()[0]->type() == NULL_ITEM ||
      arguments()[1]->type() == NULL_ITEM) {
    return 0.0;
  }

  const Item_field *fld = contributes_to_filter(
      thd, read_tables, filter_for_table, fields_to_ignore);

  if (!fld) return COND_FILTER_ALLPASS;

  return GetEqualSelectivity(thd, this, *fld, rows_in_table);
}

bool Item_func_any_value::aggregate_check_group(uchar *arg) {
  Group_check *gc = reinterpret_cast<Group_check *>(arg);
  if (gc->is_stopped(this)) return false;
  gc->stop_at(this);
  return false;
}

bool Item_func_any_value::aggregate_check_distinct(uchar *arg) {
  Distinct_check *dc = reinterpret_cast<Distinct_check *>(arg);
  if (dc->is_stopped(this)) return false;
  dc->stop_at(this);
  return false;
}

bool Item_func_any_value::collect_item_field_or_view_ref_processor(uchar *arg) {
  Collect_item_fields_or_view_refs *info =
      pointer_cast<Collect_item_fields_or_view_refs *>(arg);
  if (m_phase_post) {
    m_phase_post = false;
    info->m_any_value_level--;
  } else {
    m_phase_post = true;
    info->m_any_value_level++;
  }
  return false;
}

bool Item_cond_and::contains_only_equi_join_condition() const {
  for (const Item &item : list) {
    if (item.type() != Item::FUNC_ITEM) {
      return false;
    }

    const Item_func *item_func = down_cast<const Item_func *>(&item);
    if (!item_func->contains_only_equi_join_condition()) {
      return false;
    }
  }

  return true;
}

bool Item_eq_base::contains_only_equi_join_condition() const {
  assert(arg_count == 2);
  Item *left_arg = args[0];
  Item *right_arg = args[1];

  const table_map left_arg_used_tables =
      left_arg->used_tables() & ~PSEUDO_TABLE_BITS;
  const table_map right_arg_used_tables =
      right_arg->used_tables() & ~PSEUDO_TABLE_BITS;

  if (left_arg_used_tables == 0 || right_arg_used_tables == 0) {
    // This is a filter, and not a join condition.
    return false;
  }

  // We may have conditions like (t1.x = t1.y + t2.x) which cannot be used as an
  // equijoin condition because t1 is referenced on both sides of the equality.
  if (Overlaps(left_arg_used_tables, right_arg_used_tables)) {
    return false;
  }

  // We may have view references which are constants in the underlying
  // derived tables but used_tables() might not reflect it because the
  // merged derived table is an inner table of an outer join
  // (Item_view_ref::used_tables()). Considering conditions having these
  // constants as equi-join conditions is causing problems for secondary
  // engine. So for now, we reject these.
  if (left_arg->type() == Item::REF_ITEM &&
      down_cast<Item_ref *>(left_arg)->ref_type() == Item_ref::VIEW_REF &&
      down_cast<Item_ref *>(left_arg)->ref_item()->used_tables() == 0)
    return false;

  if (right_arg->type() == Item::REF_ITEM &&
      down_cast<Item_ref *>(right_arg)->ref_type() == Item_ref::VIEW_REF &&
      down_cast<Item_ref *>(right_arg)->ref_item()->used_tables() == 0)
    return false;

  return true;
}

bool Item_func_trig_cond::contains_only_equi_join_condition() const {
  if (args[0]->item_name.ptr() == antijoin_null_cond) {
    return true;
  }

  if (args[0]->type() != Item::FUNC_ITEM &&
      args[0]->type() != Item::COND_ITEM) {
    return false;
  }

  return down_cast<const Item_func *>(args[0])
      ->contains_only_equi_join_condition();
}

// Append a string value to join_key_buffer, extracted from "comparand".
// In general, we append the sort key from the Item, which makes it memcmp-able.
//
// For strings with NO_PAD collations, we also prepend the string value with the
// number of bytes written to the buffer if "is_multi_column_key" is true. This
// is needed when the join key consists of multiple columns. Otherwise, we would
// get the same join key for ('abc', 'def') and ('ab', 'cdef'), so that a join
// condition such as
//
//     t1.a = t2.a AND t1.b = t2.b
//
// would degenerate to
//
//     CONCAT(t1.a, t2.a) = CONCAT(t1.b, t2.b)
//
static bool append_string_value(Item *comparand,
                                const CHARSET_INFO *character_set,
                                size_t max_char_length,
                                bool pad_char_to_full_length,
                                bool is_multi_column_key,
                                String *join_key_buffer) {
  // String results must be extracted using the correct character set and
  // collation. This is given by the Arg_comparator, so we call strnxfrm
  // to make the string values memcmp-able.
  StringBuffer<STRING_BUFFER_USUAL_SIZE> str_buffer;

  String *str = eval_string_arg(character_set, comparand, &str_buffer);
  if (comparand->null_value || str == nullptr) {
    return true;
  }

  // If the collation is a PAD SPACE collation, use the pre-calculated max
  // length so that the shortest string is padded to the same length as the
  // longest string. We also do the same for the special case where the
  // (deprecated) SQL mode PAD_CHAR_TO_FULL_LENGTH is enabled, where CHAR
  // columns are padded to full length regardless of the collation used.
  // The longest possible string is given by the data type length specification
  // (CHAR(N), VARCHAR(N)).
  const bool use_padding =
      character_set->pad_attribute == PAD_SPACE ||
      (comparand->data_type() == MYSQL_TYPE_STRING && pad_char_to_full_length);
  const size_t char_length = use_padding ? max_char_length : str->numchars();
  const size_t buffer_size = character_set->coll->strnxfrmlen(
      character_set, char_length * character_set->mbmaxlen);

  // If we don't pad strings, we need to include the length of the string when
  // we have multi-column keys, so that it's unambiguous where the string ends
  // and where the next part of the key begins in case of multi-column join
  // keys. Reserve space for it here.
  const bool prepend_length = !use_padding && is_multi_column_key;
  using KeyLength = std::uint32_t;
  const size_t orig_buffer_size = join_key_buffer->length();
  if (prepend_length) {
    if (join_key_buffer->reserve(sizeof(KeyLength))) {
      return true;
    }
    join_key_buffer->length(orig_buffer_size + sizeof(KeyLength));
  }

  if (buffer_size > 0) {
    // Reserve space in the buffer so we can insert the transformed string
    // directly into the buffer.
    if (join_key_buffer->reserve(buffer_size)) {
      return true;
    }

    uchar *dptr = pointer_cast<uchar *>(join_key_buffer->ptr()) +
                  join_key_buffer->length();
    const size_t actual_length =
        my_strnxfrm(character_set, dptr, buffer_size,
                    pointer_cast<const uchar *>(str->ptr()), str->length());
    assert(actual_length <= buffer_size);

    // Increase the length of the buffer by the actual length of the
    // string transformation.
    join_key_buffer->length(join_key_buffer->length() + actual_length);
  }

  if (prepend_length) {
    const KeyLength key_length =
        join_key_buffer->length() - (orig_buffer_size + sizeof(KeyLength));
    memcpy(join_key_buffer->ptr() + orig_buffer_size, &key_length,
           sizeof(key_length));
  }

  return false;
}

// Append a double value to join_key_buffer.
static bool append_double_value(double value, bool is_null,
                                String *join_key_buffer) {
  if (is_null) return true;
  join_key_buffer->append(pointer_cast<const char *>(&value), sizeof(value),
                          static_cast<size_t>(0));
  return false;
}

// Append an integer value to join_key_buffer.
// Storing an extra byte for unsigned_flag ensures that negative values do not
// match large unsigned values.
static bool append_int_value(longlong value, bool is_null, bool unsigned_flag,
                             String *join_key_buffer) {
  if (is_null) return true;
  join_key_buffer->append(pointer_cast<const char *>(&value), sizeof(value),
                          static_cast<size_t>(0));
  // We do not need the extra byte for (0 <= value <= LLONG_MAX).
  if (value < 0) join_key_buffer->append(static_cast<char>(unsigned_flag));
  return false;
}

static bool append_hash_for_string_value(Item *comparand,
                                         const CHARSET_INFO *character_set,
                                         String *join_key_buffer) {
  StringBuffer<STRING_BUFFER_USUAL_SIZE> str_buffer;

  String *str = eval_string_arg(character_set, comparand, &str_buffer);
  if (str == nullptr) {
    return true;
  }

  // nr2 isn't used; we only need one, and some collations don't even
  // update it. The seeds are 1 and 4 by convention.
  uint64 nr1 = 1, nr2 = 4;
  character_set->coll->hash_sort(character_set,
                                 pointer_cast<const uchar *>(str->ptr()),
                                 str->length(), &nr1, &nr2);

  join_key_buffer->reserve(sizeof(nr1));
  uchar *dptr =
      pointer_cast<uchar *>(join_key_buffer->ptr()) + join_key_buffer->length();
  memcpy(dptr, &nr1, sizeof(nr1));
  join_key_buffer->length(join_key_buffer->length() + sizeof(nr1));
  return false;
}

static bool append_hash_for_json_value(Item *comparand,
                                       String *join_key_buffer) {
  Json_wrapper value;
  StringBuffer<STRING_BUFFER_USUAL_SIZE> buffer1, buffer2;
  if (get_json_atom_wrapper(
          &comparand, /*arg_idx=*/0, /*calling_function=*/"hash", &buffer1,
          &buffer2, &value, /*scalar=*/nullptr, /*accept_string=*/true)) {
    return true;
  }

  if (comparand->null_value) return true;

  const uint64_t hash = value.make_hash_key(/*hash_val=*/0);
  return join_key_buffer->append(pointer_cast<const char *>(&hash),
                                 sizeof(hash));
}

// Append a decimal value to join_key_buffer, extracted from "comparand".
//
// The number of bytes written depends on the actual value. (Leading zero digits
// are stripped off, and for +/- 0 even trailing zeros are stripped off.) In
// order to prevent ambiguity in case of multi-column join keys, the length in
// bytes is prepended to the value if "is_multi_column_key" is true.
static bool append_decimal_value(Item *comparand, bool is_multi_column_key,
                                 String *join_key_buffer) {
  my_decimal decimal_buffer;
  const my_decimal *decimal = comparand->val_decimal(&decimal_buffer);
  if (comparand->null_value) {
    return true;
  }

  if (decimal_is_zero(decimal)) {
    // Encode zero as an empty string. Write length = 0 to indicate that.
    if (is_multi_column_key) {
      if (join_key_buffer->append(char{0})) {
        return true;
      }
    }
    return false;
  }

  // Normalize the precision to get same hash length for equal numbers.
  const int scale = decimal->frac;
  const int precision = my_decimal_intg(decimal) + scale;

  const int buffer_size = my_decimal_get_binary_size(precision, scale);
  if (join_key_buffer->reserve(buffer_size + 1)) {
    return true;
  }
  if (is_multi_column_key) {
    join_key_buffer->append(static_cast<char>(buffer_size));
  }

  uchar *write_position =
      pointer_cast<uchar *>(join_key_buffer->ptr()) + join_key_buffer->length();
  my_decimal2binary(E_DEC_FATAL_ERROR, decimal, write_position, precision,
                    scale);
  join_key_buffer->length(join_key_buffer->length() + buffer_size);
  return false;
}

/// Extract the value from the item and append it to the output buffer
/// "join_key_buffer" in a memcmp-able format.
///
/// The value extracted here will be used as the key in the hash table
/// structure, where comparisons between keys are based on memcmp. Thus, it is
/// important that the values extracted are memcmp-able, so for string values,
/// we are basically creating a sort key. Other types (DECIMAL and FLOAT(M,N)
/// and DOUBLE(M, N)) may be wrapped in a typecast in order to get a memcmp-able
/// format from both sides of the condition.
/// See Item_eq_base::create_cast_if_needed for more details.
///
/// @param thd the thread handler
/// @param join_condition The hash join condition from which to get the value
///   to write into the buffer.
/// @param comparator the comparator set up by Item_cmpfunc. This gives us the
///   context in which the comparison is done. It is also needed for extracting
///   the value in case of DATE/TIME/DATETIME/YEAR values in some cases
/// @param is_left_argument whether or not the provided item is the left
///   argument of the condition. This is needed in case the comparator has set
///   up a custom function for extracting the value from the item, as there are
///   two separate functions for each side of the condition
/// @param is_multi_column_key true if the hash join key has multiple columns
///   (that is, the hash join condition is a conjunction)
/// @param[out] join_key_buffer the output buffer where the extracted value
///   is appended
///
/// @returns true if a SQL NULL value was found
static bool extract_value_for_hash_join(THD *thd,
                                        const HashJoinCondition &join_condition,
                                        const Arg_comparator *comparator,
                                        bool is_left_argument,
                                        bool is_multi_column_key,
                                        String *join_key_buffer) {
  if (comparator->get_compare_type() == ROW_RESULT) {
    // If the comparand returns a row via a subquery or a row value expression,
    // the comparator will be set up with child comparators (one for each column
    // in the row value). For hash join, we currently allow row values with only
    // one column.
    assert(comparator->get_child_comparator_count() == 1);
    comparator = comparator->get_child_comparators();
  }

  Item *comparand = is_left_argument ? join_condition.left_extractor()
                                     : join_condition.right_extractor();
  if (comparand->type() == Item::ROW_ITEM) {
    // In case of row value, get hold of the first column in the row. Note that
    // this is not needed for subqueries; val_* will execute and return the
    // value for scalar subqueries.
    comparand = comparand->element_index(0);
  }

  if (comparator->use_custom_value_extractors()) {
    // The Arg_comparator has decided that the values should be extracted using
    // the function pointer given by "get_value_[a|b]_func", so let us do the
    // same. This can happen for DATE, DATETIME and YEAR, and there are separate
    // function pointers for each side of the argument.
    bool is_null;
    longlong value = comparator->extract_value_from_argument(
        thd, comparand, is_left_argument, &is_null);
    if (is_null) {
      return true;
    }

    join_key_buffer->append(pointer_cast<const char *>(&value), sizeof(value),
                            static_cast<size_t>(0));
    return false;
  }

  switch (comparator->get_compare_type()) {
    case STRING_RESULT: {
      if (comparator->compare_as_json()) {
        // JSON values can be large, so we don't store the full sort key.
        assert(!join_condition.store_full_sort_key());
        return append_hash_for_json_value(comparand, join_key_buffer);
      }
      if (join_condition.store_full_sort_key()) {
        return append_string_value(
            comparand, comparator->cmp_collation.collation,
            join_condition.max_character_length(),
            (thd->variables.sql_mode & MODE_PAD_CHAR_TO_FULL_LENGTH) > 0,
            is_multi_column_key, join_key_buffer);
      } else {
        return append_hash_for_string_value(
            comparand, comparator->cmp_collation.collation, join_key_buffer);
      }
    }
    case REAL_RESULT: {
      double value = comparand->val_real();
      if (value == 0.0) value = 0.0;  // Ensure that -0.0 hashes as +0.0.
      return append_double_value(value, comparand->null_value, join_key_buffer);
    }
    case INT_RESULT: {
      const longlong value = comparand->val_int();
      return append_int_value(value, comparand->null_value,
                              comparand->unsigned_flag, join_key_buffer);
    }
    case DECIMAL_RESULT: {
      return append_decimal_value(comparand, is_multi_column_key,
                                  join_key_buffer);
    }
    default: {
      // This should not happen.
      assert(false);
      return true;
    }
  }

  return false;
}

bool Item_eq_base::append_join_key_for_hash_join(
    THD *thd, const table_map tables, const HashJoinCondition &join_condition,
    bool is_multi_column_key, String *join_key_buffer) const {
  const bool is_left_argument = join_condition.left_uses_any_table(tables);
  assert(is_left_argument != join_condition.right_uses_any_table(tables));

  // If this is a NULL-safe equal (<=>), we need to store NULL values in the
  // hash key. Set it to zero initially to indicate not NULL. Gets updated later
  // if it turns out the value is NULL.
  const size_t null_pos = join_key_buffer->length();
  if (join_condition.null_equals_null()) {
    join_key_buffer->append(char{0});
  }

  const bool is_null =
      extract_value_for_hash_join(thd, join_condition, &cmp, is_left_argument,
                                  is_multi_column_key, join_key_buffer);

  if (is_null && join_condition.null_equals_null()) {
    (*join_key_buffer)[null_pos] = 1;
    return false;
  }

  return is_null;
}

Item *Item_eq_base::create_cast_if_needed(MEM_ROOT *mem_root,
                                          Item *argument) const {
  // We wrap the argument in a typecast node in two cases:
  // a) If the comparison is done in a DECIMAL context.
  // b) If the comparison is done in a floating point context, AND both sides
  //    have a data type where the number of decimals is specified. Note that
  //    specifying the numbers of decimals for floating point types is
  //    deprecated, so this should be a really rare case.
  //
  // In both cases, we cast the argument to a DECIMAL, where the precision and
  // scale is the highest among the condition arguments.
  const bool cast_to_decimal = cmp.get_compare_type() == DECIMAL_RESULT ||
                               (cmp.get_compare_type() == REAL_RESULT &&
                                args[0]->decimals < DECIMAL_NOT_SPECIFIED &&
                                args[1]->decimals < DECIMAL_NOT_SPECIFIED);

  if (cast_to_decimal) {
    const int precision =
        max(args[0]->decimal_precision(), args[1]->decimal_precision());
    const int scale = max(args[0]->decimals, args[1]->decimals);

    return new (mem_root)
        Item_typecast_decimal(POS(), argument, precision, scale);
  }

  return argument;
}

HashJoinCondition::HashJoinCondition(Item_eq_base *join_condition,
                                     MEM_ROOT *mem_root)
    : m_join_condition(join_condition),
      m_left_extractor(join_condition->create_cast_if_needed(
          mem_root, join_condition->arguments()[0])),
      m_right_extractor(join_condition->create_cast_if_needed(
          mem_root, join_condition->arguments()[1])),
      m_left_used_tables(join_condition->arguments()[0]->used_tables()),
      m_right_used_tables(join_condition->arguments()[1]->used_tables()),
      m_max_character_length(max(m_left_extractor->max_char_length(),
                                 m_right_extractor->max_char_length())),
      m_null_equals_null(join_condition->functype() == Item_func::EQUAL_FUNC &&
                         (join_condition->get_arg(0)->is_nullable() ||
                          join_condition->get_arg(1)->is_nullable())) {
  m_store_full_sort_key = true;

  const bool using_secondary_storage_engine =
      (current_thd->lex->m_sql_cmd != nullptr &&
       current_thd->lex->m_sql_cmd->using_secondary_storage_engine());
  if ((join_condition->compare_type() == STRING_RESULT ||
       join_condition->compare_type() == ROW_RESULT) &&
      !using_secondary_storage_engine) {
    const CHARSET_INFO *cs = join_condition->compare_collation();
    if (cs->coll->strnxfrmlen(cs, cs->mbmaxlen * m_max_character_length) >
        1024) {
      // This field can potentially get very long keys; it is better to
      // just store the hash, and then re-check the condition afterwards.
      // The value of 1024 is fairly arbitrary, and may be changed in the
      // future. We don't do this for secondary engines; how they wish
      // to do their hash joins will be an internal implementation detail.
      m_store_full_sort_key = false;
    }
  }
}

longlong Arg_comparator::extract_value_from_argument(THD *thd, Item *item,
                                                     bool left_argument,
                                                     bool *is_null) const {
  assert(use_custom_value_extractors());
  assert(get_value_a_func != nullptr && get_value_b_func != nullptr);

  // The Arg_comparator has decided that the values should be extracted using
  // the function pointer given by "get_value_[a|b]_func", so let us do the
  // same. This can happen for DATE, DATETIME and YEAR, and there are separate
  // function pointers for each side of the argument.
  Item **item_arg = &item;
  if (left_argument) {
    return get_value_a_func(thd, &item_arg, nullptr, item, is_null);
  } else {
    return get_value_b_func(thd, &item_arg, nullptr, item, is_null);
  }
}

void find_and_adjust_equal_fields(Item *item, table_map available_tables,
                                  bool replace, bool *found) {
  WalkItem(item, enum_walk::PREFIX,
           [available_tables, replace, found](Item *inner_item) {
             if (inner_item->type() == Item::FUNC_ITEM) {
               Item_func *func_item = down_cast<Item_func *>(inner_item);
               for (uint i = 0; i < func_item->arg_count; ++i) {
                 if (func_item->arguments()[i]->type() == Item::FIELD_ITEM) {
                   func_item->arguments()[i] = FindEqualField(
                       down_cast<Item_field *>(func_item->arguments()[i]),
                       available_tables, replace, found);
                   if (*found == false && !replace) return true;
                 }
               }
             }
             return false;
           });
}

static void ensure_multi_equality_fields_are_available(
    Item **args, int arg_idx, table_map available_tables, bool replace,
    bool *found) {
  if (args[arg_idx]->type() == Item::FIELD_ITEM) {
    // The argument we want to find and adjust is an Item_field. Create a
    // new Item_field with a field that is reachable if "replace" is
    // set to true. Else, set "found" to true if a field is found.
    args[arg_idx] = FindEqualField(down_cast<Item_field *>(args[arg_idx]),
                                   available_tables, replace, found);
  } else {
    // The argument is not a field item. Walk down the item tree and see if we
    // find any Item_field that needs adjustment.
    find_and_adjust_equal_fields(args[arg_idx], available_tables, replace,
                                 found);
  }
}

void Item_func_eq::ensure_multi_equality_fields_are_available(
    table_map left_side_tables, table_map right_side_tables, bool replace,
    bool *found) {
  const table_map left_arg_used_tables = args[0]->used_tables();
  const table_map right_arg_used_tables = args[1]->used_tables();

  if (left_arg_used_tables == 0 || right_arg_used_tables == 0) {
    // This is a filter, not a join condition.
    *found = false;
    return;
  }

  if (IsSubset(left_arg_used_tables, left_side_tables) &&
      IsSubset(right_arg_used_tables, right_side_tables)) {
    // The left argument matches the left side tables, and the
    // right one to the right side tables. This can stay
    // on this join.
    *found = true;
  } else if (IsSubset(left_arg_used_tables, right_side_tables) &&
             IsSubset(right_arg_used_tables, left_side_tables)) {
    // The left argument matches the right side tables, and the
    // right one to the left side tables. This can stay
    // on this join.
    *found = true;
  } else if (IsSubset(left_arg_used_tables, left_side_tables) &&
             !IsSubset(right_arg_used_tables, right_side_tables)) {
    // The left argument matches the left side tables, so find an
    // "equal" field from right side tables. Adjust the right side
    // with the equal field if "replace" is set to true.
    ::ensure_multi_equality_fields_are_available(
        args, /*arg_idx=*/1, right_side_tables, replace, found);
  } else if (IsSubset(left_arg_used_tables, right_side_tables) &&
             !IsSubset(right_arg_used_tables, left_side_tables)) {
    // The left argument matches the right side tables, so find an
    // "equal" field from the left side tables. Adjust the right side
    // with the equal field if "replace" is set to true.
    ::ensure_multi_equality_fields_are_available(
        args, /*arg_idx=*/1, left_side_tables, replace, found);
  } else if (IsSubset(right_arg_used_tables, left_side_tables) &&
             !IsSubset(left_arg_used_tables, right_side_tables)) {
    // The right argument matches the left side tables, so find an
    // "equal" field from the right side tables. Adjust the left side
    // with the equal field if "replace" is set to true.
    ::ensure_multi_equality_fields_are_available(
        args, /*arg_idx=*/0, right_side_tables, replace, found);
  } else if (IsSubset(right_arg_used_tables, right_side_tables) &&
             !IsSubset(left_arg_used_tables, left_side_tables)) {
    // The right argument matches the right side tables, so find an
    // "equal" field from the left side tables. Adjust the left side
    // with the equal field if "replace" is set to true.
    ::ensure_multi_equality_fields_are_available(
        args, /*arg_idx=*/0, left_side_tables, replace, found);
  }

  // We must update used_tables in case we replaced any of the fields in this
  // join condition.
  if (replace) update_used_tables();
}
