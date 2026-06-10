// Copyright (c) 2025, 2026, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

#ifndef MYSQL_GTIDS_TAG_H
#define MYSQL_GTIDS_TAG_H

/// @file
/// Experimental API header

#include <array>                            // array
#include <cstdint>                          // size_t
#include <initializer_list>                 // initializer_list
#include <stdexcept>                        // domain_error
#include <string_view>                      // string_view
#include "mysql/ranges/buffer_interface.h"  // Buffer_interface
#include "mysql/strconv/strconv.h"          // Is_string_target
#include "mysql/utils/call_and_catch.h"     // call_and_catch
#include "mysql/utils/return_status.h"      // Return_status

/// @addtogroup GroupLibsMysqlGtids
/// @{

// ==== class Char_table ====

namespace mysql::gtids::detail {

/// Helper class to hold lookup tables indexed by ascii characters.
///
/// @tparam Transform_tp Function that transforms letters. It should accept a
/// char argument and return the type of values stored in the table.
///
/// @tparam char_ranges_tp An even number of char constants, each pair defining
/// the beginning and end of a range of characters included in the table. Both
/// beginning and end are *inclusive*.
template <std::invocable<unsigned char> Transform_tp,
          unsigned char... char_ranges_tp>
  requires(sizeof...(char_ranges_tp) % 2 == 0)
class Char_table {
 public:
  using Transform_t = Transform_tp;
  using Element_t = std::invoke_result_t<Transform_t, unsigned char>;
  using Table_t = std::array<Element_t, 256>;

  /// Return reference to the table.
  static Table_t &table() {
    // Compute the table on the first invocation of this function.
    static Table_t &tbl = build_table({char_ranges_tp...});
    return tbl;
  }

 private:
  /// Compute the table.
  static Table_t &build_table(
      std::initializer_list<unsigned char> char_ranges) {
    static Table_t tbl;
    // Initialize with zero bytes.
    tbl.fill({});
    // Fill with the specified ranges.
    for (const auto *it = char_ranges.begin(); it != char_ranges.end();
         it += 2) {
      auto first = *it;
      auto last = *std::next(it);
      for (unsigned char ch = first; ch <= last; ++ch)
        tbl[ch] = Transform_t{}(ch);
    }
    return tbl;
  }
};

}  // namespace mysql::gtids::detail

// ==== Base class and concept ====

namespace mysql::gtids::detail {
/// Top of the hierarchy, defining the tag format.
class Tag_base {
 public:
  /// The maximum number of characters a tag.
  static constexpr std::size_t max_size = 32;
  static constexpr std::ptrdiff_t max_ssize = 32;

 private:
  /// Lambda function that converts a character to lower case.
  using Tolower_t = decltype([](unsigned char ch) { return std::tolower(ch); });

  /// Table of characters allowed as the first letter in a tag.
  using First_char_table_t =
      Char_table<Tolower_t, 'a', 'z', 'A', 'Z', '_', '_'>;

  /// Table of characters allowed in tags at other positions than first.
  using Nonfirst_char_table_t =
      Char_table<Tolower_t, '0', '9', 'a', 'z', 'A', 'Z', '_', '_'>;

 public:
  /// Return the character converted to lowercase, if it is allowed as the first
  /// character in a tag; otherwise return 0.
  [[nodiscard]] static int get_normalized_first_char(int ch) {
    assert(ch >= 0);
    assert(ch < 256);
    return First_char_table_t::table()[ch];
  }

  /// Return true if the given character is allowed as the first character in a
  /// tag.
  [[nodiscard]] static bool is_valid_first_char(int ch) {
    return get_normalized_first_char(ch) != 0;
  }

  /// Return the character converted to lowercase, if it is allowed in a tag at
  /// other positions than the first; otherwise return 0.
  [[nodiscard]] static int get_normalized_nonfirst_char(int ch) {
    assert(ch >= 0);
    assert(ch < 256);
    return Nonfirst_char_table_t::table()[ch];
  }

  /// Return true if the given character is allowed in a tag, at other positions
  /// than the first.
  [[nodiscard]] static bool is_valid_nonfirst_char(int ch) {
    return get_normalized_nonfirst_char(ch) != 0;
  }

