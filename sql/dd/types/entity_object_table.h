/* Copyright (c) 2014, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef DD__ENTITY_OBJECT_TABLE_INCLUDED
#define DD__ENTITY_OBJECT_TABLE_INCLUDED


#include "sql/dd/types/object_table.h" // dd::Object_table

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Entity_object;
class Open_dictionary_tables_ctx;
class Raw_record;

///////////////////////////////////////////////////////////////////////////

/**
  This class represents DD table like mysql.schemata,
  mysql.tables, mysql.tablespaces and more. These corresponds to
  base DD table where the Entity_object's are persisted.

  This class does not represent table like mysql.columns,
  mysql.indexes which hold metadata child objects object of
  mysql.tables and are not directly created/searched/dropped
  without accessing mysql.tables or dd::Table Dictionary object.
*/
class Entity_object_table : virtual public Object_table
{
public:
  virtual ~Entity_object_table()
  { };

  virtual Entity_object *create_entity_object(
    const Raw_record &record) const = 0;

  virtual bool restore_object_from_record(
    Open_dictionary_tables_ctx *otx,
    const Raw_record &record,
    Entity_object **o) const = 0;
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__ENTITY_OBJECT_TABLE_INCLUDED
