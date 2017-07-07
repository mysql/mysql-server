/*
 * Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "console.h"

#include <algorithm>
#include <iostream>
#include <set>


namespace  {

template <typename Message_type>
std::string message_to_text(const std::string &binary_message) {
  std::string   result;
  Message_type  msg;

  msg.ParseFromString(binary_message);
  google::protobuf::TextFormat::PrintToString(msg, &result);

  return msg.GetDescriptor()->full_name() +
      " { " +
      result +
      " }";
}

}  // namespace

const Console::Color Console::k_red("\033[1;31m");
const Console::Color Console::k_clear("\033[0m");

Console::Console(const Options& options)
    : m_options(options),
      m_out(&std::cout),
      m_err(&std::cerr) {}

Console::Console(const Options& options, std::ostream* out, std::ostream* err)
    : m_options(options),
      m_out(out),
      m_err(err) {}


std::ostream &operator<<(std::ostream &os, const xcl::XError &err) {
  return os << err.what() << " (code " << err.error() << ")";
}

std::ostream &operator<<(std::ostream &os, const std::exception &exc) {
  return os << exc.what();
}

std::ostream& operator<<(std::ostream& os,
                         const xcl::XProtocol::Message& message) {
  std::string output;
  std::string name;
  google::protobuf::TextFormat::Printer printer;

  printer.SetInitialIndentLevel(1);

  // special handling for nested messages (at least for Notices)
  if (message.GetDescriptor()->full_name() == "Mysqlx.Notice.Frame") {
    Mysqlx::Notice::Frame frame =
        *static_cast<const Mysqlx::Notice::Frame*>(&message);

    switch (frame.type()) {
      case ::Mysqlx::Notice::Frame_Type_WARNING: {
        const auto payload_as_text = message_to_text<
            Mysqlx::Notice::Warning>(frame.payload());

        frame.set_payload(payload_as_text);
        break;
      }
      case ::Mysqlx::Notice::Frame_Type_SESSION_VARIABLE_CHANGED: {
        const auto payload_as_text = message_to_text<
            Mysqlx::Notice::SessionVariableChanged>(frame.payload());

        frame.set_payload(payload_as_text);
        break;
      }
      case ::Mysqlx::Notice::Frame_Type_SESSION_STATE_CHANGED: {
        const auto payload_as_text = message_to_text<
            Mysqlx::Notice::SessionStateChanged>(frame.payload());

        frame.set_payload(payload_as_text);
        break;
      }
    }

    printer.PrintToString(frame, &output);
  } else {
    printer.PrintToString(message, &output);
  }

  return os << message.GetDescriptor()->full_name() <<
      " {\n" <<
      output <<
      "}\n";
}

std::ostream &operator<<(std::ostream &os, const std::set<int> &value) {
  std::copy(value.begin(),
            value.end(),
            std::ostream_iterator<int>(os, " "));
  return os;
}
