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

#ifdef USE_PRAGMA_INTERFACE
#pragma interface			/* gcc class implementation */
#endif

/****************************************************************************
  Variables that are changable runtime are declared using the
  following classes
****************************************************************************/

class sys_var;
class set_var;
typedef struct system_variables SV;
typedef struct my_locale_st MY_LOCALE;

extern TYPELIB bool_typelib, delay_key_write_typelib, sql_mode_typelib;

typedef int (*sys_check_func)(THD *,  set_var *);
typedef bool (*sys_update_func)(THD *, set_var *);
typedef void (*sys_after_update_func)(THD *,enum_var_type);
typedef void (*sys_set_default_func)(THD *, enum_var_type);
typedef byte *(*sys_value_ptr_func)(THD *thd);

class sys_var
{
public:
  struct my_option *option_limits;	/* Updated by by set_var_init() */
  uint name_length;			/* Updated by by set_var_init() */
  const char *name;
  
  sys_after_update_func after_update;
  bool no_support_one_shot;
  sys_var(const char *name_arg, sys_after_update_func func= NULL)
    :name(name_arg), after_update(func)
    , no_support_one_shot(1)
  {}
  virtual ~sys_var() {}
  virtual bool check(THD *thd, set_var *var);
  bool check_enum(THD *thd, set_var *var, TYPELIB *enum_names);
  bool check_set(THD *thd, set_var *var, TYPELIB *enum_names);
  virtual bool update(THD *thd, set_var *var)=0;
  virtual void set_default(THD *thd, enum_var_type type) {}
  virtual SHOW_TYPE type() { return SHOW_UNDEF; }
  virtual byte *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base)
  { return 0; }
  virtual bool check_type(enum_var_type type)
  { return type != OPT_GLOBAL; }		/* Error if not GLOBAL */
  virtual bool check_update_type(Item_result type)
  { return type != INT_RESULT; }		/* Assume INT */
  virtual bool check_default(enum_var_type type)
  { return option_limits == 0; }
  Item *item(THD *thd, enum_var_type type, LEX_STRING *base);
  virtual bool is_struct() { return 0; }
  virtual bool is_readonly() const { return 0; }
};


/*
  A base class for all variables that require its access to
  be guarded with a mutex.
*/

class sys_var_global: public sys_var
{
protected:
  pthread_mutex_t *guard;
public:
  sys_var_global(const char *name_arg, sys_after_update_func after_update_arg,
                 pthread_mutex_t *guard_arg)
    :sys_var(name_arg, after_update_arg), guard(guard_arg) {}
};


/*
  A global-only ulong variable that requires its access to be
  protected with a mutex.
*/

class sys_var_long_ptr_global: public sys_var_global
{
public:
  ulong *value;
  sys_var_long_ptr_global(const char *name_arg, ulong *value_ptr,
                        pthread_mutex_t *guard_arg,
                        sys_after_update_func after_update_arg= NULL)
    :sys_var_global(name_arg, after_update_arg, guard_arg), value(value_ptr) {}
  bool check(THD *thd, set_var *var);
  bool update(THD *thd, set_var *var);
  void set_default(THD *thd, enum_var_type type);
  SHOW_TYPE type() { return SHOW_LONG; }
  byte *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base)
  { return (byte*) value; }
};


/*
  A global ulong variable that is protected by LOCK_global_system_variables
*/

class sys_var_long_ptr :public sys_var_long_ptr_global
{
public:
  sys_var_long_ptr(const char *name_arg, ulong *value_ptr,
                   sys_after_update_func after_update_arg= NULL);
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
  byte *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base)
  { return (byte*) value; }
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
  byte *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base)
  { return (byte*) value; }
  bool check_update_type(Item_result type) { return 0; }
};