  /// Return true if the given string is a valid tag.
  ///
  /// To be a valid tag, it must be between 0 and max_size characters long, and
  /// if it is not empty, the first character must be 'a'-'z', 'A-Z', or '_',
  /// and any remaining characters must be either that or '0'-'9'.
  [[nodiscard]] static bool is_valid(const std::string_view &sv) {
    if (sv.empty()) return true;
    if (sv.size() > max_size) return false;
    if (!is_valid_first_char(sv[0])) return false;
    // Not on all platforms is `it` a pointer and can be declared as `auto *`
    // NOLINTNEXTLINE(llvm-qualified-auto,readability-qualified-auto)
    for (auto it = std::next(sv.begin()); it != sv.end(); ++it) {
      if (!is_valid_nonfirst_char(*it)) return false;
    }
    return true;
  }

  /// Return the length of a tag that appears as an initial prefix of the given
  /// string, or no value if there is a string of tag characters that does not
  /// meet the requiremet of maximum length or the requirement that first
  /// character must be a non-digit.
  ///
  /// To be a valid tag, it must be between 0 and max_size characters long, and
  /// if it is not empty, the first character must be 'a'-'z', 'A-Z', or '_',
  /// and any remaining characters must be either that or '0'-'9'. Moreover, if
  /// the tag does not extend to the end of the string, the following character
  /// must not be any of the tag characters 'a-z', 'A-Z', '_', or '0'-'9'.
  [[nodiscard]] static std::optional<std::size_t> valid_prefix_length(
      const std::string_view &sv) {
    if (sv.empty()) return 0;
    if (!is_valid_first_char(sv[0])) {
      if (is_valid_nonfirst_char(sv[0])) return {};
      return 0;
    }
    std::size_t end = std::min(sv.size(), max_size);
    for (std::size_t pos = 1; pos != end; ++pos) {
      if (!is_valid_nonfirst_char(sv[pos])) return pos;
    }
    if (sv.size() > end) {
      if (is_valid_nonfirst_char(sv[end])) return {};
    }
    return end;
  }
};
}  // namespace mysql::gtids::detail

namespace mysql::gtids {
/// True if Test is one of the tag classes.
template <class Test>
concept Is_tag = std::derived_from<Test, detail::Tag_base>;
}  // namespace mysql::gtids

// ==== Tag_interface ====

namespace mysql::gtids::detail {

/// CRTP base class defining a tag class given an implementation that provides
/// a contiguous buffer as storage, through a `out_str` member.
///
/// Subclasses should define the following member to access the tag data:
/// @code
/// std::size_t size() const;
/// char *data() const;
/// /*Output String Wrapper type*/ out_str();
/// @endcode
// "nolint": Problem not worth fixing, and workaround too intrusive.
// NOLINTBEGIN(bugprone-crtp-constructor-accessibility)
template <class Self_tp>
class Tag_interface : public Tag_base,
                      public mysql::ranges::Buffer_interface<Self_tp> {
  // NOLINTEND(bugprone-crtp-constructor-accessibility)
 public:
  /// Copy from the given tag into this tag.
  ///
  /// @param other Tag to copy from.
  ///
  /// @return If the tag implementation allocates, returns Return_status::ok on
  /// success and Return_status::error on out-of-memory errors. Otherwise this
  /// function cannot fail and returns void.
  [[nodiscard]] auto assign(const Is_tag auto &other) {
    assert(is_valid(other.string_view()));
    return assign_and_normalize(other.string_view());
  }

  /// Copy from the given std::string_view into this tag.
  ///
  /// Use in exception-free code only if the tag format has been validated
  /// already.
  ///
  /// @param sv string_view to copy from.
  ///
  /// @throws std::domain_error if `sv` does not have the correct tag format.
  void throwing_assign(const std::string_view &sv) {
    if (!is_valid(sv)) throw std::domain_error("Invalid tag format.");
    assign_and_normalize(sv);
  }

  /// Copy from the given std::string_view into this tag.
  ///
  /// @param sv string_view to copy from.
  ///
  /// @return Return_status::ok on success, Return_status::error if the given
  /// string was not a valid tag or an out-of-memory condition occurred when
  /// allocating a character buffer.
  [[nodiscard]] mysql::utils::Return_status assign(const std::string_view &sv) {
    return mysql::utils::call_and_catch(
        [&sv, this] { this->throwing_assign(sv); });
  }

  /// Set the tag to empty.
  void clear() {
    // Can't fail when assigning empty string.
    [[maybe_unused]] auto ret = assign("");
    assert(ret == mysql::utils::Return_status::ok);
  }

 private:
  /// Copy from the given string_view, changing character case to lower case.
  [[nodiscard]] auto assign_and_normalize(const std::string_view &sv) {
    auto count = [&](mysql::strconv::String_counter &counter) {
      counter.advance(sv.size());
    };
    auto write = [&](mysql::strconv::String_writer &writer) {
      if (sv.empty()) return;
      writer.write_char(get_normalized_first_char(sv[0]));
      for (std::size_t index = 1; index != sv.size(); ++index) {
        auto ch = get_normalized_nonfirst_char(sv[index]);
        assert(ch != 0);
        writer.write_char(ch);
      }
    };
    return mysql::strconv::out_str_write(self().out_str(), count, write);
  }

  [[nodiscard]] Self_tp &self() { return static_cast<Self_tp &>(*this); }
  [[nodiscard]] const Self_tp &self() const {
    return static_cast<const Self_tp &>(*this);
  }
};  // class Tag_interface

}  // namespace mysql::gtids::detail

