/*
 * Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
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

#include "plugin/x/tests/driver/processor/send_message_block_processor.h"

#include "my_config.h"

#include <iostream>
#include <sstream>
#include <utility>

#include "plugin/x/tests/driver/common/utils_string_parsing.h"
#include "plugin/x/tests/driver/connector/mysqlx_all_msgs.h"

namespace {

static std::string data_to_bindump(const std::string &bindump) {
  std::string res;

  for (size_t i = 0; i < bindump.length(); i++) {
    unsigned char ch = bindump[i];

    if (i >= 5 && ch == '\\') {
      res.push_back('\\');
      res.push_back('\\');
    } else if (i >= 5 && isprint(ch) && !isblank(ch)) {
      res.push_back(ch);
    } else {
      res.append("\\x");
      res.push_back(aux::ALLOWED_HEX_CHARACTERS[(ch >> 4) & 0xf]);
      res.push_back(aux::ALLOWED_HEX_CHARACTERS[ch & 0xf]);
    }
  }

  return res;
}

class Error_dumper : public ::google::protobuf::io::ErrorCollector {
  std::stringstream m_out;

 public:
  void AddError(int line, int column, const std::string &message) override {
    m_out << "ERROR in message: line " << line + 1 << ": column " << column
          << ": " << message << "\n";
  }

  void AddWarning(int line, int column, const std::string &message) override {
    m_out << "WARNING in message: line " << line + 1 << ": column " << column
          << ": " << message << "\n";
  }

  std::string str() { return m_out.str(); }
};

}  // namespace

Block_processor::Result Send_message_block_processor::feed(
    std::istream &input, const char *linebuf) {
  if (m_full_name.empty()) {
    if (!(m_full_name = get_message_name(linebuf)).empty()) {
      m_buffer.clear();
      return Result::Feed_more;
    }
  } else {
    if (linebuf[0] == '}') {
      xcl::XProtocol::Client_message_type_id msg_id;
      std::string processed_buffer = m_buffer;

      m_context->m_variables->replace(&processed_buffer);

      Message_ptr msg{
          text_to_client_message(m_full_name, processed_buffer, &msg_id)};

      m_full_name.clear();
      if (!msg.get()) return Result::Indigestion;

      int process_result = process(msg_id, *msg);

      return (process_result != 0) ? Result::Indigestion
                                   : Result::Eaten_but_not_hungry;
    } else {
      m_buffer.append(linebuf).append("\n");
      return Result::Feed_more;
    }
  }

  return Result::Not_hungry;
}

bool Send_message_block_processor::feed_ended_is_state_ok() {
  if (m_full_name.empty()) return true;

  m_context->print_error(m_context->m_script_stack, "Incomplete message ",
                         m_full_name, '\n');
  return false;
}

std::string Send_message_block_processor::get_message_name(
    const char *linebuf) {
  const char *p;
  if ((p = strstr(linebuf, " {"))) {
    return std::string(linebuf, p - linebuf);
  }

  return "";
}

int Send_message_block_processor::process(
    const xcl::XProtocol::Client_message_type_id msg_id,
    const xcl::XProtocol::Message &message) {
  return process_client_message(m_context->session(), msg_id, message);
}

int Send_message_block_processor::process_client_message(
    xcl::XSession *session, const xcl::XProtocol::Client_message_type_id msg_id,
    const xcl::XProtocol::Message &msg) {
  if (!m_context->m_options.m_quiet) m_context->print("send ", msg, "\n");

  if (m_context->m_options.m_bindump)
    m_context->print(message_to_bindump(msg), "\n");

  try {
    // send request
    session->get_protocol().send(msg_id, msg);

    if (!m_context->m_expected_error.check_ok()) return 1;
  } catch (xcl::XError &err) {
    if (!m_context->m_expected_error.check_error(err)) return 1;
  }
  return 0;
}

std::string Send_message_block_processor::message_to_bindump(
    const xcl::XProtocol::Message &message) {
  std::string res;
  std::string out;

  message.SerializeToString(&out);

  res.resize(5);
  *reinterpret_cast<uint32_t *>(&res[0]) =
      static_cast<uint32_t>(out.size() + 1);

#ifdef WORDS_BIGENDIAN
  std::swap(res[0], res[3]);
  std::swap(res[1], res[2]);
#endif

  res[4] = client_msgs_by_name
               [client_msgs_by_full_name[message.GetDescriptor()->full_name()]]
                   .second;
  res.append(out);

  return data_to_bindump(res);
}

xcl::XProtocol::Message *Send_message_block_processor::text_to_client_message(
    const std::string &name, const std::string &data,
    xcl::XProtocol::Client_message_type_id *msg_id) {
  if (client_msgs_by_full_name.find(name) == client_msgs_by_full_name.end()) {
    m_context->print_error(m_context->m_script_stack, "Invalid message type ",
                           name, '\n');
    return nullptr;
  }

  auto msg = client_msgs_by_name.find(client_msgs_by_full_name[name]);

  if (msg == client_msgs_by_name.end()) {
    m_context->print_error(m_context->m_script_stack, "Invalid message type ",
                           name, '\n');
    return nullptr;
  }

  xcl::XProtocol::Message *message = msg->second.first();
  *msg_id = msg->second.second;

  google::protobuf::TextFormat::Parser parser;
  Error_dumper dumper;
  parser.RecordErrorsTo(&dumper);

  if (!parser.ParseFromString(data, message)) {
    m_context->print_error(m_context->m_script_stack,
                           "Invalid message in input: ", name, '\n');
    int i = 1;
    for (std::string::size_type p = 0, n = data.find('\n', p + 1);
         p != std::string::npos; p = (n == std::string::npos ? n : n + 1),
                                n = data.find('\n', p + 1), ++i) {
      m_context->print_error(i, ": ", data.substr(p, n - p), '\n');
    }

    m_context->print_error("\n", dumper.str(), '\n');
    delete message;

    return nullptr;
  }

  return message;
}
