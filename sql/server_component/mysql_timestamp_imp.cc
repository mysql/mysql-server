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

#include "mysql_timestamp_imp.h"
#include <my_sys.h>
#include <mysql/components/service_implementation.h>
#include <cstdarg>
#include "my_systime.h"

extern ulong opt_log_timestamps;

DEFINE_METHOD(int, Mysql_timestamp_imp::make_iso8601_timestamp_now,
              (char *buffer, size_t size)) {
  if (size < iso8601_size) return 0;
  return Mysql_timestamp_imp::make_iso8601_timestamp(
      buffer, my_micro_time(), iso8601_sysvar_logtimestamps);
}

DEFINE_METHOD(int, Mysql_timestamp_imp::make_iso8601_timestamp,
              (char *buf, ulonglong utime, enum enum_iso8601_tzmode mode)) {
  struct tm my_tm;
  char tzinfo[8] = "Z";  // max 6 chars plus \0
  size_t len;
  time_t seconds;

  seconds = utime / 1000000;
  utime = utime % 1000000;

  if (mode == iso8601_sysvar_logtimestamps)
    mode = (opt_log_timestamps == 0) ? iso8601_utc : iso8601_system_time;

  if (mode == iso8601_utc)
    gmtime_r(&seconds, &my_tm);
  else if (mode == iso8601_system_time) {
    localtime_r(&seconds, &my_tm);

#ifdef HAVE_TM_GMTOFF
    /*
      The field tm_gmtoff is the offset (in seconds) of the time represented
      from UTC, with positive values indicating east of the Prime Meridian.
      Originally a BSDism, this is also supported in glibc, so this should
      cover the majority of our platforms.
    */
    long tim = -my_tm.tm_gmtoff;
#else
    /*
      Work this out "manually".
    */
    struct tm my_gm;
    long tim, gm;
    gmtime_r(&seconds, &my_gm);
    gm = (my_gm.tm_sec + 60 * (my_gm.tm_min + 60 * my_gm.tm_hour));
    tim = (my_tm.tm_sec + 60 * (my_tm.tm_min + 60 * my_tm.tm_hour));
    tim = gm - tim;
#endif
    char dir = '-';

    if (tim < 0) {
      dir = '+';
      tim = -tim;
    }
    snprintf(tzinfo, sizeof(tzinfo), "%c%02u:%02u", dir,
             (unsigned int)((tim / (60 * 60)) % 100),
             (unsigned int)((tim / 60) % 60));
  } else {
    assert(false);
  }

  // length depends on whether timezone is "Z" or "+12:34" style
  len = snprintf(buf, iso8601_size, "%04d-%02d-%02dT%02d:%02d:%02d.%06lu%s",
                 my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
                 my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec,
                 (unsigned long)utime, tzinfo);

  return std::min<int>((int)len, iso8601_size - 1);
}
