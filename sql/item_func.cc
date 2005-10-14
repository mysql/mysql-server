/* Copyright (C) 2000-2003 MySQL AB

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


/* This file defines all numerical functions */

#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#include "slave.h"				// for wait_for_master_pos
#include <m_ctype.h>
#include <hash.h>
#include <time.h>
#include <ft_global.h>

#include "sp_head.h"
#include "sp_rcontext.h"
#include "sp.h"

#ifdef NO_EMBEDDED_ACCESS_CHECKS
#define sp_restore_security_context(A,B) while (0) {}
#endif


bool check_reserved_words(LEX_STRING *name)
{
  if (!my_strcasecmp(system_charset_info, name->str, "GLOBAL") ||
      !my_strcasecmp(system_charset_info, name->str, "LOCAL") ||
      !my_strcasecmp(system_charset_info, name->str, "SESSION"))
    return TRUE;
  return FALSE;
}


/* return TRUE if item is a constant */

bool
eval_const_cond(COND *cond)
{
  return ((Item_func*) cond)->val_int() ? TRUE : FALSE;
}


void Item_func::set_arguments(List<Item> &list)
{
  allowed_arg_cols= 1;
  arg_count=list.elements;
  args= tmp_arg;                                // If 2 arguments
  if (arg_count <= 2 || (args=(Item**) sql_alloc(sizeof(Item*)*arg_count)))
  {
    List_iterator_fast<Item> li(list);
    Item *item;
    Item **save_args= args;

    while ((item=li++))
    {
      *(save_args++)= item;
      with_sum_func|=item->with_sum_func;
    }
  }
  list.empty();					// Fields are used
}

Item_func::Item_func(List<Item> &list)
  :allowed_arg_cols(1)
{
  set_arguments(list);
}

Item_func::Item_func(THD *thd, Item_func *item)
  :Item_result_field(thd, item),
   allowed_arg_cols(item->allowed_arg_cols),
   arg_count(item->arg_count),
   used_tables_cache(item->used_tables_cache),
   not_null_tables_cache(item->not_null_tables_cache),
   const_item_cache(item->const_item_cache)
{
  if (arg_count)
  {
    if (arg_count <=2)
      args= tmp_arg;
    else
    {
      if (!(args=(Item**) thd->alloc(sizeof(Item*)*arg_count)))
	return;
    }
    memcpy((char*) args, (char*) item->args, sizeof(Item*)*arg_count);
  }
}


/*
  Resolve references to table column for a function and it's argument

  SYNOPSIS:
  fix_fields()
  thd		Thread object
  ref		Pointer to where this object is used.  This reference
		is used if we want to replace this object with another
		one (for example in the summary functions).

  DESCRIPTION
    Call fix_fields() for all arguments to the function.  The main intention
    is to allow all Item_field() objects to setup pointers to the table fields.

    Sets as a side effect the following class variables:
      maybe_null	Set if any argument may return NULL
      with_sum_func	Set if any of the arguments contains a sum function
      used_tables_cache Set to union of the tables used by arguments

      str_value.charset If this is a string function, set this to the
			character set for the first argument.
			If any argument is binary, this is set to binary

   If for any item any of the defaults are wrong, then this can
   be fixed in the fix_length_and_dec() function that is called
   after this one or by writing a specialized fix_fields() for the
   item.

  RETURN VALUES
  FALSE	ok
  TRUE	Got error.  Stored with my_error().
*/

bool
Item_func::fix_fields(THD *thd, Item **ref)
{
  DBUG_ASSERT(fixed == 0);
  Item **arg,**arg_end;
#ifndef EMBEDDED_LIBRARY			// Avoid compiler warning
  char buff[STACK_BUFF_ALLOC];			// Max argument in function
#endif

  used_tables_cache= not_null_tables_cache= 0;
  const_item_cache=1;

  if (check_stack_overrun(thd, STACK_MIN_SIZE, buff))
    return TRUE;				// Fatal error if flag is set!
  if (arg_count)
  {						// Print purify happy
    for (arg=args, arg_end=args+arg_count; arg != arg_end ; arg++)
    {
      Item *item;
      /*
	We can't yet set item to *arg as fix_fields may change *arg
	We shouldn't call fix_fields() twice, so check 'fixed' field first
      */
      if ((!(*arg)->fixed && (*arg)->fix_fields(thd, arg)))
	return TRUE;				/* purecov: inspected */
      item= *arg;

      if (allowed_arg_cols)
      {
        if (item->check_cols(allowed_arg_cols))
          return 1;
      }
      else
      {
        /*  we have to fetch allowed_arg_cols from first argument */
        DBUG_ASSERT(arg == args); // it is first argument
        allowed_arg_cols= item->cols();
        DBUG_ASSERT(allowed_arg_cols); // Can't be 0 any more
      }

      if (item->maybe_null)
	maybe_null=1;

      with_sum_func= with_sum_func || item->with_sum_func;
      used_tables_cache|=     item->used_tables();
      not_null_tables_cache|= item->not_null_tables();
      const_item_cache&=      item->const_item();
    }
  }
  fix_length_and_dec();
  if (thd->net.report_error) // An error inside fix_length_and_dec occured
    return TRUE;
  fixed= 1;
  return FALSE;
}

bool Item_func::walk (Item_processor processor, byte *argument)
{
  if (arg_count)
  {
    Item **arg,**arg_end;
    for (arg= args, arg_end= args+arg_count; arg != arg_end; arg++)
    {
      if ((*arg)->walk(processor, argument))
	return 1;
    }
  }
  return (this->*processor)(argument);
}

void Item_func::traverse_cond(Cond_traverser traverser,
                              void *argument, traverse_order order)
{
  if (arg_count)
  {
    Item **arg,**arg_end;

    switch (order) {
    case(PREFIX):
      (*traverser)(this, argument);
      for (arg= args, arg_end= args+arg_count; arg != arg_end; arg++)
      {
	(*arg)->traverse_cond(traverser, argument, order);
      }
      break;
    case (POSTFIX):
      for (arg= args, arg_end= args+arg_count; arg != arg_end; arg++)
      {
	(*arg)->traverse_cond(traverser, argument, order);
      }
      (*traverser)(this, argument);
    }
  }
}



/*
  Transform an Item_func object with a transformer callback function
   
  SYNOPSIS
    transform()
    transformer   the transformer callback function to be applied to the nodes
                  of the tree of the object
    argument      parameter to be passed to the transformer
  
  DESCRIPTION
    The function recursively applies the transform method with the
    same transformer to each argument the function.
    If the call of the method for a member item returns a new item
    the old item is substituted for a new one.
    After this the transform method is applied to the root node
    of the Item_func object. 
     
  RETURN VALUES
    Item returned as the result of transformation of the root node 
*/

Item *Item_func::transform(Item_transformer transformer, byte *argument)
{
  if (arg_count)
  {
    Item **arg,**arg_end;
    for (arg= args, arg_end= args+arg_count; arg != arg_end; arg++)
    {
      Item *new_item= (*arg)->transform(transformer, argument);
      if (!new_item)
	return 0;
      if (*arg != new_item)
        current_thd->change_item_tree(arg, new_item);
    }
  }
  return (this->*transformer)(argument);
}


/* See comments in Item_cmp_func::split_sum_func() */

void Item_func::split_sum_func(THD *thd, Item **ref_pointer_array,
                               List<Item> &fields)
{
  Item **arg, **arg_end;
  for (arg= args, arg_end= args+arg_count; arg != arg_end ; arg++)
    (*arg)->split_sum_func2(thd, ref_pointer_array, fields, arg);
}


void Item_func::update_used_tables()
{
  used_tables_cache=0;
  const_item_cache=1;
  for (uint i=0 ; i < arg_count ; i++)
  {
    args[i]->update_used_tables();
    used_tables_cache|=args[i]->used_tables();
    const_item_cache&=args[i]->const_item();
  }
}


table_map Item_func::used_tables() const
{
  return used_tables_cache;
}


table_map Item_func::not_null_tables() const
{
  return not_null_tables_cache;
}


void Item_func::print(String *str)
{
  str->append(func_name());
  str->append('(');
  print_args(str, 0);
  str->append(')');
}


void Item_func::print_args(String *str, uint from)
{
  for (uint i=from ; i < arg_count ; i++)
  {
    if (i != from)
      str->append(',');
    args[i]->print(str);
  }
}


void Item_func::print_op(String *str)
{
  str->append('(');
  for (uint i=0 ; i < arg_count-1 ; i++)
  {
    args[i]->print(str);
    str->append(' ');
    str->append(func_name());
    str->append(' ');
  }
  args[arg_count-1]->print(str);
  str->append(')');
}


bool Item_func::eq(const Item *item, bool binary_cmp) const
{
  /* Assume we don't have rtti */
  if (this == item)
    return 1;
  if (item->type() != FUNC_ITEM)
    return 0;
  Item_func *item_func=(Item_func*) item;
  if (arg_count != item_func->arg_count ||
      func_name() != item_func->func_name())
    return 0;
  for (uint i=0; i < arg_count ; i++)
    if (!args[i]->eq(item_func->args[i], binary_cmp))
      return 0;
  return 1;
}


Field *Item_func::tmp_table_field(TABLE *t_arg)
{
  Field *res;
  LINT_INIT(res);

  switch (result_type()) {
  case INT_RESULT:
    if (max_length > 11)
      res= new Field_longlong(max_length, maybe_null, name, t_arg,
			      unsigned_flag);
    else
      res= new Field_long(max_length, maybe_null, name, t_arg,
			  unsigned_flag);
    break;
  case REAL_RESULT:
    res= new Field_double(max_length, maybe_null, name, t_arg, decimals);
    break;
  case STRING_RESULT:
    res= make_string_field(t_arg);
    break;
  case DECIMAL_RESULT:
    res= new Field_new_decimal(my_decimal_precision_to_length(decimal_precision(),
                                                              decimals,
                                                              unsigned_flag),
                               maybe_null, name, t_arg, decimals, unsigned_flag);
    break;
  case ROW_RESULT:
  default:
    // This case should never be chosen
    DBUG_ASSERT(0);
    break;
  }
  return res;
}

my_decimal *Item_func::val_decimal(my_decimal *decimal_value)
{
  DBUG_ASSERT(fixed);
  int2my_decimal(E_DEC_FATAL_ERROR, val_int(), unsigned_flag, decimal_value);
  return decimal_value;
}


String *Item_real_func::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  double nr= val_real();
  if (null_value)
    return 0; /* purecov: inspected */
  str->set(nr,decimals, &my_charset_bin);
  return str;
}


my_decimal *Item_real_func::val_decimal(my_decimal *decimal_value)
{
  DBUG_ASSERT(fixed);
  double nr= val_real();
  if (null_value)
    return 0; /* purecov: inspected */
  double2my_decimal(E_DEC_FATAL_ERROR, nr, decimal_value);
  return decimal_value;
}


void Item_func::fix_num_length_and_dec()
{
  decimals= 0;
  for (uint i=0 ; i < arg_count ; i++)
  {
    set_if_bigger(decimals, args[i]->decimals);
  }
  max_length= float_length(decimals);
}


void Item_func_numhybrid::fix_num_length_and_dec()
{}


/*
  Set max_length/decimals of function if function is fixed point and
  result length/precision depends on argument ones

  SYNOPSIS
    Item_func::count_decimal_length()
*/

void Item_func::count_decimal_length()
{
  int max_int_part= 0;
  decimals= 0;
  unsigned_flag= 1;
  for (uint i=0 ; i < arg_count ; i++)
  {
    set_if_bigger(decimals, args[i]->decimals);
    set_if_bigger(max_int_part, args[i]->decimal_int_part());
    set_if_smaller(unsigned_flag, args[i]->unsigned_flag);
  }
  int precision= min(max_int_part + decimals, DECIMAL_MAX_PRECISION);
  max_length= my_decimal_precision_to_length(precision, decimals,
                                             unsigned_flag);
}


/*
  Set max_length of if it is maximum length of its arguments

  SYNOPSIS
    Item_func::count_only_length()
*/

void Item_func::count_only_length()
{
  max_length= 0;
  unsigned_flag= 0;
  for (uint i=0 ; i < arg_count ; i++)
  {
    set_if_bigger(max_length, args[i]->max_length);
    set_if_bigger(unsigned_flag, args[i]->unsigned_flag);
  }
}


/*
  Set max_length/decimals of function if function is floating point and
  result length/precision depends on argument ones

  SYNOPSIS
    Item_func::count_real_length()
*/

void Item_func::count_real_length()
{
  uint32 length= 0;
  decimals= 0;
  max_length= 0;
  for (uint i=0 ; i < arg_count ; i++)
  {
    if (decimals != NOT_FIXED_DEC)
    {
      set_if_bigger(decimals, args[i]->decimals);
      set_if_bigger(length, (args[i]->max_length - args[i]->decimals));
    }
    set_if_bigger(max_length, args[i]->max_length);
  }
  if (decimals != NOT_FIXED_DEC)
  {
    max_length= length;
    length+= decimals;
    if (length < max_length)  // If previous operation gave overflow
      max_length= UINT_MAX32;
    else
      max_length= length;
  }
}



void Item_func::signal_divide_by_null()
{
  THD *thd= current_thd;
  if (thd->variables.sql_mode & MODE_ERROR_FOR_DIVISION_BY_ZERO)
    push_warning(thd, MYSQL_ERROR::WARN_LEVEL_ERROR, ER_DIVISION_BY_ZERO,
                 ER(ER_DIVISION_BY_ZERO));
  null_value= 1;
}


Item *Item_func::get_tmp_table_item(THD *thd)
{
  if (!with_sum_func && !const_item())
    return new Item_field(result_field);
  return copy_or_same(thd);
}

String *Item_int_func::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  longlong nr=val_int();
  if (null_value)
    return 0;
  if (!unsigned_flag)
    str->set(nr,&my_charset_bin);
  else
    str->set((ulonglong) nr,&my_charset_bin);
  return str;
}


/*
  Check arguments here to determine result's type for a numeric
  function of two arguments.

  SYNOPSIS
    Item_num_op::find_num_type()
*/

void Item_num_op::find_num_type(void)
{
  DBUG_ENTER("Item_num_op::find_num_type");
  DBUG_PRINT("info", ("name %s", func_name()));
  DBUG_ASSERT(arg_count == 2);
  Item_result r0= args[0]->result_type();
  Item_result r1= args[1]->result_type();

  if (r0 == REAL_RESULT || r1 == REAL_RESULT ||
      r0 == STRING_RESULT || r1 ==STRING_RESULT)
  {
    count_real_length();
    max_length= float_length(decimals);
    hybrid_type= REAL_RESULT;
  }
  else if (r0 == DECIMAL_RESULT || r1 == DECIMAL_RESULT)
  {
    hybrid_type= DECIMAL_RESULT;
    result_precision();
  }
  else
  {
    DBUG_ASSERT(r0 == INT_RESULT && r1 == INT_RESULT);
    decimals= 0;
    hybrid_type=INT_RESULT;
    result_precision();
  }
  DBUG_PRINT("info", ("Type: %s",
             (hybrid_type == REAL_RESULT ? "REAL_RESULT" :
              hybrid_type == DECIMAL_RESULT ? "DECIMAL_RESULT" :
              hybrid_type == INT_RESULT ? "INT_RESULT" :
              "--ILLEGAL!!!--")));
  DBUG_VOID_RETURN;
}


/*
  Set result type for a numeric function of one argument
  (can be also used by a numeric function of many arguments, if the result
  type depends only on the first argument)

  SYNOPSIS
    Item_func_num1::find_num_type()
*/

void Item_func_num1::find_num_type()
{
  DBUG_ENTER("Item_func_num1::find_num_type");
  DBUG_PRINT("info", ("name %s", func_name()));
  switch (hybrid_type= args[0]->result_type()) {
  case INT_RESULT:
    unsigned_flag= args[0]->unsigned_flag;
    break;
  case STRING_RESULT:
  case REAL_RESULT:
    hybrid_type= REAL_RESULT;
    max_length= float_length(decimals);
    break;
  case DECIMAL_RESULT:
    break;
  default:
    DBUG_ASSERT(0);
  }
  DBUG_PRINT("info", ("Type: %s",
                      (hybrid_type == REAL_RESULT ? "REAL_RESULT" :
                       hybrid_type == DECIMAL_RESULT ? "DECIMAL_RESULT" :
                       hybrid_type == INT_RESULT ? "INT_RESULT" :
                       "--ILLEGAL!!!--")));
  DBUG_VOID_RETURN;
}


void Item_func_num1::fix_num_length_and_dec()
{
  decimals= args[0]->decimals;
  max_length= args[0]->max_length;
}


void Item_func_numhybrid::fix_length_and_dec()
{
  fix_num_length_and_dec();
  find_num_type();
}


