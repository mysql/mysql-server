/*
  Copyright (c) 2018, 2024, Oracle and/or its affiliates.

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

#include "mcf_error.h"

#include <string>
#include <system_error>  // error_code

const std::error_category &mcf_category() noexcept {
  class socket_category_impl : public std::error_category {
   public:
    const char *name() const noexcept override { return "mcf"; }
    std::string message(int ev) const override {
      switch (static_cast<McfErrc>(ev)) {
        case McfErrc::kParseError:
          return "parse error";
        case McfErrc::kUnknownScheme:
          return "mcf scheme is not known";
        case McfErrc::kUserNotFound:
          return "user not found";
        case McfErrc::kPasswordNotMatched:
          return "password does not match";
        default:
          return "(unrecognized error)";
      }
    }
  };

  static socket_category_impl instance;
  return instance;
}

std::error_code make_error_code(McfErrc e) {
  return {static_cast<int>(e), mcf_category()};
}
