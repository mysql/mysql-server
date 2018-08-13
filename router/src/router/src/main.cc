/*
  Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

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

/**
  @page mysql_router_mainpage MySQL Router 8.0

  MySQL Router 8.0
  ================

  This is a release of MySQL Router.

  For the avoidance of doubt, this particular copy of the software
  is released under the version 2 of the GNU General Public License.
  MySQL Router is brought to you by Oracle.

  Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.




  @subpage mysql_harness_internal_notes

  @subpage mysql_protocol_trace_replayer

*/

#define MYSQL_ROUTER_LOG_DOMAIN \
  ::mysql_harness::logging::kMainLogger  // must precede #include "logging.h"

#include <mysql.h>
#include <iostream>
#include "common.h"
#include "dim.h"
#include "mysql/harness/loader_config.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/logging/registry.h"
#include "mysql_session.h"
#include "random_generator.h"
#include "router_app.h"
#include "utils.h"
#include "windows/main-windows.h"
IMPORT_LOG_FUNCTIONS()

/** @brief Initialise Dependency Injection Manager (DIM)
 *
 * This is the place to initialise all the DI stuff used thoroughout our
 * application. (well, maybe we'll want plugins to init their own stuff, we'll
 * see).
 *
 * Naturally, unit tests will not run this code, as they will initialise the
 * objects they need their own way.
 */
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

  // MySQLSession
  dim.set_MySQLSession([]() { return new mysqlrouter::MySQLSession(); },
                       std::default_delete<mysqlrouter::MySQLSession>());

  // Ofstream
  dim.set_Ofstream([]() { return new mysqlrouter::RealOfstream(); },
                   std::default_delete<mysqlrouter::Ofstream>());

  // logging facility
  dim.set_LoggingRegistry(
      []() {
        static mysql_harness::logging::Registry registry;
        return &registry;
      },
      [](mysql_harness::logging::Registry *) {}  // don't delete our static!
  );
}

int real_main(int argc, char **argv) {
  mysql_harness::rename_thread("main");
  init_DIM();

  // initialize logger to log to stderr. After reading configuration inside of
  // MySQLRouter::start(), it will be re-initialized according to information in
  // the configuration file
  mysql_harness::LoaderConfig config(mysql_harness::Config::allow_keys);
  MySQLRouter::init_main_logger(config, true);  // true = raw logging mode

  // TODO This is very ugly, it should not be a global. It's defined in
  // config_generator.cc and
  //      used in find_executable_path() to provide path to Router binary when
  //      generating start.sh.
  extern std::string g_program_name;
  g_program_name = argv[0];

  if (mysql_library_init(argc, argv, NULL)) {
    log_error("Could not initialize MySQL library");
    return 1;
  }

  int result = 0;
  try {
    MySQLRouter router(argc, argv);
    // This nested try/catch block is necessary in Windows, to
    // workaround a crash that occurs when an exception is thrown from
    // a plugin (e.g. routing_plugin_tests)
    try {
      router.start();
    } catch (const std::invalid_argument &exc) {
      log_error("Configuration error: %s", exc.what());
      result = 1;
    } catch (const std::runtime_error &exc) {
      log_error("Error: %s", exc.what());
      result = 1;
    } catch (const silent_exception &) {
    }
  } catch (const std::invalid_argument &exc) {
    log_error("Configuration error: %s", exc.what());
    result = 1;
  } catch (const std::runtime_error &exc) {
    log_error("Error: %s", exc.what());
    result = 1;
  } catch (const mysql_harness::syntax_error &exc) {
    log_error("Configuration syntax error: %s", exc.what());
  } catch (const silent_exception &) {
  } catch (const std::exception &exc) {
    log_error("Error: %s", exc.what());
    result = 1;
  }

  // We should deinitialize mysql-lib but we can't do it safely here until
  // we do WL9558 "Plugin life-cycle that support graceful shutdown and
  // restart." Currently we can get here while there are still some threads
  // running (like metadata_cache thread that is managed by the global
  // g_metadata_cache) that still use mysql-lib, which leads to crash.
  mysql_library_end();

  return result;
}

int main(int argc, char **argv) {
#ifdef _WIN32
  return proxy_main(real_main, argc, argv);
#else
  return real_main(argc, argv);
#endif
}
