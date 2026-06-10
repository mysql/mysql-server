/*
  Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_INCLUDE_MRS_DATABASE_ENTRY_COLUMN_TYPE_H_
#define ROUTER_SRC_MYSQL_REST_SERVICE_INCLUDE_MRS_DATABASE_ENTRY_COLUMN_TYPE_H_

namespace mrs {
namespace database {
namespace entry {

/*
 * Define minimal set of types that are represented in JSON.
 *
 * The list doesn't correspond exactly to types that can be
 * stored inside JSON value, additionally it put some constrains
 * on some JSON types.
 *
 * additional constrains
 * =====================
 *  * BINARY - json string, where content is stored as base64
 *             encoded value.
 *  * GEOMETRY - json object, which corresponds to GeoJSON.
 *  * VECTOR - json array of double values.
 */

enum class ColumnType {
  UNKNOWN,
  INTEGER,
  DOUBLE,
  BOOLEAN,
  STRING,
  BINARY,
  GEOMETRY,
  JSON,
  VECTOR
};

}  // namespace entry
}  // namespace database
}  // namespace mrs

#endif /* ROUTER_SRC_MYSQL_REST_SERVICE_INCLUDE_MRS_DATABASE_ENTRY_COLUMN_TYPE_H_ \
        */
