#ifndef SET_VAR_INCLUDED
#define SET_VAR_INCLUDED
/* Copyright (C) 2000-2008 MySQL AB, 2008-2010 Sun Microsystems, Inc.

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

/**
  @file
  "public" interface to sys_var - server configuration variables.
*/

#ifdef USE_PRAGMA_INTERFACE
#pragma interface                       /* gcc class implementation */
#endif

#include <my_getopt.h>

class sys_var;
class set_var;
class sys_var_pluginvar;
class PolyLock;

extern TYPELIB bool_typelib;

struct sys_var_chain
{
  sys_var *first;
  sys_var *last;
};

int mysql_add_sys_var_chain(sys_var *chain);
int mysql_del_sys_var_chain(sys_var *chain);

/**
  A class representing one system variable - that is something
  that can be accessed as @@global.variable_name or @@session.variable_name,
  visible in SHOW xxx VARIABLES and in INFORMATION_SCHEMA.xxx_VARIABLES,
  optionally it can be assigned to, optionally it can have a command-line
  counterpart with the same name.
*/
class sys_var
{
public:
  sys_var *next;
  LEX_CSTRING name;
  enum flag_enum { GLOBAL, SESSION, ONLY_SESSION, SCOPE_MASK=1023,
                   READONLY=1024, ALLOCATED=2048 };
  static const int PARSE_EARLY= 1;
  static const int PARSE_NORMAL= 2;
  /**
    Enumeration type to indicate for a system variable whether
    it will be written to the binlog or not.
  */    
  enum binlog_status_enum { VARIABLE_NOT_IN_BINLOG,
                            SESSION_VARIABLE_IN_BINLOG } binlog_status;

protected:
  typedef bool (*on_check_function)(sys_var *self, THD *thd, set_var *var);
  typedef bool (*on_update_function)(sys_var *self, THD *thd, enum_var_type type);

  int flags;            ///< or'ed flag_enum values
  int m_parse_flag;     ///< either PARSE_EARLY or PARSE_NORMAL.
  const SHOW_TYPE show_val_type; ///< what value_ptr() returns for sql_show.cc
  my_option option;     ///< min, max, default values are stored here
  PolyLock *guard;      ///< *second* lock that protects the variable
  ptrdiff_t offset;     ///< offset to the value from global_system_variables
  on_check_function on_check;
  on_update_function on_update;
  struct { uint version; const char *substitute; } deprecated;
  bool is_os_charset; ///< true if the value is in character_set_filesystem

public:
  sys_var(sys_var_chain *chain, const char *name_arg, const char *comment,
          int flag_args, ptrdiff_t off, int getopt_id,
          enum get_opt_arg_type getopt_arg_type, SHOW_TYPE show_val_type_arg,
          longlong def_val, PolyLock *lock, enum binlog_status_enum binlog_status_arg,
          on_check_function on_check_func, on_update_function on_update_func,
          uint deprecated_version, const char *substitute, int parse_flag);
  /**
    The instance should only be destroyed on shutdown, as it doesn't unlink
    itself from the chain.
  */
  virtual ~sys_var() {}
  /**
    downcast for sys_var_pluginvar. Returns this if it's an instance
    of sys_var_pluginvar, and 0 otherwise.
  */
  virtual sys_var_pluginvar *cast_pluginvar() { return 0; }

  bool check(THD *thd, set_var *var);
  uchar *value_ptr(THD *thd, enum_var_type type, LEX_STRING *base);
  bool set_default(THD *thd, enum_var_type type);
  bool update(THD *thd, set_var *var);

