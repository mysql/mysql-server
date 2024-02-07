/*
  Copyright (c) 2015, 2024, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef MYSQLROUTER_UTILS_INCLUDED
#define MYSQLROUTER_UTILS_INCLUDED

#include "mysqlrouter/router_export.h"

#include <sys/stat.h>  // mode_t

#include <chrono>
#include <cstdint>
#include <functional>
#include <sstream>
#include <string>

#include "my_compiler.h"  // MY_ATTRIBUTE

#include "mysql/harness/stdx/expected.h"

#ifdef _WIN32
extern "C" {
extern bool ROUTER_LIB_EXPORT g_windows_service;
}
#endif

namespace mysqlrouter {

#ifndef _WIN32
using perm_mode = mode_t;
#else
using perm_mode = int;
#endif
/** @brief Constant for directory accessible only for the owner */
extern const perm_mode ROUTER_LIB_EXPORT kStrictDirectoryPerm;

// Some (older) compiler have no std::to_string available
template <typename T>
std::string to_string(const T &data) {
  std::ostringstream os;
  os << data;
  return os.str();
}

// represent milliseconds as floating point seconds
std::string ROUTER_LIB_EXPORT
ms_to_seconds_string(const std::chrono::milliseconds &msec);

/**
 * Validates a string containing a TCP port
 *
 * Validates whether the data can be used as a TCP port. A TCP port is
 * a valid number in the range of 0 and 65535. The returned integer is
 * of type uint16_t.
 *
 * An empty data string will result in TCP port 0 to be returned.
 *
 * Throws runtime_error when the given string can not be converted
 * to an integer or when the integer is to big.
 *
 * @param data string containing the TCP port number
 * @return uint16_t the TCP port number
 */
uint16_t ROUTER_LIB_EXPORT get_tcp_port(const std::string &data);

/** @brief Dumps buffer as hex values
 *
 * Debugging function which dumps the given buffer as hex values
 * in rows of 16 bytes. When literals is true, characters in a-z
 * or A-Z, are printed as-is.
 *
 * @param buffer char array or front of vector<uint8_t>
 * @param count number of bytes to dump
 * @return string containing the dump
 */
std::string hexdump(const unsigned char *buffer, size_t count);

/** @brief Prompts for a password from the console.
 */
std::string ROUTER_LIB_EXPORT prompt_password(const std::string &prompt);

/** @brief Override default prompt password function
 */
void ROUTER_LIB_EXPORT
set_prompt_password(const std::function<std::string(const std::string &)> &f);

#ifdef _WIN32
/** @brief Returns whether if the router process is running as a Windows Service
 */
bool ROUTER_LIB_EXPORT is_running_as_service();

/** @brief Writes to the Windows event log.
 *
 * @param msg Message to log
 *
 * @throws std::runtime_error in case of an error
 */
void ROUTER_LIB_EXPORT write_windows_event_log(const std::string &msg);

#endif

/** @brief Substitutes placeholders of environment variables in a string
 *
 * Substitutes placeholders of environment variables in a string. A
 * placeholder contains the name of the variable and will be fetched
 * from the environment. The substitution is done in-place.
 *
 * Note that it is not an error to pass a string with no variable to
 * be substituted - in such case success will be returned, and the
 * original string will remain unchanged.
 * Also note, that if an error occurs, the resulting string value is
 * undefined (it will be left in an inconsistent state).
 *
 * @return bool (success flag)
 */
bool ROUTER_LIB_EXPORT substitute_envvar(std::string &line) noexcept;

/*
 * @brief Substitutes placeholder of particular environment variable in file
 * path.
 *
 * @param s the file path in which variable name is substituted with value
 * @param name The environment variable name
 * @param value The environment variable value
 *
 * @return path to file
 */
std::string ROUTER_LIB_EXPORT substitute_variable(const std::string &s,
                                                  const std::string &name,
                                                  const std::string &value);

bool my_check_access(const std::string &path);

/** @brief Copy contents of one file to another.
 *
 * Exception thrown if open, create read or write operation fails.
 */
void ROUTER_LIB_EXPORT copy_file(const std::string &from,
                                 const std::string &to);

/**
 * renames file.
 *
 * The function will overwrite the 'to' file if already exists.
 *
 * @param from old filename
 * @param to   new filename
 *
 * @returns stdx::expected<void, std::error_code>
 */
stdx::expected<void, std::error_code> ROUTER_LIB_EXPORT
rename_file(const std::string &from, const std::string &to);

/** @brief Returns whether the socket name passed as parameter is valid
 */
bool ROUTER_LIB_EXPORT is_valid_socket_name(const std::string &socket,
                                            std::string &err_msg);

/** @brief Converts char array to signed integer, intuitively.
 *
 * Using strtol() can be daunting. This function wraps its with logic to ease
 * its use. Features:
 * - errno value is unaltered
 * - on error, default value is returned
 * - unlike strtol(), this function will fail (return default_result) if
 * anything other than digits and sign are present in the char array. Inputs
 * such as " 12" or "abc12.3" will fail, while strtol() would return 12.
 *
 * @param value           char array to get converted
 * @param default_result  value to return in case of nullptr being passed
 */
int strtoi_checked(const char *value, signed int default_result = 0) noexcept;

/** @brief Converts char array to unsigned integer, intuitively.
 *         adding check for null parameter and some conversion restrictions.
 *
 * Using strtoul() can be daunting. This function wraps its with logic to ease
 * its use. Features:
 * - errno value is unaltered
 * - on error, default value is returned
 * - unlike strtoul(), this function will fail (return default_result) if
 * anything other than digits and sign are present in the char array. Inputs
 * such as " 12" or "abc12.3" will fail, while strtoul() would return 12.
 *
 * @param value           char array to get converted
 * @param default_result  value to return in case of nullptr being passed
 */
unsigned ROUTER_LIB_EXPORT
strtoui_checked(const char *value, unsigned int default_result = 0) noexcept;

uint64_t ROUTER_LIB_EXPORT
strtoull_checked(const char *value, uint64_t default_result = 0) noexcept;

}  // namespace mysqlrouter

#endif  // MYSQLROUTER_UTILS_INCLUDED
