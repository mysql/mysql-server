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

#ifndef CS_READER_STATE_INCLUDED
#define CS_READER_STATE_INCLUDED

#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include "libbinlogevents/include/gtids/gtid.h"
#include "libbinlogevents/include/gtids/gtidset.h"
#include "libbinlogevents/include/trx_boundary_parser.h"

namespace cs::reader {

/**
 * @brief Represents the state of the replication stream.
 *
 */
class State {
 protected:
  /**
   * @brief The set of gtids handled in this stream.
   *
   * This set is updated every time a transaction identifier event comes down
   * the stream. The identifier is added to the stream.
   */
  binary_log::gtids::Gtid_set m_gtid_set;

 public:
  State() = default;
  virtual ~State() = default;
  State &operator=(const State &other);
  State(const State &other);

  /**
   * @brief Compares state objects. If the state matches returns true.
   *
   * @param other The other state to compare this one with.
   * @return true if the contents of the other state matches the contents of
   * this one.
   * @return false if the contents of this state does not match the contents of
   * the other state.
   */
  virtual bool operator==(const State &other) const;

  /**
   * @brief Resets this state object.
   *
   * Clears replication stream metadata completely.
   */
  virtual void reset();

  /**
   * @brief Get the gtids object.
   *
   * @return a reference to the internal set of gtids.
   */
  virtual const binary_log::gtids::Gtid_set &get_gtids() const;

  /**
   * @brief Sets the gtids object in the state. This completely overwrites the
   * existing gtid set.
   *
   * @param gtids The set of gtids to add.
   * @return true if there was an error while setting the gtids.
   * @return false if there was no error while setting the gitds.
   */
  virtual bool set_gtids(const binary_log::gtids::Gtid_set &gtids);

  /**
   * @brief Adds a set of gtid to the state.
   *
   * @param gtids the set of transaction identifiers to add.
   */
  virtual void add_gtid_set(const binary_log::gtids::Gtid_set &gtids);

  /**
   * @brief Adds a gtid to the state.
   *
   * @param gtid the transaction identifier to add to this state.
   */
  virtual void add_gtid(const binary_log::gtids::Gtid &gtid);

  /**
   * @brief Returns a human readable representation of the state.
   *
   * @return std::string A human readable representation of the state.
   */
  virtual std::string to_string() const;

  friend std::ostringstream &operator<<(std::ostringstream &out,
                                        const State &in);

  friend std::stringstream &operator<<(std::stringstream &out, const State &in);
};

}  // namespace cs::reader
#endif