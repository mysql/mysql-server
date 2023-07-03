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

#ifndef PERSISTED_VARIABLE_H_INCLUDED
#define PERSISTED_VARIABLE_H_INCLUDED

#include <stddef.h>
#include <map>
#include <string>
#include <unordered_set>

#include "map_helpers.h"
#include "my_alloc.h"
#include "my_inttypes.h"
#include "my_psi_config.h"
#include "mysql/components/services/bits/mysql_mutex_bits.h"
#include "mysql/components/services/bits/psi_bits.h"
#include "mysql/psi/mysql_mutex.h"
#include "sql/psi_memory_key.h"
#include "sql_string.h"

class Json_dom;
class Json_object;
class THD;
class set_var;
class sys_var;
struct MYSQL_FILE;

/**
  STRUCT st_persist_var

  This structure represents information of a variable which is to
  be persisted in mysql-auto.cnf file.
*/
struct st_persist_var final {
  std::string key;
  std::string value;
  ulonglong timestamp;
  std::string user;
  std::string host;
  bool is_null;
  st_persist_var();
  st_persist_var(THD *thd);
  st_persist_var(const std::string key, const std::string value,
                 const ulonglong timestamp, const std::string user,
                 const std::string host, const bool is_null);
  /* This is custom comparison function used to make the unordered_set
     work with the default std::hash for user defined types. */
  bool operator==(const st_persist_var &persist_var) const {
    return key == persist_var.key;
  }
};

/**
  STRUCT st_persist_var_hash

  This structure has a custom hasher function used to make the unordered_set
  to work with the default std::hash for userdefined types.
*/
struct st_persist_var_hash {
  size_t operator()(const st_persist_var &pv) const { return pv.key.length(); }
};

/**
  CLASS Persisted_variables_cache
    Holds <name,value> pair of all options which needs to be persisted
    to a file.

  OVERVIEW
  --------
  When first SET PERSIST statement is executed we instantiate
  Persisted_variables_cache which loads the config file if present into
  m_persisted_dynamic_variables set. This is a singleton operation.
  m_persisted_dynamic_variables is an in-memory copy of config file itself. If
  the SET statement passes then this in-memory is updated and flushed to file as
  an atomic operation.

  Next SET PERSIST statement would only update the in-memory copy and sync
  to config file instead of loading the file again.
*/

#ifdef HAVE_PSI_INTERFACE
void my_init_persist_psi_keys(void);
#endif /* HAVE_PSI_INTERFACE */

using Persisted_variables_umap =
    malloc_unordered_map<std::string, st_persist_var>;
using Persisted_variables_uset =
    malloc_unordered_set<st_persist_var, st_persist_var_hash>;

class Persisted_variables_cache final {
 protected:
  enum class File_version {
    VERSION_V1 = 1,
    VERSION_V2,
  };

 public:
  explicit Persisted_variables_cache();
  int init(int *argc, char ***argv);
  static Persisted_variables_cache *get_instance();
  /**
    Update in-memory copy for every SET PERSIST statement
  */
  bool set_variable(THD *thd, set_var *system_var);
  /**
    Driver: Flush in-memory copy to persistent file
  */
  bool flush_to_file();
  /**
    Write v2 persistent file
  */
  bool write_persist_file_v2(String &dest, bool &do_cleanup);
  /**
    Driver function: Read options from persistent file
  */
  int read_persist_file();
  /**
    Read v1 persistent file
  */
  int read_persist_file_v1(const Json_object *json_object);
  /**
    Read v2 persistent file
  */
  int read_persist_file_v2(const Json_object *json_object);
  /**
    Search for persisted config file and if found read persistent options
  */
  bool load_persist_file();
  bool set_persisted_options(bool plugin_options,
                             const char *target_var_name = nullptr,
                             int target_var_name_length = 0);
  /**
    Reset persisted options
  */
  bool reset_persisted_variables(THD *thd, const char *name, bool if_exists);

  /**
    Get persisted variables
  */
  Persisted_variables_uset *get_persisted_dynamic_variables();
  /**
    Get persisted parse-early variables
  */
  Persisted_variables_uset *get_persisted_dynamic_parse_early_variables();
  /**
    Get SENSITIVE persisted variables
  */
  Persisted_variables_uset *get_persisted_dynamic_sensitive_variables(THD *thd);

  /**
    Get persisted static variables
  */
  Persisted_variables_umap *get_persisted_static_variables();
  /**
    Get persisted parse-early static variables
  */
  Persisted_variables_umap *get_persisted_static_parse_early_variables();
  /**
    Get SENSITIVE persisted static variables
  */
  Persisted_variables_umap *get_persisted_static_sensitive_variables(THD *thd);

  /**
    append read only persisted variables to command line options with a
    separator.
  */
  bool append_read_only_variables(int *argc, char ***argv,
                                  bool arg_separator_added = false,
                                  bool plugin_options = false,
                                  MEM_ROOT *root_to_use = nullptr);

  /**
    append PARSE EARLY read only persisted variables to command
    line options with a separator.
  */
  bool append_parse_early_variables(int *argc, char ***argv,
                                    bool &arg_separator_added);

  void cleanup();

  /**
    Acquire lock on m_persisted_dynamic_variables/m_persisted_static_variables
  */
  void lock() { mysql_mutex_lock(&m_LOCK_persist_variables); }
  /**
    Release lock on m_persisted_dynamic_variables/m_persisted_static_variables
  */
  void unlock() { mysql_mutex_unlock(&m_LOCK_persist_variables); }
  /**
    Assert caller that owns lock on
    m_persisted_dynamic_variables/m_persisted_static_variables
  */
  void assert_lock_owner() {
    mysql_mutex_assert_owner(&m_LOCK_persist_variables);
  }
  /**
    Set internal state to reflect keyring support status
  */
  void keyring_support_available();

