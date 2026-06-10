/* Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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

#ifndef MYSQL_TIMESTAMP_H
#define MYSQL_TIMESTAMP_H

#include <my_inttypes.h>
#include <mysql/components/common/iso8601_tzmode.h>
#include <mysql/components/service.h>
//! @cond Doxygen_Suppress
#include <stdarg.h>
//! @endcond

/**
  This service defines the timestamp function api.
*/
BEGIN_SERVICE_DEFINITION(mysql_timestamp)
/**
  Make and return an ISO 8601 / RFC 3339 compliant timestamp.

  @param buffer    a buffer of at least iso8601_size bytes to store
                   the timestamp in. The timestamp will be \0 terminated.
  @param size      size of the buffer.

  @retval          0 if size < iso8601_size
                   else length of the timestamp (excluding \0)
*/
DECLARE_METHOD(int, make_iso8601_timestamp_now, (char *buffer, size_t size));

/**
  Make and return an ISO 8601 / RFC 3339 compliant timestamp.
  Accepts the log_timestamps global variable in its third parameter.

  @param buf       A buffer of at least iso8601_size bytes to store
                   the timestamp in. The timestamp will be \0 terminated.
  @param utime     Microseconds since the epoch
  @param mode      if 0, use UTC; if 1, use local time

  @retval          length of timestamp (excluding \0)
*/
DECLARE_METHOD(int, make_iso8601_timestamp,
               (char *buffer, ulonglong utime, enum enum_iso8601_tzmode mode));

END_SERVICE_DEFINITION(mysql_timestamp)

#endif /* MYSQL_TIMESTAMP_H */
