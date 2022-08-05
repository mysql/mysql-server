/* Copyright (c) 2017, 2022, Oracle and/or its affiliates.

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

#include "migrate_keyring.h"
#include "my_default.h"  // my_getopt_use_args_separator
#include "mysql/components/services/log_builtins.h"
#include "mysqld.h"
#include "mysqld_error.h"
#include "sql_plugin.h"  // plugin_early_load_one
#include "violite.h"

using std::string;
using keyring_load_t = SERVICE_TYPE_NO_CONST(keyring_load);
using keyring_writer_t = SERVICE_TYPE_NO_CONST(keyring_writer);

Destination_keyring_component::Destination_keyring_component(
    const std::string component_path, const std::string implementation_name)
    : component_path_(component_path),
      keyring_load_service_(nullptr),
      keyring_writer_service_(nullptr),
      component_loaded_(false),
      ok_(false) {
  {
    my_service<SERVICE_TYPE(registry_registration)> registrator(
        "registry_registration", srv_registry);
    const char *urn[] = {component_path_.c_str()};
    bool load_status = dynamic_loader_srv->load(urn, 1);
    if (load_status == true) return;
    component_loaded_ = true;
  }

  std::string load_service_name("keyring_load");
  load_service_name += "." + implementation_name;

  my_h_service h_keyring_load_service = nullptr;
  my_h_service h_keyring_writer_service = nullptr;

  auto local_cleanup = [&]() {
    if (h_keyring_load_service != nullptr)
      srv_registry->release(h_keyring_load_service);
    h_keyring_load_service = nullptr;

    if (h_keyring_writer_service != nullptr)
      srv_registry->release(h_keyring_writer_service);
    h_keyring_writer_service = nullptr;
  };

  if (srv_registry->acquire(load_service_name.c_str(),
                            &h_keyring_load_service) ||
      srv_registry->acquire_related("keyring_writer", h_keyring_load_service,
                                    &h_keyring_writer_service)) {
    local_cleanup();
    return;
  }

  keyring_load_service_ =
      reinterpret_cast<const_keyring_load_t *>(h_keyring_load_service);
  keyring_writer_service_ =
      reinterpret_cast<const_keyring_writer_t *>(h_keyring_writer_service);

  /*
    In case of migration from keyring plugin to keyring
    component, we only support reading configuration from
    plugin directory
  */
  if (keyring_load_service_->load(opt_plugin_dir, nullptr) != 0) {
    return;
  }

  ok_ = true;
}

Destination_keyring_component::~Destination_keyring_component() {
  if (keyring_load_service_)
    srv_registry->release(reinterpret_cast<my_h_service>(
        const_cast<keyring_load_t *>(keyring_load_service_)));
  keyring_load_service_ = nullptr;

  if (keyring_writer_service_ != nullptr)
    srv_registry->release(reinterpret_cast<my_h_service>(
        const_cast<keyring_writer_t *>(keyring_writer_service_)));
  keyring_writer_service_ = nullptr;

  if (component_loaded_ == true) {
    my_service<SERVICE_TYPE(registry_registration)> registrator(
        "registry_registration", srv_registry);
    const char *urn[] = {component_path_.c_str()};
    dynamic_loader_srv->unload(urn, 1);
  }
  component_loaded_ = false;
  ok_ = false;
}

Migrate_keyring::Migrate_keyring() {
  m_source_plugin_handle = nullptr;
  m_destination_plugin_handle = nullptr;
  mysql = nullptr;
  m_destination_component = nullptr;
  m_argc = 0;
  m_argv = nullptr;
}

