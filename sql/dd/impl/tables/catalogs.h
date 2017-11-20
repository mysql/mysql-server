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

  static const String_type &table_name()
  {
    static String_type s_table_name("catalogs");
    return s_table_name;
  }

public:
  Catalogs()
  {
    m_target_def.table_name(table_name());
    m_target_def.dd_version(1);

    m_target_def.add_field(0, "FIELD_ID",
            "id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT");
    m_target_def.add_field(1, "FIELD_NAME",
            "name VARCHAR(64) NOT NULL COLLATE " +
            String_type(Object_table_definition_impl::
                        fs_name_collation()->name));
    m_target_def.add_field(2, "FIELD_CREATED",
            "created TIMESTAMP NOT NULL\n"
            "  DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP");
    m_target_def.add_field(3, "FIELD_LAST_ALTERED",
            "last_altered TIMESTAMP NOT NULL DEFAULT NOW()");

    m_target_def.add_index("PRIMARY KEY (id)");
    m_target_def.add_index("UNIQUE KEY (name)");

    m_target_def.add_populate_statement(
      "INSERT INTO catalogs(id, name, created, last_altered) "
        "VALUES (1, 'def', now(), now())");
  }

  virtual const String_type &name() const
  { return Catalogs::table_name(); }
};

///////////////////////////////////////////////////////////////////////////

}
}

#endif // DD_TABLES__CATALOGS_INCLUDED
