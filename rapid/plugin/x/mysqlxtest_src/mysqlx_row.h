/*
 * Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#ifndef _MYSQLX_ROW_H_
#define _MYSQLX_ROW_H_

#include <string>
#include <set>
#include <stdint.h>
#include "ngs_common/protocol_protobuf.h"

namespace mysqlx
{
  class DateTime;
  class Time;
  class Decimal;

  class Row_decoder
  {
  public:
    /* static methods to decode from protobuf format to specific types */
    static uint64_t u64_from_buffer(const std::string& buffer);
    static int64_t s64_from_buffer(const std::string& buffer);
    static const char *string_from_buffer(const std::string& buffer, size_t &rlength);
    static float float_from_buffer(const std::string& buffer);
    static double double_from_buffer(const std::string& buffer);
    static DateTime datetime_from_buffer(const std::string& buffer);
    static Time time_from_buffer(const std::string& buffer);
    static Decimal decimal_from_buffer(const std::string& buffer);
    static void set_from_buffer(const std::string& buffer, std::set<std::string>& result);
    static std::string set_from_buffer_as_str(const std::string& buffer);

  private:

    static void read_required_uint64(
      google::protobuf::io::CodedInputStream& input_buffer,
      google::protobuf::uint64& result
      );
  };
};

#endif
