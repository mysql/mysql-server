#ifndef SET_VAR_INCLUDED
#define SET_VAR_INCLUDED
/* Copyright (c) 2002, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file
  "public" interface to sys_var - server configuration variables.
*/

#include "my_config.h"

#include <sys/types.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <map>
#include "lex_string.h"
#include "my_getopt.h"    // get_opt_arg_type
#include "my_hostname.h"  // HOSTNAME_LENGTH
#include "my_inttypes.h"
#include "my_sys.h"
#include "my_systime.h"  // my_micro_time()
#include "mysql/components/services/system_variable_source_type.h"
#include "mysql/status_var.h"
#include "mysql/strings/m_ctype.h"
#include "mysql/udf_registration_types.h"
#include "mysql_com.h"           // Item_result
#include "prealloced_array.h"    // Prealloced_array
#include "sql/sql_const.h"       // SHOW_COMP_OPTION
#include "sql/sql_plugin_ref.h"  // plugin_ref
#include "typelib.h"             // TYPELIB

class Item;
class Item_func_set_user_var;
class PolyLock;
class String;
class THD;
class Time_zone;
class set_var;
class sys_var;
class sys_var_pluginvar;
struct LEX_USER;
template <class Key, class Value>
class collation_unordered_map;

using sql_mode_t = uint64_t;
typedef enum enum_mysql_show_type SHOW_TYPE;
typedef enum enum_mysql_show_scope SHOW_SCOPE;
template <class T>
class List;

extern TYPELIB bool_typelib;

struct sys_var_chain {
  sys_var *first;
  sys_var *last;
};

bool add_static_system_variable_chain(sys_var *chain);

bool add_dynamic_system_variable_chain(sys_var *chain);
void delete_dynamic_system_variable_chain(sys_var *chain);

enum enum_var_type : int {
  OPT_DEFAULT = 0,
  OPT_SESSION,
  OPT_GLOBAL,
  OPT_PERSIST,
  OPT_PERSIST_ONLY
};

/**
  A class representing one system variable - that is something
  that can be accessed as @@global.variable_name or @@session.variable_name,
  visible in SHOW xxx VARIABLES and in INFORMATION_SCHEMA.xxx_VARIABLES,
  optionally it can be assigned to, optionally it can have a command-line
  counterpart with the same name.
*/
class sys_var {
 public:
  sys_var *next;
  LEX_CSTRING name;
  /**
    If the variable has an alias in the persisted variables file, this
    should point to it.  This has the following consequences:
    - A SET PERSIST statement that sets either of the variables will
      persist both variables in the file.
    - When loading persisted variables, an occurrence of any one of
      the variables will initialize both variables.
  */
  sys_var *m_persisted_alias;
  /**
    If m_persist_alias is set, and the current variable is deprecated
    and m_persist_alias is the recommended substitute, then this flag
    should be set to true.  This has the consequence that the code
    that loads persisted variables will generate a warning if it
    encounters this variable but does not encounter the alias.
  */
  bool m_is_persisted_deprecated;
  enum flag_enum {
    GLOBAL = 0x0001,
    SESSION = 0x0002,
    ONLY_SESSION = 0x0004,
    SCOPE_MASK = 0x03FF,  // 1023
    READONLY = 0x0400,    // 1024
    ALLOCATED = 0x0800,   // 2048
    INVISIBLE = 0x1000,   // 4096
    TRI_LEVEL = 0x2000,   // 8192 - default is neither GLOBAL nor SESSION
    NOTPERSIST = 0x4000,
    HINT_UPDATEABLE = 0x8000,  // Variable is updateable using SET_VAR hint
    /**
     There can be some variables which needs to be set before plugin is loaded.
     ex: binlog_checksum needs to be set before GR plugin is loaded.
     Also, there are some variables which needs to be set before some server
     internal component initialization.
     ex: binlog_encryption needs to be set before binary and relay log
     files generation.
    */

    PERSIST_AS_READ_ONLY = 0x10000,
    /**
      Sensitive variable. If keyring is available, the variable will be
      persisted in mysqld-auto.cnf in encrypted format
    */
    SENSITIVE = 0x20000
  };
  static const int PARSE_EARLY = 1;
  static const int PARSE_NORMAL = 2;
  /**
    Enumeration type to indicate for a system variable whether
    it will be written to the binlog or not.
  */
  enum binlog_status_enum {
    VARIABLE_NOT_IN_BINLOG,
    SESSION_VARIABLE_IN_BINLOG
  } binlog_status;

  ///< Global system variable attributes.
  std::map<std::string, std::string> m_global_attributes;

 protected:
  typedef bool (*on_check_function)(sys_var *self, THD *thd, set_var *var);
  typedef bool (*pre_update_function)(sys_var *self, THD *thd, set_var *var);
  typedef bool (*on_update_function)(sys_var *self, THD *thd,
                                     enum_var_type type);