String *Item_func_numhybrid::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  switch (hybrid_type) {
  case DECIMAL_RESULT:
  {
    my_decimal decimal_value, *val;
    if (!(val= decimal_op(&decimal_value)))
      return 0;                                 // null is set
    my_decimal_round(E_DEC_FATAL_ERROR, val, decimals, FALSE, val);
    my_decimal2string(E_DEC_FATAL_ERROR, val, 0, 0, 0, str);
    break;
  }
  case INT_RESULT:
  {
    longlong nr= int_op();
    if (null_value)
      return 0; /* purecov: inspected */
    if (!unsigned_flag)
      str->set(nr,&my_charset_bin);
    else
      str->set((ulonglong) nr,&my_charset_bin);
    break;
  }
  case REAL_RESULT:
  {
    double nr= real_op();
    if (null_value)
      return 0; /* purecov: inspected */
    str->set(nr,decimals,&my_charset_bin);
    break;
  }
  case STRING_RESULT:
    return str_op(&str_value);
  default:
    DBUG_ASSERT(0);
  }
  return str;
}


double Item_func_numhybrid::val_real()
{
  DBUG_ASSERT(fixed == 1);
  switch (hybrid_type) {
  case DECIMAL_RESULT:
  {
    my_decimal decimal_value, *val;
    double result;
    if (!(val= decimal_op(&decimal_value)))
      return 0.0;                               // null is set
    my_decimal2double(E_DEC_FATAL_ERROR, val, &result);
    return result;
  }
  case INT_RESULT:
    return (double)int_op();
  case REAL_RESULT:
    return real_op();
  case STRING_RESULT:
  {
    char *end_not_used;
    int err_not_used;
    String *res= str_op(&str_value);
    return (res ? my_strntod(res->charset(), (char*) res->ptr(), res->length(),
			     &end_not_used, &err_not_used) : 0.0);
  }
  default:
    DBUG_ASSERT(0);
  }
  return 0.0;
}


longlong Item_func_numhybrid::val_int()
{
  DBUG_ASSERT(fixed == 1);
  switch (hybrid_type) {
  case DECIMAL_RESULT:
  {
    my_decimal decimal_value, *val;
    if (!(val= decimal_op(&decimal_value)))
      return 0;                                 // null is set
    longlong result;
    my_decimal2int(E_DEC_FATAL_ERROR, val, unsigned_flag, &result);
    return result;
  }
  case INT_RESULT:
    return int_op();
  case REAL_RESULT:
    return (longlong)real_op();
  case STRING_RESULT:
  {
    int err_not_used;
    String *res;
    if (!(res= str_op(&str_value)))
      return 0;

    char *end= (char*) res->ptr() + res->length();
    CHARSET_INFO *cs= str_value.charset();
    return (*(cs->cset->strtoll10))(cs, res->ptr(), &end, &err_not_used);
  }
  default:
    DBUG_ASSERT(0);
  }
  return 0;
}


my_decimal *Item_func_numhybrid::val_decimal(my_decimal *decimal_value)
{
  my_decimal *val= decimal_value;
  DBUG_ASSERT(fixed == 1);
  switch (hybrid_type) {
  case DECIMAL_RESULT:
    val= decimal_op(decimal_value);
    break;
  case INT_RESULT:
  {
    longlong result= int_op();
    int2my_decimal(E_DEC_FATAL_ERROR, result, unsigned_flag, decimal_value);
    break;
  }
  case REAL_RESULT:
  {
    double result= (double)real_op();
    double2my_decimal(E_DEC_FATAL_ERROR, result, decimal_value);
    break;
  }
  case STRING_RESULT:
  {
    String *res;
    if (!(res= str_op(&str_value)))
      return NULL;

    str2my_decimal(E_DEC_FATAL_ERROR, (char*) res->ptr(),
                   res->length(), res->charset(), decimal_value);
    break;
  }  
  case ROW_RESULT:
  default:
    DBUG_ASSERT(0);
  }
  return val;
}


void Item_func_signed::print(String *str)
{
  str->append("cast(", 5);
  args[0]->print(str);
  str->append(" as signed)", 11);

}


longlong Item_func_signed::val_int_from_str(int *error)
{
  char buff[MAX_FIELD_WIDTH], *end;
  String tmp(buff,sizeof(buff), &my_charset_bin), *res;
  longlong value;

  /*
    For a string result, we must first get the string and then convert it
    to a longlong
  */

  if (!(res= args[0]->val_str(&tmp)))
  {
    null_value= 1;
    *error= 0;
    return 0;
  }
  null_value= 0;
  end= (char*) res->ptr()+ res->length();
  value= my_strtoll10(res->ptr(), &end, error);
  if (*error > 0 || end != res->ptr()+ res->length())
    push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                        ER_TRUNCATED_WRONG_VALUE,
                        ER(ER_TRUNCATED_WRONG_VALUE), "INTEGER",
                        res->c_ptr());
  return value;
}


longlong Item_func_signed::val_int()
{
  longlong value;
  int error;

  if (args[0]->cast_to_int_type() != STRING_RESULT)
  {
    value= args[0]->val_int();
    null_value= args[0]->null_value; 
    return value;
  }

  value= val_int_from_str(&error);
  if (value < 0 && error == 0)
  {
    push_warning(current_thd, MYSQL_ERROR::WARN_LEVEL_WARN, ER_UNKNOWN_ERROR,
                 "Cast to signed converted positive out-of-range integer to "
                 "it's negative complement");
  }
  return value;
}


void Item_func_unsigned::print(String *str)
{
  str->append("cast(", 5);
  args[0]->print(str);
  str->append(" as unsigned)", 13);

}


longlong Item_func_unsigned::val_int()
{
  longlong value;
  int error;

  if (args[0]->cast_to_int_type() != STRING_RESULT)
  {
    value= args[0]->val_int();
    null_value= args[0]->null_value; 
    return value;
  }

  value= val_int_from_str(&error);
  if (error < 0)
    push_warning(current_thd, MYSQL_ERROR::WARN_LEVEL_WARN, ER_UNKNOWN_ERROR,
                 "Cast to unsigned converted negative integer to it's "
                 "positive complement");
  return value;
}


String *Item_decimal_typecast::val_str(String *str)
{
  my_decimal tmp_buf, *tmp= val_decimal(&tmp_buf);
  if (null_value)
    return NULL;
  my_decimal2string(E_DEC_FATAL_ERROR, &tmp_buf, 0, 0, 0, str);
  return str;
}


double Item_decimal_typecast::val_real()
{
  my_decimal tmp_buf, *tmp= val_decimal(&tmp_buf);
  double res;
  if (null_value)
    return 0.0;
  my_decimal2double(E_DEC_FATAL_ERROR, tmp, &res);
  return res;
}


longlong Item_decimal_typecast::val_int()
{
  my_decimal tmp_buf, *tmp= val_decimal(&tmp_buf);
  longlong res;
  if (null_value)
    return 0;
  my_decimal2int(E_DEC_FATAL_ERROR, tmp, unsigned_flag, &res);
  return res;
}


my_decimal *Item_decimal_typecast::val_decimal(my_decimal *dec)
{
  my_decimal tmp_buf, *tmp= args[0]->val_decimal(&tmp_buf);
  if ((null_value= args[0]->null_value))
    return NULL;
  my_decimal_round(E_DEC_FATAL_ERROR, tmp, decimals, FALSE, dec);
  return dec;
}


void Item_decimal_typecast::print(String *str)
{
  str->append("cast(", 5);
  args[0]->print(str);
  str->append(" as decimal)", 12);
}


double Item_func_plus::real_op()
{
  double value= args[0]->val_real() + args[1]->val_real();
  if ((null_value=args[0]->null_value || args[1]->null_value))
    return 0.0;
  return value;
}


longlong Item_func_plus::int_op()
{
  longlong value=args[0]->val_int()+args[1]->val_int();
  if ((null_value=args[0]->null_value || args[1]->null_value))
    return 0;
  return value;
}


/*
  Calculate plus of two decimail's

  SYNOPSIS
    decimal_op()
    decimal_value	Buffer that can be used to store result

  RETURN
   0	Value was NULL;  In this case null_value is set
   #	Value of operation as a decimal
*/

my_decimal *Item_func_plus::decimal_op(my_decimal *decimal_value)
{
  my_decimal value1, *val1;
  my_decimal value2, *val2;
  val1= args[0]->val_decimal(&value1);
  if ((null_value= args[0]->null_value))
    return 0;
  val2= args[1]->val_decimal(&value2);
  if (!(null_value= (args[1]->null_value ||
                     my_decimal_add(E_DEC_FATAL_ERROR, decimal_value, val1,
                                    val2) > 1)))
    return decimal_value;
  return 0;
}

/*
  Set precision of results for additive operations (+ and -)

  SYNOPSIS
    Item_func_additive_op::result_precision()
*/
void Item_func_additive_op::result_precision()
{
  decimals= max(args[0]->decimals, args[1]->decimals);
  int max_int_part= max(args[0]->decimal_precision() - args[0]->decimals,
                        args[1]->decimal_precision() - args[1]->decimals);
  int precision= min(max_int_part + 1 + decimals, DECIMAL_MAX_PRECISION);

  /* Integer operations keep unsigned_flag if one of arguments is unsigned */
  if (result_type() == INT_RESULT)
    unsigned_flag= args[0]->unsigned_flag | args[1]->unsigned_flag;
  else
    unsigned_flag= args[0]->unsigned_flag & args[1]->unsigned_flag;
  max_length= my_decimal_precision_to_length(precision, decimals,
                                             unsigned_flag);
}


/*
  The following function is here to allow the user to force
  subtraction of UNSIGNED BIGINT to return negative values.
*/

void Item_func_minus::fix_length_and_dec()
{
  Item_num_op::fix_length_and_dec();
  if (unsigned_flag &&
      (current_thd->variables.sql_mode & MODE_NO_UNSIGNED_SUBTRACTION))
    unsigned_flag=0;
}


double Item_func_minus::real_op()
{
  double value= args[0]->val_real() - args[1]->val_real();
  if ((null_value=args[0]->null_value || args[1]->null_value))
    return 0.0;
  return value;
}


longlong Item_func_minus::int_op()
{
  longlong value=args[0]->val_int() - args[1]->val_int();
  if ((null_value=args[0]->null_value || args[1]->null_value))
    return 0;
  return value;
}


/* See Item_func_plus::decimal_op for comments */

my_decimal *Item_func_minus::decimal_op(my_decimal *decimal_value)
{
  my_decimal value1, *val1;
  my_decimal value2, *val2= 

  val1= args[0]->val_decimal(&value1);
  if ((null_value= args[0]->null_value))
    return 0;
  val2= args[1]->val_decimal(&value2);
  if (!(null_value= (args[1]->null_value ||
                     my_decimal_sub(E_DEC_FATAL_ERROR, decimal_value, val1,
                                    val2) > 1)))
    return decimal_value;
  return 0;
}


double Item_func_mul::real_op()
{
  DBUG_ASSERT(fixed == 1);
  double value= args[0]->val_real() * args[1]->val_real();
  if ((null_value=args[0]->null_value || args[1]->null_value))
    return 0.0;
  return value;
}


longlong Item_func_mul::int_op()
{
  DBUG_ASSERT(fixed == 1);
  longlong value=args[0]->val_int()*args[1]->val_int();
  if ((null_value=args[0]->null_value || args[1]->null_value))
    return 0;
  return value;
}


/* See Item_func_plus::decimal_op for comments */

my_decimal *Item_func_mul::decimal_op(my_decimal *decimal_value)
{
  my_decimal value1, *val1;
  my_decimal value2, *val2;
  val1= args[0]->val_decimal(&value1);
  if ((null_value= args[0]->null_value))
    return 0;
  val2= args[1]->val_decimal(&value2);
  if (!(null_value= (args[1]->null_value ||
                     my_decimal_mul(E_DEC_FATAL_ERROR, decimal_value, val1,
                                    val2) > 1)))
    return decimal_value;
  return 0;
}


void Item_func_mul::result_precision()
{
  /* Integer operations keep unsigned_flag if one of arguments is unsigned */
  if (result_type() == INT_RESULT)
    unsigned_flag= args[0]->unsigned_flag | args[1]->unsigned_flag;
  else
    unsigned_flag= args[0]->unsigned_flag & args[1]->unsigned_flag;
  decimals= min(args[0]->decimals + args[1]->decimals, DECIMAL_MAX_SCALE);
  int precision= min(args[0]->decimal_precision() + args[1]->decimal_precision(),
                     DECIMAL_MAX_PRECISION);
  max_length= my_decimal_precision_to_length(precision, decimals,unsigned_flag);
}


double Item_func_div::real_op()
{
  DBUG_ASSERT(fixed == 1);
  double value= args[0]->val_real();
  double val2= args[1]->val_real();
  if ((null_value= args[0]->null_value || args[1]->null_value))
    return 0.0;
  if (val2 == 0.0)
  {
    signal_divide_by_null();
    return 0.0;
  }
  return value/val2;
}


my_decimal *Item_func_div::decimal_op(my_decimal *decimal_value)
{
  my_decimal value1, *val1;
  my_decimal value2, *val2;

  val1= args[0]->val_decimal(&value1);
  if ((null_value= args[0]->null_value))
    return 0;
  val2= args[1]->val_decimal(&value2);
  if ((null_value= args[1]->null_value))
    return 0;
  switch (my_decimal_div(E_DEC_FATAL_ERROR & ~E_DEC_DIV_ZERO, decimal_value,
                         val1, val2, prec_increment)) {
  case E_DEC_TRUNCATED:
  case E_DEC_OK:
    return decimal_value;
  case E_DEC_DIV_ZERO:
    signal_divide_by_null();
  default:
    null_value= 1;                              // Safety
    return 0;
  }
}


void Item_func_div::result_precision()
{
  uint precision=min(args[0]->decimal_precision() + prec_increment,
                     DECIMAL_MAX_PRECISION);
  /* Integer operations keep unsigned_flag if one of arguments is unsigned */
  if (result_type() == INT_RESULT)
    unsigned_flag= args[0]->unsigned_flag | args[1]->unsigned_flag;
  else
    unsigned_flag= args[0]->unsigned_flag & args[1]->unsigned_flag;
  decimals= min(args[0]->decimals + prec_increment, DECIMAL_MAX_SCALE);
  max_length= my_decimal_precision_to_length(precision, decimals,
                                             unsigned_flag);
}


void Item_func_div::fix_length_and_dec()
{
  DBUG_ENTER("Item_func_div::fix_length_and_dec");
  prec_increment= current_thd->variables.div_precincrement;
  Item_num_op::fix_length_and_dec();
  switch(hybrid_type) {
  case REAL_RESULT:
  {
    decimals=max(args[0]->decimals,args[1]->decimals)+prec_increment;
    set_if_smaller(decimals, NOT_FIXED_DEC);
    max_length=args[0]->max_length - args[0]->decimals + decimals;
    uint tmp=float_length(decimals);
    set_if_smaller(max_length,tmp);
    break;
  }
  case INT_RESULT:
    hybrid_type= DECIMAL_RESULT;
    DBUG_PRINT("info", ("Type changed: DECIMAL_RESULT"));
    result_precision();
    break;
  case DECIMAL_RESULT:
    result_precision();
    break;
  default:
    DBUG_ASSERT(0);
  }
  maybe_null= 1; // devision by zero
  DBUG_VOID_RETURN;
}


/* Integer division */
longlong Item_func_int_div::val_int()
{
  DBUG_ASSERT(fixed == 1);
  longlong value=args[0]->val_int();
  longlong val2=args[1]->val_int();
  if ((null_value= (args[0]->null_value || args[1]->null_value)))
    return 0;
  if (val2 == 0)
  {
    signal_divide_by_null();
    return 0;
  }
  return (unsigned_flag ?
	  (ulonglong) value / (ulonglong) val2 :
	  value / val2);
}


void Item_func_int_div::fix_length_and_dec()
{
  max_length=args[0]->max_length - args[0]->decimals;
  maybe_null=1;
  unsigned_flag=args[0]->unsigned_flag | args[1]->unsigned_flag;
}


longlong Item_func_mod::int_op()
{
  DBUG_ASSERT(fixed == 1);
  longlong value=  args[0]->val_int();
  longlong val2= args[1]->val_int();
  if ((null_value= args[0]->null_value || args[1]->null_value))
    return 0; /* purecov: inspected */
  if (val2 == 0)
  {
    signal_divide_by_null();
    return 0;
  }
  return value % val2;
}

double Item_func_mod::real_op()
{
  DBUG_ASSERT(fixed == 1);
  double value= args[0]->val_real();
  double val2=  args[1]->val_real();
  if ((null_value= args[0]->null_value || args[1]->null_value))
    return 0.0; /* purecov: inspected */
  if (val2 == 0.0)
  {
    signal_divide_by_null();
    return 0.0;
  }
  return fmod(value,val2);
}


