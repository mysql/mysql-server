/*
  Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_REST_ENTRY_APP_CONTENT_FILE_H_
#define ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_REST_ENTRY_APP_CONTENT_FILE_H_

#include <optional>
#include <string>

#include "mrs/database/entry/content_file.h"

namespace mrs {
namespace rest {
namespace entry {

struct AppContentFile : public mrs::database::entry::ContentFile {
  using ContentFile = mrs::database::entry::ContentFile;
  using EntryKey = database::entry::EntryKey;
  using EntryType = database::entry::EntryType;

  AppContentFile() {}
  explicit AppContentFile(const ContentFile &cf) : ContentFile(cf) {}

  EntryType key_entry_type{EntryType::key_static};
  uint64_t key_subtype{0};
  EntryKey get_key() const { return {key_entry_type, id, key_subtype}; }

  std::optional<std::string> content;
  std::optional<std::string> redirect;
  bool default_handling_directory_index{true};
  bool is_index{false};
};

}  // namespace entry
}  // namespace rest
}  // namespace mrs

#endif  // ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_REST_ENTRY_APP_CONTENT_FILE_H_
