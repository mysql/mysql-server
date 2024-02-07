/* Copyright (c) 2019, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef MYSQL_BINLOG_EVENT_CODECS_BASE_H
#define MYSQL_BINLOG_EVENT_CODECS_BASE_H

#include <utility>
#include "mysql/binlog/event/binary_log.h"

namespace mysql::binlog::event::codecs {

/**
  This is the abstract and base class for binary log codecs.

  It defines the codec API. Implementations of this class must
  be stateless.
 */
class Codec {
 public:
  /**
    Member function that shall decode the contents of the given buffer into a
    binary log event.

    @param from the buffer containing the encoded event.
    @param size the length of the data in the buffer.
    @param to the binary log event to populate.

    @return a pair containing the amount of bytes decoded from the buffer and a
            boolean stating whether there was an error or not. False if no
            error, true otherwise.
  */
  virtual std::pair<std::size_t, bool> decode(const unsigned char *from,
                                              std::size_t size,
                                              Binary_log_event &to) const = 0;

  /**
    Member function that shall encode the contents of the given binary log event
    into an in memory buffer.

    @param from the binary log event to encode.
    @param to the buffer where the encoded event should be saved into.
    @param size the size of the buffer.

    @return a pair containing the amount of bytes encoded and whether there was
            an error or not. False if no error, true otherwise.
  */
  virtual std::pair<std::size_t, bool> encode(const Binary_log_event &from,
                                              unsigned char *to,
                                              std::size_t size) const = 0;
  virtual ~Codec() = default;
};

}  // namespace mysql::binlog::event::codecs

namespace [[deprecated]] binary_log {
namespace [[deprecated]] codecs {
using namespace mysql::binlog::event::codecs;
}  // namespace codecs
}  // namespace binary_log

#endif  // MYSQL_BINLOG_EVENT_CODECS_BASE_H
