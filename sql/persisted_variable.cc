/* Copyright (c) 2016, 2023, Oracle and/or its affiliates.

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

#include "sql/persisted_variable.h"

#include "my_config.h"

#include <assert.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <algorithm>
#include <iomanip>
#include <memory>
#include <new>
#include <utility>

#include "mysql/components/library_mysys/my_hex_tools.h"

#include "keyring_operations_helper.h"
#include "lex_string.h"
#include "m_ctype.h"
#include "m_string.h"
#include "my_aes.h"
#include "my_compiler.h"

#include "my_default.h"  // check_file_permissions
#include "my_getopt.h"
#include "my_io.h"
#include "my_loglevel.h"
#include "my_macros.h"
#include "my_rnd.h"
#include "my_sys.h"
#include "my_thread.h"
#include "mysql/components/services/bits/psi_bits.h"
#include "mysql/components/services/bits/psi_file_bits.h"
#include "mysql/components/services/bits/psi_memory_bits.h"
#include "mysql/components/services/bits/psi_mutex_bits.h"
#include "mysql/components/services/log_builtins.h"
#include "mysql/components/services/log_shared.h"
#include "mysql/components/services/system_variable_source_type.h"
#include "mysql/psi/mysql_file.h"
#include "mysql/psi/mysql_memory.h"
#include "mysql/psi/mysql_mutex.h"
#include "mysql/status_var.h"
#include "mysql_version.h"
#include "mysqld_error.h"
#include "prealloced_array.h"
#include "scope_guard.h"
#include "sql-common/json_dom.h"
#include "sql/auth/auth_acls.h"
#include "sql/auth/auth_internal.h"
#include "sql/auth/sql_security_ctx.h"
#include "sql/current_thd.h"
#include "sql/debug_sync.h"  // DEBUG_SYNC
#include "sql/derror.h"      // ER_THD
#include "sql/item.h"
#include "sql/log.h"
#include "sql/mysqld.h"
#include "sql/server_component/mysql_server_keyring_lockable_imp.h"
#include "sql/set_var.h"
#include "sql/sql_class.h"
#include "sql/sql_error.h"
#include "sql/sql_lex.h"
#include "sql/sql_list.h"
#include "sql/sql_show.h"
#include "sql/sys_vars_shared.h"
#include "sql/thr_malloc.h"
#include "sql_string.h"
#include "template_utils.h"
#include "thr_mutex.h"
#include "typelib.h"

using std::map;
using std::string;

namespace {
/* Keys used by mysqld-auto.cnf JSON file */

/* Options metadata keys */
constexpr char s_key_value[] = "Value";
constexpr char s_key_metadata[] = "Metadata";
constexpr char s_key_timestamp[] = "Timestamp";
constexpr char s_key_user[] = "User";
constexpr char s_key_host[] = "Host";
/* Options category keys */
constexpr char s_key_mysql_dynamic_parse_early_variables[] =
    "mysql_dynamic_parse_early_variables";
constexpr char s_key_mysql_sensitive_dynamic_variables[] =
    "mysql_dynamic_sensitive_variables";
constexpr char s_key_mysql_dynamic_variables[] = "mysql_dynamic_variables";
constexpr char s_key_mysql_static_parse_early_variables[] =
    "mysql_static_parse_early_variables";
constexpr char s_key_mysql_sensitive_static_variables[] =
    "mysql_static_sensitive_variables";
constexpr char s_key_mysql_static_variables[] = "mysql_static_variables";
/* Sensitive variables section key */
constexpr char s_key_mysql_sensitive_variables[] = "mysql_sensitive_variables";
/* Encrypted section keys */
constexpr char s_key_master_key_id[] = "master_key_id";
constexpr char s_key_file_key[] = "file_key";
constexpr char s_key_file_key_iv[] = "file_key_iv";
constexpr char s_key_key_encryption_algorithm[] = "key_encryption_algorithm";
constexpr char s_key_data_encryption_algorithm[] = "data_encryption_algorithm";
constexpr char s_key_mysql_sensitive_variables_iv[] =
    "mysql_sensitive_variables_iv";
constexpr char s_key_mysql_sensitive_variables_blob[] =
    "mysql_sensitive_variables_blob";
constexpr char s_key_mysql_encryption_algorithm_default[] = "AES_256_CBC";
}  // namespace

PSI_file_key key_persist_file_cnf;

#ifdef HAVE_PSI_FILE_INTERFACE
static PSI_file_info all_persist_files[] = {
    {&key_persist_file_cnf, "cnf", 0, 0, PSI_DOCUMENT_ME}};
#endif /* HAVE_PSI_FILE_INTERFACE */

PSI_mutex_key key_persist_file, key_persist_variables;

#ifdef HAVE_PSI_MUTEX_INTERFACE
static PSI_mutex_info all_persist_mutexes[] = {
    {&key_persist_file, "m_LOCK_persist_file", 0, 0, PSI_DOCUMENT_ME},
    {&key_persist_variables, "m_LOCK_persist_variables", 0, 0,
     PSI_DOCUMENT_ME}};
#endif /* HAVE_PSI_MUTEX_INTERFACE */

PSI_memory_key key_memory_persisted_variables;

#ifdef HAVE_PSI_MEMORY_INTERFACE
static PSI_memory_info all_options[] = {
    {&key_memory_persisted_variables, "persisted_options_root", 0,
     PSI_FLAG_ONLY_GLOBAL_STAT, PSI_DOCUMENT_ME}};
#endif /* HAVE_PSI_MEMORY_INTERFACE */

#ifdef HAVE_PSI_INTERFACE
void my_init_persist_psi_keys(void) {
  const char *category [[maybe_unused]] = "persist";
  int count [[maybe_unused]];

#ifdef HAVE_PSI_FILE_INTERFACE
  count = sizeof(all_persist_files) / sizeof(all_persist_files[0]);
  mysql_file_register(category, all_persist_files, count);
#endif

#ifdef HAVE_PSI_MUTEX_INTERFACE
  count = static_cast<int>(array_elements(all_persist_mutexes));
  mysql_mutex_register(category, all_persist_mutexes, count);
#endif

#ifdef HAVE_PSI_MEMORY_INTERFACE
  count = static_cast<int>(array_elements(all_options));
  mysql_memory_register(category, all_options, count);
#endif
}
#endif

/** A comparison operator to sort persistent variables entries by timestamp */
struct sort_tv_by_timestamp {
  bool operator()(const st_persist_var x, const st_persist_var y) const {
    return x.timestamp < y.timestamp;
  }
};

Persisted_variables_cache *Persisted_variables_cache::m_instance = nullptr;

namespace {
std::string tolower_varname(const char *name) {
  std::string str(name);
  std::transform(str.begin(), str.end(), str.begin(), tolower);
  return str;
}
}  // namespace

/* Standard Constructors for st_persist_var */

st_persist_var::st_persist_var() {
  if (current_thd) {
    my_timeval tv =
        current_thd->query_start_timeval_trunc(DATETIME_MAX_DECIMALS);
    timestamp = tv.m_tv_sec * 1000000ULL + tv.m_tv_usec;
  } else
    timestamp = my_micro_time();
  is_null = false;
}

st_persist_var::st_persist_var(THD *thd) {
  my_timeval tv = thd->query_start_timeval_trunc(DATETIME_MAX_DECIMALS);
  timestamp = tv.m_tv_sec * 1000000ULL + tv.m_tv_usec;
  user = thd->security_context()->user().str;
  host = thd->security_context()->host().str;
  is_null = false;
}

st_persist_var::st_persist_var(const std::string key, const std::string value,
                               const ulonglong timestamp,
                               const std::string user, const std::string host,
                               const bool is_null) {
  this->key = key;
  this->value = value;
  this->timestamp = timestamp;
  this->user = user;
  this->host = host;
  this->is_null = is_null;
}

Persisted_variables_cache::Persisted_variables_cache()
    : m_persisted_static_sensitive_variables(
          key_memory_persisted_variables_unordered_map),
      m_persisted_static_parse_early_variables(
          key_memory_persisted_variables_unordered_map),
      m_persisted_static_variables(
          key_memory_persisted_variables_unordered_map),
      m_persisted_dynamic_parse_early_variables(
          key_memory_persisted_variables_unordered_set),
      m_persisted_dynamic_sensitive_variables(
          key_memory_persisted_variables_unordered_set),
      m_persisted_dynamic_variables(
          key_memory_persisted_variables_unordered_set),
      m_persisted_dynamic_sensitive_plugin_variables(
          key_memory_persisted_variables_unordered_set),
      m_persisted_dynamic_plugin_variables(
          key_memory_persisted_variables_unordered_set) {}

/**
  Initialize class members. This function reads datadir if present in
  config file or set at command line, in order to know from where to
  load this config file. If datadir is not set then read from MYSQL_DATADIR.

   @param [in] argc                      Pointer to argc of original program
   @param [in] argv                      Pointer to argv of original program

   @return 0 Success
   @return 1 Failure

*/
int Persisted_variables_cache::init(int *argc, char ***argv) {
#ifdef HAVE_PSI_INTERFACE
  my_init_persist_psi_keys();
#endif

  int temp_argc = *argc;
  MEM_ROOT alloc{key_memory_persisted_variables, 512};
  char *ptr, **res, *datadir = nullptr;
  char dir[FN_REFLEN] = {0}, local_datadir_buffer[FN_REFLEN] = {0};
  const char *dirs = nullptr;
  bool persist_load = true;

  my_option persist_options[] = {
      {"persisted_globals_load", 0, "", &persist_load, &persist_load, nullptr,
       GET_BOOL, OPT_ARG, 1, 0, 0, nullptr, 0, nullptr},
      {"datadir", 0, "", &datadir, nullptr, nullptr, GET_STR, OPT_ARG, 0, 0, 0,
       nullptr, 0, nullptr},
      {nullptr, 0, nullptr, nullptr, nullptr, nullptr, GET_NO_ARG, NO_ARG, 0, 0,
       0, nullptr, 0, nullptr}};

  /* create temporary args list and pass it to handle_options */
  if (!(ptr =
            (char *)alloc.Alloc(sizeof(alloc) + (*argc + 1) * sizeof(char *))))
    return 1;
  memset(ptr, 0, (sizeof(char *) * (*argc + 1)));
  res = (char **)(ptr);
  memcpy((uchar *)res, (char *)(*argv), (*argc) * sizeof(char *));

  my_getopt_skip_unknown = true;
  if (my_handle_options(&temp_argc, &res, persist_options, nullptr, nullptr,
                        true)) {
    alloc.Clear();
    return 1;
  }
  my_getopt_skip_unknown = false;
  alloc.Clear();

  persisted_globals_load = persist_load;

  if (!datadir) {
    // mysql_real_data_home must be initialized at this point
    assert(mysql_real_data_home[0]);
    /*
      mysql_home_ptr should also be initialized at this point.
      See calculate_mysql_home_from_my_progname() for details
    */
    assert(mysql_home_ptr && mysql_home_ptr[0]);
    convert_dirname(local_datadir_buffer, mysql_real_data_home, NullS);
    (void)my_load_path(local_datadir_buffer, local_datadir_buffer,
                       mysql_home_ptr);
    datadir = local_datadir_buffer;
  }

  dirs = datadir;
  unpack_dirname(dir, dirs);
  my_realpath(datadir_buffer, dir, MYF(0));
  unpack_dirname(datadir_buffer, datadir_buffer);
  if (fn_format(dir, MYSQL_PERSIST_CONFIG_NAME, datadir_buffer, ".cnf",
                MY_UNPACK_FILENAME | MY_SAFE_PATH) == nullptr)
    return 1;
  m_persist_filename = string(dir);
  m_persist_backup_filename = m_persist_filename + ".backup";

  mysql_mutex_init(key_persist_variables, &m_LOCK_persist_variables,
                   MY_MUTEX_INIT_FAST);

  mysql_mutex_init(key_persist_file, &m_LOCK_persist_file, MY_MUTEX_INIT_FAST);

  m_instance = this;
  return 0;
}