my_decimal *Item_func_mod::decimal_op(my_decimal *decimal_value)
{
  my_decimal value1, *val1;
  my_decimal value2, *val2;

  val1= args[0]->val_decimal(&value1);
  if ((null_value= args[0]->null_value))
    return 0;
  val2= args[1]->val_decimal(&value2);
  if ((null_value= args[1]->null_value))
    return 0;
  switch (my_decimal_mod(E_DEC_FATAL_ERROR & ~E_DEC_DIV_ZERO, decimal_value,
                         val1, val2)) {
  case E_DEC_TRUNCATED:
  case E_DEC_OK:
    return decimal_value;
  case E_DEC_DIV_ZERO:
    signal_divide_by_null();
  default:
    null_value= 1;
    return 0;
  }
}


void Item_func_mod::result_precision()
{
  decimals= max(args[0]->decimals, args[1]->decimals);
  max_length= max(args[0]->max_length, args[1]->max_length);
}


double Item_func_neg::real_op()
{
  double value= args[0]->val_real();
  null_value= args[0]->null_value;
  return -value;
}


longlong Item_func_neg::int_op()
{
  longlong value= args[0]->val_int();
  null_value= args[0]->null_value;
  return -value;
}


my_decimal *Item_func_neg::decimal_op(my_decimal *decimal_value)
{
  my_decimal val, *value= args[0]->val_decimal(&val);
  if (!(null_value= args[0]->null_value))
  {
    my_decimal2decimal(value, decimal_value);
    my_decimal_neg(decimal_value);
    return decimal_value;
  }
  return 0;
}


void Item_func_neg::fix_num_length_and_dec()
{
  decimals= args[0]->decimals;
  /* 1 add because sign can appear */
  max_length= args[0]->max_length + 1;
}


void Item_func_neg::fix_length_and_dec()
{
  DBUG_ENTER("Item_func_neg::fix_length_and_dec");
  Item_func_num1::fix_length_and_dec();

  /*
    If this is in integer context keep the context as integer if possible
    (This is how multiplication and other integer functions works)
    Use val() to get value as arg_type doesn't mean that item is
    Item_int or Item_real due to existence of Item_param.
  */
  if (hybrid_type == INT_RESULT &&
      args[0]->type() == INT_ITEM &&
      ((ulonglong) args[0]->val_int() >= (ulonglong) LONGLONG_MIN))
  {
    /*
      Ensure that result is converted to DECIMAL, as longlong can't hold
      the negated number
    */
    hybrid_type= DECIMAL_RESULT;
    DBUG_PRINT("info", ("Type changed: DECIMAL_RESULT"));
  }
  unsigned_flag= 0;
  DBUG_VOID_RETURN;
}


double Item_func_abs::real_op()
{
  double value= args[0]->val_real();
  null_value= args[0]->null_value;
  return fabs(value);
}


longlong Item_func_abs::int_op()
{
  longlong value= args[0]->val_int();
  null_value= args[0]->null_value;
  return value >= 0 ? value : -value;
}


my_decimal *Item_func_abs::decimal_op(my_decimal *decimal_value)
{
  my_decimal val, *value= args[0]->val_decimal(&val);
  if (!(null_value= args[0]->null_value))
  {
    my_decimal2decimal(value, decimal_value);
    if (decimal_value->sign())
      my_decimal_neg(decimal_value);
    return decimal_value;
  }
  return 0;
}


void Item_func_abs::fix_length_and_dec()
{
  Item_func_num1::fix_length_and_dec();
}


/* Gateway to natural LOG function */
double Item_func_ln::val_real()
{
  DBUG_ASSERT(fixed == 1);
  double value= args[0]->val_real();
  if ((null_value=(args[0]->null_value || value <= 0.0)))
    return 0.0;
  return log(value);
}

/* 
 Extended but so slower LOG function
 We have to check if all values are > zero and first one is not one
 as these are the cases then result is not a number.
*/ 
double Item_func_log::val_real()
{
  DBUG_ASSERT(fixed == 1);
  double value= args[0]->val_real();
  if ((null_value=(args[0]->null_value || value <= 0.0)))
    return 0.0;
  if (arg_count == 2)
  {
    double value2= args[1]->val_real();
    if ((null_value=(args[1]->null_value || value2 <= 0.0 || value == 1.0)))
      return 0.0;
    return log(value2) / log(value);
  }
  return log(value);
}

double Item_func_log2::val_real()
{
  DBUG_ASSERT(fixed == 1);
  double value= args[0]->val_real();
  if ((null_value=(args[0]->null_value || value <= 0.0)))
    return 0.0;
  return log(value) / M_LN2;
}

double Item_func_log10::val_real()
{
  DBUG_ASSERT(fixed == 1);
  double value= args[0]->val_real();
  if ((null_value=(args[0]->null_value || value <= 0.0)))
    return 0.0; /* purecov: inspected */
  return log10(value);
}

double Item_func_exp::val_real()
{
  DBUG_ASSERT(fixed == 1);
  double value= args[0]->val_real();
  if ((null_value=args[0]->null_value))
    return 0.0; /* purecov: inspected */
  return exp(value);
}

double Item_func_sqrt::val_real()
{
  DBUG_ASSERT(fixed == 1);
  double value= args[0]->val_real();
  if ((null_value=(args[0]->null_value || value < 0)))
    return 0.0; /* purecov: inspected */
  return sqrt(value);
}

double Item_func_pow::val_real()
{
  DBUG_ASSERT(fixed == 1);
  double value= args[0]->val_real();
  double val2= args[1]->val_real();
  if ((null_value=(args[0]->null_value || args[1]->null_value)))
    return 0.0; /* purecov: inspected */
  return pow(value,val2);
}

// Trigonometric functions

double Item_func_acos::val_real()
{
  DBUG_ASSERT(fixed == 1);
  // the volatile's for BUG #2338 to calm optimizer down (because of gcc's bug)
  volatile double value= args[0]->val_real();
  if ((null_value=(args[0]->null_value || (value < -1.0 || value > 1.0))))
    return 0.0;
  return fix_result(acos(value));
}

double Item_func_asin::val_real()
{
  DBUG_ASSERT(fixed == 1);
  // the volatile's for BUG #2338 to calm optimizer down (because of gcc's bug)
  volatile double value= args[0]->val_real();
  if ((null_value=(args[0]->null_value || (value < -1.0 || value > 1.0))))
    return 0.0;
  return fix_result(asin(value));
}

double Item_func_atan::val_real()
{
  DBUG_ASSERT(fixed == 1);
  double value= args[0]->val_real();
  if ((null_value=args[0]->null_value))
    return 0.0;
  if (arg_count == 2)
  {
    double val2= args[1]->val_real();
    if ((null_value=args[1]->null_value))
      return 0.0;
    return fix_result(atan2(value,val2));
  }
  return fix_result(atan(value));
}

double Item_func_cos::val_real()
{
  DBUG_ASSERT(fixed == 1);
  double value= args[0]->val_real();
  if ((null_value=args[0]->null_value))
    return 0.0;
  return fix_result(cos(value));
}

double Item_func_sin::val_real()
{
  DBUG_ASSERT(fixed == 1);
  double value= args[0]->val_real();
  if ((null_value=args[0]->null_value))
    return 0.0;
  return fix_result(sin(value));
}

double Item_func_tan::val_real()
{
  DBUG_ASSERT(fixed == 1);
  double value= args[0]->val_real();
  if ((null_value=args[0]->null_value))
    return 0.0;
  return fix_result(tan(value));
}


// Shift-functions, same as << and >> in C/C++


longlong Item_func_shift_left::val_int()
{
  DBUG_ASSERT(fixed == 1);
  uint shift;
  ulonglong res= ((ulonglong) args[0]->val_int() <<
		  (shift=(uint) args[1]->val_int()));
  if (args[0]->null_value || args[1]->null_value)
  {
    null_value=1;
    return 0;
  }
  null_value=0;
  return (shift < sizeof(longlong)*8 ? (longlong) res : LL(0));
}

longlong Item_func_shift_right::val_int()
{
  DBUG_ASSERT(fixed == 1);
  uint shift;
  ulonglong res= (ulonglong) args[0]->val_int() >>
    (shift=(uint) args[1]->val_int());
  if (args[0]->null_value || args[1]->null_value)
  {
    null_value=1;
    return 0;
  }
  null_value=0;
  return (shift < sizeof(longlong)*8 ? (longlong) res : LL(0));
}


longlong Item_func_bit_neg::val_int()
{
  DBUG_ASSERT(fixed == 1);
  ulonglong res= (ulonglong) args[0]->val_int();
  if ((null_value=args[0]->null_value))
    return 0;
  return ~res;
}


// Conversion functions

void Item_func_integer::fix_length_and_dec()
{
  max_length=args[0]->max_length - args[0]->decimals+1;
  uint tmp=float_length(decimals);
  set_if_smaller(max_length,tmp);
  decimals=0;
}

void Item_func_int_val::fix_num_length_and_dec()
{
  max_length= args[0]->max_length - (args[0]->decimals ?
                                     args[0]->decimals + 1 :
                                     0) + 2;
  uint tmp= float_length(decimals);
  set_if_smaller(max_length,tmp);
  decimals= 0;
}


void Item_func_int_val::find_num_type()
{
  DBUG_ENTER("Item_func_int_val::find_num_type");
  DBUG_PRINT("info", ("name %s", func_name()));
  switch(hybrid_type= args[0]->result_type())
  {
  case STRING_RESULT:
  case REAL_RESULT:
    hybrid_type= REAL_RESULT;
    max_length= float_length(decimals);
    break;
  case INT_RESULT:
  case DECIMAL_RESULT:
    /*
      -2 because in most high position can't be used any digit for longlong
      and one position for increasing value during operation
    */
    if ((args[0]->max_length - args[0]->decimals) >=
        (DECIMAL_LONGLONG_DIGITS - 2))
    {
      hybrid_type= DECIMAL_RESULT;
    }
    else
    {
      unsigned_flag= args[0]->unsigned_flag;
      hybrid_type= INT_RESULT;
    }
    break;
  default:
    DBUG_ASSERT(0);
  }
  DBUG_PRINT("info", ("Type: %s",
                      (hybrid_type == REAL_RESULT ? "REAL_RESULT" :
                       hybrid_type == DECIMAL_RESULT ? "DECIMAL_RESULT" :
                       hybrid_type == INT_RESULT ? "INT_RESULT" :
                       "--ILLEGAL!!!--")));

  DBUG_VOID_RETURN;
}


longlong Item_func_ceiling::int_op()
{
  longlong result;
  switch (args[0]->result_type()) {
  case INT_RESULT:
    result= args[0]->val_int();
    null_value= args[0]->null_value;
    break;
  case DECIMAL_RESULT:
  {
    my_decimal dec_buf, *dec;
    if ((dec= Item_func_ceiling::decimal_op(&dec_buf)))
      my_decimal2int(E_DEC_FATAL_ERROR, dec, unsigned_flag, &result);
    else
      result= 0;
    break;
  }
  default:
    result= (longlong)Item_func_ceiling::real_op();
  };
  return result;
}


double Item_func_ceiling::real_op()
{
  /*
    the volatile's for BUG #3051 to calm optimizer down (because of gcc's
    bug)
  */
  volatile double value= args[0]->val_real();
  null_value= args[0]->null_value;
  return ceil(value);
}


my_decimal *Item_func_ceiling::decimal_op(my_decimal *decimal_value)
{
  my_decimal val, *value= args[0]->val_decimal(&val);
  if (!(null_value= (args[0]->null_value ||
                     my_decimal_ceiling(E_DEC_FATAL_ERROR, value,
                                        decimal_value) > 1)))
    return decimal_value;
  return 0;
}


longlong Item_func_floor::int_op()
{
  longlong result;
  switch (args[0]->result_type()) {
  case INT_RESULT:
    result= args[0]->val_int();
    null_value= args[0]->null_value;
    break;
  case DECIMAL_RESULT:
  {
    my_decimal dec_buf, *dec;
    if ((dec= Item_func_floor::decimal_op(&dec_buf)))
      my_decimal2int(E_DEC_FATAL_ERROR, dec, unsigned_flag, &result);
    else
      result= 0;
    break;
  }
  default:
    result= (longlong)Item_func_floor::real_op();
  };
  return result;
}


double Item_func_floor::real_op()
{
  /*
    the volatile's for BUG #3051 to calm optimizer down (because of gcc's
    bug)
  */
  volatile double value= args[0]->val_real();
  null_value= args[0]->null_value;
  return floor(value);
}


my_decimal *Item_func_floor::decimal_op(my_decimal *decimal_value)
{
  my_decimal val, *value= args[0]->val_decimal(&val);
  if (!(null_value= (args[0]->null_value ||
                     my_decimal_floor(E_DEC_FATAL_ERROR, value,
                                      decimal_value) > 1)))
    return decimal_value;
  return 0;
}


void Item_func_round::fix_length_and_dec()
{
  unsigned_flag= args[0]->unsigned_flag;
  if (!args[1]->const_item())
  {
    max_length= args[0]->max_length;
    decimals= args[0]->decimals;
    hybrid_type= REAL_RESULT;
    return;
  }
  
  int decimals_to_set= max((int)args[1]->val_int(), 0);
  if (args[0]->decimals == NOT_FIXED_DEC)
  {
    max_length= args[0]->max_length;
    decimals= min(decimals_to_set, NOT_FIXED_DEC);
    hybrid_type= REAL_RESULT;
    return;
  }
  
  switch (args[0]->result_type()) {
  case REAL_RESULT:
  case STRING_RESULT:
    hybrid_type= REAL_RESULT;
    decimals= min(decimals_to_set, NOT_FIXED_DEC);
    max_length= float_length(decimals);
    break;
  case INT_RESULT:
    if (!decimals_to_set &&
        (truncate || (args[0]->decimal_precision() < DECIMAL_LONGLONG_DIGITS)))
    {
      int length_can_increase= test(!truncate && (args[1]->val_int() < 0));
      max_length= args[0]->max_length + length_can_increase;
      /* Here we can keep INT_RESULT */
      hybrid_type= INT_RESULT;
      decimals= 0;
      break;
    }
    /* fall through */
  case DECIMAL_RESULT:
  {
    hybrid_type= DECIMAL_RESULT;
    int decimals_delta= args[0]->decimals - decimals_to_set;
    int precision= args[0]->decimal_precision();
    int length_increase= ((decimals_delta <= 0) || truncate) ? 0:1;

    precision-= decimals_delta - length_increase;
    decimals= decimals_to_set;
    max_length= my_decimal_precision_to_length(precision, decimals,
                                               unsigned_flag);
    break;
  }
  default:
    DBUG_ASSERT(0); /* This result type isn't handled */
  }
}

double my_double_round(double value, int dec, bool truncate)
{
  double tmp;
  uint abs_dec= abs(dec);
  /*
    tmp2 is here to avoid return the value with 80 bit precision
    This will fix that the test round(0.1,1) = round(0.1,1) is true
  */
  volatile double tmp2;

  tmp=(abs_dec < array_elements(log_10) ?
       log_10[abs_dec] : pow(10.0,(double) abs_dec));

  if (truncate)
  {
    if (value >= 0)
      tmp2= dec < 0 ? floor(value/tmp)*tmp : floor(value*tmp)/tmp;
    else
      tmp2= dec < 0 ? ceil(value/tmp)*tmp : ceil(value*tmp)/tmp;
  }
  else
    tmp2=dec < 0 ? rint(value/tmp)*tmp : rint(value*tmp)/tmp;
  return tmp2;
}


double Item_func_round::real_op()
{
  double value= args[0]->val_real();
  int dec= (int) args[1]->val_int();

  if (!(null_value= args[0]->null_value || args[1]->null_value))
    return my_double_round(value, dec, truncate);

  return 0.0;
}


longlong Item_func_round::int_op()
{
  longlong value= args[0]->val_int();
  int dec=(int) args[1]->val_int();
  decimals= 0;
  uint abs_dec;
  if ((null_value= args[0]->null_value || args[1]->null_value))
    return 0;
  if (dec >= 0)
    return value; // integer have not digits after point

  abs_dec= -dec;
  double tmp;
  /*
    tmp2 is here to avoid return the value with 80 bit precision
    This will fix that the test round(0.1,1) = round(0.1,1) is true
  */
  volatile double tmp2;

  tmp= (abs_dec < array_elements(log_10) ?
        log_10[abs_dec] : pow(10.0, (double) abs_dec));

  if (truncate)
  {
    if (unsigned_flag)
      tmp2= floor(ulonglong2double(value)/tmp)*tmp;
    else if (value >= 0)
      tmp2= floor(((double)value)/tmp)*tmp;
    else
      tmp2= ceil(((double)value)/tmp)*tmp;
  }
  else
    tmp2= rint(((double)value)/tmp)*tmp;
  return (longlong)tmp2;
}


my_decimal *Item_func_round::decimal_op(my_decimal *decimal_value)
{
  my_decimal val, *value= args[0]->val_decimal(&val);
  int dec=(int) args[1]->val_int();
  if (dec > 0)
  {
    decimals= min(dec, DECIMAL_MAX_SCALE); // to get correct output
  }
  if (!(null_value= (args[0]->null_value || args[1]->null_value ||
                     my_decimal_round(E_DEC_FATAL_ERROR, value, dec, truncate,
                                      decimal_value) > 1)))
    return decimal_value;
  return 0;
}


