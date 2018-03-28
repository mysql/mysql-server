/*
 * Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
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

#include <fstream>
#include <stdexcept>

#include "my_dbug.h"
#include "my_loglevel.h"
#include "my_sys.h"
#include "plugin/x/tests/driver/driver_command_line_options.h"
#include "plugin/x/tests/driver/processor/stream_processor.h"
#include "violite.h"

static void ignore_traces_from_libraries(enum loglevel ll, const char *format,
                                         va_list args) {}

bool parse_mysql_connstring(const std::string &connstring,
                            std::string *protocol, std::string *user,
                            std::string *password, std::string *host, int *port,
                            std::string *sock, std::string *db,
                            int *pwd_found = nullptr) {
  // format is [protocol://][user[:pass]]@host[:port][/db] or
  // user[:pass]@::socket[/db], like what cmdline utilities use
  if (pwd_found) *pwd_found = 0;
  std::string remaining = connstring;
  std::string::size_type p;
  p = remaining.find("://");
  if (p != std::string::npos) {
    *protocol = connstring.substr(0, p);
    remaining = remaining.substr(p + 3);
  }
  std::string s = remaining;
  p = remaining.find('/');
  if (p != std::string::npos) {
    *db = remaining.substr(p + 1);
    s = remaining.substr(0, p);
  }
  p = s.rfind('@');
  std::string user_part;
  std::string server_part = (p == std::string::npos) ? s : s.substr(p + 1);
  if (p == std::string::npos) {
// by default, connect using the current OS username
#ifdef _WIN32
    char tmp_buffer[1024];
    char *tmp = tmp_buffer;
    DWORD tmp_size = sizeof(tmp_buffer);
    if (!GetUserNameA(tmp_buffer, &tmp_size)) {
      tmp = NULL;
    }
#else
    const char *tmp = getenv("USER");
#endif
    user_part = tmp ? tmp : "";
  } else {
    user_part = s.substr(0, p);
  }
  if ((p = user_part.find(':')) != std::string::npos) {
    *user = user_part.substr(0, p);
    *password = user_part.substr(p + 1);
    if (pwd_found) *pwd_found = 1;
  } else {
    *user = user_part;
  }
  p = server_part.find(':');
  if (p != std::string::npos) {
    *host = server_part.substr(0, p);
    server_part = server_part.substr(p + 1);
    p = server_part.find(':');
    if (p != std::string::npos)
      *sock = server_part.substr(p + 1);
    else if (!sscanf(server_part.substr(0, p).c_str(), "%i", port))
      return false;
  } else {
    *host = server_part;
  }
  return true;
}

bool parse_mysql_connstring(const std::string &uri,
                            Connection_options *options) {
  int pwdfound;
  std::string proto;
  return parse_mysql_connstring(uri, &proto, &options->user, &options->password,
                                &options->host, &options->port,
                                &options->socket, &options->schema, &pwdfound);
}

int client_connect_and_process(const Driver_command_line_options &options,
                               std::istream &input) {
  Variable_container variables(options.m_variables);
  Console console(options.m_console_options);
  Connection_manager cm{options.m_connection_options, &variables, console};
  Execution_context context(options.m_context_options, &cm, &variables,
                            console);

  try {
    context.m_script_stack.push({0, "main"});

    cm.connect_default(options.m_cap_expired_password,
                       options.m_client_interactive, options.m_run_without_auth,
                       options.m_auth_methods);

    std::vector<Block_processor_ptr> eaters = create_block_processors(&context);
    int result_code =
        process_client_input(input, &eaters, &context.m_script_stack, console);

    if (!options.m_run_without_auth) cm.close_active(true);

    return result_code;
  } catch (const xcl::XError &e) {
    if (options.is_expected_error_set() &&
        options.m_expected_error_code == e.error()) {
      console.print("Application terminated with expected error: ", e.what(),
                    " (code ", e.error(), ")\n");
      return 0;
    }
    console.print_error_red(context.m_script_stack, e, '\n');

    return 1;
  }
}

std::istream &get_input(Driver_command_line_options *opt, std::ifstream &file,
                        std::stringstream &string) {
  if (opt->m_has_file) {
    if (!opt->m_sql.empty()) {
      std::cerr << "ERROR: specified file and SQL to execute, please enter "
                   "only one of those\n";
      opt->exit_code = 1;
    }

    file.open(opt->m_run_file.c_str());
    file.rdbuf()->pubsetbuf(NULL, 0);

    if (!file.is_open()) {
      std::cerr << "ERROR: Could not open file " << opt->m_run_file << "\n";
      opt->exit_code = 1;
    }

    return file;
  }

  if (!opt->m_sql.empty()) {
    std::streampos position = string.tellp();

    string << "-->sql\n";
    string << opt->m_sql << "\n";
    string << "-->endsql\n";
    string.seekp(position, std::ios::beg);

    return string;
  }

  return std::cin;
}

void unable_daemonize() {
  std::cerr << "ERROR: Unable to put process in background\n";
  exit(2);
}

static void daemonize() {
#ifdef WIN32
  unable_daemonize();
#else
  if (getppid() == 1)  // already a daemon
    exit(0);
  pid_t pid = fork();
  if (pid < 0) unable_daemonize();
  if (pid > 0) exit(0);
  if (setsid() < 0) unable_daemonize();
#endif
}

int main(int argc, char **argv) {
  MY_INIT(argv[0]);
  DBUG_ENTER("main");

  local_message_hook = ignore_traces_from_libraries;

  Driver_command_line_options options(argc, argv);

  if (options.exit_code != 0) return options.exit_code;

  if (options.m_daemon) daemonize();

  std::cout << std::unitbuf;
  std::ifstream fs;
  std::stringstream ss;
  std::istream &input = get_input(&options, fs, ss);
  if (options.m_uri.length()) {
    parse_mysql_connstring(options.m_uri, &options.m_connection_options);
  }
#ifdef WIN32
  if (!have_tcpip) {
    std::cerr << "OS doesn't have tcpip\n";
    DBUG_RETURN(1);
  }
#endif

  ssl_start();

  bool return_code = 0;
  try {
    return_code = client_connect_and_process(options, input);
    const bool is_ok = 0 == return_code;

    if (is_ok)
      std::cerr << "ok\n";
    else
      std::cerr << "not ok\n";
  } catch (std::exception &e) {
    std::cerr << "ERROR: " << e.what() << "\n";
    return_code = 1;
  }

  vio_end();
  my_end(0);
  DBUG_RETURN(return_code);
}
