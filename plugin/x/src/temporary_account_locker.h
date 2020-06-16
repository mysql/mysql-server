/* Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef PLUGIN_X_SRC_TEMPORARY_ACCOUNT_LOCKER_H_
#define PLUGIN_X_SRC_TEMPORARY_ACCOUNT_LOCKER_H_

#include <string>
#include <utility>

#include "plugin/x/src/account_credential_storage.h"
#include "plugin/x/src/helper/chrono.h"
#include "plugin/x/src/interface/temporary_account_locker.h"

namespace xpl {

class Temporary_account_locker : public iface::Temporary_account_locker {
 public:
  struct Failed_login_entry {
    int64_t attempt_count{0};
    bool is_locked{false};
    chrono::Date_time lock_date;
  };

  ngs::Error_code check(const std::string &user, const std::string &host,
                        const int64_t max_failed_login_attempts,
                        const int64_t password_lock_days,
                        const bool is_password_pass) override {
    return check(user, host, max_failed_login_attempts, password_lock_days,
                 is_password_pass, chrono::System_clock::now());
  }

  void clear(const std::string &user, const std::string &host) override;
  void clear() override;

  ngs::Error_code check(const std::string &user, const std::string &host,
                        const int64_t max_failed_login_attempts,
                        const int64_t password_lock_days,
                        const bool is_password_pass,
                        const chrono::Date_time &check_date);

  const Failed_login_entry *get_entry(const std::string &user,
                                      const std::string &host) const;

  size_t storage_size() const { return m_storage.size(); }

 private:
  ngs::Error_code track_fail_attempt(const std::string &user,
                                     const std::string &host,
                                     const int64_t max_failed_login_attempts,
                                     const int64_t password_lock_days,
                                     const chrono::Date_time &check_date,
                                     Failed_login_entry *entry);

  Account_credential_storage<Failed_login_entry> m_storage{true};
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_TEMPORARY_ACCOUNT_LOCKER_H_
