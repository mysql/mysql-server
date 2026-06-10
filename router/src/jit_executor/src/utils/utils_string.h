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

#ifndef MYSQLSHDK_LIBS_UTILS_UTILS_STRING_H_
#define MYSQLSHDK_LIBS_UTILS_UTILS_STRING_H_

#include <wctype.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "mysqlrouter/jit_executor_plugin_export.h"

namespace shcore {

void clear_buffer(char *buffer, size_t size);
void clear_buffer(std::string &buffer);

namespace internal {

template <typename Char, typename F>
inline std::basic_string<Char> transform(std::basic_string_view<Char> s,
                                         F fun) {
  std::basic_string<Char> r(s);
  std::transform(r.begin(), r.end(), r.begin(), fun);
  return r;
}

}  // namespace internal

/** Convert a copy of an ASCII string to uppercase and return */
inline std::string str_upper(std::string_view s) {
  return internal::transform(s, ::toupper);
}

inline std::wstring str_upper(std::wstring_view s) {
  return internal::transform(s, ::towupper);
}

/** Convert a copy of an ASCII string to lowercase and return */
inline std::string str_lower(std::string_view s) {
  return internal::transform(s, ::tolower);
}

inline std::wstring str_lower(std::wstring_view s) {
  return internal::transform(s, ::towlower);
}

/** Compares 2 strings case insensitive (for ascii) */
inline int str_casecmp(const char *a, const char *b) {
#ifdef _WIN32
  return ::_stricmp(a, b);
#else
  return ::strcasecmp(a, b);
#endif
}

inline int str_casecmp(const wchar_t *a, const wchar_t *b) {
#ifdef _WIN32
  return ::_wcsicmp(a, b);
#else
  return ::wcscasecmp(a, b);
#endif
}

inline int str_casecmp(const char *a, const char *b, size_t n) {
#ifdef _WIN32
  return ::_strnicmp(a, b, n);
#else
  return ::strncasecmp(a, b, n);
#endif
}

inline int str_casecmp(const wchar_t *a, const wchar_t *b, size_t n) {
#ifdef _WIN32
  return ::_wcsnicmp(a, b, n);
#else
  return ::wcsncasecmp(a, b, n);
#endif
}

struct Case_sensitive_comparator {
  bool operator()(const std::string &a, const std::string &b) const {
    return a.compare(b) < 0;
  }

  bool operator()(const std::wstring &a, const std::wstring &b) const {
    return a.compare(b) < 0;
  }
};

struct Case_insensitive_comparator {
  bool operator()(const std::string &a, const std::string &b) const {
    return str_casecmp(a.c_str(), b.c_str()) < 0;
  }

  bool operator()(const std::wstring &a, const std::wstring &b) const {
    return str_casecmp(a.c_str(), b.c_str()) < 0;
  }
};

struct Case_comparator {
  bool case_sensitive{true};
  Case_comparator(bool p_case_sensitive) : case_sensitive{p_case_sensitive} {}

  bool operator()(const std::string &a, const std::string &b) const {
    return case_sensitive ? Case_sensitive_comparator()(a, b)
                          : Case_insensitive_comparator()(a, b);
  }

  bool operator()(const std::wstring &a, const std::wstring &b) const {
    return case_sensitive ? Case_sensitive_comparator()(a, b)
                          : Case_insensitive_comparator()(a, b);
  }
};

struct Lexicographical_comparator {
  bool operator()(const std::string &a, const std::string &b) const {
    return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end());
  }

  bool operator()(const std::wstring &a, const std::wstring &b) const {
    return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end());
  }
};

namespace internal {

template <typename Char>
inline bool str_caseeq_pair(std::basic_string_view<Char> a,
                            std::basic_string_view<Char> b) {
  if (a.length() != b.length()) return false;
  return str_casecmp(a.data(), b.data(), a.length()) == 0;
}

template <typename Char, typename... T>
inline bool str_caseeq(std::basic_string_view<Char> a, T &&...tokens) {
  return (str_caseeq_pair<Char>(a, std::forward<T>(tokens)) || ...);
}

}  // namespace internal

template <typename... T>
inline bool str_caseeq(std::string_view a, std::string_view token,
                       T &&...tokens) {
  return internal::str_caseeq(a, token, std::forward<T>(tokens)...);
}

template <typename... T>
inline bool str_caseeq(std::wstring_view a, std::wstring_view token,
                       T &&...tokens) {
  return internal::str_caseeq(a, token, std::forward<T>(tokens)...);
}

