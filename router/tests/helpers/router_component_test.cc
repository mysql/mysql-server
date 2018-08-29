/*
  Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef _WIN32
#include <netdb.h>
#include <netinet/in.h>
#include <sys/file.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#else
#define USE_STD_REGEX
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <direct.h>
#include <io.h>
#include <stdio.h>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <codecvt>
#include <locale>
#endif

#include <fcntl.h>
#include "router_component_test.h"

#include "dim.h"
#include "keyring/keyring_manager.h"
#include "mysqlrouter/utils.h"
#include "process_launcher.h"
#include "random_generator.h"

#include <algorithm>
#include <chrono>
#include <iterator>
#include <system_error>
#include <thread>

#ifdef USE_STD_REGEX
#include <regex>
#else
#include <regex.h>
#endif

using mysql_harness::Path;
using mysql_harness::ProcessLauncher;

using mysqlrouter::get_socket_errno;

namespace {

template <typename Out>
void split_str(const std::string &input, Out result, char delim = ' ') {
  std::stringstream ss;
  ss.str(input);
  std::string item;
  while (std::getline(ss, item, delim)) {
    *(result++) = item;
  }
}

std::vector<std::string> split_str(const std::string &s, char delim = ' ') {
  std::vector<std::string> elems;
  split_str(s, std::back_inserter(elems), delim);
  return elems;
}

#ifndef _WIN32
int close_socket(int sock) { return close(sock); }
#else
int close_socket(SOCKET sock) { return closesocket(sock); }
#endif

}  // namespace

/*static*/
void RouterComponentTest::init_keyring(
    std::map<std::string, std::string> &default_section,
    const std::string &keyring_dir,
    const std::string &user /*= "mysql_router1_user"*/,
    const std::string &password /*= "root"*/) {
  // init keyring
  const std::string masterkey_file = Path(keyring_dir).join("master.key").str();
  const std::string keyring_file = Path(keyring_dir).join("keyring").str();
  mysql_harness::init_keyring(keyring_file, masterkey_file, true);
  mysql_harness::Keyring *keyring = mysql_harness::get_keyring();
  keyring->store(user, "password", password);
  mysql_harness::flush_keyring();
  mysql_harness::reset_keyring();

  // add relevant config settings to [DEFAULT] section
  default_section["keyring_path"] = keyring_file;
  default_section["master_key_path"] = masterkey_file;
}

RouterComponentTest::RouterComponentTest()
    : data_dir_(COMPONENT_TEST_DATA_DIR),
      logging_dir_(RouterComponentTest::get_tmp_dir("log")) {}

RouterComponentTest::~RouterComponentTest() { purge_dir(logging_dir_.str()); }

void RouterComponentTest::init() {
  using mysql_harness::Path;
  ;
  if (origin_dir_.str().empty()) {
    throw std::runtime_error("Origin dir not set");
  }
  plugin_dir_ = mysql_harness::get_plugin_dir(origin_dir_.str());

  auto get_exe_path = [&](const std::string &name) -> Path {
    Path path(origin_dir_);
#ifdef _WIN32
    path.append(name + ".exe");
#else
    path.append(name);
#endif
    return path.real_path();
  };

  mysqlrouter_exec_ = get_exe_path("mysqlrouter");
  mysqlserver_mock_exec_ = get_exe_path("mysql_server_mock");

  mysql_harness::DIM &dim = mysql_harness::DIM::instance();
  // RandomGenerator
  dim.set_RandomGenerator(
      []() {
        static mysql_harness::RandomGenerator rg;
        return &rg;
      },
      [](mysql_harness::RandomGeneratorInterface *) {});
}

RouterComponentTest::CommandHandle RouterComponentTest::launch_command(
    const std::string &command, const std::string &params,
    bool catch_stderr) const {
  auto params_vec = split_str(params, ' ');
  const char *params_arr[MAX_PARAMS];
  get_params(command, params_vec, params_arr);

  if (command.empty())
    throw std::logic_error("path to launchable executable must not be empty");
  return RouterComponentTest::CommandHandle(command, params_arr, catch_stderr);
}

RouterComponentTest::CommandHandle RouterComponentTest::launch_command(
    const std::string &command, const std::vector<std::string> &params,
    bool catch_stderr) const {
  const char *params_arr[MAX_PARAMS];
  get_params(command, params, params_arr);

  if (command.empty())
    throw std::logic_error("path to launchable executable must not be empty");
  return RouterComponentTest::CommandHandle(command, params_arr, catch_stderr);
}

