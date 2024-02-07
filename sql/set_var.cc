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

#include "sql/set_var.h"

#include <sys/types.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>

#include "m_string.h"
#include "map_helpers.h"
#include "my_dbug.h"
#include "my_io.h"
#include "my_sys.h"
#include "mysql/components/services/bits/psi_bits.h"
#include "mysql/components/services/log_builtins.h"
#include "mysql/components/services/log_shared.h"
#include "mysql/my_loglevel.h"
#include "mysql/plugin_audit.h"
#include "mysql/psi/mysql_mutex.h"
#include "mysql/psi/mysql_rwlock.h"
#include "mysql/strings/dtoa.h"
#include "mysql/strings/int2str.h"
#include "mysql/strings/m_ctype.h"
#include "mysqld_error.h"
#include "sql/auth/auth_acls.h"
#include "sql/auth/auth_common.h"  // SUPER_ACL, generate_password
#include "sql/auth/sql_security_ctx.h"
#include "sql/debug_sync.h"
#include "sql/derror.h"  // ER_THD
#include "sql/enum_query_type.h"
#include "sql/item.h"
#include "sql/item_func.h"
#include "sql/log.h"
#include "sql/mysqld.h"  // system_charset_info
#include "sql/persisted_variable.h"
#include "sql/protocol_classic.h"
#include "sql/session_tracker.h"
#include "sql/sql_audit.h"  // mysql_audit
#include "sql/sql_base.h"   // lock_tables
#include "sql/sql_class.h"  // THD
#include "sql/sql_error.h"
#include "sql/sql_lex.h"
#include "sql/sql_list.h"
#include "sql/sql_parse.h"  // is_supported_parser_charset
#include "sql/sql_plugin_ref.h"
#include "sql/sql_plugin_var.h"
#include "sql/sql_select.h"        // free_underlaid_joins
#include "sql/sql_show.h"          // append_identifier
#include "sql/sys_vars_shared.h"   // PolyLock_mutex
#include "sql/system_variables.h"  // system_variables
#include "sql/table.h"             // table
#include "sql/thd_raii.h"          // Prepared_stmt_arena_holder
#include "sql_string.h"
#include "string_with_len.h"
#include "strxnmov.h"

using std::min;
using std::string;

static collation_unordered_map<string, sys_var *> *static_system_variable_hash;

static collation_unordered_map<string, sys_var *> *dynamic_system_variable_hash;

static PolyLock_mutex PLock_global_system_variables(
    &LOCK_global_system_variables);

ulonglong dynamic_system_variable_hash_version = 0;

collation_unordered_map<string, sys_var *> *get_static_system_variable_hash() {
  return static_system_variable_hash;
}

collation_unordered_map<string, sys_var *> *get_dynamic_system_variable_hash() {
  return dynamic_system_variable_hash;
}

/** list of variables that shouldn't be persisted in all cases */
static collation_unordered_set<string> *never_persistable_vars;

/**
  Get source of a given system variable given its name and name length.

  @note Holds an intermediate lock on LOCK_system_variables_hash.
*/
bool get_sysvar_source(const char *name, uint length,
                       enum enum_variable_source *source) {
  DBUG_TRACE;

  /* System variable hashes should have been initialized. */
  assert(get_static_system_variable_hash() != nullptr);
  assert(get_dynamic_system_variable_hash() != nullptr);

  const std::string str{name, length};
  sys_var *sysvar = find_or_nullptr(*get_static_system_variable_hash(), str);
  if (sysvar != nullptr) {
    *source = sysvar->get_source();
    return false;
  }

  bool ret = false;

  mysql_mutex_assert_not_owner(&LOCK_plugin);
  mysql_rwlock_rdlock(&LOCK_system_variables_hash);

  sysvar = find_or_nullptr(*get_dynamic_system_variable_hash(), str);
  if (sysvar == nullptr) {
    ret = true;
  } else {
    *source = sysvar->get_source();
  }

  mysql_rwlock_unlock(&LOCK_system_variables_hash);
  return ret;
}

sys_var_chain all_sys_vars = {nullptr, nullptr};

int sys_var_init() {
  DBUG_TRACE;

  /* Must be already initialized. */
  assert(system_charset_info != nullptr);

  static_system_variable_hash = new collation_unordered_map<string, sys_var *>(
      system_charset_info, PSI_INSTRUMENT_ME);

  dynamic_system_variable_hash = new collation_unordered_map<string, sys_var *>(
      system_charset_info, PSI_INSTRUMENT_ME);

  never_persistable_vars = new collation_unordered_set<string>(
      {PERSIST_ONLY_ADMIN_X509_SUBJECT, PERSISTED_GLOBALS_LOAD},
      system_charset_info, PSI_INSTRUMENT_ME);

  if (add_static_system_variable_chain(all_sys_vars.first)) goto error;

  return 0;

error:
  LogErr(ERROR_LEVEL, ER_FAILED_TO_INIT_SYS_VAR);
  return 1;
}

int sys_var_add_options(std::vector<my_option> *long_options, int parse_flags) {
  DBUG_TRACE;

  for (sys_var *var = all_sys_vars.first; var; var = var->next) {
    if (var->register_option(long_options, parse_flags)) goto error;
  }

  return 0;

error:
  LogErr(ERROR_LEVEL, ER_FAILED_TO_INIT_SYS_VAR);
  return 1;
}

void sys_var_end() {
  DBUG_TRACE;

  delete dynamic_system_variable_hash;
  dynamic_system_variable_hash = nullptr;

  delete static_system_variable_hash;
  static_system_variable_hash = nullptr;

  delete never_persistable_vars;

  for (sys_var *var = all_sys_vars.first; var; var = var->next) var->cleanup();
}

/**
  This function will check for necessary privileges needed to perform RESET
  PERSIST or SET PERSIST[_ONLY] operation.

  @param [in] thd                     Pointer to connection handle.
  @param [in] static_variable         describes if variable is static or dynamic

  @return 0 Success
  @return 1 Failure
*/
bool check_priv(THD *thd, bool static_variable) {
  Security_context *sctx = thd->security_context();
  /* for dynamic variables user needs SUPER_ACL or SYSTEM_VARIABLES_ADMIN */
  if (!static_variable) {
    if (!sctx->check_access(SUPER_ACL) &&
        !(sctx->has_global_grant(STRING_WITH_LEN("SYSTEM_VARIABLES_ADMIN"))
              .first)) {
      my_error(ER_SPECIFIC_ACCESS_DENIED_ERROR, MYF(0),
               "SUPER or SYSTEM_VARIABLES_ADMIN");
      return true;
    }
  } else {
    /*
     for static variables user needs both SYSTEM_VARIABLES_ADMIN and
     PERSIST_RO_VARIABLES_ADMIN
    */
    if (!(sctx->has_global_grant(STRING_WITH_LEN("SYSTEM_VARIABLES_ADMIN"))
              .first &&
          sctx->has_global_grant(STRING_WITH_LEN("PERSIST_RO_VARIABLES_ADMIN"))
              .first)) {
      my_error(ER_PERSIST_ONLY_ACCESS_DENIED_ERROR, MYF(0),
               "SYSTEM_VARIABLES_ADMIN and PERSIST_RO_VARIABLES_ADMIN");
      return true;
    }
  }
  return false;
}

