/* Copyright (C) 2002-2006 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

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
class sys_var_pluginvar; /* opaque */
typedef struct system_variables SV;
typedef struct my_locale_st MY_LOCALE;

extern TYPELIB bool_typelib, delay_key_write_typelib, sql_mode_typelib,
  optimizer_switch_typelib, slave_exec_mode_typelib;

typedef int (*sys_check_func)(THD *,  set_var *);
typedef bool (*sys_update_func)(THD *, set_var *);
typedef void (*sys_after_update_func)(THD *,enum_var_type);
typedef void (*sys_set_default_func)(THD *, enum_var_type);
typedef uchar *(*sys_value_ptr_func)(THD *thd);

struct sys_var_chain
{
  sys_var *first;
  sys_var *last;
};

class sys_var
{
public:

  /**
    Enumeration type to indicate for a system variable whether it will be written to the binlog or not.
  */
  enum Binlog_status_enum
  {  
    /* The variable value is not in the binlog. */
    NOT_IN_BINLOG,
    /* The value of the @@session variable is in the binlog. */
    SESSION_VARIABLE_IN_BINLOG
    /*
      Currently, no @@global variable is ever in the binlog, so we
      don't need an enumeration value for that.
    */
  };

  sys_var *next;
  struct my_option *option_limits;	/* Updated by by set_var_init() */
  uint name_length;			/* Updated by by set_var_init() */
  const char *name;

  sys_after_update_func after_update;
  bool no_support_one_shot;
  /*
    true if the value is in character_set_filesystem, 
    false otherwise.
    Note that we can't use a pointer to the charset as the system var is 
    instantiated in global scope and the charset pointers are initialized
    later.
  */  
  bool is_os_charset;
  sys_var(const char *name_arg, sys_after_update_func func= NULL,
          Binlog_status_enum binlog_status_arg= NOT_IN_BINLOG)
    :name(name_arg), after_update(func), no_support_one_shot(1),
    is_os_charset (FALSE),
    binlog_status(binlog_status_arg),
    m_allow_empty_value(TRUE)
  {}
  virtual ~sys_var() {}
  void chain_sys_var(sys_var_chain *chain_arg)
  {
    if (chain_arg->last)
      chain_arg->last->next= this;
    else
      chain_arg->first= this;
    chain_arg->last= this;
  }
  virtual bool check(THD *thd, set_var *var);
  bool check_enum(THD *thd, set_var *var, const TYPELIB *enum_names);
  bool check_set(THD *thd, set_var *var, TYPELIB *enum_names);
  bool is_written_to_binlog(enum_var_type type)
  {
    return (type == OPT_SESSION || type == OPT_DEFAULT) &&
      (binlog_status == SESSION_VARIABLE_IN_BINLOG);
  }
  virtual bool update(THD *thd, set_var *var)=0;
  virtual void set_default(THD *thd_arg, enum_var_type type) {}
  virtual SHOW_TYPE show_type() { return SHOW_UNDEF; }
  virtual uchar *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base)
  { return 0; }
  virtual bool check_type(enum_var_type type)
  { return type != OPT_GLOBAL; }		/* Error if not GLOBAL */
  virtual bool check_update_type(Item_result type)
  { return type != INT_RESULT; }		/* Assume INT */
  virtual bool check_default(enum_var_type type)
  { return option_limits == 0; }
  virtual bool is_struct() { return 0; }
  virtual bool is_readonly() const { return 0; }
  CHARSET_INFO *charset(THD *thd);
  virtual sys_var_pluginvar *cast_pluginvar() { return 0; }

protected:
  void set_allow_empty_value(bool allow_empty_value)
  {
    m_allow_empty_value= allow_empty_value;
  }

private:
  const Binlog_status_enum binlog_status;

  bool m_allow_empty_value;
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
  sys_var_long_ptr_global(sys_var_chain *chain, const char *name_arg,
                          ulong *value_ptr_arg,
                          pthread_mutex_t *guard_arg,
                          sys_after_update_func after_update_arg= NULL)
    :sys_var_global(name_arg, after_update_arg, guard_arg),
    value(value_ptr_arg)
  { chain_sys_var(chain); }
  bool check(THD *thd, set_var *var);
  bool update(THD *thd, set_var *var);
  void set_default(THD *thd, enum_var_type type);
  SHOW_TYPE show_type() { return SHOW_LONG; }
  uchar *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base)
  { return (uchar*) value; }
};


/*
  A global ulong variable that is protected by LOCK_global_system_variables
*/

class sys_var_long_ptr :public sys_var_long_ptr_global
{
public:
  sys_var_long_ptr(sys_var_chain *chain, const char *name_arg, ulong *value_ptr,
                   sys_after_update_func after_update_arg= NULL);
};


class sys_var_ulonglong_ptr :public sys_var
{
public:
  ulonglong *value;
  sys_var_ulonglong_ptr(sys_var_chain *chain, const char *name_arg, ulonglong *value_ptr_arg)
    :sys_var(name_arg),value(value_ptr_arg)
  { chain_sys_var(chain); }
  sys_var_ulonglong_ptr(sys_var_chain *chain, const char *name_arg, ulonglong *value_ptr_arg,
		       sys_after_update_func func)
    :sys_var(name_arg,func), value(value_ptr_arg)
  { chain_sys_var(chain); }
  bool update(THD *thd, set_var *var);
  void set_default(THD *thd, enum_var_type type);
  SHOW_TYPE show_type() { return SHOW_LONGLONG; }
  uchar *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base)
  { return (uchar*) value; }
};


