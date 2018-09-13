/*
 * Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#ifndef PLUGIN_X_NGS_INCLUDE_NGS_PROTOCOL_MESSAGE_H_
#define PLUGIN_X_NGS_INCLUDE_NGS_PROTOCOL_MESSAGE_H_

#include "plugin/x/ngs/include/ngs/memory.h"
#include "plugin/x/ngs/include/ngs/protocol/protocol_protobuf.h"

namespace ngs {

#ifdef USE_MYSQLX_FULL_PROTO
typedef ::google::protobuf::Message Message;
#else
typedef ::google::protobuf::MessageLite Message;
#endif

class Message_request {
 public:
  ~Message_request() { free_msg(); }

  void reset(Message *msg = nullptr, const uint8 msg_type = 0,
             const bool cant_be_deleted = true) {
    free_msg();

    m_message = msg;
    m_message_type = msg_type;
    m_is_owned = !cant_be_deleted;
  }

  Message *get_message() const { return m_message; }
  uint8 get_message_type() const { return m_message_type; }

  bool has_message() const { return nullptr != m_message; }

 private:
  void free_msg() {
    if (m_is_owned) {
      if (m_message) free_object(m_message);
      m_is_owned = false;
    }
  }

  Message *m_message = nullptr;
  uint8 m_message_type{0};
  bool m_is_owned{false};
};

}  // namespace ngs

#endif  // PLUGIN_X_NGS_INCLUDE_NGS_PROTOCOL_MESSAGE_H_
