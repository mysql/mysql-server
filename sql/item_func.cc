/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

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
#include <m_ctype.h>
#include <hash.h>
#include <time.h>
#include <ft_global.h>
#include "slave.h" // for wait_for_master_pos

/* return TRUE if item is a constant */

bool
eval_const_cond(COND *cond)
{
  return ((Item_func*) cond)->val_int() ? TRUE : FALSE;
}


Item_func::Item_func(List<Item> &list)
{
  arg_count=list.elements;
  if ((args=(Item**) sql_alloc(sizeof(Item*)*arg_count)))
  {
    uint i=0;
    List_iterator<Item> li(list);
    Item *item;

    while ((item=li++))
    {
      args[i++]= item;
      with_sum_func|=item->with_sum_func;
    }
  }
  list.empty();					// Fields are used
}

bool
Item_func::fix_fields(THD *thd,TABLE_LIST *tables)
{
  Item **arg,**arg_end;
  char buff[sizeof(double)];			// Max argument in function
  binary=0;
  used_tables_cache=0;
  const_item_cache=1;

  if (thd && check_stack_overrun(thd,buff))
    return 0;					// Fatal error if flag is set!
  if (arg_count)
  {						// Print purify happy
    for (arg=args, arg_end=args+arg_count; arg != arg_end ; arg++)
    {
      if ((*arg)->fix_fields(thd,tables))
	return 1;				/* purecov: inspected */
      if ((*arg)->maybe_null)
	maybe_null=1;
      if ((*arg)->binary)
	binary=1;
      with_sum_func= with_sum_func || (*arg)->with_sum_func;
      used_tables_cache|=(*arg)->used_tables();
      const_item_cache&= (*arg)->const_item();
    }
  }
  fix_length_and_dec();
  return 0;
}


