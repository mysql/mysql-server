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


#ifdef __GNUC__
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#include <m_ctype.h>
#include "my_dir.h"

/*****************************************************************************
** Item functions
*****************************************************************************/

/* Init all special items */

void item_init(void)
{
  item_user_lock_init();
}

Item::Item()
{
  marker=0;
  binary=maybe_null=null_value=with_sum_func=0;
  name=0;
  decimals=0; max_length=0;
  next=current_thd->free_list;			// Put in free list
  current_thd->free_list=this;
}

void Item::set_name(char *str,uint length)
{
  if (!length)
    name=str;					// Used by AS
  else
  {
    while (length && !isgraph(*str))
    {						// Fix problem with yacc
      length--;
      str++;
    }
    name=sql_strmake(str,min(length,MAX_FIELD_WIDTH));
  }
}

bool Item::eq(const Item *item) const		// Only doing this on conds
{
  return type() == item->type() && name && item->name &&
    !my_strcasecmp(name,item->name);
}

/*
  Get the value of the function as a TIME structure.
  As a extra convenience the time structure is reset on error!
 */

bool Item::get_date(TIME *ltime,bool fuzzydate)
{
  char buff[40];
  String tmp(buff,sizeof(buff)),*res;
  if (!(res=val_str(&tmp)) ||
      str_to_TIME(res->ptr(),res->length(),ltime,0) == TIMESTAMP_NONE)
  {
    bzero((char*) ltime,sizeof(*ltime));
    return 1;
  }
  return 0;
}

/*
  Get time of first argument.
  As a extra convenience the time structure is reset on error!
 */

bool Item::get_time(TIME *ltime)
{
  char buff[40];
  String tmp(buff,sizeof(buff)),*res;
  if (!(res=val_str(&tmp)) ||
      str_to_time(res->ptr(),res->length(),ltime))
  {
    bzero((char*) ltime,sizeof(*ltime));
    return 1;
  }
  return 0;
}

Item_field::Item_field(Field *f) :Item_ident(NullS,f->table_name,f->field_name)
{
  set_field(f);
}


void Item_field::set_field(Field *field_par)
{
  field=result_field=field_par;			// for easy coding with fields
  maybe_null=field->maybe_null();
  max_length=field_par->field_length;
  decimals= field->decimals();
  table_name=field_par->table_name;
  field_name=field_par->field_name;
  binary=field_par->binary();
}

const char *Item_ident::full_name() const
{
  char *tmp;
  if (!table_name)
    return field_name ? field_name : name ? name : "tmp_field";
  if (db_name)
  {
    tmp=(char*) sql_alloc((uint) strlen(db_name)+(uint) strlen(table_name)+
			  (uint) strlen(field_name)+3);
    strxmov(tmp,db_name,".",table_name,".",field_name,NullS);
  }
  else
  {
    tmp=(char*) sql_alloc((uint) strlen(table_name)+
			  (uint) strlen(field_name)+2);
    strxmov(tmp,table_name,".",field_name,NullS);
  }
  return tmp;
}

/* ARGSUSED */
String *Item_field::val_str(String *str)
{
  if ((null_value=field->is_null()))
    return 0;
  return field->val_str(str,&str_value);
}

double Item_field::val()
{
  if ((null_value=field->is_null()))
    return 0.0;
  return field->val_real();
}

longlong Item_field::val_int()
{
  if ((null_value=field->is_null()))
    return 0;
  return field->val_int();
}


String *Item_field::str_result(String *str)
{
  if ((null_value=result_field->is_null()))
    return 0;
  return result_field->val_str(str,&str_value);
}

bool Item_field::get_date(TIME *ltime,bool fuzzydate)
{
  if ((null_value=field->is_null()) || field->get_date(ltime,fuzzydate))
  {
    bzero((char*) ltime,sizeof(*ltime));
    return 1;
  }
  return 0;
}

bool Item_field::get_time(TIME *ltime)
{
  if ((null_value=field->is_null()) || field->get_time(ltime))
  {
    bzero((char*) ltime,sizeof(*ltime));
    return 1;
  }
  return 0;
}

