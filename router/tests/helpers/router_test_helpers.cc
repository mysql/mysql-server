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

#include "router_test_helpers.h"

#include <cassert>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <regex>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#ifndef _WIN32
#include <sys/socket.h>
#include <unistd.h>
#else
#include <direct.h>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#define getcwd _getcwd
#endif

#include "keyring/keyring_manager.h"
#include "my_inttypes.h"  // ssize_t
#include "mysql/harness/filesystem.h"
#include "mysql/harness/net_ts.h"
#include "mysql/harness/net_ts/impl/socket.h"
#include "mysql/harness/net_ts/impl/socket_error.h"
#include "mysql/harness/net_ts/io_context.h"
#include "mysql/harness/net_ts/socket.h"
#include "mysql/harness/stdx/filesystem.h"
#include "mysqlrouter/mysql_session.h"
#include "mysqlrouter/utils.h"
#include "test/temp_directory.h"

using mysql_harness::Path;
using namespace std::chrono_literals;

Path get_cmake_source_dir() {
  Path result;

  // PB2 specific source location
  char *env_pb2workdir = std::getenv("PB2WORKDIR");
  char *env_sourcename = std::getenv("SOURCENAME");
  char *env_tmpdir = std::getenv("TMPDIR");
  if ((env_pb2workdir && env_sourcename && env_tmpdir) &&
      (strlen(env_pb2workdir) && strlen(env_tmpdir) &&
       strlen(env_sourcename))) {
    result = Path(env_tmpdir);
    result.append(Path(env_sourcename));
    if (result.exists()) {
      return result;
    }
  }

  char *env_value = std::getenv("CMAKE_SOURCE_DIR");

  if (env_value == nullptr) {
    // try a few places
    result = Path(stdx::filesystem::current_path().native()).join("..");
    result = Path(result).real_path();
  } else {
    result = Path(env_value).real_path();
  }

  if (!result.join("src")
           .join("router")
           .join("src")
           .join("router_app.cc")
           .is_regular()) {
    throw std::runtime_error(
        "Source directory not available. Use CMAKE_SOURCE_DIR environment "
        "variable; was " +
        result.str());
  }

  return result;
}

Path get_envvar_path(const std::string &envvar, Path alternative = Path()) {
  char *env_value = std::getenv(envvar.c_str());
  Path result;
  if (env_value == nullptr) {
    result = alternative;
  } else {
    result = Path(env_value).real_path();
  }
  return result;
}

void init_windows_sockets() {
#ifdef _WIN32
  WSADATA wsaData;
  int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (iResult != 0) {
    std::cerr << "WSAStartup() failed\n";
    exit(1);
  }
#endif
}

bool pattern_found(const std::string &s, const std::string &pattern) {
  bool result = false;
  try {
    std::smatch m;
    std::regex r(pattern);
    result = std::regex_search(s, m, r);
  } catch (const std::regex_error &e) {
    std::cerr << ">" << e.what();
  }

  return result;
}

namespace {
void shut_and_close_socket(net::impl::socket::native_handle_type sock) {
  const auto shut_both =
      static_cast<std::underlying_type_t<net::socket_base::shutdown_type>>(
          net::socket_base::shutdown_type::shutdown_both);
  net::impl::socket::shutdown(sock, shut_both);
  net::impl::socket::close(sock);
}
}  // namespace

