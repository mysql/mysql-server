/* Copyright (C) 2000-2003

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

#ifdef __GNUC__
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#include <m_ctype.h>
Item_bool_func2* Item_bool_func2::eq_creator(Item *a, Item *b)
{
  return new Item_func_eq(a, b);
}
Item_bool_func2* Item_bool_func2::ne_creator(Item *a, Item *b)
{
  return new Item_func_ne(a, b);
}
Item_bool_func2* Item_bool_func2::gt_creator(Item *a, Item *b)
{
  return new Item_func_gt(a, b);
}
Item_bool_func2* Item_bool_func2::lt_creator(Item *a, Item *b)
{
  return new Item_func_lt(a, b);
}
Item_bool_func2* Item_bool_func2::ge_creator(Item *a, Item *b)
{
  return new Item_func_ge(a, b);
}
Item_bool_func2* Item_bool_func2::le_creator(Item *a, Item *b)
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
  double value=args[0]->val();
  null_value=args[0]->null_value;
  return !null_value && value == 0 ? 1 : 0;
}

/*
  Convert a constant expression or string to an integer.
  This is done when comparing DATE's of different formats and
  also when comparing bigint to strings (in which case the string
  is converted once to a bigint).

  RESULT VALUES
  0	Can't convert item
  1	Item was replaced with an integer version of the item
*/

static bool convert_constant_item(Field *field, Item **item)
{
  if ((*item)->const_item() && (*item)->type() != Item::INT_ITEM)
  {
    if (!(*item)->save_in_field(field, 1) && !((*item)->null_value))
    {
      Item *tmp=new Item_int_with_ref(field->val_int(), *item);
      if (tmp)
	*item=tmp;
      return 1;					// Item was replaced
    }
  }
  return 0;
}

bool Item_bool_func2::set_cmp_charset(CHARSET_INFO *cs1, enum coercion co1,
				      CHARSET_INFO *cs2, enum coercion co2)
{
  if (!my_charset_same(cs1, cs2))
  {
    /* 
       We do allow to use BLOBS together with character strings
       BLOBS have more precedance
    */
    if ((co1 <= co2) && (cs1==&my_charset_bin))
    {
      cmp_charset= cs1;
      coercibility= co1;
    }
    else if ((co2 <= co1) && (cs2==&my_charset_bin))
    {
      cmp_charset= cs2;
      coercibility= co2;
    }
    else
    {
      cmp_charset= 0;
      coercibility= COER_NOCOLL;
      return 1; 
    }
  }
  else if (co1 < co2)
  {
    cmp_charset= cs1;
    coercibility= co1;
  }
  else if (co2 < co1)
  {
    cmp_charset= cs2;
    coercibility= co1;
  }
  else
  { 
    if (cs1 == cs2)
    {
      cmp_charset= cs1;
      coercibility= co1;
    }
    else 
    {
      coercibility= COER_NOCOLL;
      cmp_charset= 0;
      return  (co1 == COER_EXPLICIT) ? 1 : 0;
    }
  }
  return 0;
}


bool Item_bool_func2::fix_fields(THD *thd, struct st_table_list *tables,
				 Item ** ref)
{
  if (Item_int_func::fix_fields(thd, tables, ref))
    return 1;

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

  if (args[0] && args[1])
  {
    uint strong= 0;
    uint weak= 0;

    if ((args[0]->coercibility < args[1]->coercibility) && 
	!my_charset_same(args[0]->charset(), args[1]->charset()) &&
        (args[0]->charset()->state & MY_CS_UNICODE))
    {
      weak= 1;
    }
    else if ((args[1]->coercibility < args[0]->coercibility) && 
	     !my_charset_same(args[0]->charset(), args[1]->charset()) &&
             (args[1]->charset()->state & MY_CS_UNICODE))
    {
      strong= 1;
    }
    
    if (strong || weak)
    {
      Item* conv= 0;
      if (args[weak]->type() == STRING_ITEM)
      {
        String tmp, cstr;
        String *ostr= args[weak]->val_str(&tmp);
        cstr.copy(ostr->ptr(), ostr->length(), ostr->charset(), 
		  args[strong]->charset());
        conv= new Item_string(cstr.ptr(),cstr.length(),cstr.charset(),
			      args[weak]->coercibility);
	((Item_string*)conv)->str_value.copy();
      }
      else
      {
	conv= new Item_func_conv_charset(args[weak],args[strong]->charset());
        conv->coercibility= args[weak]->coercibility;
      }
      args[weak]= conv ? conv : args[weak];
      set_cmp_charset(args[0]->charset(), args[0]->coercibility,
		      args[1]->charset(), args[1]->coercibility);
    }
  }
  if (!cmp_charset)
  {
    /* set_cmp_charset() failed */
    my_error(ER_CANT_AGGREGATE_COLLATIONS,MYF(0),
	     args[0]->charset()->name,coercion_name(args[0]->coercibility),
	     args[1]->charset()->name,coercion_name(args[1]->coercibility),
	     func_name());
    return 1;
  }
  return 0;
}


