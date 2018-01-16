#ifndef SYS_VARS_H_INCLUDED
#define SYS_VARS_H_INCLUDED
/* Copyright (c) 2002, 2018, Oracle and/or its affiliates. All rights reserved.

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

/**
  @file
  "private" interface to sys_var - server configuration variables.

  This header is included only by the file that contains declarations
  of sys_var variables (sys_vars.cc).
*/

#include "my_global.h"
#include "keycaches.h"            // dflt_key_cache
#include "my_bit.h"               // my_count_bits
#include "my_getopt.h"            // get_opt_arg_type
#include "mysql/plugin.h"         // enum_mysql_show_type
#include "item.h"                 // Item
#include "log.h"                  // sql_print_information
#include "set_var.h"              // sys_var
#include "sql_class.h"            // THD
#include "sql_plugin.h"           // my_plugin_lock_by_name
#include "strfunc.h"              // find_type
#include "sys_vars_shared.h"      // throw_bounds_warning
#include "tztime.h"               // Time_zone
#include "binlog.h"               // mysql_bin_log
#include "rpl_rli.h"              // sql_slave_skip_counter
#include "rpl_msr.h"              // channel_map
#include "rpl_group_replication.h"// is_group_replication_running


/*
  a set of mostly trivial (as in f(X)=X) defines below to make system variable
  declarations more readable
*/
#define VALID_RANGE(X,Y) X,Y
#define DEFAULT(X) X
#define BLOCK_SIZE(X) X
#define GLOBAL_VAR(X) sys_var::GLOBAL, (((char*)&(X))-(char*)&global_system_variables), sizeof(X)
#define SESSION_VAR(X) sys_var::SESSION, offsetof(SV, X), sizeof(((SV *)0)->X)
#define SESSION_ONLY(X) sys_var::ONLY_SESSION, offsetof(SV, X), sizeof(((SV *)0)->X)
#define NO_CMD_LINE CMD_LINE(NO_ARG, -1)
/*
  the define below means that there's no *second* mutex guard,
  LOCK_global_system_variables always guards all system variables
*/
#define NO_MUTEX_GUARD ((PolyLock*)0)
#define IN_BINLOG sys_var::SESSION_VARIABLE_IN_BINLOG
#define NOT_IN_BINLOG sys_var::VARIABLE_NOT_IN_BINLOG
#define ON_READ(X) X
#define ON_CHECK(X) X
#define ON_UPDATE(X) X
#define READ_ONLY sys_var::READONLY+
#define NOT_VISIBLE sys_var::INVISIBLE+
#define UNTRACKED_DEFAULT sys_var::TRI_LEVEL+
// this means that Sys_var_charptr initial value was malloc()ed
#define PREALLOCATED sys_var::ALLOCATED+
/*
  Sys_var_bit meaning is reversed, like in
  @@foreign_key_checks <-> OPTION_NO_FOREIGN_KEY_CHECKS
*/
#define REVERSE(X) ~(X)
#define DEPRECATED(X) X

#define session_var(THD, TYPE) (*(TYPE*)session_var_ptr(THD))
#define global_var(TYPE) (*(TYPE*)global_var_ptr())

#if SIZEOF_OFF_T > 4
#define GET_HA_ROWS GET_ULL
#else
#define GET_HA_ROWS GET_ULONG
#endif

extern sys_var_chain all_sys_vars;

enum charset_enum {IN_SYSTEM_CHARSET, IN_FS_CHARSET};

static const char *bool_values[3]= {"OFF", "ON", 0};

/**
  A small wrapper class to pass getopt arguments as a pair
  to the Sys_var_* constructors. It improves type safety and helps
  to catch errors in the argument order.
*/
struct CMD_LINE
{
  int id;
  enum get_opt_arg_type arg_type;
  CMD_LINE(enum get_opt_arg_type getopt_arg_type, int getopt_id=0)
    : id(getopt_id), arg_type(getopt_arg_type) {}
};

/**
  Sys_var_integer template is used to generate Sys_var_* classes
  for variables that represent the value as a signed or unsigned integer.
  They are Sys_var_uint, Sys_var_ulong, Sys_var_harows, Sys_var_ulonglong,
  and Sys_var_long.

  An integer variable has a minimal and maximal values, and a "block_size"
  (any valid value of the variable must be divisible by the block_size).

  Class specific constructor arguments: min, max, block_size
  Backing store: uint, ulong, ha_rows, ulonglong, long, depending on the
  Sys_var_*
*/
template
  <typename T, ulong ARGT, enum enum_mysql_show_type SHOWT, bool SIGNED>
class Sys_var_integer: public sys_var
{
public:
  Sys_var_integer(const char *name_arg,
          const char *comment, int flag_args, ptrdiff_t off, size_t size,
          CMD_LINE getopt,
          T min_val, T max_val, T def_val, uint block_size, PolyLock *lock=0,
          enum binlog_status_enum binlog_status_arg=VARIABLE_NOT_IN_BINLOG,
          on_check_function on_check_func=0,
          on_update_function on_update_func=0,
          const char *substitute=0,
          int parse_flag= PARSE_NORMAL)
    : sys_var(&all_sys_vars, name_arg, comment, flag_args, off, getopt.id,
              getopt.arg_type, SHOWT, def_val, lock, binlog_status_arg,
              on_check_func, on_update_func,
              substitute, parse_flag)
  {
    option.var_type= ARGT;
    option.min_value= min_val;
    option.max_value= max_val;
    option.block_size= block_size;
    option.u_max_value= (uchar**)max_var_ptr();
    if (max_var_ptr())
      *max_var_ptr()= max_val;

    // Do not set global_var for Sys_var_keycache objects
    if (offset >= 0)
      global_var(T)= def_val;

    DBUG_ASSERT(size == sizeof(T));
    DBUG_ASSERT(min_val < max_val);
    DBUG_ASSERT(min_val <= def_val);
    DBUG_ASSERT(max_val >= def_val);
    DBUG_ASSERT(block_size > 0);
    DBUG_ASSERT(def_val % block_size == 0);
  }
  bool do_check(THD *thd, set_var *var)
  {
    my_bool fixed= FALSE;
    longlong v;
    ulonglong uv;

    v= var->value->val_int();
    if (SIGNED) /* target variable has signed type */
    {
      if (var->value->unsigned_flag)
      {
        /*
          Input value is such a large positive number that MySQL used an
          unsigned item to hold it. When cast to a signed longlong, if the
          result is negative there is "cycling" and this is incorrect (large
          positive input value should not end up as a large negative value in
          the session signed variable to be set); instead, we need to pick the
          allowed number closest to the positive input value, i.e. pick the
          biggest allowed positive integer.
        */
        if (v < 0)
          uv= max_of_int_range(ARGT);
        else /* no cycling, longlong can hold true value */
          uv= (ulonglong) v;
      }
      else
        uv= v;
      /* This will further restrict with VALID_RANGE, BLOCK_SIZE */
      var->save_result.ulonglong_value=
        getopt_ll_limit_value(uv, &option, &fixed);
    }
    else
    {
      if (var->value->unsigned_flag)
      {
        /* Guaranteed positive input value, ulonglong can hold it */
        uv= (ulonglong) v;
      }
      else
      {
        /*
          Maybe negative input value; in this case, cast to ulonglong makes it
          positive, which is wrong. Pick the closest allowed value i.e. 0.
        */
        uv= (ulonglong) (v < 0 ? 0 : v);
      }
      var->save_result.ulonglong_value=
        getopt_ull_limit_value(uv, &option, &fixed);
    }

    if (max_var_ptr())
    {
      /* check constraint set with --maximum-...=X */
      if (SIGNED)
      {
        longlong max_val= *max_var_ptr();
        if (((longlong)(var->save_result.ulonglong_value)) > max_val)
          var->save_result.ulonglong_value= max_val;
        /*
          Signed variable probably has some kind of symmetry. Then it's good
          to limit negative values just as we limit positive values.
        */
        max_val= -max_val;
        if (((longlong)(var->save_result.ulonglong_value)) < max_val)
          var->save_result.ulonglong_value= max_val;
      }
      else
      {
        ulonglong max_val= *max_var_ptr();
        if (var->save_result.ulonglong_value > max_val)
          var->save_result.ulonglong_value= max_val;
      }
    }

    return throw_bounds_warning(thd, name.str,
                                var->save_result.ulonglong_value !=
                                (ulonglong)v,
                                var->value->unsigned_flag, v);
  }
  bool session_update(THD *thd, set_var *var)
  {
    session_var(thd, T)= static_cast<T>(var->save_result.ulonglong_value);
    return false;
  }
  bool global_update(THD *thd, set_var *var)
  {
    global_var(T)= static_cast<T>(var->save_result.ulonglong_value);
    return false;
  }
  bool check_update_type(Item_result type)
  { return type != INT_RESULT; }
  void session_save_default(THD *thd, set_var *var)
  {
    var->save_result.ulonglong_value=
      static_cast<ulonglong>(*(T*)global_value_ptr(thd, 0));
  }
  void global_save_default(THD *thd, set_var *var)
  { var->save_result.ulonglong_value= option.def_value; }
  private:
  T *max_var_ptr()
  {
    return scope() == SESSION ? (T*)(((uchar*)&max_system_variables) + offset)
                              : 0;
  }
};

typedef Sys_var_integer<int32, GET_UINT, SHOW_INT, FALSE> Sys_var_int32;
typedef Sys_var_integer<uint, GET_UINT, SHOW_INT, FALSE> Sys_var_uint;
typedef Sys_var_integer<ulong, GET_ULONG, SHOW_LONG, FALSE> Sys_var_ulong;
typedef Sys_var_integer<ha_rows, GET_HA_ROWS, SHOW_HA_ROWS, FALSE>
  Sys_var_harows;
typedef Sys_var_integer<ulonglong, GET_ULL, SHOW_LONGLONG, FALSE>
  Sys_var_ulonglong;
typedef Sys_var_integer<long, GET_LONG, SHOW_SIGNED_LONG, TRUE> Sys_var_long;

/**
  Helper class for variables that take values from a TYPELIB
*/
class Sys_var_typelib: public sys_var
{
protected:
  TYPELIB typelib;
public:
  Sys_var_typelib(const char *name_arg,
          const char *comment, int flag_args, ptrdiff_t off,
          CMD_LINE getopt,
          SHOW_TYPE show_val_type_arg, const char *values[],
          ulonglong def_val, PolyLock *lock,
          enum binlog_status_enum binlog_status_arg,
          on_check_function on_check_func, on_update_function on_update_func,
          const char *substitute, int parse_flag= PARSE_NORMAL)
    : sys_var(&all_sys_vars, name_arg, comment, flag_args, off, getopt.id,
              getopt.arg_type, show_val_type_arg, def_val, lock,
              binlog_status_arg, on_check_func,
              on_update_func, substitute, parse_flag)
  {
    for (typelib.count= 0; values[typelib.count]; typelib.count++) /*no-op */;
    typelib.name="";
    typelib.type_names= values;
    typelib.type_lengths= 0;    // only used by Fields_enum and Field_set
    option.typelib= &typelib;
  }
  bool do_check(THD *thd, set_var *var) // works for enums and my_bool
  {
    char buff[STRING_BUFFER_USUAL_SIZE];
    String str(buff, sizeof(buff), system_charset_info), *res;

    if (var->value->result_type() == STRING_RESULT)
    {
      if (!(res=var->value->val_str(&str)))
        return true;
      else
      if (!(var->save_result.ulonglong_value=
            find_type(&typelib, res->ptr(), res->length(), false)))
        return true;
      else
        var->save_result.ulonglong_value--;
    }
    else
    {
      longlong tmp=var->value->val_int();
      if (tmp < 0 || tmp >= typelib.count)
        return true;
      else
        var->save_result.ulonglong_value= tmp;
    }

    return false;
  }
  bool check_update_type(Item_result type)
  { return type != INT_RESULT && type != STRING_RESULT; }
};

/**
  The class for ENUM variables - variables that take one value from a fixed
  list of values.

  Class specific constructor arguments:
    char* values[]    - 0-terminated list of strings of valid values

  Backing store: uint

  @note
  Do *not* use "enum FOO" variables as a backing store, there is no
  guarantee that sizeof(enum FOO) == sizeof(uint), there is no guarantee
  even that sizeof(enum FOO) == sizeof(enum BAR)
*/
class Sys_var_enum: public Sys_var_typelib
{
public:
  Sys_var_enum(const char *name_arg,
          const char *comment, int flag_args, ptrdiff_t off, size_t size,
          CMD_LINE getopt,
          const char *values[], uint def_val, PolyLock *lock=0,
          enum binlog_status_enum binlog_status_arg=VARIABLE_NOT_IN_BINLOG,
          on_check_function on_check_func=0,
          on_update_function on_update_func=0,
          const char *substitute=0)
    : Sys_var_typelib(name_arg, comment, flag_args, off, getopt,
                      SHOW_CHAR, values, def_val, lock,
                      binlog_status_arg, on_check_func, on_update_func,
                      substitute)
  {
    option.var_type= GET_ENUM;
    global_var(ulong)= def_val;
    DBUG_ASSERT(def_val < typelib.count);
    DBUG_ASSERT(size == sizeof(ulong));
  }
  bool session_update(THD *thd, set_var *var)
  {
    session_var(thd, ulong)=
      static_cast<ulong>(var->save_result.ulonglong_value);
    return false;
  }
  bool global_update(THD *thd, set_var *var)
  {
    global_var(ulong)=
      static_cast<ulong>(var->save_result.ulonglong_value);
    return false;
  }
  void session_save_default(THD *thd, set_var *var)
  { var->save_result.ulonglong_value= global_var(ulong); }
  void global_save_default(THD *thd, set_var *var)
  { var->save_result.ulonglong_value= option.def_value; }
  uchar *session_value_ptr(THD *running_thd, THD *target_thd, LEX_STRING *base)
  { return (uchar*)typelib.type_names[session_var(target_thd, ulong)]; }
  uchar *global_value_ptr(THD *thd, LEX_STRING *base)
  { return (uchar*)typelib.type_names[global_var(ulong)]; }
};