class sys_var_bool_ptr :public sys_var
{
public:
  my_bool *value;
  sys_var_bool_ptr(sys_var_chain *chain, const char *name_arg, my_bool *value_arg)
    :sys_var(name_arg),value(value_arg)
  { chain_sys_var(chain); }
  bool check(THD *thd, set_var *var)
  {
    return check_enum(thd, var, &bool_typelib);
  }
  bool update(THD *thd, set_var *var);
  void set_default(THD *thd, enum_var_type type);
  SHOW_TYPE show_type() { return SHOW_MY_BOOL; }
  uchar *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base)
  { return (uchar*) value; }
  bool check_update_type(Item_result type) { return 0; }
};


class sys_var_bool_ptr_readonly :public sys_var_bool_ptr
{
public:
  sys_var_bool_ptr_readonly(sys_var_chain *chain, const char *name_arg,
                            my_bool *value_arg)
    :sys_var_bool_ptr(chain, name_arg, value_arg)
  {}
  bool is_readonly() const { return 1; }
};


class sys_var_str :public sys_var
{
public:
  char *value;					// Pointer to allocated string
  uint value_length;
  sys_check_func check_func;
  sys_update_func update_func;
  sys_set_default_func set_default_func;
  sys_var_str(sys_var_chain *chain, const char *name_arg,
	      sys_check_func check_func_arg,
	      sys_update_func update_func_arg,
	      sys_set_default_func set_default_func_arg,
              char *value_arg)
    :sys_var(name_arg), value(value_arg), check_func(check_func_arg),
    update_func(update_func_arg),set_default_func(set_default_func_arg)
  { chain_sys_var(chain); }
  bool check(THD *thd, set_var *var);
  bool update(THD *thd, set_var *var)
  {
    return (*update_func)(thd, var);
  }
  void set_default(THD *thd, enum_var_type type)
  {
    (*set_default_func)(thd, type);
  }
  SHOW_TYPE show_type() { return SHOW_CHAR; }
  uchar *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base)
  { return (uchar*) value; }
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
  sys_var_const_str(sys_var_chain *chain, const char *name_arg,
                    const char *value_arg)
    :sys_var(name_arg), value((char*) value_arg)
  { chain_sys_var(chain); }
  bool check(THD *thd, set_var *var)
  {
    return 1;
  }
  bool update(THD *thd, set_var *var)
  {
    return 1;
  }
  SHOW_TYPE show_type() { return SHOW_CHAR; }
  uchar *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base)
  {
    return (uchar*) value;
  }
  bool check_update_type(Item_result type)
  {
    return 1;
  }
  bool check_default(enum_var_type type) { return 1; }
  bool is_readonly() const { return 1; }
};


class sys_var_const_os_str: public sys_var_const_str
{
public:
  sys_var_const_os_str(sys_var_chain *chain, const char *name_arg, 
                       const char *value_arg)
    :sys_var_const_str(chain, name_arg, value_arg)
  { 
    is_os_charset= TRUE; 
  }
};


class sys_var_const_str_ptr :public sys_var
{
public:
  char **value;					// Pointer to const value
  sys_var_const_str_ptr(sys_var_chain *chain, const char *name_arg, char **value_arg)
    :sys_var(name_arg),value(value_arg)
  { chain_sys_var(chain); }
  bool check(THD *thd, set_var *var)
  {
    return 1;
  }
  bool update(THD *thd, set_var *var)
  {
    return 1;
  }
  SHOW_TYPE show_type() { return SHOW_CHAR; }
  uchar *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base)
  {
    return (uchar*) *value;
  }
  bool check_update_type(Item_result type)
  {
    return 1;
  }
  bool check_default(enum_var_type type) { return 1; }
  bool is_readonly() const { return 1; }
};


class sys_var_const_os_str_ptr :public sys_var_const_str_ptr
{
public:
  sys_var_const_os_str_ptr(sys_var_chain *chain, const char *name_arg, 
                           char **value_arg)
    :sys_var_const_str_ptr(chain, name_arg, value_arg)
  {
    is_os_charset= TRUE; 
  }
};


class sys_var_enum :public sys_var
{
  uint *value;
  TYPELIB *enum_names;
public:
  sys_var_enum(sys_var_chain *chain, const char *name_arg, uint *value_arg,
	       TYPELIB *typelib, sys_after_update_func func)
    :sys_var(name_arg,func), value(value_arg), enum_names(typelib)
  { chain_sys_var(chain); }
  bool check(THD *thd, set_var *var)
  {
    return check_enum(thd, var, enum_names);
  }
  bool update(THD *thd, set_var *var);
  SHOW_TYPE show_type() { return SHOW_CHAR; }
  uchar *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base);
  bool check_update_type(Item_result type) { return 0; }
};


class sys_var_enum_const :public sys_var
{
  ulong SV::*offset;
  TYPELIB *enum_names;
public:
  sys_var_enum_const(sys_var_chain *chain, const char *name_arg, ulong SV::*offset_arg,
      TYPELIB *typelib, sys_after_update_func func)
    :sys_var(name_arg,func), offset(offset_arg), enum_names(typelib)
  { chain_sys_var(chain); }
  bool check(THD *thd, set_var *var) { return 1; }
  bool update(THD *thd, set_var *var) { return 1; }
  SHOW_TYPE show_type() { return SHOW_CHAR; }
  bool check_update_type(Item_result type) { return 1; }
  bool is_readonly() const { return 1; }
  uchar *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base);
};