double Item_field::val_result()
{
  if ((null_value=result_field->is_null()))
    return 0.0;
  return result_field->val_real();
}

longlong Item_field::val_int_result()
{
  if ((null_value=result_field->is_null()))
    return 0;
  return result_field->val_int();
}

bool Item_field::eq(const Item *item) const
{
  return item->type() == FIELD_ITEM && ((Item_field*) item)->field == field;
}

table_map Item_field::used_tables() const
{
  if (field->table->const_table)
    return 0;					// const item
  return field->table->map;
}


String *Item_int::val_str(String *str)
{
  str->set(value);
  return str;
}

void Item_int::print(String *str)
{
  if (!name)
  {
    str_value.set(value);
    name=str_value.c_ptr();
  }
  str->append(name);
}


String *Item_real::val_str(String *str)
{
  str->set(value,decimals);
  return str;
}

void Item_string::print(String *str)
{
  str->append('\'');
  str->append(full_name());
  str->append('\'');
}

bool Item_null::eq(const Item *item) const { return item->type() == type(); }
double Item_null::val() { null_value=1; return 0.0; }
longlong Item_null::val_int() { null_value=1; return 0; }
/* ARGSUSED */
String *Item_null::val_str(String *str)
{ null_value=1; return 0;}


void Item_copy_string::copy()
{
  String *res=item->val_str(&str_value);
  if (res && res != &str_value)
    str_value.copy(*res);
  null_value=item->null_value;
}

/* ARGSUSED */
String *Item_copy_string::val_str(String *str)
{
  if (null_value)
    return (String*) 0;
  return &str_value;
}

/*
** Functions to convert item to field (for send_fields)
*/

/* ARGSUSED */
bool Item::fix_fields(THD *thd,
		      struct st_table_list *list)
{
  return 0;
}

bool Item_field::fix_fields(THD *thd,TABLE_LIST *tables)
{
  if (!field)
  {
    Field *tmp;
    if (!(tmp=find_field_in_tables(thd,this,tables)))
      return 1;
    set_field(tmp);
  }
  return 0;
}


void Item::init_make_field(Send_field *tmp_field,
			   enum enum_field_types field_type)
{
  tmp_field->table_name=(char*) "";
  tmp_field->col_name=name;
  tmp_field->flags=maybe_null ? 0 : NOT_NULL_FLAG;
  tmp_field->type=field_type;
  tmp_field->length=max_length;
  tmp_field->decimals=decimals;
}

/* ARGSUSED */
void Item_field::make_field(Send_field *tmp_field)
{
  field->make_field(tmp_field);
  if (name)
    tmp_field->col_name=name;			// Use user supplied name
}

void Item_int::make_field(Send_field *tmp_field)
{
  init_make_field(tmp_field,FIELD_TYPE_LONGLONG);
}

void Item_real::make_field(Send_field *tmp_field)
{
  init_make_field(tmp_field,FIELD_TYPE_DOUBLE);
}

void Item_string::make_field(Send_field *tmp_field)
{
  init_make_field(tmp_field,FIELD_TYPE_STRING);
}

void Item_datetime::make_field(Send_field *tmp_field)
{
  init_make_field(tmp_field,FIELD_TYPE_DATETIME);
}

void Item_null::make_field(Send_field *tmp_field)
{
  init_make_field(tmp_field,FIELD_TYPE_NULL);
  tmp_field->length=4;
}

void Item_func::make_field(Send_field *tmp_field)
{
  init_make_field(tmp_field, ((result_type() == STRING_RESULT) ?
			      FIELD_TYPE_VAR_STRING :
			      (result_type() == INT_RESULT) ?
			      FIELD_TYPE_LONGLONG : FIELD_TYPE_DOUBLE));
}

void Item_avg_field::make_field(Send_field *tmp_field)
{
  init_make_field(tmp_field,FIELD_TYPE_DOUBLE);
}

void Item_std_field::make_field(Send_field *tmp_field)
{
  init_make_field(tmp_field,FIELD_TYPE_DOUBLE);
}

