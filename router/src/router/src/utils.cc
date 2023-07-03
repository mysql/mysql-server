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

#include "mysqlrouter/utils.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <climits>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <system_error>

#ifndef _WIN32
#include <fcntl.h>
#include <grp.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <termios.h>
#include <unistd.h>
#else
#include <direct.h>
#include <io.h>
#include <stdio.h>
#include <windows.h>

extern "C" bool g_windows_service = false;
#endif

#include "mysql/harness/filesystem.h"
#include "mysql/harness/net_ts/internet.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/string_utils.h"
#include "mysql/harness/utility/string.h"

using mysql_harness::trim;
using mysql_harness::utility::string_format;

const char kValidPortChars[] = "0123456789";

namespace mysqlrouter {

#ifndef _WIN32
const perm_mode kStrictDirectoryPerm = S_IRWXU;
#else
const perm_mode kStrictDirectoryPerm = 0;
#endif

bool my_check_access(const std::string &path) {
#ifndef _WIN32
  return (access(path.c_str(), R_OK | X_OK) == 0);
#else
  return (_access(path.c_str(), 0x04) == 0);
#endif
}

void copy_file(const std::string &from, const std::string &to) {
  std::ofstream ofile;
  std::ifstream ifile;

  ofile.open(to,
             std::ofstream::out | std::ofstream::binary | std::ofstream::trunc);
  if (ofile.fail()) {
    throw std::system_error(errno, std::generic_category(),
                            "Could not create file '" + to + "'");
  }
  ifile.open(from, std::ofstream::in | std::ofstream::binary);
  if (ifile.fail()) {
    throw std::system_error(errno, std::generic_category(),
                            "Could not open file '" + from + "'");
  }

  ofile << ifile.rdbuf();

  ofile.close();
  ifile.close();
}

stdx::expected<void, std::error_code> rename_file(const std::string &from,
                                                  const std::string &to) {
#ifndef _WIN32
  if (0 != rename(from.c_str(), to.c_str())) {
    return stdx::make_unexpected(
        std::error_code{errno, std::generic_category()});
  }
#else
  // In Windows, rename fails if the file destination already exists, so ...
  if (0 ==
      MoveFileExA(
          from.c_str(), to.c_str(),
          MOVEFILE_REPLACE_EXISTING |  // override existing file
              MOVEFILE_COPY_ALLOWED |  // allow copy of file to different drive
              MOVEFILE_WRITE_THROUGH   // don't return until the operation is
                                       // physically finished
          )) {
    return stdx::make_unexpected(std::error_code{
        static_cast<int>(GetLastError()), std::system_category()});
  }
#endif
  return {};
}

bool substitute_envvar(std::string &line) noexcept {
  size_t pos_start;
  size_t pos_end;

  pos_start = line.find("ENV{");
  if (pos_start == std::string::npos) {
    return true;  // no environment variable placeholder found -> this is not an
                  // error, just a no-op
  }

  pos_end = line.find("}", pos_start + 4);
  if (pos_end == std::string::npos) {
    return false;  // environment placeholder not closed (missing '}')
  }

  std::string env_var = line.substr(pos_start + 4, pos_end - pos_start - 4);
  if (env_var.empty()) {
    return false;  // no environment variable name found in placeholder
  }

  const char *env_var_value = std::getenv(env_var.c_str());
  if (env_var_value == nullptr) {
    return false;  // unknown environment variable
  }

  // substitute the variable and return success
  line.replace(pos_start, env_var.size() + 5, env_var_value);
  return true;
}

std::string substitute_variable(const std::string &s, const std::string &name,
                                const std::string &value) {
  std::string r(s);
  std::string::size_type p;
  while ((p = r.find(name)) != std::string::npos) {
    std::string tmp(r.substr(0, p));
    tmp.append(value);
    tmp.append(r.substr(p + name.size()));
    r = tmp;
  }
  mysqlrouter::substitute_envvar(r);
  mysql_harness::Path path(r);
  if (path.exists())
    return path.real_path().str();
  else
    return r;
}

std::string ms_to_seconds_string(const std::chrono::milliseconds &msec) {
  std::ostringstream os;
  os.imbue(std::locale("C"));
  std::chrono::duration<double> seconds = msec;
  os << seconds.count();
  return os.str();
}

uint16_t get_tcp_port(const std::string &data) {
  int port;

  // We refuse data which is bigger than 5 characters
  if (data.size() > 5) {
    throw std::runtime_error("too long");
  }

  if (data.find_first_not_of(kValidPortChars) != std::string::npos) {
    throw std::runtime_error("invalid characters");
  }

  try {
    port = data.empty()
               ? 0
               : static_cast<int>(std::strtol(data.c_str(), nullptr, 10));
  } catch (const std::invalid_argument &) {
    throw std::runtime_error("convertion to integer failed");
  } catch (const std::out_of_range &) {
    throw std::runtime_error("impossible port number (out-of-range)");
  }

  if (port > static_cast<int>(UINT16_MAX)) {
    throw std::runtime_error("out of range. Max " + std::to_string(UINT16_MAX));
  }
  return static_cast<uint16_t>(port);
}

#ifndef _WIN32
static std::string default_prompt_password(const std::string &prompt) {
  struct termios console;
  bool no_terminal = false;
  if (tcgetattr(STDIN_FILENO, &console) != 0) {
    // this can happen if we're running without a terminal, in that case
    // we don't care about terminal attributes
    no_terminal = true;
  }
  std::cout << prompt << ": " << std::flush;

  if (!no_terminal) {
    // prevent showing input
    console.c_lflag &= ~(uint)ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &console);
  }
  std::string result;
  std::getline(std::cin, result);

