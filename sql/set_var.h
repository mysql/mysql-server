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

/* Classes to support the SET command */

#ifdef __GNUC__
#pragma interface			/* gcc class implementation */
#endif

/****************************************************************************
  Variables that are changable runtime are declared using the
  following classes
****************************************************************************/

class sys_var;
class set_var;
typedef struct system_variables SV;
extern TYPELIB bool_typelib, delay_key_write_typelib;

enum enum_var_type
{
  OPT_DEFAULT, OPT_SESSION, OPT_GLOBAL
};

typedef bool (*sys_check_func)(THD *,  set_var *);
typedef bool (*sys_update_func)(THD *, set_var *);
typedef void (*sys_after_update_func)(THD *,enum_var_type);
typedef void (*sys_set_default_func)(THD *, enum_var_type);

class sys_var
{
public:
  struct my_option *option_limits;	/* Updated by by set_var_init() */
  uint name_length;			/* Updated by by set_var_init() */
  const char *name;
  sys_after_update_func after_update;
  sys_var(const char *name_arg) :name(name_arg),after_update(0)
  {}
  sys_var(const char *name_arg,sys_after_update_func func)
    :name(name_arg),after_update(func)
  {}
  virtual ~sys_var() {}
  virtual bool check(THD *thd, set_var *var) { return 0; }
  bool check_enum(THD *thd, set_var *var, TYPELIB *enum_names);
  virtual bool update(THD *thd, set_var *var)=0;
  virtual void set_default(THD *thd, enum_var_type type) {}
  virtual SHOW_TYPE type() { return SHOW_UNDEF; }
  virtual byte *value_ptr(THD *thd, enum_var_type type) { return 0; }
  virtual bool check_type(enum_var_type type)
  { return type != OPT_GLOBAL; }		/* Error if not GLOBAL */
  virtual bool check_update_type(Item_result type)
  { return type != INT_RESULT; }		/* Assume INT */
  virtual bool check_default(enum_var_type type)
  { return option_limits == 0; }
  Item *item(THD *thd, enum_var_type type);
};


class sys_var_long_ptr :public sys_var
{
public:
  ulong *value;
  sys_var_long_ptr(const char *name_arg, ulong *value_ptr)
    :sys_var(name_arg),value(value_ptr) {}
  sys_var_long_ptr(const char *name_arg, ulong *value_ptr,
		   sys_after_update_func func)
    :sys_var(name_arg,func), value(value_ptr) {}
  bool update(THD *thd, set_var *var);
  void set_default(THD *thd, enum_var_type type);
  SHOW_TYPE type() { return SHOW_LONG; }
  byte *value_ptr(THD *thd, enum_var_type type) { return (byte*) value; }
};


class sys_var_ulonglong_ptr :public sys_var
{
public:
  ulonglong *value;
  sys_var_ulonglong_ptr(const char *name_arg, ulonglong *value_ptr)
    :sys_var(name_arg),value(value_ptr) {}
  sys_var_ulonglong_ptr(const char *name_arg, ulonglong *value_ptr,
		       sys_after_update_func func)
    :sys_var(name_arg,func), value(value_ptr) {}
  bool update(THD *thd, set_var *var);
  void set_default(THD *thd, enum_var_type type);
  SHOW_TYPE type() { return SHOW_LONGLONG; }
  byte *value_ptr(THD *thd, enum_var_type type) { return (byte*) value; }
};


class sys_var_bool_ptr :public sys_var
{
public:
  my_bool *value;
  sys_var_bool_ptr(const char *name_arg, my_bool *value_arg)
    :sys_var(name_arg),value(value_arg)
  {}
  bool check(THD *thd, set_var *var)
  {
    return check_enum(thd, var, &bool_typelib);
  }
  bool update(THD *thd, set_var *var);
  void set_default(THD *thd, enum_var_type type);
  SHOW_TYPE type() { return SHOW_MY_BOOL; }
  byte *value_ptr(THD *thd, enum_var_type type) { return (byte*) value; }
  bool check_update_type(Item_result type) { return 0; }
};