/** Checks whether a string has another as a prefix */
namespace internal {

template <typename Char>
inline bool str_beginswith_pair(std::basic_string_view<Char> s,
                                std::basic_string_view<Char> prefix) {
  return s.compare(0, prefix.length(), prefix) == 0;
}

template <typename Char, typename... T>
inline bool str_beginswith(std::basic_string_view<Char> s, T &&...prefixes) {
  return (str_beginswith_pair<Char>(s, std::forward<T>(prefixes)) || ...);
}

}  // namespace internal

template <typename... T>
inline bool str_beginswith(std::string_view s, std::string_view prefix,
                           T &&...prefixes) {
  return internal::str_beginswith(s, prefix, std::forward<T>(prefixes)...);
}

template <typename... T>
inline bool str_beginswith(std::wstring_view s, std::wstring_view prefix,
                           T &&...prefixes) {
  return internal::str_beginswith(s, prefix, std::forward<T>(prefixes)...);
}

namespace internal {

template <typename Char>
inline bool str_ibeginswith_pair(std::basic_string_view<Char> s,
                                 std::basic_string_view<Char> prefix) {
  if (s.length() < prefix.length()) return false;
  return str_casecmp(s.data(), prefix.data(), prefix.length()) == 0;
}

template <typename Char, typename... T>
inline bool str_ibeginswith(std::basic_string_view<Char> s, T &&...prefixes) {
  return (str_ibeginswith_pair<Char>(s, std::forward<T>(prefixes)) || ...);
}

}  // namespace internal

template <typename... T>
inline bool str_ibeginswith(std::string_view s, std::string_view prefix,
                            T &&...prefixes) {
  return internal::str_ibeginswith(s, prefix, std::forward<T>(prefixes)...);
}

template <typename... T>
inline bool str_ibeginswith(std::wstring_view s, std::wstring_view prefix,
                            T &&...prefixes) {
  return internal::str_ibeginswith(s, prefix, std::forward<T>(prefixes)...);
}

/** Checks whether a string has another as a suffix */
namespace internal {

template <typename Char>
inline bool str_endswith_pair(std::basic_string_view<Char> s,
                              std::basic_string_view<Char> suffix) {
  if (suffix.length() > s.length()) return false;
  return s.compare(s.length() - suffix.length(), suffix.length(), suffix) == 0;
}

template <typename Char, typename... T>
inline bool str_endswith(std::basic_string_view<Char> s, T &&...suffixes) {
  return (str_endswith_pair<Char>(s, std::forward<T>(suffixes)) || ...);
}

}  // namespace internal

template <typename... T>
inline bool str_endswith(std::string_view s, std::string_view suffix,
                         T &&...suffixes) {
  return internal::str_endswith(s, suffix, std::forward<T>(suffixes)...);
}

template <typename... T>
inline bool str_endswith(std::wstring_view s, std::wstring_view suffix,
                         T &&...suffixes) {
  return internal::str_endswith(s, suffix, std::forward<T>(suffixes)...);
}

namespace internal {

template <typename Char>
inline bool str_iendswith_pair(std::basic_string_view<Char> s,
                               std::basic_string_view<Char> suffix) {
  if (suffix.length() > s.length()) return false;
  return str_casecmp(s.data() + s.length() - suffix.length(), suffix.data(),
                     suffix.length()) == 0;
}

template <typename Char, typename... T>
inline bool str_iendswith(std::basic_string_view<Char> s, T &&...suffixes) {
  return (str_iendswith_pair<Char>(s, std::forward<T>(suffixes)) || ...);
}

}  // namespace internal

template <typename... T>
inline bool str_iendswith(std::string_view s, std::string_view suffix,
                          T &&...suffixes) {
  return internal::str_iendswith(s, suffix, std::forward<T>(suffixes)...);
}

template <typename... T>
inline bool str_iendswith(std::wstring_view s, std::wstring_view suffix,
                          T &&...suffixes) {
  return internal::str_iendswith(s, suffix, std::forward<T>(suffixes)...);
}

const char *str_casestr(const char *haystack, const char *needle);

/** Return position of the first difference in the strings or npos if they're
 * the same */
inline size_t str_span(const std::string &s1, const std::string &s2) {
  size_t p = 0;
  while (p < s1.size() && p < s2.size()) {
    if (s1[p] != s2[p]) return p;
    ++p;
  }
  if (p == s1.size() && p == s2.size()) return std::string::npos;
  return p;
}

