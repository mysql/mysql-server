/* Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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

#include "sql/dd/impl/types/library_impl.h"

#include "sql/dd/impl/tables/routines.h"     // Routines::update_object_key()
#include "sql/dd/impl/types/routine_impl.h"  // dd::Routine_impl
#include "sql/dd/string_type.h"              // dd::String_type
#include "sql/dd/types/library.h"            // dd::Library
#include "sql/dd/types/weak_object.h"        // dd::Weak_object

namespace dd {

///////////////////////////////////////////////////////////////////////////
// Library_impl implementation.
///////////////////////////////////////////////////////////////////////////
Library_impl::Library_impl() {
  Routine_impl::set_type(RT_LIBRARY);
  Routine_impl::set_deterministic(false);
  Routine_impl::set_sql_data_access(SDA_CONTAINS_SQL);
  Routine_impl::set_security_type(View::ST_INVOKER);
  Routine_impl::set_connection_collation_id(
      my_charset_utf8mb4_0900_ai_ci.number);
  Routine_impl::set_schema_collation_id(my_charset_utf8mb4_0900_ai_ci.number);
}

bool Library_impl::update_routine_name_key(Name_key *key, Object_id schema_id,
                                           const String_type &name) const {
  return Library::update_name_key(key, schema_id, name);
}

///////////////////////////////////////////////////////////////////////////

bool Library::update_name_key(Name_key *key, Object_id schema_id,
                              const String_type &name) {
  return tables::Routines::update_object_key(key, schema_id,
                                             Routine::RT_LIBRARY, name);
}

///////////////////////////////////////////////////////////////////////////

void Library_impl::debug_print(String_type &outb) const {
  dd::Stringstream_type ss;

  String_type s;
  Routine_impl::debug_print(s);

  ss << "LIBRARY OBJECT: { " << s << "} ";

  outb = ss.str();
}

///////////////////////////////////////////////////////////////////////////

Library_impl::Library_impl(const Library_impl &src)
    : Weak_object(src), Routine_impl(src) {}

///////////////////////////////////////////////////////////////////////////
// Private methods
///////////////////////////////////////////////////////////////////////////

[[nodiscard]] Library_impl *Library_impl::clone() const {
  return new Library_impl(*this);
}

// N.B.: returning dd::Library from this function might confuse MSVC
// compiler thanks to diamond inheritance.
[[nodiscard]] Library_impl *Library_impl::clone_dropped_object_placeholder()
    const {
  auto *placeholder = new Library_impl();
  placeholder->set_id(id());
  placeholder->set_schema_id(schema_id());
  placeholder->set_name(name());
  return placeholder;
}

}  // namespace dd
