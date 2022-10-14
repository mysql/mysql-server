/*
  Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_REST_ERROR_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_REST_ERROR_H_

#include <string>

#include "mysqlrouter/http_request.h"

namespace mrs {
namespace http {

class Error {
 public:
  using Status = HttpStatusCode::key_type;

 public:
  Error(const Status s, const std::string &m) : status{s}, message{m} {}

  Error(const Status s)
      : status{s}, message{HttpStatusCode::get_default_status_text(s)} {}
  Status status;
  std::string message;
};

class ErrorChangeResponse {
 public:
  virtual ~ErrorChangeResponse() {}

  virtual Error change_response(HttpRequest *request) const = 0;
};

class ErrorRedirect : public ErrorChangeResponse {
 public:
  ErrorRedirect(const std::string &redirect) : redirect_{redirect} {}

  Error change_response(HttpRequest *request) const override {
    request->get_output_headers().add("Location", redirect_.c_str());
    return Error(HttpStatusCode::TemporaryRedirect);
  }

 private:
  std::string redirect_;
};

}  // namespace http
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_REST_ERROR_H_