class sys_var_thd :public sys_var
{
public:
  sys_var_thd(const char *name_arg, 
              sys_after_update_func func= NULL,
              Binlog_status_enum binlog_status= NOT_IN_BINLOG)
    :sys_var(name_arg, func, binlog_status)
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
  sys_var_thd_ulong(sys_var_chain *chain, const char *name_arg,
                    ulong SV::*offset_arg,
                    sys_check_func c_func= NULL,
                    sys_after_update_func au_func= NULL,
                    Binlog_status_enum binlog_status_arg= NOT_IN_BINLOG)
    :sys_var_thd(name_arg, au_func, binlog_status_arg), check_func(c_func),
    offset(offset_arg)
  { chain_sys_var(chain); }
  bool check(THD *thd, set_var *var);
  bool update(THD *thd, set_var *var);
  void set_default(THD *thd, enum_var_type type);
  SHOW_TYPE show_type() { return SHOW_LONG; }
  uchar *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base);
};


class sys_var_thd_ha_rows :public sys_var_thd
{
public:
  ha_rows SV::*offset;
  sys_var_thd_ha_rows(sys_var_chain *chain, const char *name_arg, 
                      ha_rows SV::*offset_arg)
    :sys_var_thd(name_arg), offset(offset_arg)
  { chain_sys_var(chain); }
  sys_var_thd_ha_rows(sys_var_chain *chain, const char *name_arg, 
                      ha_rows SV::*offset_arg,
		      sys_after_update_func func)
    :sys_var_thd(name_arg,func), offset(offset_arg)
  { chain_sys_var(chain); }
  bool update(THD *thd, set_var *var);
  void set_default(THD *thd, enum_var_type type);
  SHOW_TYPE show_type() { return SHOW_HA_ROWS; }
  uchar *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base);
};


class sys_var_thd_ulonglong :public sys_var_thd
{
public:
  ulonglong SV::*offset;
  bool only_global;
  sys_var_thd_ulonglong(sys_var_chain *chain, const char *name_arg, 
                        ulonglong SV::*offset_arg)
    :sys_var_thd(name_arg), offset(offset_arg)
  { chain_sys_var(chain); }
  sys_var_thd_ulonglong(sys_var_chain *chain, const char *name_arg, 
                        ulonglong SV::*offset_arg,
			sys_after_update_func func, bool only_global_arg)
    :sys_var_thd(name_arg, func), offset(offset_arg),
    only_global(only_global_arg)
  { chain_sys_var(chain); }
  bool update(THD *thd, set_var *var);
  void set_default(THD *thd, enum_var_type type);
  SHOW_TYPE show_type() { return SHOW_LONGLONG; }
  uchar *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base);
  bool check(THD *thd, set_var *var);
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
  sys_var_thd_bool(sys_var_chain *chain, const char *name_arg, my_bool SV::*offset_arg)
    :sys_var_thd(name_arg), offset(offset_arg)
  { chain_sys_var(chain); }
  sys_var_thd_bool(sys_var_chain *chain, const char *name_arg, my_bool SV::*offset_arg,
		   sys_after_update_func func)
    :sys_var_thd(name_arg,func), offset(offset_arg)
  { chain_sys_var(chain); }
  bool update(THD *thd, set_var *var);
  void set_default(THD *thd, enum_var_type type);
  SHOW_TYPE show_type() { return SHOW_MY_BOOL; }
  uchar *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base);
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
  sys_check_func check_func;
public:
  sys_var_thd_enum(sys_var_chain *chain, const char *name_arg,
                   ulong SV::*offset_arg, TYPELIB *typelib,
                   sys_after_update_func func= NULL,
                   sys_check_func check= NULL)
    :sys_var_thd(name_arg, func), offset(offset_arg),
    enum_names(typelib), check_func(check)
  { chain_sys_var(chain); }
  bool check(THD *thd, set_var *var)
  {
    int ret= 0;
    if (check_func)
      ret= (*check_func)(thd, var);
    return ret ? ret : check_enum(thd, var, enum_names);
  }
  bool update(THD *thd, set_var *var);
  void set_default(THD *thd, enum_var_type type);
  SHOW_TYPE show_type() { return SHOW_CHAR; }
  uchar *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base);
  bool check_update_type(Item_result type) { return 0; }
};


class sys_var_thd_optimizer_switch :public sys_var_thd_enum
{
public:
  sys_var_thd_optimizer_switch(sys_var_chain *chain, const char *name_arg, 
                               ulong SV::*offset_arg)
    :sys_var_thd_enum(chain, name_arg, offset_arg, &optimizer_switch_typelib)
  {}
  bool check(THD *thd, set_var *var);
  void set_default(THD *thd, enum_var_type type);
  uchar *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base);
  static bool symbolic_mode_representation(THD *thd, ulonglong sql_mode,
                                           LEX_STRING *rep);
};

extern void fix_sql_mode_var(THD *thd, enum_var_type type);

class sys_var_thd_sql_mode :public sys_var_thd_enum
{
public:
  sys_var_thd_sql_mode(sys_var_chain *chain, const char *name_arg, 
                       ulong SV::*offset_arg)
    :sys_var_thd_enum(chain, name_arg, offset_arg, &sql_mode_typelib,
                      fix_sql_mode_var)
  {}
  bool check(THD *thd, set_var *var)
  {
    return check_set(thd, var, enum_names);
  }
  void set_default(THD *thd, enum_var_type type);
  uchar *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base);
  static bool symbolic_mode_representation(THD *thd, ulonglong sql_mode,
                                           LEX_STRING *rep);
};


