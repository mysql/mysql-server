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

#include "plugin/x/src/temporary_account_locker.h"

#include <cinttypes>
#include <limits>

#include "plugin/x/src/helper/to_string.h"
#include "sql/derror.h"  // ER_DEFAULT

namespace xpl {

namespace {

inline int64_t get_password_lock_days_remaining(
    const int64_t &password_lock_days, const chrono::Date_time &check_date,
    const chrono::Date_time &lock_date) {
  if (password_lock_days < 0) return std::numeric_limits<int64_t>::max();

  const auto since_lock = chrono::to_seconds(check_date - lock_date);
  static const int32_t k_seconds_of_day = 86400;
  const auto lock_duration =
      chrono::to_seconds(chrono::Hours(24) * password_lock_days);
  return (lock_duration - since_lock + k_seconds_of_day - 1) / k_seconds_of_day;
}

inline ngs::Error_code SQLError_user_account_blocked(
    const std::string &user, const std::string &host,
    const int64_t max_failed_login_attempts, const int64_t password_lock_days,
    const int64_t password_lock_days_remaining) {
  if (password_lock_days < 0)
    return ngs::SQLError(
        ER_USER_ACCESS_DENIED_FOR_USER_ACCOUNT_BLOCKED_BY_PASSWORD_LOCK,
        user.c_str(), host.c_str(), "unlimited", "unlimited",
        static_cast<uint32_t>(max_failed_login_attempts));
  return ngs::SQLError(
      ER_USER_ACCESS_DENIED_FOR_USER_ACCOUNT_BLOCKED_BY_PASSWORD_LOCK,
      user.c_str(), host.c_str(), to_string(password_lock_days).c_str(),
      to_string(password_lock_days_remaining).c_str(),
      static_cast<uint32_t>(max_failed_login_attempts));
}
}  // namespace

ngs::Error_code Temporary_account_locker::check(
    const std::string &user, const std::string &host,
    const int64_t max_failed_login_attempts, const int64_t password_lock_days,
    const bool is_password_pass, const chrono::Date_time &check_date) {
  DBUG_TRACE;
  DBUG_PRINT("info", ("Account: %s@%s (password: %s)", user.c_str(),
                      host.c_str(), is_password_pass ? "pass" : "fail"));

  if (max_failed_login_attempts == 0 || password_lock_days == 0)
    return is_password_pass ? ngs::Success() : ngs::SQLError_access_denied();

  const auto stored_entry = m_storage.get_entry(user, host);
  const auto is_entry_tracked = stored_entry != nullptr;
  Failed_login_entry entry =
      is_entry_tracked ? *stored_entry : Failed_login_entry();

  if (!is_entry_tracked) {
    if (is_password_pass) return ngs::Success();
    return track_fail_attempt(user, host, max_failed_login_attempts,
                              password_lock_days, check_date, &entry);
  }

  if (entry.is_locked) {
    const auto password_lock_days_remaining = get_password_lock_days_remaining(
        password_lock_days, check_date, entry.lock_date);
    DBUG_PRINT("info", ("Account password lock days remaining: %" PRIi64,
                        password_lock_days_remaining));
    if (password_lock_days_remaining > 0)
      return SQLError_user_account_blocked(
          user, host, max_failed_login_attempts, password_lock_days,
          password_lock_days_remaining);
    entry = {0, false, {}};
  }

  if (is_password_pass) {
    DBUG_PRINT("info", ("Account cleared"));
    m_storage.remove(user, host);
    return ngs::Success();
  }

  return track_fail_attempt(user, host, max_failed_login_attempts,
                            password_lock_days, check_date, &entry);
}

void Temporary_account_locker::clear(const std::string &user,
                                     const std::string &host) {
  DBUG_TRACE;
  DBUG_PRINT("info", ("Clear account: %s@%s", user.c_str(), host.c_str()));
  m_storage.remove(user, host);
}

void Temporary_account_locker::clear() {
  DBUG_TRACE;
  m_storage.clear();
}

const Temporary_account_locker::Failed_login_entry *
Temporary_account_locker::get_entry(const std::string &user,
                                    const std::string &host) const {
  return m_storage.get_entry(user, host);
}

ngs::Error_code Temporary_account_locker::track_fail_attempt(
    const std::string &user, const std::string &host,
    const int64_t max_failed_login_attempts, const int64_t password_lock_days,
    const chrono::Date_time &check_date, Failed_login_entry *entry) {
  ++entry->attempt_count;
  DBUG_PRINT("info", ("Account tracked (%" PRIi64 "/%" PRIi64 ")",
                      entry->attempt_count, max_failed_login_attempts));
  if (entry->attempt_count == max_failed_login_attempts) {
    DBUG_PRINT("info", ("Account locked"));
    entry->is_locked = true;
    entry->lock_date = check_date;
  }
  m_storage.upsert(user, host, *entry);
  return entry->is_locked
             ? SQLError_user_account_blocked(
                   user, host, max_failed_login_attempts, password_lock_days,
                   get_password_lock_days_remaining(
                       password_lock_days, check_date, entry->lock_date))
             : ngs::SQLError(ER_ACCESS_DENIED_ERROR_WITH_PASSWORD, user.c_str(),
                             host.c_str(), my_get_err_msg(ER_YES));
}

}  // namespace xpl
