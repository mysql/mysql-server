/* Copyright (c) 2005, 2010, Oracle and/or its affiliates. All rights reserved.

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

#include "sql_list.h"                           /* List */
#include "handler.h"                            /* enum_schema_tables */
#include "table.h"                              /* enum_schema_table_state */

/* Forward declarations */
class JOIN;
class String;
class THD;
class sp_name;
struct TABLE_LIST;
struct st_ha_create_information;
typedef class st_select_lex SELECT_LEX;
typedef st_ha_create_information HA_CREATE_INFO;
struct LEX;
typedef struct st_mysql_show_var SHOW_VAR;
typedef struct st_schema_table ST_SCHEMA_TABLE;
struct TABLE;
typedef struct system_status_var STATUS_VAR;

enum find_files_result {
  FIND_FILES_OK,
  FIND_FILES_OOM,
  FIND_FILES_DIR
};

/* Define fields' indexes for COLUMNS table of I_S tables */
#define IS_COLUMNS_TABLE_CATALOG                0
#define IS_COLUMNS_TABLE_SCHEMA                 1
#define IS_COLUMNS_TABLE_NAME                   2
#define IS_COLUMNS_COLUMN_NAME                  3
#define IS_COLUMNS_ORDINAL_POSITION             4
#define IS_COLUMNS_COLUMN_DEFAULT               5
#define IS_COLUMNS_IS_NULLABLE                  6
#define IS_COLUMNS_DATA_TYPE                    7
#define IS_COLUMNS_CHARACTER_MAXIMUM_LENGTH     8
#define IS_COLUMNS_CHARACTER_OCTET_LENGTH       9
#define IS_COLUMNS_NUMERIC_PRECISION           10
#define IS_COLUMNS_NUMERIC_SCALE               11
#define IS_COLUMNS_DATETIME_PRECISION          12
#define IS_COLUMNS_CHARACTER_SET_NAME          13
#define IS_COLUMNS_COLLATION_NAME              14
#define IS_COLUMNS_COLUMN_TYPE                 15
#define IS_COLUMNS_COLUMN_KEY                  16
#define IS_COLUMNS_EXTRA                       17
#define IS_COLUMNS_PRIVILEGES                  18
#define IS_COLUMNS_COLUMN_COMMENT              19

/* Define fields' indexes for ROUTINES table of I_S tables */
#define IS_ROUTINES_SPECIFIC_NAME               0
#define IS_ROUTINES_ROUTINE_CATALOG             1
#define IS_ROUTINES_ROUTINE_SCHEMA              2
#define IS_ROUTINES_ROUTINE_NAME                3
#define IS_ROUTINES_ROUTINE_TYPE                4
#define IS_ROUTINES_DATA_TYPE                   5
#define IS_ROUTINES_CHARACTER_MAXIMUM_LENGTH    6
#define IS_ROUTINES_CHARACTER_OCTET_LENGTH      7
#define IS_ROUTINES_NUMERIC_PRECISION           8
#define IS_ROUTINES_NUMERIC_SCALE               9
#define IS_ROUTINES_DATETIME_PRECISION         10
#define IS_ROUTINES_CHARACTER_SET_NAME         11
#define IS_ROUTINES_COLLATION_NAME             12
#define IS_ROUTINES_DTD_IDENTIFIER             13
#define IS_ROUTINES_ROUTINE_BODY               14
#define IS_ROUTINES_ROUTINE_DEFINITION         15
#define IS_ROUTINES_EXTERNAL_NAME              16
#define IS_ROUTINES_EXTERNAL_LANGUAGE          17
#define IS_ROUTINES_PARAMETER_STYLE            18
#define IS_ROUTINES_IS_DETERMINISTIC           19
#define IS_ROUTINES_SQL_DATA_ACCESS            20
#define IS_ROUTINES_SQL_PATH                   21
#define IS_ROUTINES_SECURITY_TYPE              22
#define IS_ROUTINES_CREATED                    23
#define IS_ROUTINES_LAST_ALTERED               24
#define IS_ROUTINES_SQL_MODE                   25
#define IS_ROUTINES_ROUTINE_COMMENT            26
#define IS_ROUTINES_DEFINER                    27
#define IS_ROUTINES_CHARACTER_SET_CLIENT       28
#define IS_ROUTINES_COLLATION_CONNECTION       29
#define IS_ROUTINES_DATABASE_COLLATION         30


/* Define fields' indexes for PARAMETERS table of I_S tables */
#define IS_PARAMETERS_SPECIFIC_CATALOG          0
#define IS_PARAMETERS_SPECIFIC_SCHEMA           1
#define IS_PARAMETERS_SPECIFIC_NAME             2
#define IS_PARAMETERS_ORDINAL_POSITION          3
#define IS_PARAMETERS_PARAMETER_MODE            4
#define IS_PARAMETERS_PARAMETER_NAME            5
#define IS_PARAMETERS_DATA_TYPE                 6
#define IS_PARAMETERS_CHARACTER_MAXIMUM_LENGTH  7
#define IS_PARAMETERS_CHARACTER_OCTET_LENGTH    8
#define IS_PARAMETERS_NUMERIC_PRECISION         9
#define IS_PARAMETERS_NUMERIC_SCALE            10
#define IS_PARAMETERS_DATETIME_PRECISION       11
#define IS_PARAMETERS_CHARACTER_SET_NAME       12
#define IS_PARAMETERS_COLLATION_NAME           13
#define IS_PARAMETERS_DTD_IDENTIFIER           14
#define IS_PARAMETERS_ROUTINE_TYPE             15

