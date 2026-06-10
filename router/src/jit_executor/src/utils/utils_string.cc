/*
 * Copyright (c) 2017, 2026, Oracle and/or its affiliates.
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

// #include "utils/utils_string.h"
#include "utils/utils_string.h"

#include <array>
#include <bitset>
#include <cassert>
#include <cstdarg>
#include <functional>
#include <random>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
// Starting with Windows 8: MultiByteToWideChar is declared in Stringapiset.h.
// Before Windows 8, it was declared in Winnls.h.
#include <Stringapiset.h>
#include <shlwapi.h>
#else
#include <cwchar>
#endif

// #include "include/mysh_config.h"

namespace shcore {

void clear_buffer(char *buffer, size_t size) {
#ifdef _WIN32
  SecureZeroMemory(buffer, size);
#else
#if defined HAVE_EXPLICIT_BZERO
  explicit_bzero(buffer, size);
#elif defined HAVE_MEMSET_S
  memset_s(buffer, size, '\0', size);
#else
  volatile char *p = buffer;
  while (size--) {
    *p++ = '\0';
  }
#endif
#endif
}

void clear_buffer(std::string &buffer) {
  clear_buffer(buffer.data(), buffer.size());
  buffer.clear();
}

std::string_view str_strip_view(std::string_view s, std::string_view chars) {
  auto begin = s.find_first_not_of(chars);
  if (begin == std::string_view::npos) return {};

  auto end = s.find_last_not_of(chars);
  return s.substr(begin, end - begin + 1);
}

std::string_view str_lstrip_view(std::string_view s, std::string_view chars) {
  auto begin = s.find_first_not_of(chars);
  if (begin == std::string_view::npos) return {};
  return s.substr(begin);
}

std::string_view str_rstrip_view(std::string_view s, std::string_view chars) {
  auto end = s.find_last_not_of(chars);
  if (end == std::string_view::npos) return {};
  return s.substr(0, end + 1);
}

std::string str_strip(std::string_view s, std::string_view chars) {
  return std::string{str_strip_view(s, chars)};
}

std::string str_lstrip(std::string_view s, std::string_view chars) {
  return std::string{str_lstrip_view(s, chars)};
}

std::string str_rstrip(std::string_view s, std::string_view chars) {
  return std::string{str_rstrip_view(s, chars)};
}

std::string str_format(const char *formats, ...) {
  static const int kBufferSize = 256;
  std::string buffer;
  buffer.resize(kBufferSize);
  int len;
  va_list args;

#ifdef WIN32
  va_start(args, formats);
  len = _vscprintf(formats, args);
  va_end(args);
  if (len < 0) throw std::invalid_argument("Could not format string");
  buffer.resize(len + 1);
  va_start(args, formats);
  len = vsnprintf(&buffer[0], buffer.size(), formats, args);
  va_end(args);
  if (len < 0) throw std::invalid_argument("Could not format string");
  buffer.resize(len);
#else
  va_start(args, formats);
  len = vsnprintf(&buffer[0], buffer.size(), formats, args);
  va_end(args);
  if (len < 0) throw std::invalid_argument("Could not format string");
  if (len + 1 >= kBufferSize) {
    buffer.resize(len + 1);
    va_start(args, formats);
    len = vsnprintf(&buffer[0], buffer.size(), formats, args);
    va_end(args);
    if (len < 0) throw std::invalid_argument("Could not format string");
  }
  buffer.resize(len);
#endif

  return buffer;
}

std::string str_replace(std::string_view s, std::string_view from,
                        std::string_view to) {
  std::string str;

  if (from.empty()) {
    str.reserve(to.length() * (s.size() + 1));

    str.append(to);
    for (char c : s) {
      str.push_back(c);
      str.append(to);
    }
  } else {
    str.reserve(s.length());

    int offs = from.length();
    std::string::size_type start = 0, p = s.find(from);
    while (p != std::string::npos) {
      if (p > start) str.append(s, start, p - start);
      str.append(to);
      start = p + offs;
      p = s.find(from, start);
    }
    if (start < s.length()) str.append(s, start, s.length() - start);
  }
  return str;
}

std::string bits_to_string(uint64_t bits, int nbits) {
  auto r = std::bitset<64>{bits}.to_string();
  r.erase(0, r.size() - nbits);
  return r;
}

std::pair<uint64_t, int> string_to_bits(std::string_view s) {
  int nbits = s.size();
  if (nbits > 64)
    throw std::invalid_argument("bit string length must be <= 64");
  std::bitset<64> bits(s.data(), s.size());
  return {bits.to_ullong(), nbits};
}

std::string bits_to_string_hex(uint64_t bits, int nbits) {
  auto sbits = bits_to_string(bits, nbits);

  return str_format("0x%.*llX",
                    static_cast<int>(bits_to_string_hex_size(nbits)),
                    std::bitset<64>{sbits.data(), sbits.size()}.to_ullong());
}

size_t bits_to_string_hex_size(int nbits) {
  // 1 hex char per 4 bits and always in pairs (0xF -> 0x0F)
  auto num_chars = (nbits / 4) + ((nbits % 4) ? 1 : 0);
  num_chars += (num_chars % 2) ? 1 : 0;

  return num_chars;
}

std::string string_to_hex(std::string_view s, bool prefix) {
  constexpr std::array<char, 16> hexmap = {'0', '1', '2', '3', '4', '5',
                                           '6', '7', '8', '9', 'A', 'B',
                                           'C', 'D', 'E', 'F'};

  std::string encoded;

  encoded.reserve((prefix ? 2 : 0) + s.size() * 2);
  if (prefix) encoded.append("0x");

  for (const auto cur_char : s) {
    encoded.push_back(hexmap[(cur_char & 0xF0) >> 4]);
    encoded.push_back(hexmap[cur_char & 0x0F]);
  }

  return encoded;
}

std::string quote_string(const std::string &s, char quote) {
  const std::string q{quote};
  const std::string backslash = str_replace(s, "\\", "\\\\");
  const std::string esc = str_replace(backslash, q, "\\" + q);
  return std::string(q + esc + q);
}

std::string unquote_string(std::string_view s, char quote) {
  const std::string q{quote};
  auto result = std::string{s};

  if (result.length() >= 2 && result[0] == quote &&
      result[result.length() - 1] == quote) {
    result = result.substr(1, result.length() - 2);
  }

  result = str_replace(result, "\\" + q, q);
  result = str_replace(result, "\\\\", "\\");

  return result;
}

std::vector<std::string> str_break_into_lines(const std::string &line,
                                              std::size_t line_width) {
  std::vector<std::string> result;
  std::string rem(line);
  std::string::size_type nl_pos = std::string::npos;
  std::string::size_type split_point = std::string::npos;

  while (rem.length() > line_width ||
         (nl_pos = rem.find('\n')) != std::string::npos) {
    if (nl_pos != std::string::npos) {
      split_point = nl_pos;
    } else {
      split_point = line_width - 1;
      while (!std::isspace(rem[split_point]) && split_point > 0) --split_point;

      if (split_point == 0 && !std::isspace(rem[0])) {
        for (split_point = line_width; split_point < rem.length();
             split_point++)
          if (std::isspace(rem[split_point])) break;
        if (split_point == rem.length()) break;
      } else {
        for (int i = split_point - 1; i >= 0; --i)
          if (rem[i] == '\n') split_point = i;
      }
    }

    if (split_point == 0)
      result.push_back("");
    else
      result.push_back(rem.substr(0, split_point));
    rem = split_point + 1 >= rem.length() ? std::string()
                                          : rem.substr(split_point + 1);
  }
  if (!rem.empty()) result.push_back(rem);
  return result;
}

std::pair<std::string::size_type, std::string::size_type> get_quote_span(
    const char quote_char, const std::string &str) {
  bool escaped = false;

  // if string has less than 2 chars  we assume matching quotes were not found.
  if (str.size() < 2)
    return std::make_pair(std::string::npos, std::string::npos);

  std::string::size_type open_quote_pos = 0;

  // find opening quote char
  for (std::string::size_type i = 0, end = str.size();
       (str[i] != quote_char || escaped) && i < end; ++i) {
    open_quote_pos++;
    escaped = (str[i]) == '\\' && !escaped;
  }
  // if no quotes were found
  if (open_quote_pos == str.size())
    return std::make_pair(std::string::npos, std::string::npos);

  // find closing quote char
  std::string::size_type close_quote_pos = open_quote_pos + 1;
  for (std::string::size_type i = close_quote_pos, end = str.size();
       (str[i] != quote_char || escaped) && i < end; ++i) {
    close_quote_pos++;
    escaped = (str[i]) == '\\' && !escaped;
  }

  // closing quote was not found
  if (close_quote_pos == str.size()) close_quote_pos = std::string::npos;
  return std::make_pair(open_quote_pos, close_quote_pos);
}

std::string str_subvars(
    std::string_view s,
    const std::function<std::string(std::string_view)> &subvar,
    std::string_view var_begin, std::string_view var_end) {
  assert(!var_begin.empty());

  std::string out_s;
  out_s.reserve(s.size());

  std::string_view::size_type p0 = 0;
  while (p0 != std::string_view::npos) {
    auto pos = s.find(var_begin, p0);
    if (pos == std::string_view::npos) {
      out_s.append(s.substr(p0));
      break;
    }

    out_s.append(s.substr(p0, pos - p0));

    pos += var_begin.size();
    std::string_view::size_type p1;
    if (var_end.empty()) {
      constexpr std::string_view k_idchars{
          "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_01234567890"};
      p1 = s.find_first_not_of(k_idchars, pos);
    } else {
      p1 = s.find(var_end, pos);
      if (p1 == std::string_view::npos) return out_s.append(s.substr(pos));
    }
    if (p1 == std::string_view::npos) {
      out_s.append(subvar(s.substr(pos)));
    } else {
      out_s.append(subvar(s.substr(pos, p1 - pos)));
      p1 += var_end.size();
    }
    p0 = p1;
  }
  return out_s;
}

std::wstring utf8_to_wide(const std::string &utf8) {
  return utf8_to_wide(&utf8[0], utf8.size());
}

std::wstring utf8_to_wide(const char *utf8) {
  return utf8_to_wide(utf8, strlen(utf8));
}

std::string wide_to_utf8(const std::wstring &wide) {
  return wide_to_utf8(&wide[0], wide.size());
}

std::string wide_to_utf8(const wchar_t *wide) {
  return wide_to_utf8(wide, wcslen(wide));
}

#ifdef _WIN32

std::wstring utf8_to_wide(const char *utf8, const size_t utf8_length) {
  auto buffer_size_needed =
      MultiByteToWideChar(CP_UTF8, 0, utf8, utf8_length, nullptr, 0);
  std::wstring wide_string(buffer_size_needed, 0);
  const auto wide_string_size = MultiByteToWideChar(
      CP_UTF8, 0, utf8, utf8_length, &wide_string[0], buffer_size_needed);
  wide_string.resize(wide_string_size);
  return wide_string;
}

std::string wide_to_utf8(const wchar_t *wide, const size_t wide_length) {
  auto string_size = WideCharToMultiByte(CP_UTF8, 0, wide, wide_length, nullptr,
                                         0, nullptr, nullptr);
  std::string result_string(string_size, 0);
  WideCharToMultiByte(CP_UTF8, 0, wide, wide_length, &result_string[0],
                      string_size, nullptr, nullptr);
  return result_string;
}

#else

std::wstring utf8_to_wide(const char *utf8, const size_t utf8_length) {
  std::mbstate_t state{};
  const char *end = utf8 + utf8_length;
  int len = 0;
  std::wstring result;
  wchar_t wc;

  while ((len = std::mbrtowc(&wc, utf8, end - utf8, &state)) > 0) {
    result += wc;
    utf8 += len;
  }

  return result;
}

std::string wide_to_utf8(const wchar_t *wide, const size_t wide_length) {
  std::mbstate_t state{};
  std::string result;
  std::string mb(MB_CUR_MAX, '\0');

  for (size_t i = 0; i < wide_length; ++i) {
    const auto len = std::wcrtomb(&mb[0], wide[i], &state);
    result.append(mb.c_str(), len);
  }

  return result;
}

#endif

std::string truncate(const std::string &str, const size_t max_length) {
  return truncate(str.c_str(), str.length(), max_length);
}

std::string truncate(const char *str, const size_t length,
                     const size_t max_length) {
  return wide_to_utf8(truncate(utf8_to_wide(str, length), max_length));
}

std::wstring truncate(const std::wstring &str, const size_t max_length) {
  return truncate(str.c_str(), str.length(), max_length);
}

std::wstring truncate(const wchar_t *str, const size_t length,
                      const size_t max_length) {
#if (WCHAR_MAX + 0) <= 0xffff
  // UTF-16
  std::wstring truncated;
  std::size_t idx = 0;
  std::size_t truncated_length = 0;

  while (truncated_length < max_length && idx < length) {
    // detect high surrogate
    if (0xD800 == (str[idx] & 0xFC00)) {
      if (idx + 1 == length) {
        // no low surrogate, finish here
        break;
      }

      truncated.append(1, str[idx++]);
    }

    // low surrogate or a code point
    truncated.append(1, str[idx++]);
    ++truncated_length;
  }

  return truncated;
#else
  // UTF-32
  return std::wstring(str, std::min(length, max_length));
#endif
}

bool is_valid_utf8(std::string_view s) {
  auto c = reinterpret_cast<const unsigned char *>(s.data());
  const auto end = c + s.size();
  uint32_t cp = 0;
  size_t bytes = 0;

  while (c < end) {
    if (0x00 == (*c & 0x80)) {
      // 0xxxxxxx, U+0000 - U+007F
      bytes = 1;
      cp = *c & 0x7F;
    } else if (0xC0 == (*c & 0xE0)) {
      // 110xxxxx, U+0080 - U+07FF
      bytes = 2;
      cp = *c & 0x1F;
    } else if (0xE0 == (*c & 0xF0)) {
      // 1110xxxx, U+0800 - U+FFFF
      bytes = 3;
      cp = *c & 0x0F;
    } else if (0xF0 == (*c & 0xF8)) {
      // 11110xxx, U+10000 - U+10FFFF
      bytes = 4;
      cp = *c & 0x07;
    } else {
      return false;
    }

    // advance one byte
    ++c;

    for (size_t b = 1; b < bytes; ++b) {
      // each byte should be: 10xxxxxx
      if (0x80 != (*c & 0xC0)) {
        return false;
      }

      cp = (cp << 6) | (*c & 0x3F);

      // advance one byte
      ++c;
    }

    // invalid code points
    if ((cp <= 0x7F && 1 != bytes) ||  // overlong encoding
        (cp >= 0x80 && cp <= 0x07FF && 2 != bytes) ||
        (cp >= 0x0800 && cp <= 0xFFFF && 3 != bytes) ||
        (cp >= 0x10000 && cp <= 0x10FFFF && 4 != bytes) ||
        (cp >= 0xD800 && cp <= 0xDFFF) ||  // UTF-16 surrogate halves
        cp > 0x10FFFF) {                   // not encodable by UTF-16
      return false;
    }
  }

  return true;
}

namespace {

// Byte-values that are reserved and must be hex-encoded [0..255]
const int k_reserved_chars[] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

// Numeric values for hex-digits [0..127]
const int k_hex_values[] = {
    0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0,
    0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0,
    0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  2,  3,  4, 5, 6, 7, 8,
    9, 0, 0,  0,  0,  0,  0,  0,  10, 11, 12, 13, 14, 15, 0, 0, 0, 0, 0,
    0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0,
    0, 0, 10, 11, 12, 13, 14, 15, 0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0,
    0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0};

}  // namespace

std::string pctencode(std::string_view s) {
  constexpr std::array<char, 16> hexmap = {'0', '1', '2', '3', '4', '5',
                                           '6', '7', '8', '9', 'A', 'B',
                                           'C', 'D', 'E', 'F'};

  std::string enc;
  enc.reserve(s.size() * 3);

  for (const auto c : s) {
    if (k_reserved_chars[static_cast<unsigned char>(c)]) {
      enc.push_back('%');
      enc.push_back(hexmap[(c & 0xF0) >> 4]);
      enc.push_back(hexmap[c & 0x0F]);
    } else {
      enc.push_back(c);
    }
  }

  enc.shrink_to_fit();
  return enc;
}

std::string pctdecode(std::string_view s) {
  std::string dec;
  dec.reserve(s.size());

  for (size_t i = 0, c = s.size(); i < c;) {
    if (i <= c - 3 && s[i] == '%' && isxdigit(s[i + 1]) && isxdigit(s[i + 2])) {
      int ch = k_hex_values[static_cast<int>(s[i + 1])] << 4 |
               k_hex_values[static_cast<int>(s[i + 2])];
      dec.push_back(ch);
      i += 3;
    } else {
      dec.push_back(s[i]);
      ++i;
    }
  }

  dec.shrink_to_fit();
  return dec;
}

std::string get_random_string(size_t size, const char *source) {
  std::random_device rd;
  std::string data;
  data.reserve(size);

  std::uniform_int_distribution<int> dist_num(0, strlen(source) - 1);

  for (size_t i = 0; i < size; i++) {
    char random = source[dist_num(rd)];

    // Make sure there are no consecutive values
    if (i == 0) {
      data += random;
    } else {
      if (random != data[i - 1])
        data += random;
      else
        i--;
    }
  }

  return data;
}

const char *str_casestr(const char *haystack, const char *needle) {
#ifdef _WIN32
  return StrStrIA(haystack, needle);
#else
  return strcasestr(haystack, needle);
#endif
}

}  // namespace shcore