/**
  The class for boolean variables - a variant of ENUM variables
  with the fixed list of values of { OFF , ON }

  Backing store: my_bool
*/
class Sys_var_mybool: public Sys_var_typelib
{
public:
  Sys_var_mybool(const char *name_arg,
          const char *comment, int flag_args, ptrdiff_t off, size_t size,
          CMD_LINE getopt,
          my_bool def_val, PolyLock *lock=0,
          enum binlog_status_enum binlog_status_arg=VARIABLE_NOT_IN_BINLOG,
          on_check_function on_check_func=0,
          on_update_function on_update_func=0,
          const char *substitute=0,
          int parse_flag= PARSE_NORMAL)
    : Sys_var_typelib(name_arg, comment, flag_args, off, getopt,
                      SHOW_MY_BOOL, bool_values, def_val, lock,
                      binlog_status_arg, on_check_func, on_update_func,
                      substitute, parse_flag)
  {
    option.var_type= GET_BOOL;
    global_var(my_bool)= def_val;
    DBUG_ASSERT(def_val < 2);
    DBUG_ASSERT(getopt.arg_type == OPT_ARG || getopt.id == -1);
    DBUG_ASSERT(size == sizeof(my_bool));
  }
  bool session_update(THD *thd, set_var *var)
  {
    session_var(thd, my_bool)=
      static_cast<my_bool>(var->save_result.ulonglong_value);
    return false;
  }
  bool global_update(THD *thd, set_var *var)
  {
    global_var(my_bool)=
      static_cast<my_bool>(var->save_result.ulonglong_value);
    return false;
  }
  void session_save_default(THD *thd, set_var *var)
  {
    var->save_result.ulonglong_value=
      static_cast<ulonglong>(*(my_bool *)global_value_ptr(thd, 0));
  }
  void global_save_default(THD *thd, set_var *var)
  { var->save_result.ulonglong_value= option.def_value; }
};


/**
  A variant of enum where:
  - Each value may have multiple enum-like aliases.
  - Instances of the class can specify different default values for
    the cases:
    - User specifies the command-line option without a value (i.e.,
      --option, not --option=value).
    - User does not specify a command-line option at all.

  This exists mainly to allow extending a variable that once was
  boolean in a GA version, into an enumeration type.  Booleans accept
  multiple aliases (0=off=false, 1=on=true), but Sys_var_enum does
  not, so we could not use Sys_var_enum without breaking backward
  compatibility.  Moreover, booleans default to false if option is not
  given, and true if option is given without value.

  This is *incompatible* with boolean in the following sense:
  'SELECT @@variable' returns 0 or 1 for a boolean, whereas this class
  (similar to enum) returns the textual form. (Note that both boolean,
  enum, and this class return the textual form in SHOW VARIABLES and
  SELECT * FROM information_schema.variables).

  See enforce_gtid_consistency for an example of how this can be used.
*/
class Sys_var_multi_enum : public sys_var
{
public:
  struct ALIAS
  {
    const char *alias;
    uint number;
  };

  /**
    Class specific parameters:

    @param aliases_arg Array of ALIASes, indicating which textual
    values map to which number.  Should be terminated with an ALIAS
    having member variable alias set to NULL.  The first
    `value_count_arg' elements must map to 0, 1, etc; these will be
    used when the value is displayed.  Remaining elements may appear
    in arbitrary order.

    @param value_count_arg The number of allowed integer values.

    @param def_val The default value if no command line option is
    given. This must be a valid index into the aliases_arg array, but
    it does not have to be less than value_count.  The corresponding
    alias will be used in mysqld --help to show the default value.

    @param command_line_no_value The default value if a command line
    option is given without a value ('--command-line-option' without
    '=VALUE').  This must be less than value_count_arg.
  */
  Sys_var_multi_enum(const char *name_arg,
          const char *comment, int flag_args, ptrdiff_t off, size_t size,
          CMD_LINE getopt,
          const ALIAS aliases_arg[], uint value_count_arg,
          uint def_val, uint command_line_no_value_arg, PolyLock *lock=0,
          enum binlog_status_enum binlog_status_arg=VARIABLE_NOT_IN_BINLOG,
          on_check_function on_check_func=0,
          on_update_function on_update_func=0,
          const char *substitute=0,
          int parse_flag= PARSE_NORMAL)
    : sys_var(&all_sys_vars, name_arg, comment, flag_args, off, getopt.id,
              getopt.arg_type, SHOW_CHAR, def_val, lock,
              binlog_status_arg, on_check_func,
              on_update_func, substitute, parse_flag),
    value_count(value_count_arg),
    aliases(aliases_arg),
    command_line_no_value(command_line_no_value_arg)
  {
    for (alias_count= 0; aliases[alias_count].alias; alias_count++)
      DBUG_ASSERT(aliases[alias_count].number < value_count);
    DBUG_ASSERT(def_val < alias_count);

    option.var_type= GET_STR;
    option.value= &command_line_value;
    option.def_value= (intptr)aliases[def_val].alias;

    global_var(ulong)= aliases[def_val].number;

    DBUG_ASSERT(getopt.arg_type == OPT_ARG || getopt.id == -1);
    DBUG_ASSERT(size == sizeof(ulong));
  }

  /**
    Return the numeric value for a given alias string, or -1 if the
    string is not a valid alias.
  */
  int find_value(const char *text)
  {
    for (uint i= 0; aliases[i].alias != NULL; i++)
      if (my_strcasecmp(system_charset_info, aliases[i].alias, text) == 0)
        return aliases[i].number;
    return -1;
  }

  /**
    Because of limitations in the command-line parsing library, the
    value given on the command-line cannot be automatically copied to
    the global value.  Instead, inheritants of this class should call
    this function from mysqld.cc:mysqld_get_one_option.

    @param value_str Pointer to the value specified on the command
    line (as in --option=VALUE).

    @retval NULL Success.

    @retval non-NULL Pointer to the invalid string that was used as
    argument.
  */
  const char *fixup_command_line(const char *value_str)
  {
    DBUG_ENTER("Sys_var_multi_enum::fixup_command_line");
    char *end= NULL;
    long value;

    // User passed --option (not --option=value).
    if (value_str == NULL)
    {
      value= command_line_no_value;
      goto end;
    }

    // Get textual value.
    value= find_value(value_str);
    if (value != -1)
      goto end;

    // Get numeric value.
    value= strtol(value_str, &end, 10);
    // found a number and nothing else?
    if (end > value_str && *end == '\0')
      // value is in range?
      if (value >= 0 && (longlong)value < (longlong)value_count)
        goto end;

    // Not a valid value.
    DBUG_RETURN(value_str);

end:
    global_var(ulong)= value;
    DBUG_RETURN(NULL);
  }

  bool do_check(THD *thd, set_var *var)
  {
    DBUG_ENTER("Sys_var_multi_enum::do_check");
    char buff[STRING_BUFFER_USUAL_SIZE];
    String str(buff, sizeof(buff), system_charset_info), *res;
    if (var->value->result_type() == STRING_RESULT)
    {
      res= var->value->val_str(&str);
      if (!res)
        DBUG_RETURN(true);
      int value= find_value(res->ptr());
      if (value == -1)
        DBUG_RETURN(true);
      var->save_result.ulonglong_value= (uint)value;
    }
    else
    {
      longlong value= var->value->val_int();
      if (value < 0 || value >= (longlong)value_count)
        DBUG_RETURN(true);
      else
        var->save_result.ulonglong_value= value;
    }

    DBUG_RETURN(false);
  }
  bool check_update_type(Item_result type)
  { return type != INT_RESULT && type != STRING_RESULT; }
  bool session_update(THD *thd, set_var *var)
  {
    DBUG_ENTER("Sys_var_multi_enum::session_update");
    DBUG_ASSERT(0);
    /*
    Currently not used: uncomment if this class is used as a base for
    a session variable.

    session_var(thd, ulong)=
      static_cast<ulong>(var->save_result.ulonglong_value);
    */
    DBUG_RETURN(false);
  }
  bool global_update(THD *thd, set_var *var)
  {
    DBUG_ENTER("Sys_var_multi_enum::global_update");
    DBUG_ASSERT(0);
    /*
    Currently not used: uncomment if this some inheriting class does
    not override..

    ulong val=
      static_cast<ulong>(var->save_result.ulonglong_value);
    global_var(ulong)= val;
    */
    DBUG_RETURN(false);
  }
  void session_save_default(THD *thd, set_var *var)
  {
    DBUG_ENTER("Sys_var_multi_enum::session_save_default");
    DBUG_ASSERT(0);
    /*
    Currently not used: uncomment if this class is used as a base for
    a session variable.

    int value= find_value((char *)option.def_value);
    DBUG_ASSERT(value != -1);
    var->save_result.ulonglong_value= value;
    */
    DBUG_VOID_RETURN;
  }
  void global_save_default(THD *thd, set_var *var)
  {
    DBUG_ENTER("Sys_var_multi_enum::global_save_default");
    int value= find_value((char *)option.def_value);
    DBUG_ASSERT(value != -1);
    var->save_result.ulonglong_value= value;
    DBUG_VOID_RETURN;
  }

  uchar *session_value_ptr(THD *running_thd, THD *target_thd, LEX_STRING *base)
  {
    DBUG_ENTER("Sys_var_multi_enum::session_value_ptr");
    DBUG_ASSERT(0);
    /*
    Currently not used: uncomment if this class is used as a base for
    a session variable.

    DBUG_RETURN((uchar*)aliases[session_var(target_thd, ulong)].alias);
    */
    DBUG_RETURN(0);
  }
  uchar *global_value_ptr(THD *thd, LEX_STRING *base)
  {
    DBUG_ENTER("Sys_var_multi_enum::global_value_ptr");
    DBUG_RETURN((uchar*)aliases[global_var(ulong)].alias);
  }
private:
  /// The number of allowed numeric values.
  const uint value_count;
  /// Array of all textual aliases.
  const ALIAS *aliases;
  /// The number of elements of aliases (computed in the constructor).
  uint alias_count;

  /**
    Pointer to the value set by the command line (set by the command
    line parser, copied to the global value in fixup_command_line()).
  */
  const char *command_line_value;
  uint command_line_no_value;
};

/**
  The class for string variables. The string can be in character_set_filesystem
  or in character_set_system. The string can be allocated with my_malloc()
  or not. The state of the initial value is specified in the constructor,
  after that it's managed automatically. The value of NULL is supported.

  Class specific constructor arguments:
    enum charset_enum is_os_charset_arg

  Backing store: char*

*/
class Sys_var_charptr: public sys_var
{
public:
  Sys_var_charptr(const char *name_arg,
          const char *comment, int flag_args, ptrdiff_t off, size_t size,
          CMD_LINE getopt,
          enum charset_enum is_os_charset_arg,
          const char *def_val, PolyLock *lock= 0,
          enum binlog_status_enum binlog_status_arg= VARIABLE_NOT_IN_BINLOG,
          on_check_function on_check_func= 0,
          on_update_function on_update_func= 0,
          const char *substitute= 0,
          int parse_flag= PARSE_NORMAL)
    : sys_var(&all_sys_vars, name_arg, comment, flag_args, off, getopt.id,
              getopt.arg_type, SHOW_CHAR_PTR, (intptr) def_val,
              lock, binlog_status_arg, on_check_func, on_update_func,
              substitute, parse_flag)
  {
    is_os_charset= is_os_charset_arg == IN_FS_CHARSET;
    option.var_type= (flags & ALLOCATED) ? GET_STR_ALLOC : GET_STR;
    global_var(const char*)= def_val;
    DBUG_ASSERT(size == sizeof(char *));
  }

  void cleanup()
  {
    if (flags & ALLOCATED)
      my_free(global_var(char*));
    flags&= ~ALLOCATED;
  }

  bool do_check(THD *thd, set_var *var)
  {
    char buff[STRING_BUFFER_USUAL_SIZE], buff2[STRING_BUFFER_USUAL_SIZE];
    String str(buff, sizeof(buff), charset(thd));
    String str2(buff2, sizeof(buff2), charset(thd)), *res;

    if (!(res=var->value->val_str(&str)))
      var->save_result.string_value.str= 0;
    else
    {
      size_t unused;
      if (String::needs_conversion(res->length(), res->charset(),
                                   charset(thd), &unused))
      {
        uint errors;
        str2.copy(res->ptr(), res->length(), res->charset(), charset(thd),
                  &errors);
        res= &str2;

      }
      var->save_result.string_value.str= thd->strmake(res->ptr(), res->length());
      var->save_result.string_value.length= res->length();
    }

    return false;
  }

  bool session_update(THD *thd, set_var *var)
  {
    char *new_val=  var->save_result.string_value.str;
    size_t new_val_len= var->save_result.string_value.length;
    char *ptr= ((char *)&thd->variables + offset);

    return thd->session_sysvar_res_mgr.update((char **) ptr, new_val,
                                              new_val_len);
  }

  bool global_update(THD *thd, set_var *var)
  {
    char *new_val, *ptr= var->save_result.string_value.str;
    size_t len=var->save_result.string_value.length;
    if (ptr)
    {
      new_val= (char*) my_memdup(key_memory_Sys_var_charptr_value,
                                 ptr, len+1, MYF(MY_WME));
      if (!new_val) return true;
      new_val[len]= 0;
    }
    else
      new_val= 0;
    if (flags & ALLOCATED)
      my_free(global_var(char*));
    flags |= ALLOCATED;
    global_var(char*)= new_val;
    return false;
  }

