/*
  Copyright (c) 2015, 2022, Oracle and/or its affiliates.

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
  @page PAGE_MYSQL_ROUTER MySQL Router

  MySQL Router

  @section SEC_MYSQL_ROUTER_BUILD Building

  MySQL Router is built in like the [MySQL Server](@ref start_source).

  In case MySQL Router shall be built without building the whole
  server from the same source

  - run cmake as before
  - build the "mysqlrouter_all" target

  like in:

  @code
  $ cmake --build . --target mysqlrouter_all
  @endcode

  It builds:

  - mysqlrouter
  - libraries
  - plugins
  - all tests

  @section SEC_MYSQL_ROUTER_TESTING Testing

  Testing MySQL Router is based on:

  - unit-tests via googletest
  - component level tests using [mysql_server_mock](@ref PAGE_MYSQL_SERVER_MOCK)

  To run only the Router related tests without running all other tests
  contained in the source tree, tell ctest to only run the tests prefixed
  with "routertest_"

  @code
  $ ctest -R routertest_
  @endcode

  @subpage PAGE_MYSQL_SERVER_MOCK
*/

#define MYSQL_ROUTER_LOG_DOMAIN \
  ::mysql_harness::logging::kMainLogger  // must precede #include "logging.h"

#include <iostream>
#include <stdexcept>

#include <mysql.h>

#include "dim.h"
#include "mysql/harness/loader_config.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/logging/registry.h"
#include "mysql/harness/tty.h"
#include "mysql/harness/vt100_filter.h"
#include "mysqlrouter/mysql_client_thread_token.h"
#include "mysqlrouter/mysql_session.h"
#include "random_generator.h"
#include "router_app.h"
#include "windows/main-windows.h"

IMPORT_LOG_FUNCTIONS()

/** @brief Initialise Dependency Injection Manager (DIM)
 *
 * Unless there's a specific reason to do it elsewhere, this is the place to
 * initialise all the DI stuff used thoroughout our application. (well, maybe
 * we'll want plugins to init their own stuff, we'll see).
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
  dim.set_MySQLSession(
      []() {
        return new mysqlrouter::MySQLSession(
            std::make_unique<
                mysqlrouter::MySQLSession::LoggingStrategyDebugLogger>());
      },
      std::default_delete<mysqlrouter::MySQLSession>());
}

static void preconfig_log_init(bool use_os_logger_initially) noexcept {
  // setup registry object in DIM
  {
    mysql_harness::DIM &dim = mysql_harness::DIM::instance();
    dim.set_LoggingRegistry(
        []() {
          static mysql_harness::logging::Registry registry;
          return &registry;
        },
        [](mysql_harness::logging::Registry *) {}  // don't delete our static!
    );
  }

  // initialize logger to log to stderr or OS logger. After reading
  // configuration inside of MySQLRouter::start(), it will be re-initialized
  // according to information in the configuration file
  {
    mysql_harness::LoaderConfig config(mysql_harness::Config::allow_keys);
    try {
      MySQLRouter::init_main_logger(config, true,  // true = raw logging mode
                                    use_os_logger_initially);
    } catch (const std::runtime_error &) {
      // If log init fails, there's not much we can do here (no way to log the
      // error) except to catch this exception to prevent it from bubbling up
      // to std::terminate()
    }
  }
}

int real_main(int argc, char **argv, bool use_os_logger_initially) {
  preconfig_log_init(use_os_logger_initially);

  init_DIM();

  mysqlrouter::MySQLClientThreadToken api_token;
  if (mysql_library_init(argc, argv, nullptr)) {
    log_error("Could not initialize MySQL library");
    return 1;
  }

  // cout is a tty?
  Tty cout_tty(Tty::fd_from_stream(std::cout));
  Vt100Filter filtered_out_streambuf(
      std::cout.rdbuf(), !(cout_tty.is_tty() && cout_tty.ensure_vt100()));
  std::ostream filtered_out_stream(&filtered_out_streambuf);

  Tty cerr_tty(Tty::fd_from_stream(std::cout));
  Vt100Filter filtered_err_streambuf(
      std::cerr.rdbuf(), !(cerr_tty.is_tty() && cerr_tty.ensure_vt100()));
  std::ostream filtered_err_stream(&filtered_err_streambuf);
  int result = 0;
  try {
    MySQLRouter router(argc, argv, filtered_out_stream, filtered_err_stream);
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
    // cleanup on shutdown
    router.stop();
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

  mysql_library_end();

  return result;
}

int main(int argc, char **argv) {
#ifdef _WIN32
  return proxy_main(real_main, argc, argv);
#else
  return real_main(argc, argv, false);  // false = log initially to STDERR
#endif
}
