/* Copyright (C) 2000-2003 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


/* This file defines all compare functions */

#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#include <m_ctype.h>
#include "sql_select.h"

static bool convert_constant_item(THD *thd, Field *field, Item **item);

static Item_result item_store_type(Item_result a,Item_result b)
{
  if (a == STRING_RESULT || b == STRING_RESULT)
    return STRING_RESULT;
  else if (a == REAL_RESULT || b == REAL_RESULT)
    return REAL_RESULT;
  else if (a == DECIMAL_RESULT || b == DECIMAL_RESULT)
    return DECIMAL_RESULT;
  else
    return INT_RESULT;
}

static void agg_result_type(Item_result *type, Item **items, uint nitems)
{
  Item **item, **item_end;

  *type= STRING_RESULT;
  /* Skip beginning NULL items */
  for (item= items, item_end= item + nitems; item < item_end; item++)
  {
    if ((*item)->type() != Item::NULL_ITEM)
    {
      *type= (*item)->result_type();
      item++;
      break;
    }
  }
  /* Combine result types. Note: NULL items don't affect the result */
  for (; item < item_end; item++)
  {
    if ((*item)->type() != Item::NULL_ITEM)
      *type= item_store_type(type[0], (*item)->result_type());
  }
}


/*
  Aggregates result types from the array of items.

  SYNOPSIS:
    agg_cmp_type()
    thd          thread handle
    type   [out] the aggregated type
    items        array of items to aggregate the type from
    nitems       number of items in the array

  DESCRIPTION
    This function aggregates result types from the array of items. Found type
    supposed to be used later for comparison of values of these items.
    Aggregation itself is performed by the item_cmp_type() function.

  NOTES
    Aggregation rules:
    If there are DATE/TIME fields/functions in the list and no string
    fields/functions in the list then:
      The INT_RESULT type will be used for aggregation instead of original
      result type of any DATE/TIME field/function in the list
      All constant items in the list will be converted to a DATE/TIME using
      found field or result field of found function.

    Implementation notes:
      The code is equivalent to:
      1. Check the list for presence of a STRING field/function.
         Collect the is_const flag.
      2. Get a Field* object to use for type coercion
      3. Perform type conversion.
      1 and 2 are implemented in 2 loops. The first searches for a DATE/TIME
      field/function and checks presence of a STRING field/function.
      The second loop works only if a DATE/TIME field/function is found.
      It checks presence of a STRING field/function in the rest of the list.

  TODO
    1) The current implementation can produce false comparison results for
    expressions like:
      date_time_field BETWEEN string_field_with_dates AND string_constant
    if the string_constant will omit some of leading zeroes.
    In order to fully implement correct comparison of DATE/TIME the new
    DATETIME_RESULT result type should be introduced and agg_cmp_type()
    should return the DATE/TIME field used for the conversion. Later
    this field can be used by comparison functions like Item_func_between to
    convert string values to ints on the fly and thus return correct results.
    This modification will affect functions BETWEEN, IN and CASE.

    2) If in the list a DATE field/function and a DATETIME field/function
    are present in the list then the first found field/function will be
    used for conversion. This may lead to wrong results and probably should
    be fixed.
*/

static void agg_cmp_type(THD *thd, Item_result *type, Item **items, uint nitems)
{
  uint i;
  Item::Type res= (Item::Type)0;
  /* Used only for date/time fields, max_length = 19 */
  char buff[20];
  uchar null_byte;
  Field *field= NULL;

  /* Search for date/time fields/functions */
  for (i= 0; i < nitems; i++)
  {
    if (!items[i]->result_as_longlong())
    {
      /* Do not convert anything if a string field/function is present */
      if (!items[i]->const_item() && items[i]->result_type() == STRING_RESULT)
      {
        i= nitems;
        break;
      }
      continue;
    }
    if ((res= items[i]->real_item()->type()) == Item::FIELD_ITEM &&
        items[i]->result_type() != INT_RESULT)
    {
      field= ((Item_field *)items[i]->real_item())->field;
      break;
    }
    else if (res == Item::FUNC_ITEM)
    {
      field= items[i]->tmp_table_field_from_field_type(0, 0);
      if (field)
        field->move_field(buff, &null_byte, 0);
      break;
    }
  }
  if (field)
  {
    /* Check the rest of the list for presence of a string field/function. */
    for (i++ ; i < nitems; i++)
    {
      if (!items[i]->const_item() && items[i]->result_type() == STRING_RESULT &&
          !items[i]->result_as_longlong())
      {
        if (res == Item::FUNC_ITEM)
          delete field;
        field= 0;
        break;
      }
    }
  }
  /*
    If the first item is a date/time function then its result should be
    compared as int
  */
  if (field)
    /* Suppose we are comparing dates */
    type[0]= INT_RESULT;
  else
    type[0]= items[0]->result_type();

  for (i= 0; i < nitems ; i++)
  {
    Item_result result= items[i]->result_type();
    /*
      Use INT_RESULT as result type for DATE/TIME fields/functions and
      for constants successfully converted to DATE/TIME
    */
    if (field &&
         ((!items[i]->const_item() && items[i]->result_as_longlong()) ||
         (items[i]->const_item() && convert_constant_item(thd, field,
                                                          &items[i]))))
      result= INT_RESULT;
    type[0]= item_cmp_type(type[0], result);
  }

  if (res == Item::FUNC_ITEM && field)
    delete field;
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
  special NOT for ALL subquery
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
  return ((test_sum_item && !test_sum_item->any_value()) ||
          (test_sub_item && !test_sub_item->any_value()));
}

void Item_func_not_all::print(String *str)
{
  if (show)
    Item_func::print(str);
  else
    args[0]->print(str);
}


/*
  Special NOP (No OPeration) for ALL subquery it is like  Item_func_not_all
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


/*
  Convert a constant item to an int and replace the original item

  SYNOPSIS
    convert_constant_item()
    thd             thread handle
    field           item will be converted using the type of this field
    item  [in/out]  reference to the item to convert

  DESCRIPTION
    The function converts a constant expression or string to an integer.
    On successful conversion the original item is substituted for the
    result of the item evaluation.
    This is done when comparing DATE/TIME of different formats and
    also when comparing bigint to strings (in which case strings
    are converted to bigints).

  NOTES
    This function is called only at prepare stage.
    As all derived tables are filled only after all derived tables
    are prepared we do not evaluate items with subselects here because
    they can contain derived tables and thus we may attempt to use a
    table that has not been populated yet.

  RESULT VALUES
  0	Can't convert item
  1	Item was replaced with an integer version of the item
*/

static bool convert_constant_item(THD *thd, Field *field, Item **item)
{
  int result= 0;

  if (!(*item)->with_subselect && (*item)->const_item())
  {
    TABLE *table= field->table;
    ulong orig_sql_mode= thd->variables.sql_mode;
    my_bitmap_map *old_write_map;
    my_bitmap_map *old_read_map;

    if (table)
    {
      old_write_map= dbug_tmp_use_all_columns(table, table->write_set);
      old_read_map= dbug_tmp_use_all_columns(table, table->read_set);
    }
    /* For comparison purposes allow invalid dates like 2000-01-32 */
    thd->variables.sql_mode|= MODE_INVALID_DATES;
    if (!(*item)->save_in_field(field, 1) && !((*item)->null_value))
    {
      Item *tmp= new Item_int_with_ref(field->val_int(), *item,
                                       test(field->flags & UNSIGNED_FLAG));
      if (tmp)
        thd->change_item_tree(item, tmp);
      result= 1;					// Item was replaced
    }
    thd->variables.sql_mode= orig_sql_mode;
    if (table)
    {
      dbug_tmp_restore_column_map(table->write_set, old_write_map);
      dbug_tmp_restore_column_map(table->read_set, old_read_map);
    }
  }
  return result;
}