/**
  Return a singleton object
*/
Persisted_variables_cache *Persisted_variables_cache::get_instance() {
  assert(m_instance != nullptr);
  return m_instance;
}

/**
  For boolean variable types do validation on what value is set for the
  variable and then report error in case an invalid value is set.

   @param [in]  value        Value which needs to be checked for.
   @param [out] bool_str     Target String into which correct value needs to be
                             stored after validation.

   @return true  Failure if value is set to anything other than "true", "on",
                 "1", "false" , "off", "0"
   @return false Success
*/
static bool check_boolean_value(const char *value, String &bool_str) {
  bool ret = false;
  bool result = get_bool_argument(value, &ret);
  if (ret) return true;
  if (result) {
    bool_str = String("ON", system_charset_info);
  } else {
    bool_str = String("OFF", system_charset_info);
  }
  return false;
}

/**
  Retrieve variables name/value and update the in-memory copy with
  this new values. If value is default then remove this entry from
  in-memory copy, else update existing key with new value

   @param [in] thd           Pointer to connection handler
   @param [in] setvar        Pointer to set_var which is being SET

   @return true  Failure
   @return false Success
*/
bool Persisted_variables_cache::set_variable(THD *thd, set_var *setvar) {
  auto f = [this, thd, setvar](const System_variable_tracker &,
                               sys_var *system_var) -> bool {
    char val_buf[1024] = {0};
    String utf8_str;
    bool is_null = false;

    std::string var_name{tolower_varname(setvar->m_var_tracker.get_var_name())};

    // 1. Fetch value into local variable var_value.

    const char *var_value = val_buf;
    if (setvar->type == OPT_PERSIST_ONLY) {
      String str(val_buf, sizeof(val_buf), system_charset_info), *res;
      const CHARSET_INFO *tocs = &my_charset_utf8mb4_bin;
      uint dummy_err;
      String bool_str;
      if (setvar->value) {
        res = setvar->value->val_str(&str);
        if (system_var->get_var_type() == GET_BOOL) {
          if (res == nullptr ||
              check_boolean_value(res->c_ptr_quick(), bool_str)) {
            my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), var_name.c_str(),
                     (res ? res->c_ptr_quick() : "null"));
            return true;
          } else {
            res = &bool_str;
          }
        }
        if (res && res->length()) {
          /*
            value held by Item class can be of different charset,
            so convert to utf8mb4
          */
          utf8_str.copy(res->ptr(), res->length(), res->charset(), tocs,
                        &dummy_err);
          var_value = utf8_str.c_ptr_quick();
        }
      } else {
        /* persist default value */
        system_var->save_default(thd, setvar);
        system_var->saved_value_to_string(thd, setvar, str.ptr());
        res = &str;
        if (system_var->get_var_type() == GET_BOOL) {
          check_boolean_value(res->c_ptr_quick(), bool_str);
          res = &bool_str;
        }
        utf8_str.copy(res->ptr(), res->length(), res->charset(), tocs,
                      &dummy_err);
        var_value = utf8_str.c_ptr_quick();
      }
    } else {
      Persisted_variables_cache::get_variable_value(thd, system_var, &utf8_str,
                                                    &is_null);
      var_value = utf8_str.c_ptr_quick();
    }

    // 2. Store local variable var_value into member st_persist_var object.

    auto assign_value = [&](const char *name) -> bool {
      struct st_persist_var tmp_var(thd);
      bool is_sensitive = system_var->is_sensitive();

      if (is_sensitive && !m_keyring_support_available) {
        if (!opt_persist_sensitive_variables_in_plaintext) {
          my_error(ER_CANNOT_PERSIST_SENSITIVE_VARIABLES, MYF(0), name);
          return true;
        } else {
          push_warning_printf(
              thd, Sql_condition::SL_WARNING,
              ER_WARN_CANNOT_SECURELY_PERSIST_SENSITIVE_VARIABLES,
              ER_THD(thd, ER_WARN_CANNOT_SECURELY_PERSIST_SENSITIVE_VARIABLES),
              name);
        }
      }

      tmp_var.key = string(name);
      tmp_var.value = var_value;
      tmp_var.is_null = is_null;

      /* modification to in-memory must be thread safe */
      lock();
      DEBUG_SYNC(thd, "in_set_persist_variables");
      if ((setvar->type == OPT_PERSIST_ONLY && system_var->is_readonly()) ||
          system_var->is_persist_readonly()) {
        /* if present update variable with new value else insert into hash */
        if (system_var->is_parse_early()) {
          m_persisted_static_parse_early_variables[tmp_var.key] = tmp_var;
        } else {
          auto &variables = is_sensitive
                                ? m_persisted_static_sensitive_variables
                                : m_persisted_static_variables;
          variables[tmp_var.key] = tmp_var;
          /**
            If server was upgraded, it is possible that persisted variables were
            initially read from an old format file. If so, all RO variables:
            PARSE_EARLY or otherwise, persisted before the upgrade may be
            present in m_persisted_static_parse_early_variables container.

            This SET PERSIST/SET PERSIST_ONLY call may be setting one of those
            variables. If so, remove those values from
            m_persisted_static_parse_early_variables.
          */
          auto it = m_persisted_static_parse_early_variables.find(name);
          if (it != m_persisted_static_parse_early_variables.end()) {
            m_persisted_static_parse_early_variables.erase(it);
          }
        }
      } else {
        /*
       if element is present remove it and insert
       it again with new value.
        */
        if (system_var->is_parse_early()) {
          m_persisted_dynamic_parse_early_variables.erase(tmp_var);
          m_persisted_dynamic_parse_early_variables.insert(tmp_var);
          /*
            If server was upgraded, it is possible that persisted variables were
            initially read from an old format file. If so, all variables:
            PARSE_EARLY or otherwise, persisted before the upgrade may be
            present in m_persisted_dynamic_variables container.

            This SET PERSIST/SET PERSIST_ONLY call may be setting one of those
            variables. If so, remove those values from
            m_persisted_dynamic_variables.
          */
          m_persisted_dynamic_variables.erase(tmp_var);
        } else {
          auto &variables = is_sensitive
                                ? m_persisted_dynamic_sensitive_variables
                                : m_persisted_dynamic_variables;
          variables.erase(tmp_var);
          variables.insert(tmp_var);
          /*
            for plugin variables update m_persisted_dynamic_plugin_variables
            or m_persisted_dynamic_sensitive_plugin_variables
          */
          if (system_var->cast_pluginvar()) {
            auto &plugin_variables =
                system_var->is_sensitive()
                    ? m_persisted_dynamic_sensitive_plugin_variables
                    : m_persisted_dynamic_plugin_variables;
            plugin_variables.erase(tmp_var);
            plugin_variables.insert(tmp_var);
          }
        }
      }

      if (setvar->type != OPT_PERSIST_ONLY) {
        setvar->update_source_user_host_timestamp(thd, system_var);
      }
      unlock();
      return false;
    };

    if (assign_value(var_name.c_str())) return true;

    const char *alias_var_name = get_variable_alias(system_var);

    if (alias_var_name) assign_value(alias_var_name);

    return false;
  };

  return setvar->m_var_tracker.access_system_variable<bool>(thd, f).value_or(
      true);
}

/**
  Retrieve variables value from sys_var

   @param [in] thd           Pointer to connection handler
   @param [in] system_var    Pointer to sys_var which is being SET
   @param [in] str           Pointer to String instance into which value
                             is copied
   @param [out] is_null      Is value NULL or not.

   @return
     Pointer to String instance holding the value
*/
String *Persisted_variables_cache::get_variable_value(THD *thd,
                                                      sys_var *system_var,
                                                      String *str,
                                                      bool *is_null) {
  const char *value;
  char val_buf[1024];
  size_t val_length;
  char show_var_buffer[sizeof(SHOW_VAR)];
  SHOW_VAR *show = (SHOW_VAR *)show_var_buffer;
  const CHARSET_INFO *fromcs;
  const CHARSET_INFO *tocs = &my_charset_utf8mb4_bin;
  uint dummy_err;

  show->type = SHOW_SYS;
  show->name = system_var->name.str;
  show->value = (char *)system_var;

  mysql_mutex_lock(&LOCK_global_system_variables);
  value = get_one_variable(thd, show, OPT_GLOBAL, show->type, nullptr, &fromcs,
                           val_buf, &val_length, is_null);
  /* convert the retrieved value to utf8mb4 */
  str->copy(value, val_length, fromcs, tocs, &dummy_err);
  mysql_mutex_unlock(&LOCK_global_system_variables);
  return str;
}

const char *Persisted_variables_cache::get_variable_alias(
    const sys_var *system_var) {
  if (system_var->m_persisted_alias)
    return system_var->m_persisted_alias->name.str;
  return nullptr;
}

std::string Persisted_variables_cache::get_variable_alias(const char *name) {
  auto f = [](const System_variable_tracker &, sys_var *sysvar) -> std::string {
    const char *ret = get_variable_alias(sysvar);
    return ret ? ret : std::string{};
  };
  return System_variable_tracker::make_tracker(name)
      .access_system_variable<std::string>(current_thd, f,
                                           Suppress_not_found_error::YES)
      .value_or(std::string{});
}

static bool format_json(const st_persist_var &entry,
                        Json_object &section_object) {
  /*
    Create metadata array
    "Metadata" : {
    "Timestamp" : timestamp_value,
    "User" : "user_name",
    "Host" : "host_name"
    }
  */
  Json_object object_metadata;
  Json_uint value_timestamp(entry.timestamp);
  if (object_metadata.add_clone(s_key_timestamp, &value_timestamp)) return true;

  Json_string value_user(entry.user.c_str());
  if (object_metadata.add_clone(s_key_user, &value_user)) return true;

  Json_string value_host(entry.host.c_str());
  if (object_metadata.add_clone(s_key_host, &value_host)) return true;

  /*
    Create variable object

    "variable_name" : {
      "Value" : "variable_value",
      "Metadata" : {
      "Timestamp" : timestamp_value,
      "User" : "user_name",
      "Host" : "host_name"
      }
    },
  */
  Json_object variable;

  Json_string value_value(entry.value.c_str());
  if (variable.add_clone(s_key_value, &value_value)) return true;

  if (variable.add_clone(s_key_metadata, &object_metadata)) return true;

  /* Push object to array */
  if (section_object.add_clone(entry.key, &variable)) return true;
  return false;
}

static bool add_json_object(Json_object &section_object,
                            Json_object &json_object, const char *key_section) {
  if (section_object.cardinality() > 0) {
    if (json_object.add_clone(key_section, &section_object)) return true;
  }
  return false;
}