class sys_var_str :public sys_var
{
public:
  char *value;					// Pointer to allocated string
  uint value_length;
  sys_check_func check_func;
  sys_update_func update_func;
  sys_set_default_func set_default_func;
  sys_var_str(const char *name_arg,
	      sys_check_func check_func_arg,
	      sys_update_func update_func_arg,
	      sys_set_default_func set_default_func_arg,
              char *value_arg)
    :sys_var(name_arg), value(value_arg), check_func(check_func_arg),
    update_func(update_func_arg),set_default_func(set_default_func_arg)
  {}
  bool check(THD *thd, set_var *var);
  bool update(THD *thd, set_var *var)
  {
    return (*update_func)(thd, var);
  }
  void set_default(THD *thd, enum_var_type type)
  {
    (*set_default_func)(thd, type);
  }
  SHOW_TYPE type() { return SHOW_CHAR; }
  byte *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base)
  { return (byte*) value; }
  bool check_update_type(Item_result type)
  {
    return type != STRING_RESULT;		/* Only accept strings */
  }
  bool check_default(enum_var_type type) { return 0; }
};


class sys_var_const_str :public sys_var
{
public:
  char *value;					// Pointer to const value
  sys_var_const_str(const char *name_arg, const char *value_arg)
    :sys_var(name_arg),value((char*) value_arg)
  {}
  bool check(THD *thd, set_var *var)
  {
    return 1;
  }
  bool update(THD *thd, set_var *var)
  {
    return 1;
  }
  SHOW_TYPE type() { return SHOW_CHAR; }
  byte *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base)
  {
    return (byte*) value;
  }
  bool check_update_type(Item_result type)
  {
    return 1;
  }
  bool check_default(enum_var_type type) { return 1; }
  bool is_readonly() const { return 1; }
};


class sys_var_const_str_ptr :public sys_var
{
public:
  char **value;					// Pointer to const value
  sys_var_const_str_ptr(const char *name_arg, char **value_arg)
    :sys_var(name_arg),value(value_arg)
  {}
  bool check(THD *thd, set_var *var)
  {
    return 1;
  }
  bool update(THD *thd, set_var *var)
  {
    return 1;
  }
  SHOW_TYPE type() { return SHOW_CHAR; }
  byte *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base)
  {
    return (byte*) *value;
  }
  bool check_update_type(Item_result type)
  {
    return 1;
  }
  bool check_default(enum_var_type type) { return 1; }
  bool is_readonly() const { return 1; }
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
  byte *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base);
  bool check_update_type(Item_result type) { return 0; }
};


class sys_var_thd :public sys_var
{
public:
  sys_var_thd(const char *name_arg, sys_after_update_func func= NULL)
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
  sys_check_func check_func;
public:
  ulong SV::*offset;
  sys_var_thd_ulong(const char *name_arg, ulong SV::*offset_arg)
    :sys_var_thd(name_arg), check_func(0), offset(offset_arg)
  {}
  sys_var_thd_ulong(const char *name_arg, ulong SV::*offset_arg,
		   sys_check_func c_func, sys_after_update_func au_func)
    :sys_var_thd(name_arg,au_func), check_func(c_func), offset(offset_arg)
  {}
  bool check(THD *thd, set_var *var);
  bool update(THD *thd, set_var *var);
  void set_default(THD *thd, enum_var_type type);
  SHOW_TYPE type() { return SHOW_LONG; }
  byte *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base);
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
  byte *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base);
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
  byte *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base);
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
  byte *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base);
  bool check(THD *thd, set_var *var)
  {
    return check_enum(thd, var, &bool_typelib);
  }
  bool check_update_type(Item_result type) { return 0; }
};


class sys_var_thd_enum :public sys_var_thd
{
protected:
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
  byte *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base);
  bool check_update_type(Item_result type) { return 0; }
};


extern void fix_sql_mode_var(THD *thd, enum_var_type type);

class sys_var_thd_sql_mode :public sys_var_thd_enum
{
public:
  sys_var_thd_sql_mode(const char *name_arg, ulong SV::*offset_arg)
    :sys_var_thd_enum(name_arg, offset_arg, &sql_mode_typelib,
		      fix_sql_mode_var)
  {}
  bool check(THD *thd, set_var *var)
  {
    return check_set(thd, var, enum_names);
  }
  void set_default(THD *thd, enum_var_type type);
  byte *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base);
  static byte *symbolic_mode_representation(THD *thd, ulong sql_mode,
                                            ulong *length);
};


