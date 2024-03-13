/*
   Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

/* <openssl/applink.c> is included in client library */

#include <my_dbug.h> /* DEBUG macros */
#include <my_sys.h>  /* MY_INIT */
#include <scope_guard.h>

#include "components.h"
#include "options.h"
#include "utilities.h"

using components::deinit_components_subsystem;
using components::Destination_keyring_services;
using components::init_components_subsystem;
using components::Keyring_component_load;
using components::Keyring_migrate;
using components::Source_keyring_services;

using options::deinit_connection_basic;
using options::init_connection_basic;
using options::Options;
using options::process_options;

Log log_debug(std::cout, "DEBUG");
Log log_info(std::cout, "NOTE");
Log log_warning(std::cerr, "WARNING");
Log log_error(std::cerr, "ERROR");

class Migration_setup {
 public:
  explicit Migration_setup(const char *progname) {
    MY_INIT(progname);
    init_components_subsystem();
    init_connection_basic();
    log_debug.enabled(false);
  }

  ~Migration_setup() {
    deinit_connection_basic();
    deinit_components_subsystem();
    my_end(0);
  }
};

int main(int argc, char **argv) {
  /* Initialization */
  const Migration_setup migration_setup(argv[0]);
  DBUG_TRACE;
  DBUG_PROCESS(argv[0]);

  int exit_code;
  if (!process_options(&argc, &argv, exit_code)) {
    return exit_code;
  }

  {
    constexpr int exit_status = EXIT_FAILURE;
    const Keyring_component_load source_component_load(
        Options::s_source_keyring, "source");

    if (!source_component_load.ok()) {
      log_error << "Error loading source keyring component. Exiting."
                << std::endl;
      return exit_status;
    }
    const Keyring_component_load destination_component_load(
        Options::s_destination_keyring, "destination");

    if (!destination_component_load.ok()) {
      log_error << "Error loading destination keyring component. Exiting."
                << std::endl;
      return exit_status;
    }
    Source_keyring_services source_service(
        Options::s_source_keyring,
        Options::s_source_keyring_configuration_dir
            ? Options::s_source_keyring_configuration_dir
            : "");
    if (!source_service.ok()) {
      log_error << "Failed to load required services from source keyring. "
                   "Exiting."
                << std::endl;
      return exit_status;
    }
    Destination_keyring_services destination_service(
        Options::s_destination_keyring,
        Options::s_destination_keyring_configuration_dir != nullptr
            ? Options::s_destination_keyring_configuration_dir
            : "");

    if (!destination_service.ok()) {
      log_error << "Failed to load required services from destination "
                   "keyring. Exiting."
                << std::endl;
      return exit_status;
    }
    Keyring_migrate keyring_migrate(source_service, destination_service,
                                    Options::s_online_migration);
    if (!keyring_migrate.ok()) {
      log_error << "Error migrating keys from: " << Options::s_source_keyring
                << " to: " << Options::s_destination_keyring
                << ". See log for more details" << std::endl;
      return exit_status;
    }
    log_info << "Successfully loaded source and destination "
                "keyrings. Initiating migration."
             << std::endl;
    if (!keyring_migrate.migrate_keys()) {
      log_error << "Error migrating keys from: " << Options::s_source_keyring
                << " to: " << Options::s_destination_keyring
                << ". See log for more details" << std::endl;
      return exit_status;
    }
    log_info << "Key migration successful." << std::endl;
  }
  return EXIT_SUCCESS;
}
