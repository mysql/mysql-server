/* Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.

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

#include "dd/dd.h"

#include "dd/impl/dictionary_impl.h"                // dd::Dictionary_impl
#include "dd/impl/object_table_registry.h"          // dd::Object_table_registry
#include "dd/impl/cache/shared_dictionary_cache.h"  // dd::cache::Shared_...
#include "dd/impl/types/schema_impl.h"              // dd::Schema_impl

namespace dd {

bool init(bool install)
{
  bool error= Object_table_registry::init() ||
         Dictionary_impl::init(install);

  if (!error)
    cache::Shared_dictionary_cache::init();

  return error;
}


bool shutdown()
{
  cache::Shared_dictionary_cache::shutdown();
  return Dictionary_impl::shutdown();
}


Dictionary *get_dictionary()
{
  return Dictionary_impl::instance();
}


Schema *create_dd_schema()
{
  Schema_impl *schema=
    dynamic_cast<Schema_impl *> (
      dd::create_object<Schema>());

  schema->set_id(1); // The DD schema ID.

  return schema;
}

} // namespace dd