static bool format_set(Persisted_variables_uset &section, string key_section,
                       Json_object &json_object) {
  Json_object section_object;
  for (auto &it : section) {
    if (format_json(it, section_object)) return true;
  }
  if (add_json_object(section_object, json_object, key_section.c_str()))
    return true;
  return false;
}

static bool format_map(Persisted_variables_umap &section, string key_section,
                       Json_object &json_object) {
  Json_object section_object;
  for (auto &it : section) {
    if (format_json(it.second, section_object)) return true;
  }
  if (add_json_object(section_object, json_object, key_section.c_str()))
    return true;
  return false;
}

bool Persisted_variables_cache::write_persist_file_v2(String &dest,
                                                      bool &clean_up) {
  clean_up = false;
  Json_object main_json_object;
  string key_version("Version");
  Json_int value_version(static_cast<int>(File_version::VERSION_V2));
  main_json_object.add_clone(key_version.c_str(), &value_version);

  /**
    If original file was of File_version::VERSION_V1, some of the variables
    which may belong to object "mysql_static_variables" could be part of
    "mysql_static_parse_early_variables" object. This is because we move
    such variables to "mysql_static_variables" only when SET PERSIST or
    SET PERSIST_ONLY is executed for them.
  */
  if (format_map(m_persisted_static_parse_early_variables,
                 s_key_mysql_static_parse_early_variables, main_json_object))
    return true;

  if (format_set(m_persisted_dynamic_parse_early_variables,
                 s_key_mysql_dynamic_parse_early_variables, main_json_object))
    return true;

  if (format_map(m_persisted_static_variables, s_key_mysql_static_variables,
                 main_json_object))
    return true;

  if (format_set(m_persisted_dynamic_variables, s_key_mysql_dynamic_variables,
                 main_json_object))
    return true;

  Json_object sensitive_variables_object;
  auto encryption_success = encrypt_sensitive_variables();

  if (encryption_success != return_status::NOT_REQUIRED) {
    if (encryption_success == return_status::ERROR &&
        opt_persist_sensitive_variables_in_plaintext == false) {
      LogErr(WARNING_LEVEL, ER_WARN_CANNOT_PERSIST_SENSITIVE_VARIABLES);
      return true;
    }

    auto add_string = [](Json_object &object, const std::string key,
                         const std::string &value) -> bool {
      if (value.length() == 0) return false;
      Json_string string_value(value.c_str());
      return object.add_clone(key, &string_value);
    };

    if (add_string(sensitive_variables_object, s_key_master_key_id,
                   m_key_info.m_master_key_name))
      return true;

    if (add_string(sensitive_variables_object, s_key_file_key,
                   m_key_info.m_file_key))
      return true;

    if (add_string(sensitive_variables_object, s_key_file_key_iv,
                   m_key_info.m_file_key_iv))
      return true;

    if (add_string(sensitive_variables_object, s_key_key_encryption_algorithm,
                   "AES_256_CBC"))
      return true;

    if (add_string(sensitive_variables_object,
                   s_key_mysql_sensitive_variables_blob,
                   m_sensitive_variables_blob))
      return true;

    if (add_string(sensitive_variables_object,
                   s_key_mysql_sensitive_variables_iv, m_iv))
      return true;

    if (add_string(sensitive_variables_object, s_key_data_encryption_algorithm,
                   "AES_256_CBC"))
      return true;

    if (encryption_success == return_status::ERROR) {
      if (format_set(m_persisted_dynamic_sensitive_variables,
                     s_key_mysql_sensitive_dynamic_variables,
                     sensitive_variables_object))
        return true;
      if (format_map(m_persisted_static_sensitive_variables,
                     s_key_mysql_sensitive_static_variables,
                     sensitive_variables_object))
        return true;
    }

    if (add_json_object(sensitive_variables_object, main_json_object,
                        s_key_mysql_sensitive_variables))
      return true;
  }

  Json_wrapper json_wrapper(&main_json_object);
  json_wrapper.set_alias();
  String str;
  json_wrapper.to_string(&str, true, String().ptr(),
                         JsonDocumentDefaultDepthHandler);
  dest.append(str);

  if (encryption_success == return_status::SUCCESS) {
    /*
      If we succeeded in writing sensitive variables in blob, clear them
      before next write operation
    */
    clean_up = true;
  }

  return false;
}

/**
  Convert in-memory copy into a stream of characters and write this
  stream to persisted config file

  @return Error state
    @retval true An error occurred
    @retval false Success
*/
bool Persisted_variables_cache::flush_to_file() {
  lock();
  mysql_mutex_lock(&m_LOCK_persist_file);
  String dest("", &my_charset_utf8mb4_bin);
  bool ret = true;
  bool do_cleanup = false;

  ret = write_persist_file_v2(dest, do_cleanup);

  if (ret == true) {
    mysql_mutex_unlock(&m_LOCK_persist_file);
    unlock();
    return ret;
  }
  /*
    Always write to backup file. Once write is successful, rename backup
    file to original file.
  */
  if (open_persist_backup_file(O_CREAT | O_WRONLY)) {
    ret = true;
  } else {
    DBUG_EXECUTE_IF("crash_after_open_persist_file", DBUG_SUICIDE(););
    /* write to file */
    if (mysql_file_fputs(dest.c_ptr(), m_fd) < 0) {
      ret = true;
    }
    DBUG_EXECUTE_IF("crash_after_write_persist_file", DBUG_SUICIDE(););
    /* flush contents to disk immediately */
    if (mysql_file_fflush(m_fd) != 0) ret = true;
    if (my_sync(my_fileno(m_fd->m_file), MYF(MY_WME)) == -1) ret = true;
  }
  close_persist_file();
  if (!ret) {
    DBUG_EXECUTE_IF("crash_after_close_persist_file", DBUG_SUICIDE(););
    my_rename(m_persist_backup_filename.c_str(), m_persist_filename.c_str(),
              MYF(MY_WME));
  }
  if (ret == false && do_cleanup == true) clear_sensitive_blob_and_iv();
  mysql_mutex_unlock(&m_LOCK_persist_file);
  unlock();
  return ret;
}

/**
  Open persisted config file

  @param [in] flag    File open mode
  @return Error state
    @retval true An error occurred
    @retval false Success
*/
bool Persisted_variables_cache::open_persist_file(int flag) {
  /*
    If file does not exists create one. When persisted_globals_load is 0
    we dont read contents of mysqld-auto.cnf file, thus append any new
    variables which are persisted to this file.
  */
  if (m_fd) return 1;
  m_fd = mysql_file_fopen(key_persist_file_cnf, m_persist_filename.c_str(),
                          flag, MYF(0));

  return (m_fd ? 0 : 1);
}

/**
  Open persisted backup config file

  @param [in] flag file open mode
  @return Error state
    @retval true An error occurred
    @retval false Success
*/
bool Persisted_variables_cache::open_persist_backup_file(int flag) {
  if (m_fd) return 1;
  m_fd = mysql_file_fopen(key_persist_file_cnf,
                          m_persist_backup_filename.c_str(), flag, MYF(0));
  return (m_fd ? 0 : 1);
}

/**
  Close persisted config file.
*/
void Persisted_variables_cache::close_persist_file() {
  mysql_file_fclose(m_fd, MYF(0));
  m_fd = nullptr;
}

/**
  load_persist_file() read persisted config file

  @return Error state
    @retval true An error occurred
    @retval false Success
*/
bool Persisted_variables_cache::load_persist_file() {
  if (read_persist_file() > 0) return true;
  return false;
}

void Persisted_variables_cache::set_parse_early_sources() {
  /* create a sorted set of values sorted by timestamp */
  std::multiset<st_persist_var, sort_tv_by_timestamp> sorted_vars;
  for (auto &iter : m_persisted_static_parse_early_variables)
    sorted_vars.insert(iter.second);
  sorted_vars.insert(m_persisted_dynamic_parse_early_variables.begin(),
                     m_persisted_dynamic_parse_early_variables.end());

  for (auto &it : sorted_vars) {
    auto sysvar = intern_find_sys_var(it.key.c_str(), it.key.length());
    sysvar->set_source(enum_variable_source::PERSISTED);
#ifndef NDEBUG
    bool source_truncated =
#endif
        sysvar->set_source_name(m_persist_filename.c_str());
    assert(!source_truncated);
    sysvar->set_timestamp(it.timestamp);
    if (sysvar->set_user(it.user.c_str()))
      LogErr(WARNING_LEVEL, ER_PERSIST_OPTION_USER_TRUNCATED, it.key.c_str());
    if (sysvar->set_host(it.host.c_str()))
      LogErr(WARNING_LEVEL, ER_PERSIST_OPTION_HOST_TRUNCATED, it.key.c_str());
  }
}

