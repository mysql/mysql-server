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

#ifndef MYSQLROUTER_UTILS_INCLUDED
#define MYSQLROUTER_UTILS_INCLUDED

#include <chrono>
#include <cstdarg>
#include <cstdint>
#include <functional>
#include <sstream>
#include <string>
#include <vector>
#ifndef _WIN32
#include <netdb.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

#include <stdio.h>
#include <fstream>
#include <iostream>
#include <map>

#include "my_compiler.h"

#include "router_config.h"

namespace mysqlrouter {

#ifndef _WIN32
using perm_mode = mode_t;
#else
using perm_mode = int;
#endif
/** @brief Constant for directory accessible only for the owner */
extern const perm_mode kStrictDirectoryPerm;

/** @class Ofstream
 *  @brief interface to std::ofstream and alternative (mock) implementations
 *
 * std::ofstream is not mockable, because its methods are not virtual. To work
 * around this, we create this interface class, which then acts as superclass to
 * various std::ofstream implementation classes.
 */
class Ofstream : public std::ofstream {
 public:
  // disabled copying the ofstream constructurs as sunpro 12.5 says:
  //
  //   'ofstream' not in 'std::ofstream'
  //
  // If there is a need to have other than the no-param constructor
  // that is inherited by default, a new solution has to be found.
  //
  // using std::ofstream::ofstream;
  virtual ~Ofstream() {}
  virtual void open(const char *filename,
                    std::ios_base::openmode mode = std::ios_base::out) = 0;
  virtual void open(const std::string &filename,
                    std::ios_base::openmode mode = std::ios_base::out) = 0;
};

/** @class RealOfstream
 *  @brief simple std::ofstream adapter, needed for DI purposes
 *
 * This class is just a simple adapter for std::ofstream. It forwards all calls
 * to std::ofstream
 */
class RealOfstream : public Ofstream {
 public:
  // using Ofstream::Ofstream;
  virtual void open(const char *filename,
                    std::ios_base::openmode mode = std::ios_base::out) {
    return std::ofstream::open(filename, mode);
  }
  virtual void open(const std::string &filename,
                    std::ios_base::openmode mode = std::ios_base::out) {
    return open(filename.c_str(), mode);
  }
};

/** @class MockOfstream
 *  @brief mock implementation of std::ofstream
 *
 * The idea behind this class is to allow unit tests to run, without actually
 * causing a mess on disk. So far a minimal implementation is provided, which
 * can be expanded as needed.
 */
class MockOfstream : public Ofstream {
 public:
  MockOfstream(const char *filename, ios_base::openmode mode = ios_base::out) {
    open(filename, mode);
  }

  // mock open
  void open(const char *filename,
            ios_base::openmode mode = ios_base::out) override;
  void open(const std::string &filename,
            std::ios_base::openmode mode = std::ios_base::out) override {
    return open(filename.c_str(), mode);
  }

  // extract the original filename
  static std::string application_to_real_filename(
      const std::string &application_filename) {
    return filenames_.at(application_filename);
  }

  // run this at the end of the unit test
  static void clean_up() {
    for (auto filename : filenames_) erase_file(filename.second);
  }