/**
  sys_var constructor

  @param chain     variables are linked into chain for
                   add_static_system_variable_chain() or
                   add_dynamic_system_variable_chain()
  @param name_arg  the name of the variable. Must be 0-terminated and exist
                   for the lifetime of the sys_var object. @sa my_option::name
  @param comment   shown in mysqld --help, @sa my_option::comment
  @param flags_arg or'ed flag_enum values
  @param off       offset of the global variable value from the
                   &global_system_variables.
  @param getopt_id -1 for no command-line option, otherwise @sa my_option::id
  @param getopt_arg_type no|optional|required value @sa my_option::arg_type
  @param show_val_type_arg what value_ptr() returns for sql_show.cc
  @param def_val   default value, @sa my_option::def_value
  @param lock      mutex or rw_lock that protects the global variable
                   *in addition* to LOCK_global_system_variables.
  @param binlog_status_arg if the sysvar will be written to binlog or not @sa
  binlog_status_enum
  @param on_check_func a function to be called at the end of sys_var::check,
                   put your additional checks here
  @param on_update_func a function to be called at the end of sys_var::update,
                   any post-update activity should happen here
  @param substitute If non-NULL, this variable is deprecated and the
  string describes what one should use instead. If an empty string,
  the variable is deprecated but no replacement is offered.
  @param parse_flag either PARSE_EARLY or PARSE_NORMAL
  @param persisted_alias If this variable is persisted, it will
                   appear in the file both under its own name, and using
                   'persisted_alias'.
  @param is_persisted_deprecated If this variable is found in the
                   persisted, variables file, and its alias is not
                   found, a deprecation warning will be issued if
                   is_persisted_deprecated is true.  This flag must be
                   false if persisted_alias is null.
*/
sys_var::sys_var(sys_var_chain *chain, const char *name_arg,
                 const char *comment, int flags_arg, ptrdiff_t off,
                 int getopt_id, enum get_opt_arg_type getopt_arg_type,
                 SHOW_TYPE show_val_type_arg, longlong def_val, PolyLock *lock,
                 enum binlog_status_enum binlog_status_arg,
                 on_check_function on_check_func,
                 on_update_function on_update_func, const char *substitute,
                 int parse_flag, sys_var *persisted_alias,
                 bool is_persisted_deprecated)
    : next(nullptr),
      m_persisted_alias(persisted_alias),
      m_is_persisted_deprecated(is_persisted_deprecated),
      binlog_status(binlog_status_arg),
      flags(flags_arg),
      m_parse_flag(parse_flag),
      show_val_type(show_val_type_arg),
      guard(lock),
      offset(off),
      on_check(on_check_func),
      pre_update(nullptr),
      on_update(on_update_func),
      deprecation_substitute(substitute),
      is_os_charset(false) {
  /*
    There is a limitation in handle_options() related to short options:
    - either all short options should be declared when parsing in multiple
    stages,
    - or none should be declared.
    Because a lot of short options are used in the normal parsing phase
    for mysqld, we enforce here that no short option is present
    in the first (PARSE_EARLY) stage.
    See handle_options() for details.
  */
  assert(parse_flag == PARSE_NORMAL || getopt_id <= 0 || getopt_id >= 255);

  // the is_persist_deprecated flag is only applicable for aliases
  if (!persisted_alias) assert(!is_persisted_deprecated);

  name.str = name_arg;  // ER_NO_DEFAULT relies on 0-termination of name_arg
  name.length = strlen(name_arg);  // and so does this.
  assert(name.length <= NAME_CHAR_LEN);

  memset(&option, 0, sizeof(option));
  option.name = name_arg;
  option.id = getopt_id;
  option.comment = comment;
  option.arg_type = getopt_arg_type;
  option.value = (uchar **)global_var_ptr();
  option.def_value = def_val;

  /* set default values */
  source.m_source = enum_variable_source::COMPILED;

  timestamp = 0;
  user[0] = '\0';
  host[0] = '\0';

  memset(source.m_path_name, 0, FN_REFLEN);
  option.arg_source = &source;

  if (persisted_alias) persisted_alias->m_persisted_alias = this;

  if (chain->last)
    chain->last->next = this;
  else
    chain->first = this;
  chain->last = this;
}

bool sys_var::update(THD *thd, set_var *var) {
  /*
    Invoke preparatory step for updating a system variable. Doing this action
    before we have acquired any locks allows to invoke code which acquires other
    locks without introducing deadlocks.
  */
  if (pre_update && pre_update(this, thd, var)) return true;

  const enum_var_type type = var->type;
  if (type == OPT_GLOBAL || type == OPT_PERSIST || scope() == GLOBAL) {
    /*
      Yes, both locks need to be taken before an update, just as
      both are taken to get a value. If we'll take only 'guard' here,
      then value_ptr() for strings won't be safe in SHOW VARIABLES anymore,
      to make it safe we'll need value_ptr_unlock().
    */
    const AutoWLock lock1(&PLock_global_system_variables);
    const AutoWLock lock2(guard);
    return global_update(thd, var) ||
           (on_update && on_update(this, thd, OPT_GLOBAL));
  } else {
    /* Block reads from other threads. */
    mysql_mutex_lock(&thd->LOCK_thd_sysvar);

    const bool ret = session_update(thd, var) ||
                     (on_update && on_update(this, thd, OPT_SESSION));

    mysql_mutex_unlock(&thd->LOCK_thd_sysvar);

    /*
      Make sure we don't session-track variables that are not actually
      part of the session. tx_isolation and and tx_read_only for example
      exist as GLOBAL, SESSION, and one-shot ("for next transaction only").
    */
    if ((var->type == OPT_SESSION) || !is_trilevel()) {
      if ((!ret) && thd->session_tracker.get_tracker(SESSION_SYSVARS_TRACKER)
                        ->is_enabled())
        thd->session_tracker.get_tracker(SESSION_SYSVARS_TRACKER)
            ->mark_as_changed(thd, name);

      if ((!ret) &&
          thd->session_tracker.get_tracker(SESSION_STATE_CHANGE_TRACKER)
              ->is_enabled())
        thd->session_tracker.get_tracker(SESSION_STATE_CHANGE_TRACKER)
            ->mark_as_changed(thd, name);
    }

    return ret;
  }
}

const uchar *sys_var::session_value_ptr(THD *, THD *target_thd,
                                        std::string_view) {
  return session_var_ptr(target_thd);
}

const uchar *sys_var::global_value_ptr(THD *, std::string_view) {
  return global_var_ptr();
}

uchar *sys_var::session_var_ptr(THD *thd) {
  return ((uchar *)&(thd->variables)) + offset;
}

uchar *sys_var::global_var_ptr() {
  return ((uchar *)&global_system_variables) + offset;
}

bool sys_var::check(THD *thd, set_var *var) {
  if ((var->value && do_check(thd, var)) ||
      (on_check && on_check(this, thd, var))) {
    if (!thd->is_error()) {
      char buff[STRING_BUFFER_USUAL_SIZE];
      String str(buff, sizeof(buff), system_charset_info), *res;

      if (!var->value) {
        str.set(STRING_WITH_LEN("DEFAULT"), &my_charset_latin1);
        res = &str;
      } else if (!(res = var->value->val_str(&str))) {
        str.set(STRING_WITH_LEN("NULL"), &my_charset_latin1);
        res = &str;
      }
      const ErrConvString err(res);
      my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), name.str, err.ptr());
    }
    return true;
  }
  return false;
}

const uchar *sys_var::value_ptr(THD *running_thd, THD *target_thd,
                                enum_var_type type,
                                std::string_view keycache_name) {
  if (type == OPT_GLOBAL || type == OPT_PERSIST || scope() == GLOBAL) {
    mysql_mutex_assert_owner(&LOCK_global_system_variables);
    const AutoRLock lock(guard);
    return global_value_ptr(running_thd, keycache_name);
  } else
    return session_value_ptr(running_thd, target_thd, keycache_name);
}

const uchar *sys_var::value_ptr(THD *thd, enum_var_type type,
                                std::string_view keycache_name) {
  return value_ptr(thd, thd, type, keycache_name);
}

bool sys_var::set_default(THD *thd, set_var *var) {
  DBUG_TRACE;
  if (var->is_global_persist() || scope() == GLOBAL)
    global_save_default(thd, var);
  else
    session_save_default(thd, var);

  const bool ret = check(thd, var) || update(thd, var);
  return ret;
}

void sys_var::set_user_host(THD *thd) {
  memset(user, 0, sizeof(user));
  memset(host, 0, sizeof(host));
  Security_context *sctx = thd->security_context();
  bool truncated = false;
  if (sctx->user().length > 0) {
    truncated = set_and_truncate(user, thd->security_context()->user().str,
                                 sizeof(user));
    if (truncated) {
      LogErr(WARNING_LEVEL, ER_USERNAME_TRUNKATED, sctx->user().str,
             USERNAME_CHAR_LENGTH);
    }
  }
  if (sctx->host().length > 0) {
    truncated = set_and_truncate(host, thd->security_context()->host().str,
                                 sizeof(host));
    if (truncated) {
      LogErr(WARNING_LEVEL, ER_HOSTNAME_TRUNKATED, sctx->host().str,
             HOSTNAME_LENGTH);
    }
  }
}

void sys_var::do_deprecated_warning(THD *thd) {
  if (deprecation_substitute != nullptr) {
    char buf1[NAME_CHAR_LEN + 3];
    strxnmov(buf1, sizeof(buf1) - 1, "@@", name.str, 0);

    /*
       if deprecation_substitute is an empty string,
       there is no replacement for the syntax
    */
    const uint errmsg = deprecation_substitute[0] == '\0'
                            ? ER_DEPRECATE_MSG_NO_REPLACEMENT
                            : ER_DEPRECATE_MSG_WITH_REPLACEMENT;
    if (thd)
      push_warning_printf(
          thd, Sql_condition::SL_WARNING, ER_WARN_DEPRECATED_SYNTAX,
          ER_THD_NONCONST(thd, errmsg), buf1, deprecation_substitute);
    else
      LogErr(WARNING_LEVEL, errmsg, buf1, deprecation_substitute);
  }
}