bool Item_func_rand::fix_fields(THD *thd,Item **ref)
{
  if (Item_real_func::fix_fields(thd, ref))
    return TRUE;
  used_tables_cache|= RAND_TABLE_BIT;
  if (arg_count)
  {					// Only use argument once in query
    if (!args[0]->const_during_execution())
    {
      my_error(ER_WRONG_ARGUMENTS, MYF(0), "RAND");
      return TRUE;
    }
    /*
      Allocate rand structure once: we must use thd->stmt_arena
      to create rand in proper mem_root if it's a prepared statement or
      stored procedure.

      No need to send a Rand log event if seed was given eg: RAND(seed),
      as it will be replicated in the query as such.
    */
    if (!rand && !(rand= (struct rand_struct*)
                   thd->stmt_arena->alloc(sizeof(*rand))))
      return TRUE;
    /*
      PARAM_ITEM is returned if we're in statement prepare and consequently
      no placeholder value is set yet.
    */
    if (args[0]->type() != PARAM_ITEM)
    {
      /*
        TODO: do not do reinit 'rand' for every execute of PS/SP if
        args[0] is a constant.
      */
      uint32 tmp= (uint32) args[0]->val_int();
      randominit(rand, (uint32) (tmp*0x10001L+55555555L),
                 (uint32) (tmp*0x10000001L));
    }
  }
  else
  {
    /*
      Save the seed only the first time RAND() is used in the query
      Once events are forwarded rather than recreated,
      the following can be skipped if inside the slave thread
    */
    if (!thd->rand_used)
    {
      thd->rand_used= 1;
      thd->rand_saved_seed1= thd->rand.seed1;
      thd->rand_saved_seed2= thd->rand.seed2;
    }
    rand= &thd->rand;
  }
  return FALSE;
}

void Item_func_rand::update_used_tables()
{
  Item_real_func::update_used_tables();
  used_tables_cache|= RAND_TABLE_BIT;
}


double Item_func_rand::val_real()
{
  DBUG_ASSERT(fixed == 1);
  return my_rnd(rand);
}

longlong Item_func_sign::val_int()
{
  DBUG_ASSERT(fixed == 1);
  double value= args[0]->val_real();
  null_value=args[0]->null_value;
  return value < 0.0 ? -1 : (value > 0 ? 1 : 0);
}


double Item_func_units::val_real()
{
  DBUG_ASSERT(fixed == 1);
  double value= args[0]->val_real();
  if ((null_value=args[0]->null_value))
    return 0;
  return value*mul+add;
}


void Item_func_min_max::fix_length_and_dec()
{
  int max_int_part=0;
  decimals=0;
  max_length=0;
  maybe_null=0;
  cmp_type=args[0]->result_type();

  for (uint i=0 ; i < arg_count ; i++)
  {
    set_if_bigger(max_length, args[i]->max_length);
    set_if_bigger(decimals, args[i]->decimals);
    set_if_bigger(max_int_part, args[i]->decimal_int_part());
    if (args[i]->maybe_null)
      maybe_null=1;
    cmp_type=item_cmp_type(cmp_type,args[i]->result_type());
  }
  if (cmp_type == STRING_RESULT)
    agg_arg_charsets(collation, args, arg_count, MY_COLL_CMP_CONV);
  else if ((cmp_type == DECIMAL_RESULT) || (cmp_type == INT_RESULT))
    max_length= my_decimal_precision_to_length(max_int_part+decimals, decimals,
                                            unsigned_flag);
}


String *Item_func_min_max::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  switch (cmp_type) {
  case INT_RESULT:
  {
    longlong nr=val_int();
    if (null_value)
      return 0;
    if (!unsigned_flag)
      str->set(nr,&my_charset_bin);
    else
      str->set((ulonglong) nr,&my_charset_bin);
    return str;
  }
  case DECIMAL_RESULT:
  {
    my_decimal dec_buf, *dec_val= val_decimal(&dec_buf);
    if (null_value)
      return 0;
    my_decimal2string(E_DEC_FATAL_ERROR, dec_val, 0, 0, 0, str);
    return str;
  }
  case REAL_RESULT:
  {
    double nr= val_real();
    if (null_value)
      return 0; /* purecov: inspected */
    str->set(nr,decimals,&my_charset_bin);
    return str;
  }
  case STRING_RESULT:
  {
    String *res;
    LINT_INIT(res);
    for (uint i=0; i < arg_count ; i++)
    {
      if (i == 0)
	res=args[i]->val_str(str);
      else
      {
	String *res2;
	res2= args[i]->val_str(res == str ? &tmp_value : str);
	if (res2)
	{
	  int cmp= sortcmp(res,res2,collation.collation);
	  if ((cmp_sign < 0 ? cmp : -cmp) < 0)
	    res=res2;
	}
      }
      if ((null_value= args[i]->null_value))
        return 0;
    }
    res->set_charset(collation.collation);
    return res;
  }
  case ROW_RESULT:
  default:
    // This case should never be chosen
    DBUG_ASSERT(0);
    return 0;
  }
  return 0;					// Keep compiler happy
}


double Item_func_min_max::val_real()
{
  DBUG_ASSERT(fixed == 1);
  double value=0.0;
  for (uint i=0; i < arg_count ; i++)
  {
    if (i == 0)
      value= args[i]->val_real();
    else
    {
      double tmp= args[i]->val_real();
      if (!args[i]->null_value && (tmp < value ? cmp_sign : -cmp_sign) > 0)
	value=tmp;
    }
    if ((null_value= args[i]->null_value))
      break;
  }
  return value;
}


longlong Item_func_min_max::val_int()
{
  DBUG_ASSERT(fixed == 1);
  longlong value=0;
  for (uint i=0; i < arg_count ; i++)
  {
    if (i == 0)
      value=args[i]->val_int();
    else
    {
      longlong tmp=args[i]->val_int();
      if (!args[i]->null_value && (tmp < value ? cmp_sign : -cmp_sign) > 0)
	value=tmp;
    }
    if ((null_value= args[i]->null_value))
      break;
  }
  return value;
}


my_decimal *Item_func_min_max::val_decimal(my_decimal *dec)
{
  DBUG_ASSERT(fixed == 1);
  my_decimal tmp_buf, *tmp, *res;
  LINT_INIT(res);

  for (uint i=0; i < arg_count ; i++)
  {
    if (i == 0)
      res= args[i]->val_decimal(dec);
    else
    {
      tmp= args[i]->val_decimal(&tmp_buf);      // Zero if NULL
      if (tmp && (my_decimal_cmp(tmp, res) * cmp_sign) < 0)
      {
        if (tmp == &tmp_buf)
        {
          /* Move value out of tmp_buf as this will be reused on next loop */
          my_decimal2decimal(tmp, dec);
          res= dec;
        }
        else
          res= tmp;
      }
    }
    if ((null_value= args[i]->null_value))
    {
      res= 0;
      break;
    }
  }
  return res;
}


longlong Item_func_length::val_int()
{
  DBUG_ASSERT(fixed == 1);
  String *res=args[0]->val_str(&value);
  if (!res)
  {
    null_value=1;
    return 0; /* purecov: inspected */
  }
  null_value=0;
  return (longlong) res->length();
}


longlong Item_func_char_length::val_int()
{
  DBUG_ASSERT(fixed == 1);
  String *res=args[0]->val_str(&value);
  if (!res)
  {
    null_value=1;
    return 0; /* purecov: inspected */
  }
  null_value=0;
  return (longlong) res->numchars();
}


longlong Item_func_coercibility::val_int()
{
  DBUG_ASSERT(fixed == 1);
  null_value= 0;
  return (longlong) args[0]->collation.derivation;
}


void Item_func_locate::fix_length_and_dec()
{
  maybe_null=0; max_length=11;
  agg_arg_charsets(cmp_collation, args, 2, MY_COLL_CMP_CONV);
}


longlong Item_func_locate::val_int()
{
  DBUG_ASSERT(fixed == 1);
  String *a=args[0]->val_str(&value1);
  String *b=args[1]->val_str(&value2);
  if (!a || !b)
  {
    null_value=1;
    return 0; /* purecov: inspected */
  }
  null_value=0;
  uint start=0;
  uint start0=0;
  my_match_t match;

  if (arg_count == 3)
  {
    start0= start =(uint) args[2]->val_int()-1;
    start=a->charpos(start);
    
    if (start > a->length() || start+b->length() > a->length())
      return 0;
  }

  if (!b->length())				// Found empty string at start
    return (longlong) (start+1);
  
  if (!cmp_collation.collation->coll->instr(cmp_collation.collation,
                                            a->ptr()+start, a->length()-start,
                                            b->ptr(), b->length(),
                                            &match, 1))
    return 0;
  return (longlong) match.mblen + start0 + 1;
}


void Item_func_locate::print(String *str)
{
  str->append("locate(", 7);
  args[1]->print(str);
  str->append(',');
  args[0]->print(str);
  if (arg_count == 3)
  {
    str->append(',');
    args[2]->print(str);
  }
  str->append(')');
}


longlong Item_func_field::val_int()
{
  DBUG_ASSERT(fixed == 1);

  if (cmp_type == STRING_RESULT)
  {
    String *field;
    if (!(field= args[0]->val_str(&value)))
      return 0;
    for (uint i=1 ; i < arg_count ; i++)
    {
      String *tmp_value=args[i]->val_str(&tmp);
      if (tmp_value && !sortcmp(field,tmp_value,cmp_collation.collation))
        return (longlong) (i);
    }
  }
  else if (cmp_type == INT_RESULT)
  {
    longlong val= args[0]->val_int();
    if (args[0]->null_value)
      return 0;
    for (uint i=1; i < arg_count ; i++)
    {
      if (val == args[i]->val_int() && !args[i]->null_value)
        return (longlong) (i);
    }
  }
  else if (cmp_type == DECIMAL_RESULT)
  {
    my_decimal dec_arg_buf, *dec_arg,
               dec_buf, *dec= args[0]->val_decimal(&dec_buf);
    if (args[0]->null_value)
      return 0;
    for (uint i=1; i < arg_count; i++)
    {
      dec_arg= args[i]->val_decimal(&dec_arg_buf);
      if (!args[i]->null_value && !my_decimal_cmp(dec_arg, dec))
        return (longlong) (i);
    }
  }
  else
  {
    double val= args[0]->val_real();
    if (args[0]->null_value)
      return 0;
    for (uint i=1; i < arg_count ; i++)
    {
      if (val == args[i]->val_real() && !args[i]->null_value)
        return (longlong) (i);
    }
  }
  return 0;
}


void Item_func_field::fix_length_and_dec()
{
  maybe_null=0; max_length=3;
  cmp_type= args[0]->result_type();
  for (uint i=1; i < arg_count ; i++)
    cmp_type= item_cmp_type(cmp_type, args[i]->result_type());
  if (cmp_type == STRING_RESULT)
    agg_arg_charsets(cmp_collation, args, arg_count, MY_COLL_CMP_CONV);
}


longlong Item_func_ascii::val_int()
{
  DBUG_ASSERT(fixed == 1);
  String *res=args[0]->val_str(&value);
  if (!res)
  {
    null_value=1;
    return 0;
  }
  null_value=0;
  return (longlong) (res->length() ? (uchar) (*res)[0] : (uchar) 0);
}

longlong Item_func_ord::val_int()
{
  DBUG_ASSERT(fixed == 1);
  String *res=args[0]->val_str(&value);
  if (!res)
  {
    null_value=1;
    return 0;
  }
  null_value=0;
  if (!res->length()) return 0;
#ifdef USE_MB
  if (use_mb(res->charset()))
  {
    register const char *str=res->ptr();
    register uint32 n=0, l=my_ismbchar(res->charset(),str,str+res->length());
    if (!l)
      return (longlong)((uchar) *str);
    while (l--)
      n=(n<<8)|(uint32)((uchar) *str++);
    return (longlong) n;
  }
#endif
  return (longlong) ((uchar) (*res)[0]);
}

	/* Search after a string in a string of strings separated by ',' */
	/* Returns number of found type >= 1 or 0 if not found */
	/* This optimizes searching in enums to bit testing! */

void Item_func_find_in_set::fix_length_and_dec()
{
  decimals=0;
  max_length=3;					// 1-999
  if (args[0]->const_item() && args[1]->type() == FIELD_ITEM)
  {
    Field *field= ((Item_field*) args[1])->field;
    if (field->real_type() == FIELD_TYPE_SET)
    {
      String *find=args[0]->val_str(&value);
      if (find)
      {
	enum_value= find_type(((Field_enum*) field)->typelib,find->ptr(),
			      find->length(), 0);
	enum_bit=0;
	if (enum_value)
	  enum_bit=LL(1) << (enum_value-1);
      }
    }
  }
  agg_arg_charsets(cmp_collation, args, 2, MY_COLL_CMP_CONV);
}

static const char separator=',';

longlong Item_func_find_in_set::val_int()
{
  DBUG_ASSERT(fixed == 1);
  if (enum_value)
  {
    ulonglong tmp=(ulonglong) args[1]->val_int();
    if (!(null_value=args[1]->null_value || args[0]->null_value))
    {
      if (tmp & enum_bit)
	return enum_value;
    }
    return 0L;
  }

  String *find=args[0]->val_str(&value);
  String *buffer=args[1]->val_str(&value2);
  if (!find || !buffer)
  {
    null_value=1;
    return 0; /* purecov: inspected */
  }
  null_value=0;

  int diff;
  if ((diff=buffer->length() - find->length()) >= 0)
  {
    my_wc_t wc;
    CHARSET_INFO *cs= cmp_collation.collation;
    const char *str_begin= buffer->ptr();
    const char *str_end= buffer->ptr();
    const char *real_end= str_end+buffer->length();
    const uchar *find_str= (const uchar *) find->ptr();
    uint find_str_len= find->length();
    int position= 0;
    while (1)
    {
      int symbol_len;
      if ((symbol_len= cs->cset->mb_wc(cs, &wc, (uchar*) str_end, 
                                       (uchar*) real_end)) > 0)
      {
        const char *substr_end= str_end + symbol_len;
        bool is_last_item= (substr_end == real_end);
        bool is_separator= (wc == (my_wc_t) separator);
        if (is_separator || is_last_item)
        {
          position++;
          if (is_last_item && !is_separator)
            str_end= substr_end;
          if (!my_strnncoll(cs, (const uchar *) str_begin,
                            str_end - str_begin,
                            find_str, find_str_len))
            return (longlong) position;
          else
            str_begin= substr_end;
        }
        str_end= substr_end;
      }
      else if (str_end - str_begin == 0 &&
               find_str_len == 0 &&
               wc == (my_wc_t) separator)
        return (longlong) ++position;
      else
        return LL(0);
    }
  }
  return 0;
}

longlong Item_func_bit_count::val_int()
{
  DBUG_ASSERT(fixed == 1);
  ulonglong value= (ulonglong) args[0]->val_int();
  if (args[0]->null_value)
  {
    null_value=1; /* purecov: inspected */
    return 0; /* purecov: inspected */
  }
  return (longlong) my_count_bits(value);
}


/****************************************************************************
** Functions to handle dynamic loadable functions
** Original source by: Alexis Mikhailov <root@medinf.chuvashia.su>
** Rewritten by monty.
****************************************************************************/

#ifdef HAVE_DLOPEN

void udf_handler::cleanup()
{
  if (!not_original)
  {
    if (initialized)
    {
      if (u_d->func_deinit != NULL)
      {
        void (*deinit)(UDF_INIT *) = (void (*)(UDF_INIT*))
        u_d->func_deinit;
        (*deinit)(&initid);
      }
      free_udf(u_d);
      initialized= FALSE;
    }
    if (buffers)				// Because of bug in ecc
      delete [] buffers;
    buffers= 0;
  }
}