/* Used by handlers to store things in schema tables */
#define IS_FILES_FILE_ID              0
#define IS_FILES_FILE_NAME            1
#define IS_FILES_FILE_TYPE            2
#define IS_FILES_TABLESPACE_NAME      3
#define IS_FILES_TABLE_CATALOG        4
#define IS_FILES_TABLE_SCHEMA         5
#define IS_FILES_TABLE_NAME           6
#define IS_FILES_LOGFILE_GROUP_NAME   7
#define IS_FILES_LOGFILE_GROUP_NUMBER 8
#define IS_FILES_ENGINE               9
#define IS_FILES_FULLTEXT_KEYS       10
#define IS_FILES_DELETED_ROWS        11
#define IS_FILES_UPDATE_COUNT        12
#define IS_FILES_FREE_EXTENTS        13
#define IS_FILES_TOTAL_EXTENTS       14
#define IS_FILES_EXTENT_SIZE         15
#define IS_FILES_INITIAL_SIZE        16
#define IS_FILES_MAXIMUM_SIZE        17
#define IS_FILES_AUTOEXTEND_SIZE     18
#define IS_FILES_CREATION_TIME       19
#define IS_FILES_LAST_UPDATE_TIME    20
#define IS_FILES_LAST_ACCESS_TIME    21
#define IS_FILES_RECOVER_TIME        22
#define IS_FILES_TRANSACTION_COUNTER 23
#define IS_FILES_VERSION             24
#define IS_FILES_ROW_FORMAT          25
#define IS_FILES_TABLE_ROWS          26
#define IS_FILES_AVG_ROW_LENGTH      27
#define IS_FILES_DATA_LENGTH         28
#define IS_FILES_MAX_DATA_LENGTH     29
#define IS_FILES_INDEX_LENGTH        30
#define IS_FILES_DATA_FREE           31
#define IS_FILES_CREATE_TIME         32
#define IS_FILES_UPDATE_TIME         33
#define IS_FILES_CHECK_TIME          34
#define IS_FILES_CHECKSUM            35
#define IS_FILES_STATUS              36
#define IS_FILES_EXTRA               37

find_files_result find_files(THD *thd, List<LEX_STRING> *files, const char *db,
                             const char *path, const char *wild, bool dir);

int store_create_info(THD *thd, TABLE_LIST *table_list, String *packet,
                      HA_CREATE_INFO  *create_info_arg, bool show_database);
int view_store_create_info(THD *thd, TABLE_LIST *table, String *buff);

int copy_event_to_schema_table(THD *thd, TABLE *sch_table, TABLE *event_table);
int get_quote_char_for_identifier(THD *thd, const char *name, uint length);

void append_identifier(THD *thd, String *packet, const char *name,
		       uint length);
inline void append_identifier(THD *thd, String *packet, Simple_cstring str)
{
  append_identifier(thd, packet, str.ptr(), static_cast<uint>(str.length()));
}
void mysqld_list_fields(THD *thd,TABLE_LIST *table, const char *wild);
bool mysqld_show_create(THD *thd, TABLE_LIST *table_list);
bool mysqld_show_create_db(THD *thd, char *dbname, HA_CREATE_INFO *create);

void mysqld_list_processes(THD *thd,const char *user,bool verbose);
int mysqld_show_status(THD *thd);
int mysqld_show_variables(THD *thd,const char *wild);
bool mysqld_show_storage_engines(THD *thd);
bool mysqld_show_privileges(THD *thd);
char *make_backup_log_name(char *buff, const char *name, const char* log_ext);
void calc_sum_of_all_status(STATUS_VAR *to);
void append_definer(THD *thd, String *buffer, const LEX_STRING *definer_user,
                    const LEX_STRING *definer_host);
int add_status_vars(SHOW_VAR *list);
void remove_status_vars(SHOW_VAR *list);
void init_status_vars();
void free_status_vars();
void reset_status_vars();
bool show_create_trigger(THD *thd, const sp_name *trg_name);
void view_store_options(THD *thd, TABLE_LIST *table, String *buff);

void init_fill_schema_files_row(TABLE* table);
bool schema_table_store_record(THD *thd, TABLE *table);
void initialize_information_schema_acl();

ST_SCHEMA_TABLE *find_schema_table(THD *thd, const char* table_name);
ST_SCHEMA_TABLE *get_schema_table(enum enum_schema_tables schema_table_idx);
int make_schema_select(THD *thd,  SELECT_LEX *sel,
                       enum enum_schema_tables schema_table_idx);
int mysql_schema_table(THD *thd, LEX *lex, TABLE_LIST *table_list);
bool get_schema_tables_result(JOIN *join,
                              enum enum_schema_table_state executed_place);
enum enum_schema_tables get_schema_table_idx(ST_SCHEMA_TABLE *schema_table);

/* These functions were under INNODB_COMPATIBILITY_HOOKS */
int get_quote_char_for_identifier(THD *thd, const char *name, uint length);

/* Handle the ignored database directories list for SHOW/I_S. */
bool ignore_db_dirs_init();
void ignore_db_dirs_free();
void ignore_db_dirs_reset();
bool ignore_db_dirs_process_additions();
bool push_ignored_db_dir(char *path);
extern char *opt_ignore_db_dirs;

#endif /* SQL_SHOW_H */
