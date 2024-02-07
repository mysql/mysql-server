// Copyright (c) 2023, 2024, Oracle and/or its affiliates.
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

#ifndef MYSQL_GTID_TAG_H
#define MYSQL_GTID_TAG_H

#include <array>
#include <cstring>
#include <memory>
#include <string>

#include "mysql/binlog/event/nodiscard.h"
#include "mysql/gtid/gtid_constants.h"
#include "mysql/gtid/gtid_format.h"
#include "mysql/gtid/tag_plain.h"

/// @addtogroup GroupLibsMysqlGtid
/// @{

namespace mysql::gtid {

struct Tag_plain;

/// @class Tag
/// @brief Representation of the GTID tag
/// @details Tag format: [a-z_]{a-z0-9_}{0,31}
/// Tag may be created from a given text. Text may contain leading and
/// trailing spaces which will be omitted during Tag creation. Text may also
/// contain uppercase characters. Acceptable format for text is as follows:
/// [:space:][a-zA-Z_][a-zA-Z0-9_]{0,31}[:space:]
class Tag {
 public:
  using Tag_data = std::string;

  /// @brief Constructs an empty, valid tag
  Tag() = default;

  /// @brief Construct from Tag_plain object
  /// @param tag pattern
  explicit Tag(const Tag_plain &tag);

  /// @brief Constructs tag from a given text. If passed text is not a valid
  /// tag representation, object remains empty (empty tag)
  /// @param[in] text Textual representation of a tag
  /// @see from_cstring
  Tag(const std::string &text);

  /// @brief Obtains textual representation of internal tag
  /// @returns tag string
  std::string to_string() const;

  /// @brief Obtains textual representation of internal tag and writes it to out
  /// @param [out] out Output string
  /// @returns Number of characters written to out
  std::size_t to_string(char *out) const;

  /// @brief Creates Tag object from a given text. If passed text is not a valid
  /// tag representation, object remains empty (empty tag). Text must end
  /// with a null character or a gtid separator
  /// @param[in] text Textual representation of a tag
  /// @returns Number of characters read, 0 means that either tag is empty or
  /// invalid
  [[NODISCARD]] std::size_t from_string(const std::string &text);

  /// @brief Creates Tag object from a given text. If passed text is not a valid
  /// tag representation, object remains empty (empty tag)
  /// Since length of text is unknown, it is expected to be null terminated
  /// @param[in] text Textual representation of a tag
  /// @returns Number of characters read, 0 means that either tag is empty or
  /// invalid
  [[NODISCARD]] std::size_t from_cstring(const char *text);

  /// @brief Indicates whether transaction tag is empty
  /// @returns Answer to question: is tag empty?
  bool is_empty() const { return m_data.empty(); }

  /// @brief Indicates whether transaction tag is defined (is not empty)
  /// @returns Answer to question: is tag defined?
  bool is_defined() const { return !is_empty(); }

  /// @brief Operator ==
  /// @param other pattern to compare against
  /// @return Result of comparison ==
  bool operator==(const Tag &other) const;

  /// @brief Operator !=
  /// @param other pattern to compare against
  /// @return Result of comparison !=
  bool operator!=(const Tag &other) const;

  /// @brief Compares this tag with other, <
  /// @retval true This tag is before other (alphabetical order)
  /// @retval false Other tag is before this (alphabetical order)
  bool operator<(const Tag &other) const {
    return m_data.compare(other.m_data) < 0;
  }

  /// @brief stores Tag in buffer
  /// @param buf Buffer to store bytes, must contain at least
  /// get_encoded_length() bytes
  /// @param gtid_format Format of encoded GTID. If tag is not defined for this
  /// GTID and tagged format is used, 0 will be encoded as length of the string.
  /// In case "untagged" format is requested, function won't encode additional
  /// tag information for untagged GTIDs. When using
  /// untagged, tag is required to be empty.
  /// @return the number of bytes written into the buf
  [[NODISCARD]] std::size_t encode_tag(unsigned char *buf,
                                       const Gtid_format &gtid_format) const;

  /// @brief returns length of encoded tag, based on defined format
  /// @param gtid_format Format of encoded GTID. If tag is not defined for this
  /// GTID and tagged format is used, 0 will be encoded as length of the string.
  /// In case "untagged" format is requested, function won't encode additional
  /// tag information for untagged GTIDs. When using
  /// untagged, tag is required to be empty.
  /// @return length of encoded tag in bytes
  /// @details Note that in case encoded format is "tagged" and tag is empty,
  /// we still to encode tag length (1 byte for length equal to 0).
  std::size_t get_encoded_length(const Gtid_format &gtid_format) const;

  /// @brief returns length of tag
  /// @return tag length
  std::size_t get_length() const;

  /// @brief Obtains maximum length of encoded tag (compile time)
  /// @return Maximum length of encoded tag in bytes
  static constexpr std::size_t get_max_encoded_length() {
    // we fix maximum size encoding to 1, since maximum tag length is 32
    return 1 + tag_max_length;
  }

  /// @brief Reads Tag data from the buffer
  /// @param buf Buffer to read tag from
  /// @param buf_len Number of bytes in the buffer
  /// @param gtid_format Gtid format read from stream, if untagged, tag
  /// information is assumed to be empty. If tagged, gtid tag length will be
  /// read from the stream both for untagged and tagged GTIDs
  /// @return The number of bytes read or 0. For Gtid_format::untagged,
  /// function will read bytes. For Gtid_format::tagged, 0 means
  /// that an error occurred (e.g. not enough bytes in the buffer to read the
  /// tag - corrupted bytes in the buffer).
  [[NODISCARD]] std::size_t decode_tag(const unsigned char *buf,
                                       std::size_t buf_len,
                                       const Gtid_format &gtid_format);

  /// @brief Structure to compute hash function of a given Tag object
  struct Hash {
    /// @brief Computes hash of a given Tag object
    /// @param arg Object handle for which hash will be calculated
    size_t operator()(const Tag &arg) const;
  };

  /// @brief Internal data accessor
  /// @return Const reference to internal tag data
  const Tag_data &get_data() const { return m_data; }

  /// @brief Internal data accessor, non const (serialization)
  /// @return Reference to internal tag data
  Tag_data &get_data() { return m_data; }

 protected:
  /// @brief Checks whether current character is a valid ending of a Tag string
  /// @param[in] character A character under test
  /// @returns Answer to question: is character a valid tag ending?
  static bool is_valid_end_char(const char &character);

  /// @brief Checks whether current character is a valid Tag character
  /// @param[in] character A character under test
  /// @param[in] pos Position within a tag
  /// @returns Answer to question: is character a valid tag character?
  static bool is_character_valid(const char &character, std::size_t pos);

  /// @brief Replaces internal tag data with a given text, includes text
  /// normalization
  /// @param[in] text Pattern to copy from, valid tag string
  /// @param[in] len Number of characters to copy
  void replace(const char *text, std::size_t len);

  Tag_data m_data = "";  ///< internal tag representation
};

}  // namespace mysql::gtid

/// @}

#endif  // MYSQL_GTID_TAG_H