class sys_var_str :public sys_var
{
public:
  char *value;					// Pointer to allocated string
  sys_check_func check_func;
  sys_update_func update_func;
  sys_set_default_func set_default_func;
  sys_var_str(const char *name_arg,
	      sys_check_func check_func_arg,
	      sys_update_func update_func_arg,
	      sys_set_default_func set_default_func_arg)
    :sys_var(name_arg), check_func(check_func_arg),
    update_func(update_func_arg),set_default_func(set_default_func_arg)
  {}
  bool check(THD *thd, set_var *var)
  {
    return check_func ? (*check_func)(thd, var) : 0;
  }
  bool update(THD *thd, set_var *var)
  {
    return (*update_func)(thd, var);
  }
  void set_default(THD *thd, enum_var_type type)
  {
    (*set_default_func)(thd, type);
  }
  SHOW_TYPE type() { return SHOW_CHAR; }
  byte *value_ptr(THD *thd, enum_var_type type) { return (byte*) value; }
  bool check_update_type(Item_result type)
  {
    return type != STRING_RESULT;		/* Only accept strings */
  }
  bool check_default(enum_var_type type) { return 0; }
};


class sys_var_enum :public sys_var
{
  uint	*value; 
  TYPELIB *enum_names;
public:
  sys_var_enum(const char *name_arg, uint *value_arg,
	       TYPELIB *typelib, sys_after_update_func func)
    :sys_var(name_arg,func), value(value_arg), enum_names(typelib)
  {}
  bool check(THD *thd, set_var *var)
  {
    return check_enum(thd, var, enum_names);
  }
  bool update(THD *thd, set_var *var);
  SHOW_TYPE type() { return SHOW_CHAR; }
  byte *value_ptr(THD *thd, enum_var_type type);
  bool check_update_type(Item_result type) { return 0; }
};


class sys_var_thd :public sys_var
{
public:
  sys_var_thd(const char *name_arg)
    :sys_var(name_arg)
  {}
  sys_var_thd(const char *name_arg, sys_after_update_func func)
    :sys_var(name_arg,func)
  {}
  bool check_type(enum_var_type type) { return 0; }
  bool check_default(enum_var_type type)
  {
    return type == OPT_GLOBAL && !option_limits;
  }
};


class sys_var_thd_ulong :public sys_var_thd
{
public:
  ulong SV::*offset;
  sys_var_thd_ulong(const char *name_arg, ulong SV::*offset_arg)
    :sys_var_thd(name_arg), offset(offset_arg)
  {}
  sys_var_thd_ulong(const char *name_arg, ulong SV::*offset_arg,
		   sys_after_update_func func)
    :sys_var_thd(name_arg,func), offset(offset_arg)
  {}
  bool update(THD *thd, set_var *var);
  void set_default(THD *thd, enum_var_type type);
  SHOW_TYPE type() { return SHOW_LONG; }
  byte *value_ptr(THD *thd, enum_var_type type);
};


class sys_var_thd_ha_rows :public sys_var_thd
{
public:
  ha_rows SV::*offset;
  sys_var_thd_ha_rows(const char *name_arg, ha_rows SV::*offset_arg)
    :sys_var_thd(name_arg), offset(offset_arg)
  {}
  sys_var_thd_ha_rows(const char *name_arg, ha_rows SV::*offset_arg,
		      sys_after_update_func func)
    :sys_var_thd(name_arg,func), offset(offset_arg)
  {}
  bool update(THD *thd, set_var *var);
  void set_default(THD *thd, enum_var_type type);
  SHOW_TYPE type() { return SHOW_HA_ROWS; }
  byte *value_ptr(THD *thd, enum_var_type type);
};


class sys_var_thd_ulonglong :public sys_var_thd
{
public:
  ulonglong SV::*offset;
  bool only_global;
  sys_var_thd_ulonglong(const char *name_arg, ulonglong SV::*offset_arg)
    :sys_var_thd(name_arg), offset(offset_arg)
  {}
  sys_var_thd_ulonglong(const char *name_arg, ulonglong SV::*offset_arg,
			sys_after_update_func func, bool only_global_arg)
    :sys_var_thd(name_arg, func), offset(offset_arg),
    only_global(only_global_arg)
  {}
  bool update(THD *thd, set_var *var);
  void set_default(THD *thd, enum_var_type type);
  SHOW_TYPE type() { return SHOW_LONGLONG; }
  byte *value_ptr(THD *thd, enum_var_type type);
  bool check_default(enum_var_type type)
  {
    return type == OPT_GLOBAL && !option_limits;
  }
  bool check_type(enum_var_type type)
  {
    return (only_global && type != OPT_GLOBAL);
  }
};


