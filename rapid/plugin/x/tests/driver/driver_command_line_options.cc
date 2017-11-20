/*
 * Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "driver_command_line_options.h"

#include <iostream>

#include "processor/commands/command.h"
#include "mysqlx_version.h"
#include "ngs_common/to_string.h"
#include "print_version.h"
#include "welcome_copyright_notice.h"


void Driver_command_line_options::print_version() {
  ::print_version();
}

void Driver_command_line_options::print_help() {
  print_version();
  std::cout << (ORACLE_WELCOME_COPYRIGHT_NOTICE("2015")) << std::endl;

  std::cout << "mysqlxtest <options>\n";
  std::cout << "Options:\n";
  std::cout << "-f, --file=<file>     Reads input from file\n";
  std::cout << "-I, --import=<dir>    Reads macro files from dir; required "
               "by -->import\n";
  std::cout << "--sql=<SQL>           Use SQL as input and execute it like "
               "in -->sql block\n";
  std::cout << "-n, --no-auth         Skip authentication which is required "
               "by -->sql block (run mode)\n";
  std::cout
      << "--plain-auth          Use PLAIN text authentication mechanism\n";
  std::cout << "-u, --user=<user>     Connection user\n";
  std::cout << "-p, --password=<pass> Connection password\n";
  std::cout << "-h, --host=<host>     Connection host\n";
  std::cout << "-P, --port=<port>     Connection port (default:"
            << MYSQLX_TCP_PORT << ")\n";
  std::cout << "--ipv=<mode>          Force internet protocol (default:4):\n";
  std::cout << "                      0 - allow system to resolve IPv6 and "
               "IPv4, for example";
  std::cout << "                          resolving of 'localhost' can "
               "return both '::1' and '127.0.0.1'";
  std::cout << "                      4 - allow system to resolve only IPv4, "
               "for example\n";
  std::cout << "                          resolving of 'localhost' is going "
               "to return '127.0.0.1'";
  std::cout << "                      6 - allow system to resolve only IPv6, "
               "for example\n";
  std::cout << "                          resolving of 'localhost' is going "
               "to return '::1'";
  std::cout << "-t, --timeout=<ms>    I/O timeouts in milliseconds\n";
  std::cout << "--close-no-sync       Do not wait for connection to be "
               "closed by server(disconnect first)\n";
  std::cout << "--schema=<schema>     Default schema to connect to\n";
  std::cout << "--uri=<uri>           Connection URI\n";
  std::cout << "                      URI takes precedence before options "
               "like: user, host, password, port\n";
  std::cout << "--socket=<file>       Connection through UNIX socket\n";
  std::cout << "--use-socket          Connection through UNIX socket, using "
               "default file name '" << MYSQLX_UNIX_ADDR << "'\n";
  std::cout << "                      --use-socket* options take precedence "
               "before options like: uri, user,\n";
  std::cout << "                      host, password, port\n";
  std::cout << "--ssl-mode            SSL configuration (default: ";
  std::cout << "\"\")\n";
  std::cout << "                      \"\" - require encryption when at last "
               "one ssl option is set, otherwise is should be disabled.\n";
  std::cout << "                      \"PREFERRED\" - encryption is optional, "
               "client tries to set it up\n";
  std::cout << "                      \"DISABLED\" - encryption is disabled\n";
  std::cout << "                      \"REQUIRED\" - encryption is required\n";
  std::cout << "                      \"VERIFY_CA\" - verify server certificate"
               "\n";
  std::cout << "                      \"VERIFY_IDENTITY\" - verify certificate "
               "issuer\n";
  std::cout << "--ssl-key             X509 key in PEM format\n";
  std::cout << "--ssl-ca              CA file in PEM format\n";
  std::cout << "--ssl-ca_path         CA directory\n";
  std::cout << "--ssl-cert            X509 cert in PEM format\n";
  std::cout << "--ssl-cipher          SSL cipher to use\n";
  std::cout << "--tls-version         TLS version to use\n";
  std::cout << "--connect-expired-password Allow expired password\n";
  std::cout << "--quiet               Don't print out messages sent\n";
  std::cout << "-vVARIABLE_NAME=VALUE Set variable VARIABLE_NAME from "
               "command line\n";
  std::cout << "--fatal-errors=<0|1>  Mysqlxtest is started with ignoring or "
               "stopping on fatal error (default: 1)\n";
  std::cout << "-B, --bindump         Dump binary representation of messages "
               "sent, in format suitable for\n";
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

Driver_command_line_options::Driver_command_line_options(
    const int argc,
    char **argv)
    : Command_line_options(argc, argv),
      m_run_without_auth(false),
      m_has_file(false),
      m_cap_expired_password(false),
      m_use_plain_auth(false),
      m_daemon(false) {
  std::string user;

  for (int i = 1; i < argc && exit_code == 0; i++) {
    char *value;
    if (check_arg_with_value(argv, i, "--file", "-f", value)) {
      m_run_file = value;
      m_has_file = true;
    } else if (check_arg(argv, i, "--no-auth", "-n")) {
      m_run_without_auth = true;
    } else if (check_arg(argv, i, "--plain-auth", NULL)) {
      m_use_plain_auth = true;
    } else if (check_arg_with_value(argv, i, "--sql", NULL, value)) {
      m_sql = value;
    } else if (check_arg_with_value(argv, i, "--password", "-p", value)) {
      m_connection_options.password = value;
    } else if (check_arg_with_value(argv, i, "--ssl-mode", NULL, value)) {
      m_connection_options.ssl_mode = value;
    } else if (check_arg_with_value(argv, i, "--ssl-key", NULL, value)) {
      m_connection_options.ssl_key = value;
    } else if (check_arg_with_value(argv, i, "--ssl-ca", NULL, value)) {
      m_connection_options.ssl_ca = value;
    } else if (check_arg_with_value(argv, i, "--ssl-ca_path", NULL, value)) {
      m_connection_options.ssl_ca_path = value;
    } else if (check_arg_with_value(argv, i, "--ssl-cert", NULL, value)) {
      m_connection_options.ssl_cert = value;
    } else if (check_arg_with_value(argv, i, "--ssl-cipher", NULL, value)) {
      m_connection_options.ssl_cipher = value;
    } else if (check_arg_with_value(argv, i, "--tls-version", NULL, value)) {
      m_connection_options.allowed_tls = value;
    } else if (check_arg_with_value(argv, i, "--host", "-h", value)) {
      m_connection_options.host = value;
    } else if (check_arg_with_value(argv, i, "--user", "-u", value)) {
      m_connection_options.user = value;
    } else if (check_arg_with_value(argv, i, "--uri", NULL, value)) {
      m_uri = value;
    } else if (check_arg_with_value(argv, i, "--schema", NULL, value)) {
      m_connection_options.schema = value;
    } else if (check_arg_with_value(argv, i, "--port", "-P", value)) {
      m_connection_options.port = ngs::stoi(value);
    } else if (check_arg_with_value(argv, i, "--ipv", NULL, value)) {
      m_connection_options.ip_mode = set_protocol(ngs::stoi(value));
    } else if (check_arg_with_value(argv, i, "--timeout", "-t", value)) {
      m_connection_options.io_timeout = ngs::stoi(value);
    } else if (check_arg_with_value(argv, i, "--fatal-errors", NULL, value)) {
      m_context_options.m_fatal_errors = ngs::stoi(value);
    } else if (check_arg_with_value(argv, i, "--password", "-p", value)) {
      m_connection_options.password = value;
    } else if (check_arg_with_value(argv, i, "--socket", "-S", value)) {
      m_connection_options.socket = value;
    } else if (check_arg_with_value(argv, i, NULL, "-v", value)) {
      set_variable_option(value);
    } else if (check_arg(argv, i, "--use-socket", NULL)) {
      m_connection_options.socket = get_socket_name();
    } else if (check_arg(argv, i, "--close-no-sync", NULL)) {
      m_connection_options.dont_wait_for_disconnect = true;
    } else if (check_arg(argv, i, "--bindump", "-B")) {
      m_context_options.m_bindump = true;
    } else if (check_arg(argv, i, "--connect-expired-password", NULL)) {
      m_cap_expired_password = true;
    } else if (check_arg(argv, i, "--quiet", "-q")) {
      m_context_options.m_quiet = true;
    } else if (check_arg(argv, i, "--verbose", NULL)) {
      m_console_options.m_be_verbose = true;
    } else if (check_arg(argv, i, "--daemon", NULL)) {
      m_daemon = true;
#ifndef _WIN32
    } else if (check_arg(argv, i, "--color", NULL)) {
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