/**
  This function does the following:
    1. Read command line arguments specific to migration operation
    2. Get plugin_dir value.
    3. Get a connection handle by connecting to server.

   @param [in] argc                            Pointer to argc of original
  program
   @param [in] argv                            Pointer to argv of original
  program
   @param [in] source_plugin                   Pointer to source plugin option
   @param [in] destination_plugin              Pointer to destination plugin
  option
   @param [in] user                            User to login to server
   @param [in] host                            Host on which to connect to
  server
   @param [in] password                        Password used to connect to
  server
   @param [in] socket                          The socket file to use for
  connection
   @param [in] port                            Port number to use for connection
   @param [in] migrate_to_component            Migrate from plugin to component
  destination component

   @return 0 Success
   @return 1 Failure

*/
bool Migrate_keyring::init(int argc, char **argv, char *source_plugin,
                           char *destination_plugin, char *user, char *host,
                           char *password, char *socket, ulong port,
                           bool migrate_to_component) {
  DBUG_TRACE;

  std::size_t found = std::string::npos;
  string equal("=");
  string so(".so");
  string dll(".dll");
  const string compression_method("zlib,zstd,uncompressed");

  if (!source_plugin) {
    my_error(ER_KEYRING_MIGRATION_FAILURE, MYF(0),
             "Invalid --keyring-migration-source option.");
    return true;
  }
  if (!destination_plugin) {
    my_error(ER_KEYRING_MIGRATION_FAILURE, MYF(0),
             "Invalid --keyring-migration-destination option.");
    return true;
  }
  m_source_plugin_option = source_plugin;
  m_destination_plugin_option = destination_plugin;

  /* extract plugin name from the specified source plugin option */
  if ((found = m_source_plugin_option.find(equal)) != std::string::npos)
    m_source_plugin_name = m_source_plugin_option.substr(0, found);
  else if ((found = m_source_plugin_option.find(so)) != std::string::npos)
    m_source_plugin_name = m_source_plugin_option.substr(0, found);
  else if ((found = m_source_plugin_option.find(dll)) != std::string::npos)
    m_source_plugin_name = m_source_plugin_option.substr(0, found);
  else {
    LogErr(ERROR_LEVEL, ER_KEYRING_MIGRATE_FAILED,
           "Invalid source plugin option value.");
    return true;
  }

  /* extract plugin name from the specified destination plugin option */
  if ((found = m_destination_plugin_option.find(equal)) != std::string::npos)
    m_destination_plugin_name = m_destination_plugin_option.substr(0, found);
  else if ((found = m_destination_plugin_option.find(so)) != std::string::npos)
    m_destination_plugin_name = m_destination_plugin_option.substr(0, found);
  else if ((found = m_destination_plugin_option.find(dll)) != std::string::npos)
    m_destination_plugin_name = m_destination_plugin_option.substr(0, found);
  else {
    LogErr(ERROR_LEVEL, ER_KEYRING_MIGRATE_FAILED,
           "Invalid destination plugin option value.");
    return true;
  }

  m_migrate_to_component = migrate_to_component;
  if (m_migrate_to_component) {
    /* If we are migrating to component, construct complete URI */
    std::string uri("file://");
    if (check_valid_path(m_destination_plugin_option.c_str(),
                         m_destination_plugin_option.length())) {
      LogErr(ERROR_LEVEL, ER_KEYRING_MIGRATE_FAILED,
             "No paths allowed for shared library");
      return true;
    }
    uri.append(m_destination_plugin_name);
    m_destination_plugin_option = uri;
  }

  /* if connect options are provided then initiate connection */
  if (migrate_connect_options) {
    ssl_start();
    /* initiate connection */
    mysql = mysql_init(nullptr);
    net_server_ext_init(&server_extn);

    mysql_extension_set_server_extn(mysql, &server_extn);
    /* set default compression method */
    mysql_options(mysql, MYSQL_OPT_COMPRESSION_ALGORITHMS,
                  compression_method.c_str());
    enum mysql_ssl_mode ssl_mode = SSL_MODE_REQUIRED;
    mysql_options(mysql, MYSQL_OPT_SSL_MODE, &ssl_mode);
    mysql_options(mysql, MYSQL_OPT_CONNECT_ATTR_RESET, nullptr);
    mysql_options4(mysql, MYSQL_OPT_CONNECT_ATTR_ADD, "program_name", "mysqld");
    mysql_options4(mysql, MYSQL_OPT_CONNECT_ATTR_ADD, "_client_role",
                   "keyring_migration_tool");

    if (!mysql_real_connect(mysql, host, user, password, "", port, socket, 0)) {
      LogErr(ERROR_LEVEL, ER_KEYRING_MIGRATE_FAILED,
             "Connection to server failed.");
      return true;
    }
  }

  m_argc = argc;
  m_argv = new char *[m_argc + 2 + 1];  // 2 extra options + nullptr
  for (int cnt = 0; cnt < m_argc; ++cnt) {
    m_argv[cnt] = argv[cnt];
  }

  // add internal loose options
  // --loose_<source_plugin_name>_open_mode=1,
  // --loose_<source_plugin_name>_load_early=1,
  // --loose_<destination_plugin_name>_load_early=1,
  // open mode should disable writing on source keyring plugin
  // load early should inform plugin that it's working in migration mode
  size_t loose_option_count = 1;
  m_internal_option[0] = "--loose_" + m_source_plugin_name + "_open_mode=1";
  if (m_source_plugin_name == "keyring_hashicorp" ||
      m_destination_plugin_name == "keyring_hashicorp") {
    loose_option_count++;
    m_internal_option[1] = "--loose_keyring_hashicorp_load_early=1";
  }

  // add internal options to the argument vector
  for (size_t i = 0; i < loose_option_count; i++) {
    m_argv[m_argc] = const_cast<char *>(m_internal_option[i].c_str());
    m_argc++;
  }

  // null terminate and leave
  m_argv[m_argc] = nullptr;
  return false;
}