  void session_save_default(THD *thd, set_var *var)
  {
    char *ptr= (char*)(intptr)option.def_value;
    var->save_result.string_value.str= ptr;
    var->save_result.string_value.length= ptr ? strlen(ptr) : 0;
  }

  void global_save_default(THD *thd, set_var *var)
  {
    char *ptr= (char*)(intptr)option.def_value;
    var->save_result.string_value.str= ptr;
    var->save_result.string_value.length= ptr ? strlen(ptr) : 0;
  }

  bool check_update_type(Item_result type)
  { return type != STRING_RESULT; }
};

class Sys_var_version : public Sys_var_charptr
{
public:
  Sys_var_version(const char *name_arg,
          const char *comment, int flag_args, ptrdiff_t off, size_t size,
          CMD_LINE getopt,
          enum charset_enum is_os_charset_arg,
          const char *def_val)
    : Sys_var_charptr(name_arg, comment, flag_args, off, size, getopt, is_os_charset_arg, def_val)
  {}

  ~Sys_var_version()
  {}

  virtual uchar *global_value_ptr(THD *thd, LEX_STRING *base)
  {
    uchar *value= Sys_var_charptr::global_value_ptr(thd, base);

    DBUG_EXECUTE_IF("alter_server_version_str",
                    {
                      static const char *altered_value= "some-other-version";
                      uchar *altered_value_ptr= reinterpret_cast<uchar*> (& altered_value);
                      value= altered_value_ptr;
                    });

    return value;
  }
};


class Sys_var_proxy_user: public sys_var
{
public:
  Sys_var_proxy_user(const char *name_arg,
          const char *comment, enum charset_enum is_os_charset_arg)
    : sys_var(&all_sys_vars, name_arg, comment,
              sys_var::READONLY+sys_var::ONLY_SESSION, 0, -1,
              NO_ARG, SHOW_CHAR, 0, NULL, VARIABLE_NOT_IN_BINLOG,
              NULL, NULL, NULL, PARSE_NORMAL)
  {
    is_os_charset= is_os_charset_arg == IN_FS_CHARSET;
    option.var_type= GET_STR;
  }
  bool do_check(THD *thd, set_var *var)
  {
    DBUG_ASSERT(FALSE);
    return true;
  }
  bool session_update(THD *thd, set_var *var)
  {
    DBUG_ASSERT(FALSE);
    return true;
  }
  bool global_update(THD *thd, set_var *var)
  {
    DBUG_ASSERT(FALSE);
    return false;
  }
  void session_save_default(THD *thd, set_var *var)
  { DBUG_ASSERT(FALSE); }
  void global_save_default(THD *thd, set_var *var)
  { DBUG_ASSERT(FALSE); }
  bool check_update_type(Item_result type)
  { return true; }
protected:
  virtual uchar *session_value_ptr(THD *running_thd, THD *target_thd, LEX_STRING *base)
  {
    const char* proxy_user= target_thd->security_context()->proxy_user().str;
    return proxy_user[0] ? (uchar *)proxy_user : NULL;
  }
};

class Sys_var_external_user : public Sys_var_proxy_user
{
public:
  Sys_var_external_user(const char *name_arg, const char *comment_arg,
          enum charset_enum is_os_charset_arg)
    : Sys_var_proxy_user (name_arg, comment_arg, is_os_charset_arg)
  {}

protected:
  virtual uchar *session_value_ptr(THD *running_thd, THD *target_thd, LEX_STRING *base)
  {
    LEX_CSTRING external_user= target_thd->security_context()->external_user();
    return external_user.length ? (uchar *) external_user.str : NULL;
  }
};

/**
  The class for string variables. Useful for strings that aren't necessarily
  \0-terminated. Otherwise the same as Sys_var_charptr.

  Class specific constructor arguments:
    enum charset_enum is_os_charset_arg

  Backing store: LEX_STRING

  @note
  Behaves exactly as Sys_var_charptr, only the backing store is different.
*/
class Sys_var_lexstring: public Sys_var_charptr
{
public:
  Sys_var_lexstring(const char *name_arg,
          const char *comment, int flag_args, ptrdiff_t off, size_t size,
          CMD_LINE getopt,
          enum charset_enum is_os_charset_arg,
          const char *def_val, PolyLock *lock=0,
          enum binlog_status_enum binlog_status_arg=VARIABLE_NOT_IN_BINLOG,
          on_check_function on_check_func=0,
          on_update_function on_update_func=0,
          const char *substitute=0)
    : Sys_var_charptr(name_arg, comment, flag_args, off, sizeof(char*),
              getopt, is_os_charset_arg, def_val, lock, binlog_status_arg,
              on_check_func, on_update_func, substitute)
  {
    global_var(LEX_STRING).length= strlen(def_val);
    DBUG_ASSERT(size == sizeof(LEX_STRING));
    *const_cast<SHOW_TYPE*>(&show_val_type)= SHOW_LEX_STRING;
  }
  bool global_update(THD *thd, set_var *var)
  {
    if (Sys_var_charptr::global_update(thd, var))
      return true;
    global_var(LEX_STRING).length= var->save_result.string_value.length;
    return false;
  }
};

#ifndef DBUG_OFF
/**
  @@session.dbug and @@global.dbug variables.

  @@dbug variable differs from other variables in one aspect:
  if its value is not assigned in the session, it "points" to the global
  value, and so when the global value is changed, the change
  immediately takes effect in the session.

  This semantics is intentional, to be able to debug one session from
  another.
*/
class Sys_var_dbug: public sys_var
{
public:
  Sys_var_dbug(const char *name_arg,
               const char *comment, int flag_args,
               CMD_LINE getopt,
               const char *def_val, PolyLock *lock=0,
               enum binlog_status_enum binlog_status_arg=VARIABLE_NOT_IN_BINLOG,
               on_check_function on_check_func=0,
               on_update_function on_update_func=0,
               const char *substitute=0,
               int parse_flag= PARSE_NORMAL)
    : sys_var(&all_sys_vars, name_arg, comment, flag_args, 0, getopt.id,
              getopt.arg_type, SHOW_CHAR, (intptr)def_val,
              lock, binlog_status_arg, on_check_func, on_update_func,
              substitute, parse_flag)
  { option.var_type= GET_NO_ARG; }
  bool do_check(THD *thd, set_var *var)
  {
    char buff[STRING_BUFFER_USUAL_SIZE];
    String str(buff, sizeof(buff), system_charset_info), *res;

    if (!(res=var->value->val_str(&str)))
      var->save_result.string_value.str= const_cast<char*>("");
    else
      var->save_result.string_value.str= thd->strmake(res->ptr(), res->length());
    return false;
  }
  bool session_update(THD *thd, set_var *var)
  {
    const char *val= var->save_result.string_value.str;
    if (!var->value)
      DBUG_POP();
    else
      DBUG_SET(val);
    return false;
  }
  bool global_update(THD *thd, set_var *var)
  {
    const char *val= var->save_result.string_value.str;
    DBUG_SET_INITIAL(val);
    return false;
  }
  void session_save_default(THD *thd, set_var *var)
  { }
  void global_save_default(THD *thd, set_var *var)
  {
    char *ptr= (char*)(intptr)option.def_value;
    var->save_result.string_value.str= ptr;
  }
  uchar *session_value_ptr(THD *running_thd, THD *target_thd, LEX_STRING *base)
  {
    char buf[256];
    DBUG_EXPLAIN(buf, sizeof(buf));
    return (uchar*) running_thd->mem_strdup(buf);
  }
  uchar *global_value_ptr(THD *thd, LEX_STRING *base)
  {
    char buf[256];
    DBUG_EXPLAIN_INITIAL(buf, sizeof(buf));
    return (uchar*) thd->mem_strdup(buf);
  }
  bool check_update_type(Item_result type)
  { return type != STRING_RESULT; }
};
#endif

#define KEYCACHE_VAR(X) sys_var::GLOBAL,offsetof(KEY_CACHE, X), sizeof(((KEY_CACHE *)0)->X)
#define keycache_var_ptr(KC, OFF) (((uchar*)(KC))+(OFF))
#define keycache_var(KC, OFF) (*(ulonglong*)keycache_var_ptr(KC, OFF))
typedef bool (*keycache_update_function)(THD *, KEY_CACHE *, ptrdiff_t, ulonglong);

/**
  The class for keycache_* variables. Supports structured names,
  keycache_name.variable_name.

  Class specific constructor arguments:
    everything derived from Sys_var_ulonglong

  Backing store: ulonglong

  @note these variables can be only GLOBAL
*/
class Sys_var_keycache: public Sys_var_ulonglong
{
  keycache_update_function keycache_update;
public:
  Sys_var_keycache(const char *name_arg,
          const char *comment, int flag_args, ptrdiff_t off, size_t size,
          CMD_LINE getopt,
          ulonglong min_val, ulonglong max_val, ulonglong def_val,
          uint block_size, PolyLock *lock,
          enum binlog_status_enum binlog_status_arg,
          on_check_function on_check_func,
          keycache_update_function on_update_func,
          const char *substitute=0)
    : Sys_var_ulonglong(name_arg, comment, flag_args,
                        -1,     /* offset, see base class CTOR */
                        size,
                        getopt, min_val, max_val, def_val,
                        block_size, lock, binlog_status_arg, on_check_func, 0,
                        substitute),
    keycache_update(on_update_func)
  {
    offset= off; /* Remember offset in KEY_CACHE */
    option.var_type|= GET_ASK_ADDR;
    option.value= (uchar**)1; // crash me, please
    keycache_var(dflt_key_cache, off)= def_val;
    DBUG_ASSERT(scope() == GLOBAL);
  }
  bool global_update(THD *thd, set_var *var)
  {
    ulonglong new_value= var->save_result.ulonglong_value;
    LEX_STRING *base_name= &var->base;
    KEY_CACHE *key_cache;

    /* If no basename, assume it's for the key cache named 'default' */
    if (!base_name->length)
      base_name= &default_key_cache_base;

    key_cache= get_key_cache(base_name);

    if (!key_cache)
    {                                           // Key cache didn't exists */
      if (!new_value)                           // Tried to delete cache
        return false;                           // Ok, nothing to do
      if (!(key_cache= create_key_cache(base_name->str, base_name->length)))
        return true;
    }

    /**
      Abort if some other thread is changing the key cache
      @todo This should be changed so that we wait until the previous
      assignment is done and then do the new assign
    */
    if (key_cache->in_init)
      return true;

    return keycache_update(thd, key_cache, offset, new_value);
  }
  uchar *global_value_ptr(THD *thd, LEX_STRING *base)
  {
    KEY_CACHE *key_cache= get_key_cache(base);
    if (!key_cache)
      key_cache= &zero_key_cache;
    return keycache_var_ptr(key_cache, offset);
  }
};

/**
  The class for floating point variables

  Class specific constructor arguments: min, max

  Backing store: double
*/
class Sys_var_double: public sys_var
{
public:
  Sys_var_double(const char *name_arg,
          const char *comment, int flag_args, ptrdiff_t off, size_t size,
          CMD_LINE getopt,
          double min_val, double max_val, double def_val, PolyLock *lock=0,
          enum binlog_status_enum binlog_status_arg=VARIABLE_NOT_IN_BINLOG,
          on_check_function on_check_func=0,
          on_update_function on_update_func=0,
          const char *substitute=0,
          int parse_flag= PARSE_NORMAL)
    : sys_var(&all_sys_vars, name_arg, comment, flag_args, off, getopt.id,
              getopt.arg_type, SHOW_DOUBLE,
              (longlong) getopt_double2ulonglong(def_val),
              lock, binlog_status_arg, on_check_func, on_update_func,
              substitute, parse_flag)
  {
    option.var_type= GET_DOUBLE;
    option.min_value= (longlong) getopt_double2ulonglong(min_val);
    option.max_value= (longlong) getopt_double2ulonglong(max_val);
    global_var(double)= (double)option.def_value;
    DBUG_ASSERT(min_val <= max_val);
    DBUG_ASSERT(min_val <= def_val);
    DBUG_ASSERT(max_val >= def_val);
    DBUG_ASSERT(size == sizeof(double));
  }
  bool do_check(THD *thd, set_var *var)
  {
    my_bool fixed;
    double v= var->value->val_real();
    var->save_result.double_value= getopt_double_limit_value(v, &option, &fixed);

    return throw_bounds_warning(thd, name.str, fixed, v);
  }
  bool session_update(THD *thd, set_var *var)
  {
    session_var(thd, double)= var->save_result.double_value;
    return false;
  }
  bool global_update(THD *thd, set_var *var)
  {
    global_var(double)= var->save_result.double_value;
    return false;
  }
  bool check_update_type(Item_result type)
  {
    return type != INT_RESULT && type != REAL_RESULT && type != DECIMAL_RESULT;
  }
  void session_save_default(THD *thd, set_var *var)
  { var->save_result.double_value= global_var(double); }
  void global_save_default(THD *thd, set_var *var)
  { var->save_result.double_value= getopt_ulonglong2double(option.def_value); }
};

/**
  The class for @test_flags (core_file for now).
  It's derived from Sys_var_mybool.

  Class specific constructor arguments:
    Caller need not pass in a variable as we make up the value on the
    fly, that is, we derive it from the global test_flags bit vector.

  Backing store: my_bool
*/
class Sys_var_test_flag: public Sys_var_mybool
{
private:
  my_bool test_flag_value;
  uint    test_flag_mask;
public:
  Sys_var_test_flag(const char *name_arg, const char *comment, uint mask)
  : Sys_var_mybool(name_arg, comment, READ_ONLY GLOBAL_VAR(test_flag_value),
          NO_CMD_LINE, DEFAULT(FALSE))
  {
    test_flag_mask= mask;
  }
  uchar *global_value_ptr(THD *thd, LEX_STRING *base)
  {
    test_flag_value= ((test_flags & test_flag_mask) > 0);
    return (uchar*) &test_flag_value;
  }
};

