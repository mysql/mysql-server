/* Copyright (c) 2017, 2024, Oracle and/or its affiliates.

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

#include "migrate_keyring.h"
#include <scope_guard.h>
#include "my_default.h"  // my_getopt_use_args_separator
#include "mysql/components/services/log_builtins.h"
#include "mysqld.h"
#include "mysqld_error.h"
#include "sql_plugin.h"  // plugin_early_load_one
#include "violite.h"

using std::string;
using keyring_load_t = SERVICE_TYPE_NO_CONST(keyring_load);
using keyring_writer_t = SERVICE_TYPE_NO_CONST(keyring_writer);
using keyring_reader_with_status_t =
    SERVICE_TYPE_NO_CONST(keyring_reader_with_status);
using keyring_keys_metadata_iterator_t =
    SERVICE_TYPE_NO_CONST(keyring_keys_metadata_iterator);

Keyring_component::Keyring_component(const std::string component_path,
                                     const std::string implementation_name)
    : component_path_(component_path),
      h_keyring_load_service(nullptr),
      keyring_load_service_(nullptr),
      component_loaded_(false),
      ok_(false) {
  {
    const char *urn[] = {component_path_.c_str()};
    const bool load_status = dynamic_loader_srv->load(urn, 1);
    if (load_status == true) return;
    component_loaded_ = true;
  }
  std::string load_service_name("keyring_load");
  load_service_name += "." + implementation_name;

  auto local_cleanup = [&]() {
    if (h_keyring_load_service != nullptr)
      srv_registry->release(h_keyring_load_service);
    h_keyring_load_service = nullptr;
  };

  if (srv_registry->acquire(load_service_name.c_str(),
                            &h_keyring_load_service)) {
    local_cleanup();
    return;
  }
}

Keyring_component::~Keyring_component() {
  if (component_loaded_ == true) {
    const char *urn[] = {component_path_.c_str()};
    dynamic_loader_srv->unload(urn, 1);
  }
  component_loaded_ = false;
  ok_ = false;

  if (keyring_load_service_)
    srv_registry->release(reinterpret_cast<my_h_service>(
        const_cast<keyring_load_t *>(keyring_load_service_)));
  keyring_load_service_ = nullptr;

  if (h_keyring_load_service != nullptr)
    srv_registry->release(h_keyring_load_service);
  h_keyring_load_service = nullptr;
}

Source_keyring_component::Source_keyring_component(
    const std::string component_path, const std::string implementation_name)
    : Keyring_component(component_path, implementation_name),
      keyring_keys_metadata_iterator_service_(nullptr),
      keyring_reader_service_(nullptr) {
  my_h_service h_keyring_reader_service = nullptr;
  my_h_service h_keyring_iterator_service = nullptr;

  auto local_cleanup = [&]() {
    if (h_keyring_reader_service != nullptr)
      srv_registry->release(h_keyring_reader_service);
    h_keyring_reader_service = nullptr;

    if (h_keyring_iterator_service != nullptr)
      srv_registry->release(h_keyring_iterator_service);
    h_keyring_iterator_service = nullptr;
  };

  if (srv_registry->acquire_related("keyring_reader_with_status",
                                    h_keyring_load_service,
                                    &h_keyring_reader_service) ||
      srv_registry->acquire_related("keyring_keys_metadata_iterator",
                                    h_keyring_load_service,
                                    &h_keyring_iterator_service)) {
    local_cleanup();
    return;
  }

  keyring_load_service_ =
      reinterpret_cast<const_keyring_load_t *>(h_keyring_load_service);
  keyring_reader_service_ =
      reinterpret_cast<const_keyring_reader_with_status_t *>(
          h_keyring_reader_service);
  keyring_keys_metadata_iterator_service_ =
      reinterpret_cast<const_keyring_keys_metadata_iterator_t *>(
          h_keyring_iterator_service);

  /*
    In case of migration to or from keyring component, we only support reading
    configuration from plugin directory
  */
  if (keyring_load_service_->load(opt_plugin_dir, nullptr) != 0) {
    return;
  }

  ok_ = true;
}

