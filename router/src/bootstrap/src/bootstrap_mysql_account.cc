/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#include "bootstrap_mysql_account.h"

#include <iomanip>
#include <iostream>
#include <regex>
#include <sstream>
#include <stdexcept>

#include "harness_assert.h"
#include "mysqld_error.h"
#include "random_generator.h"
#include "sha1.h"  // compute_sha1_hash() from mysql's include/

static const int kMetadataServerPasswordLength = 16;

using MySQLSession = mysqlrouter::MySQLSession;
using namespace std::string_literals;

namespace {
struct password_too_weak : public std::runtime_error {
  using std::runtime_error::runtime_error;
};

struct plugin_not_loaded : public std::runtime_error {
  using std::runtime_error::runtime_error;
};
struct account_exists : public std::runtime_error {
  using std::runtime_error::runtime_error;
};

void throw_account_exists(MySQLSession *session, const MySQLSession::Error &e,
                          const std::string &username) {
  // clang-format off
  // Extract a list of accounts that are reported to already exist.
  //
  // We do this by parsing error message we got back from the Server.  In
  // English form, it looks like this:
  //
  //   ERROR 1396 (HY000): Operation CREATE USER failed for 'foo'@'host1','foo'@'host2'
  //
  // The message contains only the accounts that already exist, so it could
  // have been a result of:
  //
  //   CREATE USER 'foo'@'host1', 'foo'@'host2', 'foo'@'host3', 'foo'@'host4'
  //
  // if host3 and host4 did not exist yet.
  //
  // Note that on such failure, CREATE USER will not create host3 and host4.
  // clang-format on
  std::set<std::string> accounts;
  {
    std::string text = e.message();
    const std::regex re{session->quote(username) + "@'.*?'"};
    std::smatch m;

    while (std::regex_search(text, m, re)) {
      accounts.insert(m[0]);
      text = m.suffix().str();
    }
  }
  if (accounts.empty())
    throw std::runtime_error(
        "Failed to parse error message returned by CREATE USER command: "s +
        e.what());

  // Build error message informing of existing accounts
  std::string msg = "Account(s) ";

  bool is_first{true};
  for (const std::string &a : accounts) {
    if (is_first) {
      is_first = false;
    } else {
      msg += ",";
    }
    msg += a;
  }
  msg +=
      " already exist(s). If this is expected, please rerun without "
      "`--account-create always`.";

  throw account_exists(msg);
}

unsigned get_password_retries(const UserOptions &user_options) {
  return user_options.password_retries;
}

std::string make_account_list(MySQLSession *session, const std::string username,
                              const std::set<std::string> &hostnames) {
  std::string account_list;
  for (const std::string &h : hostnames) {
    if (!account_list.empty()) {
      account_list += ",";
    }
    account_list += session->quote(username) + "@" + session->quote(h);
  }
  return account_list;
}

}  // namespace