class sys_var_thd_storage_engine :public sys_var_thd
{
protected:
  plugin_ref SV::*offset;
public:
  sys_var_thd_storage_engine(sys_var_chain *chain, const char *name_arg, 
                             plugin_ref SV::*offset_arg)
    :sys_var_thd(name_arg), offset(offset_arg)
  { chain_sys_var(chain); }
  bool check(THD *thd, set_var *var);
  SHOW_TYPE show_type() { return SHOW_CHAR; }
  bool check_update_type(Item_result type)
  {
    return type != STRING_RESULT;		/* Only accept strings */
  }
  void set_default(THD *thd, enum_var_type type);
  bool update(THD *thd, set_var *var);
  uchar *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base);
};

class sys_var_thd_table_type :public sys_var_thd_storage_engine
{
public:
  sys_var_thd_table_type(sys_var_chain *chain, const char *name_arg, 
                         plugin_ref SV::*offset_arg)
    :sys_var_thd_storage_engine(chain, name_arg, offset_arg)
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
  ulonglong bit_flag;
  bool reverse;
  sys_var_thd_bit(sys_var_chain *chain, const char *name_arg,
                  sys_check_func c_func, sys_update_func u_func,
                  ulonglong bit, bool reverse_arg=0,
                  Binlog_status_enum binlog_status_arg= NOT_IN_BINLOG)
    :sys_var_thd(name_arg, NULL, binlog_status_arg), check_func(c_func),
    update_func(u_func), bit_flag(bit), reverse(reverse_arg)
  { chain_sys_var(chain); }
  bool check(THD *thd, set_var *var);
  bool update(THD *thd, set_var *var);
  bool check_update_type(Item_result type) { return 0; }
  bool check_type(enum_var_type type) { return type == OPT_GLOBAL; }
  SHOW_TYPE show_type() { return SHOW_MY_BOOL; }
  uchar *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base);
};

#ifndef DBUG_OFF
class sys_var_thd_dbug :public sys_var_thd
{
public:
  sys_var_thd_dbug(sys_var_chain *chain, const char *name_arg)
    :sys_var_thd(name_arg)
  { chain_sys_var(chain); }
  bool check_update_type(Item_result type) { return type != STRING_RESULT; }
  bool check(THD *thd, set_var *var);
  SHOW_TYPE show_type() { return SHOW_CHAR; }
  bool update(THD *thd, set_var *var);
  void set_default(THD *thd, enum_var_type type) { DBUG_POP(); }
  uchar *value_ptr(THD *thd, enum_var_type type, LEX_STRING *b);
};
#endif /* DBUG_OFF */

#if defined(ENABLED_DEBUG_SYNC)
/* Debug Sync Facility. Implemented in debug_sync.cc. */
class sys_var_debug_sync :public sys_var_thd
{
public:
  sys_var_debug_sync(sys_var_chain *chain, const char *name_arg)
    :sys_var_thd(name_arg)
  { chain_sys_var(chain); }
  bool check(THD *thd, set_var *var);
  bool update(THD *thd, set_var *var);
  SHOW_TYPE show_type() { return SHOW_CHAR; }
  bool check_update_type(Item_result type) { return type != STRING_RESULT; }
  uchar *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base);
};
#endif /* defined(ENABLED_DEBUG_SYNC) */

/* some variables that require special handling */

class sys_var_timestamp :public sys_var
{
public:
  sys_var_timestamp(sys_var_chain *chain, const char *name_arg,
                    Binlog_status_enum binlog_status_arg= NOT_IN_BINLOG)
    :sys_var(name_arg, NULL, binlog_status_arg)
  { chain_sys_var(chain); }
  bool update(THD *thd, set_var *var);
  void set_default(THD *thd, enum_var_type type);
  bool check_type(enum_var_type type)    { return type == OPT_GLOBAL; }
  bool check_default(enum_var_type type) { return 0; }
  SHOW_TYPE show_type() { return SHOW_LONG; }
  uchar *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base);
};


class sys_var_last_insert_id :public sys_var
{
public:
  sys_var_last_insert_id(sys_var_chain *chain, const char *name_arg,
                         Binlog_status_enum binlog_status_arg= NOT_IN_BINLOG)
    :sys_var(name_arg, NULL, binlog_status_arg)
  { chain_sys_var(chain); }
  bool update(THD *thd, set_var *var);
  bool check_type(enum_var_type type) { return type == OPT_GLOBAL; }
  SHOW_TYPE show_type() { return SHOW_LONGLONG; }
  uchar *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base);
};


class sys_var_insert_id :public sys_var
{
public:
  sys_var_insert_id(sys_var_chain *chain, const char *name_arg)
    :sys_var(name_arg)
  { chain_sys_var(chain); }
  bool update(THD *thd, set_var *var);
  bool check_type(enum_var_type type) { return type == OPT_GLOBAL; }
  SHOW_TYPE show_type() { return SHOW_LONGLONG; }
  uchar *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base);
};


class sys_var_rand_seed1 :public sys_var
{
public:
  sys_var_rand_seed1(sys_var_chain *chain, const char *name_arg,
                     Binlog_status_enum binlog_status_arg= NOT_IN_BINLOG)
    :sys_var(name_arg, NULL, binlog_status_arg)
  { chain_sys_var(chain); }
  bool update(THD *thd, set_var *var);
  bool check_type(enum_var_type type) { return type == OPT_GLOBAL; }
};

class sys_var_rand_seed2 :public sys_var
{
public:
  sys_var_rand_seed2(sys_var_chain *chain, const char *name_arg,
                     Binlog_status_enum binlog_status_arg= NOT_IN_BINLOG)
    :sys_var(name_arg, NULL, binlog_status_arg)
  { chain_sys_var(chain); }
  bool update(THD *thd, set_var *var);
  bool check_type(enum_var_type type) { return type == OPT_GLOBAL; }
};


