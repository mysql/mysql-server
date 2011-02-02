/* Copyright (c) 2010, 2011, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/**
  @file storage/perfschema/table_helper.cc
  Performance schema table helpers (implementation).
*/

#include "my_global.h"
#include "my_pthread.h"
#include "pfs_engine_table.h"
#include "table_helper.h"

int PFS_object_row::make_row(PFS_table_share *pfs)
{
  m_object_type= pfs->get_object_type();

  m_schema_name_length= pfs->m_schema_name_length;
  if (m_schema_name_length > sizeof(m_schema_name))
    return 1;
  if (m_schema_name_length > 0)
    memcpy(m_schema_name, pfs->m_schema_name, sizeof(m_schema_name));

  m_object_name_length= pfs->m_table_name_length;
  if (m_object_name_length > sizeof(m_object_name))
    return 1;
  if (m_object_name_length > 0)
    memcpy(m_object_name, pfs->m_table_name, sizeof(m_object_name));

  return 0;
}

void PFS_object_row::set_field(uint index, Field *f)
{
  switch(index)
  {
    case 0: /* OBJECT_TYPE */
      set_field_object_type(f, m_object_type);
      break;
    case 1: /* SCHEMA_NAME */
      PFS_engine_table::set_field_varchar_utf8(f, m_schema_name, m_schema_name_length);
      break;
    case 2: /* OBJECT_NAME */
      PFS_engine_table::set_field_varchar_utf8(f, m_object_name, m_object_name_length);
      break;
    default:
      DBUG_ASSERT(false);
  }
}

int PFS_index_row::make_row(PFS_table_share *pfs, uint table_index)
{
  if (m_object_row.make_row(pfs))
    return 1;

  if (table_index < MAX_KEY)
  {
    PFS_table_key *key= &pfs->m_keys[table_index];
    m_index_name_length= key->m_name_length;
    if (m_index_name_length > sizeof(m_index_name))
      return 1;
    memcpy(m_index_name, key->m_name, sizeof(m_index_name));
  }
  else
    m_index_name_length= 0;

  return 0;
}

void PFS_index_row::set_field(uint index, Field *f)
{
  switch(index)
  {
    case 0: /* OBJECT_TYPE */
    case 1: /* SCHEMA_NAME */
    case 2: /* OBJECT_NAME */
      m_object_row.set_field(index, f);
      break;
    case 3: /* INDEX_NAME */
      if (m_index_name_length > 0)
        PFS_engine_table::set_field_varchar_utf8(f, m_index_name, m_index_name_length);
      else
        f->set_null();
      break;
    default:
      DBUG_ASSERT(false);
  }
}

void set_field_object_type(Field *f, enum_object_type object_type)
{
  switch (object_type)
  {
  case OBJECT_TYPE_TABLE:
    PFS_engine_table::set_field_varchar_utf8(f, "TABLE", 5);
    break;
  case OBJECT_TYPE_TEMPORARY_TABLE:
    PFS_engine_table::set_field_varchar_utf8(f, "TEMPORARY TABLE", 15);
    break;
  default:
    DBUG_ASSERT(false);
  }
}