class sys_var_thd_storage_engine :public sys_var_thd
{
protected:
  ulong SV::*offset;
public:
  sys_var_thd_storage_engine(const char *name_arg, ulong SV::*offset_arg)
    :sys_var_thd(name_arg), offset(offset_arg)
  {}
  bool check(THD *thd, set_var *var);
SHOW_TYPE type() { return SHOW_CHAR; }
  bool check_update_type(Item_result type)
  {
    return type != STRING_RESULT;		/* Only accept strings */
  }
  void set_default(THD *thd, enum_var_type type);
  bool update(THD *thd, set_var *var);
  byte *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base);
};

class sys_var_thd_table_type :public sys_var_thd_storage_engine
{
public:
  sys_var_thd_table_type(const char *name_arg, ulong SV::*offset_arg)
    :sys_var_thd_storage_engine(name_arg, offset_arg)
  {}
  void warn_deprecated(THD *thd);
  void set_default(THD *thd, enum_var_type type);
  bool update(THD *thd, set_var *var);
};

class sys_var_thd_bit :public sys_var_thd
{
  sys_check_func check_func;
  sys_update_func update_func;
public:
  ulong bit_flag;
  bool reverse;
  sys_var_thd_bit(const char *name_arg, 
                  sys_check_func c_func, sys_update_func u_func,
                  ulong bit, bool reverse_arg=0)
    :sys_var_thd(name_arg), check_func(c_func), update_func(u_func),
    bit_flag(bit), reverse(reverse_arg)
  {}
  bool check(THD *thd, set_var *var);
  bool update(THD *thd, set_var *var);
  bool check_update_type(Item_result type) { return 0; }
  bool check_type(enum_var_type type) { return type == OPT_GLOBAL; }
  SHOW_TYPE type() { return SHOW_MY_BOOL; }
  byte *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base);
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
  byte *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base);
};


class sys_var_last_insert_id :public sys_var
{
public:
  sys_var_last_insert_id(const char *name_arg) :sys_var(name_arg) {}
  bool update(THD *thd, set_var *var);
  bool check_type(enum_var_type type) { return type == OPT_GLOBAL; }
  SHOW_TYPE type() { return SHOW_LONGLONG; }
  byte *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base);
};


class sys_var_insert_id :public sys_var
{
public:
  sys_var_insert_id(const char *name_arg) :sys_var(name_arg) {}
  bool update(THD *thd, set_var *var);
  bool check_type(enum_var_type type) { return type == OPT_GLOBAL; }
  SHOW_TYPE type() { return SHOW_LONGLONG; }
  byte *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base);
};


#ifdef HAVE_REPLICATION
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

class sys_var_sync_binlog_period :public sys_var_long_ptr
{
public:
  sys_var_sync_binlog_period(const char *name_arg, ulong *value_ptr)
    :sys_var_long_ptr(name_arg,value_ptr) {}
  bool update(THD *thd, set_var *var);
};
#endif

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


class sys_var_collation :public sys_var_thd
{
public:
  sys_var_collation(const char *name_arg) :sys_var_thd(name_arg)
    {
    no_support_one_shot= 0;
    }
  bool check(THD *thd, set_var *var);
SHOW_TYPE type() { return SHOW_CHAR; }
  bool check_update_type(Item_result type)
  {
    return ((type != STRING_RESULT) && (type != INT_RESULT));
  }
  bool check_default(enum_var_type type) { return 0; }
  virtual void set_default(THD *thd, enum_var_type type)= 0;
};

class sys_var_character_set :public sys_var_thd
{
public:
  bool nullable;
  sys_var_character_set(const char *name_arg) :
    sys_var_thd(name_arg)
  {
    nullable= 0;
    /*
      In fact only almost all variables derived from sys_var_character_set
      support ONE_SHOT; character_set_results doesn't. But that's good enough.
    */
    no_support_one_shot= 0;
  }
  bool check(THD *thd, set_var *var);
  SHOW_TYPE type() { return SHOW_CHAR; }
  bool check_update_type(Item_result type)
  {
    return ((type != STRING_RESULT) && (type != INT_RESULT));
  }
  bool check_default(enum_var_type type) { return 0; }
  bool update(THD *thd, set_var *var);
  byte *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base);
  virtual void set_default(THD *thd, enum_var_type type)= 0;
  virtual CHARSET_INFO **ci_ptr(THD *thd, enum_var_type type)= 0;
};