  std::string to_hex(const std::string &value);
  std::string from_hex(const std::string &value);

 private:
  /* Helper function to get variable value */
  static String *get_variable_value(THD *thd, sys_var *system_var, String *str,
                                    bool *is_null);
  /**
    If the variable has an alias, return the name for the alias.
  */
  static const char *get_variable_alias(const sys_var *system_var);
  static std::string get_variable_alias(const char *name);
  /* Helper function to extract variables from json formatted string */
  bool extract_variables_from_json(const Json_dom *dom,
                                   bool is_read_only = false);
  /**
    After extracting the variables from the JSON, we duplicate any
    variable definition that relates to an alias.
  */
  void load_aliases();

  enum class return_status { NOT_REQUIRED, SUCCESS, ERROR };
  bool get_file_encryption_key(std::unique_ptr<unsigned char[]> &file_key,
                               size_t &file_key_length, bool generate = false);
  return_status encrypt_sensitive_variables();
  return_status decrypt_sensitive_variables();

  /** Helper to set source information for PARSE_EARLY variables */
  void set_parse_early_sources();

 private:
  /* Helper functions for file IO */
  void clear_sensitive_blob_and_iv();
  bool open_persist_file(int flag);
  bool open_persist_backup_file(int flag);
  void close_persist_file();

 private:
  /*
    There are two main types of variables:
    1. Static variables: Those which cannot be set at runtime
    2. Dynamic variables: Those which can be set on a running server

    Each of these types is further divided in 3 sub-types:
    A. PARSE_EARLY variables - This set of variables require to be set very
                               early in server start-up sequence
    B. SENSITIVE variables   - This set of variables may be stored in
                               encrypted format if a suitable keyring
                               component is available
    C. All other variables   - These are the variables that do not have
                               none of the mentioned properties

    Two of the above mentioned sub-types exist because we treat
    variables with certain properties in special manner for reasons mentioned
    above for each of the In future, more categories may need special
    treatment and if so, above mentioned sub-categories will increase.

    These gives us total of 6 categories (3 each for static and dynamic
    types of variables). Each of these categories of variables are processed
    and/or loaded at different point of time in start-up sequence:

    - 1A and 2A variables are added to command line argument at the very
      beginning of the start-up
    - 1B and 2B are decrypted once keyring component is loaded.
    - 1B and 1C are added to command line
    - 2B and 2C are set at a later point in start-up cycle when THD
      initialization is possible. Some of the variables of types 2B and 2C
      are not processed until corresponding component or plugin is loaded.
      These subsets of variables are moved to special in-memory containers
      reserved for them.

    The persisted options file contains 6 JSON objects - one each for
    above mentioned categories. When file is read, data from each JSON object
    is loaded in different in-memory containers.
  */

  /* In memory copy of Persisted PARSE EARLY static variables' values */
  Persisted_variables_umap m_persisted_static_sensitive_variables;
  /* In memory copy of Persisted SENSITIVE static variables' values */
  Persisted_variables_umap m_persisted_static_parse_early_variables;
  /* In memory copy of all other Persisted static variables' values */
  Persisted_variables_umap m_persisted_static_variables;

  /* In memory copy of Persisted PARSE EARLY dynamic variables' values */
  Persisted_variables_uset m_persisted_dynamic_parse_early_variables;
  /* In memory copy of Persisted SENSITIVE dynamic variables' values */
  Persisted_variables_uset m_persisted_dynamic_sensitive_variables;
  /* In memory copy of all other Persisted dynamic variables' values */
  Persisted_variables_uset m_persisted_dynamic_variables;

  /*
    In memory copy of Persisted SENSITIVE dynamic
    variables whose plugin is not yet installed
  */
  Persisted_variables_uset m_persisted_dynamic_sensitive_plugin_variables;
  /*
    In memory copy of all other Persisted dynamic
    variables whose plugin is not yet installed
  */
  Persisted_variables_uset m_persisted_dynamic_plugin_variables;

  struct Key_info {
    std::string m_master_key_name{"persisted_variables_key"};
    std::string m_master_key_type{"AES"};
    const size_t m_master_key_size{32};

    std::string m_file_key{};
    std::string m_file_key_iv{};
  };

  /* Key */
  Key_info m_key_info;

  /* Status of keyring support */
  bool m_keyring_support_available{false};
  /* Sensitive variables blob - HEX representation */
  std::string m_sensitive_variables_blob{};
  /* IV - HEX representation */
  std::string m_iv{};

  mysql_mutex_t m_LOCK_persist_variables;
  static Persisted_variables_cache *m_instance;

  /* File handler members */
  MYSQL_FILE *m_fd;
  std::string m_persist_filename;
  std::string m_persist_backup_filename;
  mysql_mutex_t m_LOCK_persist_file;
  /* Memory for parse early read only persisted options */
  MEM_ROOT parse_early_persisted_argv_alloc{
      key_memory_persisted_variables_memroot, 512};
  /* Memory for read only persisted options */
  MEM_ROOT ro_persisted_argv_alloc{key_memory_persisted_variables_memroot, 512};
  /* Memory for read only persisted plugin options */
  MEM_ROOT ro_persisted_plugin_argv_alloc{
      key_memory_persisted_variables_memroot, 512};

  /* default version */
  File_version m_default_version = File_version::VERSION_V2;
};

#endif /* PERSISTED_VARIABLE_H_INCLUDED */
