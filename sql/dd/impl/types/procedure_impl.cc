/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include "sql/dd/impl/types/procedure_impl.h"

#include <sstream>
#include <string>

#include "sql/dd/impl/tables/routines.h"         // Routines
#include "sql/dd/impl/transaction_impl.h"        // Open_dictionary_tables_ctx
#include "sql/dd/string_type.h"                  // dd::String_type
#include "sql/dd/types/parameter.h"              // Parameter
#include "sql/dd/types/weak_object.h"

using dd::tables::Routines;

namespace dd {

///////////////////////////////////////////////////////////////////////////
// Procedure_impl implementation.
///////////////////////////////////////////////////////////////////////////

bool Procedure_impl::update_routine_name_key(Name_key *key,
                                             Object_id schema_id,
                                             const String_type &name) const
{
  return Procedure::update_name_key(key, schema_id, name);
}


///////////////////////////////////////////////////////////////////////////

bool Procedure::update_name_key(Name_key *key,
                                Object_id schema_id,
                                const String_type &name)
{
  return Routines::update_object_key(key,
                                     schema_id,
                                     Routine::RT_PROCEDURE,
                                     name);
}

///////////////////////////////////////////////////////////////////////////

void Procedure_impl::debug_print(String_type &outb) const
{
  dd::Stringstream_type ss;

  String_type s;
  Routine_impl::debug_print(s);

  ss
  << "PROCEDURE OBJECT: { "
  << s
  << "} ";

  outb= ss.str();
}

///////////////////////////////////////////////////////////////////////////

Procedure_impl::Procedure_impl(const Procedure_impl &src)
  :Weak_object(src), Routine_impl(src)
{ }

///////////////////////////////////////////////////////////////////////////

}