std::set<std::string> BootstrapMySQLAccount::get_hostnames_of_created_accounts(
    const std::string &username, const std::set<std::string> &hostnames,
    bool if_not_exists) {
  harness_assert(hostnames.size());

  // when running with IF NOT EXISTS, a warning will be produced for every
  // account that already exists.  We want to continue setup only for those
  // that don't.  Also, we need to save this list in case we need to revert
  // setup due to some errors later on.
  std::set<std::string> new_hostnames;  // if/else blocks will populate it
  if (if_not_exists && mysql_->warning_count() > 0) {
    // example response
    // clang-format off
    // +-------+------+---------------------------------------------+
    // | Level | Code | Message                                     |
    // +-------+------+---------------------------------------------+
    // | Note  | 3163 | Authorization ID 'bla'@'h1' already exists. |
    // | Note  | 3163 | Authorization ID 'bla'@'h3' already exists. |
    // +-------+------+---------------------------------------------+
    // clang-format on

    auto validator = [](unsigned num_fields, MYSQL_FIELD *fields) {
      if (num_fields != 3) {
        throw std::runtime_error(
            "SHOW WARNINGS: Unexpected number of fields in the resultset. "
            "Expected = 3, got = " +
            std::to_string(num_fields));
      }

      auto verify_column_name = [fields](unsigned idx,
                                         const std::string &expected) {
        if (fields[idx].name != expected)
          throw std::runtime_error(
              "SHOW WARNINGS: Unexpected column " + std::to_string(idx + 1) +
              " name '" + fields[idx].name + "', expected '" + expected + "'");
      };
      verify_column_name(0, "Level");
      verify_column_name(1, "Code");
      verify_column_name(2, "Message");
    };

    // start off with a full list, and we'll subtract existing hostnames from it
    new_hostnames = hostnames;

    const std::regex re{" '" + username + "'@'(.*?)' "};
    auto processor = [&](const MySQLSession::Row &row) -> bool {
      // we ignore warnings we're not expecting
      unsigned long code;
      try {
        size_t end_pos{};
        code = std::stoul(row[1], &end_pos);

        if (end_pos != strlen(row[1])) {
          throw std::invalid_argument(std::string(row[1]) +
                                      " is expected to be an positive integer");
        }
      } catch (const std::exception &e) {
        throw std::runtime_error(
            "SHOW WARNINGS: Failed to parse error code from error code column (column content = '"s +
            row[1] + "'): " + e.what());
      }
      if (code != ER_USER_ALREADY_EXISTS) {
        return true;  // true = give me another row
      }

      // extract the hostname from the warning message, and erase it from
      // new_hostnames
      const char *msg = row[2];
      std::cmatch m;
      if (std::regex_search(msg, m, re)) {
        if (!new_hostnames.erase(m[1].str())) {
          throw std::runtime_error("SHOW WARNINGS: Unexpected account name '" +
                                   username + "'@'" + m[1].str() +
                                   "' in message \""s + msg + "\"");
        }
      } else {
        throw std::runtime_error(
            "SHOW WARNINGS: Failed to extract account name ('" + username +
            "'@'<anything>') from message \""s + msg + "\"");
      }

      return true;  // true = give me another row
    };

    try {
      mysql_->query("SHOW WARNINGS", processor, validator);
    } catch (const MySQLSession::Error &e) {
      // log_error("%s: executing query: %s", e.what(), s.c_str());
      try {
        mysql_->execute("ROLLBACK");
      } catch (...) {
        // log_error("Could not rollback transaction explicitly.");
      }

      // it shouldn't have failed, let the upper layers try to handle it
      std::string err_msg = std::string(
                                "Error creating MySQL account for router (SHOW "
                                "WARNINGS stage): ") +
                            e.what();
      throw std::runtime_error(err_msg);
    }
  } else {
    // nothing special here - accounts for all hostnames were created
    // successfully, therefore all are new
    new_hostnames = hostnames;
  }

  return new_hostnames;
}

/**
 * create account to be used by Router.
 *
 * `<host>` part of `<user>@<host>` will be %, unless user specified otherwise
 * using --account-host switch. Multiple --account-host switches are allowed.
 */
