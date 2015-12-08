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

#ifndef DD_TABLES__TABLESPACES_INCLUDED
#define DD_TABLES__TABLESPACES_INCLUDED

#include "my_global.h"

#include "dd/impl/types/dictionary_object_table_impl.h" // dd::Dictionary_obj...
#include "dd/impl/types/object_table_impl.h"            // dd::Object_table_i...
#include "dd/impl/types/tablespace_impl.h"              // dd::Tablespace_impl

DD_HEADER_BEGIN

namespace dd {
namespace tables {

///////////////////////////////////////////////////////////////////////////

class Tablespaces : virtual public Dictionary_object_table_impl,
                    virtual public Object_table_impl
{
public:
  static const Tablespaces &instance()
  {
    static Tablespaces s_instance;
    return s_instance;
  }

  static const std::string &table_name()
  {
    static std::string s_table_name("tablespaces");
    return s_table_name;
  }

public:
  enum enum_fields
  {
    FIELD_ID,
    FIELD_NAME,
    FIELD_OPTIONS,
    FIELD_SE_PRIVATE_DATA,
    FIELD_COMMENT,
    FIELD_ENGINE
  };

public:
  Tablespaces()
  {
    m_target_def.table_name(table_name());

    m_target_def.add_field(FIELD_ID,
                           "FIELD_ID",
                           "id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT");
    m_target_def.add_field(FIELD_NAME,
                           "FIELD_NAME",
                           "name VARCHAR(255) NOT NULL COLLATE utf8_bin");
    m_target_def.add_field(FIELD_OPTIONS,
                           "FIELD_OPTIONS",
                           "options MEDIUMTEXT");
    m_target_def.add_field(FIELD_SE_PRIVATE_DATA,
                           "FIELD_SE_PRIVATE_DATA",
                           "se_private_data MEDIUMTEXT");
    m_target_def.add_field(FIELD_COMMENT,
                           "FIELD_COMMENT",
                           "comment VARCHAR(2048) NOT NULL");
    m_target_def.add_field(FIELD_ENGINE,
                           "FIELD_ENGINE",
                           "engine VARCHAR(64)");

    m_target_def.add_index("PRIMARY KEY(id)");
    m_target_def.add_index("UNIQUE KEY(name)");

    m_target_def.add_option("ENGINE=INNODB");
    m_target_def.add_option("DEFAULT CHARSET=utf8");
    m_target_def.add_option("COLLATE=utf8_bin");
    m_target_def.add_option("STATS_PERSISTENT=0");
  }

  virtual const std::string &name() const
  { return Tablespaces::table_name(); }

  virtual Dictionary_object *create_dictionary_object(const Raw_record &) const
  { return new (std::nothrow) Tablespace_impl(); }

public:
  static bool update_object_key(Global_name_key *key,
                                const std::string &tablespace_name);
};

///////////////////////////////////////////////////////////////////////////

}
}

DD_HEADER_END

#endif // DD_TABLES__TABLESPACES_INCLUDED