class sys_var_character_set_filesystem :public sys_var_character_set
{
public:
  sys_var_character_set_filesystem(const char *name_arg) :
    sys_var_character_set(name_arg) {}
  void set_default(THD *thd, enum_var_type type);
  CHARSET_INFO **ci_ptr(THD *thd, enum_var_type type);
};

class sys_var_character_set_client :public sys_var_character_set
{
public:
  sys_var_character_set_client(const char *name_arg) :
    sys_var_character_set(name_arg) {}
  void set_default(THD *thd, enum_var_type type);
  CHARSET_INFO **ci_ptr(THD *thd, enum_var_type type);
};

class sys_var_character_set_results :public sys_var_character_set
{
public:
  sys_var_character_set_results(const char *name_arg) :
    sys_var_character_set(name_arg) 
    { nullable= 1; }
  void set_default(THD *thd, enum_var_type type);
  CHARSET_INFO **ci_ptr(THD *thd, enum_var_type type);
};

class sys_var_character_set_server :public sys_var_character_set
{
public:
  sys_var_character_set_server(const char *name_arg) :
    sys_var_character_set(name_arg) {}
  void set_default(THD *thd, enum_var_type type);
  CHARSET_INFO **ci_ptr(THD *thd, enum_var_type type);
};

class sys_var_character_set_database :public sys_var_character_set
{
public:
  sys_var_character_set_database(const char *name_arg) :
    sys_var_character_set(name_arg) {}
  void set_default(THD *thd, enum_var_type type);
  CHARSET_INFO **ci_ptr(THD *thd, enum_var_type type);
};

class sys_var_character_set_connection :public sys_var_character_set
{
public:
  sys_var_character_set_connection(const char *name_arg) :
    sys_var_character_set(name_arg) {}
  void set_default(THD *thd, enum_var_type type);
  CHARSET_INFO **ci_ptr(THD *thd, enum_var_type type);
};

class sys_var_collation_connection :public sys_var_collation
{
public:
  sys_var_collation_connection(const char *name_arg) :sys_var_collation(name_arg) {}
  bool update(THD *thd, set_var *var);
  void set_default(THD *thd, enum_var_type type);
  byte *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base);
};

class sys_var_collation_server :public sys_var_collation
{
public:
  sys_var_collation_server(const char *name_arg) :sys_var_collation(name_arg) {}
  bool update(THD *thd, set_var *var);
  void set_default(THD *thd, enum_var_type type);
  byte *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base);
};

class sys_var_collation_database :public sys_var_collation
{
public:
  sys_var_collation_database(const char *name_arg) :sys_var_collation(name_arg) {}
  bool update(THD *thd, set_var *var);
  void set_default(THD *thd, enum_var_type type);
  byte *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base);
};


class sys_var_key_cache_param :public sys_var
{
protected:
  size_t offset;
public:
  sys_var_key_cache_param(const char *name_arg, size_t offset_arg)
    :sys_var(name_arg), offset(offset_arg)
  {}
  byte *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base);
  bool check_default(enum_var_type type) { return 1; }
  bool is_struct() { return 1; }
};


class sys_var_key_buffer_size :public sys_var_key_cache_param
{
public:
  sys_var_key_buffer_size(const char *name_arg)
    :sys_var_key_cache_param(name_arg, offsetof(KEY_CACHE, param_buff_size))
  {}
  bool update(THD *thd, set_var *var);
  SHOW_TYPE type() { return SHOW_LONGLONG; }
};


class sys_var_key_cache_long :public sys_var_key_cache_param
{
public:
  sys_var_key_cache_long(const char *name_arg, size_t offset_arg)
    :sys_var_key_cache_param(name_arg, offset_arg)
  {}
  bool update(THD *thd, set_var *var);
  SHOW_TYPE type() { return SHOW_LONG; }
};


