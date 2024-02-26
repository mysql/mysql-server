/* Copyright (c) 2017, 2023, Oracle and/or its affiliates.

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
 GNU General Public License, version 2.0, for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef PLUGIN_X_CLIENT_MESSAGE_HOLDER_H_
#define PLUGIN_X_CLIENT_MESSAGE_HOLDER_H_

#include <algorithm>
#include <memory>
#include <vector>

#include "plugin/x/client/mysqlxclient/xprotocol.h"

namespace xcl {

const char *const ERR_MSG_UNEXPECTED = "Received unexpected message";

class Message_holder {
 public:
  using Message_id = XProtocol::Server_message_type_id;
  using Message = XProtocol::Message;

 public:
  explicit Message_holder(XProtocol *protocol) : m_protocol(protocol) {}

  XError read_new_message() {
    XError error;

    m_message = m_protocol->recv_single_message(&m_message_id, &error);

    return error;
  }

  XError read_or_use_cached_message() {
    if (has_cached_message()) return {};

    return read_new_message();
  }

  template <typename Message_callback_type>
  XError read_until_expected_msg_received(
      const std::vector<Message_id> &expected_msg_ids,
      const Message_callback_type &message_callback) {
    for (;;) {
      auto error = read_or_use_cached_message();

      if (error) return error;

      if (Mysqlx::ServerMessages::ERROR == m_message_id) {
        auto error_msg = reinterpret_cast<Mysqlx::Error *>(m_message.get());

        return XError{static_cast<int>(error_msg->code()), error_msg->msg(),
                      error_msg->severity() == Mysqlx::Error::FATAL,
                      error_msg->sql_state()};
      }

      if (std::any_of(
              expected_msg_ids.begin(), expected_msg_ids.end(),
              [this](const Message_id value) { return value == m_message_id; }))
        return {};

      error = message_callback(m_message_id, m_message);

      clear_cached_message();

      if (error) return error;
    }
  }

  XError read_until_expected_msg_received(
      const std::vector<Message_id> &expected_msg_ids,
      const std::vector<Message_id> &allowed_msg_ids) {
    for (;;) {
      auto error = read_or_use_cached_message();

      if (error) return error;

      if (Mysqlx::ServerMessages::ERROR == m_message_id) {
        auto error_msg = reinterpret_cast<Mysqlx::Error *>(m_message.get());

        return XError{static_cast<int>(error_msg->code()), error_msg->msg(),
                      error_msg->severity() == Mysqlx::Error::FATAL,
                      error_msg->sql_state()};
      }

      if (std::any_of(
              expected_msg_ids.begin(), expected_msg_ids.end(),
              [this](const Message_id value) { return value == m_message_id; }))
        return {};

      if (std::none_of(
              allowed_msg_ids.begin(), allowed_msg_ids.end(),
              [this](const Message_id value) { return value == m_message_id; }))
        return XError{CR_COMMANDS_OUT_OF_SYNC, ERR_MSG_UNEXPECTED};

      clear_cached_message();
    }
  }

  void clear_cached_message() { m_message.reset(); }

  bool has_cached_message() const { return nullptr != m_message.get(); }

  Message *get_cached_message() const { return m_message.get(); }

  bool is_one_of(const std::vector<Message_id> &message_ids) const {
    if (!has_cached_message()) return false;

    return std::any_of(message_ids.begin(), message_ids.end(),
                       [this](const Message_id allowed_id) -> bool {
                         return allowed_id == m_message_id;
                       });
  }

  Message_id get_cached_message_id() const { return m_message_id; }

  std::unique_ptr<Message> m_message;

 private:
  XProtocol *m_protocol;
  Message_id m_message_id;
};

}  // namespace xcl

#endif  // PLUGIN_X_CLIENT_MESSAGE_HOLDER_H_
