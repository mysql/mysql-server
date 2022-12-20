/*
 Copyright (c) 2022, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <iostream>
#include <stdexcept>
#include <vector>

#include "mysql/harness/vt100.h"

#include "bootstrap_arguments.h"
#include "bootstrap_configurator.h"
#include "bootstrap_mode.h"
#include "process_launcher_ex.h"

int main(int argc, char **argv) {
  if (argc < 1) return -1;

  try {
    CmdArguments arguments;
    BootstrapArguments application_arguments;
    for (int arg = 0; arg < argc; ++arg) arguments.emplace_back(argv[arg]);

    application_arguments.analyze(arguments);

    if (application_arguments.bootstrap_mode.should_start_router()) {
      ProcessLauncher pl{application_arguments.path_router_application_.str(),
                         application_arguments.router_arguments,
                         {}};
      pl.start(true);
      auto status = pl.wait_until_end();
      if (EXIT_SUCCESS != status) return status;
    }

    if (application_arguments.version) return 0;

    if (application_arguments.help) {
      std::cout << std::endl << "# MySQL REST Service options" << std::endl;
      std::cout << "  --mode <all|bootstrap|mrs>" << std::endl;
      std::cout
          << "        Select the configuration mode, either if router should"
          << std::endl;
      std::cout << "        `bootstrap` or configure `mrs` (default: all)."
                << std::endl;

      std::cout << "  --mrs-metadata-account <USER_NAME>" << std::endl;
      std::cout << "        Select MySQL Server account, which MRS should use "
                << std::endl
                << "        for meta-data-schema access." << std::endl;
      std::cout << "  --mrs-data-account <USER_NAME>" << std::endl;
      std::cout << "        Select MySQL Server account, which MRS should use "
                   "for accessing "
                << std::endl
                << "        the user tables." << std::endl;
      std::cout << "  --mrs-secret <SECRET>" << std::endl;
      std::cout << "        Enables JWT token, by configuring SECRET which "
                << std::endl;
      std::cout << "        is going to use as SEED for token encryption."
                << std::endl;
      return 0;
    }

    if (application_arguments.bootstrap_mode.should_configure_mrs()) {
      std::cout << Vt100::foreground(Vt100::Color::Yellow)
                << "# Configuring `MRS` plugin..."
                << Vt100::render(Vt100::Render::ForegroundDefault) << std::endl;
      BootstrapConfigurator configurator{&application_arguments};

      bool if_not_exists =
          (application_arguments.user_options.account_create ==
           "if-not-exists") ||
          application_arguments.mrs_metadata_account.user.empty();

      if (configurator.can_configure()) {
        std::cout << "- Creating account(s) "
                  << (if_not_exists ? "(only those that are needed, if any)"
                                    : "")
                  << "\n";
        configurator.create_mrs_users();

        std::cout << "- Storing account in keyring\n";
        configurator.store_mrs_data_in_keyring();

        std::cout << "- Adjusting configuration file "
                  << configurator.get_generated_configuration_file() << "\n";
        configurator.store_configuration();
      }
    }
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << "\n";
  }

  return 0;
}
