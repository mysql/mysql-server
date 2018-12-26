/*
 * Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#ifndef NGS_TO_STRING_H_
#define NGS_TO_STRING_H_

#include <string>
#include <cstdlib>
#include "mysql/service_my_snprintf.h"
#include "m_string.h"

namespace ngs {

namespace detail {

template <typename T>
inline std::string to_string(const char* const str, T value) {
  char buffer[32];
  (void)my_snprintf(buffer, sizeof(buffer), str, value);
  return buffer;
}

template <typename T>
inline std::string to_string(const my_gcvt_arg_type arg_type, T value) {
  char buffer[100];
  my_gcvt(value, arg_type, sizeof(buffer)-1, buffer, NULL);
  return buffer;
}

}  // namespace detail

inline std::string to_string(const bool value) {
  return detail::to_string("%s", value ? "true" : "false");
}

inline std::string to_string(const int value) {
  return detail::to_string("%d", value);
}

inline std::string to_string(const unsigned value) {
  return detail::to_string("%u", value);
}

inline std::string to_string(const long value) {
  return detail::to_string("%ld", value);
}

inline std::string to_string(const long long value) {
  return detail::to_string("%lld", value);
}

inline std::string to_string(const unsigned long value) {
  return detail::to_string("%lu", value);
}

inline std::string to_string(const unsigned long long value) {
  return detail::to_string("%llu", value);
}

inline std::string to_string(const float value) {
  return detail::to_string(MY_GCVT_ARG_FLOAT, value);
}

inline std::string to_string(const double value) {
  return detail::to_string(MY_GCVT_ARG_DOUBLE, value);
}

inline int stoi(const std::string& str) { return std::atoi(str.c_str()); }

inline double stod(const std::string& str) { return std::atof(str.c_str()); }

}  // namespace ngs

#endif  // NGS_TO_STRING_H_