class sys_var_collation :public sys_var_thd
{
public:
  sys_var_collation(const char *name_arg,
                    Binlog_status_enum binlog_status_arg= NOT_IN_BINLOG)
    :sys_var_thd(name_arg, NULL, binlog_status_arg)
  {
    no_support_one_shot= 0;
  }
  bool check(THD *thd, set_var *var);
  SHOW_TYPE show_type() { return SHOW_CHAR; }
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
  sys_var_character_set(const char *name_arg, bool is_nullable= 0,
                        Binlog_status_enum binlog_status_arg= NOT_IN_BINLOG)
    :sys_var_thd(name_arg, NULL, binlog_status_arg), nullable(is_nullable)
  {
    /*
      In fact only almost all variables derived from sys_var_character_set
      support ONE_SHOT; character_set_results doesn't. But that's good enough.
    */
    no_support_one_shot= 0;
  }
  bool check(THD *thd, set_var *var);
  SHOW_TYPE show_type() { return SHOW_CHAR; }
  bool check_update_type(Item_result type)
  {
    return ((type != STRING_RESULT) && (type != INT_RESULT));
  }
  bool check_default(enum_var_type type) { return 0; }
  bool update(THD *thd, set_var *var);
  uchar *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base);
  virtual void set_default(THD *thd, enum_var_type type)= 0;
  virtual CHARSET_INFO **ci_ptr(THD *thd, enum_var_type type)= 0;
};

class sys_var_character_set_sv :public sys_var_character_set
{
  CHARSET_INFO *SV::*offset;
  CHARSET_INFO **global_default;
public:
  sys_var_character_set_sv(sys_var_chain *chain, const char *name_arg,
			   CHARSET_INFO *SV::*offset_arg,
			   CHARSET_INFO **global_default_arg,
                           bool is_nullable= 0,
                           Binlog_status_enum binlog_status_arg= NOT_IN_BINLOG)
    : sys_var_character_set(name_arg, is_nullable, binlog_status_arg),
    offset(offset_arg), global_default(global_default_arg)
  { chain_sys_var(chain); }
  void set_default(THD *thd, enum_var_type type);
  CHARSET_INFO **ci_ptr(THD *thd, enum_var_type type);
};


class sys_var_character_set_client: public sys_var_character_set_sv
{
public:
  sys_var_character_set_client(sys_var_chain *chain, const char *name_arg,
                               CHARSET_INFO *SV::*offset_arg,
                               CHARSET_INFO **global_default_arg,
                               Binlog_status_enum binlog_status_arg)
    : sys_var_character_set_sv(chain, name_arg, offset_arg, global_default_arg,
                               0, binlog_status_arg)
  { }
  bool check(THD *thd, set_var *var);
};


class sys_var_character_set_database :public sys_var_character_set
{
public:
  sys_var_character_set_database(sys_var_chain *chain, const char *name_arg,
                                 Binlog_status_enum binlog_status_arg=
                                   NOT_IN_BINLOG)
    : sys_var_character_set(name_arg, 0, binlog_status_arg)
  { chain_sys_var(chain); }
  void set_default(THD *thd, enum_var_type type);
  CHARSET_INFO **ci_ptr(THD *thd, enum_var_type type);
};

class sys_var_collation_sv :public sys_var_collation
{
  CHARSET_INFO *SV::*offset;
  CHARSET_INFO **global_default;
public:
  sys_var_collation_sv(sys_var_chain *chain, const char *name_arg,
		       CHARSET_INFO *SV::*offset_arg,
                       CHARSET_INFO **global_default_arg,
                       Binlog_status_enum binlog_status_arg= NOT_IN_BINLOG)
    :sys_var_collation(name_arg, binlog_status_arg),
    offset(offset_arg), global_default(global_default_arg)
  {
    chain_sys_var(chain);
  }
  bool update(THD *thd, set_var *var);
  void set_default(THD *thd, enum_var_type type);
  uchar *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base);
};


class sys_var_key_cache_param :public sys_var
{
protected:
  size_t offset;
public:
  sys_var_key_cache_param(sys_var_chain *chain, const char *name_arg, 
                          size_t offset_arg)
    :sys_var(name_arg), offset(offset_arg)
  { chain_sys_var(chain); }
  uchar *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base);
  bool check_default(enum_var_type type) { return 1; }
  bool is_struct() { return 1; }
};


class sys_var_key_buffer_size :public sys_var_key_cache_param
{
public:
  sys_var_key_buffer_size(sys_var_chain *chain, const char *name_arg)
    :sys_var_key_cache_param(chain, name_arg,
                             offsetof(KEY_CACHE, param_buff_size))
  {}
  bool update(THD *thd, set_var *var);
  SHOW_TYPE show_type() { return SHOW_LONGLONG; }
};


class sys_var_key_cache_long :public sys_var_key_cache_param
{
public:
  sys_var_key_cache_long(sys_var_chain *chain, const char *name_arg, size_t offset_arg)
    :sys_var_key_cache_param(chain, name_arg, offset_arg)
  {}
  bool update(THD *thd, set_var *var);
  SHOW_TYPE show_type() { return SHOW_LONG; }
};


