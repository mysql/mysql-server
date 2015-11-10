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

#ifndef DD_TABLES__TABLESPACE_FILES_INCLUDED
#define DD_TABLES__TABLESPACE_FILES_INCLUDED

#include "my_global.h"

#include "dd/object_id.h"                    // dd::Object_id
#include "dd/impl/types/object_table_impl.h" // dd::Object_table_impl

namespace dd {
  class Object_key;
namespace tables {

///////////////////////////////////////////////////////////////////////////

class Tablespace_files : virtual public Object_table_impl
{
public:
  static const Tablespace_files &instance()
  {
    static Tablespace_files s_instance;
    return s_instance;
  }

  static const std::string &table_name()
  {
    static std::string s_table_name("tablespace_files");
    return s_table_name;
  }

public:
  enum enum_fields
  {
    FIELD_TABLESPACE_ID,
    FIELD_ORDINAL_POSITION,
    FIELD_FILE_NAME,
    FIELD_SE_PRIVATE_DATA
  };

public:
  Tablespace_files()
  {
    m_target_def.table_name(table_name());

    m_target_def.add_field(FIELD_TABLESPACE_ID,
                           "FIELD_TABLESPACE_ID",
                           "tablespace_id BIGINT UNSIGNED NOT NULL");
    m_target_def.add_field(FIELD_ORDINAL_POSITION,
                           "FIELD_ORDINAL_POSITION",
                           "ordinal_position INT UNSIGNED NOT NULL");
    m_target_def.add_field(FIELD_FILE_NAME,
                           "FIELD_FILE_NAME",
                           "file_name VARCHAR(255) NOT NULL");
    m_target_def.add_field(FIELD_SE_PRIVATE_DATA,
                           "FIELD_SE_PRIVATE_DATA",
                           "se_private_data MEDIUMTEXT");

    m_target_def.add_index("UNIQUE KEY (tablespace_id, ordinal_position)");
    m_target_def.add_index("UNIQUE KEY (file_name)");

    m_target_def.add_foreign_key("FOREIGN KEY (tablespace_id) \
                                  REFERENCES tablespaces(id)");

    m_target_def.add_option("ENGINE=INNODB");
    m_target_def.add_option("DEFAULT CHARSET=utf8");
    m_target_def.add_option("COLLATE=utf8_bin");
    m_target_def.add_option("STATS_PERSISTENT=0");
  }

  virtual const std::string &name() const
  { return Tablespace_files::table_name(); }

public:
  static Object_key *create_key_by_tablespace_id(
    Object_id tablespace_id);

  static Object_key *create_primary_key(
    Object_id tablespace_id, int ordinal_position);
};

///////////////////////////////////////////////////////////////////////////

}
}

#endif // DD_TABLES__TABLESPACE_FILES_INCLUDED