Item *sys_var::copy_value(THD *thd) {
  const uchar *val_ptr = session_value_ptr(thd, thd, {});
  switch (get_var_type()) {
    case GET_INT:
      return new Item_int(*pointer_cast<const int *>(val_ptr));
    case GET_UINT:
      return new Item_int(
          static_cast<ulonglong>(*pointer_cast<const uint *>(val_ptr)));
    case GET_LONG:
      return new Item_int(
          static_cast<longlong>(*pointer_cast<const long *>(val_ptr)));
    case GET_ULONG:
      return new Item_int(
          static_cast<ulonglong>(*pointer_cast<const ulong *>(val_ptr)));
    case GET_LL:
      return new Item_int(*pointer_cast<const longlong *>(val_ptr));
    case GET_ULL:
      return new Item_int(*pointer_cast<const ulonglong *>(val_ptr));
    case GET_BOOL:
      return new Item_int(*pointer_cast<const bool *>(val_ptr));
    case GET_ENUM:
    case GET_SET:
    case GET_FLAGSET:
    case GET_STR_ALLOC:
    case GET_STR:
    case GET_NO_ARG:
    case GET_PASSWORD: {
      const char *tmp_str_val = pointer_cast<const char *>(val_ptr);
      return new Item_string(tmp_str_val, strlen(tmp_str_val),
                             system_charset_info);
    }
    case GET_DOUBLE:
      return new Item_float(*pointer_cast<const double *>(val_ptr),
                            DECIMAL_NOT_SPECIFIED);
    default:
      assert(0);
  }
  return nullptr;
}

/**
  Throw warning (error in STRICT mode) if value for variable needed bounding.
  Plug-in interface also uses this.

  @param thd         thread handle
  @param name        variable's name
  @param fixed       did we have to correct the value? (throw warn/err if so)
  @param is_unsigned is value's type unsigned?
  @param v           variable's value

  @retval         true on error, false otherwise (warning or ok)
 */
bool throw_bounds_warning(THD *thd, const char *name, bool fixed,
                          bool is_unsigned, longlong v) {
  if (fixed) {
    char buf[22];

    if (is_unsigned)
      ullstr((ulonglong)v, buf);
    else
      llstr(v, buf);

    if (thd->variables.sql_mode & MODE_STRICT_ALL_TABLES) {
      my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), name, buf);
      return true;
    }
    push_warning_printf(thd, Sql_condition::SL_WARNING,
                        ER_TRUNCATED_WRONG_VALUE,
                        ER_THD(thd, ER_TRUNCATED_WRONG_VALUE), name, buf);
  }
  return false;
}

bool throw_bounds_warning(THD *thd, const char *name, bool fixed, double v) {
  if (fixed) {
    char buf[64];

    my_gcvt(v, MY_GCVT_ARG_DOUBLE, static_cast<int>(sizeof(buf)) - 1, buf,
            nullptr);

    if (thd->variables.sql_mode & MODE_STRICT_ALL_TABLES) {
      my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), name, buf);
      return true;
    }
    push_warning_printf(thd, Sql_condition::SL_WARNING,
                        ER_TRUNCATED_WRONG_VALUE,
                        ER_THD(thd, ER_TRUNCATED_WRONG_VALUE), name, buf);
  }
  return false;
}

const CHARSET_INFO *sys_var::charset(THD *thd) {
  return is_os_charset ? thd->variables.character_set_filesystem
                       : system_charset_info;
}

static void set_tail_to_triple_dot(char *s, size_t size) {
  assert(size > 3);
  memcpy(s + size - 3, "...", 3);
}

template <size_t N>
static void copy_name(char (&to)[N], std::string_view from) {
  static_assert(N > sizeof("..."));
  size_t adjusted_size = std::min(N - 1, from.size());
  strncpy(to, from.data(), adjusted_size);
  to[adjusted_size] = '\0';
  if (adjusted_size < from.size()) {
    set_tail_to_triple_dot(to, adjusted_size);
  }
}

static void merge_names(char *to, size_t capacity, std::string_view from1,
                        std::string_view from2) {
  assert(capacity > sizeof("..."));
  size_t adjusted_size1 = std::min(capacity - 1, from1.size());
  strncpy(to, from1.data(), adjusted_size1);
  size_t rest = capacity - 1 - adjusted_size1;
  if (adjusted_size1 < from1.size() || rest < 3) {
    to[adjusted_size1] = '\0';
    set_tail_to_triple_dot(to, adjusted_size1);
    return;  // error: truncated name
  }
  to[adjusted_size1] = '.';
  rest--;
  size_t adjusted_size2 = std::min(rest, from2.size());
  strncpy(to + adjusted_size1 + 1, from2.data(), adjusted_size2);
  to[adjusted_size1 + 1 + adjusted_size2] = '\0';
  if (adjusted_size2 < from2.size()) {
    set_tail_to_triple_dot(to, adjusted_size1 + 1 + adjusted_size2);
  }
}

thread_local int System_variable_tracker::m_hash_lock_recursion_depth{0};

System_variable_tracker::System_variable_tracker(Static, sys_var *var)
    : m_tag{STATIC}, m_static{var} {
  assert(var != nullptr);
  assert(var->cast_pluginvar() == nullptr);
  assert(!var->is_struct());
}

System_variable_tracker::System_variable_tracker(Keycache,
                                                 std::string_view cache_name,
                                                 sys_var *var)
    : m_tag{KEYCACHE} {
  assert(var != nullptr);
  assert(var->cast_pluginvar() == nullptr);
  assert(var->is_struct());

  m_keycache.m_keycache_var = var;
  const std::string_view keycache_property_name{to_string_view(var->name)};
  if (cache_name.empty()) {
    m_keycache.m_keycache_name_size = 0;
    copy_name(m_keycache.m_keycache_var_name, keycache_property_name);
  } else {
    merge_names(m_keycache.m_keycache_var_name,
                sizeof(m_keycache.m_keycache_var_name), cache_name,
                keycache_property_name);
    if (cache_name.size() <= NAME_LEN) {
      m_keycache.m_keycache_name_size = cache_name.size();
    } else {
      char *first_dot = strchr(m_keycache.m_keycache_var_name, '.');
      m_keycache.m_keycache_name_size =
          first_dot - m_keycache.m_keycache_var_name;
    }
  }
}

System_variable_tracker::System_variable_tracker(Plugin, std::string_view name)
    : m_tag{PLUGIN} {
  copy_name(m_plugin.m_plugin_var_name, name);
  m_plugin.m_plugin_var_cache = nullptr;
}

System_variable_tracker::System_variable_tracker(
    Component, std::string_view dot_separated_name)
    : m_tag{COMPONENT} {
  /*
    We are getting the component name as prefix and variable name
    as suffix, and we are adding the "." as a separator to find
    the variable from dynamic_system_variable_hash.
    We are doing this, because we use the structured variable syntax for
    component variables.
  */
  copy_name(m_component.m_component_var_name, dot_separated_name);
  m_component.m_component_var_cache = nullptr;
}

System_variable_tracker::System_variable_tracker(
    Component, std::string_view component_name, std::string_view variable_name)
    : m_tag{COMPONENT} {
  /*
    We are getting the component name as prefix and variable name
    as suffix, and we are adding the "." as a separator to find
    the variable from dynamic_system_variable_hash.
    We are doing this, because we use the structured variable syntax for
    component variables.
  */
  merge_names(m_component.m_component_var_name,
              sizeof(m_component.m_component_var_name), component_name,
              variable_name);
  m_component.m_component_var_cache = nullptr;
}

System_variable_tracker::System_variable_tracker(
    const System_variable_tracker &x)
    : m_tag{x.m_tag}, m_cache{x.m_cache} {
  switch (x.m_tag) {
    case STATIC:
      new (&m_static) decltype(m_static){x.m_static};
      break;
    case KEYCACHE:
      new (&m_keycache) decltype(m_keycache){x.m_keycache};
      break;
    case PLUGIN:
      new (&m_plugin) decltype(m_plugin){x.m_plugin};
      break;
    case COMPONENT:
      new (&m_component) decltype(m_component){x.m_component};
      break;
  }
  assert(*this == x);
}

System_variable_tracker::~System_variable_tracker() {
  switch (m_tag) {
    case STATIC:
      std::destroy_at(&m_static);
      return;
    case KEYCACHE:
      std::destroy_at(&m_keycache);
      return;
    case PLUGIN:
      std::destroy_at(&m_plugin);
      return;
    case COMPONENT:
      std::destroy_at(&m_component);
      return;
  }
}

void System_variable_tracker::operator=(System_variable_tracker &&x) {
  if (&x != this) {
    this->~System_variable_tracker();
    new (this) System_variable_tracker{std::move(x)};
  }
}

const char *System_variable_tracker::get_var_name() const {
  switch (m_tag) {
    case STATIC:
      return m_static.m_static_var->name.str;
    case KEYCACHE:
      return m_keycache.m_keycache_var_name;
    case PLUGIN:
      return m_plugin.m_plugin_var_name;
    case COMPONENT:
      return m_component.m_component_var_name;
  }
  my_abort();  // to make compiler happy
}

