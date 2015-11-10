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

#include "dd/impl/tables/index_column_usage.h"

#include "dd/impl/raw/object_keys.h"  // dd::Parent_id_range_key

namespace dd {
namespace tables {

///////////////////////////////////////////////////////////////////////////

Object_key *Index_column_usage::create_key_by_index_id(
  Object_id index_id)
{
  return new (std::nothrow) Parent_id_range_key(0, FIELD_INDEX_ID, index_id);
}

///////////////////////////////////////////////////////////////////////////

Object_key *Index_column_usage::create_primary_key(
  Object_id index_id, int ordinal_position)
{
  const int INDEX_NO= 0;

  return new (std::nothrow) Composite_pk(INDEX_NO,
                          FIELD_INDEX_ID, index_id,
                          FIELD_ORDINAL_POSITION, ordinal_position);
}

///////////////////////////////////////////////////////////////////////////

}
}