/**
  The class for the @max_user_connections.
  It's derived from Sys_var_uint, but non-standard session value
  requires a new class.

  Class specific constructor arguments:
    everything derived from Sys_var_uint

  Backing store: uint
*/
class Sys_var_max_user_conn: public Sys_var_uint
{
public:
  Sys_var_max_user_conn(const char *name_arg,
          const char *comment, int flag_args, ptrdiff_t off, size_t size,
          CMD_LINE getopt,
          uint min_val, uint max_val, uint def_val,
          uint block_size, PolyLock *lock=0,
          enum binlog_status_enum binlog_status_arg=VARIABLE_NOT_IN_BINLOG,
          on_check_function on_check_func=0,
          on_update_function on_update_func=0,
          const char *substitute=0)
    : Sys_var_uint(name_arg, comment, SESSION, off, size, getopt,
              min_val, max_val, def_val, block_size,
              lock, binlog_status_arg, on_check_func, on_update_func,
              substitute)
  { }
  uchar *session_value_ptr(THD *running_thd, THD *target_thd, LEX_STRING *base)
  {
    const USER_CONN *uc= target_thd->get_user_connect();
    if (uc && uc->user_resources.user_conn)
      return (uchar*) &(uc->user_resources.user_conn);
    return global_value_ptr(running_thd, base);
  }
};

// overflow-safe (1 << X)-1
#define MAX_SET(X) ((((1UL << ((X)-1))-1) << 1) | 1)

/**
  The class for flagset variables - a variant of SET that allows in-place
  editing (turning on/off individual bits). String representations looks like
  a "flag=val,flag=val,...". Example: @@optimizer_switch

  Class specific constructor arguments:
    char* values[]    - 0-terminated list of strings of valid values

  Backing store: ulonglong

  @note
  the last value in the values[] array should
  *always* be the string "default".
*/
class Sys_var_flagset: public Sys_var_typelib
{
public:
  Sys_var_flagset(const char *name_arg,
          const char *comment, int flag_args, ptrdiff_t off, size_t size,
          CMD_LINE getopt,
          const char *values[], ulonglong def_val, PolyLock *lock=0,
          enum binlog_status_enum binlog_status_arg=VARIABLE_NOT_IN_BINLOG,
          on_check_function on_check_func=0,
          on_update_function on_update_func=0,
          const char *substitute=0)
    : Sys_var_typelib(name_arg, comment, flag_args, off, getopt,
                      SHOW_CHAR, values, def_val, lock,
                      binlog_status_arg, on_check_func, on_update_func,
                      substitute)
  {
    option.var_type= GET_FLAGSET;
    global_var(ulonglong)= def_val;
    DBUG_ASSERT(typelib.count > 1);
    DBUG_ASSERT(typelib.count <= 65);
    DBUG_ASSERT(def_val < MAX_SET(typelib.count));
    DBUG_ASSERT(strcmp(values[typelib.count-1], "default") == 0);
    DBUG_ASSERT(size == sizeof(ulonglong));
  }
  bool do_check(THD *thd, set_var *var)
  {
    char buff[STRING_BUFFER_USUAL_SIZE];
    String str(buff, sizeof(buff), system_charset_info), *res;
    ulonglong default_value, current_value;
    if (var->type == OPT_GLOBAL)
    {
      default_value= option.def_value;
      current_value= global_var(ulonglong);
    }
    else
    {
      default_value= global_var(ulonglong);
      current_value= session_var(thd, ulonglong);
    }

    if (var->value->result_type() == STRING_RESULT)
    {
      if (!(res=var->value->val_str(&str)))
        return true;
      else
      {
        char *error;
        uint error_len;

        var->save_result.ulonglong_value=
              find_set_from_flags(&typelib,
                                  typelib.count,
                                  current_value,
                                  default_value,
                                  res->ptr(),
                                  static_cast<uint>(res->length()),
                                  &error, &error_len);
        if (error)
        {
          ErrConvString err(error, error_len, res->charset());
          my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), name.str, err.ptr());
          return true;
        }
      }
    }
    else
    {
      longlong tmp=var->value->val_int();
      if ((tmp < 0 && ! var->value->unsigned_flag)
          || (ulonglong)tmp > MAX_SET(typelib.count))
        return true;
      else
        var->save_result.ulonglong_value= tmp;
    }

    return false;
  }
  bool session_update(THD *thd, set_var *var)
  {
    session_var(thd, ulonglong)= var->save_result.ulonglong_value;
    return false;
  }
  bool global_update(THD *thd, set_var *var)
  {
    global_var(ulonglong)= var->save_result.ulonglong_value;
    return false;
  }
  void session_save_default(THD *thd, set_var *var)
  { var->save_result.ulonglong_value= global_var(ulonglong); }
  void global_save_default(THD *thd, set_var *var)
  { var->save_result.ulonglong_value= option.def_value; }
  uchar *session_value_ptr(THD *running_thd, THD *target_thd, LEX_STRING *base)
  {
    return (uchar*)flagset_to_string(running_thd, 0, session_var(target_thd, ulonglong),
                                     typelib.type_names);
  }
  uchar *global_value_ptr(THD *thd, LEX_STRING *base)
  {
    return (uchar*)flagset_to_string(thd, 0, global_var(ulonglong),
                                     typelib.type_names);
  }
};

/**
  The class for SET variables - variables taking zero or more values
  from the given list. Example: @@sql_mode

  Class specific constructor arguments:
    char* values[]    - 0-terminated list of strings of valid values

  Backing store: ulonglong
*/
class Sys_var_set: public Sys_var_typelib
{
public:
  Sys_var_set(const char *name_arg,
          const char *comment, int flag_args, ptrdiff_t off, size_t size,
          CMD_LINE getopt,
          const char *values[], ulonglong def_val, PolyLock *lock=0,
          enum binlog_status_enum binlog_status_arg=VARIABLE_NOT_IN_BINLOG,
          on_check_function on_check_func=0,
          on_update_function on_update_func=0,
          const char *substitute=0)
    : Sys_var_typelib(name_arg, comment, flag_args, off, getopt,
                      SHOW_CHAR, values, def_val, lock,
                      binlog_status_arg, on_check_func, on_update_func,
                      substitute)
  {
    option.var_type= GET_SET;
    global_var(ulonglong)= def_val;
    DBUG_ASSERT(typelib.count > 0);
    DBUG_ASSERT(typelib.count <= 64);
    DBUG_ASSERT(def_val < MAX_SET(typelib.count));
    DBUG_ASSERT(size == sizeof(ulonglong));
  }
  bool do_check(THD *thd, set_var *var)
  {
    char buff[STRING_BUFFER_USUAL_SIZE];
    String str(buff, sizeof(buff), system_charset_info), *res;

    if (var->value->result_type() == STRING_RESULT)
    {
      if (!(res=var->value->val_str(&str)))
        return true;
      else
      {
        char *error;
        uint error_len;
        bool not_used;

        var->save_result.ulonglong_value=
              find_set(&typelib, res->ptr(),
                       static_cast<uint>(res->length()), NULL,
                       &error, &error_len, &not_used);
        /*
          note, we only issue an error if error_len > 0.
          That is even while empty (zero-length) values are considered
          errors by find_set(), these errors are ignored here
        */
        if (error_len)
        {
          ErrConvString err(error, error_len, res->charset());
          my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), name.str, err.ptr());
          return true;
        }
      }
    }
    else
    {
      longlong tmp=var->value->val_int();
      if ((tmp < 0 && ! var->value->unsigned_flag)
          || (ulonglong)tmp > MAX_SET(typelib.count))
        return true;
      else
        var->save_result.ulonglong_value= tmp;
    }

    return false;
  }
  bool session_update(THD *thd, set_var *var)
  {
    session_var(thd, ulonglong)= var->save_result.ulonglong_value;
    return false;
  }
  bool global_update(THD *thd, set_var *var)
  {
    global_var(ulonglong)= var->save_result.ulonglong_value;
    return false;
  }
  void session_save_default(THD *thd, set_var *var)
  { var->save_result.ulonglong_value= global_var(ulonglong); }
  void global_save_default(THD *thd, set_var *var)
  { var->save_result.ulonglong_value= option.def_value; }
  uchar *session_value_ptr(THD *running_thd, THD *target_thd, LEX_STRING *base)
  {
    return (uchar*)set_to_string(running_thd, 0, session_var(target_thd, ulonglong),
                                 typelib.type_names);
  }
  uchar *global_value_ptr(THD *thd, LEX_STRING *base)
  {
    return (uchar*)set_to_string(thd, 0, global_var(ulonglong),
                                 typelib.type_names);
  }
};

/**
  The class for variables which value is a plugin.
  Example: @@default_storage_engine

  Class specific constructor arguments:
    int plugin_type_arg (for example MYSQL_STORAGE_ENGINE_PLUGIN)

  Backing store: plugin_ref

  @note
  these variables don't support command-line equivalents, any such
  command-line options should be added manually to my_long_options in mysqld.cc
*/
class Sys_var_plugin: public sys_var
{
  int plugin_type;
public:
  Sys_var_plugin(const char *name_arg,
          const char *comment, int flag_args, ptrdiff_t off, size_t size,
          CMD_LINE getopt,
          int plugin_type_arg, char **def_val, PolyLock *lock=0,
          enum binlog_status_enum binlog_status_arg=VARIABLE_NOT_IN_BINLOG,
          on_check_function on_check_func=0,
          on_update_function on_update_func=0,
          const char *substitute=0,
          int parse_flag= PARSE_NORMAL)
    : sys_var(&all_sys_vars, name_arg, comment, flag_args, off, getopt.id,
              getopt.arg_type, SHOW_CHAR, (intptr)def_val,
              lock, binlog_status_arg, on_check_func, on_update_func,
              substitute, parse_flag),
    plugin_type(plugin_type_arg)
  {
    option.var_type= GET_STR;
    DBUG_ASSERT(size == sizeof(plugin_ref));
    DBUG_ASSERT(getopt.id == -1); // force NO_CMD_LINE
  }
  bool do_check(THD *thd, set_var *var)
  {
    char buff[STRING_BUFFER_USUAL_SIZE];
    String str(buff,sizeof(buff), system_charset_info), *res;

    /* NULLs can't be used as a default storage engine */
    if (!(res=var->value->val_str(&str)))
      return true;

    const LEX_STRING pname= { const_cast<char*>(res->ptr()), res->length() };
    plugin_ref plugin;

    // special code for storage engines (e.g. to handle historical aliases)
    if (plugin_type == MYSQL_STORAGE_ENGINE_PLUGIN)
      plugin= ha_resolve_by_name(thd, &pname, FALSE);
    else
    {
      LEX_CSTRING pname_cstr= { pname.str, pname.length };
      plugin= my_plugin_lock_by_name(thd, pname_cstr, plugin_type);
    }

    if (!plugin)
    {
      // historically different error code
      if (plugin_type == MYSQL_STORAGE_ENGINE_PLUGIN)
      {
        ErrConvString err(res);
        my_error(ER_UNKNOWN_STORAGE_ENGINE, MYF(0), err.ptr());
      }
      return true;
    }
    var->save_result.plugin= plugin;
    return false;
  }
  void do_update(plugin_ref *valptr, plugin_ref newval)
  {
    plugin_ref oldval= *valptr;
    if (oldval != newval)
    {
      *valptr= my_plugin_lock(NULL, &newval);
      plugin_unlock(NULL, oldval);
    }
  }
  bool session_update(THD *thd, set_var *var)
  {
    do_update((plugin_ref*)session_var_ptr(thd),
              var->save_result.plugin);
    return false;
  }
  bool global_update(THD *thd, set_var *var)
  {
    do_update((plugin_ref*)global_var_ptr(),
              var->save_result.plugin);
    return false;
  }
  void session_save_default(THD *thd, set_var *var)
  {
    plugin_ref plugin= global_var(plugin_ref);
    var->save_result.plugin= my_plugin_lock(thd, &plugin);
  }
  void global_save_default(THD *thd, set_var *var)
  {
    LEX_STRING pname;
    char **default_value= reinterpret_cast<char**>(option.def_value);
    pname.str= *default_value;
    pname.length= strlen(pname.str);

    plugin_ref plugin;
    if (plugin_type == MYSQL_STORAGE_ENGINE_PLUGIN)
      plugin= ha_resolve_by_name(thd, &pname, FALSE);
    else
    {
      LEX_CSTRING pname_cstr= {pname.str,pname.length};
      plugin= my_plugin_lock_by_name(thd, pname_cstr, plugin_type);
    }
    DBUG_ASSERT(plugin);

    var->save_result.plugin= my_plugin_lock(thd, &plugin);
  }
  bool check_update_type(Item_result type)
  { return type != STRING_RESULT; }
  uchar *session_value_ptr(THD *running_thd, THD *target_thd, LEX_STRING *base)
  {
    plugin_ref plugin= session_var(target_thd, plugin_ref);
    return (uchar*)(plugin ? running_thd->strmake(plugin_name(plugin)->str,
                                          plugin_name(plugin)->length) : 0);
  }
  uchar *global_value_ptr(THD *thd, LEX_STRING *base)
  {
    plugin_ref plugin= global_var(plugin_ref);
    return (uchar*)(plugin ? thd->strmake(plugin_name(plugin)->str,
                                          plugin_name(plugin)->length) : 0);
  }
};

