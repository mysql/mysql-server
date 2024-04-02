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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_ENTRY_CONTENT_FILE_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_ENTRY_CONTENT_FILE_H_

#include <cstdint>
#include <string>

#include "mrs/database/entry/entry.h"
#include "mrs/database/entry/universal_id.h"

namespace mrs {
namespace database {
namespace entry {

struct ContentFile {
  UniversalId id;
  UniversalId content_set_id;
  UniversalId service_id;
  uint64_t size;
  std::string host;
  std::string service_path;
  std::string schema_path;
  std::string file_path;
  bool active_service;
  bool active_set;
  bool active_file;
  bool schema_requires_authentication;
  bool requires_authentication;
  bool deleted;
  std::string options_json_schema;
  std::string options_json_service;
};

}  // namespace entry
}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_ENTRY_CONTENT_FILE_H_