/*
** Set a field:s value from a item
*/


void Item_field::save_org_in_field(Field *to)
{
  if (field->is_null())
  {
    null_value=1;
    set_field_to_null(to);
  }
  else
  {
    to->set_notnull();
    field_conv(to,field);
    null_value=0;
  }
}

bool Item_field::save_in_field(Field *to)
{
  if (result_field->is_null())
  {
    null_value=1;
    return set_field_to_null(to);
  }
  else
  {
    to->set_notnull();
    field_conv(to,result_field);
    null_value=0;
  }
  return 0;
}


bool Item_null::save_in_field(Field *field)
{
  return set_field_to_null(field);
}


bool Item::save_in_field(Field *field)
{
  if (result_type() == STRING_RESULT ||
      result_type() == REAL_RESULT &&
      field->result_type() == STRING_RESULT)
  {
    String *result;
    char buff[MAX_FIELD_WIDTH];		// Alloc buffer for small columns
    str_value.set_quick(buff,sizeof(buff));
    result=val_str(&str_value);
    if (null_value)
      return set_field_to_null(field);
    field->set_notnull();
    field->store(result->ptr(),result->length());
    str_value.set_quick(0, 0);
  }
  else if (result_type() == REAL_RESULT)
  {
    double nr=val();
    if (null_value)
      return set_field_to_null(field);
    field->set_notnull();
    field->store(nr);
  }
  else
  {
    longlong nr=val_int();
    if (null_value)
      return set_field_to_null(field);
    field->set_notnull();
    field->store(nr);
  }
  return 0;
}

bool Item_string::save_in_field(Field *field)
{
  String *result;
  result=val_str(&str_value);
  if (null_value)
    return set_field_to_null(field);
  field->set_notnull();
  field->store(result->ptr(),result->length());
  return 0;
}

bool Item_int::save_in_field(Field *field)
{
  longlong nr=val_int();
  if (null_value)
    return set_field_to_null(field);
  field->set_notnull();
  field->store(nr);
  return 0;
}

bool Item_real::save_in_field(Field *field)
{
  double nr=val();
  if (null_value)
    return set_field_to_null(field);
  field->set_notnull();
  field->store(nr);
  return 0;
}

/****************************************************************************
** varbinary item
** In string context this is a binary string
** In number context this is a longlong value.
****************************************************************************/

inline uint char_val(char X)
{
  return (uint) (X >= '0' && X <= '9' ? X-'0' :
		 X >= 'A' && X <= 'Z' ? X-'A'+10 :
		 X-'a'+10);
}

Item_varbinary::Item_varbinary(const char *str, uint str_length)
{
  name=(char*) str-2;				// Lex makes this start with 0x
  max_length=(str_length+1)/2;
  char *ptr=(char*) sql_alloc(max_length+1);
  if (!ptr)
    return;
  str_value.set(ptr,max_length);
  char *end=ptr+max_length;
  if (max_length*2 != str_length)
    *ptr++=char_val(*str++);			// Not even, assume 0 prefix
  while (ptr != end)
  {
    *ptr++= (char) (char_val(str[0])*16+char_val(str[1]));
    str+=2;
  }
  *ptr=0;					// Keep purify happy
  binary=1;					// Binary is default
}

longlong Item_varbinary::val_int()
{
  char *end=(char*) str_value.ptr()+str_value.length(),
       *ptr=end-min(str_value.length(),sizeof(longlong));

  ulonglong value=0;
  for (; ptr != end ; ptr++)
    value=(value << 8)+ (ulonglong) (uchar) *ptr;
  return (longlong) value;
}


bool Item_varbinary::save_in_field(Field *field)
{
  field->set_notnull();
  if (field->result_type() == STRING_RESULT)
  {
    field->store(str_value.ptr(),str_value.length());
  }
  else
  {
    longlong nr=val_int();
    field->store(nr);
  }
  return 0;
}


void Item_varbinary::make_field(Send_field *tmp_field)
{
  init_make_field(tmp_field,FIELD_TYPE_STRING);
}

/*
** pack data in buffer for sending
*/

