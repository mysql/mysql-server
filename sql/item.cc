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

static void mark_as_dependent(THD *thd,
			      SELECT_LEX *last, SELECT_LEX *current,
			      Item_ident *item);

/*****************************************************************************
** Item functions
*****************************************************************************/

/* Init all special items */

void item_init(void)
{
  item_user_lock_init();
}

Item::Item():
  fixed(0)
{
  marker= 0;
  maybe_null=null_value=with_sum_func=unsigned_flag=0;
  collation.set(default_charset(), DERIVATION_COERCIBLE);
  name= 0;
  decimals= 0; max_length= 0;
  THD *thd= current_thd;
  next= thd->free_list;			// Put in free list
  thd->free_list= this;
  /*
    Item constructor can be called during execution other then SQL_COM
    command => we should check thd->lex.current_select on zero (thd->lex
    can be uninitialised)
  */
  if (thd->lex.current_select)
  {
    SELECT_LEX_NODE::enum_parsing_place place= 
      thd->lex.current_select->parsing_place;
    if (place == SELECT_LEX_NODE::SELECT_LIST ||
	place == SELECT_LEX_NODE::IN_HAVING)
      thd->lex.current_select->select_n_having_items++;
  }
}

/*
  Constructor used by Item_field, Item_ref & agregate (sum) functions.
  Used for duplicating lists in processing queries with temporary
  tables
*/
Item::Item(THD *thd, Item &item):
  str_value(item.str_value),
  name(item.name),
  max_length(item.max_length),
  marker(item.marker),
  decimals(item.decimals),
  maybe_null(item.maybe_null),
  null_value(item.null_value),
  unsigned_flag(item.unsigned_flag),
  with_sum_func(item.with_sum_func),
  fixed(item.fixed),
  collation(item.collation)
{
  next=thd->free_list;			// Put in free list
  thd->free_list= this;
}

// Constructor used by Item_field & Item_ref (see Item comment)
Item_ident::Item_ident(THD *thd, Item_ident &item):
  Item(thd, item),
  db_name(item.db_name),
  table_name(item.table_name),
  field_name(item.field_name),
  depended_from(item.depended_from)
{}

bool Item_ident::remove_dependence_processor(byte * arg)
{
  DBUG_ENTER("Item_ident::remove_dependence_processor");
  if (depended_from == (st_select_lex *) arg)
    depended_from= 0;
  DBUG_RETURN(0);
}


bool Item::check_cols(uint c)
{
  if (c != 1)
  {
    my_error(ER_OPERAND_COLUMNS, MYF(0), c);
    return 1;
  }
  return 0;
}


void Item::set_name(const char *str, uint length, CHARSET_INFO *cs)
{
  if (!length)
  {
    /* Empty string, used by AS or internal function like last_insert_id() */
    name= (char*) str;
    return;
  }
  while (length && !my_isgraph(cs,*str))
  {						// Fix problem with yacc
    length--;
    str++;
  }
  if (!my_charset_same(cs, system_charset_info))
  {
    uint32 res_length;
    name= sql_strmake_with_convert(str, length, cs,
				   MAX_ALIAS_NAME, system_charset_info,
				   &res_length);
  }
  else
    name=sql_strmake(str, min(length,MAX_ALIAS_NAME));
}


/*
  This function is called when:
  - Comparing items in the WHERE clause (when doing where optimization)
  - When trying to find an ORDER BY/GROUP BY item in the SELECT part
*/

bool Item::eq(const Item *item, bool binary_cmp) const
{
  return type() == item->type() && name && item->name &&
    !my_strcasecmp(system_charset_info,name,item->name);
}

bool Item_string::eq(const Item *item, bool binary_cmp) const
{
  if (type() == item->type())
  {
    if (binary_cmp)
      return !sortcmp(&str_value, &item->str_value, &my_charset_bin);
    return !sortcmp(&str_value, &item->str_value, collation.collation);
  }
  return 0;
}


/*
  Get the value of the function as a TIME structure.
  As a extra convenience the time structure is reset on error!
 */

bool Item::get_date(TIME *ltime,bool fuzzydate)
{
  char buff[40];
  String tmp(buff,sizeof(buff), &my_charset_bin),*res;
  if (!(res=val_str(&tmp)) ||
      str_to_TIME(res->ptr(),res->length(),ltime,fuzzydate) == TIMESTAMP_NONE)
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
  String tmp(buff,sizeof(buff),&my_charset_bin),*res;
  if (!(res=val_str(&tmp)) ||
      str_to_time(res->ptr(),res->length(),ltime))
  {
    bzero((char*) ltime,sizeof(*ltime));
    return 1;
  }
  return 0;
}

CHARSET_INFO * Item::default_charset() const
{
  return current_thd->variables.collation_connection;
}

bool DTCollation::aggregate(DTCollation &dt)
{
  if (!my_charset_same(collation, dt.collation))
  {
    /* 
       We do allow to use binary strings (like BLOBS)
       together with character strings.
       Binaries have more precedance than a character
       string of the same derivation.
    */
    if (collation == &my_charset_bin)
    {
      if (derivation <= dt.derivation)
	; // Do nothing
      else
	set(dt);
    }
    else if (dt.collation == &my_charset_bin)
    {
      if (dt.derivation <= derivation)
        set(dt);
      else
       ; // Do nothing
    }
    else
    {
      set(0, DERIVATION_NONE);
      return 1; 
    }
  }
  else if (derivation < dt.derivation)
  {
    // Do nothing
  }
  else if (dt.derivation < derivation)
  {
    set(dt);
  }
  else
  { 
    if (collation == dt.collation)
    {
      // Do nothing
    }
    else 
    {
      if (derivation == DERIVATION_EXPLICIT)
      {
	set(0, DERIVATION_NONE);
	return 1;
      }
      CHARSET_INFO *bin= get_charset_by_csname(collation->csname, 
					       MY_CS_BINSORT,MYF(0));
      set(bin, DERIVATION_NONE);
    }
  }
  return 0;
}

