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

#include "http/cno/callback_init.h"

#include <stdio.h>

namespace http {
namespace cno {

CnoInterface *get_cno(void *cb_data) {
  return reinterpret_cast<CnoInterface *>(cb_data);
}

int on_writev(void *cb_data, const struct cno_buffer_t *buffer, size_t count) {
  return get_cno(cb_data)->on_cno_writev(buffer, count);
}

int on_close(void *cb_data) { return get_cno(cb_data)->on_cno_close(); }

int on_stream_start(void *cb_data, uint32_t id) {
  return get_cno(cb_data)->on_cno_stream_start(id);
}

int on_stream_end(void *cb_data, uint32_t id, uint32_t /*code*/,
                  enum CNO_PEER_KIND) {
  return get_cno(cb_data)->on_cno_stream_end(id);
}

int on_flow_increase(void * /*cb_data*/, uint32_t /*id*/) { return 0; }

int on_message_head(void *cb_data, uint32_t id,
                    const struct cno_message_t *msg) {
  //  cb_data,
  //         (int)id, (int)msg->method.size, msg->method.data,
  //         (int)msg->path.size, msg->path.data);
  return get_cno(cb_data)->on_cno_message_head(id, msg);
}

int on_message_push(void * /*cb_data*/, uint32_t /*id*/,
                    const struct cno_message_t *, uint32_t /*parent*/) {
  return 0;
}

int on_message_data(void *cb_data, uint32_t id, const char *data, size_t size) {
  return get_cno(cb_data)->on_cno_message_body(id, data, size);
}

int on_message_tail(void *cb_data, uint32_t id, const struct cno_tail_t *tail) {
  return get_cno(cb_data)->on_cno_message_tail(id, tail);
}

int on_frame(void * /*cb_data*/, const struct cno_frame_t *) { return 0; }

int on_frame_send(void * /*cb_data*/, const struct cno_frame_t *) { return 0; }

int on_pong(void * /*cb_data*/, const char[8]) { return 0; }

int on_settings(void *cb_data) { return get_cno(cb_data)->on_settings(); }

int on_upgrade(void * /*cb_data*/, uint32_t /*id*/) { return 0; }

cno_vtable_t g_cno_vtable{on_writev,       on_close,         on_stream_start,
                          on_stream_end,   on_flow_increase, on_message_head,
                          on_message_push, on_message_data,  on_message_tail,
                          on_frame,        on_frame_send,    on_pong,
                          on_settings,     on_upgrade};

void callback_init(cno_connection_t *cno, CnoInterface *icno) {
  cno->cb_data = icno;
  cno->cb_code = &g_cno_vtable;
}

}  // namespace cno
}  // namespace http