static std::vector<std::string> build_exec_args(
    const std::string &mysqlrouter_exec, bool with_sudo) {
  const std::string sudo_cmd = "sudo";
  const std::string sudo_args = "--non-interactive";
  const std::string valgrind_cmd = "valgrind";
  const std::string valgrind_args = "--error-exitcode=1 --quiet";
  std::vector<std::string> args;

  if (with_sudo) {
    args.emplace_back(sudo_cmd);
    args.emplace_back(sudo_args);
  }

  if (getenv("WITH_VALGRIND")) {
    args.emplace_back(valgrind_cmd);
    args.emplace_back(valgrind_args);
  }

  args.emplace_back(mysqlrouter_exec);

  return args;
}

RouterComponentTest::CommandHandle RouterComponentTest::launch_router(
    const std::string &params, bool catch_stderr, bool with_sudo) const {
  std::vector<std::string> args =
      build_exec_args(mysqlrouter_exec_.str(), with_sudo);
  args.emplace_back(params);

  // first argument is special - it needs to be passed as "command" to
  // launch_router()
  auto it = args.begin();
  std::string cmd(*it++);

  // and the rest of them go into 'cmd_args', separated by spaces
  std::string cmd_args;
  for (; it < args.end(); it++) cmd_args += *it + " ";
  cmd_args.resize(cmd_args.size() - 1);  // remove last space

  return launch_command(cmd, cmd_args, catch_stderr);
}

RouterComponentTest::CommandHandle RouterComponentTest::launch_router(
    const std::vector<std::string> &params, bool catch_stderr,
    bool with_sudo) const {
  std::vector<std::string> args =
      build_exec_args(mysqlrouter_exec_.str(), with_sudo);

  // 1st argument is special - it needs to be passed as "command" to
  // launch_router()
  std::string cmd = args.at(0);
  args.erase(args.begin());
  std::copy(params.begin(), params.end(), std::back_inserter(args));

  return launch_command(cmd, args, catch_stderr);
}

RouterComponentTest::CommandHandle
RouterComponentTest::launch_mysql_server_mock(
    const std::string &json_file, unsigned port, bool debug_mode,
    uint16_t http_port, const std::string &module_prefix /* = "" */
    ) const {
  if (mysqlserver_mock_exec_.str().empty())
    throw std::logic_error("path to mysql-server-mock must not be empty");
  return launch_command(
      mysqlserver_mock_exec_.str(),
      "--filename=" + json_file + " --port=" + std::to_string(port) +
          " --http-port=" + std::to_string(http_port) + " --module-prefix=" +
          (!module_prefix.empty() ? module_prefix : get_data_dir().str()) +
          (debug_mode ? " --verbose" : ""),
      true);
}

bool RouterComponentTest::wait_for_port_ready(
    unsigned port, unsigned timeout_msec, const std::string &hostname) const {
  struct addrinfo hints, *ainfo;
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  // Valgrind needs way more time
  if (getenv("WITH_VALGRIND")) {
    timeout_msec *= 10;
  }

  int status = getaddrinfo(hostname.c_str(), std::to_string(port).c_str(),
                           &hints, &ainfo);
  if (status != 0) {
    throw std::runtime_error(
        std::string("wait_for_port_ready(): getaddrinfo() failed: ") +
        gai_strerror(status));
  }
  std::shared_ptr<void> exit_freeaddrinfo(nullptr,
                                          [&](void *) { freeaddrinfo(ainfo); });

  const unsigned MSEC_STEP = 10;
  const auto started = std::chrono::steady_clock::now();
  do {
    auto sock_id =
        socket(ainfo->ai_family, ainfo->ai_socktype, ainfo->ai_protocol);
    if (sock_id < 0) {
      throw std::runtime_error("wait_for_port_ready(): socket() failed: " +
                               std::to_string(get_socket_errno()));
    }
    std::shared_ptr<void> exit_close_socket(
        nullptr, [&](void *) { close_socket(sock_id); });

    status = connect(sock_id, ainfo->ai_addr, ainfo->ai_addrlen);
    if (status < 0) {
      unsigned step = std::min(timeout_msec, MSEC_STEP);
      std::this_thread::sleep_for(std::chrono::milliseconds(step));
      timeout_msec -= step;
    }
  } while (status < 0 &&
           timeout_msec > std::chrono::duration_cast<std::chrono::milliseconds>(
                              std::chrono::steady_clock::now() - started)
                              .count());

  return status >= 0;
}

