/*
   Copyright (c) 2010, 2026, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "ndb_internal.hpp"
#include "API.hpp"
#include "portlib/NdbTimestamp.h"

int Ndb_internal::send_event_report(bool is_poll_owner, Ndb *ndb, Uint32 *data,
                                    Uint32 length) {
  return ndb->theImpl->send_event_report(is_poll_owner, data, length);
}

void Ndb_internal::setForceShortRequests(Ndb *ndb, bool val) {
  ndb->theImpl->forceShortRequests = val;
}

void Ndb_internal::set_TC_COMMIT_ACK_immediate(Ndb *ndb, bool flag) {
  ndb->theImpl->set_TC_COMMIT_ACK_immediate(flag);
}

int Ndb_internal::send_dump_state_all(Ndb *ndb, Uint32 *dumpStateCodeArray,
                                      Uint32 len) {
  return ndb->theImpl->send_dump_state_all(dumpStateCodeArray, len);
}

int Ndb_internal::set_log_timestamp_format(log_timestamp_format format) {
  switch (format) {
    case log_timestamp_format::default_format:
      NdbTimestamp_SetDefaultStringFormat(
          NdbTimestampStringFormat::DefaultFormat);
      break;
    case log_timestamp_format::legacy_format:
      NdbTimestamp_SetDefaultStringFormat(
          NdbTimestampStringFormat::LegacyFormat);
      break;
    case log_timestamp_format::iso8601_utc:
      NdbTimestamp_SetDefaultStringFormat(NdbTimestampStringFormat::Iso8601Utc);
      break;
    case log_timestamp_format::iso8601_system_time:
      NdbTimestamp_SetDefaultStringFormat(
          NdbTimestampStringFormat::Iso8601SystemTime);
      break;
    default:
      return -1;
  }
  return 0;
}
