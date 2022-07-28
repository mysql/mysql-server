/* Copyright (c) 2021, 2022, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef BINARY_LOG_GTIDS_GTID_INCLUDED
#define BINARY_LOG_GTIDS_GTID_INCLUDED

#include <set>
#include <sstream>

#include "libbinlogevents/include/gtids/global.h"

namespace binary_log::gtids {

/**
 * @brief Represents a MySQL Global Transaction Identifier.
 *
 * This class abstracts the representation of a Global Transaction Identifier.
 *
 * It contains two fields, a UUID and a sequence number.
 */
class Gtid {
 public:
  /// In 'UUID:SEQNO', this is the ':'
  static const inline std::string SEPARATOR_UUID_SEQNO{":"};

 protected:
  Uuid m_uuid;
  gno_t m_gno{0};

 public:
  /**
   * @brief Construct a new Gtid object
   *
   * @param uuid the uuid part of the transaction identifier.
   * @param gno The gno part of the transaction identfier.
   */
  Gtid(const Uuid &uuid, gno_t gno);

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
   * @brief Gets a human readable representation of this transaction identifier.
   *
   * @return A human readable representation of this transaction identifier.
   */
  virtual std::string to_string() const;

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

}  // namespace binary_log::gtids

#endif