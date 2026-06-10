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

#ifndef MYSQL_GTIDS_HAS_TAGS_H
#define MYSQL_GTIDS_HAS_TAGS_H

/// @file
/// Experimental API header

#include "mysql/gtids/gtid.h"      // Gtid
#include "mysql/gtids/gtid_set.h"  // Gtid_set
#include "mysql/gtids/tag.h"       // Tag
#include "mysql/gtids/tsid.h"      // Tsid

/// @addtogroup GroupLibsMysqlGtids
/// @{

namespace mysql::gtids {

/// @return true if the tag is not empty.
bool has_tags(const Is_tag auto &tag) { return !tag.empty(); }

/// @return true if the Tsid has a non-empty tag.
bool has_tags_tsid(const Is_tsid auto &tsid) { return has_tags(tsid.tag()); }

/// @return true if the Gtid has a non-empty tag.
bool has_tags(const Is_gtid auto &gtid) { return has_tags(gtid.tag()); }

/// @return true if the Gtid_set has at least one Tsid with non-empty tag.
bool has_tags(const Is_gtid_set auto &gtid_set) {
  for (const auto &[tsid, interval_set] : gtid_set) {
    if (has_tags_tsid(tsid)) return true;
  }
  return false;
}

}  // namespace mysql::gtids

// addtogroup GroupLibsMysqlGtids
/// @}

#endif  // ifndef MYSQL_GTIDS_HAS_TAGS_H