class sys_var_thd_date_time_format :public sys_var_thd
{
  DATE_TIME_FORMAT *SV::*offset;
  timestamp_type date_time_type;
public:
  sys_var_thd_date_time_format(sys_var_chain *chain, const char *name_arg,
			       DATE_TIME_FORMAT *SV::*offset_arg,
			       timestamp_type date_time_type_arg)
    :sys_var_thd(name_arg), offset(offset_arg),
    date_time_type(date_time_type_arg)
  { chain_sys_var(chain); }
  SHOW_TYPE show_type() { return SHOW_CHAR; }
  bool check_update_type(Item_result type)
  {
    return type != STRING_RESULT;		/* Only accept strings */
  }
  bool check_default(enum_var_type type) { return 0; }
  bool check(THD *thd, set_var *var);
  bool update(THD *thd, set_var *var);
  void update2(THD *thd, enum_var_type type, DATE_TIME_FORMAT *new_value);
  uchar *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base);
  void set_default(THD *thd, enum_var_type type);
};


class sys_var_log_state :public sys_var_bool_ptr
{
  uint log_type;
public:
  sys_var_log_state(sys_var_chain *chain, const char *name_arg, my_bool *value_arg, 
                    uint log_type_arg)
    :sys_var_bool_ptr(chain, name_arg, value_arg), log_type(log_type_arg) {}
  bool update(THD *thd, set_var *var);
  void set_default(THD *thd, enum_var_type type);
};


class sys_var_set :public sys_var
{
protected:
  ulong *value;
  TYPELIB *enum_names;
public:
  sys_var_set(sys_var_chain *chain, const char *name_arg, ulong *value_arg,
              TYPELIB *typelib, sys_after_update_func func)
    :sys_var(name_arg, func), value(value_arg), enum_names(typelib)
  { chain_sys_var(chain); }
  virtual bool check(THD *thd, set_var *var)
  {
    return check_set(thd, var, enum_names);
  }
  virtual void set_default(THD *thd, enum_var_type type)
  {  
    *value= 0;
  }
  bool update(THD *thd, set_var *var);
  uchar *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base);
  bool check_update_type(Item_result type) { return 0; }
  SHOW_TYPE show_type() { return SHOW_CHAR; }
};

class sys_var_set_slave_mode :public sys_var_set
{
public:
  sys_var_set_slave_mode(sys_var_chain *chain, const char *name_arg,
                         ulong *value_arg,
                         TYPELIB *typelib, sys_after_update_func func) :
    sys_var_set(chain, name_arg, value_arg, typelib, func) {}
  void set_default(THD *thd, enum_var_type type);
  bool check(THD *thd, set_var *var);
  bool update(THD *thd, set_var *var);
};

class sys_var_log_output :public sys_var
{
  ulong *value;
  TYPELIB *enum_names;
public:
  sys_var_log_output(sys_var_chain *chain, const char *name_arg, ulong *value_arg,
                     TYPELIB *typelib, sys_after_update_func func)
    :sys_var(name_arg,func), value(value_arg), enum_names(typelib)
  {
    chain_sys_var(chain);
    set_allow_empty_value(FALSE);
  }
  virtual bool check(THD *thd, set_var *var)
  {
    return check_set(thd, var, enum_names);
  }
  bool update(THD *thd, set_var *var);
  uchar *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base);
  bool check_update_type(Item_result type) { return 0; }
  void set_default(THD *thd, enum_var_type type);
  SHOW_TYPE show_type() { return SHOW_CHAR; }
};


/* Variable that you can only read from */

class sys_var_readonly: public sys_var
{
public:
  enum_var_type var_type;
  SHOW_TYPE show_type_value;
  sys_value_ptr_func value_ptr_func;
  sys_var_readonly(sys_var_chain *chain, const char *name_arg, enum_var_type type,
		   SHOW_TYPE show_type_arg,
		   sys_value_ptr_func value_ptr_func_arg)
    :sys_var(name_arg), var_type(type), 
       show_type_value(show_type_arg), value_ptr_func(value_ptr_func_arg)
  { chain_sys_var(chain); }
  bool update(THD *thd, set_var *var) { return 1; }
  bool check_default(enum_var_type type) { return 1; }
  bool check_type(enum_var_type type) { return type != var_type; }
  bool check_update_type(Item_result type) { return 1; }
  uchar *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base)
  {
    return (*value_ptr_func)(thd);
  }
  SHOW_TYPE show_type() { return show_type_value; }
  bool is_readonly() const { return 1; }
};


class sys_var_readonly_os: public sys_var_readonly
{
public:
  sys_var_readonly_os(sys_var_chain *chain, const char *name_arg, enum_var_type type,
		   SHOW_TYPE show_type_arg,
		   sys_value_ptr_func value_ptr_func_arg)
    :sys_var_readonly(chain, name_arg, type, show_type_arg, value_ptr_func_arg)
  {
    is_os_charset= TRUE;
  }
};


/**
  Global-only, read-only variable. E.g. command line option.
*/

class sys_var_const: public sys_var
{
public:
  enum_var_type var_type;
  SHOW_TYPE show_type_value;
  uchar *ptr;
  sys_var_const(sys_var_chain *chain, const char *name_arg, enum_var_type type,
                SHOW_TYPE show_type_arg, uchar *ptr_arg)
    :sys_var(name_arg), var_type(type),
    show_type_value(show_type_arg), ptr(ptr_arg)
  { chain_sys_var(chain); }
  bool update(THD *thd, set_var *var) { return 1; }
  bool check_default(enum_var_type type) { return 1; }
  bool check_type(enum_var_type type) { return type != var_type; }
  bool check_update_type(Item_result type) { return 1; }
  uchar *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base)
  {
    return ptr;
  }
  SHOW_TYPE show_type() { return show_type_value; }
  bool is_readonly() const { return 1; }
};


