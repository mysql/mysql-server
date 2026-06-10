/*
   Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef STORAGE_NDB_PLUGIN_NDB_STRING_SPLIT_H_
#define STORAGE_NDB_PLUGIN_NDB_STRING_SPLIT_H_

#include <iterator>
#include <string_view>

namespace ndbcluster {

/**
 * @brief Range-based splitter for use in range-for loops.
 *
 * Produces tokens as std::string_view without allocations. Includes empty
 * tokens for consecutive, leading, and trailing delimiters.
 *
 * Semantics:
 * - Empty input produces exactly one empty token (matches standard split-like
 * behavior).
 */
class SplitRange {
 public:
  class iterator {
   public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = std::string_view;
    using difference_type = std::ptrdiff_t;
    using pointer = const std::string_view *;
    using reference = const std::string_view &;

    // Default-constructed iterator models end()
    constexpr iterator() noexcept = default;

    // Begin iterator
    constexpr iterator(std::string_view str, char delimiter) noexcept
        : m_input_string(str), m_delimiter(delimiter), m_start(0) {
      m_pos = m_input_string.find(m_delimiter, m_start);
    }

    constexpr std::string_view operator*() const noexcept {
      if (m_start == npos) return {};
      if (m_pos == npos) {
        // last element in range, use rest of string
        return m_input_string.substr(m_start);
      }
      return m_input_string.substr(m_start, m_pos - m_start);
    }

    constexpr iterator &operator++() noexcept {
      if (m_start == npos) return *this;
      if (m_pos == npos) {
        // no more tokens
        m_start = npos;
        return *this;
      }
      m_start = m_pos + 1;
      m_pos = m_input_string.find(m_delimiter, m_start);
      // If start == size() (trailing delimiter), we will return "" once,
      // then next ++ will set end on the subsequent call when m_pos == npos
      // above.
      return *this;
    }

    constexpr iterator operator++(int) noexcept {
      iterator tmp = *this;
      ++(*this);
      return tmp;
    }

    constexpr bool operator==(const iterator &other) const noexcept {
      // All "end" iterators compare equal regardless of source
      if (m_start == npos && other.m_start == npos) return true;
      return m_input_string.data() == other.m_input_string.data() &&
             m_input_string.size() == other.m_input_string.size() &&
             m_start == other.m_start && m_delimiter == other.m_delimiter;
    }

    constexpr bool operator!=(const iterator &other) const noexcept {
      return !(*this == other);
    }

   private:
    // Sentinel equal to std::string_view::npos
    static constexpr auto npos = std::string_view::npos;

    const std::string_view m_input_string{};
    const char m_delimiter{','};
    // start offset of current token; npos means end()
    size_t m_start{npos};
    // position of next delimiter; npos means last token
    size_t m_pos{npos};
  };

  /**
   * @brief Construct a range-based splitter.
   *
   * @param sv Input string view to split.
   * @param delimiter Delimiter character (default ',').
   */
  constexpr SplitRange(std::string_view sv, char delimiter = ',') noexcept
      : m_input_string(sv), m_delimiter(delimiter) {}

  constexpr iterator begin() const noexcept {
    return iterator(m_input_string, m_delimiter);
  }
  constexpr iterator end() const noexcept { return iterator(); }

 private:
  const std::string_view m_input_string;
  const char m_delimiter;
};

/**
 * @brief Helper to construct a SplitRange from a string_view.
 *
 * @param sv Input string view to split.
 * @param delimiter Delimiter character (default ',').
 * @return SplitRange value suitable for range-for iteration.
 */
inline constexpr auto split_range(std::string_view sv,
                                  char delimiter = ',') noexcept {
  return SplitRange{sv, delimiter};
}

}  // namespace ndbcluster

#endif  // STORAGE_NDB_PLUGIN_NDB_STRING_SPLIT_H_