  int flags;                      ///< or'ed flag_enum values
  int m_parse_flag;               ///< either PARSE_EARLY or PARSE_NORMAL.
  const SHOW_TYPE show_val_type;  ///< what value_ptr() returns for sql_show.cc
  my_option option;               ///< min, max, default values are stored here
  PolyLock *guard;                ///< *second* lock that protects the variable
  ptrdiff_t offset;  ///< offset to the value from global_system_variables
  on_check_function on_check;
  /**
    Pointer to function to be invoked before updating system variable (but
    after calling on_check hook), while we do not hold any locks yet.
  */
  pre_update_function pre_update;
  on_update_function on_update;
  const char *const deprecation_substitute;
  bool is_os_charset;  ///< true if the value is in character_set_filesystem
  struct get_opt_arg_source source;
  char user[USERNAME_CHAR_LENGTH + 1]; /* which user  has set this variable */
  char host[HOSTNAME_LENGTH + 1];      /* host on which this variable is set */
  ulonglong timestamp; /* represents when this variable was set */

 public:
  sys_var(sys_var_chain *chain, const char *name_arg, const char *comment,
          int flag_args, ptrdiff_t off, int getopt_id,
          enum get_opt_arg_type getopt_arg_type, SHOW_TYPE show_val_type_arg,
          longlong def_val, PolyLock *lock,
          enum binlog_status_enum binlog_status_arg,
          on_check_function on_check_func, on_update_function on_update_func,
          const char *substitute, int parse_flag,
          sys_var *persisted_alias = nullptr,
          bool is_persisted_deprecated = false);

  virtual ~sys_var() = default;

  const char *get_deprecation_substitute() { return deprecation_substitute; }
  /**
    All the cleanup procedures should be performed here
  */
  virtual void cleanup() {}
  /**
    downcast for sys_var_pluginvar. Returns this if it's an instance
    of sys_var_pluginvar, and 0 otherwise.
  */
  virtual sys_var_pluginvar *cast_pluginvar() { return nullptr; }

  bool check(THD *thd, set_var *var);
  const uchar *value_ptr(THD *running_thd, THD *target_thd, enum_var_type type,
                         std::string_view keycache_name);
  const uchar *value_ptr(THD *thd, enum_var_type type,
                         std::string_view keycache_name);
  virtual void update_default(longlong new_def_value) {
    option.def_value = new_def_value;
  }
  virtual longlong get_default() { return option.def_value; }
  virtual longlong get_min_value() { return option.min_value; }
  virtual ulonglong get_max_value() { return option.max_value; }
  /**
    Returns variable type.

    @return variable type
  */
  virtual ulong get_var_type() { return (option.var_type & GET_TYPE_MASK); }
  virtual void set_arg_source(get_opt_arg_source *) {}
  virtual void set_is_plugin(bool) {}
  virtual enum_variable_source get_source() { return source.m_source; }
  virtual const char *get_source_name() { return source.m_path_name; }
  virtual void set_source(enum_variable_source src) {
    option.arg_source->m_source = src;
  }
  virtual bool set_source_name(const char *path) {
    return set_and_truncate(option.arg_source->m_path_name, path,
                            sizeof(option.arg_source->m_path_name));
  }
  virtual bool set_user(const char *usr) {
    return set_and_truncate(user, usr, sizeof(user));
  }
  virtual const char *get_user() { return user; }
  virtual const char *get_host() { return host; }
  virtual bool set_host(const char *hst) {
    return set_and_truncate(host, hst, sizeof(host));
  }
  virtual ulonglong get_timestamp() const { return timestamp; }
  virtual void set_user_host(THD *thd);
  my_option *get_option() { return &option; }
  virtual void set_timestamp() { timestamp = my_micro_time(); }
  virtual void set_timestamp(ulonglong ts) { timestamp = ts; }
  virtual bool is_non_persistent() { return flags & NOTPERSIST; }

  /**
     Update the system variable with the default value from either
     session or global scope.  The default value is stored in the
     'var' argument. Return false when successful.
  */
  bool set_default(THD *thd, set_var *var);
  bool update(THD *thd, set_var *var);

  /**
    This function converts value stored in save_result to string. This
    function must be called after calling save_default() as save_default() will
    store default value to save_result.
  */
  virtual void saved_value_to_string(THD *thd, set_var *var, char *def_val) = 0;

  SHOW_TYPE show_type() { return show_val_type; }
  int scope() const { return flags & SCOPE_MASK; }
  const CHARSET_INFO *charset(THD *thd);
  bool is_readonly() const { return flags & READONLY; }
  bool not_visible() const { return flags & INVISIBLE; }
  bool is_trilevel() const { return flags & TRI_LEVEL; }
  bool is_persist_readonly() const { return flags & PERSIST_AS_READ_ONLY; }
  bool is_parse_early() const { return (m_parse_flag == PARSE_EARLY); }
  bool is_sensitive() const { return flags & SENSITIVE; }

