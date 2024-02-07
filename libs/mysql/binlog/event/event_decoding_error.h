/* Copyright (c) 2014, 2024, Oracle and/or its affiliates.

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

#ifndef MYSQL_BINLOG_EVENT_EVENT_DECODING_ERROR_H
#define MYSQL_BINLOG_EVENT_EVENT_DECODING_ERROR_H

/// @addtogroup GroupLibsMysqlBinlogEvent
/// @{

namespace mysql::binlog::event {

/// @brief Errors that we distinguish during event decoding, that are
/// translated to specific error returned by the Server
enum class Event_decoding_error {
  /// @brief no error
  ok,
  /// @brief Unknown, non ignorable fields found in the event stream
  unknown_non_ignorable_fields,
  /// @brief Invalid event - cannot read an event
  invalid_event,
  /// @brief End of enum, put additional constants above
  last
};

}  // namespace mysql::binlog::event

/// @}

#endif  // MYSQL_BINLOG_EVENT_EVENT_DECODING_ERROR_H