void Item_func::split_sum_func(List<Item> &fields)
{
  Item **arg,**arg_end;
  for (arg=args, arg_end=args+arg_count; arg != arg_end ; arg++)
  {
    if ((*arg)->with_sum_func && (*arg)->type() != SUM_FUNC_ITEM)
      (*arg)->split_sum_func(fields);
    else if ((*arg)->used_tables() || (*arg)->type() == SUM_FUNC_ITEM)
    {
      fields.push_front(*arg);
      *arg=new Item_ref((Item**) fields.head_ref(),0,(*arg)->name);
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

void Item_func::print(String *str)
{
  str->append(func_name());
  str->append('(');
  for (uint i=0 ; i < arg_count ; i++)
  {
    if (i)
      str->append(',');
    args[i]->print(str);
  }
  str->append(')');
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

bool Item_func::eq(const Item *item) const
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
    if (!args[i]->eq(item_func->args[i]))
      return 0;
  return 1;
}


String *Item_real_func::val_str(String *str)
{
  double nr=val();
  if (null_value)
    return 0; /* purecov: inspected */
  else
    str->set(nr,decimals);
  return str;
}


String *Item_num_func::val_str(String *str)
{
  if (hybrid_type == INT_RESULT)
  {
    longlong nr=val_int();
    if (null_value)
      return 0; /* purecov: inspected */
    else
      str->set(nr);
  }
  else
  {
    double nr=val();
    if (null_value)
      return 0; /* purecov: inspected */
    else
      str->set(nr,decimals);
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


String *Item_int_func::val_str(String *str)
{
  longlong nr=val_int();
  if (null_value)
    return 0;
  else
    str->set(nr);
  return str;
}

/* Change from REAL_RESULT (default) to INT_RESULT if both arguments are integers */

void Item_num_op::find_num_type(void)
{
  if (args[0]->result_type() == INT_RESULT &&
      args[1]->result_type() == INT_RESULT)
    hybrid_type=INT_RESULT;
}

String *Item_num_op::val_str(String *str)
{
  if (hybrid_type == INT_RESULT)
  {
    longlong nr=val_int();
    if (null_value)
      return 0; /* purecov: inspected */
    else
      str->set(nr);
  }
  else
  {
    double nr=val();
    if (null_value)
      return 0; /* purecov: inspected */
    else
      str->set(nr,decimals);
  }
  return str;
}


double Item_func_plus::val()
{
  double value=args[0]->val()+args[1]->val();
  if ((null_value=args[0]->null_value || args[1]->null_value))
    return 0.0;
  return value;
}

longlong Item_func_plus::val_int()
{
  longlong value=args[0]->val_int()+args[1]->val_int();
  if ((null_value=args[0]->null_value || args[1]->null_value))
    return 0;
  return value;
}

double Item_func_minus::val()
{
  double value=args[0]->val() - args[1]->val();
  if ((null_value=args[0]->null_value || args[1]->null_value))
    return 0.0;
  return value;
}

longlong Item_func_minus::val_int()
{
  longlong value=args[0]->val_int() - args[1]->val_int();
  if ((null_value=args[0]->null_value || args[1]->null_value))
    return 0;
  return value;
}

double Item_func_mul::val()
{
  double value=args[0]->val()*args[1]->val();
  if ((null_value=args[0]->null_value || args[1]->null_value))
    return 0.0; /* purecov: inspected */
  return value;
}

longlong Item_func_mul::val_int()
{
  longlong value=args[0]->val_int()*args[1]->val_int();
  if ((null_value=args[0]->null_value || args[1]->null_value))
    return 0; /* purecov: inspected */
  return value;
}


double Item_func_div::val()
{
  double value=args[0]->val();
  double val2=args[1]->val();
  if ((null_value= val2 == 0.0 || args[0]->null_value || args[1]->null_value))
    return 0.0;
  return value/val2;
}

longlong Item_func_div::val_int()
{
  longlong value=args[0]->val_int();
  longlong val2=args[1]->val_int();
  if ((null_value= val2 == 0 || args[0]->null_value || args[1]->null_value))
    return 0;
  return value/val2;
}

void Item_func_div::fix_length_and_dec()
{
  decimals=max(args[0]->decimals,args[1]->decimals)+2;
  max_length=args[0]->max_length - args[0]->decimals + decimals;
  uint tmp=float_length(decimals);
  set_if_smaller(max_length,tmp);
  maybe_null=1;
}

double Item_func_mod::val()
{
  double value= floor(args[0]->val()+0.5);
  double val2=floor(args[1]->val()+0.5);
  if ((null_value=val2 == 0.0 || args[0]->null_value || args[1]->null_value))
    return 0.0; /* purecov: inspected */
  return fmod(value,val2);
}

longlong Item_func_mod::val_int()
{
  longlong value=  args[0]->val_int();
  longlong val2= args[1]->val_int();
  if ((null_value=val2 == 0 || args[0]->null_value || args[1]->null_value))
    return 0; /* purecov: inspected */
  return value % val2;
}

void Item_func_mod::fix_length_and_dec()
{
  max_length=args[1]->max_length;
  decimals=0;
  maybe_null=1;
  find_num_type();
}


double Item_func_neg::val()
{
  double value=args[0]->val();
  null_value=args[0]->null_value;
  return -value;
}

longlong Item_func_neg::val_int()
{
  longlong value=args[0]->val_int();
  null_value=args[0]->null_value;
  return -value;
}

void Item_func_neg::fix_length_and_dec()
{
  decimals=args[0]->decimals;
  max_length=args[0]->max_length;
  hybrid_type= args[0]->result_type() == INT_RESULT ? INT_RESULT : REAL_RESULT;
}

double Item_func_abs::val()
{
  double value=args[0]->val();
  null_value=args[0]->null_value;
  return fabs(value);
}

longlong Item_func_abs::val_int()
{
  longlong value=args[0]->val_int();
  null_value=args[0]->null_value;
  return value >= 0 ? value : -value;
}

void Item_func_abs::fix_length_and_dec()
{
  decimals=args[0]->decimals;
  max_length=args[0]->max_length;
  hybrid_type= args[0]->result_type() == INT_RESULT ? INT_RESULT : REAL_RESULT;
}

double Item_func_log::val()
{
  double value=args[0]->val();
  if ((null_value=(args[0]->null_value || value <= 0.0)))
    return 0.0; /* purecov: inspected */
  return log(value);
}

double Item_func_log10::val()
{
  double value=args[0]->val();
  if ((null_value=(args[0]->null_value || value <= 0.0)))
    return 0.0; /* purecov: inspected */
  return log10(value);
}

double Item_func_exp::val()
{
  double value=args[0]->val();
  if ((null_value=args[0]->null_value))
    return 0.0; /* purecov: inspected */
  return exp(value);
}

double Item_func_sqrt::val()
{
  double value=args[0]->val();
  if ((null_value=(args[0]->null_value || value < 0)))
    return 0.0; /* purecov: inspected */
  return sqrt(value);
}

double Item_func_pow::val()
{
  double value=args[0]->val();
  double val2=args[1]->val();
  if ((null_value=(args[0]->null_value || args[1]->null_value)))
    return 0.0; /* purecov: inspected */
  return pow(value,val2);
}

// Trigonometric functions

double Item_func_acos::val()
{
  double value=args[0]->val();
  if ((null_value=(args[0]->null_value || (value < -1.0 || value > 1.0))))
    return 0.0;
  return fix_result(acos(value));
}

double Item_func_asin::val()
{
  double value=args[0]->val();
  if ((null_value=(args[0]->null_value || (value < -1.0 || value > 1.0))))
    return 0.0;
  return fix_result(asin(value));
}

double Item_func_atan::val()
{
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
  double value=args[0]->val();
  if ((null_value=args[0]->null_value))
    return 0.0;
  return fix_result(cos(value));
}

double Item_func_sin::val()
{
  double value=args[0]->val();
  if ((null_value=args[0]->null_value))
    return 0.0;
  return fix_result(sin(value));
}

double Item_func_tan::val()
{
  double value=args[0]->val();
  if ((null_value=args[0]->null_value))
    return 0.0;
  return fix_result(tan(value));
}


// Shift-functions, same as << and >> in C/C++


longlong Item_func_shift_left::val_int()
{
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
  double value=args[0]->val();
  null_value=args[0]->null_value;
  return (longlong) ceil(value);
}

longlong Item_func_floor::val_int()
{
  double value=args[0]->val();
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
      decimals=tmp;
  }
}

double Item_func_round::val()
{
  double value=args[0]->val();
  int dec=(int) args[1]->val_int();
  uint abs_dec=abs(dec);

  if ((null_value=args[0]->null_value || args[1]->null_value))
    return 0.0;
  double tmp=(abs_dec < array_elements(log_10) ?
	      log_10[abs_dec] : pow(10.0,(double) abs_dec));

  if (truncate)
    return dec < 0 ? floor(value/tmp)*tmp : floor(value*tmp)/tmp;
  return dec < 0 ? rint(value/tmp)*tmp : rint(value*tmp)/tmp;
}


double Item_func_rand::val()
{
  if (arg_count)
  {					// Only use argument once in query
    ulong tmp=((ulong) args[0]->val_int())+55555555L;
    randominit(&current_thd->rand,tmp,tmp/2);
#ifdef DELETE_ITEMS
    delete args[0];
#endif
    arg_count=0;
  }
  return rnd(&current_thd->rand);
}

longlong Item_func_sign::val_int()
{
  double value=args[0]->val();
  null_value=args[0]->null_value;
  return value < 0.0 ? -1 : (value > 0 ? 1 : 0);
}


double Item_func_units::val()
{
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
  binary=0;
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
    if (args[i]->binary)
      binary=1;
  }
}


String *Item_func_min_max::val_str(String *str)
{
  switch (cmp_type) {
  case INT_RESULT:
  {
    longlong nr=val_int();
    if (null_value)
      return 0;
    else
      str->set(nr);
    return str;
  }
  case REAL_RESULT:
  {
    double nr=val();
    if (null_value)
      return 0; /* purecov: inspected */
    else
      str->set(nr,decimals);
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
	  int cmp=binary ? stringcmp(res,res2) : sortcmp(res,res2);
	  if ((cmp_sign < 0 ? cmp : -cmp) < 0)
	    res=res2;
	}
      }
    }
    return res;
  }
  }
  return 0;					// Keep compiler happy
}