/** Partition a string in 2 at a separator, if present */
template <class TOutput = std::string>
inline std::pair<TOutput, TOutput> str_partition(std::string_view s,
                                                 std::string_view sep,
                                                 bool *found_sep = nullptr) {
  static_assert(std::is_same_v<TOutput, std::string> ||
                    std::is_same_v<TOutput, std::string_view>,
                "Type must either be std::string or std::string_view");

  auto p = s.find(sep);

  if (found_sep) {
    *found_sep = p != std::string_view::npos;
  }

  if (p == std::string_view::npos)
    return std::make_pair(TOutput{s}, TOutput{""});

  return std::make_pair(TOutput{s.substr(0, p)},
                        TOutput{s.substr(p + sep.length())});
}

/** Partition a string in 2 after separator, in place, if present */
template <class TOutput = std::string>
inline std::pair<TOutput, TOutput> str_partition_after(std::string_view s,
                                                       std::string_view sep) {
  static_assert(std::is_same_v<TOutput, std::string> ||
                    std::is_same_v<TOutput, std::string_view>,
                "Type must either be std::string or std::string_view");

  auto p = s.find(sep);
  if (p == std::string_view::npos) {
    return std::make_pair(TOutput{s}, TOutput{""});
  }

  p += sep.length();
  return std::make_pair(TOutput{s.substr(0, p)}, TOutput{s.substr(p)});
}

/** Partition a string in 2 after separator, in place, if present */
inline std::string str_partition_after_inpl(std::string *s,
                                            const std::string &sep) {
  auto p = s->find(sep);
  if (p == std::string::npos) {
    std::string tmp = *s;
    s->clear();
    return tmp;
  } else {
    std::string tmp = s->substr(0, p + sep.length());
    s->erase(0, p + sep.length());
    return tmp;
  }
}

/**
 * Splits string based on each of the individual characters of the separator
 * string
 *
 * @param input The string to be split
 * @param separator_chars String containing characters wherein the input string
 *   is split on any of the characters
 * @param maxsplit max number of times to split or -1 for no limit
 * @param compress Boolean value which when true ensures consecutive separators
 * do not generate new elements in the split, but they're still counted towards
 * maxsplit.
 *
 * @returns vector of splitted strings
 */
inline std::vector<std::string> str_split(
    std::string_view input, std::string_view separator_chars = " \r\n\t",
    int maxsplit = -1, bool compress = false) {
  std::vector<std::string> ret_val;
  size_t index = 0, new_find = 0;
  const size_t end = input.size();

  while (new_find < end) {
    if (maxsplit--)
      new_find = input.find_first_of(separator_chars, index);
    else
      new_find = std::string_view::npos;

    if (new_find != std::string_view::npos) {
      // When compress is enabled, consecutive separators
      // do not generate new elements
      if (new_find > index || !compress || new_find == 0)
        ret_val.push_back(std::string{input.substr(index, new_find - index)});

      index = new_find + 1;
    } else {
      ret_val.push_back(std::string{input.substr(index)});
    }
  }
  return ret_val;
}

/**
 * Split the given input string and call the functor for each token.
 *
 * @param input the string to be split
 * @param f a functor called once for each split string. If its return value
 *   is false, the splitting is interrupted.
 * @param separator_chars String containing characters wherein the input string
 *   is split on any of the characters
 * @param maxsplit max number of times to split or -1 for no limit
 * @param compress Boolean value which when true ensures consecutive separators
 *   do not generate new elements in the split, but they're still counted
 * towards maxsplit.
 *
 * @returns false if f returns false, true otherwise
 */
template <class TCallback>
inline bool str_itersplit(std::string_view input, TCallback &&f,
                          std::string_view separator_chars = " \r\n\t",
                          int maxsplit = -1, bool compress = false) {
  static_assert(std::is_invocable_r_v<bool, TCallback, std::string_view>);

  size_t index = 0, new_find = 0;
  const auto end = input.size();

  while (new_find < end) {
    if (maxsplit--)
      new_find = input.find_first_of(separator_chars, index);
    else
      new_find = std::string_view::npos;

    if (new_find != std::string_view::npos) {
      // When compress is enabled, consecutive separators
      // do not generate new elements
      if (new_find > index || !compress || new_find == 0)
        if (!f(input.substr(index, new_find - index))) return false;

      index = new_find + 1;
    } else {
      if (!f(input.substr(index))) return false;
    }
  }
  return true;
}

/** Strip a string out of blank chars */
std::string_view str_strip_view(std::string_view s,
                                std::string_view chars = " \r\n\t");
std::string_view str_lstrip_view(std::string_view s,
                                 std::string_view chars = " \r\n\t");
std::string_view str_rstrip_view(std::string_view s,
                                 std::string_view chars = " \r\n\t");

