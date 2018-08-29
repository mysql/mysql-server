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

#include "mysqlrouter/utils.h"
#include "common.h"
#include "mysql/harness/filesystem.h"

#include <string.h>
#include <algorithm>
#include <cassert>
#include <cctype>
#include <climits>
#include <cstdarg>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <stdexcept>

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
namespace {
extern "C" bool g_windows_service;
}
#endif

using std::string;

const string kValidIPv6Chars = "abcdefgABCDEFG0123456789:";
const string kValidPortChars = "0123456789";

namespace mysqlrouter {

#ifndef _WIN32
const perm_mode kStrictDirectoryPerm = S_IRWXU;
#else
const perm_mode kStrictDirectoryPerm = 0;
#endif

void MockOfstream::open(const char *filename,
                        ios_base::openmode mode /*= ios_base::out*/) {
  // deal properly with A, B, C, B scenario
  // (without this, last B would create a 4th file which would not be tracked by
  // map)
  if (filenames_.count(filename)) {
    erase_file(filenames_.at(filename));
    filenames_.erase(filename);
  }

  std::string fake_filename = gen_fake_filename(filenames_.size());
  filenames_.emplace(filename, fake_filename);

  std::ofstream::open(fake_filename, mode);
}

/*static*/ std::map<std::string, std::string> MockOfstream::filenames_;

/*static*/ void MockOfstream::erase_file(const std::string &filename) {
  remove(filename.c_str());
}

/*static*/ std::string MockOfstream::gen_fake_filename(unsigned long i) {
#ifndef _WIN32
  return std::string("/tmp/mysqlrouter_mockfile") + std::to_string(i);
#else
  return std::string("C:\\temp\\mysqlrouter_mockfile") + std::to_string(i);
#endif
}

std::vector<string> wrap_string(const string &to_wrap, size_t width,
                                size_t indent_size) {
  size_t curr_pos = 0;
  size_t wrap_pos = 0;
  size_t prev_pos = 0;
  string work{to_wrap};
  std::vector<string> res{};
  auto indent = string(indent_size, ' ');
  auto real_width = width - indent_size;

  size_t str_size = work.size();
  if (str_size < real_width) {
    res.push_back(indent + work);
  } else {
    work.erase(std::remove(work.begin(), work.end(), '\r'), work.end());
    std::replace(work.begin(), work.end(), '\t', ' '), work.end();
    str_size = work.size();

    do {
      curr_pos = prev_pos + real_width;

      // respect forcing newline
      wrap_pos = work.find("\n", prev_pos);
      if (wrap_pos == string::npos || wrap_pos > curr_pos) {
        // No new line found till real_width
        wrap_pos = work.find_last_of(" ", curr_pos);
      }
      if (wrap_pos != string::npos) {
        res.push_back(indent + work.substr(prev_pos, wrap_pos - prev_pos));
        prev_pos = wrap_pos + 1;  // + 1 to skip space
      } else {
        break;
      }
    } while (str_size - prev_pos > real_width ||
             work.find("\n", prev_pos) != string::npos);
    res.push_back(indent + work.substr(prev_pos));
  }

  return res;
}

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
    throw std::runtime_error("Could not create file '" + to +
                             "': " + mysql_harness::get_strerror(errno));
  }
  ifile.open(from, std::ofstream::in | std::ofstream::binary);
  if (ifile.fail()) {
    throw std::runtime_error("Could not open file '" + from +
                             "': " + mysql_harness::get_strerror(errno));
  }

  ofile << ifile.rdbuf();

  ofile.close();
  ifile.close();
}

int rename_file(const std::string &from, const std::string &to) {
#ifndef _WIN32
  return rename(from.c_str(), to.c_str());
#else
  // In Windows, rename fails if the file destination alreayd exists, so ...
  if (MoveFileExA(
          from.c_str(), to.c_str(),
          MOVEFILE_REPLACE_EXISTING |  // override existing file
              MOVEFILE_COPY_ALLOWED |  // allow copy of file to different drive
              MOVEFILE_WRITE_THROUGH   // don't return until the operation is
                                       // physically finished
          ))
    return 0;
  else
    return -1;
#endif
}