double Item_func_min_max::val()
{
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
  String *res=args[0]->val_str(&value);
  if (!res)
  {
    null_value=1;
    return 0; /* purecov: inspected */
  }
  null_value=0;
  return (longlong) (!args[0]->binary) ? res->numchars() : res->length();
}


longlong Item_func_locate::val_int()
{
  String *a=args[0]->val_str(&value1);
  String *b=args[1]->val_str(&value2);
#ifdef USE_MB
  bool binary_str = args[0]->binary || args[1]->binary;
#endif
  if (!a || !b)
  {
    null_value=1;
    return 0; /* purecov: inspected */
  }
  null_value=0;
  uint start=0;
#ifdef USE_MB
  uint start0=0;
#endif
  if (arg_count == 3)
  {
    start=(uint) args[2]->val_int()-1;
#ifdef USE_MB
    if (use_mb(default_charset_info))
    {
      start0=start;
      if (!binary_str)
        start=a->charpos(start);
    }
#endif
    if (start > a->length() || start+b->length() > a->length())
      return 0;
  }
  if (!b->length())				// Found empty string at start
    return (longlong) (start+1);
#ifdef USE_MB
  if (use_mb(default_charset_info) && !binary_str)
  {
    const char *ptr=a->ptr()+start;
    const char *search=b->ptr();
    const char *strend = ptr+a->length();
    const char *end=strend-b->length()+1;
    const char *search_end=search+b->length();
    register  uint32 l;
    while (ptr < end)
    {
      if (*ptr == *search)
      {
        register char *i,*j;
        i=(char*) ptr+1; j=(char*) search+1;
        while (j != search_end)
          if (*i++ != *j++) goto skipp;
        return (longlong) start0+1;
      }
  skipp:
      if ((l=my_ismbchar(default_charset_info,ptr,strend))) ptr+=l;
      else ++ptr;
      ++start0;
    }
    return 0;
  }
#endif /* USE_MB */
  return (longlong) (a->strstr(*b,start)+1) ;
}


