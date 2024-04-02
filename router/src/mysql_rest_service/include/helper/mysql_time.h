/*
  Copyright (c) 2021, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef ROUTER_SRC_REST_MRS_SRC_HELPER_MYSQL_TIME_H_
#define ROUTER_SRC_REST_MRS_SRC_HELPER_MYSQL_TIME_H_

#include <time.h>
#include <iomanip>
#include <sstream>
#include <string>

#include "my_macros.h"

namespace helper {

class DateTime {
 public:
  DateTime() : time_{0} {}

  DateTime(const std::string &text_time) { from_string(text_time); }

  void from_string(const std::string &text_time) {
    tm out_time;
#ifdef HAVE_STRPTIME
    strptime(text_time.c_str(), "%Y-%m-%d %T", &out_time);
#else
    std::stringstream timestamp_ss(text_time);

    timestamp_ss >> std::get_time(&out_time, "%Y-%m-%d %T");
#endif
    time_ = IF_WIN(_mkgmtime, timegm)(&out_time);
  }

  std::string to_string() const {
    std::string result(70, '\0');

    tm out_time;

#ifdef _WIN32
    gmtime_s(&out_time, &time_);
#else
    gmtime_r(&time_, &out_time);
#endif
    auto size =
        strftime(&result[0], result.length(), "'%Y-%m-%d %T'", &out_time);

    if (0 == size) {
      result[0] = '0';
      size = 1;
    }

    result.resize(size);

    return result;
  }

  friend bool operator<=(const DateTime &l, const DateTime &r);

  time_t time_;

  // TODO(lkotula): move private up (Shouldn't be in review)
 private:
};

inline bool operator<=(const DateTime &l, const DateTime &r) {
  return l.time_ <= r.time_;
}

}  // namespace helper

#endif  // ROUTER_SRC_REST_MRS_SRC_HELPER_MYSQL_TIME_H_