/**
  set_persisted_options() will set the options read from persisted config file

  This function does nothing when --no-defaults is set or if
  persisted_globals_load is set to false.
  Initial call to set_persisted_options(false) is needed to initialize
  m_persisted_dynamic_plugin_variables set, so that next subsequent
  set_persisted_options(true) calls will work with correct state.

   @param [in] plugin_options      Flag which tells what options are being set.
                                   If set to false non dynamically-registered
                                   system variables are set
                                   else plugin- and component-registered
                                   variables are set.
   @param [in] target_var_name     If not-null the name of variable to try and
                                   set from the persisted cache values
   @param [in] target_var_name_length length of target_var_name
  @return Error state
    @retval true An error occurred
    @retval false Success
*/
bool Persisted_variables_cache::set_persisted_options(
    bool plugin_options, const char *target_var_name,
    int target_var_name_length) {
  THD *thd;
  bool result = false, new_thd = false;
  const std::vector<std::string> priv_list = {
      "ENCRYPTION_KEY_ADMIN", "ROLE_ADMIN",          "SYSTEM_VARIABLES_ADMIN",
      "AUDIT_ADMIN",          "TELEMETRY_LOG_ADMIN", "CONNECTION_ADMIN"};
  const ulong static_priv_list = (SUPER_ACL | FILE_ACL);
  Sctx_ptr<Security_context> ctx;
  /*
    if persisted_globals_load is set to false or --no-defaults is set
    then do not set persistent options
  */
  if (no_defaults || !persisted_globals_load) return false;
  /*
    This function is called in 3 places
      1. During server startup, see mysqld_main()
      2. During install plugin after server has started,
         see update_persisted_plugin_sysvars()
      3. During component installation after server has started,
         see mysql_component_sys_variable_imp::register_variable

    During server startup before server components are initialized
    current_thd is NULL thus instantiate new temporary THD.
    After server has started we have current_thd so make use of current_thd.
  */
  if (current_thd) {
    thd = current_thd;
  } else {
    if (!(thd = new THD)) {
      LogErr(ERROR_LEVEL, ER_FAILED_TO_SET_PERSISTED_OPTIONS);
      return true;
    }
    thd->thread_stack = (char *)&thd;
    thd->set_new_thread_id();
    thd->store_globals();
    lex_start(thd);
    /* create security context for bootstrap auth id */
    Security_context_factory default_factory(
        thd, "bootstrap", "localhost", Default_local_authid(thd),
        Grant_temporary_dynamic_privileges(thd, priv_list),
        Grant_temporary_static_privileges(thd, static_priv_list),
        Drop_temporary_dynamic_privileges(priv_list));
    ctx = default_factory.create();
    /* attach this auth id to current security_context */
    thd->set_security_context(ctx.get());
    thd->real_id = my_thread_self();
#ifndef NDEBUG
    thd->for_debug_only_is_set_persist_options = true;
#endif
    new_thd = true;
    alloc_and_copy_thd_dynamic_variables(thd, !plugin_options);
  }
  if (plugin_options) {
    assert(!new_thd);
  }
  /*
   locking is not needed as this function is executed only during server
   bootstrap, but we take the lock to be on safer side.
  */
  lock();
  assert_lock_owner();
  /*
    Based on plugin_options, we decide on what options to be set. If
    plugin_options is false we set all non plugin variables and then
    keep all plugin variables in a map. When the plugin is installed
    plugin variables are read from the map and set.
  */
  auto &persist_variables =
      (plugin_options ? m_persisted_dynamic_plugin_variables
                      : m_persisted_dynamic_variables);
  auto &persist_sensitive_variables =
      (plugin_options ? m_persisted_dynamic_sensitive_plugin_variables
                      : m_persisted_dynamic_sensitive_variables);

  /* create a sorted set of values sorted by timestamp */
  std::multiset<st_persist_var, sort_tv_by_timestamp> sorted_vars;

  /*
    if a target variable is specified try to find and set only the variable
    and not every value in the persist file
  */
  if (target_var_name != nullptr && target_var_name_length > 0 &&
      *target_var_name != 0) {
    auto it = std::find_if(
        persist_variables.begin(), persist_variables.end(),
        [target_var_name, target_var_name_length](st_persist_var const &s) {
          return !strncmp(s.key.c_str(), target_var_name,
                          target_var_name_length);
        });
    if (it != persist_variables.end()) sorted_vars.insert(*it);
    auto sensitive_it = std::find_if(
        persist_sensitive_variables.begin(), persist_sensitive_variables.end(),
        [target_var_name, target_var_name_length](st_persist_var const &s) {
          return !strncmp(s.key.c_str(), target_var_name,
                          target_var_name_length);
        });
    if (sensitive_it != persist_sensitive_variables.end())
      sorted_vars.insert(*sensitive_it);
  } else {
    sorted_vars.insert(persist_variables.begin(), persist_variables.end());
    sorted_vars.insert(persist_sensitive_variables.begin(),
                       persist_sensitive_variables.end());
  }

  for (const st_persist_var &iter : sorted_vars) {
    const std::string &var_name = iter.key;
    auto f = [this, thd, &iter, &var_name, plugin_options](
                 const System_variable_tracker &var_tracker,
                 sys_var *sysvar) -> bool {
      Item *res = nullptr;
      /*
        For aliases with the m_is_persisted_deprecated flag set, the
      non-alias has its own entry in m_persisted_dynamic_variables.  Therefore,
        we rely on setting the value for the non-alias and skip setting
        the value for the alias.

        It would be harmless to set the value also for the alias, except
        it would generate an extra deprecation warning.  The correct
        deprecation warning was already generated, if needed, in the
        previous call to load_aliases().
      */
      if (!(get_variable_alias(sysvar) && sysvar->m_is_persisted_deprecated)) {
        switch (sysvar->show_type()) {
          case SHOW_INT:
          case SHOW_LONG:
          case SHOW_LONGLONG:
          case SHOW_HA_ROWS:
            res = new (thd->mem_root)
                Item_uint(iter.value.c_str(), (uint)iter.value.length());
            break;
          case SHOW_SIGNED_INT:
          case SHOW_SIGNED_LONG:
          case SHOW_SIGNED_LONGLONG:
            res = new (thd->mem_root)
                Item_int(iter.value.c_str(), (uint)iter.value.length());
            break;
          case SHOW_CHAR:
          case SHOW_LEX_STRING:
          case SHOW_BOOL:
          case SHOW_MY_BOOL:
            res = new (thd->mem_root)
                Item_string(iter.value.c_str(), iter.value.length(),
                            &my_charset_utf8mb4_bin);
            break;
          case SHOW_CHAR_PTR:
            if (iter.is_null)
              res = new (thd->mem_root) Item_null();
            else
              res = new (thd->mem_root)
                  Item_string(iter.value.c_str(), iter.value.length(),
                              &my_charset_utf8mb4_bin);
            break;
          case SHOW_DOUBLE:
            res = new (thd->mem_root)
                Item_float(iter.value.c_str(), (uint)iter.value.length());
            break;
          default:
            my_error(ER_UNKNOWN_SYSTEM_VARIABLE, MYF(0), sysvar->name.str);
            return true;
        }

        set_var *var =
            new (thd->mem_root) set_var(OPT_GLOBAL, var_tracker, res);
        List<set_var_base> tmp_var_list;
        tmp_var_list.push_back(var);
        LEX *saved_lex = thd->lex, lex_tmp;
        thd->lex = &lex_tmp;
        lex_start(thd);
        if (sql_set_variables(thd, &tmp_var_list, false)) {
          thd->lex = saved_lex;
          /*
           If there is a connection and an error occurred during install
           plugin then report error at sql layer, else log the error in
           server log.
          */
          if (current_thd && plugin_options) {
            if (thd->is_error())
              LogErr(ERROR_LEVEL, ER_PERSIST_OPTION_STATUS,
                     thd->get_stmt_da()->message_text());
            else
              my_error(ER_CANT_SET_PERSISTED, MYF(0));
          } else {
            if (thd->is_error())
              LogErr(ERROR_LEVEL, ER_PERSIST_OPTION_STATUS,
                     thd->get_stmt_da()->message_text());
            else
              LogErr(ERROR_LEVEL, ER_FAILED_TO_SET_PERSISTED_OPTIONS);
          }
          return true;
        }
        thd->lex = saved_lex;
      }
      /*
        Once persisted variables are SET in the server,
      update variables source/user/timestamp/host from
      m_persisted_dynamic_variables.
      */
      auto set_source = [&](auto &variables) -> bool {
        auto it = variables.find(iter);
        if (it != variables.end()) {
          /* persisted variable is found */
          sysvar->set_source(enum_variable_source::PERSISTED);
#ifndef NDEBUG
          bool source_truncated =
#endif
              sysvar->set_source_name(m_persist_filename.c_str());
          assert(!source_truncated);
          sysvar->set_timestamp(it->timestamp);
          if (sysvar->set_user(it->user.c_str()))
            LogErr(WARNING_LEVEL, ER_PERSIST_OPTION_USER_TRUNCATED,
                   var_name.c_str());
          if (sysvar->set_host(it->host.c_str()))
            LogErr(WARNING_LEVEL, ER_PERSIST_OPTION_HOST_TRUNCATED,
                   var_name.c_str());
          return false;
        }
        return true;
      };

      if (set_source(m_persisted_dynamic_variables))
        (void)set_source(m_persisted_dynamic_sensitive_variables);

      /*
        We need to keep the currently set persisted variable into the in-memory
        copy for plugin vars for further UNINSTALL followed by INSTALL sans
        restart.
      */
      if (sysvar->cast_pluginvar() && !plugin_options) {
        auto &plugin_vars =
            m_persisted_dynamic_sensitive_plugin_variables.find(iter) ==
                    m_persisted_dynamic_sensitive_plugin_variables.end()
                ? m_persisted_dynamic_plugin_variables
                : m_persisted_dynamic_sensitive_plugin_variables;
#ifndef NDEBUG
        auto ret =
#endif
            plugin_vars.insert(iter);
        // the value should not be present in the plugins copy
        assert(ret.second);
      }

      return false;
    };
    /*
      There are currently 4 groups of code paths to call the current
      function Persisted_variables_cache::set_persisted_options():

      1. Directly from mysqld_main().

         There is only one thread ATM (current_thd == nullptr), so locks aren't
         necessary and we suppress them using the Is_single_thread::YES flag.

      2. Indirectly from mysqld_main():
           mysqld_main() ->
             init_server_components() ->
               plugin_register_builtin_and_init_core_se() ->
                 update_persisted_plugin_sysvars()

         Ditto: Is_single_thread::YES.

      3. Indirectly from mysql_install_plugin():
           mysql_install_plugin() ->
             update_persisted_plugin_sysvars()

         sql_plugin.cc always acquire LOCK_system_variables_hash and
         LOCK_plugin for us before calling method,
         so, we suppress double locks with Is_already_locked::YES.

      4. Directly from mysql_component_sys_variable_imp::register_variable().

         mysql_component_sys_variable_imp::register_variable() holds
         LOCK_plugin and LOCK_system_variables_hash for us, so,
         we suppress double locks with Is_already_locked::YES.

       Note: Is_single_thread::YES implies Is_already_locked::YES,
       so, Is_already_locked is "YES" here unconditionally.
    */
    std::optional<bool> sv_status =
        System_variable_tracker::make_tracker(var_name)
            .access_system_variable<bool>(
                thd, f, Suppress_not_found_error::YES,
                Force_sensitive_system_variable_access::YES,
                Is_already_locked::YES,
                new_thd ? Is_single_thread::YES : Is_single_thread::NO);

    if (!sv_status.has_value()) {  // not found
      /*
        for dynamically-registered variables we report a warning in error log,
        keep track of this variable so that it is set when plugin
        is loaded and continue with remaining persisted variables
      */
      if (m_persisted_dynamic_plugin_variables.insert(iter).second)
        LogErr(WARNING_LEVEL, ER_UNKNOWN_VARIABLE_IN_PERSISTED_CONFIG_FILE,
               var_name.c_str());
      continue;
    } else if (sv_status.value()) {
      break;  // fatal error
    }
  }

  /*
     If the function is called at start-up time, set
     source information for PARSE_EARLY variables
  */
  if (!plugin_options) set_parse_early_sources();

  if (new_thd) {
    /* check for warnings in DA */
    Diagnostics_area::Sql_condition_iterator it =
        thd->get_stmt_da()->sql_conditions();
    const Sql_condition *err = nullptr;
    while ((err = it++)) {
      if (err->severity() == Sql_condition::SL_WARNING) {
        // Rewrite error number for "deprecated" to error log equivalent.
        if (err->mysql_errno() == ER_WARN_DEPRECATED_SYNTAX)
          LogEvent()
              .type(LOG_TYPE_ERROR)
              .prio(WARNING_LEVEL)
              .errcode(ER_SERVER_WARN_DEPRECATED)
              .verbatim(err->message_text());
        /*
          Any other (unexpected) message is wrapped to preserve its
          original error number, and to explain the issue.
          This is a failsafe; "expected", that is to say, common
          messages should be handled explicitly like the deprecation
          warning above.
        */
        else
          LogErr(WARNING_LEVEL, ER_ERROR_INFO_FROM_DA, err->mysql_errno(),
                 err->message_text());
      }
    }
    thd->free_items();
    lex_end(thd->lex);
    thd->release_resources();
    ctx.reset(nullptr);
    delete thd;
  }
  unlock();
  return result;
}

