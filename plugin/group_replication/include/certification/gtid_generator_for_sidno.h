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
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

#ifndef GR_CERTIFICATION_gtid_generator_for_sidno_INCLUDED
#define GR_CERTIFICATION_gtid_generator_for_sidno_INCLUDED

#include <unordered_map>

#include "plugin/group_replication/include/certification_result.h"
#include "sql/rpl_gtid.h"

namespace gr {

/// @brief Class that is responsible for holding available GTIDs and assigning
/// GTID blocks to specific group members
class Gtid_generator_for_sidno {
 public:
  /// @brief Constructs Gtid_generator_for_sidno for a given sidno and
  /// block_size
  /// @param sidno Sidno assigned to this object
  /// @param block_size Object will reserve blocks of this size
  Gtid_generator_for_sidno(rpl_sidno sidno, std::size_t block_size);

  /// @brief Generates gno for transaction originating from server
  /// identified with the 'member_uuid'
  /// When needed, function will consult gtid_set for the list of available
  /// GTIDs
  /// @param member_uuid UUID of a group member from which trx originates
  /// @param gtid_set Currently used GTID set
  /// @returns A pair of:
  ///  - Generated gno and OK in case gno generation was possible
  ///  - invalid gno and a corresponding error code:
  ///   (-1) when there are no available gnos for this SIDNO
  ///   (-2) when assigned GTIDs block is exhausted
  std::pair<rpl_gno, mysql::utils::Return_status> get_next_available_gtid(
      const char *member_uuid, const Gtid_set &gtid_set);

  /// @brief For a given sidno, clear the assigned blocks for all members,
  /// and compute the available GTID intervals for the sidno.
  /// @details Clearing the assigned blocks implies that subsequent
  /// GTID assignments will assign new blocks for any member that commits
  /// a transaction, picking them from the available intervals for the sidno.
  /// The available intervals for the sidno, which this function also
  /// recomputes, consists of the list of intervals that are *not* in the
  /// given gtid_set, i.e., the complement of the set for that sidno.
  /// For example, if gtid_set is equal to UUID1:11-47,UUID2:1-100, and sidno
  /// corresponds to UUID1, then this will compute the set consisting of the
  /// two intervals [1,10] and [48,MAX_GNO].
  /// @param gtid_set           Gtid set under consideration
  void compute_group_available_gtid_intervals(const Gtid_set &gtid_set);

 private:
  /// @brief Represents result of GNO generation function
  enum class Gno_generation_result {
    ok,                   ///< successfully generated
    gno_exhausted,        ///< gno exausted for the given sidno/uuid (error)
    gtid_block_overflow,  ///< generated GNO > GNO_MAX defined for the current
                          ///< interval
    error                 ///< Other error, such as OOM
  };

  /// @brief This function reserves a block of GTIDs from the list of
  /// available GTIDs
  /// @param block_size         Size of the reserved block
  /// @param gtid_set           Gtid set under consideration
  /// @return Assigned interval
  Gtid_set::Interval reserve_gtid_block(longlong block_size,
                                        const Gtid_set &gtid_set);

  /// @brief Generate the candidate GNO for the current transaction.
  /// The candidate will be on the interval [start, end] or a error
  /// be returned.
  /// This method will consult group_gtid_executed (or group_gtid_extracted)
  /// to avoid generate the same value twice.

  /// @param start              The first possible value for the GNO
  /// @param end                The last possible value for the GNO
  /// @param gtid_set           Gtid set under consideration

  /// @details This method walks through available intervals for the given sidno
  /// until it finds the correct one. Returns a free GTID or one of the error
  /// codes.
  /// @return generated gno, gno_generation_result (possible error states or ok
  /// code)
  /// @see Gno_generation_result for possible error states
  std::pair<rpl_gno, Gno_generation_result> get_next_available_gtid_candidate(
      rpl_gno start, rpl_gno end, const Gtid_set &gtid_set) const;

  /// Interval container type, list of intervals
  using Interval_container_type = std::list<Gtid_set::Interval>;

  /// Container type hold currently assigned intervals (value)
  /// for the given member (key)
  using Assigned_intervals_container_type =
      std::unordered_map<std::string, Gtid_set::Interval>;

  /// Type of iterator of assigned_gtids
  using Assigned_intervals_it = Assigned_intervals_container_type::iterator;

  /// @brief gets Interval assigned to the given member
  /// @param member_uuid UUID of a group member
  /// @param gtid_set Gtid set under consideration
  /// @returns Block assigned iterator, which may be invalid in case GNO
  /// intervals exhausted for this map
  Assigned_intervals_it get_assigned_interval(const std::string &member_uuid,
                                              const Gtid_set &gtid_set);

  /// @brief Allocates GTID block for the given member
  /// @param member_uuid UUID of a group member
  /// @param gtid_set Gtid set under consideration
  /// @returns Block assigned iterator, which may be invalid in case GNO
  /// intervals exhausted for this map
  Assigned_intervals_it reserve_gtid_block(const std::string &member_uuid,
                                           const Gtid_set &gtid_set);

  rpl_sidno m_sidno;      ///< Sidno for which this map has been created
  long m_block_size;      ///< Block size used to assign GTIDs
  std::size_t m_counter;  ///< The number of assigned GTIDs
  Interval_container_type m_available_intervals;  ///< Free intervals
  Assigned_intervals_container_type
      m_assigned_intervals;  ///< Holds currently assigned intervals for the
                             ///< given member
};

}  // namespace gr

#endif  // GR_CERTIFICATION_gtid_generator_for_sidno_INCLUDED