bool
udf_handler::fix_fields(THD *thd, Item_result_field *func,
			uint arg_count, Item **arguments)
{
#ifndef EMBEDDED_LIBRARY			// Avoid compiler warning
  char buff[STACK_BUFF_ALLOC];			// Max argument in function
#endif
  DBUG_ENTER("Item_udf_func::fix_fields");

  if (check_stack_overrun(thd, STACK_MIN_SIZE, buff))
    DBUG_RETURN(TRUE);				// Fatal error flag is set!

  udf_func *tmp_udf=find_udf(u_d->name.str,(uint) u_d->name.length,1);

  if (!tmp_udf)
  {
    my_error(ER_CANT_FIND_UDF, MYF(0), u_d->name.str, errno);
    DBUG_RETURN(TRUE);
  }
  u_d=tmp_udf;
  args=arguments;

  /* Fix all arguments */
  func->maybe_null=0;
  used_tables_cache=0;
  const_item_cache=1;

  if ((f_args.arg_count=arg_count))
  {
    if (!(f_args.arg_type= (Item_result*)
	  sql_alloc(f_args.arg_count*sizeof(Item_result))))

    {
      free_udf(u_d);
      DBUG_RETURN(TRUE);
    }
    uint i;
    Item **arg,**arg_end;
    for (i=0, arg=arguments, arg_end=arguments+arg_count;
	 arg != arg_end ;
	 arg++,i++)
    {
      if (!(*arg)->fixed &&
          (*arg)->fix_fields(thd, arg))
	DBUG_RETURN(1);
      // we can't assign 'item' before, because fix_fields() can change arg
      Item *item= *arg;
      if (item->check_cols(1))
	DBUG_RETURN(TRUE);
      /*
	TODO: We should think about this. It is not always
	right way just to set an UDF result to return my_charset_bin
	if one argument has binary sorting order.
	The result collation should be calculated according to arguments
	derivations in some cases and should not in other cases.
	Moreover, some arguments can represent a numeric input
	which doesn't effect the result character set and collation.
	There is no a general rule for UDF. Everything depends on
        the particular user defined function.
      */
      if (item->collation.collation->state & MY_CS_BINSORT)
	func->collation.set(&my_charset_bin);
      if (item->maybe_null)
	func->maybe_null=1;
      func->with_sum_func= func->with_sum_func || item->with_sum_func;
      used_tables_cache|=item->used_tables();
      const_item_cache&=item->const_item();
      f_args.arg_type[i]=item->result_type();
    }
    //TODO: why all following memory is not allocated with 1 call of sql_alloc?
    if (!(buffers=new String[arg_count]) ||
	!(f_args.args= (char**) sql_alloc(arg_count * sizeof(char *))) ||
	!(f_args.lengths= (ulong*) sql_alloc(arg_count * sizeof(long))) ||
	!(f_args.maybe_null= (char*) sql_alloc(arg_count * sizeof(char))) ||
	!(num_buffer= (char*) sql_alloc(arg_count *
					ALIGN_SIZE(sizeof(double)))) ||
	!(f_args.attributes= (char**) sql_alloc(arg_count * sizeof(char *))) ||
	!(f_args.attribute_lengths= (ulong*) sql_alloc(arg_count *
						       sizeof(long))))
    {
      free_udf(u_d);
      DBUG_RETURN(TRUE);
    }
  }
  func->fix_length_and_dec();
  initid.max_length=func->max_length;
  initid.maybe_null=func->maybe_null;
  initid.const_item=const_item_cache;
  initid.decimals=func->decimals;
  initid.ptr=0;

  if (u_d->func_init)
  {
    char *to=num_buffer;
    for (uint i=0; i < arg_count; i++)
    {
      f_args.args[i]=0;
      f_args.lengths[i]= arguments[i]->max_length;
      f_args.maybe_null[i]= (char) arguments[i]->maybe_null;
      f_args.attributes[i]= arguments[i]->name;
      f_args.attribute_lengths[i]= arguments[i]->name_length;

      switch(arguments[i]->type()) {
      case Item::STRING_ITEM:			// Constant string !
      {
	String *res=arguments[i]->val_str((String *) 0);
	if (arguments[i]->null_value)
	  continue;
	f_args.args[i]=    (char*) res->ptr();
	break;
      }
      case Item::INT_ITEM:
	*((longlong*) to) = arguments[i]->val_int();
	if (!arguments[i]->null_value)
	{
	  f_args.args[i]=to;
	  to+= ALIGN_SIZE(sizeof(longlong));
	}
	break;
      case Item::REAL_ITEM:
	*((double*) to)= arguments[i]->val_real();
	if (!arguments[i]->null_value)
	{
	  f_args.args[i]=to;
	  to+= ALIGN_SIZE(sizeof(double));
	}
	break;
      default:					// Skip these
	break;
      }
    }
    thd->net.last_error[0]=0;
    my_bool (*init)(UDF_INIT *, UDF_ARGS *, char *)=
      (my_bool (*)(UDF_INIT *, UDF_ARGS *,  char *))
      u_d->func_init;
    if ((error=(uchar) init(&initid, &f_args, thd->net.last_error)))
    {
      my_error(ER_CANT_INITIALIZE_UDF, MYF(0),
               u_d->name.str, thd->net.last_error);
      free_udf(u_d);
      DBUG_RETURN(TRUE);
    }
    func->max_length=min(initid.max_length,MAX_BLOB_WIDTH);
    func->maybe_null=initid.maybe_null;
    const_item_cache=initid.const_item;
    func->decimals=min(initid.decimals,NOT_FIXED_DEC);
  }
  initialized=1;
  if (error)
  {
    my_error(ER_CANT_INITIALIZE_UDF, MYF(0),
             u_d->name.str, ER(ER_UNKNOWN_ERROR));
    DBUG_RETURN(TRUE);
  }
  DBUG_RETURN(FALSE);
}


bool udf_handler::get_arguments()
{
  if (error)
    return 1;					// Got an error earlier
  char *to= num_buffer;
  uint str_count=0;
  for (uint i=0; i < f_args.arg_count; i++)
  {
    f_args.args[i]=0;
    switch (f_args.arg_type[i]) {
    case STRING_RESULT:
    case DECIMAL_RESULT:
      {
	String *res=args[i]->val_str(&buffers[str_count++]);
	if (!(args[i]->null_value))
	{
	  f_args.args[i]=    (char*) res->ptr();
	  f_args.lengths[i]= res->length();
	  break;
	}
      }
    case INT_RESULT:
      *((longlong*) to) = args[i]->val_int();
      if (!args[i]->null_value)
      {
	f_args.args[i]=to;
	to+= ALIGN_SIZE(sizeof(longlong));
      }
      break;
    case REAL_RESULT:
      *((double*) to)= args[i]->val_real();
      if (!args[i]->null_value)
      {
	f_args.args[i]=to;
	to+= ALIGN_SIZE(sizeof(double));
      }
      break;
    case ROW_RESULT:
    default:
      // This case should never be chosen
      DBUG_ASSERT(0);
      break;
    }
  }
  return 0;
}

/* This returns (String*) 0 in case of NULL values */

String *udf_handler::val_str(String *str,String *save_str)
{
  uchar is_null_tmp=0;
  ulong res_length;

  if (get_arguments())
    return 0;
  char * (*func)(UDF_INIT *, UDF_ARGS *, char *, ulong *, uchar *, uchar *)=
    (char* (*)(UDF_INIT *, UDF_ARGS *, char *, ulong *, uchar *, uchar *))
    u_d->func;

  if ((res_length=str->alloced_length()) < MAX_FIELD_WIDTH)
  {						// This happens VERY seldom
    if (str->alloc(MAX_FIELD_WIDTH))
    {
      error=1;
      return 0;
    }
  }
  char *res=func(&initid, &f_args, (char*) str->ptr(), &res_length,
		 &is_null_tmp, &error);
  if (is_null_tmp || !res || error)		// The !res is for safety
  {
    return 0;
  }
  if (res == str->ptr())
  {
    str->length(res_length);
    return str;
  }
  save_str->set(res, res_length, str->charset());
  return save_str;
}


/*
  For the moment, UDF functions are returning DECIMAL values as strings
*/

my_decimal *udf_handler::val_decimal(my_bool *null_value, my_decimal *dec_buf)
{
  char buf[DECIMAL_MAX_STR_LENGTH+1], *end;
  ulong res_length= DECIMAL_MAX_STR_LENGTH;

  if (get_arguments())
  {
    *null_value=1;
    return 0;
  }
  char *(*func)(UDF_INIT *, UDF_ARGS *, char *, ulong *, uchar *, uchar *)=
    (char* (*)(UDF_INIT *, UDF_ARGS *, char *, ulong *, uchar *, uchar *))
    u_d->func;

  char *res= func(&initid, &f_args, buf, &res_length, &is_null, &error);
  if (is_null || error)
  {
    *null_value= 1;
    return 0;
  }
  end= res+ res_length;
  str2my_decimal(E_DEC_FATAL_ERROR, res, dec_buf, &end);
  return dec_buf;
}


void Item_udf_func::cleanup()
{
  udf.cleanup();
  Item_func::cleanup();
}


double Item_func_udf_float::val_real()
{
  DBUG_ASSERT(fixed == 1);
  DBUG_ENTER("Item_func_udf_float::val");
  DBUG_PRINT("info",("result_type: %d  arg_count: %d",
		     args[0]->result_type(), arg_count));
  DBUG_RETURN(udf.val(&null_value));
}


String *Item_func_udf_float::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  double nr= val_real();
  if (null_value)
    return 0;					/* purecov: inspected */
  str->set(nr,decimals,&my_charset_bin);
  return str;
}


longlong Item_func_udf_int::val_int()
{
  DBUG_ASSERT(fixed == 1);
  DBUG_ENTER("Item_func_udf_int::val_int");
  DBUG_PRINT("info",("result_type: %d  arg_count: %d",
		     args[0]->result_type(), arg_count));

  DBUG_RETURN(udf.val_int(&null_value));
}


String *Item_func_udf_int::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  longlong nr=val_int();
  if (null_value)
    return 0;
  if (!unsigned_flag)
    str->set(nr,&my_charset_bin);
  else
    str->set((ulonglong) nr,&my_charset_bin);
  return str;
}


longlong Item_func_udf_decimal::val_int()
{
  my_decimal dec_buf, *dec= udf.val_decimal(&null_value, &dec_buf);
  longlong result;
  if (null_value)
    return 0;
  my_decimal2int(E_DEC_FATAL_ERROR, dec, unsigned_flag, &result);
  return result;
}


double Item_func_udf_decimal::val_real()
{
  my_decimal dec_buf, *dec= udf.val_decimal(&null_value, &dec_buf);
  double result;
  if (null_value)
    return 0.0;
  my_decimal2double(E_DEC_FATAL_ERROR, dec, &result);
  return result;
}


my_decimal *Item_func_udf_decimal::val_decimal(my_decimal *dec_buf)
{
  DBUG_ASSERT(fixed == 1);
  DBUG_ENTER("Item_func_udf_decimal::val_decimal");
  DBUG_PRINT("info",("result_type: %d  arg_count: %d",
                     args[0]->result_type(), arg_count));

  DBUG_RETURN(udf.val_decimal(&null_value, dec_buf));
}


String *Item_func_udf_decimal::val_str(String *str)
{
  my_decimal dec_buf, *dec= udf.val_decimal(&null_value, &dec_buf);
  if (null_value)
    return 0;
  if (str->length() < DECIMAL_MAX_STR_LENGTH)
    str->length(DECIMAL_MAX_STR_LENGTH);
  my_decimal_round(E_DEC_FATAL_ERROR, dec, decimals, FALSE, &dec_buf);
  my_decimal2string(E_DEC_FATAL_ERROR, &dec_buf, 0, 0, '0', str);
  return str;
}


void Item_func_udf_decimal::fix_length_and_dec()
{
  fix_num_length_and_dec();
}


/* Default max_length is max argument length */

void Item_func_udf_str::fix_length_and_dec()
{
  DBUG_ENTER("Item_func_udf_str::fix_length_and_dec");
  max_length=0;
  for (uint i = 0; i < arg_count; i++)
    set_if_bigger(max_length,args[i]->max_length);
  DBUG_VOID_RETURN;
}

String *Item_func_udf_str::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  String *res=udf.val_str(str,&str_value);
  null_value = !res;
  return res;
}


/*
   This has to come last in the udf_handler methods, or C for AIX
   version 6.0.0.0 fails to compile with debugging enabled. (Yes, really.)
 */

udf_handler::~udf_handler()
{
  /* Everything should be properly cleaned up by this moment. */
  DBUG_ASSERT(not_original || !(initialized || buffers));
}

#else
bool udf_handler::get_arguments() { return 0; }
#endif /* HAVE_DLOPEN */

/*
** User level locks
*/

pthread_mutex_t LOCK_user_locks;
static HASH hash_user_locks;

class User_level_lock
{
  char *key;
  uint key_length;

public:
  int count;
  bool locked;
  pthread_cond_t cond;
  pthread_t thread;
  ulong thread_id;

  User_level_lock(const char *key_arg,uint length, ulong id) 
    :key_length(length),count(1),locked(1), thread_id(id)
  {
    key=(char*) my_memdup((byte*) key_arg,length,MYF(0));
    pthread_cond_init(&cond,NULL);
    if (key)
    {
      if (my_hash_insert(&hash_user_locks,(byte*) this))
      {
	my_free((gptr) key,MYF(0));
	key=0;
      }
    }
  }
  ~User_level_lock()
  {
    if (key)
    {
      hash_delete(&hash_user_locks,(byte*) this);
      my_free((gptr) key,MYF(0));
    }
    pthread_cond_destroy(&cond);
  }
  inline bool initialized() { return key != 0; }
  friend void item_user_lock_release(User_level_lock *ull);
  friend char *ull_get_key(const User_level_lock *ull, uint *length,
                           my_bool not_used);
};

char *ull_get_key(const User_level_lock *ull, uint *length,
		  my_bool not_used __attribute__((unused)))
{
  *length=(uint) ull->key_length;
  return (char*) ull->key;
}


static bool item_user_lock_inited= 0;

void item_user_lock_init(void)
{
  pthread_mutex_init(&LOCK_user_locks,MY_MUTEX_INIT_SLOW);
  hash_init(&hash_user_locks,system_charset_info,
	    16,0,0,(hash_get_key) ull_get_key,NULL,0);
  item_user_lock_inited= 1;
}

void item_user_lock_free(void)
{
  if (item_user_lock_inited)
  {
    item_user_lock_inited= 0;
    hash_free(&hash_user_locks);
    pthread_mutex_destroy(&LOCK_user_locks);
  }
}

void item_user_lock_release(User_level_lock *ull)
{
  ull->locked=0;
  if (--ull->count)
    pthread_cond_signal(&ull->cond);
  else
    delete ull;
}

/*
   Wait until we are at or past the given position in the master binlog
   on the slave
 */

longlong Item_master_pos_wait::val_int()
{
  DBUG_ASSERT(fixed == 1);
  THD* thd = current_thd;
  String *log_name = args[0]->val_str(&value);
  int event_count= 0;

  null_value=0;
  if (thd->slave_thread || !log_name || !log_name->length())
  {
    null_value = 1;
    return 0;
  }
  longlong pos = (ulong)args[1]->val_int();
  longlong timeout = (arg_count==3) ? args[2]->val_int() : 0 ;
#ifdef HAVE_REPLICATION
  if ((event_count = active_mi->rli.wait_for_pos(thd, log_name, pos, timeout)) == -2)
  {
    null_value = 1;
    event_count=0;
  }
#endif
  return event_count;
}

#ifdef EXTRA_DEBUG
void debug_sync_point(const char* lock_name, uint lock_timeout)
{
  THD* thd=current_thd;
  User_level_lock* ull;
  struct timespec abstime;
  int lock_name_len;
  lock_name_len=strlen(lock_name);
  pthread_mutex_lock(&LOCK_user_locks);

  if (thd->ull)
  {
    item_user_lock_release(thd->ull);
    thd->ull=0;
  }

  /*
    If the lock has not been aquired by some client, we do not want to
    create an entry for it, since we immediately release the lock. In
    this case, we will not be waiting, but rather, just waste CPU and
    memory on the whole deal
  */
  if (!(ull= ((User_level_lock*) hash_search(&hash_user_locks, lock_name,
				 lock_name_len))))
  {
    pthread_mutex_unlock(&LOCK_user_locks);
    return;
  }
  ull->count++;

  /*
    Structure is now initialized.  Try to get the lock.
    Set up control struct to allow others to abort locks
  */
  thd->proc_info="User lock";
  thd->mysys_var->current_mutex= &LOCK_user_locks;
  thd->mysys_var->current_cond=  &ull->cond;

  set_timespec(abstime,lock_timeout);
  while (ull->locked && !thd->killed)
  {
    int error= pthread_cond_timedwait(&ull->cond, &LOCK_user_locks, &abstime);
    if (error == ETIMEDOUT || error == ETIME)
      break;
  }

  if (ull->locked)
  {
    if (!--ull->count)
      delete ull;				// Should never happen
  }
  else
  {
    ull->locked=1;
    ull->thread=thd->real_id;
    thd->ull=ull;
  }
  pthread_mutex_unlock(&LOCK_user_locks);
  pthread_mutex_lock(&thd->mysys_var->mutex);
  thd->proc_info=0;
  thd->mysys_var->current_mutex= 0;
  thd->mysys_var->current_cond=  0;
  pthread_mutex_unlock(&thd->mysys_var->mutex);
  pthread_mutex_lock(&LOCK_user_locks);
  if (thd->ull)
  {
    item_user_lock_release(thd->ull);
    thd->ull=0;
  }
  pthread_mutex_unlock(&LOCK_user_locks);
}

#endif

/*
  Get a user level lock. If the thread has an old lock this is first released.
  Returns 1:  Got lock
  Returns 0:  Timeout
  Returns NULL: Error
*/

