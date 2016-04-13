/* Copyright (c) 2014, 2016, Oracle and/or its affiliates. All rights reserved.

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

#ifndef DD_TABLES__SCHEMATA_INCLUDED
#define DD_TABLES__SCHEMATA_INCLUDED

#include "my_global.h"

#include "dd/impl/types/dictionary_object_table_impl.h" // dd::Dictionary_obj...
#include "dd/impl/types/schema_impl.h"                  // dd::Schema_impl

#include <string>

namespace dd {
namespace tables {

///////////////////////////////////////////////////////////////////////////

class Schemata : public Dictionary_object_table_impl
{
public:
  static const Schemata &instance()
  {
    static Schemata *s_instance= new Schemata();
    return *s_instance;
  }

  static const std::string &table_name()
  {
    static std::string s_table_name("schemata");
    return s_table_name;
  }

public:
  enum enum_fields
  {
    FIELD_ID,
    FIELD_CATALOG_ID,
    FIELD_NAME,
    FIELD_DEFAULT_COLLATION_ID,
    FIELD_CREATED,
    FIELD_LAST_ALTERED
  };

public:
  Schemata()
  {
    m_target_def.table_name(table_name());
    m_target_def.dd_version(1);

    m_target_def.add_field(FIELD_ID,
                           "FIELD_ID",
                           "id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT");
    m_target_def.add_field(FIELD_CATALOG_ID, "FIELD_CATALOG_ID",
                           "catalog_id BIGINT UNSIGNED NOT NULL");
    m_target_def.add_field(FIELD_NAME,
                           "FIELD_NAME",
                           "name VARCHAR(64) NOT NULL COLLATE " +
                           std::string(Object_table_definition_impl::
                                         fs_name_collation()->name));
    m_target_def.add_field(FIELD_DEFAULT_COLLATION_ID,
                           "FIELD_DEFAULT_COLLATION_ID",
                           "default_collation_id BIGINT UNSIGNED NOT NULL");
    m_target_def.add_field(FIELD_CREATED,
                           "FIELD_CREATED",
                           "created TIMESTAMP NOT NULL\n"
                           " DEFAULT CURRENT_TIMESTAMP\n"
                           " ON UPDATE CURRENT_TIMESTAMP");
    m_target_def.add_field(FIELD_LAST_ALTERED,
                           "FIELD_LAST_ALTERED",
                           "last_altered TIMESTAMP NOT NULL DEFAULT NOW()");

    m_target_def.add_index("PRIMARY KEY (id)");
    m_target_def.add_index("UNIQUE KEY (catalog_id, name)");

    m_target_def.add_foreign_key("FOREIGN KEY (catalog_id) REFERENCES \
                                  catalogs(id)");
    m_target_def.add_foreign_key("FOREIGN KEY (default_collation_id) \
                                  REFERENCES collations(id)");

    m_target_def.add_option("ENGINE=INNODB");
    m_target_def.add_option("DEFAULT CHARSET=utf8");
    m_target_def.add_option("COLLATE=utf8_bin");
    m_target_def.add_option("ROW_FORMAT=DYNAMIC");
    m_target_def.add_option("STATS_PERSISTENT=0");
  }

  virtual const std::string &name() const
  { return Schemata::table_name(); }

  virtual Dictionary_object *create_dictionary_object(const Raw_record &) const
  { return new (std::nothrow) Schema_impl(); }

public:
  static bool update_object_key(Item_name_key *key,
                                Object_id catalog_id,
                                const std::string &schema_name);

  static Object_key *create_key_by_catalog_id(
    Object_id catalog_id);
};

///////////////////////////////////////////////////////////////////////////

}
}

#endif // DD_TABLES__SCHEMATA_INCLUDED
