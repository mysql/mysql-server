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

#ifdef __GNUC__
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#include "slave.h"				// for wait_for_master_pos
#include <m_ctype.h>
#include <hash.h>
#include <time.h>
#include <ft_global.h>


bool check_reserved_words(LEX_STRING *name)
{
  if (!my_strcasecmp(system_charset_info, name->str, "GLOBAL") ||
      !my_strcasecmp(system_charset_info, name->str, "LOCAL") ||
      !my_strcasecmp(system_charset_info, name->str, "SESSION"))
    return TRUE;
  return FALSE;
}


static void my_coll_agg_error(DTCollation &c1, DTCollation &c2,
			      const char *fname)
{
  my_error(ER_CANT_AGGREGATE_2COLLATIONS,MYF(0),
	   c1.collation->name,c1.derivation_name(),
	   c2.collation->name,c2.derivation_name(),
	   fname);
}

static void my_coll_agg_error(DTCollation &c1,
			       DTCollation &c2,
			       DTCollation &c3,
			       const char *fname)
{
  my_error(ER_CANT_AGGREGATE_3COLLATIONS,MYF(0),
  	   c1.collation->name,c1.derivation_name(),
	   c2.collation->name,c2.derivation_name(),
	   c3.collation->name,c3.derivation_name(),
	   fname);
}


static void my_coll_agg_error(Item** args, uint count, const char *fname)
{
  if (count == 2)
    my_coll_agg_error(args[0]->collation, args[1]->collation, fname);
  else if (count == 3)
    my_coll_agg_error(args[0]->collation,
		      args[1]->collation,
		      args[2]->collation,
		      fname);
  else
    my_error(ER_CANT_AGGREGATE_NCOLLATIONS,MYF(0),fname);
}


bool Item_func::agg_arg_collations(DTCollation &c, Item **av, uint count,
                                   uint flags)
{
  uint i;
  c.nagg= 0;
  c.strong= 0;
  c.set(av[0]->collation);
  for (i= 1; i < count; i++)
  {
    if (c.aggregate(av[i]->collation, flags))
    {
      my_coll_agg_error(av, count, func_name());
      return TRUE;
    }
  }
  if ((flags & MY_COLL_DISALLOW_NONE) &&
      c.derivation == DERIVATION_NONE)
  {
    my_coll_agg_error(av, count, func_name());
    return TRUE;
  }
  return FALSE;
}


bool Item_func::agg_arg_collations_for_comparison(DTCollation &c,
						  Item **av, uint count,
                                                  uint flags)
{
  return (agg_arg_collations(c, av, count, flags | MY_COLL_DISALLOW_NONE));
}


/* return TRUE if item is a constant */

bool
eval_const_cond(COND *cond)
{
  return ((Item_func*) cond)->val_int() ? TRUE : FALSE;
}



/* 
  Collect arguments' character sets together.
  We allow to apply automatic character set conversion in some cases.
  The conditions when conversion is possible are:
  - arguments A and B have different charsets
  - A wins according to coercibility rules
    (i.e. a column is stronger than a string constant,
     an explicit COLLATE clause is stronger than a column)
  - character set of A is either superset for character set of B,
    or B is a string constant which can be converted into the
    character set of A without data loss.
    
  If all of the above is true, then it's possible to convert
  B into the character set of A, and then compare according
  to the collation of A.
  
  For functions with more than two arguments:

    collect(A,B,C) ::= collect(collect(A,B),C)
*/

bool Item_func::agg_arg_charsets(DTCollation &coll,
                                 Item **args, uint nargs, uint flags)
{
  Item **arg, **last, *safe_args[2];
  if (agg_arg_collations(coll, args, nargs, flags))
    return TRUE;

  /*
    For better error reporting: save the first and the second argument.
    We need this only if the the number of args is 3 or 2:
    - for a longer argument list, "Illegal mix of collations"
      doesn't display each argument's characteristics.
    - if nargs is 1, then this error cannot happen.
  */
  if (nargs >=2 && nargs <= 3)
  {
    safe_args[0]= args[0];
    safe_args[1]= args[1];
  }

  THD *thd= current_thd;
  Item_arena *arena= thd->current_arena, backup;
  bool res= FALSE;
  /*
    In case we're in statement prepare, create conversion item
    in its memory: it will be reused on each execute.
  */
  if (arena->is_stmt_prepare())
    thd->set_n_backup_item_arena(arena, &backup);

  for (arg= args, last= args + nargs; arg < last; arg++)
  {
    Item* conv;
    uint dummy_offset;
    if (!String::needs_conversion(0, coll.collation,
                                  (*arg)->collation.collation,
                                  &dummy_offset))
      continue;

    if (!(conv= (*arg)->safe_charset_converter(coll.collation)))
    {
      if (nargs >=2 && nargs <= 3)
      {
        /* restore the original arguments for better error message */
        args[0]= safe_args[0];
        args[1]= safe_args[1];
      }
      my_coll_agg_error(args, nargs, func_name());
      res= TRUE;
      break; // we cannot return here, we need to restore "arena".
    }
    conv->fix_fields(thd, 0, &conv);
    *arg= conv;
  }
  if (arena->is_stmt_prepare())
    thd->restore_backup_item_arena(arena, &backup);
  return res;
}



