/* Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#include <cstring>
#include <memory>

#include "keyring_file.h"
#include "option_usage.h"

/* Keyring_encryption_service_impl */
#include <components/keyrings/common/component_helpers/include/keyring_encryption_service_definition.h>
/* Keyring_generator_service_impl */
#include <components/keyrings/common/component_helpers/include/keyring_generator_service_definition.h>
/* Keyring_load_service_impl */
#include <components/keyrings/common/component_helpers/include/keyring_load_service_definition.h>
/* Keyring_keys_metadata_iterator_service_impl */
#include <components/keyrings/common/component_helpers/include/keyring_keys_metadata_iterator_service_definition.h>
/* Log_builtins_keyring */
#include <components/keyrings/common/component_helpers/include/keyring_log_builtins_definition.h>
/* Keyring_metadata_query_service_impl */
#include <components/keyrings/common/component_helpers/include/keyring_metadata_query_service_definition.h>
/* Keyring_reader_service_impl */
#include <components/keyrings/common/component_helpers/include/keyring_reader_service_definition.h>
/* Keyring_writer_service_impl */
#include <components/keyrings/common/component_helpers/include/keyring_writer_service_definition.h>

/* clang-format off */
/**
  @page PAGE_COMPONENT_KEYRING_FILE component_keyring_file

  This is keyring component services' implementation with file as backend to
  store data. This component implements following keyring services:

  - keyring_aes
  - keyring_generate
  - keyring_keys_metadata_iterator
  - keyring_component_status
  - keyring_metadata_query
  - keyring_reader
  - keyring_reload
  - keyring_writer

  Data is stored in JSON format.
  @code
  {
    "version": "1.0",
    "elements": [
      {
        "user": "<user_name>",
        "data_id": "<name>",
        "data_type": "<data_type>",
        "data": "<hex_of_data>",
        "extension": []
      },
      ...
      ...
    ]
  }
  @endcode

  For most parts, component_keyring_file relies on keyring_common library
  for implementation.

  The component relies on component_keyring_file.cnf file for configuration.

  Location of this configuration file is same directory where component_keyring_file
  shared library is located. This configuration file should contain information
  in one of the following formats.

  1. Signal component to read configuration from current working directory
  @code
  {
    "read_local_config": true
  }
  @endcode

  2. Details of data file and nature of keyring
  @code
  {
    "path": <path to data file>,
    "read_only": <boolean value to signal state of the keyring>
  }
  @endcode

  If configuration file co-located with shared library signals to read
  configuration locally, current working directory is searched for
  component_keyring_file.cnf and expected format is 2.

  The component exposes following status information through
  keyring_metadata_query service implementation.

  1. Name of the keyring
  2. Author
  3. Implementation name
  4. Version
  5. Component status
  6. Data file location
  7. Read only status

  <b>
    Note: Implementation does not provide concurrency control.
          That is responsibility of users of the services.
  </b>
*/
/* clang-format on */

using keyring_common::operations::Keyring_operations;
using keyring_file::backend::Keyring_file_backend;
using keyring_file::config::Config_pod;
using keyring_file::config::g_component_path;
using keyring_file::config::g_instance_path;

/** Dependencies */
REQUIRES_SERVICE_PLACEHOLDER(log_builtins);
REQUIRES_SERVICE_PLACEHOLDER(log_builtins_string);
REQUIRES_SERVICE_PLACEHOLDER(registry_registration);
REQUIRES_SERVICE_PLACEHOLDER_AS(registry, mysql_service_registry_no_lock);
REQUIRES_SERVICE_PLACEHOLDER_AS(registry_registration,
                                mysql_service_registration_no_lock);

SERVICE_TYPE(log_builtins) * log_bi;
SERVICE_TYPE(log_builtins_string) * log_bs;

