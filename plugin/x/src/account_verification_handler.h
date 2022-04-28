/*
 * Copyright (c) 2017, 2022, Oracle and/or its affiliates.
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

#ifndef PLUGIN_X_SRC_ACCOUNT_VERIFICATION_HANDLER_H_
#define PLUGIN_X_SRC_ACCOUNT_VERIFICATION_HANDLER_H_

#include <map>
#include <memory>
#include <string>
#include "plugin/x/src/interface/account_verification.h"
#include "plugin/x/src/interface/account_verification_handler.h"
#include "plugin/x/src/interface/authentication.h"
#include "plugin/x/src/interface/session.h"
#include "plugin/x/src/ngs/error_code.h"
#include "plugin/x/src/sql_user_require.h"

namespace xpl {

class Account_verification_handler
    : public iface::Account_verification_handler {
 public:
  using Unique_ptr = std::unique_ptr<Account_verification_handler>;

  explicit Account_verification_handler(iface::Session *session)
      : m_session(session) {}
  Account_verification_handler(
      iface::Session *session,
      const iface::Account_verification::Account_type account_type,
      iface::Account_verification *verificator)
      : m_session(session), m_account_type(account_type) {
    add_account_verificator(account_type, verificator);
  }

  ngs::Error_code authenticate(const iface::Authentication &account_verificator,
                               iface::Authentication_info *authenication_info,
                               const std::string &sasl_message) const override;

  ngs::Error_code verify_account(
      const std::string &user, const std::string &host,
      const std::string &passwd,
      const iface::Authentication_info *authenication_info) const override;

  void add_account_verificator(
      const iface::Account_verification::Account_type account_type,
      iface::Account_verification *verificator) {
    m_verificators[account_type].reset(verificator);
  }

  const iface::Account_verification *get_account_verificator(
      const iface::Account_verification::Account_type account_type)
      const override;

 private:
  using Account_verificator_list =
      std::map<iface::Account_verification::Account_type,
               std::unique_ptr<iface::Account_verification>>;

  struct Account_record {
    bool require_secure_transport{true};
    std::string db_password_hash;
    std::string auth_plugin_name;
    bool is_account_locked{true};
    bool is_password_expired{true};
    bool disconnect_on_expired_password{true};
    bool is_offline_mode_and_not_super_user{true};
    Sql_user_require user_required;
  };

  bool extract_sub_message(const std::string &message,
                           std::size_t &element_position,
                           std::string &sub_message) const;

  bool extract_last_sub_message(const std::string &message,
                                std::size_t &element_position,
                                std::string &sub_message) const;

  iface::Account_verification::Account_type get_account_verificator_id(
      const std::string &plugin_name) const;

  ngs::Error_code get_account_record(const std::string &user,
                                     const std::string &host,
                                     Account_record &record) const;

  ngs::PFS_string get_sql(const std::string &user,
                          const std::string &host) const;

  iface::Session *m_session;
  Account_verificator_list m_verificators;
  iface::Account_verification::Account_type m_account_type =
      iface::Account_verification::Account_type::k_unsupported;
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_ACCOUNT_VERIFICATION_HANDLER_H_