/**
  extract_variables_from_json() is used to extract all the variable information
  which is in the form of Json_object.

  New format for mysqld-auto.cnf is as below:
  {
    "mysql_static_parse_early_variables": {
      "variable_name_1" : {
        "Value" : "variable_value",
        "Metadata" : {
          "Timestamp" : timestamp_value,
          "User" : "user_name",
          "Host" : "host_name"
        }
      },
      "variable_name_2" : {
        "Value" : "variable_value",
        "Metadata" : {
          "Timestamp" : timestamp_value,
          "User" : "user_name",
          "Host" : "host_name"
        }
      },
      ...
      ...
      "variable_name_n" : {
        "Value" : "variable_value",
        "Metadata" : {
          "Timestamp" : timestamp_value,
          "User" : "user_name",
          "Host" : "host_name"
        }
      }
    },
    "mysql_dynamic_parse_early_variables": {
      "variable_name_1" : {
        "Value" : "variable_value",
        "Metadata" : {
          "Timestamp" : timestamp_value,
          "User" : "user_name",
          "Host" : "host_name"
        }
      },
      "variable_name_2" : {
        "Value" : "variable_value",
        "Metadata" : {
          "Timestamp" : timestamp_value,
          "User" : "user_name",
          "Host" : "host_name"
        }
      },
      ...
      ...
      "variable_name_n" : {
        "Value" : "variable_value",
        "Metadata" : {
          "Timestamp" : timestamp_value,
          "User" : "user_name",
          "Host" : "host_name"
        }
      }
    },
    "mysql_static_variables": {
      "variable_name_1" : {
        "Value" : "variable_value",
        "Metadata" : {
          "Timestamp" : timestamp_value,
          "User" : "user_name",
          "Host" : "host_name"
        }
      },
      "variable_name_2" : {
        "Value" : "variable_value",
        "Metadata" : {
          "Timestamp" : timestamp_value,
          "User" : "user_name",
          "Host" : "host_name"
        }
      },
      ...
      ...
      "variable_name_n" : {
        "Value" : "variable_value",
        "Metadata" : {
          "Timestamp" : timestamp_value,
          "User" : "user_name",
          "Host" : "host_name"
        }
      }
    },
    "mysql_dynamic_variables": {
      "variable_name_1" : {
        "Value" : "variable_value",
        "Metadata" : {
          "Timestamp" : timestamp_value,
          "User" : "user_name",
          "Host" : "host_name"
        }
      },
      "variable_name_2" : {
        "Value" : "variable_value",
        "Metadata" : {
          "Timestamp" : timestamp_value,
          "User" : "user_name",
          "Host" : "host_name"
        }
      },
      ...
      ...
      "variable_name_n" : {
        "Value" : "variable_value",
        "Metadata" : {
          "Timestamp" : timestamp_value,
          "User" : "user_name",
          "Host" : "host_name"
        }
      }
    },
    "mysql_sensitive_variables" : {
      "master_key": "<master_key_name>",
      "mysql_file_key": "<ENCRYPTED_FILE_KEY_IN_HEX>",
      "mysql_file_key_iv": "<IV_IN_HEX>",
      "key_encryption_algorithm": "AES_256_CBC",
      "mysql_sensitive_variables_blob":
  "<SENSITIVE_VARIABLES_INFO_IN_ENCRYPTED_FORM_IN_HEX>",
      "mysql_sensitive_variables_iv": "<IV_IN_HEX>",
      "data_encryption_algorithm": "AES_256_CBC"
    }
  }

  @param [in] dom             Pointer to the Json_dom object which is an
  internal representation of parsed json string
  @param [in] is_read_only    Bool value when set to TRUE extracts read only
                              variables and dynamic variables when set to FALSE.

  @return 0 Success
  @return 1 Failure
*/
bool Persisted_variables_cache::extract_variables_from_json(const Json_dom *dom,
                                                            bool is_read_only) {
  if (dom->json_type() != enum_json_type::J_OBJECT) goto err;
  for (auto &var_iter : *down_cast<const Json_object *>(dom)) {
    string var_value, var_user, var_host;
    ulonglong timestamp = 0;
    bool is_null = false;

    const string &var_name = var_iter.first;
    if (var_iter.second->json_type() != enum_json_type::J_OBJECT) goto err;
    const Json_object *dom_obj =
        down_cast<const Json_object *>(var_iter.second.get());

    /**
      Static variables by themselves is represented as a json object with key
      "mysql_server_static_options" as parent element.
    */
    if (var_name == "mysql_server_static_options") {
      if (extract_variables_from_json(dom_obj, true)) return true;
      continue;
    }

    /**
      Every Json object which represents Variable information must have only
      2 elements which is
      {
      "Value" : "variable_value",   -- 1st element
      "Metadata" : {                -- 2nd element
        "Timestamp" : timestamp_value,
        "User" : "user_name",
        "Host" : "host_name"
        }
      }
    */
    if (dom_obj->depth() != 3 && dom_obj->cardinality() != 2) goto err;

    Json_object::const_iterator var_properties_iter = dom_obj->begin();
    /* extract variable value */
    if (var_properties_iter->first != "Value") goto err;

    const Json_dom *value = var_properties_iter->second.get();
    /* if value is not in string form or null throw error. */
    if (value->json_type() == enum_json_type::J_STRING) {
      var_value = down_cast<const Json_string *>(value)->value();
    } else if (value->json_type() == enum_json_type::J_NULL) {
      var_value = "";
      is_null = true;
    } else {
      goto err;
    }

    ++var_properties_iter;
    /* extract metadata */
    if (var_properties_iter->first != "Metadata") goto err;

    if (var_properties_iter->second->json_type() != enum_json_type::J_OBJECT)
      goto err;
    dom_obj = down_cast<const Json_object *>(var_properties_iter->second.get());
    if (dom_obj->depth() != 1 && dom_obj->cardinality() != 3) goto err;

    for (auto &metadata_iter : *dom_obj) {
      const string &metadata_type = metadata_iter.first;
      const Json_dom *metadata_value = metadata_iter.second.get();
      if (metadata_type == "Timestamp") {
        if (metadata_value->json_type() != enum_json_type::J_UINT) goto err;
        const Json_uint *i = down_cast<const Json_uint *>(metadata_value);
        timestamp = i->value();
      } else if (metadata_type == "User" || metadata_type == "Host") {
        if (metadata_value->json_type() != enum_json_type::J_STRING) goto err;
        const Json_string *i = down_cast<const Json_string *>(metadata_value);
        if (metadata_type == "User")
          var_user = i->value();
        else
          var_host = i->value();
      } else {
        goto err;
      }
    }
    st_persist_var persist_var(var_name, var_value, timestamp, var_user,
                               var_host, is_null);
    lock();
    assert_lock_owner();
    if (is_read_only) {
      /**
        If we are reading from a v1 persisted file, all static options
        including PARSE_EARLY options are stored in array
        "mysql_server_static_options". Since there is no way to identify
        PARSE_EARLY static variables, we push all these variables to
        m_persisted_static_parse_early_variables.
      */
      m_persisted_static_parse_early_variables[var_name] = persist_var;
    } else {
      m_persisted_dynamic_variables.insert(persist_var);
    }
    unlock();
  }
  return false;

err:
  LogErr(ERROR_LEVEL, ER_JSON_PARSE_ERROR);
  return true;
}

int Persisted_variables_cache::read_persist_file_v1(
    const Json_object *json_object) {
  Json_dom *options_dom = json_object->get("mysql_server");
  if (options_dom == nullptr) {
    LogErr(ERROR_LEVEL, ER_CONFIG_OPTION_WITHOUT_GROUP);
    return 1;
  }
  /* Extract key/value pair and populate in a global hash map */
  if (extract_variables_from_json(options_dom)) return 1;
  return 0;
}

static bool extract_variable_value_and_metadata(const Json_object *json_object,
                                                st_persist_var &output) {
  if (json_object->json_type() != enum_json_type::J_OBJECT) return true;

  Json_dom *dom = nullptr;
  Json_string *value_string = nullptr;
  Json_uint *value_uint = nullptr;

  dom = json_object->get(s_key_value);
  if (dom == nullptr || dom->json_type() != enum_json_type::J_STRING)
    return true;
  value_string = down_cast<Json_string *>(dom);
  output.value.assign(value_string->value());
  output.is_null = (output.value.length() == 0);

  dom = json_object->get(s_key_metadata);
  if (dom == nullptr || dom->json_type() != enum_json_type::J_OBJECT)
    return true;
  Json_object *object_metadata = down_cast<Json_object *>(dom);

  dom = object_metadata->get(s_key_timestamp);
  if (dom == nullptr || dom->json_type() != enum_json_type::J_UINT) return true;
  value_uint = down_cast<Json_uint *>(dom);
  output.timestamp = value_uint->value();

  dom = object_metadata->get(s_key_user);
  if (dom == nullptr || dom->json_type() != enum_json_type::J_STRING)
    return true;
  value_string = down_cast<Json_string *>(dom);
  output.user.assign(value_string->value());

  dom = object_metadata->get(s_key_host);
  if (dom == nullptr || dom->json_type() != enum_json_type::J_STRING)
    return true;
  value_string = down_cast<Json_string *>(dom);
  output.host.assign(value_string->value());

  return false;
}

static bool extract_set(const Json_object &json_object,
                        const std::string set_key,
                        Persisted_variables_uset &output) {
  if (json_object.json_type() != enum_json_type::J_OBJECT) return true;
  Json_dom *vector_dom = json_object.get(set_key);
  /* Not having any entry is fine */
  if (vector_dom == nullptr) return false;
  if (vector_dom->json_type() != enum_json_type::J_OBJECT) return true;
  Json_object *section_object = down_cast<Json_object *>(vector_dom);
  for (auto &it : *section_object) {
    /* Ignore problematic entry */
    if (it.second == nullptr ||
        it.second->json_type() != enum_json_type::J_OBJECT)
      continue;
    const Json_object *element =
        down_cast<const Json_object *>(it.second.get());
    st_persist_var entry;
    entry.key = it.first;
    /* Ignore problematic entry */
    if (extract_variable_value_and_metadata(element, entry)) continue;
    output.insert(entry);
  }
  return false;
}

static bool extract_map(const Json_object &json_object,
                        const std::string map_key,
                        Persisted_variables_umap &output) {
  if (json_object.json_type() != enum_json_type::J_OBJECT) return true;
  Json_dom *map_dom = json_object.get(map_key);
  /* Not having any entry is fine */
  if (map_dom == nullptr) return false;
  if (map_dom->json_type() != enum_json_type::J_OBJECT) return true;
  Json_object *section_object = down_cast<Json_object *>(map_dom);
  for (auto &it : *section_object) {
    /* Ignore problematic entry */
    if (it.second == nullptr ||
        it.second->json_type() != enum_json_type::J_OBJECT)
      continue;
    const Json_object *element =
        down_cast<const Json_object *>(it.second.get());
    st_persist_var entry;
    entry.key = it.first;
    /* Ignore problematic entry */
    if (extract_variable_value_and_metadata(element, entry)) continue;
    output[entry.key] = entry;
  }
  return false;
}

static bool extract_string(Json_object *obj, const std::string &key,
                           std::string &value) {
  if (obj != nullptr) {
    Json_dom *json_dom = obj->get(key);
    if (json_dom != nullptr) {
      if (json_dom->json_type() != enum_json_type::J_STRING) return true;
      Json_string *string_dom = down_cast<Json_string *>(json_dom);
      value = string_dom->value();
    }
  }
  return false;
}

