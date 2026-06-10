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

#include "mysqlrouter/jit_executor_value.h"

#include <array>
#include <cassert>
#include <charconv>
#include <cmath>
#include <cstring>
#include <sstream>
#include <stdexcept>

#include "include/mysql/strings/dtoa.h"
#include "native_wrappers/polyglot_object_bridge.h"
#include "polyglot_wrappers/types_polyglot.h"
#include "utils/utils_json.h"
#include "utils/utils_string.h"

namespace shcore {
namespace {

// #ifdef _WIN32

// std::wstring utf8_to_wide(const char *utf8, const size_t utf8_length) {
//   auto buffer_size_needed =
//       MultiByteToWideChar(CP_UTF8, 0, utf8, utf8_length, nullptr, 0);
//   std::wstring wide_string(buffer_size_needed, 0);
//   const auto wide_string_size = MultiByteToWideChar(
//       CP_UTF8, 0, utf8, utf8_length, &wide_string[0], buffer_size_needed);
//   wide_string.resize(wide_string_size);
//   return wide_string;
// }

// std::string wide_to_utf8(const wchar_t *wide, const size_t wide_length) {
//   auto string_size = WideCharToMultiByte(CP_UTF8, 0, wide, wide_length,
//   nullptr,
//                                          0, nullptr, nullptr);
//   std::string result_string(string_size, 0);
//   WideCharToMultiByte(CP_UTF8, 0, wide, wide_length, &result_string[0],
//                       string_size, nullptr, nullptr);
//   return result_string;
// }

// #else

// std::wstring utf8_to_wide(const char *utf8, const size_t utf8_length) {
//   std::mbstate_t state{};
//   const char *end = utf8 + utf8_length;
//   int len = 0;
//   std::wstring result;
//   wchar_t wc;

//   while ((len = std::mbrtowc(&wc, utf8, end - utf8, &state)) > 0) {
//     result += wc;
//     utf8 += len;
//   }

//   return result;
// }

// std::string wide_to_utf8(const wchar_t *wide, const size_t wide_length) {
//   std::mbstate_t state{};
//   std::string result;
//   std::string mb(MB_CUR_MAX, '\0');

//   for (size_t i = 0; i < wide_length; ++i) {
//     const auto len = std::wcrtomb(&mb[0], wide[i], &state);
//     result.append(mb.c_str(), len);
//   }

//   return result;
// }

// #endif

// std::wstring utf8_to_wide(const std::string &utf8) {
//   return utf8_to_wide(&utf8[0], utf8.size());
// }

// std::wstring utf8_to_wide(const char *utf8) {
//   return utf8_to_wide(utf8, strlen(utf8));
// }

// std::string wide_to_utf8(const std::wstring &wide) {
//   return wide_to_utf8(&wide[0], wide.size());
// }

// std::string wide_to_utf8(const wchar_t *wide) {
//   return wide_to_utf8(wide, wcslen(wide));
// }

std::string dtoa(double x) {
  char buffer[32];
  size_t len;
  len = my_gcvt(x, MY_GCVT_ARG_DOUBLE, sizeof(buffer) - 1, buffer, NULL);
  return std::string(buffer, len);
}

std::string ftoa(float x) {
  char buffer[32];
  size_t len;
  len = my_gcvt(double(x), MY_GCVT_ARG_FLOAT, sizeof(buffer) - 1, buffer, NULL);
  return std::string(buffer, len);
}

// std::string str_replace(std::string_view s, std::string_view from,
//                         std::string_view to) {
//   std::string str;

//   if (from.empty()) {
//     str.reserve(to.length() * (s.size() + 1));

//     str.append(to);
//     for (char c : s) {
//       str.push_back(c);
//       str.append(to);
//     }
//   } else {
//     str.reserve(s.length());

//     int offs = from.length();
//     std::string::size_type start = 0, p = s.find(from);
//     while (p != std::string::npos) {
//       if (p > start) str.append(s, start, p - start);
//       str.append(to);
//       start = p + offs;
//       p = s.find(from, start);
//     }
//     if (start < s.length()) str.append(s, start, s.length() - start);
//   }
//   return str;
// }

// std::string quote_string(const std::string &s, char quote) {
//   const std::string q{quote};
//   const std::string backslash = str_replace(s, "\\", "\\\\");
//   const std::string esc = str_replace(backslash, q, "\\" + q);
//   return std::string(q + esc + q);
// }

// std::string string_to_hex(std::string_view s, bool prefix = true) {
//   constexpr std::array<char, 16> hexmap = {'0', '1', '2', '3', '4', '5',
//                                            '6', '7', '8', '9', 'A', 'B',
//                                            'C', 'D', 'E', 'F'};

//   std::string encoded;

//   encoded.reserve((prefix ? 2 : 0) + s.size() * 2);
//   if (prefix) encoded.append("0x");

//   for (const auto cur_char : s) {
//     encoded.push_back(hexmap[(cur_char & 0xF0) >> 4]);
//     encoded.push_back(hexmap[cur_char & 0x0F]);
//   }

//   return encoded;
// }

// namespace internal {
// /** Compares 2 strings case insensitive (for ascii) */
// inline int str_casecmp(const char *a, const char *b) {
// #ifdef _WIN32
//   return ::_stricmp(a, b);
// #else
//   return ::strcasecmp(a, b);
// #endif
// }

// inline int str_casecmp(const wchar_t *a, const wchar_t *b) {
// #ifdef _WIN32
//   return ::_wcsicmp(a, b);
// #else
//   return ::wcscasecmp(a, b);
// #endif
// }

// inline int str_casecmp(const char *a, const char *b, size_t n) {
// #ifdef _WIN32
//   return ::_strnicmp(a, b, n);
// #else
//   return ::strncasecmp(a, b, n);
// #endif
// }

// inline int str_casecmp(const wchar_t *a, const wchar_t *b, size_t n) {
// #ifdef _WIN32
//   return ::_wcsnicmp(a, b, n);
// #else
//   return ::wcsncasecmp(a, b, n);
// #endif
// }

// template <typename Char>
// inline bool str_caseeq_pair(std::basic_string_view<Char> a,
//                             std::basic_string_view<Char> b) {
//   if (a.length() != b.length()) return false;
//   return str_casecmp(a.data(), b.data(), a.length()) == 0;
// }

// template <typename Char, typename... T>
// inline bool str_caseeq(std::basic_string_view<Char> a, T &&...tokens) {
//   return (str_caseeq_pair<Char>(a, std::forward<T>(tokens)) || ...);
// }

// }  // namespace internal

// template <typename... T>
// inline bool str_caseeq(std::string_view a, std::string_view token,
//                        T &&...tokens) {
//   return internal::str_caseeq(a, token, std::forward<T>(tokens)...);
// }

// template <typename... T>
// inline bool str_caseeq(std::wstring_view a, std::wstring_view token,
//                        T &&...tokens) {
//   return internal::str_caseeq(a, token, std::forward<T>(tokens)...);
// }

template <class T>
auto lexical_cast(T &&data) noexcept {
  return std::forward<T>(data);
}

template <class T>
auto lexical_cast(std::string_view str) {
  // this version converts from string to something else

  // same behavior as boost: doesn't make sense so convert back to these types
  static_assert(!std::is_same_v<std::remove_cv_t<T>, char *> &&
                    !std::is_same_v<std::remove_cv_t<T>, char> &&
                    !std::is_same_v<std::remove_cv_t<T>, std::string_view>,
                "Invalid type to convert to.");

  if constexpr (std::is_same_v<T, std::string>) {
    // simple copies are allowed
    return T{str};

  } else if constexpr (std::is_same<T, bool>::value) {
    // conversion to bool
    if (str_caseeq(str, "true")) return true;
    if (str_caseeq(str, "false")) return false;

    std::stringstream ss;
    ss << str;
    if (std::is_unsigned<T>::value && ss.peek() == '-')
      throw std::invalid_argument("Unable to perform conversion.");
    T t;
    ss >> t;
    if (ss.fail()) throw std::invalid_argument("Unable to perform conversion.");
    if (!ss.eof())
      throw std::invalid_argument("Conversion did not consume whole input.");

    return t;

  } else if constexpr (std::is_integral_v<T>) {
    // some compilers don't have FP implementations of std::from_chars (GCC and
    // Clang) so it can only be used with integrals
    T value;
    auto [ptr, ec] =
        std::from_chars(str.data(), str.data() + str.size(), value);

    if (ec != std::errc())
      throw std::invalid_argument("Unable to perform conversion.");
    if (ptr != (str.data() + str.size()))
      throw std::invalid_argument("Conversion did not consume whole input.");
    return value;

  } else {
    // all other cases
    std::stringstream ss;
    ss << str;
    if (std::is_unsigned<T>::value && ss.peek() == '-')
      throw std::invalid_argument("Unable to perform conversion.");
    T t;
    ss >> t;
    if (ss.fail()) throw std::invalid_argument("Unable to perform conversion.");
    if (!ss.eof())
      throw std::invalid_argument("Conversion did not consume whole input.");

    return t;
  }
}

template <class T, class S>
auto lexical_cast(S &&data) {
  // lexical_cast doesn't allow to convert to "char*" or std::string_view
  static_assert(!std::is_same_v<T, char *> &&
                !std::is_same_v<T, const char *> &&
                !std::is_same_v<T, std::string_view>);

  // lexical_cast should be to / from strings, so at least T or S must be one
  static_assert(std::is_same_v<std::decay_t<S>, char *> ||
                std::is_same_v<std::decay_t<S>, const char *> ||
                std::is_same_v<std::decay_t<S>, std::string> ||
                std::is_same_v<T, std::string>);
  /*
   - if T and S are the same, forward S
   - if S is a string, use the specialization for strings (above this one)
   - if T is a string, convert it here
   - every other combination is "translated" through a std::stringstream
  */
  if constexpr (std::is_same_v<std::decay_t<S>, T>) {
    // if T and S match, just return the parameter
    return std::forward<T>(data);
  } else if constexpr (std::is_same_v<std::decay_t<S>, char *> ||
                       std::is_same_v<std::decay_t<S>, const char *> ||
                       std::is_same_v<std::decay_t<S>, std::string>) {
    // if S is char* or std::string (const or non-const) then use
    // std::string_view version
    return lexical_cast<T>(std::string_view{data});

  } else {
    // convert from S (which is not char*, std::string or std::string_view) to
    // std::string
    if constexpr (std::is_same_v<std::decay_t<S>, bool>) {
      return std::string{data ? "true" : "false"};

    } else if constexpr (std::is_integral_v<std::decay_t<S>>) {
      std::array<char, 64> str{};

      auto [ptr, ec] = std::to_chars(str.data(), str.data() + str.size(), data);
      if (ec == std::errc()) return std::string{str.data(), ptr};

      throw std::invalid_argument("Unable to perform conversion.");

    } else {
      std::stringstream ss;
      ss << data;
      return ss.str();
    }
  }
}

template <class T, class S>
auto lexical_cast(S &&data, T default_value) noexcept {
  try {
    return lexical_cast<T>(std::forward<S>(data));
  } catch (...) {
  }
  return default_value;
}

// These is* functions have undefined behavior if the passed value
// is out of the -1-255 range
#define IS_ALPHA(x) (isalpha(static_cast<unsigned char>(x)))
#define IS_DIGIT(x) (isdigit(static_cast<unsigned char>(x)))

// kTypeConvertible[from_type][to_type] = is_convertible
// from_type = row, to_type = column
#define T true
#define F false
const bool kTypeConvertible[12][12] = {
    // Undf, Null,Bool,Str, Int, UInt,Flot,Obj, Arr, Map, Fun, Binary
    {T, F, F, F, F, F, F, F, F, F, F, F},  // Undefined
    {T, T, F, F, F, F, F, T, T, T, T, F},  // Null
    {T, F, T, F, T, T, T, F, F, F, F, F},  // Bool
    {T, F, T, T, T, T, T, F, F, F, F, T},  // String
    {T, F, T, F, T, T, T, F, F, F, F, F},  // Integer
    {T, F, T, F, T, T, T, F, F, F, F, F},  // UInteger
    {T, F, T, F, T, T, T, F, F, F, F, F},  // Float
    {T, F, F, F, F, F, F, T, F, F, F, F},  // Object
    {T, F, F, F, F, F, F, F, T, F, F, F},  // Array
    {T, F, F, F, F, F, F, F, F, T, F, F},  // Map
    {T, F, F, F, F, F, F, F, F, F, T, F},  // Function
    {T, F, F, T, F, F, F, F, F, F, F, T},  // Binary
};
#undef T
#undef F
// Note: Null can be cast to Object/Array/Map, but a valid Object/Array/Map
// pointer is not NULL, so they can't be cast to it.

/**
 * Translate hex value from printable ascii character ([0-9a-zA-Z]) to decimal
 * value
 */
constexpr uint32_t ascii_to_hex[256] = {
    0,  0,  0,  0, 0, 0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0,  0,  0,
    0,  0,  0,  0, 0, 0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0,  0,  0,
    0,  0,  0,  0, 0, 0,  0,  0,  0,  1,  2,  3, 4, 5, 6, 7, 8, 9,  0,  0,
    0,  0,  0,  0, 0, 10, 11, 12, 13, 14, 15, 0, 0, 0, 0, 0, 0, 0,  0,  0,
    0,  0,  0,  0, 0, 0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 10, 11, 12,
    13, 14, 15, 0, 0, 0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0,  0,  0,
    0,  0,  0,  0, 0, 0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0,  0,  0,
    0,  0,  0,  0, 0, 0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0,  0,  0,
    0,  0,  0,  0, 0, 0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0,  0,  0,
    0,  0,  0,  0, 0, 0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0,  0,  0,
    0,  0,  0,  0, 0, 0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0,  0,  0,
    0,  0,  0,  0, 0, 0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0,  0,  0,
    0,  0,  0,  0, 0, 0,  0,  0,  0,  0,  0,  0, 0, 0, 0};

[[nodiscard]] std::string_view skip_whitespace(std::string_view t) {
  for (; !t.empty(); t.remove_prefix(1)) {
    switch (t.front()) {
      case ' ':
      case '\t':
      case '\r':
      case '\n':
      case '\v':
      case '\f':
        continue;
    }

    break;
  }

  return t;
}

void unicode_codepoint_to_utf8(uint32_t uni, std::string *out) {
  assert(out);

  if (uni <= 0x7f) {
    out->push_back(static_cast<char>(uni & 0xff));  // 0xxxxxxx
  } else if (uni <= 0x7ff) {
    out->push_back(static_cast<char>(((uni >> 6) & 0xff) | 0xc0));  // 110xxxxxx
    out->push_back(static_cast<char>((uni & 0x3f) | 0x80));         // 10xxxxxx

  } else if (uni <= 0xffff) {
    out->push_back(static_cast<char>(((uni >> 12) & 0xff) | 0xe0));  // 1110xxxx
    out->push_back(static_cast<char>(((uni >> 6) & 0x3f) | 0x80));  // 110xxxxxx
    out->push_back(static_cast<char>((uni & 0x3f) | 0x80));         // 10xxxxxx
  } else {
    if (uni >= 0x10ffff) throw Parser_error("Invalid unicode codepoint");

    out->push_back(static_cast<char>(((uni >> 18) & 0xff) | 0xf0));  // 11110xxx
    out->push_back(static_cast<char>(((uni >> 12) & 0x3f) | 0x80));  // 1110xxxx
    out->push_back(static_cast<char>(((uni >> 6) & 0x3f) | 0x80));  // 110xxxxxx
    out->push_back(static_cast<char>((uni & 0x3f) | 0x80));         // 10xxxxxx
  }
}

// parsing is recursive so we need to forward declare the main function
[[nodiscard]] std::tuple<Value, std::string_view> parse_main(
    std::string_view pc);

[[nodiscard]] std::tuple<std::string, std::string_view> parse_string(
    std::string_view pc, char quote) {
  assert(pc.front() == quote);

  // obtain the string
  auto p = pc;
  {
    for (p.remove_prefix(1); !p.empty(); p.remove_prefix(1)) {
      auto cur_char = p.front();
      if (cur_char == quote) break;
      if (cur_char == '\\') p.remove_prefix(1);  // escaped char
    }

    if (p.empty() || (p.front() != quote))
      throw Parser_error(str_format("missing closing %c", quote));

    p = pc.substr(1, pc.size() - p.size() - 1);  // remove quotes

    pc.remove_prefix(p.size() + 2);  // next after quoted string
  }

  std::string s;
  s.reserve(p.size());

  for (; !p.empty(); p.remove_prefix(1)) {
    auto cur_char = p.front();
    if (cur_char != '\\') {
      s += cur_char;
      continue;
    }

    p.remove_prefix(1);  // ignore escape char
    if (p.empty()) throw Parser_error("incorrect use of escape char '\\'");

    cur_char = p.front();
    switch (cur_char) {
      case 'n':
        s += '\n';
        break;
      case '"':
        s += '"';
        break;
      case '\'':
        s += '\'';
        break;
      case 'a':
        s += '\a';
        break;
      case 'b':
        s += '\b';
        break;
      case 'f':
        s += '\f';
        break;
      case 'r':
        s += '\r';
        break;
      case 't':
        s += '\t';
        break;
      case 'v':
        s += '\v';
        break;
      case '\\':
        s += '\\';
        break;
      case '\0':
        s += '\0';
        break;

      case 'x':
        if ((p.size() >= 3) && isxdigit(p[1]) && isxdigit(p[2])) {
          s += (ascii_to_hex[static_cast<unsigned char>(p[1])] << 4) |
               ascii_to_hex[static_cast<unsigned char>(p[2])];

          p.remove_prefix(2);  // loop already removes one
          break;
        }

        throw Parser_error("Invalid \\xXX hex escape");
        break;

      case 'u':
        if ((p.size() >= 5) && isxdigit(p[1]) && isxdigit(p[2]) &&
            isxdigit(p[3]) && isxdigit(p[4])) {
          uint32_t unich = (ascii_to_hex[static_cast<unsigned>(p[1])] << 12) |
                           (ascii_to_hex[static_cast<unsigned>(p[2])] << 8) |
                           (ascii_to_hex[static_cast<unsigned>(p[3])] << 4) |
                           ascii_to_hex[static_cast<unsigned>(p[4])];
          unicode_codepoint_to_utf8(unich, &s);

          p.remove_prefix(4);  // loop already removes one
          break;
        }

        throw Parser_error("Invalid \\uXXXX unicode escape");
        break;

      default:
        throw Parser_error(
            str_format("Unknown escape sequence \\%c", cur_char));
    }
  }

  return {std::move(s), pc};
}

[[nodiscard]] std::tuple<Value, std::string_view> parse_map(
    std::string_view pc) {
  Value::Map_type_ref map = std::make_shared<Value::Map_type>();

  // Skips the opening {
  assert(pc.front() == '{');
  pc.remove_prefix(1);

  bool done = false;
  while (!done) {
    pc = skip_whitespace(pc);

    if (pc.empty())
      throw Parser_error("Error parsing map, unexpected end of stream.");

    auto cur_char = pc.front();
    if (cur_char == '}') {
      pc.remove_prefix(1);
      break;
    }

    if (cur_char != '"' && cur_char != '\'')
      throw Parser_error(
          "Error parsing map, unexpected character reading key.");

    std::string key;
    std::tie(key, pc) = parse_string(pc, cur_char);

    pc = skip_whitespace(pc);

    if (pc.empty() || (pc.front() != ':'))
      throw Parser_error("Error parsing map, unexpected item value separator.");

    // skips the :
    pc.remove_prefix(1);

    pc = skip_whitespace(pc);

    {
      Value value;
      std::tie(value, pc) = parse_main(pc);

      (*map)[key] = std::move(value);
    }

    pc = skip_whitespace(pc);
    if (pc.empty())
      throw Parser_error("Error parsing map, unexpected end of stream.");

    switch (pc.front()) {
      case '}':
        done = true;
        [[fallthrough]];
      case ',':
        pc.remove_prefix(1);  // skips either the '}' or the ','
        break;
      default:
        throw Parser_error("Error parsing map, unexpected item separator.");
    }

    pc = skip_whitespace(pc);
  }

  return {Value(map), pc};
}

[[nodiscard]] std::tuple<Value, std::string_view> parse_array(
    std::string_view pc) {
  Value::Array_type_ref array = std::make_shared<Value::Array_type>();

  // Skips the opening [
  assert(pc.front() == '[');
  pc.remove_prefix(1);

  pc = skip_whitespace(pc);

  bool done = false;
  while (!done) {
    pc = skip_whitespace(pc);
    if (pc.empty())
      throw Parser_error("Error parsing array, unexpected end of stream.");

    if (pc.front() != ']') {
      Value value;
      std::tie(value, pc) = parse_main(pc);

      array->push_back(std::move(value));
    }

    pc = skip_whitespace(pc);
    if (pc.empty())
      throw Parser_error("Error parsing array, unexpected end of stream.");

    switch (pc.front()) {
      case ']':
        done = true;
        pc.remove_prefix(1);  // skips the ]
        break;
      case ',':
        pc.remove_prefix(1);
        break;
      default:
        throw Parser_error("Error parsing array, unexpected value separator.");
    }

    pc = skip_whitespace(pc);
  }

  return {Value(array), pc};
}

[[nodiscard]] std::tuple<Value, std::string_view> parse_single_quoted_string(
    std::string_view pc) {
  auto [value, new_pc] = parse_string(pc, '\'');
  return {Value(std::move(value)), new_pc};
}

[[nodiscard]] std::tuple<Value, std::string_view> parse_double_quoted_string(
    std::string_view pc) {
  auto [value, new_pc] = parse_string(pc, '"');
  return {Value(std::move(value)), new_pc};
}

[[nodiscard]] std::tuple<Value, std::string_view> parse_number(
    std::string_view pcc) {
  assert(!pcc.empty());

  // Parsing based on the following state table
  // Valid integer: state == 1
  // Valid float: state == 2,4,6
  //                                0  1  2  3  4  5  6
  //                                +  #  .  #  e  +  d
  constexpr unsigned char STATE_VALIDITY[] = {0, 1, 0, 1, 0, 0, 1};
  enum States {
    FRONT_SIGN,
    INT_DIGITS,
    DOT,
    FLOAT_DIGITS,
    EXP,
    EXP_SIGN,
    EXP_DIGITS
  };

  // State starts at 1
  int state = INT_DIGITS;

  // Sign can appear at the beginning
  auto pc = pcc;
  switch (pc.front()) {
    case '-':
    case '+':
      pc.remove_prefix(1);
      state = FRONT_SIGN;
      break;
  }

  auto consume_digits = [&pc, &state](auto target_state) {
    if (!pc.empty() && IS_DIGIT(pc.front())) {
      state = target_state;
      pc.remove_prefix(1);
    }
    while (!pc.empty() && IS_DIGIT(pc.front())) pc.remove_prefix(1);
  };

  // Continues while there are digits
  consume_digits(INT_DIGITS);

  if (STATE_VALIDITY[state] == 1 && !pc.empty() && (pc.front() == '.')) {
    state = DOT;

    // Skips the .
    pc.remove_prefix(1);

    // Continues while there are digits
    consume_digits(FLOAT_DIGITS);
  }

  // exponential
  if (STATE_VALIDITY[state] == 1 && !pc.empty() &&
      (tolower(pc.front()) == 'e')) {
    state = EXP;

    // Skips the e
    pc.remove_prefix(1);

    // Sign can appear for exponential numbers
    switch (pc.empty() ? '\0' : pc.front()) {
      case '-':
      case '+':
        pc.remove_prefix(1);
        state = EXP_SIGN;
        break;
    }

    // Continues while there are digits
    consume_digits(FLOAT_DIGITS);
  }

  auto number = pcc.substr(0, pcc.size() - pc.size());

  if (STATE_VALIDITY[state] != 1)
    throw Parser_error(str_format("Error parsing number from: '%.*s'",
                                  static_cast<int>(number.length()),
                                  number.data()));

  Value ret_val;
  if (state == INT_DIGITS) {
    int64_t ll = 0;
    try {
      ll = std::atoll(std::string{number}.c_str());
    } catch (const std::invalid_argument &e) {
      throw Parser_error(str_format("Error parsing int: %s", e.what()));
    }
    ret_val = Value(static_cast<int64_t>(ll));
  } else {
    double d = 0;
    try {
      d = std::stod(std::string{number}.c_str());
    } catch (const std::invalid_argument &e) {
      throw Parser_error(str_format("Error parsing float: %s", e.what()));
    }

    ret_val = Value(d);
  }

  return {ret_val, pc};
}

std::tuple<Value, std::string_view> parse_main(std::string_view pc) {
  assert(!pc.empty());

  auto start = pc.front();
  switch (start) {
    case '{':
      return parse_map(pc);
    case '[':
      return parse_array(pc);
    case '"':
      return parse_double_quoted_string(pc);
    case '\'':
      return parse_single_quoted_string(pc);
    default:
      break;
  }

  if (IS_DIGIT(start) || (start == '-') || (start == '+'))  // a number
    return parse_number(pc);

  // a constant between true, false, null
  auto token = pc;
  while (!pc.empty() && IS_ALPHA(pc.front())) pc.remove_prefix(1);

  token = token.substr(0, token.size() - pc.size());

  if (str_caseeq(token, "undefined")) return {Value(), pc};
  if (str_caseeq(token, "true")) return {Value(true), pc};
  if (str_caseeq(token, "null")) return {Value::Null(), pc};
  if (str_caseeq(token, "false")) return {Value(false), pc};

  throw Parser_error(str_format(
      "Can't parse '%.*s'", static_cast<int>(token.length()), token.data()));
}
}  // namespace

std::string type_name(Value_type type) {
  switch (type) {
    case Undefined:
      return "Undefined";
    case Null:
      return "Null";
    case Bool:
      return "Bool";
    case Integer:
      return "Integer";
    case UInteger:
      return "UInteger";
    case Float:
      return "Float";
    case String:
      return "String";
    // case Object:
    //   return "Object";
    case Array:
      return "Array";
    case Map:
      return "Map";
    // case Function:
    //   return "Function";
    case Binary:
      return "Binary";
    default:
      return "";
  }
}

std::string type_description(Value_type type) {
  switch (type) {
    case Undefined:
      return "an undefined";
    case Null:
      return "a null";
    case Bool:
      return "a bool";
    case Integer:
      return "an integer";
    case UInteger:
      return "an unsigned integer";
    case Float:
      return "a float";
    case String:
      return "a string";
    // case Object:
    //   return "an object";
    case Array:
      return "an array";
    case Map:
      return "a map";
    // case Function:
    //   return "a function";
    case Binary:
      return "a binary string";
    default:
      return "";
  }
}

// --

Value_type Value::Map_type::get_type(const std::string &k) const {
  const_iterator iter = find(k);
  if (iter == end()) return Undefined;
  return iter->second.get_type();
}

std::string Value::Map_type::get_string(const std::string &k,
                                        const std::string &def) const {
  const_iterator iter = find(k);
  if (iter == end()) return def;
  iter->second.check_type(String);
  return iter->second.get_string();
}

bool Value::Map_type::get_bool(const std::string &k, bool def) const {
  const_iterator iter = find(k);
  if (iter == end()) return def;
  iter->second.check_type(Bool);
  return iter->second.as_bool();
}

int64_t Value::Map_type::get_int(const std::string &k, int64_t def) const {
  const_iterator iter = find(k);
  if (iter == end()) return def;
  iter->second.check_type(Integer);
  return iter->second.as_int();
}

uint64_t Value::Map_type::get_uint(const std::string &k, uint64_t def) const {
  const_iterator iter = find(k);
  if (iter == end()) return def;
  iter->second.check_type(UInteger);
  return iter->second.as_uint();
}

double Value::Map_type::get_double(const std::string &k, double def) const {
  const_iterator iter = find(k);
  if (iter == end()) return def;
  iter->second.check_type(Float);
  return iter->second.as_double();
}

// std::shared_ptr<Value::Map_type> Value::Map_type::get_map(
//     const std::string &k, std::shared_ptr<Map_type> def) const {
//   const_iterator iter = find(k);
//   if (iter == end()) return def;
//   iter->second.check_type(Map);
//   return iter->second.as_map();
// }

// std::shared_ptr<Value::Array_type> Value::Map_type::get_array(
//     const std::string &k, std::shared_ptr<Array_type> def) const {
//   const_iterator iter = find(k);
//   if (iter == end()) return def;
//   iter->second.check_type(Array);
//   return iter->second.as_array();
// }

void Value::Map_type::merge_contents(std::shared_ptr<Map_type> source,
                                     bool overwrite) {
  Value::Map_type::const_iterator iter;
  for (iter = source->begin(); iter != source->end(); ++iter) {
    std::string k = iter->first;
    Value v = iter->second;

    if (!overwrite && this->has_key(k)) continue;

    (*this)[k] = v;
  }
}

Value::Value(const std::string &s, bool binary) {
  if (binary)
    m_value = binary_string{s};
  else
    m_value = s;
}

Value::Value(std::string &&s, bool binary) {
  if (binary)
    m_value = binary_string{std::move(s)};
  else
    m_value = std::move(s);
}

Value::Value(const char *s) {
  if (s)
    m_value = std::string{s};
  else
    m_value = null_value{};
}

Value::Value(const char *s, size_t n, bool binary) {
  if (!s)
    m_value = null_value{};
  else if (binary)
    m_value = binary_string{std::string{s, n}};
  else
    m_value = std::string{s, n};
}

Value::Value(std::string_view s, bool binary) {
  if (binary)
    m_value = binary_string{std::string{s}};
  else
    m_value = std::string{s};
}

Value::Value(std::wstring_view s) : Value(wide_to_utf8(s.data(), s.length())) {}

Value::Value(std::nullptr_t) { m_value = null_value{}; }

Value::Value(int i) { m_value = static_cast<int64_t>(i); }

Value::Value(unsigned int ui) { m_value = static_cast<uint64_t>(ui); }

Value::Value(int64_t i) { m_value = i; }

Value::Value(uint64_t ui) { m_value = ui; }

Value::Value(float f) {
  // direct typecast from float to double works by just appending 0s to the
  // binary IEEE representation, which will result in a different number
  // So we convert through decimal instead
  m_value = std::stod(ftoa(f));
}

Value::Value(double d) { m_value = d; }

Value::Value(bool b) { m_value = b; }

// Value::Value(const std::shared_ptr<Function_base> &f) {
//   if (f)
//     m_value = f;
//   else
//     m_value = null_value{};
// }

// Value::Value(std::shared_ptr<Function_base> &&f) {
//   if (f)
//     m_value = std::move(f);
//   else
//     m_value = null_value{};
// }

// Value::Value(const std::shared_ptr<Object_bridge> &n) {
//   if (n)
//     m_value = n;
//   else
//     m_value = null_value{};
// }

Value::Value(const std::shared_ptr<polyglot::Polyglot_object> &n) {
  if (n)
    m_value = n;
  else
    m_value = null_value{};
}

Value::Value(const std::shared_ptr<polyglot::Object_bridge> &n) {
  if (n)
    m_value = n;
  else
    m_value = null_value{};
}

// Value::Value(std::shared_ptr<Object_bridge> &&n) {
//   if (n)
//     m_value = std::move(n);
//   else
//     m_value = null_value{};
// }

Value::Value(const Map_type_ref &n) {
  if (n)
    m_value = n;
  else
    m_value = null_value{};
}

Value::Value(Map_type_ref &&n) {
  if (n)
    m_value = std::move(n);
  else
    m_value = null_value{};
}

Value::Value(const Array_type_ref &n) {
  if (n)
    m_value = n;
  else
    m_value = null_value{};
}

Value::Value(Array_type_ref &&n) {
  if (n)
    m_value = std::move(n);
  else
    m_value = null_value{};
}

Value Value::parse(std::string_view s) {
  // ignore any white-space at the beginning of string
  auto pc = skip_whitespace(s);
  if (pc.empty()) throw Parser_error("Unable to parse empty string.");

  Value value;
  std::tie(value, pc) = parse_main(pc);

  if (!pc.empty()) {
    // ensure any leftover chars are just whitespaces
    pc = skip_whitespace(pc);
    if (!pc.empty())
      throw Parser_error(str_format(
          "Unexpected characters left at the end of document: ...%.*s",
          static_cast<int>(pc.length()), pc.data()));
  }

  return value;
}

bool Value::operator==(const Value &other) const {
  if (m_value.index() == other.m_value.index()) {
    return std::visit(
        [&other](auto &&value) {
          using T = std::decay_t<decltype(value)>;

          if constexpr (std::is_same_v<T, std::monostate> ||
                        std::is_same_v<T, null_value>)
            return true;

          else if constexpr (std::is_same_v<T, bool> ||
                             std::is_same_v<T, std::string> ||
                             std::is_same_v<T, binary_string> ||
                             std::is_same_v<T, int64_t> ||
                             std::is_same_v<T, uint64_t> ||
                             std::is_same_v<T, double>)
            return (value == std::get<T>(other.m_value));

          else if constexpr (/*std::is_same_v<T,
                                            std::shared_ptr<Object_bridge>> ||*/
                             std::is_same_v<T, std::shared_ptr<Array_type>> ||
                             std::is_same_v<T, std::shared_ptr<Map_type>>/* ||
                             std::is_same_v<T, std::shared_ptr<Function_base>>*/)
            return (*value == *std::get<T>(other.m_value));

          else
            return false;
        },
        m_value);
  }

  // with type conversion
  return std::visit(
      [this, &other](auto &&value) {
        using T = std::decay_t<decltype(value)>;

        if constexpr (std::is_same_v<T, bool>) {
          if (std::holds_alternative<int64_t>(other.m_value)) {
            if (std::get<int64_t>(other.m_value) == 1) return value;
            if (std::get<int64_t>(other.m_value) == 0) return !value;
            return false;
          }

          if (std::holds_alternative<uint64_t>(other.m_value)) {
            if (std::get<uint64_t>(other.m_value) == 1) return value;
            if (std::get<uint64_t>(other.m_value) == 0) return !value;
            return false;
          }

          if (std::holds_alternative<double>(other.m_value)) {
            if (std::get<double>(other.m_value) == 1.0) return value;
            if (std::get<double>(other.m_value) == 0.0) return !value;
            return false;
          }

          return false;

        } else if constexpr (std::is_same_v<T, int64_t>) {
          if (std::holds_alternative<bool>(other.m_value))
            return other.operator==(*this);

          if (std::holds_alternative<double>(other.m_value)) {
            auto val = std::get<double>(other.m_value);
            auto val_as_int = static_cast<int64_t>(val);
            return ((value == val_as_int) && ((val - val_as_int) == 0.0));
          }

          return false;

        } else if constexpr (std::is_same_v<T, uint64_t>) {
          if (std::holds_alternative<bool>(other.m_value))
            return other.operator==(*this);

          if (std::holds_alternative<double>(other.m_value)) {
            auto val = std::get<double>(other.m_value);
            auto val_as_int = static_cast<uint64_t>(val);
            return ((value == val_as_int) && ((val - val_as_int) == 0.0));
          }

          return false;

        } else if constexpr (std::is_same_v<T, double>) {
          if (std::holds_alternative<bool>(other.m_value) ||
              std::holds_alternative<int64_t>(other.m_value) ||
              std::holds_alternative<uint64_t>(other.m_value))
            return other.operator==(*this);

          return false;

        } else {
          return false;
        }
      },
      m_value);
}

std::string Value::json(bool pprint) const {
  JSON_dumper dumper(pprint);

  dumper.append_value(*this);

  return dumper.str();
}

std::string Value::descr(bool pprint) const {
  std::string s;
  append_descr(s, pprint ? 0 : -1, false);  // top level strings are not quoted
  return s;
}

std::string Value::repr() const {
  std::string s;
  append_repr(s);
  return s;
}

std::string &Value::append_descr(std::string &s_out, int indent,
                                 char quote_strings) const {
  std::visit(
      [this, &s_out, indent, quote_strings](auto &&value) {
        using T = std::decay_t<decltype(value)>;

        [[maybe_unused]] std::string_view nl = (indent >= 0) ? "\n" : "";

        if constexpr (std::is_same_v<T, std::monostate>) {
          s_out.append("undefined");
        } else if constexpr (std::is_same_v<T, null_value>) {
          s_out.append("null");
        } else if constexpr (std::is_same_v<T, bool>) {
          s_out.append(value ? "true" : "false");
        } else if constexpr (std::is_same_v<T, std::string>) {
          if (quote_strings)
            s_out.append(shcore::quote_string(value, quote_strings));
          else
            s_out.append(value);
        } else if constexpr (std::is_same_v<T, binary_string>) {
          s_out.append(string_to_hex(value));
        } else if constexpr (std::is_same_v<T, int64_t>) {
          s_out.append(lexical_cast<std::string>(value));
        } else if constexpr (std::is_same_v<T, uint64_t>) {
          s_out.append(lexical_cast<std::string>(value));
        } else if constexpr (std::is_same_v<T, double>) {
          s_out.append(dtoa(value));
          // } else if constexpr (std::is_same_v<T,
          //                                     std::shared_ptr<Object_bridge>>)
          //                                     {
          //   if (!value)
          //     throw Exception::value_error("Invalid object value
          //     encountered");

          //   as_object()->append_descr(s_out, indent, quote_strings);
        } else if constexpr (std::is_same_v<
                                 T,
                                 std::shared_ptr<polyglot::Polyglot_object>>) {
          if (!value)
            throw std::runtime_error("Invalid object value encountered");
          s_out.append("<" + as_object()->class_name() + ">");
        } else if constexpr (std::is_same_v<T, std::shared_ptr<Array_type>>) {
          if (!value)
            throw std::runtime_error("Invalid array value encountered");

          s_out.append("[");

          auto myend = value->end(), mybegin = value->begin();
          for (auto iter = mybegin; iter != myend; ++iter) {
            if (iter != mybegin) s_out.append(", ");
            s_out.append(nl);
            if (indent >= 0) s_out.append((indent + 1) * 4, ' ');
            iter->append_descr(s_out, indent < 0 ? indent : indent + 1, '"');
          }

          if (!value->empty()) {
            s_out.append(nl);
            if (indent > 0) s_out.append(indent * 4, ' ');
          }

          s_out.append("]");

        } else if constexpr (std::is_same_v<T, std::shared_ptr<Map_type>>) {
          if (!value) throw std::runtime_error("Invalid map value encountered");

          s_out.append("{");

          if (!value->empty()) s_out.append(nl);

          auto myend = value->end(), mybegin = value->begin();
          for (auto iter = mybegin; iter != myend; ++iter) {
            if (iter != mybegin) s_out.append(", ").append(nl);
            if (indent >= 0) s_out.append((indent + 1) * 4, ' ');
            s_out.append(quote_string(iter->first, '"')).append(": ");
            iter->second.append_descr(s_out, indent < 0 ? indent : indent + 1,
                                      '"');
          }

          if (!value->empty()) {
            s_out.append(nl);
            if (indent > 0) s_out.append(indent * 4, ' ');
          }

          s_out.append("}");

          // } else if constexpr (std::is_same_v<T,
          //                                     std::shared_ptr<Function_base>>)
          //                                     {
          //   value->append_descr(&s_out, indent, quote_strings);
        }
      },
      m_value);

  return s_out;
}

std::string &Value::append_repr(std::string &s_out) const {
  std::visit(
      [&s_out](auto &&value) {
        using T = std::decay_t<decltype(value)>;

        if constexpr (std::is_same_v<T, std::monostate>) {
          s_out.append("undefined");
        } else if constexpr (std::is_same_v<T, null_value>) {
          s_out.append("null");
        } else if constexpr (std::is_same_v<T, bool>) {
          s_out.append(value ? "true" : "false");
        } else if constexpr (std::is_same_v<T, std::string>) {
          s_out.append("\"");
          for (const unsigned char c : value) {
            // non-printable ascii char
            if (!(0x20 <= c && c < 0x7f)) {
              s_out.append("\\x");
              s_out.append(1, "0123456789abcdef"[c >> 4]);
              s_out.append(1, "0123456789abcdef"[c & 0xf]);
              continue;
            }

            switch (c) {
              case '\n':
                s_out.append("\\n");
                break;
              case '"':
                s_out.append("\\\"");
                break;
              case '\'':
                s_out.append("\\\'");
                break;
              case '\a':
                s_out.append("\\a");
                break;
              case '\b':
                s_out.append("\\b");
                break;
              case '\f':
                s_out.append("\\f");
                break;
              case '\r':
                s_out.append("\\r");
                break;
              case '\t':
                s_out.append("\\t");
                break;
              case '\v':
                s_out.append("\\v");
                break;
              case '\\':
                s_out.append("\\\\");
                break;
              default:
                s_out.append(1, c);
            }
          }
          s_out.append("\"");
        } else if constexpr (std::is_same_v<T, binary_string>) {
          s_out.append(string_to_hex(value));
        } else if constexpr (std::is_same_v<T, int64_t>) {
          s_out.append(lexical_cast<std::string>(value));
        } else if constexpr (std::is_same_v<T, uint64_t>) {
          s_out.append(lexical_cast<std::string>(value));
        } else if constexpr (std::is_same_v<T, double>) {
          s_out.append(str_format("%g", value));
          // } else if constexpr (std::is_same_v<T,
          //                                     std::shared_ptr<Object_bridge>>)
          //                                     {
          //   value->append_repr(s_out);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<Array_type>>) {
          s_out.append("[");

          auto myend = value->end(), mybegin = value->begin();
          for (auto iter = mybegin; iter != myend; ++iter) {
            if (iter != mybegin) s_out.append(", ");
            iter->append_repr(s_out);
          }
          s_out.append("]");
        } else if constexpr (std::is_same_v<T, std::shared_ptr<Map_type>>) {
          s_out.append("{");

          auto myend = value->end(), mybegin = value->begin();
          for (auto iter = mybegin; iter != myend; ++iter) {
            if (iter != mybegin) s_out.append(", ");
            Value(iter->first).append_repr(s_out);
            s_out.append(": ");
            iter->second.append_repr(s_out);
          }
          s_out.append("}");
          // } else if constexpr (std::is_same_v<T,
          //                                     std::shared_ptr<Function_base>>)
          //                                     {
          //   value->append_repr(&s_out);
        }
      },
      m_value);

  return s_out;
}

bool is_compatible_type(Value_type source_type, Value_type target_type) {
  return kTypeConvertible[source_type][target_type];
}

class Type_error : public std::runtime_error {
 public:
  Type_error(const std::string &error) : std::runtime_error(error.c_str()) {}
};

inline Type_error type_conversion_error(Value_type from, Value_type expected) {
  return Type_error("Invalid typecast: " + type_name(expected) +
                    " expected, but value is " + type_name(from));
}

inline Type_error type_range_error(Value_type from, Value_type expected) {
  return Type_error("Invalid typecast: " + type_name(expected) +
                    " expected, but " + type_name(from) +
                    " value is out of range");
}

void Value::check_type(Value_type t) const {
  if (auto type = get_type(); !is_compatible_type(type, t)) {
    throw type_conversion_error(type, t);
  }
}

Value_type Value::get_type() const noexcept {
  return std::visit(
      [](auto &&value) -> Value_type {
        using T = std::decay_t<decltype(value)>;

        if constexpr (std::is_same_v<T, std::monostate>) {
          return Value_type::Undefined;
        } else if constexpr (std::is_same_v<T, null_value>) {
          return Value_type::Null;
        } else if constexpr (std::is_same_v<T, bool>) {
          return Value_type::Bool;
        } else if constexpr (std::is_same_v<T, std::string>) {
          return Value_type::String;
        } else if constexpr (std::is_same_v<T, binary_string>) {
          return Value_type::Binary;
        } else if constexpr (std::is_same_v<T, int64_t>) {
          return Value_type::Integer;
        } else if constexpr (std::is_same_v<T, uint64_t>) {
          return Value_type::UInteger;
        } else if constexpr (std::is_same_v<T, double>) {
          return Value_type::Float;
        } else if constexpr (std::is_same_v<
                                 T,
                                 std::shared_ptr<polyglot::Polyglot_object>>) {
          return Value_type::Object;
        } else if constexpr (std::is_same_v<
                                 T, std::shared_ptr<polyglot::Object_bridge>>) {
          return Value_type::ObjectBridge;
        } else if constexpr (std::is_same_v<T, std::shared_ptr<Array_type>>) {
          return Value_type::Array;
        } else if constexpr (std::is_same_v<T, std::shared_ptr<Map_type>>) {
          return Value_type::Map;
          // } else if constexpr (std::is_same_v<T,
          //                                     std::shared_ptr<Function_base>>)
          //                                     {
          //   return Value_type::Function;
        } else {
          assert(!"Unknown value type");
          throw std::logic_error("Unknown value type");
        }
      },
      m_value);
}

bool Value::as_bool() const {
  return std::visit(
      [this](auto &&value) -> bool {
        using T = std::decay_t<decltype(value)>;

        if constexpr (std::is_same_v<T, bool>) {
          return value;
        } else if constexpr (std::is_same_v<T, std::string>) {
          try {
            return lexical_cast<bool>(value);
          } catch (...) {
            throw type_conversion_error(get_type(), Bool);
          }
        } else if constexpr (std::is_same_v<T, int64_t>) {
          return (value != 0);
        } else if constexpr (std::is_same_v<T, uint64_t>) {
          return (value != 0);
        } else if constexpr (std::is_same_v<T, double>) {
          return (value != 0.0);
        } else {
          throw type_conversion_error(get_type(), Bool);
        }
      },
      m_value);
}

int64_t Value::as_int() const {
  return std::visit(
      [this](auto &&value) -> int64_t {
        using T = std::decay_t<decltype(value)>;

        if constexpr (std::is_same_v<T, bool>) {
          return value ? 1 : 0;
        } else if constexpr (std::is_same_v<T, std::string>) {
          try {
            return lexical_cast<int64_t>(value);
          } catch (...) {
            throw type_conversion_error(get_type(), Integer);
          }
        } else if constexpr (std::is_same_v<T, int64_t>) {
          return value;
        } else if constexpr (std::is_same_v<T, uint64_t>) {
          if (value <= (uint64_t)std::numeric_limits<int64_t>::max())
            return static_cast<int64_t>(value);
          throw type_range_error(get_type(), Integer);
        } else if constexpr (std::is_same_v<T, double>) {
          double integral;
          if (std::modf(value, &integral) == 0.0 &&
              value >= -(1LL << DBL_MANT_DIG) && value <= (1LL << DBL_MANT_DIG))
            return static_cast<int64_t>(value);
          throw type_range_error(get_type(), Integer);
        } else {
          throw type_conversion_error(get_type(), Integer);
        }
      },
      m_value);
}

uint64_t Value::as_uint() const {
  return std::visit(
      [this](auto &&value) -> int64_t {
        using T = std::decay_t<decltype(value)>;

        if constexpr (std::is_same_v<T, bool>) {
          return value ? 1 : 0;
        } else if constexpr (std::is_same_v<T, std::string>) {
          try {
            return lexical_cast<uint64_t>(value);
          } catch (...) {
            throw type_conversion_error(get_type(), UInteger);
          }
        } else if constexpr (std::is_same_v<T, int64_t>) {
          if (value >= 0) return static_cast<uint64_t>(value);
          throw type_range_error(get_type(), UInteger);

        } else if constexpr (std::is_same_v<T, uint64_t>) {
          return value;
        } else if constexpr (std::is_same_v<T, double>) {
          double integral;
          if (std::modf(value, &integral) == 0.0 && value >= 0.0 &&
              value <= (1LL << DBL_MANT_DIG))
            return static_cast<uint64_t>(value);
          throw type_range_error(get_type(), UInteger);
        } else {
          throw type_conversion_error(get_type(), UInteger);
        }
      },
      m_value);
}

double Value::as_double() const {
  // DBL_MANT_DIG is the number of bits used to represent the number itself,
  // without the exponent or sign

  return std::visit(
      [this](auto &&value) -> double {
        using T = std::decay_t<decltype(value)>;

        if constexpr (std::is_same_v<T, bool>) {
          return value ? 1.0 : 0.0;
        } else if constexpr (std::is_same_v<T, std::string>) {
          try {
            return lexical_cast<double>(value);
          } catch (...) {
            throw type_conversion_error(get_type(), Float);
          }
        } else if constexpr (std::is_same_v<T, int64_t>) {
          if (value <= (1LL << DBL_MANT_DIG) && value >= -(1LL << DBL_MANT_DIG))
            return static_cast<double>(value);

          // higher or lower values lose precision
          throw type_range_error(get_type(), Float);
        } else if constexpr (std::is_same_v<T, uint64_t>) {
          if (value <= (1ULL << DBL_MANT_DIG))
            return static_cast<double>(value);

          // higher values lose precision
          throw type_range_error(get_type(), Float);
        } else if constexpr (std::is_same_v<T, double>) {
          return value;
        } else {
          throw type_conversion_error(get_type(), Float);
        }
      },
      m_value);
}

std::string Value::as_string() const {
  return std::visit(
      [this](auto &&value) -> std::string {
        using T = std::decay_t<decltype(value)>;

        if constexpr (std::is_same_v<T, bool>) {
          return lexical_cast<std::string>(value);
        } else if constexpr (std::is_same_v<T, std::string> ||
                             std::is_same_v<T, binary_string>) {
          return value;
        } else if constexpr (std::is_same_v<T, int64_t>) {
          return lexical_cast<std::string>(value);
        } else if constexpr (std::is_same_v<T, uint64_t>) {
          return lexical_cast<std::string>(value);
        } else if constexpr (std::is_same_v<T, double>) {
          return lexical_cast<std::string>(value);
        } else {
          throw type_conversion_error(get_type(), String);
        }
      },
      m_value);
}

std::wstring Value::as_wstring() const { return utf8_to_wide(as_string()); }

std::shared_ptr<polyglot::Object_bridge> Value::as_object_bridge() const {
  check_type(ObjectBridge);

  if (is_null()) return nullptr;
  return std::get<std::shared_ptr<polyglot::Object_bridge>>(m_value);
}

std::shared_ptr<polyglot::Polyglot_object> Value::as_object() const {
  check_type(Object);

  if (is_null()) return nullptr;
  return std::get<std::shared_ptr<polyglot::Polyglot_object>>(m_value);
}

// std::string Value::yaml() const { return "---\n" + yaml(0) + "\n"; }

// std::string Value::yaml(int init_indent) const {
//   // implementation based on: http://yaml.org/spec/1.2/spec.html
//   constexpr size_t k_indent_size = 2;

//   const auto new_line = [indent_size = k_indent_size](std::string *s,
//                                                       int indent) {
//     s->append(1, '\n');
//     s->append(indent_size * indent, ' ');
//   };

//   const auto map2yaml = [&](const Dictionary_t &m, int indent) {
//     if (!m) {
//       // null -> empty string
//       return std::string{};
//     }

//     std::string map;
//     bool first_item = true;

//     for (const auto &kv : *m) {
//       if (first_item) {
//         first_item = false;
//       } else {
//         new_line(&map, indent);
//       }

//       map += kv.first + ":";
//       const auto sub_yaml = kv.second.yaml(indent + 1);

//       // if value is a map or an array it needs to be in a separate line
//       if (auto type = kv.second.get_type();
//           type == Value_type::Map || type == Value_type::Array) {
//         new_line(&map, indent + 1);
//       } else {
//         if (!sub_yaml.empty()) {
//           map += " ";
//         }
//       }

//       map += sub_yaml;
//     }

//     return map;
//   };

//   const auto string2yaml = [indent_size = k_indent_size](
//                                std::string_view s, int indent) -> std::string
//                                {
//     if (s.empty()) {
//       // empty string must be explicitly quoted
//       return "''";
//     }

//     auto content = split_string(s, "\n");

//     if (content.size() == 1) {
//       // use either plain or quoted style
//       bool quote = false;

//       {
//         // plain string cannot contain leading or trailing white-space
//         // characters
//         constexpr std::string_view k_whitespace{" \t"};
//         if (k_whitespace.find(s[0]) != std::string_view::npos ||
//             k_whitespace.find(s.back()) != std::string_view::npos) {
//           quote = true;
//         }
//       }

//       if (!quote) {
//         // plain string cannot start with an indicator
//         if (std::string_view{"-?:,[]{}#&*!|>'\"%@`"}.find(s[0]) !=
//             std::string_view::npos) {
//           if (s.length() > 1 &&
//               std::string_view{"-?:"}.find(s[0]) != std::string_view::npos &&
//               s[1] != ' ') {
//             // unless it's '-', '?' or ':' and it's not followed by a space
//             quote = false;
//           } else {
//             quote = true;
//           }
//         }
//       }

//       if (!quote) {
//         // plain scalars must never contain the ": " and " #" character
//         // combinations
//         if (s.find(": ") != std::string_view::npos ||
//             s.find(" #") != std::string_view::npos) {
//           quote = true;
//         }
//       }

//       if (quote) return "'" + str_replace(s, "'", "''") + "'";
//       return std::string{s};

//     } else {
//       // by default set chomping to 'strip'
//       char chomping = '-';

//       if (content.back().empty()) {
//         // if the last line is an empty string, it means that the input
//         string
//         // ends with a new line character and the extra line is not needed
//         content.pop_back();
//         // chomping needs to be set to 'clip', so the final line break is
//         // included
//         chomping = 0;
//       }

//       if (content.back().empty()) {
//         // if the last line is still an empty string, set chomping to 'keep',
//         // so multiple empty lines are preserved
//         chomping = '+';
//       }

//       // use literal block style
//       std::string literal = "|";
//       // increase the indent for contents
//       ++indent;

//       if (content[0][0] == ' ' || content[0][0] == '\t') {
//         // if the first line starts with a white-space, we need to explicitly
//         // specify the indent
//         literal += std::to_string(indent_size * indent);
//       }

//       // add chomping indicator if not 'clip'
//       if (chomping) {
//         literal += chomping;
//       }

//       literal += "\n";

//       for (const auto &line : content) {
//         literal.append(indent_size * indent, ' ');
//         literal.append(line);
//         literal.append(1, '\n');
//       }

//       // remove the last new line character
//       literal.pop_back();

//       return literal;
//     }
//   };

//   return std::visit(
//       [&](auto &&value) -> std::string {
//         using T = std::decay_t<decltype(value)>;

//         if constexpr (std::is_same_v<T, std::monostate> ||
//                       std::is_same_v<T, null_value>) {
//           // YAML has no notion of these values, they are represented by
//           empty
//           // strings
//           return "";
//         } else if constexpr (std::is_same_v<T, bool> ||
//                              std::is_same_v<T, int64_t> ||
//                              std::is_same_v<T, uint64_t> ||
//                              std::is_same_v<T, double> ||
//                             //  std::is_same_v<T,
//                             // std::shared_ptr<Object_bridge>> ||
//                              std::is_same_v<T,
//                                             std::shared_ptr<Function_base>>)
//                                             {
//           // we treat these as scalars
//           return string2yaml(descr(), init_indent);

//         } else if constexpr (std::is_same_v<T, std::string>) {
//           return string2yaml(value, init_indent);

//         } else if constexpr (std::is_same_v<T, binary_string>) {
//           // TODO(rennox): implement binary
//           return string2yaml(value, init_indent);

//         } else if constexpr (std::is_same_v<T, std::shared_ptr<Array_type>>)
//         {
//           std::string array;
//           bool first_item = true;

//           for (const auto &v : *value) {
//             if (first_item) {
//               first_item = false;
//             } else {
//               new_line(&array, init_indent);
//             }

//             array += "-";
//             const auto sub_yaml = v.yaml(init_indent + 1);

//             if (!sub_yaml.empty()) {
//               array += " " + sub_yaml;
//             }
//           }

//           return array;

//         } else if constexpr (std::is_same_v<T, std::shared_ptr<Map_type>>) {
//           return map2yaml(value, init_indent);
//         } else {
//           throw std::logic_error(str_format(
//               "Type '%s' was not handled.", type_name(get_type()).c_str()));
//         }
//       },
//       m_value);
// }

//---

// const std::string &Argument_list::string_at(unsigned int i) const {
//   if (i >= size())
//     throw Exception::argument_error("Insufficient number of arguments");

//   switch (at(i).get_type()) {
//     case String:
//       return at(i).get_string();
//     default:
//       throw Exception::type_error(
//           str_format("Argument #%u is expected to be a string", (i + 1)));
//   }
// }

// bool Argument_list::bool_at(unsigned int i) const {
//   if (i >= size())
//     throw Exception::argument_error("Insufficient number of arguments");

//   try {
//     return at(i).as_bool();
//   } catch (...) {
//     throw Exception::type_error(
//         str_format("Argument #%u is expected to be a bool", (i + 1)));
//   }
// }

// int64_t Argument_list::int_at(unsigned int i) const {
//   if (i >= size())
//     throw Exception::argument_error("Insufficient number of arguments");

//   try {
//     return at(i).as_int();
//   } catch (...) {
//     throw Exception::type_error(
//         str_format("Argument #%u is expected to be an int", (i + 1)));
//   }
// }

// uint64_t Argument_list::uint_at(unsigned int i) const {
//   if (i >= size())
//     throw Exception::argument_error("Insufficient number of arguments");

//   try {
//     return at(i).as_uint();
//   } catch (...) {
//     throw Exception::type_error(
//         str_format("Argument #%u is expected to be an unsigned int", (i +
//         1)));
//   }
// }

// double Argument_list::double_at(unsigned int i) const {
//   if (i >= size())
//     throw Exception::argument_error("Insufficient number of arguments");

//   try {
//     return at(i).as_double();
//   } catch (...) {
//     throw Exception::type_error(
//         str_format("Argument #%u is expected to be a double", (i + 1)));
//   }
// }

// std::shared_ptr<Object_bridge> Argument_list::object_at(unsigned int i) const
// {
//   if (i >= size())
//     throw Exception::argument_error("Insufficient number of arguments");

//   if (at(i).get_type() == Object) return at(i).as_object();
//   throw Exception::type_error(
//       str_format("Argument #%u is expected to be an object", (i + 1)));
// }

// std::shared_ptr<Value::Map_type> Argument_list::map_at(unsigned int i) const
// {
//   if (i >= size())
//     throw Exception::argument_error("Insufficient number of arguments");

//   if (at(i).get_type() == Map) return at(i).as_map();
//   throw Exception::type_error(
//       str_format("Argument #%u is expected to be a map", (i + 1)));
// }

// std::shared_ptr<Value::Array_type> Argument_list::array_at(
//     unsigned int i) const {
//   if (i >= size())
//     throw Exception::argument_error("Insufficient number of arguments");

//   if (at(i).get_type() == Array) return at(i).as_array();
//   throw Exception::type_error(
//       str_format("Argument #%u is expected to be an array", (i + 1)));
// }

Dictionary_t make_dict() { return std::make_shared<Value::Map_type>(); }

}  // namespace shcore