  /**
    Check if the variable can be set using SET_VAR hint.

    @return true if the variable can be set using SET_VAR hint,
            false otherwise.
  */
  bool is_hint_updateable() const { return flags & HINT_UPDATEABLE; }
  /**
    the following is only true for keycache variables,
    that support the syntax @@keycache_name.variable_name
  */
  bool is_struct() { return option.var_type & GET_ASK_ADDR; }
  /*
    Indicates whether this system variable is written to the binlog or not.

    Variables are written to the binlog as part of "status_vars" in
    Query_log_event, as an Intvar_log_event, or a Rand_log_event.

    @return true if the variable is written to the binlog, false otherwise.
  */
  bool is_written_to_binlog(enum_var_type type) {
    return type != OPT_GLOBAL && binlog_status == SESSION_VARIABLE_IN_BINLOG;
  }
  virtual bool check_update_type(Item_result type) = 0;

  /**
    Return true for success if:
      Global query and variable scope is GLOBAL or SESSION, or
      Session query and variable scope is SESSION or ONLY_SESSION.
  */
  bool check_scope(enum_var_type query_type) {
    switch (query_type) {
      case OPT_PERSIST:
      case OPT_PERSIST_ONLY:
      case OPT_GLOBAL:
        return scope() & (GLOBAL | SESSION);
      case OPT_SESSION:
        return scope() & (SESSION | ONLY_SESSION);
      case OPT_DEFAULT:
        return scope() & (SESSION | ONLY_SESSION);
    }
    return false;
  }
  bool is_global_persist(enum_var_type type) {
    return (type == OPT_GLOBAL || type == OPT_PERSIST ||
            type == OPT_PERSIST_ONLY);
  }

  /**
    Return true if settable at the command line
  */
  bool is_settable_at_command_line() { return option.id != -1; }

  bool register_option(std::vector<my_option> *array, int parse_flags) {
    return is_settable_at_command_line() && (m_parse_flag & parse_flags) &&
           (array->push_back(option), false);
  }
  void do_deprecated_warning(THD *thd);
  /**
    Create item from system variable value.

    @param  thd  pointer to THD object

    @return pointer to Item object or NULL if it's
            impossible to obtain the value.
  */
  Item *copy_value(THD *thd);

  void save_default(THD *thd, set_var *var) { global_save_default(thd, var); }

  bool check_if_sensitive_in_context(THD *, bool suppress_errors = true) const;

