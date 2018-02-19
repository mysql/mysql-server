/* Copyright (c) 2005, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef SQL_SHOW_H
#define SQL_SHOW_H

#include <stddef.h>
#include <vector>

#include "binary_log_types.h"
#include "lex_string.h"
#include "m_ctype.h"
#include "my_inttypes.h"
#include "mysql/status_var.h"
#include "sql/handler.h"  // enum_schema_tables
#include "sql/set_var.h"  // enum_var_type
#include "sql/table.h"    // enum_schema_table_state
#include "sql_string.h"   // Simple_cstring
#include "typelib.h"

/* Forward declarations */
class JOIN;
class SELECT_LEX;
class THD;
class sp_name;
struct LEX;
struct MEM_ROOT;
struct System_status_var;
template <class T>
class List;

// TODO: allocator based on my_malloc.
typedef std::vector<SHOW_VAR> Status_var_array;
enum find_files_result { FIND_FILES_OK, FIND_FILES_OOM, FIND_FILES_DIR };

/* Used by handlers to store things in schema tables */
#define IS_FILES_FILE_ID 0
#define IS_FILES_FILE_NAME 1
#define IS_FILES_FILE_TYPE 2
#define IS_FILES_TABLESPACE_NAME 3
#define IS_FILES_TABLE_CATALOG 4
#define IS_FILES_TABLE_SCHEMA 5
#define IS_FILES_TABLE_NAME 6
#define IS_FILES_LOGFILE_GROUP_NAME 7
#define IS_FILES_LOGFILE_GROUP_NUMBER 8
#define IS_FILES_ENGINE 9
#define IS_FILES_FULLTEXT_KEYS 10
#define IS_FILES_DELETED_ROWS 11
#define IS_FILES_UPDATE_COUNT 12
#define IS_FILES_FREE_EXTENTS 13
#define IS_FILES_TOTAL_EXTENTS 14
#define IS_FILES_EXTENT_SIZE 15
#define IS_FILES_INITIAL_SIZE 16
#define IS_FILES_MAXIMUM_SIZE 17
#define IS_FILES_AUTOEXTEND_SIZE 18
#define IS_FILES_CREATION_TIME 19
#define IS_FILES_LAST_UPDATE_TIME 20
#define IS_FILES_LAST_ACCESS_TIME 21
#define IS_FILES_RECOVER_TIME 22
#define IS_FILES_TRANSACTION_COUNTER 23
#define IS_FILES_VERSION 24
#define IS_FILES_ROW_FORMAT 25
#define IS_FILES_TABLE_ROWS 26
#define IS_FILES_AVG_ROW_LENGTH 27
#define IS_FILES_DATA_LENGTH 28
#define IS_FILES_MAX_DATA_LENGTH 29
#define IS_FILES_INDEX_LENGTH 30
#define IS_FILES_DATA_FREE 31
#define IS_FILES_CREATE_TIME 32
#define IS_FILES_UPDATE_TIME 33
#define IS_FILES_CHECK_TIME 34
#define IS_FILES_CHECKSUM 35
#define IS_FILES_STATUS 36
#define IS_FILES_EXTRA 37

/* Define fields' indexes for COLUMNS of temporary tables */
#define TMP_TABLE_COLUMNS_COLUMN_NAME 0
#define TMP_TABLE_COLUMNS_COLUMN_TYPE 1
#define TMP_TABLE_COLUMNS_COLLATION_NAME 2
#define TMP_TABLE_COLUMNS_IS_NULLABLE 3
#define TMP_TABLE_COLUMNS_COLUMN_KEY 4
#define TMP_TABLE_COLUMNS_COLUMN_DEFAULT 5
#define TMP_TABLE_COLUMNS_EXTRA 6
#define TMP_TABLE_COLUMNS_PRIVILEGES 7
#define TMP_TABLE_COLUMNS_COLUMN_COMMENT 8
#define TMP_TABLE_COLUMNS_GENERATION_EXPRESSION 9

/* Define fields' indexes for KEYS of temporary tables */
#define TMP_TABLE_KEYS_TABLE_NAME 0
#define TMP_TABLE_KEYS_IS_NON_UNIQUE 1
#define TMP_TABLE_KEYS_INDEX_SCHEMA 2
#define TMP_TABLE_KEYS_INDEX_NAME 3
#define TMP_TABLE_KEYS_SEQ_IN_INDEX 4
#define TMP_TABLE_KEYS_COLUMN_NAME 5
#define TMP_TABLE_KEYS_COLLATION 6
#define TMP_TABLE_KEYS_CARDINALITY 7
#define TMP_TABLE_KEYS_SUB_PART 8
#define TMP_TABLE_KEYS_PACKED 9
#define TMP_TABLE_KEYS_IS_NULLABLE 10
#define TMP_TABLE_KEYS_INDEX_TYPE 11
#define TMP_TABLE_KEYS_COMMENT 12
#define TMP_TABLE_KEYS_INDEX_COMMENT 13
#define TMP_TABLE_KEYS_IS_VISIBLE 14

