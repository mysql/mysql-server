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

#ifndef MYSQL_GTID_GTIDSET_H
#define MYSQL_GTID_GTIDSET_H

#include <cstddef>
#include <map>
#include <set>
#include <sstream>

#include "mysql/gtid/global.h"
#include "mysql/gtid/gtid.h"
#include "mysql/gtid/tag.h"
#include "mysql/gtid/tsid.h"
#include "mysql/utils/nodiscard.h"

/// @addtogroup GroupLibsMysqlGtid
/// @{

namespace mysql::gtid {

/**
 * @brief This class represents a range of transaction identifiers.
 *
 * A transaction identifier is composed of two parts, the UUID and the sequence
 * number.
 *
 * There can be multiple transaction identifiers for a given UUID. When their
 * sequence number are contiguous they can be represented as an interval. This
 * is the class that represents on of those intervals.
 *
 */
class Gno_interval {
 public:
  /// In 'UUID:GNO-GNO', this is the '-'
  static const inline std::string SEPARATOR_GNO_START_END{"-"};

 private:
  gno_t m_start{0};
  gno_t m_next_gno_after_end{0};

 public:
  virtual ~Gno_interval() = default;

  /**
   * @brief Copy assignment.
   *
   * @note This operator does not perform any check about the other interval
   * being valid or not. It is up to the caller to verify that if needed.
   *
   * @param other The other interval to copy.
   * @return Gno_interval& a reference to the copied interval.
   */
  Gno_interval &operator=(const Gno_interval &other);

  /**
   * @brief Construct a new Gno_interval object from the other one provided.
   *
   * @note This operator does not perform any check about the other interval
   * being valid or not. It is up to the caller to verify that if needed.
   *
   * @param other The object to copy.
   */
  Gno_interval(const Gno_interval &other);

  /**
   * @brief Compares this interval with another one. Returns true if there is a
   * match.
   *
   * @note This operator does not perform any check about the other interval
   * being valid or not. It is up to the caller to verify that if needed, before
   * calling this member function.
   *
   * @param other The other interval to compare this one with.
   * @return true If both intervals match.
   * @return false If intervals do not match.
   */
  virtual bool operator==(const Gno_interval &other) const;

  /**
   * @brief Construct a new Gno_interval object.
   *
   * @param start The start of the interval (inclusive).
   * @param end The end of the interval (inclusive).
   */
  Gno_interval(gno_t start, gno_t end);

  /**
   * @brief Establishes a total order between two intervals.
   *
   * Total order is determined using the start and end values of
   * the interval.
   *
   * An interval A comes before interval B if its start value is smaller
   * than the start value of B.
   *
   * If B's start value is smaller than A's start value then B comes
   * before A.
   *
   * If A and B have the same start value, the the end of the intervals
   * are checked and if A has a lower interval end than B, then A is
   * considered to come before B in that case.
   *
   * @note This operator does not perform any check about the other interval
   * being valid or not. It is up to the caller to verify that if needed, before
   * calling this member function.
   *
   * @param other the other interval.
   * @return true if this is smaller than the other
   * @return false if this is equal or greater than the other.
   */
  virtual bool operator<(const Gno_interval &other) const;

  /**
   * @brief This checks whether this interval intersects with the other.
   *
   * @note This operator does not perform any check about the other interval
   * being valid or not. It is up to the caller to verify that if needed, before
   * calling this member function.
   *
   * @param other The other interval to check against this one.
   * @return true if the intervals intersect.
   * @return false if it does not intersect.
   */
  virtual bool intersects(const Gno_interval &other) const;

  /**
   * @brief Checks if this interval is contiguous with the other one.
   *
   * Two intervals are contiguous if they do not intersect but there
   * are no gaps between them. No gaps means that the upper limit of
   * interval A is the value immediately preceding the lower limit
   * of interval B, under numeric natural ordering.
   *
   * @note This operator does not perform any check about the other interval
   * being valid or not. It is up to the caller to verify that if needed, before
   * calling this member function.
   *
   * @param other the interval to check against.
   * @return true if the intervals are contiguous.
   * @return false otherwise.
   */
  virtual bool contiguous(const Gno_interval &other) const;

  /**
   * @brief Checks if the other interval intersects or is contiguous with this
   * one.
   *
   * @param other The interval to check against this one.
   * @return true if the other interval intersects or is contiguous with this
   * one.
   * @return false otherwise.
   */
  virtual bool intersects_or_contiguous(const Gno_interval &other) const;

  /**
   * @brief Adds the other interval to this one.
   *
   * For this operation to complete successfully, the intervals must
   * intersect or be contiguous. Otherwise, the operation will fail.
   *
   * @note This operator does not perform any check about the other interval
   * being valid or not. It is up to the caller to verify that if needed, before
   * calling this member function.
   *
   * @param other The interval to add to this one.
   * @return true if the adding was not successful.
   * @return false if the adding was successful.
   */
  virtual bool add(const Gno_interval &other);

  /**
   * @brief Gets the first sequence number in the interval.
   *
   * @return the first sequence number of the interval.
   */
  virtual gno_t get_start() const;

  /**
   * @brief Gets the last sequence number in the interval.
   *
   * @return the last sequence number in the interval.
   */
  virtual gno_t get_end() const;

  /**
   * @brief Number of entries in this interval.
   *
   * @return the size of the interval.
   */
  virtual std::size_t count() const;

