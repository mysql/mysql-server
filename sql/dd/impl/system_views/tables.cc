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

#include "sql/dd/impl/system_views/tables.h"

#include <string>

#include "sql/stateless_allocator.h"

namespace dd {
namespace system_views {

const Tables_base &Tables::instance()
{
  static Tables_base *s_instance= new Tables();
  return *s_instance;
}


Tables_base::Tables_base()
{
  m_target_def.add_field(FIELD_TABLE_CATALOG, "TABLE_CATALOG",
                         "cat.name" + m_target_def.fs_name_collation());
  m_target_def.add_field(FIELD_TABLE_SCHEMA, "TABLE_SCHEMA",
                         "sch.name" + m_target_def.fs_name_collation());
  m_target_def.add_field(FIELD_TABLE_NAME, "TABLE_NAME",
                         "tbl.name" + m_target_def.fs_name_collation());
  m_target_def.add_field(FIELD_TABLE_TYPE, "TABLE_TYPE", "tbl.type");
  m_target_def.add_field(FIELD_ENGINE, "ENGINE",
                         "IF(tbl.type = 'BASE TABLE', tbl.engine, NULL)");
  m_target_def.add_field(FIELD_VERSION, "VERSION",
                 "IF(tbl.type = 'VIEW', NULL, 10 /* FRM_VER_TRUE_VARCHAR */)");
  m_target_def.add_field(FIELD_ROW_FORMAT, "ROW_FORMAT", "tbl.row_format");

  m_target_def.add_field(FIELD_CREATE_TIME, "CREATE_TIME", "tbl.created");
  m_target_def.add_field(FIELD_TABLE_COLLATION, "TABLE_COLLATION", "col.name");
  m_target_def.add_field(FIELD_CREATE_OPTIONS, "CREATE_OPTIONS",
    "IF (tbl.type = 'VIEW', NULL,"
    "  GET_DD_CREATE_OPTIONS(tbl.options,"
    "  IF(IFNULL(tbl.partition_expression, 'NOT_PART_TBL')='NOT_PART_TBL',"
    "     0, 1)))");
  m_target_def.add_field(FIELD_TABLE_COMMENT,"TABLE_COMMENT",
         "INTERNAL_GET_COMMENT_OR_ERROR(sch.name, tbl.name, tbl.type, "
                                       "tbl.options, tbl.comment)");

  m_target_def.add_from("mysql.tables tbl");
  m_target_def.add_from("JOIN mysql.schemata sch ON tbl.schema_id=sch.id");
  m_target_def.add_from("JOIN mysql.catalogs cat ON " "cat.id=sch.catalog_id");
  m_target_def.add_from("LEFT JOIN mysql.collations col ON "
                        "tbl.collation_id=col.id");

  m_target_def.add_where("CAN_ACCESS_TABLE(sch.name, tbl.name)");
  m_target_def.add_where("AND IS_VISIBLE_DD_OBJECT(tbl.hidden)");
}

/*
  Adding column definition so as to pick cached table statistics from
  mysql.table_stats.
*/
Tables::Tables()
{
  m_target_def.set_view_name(view_name());

  /*
    stat.<value> and stat.cached_time should be passed directly to UDFs
    and UDF implementation should handle NULL value. Using IFNULL() as a
    workaround until Bug#26389402 is fixed.
  */
  m_target_def.add_field(FIELD_TABLE_ROWS, "TABLE_ROWS",
                         "INTERNAL_TABLE_ROWS(sch.name, tbl.name,"
                         "  IF(ISNULL(tbl.partition_type), tbl.engine, ''),"
                         "     tbl.se_private_id, tbl.hidden != 'Visible', "
                         "     ts.se_private_data,"
                         "   IF(ISNULL(stat.table_rows), 0, stat.table_rows),"
                         "   IF(ISNULL(stat.cached_time), 0, stat.cached_time))");
  m_target_def.add_field(FIELD_AVG_ROW_LENGTH, "AVG_ROW_LENGTH",
                         "INTERNAL_AVG_ROW_LENGTH(sch.name, tbl.name,"
                         "  IF(ISNULL(tbl.partition_type), tbl.engine, ''),"
                         "     tbl.se_private_id, tbl.hidden != 'Visible', "
                         "     ts.se_private_data,"
                         "   IF(ISNULL(stat.avg_row_length),0, stat.avg_row_length),"
                         "   IF(ISNULL(stat.cached_time), 0, stat.cached_time))");
  m_target_def.add_field(FIELD_DATA_LENGTH, "DATA_LENGTH",
                         "INTERNAL_DATA_LENGTH(sch.name, tbl.name,"
                         "  IF(ISNULL(tbl.partition_type), tbl.engine, ''),"
                         "     tbl.se_private_id, tbl.hidden != 'Visible', "
                         "     ts.se_private_data,"
                         "   IF(ISNULL(stat.data_length), 0, stat.data_length),"
                         "   IF(ISNULL(stat.cached_time), 0, stat.cached_time))");
  m_target_def.add_field(FIELD_MAX_DATA_LENGTH, "MAX_DATA_LENGTH",
                         "INTERNAL_MAX_DATA_LENGTH(sch.name, tbl.name,"
                         "  IF(ISNULL(tbl.partition_type), tbl.engine, ''),"
                         "     tbl.se_private_id, tbl.hidden != 'Visible', "
                         "     ts.se_private_data,"
                         "   IF(ISNULL(stat.max_data_length), 0, stat.max_data_length),"
                         "   IF(ISNULL(stat.cached_time), 0, stat.cached_time))");
  m_target_def.add_field(FIELD_INDEX_LENGTH, "INDEX_LENGTH",
                         "INTERNAL_INDEX_LENGTH(sch.name, tbl.name,"
                         "  IF(ISNULL(tbl.partition_type), tbl.engine, ''),"
                         "     tbl.se_private_id, tbl.hidden != 'Visible', "
                         "     ts.se_private_data,"
                         "   IF(ISNULL(stat.index_length), 0, stat.index_length),"
                         "   IF(ISNULL(stat.cached_time), 0, stat.cached_time))");
  m_target_def.add_field(FIELD_DATA_FREE, "DATA_FREE",
                         "INTERNAL_DATA_FREE(sch.name, tbl.name,"
                         "  IF(ISNULL(tbl.partition_type), tbl.engine, ''),"
                         "     tbl.se_private_id, tbl.hidden != 'Visible', "
                         "     ts.se_private_data,"
                         "   IF(ISNULL(stat.data_free), 0, stat.data_free),"
                         "   IF(ISNULL(stat.cached_time), 0, stat.cached_time))");
  m_target_def.add_field(FIELD_AUTO_INCREMENT, "AUTO_INCREMENT",
                         "INTERNAL_AUTO_INCREMENT(sch.name, tbl.name,"
                         "  IF(ISNULL(tbl.partition_type), tbl.engine, ''),"
                         "     tbl.se_private_id, tbl.hidden != 'Visible', "
                         "     ts.se_private_data,"
                         "   IF(ISNULL(stat.auto_increment), 0, stat.auto_increment),"
                         "   IF(ISNULL(stat.cached_time), 0, stat.cached_time),"
                         "     tbl.se_private_data)");
  m_target_def.add_field(FIELD_UPDATE_TIME, "UPDATE_TIME",
                         "INTERNAL_UPDATE_TIME(sch.name, tbl.name,"
                         "  IF(ISNULL(tbl.partition_type), tbl.engine, ''),"
                         "     tbl.se_private_id, tbl.hidden != 'Visible', "
                         "     ts.se_private_data,"
                         "   IF(ISNULL(stat.update_time), 0, stat.update_time),"
                         "   IF(ISNULL(stat.cached_time), 0, stat.cached_time))");
  m_target_def.add_field(FIELD_CHECK_TIME, "CHECK_TIME",
                         "INTERNAL_CHECK_TIME(sch.name, tbl.name,"
                         "  IF(ISNULL(tbl.partition_type), tbl.engine, ''),"
                         "     tbl.se_private_id, tbl.hidden != 'Visible', "
                         "     ts.se_private_data,"
                         "   IF(ISNULL(stat.check_time), 0, stat.check_time),"
                         "   IF(ISNULL(stat.cached_time), 0, stat.cached_time))");
  m_target_def.add_field(FIELD_CHECKSUM, "CHECKSUM",
                         "INTERNAL_CHECKSUM(sch.name, tbl.name,"
                         "  IF(ISNULL(tbl.partition_type), tbl.engine, ''),"
                         "     tbl.se_private_id, tbl.hidden != 'Visible', "
                         "     ts.se_private_data,"
                         "   IF(ISNULL(stat.checksum), 0, stat.checksum),"
                         "   IF(ISNULL(stat.cached_time), 0, stat.cached_time))");
  /*
    Supply mysql.tablespaces.se_private_data to internal functions
    INTERNAL_*(), which is used by SE to read the SE specific tablespace
    metadata when fetching table dynamic statistics. E.g., InnoDB would
    read the SE specific space_id from se_private_data column.
  */
  m_target_def.add_from("LEFT JOIN mysql.tablespaces ts ON "
                        "tbl.tablespace_id=ts.id");

  m_target_def.add_from("LEFT JOIN mysql.table_stats stat ON "
                        "tbl.name=stat.table_name "
                        "AND sch.name=stat.schema_name");
}

} // system_views
} // dd
