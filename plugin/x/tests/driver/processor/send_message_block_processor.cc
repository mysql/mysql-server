/*
 * Copyright (c) 2017, 2024, Oracle and/or its affiliates.
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

#include "plugin/x/tests/driver/processor/send_message_block_processor.h"

#include <iostream>
#include <sstream>
#include <utility>

#include "my_config.h"
#include "my_dbug.h"

#include "plugin/x/tests/driver/common/utils_string_parsing.h"
#include "plugin/x/tests/driver/connector/mysqlx_all_msgs.h"
#include "plugin/x/tests/driver/parsers/message_parser.h"

namespace {

std::string data_to_bindump(const std::string &bindump) {
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

}  // namespace

Block_processor::Result Send_message_block_processor::feed(
    std::istream &input, const char *linebuf) {
  if (!is_eating()) {
    if (parser::get_name_and_body_from_text(linebuf, &m_full_name, &m_buffer)) {
      if (!m_buffer.empty() && '{' == m_buffer[0])
        m_buffer.erase(m_buffer.begin());

      return Result::Feed_more;
    }

    return Result::Not_hungry;
  }

  if (linebuf[0] == '}') {
    xcl::XProtocol::Client_message_type_id msg_id =
        static_cast<xcl::XProtocol::Client_message_type_id>(
            Mysqlx::ClientMessages::Type_MIN);
    std::string processed_buffer = m_buffer;

    m_context->m_variables->replace(&m_full_name);
    m_context->m_variables->replace(&processed_buffer);

    std::string out_error;
    Message_ptr msg{parser::get_client_message_from_text(
        m_full_name, processed_buffer, &msg_id, &out_error)};

    m_full_name.clear();
    if (!msg.get()) {
      m_context->print_error(m_context->m_script_stack, out_error);
      return Result::Indigestion;
    }

    int process_result = process(msg_id, *msg);

    return (process_result != 0) ? Result::Indigestion
                                 : Result::Eaten_but_not_hungry;
  }

  m_buffer.append(linebuf).append("\n");
  return Result::Feed_more;
}

bool Send_message_block_processor::feed_ended_is_state_ok() {
  if (!is_eating()) return true;

  m_context->print_error(m_context->m_script_stack, "Incomplete message ",
                         m_full_name, '\n');
  return false;
}

bool Send_message_block_processor::is_eating() const {
  return !m_full_name.empty();
}

int Send_message_block_processor::process(
    const xcl::XProtocol::Client_message_type_id msg_id,
    const xcl::XProtocol::Message &message) {
  DBUG_TRACE;
  return process_client_message(m_context->session(), msg_id, message);
}

int Send_message_block_processor::process_client_message(
    xcl::XSession *session, const xcl::XProtocol::Client_message_type_id msg_id,
    const xcl::XProtocol::Message &msg) {
  if (!m_context->m_options.m_quiet) m_context->print("send ", msg, "\n");

  if (m_context->m_options.m_bindump)
    m_context->print(message_to_bindump(msg), "\n");

  const auto error = session->get_protocol().send(msg_id, msg);

  if (error) {
    if (!m_context->m_expected_error.check_error(error)) return 1;

    return 0;
  }

  if (!m_context->m_expected_error.check_ok()) return 1;

  return 0;
}

std::string Send_message_block_processor::message_serialize(
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

  return res;
}

std::string Send_message_block_processor::message_to_bindump(
    const xcl::XProtocol::Message &message) {
  return data_to_bindump(message_serialize(message));
}