longlong Item_func_get_lock::val_int()
{
  DBUG_ASSERT(fixed == 1);
  String *res=args[0]->val_str(&value);
  longlong timeout=args[1]->val_int();
  struct timespec abstime;
  THD *thd=current_thd;
  User_level_lock *ull;
  int error;

  /*
    In slave thread no need to get locks, everything is serialized. Anyway
    there is no way to make GET_LOCK() work on slave like it did on master
    (i.e. make it return exactly the same value) because we don't have the
    same other concurrent threads environment. No matter what we return here,
    it's not guaranteed to be same as on master.
  */
  if (thd->slave_thread)
    return 1;

  pthread_mutex_lock(&LOCK_user_locks);

  if (!res || !res->length())
  {
    pthread_mutex_unlock(&LOCK_user_locks);
    null_value=1;
    return 0;
  }
  null_value=0;

  if (thd->ull)
  {
    item_user_lock_release(thd->ull);
    thd->ull=0;
  }

  if (!(ull= ((User_level_lock *) hash_search(&hash_user_locks,
                                              (byte*) res->ptr(),
                                              res->length()))))
  {
    ull=new User_level_lock(res->ptr(),res->length(), thd->thread_id);
    if (!ull || !ull->initialized())
    {
      delete ull;
      pthread_mutex_unlock(&LOCK_user_locks);
      null_value=1;				// Probably out of memory
      return 0;
    }
    ull->thread=thd->real_id;
    thd->ull=ull;
    pthread_mutex_unlock(&LOCK_user_locks);
    return 1;					// Got new lock
  }
  ull->count++;

  /*
    Structure is now initialized.  Try to get the lock.
    Set up control struct to allow others to abort locks.
  */
  thd->proc_info="User lock";
  thd->mysys_var->current_mutex= &LOCK_user_locks;
  thd->mysys_var->current_cond=  &ull->cond;

  set_timespec(abstime,timeout);
  error= 0;
  while (ull->locked && !thd->killed)
  {
    error= pthread_cond_timedwait(&ull->cond,&LOCK_user_locks,&abstime);
    if (error == ETIMEDOUT || error == ETIME)
      break;
    error= 0;
  }

  if (ull->locked)
  {
    if (!--ull->count)
    {
      DBUG_ASSERT(0);
      delete ull;				// Should never happen
    }
    if (!error)                                 // Killed (thd->killed != 0)
    {
      error=1;
      null_value=1;				// Return NULL
    }
  }
  else                                          // We got the lock
  {
    ull->locked=1;
    ull->thread=thd->real_id;
    thd->ull=ull;
    error=0;
  }
  pthread_mutex_unlock(&LOCK_user_locks);

  pthread_mutex_lock(&thd->mysys_var->mutex);
  thd->proc_info=0;
  thd->mysys_var->current_mutex= 0;
  thd->mysys_var->current_cond=  0;
  pthread_mutex_unlock(&thd->mysys_var->mutex);

  return !error ? 1 : 0;
}


/*
  Release a user level lock.
  Return:
    1 if lock released
    0 if lock wasn't held
    (SQL) NULL if no such lock
*/

longlong Item_func_release_lock::val_int()
{
  DBUG_ASSERT(fixed == 1);
  String *res=args[0]->val_str(&value);
  User_level_lock *ull;
  longlong result;
  if (!res || !res->length())
  {
    null_value=1;
    return 0;
  }
  null_value=0;

  result=0;
  pthread_mutex_lock(&LOCK_user_locks);
  if (!(ull= ((User_level_lock*) hash_search(&hash_user_locks,
                                             (const byte*) res->ptr(),
                                             res->length()))))
  {
    null_value=1;
  }
  else
  {
    if (ull->locked && pthread_equal(pthread_self(),ull->thread))
    {
      result=1;					// Release is ok
      item_user_lock_release(ull);
      current_thd->ull=0;
    }
  }
  pthread_mutex_unlock(&LOCK_user_locks);
  return result;
}


longlong Item_func_last_insert_id::val_int()
{
  THD *thd= current_thd;
  DBUG_ASSERT(fixed == 1);
  if (arg_count)
  {
    longlong value= args[0]->val_int();
    thd->insert_id(value);
    null_value= args[0]->null_value;
    return value;                       // Avoid side effect of insert_id()
  }
  thd->lex->uncacheable(UNCACHEABLE_SIDEEFFECT);
  return thd->insert_id();
}

/* This function is just used to test speed of different functions */

longlong Item_func_benchmark::val_int()
{
  DBUG_ASSERT(fixed == 1);
  char buff[MAX_FIELD_WIDTH];
  String tmp(buff,sizeof(buff), &my_charset_bin);
  THD *thd=current_thd;

  for (ulong loop=0 ; loop < loop_count && !thd->killed; loop++)
  {
    switch (args[0]->result_type()) {
    case REAL_RESULT:
      (void) args[0]->val_real();
      break;
    case INT_RESULT:
      (void) args[0]->val_int();
      break;
    case STRING_RESULT:
      (void) args[0]->val_str(&tmp);
      break;
    case ROW_RESULT:
    default:
      // This case should never be chosen
      DBUG_ASSERT(0);
      return 0;
    }
  }
  return 0;
}


void Item_func_benchmark::print(String *str)
{
  str->append("benchmark(", 10);
  char buffer[20];
  // my_charset_bin is good enough for numbers
  String st(buffer, sizeof(buffer), &my_charset_bin);
  st.set((ulonglong)loop_count, &my_charset_bin);
  str->append(st);
  str->append(',');
  args[0]->print(str);
  str->append(')');
}


/* This function is just used to create tests with time gaps */

longlong Item_func_sleep::val_int()
{
  THD *thd= current_thd;
  struct timespec abstime;
  pthread_cond_t cond;
  int error;

  DBUG_ASSERT(fixed == 1);

  double time= args[0]->val_real();
  set_timespec_nsec(abstime, (ulonglong)(time * ULL(1000000000)));

  pthread_cond_init(&cond, NULL);
  pthread_mutex_lock(&LOCK_user_locks);

  thd->mysys_var->current_mutex= &LOCK_user_locks;
  thd->mysys_var->current_cond=  &cond;

  error= 0;
  while (!thd->killed)
  {
    error= pthread_cond_timedwait(&cond, &LOCK_user_locks, &abstime);
    if (error == ETIMEDOUT || error == ETIME)
      break;
    error= 0;
  }

  pthread_mutex_lock(&thd->mysys_var->mutex);
  thd->mysys_var->current_mutex= 0;
  thd->mysys_var->current_cond=  0;
  pthread_mutex_unlock(&thd->mysys_var->mutex);

  pthread_mutex_unlock(&LOCK_user_locks);
  pthread_cond_destroy(&cond);

  return test(!error); 		// Return 1 killed
}


#define extra_size sizeof(double)

static user_var_entry *get_variable(HASH *hash, LEX_STRING &name,
				    bool create_if_not_exists)
{
  user_var_entry *entry;

  if (!(entry = (user_var_entry*) hash_search(hash, (byte*) name.str,
					      name.length)) &&
      create_if_not_exists)
  {
    uint size=ALIGN_SIZE(sizeof(user_var_entry))+name.length+1+extra_size;
    if (!hash_inited(hash))
      return 0;
    if (!(entry = (user_var_entry*) my_malloc(size,MYF(MY_WME))))
      return 0;
    entry->name.str=(char*) entry+ ALIGN_SIZE(sizeof(user_var_entry))+
      extra_size;
    entry->name.length=name.length;
    entry->value=0;
    entry->length=0;
    entry->update_query_id=0;
    entry->collation.set(NULL, DERIVATION_IMPLICIT);
    /*
      If we are here, we were called from a SET or a query which sets a
      variable. Imagine it is this:
      INSERT INTO t SELECT @a:=10, @a:=@a+1.
      Then when we have a Item_func_get_user_var (because of the @a+1) so we
      think we have to write the value of @a to the binlog. But before that,
      we have a Item_func_set_user_var to create @a (@a:=10), in this we mark
      the variable as "already logged" (line below) so that it won't be logged
      by Item_func_get_user_var (because that's not necessary).
    */
    entry->used_query_id=current_thd->query_id;
    entry->type=STRING_RESULT;
    memcpy(entry->name.str, name.str, name.length+1);
    if (my_hash_insert(hash,(byte*) entry))
    {
      my_free((char*) entry,MYF(0));
      return 0;
    }
  }
  return entry;
}

/*
  When a user variable is updated (in a SET command or a query like
  SELECT @a:= ).
*/

bool Item_func_set_user_var::fix_fields(THD *thd, Item **ref)
{
  DBUG_ASSERT(fixed == 0);
  /* fix_fields will call Item_func_set_user_var::fix_length_and_dec */
  if (Item_func::fix_fields(thd, ref) ||
      !(entry= get_variable(&thd->user_vars, name, 1)))
    return TRUE;
  /* 
     Remember the last query which updated it, this way a query can later know
     if this variable is a constant item in the query (it is if update_query_id
     is different from query_id).
  */
  entry->update_query_id= thd->query_id;
  /*
    As it is wrong and confusing to associate any 
    character set with NULL, @a should be latin2
    after this query sequence:

      SET @a=_latin2'string';
      SET @a=NULL;

    I.e. the second query should not change the charset
    to the current default value, but should keep the 
    original value assigned during the first query.
    In order to do it, we don't copy charset
    from the argument if the argument is NULL
    and the variable has previously been initialized.
  */
  null_item= (args[0]->type() == NULL_ITEM);
  if (!entry->collation.collation || !null_item)
    entry->collation.set(args[0]->collation.collation, DERIVATION_IMPLICIT);
  collation.set(entry->collation.collation, DERIVATION_IMPLICIT);
  cached_result_type= args[0]->result_type();
  return FALSE;
}


void
Item_func_set_user_var::fix_length_and_dec()
{
  maybe_null=args[0]->maybe_null;
  max_length=args[0]->max_length;
  decimals=args[0]->decimals;
  collation.set(args[0]->collation.collation, DERIVATION_IMPLICIT);
}


/*
  Set value to user variable.

  SYNOPSYS
    update_hash()
    entry    - pointer to structure representing variable
    set_null - should we set NULL value ?
    ptr      - pointer to buffer with new value
    length   - length of new value
    type     - type of new value
    cs       - charset info for new value
    dv       - derivation for new value

  RETURN VALUE
    False - success, True - failure
*/

static bool
update_hash(user_var_entry *entry, bool set_null, void *ptr, uint length,
            Item_result type, CHARSET_INFO *cs, Derivation dv)
{
  if (set_null)
  {
    char *pos= (char*) entry+ ALIGN_SIZE(sizeof(user_var_entry));
    if (entry->value && entry->value != pos)
      my_free(entry->value,MYF(0));
    entry->value= 0;
    entry->length= 0;
  }
  else
  {
    if (type == STRING_RESULT)
      length++;					// Store strings with end \0
    if (length <= extra_size)
    {
      /* Save value in value struct */
      char *pos= (char*) entry+ ALIGN_SIZE(sizeof(user_var_entry));
      if (entry->value != pos)
      {
	if (entry->value)
	  my_free(entry->value,MYF(0));
	entry->value=pos;
      }
    }
    else
    {
      /* Allocate variable */
      if (entry->length != length)
      {
	char *pos= (char*) entry+ ALIGN_SIZE(sizeof(user_var_entry));
	if (entry->value == pos)
	  entry->value=0;
	if (!(entry->value=(char*) my_realloc(entry->value, length,
					      MYF(MY_ALLOW_ZERO_PTR))))
	  return 1;
      }
    }
    if (type == STRING_RESULT)
    {
      length--;					// Fix length change above
      entry->value[length]= 0;			// Store end \0
    }
    memcpy(entry->value,ptr,length);
    if (type == DECIMAL_RESULT)
      ((my_decimal*)entry->value)->fix_buffer_pointer();
    entry->length= length;
    entry->collation.set(cs, dv);
  }
  entry->type=type;
  return 0;
}


bool
Item_func_set_user_var::update_hash(void *ptr, uint length, Item_result type,
                                    CHARSET_INFO *cs, Derivation dv)
{
  /*
    If we set a variable explicitely to NULL then keep the old
    result type of the variable
  */
  if ((null_value= args[0]->null_value) && null_item)
    type= entry->type;                          // Don't change type of item
  if (::update_hash(entry, (null_value= args[0]->null_value),
                    ptr, length, type, cs, dv))
  {
    current_thd->fatal_error();     // Probably end of memory
    null_value= 1;
    return 1;
  }
  return 0;
}


/* Get the value of a variable as a double */

double user_var_entry::val_real(my_bool *null_value)
{
  if ((*null_value= (value == 0)))
    return 0.0;

  switch (type) {
  case REAL_RESULT:
    return *(double*) value;
  case INT_RESULT:
    return (double) *(longlong*) value;
  case DECIMAL_RESULT:
  {
    double result;
    my_decimal2double(E_DEC_FATAL_ERROR, (my_decimal *)value, &result);
    return result;
  }
  case STRING_RESULT:
    return my_atof(value);                      // This is null terminated
  case ROW_RESULT:
    DBUG_ASSERT(1);				// Impossible
    break;
  }
  return 0.0;					// Impossible
}


/* Get the value of a variable as an integer */

longlong user_var_entry::val_int(my_bool *null_value)
{
  if ((*null_value= (value == 0)))
    return LL(0);

  switch (type) {
  case REAL_RESULT:
    return (longlong) *(double*) value;
  case INT_RESULT:
    return *(longlong*) value;
  case DECIMAL_RESULT:
  {
    longlong result;
    my_decimal2int(E_DEC_FATAL_ERROR, (my_decimal *)value, 0, &result);
    return result;
  }
  case STRING_RESULT:
  {
    int error;
    return my_strtoll10(value, (char**) 0, &error);// String is null terminated
  }
  case ROW_RESULT:
    DBUG_ASSERT(1);				// Impossible
    break;
  }
  return LL(0);					// Impossible
}


/* Get the value of a variable as a string */

String *user_var_entry::val_str(my_bool *null_value, String *str,
				uint decimals)
{
  if ((*null_value= (value == 0)))
    return (String*) 0;

  switch (type) {
  case REAL_RESULT:
    str->set(*(double*) value, decimals, &my_charset_bin);
    break;
  case INT_RESULT:
    str->set(*(longlong*) value, &my_charset_bin);
    break;
  case DECIMAL_RESULT:
    my_decimal2string(E_DEC_FATAL_ERROR, (my_decimal *)value, 0, 0, 0, str);
    break;
  case STRING_RESULT:
    if (str->copy(value, length, collation.collation))
      str= 0;					// EOM error
  case ROW_RESULT:
    DBUG_ASSERT(1);				// Impossible
    break;
  }
  return(str);
}

/* Get the value of a variable as a decimal */

my_decimal *user_var_entry::val_decimal(my_bool *null_value, my_decimal *val)
{
  if ((*null_value= (value == 0)))
    return 0;

  switch (type) {
  case REAL_RESULT:
    double2my_decimal(E_DEC_FATAL_ERROR, *(double*) value, val);
    break;
  case INT_RESULT:
    int2my_decimal(E_DEC_FATAL_ERROR, *(longlong*) value, 0, val);
    break;
  case DECIMAL_RESULT:
    val= (my_decimal *)value;
    break;
  case STRING_RESULT:
    str2my_decimal(E_DEC_FATAL_ERROR, value, length, collation.collation, val);
    break;
  case ROW_RESULT:
    DBUG_ASSERT(1);				// Impossible
    break;
  }
  return(val);
}

/*
  This functions is invoked on SET @variable or @variable:= expression.
  Evaluate (and check expression), store results.

  SYNOPSYS
    Item_func_set_user_var::check()

  NOTES
    For now it always return OK. All problem with value evaluating
    will be caught by thd->net.report_error check in sql_set_variables().

  RETURN
    FALSE OK.
*/

bool
Item_func_set_user_var::check()
{
  DBUG_ENTER("Item_func_set_user_var::check");

  switch (cached_result_type) {
  case REAL_RESULT:
  {
    save_result.vreal= args[0]->val_real();
    break;
  }
  case INT_RESULT:
  {
    save_result.vint= args[0]->val_int();
    break;
  }
  case STRING_RESULT:
  {
    save_result.vstr= args[0]->val_str(&value);
    break;
  }
  case DECIMAL_RESULT:
  {
    save_result.vdec= args[0]->val_decimal(&decimal_buff);
    break;
  }
  case ROW_RESULT:
  default:
    // This case should never be chosen
    DBUG_ASSERT(0);
    break;
  }
  DBUG_RETURN(FALSE);
}


/*
  This functions is invoked on SET @variable or @variable:= expression.

  SYNOPSIS
    Item_func_set_user_var::update()

  NOTES
    We have to store the expression as such in the variable, independent of
    the value method used by the user

  RETURN
    0	OK
    1	EOM Error

*/