class sys_var_thd_date_time_format :public sys_var_thd
{
  DATE_TIME_FORMAT *SV::*offset;
  timestamp_type date_time_type;
public:
  sys_var_thd_date_time_format(const char *name_arg,
			       DATE_TIME_FORMAT *SV::*offset_arg,
			       timestamp_type date_time_type_arg)
    :sys_var_thd(name_arg), offset(offset_arg),
    date_time_type(date_time_type_arg)
  {}
  SHOW_TYPE type() { return SHOW_CHAR; }
  bool check_update_type(Item_result type)
  {
    return type != STRING_RESULT;		/* Only accept strings */
  }
  bool check_default(enum_var_type type) { return 0; }
  bool check(THD *thd, set_var *var);
  bool update(THD *thd, set_var *var);
  void update2(THD *thd, enum_var_type type, DATE_TIME_FORMAT *new_value);
  byte *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base);
  void set_default(THD *thd, enum_var_type type);
};


/* Variable that you can only read from */

class sys_var_readonly: public sys_var
{
public:
  enum_var_type var_type;
  SHOW_TYPE show_type;
  sys_value_ptr_func value_ptr_func;
  sys_var_readonly(const char *name_arg, enum_var_type type,
		   SHOW_TYPE show_type_arg,
		   sys_value_ptr_func value_ptr_func_arg)
    :sys_var(name_arg), var_type(type), 
       show_type(show_type_arg), value_ptr_func(value_ptr_func_arg)
  {}
  bool update(THD *thd, set_var *var) { return 1; }
  bool check_default(enum_var_type type) { return 1; }
  bool check_type(enum_var_type type) { return type != var_type; }
  bool check_update_type(Item_result type) { return 1; }
  byte *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base)
  {
    return (*value_ptr_func)(thd);
  }
  SHOW_TYPE type() { return show_type; }
  bool is_readonly() const { return 1; }
};

class sys_var_thd_time_zone :public sys_var_thd
{
public:
  sys_var_thd_time_zone(const char *name_arg):
    sys_var_thd(name_arg) 
  {
    no_support_one_shot= 0;
  }
  bool check(THD *thd, set_var *var);
  SHOW_TYPE type() { return SHOW_CHAR; }
  bool check_update_type(Item_result type)
  {
    return type != STRING_RESULT;		/* Only accept strings */
  }
  bool check_default(enum_var_type type) { return 0; }
  bool update(THD *thd, set_var *var);
  byte *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base);
  virtual void set_default(THD *thd, enum_var_type type);
};


class sys_var_max_user_conn : public sys_var_thd
{
public:
  sys_var_max_user_conn(const char *name_arg):
    sys_var_thd(name_arg) {}
  bool check(THD *thd, set_var *var);
  bool update(THD *thd, set_var *var);
  bool check_default(enum_var_type type)
  {
    return type != OPT_GLOBAL || !option_limits;
  }
  void set_default(THD *thd, enum_var_type type);
  SHOW_TYPE type() { return SHOW_INT; }
  byte *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base);
};

class sys_var_trust_routine_creators :public sys_var_bool_ptr
{
  /* We need a derived class only to have a warn_deprecated() */
public:
  sys_var_trust_routine_creators(const char *name_arg, my_bool *value_arg) :
    sys_var_bool_ptr(name_arg, value_arg) {};
  void warn_deprecated(THD *thd);
  void set_default(THD *thd, enum_var_type type);
  bool update(THD *thd, set_var *var);
};