#if defined(ENABLED_DEBUG_SYNC)
/**
  The class for @@debug_sync session-only variable
*/
class Sys_var_debug_sync :public sys_var
{
public:
  Sys_var_debug_sync(const char *name_arg,
               const char *comment, int flag_args,
               CMD_LINE getopt,
               const char *def_val, PolyLock *lock=0,
               enum binlog_status_enum binlog_status_arg=VARIABLE_NOT_IN_BINLOG,
               on_check_function on_check_func=0,
               on_update_function on_update_func=0,
               const char *substitute=0,
               int parse_flag= PARSE_NORMAL)
    : sys_var(&all_sys_vars, name_arg, comment, flag_args, 0, getopt.id,
              getopt.arg_type, SHOW_CHAR, (intptr)def_val,
              lock, binlog_status_arg, on_check_func, on_update_func,
              substitute, parse_flag)
  {
    DBUG_ASSERT(scope() == ONLY_SESSION);
    option.var_type= GET_NO_ARG;
  }
  bool do_check(THD *thd, set_var *var)
  {
    char buff[STRING_BUFFER_USUAL_SIZE];
    String str(buff, sizeof(buff), system_charset_info), *res;

    if (!(res=var->value->val_str(&str)))
      var->save_result.string_value.str= const_cast<char*>("");
    else
      var->save_result.string_value.str= thd->strmake(res->ptr(), res->length());
    return false;
  }
  bool session_update(THD *thd, set_var *var)
  {
    extern bool debug_sync_update(THD *thd, char *val_str);
    return debug_sync_update(thd, var->save_result.string_value.str);
  }
  bool global_update(THD *thd, set_var *var)
  {
    DBUG_ASSERT(FALSE);
    return true;
  }
  void session_save_default(THD *thd, set_var *var)
  {
    var->save_result.string_value.str= const_cast<char*>("");
    var->save_result.string_value.length= 0;
  }
  void global_save_default(THD *thd, set_var *var)
  {
    DBUG_ASSERT(FALSE);
  }
  uchar *session_value_ptr(THD *running_thd, THD *target_thd, LEX_STRING *base)
  {
    extern uchar *debug_sync_value_ptr(THD *thd);
    return debug_sync_value_ptr(running_thd);
  }
  uchar *global_value_ptr(THD *thd, LEX_STRING *base)
  {
    DBUG_ASSERT(FALSE);
    return 0;
  }
  bool check_update_type(Item_result type)
  { return type != STRING_RESULT; }
};
#endif /* defined(ENABLED_DEBUG_SYNC) */

/**
  The class for bit variables - a variant of boolean that stores the value
  in a bit.

  Class specific constructor arguments:
    ulonglong bitmask_arg - the mask for the bit to set in the ulonglong
                            backing store

  Backing store: ulonglong

  @note
  This class supports the "reverse" semantics, when the value of the bit
  being 0 corresponds to the value of variable being set. To activate it
  use REVERSE(bitmask) instead of simply bitmask in the constructor.

  @note
  variables of this class cannot be set from the command line as
  my_getopt does not support bits.
*/
class Sys_var_bit: public Sys_var_typelib
{
  ulonglong bitmask;
  bool reverse_semantics;
  void set(uchar *ptr, ulonglong value)
  {
    if ((value != 0) ^ reverse_semantics)
      (*(ulonglong *)ptr)|= bitmask;
    else
      (*(ulonglong *)ptr)&= ~bitmask;
  }
public:
  Sys_var_bit(const char *name_arg,
          const char *comment, int flag_args, ptrdiff_t off, size_t size,
          CMD_LINE getopt,
          ulonglong bitmask_arg, my_bool def_val, PolyLock *lock=0,
          enum binlog_status_enum binlog_status_arg=VARIABLE_NOT_IN_BINLOG,
          on_check_function on_check_func=0,
          on_update_function on_update_func=0,
          const char *substitute=0)
    : Sys_var_typelib(name_arg, comment, flag_args, off, getopt,
                      SHOW_MY_BOOL, bool_values, def_val, lock,
                      binlog_status_arg, on_check_func, on_update_func,
                      substitute)
  {
    option.var_type= GET_BOOL;
    reverse_semantics= my_count_bits(bitmask_arg) > 1;
    bitmask= reverse_semantics ? ~bitmask_arg : bitmask_arg;
    set(global_var_ptr(), def_val);
    DBUG_ASSERT(def_val < 2);
    DBUG_ASSERT(getopt.id == -1); // force NO_CMD_LINE
    DBUG_ASSERT(size == sizeof(ulonglong));
  }
  bool session_update(THD *thd, set_var *var)
  {
    set(session_var_ptr(thd), var->save_result.ulonglong_value);
    return false;
  }
  bool global_update(THD *thd, set_var *var)
  {
    set(global_var_ptr(), var->save_result.ulonglong_value);
    return false;
  }
  void session_save_default(THD *thd, set_var *var)
  { var->save_result.ulonglong_value= global_var(ulonglong) & bitmask; }
  void global_save_default(THD *thd, set_var *var)
  { var->save_result.ulonglong_value= option.def_value; }
  uchar *session_value_ptr(THD *running_thd, THD *target_thd, LEX_STRING *base)
  {
    running_thd->sys_var_tmp.my_bool_value=
      static_cast<my_bool>(reverse_semantics ^
                           ((session_var(target_thd, ulonglong) & bitmask) != 0));
    return (uchar*) &running_thd->sys_var_tmp.my_bool_value;
  }
  uchar *global_value_ptr(THD *thd, LEX_STRING *base)
  {
    thd->sys_var_tmp.my_bool_value=
      static_cast<my_bool>(reverse_semantics ^
                           ((global_var(ulonglong) & bitmask) != 0));
    return (uchar*) &thd->sys_var_tmp.my_bool_value;
  }
};

/**
  The class for variables that have a special meaning for a session,
  such as @@timestamp or @@rnd_seed1, their values typically cannot be read
  from SV structure, and a special "read" callback is provided.

  Class specific constructor arguments:
    everything derived from Sys_var_ulonglong
    session_special_read_function read_func_arg

  Backing store: ulonglong

  @note
  These variables are session-only, global or command-line equivalents
  are not supported as they're generally meaningless.
*/
class Sys_var_session_special: public Sys_var_ulonglong
{
  typedef bool (*session_special_update_function)(THD *thd, set_var *var);
  typedef ulonglong (*session_special_read_function)(THD *thd);

  session_special_read_function read_func;
  session_special_update_function update_func;
public:
  Sys_var_session_special(const char *name_arg,
               const char *comment, int flag_args,
               CMD_LINE getopt,
               ulonglong min_val, ulonglong max_val, uint block_size,
               PolyLock *lock, enum binlog_status_enum binlog_status_arg,
               on_check_function on_check_func,
               session_special_update_function update_func_arg,
               session_special_read_function read_func_arg,
               const char *substitute=0)
    : Sys_var_ulonglong(name_arg, comment, flag_args, 0,
              sizeof(ulonglong), getopt, min_val,
              max_val, 0, block_size, lock, binlog_status_arg, on_check_func, 0,
              substitute),
      read_func(read_func_arg), update_func(update_func_arg)
  {
    DBUG_ASSERT(scope() == ONLY_SESSION);
    DBUG_ASSERT(getopt.id == -1); // NO_CMD_LINE, because the offset is fake
  }
  bool session_update(THD *thd, set_var *var)
  { return update_func(thd, var); }
  bool global_update(THD *thd, set_var *var)
  {
    DBUG_ASSERT(FALSE);
    return true;
  }
  void session_save_default(THD *thd, set_var *var)
  { var->value= 0; }
  void global_save_default(THD *thd, set_var *var)
  { DBUG_ASSERT(FALSE); }
  uchar *session_value_ptr(THD *running_thd, THD *target_thd, LEX_STRING *base)
  {
    running_thd->sys_var_tmp.ulonglong_value= read_func(target_thd);
    return (uchar*) &running_thd->sys_var_tmp.ulonglong_value;
  }
  uchar *global_value_ptr(THD *thd, LEX_STRING *base)
  {
    DBUG_ASSERT(FALSE);
    return 0;
  }
};


/**
  Similar to Sys_var_session_special, but with double storage.
*/
class Sys_var_session_special_double: public Sys_var_double
{
  typedef bool (*session_special_update_function)(THD *thd, set_var *var);
  typedef double (*session_special_read_double_function)(THD *thd);

  session_special_read_double_function read_func;
  session_special_update_function update_func;
public:
  Sys_var_session_special_double(const char *name_arg,
               const char *comment, int flag_args,
               CMD_LINE getopt,
               double min_val, double max_val, uint block_size,
               PolyLock *lock, enum binlog_status_enum binlog_status_arg,
               on_check_function on_check_func,
               session_special_update_function update_func_arg,
               session_special_read_double_function read_func_arg,
               const char *substitute=0)
    : Sys_var_double(name_arg, comment, flag_args, 0,
              sizeof(double), getopt,
              min_val, max_val, 0.0,
              lock, binlog_status_arg, on_check_func, 0,
              substitute),
      read_func(read_func_arg), update_func(update_func_arg)
  {
    DBUG_ASSERT(scope() == ONLY_SESSION);
    DBUG_ASSERT(getopt.id == -1); // NO_CMD_LINE, because the offset is fake
  }
  bool session_update(THD *thd, set_var *var)
  { return update_func(thd, var); }
  bool global_update(THD *thd, set_var *var)
  {
    DBUG_ASSERT(FALSE);
    return true;
  }
  void session_save_default(THD *thd, set_var *var)
  { var->value= 0; }
  void global_save_default(THD *thd, set_var *var)
  { DBUG_ASSERT(FALSE); }
  uchar *session_value_ptr(THD *running_thd, THD *target_thd, LEX_STRING *base)
  {
    running_thd->sys_var_tmp.double_value= read_func(target_thd);
    return (uchar *) &running_thd->sys_var_tmp.double_value;
  }
  uchar *global_value_ptr(THD *thd, LEX_STRING *base)
  {
    DBUG_ASSERT(FALSE);
    return 0;
  }
};


/**
  The class for read-only variables that show whether a particular
  feature is supported by the server. Example: have_compression

  Backing store: enum SHOW_COMP_OPTION

  @note
  These variables are necessarily read-only, only global, and have no
  command-line equivalent.
*/
class Sys_var_have: public sys_var
{
public:
  Sys_var_have(const char *name_arg,
               const char *comment, int flag_args, ptrdiff_t off, size_t size,
               CMD_LINE getopt,
               PolyLock *lock=0,
               enum binlog_status_enum binlog_status_arg=VARIABLE_NOT_IN_BINLOG,
               on_check_function on_check_func=0,
               on_update_function on_update_func=0,
               const char *substitute=0,
               int parse_flag= PARSE_NORMAL)
    : sys_var(&all_sys_vars, name_arg, comment, flag_args, off, getopt.id,
              getopt.arg_type, SHOW_CHAR, 0,
              lock, binlog_status_arg, on_check_func, on_update_func,
              substitute, parse_flag)
  {
    DBUG_ASSERT(scope() == GLOBAL);
    DBUG_ASSERT(getopt.id == -1);
    DBUG_ASSERT(lock == 0);
    DBUG_ASSERT(binlog_status_arg == VARIABLE_NOT_IN_BINLOG);
    DBUG_ASSERT(is_readonly());
    DBUG_ASSERT(on_update == 0);
    DBUG_ASSERT(size == sizeof(enum SHOW_COMP_OPTION));
  }
  bool do_check(THD *thd, set_var *var) {
    DBUG_ASSERT(FALSE);
    return true;
  }
  bool session_update(THD *thd, set_var *var)
  {
    DBUG_ASSERT(FALSE);
    return true;
  }
  bool global_update(THD *thd, set_var *var)
  {
    DBUG_ASSERT(FALSE);
    return true;
  }
  void session_save_default(THD *thd, set_var *var) { }
  void global_save_default(THD *thd, set_var *var) { }
  uchar *session_value_ptr(THD *running_thd, THD *target_thd, LEX_STRING *base)
  {
    DBUG_ASSERT(FALSE);
    return 0;
  }
  uchar *global_value_ptr(THD *thd, LEX_STRING *base)
  {
    return (uchar*)show_comp_option_name[global_var(enum SHOW_COMP_OPTION)];
  }
  bool check_update_type(Item_result type) { return false; }
};

/**
  Generic class for variables for storing entities that are internally
  represented as structures, have names, and possibly can be referred to by
  numbers.  Examples: character sets, collations, locales,

  Class specific constructor arguments:
    ptrdiff_t name_offset  - offset of the 'name' field in the structure

  Backing store: void*

  @note
  As every such a structure requires special treatment from my_getopt,
  these variables don't support command-line equivalents, any such
  command-line options should be added manually to my_long_options in mysqld.cc
*/
class Sys_var_struct: public sys_var
{
  ptrdiff_t name_offset; // offset to the 'name' property in the structure
public:
  Sys_var_struct(const char *name_arg,
          const char *comment, int flag_args, ptrdiff_t off, size_t size,
          CMD_LINE getopt,
          ptrdiff_t name_off, void *def_val, PolyLock *lock=0,
          enum binlog_status_enum binlog_status_arg=VARIABLE_NOT_IN_BINLOG,
          on_check_function on_check_func=0,
          on_update_function on_update_func=0,
          const char *substitute=0,
          int parse_flag= PARSE_NORMAL)
    : sys_var(&all_sys_vars, name_arg, comment, flag_args, off, getopt.id,
              getopt.arg_type, SHOW_CHAR, (intptr)def_val,
              lock, binlog_status_arg, on_check_func, on_update_func,
              substitute, parse_flag),
      name_offset(name_off)
  {
    option.var_type= GET_STR;
    /*
      struct variables are special on the command line - often (e.g. for
      charsets) the name cannot be immediately resolved, but only after all
      options (in particular, basedir) are parsed.

      thus all struct command-line options should be added manually
      to my_long_options in mysqld.cc
    */
    DBUG_ASSERT(getopt.id == -1);
    DBUG_ASSERT(size == sizeof(void *));
  }
  bool do_check(THD *thd, set_var *var)
  { return false; }
  bool session_update(THD *thd, set_var *var)
  {
    session_var(thd, const void*)= var->save_result.ptr;
    return false;
  }
  bool global_update(THD *thd, set_var *var)
  {
    global_var(const void*)= var->save_result.ptr;
    return false;
  }
  void session_save_default(THD *thd, set_var *var)
  { var->save_result.ptr= global_var(void*); }
  void global_save_default(THD *thd, set_var *var)
  {
    void **default_value= reinterpret_cast<void**>(option.def_value);
    var->save_result.ptr= *default_value;
  }
  bool check_update_type(Item_result type)
  { return type != INT_RESULT && type != STRING_RESULT; }
  uchar *session_value_ptr(THD *running_thd, THD *target_thd, LEX_STRING *base)
  {
    uchar *ptr= session_var(target_thd, uchar*);
    return ptr ? *(uchar**)(ptr+name_offset) : 0;
  }
  uchar *global_value_ptr(THD *thd, LEX_STRING *base)
  {
    uchar *ptr= global_var(uchar*);
    return ptr ? *(uchar**)(ptr+name_offset) : 0;
  }
};