bool
Item_func_set_user_var::update()
{
  bool res;
  DBUG_ENTER("Item_func_set_user_var::update");
  LINT_INIT(res);

  switch (cached_result_type) {
  case REAL_RESULT:
  {
    res= update_hash((void*) &save_result.vreal,sizeof(save_result.vreal),
		     REAL_RESULT, &my_charset_bin, DERIVATION_IMPLICIT);
    break;
  }
  case INT_RESULT:
  {
    res= update_hash((void*) &save_result.vint, sizeof(save_result.vint),
		     INT_RESULT, &my_charset_bin, DERIVATION_IMPLICIT);
    break;
  }
  case STRING_RESULT:
  {
    if (!save_result.vstr)					// Null value
      res= update_hash((void*) 0, 0, STRING_RESULT, &my_charset_bin,
		       DERIVATION_IMPLICIT);
    else
      res= update_hash((void*) save_result.vstr->ptr(),
		       save_result.vstr->length(), STRING_RESULT,
		       save_result.vstr->charset(),
		       DERIVATION_IMPLICIT);
    break;
  }
  case DECIMAL_RESULT:
  {
    if (!save_result.vdec)					// Null value
      res= update_hash((void*) 0, 0, DECIMAL_RESULT, &my_charset_bin,
                       DERIVATION_IMPLICIT);
    else
      res= update_hash((void*) save_result.vdec,
                       sizeof(my_decimal), DECIMAL_RESULT,
                       &my_charset_bin, DERIVATION_IMPLICIT);
    break;
  }
  case ROW_RESULT:
  default:
    // This case should never be chosen
    DBUG_ASSERT(0);
    break;
  }
  DBUG_RETURN(res);
}


double Item_func_set_user_var::val_real()
{
  DBUG_ASSERT(fixed == 1);
  check();
  update();					// Store expression
  return entry->val_real(&null_value);
}

longlong Item_func_set_user_var::val_int()
{
  DBUG_ASSERT(fixed == 1);
  check();
  update();					// Store expression
  return entry->val_int(&null_value);
}

String *Item_func_set_user_var::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  check();
  update();					// Store expression
  return entry->val_str(&null_value, str, decimals);
}


my_decimal *Item_func_set_user_var::val_decimal(my_decimal *val)
{
  DBUG_ASSERT(fixed == 1);
  check();
  update();					// Store expression
  return entry->val_decimal(&null_value, val);
}


void Item_func_set_user_var::print(String *str)
{
  str->append("(@", 2);
  str->append(name.str, name.length);
  str->append(":=", 2);
  args[0]->print(str);
  str->append(')');
}


void Item_func_set_user_var::print_as_stmt(String *str)
{
  str->append("set @", 5);
  str->append(name.str, name.length);
  str->append(":=", 2);
  args[0]->print(str);
  str->append(')');
}


String *
Item_func_get_user_var::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  DBUG_ENTER("Item_func_get_user_var::val_str");
  if (!var_entry)
    DBUG_RETURN((String*) 0);			// No such variable
  DBUG_RETURN(var_entry->val_str(&null_value, str, decimals));
}


double Item_func_get_user_var::val_real()
{
  DBUG_ASSERT(fixed == 1);
  if (!var_entry)
    return 0.0;					// No such variable
  return (var_entry->val_real(&null_value));
}


my_decimal *Item_func_get_user_var::val_decimal(my_decimal *dec)
{
  DBUG_ASSERT(fixed == 1);
  if (!var_entry)
    return 0;
  return var_entry->val_decimal(&null_value, dec);
}


longlong Item_func_get_user_var::val_int()
{
  DBUG_ASSERT(fixed == 1);
  if (!var_entry)
    return LL(0);				// No such variable
  return (var_entry->val_int(&null_value));
}


/*
  Get variable by name and, if necessary, put the record of variable 
  use into the binary log.
  
  SYNOPSIS
    get_var_with_binlog()
      thd        Current thread
      name       Variable name 
      out_entry  [out] variable structure or NULL. The pointer is set 
                 regardless of whether function succeeded or not.

  When a user variable is invoked from an update query (INSERT, UPDATE etc),
  stores this variable and its value in thd->user_var_events, so that it can be
  written to the binlog (will be written just before the query is written, see
  log.cc).

  RETURN
    0  OK
    1  Failed to put appropriate record into binary log

*/

int get_var_with_binlog(THD *thd, enum_sql_command sql_command,
                        LEX_STRING &name, user_var_entry **out_entry)
{
  BINLOG_USER_VAR_EVENT *user_var_event;
  user_var_entry *var_entry;
  var_entry= get_variable(&thd->user_vars, name, 0);

  if (!(opt_bin_log && is_update_query(sql_command)))
  {
    *out_entry= var_entry;
    return 0;
  }

  if (!var_entry)
  {
    /*
      If the variable does not exist, it's NULL, but we want to create it so
      that it gets into the binlog (if it didn't, the slave could be
      influenced by a variable of the same name previously set by another
      thread).
      We create it like if it had been explicitly set with SET before.
      The 'new' mimics what sql_yacc.yy does when 'SET @a=10;'.
      sql_set_variables() is what is called from 'case SQLCOM_SET_OPTION'
      in dispatch_command()). Instead of building a one-element list to pass to
      sql_set_variables(), we could instead manually call check() and update();
      this would save memory and time; but calling sql_set_variables() makes
      one unique place to maintain (sql_set_variables()). 
    */

    List<set_var_base> tmp_var_list;
    tmp_var_list.push_back(new set_var_user(new Item_func_set_user_var(name,
                                                                       new Item_null())));
    /* Create the variable */
    if (sql_set_variables(thd, &tmp_var_list))
      goto err;
    if (!(var_entry= get_variable(&thd->user_vars, name, 0)))
      goto err;
  }
  else if (var_entry->used_query_id == thd->query_id ||
           mysql_bin_log.is_query_in_union(thd, var_entry->used_query_id))
  {
    /* 
       If this variable was already stored in user_var_events by this query
       (because it's used in more than one place in the query), don't store
       it.
    */
    *out_entry= var_entry;
    return 0;
  }

  uint size;
  /*
    First we need to store value of var_entry, when the next situation
    appears:
    > set @a:=1;
    > insert into t1 values (@a), (@a:=@a+1), (@a:=@a+1);
    We have to write to binlog value @a= 1.
    
    We allocate the user_var_event on user_var_events_alloc pool, not on
    the this-statement-execution pool because in SPs user_var_event objects 
    may need to be valid after current [SP] statement execution pool is
    destroyed.
  */
  size= ALIGN_SIZE(sizeof(BINLOG_USER_VAR_EVENT)) + var_entry->length;
  if (!(user_var_event= (BINLOG_USER_VAR_EVENT *)
        alloc_root(thd->user_var_events_alloc, size)))
    goto err;
  
  user_var_event->value= (char*) user_var_event +
    ALIGN_SIZE(sizeof(BINLOG_USER_VAR_EVENT));
  user_var_event->user_var_event= var_entry;
  user_var_event->type= var_entry->type;
  user_var_event->charset_number= var_entry->collation.collation->number;
  if (!var_entry->value)
  {
    /* NULL value*/
    user_var_event->length= 0;
    user_var_event->value= 0;
  }
  else
  {
    user_var_event->length= var_entry->length;
    memcpy(user_var_event->value, var_entry->value,
           var_entry->length);
  }
  /* Mark that this variable has been used by this query */
  var_entry->used_query_id= thd->query_id;
  if (insert_dynamic(&thd->user_var_events, (gptr) &user_var_event))
    goto err;
  
  *out_entry= var_entry;
  return 0;

err:
  *out_entry= var_entry;
  return 1;
}


void Item_func_get_user_var::fix_length_and_dec()
{
  THD *thd=current_thd;
  int error;
  maybe_null=1;
  decimals=NOT_FIXED_DEC;
  max_length=MAX_BLOB_WIDTH;

  error= get_var_with_binlog(thd, thd->lex->sql_command, name, &var_entry);

  if (var_entry)
  {
    collation.set(var_entry->collation);
    switch (var_entry->type) {
    case REAL_RESULT:
      max_length= DBL_DIG + 8;
      break;
    case INT_RESULT:
      max_length= MAX_BIGINT_WIDTH;
      decimals=0;
      break;
    case STRING_RESULT:
      max_length= MAX_BLOB_WIDTH;
      break;
    case DECIMAL_RESULT:
      max_length= DECIMAL_MAX_STR_LENGTH;
      decimals= DECIMAL_MAX_SCALE;
      break;
    case ROW_RESULT:                            // Keep compiler happy
    default:
      DBUG_ASSERT(0);
      break;
    }
  }
  else
  {
    collation.set(&my_charset_bin, DERIVATION_IMPLICIT);
    null_value= 1;
  }

  if (error)
    thd->fatal_error();

  return;
}


bool Item_func_get_user_var::const_item() const
{
  return (!var_entry || current_thd->query_id != var_entry->update_query_id);
}


enum Item_result Item_func_get_user_var::result_type() const
{
  user_var_entry *entry;
  if (!(entry = (user_var_entry*) hash_search(&current_thd->user_vars,
					      (byte*) name.str,
					      name.length)))
    return STRING_RESULT;
  return entry->type;
}


void Item_func_get_user_var::print(String *str)
{
  str->append("(@", 2);
  str->append(name.str,name.length);
  str->append(')');
}


bool Item_func_get_user_var::eq(const Item *item, bool binary_cmp) const
{
  /* Assume we don't have rtti */
  if (this == item)
    return 1;					// Same item is same.
  /* Check if other type is also a get_user_var() object */
  if (item->type() != FUNC_ITEM ||
      ((Item_func*) item)->functype() != functype())
    return 0;
  Item_func_get_user_var *other=(Item_func_get_user_var*) item;
  return (name.length == other->name.length &&
	  !memcmp(name.str, other->name.str, name.length));
}


bool Item_user_var_as_out_param::fix_fields(THD *thd, Item **ref)
{
  DBUG_ASSERT(fixed == 0);
  if (Item::fix_fields(thd, ref) ||
      !(entry= get_variable(&thd->user_vars, name, 1)))
    return TRUE;
  entry->type= STRING_RESULT;
  /*
    Let us set the same collation which is used for loading
    of fields in LOAD DATA INFILE.
    (Since Item_user_var_as_out_param is used only there).
  */
  entry->collation.set(thd->variables.collation_database);
  entry->update_query_id= thd->query_id;
  return FALSE;
}


void Item_user_var_as_out_param::set_null_value(CHARSET_INFO* cs)
{
  if (::update_hash(entry, TRUE, 0, 0, STRING_RESULT, cs,
                    DERIVATION_IMPLICIT))
    current_thd->fatal_error();			// Probably end of memory
}


void Item_user_var_as_out_param::set_value(const char *str, uint length,
                                           CHARSET_INFO* cs)
{
  if (::update_hash(entry, FALSE, (void*)str, length, STRING_RESULT, cs,
                    DERIVATION_IMPLICIT))
    current_thd->fatal_error();			// Probably end of memory
}


double Item_user_var_as_out_param::val_real()
{
  DBUG_ASSERT(0);
  return 0.0;
}


longlong Item_user_var_as_out_param::val_int()
{
  DBUG_ASSERT(0);
  return 0;
}


String* Item_user_var_as_out_param::val_str(String *str)
{
  DBUG_ASSERT(0);
  return 0;
}


my_decimal* Item_user_var_as_out_param::val_decimal(my_decimal *decimal_buffer)
{
  DBUG_ASSERT(0);
  return 0;
}


void Item_user_var_as_out_param::print(String *str)
{
  str->append('@');
  str->append(name.str,name.length);
}


Item_func_get_system_var::
Item_func_get_system_var(sys_var *var_arg, enum_var_type var_type_arg,
                       LEX_STRING *component_arg, const char *name_arg,
                       size_t name_len_arg)
  :var(var_arg), var_type(var_type_arg), component(*component_arg)
{
  /* set_name() will allocate the name */
  set_name(name_arg, name_len_arg, system_charset_info);
}


bool
Item_func_get_system_var::fix_fields(THD *thd, Item **ref)
{
  Item *item;
  DBUG_ENTER("Item_func_get_system_var::fix_fields");

  /*
    Evaluate the system variable and substitute the result (a basic constant)
    instead of this item. If the variable can not be evaluated,
    the error is reported in sys_var::item().
  */
  if (!(item= var->item(thd, var_type, &component)))
    DBUG_RETURN(1);                             // Impossible
  item->set_name(name, 0, system_charset_info); // don't allocate a new name
  thd->change_item_tree(ref, item);

  DBUG_RETURN(0);
}


longlong Item_func_inet_aton::val_int()
{
  DBUG_ASSERT(fixed == 1);
  uint byte_result = 0;
  ulonglong result = 0;			// We are ready for 64 bit addresses
  const char *p,* end;
  char c = '.'; // we mark c to indicate invalid IP in case length is 0
  char buff[36];
  int dot_count= 0;

  String *s,tmp(buff,sizeof(buff),&my_charset_bin);
  if (!(s = args[0]->val_str(&tmp)))		// If null value
    goto err;
  null_value=0;

  end= (p = s->ptr()) + s->length();
  while (p < end)
  {
    c = *p++;
    int digit = (int) (c - '0');		// Assume ascii
    if (digit >= 0 && digit <= 9)
    {
      if ((byte_result = byte_result * 10 + digit) > 255)
	goto err;				// Wrong address
    }
    else if (c == '.')
    {
      dot_count++;
      result= (result << 8) + (ulonglong) byte_result;
      byte_result = 0;
    }
    else
      goto err;					// Invalid character
  }
  if (c != '.')					// IP number can't end on '.'
  {
    /*
      Handle short-forms addresses according to standard. Examples:
      127		-> 0.0.0.127
      127.1		-> 127.0.0.1
      127.2.1		-> 127.2.0.1
    */
    switch (dot_count) {
    case 1: result<<= 8; /* Fall through */
    case 2: result<<= 8; /* Fall through */
    }
    return (result << 8) + (ulonglong) byte_result;
  }

err:
  null_value=1;
  return 0;
}


void Item_func_match::init_search(bool no_order)
{
  DBUG_ENTER("Item_func_match::init_search");

  /* Check if init_search() has been called before */
  if (ft_handler)
    DBUG_VOID_RETURN;

  if (key == NO_SUCH_KEY)
  {
    List<Item> fields;
    fields.push_back(new Item_string(" ",1, cmp_collation.collation));
    for (uint i=1; i < arg_count; i++)
      fields.push_back(args[i]);
    concat=new Item_func_concat_ws(fields);
    /*
      Above function used only to get value and do not need fix_fields for it:
      Item_string - basic constant
      fields - fix_fields() was already called for this arguments
      Item_func_concat_ws - do not need fix_fields() to produce value
    */
    concat->quick_fix_field();
  }

  if (master)
  {
    join_key=master->join_key=join_key|master->join_key;
    master->init_search(no_order);
    ft_handler=master->ft_handler;
    join_key=master->join_key;
    DBUG_VOID_RETURN;
  }

  String *ft_tmp= 0;

  // MATCH ... AGAINST (NULL) is meaningless, but possible
  if (!(ft_tmp=key_item()->val_str(&value)))
  {
    ft_tmp= &value;
    value.set("",0,cmp_collation.collation);
  }

  if (ft_tmp->charset() != cmp_collation.collation)
  {
    uint dummy_errors;
    search_value.copy(ft_tmp->ptr(), ft_tmp->length(), ft_tmp->charset(),
                      cmp_collation.collation, &dummy_errors);
    ft_tmp= &search_value;
  }

  if (join_key && !no_order)
    flags|=FT_SORTED;
  ft_handler=table->file->ft_init_ext(flags, key, ft_tmp);

  if (join_key)
    table->file->ft_handler=ft_handler;

  DBUG_VOID_RETURN;
}


bool Item_func_match::fix_fields(THD *thd, Item **ref)
{
  DBUG_ASSERT(fixed == 0);
  Item *item;
  LINT_INIT(item);				// Safe as arg_count is > 1

  maybe_null=1;
  join_key=0;

  /*
    const_item is assumed in quite a bit of places, so it would be difficult
    to remove;  If it would ever to be removed, this should include
    modifications to find_best and auto_close as complement to auto_init code
    above.
   */
  if (Item_func::fix_fields(thd, ref) ||
      !args[0]->const_during_execution())
  {
    my_error(ER_WRONG_ARGUMENTS,MYF(0),"AGAINST");
    return TRUE;
  }

  const_item_cache=0;
  for (uint i=1 ; i < arg_count ; i++)
  {
    item=args[i];
    if (item->type() == Item::REF_ITEM)
      args[i]= item= *((Item_ref *)item)->ref;
    if (item->type() != Item::FIELD_ITEM)
      key=NO_SUCH_KEY;
  }
  /*
    Check that all columns come from the same table.
    We've already checked that columns in MATCH are fields so
    PARAM_TABLE_BIT can only appear from AGAINST argument.
  */
  if ((used_tables_cache & ~PARAM_TABLE_BIT) != item->used_tables())
    key=NO_SUCH_KEY;

  if (key == NO_SUCH_KEY && !(flags & FT_BOOL))
  {
    my_error(ER_WRONG_ARGUMENTS,MYF(0),"MATCH");
    return TRUE;
  }
  table=((Item_field *)item)->field->table;
  if (!(table->file->table_flags() & HA_CAN_FULLTEXT))
  {
    my_error(ER_TABLE_CANT_HANDLE_FT, MYF(0));
    return 1;
  }
  table->fulltext_searched=1;
  return agg_arg_collations_for_comparison(cmp_collation, args+1, arg_count-1);
}

