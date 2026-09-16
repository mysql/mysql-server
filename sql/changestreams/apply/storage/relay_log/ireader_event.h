// Copyright (c) 2026, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

#ifndef MYSQL_CSA_RELAY_LOG_IREADER_EVENT_H
#define MYSQL_CSA_RELAY_LOG_IREADER_EVENT_H

#include <memory>
#include "sql/log_event.h"  // Log_event

namespace mysql::csa {

class IReader_event;
using IReader_event_ptr = std::shared_ptr<IReader_event>;

/// @brief Interface for abstract event data returned by the reader,
/// which can be decoded into a log event using the "decode" function
/// @see Reader_event_controller_read_type
class IReader_event {
 public:
  /// @brief Decode function, which obtains Log event smart pointer
  /// @return Log event smart pointer
  virtual std::shared_ptr<Log_event> decode() = 0;
  virtual ~IReader_event() = default;
  /// @brief Resets event state
  virtual void reset(const Format_description_log_event *) = 0;
};

}  // namespace mysql::csa

#endif  // MYSQL_CSA_RELAY_LOG_IREADER_EVENT_H
