/* Copyright (c) 2020, 2023, Oracle and/or its affiliates.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql/dd/impl/system_views/tablespaces_extensions.h"
#include "sql/dd/string_type.h"

namespace {
enum { FIELD_TABLESPACE_NAME, FIELD_ENGINE_ATTRIBUTE };

const dd::String_type s_view_name{STRING_WITH_LEN("TABLESPACES_EXTENSIONS")};
const dd::system_views::Tablespaces_extensions *s_instance =
    new dd::system_views::Tablespaces_extensions(s_view_name);

}  // namespace

namespace dd {
namespace system_views {

const Tablespaces_extensions &Tablespaces_extensions::instance() {
  return *s_instance;
}

Tablespaces_extensions::Tablespaces_extensions(const dd::String_type &n) {
  m_target_def.set_view_name(n);

  // SELECT Identifier
  m_target_def.add_field(FIELD_TABLESPACE_NAME, "TABLESPACE_NAME", "tsps.name");

  // SELECT extension fields
  m_target_def.add_field(FIELD_ENGINE_ATTRIBUTE, "ENGINE_ATTRIBUTE",
                         "tsps.engine_attribute");

  // FROM
  m_target_def.add_from("mysql.tablespaces tsps");
}

const dd::String_type &Tablespaces_extensions::view_name() {
  return s_view_name;
}
}  // namespace system_views
}  // namespace dd
