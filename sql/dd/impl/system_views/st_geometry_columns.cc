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

#include "sql/dd/impl/system_views/st_geometry_columns.h"

namespace dd {
namespace system_views {

const St_geometry_columns &St_geometry_columns::instance()
{
  static St_geometry_columns *s_instance= new St_geometry_columns();
  return *s_instance;
}

St_geometry_columns::St_geometry_columns()
{
  m_target_def.set_view_name(view_name());

  m_target_def.add_field(FIELD_TABLE_CATALOG, "TABLE_CATALOG",
                         "cols.TABLE_CATALOG");
  m_target_def.add_field(FIELD_TABLE_SCHEMA, "TABLE_SCHEMA",
                         "cols.TABLE_SCHEMA");
  m_target_def.add_field(FIELD_TABLE_NAME, "TABLE_NAME", "cols.TABLE_NAME");
  m_target_def.add_field(FIELD_COLUMN_NAME, "COLUMN_NAME", "cols.COLUMN_NAME");
  m_target_def.add_field(FIELD_SRS_NAME, "SRS_NAME", "srs.SRS_NAME");
  m_target_def.add_field(FIELD_SRS_ID, "SRS_ID", "cols.SRS_ID");
  m_target_def.add_field(FIELD_GEOMETRY_TYPE_NAME, "GEOMETRY_TYPE_NAME",
                         "cols.DATA_TYPE");

  m_target_def.add_from("INFORMATION_SCHEMA.COLUMNS cols");
  m_target_def.add_from("LEFT JOIN "
                        "  INFORMATION_SCHEMA.ST_SPATIAL_REFERENCE_SYSTEMS srs "
                        "  ON (cols.SRS_ID = srs.SRS_ID)");

  m_target_def.add_where(
    "DATA_TYPE IN ('geometry','point','linestring','polygon', 'multipoint',"
    "              'multilinestring', 'multipolygon','geometrycollection')");

}

}
}
