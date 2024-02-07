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

#include "mysql/binlog/event/codecs/factory.h"
#include "mysql/binlog/event/codecs/binary.h"

namespace mysql::binlog::event::codecs {

std::unique_ptr<Codec> Factory::build_codec(Log_event_type t) {
  switch (t) {
    case TRANSACTION_PAYLOAD_EVENT:
      return std::make_unique<
          mysql::binlog::event::codecs::binary::Transaction_payload>();
    case HEARTBEAT_LOG_EVENT_V2:
      return std::make_unique<
          mysql::binlog::event::codecs::binary::Heartbeat>();
    default:              /* purecov: inspected */
      BAPI_ASSERT(false); /* purecov: inspected */
  }

  return nullptr;
}

}  // namespace mysql::binlog::event::codecs