  /**
   * @brief Gets a human readable representation of this identifier.
   *
   * @return A human readable representation of this identifier.
   */
  virtual std::string to_string() const;

  /**
   * @brief Checks whether this interval is valid or not.
   *
   * An interval is invalid if the start value is smaller than 0 or
   * if the start is larger than the end of the interval.
   *
   * @return true if the interval is valid, false otherwise.
   */
  virtual bool is_valid() const;
};

/**
 * @brief This class represents a set of transaction identifiers.
 *
 * A set of transaction identifiers contains zero or more entries. When there
 * are multiple entries, there can multiple intervals as well. Different
 * intervals may share the same UUID part or not. This class abstracts that
 * in-memory representation.
 *
 */
class Gtid_set {
 public:
  static const inline std::string empty_gtid_set_str{""};
  /// In 'UUID:INTERVAL:INTERVAL', this is the second ':'
  static const inline std::string separator_interval{":"};
  /// In 'SID:GNO,SID:GNO', this is the ','
  static const inline std::string separator_uuid_set{","};
  using Tag = mysql::gtid::Tag;
  using Tsid = mysql::gtid::Tsid;

 protected:
  [[NODISCARD]] virtual bool do_add(const Tsid &tsid,
                                    const Gno_interval &interval);
  [[NODISCARD]] virtual bool do_add(const Uuid &uuid, const Tag &tag,
                                    const Gno_interval &interval);

 public:
  /**
   * @brief Tsid_interval_map is a 2-level map between tsids and an ordered
   * set of Gno_intervals.
   * Uuid is mapped into GTID tags, each pair of Uuid and Tag (TSID) is mapped
   * into ordered set of gno intervals
   */
  using Interval_set = std::set<Gno_interval>;
  using Tag_interval_map = std::map<Tag, Interval_set>;
  using Tsid_interval_map = std::map<Uuid, Tag_interval_map>;

  Gtid_set() = default;
  virtual ~Gtid_set();

  Gtid_set(const Gtid_set &other) = delete;

  /**
   * @brief Copy assignment.
   *
   * @note This operator does not check whether the parameters are valid
   * or not. The caller should perform such check before calling this member
   * function.
   *
   * @param other the Gtid_set to be copied over to this one.
   * @return Gtid_set& a reference to the copied Gtid_set.
   */
  Gtid_set &operator=(const Gtid_set &other);

  /**
   * @brief Compares this set with another one.
   *
   * @param other The other set to compare this one with.
   * @return true If both sets match.
   * @return false If sets do not match.
   */
  virtual bool operator==(const Gtid_set &other) const;

  /**
   * @brief Iterates through recorded TSIDs and returns
   * format of the Gtid_set
   *
   * @return Format of this GTID set
   * @see Gtid_format
   */
  virtual Gtid_format get_gtid_set_format() const;

  /**
   * @brief Adds a new interval indexed by the given uuid.
   *
   * @note This member function does not check whether the parameters are valid
   * or not. The caller should perform such check before calling this member
   * function.
   *
   * @return true if the there was an error adding the interval, false
   * otherwise.
   */
  [[NODISCARD]] virtual bool add(const Tsid &tsid,
                                 const Gno_interval &interval);

  /**
   * @brief Gets a copy of the internal set.
   *
   * @return an internal copy of the given set.
   */
  virtual const Tsid_interval_map &get_gtid_set() const;

  /**
   * @brief Add a set of identifiers to this one.
   *
   * @note This member function does not check whether the parameters are valid
   * or not. The caller should perform such check before calling this member
   * function.
   *
   * @param other the set to add to this one.
   * @return true if there was a failure adding the gtids.
   * @return false otherwise.
   */
  virtual bool add(const Gtid_set &other);

  /**
   * @brief Get the num TSIDs held in the GTID set
   *
   * @return std::size_t Number of TSIDs
   */
  virtual std::size_t get_num_tsids() const;

  /**
   * @brief Adds the given identifier to this set.
   *
   * @note This member function does not check whether the parameters are valid
   * or not. The caller should perform such check before calling this member
   * function.
   *
   * @param gtid the identifier to add.
   * @return true if it failed while adding the identifier.
   * @return false otherwise.
   */
  virtual bool add(const Gtid &gtid);

  /**
   * @brief Checks whether this set contains the given identifier.
   *
   * @note This member function does not check whether the parameters are valid
   * or not. The caller should perform such check before calling this member
   * function.
   *
   * @param gtid the gtid to check whehther it exists in this set or not.
   * @return true if the identifier exists in this set.
   * @return false if the identifier does not exist in this set.
   */
  virtual bool contains(const Gtid &gtid) const;

  /**
   * @brief A human readable representation of this set.
   *
   * @return a human readable representation of this set.
   */
  virtual std::string to_string() const;

  /**
   * @brief Resets this set, making it empty.
   *
   */
  virtual void reset();

  /**
   * @brief Returns true if this is an empty set.
   *
   * @return true
   * @return false
   */
  virtual bool is_empty() const;

  /**
   * @brief Gets the number of entries in this set.
   *
   * @return the cardinality of this set.
   */
  virtual std::size_t count() const;

 protected:
  /**
   * @brief An ordered map of entries mapping Uuid to a list of intervals.
   */
  Tsid_interval_map m_gtid_set{};
};

}  // namespace mysql::gtid

/// @}

#endif  // MYSQL_GTID_GTIDSET_H
