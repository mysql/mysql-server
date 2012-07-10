#ifndef PROCEDURE_INCLUDED
#define PROCEDURE_INCLUDED

/* Copyright (c) 2000, 2011, Oracle and/or its affiliates. All rights reserved.

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


/* When using sql procedures */

/*
  It is necessary to include set_var.h instead of item.h because there
  are dependencies on include order for set_var.h and item.h. This
  will be resolved later.
*/
#include "sql_class.h"                          /* select_result, set_var.h: THD */
#include "set_var.h"                            /* Item */

/* Procedure items used by procedures to store values for send_result_set_metadata */

class Item_proc :public Item
{
public:
  Item_proc(const char *name_par): Item()
  {
     this->item_name.set(name_par);
  }
  enum Type type() const { return Item::PROC_ITEM; }
  virtual void set(const char *str,uint length, const CHARSET_INFO *cs)=0;
  virtual void set(longlong nr)=0;
  virtual enum_field_types field_type() const=0;
  void set(const char *str) { set(str,(uint) strlen(str), default_charset()); }
  unsigned int size_of() { return sizeof(*this);}  
};


class Item_proc_int :public Item_proc
{
  longlong value;
public:
  Item_proc_int(const char *name_par) :Item_proc(name_par)
  { max_length=11; }
  enum Item_result result_type () const { return INT_RESULT; }
  enum_field_types field_type() const { return MYSQL_TYPE_LONGLONG; }
  void set(longlong nr) { value=nr; }
  void set(const char *str,uint length, const CHARSET_INFO *cs)
  { int err; value=my_strntoll(cs,str,length,10,NULL,&err); }
  double val_real() { return (double) value; }
  longlong val_int() { return value; }
  String *val_str(String *s) { s->set(value, default_charset()); return s; }
  my_decimal *val_decimal(my_decimal *);
  bool get_date(MYSQL_TIME *ltime, uint fuzzydate)
  {
    return get_date_from_int(ltime, fuzzydate);
  }
  bool get_time(MYSQL_TIME *ltime)
  {
    return get_time_from_int(ltime);
  }
  unsigned int size_of() { return sizeof(*this);}
};


class Item_proc_string :public Item_proc
{
public:
  Item_proc_string(const char *name_par,uint length) :Item_proc(name_par)
    { this->max_length=length; }
  enum Item_result result_type () const { return STRING_RESULT; }
  enum_field_types field_type() const { return MYSQL_TYPE_VARCHAR; }
  void set(longlong nr) { str_value.set(nr, default_charset()); }
  void set(const char *str, uint length, const CHARSET_INFO *cs)
  { str_value.copy(str,length,cs); }
  double val_real()
  {
    int err_not_used;
    char *end_not_used;
    const CHARSET_INFO *cs= str_value.charset();
    return my_strntod(cs, (char*) str_value.ptr(), str_value.length(),
		      &end_not_used, &err_not_used);
  }
  longlong val_int()
  { 
    int err;
    const CHARSET_INFO *cs=str_value.charset();
    return my_strntoll(cs,str_value.ptr(),str_value.length(),10,NULL,&err);
  }
  bool get_date(MYSQL_TIME *ltime, uint fuzzydate)
  {
    return get_date_from_string(ltime, fuzzydate);
  }
  bool get_time(MYSQL_TIME *ltime)
  {
    return get_time_from_string(ltime);
  }
  String *val_str(String*)
  {
    return null_value ? (String*) 0 : (String*) &str_value;
  }
  my_decimal *val_decimal(my_decimal *);
  unsigned int size_of() { return sizeof(*this);}  
};

/* The procedure class definitions */

#endif /* PROCEDURE_INCLUDED */