class sys_var_const_os: public sys_var_const
{
public:
  enum_var_type var_type;
  SHOW_TYPE show_type_value;
  uchar *ptr;
  sys_var_const_os(sys_var_chain *chain, const char *name_arg, 
                   enum_var_type type,
                SHOW_TYPE show_type_arg, uchar *ptr_arg)
    :sys_var_const(chain, name_arg, type, show_type_arg, ptr_arg)
  {
    is_os_charset= TRUE;
  }
};


class sys_var_have_option: public sys_var
{
protected:
  virtual SHOW_COMP_OPTION get_option() = 0;
public:
  sys_var_have_option(sys_var_chain *chain, const char *variable_name):
    sys_var(variable_name)
  { chain_sys_var(chain); }
  uchar *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base)
  {
    return (uchar*) show_comp_option_name[get_option()];
  }
  bool update(THD *thd, set_var *var) { return 1; }
  bool check_default(enum_var_type type) { return 1; }
  bool check_type(enum_var_type type) { return type != OPT_GLOBAL; }
  bool check_update_type(Item_result type) { return 1; }
  SHOW_TYPE show_type() { return SHOW_CHAR; }
  bool is_readonly() const { return 1; }
};


class sys_var_have_variable: public sys_var_have_option
{
  SHOW_COMP_OPTION *have_variable;

public:
  sys_var_have_variable(sys_var_chain *chain, const char *variable_name,
                        SHOW_COMP_OPTION *have_variable_arg):
    sys_var_have_option(chain, variable_name),
    have_variable(have_variable_arg)
  { }
  SHOW_COMP_OPTION get_option() { return *have_variable; }
};


class sys_var_have_plugin: public sys_var_have_option
{
  const char *plugin_name_str;
  const uint plugin_name_len;
  const int plugin_type;

public:
  sys_var_have_plugin(sys_var_chain *chain, const char *variable_name,
                      const char *plugin_name_str_arg, uint plugin_name_len_arg, 
                      int plugin_type_arg):
    sys_var_have_option(chain, variable_name), 
    plugin_name_str(plugin_name_str_arg), plugin_name_len(plugin_name_len_arg),
    plugin_type(plugin_type_arg)
  { }
  /* the following method is declared in sql_plugin.cc */
  SHOW_COMP_OPTION get_option();
};


class sys_var_thd_time_zone :public sys_var_thd
{
public:
  sys_var_thd_time_zone(sys_var_chain *chain, const char *name_arg,
                        Binlog_status_enum binlog_status_arg= NOT_IN_BINLOG)
    :sys_var_thd(name_arg, NULL, binlog_status_arg)
  {
    no_support_one_shot= 0;
    chain_sys_var(chain);
  }
  bool check(THD *thd, set_var *var);
  SHOW_TYPE show_type() { return SHOW_CHAR; }
  bool check_update_type(Item_result type)
  {
    return type != STRING_RESULT;		/* Only accept strings */
  }
  bool check_default(enum_var_type type) { return 0; }
  bool update(THD *thd, set_var *var);
  uchar *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base);
  virtual void set_default(THD *thd, enum_var_type type);
};


class sys_var_max_user_conn : public sys_var_thd
{
public:
  sys_var_max_user_conn(sys_var_chain *chain, const char *name_arg):
    sys_var_thd(name_arg)
  { chain_sys_var(chain); }
  bool check(THD *thd, set_var *var);
  bool update(THD *thd, set_var *var);
  bool check_default(enum_var_type type)
  {
    return type != OPT_GLOBAL || !option_limits;
  }
  void set_default(THD *thd, enum_var_type type);
  SHOW_TYPE show_type() { return SHOW_INT; }
  uchar *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base);
};


/**
 * @brief This is a specialization of sys_var_thd_ulong that implements a 
   read-only session variable. The class overrides check() and check_default() 
   to achieve the read-only property for the session part of the variable.
 */
class sys_var_thd_ulong_session_readonly : public sys_var_thd_ulong
{
public:
  sys_var_thd_ulong_session_readonly(sys_var_chain *chain_arg, 
                                     const char *name_arg, ulong SV::*offset_arg, 
				     sys_check_func c_func= NULL,
                                     sys_after_update_func au_func= NULL, 
                                     Binlog_status_enum bl_status_arg= NOT_IN_BINLOG):
    sys_var_thd_ulong(chain_arg, name_arg, offset_arg, c_func, au_func, bl_status_arg)
  { }
  bool check(THD *thd, set_var *var);
  bool check_default(enum_var_type type)
  {
    return type != OPT_GLOBAL || !option_limits;
  }
};


class sys_var_microseconds :public sys_var_thd
{
  ulonglong SV::*offset;
public:
  sys_var_microseconds(sys_var_chain *chain, const char *name_arg,
                       ulonglong SV::*offset_arg):
    sys_var_thd(name_arg), offset(offset_arg)
  { chain_sys_var(chain); }
  bool check(THD *thd, set_var *var) {return 0;}
  bool update(THD *thd, set_var *var);
  void set_default(THD *thd, enum_var_type type);
  SHOW_TYPE show_type() { return SHOW_DOUBLE; }
  bool check_update_type(Item_result type)
  {
    return (type != INT_RESULT && type != REAL_RESULT && type != DECIMAL_RESULT);
  }
  uchar *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base);
};


class sys_var_trust_routine_creators :public sys_var_bool_ptr
{
  /* We need a derived class only to have a warn_deprecated() */
public:
  sys_var_trust_routine_creators(sys_var_chain *chain,
                                 const char *name_arg, my_bool *value_arg) :
    sys_var_bool_ptr(chain, name_arg, value_arg) {};
  void warn_deprecated(THD *thd);
  void set_default(THD *thd, enum_var_type type);
  bool update(THD *thd, set_var *var);
};


