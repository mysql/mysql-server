/*
 * Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#ifndef _XPL_ACCOUNT_VERIFICATION_HANDLER_H_
#define _XPL_ACCOUNT_VERIFICATION_HANDLER_H_

#include <map>

#include "ngs/error_code.h"
#include "ngs/interface/account_verification_interface.h"
#include "ngs/interface/authentication_interface.h"
#include "ngs/interface/session_interface.h"
#include "sql_user_require.h"

namespace xpl {

class Account_verification_handler {
 public:
  explicit Account_verification_handler(ngs::Session_interface *session)
      : m_session(session) {}
  Account_verification_handler(
      ngs::Session_interface *session,
      const ngs::Account_verification_interface::Account_type account_type,
      ngs::Account_verification_interface *verificator)
      : m_session(session) {
    add_account_verificator(account_type, verificator);
  }

  virtual ~Account_verification_handler() {}

  virtual ngs::Error_code authenticate(
      const ngs::Authentication_interface &account_verificator,
      const std::string &sasl_message) const;

  ngs::Error_code verify_account(const std::string &user,
                                 const std::string &host,
                                 const std::string &passwd) const;

  void add_account_verificator(
      const ngs::Account_verification_interface::Account_type account_type,
      ngs::Account_verification_interface *verificator) {
    m_verificators[account_type].reset(verificator);
  }

  virtual const ngs::Account_verification_interface *
      get_account_verificator(
          const ngs::Account_verification_interface::Account_type account_type)
      const;

 private:
  typedef std::map<ngs::Account_verification_interface::Account_type,
                   ngs::Account_verification_interface_ptr>
      Account_verificator_list;

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

  ngs::Account_verification_interface::Account_type
      get_account_verificator_id(const std::string &plugin_name) const;

  ngs::Error_code get_account_record(const std::string &user,
                                     const std::string &host,
                                     Account_record &record) const;

  ngs::PFS_string get_sql(const std::string &user,
                          const std::string &host) const;

  ngs::Session_interface *m_session;
  Account_verificator_list m_verificators;
};

typedef ngs::Memory_instrumented<Account_verification_handler>::Unique_ptr
    Account_verification_handler_ptr;

}  // namespace xpl

#endif  // _XPL_ACCOUNT_VERIFICATION_HANDLER_H_