std::string BootstrapMySQLAccount::create_router_accounts(
    const UserOptions &user_options, const std::set<std::string> &hostnames,
    const std::string &username, const std::string &password,
    bool password_change_ok) {
  /*
  Ideally, we create a single account for the specific host that the router is
  running on. But that has several problems in real world, including:
  - if you're configuring on localhost ref to metadata server, the router will
  think it's in localhost and thus it will need 2 accounts: user@localhost
  and user@public_ip... further, there could be more than 1 IP for the host,
  which (like lan IP, localhost, internet IP, VPN IP, IPv6 etc). We don't know
  which ones are needed, so either we need to automatically create all of
  those or have some very complicated and unreliable logic.
  - using hostname is not reliable, because not every place will have name
  resolution available
  - using IP (even if we can detect it correctly) will not work if IP is not
  static

  Summing up, '%' is the easy way to avoid these problems. But the decision
  ultimately belongs to the user.
  */

  bool if_not_exists;
  {
    const std::string ac = user_options.account_create;
    if (ac == "never")
      return password;
    else if (ac == "if-not-exists" || user_options.autogenerated)
      if_not_exists = true;
    else if (ac == "always")
      if_not_exists = false;
    else
      harness_assert_this_should_not_execute();
  }

  // NOTE ON EXCEPTIONS:
  // create_accounts*() functions throw many things (see their descriptions)
  // - we let the higher-level logic deal with them when that happens.

  if (hostnames.size()) {
    // NOTE: it may update the password
    return create_accounts_with_compliant_password(
        user_options, username, hostnames, password, password_change_ok,
        if_not_exists);
  }

  return password;
}

std::string BootstrapMySQLAccount::create_accounts_with_compliant_password(
    const UserOptions &user_options, const std::string &username,
    const std::set<std::string> &hostnames, const std::string &password,
    bool password_change_ok, const bool if_not_exists) {
  mysql_harness::RandomGenerator rg;

  auto retries =
      get_password_retries(user_options);  // throws std::invalid_argument

  // try to create an account using the password directly
  while (true) {
    const std::string password_candidate =
        password.empty() && password_change_ok
            ? rg.generate_strong_password(kMetadataServerPasswordLength)
            : password;

    try {
      // create_accounts() throws many things, see its description
      create_accounts(user_options, username, hostnames, password_candidate,
                      if_not_exists);
      return password_candidate;
    } catch (const password_too_weak &e) {
      if (--retries == 0          // retries used up
          || !password.empty()    // \_ retrying is pointless b/c the password
          || !password_change_ok  // /  will be the same every time
      ) {
        // If this failed issue an error suggesting the change to
        // validate_password rules
        std::stringstream err_msg;
        err_msg << "Error creating user account: " << e.what() << std::endl
                << " Try to decrease the validate_password rules and try the "
                   "operation again.";
        throw std::runtime_error(err_msg.str());
      }
      // generated password does not satisfy the current policy requirements.
      // we do our best to generate strong password but with the
      // validate_password plugin, the user can set very strong or unusual
      // requirements that we are not able to predict so we just retry several
      // times hoping to meet the requirements with the next generated
      // password.
      continue;
    }
  }

  harness_assert_this_should_not_execute();
}

/*
  Create MySQL account for this instance of the router in the target cluster.

  The account will have access to the cluster metadata and to the
  replication_group_members table of the performance_schema.
  Note that this assumes that the metadata schema is stored in the
  destinations cluster and that there is only one replicaset in it.
 */
void BootstrapMySQLAccount::create_accounts(
    const UserOptions &user_options, const std::string &username,
    const std::set<std::string> &hostnames, const std::string &password,
    bool if_not_exists /*=false*/) {
  harness_assert(hostnames.size());
  //  harness_assert(undo_create_account_list_.type ==
  //                 UndoCreateAccountList::kNotSet);

  // when this throws, it may trigger failover (depends on what exception it
  // throws)
  create_users(username, hostnames, password, if_not_exists);

  // Now that we created users, we can no longer fail-over on subsequent
  // errors, because that write operation may automatically get propagated to
  // other nodes.  If we were to fail-over to another node and start over from
  // scratch, our writes (CREATE USER in this case) would be in conflict with
  // the writes coming through database replication mechanism.
  // All subsequent failures bypass fail-over and trigger bootstrap exit for
  // this reason.

  // save the list of all accounts, so it can be used to clean up the accounts
  // we just created, in case something later fails.  Saving the list of JUST
  // NEW accounts would be better (and we do that later), but in the meantime if
  // determining new accounts fails, at least we'll have a list of all accounts
  // that went into CREATE USER [IF NOT EXISTS] statement
  undo_create_account_list_ = {UndoCreateAccountList::kAllAccounts,
                               make_account_list(mysql_, username, hostnames)};

  // determine which of the accounts we ran in CREATE USER... statement did not
  // exist before
  const std::set<std::string> new_hostnames =
      get_hostnames_of_created_accounts(username, hostnames, if_not_exists);
  const std::string new_accounts =
      new_hostnames.empty()
          ? ""
          : make_account_list(mysql_, username, new_hostnames);

  // if we made it here, we managed to get a list of JUST NEW accounts that got
  // created.  This is more useful than the previous list of ALL accounts, so
  // let's replace it with this new better list.
  undo_create_account_list_ = {UndoCreateAccountList::kNewAccounts,
                               new_accounts};

  // proceed to giving grants
  give_grants_to_users(user_options, new_accounts);
}

