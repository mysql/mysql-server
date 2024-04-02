/*
  Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#include "http/base/request.h"
#include "http/base/status_code.h"

namespace mrs {
namespace http {

class Error {
 public:
  using Status = ::http::base::status_code::key_type;

 public:
  template <typename... T>
  Error(const Status s, const std::string &m, const T &... t) : status{s} {
    message = (m + ... + t);
  }

  Error(const Status s)
      : status{s}, message{HttpStatusCode::get_default_status_text(s)} {}
  Status status;
  std::string message;
};

class ErrorChangeResponse {
 public:
  virtual ~ErrorChangeResponse() {}

  virtual const char *name() const = 0;
  virtual bool retry() const = 0;
  virtual Error change_response(::http::base::Request *request) const = 0;
};

class ErrorWithHttpHeaders : public ErrorChangeResponse {
 public:
  using Headers = std::vector<std::pair<std::string, std::string>>;

  ErrorWithHttpHeaders(HttpStatusCode::key_type status_code, Headers headers)
      : status_code_{status_code}, headers_{headers} {}

  const char *name() const override { return "ErrorWithHttpHeaders"; }
  bool retry() const override { return false; }
  Error change_response(::http::base::Request *request) const override {
    for (auto [k, v] : headers_) {
      request->get_output_headers().add(k.c_str(), v.c_str());
    }

    return Error(status_code_);
  }

 private:
  HttpStatusCode::key_type status_code_;
  Headers headers_;
};

class ErrorRedirect : public ErrorChangeResponse {
 public:
  ErrorRedirect(const std::string &redirect) : redirect_{redirect} {}

  const char *name() const override { return "ErrorRedirect"; }
  bool retry() const override { return false; }
  Error change_response(::http::base::Request *request) const override {
    request->get_output_headers().add("Location", redirect_.c_str());
    return Error(HttpStatusCode::TemporaryRedirect);
  }

 private:
  std::string redirect_;
};

}  // namespace http
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_REST_ERROR_H_