  SHOW_TYPE show_type() { return show_val_type; }
  int scope() const { return flags & SCOPE_MASK; }
  CHARSET_INFO *charset(THD *thd);
  bool is_readonly() const { return flags & READONLY; }
  /**
    the following is only true for keycache variables,
    that support the syntax @@keycache_name.variable_name
  */
  bool is_struct() { return option.var_type & GET_ASK_ADDR; }
  bool is_written_to_binlog(enum_var_type type)
  { return type != OPT_GLOBAL && binlog_status == SESSION_VARIABLE_IN_BINLOG; }
  virtual bool check_update_type(Item_result type) = 0;
  bool check_type(enum_var_type type)
  {
    switch (scope())
    {
    case GLOBAL:       return type != OPT_GLOBAL;
    case SESSION:      return false; // always ok
    case ONLY_SESSION: return type == OPT_GLOBAL;
    }
    return true; // keep gcc happy
  }
  bool register_option(DYNAMIC_ARRAY *array, int parse_flags)
  {
    return (option.id != -1) && (m_parse_flag & parse_flags) &&
           insert_dynamic(array, (uchar*)&option);
  }

private:
  virtual bool do_check(THD *thd, set_var *var) = 0;
  /**
    save the session default value of the variable in var
  */
  virtual void session_save_default(THD *thd, set_var *var) = 0;
  /**
    save the global default value of the variable in var
  */
  virtual void global_save_default(THD *thd, set_var *var) = 0;
  virtual bool session_update(THD *thd, set_var *var) = 0;
  virtual bool global_update(THD *thd, set_var *var) = 0;
  void do_deprecated_warning(THD *thd);
protected:
  /**
    A pointer to a value of the variable for SHOW.
    It must be of show_val_type type (bool for SHOW_BOOL, int for SHOW_INT,
    longlong for SHOW_LONGLONG, etc).
  */
  virtual uchar *session_value_ptr(THD *thd, LEX_STRING *base);
  virtual uchar *global_value_ptr(THD *thd, LEX_STRING *base);

  /**
    A pointer to a storage area of the variable, to the raw data.
    Typically it's the same as session_value_ptr(), but it's different,
    for example, for ENUM, that is printed as a string, but stored as a number.
  */
  uchar *session_var_ptr(THD *thd)
  { return ((uchar*)&(thd->variables)) + offset; }

  uchar *global_var_ptr()
  { return ((uchar*)&global_system_variables) + offset; }
};

/****************************************************************************
  Classes for parsing of the SET command
****************************************************************************/

/**
  A base class for everything that can be set with SET command.
  It's similar to Items, an instance of this is created by the parser
  for every assigmnent in SET (or elsewhere, e.g. in SELECT).
*/
class set_var_base :public Sql_alloc
{
public:
  set_var_base() {}
  virtual ~set_var_base() {}
  virtual int check(THD *thd)=0;           /* To check privileges etc. */
  virtual int update(THD *thd)=0;                  /* To set the value */
  virtual int light_check(THD *thd) { return check(thd); }   /* for PS */
};


/**
  set_var_base descendant for assignments to the system variables.
*/
class set_var :public set_var_base
{
public:
  sys_var *var; ///< system variable to be updated
  Item *value;  ///< the expression that provides the new value of the variable
  enum_var_type type;
  union ///< temp storage to hold a value between sys_var::check and ::update
  {
    ulonglong ulonglong_value;          ///< for all integer, set, enum sysvars
    double double_value;                ///< for Sys_var_double
    plugin_ref plugin;                  ///< for Sys_var_plugin
    Time_zone *time_zone;               ///< for Sys_var_tz
    LEX_STRING string_value;            ///< for Sys_var_charptr and others
    void *ptr;                          ///< for Sys_var_struct
  } save_result;
  LEX_STRING base; /**< for structured variables, like keycache_name.variable_name */

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
                                  system_charset_info))) // names are utf8
        value=value_arg;                        /* Give error message later */
    }
    else
      value=value_arg;
  }
  int check(THD *thd);
  int update(THD *thd);
  int light_check(THD *thd);
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

/*
  Prototypes for helper functions
*/

SHOW_VAR* enumerate_sys_vars(THD *thd, bool sorted, enum enum_var_type type);

sys_var *find_sys_var(THD *thd, const char *str, uint length=0);
int sql_set_variables(THD *thd, List<set_var_base> *var_list);

bool fix_delay_key_write(sys_var *self, THD *thd, enum_var_type type);

ulong expand_sql_mode(ulonglong sql_mode);
bool sql_mode_string_representation(THD *thd, ulong sql_mode, LEX_STRING *ls);

extern sys_var *Sys_autocommit_ptr;

CHARSET_INFO *get_old_charset_by_name(const char *old_name);

int sys_var_init();
int sys_var_add_options(DYNAMIC_ARRAY *long_options, int parse_flags);
void sys_var_end(void);

#endif

