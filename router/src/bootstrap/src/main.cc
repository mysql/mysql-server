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
#include "print_version.h"
#include "welcome_copyright_notice.h"

#include "bootstrap_arguments.h"
#include "bootstrap_configurator.h"
#include "bootstrap_mode.h"
#include "process_launcher_ex.h"

void print_version(BootstrapArguments &b) {
  std::string output;
  build_version(b.path_this_application_.basename().str(), &output);
  std::cout << output << std::endl;
}

void print_copyrights() {
  std::cout << ORACLE_WELCOME_COPYRIGHT_NOTICE("2015") << std::endl;
}

int main(int argc, char **argv) {
  if (argc < 1) return -1;

  try {
    CmdArguments arguments;
    BootstrapArguments application_arguments;
    for (int arg = 0; arg < argc; ++arg) arguments.emplace_back(argv[arg]);

    application_arguments.analyze(arguments);

    if (application_arguments.should_start_router()) {
      ProcessLauncher pl{application_arguments.path_router_application_.str(),
                         application_arguments.router_arguments,
                         {}};
      pl.start(true);
      auto status = pl.wait_until_end();
      if (EXIT_SUCCESS != status) return status;
    }

    if (application_arguments.version) {
      print_version(application_arguments);
      return 0;
    }

    if (application_arguments.help) {
      print_version(application_arguments);
      print_copyrights();

      std::cout << Vt100::render(Vt100::Render::Bold) << "# Usage"
                << Vt100::render(Vt100::Render::Normal) << "\n\n";
      auto app = application_arguments.path_this_application_.basename().str();
      std::cout << "      " << app << " --version|-V" << std::endl << std::endl;
      std::cout << "      " << app << " --help" << std::endl << std::endl;
      std::cout
          << "      " << app << " [--account-host=<account-host>]" << std::endl
          << "                  [--bootstrap-socket=<socket_name>]" << std::endl
          << "                  [--client-ssl-cert=<path>]" << std::endl
          << "                  [--client-ssl-cipher=<VALUE>]" << std::endl
          << "                  [--client-ssl-curves=<VALUE>]" << std::endl
          << "                  [--client-ssl-key=<path>]" << std::endl
          << "                  [--client-ssl-mode=<mode>]" << std::endl
          << "                  [--conf-base-port=<port>] [--conf-skip-tcp]"
          << std::endl
          << "                  [--conf-use-sockets] [--core-file=[<VALUE>]]"
          << std::endl
          << "                  [--connect-timeout=[<VALUE>]]" << std::endl
          << "                  [--conf-use-gr-notifications=[<VALUE>]]"
          << std::endl
          << "                  [-d|--directory=<directory>] [--force]"
          << std::endl
          << "                  [--force-password-validation]" << std::endl
          << "                  [--master-key-reader=<VALUE>]" << std::endl
          << "                  [--master-key-writer=<VALUE>] [--name=[<name>]]"
          << std::endl
          << "                  [--password-retries=[<password-retries>]]"
          << std::endl
          << "                  [--read-timeout=[<VALUE>]]" << std::endl
          << "                  [--report-host=<report-host>]" << std::endl
          << "                  [--server-ssl-ca=<path>]" << std::endl
          << "                  [--server-ssl-capath=<directory>]" << std::endl
          << "                  [--server-ssl-cipher=<VALUE>]" << std::endl
          << "                  [--server-ssl-crl=<path>]" << std::endl
          << "                  [--server-ssl-crlpath=<directory>]" << std::endl
          << "                  [--server-ssl-curves=<VALUE>]" << std::endl
          << "                  [--server-ssl-mode=<ssl-mode>]" << std::endl
          << "                  [--server-ssl-verify=<verify-mode>]"
          << std::endl
          << "                  [--ssl-ca=<path>] [--ssl-cert=<path>]"
          << std::endl
          << "                  [--ssl-cipher=<ciphers>] [--ssl-crl=<path>]"
          << std::endl
          << "                  [--ssl-crlpath=<directory>] [--ssl-key=<path>]"
          << std::endl
          << "                  [--ssl-mode=<mode>] [--tls-version=<versions>]"
          << std::endl
          << "                  [-u|--user=<username>]" << std::endl
          << "                  [--conf-set-option=<conf-set-option>]"
          << std::endl
          << "                  <server_url>" << std::endl;

      std::cout << std::endl
                << Vt100::render(Vt100::Render::Bold)
                << "# MySQL REST Service options"
                << Vt100::render(Vt100::Render::Normal) << std::endl
                << std::endl;
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

      std::cout << std::endl
                << Vt100::render(Vt100::Render::Bold) << "# Examples"
                << Vt100::render(Vt100::Render::Normal) << std::endl
                << std::endl;

      constexpr const char kStartWithSudo[]{IF_WIN("", "sudo ")};
      constexpr const char kStartWithUser[]{IF_WIN("", " --user=mysqlrouter")};

      std::cout << "Bootstrap for use with InnoDB cluster into system-wide "
                   "installation\n\n"
                << "    " << kStartWithSudo << "mysqlrouter_bootstrap "
                << kStartWithUser << " root@clusterinstance01"
                << "\n\n"
                << "Bootstrap for use with InnoDb cluster in a self-contained "
                   "directory\n\n"
                << "    "
                << "mysqlrouter_bootstrap -d myrouter root@clusterinstance01"
                << "\n\n";
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
