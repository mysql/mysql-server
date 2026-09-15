/* Copyright (c) 2026, Oracle and/or its affiliates.

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
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef MRS_COMPONENT_EMBEDDED_MYSQL_SESSION_H
#define MRS_COMPONENT_EMBEDDED_MYSQL_SESSION_H

#include <memory>

namespace mysqlrouter {
class MySQLSession;
}  // namespace mysqlrouter

namespace mrs_component {

// Creates the component-only MySQLSession adapter used by shared MRS code.
// connect() opens an embedded server session instead of a network connection:
// the username parameter is the canonical MySQL auth id whose security context
// is attached, default_schema is still applied, and password/network
// destination parameters are intentionally ignored.
std::unique_ptr<mysqlrouter::MySQLSession> make_embedded_mysql_session(
    bool bypass_max_connections = false);

}  // namespace mrs_component

#endif  // MRS_COMPONENT_EMBEDDED_MYSQL_SESSION_H