/**
  The class for variables that store time zones

  Backing store: Time_zone*

  @note
  Time zones cannot be supported directly by my_getopt, thus
  these variables don't support command-line equivalents, any such
  command-line options should be added manually to my_long_options in mysqld.cc
*/
class Sys_var_tz: public sys_var
{
public:
  Sys_var_tz(const char *name_arg,
             const char *comment, int flag_args, ptrdiff_t off, size_t size,
             CMD_LINE getopt,
             Time_zone **def_val, PolyLock *lock=0,
             enum binlog_status_enum binlog_status_arg=VARIABLE_NOT_IN_BINLOG,
             on_check_function on_check_func=0,
             on_update_function on_update_func=0,
             const char *substitute=0,
             int parse_flag= PARSE_NORMAL)
    : sys_var(&all_sys_vars, name_arg, comment, flag_args, off, getopt.id,
              getopt.arg_type, SHOW_CHAR, (intptr)def_val,
              lock, binlog_status_arg, on_check_func, on_update_func,
              substitute, parse_flag)
  {
    DBUG_ASSERT(getopt.id == -1);
    DBUG_ASSERT(size == sizeof(Time_zone *));
  }
  bool do_check(THD *thd, set_var *var)
  {
    char buff[MAX_TIME_ZONE_NAME_LENGTH];
    String str(buff, sizeof(buff), &my_charset_latin1);
    String *res= var->value->val_str(&str);

    if (!res)
      return true;

    if (!(var->save_result.time_zone= my_tz_find(thd, res)))
    {
      ErrConvString err(res);
      my_error(ER_UNKNOWN_TIME_ZONE, MYF(0), err.ptr());
      return true;
    }
    return false;
  }
  bool session_update(THD *thd, set_var *var)
  {
    session_var(thd, Time_zone*)= var->save_result.time_zone;
    return false;
  }
  bool global_update(THD *thd, set_var *var)
  {
    global_var(Time_zone*)= var->save_result.time_zone;
    return false;
  }
  void session_save_default(THD *thd, set_var *var)
  {
    var->save_result.time_zone= global_var(Time_zone*);
  }
  void global_save_default(THD *thd, set_var *var)
  {
    var->save_result.time_zone=
      *(Time_zone**)(intptr)option.def_value;
  }
  uchar *session_value_ptr(THD *running_thd, THD *target_thd, LEX_STRING *base)
  {
    /*
      This is an ugly fix for replication: we don't replicate properly queries
      invoking system variables' values to update tables; but
      CONVERT_TZ(,,@@session.time_zone) is so popular that we make it
      replicable (i.e. we tell the binlog code to store the session
      timezone). If it's the global value which was used we can't replicate
      (binlog code stores session value only).
    */
    target_thd->time_zone_used= 1;
    return (uchar *)(session_var(target_thd, Time_zone*)->get_name()->ptr());
  }
  uchar *global_value_ptr(THD *thd, LEX_STRING *base)
  {
    return (uchar *)(global_var(Time_zone*)->get_name()->ptr());
  }
  bool check_update_type(Item_result type)
  { return type != STRING_RESULT; }
};


/**
  Class representing the 'tx_isolation' system variable. This
  variable can also be indirectly set using 'SET TRANSACTION ISOLATION
  LEVEL'. This variable is deprecated and will be removed in a
  future release. 'transaction_isolation' is used an alternative
  instead.
*/
class Sys_var_tx_isolation: public Sys_var_enum
{
public:
  Sys_var_tx_isolation(const char *name_arg,
          const char *comment, int flag_args, ptrdiff_t off, size_t size,
          CMD_LINE getopt,
          const char *values[], uint def_val, PolyLock *lock,
          enum binlog_status_enum binlog_status_arg,
          on_check_function on_check_func,
          on_update_function on_update_func=0,
          const char *substitute=0)
    :Sys_var_enum(name_arg, comment, flag_args, off, size, getopt,
                  values, def_val, lock, binlog_status_arg, on_check_func,
                  on_update_func, substitute)
  {}
  virtual bool session_update(THD *thd, set_var *var);
};


/**
  Class representing the tx_read_only system variable for setting
  default transaction access mode. This variable is deprecated
  and will be removed in future release. 'transaction_read_only'
  is used as an alternative instead.

  Note that there is a special syntax - SET TRANSACTION READ ONLY
  (or READ WRITE) that sets the access mode for the next transaction
  only.
*/

class Sys_var_tx_read_only: public Sys_var_mybool
{
public:
  Sys_var_tx_read_only(const char *name_arg, const char *comment, int flag_args,
                       ptrdiff_t off, size_t size, CMD_LINE getopt,
                       my_bool def_val, PolyLock *lock,
                       enum binlog_status_enum binlog_status_arg,
                       on_check_function on_check_func,
                       on_update_function on_update_func=0,
                       const char *substitute=0)
    :Sys_var_mybool(name_arg, comment, flag_args, off, size, getopt,
                    def_val, lock, binlog_status_arg, on_check_func,
                    on_update_func, substitute)
  {}
  virtual bool session_update(THD *thd, set_var *var);
};


/**
  Class representing the sql_log_bin system variable for controlling
  whether logging to the binary log is done.
*/

class Sys_var_sql_log_bin: public Sys_var_mybool
{
public:
  Sys_var_sql_log_bin(const char *name_arg, const char *comment, int flag_args,
                      ptrdiff_t off, size_t size, CMD_LINE getopt,
                      my_bool def_val, PolyLock *lock,
                      enum binlog_status_enum binlog_status_arg,
                      on_check_function on_check_func,
                      on_update_function on_update_func)
    :Sys_var_mybool(name_arg, comment, flag_args, off, size, getopt,
                    def_val, lock, binlog_status_arg, on_check_func,
                    on_update_func)
  {}

  uchar *global_value_ptr(THD *thd, LEX_STRING *base)
  {
    /* Reading GLOBAL SQL_LOG_BIN produces a deprecation warning. */
    if (base != NULL)
      push_warning_printf(thd, Sql_condition::SL_WARNING,
                          ER_WARN_DEPRECATED_SYNTAX,
                          ER(ER_WARN_DEPRECATED_SYNTAX),
                          "@@global.sql_log_bin", "the constant 1 "
                          "(since @@global.sql_log_bin is always equal to 1)");

    return Sys_var_mybool::global_value_ptr(thd, base);
  }
};

/**
   A class for @@global.binlog_checksum that has
   a specialized update method.
*/
class Sys_var_enum_binlog_checksum: public Sys_var_enum
{
public:
  Sys_var_enum_binlog_checksum(const char *name_arg,
          const char *comment, int flag_args, ptrdiff_t off, size_t size,
          CMD_LINE getopt,
          const char *values[], uint def_val, PolyLock *lock,
          enum binlog_status_enum binlog_status_arg,
          on_check_function on_check_func=0
          )
    :Sys_var_enum(name_arg, comment, flag_args, off, size, getopt,
                  values, def_val, lock, binlog_status_arg, on_check_func, NULL)
  {}
  virtual bool global_update(THD *thd, set_var *var);
};


/**
  Class for gtid_next.
*/
class Sys_var_gtid_next: public sys_var
{
public:
  Sys_var_gtid_next(const char *name_arg,
          const char *comment, int flag_args, ptrdiff_t off, size_t size,
          CMD_LINE getopt,
          const char *def_val,
          PolyLock *lock= 0,
          enum binlog_status_enum binlog_status_arg=VARIABLE_NOT_IN_BINLOG,
          on_check_function on_check_func=0,
          on_update_function on_update_func=0,
          const char *substitute=0,
          int parse_flag= PARSE_NORMAL)
    : sys_var(&all_sys_vars, name_arg, comment, flag_args, off, getopt.id,
              getopt.arg_type, SHOW_CHAR, (intptr)def_val,
              lock, binlog_status_arg, on_check_func, on_update_func,
              substitute, parse_flag)
  {
    DBUG_ASSERT(size == sizeof(Gtid_specification));
  }
  bool session_update(THD *thd, set_var *var)
  {
    DBUG_ENTER("Sys_var_gtid_next::session_update");
    char buf[Gtid::MAX_TEXT_LENGTH + 1];
    // Get the value
    String str(buf, sizeof(buf), &my_charset_latin1);
    char* res= NULL;
    if (!var->value)
    {
      // set session gtid_next= default
      DBUG_ASSERT(var->save_result.string_value.str);
      DBUG_ASSERT(var->save_result.string_value.length);
      res= var->save_result.string_value.str;
    }
    else if (var->value->val_str(&str))
      res= var->value->val_str(&str)->c_ptr_safe();
    if (!res)
    {
      my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), name.str, "NULL");
      DBUG_RETURN(true);
    }
    global_sid_lock->rdlock();
    Gtid_specification spec;
    if (spec.parse(global_sid_map, res) != RETURN_STATUS_OK)
    {
      global_sid_lock->unlock();
      DBUG_RETURN(true);
    }

    bool ret= set_gtid_next(thd, spec);
    // set_gtid_next releases global_sid_lock
    DBUG_RETURN(ret);
  }
  bool global_update(THD *thd, set_var *var)
  { DBUG_ASSERT(FALSE); return true; }
  void session_save_default(THD *thd, set_var *var)
  {
    DBUG_ENTER("Sys_var_gtid_next::session_save_default");
    char* ptr= (char*)(intptr)option.def_value;
    var->save_result.string_value.str= ptr;
    var->save_result.string_value.length= ptr ? strlen(ptr) : 0;
    DBUG_VOID_RETURN;
  }
  void global_save_default(THD *thd, set_var *var)
  { DBUG_ASSERT(FALSE); }
  bool do_check(THD *thd, set_var *var)
  { return false; }
  bool check_update_type(Item_result type)
  { return type != STRING_RESULT; }
  uchar *session_value_ptr(THD *running_thd, THD *target_thd, LEX_STRING *base)
  {
    DBUG_ENTER("Sys_var_gtid_next::session_value_ptr");
    char buf[Gtid_specification::MAX_TEXT_LENGTH + 1];
    global_sid_lock->rdlock();
    ((Gtid_specification *)session_var_ptr(target_thd))->
      to_string(global_sid_map, buf);
    global_sid_lock->unlock();
    char *ret= running_thd->mem_strdup(buf);
    DBUG_RETURN((uchar *)ret);
  }
  uchar *global_value_ptr(THD *thd, LEX_STRING *base)
  { DBUG_ASSERT(FALSE); return NULL; }
};

