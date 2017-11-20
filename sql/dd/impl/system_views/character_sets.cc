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

#include "sql/dd/impl/system_views/character_sets.h"

namespace dd {
namespace system_views {

const Character_sets &Character_sets::instance()
{
  static Character_sets *s_instance= new Character_sets();
  return *s_instance;
}

Character_sets::Character_sets()
{
  m_target_def.set_view_name(view_name());

  m_target_def.add_field(FIELD_CHARACTER_SET_NAME, "CHARACTER_SET_NAME",
                         "cs.name");
  m_target_def.add_field(FIELD_DEFAULT_COLLATE_NAME, "DEFAULT_COLLATE_NAME",
                         "col.name");
  m_target_def.add_field(FIELD_DESCRIPTION, "DESCRIPTION", "cs.comment");
  m_target_def.add_field(FIELD_MAXLEN, "MAXLEN", "cs.mb_max_length");

  m_target_def.add_from("mysql.character_sets cs");
  m_target_def.add_from("JOIN mysql.collations col ON "
                        "cs.default_collation_id=col.id ");
}

}
}