std::string str_strip(std::string_view s, std::string_view chars = " \r\n\t");
std::string str_lstrip(std::string_view s, std::string_view chars = " \r\n\t");
std::string str_rstrip(std::string_view s, std::string_view chars = " \r\n\t");

inline std::string str_ljust(const std::string &s, size_t width,
                             char pad = ' ') {
  if (s.size() < width) return s + std::string(width - s.size(), pad);
  return s;
}

inline std::string str_rjust(const std::string &s, size_t width,
                             char pad = ' ') {
  if (s.size() < width) return std::string(width - s.size(), pad).append(s);
  return s;
}

/** Return a formatted a string

  Throws invalid_argument on encoding error
*/
#if defined(__GNUC__) && (__GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ > 4))
std::string str_format(const char *formats, ...)
    __attribute__((__format__(__printf__, 1, 2)));
#elif _MSC_VER
std::string str_format(_In_z_ _Printf_format_string_ const char *format, ...);
#else
std::string str_format(const char *formats, ...);
#endif

template <typename Iter>
inline std::string str_join(Iter begin, Iter end, std::string_view sep) {
  if (begin == end) return {};

  std::string s;

  s.append(*begin);
  while (++begin != end) {
    s.append(sep);
    s.append(*begin);
  }

  return s;
}

template <typename Iter, typename CTransform>
inline std::string str_join(Iter begin, Iter end, std::string_view sep,
                            CTransform &&f) {
  if (begin == end) return {};

  std::string s;

  s.append(f(*begin));
  while (++begin != end) {
    s.append(sep);
    s.append(f(*begin));
  }

  return s;
}

template <typename C>
inline std::string str_join(const C &container, std::string_view sep) {
  return str_join(container.begin(), container.end(), sep);
}

template <typename C, typename CTransform>
inline std::string str_join(const C &container, std::string_view sep,
                            CTransform &&f) {
  return str_join(container.begin(), container.end(), sep,
                  std::forward<CTransform>(f));
}

std::string JIT_EXECUTOR_PLUGIN_EXPORT str_replace(std::string_view s,
                                                   std::string_view from,
                                                   std::string_view to);

template <typename... Args>
inline std::string JIT_EXECUTOR_PLUGIN_EXPORT str_replace(std::string_view s,
                                                          std::string_view from,
                                                          std::string_view to,
                                                          const Args &...rest) {
  return str_replace(str_replace(s, from, to), rest...);
}

std::string bits_to_string(uint64_t bits, int nbits);
std::pair<uint64_t, int> string_to_bits(std::string_view s);

std::string bits_to_string_hex(uint64_t bits, int nbits);
size_t bits_to_string_hex_size(int nbits);

std::string string_to_hex(std::string_view s, bool prefix = true);

/**
 * Escape `quote` and `\` chars.
 *
 * @param s String to escape.
 * @param quote `'` `"`
 * @return Quote escaped string.
 */
std::string quote_string(const std::string &s, char quote);

/**
 * Inverse of quote_string().
 *
 * If the first and the last characters in the given strings match `quote`
 * they are removed as well.
 *
 * @param s String to be processed.
 * @param quote The quote character (`'`, `"`)
 *
 * @return Unquoted string.
 */
std::string unquote_string(std::string_view s, char quote);

// Macro to turn a symbol into a string
#define STRINGIFY(s) STRINGIFY_(s)
#define STRINGIFY_(s) #s

/** Breaks string into lines of specified width without breaking words.
 *
 * @param line long string to break.
 * @param line_width maximum line width
 * @return vector with split lines.
 */
std::vector<std::string> str_break_into_lines(const std::string &line,
                                              std::size_t line_width);

/**
 * Auxiliary function to get the quotes span (i.e., start and end positions)
 * for the given string.
 *
 * If not quote is found then std::string::npos is returned for both elements
 * in the pair. If only one quote is found (no ending quote) then
 * std::string::npos is returned for the second position of the pair.
 *
 * @param quote_char character with the quote to look for.
 * @param str target string to get the start and end quote position.
 * @return return a pair with the position of the starting quote and ending
 *         quote.
 */
std::pair<std::string::size_type, std::string::size_type> get_quote_span(
    const char quote_char, const std::string &str);

/**
 * Convert UTF-8 string to UTF-16/UTF-32 (platform dependent) string.
 *
 * @param utf8 UTF-8 encoded string.
 * @return std::wstring UTF-16/UTF-32 (platform dependent) string.
 */
std::wstring utf8_to_wide(const std::string &utf8);

/**
 * Convert UTF-8 string to UTF-16/UTF-32 (platform dependent) string.
 *
 * @param utf8 Pointer to UTF-8 encoded string.
 * @param utf8_length Length of UTF-8 string in bytes.
 * @return std::wstring UTF-16/UTF-32 (platform dependent) string.
 */