Item_field::Item_field(Field *f) :Item_ident(NullS,f->table_name,f->field_name)
{
  set_field(f);
  collation.set(DERIVATION_IMPLICIT);
  fixed= 1; // This item is not needed in fix_fields
}

// Constructor need to process subselect with temporary tables (see Item)
Item_field::Item_field(THD *thd, Item_field &item):
  Item_ident(thd, item),
  field(item.field),
  result_field(item.result_field)
{ collation.set(DERIVATION_IMPLICIT); }

void Item_field::set_field(Field *field_par)
{
  field=result_field=field_par;			// for easy coding with fields
  maybe_null=field->maybe_null();
  max_length=field_par->field_length;
  decimals= field->decimals();
  table_name=field_par->table_name;
  field_name=field_par->field_name;
  db_name=field_par->table->table_cache_key;
  unsigned_flag=test(field_par->flags & UNSIGNED_FLAG);
  collation.set(field_par->charset(), DERIVATION_IMPLICIT);
}

const char *Item_ident::full_name() const
{
  char *tmp;
  if (!table_name)
    return field_name ? field_name : name ? name : "tmp_field";
  if (db_name && db_name[0])
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
  str->set_charset(str_value.charset());
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
  str->set_charset(str_value.charset());
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

bool Item_field::get_date_result(TIME *ltime,bool fuzzydate)
{
  if ((null_value=result_field->is_null()) ||
      result_field->get_date(ltime,fuzzydate))
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


bool Item_field::eq(const Item *item, bool binary_cmp) const
{
  if (item->type() != FIELD_ITEM)
    return 0;
  
  Item_field *item_field= (Item_field*) item;
  if (item_field->field)
    return item_field->field == field;
  /*
    We may come here when we are trying to find a function in a GROUP BY
    clause from the select list.
    In this case the '100 % correct' way to do this would be to first
    run fix_fields() on the GROUP BY item and then retry this function, but
    I think it's better to relax the checking a bit as we will in
    most cases do the correct thing by just checking the field name.
    (In cases where we would choose wrong we would have to generate a
    ER_NON_UNIQ_ERROR).
  */
  return (!my_strcasecmp(system_charset_info, item_field->name,
			 field_name) &&
	  (!item_field->table_name ||
	   (!my_strcasecmp(table_alias_charset, item_field->table_name,
			   table_name) &&
	    (!item_field->db_name ||
	     (item_field->db_name && !my_strcasecmp(table_alias_charset,
						    item_field->db_name,
						    db_name))))));
}

table_map Item_field::used_tables() const
{
  if (field->table->const_table)
    return 0;					// const item
  return (depended_from ? OUTER_REF_TABLE_BIT : field->table->map);
}

Item *Item_field::get_tmp_table_item(THD *thd)
{
  Item_field *new_item= new Item_field(thd, *this);
  if (new_item)
    new_item->field= new_item->result_field;
  return new_item;
}

String *Item_int::val_str(String *str)
{
  str->set(value, default_charset());
  return str;
}

void Item_int::print(String *str)
{
  if (!name)
  {
    str_value.set(value, default_charset());
    name=str_value.c_ptr();
  }
  str->append(name);
}

String *Item_uint::val_str(String *str)
{
  str->set((ulonglong) value, default_charset());
  return str;
}

void Item_uint::print(String *str)
{
  if (!name)
  {
    str_value.set((ulonglong) value, default_charset());
    name=str_value.c_ptr();
  }
  str->append(name);
}


String *Item_real::val_str(String *str)
{
  str->set(value,decimals,default_charset());
  return str;
}

void Item_string::print(String *str)
{
  str->append('\'');
  str->append(full_name());
  str->append('\'');
}

bool Item_null::eq(const Item *item, bool binary_cmp) const
{ return item->type() == type(); }
double Item_null::val() { null_value=1; return 0.0; }
longlong Item_null::val_int() { null_value=1; return 0; }
/* ARGSUSED */
String *Item_null::val_str(String *str)
{ null_value=1; return 0;}


/* Item_param related */
void Item_param::set_null()
{
  DBUG_ENTER("Item_param::set_null");
  maybe_null= null_value= 1;
  DBUG_VOID_RETURN;
}

void Item_param::set_int(longlong i)
{
  DBUG_ENTER("Item_param::set_int");
  int_value= (longlong)i;
  item_type= INT_ITEM;
  DBUG_PRINT("info", ("integer: %lld", int_value));
  DBUG_VOID_RETURN;
}

void Item_param::set_double(double value)
{
  DBUG_ENTER("Item_param::set_double");
  real_value=value;
  item_type= REAL_ITEM;
  DBUG_PRINT("info", ("double: %lg", real_value));
  DBUG_VOID_RETURN;
}


void Item_param::set_value(const char *str, uint length)
{
  DBUG_ENTER("Item_param::set_value");
  str_value.copy(str,length,default_charset());
  item_type= STRING_ITEM;
  DBUG_PRINT("info", ("string: %s", str_value.ptr()));
  DBUG_VOID_RETURN;
}


void Item_param::set_time(TIME *tm, timestamp_type type)
{ 
  ltime.year= tm->year;
  ltime.month= tm->month;
  ltime.day= tm->day;
  
  ltime.hour= tm->hour;
  ltime.minute= tm->minute;
  ltime.second= tm->second; 

  ltime.second_part= tm->second_part;

  ltime.time_type= type;
  
  item_is_time= true;
  item_type= STRING_ITEM;
}


void Item_param::set_longdata(const char *str, ulong length)
{  
  str_value.append(str,length);
  long_data_supplied= 1;
}


int Item_param::save_in_field(Field *field, bool no_conversions)
{
  THD *thd= current_thd;

  if (thd->command == COM_PREPARE)
    return -1;
  
  if (null_value)
    return (int) set_field_to_null(field);   
    
  field->set_notnull();
  if (item_result_type == INT_RESULT)
  {
    longlong nr=val_int();
    return (field->store(nr)) ? -1 : 0;
  }
  if (item_result_type == REAL_RESULT)
  {
    double nr=val();    
    return (field->store(nr)) ? -1 : 0; 
  }  
  if (item_is_time)
  {
    field->store_time(&ltime, ltime.time_type);
    return 0;
  }
  String *result=val_str(&str_value);
  return (field->store(result->ptr(),result->length(),field->charset())) ? -1 : 0;
}

bool Item_param::get_time(TIME *res)
{
  *res=ltime;
  return 0;
}

double Item_param::val() 
{
  int err;
  switch (item_result_type) {
  case STRING_RESULT:
    return (double) my_strntod(str_value.charset(), (char*) str_value.ptr(),
			       str_value.length(), (char**) 0, &err); 
  case INT_RESULT:
    return (double)int_value;
  default:
    return real_value;
  }
} 


longlong Item_param::val_int() 
{ 
 int err;
 switch (item_result_type) {
  case STRING_RESULT:
    return my_strntoll(str_value.charset(),
		       str_value.ptr(),str_value.length(),10,
		       (char**) 0,&err);
  case REAL_RESULT:
    return (longlong) (real_value+(real_value > 0 ? 0.5 : -0.5));
  default:
    return int_value;
  }
}


String *Item_param::val_str(String* str) 
{ 
  switch (item_result_type) {
  case INT_RESULT:
    str->set(int_value, default_charset());
    return str;
  case REAL_RESULT:
    str->set(real_value, 2, default_charset());
    return str;
  default:
    return (String*) &str_value;
  }
}

/*
  Return Param item values in string format, for generating the dynamic 
  query used in update/binary logs
*/

String *Item_param::query_val_str(String* str) 
{ 
  switch (item_result_type) {
  case INT_RESULT:
  case REAL_RESULT:
    return val_str(str);
    break;
  default:
    str->set("'", 1, default_charset());
    
    if (!item_is_time)
    {
      str->append(str_value);
      const char *from= str->ptr(); 
      uint32 length= 1;
      
      // Escape misc cases
      char *to= (char *)from, *end= (char *)to+str->length(); 
      for (to++; to != end ; length++, to++)
      {
        switch(*to) {
          case '\'':
          case '"':  
          case '\r':
          case '\n':
          case '\\': // TODO: Add remaining ..
            str->replace(length,0,"\\",1); 
            to++; end++; length++;
            break;
          default:     
            break;
        }
      }
    }
    else
    {
      char buff[25];
      
      switch (ltime.time_type)  {
        case TIMESTAMP_NONE:
          break;
        case TIMESTAMP_DATE:
          sprintf(buff, "%04d-%02d-%02d", 
                        ltime.year,ltime.month,ltime.day);
          str->append(buff, 10);
          break;
        case TIMESTAMP_FULL:
          sprintf(buff, "%04d-%02d-%02d %02d:%02d:%02d",
 	                ltime.year,ltime.month,ltime.day,
	                ltime.hour,ltime.minute,ltime.second);
          str->append(buff, 19);
          break;
        case TIMESTAMP_TIME:
        {
          sprintf(buff, "%02d:%02d:%02d",
	  	            ltime.hour,ltime.minute,ltime.second);
          str->append(buff, 8);
          break;
        }
      }
    }
    str->append("'");
  }
  return str;
}
/* End of Item_param related */


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
  Functions to convert item to field (for send_fields)
*/

/* ARGSUSED */
bool Item::fix_fields(THD *thd,
		      struct st_table_list *list,
		      Item ** ref)
{
  fixed= 1;
  return 0;
}

double Item_ref_null_helper::val()
{
  double tmp= (*ref)->val_result();
  owner->was_null|= null_value= (*ref)->null_value;
  return tmp;
}
longlong Item_ref_null_helper::val_int()
{
  longlong tmp= (*ref)->val_int_result();
  owner->was_null|= null_value= (*ref)->null_value;
  return tmp;
}
String* Item_ref_null_helper::val_str(String* s)
{
  String* tmp= (*ref)->str_result(s);
  owner->was_null|= null_value= (*ref)->null_value;
  return tmp;
}
bool Item_ref_null_helper::get_date(TIME *ltime, bool fuzzydate)
{  
  return (owner->was_null|= null_value= (*ref)->get_date(ltime, fuzzydate));
}


/*
  Mark item and SELECT_LEXs as dependent if it is not outer resolving

  SYNOPSIS
    mark_as_dependent()
    thd - thread handler
    last - select from which current item depend
    current  - current select
    item - item which should be marked
*/

static void mark_as_dependent(THD *thd, SELECT_LEX *last, SELECT_LEX *current,
			      Item_ident *item)
{
  // store pointer on SELECT_LEX from wich item is dependent
  item->depended_from= last;
  current->mark_as_dependent(last);
  if (thd->lex.describe)
  {
    char warn_buff[MYSQL_ERRMSG_SIZE];
    sprintf(warn_buff, ER(ER_WARN_FIELD_RESOLVED),
	    (item->db_name?item->db_name:""), (item->db_name?".":""),
	    (item->table_name?item->table_name:""), (item->table_name?".":""),
	    item->field_name,
	    current->select_number, last->select_number);
    push_warning(thd, MYSQL_ERROR::WARN_LEVEL_NOTE,
		 ER_WARN_FIELD_RESOLVED, warn_buff);
  }
}


bool Item_field::fix_fields(THD *thd, TABLE_LIST *tables, Item **ref)
{
  if (!field)					// If field is not checked
  {
    TABLE_LIST *where= 0;
    Field *tmp= (Field *)not_found_field;
    if ((tmp= find_field_in_tables(thd, this, tables, &where, 0)) ==
	not_found_field)
    {
      /*
	We can't find table field in table list of current select, 
	consequently we have to find it in outer subselect(s).
	We can't join lists of outer & current select, because of scope 
	of view rules. For example if both tables (outer & current) have 
	field 'field' it is not mistake to refer to this field without 
	mention of table name, but if we join tables in one list it will
	cause error ER_NON_UNIQ_ERROR in find_field_in_tables.
      */
      SELECT_LEX *last= 0;
#ifdef EMBEDDED_LIBRARY
      thd->net.last_errno= 0;
#endif
      TABLE_LIST *table_list;
      Item **refer= (Item **)not_found_item;
      uint counter;
      // Prevent using outer fields in subselects, that is not supported now
      SELECT_LEX *cursel=(SELECT_LEX *) thd->lex.current_select;
      if (cursel->master_unit()->first_select()->linkage != DERIVED_TABLE_TYPE)
      {
	SELECT_LEX_UNIT *prev_unit= cursel->master_unit();
	for (SELECT_LEX *sl= prev_unit->outer_select();
	     sl;
	     sl= (prev_unit= sl->master_unit())->outer_select())
	{
	  table_list= (last= sl)->get_table_list();
	  if (sl->resolve_mode == SELECT_LEX::INSERT_MODE && table_list)
	  {
	    // it is primary INSERT st_select_lex => skip first table resolving
	    table_list= table_list->next;
	  }
	  if ((tmp= find_field_in_tables(thd, this,
					 table_list, &where,
					 0)) != not_found_field)
	  {
	    prev_unit->item->used_tables_cache|= tmp->table->map;
	    prev_unit->item->const_item_cache= 0;
	    break;
	  }
	  if (sl->resolve_mode == SELECT_LEX::SELECT_MODE &&
	      (refer= find_item_in_list(this, sl->item_list, &counter, 
					 REPORT_EXCEPT_NOT_FOUND)) != 
	       (Item **) not_found_item)
	  {
	    if (*refer && (*refer)->fixed) // Avoid crash in case of error
	    {
	      prev_unit->item->used_tables_cache|= (*refer)->used_tables();
	      prev_unit->item->const_item_cache&= (*refer)->const_item();
	    }
	    break;
	  }

	  // Reference is not found => depend from outer (or just error)
	  prev_unit->item->used_tables_cache|= OUTER_REF_TABLE_BIT;
	  prev_unit->item->const_item_cache= 0;

	  if (sl->master_unit()->first_select()->linkage ==
	      DERIVED_TABLE_TYPE)
	    break; // do not look over derived table
	}
      }
      if (!tmp)
	return -1;
      else if (!refer)
	return 1;
      else if (tmp == not_found_field && refer == (Item **)not_found_item)
      {
	// call to return error code
	find_field_in_tables(thd, this, tables, &where, 1);
	return -1;
      }
      else if (refer != (Item **)not_found_item)
      {
	if (!(*refer)->fixed)
	{
	  my_error(ER_ILLEGAL_REFERENCE, MYF(0), name,
		   "forward reference in item list");
	  return -1;
	}

	Item_ref *rf;
	*ref= rf= new Item_ref(last->ref_pointer_array + counter,
			       (char *)table_name,
			       (char *)field_name);
	if (!rf)
	  return 1;
	if (rf->fix_fields(thd, tables, ref) || rf->check_cols(1))
	  return 1;

	mark_as_dependent(thd, last, cursel, rf);
	return 0;
      }
      else
      {
	mark_as_dependent(thd, last, cursel, this);
	if (last->having_fix_field)
	{
	  Item_ref *rf;
	  *ref= rf= new Item_ref((where->db[0]?where->db:0), 
				 (char *)where->alias,
				 (char *)field_name);
	  if (!rf)
	    return 1;
	  return rf->fix_fields(thd, tables, ref) ||  rf->check_cols(1);
	}
      }
    }
    else if (!tmp)
      return -1;

    set_field(tmp);
  }
  else if (thd && thd->set_query_id && field->query_id != thd->query_id)
  {
    /* We only come here in unions */
    TABLE *table=field->table;
    field->query_id=thd->query_id;
    table->used_fields++;
    table->used_keys&=field->part_of_key;
  }
  fixed= 1;
  return 0;
}


void Item::init_make_field(Send_field *tmp_field,
			   enum enum_field_types field_type)
{  
  char *empty_name= (char*) "";
  tmp_field->db_name=	 	empty_name;
  tmp_field->org_table_name=	empty_name;
  tmp_field->org_col_name=	empty_name;
  tmp_field->table_name=	empty_name;
  tmp_field->col_name=		name;
  tmp_field->charsetnr= 	collation.collation->number;
  tmp_field->flags=maybe_null ? 0 : NOT_NULL_FLAG;
  tmp_field->type=field_type;
  tmp_field->length=max_length;
  tmp_field->decimals=decimals;
  if (unsigned_flag)
    tmp_field->flags |= UNSIGNED_FLAG;
}

void Item::make_field(Send_field *tmp_field)
{
  init_make_field(tmp_field, field_type());
}


void Item_empty_string::make_field(Send_field *tmp_field)
{
  init_make_field(tmp_field,FIELD_TYPE_VAR_STRING);
}


enum_field_types Item::field_type() const
{
  return ((result_type() == STRING_RESULT) ? FIELD_TYPE_VAR_STRING :
	  (result_type() == INT_RESULT) ? FIELD_TYPE_LONGLONG :
	  FIELD_TYPE_DOUBLE);
}

Field *Item::tmp_table_field_from_field_type(TABLE *table)
{
  switch (field_type()) 
  {
  case MYSQL_TYPE_DECIMAL:
    return new Field_decimal(max_length, maybe_null, name, table,
			     unsigned_flag);
  case MYSQL_TYPE_TINY:
    return new Field_tiny(max_length, maybe_null, name, table,
			  unsigned_flag);
  case MYSQL_TYPE_SHORT:
    return new Field_short(max_length, maybe_null, name, table,
			   unsigned_flag);
  case MYSQL_TYPE_LONG:
    return new Field_long(max_length, maybe_null, name, table,
			  unsigned_flag);
  case MYSQL_TYPE_FLOAT:
    return new Field_float(max_length, maybe_null, name, table, decimals);
  case MYSQL_TYPE_DOUBLE:
    return new Field_double(max_length, maybe_null, name, table, decimals);
  case MYSQL_TYPE_NULL:
    return new Field_null(max_length, name, table, &my_charset_bin);
#ifdef HAVE_LONG_LONG
  case MYSQL_TYPE_LONGLONG:
    return new Field_longlong(max_length, maybe_null, name, table,
			      unsigned_flag);
#endif
  case MYSQL_TYPE_NEWDATE:
  case MYSQL_TYPE_INT24:
    return new Field_long(max_length, maybe_null, name, table,
			  unsigned_flag);
  case MYSQL_TYPE_DATE:
    return new Field_date(maybe_null, name, table, &my_charset_bin);
  case MYSQL_TYPE_TIME:
    return new Field_time(maybe_null, name, table, &my_charset_bin);
  case MYSQL_TYPE_TIMESTAMP:
  case MYSQL_TYPE_DATETIME:
    return new Field_datetime(maybe_null, name, table, &my_charset_bin);
  case MYSQL_TYPE_YEAR:
    return new Field_year(max_length, maybe_null, name, table);
  case MYSQL_TYPE_ENUM:
  case MYSQL_TYPE_SET:
    return new Field_long(max_length, maybe_null, name, table,
			  unsigned_flag);
  case MYSQL_TYPE_TINY_BLOB:
  case MYSQL_TYPE_MEDIUM_BLOB:
  case MYSQL_TYPE_LONG_BLOB:
  case MYSQL_TYPE_BLOB:
  case MYSQL_TYPE_GEOMETRY:
    return new Field_blob(max_length, maybe_null, name, table, collation.collation);
  case MYSQL_TYPE_VAR_STRING:
    if (max_length > 255)
      return new Field_blob(max_length, maybe_null, name, table, collation.collation);
    else
      return new Field_varstring(max_length, maybe_null, name, table, collation.collation);
  case MYSQL_TYPE_STRING:
    if (max_length > 255)
      return new Field_blob(max_length, maybe_null, name, table, collation.collation);
    else
      return new Field_string(max_length, maybe_null, name, table, collation.collation);
  default:
    // This case should never be choosen
    DBUG_ASSERT(0);
    return 0;
  }
}

/* ARGSUSED */
void Item_field::make_field(Send_field *tmp_field)
{
  field->make_field(tmp_field);
  if (name)
    tmp_field->col_name=name;			// Use user supplied name
}

/*
** Set a field:s value from a item
*/


void Item_field::save_org_in_field(Field *to)
{
  if (field->is_null())
  {
    null_value=1;
    set_field_to_null_with_conversions(to, 1);
  }
  else
  {
    to->set_notnull();
    field_conv(to,field);
    null_value=0;
  }
}

int Item_field::save_in_field(Field *to, bool no_conversions)
{
  if (result_field->is_null())
  {
    null_value=1;
    return set_field_to_null_with_conversions(to, no_conversions);
  }
  else
  {
    to->set_notnull();
    field_conv(to,result_field);
    null_value=0;
  }
  return 0;
}


/*
  Store null in field

  SYNOPSIS
    save_in_field()
    field		Field where we want to store NULL

  DESCRIPTION
    This is used on INSERT.
    Allow NULL to be inserted in timestamp and auto_increment values

  RETURN VALUES
    0	 ok
    1	 Field doesn't support NULL values and can't handle 'field = NULL'
*/   

int Item_null::save_in_field(Field *field, bool no_conversions)
{
  return set_field_to_null_with_conversions(field, no_conversions);
}


/*
  Store null in field

  SYNOPSIS
    save_safe_in_field()
    field		Field where we want to store NULL

  RETURN VALUES
    0	 ok
    1	 Field doesn't support NULL values
*/   

int Item_null::save_safe_in_field(Field *field)
{
  return set_field_to_null(field);
}


int Item::save_in_field(Field *field, bool no_conversions)
{
  int error;
  if (result_type() == STRING_RESULT ||
      result_type() == REAL_RESULT &&
      field->result_type() == STRING_RESULT)
  {
    String *result;
    CHARSET_INFO *cs= collation.collation;
    char buff[MAX_FIELD_WIDTH];		// Alloc buffer for small columns
    str_value.set_quick(buff,sizeof(buff),cs);
    result=val_str(&str_value);
    if (null_value)
      return set_field_to_null_with_conversions(field, no_conversions);
    field->set_notnull();
    error=field->store(result->ptr(),result->length(),cs);
    str_value.set_quick(0, 0, cs);
  }
  else if (result_type() == REAL_RESULT)
  {
    double nr=val();
    if (null_value)
      return set_field_to_null(field);
    field->set_notnull();
    error=field->store(nr);
  }
  else
  {
    longlong nr=val_int();
    if (null_value)
      return set_field_to_null_with_conversions(field, no_conversions);
    field->set_notnull();
    error=field->store(nr);
  }
  return (error) ? -1 : 0;
}


int Item_string::save_in_field(Field *field, bool no_conversions)
{
  String *result;
  result=val_str(&str_value);
  if (null_value)
    return set_field_to_null(field);
  field->set_notnull();
  return (field->store(result->ptr(),result->length(),collation.collation)) ? 
	  -1 : 0;
}


int Item_int::save_in_field(Field *field, bool no_conversions)
{
  longlong nr=val_int();
  if (null_value)
    return set_field_to_null(field);
  field->set_notnull();
  return (field->store(nr)) ? -1 : 0;
}


int Item_real::save_in_field(Field *field, bool no_conversions)
{
  double nr=val();
  if (null_value)
    return set_field_to_null(field);
  field->set_notnull();
  return (field->store(nr)) ? -1 : 0;
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
  str_value.set(ptr,max_length,&my_charset_bin);
  char *end=ptr+max_length;
  if (max_length*2 != str_length)
    *ptr++=char_val(*str++);			// Not even, assume 0 prefix
  while (ptr != end)
  {
    *ptr++= (char) (char_val(str[0])*16+char_val(str[1]));
    str+=2;
  }
  *ptr=0;					// Keep purify happy
  collation.set(&my_charset_bin, DERIVATION_COERCIBLE);
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


int Item_varbinary::save_in_field(Field *field, bool no_conversions)
{
  int error;
  field->set_notnull();
  if (field->result_type() == STRING_RESULT)
  {
    error=field->store(str_value.ptr(),str_value.length(),collation.collation);
  }
  else
  {
    longlong nr=val_int();
    error=field->store(nr);
  }
  return (error) ? -1 :  0;
}


/*
  Pack data in buffer for sending
*/

bool Item_null::send(Protocol *protocol, String *packet)
{
  return protocol->store_null();
}

/*
  This is only called from items that is not of type item_field
*/

bool Item::send(Protocol *protocol, String *buffer)
{
  bool result;
  enum_field_types type;
  LINT_INIT(result);

  switch ((type=field_type())) {
  default:
  case MYSQL_TYPE_NULL:
  case MYSQL_TYPE_DECIMAL:
  case MYSQL_TYPE_ENUM:
  case MYSQL_TYPE_SET:
  case MYSQL_TYPE_TINY_BLOB:
  case MYSQL_TYPE_MEDIUM_BLOB:
  case MYSQL_TYPE_LONG_BLOB:
  case MYSQL_TYPE_BLOB:
  case MYSQL_TYPE_GEOMETRY:
  case MYSQL_TYPE_STRING:
  case MYSQL_TYPE_VAR_STRING:
  {
    String *res;
    if ((res=val_str(buffer)))
      result= protocol->store(res->ptr(),res->length(),res->charset());
    break;
  }
  case MYSQL_TYPE_TINY:
  {
    longlong nr;
    nr= val_int();
    if (!null_value)
      result= protocol->store_tiny(nr);
    break;
  }
  case MYSQL_TYPE_SHORT:
  {
    longlong nr;
    nr= val_int();
    if (!null_value)
      result= protocol->store_short(nr);
    break;
  }
  case MYSQL_TYPE_INT24:
  case MYSQL_TYPE_LONG:
  {
    longlong nr;
    nr= val_int();
    if (!null_value)
      result= protocol->store_long(nr);
    break;
  }
  case MYSQL_TYPE_LONGLONG:
  {
    longlong nr;
    nr= val_int();
    if (!null_value)
      result= protocol->store_longlong(nr, unsigned_flag);
    break;
  }
  case MYSQL_TYPE_DOUBLE:
  {
    double nr;
    nr= val();
    if (!null_value)
      result= protocol->store(nr, decimals, buffer);
    break;
  }
  case MYSQL_TYPE_DATETIME:
  case MYSQL_TYPE_DATE:
  case MYSQL_TYPE_TIMESTAMP:
  {
    TIME tm;
    get_date(&tm, 1);
    if (!null_value)
    {
      if (type == MYSQL_TYPE_DATE)
	return protocol->store_date(&tm);
      else
	result= protocol->store(&tm);
    }
    break;
  }
  case MYSQL_TYPE_TIME:
  {
    TIME tm;
    get_time(&tm);
    if (!null_value)
      result= protocol->store_time(&tm);
    break;
  }
  }
  if (null_value)
    result= protocol->store_null();
  return result;
}


bool Item_field::send(Protocol *protocol, String *buffer)
{
  return protocol->store(result_field);
}

/*
  This is used for HAVING clause
  Find field in select list having the same name
 */

bool Item_ref::fix_fields(THD *thd,TABLE_LIST *tables, Item **reference)
{
  uint counter;
  if (!ref)
  {
    TABLE_LIST *where= 0, *table_list;
    SELECT_LEX_UNIT *prev_unit= thd->lex.current_select->master_unit();
    SELECT_LEX *sl= prev_unit->outer_select();
    /*
      Finding only in current select will be performed for selects that have 
      not outer one and for derived tables (which not support using outer 
      fields for now)
    */
    if ((ref= find_item_in_list(this, 
				*(thd->lex.current_select->get_item_list()),
				&counter,
				((sl && 
				  thd->lex.current_select->master_unit()->
				  first_select()->linkage !=
				  DERIVED_TABLE_TYPE) ? 
				  REPORT_EXCEPT_NOT_FOUND :
				  REPORT_ALL_ERRORS))) ==
	(Item **)not_found_item)
    {
      Field *tmp= (Field*) not_found_field;
      /*
	We can't find table field in table list of current select,
	consequently we have to find it in outer subselect(s).
	We can't join lists of outer & current select, because of scope
	of view rules. For example if both tables (outer & current) have
	field 'field' it is not mistake to refer to this field without
	mention of table name, but if we join tables in one list it will
	cause error ER_NON_UNIQ_ERROR in find_item_in_list.
      */
      SELECT_LEX *last=0;
      for ( ; sl ; sl= (prev_unit= sl->master_unit())->outer_select())
      {
	last= sl;
	if (sl->resolve_mode == SELECT_LEX::SELECT_MODE &&
	    (ref= find_item_in_list(this, sl->item_list,
				    &counter,
				    REPORT_EXCEPT_NOT_FOUND)) !=
	   (Item **)not_found_item)
	{
	  if (*ref && (*ref)->fixed) // Avoid crash in case of error
	  {
	    prev_unit->item->used_tables_cache|= (*ref)->used_tables();
	    prev_unit->item->const_item_cache&= (*ref)->const_item();
	  }
	  break;
	}
	table_list= sl->get_table_list();
	if (sl->resolve_mode == SELECT_LEX::INSERT_MODE && table_list)
	{
	  // it is primary INSERT st_select_lex => skip first table resolving
	  table_list= table_list->next;
	}
	if ((tmp= find_field_in_tables(thd, this,
				       table_list, &where,
				       0)) != not_found_field)
	{
	  prev_unit->item->used_tables_cache|= tmp->table->map;
	  prev_unit->item->const_item_cache= 0;
	  break;
	}

	// Reference is not found => depend from outer (or just error)
	prev_unit->item->used_tables_cache|= OUTER_REF_TABLE_BIT;
	prev_unit->item->const_item_cache= 0;

	if (sl->master_unit()->first_select()->linkage ==
	    DERIVED_TABLE_TYPE)
	  break; // do not look over derived table
      }

      if (!ref)
	return 1;
      else if (!tmp)
	return -1;
      else if (ref == (Item **)not_found_item && tmp == not_found_field)
      {
	// Call to report error
	find_item_in_list(this,
			  *(thd->lex.current_select->get_item_list()),
			  &counter,
			  REPORT_ALL_ERRORS);
        ref= 0;
	return 1;
      }
      else if (tmp != not_found_field)
      {
	ref= 0; // To prevent "delete *ref;" on ~Item_erf() of this item
	Item_field* fld;
	if (!((*reference)= fld= new Item_field(tmp)))
	  return 1;
	mark_as_dependent(thd, last, thd->lex.current_select, fld);
	return 0;
      }
      else
      {
	if (!(*ref)->fixed)
	{
	  my_error(ER_ILLEGAL_REFERENCE, MYF(0), name,
		   "forward reference in item list");
	  return -1;
	}
	mark_as_dependent(thd, last, thd->lex.current_select,
			  this);
	ref= last->ref_pointer_array + counter;
      }
    }
    else if (!ref)
      return 1;
    else
    {
      if (!(*ref)->fixed)
      {
	my_error(ER_ILLEGAL_REFERENCE, MYF(0), name,
		 "forward reference in item list");
	return -1;
      }
      ref= thd->lex.current_select->ref_pointer_array + counter;
    }
  }

  /*
    The following conditional is changed as to correctly identify 
    incorrect references in group functions or forward references 
    with sub-select's / derived tables, while it prevents this 
    check when Item_ref is created in an expression involving 
    summing function, which is to be placed in the user variable.
  */
  if (((*ref)->with_sum_func && name &&
       (depended_from ||
	!(thd->lex.current_select->linkage != GLOBAL_OPTIONS_TYPE &&
	  thd->lex.current_select->having_fix_field))) ||
      !(*ref)->fixed)
  {
    my_error(ER_ILLEGAL_REFERENCE, MYF(0), name, 
	     ((*ref)->with_sum_func?
	      "reference on group function":
	      "forward reference in item list"));
    return 1;
  }
  max_length= (*ref)->max_length;
  maybe_null= (*ref)->maybe_null;
  decimals=   (*ref)->decimals;
  collation.set((*ref)->collation);
  with_sum_func= (*ref)->with_sum_func;
  fixed= 1;

  if (ref && (*ref)->check_cols(1))
    return 1;
  return 0;
}


bool Item_default_value::eq(const Item *item, bool binary_cmp) const
{
  return item->type() == DEFAULT_VALUE_ITEM && 
    ((Item_default_value *)item)->arg->eq(arg, binary_cmp);
}


bool Item_default_value::fix_fields(THD *thd, struct st_table_list *table_list, Item **items)
{
  if (!arg)
    return false;
  bool res= arg->fix_fields(thd, table_list, items);
  if (res)
    return res;
  /* arg->type() can be only REF_ITEM or FIELD_ITEM for it defined as
   simple_ident in sql_yacc.yy
  */
  if (arg->type() == REF_ITEM)
  {
    Item_ref *ref= (Item_ref *)arg;
    if (ref->ref[0]->type() != FIELD_ITEM)
    {
      return 1;
    }
    arg= ref->ref[0];
  }
  Item_field *field_arg= (Item_field *)arg;
  Field *def_field= (Field*) sql_alloc(field_arg->field->size_of());
  if (!def_field)
    return 1;
  memcpy(def_field, field_arg->field, field_arg->field->size_of());
  def_field->move_field(def_field->table->default_values -
                        def_field->table->record[0]);
  set_field(def_field);
  return 0;
}

void Item_default_value::print(String *str)
{
  if (!arg)
  {
    str->append("DEFAULT");
    return;
  }
  str->append("DEFAULT(");
  arg->print(str);
  str->append(')');
}

bool Item_insert_value::eq(const Item *item, bool binary_cmp) const
{
  return item->type() == INSERT_VALUE_ITEM &&
    ((Item_default_value *)item)->arg->eq(arg, binary_cmp);
}


bool Item_insert_value::fix_fields(THD *thd, struct st_table_list *table_list, Item **items)
{
  bool res= arg->fix_fields(thd, table_list, items);
  if (res)
    return res;
  /*
    arg->type() can be only REF_ITEM or FIELD_ITEM as arg is
    a simple_ident in sql_yacc.yy
  */
  if (arg->type() == REF_ITEM)
  {
    Item_ref *ref= (Item_ref *)arg;
    if (ref->ref[0]->type() != FIELD_ITEM)
    {
      return 1;
    }
    arg= ref->ref[0];
  }
  Item_field *field_arg= (Item_field *)arg;
  if (field_arg->field->table->insert_values)
  {
    Field *def_field= (Field*) sql_alloc(field_arg->field->size_of());
    if (!def_field)
      return 1;
    memcpy(def_field, field_arg->field, field_arg->field->size_of());
    def_field->move_field(def_field->table->insert_values -
                          def_field->table->record[0]);
    set_field(def_field);
  }
  else
  {
    Field *field=field_arg->field;
    /* charset doesn't matter here, it's to avoid sigsegv only */
    set_field(new Field_null(0,0,Field::NONE,field->field_name,field->table,
          &my_charset_bin));
  }
  return 0;
}

void Item_insert_value::print(String *str)
{
  str->append("VALUE(");
  arg->print(str);
  str->append(')');
}

/*
  If item is a const function, calculate it and return a const item
  The original item is freed if not returned
*/

Item_result item_cmp_type(Item_result a,Item_result b)
{
  if (a == STRING_RESULT && b == STRING_RESULT)
    return STRING_RESULT;
  else if (a == INT_RESULT && b == INT_RESULT)
    return INT_RESULT;
  else if (a == ROW_RESULT || b == ROW_RESULT)
    return ROW_RESULT;
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
    String tmp(buff,sizeof(buff),&my_charset_bin),*result;
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
    return new Item_string(name,tmp_str,length,result->charset());
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
    String item_tmp(item_buff,sizeof(item_buff),&my_charset_bin),*item_result;
    String field_tmp(field_buff,sizeof(field_buff),&my_charset_bin);
    item_result=item->val_str(&item_tmp);
    if (item->null_value)
      return 1;					// This must be true
    field->val_str(&field_tmp,&field_tmp);
    return !sortcmp(&field_tmp,item_result,&my_charset_bin);
  }
  if (res_type == INT_RESULT)
    return 1;					// Both where of type int
  double result=item->val();
  if (item->null_value)
    return 1;
  return result == field->val_real();
}

Item_cache* Item_cache::get_cache(Item_result type)
{
  switch (type)
  {
  case INT_RESULT:
    return new Item_cache_int();
  case REAL_RESULT:
    return new Item_cache_real();
  case STRING_RESULT:
    return new Item_cache_str();
  case ROW_RESULT:
    return new Item_cache_row();
  default:
    // should never be in real life
    DBUG_ASSERT(0);
    return 0;
  }
}

void Item_cache_str::store(Item *item)
{
  value_buff.set(buffer, sizeof(buffer), item->collation.collation);
  value= item->str_result(&value_buff);
  if ((null_value= item->null_value))
    value= 0;
  else if (value != &value_buff)
  {
    /*
      We copy string value to avoid changing value if 'item' is table field
      in queries like following (where t1.c is varchar):
      select a, 
             (select a,b,c from t1 where t1.a=t2.a) = ROW(a,2,'a'),
             (select c from t1 where a=t2.a)
        from t2;
    */
    value_buff.copy(*value);
    value= &value_buff;
  }
  collation.set(item->collation);
}
double Item_cache_str::val()
{ 
  int err;
  if (value)
    return my_strntod(value->charset(), (char*) value->ptr(),
		      value->length(), (char**) 0, &err);
  else
    return (double)0;
}
longlong Item_cache_str::val_int()
{
  int err;
  if (value)
    return my_strntoll(value->charset(), value->ptr(),
		       value->length(), 10, (char**) 0, &err);
  else
    return (longlong)0;
}

bool Item_cache_row::allocate(uint num)
{
  item_count= num;
  THD *thd= current_thd;
  return (!(values= 
	    (Item_cache **) thd->calloc(sizeof(Item_cache *)*item_count)));
}

bool Item_cache_row::setup(Item * item)
{
  if (!values && allocate(item->cols()))
    return 1;
  for (uint i= 0; i < item_count; i++)
  {
    Item *el= item->el(i);
    Item_cache *tmp;
    if (!(tmp= values[i]= Item_cache::get_cache(el->result_type())))
      return 1;
    tmp->setup(el);
  }
  return 0;
}

void Item_cache_row::store(Item * item)
{
  null_value= 0;
  item->bring_value();
  for (uint i= 0; i < item_count; i++)
  {
    values[i]->store(item->el(i));
    null_value|= values[i]->null_value;
  }
}

void Item_cache_row::illegal_method_call(const char *method)
{
  DBUG_ENTER("Item_cache_row::illegal_method_call");
  DBUG_PRINT("error", ("!!! %s method was called for row item", method));
  DBUG_ASSERT(0);
  my_error(ER_OPERAND_COLUMNS, MYF(0), 1);
  DBUG_VOID_RETURN;
}

bool Item_cache_row::check_cols(uint c)
{
  if (c != item_count)
  {
    my_error(ER_OPERAND_COLUMNS, MYF(0), c);
    return 1;
  }
  return 0;
}

bool Item_cache_row::null_inside()
{
  for (uint i= 0; i < item_count; i++)
  {
    if (values[i]->cols() > 1)
    {
      if (values[i]->null_inside())
	return 1;
    }
    else
    {
      values[i]->val_int();
      if (values[i]->null_value)
	return 1;
    }
  }
  return 0;
}

void Item_cache_row::bring_value()
{
  for (uint i= 0; i < item_count; i++)
    values[i]->bring_value();
  return;
}

/*****************************************************************************
** Instantiate templates
*****************************************************************************/

#ifdef __GNUC__
template class List<Item>;
template class List_iterator<Item>;
template class List_iterator_fast<Item>;
template class List<List_item>;
#endif
