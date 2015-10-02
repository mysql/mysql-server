/*
   Copyright (c) 2014,2015 Oracle and/or its affiliates. All rights reserved.

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

#ifndef MESSAGE_DATA_INCLUDED
#define MESSAGE_DATA_INCLUDED

#include <iostream>
#include <string>
#include "my_global.h"

namespace Mysql{
namespace Tools{
namespace Base{

enum Message_type
{
  Message_type_info,
  Message_type_note,
  Message_type_warning,
  Message_type_error,
  Message_type_unknown
};

/**
  Structure to represent message from server sent after executing query.
 */
class Message_data
{
public:
  Message_data(uint64 code, std::string message, Message_type message_type);
  virtual ~Message_data()
  { }
  uint64 get_code() const;
  std::string get_message() const;
  Message_type get_message_type() const;
  std::string get_message_type_string() const;

  static const char* message_type_strings[];
  static const int message_type_strings_count;
  /**
    Prints errors, warnings and notes to standard error.
  */
  virtual void print_error(std::string program_name) const;

private:
  uint64 m_code;
  std::string m_message;
  Message_type m_message_type;
};


class Warning_data: public Message_data
{
public:
  Warning_data(uint64 code, std::string message, Message_type message_type):
    Message_data(code, message, message_type)
  { }
  void print_error(std::string program_name) const;
};


}
}
}

#endif