/*static*/
int RouterComponentTest::purge_dir(const std::string &dir) {
  return mysql_harness::delete_dir_recursive(dir);
}

/*static*/
std::string RouterComponentTest::get_tmp_dir(const std::string &name) {
  return mysql_harness::get_tmp_dir(name);
}

void RouterComponentTest::get_params(const std::string &command,
                                     const std::vector<std::string> &params_vec,
                                     const char *out_params[MAX_PARAMS]) const {
  out_params[0] = command.c_str();

  size_t i = 1;
  for (const auto &par : params_vec) {
    if (i >= MAX_PARAMS - 1) {
      throw std::runtime_error("Too many parameters passed to the MySQLRouter");
    }
    out_params[i++] = par.c_str();
  }
  out_params[i] = nullptr;
}

/* static */
void RouterComponentTest::replace_process_env(
    std::istream &ins, std::ostream &outs,
    const std::map<std::string, std::string> &env_vars) {
  std::string line;
  const char *regex = "^(.*)process\\.env\\.([A-Za-z_][A-Za-z0-9_]*)(.*)$";

#ifdef USE_STD_REGEX
  std::regex js_process_env_regex(regex);
  while (std::getline(ins, line)) {
    std::smatch m;
    if (std::regex_match(line, m, js_process_env_regex)) {
      try {
        outs << m[1].str() << "\"" << env_vars.at(m[2].str()) << "\""
             << m[3].str() << std::endl;
      } catch (const std::out_of_range &e) {
        throw std::runtime_error("Envvar " + m[2].str() +
                                 " requested, but isn't defined");
      }
    } else {
      outs << line << std::endl;
    }
  }
#else
  regex_t r;
  int r_err;
  char r_errbuf[256];

  r_err = regcomp(&r, regex, REG_EXTENDED);

  if (r_err) {
    regerror(r_err, NULL, r_errbuf, sizeof(r_errbuf));
    throw std::runtime_error(std::string("compiling regex ") + regex +
                             " failed: " + std::string(r_errbuf));
  }

  std::shared_ptr<void> exit_guard(nullptr, [&](void *) { regfree(&r); });

  const size_t expected_nsub = 3;

  if (r.re_nsub != expected_nsub) {
    throw std::runtime_error(std::string("compiling regex ") + regex +
                             " resulted in " + std::to_string(r.re_nsub) +
                             " captures, expected " +
                             std::to_string(expected_nsub));
  }

  while (std::getline(ins, line)) {
    regmatch_t m[expected_nsub + 1];
    r_err = regexec(&r, line.c_str(), sizeof(m) / sizeof(m[0]), m, 0);

    if (r_err == 0) {
      std::string m_1 =
          std::string(line.c_str() + m[1].rm_so, m[1].rm_eo - m[1].rm_so);
      std::string m_2 =
          std::string(line.c_str() + m[2].rm_so, m[2].rm_eo - m[2].rm_so);
      std::string m_3 =
          std::string(line.c_str() + m[3].rm_so, m[3].rm_eo - m[3].rm_so);
      try {
        outs << m_1 << "\"" << env_vars.at(m_2) << "\"" << m_3 << std::endl;
      } catch (const std::out_of_range &e) {
        throw std::runtime_error("Envvar " + m_2 +
                                 " requested, but isn't defined");
      }
    } else if (r_err == REG_NOMATCH) {
      outs << line << std::endl;
    } else {
      regerror(r_err, NULL, r_errbuf, sizeof(r_errbuf));
      throw std::runtime_error(std::string("executing regex ") + regex +
                               " failed: " + std::string(r_errbuf));
    }
  }
#endif
}

/* static */
void RouterComponentTest::rewrite_js_to_tracefile(
    const std::string &infile_name, const std::string &outfile_name,
    const std::map<std::string, std::string> &env_vars) {
  std::ifstream js_file(infile_name);
  std::ofstream json_file(outfile_name);

  replace_process_env(js_file, json_file, env_vars);
}

