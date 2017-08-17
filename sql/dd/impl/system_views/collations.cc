/* Copyright (c) 2017 Oracle and/or its affiliates. All rights reserved.

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

#include "sql/dd/impl/system_views/collations.h"

namespace dd {
namespace system_views {

const Collations &Collations::instance()
{
  static Collations *s_instance= new Collations();
  return *s_instance;
}

Collations::Collations()
{
  m_target_def.set_view_name(view_name());

  m_target_def.add_field(FIELD_COLLATION_NAME, "COLLATION_NAME", "col.name");
  m_target_def.add_field(FIELD_CHARACTER_SET_NAME, "CHARACTER_SET_NAME",
                         "cs.name");
  m_target_def.add_field(FIELD_ID, "ID", "col.id");
  m_target_def.add_field(FIELD_IS_DEFAULT, "IS_DEFAULT",
    "IF(EXISTS(SELECT * FROM mysql.character_sets "
    "          WHERE mysql.character_sets.default_collation_id= col.id),"
    "   'Yes','')");
  m_target_def.add_field(FIELD_IS_COMPILED, "IS_COMPILED",
                         "IF(col.is_compiled,'Yes','')");
  m_target_def.add_field(FIELD_SORTLEN, "SORTLEN", "col.sort_length");
  m_target_def.add_field(FIELD_PAD_ATTRIBUTE, "PAD_ATTRIBUTE",
                         "col.pad_attribute");

  m_target_def.add_from("mysql.collations col");
  m_target_def.add_from("JOIN mysql.character_sets cs ON "
                        "col.character_set_id=cs.id ");
}

}
}
