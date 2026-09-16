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

#include "health_monitor_util.h"

#include <algorithm>  // std::clamp
#include <chrono>
#include <cmath>  // std::isfinite
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

#include "health_monitor_constants.h"

namespace hm {

//------------------------------------------------------------------------------
// Conversion utilities
//------------------------------------------------------------------------------

/**
  Compute (part / whole) * 100, clamped to [0, 100].
  @param part   The numerator (e.g. MB used)
  @param whole  The denominator (e.g. MB available)
  @return Percentage in range [0.0, 100.0]
*/
double utl::percent(double part, double whole) {
  if (whole <= 0.0 || !std::isfinite(whole)) {
    return 0.0;
  }
  double clamped_part = std::clamp(part, 0.0, whole);
  return (clamped_part / whole) * 100.0;
}

/**
  Safe conversion functions.

  @param s The input string to be converted
  @param result Result of the conversion
  @return false if successful, true otherwise

  @note The functions check if the entire string is consumed during
        the conversion. If the string is empty or contains non-numeric
        characters, the function returns true.
*/
bool utl::stoi(const std::string &s, int &result) {
  if (s.empty()) {
    return true;
  }
  std::istringstream ss(s);
  // Success if conversion is successful and entire string was consumed
  return !((ss >> result) && ss.eof());
}

bool utl::stoul(const std::string &s, uint64_t &result) {
  if (s.empty() || s[0] == '-') {
    return true;
  }
  std::istringstream ss(s);
  return !((ss >> result) && ss.eof());
}

bool utl::stoll(const std::string &s, int64_t &result) {
  if (s.empty()) {
    return true;
  }
  std::istringstream ss(s);
  return !((ss >> result) && ss.eof());
}

bool utl::stod(const std::string &s, double &result) {
  if (s.empty()) {
    return true;
  }
  std::istringstream ss(s);
  ss >> std::noskipws;  // don't skip whitespace
  return !((ss >> result) && ss.eof());
}

std::string utl::trim(const std::string &s) {
  if (s.empty()) {
    return "";
  }
  size_t start = s.find_first_not_of(' ');
  size_t end = s.find_last_not_of(' ');
  return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
}

#if 0
/**
  Converts a time interval string to seconds.

  @param interval Time interval string (e.g., '1h', '5d', '3m', '5y')
  @param seconds  Output parameter for the converted seconds
  @return 0 on success, 1 on failure
*/
int utl::time_interval_to_seconds(const std::string &interval,
                                  uint64_t &seconds) {
  static const std::unordered_map<char, uint64_t> conversion_factors = {
      {'h', seconds_per_hour},   // 1 hour = 3600 seconds
      {'d', seconds_per_day},    // 1 day = 86400 seconds
      {'m', seconds_per_month},  // 1 month = 2592000 seconds (approximate)
      {'y', seconds_per_year}    // 1 year = 31536000 seconds (approximate)
  };

  if (interval.empty()) {
    return 1;
  }

  // Check if the last character is a valid unit
  char unit = interval.back();
  if (conversion_factors.find(unit) == conversion_factors.end()) {
    return 1;
  }

  // Check if the rest of the string is a positive integer
  try {
    int64_t value{0};
    utl::stoll(interval.substr(0, interval.size() - 1), value);
    if (value <= 0) {
      return 1;
    }
    // Convert the value to seconds
    seconds = value * conversion_factors.at(unit);
    return 0;
  } catch (const std::invalid_argument &e) {
    // Not a valid integer
    return 1;
  } catch (const std::out_of_range &e) {
    // Value is out of range
    return 1;
  }
}
#endif

/**
  Convert a Unix-style timestamp to time_t.

  @param[in]  ts_str  Unix timestamp string, e.g. "2022-01-01 12:00:00"
  @param[out] result  Converted timestamp
  @return 0 on success, non-zero otherwise
*/
int utl::convert_timestamp(const std::string &ts_str, time_t &result) {
  std::tm t{};  // parsed time structure
  std::istringstream ss(ts_str);
  // Parse timestamp string into std::tm using the specified format
  ss >> std::get_time(&t, "%Y-%m-%d %H:%M:%S");
  if (ss.fail()) {
    return 1;
  }
  // Convert parsed time to time_t (UTC)
  result = timegm(&t);
  return 0;
}

//------------------------------------------------------------------------------
// Formatting utilities
//------------------------------------------------------------------------------

#if 0
std::string fmt::format_bytes(const std::string &bytes_str) {
  if (bytes_str.empty()) {
    return "";
  }

  static const char *units[] = {"B", "KB", "MB", "GB", "TB", "PB"};
  int unit{0};
  double size{0.0};
  uint64_t bytes{0};

  if (!utl::stoul(bytes_str, bytes)) {
    return "";
  }
  size = static_cast<double>(bytes);
  while (size >= 1024.0 && unit < 5) {
    size /= 1024.0;
    ++unit;
  }
  std::stringstream ss;
  ss << std::fixed << std::setprecision(2) << size << " " << units[unit];
  return ss.str();
}

std::string fmt::format_bytes(uint64_t bytes) {
  static const char *units[] = {"B", "KB", "MB", "GB", "TB", "PB"};
  int unit{0};
  double size{static_cast<double>(bytes)};
  std::stringstream ss;

  while (size >= 1024.0 && unit < 5) {
    size /= 1024.0;
    ++unit;
  }
  ss << std::fixed << std::setprecision(2) << size << " " << units[unit];
  return ss.str();
}
#endif

std::string fmt::format_percent(const std::string &percent_str) {
  if (percent_str.empty()) {
    return "";
  }
  // double percent{0.0};
  int percent{0};
  if (!utl::stoi(percent_str, percent)) {
    return "";
  }
  std::stringstream ss;
  ss << std::fixed << std::setprecision(2) << percent;
  return ss.str() + "%";
}

std::string fmt::format_percent(double percent) {
  std::stringstream ss;
  ss << std::fixed << std::setprecision(2) << percent << "%";
  return ss.str();
}

std::string fmt::format_ms(const std::string &ms_str, bool units) {
  if (ms_str.empty()) {
    return "";
  }
  // TODO: Support larger units
  double ms{0.0};
  if (!utl::stod(ms_str, ms)) {
    return "";
  }
  std::stringstream ss;
  ss << std::fixed << std::setprecision(2) << (ms / 1000.0);
  if (units) {
    ss << " s";
  }
  return ss.str();
}

std::string fmt::format_ms(uint64_t ms, bool units) {
  std::stringstream ss;
  ss << std::fixed << std::setprecision(2) << (ms / 1000.0);
  if (units) ss << " s";
  return ss.str();
}

/**
  Format time - Convert seconds to YYYY-MM-DD HH:MM:SS
  @param time_value  Numeric time value in seconds, e.g. from time()
  @returns Formatted time string
*/
std::string fmt::format_time(time_t time_value) {
  std::tm ti{};
  if (localtime_r(&time_value, &ti) == nullptr) {
    return {};
  }
  char buffer[20];  // "YYYY-MM-DD HH:MM:SS" + null fits (19 + 1)
  const size_t n =
      std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &ti);
  if (n == 0) {
    return {};
  }
  return {buffer, n};  // construct from the produced length
}

