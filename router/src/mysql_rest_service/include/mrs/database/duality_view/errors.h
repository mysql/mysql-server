/*
 * Copyright (c) 2024, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
 *
 * This program is distributed in the hope that it will be useful,  but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 * the GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_DUALITY_VIEW_ERRORS_H_
#define ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_DUALITY_VIEW_ERRORS_H_

#include <stdexcept>
#include <string>

namespace mrs {
namespace database {

class JSONInputError : public std::runtime_error {
 public:
  explicit JSONInputError(const std::string &msg)
      : std::runtime_error("Invalid input JSON document: " + msg) {}
};

inline void throw_invalid_field(const std::string &table,
                                const std::string &field) {
  throw JSONInputError("Invalid field \"" + field + "\" in table `" + table +
                       "` in JSON input");
}

inline void throw_missing_field(const std::string &table,
                                const std::string &field) {
  throw JSONInputError("Field \"" + field + "\" for table `" + table +
                       "` missing in JSON input");
}

inline void throw_missing_id(const std::string &table) {
  throw JSONInputError("ID for table `" + table + "` missing in JSON input");
}

inline void throw_mismatching_id(const std::string &table,
                                 const std::string &column) {
  throw JSONInputError("Value for column `" + column + "` of table `" + table +
                       "` does not match referenced ID");
}

inline void throw_immutable_id(const std::string &table) {
  throw JSONInputError("ID for table `" + table + "` cannot be changed");
}

inline void throw_invalid_type(const std::string &table,
                               const std::string &field = "") {
  if (field.empty())
    throw JSONInputError("Invalid document in JSON input for table `" + table +
                         "`");
  else
    throw JSONInputError("Invalid value for \"" + field + "\" for table `" +
                         table + "` in JSON input");
}

inline void throw_duplicate_key(const std::string &table,
                                const std::string &field) {
  throw JSONInputError("Duplicate keys in \"" + field + "\" for table `" +
                       table + "` in JSON input");
}

class DualityViewError : public std::runtime_error {
 public:
  explicit DualityViewError(const std::string &msg) : std::runtime_error(msg) {}
};

inline void throw_ENOINSERT(const std::string &table) {
  throw DualityViewError("Duality View does not allow INSERT for table `" +
                         table + "`");
}

inline void throw_ENOUPDATE(const std::string &table,
                            const std::string &field = "") {
  if (field.empty())
    throw DualityViewError("Duality View does not allow UPDATE for table `" +
                           table + "`");
  else
    throw DualityViewError("Duality View does not allow UPDATE for field \"" +
                           field + "\" of table `" + table + "`");
}

inline void throw_ENODELETE(const std::string &table = "") {
  if (table.empty())
    throw DualityViewError(
        "Duality View does not allow DELETE for a referenced table");
  else
    throw DualityViewError("Duality View does not allow DELETE for table `" +
                           table + "`");
}

inline void throw_read_only() {
  throw DualityViewError("Duality View is read-only");
}

}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_DUALITY_VIEW_ERRORS_H_
