/*
 * Copyright (c) 2016 Oracle and/or its affiliates. All rights reserved.
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

namespace ngs {

namespace detail {

template <typename T>
inline std::string to_string(const char* const str, T value) {
  char buffer[32];
  (void)my_snprintf(buffer, sizeof(buffer), str, value);
  return buffer;
}

}  // namespace detail

inline std::string to_string(int value) {
  return detail::to_string("%d", value);
}

inline std::string to_string(unsigned value) {
  return detail::to_string("%u", value);
}

inline std::string to_string(long value) {
  return detail::to_string("%ld", value);
}

inline std::string to_string(long long value) {
  return detail::to_string("%lld", value);
}

inline std::string to_string(unsigned long value) {
  return detail::to_string("%lu", value);
}

inline std::string to_string(unsigned long long value) {
  return detail::to_string("%llu", value);
}

inline int stoi(const std::string& str) { return std::atoi(str.c_str()); }

inline double stod(const std::string& str) { return std::atof(str.c_str()); }

}  // namespace ngs

#endif  // NGS_TO_STRING_H_