int Persisted_variables_cache::read_persist_file_v2(
    const Json_object *main_json_object) {
  auto process_sensitive_section =
      [](const Json_object &json_object, std::string &master_key_id,
         std::string &file_key, std::string &file_key_iv,
         std::string &sensitive_variables_blob, std::string &iv,
         Json_object **sensitive_variables_section) -> bool {
    sensitive_variables_blob.clear();
    iv.clear();
    if (json_object.json_type() != enum_json_type::J_OBJECT) return true;
    Json_dom *section_dom = json_object.get(s_key_mysql_sensitive_variables);
    if (section_dom == nullptr) return false;
    if (section_dom->json_type() != enum_json_type::J_OBJECT) return true;
    *sensitive_variables_section = down_cast<Json_object *>(section_dom);

    if (extract_string((*sensitive_variables_section), s_key_master_key_id,
                       master_key_id))
      return true;

    if (extract_string((*sensitive_variables_section), s_key_file_key,
                       file_key))
      return true;
    if (extract_string((*sensitive_variables_section), s_key_file_key_iv,
                       file_key_iv))
      return true;

    string encryption_mode{};
    if (extract_string((*sensitive_variables_section),
                       s_key_key_encryption_algorithm, encryption_mode))
      return true;

    std::transform(encryption_mode.begin(), encryption_mode.end(),
                   encryption_mode.begin(),
                   [](unsigned char c) { return std::toupper(c); });

    if (encryption_mode != s_key_mysql_encryption_algorithm_default)
      return true;

    encryption_mode.clear();

    if (extract_string((*sensitive_variables_section),
                       s_key_data_encryption_algorithm, encryption_mode))
      return true;

    std::transform(encryption_mode.begin(), encryption_mode.end(),
                   encryption_mode.begin(),
                   [](unsigned char c) { return std::toupper(c); });

    if (encryption_mode != s_key_mysql_encryption_algorithm_default)
      return true;

    if (extract_string((*sensitive_variables_section),
                       s_key_mysql_sensitive_variables_blob,
                       sensitive_variables_blob))
      return true;

    if (extract_string((*sensitive_variables_section),
                       s_key_mysql_sensitive_variables_iv, iv))
      return true;
    return false;
  };

  lock();
  assert_lock_owner();

  if (extract_set(*main_json_object, s_key_mysql_dynamic_variables,
                  m_persisted_dynamic_variables) ||
      extract_set(*main_json_object, s_key_mysql_dynamic_parse_early_variables,
                  m_persisted_dynamic_parse_early_variables) ||
      extract_map(*main_json_object, s_key_mysql_static_variables,
                  m_persisted_static_variables) ||
      extract_map(*main_json_object, s_key_mysql_static_parse_early_variables,
                  m_persisted_static_parse_early_variables)) {
    unlock();
    LogErr(ERROR_LEVEL, ER_JSON_PARSE_ERROR);
    return 1;
  }

  Json_object *sensitive_variables_section = nullptr;

  if (process_sensitive_section(*main_json_object, m_key_info.m_master_key_name,
                                m_key_info.m_file_key, m_key_info.m_file_key_iv,
                                m_sensitive_variables_blob, m_iv,
                                &sensitive_variables_section)) {
    unlock();
    LogErr(ERROR_LEVEL, ER_JSON_PARSE_ERROR);
    return 1;
  }

  if (sensitive_variables_section != nullptr) {
    if (extract_set(*sensitive_variables_section,
                    s_key_mysql_sensitive_dynamic_variables,
                    m_persisted_dynamic_sensitive_variables) ||
        extract_map(*sensitive_variables_section,
                    s_key_mysql_sensitive_static_variables,
                    m_persisted_static_sensitive_variables)) {
      unlock();
      LogErr(ERROR_LEVEL, ER_JSON_PARSE_ERROR);
      return 1;
    }
  }
  unlock();

  return 0;
}

void Persisted_variables_cache::load_aliases() {
  // Store deprecation warnings in a set, so that we can report them
  // in alphabetic order. This makes test cases more deterministic.
  std::map<std::string, std::string> deprecated;

  std::unordered_set<st_persist_var, st_persist_var_hash> var_set;
  for (auto &var : m_persisted_dynamic_variables) var_set.insert(var);

  /*
    If variable has an alias, and it does not exist in the container,
    insert the alias in container.

    This lambda is agnostic to container type, taking arguments that
    are functions that check for existing elements and insert
    elements.
  */
  auto insert_alias =
      [&](std::function<bool(const char *)> exists,
          std::function<void(st_persist_var &)> insert_in_container,
          st_persist_var &var) {
        auto *sysvar = intern_find_sys_var(var.key.c_str(), var.key.length());
        if (sysvar) {
          const char *alias = get_variable_alias(sysvar);
          if (alias) {
            if (!exists(alias)) {
              st_persist_var alias_var{var};
              alias_var.key = alias;
              insert_in_container(alias_var);
              if (sysvar->m_is_persisted_deprecated)
                deprecated[alias] = var.key;
            }
          }
        }
      };
  lock();
  if (current_thd != nullptr) mysql_mutex_assert_not_owner(&LOCK_plugin);
  mysql_rwlock_rdlock(&LOCK_system_variables_hash);

  for (auto iter : var_set) {
    insert_alias(
        [&](const char *name) -> bool {
          auto it = std::find_if(var_set.begin(), var_set.end(),
                                 [name](st_persist_var const &s) {
                                   return !strcmp(s.key.c_str(), name);
                                 });
          return it != var_set.end();
        },
        [&](st_persist_var &v) { m_persisted_dynamic_variables.insert(v); },
        iter);
  }

  var_set.clear();
  for (auto &var : m_persisted_dynamic_sensitive_variables) var_set.insert(var);
  for (auto iter : var_set) {
    insert_alias(
        [&](const char *name) -> bool {
          auto it = std::find_if(var_set.begin(), var_set.end(),
                                 [name](st_persist_var const &s) {
                                   return !strcmp(s.key.c_str(), name);
                                 });
          return it != var_set.end();
        },
        [&](st_persist_var &v) {
          m_persisted_dynamic_sensitive_variables.insert(v);
        },
        iter);
  }

  for (auto pair : m_persisted_static_variables) {
    insert_alias(
        [&](const char *name) -> bool {
          return m_persisted_static_variables.find(name) !=
                 m_persisted_static_variables.end();
        },
        [&](st_persist_var &v) { m_persisted_static_variables[v.key] = v; },
        pair.second);
  }

  for (auto pair : m_persisted_static_sensitive_variables) {
    insert_alias(
        [&](const char *name) -> bool {
          return m_persisted_static_sensitive_variables.find(name) !=
                 m_persisted_static_sensitive_variables.end();
        },
        [&](st_persist_var &v) {
          m_persisted_static_sensitive_variables[v.key] = v;
        },
        pair.second);
  }
  mysql_rwlock_unlock(&LOCK_system_variables_hash);
  unlock();

  // Generate deprecation warnings
  for (auto pair : deprecated)
    LogErr(WARNING_LEVEL, ER_DEPRECATED_PERSISTED_VARIABLE_WITH_ALIAS,
           pair.second.c_str(), pair.first.c_str());
}

/**
  read_persist_file() reads the persisted config file

  This function does following:
    1. Read the persisted config file into a string buffer
    2. This string buffer is parsed with JSON parser to check
       if the format is correct or not.
    3. Check for correct group name.
    4. Extract key/value pair and populate in m_persisted_dynamic_variables,
       m_persisted_static_variables.
  mysqld-auto.cnf file will have variable properties like when a
  variable is set, by wholm and on what host this variable was set.

  @return Error state
    @retval -1 or 1 Failure
    @retval 0 Success
*/
int Persisted_variables_cache::read_persist_file() {
  Json_dom_ptr json;
  if ((check_file_permissions(m_persist_filename.c_str(), false)) < 2)
    return -1;

  auto read_file = [&]() -> bool {
    string parsed_value;
    char buff[4096] = {0};
    do {
      /* Read the persisted config file into a string buffer */
      parsed_value.append(buff);
      buff[0] = '\0';
    } while (mysql_file_fgets(buff, sizeof(buff) - 1, m_fd));
    close_persist_file();
    /* parse the file contents to check if it is in json format or not */
    json = Json_dom::parse(
        parsed_value.c_str(), parsed_value.length(),
        [](const char *, size_t) {}, JsonDocumentDefaultDepthHandler);
    if (!json.get()) return true;
    return false;
  };

  if (!(open_persist_backup_file(O_RDONLY) == false && read_file() == false)) {
    /*
      if opening or reading of backup file failed, delete backup file
      and read original file
    */
    my_delete(m_persist_backup_filename.c_str(), MYF(0));
    if (open_persist_file(O_RDONLY)) return -1;
    if (read_file()) {
      LogErr(ERROR_LEVEL, ER_JSON_PARSE_ERROR);
      return 1;
    }
  } else {
    /* backup file was read successfully, thus rename it to original. */
    my_rename(m_persist_backup_filename.c_str(), m_persist_filename.c_str(),
              MYF(MY_WME));
  }
  Json_object *json_obj = down_cast<Json_object *>(json.get());
  /* Check file version */
  Json_dom *version_dom = json_obj->get("Version");
  if (version_dom == nullptr) {
    LogErr(ERROR_LEVEL, ER_PERSIST_OPTION_STATUS,
           "Persisted config file corrupted.");
    return 1;
  }
  if (version_dom->json_type() != enum_json_type::J_INT) {
    LogErr(ERROR_LEVEL, ER_PERSIST_OPTION_STATUS,
           "Persisted config file version invalid.");
    return 1;
  }
  Json_int *fetched_version = down_cast<Json_int *>(version_dom);

  int retval = 1;
  switch (static_cast<File_version>(fetched_version->value())) {
    case File_version::VERSION_V1:
      retval = read_persist_file_v1(json_obj);
      break;
    case File_version::VERSION_V2:
      retval = read_persist_file_v2(json_obj);
      break;
    default:
      retval = 1;
      LogErr(ERROR_LEVEL, ER_PERSIST_OPTION_STATUS,
             "Persisted config file version invalid.");
      break;
  };

  if (!retval) load_aliases();
  return retval;
}

/**
  append_parse_early_variables() does a lookup into persist_variables
  for read only variables and place them after the command line options with a
  separator "----persist-args-separator----"

  This function does nothing when --no-defaults is set or if
  persisted_globals_load is disabled.

  @param [in]  argc                      Pointer to argc of original program
  @param [in]  argv                      Pointer to argv of original program
  @param [out] arg_separator_added       Whether the separator is added or not

  @return 0 Success
  @return 1 Failure
*/
bool Persisted_variables_cache::append_parse_early_variables(
    int *argc, char ***argv, bool &arg_separator_added) {
  Prealloced_array<char *, 100> my_args(key_memory_persisted_variables);
  MEM_ROOT alloc(key_memory_persisted_variables, 512);
  arg_separator_added = false;

  if (*argc < 2 || no_defaults || !persisted_globals_load) return false;

  /* create a set of values sorted by timestamp */
  std::multiset<st_persist_var, sort_tv_by_timestamp> sorted_vars;
  for (auto iter : m_persisted_static_parse_early_variables)
    sorted_vars.insert(iter.second);
  for (auto iter : m_persisted_dynamic_parse_early_variables)
    sorted_vars.insert(iter);

  for (auto iter : sorted_vars) {
    string persist_option = "--loose_" + iter.key + "=" + iter.value;
    char *tmp;

    if (nullptr == (tmp = strdup_root(&alloc, persist_option.c_str())) ||
        my_args.push_back(tmp))
      return true;
  }
  /*
   Update existing command line options if there are any persisted
   reasd only options to be appendded
  */
  if (my_args.size()) {
    char **res = new (&alloc) char *[my_args.size() + *argc + 2];
    if (res == nullptr) goto err;
    memset(res, 0, (sizeof(char *) * (my_args.size() + *argc + 2)));
    /* copy all arguments to new array */
    memcpy((uchar *)(res), (char *)(*argv), (*argc) * sizeof(char *));

    if (!my_args.empty()) {
      /*
       Set args separator to know options set as part of command line and
       options set from persisted config file
      */
      set_persist_args_separator(&res[*argc]);
      arg_separator_added = true;
      /* copy arguments from persistent config file */
      memcpy((res + *argc + 1), &my_args[0], my_args.size() * sizeof(char *));
    }
    res[my_args.size() + *argc + 1] = nullptr; /* last null */
    (*argc) += (int)my_args.size() + 1;
    *argv = res;
    parse_early_persisted_argv_alloc = std::move(alloc);
    return false;
  }
  return false;

err:
  LogErr(ERROR_LEVEL, ER_FAILED_TO_HANDLE_DEFAULTS_FILE);
  exit(1);
}

