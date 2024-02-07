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

#ifndef GR_CERTIFICATION_CERTIFIED_GTID_INCLUDED
#define GR_CERTIFICATION_CERTIFIED_GTID_INCLUDED

#include "plugin/group_replication/include/certification_result.h"
#include "sql/rpl_gtid.h"

namespace gr {

class Certified_gtid;

/// @brief Class that aggregates important information about already certified
/// gtid
/// @details GTID consists of TSID and transaction number.
/// Due to the fact that TSID is represented internally by the number relative
/// to the used sidno map, there are as many GTID representations as maps used
/// (snapshot map / global server map used in TC / group sid map...) Certified
/// GTID provides information about server representation of the GTID (used
/// globally in the TC) and group representation of the GTID (group gtid). Other
/// information used in the code and aggregated inside of this class is: type of
/// the certified GTID, certification result (negative/positive/negative-error)
/// and information whether this GTID is local to the current server performing
/// certification
class Certified_gtid {
 public:
  /// @brief Constructs Certified_gtid object
  /// @param server_gtid GTID as seen by server (global)
  /// @param group_gtid GTID as seen by group (local to group)
  /// @param is_gtid_specified True if GTID is specified
  /// @param is_local pass true if transaction originates from this server
  /// @param cert_result Obtained certification result for transaction
  Certified_gtid(
      const Gtid &server_gtid, const Gtid &group_gtid, bool is_gtid_specified,
      bool is_local,
      const Certification_result &cert_result = Certification_result::negative);
  /// @brief Constructor, sets default gtids
  /// @param is_gtid_specified True if GTID is specified
  /// @param is_local pass true if transaction originates from this server
  Certified_gtid(bool is_gtid_specified, bool is_local);
  /// @brief Copying constructor
  /// @param src Pattern to be copied from
  Certified_gtid(const Certified_gtid &src) = default;
  /// @brief Assignment operator
  /// @param src Pattern to be copied from
  /// @return Reference to this object
  Certified_gtid &operator=(const Certified_gtid &src) = default;

  /// @brief Returns server representation of the GTID (global used in binlog)
  /// @return Server representation of the GTID (global used in binlog)
  const Gtid &get_server_gtid() const;

  /// @brief Accesses Group representation of the GTID
  /// @return Group representation of the GTID
  const Gtid &get_group_gtid() const;

  /// @brief Certification result accessor
  /// @return Certification result
  const Certification_result &get_cert_result() const;
  /// @brief Checks whether transaction originates from this server
  /// @return true if transaction originates from this server
  bool is_local() const;

  /// @brief Returns true in case certified GTID was a specified GTID
  bool is_specified_gtid() const;

 private:
  Gtid m_server_gtid;  ///< Global representation of the GTID
  Gtid m_group_gtid;   ///< group representation gtid  the way gtid is seen by
                       ///< the group
  /// Indication whether this is a local GTID
  bool m_is_local = true;
  /// True if GTID was specified before certification
  bool m_is_gtid_specified = false;
  /// Certification result obtained from the certify function
  Certification_result m_cert = Certification_result::negative;
};

}  // namespace gr

#endif  // GR_CERTIFICATION_CERTIFIED_GTID_INCLUDED