namespace {
int mkdir_wrapper(const std::string &dir, perm_mode mode) {
#ifndef _WIN32
  return ::mkdir(dir.c_str(), mode);
#else
  return _mkdir(dir.c_str());
#endif
}

int mkdir_recursive(const mysql_harness::Path &path, perm_mode mode) {
  if (path.str().empty() || path.c_str() == mysql_harness::Path::root_directory)
    return -1;

  // mkdir -p succeeds even if the directory one tries to create exists
  if (path.exists()) {
    return path.is_directory() ? 0 : -1;
  }

  const auto parent = path.dirname();
  if (!parent.exists()) {
    auto res = mkdir_recursive(parent, mode);
    if (res != 0) return res;
  }

  return mkdir_wrapper(path.str(), mode);
}
}  // namespace

int mkdir(const std::string &dir, perm_mode mode, bool recursive /*= false */) {
  if (!recursive) {
    return mkdir_wrapper(dir, mode);
  }

  return mkdir_recursive(mysql_harness::Path(dir), mode);
}

bool substitute_envvar(std::string &line) noexcept {
  size_t pos_start;
  size_t pos_end;

  pos_start = line.find("ENV{");
  if (pos_start == string::npos) {
    return true;  // no environment variable placeholder found -> this is not an
                  // error, just a no-op
  }

  pos_end = line.find("}", pos_start + 4);
  if (pos_end == string::npos) {
    return false;  // environment placeholder not closed (missing '}')
  }

  string env_var = line.substr(pos_start + 4, pos_end - pos_start - 4);
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

string string_format(const char *format, ...) {
  va_list args;
  va_start(args, format);
  va_list args_next;
  va_copy(args_next, args);

  int size = std::vsnprintf(nullptr, 0, format, args);
  std::vector<char> buf(static_cast<size_t>(size) + 1U);
  va_end(args);

  std::vsnprintf(buf.data(), buf.size(), format, args_next);
  va_end(args_next);

  return string(buf.begin(), buf.end() - 1);
}

std::string ms_to_seconds_string(const std::chrono::milliseconds &msec) {
  std::ostringstream os;
  os.imbue(std::locale("C"));
  std::chrono::duration<double> seconds = msec;
  os << seconds.count();
  return os.str();
}

std::pair<string, uint16_t> split_addr_port(string data) {
  size_t pos;
  string addr;
  uint16_t port = 0;
  trim(data);

  if (data.at(0) == '[') {
    // IPv6 with port
    pos = data.find(']');
    if (pos == string::npos) {
      throw std::runtime_error(
          "invalid IPv6 address: missing closing square bracket");
    }
    addr.assign(data, 1, pos - 1);
    if (addr.find_first_not_of(kValidIPv6Chars) != string::npos) {
      throw std::runtime_error("invalid IPv6 address: illegal character(s)");
    }
    pos = data.find(":", pos);
    if (pos != string::npos) {
      try {
        port = get_tcp_port(data.substr(pos + 1));
      } catch (const std::runtime_error &exc) {
        throw std::runtime_error("invalid TCP port: " + string(exc.what()));
      }
    }
  } else if (std::count(data.begin(), data.end(), ':') > 1) {
    // IPv6 without port
    pos = data.find(']');
    if (pos != string::npos) {
      throw std::runtime_error(
          "invalid IPv6 address: missing opening square bracket");
    }
    if (data.find_first_not_of(kValidIPv6Chars) != string::npos) {
      throw std::runtime_error("invalid IPv6 address: illegal character(s)");
    }
    addr.assign(data);
  } else {
    // IPv4 or address
    pos = data.find(":");
    addr = data.substr(0, pos);
    if (pos != string::npos) {
      try {
        port = get_tcp_port(data.substr(pos + 1));
      } catch (const std::runtime_error &exc) {
        throw std::runtime_error("invalid TCP port: " + string(exc.what()));
      }
    }
  }

  return std::make_pair(addr, port);
}

uint16_t get_tcp_port(const string &data) {
  int port;

  // We refuse data which is bigger than 5 characters
  if (data.find_first_not_of(kValidPortChars) != string::npos ||
      data.size() > 5) {
    throw std::runtime_error("invalid characters or too long");
  }

  try {
    port = data.empty()
               ? 0
               : static_cast<int>(std::strtol(data.c_str(), nullptr, 10));
  } catch (const std::invalid_argument &exc) {
    throw std::runtime_error("convertion to integer failed");
  } catch (const std::out_of_range &exc) {
    throw std::runtime_error("impossible port number (out-of-range)");
  }

  if (port > UINT16_MAX) {
    throw std::runtime_error("impossible port number");
  }
  return static_cast<uint16_t>(port);
}

std::vector<string> split_string(const string &data, const char delimiter,
                                 bool allow_empty) {
  std::stringstream ss(data);
  std::string token;
  std::vector<string> result;

  if (data.empty()) {
    return {};
  }

  while (std::getline(ss, token, delimiter)) {
    if (token.empty() && !allow_empty) {
      // Skip empty
      continue;
    }
    result.push_back(token);
  }

  // When last character is delimiter, it denotes an empty token
  if (allow_empty && data.back() == delimiter) {
    result.push_back("");
  }

  return result;
}

void left_trim(string &str) {
  str.erase(str.begin(), std::find_if_not(str.begin(), str.end(), ::isspace));
}

void right_trim(string &str) {
  str.erase(std::find_if_not(str.rbegin(), str.rend(), ::isspace).base(),
            str.end());
}

void trim(string &str) {
  left_trim(str);
  right_trim(str);
}

string hexdump(const unsigned char *buffer, size_t count, long start,
               bool literals) {
  std::ostringstream os;

  using std::hex;
  using std::setfill;
  using std::setw;

  int w = 16;
  buffer += start;
  size_t n = 0;
  for (const unsigned char *ptr = buffer; n < count; ++n, ++ptr) {
    if (literals &&
        ((*ptr >= 0x41 && *ptr <= 0x5a) || (*ptr >= 61 && *ptr <= 0x7a))) {
      os << setfill(' ') << setw(2) << *ptr;
    } else {
      os << setfill('0') << setw(2) << hex << static_cast<int>(*ptr);
    }
    if (w == 1) {
      os << std::endl;
      w = 16;
    } else {
      os << " ";
      --w;
    }
  }
  // Make sure there is always a new line on the last line
  if (w < 16) {
    os << std::endl;
  }
  return os.str();
}

/*
 * Returns the last system specific error description (using GetLastError in
 * Windows or errno in Unix/OSX).
 */
std::string get_last_error(int myerrnum) {
#ifdef _WIN32
  DWORD dwCode = myerrnum ? myerrnum : GetLastError();
  LPTSTR lpMsgBuf;

  FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                    FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL, dwCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                (LPTSTR)&lpMsgBuf, 0, NULL);
  std::string msgerr = "SystemError: ";
  msgerr += lpMsgBuf;
  msgerr += "with error code %d.";
  std::string result = string_format(msgerr.c_str(), dwCode);
  LocalFree(lpMsgBuf);
  return result;
#else
  char sys_err[64];
  int errnum = myerrnum ? myerrnum : errno;

  sys_err[0] = 0;  // init, in case strerror_r() fails

  // we do this #ifdef dance because on unix systems strerror_r() will generate
  // a warning if we don't collect the result (warn_unused_result attribute)
#if ((defined _POSIX_C_SOURCE && (_POSIX_C_SOURCE >= 200112L)) || \
     (defined _XOPEN_SOURCE && (_XOPEN_SOURCE >= 600))) &&        \
    !defined _GNU_SOURCE
  int r = strerror_r(errno, sys_err, sizeof(sys_err));
  (void)r;  // silence unused variable;
