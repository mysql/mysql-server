/*
 * Copyright (c) 2017, 2022, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#include "plugin/x/tests/driver/driver_command_line_options.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <iostream>
#include <stdexcept>

#include "my_dbug.h"                   // NOLINT(build/include_subdir)
#include "print_version.h"             // NOLINT(build/include_subdir)
#include "welcome_copyright_notice.h"  // NOLINT(build/include_subdir)

#include "plugin/x/generated/mysqlx_version.h"
#include "plugin/x/src/helper/string_case.h"
#include "plugin/x/src/helper/to_string.h"
#include "plugin/x/tests/driver/common/utils_string_parsing.h"
#include "plugin/x/tests/driver/processor/commands/command.h"
#include "plugin/x/tests/driver/processor/commands/mysqlxtest_error_names.h"

namespace {

std::vector<std::string> multivalue_argument(const std::string &value) {
  if (value.empty()) return {};

  std::vector<std::string> result;

  aux::split(result, value, ",", false);

  return result;
}

int32_t integer_argument(const std::string &value, int32_t *exit_code) {
  try {
    return std::stoi(value);
  } catch (std::invalid_argument &) {
    *exit_code = 1;
    std::cerr << "'" << value << "' is not an integer value\n";
  }
  return 0;
}

bool boolean_argument(const std::string &value, int32_t *exit_code) {
  static const std::array<const char *, 5> k_true = {
      "t", "true", "y", "yes", "1",
  };
  static const std::array<const char *, 5> k_false = {
      "f", "false", "n", "no", "0",
  };
  const auto val = xpl::to_lower(value);
  const auto is_valid = [&val](const char *v) {
    return std::strcmp(v, val.c_str()) == 0;
  };

  if (std::any_of(k_true.begin(), k_true.end(), is_valid)) return true;
  if (std::any_of(k_false.begin(), k_false.end(), is_valid)) return false;
  *exit_code = 1;
  std::cerr << "'" << value << "' is not an boolean value\n";
  return false;
}

}  // namespace

void Driver_command_line_options::print_version() { ::print_version(); }

void Driver_command_line_options::print_help() {
  print_version();
  std::cout << (ORACLE_WELCOME_COPYRIGHT_NOTICE("2015")) << std::endl;

  std::cout << "mysqlxtest <options> [SCHEMA]\n";
  std::cout << "Options:\n";
  std::cout << "-f, --file=<file>     Reads input from file\n";
  std::cout << "-I, --import=<dir>    "
               "Reads macro files from dir; required by -->import\n";
  std::cout << "--sql=<SQL>           "
               "Use SQL as input and execute it like in -->sql block\n";
  std::cout << "-e=<SQL>, --execute=<SQL> Aliases for \"--sql\" option\n";
  std::cout << "-n, --no-auth         Skip authentication which is required "
               "by -->sql block (run mode)\n";
  std::cout << "--no-connect-attrs    "
               "Skip send session connection attributes\n";
  std::cout << "--plain-auth          "
               "Use PLAIN text authentication mechanism\n";
  std::cout << "--cached-auth         "
               "Use SHA256_MEMORY authentication mechanism\n";
  std::cout << "--mysql41-auth        Use MYSQL41 authentication mechanism\n";
  std::cout << "--using-cap-auth      Get capabilities to check which\n"
            << "                      authentication mechanism are supported\n";
  std::cout << "--mysql57-compatible  Use features that are 5.7 compatible:\n";
  std::cout << "                      * limit auth-mechanisms\n";
  std::cout << "-u, --user=<user>     Connection user\n";
  std::cout << "-p, --password=<pass> Connection password\n";
  std::cout << "-h, --host=<host>     Connection host\n";
  std::cout << "-P, --port=<port>     Connection port (default:"
            << MYSQLX_TCP_PORT << ")\n";
  std::cout << "--ipv=<mode>          Force internet protocol (default:4):\n";
  std::cout << "                      0 - allow system to resolve IPv6 and "
               "IPv4, for example\n";
  std::cout << "                          resolving of 'localhost' can "
               "return both '::1' and '127.0.0.1'\n";
  std::cout << "                      4 - allow system to resolve only IPv4, "
               "for example\n";
  std::cout << "                          resolving of 'localhost' is going "
               "to return '127.0.0.1'\n";
  std::cout << "                      6 - allow system to resolve only IPv6, "
               "for example\n";
  std::cout << "                          resolving of 'localhost' is going "
               "to return '::1'\n";
  std::cout << "-t, --timeout=<ms>    I/O timeouts in milliseconds\n";
  std::cout << "--close-no-sync       Do not wait for connection to be "
               "closed by server(disconnect first)\n";
  std::cout << "--schema=<schema>     Default schema to connect to\n";
  std::cout << "--uri=<uri>           Connection URI\n";
  std::cout << "                      URI takes precedence before options "
               "like: user, host, password, port\n";
  std::cout << "--socket=<file>       Connection through UNIX socket\n";
  std::cout << "--use-socket          "
               "Connection through UNIX socket, using default file name '"
            << MYSQLX_UNIX_ADDR << "'\n";
  std::cout << "                      --use-socket* options take precedence "
               "before options like: uri, user,\n";
  std::cout << "                      host, password, port\n";
  std::cout << "--compression-mode=<mode>  Enable/disable compression "
               "negotiation algorithm (default: \"DISABLED\")\n";
  std::cout << "                      \"DISABLED\" - do not negotiate "
               "compression\n";
  std::cout << "                      \"PREFERRED\" - if server doesn't "
               "support selected compression, continue without compression.\n";
  std::cout << "                      \"REQUIRED\" - if server doesn't support "
               "selected compression, fail the connection.\n";
  std::cout
      << "--compression-algorithm=<algo[,algo...]>  Try to negotiate specified"
         " compression algorithm with the server "
         "(default:\"DEFLATE_STREAM,LZ4_MESSAGE,ZSTD_STREAM\")\n";
  std::cout << "                      \"\" - compression not enabled\n";
  std::cout << "                      \"DEFLATE_STREAM\" - zlib compression "
               "enabled\n";
  std::cout
      << "                      \"LZ4_MESSAGE\" - lz4f compression enabled\n";
  std::cout
      << "                      \"ZSTD_STREAM\" - zstd compression enabled\n";
  std::cout << "--compression-combine-mixed-messages=<0/1>  If 1, server is "
               "allowed to combine different message types into a compressed "
               "message (default: 1)\n";
  std::cout << "--compression-max-combine-messages=<N>  "
               "If set, the server MUST not store more than N uncompressed "
               "messages into a compressed message (default: no limit)\n";
  std::cout << "--compression-level=<N>  "
               "If set, the server MUST compress messages with given level N; "
               "otherwise the server use default level "
               "(depend on compression algorithm)\n";
  std::cout << "--ssl-mode            SSL configuration (default: \"\")\n";
  std::cout << "                      \"\" - require encryption when at last "
               "one ssl option is set, otherwise is should be disabled.\n";
  std::cout << "                      \"PREFERRED\" - encryption is optional, "
               "client tries to set it up\n";
  std::cout << "                      \"DISABLED\" - encryption is disabled\n";
  std::cout << "                      \"REQUIRED\" - encryption is required\n";
  std::cout << "                      "
               "\"VERIFY_CA\" - verify server certificate\n";
  std::cout << "                      "
               "\"VERIFY_IDENTITY\" - verify certificate issuer\n";
  std::cout << "--ssl-key             X509 key in PEM format\n";
  std::cout << "--ssl-ca              CA file in PEM format\n";
  std::cout << "--ssl-ca_path         CA directory\n";
  std::cout << "--ssl-cert            X509 cert in PEM format\n";
  std::cout << "--ssl-cipher          SSL cipher to use\n";
  std::cout << "--tls-version         TLS version to use\n";
  std::cout << "--ssl-fips-mode       Fips mode to use\n";
  std::cout << "--connect-expired-password Allow expired password\n";
  std::cout << "--client-interactive  Connect in interactive mode\n";
  std::cout << "--quiet               Don't print out messages sent\n";
  std::cout << "-vVARIABLE_NAME=VALUE "
               "Set variable VARIABLE_NAME from command line\n";
  std::cout << "--fatal-errors=<0|1>  Mysqlxtest is started with ignoring or "
               "stopping on fatal error (default: 1)\n";
  std::cout << "--expect-error=<error_code> Default connection must fail with "
               "specified code (default: OFF)\n";
  std::cout << "-B, --bindump         Dump binary representation of messages "
               "sent, in format suitable for\n";
  std::cout << "--trace-protocol      Enable X Protocol tracing\n";
  std::cout << "--history-protocol=<0|1|true|false>\n";
  std::cout << "                      Enable/disable X Protocol message "
               "tracing into\n";
  std::cout << "                      history buffer, which is printed at test "
               "failure\n";
  std::cout << "--verbose             Enable extra verbose messages\n";
  std::cout << "--daemon              Work as a daemon (unix only)\n";
  std::cout << "--help                Show command line help\n";
  std::cout << "--help-commands       Show help for input commands\n";
  std::cout << "-V, --version         Show version of mysqlxtest\n";
  std::cout << "\nOnly one option that changes run mode is allowed.\n";
}

std::string Driver_command_line_options::get_socket_name() {
  return MYSQLX_UNIX_ADDR;
}

Driver_command_line_options::Driver_command_line_options(const int argc,
                                                         char **argv)
    : Command_line_options(argc, argv),
      m_run_without_auth(false),
      m_has_file(false),
      m_cap_expired_password(false),
      m_client_interactive(false),
      m_connect_attrs(true),
      m_daemon(false) {
  std::string user;

  for (int i = 1; i < argc && exit_code == 0; i++) {
    char *value;
    if (check_arg_with_value(argv, i, "--file", "-f", value)) {
      m_run_file = value;
      m_has_file = true;
    } else if (check_arg(argv, i, "--no-auth", "-n")) {
      m_run_without_auth = true;
    } else if (check_arg(argv, i, "--no-connect-attrs", nullptr)) {
      m_connect_attrs = false;
    } else if (check_arg(argv, i, "--plain-auth", nullptr)) {
      m_connection_options.auth_methods.push_back("PLAIN");
    } else if (check_arg(argv, i, "--cached-auth", nullptr)) {
      m_connection_options.auth_methods.push_back("SHA256_MEMORY");
    } else if (check_arg(argv, i, "--using-cap-auth", nullptr)) {
      m_connection_options.auth_methods.push_back("FROM_CAPABILITIES");
    } else if (check_arg(argv, i, "--mysql41-auth", nullptr)) {
      m_connection_options.auth_methods.push_back("MYSQL41");
    } else if (check_arg_with_value(argv, i, "--debug", nullptr, value)) {
#ifndef NDEBUG
      DBUG_PUSH(value);
#endif  // NDEBUG
    } else if (check_arg_with_value(argv, i, "--sql", nullptr, value)) {
      m_sql = value;
    } else if (check_arg_with_value(argv, i, "--execute", "-e", value)) {
      m_sql = value;
    } else if (check_arg_with_value(argv, i, "--password", "-p", value)) {
      m_connection_options.password = value;
    } else if (check_arg_with_value(argv, i, "--compression-mode", nullptr,
                                    value)) {
      m_connection_options.compression_mode = value;
    } else if (check_arg_with_value(argv, i, "--compression-algorithm", nullptr,
                                    value)) {
      m_connection_options.compression_algorithm = multivalue_argument(value);
    } else if (check_arg_with_value(argv, i,
                                    "--compression-combine-mixed-messages",
                                    nullptr, value)) {
      m_connection_options.compression_combine_mixed_messages =
          boolean_argument(value, &exit_code);
    } else if (check_arg_with_value(argv, i,
                                    "--compression-max-combine-messages",
                                    nullptr, value)) {
      m_connection_options.compression_max_combine_messages =
          integer_argument(value, &exit_code);
    } else if (check_arg_with_value(argv, i, "--compression-level", nullptr,
                                    value)) {
      m_connection_options.compression_level =
          integer_argument(value, &exit_code);
    } else if (check_arg_with_value(argv, i, "--ssl-mode", nullptr, value)) {
      m_connection_options.ssl_mode = value;
    } else if (check_arg_with_value(argv, i, "--ssl-key", nullptr, value)) {
      m_connection_options.ssl_key = value;
    } else if (check_arg_with_value(argv, i, "--ssl-ca", nullptr, value)) {
      m_connection_options.ssl_ca = value;
    } else if (check_arg_with_value(argv, i, "--ssl-fips-mode", nullptr,
                                    value)) {
      m_connection_options.ssl_fips_mode = value;
    } else if (check_arg_with_value(argv, i, "--ssl-ca_path", nullptr, value)) {
      m_connection_options.ssl_ca_path = value;
    } else if (check_arg_with_value(argv, i, "--ssl-cert", nullptr, value)) {
      m_connection_options.ssl_cert = value;
    } else if (check_arg_with_value(argv, i, "--ssl-cipher", nullptr, value)) {
      m_connection_options.ssl_cipher = value;
    } else if (check_arg_with_value(argv, i, "--tls-version", nullptr, value)) {
      m_connection_options.allowed_tls = value;
    } else if (check_arg_with_value(argv, i, "--host", "-h", value)) {
      m_connection_options.host = value;
    } else if (check_arg_with_value(argv, i, "--network-namespace", nullptr,
                                    value)) {
      m_connection_options.network_namespace = value;
    } else if (check_arg_with_value(argv, i, "--user", "-u", value)) {
      m_connection_options.user = value;
    } else if (check_arg_with_value(argv, i, "--uri", nullptr, value)) {
      m_uri = value;
    } else if (check_arg_with_value(argv, i, "--schema", nullptr, value)) {
      m_connection_options.schema = value;
    } else if (check_arg_with_value(argv, i, "--port", "-P", value)) {
      m_connection_options.port = integer_argument(value, &exit_code);
    } else if (check_arg_with_value(argv, i, "--ipv", nullptr, value)) {
      m_connection_options.ip_mode =
          set_protocol(integer_argument(value, &exit_code));
    } else if (check_arg_with_value(argv, i, "--timeout", "-t", value)) {
      m_connection_options.session_connect_timeout =
          m_connection_options.io_timeout = integer_argument(value, &exit_code);
    } else if (check_arg_with_value(argv, i, "--expect-error", nullptr,
                                    value)) {
      m_expected_error_code = mysqlxtest::get_error_code_by_text(value);
    } else if (check_arg_with_value(argv, i, "--fatal-errors", nullptr,
                                    value)) {
      m_context_options.m_fatal_errors = integer_argument(value, &exit_code);
    } else if (check_arg_with_value(argv, i, "--password", "-p", value)) {
      m_connection_options.password = value;
    } else if (check_arg_with_value(argv, i, "--socket", "-S", value)) {
      m_connection_options.socket = value;
    } else if (check_arg(argv, i, "--mysql57-compatible", nullptr)) {
      m_connection_options.compatible = true;
    } else if (check_arg_with_value(argv, i, nullptr, "-v", value)) {
      set_variable_option(value);
    } else if (check_arg(argv, i, "--use-socket", nullptr)) {
      m_connection_options.socket = get_socket_name();
    } else if (check_arg(argv, i, "--trace-protocol", nullptr)) {
      m_connection_options.trace_protocol = true;
    } else if (check_arg_with_value(argv, i, "--history-protocol", nullptr,
                                    value)) {
      m_connection_options.trace_protocol_history =
          boolean_argument(value, &exit_code);
    } else if (check_arg(argv, i, "--close-no-sync", nullptr)) {
      m_connection_options.dont_wait_for_disconnect = true;
    } else if (check_arg(argv, i, "--bindump", "-B")) {
      m_context_options.m_bindump = true;
    } else if (check_arg(argv, i, "--connect-expired-password", nullptr)) {
      m_cap_expired_password = true;
    } else if (check_arg(argv, i, "--client-interactive", nullptr)) {
      m_client_interactive = true;
    } else if (check_arg(argv, i, "--quiet", "-q")) {
      m_context_options.m_quiet = true;
    } else if (check_arg(argv, i, "--verbose", nullptr)) {
      m_console_options.m_be_verbose = true;
    } else if (check_arg(argv, i, "--daemon", nullptr)) {
      m_daemon = true;
#ifndef _WIN32
    } else if (check_arg(argv, i, "--color", nullptr)) {
      m_console_options.m_use_color = true;
#endif
    } else if (check_arg_with_value(argv, i, "--import", "-I", value)) {
      m_context_options.m_import_path = value;
      if (*m_context_options.m_import_path.rbegin() != FN_LIBCHAR)
        m_context_options.m_import_path += FN_LIBCHAR;
    } else if (check_arg(argv, i, "--help", "--help")) {
      print_help();
      exit_code = 1;
    } else if (check_arg(argv, i, "--help-commands", "--help-commands")) {
      print_help_commands();
      exit_code = 1;
    } else if (check_arg(argv, i, "--version", "-V")) {
      print_version();
      exit_code = 1;
    } else if (exit_code == 0) {
      const auto index_of_last_argument = argc - 1;

      if (index_of_last_argument == i && std::isalnum(argv[i][0])) {
        m_connection_options.schema = argv[i];

        break;
      }

      std::cerr << argv[0] << ": unknown option " << argv[i] << "\n";
      exit_code = 1;
      break;
    }
  }

  if (m_connection_options.port == 0)
    m_connection_options.port = MYSQLX_TCP_PORT;
  if (m_connection_options.host.empty())
    m_connection_options.host = "localhost";
}

void Driver_command_line_options::set_variable_option(
    const std::string &set_expression) {
  std::vector<std::string> args;

  aux::split(args, set_expression, "=", false);

  if (2 != args.size()) {
    std::cerr << "Wrong format expected NAME=VALUE\n";
    exit_code = 1;
    return;
  }

  m_variables[args[0]] = args[1];
}

xcl::Internet_protocol Driver_command_line_options::set_protocol(
    const int mode) {
  switch (mode) {
    case 0:
      return xcl::Internet_protocol::Any;

    case 4:
      return xcl::Internet_protocol::V4;

    case 6:
      return xcl::Internet_protocol::V6;

    default:
      std::cerr << "Wrong Internet protocol version\n";
      exit_code = 1;
      return xcl::Internet_protocol::Any;
  }
}
