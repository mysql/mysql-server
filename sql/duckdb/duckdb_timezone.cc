/*****************************************************************************

Copyright (c) 2025, Alibaba and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

#include "sql/duckdb/duckdb_timezone.h"
#include "my_time.h"
#include "sql/derror.h"

namespace myduck {

std::map<int64_t, std::string> TimeZoneOffsetHelper::timezone_offset_map;

/**
  Retrieves the timezone offset of the system in seconds.

  This function calculates the difference in seconds between the local system
  time and Coordinated Universal Time (UTC). It uses the `localtime_r` function
  to obtain the local time and then computes the offset by comparing the seconds
  since the Unix epoch (1970-01-01 00:00:00 UTC) with offsets get by
  my_system_gmt_sec.

  @return The timezone offset in seconds.
*/
static my_time_t get_system_timezone_offset() {
  time_t seconds_os;
  struct tm l_time;

  seconds_os = time(nullptr);
  localtime_r(&seconds_os, &l_time);

  MYSQL_TIME t;
  t.year = static_cast<uint>(l_time.tm_year) + 1900;
  t.month = static_cast<uint>(l_time.tm_mon) + 1;
  t.day = static_cast<uint>(l_time.tm_mday);
  t.hour = static_cast<uint>(l_time.tm_hour);
  t.minute = static_cast<uint>(l_time.tm_min);
  t.second = static_cast<uint>(l_time.tm_sec);
  t.time_type = MYSQL_TIMESTAMP_DATETIME;
  t.neg = false;
  t.second_part = 0;

  /* Compute seconds since 1970-01-01 00:00:00 */
  my_time_t days =
      calc_daynr(static_cast<uint>(t.year), static_cast<uint>(t.month),
                 static_cast<uint>(t.day)) -
      static_cast<my_time_t>(days_at_timestart);
  my_time_t seconds =
      days * SECONDS_IN_24H + (static_cast<int64_t>(t.hour) * 3600 +
                               static_cast<int64_t>(t.minute * 60 + t.second));

  /* Get my_time_t form by system time zone. */
  my_time_t not_used_tz;
  bool not_used_gap;
  my_time_t seconds_syszone = my_system_gmt_sec(t, &not_used_tz, &not_used_gap);

  assert(seconds_syszone == seconds_os);

  return (seconds - seconds_syszone);
}

std::string convert_timezone_for_duckdb(Time_zone *tz, std::string &warn_msg) {
  std::string target_tzname;

  switch (tz->get_timezone_type()) {
    case Time_zone::TZ_SYSTEM: {
      /* Convert system timezone. */
      my_time_t offset = get_system_timezone_offset();
      target_tzname =
          TimeZoneOffsetHelper::get_name_by_offset(offset, warn_msg);
      break;
    }

    case Time_zone::TZ_DB: {
      /* We believe mysql's time_zone name is valid for duckdb. */
      target_tzname =
          std::string(tz->get_name()->ptr(), tz->get_name()->length());
      break;
    }

    case Time_zone::TZ_OFFSET: {
      target_tzname = TimeZoneOffsetHelper::get_name_by_offset(
          tz->get_timezone_offset(), warn_msg);
      break;
    }

    case Time_zone::TZ_UTC: {
      assert(0);  // no reason
    }
  }

  return target_tzname;
}

std::string get_timezone_according_thd(THD *thd, std::string &warn_msg) {
  return convert_timezone_for_duckdb(thd->variables.time_zone, warn_msg);
}
}  // namespace myduck