find_files_result find_files(THD *thd, List<LEX_STRING> *files, const char *db,
                             const char *path, const char *wild, bool dir,
                             MEM_ROOT *tmp_mem_root);

int store_create_info(THD *thd, TABLE_LIST *table_list, String *packet,
                      HA_CREATE_INFO *create_info_arg, bool show_database);

void append_identifier(THD *thd, String *packet, const char *name,
                       size_t length, const CHARSET_INFO *from_cs,
                       const CHARSET_INFO *to_cs);

void append_identifier(THD *thd, String *packet, const char *name,
                       size_t length);

inline void append_identifier(THD *thd, String *packet, Simple_cstring str) {
  append_identifier(thd, packet, str.ptr(), str.length());
}
void mysqld_list_fields(THD *thd, TABLE_LIST *table, const char *wild);
bool mysqld_show_create(THD *thd, TABLE_LIST *table_list);
bool mysqld_show_create_db(THD *thd, char *dbname, HA_CREATE_INFO *create);

void mysqld_list_processes(THD *thd, const char *user, bool verbose);
bool mysqld_show_privileges(THD *thd);
void calc_sum_of_all_status(System_status_var *to);
void append_definer(THD *thd, String *buffer, const LEX_CSTRING &definer_user,
                    const LEX_CSTRING &definer_host);
int add_status_vars(const SHOW_VAR *list);
void remove_status_vars(SHOW_VAR *list);
void init_status_vars();
void free_status_vars();
bool get_status_var(THD *thd, SHOW_VAR *list, const char *name,
                    char *const buff, enum_var_type var_type, size_t *length);
void reset_status_vars();
ulonglong get_status_vars_version(void);
bool show_create_trigger(THD *thd, const sp_name *trg_name);
void view_store_options(THD *thd, TABLE_LIST *table, String *buff);

void init_fill_schema_files_row(TABLE *table);
bool schema_table_store_record(THD *thd, TABLE *table);

/**
  Store record to I_S table, convert HEAP table to InnoDB table if necessary.

  @param[in]  thd            thread handler
  @param[in]  table          Information schema table to be updated
  @param[in]  make_ondisk    if true, convert heap table to on disk table.
                             default value is true.
  @return 0 on success
  @return error code on failure.
*/
int schema_table_store_record2(THD *thd, TABLE *table, bool make_ondisk);

/**
  Convert HEAP table to InnoDB table if necessary

  @param[in] thd     thread handler
  @param[in] table   Information schema table to be converted.
  @param[in] error   the error code returned previously.
  @return false on success, true on error.
*/
bool convert_heap_table_to_ondisk(THD *thd, TABLE *table, int error);
void initialize_information_schema_acl();
int make_table_list(THD *thd, SELECT_LEX *sel, const LEX_CSTRING &db_name,
                    const LEX_CSTRING &table_name);

ST_SCHEMA_TABLE *find_schema_table(THD *thd, const char *table_name);
ST_SCHEMA_TABLE *get_schema_table(enum enum_schema_tables schema_table_idx);
int make_schema_select(THD *thd, SELECT_LEX *sel,
                       enum enum_schema_tables schema_table_idx);
int mysql_schema_table(THD *thd, LEX *lex, TABLE_LIST *table_list);
bool get_schema_tables_result(JOIN *join,
                              enum enum_schema_table_state executed_place);
enum enum_schema_tables get_schema_table_idx(ST_SCHEMA_TABLE *schema_table);

const char *get_one_variable(THD *thd, const SHOW_VAR *variable,
                             enum_var_type value_type, SHOW_TYPE show_type,
                             System_status_var *status_var,
                             const CHARSET_INFO **charset, char *buff,
                             size_t *length);

const char *get_one_variable_ext(THD *running_thd, THD *target_thd,
                                 const SHOW_VAR *variable,
                                 enum_var_type value_type, SHOW_TYPE show_type,
                                 System_status_var *status_var,
                                 const CHARSET_INFO **charset, char *buff,
                                 size_t *length);

/* These functions were under INNODB_COMPATIBILITY_HOOKS */
int get_quote_char_for_identifier(THD *thd, const char *name, size_t length);

bool try_acquire_high_prio_shared_mdl_lock(THD *thd, TABLE_LIST *table,
                                           bool can_deadlock);

void show_sql_type(enum_field_types type, uint16 metadata, String *str,
                   const CHARSET_INFO *field_cs = NULL);

extern TYPELIB grant_types;
#endif /* SQL_SHOW_H */
