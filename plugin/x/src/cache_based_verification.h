/*
 * Copyright (c) 2017, 2024, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
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

#ifndef PLUGIN_X_SRC_CACHE_BASED_VERIFICATION_H_
#define PLUGIN_X_SRC_CACHE_BASED_VERIFICATION_H_

#include <cstdint>
#include <string>

#include "plugin/x/src/challenge_response_verification.h"

namespace xpl {

/**
  Perform account verification based on information stored in the sha256
  password cache
*/
class Cache_based_verification : public Challenge_response_verification {
 public:
  explicit Cache_based_verification(iface::SHA256_password_cache *cache)
      : Challenge_response_verification(cache) {}
  bool verify_authentication_string(
      const std::string &user, const std::string &host,
      const std::string &client_string_hex,
      const std::string & /* unused */) const override;

 private:
  void hex2octet(uint8_t *to, const char *str, uint32_t len) const;
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_CACHE_BASED_VERIFICATION_H_