longlong Item_func_field::val_int()
{
  String *field;
  if (!(field=item->val_str(&value)))
    return 0;					// -1 if null ?
  for (uint i=0 ; i < arg_count ; i++)
  {
    String *tmp_value=args[i]->val_str(&tmp);
    if (tmp_value && field->length() == tmp_value->length() &&
	!memcmp(field->ptr(),tmp_value->ptr(),tmp_value->length()))
      return (longlong) (i+1);
  }
  return 0;
}


longlong Item_func_ascii::val_int()
{
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
  String *res=args[0]->val_str(&value);
  if (!res)
  {
    null_value=1;
    return 0;
  }
  null_value=0;
  if (!res->length()) return 0;
#ifdef USE_MB
  if (use_mb(default_charset_info) && !args[0]->binary)
  {
    register const char *str=res->ptr();
    register uint32 n=0, l=my_ismbchar(default_charset_info,
                                       str,str+res->length());
    if (!l) return (longlong)((uchar) *str);
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
	enum_value=find_enum(((Field_enum*) field)->typelib,find->ptr(),
			     find->length());
	enum_bit=0;
	if (enum_value)
	  enum_bit=LL(1) << (enum_value-1);
      }
    }
  }
}

static const char separator=',';

longlong Item_func_find_in_set::val_int()
{
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
    const char *f_pos=find->ptr();
    const char *f_end=f_pos+find->length();
    const char *str=buffer->ptr();
    const char *end=str+diff+1;
    const char *real_end=str+buffer->length();
    uint position=1;
    do
    {
      const char *pos= f_pos;
      while (pos != f_end)
      {
	if (toupper(*str) != toupper(*pos))
	  goto not_found;
	str++;
	pos++;
      }
      if (str == real_end || str[0] == separator)
	return (longlong) position;
  not_found:
      while (str < end && str[0] != separator)
	str++;
      position++;
    } while (++str <= end);
  }
  return 0;
}

static char nbits[256] = {
  0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
  1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
  1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
  2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
  1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
  2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
  2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
  3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
  1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
  2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
  2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
  3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
  2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
  3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
  3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
  4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8,
};

uint count_bits(ulonglong v)
{
#if SIZEOF_LONG_LONG > 4
  /* The following code is a bit faster on 16 bit machines than if we would
     only shift v */
  ulong v2=(ulong) (v >> 32);
  return (uint) (uchar) (nbits[(uchar)  v] +
                         nbits[(uchar) (v >> 8)] +
                         nbits[(uchar) (v >> 16)] +
                         nbits[(uchar) (v >> 24)] +
                         nbits[(uchar) (v2)] +
                         nbits[(uchar) (v2 >> 8)] +
                         nbits[(uchar) (v2 >> 16)] +
                         nbits[(uchar) (v2 >> 24)]);
#else
  return (uint) (uchar) (nbits[(uchar)  v] +
                         nbits[(uchar) (v >> 8)] +
                         nbits[(uchar) (v >> 16)] +
                         nbits[(uchar) (v >> 24)]);
#endif
}

longlong Item_func_bit_count::val_int()
{
  ulonglong value= (ulonglong) args[0]->val_int();
  if (args[0]->null_value)
  {
    null_value=1; /* purecov: inspected */
    return 0; /* purecov: inspected */
  }
  return (longlong) count_bits(value);
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
  delete [] buffers;
}


