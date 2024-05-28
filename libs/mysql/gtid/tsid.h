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

#ifndef MYSQL_GTID_TSID_H
#define MYSQL_GTID_TSID_H

#include "mysql/gtid/gtid_constants.h"
#include "mysql/gtid/gtid_format.h"
#include "mysql/gtid/tag.h"
#include "mysql/gtid/uuid.h"

/// @addtogroup GroupLibsMysqlGtid
/// @{

namespace mysql::gtid {

/// @brief Maximum TSID text length (without null character)
inline constexpr auto tsid_max_length = Uuid::TEXT_LENGTH + 1 + tag_max_length;

struct Tsid_plain;

/// @brief Represents Transaction Source Identifier which is composed of source
/// UUID and transaction tag. Transaction tag may be empty.
class Tsid {
 public:
  /// @brief Constructs TSID from a given uuid and a tag
  /// @param[in] uuid UUID component
  /// @param[in] tag Tag component
  Tsid(const Uuid &uuid, const Tag &tag);

  /// @brief Constructs TSID from a given uuid, tag is empty
  /// @param[in] uuid UUID component
  Tsid(const Uuid &uuid);

  /// @brief Construct from Tsid_plain object
  /// @param arg Source to copy from
  explicit Tsid(const Tsid_plain &arg);

  /// @brief Constructs empty TSID
  Tsid() = default;
  Tsid(Tsid const &) = default;
  Tsid(Tsid &&) = default;
  Tsid &operator=(Tsid const &) = default;
  Tsid &operator=(Tsid &&) = default;

  /// @brief Returns textual representation of Transaction Source Identifier
  std::string to_string() const;

  /// @brief Obtains textual representation of TSID and writes it to out
  /// @param [out] out Output string
  /// @returns Number of characters written to out
  std::size_t to_string(char *out) const;

  /// @brief Obtains textual representation of TSID and writes it to out
  /// @details version with a custom tag-sid separator
  /// @param [out] out Output string
  /// @param [in] tag_sid_separator Tag-sid separator
  /// @returns Number of characters written to out
  std::size_t to_string(char *out, const char *tag_sid_separator) const;

  /// @brief Fills Tsid with data from text
  /// @param[in] text Encoded TSID representation terminated with null sign,
  /// GTID separator or UUID set separator if part of the GTID set encoding
  /// @return The number of bytes read, or 0 on error
  [[NODISCARD]] std::size_t from_cstring(const char *text);

  /// @brief Default TSID separator
  static constexpr auto tsid_separator = ":";

  /// @brief Operator ==
  /// @param other pattern to compare against
  /// @return Result of comparison ==
  bool operator==(const Tsid &other) const;

  /// @brief Operator !=
  /// @param other pattern to compare against
  /// @return Result of comparison !=
  bool operator!=(const Tsid &other) const;

  /// @brief Operator <
  /// @details Compares uuid first. If uuids are equal, compares tags
  /// @param other pattern to compare against
  /// @return Result of comparison this < other
  bool operator<(const Tsid &other) const;

  /// @brief Tag accessor
  /// @return Const reference to Tag object
  const Tag &get_tag() const { return m_tag; }

  /// @brief Tag accessor, non const (serialization)
  /// @return Non-const Reference to Tag object
  Tag &get_tag_ref() { return m_tag; }

  /// @brief Sets internal tag to a given tag object
  /// @param tag Source to copy from
  void set_tag(const Tag &tag) { m_tag = tag; }

  /// @brief UUID accessor
  /// @return Const reference to UUID component of TSID
  const Uuid &get_uuid() const { return m_uuid; }

  /// @brief Non const getter is needed in some functions (copy data)
  /// @returns Reference to internal UUID
  Uuid &get_uuid() { return m_uuid; }

  /// @brief Checks whether this TSID contains tag
  /// @retval true This TSID contains tag
  /// @retval false This TSID contains empty tag
  bool is_tagged() const { return m_tag.is_defined(); }

  /// @brief Structure to compute hash function of a given Tag object
  struct Hash {
    /// @brief Computes hash of a given Tsid object
    /// @param arg Object handle for which hash will be calculated
    size_t operator()(const Tsid &arg) const {
      return Uuid_hash{}(arg.m_uuid) ^ Tag::Hash{}(arg.m_tag);
    }
  };
  friend struct Hash;

  /// @brief Clears data - uuid and tag
  void clear();

  /// @brief Obtains maximum length of encoded TSID (compile time)
  /// @return Maximum length of encoded tag in bytes
  static constexpr std::size_t get_max_encoded_length() {
    return Uuid::BYTE_LENGTH + Tag::get_max_encoded_length();
  }

  /// @brief stores TSID in buffer
  /// @param buf Buffer to store bytes
  /// @return the number of bytes written into the buf
  /// @param gtid_format Format of encoded GTID. If tag is not defined for this
  /// GTID and tagged format is used, 0 will be encoded as length of the string.
  /// In case "untagged" format is requested, function won't encode additional
  /// tag information for untagged GTIDs. When using
  /// untagged, tag is required to be empty.
  std::size_t encode_tsid(unsigned char *buf,
                          const Gtid_format &gtid_format) const;

  /// @brief reads TSID from the buffer
  /// @param stream Stream to read tsid from
  /// @param stream_len Length of the stream
  /// @param gtid_format Gtid format expected in the stream
  /// @return The number of bytes read or 0. 0 means that an error occurred
  /// (e.g. not enough bytes in the buffer to read the
  /// tsid - corrupted bytes in the buffer).
  [[NODISCARD]] std::size_t decode_tsid(const unsigned char *stream,
                                        std::size_t stream_len,
                                        const Gtid_format &gtid_format);

 private:
  Uuid m_uuid = {0};  ///< GTID UUID
  Tag m_tag;          ///< GTID Tag
};

}  // namespace mysql::gtid

/// @}

#endif  // MYSQL_GTID_TSID_H
