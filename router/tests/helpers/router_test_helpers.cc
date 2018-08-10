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

#include "router_test_helpers.h"
#include "cmd_exec.h"
#include "mysql/harness/filesystem.h"
#include "mysqlrouter/utils.h"

#include <cassert>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <thread>

#ifndef _WIN32
#include <regex.h>
#include <unistd.h>
#else
#include <direct.h>
#include <windows.h>
#include <winsock2.h>
#include <regex>
#define getcwd _getcwd
typedef long ssize_t;
#endif

using mysql_harness::Path;

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

bool ends_with(const std::string &str, const std::string &suffix) {
  auto suffix_size = suffix.size();
  auto str_size = str.size();
  return (str_size >= suffix_size &&
          str.compare(str_size - suffix_size, str_size, suffix) == 0);
}

bool starts_with(const std::string &str, const std::string &prefix) {
  auto prefix_size = prefix.size();
  auto str_size = str.size();
  return (str_size >= prefix_size && str.compare(0, prefix_size, prefix) == 0);
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
                nullptr, GetLastError(), LANG_NEUTRAL, message, sizeof(message),
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
#ifndef _WIN32
  regex_t regex;
  auto r = regcomp(&regex, pattern.c_str(), 0);
  if (r) {
    throw std::runtime_error("Error compiling regex pattern: " + pattern);
  }
  r = regexec(&regex, s.c_str(), 0, NULL, 0);
  regfree(&regex);
  return (r == 0);
#else
  std::smatch m;
  std::regex r(pattern);
  bool result = std::regex_search(s, m, r);

  return result;
#endif
}
