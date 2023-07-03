#ifndef SYS_VARS_H_INCLUDED
#define SYS_VARS_H_INCLUDED
<<<<<<< HEAD
/* Copyright (c) 2002, 2022, Oracle and/or its affiliates.
=======
/* Copyright (c) 2002, 2023, Oracle and/or its affiliates.
>>>>>>> pr/231

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file
  "private" interface to sys_var - server configuration variables.

  This header is included only by the file that contains declarations
  of sys_var variables (sys_vars.cc).
*/

#include "my_config.h"

#include <sys/types.h>

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <string_view>

#include "keycache.h"  // dflt_key_cache
#include "lex_string.h"
#include "m_ctype.h"
#include "my_base.h"
#include "my_bit.h"  // my_count_bits
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_getopt.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "mysql/plugin.h"
#include "mysql/service_mysql_alloc.h"
#include "mysql/status_var.h"
#include "mysql/udf_registration_types.h"
#include "mysqld_error.h"
#include "sql/auth/sql_security_ctx.h"
#include "sql/debug_sync.h"  // debug_sync_update
#include "sql/handler.h"
#include "sql/item.h"       // Item
#include "sql/keycaches.h"  // default_key_cache_base
#include "sql/mysqld.h"     // max_system_variables
#include "sql/rpl_gtid.h"
#include "sql/set_var.h"    // sys_var_chain
#include "sql/sql_class.h"  // THD
#include "sql/sql_connect.h"
#include "sql/sql_const.h"
#include "sql/sql_error.h"
#include "sql/sql_plugin.h"  // my_plugin_lock_by_name
#include "sql/sql_plugin_ref.h"
#include "sql/strfunc.h"  // find_type
#include "sql/sys_vars_resource_mgr.h"
#include "sql/sys_vars_shared.h"  // throw_bounds_warning
#include "sql/tztime.h"           // Time_zone
#include "sql_string.h"
#include "typelib.h"

class Sys_var_bit;
class Sys_var_bool;
class Sys_var_charptr;
class Sys_var_double;
class Sys_var_enforce_gtid_consistency;
class Sys_var_enum;
class Sys_var_flagset;
class Sys_var_gtid_mode;
class Sys_var_have;
class Sys_var_lexstring;
class Sys_var_multi_enum;
class Sys_var_plugin;
class Sys_var_set;
class Sys_var_tz;
struct CMD_LINE;
struct System_variables;
template <typename Struct_type, typename Name_getter>
class Sys_var_struct;
template <typename T, ulong ARGT, enum enum_mysql_show_type SHOWT, bool SIGNED>
class Sys_var_integer;

constexpr const unsigned long TABLE_OPEN_CACHE_DEFAULT{4000};
constexpr const unsigned long TABLE_DEF_CACHE_DEFAULT{400};
/**
  Maximum number of connections default value.
  151 is larger than Apache's default max children,
  to avoid "too many connections" error in a common setup.
*/
constexpr const unsigned long MAX_CONNECTIONS_DEFAULT{151};

/*
  a set of mostly trivial (as in f(X)=X) defines below to make system variable
  declarations more readable
*/
#define VALID_RANGE(X, Y) X, Y
#define DEFAULT(X) X
#define BLOCK_SIZE(X) X
#define GLOBAL_VAR(X)                                                         \
  sys_var::GLOBAL, (((const char *)&(X)) - (char *)&global_system_variables), \
      sizeof(X)
#define SESSION_VAR(X)                             \
  sys_var::SESSION, offsetof(System_variables, X), \
      sizeof(((System_variables *)0)->X)
#define SESSION_ONLY(X)                                 \
  sys_var::ONLY_SESSION, offsetof(System_variables, X), \
      sizeof(((System_variables *)0)->X)
#define NO_CMD_LINE CMD_LINE(NO_ARG, -1)
/*
  the define below means that there's no *second* mutex guard,
  LOCK_global_system_variables always guards all system variables
*/
#define NO_MUTEX_GUARD ((PolyLock *)0)
#define IN_BINLOG sys_var::SESSION_VARIABLE_IN_BINLOG
#define NOT_IN_BINLOG sys_var::VARIABLE_NOT_IN_BINLOG
#define ON_READ(X) X
#define ON_CHECK(X) X
#define PRE_UPDATE(X) X
#define ON_UPDATE(X) X
#define READ_ONLY sys_var::READONLY +
#define NOT_VISIBLE sys_var::INVISIBLE +
#define UNTRACKED_DEFAULT sys_var::TRI_LEVEL +
#define HINT_UPDATEABLE sys_var::HINT_UPDATEABLE +
// this means that Sys_var_charptr initial value was malloc()ed
#define PREALLOCATED sys_var::ALLOCATED +
#define NON_PERSIST sys_var::NOTPERSIST +
#define PERSIST_AS_READONLY sys_var::PERSIST_AS_READ_ONLY +
#define SENSITIVE sys_var::SENSITIVE +

/*
  Sys_var_bit meaning is reversed, like in
  @@foreign_key_checks <-> OPTION_NO_FOREIGN_KEY_CHECKS
*/
#define REVERSE(X) ~(X)
#define DEPRECATED_VAR(X) X

#define session_var(THD, TYPE) (*(TYPE *)session_var_ptr(THD))
#define global_var(TYPE) (*(TYPE *)global_var_ptr())

#define GET_HA_ROWS GET_ULL

extern sys_var_chain all_sys_vars;

enum charset_enum { IN_SYSTEM_CHARSET, IN_FS_CHARSET };

static const char *bool_values[3] = {"OFF", "ON", nullptr};

const char *fixup_enforce_gtid_consistency_command_line(char *value_arg);

/**
  A small wrapper class to pass getopt arguments as a pair
  to the Sys_var_* constructors. It improves type safety and helps
  to catch errors in the argument order.
*/
struct CMD_LINE {
  int id;
  enum get_opt_arg_type arg_type;
  CMD_LINE(enum get_opt_arg_type getopt_arg_type, int getopt_id = 0)
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
// clang-format off
template <typename T, ulong ARGT, enum enum_mysql_show_type SHOWT, bool SIGNED>
class Sys_var_integer : public sys_var {
 public:
  Sys_var_integer(
      const char *name_arg, const char *comment, int flag_args,
      ptrdiff_t off, size_t size [[maybe_unused]], CMD_LINE getopt,
      T min_val, T max_val, T def_val, uint block_size,
      PolyLock *lock = nullptr,
      enum binlog_status_enum binlog_status_arg = VARIABLE_NOT_IN_BINLOG,
      on_check_function on_check_func = nullptr,
      on_update_function on_update_func = nullptr,
      const char *substitute = nullptr, int parse_flag = PARSE_NORMAL)
      : sys_var(&all_sys_vars, name_arg, comment, flag_args, off,
                getopt.id, getopt.arg_type, SHOWT, def_val, lock,
                binlog_status_arg, on_check_func, on_update_func,
                substitute, parse_flag) {
    option.var_type = ARGT;
    if ((min_val % block_size) != 0)
      min_val += block_size - (min_val % block_size);
    option.min_value = min_val;
    option.max_value = max_val - (max_val % block_size);
    option.block_size = block_size;
    option.u_max_value = (uchar **)max_var_ptr();
    if (max_var_ptr()) * max_var_ptr() = max_val;

    // Do not set global_var for Sys_var_keycache objects
    if (offset >= 0) global_var(T) = def_val;

    assert(size == sizeof(T));
    assert(min_val <= def_val);
    assert(def_val <= max_val);
    assert(block_size > 0);
    assert(option.min_value % block_size == 0);
    assert(def_val % block_size == 0);
    assert(option.max_value % block_size == 0);
  }
  bool do_check(THD *thd, set_var *var) override {
    bool fixed = false;
    longlong v;
    ulonglong uv;

<<<<<<< HEAD
    v = var->value->val_int();
    if (SIGNED) { /* target variable has signed type */
      if (var->value->unsigned_flag) {
        /*
          Input value is such a large positive number that MySQL used
          an unsigned item to hold it. When cast to a signed longlong,
          if the result is negative there is "cycling" and this is
          incorrect (large positive input value should not end up as a
          large negative value in the session signed variable to be
          set); instead, we need to pick the allowed number closest to
          the positive input value, i.e. pick the biggest allowed
          positive integer.
        */
        if (v < 0)
          uv = max_of_int_range(ARGT);
        else /* no cycling, longlong can hold true value */
          uv = (ulonglong)v;
      } else
        uv = v;
      /* This will further restrict with VALID_RANGE, BLOCK_SIZE */
      var->save_result.ulonglong_value =
=======
  v = var->value->val_int();
  if (SIGNED) /* target variable has signed type */
  {
<<<<<<< HEAD
    if (var->value->unsigned_flag) {
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
        uv = max_of_int_range(ARGT);
      else /* no cycling, longlong can hold true value */
        uv = (ulonglong)v;
    } else
      uv = v;
    /* This will further restrict with VALID_RANGE, BLOCK_SIZE */
    var->save_result.ulonglong_value =
=======
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

    assert(size == sizeof(T));
    assert(min_val < max_val);
    assert(min_val <= def_val);
    assert(max_val >= def_val);
    assert(block_size > 0);
    assert(def_val % block_size == 0);
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
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231
        getopt_ll_limit_value(uv, &option, &fixed);
    } else {
      if (var->value->unsigned_flag) {
        /* Guaranteed positive input value, ulonglong can hold it */
        uv = (ulonglong)v;
      } else {
        /*
          Maybe negative input value; in this case, cast to ulonglong
          makes it positive, which is wrong. Pick the closest allowed
          value i.e. 0.
        */
        uv = (ulonglong)(v < 0 ? 0 : v);
      }
      var->save_result.ulonglong_value =
        getopt_ull_limit_value(uv, &option, &fixed);
    }

    if (max_var_ptr()) {
      /* check constraint set with --maximum-...=X */
      if (SIGNED) {
        longlong max_val = *max_var_ptr();
        if (((longlong)(var->save_result.ulonglong_value)) > max_val)
          var->save_result.ulonglong_value = max_val;
        /*
          Signed variable probably has some kind of symmetry. Then
          it's good to limit negative values just as we limit positive
          values.
        */
        max_val = -max_val;
        if (((longlong)(var->save_result.ulonglong_value)) < max_val)
          var->save_result.ulonglong_value = max_val;
      } else {
        ulonglong max_val = *max_var_ptr();
        if (var->save_result.ulonglong_value > max_val)
          var->save_result.ulonglong_value = max_val;
      }
    }

    return throw_bounds_warning(
        thd, name.str, var->save_result.ulonglong_value != (ulonglong)v,
        var->value->unsigned_flag, v);
  }
  bool session_update(THD *thd, set_var *var) override {
    session_var(thd, T) = static_cast<T>(var->save_result.ulonglong_value);
    return false;
  }
  bool global_update(THD *, set_var *var) override {
    global_var(T) = static_cast<T>(var->save_result.ulonglong_value);
    return false;
  }
  bool check_update_type(Item_result type) override {
    return type != INT_RESULT;
  }
  void session_save_default(THD *thd, set_var *var) override {
    var->save_result.ulonglong_value = static_cast<ulonglong>(
      *pointer_cast<const T *>(global_value_ptr(thd, {})));
  }
  void global_save_default(THD *, set_var *var) override {
    var->save_result.ulonglong_value = option.def_value;
  }
  void saved_value_to_string(THD *, set_var *var, char *def_val) override {
    if (SIGNED)
      longlong10_to_str((longlong)var->save_result.ulonglong_value,
                        def_val, -10);
    else
      longlong10_to_str((longlong)var->save_result.ulonglong_value,
                        def_val, 10);
  }

 private:
  T *max_var_ptr() {
    return scope() == SESSION
        ? (T *)(((uchar *)&max_system_variables) + offset)
        : nullptr;
  }
};
// clang-format on

typedef Sys_var_integer<int32, GET_UINT, SHOW_INT, false> Sys_var_int32;
typedef Sys_var_integer<uint, GET_UINT, SHOW_INT, false> Sys_var_uint;
typedef Sys_var_integer<ulong, GET_ULONG, SHOW_LONG, false> Sys_var_ulong;
typedef Sys_var_integer<ha_rows, GET_HA_ROWS, SHOW_HA_ROWS, false>
    Sys_var_harows;
typedef Sys_var_integer<ulonglong, GET_ULL, SHOW_LONGLONG, false>
    Sys_var_ulonglong;
typedef Sys_var_integer<long, GET_LONG, SHOW_SIGNED_LONG, true> Sys_var_long;

/**
  A sys_var that is an alias for another sys_var.

  The two variables effectively share (almost) all members, so
  whenever you change one of them, it affects both.

  Usually you want to use Sys_var_deprecated_alias instead.
*/
class Sys_var_alias : public sys_var {
 private:
  sys_var &m_base_var;

 protected:
  /**
    Special constructor used to implement Sys_var_deprecated alias.

    @param name_arg The name of this sys_var.

    @param base_var The "parent" sys_var that this sys_var is an alias
    for.

    @param deprecation_substitute_arg The deprecation_substitute to
    use for this variable. While other fields in the created variable
    are inherited from real_var, the deprecation_substitute can be set
    using this parameter.

    @param persisted_alias When this variable is persisted, it will
    duplicate the entry in the persisted variables file: It will be
    stored both using the variable name name_arg, and the name of
    persisted_alias.

    @param is_persisted_deprecated If true, this variable is
    deprecated when appearing in the persisted variables file.
  */
  Sys_var_alias(const char *name_arg, sys_var &base_var,
                const char *deprecation_substitute_arg,
                sys_var *persisted_alias, bool is_persisted_deprecated)
      : sys_var(&all_sys_vars, name_arg, base_var.option.comment,
                base_var.flags, base_var.offset, base_var.option.id,
                base_var.option.arg_type, base_var.show_val_type,
                base_var.option.def_value, base_var.guard,
                base_var.binlog_status, base_var.on_check, base_var.on_update,
                deprecation_substitute_arg, base_var.m_parse_flag,
                persisted_alias, is_persisted_deprecated),
        m_base_var(base_var) {
    option = base_var.option;
    option.name = name_arg;
  }

 public:
  Sys_var_alias(const char *name_arg, sys_var &base_var)
      : Sys_var_alias(name_arg, base_var, base_var.deprecation_substitute,
                      nullptr, false) {}

  sys_var &get_base_var() { return m_base_var; }