bool System_variable_tracker::access_system_variable(
    THD *thd, std::function<void(const System_variable_tracker &, sys_var *)> f,
    Suppress_not_found_error suppress_not_found_error,
    Force_sensitive_system_variable_access force_sensitive_variable_access,
    Is_already_locked is_already_locked,
    Is_single_thread is_single_thread) const {
  switch (m_tag) {
    case STATIC:
      cache_metadata(thd, m_static.m_static_var);
      if (force_sensitive_variable_access !=
              Force_sensitive_system_variable_access::YES &&
          m_static.m_static_var->check_if_sensitive_in_context(
              thd, suppress_not_found_error == Suppress_not_found_error::YES))
        return true;
      if (f) {
        f(*this, m_static.m_static_var);
      }
      return false;
    case KEYCACHE:
      cache_metadata(thd, m_keycache.m_keycache_var);
      if (force_sensitive_variable_access !=
              Force_sensitive_system_variable_access::YES &&
          m_keycache.m_keycache_var->check_if_sensitive_in_context(
              thd, suppress_not_found_error == Suppress_not_found_error::YES))
        return true;
      if (f) {
        f(*this, m_keycache.m_keycache_var);
      }
      return false;
    case PLUGIN: {
      if (m_plugin.m_plugin_var_cache != nullptr) {
        if (f) {
          f(*this, m_plugin.m_plugin_var_cache);
        }
        return false;
      }
      auto wrapper = [this, thd, suppress_not_found_error,
                      force_sensitive_variable_access,
                      f](sys_var *var) -> bool {
        cache_metadata(thd, var);
        if (force_sensitive_variable_access !=
                Force_sensitive_system_variable_access::YES &&
            var->check_if_sensitive_in_context(
                thd, suppress_not_found_error == Suppress_not_found_error::YES))
          return true;
        m_plugin.m_plugin_var_cache = var;
        if (f) {
          f(*this, var);
        }
        m_plugin.m_plugin_var_cache = nullptr;
        return false;
      };
      return visit_plugin_variable(thd, wrapper, suppress_not_found_error,
                                   is_already_locked, is_single_thread);
    }
    case COMPONENT:
      if (m_component.m_component_var_cache != nullptr) {
        if (f) {
          f(*this, m_component.m_component_var_cache);
        }
        return false;
      }
      auto wrapper = [this, thd, suppress_not_found_error,
                      force_sensitive_variable_access,
                      f](sys_var *var) -> bool {
        cache_metadata(thd, var);
        if (force_sensitive_variable_access !=
                Force_sensitive_system_variable_access::YES &&
            var->check_if_sensitive_in_context(
                thd, suppress_not_found_error == Suppress_not_found_error::YES))
          return true;
        m_component.m_component_var_cache = var;
        if (f) {
          f(*this, var);
        }
        m_component.m_component_var_cache = nullptr;
        return false;
      };
      return visit_component_variable(thd, wrapper, suppress_not_found_error,
                                      is_already_locked, is_single_thread);
  }
  my_abort();  // to make compiler happy
}

bool System_variable_tracker::names_are_same(const char *a, const char *b) {
  return my_strcasecmp(system_charset_info, a, b) == 0;
}

System_variable_tracker System_variable_tracker::make_tracker(
    std::string_view prefix, std::string_view suffix) {
  /*
     1. 1D (unqualified) variable names:
  */
  if (prefix.empty()) {
    sys_var *var = find_static_system_variable(std::string{suffix});
    if (var != nullptr) {      // static/keycache variable has found
      if (var->is_struct()) {  // is it a keycache default?
        assert(is_key_cache_variable_suffix(var->name.str));
        return System_variable_tracker{Keycache{}, {}, var};
      } else {  // this is a regular static variable not related to key caches
        return System_variable_tracker{Static{}, var};
      }
    } else {  // probable plugin-registered variable (not resolved yet)
      return System_variable_tracker{Plugin{}, suffix};
    }
  }

  /*
    2. 2D (qualified) system variable names:
  */

  /*
    2.1. Process Multiple Key Cache variables:

             * <cache name>.key_buffer_size
             * <cache name>.key_cache_block_size
             * <cache name>.key_cache_division_limit
             * <cache name>.key_cache_age_threshold

         where <cache name> might include DEFAULT:
  */
  if (is_key_cache_variable_suffix(suffix)) {
    sys_var *var = find_static_system_variable(std::string{suffix});
    assert(var != nullptr);
    assert(!var->cast_pluginvar());
    assert(var->is_struct());
    return System_variable_tracker{Keycache{}, prefix, var};
  }

  /*
    2.2. Process a component-provided variable name:
  */

  return System_variable_tracker{Component{}, prefix, suffix};
}

System_variable_tracker System_variable_tracker::make_tracker(
    std::string_view multipart_name) {
  size_t dot_position = multipart_name.find('.');
  if (dot_position == multipart_name.npos) {  // the name is not dot-separated
    return make_tracker({}, multipart_name);
  }
  const std::string_view prefix{multipart_name.data(), dot_position};
  const std::string_view suffix{multipart_name.data() + dot_position + 1,
                                multipart_name.size() - dot_position - 1};
  return make_tracker(prefix, suffix);
}

struct my_old_conv {
  const char *old_name;
  const char *new_name;
};

static my_old_conv old_conv[] = {{"cp1251_koi8", "cp1251"},
                                 {"cp1250_latin2", "cp1250"},
                                 {"kam_latin2", "keybcs2"},
                                 {"mac_latin2", "MacRoman"},
                                 {"macce_latin2", "MacCE"},
                                 {"pc2_latin2", "pclatin2"},
                                 {"vga_latin2", "pclatin1"},
                                 {"koi8_cp1251", "koi8r"},
                                 {"win1251ukr_koi8_ukr", "win1251ukr"},
                                 {"koi8_ukr_win1251ukr", "koi8u"},
                                 {nullptr, nullptr}};

const CHARSET_INFO *get_old_charset_by_name(const char *name) {
  my_old_conv *conv;

  for (conv = old_conv; conv->old_name; conv++) {
    if (!my_strcasecmp(&my_charset_latin1, name, conv->old_name))
      return get_charset_by_csname(conv->new_name, MY_CS_PRIMARY, MYF(0));
  }
  return nullptr;
}

/****************************************************************************
  Main handling of variables:
  - Initialisation
  - Searching during parsing
  - Update loop
****************************************************************************/

/**
  Add variables to the dynamic hash of system variables

  @param first       Pointer to first system variable to add

  @returns false on success, otherwise true.

  @note Requires r/w lock on LOCK_system_variables_hash.
*/
bool add_dynamic_system_variable_chain(sys_var *first) {
  for (sys_var *var = first; var != nullptr; var = var->next) {
    /* this fails if there is a conflicting variable name. */
    std::string name = to_string(var->name);
    if (static_system_variable_hash->find(name) !=
            static_system_variable_hash->end() ||
        !dynamic_system_variable_hash->emplace(name, var).second) {
      LogErr(ERROR_LEVEL, ER_DUPLICATE_SYS_VAR, name.c_str());
      for (; first != var; first = first->next)
        dynamic_system_variable_hash->erase(to_string(first->name));
      return true;
    }
  }

  /* Update system_variable_hash version. */
  dynamic_system_variable_hash_version++;
  return false;
}

/**
  Add variables to the hash of static system variables

  @param first       Pointer to first system variable to add

  @returns false on success, otherwise true.
*/
bool add_static_system_variable_chain(sys_var *first) {
  assert(static_system_variable_hash->empty());

  for (sys_var *var = first; var != nullptr; var = var->next) {
    std::string name = to_string(var->name);
    /* this fails if there is a conflicting variable name. */
    if (!static_system_variable_hash->emplace(name, var).second) {
      LogErr(ERROR_LEVEL, ER_DUPLICATE_SYS_VAR, var->name.str);
      for (; first != var; first = first->next)
        dynamic_system_variable_hash->erase(to_string(first->name));
      return true;
    }
  }

  return false;
}

/**
  Remove variables from the dynamic hash of system variables

  @param first  Pointer to first system variable to remove

  @note Requires r/w lock on LOCK_system_variables_hash.
*/

void delete_dynamic_system_variable_chain(sys_var *first) {
  for (sys_var *var = first; var; var = var->next)
    dynamic_system_variable_hash->erase(to_string(var->name));

  /* Update system_variable_hash version. */
  dynamic_system_variable_hash_version++;
}

/*
  Number of records in the system_variable_hash.
  Requires r/o lock on LOCK_system_variables_hash.
*/
ulong get_system_variable_count(void) {
  return static_system_variable_hash->size() +
         dynamic_system_variable_hash->size();
}

/*
  Current version of the system_variable_hash.
  Requires r/o lock on LOCK_system_variables_hash.
*/
ulonglong get_dynamic_system_variable_hash_version(void) {
  return dynamic_system_variable_hash_version;
}