bool Item::send(String *packet)
{
  char buff[MAX_FIELD_WIDTH];
  String s(buff,sizeof(buff)),*res;
  if (!(res=val_str(&s)))
    return net_store_null(packet);
  CONVERT *convert;
  if ((convert=current_thd->convert_set))
    return convert->store(packet,res->ptr(),res->length());
  return net_store_data(packet,res->ptr(),res->length());
}

bool Item_null::send(String *packet)
{
  return net_store_null(packet);
}

/*
  This is used for HAVING clause
  Find field in select list having the same name
 */

bool Item_ref::fix_fields(THD *thd,TABLE_LIST *tables)
{
  if (!ref)
  {
    if (!(ref=find_item_in_list(this,thd->lex.item_list)))
      return 1;
    max_length= (*ref)->max_length;
    maybe_null= (*ref)->maybe_null;
    decimals=	(*ref)->decimals;
    binary=	(*ref)->binary;
  }
  return 0;
}

/*
** If item is a const function, calculate it and return a const item
** The original item is freed if not returned
*/

Item_result item_cmp_type(Item_result a,Item_result b)
{
  if (a == STRING_RESULT && b == STRING_RESULT)
    return STRING_RESULT;
  else if (a == INT_RESULT && b == INT_RESULT)
    return INT_RESULT;
  else
    return REAL_RESULT;
}


Item *resolve_const_item(Item *item,Item *comp_item)
{
  if (item->basic_const_item())
    return item;				// Can't be better
  Item_result res_type=item_cmp_type(comp_item->result_type(),
				     item->result_type());
  char *name=item->name;			// Alloced by sql_alloc

  if (res_type == STRING_RESULT)
  {
    char buff[MAX_FIELD_WIDTH];
    String tmp(buff,sizeof(buff)),*result;
    result=item->val_str(&tmp);
    if (item->null_value)
    {
#ifdef DELETE_ITEMS
      delete item;
#endif
      return new Item_null(name);
    }
    uint length=result->length();
    char *tmp_str=sql_strmake(result->ptr(),length);
#ifdef DELETE_ITEMS
    delete item;
#endif
    return new Item_string(name,tmp_str,length);
  }
  if (res_type == INT_RESULT)
  {
    longlong result=item->val_int();
    uint length=item->max_length;
    bool null_value=item->null_value;
#ifdef DELETE_ITEMS
    delete item;
#endif
    return (null_value ? (Item*) new Item_null(name) :
	    (Item*) new Item_int(name,result,length));
  }
  else
  {						// It must REAL_RESULT
    double result=item->val();
    uint length=item->max_length,decimals=item->decimals;
    bool null_value=item->null_value;
#ifdef DELETE_ITEMS
    delete item;
#endif
    return (null_value ? (Item*) new Item_null(name) :
	    (Item*) new Item_real(name,result,decimals,length));
  }
}

/*
  Return true if the value stored in the field is equal to the const item
  We need to use this on the range optimizer because in some cases
  we can't store the value in the field without some precision/character loss.
*/

bool field_is_equal_to_item(Field *field,Item *item)
{

  Item_result res_type=item_cmp_type(field->result_type(),
				     item->result_type());
  if (res_type == STRING_RESULT)
  {
    char item_buff[MAX_FIELD_WIDTH];
    char field_buff[MAX_FIELD_WIDTH];
    String item_tmp(item_buff,sizeof(item_buff)),*item_result;
    String field_tmp(field_buff,sizeof(field_buff));
    item_result=item->val_str(&item_tmp);
    if (item->null_value)
      return 1;					// This must be true
    field->val_str(&field_tmp,&field_tmp);
    return !stringcmp(&field_tmp,item_result);
  }
  if (res_type == INT_RESULT)
    return 1;					// Both where of type int
  double result=item->val();
  if (item->null_value)
    return 1;
  return result == field->val_real();
}


/*****************************************************************************
** Instantiate templates
*****************************************************************************/

#ifdef __GNUC__
template class List<Item>;
template class List_iterator<Item>;
template class List<List_item>;
#endif