void Item_bool_func2::fix_length_and_dec()
{
  max_length= 1;				     // Function returns 0 or 1
  THD *thd= current_thd;

  /*
    As some compare functions are generated after sql_yacc,
    we have to check for out of memory conditions here
  */
  if (!args[0] || !args[1])
    return;

  /* 
    We allow to convert to Unicode character sets in some cases.
    The conditions when conversion is possible are:
    - arguments A and B have different charsets
    - A wins according to coercibility rules
    - character set of A is superset for character set of B
   
    If all of the above is true, then it's possible to convert
    B into the character set of A, and then compare according
    to the collation of A.
  */

  
  DTCollation coll;
  if (args[0]->result_type() == STRING_RESULT &&
      args[1]->result_type() == STRING_RESULT &&
      agg_arg_charsets(coll, args, 2, MY_COLL_CMP_CONV, 1))
    return;
    
 
  // Make a special case of compare with fields to get nicer DATE comparisons

  if (functype() == LIKE_FUNC)  // Disable conversion in case of LIKE function.
  {
    set_cmp_func();
    return;
  }

  if (!thd->is_context_analysis_only())
  {
    Item *real_item= args[0]->real_item();
    if (real_item->type() == FIELD_ITEM)
    {
      Field *field=((Item_field*) real_item)->field;
      if (field->can_be_compared_as_longlong())
      {
        if (convert_constant_item(thd, field,&args[1]))
        {
          cmp.set_cmp_func(this, tmp_arg, tmp_arg+1,
                           INT_RESULT);		// Works for all types.
          return;
        }
      }
    }
    real_item= args[1]->real_item();
    if (real_item->type() == FIELD_ITEM /* && !real_item->const_item() */)
    {
      Field *field=((Item_field*) real_item)->field;
      if (field->can_be_compared_as_longlong())
      {
        if (convert_constant_item(thd, field,&args[0]))
        {
          cmp.set_cmp_func(this, tmp_arg, tmp_arg+1,
                           INT_RESULT); // Works for all types.
          return;
        }
      }
    }
  }
  set_cmp_func();
}


int Arg_comparator::set_compare_func(Item_bool_func2 *item, Item_result type)
{
  owner= item;
  func= comparator_matrix[type]
                         [test(owner->functype() == Item_func::EQUAL_FUNC)];
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
      if ((*a)->el(i)->cols() != (*b)->el(i)->cols())
      {
	my_error(ER_OPERAND_COLUMNS, MYF(0), (*a)->el(i)->cols());
	return 1;
      }
      comparators[i].set_cmp_func(owner, (*a)->addr(i), (*b)->addr(i));
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
      my_coll_agg_error((*a)->collation, (*b)->collation, owner->func_name());
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
      (*a)->walk(&Item::set_no_const_sub, FALSE, (byte*) 0);
      (*b)->walk(&Item::set_no_const_sub, FALSE, (byte*) 0);
    }
    break;
  }
  case INT_RESULT:
  {
    if (func == &Arg_comparator::compare_int_signed)
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
  case REAL_RESULT:
    break;
  default:
    DBUG_ASSERT(0);
  }
  return 0;
}


int Arg_comparator::compare_string()
{
  String *res1,*res2;
  if ((res1= (*a)->val_str(&owner->tmp_value1)))
  {
    if ((res2= (*b)->val_str(&owner->tmp_value2)))
    {
      owner->null_value= 0;
      return sortcmp(res1,res2,cmp_collation.collation);
    }
  }
  owner->null_value= 1;
  return -1;
}


/*
  Compare strings byte by byte. End spaces are also compared.

  RETURN
   < 0	*a < *b
   0	*b == *b
   > 0	*a > *b
*/

int Arg_comparator::compare_binary_string()
{
  String *res1,*res2;
  if ((res1= (*a)->val_str(&owner->tmp_value1)))
  {
    if ((res2= (*b)->val_str(&owner->tmp_value2)))
    {
      owner->null_value= 0;
      uint res1_length= res1->length();
      uint res2_length= res2->length();
      int cmp= memcmp(res1->ptr(), res2->ptr(), min(res1_length,res2_length));
      return cmp ? cmp : (int) (res1_length - res2_length);
    }
  }
  owner->null_value= 1;
  return -1;
}


/*
  Compare strings, but take into account that NULL == NULL
*/

int Arg_comparator::compare_e_string()
{
  String *res1,*res2;
  res1= (*a)->val_str(&owner->tmp_value1);
  res2= (*b)->val_str(&owner->tmp_value2);
  if (!res1 || !res2)
    return test(res1 == res2);
  return test(sortcmp(res1, res2, cmp_collation.collation) == 0);
}


int Arg_comparator::compare_e_binary_string()
{
  String *res1,*res2;
  res1= (*a)->val_str(&owner->tmp_value1);
  res2= (*b)->val_str(&owner->tmp_value2);
  if (!res1 || !res2)
    return test(res1 == res2);
  return test(stringcmp(res1, res2) == 0);
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
      owner->null_value= 0;
      if (val1 < val2)	return -1;
      if (val1 == val2) return 0;
      return 1;
    }
  }
  owner->null_value= 1;
  return -1;
}

int Arg_comparator::compare_decimal()
{
  my_decimal value1;
  my_decimal *val1= (*a)->val_decimal(&value1);
  if (!(*a)->null_value)
  {
    my_decimal value2;
    my_decimal *val2= (*b)->val_decimal(&value2);
    if (!(*b)->null_value)
    {
      owner->null_value= 0;
      return my_decimal_cmp(val1, val2);
    }
  }
  owner->null_value= 1;
  return -1;
}

int Arg_comparator::compare_e_real()
{
  double val1= (*a)->val_real();
  double val2= (*b)->val_real();
  if ((*a)->null_value || (*b)->null_value)
    return test((*a)->null_value && (*b)->null_value);
  return test(val1 == val2);
}

int Arg_comparator::compare_e_decimal()
{
  my_decimal value1, value2;
  my_decimal *val1= (*a)->val_decimal(&value1);
  my_decimal *val2= (*b)->val_decimal(&value2);
  if ((*a)->null_value || (*b)->null_value)
    return test((*a)->null_value && (*b)->null_value);
  return test(my_decimal_cmp(val1, val2) == 0);
}

int Arg_comparator::compare_int_signed()
{
  longlong val1= (*a)->val_int();
  if (!(*a)->null_value)
  {
    longlong val2= (*b)->val_int();
    if (!(*b)->null_value)
    {
      owner->null_value= 0;
      if (val1 < val2)	return -1;
      if (val1 == val2)   return 0;
      return 1;
    }
  }
  owner->null_value= 1;
  return -1;
}


/*
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
      owner->null_value= 0;
      if (val1 < val2)	return -1;
      if (val1 == val2)   return 0;
      return 1;
    }
  }
  owner->null_value= 1;
  return -1;
}


/*
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
      owner->null_value= 0;
      if (sval1 < 0 || (ulonglong)sval1 < uval2)
        return -1;
      if ((ulonglong)sval1 == uval2)
        return 0;
      return 1;
    }
  }
  owner->null_value= 1;
  return -1;
}


/*
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
  owner->null_value= 1;
  return -1;
}


int Arg_comparator::compare_e_int()
{
  longlong val1= (*a)->val_int();
  longlong val2= (*b)->val_int();
  if ((*a)->null_value || (*b)->null_value)
    return test((*a)->null_value && (*b)->null_value);
  return test(val1 == val2);
}

/*
  Compare unsigned *a with signed *b or signed *a with unsigned *b.
*/
int Arg_comparator::compare_e_int_diff_signedness()
{
  longlong val1= (*a)->val_int();
  longlong val2= (*b)->val_int();
  if ((*a)->null_value || (*b)->null_value)
    return test((*a)->null_value && (*b)->null_value);
  return (val1 >= 0) && test(val1 == val2);
}

