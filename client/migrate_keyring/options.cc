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

#include <sys/types.h>

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <utility>

#include <my_alloc.h>                 /* MEM_ROOT */
#include <my_default.h>               /* print_defaults */
#include <my_getopt.h>                /* Options handling */
#include <my_inttypes.h>              /* typedefs */
#include <my_macros.h>                /* STRINGIFY_ARG */
#include <mysql.h>                    /* MYSQL */
#include <mysql/service_mysql_alloc.h>/* my_strdup */
#include <mysql_com.h>                /* get_tty_password */
#include <print_version.h>            /* print_version */
#include <typelib.h>                  /* find_type_or_exit */
#include <welcome_copyright_notice.h> /* ORACLE_WELCOME_COPYRIGHT_NOTICE */
#include "m_string.h"
#include "mysql/strings/m_ctype.h" /* Character set */
#include "nulls.h"
#include "template_utils.h"

#include "options.h"
#include "utilities.h"

/* TLS variables */
#include "client/include/sslopt-vars.h"

namespace options {

/** MEM_ROOT for arguments */
static MEM_ROOT argv_alloc{PSI_NOT_INSTRUMENTED, 512};

enum migration_options {
  OPT_COMPONENT_DIR = 512,
  OPT_SOURCE_KEYRING,
  OPT_SOURCE_KEYRING_CONFIG_DIR,
  OPT_DESTINATION_KEYRING,
  OPT_DESTINATION_KEYRING_CONFIG_DIR,
  OPT_ONLINE_MIGRATION,
  OPT_SSL_MODE,
  OPT_SSL_CA,
  OPT_SSL_CAPATH,
  OPT_SSL_CERT,
  OPT_SSL_CIPHER,
  OPT_SSL_KEY,
  OPT_SSL_CRL,
  OPT_SSL_CRLPATH,
  OPT_TLS_VERSION,
  OPT_SSL_FIPS_MODE,
  OPT_TLS_CIPHERSUITES,
  OPT_SERVER_PUBLIC_KEY,
  OPT_SSL_SESSION_DATA,
  OPT_SSL_SESSION_DATA_CONTINUE_ON_FAILED_REUSE,
  OPT_TLS_SNI_SERVERNAME,
  /* Add new value above this */
  OPT_LAST
};

bool Options::s_help = false;
bool Options::s_verbose = false;
char *Options::s_component_dir = nullptr;
char *Options::s_source_keyring = nullptr;
char *Options::s_source_keyring_configuration_dir = nullptr;
char *Options::s_destination_keyring = nullptr;
char *Options::s_destination_keyring_configuration_dir = nullptr;
bool Options::s_online_migration = false;
char *Options::s_hostname = nullptr;
unsigned int Options::s_port = 0;
char *Options::s_username = nullptr;
char *Options::s_password = nullptr;
char *Options::s_socket = nullptr;
bool Options::s_tty_password = false;

/* Caching sha2 password variables */
#include "client/include/caching_sha2_passwordopt-vars.h"

/** Options group */
static const char *load_default_groups[] = {"mysql_migrate_keyring", nullptr};

/** Command line options */
static struct my_option my_long_options[] = {
    {"help", '?', "Display this help and exit.", nullptr, nullptr, nullptr,
     GET_NO_ARG, NO_ARG, 0, 0, 0, nullptr, 0, nullptr},
    {"version", 'V', "Output version information and exit.", nullptr, nullptr,
     nullptr, GET_NO_ARG, NO_ARG, 0, 0, 0, nullptr, 0, nullptr},
    {"component_dir", OPT_COMPONENT_DIR, "Directory for components/plugins.",
     &Options::s_component_dir, &Options::s_component_dir, nullptr, GET_STR,
     REQUIRED_ARG, 0, 0, 0, nullptr, 0, nullptr},
    {"source_keyring", OPT_SOURCE_KEYRING,
     "Source keyring name (without extension)", &Options::s_source_keyring,
     &Options::s_source_keyring, nullptr, GET_STR, REQUIRED_ARG, 0, 0, 0,
     nullptr, 0, nullptr},
    {"source_keyring_configuration_dir", OPT_SOURCE_KEYRING_CONFIG_DIR,
     "Source keyring configuration directory",
     &Options::s_source_keyring_configuration_dir,
     &Options::s_source_keyring_configuration_dir, nullptr, GET_STR,
     REQUIRED_ARG, 0, 0, 0, nullptr, 0, nullptr},
    {"destination_keyring", OPT_DESTINATION_KEYRING,
     "Destination keyring component name (without extension)",
     &Options::s_destination_keyring, &Options::s_destination_keyring, nullptr,
     GET_STR, REQUIRED_ARG, 0, 0, 0, nullptr, 0, nullptr},
    {"destination_keyring_configuration_dir",
     OPT_DESTINATION_KEYRING_CONFIG_DIR,
     "Destination keyring configuration directory",
     &Options::s_destination_keyring_configuration_dir,
     &Options::s_destination_keyring_configuration_dir, nullptr, GET_STR,
     REQUIRED_ARG, 0, 0, 0, nullptr, 0, nullptr},
    {"online_migration", OPT_ONLINE_MIGRATION,
     "Signal the utility that source of migration is an active server",
     &Options::s_online_migration, &Options::s_online_migration, nullptr,
     GET_BOOL, NO_ARG, 0, 0, 0, nullptr, 0, nullptr},
    {"host", 'h', "Connect to host.", &Options::s_hostname,
     &Options::s_hostname, nullptr, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0,
     nullptr, 0, nullptr},
    {"port", 'P',
     "Port number to use for connection or 0 for default to, in "
     "order of preference, my.cnf, $MYSQL_TCP_PORT, "
#if MYSQL_PORT_DEFAULT == 0
     "/etc/services, "
#endif
     "built-in default (" STRINGIFY_ARG(MYSQL_PORT) ").",
     &Options::s_port, &Options::s_port, nullptr, GET_UINT, REQUIRED_ARG, 0, 0,
     0, nullptr, 0, nullptr},
#ifndef _WIN32
    {"socket", 'S', "The socket file to use for connection.",
     &Options::s_socket, &Options::s_socket, nullptr, GET_STR_ALLOC,
     REQUIRED_ARG, 0, 0, 0, nullptr, 0, nullptr},
#endif  //  _WIN32
    {"user", 'u', "User for login if not current user.", &Options::s_username,
     &Options::s_username, nullptr, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0,
     nullptr, 0, nullptr},
    {"password", 'p',
     "Password to use when connecting to server. If password is not given it's "
     "asked from the tty.",
     nullptr, nullptr, nullptr, GET_PASSWORD, OPT_ARG, 0, 0, 0, nullptr, 0,
     nullptr},
/* TLS options */
#include "client/include/sslopt-longopts.h"
/* Caching sha2 password options */
#include "client/include/caching_sha2_passwordopt-longopts.h"
    {"verbose", 'v', "Write more.", nullptr, nullptr, nullptr, GET_NO_ARG,
     NO_ARG, 0, 0, 0, nullptr, 0, nullptr},
    /* Must be the last one */
    {nullptr, 0, nullptr, nullptr, nullptr, nullptr, GET_NO_ARG, NO_ARG, 0, 0,
     0, nullptr, 0, nullptr}};

static void usage(bool version_only) {
  print_version();
  if (version_only) return;
  std::cout << ORACLE_WELCOME_COPYRIGHT_NOTICE("2021") << std::endl;
  std::cout << "MySQL Keyring Migration Utility" << std::endl;
  std::cout << "Usage: " << my_progname << " [OPTIONS] " << std::endl;
  my_print_help(my_long_options);
  print_defaults("my", load_default_groups);
  my_print_variables(my_long_options);
}

bool get_one_option(int optid, const struct my_option *opt, char *argument) {
  switch (optid) {
    case 'V':
      Options::s_help = true;
      usage(true);
      break;
    case 'I':
      [[fallthrough]];
    case '?':
      Options::s_help = true;
      usage(false);
      break;
    case 'v':
      log_debug.enabled(!(argument == disabled_my_option));
      break;
    case 'p':
      if (argument == disabled_my_option) {
        // Don't require password
        static char empty_password[] = {'\0'};
        assert(empty_password[0] ==
               '\0');  // Check that it has not been overwritten
        argument = empty_password;
      }
      if (argument) {
        char *start = argument;
        my_free(Options::s_password);
        Options::s_password =
            my_strdup(PSI_NOT_INSTRUMENTED, argument, MYF(MY_FAE));
        while (*argument) *argument++ = 'x'; /* Destroy argument */
        if (*start) start[1] = 0;            /* Cut length of argument */
        Options::s_tty_password = false;
      } else
        Options::s_tty_password = true;
      break;
/* Handle TLS options */
#include "client/include/sslopt-case.h"
  }
  return false;
}

static bool check_options_for_sanity() {
  if (Options::s_component_dir == nullptr || !*Options::s_component_dir ||
      Options::s_source_keyring == nullptr || !*Options::s_source_keyring ||
      Options::s_destination_keyring == nullptr ||
      !*Options::s_destination_keyring) {
    log_error << "Location of components (--component-dir) and details of "
                 "source (--source-keyirng) and destination "
                 "(--destination-keyring) components is mandatory"
              << std::endl;
    return false;
  }

  if (strcmp(Options::s_source_keyring, Options::s_destination_keyring) == 0) {
    log_error << "Source and destination cannot be the same." << std::endl;
    return false;
  }
  return true;
}

static bool get_options(int argc, char **argv, int &exit_code) {
  exit_code = handle_options(&argc, &argv, my_long_options, get_one_option);
  if (exit_code != 0) {
    log_error << "Failed to parse command line arguments." << std::endl;
    return false;
  }

  if (Options::s_help == true) {
    exit_code = EXIT_SUCCESS;
    return false;
  }

  if (check_options_for_sanity() == false) return false;

  if (Options::s_tty_password) Options::s_password = get_tty_password(NullS);

  return true;
}

bool process_options(int *argc, char ***argv, int &exit_code) {
  exit_code = 0;
#ifdef _WIN32
  /* Convert command line parameters from UTF16LE to UTF8MB4. */
  my_win_translate_command_line_args(&my_charset_utf8mb4_bin, argc, argv);
#endif

  my_getopt_use_args_separator = true;
  if (load_defaults("my", load_default_groups, argc, argv, &argv_alloc)) {
    log_error << "Failed to load default options groups" << std::endl;
    return false;
  }
  my_getopt_use_args_separator = false;

  const bool save_skip_unknown = my_getopt_skip_unknown;
  my_getopt_skip_unknown = true;
  const bool ret = get_options(*argc, *argv, exit_code);
  my_getopt_skip_unknown = save_skip_unknown;
  return ret;
}

/* MYSQL Handle used to connect to an active server */

void init_connection_basic() {}
void deinit_connection_basic() {
  if (Options::s_password != nullptr) {
    char *start = Options::s_password;
    while (*start) *start++ = 'x';
    my_free(Options::s_password);
    Options::s_password = nullptr;
  }
}

const char *default_charset = MYSQL_AUTODETECT_CHARSET_NAME;

Mysql_connection::Mysql_connection(bool connect) : ok_(false), mysql(nullptr) {
  if (connect == false) return;
  mysql_library_init(0, nullptr, nullptr);
  mysql = new (std::nothrow) MYSQL();
  if (mysql == nullptr) {
    log_error << "Failed to allocate memory for MYSQL structure" << std::endl;
    return;
  }
  if (mysql_init(mysql) == nullptr) {
    log_error << " Failed to initialize MySQL connection structure"
              << std::endl;
    return;
  }

  if (SSL_SET_OPTIONS(mysql) != 0) {
    log_error << "Failed to set SSL options" << std::endl;
    return;
  }

  mysql_options(mysql, MYSQL_SET_CHARSET_NAME, default_charset);
  if (Options::s_component_dir != nullptr && *Options::s_component_dir) {
    if (mysql_options(mysql, MYSQL_PLUGIN_DIR, Options::s_component_dir) != 0) {
      log_error << "Failed to set plugin directory" << std::endl;
      return;
    }
  }

  if (mysql_options(mysql, MYSQL_OPT_CONNECT_ATTR_RESET, nullptr) != 0) {
    log_error << "Failed to reset connection attributes" << std::endl;
    return;
  }
  if (mysql_options4(mysql, MYSQL_OPT_CONNECT_ATTR_ADD, "program_name",
                     "mysql_migrate_keyring") != 0) {
    log_error << "Failed to add program name to connection attributes"
              << std::endl;
    return;
  }

  set_server_public_key(mysql);
  set_get_server_public_key_option(mysql);

  if (!mysql_real_connect(mysql, Options::s_hostname, Options::s_username,
                          Options::s_password, NullS, Options::s_port,
                          Options::s_socket, CLIENT_REMEMBER_OPTIONS)) {
    log_error << "Failed to connect to server. Received error: "
              << mysql_error(mysql) << std::endl;
    return;
  }
  if (ssl_client_check_post_connect_ssl_setup(
          mysql, [](const char *err) { log_error << err << std::endl; }))
    return;

  log_info << "Successfully connected to MySQL server" << std::endl;

  ok_ = true;
}

Mysql_connection::~Mysql_connection() {
  if (mysql != nullptr) {
    mysql_close(mysql);
    delete mysql;
    mysql = nullptr;
  }
  mysql_library_end();
  ok_ = false;
}

bool Mysql_connection::execute(std::string command) {
  if (!ok_) {
    log_error << "Connection to MySQL server is not initialized." << std::endl;
    return false;
  }
  if (mysql_real_query(mysql, command.c_str(), command.length())) {
    log_error << "Failed to execute: " << command
              << ". Server error: " << mysql_error(mysql) << std::endl;
    return false;
  }
  return true;
}

}  // namespace options
