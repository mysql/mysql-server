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

#include "http/cno/error_code.h"

#include <cassert>
#include <string>

namespace http {
namespace cno {
namespace {

class ErrorCategory : public std::error_category {
 public:
  const char *name() const noexcept override { return "client_failure"; }

  std::string message(const int code) const override {
    switch (code) {
      case CNO_ERRNO_ASSERTION:
        return "HTTP library assertion";
      case CNO_ERRNO_NO_MEMORY:
        return "HTTP can't allocate memory, to handle the data";
      case CNO_ERRNO_NOT_IMPLEMENTED:
        return "HTTP flow not implemented";
      case CNO_ERRNO_PROTOCOL:
        return "HTTP invalid protocol";
      case CNO_ERRNO_INVALID_STREAM:
        return "HTTP invalid stream";
      case CNO_ERRNO_WOULD_BLOCK:
        return "HTTP I/O operation would block";
      case CNO_ERRNO_DISCONNECT:
        return "HTTP stream disconnected";
    }

    return "unknown-" + std::to_string(code);
  }

  static const std::error_category &singleton() noexcept {
    static ErrorCategory instance;
    return instance;
  }
};

}  // namespace
}  // namespace cno
}  // namespace http

std::error_code make_error_code(const cno_error_t *ec) {
  return {ec->code, http::cno::ErrorCategory::singleton()};
}
