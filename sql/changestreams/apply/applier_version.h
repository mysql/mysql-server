// Copyright (c) 2026, Oracle and/or its affiliates.
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

#ifndef MYSQL_CS_APPLY_APPLIER_VERSION_H
#define MYSQL_CS_APPLY_APPLIER_VERSION_H

namespace cs::apply {

/// @struct Applier_version
/// @brief Enumerates the available versions of the applier.
///
/// This enum defines the different applier versions that can be used in the
/// system.
struct Applier_version {
  /// Required by system tables and PFS when no specific version is chosen.
  static constexpr unsigned int unspecified{0};
  /// Use Multi-threaded Applier (MTA) for replication.
  static constexpr unsigned int mta{1};
  /// Use Change Stream Applier (CSA) for replication.
  static constexpr unsigned int csa{2};
  /// An unknown or invalid applier version, used as an end guard.
  static constexpr unsigned int unknown{3};
};

}  // namespace cs::apply

#endif  // MYSQL_CS_APPLY_APPLIER_VERSION_H
