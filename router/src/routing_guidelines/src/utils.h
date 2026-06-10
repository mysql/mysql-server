/*
 * Copyright (c) 2024, 2026, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
 *
 * This program is distributed in the hope that it will be useful,  but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 * the GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef ROUTER_SRC_ROUTING_GUIDELINES_SRC_UTILS_H_
#define ROUTER_SRC_ROUTING_GUIDELINES_SRC_UTILS_H_

#include <algorithm>
#include <cstring>
#include <functional>
#include <ranges>
#include <string>
#include <vector>

namespace rapidjson {
struct ParseResult;
}

namespace routing_guidelines {

std::string str_strip(const std::string &s,
                      const std::string &chars = " \r\n\t");

inline bool str_eq(std::string_view a, std::string_view b) {
  return std::ranges::equal(a, b);
}

inline bool str_caseeq(const char *a, const char *b) {
#ifdef _WIN32
  return ::_stricmp(a, b) == 0;
#else
  return ::strcasecmp(a, b) == 0;
#endif
}

inline bool str_caseeq(const char *a, const char *b, size_t n) {
#ifdef _WIN32
  return ::_strnicmp(a, b, n) == 0;
#else
  return ::strncasecmp(a, b, n) == 0;
#endif
}

inline bool str_caseeq(std::string_view lhs, std::string_view rhs) {
  return std::ranges::equal(lhs, rhs, [](const auto a, const auto b) {
    return std::tolower(static_cast<unsigned char>(a)) ==
           std::tolower(static_cast<unsigned char>(b));
  });
}

/** Compares 2 strings case insensitive (for ascii) */
inline int str_casecmp(const char *a, const char *b) {
#ifdef _WIN32
  return ::_stricmp(a, b);
#else
  return ::strcasecmp(a, b);
#endif
}

inline int str_casecmp(const std::string &a, const std::string &b) {
  return str_casecmp(a.c_str(), b.c_str());
}

inline bool str_ibeginswith(std::string_view str, std::string_view prefix) {
  if (prefix.size() > str.size()) return false;

  for (size_t i = 0; i < prefix.size(); ++i) {
    if (std::tolower(static_cast<unsigned char>(str[i])) !=
        std::tolower(static_cast<unsigned char>(prefix[i]))) {
      return false;
    }
  }

  return true;
}

inline bool str_beginswith(std::string_view str, std::string_view prefix) {
  return str.starts_with(prefix);
}

/** Convert a copy of an ASCII string to uppercase and return */
inline std::string str_upper(std::string_view s) {
  std::string r(s);
  std::transform(r.begin(), r.end(), r.begin(), ::toupper);
  return r;
}

/** Convert a copy of an ASCII string to lowercase and return */
inline std::string str_lower(std::string_view s) {
  std::string r(s);
  std::transform(r.begin(), r.end(), r.begin(), ::tolower);
  return r;
}

/// process escapes in a string , keep synchronous with sql_load unescape
/// ("ntrb0ZN")
std::string mysql_unescape_string(std::string_view s);

std::string like_to_regexp(std::string_view pattern);

/// Check if container contains identincal string independent of case
template <typename Container>
bool case_contains(const Container &container, std::string_view str) {
  for (const auto &el : container)
    if (str_caseeq(str, el)) return true;
  return false;
}

bool is_ipv4(const std::string &address);
bool is_ipv6(const std::string &address);

/// Compute network part of an IPv4 address
std::string network(const std::string &address, unsigned int bitlen);

/** Format JSON parse error adding responsible JSON part
 *
 * @param s parsed JSON
 * @param ok parse result
 * @param chars how big surrounding of erroneous part to attach
 */
std::string format_json_error(const std::string &s,
                              const rapidjson::ParseResult &ok, size_t chars);

/// Is JSON document held by the string complete or not
bool json_document_complete(const std::string &s);

}  // namespace routing_guidelines

#endif  // ROUTER_SRC_ROUTING_GUIDELINES_SRC_UTILS_H_
