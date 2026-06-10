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

#include "mysqlrouter/accounts_cleaner.h"

#include "harness_assert.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/logging/registry.h"
#include "mysql/harness/vt100.h"

IMPORT_LOG_FUNCTIONS()

using namespace mysqlrouter;
using namespace std::string_literals;

std::string MySQLAccountsCleaner::make_account_list(
    const std::string username, const std::set<std::string> &hostnames) {
  std::string account_list;
  harness_assert(session_);
  for (const std::string &h : hostnames) {
    if (!account_list.empty()) {
      account_list += ",";
    }
    account_list += session_->quote(username) + "@" + session_->quote(h);
  }
  return account_list;
}

void MySQLAccountsCleaner::undo_create_user_for_new_accounts() noexcept {
  try {  // need to guarantee noexcept

    if (tmp_undo_create_account_list_.type != UndoCreateAccountList::kNotSet) {
      undo_create_account_list_.push_back(tmp_undo_create_account_list_);
    }

    bool error_printed{false};
    for (const auto &list : undo_create_account_list_) {
      switch (list.type) {
        case UndoCreateAccountList::kNotSet:
          // we didn't get around to creating accounts yet -> nothing to do
          return;
        case UndoCreateAccountList::kAllAccounts:
          // fallthrough
        case UndoCreateAccountList::kNewAccounts:
          if (list.accounts.empty()) {
            // even if we created some accounts, none of them were new ->
            // nothing to do
            continue;
          }
      };

      if (!error_printed) {
        err_stream_
            << "FATAL ERROR ENCOUNTERED, attempting to undo new accounts "
               "that were created"
            << std::endl;
        error_printed = true;
      }

      // shorter name
      const std::string &account_list = list.accounts;

      if (list.type == UndoCreateAccountList::kAllAccounts) {
        // we successfully ran CREATE USER [IF NOT EXISTS] on requested
        // accounts, but determining which of them were new (via SHOW WARNINGS)
        // failed.

        err_stream_
            << "\n"
            << Vt100::foreground(Vt100::Color::Red)
            << "ERROR: " << Vt100::render(Vt100::Render::ForegroundDefault)
            << R"(We created account(s), of which at least one already existed.
A fatal error occurred while we tried to determine which account(s) were new,
therefore to be safe, we did not erase any accounts while cleaning-up before
exiting.
You may want to clean those up yourself, if you deem it appropriate.
Here's a full list of accounts that bootstrap tried to create (some of which
might have already existed before bootstrapping):

  )"s << account_list
            << std::endl;

        continue;
      }

      harness_assert(list.type == UndoCreateAccountList::kNewAccounts);
      // we successfully ran CREATES USER [IF NOT EXISTS] on requested
      // accounts, and we have the (undo) list of which ones were new

      // build DROP USER statement to erase all existing accounts
      std::string query = "DROP USER IF EXISTS " + account_list;

      auto handle_error = [this, &account_list](const std::exception &e) {
        err_stream_ << "\n"
                    << Vt100::foreground(Vt100::Color::Red) << "ERROR: "
                    << Vt100::render(Vt100::Render::ForegroundDefault) <<
            R"(As part of cleanup after bootstrap failure, we tried to erase account(s)
that we created.  Unfortunately the cleanup failed with error:

  )"s << e.what() << R"(
You may want to clean up the accounts yourself, here is the full list of
accounts that were created:
  )"s << account_list
                    << std::endl;

        log_error("Undoing creating new users failed: %s", e.what());
      };

      // since we're running this code as result of prior errors, we can't
      // really do anything about new exceptions, except to advise user.
      try {
        harness_assert(session_);
        session_->execute(query);
        err_stream_ << "- New accounts cleaned up successfully" << std::endl;
      } catch (const MySQLSession::Error &e) {
        handle_error(e);
        break;
      } catch (const std::logic_error &e) {
        handle_error(e);
        break;
      }
    }
  } catch (...) {
  }
}
