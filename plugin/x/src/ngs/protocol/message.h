/*
 * Copyright (c) 2015, 2024, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
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

#ifndef PLUGIN_X_SRC_NGS_PROTOCOL_MESSAGE_H_
#define PLUGIN_X_SRC_NGS_PROTOCOL_MESSAGE_H_

#include "plugin/x/src/ngs/memory.h"
#include "plugin/x/src/ngs/protocol/protocol_protobuf.h"

namespace ngs {

#ifdef USE_MYSQLX_FULL_PROTO
typedef ::google::protobuf::Message Message;
#else
typedef ::google::protobuf::MessageLite Message;
#endif

class Message_request {
 public:
  ~Message_request() { free_msg(); }

  void reset(const uint8_t message_type = 0, Message *message = nullptr,
             const bool must_be_deleted = false) {
    free_msg();

    m_message = message;
    m_message_type = message_type;
    m_must_be_deleted = must_be_deleted;
  }

  Message *get_message() const { return m_message; }
  uint8_t get_message_type() const { return m_message_type; }

  bool has_message() const { return nullptr != m_message; }

 private:
  void free_msg() {
    if (m_must_be_deleted) {
      if (m_message) free_object(m_message);
      m_must_be_deleted = false;
    }
  }

  Message *m_message = nullptr;
  uint8_t m_message_type{0};
  bool m_must_be_deleted{false};
};

}  // namespace ngs

#endif  // PLUGIN_X_SRC_NGS_PROTOCOL_MESSAGE_H_