int RouterComponentTest::CommandHandle::
    wait_for_exit_while_reading_and_autoresponding_to_output(
        unsigned timeout_ms) {
  namespace ch = std::chrono;
  ch::time_point<ch::steady_clock> timeout =
      ch::steady_clock::now() + ch::milliseconds(timeout_ms);

  // We alternate between non-blocking read() and non-blocking waitpid() here.
  // Reading/autoresponding must be done, because the child might be blocked on
  // them (for example, it might block on password prompt), and therefore won't
  // exit until we deal with its output.
  std::exception_ptr eptr;
  exit_code_set_ = false;
  while (ch::steady_clock::now() < timeout) {
    read_and_autorespond_to_output(0);

    try {
      // throws std::runtime_error or std::system_error
      exit_code_ = launcher_.wait(0);
      exit_code_set_ = true;
      break;
    } catch (const std::runtime_error &e) {
      eptr = std::current_exception();
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  if (exit_code_set_) {
    // the child exited, but there might still be some data left in the pipe to
    // read, so let's consume it all
    while (read_and_autorespond_to_output(1, false))
      ;  // false = disable autoresponder
    return exit_code_;
  } else {
    // we timed out waiting for child
    std::rethrow_exception(eptr);
  }
}

bool RouterComponentTest::CommandHandle::expect_output(const std::string &str,
                                                       bool regex,
                                                       unsigned timeout_ms) {
  auto now = std::chrono::steady_clock::now();
  auto until = now + std::chrono::milliseconds(timeout_ms);
  for (;;) {
    if (output_contains(str, regex)) return true;

    now = std::chrono::steady_clock::now();

    if (now > until) {
      return false;
    }

    if (!read_and_autorespond_to_output(
            std::chrono::duration_cast<std::chrono::milliseconds>(until - now)
                .count()))
      return false;
  }
}

bool RouterComponentTest::CommandHandle::output_contains(const std::string &str,
                                                         bool regex) const {
  if (!regex) {
    return execute_output_raw_.find(str) != std::string::npos;
  }

  // regex
  return pattern_found(execute_output_raw_, str);
}

bool RouterComponentTest::CommandHandle::read_and_autorespond_to_output(
    unsigned timeout_ms, bool autoresponder_enabled /*= true*/) {
  char cmd_output[kReadBufSize] = {
      0};  // hygiene (cmd_output[bytes_read] = 0 would suffice)

  // blocks until timeout expires (very likely) or until at least one byte is
  // read (unlikely) throws std::runtime_error on read error
  int bytes_read =
      launcher_.read(cmd_output, kReadBufSize - 1,
                     timeout_ms);  // cmd_output may contain multiple lines

  if (bytes_read > 0) {
#ifdef _WIN32
    // On Windows we get \r\n instead of \n, so we need to get rid of the \r
    // everywhere. As surprising as it is, WIN32API doesn't provide the
    // automatic conversion:
    // https://stackoverflow.com/questions/18294650/win32-changing-to-binary-mode-childs-stdout-pipe
    {
      char *new_end = std::remove(cmd_output, cmd_output + bytes_read, '\r');
      *new_end = '\0';
      bytes_read = new_end - cmd_output;
    }
#endif

    execute_output_raw_ += cmd_output;

    if (autoresponder_enabled)
      autorespond_to_matching_lines(bytes_read, cmd_output);

    return true;
  } else {
    return false;
  }
}

void RouterComponentTest::CommandHandle::autorespond_to_matching_lines(
    int bytes_read, char *cmd_output) {
  // returned lines do not contain the \n
  std::vector<std::string> lines =
      split_str(std::string(cmd_output, cmd_output + bytes_read), '\n');
  if (lines.empty()) return;

  // it is possible that the last line from the previous call did not match
  // because it arrived incomplete. Here we try an assumption that the first
  // line is a continuation of last line from previous call.
  if (last_line_read_.size() &&
      autorespond_on_matching_pattern(last_line_read_ + lines.front())) {
    // indeed, it was a continuation of previous line. So now we must prevent
    // both fragments from being used again
    lines.erase(lines.begin());
    last_line_read_.clear();

    if (lines.empty()) return;
  }

  // try matching all but last line
  for (auto it = lines.cbegin(); it != lines.cend() - 1; ++it)
    autorespond_on_matching_pattern(*it);

  // try matching the last line
  if (autorespond_on_matching_pattern(lines.back()))
    last_line_read_.clear();
  else
    // last line failed to match, it may be because it arrived incomplete. Save
    // it for the next time
    last_line_read_ = lines.back();
}

bool RouterComponentTest::CommandHandle::autorespond_on_matching_pattern(
    const std::string &line) {
  for (const auto &response : output_responses_) {
    const std::string &output = response.first;
    if (line.substr(0, output.size()) == output) {
      const char *resp = response.second.c_str();
      launcher_.write(resp, strlen(resp));
      return true;
    }
  }

  return false;
}

std::map<std::string, std::string> RouterComponentTest::get_DEFAULT_defaults()
    const {
  return {
      {"logging_folder", logging_dir_.str()},
      {"plugin_folder", plugin_dir_.str()},
      {"runtime_folder", origin_dir_.str()},
      {"config_folder", origin_dir_.str()},
      {"data_folder", origin_dir_.str()},
  };
}

std::string RouterComponentTest::make_DEFAULT_section(
    const std::map<std::string, std::string> *params) const {
  auto l = [params](const char *key) -> std::string {
    return (params->count(key))
               ? std::string(key) + " = " + params->at(key) + "\n"
               : "";
  };

  return params ? std::string("[DEFAULT]\n") + l("logging_folder") +
                      l("plugin_folder") + l("runtime_folder") +
                      l("config_folder") + l("data_folder") +
                      l("keyring_path") + l("master_key_path") +
                      l("master_key_reader") + l("master_key_writer") + "\n"
                : std::string("[DEFAULT]\n") +
                      "logging_folder = " + logging_dir_.str() + "\n" +
                      "plugin_folder = " + plugin_dir_.str() + "\n" +
                      "runtime_folder = " + origin_dir_.str() + "\n" +
                      "config_folder = " + origin_dir_.str() + "\n" +
                      "data_folder = " + origin_dir_.str() + "\n\n";
}

std::string RouterComponentTest::create_config_file(
    const std::string &directory, const std::string &sections,
    const std::map<std::string, std::string> *default_section,
    const std::string &name) const {
  Path file_path = Path(directory).join(name);
  std::ofstream ofs_config(file_path.str());

  if (!ofs_config.good()) {
    throw(
        std::runtime_error("Could not create config file " + file_path.str()));
  }

  ofs_config << make_DEFAULT_section(default_section);
  ofs_config << sections << std::endl;
  ofs_config.close();

  return file_path.str();
}

bool RouterComponentTest::find_in_file(
    const std::string &file_path,
    const std::function<bool(const std::string &)> &predicate,
    std::chrono::milliseconds sleep_time) {
  const auto STEP = std::chrono::milliseconds(100);
  std::ifstream in_file;
  std::ios::streampos cur_pos;
  do {
    try {
      // This is proxy function to account for the fact that I/O can sometimes
      // be slow.
      if (real_find_in_file(file_path, predicate, in_file, cur_pos))
        return true;
    } catch (const std::runtime_error &) {
      // report I/O error only on the last attempt
      if (sleep_time == std::chrono::milliseconds(0)) {
        std::cerr << "  find_in_file() failed, giving up." << std::endl;
        throw;
      }
    }

    const auto sleep_for = std::min(STEP, sleep_time);
    std::this_thread::sleep_for(sleep_for);
    sleep_time -= sleep_for;

  } while (sleep_time > std::chrono::milliseconds(0));

  return false;
}

bool RouterComponentTest::real_find_in_file(
    const std::string &file_path,
    const std::function<bool(const std::string &)> &predicate,
    std::ifstream &in_file, std::ios::streampos &cur_pos) {
  if (!in_file.is_open()) {
    in_file.clear();
    Path file(file_path);
    in_file.open(file.c_str(), std::ifstream::in);
    if (!in_file) {
      throw std::runtime_error("Error opening file " + file.str());
    }
    cur_pos = in_file.tellg();  // initialize properly
  } else {
    // set current position to the end of what was already read
    in_file.clear();
    in_file.seekg(cur_pos);
  }

  std::string line;
  while (std::getline(in_file, line)) {
    cur_pos = in_file.tellg();
    if (predicate(line)) return true;
  }

  return false;
}

std::string RouterComponentTest::get_router_log_output(
    const std::string &file_name, const std::string &file_path) {
  std::ifstream in_file;
  const std::string path = file_path.empty() ? logging_dir_.str() : file_path;
  Path file(path + "/" + file_name);
  in_file.open(file.c_str(), std::ifstream::in);
  if (!in_file) {
    return "Could not open log file " + file.str() + " for reading.";
  }

  std::string result((std::istreambuf_iterator<char>(in_file)),
                     std::istreambuf_iterator<char>());

  return result;
}