 private:
  static std::string gen_fake_filename(unsigned long i);
  static void erase_file(const std::string &filename);
  static std::map<std::string, std::string>
      filenames_;  // key = application filename, value = filename on disk
};

// Some (older) compiler have no std::to_string available
template <typename T>
std::string to_string(const T &data) {
  std::ostringstream os;
  os << data;
  return os.str();
}

// represent milliseconds as floating point seconds
std::string ms_to_seconds_string(const std::chrono::milliseconds &msec);

/** @brief Returns string formatted using given data
 *
 * Returns string formatted using given data accepting the same arguments
 * and format specifies as the typical printf.
 *
 * @param format specify how to format the data
 * @param ... variable argument list containing the data
 * @returns formatted text as string
 */
MY_ATTRIBUTE((format(printf, 1, 2)))
std::string string_format(const char *format, ...);

/**
 * Split host and port
 *
 * @param data a string with hostname and port
 * @return std::pair<string, uint16_t> containing address and port
 */
std::pair<std::string, uint16_t> split_addr_port(const std::string data);

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
uint16_t get_tcp_port(const std::string &data);

/** @brief Splits a string using a delimiter
 *
 * Splits a string using the given delimiter. When allow_empty
 * is true (default), tokens can be empty, and will be included
 * as empty in the result.
 *
 * @param data a string to split
 * @param delimiter a char used as delimiter
 * @param allow_empty whether to allow empty tokens or not (default true)
 * @return std::vector<string> containing tokens
 */
std::vector<std::string> split_string(const std::string &data,
                                      const char delimiter,
                                      bool allow_empty = true);

/**
 * Removes leading whitespaces from the string
 *
 * @param str the string to be trimmed
 */
void left_trim(std::string &str);

/**
 * Removes trailing whitespaces from the string
 *
 * @param str the string to be trimmed
 */
void right_trim(std::string &str);

/**
 * Removes both leading and trailing whitespaces from the string
 *
 * @param str the string to be trimmed
 */
void trim(std::string &str);

/** @brief Dumps buffer as hex values
 *
 * Debugging function which dumps the given buffer as hex values
 * in rows of 16 bytes. When literals is true, characters in a-z
 * or A-Z, are printed as-is.
 *
 * @param buffer char array or front of vector<uint8_t>
 * @param count number of bytes to dump
 * @param start from where to start dumping
 * @param literals whether to show a-zA-Z as-is
 * @return string containing the dump
 */
std::string hexdump(const unsigned char *buffer, size_t count, long start = 0,
                    bool literals = false);

/** @brief Returns the platform specific error code of last operation
 * Using errno in UNIX & Linux systems and GetLastError in Windows systems.
 * If myerrnum arg is not zero will use GetLastError in Windows (if myerrnum is
 * zero in Unix will read the *current* errno).
 * @return the error code description
 */
std::string get_last_error(int myerrnum = 0);

/** @brief Returns error number of the last failed socket operation
 */
int get_socket_errno() noexcept;

/** @brief Prompts for a password from the console.
 */
std::string prompt_password(const std::string &prompt);

/** @brief Override default prompt password function
 */
void set_prompt_password(
    const std::function<std::string(const std::string &)> &f);

#ifdef _WIN32
/** @brief Returns whether if the router process is running as a Windows Service
 */
bool is_running_as_service();

/** @brief Writes to the Windows event log.
 */
void write_windows_event_log(const std::string &msg);

#endif

/** @brief Substitutes placeholders of environment variables in a string
 *
 * Substitutes placeholders of environement variables in a string. A
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
bool substitute_envvar(std::string &line) noexcept;

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
std::string substitute_variable(const std::string &s, const std::string &name,
                                const std::string &value);

/** @brief Wraps the given string
 *
 * Wraps the given string based on the spaces between words.
 * New lines are respected; carriage return and tab characters are
 * removed.
 *
 * The `width` specifies how much characters will in each line. It is also
 * possible to prefix each line with a number of spaces using the `indent_size`
 * argument.
 *
 * @param str string to wrap
 * @param width maximum line length
 * @param indent number of spaces to prefix each line with
 * @return vector of strings
 */
std::vector<std::string> wrap_string(const std::string &str, size_t width,
                                     size_t indent);

bool my_check_access(const std::string &path);

/** @brief Creates a directory
 * *
 * @param dir       name (or path) of the directory to create
 * @param mode      permission mode for the created directory
 * @param recursive if true then immitated unix `mkdir -p` recursively
 *                  creating parent directories if needed
 * @return 0 if succeeded, non-0 otherwise
 */
int mkdir(const std::string &dir, perm_mode mode, bool recursive = false);

/** @brief Copy contents of one file to another.
 *
 * Exception thrown if open, create read or write operation fails.
 */
void copy_file(const std::string &from, const std::string &to);

/** @brief renames file, returns 0 if succeed, or positive error code if fails.
 *
 * The function will overwrite the 'to' file if already exists.
 */
int rename_file(const std::string &from, const std::string &to);

/** @brief Returns whether the socket name passed as parameter is valid
 */
bool is_valid_socket_name(const std::string &socket, std::string &err_msg);

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
unsigned strtoui_checked(const char *value,
                         unsigned int default_result = 0) noexcept;

#ifndef _WIN32

/** @class SysUserOperationsBase
 * @brief Base class to allow multiple SysUserOperations implementations
 */
class SysUserOperationsBase {
 public:
#ifdef __APPLE__
  using gid_type = int;
#else
  using gid_type = gid_t;
#endif
  virtual ~SysUserOperationsBase() = default;

