/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_ENTRY_AUTH_PRIVILEGE_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_ENTRY_AUTH_PRIVILEGE_H_

#include <cstdint>

#include "mrs/database/entry/set_operation.h"
#include "mrs/database/entry/universal_id.h"

#include "helper/optional.h"

namespace mrs {
namespace database {
namespace entry {

/**
 Representation of  entries in `auth_privilege`..

 This class doesn't have row-id, primary key etc, because it may represent
 several aggregated entries.
 */
class AuthPrivilege {
 public:
  helper::Optional<UniversalId> service_id;
  helper::Optional<UniversalId> schema_id;
  helper::Optional<UniversalId> object_id;

  Operation::ValueType crud;
};

}  // namespace entry
}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_ENTRY_AUTH_PRIVILEGE_H_