bool System_variable_tracker::enumerate_sys_vars_in_hash(
    collation_unordered_map<string, sys_var *> *hash,
    enum enum_var_type query_scope, bool strict,
    System_variable_tracker::Array *output) {
  bool privileged_user = false;
  THD *thd = current_thd;

  if (thd != nullptr) {
    privileged_user =
        thd->security_context()
            ->has_global_grant(STRING_WITH_LEN("SENSITIVE_VARIABLES_OBSERVER"))
            .first;
  }

  for (const auto &key_and_value : *hash) {
    sys_var *sysvar = key_and_value.second;
    /*
      Don't show sensitive variables.
      ToDo: Figure out a way to make it visible to privileged users
    */
    if (sysvar->is_sensitive() && !privileged_user) continue;

    if (strict) {
      /*
        Strict scope match. Success if this is a:
          - global query and the variable scope is GLOBAL or SESSION, OR
          - session query and the variable scope is SESSION or ONLY_SESSION.
      */
      if (!sysvar->check_scope(query_scope)) {
        continue;
      }
    } else {
      /*
        Non-strict scope match (5.6). Success if this is a:
          - global query and the variable scope is GLOBAL or SESSION, OR
          - session query and the variable scope is GLOBAL, SESSION or
        ONLY_SESSION.
      */
      if (query_scope == OPT_GLOBAL && !sysvar->check_scope(query_scope))
        continue;
    }

    /* Don't show non-visible variables. */
    if (sysvar->not_visible()) {
      continue;
    }

    if (hash == static_system_variable_hash) {
      if (sysvar->is_struct()) {
        assert(is_key_cache_variable_suffix(sysvar->name.str));
        if (output->emplace_back(Keycache{}, std::string_view{}, sysvar))
          return true;  // OOM
      } else {
        if (output->emplace_back(Static{}, sysvar)) return true;  // OOM
      }
    } else {
      assert(hash == dynamic_system_variable_hash);
      const char *dot = static_cast<const char *>(
          memchr(sysvar->name.str, '.', sysvar->name.length));
      if (dot == nullptr) {
        if (output->emplace_back(Plugin{}, to_string_view(sysvar->name)))
          return true;  // OOM
      } else {
        if (output->emplace_back(Component{}, to_string_view(sysvar->name)))
          return true;  // OOM
      }
    }
  }
  return false;
}

/**
  Constructs an array of system variables for display to the user.

  @param output         Prealloced_array of elements for display
  @param sort           If true, the system variables should be sorted
  @param query_scope    OPT_GLOBAL or OPT_SESSION for SHOW GLOBAL|SESSION
                        VARIABLES
  @param strict         Use strict scope checking
  @returns              True on error, false otherwise

  @note Requires r/o lock on LOCK_system_variables_hash.
*/
bool System_variable_tracker::enumerate_sys_vars(
    bool sort, enum enum_var_type query_scope, bool strict,
    System_variable_tracker::Array *output) {
  assert(output != nullptr);
  assert(query_scope == OPT_SESSION || query_scope == OPT_GLOBAL);
  const int count = get_system_variable_count();

  /* Resize array if necessary. */
  if (output->reserve(count + 1)) return true;

  if (enumerate_sys_vars_in_hash(static_system_variable_hash, query_scope,
                                 strict, output) ||
      enumerate_sys_vars_in_hash(dynamic_system_variable_hash, query_scope,
                                 strict, output))
    return true;  // OOM

  if (sort)
    std::sort(
        output->begin(), output->end(),
        [](const System_variable_tracker &a, const System_variable_tracker &b) {
          return my_strcasecmp(system_charset_info, a.get_var_name(),
                               b.get_var_name()) < 0;
        });

  return false;
}

bool System_variable_tracker::visit_plugin_variable(
    THD *thd, std::function<bool(sys_var *)> function,
    Suppress_not_found_error suppress_not_found_error,
    Is_already_locked is_already_locked,
    Is_single_thread is_single_thread) const {
  assert(m_tag == PLUGIN);
  assert(function);

  if (thd != nullptr) {
    m_hash_lock_recursion_depth++;
  }
  const bool force_hash_lock = thd != nullptr &&
                               m_hash_lock_recursion_depth == 1 &&
                               is_already_locked == Is_already_locked::NO &&
                               is_single_thread == Is_single_thread::NO;

  if (force_hash_lock) {
    mysql_mutex_assert_not_owner(&LOCK_plugin);
    mysql_rwlock_rdlock(&LOCK_system_variables_hash);
  }
  sys_var *var =
      find_dynamic_system_variable(std::string{m_plugin.m_plugin_var_name});
  if (var != nullptr) {
    sys_var_pluginvar *pi = var->cast_pluginvar();
    assert(pi != nullptr && pi->is_plugin);
    /*
      pi->plugin is NULL if:

      A. we calling this function from the INSTALL PLUGIN statement executor

      or

      B. we are loading a bunch of plugins from the mysql.plugin table,
         and plugin objects aren't allocated yet but their dynamic system
         variables are registered in the dictionary

      or

      C. var is not a plugin-registered variable but a component-registered
      one (should not happen here)

      so, an internal locking by intern_plugin_lock() is not
      needed/impossible.

      Otherwise call intern_plugin_lock() and check the current state of the
      plugin for PLUGIN_IS_READY:
    */
    if (pi->plugin != nullptr) {
      LEX *lex = thd ? thd->lex : nullptr;

      const bool force_plugin_lock =
          thd != nullptr && is_already_locked == Is_already_locked::NO &&
          is_single_thread == Is_single_thread::NO;

      if (force_plugin_lock) {
        mysql_mutex_lock(&LOCK_plugin);
      }
      if (is_single_thread == Is_single_thread::NO) {
        extern plugin_ref intern_plugin_lock(LEX *, plugin_ref);
        extern void intern_plugin_unlock(LEX *, plugin_ref);

        plugin_ref plugin =
            intern_plugin_lock(lex, plugin_int_to_ref(pi->plugin));

        if (plugin == nullptr) {
          var = nullptr; /* failed to lock it, it must be uninstalling */
        } else if (!(plugin_state(plugin) & PLUGIN_IS_READY)) {
          /* initialization not completed */
          var = nullptr;
        }

        // Always unlock it, even if init fails.
        intern_plugin_unlock(lex, plugin);
      }
      if (force_plugin_lock) {
        // Safe to unlock: thd is NULL, or a reference counter holds the plugin
        // because of my_intern_plugin_lock()
        mysql_mutex_unlock(&LOCK_plugin);
      }
    }
  }

  if (force_hash_lock) {
    // Safe to unlock: we hold LOCK_plugin anyway, so sys_var* is stable:
    mysql_rwlock_unlock(&LOCK_system_variables_hash);
  }

  if (thd != nullptr) {
    m_hash_lock_recursion_depth--;
  }

  if (var == nullptr) {
    if (suppress_not_found_error == Suppress_not_found_error::NO)
      my_error(ER_UNKNOWN_SYSTEM_VARIABLE, MYF(0), get_var_name());
    return true;
  }

  // Safe to call a variable data/metadata access function here: a reference
  // counter holds the plugin because of my_intern_plugin_lock(), and
  // sys_var* is stable:
  return function(var);
}

bool System_variable_tracker::visit_component_variable(
    THD *thd, std::function<bool(sys_var *)> function,
    Suppress_not_found_error suppress_not_found_error,
    Is_already_locked is_already_locked,
    Is_single_thread is_single_thread) const {
  assert(m_tag == COMPONENT);
  assert(function);

  if (thd != nullptr) {
    m_hash_lock_recursion_depth++;
  }

  const bool force_hash_lock = thd != nullptr &&
                               m_hash_lock_recursion_depth == 1 &&
                               is_already_locked == Is_already_locked::NO &&
                               is_single_thread == Is_single_thread::NO;

  if (force_hash_lock) {
    mysql_mutex_assert_not_owner(&LOCK_plugin);
    mysql_rwlock_rdlock(&LOCK_system_variables_hash);
  }

  sys_var *var = find_dynamic_system_variable(m_component.m_component_var_name);
  assert(var == nullptr || (var->cast_pluginvar() != nullptr &&
                            !var->cast_pluginvar()->is_plugin));

  const bool result = var == nullptr ? true : function(var);

  if (force_hash_lock) {
    mysql_rwlock_unlock(&LOCK_system_variables_hash);
  }
  if (thd != nullptr) {
    m_hash_lock_recursion_depth--;
  }

  if (var == nullptr) {
    if (suppress_not_found_error == Suppress_not_found_error::NO)
      my_error(ER_UNKNOWN_SYSTEM_VARIABLE, MYF(0), get_var_name());
    return true;
  }
  return result;
}

