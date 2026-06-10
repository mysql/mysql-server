/*
  Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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

#ifndef _ROUTER_ACCOUNTS_CLEANER_H_
#define _ROUTER_ACCOUNTS_CLEANER_H_

#include "mysql/harness/logging/logger.h"
#include "mysqlrouter/mysql_session.h"
#include "mysqlrouter/router_mysql_export.h"

namespace mysqlrouter {

class ROUTER_MYSQL_EXPORT MySQLAccountsCleaner {
 public:
  MySQLAccountsCleaner(std::ostream &err_stream) : err_stream_(err_stream) {}

  ~MySQLAccountsCleaner() { undo_create_user_for_new_accounts(); }

  struct UndoCreateAccountList {
    enum { kNotSet = 1, kAllAccounts, kNewAccounts } type = kNotSet;
    std::string accounts;

    void clear() {
      type = kNotSet;
      accounts = "";
    }
  };

  std::string make_account_list(const std::string username,
                                const std::set<std::string> &hostnames);

  void set_session(MySQLSession *session) { session_ = session; }

  void register_tmp_undo_account_list(
      const UndoCreateAccountList &account_list) {
    tmp_undo_create_account_list_ = account_list;
  }

  void register_undo_account_list(const UndoCreateAccountList &account_list) {
    tmp_undo_create_account_list_.clear();
    undo_create_account_list_.push_back(account_list);
  }

  void clear() {
    tmp_undo_create_account_list_.clear();
    undo_create_account_list_.clear();
  }

  /** @brief Deletes Router accounts just created
   *
   * This method runs as a cleanup after something goes wrong.  Its purpose is
   * to undo CREATE USER [IF NOT EXISTS] for accounts that got created during
   * bootstrap.  Note that it will drop only those accounts which did not exist
   * prior to bootstrap (it may be a subset of account names passed to
   * CREATE USER [IF NOT EXISTS]).  If it is not able to determine what this
   * (sub)set is, it will not drop anything - instead it will advise user on
   * how to clean those up manually.
   */
  void undo_create_user_for_new_accounts() noexcept;

 private:
  MySQLSession *session_{nullptr};
  std::ostream &err_stream_;
  UndoCreateAccountList tmp_undo_create_account_list_;
  std::vector<UndoCreateAccountList> undo_create_account_list_;
};

}  // namespace mysqlrouter

#endif