Source_keyring_component::~Source_keyring_component() {
  if (keyring_reader_service_ != nullptr)
    srv_registry->release(reinterpret_cast<my_h_service>(
        const_cast<keyring_reader_with_status_t *>(keyring_reader_service_)));
  keyring_reader_service_ = nullptr;

  if (keyring_keys_metadata_iterator_service_ != nullptr)
    srv_registry->release(reinterpret_cast<my_h_service>(
        const_cast<keyring_keys_metadata_iterator_t *>(
            keyring_keys_metadata_iterator_service_)));
  keyring_keys_metadata_iterator_service_ = nullptr;
}

Destination_keyring_component::Destination_keyring_component(
    const std::string component_path, const std::string implementation_name)
    : Keyring_component(component_path, implementation_name),
      keyring_writer_service_(nullptr) {
  my_h_service h_keyring_writer_service = nullptr;

  auto local_cleanup = [&]() {
    if (h_keyring_writer_service != nullptr)
      srv_registry->release(h_keyring_writer_service);
    h_keyring_writer_service = nullptr;
  };

  if (srv_registry->acquire_related("keyring_writer", h_keyring_load_service,
                                    &h_keyring_writer_service)) {
    local_cleanup();
    return;
  }

  keyring_load_service_ =
      reinterpret_cast<const_keyring_load_t *>(h_keyring_load_service);
  keyring_writer_service_ =
      reinterpret_cast<const_keyring_writer_t *>(h_keyring_writer_service);

  /*
    In case of migration to or from keyring component, we only support reading
    configuration from plugin directory
  */
  if (keyring_load_service_->load(opt_plugin_dir, nullptr) != 0) {
    return;
  }

  ok_ = true;
}

Destination_keyring_component::~Destination_keyring_component() {
  if (keyring_writer_service_ != nullptr)
    srv_registry->release(reinterpret_cast<my_h_service>(
        const_cast<keyring_writer_t *>(keyring_writer_service_)));
  keyring_writer_service_ = nullptr;
}