void BootstrapMySQLAccount::create_users(const std::string &username,
                                         const std::set<std::string> &hostnames,
                                         const std::string &password,
                                         bool if_not_exists /*=false*/) {
  harness_assert(hostnames.size());

  // build string containing account/auth list
  std::string accounts_with_auth;
  {
    const std::string auth_part =
        " IDENTIFIED WITH `caching_sha2_password` BY "s +
        mysql_->quote(password);

    const std::string quoted_username = mysql_->quote(username);
    bool is_first{true};
    for (const std::string &h : hostnames) {
      if (is_first) {
        is_first = false;
      } else {
        accounts_with_auth += ",";
      }
      accounts_with_auth +=
          quoted_username + "@" + mysql_->quote(h) + auth_part;
    }
  }

  try {
    mysql_->execute(
        "CREATE USER "s + (if_not_exists ? "IF NOT EXISTS " : "") +
        accounts_with_auth);  // throws MySQLSession::Error, std::logic_error
  } catch (const MySQLSession::Error &e) {
    // log_error("%s: executing query: %s", e.what(), s.c_str());
    try {
      mysql_->execute("ROLLBACK");
    } catch (...) {
      // log_error("Could not rollback transaction explicitly.");
    }
    std::string err_msg =
        std::string(
            "Error creating MySQL account for router (CREATE USER stage): ") +
        e.what();
    if (e.code() == ER_NOT_VALID_PASSWORD) {  // password does not satisfy the
                                              // current policy requirements
      throw password_too_weak(err_msg);
    }
    if (e.code() == ER_CANNOT_USER) {  // user already exists
      // this should only happen when running with --account-create always,
      // which sets if_not_exists to false harness_assert(!if_not_exists);

      throw_account_exists(mysql_, e, username);
    }

    // it shouldn't have failed, let the upper layers try to handle it
    throw MySQLSession::Error(err_msg, e.code());
  }
}

void BootstrapMySQLAccount::give_grants_to_users(
    const UserOptions &user_options, const std::string &new_accounts) {
  // give GRANTs to new accounts
  if (!new_accounts.empty()) {
    // run GRANT stantements
    std::vector<std::string> statements;

    for (auto &role : user_options.grant_role) {
      statements.push_back("GRANT "s + role + " TO " + new_accounts);
    }

    for (const auto &s : statements) {
      try {
        mysql_->execute(s);  // throws MySQLSession::Error, std::logic_error
      } catch (const MySQLSession::Error &e) {
        // log_error("%s: executing query: %s", e.what(), s.c_str());
        try {
          mysql_->execute("ROLLBACK");
        } catch (...) {
          // log_error("Could not rollback transaction explicitly.");
        }

        // we throw such that fail-over WILL NOT work.  Since CREATE USER
        // already succeeded, we can't simply go over to next node and start
        // over because the state of the next node is uncertain due to
        // replication syncing the effect of CREATE USER that already succeeded.
        std::string err_msg =
            std::string(
                "Error creating MySQL account for router (GRANTs stage): ") +
            e.what();
        throw std::runtime_error(err_msg);
      }
    }
  }
}