 private:
  /**
    Like strncpy, but ensures the destination is '\0'-terminated.  Is
    also safe to call if dst==string (but not if they overlap in any
    other way).

    @param dst Target string
    @param string Source string
    @param sizeof_dst Size of the dst buffer
    @retval false The entire string was copied to dst
    @retval true strlen(string) was bigger than or equal to sizeof_dst, so
    dst contains only the sizeof_dst-1 first characters of string.
  */
  inline static bool set_and_truncate(char *dst, const char *string,
                                      size_t sizeof_dst) {
    if (dst == string) return false;
    const size_t string_length = strlen(string);
    const size_t length = std::min(sizeof_dst - 1, string_length);
    memcpy(dst, string, length);
    dst[length] = 0;
    return length < string_length;  // truncated
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

 protected:
  /**
    A pointer to a value of the variable for SHOW.
    It must be of show_val_type type (bool for SHOW_BOOL, int for SHOW_INT,
    longlong for SHOW_LONGLONG, etc).
  */
  virtual const uchar *session_value_ptr(THD *running_thd, THD *target_thd,
                                         std::string_view keycache_name);
  virtual const uchar *global_value_ptr(THD *thd,
                                        std::string_view keycache_name);

  /**
    A pointer to a storage area of the variable, to the raw data.
    Typically it's the same as session_value_ptr(), but it's different,
    for example, for ENUM, that is printed as a string, but stored as a number.
  */
  uchar *session_var_ptr(THD *thd);

  uchar *global_var_ptr();

  friend class Sys_var_alias;
};

enum class Suppress_not_found_error { NO, YES };

enum class Force_sensitive_system_variable_access { NO, YES };

enum class Is_already_locked { NO, YES };

enum class Is_single_thread { NO, YES };

/**
  Wrapper interface for all kinds of system variables.

  The interface encapsulates parse- and execution-time resolvers for all kinds
  of sys_var:
  * regular (static) system variables
  * MyISAM Multiple Key Cache variables
  * plugin-registered variables
  * component-registered variables
*/
/*
  There are 4 different sorts of system variables in MySQL:

  1. Static system variables.

  2. MyISAM Multiple Key Cache variables A.K.A. "Structured Variables" (the
     latest is easy to confuse with component-registered variables, so here
     and below the "Multiple Key Cache variable" or just "key cache variable"
     name is preferred.

  3. Plugin-registered variables.

  4. Component-registered variables.

  While they share the same internals/data structures for resolving them by
  name, they have different naming conventions, lifetimes, and lock policies
  at the same time.


  How to differentiate sorts of system variables by their syntax in SQL
  ---------------------------------------------------------------------

  Common note: the "@@" prefix is optional in lvalue syntax (left-hand sides of
  assignments in the SET statement) and mandatory in rvalue syntax (query
  blocks).

  1. Static system variable names have the simplest syntax:

     ["@@"][<scope> "."]<name>

     where <scope> ::= SESSION | LOCAL | GLOBAL | PERSIST | PERSIST_ONLY

     Thus, static system variable names can be confused with plugin-registered
     variables names or reduced forms of key cache variable names.

  2. Key cache variables can have either simple (reduced) or structured syntax:

     structured:

     ["@@"][<scope> "."][<cache-name> | DEFAULT "."]<cache-variable>

     simple (reduced):

     ["@@"][<scope> "."]<cache-variable>

     where <scope> ::= GLOBAL | PERSIST | PERSIST_ONLY

     and <cache-variable> ::= key_buffer_size |
                              key_cache_block_size |
                              key_cache_division_limit |
                              key_cache_age_threshold

     Semantically, a name of the reduced form "cache_name_foo" is equivalent to
     the structured name "DEFAULT.cache_name_foo".

     Thus, key cache variable names of the simple form can be confused with
     static system variable names while names of the structured form can be
     confused with component-registered variable names.

  3. Plugin-registered variables have almost same simple syntax as static
     system variables:

     ["@@"][<scope> "."]<name>

     where <scope> ::= GLOBAL | PERSIST | PERSIST_ONLY

  4. Component-registered variable name syntax reminds the structured syntax
     of key cache variables:

     ["@@"][<scope> "."]<component-name> "." <variable-name>

     where <scope> ::= GLOBAL | PERSIST | PERSIST_ONLY

  Note on overlapping names:

  Component-registered and key cache variable names have a similar structured
  syntax (dot-separated).
  Plugin-registered and static system have similar syntax too (plain
  identifiers).
  To manage name conflicts, the server rejects registration of
  plugin-registered variable names coinciding with compiled-in names
  of static system variables.
  OTOH, the server silently accepts component-registered variable names like
  key_buffer_size etc. (conflict with qualified key cache variables), so
  those newly-registered variables won't be easily accessible via SQL.


  API
  ---

  All 4 sorts of system variables share the same interface: sys_var.

  See following paragraph "Lifetime" for API return value lifetime details.

  Common note about using variable names as API parameter:
  the search key is

    * case-insensitive
    * doesn't include the "@@" prefix
    * doesn't include a scope prefix


  Lifetimes
  ---------

  1. Static system variable definitions are compiled into the server binary
     and have a runtime-wide life time, so pointers to their sys_var objects
     are stable.

  2. While key cache variables are dynamic by their nature, their sys_var
     object pointers are stable, since all key caches (including DEFAULT) do
     share 4 always existing sys_var objects. So, pointers to key cache
     sys_var objects are as stable as pointers to sys_var objects of static
     system variables.

  3. Plugin-registered variables can be unregistered by the UNINSTALL PLUGIN
     statement any time, so pointers to their sys_var objects are unstable
     between references to them. To make sys_var pointers stable
     during a single query execution, the plugin library maintains:

       * internally: a reference counting
       * in the outer world: the LEX::plugins release list.

  4. Component-registered variables can be unregistered by the UNINSTALL
     COMPONENT statement any time, so pointers to their sys_var objects are
     unstable.


  Locking internals
  -----------------

  1. Static system variables and key cache variables don't need/cause a lock on
     LOCK_system_variables_hash for resolving their sys_var objects since they
     use a separate stable dictionary: static_system_variable_hash.

  2. Both plugin-registered and component-registered variables share the same
     dynamic_system_variable_hash dictionary, and, normally, both need to lock
     LOCK_system_variables_hash while resolving their sys_var objects and
     accessing variable data.
*/

/**
  Encapsulation of system variable resolving and data accessing tasks.
*/
class System_variable_tracker final {
  struct Static {};
  struct Keycache {};
  struct Plugin {};
  struct Component {};

  System_variable_tracker(Static, sys_var *var);

  System_variable_tracker(Keycache, std::string_view cache_name, sys_var *var);

  System_variable_tracker(Plugin, std::string_view name);

  System_variable_tracker(Component, std::string_view dot_separated_name);
  System_variable_tracker(Component, std::string_view component_name,
                          std::string_view variable_name);

