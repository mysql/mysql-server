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

#ifndef MYSQL_GTIDS_GTIDS_H
#define MYSQL_GTIDS_GTIDS_H

/// @file
/// Experimental API header

// ==== Simple data structures ====

// The Gtid and Gtid_plain data structures, representing a (uuid, tag, number)
// triple.
#include "mysql/gtids/gtid.h"

// The Sequence_number in the last component of a Gtid; equal to uint64_t.
#include "mysql/gtids/sequence_number.h"

// The Tag and Tag_plain data structures in the second component of a Gtid.
#include "mysql/gtids/tag.h"

// The Tsid and Tsid_plain data structures, representing a (uuid, tag) pair.
#include "mysql/gtids/tsid.h"

// The Gtid_set data structure, an alias for a type defined in mysql::sets.
#include "mysql/gtids/gtid_set.h"

// ==== Gtid set operations ====

// Union_view, is_subset, volume, etc
#include "mysql/sets/sets.h"

// ==== Other Gtid operations ====

// Predicate to check if a tag/tsid/gtid/gtid_set is/contains a nonempty tag.
#include "mysql/gtids/has_tags.h"

// ==== String conversion ====

// Format class holding configuration for the text form of Gtids.
#include "mysql/gtids/strconv/gtid_binary_format.h"

// Convert Gtid_set/Gtid/Tsid/Tag to/from binary.
#include "mysql/gtids/strconv/gtid_binary_format_conv.h"

// Format class holding configuration for the text form of Gtids.
#include "mysql/gtids/strconv/gtid_text_format.h"

// Convert Gtid_set/Gtid/Tsid/Tag to/from text.
#include "mysql/gtids/strconv/gtid_text_format_conv.h"

#endif  // ifndef MYSQL_GTIDS_GTIDS_H