#elif defined(_GNU_SOURCE) && defined(__GLIBC__)
  const char *r = strerror_r(errno, sys_err, sizeof(sys_err));
  (void)r;  // silence unused variable;
#else
  strerror_r(errno, sys_err, sizeof(sys_err));
#endif

  std::string s = sys_err;
  s += "with errno %d.";
  std::string result = string_format(s.c_str(), errnum);
  return result;
#endif
}

#ifndef _WIN32
static string default_prompt_password(const string &prompt) {
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
  string result;
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
static string default_prompt_password(const string &prompt) {
  std::cout << prompt << ": " << std::flush;

  // prevent showing input
  HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
  DWORD mode;
  GetConsoleMode(hStdin, &mode);
  mode &= ~ENABLE_ECHO_INPUT;
  SetConsoleMode(hStdin, mode & (~ENABLE_ECHO_INPUT));

  string result;
  std::getline(std::cin, result);

  // reset
  SetConsoleMode(hStdin, mode);

  std::cout << std::endl;
  return result;
}
#endif

static std::function<string(const string &)> g_prompt_password =
    default_prompt_password;

void set_prompt_password(
    const std::function<std::string(const std::string &)> &f) {
  g_prompt_password = f;
}

string prompt_password(const std::string &prompt) {
  return g_prompt_password(prompt);
}

int get_socket_errno() noexcept {
#ifdef _WIN32
  return GetLastError();
#else
  return errno;
#endif
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
    DeregisterEventSource(event_src);
  } else {
    throw std::runtime_error("Cannot create event log source, error: " +
                             std::to_string(GetLastError()));
  }
}
#endif

