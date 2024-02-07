/* Copyright (c) 2018, 2024, Oracle and/or its affiliates.

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

#ifndef MYSQL_VERSION_GCS_PROTOCOL_MAP_INCLUDE
#define MYSQL_VERSION_GCS_PROTOCOL_MAP_INCLUDE

#include "plugin/group_replication/include/member_version.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_types.h"

/**
 * Specific Member versions.
 *
 * Add here specific versions that you want to use in the code to do
 * comparisons.
 */

/**
 * @brief First member version where we have XCom's single leader
 */
#define FIRST_PROTOCOL_WITH_SUPPORT_FOR_CONSENSUS_LEADERS 0x080027

/**
 * Converts the given GCS protocol version into the respective MySQL version.
 *
 * @param gcs_protocol The GCS protocol to convert
 * @returns the respective MySQL version as a Member_version object
 */
Member_version convert_to_mysql_version(
    Gcs_protocol_version const &gcs_protocol);

/** Maps GCS protocol version to MySQL version. */
/**
 * Converts the @c mysql_version into the respective GCS protocol, taking into
 * account this server's version @c my_version.
 *
 * @param mysql_version The MySQL version to convert
 * @param my_version The MySQL version of this server
 * @returns the respective GCS protocol version
 */
Gcs_protocol_version convert_to_gcs_protocol(
    Member_version const &mysql_version, Member_version const &my_version);

/**
 * Checks whether the given C-style string has the version format
 * "major.minor.patch".
 *
 * @param version_str the string to validate
 * @retval true if valid
 * @retval false otherwise
 */
bool valid_mysql_version_string(char const *version_str);

/**
 * Converts a "major.minor.patch" C-style string to a Member_version object.
 *
 * Requires that version_str is a valid_mysql_version_string.
 *
 * @param version_str the string to convert
 * @returns the version string as a Member_version object
 */
Member_version convert_to_member_version(char const *version_str);

#endif /* MYSQL_VERSION_GCS_PROTOCOL_MAP_INCLUDE */
