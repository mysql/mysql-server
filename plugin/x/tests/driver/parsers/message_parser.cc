/*
 * Copyright (c) 2018, 2024, Oracle and/or its affiliates.
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

#include "plugin/x/tests/driver/parsers/message_parser.h"

#include <memory>

#include <google/protobuf/stubs/common.h>

#include "plugin/x/tests/driver/common/utils_string_parsing.h"
#include "plugin/x/tests/driver/connector/mysqlx_all_msgs.h"

using Message = xcl::XProtocol::Message;

namespace parser {

namespace details {

class Error_dumper : public ::google::protobuf::io::ErrorCollector {
  std::stringstream m_out;

 public:
#if (GOOGLE_PROTOBUF_VERSION >= 4024000)
  void RecordError(int line, int column, absl::string_view message) override {
    m_out << "ERROR in message: line " << line + 1 << ": column " << column
          << ": " << message << "\n";
  }

  void RecordWarning(int line, int column, absl::string_view message) override {
    m_out << "WARNING in message: line " << line + 1 << ": column " << column
          << ": " << message << "\n";
  }
#else
  void AddError(int line, int column, const std::string &message) override {
    m_out << "ERROR in message: line " << line + 1 << ": column " << column
          << ": " << message << "\n";
  }

  void AddWarning(int line, int column, const std::string &message) override {
    m_out << "WARNING in message: line " << line + 1 << ": column " << column
          << ": " << message << "\n";
  }
#endif  // (GOOGLE_PROTOBUF_VERSION >= 4024000)

  std::string str() { return m_out.str(); }
};

bool parse_mesage(const std::string &text_message, const std::string &text_name,
                  Message *message, std::string *out_error,
                  const bool allow_partial_messaged) {
  google::protobuf::TextFormat::Parser parser;
  Error_dumper dumper;
  parser.RecordErrorsTo(&dumper);
  parser.AllowPartialMessage(allow_partial_messaged);
  if (!parser.ParseFromString(text_message, message)) {
    if (nullptr != out_error) {
      *out_error = "Invalid message in input: " + text_name + '\n';
      int i = 1;
      for (std::string::size_type p = 0, n = text_message.find('\n', p + 1);
           p != std::string::npos; p = (n == std::string::npos ? n : n + 1),
                                  n = text_message.find('\n', p + 1), ++i) {
        *out_error +=
            std::to_string(i) + ": " + text_message.substr(p, n - p) + '\n';
      }
      *out_error += "\n" + dumper.str() + '\n';
    }

    return false;
  }

  return true;
}

template <typename MSG>
Message *parse_serialize_message(const std::string &text_payload,
                                 std::string *out_error,
                                 const bool allow_partial_messaged) {
  std::unique_ptr<MSG> msg{new MSG()};

  if (!parse_mesage(text_payload, "", msg.get(), out_error,
                    allow_partial_messaged))
    return {};

  return msg.release();
}

bool get_notice_payload_from_text(const Mysqlx::Notice::Frame_Type type,
                                  const std::string &text_payload,
                                  std::string *out_binary_payload,
                                  const bool allow_partial_messaged) {
  std::string error;
  std::unique_ptr<Message> msg{parser::get_notice_message_from_text(
      type, text_payload, &error, allow_partial_messaged)};

  if (nullptr == msg) {
    // Fail when there is a payload, still we received a null message
    return text_payload.empty();
  }

  if (allow_partial_messaged)
    return msg->SerializePartialToString(out_binary_payload);

  return msg->SerializeToString(out_binary_payload);
}

}  // namespace details

Message *get_notice_message_from_text(const Mysqlx::Notice::Frame_Type type,
                                      const std::string &text_payload,
                                      std::string *out_error,
                                      const bool allow_partial_messaged) {
  switch (type) {
    case Mysqlx::Notice::Frame_Type_WARNING:
      return details::parse_serialize_message<Mysqlx::Notice::Warning>(
          text_payload, out_error, allow_partial_messaged);
    case Mysqlx::Notice::Frame_Type_SESSION_VARIABLE_CHANGED:
      return details::parse_serialize_message<
          Mysqlx::Notice::SessionVariableChanged>(text_payload, out_error,
                                                  allow_partial_messaged);
    case Mysqlx::Notice::Frame_Type_SESSION_STATE_CHANGED:
      return details::parse_serialize_message<
          Mysqlx::Notice::SessionStateChanged>(text_payload, out_error,
                                               allow_partial_messaged);
    case Mysqlx::Notice::Frame_Type_GROUP_REPLICATION_STATE_CHANGED:
      return details::parse_serialize_message<
          Mysqlx::Notice::GroupReplicationStateChanged>(text_payload, out_error,
                                                        allow_partial_messaged);
    default:
      return nullptr;
  }
}

bool get_name_and_body_from_text(const std::string &text_message,
                                 std::string *out_full_message_name,
                                 std::string *out_message_body,
                                 const bool is_body_full) {
  const auto separator = text_message.find("{");

  if (std::string::npos == separator) {
    return false;
  }

  if (nullptr != out_full_message_name) {
    *out_full_message_name = text_message.substr(0, separator);
    aux::trim(*out_full_message_name);
  }

  auto body = text_message.substr(separator);

  if (is_body_full) {
    aux::trim(body, " \t\n\r");

    if (body.size() < 2) return false;

    if (body[0] != '{') return false;
    if (body[body.size() - 1] != '}') return false;

    body = body.substr(1, body.size() - 2);
  }

  if (nullptr != out_message_body) {
    *out_message_body = body;
  }

  return true;
}

Message *get_client_message_from_text(
    const std::string &name, const std::string &data,
    xcl::XProtocol::Client_message_type_id *msg_id, std::string *out_error,
    const bool allow_partial_messaged) {
  std::string find_by = name;
  Message *message;

  if (find_by.empty()) {
    *out_error = "Message name is empty";
    return nullptr;
  }

  while (true) {
    auto msg = client_msgs_by_name.find(find_by);

    if (msg == client_msgs_by_name.end()) {
      if (client_msgs_by_full_name.count(name) &&
          find_by != client_msgs_by_full_name[name]) {
        find_by = client_msgs_by_full_name[name];
        continue;
      }
      *out_error = "Invalid message type " + name;
      return nullptr;
    }

    message = msg->second.first();
    *msg_id = msg->second.second;
    break;
  }

  if (!details::parse_mesage(data, name, message, out_error,
                             allow_partial_messaged)) {
    delete message;
    return nullptr;
  }

  return message;
}

Message *get_server_message_from_text(
    const std::string &name, const std::string &data,
    xcl::XProtocol::Server_message_type_id *msg_id, std::string *out_error,
    const bool allow_partial_messaged) {
  std::string find_by = name;
  Message *message;

  while (true) {
    auto msg = server_msgs_by_name.find(find_by);

    if (msg == server_msgs_by_name.end()) {
      if (server_msgs_by_full_name.count(name) &&
          find_by != server_msgs_by_full_name[name]) {
        find_by = server_msgs_by_full_name[name];
        continue;
      }
      *out_error = "Invalid message type " + name;
      return nullptr;
    }

    message = msg->second.first();
    *msg_id = msg->second.second;
    break;
  }

  if (!details::parse_mesage(data, name, message, out_error,
                             allow_partial_messaged)) {
    delete message;
    return nullptr;
  }

  if (Mysqlx::ServerMessages::NOTICE == *msg_id) {
    auto notice = reinterpret_cast<Mysqlx::Notice::Frame *>(message);

    std::string out_payload;
    if (!details::get_notice_payload_from_text(
            static_cast<Mysqlx::Notice::Frame_Type>(notice->type()),
            notice->payload(), &out_payload, allow_partial_messaged)) {
      *out_error = "Invalid notice payload: " + notice->payload();
      return nullptr;
    }

    notice->set_payload(out_payload);
  }

  return message;
}

}  // namespace parser
