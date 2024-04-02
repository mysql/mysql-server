/*
  Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_AUTHENTICATION_HELPER_OPTION_PARSER_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_AUTHENTICATION_HELPER_OPTION_PARSER_H_

#include "mrs/authentication/helper/key_stored_informations.h"

#include <string>
#include <vector>

namespace mrs {
namespace authentication {

class UserOptionsParser {
 public:
  using Result = KeyStoredInformations;

  UserOptionsParser(const std::string &auth_string);

  Result ksi;

  bool is_valid() const;
  Result decode();

 private:
  template <typename T>
  static std::string as_string(const std::vector<T> &v) {
    return std::string(v.begin(), v.end());
  }

  void parse();

  std::string auth_string_;
  bool is_hmac_sha256{false};
  bool has_iterations_{false};
};

}  // namespace authentication
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_AUTHENTICATION_HELPER_OPTION_PARSER_H_