/**
  Handler for setting the system variable --read-only.
*/

class sys_var_opt_readonly :public sys_var_bool_ptr
{
public:
  sys_var_opt_readonly(sys_var_chain *chain, const char *name_arg, 
                       my_bool *value_arg) :
    sys_var_bool_ptr(chain, name_arg, value_arg) {};
  ~sys_var_opt_readonly() {};
  bool update(THD *thd, set_var *var);
};


class sys_var_thd_lc_time_names :public sys_var_thd
{
public:
  sys_var_thd_lc_time_names(sys_var_chain *chain, const char *name_arg,
                            Binlog_status_enum binlog_status_arg= NOT_IN_BINLOG)
    : sys_var_thd(name_arg, NULL, binlog_status_arg)
  {
#if MYSQL_VERSION_ID < 50000
    no_support_one_shot= 0;
#endif
    chain_sys_var(chain);
  }
  bool check(THD *thd, set_var *var);
  SHOW_TYPE show_type() { return SHOW_CHAR; }
  bool check_update_type(Item_result type)
  {
    return ((type != STRING_RESULT) && (type != INT_RESULT));
  }
  bool check_default(enum_var_type type) { return 0; }
  bool update(THD *thd, set_var *var);
  uchar *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base);
  virtual void set_default(THD *thd, enum_var_type type);
};

#ifdef HAVE_EVENT_SCHEDULER
class sys_var_event_scheduler :public sys_var_long_ptr
{
  /* We need a derived class only to have a warn_deprecated() */
public:
  sys_var_event_scheduler(sys_var_chain *chain, const char *name_arg) :
    sys_var_long_ptr(chain, name_arg, NULL, NULL) {};
  bool update(THD *thd, set_var *var);
  uchar *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base);
  SHOW_TYPE show_type() { return SHOW_CHAR; }
  bool check(THD *thd, set_var *var);
  bool check_update_type(Item_result type)
  {
    return type != STRING_RESULT && type != INT_RESULT;
  }
};
#endif

extern void fix_binlog_format_after_update(THD *thd, enum_var_type type);

class sys_var_thd_binlog_format :public sys_var_thd_enum
{
public:
  sys_var_thd_binlog_format(sys_var_chain *chain, const char *name_arg, 
                            ulong SV::*offset_arg)
    :sys_var_thd_enum(chain, name_arg, offset_arg,
                      &binlog_format_typelib,
                      fix_binlog_format_after_update)
  {};
  bool check(THD *thd, set_var *var);
  bool is_readonly() const;
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
    plugin_ref plugin;
    DATE_TIME_FORMAT *date_time_format;
    Time_zone *time_zone;
    MY_LOCALE *locale_value;
  } save_result;
  LEX_STRING base;			/* for structs */

  set_var(enum_var_type type_arg, sys_var *var_arg,
          const LEX_STRING *base_name_arg, Item *value_arg)
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


extern "C"
{
  typedef int (*process_key_cache_t) (const char *, KEY_CACHE *);
}

/* Named lists (used for keycaches) */

class NAMED_LIST :public ilink
{
  const char *name;
  uint name_length;
public:
  uchar* data;

  NAMED_LIST(I_List<NAMED_LIST> *links, const char *name_arg,
	     uint name_length_arg, uchar* data_arg)
    :name_length(name_length_arg), data(data_arg)
  {
    name= my_strndup(name_arg, name_length, MYF(MY_WME));
    links->push_back(this);
  }
  inline bool cmp(const char *name_cmp, uint length)
  {
    return length == name_length && !memcmp(name, name_cmp, length);
  }
  ~NAMED_LIST()
  {
    my_free((uchar*) name, MYF(0));
  }
  friend bool process_key_caches(process_key_cache_t func);
  friend void delete_elements(I_List<NAMED_LIST> *list,
			      void (*free_element)(const char*, uchar*));
};

/* updated in sql_acl.cc */

extern sys_var_thd_bool sys_old_alter_table;
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

int set_var_init();
void set_var_free();
SHOW_VAR* enumerate_sys_vars(THD *thd, bool sorted);
int mysql_add_sys_var_chain(sys_var *chain, struct my_option *long_options);
int mysql_del_sys_var_chain(sys_var *chain);
sys_var *find_sys_var(THD *thd, const char *str, uint length=0);
int sql_set_variables(THD *thd, List<set_var_base> *var_list);
bool not_all_support_one_shot(List<set_var_base> *var_list);
void fix_delay_key_write(THD *thd, enum_var_type type);
void fix_slave_exec_mode(enum_var_type type);
ulong fix_sql_mode(ulong sql_mode);
extern sys_var_const_str sys_charset_system;
extern sys_var_str sys_init_connect;
extern sys_var_str sys_init_slave;
extern sys_var_thd_time_zone sys_time_zone;
extern sys_var_thd_bit sys_autocommit;
CHARSET_INFO *get_old_charset_by_name(const char *old_name);
uchar* find_named(I_List<NAMED_LIST> *list, const char *name, uint length,
		NAMED_LIST **found);

extern sys_var_str sys_var_general_log_path, sys_var_slow_log_path;

/* key_cache functions */
KEY_CACHE *get_key_cache(LEX_STRING *cache_name);
KEY_CACHE *get_or_create_key_cache(const char *name, uint length);
void free_key_cache(const char *name, KEY_CACHE *key_cache);
bool process_key_caches(process_key_cache_t func);
void delete_elements(I_List<NAMED_LIST> *list,
		     void (*free_element)(const char*, uchar*));
