/* Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/// @defgroup GroupLibsMysqlGtid MySQL Libraries : GTID
/// @ingroup GroupLibsMysql

#ifndef MYSQL_GTID_GTID_H
#define MYSQL_GTID_GTID_H

#include <set>
#include <sstream>

#include "mysql/binlog/event/nodiscard.h"
#include "mysql/gtid/global.h"
#include "mysql/gtid/tsid.h"
#include "mysql/serialization/archive_binary.h"

/// @addtogroup GroupLibsMysqlGtid
/// @{

namespace mysql::gtid {

/**
 * @brief Represents a MySQL Global Transaction Identifier.
 *
 * This class abstracts the representation of a Global Transaction Identifier.
 *
 * It contains two fields, a TSID, composed of UUID and tag, and a sequence
 * number.
 */
class Gtid {
 public:
  /// In 'UUID:SEQNO', this is the ':'
  static constexpr auto separator_gtid{':'};

 protected:
  Tsid m_tsid;
  gno_t m_gno{0};

 public:
  /**
   * @brief Construct an empty GTID
   */
  Gtid() = default;

  /**
   * @brief Construct a new Gtid object
   *
   * @param tsid TSID part of the transaction identifier
   * @param gno The gno part of the transaction identfier.
   */
  Gtid(const Tsid &tsid, gno_t gno);

  /**
   * @brief Destroy the Gtid object
   *
   */
  virtual ~Gtid();

  /**
   * @brief Get the sequence number of this transaction identifier.
   *
   * @return The sequence number part of this transaction identifier.
   */
  virtual gno_t get_gno() const;

  /**
   * @brief Get the uuid of this transaction identifier.
   *
   * @return The uuid part of this transaction identifier.
   */
  virtual const Uuid &get_uuid() const;

  /**
   * @brief Get the tsid of this transaction identifier.
   *
   * @return The tsid part of this transaction identifier.
   */
  virtual const Tsid &get_tsid() const;

  /**
   * @brief Get the tag of this transaction identifier.
   *
   * @return The tag part of this transaction identifier.
   */
  virtual const Tag &get_tag() const;

  /**
   * @brief Gets a human readable representation of this transaction identifier.
   *
   * @return A human readable representation of this transaction identifier.
   */
  virtual std::string to_string() const;

  /**
   * @brief Encodes GTID into a binary format. Supported is only tagged format
   * of a GTID. Buf must be preallocated with a required number of bytes
   *
   * @param buf Buffer to write to
   *
   * @return Number of bytes written
   */
  virtual std::size_t encode_gtid_tagged(unsigned char *buf) const;

  /**
   * @brief Decodes GTID from a given buffer. Supported is only tagged format
   * of a GTID. Buf must contain required number of bytes
   *
   * @param buf Buffer to read from
   * @param buf_len Buffer length in bytes
   *
   * @return Number of bytes read from the buffer or 0 in case decoding is not
   * possible
   */
  [[NODISCARD]] virtual std::size_t decode_gtid_tagged(const unsigned char *buf,
                                                       std::size_t buf_len);

  /**
   * @brief Gets maximum length of encoded GTID in compile time
   *
   * @return Maximum number of bytes needed to store GTID
   */
  static constexpr std::size_t get_max_encoded_length() {
    return Tsid::get_max_encoded_length() +
           mysql::serialization::Archive_binary::get_max_size<int64_t, 0>();
  }

  /**
   * @brief Compares two identifiers and returns whether they match or not.
   *
   * @param other The other transaction identifier to compare to this one.
   * @return true if the identifiers are equal.
   * @return false otherwise.
   */
  virtual bool operator==(const Gtid &other) const;

  /**
   * @brief Compares two identifiers and returns whether they are different.
   *
   * @param other The other transaction identifier to compare to this one.
   * @return true if the identifiers are different.
   * @return false otherwise.
   */
  virtual bool operator!=(const Gtid &other) const;

  /**
   * @brief Copy assignment.
   *
   * Note that this operator will do a deep copy of the other identifier.
   *
   * @param other Copies the uuid and gno of the other identifier.
   * @return a copy of the other identifier.
   */
  virtual Gtid &operator=(const Gtid &other);

  /**
   * @brief Construct a new Gtid object from another one, by deep copying its
   * contents.
   *
   * @param other the other gtid to construct this gtid from.
   */
  Gtid(const Gtid &other);
};

}  // namespace mysql::gtid

/// @}

#endif  // MYSQL_GTID_GTID_H