bool
udf_handler::fix_fields(THD *thd,TABLE_LIST *tables,Item_result_field *func,
			uint arg_count, Item **arguments)
{
  char buff[sizeof(double)];			// Max argument in function
  DBUG_ENTER("Item_udf_func::fix_fields");

  if (thd)
  {
    if (check_stack_overrun(thd,buff))
      return 0;					// Fatal error flag is set!
  }
  else
    thd=current_thd;				// In WHERE / const clause
  udf_func *tmp_udf=find_udf(u_d->name,(uint) strlen(u_d->name),1);

  if (!tmp_udf)
  {
    my_printf_error(ER_CANT_FIND_UDF,ER(ER_CANT_FIND_UDF),MYF(0),u_d->name,
		    errno);
    DBUG_RETURN(1);
  }
  u_d=tmp_udf;
  args=arguments;

  /* Fix all arguments */
  func->binary=func->maybe_null=0;
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
      if ((*arg)->fix_fields(thd,tables))
	return 1;
      if ((*arg)->binary)
	func->binary=1;
      if ((*arg)->maybe_null)
	func->maybe_null=1;
      func->with_sum_func= func->with_sum_func || (*arg)->with_sum_func;
      used_tables_cache|=(*arg)->used_tables();
      const_item_cache&=(*arg)->const_item();
      f_args.arg_type[i]=(*arg)->result_type();
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
		      u_d->name,thd->net.last_error);
      free_udf(u_d);
      DBUG_RETURN(1);
    }
    func->max_length=min(initid.max_length,MAX_BLOB_WIDTH);
    func->maybe_null=initid.maybe_null;
    const_item_cache=initid.const_item;
    func->decimals=min(initid.decimals,31);
  }
  initialized=1;
  if (error)
  {
    my_printf_error(ER_CANT_INITIALIZE_UDF,ER(ER_CANT_INITIALIZE_UDF),MYF(0),
		    u_d->name, ER(ER_UNKNOWN_ERROR));
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
    }
  }
  return 0;
}

/* This returns (String*) 0 in case of NULL values */

String *udf_handler::val_str(String *str,String *save_str)
{
  uchar is_null=0;
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
  char *res=func(&initid, &f_args, (char*) str->ptr(), &res_length, &is_null,
		&error);
  if (is_null || !res || error)			// The !res is for safety
  {
    return 0;
  }
  if (res == str->ptr())
  {
    str->length(res_length);
    return str;
  }
  save_str->set(res, res_length);
  return save_str;
}



double Item_func_udf_float::val()
{
  DBUG_ENTER("Item_func_udf_float::val");
  DBUG_PRINT("info",("result_type: %d  arg_count: %d",
		     args[0]->result_type(), arg_count));
  DBUG_RETURN(udf.val(&null_value));
}


String *Item_func_udf_float::val_str(String *str)
{
  double nr=val();
  if (null_value)
    return 0;					/* purecov: inspected */
  else
    str->set(nr,decimals);
  return str;
}


longlong Item_func_udf_int::val_int()
{
  DBUG_ENTER("Item_func_udf_int::val_int");
  DBUG_PRINT("info",("result_type: %d  arg_count: %d",
		     args[0]->result_type(), arg_count));

  DBUG_RETURN(udf.val_int(&null_value));
}


String *Item_func_udf_int::val_str(String *str)
{
  longlong nr=val_int();
  if (null_value)
    return 0;
  else
    str->set(nr);
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

class ULL
{
  char *key;
  uint key_length;

public:
  int count;
  bool locked;
  pthread_cond_t cond;
  pthread_t thread;

  ULL(const char *key_arg,uint length) :key_length(length),count(1),locked(1)
  {
    key=(char*) my_memdup((byte*) key_arg,length,MYF(0));
    pthread_cond_init(&cond,NULL);
    if (key)
    {
      if (hash_insert(&hash_user_locks,(byte*) this))
      {
	my_free((gptr) key,MYF(0));
	key=0;
      }
    }
  }
  ~ULL()
  {
    if (key)
    {
      hash_delete(&hash_user_locks,(byte*) this);
      my_free((gptr) key,MYF(0));
    }
    pthread_cond_destroy(&cond);
  }
  inline bool initialized() { return key != 0; }
  friend void item_user_lock_release(ULL *ull);
  friend char *ull_get_key(const ULL *ull,uint *length,my_bool not_used);
};

char *ull_get_key(const ULL *ull,uint *length,
		  my_bool not_used __attribute__((unused)))
{
  *length=(uint) ull->key_length;
  return (char*) ull->key;
}

void item_user_lock_init(void)
{
  pthread_mutex_init(&LOCK_user_locks,MY_MUTEX_INIT_SLOW);
  hash_init(&hash_user_locks,16,0,0,(hash_get_key) ull_get_key,NULL,0);
}

void item_user_lock_free(void)
{
  hash_free(&hash_user_locks);
}

void item_user_lock_release(ULL *ull)
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
  THD* thd = current_thd;
  String *log_name = args[0]->val_str(&value);
  int event_count;
  
  null_value=0;
  if (thd->slave_thread || !log_name || !log_name->length())
  {
    null_value = 1;
    return 0;
  }
  ulong pos = (ulong)args[1]->val_int();
  if ((event_count = glob_mi.wait_for_pos(thd, log_name, pos)) == -1)
  {
    null_value = 1;
    event_count=0;
  }
  return event_count;
}