bool wait_for_port_ready(uint16_t port, std::chrono::milliseconds timeout,
                         const std::string &hostname) {
  struct addrinfo hints, *ainfo;
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  auto step_ms = 10ms;
  // Valgrind needs way more time
  if (getenv("WITH_VALGRIND")) {
    timeout *= 10;
    step_ms *= 10;
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

  const auto started = std::chrono::steady_clock::now();
  do {
    auto sock_id =
        socket(ainfo->ai_family, ainfo->ai_socktype, ainfo->ai_protocol);
    if (sock_id < 0) {
      throw std::system_error(net::impl::socket::last_error_code(),
                              "wait_for_port_ready(): socket() failed");
    }
    std::shared_ptr<void> exit_close_socket(
        nullptr, [&](void *) { shut_and_close_socket(sock_id); });

#ifdef _WIN32
    // On Windows if the port is not ready yet when we try the connect() first
    // time it will block for 500ms (depends on the OS wide configuration) and
    // retry again internally. Here we sleep for 100ms but will save this 500ms
    // for most of the cases which is still a good deal
    std::this_thread::sleep_for(100ms);
#endif
    status = connect(sock_id, ainfo->ai_addr, ainfo->ai_addrlen);
    if (status < 0) {
      // if the address is not available, it is a client side problem.
#ifdef _WIN32
      if (WSAGetLastError() == WSAEADDRNOTAVAIL) {
        throw std::system_error(net::impl::socket::last_error_code());
      }
#else
      if (errno == EADDRNOTAVAIL) {
        throw std::system_error(net::impl::socket::last_error_code());
      }
#endif
      const auto step = std::min(timeout, step_ms);
      std::this_thread::sleep_for(std::chrono::milliseconds(step));
      timeout -= step;
    }
  } while (status < 0 && timeout > std::chrono::steady_clock::now() - started);

  return status >= 0;
}

bool is_port_bindable(const uint16_t port) {
  net::io_context io_ctx;
  net::ip::tcp::acceptor acceptor(io_ctx);

  net::ip::tcp::resolver resolver(io_ctx);
  const auto &resolve_res = resolver.resolve("127.0.0.1", std::to_string(port));
  if (!resolve_res) {
    throw std::runtime_error(std::string("resolve failed: ") +
                             resolve_res.error().message());
  }

  acceptor.set_option(net::socket_base::reuse_address(true));
  const auto &open_res =
      acceptor.open(resolve_res->begin()->endpoint().protocol());
  if (!open_res) return false;

  const auto &bind_res = acceptor.bind(resolve_res->begin()->endpoint());
  if (!bind_res) return false;

  const auto &listen_res = acceptor.listen(128);
  if (!listen_res) return false;

  return true;
}

bool is_port_unused(const uint16_t port) {
#if defined(__linux__)
  const std::string netstat_cmd{"netstat -tnl"};
#elif defined(_WIN32)
  const std::string netstat_cmd{"netstat -p tcp -n -a"};
#elif defined(__sun)
  const std::string netstat_cmd{"netstat -na -P tcp"};
#else
  // BSD and MacOS
  const std::string netstat_cmd{"netstat -p tcp -an"};
#endif

  TempDirectory temp_dir;
  std::string filename = Path(temp_dir.name()).join("netstat_output.txt").str();
  const std::string cmd{netstat_cmd + " > " + filename};
  if (std::system(cmd.c_str()) != 0) {
    // netstat command failed, do the check by trying to bind to the port
    // instead
    return is_port_bindable(port);
  }

  std::ifstream file{filename};
  if (!file) throw std::runtime_error("Could not open " + filename);

  std::string line;
  while (std::getline(file, line)) {
    // Check if netstat output contains listening port <XYZ> given the following
    // netstat outputs:
    //
    // MacOS
    // tcp46   0   0 *.XYZ             *.*          LISTEN
    // tcp4    0   0 127.0.0.1.XYZ     *.*          LISTEN
    //
    // Windows
    //  TCP    127.0.0.1:XYZ          0.0.0.0:0              LISTENING
    //  TCP    0.0.0.0:XYZ            0.0.0.0:0              LISTENING
    //
    //  Linux/BSD
    //  tcp     0    0 0.0.0.0:XYZ       0.0.0.0:*               LISTEN
    //  tcp     0    0 127.0.0.1:XYZ     0.0.0.0:*               LISTEN
    //
    //  SunOS
    //  *.XYZ                 *.*              0      0  256000      0 LISTEN
    //  127.0.0.1.XYZ         *.*              0      0  256000      0 LISTEN
    if (pattern_found(line, "[\\*,0,127]\\..*[.:]" + std::to_string(port) +
                                "[^\\d].*LISTEN")) {
      return false;
    }
  }

  return true;
}

static bool wait_for_port(const bool available, const uint16_t port,
                          std::chrono::milliseconds timeout = 2s) {
  const std::chrono::milliseconds step = 50ms;
  using clock_type = std::chrono::steady_clock;
  const auto end = clock_type::now() + timeout;
  do {
    if (available == is_port_unused(port)) return true;
    std::this_thread::sleep_for(step);
  } while (clock_type::now() < end);
  return false;
}

bool wait_for_port_used(const uint16_t port,
                        std::chrono::milliseconds timeout) {
  return wait_for_port(/*available*/ false, port, timeout);
}

bool wait_for_port_unused(const uint16_t port,
                          std::chrono::milliseconds timeout) {
  return wait_for_port(/*available*/ true, port, timeout);
}

void init_keyring(std::map<std::string, std::string> &default_section,
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

namespace {

bool real_find_in_file(
    const std::string &file_path,
    const std::function<bool(const std::string &)> &predicate,
    std::ifstream &in_file, std::streampos &cur_pos) {
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
    if (predicate(line)) {
      return true;
    }
  }

  return false;
}

}  // namespace

bool find_in_file(const std::string &file_path,
                  const std::function<bool(const std::string &)> &predicate,
                  std::chrono::milliseconds sleep_time) {
  const auto STEP = std::chrono::milliseconds(100);
  std::ifstream in_file;
  std::streampos cur_pos;
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

std::string get_file_output(const std::string &file_name,
                            const std::string &file_path,
                            bool throw_on_error /*=false*/) {
  return get_file_output(file_path + "/" + file_name, throw_on_error);
}

std::string get_file_output(const std::string &file_name,
                            bool throw_on_error /*=false*/) {
  Path file(file_name);
  std::ifstream in_file;
  in_file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
  try {
    in_file.open(file.c_str(), std::ifstream::in);
  } catch (const std::exception &e) {
    const std::string msg =
        "Could not open file '" + file.str() + "' for reading: ";
    if (throw_on_error)
      throw std::runtime_error(msg + e.what());
    else
      return "<THIS ERROR COMES FROM TEST FRAMEWORK'S get_file_output(), IT IS "
             "NOT PART OF PROCESS OUTPUT: " +
             msg + e.what() + ">";
  }
  assert(in_file);

  std::string result;
  try {
    result.assign((std::istreambuf_iterator<char>(in_file)),
                  std::istreambuf_iterator<char>());
  } catch (const std::exception &e) {
    const std::string msg = "Reading file '" + file.str() + "' failed: ";
    if (throw_on_error)
      throw std::runtime_error(msg + e.what());
    else
      return "<THIS ERROR COMES FROM TEST FRAMEWORK'S get_file_output(), IT IS "
             "NOT PART OF PROCESS OUTPUT: " +
             msg + e.what() + ">";
  }

  return result;
}

bool add_line_to_config_file(const std::string &config_path,
                             const std::string &section_name,
                             const std::string &key, const std::string &value) {
  std::ifstream config_stream{config_path};
  if (!config_stream) return false;

  std::vector<std::string> config;
  std::string line;
  bool found{false};
  while (std::getline(config_stream, line)) {
    config.push_back(line);
    if (line == "[" + section_name + "]") {
      config.push_back(key + "=" + value);
      found = true;
    }
  }
  config_stream.close();
  if (!found) return false;

  std::ofstream out_stream{config_path};
  if (!out_stream) return false;

  std::copy(std::begin(config), std::end(config),
            std::ostream_iterator<std::string>(out_stream, "\n"));
  out_stream.close();
  return true;
}

void connect_client_and_query_port(unsigned router_port, std::string &out_port,
                                   bool should_fail) {
  using mysqlrouter::MySQLSession;
  MySQLSession client;

  if (should_fail) {
    try {
      client.connect("127.0.0.1", router_port, "username", "password", "", "");
    } catch (const std::exception &exc) {
      if (std::string(exc.what()).find("Error connecting to MySQL server") !=
          std::string::npos) {
        out_port = "";
        return;
      } else
        throw;
    }
    throw std::runtime_error(
        "connect_client_and_query_port: did not fail as expected");

  } else {
    client.connect("127.0.0.1", router_port, "username", "password", "", "");
  }

  std::unique_ptr<MySQLSession::ResultRow> result{
      client.query_one("select @@port")};
  if (nullptr == result.get()) {
    throw std::runtime_error(
        "connect_client_and_query_port: error querying the port");
  }
  if (1u != result->size()) {
    throw std::runtime_error(
        "connect_client_and_query_port: wrong number of columns returned " +
        std::to_string(result->size()));
  }
  out_port = std::string((*result)[0]);
}

// Wait for the nth occurrence of the log_regex in the log_file with the timeout
// If it's found returns the full line containing the log_regex
// If the timeout has been reached returns unexpected
static std::optional<std::string> wait_log_line(
    const std::string &log_file, const std::string &log_regex,
    const unsigned n_occurence = 1,
    const std::chrono::milliseconds timeout = 1s) {
  const auto start_timestamp = std::chrono::steady_clock::now();
  const auto kStep = 50ms;

  do {
    std::istringstream ss{get_file_output(log_file)};

    unsigned current_occurence = 0;
    for (std::string line; std::getline(ss, line);) {
      if (pattern_found(line, log_regex)) {
        current_occurence++;
        if (current_occurence == n_occurence) return {line};
      }
    }

    if (std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_timestamp) >= timeout) {
      return std::nullopt;
    }
    std::this_thread::sleep_for(kStep);
  } while (true);
}

std::optional<std::chrono::time_point<std::chrono::system_clock>>
get_log_timestamp(const std::string &log_file, const std::string &log_regex,
                  const unsigned occurence,
                  const std::chrono::milliseconds timeout) {
  // first wait for the nth occurrence of the pattern
  const auto log_line = wait_log_line(log_file, log_regex, occurence, timeout);
  if (!log_line) {
    return std::nullopt;
  }

  const std::string log_line_str = log_line.value();
  // make sure the line is prefixed with the expected timestamp
  // 2020-06-09 03:53:26.027 foo bar
  if (!pattern_found(log_line_str,
                     "^\\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2}\\.\\d{3}.*")) {
    return std::nullopt;
  }

  // extract the timestamp prefix and convert to the duration
  std::string timestamp_str =
      log_line_str.substr(0, strlen("2020-06-09 03:53:26.027"));
  std::tm tm{};
#ifdef HAVE_STRPTIME
  char *rest = strptime(timestamp_str.c_str(), "%Y-%m-%d %H:%M:%S", &tm);
  assert(*rest == '.');
  int milliseconds = atoi(++rest);
#else
  std::stringstream timestamp_ss(timestamp_str);
  char dot;
  unsigned milliseconds;
  timestamp_ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S") >> dot >>
      milliseconds;
#endif
  auto result = std::chrono::system_clock::from_time_t(std::mktime(&tm));
  result += std::chrono::milliseconds(milliseconds);

  return result;
}
