/*
  Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_HTTP_INCLUDE_HTTP_BASE_CONNECTION_INTERFACE_H_
#define ROUTER_SRC_HTTP_INCLUDE_HTTP_BASE_CONNECTION_INTERFACE_H_

#include <map>
#include <string>
#include <vector>

#include "http/base/headers.h"
#include "http/base/io_buffer.h"

#include "mysqlrouter/http_common_export.h"

namespace http {
namespace base {

class HTTP_COMMON_EXPORT ConnectionInterface {
 public:
  using IOBuffer = http::base::IOBuffer;
  using Headers = http::base::Headers;

 public:
  virtual ~ConnectionInterface();

  virtual bool send(const uint32_t *stream_id_ptr, const int status_code,
                    const std::string &method, const std::string &path,
                    const Headers &headers, const IOBuffer &data) = 0;

  virtual std::string get_peer_address() const = 0;
  virtual uint16_t get_peer_port() const = 0;

  virtual void start() = 0;
};

}  // namespace base
}  // namespace http

#endif  // ROUTER_SRC_HTTP_INCLUDE_HTTP_BASE_CONNECTION_INTERFACE_H_
