/*
  Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef MYSQLROUTER_CLUSTER_METADATA_INCLUDED
#define MYSQLROUTER_CLUSTER_METADATA_INCLUDED

#include "mysqlrouter/mysql_session.h"

namespace mysqlrouter {

struct MetadataSchemaVersion {
  unsigned int major;
  unsigned int minor;
  unsigned int patch;
};

// Semantic version number that this Router version supports
constexpr MetadataSchemaVersion required_metadata_schema_version{1, 0, 0};

MetadataSchemaVersion get_metadata_schema_version(MySQLSession *mysql);

bool metadata_schema_version_is_compatible(
    const mysqlrouter::MetadataSchemaVersion &required,
    const mysqlrouter::MetadataSchemaVersion &available);

}  // namespace mysqlrouter
#endif