int Arg_comparator::compare_row()
{
  int res= 0;
  bool was_null= 0;
  (*a)->bring_value();
  (*b)->bring_value();
  uint n= (*a)->cols();
  for (uint i= 0; i<n; i++)
  {
    res= comparators[i].compare();
    if (owner->null_value)
    {
      // NULL was compared
      if (owner->abort_on_null)
        return -1; // We do not need correct NULL returning
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
      There was NULL(s) in comparison in some parts, but there was not
      explicit difference in other parts, so we have to return NULL
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


bool Item_in_optimizer::fix_left(THD *thd, Item **ref)
{
  if (!args[0]->fixed && args[0]->fix_fields(thd, args) ||
      !cache && !(cache= Item_cache::get_cache(args[0]->result_type())))
    return 1;

  cache->setup(args[0]);
  if (cache->cols() == 1)
  {
    if ((used_tables_cache= args[0]->used_tables()))
      cache->set_used_tables(OUTER_REF_TABLE_BIT);
    else
      cache->set_used_tables(0);
  }
  else
  {
    uint n= cache->cols();
    for (uint i= 0; i < n; i++)
    {
      if (args[0]->el(i)->used_tables())
	((Item_cache *)cache->el(i))->set_used_tables(OUTER_REF_TABLE_BIT);
      else
	((Item_cache *)cache->el(i))->set_used_tables(0);
    }
    used_tables_cache= args[0]->used_tables();
  }
  not_null_tables_cache= args[0]->not_null_tables();
  with_sum_func= args[0]->with_sum_func;
  const_item_cache= args[0]->const_item();
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
  const_item_cache&= args[1]->const_item();
  fixed= 1;
  return FALSE;
}


longlong Item_in_optimizer::val_int()
{
  DBUG_ASSERT(fixed == 1);
  cache->store(args[0]);
  if (cache->null_value)
  {
    null_value= 1;
    return 0;
  }
  bool tmp= args[1]->val_bool_result();
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
  cache->store(args[0]);
  return (null_value= (cache->null_value || args[1]->is_null()));
}


longlong Item_func_eq::val_int()
{
  DBUG_ASSERT(fixed == 1);
  int value= cmp.compare();
  return value == 0 ? 1 : 0;
}


/* Same as Item_func_eq, but NULL = NULL */

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
  String *a=args[0]->val_str(&tmp_value1);
  String *b=args[1]->val_str(&tmp_value2);
  if (!a || !b)
  {
    null_value=1;
    return 0;
  }
  int value= sortcmp(a,b,cmp.cmp_collation.collation);
  null_value=0;
  return !value ? 0 : (value < 0 ? (longlong) -1 : (longlong) 1);
}


void Item_func_interval::fix_length_and_dec()
{
  use_decimal_comparison= (row->el(0)->result_type() == DECIMAL_RESULT) ||
    (row->el(0)->result_type() == INT_RESULT);
  if (row->cols() > 8)
  {
    bool consts=1;

    for (uint i=1 ; consts && i < row->cols() ; i++)
    {
      consts&= row->el(i)->const_item();
    }

    if (consts &&
        (intervals=
          (interval_range*) sql_alloc(sizeof(interval_range)*(row->cols()-1))))
    {
      if (use_decimal_comparison)
      {
        for (uint i=1 ; i < row->cols(); i++)
        {
          Item *el= row->el(i);
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
              range->dec.fix_buffer_pointer();
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
        for (uint i=1 ; i < row->cols(); i++)
        {
          intervals[i-1].dbl= row->el(i)->val_real();
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


/*
  Execute Item_func_interval()

  SYNOPSIS
    Item_func_interval::val_int()

  NOTES
    If we are doing a decimal comparison, we are
    evaluating the first item twice.

  RETURN
    -1 if null value,
    0 if lower than lowest
    1 - arg_count-1 if between args[n] and args[n+1]
    arg_count if higher than biggest argument
*/

longlong Item_func_interval::val_int()
{
  DBUG_ASSERT(fixed == 1);
  double value;
  my_decimal dec_buf, *dec= NULL;
  uint i;

  if (use_decimal_comparison)
  {
    dec= row->el(0)->val_decimal(&dec_buf);
    if (row->el(0)->null_value)
      return -1;
    my_decimal2double(E_DEC_FATAL_ERROR, dec, &value);
  }
  else
  {
    value= row->el(0)->val_real();
    if (row->el(0)->null_value)
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
    Item *el= row->el(i);
    if (use_decimal_comparison &&
        ((el->result_type() == DECIMAL_RESULT) ||
         (el->result_type() == INT_RESULT)))
    {
      my_decimal e_dec_buf, *e_dec= row->el(i)->val_decimal(&e_dec_buf);
      if (my_decimal_cmp(e_dec, dec) > 0)
        return i-1;
    }
    else if (row->el(i)->val_real() > value)
      return i-1;
  }
  return i-1;
}


/*
  Perform context analysis of a BETWEEN item tree

  SYNOPSIS:
    fix_fields()
    thd     reference to the global context of the query thread
    tables  list of all open tables involved in the query
    ref     pointer to Item* variable where pointer to resulting "fixed"
            item is to be assigned

  DESCRIPTION
    This function performs context analysis (name resolution) and calculates
    various attributes of the item tree with Item_func_between as its root.
    The function saves in ref the pointer to the item or to a newly created
    item that is considered as a replacement for the original one.

  NOTES
    Let T0(e)/T1(e) be the value of not_null_tables(e) when e is used on
    a predicate/function level. Then it's easy to show that:
      T0(e BETWEEN e1 AND e2)     = union(T1(e),T1(e1),T1(e2))
      T1(e BETWEEN e1 AND e2)     = union(T1(e),intersection(T1(e1),T1(e2)))
      T0(e NOT BETWEEN e1 AND e2) = union(T1(e),intersection(T1(e1),T1(e2)))
      T1(e NOT BETWEEN e1 AND e2) = union(T1(e),intersection(T1(e1),T1(e2)))

  RETURN
    0   ok
    1   got error
*/

bool Item_func_between::fix_fields(THD *thd, Item **ref)
{
  if (Item_func_opt_neg::fix_fields(thd, ref))
    return 1;

  /* not_null_tables_cache == union(T1(e),T1(e1),T1(e2)) */
  if (pred_level && !negated)
    return 0;

  /* not_null_tables_cache == union(T1(e), intersection(T1(e1),T1(e2))) */
  not_null_tables_cache= (args[0]->not_null_tables() |
                          (args[1]->not_null_tables() &
                           args[2]->not_null_tables()));

  return 0;
}


void Item_func_between::fix_length_and_dec()
{
   max_length= 1;
   THD *thd= current_thd;

  /*
    As some compare functions are generated after sql_yacc,
    we have to check for out of memory conditions here
  */
  if (!args[0] || !args[1] || !args[2])
    return;
  agg_cmp_type(thd, &cmp_type, args, 3);

  if (cmp_type == STRING_RESULT)
      agg_arg_charsets(cmp_collation, args, 3, MY_COLL_CMP_CONV, 1);
}


longlong Item_func_between::val_int()
{						// ANSI BETWEEN
  DBUG_ASSERT(fixed == 1);
  if (cmp_type == STRING_RESULT)
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
    longlong value=args[0]->val_int(), a, b;
    if ((null_value=args[0]->null_value))
      return 0;					/* purecov: inspected */
    a=args[1]->val_int();
    b=args[2]->val_int();
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


void Item_func_between::print(String *str)
{
  str->append('(');
  args[0]->print(str);
  if (negated)
    str->append(STRING_WITH_LEN(" not"));
  str->append(STRING_WITH_LEN(" between "));
  args[1]->print(str);
  str->append(STRING_WITH_LEN(" and "));
  args[2]->print(str);
  str->append(')');
}

void
Item_func_ifnull::fix_length_and_dec()
{
  agg_result_type(&hybrid_type, args, 2);
  maybe_null=args[1]->maybe_null;
  decimals= max(args[0]->decimals, args[1]->decimals);
  max_length= (hybrid_type == DECIMAL_RESULT || hybrid_type == INT_RESULT) ?
    (max(args[0]->max_length - args[0]->decimals,
         args[1]->max_length - args[1]->decimals) + decimals) :
    max(args[0]->max_length, args[1]->max_length);

  switch (hybrid_type) {
  case STRING_RESULT:
    agg_arg_charsets(collation, args, arg_count, MY_COLL_CMP_CONV, 1);
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
  cached_field_type= args[0]->field_type();
  if (cached_field_type != args[1]->field_type())
    cached_field_type= Item_func::field_type();
}


uint Item_func_ifnull::decimal_precision() const
{
  int max_int_part=max(args[0]->decimal_int_part(),args[1]->decimal_int_part());
  return min(max_int_part + decimals, DECIMAL_MAX_PRECISION);
}


enum_field_types Item_func_ifnull::field_type() const 
{
  return cached_field_type;
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


/*
  Perform context analysis of an IF item tree

  SYNOPSIS:
    fix_fields()
    thd     reference to the global context of the query thread
    tables  list of all open tables involved in the query
    ref     pointer to Item* variable where pointer to resulting "fixed"
            item is to be assigned

  DESCRIPTION
    This function performs context analysis (name resolution) and calculates
    various attributes of the item tree with Item_func_if as its root.
    The function saves in ref the pointer to the item or to a newly created
    item that is considered as a replacement for the original one.

  NOTES
    Let T0(e)/T1(e) be the value of not_null_tables(e) when e is used on
    a predicate/function level. Then it's easy to show that:
      T0(IF(e,e1,e2)  = T1(IF(e,e1,e2))
      T1(IF(e,e1,e2)) = intersection(T1(e1),T1(e2))

  RETURN
    0   ok
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


void
Item_func_if::fix_length_and_dec()
{
  maybe_null=args[1]->maybe_null || args[2]->maybe_null;
  decimals= max(args[1]->decimals, args[2]->decimals);

  enum Item_result arg1_type=args[1]->result_type();
  enum Item_result arg2_type=args[2]->result_type();
  bool null1=args[1]->const_item() && args[1]->null_value;
  bool null2=args[2]->const_item() && args[2]->null_value;

  if (null1)
  {
    cached_result_type= arg2_type;
    collation.set(args[2]->collation.collation);
  }
  else if (null2)
  {
    cached_result_type= arg1_type;
    collation.set(args[1]->collation.collation);
  }
  else
  {
    agg_result_type(&cached_result_type, args+1, 2);
    if (cached_result_type == STRING_RESULT)
    {
      if (agg_arg_charsets(collation, args+1, 2, MY_COLL_ALLOW_CONV, 1))
        return;
    }
    else
    {
      collation.set(&my_charset_bin);	// Number
    }
  }
  max_length=
    (cached_result_type == DECIMAL_RESULT || cached_result_type == INT_RESULT) ?
    (max(args[1]->max_length - args[1]->decimals,
         args[2]->max_length - args[2]->decimals) + decimals +
         (unsigned_flag ? 0 : 1) ) :
    max(args[1]->max_length, args[2]->max_length);
}


uint Item_func_if::decimal_precision() const
{
  int precision=(max(args[1]->decimal_int_part(),args[2]->decimal_int_part())+
                 decimals);
  return min(precision, DECIMAL_MAX_PRECISION);
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
  Item *arg= args[0]->val_bool() ? args[1] : args[2];
  String *res=arg->val_str(str);
  if (res)
    res->set_charset(collation.collation);
  null_value=arg->null_value;
  return res;
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
        agg_arg_charsets(collation, args, arg_count, MY_COLL_CMP_CONV, 1))
      return;
  }
}


/*
  nullif () returns NULL if arguments are equal, else it returns the
  first argument.
  Note that we have to evaluate the first argument twice as the compare
  may have been done with a different type than return value
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

/*
  CASE expression 
  Return the matching ITEM or NULL if all compares (including else) failed
*/

Item *Item_func_case::find_item(String *str)
{
  String *first_expr_str, *tmp;
  my_decimal *first_expr_dec, first_expr_dec_val;
  longlong first_expr_int;
  double   first_expr_real;
  char buff[MAX_FIELD_WIDTH];
  String buff_str(buff,sizeof(buff),default_charset());

  /* These will be initialized later */
  LINT_INIT(first_expr_str);
  LINT_INIT(first_expr_int);
  LINT_INIT(first_expr_real);
  LINT_INIT(first_expr_dec);

  if (first_expr_num != -1)
  {
    switch (cmp_type)
    {
      case STRING_RESULT:
      	// We can't use 'str' here as this may be overwritten
	if (!(first_expr_str= args[first_expr_num]->val_str(&buff_str)))
	  return else_expr_num != -1 ? args[else_expr_num] : 0;	// Impossible
        break;
      case INT_RESULT:
	first_expr_int= args[first_expr_num]->val_int();
	if (args[first_expr_num]->null_value)
	  return else_expr_num != -1 ? args[else_expr_num] : 0;
	break;
      case REAL_RESULT:
	first_expr_real= args[first_expr_num]->val_real();
	if (args[first_expr_num]->null_value)
	  return else_expr_num != -1 ? args[else_expr_num] : 0;
	break;
      case DECIMAL_RESULT:
        first_expr_dec= args[first_expr_num]->val_decimal(&first_expr_dec_val);
        if (args[first_expr_num]->null_value)
          return else_expr_num != -1 ? args[else_expr_num] : 0;
        break;
      case ROW_RESULT:
      default:
        // This case should never be chosen
	DBUG_ASSERT(0);
	break;
    }
  }

  // Compare every WHEN argument with it and return the first match
  for (uint i=0 ; i < ncases ; i+=2)
  {
    if (first_expr_num == -1)
    {
      // No expression between CASE and the first WHEN
      if (args[i]->val_bool())
	return args[i+1];
      continue;
    }
    switch (cmp_type) {
    case STRING_RESULT:
      if ((tmp=args[i]->val_str(str)))		// If not null
	if (sortcmp(tmp,first_expr_str,cmp_collation.collation)==0)
	  return args[i+1];
      break;
    case INT_RESULT:
      if (args[i]->val_int()==first_expr_int && !args[i]->null_value) 
        return args[i+1];
      break;
    case REAL_RESULT: 
      if (args[i]->val_real() == first_expr_real && !args[i]->null_value)
        return args[i+1];
      break;
    case DECIMAL_RESULT:
    {
      my_decimal value;
      if (my_decimal_cmp(args[i]->val_decimal(&value), first_expr_dec) == 0)
        return args[i+1];
      break;
    }
    case ROW_RESULT:
    default:
      // This case should never be chosen
      DBUG_ASSERT(0);
      break;
    }
  }
  // No, WHEN clauses all missed, return ELSE expression
  return else_expr_num != -1 ? args[else_expr_num] : 0;
}


String *Item_func_case::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  String *res;
  Item *item=find_item(str);

  if (!item)
  {
    null_value=1;
    return 0;
  }
  null_value= 0;
  if (!(res=item->val_str(str)))
    null_value= 1;
  return res;
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


bool Item_func_case::fix_fields(THD *thd, Item **ref)
{
  /*
    buff should match stack usage from
    Item_func_case::val_int() -> Item_func_case::find_item()
  */
  char buff[MAX_FIELD_WIDTH*2+sizeof(String)*2+sizeof(String*)*2+sizeof(double)*2+sizeof(longlong)*2];
  bool res= Item_func::fix_fields(thd, ref);
  /*
    Call check_stack_overrun after fix_fields to be sure that stack variable
    is not optimized away
  */
  if (check_stack_overrun(thd, STACK_MIN_SIZE, buff))
    return TRUE;				// Fatal error flag is set!
  return res;
}



void Item_func_case::fix_length_and_dec()
{
  Item **agg;
  uint nagg;
  THD *thd= current_thd;
  
  if (!(agg= (Item**) sql_alloc(sizeof(Item*)*(ncases+1))))
    return;
  
  /*
    Aggregate all THEN and ELSE expression types
    and collations when string result
  */
  
  for (nagg= 0 ; nagg < ncases/2 ; nagg++)
    agg[nagg]= args[nagg*2+1];
  
  if (else_expr_num != -1)
    agg[nagg++]= args[else_expr_num];
  
  agg_result_type(&cached_result_type, agg, nagg);
  if ((cached_result_type == STRING_RESULT) &&
      agg_arg_charsets(collation, agg, nagg, MY_COLL_ALLOW_CONV, 1))
    return;
  
  
  /*
    Aggregate first expression and all THEN expression types
    and collations when string comparison
  */
  if (first_expr_num != -1)
  {
    agg[0]= args[first_expr_num];
    for (nagg= 0; nagg < ncases/2 ; nagg++)
      agg[nagg+1]= args[nagg*2];
    nagg++;
    agg_cmp_type(thd, &cmp_type, agg, nagg);
    if ((cmp_type == STRING_RESULT) &&
        agg_arg_charsets(cmp_collation, agg, nagg, MY_COLL_CMP_CONV, 1))
      return;
  }
  
  if (else_expr_num == -1 || args[else_expr_num]->maybe_null)
    maybe_null=1;
  
  max_length=0;
  decimals=0;
  for (uint i=0 ; i < ncases ; i+=2)
  {
    set_if_bigger(max_length,args[i+1]->max_length);
    set_if_bigger(decimals,args[i+1]->decimals);
  }
  if (else_expr_num != -1) 
  {
    set_if_bigger(max_length,args[else_expr_num]->max_length);
    set_if_bigger(decimals,args[else_expr_num]->decimals);
  }
}


uint Item_func_case::decimal_precision() const
{
  int max_int_part=0;
  for (uint i=0 ; i < ncases ; i+=2)
    set_if_bigger(max_int_part, args[i+1]->decimal_int_part());

  if (else_expr_num != -1) 
    set_if_bigger(max_int_part, args[else_expr_num]->decimal_int_part());
  return min(max_int_part + decimals, DECIMAL_MAX_PRECISION);
}


/* TODO:  Fix this so that it prints the whole CASE expression */

void Item_func_case::print(String *str)
{
  str->append(STRING_WITH_LEN("(case "));
  if (first_expr_num != -1)
  {
    args[first_expr_num]->print(str);
    str->append(' ');
  }
  for (uint i=0 ; i < ncases ; i+=2)
  {
    str->append(STRING_WITH_LEN("when "));
    args[i]->print(str);
    str->append(STRING_WITH_LEN(" then "));
    args[i+1]->print(str);
    str->append(' ');
  }
  if (else_expr_num != -1)
  {
    str->append(STRING_WITH_LEN("else "));
    args[else_expr_num]->print(str);
    str->append(' ');
  }
  str->append(STRING_WITH_LEN("end)"));
}

/*
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


void Item_func_coalesce::fix_length_and_dec()
{
  agg_result_type(&hybrid_type, args, arg_count);
  switch (hybrid_type) {
  case STRING_RESULT:
    count_only_length();
    decimals= NOT_FIXED_DEC;
    agg_arg_charsets(collation, args, arg_count, MY_COLL_ALLOW_CONV, 1);
    break;
  case DECIMAL_RESULT:
    count_decimal_length();
    break;
  case REAL_RESULT:
    count_real_length();
    break;
  case INT_RESULT:
    count_only_length();
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

static int cmp_longlong(void *cmp_arg, longlong *a,longlong *b)
{
  return *a < *b ? -1 : *a == *b ? 0 : 1;
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
  byte *result=get_value(item);
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

in_string::in_string(uint elements,qsort2_cmp cmp_func, CHARSET_INFO *cs)
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
    *str= *res;
  }
  if (!str->charset())
  {
    CHARSET_INFO *cs;
    if (!(cs= item->collation.collation))
      cs= &my_charset_bin;		// Should never happen for STR items
    str->set_charset(cs);
  }
}


byte *in_string::get_value(Item *item)
{
  return (byte*) item->val_str(&tmp);
}

in_row::in_row(uint elements, Item * item)
{
  base= (char*) new cmp_item_row[count= elements];
  size= sizeof(cmp_item_row);
  compare= (qsort2_cmp) cmp_row;
  tmp.store_value(item);
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

byte *in_row::get_value(Item *item)
{
  tmp.store_value(item);
  if (item->is_null())
    return 0;
  return (byte *)&tmp;
}

void in_row::set(uint pos, Item *item)
{
  DBUG_ENTER("in_row::set");
  DBUG_PRINT("enter", ("pos %u item 0x%lx", pos, (ulong) item));
  ((cmp_item_row*) base)[pos].store_value_by_template(&tmp, item);
  DBUG_VOID_RETURN;
}

in_longlong::in_longlong(uint elements)
  :in_vector(elements,sizeof(longlong),(qsort2_cmp) cmp_longlong, 0)
{}

void in_longlong::set(uint pos,Item *item)
{
  ((longlong*) base)[pos]=item->val_int();
}

byte *in_longlong::get_value(Item *item)
{
  tmp= item->val_int();
  if (item->null_value)
    return 0;
  return (byte*) &tmp;
}

in_double::in_double(uint elements)
  :in_vector(elements,sizeof(double),(qsort2_cmp) cmp_double, 0)
{}

void in_double::set(uint pos,Item *item)
{
  ((double*) base)[pos]= item->val_real();
}

byte *in_double::get_value(Item *item)
{
  tmp= item->val_real();
  if (item->null_value)
    return 0;					/* purecov: inspected */
  return (byte*) &tmp;
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
  if (res != dec)
    my_decimal2decimal(res, dec);
}


byte *in_decimal::get_value(Item *item)
{
  my_decimal *result= item->val_decimal(&val);
  if (item->null_value)
    return 0;
  return (byte *)result;
}


cmp_item* cmp_item::get_comparator(Item_result type,
                                   CHARSET_INFO *cs)
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
  DBUG_PRINT("enter",("this: 0x%lx", this));
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


void cmp_item_row::store_value(Item *item)
{
  DBUG_ENTER("cmp_item_row::store_value");
  n= item->cols();
  if (!comparators)
    comparators= (cmp_item **) current_thd->calloc(sizeof(cmp_item *)*n);
  if (comparators)
  {
    item->bring_value();
    item->null_value= 0;
    for (uint i=0; i < n; i++)
    {
      if (!comparators[i])
        if (!(comparators[i]=
              cmp_item::get_comparator(item->el(i)->result_type(),
                                       item->el(i)->collation.collation)))
	  break;					// new failed
      comparators[i]->store_value(item->el(i));
      item->null_value|= item->el(i)->null_value;
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
					      item->el(i));
      item->null_value|= item->el(i)->null_value;
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
    if (comparators[i]->cmp(arg->el(i)))
    {
      if (!arg->el(i)->null_value)
	return 1;
      was_null= 1;
    }
  }
  return (arg->null_value= was_null);
}


int cmp_item_row::compare(cmp_item *c)
{
  cmp_item_row *cmp= (cmp_item_row *) c;
  for (uint i=0; i < n; i++)
  {
    int res;
    if ((res= comparators[i]->compare(cmp->comparators[i])))
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
  cmp_item_decimal *cmp= (cmp_item_decimal*) arg;
  return my_decimal_cmp(&value, &cmp->value);
}


cmp_item* cmp_item_decimal::make_same()
{
  return new cmp_item_decimal();
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


/*
  Perform context analysis of an IN item tree

  SYNOPSIS:
    fix_fields()
    thd     reference to the global context of the query thread
    tables  list of all open tables involved in the query
    ref     pointer to Item* variable where pointer to resulting "fixed"
            item is to be assigned

  DESCRIPTION
    This function performs context analysis (name resolution) and calculates
    various attributes of the item tree with Item_func_in as its root.
    The function saves in ref the pointer to the item or to a newly created
    item that is considered as a replacement for the original one.

  NOTES
    Let T0(e)/T1(e) be the value of not_null_tables(e) when e is used on
    a predicate/function level. Then it's easy to show that:
      T0(e IN(e1,...,en))     = union(T1(e),intersection(T1(ei)))
      T1(e IN(e1,...,en))     = union(T1(e),intersection(T1(ei)))
      T0(e NOT IN(e1,...,en)) = union(T1(e),union(T1(ei)))
      T1(e NOT IN(e1,...,en)) = union(T1(e),intersection(T1(ei)))

  RETURN
    0   ok
    1   got error
*/

bool
Item_func_in::fix_fields(THD *thd, Item **ref)
{
  Item **arg, **arg_end;

  if (Item_func_opt_neg::fix_fields(thd, ref))
    return 1;

  /* not_null_tables_cache == union(T1(e),union(T1(ei))) */
  if (pred_level && negated)
    return 0;

  /* not_null_tables_cache = union(T1(e),intersection(T1(ei))) */
  not_null_tables_cache= ~(table_map) 0;
  for (arg= args + 1, arg_end= args + arg_count; arg != arg_end; arg++)
    not_null_tables_cache&= (*arg)->not_null_tables();
  not_null_tables_cache|= (*args)->not_null_tables();
  return 0;
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
  uint const_itm= 1;
  THD *thd= current_thd;
  
  agg_cmp_type(thd, &cmp_type, args, arg_count);

  if (cmp_type == STRING_RESULT &&
      agg_arg_charsets(cmp_collation, args, arg_count, MY_COLL_CMP_CONV, 1))
    return;

  for (arg=args+1, arg_end=args+arg_count; arg != arg_end ; arg++)
  {
    if (!arg[0]->const_item())
    {
      const_itm= 0;
      break;
    }
  }

  /*
    Row item with NULLs inside can return NULL or FALSE => 
    they can't be processed as static
  */
  if (const_itm && !nulls_in_row())
  {
    switch (cmp_type) {
    case STRING_RESULT:
      array=new in_string(arg_count-1,(qsort2_cmp) srtcmp_in, 
			  cmp_collation.collation);
      break;
    case INT_RESULT:
      array= new in_longlong(arg_count-1);
      break;
    case REAL_RESULT:
      array= new in_double(arg_count-1);
      break;
    case ROW_RESULT:
      array= new in_row(arg_count-1, args[0]);
      break;
    case DECIMAL_RESULT:
      array= new in_decimal(arg_count - 1);
      break;
    default:
      DBUG_ASSERT(0);
      return;
    }
    if (array && !(thd->is_fatal_error))		// If not EOM
    {
      uint j=0;
      for (uint i=1 ; i < arg_count ; i++)
      {
	array->set(j,args[i]);
	if (!args[i]->null_value)			// Skip NULL values
	  j++;
	else
	  have_null= 1;
      }
      if ((array->used_count=j))
	array->sort();
    }
  }
  else
  {
    in_item= cmp_item::get_comparator(cmp_type, cmp_collation.collation);
    if (cmp_type  == STRING_RESULT)
      in_item->cmp_charset= cmp_collation.collation;
  }
  maybe_null= args[0]->maybe_null;
  max_length= 1;
}


void Item_func_in::print(String *str)
{
  str->append('(');
  args[0]->print(str);
  if (negated)
    str->append(STRING_WITH_LEN(" not"));
  str->append(STRING_WITH_LEN(" in ("));
  print_args(str, 1);
  str->append(STRING_WITH_LEN("))"));
}


longlong Item_func_in::val_int()
{
  DBUG_ASSERT(fixed == 1);
  if (array)
  {
    int tmp=array->find(args[0]);
    null_value=args[0]->null_value || (!tmp && have_null);
    return (longlong) (!null_value && tmp != negated);
  }
  in_item->store_value(args[0]);
  if ((null_value=args[0]->null_value))
    return 0;
  have_null= 0;
  for (uint i=1 ; i < arg_count ; i++)
  {
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
   abort_on_null(item->abort_on_null),
   and_tables_cache(item->and_tables_cache)
{
  /*
    item->list will be copied by copy_andor_arguments() call
  */
}


void Item_cond::copy_andor_arguments(THD *thd, Item_cond *item)
{
  List_iterator_fast<Item> li(item->list);
  while (Item *it= li++)
    list.push_back(it->copy_andor_structure(thd));
}


bool
Item_cond::fix_fields(THD *thd, Item **ref)
{
  DBUG_ASSERT(fixed == 0);
  List_iterator<Item> li(list);
  Item *item;
#ifndef EMBEDDED_LIBRARY
  char buff[sizeof(char*)];			// Max local vars in function
#endif
  not_null_tables_cache= used_tables_cache= 0;
  const_item_cache= 1;
  /*
    and_table_cache is the value that Item_cond_or() returns for
    not_null_tables()
  */
  and_tables_cache= ~(table_map) 0;

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
    table_map tmp_table_map;
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
    used_tables_cache|=     item->used_tables();
    if (item->const_item())
      and_tables_cache= (table_map) 0;
    else
    {
      tmp_table_map= item->not_null_tables();
      not_null_tables_cache|= tmp_table_map;
      and_tables_cache&= tmp_table_map;
      const_item_cache= FALSE;
    }  
    with_sum_func=	    with_sum_func || item->with_sum_func;
    with_subselect|=        item->with_subselect;
    if (item->maybe_null)
      maybe_null=1;
  }
  thd->lex->current_select->cond_count+= list.elements;
  fix_length_and_dec();
  fixed= 1;
  return FALSE;
}

bool Item_cond::walk(Item_processor processor, bool walk_subquery, byte *arg)
{
  List_iterator_fast<Item> li(list);
  Item *item;
  while ((item= li++))
    if (item->walk(processor, walk_subquery, arg))
      return 1;
  return Item_func::walk(processor, walk_subquery, arg);
}


/*
  Transform an Item_cond object with a transformer callback function
   
  SYNOPSIS
    transform()
    transformer   the transformer callback function to be applied to the nodes
                  of the tree of the object
    arg           parameter to be passed to the transformer
  
  DESCRIPTION
    The function recursively applies the transform method with the
    same transformer to each member item of the condition list.
    If the call of the method for a member item returns a new item
    the old item is substituted for a new one.
    After this the transform method is applied to the root node
    of the Item_cond object. 
     
  RETURN VALUES
    Item returned as the result of transformation of the root node 
*/

Item *Item_cond::transform(Item_transformer transformer, byte *arg)
{
  DBUG_ASSERT(!current_thd->is_stmt_prepare());

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

/*
  Move SUM items out from item tree and replace with reference

  SYNOPSIS
    split_sum_func()
    thd			Thread handler
    ref_pointer_array	Pointer to array of reference fields
    fields		All fields in select

  NOTES
   This function is run on all expression (SELECT list, WHERE, HAVING etc)
   that have or refer (HAVING) to a SUM expression.

   The split is done to get an unique item for each SUM function
   so that we can easily find and calculate them.
   (Calculation done by update_sum_func() and copy_sum_funcs() in
   sql_select.cc)
*/

void Item_cond::split_sum_func(THD *thd, Item **ref_pointer_array,
                               List<Item> &fields)
{
  List_iterator<Item> li(list);
  Item *item;
  while ((item= li++))
    item->split_sum_func2(thd, ref_pointer_array, fields, li.ref(), TRUE);
}


table_map
Item_cond::used_tables() const
{						// This caches used_tables
  return used_tables_cache;
}


void Item_cond::update_used_tables()
{
  List_iterator_fast<Item> li(list);
  Item *item;

  used_tables_cache=0;
  const_item_cache=1;
  while ((item=li++))
  {
    item->update_used_tables();
    used_tables_cache|= item->used_tables();
    const_item_cache&= item->const_item();
  }
}


void Item_cond::print(String *str)
{
  str->append('(');
  List_iterator_fast<Item> li(list);
  Item *item;
  if ((item=li++))
    item->print(str);
  while ((item=li++))
  {
    str->append(' ');
    str->append(func_name());
    str->append(' ');
    item->print(str);
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
    VOID(li.replace(new_item));
  }
}


/*
  Evaluation of AND(expr, expr, expr ...)

  NOTES:
    abort_if_null is set for AND expressions for which we don't care if the
    result is NULL or 0. This is set for:
    - WHERE clause
    - HAVING clause
    - IF(expression)

  RETURN VALUES
    1  If all expressions are true
    0  If all expressions are false or if we find a NULL expression and
       'abort_on_null' is set.
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

/*
  Create an AND expression from two expressions

  SYNOPSIS
   and_expressions()
   a		expression or NULL
   b    	expression.
   org_item	Don't modify a if a == *org_item
		If a == NULL, org_item is set to point at b,
		to ensure that future calls will not modify b.

  NOTES
    This will not modify item pointed to by org_item or b
    The idea is that one can call this in a loop and create and
    'and' over all items without modifying any of the original items.

  RETURN
    NULL	Error
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
      res->used_tables_cache= a->used_tables() | b->used_tables();
      res->not_null_tables_cache= a->not_null_tables() | b->not_null_tables();
    }
    return res;
  }
  if (((Item_cond_and*) a)->add((Item*) b))
    return 0;
  ((Item_cond_and*) a)->used_tables_cache|= b->used_tables();
  ((Item_cond_and*) a)->not_null_tables_cache|= b->not_null_tables();
  return a;
}


longlong Item_func_isnull::val_int()
{
  DBUG_ASSERT(fixed == 1);
  /*
    Handle optimization if the argument can't be null
    This has to be here because of the test in update_used_tables().
  */
  if (!used_tables_cache)
    return cached_value;
  return args[0]->is_null() ? 1: 0;
}

longlong Item_is_not_null_test::val_int()
{
  DBUG_ASSERT(fixed == 1);
  DBUG_ENTER("Item_is_not_null_test::val_int");
  if (!used_tables_cache)
  {
    owner->was_null|= (!cached_value);
    DBUG_PRINT("info", ("cached :%d", cached_value));
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

/* Optimize case of not_null_column IS NULL */
void Item_is_not_null_test::update_used_tables()
{
  if (!args[0]->maybe_null)
  {
    used_tables_cache= 0;			/* is always true */
    cached_value= (longlong) 1;
  }
  else
  {
    args[0]->update_used_tables();
    if (!(used_tables_cache=args[0]->used_tables()))
    {
      /* Remember if the value is always NULL or never NULL */
      cached_value= (longlong) !args[0]->is_null();
    }
  }
}


longlong Item_func_isnotnull::val_int()
{
  DBUG_ASSERT(fixed == 1);
  return args[0]->is_null() ? 0 : 1;
}


void Item_func_isnotnull::print(String *str)
{
  str->append('(');
  args[0]->print(str);
  str->append(STRING_WITH_LEN(" is not null)"));
}


longlong Item_func_like::val_int()
{
  DBUG_ASSERT(fixed == 1);
  String* res = args[0]->val_str(&tmp_value1);
  if (args[0]->null_value)
  {
    null_value=1;
    return 0;
  }
  String* res2 = args[1]->val_str(&tmp_value2);
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


/* We can optimize a where if first character isn't a wildcard */

Item_func::optimize_type Item_func_like::select_optimize() const
{
  if (args[1]->const_item())
  {
    String* res2= args[1]->val_str((String *)&tmp_value2);

    if (!res2)
      return OPTIMIZE_NONE;

    if (*res2->ptr() != wild_many)
    {
      if (args[0]->result_type() != STRING_RESULT || *res2->ptr() != wild_one)
	return OPTIMIZE_OP;
    }
  }
  return OPTIMIZE_NONE;
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
    String *escape_str= escape_item->val_str(&tmp_value1);
    if (escape_str)
    {
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
        CHARSET_INFO *cs= escape_str->charset();
        my_wc_t wc;
        int rc= cs->cset->mb_wc(cs, &wc,
                                (const uchar*) escape_str->ptr(),
                                (const uchar*) escape_str->ptr() +
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
        CHARSET_INFO *cs= cmp.cmp_collation.collation;
        uint32 unused;
        if (escape_str->needs_conversion(escape_str->length(),
                                         escape_str->charset(), cs, &unused))
        {
          char ch;
          uint errors;
          uint32 cnvlen= copy_and_convert(&ch, 1, cs, escape_str->ptr(),
                                          escape_str->length(),
                                          escape_str->charset(), &errors);
          escape= cnvlen ? ch : '\\';
        }
        else
          escape= *(escape_str->ptr());
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
      String* res2 = args[1]->val_str(&tmp_value2);
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
        pattern     = first + 1;
        pattern_len = (int) len - 2;
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

#ifdef USE_REGEX

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
  max_length= 1;
  decimals= 0;

  if (agg_arg_charsets(cmp_collation, args, 2, MY_COLL_CMP_CONV, 1))
    return TRUE;

  used_tables_cache=args[0]->used_tables() | args[1]->used_tables();
  not_null_tables_cache= (args[0]->not_null_tables() |
			  args[1]->not_null_tables());
  const_item_cache=args[0]->const_item() && args[1]->const_item();
  if (!regex_compiled && args[1]->const_item())
  {
    char buff[MAX_FIELD_WIDTH];
    String tmp(buff,sizeof(buff),&my_charset_bin);
    String *res=args[1]->val_str(&tmp);
    if (args[1]->null_value)
    {						// Will always return NULL
      maybe_null=1;
      return FALSE;
    }
    int error;
    if ((error= my_regcomp(&preg,res->c_ptr(),
                           ((cmp_collation.collation->state &
                             (MY_CS_BINSORT | MY_CS_CSSORT)) ?
                            REG_EXTENDED | REG_NOSUB :
                            REG_EXTENDED | REG_NOSUB | REG_ICASE),
                           cmp_collation.collation)))
    {
      (void) my_regerror(error,&preg,buff,sizeof(buff));
      my_error(ER_REGEXP_ERROR, MYF(0), buff);
      return TRUE;
    }
    regex_compiled=regex_is_const=1;
    maybe_null=args[0]->maybe_null;
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
  String *res, tmp(buff,sizeof(buff),&my_charset_bin);

  res=args[0]->val_str(&tmp);
  if (args[0]->null_value)
  {
    null_value=1;
    return 0;
  }
  if (!regex_is_const)
  {
    char buff2[MAX_FIELD_WIDTH];
    String *res2, tmp2(buff2,sizeof(buff2),&my_charset_bin);

    res2= args[1]->val_str(&tmp2);
    if (args[1]->null_value)
    {
      null_value=1;
      return 0;
    }
    if (!regex_compiled || stringcmp(res2,&prev_regexp))
    {
      prev_regexp.copy(*res2);
      if (regex_compiled)
      {
	my_regfree(&preg);
	regex_compiled=0;
      }
      if (my_regcomp(&preg,res2->c_ptr_safe(),
                     ((cmp_collation.collation->state &
                       (MY_CS_BINSORT | MY_CS_CSSORT)) ?
                      REG_EXTENDED | REG_NOSUB :
                      REG_EXTENDED | REG_NOSUB | REG_ICASE),
                     cmp_collation.collation))
      {
	null_value=1;
	return 0;
      }
      regex_compiled=1;
    }
  }
  null_value=0;
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
  }
  DBUG_VOID_RETURN;
}


#endif /* USE_REGEX */


#ifdef LIKE_CMP_TOUPPER
#define likeconv(cs,A) (uchar) (cs)->toupper(A)
#else
#define likeconv(cs,A) (uchar) (cs)->sort_order[(uchar) (A)]
#endif


/**********************************************************************
  turboBM_compute_suffixes()
  Precomputation dependent only on pattern_len.
**********************************************************************/

void Item_func_like::turboBM_compute_suffixes(int *suff)
{
  const int   plm1 = pattern_len - 1;
  int            f = 0;
  int            g = plm1;
  int *const splm1 = suff + plm1;
  CHARSET_INFO	*cs= cmp.cmp_collation.collation;

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


/**********************************************************************
   turboBM_compute_good_suffix_shifts()
   Precomputation dependent only on pattern_len.
**********************************************************************/

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


/**********************************************************************
   turboBM_compute_bad_character_shifts()
   Precomputation dependent on pattern_len.
**********************************************************************/

void Item_func_like::turboBM_compute_bad_character_shifts()
{
  int *i;
  int *end = bmBc + alphabet_size;
  int j;
  const int plm1 = pattern_len - 1;
  CHARSET_INFO	*cs= cmp.cmp_collation.collation;

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


/**********************************************************************
  turboBM_matches()
  Search for pattern in text, returns true/false for match/no match
**********************************************************************/

bool Item_func_like::turboBM_matches(const char* text, int text_len) const
{
  register int bcShift;
  register int turboShift;
  int shift = pattern_len;
  int j     = 0;
  int u     = 0;
  CHARSET_INFO	*cs= cmp.cmp_collation.collation;

  const int plm1=  pattern_len - 1;
  const int tlmpl= text_len - pattern_len;

  /* Searching */
  if (!cs->sort_order)
  {
    while (j <= tlmpl)
    {
      register int i= plm1;
      while (i >= 0 && pattern[i] == text[i + j])
      {
	i--;
	if (i == plm1 - shift)
	  i-= u;
      }
      if (i < 0)
	return 1;

      register const int v = plm1 - i;
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
      register int i = plm1;
      while (i >= 0 && likeconv(cs,pattern[i]) == likeconv(cs,text[i + j]))
      {
	i--;
	if (i == plm1 - shift)
	  i-= u;
      }
      if (i < 0)
	return 1;

      register const int v = plm1 - i;
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


/*
  Make a logical XOR of the arguments.

  SYNOPSIS
    val_int()

  DESCRIPTION
  If either operator is NULL, return NULL.

  NOTE
    As we don't do any index optimization on XOR this is not going to be
    very fast to use.

  TODO (low priority)
    Change this to be optimized as:
      A XOR B   ->  (A) == 1 AND (B) <> 1) OR (A <> 1 AND (B) == 1)
    To be able to do this, we would however first have to extend the MySQL
    range optimizer to handle OR better.
*/

longlong Item_cond_xor::val_int()
{
  DBUG_ASSERT(fixed == 1);
  List_iterator<Item> li(list);
  Item *item;
  int result=0;	
  null_value=0;
  while ((item=li++))
  {
    result^= (item->val_int() != 0);
    if (item->null_value)
    {
      null_value=1;
      return 0;
    }
  }
  return (longlong) result;
}

/*
  Apply NOT transformation to the item and return a new one.

  SYNOPSIS
    neg_transformer()
    thd		thread handler

  DESCRIPTION
    Transform the item using next rules:
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

  RETURN
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


/* a IS NULL  ->  a IS NOT NULL */
Item *Item_func_isnull::neg_transformer(THD *thd)
{
  Item *item= new Item_func_isnotnull(args[0]);
  return item;
}


/* a IS NOT NULL  ->  a IS NULL */
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

// just fake method, should never be called
Item *Item_bool_rowready_func2::negated_item()
{
  DBUG_ASSERT(0);
  return 0;
}

Item_equal::Item_equal(Item_field *f1, Item_field *f2)
  : Item_bool_func(), const_item(0), eval_item(0), cond_false(0)
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
  cond_false= item_equal->cond_false;
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
  Item_func_eq *func= new Item_func_eq(c, const_item);
  func->set_cmp_func();
  func->quick_fix_field();
  if ((cond_false= !func->val_int()))
    const_item_cache= 1;
}

void Item_equal::add(Item_field *f)
{
  fields.push_back(f);
}

uint Item_equal::members()
{
  return fields.elements;
}


/*
  Check whether a field is referred in the multiple equality 

  SYNOPSIS
    contains()
    field   field whose occurrence is to be checked
  
  DESCRIPTION
    The function checks whether field is occurred in the Item_equal object 
    
  RETURN VALUES
    1       if nultiple equality contains a reference to field
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


/*
  Join members of another Item_equal object  

  SYNOPSIS
    merge()
    item    multiple equality whose members are to be joined
  
  DESCRIPTION
    The function actually merges two multiple equalities.
    After this operation the Item_equal object additionally contains
    the field items of another item of the type Item_equal.
    If the optional constant items are not equal the cond_false flag is
    set to 1.  
       
  RETURN VALUES
    none    
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


/*
  Order field items in multiple equality according to a sorting criteria 

  SYNOPSIS
    sort()
    cmp          function to compare field item 
    arg          context extra parameter for the cmp function
  
  DESCRIPTION
    The function perform ordering of the field items in the Item_equal
    object according to the criteria determined by the cmp callback parameter.
    If cmp(item_field1,item_field2,arg)<0 than item_field1 must be
    placed after item_fiel2.

  IMPLEMENTATION
    The function sorts field items by the exchange sort algorithm.
    The list of field items is looked through and whenever two neighboring
    members follow in a wrong order they are swapped. This is performed
    again and again until we get all members in a right order.
         
  RETURN VALUES
    None    
*/

void Item_equal::sort(Item_field_cmpfunc cmp, void *arg)
{
  bool swap;
  List_iterator<Item_field> it(fields);
  do
  {
    Item_field *item1= it++;
    Item_field **ref1= it.ref();
    Item_field *item2;

    swap= FALSE;
    while ((item2= it++))
    {
      Item_field **ref2= it.ref();
      if (cmp(item1, item2, arg) < 0)
      {
        Item_field *item= *ref1;
        *ref1= *ref2;
        *ref2= item;
        swap= TRUE;
      }
      else
      {
        item1= item2;
        ref1= ref2;
      }
    }
    it.rewind();
  } while (swap);
}


/*
  Check appearance of new constant items in the multiple equality object

  SYNOPSIS
    update_const()
  
  DESCRIPTION
    The function checks appearance of new constant items among
    the members of multiple equalities. Each new constant item is
    compared with the designated constant item if there is any in the
    multiple equality. If there is none the first new constant item
    becomes designated.
      
  RETURN VALUES
    none    
*/

void Item_equal::update_const()
{
  List_iterator<Item_field> it(fields);
  Item *item;
  while ((item= it++))
  {
    if (item->const_item())
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
  while ((item=li++))
  {
    item->update_used_tables();
    used_tables_cache|= item->used_tables();
    const_item_cache&= item->const_item();
  }
}

longlong Item_equal::val_int()
{
  Item_field *item_field;
  if (cond_false)
    return 0;
  List_iterator_fast<Item_field> it(fields);
  Item *item= const_item ? const_item : it++;
  if ((null_value= item->null_value))
    return 0;
  eval_item->store_value(item);
  while ((item_field= it++))
  {
    /* Skip fields of non-const tables. They haven't been read yet */
    if (item_field->field->table->const_table)
    {
      if ((null_value= item_field->null_value) || eval_item->cmp(item_field))
        return 0;
    }
  }
  return 1;
}

void Item_equal::fix_length_and_dec()
{
  Item *item= const_item ? const_item : get_first();
  eval_item= cmp_item::get_comparator(item->result_type(),
                                      item->collation.collation);
  if (item->result_type() == STRING_RESULT)
    eval_item->cmp_charset= cmp_collation.collation;
}

bool Item_equal::walk(Item_processor processor, bool walk_subquery, byte *arg)
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

Item *Item_equal::transform(Item_transformer transformer, byte *arg)
{
  DBUG_ASSERT(!current_thd->is_stmt_prepare());

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

void Item_equal::print(String *str)
{
  str->append(func_name());
  str->append('(');
  List_iterator_fast<Item_field> it(fields);
  Item *item;
  if (const_item)
    const_item->print(str);
  else
  {
    item= it++;
    item->print(str);
  }
  while ((item= it++))
  {
    str->append(',');
    str->append(' ');
    item->print(str);
  }
  str->append(')');
}