  virtual void cleanup() override { m_base_var.cleanup(); }
  virtual sys_var_pluginvar *cast_pluginvar() override {
    return m_base_var.cast_pluginvar();
  }
  virtual void update_default(longlong new_def_value) override {
    m_base_var.update_default(new_def_value);
  }
  virtual longlong get_default() override { return m_base_var.get_default(); }
  virtual longlong get_min_value() override {
    return m_base_var.get_min_value();
  }
  virtual ulonglong get_max_value() override {
    return m_base_var.get_max_value();
  }
  virtual ulong get_var_type() override { return m_base_var.get_var_type(); }
  virtual void set_arg_source(get_opt_arg_source *arg_source) override {
    m_base_var.set_arg_source(arg_source);
  }
  virtual void set_is_plugin(bool is_plugin) override {
    m_base_var.set_is_plugin(is_plugin);
  }
  virtual bool is_non_persistent() override {
    return m_base_var.is_non_persistent();
  }
  virtual void saved_value_to_string(THD *thd, set_var *var,
                                     char *def_val) override {
    return m_base_var.saved_value_to_string(thd, var, def_val);
  }
  virtual bool check_update_type(Item_result type) override {
    return m_base_var.check_update_type(type);
  }
  virtual enum_variable_source get_source() override {
    return m_base_var.get_source();
  }
  virtual const char *get_source_name() override {
    return m_base_var.get_source_name();
  }
  virtual void set_source(enum_variable_source src) override {
    m_base_var.set_source(src);
  }
  virtual bool set_source_name(const char *path) override {
    return m_base_var.set_source_name(path);
  }
  virtual bool set_user(const char *usr) override {
    return m_base_var.set_user(usr);
  }
  virtual const char *get_user() override { return m_base_var.get_user(); }
  virtual const char *get_host() override { return m_base_var.get_host(); }
  virtual bool set_host(const char *hst) override {
    return m_base_var.set_host(hst);
  }
  virtual ulonglong get_timestamp() const override {
    return m_base_var.get_timestamp();
  }
  virtual void set_user_host(THD *thd) override {
    m_base_var.set_user_host(thd);
  }
  virtual void set_timestamp() override { m_base_var.set_timestamp(); }
  virtual void set_timestamp(ulonglong ts) override {
    m_base_var.set_timestamp(ts);
  }

 private:
  virtual bool do_check(THD *thd, set_var *var) override {
    return m_base_var.do_check(thd, var);
  }
  virtual void session_save_default(THD *thd, set_var *var) override {
    return m_base_var.session_save_default(thd, var);
  }
  virtual void global_save_default(THD *thd, set_var *var) override {
    return m_base_var.global_save_default(thd, var);
  }
  virtual bool session_update(THD *thd, set_var *var) override {
    return m_base_var.session_update(thd, var);
  }
  virtual bool global_update(THD *thd, set_var *var) override {
    return m_base_var.global_update(thd, var);
  }

 protected:
  virtual const uchar *session_value_ptr(
      THD *running_thd, THD *target_thd,
      std::string_view keycache_name) override {
    return m_base_var.session_value_ptr(running_thd, target_thd, keycache_name);
  }
  virtual const uchar *global_value_ptr(
      THD *thd, std::string_view keycache_name) override {
    return m_base_var.global_value_ptr(thd, keycache_name);
  }
};

/**
  A deprecated alias for a variable.

  This tool allows us to rename system variables without breaking
  backward compatibility.

  Procedure for a developer to create a new name for a variable in
  version X and remove the old name in version X+1:

  - In version X:

    - Change the string passed to the Sys_var constructor for the
      variable the new new name.  All existing code for this should
      remain as it is.

    - Create a Sys_var_deprecated_alias taking the old name as the
      first argument and the Sys_var object having the new name as the
      second argument.

  - In version X+1:

    - Remove the Sys_var_deprecated_alias.

  This has the following effects in version X:

  - Both variables coexist. They are both visible in
    performance_schema tables and accessible in SET statements and
    SELECT @@variable statements. Both variables always have the same
    values.

  - A SET statement using either the old name or the new name changes
    the value of both variables.

  - A SET statement using the old name generates a deprecation
    warning.

  - The procedure that loads persisted variables from file accepts
    either the old name, or the new name, or both.  It generates a
    deprecation warning in case only the old name exists in the file.
    A SET PERSIST statement writes both variables to the file.

  The procedures for a user to upgrade or downgrade are:

  - After upgrade from version X-1 to X, all persisted variables
    retain their persisted values.  User will see deprecation warnings
    when loading the persisted variables file, with instructions to
    run a SET PERSIST statement any time before the next upgrade to
    X+1.

  - While on version X, user needs to run a SET PERSIST statement any
    time before upgrading to X+1. Due to the logic described above, it
    will write both variables to the file.

  - While on version X, user needs to change their cnf files,
    command-line arguments, and @@variables accessed through
    application logic, to use the new names, before upgrading to X+1.
    The deprecation warnings will help identify the relevant places to
    update.

  - After upgrade from X to X+1, the server will read the old
    variables from the file.  Since this version does not know about
    the old variables, it will ignore them and print a warning.  The
    user can remove the unknown variable from the persisted variable
    file, and get rid of the warning, using RESET PERSIST
    OLD_VARIABLE_NAME.

  - After downgrade from version X+1 to version X, all persisted
    variables retain their values.  User will not see deprecation
    warnings.  If user needs to further downgrade to version X-1, user
    needs to first run SET PERSIST for some variable in order to
    rewrite the file so that the old variable names exist in the file.

  - After downgrade from version X to version X-1, all persisted
    variables retain their values.  If the new variable names exist in
    the persisted variables file, a warning will be printed stating
    that the variable is not known and will be ignored.  User can get
    rid of the warning by running RESET PERSIST NEW_VARIABLE_NAME.
*/
class Sys_var_deprecated_alias : public Sys_var_alias {
 private:
  std::string m_comment;

 public:
  Sys_var_deprecated_alias(const char *name_arg, sys_var &base_var)
      : Sys_var_alias{name_arg, base_var, base_var.name.str, &base_var, true} {
    m_comment = std::string("This option is deprecated. Use ") +
                base_var.get_option()->name + " instead.";
    option.comment = m_comment.c_str();
  }
};

/**
  Helper class for variables that take values from a TYPELIB
*/
class Sys_var_typelib : public sys_var {
 protected:
  TYPELIB typelib;

 public:
  Sys_var_typelib(const char *name_arg, const char *comment, int flag_args,
                  ptrdiff_t off, CMD_LINE getopt, SHOW_TYPE show_val_type_arg,
                  const char *values[], ulonglong def_val, PolyLock *lock,
                  enum binlog_status_enum binlog_status_arg,
                  on_check_function on_check_func,
                  on_update_function on_update_func, const char *substitute,
                  int parse_flag = PARSE_NORMAL)
      : sys_var(&all_sys_vars, name_arg, comment, flag_args, off, getopt.id,
                getopt.arg_type, show_val_type_arg, def_val, lock,
                binlog_status_arg, on_check_func, on_update_func, substitute,
                parse_flag) {
    for (typelib.count = 0; values[typelib.count]; typelib.count++) /*no-op */
      ;
    typelib.name = "";
    typelib.type_names = values;
    typelib.type_lengths = nullptr;  // only used by Fields_enum and Field_set
    option.typelib = &typelib;
  }
  bool do_check(THD *, set_var *var) override  // works for enums and bool
  {
    char buff[STRING_BUFFER_USUAL_SIZE];
    String str(buff, sizeof(buff), system_charset_info), *res;

    if (var->value->result_type() == STRING_RESULT) {
      if (!(res = var->value->val_str(&str)))
        return true;
      else if (!(var->save_result.ulonglong_value =
                     find_type(&typelib, res->ptr(), res->length(), false)))
        return true;
      else
        var->save_result.ulonglong_value--;
    } else {
      longlong tmp = var->value->val_int();
      if (tmp < 0 || tmp >= static_cast<longlong>(typelib.count))
        return true;
      else
        var->save_result.ulonglong_value = tmp;
    }

    return false;
  }
  bool check_update_type(Item_result type) override {
    return type != INT_RESULT && type != STRING_RESULT;
  }
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
<<<<<<< HEAD
class Sys_var_enum : public Sys_var_typelib {
 public:
  Sys_var_enum(
      const char *name_arg, const char *comment, int flag_args, ptrdiff_t off,
      size_t size [[maybe_unused]], CMD_LINE getopt, const char *values[],
      uint def_val, PolyLock *lock = nullptr,
      enum binlog_status_enum binlog_status_arg = VARIABLE_NOT_IN_BINLOG,
      on_check_function on_check_func = nullptr,
      on_update_function on_update_func = nullptr,
      const char *substitute = nullptr, int parse_flag = PARSE_NORMAL)
      : Sys_var_typelib(name_arg, comment, flag_args, off, getopt, SHOW_CHAR,
                        values, def_val, lock, binlog_status_arg, on_check_func,
                        on_update_func, substitute, parse_flag) {
    option.var_type = GET_ENUM;
    global_var(ulong) = def_val;
<<<<<<< HEAD
    assert(def_val < typelib.count);
    assert(size == sizeof(ulong));
=======
    DBUG_ASSERT(def_val < typelib.count);
    DBUG_ASSERT(size == sizeof(ulong));
=======
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
    assert(def_val < typelib.count);
    assert(size == sizeof(ulong));
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231
  }
  bool session_update(THD *thd, set_var *var) override {
    session_var(thd, ulong) =
        static_cast<ulong>(var->save_result.ulonglong_value);
    return false;
  }
  bool global_update(THD *, set_var *var) override {
    global_var(ulong) = static_cast<ulong>(var->save_result.ulonglong_value);
    return false;
  }
  void session_save_default(THD *, set_var *var) override {
    var->save_result.ulonglong_value = global_var(ulong);
  }
  void global_save_default(THD *, set_var *var) override {
    var->save_result.ulonglong_value = option.def_value;
  }
  void saved_value_to_string(THD *, set_var *var, char *def_val) override {
    // Copy the symbolic name, not the numeric value.
    strcpy(def_val, typelib.type_names[var->save_result.ulonglong_value]);
  }
  const uchar *session_value_ptr(THD *, THD *target_thd,
                                 std::string_view) override {
    return pointer_cast<const uchar *>(
        typelib.type_names[session_var(target_thd, ulong)]);
  }
  const uchar *global_value_ptr(THD *, std::string_view) override {
    return pointer_cast<const uchar *>(typelib.type_names[global_var(ulong)]);
  }
};

/**
  The class for boolean variables - a variant of ENUM variables
  with the fixed list of values of { OFF , ON }

  Backing store: bool
*/
<<<<<<< HEAD
class Sys_var_bool : public Sys_var_typelib {
 public:
  Sys_var_bool(
      const char *name_arg, const char *comment, int flag_args, ptrdiff_t off,
      size_t size [[maybe_unused]], CMD_LINE getopt, bool def_val,
      PolyLock *lock = nullptr,
      enum binlog_status_enum binlog_status_arg = VARIABLE_NOT_IN_BINLOG,
      on_check_function on_check_func = nullptr,
      on_update_function on_update_func = nullptr,
      const char *substitute = nullptr, int parse_flag = PARSE_NORMAL)
      : Sys_var_typelib(name_arg, comment, flag_args, off, getopt, SHOW_MY_BOOL,
                        bool_values, def_val, lock, binlog_status_arg,
                        on_check_func, on_update_func, substitute, parse_flag) {
    option.var_type = GET_BOOL;
    global_var(bool) = def_val;
<<<<<<< HEAD
    assert(getopt.arg_type == OPT_ARG || getopt.id == -1);
    assert(size == sizeof(bool));
=======
    DBUG_ASSERT(getopt.arg_type == OPT_ARG || getopt.id == -1);
    DBUG_ASSERT(size == sizeof(bool));
=======
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
    assert(def_val < 2);
    assert(getopt.arg_type == OPT_ARG || getopt.id == -1);
    assert(size == sizeof(my_bool));
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231
  }
  bool session_update(THD *thd, set_var *var) override {
    session_var(thd, bool) =
        static_cast<bool>(var->save_result.ulonglong_value);
    return false;
  }
  bool global_update(THD *, set_var *var) override {
    global_var(bool) = static_cast<bool>(var->save_result.ulonglong_value);
    return false;
  }
  void session_save_default(THD *thd, set_var *var) override {
    var->save_result.ulonglong_value = static_cast<ulonglong>(
        *pointer_cast<const bool *>(global_value_ptr(thd, {})));
  }
  void global_save_default(THD *, set_var *var) override {
    var->save_result.ulonglong_value = option.def_value;
  }
  void saved_value_to_string(THD *, set_var *var, char *def_val) override {
    longlong10_to_str((longlong)var->save_result.ulonglong_value, def_val, 10);
  }
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
class Sys_var_multi_enum : public sys_var {
 public:
  struct ALIAS {
    const char *alias;
    uint number;
  };

  /**
    Enumerated type system variable.

    @param name_arg See sys_var::sys_var()

    @param comment See sys_var::sys_var()

    @param flag_args See sys_var::sys_var()

    @param off See sys_var::sys_var()

    @param size See sys_var::sys_var()

    @param getopt See sys_var::sys_var()

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

    @param command_line_no_value_arg The default value if a command line
    option is given without a value ('--command-line-option' without
    '=VALUE').  This must be less than value_count_arg.

    @param lock See sys_var::sys_var()

    @param binlog_status_arg See sys_var::sys_var()

    @param on_check_func See sys_var::sys_var()

    @param on_update_func See sys_var::sys_var()

    @param substitute See sys_var::sys_var()

    @param parse_flag See sys_var::sys_var()
  */
<<<<<<< HEAD
  Sys_var_multi_enum(
      const char *name_arg, const char *comment, int flag_args, ptrdiff_t off,
      size_t size [[maybe_unused]], CMD_LINE getopt, const ALIAS aliases_arg[],
      uint value_count_arg, uint def_val, uint command_line_no_value_arg,
      PolyLock *lock = nullptr,
      enum binlog_status_enum binlog_status_arg = VARIABLE_NOT_IN_BINLOG,
      on_check_function on_check_func = nullptr,
      on_update_function on_update_func = nullptr,
      const char *substitute = nullptr, int parse_flag = PARSE_NORMAL)
      : sys_var(&all_sys_vars, name_arg, comment, flag_args, off, getopt.id,
                getopt.arg_type, SHOW_CHAR, def_val, lock, binlog_status_arg,
                on_check_func, on_update_func, substitute, parse_flag),
        value_count(value_count_arg),
        aliases(aliases_arg),
        command_line_no_value(command_line_no_value_arg) {
    for (alias_count = 0; aliases[alias_count].alias; alias_count++)
<<<<<<< HEAD
      assert(aliases[alias_count].number < value_count);
    assert(def_val < alias_count);
=======
      DBUG_ASSERT(aliases[alias_count].number < value_count);
    DBUG_ASSERT(def_val < alias_count);
=======
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
      assert(aliases[alias_count].number < value_count);
    assert(def_val < alias_count);
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231

    option.var_type = GET_STR;
    option.value = &command_line_value;
    option.def_value = (intptr)aliases[def_val].alias;

    global_var(ulong) = aliases[def_val].number;

    assert(getopt.arg_type == OPT_ARG || getopt.id == -1);
    assert(size == sizeof(ulong));
  }

