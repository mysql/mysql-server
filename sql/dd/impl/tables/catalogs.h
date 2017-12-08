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

#ifndef DD_TABLES__CATALOGS_INCLUDED
#define DD_TABLES__CATALOGS_INCLUDED

#include "sql/dd/impl/tables/dd_properties.h"    // TARGET_DD_VERSION
#include "sql/dd/impl/types/object_table_impl.h"

namespace dd {

namespace tables {

///////////////////////////////////////////////////////////////////////////

class Catalogs: public Object_table_impl
{
public:
  static const Catalogs &instance()
  {
    static Catalogs *s_instance= new Catalogs();
    return *s_instance;
  }

  enum enum_fields
  {
    FIELD_ID= static_cast<uint>(Common_field::ID),
    FIELD_NAME,
    FIELD_CREATED,
    FIELD_LAST_ALTERED,
    FIELD_OPTIONS
  };

  enum enum_indexes
  {
    INDEX_PK_ID= static_cast<uint>(Common_index::PK_ID),
    INDEX_UK_NAME= static_cast<uint>(Common_index::UK_NAME)
  };

  Catalogs()
  {
    m_target_def.set_table_name("catalogs");

    m_target_def.add_field(FIELD_ID,
                           "FIELD_ID",
                           "id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT");
    m_target_def.add_field(FIELD_NAME,
                           "FIELD_NAME",
                           "name VARCHAR(64) NOT NULL COLLATE " +
                           String_type(Object_table_definition_impl::
                             fs_name_collation()->name));
    m_target_def.add_field(FIELD_CREATED,
                           "FIELD_CREATED",
                           "created TIMESTAMP NOT NULL\n"
                           "  DEFAULT CURRENT_TIMESTAMP"
                           "  ON UPDATE CURRENT_TIMESTAMP");
    m_target_def.add_field(FIELD_LAST_ALTERED,
                           "FIELD_LAST_ALTERED",
                           "last_altered TIMESTAMP NOT NULL DEFAULT NOW()");
    m_target_def.add_field(FIELD_OPTIONS,
                           "FIELD_OPTIONS",
                           "options MEDIUMTEXT");

    m_target_def.add_index(INDEX_PK_ID,
                           "INDEX_PK_ID",
                           "PRIMARY KEY (id)");
    m_target_def.add_index(INDEX_UK_NAME,
                           "INDEX_UK_NAME",
                           "UNIQUE KEY (name)");

    m_target_def.add_populate_statement(
      "INSERT INTO catalogs(id, name, options, created, last_altered) "
        "VALUES (1, 'def', NULL, now(), now())");
  }
};

///////////////////////////////////////////////////////////////////////////

}
}

#endif // DD_TABLES__CATALOGS_INCLUDED