 public:
  enum Lifetime {
    STATIC,     ///< for regular static variables
    KEYCACHE,   ///< for MyISAM Multiple Key Cache variables
    PLUGIN,     ///< for plugin-registered system variables
    COMPONENT,  ///< for component-registered variables
  };

  System_variable_tracker(const System_variable_tracker &);

  ~System_variable_tracker();

  void operator=(System_variable_tracker &&);

  /**
    Static "constructor"

    @note This function requires no locks and allocates nothing on memory heaps.

    @param prefix
        One of: component name, MyISAM Multiple Key Cache name, or empty string.

    @param suffix
        In a dot-separated syntax (@p prefix is mandatory):
        component-registered variable name, or
        MyISAM Multiple Key Cache property name.
        Otherwise (@p is empty): name of static or plugin-registered variables.

    @returns System_variable_tracker object
  */
  static System_variable_tracker make_tracker(std::string_view prefix,
                                              std::string_view suffix);
  /**
    Static "constructor"

    @note This function requires no locks and allocates nothing on memory heaps.

    @param multipart_name
        Variable name, optionally dot-separated.

    @returns System_variable_tracker object
  */
  static System_variable_tracker make_tracker(std::string_view multipart_name);

  Lifetime lifetime() const { return m_tag; }

  bool operator==(const System_variable_tracker &x) const {
    if (m_tag != x.m_tag) {
      return false;
    }
    switch (m_tag) {
      case STATIC:
        return m_static.m_static_var == x.m_static.m_static_var;
      case KEYCACHE:
        return m_keycache.m_keycache_var == x.m_keycache.m_keycache_var;
      case PLUGIN:
        return names_are_same(m_plugin.m_plugin_var_name,
                              x.m_plugin.m_plugin_var_name);
      case COMPONENT:
        return names_are_same(m_component.m_component_var_name,
                              x.m_component.m_component_var_name);
    }
    my_abort();  // to make compiler happy
  }

  bool is_keycache_var() const { return m_tag == KEYCACHE; }

  bool eq_static_sys_var(const sys_var *var) const {
    return m_tag == STATIC && m_static.m_static_var == var;
  }

  /**
    @returns 1) normalized names of regular system variables;
             2) user-specified dot-separated names of key cache variables, or
                user-specified unqualified names of default key cache
                property names;
             3) user-specified names of plugin-registered variables;
             4) user-specified dot-separated names of component-registered
                variables.
  */
  const char *get_var_name() const;

  /**
    @returns 1) not normalized names of MyISAM Multiple Key Caches,
                also can return empty string for the implicit
                prefix (i.e. implicit "DEFAULT.").
             2) empty string for the rest of system variables.
  */
  std::string_view get_keycache_name() const {
    return m_tag == KEYCACHE ? std::string_view{m_keycache.m_keycache_var_name,
                                                m_keycache.m_keycache_name_size}
                             : std::string_view{};
  }

  /**
    @returns true if the underlying variable can be referenced in the
             SET_VAR optimizer hint syntax, otherwise false.
  */
  bool is_hint_updateable() const {
    return m_tag == STATIC && m_static.m_static_var->is_hint_updateable();
  }

  /**
    Safely pass a sys_var object to a function. Non-template variant.

    access_system_variable() is the most important interface:
    it does necessary locks,
    if a dynamic variable is inaccessible it exits,
    otherwise it calls @p function.
    access_system_variable() is reentrant:
    being called recursively it suppresses double locks.

    Simplified pseudo code of access_system_variable() implementation:

        if system variable is @@session_track_system_variables {
          // a special case: the system variable is static,
          // but it references a comma-separated list of
          // other system variables
          if function is not no-op {
            process @@session_track_system_variables as plugin-registered one
          } else {
            return false  // success
          }
        }
        if system variable is static or key cache variable {
          // no dictionary/plugin locks needed
          call function(cached sys_var pointer)
          return false  // success
        }
        if system variable is plugin-registered one {
          if need to hold LOCK_system_variables_hash {
            acquire read-only LOCK_system_variables_hash
          }
          if need to hold LOCK_plugin {
            acquire LOCK_plugin, unlock on exit
          }
          find sys_var
          if found {
            intern_plugin_lock
          }
          if this function acquired LOCK_plugin {
            release LOCK_plugin
          }
          if this function acquired LOCK_system_variables_hash {
            release LOCK_system_variables_hash
          }
          if not found or plugin is not in the PLUGIN_IS_READY state {
            return true  // error: variable not found or
          }
          call function(found sys_var pointer)
          return false  // success
        }
        if system variable is component-registered one {
          if need to hold LOCK_system_variables_hash(**) {
            acquire read-only LOCK_system_variables_hash, unlock on exit
          }
          find sys_var
          if not found {
            return true
          }
          call function(found sys_var pointer)
          return false  // success
        }

    @param thd
        A connection handler or nullptr.
    @param function
        An optional anonymous function to pass a sys_var object if the latest
        is accessible.
    @param suppress_not_found_error
        Suppress or output ER_UNKNOWN_SYSTEM_VARIABLE if a dynamic variable is
        unavailable.
    @param force_sensitive_variable_access
        Suppress privilege check for SENSITIVE variables.
    @param is_already_locked
        YES means LOCK_system_variables_hash has already been taken.
    @param is_single_thread
        YES means we are called from mysqld_main, during bootstrap.

    @returns True if the dynamic variable is unavailable, otherwise false.
  */
  bool access_system_variable(
      THD *thd,
      std::function<void(const System_variable_tracker &, sys_var *)> function =
          {},
      Suppress_not_found_error suppress_not_found_error =
          Suppress_not_found_error::NO,
      Force_sensitive_system_variable_access force_sensitive_variable_access =
          Force_sensitive_system_variable_access::NO,
      Is_already_locked is_already_locked = Is_already_locked::NO,
      Is_single_thread is_single_thread = Is_single_thread::NO) const;