bool is_valid_socket_name(const std::string &socket, std::string &err_msg) {
  bool result = true;

#ifndef _WIN32
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
  static_assert(sizeof(RET) <= sizeof(decltype(
                                   std::strtol((char *)0, (char **)0, (int)0))),
                "This function uses strtol() to convert signed integers, "
                "therefore the integer bit width cannot be larger than what it "
                "supports.");
  static_assert(sizeof(RET) <= sizeof(decltype(std::strtoul(
                                   (char *)0, (char **)0, (int)0))),
                "This function uses strtoul() to convert unsigned integers, "
                "therefore the integer bit width cannot be larger than what it "
                "supports.");

  if (value == nullptr) return default_value;

  // Verify that input string consists of only valid_chars.  The idea is to
  // impose extra restrictions on top of those implemented in conv_func,
  // particularly to disallow:
  // - whitespace characters
  // - decimal numbers
  // Futher validation is responsibility of conv_func.
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

int strtoi_checked(const char *value, signed int default_value) noexcept {
  return strtoX_checked_common<signed int>(value, default_value);
}

unsigned strtoui_checked(const char *value,
                         unsigned int default_value) noexcept {
  return strtoX_checked_common<unsigned int>(value, default_value);
}

#ifndef _WIN32

// class SysUserOperations

SysUserOperations *SysUserOperations::instance() {
  static SysUserOperations instance_;
  return &instance_;
}

int SysUserOperations::initgroups(const char *user, gid_type gid) {
  return ::initgroups(user, gid);
}

int SysUserOperations::setgid(gid_t gid) { return ::setgid(gid); }

int SysUserOperations::setuid(uid_t uid) { return ::setuid(uid); }

int SysUserOperations::setegid(gid_t gid) { return ::setegid(gid); }

int SysUserOperations::seteuid(uid_t uid) { return ::seteuid(uid); }

uid_t SysUserOperations::geteuid() { return ::geteuid(); }

struct passwd *SysUserOperations::getpwnam(const char *name) {
  return ::getpwnam(name);
}

struct passwd *SysUserOperations::getpwuid(uid_t uid) {
  return ::getpwuid(uid);
}

int SysUserOperations::chown(const char *file, uid_t owner, gid_t group) {
  return ::chown(file, owner, group);
}

void set_owner_if_file_exists(const std::string &filepath,
                              const std::string &username,
                              struct passwd *user_info_arg,
                              SysUserOperationsBase *sys_user_operations) {
  assert(user_info_arg != nullptr);
  assert(sys_user_operations != nullptr);

  if (sys_user_operations->chown(filepath.c_str(), user_info_arg->pw_uid,
                                 user_info_arg->pw_gid) == -1) {
    if (errno != ENOENT) {  // Not such file or directory is not an error
      std::string info;
      if (errno == EACCES || errno == EPERM) {
        info =
            "\nOne possible reason can be that the root user does not have "
            "proper "
            "rights because of root_squash on the NFS share.\n";
      }

      throw std::runtime_error(string_format(
          "Can't set ownership of file '%s' to the user '%s'. "
          "error: %s. %s",
          filepath.c_str(), username.c_str(), strerror(errno), info.c_str()));
    }
  }
}

static bool check_if_root(const std::string &username,
                          SysUserOperationsBase *sys_user_operations) {
  auto user_id = sys_user_operations->geteuid();

  if (user_id) {
    /* If real user is same as given with --user don't treat it as an error */
    struct passwd *tmp_user_info =
        sys_user_operations->getpwnam(username.c_str());
    if ((!tmp_user_info || user_id != tmp_user_info->pw_uid)) {
      throw std::runtime_error(string_format(
          "One can only use the -u/--user switch if running as root"));
    }
    return false;
  }

  return true;
}

static passwd *get_user_info(const std::string &username,
                             SysUserOperationsBase *sys_user_operations) {
  struct passwd *tmp_user_info;
  bool failed = false;

  if (!(tmp_user_info = sys_user_operations->getpwnam(username.c_str()))) {
    // Allow a numeric uid to be used
    const char *pos;
    for (pos = username.c_str(); std::isdigit(*pos); pos++)
      ;

    if (*pos)  // Not numeric id
      failed = true;
    else if (!(tmp_user_info = sys_user_operations->getpwuid(
                   (uid_t)atoi(username.c_str()))))
      failed = true;
  }

  if (failed) {
    throw std::runtime_error(
        string_format("Can't use user '%s'. "
                      "Please check that the user exists!",
                      username.c_str()));
  }

  return tmp_user_info;
}

struct passwd *check_user(const std::string &username, bool must_be_root,
                          SysUserOperationsBase *sys_user_operations) {
  assert(sys_user_operations != nullptr);
  if (username.empty()) {
    throw std::runtime_error("Empty user name in check_user() function.");
  }

  if (must_be_root) {
    if (!check_if_root(username, sys_user_operations)) return nullptr;
  }

  return get_user_info(username, sys_user_operations);
}

static void set_user_priv(const std::string &username,
                          struct passwd *user_info_arg, bool permanently,
                          SysUserOperationsBase *sys_user_operations) {
  assert(user_info_arg != nullptr);
  assert(sys_user_operations != nullptr);

  sys_user_operations->initgroups(
      (char *)username.c_str(),
      (SysUserOperationsBase::gid_type)user_info_arg->pw_gid);

  if (permanently) {
    if (sys_user_operations->setgid(user_info_arg->pw_gid) == -1) {
      throw std::runtime_error(
          string_format("Error trying to set the user. "
                        "setgid failed: %s ",
                        strerror(errno)));
    }

    if (sys_user_operations->setuid(user_info_arg->pw_uid) == -1) {
      throw std::runtime_error(
          string_format("Error trying to set the user. "
                        "setuid failed: %s ",
                        strerror(errno)));
    }
  } else {
    if (sys_user_operations->setegid(user_info_arg->pw_gid) == -1) {
      throw std::runtime_error(
          string_format("Error trying to set the user. "
                        "setegid failed: %s ",
                        strerror(errno)));
    }

    if (sys_user_operations->seteuid(user_info_arg->pw_uid) == -1) {
      throw std::runtime_error(
          string_format("Error trying to set the user. "
                        "seteuid failed: %s ",
                        strerror(errno)));
    }
  }
}

void set_user(const std::string &username, bool permanently,
              SysUserOperationsBase *sys_user_operations) {
  auto user_info = check_user(username, permanently, sys_user_operations);
  if (user_info != nullptr) {
    set_user_priv(username, user_info, permanently, sys_user_operations);
  }
}

#endif

}  // namespace mysqlrouter