  /**
    Return the numeric value for a given alias string, or -1 if the
    string is not a valid alias.
  */
  int find_value(const char *text) {
    for (uint i = 0; aliases[i].alias != nullptr; i++)
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
  const char *fixup_command_line(const char *value_str) {
    DBUG_TRACE;
    char *end = nullptr;
    long value;

    // User passed --option (not --option=value).
    if (value_str == nullptr) {
      value = command_line_no_value;
      goto end;
    }

    // Get textual value.
    value = find_value(value_str);
    if (value != -1) goto end;

    // Get numeric value.
    value = strtol(value_str, &end, 10);
    // found a number and nothing else?
    if (end > value_str && *end == '\0')
      // value is in range?
      if (value >= 0 && (longlong)value < (longlong)value_count) goto end;

    // Not a valid value.
    return value_str;

  end:
    global_var(ulong) = value;
    return nullptr;
  }

  bool do_check(THD *, set_var *var) override {
    DBUG_TRACE;
    char buff[STRING_BUFFER_USUAL_SIZE];
    String str(buff, sizeof(buff), system_charset_info), *res;
    if (var->value->result_type() == STRING_RESULT) {
      res = var->value->val_str(&str);
      if (!res) return true;

      /* Check if the value is a valid string. */
      size_t valid_len;
      bool len_error;
      if (validate_string(system_charset_info, res->ptr(), res->length(),
                          &valid_len, &len_error))
        return true;

      int value = find_value(res->ptr());
      if (value == -1) return true;
      var->save_result.ulonglong_value = (uint)value;
    } else {
      longlong value = var->value->val_int();
      if (value < 0 || value >= (longlong)value_count)
        return true;
      else
        var->save_result.ulonglong_value = value;
    }

    return false;
  }
  bool check_update_type(Item_result type) override {
    return type != INT_RESULT && type != STRING_RESULT;
  }
<<<<<<< HEAD
  bool session_update(THD *, set_var *) override {
    DBUG_TRACE;
=======
  bool session_update(THD *, set_var *) {
    DBUG_ENTER("Sys_var_multi_enum::session_update");
>>>>>>> pr/231
    assert(0);
    /*
    Currently not used: uncomment if this class is used as a base for
    a session variable.

    session_var(thd, ulong)=
      static_cast<ulong>(var->save_result.ulonglong_value);
    */
    return false;
  }
<<<<<<< HEAD
  bool global_update(THD *, set_var *) override {
    DBUG_TRACE;
=======
  bool global_update(THD *, set_var *) {
    DBUG_ENTER("Sys_var_multi_enum::global_update");
>>>>>>> pr/231
    assert(0);
    /*
    Currently not used: uncomment if this some inheriting class does
    not override..

    ulong val=
      static_cast<ulong>(var->save_result.ulonglong_value);
    global_var(ulong)= val;
    */
    return false;
  }
<<<<<<< HEAD
  void session_save_default(THD *, set_var *) override {
    DBUG_TRACE;
=======
  void session_save_default(THD *, set_var *) {
    DBUG_ENTER("Sys_var_multi_enum::session_save_default");
>>>>>>> pr/231
    assert(0);
    /*
    Currently not used: uncomment if this class is used as a base for
    a session variable.

    int value= find_value((char *)option.def_value);
    assert(value != -1);
    var->save_result.ulonglong_value= value;
    */
    return;
  }
<<<<<<< HEAD
  void global_save_default(THD *, set_var *var) override {
    DBUG_TRACE;
=======
  void global_save_default(THD *, set_var *var) {
    DBUG_ENTER("Sys_var_multi_enum::global_save_default");
<<<<<<< HEAD
>>>>>>> pr/231
    int value = find_value((char *)option.def_value);
    assert(value != -1);
    var->save_result.ulonglong_value = value;
<<<<<<< HEAD
    return;
  }
  void saved_value_to_string(THD *, set_var *var, char *def_val) override {
    longlong10_to_str((longlong)var->save_result.ulonglong_value, def_val, 10);
  }

  const uchar *session_value_ptr(THD *, THD *, std::string_view) override {
    DBUG_TRACE;
=======
=======
    int value= find_value((char *)option.def_value);
    assert(value != -1);
    var->save_result.ulonglong_value= value;
>>>>>>> upstream/cluster-7.6
    DBUG_VOID_RETURN;
  }

  uchar *session_value_ptr(THD *, THD *, LEX_STRING *) {
    DBUG_ENTER("Sys_var_multi_enum::session_value_ptr");
>>>>>>> pr/231
    assert(0);
    /*
    Currently not used: uncomment if this class is used as a base for
    a session variable.

    return (uchar*)aliases[session_var(target_thd, ulong)].alias;
    */
    return nullptr;
  }
  const uchar *global_value_ptr(THD *, std::string_view) override {
    DBUG_TRACE;
    return pointer_cast<const uchar *>(aliases[global_var(ulong)].alias);
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
<<<<<<< HEAD
class Sys_var_charptr : public sys_var {
 public:
  Sys_var_charptr(
      const char *name_arg, const char *comment, int flag_args, ptrdiff_t off,
      size_t size [[maybe_unused]], CMD_LINE getopt,
      enum charset_enum is_os_charset_arg, const char *def_val,
      PolyLock *lock = nullptr,
      enum binlog_status_enum binlog_status_arg = VARIABLE_NOT_IN_BINLOG,
      on_check_function on_check_func = nullptr,
      on_update_function on_update_func = nullptr,
      const char *substitute = nullptr, int parse_flag = PARSE_NORMAL)
      : sys_var(&all_sys_vars, name_arg, comment, flag_args, off, getopt.id,
                getopt.arg_type, SHOW_CHAR_PTR, (intptr)def_val, lock,
                binlog_status_arg, on_check_func, on_update_func, substitute,
                parse_flag) {
    is_os_charset = is_os_charset_arg == IN_FS_CHARSET;
    option.var_type = (flags & ALLOCATED) ? GET_STR_ALLOC : GET_STR;
    global_var(const char *) = def_val;
<<<<<<< HEAD
    assert(size == sizeof(char *));
=======
    DBUG_ASSERT(size == sizeof(char *));
=======
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
    assert(size == sizeof(char *));
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231
  }

  void cleanup() override {
    if (flags & ALLOCATED) my_free(global_var(char *));
    flags &= ~ALLOCATED;
  }

  bool do_check(THD *thd, set_var *var) override {
    char buff[STRING_BUFFER_USUAL_SIZE], buff2[STRING_BUFFER_USUAL_SIZE];
    String str(buff, sizeof(buff), charset(thd));
    String str2(buff2, sizeof(buff2), charset(thd)), *res;

    if (!(res = var->value->val_str(&str)))
      var->save_result.string_value.str = nullptr;
    else {
      size_t unused;
      if (String::needs_conversion(res->length(), res->charset(), charset(thd),
                                   &unused)) {
        uint errors;
        str2.copy(res->ptr(), res->length(), res->charset(), charset(thd),
                  &errors);
        res = &str2;
      }
      var->save_result.string_value.str =
          thd->strmake(res->ptr(), res->length());
      var->save_result.string_value.length = res->length();
    }

    return false;
  }

  bool session_update(THD *thd, set_var *var) override {
    char *new_val = var->save_result.string_value.str;
    size_t new_val_len = var->save_result.string_value.length;
    char *ptr = ((char *)&thd->variables + offset);

    return thd->session_sysvar_res_mgr.update((char **)ptr, new_val,
                                              new_val_len);
  }

  bool global_update(THD *thd, set_var *var) override;

  void session_save_default(THD *, set_var *var) override {
    char *ptr = (char *)(intptr)option.def_value;
    var->save_result.string_value.str = ptr;
    var->save_result.string_value.length = ptr ? strlen(ptr) : 0;
  }

  void global_save_default(THD *, set_var *var) override {
    char *ptr = (char *)(intptr)option.def_value;
    /*
     TODO: default values should not be null. Fix all and turn this into an
     assert.
     Do that only for NON_PERSIST READ_ONLY variables since the rest use
     the NULL value as a flag that SET .. = DEFAULT was issued and hence
     it should not be alterned.
    */
    var->save_result.string_value.str =
        ptr || ((sys_var::READONLY | sys_var::NOTPERSIST) !=
                (flags & (sys_var::READONLY | sys_var::NOTPERSIST)))
            ? ptr
            : const_cast<char *>("");
    var->save_result.string_value.length = ptr ? strlen(ptr) : 0;
  }
  void saved_value_to_string(THD *, set_var *var, char *def_val) override {
    memcpy(def_val, var->save_result.string_value.str,
           var->save_result.string_value.length);
  }
  bool check_update_type(Item_result type) override {
    return type != STRING_RESULT;
  }
};

class Sys_var_version : public Sys_var_charptr {
 public:
  Sys_var_version(const char *name_arg, const char *comment, int flag_args,
                  ptrdiff_t off, size_t size, CMD_LINE getopt,
                  enum charset_enum is_os_charset_arg, const char *def_val)
      : Sys_var_charptr(name_arg, comment, flag_args, off, size, getopt,
                        is_os_charset_arg, def_val) {}

  ~Sys_var_version() override = default;

  const uchar *global_value_ptr(THD *thd,
                                std::string_view keycache_name) override {
    const uchar *value = Sys_var_charptr::global_value_ptr(thd, keycache_name);

    DBUG_EXECUTE_IF("alter_server_version_str", {
      static const char *altered_value = "some-other-version";
      const uchar *altered_value_ptr = pointer_cast<uchar *>(&altered_value);
      value = altered_value_ptr;
    });

    return value;
  }
};

class Sys_var_proxy_user : public sys_var {
 public:
  Sys_var_proxy_user(const char *name_arg, const char *comment,
                     enum charset_enum is_os_charset_arg)
      : sys_var(&all_sys_vars, name_arg, comment,
                sys_var::READONLY + sys_var::ONLY_SESSION, 0, -1, NO_ARG,
                SHOW_CHAR, 0, nullptr, VARIABLE_NOT_IN_BINLOG, nullptr, nullptr,
                nullptr, PARSE_NORMAL) {
    is_os_charset = is_os_charset_arg == IN_FS_CHARSET;
    option.var_type = GET_STR;
  }
<<<<<<< HEAD
  bool do_check(THD *, set_var *) override {
    assert(false);
=======
<<<<<<< HEAD
  bool do_check(THD *, set_var *) {
    DBUG_ASSERT(false);
>>>>>>> pr/231
    return true;
  }
  bool session_update(THD *, set_var *) override {
    assert(false);
    return true;
  }
  bool global_update(THD *, set_var *) override {
    assert(false);
    return false;
  }
  void session_save_default(THD *, set_var *) override { assert(false); }
  void global_save_default(THD *, set_var *) override { assert(false); }
  void saved_value_to_string(THD *, set_var *, char *) override {
    assert(false);
  }
  bool check_update_type(Item_result) override { return true; }

 protected:
  const uchar *session_value_ptr(THD *, THD *target_thd,
                                 std::string_view) override {
    const char *proxy_user = target_thd->security_context()->proxy_user().str;
<<<<<<< HEAD
    return proxy_user[0] ? pointer_cast<const uchar *>(proxy_user) : nullptr;
=======
=======
  bool do_check(THD *thd, set_var *var)
  {
    assert(FALSE);
    return true;
  }
  bool session_update(THD *thd, set_var *var)
  {
    assert(FALSE);
    return true;
  }
  bool global_update(THD *thd, set_var *var)
  {
    assert(FALSE);
    return false;
  }
  void session_save_default(THD *thd, set_var *var)
  { assert(FALSE); }
  void global_save_default(THD *thd, set_var *var)
  { assert(FALSE); }
  bool check_update_type(Item_result type)
  { return true; }
protected:
  virtual uchar *session_value_ptr(THD *running_thd, THD *target_thd, LEX_STRING *base)
  {
    const char* proxy_user= target_thd->security_context()->proxy_user().str;
>>>>>>> upstream/cluster-7.6
    return proxy_user[0] ? (uchar *)proxy_user : NULL;
>>>>>>> pr/231
  }
};

class Sys_var_external_user : public Sys_var_proxy_user {
 public:
  Sys_var_external_user(const char *name_arg, const char *comment_arg,
                        enum charset_enum is_os_charset_arg)
      : Sys_var_proxy_user(name_arg, comment_arg, is_os_charset_arg) {}

 protected:
  const uchar *session_value_ptr(THD *, THD *target_thd,
                                 std::string_view) override {
    LEX_CSTRING external_user = target_thd->security_context()->external_user();
    return external_user.length ? pointer_cast<const uchar *>(external_user.str)
                                : nullptr;
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
<<<<<<< HEAD
class Sys_var_lexstring : public Sys_var_charptr {
 public:
  Sys_var_lexstring(
      const char *name_arg, const char *comment, int flag_args, ptrdiff_t off,
      size_t size [[maybe_unused]], CMD_LINE getopt,
      enum charset_enum is_os_charset_arg, const char *def_val,
      PolyLock *lock = nullptr,
      enum binlog_status_enum binlog_status_arg = VARIABLE_NOT_IN_BINLOG,
      on_check_function on_check_func = nullptr,
      on_update_function on_update_func = nullptr,
      const char *substitute = nullptr)
      : Sys_var_charptr(name_arg, comment, flag_args, off, sizeof(char *),
                        getopt, is_os_charset_arg, def_val, lock,
                        binlog_status_arg, on_check_func, on_update_func,
                        substitute) {
    global_var(LEX_STRING).length = strlen(def_val);
    assert(size == sizeof(LEX_STRING));
    *const_cast<SHOW_TYPE *>(&show_val_type) = SHOW_LEX_STRING;
=======
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
    assert(size == sizeof(LEX_STRING));
    *const_cast<SHOW_TYPE*>(&show_val_type)= SHOW_LEX_STRING;
>>>>>>> upstream/cluster-7.6
  }
  bool global_update(THD *thd, set_var *var) override {
    if (Sys_var_charptr::global_update(thd, var)) return true;
    global_var(LEX_STRING).length = var->save_result.string_value.length;
    return false;
  }
};

#ifndef NDEBUG
/**
  @@session.dbug and @@global.dbug variables.

  @@dbug variable differs from other variables in one aspect:
  if its value is not assigned in the session, it "points" to the global
  value, and so when the global value is changed, the change
  immediately takes effect in the session.

  This semantics is intentional, to be able to debug one session from
  another.
*/
class Sys_var_dbug : public sys_var {
 public:
  Sys_var_dbug(
      const char *name_arg, const char *comment, int flag_args, CMD_LINE getopt,
      const char *def_val, PolyLock *lock = nullptr,
      enum binlog_status_enum binlog_status_arg = VARIABLE_NOT_IN_BINLOG,
      on_check_function on_check_func = nullptr,
      on_update_function on_update_func = nullptr,
      const char *substitute = nullptr, int parse_flag = PARSE_NORMAL)
      : sys_var(&all_sys_vars, name_arg, comment, flag_args, 0, getopt.id,
                getopt.arg_type, SHOW_CHAR, (intptr)def_val, lock,
                binlog_status_arg, on_check_func, on_update_func, substitute,
                parse_flag) {
    option.var_type = GET_NO_ARG;
  }
  bool do_check(THD *thd, set_var *var) override {
    char buff[STRING_BUFFER_USUAL_SIZE];
    String str(buff, sizeof(buff), system_charset_info), *res;

    if (!(res = var->value->val_str(&str)))
      var->save_result.string_value.str = const_cast<char *>("");
    else
      var->save_result.string_value.str =
          thd->strmake(res->ptr(), res->length());
    return false;
  }
  bool session_update(THD *, set_var *var) override {
    const char *val = var->save_result.string_value.str;
    if (!var->value)
      DBUG_POP();
    else
      DBUG_SET(val);
    return false;
  }
  bool global_update(THD *, set_var *var) override {
    const char *val = var->save_result.string_value.str;
    DBUG_SET_INITIAL(val);
    return false;
  }
  void session_save_default(THD *, set_var *) override {}
  void global_save_default(THD *, set_var *var) override {
    char *ptr = (char *)(intptr)option.def_value;
    var->save_result.string_value.str = ptr;
  }
  void saved_value_to_string(THD *, set_var *var, char *def_val) override {
    memcpy(def_val, var->save_result.string_value.str,
           var->save_result.string_value.length);
  }
  const uchar *session_value_ptr(THD *running_thd, THD *,
                                 std::string_view) override {
    char buf[512];
    DBUG_EXPLAIN(buf, sizeof(buf));
    return (uchar *)running_thd->mem_strdup(buf);
  }
  const uchar *global_value_ptr(THD *thd, std::string_view) override {
    char buf[512];
    DBUG_EXPLAIN_INITIAL(buf, sizeof(buf));
    return (uchar *)thd->mem_strdup(buf);
  }
  bool check_update_type(Item_result type) override {
    return type != STRING_RESULT;
  }
};
#endif

#define KEYCACHE_VAR(X) \
  sys_var::GLOBAL, offsetof(KEY_CACHE, X), sizeof(((KEY_CACHE *)0)->X)
#define keycache_var_ptr(KC, OFF) (((uchar *)(KC)) + (OFF))
#define keycache_var(KC, OFF) (*(ulonglong *)keycache_var_ptr(KC, OFF))
typedef bool (*keycache_update_function)(THD *, KEY_CACHE *, ptrdiff_t,
                                         ulonglong);

/**
  The class for keycache_* variables. Supports structured names,
  keycache_name.variable_name.

  Class specific constructor arguments:
    everything derived from Sys_var_ulonglong

  Backing store: ulonglong

  @note these variables can be only GLOBAL
*/
class Sys_var_keycache : public Sys_var_ulonglong {
  keycache_update_function keycache_update;
<<<<<<< HEAD

 public:
  Sys_var_keycache(const char *name_arg, const char *comment, int flag_args,
                   ptrdiff_t off, size_t size, CMD_LINE getopt,
                   ulonglong min_val, ulonglong max_val, ulonglong def_val,
                   uint block_size, PolyLock *lock,
                   enum binlog_status_enum binlog_status_arg,
                   on_check_function on_check_func,
                   keycache_update_function on_update_func,
                   const char *substitute = nullptr)
      : Sys_var_ulonglong(
            name_arg, comment, flag_args, -1, /* offset, see base class CTOR */
            size, getopt, min_val, max_val, def_val, block_size, lock,
            binlog_status_arg, on_check_func, nullptr, substitute),
        keycache_update(on_update_func) {
    offset = off; /* Remember offset in KEY_CACHE */
    option.var_type |= GET_ASK_ADDR;
    option.value = (uchar **)1;  // crash me, please
    keycache_var(dflt_key_cache, off) = def_val;
<<<<<<< HEAD
    assert(scope() == GLOBAL);
=======
    DBUG_ASSERT(scope() == GLOBAL);
=======
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
    assert(scope() == GLOBAL);
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231
  }
  bool global_update(THD *thd, set_var *var) override {
    ulonglong new_value = var->save_result.ulonglong_value;

    assert(var->m_var_tracker.is_keycache_var());
    std::string_view base_name = var->m_var_tracker.get_keycache_name();

    /* If no basename, assume it's for the key cache named 'default' */
    if (!base_name.empty()) {
      push_warning_printf(
          thd, Sql_condition::SL_WARNING, ER_WARN_DEPRECATED_SYNTAX,
          "%.*s.%s syntax "
          "is deprecated and will be removed in a "
          "future release",
          static_cast<int>(base_name.size()), base_name.data(), name.str);
    }

    KEY_CACHE *key_cache = get_key_cache(base_name);

    if (!key_cache) {  // Key cache didn't exists */
      if (!new_value)  // Tried to delete cache
        return false;  // Ok, nothing to do
      if (!(key_cache = create_key_cache(base_name))) return true;
    }

    /**
      Abort if some other thread is changing the key cache
      @todo This should be changed so that we wait until the previous
      assignment is done and then do the new assign
    */
    if (key_cache->in_init) return true;

    return keycache_update(thd, key_cache, offset, new_value);
  }
  const uchar *global_value_ptr(THD *thd,
                                std::string_view keycache_name) override {
    if (!keycache_name.empty())
      push_warning_printf(thd, Sql_condition::SL_WARNING,
                          ER_WARN_DEPRECATED_SYNTAX,
                          "@@global.%.*s.%s syntax "
                          "is deprecated and will be removed in a "
                          "future release",
                          static_cast<int>(keycache_name.size()),
                          keycache_name.data(), name.str);

    KEY_CACHE *key_cache = get_key_cache(keycache_name);
    if (!key_cache) key_cache = &zero_key_cache;
    return keycache_var_ptr(key_cache, offset);
  }
};

/**
  The class for floating point variables

  Class specific constructor arguments: min, max

  Backing store: double
*/
<<<<<<< HEAD
class Sys_var_double : public sys_var {
 public:
  Sys_var_double(
      const char *name_arg, const char *comment, int flag_args, ptrdiff_t off,
      size_t size [[maybe_unused]], CMD_LINE getopt, double min_val,
      double max_val, double def_val, PolyLock *lock = nullptr,
      enum binlog_status_enum binlog_status_arg = VARIABLE_NOT_IN_BINLOG,
      on_check_function on_check_func = nullptr,
      on_update_function on_update_func = nullptr,
      const char *substitute = nullptr, int parse_flag = PARSE_NORMAL)
      : sys_var(&all_sys_vars, name_arg, comment, flag_args, off, getopt.id,
                getopt.arg_type, SHOW_DOUBLE,
                (longlong)getopt_double2ulonglong(def_val), lock,
                binlog_status_arg, on_check_func, on_update_func, substitute,
                parse_flag) {
    option.var_type = GET_DOUBLE;
    option.min_value = (longlong)getopt_double2ulonglong(min_val);
    option.max_value = (longlong)getopt_double2ulonglong(max_val);
<<<<<<< HEAD
    global_var(double) = getopt_ulonglong2double(option.def_value);
=======
    global_var(double) = (double)option.def_value;
    DBUG_ASSERT(min_val <= max_val);
    DBUG_ASSERT(min_val <= def_val);
    DBUG_ASSERT(max_val >= def_val);
    DBUG_ASSERT(size == sizeof(double));
=======
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
>>>>>>> pr/231
    assert(min_val <= max_val);
    assert(min_val <= def_val);
    assert(max_val >= def_val);
    assert(size == sizeof(double));
<<<<<<< HEAD
=======
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231
  }
  bool do_check(THD *thd, set_var *var) override {
    bool fixed;
    double v = var->value->val_real();
    var->save_result.double_value =
        getopt_double_limit_value(v, &option, &fixed);

    return throw_bounds_warning(thd, name.str, fixed, v);
  }
  bool session_update(THD *thd, set_var *var) override {
    session_var(thd, double) = var->save_result.double_value;
    return false;
  }
  bool global_update(THD *, set_var *var) override {
    global_var(double) = var->save_result.double_value;
    return false;
  }
  bool check_update_type(Item_result type) override {
    return type != INT_RESULT && type != REAL_RESULT && type != DECIMAL_RESULT;
  }
  void session_save_default(THD *, set_var *var) override {
    var->save_result.double_value = global_var(double);
  }
  void global_save_default(THD *, set_var *var) override {
    var->save_result.double_value = getopt_ulonglong2double(option.def_value);
  }
  void saved_value_to_string(THD *, set_var *var, char *def_val) override {
    my_fcvt(var->save_result.double_value, 6, def_val, nullptr);
  }
};

/**
  The class for @c test_flags (core_file for now).
  It's derived from Sys_var_bool.

  Class specific constructor arguments:
    Caller need not pass in a variable as we make up the value on the
    fly, that is, we derive it from the global test_flags bit vector.

  Backing store: bool
*/
class Sys_var_test_flag : public Sys_var_bool {
 private:
  bool test_flag_value;
  uint test_flag_mask;

 public:
  Sys_var_test_flag(const char *name_arg, const char *comment, uint mask)
      : Sys_var_bool(name_arg, comment,
                     READ_ONLY NON_PERSIST GLOBAL_VAR(test_flag_value),
                     NO_CMD_LINE, DEFAULT(false)) {
    test_flag_mask = mask;
  }
  const uchar *global_value_ptr(THD *, std::string_view) override {
    test_flag_value = ((test_flags & test_flag_mask) > 0);
    return (uchar *)&test_flag_value;
  }
};

/**
  The class for the @c max_user_connections.
  It's derived from Sys_var_uint, but non-standard session value
  requires a new class.

  Class specific constructor arguments:
    everything derived from Sys_var_uint

  Backing store: uint
*/
class Sys_var_max_user_conn : public Sys_var_uint {
 public:
  Sys_var_max_user_conn(
      const char *name_arg, const char *comment, int, ptrdiff_t off,
      size_t size, CMD_LINE getopt, uint min_val, uint max_val, uint def_val,
      uint block_size, PolyLock *lock = nullptr,
      enum binlog_status_enum binlog_status_arg = VARIABLE_NOT_IN_BINLOG,
      on_check_function on_check_func = nullptr,
      on_update_function on_update_func = nullptr,
      const char *substitute = nullptr)
      : Sys_var_uint(name_arg, comment, SESSION, off, size, getopt, min_val,
                     max_val, def_val, block_size, lock, binlog_status_arg,
                     on_check_func, on_update_func, substitute) {}
  const uchar *session_value_ptr(THD *running_thd, THD *target_thd,
                                 std::string_view keycache_name) override {
    const USER_CONN *uc = target_thd->get_user_connect();
    if (uc && uc->user_resources.user_conn)
      return pointer_cast<const uchar *>(&(uc->user_resources.user_conn));
    return global_value_ptr(running_thd, keycache_name);
  }
};

// overflow-safe (1 << X)-1
#define MAX_SET(X) ((((1ULL << ((X)-1)) - 1) << 1) | 1)

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
<<<<<<< HEAD
class Sys_var_flagset : public Sys_var_typelib {
 public:
  Sys_var_flagset(
      const char *name_arg, const char *comment, int flag_args, ptrdiff_t off,
      size_t size [[maybe_unused]], CMD_LINE getopt, const char *values[],
      ulonglong def_val, PolyLock *lock = nullptr,
      enum binlog_status_enum binlog_status_arg = VARIABLE_NOT_IN_BINLOG,
      on_check_function on_check_func = nullptr,
      on_update_function on_update_func = nullptr,
      const char *substitute = nullptr)
      : Sys_var_typelib(name_arg, comment, flag_args, off, getopt, SHOW_CHAR,
                        values, def_val, lock, binlog_status_arg, on_check_func,
                        on_update_func, substitute) {
    option.var_type = GET_FLAGSET;
    global_var(ulonglong) = def_val;
<<<<<<< HEAD
    assert(typelib.count > 1);
    assert(typelib.count <= 65);
    assert(def_val < MAX_SET(typelib.count));
    assert(strcmp(values[typelib.count - 1], "default") == 0);
    assert(size == sizeof(ulonglong));
=======
    DBUG_ASSERT(typelib.count > 1);
    DBUG_ASSERT(typelib.count <= 65);
    DBUG_ASSERT(def_val < MAX_SET(typelib.count));
    DBUG_ASSERT(strcmp(values[typelib.count - 1], "default") == 0);
    DBUG_ASSERT(size == sizeof(ulonglong));
=======
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
    assert(typelib.count > 1);
    assert(typelib.count <= 65);
    assert(def_val < MAX_SET(typelib.count));
    assert(strcmp(values[typelib.count-1], "default") == 0);
    assert(size == sizeof(ulonglong));
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231
  }
  bool do_check(THD *thd, set_var *var) override {
    char buff[STRING_BUFFER_USUAL_SIZE];
    String str(buff, sizeof(buff), system_charset_info), *res;
    ulonglong default_value, current_value;
    if (var->type == OPT_GLOBAL) {
      default_value = option.def_value;
      current_value = global_var(ulonglong);
    } else {
      default_value = global_var(ulonglong);
      current_value = session_var(thd, ulonglong);
    }

    if (var->value->result_type() == STRING_RESULT) {
      if (!(res = var->value->val_str(&str)))
        return true;
      else {
        const char *error;
        uint error_len;

        var->save_result.ulonglong_value = find_set_from_flags(
            &typelib, typelib.count, current_value, default_value, res->ptr(),
            static_cast<uint>(res->length()), &error, &error_len);
        if (error) {
          ErrConvString err(error, error_len, res->charset());
          my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), name.str, err.ptr());
          return true;
        }
      }
    } else {
      longlong tmp = var->value->val_int();
      if ((tmp < 0 && !var->value->unsigned_flag) ||
          (ulonglong)tmp > MAX_SET(typelib.count))
        return true;
      else
        var->save_result.ulonglong_value = tmp;
    }

    return false;
  }
  bool session_update(THD *thd, set_var *var) override {
    session_var(thd, ulonglong) = var->save_result.ulonglong_value;
    return false;
  }
  bool global_update(THD *, set_var *var) override {
    global_var(ulonglong) = var->save_result.ulonglong_value;
    return false;
  }
  void session_save_default(THD *, set_var *var) override {
    var->save_result.ulonglong_value = global_var(ulonglong);
  }
  void global_save_default(THD *, set_var *var) override {
    var->save_result.ulonglong_value = option.def_value;
  }
  void saved_value_to_string(THD *thd, set_var *var, char *def_val) override {
    strcpy(def_val,
           flagset_to_string(thd, nullptr, var->save_result.ulonglong_value,
                             typelib.type_names));
  }
  const uchar *session_value_ptr(THD *running_thd, THD *target_thd,
                                 std::string_view) override {
    return (uchar *)flagset_to_string(running_thd, nullptr,
                                      session_var(target_thd, ulonglong),
                                      typelib.type_names);
  }
  const uchar *global_value_ptr(THD *thd, std::string_view) override {
    return (uchar *)flagset_to_string(thd, nullptr, global_var(ulonglong),
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
<<<<<<< HEAD
class Sys_var_set : public Sys_var_typelib {
 public:
  Sys_var_set(
      const char *name_arg, const char *comment, int flag_args, ptrdiff_t off,
      size_t size [[maybe_unused]], CMD_LINE getopt, const char *values[],
      ulonglong def_val, PolyLock *lock = nullptr,
      enum binlog_status_enum binlog_status_arg = VARIABLE_NOT_IN_BINLOG,
      on_check_function on_check_func = nullptr,
      on_update_function on_update_func = nullptr,
      const char *substitute = nullptr)
      : Sys_var_typelib(name_arg, comment, flag_args, off, getopt, SHOW_CHAR,
                        values, def_val, lock, binlog_status_arg, on_check_func,
                        on_update_func, substitute) {
    option.var_type = GET_SET;
    global_var(ulonglong) = def_val;
<<<<<<< HEAD
=======
    DBUG_ASSERT(typelib.count > 0);
    DBUG_ASSERT(typelib.count <= 64);
    DBUG_ASSERT(def_val < MAX_SET(typelib.count));
    DBUG_ASSERT(size == sizeof(ulonglong));
=======
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
>>>>>>> pr/231
    assert(typelib.count > 0);
    assert(typelib.count <= 64);
    assert(def_val < MAX_SET(typelib.count));
    assert(size == sizeof(ulonglong));
<<<<<<< HEAD
=======
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231
  }
  bool do_check(THD *, set_var *var) override {
    char buff[STRING_BUFFER_USUAL_SIZE];
    String str(buff, sizeof(buff), system_charset_info), *res;

    if (var->value->result_type() == STRING_RESULT) {
      if (!(res = var->value->val_str(&str)))
        return true;
      else {
        const char *error;
        uint error_len;
        bool not_used;

        var->save_result.ulonglong_value =
            find_set(&typelib, res->ptr(), static_cast<uint>(res->length()),
                     nullptr, &error, &error_len, &not_used);
        /*
          note, we only issue an error if error_len > 0.
          That is even while empty (zero-length) values are considered
          errors by find_set(), these errors are ignored here
        */
        if (error_len) {
          ErrConvString err(error, error_len, res->charset());
          my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), name.str, err.ptr());
          return true;
        }
      }
    } else {
      longlong tmp = var->value->val_int();
      if ((tmp < 0 && !var->value->unsigned_flag) ||
          (ulonglong)tmp > MAX_SET(typelib.count))
        return true;
      else
        var->save_result.ulonglong_value = tmp;
    }

    return false;
  }
  bool session_update(THD *thd, set_var *var) override {
    session_var(thd, ulonglong) = var->save_result.ulonglong_value;
    return false;
  }
  bool global_update(THD *, set_var *var) override {
    global_var(ulonglong) = var->save_result.ulonglong_value;
    return false;
  }
  void session_save_default(THD *, set_var *var) override {
    var->save_result.ulonglong_value = global_var(ulonglong);
  }
  void global_save_default(THD *, set_var *var) override {
    var->save_result.ulonglong_value = option.def_value;
  }
  void saved_value_to_string(THD *thd, set_var *var, char *def_val) override {
    strcpy(def_val,
           set_to_string(thd, nullptr, var->save_result.ulonglong_value,
                         typelib.type_names));
  }
  const uchar *session_value_ptr(THD *running_thd, THD *target_thd,
                                 std::string_view) override {
    return (uchar *)set_to_string(running_thd, nullptr,
                                  session_var(target_thd, ulonglong),
                                  typelib.type_names);
  }
  const uchar *global_value_ptr(THD *thd, std::string_view) override {
    return (uchar *)set_to_string(thd, nullptr, global_var(ulonglong),
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
class Sys_var_plugin : public sys_var {
  int plugin_type;
<<<<<<< HEAD

 public:
  Sys_var_plugin(
      const char *name_arg, const char *comment, int flag_args, ptrdiff_t off,
      size_t size [[maybe_unused]], CMD_LINE getopt, int plugin_type_arg,
      const char **def_val, PolyLock *lock = nullptr,
      enum binlog_status_enum binlog_status_arg = VARIABLE_NOT_IN_BINLOG,
      on_check_function on_check_func = nullptr,
      on_update_function on_update_func = nullptr,
      const char *substitute = nullptr, int parse_flag = PARSE_NORMAL)
      : sys_var(&all_sys_vars, name_arg, comment, flag_args, off, getopt.id,
                getopt.arg_type, SHOW_CHAR, (intptr)def_val, lock,
                binlog_status_arg, on_check_func, on_update_func, substitute,
                parse_flag),
        plugin_type(plugin_type_arg) {
    option.var_type = GET_STR;
<<<<<<< HEAD
    assert(size == sizeof(plugin_ref));
    assert(getopt.id == -1);  // force NO_CMD_LINE
=======
    DBUG_ASSERT(size == sizeof(plugin_ref));
    DBUG_ASSERT(getopt.id == -1);  // force NO_CMD_LINE
=======
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
    assert(size == sizeof(plugin_ref));
    assert(getopt.id == -1); // force NO_CMD_LINE
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231
  }
  bool do_check(THD *thd, set_var *var) override {
    char buff[STRING_BUFFER_USUAL_SIZE];
    String str(buff, sizeof(buff), system_charset_info), *res;

    /* NULLs can't be used as a default storage engine */
    if (!(res = var->value->val_str(&str))) return true;

    LEX_CSTRING pname_cstr = res->lex_cstring();
    plugin_ref plugin;

    // special code for storage engines (e.g. to handle historical aliases)
    if (plugin_type == MYSQL_STORAGE_ENGINE_PLUGIN)
      plugin = ha_resolve_by_name(thd, &pname_cstr, false);
    else {
      plugin = my_plugin_lock_by_name(thd, pname_cstr, plugin_type);
    }

    if (!plugin) {
      // historically different error code
      if (plugin_type == MYSQL_STORAGE_ENGINE_PLUGIN) {
        ErrConvString err(res);
        my_error(ER_UNKNOWN_STORAGE_ENGINE, MYF(0), err.ptr());
      }
      return true;
    }
    var->save_result.plugin = plugin;
    return false;
  }
  void do_update(plugin_ref *valptr, plugin_ref newval) {
    plugin_ref oldval = *valptr;
    if (oldval != newval) {
      *valptr = my_plugin_lock(nullptr, &newval);
      plugin_unlock(nullptr, oldval);
    }
  }
  bool session_update(THD *thd, set_var *var) override {
    do_update((plugin_ref *)session_var_ptr(thd), var->save_result.plugin);
    return false;
  }
  bool global_update(THD *, set_var *var) override {
    do_update((plugin_ref *)global_var_ptr(), var->save_result.plugin);
    return false;
  }
  void session_save_default(THD *thd, set_var *var) override {
    plugin_ref plugin = global_var(plugin_ref);
    var->save_result.plugin = my_plugin_lock(thd, &plugin);
  }
  void global_save_default(THD *thd, set_var *var) override {
    LEX_CSTRING pname;
    char **default_value = reinterpret_cast<char **>(option.def_value);
    pname.str = *default_value;
    pname.length = strlen(pname.str);

    plugin_ref plugin;
    if (plugin_type == MYSQL_STORAGE_ENGINE_PLUGIN)
      plugin = ha_resolve_by_name(thd, &pname, false);
    else {
      plugin = my_plugin_lock_by_name(thd, pname, plugin_type);
    }
    assert(plugin);

    var->save_result.plugin = my_plugin_lock(thd, &plugin);
  }
  void saved_value_to_string(THD *, set_var *var, char *def_val) override {
    strncpy(def_val, plugin_name(var->save_result.plugin)->str,
            plugin_name(var->save_result.plugin)->length);
  }
  bool check_update_type(Item_result type) override {
    return type != STRING_RESULT;
  }
  const uchar *session_value_ptr(THD *running_thd, THD *target_thd,
                                 std::string_view) override {
    plugin_ref plugin = session_var(target_thd, plugin_ref);
    return (uchar *)(plugin ? running_thd->strmake(plugin_name(plugin)->str,
                                                   plugin_name(plugin)->length)
                            : nullptr);
  }
  const uchar *global_value_ptr(THD *thd, std::string_view) override {
    plugin_ref plugin = global_var(plugin_ref);
    return (uchar *)(plugin ? thd->strmake(plugin_name(plugin)->str,
                                           plugin_name(plugin)->length)
                            : nullptr);
  }
};

#if defined(ENABLED_DEBUG_SYNC)
/**
  The class for @@debug_sync session-only variable
*/
<<<<<<< HEAD
class Sys_var_debug_sync : public sys_var {
 public:
  Sys_var_debug_sync(
      const char *name_arg, const char *comment, int flag_args, CMD_LINE getopt,
      const char *def_val, PolyLock *lock = nullptr,
      enum binlog_status_enum binlog_status_arg = VARIABLE_NOT_IN_BINLOG,
      on_check_function on_check_func = nullptr,
      on_update_function on_update_func = nullptr,
      const char *substitute = nullptr, int parse_flag = PARSE_NORMAL)
      : sys_var(&all_sys_vars, name_arg, comment, flag_args, 0, getopt.id,
                getopt.arg_type, SHOW_CHAR, (intptr)def_val, lock,
                binlog_status_arg, on_check_func, on_update_func, substitute,
                parse_flag) {
    assert(scope() == ONLY_SESSION);
    option.var_type = GET_NO_ARG;
=======
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
    assert(scope() == ONLY_SESSION);
    option.var_type= GET_NO_ARG;
>>>>>>> upstream/cluster-7.6
  }
  bool do_check(THD *thd, set_var *var) override {
    char buff[STRING_BUFFER_USUAL_SIZE];
    String str(buff, sizeof(buff), system_charset_info), *res;

    if (!(res = var->value->val_str(&str)))
      var->save_result.string_value.str = const_cast<char *>("");
    else
      var->save_result.string_value.str =
          thd->strmake(res->ptr(), res->length());
    return false;
  }
  bool session_update(THD *thd, set_var *var) override {
    return debug_sync_update(thd, var->save_result.string_value.str);
  }
<<<<<<< HEAD
  bool global_update(THD *, set_var *) override {
    assert(false);
=======
<<<<<<< HEAD
  bool global_update(THD *, set_var *) {
    DBUG_ASSERT(false);
=======
  bool global_update(THD *thd, set_var *var)
  {
    assert(FALSE);
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231
    return true;
  }
  void session_save_default(THD *, set_var *var) override {
    var->save_result.string_value.str = const_cast<char *>("");
    var->save_result.string_value.length = 0;
  }
<<<<<<< HEAD
  void global_save_default(THD *, set_var *) override { assert(false); }
  void saved_value_to_string(THD *, set_var *, char *) override {
    assert(false);
  }
  const uchar *session_value_ptr(THD *running_thd, THD *,
                                 std::string_view) override {
    return debug_sync_value_ptr(running_thd);
  }
  const uchar *global_value_ptr(THD *, std::string_view) override {
    assert(false);
    return nullptr;
  }
  bool check_update_type(Item_result type) override {
    return type != STRING_RESULT;
=======
<<<<<<< HEAD
  void global_save_default(THD *, set_var *) { DBUG_ASSERT(false); }
  uchar *session_value_ptr(THD *running_thd, THD *, LEX_STRING *) {
    return debug_sync_value_ptr(running_thd);
  }
  uchar *global_value_ptr(THD *, LEX_STRING *) {
    DBUG_ASSERT(false);
=======
  void global_save_default(THD *thd, set_var *var)
  {
    assert(FALSE);
  }
  uchar *session_value_ptr(THD *running_thd, THD *target_thd, LEX_STRING *base)
  {
    extern uchar *debug_sync_value_ptr(THD *thd);
    return debug_sync_value_ptr(running_thd);
  }
  uchar *global_value_ptr(THD *thd, LEX_STRING *base)
  {
    assert(FALSE);
>>>>>>> upstream/cluster-7.6
    return 0;
>>>>>>> pr/231
  }
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
class Sys_var_bit : public Sys_var_typelib {
  ulonglong bitmask;
  bool reverse_semantics;
  void set(uchar *ptr, ulonglong value) {
    if ((value != 0) ^ reverse_semantics)
      (*(ulonglong *)ptr) |= bitmask;
    else
      (*(ulonglong *)ptr) &= ~bitmask;
  }
<<<<<<< HEAD

 public:
  Sys_var_bit(
      const char *name_arg, const char *comment, int flag_args, ptrdiff_t off,
      size_t size [[maybe_unused]], CMD_LINE getopt, ulonglong bitmask_arg,
      bool def_val, PolyLock *lock = nullptr,
      enum binlog_status_enum binlog_status_arg = VARIABLE_NOT_IN_BINLOG,
      on_check_function on_check_func = nullptr,
      pre_update_function pre_update_func = nullptr,
      on_update_function on_update_func = nullptr,
      const char *substitute = nullptr)
      : Sys_var_typelib(name_arg, comment, flag_args, off, getopt, SHOW_MY_BOOL,
                        bool_values, def_val, lock, binlog_status_arg,
                        on_check_func, on_update_func, substitute) {
    option.var_type = GET_BOOL;
    pre_update = pre_update_func;
    reverse_semantics = my_count_bits(bitmask_arg) > 1;
    bitmask = reverse_semantics ? ~bitmask_arg : bitmask_arg;
    set(global_var_ptr(), def_val);
<<<<<<< HEAD
    assert(getopt.id == -1);  // force NO_CMD_LINE
    assert(size == sizeof(ulonglong));
=======
    DBUG_ASSERT(getopt.id == -1);  // force NO_CMD_LINE
    DBUG_ASSERT(size == sizeof(ulonglong));
=======
public:
  Sys_var_bit(const char *name_arg,
          const char *comment, int flag_args, ptrdiff_t off, size_t size,
          CMD_LINE getopt,
          ulonglong bitmask_arg, my_bool def_val, PolyLock *lock=0,
          enum binlog_status_enum binlog_status_arg=VARIABLE_NOT_IN_BINLOG,
          on_check_function on_check_func=0,
          pre_update_function pre_update_func=0,
          on_update_function on_update_func=0,
          const char *substitute=0)
    : Sys_var_typelib(name_arg, comment, flag_args, off, getopt,
                      SHOW_MY_BOOL, bool_values, def_val, lock,
                      binlog_status_arg, on_check_func, on_update_func,
                      substitute)
  {
    option.var_type= GET_BOOL;
    pre_update= pre_update_func;
    reverse_semantics= my_count_bits(bitmask_arg) > 1;
    bitmask= reverse_semantics ? ~bitmask_arg : bitmask_arg;
    set(global_var_ptr(), def_val);
    assert(def_val < 2);
    assert(getopt.id == -1); // force NO_CMD_LINE
    assert(size == sizeof(ulonglong));
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231
  }
  bool session_update(THD *thd, set_var *var) override {
    set(session_var_ptr(thd), var->save_result.ulonglong_value);
    return false;
  }
  bool global_update(THD *, set_var *var) override {
    set(global_var_ptr(), var->save_result.ulonglong_value);
    return false;
  }
  void session_save_default(THD *, set_var *var) override {
    var->save_result.ulonglong_value = global_var(ulonglong) & bitmask;
  }
  void global_save_default(THD *, set_var *var) override {
    var->save_result.ulonglong_value = option.def_value;
  }
  void saved_value_to_string(THD *, set_var *var, char *def_val) override {
    longlong10_to_str((longlong)var->save_result.ulonglong_value, def_val, 10);
  }
  const uchar *session_value_ptr(THD *running_thd, THD *target_thd,
                                 std::string_view) override {
    running_thd->sys_var_tmp.bool_value = static_cast<bool>(
        reverse_semantics ^
        ((session_var(target_thd, ulonglong) & bitmask) != 0));
    return (uchar *)&running_thd->sys_var_tmp.bool_value;
  }
  const uchar *global_value_ptr(THD *thd, std::string_view) override {
    thd->sys_var_tmp.bool_value = static_cast<bool>(
        reverse_semantics ^ ((global_var(ulonglong) & bitmask) != 0));
    return (uchar *)&thd->sys_var_tmp.bool_value;
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
class Sys_var_session_special : public Sys_var_ulonglong {
  typedef bool (*session_special_update_function)(THD *thd, set_var *var);
  typedef ulonglong (*session_special_read_function)(THD *thd);

  session_special_read_function read_func;
  session_special_update_function update_func;
<<<<<<< HEAD

 public:
  Sys_var_session_special(const char *name_arg, const char *comment,
                          int flag_args, CMD_LINE getopt, ulonglong min_val,
                          ulonglong max_val, uint block_size, PolyLock *lock,
                          enum binlog_status_enum binlog_status_arg,
                          on_check_function on_check_func,
                          session_special_update_function update_func_arg,
                          session_special_read_function read_func_arg,
                          const char *substitute = nullptr)
      : Sys_var_ulonglong(name_arg, comment, flag_args, 0, sizeof(ulonglong),
                          getopt, min_val, max_val, 0, block_size, lock,
                          binlog_status_arg, on_check_func, nullptr,
                          substitute),
        read_func(read_func_arg),
        update_func(update_func_arg) {
    assert(scope() == ONLY_SESSION);
    assert(getopt.id == -1);  // NO_CMD_LINE, because the offset is fake
  }
  bool session_update(THD *thd, set_var *var) override {
    return update_func(thd, var);
  }
  bool global_update(THD *, set_var *) override {
    assert(false);
    return true;
  }
  void session_save_default(THD *, set_var *var) override {
    var->value = nullptr;
  }
  void global_save_default(THD *, set_var *) override { assert(false); }
  void saved_value_to_string(THD *, set_var *, char *) override {
    assert(false);
  }
  const uchar *session_value_ptr(THD *running_thd, THD *target_thd,
                                 std::string_view) override {
    running_thd->sys_var_tmp.ulonglong_value = read_func(target_thd);
    return (uchar *)&running_thd->sys_var_tmp.ulonglong_value;
  }
<<<<<<< HEAD
  const uchar *global_value_ptr(THD *, std::string_view) override {
    assert(false);
    return nullptr;
=======
  uchar *global_value_ptr(THD *, LEX_STRING *) {
    DBUG_ASSERT(false);
=======
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
    assert(scope() == ONLY_SESSION);
    assert(getopt.id == -1); // NO_CMD_LINE, because the offset is fake
  }
  bool session_update(THD *thd, set_var *var)
  { return update_func(thd, var); }
  bool global_update(THD *thd, set_var *var)
  {
    assert(FALSE);
    return true;
  }
  void session_save_default(THD *thd, set_var *var)
  { var->value= 0; }
  void global_save_default(THD *thd, set_var *var)
  { assert(FALSE); }
  uchar *session_value_ptr(THD *running_thd, THD *target_thd, LEX_STRING *base)
  {
    running_thd->sys_var_tmp.ulonglong_value= read_func(target_thd);
    return (uchar*) &running_thd->sys_var_tmp.ulonglong_value;
  }
  uchar *global_value_ptr(THD *thd, LEX_STRING *base)
  {
    assert(FALSE);
>>>>>>> upstream/cluster-7.6
    return 0;
>>>>>>> pr/231
  }
};

/**
  Similar to Sys_var_session_special, but with double storage.
*/
class Sys_var_session_special_double : public Sys_var_double {
  typedef bool (*session_special_update_function)(THD *thd, set_var *var);
  typedef double (*session_special_read_double_function)(THD *thd);

  session_special_read_double_function read_func;
  session_special_update_function update_func;
<<<<<<< HEAD

 public:
  Sys_var_session_special_double(
      const char *name_arg, const char *comment, int flag_args, CMD_LINE getopt,
      double min_val, double max_val, uint, PolyLock *lock,
      enum binlog_status_enum binlog_status_arg,
      on_check_function on_check_func,
      session_special_update_function update_func_arg,
      session_special_read_double_function read_func_arg,
      const char *substitute = nullptr)
      : Sys_var_double(name_arg, comment, flag_args, 0, sizeof(double), getopt,
                       min_val, max_val, 0.0, lock, binlog_status_arg,
                       on_check_func, nullptr, substitute),
        read_func(read_func_arg),
        update_func(update_func_arg) {
    assert(scope() == ONLY_SESSION);
    assert(getopt.id == -1);  // NO_CMD_LINE, because the offset is fake
  }
  bool session_update(THD *thd, set_var *var) override {
    return update_func(thd, var);
  }
  bool global_update(THD *, set_var *) override {
    assert(false);
    return true;
  }
  void session_save_default(THD *, set_var *var) override {
    var->value = nullptr;
  }
  void global_save_default(THD *, set_var *) override { assert(false); }
  void saved_value_to_string(THD *, set_var *, char *) override {
    assert(false);
  }
  const uchar *session_value_ptr(THD *running_thd, THD *target_thd,
                                 std::string_view) override {
    running_thd->sys_var_tmp.double_value = read_func(target_thd);
    return (uchar *)&running_thd->sys_var_tmp.double_value;
  }
<<<<<<< HEAD
  const uchar *global_value_ptr(THD *, std::string_view) override {
    assert(false);
    return nullptr;
=======
  uchar *global_value_ptr(THD *, LEX_STRING *) {
    DBUG_ASSERT(false);
=======
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
    assert(scope() == ONLY_SESSION);
    assert(getopt.id == -1); // NO_CMD_LINE, because the offset is fake
  }
  bool session_update(THD *thd, set_var *var)
  { return update_func(thd, var); }
  bool global_update(THD *thd, set_var *var)
  {
    assert(FALSE);
    return true;
  }
  void session_save_default(THD *thd, set_var *var)
  { var->value= 0; }
  void global_save_default(THD *thd, set_var *var)
  { assert(FALSE); }
  uchar *session_value_ptr(THD *running_thd, THD *target_thd, LEX_STRING *base)
  {
    running_thd->sys_var_tmp.double_value= read_func(target_thd);
    return (uchar *) &running_thd->sys_var_tmp.double_value;
  }
  uchar *global_value_ptr(THD *thd, LEX_STRING *base)
  {
    assert(FALSE);
>>>>>>> upstream/cluster-7.6
    return 0;
>>>>>>> pr/231
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
<<<<<<< HEAD
class Sys_var_have : public sys_var {
 public:
  Sys_var_have(
      const char *name_arg, const char *comment, int flag_args, ptrdiff_t off,
      size_t size [[maybe_unused]], CMD_LINE getopt, PolyLock *lock = nullptr,
      enum binlog_status_enum binlog_status_arg = VARIABLE_NOT_IN_BINLOG,
      on_check_function on_check_func = nullptr,
      on_update_function on_update_func = nullptr,
      const char *substitute = nullptr, int parse_flag = PARSE_NORMAL)
      : sys_var(&all_sys_vars, name_arg, comment, flag_args, off, getopt.id,
                getopt.arg_type, SHOW_CHAR, 0, lock, binlog_status_arg,
                on_check_func, on_update_func, substitute, parse_flag) {
    assert(scope() == GLOBAL);
    assert(getopt.id == -1);
    assert(lock == nullptr);
    assert(binlog_status_arg == VARIABLE_NOT_IN_BINLOG);
    assert(is_readonly());
    assert(on_update == nullptr);
    assert(size == sizeof(enum SHOW_COMP_OPTION));
  }
  bool do_check(THD *, set_var *) override {
    assert(false);
    return true;
  }
  bool session_update(THD *, set_var *) override {
    assert(false);
    return true;
  }
  bool global_update(THD *, set_var *) override {
    assert(false);
    return true;
  }
<<<<<<< HEAD
  void session_save_default(THD *, set_var *) override {}
  void global_save_default(THD *, set_var *) override {}
  void saved_value_to_string(THD *, set_var *, char *) override {}
  const uchar *session_value_ptr(THD *, THD *, std::string_view) override {
    assert(false);
    return nullptr;
=======
  void session_save_default(THD *, set_var *) {}
  void global_save_default(THD *, set_var *) {}
  uchar *session_value_ptr(THD *, THD *, LEX_STRING *) {
    DBUG_ASSERT(false);
=======
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
    assert(scope() == GLOBAL);
    assert(getopt.id == -1);
    assert(lock == 0);
    assert(binlog_status_arg == VARIABLE_NOT_IN_BINLOG);
    assert(is_readonly());
    assert(on_update == 0);
    assert(size == sizeof(enum SHOW_COMP_OPTION));
  }
  bool do_check(THD *thd, set_var *var) {
    assert(FALSE);
    return true;
  }
  bool session_update(THD *thd, set_var *var)
  {
    assert(FALSE);
    return true;
  }
  bool global_update(THD *thd, set_var *var)
  {
    assert(FALSE);
    return true;
  }
  void session_save_default(THD *thd, set_var *var) { }
  void global_save_default(THD *thd, set_var *var) { }
  uchar *session_value_ptr(THD *running_thd, THD *target_thd, LEX_STRING *base)
  {
    assert(FALSE);
>>>>>>> upstream/cluster-7.6
    return 0;
>>>>>>> pr/231
  }
  const uchar *global_value_ptr(THD *, std::string_view) override {
    return pointer_cast<const uchar *>(
        show_comp_option_name[global_var(enum SHOW_COMP_OPTION)]);
  }
  bool check_update_type(Item_result) override { return false; }
};

/**
   A subclass of @ref Sys_var_have to return dynamic values

   All the usual restrictions for @ref Sys_var_have apply.
   But instead of reading a global variable it calls a function
   to return the value.
 */
class Sys_var_have_func : public Sys_var_have {
 public:
  /**
    Construct a new variable.

    @param name_arg The name of the variable
    @param comment  Explanation of what the variable does
    @param func     The function to call when in need to read the global value
    @param substitute If the variable is deprecated what to use instead
  */
  Sys_var_have_func(const char *name_arg, const char *comment,
                    enum SHOW_COMP_OPTION (*func)(THD *),
                    const char *substitute = nullptr)
      /*
        Note: it doesn't really matter what variable we use, as long as we are
        using one. So we use a local static dummy
      */
      : Sys_var_have(name_arg, comment,
                     READ_ONLY NON_PERSIST GLOBAL_VAR(dummy_), NO_CMD_LINE,
                     nullptr, VARIABLE_NOT_IN_BINLOG, nullptr, nullptr,
                     substitute),
        func_(func) {}

  const uchar *global_value_ptr(THD *thd, std::string_view) override {
    return pointer_cast<const uchar *>(show_comp_option_name[func_(thd)]);
  }

 protected:
  enum SHOW_COMP_OPTION (*func_)(THD *);
  static enum SHOW_COMP_OPTION dummy_;
};
/**
  Generic class for variables for storing entities that are internally
  represented as structures, have names, and possibly can be referred to by
  numbers.  Examples: character sets, collations, locales,

  Backing store: void*
  @tparam Struct_type type of struct being wrapped
  @tparam Name_getter must provide Name_getter(Struct_type*).get_name()

  @note
  As every such a structure requires special treatment from my_getopt,
  these variables don't support command-line equivalents, any such
  command-line options should be added manually to my_long_options in mysqld.cc
*/
template <typename Struct_type, typename Name_getter>
class Sys_var_struct : public sys_var {
 public:
  Sys_var_struct(
      const char *name_arg, const char *comment, int flag_args, ptrdiff_t off,
      size_t size [[maybe_unused]], CMD_LINE getopt, void *def_val,
      PolyLock *lock = nullptr,
      enum binlog_status_enum binlog_status_arg = VARIABLE_NOT_IN_BINLOG,
      on_check_function on_check_func = nullptr,
      on_update_function on_update_func = nullptr,
      const char *substitute = nullptr, int parse_flag = PARSE_NORMAL)
      : sys_var(&all_sys_vars, name_arg, comment, flag_args, off, getopt.id,
                getopt.arg_type, SHOW_CHAR, (intptr)def_val, lock,
                binlog_status_arg, on_check_func, on_update_func, substitute,
                parse_flag) {
    option.var_type = GET_STR;
    /*
      struct variables are special on the command line - often (e.g. for
      charsets) the name cannot be immediately resolved, but only after all
      options (in particular, basedir) are parsed.

      thus all struct command-line options should be added manually
      to my_long_options in mysqld.cc
    */
    assert(getopt.id == -1);
    assert(size == sizeof(void *));
  }
  bool do_check(THD *, set_var *) override { return false; }
  bool session_update(THD *thd, set_var *var) override {
    session_var(thd, const void *) = var->save_result.ptr;
    return false;
  }
  bool global_update(THD *, set_var *var) override {
    global_var(const void *) = var->save_result.ptr;
    return false;
  }
  void session_save_default(THD *, set_var *var) override {
    var->save_result.ptr = global_var(void *);
  }
  void global_save_default(THD *, set_var *var) override {
    void **default_value = reinterpret_cast<void **>(option.def_value);
    var->save_result.ptr = *default_value;
  }
  void saved_value_to_string(THD *, set_var *var, char *def_val) override {
    const Struct_type *ptr =
        static_cast<const Struct_type *>(var->save_result.ptr);
    if (ptr)
      strcpy(def_val, pointer_cast<const char *>(Name_getter(ptr).get_name()));
  }
  bool check_update_type(Item_result type) override {
    return type != INT_RESULT && type != STRING_RESULT;
  }
  const uchar *session_value_ptr(THD *, THD *target_thd,
                                 std::string_view) override {
    const Struct_type *ptr = session_var(target_thd, const Struct_type *);
    return ptr ? Name_getter(ptr).get_name() : nullptr;
  }
  const uchar *global_value_ptr(THD *, std::string_view) override {
    const Struct_type *ptr = global_var(const Struct_type *);
    return ptr ? Name_getter(ptr).get_name() : nullptr;
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
<<<<<<< HEAD
class Sys_var_tz : public sys_var {
 public:
  Sys_var_tz(const char *name_arg, const char *comment, int flag_args,
             ptrdiff_t off, size_t size [[maybe_unused]], CMD_LINE getopt,
             Time_zone **def_val, PolyLock *lock = nullptr,
             enum binlog_status_enum binlog_status_arg = VARIABLE_NOT_IN_BINLOG,
             on_check_function on_check_func = nullptr,
             on_update_function on_update_func = nullptr,
             const char *substitute = nullptr, int parse_flag = PARSE_NORMAL)
      : sys_var(&all_sys_vars, name_arg, comment, flag_args, off, getopt.id,
                getopt.arg_type, SHOW_CHAR, (intptr)def_val, lock,
                binlog_status_arg, on_check_func, on_update_func, substitute,
                parse_flag) {
<<<<<<< HEAD
    assert(getopt.id == -1);
    assert(size == sizeof(Time_zone *));
    option.var_type = GET_STR;
=======
    DBUG_ASSERT(getopt.id == -1);
    DBUG_ASSERT(size == sizeof(Time_zone *));
=======
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
    assert(getopt.id == -1);
    assert(size == sizeof(Time_zone *));
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231
  }
  bool do_check(THD *thd, set_var *var) override {
    char buff[MAX_TIME_ZONE_NAME_LENGTH];
    String str(buff, sizeof(buff), &my_charset_latin1);
    String *res = var->value->val_str(&str);

    if (!res) return true;

    if (!(var->save_result.time_zone = my_tz_find(thd, res))) {
      ErrConvString err(res);
      my_error(ER_UNKNOWN_TIME_ZONE, MYF(0), err.ptr());
      return true;
    }
    return false;
  }
  bool session_update(THD *thd, set_var *var) override {
    session_var(thd, Time_zone *) = var->save_result.time_zone;
    return false;
  }
  bool global_update(THD *, set_var *var) override {
    global_var(Time_zone *) = var->save_result.time_zone;
    return false;
  }
  void session_save_default(THD *, set_var *var) override {
    var->save_result.time_zone = global_var(Time_zone *);
  }
  void global_save_default(THD *, set_var *var) override {
    var->save_result.time_zone = *(Time_zone **)(intptr)option.def_value;
  }
  void saved_value_to_string(THD *, set_var *var, char *def_val) override {
    strcpy(def_val, var->save_result.time_zone->get_name()->ptr());
  }
  const uchar *session_value_ptr(THD *, THD *target_thd,
                                 std::string_view) override {
    /*
      This is an ugly fix for replication: we don't replicate properly queries
      invoking system variables' values to update tables; but
      CONVERT_TZ(,,@@session.time_zone) is so popular that we make it
      replicable (i.e. we tell the binlog code to store the session
      timezone). If it's the global value which was used we can't replicate
      (binlog code stores session value only).
    */
    target_thd->time_zone_used = true;
    return pointer_cast<const uchar *>(
        session_var(target_thd, Time_zone *)->get_name()->ptr());
  }
  const uchar *global_value_ptr(THD *, std::string_view) override {
    return pointer_cast<const uchar *>(
        global_var(Time_zone *)->get_name()->ptr());
  }
  bool check_update_type(Item_result type) override {
    return type != STRING_RESULT;
  }
};

/**
  Class representing the 'transaction_isolation' system variable. This
  variable can also be indirectly set using 'SET TRANSACTION ISOLATION
  LEVEL'.
*/

class Sys_var_transaction_isolation : public Sys_var_enum {
 public:
  Sys_var_transaction_isolation(const char *name_arg, const char *comment,
                                int flag_args, ptrdiff_t off, size_t size,
                                CMD_LINE getopt, const char *values[],
                                uint def_val, PolyLock *lock,
                                enum binlog_status_enum binlog_status_arg,
                                on_check_function on_check_func)
      : Sys_var_enum(name_arg, comment, flag_args, off, size, getopt, values,
                     def_val, lock, binlog_status_arg, on_check_func) {}
  bool session_update(THD *thd, set_var *var) override;
};

/**
  Class representing the tx_read_only system variable for setting
  default transaction access mode.

  Note that there is a special syntax - SET TRANSACTION READ ONLY
  (or READ WRITE) that sets the access mode for the next transaction
  only.
*/

class Sys_var_transaction_read_only : public Sys_var_bool {
 public:
  Sys_var_transaction_read_only(const char *name_arg, const char *comment,
                                int flag_args, ptrdiff_t off, size_t size,
                                CMD_LINE getopt, bool def_val, PolyLock *lock,
                                enum binlog_status_enum binlog_status_arg,
                                on_check_function on_check_func)
      : Sys_var_bool(name_arg, comment, flag_args, off, size, getopt, def_val,
                     lock, binlog_status_arg, on_check_func) {}
  bool session_update(THD *thd, set_var *var) override;
};

/**
   A class for @@global.binlog_checksum that has
   a specialized update method.
*/
class Sys_var_enum_binlog_checksum : public Sys_var_enum {
 public:
  Sys_var_enum_binlog_checksum(const char *name_arg, const char *comment,
                               int flag_args, ptrdiff_t off, size_t size,
                               CMD_LINE getopt, const char *values[],
                               uint def_val, PolyLock *lock,
                               enum binlog_status_enum binlog_status_arg,
                               on_check_function on_check_func = nullptr)
      : Sys_var_enum(name_arg, comment, flag_args | PERSIST_AS_READ_ONLY, off,
                     size, getopt, values, def_val, lock, binlog_status_arg,
                     on_check_func, nullptr) {}
  bool global_update(THD *thd, set_var *var) override;
};

/**
  Class for gtid_next.
*/
<<<<<<< HEAD
class Sys_var_gtid_next : public sys_var {
 public:
  Sys_var_gtid_next(
      const char *name_arg, const char *comment, int flag_args, ptrdiff_t off,
      size_t size [[maybe_unused]], CMD_LINE getopt, const char *def_val,
      PolyLock *lock = nullptr,
      enum binlog_status_enum binlog_status_arg = VARIABLE_NOT_IN_BINLOG,
      on_check_function on_check_func = nullptr,
      on_update_function on_update_func = nullptr,
      const char *substitute = nullptr, int parse_flag = PARSE_NORMAL)
      : sys_var(&all_sys_vars, name_arg, comment, flag_args, off, getopt.id,
                getopt.arg_type, SHOW_CHAR, (intptr)def_val, lock,
                binlog_status_arg, on_check_func, on_update_func, substitute,
                parse_flag) {
    assert(size == sizeof(Gtid_specification));
  }
<<<<<<< HEAD
  bool session_update(THD *thd, set_var *var) override;
=======
  bool session_update(THD *thd, set_var *var);
=======
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
    assert(size == sizeof(Gtid_specification));
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
      assert(var->save_result.string_value.str);
      assert(var->save_result.string_value.length);
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
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231

  bool global_update(THD *, set_var *) override {
    assert(false);
    return true;
  }
<<<<<<< HEAD
  void session_save_default(THD *, set_var *var) override {
    DBUG_TRACE;
=======
<<<<<<< HEAD
  void session_save_default(THD *, set_var *var) {
=======
  bool global_update(THD *thd, set_var *var)
  { assert(FALSE); return true; }
  void session_save_default(THD *thd, set_var *var)
  {
>>>>>>> upstream/cluster-7.6
    DBUG_ENTER("Sys_var_gtid_next::session_save_default");
>>>>>>> pr/231
    char *ptr = (char *)(intptr)option.def_value;
    var->save_result.string_value.str = ptr;
    var->save_result.string_value.length = ptr ? strlen(ptr) : 0;
    return;
  }
<<<<<<< HEAD
  void global_save_default(THD *, set_var *) override { assert(false); }
  void saved_value_to_string(THD *, set_var *, char *) override {
    assert(false);
  }
  bool do_check(THD *, set_var *) override { return false; }
  bool check_update_type(Item_result type) override {
    return type != STRING_RESULT;
  }
  const uchar *session_value_ptr(THD *running_thd, THD *target_thd,
                                 std::string_view) override {
    DBUG_TRACE;
=======
<<<<<<< HEAD
  void global_save_default(THD *, set_var *) { DBUG_ASSERT(false); }
  bool do_check(THD *, set_var *) { return false; }
  bool check_update_type(Item_result type) { return type != STRING_RESULT; }
  uchar *session_value_ptr(THD *running_thd, THD *target_thd, LEX_STRING *) {
=======
  void global_save_default(THD *thd, set_var *var)
  { assert(FALSE); }
  bool do_check(THD *thd, set_var *var)
  { return false; }
  bool check_update_type(Item_result type)
  { return type != STRING_RESULT; }
  uchar *session_value_ptr(THD *running_thd, THD *target_thd, LEX_STRING *base)
  {
>>>>>>> upstream/cluster-7.6
    DBUG_ENTER("Sys_var_gtid_next::session_value_ptr");
>>>>>>> pr/231
    char buf[Gtid_specification::MAX_TEXT_LENGTH + 1];
    global_sid_lock->rdlock();
    ((Gtid_specification *)session_var_ptr(target_thd))
        ->to_string(global_sid_map, buf);
    global_sid_lock->unlock();
    char *ret = running_thd->mem_strdup(buf);
    return (uchar *)ret;
  }
<<<<<<< HEAD
  const uchar *global_value_ptr(THD *, std::string_view) override {
    assert(false);
    return nullptr;
=======
<<<<<<< HEAD
  uchar *global_value_ptr(THD *, LEX_STRING *) {
    DBUG_ASSERT(false);
    return NULL;
>>>>>>> pr/231
  }
=======
  uchar *global_value_ptr(THD *thd, LEX_STRING *base)
  { assert(FALSE); return NULL; }
>>>>>>> upstream/cluster-7.6
};

#ifdef HAVE_GTID_NEXT_LIST
/**
  Class for variables that store values of type Gtid_set.

  The back-end storage should be a Gtid_set_or_null, and it should be
  set to null by default.  When the variable is set for the first
  time, the Gtid_set* will be allocated.
*/
<<<<<<< HEAD
class Sys_var_gtid_set : public sys_var {
 public:
  Sys_var_gtid_set(
      const char *name_arg, const char *comment, int flag_args, ptrdiff_t off,
      size_t size, CMD_LINE getopt, const char *def_val, PolyLock *lock = 0,
      enum binlog_status_enum binlog_status_arg = VARIABLE_NOT_IN_BINLOG,
      on_check_function on_check_func = 0,
      on_update_function on_update_func = 0, const char *substitute = 0,
      int parse_flag = PARSE_NORMAL)
      : sys_var(&all_sys_vars, name_arg, comment, flag_args, off, getopt.id,
                getopt.arg_type, SHOW_CHAR, (intptr)def_val, lock,
                binlog_status_arg, on_check_func, on_update_func, substitute,
                parse_flag) {
<<<<<<< HEAD
    assert(size == sizeof(Gtid_set_or_null));
=======
    DBUG_ASSERT(size == sizeof(Gtid_set_or_null));
=======
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
    assert(size == sizeof(Gtid_set_or_null));
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231
  }
  bool session_update(THD *thd, set_var *var);

  bool global_update(THD *thd, set_var *var) {
    assert(false);
    return true;
  }
<<<<<<< HEAD
  void session_save_default(THD *thd, set_var *var) {
<<<<<<< HEAD
    DBUG_TRACE;
=======
=======
  bool global_update(THD *thd, set_var *var)
  { assert(FALSE); return true; }
  void session_save_default(THD *thd, set_var *var)
  {
>>>>>>> upstream/cluster-7.6
    DBUG_ENTER("Sys_var_gtid_set::session_save_default");
>>>>>>> pr/231
    global_sid_lock->rdlock();
    char *ptr = (char *)(intptr)option.def_value;
    var->save_result.string_value.str = ptr;
    var->save_result.string_value.length = ptr ? strlen(ptr) : 0;
    global_sid_lock->unlock();
    return;
  }
<<<<<<< HEAD
  void global_save_default(THD *thd, set_var *var) { assert(false); }
  void saved_value_to_string(THD *, set_var *, char *) { assert(false); }
  bool do_check(THD *thd, set_var *var) {
    DBUG_TRACE;
=======
<<<<<<< HEAD
  void global_save_default(THD *thd, set_var *var) { DBUG_ASSERT(false); }
  bool do_check(THD *thd, set_var *var) {
=======
  void global_save_default(THD *thd, set_var *var)
  { assert(FALSE); }
  bool do_check(THD *thd, set_var *var)
  {
>>>>>>> upstream/cluster-7.6
    DBUG_ENTER("Sys_var_gtid_set::do_check");
>>>>>>> pr/231
    String str;
    String *res = var->value->val_str(&str);
    if (res == NULL) {
      var->save_result.string_value.str = NULL;
      return false;
    }
<<<<<<< HEAD
    assert(res->ptr() != NULL);
    var->save_result.string_value.str = thd->strmake(res->ptr(), res->length());
    if (var->save_result.string_value.str == NULL) {
      my_error(ER_OUT_OF_RESOURCES, MYF(0));  // thd->strmake failed
      return 1;
=======
<<<<<<< HEAD
    DBUG_ASSERT(res->ptr() != NULL);
    var->save_result.string_value.str = thd->strmake(res->ptr(), res->length());
    if (var->save_result.string_value.str == NULL) {
      my_error(ER_OUT_OF_RESOURCES, MYF(0));  // thd->strmake failed
=======
    assert(res->ptr() != NULL);
    var->save_result.string_value.str= thd->strmake(res->ptr(), res->length());
    if (var->save_result.string_value.str == NULL)
    {
      my_error(ER_OUT_OF_RESOURCES, MYF(0)); // thd->strmake failed
>>>>>>> upstream/cluster-7.6
      DBUG_RETURN(1);
>>>>>>> pr/231
    }
    var->save_result.string_value.length = res->length();
    bool ret = !Gtid_set::is_valid(res->ptr());
    return ret;
  }
  bool check_update_type(Item_result type) { return type != STRING_RESULT; }
  uchar *session_value_ptr(THD *running_thd, THD *target_thd,
                           const std::string &) override {
    DBUG_TRACE;
    Gtid_set_or_null *gsn = (Gtid_set_or_null *)session_var_ptr(target_thd);
    Gtid_set *gs = gsn->get_gtid_set();
    if (gs == NULL) return NULL;
    char *buf;
    global_sid_lock->rdlock();
    buf = (char *)running_thd->alloc(gs->get_string_length() + 1);
    if (buf)
      gs->to_string(buf);
    else
      my_error(ER_OUT_OF_RESOURCES, MYF(0));  // thd->alloc failed
    global_sid_lock->unlock();
    return (uchar *)buf;
  }
<<<<<<< HEAD
  uchar *global_value_ptr(THD *thd, const std::string &) override {
    assert(false);
=======
<<<<<<< HEAD
  uchar *global_value_ptr(THD *thd, LEX_STRING *base) {
    DBUG_ASSERT(false);
>>>>>>> pr/231
    return NULL;
  }
=======
  uchar *global_value_ptr(THD *thd, LEX_STRING *base)
  { assert(FALSE); return NULL; }
>>>>>>> upstream/cluster-7.6
};
#endif

/**
  Abstract base class for read-only variables (global or session) of
  string type where the value is generated by some function.  This
  needs to be subclassed; the session_value_ptr or global_value_ptr
  function should be overridden. Since these variables cannot be
  set at command line, they cannot be persisted.
*/
class Sys_var_charptr_func : public sys_var {
 public:
  Sys_var_charptr_func(const char *name_arg, const char *comment,
                       flag_enum flag_arg)
<<<<<<< HEAD
      : sys_var(&all_sys_vars, name_arg, comment,
                READ_ONLY NON_PERSIST flag_arg, 0 /*off*/, NO_CMD_LINE.id,
                NO_CMD_LINE.arg_type, SHOW_CHAR, (intptr)0 /*def_val*/,
                nullptr /*polylock*/, VARIABLE_NOT_IN_BINLOG,
                nullptr /*on_check_func*/, nullptr /*on_update_func*/,
                nullptr /*substitute*/, PARSE_NORMAL /*parse_flag*/) {
    assert(flag_arg == sys_var::GLOBAL || flag_arg == sys_var::SESSION ||
           flag_arg == sys_var::ONLY_SESSION);
  }
  bool session_update(THD *, set_var *) override {
    assert(false);
    return true;
  }
  bool global_update(THD *, set_var *) override {
    assert(false);
    return true;
  }
  void session_save_default(THD *, set_var *) override { assert(false); }
  void global_save_default(THD *, set_var *) override { assert(false); }
  void saved_value_to_string(THD *, set_var *, char *) override {
    assert(false);
  }
  bool do_check(THD *, set_var *) override {
    assert(false);
    return true;
  }
  bool check_update_type(Item_result) override {
    assert(false);
    return true;
  }
  const uchar *session_value_ptr(THD *, THD *, std::string_view) override {
    assert(false);
    return nullptr;
  }
  const uchar *global_value_ptr(THD *, std::string_view) override {
    assert(false);
    return nullptr;
  }
=======
    : sys_var(&all_sys_vars, name_arg, comment, READ_ONLY flag_arg,
              0/*off*/, NO_CMD_LINE.id, NO_CMD_LINE.arg_type,
              SHOW_CHAR, (intptr)0/*def_val*/,
              NULL/*polylock*/, VARIABLE_NOT_IN_BINLOG,
              NULL/*on_check_func*/, NULL/*on_update_func*/,
              NULL/*substitute*/, PARSE_NORMAL/*parse_flag*/)
  {
    assert(flag_arg == sys_var::GLOBAL || flag_arg == sys_var::SESSION ||
           flag_arg == sys_var::ONLY_SESSION);
  }
  bool session_update(THD *thd, set_var *var)
  { assert(FALSE); return true; }
  bool global_update(THD *thd, set_var *var)
  { assert(FALSE); return true; }
  void session_save_default(THD *thd, set_var *var) { assert(FALSE); }
  void global_save_default(THD *thd, set_var *var) { assert(FALSE); }
  bool do_check(THD *thd, set_var *var) { assert(FALSE); return true; }
  bool check_update_type(Item_result type) { assert(FALSE); return true; }
  virtual uchar *session_value_ptr(THD *running_thd, THD *target_thd, LEX_STRING *base)
  { assert(FALSE); return NULL; }
  virtual uchar *global_value_ptr(THD *thd, LEX_STRING *base)
  { assert(FALSE); return NULL; }
>>>>>>> upstream/cluster-7.6
};

/**
  Class for @@global.gtid_executed.
*/
class Sys_var_gtid_executed : Sys_var_charptr_func {
 public:
  Sys_var_gtid_executed(const char *name_arg, const char *comment_arg)
      : Sys_var_charptr_func(name_arg, comment_arg, GLOBAL) {}

  const uchar *global_value_ptr(THD *thd, std::string_view) override {
    DBUG_TRACE;
    global_sid_lock->wrlock();
    const Gtid_set *gs = gtid_state->get_executed_gtids();
    char *buf = (char *)thd->alloc(gs->get_string_length() + 1);
    if (buf == nullptr)
      my_error(ER_OUT_OF_RESOURCES, MYF(0));
    else
      gs->to_string(buf);
    global_sid_lock->unlock();
    return (uchar *)buf;
  }
};

/**
  Class for @@global.system_time_zone.
*/
class Sys_var_system_time_zone : Sys_var_charptr_func {
 public:
  Sys_var_system_time_zone(const char *name_arg, const char *comment_arg)
      : Sys_var_charptr_func(name_arg, comment_arg, GLOBAL) {
    is_os_charset = true;
  }

  const uchar *global_value_ptr(THD *, std::string_view) override {
    DBUG_TRACE;
    time_t current_time = time(nullptr);
    DBUG_EXECUTE_IF("set_cet_before_dst", {
      // 1616893190 => Sunday March 28, 2021 01:59:50 (am) (CET)
      current_time = 1616893190;
    });
    DBUG_EXECUTE_IF("set_cet_after_dst", {
      // 1616893200 => Sunday March 28, 2021 03:00:00 (am) (CEST)
      current_time = 1616893200;
    });

    struct tm tm_tmp;
    localtime_r(&current_time, &tm_tmp);
    return (uchar *)(tm_tmp.tm_isdst != 0 ? system_time_zone_dst_on
                                          : system_time_zone_dst_off);
  }
};

/**
  Class for @@session.gtid_purged.
*/
class Sys_var_gtid_purged : public sys_var {
 public:
  Sys_var_gtid_purged(
      const char *name_arg, const char *comment, int flag_args, ptrdiff_t off,
      size_t, CMD_LINE getopt, const char *def_val, PolyLock *lock = nullptr,
      enum binlog_status_enum binlog_status_arg = VARIABLE_NOT_IN_BINLOG,
      on_check_function on_check_func = nullptr,
      on_update_function on_update_func = nullptr,
      const char *substitute = nullptr, int parse_flag = PARSE_NORMAL)
      : sys_var(&all_sys_vars, name_arg, comment, flag_args, off, getopt.id,
                getopt.arg_type, SHOW_CHAR, (intptr)def_val, lock,
                binlog_status_arg, on_check_func, on_update_func, substitute,
                parse_flag) {}

<<<<<<< HEAD
  bool session_update(THD *, set_var *) override {
    assert(false);
    return true;
  }

  void session_save_default(THD *, set_var *) override { assert(false); }
=======
<<<<<<< HEAD
  bool session_update(THD *, set_var *) {
    DBUG_ASSERT(false);
    return true;
  }

  void session_save_default(THD *, set_var *) { DBUG_ASSERT(false); }
=======
  bool session_update(THD *thd, set_var *var)
  {
    assert(FALSE);
    return true;
  }

  void session_save_default(THD *thd, set_var *var)
  { assert(FALSE); }
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231

  bool global_update(THD *thd, set_var *var) override;

  void global_save_default(THD *, set_var *) override {
    /* gtid_purged does not have default value */
    my_error(ER_NO_DEFAULT, MYF(0), name.str);
  }
  void saved_value_to_string(THD *, set_var *, char *) override {
    my_error(ER_NO_DEFAULT, MYF(0), name.str);
  }

  bool do_check(THD *thd, set_var *var) override {
    DBUG_TRACE;
    char buf[1024];
    String str(buf, sizeof(buf), system_charset_info);
    String *res = var->value->val_str(&str);
    if (!res) return true;
    var->save_result.string_value.str = thd->strmake(res->ptr(), res->length());
    if (!var->save_result.string_value.str) {
      my_error(ER_OUT_OF_RESOURCES, MYF(0));  // thd->strmake failed
      return true;
    }
    var->save_result.string_value.length = res->length();
    bool ret =
        Gtid_set::is_valid(var->save_result.string_value.str) ? false : true;
    DBUG_PRINT("info", ("ret=%d", ret));
    return ret;
  }

  bool check_update_type(Item_result type) override {
    return type != STRING_RESULT;
  }

  const uchar *global_value_ptr(THD *thd, std::string_view) override {
    DBUG_TRACE;
    const Gtid_set *gs;
    global_sid_lock->wrlock();
    if (opt_bin_log)
      gs = gtid_state->get_lost_gtids();
    else
      /*
        When binlog is off, report @@GLOBAL.GTID_PURGED from
        executed_gtids, since @@GLOBAL.GTID_PURGED and
        @@GLOBAL.GTID_EXECUTED are always same, so we did not
        save gtid into lost_gtids for every transaction for
        improving performance.
      */
      gs = gtid_state->get_executed_gtids();
    char *buf = (char *)thd->alloc(gs->get_string_length() + 1);
    if (buf == nullptr)
      my_error(ER_OUT_OF_RESOURCES, MYF(0));
    else
      gs->to_string(buf);
    global_sid_lock->unlock();
    return (uchar *)buf;
  }

<<<<<<< HEAD
  const uchar *session_value_ptr(THD *, THD *, std::string_view) override {
    assert(false);
    return nullptr;
=======
<<<<<<< HEAD
  uchar *session_value_ptr(THD *, THD *, LEX_STRING *) {
    DBUG_ASSERT(0);
    return NULL;
>>>>>>> pr/231
  }
=======
  uchar *session_value_ptr(THD *running_thd, THD *target_thd, LEX_STRING *base)
  { assert(0); return NULL; }
>>>>>>> upstream/cluster-7.6
};

class Sys_var_gtid_owned : Sys_var_charptr_func {
 public:
  Sys_var_gtid_owned(const char *name_arg, const char *comment_arg)
      : Sys_var_charptr_func(name_arg, comment_arg, SESSION) {}

 public:
  const uchar *session_value_ptr(THD *running_thd, THD *target_thd,
                                 std::string_view) override {
    DBUG_TRACE;
    char *buf = nullptr;
    bool remote = (target_thd != running_thd);

    if (target_thd->owned_gtid.sidno == 0)
<<<<<<< HEAD
      return (uchar *)running_thd->mem_strdup("");
    else if (target_thd->owned_gtid.sidno == THD::OWNED_SIDNO_ANONYMOUS) {
      assert(gtid_state->get_anonymous_ownership_count() > 0);
      return (uchar *)running_thd->mem_strdup("ANONYMOUS");
=======
      DBUG_RETURN((uchar *)running_thd->mem_strdup(""));
<<<<<<< HEAD
    else if (target_thd->owned_gtid.sidno == THD::OWNED_SIDNO_ANONYMOUS) {
      DBUG_ASSERT(gtid_state->get_anonymous_ownership_count() > 0);
=======
    else if (target_thd->owned_gtid.sidno == THD::OWNED_SIDNO_ANONYMOUS)
    {
      assert(gtid_state->get_anonymous_ownership_count() > 0);
>>>>>>> upstream/cluster-7.6
      DBUG_RETURN((uchar *)running_thd->mem_strdup("ANONYMOUS"));
>>>>>>> pr/231
    } else if (target_thd->owned_gtid.sidno == THD::OWNED_SIDNO_GTID_SET) {
#ifdef HAVE_GTID_NEXT_LIST
      buf = (char *)running_thd->alloc(
          target_thd->owned_gtid_set.get_string_length() + 1);
      if (buf) {
        global_sid_lock->rdlock();
        target_thd->owned_gtid_set.to_string(buf);
        global_sid_lock->unlock();
      } else
        my_error(ER_OUT_OF_RESOURCES, MYF(0));
#else
<<<<<<< HEAD
      assert(0);
=======
<<<<<<< HEAD
      DBUG_ASSERT(0);
=======
      assert(0); 
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231
#endif
    } else {
      buf = (char *)running_thd->alloc(Gtid::MAX_TEXT_LENGTH + 1);
      if (buf) {
        /* Take the lock if accessing another session. */
        if (remote) global_sid_lock->rdlock();
        running_thd->owned_gtid.to_string(target_thd->owned_sid, buf);
        if (remote) global_sid_lock->unlock();
      } else
        my_error(ER_OUT_OF_RESOURCES, MYF(0));
    }
    return (uchar *)buf;
  }

  const uchar *global_value_ptr(THD *thd, std::string_view) override {
    DBUG_TRACE;
    const Owned_gtids *owned_gtids = gtid_state->get_owned_gtids();
    global_sid_lock->wrlock();
    char *buf = (char *)thd->alloc(owned_gtids->get_max_string_length());
    if (buf)
      owned_gtids->to_string(buf);
    else
      my_error(ER_OUT_OF_RESOURCES, MYF(0));  // thd->alloc failed
    global_sid_lock->unlock();
    return (uchar *)buf;
  }
};

class Sys_var_gtid_mode : public Sys_var_enum {
 public:
  Sys_var_gtid_mode(
      const char *name_arg, const char *comment, int flag_args, ptrdiff_t off,
      size_t size, CMD_LINE getopt, const char *values[], uint def_val,
      PolyLock *lock = nullptr,
      enum binlog_status_enum binlog_status_arg = VARIABLE_NOT_IN_BINLOG,
      on_check_function on_check_func = nullptr)
      : Sys_var_enum(name_arg, comment, flag_args, off, size, getopt, values,
                     def_val, lock, binlog_status_arg, on_check_func) {}

<<<<<<< HEAD
  bool global_update(THD *thd, set_var *var) override;
=======
<<<<<<< HEAD
  bool global_update(THD *thd, set_var *var);
=======
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
    assert(new_gtid_mode <= GTID_MODE_ON);

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
      assert(old_gtid_mode == GTID_MODE_ON);
      assert(new_gtid_mode == GTID_MODE_ON_PERMISSIVE);
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
    assert(lock_count >= 0);
    assert(lock_count <= 4);
    if (lock_count == 4)
      global_sid_lock->unlock();
    mysql_mutex_unlock(mysql_bin_log.get_log_lock());
    channel_map.unlock();
    gtid_mode_lock->unlock();
    DBUG_RETURN(ret);
  }
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231
};

class Sys_var_enforce_gtid_consistency : public Sys_var_multi_enum {
 public:
  Sys_var_enforce_gtid_consistency(
      const char *name_arg, const char *comment, int flag_args, ptrdiff_t off,
      size_t size, CMD_LINE getopt, const ALIAS aliases[],
      const uint value_count, uint def_val, uint command_line_no_value,
      PolyLock *lock = nullptr,
      enum binlog_status_enum binlog_status_arg = VARIABLE_NOT_IN_BINLOG,
      on_check_function on_check_func = nullptr)
      : Sys_var_multi_enum(name_arg, comment, flag_args, off, size, getopt,
                           aliases, value_count, def_val, command_line_no_value,
                           lock, binlog_status_arg, on_check_func) {}

<<<<<<< HEAD
  bool global_update(THD *thd, set_var *var) override;
};

class Sys_var_binlog_encryption : public Sys_var_bool {
 public:
  Sys_var_binlog_encryption(const char *name_arg, const char *comment,
                            int flag_args, ptrdiff_t off, size_t size,
                            CMD_LINE getopt, bool def_val, PolyLock *lock,
                            enum binlog_status_enum binlog_status_arg,
                            on_check_function on_check_func)
      : Sys_var_bool(name_arg, comment, flag_args | PERSIST_AS_READ_ONLY, off,
                     size, getopt, def_val, lock, binlog_status_arg,
                     on_check_func) {}
  bool global_update(THD *thd, set_var *var) override;
=======
<<<<<<< HEAD
  bool global_update(THD *thd, set_var *var);
=======
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

    assert(new_mode <= GTID_CONSISTENCY_MODE_WARN);

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
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231
};

#endif /* SYS_VARS_H_INCLUDED */