  /**
    Safely pass a sys_var object to a function. Template variant.

    access_system_variable() is the most important interface:
    it does necessary locks,
    if a dynamic variable is inaccessible then access_system_variable() exits
    returning empty std::option,
    otherwise it calls @p function and returns the result value of @p function
    to the caller.

    See the pseudo code part at the comment above a signature of the
    non-template variant of access_system_variable() for details (replace
    "return true" with "return std::option{}" and "return false" with "return
    result value of anonymous function call").

    @tparam T
        A type of a return value of the callback function.

    @param thd
        A connection handler or nullptr.
    @param function
        A function to pass a sys_var object if the latest is accessible.
    @param suppress_not_found_error
        Suppress or output ER_UNKNOWN_SYSTEM_VARIABLE if a dynamic variable is
        unavailable.
    @param force_sensitive_variable_access
        Suppress privilege check for SENSITIVE variables.
    @param is_already_locked
        YES means LOCK_system_variables_hash has already been taken.
    @param is_single_thread
        YES means we are called from mysqld_main, during bootstrap.

    @returns
      No value if the dynamic variable is unavailable, otherwise a value of
      type T returned by the callback function.
  */
  template <typename T>
  std::optional<T> access_system_variable(
      THD *thd,
      std::function<T(const System_variable_tracker &, sys_var *)> function,
      Suppress_not_found_error suppress_not_found_error =
          Suppress_not_found_error::NO,
      Force_sensitive_system_variable_access force_sensitive_variable_access =
          Force_sensitive_system_variable_access::NO,
      Is_already_locked is_already_locked = Is_already_locked::NO,
      Is_single_thread is_single_thread = Is_single_thread::NO) const {
    T result;
    auto wrapper = [function, &result](const System_variable_tracker &t,
                                       sys_var *v) {
      result = function(t, v);  // clang-format
    };
    return access_system_variable(thd, wrapper, suppress_not_found_error,
                                  force_sensitive_variable_access,
                                  is_already_locked, is_single_thread)
               ? std::optional<T>{}
               : std::optional<T>{result};
  }

  SHOW_TYPE cached_show_type() const {
    if (!m_cache.has_value()) my_abort();
    return m_cache.value().m_cached_show_type;
  }

  bool cached_is_sensitive() const {
    if (!m_cache.has_value()) my_abort();
    return m_cache.value().m_cached_is_sensitive;
  }

  bool cached_is_applied_as_command_line() const {
    if (!m_cache.has_value()) my_abort();
    return m_cache.value().m_cached_is_applied_as_command_line;
  }

  /** Number of system variable elements to preallocate. */
  static constexpr size_t SYSTEM_VARIABLE_PREALLOC = 200;

  typedef Prealloced_array<System_variable_tracker, SYSTEM_VARIABLE_PREALLOC>
      Array;
  friend Array;  // for Array::emplace_back()

  static bool enumerate_sys_vars(bool sort, enum enum_var_type type,
                                 bool strict, Array *output);

 private:
  static bool enumerate_sys_vars_in_hash(
      collation_unordered_map<std::string, sys_var *> *hash,
      enum enum_var_type query_scope, bool strict,
      System_variable_tracker::Array *output);

  static bool names_are_same(const char *, const char *);

  sys_var *get_stable_var() const {
    switch (m_tag) {
      case STATIC:
        return m_static.m_static_var;
      case KEYCACHE:
        return m_keycache.m_keycache_var;
      default:
        assert(false);
        return nullptr;  // should never happen
    }
  }

 private:
  bool visit_plugin_variable(THD *, std::function<bool(sys_var *)>,
                             Suppress_not_found_error, Is_already_locked,
                             Is_single_thread) const;

