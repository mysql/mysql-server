/* Copyright (c) 2000, 2015, Oracle and/or its affiliates. All rights reserved.

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
  @file

  @brief
  This file defines all compare functions
*/

#include "sql_priv.h"
#include <m_ctype.h>
#include "sql_select.h"
#include "sql_optimizer.h"             // JOIN_TAB
#include "sql_parse.h"                          // check_stack_overrun
#include "sql_time.h"                  // make_truncated_value_warning

#include <algorithm>
using std::min;
using std::max;

static bool convert_constant_item(THD *, Item_field *, Item **);
static longlong
get_year_value(THD *thd, Item ***item_arg, Item **cache_arg,
               Item *warn_item, bool *is_null);

static Item_result item_store_type(Item_result a, Item *item,
                                   my_bool unsigned_flag)
{
  Item_result b= item->result_type();

  if (a == STRING_RESULT || b == STRING_RESULT)
    return STRING_RESULT;
  else if (a == REAL_RESULT || b == REAL_RESULT)
    return REAL_RESULT;
  else if (a == DECIMAL_RESULT || b == DECIMAL_RESULT ||
           unsigned_flag != item->unsigned_flag)
    return DECIMAL_RESULT;
  else
    return INT_RESULT;
}

static void agg_result_type(Item_result *type, Item **items, uint nitems)
{
  Item **item, **item_end;
  my_bool unsigned_flag= 0;

  *type= STRING_RESULT;
  /* Skip beginning NULL items */
  for (item= items, item_end= item + nitems; item < item_end; item++)
  {
    if ((*item)->type() != Item::NULL_ITEM)
    {
      *type= (*item)->result_type();
      unsigned_flag= (*item)->unsigned_flag;
      item++;
      break;
    }
  }
  /* Combine result types. Note: NULL items don't affect the result */
  for (; item < item_end; item++)
  {
    if ((*item)->type() != Item::NULL_ITEM)
      *type= item_store_type(*type, *item, unsigned_flag);
  }
}


/*
  Compare row signature of two expressions

  SYNOPSIS:
    cmp_row_type()
    item1          the first expression
    item2         the second expression

  DESCRIPTION
    The function checks that two expressions have compatible row signatures
    i.e. that the number of columns they return are the same and that if they
    are both row expressions then each component from the first expression has 
    a row signature compatible with the signature of the corresponding component
    of the second expression.

  RETURN VALUES
    1  type incompatibility has been detected
    0  otherwise
*/

static int cmp_row_type(Item* item1, Item* item2)
{
  uint n= item1->cols();
  if (item2->check_cols(n))
    return 1;
  for (uint i=0; i<n; i++)
  {
    if (item2->element_index(i)->check_cols(item1->element_index(i)->cols()) ||
        (item1->element_index(i)->result_type() == ROW_RESULT &&
         cmp_row_type(item1->element_index(i), item2->element_index(i))))
      return 1;
  }
  return 0;
}


/**
  Aggregates result types from the array of items.

  SYNOPSIS:
    agg_cmp_type()
    type   [out] the aggregated type
    items        array of items to aggregate the type from
    nitems       number of items in the array

  DESCRIPTION
    This function aggregates result types from the array of items. Found type
    supposed to be used later for comparison of values of these items.
    Aggregation itself is performed by the item_cmp_type() function.
  @param[out] type    the aggregated type
  @param      items        array of items to aggregate the type from
  @param      nitems       number of items in the array

  @retval
    1  type incompatibility has been detected
  @retval
    0  otherwise
*/

static int agg_cmp_type(Item_result *type, Item **items, uint nitems)
{
  uint i;
  type[0]= items[0]->result_type();
  for (i= 1 ; i < nitems ; i++)
  {
    type[0]= item_cmp_type(type[0], items[i]->result_type());
    /*
      When aggregating types of two row expressions we have to check
      that they have the same cardinality and that each component
      of the first row expression has a compatible row signature with
      the signature of the corresponding component of the second row
      expression.
    */ 
    if (type[0] == ROW_RESULT && cmp_row_type(items[0], items[i]))
      return 1;     // error found: invalid usage of rows
  }
  return 0;
}


/**
  @brief Aggregates field types from the array of items.

  @param[in] items  array of items to aggregate the type from
  @paran[in] nitems number of items in the array

  @details This function aggregates field types from the array of items.
    Found type is supposed to be used later as the result field type
    of a multi-argument function.
    Aggregation itself is performed by the Field::field_type_merge()
    function.

  @note The term "aggregation" is used here in the sense of inferring the
    result type of a function from its argument types.

  @return aggregated field type.
*/

enum_field_types agg_field_type(Item **items, uint nitems)
{
  uint i;
  if (!nitems || items[0]->result_type() == ROW_RESULT )
    return (enum_field_types)-1;
  enum_field_types res= items[0]->field_type();
  for (i= 1 ; i < nitems ; i++)
    res= Field::field_type_merge(res, items[i]->field_type());
  return real_type_to_type(res);
}

/*
  Collects different types for comparison of first item with each other items

  SYNOPSIS
    collect_cmp_types()
      items             Array of items to collect types from
      nitems            Number of items in the array
      skip_nulls        Don't collect types of NULL items if TRUE

  DESCRIPTION
    This function collects different result types for comparison of the first
    item in the list with each of the remaining items in the 'items' array.

  RETURN
    0 - if row type incompatibility has been detected (see cmp_row_type)
    Bitmap of collected types - otherwise
*/

static uint collect_cmp_types(Item **items, uint nitems, bool skip_nulls= FALSE)
{
  uint i;
  uint found_types;
  Item_result left_result= items[0]->result_type();
  DBUG_ASSERT(nitems > 1);
  found_types= 0;
  for (i= 1; i < nitems ; i++)
  {
    if (skip_nulls && items[i]->type() == Item::NULL_ITEM)
      continue; // Skip NULL constant items
    if ((left_result == ROW_RESULT || 
         items[i]->result_type() == ROW_RESULT) &&
        cmp_row_type(items[0], items[i]))
      return 0;
    found_types|= 1U << (uint)item_cmp_type(left_result,
                                            items[i]->result_type());
  }
  /*
   Even if all right-hand items are NULLs and we are skipping them all, we need
   at least one type bit in the found_type bitmask.
  */
  if (skip_nulls && !found_types)
    found_types= 1U << (uint)left_result;
  return found_types;
}

static void my_coll_agg_error(DTCollation &c1, DTCollation &c2,
                              const char *fname)
{
  my_error(ER_CANT_AGGREGATE_2COLLATIONS, MYF(0),
           c1.collation->name,c1.derivation_name(),
           c2.collation->name,c2.derivation_name(),
           fname);
}


Item_bool_func2* Eq_creator::create(Item *a, Item *b) const
{
  return new Item_func_eq(a, b);
}


Item_bool_func2* Ne_creator::create(Item *a, Item *b) const
{
  return new Item_func_ne(a, b);
}


Item_bool_func2* Gt_creator::create(Item *a, Item *b) const
{
  return new Item_func_gt(a, b);
}


Item_bool_func2* Lt_creator::create(Item *a, Item *b) const
{
  return new Item_func_lt(a, b);
}


Item_bool_func2* Ge_creator::create(Item *a, Item *b) const
{
  return new Item_func_ge(a, b);
}


Item_bool_func2* Le_creator::create(Item *a, Item *b) const
{
  return new Item_func_le(a, b);
}

/*
  Test functions
  Most of these  returns 0LL if false and 1LL if true and
  NULL if some arg is NULL.
*/

