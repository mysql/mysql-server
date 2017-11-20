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

#include "dd/impl/system_views/innodb_fields.h"

namespace dd {
namespace system_views {

const Innodb_fields &Innodb_fields::instance()
{
  static Innodb_fields *s_instance= new Innodb_fields();
  return *s_instance;
}

Innodb_fields::Innodb_fields()
{
  m_target_def.set_view_name(view_name());

  m_target_def.add_field(FIELD_INDEX_ID, "INDEX_ID",
                 "GET_DD_INDEX_PRIVATE_DATA(idx.se_private_data, 'id')");
  m_target_def.add_field(FIELD_NAME, "NAME", "col.name");
  m_target_def.add_field(FIELD_POS, "POS", "fld.ordinal_position - 1");

  m_target_def.add_from("mysql.index_column_usage fld");
  m_target_def.add_from("JOIN mysql.columns col ON fld.column_id=col.id");
  m_target_def.add_from("JOIN mysql.indexes idx ON fld.index_id=idx.id");
  m_target_def.add_from("JOIN mysql.tables tbl ON tbl.id=idx.table_id");

  m_target_def.add_where("NOT tbl.type = 'VIEW'");
  m_target_def.add_where("AND tbl.hidden = 'Visible'");
  m_target_def.add_where("AND NOT fld.hidden");
  m_target_def.add_where("AND tbl.se_private_id IS NOT NULL");
  m_target_def.add_where("AND tbl.engine='INNODB'");
}

}
}