void System_variable_tracker::cache_metadata(THD *thd, sys_var *v) const {
  if (m_cache.has_value()) {
    return;
  }
  m_cache = Cache{
      v->show_type(), v->is_sensitive(),
      v->is_persist_readonly() || v->is_readonly() || v->is_parse_early()};
  if (v->is_sensitive()) {
    thd->lex->set_rewrite_required();
  }
}

/**
  Find a static system variable.

  @param name   Name of system variable to find

  @retval
    pointer     pointer to variable definitions
  @retval
    nullptr     1. Unknown static variable (error message is given).
                2. Invisible static variable (no error message).

  @note  Unlike intern_find_sys_var() and find_dynamic_system_variable(),
         an external lock on LOCK_system_variable_hash is not necessary.
*/
sys_var *find_static_system_variable(const std::string &name) {
  sys_var *var = find_or_nullptr(*static_system_variable_hash, name);
  return var == nullptr || var->not_visible() ? nullptr : var;
}

/**
  Find a dynamic system variable.

  @param name   Name of system variable to find

  @retval
    pointer     pointer to variable definitions
  @retval
    nullptr     1. Unknown static variable (error message is given).
                2. Invisible static variable (no error message).

  @note  Requires an external lock on LOCK_system_variable_hash.
*/
sys_var *find_dynamic_system_variable(const std::string &name) {
  sys_var *var = find_or_nullptr(*dynamic_system_variable_hash, name);
  return var == nullptr || var->not_visible() ? nullptr : var;
}

/**
  Find a system variable, either static or dynamic.

  @param str       Name of system variable to find
  @param length    Length of variable.  zero means that we should use strlen()
                   on the variable

  @retval
    pointer     pointer to variable definitions
  @retval
    nullptr     1. Unknown variable (error message is given).
                2. Invisible variable (no error message).

  @note  Requires an external lock on LOCK_system_variable_hash.
*/

sys_var *intern_find_sys_var(const char *str, size_t length) {
  const std::string name{str, length ? length : strlen(str)};
  sys_var *var = find_static_system_variable(name);
  if (var != nullptr) {
    return var;
  }
  DBUG_EXECUTE_IF(
      "check_intern_find_sys_var_lock", if (current_thd) {
        const int err = mysql_rwlock_trywrlock(&LOCK_system_variables_hash);
        assert(err == EBUSY || err == EDEADLK);
      });
  return find_dynamic_system_variable(name);
}

bool sys_var::check_if_sensitive_in_context(THD *thd,
                                            bool suppress_errors) const {
  if (is_sensitive() && thd->security_context()
                                ->has_global_grant(STRING_WITH_LEN(
                                    "SENSITIVE_VARIABLES_OBSERVER"))
                                .first == false) {
    if (!suppress_errors) {
      my_error(ER_SPECIFIC_ACCESS_DENIED_ERROR, MYF(0),
               "SENSITIVE_VARIABLES_OBSERVER");
    }
    return true;
  }
  return false;
}

sys_var *check_find_sys_var(THD *thd, const char *str, size_t length,
                            bool *sensitive /* = nullptr */) {
  if (thd == nullptr) return nullptr;
  sys_var *var = intern_find_sys_var(str, length);
  if (var && var->is_sensitive()) {
    if (thd->security_context()
            ->has_global_grant(STRING_WITH_LEN("SENSITIVE_VARIABLES_OBSERVER"))
            .first == false) {
      if (sensitive != nullptr) *sensitive = true;
      return nullptr;
    }
  }
  return var;
}

/**
  Execute update of all variables.

  First run a check of all variables that all updates will go ok.
  If yes, then execute all updates, returning an error if any one failed.

  This should ensure that in all normal cases none all or variables are
  updated.

  @param thd            Thread id
  @param var_list       List of variables to update
  @param opened         True means tables are open and this function will lock
                        them.

  @retval
    0   ok
  @retval
    1   ERROR, message sent (normally no variables was updated)
  @retval
    -1  ERROR, message not sent
*/

int sql_set_variables(THD *thd, List<set_var_base> *var_list, bool opened) {
  int error;
  List_iterator_fast<set_var_base> it(*var_list);
  DBUG_TRACE;

  DEBUG_SYNC(thd, "after_error_checking");

  LEX *lex = thd->lex;
  set_var_base *var;
  if (!thd->lex->unit->is_prepared()) {
    lex->set_using_hypergraph_optimizer(
        thd->optimizer_switch_flag(OPTIMIZER_SWITCH_HYPERGRAPH_OPTIMIZER));

    const Prepared_stmt_arena_holder ps_arena_holder(thd);
    while ((var = it++)) {
      if ((error = var->resolve(thd))) goto err;
    }
    if ((error = thd->is_error())) goto err;
    thd->lex->unit->set_prepared();
    if (!thd->stmt_arena->is_regular()) thd->lex->save_cmd_properties(thd);
  }
  if (opened && lock_tables(thd, lex->query_tables, lex->table_count, 0)) {
    error = 1;
    goto err;
  }
  thd->lex->set_exec_started();
  it.rewind();
  while ((var = it++)) {
    if ((error = var->check(thd))) goto err;

    set_var *setvar = dynamic_cast<set_var *>(var);
    if (setvar &&
        (setvar->type == OPT_PERSIST || setvar->type == OPT_PERSIST_ONLY) &&
        setvar->m_var_tracker.cached_is_applied_as_command_line()) {
      /*
        There are certain variables that can process NULL as a default value
        TODO: there should be no exceptions!
      */
      static const std::set<std::string> exceptions = {
          "basedir",
          "character_sets_dir",
          "ft_stopword_file",
          "lc_messages_dir",
          "plugin_dir",
          "relay_log",
          "replica_load_tmpdir",
          "socket",
          "tmpdir",
          "init_file",
          "admin_ssl_ca",
          "admin_ssl_capath",
          "admin_ssl_cert",
          "admin_ssl_cipher",
          "admin_tls_ciphersuites",
          "admin_ssl_key",
          "admin_ssl_crl",
          "admin_ssl_crlpath",
          "ssl_ca",
          "ssl_capath",
          "ssl_cert",
          "ssl_cipher",
          "tls_ciphersuites",
          "ssl_key",
          "ssl_crl",
          "ssl_crlpath",
          "group_replication_recovery_tls_ciphersuites"};

      if (setvar->value && setvar->value->is_null() &&
          exceptions.find(setvar->m_var_tracker.get_var_name()) ==
              exceptions.end()) {
        /* an explicit NULL value */
        my_error(ER_NULL_CANT_BE_PERSISTED_FOR_READONLY, MYF(0),
                 setvar->m_var_tracker.get_var_name());
        error = 1;
        goto err;
      } else if (!setvar->value &&
                 setvar->m_var_tracker.cached_show_type() == SHOW_CHAR_PTR &&
                 exceptions.find(setvar->m_var_tracker.get_var_name()) ==
                     exceptions.end()) {
        /* SET = DEFAULT for a CHARPTR variable, check the default value */
        auto f = [&error](const System_variable_tracker &, sys_var *lvar) {
          assert(lvar->show_type() == SHOW_CHAR_PTR);
          char *ptr = (char *)(intptr)lvar->get_option()->def_value;
          if (!ptr) {
            my_error(ER_NULL_CANT_BE_PERSISTED_FOR_READONLY, MYF(0),
                     lvar->name.str);
            error = 1;
          }
        };
        setvar->m_var_tracker.access_system_variable(thd, f);
        if (error) goto err;
      }
    }
  }
  if ((error = thd->is_error())) goto err;

  it.rewind();
  while ((var = it++)) {
    if ((error = var->update(thd)))  // Returns 0, -1 or 1
      goto err;
  }
  if (!error) {
    /* At this point SET statement is considered a success. */
    Persisted_variables_cache *pv = nullptr;
    it.rewind();
    while ((var = it++)) {
      set_var *setvar = dynamic_cast<set_var *>(var);
      if (setvar &&
          (setvar->type == OPT_PERSIST || setvar->type == OPT_PERSIST_ONLY)) {
        pv = Persisted_variables_cache::get_instance();
        /* update in-memory copy of persistent options */
        if (pv->set_variable(thd, setvar)) return 1;
      }
    }
    /* flush all persistent options to a file */
    if (pv && pv->flush_to_file()) {
      my_error(ER_VARIABLE_NOT_PERSISTED, MYF(0));
      return 1;
    }
  }

err:
  for (set_var_base &v : *var_list) {
    v.cleanup();
  }
  free_underlaid_joins(thd->lex->query_block);
  return error;
}

/**
  This function is used to check if key management UDFs like
  keying_key_generate/store/remove should proceed or not. If global
  variable @@keyring_operations is OFF then above said udfs will fail.

  @return Operation status
    @retval 0 OK
    @retval 1 ERROR, keyring operations are not allowed

  @sa Sys_keyring_operations
*/
bool keyring_access_test() {
  bool keyring_operations;
  mysql_mutex_lock(&LOCK_keyring_operations);
  keyring_operations = !opt_keyring_operations;
  mysql_mutex_unlock(&LOCK_keyring_operations);
  return keyring_operations;
}