#ifdef HAVE_GTID_NEXT_LIST
/**
  Class for variables that store values of type Gtid_set.

  The back-end storage should be a Gtid_set_or_null, and it should be
  set to null by default.  When the variable is set for the first
  time, the Gtid_set* will be allocated.
*/
class Sys_var_gtid_set: public sys_var
{
public:
  Sys_var_gtid_set(const char *name_arg,
          const char *comment, int flag_args, ptrdiff_t off, size_t size,
          CMD_LINE getopt,
          const char *def_val,
          PolyLock *lock= 0,
          enum binlog_status_enum binlog_status_arg=VARIABLE_NOT_IN_BINLOG,
          on_check_function on_check_func=0,
          on_update_function on_update_func=0,
          const char *substitute=0,
          int parse_flag= PARSE_NORMAL)
    : sys_var(&all_sys_vars, name_arg, comment, flag_args, off, getopt.id,
              getopt.arg_type, SHOW_CHAR, (intptr)def_val,
              lock, binlog_status_arg, on_check_func, on_update_func,
              substitute, parse_flag)
  {
    DBUG_ASSERT(size == sizeof(Gtid_set_or_null));
  }
  bool session_update(THD *thd, set_var *var)
  {
    DBUG_ENTER("Sys_var_gtid_set::session_update");
    Gtid_set_or_null *gsn=
      (Gtid_set_or_null *)session_var_ptr(thd);
    char *value= var->save_result.string_value.str;
    if (value == NULL)
      gsn->set_null();
    else
    {
      Gtid_set *gs= gsn->set_non_null(global_sid_map);
      if (gs == NULL)
      {
        my_error(ER_OUT_OF_RESOURCES, MYF(0)); // allocation failed
        DBUG_RETURN(true);
      }
      /*
        If string begins with '+', add to the existing set, otherwise
        replace existing set.
      */
      while (isspace(*value))
        value++;
      if (*value == '+')
        value++;
      else
        gs->clear();
      // Add specified set of groups to Gtid_set.
      global_sid_lock->rdlock();
      enum_return_status ret= gs->add_gtid_text(value);
      global_sid_lock->unlock();
      if (ret != RETURN_STATUS_OK)
      {
        gsn->set_null();
        DBUG_RETURN(true);
      }
    }
    DBUG_RETURN(false);
  }
  bool global_update(THD *thd, set_var *var)
  { DBUG_ASSERT(FALSE); return true; }
  void session_save_default(THD *thd, set_var *var)
  {
    DBUG_ENTER("Sys_var_gtid_set::session_save_default");
    global_sid_lock->rdlock();
    char *ptr= (char*)(intptr)option.def_value;
    var->save_result.string_value.str= ptr;
    var->save_result.string_value.length= ptr ? strlen(ptr) : 0;
    global_sid_lock->unlock();
    DBUG_VOID_RETURN;
  }
  void global_save_default(THD *thd, set_var *var)
  { DBUG_ASSERT(FALSE); }
  bool do_check(THD *thd, set_var *var)
  {
    DBUG_ENTER("Sys_var_gtid_set::do_check");
    String str;
    String *res= var->value->val_str(&str);
    if (res == NULL)
    {
      var->save_result.string_value.str= NULL;
      DBUG_RETURN(FALSE);
    }
    DBUG_ASSERT(res->ptr() != NULL);
    var->save_result.string_value.str= thd->strmake(res->ptr(), res->length());
    if (var->save_result.string_value.str == NULL)
    {
      my_error(ER_OUT_OF_RESOURCES, MYF(0)); // thd->strmake failed
      DBUG_RETURN(1);
    }
    var->save_result.string_value.length= res->length();
    bool ret= !Gtid_set::is_valid(res->ptr());
    DBUG_RETURN(ret);
  }
  bool check_update_type(Item_result type)
  { return type != STRING_RESULT; }
  uchar *session_value_ptr(THD *running_thd, THD *target_thd, LEX_STRING *base)
  {
    DBUG_ENTER("Sys_var_gtid_set::session_value_ptr");
    Gtid_set_or_null *gsn= (Gtid_set_or_null *)session_var_ptr(target_thd);
    Gtid_set *gs= gsn->get_gtid_set();
    if (gs == NULL)
      DBUG_RETURN(NULL);
    char *buf;
    global_sid_lock->rdlock();
    buf= (char *)running_thd->alloc(gs->get_string_length() + 1);
    if (buf)
      gs->to_string(buf);
    else
      my_error(ER_OUT_OF_RESOURCES, MYF(0)); // thd->alloc failed
    global_sid_lock->unlock();
    DBUG_RETURN((uchar *)buf);
  }
  uchar *global_value_ptr(THD *thd, LEX_STRING *base)
  { DBUG_ASSERT(FALSE); return NULL; }
};
#endif


/**
  Abstract base class for read-only variables (global or session) of
  string type where the value is generated by some function.  This
  needs to be subclassed; the session_value_ptr or global_value_ptr
  function should be overridden.
*/
class Sys_var_charptr_func: public sys_var
{
public:
  Sys_var_charptr_func(const char *name_arg, const char *comment,
                       flag_enum flag_arg)
    : sys_var(&all_sys_vars, name_arg, comment, READ_ONLY flag_arg,
              0/*off*/, NO_CMD_LINE.id, NO_CMD_LINE.arg_type,
              SHOW_CHAR, (intptr)0/*def_val*/,
              NULL/*polylock*/, VARIABLE_NOT_IN_BINLOG,
              NULL/*on_check_func*/, NULL/*on_update_func*/,
              NULL/*substitute*/, PARSE_NORMAL/*parse_flag*/)
  {
    DBUG_ASSERT(flag_arg == sys_var::GLOBAL || flag_arg == sys_var::SESSION ||
                flag_arg == sys_var::ONLY_SESSION);
  }
  bool session_update(THD *thd, set_var *var)
  { DBUG_ASSERT(FALSE); return true; }
  bool global_update(THD *thd, set_var *var)
  { DBUG_ASSERT(FALSE); return true; }
  void session_save_default(THD *thd, set_var *var) { DBUG_ASSERT(FALSE); }
  void global_save_default(THD *thd, set_var *var) { DBUG_ASSERT(FALSE); }
  bool do_check(THD *thd, set_var *var) { DBUG_ASSERT(FALSE); return true; }
  bool check_update_type(Item_result type) { DBUG_ASSERT(FALSE); return true; }
  virtual uchar *session_value_ptr(THD *running_thd, THD *target_thd, LEX_STRING *base)
  { DBUG_ASSERT(FALSE); return NULL; }
  virtual uchar *global_value_ptr(THD *thd, LEX_STRING *base)
  { DBUG_ASSERT(FALSE); return NULL; }
};


/**
  Class for @@session.gtid_executed and @@global.gtid_executed.
*/
class Sys_var_gtid_executed : Sys_var_charptr_func
{
public:
  Sys_var_gtid_executed(const char *name_arg, const char *comment_arg)
    : Sys_var_charptr_func(name_arg, comment_arg, SESSION) {}

  uchar *global_value_ptr(THD *thd, LEX_STRING *base)
  {
    DBUG_ENTER("Sys_var_gtid_executed::global_value_ptr");
    global_sid_lock->wrlock();
    const Gtid_set *gs= gtid_state->get_executed_gtids();
    char *buf= (char *)thd->alloc(gs->get_string_length() + 1);
    if (buf == NULL)
      my_error(ER_OUT_OF_RESOURCES, MYF(0));
    else
      gs->to_string(buf);
    global_sid_lock->unlock();
    DBUG_RETURN((uchar *)buf);
  }

public:
  uchar *session_value_ptr(THD *running_thd, THD *target_thd, LEX_STRING *base)
  {
    DBUG_ENTER("Sys_var_gtid_executed::session_value_ptr");

    if (!running_thd->gtid_executed_warning_issued)
    {
      push_warning_printf(running_thd, Sql_condition::SL_WARNING,
          ER_WARN_DEPRECATED_SYNTAX_NO_REPLACEMENT,
          ER(ER_WARN_DEPRECATED_SYNTAX_NO_REPLACEMENT),
          "@@SESSION.GTID_EXECUTED");
      running_thd->gtid_executed_warning_issued= true;
    }
    if (opt_bin_log &&
       (target_thd == running_thd)) /* Supported for current thread only. */
    {
      target_thd->binlog_setup_trx_data();
      /*
        It is enough to check if the transaction cache is nonempty, we
        do not need to check the statement cache.  The statement cache
        can only be nonempty for single-statement transactions.  Since
        single-statement transactions are flushed to the binary log
        before the next statement gets to execute, there is no chance
        that the value of @@session.gtid_executed is read when the
        statement cache is nonempty.
      */
      if (!target_thd->is_binlog_cache_empty(true))
      {
        /*
          The case sidno == -1 cannot happen since it is not implemented.
          The cases sidno == 0 and sidno == -2 mean that we don't own any
          GTID, so we should return empty string.
        */
        if (target_thd->owned_gtid.sidno > 0)
        {
          uchar *buf= (uchar *)running_thd->alloc(Gtid::MAX_TEXT_LENGTH + 1);
          if (buf == NULL)
            my_error(ER_OUT_OF_RESOURCES, MYF(0));
          else
            target_thd->owned_gtid.to_string(target_thd->owned_sid, (char *)buf);
          DBUG_RETURN(buf);
        }
      }
    }

    uchar *buf= (uchar *)running_thd->alloc(1);
    if (buf == NULL)
      my_error(ER_OUT_OF_RESOURCES, MYF(0));
    else
      buf[0]= 0;
    DBUG_RETURN(buf);
  }
};


/**
  Class for @@session.gtid_purged.
*/
class Sys_var_gtid_purged : public sys_var
{
public:
  Sys_var_gtid_purged(const char *name_arg,
          const char *comment, int flag_args, ptrdiff_t off, size_t size,
          CMD_LINE getopt,
          const char *def_val,
          PolyLock *lock= 0,
          enum binlog_status_enum binlog_status_arg=VARIABLE_NOT_IN_BINLOG,
          on_check_function on_check_func=0,
          on_update_function on_update_func=0,
          const char *substitute=0,
          int parse_flag= PARSE_NORMAL)
    : sys_var(&all_sys_vars, name_arg, comment, flag_args, off, getopt.id,
              getopt.arg_type, SHOW_CHAR, (intptr)def_val,
              lock, binlog_status_arg, on_check_func, on_update_func,
              substitute, parse_flag)
  {}

  bool session_update(THD *thd, set_var *var)
  {
    DBUG_ASSERT(FALSE);
    return true;
  }

  void session_save_default(THD *thd, set_var *var)
  { DBUG_ASSERT(FALSE); }

  bool global_update(THD *thd, set_var *var);

  void global_save_default(THD *thd, set_var *var)
  {
    /* gtid_purged does not have default value */
    my_error(ER_NO_DEFAULT, MYF(0), var->var->name.str);
  }

  bool do_check(THD *thd, set_var *var)
  {
    DBUG_ENTER("Sys_var_gtid_purged::do_check");
    char buf[1024];
    String str(buf, sizeof(buf), system_charset_info);
    String *res= var->value->val_str(&str);
    if (!res)
      DBUG_RETURN(true);
    var->save_result.string_value.str= thd->strmake(res->c_ptr_safe(),
                                                    res->length());
    if (!var->save_result.string_value.str)
    {
      my_error(ER_OUT_OF_RESOURCES, MYF(0)); // thd->strmake failed
      DBUG_RETURN(true);
    }
    var->save_result.string_value.length= res->length();
    bool ret= Gtid_set::is_valid(res->c_ptr_safe()) ? false : true;
    DBUG_PRINT("info", ("ret=%d", ret));
    DBUG_RETURN(ret);
  }

  bool check_update_type(Item_result type)
  { return type != STRING_RESULT; }

  uchar *global_value_ptr(THD *thd, LEX_STRING *base)
  {
    DBUG_ENTER("Sys_var_gtid_purged::global_value_ptr");
    const Gtid_set *gs;
    global_sid_lock->wrlock();
    if (opt_bin_log)
      gs= gtid_state->get_lost_gtids();
    else
      /*
        When binlog is off, report @@GLOBAL.GTID_PURGED from
        executed_gtids, since @@GLOBAL.GTID_PURGED and
        @@GLOBAL.GTID_EXECUTED are always same, so we did not
        save gtid into lost_gtids for every transaction for
        improving performance.
      */
      gs= gtid_state->get_executed_gtids();
    char *buf= (char *)thd->alloc(gs->get_string_length() + 1);
    if (buf == NULL)
      my_error(ER_OUT_OF_RESOURCES, MYF(0));
    else
      gs->to_string(buf);
    global_sid_lock->unlock();
    DBUG_RETURN((uchar *)buf);
  }

  uchar *session_value_ptr(THD *running_thd, THD *target_thd, LEX_STRING *base)
  { DBUG_ASSERT(0); return NULL; }
};


class Sys_var_gtid_owned : Sys_var_charptr_func
{
public:
  Sys_var_gtid_owned(const char *name_arg, const char *comment_arg)
    : Sys_var_charptr_func(name_arg, comment_arg, SESSION) {}

public:
  uchar *session_value_ptr(THD *running_thd, THD *target_thd, LEX_STRING *base)
  {
    DBUG_ENTER("Sys_var_gtid_owned::session_value_ptr");
    char *buf= NULL;
    bool remote= (target_thd != running_thd);

    if (target_thd->owned_gtid.sidno == 0)
      DBUG_RETURN((uchar *)running_thd->mem_strdup(""));
    else if (target_thd->owned_gtid.sidno == THD::OWNED_SIDNO_ANONYMOUS)
    {
      DBUG_ASSERT(gtid_state->get_anonymous_ownership_count() > 0);
      DBUG_RETURN((uchar *)running_thd->mem_strdup("ANONYMOUS"));
    }
    else if (target_thd->owned_gtid.sidno == THD::OWNED_SIDNO_GTID_SET)
    {
#ifdef HAVE_GTID_NEXT_LIST
      buf= (char *)running_thd->alloc(target_thd->owned_gtid_set.get_string_length() + 1);
      if (buf)
      {
        global_sid_lock->rdlock();
        target_thd->owned_gtid_set.to_string(buf);
        global_sid_lock->unlock();
      }
      else
        my_error(ER_OUT_OF_RESOURCES, MYF(0));
#else
     DBUG_ASSERT(0); 
#endif
    }
    else
    {
      buf= (char *)running_thd->alloc(Gtid::MAX_TEXT_LENGTH + 1);
      if (buf)
      {
        /* Take the lock if accessing another session. */
        if (remote)
          global_sid_lock->rdlock();
        running_thd->owned_gtid.to_string(target_thd->owned_sid, buf);
        if (remote)
          global_sid_lock->unlock();
      }
      else
        my_error(ER_OUT_OF_RESOURCES, MYF(0));
    }
    DBUG_RETURN((uchar *)buf);
  }

  uchar *global_value_ptr(THD *thd, LEX_STRING *base)
  {
    DBUG_ENTER("Sys_var_gtid_owned::global_value_ptr");
    const Owned_gtids *owned_gtids= gtid_state->get_owned_gtids();
    global_sid_lock->wrlock();
    char *buf= (char *)thd->alloc(owned_gtids->get_max_string_length());
    if (buf)
      owned_gtids->to_string(buf);
    else
      my_error(ER_OUT_OF_RESOURCES, MYF(0)); // thd->alloc failed
    global_sid_lock->unlock();
    DBUG_RETURN((uchar *)buf);
  }
};

