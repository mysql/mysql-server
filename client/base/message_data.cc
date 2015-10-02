/*
   Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "message_data.h"
#include "sql_string.h"
#include "m_ctype.h"

using namespace Mysql::Tools::Base;

Message_data::Message_data(uint64 code, std::string message,
  enum Message_type message_type)
    : m_code(code),
    m_message(message),
    m_message_type(message_type)
{}

uint64 Message_data::get_code() const
{
  return m_code;
}
std::string Message_data::get_message() const
{
  return m_message;
}
Message_type Message_data::get_message_type() const
{
  return m_message_type;
}
std::string Message_data::get_message_type_string() const
{
  return message_type_strings[m_message_type];
}

void Message_data::print_error(std::string program_name) const
{
  std::cerr << program_name << ": ["
    << get_message_type_string() << "] " << get_code()
    << ": " << get_message() << std::endl;
}

const char* Message_data::message_type_strings[]={
  "INFORMATION",
  "NOTE",
  "WARNING",
  "ERROR",
  "Unknown message type"
};

const int Message_data::message_type_strings_count=
  array_elements(Message_data::message_type_strings);


void Warning_data::print_error(std::string program_name) const
{
  std::cerr << program_name << ": (non fatal) ["
    << get_message_type_string() << "] " << get_code()
    << ": " << get_message() << std::endl;
}