/*
  Get a user level lock. If the thread has an old lock this is first released.
  Returns 1:  Got lock
  Returns 0:  Timeout
  Returns NULL: Error
*/

longlong Item_func_get_lock::val_int()
{
  String *res=args[0]->val_str(&value);
  longlong timeout=args[1]->val_int();
  struct timespec abstime;
  THD *thd=current_thd;
  ULL *ull;
  int error;

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

  if (!(ull= ((ULL*) hash_search(&hash_user_locks,(byte*) res->ptr(),
				 res->length()))))
  {
    ull=new ULL(res->ptr(),res->length());
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

  /* structure is now initialized.  Try to get the lock */
  /* Set up control struct to allow others to abort locks */
  pthread_mutex_lock(&thd->mysys_var->mutex);
  thd->proc_info="User lock";
  thd->mysys_var->current_mutex= &LOCK_user_locks;
  thd->mysys_var->current_cond=  &ull->cond;
  pthread_mutex_unlock(&thd->mysys_var->mutex);

  abstime.tv_sec=time((time_t*) 0)+(time_t) timeout;
  abstime.tv_nsec=0;
  while ((error=pthread_cond_timedwait(&ull->cond,&LOCK_user_locks,&abstime))
	 != ETIME && error != ETIMEDOUT && ull->locked)
  {
    if (thd->killed || abort_loop)
    {
      error=EINTR;				// Return NULL
      break;
    }
  }
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
** Release a user level lock.
** Returns 1 if lock released
** 0 if lock wasn't held
** NULL if no such lock
*/

longlong Item_func_release_lock::val_int()
{
  String *res=args[0]->val_str(&value);
  ULL *ull;
  longlong result;
  if (!res || !res->length())
  {
    null_value=1;
    return 0;
  }
  null_value=0;

  result=0;
  pthread_mutex_lock(&LOCK_user_locks);
  if (!(ull= ((ULL*) hash_search(&hash_user_locks,(const byte*) res->ptr(),
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


longlong Item_func_set_last_insert_id::val_int()
{
  longlong value=args[0]->val_int();
  current_thd->insert_id(value);
  null_value=args[0]->null_value;
  return value;
}

/* This function is just used to test speed of different functions */

longlong Item_func_benchmark::val_int()
{
  char buff[MAX_FIELD_WIDTH];
  String tmp(buff,sizeof(buff));
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
    }
  }
  return 0;
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
    entry->type=STRING_RESULT;
    memcpy(entry->name.str, name.str, name.length+1);
    if (hash_insert(hash,(byte*) entry))
    {
      my_free((char*) entry,MYF(0));
      return 0;
    }
  }
  return entry;
}


bool Item_func_set_user_var::fix_fields(THD *thd,TABLE_LIST *tables)
{
  if (!thd)
    thd=current_thd;
  if (Item_func::fix_fields(thd,tables) ||
      !(entry= get_variable(&thd->user_vars, name, 1)))
    return 1;
  entry->update_query_id=thd->query_id;
  return 0;
}


void
Item_func_set_user_var::fix_length_and_dec()
{
  maybe_null=args[0]->maybe_null;
  max_length=args[0]->max_length;
  decimals=args[0]->decimals;
  cached_result_type=args[0]->result_type();
}

void Item_func_set_user_var::update_hash(void *ptr, uint length,
					 Item_result type)
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
    memcpy(entry->value,ptr,length);
    entry->length= length;
    entry->type=type;
  }
  return;

 err:
  current_thd->fatal_error=1;			// Probably end of memory
  null_value=1;
  return;
}


bool
Item_func_set_user_var::update()
{
  switch (cached_result_type) {
  case REAL_RESULT:
    (void) val();
    break;
  case INT_RESULT:
    (void) val_int();
    break;
  case STRING_RESULT:
    char buffer[MAX_FIELD_WIDTH];
    String tmp(buffer,sizeof(buffer));
    (void) val_str(&tmp);
    break;
  }
  return current_thd->fatal_error;
}


double
Item_func_set_user_var::val()
{
  double value=args[0]->val();
  update_hash((void*) &value,sizeof(value), REAL_RESULT);
  return value;
}

longlong
Item_func_set_user_var::val_int()
{
  longlong value=args[0]->val_int();
  update_hash((void*) &value,sizeof(longlong),INT_RESULT);
  return value;
}

String *
Item_func_set_user_var::val_str(String *str)
{
  String *res=args[0]->val_str(str);
  if (!res)					// Null value
    update_hash((void*) 0,0,STRING_RESULT);
  else
    update_hash(res->c_ptr(),res->length()+1,STRING_RESULT);
  return res;
}


user_var_entry *Item_func_get_user_var::get_entry()
{
  if (!entry  || ! entry->value)
  {
    null_value=1;
    return 0;
  }
  null_value=0;
  return entry;
}


String *
Item_func_get_user_var::val_str(String *str)
{
  user_var_entry *entry=get_entry();
  if (!entry)
    return NULL;
  switch (entry->type) {
  case REAL_RESULT:
    str->set(*(double*) entry->value,decimals);
    break;
  case INT_RESULT:
    str->set(*(longlong*) entry->value);
    break;
  case STRING_RESULT:
    if (str->copy(entry->value, entry->length-1))
    {
      null_value=1;
      return NULL;
    }
    break;
  }
  return str;
}


double Item_func_get_user_var::val()
{
  user_var_entry *entry=get_entry();
  if (!entry)
    return 0.0;
  switch (entry->type) {
  case REAL_RESULT:
    return *(double*) entry->value;
  case INT_RESULT:
    return (double) *(longlong*) entry->value;
  case STRING_RESULT:
    return atof(entry->value);			// This is null terminated
  }
  return 0.0;					// Impossible
}


longlong Item_func_get_user_var::val_int()
{
  user_var_entry *entry=get_entry();
  if (!entry)
    return LL(0);
  switch (entry->type) {
  case REAL_RESULT:
    return (longlong) *(double*) entry->value;
  case INT_RESULT:
    return *(longlong*) entry->value;
  case STRING_RESULT:
    return strtoull(entry->value,NULL,10);	// String is null terminated
  }
  return LL(0);					// Impossible
}


void Item_func_get_user_var::fix_length_and_dec()
{
  THD *thd=current_thd;
  maybe_null=1;
  decimals=NOT_FIXED_DEC;
  max_length=MAX_BLOB_WIDTH;
  if ((entry= get_variable(&thd->user_vars, name, 0)))
    const_var_flag= thd->query_id != entry->update_query_id;
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

longlong Item_func_inet_aton::val_int()
{
  uint byte_result = 0;
  ulonglong result = 0;			// We are ready for 64 bit addresses
  const char *p,* end;
  char c = '.'; // we mark c to indicate invalid IP in case length is 0
  char buff[36];

  String *s,tmp(buff,sizeof(buff));
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
      result= (result << 8) + (ulonglong) byte_result;
      byte_result = 0;
    }
    else
      goto err;					// Invalid character
  }
  if (c != '.')					// IP number can't end on '.'
    return (result << 8) + (ulonglong) byte_result;

err:
  null_value=1;
  return 0;
}

double Item_func_match::val()
{
  if (ft_handler==NULL)
    init_search(1);

  if ((null_value= (ft_handler==NULL)))
    return 0.0;

  if (join_key)
  {
    if (table->file->ft_handler)
      return ft_get_relevance(ft_handler);

    join_key=0; // Magic here ! See ha_myisam::ft_read()
  }

  /* we'll have to find ft_relevance manually in ft_handler array */

  int a,b,c;
  FT_DOC  *docs=ft_handler->doc;
  my_off_t docid=table->file->row_position();

  if ((null_value=(docid==HA_OFFSET_ERROR)))
    return 0.0;

  // Assuming docs[] is sorted by dpos...

  for (a=0, b=ft_handler->ndocs, c=(a+b)/2; b-a>1; c=(a+b)/2)
  {
    if (docs[c].dpos > docid)
      b=c;
    else
      a=c;
  }
  if (docs[a].dpos == docid)
    return docs[a].weight;
  else
    return 0.0;

}

void Item_func_match::init_search(bool no_order)
{
  if (ft_handler)
    return;

  if (master)
  {
    join_key=master->join_key=join_key|master->join_key;
    master->init_search(no_order);
    ft_handler=master->ft_handler;
    join_key=master->join_key;
    return;
  }

  String *ft_tmp=0;
  char tmp1[FT_QUERY_MAXLEN];
  String tmp2(tmp1,sizeof(tmp1));

  // MATCH ... AGAINST (NULL) is meaningless, but possible 
  if (!(ft_tmp=key_item()->val_str(&tmp2)))
  {
    ft_tmp=&tmp2;
    tmp2.set("",0);
  }

  ft_handler=(FT_DOCLIST *)
     table->file->ft_init_ext(key, (byte*) ft_tmp->ptr(), ft_tmp->length(),
                              join_key && !no_order);

  if (join_key)
  {
    table->file->ft_handler=ft_handler;
    return;
  }
}

bool Item_func_match::fix_fields(THD *thd,struct st_table_list *tlist)
{
  List_iterator<Item> li(fields);
  Item *item;

  maybe_null=1;
  join_key=0;

  /* Why testing for const_item ? Monty */
  /* I'll remove it later, but this should include modifications to
     find_best and auto_close as complement to auto_init code above. SerG */
  /* I'd rather say now that const_item is assumed in quite a bit of
     places, so it would be difficult to remove. SerG */
  if (Item_func::fix_fields(thd,tlist) || !const_item())
    return 1;

  while ((item=li++))
  {
    if (item->fix_fields(thd,tlist))
      return 1;
    if (item->type() == Item::REF_ITEM)
      li.replace(item= *((Item_ref *)item)->ref);
    if (item->type() != Item::FIELD_ITEM || !item->used_tables())
      return 1;
    used_tables_cache|=item->used_tables();
  }
  /* check that all columns come from the same table */
  if (count_bits(used_tables_cache) != 1)
    return 1;
  const_item_cache=0;
  table=((Item_field *)fields.head())->field->table;
  return 0;
}


bool Item_func_match::fix_index()
{
  List_iterator<Item> li(fields);
  Item_field *item;
  uint ft_to_key[MAX_KEY], ft_cnt[MAX_KEY], fts=0, key;

  for (key=0 ; key<table->keys ; key++)
  {
    if ((table->key_info[key].flags & HA_FULLTEXT) &&
        (table->keys_in_use_for_query & (((key_map)1) << key)))
    {
      ft_to_key[fts]=key;
      ft_cnt[fts]=0;
      fts++;
    }
  }

  if (!fts)
  {
    my_printf_error(ER_FT_MATCHING_KEY_NOT_FOUND,
                 ER(ER_FT_MATCHING_KEY_NOT_FOUND),MYF(0));
    return 1;
  }

  while ((item=(Item_field*)(li++)))
  {
    for (key=0 ; key<fts ; key++)
    {
      KEY *ft_key=&table->key_info[ft_to_key[key]];
      uint key_parts=ft_key->key_parts;

      for (uint part=0 ; part < key_parts ; part++)
      {
	if (item->field->eq(ft_key->key_part[part].field))
	  ft_cnt[key]++;
      }
    }
  }

  uint max_cnt=0, mkeys=0;
  for (key=0 ; key<fts ; key++)
  {
    if (ft_cnt[key] > max_cnt)
    {
      mkeys=0;
      max_cnt=ft_cnt[mkeys]=ft_cnt[key];
      ft_to_key[mkeys]=ft_to_key[key];
      continue;
    }
    if (max_cnt && ft_cnt[key] == max_cnt)
    {
      mkeys++;
      ft_cnt[mkeys]=ft_cnt[key];
      ft_to_key[mkeys]=ft_to_key[key];
      continue;
    }
  }

  for (key=0 ; key<=mkeys ; key++)
  {
    // for now, partial keys won't work. SerG
    if (max_cnt < fields.elements ||
        max_cnt < table->key_info[ft_to_key[key]].key_parts)
      continue;

    this->key=ft_to_key[key];

    return 0;
  }

  my_printf_error(ER_FT_MATCHING_KEY_NOT_FOUND,
               ER(ER_FT_MATCHING_KEY_NOT_FOUND),MYF(0));
  return 1;
}

bool Item_func_match::eq(const Item *item) const
{
  if (item->type() != FUNC_ITEM)
    return 0;

  if (func_name() != ((Item_func*)item)->func_name())
    return 0;

  Item_func_match *ifm=(Item_func_match*) item;

  if (key == ifm->key && table == ifm->table &&
      key_item()->eq(ifm->key_item()))
    return 1;

  return 0;
}


/***************************************************************************
  System variables
  This has to be recoded after we get more than 3 system variables
****************************************************************************/

Item *get_system_var(LEX_STRING name)
{
  if (!strcmp(name.str,"IDENTITY"))
    return new Item_int((char*) "@@IDENTITY",
			current_thd->insert_id(),21);
  my_error(ER_UNKNOWN_SYSTEM_VARIABLE,MYF(0),name);
  return 0;
}
