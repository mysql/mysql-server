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

#ifndef ROUTER_SRC_HTTP_INCLUDE_HTTP_BASE_REQUEST_H_
#define ROUTER_SRC_HTTP_INCLUDE_HTTP_BASE_REQUEST_H_

#include <cassert>
#include <string>

#include "http/base/connection_interface.h"
#include "http/base/headers.h"
#include "http/base/io_buffer.h"
#include "http/base/method.h"
#include "http/base/status_code.h"
#include "http/base/uri.h"

#include "mysqlrouter/http_common_export.h"

namespace http {
namespace base {

class HTTP_COMMON_EXPORT Request {
 public:
  using IOBuffer = http::base::IOBuffer;
  using Headers = http::base::Headers;
  using Uri = http::base::Uri;
  using MethodType = http::base::method::key_type;
  using StatusType = http::base::status_code::key_type;
  using ConnectionInterface = http::base::ConnectionInterface;

 public:
  virtual ~Request() = default;

  // Informations that we received from other side
  // In case of:
  // * server, they are Request data
  // * client, they are Response data
  virtual const Headers &get_input_headers() const = 0;
  virtual IOBuffer &get_input_buffer() const = 0;
  virtual const std::string &get_input_body() const = 0;

  virtual Headers &get_output_headers() = 0;
  virtual IOBuffer &get_output_buffer() = 0;

  virtual StatusType get_response_code() const {
    assert(false &&
           "Unsupported in current derived class of `http::base::Request`");
    return status_code::NotImplemented;
  }
  //  std::string get_response_code_line() const = 0;

  virtual void set_method(MethodType) {
    assert(false &&
           "Unsupported in current derived class of `http::base::Request`");
  }

  virtual MethodType get_method() const {
    assert(false &&
           "Unsupported in current derived class of `http::base::Request`");

    return {};
  }

  virtual void set_uri([[maybe_unused]] Uri &&uri) {
    assert(false &&
           "Unsupported in current derived class of `http::base::Request`");
  }

  virtual void set_uri([[maybe_unused]] const Uri &uri) {
    assert(false &&
           "Unsupported in current derived class of `http::base::Request`");
  }

  virtual const Uri &get_uri() const = 0;

  virtual void send_reply([[maybe_unused]] StatusType status_code) {
    assert(false &&
           "Unsupported in current derived class of `http::base::Request`");
  }

  virtual void send_reply([[maybe_unused]] StatusType status_code,
                          [[maybe_unused]] const std::string &status_text) {
    assert(false &&
           "Unsupported in current derived class of `http::base::Request`");
  }

  virtual void send_reply([[maybe_unused]] StatusType status_code,
                          [[maybe_unused]] const std::string &status_text,
                          [[maybe_unused]] const IOBuffer &buffer) {
    assert(false &&
           "Unsupported in current derived class of `http::base::Request`");
  }

  virtual void send_error([[maybe_unused]] StatusType status_code) {
    assert(false &&
           "Unsupported in current derived class of `http::base::Request`");
  }

  virtual void send_error([[maybe_unused]] StatusType status_code,
                          [[maybe_unused]] const std::string &status_text) {
    assert(false &&
           "Unsupported in current derived class of `http::base::Request`");
  }

  /**
   * is request modified since 'last_modified'.
   *
   * @return true, if local content is newer than the clients last known date,
   * false otherwise
   */
  virtual bool is_modified_since([[maybe_unused]] time_t last_modified) {
    assert(false &&
           "Unsupported in current derived class of `http::base::Request`");
    return false;
  }

  /**
   * add a Last-Modified-Since header to the response headers.
   */
  virtual bool add_last_modified([[maybe_unused]] time_t last_modified) {
    assert(false &&
           "Unsupported in current derived class of `http::base::Request`");
    return false;
  }

  virtual ConnectionInterface *get_connection() const = 0;
};

}  // namespace base
}  // namespace http

#endif  // ROUTER_SRC_HTTP_INCLUDE_HTTP_BASE_REQUEST_H_