#ifdef HAVE_REPLICATION
class Sys_var_gtid_mode : public Sys_var_enum
{
public:
  Sys_var_gtid_mode(const char *name_arg,
                    const char *comment,
                    int flag_args,
                    ptrdiff_t off,
                    size_t size,
                    CMD_LINE getopt,
                    const char *values[],
                    uint def_val,
                    PolyLock *lock=0,
                    enum binlog_status_enum binlog_status_arg=VARIABLE_NOT_IN_BINLOG,
                    on_check_function on_check_func=0) :
                      Sys_var_enum(name_arg, comment, flag_args, off, size,
                                   getopt, values, def_val, lock, binlog_status_arg,
                                   on_check_func)
  { }

  bool global_update(THD *thd, set_var *var)
  {
    DBUG_ENTER("Sys_var_gtid_mode::global_update");
    bool ret= true;

     /*
      SET GITD_MODE command should ignore 'read-only' and 'super_read_only'
      options so that it can update 'mysql.gtid_executed' replication repository
      table.
     */
    thd->set_skip_readonly_check();

    /*
      Hold lock_log so that:
      - other transactions are not flushed while gtid_mode is changed;
      - gtid_mode is not changed while some other thread is rotating
        the binlog.

      Hold channel_map lock so that:
      - gtid_mode is not changed during the execution of some
        replication command; particularly CHANGE MASTER. CHANGE MASTER
        checks if GTID_MODE is compatible with AUTO_POSITION, and
        later it actually updates the in-memory structure for
        AUTO_POSITION.  If gtid_mode was changed between these calls,
        auto_position could be set incompatible with gtid_mode.

      Hold global_sid_lock.wrlock so that:
      - other transactions cannot acquire ownership of any gtid.

      Hold gtid_mode_lock so that all places that don't want to hold
      any of the other locks, but want to read gtid_mode, don't need
      to take the other locks.
    */
    gtid_mode_lock->wrlock();
    channel_map.wrlock();
    mysql_mutex_lock(mysql_bin_log.get_log_lock());
    global_sid_lock->wrlock();
    int lock_count= 4;

    enum_gtid_mode new_gtid_mode=
      (enum_gtid_mode)var->save_result.ulonglong_value;
    enum_gtid_mode old_gtid_mode= get_gtid_mode(GTID_MODE_LOCK_SID);
    DBUG_ASSERT(new_gtid_mode <= GTID_MODE_ON);

    DBUG_PRINT("info", ("old_gtid_mode=%d new_gtid_mode=%d",
                        old_gtid_mode, new_gtid_mode));

    if (new_gtid_mode == old_gtid_mode)
      goto end;

    // Can only change one step at a time.
    if (abs((int)new_gtid_mode - (int)old_gtid_mode) > 1)
    {
      my_error(ER_GTID_MODE_CAN_ONLY_CHANGE_ONE_STEP_AT_A_TIME, MYF(0));
      goto err;
    }

    // Not allowed with slave_sql_skip_counter
    DBUG_PRINT("info", ("sql_slave_skip_counter=%d", sql_slave_skip_counter));
    if (new_gtid_mode == GTID_MODE_ON && sql_slave_skip_counter > 0)
    {
      my_error(ER_CANT_SET_GTID_MODE, MYF(0), "ON",
               "@@GLOBAL.SQL_SLAVE_SKIP_COUNTER is greater than zero");
      goto err;
    }

    // Cannot set OFF when some channel uses AUTO_POSITION.
    if (new_gtid_mode == GTID_MODE_OFF)
    {
      for (mi_map::iterator it= channel_map.begin(); it!= channel_map.end(); it++)
      {
        Master_info *mi= it->second;
        DBUG_PRINT("info", ("auto_position for channel '%s' is %d",
                            mi->get_channel(), mi->is_auto_position()));
        if (mi != NULL && mi->is_auto_position())
        {
          char buf[1024];
          sprintf(buf, "replication channel '%.192s' is configured "
                  "in AUTO_POSITION mode. Execute "
                  "CHANGE MASTER TO MASTER_AUTO_POSITION = 0 "
                  "FOR CHANNEL '%.192s' before you set "
                  "@@GLOBAL.GTID_MODE = OFF.",
                  mi->get_channel(), mi->get_channel());
          my_error(ER_CANT_SET_GTID_MODE, MYF(0), "OFF", buf);
          goto err;
        }
      }
    }

    // Can't set GTID_MODE != ON when group replication is enabled.
    if (is_group_replication_running())
    {
      DBUG_ASSERT(old_gtid_mode == GTID_MODE_ON);
      DBUG_ASSERT(new_gtid_mode == GTID_MODE_ON_PERMISSIVE);
      my_error(ER_CANT_SET_GTID_MODE, MYF(0),
               get_gtid_mode_string(new_gtid_mode),
               "group replication requires @@GLOBAL.GTID_MODE=ON");
      goto err;
    }

    // Compatible with ongoing transactions.
    DBUG_PRINT("info", ("anonymous_ownership_count=%d owned_gtids->is_empty=%d",
                        gtid_state->get_anonymous_ownership_count(),
                        gtid_state->get_owned_gtids()->is_empty()));
    gtid_state->get_owned_gtids()->dbug_print("global owned_gtids");
    if (new_gtid_mode == GTID_MODE_ON &&
        gtid_state->get_anonymous_ownership_count() > 0)
    {
      my_error(ER_CANT_SET_GTID_MODE, MYF(0), "ON",
               "there are ongoing, anonymous transactions. Before "
               "setting @@GLOBAL.GTID_MODE = ON, wait until "
               "SHOW STATUS LIKE 'ANONYMOUS_TRANSACTION_COUNT' "
               "shows zero on all servers. Then wait for all "
               "existing, anonymous transactions to replicate to "
               "all slaves, and then execute "
               "SET @@GLOBAL.GTID_MODE = ON on all servers. "
               "See the Manual for details");
      goto err;
    }

    if (new_gtid_mode == GTID_MODE_OFF &&
        !gtid_state->get_owned_gtids()->is_empty())
    {
      my_error(ER_CANT_SET_GTID_MODE, MYF(0), "OFF",
               "there are ongoing transactions that have a GTID. "
               "Before you set @@GLOBAL.GTID_MODE = OFF, wait "
               "until SELECT @@GLOBAL.GTID_OWNED is empty on all "
               "servers. Then wait for all GTID-transactions to "
               "replicate to all servers, and then execute "
               "SET @@GLOBAL.GTID_MODE = OFF on all servers. "
               "See the Manual for details");
      goto err;
    }

    // Compatible with ongoing GTID-violating transactions
    DBUG_PRINT("info", ("automatic_gtid_violating_transaction_count=%d",
                        gtid_state->get_automatic_gtid_violating_transaction_count()));
    if (new_gtid_mode >= GTID_MODE_ON_PERMISSIVE &&
        gtid_state->get_automatic_gtid_violating_transaction_count() > 0)
    {
      my_error(ER_CANT_SET_GTID_MODE, MYF(0), "ON_PERMISSIVE",
               "there are ongoing transactions that use "
               "GTID_NEXT = 'AUTOMATIC', which violate GTID "
               "consistency. Adjust your workload to be "
               "GTID-consistent before setting "
               "@@GLOBAL.GTID_MODE = ON_PERMISSIVE. "
               "See the Manual for "
               "@@GLOBAL.ENFORCE_GTID_CONSISTENCY for details");
      goto err;
    }

    // Compatible with ENFORCE_GTID_CONSISTENCY.
    if (new_gtid_mode == GTID_MODE_ON &&
        get_gtid_consistency_mode() != GTID_CONSISTENCY_MODE_ON)
    {
      my_error(ER_CANT_SET_GTID_MODE, MYF(0), "ON",
               "ENFORCE_GTID_CONSISTENCY is not ON");
      goto err;
    }

    // Can't set GTID_MODE=OFF with ongoing calls to
    // WAIT_FOR_EXECUTED_GTID_SET or
    // WAIT_UNTIL_SQL_THREAD_AFTER_GTIDS.
    DBUG_PRINT("info", ("gtid_wait_count=%d", gtid_state->get_gtid_wait_count() > 0));
    if (new_gtid_mode == GTID_MODE_OFF &&
        gtid_state->get_gtid_wait_count() > 0)
    {
      my_error(ER_CANT_SET_GTID_MODE, MYF(0), "OFF",
               "there are ongoing calls to "
               "WAIT_FOR_EXECUTED_GTID_SET or "
               "WAIT_UNTIL_SQL_THREAD_AFTER_GTIDS. Before you set "
               "@@GLOBAL.GTID_MODE = OFF, ensure that no other "
               "client is waiting for GTID-transactions to be "
               "committed");
      goto err;
    }

    // Update the mode
    global_var(ulong)= new_gtid_mode;
    global_sid_lock->unlock();
    lock_count= 3;

    // Generate note in log
    sql_print_information("Changed GTID_MODE from %s to %s.",
                          gtid_mode_names[old_gtid_mode],
                          gtid_mode_names[new_gtid_mode]);

    // Rotate
    {
      bool dont_care= false;
      if (mysql_bin_log.rotate(true, &dont_care))
        goto err;
    }

end:
    ret= false;
err:
    DBUG_ASSERT(lock_count >= 0);
    DBUG_ASSERT(lock_count <= 4);
    if (lock_count == 4)
      global_sid_lock->unlock();
    mysql_mutex_unlock(mysql_bin_log.get_log_lock());
    channel_map.unlock();
    gtid_mode_lock->unlock();
    DBUG_RETURN(ret);
  }
};

#endif /* HAVE_REPLICATION */


class Sys_var_enforce_gtid_consistency : public Sys_var_multi_enum
{
public:
  Sys_var_enforce_gtid_consistency(
    const char *name_arg,
    const char *comment,
    int flag_args,
    ptrdiff_t off,
    size_t size,
    CMD_LINE getopt,
    const ALIAS aliases[],
    const uint value_count,
    uint def_val,
    uint command_line_no_value,
    PolyLock *lock=0,
    enum binlog_status_enum binlog_status_arg=VARIABLE_NOT_IN_BINLOG,
    on_check_function on_check_func=0) :
  Sys_var_multi_enum(name_arg, comment, flag_args, off, size, getopt,
               aliases, value_count, def_val, command_line_no_value,
               lock, binlog_status_arg, on_check_func)
  { }

  bool global_update(THD *thd, set_var *var)
  {
    DBUG_ENTER("Sys_var_enforce_gtid_consistency::global_update");
    bool ret= true;

    /*
      Hold global_sid_lock.wrlock so that other transactions cannot
      acquire ownership of any gtid.
    */
    global_sid_lock->wrlock();

    DBUG_PRINT("info", ("var->save_result.ulonglong_value=%llu",
                        var->save_result.ulonglong_value));
    enum_gtid_consistency_mode new_mode=
      (enum_gtid_consistency_mode)var->save_result.ulonglong_value;
    enum_gtid_consistency_mode old_mode= get_gtid_consistency_mode();
    enum_gtid_mode gtid_mode= get_gtid_mode(GTID_MODE_LOCK_SID);

    DBUG_ASSERT(new_mode <= GTID_CONSISTENCY_MODE_WARN);

    DBUG_PRINT("info", ("old enforce_gtid_consistency=%d "
                        "new enforce_gtid_consistency=%d "
                        "gtid_mode=%d ",
                        old_mode, new_mode, gtid_mode));

    if (new_mode == old_mode)
      goto end;

    // Can't turn off GTID-consistency when GTID_MODE=ON.
    if (new_mode != GTID_CONSISTENCY_MODE_ON && gtid_mode == GTID_MODE_ON)
    {
      my_error(ER_GTID_MODE_ON_REQUIRES_ENFORCE_GTID_CONSISTENCY_ON, MYF(0));
      goto err;
    }
    // If there are ongoing GTID-violating transactions, and we are
    // moving from OFF->ON, WARN->ON, or OFF->WARN, generate warning
    // or error accordingly.
    if (new_mode == GTID_CONSISTENCY_MODE_ON ||
        (old_mode == GTID_CONSISTENCY_MODE_OFF &&
         new_mode == GTID_CONSISTENCY_MODE_WARN))
    {
      DBUG_PRINT("info",
                 ("automatic_gtid_violating_transaction_count=%d "
                  "anonymous_gtid_violating_transaction_count=%d",
                  gtid_state->get_automatic_gtid_violating_transaction_count(),
                  gtid_state->get_anonymous_gtid_violating_transaction_count()));
      if (gtid_state->get_automatic_gtid_violating_transaction_count() > 0 ||
          gtid_state->get_anonymous_gtid_violating_transaction_count() > 0)
      {
        if (new_mode == GTID_CONSISTENCY_MODE_ON)
        {
          my_error(ER_CANT_SET_ENFORCE_GTID_CONSISTENCY_ON_WITH_ONGOING_GTID_VIOLATING_TRANSACTIONS, MYF(0));
          goto err;
        }
        else
        {
          push_warning_printf(thd, Sql_condition::SL_WARNING,
                              ER_SET_ENFORCE_GTID_CONSISTENCY_WARN_WITH_ONGOING_GTID_VIOLATING_TRANSACTIONS,
                              "%s", ER(ER_SET_ENFORCE_GTID_CONSISTENCY_WARN_WITH_ONGOING_GTID_VIOLATING_TRANSACTIONS));
        }
      }
    }

    // Update the mode
    global_var(ulong)= new_mode;

    // Generate note in log
    sql_print_information("Changed ENFORCE_GTID_CONSISTENCY from %s to %s.",
                          get_gtid_consistency_mode_string(old_mode),
                          get_gtid_consistency_mode_string(new_mode));

end:
    ret= false;
err:
    global_sid_lock->unlock();
    DBUG_RETURN(ret);
  }
};

#endif /* SYS_VARS_H_INCLUDED */

