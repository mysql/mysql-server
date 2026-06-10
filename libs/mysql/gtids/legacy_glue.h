// Copyright (c) 2025, 2026, Oracle and/or its affiliates.
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

#ifndef MYSQL_GTIDS_LEGACY_GLUE_H
#define MYSQL_GTIDS_LEGACY_GLUE_H

/// @file
/// Experimental API header

#include "mysql/gtid/gtid.h"            // Gtid (old)
#include "mysql/gtid/tag.h"             // Tag (old)
#include "mysql/gtid/tsid.h"            // Tsid (old)
#include "mysql/gtid/uuid.h"            // Uuid (old)
#include "mysql/gtids/gtid.h"           // Gtid (new)
#include "mysql/gtids/tag.h"            // Tag (new)
#include "mysql/gtids/tsid.h"           // Tsid (new)
#include "mysql/utils/return_status.h"  // Return_status
#include "mysql/uuids/uuid.h"           // Uuid (new)

/// @addtogroup GroupLibsMysqlGtids
/// @{

namespace mysql::gtids {

/// Copy the legacy Uuid into mysql::uuids::Uuid.
inline void old_to_new(const mysql::gtid::Uuid &old_uuid,
                       mysql::uuids::Uuid &new_uuid) {
  old_uuid.copy_to(new_uuid.udata());
}

/// Copy the legacy Tag into mysql::gtids::Tag.
inline void old_to_new(const mysql::gtid::Tag &old_tag,
                       mysql::gtids::Tag &new_tag) {
  [[maybe_unused]] auto ret = new_tag.assign(old_tag.get_data());
  assert(ret == mysql::utils::Return_status::ok);
}

/// Copy the legacy Tsid into mysql::gtids::Tsid.
inline void old_to_new(const mysql::gtid::Tsid &old_tsid,
                       mysql::gtids::Tsid &new_tsid) {
  old_to_new(old_tsid.get_uuid(), new_tsid.uuid());
  old_to_new(old_tsid.get_tag(), new_tsid.tag());
}

/// Copy the legacy Gtid into mysql::gtids::Gtid.
inline void old_to_new(const mysql::gtid::Gtid &old_gtid,
                       mysql::gtids::Gtid &new_gtid) {
  old_to_new(old_gtid.get_tsid(), new_gtid.tsid());
}

}  // namespace mysql::gtids

// addtogroup GroupLibsMysqlGtids
/// @}

#endif  // ifndef MYSQL_GTIDS_LEGACY_GLUE_H