Migrate_keyring::Migrate_keyring() {
  m_source_plugin_handle = nullptr;
  m_destination_plugin_handle = nullptr;
  mysql = nullptr;
  m_source_component = nullptr;
  m_destination_component = nullptr;
  m_argc = 0;
  m_argv = nullptr;
  m_migrate_from_component = false;
  m_migrate_to_component = false;
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
   @param [in] migrate_from_component          Migrate from component to plugin

   @return 0 Success
   @return 1 Failure

*/
bool Migrate_keyring::init(int argc, char **argv, char *source_plugin,
                           char *destination_plugin, char *user, char *host,
                           char *password, char *socket, ulong port,
                           bool migrate_to_component,
                           bool migrate_from_component) {
  DBUG_TRACE;

  std::size_t found = std::string::npos;
  const string equal("=");
  const string so(".so");
  const string dll(".dll");
  const string compression_method("zlib,zstd,uncompressed");

  if (migrate_from_component && migrate_to_component) {
    LogErr(ERROR_LEVEL, ER_KEYRING_MIGRATE_FAILED,
           "Component to component migration cannot be performed using "
           "migration server. Please use mysql_migrate_keyring utility");
    return true;
  }

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
  m_migrate_from_component = migrate_from_component;
  if (m_migrate_from_component) {
    /* If we are migrating from component, construct complete URI */
    std::string uri("file://");
    if (check_valid_path(m_source_plugin_option.c_str(),
                         m_source_plugin_option.length())) {
      LogErr(ERROR_LEVEL, ER_KEYRING_MIGRATE_FAILED,
             "No paths allowed for shared library");
      return true;
    }
    uri.append(m_source_plugin_name);
    m_source_plugin_option = uri;
  }
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
  assert(!m_migrate_from_component || !m_migrate_to_component);
  char **tmp_m_argv;

  /* Disable access to keyring service APIs */
  if (migrate_connect_options && disable_keyring_operations()) goto error;

  if (m_migrate_from_component ? load_component()
                               : load_plugin(enum_plugin_type::SOURCE_PLUGIN)) {
    LogErr(ERROR_LEVEL, ER_KEYRING_MIGRATE_FAILED,
           "Failed to initialize source keyring");
    goto error;
  }

  if (m_migrate_to_component
          ? load_component()
          : load_plugin(enum_plugin_type::DESTINATION_PLUGIN)) {
    LogErr(ERROR_LEVEL, ER_KEYRING_MIGRATE_FAILED,
           "Failed to initialize destination keyring");
    goto error;
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
  if (m_migrate_from_component) {
    m_source_component = new (std::nothrow)
        Source_keyring_component(m_source_plugin_option, m_source_plugin_name);

    if (m_source_component && !m_source_component->ok()) {
      delete m_source_component;
      m_source_component = nullptr;
      return true;
    }
  }
  if (m_migrate_to_component) {
    m_destination_component = new (std::nothrow) Destination_keyring_component(
        m_destination_plugin_option, m_destination_plugin_name);

    if (m_destination_component && !m_destination_component->ok()) {
      delete m_destination_component;
      m_destination_component = nullptr;
      return true;
    }
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

static bool fetch_key_from_source_keyring_component(
    const_keyring_keys_metadata_iterator_t *metadata_iterator,
    my_h_keyring_keys_metadata_iterator iterator_,
    const_keyring_reader_with_status_t *reader, char *key_id, char *user_id,
    void **key, size_t *key_len, char **key_type, bool *skipped) {
  size_t key_id_length = 0, user_id_length = 0, data_type_size = 0;

  /* Fetch length */
  if (metadata_iterator->get_length(iterator_, &key_id_length,
                                    &user_id_length) != 0) {
    LogErr(ERROR_LEVEL, ER_KEYRING_MIGRATE_FAILED,
           "Could not fetch next available key content from keyring source");
    return true;
  }
  /* Fetch metadata of next available key */
  if (metadata_iterator->get(iterator_, key_id, key_id_length + 1, user_id,
                             user_id_length + 1) != 0) {
    LogErr(ERROR_LEVEL, ER_KEYRING_MIGRATE_FAILED,
           "Could not fetch next available key content from keyring source");
    return true;
  }
  /* Fetch key details */
  my_h_keyring_reader_object reader_object = nullptr;
  const bool status = reader->init(key_id, user_id, &reader_object);
  if (status == true) {
    LogErr(ERROR_LEVEL, ER_KEYRING_MIGRATE_FAILED, "Keyring reported error");
    return true;
  }
  if (reader_object == nullptr) {
    LogErr(INFORMATION_LEVEL, ER_KEYRING_MIGRATE_SKIPPED_KEY, key_id, user_id);
    *skipped = true;
    return false;
  }
  auto cleanup_guard = create_scope_guard([&] {
    if (reader_object != nullptr) {
      if (reader->deinit(reader_object) != 0)
        LogErr(ERROR_LEVEL, ER_KEYRING_MIGRATE_MEMORY_DEALLOCATION_FAILED);
    }
    reader_object = nullptr;
  });
  if (reader->fetch_length(reader_object, key_len, &data_type_size) != 0) {
    LogErr(INFORMATION_LEVEL, ER_KEYRING_MIGRATE_SKIPPED_KEY, key_id, user_id);
    *skipped = true;
    return false;
  }
  unsigned char *key_local = reinterpret_cast<unsigned char *>(
      my_malloc(PSI_NOT_INSTRUMENTED, *key_len, MYF(MY_WME)));
  char *key_type_local = reinterpret_cast<char *>(
      my_malloc(PSI_NOT_INSTRUMENTED, data_type_size + 1, MYF(MY_WME)));
  memset(key_local, 0, *key_len);
  memset(key_type_local, 0, data_type_size + 1);

  if (key_local == nullptr || key_type_local == nullptr) {
    string errmsg =
        "Failed to allocated required memory for data pointed by "
        "data_id: " +
        string(key_id) + ", auth_id: " + string(user_id);
    LogErr(ERROR_LEVEL, ER_KEYRING_MIGRATE_FAILED, errmsg.c_str());
  }
  if (reader->fetch(reader_object, key_local, *key_len, key_len, key_type_local,
                    data_type_size + 1, &data_type_size) != 0) {
    LogErr(INFORMATION_LEVEL, ER_KEYRING_MIGRATE_SKIPPED_KEY, key_id, user_id);
    *skipped = true;
    return false;
  }
  *key = key_local;
  *key_type = key_type_local;
  return false;
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
  const_keyring_reader_with_status_t *reader = nullptr;
  const_keyring_writer_t *writer = nullptr;
  const_keyring_keys_metadata_iterator_t *metadata_iterator = nullptr;
  my_h_keyring_keys_metadata_iterator iterator_{nullptr};
  bool next_ok = true, skipped = false;

  if (m_migrate_from_component) {
    metadata_iterator = m_source_component->metadata_iterator();
    reader = m_source_component->reader();
    metadata_iterator->init(&iterator_);
  } else {
    m_source_plugin_handle->mysql_key_iterator_init(&key_iterator);
  }
  if (key_iterator == nullptr && iterator_ == nullptr) {
    LogErr(ERROR_LEVEL, ER_KEYRING_MIGRATE_FAILED,
           "Initializing source keyring iterator failed.");
    return true;
  }

  if (m_migrate_to_component) {
    writer = m_destination_component->writer();
  }
  while (!error) {
    if (m_migrate_from_component) {
      if (!metadata_iterator->is_valid(iterator_) || !next_ok) break;
      skipped = false;
      memset(key_id, 0, MAX_KEY_LEN);
      memset(user_id, 0, USERNAME_LENGTH);
      key = nullptr;
      key_type = nullptr;
      error = fetch_key_from_source_keyring_component(
          metadata_iterator, iterator_, reader, key_id, user_id, &key, &key_len,
          &key_type, &skipped);
      next_ok = !metadata_iterator->next(iterator_);
    } else {
      if (m_source_plugin_handle->mysql_key_iterator_get_key(key_iterator,
                                                             key_id, user_id))
        break;

      /* using key_info metadata fetch the actual key */
      if (m_source_plugin_handle->mysql_key_fetch(key_id, &key_type, user_id,
                                                  &key, &key_len)) {
        /* fetch failed */
        string errmsg =
            "Fetching key (" + string(key_id) + ") from source plugin failed.";
        LogErr(ERROR_LEVEL, ER_KEYRING_MIGRATE_FAILED, errmsg.c_str());
        error = true;
      }
    }
    if (!error && !skipped) {
      /* store the fetched key into destination plugin */
      error = m_migrate_to_component
                  ? writer->store(key_id, user_id,
                                  static_cast<const unsigned char *>(key),
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
        const Key_info ki(key_id, user_id);
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
    /* something went wrong remove keys from destination keystore. */
    while (m_source_keys.size()) {
      Key_info ki = m_source_keys.back();
      if (m_migrate_to_component
              ? writer->remove(ki.m_key_id.c_str(), ki.m_user_id.c_str())
              : m_destination_plugin_handle->mysql_key_remove(
                    ki.m_key_id.c_str(), ki.m_user_id.c_str())) {
        string errmsg = "Removing key (" + string(ki.m_key_id.c_str()) +
                        ") from destination keystore failed.";
        LogErr(ERROR_LEVEL, ER_KEYRING_MIGRATE_FAILED, errmsg.c_str());
      }
      m_source_keys.pop_back();
    }
  }
  if (m_migrate_from_component)
    metadata_iterator->deinit(iterator_);
  else
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
  if (m_source_component != nullptr) delete m_source_component;
  m_source_component = nullptr;
  if (m_destination_component != nullptr) delete m_destination_component;
  m_destination_component = nullptr;
}
