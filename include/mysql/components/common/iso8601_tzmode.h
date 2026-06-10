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

#ifndef ISO8601_TZMODE_H
#define ISO8601_TZMODE_H

#ifdef __cplusplus
extern "C" {
#endif

/*
   enum_iso8601_tzmode is used to specify timestamp mode for ISO8601/RFC3339.
   The meaning of the values should match any use in server or plugin sources.
*/
enum enum_iso8601_tzmode {
  iso8601_sysvar_logtimestamps = -1, /**< use value of opt_log_timestamps */
  iso8601_utc = 0,
  iso8601_system_time = 1 /**< use system time zone */
};

#ifdef __cplusplus
}
#endif

#endif /* ISO8601_TZMODE_H */
