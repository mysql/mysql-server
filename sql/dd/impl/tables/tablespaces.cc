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

#include "dd/impl/tables/tablespaces.h"

#include "dd/properties.h"             // Needed for destructor
#include "dd/impl/raw/object_keys.h"   // dd::Global_name_key

namespace dd {
namespace tables {

///////////////////////////////////////////////////////////////////////////

bool Tablespaces::update_object_key(Global_name_key *key,
                                    const std::string &tablespace_name)
{
  key->update(FIELD_NAME, tablespace_name);
  return false;
}

///////////////////////////////////////////////////////////////////////////

}
}
