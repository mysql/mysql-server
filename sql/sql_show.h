/* Copyright 2006-2008 MySQL AB, 2008 Sun Microsystems, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#ifndef SQL_SHOW_H
#define SQL_SHOW_H

/* Forward declarations */
class String;
class THD;
struct st_ha_create_information;
typedef st_ha_create_information HA_CREATE_INFO;
struct TABLE_LIST;

enum find_files_result {
  FIND_FILES_OK,
  FIND_FILES_OOM,
  FIND_FILES_DIR
};

find_files_result find_files(THD *thd, List<LEX_STRING> *files, const char *db,
                             const char *path, const char *wild, bool dir);

int store_create_info(THD *thd, TABLE_LIST *table_list, String *packet,
                      HA_CREATE_INFO  *create_info_arg, bool show_database);
int view_store_create_info(THD *thd, TABLE_LIST *table, String *buff);

int copy_event_to_schema_table(THD *thd, TABLE *sch_table, TABLE *event_table);

#endif /* SQL_SHOW_H */