/**
  Format time - Convert microseconds to seconds
  @param tv  Structured time value in microseconds, i.e. from gettimeofday()
  @returns Formatted time string
*/
std::string fmt::format_time(const struct timeval &tv) {
  double seconds = tv.tv_sec + (tv.tv_usec / 1000000.0);
  std::stringstream ss;
  ss << std::fixed << std::setprecision(3) << seconds << " s";
  return ss.str();
}

/**
  Format time - Convert string of microsends to seconds
  @param tv_str  String with time value as 'sec.usec'
  @returns Formatted time string
*/
std::string fmt::format_time(const std::string &tv_str) {
  if (tv_str.empty()) {
    return "";
  }
  // Assuming tv_str is in the format "sec.usec"
  size_t dot_pos = tv_str.find('.');
  if (dot_pos == std::string::npos) {
    return tv_str;
  }
  time_t sec;
  suseconds_t usec;
  try {
    sec = std::stoll(tv_str.substr(0, dot_pos));
    usec = std::stoll(tv_str.substr(dot_pos + 1));
  } catch (const std::exception &) {
    return tv_str;  // return original string
  }
  // struct timeval tv{.tv_sec = sec, .tv_usec = usec};
  struct timeval tv {
    sec, usec
  };
  return format_time(tv);
}

/**
  Format the horizon window for external use.
  @param minutes  The forecast horizon in minutes.
  @param verbose  True for long format, false for compact format
  @return         Verbose: 30 min, 1 hour, 2 days, ...
                  Compact: 60m, 24h, 48h, 72h, 4.5d, 7.0d, 30d, ...
*/
std::string fmt::format_horizon(int32_t minutes, bool verbose) {
  std::string days = verbose ? " days" : "d";
  std::string hours = verbose ? " hours" : "h";
  std::string mins = verbose ? " min" : "m";

  // Clamp negatives to 0 and work in unsigned for mixed comparisons
  const uint64_t mu = static_cast<uint64_t>(std::max<int32_t>(minutes, 0));

  // Use minutes for 3 hours or less
  if (mu <= minutes_per_hour * 3) {
    return std::to_string(static_cast<int64_t>(mu)) + mins;
  }

  // Use hours for 3 days or less
  if (mu <= minutes_per_day * 3) {
    return std::to_string(static_cast<int64_t>(mu / 60)) + hours;
  }

  std::ostringstream oss;
  oss << std::fixed << std::setprecision(1)
      << (static_cast<double>(mu) / static_cast<double>(minutes_per_day))
      << days;
  return oss.str();
}

}  // namespace hm