bool Item_func_match::fix_index()
{
  Item_field *item;
  uint ft_to_key[MAX_KEY], ft_cnt[MAX_KEY], fts=0, keynr;
  uint max_cnt=0, mkeys=0, i;

  if (key == NO_SUCH_KEY)
    return 0;
  
  if (!table) 
    goto err;

  for (keynr=0 ; keynr < table->s->keys ; keynr++)
  {
    if ((table->key_info[keynr].flags & HA_FULLTEXT) &&
        (table->keys_in_use_for_query.is_set(keynr)))
    {
      ft_to_key[fts]=keynr;
      ft_cnt[fts]=0;
      fts++;
    }
  }

  if (!fts)
    goto err;

  for (i=1; i < arg_count; i++)
  {
    item=(Item_field*)args[i];
    for (keynr=0 ; keynr < fts ; keynr++)
    {
      KEY *ft_key=&table->key_info[ft_to_key[keynr]];
      uint key_parts=ft_key->key_parts;

      for (uint part=0 ; part < key_parts ; part++)
      {
	if (item->field->eq(ft_key->key_part[part].field))
	  ft_cnt[keynr]++;
      }
    }
  }

  for (keynr=0 ; keynr < fts ; keynr++)
  {
    if (ft_cnt[keynr] > max_cnt)
    {
      mkeys=0;
      max_cnt=ft_cnt[mkeys]=ft_cnt[keynr];
      ft_to_key[mkeys]=ft_to_key[keynr];
      continue;
    }
    if (max_cnt && ft_cnt[keynr] == max_cnt)
    {
      mkeys++;
      ft_cnt[mkeys]=ft_cnt[keynr];
      ft_to_key[mkeys]=ft_to_key[keynr];
      continue;
    }
  }

  for (keynr=0 ; keynr <= mkeys ; keynr++)
  {
    // partial keys doesn't work
    if (max_cnt < arg_count-1 ||
        max_cnt < table->key_info[ft_to_key[keynr]].key_parts)
      continue;

    key=ft_to_key[keynr];

    return 0;
  }

err:
  if (flags & FT_BOOL)
  {
    key=NO_SUCH_KEY;
    return 0;
  }
  my_message(ER_FT_MATCHING_KEY_NOT_FOUND,
             ER(ER_FT_MATCHING_KEY_NOT_FOUND), MYF(0));
  return 1;
}


bool Item_func_match::eq(const Item *item, bool binary_cmp) const
{
  if (item->type() != FUNC_ITEM ||
      ((Item_func*)item)->functype() != FT_FUNC ||
      flags != ((Item_func_match*)item)->flags)
    return 0;

  Item_func_match *ifm=(Item_func_match*) item;

  if (key == ifm->key && table == ifm->table &&
      key_item()->eq(ifm->key_item(), binary_cmp))
    return 1;

  return 0;
}


double Item_func_match::val_real()
{
  DBUG_ASSERT(fixed == 1);
  DBUG_ENTER("Item_func_match::val");
  if (ft_handler == NULL)
    DBUG_RETURN(-1.0);

  if (table->null_row) /* NULL row from an outer join */
    return 0.0;

  if (join_key)
  {
    if (table->file->ft_handler)
      DBUG_RETURN(ft_handler->please->get_relevance(ft_handler));
    join_key=0;
  }

  if (key == NO_SUCH_KEY)
  {
    String *a= concat->val_str(&value);
    if ((null_value= (a == 0)))
      DBUG_RETURN(0);
    DBUG_RETURN(ft_handler->please->find_relevance(ft_handler,
				      (byte *)a->ptr(), a->length()));
  }
  else
    DBUG_RETURN(ft_handler->please->find_relevance(ft_handler,
                                                   table->record[0], 0));
}

void Item_func_match::print(String *str)
{
  str->append("(match ", 7);
  print_args(str, 1);
  str->append(" against (", 10);
  args[0]->print(str);
  if (flags & FT_BOOL)
    str->append(" in boolean mode", 16);
  else if (flags & FT_EXPAND)
    str->append(" with query expansion", 21);
  str->append("))", 2);
}

longlong Item_func_bit_xor::val_int()
{
  DBUG_ASSERT(fixed == 1);
  ulonglong arg1= (ulonglong) args[0]->val_int();
  ulonglong arg2= (ulonglong) args[1]->val_int();
  if ((null_value= (args[0]->null_value || args[1]->null_value)))
    return 0;
  return (longlong) (arg1 ^ arg2);
}


/***************************************************************************
  System variables
****************************************************************************/

/*
  Return value of an system variable base[.name] as a constant item

  SYNOPSIS
    get_system_var()
    thd			Thread handler
    var_type		global / session
    name		Name of base or system variable
    component		Component.

  NOTES
    If component.str = 0 then the variable name is in 'name'

  RETURN
    0	error
    #	constant item
*/


Item *get_system_var(THD *thd, enum_var_type var_type, LEX_STRING name,
		     LEX_STRING component)
{
  sys_var *var;
  char buff[MAX_SYS_VAR_LENGTH*2+4+8], *pos;
  LEX_STRING *base_name, *component_name;

  if (component.str == 0 &&
      !my_strcasecmp(system_charset_info, name.str, "VERSION"))
    return new Item_string(NULL, server_version,
			   (uint) strlen(server_version),
			   system_charset_info, DERIVATION_SYSCONST);

  if (component.str)
  {
    base_name= &component;
    component_name= &name;
  }
  else
  {
    base_name= &name;
    component_name= &component;			// Empty string
  }

  if (!(var= find_sys_var(base_name->str, base_name->length)))
    return 0;
  if (component.str)
  {
    if (!var->is_struct())
    {
      my_error(ER_VARIABLE_IS_NOT_STRUCT, MYF(0), base_name->str);
      return 0;
    }
  }
  thd->lex->uncacheable(UNCACHEABLE_SIDEEFFECT);

  set_if_smaller(component_name->length, MAX_SYS_VAR_LENGTH);

  return new Item_func_get_system_var(var, var_type, component_name,
                                      NULL, 0);
}


/*
  Check a user level lock.

  SYNOPSIS:
    val_int()

  RETURN VALUES
    1		Available
    0		Already taken
    NULL	Error
*/

longlong Item_func_is_free_lock::val_int()
{
  DBUG_ASSERT(fixed == 1);
  String *res=args[0]->val_str(&value);
  User_level_lock *ull;

  null_value=0;
  if (!res || !res->length())
  {
    null_value=1;
    return 0;
  }
  
  pthread_mutex_lock(&LOCK_user_locks);
  ull= (User_level_lock *) hash_search(&hash_user_locks, (byte*) res->ptr(),
			  res->length());
  pthread_mutex_unlock(&LOCK_user_locks);
  if (!ull || !ull->locked)
    return 1;
  return 0;
}

longlong Item_func_is_used_lock::val_int()
{
  DBUG_ASSERT(fixed == 1);
  String *res=args[0]->val_str(&value);
  User_level_lock *ull;

  null_value=1;
  if (!res || !res->length())
    return 0;
  
  pthread_mutex_lock(&LOCK_user_locks);
  ull= (User_level_lock *) hash_search(&hash_user_locks, (byte*) res->ptr(),
			  res->length());
  pthread_mutex_unlock(&LOCK_user_locks);
  if (!ull || !ull->locked)
    return 0;

  null_value=0;
  return ull->thread_id;
}


longlong Item_func_row_count::val_int()
{
  DBUG_ASSERT(fixed == 1);
  THD *thd= current_thd;

  return thd->row_count_func;
}


Item_func_sp::Item_func_sp(Name_resolution_context *context_arg, sp_name *name)
  :Item_func(), context(context_arg), m_name(name), m_sp(NULL),
   result_field(NULL)
{
  maybe_null= 1;
  m_name->init_qname(current_thd);
  dummy_table= (TABLE*) sql_calloc(sizeof(TABLE));
}


Item_func_sp::Item_func_sp(Name_resolution_context *context_arg,
                           sp_name *name, List<Item> &list)
  :Item_func(list), context(context_arg), m_name(name), m_sp(NULL),
   result_field(NULL)
{
  maybe_null= 1;
  m_name->init_qname(current_thd);
  dummy_table= (TABLE*) sql_calloc(sizeof(TABLE));
}

void
Item_func_sp::cleanup()
{
  if (result_field)
  {
    delete result_field;
    result_field= NULL;
  }
  m_sp= NULL;
  Item_func::cleanup();
}

const char *
Item_func_sp::func_name() const
{
  THD *thd= current_thd;
  /* Calculate length to avoid reallocation of string for sure */
  uint len= ((m_name->m_db.length +
              m_name->m_name.length)*2 + //characters*quoting
             2 +                         // ` and `
             1 +                         // .
             1 +                         // end of string
             ALIGN_SIZE(1));             // to avoid String reallocation
  String qname((char *)alloc_root(thd->mem_root, len), len,
               system_charset_info);

  qname.length(0);
  append_identifier(thd, &qname, m_name->m_db.str, m_name->m_db.length);
  qname.append('.');
  append_identifier(thd, &qname, m_name->m_name.str, m_name->m_name.length);
  return qname.ptr();
}


Field *
Item_func_sp::sp_result_field(void) const
{
  Field *field;
  DBUG_ENTER("Item_func_sp::sp_result_field");

  if (!m_sp)
  {
    if (!(m_sp= sp_find_function(current_thd, m_name, TRUE)))
    {
      my_error(ER_SP_DOES_NOT_EXIST, MYF(0), "FUNCTION", m_name->m_qname.str);
      DBUG_RETURN(0);
    }
  }
  if (!dummy_table->s)
  {
    char *empty_name= (char *) "";
    TABLE_SHARE *share;
    dummy_table->s= share= &dummy_table->share_not_to_be_used;      
    dummy_table->alias = empty_name;
    dummy_table->maybe_null = maybe_null;
    dummy_table->in_use= current_thd;
    share->table_cache_key = empty_name;
    share->table_name = empty_name;
  }
  field= m_sp->make_field(max_length, name, dummy_table);
  DBUG_RETURN(field);
}


/*
  Execute function & store value in field

  RETURN
   0  value <> NULL
   1  value =  NULL  or error
*/

int
Item_func_sp::execute(Field **flp)
{
  Item *it;
  Field *f;
  if (execute(&it))
  {
    null_value= 1;
    return 1;
  }
  if (!(f= *flp))
  {
    *flp= f= sp_result_field();
    f->move_field((f->pack_length() > sizeof(result_buf)) ? 
                  sql_alloc(f->pack_length()) : result_buf);
    f->null_ptr= (uchar *)&null_value;
    f->null_bit= 1;
  }
  it->save_in_field(f, 1);
  return null_value= f->is_null();
}


int
Item_func_sp::execute(Item **itp)
{
  DBUG_ENTER("Item_func_sp::execute");
  THD *thd= current_thd;
  int res= -1;
  Sub_statement_state statement_state;
  Security_context *save_ctx;

  if (find_and_check_access(thd, EXECUTE_ACL, &save_ctx))
    goto error;

  /*
    Disable the binlogging if this is not a SELECT statement. If this is a
    SELECT, leave binlogging on, so execute_function() code writes the
    function call into binlog.
  */
  thd->reset_sub_statement_state(&statement_state, SUB_STMT_FUNCTION);
  res= m_sp->execute_function(thd, args, arg_count, itp);
  thd->restore_sub_statement_state(&statement_state);

  if (res && mysql_bin_log.is_open() &&
      (m_sp->m_chistics->daccess == SP_CONTAINS_SQL ||
       m_sp->m_chistics->daccess == SP_MODIFIES_SQL_DATA))
    push_warning(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                 ER_FAILED_ROUTINE_BREAK_BINLOG,
		 ER(ER_FAILED_ROUTINE_BREAK_BINLOG));

  sp_restore_security_context(thd, save_ctx);
error:
  DBUG_RETURN(res);
}


void
Item_func_sp::make_field(Send_field *tmp_field)
{
  Field *field;
  DBUG_ENTER("Item_func_sp::make_field");
  if ((field= sp_result_field()))
  {
    field->make_field(tmp_field);
    delete field;
    DBUG_VOID_RETURN;
  }
  init_make_field(tmp_field, MYSQL_TYPE_VARCHAR);  
  DBUG_VOID_RETURN;
}


enum enum_field_types
Item_func_sp::field_type() const
{
  Field *field;
  DBUG_ENTER("Item_func_sp::field_type");

  if (result_field)
    DBUG_RETURN(result_field->type());
  if ((field= sp_result_field()))
  {
    enum_field_types result= field->type();
    delete field;
    DBUG_RETURN(result);
  }
  DBUG_RETURN(MYSQL_TYPE_VARCHAR);
}


Item_result
Item_func_sp::result_type() const
{
  Field *field;
  DBUG_ENTER("Item_func_sp::result_type");
  DBUG_PRINT("info", ("m_sp = %p", m_sp));

  if (result_field)
    DBUG_RETURN(result_field->result_type());
  if ((field= sp_result_field()))
  {
    Item_result result= field->result_type();
    delete field;
    DBUG_RETURN(result);
  }
  DBUG_RETURN(STRING_RESULT);
}

void
Item_func_sp::fix_length_and_dec()
{
  Field *field;
  DBUG_ENTER("Item_func_sp::fix_length_and_dec");

  if (result_field)
  {
    decimals= result_field->decimals();
    max_length= result_field->field_length;
    DBUG_VOID_RETURN;
  }

  if (!(field= sp_result_field()))
  {
    context->process_error(current_thd);
    DBUG_VOID_RETURN;
  }
  decimals= field->decimals();
  max_length= field->field_length;
  maybe_null= 1;
  delete field;
  DBUG_VOID_RETURN;
}


longlong Item_func_found_rows::val_int()
{
  DBUG_ASSERT(fixed == 1);
  return current_thd->found_rows();
}


Field *
Item_func_sp::tmp_table_field(TABLE *t_arg)
{
  Field *res= 0;
  DBUG_ENTER("Item_func_sp::tmp_table_field");

  if (m_sp)
    res= m_sp->make_field(max_length, (const char *)name, t_arg);
  
  if (!res) 
    res= Item_func::tmp_table_field(t_arg);

  DBUG_RETURN(res);
}


/*
  Find the function and chack access rigths to the function

  SYNOPSIS
    find_and_check_access()
    thd           thread handler
    want_access   requested access
    save          backup of security context

  RETURN
    FALSE    Access granted
    TRUE     Requested access can't be granted or function doesn't exists
	     In this case security context is not changed and *save = 0

  NOTES
    Checks if requested access to function can be granted to user.
    If function isn't found yet, it searches function first.
    If function can't be found or user don't have requested access
    error is raised.
    If security context sp_ctx is provided and access can be granted then
    switch back to previous context isn't performed.
    In case of access error or if context is not provided then
    find_and_check_access() switches back to previous security context.
*/

bool
Item_func_sp::find_and_check_access(THD *thd, ulong want_access,
                                    Security_context **save)
{
  bool res= TRUE;

  *save= 0;                                     // Safety if error
  if (! m_sp && ! (m_sp= sp_find_function(thd, m_name, TRUE)))
  {
    my_error(ER_SP_DOES_NOT_EXIST, MYF(0), "FUNCTION", m_name->m_qname.str);
    goto error;
  }

#ifndef NO_EMBEDDED_ACCESS_CHECKS
  if (check_routine_access(thd, want_access,
			   m_sp->m_db.str, m_sp->m_name.str, 0, FALSE))
    goto error;

  sp_change_security_context(thd, m_sp, save);
  /*
    If we changed context to run as another user, we need to check the
    access right for the new context again as someone may have deleted
    this person the right to use the procedure

    TODO:
      Cache if the definer has the right to use the object on the first
      usage and only reset the cache if someone does a GRANT statement
      that 'may' affect this.
  */
  if (*save &&
      check_routine_access(thd, want_access,
			   m_sp->m_db.str, m_sp->m_name.str, 0, FALSE))
  {
    sp_restore_security_context(thd, *save);
    *save= 0;                                   // Safety
    goto error;
  }
#endif
  res= FALSE;                                   // no error

error:
  return res;
}

bool
Item_func_sp::fix_fields(THD *thd, Item **ref)
{
  bool res;
  DBUG_ASSERT(fixed == 0);
  res= Item_func::fix_fields(thd, ref);
  if (!res)
  {
    Security_context *save_ctx;
    if (!(res= find_and_check_access(thd, EXECUTE_ACL, &save_ctx)))
      sp_restore_security_context(thd, save_ctx);
  }
  return res;
}