class sys_var_thd_bool :public sys_var_thd
{
public:
  my_bool SV::*offset;
  sys_var_thd_bool(const char *name_arg, my_bool SV::*offset_arg)
    :sys_var_thd(name_arg), offset(offset_arg)
  {}
  sys_var_thd_bool(const char *name_arg, my_bool SV::*offset_arg,
		   sys_after_update_func func)
    :sys_var_thd(name_arg,func), offset(offset_arg)
  {}
  bool update(THD *thd, set_var *var);
  void set_default(THD *thd, enum_var_type type);
  SHOW_TYPE type() { return SHOW_MY_BOOL; }
  byte *value_ptr(THD *thd, enum_var_type type);
  bool check(THD *thd, set_var *var)
  {
    return check_enum(thd, var, &bool_typelib);
  }
  bool check_update_type(Item_result type) { return 0; }
};


class sys_var_thd_enum :public sys_var_thd
{
  ulong SV::*offset;
  TYPELIB *enum_names;
public:
  sys_var_thd_enum(const char *name_arg, ulong SV::*offset_arg,
		   TYPELIB *typelib)
    :sys_var_thd(name_arg), offset(offset_arg), enum_names(typelib)
  {}
  sys_var_thd_enum(const char *name_arg, ulong SV::*offset_arg,
		   TYPELIB *typelib,
		   sys_after_update_func func)
    :sys_var_thd(name_arg,func), offset(offset_arg), enum_names(typelib)
  {}
  bool check(THD *thd, set_var *var)
  {
    return check_enum(thd, var, enum_names);
  }
  bool update(THD *thd, set_var *var);
  void set_default(THD *thd, enum_var_type type);
  SHOW_TYPE type() { return SHOW_CHAR; }
  byte *value_ptr(THD *thd, enum_var_type type);
  bool check_update_type(Item_result type) { return 0; }
};


class sys_var_thd_bit :public sys_var_thd
{
  sys_update_func update_func;
public:
  ulong bit_flag;
  bool reverse;
  sys_var_thd_bit(const char *name_arg, sys_update_func func, ulong bit,
		  bool reverse_arg=0)
    :sys_var_thd(name_arg), update_func(func), bit_flag(bit),
    reverse(reverse_arg)
  {}
  bool check(THD *thd, set_var *var)
  {
    return check_enum(thd, var, &bool_typelib);
  }
  bool update(THD *thd, set_var *var);
  bool check_update_type(Item_result type) { return 0; }
  bool check_type(enum_var_type type) { return type == OPT_GLOBAL; }
  SHOW_TYPE type() { return SHOW_MY_BOOL; }
  byte *value_ptr(THD *thd, enum_var_type type);
};


/* some variables that require special handling */

class sys_var_timestamp :public sys_var
{
public:
  sys_var_timestamp(const char *name_arg) :sys_var(name_arg) {}
  bool update(THD *thd, set_var *var);
  void set_default(THD *thd, enum_var_type type);
  bool check_type(enum_var_type type)    { return type == OPT_GLOBAL; }
  bool check_default(enum_var_type type) { return 0; }
  SHOW_TYPE type() { return SHOW_LONG; }
  byte *value_ptr(THD *thd, enum_var_type type);
};


class sys_var_last_insert_id :public sys_var
{
public:
  sys_var_last_insert_id(const char *name_arg) :sys_var(name_arg) {}
  bool update(THD *thd, set_var *var);
  bool check_type(enum_var_type type) { return type == OPT_GLOBAL; }
  SHOW_TYPE type() { return SHOW_LONGLONG; }
  byte *value_ptr(THD *thd, enum_var_type type);
};