  virtual int initgroups(const char *user, gid_type gid) = 0;
  virtual int setgid(gid_t gid) = 0;
  virtual int setuid(uid_t uid) = 0;
  virtual int setegid(gid_t gid) = 0;
  virtual int seteuid(uid_t uid) = 0;
  virtual uid_t geteuid(void) = 0;
  virtual struct passwd *getpwnam(const char *name) = 0;
  virtual struct passwd *getpwuid(uid_t uid) = 0;
  virtual int chown(const char *file, uid_t owner, gid_t group) = 0;
};

/** @class SysUserOperations
 * @brief This class provides implementations of SysUserOperationsBase methods
 */
class SysUserOperations : public SysUserOperationsBase {
 public:
  static SysUserOperations *instance();

  /** @brief Thin wrapper around system initgroups() */
  int initgroups(const char *user, gid_type gid) override;

  /** @brief Thin wrapper around system setgid() */
  virtual int setgid(gid_t gid) override;

  /** @brief Thin wrapper around system setuid() */
  virtual int setuid(uid_t uid) override;

  /** @brief Thin wrapper around system setegid() */
  virtual int setegid(gid_t gid) override;

  /** @brief Thin wrapper around system seteuid() */
  virtual int seteuid(uid_t uid) override;

  /** @brief Thin wrapper around system geteuid() */
  virtual uid_t geteuid() override;

  /** @brief Thin wrapper around system getpwnam() */
  virtual struct passwd *getpwnam(const char *name) override;

  /** @brief Thin wrapper around system getpwuid() */
  virtual struct passwd *getpwuid(uid_t uid) override;

  /** @brief Thin wrapper around system chown() */
  virtual int chown(const char *file, uid_t owner, gid_t group) override;

 private:
  SysUserOperations(const SysUserOperations &) = delete;
  SysUserOperations operator=(const SysUserOperations &) = delete;
  SysUserOperations() = default;
};

/** @brief Sets the owner of selected file/directory if it exists.
 *
 * @throws std::runtime_error in case of an error
 *
 * @param filepath              path to the file/directory this operation
 * applies to
 * @param username              name of the system user that should be new owner
 * of the file
 * @param user_info_arg         passwd structure for the system user that should
 * be new owner of the file
 * @param sys_user_operations   object for the system specific operation that
 * should be used by the function
 */
void set_owner_if_file_exists(
    const std::string &filepath, const std::string &username,
    struct passwd *user_info_arg,
    mysqlrouter::SysUserOperationsBase *sys_user_operations);

/** @brief Sets effective user of the calling process.
 *
 * @throws std::runtime_error in case of an error
 *
 * @param username            name of the system user that the process should
 * switch to
 * @param permanently         if it's tru then if the root is dropping
 * privileges it can't be regained after this call
 * @param sys_user_operations object for the system specific operation that
 * should be used by the function
 */
void set_user(const std::string &username, bool permanently = false,
              mysqlrouter::SysUserOperationsBase *sys_user_operations =
                  SysUserOperations::instance());

/** @brief Checks if the given user can be switched to or made an owner of a
 * selected file.
 *
 * @throws std::runtime_error in case of an error
 *
 * @param username            name of the system user to check
 * @param must_be_root        make sure that the current user is root
 * @param sys_user_operations object for the system specific operation that
 * should be used by the function
 * @return pointer to the user's passwd structure if the user can be switched to
 * or nullptr otherwise
 *
 */
struct passwd *check_user(
    const std::string &username, bool must_be_root,
    mysqlrouter::SysUserOperationsBase *sys_user_operations);

#endif  // ! _WIN32

}  // namespace mysqlrouter

/** @brief Declare test (class)
 *
 * When using FRIEND_TEST() on classes that are not in the same namespace
 * as the test, the test (class) needs to be forward-declared. This marco
 * eases this.
 *
 * @note We need this for unit tests, BUT on the TESTED code side (not in unit
 * test code)
 */
#define DECLARE_TEST(test_case_name, test_name) \
  class test_case_name##_##test_name##_Test

#endif  // MYSQLROUTER_UTILS_INCLUDED
