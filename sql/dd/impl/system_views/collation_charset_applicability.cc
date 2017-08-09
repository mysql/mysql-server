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

#include "sql/dd/impl/system_views/collation_charset_applicability.h"

namespace dd {
namespace system_views {

const Collation_charset_applicability &
  Collation_charset_applicability::instance()
{
  static Collation_charset_applicability *s_instance= new
    Collation_charset_applicability();
  return *s_instance;
}

Collation_charset_applicability::Collation_charset_applicability()
{
  m_target_def.set_view_name(view_name());

  m_target_def.add_field(FIELD_COLLATION_NAME, "COLLATION_NAME", "col.name");
  m_target_def.add_field(FIELD_CHARACTER_SET_NAME, "CHARACTER_SET_NAME",
                         "cs.name");

  m_target_def.add_from("mysql.character_sets cs");
  m_target_def.add_from("JOIN mysql.collations col ON "
                        "cs.id = col.character_set_id ");
}

}
}
