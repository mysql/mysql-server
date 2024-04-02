/*
  Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_DATA_ENTRY_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_DATA_ENTRY_H_

#include <cstdint>
#include "mrs/database/entry/universal_id.h"

namespace mrs {
namespace database {
namespace entry {

enum EntryType { key_static, key_rest, key_static_sub };

struct EntryKey {
  EntryType type;
  UniversalId id;
  // sub_id is introduced to create virtual objects under
  // given category.
  uint64_t sub_id{0};

  bool operator<(const EntryKey &other) const {
    if (type < other.type) return true;
    if (type > other.type) return false;

    if (id < other.id) return true;
    if (id > other.id) return false;

    return sub_id < other.sub_id;
  }
};

}  // namespace entry
}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_DATA_ENTRY_H_
