/*
  Copyright (c) 2015, 2020, Oracle and/or its affiliates. All rights reserved.

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
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <regex>
#include <stdexcept>
#include <thread>

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
#include "mysqlrouter/mysql_session.h"
#include "mysqlrouter/utils.h"

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
    result = Path(get_cwd()).join("..");
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

const std::string get_cwd() {
  char buffer[FILENAME_MAX];
  if (!getcwd(buffer, FILENAME_MAX)) {
    throw std::runtime_error("getcwd failed: " + std::string(strerror(errno)));
  }
  return std::string(buffer);
}

const std::string change_cwd(std::string &dir) {
  auto cwd = get_cwd();
#ifndef _WIN32
  if (chdir(dir.c_str()) == -1) {
#else
  if (!SetCurrentDirectory(dir.c_str())) {
#endif
    throw std::runtime_error("chdir failed: " + mysqlrouter::get_last_error());
  }
  return cwd;
}

size_t read_bytes_with_timeout(int sockfd, void *buffer, size_t n_bytes,
                               uint64_t timeout_in_ms) {
  // returns epoch time (aka unix time, etc), expressed in milliseconds
  auto get_epoch_in_ms = []() -> uint64_t {
    using namespace std::chrono;
    time_point<system_clock> now = system_clock::now();
    return static_cast<uint64_t>(
        duration_cast<milliseconds>(now.time_since_epoch()).count());
  };

  // calculate deadline time
  uint64_t now_in_ms = get_epoch_in_ms();
  uint64_t deadline_epoch_in_ms = now_in_ms + timeout_in_ms;

  // read until 1 of 3 things happen: enough bytes were read, we time out or
  // read() fails
  size_t bytes_read = 0;
  while (true) {
#ifndef _WIN32
    ssize_t res = read(sockfd, static_cast<char *>(buffer) + bytes_read,
                       n_bytes - bytes_read);
#else
    WSASetLastError(0);
    ssize_t res = recv(sockfd, static_cast<char *>(buffer) + bytes_read,
                       n_bytes - bytes_read, 0);
#endif

    if (res == 0) {  // reached EOF?
      return bytes_read;
    }

    if (get_epoch_in_ms() > deadline_epoch_in_ms) {
      throw std::runtime_error("read() timed out");
    }

    if (res == -1) {
#ifndef _WIN32
      if (errno != EAGAIN) {
        throw std::runtime_error(std::string("read() failed: ") +
                                 strerror(errno));
      }
#else
      int err_code = WSAGetLastError();
      if (err_code != 0) {
        throw std::runtime_error("recv() failed with error: " +
                                 get_last_error(err_code));
      }

#endif
    } else {
      bytes_read += static_cast<size_t>(res);
      if (bytes_read >= n_bytes) {
        assert(bytes_read == n_bytes);
        return bytes_read;
      }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

#ifdef _WIN32
std::string get_last_error(int err_code) {
  char message[512];
  FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS |
                    FORMAT_MESSAGE_ALLOCATE_BUFFER,
                nullptr, err_code, LANG_NEUTRAL, message, sizeof(message),
                nullptr);
  return std::string(message);
}
#endif

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
#ifndef _WIN32
int close_socket(int sock) {
  ::shutdown(sock, SHUT_RDWR);
  return close(sock);
}
#else
int close_socket(SOCKET sock) {
  ::shutdown(sock, SD_BOTH);
  return closesocket(sock);
}
#endif
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
      throw std::runtime_error("wait_for_port_ready(): socket() failed: " +
                               std::to_string(mysqlrouter::get_socket_errno()));
    }
    std::shared_ptr<void> exit_close_socket(
        nullptr, [&](void *) { close_socket(sock_id); });

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
        throw std::system_error(mysqlrouter::get_socket_errno(),
                                std::system_category());
      }
#else
      if (errno == EADDRNOTAVAIL) {
        throw std::system_error(mysqlrouter::get_socket_errno(),
                                std::generic_category());
      }
#endif
      const auto step = std::min(timeout, step_ms);
      std::this_thread::sleep_for(std::chrono::milliseconds(step));
      timeout -= step;
    }
  } while (status < 0 && timeout > std::chrono::steady_clock::now() - started);

  return status >= 0;
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