/**
  append_read_only_variables() does a lookup into persist_variables for read
  only variables and place them after the command line options with a separator
  "----persist-args-separator----"

  This function does nothing when --no-defaults is set or if
  persisted_globals_load is disabled.

  @param [in] argc                      Pointer to argc of original program
  @param [in] argv                      Pointer to argv of original program
  @param [in] arg_separator_added       This flag tells whether arg separator
                                        has already been added or not
  @param [in] plugin_options            This flag tells whether options are
                                        handled during plugin install.
                                        If set to true options are handled
                                        as part of
  @param [in] root                      The memory root to use for the
                                        allocations. Null if you want to use
                                        the PV cache root(s). install plugin.

  @return 0 Success
  @return 1 Failure
*/
bool Persisted_variables_cache::append_read_only_variables(
    int *argc, char ***argv, bool arg_separator_added /* = false */,
    bool plugin_options /* = false */, MEM_ROOT *root /* = nullptr */) {
  Prealloced_array<char *, 100> my_args(key_memory_persisted_variables);
  MEM_ROOT local_alloc{key_memory_persisted_variables, 512};
  MEM_ROOT &alloc = root ? *root : local_alloc;

  if (plugin_options == false) keyring_support_available();

  if (*argc < 2 || no_defaults || !persisted_globals_load) return false;

  auto result = decrypt_sensitive_variables();
  if (result == return_status::ERROR) {
    LogErr(ERROR_LEVEL, ER_CANNOT_INTERPRET_PERSISTED_SENSITIVE_VARIABLES);
    return true;
  }

  /* create a set of values sorted by timestamp */
  std::multiset<st_persist_var, sort_tv_by_timestamp> sorted_vars;
  for (auto iter : m_persisted_static_variables)
    sorted_vars.insert(iter.second);
  for (auto iter : m_persisted_static_sensitive_variables)
    sorted_vars.insert(iter.second);

  for (auto iter : sorted_vars) {
    string persist_option = "--loose_" + iter.key + "=" + iter.value;
    char *tmp;

    if (nullptr == (tmp = strdup_root(&alloc, persist_option.c_str())) ||
        my_args.push_back(tmp))
      return true;
  }
  /*
   Update existing command line options if there are any persisted
   reasd only options to be appendded
  */
  if (my_args.size()) {
    unsigned int extra_args = (arg_separator_added == false) ? 2 : 1;
    char **res = new (&alloc) char *[my_args.size() + *argc + extra_args];
    if (res == nullptr) goto err;
    memset(res, 0, (sizeof(char *) * (my_args.size() + *argc + extra_args)));
    /* copy all arguments to new array */
    memcpy((uchar *)(res), (char *)(*argv), (*argc) * sizeof(char *));

    if (!my_args.empty()) {
      if (arg_separator_added == false) {
        /*
         Set args separator to know options set as part of command line and
         options set from persisted config file
        */
        set_persist_args_separator(&res[*argc]);
      }
      /* copy arguments from persistent config file */
      memcpy((res + *argc + (extra_args - 1)), &my_args[0],
             my_args.size() * sizeof(char *));
    }
    res[my_args.size() + *argc + (extra_args - 1)] = nullptr; /* last null */
    (*argc) += (int)my_args.size() + (extra_args - 1);
    *argv = res;
    if (!root) {
      if (plugin_options)
        ro_persisted_plugin_argv_alloc =
            std::move(alloc);  // Possibly overwrite previous.
      else
        ro_persisted_argv_alloc = std::move(alloc);
    }
    return false;
  }
  return false;

err:
  LogErr(ERROR_LEVEL, ER_FAILED_TO_HANDLE_DEFAULTS_FILE);
  exit(1);
}

/**
  reset_persisted_variables() does a lookup into persist_variables and remove
  the variable from the hash if present and flush the hash to file.

  @param [in] thd                     Pointer to connection handle.
  @param [in] name                    Name of variable to remove, if NULL all
                                      variables are removed from config file.
  @param [in] if_exists               Bool value when set to true reports
                                      warning else error if variable is not
                                      present in the config file.

  @return 0 Success
  @return 1 Failure
*/
bool Persisted_variables_cache::reset_persisted_variables(THD *thd,
                                                          const char *name,
                                                          bool if_exists) {
  bool result = false, found = false;
  bool reset_all = (name ? 0 : 1);
  /* update on m_persisted_dynamic_variables/m_persisted_static_variables must
   * be thread safe */
  lock();

  if (reset_all) {
    /* check for necessary privileges */
    if ((!m_persisted_dynamic_variables.empty() ||
         !m_persisted_dynamic_parse_early_variables.empty() ||
         !m_persisted_dynamic_sensitive_variables.empty()) &&
        check_priv(thd, false))
      goto end;

    if ((!m_persisted_static_parse_early_variables.empty() ||
         !m_persisted_static_variables.empty() ||
         !m_persisted_static_sensitive_variables.empty()) &&
        check_priv(thd, true))
      goto end;

    auto clear_one = [&found](auto &variables) {
      if (!variables.empty()) {
        variables.clear();
        found = true;
      }
    };

    clear_one(m_persisted_dynamic_variables);
    clear_one(m_persisted_dynamic_parse_early_variables);
    clear_one(m_persisted_dynamic_sensitive_variables);
    clear_one(m_persisted_static_variables);
    clear_one(m_persisted_static_sensitive_variables);
    clear_one(m_persisted_static_parse_early_variables);
    clear_one(m_persisted_dynamic_plugin_variables);
    clear_one(m_persisted_static_sensitive_variables);
  } else {
    auto erase_variable = [&](const char *name_cptr) -> bool {
      string name_str{tolower_varname(name_cptr)};

      auto checkvariable = [&name_str](st_persist_var const &s) -> bool {
        return s.key == name_str;
      };

      auto update_unordered_set = [&thd, &found,
                                   &checkvariable](auto &variables) -> bool {
        if (variables.size()) {
          auto it =
              std::find_if(variables.begin(), variables.end(), checkvariable);
          if (it != variables.end()) {
            /* if variable is present in config file remove it */
            if (check_priv(thd, false)) return true;
            variables.erase(it);
            found = true;
          }
        }
        return false;
      };

      if (update_unordered_set(m_persisted_dynamic_variables) ||
          update_unordered_set(m_persisted_dynamic_parse_early_variables) ||
          update_unordered_set(m_persisted_dynamic_plugin_variables) ||
          update_unordered_set(m_persisted_dynamic_sensitive_variables) ||
          update_unordered_set(m_persisted_dynamic_sensitive_plugin_variables))
        return true;

      auto update_map = [&thd, &found, &name_str](auto &variables) -> bool {
        auto it = variables.find(name_str);
        if (it != variables.end()) {
          if (check_priv(thd, true)) return true;
          variables.erase(it);
          found = true;
        }
        return false;
      };

      if (update_map(m_persisted_static_variables) ||
          update_map(m_persisted_static_parse_early_variables) ||
          update_map(m_persisted_static_sensitive_variables))
        return true;

      return false;
    };

    // Erase the named variable
    if (erase_variable(name)) goto end;

    // If the variable has an alias, erase that too.
    std::string alias_name;
    mysql_mutex_assert_not_owner(&LOCK_plugin);
    mysql_rwlock_rdlock(&LOCK_system_variables_hash);
    { alias_name = get_variable_alias(name); }
    mysql_rwlock_unlock(&LOCK_system_variables_hash);
    if (!alias_name.empty() && erase_variable(alias_name.c_str())) goto end;

    if (!found) {
      /* if not present and if exists is specified, report warning */
      if (if_exists) {
        push_warning_printf(thd, Sql_condition::SL_WARNING,
                            ER_VAR_DOES_NOT_EXIST,
                            ER_THD(thd, ER_VAR_DOES_NOT_EXIST), name);
      } else {
        /* without IF EXISTS, report error */
        my_error(ER_VAR_DOES_NOT_EXIST, MYF(0), name);
        result = true;
      }
    }
  }
  unlock();
  if (found) flush_to_file();

  return result;

end:
  unlock();
  return true;
}

/**
  Return in-memory copy persist_variables_
*/
Persisted_variables_uset *
Persisted_variables_cache::get_persisted_dynamic_variables() {
  return &m_persisted_dynamic_variables;
}

/*
  Get SENSITIVE persisted variables
*/
Persisted_variables_uset *
Persisted_variables_cache::get_persisted_dynamic_sensitive_variables(THD *thd) {
  if (thd != nullptr && thd->security_context()
                                ->has_global_grant(STRING_WITH_LEN(
                                    "SENSITIVE_VARIABLES_OBSERVER"))
                                .first == true) {
    return &m_persisted_dynamic_sensitive_variables;
  }
  return nullptr;
}

/**
  Get PARSE_EARLY persisted variables
*/
Persisted_variables_uset *
Persisted_variables_cache::get_persisted_dynamic_parse_early_variables() {
  return &m_persisted_dynamic_parse_early_variables;
}

/**
  Return in-memory copy for static persisted variables
*/
Persisted_variables_umap *
Persisted_variables_cache::get_persisted_static_variables() {
  return &m_persisted_static_variables;
}

/**
  Get SENSITIVE persisted static variables
*/
Persisted_variables_umap *
Persisted_variables_cache::get_persisted_static_sensitive_variables(THD *thd) {
  if (thd != nullptr && thd->security_context()
                                ->has_global_grant(STRING_WITH_LEN(
                                    "SENSITIVE_VARIABLES_OBSERVER"))
                                .first == true) {
    return &m_persisted_static_sensitive_variables;
  }
  return nullptr;
}

Persisted_variables_umap *
Persisted_variables_cache::get_persisted_static_parse_early_variables() {
  return &m_persisted_static_parse_early_variables;
}

void Persisted_variables_cache::cleanup() {
  mysql_mutex_destroy(&m_LOCK_persist_variables);
  mysql_mutex_destroy(&m_LOCK_persist_file);
  parse_early_persisted_argv_alloc.Clear();
  ro_persisted_argv_alloc.Clear();
  ro_persisted_plugin_argv_alloc.Clear();
}

void Persisted_variables_cache::clear_sensitive_blob_and_iv() {
  m_sensitive_variables_blob.clear();
  m_iv.clear();
}

std::string Persisted_variables_cache::to_hex(const std::string &value) {
  std::stringstream output;
  for (auto const &character : value)
    output << std::hex << std::setfill('0') << std::setw(2)
           << (int)(unsigned char)character;
  return output.str();
}

