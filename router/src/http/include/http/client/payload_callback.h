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

#ifndef ROUTER_SRC_HTTP_INCLUDE_HTTP_CLIENT_PAYLOAD_CALLBACK_H_
#define ROUTER_SRC_HTTP_INCLUDE_HTTP_CLIENT_PAYLOAD_CALLBACK_H_

#include <string>

#include "mysqlrouter/http_client_export.h"

namespace http {
namespace client {

class HTTP_CLIENT_EXPORT PayloadCallback {
 public:
  virtual ~PayloadCallback();

  virtual void on_connection_ready() = 0;
  virtual void on_input_payload(const char *data, size_t size) = 0;
  virtual void on_input_begin(int status_code,
                              const std::string &status_text) = 0;
  virtual void on_input_end() = 0;
  virtual void on_input_header(std::string &&key, std::string &&value) = 0;
  virtual void on_output_end_payload() = 0;
};

}  // namespace client

}  // namespace http

#endif  // ROUTER_SRC_HTTP_INCLUDE_HTTP_CLIENT_PAYLOAD_CALLBACK_H_