namespace mysql::gtids {

// ==== Class Tag_trivial ====

/// Class representing a tag by storing the characters in a member array. This
/// never allocates, and is trivially default constructible and trivially
/// copyable.
///
/// Note that trivially default constructible implies that a default-constructed
/// object is *uninitialized*, so it is in an invalid state rather than a valid
/// empty tag.
class Tag_trivial : public detail::Tag_interface<Tag_trivial> {
  using Base_t = detail::Tag_interface<Tag_trivial>;

 public:
  /// Default constructor: This does not initialize the tag! Use Tag
  /// unless you need std::is_trivially_default_constructible.
  Tag_trivial() noexcept = default;

  /// Copy from any other tag
  explicit Tag_trivial(const Is_tag auto &other) noexcept : Base_t() {
    assign(other);
  }
  // Default rule-of-5

  /// Return a new Tag, initialized with the given string.
  ///
  /// Use in exception-free code only if the tag format has been validated
  /// already.
  ///
  /// @param sv string_view to copy from.
  ///
  /// @throws std::domain_error if `sv` does not have the correct tag format.
  static auto throwing_make(const std::string_view &sv) {
    Tag_trivial ret;
    ret.throwing_assign(sv);
    return ret;
  }

  /// Return the number of characters of the tag: an integer between 0 and
  /// max_size.
  [[nodiscard]] std::size_t size() const { return m_size; }

  /// Return a const pointer to the data.
  [[nodiscard]] const char *data() const { return m_data; }

 private:
  /// Return an Output String Wrapper writing to this Tag.
  [[nodiscard]] auto out_str() {
    return mysql::strconv::out_str_fixed_nz(m_data, m_size, max_size);
  }

  /// The base class needs to call `out_str()`.
  friend Base_t;

  /// Tag represented as data + size.
  char m_data[max_size];
  std::size_t m_size;
};

// ==== Class Tag ====

/// Class representing a tag by storing the characters in a member array. This
/// never allocates and is trivially copyable. A default-constructed tag is
/// initialized to empty.
///
/// The initialization at construction time implies that the tag is not
/// trivially default constructible.
class Tag : public Tag_trivial {
  using Base_t = Tag_trivial;

 public:
  /// Construct a new empty tag.
  Tag() noexcept { clear(); }

  /// Construct a new tag by copying the other tag
  explicit Tag(const Is_tag auto &other) noexcept : Base_t(other) {}
  // Default rule-of-5

  /// Return a new Tag, initialized with the given string. This raises an
  /// exception if the string does not have the correct tag format: use in
  /// exception-free code only if the sequence_number has been validated
  /// already.
  static auto throwing_make(const std::string_view &sv) {
    Tag ret;
    ret.throwing_assign(sv);
    return ret;
  }
};

/// Enable operator== for mixed tag types.
[[nodiscard]] bool operator==(const Is_tag auto &tag1,
                              const Is_tag auto &tag2) {
  return tag1.string_view() == tag2.string_view();
}

/// Enable operator!= for mixed tag types.
[[nodiscard]] bool operator!=(const Is_tag auto &tag1,
                              const Is_tag auto &tag2) {
  return !(tag1 == tag2);
}

}  // namespace mysql::gtids

// addtogroup GroupLibsMysqlGtids
/// @}

#endif  // ifndef MYSQL_GTIDS_TAG_H