void Item_bool_func2::fix_length_and_dec()
{
  max_length= 1;				     // Function returns 0 or 1

  /*
    As some compare functions are generated after sql_yacc,
    we have to check for out of memory conditions here
  */
  if (!args[0] || !args[1])
    return;


  // Make a special case of compare with fields to get nicer DATE comparisons
  if (args[0]->type() == FIELD_ITEM)
  {
    Field *field=((Item_field*) args[0])->field;
    if (field->store_for_compare())
    {
      if (convert_constant_item(field,&args[1]))
      {
	cmp.set_cmp_func(this, tmp_arg, tmp_arg+1,
			 INT_RESULT);		// Works for all types.
	cmp_charset= &my_charset_bin;		// For test in fix_fields
	return;
      }
    }
  }
  if (args[1]->type() == FIELD_ITEM)
  {
    Field *field=((Item_field*) args[1])->field;
    if (field->store_for_compare())
    {
      if (convert_constant_item(field,&args[0]))
      {
	cmp.set_cmp_func(this, tmp_arg, tmp_arg+1,
			 INT_RESULT); // Works for all types.
	cmp_charset= &my_charset_bin;		// For test in fix_fields
	return;
      }
    }
  }
  set_cmp_func();
  /*
    We must set cmp_charset here as we may be called from for an automatic
    generated item, like in natural join
  */
  set_cmp_charset(args[0]->charset(), args[0]->coercibility,
		  args[1]->charset(), args[1]->coercibility);
}


int Arg_comparator::set_compare_func(Item_bool_func2 *item, Item_result type)
{
  owner= item;
  func= comparator_matrix[type][(owner->functype() == Item_func::EQUAL_FUNC)?
				1:0];
  if (type == ROW_RESULT)
  {
    uint n= (*a)->cols();
    if (n != (*b)->cols())
    {
      my_error(ER_CARDINALITY_COL, MYF(0), n);
      comparators= 0;
      return 1;
    }
    if (!(comparators= (Arg_comparator *) sql_alloc(sizeof(Arg_comparator)*n)))
      return 1;
    for (uint i=0; i < n; i++)
    {
      if ((*a)->el(i)->cols() != (*b)->el(i)->cols())
      {
	my_error(ER_CARDINALITY_COL, MYF(0), (*a)->el(i)->cols());
	return 1;
      }
      comparators[i].set_cmp_func(owner, (*a)->addr(i), (*b)->addr(i));
    }
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
      return sortcmp(res1,res2,owner->cmp_charset);
    }
  }
  owner->null_value= 1;
  return -1;
}

int Arg_comparator::compare_e_string()
{
  String *res1,*res2;
  res1= (*a)->val_str(&owner->tmp_value1);
  res2= (*b)->val_str(&owner->tmp_value2);
  if (!res1 || !res2)
    return test(res1 == res2);
  return test(sortcmp(res1, res2, owner->cmp_charset) == 0);
}