  if (!no_terminal) {
    // reset
    console.c_lflag |= ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &console);
  }
  std::cout << std::endl;
  return result;
}
#else
static std::string default_prompt_password(const std::string &prompt) {
  std::cout << prompt << ": " << std::flush;

  // prevent showing input
  HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
  DWORD mode;
  GetConsoleMode(hStdin, &mode);
  mode &= ~ENABLE_ECHO_INPUT;
  SetConsoleMode(hStdin, mode & (~ENABLE_ECHO_INPUT));

  std::string result;
  std::getline(std::cin, result);

  // reset
  SetConsoleMode(hStdin, mode);

  std::cout << std::endl;
  return result;
}
#endif

static std::function<std::string(const std::string &)> g_prompt_password =
    default_prompt_password;

void set_prompt_password(
    const std::function<std::string(const std::string &)> &f) {
  g_prompt_password = f;
}

std::string prompt_password(const std::string &prompt) {
  return g_prompt_password(prompt);
}

#ifdef _WIN32

bool is_running_as_service() { return ::g_windows_service; }

void write_windows_event_log(const std::string &msg) {
  static const std::string event_source_name = "MySQL Router";
  HANDLE event_src = NULL;
  LPCSTR strings[2] = {NULL, NULL};
  event_src = RegisterEventSourceA(NULL, event_source_name.c_str());
  if (event_src) {
    strings[0] = event_source_name.c_str();
    strings[1] = msg.c_str();
    ReportEventA(event_src, EVENTLOG_ERROR_TYPE, 0, 0, NULL, 2, 0, strings,
                 NULL);
    BOOL ok = DeregisterEventSource(event_src);
    if (!ok) {
      throw std::runtime_error(
          "Cannot destroy event log source after logging '" + msg +
          "', error: " + std::to_string(GetLastError()));
    }
  } else {
    throw std::runtime_error("Cannot create event log source, error: " +
                             std::to_string(GetLastError()));
  }
}
#endif

bool is_valid_socket_name(const std::string &socket, std::string &err_msg) {
  bool result = true;

#ifdef _WIN32
  UNREFERENCED_PARAMETER(socket);
  UNREFERENCED_PARAMETER(err_msg);
#else
  result = socket.size() <= (sizeof(sockaddr_un().sun_path) - 1);
  err_msg = "Socket file path can be at most " +
            to_string(sizeof(sockaddr_un().sun_path) - 1) +
            " characters (was " + to_string(socket.size()) + ")";
#endif

  return result;
}

