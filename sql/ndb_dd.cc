/*
   Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "ndb_dd.h"

#include "dd/impl/sdi.h"

#include <iostream> // cout

bool ndb_sdi_serialize(class THD *thd,
                       const dd::Table &table,
                       const char* schema_name,
                       dd::sdi_t& sdi)
{
  sdi = dd::serialize(thd, table, dd::String_type(schema_name));
  if (sdi.empty())
    return false; // Failed to serialize

  // Write the sdi string to stdout
  std::cout << "  sdi: '" << sdi.c_str() << "'" << std::endl;

  return true; // OK
}


