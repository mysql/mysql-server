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


/* When using sql procedures */

#ifdef __GNUC__
#pragma interface				/* gcc class implementation */
#endif

#define PROC_NO_SORT 1				/* Bits in flags */
#define PROC_GROUP   2				/* proc must have group */

/* Procedure items used by procedures to store values for send_fields */

class Item_proc :public Item
{
public:
  Item_proc(const char *name_par): Item()
  {
     this->name=(char*) name_par;
  }
  enum Type type() const { return Item::PROC_ITEM; }
  virtual void set(double nr)=0;
  virtual void set(const char *str,uint length)=0;
  virtual void set(longlong nr)=0;
  virtual enum_field_types field_type() const=0;
  void set(const char *str) { set(str,strlen(str)); }
  void make_field(Send_field *tmp_field)
  {
    init_make_field(tmp_field,field_type());
  }
};

class Item_proc_real :public Item_proc
{
  double value;
public:
  Item_proc_real(const char *name_par,uint dec) : Item_proc(name_par)
  {
     decimals=dec; max_length=float_length(dec);
  }
  enum Item_result result_type () const { return REAL_RESULT; }
  enum_field_types field_type() const { return FIELD_TYPE_DOUBLE; }
  void set(double nr) { value=nr; }
  void set(longlong nr) { value=(double) nr; }
  void set(const char *str,uint length __attribute__((unused)))
  { value=atof(str); }
  double val() { return value; }
  longlong val_int() { return (longlong) value; }
  String *val_str(String *s) { s->set(value,decimals); return s; }
};

class Item_proc_int :public Item_proc
{
  longlong value;
public:
  Item_proc_int(const char *name_par) :Item_proc(name_par)
  { max_length=11; }
  enum Item_result result_type () const { return INT_RESULT; }
  enum_field_types field_type() const { return FIELD_TYPE_LONG; }
  void set(double nr) { value=(longlong) nr; }
  void set(longlong nr) { value=nr; }
  void set(const char *str,uint length __attribute__((unused)))
  { value=strtoll(str,NULL,10); }
  double val() { return (double) value; }
  longlong val_int() { return value; }
  String *val_str(String *s) { s->set(value); return s; }
};


class Item_proc_string :public Item_proc
{
public:
  Item_proc_string(const char *name_par,uint length) :Item_proc(name_par)
    { this->max_length=length; }
  enum Item_result result_type () const { return STRING_RESULT; }
  enum_field_types field_type() const { return FIELD_TYPE_STRING; }
  void set(double nr) { str_value.set(nr); }
  void set(longlong nr) { str_value.set(nr); }
  void set(const char *str, uint length) { str_value.copy(str,length); }
  double val() { return atof(str_value.ptr()); }
  longlong val_int() { return strtoll(str_value.ptr(),NULL,10); }
  String *val_str(String*)
  {
    return null_value ? (String*) 0 : (String*) &str_value;
  }
};

/* The procedure class definitions */

class Procedure {
protected:
  List<Item> *fields;
  select_result *result;
public:
  const uint flags;
  ORDER *group,*param_fields;
  Procedure(select_result *res,uint flags_par) :result(res),flags(flags_par),
    group(0),param_fields(0) {}
  virtual ~Procedure() {group=param_fields=0; fields=0; }
  virtual void add(void)=0;
  virtual void end_group(void)=0;
  virtual int send_row(List<Item> &fields)=0;
  virtual bool change_columns(List<Item> &fields)=0;
  virtual void update_refs(void) {}
  virtual bool end_of_records() { return 0; }
};

Procedure *setup_procedure(THD *thd,ORDER *proc_param,select_result *result,
			   List<Item> &field_list,int *error);
