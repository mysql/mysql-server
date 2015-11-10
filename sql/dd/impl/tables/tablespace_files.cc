/* Copyright (c) 2014, 2015 Oracle and/or its affiliates. All rights reserved.

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

#include "dd/impl/tables/tablespace_files.h"

#include "dd/impl/raw/object_keys.h"  // Parent_id_range_key

namespace dd {
namespace tables {

///////////////////////////////////////////////////////////////////////////

Object_key *Tablespace_files::create_key_by_tablespace_id(
  Object_id tablespace_id)
{
  return new (std::nothrow) Parent_id_range_key(0, FIELD_TABLESPACE_ID,
                                               tablespace_id);
}

///////////////////////////////////////////////////////////////////////////

Object_key *Tablespace_files::create_primary_key(
  Object_id tablespace_id, int ordinal_position)
{
  const int INDEX_NO= 0;

  return new (std::nothrow) Composite_pk(INDEX_NO,
                          FIELD_TABLESPACE_ID, tablespace_id,
                          FIELD_ORDINAL_POSITION, ordinal_position);
}

///////////////////////////////////////////////////////////////////////////

}
}
