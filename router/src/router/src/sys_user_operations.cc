/*
  Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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

#include "mysqlrouter/sys_user_operations.h"

#include <cassert>
#include <cerrno>
#include <cstring>
#include <stdexcept>  // runtime_error

#ifndef _WIN32
#include <grp.h>  // initgroups
#include <sys/types.h>
#include <unistd.h>  // getgid
#endif

#include "mysql/harness/utility/string.h"  // string_format

using mysql_harness::utility::string_format;

namespace mysqlrouter {

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
      username.c_str(),
      static_cast<SysUserOperationsBase::gid_type>(user_info_arg->pw_gid));

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
