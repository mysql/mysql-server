/*
 Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#define MYSQL_ROUTER_LOG_DOMAIN \
  ::mysql_harness::logging::kMainLogger  // must precede #include "logging.h"

#include <iostream>
#include <stdexcept>
#include <vector>

#include "bootstrap_configurator.h"
#include "mysql/harness/logging/registry.h"
#include "mysql/harness/tty.h"
#include "mysql/harness/utility/string.h"
#include "mysql/harness/vt100.h"
#include "mysql/harness/vt100_filter.h"
#include "print_version.h"
#include "welcome_copyright_notice.h"

IMPORT_LOG_FUNCTIONS()

class Tty_init {
 public:
  Tty_init()
      : cout_tty(Tty::fd_from_stream(std::cout)),
        cerr_tty(Tty::fd_from_stream(std::cout)),
        filtered_out_streambuf(std::cout.rdbuf(),
                               !(cout_tty.is_tty() && cout_tty.ensure_vt100())),
        filtered_err_streambuf(std::cerr.rdbuf(),
                               !(cerr_tty.is_tty() && cerr_tty.ensure_vt100())),
        out(&filtered_out_streambuf),
        err(&filtered_err_streambuf) {}

 private:
  Tty cout_tty;
  Tty cerr_tty;
  Vt100Filter filtered_out_streambuf;
  Vt100Filter filtered_err_streambuf;

 public:
  std::ostream out;
  std::ostream err;
};

static void init_DIM() {
  mysql_harness::DIM &dim = mysql_harness::DIM::instance();

  // RandomGenerator
  dim.set_RandomGenerator(
      []() {
        static mysql_harness::RandomGenerator rg;
        return &rg;
      },
      [](mysql_harness::RandomGeneratorInterface *) {}
      // don't delete our static!
  );

  //  // MySQLSession
  //  dim.set_MySQLSession(
  //      []() {
  //        return new mysqlrouter::MySQLSession(
  //            std::make_unique<
  //                mysqlrouter::MySQLSession::LoggingStrategyDebugLogger>());
  //      },
  //      std::default_delete<mysqlrouter::MySQLSession>());
}

static void preconfig_log_init() noexcept {
  using namespace mysql_harness;
  // setup registry object in DIM
  {
    DIM &dim = DIM::instance();
    dim.set_LoggingRegistry(
        []() {
          static logging::Registry registry;
          return &registry;
        },
        [](logging::Registry *) {}  // don't delete our static!
    );
  }

  // initialize logger to log to stderr or OS logger. After reading
  // configuration inside of MySQLRouter::start(), it will be re-initialized
  // according to information in the configuration file
  {
    mysql_harness::LoaderConfig config(mysql_harness::Config::allow_keys);
    try {
      BootstrapConfigurator::init_main_logger(config,
                                              true);  // true = raw logging mode
    } catch (const std::runtime_error &) {
      // If log init fails, there's not much we can do here (no way to log the
      // error) except to catch this exception to prevent it from bubbling up
      // to std::terminate()
    }
  }
}

int main(int argc, char **argv) {
  Tty_init tty;
  preconfig_log_init();

  init_DIM();

  try {
    BootstrapConfigurator configurator{tty.out, tty.err};
    configurator.init(argc, argv);

    configurator.run();

#if 0
    std::string mysql_password;

    // Connect to DB, prompt DB password if needed, check for metadata
    configurator.connect(&mysql_password);

    if (configurator.has_innodb_cluster_metadata()) {
      if (application_arguments.standalone) {
        throw std::invalid_argument(
            "Option --standalone not allowed when bootstrapping with InnoDB "
            "Cluster");
      }
    } else {
      if (!application_arguments.standalone) {
        if (application_arguments.bootstrap_mrs)
          throw std::invalid_argument(
              "InnoDB Cluster metadata not found. To use MySQL REST Service in "
              "standalone server mode, use the --standalone option");
        else
          throw std::invalid_argument("InnoDB Cluster metadata not found.");
      }
    }

    if (configurator.can_configure_mrs()) {
      configurator.check_mrs_metadata();
    }

    if (configurator.needs_configure_routing()) {
      ProcessLauncher pl{application_arguments.path_router_application_.str(),
                         application_arguments.router_arguments,
                         {}};
      pl.start(true, false);

      // feed password
      pl.write(&mysql_password[0], mysql_password.size());
      pl.write("\n", 1);

      auto status = pl.wait_until_end();
      if (EXIT_SUCCESS != status) return status;
    }

    if (application_arguments.bootstrap_mrs) {
      configurator.load_configuration();

      if (configurator.can_configure_mrs()) {
        bool if_not_exists =
            (application_arguments.user_options.account_create ==
             "if-not-exists") ||
            application_arguments.mrs_metadata_account.user.empty();

        configurator.configure_mrs(if_not_exists);
      }
    }
#endif
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }

  return 0;
}
