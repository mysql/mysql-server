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

#ifndef _XPL_NATIVE_VERIFICATION_H_
#define _XPL_NATIVE_VERIFICATION_H_

#include "ngs/interface/account_verification_interface.h"

namespace xpl {

class Native_verification : public ngs::Account_verification_interface {
 public:
  Native_verification() : k_salt(generate_salt()) {}
  const std::string &get_salt() const override { return k_salt; }
  bool verify_authentication_string(const std::string &client_string,
                                    const std::string &db_string) const
      override;

 private:
  std::string generate_salt();
  const std::string k_salt;
};

}  // namespace xpl

#endif  // _XPL_NATIVE_PLAIN_VERIFICATION_H_