/**
  This function does the following in sequence:
    1. Disable access to keyring service APIs.
    2. Load source plugin.
    3. Load destination plugin.
    4. Fetch all keys from source plugin and upon
       success store in destination plugin.
    5. Enable access to keyring service APIs.
    6. Unload source plugin.
    7. Unload destination plugin.

  NOTE: In case there is any error while fetching keys from source plugin,
  this function would remove all keys stored as part of fetch.

  @return 0 Success
  @return 1 Failure
*/
bool Migrate_keyring::execute() {
  DBUG_TRACE;

  char **tmp_m_argv;

  /* Disable access to keyring service APIs */
  if (migrate_connect_options && disable_keyring_operations()) goto error;

  /* Load source plugin. */
  if (load_plugin(enum_plugin_type::SOURCE_PLUGIN)) {
    LogErr(ERROR_LEVEL, ER_KEYRING_MIGRATE_FAILED,
           "Failed to initialize source keyring");
    goto error;
  }

  if (!m_migrate_to_component) {
    /* Load destination plugin. */
    if (load_plugin(enum_plugin_type::DESTINATION_PLUGIN)) {
      LogErr(ERROR_LEVEL, ER_KEYRING_MIGRATE_FAILED,
             "Failed to initialize destination keyring");
      goto error;
    }
  } else {
    if (load_component()) {
      LogErr(ERROR_LEVEL, ER_KEYRING_MIGRATE_FAILED,
             "Failed to initialize destination keyring");
      goto error;
    }
  }

  /* skip program name */
  m_argc--;
  /* We use a tmp ptr instead of m_argv since if the latter gets changed, we
   * lose access to the allocated mem and hence there would be leak */
  tmp_m_argv = m_argv + 1;
  /* check for invalid options */
  if (m_argc > 1) {
    struct my_option no_opts[] = {{nullptr, 0, nullptr, nullptr, nullptr,
                                   nullptr, GET_NO_ARG, NO_ARG, 0, 0, 0,
                                   nullptr, 0, nullptr}};
    my_getopt_skip_unknown = false;
    my_getopt_use_args_separator = true;
    if (handle_options(&m_argc, &tmp_m_argv, no_opts, nullptr)) return true;

    if (m_argc > 1) {
      LogErr(WARNING_LEVEL, ER_KEYRING_MIGRATION_EXTRA_OPTIONS);
      return true;
    }
  }

  /* Fetch all keys from source plugin and store into destination plugin. */
  if (fetch_and_store_keys()) goto error;

  /* Enable access to keyring service APIs */
  if (migrate_connect_options) enable_keyring_operations();
  return false;

error:
  /*
   Enable keyring_operations in case of error
  */
  if (migrate_connect_options) enable_keyring_operations();
  return true;
}