int Arg_comparator::compare_real()
{
  double val1= (*a)->val();
  if (!(*a)->null_value)
  {
    double val2= (*b)->val();
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

int Arg_comparator::compare_e_real()
{
  double val1= (*a)->val();
  double val2= (*b)->val();
  if ((*a)->null_value || (*b)->null_value)
    return test((*a)->null_value && (*b)->null_value);
  return test(val1 == val2);
}

int Arg_comparator::compare_int()
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

int Arg_comparator::compare_e_int()
{
  longlong val1= (*a)->val_int();
  longlong val2= (*b)->val_int();
  if ((*a)->null_value || (*b)->null_value)
    return test((*a)->null_value && (*b)->null_value);
  return test(val1 == val2);
}


int Arg_comparator::compare_row()
{
  int res= 0;
  (*a)->bring_value();
  (*b)->bring_value();
  uint n= (*a)->cols();
  for (uint i= 0; i<n; i++)
  {
    if ((res= comparators[i].compare()))
      return res;
    if (owner->null_value)
      return -1;
  }
  return res;
}

int Arg_comparator::compare_e_row()
{
  int res= 0;
  (*a)->bring_value();
  (*b)->bring_value();
  uint n= (*a)->cols();
  for (uint i= 0; i<n; i++)
  {
    if ((res= !comparators[i].compare()))
      return 0;
  }
  return 1;
}

bool Item_in_optimizer::preallocate_row()
{
  return (!(cache= Item_cache::get_cache(ROW_RESULT)));
}


bool Item_in_optimizer::fix_fields(THD *thd, struct st_table_list *tables,
				   Item ** ref)
{
  if (args[0]->fix_fields(thd, tables, args))
    return 1;
  if (args[0]->maybe_null)
    maybe_null=1;
  /*
    TODO: Check if following is right
    (set_charset set type of result, not how compare should be used)
  */
  if (args[0]->binary())
    set_charset(&my_charset_bin);
  with_sum_func= args[0]->with_sum_func;
  used_tables_cache= args[0]->used_tables();
  const_item_cache= args[0]->const_item();
  if (!cache && !(cache= Item_cache::get_cache(args[0]->result_type())))
    return 1;
  cache->setup(args[0]);
  if (cache->cols() == 1)
  {
    if (args[0]->used_tables())
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
  }
  if (args[1]->fix_fields(thd, tables, args))
    return 1;
  Item_in_subselect * sub= (Item_in_subselect *)args[1];
  if (args[0]->cols() != sub->engine->cols())
  {
    my_error(ER_CARDINALITY_COL, MYF(0), args[0]->cols());
    return 1;
  }
  if (args[1]->maybe_null)
    maybe_null=1;
  with_sum_func= with_sum_func || args[1]->with_sum_func;
  used_tables_cache|= args[1]->used_tables();
  const_item_cache&= args[1]->const_item();
  return 0;
}

longlong Item_in_optimizer::val_int()
{
  cache->store(args[0]);
  if (cache->null_value)
  {
    null_value= 1;
    return 0;
  }
  longlong tmp= args[1]->val_int_result();
  null_value= args[1]->null_value;
  return tmp;
}

bool Item_in_optimizer::is_null()
{
  cache->store(args[0]);
  return (null_value= (cache->null_value || args[1]->is_null()));
}

longlong Item_func_eq::val_int()
{
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
  return cmp.compare();
}

longlong Item_func_ne::val_int()
{
  int value= cmp.compare();
  return value != 0 && !null_value ? 1 : 0;
}


longlong Item_func_ge::val_int()
{
  int value= cmp.compare();
  return value >= 0 ? 1 : 0;
}


longlong Item_func_gt::val_int()
{
  int value= cmp.compare();
  return value > 0 ? 1 : 0;
}

longlong Item_func_le::val_int()
{
  int value= cmp.compare();
  return value <= 0 && !null_value ? 1 : 0;
}


longlong Item_func_lt::val_int()
{
  int value= cmp.compare();
  return value < 0 && !null_value ? 1 : 0;
}


longlong Item_func_strcmp::val_int()
{
  String *a=args[0]->val_str(&tmp_value1);
  String *b=args[1]->val_str(&tmp_value2);
  if (!a || !b)
  {
    null_value=1;
    return 0;
  }
  int value= sortcmp(a,b,cmp_charset);
  null_value=0;
  return !value ? 0 : (value < 0 ? (longlong) -1 : (longlong) 1);
}


void Item_func_interval::fix_length_and_dec()
{
  if (row->cols() > 8)
  {
    bool consts=1;

    for (uint i=1 ; consts && i < row->cols() ; i++)
    {
      consts&= row->el(i)->const_item();
    }

    if (consts &&
        (intervals=(double*) sql_alloc(sizeof(double)*(row->cols()-1))))
    {
      for (uint i=1 ; i < row->cols(); i++)
        intervals[i-1]=row->el(i)->val();
    }
  }
  maybe_null= 0;
  max_length= 2;
  used_tables_cache|= row->used_tables();
  with_sum_func= with_sum_func || row->with_sum_func;
}


/*
  return -1 if null value,
	  0 if lower than lowest
	  1 - arg_count-1 if between args[n] and args[n+1]
	  arg_count if higher than biggest argument
*/

longlong Item_func_interval::val_int()
{
  double value=row->el(0)->val();
  if (row->el(0)->null_value)
    return -1;				// -1 if null
  if (intervals)
  {					// Use binary search to find interval
    uint start,end;
    start=1; end=row->cols()-2;
    while (start != end)
    {
      uint mid=(start+end+1)/2;
      if (intervals[mid] <= value)
	start=mid;
      else
	end=mid-1;
    }
    return (value < intervals[start]) ? 0 : start+1;
  }

  uint i;
  for (i=1 ; i < row->cols() ; i++)
  {
    if (row->el(i)->val() > value)
      return i-1;
  }
  return i-1;
}

void Item_func_between::fix_length_and_dec()
{
   max_length= 1;

  /*
    As some compare functions are generated after sql_yacc,
    we have to check for out of memory conditons here
  */
  if (!args[0] || !args[1] || !args[2])
    return;
  cmp_type=item_cmp_type(args[0]->result_type(),
           item_cmp_type(args[1]->result_type(),
                         args[2]->result_type()));
  /* QQ: COERCIBILITY */
  if (args[0]->binary() | args[1]->binary() | args[2]->binary())
    cmp_charset= &my_charset_bin;
  else
    cmp_charset= args[0]->charset();

  /*
    Make a special case of compare with date/time and longlong fields.
    They are compared as integers, so for const item this time-consuming
    conversion can be done only once, not for every single comparison
  */
  if (args[0]->type() == FIELD_ITEM)
  {
    Field *field=((Item_field*) args[0])->field;
    if (field->store_for_compare())
    {
      if (convert_constant_item(field,&args[1]))
	cmp_type=INT_RESULT;			// Works for all types.
      if (convert_constant_item(field,&args[2]))
	cmp_type=INT_RESULT;			// Works for all types.
    }
  }
}


longlong Item_func_between::val_int()
{						// ANSI BETWEEN
  if (cmp_type == STRING_RESULT)
  {
    String *value,*a,*b;
    value=args[0]->val_str(&value0);
    if ((null_value=args[0]->null_value))
      return 0;
    a=args[1]->val_str(&value1);
    b=args[2]->val_str(&value2);
    if (!args[1]->null_value && !args[2]->null_value)
      return (sortcmp(value,a,cmp_charset) >= 0 && 
	      sortcmp(value,b,cmp_charset) <= 0) ? 1 : 0;
    if (args[1]->null_value && args[2]->null_value)
      null_value=1;
    else if (args[1]->null_value)
    {
      null_value= sortcmp(value,b,cmp_charset) <= 0; // not null if false range.
    }
    else
    {
      null_value= sortcmp(value,a,cmp_charset) >= 0; // not null if false range.
    }
  }
  else if (cmp_type == INT_RESULT)
  {
    longlong value=args[0]->val_int(),a,b;
    if ((null_value=args[0]->null_value))
      return 0;					/* purecov: inspected */
    a=args[1]->val_int();
    b=args[2]->val_int();
    if (!args[1]->null_value && !args[2]->null_value)
      return (value >= a && value <= b) ? 1 : 0;
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
  else
  {
    double value=args[0]->val(),a,b;
    if ((null_value=args[0]->null_value))
      return 0;					/* purecov: inspected */
    a=args[1]->val();
    b=args[2]->val();
    if (!args[1]->null_value && !args[2]->null_value)
      return (value >= a && value <= b) ? 1 : 0;
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
  return 0;
}

static Item_result item_store_type(Item_result a,Item_result b)
{
  if (a == STRING_RESULT || b == STRING_RESULT)
    return STRING_RESULT;
  else if (a == REAL_RESULT || b == REAL_RESULT)
    return REAL_RESULT;
  else
    return INT_RESULT;
}

void
Item_func_ifnull::fix_length_and_dec()
{
  maybe_null=args[1]->maybe_null;
  max_length=max(args[0]->max_length,args[1]->max_length);
  decimals=max(args[0]->decimals,args[1]->decimals);
  if ((cached_result_type=item_store_type(args[0]->result_type(),
					  args[1]->result_type())) !=
      REAL_RESULT)
    decimals= 0;
  if (set_charset(args[0]->charset(),args[0]->coercibility,
		  args[1]->charset(),args[1]->coercibility))
    my_error(ER_CANT_AGGREGATE_COLLATIONS,MYF(0),
	     args[0]->charset()->name,coercion_name(args[0]->coercibility),
	     args[1]->charset()->name,coercion_name(args[1]->coercibility),
	     func_name());
}


double
Item_func_ifnull::val()
{
  double value=args[0]->val();
  if (!args[0]->null_value)
  {
    null_value=0;
    return value;
  }
  value=args[1]->val();
  if ((null_value=args[1]->null_value))
    return 0.0;
  return value;
}

longlong
Item_func_ifnull::val_int()
{
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

String *
Item_func_ifnull::val_str(String *str)
{
  String *res  =args[0]->val_str(str);
  if (!args[0]->null_value)
  {
    null_value=0;
    res->set_charset(charset());
    return res;
  }
  res=args[1]->val_str(str);
  if ((null_value=args[1]->null_value))
    return 0;
  res->set_charset(charset());
  return res;
}


void
Item_func_if::fix_length_and_dec()
{
  maybe_null=args[1]->maybe_null || args[2]->maybe_null;
  max_length=max(args[1]->max_length,args[2]->max_length);
  decimals=max(args[1]->decimals,args[2]->decimals);
  enum Item_result arg1_type=args[1]->result_type();
  enum Item_result arg2_type=args[2]->result_type();
  bool null1=args[1]->null_value;
  bool null2=args[2]->null_value;

  if (null1)
  {
    cached_result_type= arg2_type;
    set_charset(args[2]->charset());
  }
  else if (null2)
  {
    cached_result_type= arg1_type;
    set_charset(args[1]->charset());
  }
  else if (arg1_type == STRING_RESULT || arg2_type == STRING_RESULT)
  {
    cached_result_type = STRING_RESULT;
    if (set_charset(args[1]->charset(), args[1]->coercibility,
		args[2]->charset(), args[2]->coercibility))
    {
      my_error(ER_CANT_AGGREGATE_COLLATIONS,MYF(0),
	     args[0]->charset()->name,coercion_name(args[0]->coercibility),
	     args[1]->charset()->name,coercion_name(args[1]->coercibility),
	     func_name());
      return;
    }
  }
  else
  {
    set_charset(&my_charset_bin);	// Number
    if (arg1_type == REAL_RESULT || arg2_type == REAL_RESULT)
      cached_result_type = REAL_RESULT;
    else
      cached_result_type=arg1_type;		// Should be INT_RESULT
  }
}


double
Item_func_if::val()
{
  Item *arg= args[0]->val_int() ? args[1] : args[2];
  double value=arg->val();
  null_value=arg->null_value;
  return value;
}

longlong
Item_func_if::val_int()
{
  Item *arg= args[0]->val_int() ? args[1] : args[2];
  longlong value=arg->val_int();
  null_value=arg->null_value;
  return value;
}

String *
Item_func_if::val_str(String *str)
{
  Item *arg= args[0]->val_int() ? args[1] : args[2];
  String *res=arg->val_str(str);
  if (res)
    res->set_charset(charset());
  null_value=arg->null_value;
  return res;
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
    cached_result_type=args[0]->result_type();
  }
}

/*
  nullif () returns NULL if arguments are different, else it returns the
  first argument.
  Note that we have to evaluate the first argument twice as the compare
  may have been done with a different type than return value
*/

double
Item_func_nullif::val()
{
  double value;
  if (!cmp.compare() || null_value)
  {
    null_value=1;
    return 0.0;
  }
  value=args[0]->val();
  null_value=args[0]->null_value;
  return value;
}

longlong
Item_func_nullif::val_int()
{
  longlong value;
  if (!cmp.compare() || null_value)
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
  String *res;
  if (!cmp.compare() || null_value)
  {
    null_value=1;
    return 0;
  }
  res=args[0]->val_str(str);
  null_value=args[0]->null_value;
  return res;
}

/*
  CASE expression 
  Return the matching ITEM or NULL if all compares (including else) failed
*/

Item *Item_func_case::find_item(String *str)
{
  String *first_expr_str,*tmp;
  longlong first_expr_int;
  double   first_expr_real;
  bool int_used, real_used,str_used;
  int_used=real_used=str_used=0;

  /* These will be initialized later */
  LINT_INIT(first_expr_str);
  LINT_INIT(first_expr_int);
  LINT_INIT(first_expr_real);

  // Compare every WHEN argument with it and return the first match
  for (uint i=0 ; i < arg_count ; i+=2)
  {
    if (!first_expr)
    {
      // No expression between CASE and first WHEN
      if (args[i]->val_int())
	return args[i+1];
      continue;
    }
    switch (args[i]->result_type()) {
    case STRING_RESULT:
      if (!str_used)
      {
	str_used=1;
	// We can't use 'str' here as this may be overwritten
	if (!(first_expr_str= first_expr->val_str(&str_value)))
	  return else_expr;			// Impossible
      }
      if ((tmp=args[i]->val_str(str)))		// If not null
      {
	/* QQ: COERCIBILITY */
	if (first_expr_is_binary || args[i]->binary())
	{
	  if (sortcmp(tmp,first_expr_str,&my_charset_bin)==0)
	    return args[i+1];
	}
	else if (sortcmp(tmp,first_expr_str,tmp->charset())==0)
	  return args[i+1];
      }
      break;
    case INT_RESULT:
      if (!int_used)
      {
	int_used=1;
	first_expr_int= first_expr->val_int();
	if (first_expr->null_value)
	  return else_expr;
      }
      if (args[i]->val_int()==first_expr_int && !args[i]->null_value) 
        return args[i+1];
      break;
    case REAL_RESULT: 
      if (!real_used)
      {
	real_used=1;
	first_expr_real= first_expr->val();
	if (first_expr->null_value)
	  return else_expr;
      }
      if (args[i]->val()==first_expr_real && !args[i]->null_value) 
        return args[i+1];
      break;
    case ROW_RESULT:
    default:
      // This case should never be choosen
      DBUG_ASSERT(0);
      break;
    }
  }
  // No, WHEN clauses all missed, return ELSE expression
  return else_expr;
}



String *Item_func_case::val_str(String *str)
{
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

double Item_func_case::val()
{
  char buff[MAX_FIELD_WIDTH];
  String dummy_str(buff,sizeof(buff),default_charset());
  Item *item=find_item(&dummy_str);
  double res;

  if (!item)
  {
    null_value=1;
    return 0;
  }
  res=item->val();
  null_value=item->null_value;
  return res;
}


bool
Item_func_case::fix_fields(THD *thd, TABLE_LIST *tables, Item **ref)
{
  if (first_expr && (first_expr->fix_fields(thd, tables, &first_expr) ||
		     first_expr->check_cols(1)) ||
      else_expr && (else_expr->fix_fields(thd, tables, &else_expr) ||
		    else_expr->check_cols(1)))
    return 1;
  if (Item_func::fix_fields(thd, tables, ref))
    return 1;
  if (first_expr)
  {
    used_tables_cache|=(first_expr)->used_tables();
    const_item_cache&= (first_expr)->const_item();
    with_sum_func= with_sum_func || (first_expr)->with_sum_func;
    first_expr_is_binary= first_expr->binary();
  }
  if (else_expr)
  {
    used_tables_cache|=(else_expr)->used_tables();
    const_item_cache&= (else_expr)->const_item();
    with_sum_func= with_sum_func || (else_expr)->with_sum_func;
  }
  if (!else_expr || else_expr->maybe_null)
    maybe_null=1;				// The result may be NULL
  return 0;
}


void Item_func_case::split_sum_func(Item **ref_pointer_array,
				    List<Item> &fields)
{
  if (first_expr)
  {
    if (first_expr->with_sum_func && first_expr->type() != SUM_FUNC_ITEM)
      first_expr->split_sum_func(ref_pointer_array, fields);
    else if (first_expr->used_tables() || first_expr->type() == SUM_FUNC_ITEM)
    {
      uint el= fields.elements;
      fields.push_front(first_expr);
      ref_pointer_array[el]= first_expr;
      first_expr= new Item_ref(ref_pointer_array + el, 0, first_expr->name);
    }
  }
  if (else_expr)
  {
    if (else_expr->with_sum_func && else_expr->type() != SUM_FUNC_ITEM)
      else_expr->split_sum_func(ref_pointer_array, fields);
    else if (else_expr->used_tables() || else_expr->type() == SUM_FUNC_ITEM)
    {
      uint el= fields.elements;
      fields.push_front(else_expr);
      ref_pointer_array[el]= else_expr;
      else_expr= new Item_ref(ref_pointer_array + el, 0, else_expr->name);
    }
  }
  Item_func::split_sum_func(ref_pointer_array, fields);
}


void Item_func_case::set_outer_resolving()
{
  first_expr->set_outer_resolving();
  else_expr->set_outer_resolving();
  Item_func::set_outer_resolving();
}

void Item_func_case::update_used_tables()
{
  Item_func::update_used_tables();
  if (first_expr)
  {
    used_tables_cache|=(first_expr)->used_tables();
    const_item_cache&= (first_expr)->const_item();
  }
  if (else_expr)
  {
    used_tables_cache|=(else_expr)->used_tables();
    const_item_cache&= (else_expr)->const_item();
  }
}


void Item_func_case::fix_length_and_dec()
{
  max_length=0;
  decimals=0;
  cached_result_type = args[1]->result_type();
  for (uint i=0 ; i < arg_count ; i+=2)
  {
    set_if_bigger(max_length,args[i+1]->max_length);
    set_if_bigger(decimals,args[i+1]->decimals);
  }
  if (else_expr != NULL) 
  {
    set_if_bigger(max_length,else_expr->max_length);
    set_if_bigger(decimals,else_expr->decimals);
  }
}

/* TODO:  Fix this so that it prints the whole CASE expression */

void Item_func_case::print(String *str)
{
  str->append("case ");				// Not yet complete
}

/*
  Coalesce - return first not NULL argument.
*/

String *Item_func_coalesce::val_str(String *str)
{
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

longlong Item_func_coalesce::val_int()
{
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

double Item_func_coalesce::val()
{
  null_value=0;
  for (uint i=0 ; i < arg_count ; i++)
  {
    double res=args[i]->val();
    if (!args[i]->null_value)
      return res;
  }
  null_value=1;
  return 0;
}


void Item_func_coalesce::fix_length_and_dec()
{
  max_length= 0;
  decimals= 0;
  cached_result_type = args[0]->result_type();
  for (uint i=0 ; i < arg_count ; i++)
  {
    set_if_bigger(max_length,args[i]->max_length);
    set_if_bigger(decimals,args[i]->decimals);
  }
}

/****************************************************************************
 Classes and function for the IN operator
****************************************************************************/

static int cmp_longlong(longlong *a,longlong *b)
{
  return *a < *b ? -1 : *a == *b ? 0 : 1;
}

static int cmp_double(double *a,double *b)
{
  return *a < *b ? -1 : *a == *b ? 0 : 1;
}

static int cmp_row(cmp_item_row* a, cmp_item_row* b)
{
  return a->compare(b);
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
    if ((res=(*compare)(base+mid*size,result)) == 0)
      return 1;
    if (res < 0)
      start=mid;
    else
      end=mid-1;
  }
  return (int) ((*compare)(base+start*size,result) == 0);
}

in_string::in_string(uint elements,qsort_cmp cmp_func)
  :in_vector(elements, sizeof(String), cmp_func),
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
    *str= *res;
  if (!str->charset())
  {
    CHARSET_INFO *cs;
    if (!(cs= item->charset()))
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
  compare= (qsort_cmp) cmp_row;
  tmp.store_value(item);
}

in_row::~in_row()
{
  if (base)
    delete [] (cmp_item_row*) base;
}

byte *in_row::get_value(Item *item)
{
  tmp.store_value(item);
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
  :in_vector(elements,sizeof(longlong),(qsort_cmp) cmp_longlong)
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
  :in_vector(elements,sizeof(double),(qsort_cmp) cmp_double)
{}

void in_double::set(uint pos,Item *item)
{
  ((double*) base)[pos]=item->val();
}

byte *in_double::get_value(Item *item)
{
  tmp= item->val();
  if (item->null_value)
    return 0;					/* purecov: inspected */
  return (byte*) &tmp;
}

cmp_item* cmp_item::get_comparator(Item *item)
{
  switch (item->result_type()) {
  case STRING_RESULT:
    return new cmp_item_sort_string(item->charset());
    break;
  case INT_RESULT:
    return new cmp_item_int;
    break;
  case REAL_RESULT:
    return new cmp_item_real;
    break;
  case ROW_RESULT:
    return new cmp_item_row;
    break;
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

void cmp_item_row::store_value(Item *item)
{
  THD *thd= current_thd;
  n= item->cols();
  if ((comparators= (cmp_item **) thd->calloc(sizeof(cmp_item *)*n)))
  {
    item->bring_value();
    item->null_value= 0;
    for (uint i=0; i < n; i++)
      if ((comparators[i]= cmp_item::get_comparator(item->el(i))))
      {
	comparators[i]->store_value(item->el(i));
	item->null_value|= item->el(i)->null_value;
      }
      else
	return;
  }
  else
    return;
}

void cmp_item_row::store_value_by_template(cmp_item *t, Item *item)
{
  cmp_item_row *tmpl= (cmp_item_row*) t;
  if (tmpl->n != item->cols())
  {
    my_error(ER_CARDINALITY_COL, MYF(0), tmpl->n);
    return;
  }
  n= tmpl->n;
  if ((comparators= (cmp_item **) sql_alloc(sizeof(cmp_item *)*n)))
  {
    item->bring_value();
    item->null_value= 0;
    for (uint i=0; i < n; i++)
      if ((comparators[i]= tmpl->comparators[i]->make_same()))
      {
	comparators[i]->store_value_by_template(tmpl->comparators[i],
						item->el(i));
	item->null_value|= item->el(i)->null_value;
      }
      else
	return;
  }
  else
    return;
}

int cmp_item_row::cmp(Item *arg)
{
  arg->null_value= 0;
  if (arg->cols() != n)
  {
    my_error(ER_CARDINALITY_COL, MYF(0), n);
    return 1;
  }
  bool was_null= 0;
  arg->bring_value();
  for (uint i=0; i < n; i++)
    if (comparators[i]->cmp(arg->el(i)))
    {
      if (!arg->el(i)->null_value)
	return 1;
      was_null= 1;
    }
  return (arg->null_value= was_null);
}

int cmp_item_row::compare(cmp_item *c)
{
  int res;
  cmp_item_row *cmp= (cmp_item_row *) c;
  for (uint i=0; i < n; i++)
    if ((res= comparators[i]->compare(cmp->comparators[i])))
      return res;
  return 0;
}

bool Item_func_in::nulls_in_row()
{
  Item **arg,**arg_end;
  for (arg= args, arg_end= args+arg_count; arg != arg_end ; arg++)
  {
    if ((*arg)->null_inside())
      return 1;
  }
  return 0;
}

static int srtcmp_in(const String *x,const String *y)
{
  CHARSET_INFO *cs= x->charset();
  return cs->coll->strnncollsp(cs,
                        (unsigned char *) x->ptr(),x->length(),
			(unsigned char *) y->ptr(),y->length());
}

static int bincmp_in(const String *x,const String *y)
{
  CHARSET_INFO *cs= &my_charset_bin;
  return cs->coll->strnncollsp(cs,
                        (unsigned char *) x->ptr(),x->length(),
			(unsigned char *) y->ptr(),y->length());
}

void Item_func_in::fix_length_and_dec()
{
  /*
    Row item with NULLs inside can return NULL or FALSE => 
    they can't be processed as static
  */
  if (const_item() && !nulls_in_row())
  {
    switch (item->result_type()) {
    case STRING_RESULT:
      if (item->binary())
	array=new in_string(arg_count,(qsort_cmp) srtcmp_in);
      else
	array=new in_string(arg_count,(qsort_cmp) bincmp_in);
      break;
    case INT_RESULT:
      array= new in_longlong(arg_count);
      break;
    case REAL_RESULT:
      array= new in_double(arg_count);
      break;
    case ROW_RESULT:
      array= new in_row(arg_count, item);
      break;
    default:
      DBUG_ASSERT(0);
      return;
    }
    uint j=0;
    for (uint i=0 ; i < arg_count ; i++)
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
  else
  {
    in_item= cmp_item::get_comparator(item);
  }
  maybe_null= item->maybe_null;
  max_length= 1;
  used_tables_cache|=item->used_tables();
  const_item_cache&=item->const_item();
}


void Item_func_in::print(String *str)
{
  str->append('(');
  item->print(str);
  Item_func::print(str);
  str->append(')');
}


longlong Item_func_in::val_int()
{
  if (array)
  {
    int tmp=array->find(item);
    null_value=item->null_value || (!tmp && have_null);
    return tmp;
  }
  in_item->store_value(item);
  if ((null_value=item->null_value))
    return 0;
  have_null= 0;
  for (uint i=0 ; i < arg_count ; i++)
  {
    if (!in_item->cmp(args[i]) && !args[i]->null_value)
      return 1;					// Would maybe be nice with i ?
    have_null|= args[i]->null_value;
  }
  null_value= have_null;
  return 0;
}


void Item_func_in::update_used_tables()
{
  Item_func::update_used_tables();
  item->update_used_tables();
  used_tables_cache|=item->used_tables();
  const_item_cache&=item->const_item();
}

void Item_func_in::split_sum_func(Item **ref_pointer_array, List<Item> &fields)
{
  if (item->with_sum_func && item->type() != SUM_FUNC_ITEM)
    item->split_sum_func(ref_pointer_array, fields);
  else if (item->used_tables() || item->type() == SUM_FUNC_ITEM)
  {
    uint el= fields.elements;
    fields.push_front(item);
    ref_pointer_array[el]= item;
    item= new Item_ref(ref_pointer_array + el, 0, item->name);
  }  
  Item_func::split_sum_func(ref_pointer_array, fields);
}


longlong Item_func_bit_or::val_int()
{
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


bool
Item_cond::fix_fields(THD *thd, TABLE_LIST *tables, Item **ref)
{
  List_iterator<Item> li(list);
  Item *item;
#ifndef EMBEDDED_LIBRARY
  char buff[sizeof(char*)];			// Max local vars in function
#endif
  used_tables_cache=0;
  const_item_cache=0;

  if (thd && check_stack_overrun(thd,buff))
    return 0;					// Fatal error flag is set!
  while ((item=li++))
  {
    while (item->type() == Item::COND_ITEM &&
	   ((Item_cond*) item)->functype() == functype())
    {						// Identical function
      li.replace(((Item_cond*) item)->list);
      ((Item_cond*) item)->list.empty();
#ifdef DELETE_ITEMS
      delete (Item_cond*) item;
#endif
      item= *li.ref();				// new current item
    }
    if (abort_on_null)
      item->top_level_item();
    if (item->fix_fields(thd, tables, li.ref()) || item->check_cols(1))
      return 1; /* purecov: inspected */
    used_tables_cache|=item->used_tables();
    with_sum_func= with_sum_func || item->with_sum_func;
    const_item_cache&=item->const_item();
    if (item->maybe_null)
      maybe_null=1;
  }
  if (thd)
    thd->cond_count+=list.elements;
  fix_length_and_dec();
  fixed= 1;
  return 0;
}

void Item_cond::set_outer_resolving()
{
  Item_func::set_outer_resolving();
  List_iterator<Item> li(list);
  Item *item;
  while ((item= li++))
    item->set_outer_resolving();
}

void Item_cond::split_sum_func(Item **ref_pointer_array, List<Item> &fields)
{
  List_iterator<Item> li(list);
  Item *item;
  used_tables_cache=0;
  const_item_cache=0;
  while ((item=li++))
  {
    if (item->with_sum_func && item->type() != SUM_FUNC_ITEM)
      item->split_sum_func(ref_pointer_array, fields);
    else if (item->used_tables() || item->type() == SUM_FUNC_ITEM)
    {
      uint el= fields.elements;
      fields.push_front(item);
      ref_pointer_array[el]= item;
      li.replace(new Item_ref(ref_pointer_array + el, 0, item->name));
    }
    item->update_used_tables();
    used_tables_cache|=item->used_tables();
    const_item_cache&=item->const_item();
  }
}


table_map
Item_cond::used_tables() const
{						// This caches used_tables
  return used_tables_cache;
}

void Item_cond::update_used_tables()
{
  used_tables_cache=0;
  const_item_cache=1;
  List_iterator_fast<Item> li(list);
  Item *item;
  while ((item=li++))
  {
    item->update_used_tables();
    used_tables_cache|=item->used_tables();
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

/*
  Evalution of AND(expr, expr, expr ...)

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
  List_iterator_fast<Item> li(list);
  Item *item;
  null_value= 0;
  while ((item=li++))
  {
    if (item->val_int() == 0)
    {
      if (abort_on_null || !(null_value= item->null_value))
	return 0;				// return FALSE
    }
  }
  return null_value ? 0 : 1;
}


longlong Item_cond_or::val_int()
{
  List_iterator_fast<Item> li(list);
  Item *item;
  null_value=0;
  while ((item=li++))
  {
    if (item->val_int() != 0)
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
      res->used_tables_cache= a->used_tables() | b->used_tables();
    return res;
  }
  if (((Item_cond_and*) a)->add((Item*) b))
    return 0;
  ((Item_cond_and*) a)->used_tables_cache|= b->used_tables();
  return a;
}


longlong Item_func_isnull::val_int()
{
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
  DBUG_ENTER("Item_is_not_null_test::val_int");
  if (!used_tables_cache)
  {
    owner->was_null|= (!cached_value);
    DBUG_PRINT("info", ("cached :%d", cached_value));
    DBUG_RETURN(cached_value);
  }
  if (args[0]->is_null())
  {
    DBUG_PRINT("info", ("null"))
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
  return args[0]->is_null() ? 0 : 1;
}


longlong Item_func_like::val_int()
{
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
  return my_wildcmp(cmp_charset,
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


bool Item_func_like::fix_fields(THD *thd, TABLE_LIST *tlist, Item ** ref)
{
  if (Item_bool_func2::fix_fields(thd, tlist, ref))
    return 1;

  /*
    We could also do boyer-more for non-const items, but as we would have to
    recompute the tables for each row it's not worth it.
  */
  if (args[1]->const_item() && !use_strnxfrm(charset()) &&
      !(specialflag & SPECIAL_NO_NEW_FUNC))
  {
    String* res2 = args[1]->val_str(&tmp_value2);
    if (!res2)
      return 0;					// Null argument

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
      canDoTurboBM = (tmp == last) && !use_mb(args[0]->charset());
    }

    if (canDoTurboBM)
    {
      pattern     = first + 1;
      pattern_len = len - 2;
      DBUG_PRINT("info", ("Initializing pattern: '%s'", first));
      int *suff = (int*) thd->alloc(sizeof(int)*((pattern_len + 1)*2+
						 alphabet_size));
      bmGs      = suff + pattern_len + 1;
      bmBc      = bmGs + pattern_len + 1;
      turboBM_compute_good_suffix_shifts(suff);
      turboBM_compute_bad_character_shifts();
      DBUG_PRINT("info",("done"));
    }
  }
  return 0;
}

#ifdef USE_REGEX

bool
Item_func_regex::fix_fields(THD *thd, TABLE_LIST *tables, Item **ref)
{
  if (args[0]->fix_fields(thd, tables, args) || args[0]->check_cols(1) ||
      args[1]->fix_fields(thd,tables, args + 1) || args[1]->check_cols(1))
    return 1;					/* purecov: inspected */
  with_sum_func=args[0]->with_sum_func || args[1]->with_sum_func;
  max_length= 1;
  decimals= 0;
  binary_cmp= (args[0]->binary() || args[1]->binary());

  used_tables_cache=args[0]->used_tables() | args[1]->used_tables();
  const_item_cache=args[0]->const_item() && args[1]->const_item();
  if (!regex_compiled && args[1]->const_item())
  {
    char buff[MAX_FIELD_WIDTH];
    String tmp(buff,sizeof(buff),&my_charset_bin);
    String *res=args[1]->val_str(&tmp);
    if (args[1]->null_value)
    {						// Will always return NULL
      maybe_null=1;
      return 0;
    }
    int error;
    if ((error=regcomp(&preg,res->c_ptr(),
		       binary_cmp ? REG_EXTENDED | REG_NOSUB :
		       REG_EXTENDED | REG_NOSUB | REG_ICASE,
		       res->charset())))
    {
      (void) regerror(error,&preg,buff,sizeof(buff));
      my_printf_error(ER_REGEXP_ERROR,ER(ER_REGEXP_ERROR),MYF(0),buff);
      return 1;
    }
    regex_compiled=regex_is_const=1;
    maybe_null=args[0]->maybe_null;
  }
  else
    maybe_null=1;
  fixed= 1;
  return 0;
}


longlong Item_func_regex::val_int()
{
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
    if (!regex_compiled || sortcmp(res2,&prev_regexp,&my_charset_bin))
    {
      prev_regexp.copy(*res2);
      if (regex_compiled)
      {
	regfree(&preg);
	regex_compiled=0;
      }
      if (regcomp(&preg,res2->c_ptr(),
		  binary_cmp ? REG_EXTENDED | REG_NOSUB :
		  REG_EXTENDED | REG_NOSUB | REG_ICASE,
		  res->charset()))

      {
	null_value=1;
	return 0;
      }
      regex_compiled=1;
    }
  }
  null_value=0;
  return regexec(&preg,res->c_ptr(),0,(regmatch_t*) 0,0) ? 0 : 1;
}


Item_func_regex::~Item_func_regex()
{
  if (regex_compiled)
  {
    regfree(&preg);
    regex_compiled=0;
  }
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
  CHARSET_INFO	*cs=system_charset_info;	// QQ Needs to be fixed

  *splm1 = pattern_len;

  if (cmp_charset == &my_charset_bin)
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
  CHARSET_INFO	*cs=system_charset_info;	// QQ Needs to be fixed

  for (i = bmBc; i < end; i++)
    *i = pattern_len;

  if (cmp_charset == &my_charset_bin)
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
  CHARSET_INFO	*cs=system_charset_info;	// QQ Needs to be fixed

  const int plm1=  pattern_len - 1;
  const int tlmpl= text_len - pattern_len;

  /* Searching */
  if (cmp_charset == &my_charset_bin)
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
