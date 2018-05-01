/*
 * Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
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

#ifndef PLUGIN_X_NGS_INCLUDE_NGS_INTERFACE_AUTHENTICATION_INTERFACE_H_
#define PLUGIN_X_NGS_INCLUDE_NGS_INTERFACE_AUTHENTICATION_INTERFACE_H_

#include "plugin/x/ngs/include/ngs/error_code.h"
#include "plugin/x/ngs/include/ngs/interface/account_verification_interface.h"
#include "plugin/x/ngs/include/ngs/interface/sha256_password_cache_interface.h"
#include "plugin/x/ngs/include/ngs/memory.h"
#include "plugin/x/ngs/include/ngs_common/bind.h"

namespace ngs {
class Session_interface;
class Authentication_interface;

typedef ngs::Memory_instrumented<Authentication_interface>::Unique_ptr
    Authentication_interface_ptr;

class Authentication_info {
 public:
  std::string m_tried_account_name;
  bool m_was_using_password{false};

  void reset() {
    m_was_using_password = false;
    m_tried_account_name.clear();
  }

  bool is_valid() const { return !m_tried_account_name.empty(); }
};

class Authentication_interface {
 public:
  enum Status { Ongoing, Succeeded, Failed, Error };

  struct Response {
    Response(const Status status_ = Ongoing, const int error_ = 0,
             const std::string &data_ = "")
        : data(data_), status(status_), error_code(error_) {}
    std::string data;
    Status status;
    int error_code;
  };

  using Create = Authentication_interface_ptr (*)(
      Session_interface *, SHA256_password_cache_interface *);

  Authentication_interface() {}
  Authentication_interface(const Authentication_interface &) = delete;
  Authentication_interface &operator=(const Authentication_interface &) =
      delete;

  virtual ~Authentication_interface() {}

  virtual Response handle_start(const std::string &mechanism,
                                const std::string &data,
                                const std::string &initial_response) = 0;

  virtual Response handle_continue(const std::string &data) = 0;

  virtual ngs::Error_code authenticate_account(
      const std::string &user, const std::string &host,
      const std::string &passwd) const = 0;

  virtual Authentication_info get_authentication_info() const = 0;
};

}  // namespace ngs

#endif  // PLUGIN_X_NGS_INCLUDE_NGS_INTERFACE_AUTHENTICATION_INTERFACE_H_