/**
  Load component.

  @return false Success
  @return true Failure
*/
bool Migrate_keyring::load_component() {
  m_destination_component = new (std::nothrow) Destination_keyring_component(
      m_destination_plugin_option, m_destination_plugin_name);

  if (m_destination_component && !m_destination_component->ok()) {
    delete m_destination_component;
    m_destination_component = nullptr;
    return true;
  }
  return false;
}

/**
  Load plugin.

  @param [in] plugin_type        Indicates what plugin to be loaded

  @return 0 Success
  @return 1 Failure
*/
bool Migrate_keyring::load_plugin(enum_plugin_type plugin_type) {
  DBUG_TRACE;

  char *keyring_plugin = nullptr;
  char *plugin_name = nullptr;
  bool is_source_plugin = false;

  if (plugin_type == enum_plugin_type::SOURCE_PLUGIN) is_source_plugin = true;

  if (is_source_plugin) {
    keyring_plugin = const_cast<char *>(m_source_plugin_option.c_str());
    plugin_name = const_cast<char *>(m_source_plugin_name.c_str());
  } else {
    keyring_plugin = const_cast<char *>(m_destination_plugin_option.c_str());
    plugin_name = const_cast<char *>(m_destination_plugin_name.c_str());
  }

  if (plugin_early_load_one(&m_argc, m_argv, keyring_plugin))
    goto error;
  else {
    /* set plugin handle */
    plugin_ref plugin;
    plugin = my_plugin_lock_by_name(nullptr, to_lex_cstring(plugin_name),
                                    MYSQL_KEYRING_PLUGIN);
    if (plugin == nullptr) goto error;

    if (is_source_plugin)
      m_source_plugin_handle = (st_mysql_keyring *)plugin_decl(plugin)->info;
    else
      m_destination_plugin_handle =
          (st_mysql_keyring *)plugin_decl(plugin)->info;

    plugin_unlock(nullptr, plugin);
  }
  return false;

error:
  if (is_source_plugin)
    LogErr(ERROR_LEVEL, ER_KEYRING_MIGRATE_FAILED,
           "Failed to load source keyring plugin.");
  else
    LogErr(ERROR_LEVEL, ER_KEYRING_MIGRATE_FAILED,
           "Failed to load destination keyring plugin.");
  return true;
}

