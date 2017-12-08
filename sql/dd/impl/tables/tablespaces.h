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

#ifndef DD_TABLES__TABLESPACES_INCLUDED
#define DD_TABLES__TABLESPACES_INCLUDED

#include <string>

#include "sql/dd/impl/types/entity_object_table_impl.h"
#include "sql/dd/string_type.h"
#include "sql/dd/types/tablespace.h"

namespace dd {
class Global_name_key;
class Raw_record;

namespace tables {

///////////////////////////////////////////////////////////////////////////

class Tablespaces : public Entity_object_table_impl
{
public:
  static const Tablespaces &instance();

  enum enum_fields
  {
    FIELD_ID,
    FIELD_NAME,
    FIELD_OPTIONS,
    FIELD_SE_PRIVATE_DATA,
    FIELD_COMMENT,
    FIELD_ENGINE
  };

  enum enum_indexes
  {
    INDEX_PK_ID= static_cast<uint>(Common_index::PK_ID),
    INDEX_UK_NAME= static_cast<uint>(Common_index::UK_NAME)
  };

  enum enum_foreign_keys
  {
  };

  Tablespaces();

  virtual Tablespace *create_entity_object(const Raw_record &) const;

  static bool update_object_key(Global_name_key *key,
                                const String_type &tablespace_name);
};

///////////////////////////////////////////////////////////////////////////

}
}

#endif // DD_TABLES__TABLESPACES_INCLUDED