std::string Persisted_variables_cache::from_hex(const std::string &value) {
  // string length has to be even
  if (value.length() % 2 != 0) return {};

  // string must consist of hex digits, all lowercase
  for (auto const &symbol : value)
    if (std::isupper(symbol) || !std::isxdigit(symbol)) return {};

  string output;
  for (size_t offset = 0; offset < value.length(); offset += 2) {
    std::stringstream chunk;
    chunk << std::hex << value.substr(offset, 2);
    size_t int_value;
    chunk >> int_value;
    output.push_back(static_cast<unsigned char>(int_value));
  }

  return output;
}

/**
  Get file encryption key. Use master key from keyring to decrypt it

  @param [out] file_key           Decrypted Key
  @param [out] file_key_length    Decrypted key length
  @param [in]  generate           Generate key if missing

  @returns status of key extraction operation
    @retval true  Error
    @retval false Success
*/
bool Persisted_variables_cache::get_file_encryption_key(
    std::unique_ptr<unsigned char[]> &file_key, size_t &file_key_length,
    bool generate /* = false */) {
  bool retval = true;
  file_key_length = 32;

  /* Check status of keyring service */
  if (m_keyring_support_available == false) {
    LogErr(ERROR_LEVEL, ER_PERSISTED_VARIABLES_LACK_KEYRING_SUPPORT);
    return retval;
  }

  /* First retrieve master key or create one if it's not available */
  unsigned char *secret = nullptr;
  size_t secret_length = 0;
  char *secret_type = nullptr;

  auto cleanup = create_scope_guard([&]() {
    if (secret != nullptr) my_free(secret);
    if (secret_type != nullptr) my_free(secret_type);
  });

  auto output = keyring_operations_helper::read_secret(
      srv_keyring_reader, m_key_info.m_master_key_name.c_str(), nullptr,
      &secret, &secret_length, &secret_type, PSI_NOT_INSTRUMENTED);

  if (output == -1) return retval;

  if (output == 0) {
    /* If key is missing and generation flag is false, return */
    if (generate == false) return retval;

    if (m_key_info.m_file_key.length() != 0 ||
        m_key_info.m_file_key_iv.length() != 0) {
      /*
        If an encrypted file key exists but the master key doesn't,
        return from here. This is a problem related to unavailability
        of master key.
      */
      LogErr(ERROR_LEVEL, ER_PERSISTED_VARIABLES_MASTER_KEY_NOT_FOUND,
             m_key_info.m_master_key_name.c_str());
      return retval;
    }
    /* Generate master key */
    if (srv_keyring_generator->generate(m_key_info.m_master_key_name.c_str(),
                                        nullptr,
                                        m_key_info.m_master_key_type.c_str(),
                                        m_key_info.m_master_key_size)) {
      LogErr(ERROR_LEVEL, ER_PERSISTED_VARIABLES_MASTER_KEY_CANNOT_BE_GENERATED,
             m_key_info.m_master_key_name.c_str());
      return retval;
    }

    output = keyring_operations_helper::read_secret(
        srv_keyring_reader, m_key_info.m_master_key_name.c_str(), nullptr,
        &secret, &secret_length, &secret_type, PSI_NOT_INSTRUMENTED);

    /* Doh! */
    if (output != 1) return retval;
  }

  if (m_key_info.m_file_key.length() == 0) {
    if (generate == false) return retval;
    /* File key does not exist, generate one */
    file_key = std::make_unique<unsigned char[]>(file_key_length);
    unsigned char iv[16];

    if (my_rand_buffer(iv, sizeof(iv)) ||
        my_rand_buffer(file_key.get(), file_key_length))
      return retval;

    /* encrypt file key */
    size_t encrypted_key_length =
        (file_key_length / MY_AES_BLOCK_SIZE) * MY_AES_BLOCK_SIZE;
    std::unique_ptr<unsigned char[]> encrypted_key =
        std::make_unique<unsigned char[]>(encrypted_key_length);

    auto error =
        my_aes_encrypt(file_key.get(), file_key_length, encrypted_key.get(),
                       secret, secret_length, my_aes_256_cbc, iv, false);

    if (error == MY_AES_BAD_DATA) {
      LogErr(ERROR_LEVEL, ER_PERSISTED_VARIABLES_ENCRYPTION_FAILED, "file key",
             "master key");
      return retval;
    }

    /* Store IV and encrypted key */
    m_key_info.m_file_key_iv =
        to_hex(std::string{reinterpret_cast<char *>(iv), sizeof(iv)});
    m_key_info.m_file_key =
        to_hex(std::string{reinterpret_cast<char *>(encrypted_key.get()),
                           static_cast<size_t>(error)});
    retval = false;
  } else {
    /* File key exists, decrypt it */
    std::string unhex_key = from_hex(m_key_info.m_file_key);
    std::string unhex_iv = from_hex(m_key_info.m_file_key_iv);

    std::unique_ptr<unsigned char[]> decrypted_file_key =
        std::make_unique<unsigned char[]>(unhex_key.length());

    auto error = my_aes_decrypt(
        reinterpret_cast<const unsigned char *>(unhex_key.c_str()),
        static_cast<uint32>(unhex_key.length()), decrypted_file_key.get(),
        secret, secret_length, my_aes_256_cbc,
        reinterpret_cast<const unsigned char *>(unhex_iv.c_str()), false);

    if (error == MY_AES_BAD_DATA) {
      LogErr(ERROR_LEVEL, ER_PERSISTED_VARIABLES_DECRYPTION_FAILED, "file key",
             "master key");
      return retval;
    }

    file_key = std::make_unique<unsigned char[]>(error);
    memcpy(file_key.get(), decrypted_file_key.get(), error);
    retval = false;
  }
  return retval;
}

/**
  Encrypt sensitive variables values

  @returns Status of the operation
*/
Persisted_variables_cache::return_status
Persisted_variables_cache::encrypt_sensitive_variables() {
  if (m_sensitive_variables_blob.length() == 0 && m_iv.length() == 0 &&
      m_persisted_static_sensitive_variables.size() == 0 &&
      m_persisted_dynamic_sensitive_variables.size() == 0)
    return return_status::NOT_REQUIRED;

  return_status retval = return_status::ERROR;
  /*
    Presence of blob/iv indicates that they could not be parsed at the
    beginning.
  */
  if (m_sensitive_variables_blob.length() != 0 || m_iv.length() != 0)
    return retval;

  /* Get file key */
  std::unique_ptr<unsigned char[]> file_key;
  size_t file_key_length;
  if (get_file_encryption_key(file_key, file_key_length, true)) return retval;

  /* Serialize sensitive variables */
  Json_object sensitive_variables_object;
  if (format_set(m_persisted_dynamic_sensitive_variables,
                 s_key_mysql_sensitive_dynamic_variables,
                 sensitive_variables_object))
    return retval;
  if (format_map(m_persisted_static_sensitive_variables,
                 s_key_mysql_sensitive_static_variables,
                 sensitive_variables_object))
    return retval;

  Json_wrapper json_wrapper(&sensitive_variables_object);
  json_wrapper.set_alias();
  String str;
  json_wrapper.to_string(&str, true, String().ptr(),
                         JsonDocumentDefaultDepthHandler);

  /* Encrypt sensitive variables */
  unsigned char iv[16];
  if (my_rand_buffer(iv, sizeof(iv))) return retval;
  size_t data_len = (str.length() / MY_AES_BLOCK_SIZE + 1) * MY_AES_BLOCK_SIZE;
  std::unique_ptr<unsigned char[]> encrypted_buffer =
      std::make_unique<unsigned char[]>(data_len);
  if (encrypted_buffer.get() == nullptr) return retval;

  auto error = my_aes_encrypt(
      reinterpret_cast<unsigned char *>(str.ptr()),
      static_cast<uint32>(str.length()), encrypted_buffer.get(), file_key.get(),
      static_cast<uint32>(file_key_length), my_aes_256_cbc, iv, true);

  if (error == MY_AES_BAD_DATA) {
    LogErr(ERROR_LEVEL, ER_PERSISTED_VARIABLES_ENCRYPTION_FAILED,
           "persisted SENSITIVE variables", "file key");
    return retval;
  }

  /* Convert to hex for storage */
  m_iv = to_hex(std::string{reinterpret_cast<char *>(iv), sizeof(iv)});
  m_sensitive_variables_blob =
      to_hex(std::string{reinterpret_cast<char *>(encrypted_buffer.get()),
                         static_cast<size_t>(error)});

  return return_status::SUCCESS;
}

/**
  Decrypt sensitive variables values

  @returns Status of the operation
    @retval false Success
    @retval true  Error
*/
Persisted_variables_cache::return_status
Persisted_variables_cache::decrypt_sensitive_variables() {
  if (m_sensitive_variables_blob.length() == 0 && m_iv.length() == 0)
    return return_status::NOT_REQUIRED;

  return_status retval = return_status::ERROR;

  /* Get file key */
  std::unique_ptr<unsigned char[]> file_key;
  size_t file_key_length;

  if (get_file_encryption_key(file_key, file_key_length, false)) return retval;

  /* Convert from hex to binary */
  std::string unhex_iv = from_hex(m_iv);
  std::string unhex_data = from_hex(m_sensitive_variables_blob);

  /* Decrypt the blob */
  std::unique_ptr<unsigned char[]> decrypted_data =
      std::make_unique<unsigned char[]>(unhex_data.length());
  if (decrypted_data.get() == nullptr) return retval;

  auto error = my_aes_decrypt(
      reinterpret_cast<const unsigned char *>(unhex_data.c_str()),
      static_cast<uint32>(unhex_data.length()), decrypted_data.get(),
      file_key.get(), file_key_length, my_aes_256_cbc,
      reinterpret_cast<const unsigned char *>(unhex_iv.c_str()), true);

  if (error == MY_AES_BAD_DATA) {
    LogErr(ERROR_LEVEL, ER_PERSISTED_VARIABLES_DECRYPTION_FAILED,
           "persisted SENSITIVE variables", "file key");
    return retval;
  }

  /* Parse the decrypted blob */
  std::unique_ptr<Json_dom> json(Json_dom::parse(
      reinterpret_cast<char *>(decrypted_data.get()), error,
      [](const char *, size_t) {}, JsonDocumentDefaultDepthHandler));
  if (!json.get()) return retval;

  if (json.get()->json_type() != enum_json_type::J_OBJECT) return retval;
  Json_object *json_obj = down_cast<Json_object *>(json.get());

  if (extract_set(*json_obj, s_key_mysql_sensitive_dynamic_variables,
                  m_persisted_dynamic_sensitive_variables) ||
      extract_map(*json_obj, s_key_mysql_sensitive_static_variables,
                  m_persisted_static_sensitive_variables)) {
    return retval;
  }

  /*
    Variables are added to required containers.
    There is no need to maintain the sensitive variables blob.
  */
  clear_sensitive_blob_and_iv();

  return return_status::SUCCESS;
}

/**
  We cache keyring support status just after reading manifest file.
  This is required because in the absence of a keyring component,
  keyring plugin may provide some of the services through
  daemon proxy keyring.

  However, we CANNOT use keyring plugin to encrypt SENSITIVE
  variables because upon server restart, keyring plugins will
  be loaded quite late.

  Later on, before each encryption operation, we refer to this
  cached value to decide whether to proceed with encryption or
  not.
*/
void Persisted_variables_cache::keyring_support_available() {
  m_keyring_support_available = keyring_status_no_error();
}