  bool visit_component_variable(THD *, std::function<bool(sys_var *)>,
                                Suppress_not_found_error, Is_already_locked,
                                Is_single_thread) const;

  void cache_metadata(THD *thd, sys_var *v) const;

 private:
  Lifetime m_tag;

  struct Cache {
    SHOW_TYPE m_cached_show_type;
    bool m_cached_is_sensitive;
    bool m_cached_is_applied_as_command_line;
  };
  mutable std::optional<Cache> m_cache;

  /**
    A non-zero value suppresses LOCK_system_variables_hash guards in
    System_variable_tracker::access_system_variable
  */
  thread_local static int m_hash_lock_recursion_depth;

  union {
    struct {
      sys_var *m_static_var;
    } m_static;  // when m_tag == STATIC

    struct {
      /// A dot-separated key cache name or, for a reduced form of key cache
      /// variable names, a key cache property name as specified by the
      /// caller of System_variable_tracker::make_traker().
      char m_keycache_var_name[NAME_LEN + sizeof(".key_cache_division_limit")];
      size_t m_keycache_name_size;
      sys_var *m_keycache_var;
    } m_keycache;  // when m_tag == KEYCACHE

    struct {
      /// A "_"-separated plugin-registered variables name.
      char m_plugin_var_name[NAME_LEN + 1];
      mutable sys_var *m_plugin_var_cache;
    } m_plugin;  // when m_tag == PLUGIN

    struct {
      /// A dot-separated component-registered variable name.
      char m_component_var_name[NAME_LEN + sizeof('.') + NAME_LEN + 1];
      mutable sys_var *m_component_var_cache;
    } m_component;
  };
};

/****************************************************************************
  Classes for parsing of the SET command
****************************************************************************/

/**
  A base class for everything that can be set with SET command.
  It's similar to Items, an instance of this is created by the parser
  for every assignment in SET (or elsewhere, e.g. in SELECT).
*/
class set_var_base {
 public:
  set_var_base() = default;
  virtual ~set_var_base() = default;
  virtual int resolve(THD *thd) = 0;  ///< Check privileges & fix_fields
  virtual int check(THD *thd) = 0;    ///< Evaluate the expression
  virtual int update(THD *thd) = 0;   ///< Set the value
  virtual bool print(const THD *thd, String *str) = 0;  ///< To self-print

  /**
    @returns whether this variable is @@@@optimizer_trace.
  */
  virtual bool is_var_optimizer_trace() const { return false; }
  virtual void cleanup() {}

  /**
    Used only by prepared statements to resolve and check. No locking of tables
    between the two phases.
  */
  virtual int light_check(THD *thd) { return (resolve(thd) || check(thd)); }

  /** Used to identify if variable is sensitive or not */
  virtual bool is_sensitive() const { return false; }
};

/**
  set_var_base descendant for assignments to the system variables.
*/
class set_var : public set_var_base {
 public:
  Item *value;  ///< the expression that provides the new value of the variable
  const enum_var_type type;
  union  ///< temp storage to hold a value between sys_var::check and ::update
  {
    ulonglong ulonglong_value;  ///< for all integer, set, enum sysvars
    double double_value;        ///< for Sys_var_double
    plugin_ref plugin;          ///< for Sys_var_plugin
    Time_zone *time_zone;       ///< for Sys_var_tz
    LEX_STRING string_value;    ///< for Sys_var_charptr and others
    const void *ptr;            ///< for Sys_var_struct
  } save_result;

  ///< Resolver of the variable at the left hand side of the assignment.
  const System_variable_tracker m_var_tracker;

 public:
  set_var(enum_var_type type_arg, const System_variable_tracker &var_arg,
          Item *value_arg)
      : value{value_arg}, type{type_arg}, m_var_tracker(var_arg) {}

  int resolve(THD *thd) override;
  int check(THD *thd) override;
  int update(THD *thd) override;
  int light_check(THD *thd) override;
  /**
    Print variable in short form.

    @param thd Thread handle.
    @param str String buffer to append the partial assignment to.
  */
  void print_short(const THD *thd, String *str);
  bool print(const THD *, String *str) override; /* To self-print */
  bool is_global_persist() {
    return (type == OPT_GLOBAL || type == OPT_PERSIST ||
            type == OPT_PERSIST_ONLY);
  }
  bool is_var_optimizer_trace() const override {
    extern sys_var *Sys_optimizer_trace_ptr;
    return m_var_tracker.eq_static_sys_var(Sys_optimizer_trace_ptr);
  }

  bool is_sensitive() const override;

  void update_source_user_host_timestamp(THD *thd, sys_var *var);
};

/* User variables like @my_own_variable */
class set_var_user : public set_var_base {
  Item_func_set_user_var *user_var_item;

