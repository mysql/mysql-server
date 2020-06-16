/*
 * Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#ifndef PLUGIN_X_SRC_INTERFACE_TEMPORARY_ACCOUNT_LOCKER_H_
#define PLUGIN_X_SRC_INTERFACE_TEMPORARY_ACCOUNT_LOCKER_H_

#include <string>

#include "plugin/x/ngs/include/ngs/error_code.h"

namespace xpl {
namespace iface {

class Temporary_account_locker {
 public:
  virtual ~Temporary_account_locker() = default;
  virtual ngs::Error_code check(const std::string &user,
                                const std::string &host,
                                const int64_t max_failed_login_attempts,
                                const int64_t password_lock_days,
                                const bool is_password_pass) = 0;

  virtual void clear(const std::string &user, const std::string &host) = 0;
  virtual void clear() = 0;
};

}  // namespace iface
}  // namespace xpl

#endif  // PLUGIN_X_SRC_INTERFACE_TEMPORARY_ACCOUNT_LOCKER_H_