void Item_func::set_arguments(List<Item> &list)
{
  allowed_arg_cols= 1;
  arg_count=list.elements;
  if ((args=(Item**) sql_alloc(sizeof(Item*)*arg_count)))
  {
    uint i=0;
    List_iterator_fast<Item> li(list);
    Item *item;

    while ((item=li++))
    {
      args[i++]= item;
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
  tables	List of all open tables involved in the query
  ref		Pointer to where this object is used.  This reference
		is used if we want to replace this object with another
		one (for example in the summary functions).

  DESCRIPTION
    Call fix_fields() for all arguments to the function.  The main intention
    is to allow all Item_field() objects to setup pointers to the table fields.

    Sets as a side effect the following class variables:
      maybe_null	Set if any argument may return NULL
      with_sum_func	Set if any of the arguments contains a sum function
      used_table_cache  Set to union of the arguments used table

      str_value.charset If this is a string function, set this to the
			character set for the first argument.
			If any argument is binary, this is set to binary

   If for any item any of the defaults are wrong, then this can
   be fixed in the fix_length_and_dec() function that is called
   after this one or by writing a specialized fix_fields() for the
   item.

  RETURN VALUES
  0	ok
  1	Got error.  Stored with my_error().
*/

bool
Item_func::fix_fields(THD *thd, TABLE_LIST *tables, Item **ref)
{
  DBUG_ASSERT(fixed == 0);
  Item **arg,**arg_end;
#ifndef EMBEDDED_LIBRARY			// Avoid compiler warning
  char buff[STACK_BUFF_ALLOC];			// Max argument in function
#endif

  used_tables_cache= not_null_tables_cache= 0;
  const_item_cache=1;

  if (check_stack_overrun(thd, buff))
    return 1;					// Fatal error if flag is set!
  if (arg_count)
  {						// Print purify happy
    for (arg=args, arg_end=args+arg_count; arg != arg_end ; arg++)
    {
      Item *item;
      /*
	We can't yet set item to *arg as fix_fields may change *arg
	We shouldn't call fix_fields() twice, so check 'fixed' field first
      */
      if ((!(*arg)->fixed && (*arg)->fix_fields(thd, tables, arg)) ||
	  (*arg)->check_cols(allowed_arg_cols))
	return 1;				/* purecov: inspected */
      item= *arg;
      if (item->maybe_null)
	maybe_null=1;

      with_sum_func= with_sum_func || item->with_sum_func;
      used_tables_cache|=     item->used_tables();
      not_null_tables_cache|= item->not_null_tables();
      const_item_cache&=      item->const_item();
    }
  }
  fix_length_and_dec();
  if (thd->net.last_errno) // An error inside fix_length_and_dec occured
    return 1;
  fixed= 1;
  return 0;
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

void Item_func::split_sum_func(THD *thd, Item **ref_pointer_array,
                               List<Item> &fields)
{
  Item **arg, **arg_end;
  for (arg= args, arg_end= args+arg_count; arg != arg_end ; arg++)
  {
    Item *item=* arg;
    if (item->with_sum_func && item->type() != SUM_FUNC_ITEM)
      item->split_sum_func(thd, ref_pointer_array, fields);
    else if (item->used_tables() || item->type() == SUM_FUNC_ITEM)
    {
      uint el= fields.elements;
      Item *new_item= new Item_ref(ref_pointer_array + el, 0, item->name);
      fields.push_front(item);
      ref_pointer_array[el]= item;
      thd->change_item_tree(arg, new_item);
    }
  }
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
    if (max_length > 255)
      res= new Field_blob(max_length, maybe_null, name, t_arg, collation.collation);
    else
      res= new Field_string(max_length, maybe_null, name, t_arg, collation.collation);
    break;
  case ROW_RESULT:
  default:
    // This case should never be choosen
    DBUG_ASSERT(0);
    break;
  }
  return res;
}


String *Item_real_func::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  double nr=val();
  if (null_value)
    return 0; /* purecov: inspected */
  str->set(nr,decimals, &my_charset_bin);
  return str;
}


String *Item_num_func::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  if (hybrid_type == INT_RESULT)
  {
    longlong nr=val_int();
    if (null_value)
      return 0; /* purecov: inspected */
    if (!unsigned_flag)
      str->set(nr,&my_charset_bin);
    else
      str->set((ulonglong) nr,&my_charset_bin);
  }
  else
  {
    double nr=val();
    if (null_value)
      return 0; /* purecov: inspected */
    str->set(nr,decimals,&my_charset_bin);
  }
  return str;
}


void Item_func::fix_num_length_and_dec()
{
  decimals=0;
  for (uint i=0 ; i < arg_count ; i++)
    set_if_bigger(decimals,args[i]->decimals);
  max_length=float_length(decimals);
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
  Change from REAL_RESULT (default) to INT_RESULT if both arguments are
  integers
*/

void Item_num_op::find_num_type(void)
{
  if (args[0]->result_type() == INT_RESULT &&
      args[1]->result_type() == INT_RESULT)
  {
    hybrid_type=INT_RESULT;
    unsigned_flag=args[0]->unsigned_flag | args[1]->unsigned_flag;
  }
}

String *Item_num_op::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  if (hybrid_type == INT_RESULT)
  {
    longlong nr=val_int();
    if (null_value)
      return 0; /* purecov: inspected */
    if (!unsigned_flag)
      str->set(nr,&my_charset_bin);
    else
      str->set((ulonglong) nr,&my_charset_bin);
  }
  else
  {
    double nr=val();
    if (null_value)
      return 0; /* purecov: inspected */
    str->set(nr,decimals,&my_charset_bin);
  }
  return str;
}


void Item_func_signed::print(String *str)
{
  str->append("cast(", 5);
  args[0]->print(str);
  str->append(" as signed)", 11);

}


void Item_func_unsigned::print(String *str)
{
  str->append("cast(", 5);
  args[0]->print(str);
  str->append(" as unsigned)", 13);

}


double Item_func_plus::val()
{
  DBUG_ASSERT(fixed == 1);
  double value=args[0]->val()+args[1]->val();
  if ((null_value=args[0]->null_value || args[1]->null_value))
    return 0.0;
  return value;
}

longlong Item_func_plus::val_int()
{
  DBUG_ASSERT(fixed == 1);
  if (hybrid_type == INT_RESULT)
  {
    longlong value=args[0]->val_int()+args[1]->val_int();
    if ((null_value=args[0]->null_value || args[1]->null_value))
      return 0;
    return value;
  }
  return (longlong) Item_func_plus::val();
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


double Item_func_minus::val()
{
  DBUG_ASSERT(fixed == 1);
  double value=args[0]->val() - args[1]->val();
  if ((null_value=args[0]->null_value || args[1]->null_value))
    return 0.0;
  return value;
}

longlong Item_func_minus::val_int()
{
  DBUG_ASSERT(fixed == 1);
  if (hybrid_type == INT_RESULT)
  {
    longlong value=args[0]->val_int() - args[1]->val_int();
    if ((null_value=args[0]->null_value || args[1]->null_value))
      return 0;
    return value;
  }
  return (longlong) Item_func_minus::val();
}


double Item_func_mul::val()
{
  DBUG_ASSERT(fixed == 1);
  double value=args[0]->val()*args[1]->val();
  if ((null_value=args[0]->null_value || args[1]->null_value))
    return 0.0; /* purecov: inspected */
  return value;
}

longlong Item_func_mul::val_int()
{
  DBUG_ASSERT(fixed == 1);
  if (hybrid_type == INT_RESULT)
  {
    longlong value=args[0]->val_int()*args[1]->val_int();
    if ((null_value=args[0]->null_value || args[1]->null_value))
      return 0; /* purecov: inspected */
    return value;
  }
  return (longlong) Item_func_mul::val();
}


double Item_func_div::val()
{
  DBUG_ASSERT(fixed == 1);
  double value=args[0]->val();
  double val2=args[1]->val();
  if ((null_value= val2 == 0.0 || args[0]->null_value || args[1]->null_value))
    return 0.0;
  return value/val2;
}

longlong Item_func_div::val_int()
{
  DBUG_ASSERT(fixed == 1);
  if (hybrid_type == INT_RESULT)
  {
    longlong value=args[0]->val_int();
    longlong val2=args[1]->val_int();
    if ((null_value= val2 == 0 || args[0]->null_value || args[1]->null_value))
      return 0;
    return value/val2;
  }
  return (longlong) Item_func_div::val();
}

void Item_func_div::fix_length_and_dec()
{
  decimals=max(args[0]->decimals,args[1]->decimals)+2;
  set_if_smaller(decimals, NOT_FIXED_DEC);
  max_length=args[0]->max_length - args[0]->decimals + decimals;
  uint tmp=float_length(decimals);
  set_if_smaller(max_length,tmp);
  maybe_null=1;
}


/* Integer division */
longlong Item_func_int_div::val_int()
{
  DBUG_ASSERT(fixed == 1);
  longlong value=args[0]->val_int();
  longlong val2=args[1]->val_int();
  if ((null_value= val2 == 0 || args[0]->null_value || args[1]->null_value))
    return 0;
  return (unsigned_flag ?
	  (ulonglong) value / (ulonglong) val2 :
	  value / val2);
}


void Item_func_int_div::fix_length_and_dec()
{
  find_num_type();
  max_length=args[0]->max_length - args[0]->decimals;
  maybe_null=1;
}


double Item_func_mod::val()
{
  DBUG_ASSERT(fixed == 1);
  double x= args[0]->val();
  double y= args[1]->val();
  if ((null_value= (y == 0.0) || args[0]->null_value || args[1]->null_value))
    return 0.0; /* purecov: inspected */
  return fmod(x, y);
}

longlong Item_func_mod::val_int()
{
  DBUG_ASSERT(fixed == 1);
  longlong value=  args[0]->val_int();
  longlong val2= args[1]->val_int();
  if ((null_value=val2 == 0 || args[0]->null_value || args[1]->null_value))
    return 0; /* purecov: inspected */
  return value % val2;
}

void Item_func_mod::fix_length_and_dec()
{
  Item_num_op::fix_length_and_dec();
}


double Item_func_neg::val()
{
  DBUG_ASSERT(fixed == 1);
  double value=args[0]->val();
  null_value=args[0]->null_value;
  return -value;
}


longlong Item_func_neg::val_int()
{
  DBUG_ASSERT(fixed == 1);
  longlong value=args[0]->val_int();
  null_value=args[0]->null_value;
  return -value;
}


void Item_func_neg::fix_length_and_dec()
{
  decimals=args[0]->decimals;
  max_length=args[0]->max_length;
  hybrid_type= REAL_RESULT;
  if (args[0]->result_type() == INT_RESULT)
  {
    /*
      If this is in integer context keep the context as integer
      (This is how multiplication and other integer functions works)

      We must however do a special case in the case where the argument
      is a unsigned bigint constant as in this case the only safe
      number to convert in integer context is 9223372036854775808.
      (This is needed because the lex parser doesn't anymore handle
      signed integers)
    */
    if (args[0]->type() != INT_ITEM ||
	((ulonglong) ((Item_uint*) args[0])->value <=
	 (ulonglong) LONGLONG_MIN))
      hybrid_type= INT_RESULT;
  }
}


double Item_func_abs::val()
{
  DBUG_ASSERT(fixed == 1);
  double value=args[0]->val();
  null_value=args[0]->null_value;
  return fabs(value);
}


longlong Item_func_abs::val_int()
{
  DBUG_ASSERT(fixed == 1);
  longlong value=args[0]->val_int();
  null_value=args[0]->null_value;
  return value >= 0 ? value : -value;
}


void Item_func_abs::fix_length_and_dec()
{
  decimals=args[0]->decimals;
  max_length=args[0]->max_length;
  hybrid_type= REAL_RESULT;
  if (args[0]->result_type() == INT_RESULT)
  {
    hybrid_type= INT_RESULT;
    unsigned_flag= 1;
  }
}


/* Gateway to natural LOG function */
double Item_func_ln::val()
{
  DBUG_ASSERT(fixed == 1);
  double value=args[0]->val();
  if ((null_value=(args[0]->null_value || value <= 0.0)))
    return 0.0;
  return log(value);
}

/* 
 Extended but so slower LOG function
 We have to check if all values are > zero and first one is not one
 as these are the cases then result is not a number.
*/ 
double Item_func_log::val()
{
  DBUG_ASSERT(fixed == 1);
  double value=args[0]->val();
  if ((null_value=(args[0]->null_value || value <= 0.0)))
    return 0.0;
  if (arg_count == 2)
  {
    double value2= args[1]->val();
    if ((null_value=(args[1]->null_value || value2 <= 0.0 || value == 1.0)))
      return 0.0;
    return log(value2) / log(value);
  }
  return log(value);
}

double Item_func_log2::val()
{
  DBUG_ASSERT(fixed == 1);
  double value=args[0]->val();
  if ((null_value=(args[0]->null_value || value <= 0.0)))
    return 0.0;
  return log(value) / log(2.0);
}

double Item_func_log10::val()
{
  DBUG_ASSERT(fixed == 1);
  double value=args[0]->val();
  if ((null_value=(args[0]->null_value || value <= 0.0)))
    return 0.0; /* purecov: inspected */
  return log10(value);
}

double Item_func_exp::val()
{
  DBUG_ASSERT(fixed == 1);
  double value=args[0]->val();
  if ((null_value=args[0]->null_value))
    return 0.0; /* purecov: inspected */
  return exp(value);
}

double Item_func_sqrt::val()
{
  DBUG_ASSERT(fixed == 1);
  double value=args[0]->val();
  if ((null_value=(args[0]->null_value || value < 0)))
    return 0.0; /* purecov: inspected */
  return sqrt(value);
}

double Item_func_pow::val()
{
  DBUG_ASSERT(fixed == 1);
  double value=args[0]->val();
  double val2=args[1]->val();
  if ((null_value=(args[0]->null_value || args[1]->null_value)))
    return 0.0; /* purecov: inspected */
  return pow(value,val2);
}

// Trigonometric functions

double Item_func_acos::val()
{
  DBUG_ASSERT(fixed == 1);
  // the volatile's for BUG #2338 to calm optimizer down (because of gcc's bug)
  volatile double value=args[0]->val();
  if ((null_value=(args[0]->null_value || (value < -1.0 || value > 1.0))))
    return 0.0;
  return fix_result(acos(value));
}

double Item_func_asin::val()
{
  DBUG_ASSERT(fixed == 1);
  // the volatile's for BUG #2338 to calm optimizer down (because of gcc's bug)
  volatile double value=args[0]->val();
  if ((null_value=(args[0]->null_value || (value < -1.0 || value > 1.0))))
    return 0.0;
  return fix_result(asin(value));
}

double Item_func_atan::val()
{
  DBUG_ASSERT(fixed == 1);
  double value=args[0]->val();
  if ((null_value=args[0]->null_value))
    return 0.0;
  if (arg_count == 2)
  {
    double val2= args[1]->val();
    if ((null_value=args[1]->null_value))
      return 0.0;
    return fix_result(atan2(value,val2));
  }
  return fix_result(atan(value));
}

double Item_func_cos::val()
{
  DBUG_ASSERT(fixed == 1);
  double value=args[0]->val();
  if ((null_value=args[0]->null_value))
    return 0.0;
  return fix_result(cos(value));
}

double Item_func_sin::val()
{
  DBUG_ASSERT(fixed == 1);
  double value=args[0]->val();
  if ((null_value=args[0]->null_value))
    return 0.0;
  return fix_result(sin(value));
}

double Item_func_tan::val()
{
  DBUG_ASSERT(fixed == 1);
  double value=args[0]->val();
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

longlong Item_func_ceiling::val_int()
{
  DBUG_ASSERT(fixed == 1);
  double value=args[0]->val();
  null_value=args[0]->null_value;
  return (longlong) ceil(value);
}

longlong Item_func_floor::val_int()
{
  DBUG_ASSERT(fixed == 1);
  // the volatile's for BUG #3051 to calm optimizer down (because of gcc's bug)
  volatile double value=args[0]->val();
  null_value=args[0]->null_value;
  return (longlong) floor(value);
}

void Item_func_round::fix_length_and_dec()
{
  max_length=args[0]->max_length;
  decimals=args[0]->decimals;
  if (args[1]->const_item())
  {
    int tmp=(int) args[1]->val_int();
    if (tmp < 0)
      decimals=0;
    else
      decimals=min(tmp,NOT_FIXED_DEC);
  }
}

double Item_func_round::val()
{
  DBUG_ASSERT(fixed == 1);
  double value=args[0]->val();
  int dec=(int) args[1]->val_int();
  uint abs_dec=abs(dec);
  double tmp;
  /*
    tmp2 is here to avoid return the value with 80 bit precision
    This will fix that the test round(0.1,1) = round(0.1,1) is true
  */
  volatile double tmp2;

  if ((null_value=args[0]->null_value || args[1]->null_value))
    return 0.0;
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


bool Item_func_rand::fix_fields(THD *thd, struct st_table_list *tables,
                                Item **ref)
{
  Item_real_func::fix_fields(thd, tables, ref);
  used_tables_cache|= RAND_TABLE_BIT;
  if (arg_count)
  {					// Only use argument once in query
    /*
      Allocate rand structure once: we must use thd->current_arena
      to create rand in proper mem_root if it's a prepared statement or
      stored procedure.
    */
    if (!rand && !(rand= (struct rand_struct*)
                   thd->current_arena->alloc(sizeof(*rand))))
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
      No need to send a Rand log event if seed was given eg: RAND(seed),
      as it will be replicated in the query as such.

      Save the seed only the first time RAND() is used in the query
      Once events are forwarded rather than recreated,
      the following can be skipped if inside the slave thread
    */
    thd->rand_used=1;
    thd->rand_saved_seed1=thd->rand.seed1;
    thd->rand_saved_seed2=thd->rand.seed2;
    rand= &thd->rand;
  }
  return FALSE;
}

void Item_func_rand::update_used_tables()
{
  Item_real_func::update_used_tables();
  used_tables_cache|= RAND_TABLE_BIT;
}


double Item_func_rand::val()
{
  DBUG_ASSERT(fixed == 1);
  return my_rnd(rand);
}

longlong Item_func_sign::val_int()
{
  DBUG_ASSERT(fixed == 1);
  double value=args[0]->val();
  null_value=args[0]->null_value;
  return value < 0.0 ? -1 : (value > 0 ? 1 : 0);
}


double Item_func_units::val()
{
  DBUG_ASSERT(fixed == 1);
  double value=args[0]->val();
  if ((null_value=args[0]->null_value))
    return 0;
  return value*mul+add;
}


void Item_func_min_max::fix_length_and_dec()
{
  decimals=0;
  max_length=0;
  maybe_null=1;
  cmp_type=args[0]->result_type();

  for (uint i=0 ; i < arg_count ; i++)
  {
    if (max_length < args[i]->max_length)
      max_length=args[i]->max_length;
    if (decimals < args[i]->decimals)
      decimals=args[i]->decimals;
    if (!args[i]->maybe_null)
      maybe_null=0;
    cmp_type=item_cmp_type(cmp_type,args[i]->result_type());
  }
  if (cmp_type == STRING_RESULT)
    agg_arg_charsets(collation, args, arg_count, MY_COLL_CMP_CONV);
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
  case REAL_RESULT:
  {
    double nr=val();
    if (null_value)
      return 0; /* purecov: inspected */
    str->set(nr,decimals,&my_charset_bin);
    return str;
  }
  case STRING_RESULT:
  {
    String *res;
    LINT_INIT(res);
    null_value=1;
    for (uint i=0; i < arg_count ; i++)
    {
      if (null_value)
      {
	res=args[i]->val_str(str);
	null_value=args[i]->null_value;
      }
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
    }
    if (res)					// If !NULL
      res->set_charset(collation.collation);
    return res;
  }
  case ROW_RESULT:
  default:
    // This case should never be choosen
    DBUG_ASSERT(0);
    return 0;
  }
  return 0;					// Keep compiler happy
}


double Item_func_min_max::val()
{
  DBUG_ASSERT(fixed == 1);
  double value=0.0;
  null_value=1;
  for (uint i=0; i < arg_count ; i++)
  {
    if (null_value)
    {
      value=args[i]->val();
      null_value=args[i]->null_value;
    }
    else
    {
      double tmp=args[i]->val();
      if (!args[i]->null_value && (tmp < value ? cmp_sign : -cmp_sign) > 0)
	value=tmp;
    }
  }
  return value;
}


longlong Item_func_min_max::val_int()
{
  DBUG_ASSERT(fixed == 1);
  longlong value=0;
  null_value=1;
  for (uint i=0; i < arg_count ; i++)
  {
    if (null_value)
    {
      value=args[i]->val_int();
      null_value=args[i]->null_value;
    }
    else
    {
      longlong tmp=args[i]->val_int();
      if (!args[i]->null_value && (tmp < value ? cmp_sign : -cmp_sign) > 0)
	value=tmp;
    }
  }
  return value;
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
  if (args[0]->null_value)
  {
    null_value= 1;
    return 0;
  }
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
    if (!(field=args[0]->val_str(&value)))
      return 0;					// -1 if null ?
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
    for (uint i=1; i < arg_count ; i++)
    {
      if (val == args[i]->val_int())
 	return (longlong) (i);
    }
  }
  else
  {
    double val= args[0]->val();
    for (uint i=1; i < arg_count ; i++)
    {
      if (val == args[i]->val())
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
  agg_arg_collations_for_comparison(cmp_collation, args, 2);
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
        return (longlong) 0;
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

udf_handler::~udf_handler()
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
  }
  if (buffers)					// Because of bug in ecc
    delete [] buffers;
}


bool
udf_handler::fix_fields(THD *thd, TABLE_LIST *tables, Item_result_field *func,
			uint arg_count, Item **arguments)
{
#ifndef EMBEDDED_LIBRARY			// Avoid compiler warning
  char buff[STACK_BUFF_ALLOC];			// Max argument in function
#endif
  DBUG_ENTER("Item_udf_func::fix_fields");

  if (check_stack_overrun(thd, buff))
    DBUG_RETURN(1);				// Fatal error flag is set!

  udf_func *tmp_udf=find_udf(u_d->name.str,(uint) u_d->name.length,1);

  if (!tmp_udf)
  {
    my_printf_error(ER_CANT_FIND_UDF,ER(ER_CANT_FIND_UDF),MYF(0),u_d->name.str,
		    errno);
    DBUG_RETURN(1);
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
      DBUG_RETURN(1);
    }
    uint i;
    Item **arg,**arg_end;
    for (i=0, arg=arguments, arg_end=arguments+arg_count;
	 arg != arg_end ;
	 arg++,i++)
    {
      if ((*arg)->fix_fields(thd, tables, arg))
	DBUG_RETURN(1);
      // we can't assign 'item' before, because fix_fields() can change arg
      Item *item= *arg;
      if (item->check_cols(1))
	DBUG_RETURN(1);
      /*
	TODO: We should think about this. It is not always
	right way just to set an UDF result to return my_charset_bin
	if one argument has binary sorting order.
	The result collation should be calculated according to arguments
	derivations in some cases and should not in other cases.
	Moreover, some arguments can represent a numeric input
	which doesn't effect the result character set and collation.
	There is no a general rule for UDF. Everything depends on
	the particular user definted function.
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
    if (!(buffers=new String[arg_count]) ||
	!(f_args.args= (char**) sql_alloc(arg_count * sizeof(char *))) ||
	!(f_args.lengths=(ulong*) sql_alloc(arg_count * sizeof(long))) ||
	!(f_args.maybe_null=(char*) sql_alloc(arg_count * sizeof(char))) ||
	!(num_buffer= (char*) sql_alloc(ALIGN_SIZE(sizeof(double))*arg_count)))
    {
      free_udf(u_d);
      DBUG_RETURN(1);
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
      f_args.lengths[i]=arguments[i]->max_length;
      f_args.maybe_null[i]=(char) arguments[i]->maybe_null;

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
	*((double*) to) = arguments[i]->val();
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
      my_printf_error(ER_CANT_INITIALIZE_UDF,ER(ER_CANT_INITIALIZE_UDF),MYF(0),
		      u_d->name.str, thd->net.last_error);
      free_udf(u_d);
      DBUG_RETURN(1);
    }
    func->max_length=min(initid.max_length,MAX_BLOB_WIDTH);
    func->maybe_null=initid.maybe_null;
    const_item_cache=initid.const_item;
    func->decimals=min(initid.decimals,NOT_FIXED_DEC);
  }
  initialized=1;
  if (error)
  {
    my_printf_error(ER_CANT_INITIALIZE_UDF,ER(ER_CANT_INITIALIZE_UDF),MYF(0),
		    u_d->name.str, ER(ER_UNKNOWN_ERROR));
    DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
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
      *((double*) to) = args[i]->val();
      if (!args[i]->null_value)
      {
	f_args.args[i]=to;
	to+= ALIGN_SIZE(sizeof(double));
      }
      break;
    case ROW_RESULT:
    default:
      // This case should never be choosen
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



double Item_func_udf_float::val()
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
  double nr=val();
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
  if (mysql_bin_log.is_open())
  {
    char buf[256];
    const char *command="DO RELEASE_LOCK(\"";
    String tmp(buf,sizeof(buf), system_charset_info);
    tmp.copy(command, strlen(command), tmp.charset());
    tmp.append(ull->key,ull->key_length);
    tmp.append("\")", 2);
    Query_log_event qev(current_thd, tmp.ptr(), tmp.length(),1);
    qev.error_code=0; // this query is always safe to run on slave
    mysql_bin_log.write(&qev);
  }
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
  int lock_name_len,error=0;
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
  while (!thd->killed &&
	 (error=pthread_cond_timedwait(&ull->cond,&LOCK_user_locks,&abstime))
	 != ETIME && error != ETIMEDOUT && ull->locked) ;
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
  int error=0;

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
  while (!thd->killed &&
	 (error=pthread_cond_timedwait(&ull->cond,&LOCK_user_locks,&abstime))
	 != ETIME && error != ETIMEDOUT && error != EINVAL && ull->locked) ;
  if (thd->killed)
    error=EINTR;				// Return NULL
  if (ull->locked)
  {
    if (!--ull->count)
      delete ull;				// Should never happen
    if (error != ETIME && error != ETIMEDOUT)
    {
      error=1;
      null_value=1;				// Return NULL
    }
  }
  else
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
  DBUG_ASSERT(fixed == 1);
  if (arg_count)
  {
    longlong value=args[0]->val_int();
    current_thd->insert_id(value);
    null_value=args[0]->null_value;
    return value;
  }
  else
  {
    Item *it= get_system_var(current_thd, OPT_SESSION, "last_insert_id", 14,
			     "last_insert_id()");
    return it->val_int();
  }
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
      (void) args[0]->val();
      break;
    case INT_RESULT:
      (void) args[0]->val_int();
      break;
    case STRING_RESULT:
      (void) args[0]->val_str(&tmp);
      break;
    case ROW_RESULT:
    default:
      // This case should never be choosen
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
    entry->collation.set(NULL, DERIVATION_NONE);
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

bool Item_func_set_user_var::fix_fields(THD *thd, TABLE_LIST *tables,
					Item **ref)
{
  DBUG_ASSERT(fixed == 0);
  /* fix_fields will call Item_func_set_user_var::fix_length_and_dec */
  if (Item_func::fix_fields(thd, tables, ref) ||
      !(entry= get_variable(&thd->user_vars, name, 1)))
    return 1;
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
  if (!entry->collation.collation || !args[0]->null_value)
    entry->collation.set(args[0]->collation);
  collation.set(entry->collation);
  cached_result_type= args[0]->result_type();
  return 0;
}


void
Item_func_set_user_var::fix_length_and_dec()
{
  maybe_null=args[0]->maybe_null;
  max_length=args[0]->max_length;
  decimals=args[0]->decimals;
  collation.set(args[0]->collation);
}


bool Item_func_set_user_var::update_hash(void *ptr, uint length,
					 Item_result type,
					 CHARSET_INFO *cs,
					 Derivation dv)
{
  if ((null_value=args[0]->null_value))
  {
    char *pos= (char*) entry+ ALIGN_SIZE(sizeof(user_var_entry));
    if (entry->value && entry->value != pos)
      my_free(entry->value,MYF(0));
    entry->value=0;
    entry->length=0;
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
	  goto err;
      }
    }
    if (type == STRING_RESULT)
    {
      length--;					// Fix length change above
      entry->value[length]= 0;			// Store end \0
    }
    memcpy(entry->value,ptr,length);
    entry->length= length;
    entry->type=type;
    entry->collation.set(cs, dv);
  }
  return 0;

 err:
  current_thd->fatal_error();			// Probably end of memory
  null_value= 1;
  return 1;
}


/* Get the value of a variable as a double */

double user_var_entry::val(my_bool *null_value)
{
  if ((*null_value= (value == 0)))
    return 0.0;

  switch (type) {
  case REAL_RESULT:
    return *(double*) value;
  case INT_RESULT:
    return (double) *(longlong*) value;
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
  case STRING_RESULT:
    if (str->copy(value, length, collation.collation))
      str= 0;					// EOM error
  case ROW_RESULT:
    DBUG_ASSERT(1);				// Impossible
    break;
  }
  return(str);
}

/*
  This functions is invoked on SET @variable or @variable:= expression.
  Evaluete (and check expression), store results.

  SYNOPSYS
    Item_func_set_user_var::check()

  NOTES
    For now it always return OK. All problem with value evalueting
    will be catched by thd->net.report_error check in sql_set_variables().

  RETURN
    0 - OK.
*/

bool
Item_func_set_user_var::check()
{
  DBUG_ENTER("Item_func_set_user_var::check");

  switch (cached_result_type) {
  case REAL_RESULT:
  {
    save_result.vreal= args[0]->val();
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
  case ROW_RESULT:
  default:
    // This case should never be choosen
    DBUG_ASSERT(0);
    break;
  }
  DBUG_RETURN(0);
}


/*
  This functions is invoked on SET @variable or @variable:= expression.

  SYNOPSIS
    Item_func_set_user_var::update()

  NOTES
    We have to store the expression as such in the variable, independent of
    the value method used by the user

  RETURN
    0	Ok
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
		     REAL_RESULT, &my_charset_bin, DERIVATION_NONE);
    break;
  }
  case INT_RESULT:
  {
    res= update_hash((void*) &save_result.vint, sizeof(save_result.vint),
		     INT_RESULT, &my_charset_bin, DERIVATION_NONE);
    break;
  }
  case STRING_RESULT:
  {
    if (!save_result.vstr)					// Null value
      res= update_hash((void*) 0, 0, STRING_RESULT, &my_charset_bin,
		       DERIVATION_NONE);
    else
      res= update_hash((void*) save_result.vstr->ptr(),
		       save_result.vstr->length(), STRING_RESULT,
		       save_result.vstr->charset(),
		       args[0]->collation.derivation);
    break;
  }
  case ROW_RESULT:
  default:
    // This case should never be choosen
    DBUG_ASSERT(0);
    break;
  }
  DBUG_RETURN(res);
}


double Item_func_set_user_var::val()
{
  DBUG_ASSERT(fixed == 1);
  check();
  update();					// Store expression
  return entry->val(&null_value);
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


void Item_func_set_user_var::print(String *str)
{
  str->append("(@", 2);
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


double Item_func_get_user_var::val()
{
  DBUG_ASSERT(fixed == 1);
  if (!var_entry)
    return 0.0;					// No such variable
  return (var_entry->val(&null_value));
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
    1  Failed to put appropiate record into binary log
    
*/

int get_var_with_binlog(THD *thd, LEX_STRING &name, 
                        user_var_entry **out_entry)
{
  BINLOG_USER_VAR_EVENT *user_var_event;
  user_var_entry *var_entry;
  var_entry= get_variable(&thd->user_vars, name, 0);
  
  if (!(opt_bin_log && is_update_query(thd->lex->sql_command)))
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
      We create it like if it had been explicitely set with SET before.
      The 'new' mimicks what sql_yacc.yy does when 'SET @a=10;'.
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
  else if (var_entry->used_query_id == thd->query_id)
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
    appers:
    > set @a:=1;
    > insert into t1 values (@a), (@a:=@a+1), (@a:=@a+1);
    We have to write to binlog value @a= 1;
  */
  size= ALIGN_SIZE(sizeof(BINLOG_USER_VAR_EVENT)) + var_entry->length;      
  if (!(user_var_event= (BINLOG_USER_VAR_EVENT *) thd->alloc(size)))
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

  error= get_var_with_binlog(thd, name, &var_entry);

  if (var_entry)
  {
    collation.set(var_entry->collation);
    switch (var_entry->type) {
    case REAL_RESULT:
      max_length= DBL_DIG + 8;
    case INT_RESULT:
      max_length= MAX_BIGINT_WIDTH;
      break;
    case STRING_RESULT:
      max_length= MAX_BLOB_WIDTH;
      break;
    case ROW_RESULT:                            // Keep compiler happy
      break;
    }
  }
  else
    null_value= 1;

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
      ((Item_func*) item)->func_name() != func_name())
    return 0;
  Item_func_get_user_var *other=(Item_func_get_user_var*) item;
  return (name.length == other->name.length &&
	  !memcmp(name.str, other->name.str, name.length));
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
  ft_handler=table->file->ft_init_ext(flags, key,
				      (byte*) ft_tmp->ptr(),
				      ft_tmp->length());

  if (join_key)
    table->file->ft_handler=ft_handler;

  DBUG_VOID_RETURN;
}


bool Item_func_match::fix_fields(THD *thd, TABLE_LIST *tlist, Item **ref)
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
  if (Item_func::fix_fields(thd, tlist, ref) ||
      !args[0]->const_during_execution())
  {
    my_error(ER_WRONG_ARGUMENTS,MYF(0),"AGAINST");
    return 1;
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
    return 1;
  }
  table=((Item_field *)item)->field->table;
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

  for (keynr=0 ; keynr < table->keys ; keynr++)
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
  my_error(ER_FT_MATCHING_KEY_NOT_FOUND,MYF(0));
  return 1;
}


bool Item_func_match::eq(const Item *item, bool binary_cmp) const
{
  if (item->type() != FUNC_ITEM || ((Item_func*)item)->functype() != FT_FUNC ||
      flags != ((Item_func_match*)item)->flags)
    return 0;

  Item_func_match *ifm=(Item_func_match*) item;

  if (key == ifm->key && table == ifm->table &&
      key_item()->eq(ifm->key_item(), binary_cmp))
    return 1;

  return 0;
}


double Item_func_match::val()
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
  if (component.str == 0 &&
      !my_strcasecmp(system_charset_info, name.str, "VERSION"))
    return new Item_string("@@VERSION", server_version,
			   (uint) strlen(server_version),
			   system_charset_info);

  Item *item;
  sys_var *var;
  char buff[MAX_SYS_VAR_LENGTH*2+4+8], *pos;
  LEX_STRING *base_name, *component_name;

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
      net_printf(thd, ER_VARIABLE_IS_NOT_STRUCT, base_name->str);
      return 0;
    }
  }
  if (!(item=var->item(thd, var_type, component_name)))
    return 0;					// Impossible
  thd->lex->uncacheable(UNCACHEABLE_SIDEEFFECT);
  buff[0]='@';
  buff[1]='@';
  pos=buff+2;
  if (var_type == OPT_SESSION)
    pos=strmov(pos,"session.");
  else if (var_type == OPT_GLOBAL)
    pos=strmov(pos,"global.");
  
  set_if_smaller(component_name->length, MAX_SYS_VAR_LENGTH);
  set_if_smaller(base_name->length, MAX_SYS_VAR_LENGTH);

  if (component_name->str)
  {
    memcpy(pos, component_name->str, component_name->length);
    pos+= component_name->length;
    *pos++= '.';
  }
  memcpy(pos, base_name->str, base_name->length);
  pos+= base_name->length;

  // set_name() will allocate the name
  item->set_name(buff,(uint) (pos-buff), system_charset_info);
  return item;
}


Item *get_system_var(THD *thd, enum_var_type var_type, const char *var_name,
		     uint length, const char *item_name)
{
  Item *item;
  sys_var *var;
  LEX_STRING null_lex_string;

  null_lex_string.str= 0;

  var= find_sys_var(var_name, length);
  DBUG_ASSERT(var != 0);
  if (!(item=var->item(thd, var_type, &null_lex_string)))
    return 0;						// Impossible
  thd->lex->uncacheable(UNCACHEABLE_SIDEEFFECT);
  item->set_name(item_name, 0, system_charset_info);	// Will use original name
  return item;
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


longlong Item_func_found_rows::val_int()
{
  DBUG_ASSERT(fixed == 1);
  THD *thd= current_thd;

  return thd->found_rows();
}