/**
  This function does the following in sequence:
    1. Initialize key iterator which will make iterator to position itself
       inorder to fetch a key.
    2. Using iterator get key ID and user name.
    3. Fetch the key information using key ID and user name.
    4. Store the fetched key into destination plugin.
    5. In case of errors remove keys from destination plugin.

  @return 0 Success
  @return 1 Failure
*/
bool Migrate_keyring::fetch_and_store_keys() {
  DBUG_TRACE;

  bool error = false;
  char key_id[MAX_KEY_LEN] = {0};
  char user_id[USERNAME_LENGTH] = {0};
  void *key = nullptr;
  size_t key_len = 0;
  char *key_type = nullptr;
  void *key_iterator = nullptr;
  const_keyring_writer_t *keyring_writer_service = nullptr;

  m_source_plugin_handle->mysql_key_iterator_init(&key_iterator);
  if (key_iterator == nullptr) {
    LogErr(ERROR_LEVEL, ER_KEYRING_MIGRATE_FAILED,
           "Initializing source keyring iterator failed.");
    return true;
  }

  if (m_migrate_to_component) {
    keyring_writer_service = m_destination_component->writer();
  }
  while (!error) {
    if (m_source_plugin_handle->mysql_key_iterator_get_key(key_iterator, key_id,
                                                           user_id))
      break;

    /* using key_info metadata fetch the actual key */
    if (m_source_plugin_handle->mysql_key_fetch(key_id, &key_type, user_id,
                                                &key, &key_len)) {
      /* fetch failed */
      string errmsg =
          "Fetching key (" + string(key_id) + ") from source plugin failed.";
      LogErr(ERROR_LEVEL, ER_KEYRING_MIGRATE_FAILED, errmsg.c_str());
      error = true;
    } else /* store the fetched key into destination plugin */ {
      error =
          m_migrate_to_component
              ? keyring_writer_service->store(
                    key_id, user_id, static_cast<const unsigned char *>(key),
                    key_len, key_type)
              : m_destination_plugin_handle->mysql_key_store(
                    key_id, key_type, user_id, key, key_len);
      if (error) {
        string errmsg = "Storing key (" + string(key_id) +
                        ") into destination plugin failed.";
        LogErr(ERROR_LEVEL, ER_KEYRING_MIGRATE_FAILED, errmsg.c_str());
      } else {
        /*
         keep track of keys stored in successfully so that they can be
         removed in case of error.
        */
        Key_info ki(key_id, user_id);
        m_source_keys.push_back(ki);
      }
    }
    if (key) my_free((char *)key);
    if (key_type) my_free(key_type);
  }

  /* If there are zero keys in the keyring, it means no keys were migrated */
  if (!error && m_source_keys.size() == 0) {
    LogErr(WARNING_LEVEL, ER_WARN_MIGRATION_EMPTY_SOURCE_KEYRING);
  }

  if (error) {
    /* something went wrong remove keys from destination plugin. */
    while (m_source_keys.size()) {
      Key_info ki = m_source_keys.back();
      if (m_destination_plugin_handle->mysql_key_remove(ki.m_key_id.c_str(),
                                                        ki.m_user_id.c_str())) {
        string errmsg = "Removing key (" + string(ki.m_key_id.c_str()) +
                        ") from destination plugin failed.";
        LogErr(ERROR_LEVEL, ER_KEYRING_MIGRATE_FAILED, errmsg.c_str());
      }
      m_source_keys.pop_back();
    }
  }
  m_source_plugin_handle->mysql_key_iterator_deinit(key_iterator);
  return error;
}

/**
  Disable variable @@keyring_operations.

  @return 0 Success
  @return 1 Failure
*/
bool Migrate_keyring::disable_keyring_operations() {
  DBUG_TRACE;
  const char query[] = "SET GLOBAL KEYRING_OPERATIONS=0";
  if (mysql && mysql_real_query(mysql, query, strlen(query))) {
    LogErr(ERROR_LEVEL, ER_KEYRING_MIGRATE_FAILED,
           "Failed to disable keyring_operations variable.");
    return true;
  }
  return false;
}

/**
  Enable variable @@keyring_operations.

  @return 0 Success
  @return 1 Failure
*/
bool Migrate_keyring::enable_keyring_operations() {
  DBUG_TRACE;
  const char query[] = "SET GLOBAL KEYRING_OPERATIONS=1";

  /* clear the SSL error stack first as the connection could be encrypted */
  ERR_clear_error();

  if (mysql && mysql_real_query(mysql, query, strlen(query))) {
    LogErr(ERROR_LEVEL, ER_KEYRING_MIGRATE_FAILED,
           "Failed to enable keyring_operations variable.");
    return true;
  }
  return false;
}

/**
  Standard destructor to close connection handle.
*/
Migrate_keyring::~Migrate_keyring() {
  delete[] m_argv;
  m_argv = nullptr;
  if (mysql) {
    mysql_close(mysql);
    mysql = nullptr;
    if (migrate_connect_options) vio_end();
  }
  if (m_destination_component != nullptr) delete m_destination_component;
  m_destination_component = nullptr;
}
