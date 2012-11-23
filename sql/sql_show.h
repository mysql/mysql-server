/*
   Copyright (c) 2005-2008 MySQL AB, 2008 Sun Microsystems, Inc.
   Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef SQL_SHOW_H
#define SQL_SHOW_H

/* Forward declarations */
class String;
class THD;
struct st_ha_create_information;
typedef st_ha_create_information HA_CREATE_INFO;
struct TABLE_LIST;

#ifndef MCP_WL1735
typedef struct st_lookup_field_values
{
  LEX_STRING db_value, table_value;
  bool wild_db_value, wild_table_value;
} LOOKUP_FIELD_VALUES;
#endif

enum find_files_result {
  FIND_FILES_OK,
  FIND_FILES_OOM,
  FIND_FILES_DIR
};

#ifndef MCP_WL1735
int make_db_list(THD *thd, List<LEX_STRING> *files,
                 LOOKUP_FIELD_VALUES *lookup_field_vals,
                 bool *with_i_schema);
#endif

find_files_result find_files(THD *thd, List<LEX_STRING> *files, const char *db,
                             const char *path, const char *wild, bool dir);

int store_create_info(THD *thd, TABLE_LIST *table_list, String *packet,
                      HA_CREATE_INFO  *create_info_arg, bool show_database);
int view_store_create_info(THD *thd, TABLE_LIST *table, String *buff);

int copy_event_to_schema_table(THD *thd, TABLE *sch_table, TABLE *event_table);
int get_quote_char_for_identifier(THD *thd, const char *name, uint length);

#endif /* SQL_SHOW_H */
