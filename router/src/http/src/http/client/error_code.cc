/*
  Copyright (c) 2024, Oracle and/or its affiliates.

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

#include "http/client/error_code.h"

#include <cassert>
#include <string>

namespace http {
namespace client {
namespace {

class ErrorCategory : public std::error_category {
 public:
  const char *name() const noexcept override { return "client_failure"; }

  std::string message(const int fc) const override {
    auto enum_fc = static_cast<FailureCode>(fc);
    switch (enum_fc) {
      case FailureCode::kInvalidScheme:
        return "Unknown scheme in URL";
      case FailureCode::kInvalidUrl:
        return "Invalid URL";
      case FailureCode::kInvalidHostname:
        return "Invalid or empty host in URL";
      case FailureCode::kResolveFailure:
        return "Can't resolve host";
      case FailureCode::kResolveHostNotFound:
        return "Host not found";
      case FailureCode::kConnectionFailure:
        return "Can't connect to remote host";
      case FailureCode::kUnknowHttpMethod:
        return "Unknown HTTP method";
    }

    return "unknown-" + std::to_string(fc);
  }

  static const std::error_category &singleton() noexcept {
    static ErrorCategory instance;
    return instance;
  }
};

}  // namespace
}  // namespace client
}  // namespace http

std::error_code make_error_code(const http::client::FailureCode ec) {
  return {static_cast<int>(ec), http::client::ErrorCategory::singleton()};
}