 public:
  set_var_user(Item_func_set_user_var *item) : user_var_item(item) {}
  int resolve(THD *thd) override;
  int check(THD *thd) override;
  int update(THD *thd) override;
  int light_check(THD *thd) override;
  bool print(const THD *thd, String *str) override; /* To self-print */
};

class set_var_password : public set_var_base {
  LEX_USER *user;
  char *password;
  const char *current_password;
  bool retain_current_password;
  bool generate_password;
  char *str_generated_password;

 public:
  set_var_password(LEX_USER *user_arg, char *password_arg,
                   char *current_password_arg, bool retain_current,
                   bool generate_password);

  const LEX_USER *get_user(void) { return user; }
  bool has_generated_password(void) { return generate_password; }
  const char *get_generated_password(void) { return str_generated_password; }
  int resolve(THD *) override { return 0; }
  int check(THD *thd) override;
  int update(THD *thd) override;
  bool print(const THD *thd, String *str) override; /* To self-print */
  ~set_var_password() override;
};

/* For SET NAMES and SET CHARACTER SET */

class set_var_collation_client : public set_var_base {
  int set_cs_flags;
  const CHARSET_INFO *character_set_client;
  const CHARSET_INFO *character_set_results;
  const CHARSET_INFO *collation_connection;

 public:
  enum set_cs_flags_enum {
    SET_CS_NAMES = 1,
    SET_CS_DEFAULT = 2,
    SET_CS_COLLATE = 4
  };
  set_var_collation_client(int set_cs_flags_arg,
                           const CHARSET_INFO *client_coll_arg,
                           const CHARSET_INFO *connection_coll_arg,
                           const CHARSET_INFO *result_coll_arg)
      : set_cs_flags(set_cs_flags_arg),
        character_set_client(client_coll_arg),
        character_set_results(result_coll_arg),
        collation_connection(connection_coll_arg) {}
  int resolve(THD *) override { return 0; }
  int check(THD *thd) override;
  int update(THD *thd) override;
  bool print(const THD *thd, String *str) override; /* To self-print */
};

/* optional things, have_* variables */
extern SHOW_COMP_OPTION have_profiling;

extern SHOW_COMP_OPTION have_symlink, have_dlopen;
extern SHOW_COMP_OPTION have_query_cache;
extern SHOW_COMP_OPTION have_geometry, have_rtree_keys;
extern SHOW_COMP_OPTION have_compress;
extern SHOW_COMP_OPTION have_statement_timeout;

/*
  Helper functions
*/
ulong get_system_variable_count(void);
ulonglong get_dynamic_system_variable_hash_version(void);
collation_unordered_map<std::string, sys_var *>
    *get_static_system_variable_hash(void);
collation_unordered_map<std::string, sys_var *>
    *get_dynamic_system_variable_hash(void);

bool get_global_variable_attributes(
    const char *variable_base, const char *variable_name,
    std::vector<std::pair<std::string, std::string>> &attributes);
bool get_global_variable_attribute(const char *variable_base,
                                   const char *variable_name,
                                   const char *attribute_name,
                                   std::string &value);
bool set_global_variable_attribute(const char *variable_base,
                                   const char *variable_name,
                                   const char *attribute_name,
                                   const char *attribute_value);
bool set_global_variable_attribute(const System_variable_tracker &var_tracker,
                                   const char *attribute_name,
                                   const char *attribute_value);

extern bool get_sysvar_source(const char *name, uint length,
                              enum enum_variable_source *source);

int sql_set_variables(THD *thd, List<set_var_base> *var_list, bool opened);
bool keyring_access_test();
bool fix_delay_key_write(sys_var *self, THD *thd, enum_var_type type);

sql_mode_t expand_sql_mode(sql_mode_t sql_mode, THD *thd);
bool sql_mode_string_representation(THD *thd, sql_mode_t sql_mode,
                                    LEX_STRING *ls);
bool sql_mode_quoted_string_representation(THD *thd, sql_mode_t sql_mode,
                                           LEX_STRING *ls);

extern sys_var *Sys_autocommit_ptr;
extern sys_var *Sys_gtid_next_ptr;
extern sys_var *Sys_gtid_next_list_ptr;
extern sys_var *Sys_gtid_purged_ptr;

extern ulonglong dynamic_system_variable_hash_version;

const CHARSET_INFO *get_old_charset_by_name(const char *old_name);

int sys_var_init();
int sys_var_add_options(std::vector<my_option> *long_options, int parse_flags);
void sys_var_end(void);

/* check needed privileges to perform SET PERSIST[_only] or RESET PERSIST */
bool check_priv(THD *thd, bool static_variable);

#define PERSIST_ONLY_ADMIN_X509_SUBJECT "persist_only_admin_x509_subject"
#define PERSISTED_GLOBALS_LOAD "persisted_globals_load"
extern char *sys_var_persist_only_admin_x509_subject;

#endif