longlong Item_func_not::val_int()
{
  DBUG_ASSERT(fixed == 1);
  bool value= args[0]->val_bool();
  null_value=args[0]->null_value;
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

void Item_func_not::print(String *str, enum_query_type query_type)
{
  str->append('(');
  Item_func::print(str, query_type);
  str->append(')');
}

/**
  special NOT for ALL subquery.
*/


longlong Item_func_not_all::val_int()
{
  DBUG_ASSERT(fixed == 1);
  bool value= args[0]->val_bool();

  /*
    return TRUE if there was records in underlying select in max/min
    optimization (ALL subquery)
  */
  if (empty_underlying_subquery())
    return 1;

  null_value= args[0]->null_value;
  return ((!null_value && value == 0) ? 1 : 0);
}


bool Item_func_not_all::empty_underlying_subquery()
{
  DBUG_ASSERT(subselect || !(test_sum_item || test_sub_item));
  /*
   When outer argument is NULL the subquery has not yet been evaluated, we
   need to evaluate it to get to know whether it returns any rows to return
   the correct result. 'ANY' subqueries are an exception because the
   result would be false or null which for a top level item always mean false.
   The subselect->unit->item->... chain should be used instead of
   subselect->... to workaround subquery transformation which could make
   subselect->engine unusable.
  */
  if (subselect && 
      subselect->substype() != Item_subselect::ANY_SUBS &&
      !subselect->unit->item->is_evaluated())
    subselect->unit->item->exec();
  return ((test_sum_item && !test_sum_item->any_value()) ||
          (test_sub_item && !test_sub_item->any_value()));
}

void Item_func_not_all::print(String *str, enum_query_type query_type)
{
  if (show)
    Item_func::print(str, query_type);
  else
    args[0]->print(str, query_type);
}


/**
  Special NOP (No OPeration) for ALL subquery. It is like
  Item_func_not_all.

  @return
    (return TRUE if underlying subquery do not return rows) but if subquery
    returns some rows it return same value as argument (TRUE/FALSE).
*/

longlong Item_func_nop_all::val_int()
{
  DBUG_ASSERT(fixed == 1);
  longlong value= args[0]->val_int();

  /*
    return FALSE if there was records in underlying select in max/min
    optimization (SAME/ANY subquery)
  */
  if (empty_underlying_subquery())
    return 0;

  null_value= args[0]->null_value;
  return (null_value || value == 0) ? 0 : 1;
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
  @param  field           item will be converted using the type of this field
  @param[in,out] item     reference to the item to convert

  @note
    This function is called only at prepare stage.
    As all derived tables are filled only after all derived tables
    are prepared we do not evaluate items with subselects here because
    they can contain derived tables and thus we may attempt to use a
    table that has not been populated yet.

  @retval
    0  Can't convert item
  @retval
    1  Item was replaced with an integer version of the item
*/

static bool convert_constant_item(THD *thd, Item_field *field_item,
                                  Item **item)
{
  Field *field= field_item->field;
  int result= 0;

  if ((*item)->const_item())
  {
    TABLE *table= field->table;
    sql_mode_t orig_sql_mode= thd->variables.sql_mode;
    enum_check_fields orig_count_cuted_fields= thd->count_cuted_fields;
    my_bitmap_map *old_maps[2];
    ulonglong UNINIT_VAR(orig_field_val); /* original field value if valid */

    LINT_INIT(old_maps[0]);
    LINT_INIT(old_maps[1]);

    if (table)
      dbug_tmp_use_all_columns(table, old_maps, 
                               table->read_set, table->write_set);
    /* For comparison purposes allow invalid dates like 2000-01-32 */
    thd->variables.sql_mode= (orig_sql_mode & ~MODE_NO_ZERO_DATE) | 
                             MODE_INVALID_DATES;
    thd->count_cuted_fields= CHECK_FIELD_IGNORE;

    /*
      Store the value of the field/constant if it references an outer field
      because the call to save_in_field below overrides that value.
      Don't save field value if no data has been read yet.
      Outer constant values are always saved.
    */
    bool save_field_value= (field_item->depended_from &&
                            (field_item->const_item() ||
                             !(field->table->status &
                               (STATUS_GARBAGE | STATUS_NOT_FOUND))));
    if (save_field_value)
      orig_field_val= field->val_int();
    int rc;
    if (!(*item)->is_null() &&
        (((rc= (*item)->save_in_field(field, 1)) == TYPE_OK) ||
         rc == TYPE_NOTE_TIME_TRUNCATED)) // TS-TODO
    {
      int field_cmp= 0;
      /*
        If item is a decimal value, we must reject it if it was truncated.
        TODO: consider doing the same for MYSQL_TYPE_YEAR,.
        However: we have tests which assume that things '1999' and
        '1991-01-01 01:01:01' can be converted to year.
        Testing for MYSQL_TYPE_YEAR here, would treat such literals
        as 'incorrect DOUBLE value'.
        See Bug#13580652 YEAR COLUMN CAN BE EQUAL TO 1999.1
      */
      if (field->type() == MYSQL_TYPE_LONGLONG)
      {
        field_cmp= stored_field_cmp_to_item(thd, field, *item);
        DBUG_PRINT("info", ("convert_constant_item %d", field_cmp));
      }

      if (0 == field_cmp)
      {
        Item *tmp= field->type() == MYSQL_TYPE_TIME ?
#define OLD_CMP
#ifdef OLD_CMP
          new Item_time_with_ref(field->decimals(),
                                 field->val_time_temporal(), *item) :
#else
          new Item_time_with_ref(max((*item)->time_precision(),
                                     field->decimals()),
                                 (*item)->val_time_temporal(),
                                 *item) :
#endif
          field->is_temporal_with_date() ?
#ifdef OLD_CMP
          new Item_datetime_with_ref(field->type(),
                                     field->decimals(),
                                     field->val_date_temporal(),
                                     *item) :
#else
          new Item_datetime_with_ref(field->type(),
                                     max((*item)->datetime_precision(),
                                         field->decimals()),
                                     (*item)->val_date_temporal(),
                                     *item) :
#endif
          new Item_int_with_ref(field->val_int(), *item,
                                MY_TEST(field->flags & UNSIGNED_FLAG));
        if (tmp)
          thd->change_item_tree(item, tmp);
        result= 1;                              // Item was replaced
      }
    }
    /* Restore the original field value. */
    if (save_field_value)
    {
      result= field->store(orig_field_val, TRUE);
      /* orig_field_val must be a valid value that can be restored back. */
      DBUG_ASSERT(!result);
    }
    thd->variables.sql_mode= orig_sql_mode;
    thd->count_cuted_fields= orig_count_cuted_fields;
    if (table)
      dbug_tmp_restore_column_maps(table->read_set, table->write_set, old_maps);
  }
  return result;
}


bool Item_bool_func2::convert_constant_arg(THD *thd, Item *field, Item **item)
{
  if (field->real_item()->type() != FIELD_ITEM)
    return false;

  Item_field *field_item= (Item_field*) (field->real_item());
  if (field_item->field->can_be_compared_as_longlong() &&
      !(field_item->is_temporal_with_date() &&
      (*item)->result_type() == STRING_RESULT))
  {
    if (convert_constant_item(thd, field_item, item))
    {
      cmp.set_cmp_func(this, tmp_arg, tmp_arg + 1, INT_RESULT);
      field->cmp_context= (*item)->cmp_context= INT_RESULT;
      return true;
    }
  }
  return false;
}


void Item_bool_func2::fix_length_and_dec()
{
  max_length= 1;				     // Function returns 0 or 1
  THD *thd;

  /*
    As some compare functions are generated after sql_yacc,
    we have to check for out of memory conditions here
  */
  if (!args[0] || !args[1])
    return;

  DBUG_ENTER("Item_bool_func2::fix_length_and_dec");

  /*
    See agg_item_charsets() in item.cc for comments
    on character set and collation aggregation.
  */
  if (args[0]->result_type() == STRING_RESULT &&
      args[1]->result_type() == STRING_RESULT &&
      agg_arg_charsets_for_comparison(cmp.cmp_collation, args, 2))
    DBUG_VOID_RETURN;
    
  args[0]->cmp_context= args[1]->cmp_context=
    item_cmp_type(args[0]->result_type(), args[1]->result_type());
  // Make a special case of compare with fields to get nicer DATE comparisons

  if (functype() == LIKE_FUNC)  // Disable conversion in case of LIKE function.
  {
    set_cmp_func();
    DBUG_VOID_RETURN;
  }

  thd= current_thd;
  if (!thd->lex->is_ps_or_view_context_analysis())
  {
    if (convert_constant_arg(thd, args[0], &args[1]) ||
        convert_constant_arg(thd, args[1], &args[0]))
      DBUG_VOID_RETURN;
  }
  set_cmp_func();
  DBUG_VOID_RETURN;
}


int Arg_comparator::set_compare_func(Item_result_field *item, Item_result type)
{
  owner= item;
  func= comparator_matrix[type]
                         [is_owner_equal_func()];

  switch (type) {
  case ROW_RESULT:
  {
    uint n= (*a)->cols();
    if (n != (*b)->cols())
    {
      my_error(ER_OPERAND_COLUMNS, MYF(0), n);
      comparators= 0;
      return 1;
    }
    if (!(comparators= new Arg_comparator[n]))
      return 1;
    for (uint i=0; i < n; i++)
    {
      if ((*a)->element_index(i)->cols() != (*b)->element_index(i)->cols())
      {
	my_error(ER_OPERAND_COLUMNS, MYF(0), (*a)->element_index(i)->cols());
	return 1;
      }
      if (comparators[i].set_cmp_func(owner, (*a)->addr(i), (*b)->addr(i),
                                      set_null))
        return 1;
    }
    break;
  }
  case STRING_RESULT:
  {
    /*
      We must set cmp_charset here as we may be called from for an automatic
      generated item, like in natural join
    */
    if (cmp_collation.set((*a)->collation, (*b)->collation) || 
	cmp_collation.derivation == DERIVATION_NONE)
    {
      my_coll_agg_error((*a)->collation, (*b)->collation,
                        owner->func_name());
      return 1;
    }
    if (cmp_collation.collation == &my_charset_bin)
    {
      /*
	We are using BLOB/BINARY/VARBINARY, change to compare byte by byte,
	without removing end space
      */
      if (func == &Arg_comparator::compare_string)
	func= &Arg_comparator::compare_binary_string;
      else if (func == &Arg_comparator::compare_e_string)
	func= &Arg_comparator::compare_e_binary_string;

      /*
        As this is binary compassion, mark all fields that they can't be
        transformed. Otherwise we would get into trouble with comparisons
        like:
        WHERE col= 'j' AND col LIKE BINARY 'j'
        which would be transformed to:
        WHERE col= 'j'
      */
      (*a)->walk(&Item::set_no_const_sub, FALSE, (uchar*) 0);
      (*b)->walk(&Item::set_no_const_sub, FALSE, (uchar*) 0);
    }
    break;
  }
  case INT_RESULT:
  {
    if ((*a)->is_temporal() && (*b)->is_temporal())
    {
      func= is_owner_equal_func() ?
            &Arg_comparator::compare_e_time_packed :
            &Arg_comparator::compare_time_packed;
    }
    else if (func == &Arg_comparator::compare_int_signed)
    {
      if ((*a)->unsigned_flag)
        func= (((*b)->unsigned_flag)?
               &Arg_comparator::compare_int_unsigned :
               &Arg_comparator::compare_int_unsigned_signed);
      else if ((*b)->unsigned_flag)
        func= &Arg_comparator::compare_int_signed_unsigned;
    }
    else if (func== &Arg_comparator::compare_e_int)
    {
      if ((*a)->unsigned_flag ^ (*b)->unsigned_flag)
        func= &Arg_comparator::compare_e_int_diff_signedness;
    }
    break;
  }
  case DECIMAL_RESULT:
    break;
  case REAL_RESULT:
  {
    if ((*a)->decimals < NOT_FIXED_DEC && (*b)->decimals < NOT_FIXED_DEC)
    {
      precision= 5 / log_10[max((*a)->decimals, (*b)->decimals) + 1];
      if (func == &Arg_comparator::compare_real)
        func= &Arg_comparator::compare_real_fixed;
      else if (func == &Arg_comparator::compare_e_real)
        func= &Arg_comparator::compare_e_real_fixed;
    }
    break;
  }
  default:
    DBUG_ASSERT(0);
  }
  return 0;
}

/**
  Parse date provided in a string to a MYSQL_TIME.

  @param[in]   thd        Thread handle
  @param[in]   str        A string to convert
  @param[in]   warn_type  Type of the timestamp for issuing the warning
  @param[in]   warn_name  Field name for issuing the warning
  @param[out]  l_time     The MYSQL_TIME objects is initialized.

  Parses a date provided in the string str into a MYSQL_TIME object. If the
  string contains an incorrect date or doesn't correspond to a date at all
  then a warning is issued. The warn_type and the warn_name arguments are used
  as the name and the type of the field when issuing the warning. If any input
  was discarded (trailing or non-timestamp-y characters), return value will be
  TRUE.

  @return Status flag
  @retval FALSE Success.
  @retval True Indicates failure.
*/

bool get_mysql_time_from_str(THD *thd, String *str, timestamp_type warn_type, 
                             const char *warn_name, MYSQL_TIME *l_time)
{
  bool value;
  MYSQL_TIME_STATUS status;

  if (!str_to_datetime(str, l_time,
                      (TIME_FUZZY_DATE | MODE_INVALID_DATES |
                       (thd->variables.sql_mode &
                       (MODE_NO_ZERO_IN_DATE | MODE_NO_ZERO_DATE))), &status) &&
      (l_time->time_type == MYSQL_TIMESTAMP_DATETIME || 
       l_time->time_type == MYSQL_TIMESTAMP_DATE))
    /*
      Do not return yet, we may still want to throw a "trailing garbage"
      warning.
    */
    value= FALSE;
  else
  {
    value= TRUE;
    status.warnings= MYSQL_TIME_WARN_TRUNCATED;  /* force warning */
  }

  if (status.warnings > 0)
    make_truncated_value_warning(thd, Sql_condition::WARN_LEVEL_WARN,
                                 ErrConvString(str), warn_type, warn_name);

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
                                   timestamp_type warn_type, 
                                   const char *warn_name, bool *error_arg)
{
  MYSQL_TIME l_time;
  *error_arg= get_mysql_time_from_str(thd, str, warn_type, warn_name, &l_time);

  if (*error_arg)
    return 0;
  return TIME_to_longlong_datetime_packed(&l_time);
}


/**
  Check if str_arg is a constant and convert it to datetime packed value.
  Note, const_value may stay untouched, so the caller is responsible to
  initialize it.

  @param date_arg        - date argument, it's name is used for error reporting.
  @param str_arg         - string argument to get datetime value from.
  @param OUT const_value - the converted value is stored here, if not NULL.

  @return true on error, false on success, false if str_arg is not a const.
*/
bool Arg_comparator::get_date_from_const(Item *date_arg,
                                         Item *str_arg,
                                         ulonglong *const_value)
{
  THD *thd= current_thd;
  /*
    Do not cache GET_USER_VAR() function as its const_item() may return TRUE
    for the current thread but it still may change during the execution.
    Don't use cache while in the context analysis mode only (i.e. for 
    EXPLAIN/CREATE VIEW and similar queries). Cache is useless in such 
    cases and can cause problems. For example evaluating subqueries can 
    confuse storage engines since in context analysis mode tables 
    aren't locked.
  */
  if (!thd->lex->is_ps_or_view_context_analysis() &&
      str_arg->const_item() &&
      (str_arg->type() != Item::FUNC_ITEM ||
      ((Item_func*) str_arg)->functype() != Item_func::GUSERVAR_FUNC))
  {
    ulonglong value;
    if (str_arg->field_type() == MYSQL_TYPE_TIME)
    {
      // Convert from TIME to DATETIME
      value= str_arg->val_date_temporal();
      if (str_arg->null_value)
        return true;
    }
    else
    {
      // Convert from string to DATETIME
      DBUG_ASSERT(str_arg->result_type() == STRING_RESULT);
      bool error;
      String tmp, *str_val= 0;
      timestamp_type t_type= (date_arg->field_type() == MYSQL_TYPE_DATE ?
                              MYSQL_TIMESTAMP_DATE : MYSQL_TIMESTAMP_DATETIME);
      str_val= str_arg->val_str(&tmp);
      if (str_arg->null_value)
        return true;
      value= get_date_from_str(thd, str_val, t_type,
                               date_arg->item_name.ptr(), &error);
      if (error)
        return true;
    }
    if (const_value)
      *const_value= value;
  }
  return false;
}


/*
  Check whether compare_datetime() can be used to compare items.

  SYNOPSIS
    Arg_comparator::can_compare_as_dates()
    a, b          [in]  items to be compared
    const_value   [out] converted value of the string constant, if any

  DESCRIPTION
    Check several cases when the DATE/DATETIME comparator should be used.
    The following cases are checked:
      1. Both a and b is a DATE/DATETIME field/function returning string or
         int result.
      2. Only a or b is a DATE/DATETIME field/function returning string or
         int result and the other item (b or a) is an item with string result.
         If the second item is a constant one then it's checked to be
         convertible to the DATE/DATETIME type. If the constant can't be
         converted to a DATE/DATETIME then the compare_datetime() comparator
         isn't used and the warning about wrong DATE/DATETIME value is issued.
      In all other cases (date-[int|real|decimal]/[int|real|decimal]-date)
      the comparison is handled by other comparators.
    If the datetime comparator can be used and one the operands of the
    comparison is a string constant that was successfully converted to a
    DATE/DATETIME type then the result of the conversion is returned in the
    const_value if it is provided.  If there is no constant or
    compare_datetime() isn't applicable then the *const_value remains
    unchanged.

  @return true if can compare as dates, false otherwise.
*/

bool
Arg_comparator::can_compare_as_dates(Item *a, Item *b, ulonglong *const_value)
{
  if (a->type() == Item::ROW_ITEM || b->type() == Item::ROW_ITEM)
    return false;

  if (a->is_temporal_with_date())
  {
    if (b->is_temporal_with_date()) //  date[time] + date
    {
      return true;
    }
    else if (b->result_type() == STRING_RESULT) // date[time] + string
    {
      return !get_date_from_const(a, b, const_value);
    }
    else
      return false; // date[time] + number
  }
  else if (b->is_temporal_with_date() &&
           a->result_type() == STRING_RESULT) // string + date[time]
  {
    return !get_date_from_const(b, a, const_value);
  }
  else
    return false; // No date[time] items found
}


/*
  Retrieves correct TIME value from the given item.

  SYNOPSIS
    get_time_value()
    thd                 thread handle
    item_arg   [in/out] item to retrieve TIME value from
    cache_arg  [in/out] pointer to place to store the cache item to
    warn_item  [in]     unused
    is_null    [out]    TRUE <=> the item_arg is null

  DESCRIPTION
    Retrieves the correct TIME value from given item for comparison by the
    compare_datetime() function.
    If item's result can be compared as longlong then its int value is used
    and a value returned by get_time function is used otherwise.
    If an item is a constant one then its value is cached and it isn't
    get parsed again. An Item_cache_int object is used for for cached values.
    It seamlessly substitutes the original item.  The cache item is marked as
    non-constant to prevent re-caching it again.

  RETURN
    obtained value
*/

longlong
get_time_value(THD *thd, Item ***item_arg, Item **cache_arg,
               Item *warn_item, bool *is_null)
{
  longlong value;
  Item *item= **item_arg;

  /*
    Note, it's wrong to assume that we always get
    a TIME expression or NULL here:

  DBUG_ASSERT(item->field_type() == MYSQL_TYPE_TIME || 
              item->field_type() == MYSQL_TYPE_NULL);

    because when this condition is optimized:

    WHERE time_column=DATE(NULL) AND time_column=TIME(NULL);

    rhe first AND part is eliminated and DATE(NULL) is substituted
    to the second AND part like this:

    WHERE DATE(NULL) = TIME(NULL) // as TIME

    whose Arg_comparator has already get_time_value set for both arguments.
    Therefore, get_time_value is executed for DATE(NULL).
    This condition is further evaluated as impossible condition.

    TS-TODO: perhaps such cases should be evaluated without
    calling get_time_value at all.

    See a similar comment in Arg_comparator::compare_time_packed.
  */
  value= item->val_time_temporal();
  *is_null= item->null_value;

  /*
    Do not cache GET_USER_VAR() function as its const_item() may return TRUE
    for the current thread but it still may change during the execution.
  */
  if (item->const_item() && cache_arg &&
      item->type() != Item::CACHE_ITEM &&
      (item->type() != Item::FUNC_ITEM ||
       ((Item_func*)item)->functype() != Item_func::GUSERVAR_FUNC))
  {
    Item_cache_datetime *cache= new Item_cache_datetime(item->field_type());
    /* Mark the cache as non-const to prevent re-caching. */
    cache->set_used_tables(1);
    cache->store(item, value);
    *cache_arg= cache;
    *item_arg= cache_arg;
  }
  return value;
}


int Arg_comparator::set_cmp_func(Item_result_field *owner_arg,
                                        Item **a1, Item **a2,
                                        Item_result type)
{
  ulonglong const_value= (ulonglong)-1;
  thd= current_thd;
  owner= owner_arg;
  set_null= set_null && owner_arg;
  a= a1;
  b= a2;

  if (can_compare_as_dates(*a, *b, &const_value))
  {
    a_type= (*a)->field_type();
    b_type= (*b)->field_type();
    a_cache= 0;
    b_cache= 0;

    if (const_value != (ulonglong)-1)
    {
      /*
        cache_converted_constant can't be used here because it can't
        correctly convert a DATETIME value from string to int representation.
      */
      Item_cache_datetime *cache= new Item_cache_datetime(MYSQL_TYPE_DATETIME);
      /* Mark the cache as non-const to prevent re-caching. */
      cache->set_used_tables(1);
      if (!(*a)->is_temporal_with_date())
      {
        cache->store((*a), const_value);
        a_cache= cache;
        a= (Item **)&a_cache;
      }
      else
      {
        cache->store((*b), const_value);
        b_cache= cache;
        b= (Item **)&b_cache;
      }
    }
    is_nulls_eq= is_owner_equal_func();
    func= &Arg_comparator::compare_datetime;
    get_value_a_func= &get_datetime_value;
    get_value_b_func= &get_datetime_value;
    cmp_collation.set(&my_charset_numeric);
    set_cmp_context_for_datetime();
    return 0;
  }
  else if (type == STRING_RESULT && (*a)->field_type() == MYSQL_TYPE_TIME &&
           (*b)->field_type() == MYSQL_TYPE_TIME)
  {
    /* Compare TIME values as integers. */
    a_cache= 0;
    b_cache= 0;
    is_nulls_eq= is_owner_equal_func();
    func= &Arg_comparator::compare_datetime;
    get_value_a_func= &get_time_value;
    get_value_b_func= &get_time_value;
    set_cmp_context_for_datetime();
    return 0;
  }
  else if (type == STRING_RESULT &&
           (*a)->result_type() == STRING_RESULT &&
           (*b)->result_type() == STRING_RESULT)
  {
    DTCollation coll;
    coll.set((*a)->collation.collation);
    if (agg_item_set_converter(coll, owner->func_name(),
                               b, 1, MY_COLL_CMP_CONV, 1))
      return 1;
  }
  else if (try_year_cmp_func(type))
    return 0;

  a= cache_converted_constant(thd, a, &a_cache, type);
  b= cache_converted_constant(thd, b, &b_cache, type);
  return set_compare_func(owner_arg, type);
}


/*
  Helper function to call from Arg_comparator::set_cmp_func()
*/

bool Arg_comparator::try_year_cmp_func(Item_result type)
{
  if (type == ROW_RESULT)
    return FALSE;

  bool a_is_year= (*a)->field_type() == MYSQL_TYPE_YEAR;
  bool b_is_year= (*b)->field_type() == MYSQL_TYPE_YEAR;

  if (!a_is_year && !b_is_year)
    return FALSE;

  if (a_is_year && b_is_year)
  {
    get_value_a_func= &get_year_value;
    get_value_b_func= &get_year_value;
  }
  else if (a_is_year && (*b)->is_temporal_with_date())
  {
    get_value_a_func= &get_year_value;
    get_value_b_func= &get_datetime_value;
  }
  else if (b_is_year && (*a)->is_temporal_with_date())
  {
    get_value_b_func= &get_year_value;
    get_value_a_func= &get_datetime_value;
  }
  else
    return FALSE;

  is_nulls_eq= is_owner_equal_func();
  func= &Arg_comparator::compare_datetime;
  set_cmp_context_for_datetime();

  return TRUE;
}

/**
  Convert and cache a constant.

  @param value      [in]  An item to cache
  @param cache_item [out] Placeholder for the cache item
  @param type       [in]  Comparison type

  @details
    When given item is a constant and its type differs from comparison type
    then cache its value to avoid type conversion of this constant on each
    evaluation. In this case the value is cached and the reference to the cache
    is returned.
    Original value is returned otherwise.

  @return cache item or original value.
*/

Item** Arg_comparator::cache_converted_constant(THD *thd_arg, Item **value,
                                                Item **cache_item,
                                                Item_result type)
{
  /* Don't need cache if doing context analysis only. */
  if (!thd->lex->is_ps_or_view_context_analysis() &&
      (*value)->const_item() && type != (*value)->result_type())
  {
    Item_cache *cache= Item_cache::get_cache(*value, type);
    cache->setup(*value);
    *cache_item= cache;
    return cache_item;
  }
  return value;
}


void Arg_comparator::set_datetime_cmp_func(Item_result_field *owner_arg,
                                           Item **a1, Item **b1)
{
  thd= current_thd;
  owner= owner_arg;
  a= a1;
  b= b1;
  a_type= (*a)->field_type();
  b_type= (*b)->field_type();
  a_cache= 0;
  b_cache= 0;
  is_nulls_eq= FALSE;
  func= &Arg_comparator::compare_datetime;
  get_value_a_func= &get_datetime_value;
  get_value_b_func= &get_datetime_value;
  set_cmp_context_for_datetime();
}


/*
  Retrieves correct DATETIME value from given item.

  SYNOPSIS
    get_datetime_value()
    thd                 thread handle
    item_arg   [in/out] item to retrieve DATETIME value from
    cache_arg  [in/out] pointer to place to store the caching item to
    warn_item  [in]     item for issuing the conversion warning
    is_null    [out]    TRUE <=> the item_arg is null

  DESCRIPTION
    Retrieves the correct DATETIME value from given item for comparison by the
    compare_datetime() function.
    If item's result can be compared as longlong then its int value is used
    and its string value is used otherwise. Strings are always parsed and
    converted to int values by the get_date_from_str() function.
    This allows us to compare correctly string dates with missed insignificant
    zeros. If an item is a constant one then its value is cached and it isn't
    get parsed again. An Item_cache_int object is used for caching values. It
    seamlessly substitutes the original item.  The cache item is marked as
    non-constant to prevent re-caching it again.  In order to compare
    correctly DATE and DATETIME items the result of the former are treated as
    a DATETIME with zero time (00:00:00).

  RETURN
    obtained value
*/


longlong
get_datetime_value(THD *thd, Item ***item_arg, Item **cache_arg,
                   Item *warn_item, bool *is_null)
{
  longlong value= 0;
  String buf, *str= 0;
  Item *item= **item_arg;

  if (item->is_temporal())
  {
    value= item->val_date_temporal();
    *is_null= item->null_value;
  }
  else
  {
    str= item->val_str(&buf);
    *is_null= item->null_value;
  }
  if (*is_null)
    return ~(ulonglong) 0;
  /*
    Convert strings to the integer DATE/DATETIME representation.
    Even if both dates provided in strings we can't compare them directly as
    strings as there is no warranty that they are correct and do not miss
    some insignificant zeros.
  */
  if (str)
  {
    bool error;
    enum_field_types f_type= warn_item->field_type();
    timestamp_type t_type= f_type ==
      MYSQL_TYPE_DATE ? MYSQL_TIMESTAMP_DATE : MYSQL_TIMESTAMP_DATETIME;
    value= (longlong) get_date_from_str(thd, str, t_type, warn_item->item_name.ptr(), &error);
    /*
      If str did not contain a valid date according to the current
      SQL_MODE, get_date_from_str() has already thrown a warning,
      and we don't want to throw NULL on invalid date (see 5.2.6
      "SQL modes" in the manual), so we're done here.
    */
  }
  /*
    Do not cache GET_USER_VAR() function as its const_item() may return TRUE
    for the current thread but it still may change during the execution.
  */
  if (item->const_item() && cache_arg &&
      item->type() != Item::CACHE_ITEM &&
      (item->type() != Item::FUNC_ITEM ||
       ((Item_func*)item)->functype() != Item_func::GUSERVAR_FUNC))
  {
    Item_cache_datetime *cache= new Item_cache_datetime(MYSQL_TYPE_DATETIME);
    /* Mark the cache as non-const to prevent re-caching. */
    cache->set_used_tables(1);
    cache->store(item, value);
    *cache_arg= cache;
    *item_arg= cache_arg;
  }
  return value;
}


/*
  Retrieves YEAR value of 19XX-00-00 00:00:00 form from given item.

  SYNOPSIS
    get_year_value()
    thd                 thread handle
    item_arg   [in/out] item to retrieve YEAR value from
    cache_arg  [in/out] pointer to place to store the caching item to
    warn_item  [in]     item for issuing the conversion warning
    is_null    [out]    TRUE <=> the item_arg is null

  DESCRIPTION
    Retrieves the YEAR value of 19XX form from given item for comparison by the
    compare_datetime() function.
    Converts year to DATETIME of form YYYY-00-00 00:00:00 for the compatibility
    with the get_datetime_value function result.

  RETURN
    obtained value
*/

static longlong
get_year_value(THD *thd, Item ***item_arg, Item **cache_arg,
               Item *warn_item, bool *is_null)
{
  longlong value= 0;
  Item *item= **item_arg;

  value= item->val_int();
  *is_null= item->null_value;
  if (*is_null)
    return ~(ulonglong) 0;

  /*
    Coerce value to the 19XX form in order to correctly compare
    YEAR(2) & YEAR(4) types.
    Here we are converting all item values but YEAR(4) fields since
      1) YEAR(4) already has a regular YYYY form and
      2) we don't want to convert zero/bad YEAR(4) values to the
         value of 2000.
  */
  Item *real_item= item->real_item();
  Field *field= NULL;
  if (real_item->type() == Item::FIELD_ITEM)
    field= ((Item_field *)real_item)->field;
  else if (real_item->type() == Item::CACHE_ITEM)
    field= ((Item_cache *)real_item)->field();
  if (!(field && field->type() == MYSQL_TYPE_YEAR && field->field_length == 4))
  {
    if (value < 70)
      value+= 100;
    if (value <= 1900)
      value+= 1900;
  }
  /* Convert year to DATETIME packed format */
  return year_to_longlong_datetime_packed(value);
}


/*
  Compare items values as dates.

  SYNOPSIS
    Arg_comparator::compare_datetime()

  DESCRIPTION
    Compare items values as DATE/DATETIME for both EQUAL_FUNC and from other
    comparison functions. The correct DATETIME values are obtained
    with help of the get_datetime_value() function.

  RETURN
    If is_nulls_eq is TRUE:
       1    if items are equal or both are null
       0    otherwise
    If is_nulls_eq is FALSE:
      -1   a < b or at least one item is null
       0   a == b
       1   a > b
    See the table:
    is_nulls_eq | 1 | 1 | 1 | 1 | 0 | 0 | 0 | 0 |
    a_is_null   | 1 | 0 | 1 | 0 | 1 | 0 | 1 | 0 |
    b_is_null   | 1 | 1 | 0 | 0 | 1 | 1 | 0 | 0 |
    result      | 1 | 0 | 0 |0/1|-1 |-1 |-1 |-1/0/1|
*/

int Arg_comparator::compare_datetime()
{
  bool a_is_null, b_is_null;
  longlong a_value, b_value;

  /* Get DATE/DATETIME/TIME value of the 'a' item. */
  a_value= (*get_value_a_func)(thd, &a, &a_cache, *b, &a_is_null);
  if (!is_nulls_eq && a_is_null)
  {
    if (set_null)
      owner->null_value= 1;
    return -1;
  }

  /* Get DATE/DATETIME/TIME value of the 'b' item. */
  b_value= (*get_value_b_func)(thd, &b, &b_cache, *a, &b_is_null);
  if (a_is_null || b_is_null)
  {
    if (set_null)
      owner->null_value= is_nulls_eq ? 0 : 1;
    return is_nulls_eq ? (a_is_null == b_is_null) : -1;
  }

  /* Here we have two not-NULL values. */
  if (set_null)
    owner->null_value= 0;

  /* Compare values. */
  if (is_nulls_eq)
    return (a_value == b_value);
  return a_value < b_value ? -1 : (a_value > b_value ? 1 : 0);
}


int Arg_comparator::compare_string()
{
  String *res1,*res2;
  if ((res1= (*a)->val_str(&value1)))
  {
    if ((res2= (*b)->val_str(&value2)))
    {
      if (set_null)
        owner->null_value= 0;
      return sortcmp(res1,res2,cmp_collation.collation);
    }
  }
  if (set_null)
    owner->null_value= 1;
  return -1;
}


/**
  Compare strings byte by byte. End spaces are also compared.

  @retval
    <0  *a < *b
  @retval
     0  *b == *b
  @retval
    >0  *a > *b
*/

int Arg_comparator::compare_binary_string()
{
  String *res1,*res2;
  if ((res1= (*a)->val_str(&value1)))
  {
    if ((res2= (*b)->val_str(&value2)))
    {
      if (set_null)
        owner->null_value= 0;
      uint res1_length= res1->length();
      uint res2_length= res2->length();
      int cmp= memcmp(res1->ptr(), res2->ptr(), min(res1_length,res2_length));
      return cmp ? cmp : (int) (res1_length - res2_length);
    }
  }
  if (set_null)
    owner->null_value= 1;
  return -1;
}


/**
  Compare strings, but take into account that NULL == NULL.
*/


int Arg_comparator::compare_e_string()
{
  String *res1,*res2;
  res1= (*a)->val_str(&value1);
  res2= (*b)->val_str(&value2);
  if (!res1 || !res2)
    return MY_TEST(res1 == res2);
  return MY_TEST(sortcmp(res1, res2, cmp_collation.collation) == 0);
}


int Arg_comparator::compare_e_binary_string()
{
  String *res1,*res2;
  res1= (*a)->val_str(&value1);
  res2= (*b)->val_str(&value2);
  if (!res1 || !res2)
    return MY_TEST(res1 == res2);
  return MY_TEST(stringcmp(res1, res2) == 0);
}


int Arg_comparator::compare_real()
{
  /*
    Fix yet another manifestation of Bug#2338. 'Volatile' will instruct
    gcc to flush double values out of 80-bit Intel FPU registers before
    performing the comparison.
  */
  volatile double val1, val2;
  val1= (*a)->val_real();
  if (!(*a)->null_value)
  {
    val2= (*b)->val_real();
    if (!(*b)->null_value)
    {
      if (set_null)
        owner->null_value= 0;
      if (val1 < val2)	return -1;
      if (val1 == val2) return 0;
      return 1;
    }
  }
  if (set_null)
    owner->null_value= 1;
  return -1;
}

int Arg_comparator::compare_decimal()
{
  my_decimal decimal1;
  my_decimal *val1= (*a)->val_decimal(&decimal1);
  if (!(*a)->null_value)
  {
    my_decimal decimal2;
    my_decimal *val2= (*b)->val_decimal(&decimal2);
    if (!(*b)->null_value)
    {
      if (set_null)
        owner->null_value= 0;
      return my_decimal_cmp(val1, val2);
    }
  }
  if (set_null)
    owner->null_value= 1;
  return -1;
}

int Arg_comparator::compare_e_real()
{
  double val1= (*a)->val_real();
  double val2= (*b)->val_real();
  if ((*a)->null_value || (*b)->null_value)
    return MY_TEST((*a)->null_value && (*b)->null_value);
  return MY_TEST(val1 == val2);
}

int Arg_comparator::compare_e_decimal()
{
  my_decimal decimal1, decimal2;
  my_decimal *val1= (*a)->val_decimal(&decimal1);
  my_decimal *val2= (*b)->val_decimal(&decimal2);
  if ((*a)->null_value || (*b)->null_value)
    return MY_TEST((*a)->null_value && (*b)->null_value);
  return MY_TEST(my_decimal_cmp(val1, val2) == 0);
}


int Arg_comparator::compare_real_fixed()
{
  /*
    Fix yet another manifestation of Bug#2338. 'Volatile' will instruct
    gcc to flush double values out of 80-bit Intel FPU registers before
    performing the comparison.
  */
  volatile double val1, val2;
  val1= (*a)->val_real();
  if (!(*a)->null_value)
  {
    val2= (*b)->val_real();
    if (!(*b)->null_value)
    {
      if (set_null)
        owner->null_value= 0;
      if (val1 == val2 || fabs(val1 - val2) < precision)
        return 0;
      if (val1 < val2)
        return -1;
      return 1;
    }
  }
  if (set_null)
    owner->null_value= 1;
  return -1;
}


int Arg_comparator::compare_e_real_fixed()
{
  double val1= (*a)->val_real();
  double val2= (*b)->val_real();
  if ((*a)->null_value || (*b)->null_value)
    return MY_TEST((*a)->null_value && (*b)->null_value);
  return MY_TEST(val1 == val2 || fabs(val1 - val2) < precision);
}


int Arg_comparator::compare_int_signed()
{
  longlong val1= (*a)->val_int();
  if (!(*a)->null_value)
  {
    longlong val2= (*b)->val_int();
    if (!(*b)->null_value)
    {
      if (set_null)
        owner->null_value= 0;
      if (val1 < val2)	return -1;
      if (val1 == val2)   return 0;
      return 1;
    }
  }
  if (set_null)
    owner->null_value= 1;
  return -1;
}


/**
  Compare arguments using numeric packed temporal representation.
*/
int Arg_comparator::compare_time_packed()
{
  /*
    Note, we cannot do this:
    DBUG_ASSERT((*a)->field_type() == MYSQL_TYPE_TIME);
    DBUG_ASSERT((*b)->field_type() == MYSQL_TYPE_TIME);
    
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
  longlong val1= (*a)->val_time_temporal();
  if (!(*a)->null_value)
  {
    longlong val2= (*b)->val_time_temporal();
    if (!(*b)->null_value)
    {
      if (set_null)
        owner->null_value= 0;
      return val1 < val2 ? -1 : val1 > val2 ? 1 : 0;
    }
  }
  if (set_null)
    owner->null_value= 1;
  return -1;
}


/**
  Compare arguments using numeric packed representation for '<=>'.
*/
int Arg_comparator::compare_e_time_packed()
{
  longlong val1= (*a)->val_time_temporal();
  longlong val2= (*b)->val_time_temporal();
  if ((*a)->null_value || (*b)->null_value)
    return MY_TEST((*a)->null_value && (*b)->null_value);
  return MY_TEST(val1 == val2);
}



/**
  Compare values as BIGINT UNSIGNED.
*/

int Arg_comparator::compare_int_unsigned()
{
  ulonglong val1= (*a)->val_int();
  if (!(*a)->null_value)
  {
    ulonglong val2= (*b)->val_int();
    if (!(*b)->null_value)
    {
      if (set_null)
        owner->null_value= 0;
      if (val1 < val2)	return -1;
      if (val1 == val2)   return 0;
      return 1;
    }
  }
  if (set_null)
    owner->null_value= 1;
  return -1;
}


/**
  Compare signed (*a) with unsigned (*B)
*/

int Arg_comparator::compare_int_signed_unsigned()
{
  longlong sval1= (*a)->val_int();
  if (!(*a)->null_value)
  {
    ulonglong uval2= (ulonglong)(*b)->val_int();
    if (!(*b)->null_value)
    {
      if (set_null)
        owner->null_value= 0;
      if (sval1 < 0 || (ulonglong)sval1 < uval2)
        return -1;
      if ((ulonglong)sval1 == uval2)
        return 0;
      return 1;
    }
  }
  if (set_null)
    owner->null_value= 1;
  return -1;
}


/**
  Compare unsigned (*a) with signed (*B)
*/

int Arg_comparator::compare_int_unsigned_signed()
{
  ulonglong uval1= (ulonglong)(*a)->val_int();
  if (!(*a)->null_value)
  {
    longlong sval2= (*b)->val_int();
    if (!(*b)->null_value)
    {
      if (set_null)
        owner->null_value= 0;
      if (sval2 < 0)
        return 1;
      if (uval1 < (ulonglong)sval2)
        return -1;
      if (uval1 == (ulonglong)sval2)
        return 0;
      return 1;
    }
  }
  if (set_null)
    owner->null_value= 1;
  return -1;
}


int Arg_comparator::compare_e_int()
{
  longlong val1= (*a)->val_int();
  longlong val2= (*b)->val_int();
  if ((*a)->null_value || (*b)->null_value)
    return MY_TEST((*a)->null_value && (*b)->null_value);
  return MY_TEST(val1 == val2);
}

/**
  Compare unsigned *a with signed *b or signed *a with unsigned *b.
*/
int Arg_comparator::compare_e_int_diff_signedness()
{
  longlong val1= (*a)->val_int();
  longlong val2= (*b)->val_int();
  if ((*a)->null_value || (*b)->null_value)
    return MY_TEST((*a)->null_value && (*b)->null_value);
  return (val1 >= 0) && MY_TEST(val1 == val2);
}

int Arg_comparator::compare_row()
{
  int res= 0;
  bool was_null= 0;
  (*a)->bring_value();
  (*b)->bring_value();

  if ((*a)->null_value || (*b)->null_value)
  {
    owner->null_value= 1;
    return -1;
  }

  uint n= (*a)->cols();
  for (uint i= 0; i<n; i++)
  {
    res= comparators[i].compare();
    /* Aggregate functions don't need special null handling. */
    if (owner->null_value && owner->type() == Item::FUNC_ITEM)
    {
      // NULL was compared
      switch (((Item_func*)owner)->functype()) {
      case Item_func::NE_FUNC:
        break; // NE never aborts on NULL even if abort_on_null is set
      case Item_func::LT_FUNC:
      case Item_func::LE_FUNC:
      case Item_func::GT_FUNC:
      case Item_func::GE_FUNC:
        return -1; // <, <=, > and >= always fail on NULL
      default: // EQ_FUNC
        if (((Item_bool_func2*)owner)->abort_on_null)
          return -1; // We do not need correct NULL returning
      }
      was_null= 1;
      owner->null_value= 0;
      res= 0;  // continue comparison (maybe we will meet explicit difference)
    }
    else if (res)
      return res;
  }
  if (was_null)
  {
    /*
      There was NULL(s) in comparison in some parts, but there was no
      explicit difference in other parts, so we have to return NULL.
    */
    owner->null_value= 1;
    return -1;
  }
  return 0;
}


int Arg_comparator::compare_e_row()
{
  (*a)->bring_value();
  (*b)->bring_value();
  uint n= (*a)->cols();
  for (uint i= 0; i<n; i++)
  {
    if (!comparators[i].compare())
      return 0;
  }
  return 1;
}


void Item_func_truth::fix_length_and_dec()
{
  maybe_null= 0;
  null_value= 0;
  decimals= 0;
  max_length= 1;
}


void Item_func_truth::print(String *str, enum_query_type query_type)
{
  str->append('(');
  args[0]->print(str, query_type);
  str->append(STRING_WITH_LEN(" is "));
  if (! affirmative)
    str->append(STRING_WITH_LEN("not "));
  if (value)
    str->append(STRING_WITH_LEN("true"));
  else
    str->append(STRING_WITH_LEN("false"));
  str->append(')');
}


bool Item_func_truth::val_bool()
{
  bool val= args[0]->val_bool();
  if (args[0]->null_value)
  {
    /*
      NULL val IS {TRUE, FALSE} --> FALSE
      NULL val IS NOT {TRUE, FALSE} --> TRUE
    */
    return (! affirmative);
  }

  if (affirmative)
  {
    /* {TRUE, FALSE} val IS {TRUE, FALSE} value */
    return (val == value);
  }

  /* {TRUE, FALSE} val IS NOT {TRUE, FALSE} value */
  return (val != value);
}


longlong Item_func_truth::val_int()
{
  return (val_bool() ? 1 : 0);
}


bool Item_in_optimizer::fix_left(THD *thd, Item **ref)
{
  /*
    Refresh this pointer as left_expr may have been substituted
    during resolving.
  */
  args[0]= ((Item_in_subselect *)args[1])->left_expr;

  if ((!args[0]->fixed && args[0]->fix_fields(thd, args)) ||
      (!cache && !(cache= Item_cache::get_cache(args[0]))))
    return 1;

  cache->setup(args[0]);
  used_tables_cache= args[0]->used_tables();
  if (cache->cols() == 1)
  {
    cache->set_used_tables(used_tables_cache);
  }
  else
  {
    uint n= cache->cols();
    for (uint i= 0; i < n; i++)
    {
      ((Item_cache *)cache->element_index(i))->
        set_used_tables(args[0]->element_index(i)->used_tables());
    }
  }
  not_null_tables_cache= args[0]->not_null_tables();
  with_sum_func= args[0]->with_sum_func;
  if ((const_item_cache= args[0]->const_item()))
    cache->store(args[0]);
  return 0;
}


bool Item_in_optimizer::fix_fields(THD *thd, Item **ref)
{
  DBUG_ASSERT(fixed == 0);
  if (fix_left(thd, ref))
    return TRUE;
  if (args[0]->maybe_null)
    maybe_null=1;

  if (!args[1]->fixed && args[1]->fix_fields(thd, args+1))
    return TRUE;
  Item_in_subselect * sub= (Item_in_subselect *)args[1];
  if (args[0]->cols() != sub->engine->cols())
  {
    my_error(ER_OPERAND_COLUMNS, MYF(0), args[0]->cols());
    return TRUE;
  }
  if (args[1]->maybe_null)
    maybe_null=1;
  with_sum_func= with_sum_func || args[1]->with_sum_func;
  used_tables_cache|= args[1]->used_tables();
  not_null_tables_cache|= args[1]->not_null_tables();

  if (!sub->is_top_level_item())
  {
    /*
      This is a NOT IN subquery predicate (or equivalent). Null values passed
      from outer tables and used in the left-hand expression of the predicate
      must be considered in the evaluation, hence filter out these tables
      from the set of null-rejecting tables.
    */
    not_null_tables_cache&= ~args[0]->not_null_tables();
  }
  const_item_cache&= args[1]->const_item();
  fixed= 1;
  return FALSE;
}


void Item_in_optimizer::fix_after_pullout(st_select_lex *parent_select,
                                          st_select_lex *removed_select)
{
  used_tables_cache= get_initial_pseudo_tables();
  not_null_tables_cache= 0;
  const_item_cache= 1;

  /*
    No need to call fix_after_pullout() on args[0] (ie left expression),
    as Item_in_subselect::fix_after_pullout() will do this.
    So, just forward the call to the Item_in_subselect object.
  */

  args[1]->fix_after_pullout(parent_select, removed_select);

  used_tables_cache|= args[1]->used_tables();
  not_null_tables_cache|= args[1]->not_null_tables();
  const_item_cache&= args[1]->const_item();
}


/**
   The implementation of optimized \<outer expression\> [NOT] IN \<subquery\>
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
     \<in predicate\>
     ( SELECT ic_1, ..., ic_n
       FROM \<table\>
       WHERE \<inner where\> 
     )
     </tt>

     was transformed into
     
     <tt>
     ( oc_1, ..., oc_n ) 
     \<in predicate\>
     ( SELECT ic_1, ..., ic_n 
       FROM \<table\> 
       WHERE \<inner where\> AND ... ( ic_k = oc_k OR ic_k IS NULL ) 
       HAVING ... NOT ic_k IS NULL
     )
     </tt>

     The evaluation will now proceed according to special rules set up
     elsewhere. These rules include:

     - The HAVING NOT \<inner column\> IS NULL conditions added by the
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

     @see Item_in_subselect::val_bool()
     @see Item_is_not_null_test::val_int()
 */

longlong Item_in_optimizer::val_int()
{
  bool tmp;
  DBUG_ASSERT(fixed == 1);
  cache->store(args[0]);
  cache->cache_value();
  
  if (cache->null_value)
  {
    Item_in_subselect * const item_subs=
      static_cast<Item_in_subselect *>(args[1]);
    /*
      We're evaluating 
      "<outer_value_list> [NOT] IN (SELECT <inner_value_list>...)" 
      where one or more of the outer values is NULL. 
    */
    if (item_subs->is_top_level_item())
    {
      /*
        We're evaluating a top level item, e.g. 
	"<outer_value_list> IN (SELECT <inner_value_list>...)",
	and in this case a NULL value in the outer_value_list means
        that the result shall be NULL/FALSE (makes no difference for
        top level items). The cached value is NULL, so just return
        NULL.
      */
      null_value= 1;
    }
    else
    {
      /*
	We're evaluating an item where a NULL value in either the
        outer or inner value list does not automatically mean that we
        can return NULL/FALSE. An example of such a query is
        "<outer_value_list> NOT IN (SELECT <inner_value_list>...)" 
        The result when there is at least one NULL value is: NULL if the
        SELECT evaluated over the non-NULL values produces at least
        one row, FALSE otherwise
      */
      bool all_left_cols_null= true;
      const uint ncols= cache->cols();

      /*
        Turn off the predicates that are based on column compares for
        which the left part is currently NULL
      */
      for (uint i= 0; i < ncols; i++)
      {
        if (cache->element_index(i)->null_value)
          item_subs->set_cond_guard_var(i, FALSE);
        else 
          all_left_cols_null= false;
      }

      if (all_left_cols_null && result_for_null_param != UNKNOWN &&
          !item_subs->originally_dependent())
      {
        /*
           This subquery was originally not correlated. The IN->EXISTS
           transformation may have made it correlated, but only to the left
           expression. All values in the left expression are NULL, and we have
           already evaluated the subquery for all NULL values: return the same
           result we did last time without evaluating the subquery.
        */
        null_value= result_for_null_param;
      } 
      else 
      {
        /* The subquery has to be evaluated */
        (void) item_subs->val_bool_result();
        if (!item_subs->value)
          null_value= item_subs->null_value;
        else
          null_value= TRUE;
        if (all_left_cols_null)
          result_for_null_param= null_value;
      }

      /* Turn all predicates back on */
      for (uint i= 0; i < ncols; i++)
        item_subs->set_cond_guard_var(i, TRUE);
    }
    return 0;
  }
  tmp= args[1]->val_bool_result();
  null_value= args[1]->null_value;
  return tmp;
}


void Item_in_optimizer::keep_top_level_cache()
{
  cache->keep_array();
  save_cache= 1;
}


void Item_in_optimizer::cleanup()
{
  DBUG_ENTER("Item_in_optimizer::cleanup");
  Item_bool_func::cleanup();
  if (!save_cache)
    cache= 0;
  DBUG_VOID_RETURN;
}


bool Item_in_optimizer::is_null()
{
  val_int();
  return null_value;
}


/**
  Transform an Item_in_optimizer and its arguments with a callback function.

  @param transformer the transformer callback function to be applied to the
         nodes of the tree of the object
  @param parameter to be passed to the transformer

  @detail
    Recursively transform the left and the right operand of this Item. The
    Right operand is an Item_in_subselect or its subclass. To avoid the
    creation of new Items, we use the fact the the left operand of the
    Item_in_subselect is the same as the one of 'this', so instead of
    transforming its operand, we just assign the left operand of the
    Item_in_subselect to be equal to the left operand of 'this'.
    The transformation is not applied further to the subquery operand
    if the IN predicate.

  @returns
    @retval pointer to the transformed item
    @retval NULL if an error occurred
*/

Item *Item_in_optimizer::transform(Item_transformer transformer, uchar *argument)
{
  Item *new_item;

  DBUG_ASSERT(!current_thd->stmt_arena->is_stmt_prepare());
  DBUG_ASSERT(arg_count == 2);

  /* Transform the left IN operand. */
  new_item= args[0]->transform(transformer, argument);
  if (!new_item)
    return 0;
  /*
    THD::change_item_tree() should be called only if the tree was
    really transformed, i.e. when a new item has been created.
    Otherwise we'll be allocating a lot of unnecessary memory for
    change records at each execution.
  */
  if (args[0] != new_item)
    current_thd->change_item_tree(args, new_item);

  /*
    Transform the right IN operand which should be an Item_in_subselect or a
    subclass of it. The left operand of the IN must be the same as the left
    operand of this Item_in_optimizer, so in this case there is no further
    transformation, we only make both operands the same.
    TODO: is it the way it should be?
  */
  DBUG_ASSERT((args[1])->type() == Item::SUBSELECT_ITEM &&
              (((Item_subselect*)(args[1]))->substype() ==
               Item_subselect::IN_SUBS ||
               ((Item_subselect*)(args[1]))->substype() ==
               Item_subselect::ALL_SUBS ||
               ((Item_subselect*)(args[1]))->substype() ==
               Item_subselect::ANY_SUBS));

  Item_in_subselect *in_arg= (Item_in_subselect*)args[1];

  if (in_arg->left_expr != args[0])
    current_thd->change_item_tree(&in_arg->left_expr, args[0]);

  return (this->*transformer)(argument);
}


longlong Item_func_eq::val_int()
{
  DBUG_ASSERT(fixed == 1);
  int value= cmp.compare();
  return value == 0 ? 1 : 0;
}


/** Same as Item_func_eq, but NULL = NULL. */

void Item_func_equal::fix_length_and_dec()
{
  Item_bool_func2::fix_length_and_dec();
  maybe_null=null_value=0;
}

longlong Item_func_equal::val_int()
{
  DBUG_ASSERT(fixed == 1);
  return cmp.compare();
}

longlong Item_func_ne::val_int()
{
  DBUG_ASSERT(fixed == 1);
  int value= cmp.compare();
  return value != 0 && !null_value ? 1 : 0;
}


longlong Item_func_ge::val_int()
{
  DBUG_ASSERT(fixed == 1);
  int value= cmp.compare();
  return value >= 0 ? 1 : 0;
}


longlong Item_func_gt::val_int()
{
  DBUG_ASSERT(fixed == 1);
  int value= cmp.compare();
  return value > 0 ? 1 : 0;
}

longlong Item_func_le::val_int()
{
  DBUG_ASSERT(fixed == 1);
  int value= cmp.compare();
  return value <= 0 && !null_value ? 1 : 0;
}


longlong Item_func_lt::val_int()
{
  DBUG_ASSERT(fixed == 1);
  int value= cmp.compare();
  return value < 0 && !null_value ? 1 : 0;
}


longlong Item_func_strcmp::val_int()
{
  DBUG_ASSERT(fixed == 1);
  String *a=args[0]->val_str(&cmp.value1);
  String *b=args[1]->val_str(&cmp.value2);
  if (!a || !b)
  {
    null_value=1;
    return 0;
  }
  int value= sortcmp(a,b,cmp.cmp_collation.collation);
  null_value=0;
  return !value ? 0 : (value < 0 ? (longlong) -1 : (longlong) 1);
}


bool Item_func_opt_neg::eq(const Item *item, bool binary_cmp) const
{
  /* Assume we don't have rtti */
  if (this == item)
    return 1;
  if (item->type() != FUNC_ITEM)
    return 0;
  Item_func *item_func=(Item_func*) item;
  if (arg_count != item_func->arg_count ||
      functype() != item_func->functype())
    return 0;
  if (negated != ((Item_func_opt_neg *) item_func)->negated)
    return 0;
  for (uint i=0; i < arg_count ; i++)
    if (!args[i]->eq(item_func->arguments()[i], binary_cmp))
      return 0;
  return 1;
}


void Item_func_interval::fix_length_and_dec()
{
  uint rows= row->cols();
  
  use_decimal_comparison= ((row->element_index(0)->result_type() ==
                            DECIMAL_RESULT) ||
                           (row->element_index(0)->result_type() ==
                            INT_RESULT));
  if (rows > 8)
  {
    bool not_null_consts= TRUE;

    for (uint i= 1; not_null_consts && i < rows; i++)
    {
      Item *el= row->element_index(i);
      not_null_consts&= el->const_item() & !el->is_null();
    }

    if (not_null_consts &&
        (intervals=
          (interval_range*) sql_alloc(sizeof(interval_range) * (rows - 1))))
    {
      if (use_decimal_comparison)
      {
        for (uint i= 1; i < rows; i++)
        {
          Item *el= row->element_index(i);
          interval_range *range= intervals + (i-1);
          if ((el->result_type() == DECIMAL_RESULT) ||
              (el->result_type() == INT_RESULT))
          {
            range->type= DECIMAL_RESULT;
            range->dec.init();
            my_decimal *dec= el->val_decimal(&range->dec);
            if (dec != &range->dec)
            {
              range->dec= *dec;
            }
          }
          else
          {
            range->type= REAL_RESULT;
            range->dbl= el->val_real();
          }
        }
      }
      else
      {
        for (uint i= 1; i < rows; i++)
        {
          intervals[i-1].dbl= row->element_index(i)->val_real();
        }
      }
    }
  }
  maybe_null= 0;
  max_length= 2;
  used_tables_cache|= row->used_tables();
  not_null_tables_cache= row->not_null_tables();
  with_sum_func= with_sum_func || row->with_sum_func;
  const_item_cache&= row->const_item();
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

longlong Item_func_interval::val_int()
{
  DBUG_ASSERT(fixed == 1);
  double value;
  my_decimal dec_buf, *dec= NULL;
  uint i;

  if (use_decimal_comparison)
  {
    dec= row->element_index(0)->val_decimal(&dec_buf);
    if (row->element_index(0)->null_value)
      return -1;
    my_decimal2double(E_DEC_FATAL_ERROR, dec, &value);
  }
  else
  {
    value= row->element_index(0)->val_real();
    if (row->element_index(0)->null_value)
      return -1;
  }

  if (intervals)
  {					// Use binary search to find interval
    uint start,end;
    start= 0;
    end=   row->cols()-2;
    while (start != end)
    {
      uint mid= (start + end + 1) / 2;
      interval_range *range= intervals + mid;
      my_bool cmp_result;
      /*
        The values in the range intervall may have different types,
        Only do a decimal comparision of the first argument is a decimal
        and we are comparing against a decimal
      */
      if (dec && range->type == DECIMAL_RESULT)
        cmp_result= my_decimal_cmp(&range->dec, dec) <= 0;
      else
        cmp_result= (range->dbl <= value);
      if (cmp_result)
	start= mid;
      else
	end= mid - 1;
    }
    interval_range *range= intervals+start;
    return ((dec && range->type == DECIMAL_RESULT) ?
            my_decimal_cmp(dec, &range->dec) < 0 :
            value < range->dbl) ? 0 : start + 1;
  }

  for (i=1 ; i < row->cols() ; i++)
  {
    Item *el= row->element_index(i);
    if (use_decimal_comparison &&
        ((el->result_type() == DECIMAL_RESULT) ||
         (el->result_type() == INT_RESULT)))
    {
      my_decimal e_dec_buf, *e_dec= el->val_decimal(&e_dec_buf);
      /* Skip NULL ranges. */
      if (el->null_value)
        continue;
      if (my_decimal_cmp(e_dec, dec) > 0)
        return i - 1;
    }
    else 
    {
      double val= el->val_real();
      /* Skip NULL ranges. */
      if (el->null_value)
        continue;
      if (val > value)
        return i - 1;
    }
  }
  return i-1;
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

bool Item_func_between::fix_fields(THD *thd, Item **ref)
{
  if (Item_func_opt_neg::fix_fields(thd, ref))
    return 1;

  thd->lex->current_select->between_count++;

  // not_null_tables_cache == union(T1(e),T1(e1),T1(e2))
  if (pred_level && !negated)
    return 0;

  // not_null_tables_cache == union(T1(e), intersection(T1(e1),T1(e2)))
  not_null_tables_cache= (args[0]->not_null_tables() |
                          (args[1]->not_null_tables() &
                           args[2]->not_null_tables()));

  return 0;
}


void Item_func_between::fix_after_pullout(st_select_lex *parent_select,
                                          st_select_lex *removed_select)
{
  Item_func_opt_neg::fix_after_pullout(parent_select, removed_select);

  // not_null_tables_cache == union(T1(e),T1(e1),T1(e2))
  if (pred_level && !negated)
    return;

  // not_null_tables_cache == union(T1(e), intersection(T1(e1),T1(e2)))
  not_null_tables_cache= args[0]->not_null_tables() |
                         (args[1]->not_null_tables() &
                          args[2]->not_null_tables());
}


void Item_func_between::fix_length_and_dec()
{
  max_length= 1;
  int i;
  int datetime_items_found= 0;
  int time_items_found= 0;
  compare_as_dates_with_strings= false;
  compare_as_temporal_times= compare_as_temporal_dates= false;
  THD *thd= current_thd;

  /*
    As some compare functions are generated after sql_yacc,
    we have to check for out of memory conditions here
  */
  if (!args[0] || !args[1] || !args[2])
    return;
  if ( agg_cmp_type(&cmp_type, args, 3))
    return;
  if (cmp_type == STRING_RESULT &&
      agg_arg_charsets_for_comparison(cmp_collation, args, 3))
   return;

  /*
    Detect the comparison of DATE/DATETIME items.
    At least one of items should be a DATE/DATETIME item and other items
    should return the STRING result.
  */
  if (cmp_type == STRING_RESULT)
  {
    for (i= 0; i < 3; i++)
    {
      if (args[i]->is_temporal_with_date())
        datetime_items_found++;
      else
      if (args[i]->field_type() == MYSQL_TYPE_TIME)
        time_items_found++;
    }
  }

  if (datetime_items_found + time_items_found == 3)
  {
    if (time_items_found == 3)
    {
      // All items are TIME
      cmp_type= INT_RESULT;
      compare_as_temporal_times= true;
    }
    else
    {
      /*
        There is at least one DATE or DATETIME item,
        all other items are DATE, DATETIME or TIME.
      */
      cmp_type= INT_RESULT;
      compare_as_temporal_dates= true;
    }
  }
  else if (datetime_items_found > 0)
  {
    /*
       There is at least one DATE or DATETIME item.
       All other items are DATE, DATETIME or strings.
    */
    compare_as_dates_with_strings= true;
    ge_cmp.set_datetime_cmp_func(this, args, args + 1);
    le_cmp.set_datetime_cmp_func(this, args, args + 2);
  }
  else if (args[0]->real_item()->type() == FIELD_ITEM &&
           thd->lex->sql_command != SQLCOM_CREATE_VIEW &&
           thd->lex->sql_command != SQLCOM_SHOW_CREATE)
  {
    Item_field *field_item= (Item_field*) (args[0]->real_item());
    if (field_item->field->can_be_compared_as_longlong())
    {
      /*
        The following can't be recoded with || as convert_constant_item
        changes the argument
      */
      const bool cvt_arg1= convert_constant_item(thd, field_item, &args[1]);
      const bool cvt_arg2= convert_constant_item(thd, field_item, &args[2]);
      if (args[0]->is_temporal())
      { // special handling of date/time etc.
        if (cvt_arg1 || cvt_arg2)
          cmp_type=INT_RESULT;
      }
      else
      {
        if (cvt_arg1 && cvt_arg2)
          cmp_type=INT_RESULT;
      }

      if (args[0]->is_temporal() &&
          args[1]->is_temporal() &&
          args[2]->is_temporal())
      {
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
        if (field_item->field_type() == MYSQL_TYPE_TIME)
          compare_as_temporal_times= true;
        else if (field_item->is_temporal_with_date())
          compare_as_temporal_dates= true;
      }
    }
  }
}


longlong Item_func_between::val_int()
{						// ANSI BETWEEN
  DBUG_ASSERT(fixed == 1);
  if (compare_as_dates_with_strings)
  {
    int ge_res, le_res;

    ge_res= ge_cmp.compare();
    if ((null_value= args[0]->null_value))
      return 0;
    le_res= le_cmp.compare();

    if (!args[1]->null_value && !args[2]->null_value)
      return (longlong) ((ge_res >= 0 && le_res <=0) != negated);
    else if (args[1]->null_value)
    {
      null_value= le_res > 0;			// not null if false range.
    }
    else
    {
      null_value= ge_res < 0;
    }
  }
  else if (cmp_type == STRING_RESULT)
  {
    String *value,*a,*b;
    value=args[0]->val_str(&value0);
    if ((null_value=args[0]->null_value))
      return 0;
    a=args[1]->val_str(&value1);
    b=args[2]->val_str(&value2);
    if (!args[1]->null_value && !args[2]->null_value)
      return (longlong) ((sortcmp(value,a,cmp_collation.collation) >= 0 &&
                          sortcmp(value,b,cmp_collation.collation) <= 0) !=
                         negated);
    if (args[1]->null_value && args[2]->null_value)
      null_value=1;
    else if (args[1]->null_value)
    {
      // Set to not null if false range.
      null_value= sortcmp(value,b,cmp_collation.collation) <= 0;
    }
    else
    {
      // Set to not null if false range.
      null_value= sortcmp(value,a,cmp_collation.collation) >= 0;
    }
  }
  else if (cmp_type == INT_RESULT)
  {
    longlong a, b, value;
    value= compare_as_temporal_times ? args[0]->val_time_temporal() :
           compare_as_temporal_dates ? args[0]->val_date_temporal() :
           args[0]->val_int();
    if ((null_value=args[0]->null_value))
      return 0;					/* purecov: inspected */
    if (compare_as_temporal_times)
    {
      a= args[1]->val_time_temporal();
      b= args[2]->val_time_temporal();
    }
    else if (compare_as_temporal_dates)
    {
      a= args[1]->val_date_temporal();
      b= args[2]->val_date_temporal();
    }
    else
    {
      a= args[1]->val_int();
      b= args[2]->val_int();
    }
    if (!args[1]->null_value && !args[2]->null_value)
      return (longlong) ((value >= a && value <= b) != negated);
    if (args[1]->null_value && args[2]->null_value)
      null_value=1;
    else if (args[1]->null_value)
    {
      null_value= value <= b;			// not null if false range.
    }
    else
    {
      null_value= value >= a;
    }
  }
  else if (cmp_type == DECIMAL_RESULT)
  {
    my_decimal dec_buf, *dec= args[0]->val_decimal(&dec_buf),
               a_buf, *a_dec, b_buf, *b_dec;
    if ((null_value=args[0]->null_value))
      return 0;					/* purecov: inspected */
    a_dec= args[1]->val_decimal(&a_buf);
    b_dec= args[2]->val_decimal(&b_buf);
    if (!args[1]->null_value && !args[2]->null_value)
      return (longlong) ((my_decimal_cmp(dec, a_dec) >= 0 &&
                          my_decimal_cmp(dec, b_dec) <= 0) != negated);
    if (args[1]->null_value && args[2]->null_value)
      null_value=1;
    else if (args[1]->null_value)
      null_value= (my_decimal_cmp(dec, b_dec) <= 0);
    else
      null_value= (my_decimal_cmp(dec, a_dec) >= 0);
  }
  else
  {
    double value= args[0]->val_real(),a,b;
    if ((null_value=args[0]->null_value))
      return 0;					/* purecov: inspected */
    a= args[1]->val_real();
    b= args[2]->val_real();
    if (!args[1]->null_value && !args[2]->null_value)
      return (longlong) ((value >= a && value <= b) != negated);
    if (args[1]->null_value && args[2]->null_value)
      null_value=1;
    else if (args[1]->null_value)
    {
      null_value= value <= b;			// not null if false range.
    }
    else
    {
      null_value= value >= a;
    }
  }
  return (longlong) (!null_value && negated);
}


void Item_func_between::print(String *str, enum_query_type query_type)
{
  str->append('(');
  args[0]->print(str, query_type);
  if (negated)
    str->append(STRING_WITH_LEN(" not"));
  str->append(STRING_WITH_LEN(" between "));
  args[1]->print(str, query_type);
  str->append(STRING_WITH_LEN(" and "));
  args[2]->print(str, query_type);
  str->append(')');
}

void
Item_func_ifnull::fix_length_and_dec()
{
  uint32 char_length;
  agg_result_type(&hybrid_type, args, 2);
  cached_field_type= agg_field_type(args, 2);
  maybe_null=args[1]->maybe_null;
  decimals= max(args[0]->decimals, args[1]->decimals);
  unsigned_flag= args[0]->unsigned_flag && args[1]->unsigned_flag;

  if (hybrid_type == DECIMAL_RESULT || hybrid_type == INT_RESULT) 
  {
    int len0= args[0]->max_char_length() - args[0]->decimals
      - (args[0]->unsigned_flag ? 0 : 1);

    int len1= args[1]->max_char_length() - args[1]->decimals
      - (args[1]->unsigned_flag ? 0 : 1);

    char_length= max(len0, len1) + decimals + (unsigned_flag ? 0 : 1);
  }
  else
    char_length= max(args[0]->max_char_length(), args[1]->max_char_length());

  switch (hybrid_type) {
  case STRING_RESULT:
    if (count_string_result_length(cached_field_type, args, arg_count))
      return;
    break;
  case DECIMAL_RESULT:
  case REAL_RESULT:
    break;
  case INT_RESULT:
    decimals= 0;
    break;
  case ROW_RESULT:
  default:
    DBUG_ASSERT(0);
  }
  fix_char_length(char_length);
}


uint Item_func_ifnull::decimal_precision() const
{
  int arg0_int_part= args[0]->decimal_int_part();
  int arg1_int_part= args[1]->decimal_int_part();
  int max_int_part= max(arg0_int_part, arg1_int_part);
  int precision= max_int_part + decimals;
  return min<uint>(precision, DECIMAL_MAX_PRECISION);
}


Field *Item_func_ifnull::tmp_table_field(TABLE *table)
{
  return tmp_table_field_from_field_type(table, 0);
}

double
Item_func_ifnull::real_op()
{
  DBUG_ASSERT(fixed == 1);
  double value= args[0]->val_real();
  if (!args[0]->null_value)
  {
    null_value=0;
    return value;
  }
  value= args[1]->val_real();
  if ((null_value=args[1]->null_value))
    return 0.0;
  return value;
}

longlong
Item_func_ifnull::int_op()
{
  DBUG_ASSERT(fixed == 1);
  longlong value=args[0]->val_int();
  if (!args[0]->null_value)
  {
    null_value=0;
    return value;
  }
  value=args[1]->val_int();
  if ((null_value=args[1]->null_value))
    return 0;
  return value;
}


my_decimal *Item_func_ifnull::decimal_op(my_decimal *decimal_value)
{
  DBUG_ASSERT(fixed == 1);
  my_decimal *value= args[0]->val_decimal(decimal_value);
  if (!args[0]->null_value)
  {
    null_value= 0;
    return value;
  }
  value= args[1]->val_decimal(decimal_value);
  if ((null_value= args[1]->null_value))
    return 0;
  return value;
}


bool Item_func_ifnull::date_op(MYSQL_TIME *ltime, uint fuzzydate)
{
  DBUG_ASSERT(fixed == 1);
  if (!args[0]->get_date(ltime, fuzzydate))
    return (null_value= false);
  return (null_value= args[1]->get_date(ltime, fuzzydate));
}


bool Item_func_ifnull::time_op(MYSQL_TIME *ltime)
{
  DBUG_ASSERT(fixed == 1);
  if (!args[0]->get_time(ltime))
    return (null_value= false);
  return (null_value= args[1]->get_time(ltime));
}


String *
Item_func_ifnull::str_op(String *str)
{
  DBUG_ASSERT(fixed == 1);
  String *res  =args[0]->val_str(str);
  if (!args[0]->null_value)
  {
    null_value=0;
    res->set_charset(collation.collation);
    return res;
  }
  res=args[1]->val_str(str);
  if ((null_value=args[1]->null_value))
    return 0;
  res->set_charset(collation.collation);
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

bool
Item_func_if::fix_fields(THD *thd, Item **ref)
{
  DBUG_ASSERT(fixed == 0);
  args[0]->top_level_item();

  if (Item_func::fix_fields(thd, ref))
    return 1;

  not_null_tables_cache= (args[1]->not_null_tables() &
                          args[2]->not_null_tables());

  return 0;
}


void Item_func_if::fix_after_pullout(st_select_lex *parent_select,
                                     st_select_lex *removed_select)
{
  Item_func::fix_after_pullout(parent_select, removed_select);

  not_null_tables_cache= (args[1]->not_null_tables() &
                          args[2]->not_null_tables());
}

void Item_func_if::cache_type_info(Item *source)
{
  collation.set(source->collation);
  cached_field_type=  source->field_type();
  cached_result_type= source->result_type();
  decimals=           source->decimals;
  max_length=         source->max_length;
  maybe_null=         source->maybe_null;
  unsigned_flag=      source->unsigned_flag;
}


void
Item_func_if::fix_length_and_dec()
{
  // Let IF(cond, expr, NULL) and IF(cond, NULL, expr) inherit type from expr.
  if (args[1]->type() == NULL_ITEM)
  {
    cache_type_info(args[2]);
    maybe_null= true;
    // If both arguments are NULL, make resulting type BINARY(0).
    if (args[2]->type() == NULL_ITEM)
      cached_field_type= MYSQL_TYPE_STRING;
    return;
  }
  if (args[2]->type() == NULL_ITEM)
  {
    cache_type_info(args[1]);
    maybe_null= true;
    return;
  }

  agg_result_type(&cached_result_type, args + 1, 2);
  cached_field_type= agg_field_type(args + 1, 2);
  maybe_null= args[1]->maybe_null || args[2]->maybe_null;
  decimals= max(args[1]->decimals, args[2]->decimals);
  unsigned_flag=args[1]->unsigned_flag && args[2]->unsigned_flag;

  if (cached_result_type == STRING_RESULT)
  {
    if (count_string_result_length(cached_field_type, args + 1, 2))
      return;
  }
  else
  {
    collation.set_numeric(); // Number
  }

  uint32 char_length;
  if ((cached_result_type == DECIMAL_RESULT )
      || (cached_result_type == INT_RESULT))
  {
    int len1= args[1]->max_length - args[1]->decimals
      - (args[1]->unsigned_flag ? 0 : 1);

    int len2= args[2]->max_length - args[2]->decimals
      - (args[2]->unsigned_flag ? 0 : 1);

    char_length= max(len1, len2) + decimals + (unsigned_flag ? 0 : 1);
  }
  else
    char_length= max(args[1]->max_char_length(), args[2]->max_char_length());
  fix_char_length(char_length);
}


uint Item_func_if::decimal_precision() const
{
  int arg1_prec= args[1]->decimal_int_part();
  int arg2_prec= args[2]->decimal_int_part();
  int precision=max(arg1_prec,arg2_prec) + decimals;
  return min<uint>(precision, DECIMAL_MAX_PRECISION);
}


double
Item_func_if::val_real()
{
  DBUG_ASSERT(fixed == 1);
  Item *arg= args[0]->val_bool() ? args[1] : args[2];
  double value= arg->val_real();
  null_value=arg->null_value;
  return value;
}

longlong
Item_func_if::val_int()
{
  DBUG_ASSERT(fixed == 1);
  Item *arg= args[0]->val_bool() ? args[1] : args[2];
  longlong value=arg->val_int();
  null_value=arg->null_value;
  return value;
}

String *
Item_func_if::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);

  switch (field_type())
  {
  case MYSQL_TYPE_DATETIME:
  case MYSQL_TYPE_TIMESTAMP:
    return val_string_from_datetime(str);
  case MYSQL_TYPE_DATE:
    return val_string_from_date(str);
  case MYSQL_TYPE_TIME:
    return val_string_from_time(str);
  default:
    {
      Item *item= args[0]->val_bool() ? args[1] : args[2];
      String *res;
      if ((res= item->val_str(str)))
      {
        res->set_charset(collation.collation);
        null_value= 0;
        return res;   
      }
    }
  }
  null_value= true;
  return (String *) 0;
}


my_decimal *
Item_func_if::val_decimal(my_decimal *decimal_value)
{
  DBUG_ASSERT(fixed == 1);
  Item *arg= args[0]->val_bool() ? args[1] : args[2];
  my_decimal *value= arg->val_decimal(decimal_value);
  null_value= arg->null_value;
  return value;
}


bool Item_func_if::get_date(MYSQL_TIME *ltime, uint fuzzydate)
{
  DBUG_ASSERT(fixed == 1);
  Item *arg= args[0]->val_bool() ? args[1] : args[2];
  return (null_value= arg->get_date(ltime, fuzzydate));
}


bool Item_func_if::get_time(MYSQL_TIME *ltime)
{
  DBUG_ASSERT(fixed == 1);
  Item *arg= args[0]->val_bool() ? args[1] : args[2];
  return (null_value= arg->get_time(ltime));
}


void
Item_func_nullif::fix_length_and_dec()
{
  Item_bool_func2::fix_length_and_dec();
  maybe_null=1;
  if (args[0])					// Only false if EOM
  {
    max_length=args[0]->max_length;
    decimals=args[0]->decimals;
    unsigned_flag= args[0]->unsigned_flag;
    cached_result_type= args[0]->result_type();
    if (cached_result_type == STRING_RESULT &&
        agg_arg_charsets_for_comparison(collation, args, arg_count))
      return;
  }
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

double
Item_func_nullif::val_real()
{
  DBUG_ASSERT(fixed == 1);
  double value;
  if (!cmp.compare())
  {
    null_value=1;
    return 0.0;
  }
  value= args[0]->val_real();
  null_value=args[0]->null_value;
  return value;
}

longlong
Item_func_nullif::val_int()
{
  DBUG_ASSERT(fixed == 1);
  longlong value;
  if (!cmp.compare())
  {
    null_value=1;
    return 0;
  }
  value=args[0]->val_int();
  null_value=args[0]->null_value;
  return value;
}

String *
Item_func_nullif::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  String *res;
  if (!cmp.compare())
  {
    null_value=1;
    return 0;
  }
  res=args[0]->val_str(str);
  null_value=args[0]->null_value;
  return res;
}


my_decimal *
Item_func_nullif::val_decimal(my_decimal * decimal_value)
{
  DBUG_ASSERT(fixed == 1);
  my_decimal *res;
  if (!cmp.compare())
  {
    null_value=1;
    return 0;
  }
  res= args[0]->val_decimal(decimal_value);
  null_value= args[0]->null_value;
  return res;
}


bool
Item_func_nullif::is_null()
{
  return (null_value= (!cmp.compare() ? 1 : args[0]->null_value)); 
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

Item *Item_func_case::find_item(String *str)
{
  uint value_added_map= 0;

  if (first_expr_num == -1)
  {
    for (uint i=0 ; i < ncases ; i+=2)
    {
      // No expression between CASE and the first WHEN
      if (args[i]->val_bool())
	return args[i+1];
      continue;
    }
  }
  else
  {
    /* Compare every WHEN argument with it and return the first match */
    for (uint i=0 ; i < ncases ; i+=2)
    {
      if (args[i]->real_item()->type() == NULL_ITEM)
        continue;
      cmp_type= item_cmp_type(left_result_type, args[i]->result_type());
      DBUG_ASSERT(cmp_type != ROW_RESULT);
      DBUG_ASSERT(cmp_items[(uint)cmp_type]);
      if (!(value_added_map & (1U << (uint)cmp_type)))
      {
        cmp_items[(uint)cmp_type]->store_value(args[first_expr_num]);
        if ((null_value=args[first_expr_num]->null_value))
          return else_expr_num != -1 ? args[else_expr_num] : 0;
        value_added_map|= 1U << (uint)cmp_type;
      }
      if (!cmp_items[(uint)cmp_type]->cmp(args[i]) && !args[i]->null_value)
        return args[i + 1];
    }
  }
  // No, WHEN clauses all missed, return ELSE expression
  return else_expr_num != -1 ? args[else_expr_num] : 0;
}


String *Item_func_case::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  switch (field_type()) {
  case MYSQL_TYPE_DATETIME:
  case MYSQL_TYPE_TIMESTAMP:
    return val_string_from_datetime(str);
  case MYSQL_TYPE_DATE:
    return val_string_from_date(str);
  case MYSQL_TYPE_TIME:
    return val_string_from_time(str);
  default:
    {
      Item *item= find_item(str);
      if (item)
      {
        String *res;
        if ((res= item->val_str(str)))
        {
          res->set_charset(collation.collation);
          null_value= 0;
          return res;
        }
      }
    }
  }
  null_value= true;
  return (String *) 0;
}


longlong Item_func_case::val_int()
{
  DBUG_ASSERT(fixed == 1);
  char buff[MAX_FIELD_WIDTH];
  String dummy_str(buff,sizeof(buff),default_charset());
  Item *item=find_item(&dummy_str);
  longlong res;

  if (!item)
  {
    null_value=1;
    return 0;
  }
  res=item->val_int();
  null_value=item->null_value;
  return res;
}

double Item_func_case::val_real()
{
  DBUG_ASSERT(fixed == 1);
  char buff[MAX_FIELD_WIDTH];
  String dummy_str(buff,sizeof(buff),default_charset());
  Item *item=find_item(&dummy_str);
  double res;

  if (!item)
  {
    null_value=1;
    return 0;
  }
  res= item->val_real();
  null_value=item->null_value;
  return res;
}


my_decimal *Item_func_case::val_decimal(my_decimal *decimal_value)
{
  DBUG_ASSERT(fixed == 1);
  char buff[MAX_FIELD_WIDTH];
  String dummy_str(buff, sizeof(buff), default_charset());
  Item *item= find_item(&dummy_str);
  my_decimal *res;

  if (!item)
  {
    null_value=1;
    return 0;
  }

  res= item->val_decimal(decimal_value);
  null_value= item->null_value;
  return res;
}


bool Item_func_case::get_date(MYSQL_TIME *ltime, uint fuzzydate)
{
  DBUG_ASSERT(fixed == 1);
  char buff[MAX_FIELD_WIDTH];
  String dummy_str(buff, sizeof(buff), default_charset());
  Item *item= find_item(&dummy_str);
  if (!item)
    return (null_value= true);
  return (null_value= item->get_date(ltime, fuzzydate));
}


bool Item_func_case::get_time(MYSQL_TIME *ltime)
{
  DBUG_ASSERT(fixed == 1);
  char buff[MAX_FIELD_WIDTH];
  String dummy_str(buff, sizeof(buff), default_charset());
  Item *item= find_item(&dummy_str);
  if (!item)
    return (null_value= true);
  return (null_value= item->get_time(ltime));
}


bool Item_func_case::fix_fields(THD *thd, Item **ref)
{
  /*
    buff should match stack usage from
    Item_func_case::val_int() -> Item_func_case::find_item()
  */
  uchar buff[MAX_FIELD_WIDTH*2+sizeof(String)*2+sizeof(String*)*2+sizeof(double)*2+sizeof(longlong)*2];
  bool res= Item_func::fix_fields(thd, ref);
  /*
    Call check_stack_overrun after fix_fields to be sure that stack variable
    is not optimized away
  */
  if (check_stack_overrun(thd, STACK_MIN_SIZE, buff))
    return TRUE;				// Fatal error flag is set!
  return res;
}


void Item_func_case::agg_num_lengths(Item *arg)
{
  uint len= my_decimal_length_to_precision(arg->max_length, arg->decimals,
                                           arg->unsigned_flag) - arg->decimals;
  set_if_bigger(max_length, len); 
  set_if_bigger(decimals, arg->decimals);
  unsigned_flag= unsigned_flag && arg->unsigned_flag; 
}


/**
  Check if (*place) and new_value points to different Items and call
  THD::change_item_tree() if needed.

  This function is a workaround for implementation deficiency in
  Item_func_case. The problem there is that the 'args' attribute contains
  Items from different expressions.
 
  The function must not be used elsewhere and will be remove eventually.
*/

static void change_item_tree_if_needed(THD *thd,
                                       Item **place,
                                       Item *new_value)
{
  if (*place == new_value)
    return;

  thd->change_item_tree(place, new_value);
}


void Item_func_case::fix_length_and_dec()
{
  Item **agg;
  uint nagg;
  uint found_types= 0;
  THD *thd= current_thd;

  if (!(agg= (Item**) sql_alloc(sizeof(Item*)*(ncases+1))))
    return;

  /*
    fix_fields() does not handle ELSE expression automatically,
    as it's not in the args[] list. Check its maybe_null value.
  */
  if (else_expr_num == -1 || args[else_expr_num]->maybe_null)
    maybe_null=1;

  /*
    Aggregate all THEN and ELSE expression types
    and collations when string result
  */

  for (nagg= 0; nagg < ncases / 2; nagg++)
    agg[nagg]= args[nagg * 2 + 1];

  if (else_expr_num != -1)
    agg[nagg++]= args[else_expr_num];

  cached_field_type= agg_field_type(agg, nagg);
  agg_result_type(&cached_result_type, agg, nagg);
  if (cached_result_type == STRING_RESULT)
  {
    /* Note: String result type is the same for CASE and COALESCE. */
    if (count_string_result_length(cached_field_type, agg, nagg))
      return;
    /*
      Copy all THEN and ELSE items back to args[] array.
      Some of the items might have been changed to Item_func_conv_charset.
    */
    for (nagg= 0 ; nagg < ncases / 2 ; nagg++)
      change_item_tree_if_needed(thd, &args[nagg * 2 + 1], agg[nagg]);

    if (else_expr_num != -1)
      change_item_tree_if_needed(thd, &args[else_expr_num], agg[nagg++]);
  }
  else
  {
    /*
      TODO: Perhaps CASE and COALESCE should eventually
      share fix_length_and_dec() code for numeric result types.
      COALESCE is a CASE abbreviation according to the standard,
      so there is little sense to have separate fix_length_and_dec()
      implementations for numeric result types and thus potentually
      different behaviour.
    */
    collation.set_numeric();
    max_length= 0;
    decimals= 0;
    unsigned_flag= TRUE;
    for (uint i= 0; i < nagg; i++)
      agg_num_lengths(agg[i]);
    max_length= my_decimal_precision_to_length_no_truncation(max_length +
                                                             decimals, decimals,
                                                             unsigned_flag);
  }


  /*
    Aggregate first expression and all WHEN expression types
    and collations when string comparison
  */
  if (first_expr_num != -1)
  {
    uint i;
    agg[0]= args[first_expr_num];
    left_result_type= agg[0]->result_type();

    /*
      As the first expression and WHEN expressions
      are intermixed in args[] array THEN and ELSE items,
      extract the first expression and all WHEN expressions into 
      a temporary array, to process them easier.
    */
    for (nagg= 0; nagg < ncases/2 ; nagg++)
      agg[nagg+1]= args[nagg*2];
    nagg++;
    if (!(found_types= collect_cmp_types(agg, nagg)))
      return;
    if (found_types & (1U << STRING_RESULT))
    {
      /*
        If we'll do string comparison, we also need to aggregate
        character set and collation for first/WHEN items and
        install converters for some of them to cmp_collation when necessary.
        This is done because cmp_item compatators cannot compare
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
        return;
      /*
        Now copy first expression and all WHEN expressions back to args[]
        arrray, because some of the items might have been changed to converters
        (e.g. Item_func_conv_charset, or Item_string for constants).
      */
      change_item_tree_if_needed(thd, &args[first_expr_num], agg[0]);

      for (nagg= 0; nagg < ncases / 2; nagg++)
        change_item_tree_if_needed(thd, &args[nagg * 2], agg[nagg + 1]);
    }
    for (i= 0; i <= (uint)DECIMAL_RESULT; i++)
    {
      if (found_types & (1U << i) && !cmp_items[i])
      {
        DBUG_ASSERT((Item_result)i != ROW_RESULT);
        if (!(cmp_items[i]=
            cmp_item::get_comparator((Item_result)i,
                                     cmp_collation.collation)))
          return;
      }
    }
    /*
      Set cmp_context of all WHEN arguments. This prevents
      Item_field::equal_fields_propagator() from transforming a
      zerofill argument into a string constant. Such a change would
      require rebuilding cmp_items.
    */
    for (i= 0; i < ncases; i+= 2)
      args[i]->cmp_context= item_cmp_type(left_result_type,
                                          args[i]->result_type());
  }

}


uint Item_func_case::decimal_precision() const
{
  int max_int_part=0;
  for (uint i=0 ; i < ncases ; i+=2)
    set_if_bigger(max_int_part, args[i+1]->decimal_int_part());

  if (else_expr_num != -1) 
    set_if_bigger(max_int_part, args[else_expr_num]->decimal_int_part());
  return min<uint>(max_int_part + decimals, DECIMAL_MAX_PRECISION);
}


/**
  @todo
    Fix this so that it prints the whole CASE expression
*/

void Item_func_case::print(String *str, enum_query_type query_type)
{
  str->append(STRING_WITH_LEN("(case "));
  if (first_expr_num != -1)
  {
    args[first_expr_num]->print(str, query_type);
    str->append(' ');
  }
  for (uint i=0 ; i < ncases ; i+=2)
  {
    str->append(STRING_WITH_LEN("when "));
    args[i]->print(str, query_type);
    str->append(STRING_WITH_LEN(" then "));
    args[i+1]->print(str, query_type);
    str->append(' ');
  }
  if (else_expr_num != -1)
  {
    str->append(STRING_WITH_LEN("else "));
    args[else_expr_num]->print(str, query_type);
    str->append(' ');
  }
  str->append(STRING_WITH_LEN("end)"));
}


void Item_func_case::cleanup()
{
  uint i;
  DBUG_ENTER("Item_func_case::cleanup");
  Item_func::cleanup();
  for (i= 0; i <= (uint)DECIMAL_RESULT; i++)
  {
    delete cmp_items[i];
    cmp_items[i]= 0;
  }
  DBUG_VOID_RETURN;
}


/**
  Coalesce - return first not NULL argument.
*/

String *Item_func_coalesce::str_op(String *str)
{
  DBUG_ASSERT(fixed == 1);
  null_value=0;
  for (uint i=0 ; i < arg_count ; i++)
  {
    String *res;
    if ((res=args[i]->val_str(str)))
      return res;
  }
  null_value=1;
  return 0;
}

longlong Item_func_coalesce::int_op()
{
  DBUG_ASSERT(fixed == 1);
  null_value=0;
  for (uint i=0 ; i < arg_count ; i++)
  {
    longlong res=args[i]->val_int();
    if (!args[i]->null_value)
      return res;
  }
  null_value=1;
  return 0;
}

double Item_func_coalesce::real_op()
{
  DBUG_ASSERT(fixed == 1);
  null_value=0;
  for (uint i=0 ; i < arg_count ; i++)
  {
    double res= args[i]->val_real();
    if (!args[i]->null_value)
      return res;
  }
  null_value=1;
  return 0;
}


my_decimal *Item_func_coalesce::decimal_op(my_decimal *decimal_value)
{
  DBUG_ASSERT(fixed == 1);
  null_value= 0;
  for (uint i= 0; i < arg_count; i++)
  {
    my_decimal *res= args[i]->val_decimal(decimal_value);
    if (!args[i]->null_value)
      return res;
  }
  null_value=1;
  return 0;
}



bool Item_func_coalesce::date_op(MYSQL_TIME *ltime, uint fuzzydate)
{
  DBUG_ASSERT(fixed == 1);
  for (uint i= 0; i < arg_count; i++)
  {
    if (!args[i]->get_date(ltime, fuzzydate))
      return (null_value= false);
  }
  return (null_value= true);
}


bool Item_func_coalesce::time_op(MYSQL_TIME *ltime)
{
  DBUG_ASSERT(fixed == 1);
  for (uint i= 0; i < arg_count; i++)
  {
    if (!args[i]->get_time(ltime))
      return (null_value= false);
  }
  return (null_value= true);
}


void Item_func_coalesce::fix_length_and_dec()
{
  cached_field_type= agg_field_type(args, arg_count);
  agg_result_type(&hybrid_type, args, arg_count);
  switch (hybrid_type) {
  case STRING_RESULT:
    if (count_string_result_length(cached_field_type, args, arg_count))
      return;
    break;
  case DECIMAL_RESULT:
    count_decimal_length();
    break;
  case REAL_RESULT:
    count_real_length();
    break;
  case INT_RESULT:
    count_only_length(args, arg_count);
    decimals= 0;
    break;
  case ROW_RESULT:
  default:
    DBUG_ASSERT(0);
  }
}

/****************************************************************************
 Classes and function for the IN operator
****************************************************************************/

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
static inline int cmp_longs (longlong a_val, longlong b_val)
{
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
static inline int cmp_ulongs (ulonglong a_val, ulonglong b_val)
{
  return a_val < b_val ? -1 : a_val == b_val ? 0 : 1;
}


/*
  Compare two integers in IN value list format (packed_longlong) 

  SYNOPSIS
    cmp_longlong()
      cmp_arg   an argument passed to the calling function (my_qsort2)
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
int cmp_longlong(void *cmp_arg, 
                 in_longlong::packed_longlong *a,
                 in_longlong::packed_longlong *b)
{
  if (a->unsigned_flag != b->unsigned_flag)
  { 
    /* 
      One of the args is unsigned and is too big to fit into the 
      positive signed range. Report no match.
    */  
    if ((a->unsigned_flag && ((ulonglong) a->val) > (ulonglong) LONGLONG_MAX) ||
        (b->unsigned_flag && ((ulonglong) b->val) > (ulonglong) LONGLONG_MAX))
      return a->unsigned_flag ? 1 : -1;
    /*
      Although the signedness differs both args can fit into the signed 
      positive range. Make them signed and compare as usual.
    */  
    return cmp_longs (a->val, b->val);
  }
  if (a->unsigned_flag)
    return cmp_ulongs ((ulonglong) a->val, (ulonglong) b->val);
  else
    return cmp_longs (a->val, b->val);
}

static int cmp_double(void *cmp_arg, double *a,double *b)
{
  return *a < *b ? -1 : *a == *b ? 0 : 1;
}

static int cmp_row(void *cmp_arg, cmp_item_row *a, cmp_item_row *b)
{
  return a->compare(b);
}


static int cmp_decimal(void *cmp_arg, my_decimal *a, my_decimal *b)
{
  /*
    We need call of fixing buffer pointer, because fast sort just copy
    decimal buffers in memory and pointers left pointing on old buffer place
  */
  a->fix_buffer_pointer();
  b->fix_buffer_pointer();
  return my_decimal_cmp(a, b);
}


int in_vector::find(Item *item)
{
  uchar *result=get_value(item);
  if (!result || !used_count)
    return 0;				// Null value

  uint start,end;
  start=0; end=used_count-1;
  while (start != end)
  {
    uint mid=(start+end+1)/2;
    int res;
    if ((res=(*compare)(collation, base+mid*size, result)) == 0)
      return 1;
    if (res < 0)
      start=mid;
    else
      end=mid-1;
  }
  return (int) ((*compare)(collation, base+start*size, result) == 0);
}

in_string::in_string(uint elements,qsort2_cmp cmp_func,
                     const CHARSET_INFO *cs)
  :in_vector(elements, sizeof(String), cmp_func, cs),
   tmp(buff, sizeof(buff), &my_charset_bin)
{}

in_string::~in_string()
{
  if (base)
  {
    // base was allocated with help of sql_alloc => following is OK
    for (uint i=0 ; i < count ; i++)
      ((String*) base)[i].free();
  }
}

void in_string::set(uint pos,Item *item)
{
  String *str=((String*) base)+pos;
  String *res=item->val_str(str);
  if (res && res != str)
  {
    if (res->uses_buffer_owned_by(str))
      res->copy();
    if (item->type() == Item::FUNC_ITEM)
      str->copy(*res);
    else
      *str= *res;
  }
  if (!str->charset())
  {
    const CHARSET_INFO *cs;
    if (!(cs= item->collation.collation))
      cs= &my_charset_bin;		// Should never happen for STR items
    str->set_charset(cs);
  }
}


uchar *in_string::get_value(Item *item)
{
  return (uchar*) item->val_str(&tmp);
}

in_row::in_row(uint elements, Item * item)
{
  base= (char*) new cmp_item_row[count= elements];
  size= sizeof(cmp_item_row);
  compare= (qsort2_cmp) cmp_row;
  /*
    We need to reset these as otherwise we will call sort() with
    uninitialized (even if not used) elements
  */
  used_count= elements;
  collation= 0;
}

in_row::~in_row()
{
  if (base)
    delete [] (cmp_item_row*) base;
}

uchar *in_row::get_value(Item *item)
{
  tmp.store_value(item);
  if (item->is_null())
    return 0;
  return (uchar *)&tmp;
}

void in_row::set(uint pos, Item *item)
{
  DBUG_ENTER("in_row::set");
  DBUG_PRINT("enter", ("pos: %u  item: 0x%lx", pos, (ulong) item));
  ((cmp_item_row*) base)[pos].store_value_by_template(&tmp, item);
  DBUG_VOID_RETURN;
}

in_longlong::in_longlong(uint elements)
  :in_vector(elements,sizeof(packed_longlong),(qsort2_cmp) cmp_longlong, 0)
{}

void in_longlong::set(uint pos,Item *item)
{
  struct packed_longlong *buff= &((packed_longlong*) base)[pos];
  
  buff->val= item->val_int();
  buff->unsigned_flag= item->unsigned_flag;
}

uchar *in_longlong::get_value(Item *item)
{
  tmp.val= item->val_int();
  if (item->null_value)
    return 0;
  tmp.unsigned_flag= item->unsigned_flag;
  return (uchar*) &tmp;
}


void in_time_as_longlong::set(uint pos,Item *item)
{
  struct packed_longlong *buff= &((packed_longlong*) base)[pos];
  buff->val= item->val_time_temporal();
  buff->unsigned_flag= item->unsigned_flag;
}


uchar *in_time_as_longlong::get_value(Item *item)
{
  tmp.val= item->val_time_temporal();
  if (item->null_value)
    return 0;
  tmp.unsigned_flag= item->unsigned_flag;
  return (uchar*) &tmp;
}


void in_datetime_as_longlong::set(uint pos,Item *item)
{
  struct packed_longlong *buff= &((packed_longlong*) base)[pos];
  buff->val= item->val_date_temporal();
  buff->unsigned_flag= item->unsigned_flag;
}


uchar *in_datetime_as_longlong::get_value(Item *item)
{
  tmp.val= item->val_date_temporal();
  if (item->null_value)
    return 0;
  tmp.unsigned_flag= item->unsigned_flag;
  return (uchar*) &tmp;
}


void in_datetime::set(uint pos,Item *item)
{
  Item **tmp_item= &item;
  bool is_null;
  struct packed_longlong *buff= &((packed_longlong*) base)[pos];

  buff->val= get_datetime_value(thd, &tmp_item, 0, warn_item, &is_null);
  buff->unsigned_flag= 1L;
}


uchar *in_datetime::get_value(Item *item)
{
  bool is_null;
  Item **tmp_item= lval_cache ? &lval_cache : &item;
  tmp.val= get_datetime_value(thd, &tmp_item, &lval_cache, warn_item, &is_null);
  if (item->null_value)
    return 0;
  tmp.unsigned_flag= 1L;
  return (uchar*) &tmp;
}


in_double::in_double(uint elements)
  :in_vector(elements,sizeof(double),(qsort2_cmp) cmp_double, 0)
{}

void in_double::set(uint pos,Item *item)
{
  ((double*) base)[pos]= item->val_real();
}

uchar *in_double::get_value(Item *item)
{
  tmp= item->val_real();
  if (item->null_value)
    return 0;					/* purecov: inspected */
  return (uchar*) &tmp;
}


in_decimal::in_decimal(uint elements)
  :in_vector(elements, sizeof(my_decimal),(qsort2_cmp) cmp_decimal, 0)
{}


void in_decimal::set(uint pos, Item *item)
{
  /* as far as 'item' is constant, we can store reference on my_decimal */
  my_decimal *dec= ((my_decimal *)base) + pos;
  dec->len= DECIMAL_BUFF_LENGTH;
  dec->fix_buffer_pointer();
  my_decimal *res= item->val_decimal(dec);
  /* if item->val_decimal() is evaluated to NULL then res == 0 */ 
  if (!item->null_value && res != dec)
    my_decimal2decimal(res, dec);
}


uchar *in_decimal::get_value(Item *item)
{
  my_decimal *result= item->val_decimal(&val);
  if (item->null_value)
    return 0;
  return (uchar *)result;
}


cmp_item* cmp_item::get_comparator(Item_result type,
                                   const CHARSET_INFO *cs)
{
  switch (type) {
  case STRING_RESULT:
    return new cmp_item_sort_string(cs);
  case INT_RESULT:
    return new cmp_item_int;
  case REAL_RESULT:
    return new cmp_item_real;
  case ROW_RESULT:
    return new cmp_item_row;
  case DECIMAL_RESULT:
    return new cmp_item_decimal;
  default:
    DBUG_ASSERT(0);
    break;
  }
  return 0; // to satisfy compiler :)
}


cmp_item* cmp_item_sort_string::make_same()
{
  return new cmp_item_sort_string_in_static(cmp_charset);
}

cmp_item* cmp_item_int::make_same()
{
  return new cmp_item_int();
}

cmp_item* cmp_item_real::make_same()
{
  return new cmp_item_real();
}

cmp_item* cmp_item_row::make_same()
{
  return new cmp_item_row();
}


cmp_item_row::~cmp_item_row()
{
  DBUG_ENTER("~cmp_item_row");
  DBUG_PRINT("enter",("this: 0x%lx", (long) this));
  if (comparators)
  {
    for (uint i= 0; i < n; i++)
    {
      if (comparators[i])
	delete comparators[i];
    }
  }
  DBUG_VOID_RETURN;
}


void cmp_item_row::alloc_comparators(Item *item)
{
  n= item->cols();
  DBUG_ASSERT(comparators == NULL);
  if (!comparators)
    comparators= (cmp_item **) current_thd->calloc(sizeof(cmp_item *)*n);
  if (comparators)
  {
    for (uint i= 0; i < n; i++)
    {
      DBUG_ASSERT(comparators[i] == NULL);
      Item *item_i= item->element_index(i);
      if (!(comparators[i]=
            cmp_item::get_comparator(item_i->result_type(),
                                     item_i->collation.collation)))
        break;                                  // new failed
      if (item_i->result_type() == ROW_RESULT)
        static_cast<cmp_item_row*>(comparators[i])->alloc_comparators(item_i);
    }
  }
}


void cmp_item_row::store_value(Item *item)
{
  DBUG_ENTER("cmp_item_row::store_value");
  DBUG_ASSERT(comparators);
  if (comparators)
  {
    item->bring_value();
    item->null_value= 0;
    for (uint i= 0; i < n; i++)
    {
      comparators[i]->store_value(item->element_index(i));
      item->null_value|= item->element_index(i)->null_value;
    }
  }
  DBUG_VOID_RETURN;
}


void cmp_item_row::store_value_by_template(cmp_item *t, Item *item)
{
  cmp_item_row *tmpl= (cmp_item_row*) t;
  if (tmpl->n != item->cols())
  {
    my_error(ER_OPERAND_COLUMNS, MYF(0), tmpl->n);
    return;
  }
  n= tmpl->n;
  if ((comparators= (cmp_item **) sql_alloc(sizeof(cmp_item *)*n)))
  {
    item->bring_value();
    item->null_value= 0;
    for (uint i=0; i < n; i++)
    {
      if (!(comparators[i]= tmpl->comparators[i]->make_same()))
	break;					// new failed
      comparators[i]->store_value_by_template(tmpl->comparators[i],
					      item->element_index(i));
      item->null_value|= item->element_index(i)->null_value;
    }
  }
}


int cmp_item_row::cmp(Item *arg)
{
  arg->null_value= 0;
  if (arg->cols() != n)
  {
    my_error(ER_OPERAND_COLUMNS, MYF(0), n);
    return 1;
  }
  bool was_null= 0;
  arg->bring_value();
  for (uint i=0; i < n; i++)
  {
    if (comparators[i]->cmp(arg->element_index(i)))
    {
      if (!arg->element_index(i)->null_value)
	return 1;
      was_null= 1;
    }
  }
  return (arg->null_value= was_null);
}


int cmp_item_row::compare(cmp_item *c)
{
  cmp_item_row *l_cmp= (cmp_item_row *) c;
  for (uint i=0; i < n; i++)
  {
    int res;
    if ((res= comparators[i]->compare(l_cmp->comparators[i])))
      return res;
  }
  return 0;
}


void cmp_item_decimal::store_value(Item *item)
{
  my_decimal *val= item->val_decimal(&value);
  /* val may be zero if item is nnull */
  if (val && val != &value)
    my_decimal2decimal(val, &value);
}


int cmp_item_decimal::cmp(Item *arg)
{
  my_decimal tmp_buf, *tmp= arg->val_decimal(&tmp_buf);
  if (arg->null_value)
    return 1;
  return my_decimal_cmp(&value, tmp);
}


int cmp_item_decimal::compare(cmp_item *arg)
{
  cmp_item_decimal *l_cmp= (cmp_item_decimal*) arg;
  return my_decimal_cmp(&value, &l_cmp->value);
}


cmp_item* cmp_item_decimal::make_same()
{
  return new cmp_item_decimal();
}


void cmp_item_datetime::store_value(Item *item)
{
  bool is_null;
  Item **tmp_item= lval_cache ? &lval_cache : &item;
  value= get_datetime_value(thd, &tmp_item, &lval_cache, warn_item, &is_null);
}


int cmp_item_datetime::cmp(Item *arg)
{
  bool is_null;
  Item **tmp_item= &arg;
  return value !=
    get_datetime_value(thd, &tmp_item, 0, warn_item, &is_null);
}


int cmp_item_datetime::compare(cmp_item *ci)
{
  cmp_item_datetime *l_cmp= (cmp_item_datetime *)ci;
  return (value < l_cmp->value) ? -1 : ((value == l_cmp->value) ? 0 : 1);
}


cmp_item *cmp_item_datetime::make_same()
{
  return new cmp_item_datetime(warn_item);
}


bool Item_func_in::nulls_in_row()
{
  Item **arg,**arg_end;
  for (arg= args+1, arg_end= args+arg_count; arg != arg_end ; arg++)
  {
    if ((*arg)->null_inside())
      return 1;
  }
  return 0;
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

bool Item_func_in::fix_fields(THD *thd, Item **ref)
{
  if (Item_func_opt_neg::fix_fields(thd, ref))
    return true;

  // not_null_tables_cache == union(T1(e),union(T1(ei)))
  if (pred_level && negated)
    return false;

  // not_null_tables_cache = union(T1(e),intersection(T1(ei)))
  not_null_tables_cache= ~(table_map) 0;
  Item **arg_end= args + arg_count;
  for (Item **arg= args + 1; arg != arg_end; arg++)
    not_null_tables_cache&= (*arg)->not_null_tables();
  not_null_tables_cache|= (*args)->not_null_tables();

  return false;
}


void Item_func_in::fix_after_pullout(st_select_lex *parent_select,
                                     st_select_lex *removed_select)
{
  Item_func_opt_neg::fix_after_pullout(parent_select, removed_select);

  // not_null_tables_cache == union(T1(e),union(T1(ei)))
  if (pred_level && negated)
    return;

  // not_null_tables_cache = union(T1(e),intersection(T1(ei)))
  not_null_tables_cache= ~(table_map) 0;
  Item **arg_end= args + arg_count;
  for (Item **arg= args + 1; arg != arg_end; arg++)
    not_null_tables_cache&= (*arg)->not_null_tables();
  not_null_tables_cache|= (*args)->not_null_tables();
}


static int srtcmp_in(CHARSET_INFO *cs, const String *x,const String *y)
{
  return cs->coll->strnncollsp(cs,
                               (uchar *) x->ptr(),x->length(),
                               (uchar *) y->ptr(),y->length(), 0);
}


void Item_func_in::fix_length_and_dec()
{
  Item **arg, **arg_end;
  bool const_itm= 1;
  THD *thd= current_thd;
  bool datetime_found= FALSE;
  /* TRUE <=> arguments values will be compared as DATETIMEs. */
  bool compare_as_datetime= FALSE;
  Item *date_arg= 0;
  uint found_types= 0;
  uint type_cnt= 0, i;
  Item_result cmp_type= STRING_RESULT;
  left_result_type= args[0]->result_type();
  if (!(found_types= collect_cmp_types(args, arg_count, true)))
    return;
  
  for (arg= args + 1, arg_end= args + arg_count; arg != arg_end ; arg++)
  {
    if (!arg[0]->const_item())
    {
      const_itm= 0;
      break;
    }
  }
  for (i= 0; i <= (uint)DECIMAL_RESULT; i++)
  {
    if (found_types & (1U << i))
    {
      (type_cnt)++;
      cmp_type= (Item_result) i;
    }
  }

  if (type_cnt == 1)
  {
    if (cmp_type == STRING_RESULT && 
        agg_arg_charsets_for_comparison(cmp_collation, args, arg_count))
      return;
    arg_types_compatible= TRUE;
  }
  if (type_cnt == 1)
  {
    /*
      When comparing rows create the row comparator object beforehand to ease
      the DATETIME comparison detection procedure.
    */
    if (cmp_type == ROW_RESULT)
    {
      cmp_item_row *cmp= 0;
      if (const_itm && !nulls_in_row())
      {
        array= new in_row(arg_count-1, 0);
        cmp= &((in_row*)array)->tmp;
      }
      else
      {
        if (!(cmp= new cmp_item_row))
          return;
        cmp_items[ROW_RESULT]= cmp;
      }
      cmp->n= args[0]->cols();
      cmp->alloc_comparators(args[0]);
    }
    /* All DATE/DATETIME fields/functions has the STRING result type. */
    if (cmp_type == STRING_RESULT || cmp_type == ROW_RESULT)
    {
      uint col, cols= args[0]->cols();

      for (col= 0; col < cols; col++)
      {
        bool skip_column= FALSE;
        /*
          Check that all items to be compared has the STRING result type and at
          least one of them is a DATE/DATETIME item.
        */
        for (arg= args, arg_end= args + arg_count; arg != arg_end ; arg++)
        {
          Item *itm= ((cmp_type == STRING_RESULT) ? arg[0] :
                      arg[0]->element_index(col));
          if (itm->result_type() != STRING_RESULT)
          {
            skip_column= TRUE;
            break;
          }
          else if (itm->is_temporal_with_date())
          {
            datetime_found= TRUE;
            /*
              Internally all DATE/DATETIME values are converted to the DATETIME
              type. So try to find a DATETIME item to issue correct warnings.
            */
            if (!date_arg)
              date_arg= itm;
            else if (itm->field_type() == MYSQL_TYPE_DATETIME)
            {
              date_arg= itm;
              /* All arguments are already checked to have the STRING result. */
              if (cmp_type == STRING_RESULT)
                break;
            }
          }
        }
        if (skip_column)
          continue;
        if (datetime_found)
        {
          if (cmp_type == ROW_RESULT)
          {
            cmp_item **cmp= 0;
            if (array)
              cmp= ((in_row*)array)->tmp.comparators + col;
            else
              cmp= ((cmp_item_row*)cmp_items[ROW_RESULT])->comparators + col;
            *cmp= new cmp_item_datetime(date_arg);
            /* Reset variables for the next column. */
            date_arg= 0;
            datetime_found= FALSE;
          }
          else
            compare_as_datetime= TRUE;
        }
      }
    }
  }
  /*
    Row item with NULLs inside can return NULL or FALSE =>
    they can't be processed as static
  */
  if (type_cnt == 1 && const_itm && !nulls_in_row())
  {
    if (compare_as_datetime)
      array= new in_datetime(date_arg, arg_count - 1);
    else
    {
      /*
        IN must compare INT columns and constants as int values (the same
        way as equality does).
        So we must check here if the column on the left and all the constant 
        values on the right can be compared as integers and adjust the 
        comparison type accordingly.
      */
      bool datetime_as_longlong= false;
      if (args[0]->real_item()->type() == FIELD_ITEM &&
          thd->lex->sql_command != SQLCOM_CREATE_VIEW &&
          thd->lex->sql_command != SQLCOM_SHOW_CREATE &&
          cmp_type != INT_RESULT)
      {
        Item_field *field_item= (Item_field*) (args[0]->real_item());
        if (field_item->field->can_be_compared_as_longlong())
        {
          bool all_converted= true;
          for (arg=args + 1, arg_end= args + arg_count; arg != arg_end ; arg++)
          {
            if (!convert_constant_item (thd, field_item, &arg[0]))
              all_converted= false;
          }
          if (all_converted)
          {
            cmp_type= INT_RESULT;
            datetime_as_longlong= field_item->is_temporal();
          }
        }
      }
      switch (cmp_type) {
      case STRING_RESULT:
        array=new in_string(arg_count-1,(qsort2_cmp) srtcmp_in, 
                            cmp_collation.collation);
        break;
      case INT_RESULT:
        array= datetime_as_longlong ?
               args[0]->field_type() == MYSQL_TYPE_TIME ?
               (in_vector*) new in_time_as_longlong(arg_count - 1) :
               (in_vector*) new in_datetime_as_longlong(arg_count - 1) :
               (in_vector*) new in_longlong(arg_count - 1);
        break;
      case REAL_RESULT:
        array= new in_double(arg_count-1);
        break;
      case ROW_RESULT:
        /*
          The row comparator was created at the beginning.
        */
        break;
      case DECIMAL_RESULT:
        array= new in_decimal(arg_count - 1);
        break;
      default:
        DBUG_ASSERT(0);
        return;
      }
    }
    if (array && !(thd->is_fatal_error))		// If not EOM
    {
      uint j=0;
      for (uint i=1 ; i < arg_count ; i++)
      {
        array->set(j,args[i]);
        if (!args[i]->null_value)                      // Skip NULL values
          j++;
        else
          have_null= 1;
      }
      if ((array->used_count= j))
	array->sort();
    }
  }
  else
  {
    if (compare_as_datetime)
      cmp_items[STRING_RESULT]= new cmp_item_datetime(date_arg);
    else
    {
      for (i= 0; i <= (uint) DECIMAL_RESULT; i++)
      {
        if (found_types & (1U << i) && !cmp_items[i])
        {
          if ((Item_result)i == STRING_RESULT &&
              agg_arg_charsets_for_comparison(cmp_collation, args, arg_count))
            return;
          if (!cmp_items[i] && !(cmp_items[i]=
              cmp_item::get_comparator((Item_result)i,
                                       cmp_collation.collation)))
            return;
        }
      }
    }
  }
  /*
    Set cmp_context of all arguments. This prevents
    Item_field::equal_fields_propagator() from transforming a zerofill integer
    argument into a string constant. Such a change would require rebuilding
    cmp_itmes.
   */
  for (arg= args + 1, arg_end= args + arg_count; arg != arg_end ; arg++)
  {
    arg[0]->cmp_context= item_cmp_type(left_result_type, arg[0]->result_type());
  }
  max_length= 1;
}


void Item_func_in::print(String *str, enum_query_type query_type)
{
  str->append('(');
  args[0]->print(str, query_type);
  if (negated)
    str->append(STRING_WITH_LEN(" not"));
  str->append(STRING_WITH_LEN(" in ("));
  print_args(str, 1, query_type);
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

longlong Item_func_in::val_int()
{
  cmp_item *in_item;
  DBUG_ASSERT(fixed == 1);
  uint value_added_map= 0;
  if (array)
  {
    int tmp=array->find(args[0]);
    null_value=args[0]->null_value || (!tmp && have_null);
    return (longlong) (!null_value && tmp != negated);
  }

  if ((null_value= args[0]->real_item()->type() == NULL_ITEM))
    return 0;

  have_null= 0;
  for (uint i= 1 ; i < arg_count ; i++)
  {
    if (args[i]->real_item()->type() == NULL_ITEM)
    {
      have_null= TRUE;
      continue;
    }
    Item_result cmp_type= item_cmp_type(left_result_type, args[i]->result_type());
    in_item= cmp_items[(uint)cmp_type];
    DBUG_ASSERT(in_item);
    if (!(value_added_map & (1U << (uint)cmp_type)))
    {
      in_item->store_value(args[0]);
      if ((null_value= args[0]->null_value))
        return 0;
      value_added_map|= 1U << (uint)cmp_type;
    }
    if (!in_item->cmp(args[i]) && !args[i]->null_value)
      return (longlong) (!negated);
    have_null|= args[i]->null_value;
  }

  null_value= have_null;
  return (longlong) (!null_value && negated);
}


longlong Item_func_bit_or::val_int()
{
  DBUG_ASSERT(fixed == 1);
  ulonglong arg1= (ulonglong) args[0]->val_int();
  if (args[0]->null_value)
  {
    null_value=1; /* purecov: inspected */
    return 0; /* purecov: inspected */
  }
  ulonglong arg2= (ulonglong) args[1]->val_int();
  if (args[1]->null_value)
  {
    null_value=1;
    return 0;
  }
  null_value=0;
  return (longlong) (arg1 | arg2);
}


longlong Item_func_bit_and::val_int()
{
  DBUG_ASSERT(fixed == 1);
  ulonglong arg1= (ulonglong) args[0]->val_int();
  if (args[0]->null_value)
  {
    null_value=1; /* purecov: inspected */
    return 0; /* purecov: inspected */
  }
  ulonglong arg2= (ulonglong) args[1]->val_int();
  if (args[1]->null_value)
  {
    null_value=1; /* purecov: inspected */
    return 0; /* purecov: inspected */
  }
  null_value=0;
  return (longlong) (arg1 & arg2);
}

Item_cond::Item_cond(THD *thd, Item_cond *item)
  :Item_bool_func(thd, item),
   abort_on_null(item->abort_on_null)
{
  /*
    item->list will be copied by copy_andor_arguments() call
  */
}


void Item_cond::copy_andor_arguments(THD *thd, Item_cond *item, bool real_items)
{
  List_iterator_fast<Item> li(item->list);
  while (Item *it= li++)
    list.push_back((real_items ? it->real_item() : it)->
                   copy_andor_structure(thd, real_items));
}


bool
Item_cond::fix_fields(THD *thd, Item **ref)
{
  DBUG_ASSERT(fixed == 0);
  List_iterator<Item> li(list);
  Item *item;
  Switch_resolve_place SRP(&thd->lex->current_select->resolve_place,
                           st_select_lex::RESOLVE_NONE,
                           functype() != COND_AND_FUNC);
  uchar buff[sizeof(char*)];			// Max local vars in function
  used_tables_cache= 0;
  const_item_cache= true;

  if (functype() == COND_AND_FUNC && abort_on_null)
    not_null_tables_cache= 0;
  else
    not_null_tables_cache= ~(table_map) 0;

  if (check_stack_overrun(thd, STACK_MIN_SIZE, buff))
    return TRUE;				// Fatal error flag is set!
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
  while ((item=li++))
  {
    while (item->type() == Item::COND_ITEM &&
	   ((Item_cond*) item)->functype() == functype() &&
           !((Item_cond*) item)->list.is_empty())
    {						// Identical function
      li.replace(((Item_cond*) item)->list);
      ((Item_cond*) item)->list.empty();
      item= *li.ref();				// new current item
    }
    if (abort_on_null)
      item->top_level_item();

    // item can be substituted in fix_fields
    if ((!item->fixed &&
	 item->fix_fields(thd, li.ref())) ||
	(item= *li.ref())->check_cols(1))
      return TRUE; /* purecov: inspected */
    used_tables_cache|= item->used_tables();
    const_item_cache&=  item->const_item();

    if (functype() == COND_AND_FUNC && abort_on_null)
      not_null_tables_cache|= item->not_null_tables();
    else
      not_null_tables_cache&= item->not_null_tables();
    with_sum_func|=  item->with_sum_func;
    with_subselect|= item->has_subquery();
    with_stored_program|= item->has_stored_program();
    if (item->maybe_null)
      maybe_null= true;
  }
  thd->lex->current_select->cond_count+= list.elements;
  fix_length_and_dec();
  fixed= true;
  return false;
}


void Item_cond::fix_after_pullout(st_select_lex *parent_select,
                                  st_select_lex *removed_select)
{
  List_iterator<Item> li(list);
  Item *item;

  used_tables_cache= get_initial_pseudo_tables();
  const_item_cache= true;

  if (functype() == COND_AND_FUNC && abort_on_null)
    not_null_tables_cache= 0;
  else
    not_null_tables_cache= ~(table_map) 0;

  while ((item=li++))
  {
    item->fix_after_pullout(parent_select, removed_select);
    used_tables_cache|= item->used_tables();
    const_item_cache&= item->const_item();
    if (functype() == COND_AND_FUNC && abort_on_null)
      not_null_tables_cache|= item->not_null_tables();
    else
      not_null_tables_cache&= item->not_null_tables();
  }
}


bool Item_cond::walk(Item_processor processor, bool walk_subquery, uchar *arg)
{
  List_iterator_fast<Item> li(list);
  Item *item;
  while ((item= li++))
    if (item->walk(processor, walk_subquery, arg))
      return 1;
  return Item_func::walk(processor, walk_subquery, arg);
}


/**
  Transform an Item_cond object with a transformer callback function.
  
    The function recursively applies the transform method to each
     member item of the condition list.
    If the call of the method for a member item returns a new item
    the old item is substituted for a new one.
    After this the transformer is applied to the root node
    of the Item_cond object. 
     
  @param transformer   the transformer callback function to be applied to
                       the nodes of the tree of the object
  @param arg           parameter to be passed to the transformer

  @return
    Item returned as the result of transformation of the root node 
*/

Item *Item_cond::transform(Item_transformer transformer, uchar *arg)
{
  DBUG_ASSERT(!current_thd->stmt_arena->is_stmt_prepare());

  List_iterator<Item> li(list);
  Item *item;
  while ((item= li++))
  {
    Item *new_item= item->transform(transformer, arg);
    if (!new_item)
      return 0;

    /*
      THD::change_item_tree() should be called only if the tree was
      really transformed, i.e. when a new item has been created.
      Otherwise we'll be allocating a lot of unnecessary memory for
      change records at each execution.
    */
    if (new_item != item)
      current_thd->change_item_tree(li.ref(), new_item);
  }
  return Item_func::transform(transformer, arg);
}


/**
  Compile Item_cond object with a processor and a transformer
  callback functions.
  
    First the function applies the analyzer to the root node of
    the Item_func object. Then if the analyzer succeeeds (returns TRUE)
    the function recursively applies the compile method to member
    item of the condition list.
    If the call of the method for a member item returns a new item
    the old item is substituted for a new one.
    After this the transformer is applied to the root node
    of the Item_cond object. 
     
  @param analyzer      the analyzer callback function to be applied to the
                       nodes of the tree of the object
  @param[in,out] arg_p parameter to be passed to the analyzer
  @param transformer   the transformer callback function to be applied to the
                       nodes of the tree of the object
  @param arg_t         parameter to be passed to the transformer

  @return              Item returned as result of transformation of the node,
                       the same item if no transformation applied, or NULL if
                       transformation caused an error.
*/

Item *Item_cond::compile(Item_analyzer analyzer, uchar **arg_p,
                         Item_transformer transformer, uchar *arg_t)
{
  if (!(this->*analyzer)(arg_p))
    return this;
  
  List_iterator<Item> li(list);
  Item *item;
  while ((item= li++))
  {
    /* 
      The same parameter value of arg_p must be passed
      to analyze any argument of the condition formula.
    */   
    uchar *arg_v= *arg_p;
    Item *new_item= item->compile(analyzer, &arg_v, transformer, arg_t);
    if (new_item == NULL)
      return NULL;
    if (new_item != item)
      current_thd->change_item_tree(li.ref(), new_item);
  }
  return Item_func::transform(transformer, arg_t);
}

void Item_cond::traverse_cond(Cond_traverser traverser,
                              void *arg, traverse_order order)
{
  List_iterator<Item> li(list);
  Item *item;

  switch(order) {
  case(PREFIX):
    (*traverser)(this, arg);
    while ((item= li++))
    {
      item->traverse_cond(traverser, arg, order);
    }
    (*traverser)(NULL, arg);
    break;
  case(POSTFIX):
    while ((item= li++))
    {
      item->traverse_cond(traverser, arg, order);
    }
    (*traverser)(this, arg);
  }
}

/**
  Move SUM items out from item tree and replace with reference.

  The split is done to get an unique item for each SUM function
  so that we can easily find and calculate them.
  (Calculation done by update_sum_func() and copy_sum_funcs() in
  sql_select.cc)

  @param thd			Thread handler
  @param ref_pointer_array	Pointer to array of reference fields
  @param fields		All fields in select

  @note
    This function is run on all expression (SELECT list, WHERE, HAVING etc)
    that have or refer (HAVING) to a SUM expression.
*/

void Item_cond::split_sum_func(THD *thd, Ref_ptr_array ref_pointer_array,
                               List<Item> &fields)
{
  List_iterator<Item> li(list);
  Item *item;
  while ((item= li++))
    item->split_sum_func2(thd, ref_pointer_array, fields, li.ref(), TRUE);
}


void Item_cond::update_used_tables()
{
  List_iterator_fast<Item> li(list);
  Item *item;

  used_tables_cache=0;
  const_item_cache=1;
  with_subselect= false;
  with_stored_program= false;
  while ((item=li++))
  {
    item->update_used_tables();
    used_tables_cache|= item->used_tables();
    const_item_cache&= item->const_item();
    with_subselect|= item->has_subquery();
    with_stored_program|= item->has_stored_program();
  }
}


void Item_cond::print(String *str, enum_query_type query_type)
{
  str->append('(');
  List_iterator_fast<Item> li(list);
  Item *item;
  if ((item=li++))
    item->print(str, query_type);
  while ((item=li++))
  {
    str->append(' ');
    str->append(func_name());
    str->append(' ');
    item->print(str, query_type);
  }
  str->append(')');
}


void Item_cond::neg_arguments(THD *thd)
{
  List_iterator<Item> li(list);
  Item *item;
  while ((item= li++))		/* Apply not transformation to the arguments */
  {
    Item *new_item= item->neg_transformer(thd);
    if (!new_item)
    {
      if (!(new_item= new Item_func_not(item)))
	return;					// Fatal OEM error
    }
    (void) li.replace(new_item);
  }
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


longlong Item_cond_and::val_int()
{
  DBUG_ASSERT(fixed == 1);
  List_iterator_fast<Item> li(list);
  Item *item;
  null_value= 0;
  while ((item=li++))
  {
    if (!item->val_bool())
    {
      if (abort_on_null || !(null_value= item->null_value))
	return 0;				// return FALSE
    }
  }
  return null_value ? 0 : 1;
}


longlong Item_cond_or::val_int()
{
  DBUG_ASSERT(fixed == 1);
  List_iterator_fast<Item> li(list);
  Item *item;
  null_value=0;
  while ((item=li++))
  {
    if (item->val_bool())
    {
      null_value=0;
      return 1;
    }
    if (item->null_value)
      null_value=1;
  }
  return 0;
}

/**
  Create an AND expression from two expressions.

  @param a	expression or NULL
  @param b    	expression.
  @param org_item	Don't modify a if a == *org_item.
                        If a == NULL, org_item is set to point at b,
                        to ensure that future calls will not modify b.

  @note
    This will not modify item pointed to by org_item or b
    The idea is that one can call this in a loop and create and
    'and' over all items without modifying any of the original items.

  @retval
    NULL	Error
  @retval
    Item
*/

Item *and_expressions(Item *a, Item *b, Item **org_item)
{
  if (!a)
    return (*org_item= (Item*) b);
  if (a == *org_item)
  {
    Item_cond *res;
    if ((res= new Item_cond_and(a, (Item*) b)))
    {
      res->set_used_tables(a->used_tables() | b->used_tables());
      res->set_not_null_tables(a->not_null_tables() | b->not_null_tables());
    }
    return res;
  }
  if (((Item_cond_and*) a)->add((Item*) b))
    return 0;
  ((Item_cond_and*) a)->set_used_tables(a->used_tables() | b->used_tables());
  ((Item_cond_and*) a)->set_not_null_tables(a->not_null_tables() |
                                            b->not_null_tables());
  return a;
}


longlong Item_func_isnull::val_int()
{
  DBUG_ASSERT(fixed == 1);
  /*
    Handle optimization if the argument can't be null
    This has to be here because of the test in update_used_tables().
  */
  if (const_item_cache)
    return cached_value;
  return args[0]->is_null() ? 1: 0;
}

longlong Item_is_not_null_test::val_int()
{
  DBUG_ASSERT(fixed == 1);
  DBUG_ENTER("Item_is_not_null_test::val_int");
  if (!used_tables_cache && !with_subselect && !with_stored_program)
  {
    /*
     TODO: Currently this branch never executes, since used_tables_cache
     is never equal to 0 --  it always contains RAND_TABLE_BIT,
     see get_initial_pseudo_tables().
    */
    owner->was_null|= (!cached_value);
    DBUG_PRINT("info", ("cached: %ld", (long) cached_value));
    DBUG_RETURN(cached_value);
  }
  if (args[0]->is_null())
  {
    DBUG_PRINT("info", ("null"));
    owner->was_null|= 1;
    DBUG_RETURN(0);
  }
  else
    DBUG_RETURN(1);
}

/**
  Optimize case of not_null_column IS NULL.
*/
void Item_is_not_null_test::update_used_tables()
{
  const table_map initial_pseudo_tables= get_initial_pseudo_tables();
  used_tables_cache= initial_pseudo_tables;
  if (!args[0]->maybe_null)
  {
    cached_value= 1;
    return;
  }
  args[0]->update_used_tables();
  with_subselect= args[0]->has_subquery();
  with_stored_program= args[0]->has_stored_program();
  used_tables_cache|= args[0]->used_tables();
  if (used_tables_cache == initial_pseudo_tables && !with_subselect &&
      !with_stored_program)
    /* Remember if the value is always NULL or never NULL */
    cached_value= !args[0]->is_null();
}


longlong Item_func_isnotnull::val_int()
{
  DBUG_ASSERT(fixed == 1);
  return args[0]->is_null() ? 0 : 1;
}


void Item_func_isnotnull::print(String *str, enum_query_type query_type)
{
  str->append('(');
  args[0]->print(str, query_type);
  str->append(STRING_WITH_LEN(" is not null)"));
}


longlong Item_func_like::val_int()
{
  DBUG_ASSERT(fixed == 1);
  String* res = args[0]->val_str(&cmp.value1);
  if (args[0]->null_value)
  {
    null_value=1;
    return 0;
  }
  String* res2 = args[1]->val_str(&cmp.value2);
  if (args[1]->null_value)
  {
    null_value=1;
    return 0;
  }
  null_value=0;
  if (canDoTurboBM)
    return turboBM_matches(res->ptr(), res->length()) ? 1 : 0;
  return my_wildcmp(cmp.cmp_collation.collation,
		    res->ptr(),res->ptr()+res->length(),
		    res2->ptr(),res2->ptr()+res2->length(),
		    escape,wild_one,wild_many) ? 0 : 1;
}


/**
  We can optimize a where if first character isn't a wildcard
*/

Item_func::optimize_type Item_func_like::select_optimize() const
{
  if (!args[1]->const_item())
    return OPTIMIZE_NONE;

  String* res2= args[1]->val_str((String *)&cmp.value2);
  if (!res2)
    return OPTIMIZE_NONE;

  if (!res2->length()) // Can optimize empty wildcard: column LIKE ''
    return OPTIMIZE_OP;

  DBUG_ASSERT(res2->ptr());
  char first= res2->ptr()[0];
  return (first == wild_many || first == wild_one) ?
    OPTIMIZE_NONE : OPTIMIZE_OP;
}


bool Item_func_like::fix_fields(THD *thd, Item **ref)
{
  DBUG_ASSERT(fixed == 0);
  if (Item_bool_func2::fix_fields(thd, ref) ||
      escape_item->fix_fields(thd, &escape_item))
    return TRUE;

  if (!escape_item->const_during_execution())
  {
    my_error(ER_WRONG_ARGUMENTS,MYF(0),"ESCAPE");
    return TRUE;
  }
  
  if (escape_item->const_item())
  {
    /* If we are on execution stage */
    String *escape_str= escape_item->val_str(&cmp.value1);
    if (escape_str)
    {
      const char *escape_str_ptr= escape_str->ptr();
      if (escape_used_in_parsing && (
             (((thd->variables.sql_mode & MODE_NO_BACKSLASH_ESCAPES) &&
                escape_str->numchars() != 1) ||
               escape_str->numchars() > 1)))
      {
        my_error(ER_WRONG_ARGUMENTS,MYF(0),"ESCAPE");
        return TRUE;
      }

      if (use_mb(cmp.cmp_collation.collation))
      {
        const CHARSET_INFO *cs= escape_str->charset();
        my_wc_t wc;
        int rc= cs->cset->mb_wc(cs, &wc,
                                (const uchar*) escape_str_ptr,
                                (const uchar*) escape_str_ptr +
                                escape_str->length());
        escape= (int) (rc > 0 ? wc : '\\');
      }
      else
      {
        /*
          In the case of 8bit character set, we pass native
          code instead of Unicode code as "escape" argument.
          Convert to "cs" if charset of escape differs.
        */
        const CHARSET_INFO *cs= cmp.cmp_collation.collation;
        uint32 unused;
        if (escape_str->needs_conversion(escape_str->length(),
                                         escape_str->charset(), cs, &unused))
        {
          char ch;
          uint errors;
          uint32 cnvlen= copy_and_convert(&ch, 1, cs, escape_str_ptr,
                                          escape_str->length(),
                                          escape_str->charset(), &errors);
          escape= cnvlen ? ch : '\\';
        }
        else
          escape= escape_str_ptr ? *escape_str_ptr : '\\';
      }
    }
    else
      escape= '\\';

    /*
      We could also do boyer-more for non-const items, but as we would have to
      recompute the tables for each row it's not worth it.
    */
    if (args[1]->const_item() && !use_strnxfrm(collation.collation) &&
       !(specialflag & SPECIAL_NO_NEW_FUNC))
    {
      String* res2 = args[1]->val_str(&cmp.value2);
      if (!res2)
        return FALSE;				// Null argument
      
      const size_t len   = res2->length();
      const char*  first = res2->ptr();
      const char*  last  = first + len - 1;
      /*
        len must be > 2 ('%pattern%')
        heuristic: only do TurboBM for pattern_len > 2
      */
      
      if (len > MIN_TURBOBM_PATTERN_LEN + 2 &&
          *first == wild_many &&
          *last  == wild_many)
      {
        const char* tmp = first + 1;
        for (; *tmp != wild_many && *tmp != wild_one && *tmp != escape; tmp++) ;
        canDoTurboBM = (tmp == last) && !use_mb(args[0]->collation.collation);
      }
      if (canDoTurboBM)
      {
        pattern_len = (int) len - 2;
        pattern     = thd->strmake(first + 1, pattern_len);
        DBUG_PRINT("info", ("Initializing pattern: '%s'", first));
        int *suff = (int*) thd->alloc((int) (sizeof(int)*
                                      ((pattern_len + 1)*2+
                                      alphabet_size)));
        bmGs      = suff + pattern_len + 1;
        bmBc      = bmGs + pattern_len + 1;
        turboBM_compute_good_suffix_shifts(suff);
        turboBM_compute_bad_character_shifts();
        DBUG_PRINT("info",("done"));
      }
    }
  }
  return FALSE;
}

void Item_func_like::cleanup()
{
  canDoTurboBM= FALSE;
  Item_bool_func2::cleanup();
}

/**
  @brief Compile regular expression.

  @param[in]    send_error     send error message if any.

  @details Make necessary character set conversion then 
  compile regular expression passed in the args[1].

  @retval    0     success.
  @retval    1     error occurred.
  @retval   -1     given null regular expression.
 */

int Item_func_regex::regcomp(bool send_error)
{
  char buff[MAX_FIELD_WIDTH];
  String tmp(buff,sizeof(buff),&my_charset_bin);
  String *res= args[1]->val_str(&tmp);
  int error;

  if (args[1]->null_value)
    return -1;

  if (regex_compiled)
  {
    if (!stringcmp(res, &prev_regexp))
      return 0;
    prev_regexp.copy(*res);
    my_regfree(&preg);
    regex_compiled= 0;
  }

  if (cmp_collation.collation != regex_lib_charset)
  {
    /* Convert UCS2 strings to UTF8 */
    uint dummy_errors;
    if (conv.copy(res->ptr(), res->length(), res->charset(),
                  regex_lib_charset, &dummy_errors))
      return 1;
    res= &conv;
  }

  if ((error= my_regcomp(&preg, res->c_ptr_safe(),
                         regex_lib_flags, regex_lib_charset)))
  {
    if (send_error)
    {
      (void) my_regerror(error, &preg, buff, sizeof(buff));
      my_error(ER_REGEXP_ERROR, MYF(0), buff);
    }
    return 1;
  }
  regex_compiled= 1;
  return 0;
}


bool
Item_func_regex::fix_fields(THD *thd, Item **ref)
{
  DBUG_ASSERT(fixed == 0);
  if ((!args[0]->fixed &&
       args[0]->fix_fields(thd, args)) || args[0]->check_cols(1) ||
      (!args[1]->fixed &&
       args[1]->fix_fields(thd, args + 1)) || args[1]->check_cols(1))
    return TRUE;				/* purecov: inspected */
  with_sum_func=args[0]->with_sum_func || args[1]->with_sum_func;
  with_subselect= args[0]->has_subquery() || args[1]->has_subquery();
  with_stored_program= args[0]->has_stored_program() ||
                       args[1]->has_stored_program();
  max_length= 1;
  decimals= 0;

  if (agg_arg_charsets_for_comparison(cmp_collation, args, 2))
    return TRUE;

  regex_lib_flags= (cmp_collation.collation->state &
                    (MY_CS_BINSORT | MY_CS_CSSORT)) ?
                   MY_REG_EXTENDED | MY_REG_NOSUB :
                   MY_REG_EXTENDED | MY_REG_NOSUB | MY_REG_ICASE;
  /*
    If the case of UCS2 and other non-ASCII character sets,
    we will convert patterns and strings to UTF8.
  */
  regex_lib_charset= (cmp_collation.collation->mbminlen > 1) ?
                     &my_charset_utf8_general_ci :
                     cmp_collation.collation;

  used_tables_cache=args[0]->used_tables() | args[1]->used_tables();
  not_null_tables_cache= (args[0]->not_null_tables() |
			  args[1]->not_null_tables());
  const_item_cache=args[0]->const_item() && args[1]->const_item();
  if (!regex_compiled && args[1]->const_item())
  {
    int comp_res= regcomp(TRUE);
    if (comp_res == -1)
    {						// Will always return NULL
      maybe_null=1;
      fixed= 1;
      return FALSE;
    }
    else if (comp_res)
      return TRUE;
    regex_is_const= 1;
    maybe_null= args[0]->maybe_null;
  }
  else
    maybe_null=1;
  fixed= 1;
  return FALSE;
}


longlong Item_func_regex::val_int()
{
  DBUG_ASSERT(fixed == 1);
  char buff[MAX_FIELD_WIDTH];
  String tmp(buff,sizeof(buff),&my_charset_bin);
  String *res= args[0]->val_str(&tmp);

  if ((null_value= (args[0]->null_value ||
                    (!regex_is_const && regcomp(FALSE)))))
    return 0;

  if (cmp_collation.collation != regex_lib_charset)
  {
    /* Convert UCS2 strings to UTF8 */
    uint dummy_errors;
    if (conv.copy(res->ptr(), res->length(), res->charset(),
                  regex_lib_charset, &dummy_errors))
    {
      null_value= 1;
      return 0;
    }
    res= &conv;
  }
  return my_regexec(&preg,res->c_ptr_safe(),0,(my_regmatch_t*) 0,0) ? 0 : 1;
}


void Item_func_regex::cleanup()
{
  DBUG_ENTER("Item_func_regex::cleanup");
  Item_bool_func::cleanup();
  if (regex_compiled)
  {
    my_regfree(&preg);
    regex_compiled=0;
    prev_regexp.length(0);
  }
  DBUG_VOID_RETURN;
}


#ifdef LIKE_CMP_TOUPPER
#define likeconv(cs,A) (uchar) (cs)->toupper(A)
#else
#define likeconv(cs,A) (uchar) (cs)->sort_order[(uchar) (A)]
#endif


/**
  Precomputation dependent only on pattern_len.
*/

void Item_func_like::turboBM_compute_suffixes(int *suff)
{
  const int   plm1 = pattern_len - 1;
  int            f = 0;
  int            g = plm1;
  int *const splm1 = suff + plm1;
  const CHARSET_INFO	*cs= cmp.cmp_collation.collation;

  *splm1 = pattern_len;

  if (!cs->sort_order)
  {
    int i;
    for (i = pattern_len - 2; i >= 0; i--)
    {
      int tmp = *(splm1 + i - f);
      if (g < i && tmp < i - g)
	suff[i] = tmp;
      else
      {
	if (i < g)
	  g = i; // g = min(i, g)
	f = i;
	while (g >= 0 && pattern[g] == pattern[g + plm1 - f])
	  g--;
	suff[i] = f - g;
      }
    }
  }
  else
  {
    int i;
    for (i = pattern_len - 2; 0 <= i; --i)
    {
      int tmp = *(splm1 + i - f);
      if (g < i && tmp < i - g)
	suff[i] = tmp;
      else
      {
	if (i < g)
	  g = i; // g = min(i, g)
	f = i;
	while (g >= 0 &&
	       likeconv(cs, pattern[g]) == likeconv(cs, pattern[g + plm1 - f]))
	  g--;
	suff[i] = f - g;
      }
    }
  }
}


/**
  Precomputation dependent only on pattern_len.
*/

void Item_func_like::turboBM_compute_good_suffix_shifts(int *suff)
{
  turboBM_compute_suffixes(suff);

  int *end = bmGs + pattern_len;
  int *k;
  for (k = bmGs; k < end; k++)
    *k = pattern_len;

  int tmp;
  int i;
  int j          = 0;
  const int plm1 = pattern_len - 1;
  for (i = plm1; i > -1; i--)
  {
    if (suff[i] == i + 1)
    {
      for (tmp = plm1 - i; j < tmp; j++)
      {
	int *tmp2 = bmGs + j;
	if (*tmp2 == pattern_len)
	  *tmp2 = tmp;
      }
    }
  }

  int *tmp2;
  for (tmp = plm1 - i; j < tmp; j++)
  {
    tmp2 = bmGs + j;
    if (*tmp2 == pattern_len)
      *tmp2 = tmp;
  }

  tmp2 = bmGs + plm1;
  for (i = 0; i <= pattern_len - 2; i++)
    *(tmp2 - suff[i]) = plm1 - i;
}


/**
   Precomputation dependent on pattern_len.
*/

void Item_func_like::turboBM_compute_bad_character_shifts()
{
  int *i;
  int *end = bmBc + alphabet_size;
  int j;
  const int plm1 = pattern_len - 1;
  const CHARSET_INFO	*cs= cmp.cmp_collation.collation;

  for (i = bmBc; i < end; i++)
    *i = pattern_len;

  if (!cs->sort_order)
  {
    for (j = 0; j < plm1; j++)
      bmBc[(uint) (uchar) pattern[j]] = plm1 - j;
  }
  else
  {
    for (j = 0; j < plm1; j++)
      bmBc[(uint) likeconv(cs,pattern[j])] = plm1 - j;
  }
}


/**
  Search for pattern in text.

  @return
    returns true/false for match/no match
*/

bool Item_func_like::turboBM_matches(const char* text, int text_len) const
{
  int bcShift;
  int turboShift;
  int shift = pattern_len;
  int j     = 0;
  int u     = 0;
  const CHARSET_INFO	*cs= cmp.cmp_collation.collation;

  const int plm1=  pattern_len - 1;
  const int tlmpl= text_len - pattern_len;

  /* Searching */
  if (!cs->sort_order)
  {
    while (j <= tlmpl)
    {
      int i= plm1;
      while (i >= 0 && pattern[i] == text[i + j])
      {
	i--;
	if (i == plm1 - shift)
	  i-= u;
      }
      if (i < 0)
	return 1;

      const int v = plm1 - i;
      turboShift = u - v;
      bcShift    = bmBc[(uint) (uchar) text[i + j]] - plm1 + i;
      shift      = max(turboShift, bcShift);
      shift      = max(shift, bmGs[i]);
      if (shift == bmGs[i])
	u = min(pattern_len - shift, v);
      else
      {
	if (turboShift < bcShift)
	  shift = max(shift, u + 1);
	u = 0;
      }
      j+= shift;
    }
    return 0;
  }
  else
  {
    while (j <= tlmpl)
    {
      int i = plm1;
      while (i >= 0 && likeconv(cs,pattern[i]) == likeconv(cs,text[i + j]))
      {
	i--;
	if (i == plm1 - shift)
	  i-= u;
      }
      if (i < 0)
	return 1;

      const int v = plm1 - i;
      turboShift = u - v;
      bcShift    = bmBc[(uint) likeconv(cs, text[i + j])] - plm1 + i;
      shift      = max(turboShift, bcShift);
      shift      = max(shift, bmGs[i]);
      if (shift == bmGs[i])
	u = min(pattern_len - shift, v);
      else
      {
	if (turboShift < bcShift)
	  shift = max(shift, u + 1);
	u = 0;
      }
      j+= shift;
    }
    return 0;
  }
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

longlong Item_func_xor::val_int()
{
  DBUG_ASSERT(fixed == 1);
  uint result= 0;
  null_value= false;
  for (uint i= 0; i < arg_count; i++)
  {
    result^= (args[i]->val_bool() != 0);
    if (args[i]->null_value)
    {
      null_value= true;
      return 0;
    }
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
    @endverbatim

  @param thd		thread handler

  @return
    New item or
    NULL if we cannot apply NOT transformation (see Item::neg_transformer()).
*/

Item *Item_func_not::neg_transformer(THD *thd)	/* NOT(x)  ->  x */
{
  return args[0];
}


Item *Item_bool_rowready_func2::neg_transformer(THD *thd)
{
  Item *item= negated_item();
  return item;
}

/**
  XOR can be negated by negating one of the operands:

  NOT (a XOR b)  => (NOT a) XOR b
                 => a       XOR (NOT b)

  @param thd     Thread handle
  @return        New negated item
*/
Item *Item_func_xor::neg_transformer(THD *thd)
{
  Item *neg_operand;
  Item_func_xor *new_item;
  if ((neg_operand= args[0]->neg_transformer(thd)))
    // args[0] has neg_tranformer
    new_item= new(thd->mem_root) Item_func_xor(neg_operand, args[1]);
  else if ((neg_operand= args[1]->neg_transformer(thd)))
    // args[1] has neg_tranformer
    new_item= new(thd->mem_root) Item_func_xor(args[0], neg_operand);
  else
  {
    neg_operand= new(thd->mem_root) Item_func_not(args[0]);
    new_item= new(thd->mem_root) Item_func_xor(neg_operand, args[1]);
  }
  return new_item;
}


/**
  a IS NULL  ->  a IS NOT NULL.
*/
Item *Item_func_isnull::neg_transformer(THD *thd)
{
  Item *item= new Item_func_isnotnull(args[0]);
  return item;
}


/**
  a IS NOT NULL  ->  a IS NULL.
*/
Item *Item_func_isnotnull::neg_transformer(THD *thd)
{
  Item *item= new Item_func_isnull(args[0]);
  return item;
}


Item *Item_cond_and::neg_transformer(THD *thd)	/* NOT(a AND b AND ...)  -> */
					/* NOT a OR NOT b OR ... */
{
  neg_arguments(thd);
  Item *item= new Item_cond_or(list);
  return item;
}


Item *Item_cond_or::neg_transformer(THD *thd)	/* NOT(a OR b OR ...)  -> */
					/* NOT a AND NOT b AND ... */
{
  neg_arguments(thd);
  Item *item= new Item_cond_and(list);
  return item;
}


Item *Item_func_nop_all::neg_transformer(THD *thd)
{
  /* "NOT (e $cmp$ ANY (SELECT ...)) -> e $rev_cmp$" ALL (SELECT ...) */
  Item_func_not_all *new_item= new Item_func_not_all(args[0]);
  Item_allany_subselect *allany= (Item_allany_subselect*)args[0];
  allany->func= allany->func_creator(FALSE);
  allany->all= !allany->all;
  allany->upper_item= new_item;
  return new_item;
}

Item *Item_func_not_all::neg_transformer(THD *thd)
{
  /* "NOT (e $cmp$ ALL (SELECT ...)) -> e $rev_cmp$" ANY (SELECT ...) */
  Item_func_nop_all *new_item= new Item_func_nop_all(args[0]);
  Item_allany_subselect *allany= (Item_allany_subselect*)args[0];
  allany->all= !allany->all;
  allany->func= allany->func_creator(TRUE);
  allany->upper_item= new_item;
  return new_item;
}

Item *Item_func_eq::negated_item()		/* a = b  ->  a != b */
{
  return new Item_func_ne(args[0], args[1]);
}


Item *Item_func_ne::negated_item()		/* a != b  ->  a = b */
{
  return new Item_func_eq(args[0], args[1]);
}


Item *Item_func_lt::negated_item()		/* a < b  ->  a >= b */
{
  return new Item_func_ge(args[0], args[1]);
}


Item *Item_func_ge::negated_item()		/* a >= b  ->  a < b */
{
  return new Item_func_lt(args[0], args[1]);
}


Item *Item_func_gt::negated_item()		/* a > b  ->  a <= b */
{
  return new Item_func_le(args[0], args[1]);
}


Item *Item_func_le::negated_item()		/* a <= b  ->  a > b */
{
  return new Item_func_gt(args[0], args[1]);
}

/**
  just fake method, should never be called.
*/
Item *Item_bool_rowready_func2::negated_item()
{
  DBUG_ASSERT(0);
  return 0;
}

Item_equal::Item_equal(Item_field *f1, Item_field *f2)
  : Item_bool_func(), const_item(0), eval_item(0), cond_false(0),
    compare_as_dates(FALSE)
{
  const_item_cache= 0;
  fields.push_back(f1);
  fields.push_back(f2);
}

Item_equal::Item_equal(Item *c, Item_field *f)
  : Item_bool_func(), eval_item(0), cond_false(0)
{
  const_item_cache= 0;
  fields.push_back(f);
  const_item= c;
  compare_as_dates= f->is_temporal_with_date();
}


Item_equal::Item_equal(Item_equal *item_equal)
  : Item_bool_func(), eval_item(0), cond_false(0)
{
  const_item_cache= 0;
  List_iterator_fast<Item_field> li(item_equal->fields);
  Item_field *item;
  while ((item= li++))
  {
    fields.push_back(item);
  }
  const_item= item_equal->const_item;
  compare_as_dates= item_equal->compare_as_dates;
  cond_false= item_equal->cond_false;
}


void Item_equal::compare_const(Item *c)
{
  if (compare_as_dates)
  {
    cmp.set_datetime_cmp_func(this, &c, &const_item);
    cond_false= cmp.compare();
  }
  else
  {
    Item_func_eq *func= new Item_func_eq(c, const_item);
    if(func->set_cmp_func())
      return;
    func->quick_fix_field();
    cond_false= !func->val_int();
  }
  if (cond_false)
    const_item_cache= 1;
}


void Item_equal::add(Item *c, Item_field *f)
{
  if (cond_false)
    return;
  if (!const_item)
  {
    DBUG_ASSERT(f);
    const_item= c;
    compare_as_dates= f->is_temporal_with_date();
    return;
  }
  compare_const(c);
}


void Item_equal::add(Item *c)
{
  if (cond_false)
    return;
  if (!const_item)
  {
    const_item= c;
    return;
  }
  compare_const(c);
}

void Item_equal::add(Item_field *f)
{
  fields.push_back(f);
}

uint Item_equal::members()
{
  return fields.elements;
}


/**
  Check whether a field is referred in the multiple equality.

  The function checks whether field is occurred in the Item_equal object .

  @param field   field whose occurrence is to be checked

  @retval
    1       if nultiple equality contains a reference to field
  @retval
    0       otherwise    
*/

bool Item_equal::contains(Field *field)
{
  List_iterator_fast<Item_field> it(fields);
  Item_field *item;
  while ((item= it++))
  {
    if (field->eq(item->field))
        return 1;
  }
  return 0;
}


/**
  Join members of another Item_equal object.
  
    The function actually merges two multiple equalities.
    After this operation the Item_equal object additionally contains
    the field items of another item of the type Item_equal.
    If the optional constant items are not equal the cond_false flag is
    set to 1.  
  @param item    multiple equality whose members are to be joined
*/

void Item_equal::merge(Item_equal *item)
{
  fields.concat(&item->fields);
  Item *c= item->const_item;
  if (c)
  {
    /* 
      The flag cond_false will be set to 1 after this, if 
      the multiple equality already contains a constant and its 
      value is  not equal to the value of c.
    */
    add(c);
  }
  cond_false|= item->cond_false;
} 


/**
  Order field items in multiple equality according to a sorting criteria.

  The function perform ordering of the field items in the Item_equal
  object according to the criteria determined by the cmp callback parameter.
  If cmp(item_field1,item_field2,arg)<0 than item_field1 must be
  placed after item_fiel2.

  The function sorts field items by the exchange sort algorithm.
  The list of field items is looked through and whenever two neighboring
  members follow in a wrong order they are swapped. This is performed
  again and again until we get all members in a right order.

  @param compare      function to compare field item
  @param arg          context extra parameter for the cmp function
*/

void Item_equal::sort(Item_field_cmpfunc compare, void *arg)
{
  fields.sort((Node_cmp_func)compare, arg);
}


/**
  Check appearance of new constant items in the multiple equality object.

  The function checks appearance of new constant items among
  the members of multiple equalities. Each new constant item is
  compared with the designated constant item if there is any in the
  multiple equality. If there is none the first new constant item
  becomes designated.
*/

void Item_equal::update_const()
{
  List_iterator<Item_field> it(fields);
  Item *item;
  while ((item= it++))
  {
    if (item->const_item() &&
        /*
          Don't propagate constant status of outer-joined column.
          Such a constant status here is a result of:
            a) empty outer-joined table: in this case such a column has a
               value of NULL; but at the same time other arguments of
               Item_equal don't have to be NULLs and the value of the whole
               multiple equivalence expression doesn't have to be NULL or FALSE
               because of the outer join nature;
          or
            b) outer-joined table contains only 1 row: the result of
               this column is equal to a row field value *or* NULL.
          Both values are inacceptable as Item_equal constants.
        */
        !item->is_outer_field())
    {
      it.remove();
      add(item);
    }
  }
}

bool Item_equal::fix_fields(THD *thd, Item **ref)
{
  List_iterator_fast<Item_field> li(fields);
  Item *item;
  not_null_tables_cache= used_tables_cache= 0;
  const_item_cache= 0;
  while ((item= li++))
  {
    table_map tmp_table_map;
    used_tables_cache|= item->used_tables();
    tmp_table_map= item->not_null_tables();
    not_null_tables_cache|= tmp_table_map;
    if (item->maybe_null)
      maybe_null=1;
  }
  fix_length_and_dec();
  fixed= 1;
  return 0;
}

void Item_equal::update_used_tables()
{
  List_iterator_fast<Item_field> li(fields);
  Item *item;
  not_null_tables_cache= used_tables_cache= 0;
  if ((const_item_cache= cond_false))
    return;
  with_subselect= false;
  with_stored_program= false;
  while ((item=li++))
  {
    item->update_used_tables();
    used_tables_cache|= item->used_tables();
    /* see commentary at Item_equal::update_const() */
    const_item_cache&= item->const_item() && !item->is_outer_field();
    with_subselect|= item->has_subquery();
    with_stored_program|= item->has_stored_program();
  }
}

longlong Item_equal::val_int()
{
  Item_field *item_field;
  if (cond_false)
    return 0;
  List_iterator_fast<Item_field> it(fields);
  Item *item= const_item ? const_item : it++;
  eval_item->store_value(item);
  if ((null_value= item->null_value))
    return 0;
  while ((item_field= it++))
  {
    /* Skip fields of non-const tables. They haven't been read yet */
    if (item_field->field->table->const_table)
    {
      if (eval_item->cmp(item_field) || (null_value= item_field->null_value))
        return 0;
    }
  }
  return 1;
}

void Item_equal::fix_length_and_dec()
{
  Item *item= get_first();
  eval_item= cmp_item::get_comparator(item->result_type(),
                                      item->collation.collation);
}

bool Item_equal::walk(Item_processor processor, bool walk_subquery, uchar *arg)
{
  List_iterator_fast<Item_field> it(fields);
  Item *item;
  while ((item= it++))
  {
    if (item->walk(processor, walk_subquery, arg))
      return 1;
  }
  return Item_func::walk(processor, walk_subquery, arg);
}

Item *Item_equal::transform(Item_transformer transformer, uchar *arg)
{
  DBUG_ASSERT(!current_thd->stmt_arena->is_stmt_prepare());

  List_iterator<Item_field> it(fields);
  Item *item;
  while ((item= it++))
  {
    Item *new_item= item->transform(transformer, arg);
    if (!new_item)
      return 0;

    /*
      THD::change_item_tree() should be called only if the tree was
      really transformed, i.e. when a new item has been created.
      Otherwise we'll be allocating a lot of unnecessary memory for
      change records at each execution.
    */
    if (new_item != item)
      current_thd->change_item_tree((Item **) it.ref(), new_item);
  }
  return Item_func::transform(transformer, arg);
}

void Item_equal::print(String *str, enum_query_type query_type)
{
  str->append(func_name());
  str->append('(');
  List_iterator_fast<Item_field> it(fields);
  Item *item;
  if (const_item)
    const_item->print(str, query_type);
  else
  {
    item= it++;
    item->print(str, query_type);
  }
  while ((item= it++))
  {
    str->append(',');
    str->append(' ');
    item->print(str, query_type);
  }
  str->append(')');
}


void Item_func_trig_cond::print(String *str, enum_query_type query_type)
{
  /*
    Print:
    <if>(<property><(optional list of source tables)>, condition, TRUE)
    which means: if a certain property (<property>) is true, then return
    the value of <condition>, else return TRUE. If source tables are
    present, they are the owner of the property.
  */
  str->append(func_name());
  str->append("(");
  switch(trig_type)
  {
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
    DBUG_ASSERT(0);
  }
  if (trig_tab != NULL)
  {
    str->append("(");
    str->append(trig_tab->table->alias);
    if (trig_tab->last_inner != trig_tab)
    {
      /* case of t1 LEFT JOIN (t2,t3,...): print range of inner tables */
      str->append("..");
      str->append(trig_tab->last_inner->table->alias);
    }
    str->append(")");
  }
  str->append(", ");
  args[0]->print(str, query_type);
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

Item_field* Item_equal::get_subst_item(const Item_field *field)
{
  DBUG_ASSERT(field != NULL);

  const JOIN_TAB *field_tab= field->field->table->reginfo.join_tab;

  /*
    field_tab is NULL if this function was not called from
    JOIN::optimize() but from e.g. mysql_delete() or mysql_update().
    In these cases there is only one table and no semijoin
  */
  if (field_tab &&
      sj_is_materialize_strategy(field_tab->get_sj_strategy()))
  {
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
      never Item_field, so they can be part of an Item_equal only if they are
      constant (in which case there is no problem with choosing them below);
      @see check_simple_equality().
    */
    List_iterator<Item_field> it(fields);
    Item_field *item;
    const JOIN_TAB *first= field_tab->first_sj_inner_tab;
    const JOIN_TAB *last=  field_tab->last_sj_inner_tab;

    while ((item= it++))
    {
      if (item->field->table->reginfo.join_tab >= first &&
          item->field->table->reginfo.join_tab <= last)
      {
        return item;
      }
    }
  }
  else
  {
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
  DBUG_ASSERT(FALSE);                          // Should never get here.
  return NULL;
}

/**
  Transform an Item_equal object after having added a table that
  represents a materialized semi-join.

  @details
    If the multiple equality represented by the Item_equal object contains
    a field from the subquery that was used to create the materialized table,
    add the corresponding key field from the materialized table to the
    multiple equality.
    @see JOIN::update_equalities_for_sjm() for the reason.
*/

Item* Item_equal::equality_substitution_transformer(uchar *arg)
{
  TABLE_LIST *sj_nest= reinterpret_cast<TABLE_LIST *>(arg);
  List_iterator<Item_field> it(fields);
  List<Item_field> added_fields;
  Item_field *item;
  // Iterate over the fields in the multiple equality
  while ((item= it++))
  {
    // Skip fields that do not come from materialized subqueries
    const JOIN_TAB *tab= item->field->table->reginfo.join_tab;
    if (!tab || !sj_is_materialize_strategy(tab->get_sj_strategy()))
      continue;

    // Iterate over the fields selected from the subquery
    List_iterator<Item> mit(sj_nest->nested_join->sj_inner_exprs);
    Item *existing;
    uint fieldno= 0;
    while ((existing= mit++))
    {
      if (existing->real_item()->eq(item, false))
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
Item* Item_func_eq::equality_substitution_transformer(uchar *arg)
{
  TABLE_LIST *sj_nest= reinterpret_cast<TABLE_LIST *>(arg);

  // Iterate over the fields selected from the subquery
  List_iterator<Item> mit(sj_nest->nested_join->sj_inner_exprs);
  Item *existing;
  uint fieldno= 0;
  while ((existing= mit++))
  {
    if (existing->real_item()->eq(args[1], false) &&
        (args[0]->used_tables() & ~sj_nest->sj_inner_tables))
      current_thd->change_item_tree(args+1,
                                 sj_nest->nested_join->sjm.mat_fields[fieldno]);
    fieldno++;
  }
  return this;
}
