/*
  Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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

#ifndef MYSQLROUTER_SYS_USER_OPERATIONS_INCLUDED
#define MYSQLROUTER_SYS_USER_OPERATIONS_INCLUDED

#ifndef _WIN32
#include <netdb.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

#include <string>

namespace mysqlrouter {

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
  int setgid(gid_t gid) override;

  /** @brief Thin wrapper around system setuid() */
  int setuid(uid_t uid) override;

  /** @brief Thin wrapper around system setegid() */
  int setegid(gid_t gid) override;

  /** @brief Thin wrapper around system seteuid() */
  int seteuid(uid_t uid) override;

  /** @brief Thin wrapper around system geteuid() */
  uid_t geteuid() override;

  /** @brief Thin wrapper around system getpwnam() */
  struct passwd *getpwnam(const char *name) override;

  /** @brief Thin wrapper around system getpwuid() */
  struct passwd *getpwuid(uid_t uid) override;

  /** @brief Thin wrapper around system chown() */
  int chown(const char *file, uid_t owner, gid_t group) override;

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

#endif  // MYSQLROUTER_UTILS_INCLUDED
