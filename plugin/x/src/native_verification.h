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

#include <string>

#include "plugin/x/src/challenge_response_verification.h"

namespace xpl {

class Native_verification : public Challenge_response_verification {
 public:
  explicit Native_verification(ngs::SHA256_password_cache_interface *cache)
    : Challenge_response_verification(cache) {}
  bool verify_authentication_string(const std::string &user,
      const std::string &host, const std::string &client_string,
      const std::string &db_string) const override;
};

}  // namespace xpl

#endif  // _XPL_NATIVE_PLAIN_VERIFICATION_H_
