/*
  Copyright (c) 2018, 2024, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <array>
#include <cstdio>   // sscanf
#include <cstring>  // memset
#include <ctime>    // mktime, gmtime_r, gmtime_s
#include <map>
#include <stdexcept>  // out_of_range
#include <string>

#include "http/base/http_time.h"

namespace http {
namespace base {

int time_to_rfc5322_fixdate(time_t ts, char *date_buf, size_t date_buf_len) {
  struct tm t_m;

#ifdef _WIN32
  // returns a errno_t
  if (0 != gmtime_s(&t_m, &ts)) {
    return 0;  // no bytes written to output
  }
#else
  if (nullptr == gmtime_r(&ts, &t_m)) {
    return 0;  // no bytes written to output
  }
#endif

  constexpr std::array kDayNames{"Sun", "Mon", "Tue", "Wed",
                                 "Thu", "Fri", "Sat"};

  constexpr std::array kMonthNames{"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                   "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

  return snprintf(date_buf, date_buf_len, "%s, %02d %s %4d %02d:%02d:%02d GMT",
                  kDayNames.at(t_m.tm_wday), t_m.tm_mday,
                  kMonthNames.at(t_m.tm_mon), 1900 + t_m.tm_year, t_m.tm_hour,
                  t_m.tm_min, t_m.tm_sec);
}

static time_t time_from_struct_tm_utc(struct tm *t_m) {
#if defined(_WIN32)
  return _mkgmtime(t_m);
#elif defined(__sun)
  // solaris, linux have typeof('timezone') == time_t
  return mktime(t_m) - timezone;
#else
  // linux, freebsd and apple have timegm()
  return timegm(t_m);
#endif
}

time_t time_from_rfc5322_fixdate(const char *date_buf) {
  // we can't use strptime as
  //
  // - it isn't portable
  // - takes locale into account, but we need en_EN all the time
  //
  // std::regex is broken on gcc-4.x
  // sscanf it is.
  //
  struct tm t_m;
  memset(&t_m, 0, sizeof(t_m));

  char wday[4];
  char mon[4];
  char timezone_s[4];
  if (8 != sscanf(date_buf, "%3s, %2u %3s %4u %2u:%2u:%2u %3s", wday,
                  &t_m.tm_mday, mon, &t_m.tm_year, &t_m.tm_hour, &t_m.tm_min,
                  &t_m.tm_sec, timezone_s)) {
    throw std::out_of_range("invalid date");
  }

  const std::map<std::string, decltype(t_m.tm_mon)> weekdays{
      {"Sun", 0}, {"Mon", 1}, {"Tue", 2}, {"Wed", 3},
      {"Thu", 4}, {"Fri", 5}, {"Sat", 6},
  };

  if (weekdays.find(wday) == weekdays.end()) {
    throw std::out_of_range(wday);
  }

  // throws out-of-range
  t_m.tm_mon =
      std::map<std::string, decltype(t_m.tm_mon)>{
          {"Jan", 0}, {"Feb", 1}, {"Mar", 2},  {"Apr", 3},
          {"May", 4}, {"Jun", 5}, {"Jul", 6},  {"Aug", 7},
          {"Sep", 8}, {"Oct", 9}, {"Nov", 10}, {"Dec", 11},
      }
          .at(mon);
  if (t_m.tm_year < 1900) {
    throw std::out_of_range("year too small");
  }
  if (std::string(timezone_s) != "GMT") {
    throw std::out_of_range("invalid timezone");
  }
  t_m.tm_year -= 1900;

  return time_from_struct_tm_utc(&t_m);
}

}  // namespace base
}  // namespace http