/*****************************************************************************
  Functions to handle SET mysql_internal_variable=const_expr
*****************************************************************************/

/**
  global X509 subject name to require from the client session
  to allow SET PERSIST[_ONLY] on sys_var::NOTPERSIST variables

  @sa set_var::resolve
*/
char *sys_var_persist_only_admin_x509_subject = nullptr;

/**
  Checks if a THD can set non-persist variables

  Requires that:
  * the session uses SSL
  * the peer has presented a valid certificate
  * the certificate has a certain subject name

  The format checked is deliberately kept the same as the
  other SSL system and status variables representing names.
  Hence X509_NAME_oneline is used.

  @retval true the THD can set NON_PERSIST variables
  @retval false usual restrictions apply
  @param thd the THD handle
  @param var the variable to be set
  @param setvar_type  the operation to check against.

  @sa sys_variables_admin_dn
*/
static bool can_persist_non_persistent_var(THD *thd, sys_var *var,
                                           enum_var_type setvar_type) {
  SSL *ssl = nullptr;
  X509 *cert = nullptr;
  char *ptr = nullptr;
  bool result = false;

  /* Bail off if no subject is set */
  if (likely(!sys_var_persist_only_admin_x509_subject ||
             !sys_var_persist_only_admin_x509_subject[0]))
    return false;

  /* Can't persist read only variables without command line support */
  if (unlikely(setvar_type == OPT_PERSIST_ONLY &&
               !var->is_settable_at_command_line() &&
               (var->is_readonly() || var->is_persist_readonly())))
    return false;

  /* do not allow setting the controlling variables */
  if (never_persistable_vars->find(var->name.str) !=
      never_persistable_vars->end())
    return false;

  ssl = thd->get_ssl();
  if (!ssl) return false;

  cert = SSL_get_peer_certificate(ssl);
  if (!cert) goto done;

  ptr = X509_NAME_oneline(X509_get_subject_name(cert), nullptr, 0);
  if (!ptr) goto done;

  result = !strcmp(sys_var_persist_only_admin_x509_subject, ptr);
done:
  if (ptr) OPENSSL_free(ptr);
  if (cert) X509_free(cert);
  return result;
}

/**
Resolve the variable assignment

@param thd Thread handler

@return status code
@retval -1 Failure
@retval 0 Success
*/

int set_var::resolve(THD *thd) {
  DBUG_TRACE;

  auto f = [this, thd](const System_variable_tracker &, sys_var *var) -> int {
    var->do_deprecated_warning(thd);
    if (var->is_readonly()) {
      if (type != OPT_PERSIST_ONLY) {
        my_error(ER_INCORRECT_GLOBAL_LOCAL_VAR, MYF(0), var->name.str,
                 "read only");
        return -1;
      }
      if (type == OPT_PERSIST_ONLY && var->is_non_persistent() &&
          !can_persist_non_persistent_var(thd, var, type)) {
        my_error(ER_INCORRECT_GLOBAL_LOCAL_VAR, MYF(0), var->name.str,
                 "non persistent read only");
        return -1;
      }
    }
    if (!var->check_scope(type)) {
      const int err =
          (is_global_persist()) ? ER_LOCAL_VARIABLE : ER_GLOBAL_VARIABLE;
      my_error(err, MYF(0), var->name.str);
      return -1;
    }
    if (type == OPT_GLOBAL || type == OPT_PERSIST) {
      /* Either the user has SUPER_ACL or she has SYSTEM_VARIABLES_ADMIN */
      if (check_priv(thd, false)) {
        return -1;
      }
    }
    if (type == OPT_PERSIST_ONLY) {
      if (check_priv(thd, true)) {
        return -1;
      }
    }

    /* check if read/write non-persistent variables can be persisted */
    if ((type == OPT_PERSIST || type == OPT_PERSIST_ONLY) &&
        var->is_non_persistent() &&
        !can_persist_non_persistent_var(thd, var, type)) {
      my_error(ER_INCORRECT_GLOBAL_LOCAL_VAR, MYF(0), var->name.str,
               "non persistent");
      return -1;
    }

    /* value is a NULL pointer if we are using SET ... = DEFAULT */
    if (value == nullptr || value->fixed) {
      return 0;
    }

    if (value->fix_fields(thd, &value)) {
      return -1;
    }
    /*
      If expression has no data type (e.g because it contains a parameter),
      assign type character string.
    */
    if (value->data_type() == MYSQL_TYPE_INVALID &&
        value->propagate_type(thd, MYSQL_TYPE_VARCHAR)) {
      return -1;
    }

    if (value->check_cols(1)) {
      return -1;
    }
    return 0;
  };

  return m_var_tracker
      .access_system_variable<int>(thd, f, Suppress_not_found_error::NO)
      .value_or(-1);
}

/**
  Verify that the supplied value is correct.

  @param thd Thread handler

  @return status code
   @retval -1 Failure
   @retval 0 Success
*/

int set_var::check(THD *thd) {
  DBUG_TRACE;
  DEBUG_SYNC(thd, "after_error_checking");

  /* value is a NULL pointer if we are using SET ... = DEFAULT */
  if (value == nullptr) {
    return 0;
  }

  auto f = [this, thd](const System_variable_tracker &, sys_var *var) -> int {
    if (var->check_update_type(value->result_type())) {
      my_error(ER_WRONG_TYPE_FOR_VAR, MYF(0), var->name.str);
      return -1;
    }
    return (type != OPT_PERSIST_ONLY && var->check(thd, this)) ? -1 : 0;
  };

  int ret =
      m_var_tracker
          .access_system_variable<int>(thd, f, Suppress_not_found_error::NO)
          .value_or(-1);

  if (!ret && (is_global_persist())) {
    ret = mysql_event_tracking_global_variable_notify(
        thd, AUDIT_EVENT(EVENT_TRACKING_GLOBAL_VARIABLE_SET),
        m_var_tracker.get_var_name(), value->item_name.ptr(),
        value->item_name.length());
  }

  return ret;
}

/**
  Check variable, but without assigning value (used by PS).

  @param thd            thread handler

  @retval
    0   ok
  @retval
    1   ERROR, message sent (normally no variables was updated)
  @retval
    -1   ERROR, message not sent
*/
int set_var::light_check(THD *thd) {
  auto f = [this](const System_variable_tracker &, sys_var *var) -> bool {
    if (!var->check_scope(type)) {
      const int err =
          (is_global_persist()) ? ER_LOCAL_VARIABLE : ER_GLOBAL_VARIABLE;
      my_error(err, MYF(0), var->name.str);
      return true;
    }
    return false;
  };
  if (m_var_tracker.access_system_variable<bool>(thd, f).value_or(true)) {
    return -1;
  }
  Security_context *sctx = thd->security_context();
  if ((type == OPT_GLOBAL || type == OPT_PERSIST) &&
      !(sctx->check_access(SUPER_ACL) ||
        sctx->has_global_grant(STRING_WITH_LEN("SYSTEM_VARIABLES_ADMIN"))
            .first))
    return 1;

  if ((type == OPT_PERSIST_ONLY) &&
      !(sctx->has_global_grant(STRING_WITH_LEN("PERSIST_RO_VARIABLES_ADMIN"))
            .first &&
        sctx->has_global_grant(STRING_WITH_LEN("SYSTEM_VARIABLES_ADMIN"))
            .first))
    return 1;

  if (value == nullptr || value->fixed) return 0;

  if (value->fix_fields(thd, &value)) {
    return -1;
  }
  /*
    If expression has no data type (e.g because it contains a parameter),
    assign type character string.
  */
  if (value->data_type() == MYSQL_TYPE_INVALID &&
      value->propagate_type(thd, MYSQL_TYPE_VARCHAR)) {
    return -1;
  }

  if (value->check_cols(1)) {
    return -1;
  }
  return 0;
}

/**
  Update variable source, user, host and timestamp values.
*/

void set_var::update_source_user_host_timestamp(THD *thd, sys_var *var) {
  var->set_source(enum_variable_source::DYNAMIC);
  var->set_source_name(EMPTY_CSTR.str);
  var->set_user_host(thd);
  var->set_timestamp();
}

/**
  Update variable

  @param   thd    thread handler
  @returns 0|1    ok or ERROR

  @note ERROR can be only due to abnormal operations involving
  the server's execution environment such as
  out of memory, hard disk failure or the computer blows up.
  Consider set_var::check() method if there is a need to return
  an error due to logics.
*/
int set_var::update(THD *thd) {
  auto f = [this, thd](const System_variable_tracker &, sys_var *var) -> bool {
    bool ret = false;
    /* for persist only syntax do not update the value */
    if (type != OPT_PERSIST_ONLY) {
      if (value)
        ret = var->update(thd, this);
      else
        ret = var->set_default(thd, this);
      /*
       For PERSIST_ONLY syntax we dont change the value of the variable
       for the current session, thus we should not change variables
       source/timestamp/user/host.
      */
      if (!ret) {
        update_source_user_host_timestamp(thd, var);
      }
    }
    return ret;
  };
  return m_var_tracker
                 .access_system_variable<bool>(thd, f,
                                               Suppress_not_found_error::NO)
                 .value_or(true)
             ? 1
             : 0;
}

