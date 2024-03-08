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

#ifndef ROUTER_SRC_HTTP_SRC_HTTP_CNO_CNO_INTERFACE_H_
#define ROUTER_SRC_HTTP_SRC_HTTP_CNO_CNO_INTERFACE_H_

#include "cno/core.h"

#include "mysqlrouter/http_common_export.h"

namespace http {
namespace cno {

class HTTP_COMMON_EXPORT CnoInterface {
 public:
  virtual ~CnoInterface();

  virtual int on_cno_message_head(const uint32_t session_id,
                                  const cno_message_t *message) = 0;
  virtual int on_cno_message_body(const uint32_t session_id, const char *data,
                                  const size_t size) = 0;
  virtual int on_cno_message_tail(const uint32_t session_id,
                                  const cno_tail_t *tail) = 0;

  virtual int on_cno_writev(const cno_buffer_t *buffer, size_t count) = 0;

  virtual int on_cno_stream_start(const uint32_t id) = 0;
  virtual int on_cno_stream_end(const uint32_t id) = 0;
  virtual int on_cno_close() = 0;
};

}  // namespace cno
}  // namespace http

#endif  // ROUTER_SRC_HTTP_SRC_HTTP_CNO_CNO_INTERFACE_H_