namespace keyring_file {
/** Keyring operations object */
Keyring_operations<Keyring_file_backend> *g_keyring_operations = nullptr;

/** Keyring data source */
Config_pod *g_config_pod = nullptr;

/** Keyring state */
bool g_keyring_file_inited = false;

/**
  Set path to component

  @param [in] component_path  Path to component library
  @param [in] instance_path   Path to instance specific config

  @returns initialization status
    @retval false Successful initialization
    @retval true  Error
*/
bool set_paths(const char *component_path, const char *instance_path) {
  char *save_c = g_component_path;
  char *save_i = g_instance_path;
  g_component_path = strdup(component_path != nullptr ? component_path : "");
  g_instance_path = strdup(instance_path != nullptr ? instance_path : "");
  if (g_component_path == nullptr || g_instance_path == nullptr) {
    g_component_path = save_c;
    g_instance_path = save_i;
    return true;
  }

  if (save_c != nullptr) free(save_c);
  if (save_i != nullptr) free(save_i);
  return false;
}

/**
  Initialize or re-initialize keyring.
  1. Read configuration file
  2. Read keyring file
  3. Initialize internal cache

  @param [out] err Error message

  @returns Status of read operations
    @retval false Read config and data
    @retval true  Error reading config or data.
                  Existing data remains as it is.
*/
bool init_or_reinit_keyring(std::string &err) {
  /* Get config */
  std::unique_ptr<Config_pod> new_config_pod;
  if (keyring_file::config::find_and_read_config_file(new_config_pod, err))
    return true;

  /* Initialize backend handler */
  std::unique_ptr<Keyring_file_backend> new_backend =
      std::make_unique<Keyring_file_backend>(new_config_pod->config_file_path_,
                                             new_config_pod->read_only_);
  if (!new_backend || !new_backend->valid()) {
    err = "Failed to initialize keyring backend";
    return true;
  }

  /* Create new operations class */
  auto *new_operations = new (std::nothrow)
      Keyring_operations<Keyring_file_backend>(true, new_backend.release());
  if (new_operations == nullptr) {
    err = "Failed to allocate memory for keyring operations";
    return true;
  }

  if (!new_operations->valid()) {
    delete new_operations;
    err = "Failed to initialize keyring operations";
    return true;
  }

  std::swap(g_keyring_operations, new_operations);
  const Config_pod *current = g_config_pod;
  g_config_pod = new_config_pod.release();
  delete current;
  delete new_operations;
  return false;
}

/**
  Initialization function for component - Used when loading the component
*/
static mysql_service_status_t keyring_file_init() {
  log_bi = mysql_service_log_builtins;
  log_bs = mysql_service_log_builtins_string;
  if (keyring_file_component_option_usage_init()) return true;
  g_component_callbacks = new (std::nothrow)
      keyring_common::service_implementation::Component_callbacks();

  return false;
}

/**
  De-initialization function for component - Used when unloading the component
*/
static mysql_service_status_t keyring_file_deinit() {
  if (keyring_file_component_option_usage_deinit()) return true;
  g_keyring_file_inited = false;
  if (g_component_path) free(g_component_path);
  g_component_path = nullptr;
  if (g_instance_path) free(g_instance_path);
  g_instance_path = nullptr;

  delete g_keyring_operations;
  g_keyring_operations = nullptr;

  delete g_config_pod;
  g_config_pod = nullptr;

  delete g_component_callbacks;
  g_component_callbacks = nullptr;

  return false;
}

}  // namespace keyring_file

/** ======================================================================= */

/** Component declaration related stuff */

/** This component provides implementation of following component services */
KEYRING_AES_IMPLEMENTOR(component_keyring_file);
KEYRING_GENERATOR_IMPLEMENTOR(component_keyring_file);
KEYRING_LOAD_IMPLEMENTOR(component_keyring_file);
KEYRING_KEYS_METADATA_FORWARD_ITERATOR_IMPLEMENTOR(component_keyring_file);
KEYRING_COMPONENT_STATUS_IMPLEMENTOR(component_keyring_file);
KEYRING_COMPONENT_METADATA_QUERY_IMPLEMENTOR(component_keyring_file);
KEYRING_READER_IMPLEMENTOR(component_keyring_file);
KEYRING_WRITER_IMPLEMENTOR(component_keyring_file);
/* Used if log_builtins is not available */
KEYRING_LOG_BUILTINS_IMPLEMENTOR(component_keyring_file);
KEYRING_LOG_BUILTINS_STRING_IMPLEMENTOR(component_keyring_file);

/** Component provides */
BEGIN_COMPONENT_PROVIDES(component_keyring_file)
PROVIDES_SERVICE(component_keyring_file, keyring_aes),
    PROVIDES_SERVICE(component_keyring_file, keyring_generator),
    PROVIDES_SERVICE(component_keyring_file, keyring_load),
    PROVIDES_SERVICE(component_keyring_file, keyring_keys_metadata_iterator),
    PROVIDES_SERVICE(component_keyring_file, keyring_component_status),
    PROVIDES_SERVICE(component_keyring_file, keyring_component_metadata_query),
    PROVIDES_SERVICE(component_keyring_file, keyring_reader_with_status),
    PROVIDES_SERVICE(component_keyring_file, keyring_writer),
    PROVIDES_SERVICE(component_keyring_file, log_builtins),
    PROVIDES_SERVICE(component_keyring_file, log_builtins_string),
    END_COMPONENT_PROVIDES();

/** List of dependencies */
BEGIN_COMPONENT_REQUIRES(component_keyring_file)
REQUIRES_SERVICE(log_builtins), REQUIRES_SERVICE(log_builtins_string),
    REQUIRES_SERVICE(registry_registration),
    REQUIRES_SERVICE_IMPLEMENTATION_AS(registry_registration,
                                       mysql_minimal_chassis_no_lock,
                                       mysql_service_registration_no_lock),
    REQUIRES_SERVICE_IMPLEMENTATION_AS(registry, mysql_minimal_chassis_no_lock,
                                       mysql_service_registry_no_lock),
    END_COMPONENT_REQUIRES();

/** Component description */
BEGIN_COMPONENT_METADATA(component_keyring_file)
METADATA("mysql.author", "Oracle Corporation"),
    METADATA("mysql.license", "GPL"),
    METADATA("component_keyring_file_service", "1"), END_COMPONENT_METADATA();

/** Component declaration */
DECLARE_COMPONENT(component_keyring_file, "component_keyring_file")
keyring_file::keyring_file_init,
    keyring_file::keyring_file_deinit END_DECLARE_COMPONENT();

/** Component contained in this library */
DECLARE_LIBRARY_COMPONENTS &COMPONENT_REF(component_keyring_file)
    END_DECLARE_LIBRARY_COMPONENTS
