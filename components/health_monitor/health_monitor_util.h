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

#pragma once

#include <cstdint>  // uint64_t
#include <iostream>
#include <sstream>
#include <string>

namespace hm {

class utl {
 public:
  utl() = delete;  // prevent instantiation
  static double percent(double part, double whole);
  /** Return the absolute percentage change, or zero without a baseline. */
  static double change_percent(uint64_t previous, uint64_t current) {
    if (previous == 0) return 0.0;
    const uint64_t difference =
        current > previous ? current - previous : previous - current;
    return static_cast<double>(difference) / previous * 100.0;
  }
  static bool stoi(const std::string &s, int &result);
  static bool stoul(const std::string &s, uint64_t &result);
  static bool stoll(const std::string &s, int64_t &result);
  static bool stod(const std::string &s, double &result);
  static std::string trim(const std::string &s);
#if 0
  static int time_interval_to_seconds(const std::string &interval,
                                      uint64_t &seconds);
#endif
  static int convert_timestamp(const std::string &ts_str, time_t &result);
};

class fmt {
 public:
  fmt() = delete;  // prevent instantiation
#if 0
  static std::string format_bytes(const std::string &bytes_str);
  static std::string format_bytes(uint64_t bytes);
  static std::string format_bytes_str(const std::string str) {
    return format_bytes(str);
  }
#endif
  static std::string format_percent(const std::string &percent_str);
  static std::string format_percent(double percent);
  static std::string format_ms(const std::string &ms_str, bool units = true);
  static std::string format_ms(uint64_t ms, bool units = true);
  static std::string format_ms_str(const std::string &str) {
    return format_ms(str);
  }
  static std::string format_ms_str_value(const std::string &str) {
    return format_ms(str, false);
  }
  static std::string format_time(time_t time_value);
  static std::string format_time(const struct timeval &tv);
  static std::string format_time(const std::string &tv_str);
  static std::string format_time_str(const std::string str) {
    return format_time(str);
  }
  static std::string format_horizon(int32_t minutes, bool verbose = false);
};

}  // namespace hm