std::wstring utf8_to_wide(const char *utf8, const size_t utf8_length);

/**
 * Convert UTF-8 string to UTF-16/UTF-32 (platform dependent) string.
 *
 * @param utf8 Pointer to UTF-8 encoded string.
 * @return std::wstring UTF-16/UTF-32 (platform dependent) string.
 */
std::wstring utf8_to_wide(const char *utf8);

/**
 * Convert UTF-16/UTF-32 (platform dependent) string to UTF-8 string.
 *
 * @param wide UTF-16/UTF-32 (platform dependent) encoded string.
 * @return std::string UTF-8 encoded string.
 */
std::string wide_to_utf8(const std::wstring &wide);

/**
 * Convert UTF-16/UTF-32 (platform dependent) string to UTF-8 string.
 *
 * @param wide Pointer to UTF-16/UTF-32 (platform dependent) encoded string.
 * @param wide_length Length of UTF-16/UTF-32 string in bytes.
 * @return std::string UTF-8 encoded string.
 */
std::string wide_to_utf8(const wchar_t *wide, const size_t wide_length);

/**
 * Convert UTF-16/UTF-32 (platform dependent) string to UTF-8 string.
 *
 * @param wide Pointer to UTF-16/UTF-32 (platform dependent) encoded string.
 * @return std::string UTF-8 encoded string.
 */
std::string wide_to_utf8(const wchar_t *wide);

/**
 * Truncates the given string to max_length code points.
 *
 * @param str UTF-8 string to be truncated.
 * @param max_length Maximum number of code points.
 *
 * @return Input string truncated to max_length code points.
 */
std::string truncate(const std::string &str, const size_t max_length);

/**
 * Truncates the given string to max_length code points.
 *
 * @param str UTF-8 string to be truncated.
 * @param length Length of string in bytes.
 * @param max_length Maximum number of code points.
 *
 * @return Input string truncated to max_length code points.
 */
std::string truncate(const char *str, const size_t length,
                     const size_t max_length);

/**
 * Truncates the given string to max_length code points.
 *
 * @param str UTF-16/UTF-32 string to be truncated.
 * @param max_length Maximum number of code points.
 *
 * @return Input string truncated to max_length code points.
 */
std::wstring truncate(const std::wstring &str, const size_t max_length);

/**
 * Truncates the given string to max_length code points.
 *
 * @param str UTF-16/UTF-32 string to be truncated.
 * @param length Length of string in bytes.
 * @param max_length Maximum number of code points.
 *
 * @return Input string truncated to max_length code points.
 */
std::wstring truncate(const wchar_t *str, const size_t length,
                      const size_t max_length);

/**
 * Checks if the given string contains only valid UTF-8 code points.
 *
 * @param s String to be checked.
 *
 * @returns true if the given string is a valid UTF-8 string
 */
bool is_valid_utf8(std::string_view s);

/**
 * Generates a percent encoded string based on RFC-3986, only unreserved
 * characters are not encoded.
 */
std::string pctencode(std::string_view s);

/**
 * Decodes a string that is percent encoded based on RFC-3986.
 */
std::string pctdecode(std::string_view s);

/**
 * Returns a string of the given size created with random characters from the
 * provided source.
 */
std::string get_random_string(size_t size, const char *source);

/**
 * Transparent hashing function, enables heterogeneous lookup in unordered
 * containers.
 */
struct string_hash {
  using is_transparent = void;

  [[nodiscard]] size_t operator()(const char *txt) const {
    return std::hash<std::string_view>{}(txt);
  }

  [[nodiscard]] size_t operator()(std::string_view txt) const {
    return std::hash<std::string_view>{}(txt);
  }

  [[nodiscard]] size_t operator()(const std::string &txt) const {
    return std::hash<std::string>{}(txt);
  }
};

template <typename Key, typename T>
using heterogeneous_map =
    std::unordered_map<Key, T, string_hash, std::equal_to<>>;

template <typename Key>
using heterogeneous_set = std::unordered_set<Key, string_hash, std::equal_to<>>;

inline std::string utf8_upper(std::string_view s) {
  return wide_to_utf8(str_upper(utf8_to_wide(s.data(), s.length())));
}

inline std::string utf8_lower(std::string_view s) {
  return wide_to_utf8(str_lower(utf8_to_wide(s.data(), s.length())));
}

}  // namespace shcore

#endif  // MYSQLSHDK_LIBS_UTILS_UTILS_STRING_H_