void set_var::print_short(const THD *thd, String *str) {
  str->append(m_var_tracker.get_var_name());
  str->append(STRING_WITH_LEN("="));
  if (value) {
    if (m_var_tracker.cached_is_sensitive()) {
      str->append(STRING_WITH_LEN("<REDACTED>"));
    } else {
      value->print(thd, str, QT_ORDINARY);
    }
  } else
    str->append(STRING_WITH_LEN("DEFAULT"));
}

/**
  Self-print assignment

  @param thd Thread handle
  @param str String buffer to append the partial assignment to.

  @returns status of rewritten
*/
bool set_var::print(const THD *thd, String *str) {
  switch (type) {
    case OPT_PERSIST:
      str->append("PERSIST ");
      break;
    case OPT_PERSIST_ONLY:
      str->append("PERSIST_ONLY ");
      break;
    case OPT_GLOBAL:
      str->append("GLOBAL ");
      break;
    default:
      str->append("SESSION ");
  }
  if (m_var_tracker.is_keycache_var()) {
    str->append(m_var_tracker.get_keycache_name());
    str->append(STRING_WITH_LEN("."));
  }
  print_short(thd, str);
  return true;
}

/**
  Check if system variable is of type SENSITIVE

  @returns If variable is sensitive or not
*/
bool set_var::is_sensitive() const {
  return m_var_tracker.cached_is_sensitive();
}

/*****************************************************************************
  Functions to handle SET @user_variable=const_expr
*****************************************************************************/

int set_var_user::resolve(THD *thd) {
  /*
    Item_func_set_user_var can't substitute something else on its place =>
    NULL can be passed as last argument (reference on item)
  */
  return !user_var_item->fixed && user_var_item->fix_fields(thd, nullptr) ? -1
                                                                          : 0;
}

int set_var_user::check(THD *) {
  /*
    Item_func_set_user_var can't substitute something else on its place =>
    0 can be passed as last argument (reference on item)
  */
  return user_var_item->check(false) ? -1 : 0;
}

/**
  Check variable, but without assigning value (used by PS).

  @param thd            thread handler

  @retval
    0   ok
  @retval
    1   ERROR, message sent (normally no variables was updated)
  @retval
    -1   ERROR, message not sent
*/
int set_var_user::light_check(THD *thd) {
  /*
    Item_func_set_user_var can't substitute something else on its place =>
    0 can be passed as last argument (reference on item)
  */
  return (user_var_item->fix_fields(thd, (Item **)nullptr));
}

int set_var_user::update(THD *thd) {
  if (user_var_item->update()) {
    /* Give an error if it's not given already */
    my_error(ER_SET_CONSTANTS_ONLY, MYF(0));
    return -1;
  }
  if (thd->session_tracker.get_tracker(SESSION_STATE_CHANGE_TRACKER)
          ->is_enabled())
    thd->session_tracker.get_tracker(SESSION_STATE_CHANGE_TRACKER)
        ->mark_as_changed(thd, {});
  return 0;
}

bool set_var_user::print(const THD *thd, String *str) {
  user_var_item->print_assignment(thd, str, QT_ORDINARY);
  return true;
}

/*****************************************************************************
  Functions to handle SET PASSWORD
*****************************************************************************/

set_var_password::set_var_password(LEX_USER *user_arg, char *password_arg,
                                   char *current_password_arg,
                                   bool retain_current, bool gen_pass)
    : user(user_arg),
      password(password_arg),
      current_password(current_password_arg),
      retain_current_password(retain_current),
      generate_password(gen_pass) {
  if (current_password != nullptr) {
    user_arg->uses_replace_clause = true;
    user_arg->current_auth.str = current_password_arg;
    user_arg->current_auth.length = strlen(current_password_arg);
  }
  user_arg->retain_current_password = retain_current_password;
}

set_var_password::~set_var_password() {
  // We copied the generated password buffer to circumvent
  // the password nullification code in change_password()
  if (generate_password) my_free(password);
}

/**
  Check the validity of the SET PASSWORD request

  @param  thd  The current thread
  @return      status code
  @retval 0    failure
  @retval 1    success
*/
int set_var_password::check(THD *thd) {
  /* Returns 1 as the function sends error to client */
  return check_change_password(thd, user->host.str, user->user.str,
                               retain_current_password)
             ? 1
             : 0;
}

int set_var_password::update(THD *thd) {
  if (generate_password) {
    thd->m_disable_password_validation = true;
    std::string generated_password;
    generate_random_password(&generated_password,
                             thd->variables.generated_random_password_length);
    /*
      We need to copy the password buffer here because it will be set to \0
      later by change_password() and since we're generated a random password
      we need to retain it until it can be sent to the client.
      Because set_var_password never will get its destructor called we also
      need to move the string allocated memory to the THD mem root.
    */
    password = thd->mem_strdup(generated_password.c_str());
    str_generated_password = thd->mem_strdup(generated_password.c_str());
  }
  /* Returns 1 as the function sends error to client */
  auto res = change_password(thd, user, password, current_password,
                             retain_current_password)
                 ? 1
                 : 0;
  return res;
}

bool set_var_password::print(const THD *thd, String *str) {
  if (user->user.str != nullptr && user->user.length > 0) {
    str->append(STRING_WITH_LEN("PASSWORD FOR "));
    append_identifier(thd, str, user->user.str, user->user.length);
    if (user->host.str != nullptr && user->host.length > 0) {
      str->append(STRING_WITH_LEN("@"));
      append_identifier(thd, str, user->host.str, user->host.length);
    }
    str->append(STRING_WITH_LEN("="));
  } else
    str->append(STRING_WITH_LEN("PASSWORD FOR CURRENT_USER()="));
  str->append(STRING_WITH_LEN("<secret>"));
  if (user->uses_replace_clause) {
    str->append(STRING_WITH_LEN(" REPLACE <secret>"));
  }
  if (user->retain_current_password) {
    str->append(STRING_WITH_LEN(" RETAIN CURRENT PASSWORD"));
  }
  return true;
}

/*****************************************************************************
  Functions to handle SET NAMES and SET CHARACTER SET
*****************************************************************************/

int set_var_collation_client::check(THD *) {
  /* Currently, UCS-2 cannot be used as a client character set */
  if (!is_supported_parser_charset(character_set_client)) {
    my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), "character_set_client",
             character_set_client->csname);
    return 1;
  }
  return 0;
}

int set_var_collation_client::update(THD *thd) {
  thd->variables.character_set_client = character_set_client;
  thd->variables.character_set_results = character_set_results;
  thd->variables.collation_connection = collation_connection;
  thd->update_charset();

  /* Mark client collation variables as changed */
  if (thd->session_tracker.get_tracker(SESSION_SYSVARS_TRACKER)->is_enabled()) {
    const LEX_CSTRING cs_client = {STRING_WITH_LEN("character_set_client")};
    thd->session_tracker.get_tracker(SESSION_SYSVARS_TRACKER)
        ->mark_as_changed(thd, cs_client);
    const LEX_CSTRING cs_results = {STRING_WITH_LEN("character_set_results")};
    thd->session_tracker.get_tracker(SESSION_SYSVARS_TRACKER)
        ->mark_as_changed(thd, cs_results);
    const LEX_CSTRING cs_connection = {
        STRING_WITH_LEN("character_set_connection")};
    thd->session_tracker.get_tracker(SESSION_SYSVARS_TRACKER)
        ->mark_as_changed(thd, cs_connection);
  }
  if (thd->session_tracker.get_tracker(SESSION_STATE_CHANGE_TRACKER)
          ->is_enabled())
    thd->session_tracker.get_tracker(SESSION_STATE_CHANGE_TRACKER)
        ->mark_as_changed(thd, {});
  thd->protocol_text->init(thd);
  thd->protocol_binary->init(thd);
  return 0;
}

bool set_var_collation_client::print(const THD *, String *str) {
  str->append((set_cs_flags & SET_CS_NAMES) ? "NAMES " : "CHARACTER SET ");
  if (set_cs_flags & SET_CS_DEFAULT)
    str->append("DEFAULT");
  else {
    str->append("'");
    str->append(character_set_client->csname);
    str->append("'");
    if (set_cs_flags & SET_CS_COLLATE) {
      str->append(" COLLATE '");
      str->append(collation_connection->m_coll_name);
      str->append("'");
    }
  }
  return true;
}