template <typename RET>
static RET strtoX_checked_common(const char *value,
                                 RET default_value) noexcept {
  static_assert(std::is_integral<RET>::value,
                "This template function is meant for integers.");
  static_assert(sizeof(RET) <= sizeof(decltype(  //
                                   std::strtol("", (char **)nullptr, (int)0))),
                "This function uses strtol() to convert signed integers, "
                "therefore the integer bit width cannot be larger than what it "
                "supports.");
  static_assert(sizeof(RET) <= sizeof(decltype(  //
                                   std::strtoul("", (char **)nullptr, (int)0))),
                "This function uses strtoul() to convert unsigned integers, "
                "therefore the integer bit width cannot be larger than what it "
                "supports.");

  if (value == nullptr) return default_value;

  // Verify that input string consists of only valid_chars.  The idea is to
  // impose extra restrictions on top of those implemented in conv_func,
  // particularly to disallow:
  // - whitespace characters
  // - decimal numbers
  // Further validation is responsibility of conv_func.
  {
    // Compute (roughly) the max number of base10 digits RET can have.
    //   max(1 byte)  = 255 -> 3 digits,
    //   max(2 bytes) = 65,535 -> 5 digits,
    //   max(4 bytes) = 4,294,967,295 -> 10 digits,
    // etc
    constexpr int kMaxDigits = static_cast<int>(
        (float)sizeof(RET) * 2.41 + 1.0);  // log10(2^8) = 2.408, +1 to round up

    bool found_terminator = false;
    for (int i = 0; i < kMaxDigits + 2;
         i++) {  // +2 for sign and string-terminator
      const char c = value[i];
      if (c == 0) {
        found_terminator = true;
        break;
      }
      if (!(('0' <= c && c <= '9') ||
            (c == '-' && std::is_signed<RET>::value) || c == '+'))
        return default_value;
    }

    if (!found_terminator) return default_value;
  }

  // NOTE: we need to play with errno here as it is not enough to check
  // for LONG_MIN, LONG_MAX as these are still valid values and ERANGE
  // can be the result of some previous operation
  auto old_errno = errno;
  errno = 0;

  // run strtol() or strtoul() on input string, depending signedness of RET
  char *tmp{nullptr};
  constexpr bool RET_is_signed = std::is_signed<RET>::value;
  typename std::conditional<
      RET_is_signed, decltype(std::strtol(value, &tmp, 10)),
      decltype(std::strtoul(value, &tmp, 10))>::type result =
      RET_is_signed ? std::strtol(value, &tmp, 10)
                    : std::strtoul(value, &tmp, 10);

  // if our operation did not set the errno let's be kind enough
  // to restore it's old value
  auto our_errno = errno;
  if (errno == 0) {
    errno = old_errno;
  }

  // check if the conversion was valid
  if (value == tmp || *tmp != '\0' || our_errno == ERANGE) {
    return default_value;
  }

  // check if the value fits after reducing bit width
  RET r = static_cast<RET>(result);
  if (r == result)  // false if high-order bytes were truncated
    return r;
  else
    return default_value;
}

int strtoi_checked(const char *value, signed int default_result) noexcept {
  return strtoX_checked_common<signed int>(value, default_result);
}

unsigned strtoui_checked(const char *value,
                         unsigned int default_result) noexcept {
  return strtoX_checked_common<unsigned int>(value, default_result);
}

uint64_t strtoull_checked(const char *value, uint64_t default_result) noexcept {
  static_assert(std::numeric_limits<uint64_t>::max() <=
                std::numeric_limits<unsigned long long>::max());

  if (value == nullptr) return default_result;

  char *rest;
  errno = 0;
  unsigned long long toul = std::strtoull(value, &rest, 10);
  uint64_t result = static_cast<uint64_t>(toul);

  if (errno > 0 || *rest != '\0' ||
      result > std::numeric_limits<uint64_t>::max() || result != toul) {
    return default_result;
  }
  return result;
}

}  // namespace mysqlrouter