class sys_var_insert_id :public sys_var
{
public:
  sys_var_insert_id(const char *name_arg) :sys_var(name_arg) {}
  bool update(THD *thd, set_var *var);
  bool check_type(enum_var_type type) { return type == OPT_GLOBAL; }
  SHOW_TYPE type() { return SHOW_LONGLONG; }
  byte *value_ptr(THD *thd, enum_var_type type);
};


class sys_var_slave_skip_counter :public sys_var
{
public:
  sys_var_slave_skip_counter(const char *name_arg) :sys_var(name_arg) {}
  bool check(THD *thd, set_var *var);
  bool update(THD *thd, set_var *var);
  bool check_type(enum_var_type type) { return type != OPT_GLOBAL; }
  /*
    We can't retrieve the value of this, so we don't have to define
    type() or value_ptr()
  */
};


class sys_var_rand_seed1 :public sys_var
{
public:
  sys_var_rand_seed1(const char *name_arg) :sys_var(name_arg) {}
  bool update(THD *thd, set_var *var);
  bool check_type(enum_var_type type) { return type == OPT_GLOBAL; }
};

class sys_var_rand_seed2 :public sys_var
{
public:
  sys_var_rand_seed2(const char *name_arg) :sys_var(name_arg) {}
  bool update(THD *thd, set_var *var);
  bool check_type(enum_var_type type) { return type == OPT_GLOBAL; }
};


class sys_var_thd_conv_charset :public sys_var_thd
{
public:
  sys_var_thd_conv_charset(const char *name_arg)
    :sys_var_thd(name_arg)
  {}
  bool check(THD *thd, set_var *var);
  bool update(THD *thd, set_var *var);
  SHOW_TYPE type() { return SHOW_CHAR; }
  byte *value_ptr(THD *thd, enum_var_type type);
  bool check_update_type(Item_result type)
  {
    return type != STRING_RESULT;		/* Only accept strings */
  }
  bool check_default(enum_var_type type) { return 0; }
  void set_default(THD *thd, enum_var_type type);
};


/****************************************************************************
  Classes for parsing of the SET command
****************************************************************************/

class set_var_base :public Sql_alloc
{
public:
  set_var_base() {}
  virtual ~set_var_base() {}
  virtual int check(THD *thd)=0;	/* To check privileges etc. */
  virtual int update(THD *thd)=0;	/* To set the value */
};


/* MySQL internal variables, like query_cache_size */

class set_var :public set_var_base
{
public:
  sys_var *var;
  Item *value;
  enum_var_type type;
  union
  {
    CONVERT *convert;
    ulong ulong_value;
  } save_result;

  set_var(enum_var_type type_arg, sys_var *var_arg, Item *value_arg)
    :var(var_arg), type(type_arg)
  {
    /*
      If the set value is a field, change it to a string to allow things like
      SET table_type=MYISAM;
    */
    if (value_arg && value_arg->type() == Item::FIELD_ITEM)
    {
      Item_field *item= (Item_field*) value_arg;
      if (!(value=new Item_string(item->field_name, strlen(item->field_name))))
	value=value_arg;			/* Give error message later */
    }
    else
      value=value_arg;
  }
  int check(THD *thd);
  int update(THD *thd);
};


/* User variables like @my_own_variable */

class set_var_user: public set_var_base
{
  Item_func_set_user_var *user_var_item;
public:
  set_var_user(Item_func_set_user_var *item)
    :user_var_item(item)
  {}
  int check(THD *thd);
  int update(THD *thd);
};

/* For SET PASSWORD */

class set_var_password: public set_var_base
{
  LEX_USER *user;
  char *password;
public:
  set_var_password(LEX_USER *user_arg,char *password_arg)
    :user(user_arg), password(password_arg)
  {}
  int check(THD *thd);
  int update(THD *thd);
};


/*
  Prototypes for helper functions
*/

void set_var_init();
void set_var_free();
sys_var *find_sys_var(const char *str, uint length=0);
int sql_set_variables(THD *thd, List<set_var_base> *var_list);
void fix_delay_key_write(THD *thd, enum_var_type type);

extern sys_var_str sys_charset;