class sys_var_thd_lc_time_names :public sys_var_thd
{
public:
  sys_var_thd_lc_time_names(const char *name_arg):
    sys_var_thd(name_arg)
  {}
  bool check(THD *thd, set_var *var);
  SHOW_TYPE type() { return SHOW_CHAR; }
  bool check_update_type(Item_result type)
  {
    return type != STRING_RESULT;		/* Only accept strings */
  }
  bool check_default(enum_var_type type) { return 0; }
  bool update(THD *thd, set_var *var);
  byte *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base);
  virtual void set_default(THD *thd, enum_var_type type);
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
  /* light check for PS */
  virtual int light_check(THD *thd) { return check(thd); }
  virtual bool no_support_one_shot() { return 1; }
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
    CHARSET_INFO *charset;
    ulong ulong_value;
    ulonglong ulonglong_value;
    DATE_TIME_FORMAT *date_time_format;
    Time_zone *time_zone;
    MY_LOCALE *locale_value;
  } save_result;
  LEX_STRING base;			/* for structs */

  set_var(enum_var_type type_arg, sys_var *var_arg, const LEX_STRING *base_name_arg,
	  Item *value_arg)
    :var(var_arg), type(type_arg), base(*base_name_arg)
  {
    /*
      If the set value is a field, change it to a string to allow things like
      SET table_type=MYISAM;
    */
    if (value_arg && value_arg->type() == Item::FIELD_ITEM)
    {
      Item_field *item= (Item_field*) value_arg;
      if (!(value=new Item_string(item->field_name, 
                  (uint) strlen(item->field_name),
				  item->collation.collation)))
	value=value_arg;			/* Give error message later */
    }
    else
      value=value_arg;
  }
  int check(THD *thd);
  int update(THD *thd);
  int light_check(THD *thd);
  bool no_support_one_shot() { return var->no_support_one_shot; }
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
  int light_check(THD *thd);
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


/* For SET NAMES and SET CHARACTER SET */

class set_var_collation_client: public set_var_base
{
  CHARSET_INFO *character_set_client;
  CHARSET_INFO *character_set_results;
  CHARSET_INFO *collation_connection;
public:
  set_var_collation_client(CHARSET_INFO *client_coll_arg,
  			   CHARSET_INFO *connection_coll_arg,
  			   CHARSET_INFO *result_coll_arg)
    :character_set_client(client_coll_arg),
     character_set_results(result_coll_arg),
     collation_connection(connection_coll_arg)
  {}
  int check(THD *thd);
  int update(THD *thd);
};


/* Named lists (used for keycaches) */

class NAMED_LIST :public ilink
{
  const char *name;
  uint name_length;
public:
  gptr data;

  NAMED_LIST(I_List<NAMED_LIST> *links, const char *name_arg,
	     uint name_length_arg, gptr data_arg)
    :name_length(name_length_arg), data(data_arg)
  {
    name= my_strdup_with_length(name_arg, name_length, MYF(MY_WME));
    links->push_back(this);
  }
  inline bool cmp(const char *name_cmp, uint length)
  {
    return length == name_length && !memcmp(name, name_cmp, length);
  }
  ~NAMED_LIST()
  {
    my_free((char*) name, MYF(0));
  }
  friend bool process_key_caches(int (* func) (const char *name,
					       KEY_CACHE *));
  friend void delete_elements(I_List<NAMED_LIST> *list,
			      void (*free_element)(const char*, gptr));
};

/* updated in sql_acl.cc */

extern sys_var_thd_bool sys_old_passwords;
extern LEX_STRING default_key_cache_base;

/* For sql_yacc */
struct sys_var_with_base
{
  sys_var *var;
  LEX_STRING base_name;
};

/*
  Prototypes for helper functions
*/

void set_var_init();
void set_var_free();
sys_var *find_sys_var(const char *str, uint length=0);
int sql_set_variables(THD *thd, List<set_var_base> *var_list);
bool not_all_support_one_shot(List<set_var_base> *var_list);
void fix_delay_key_write(THD *thd, enum_var_type type);
ulong fix_sql_mode(ulong sql_mode);
extern sys_var_const_str sys_charset_system;
extern sys_var_str sys_init_connect;
extern sys_var_str sys_init_slave;
extern sys_var_thd_time_zone sys_time_zone;
extern sys_var_thd_bit sys_autocommit;
CHARSET_INFO *get_old_charset_by_name(const char *old_name);
gptr find_named(I_List<NAMED_LIST> *list, const char *name, uint length,
		NAMED_LIST **found);

/* key_cache functions */
KEY_CACHE *get_key_cache(LEX_STRING *cache_name);
KEY_CACHE *get_or_create_key_cache(const char *name, uint length);
void free_key_cache(const char *name, KEY_CACHE *key_cache);
bool process_key_caches(int (* func) (const char *name, KEY_CACHE *));
void delete_elements(I_List<NAMED_LIST> *list,
		     void (*free_element)(const char*, gptr));
