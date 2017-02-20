/* Copyright (c) 2006, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef SQL_TABLE_INCLUDED
#define SQL_TABLE_INCLUDED

#include <stddef.h>
#include <sys/types.h>
#include <set>

#include "binary_log_types.h"  // enum_field_types
#include "my_inttypes.h"
#include "my_sharedlib.h"
#include "mysql/psi/mysql_mutex.h"
#ifndef WORKAROUND_TO_BE_REMOVED_ONCE_WL7016_IS_READY
#include "prealloced_array.h"
#endif

class Alter_info;
class Alter_table_ctx;
class Create_field;
class THD;
class handler;
struct TABLE;
struct TABLE_SHARE;
struct TABLE_LIST;
struct handlerton;
class KEY;
class FOREIGN_KEY;

typedef struct st_ha_check_opt HA_CHECK_OPT;
typedef struct st_ha_create_information HA_CREATE_INFO;
typedef struct st_lock_param_type ALTER_PARTITION_PARAM_TYPE;
typedef struct charset_info_st CHARSET_INFO;
typedef struct st_mysql_mutex mysql_mutex_t;
template<typename T> class List;


enum ddl_log_entry_code
{
  /*
    DDL_LOG_EXECUTE_CODE:
      This is a code that indicates that this is a log entry to
      be executed, from this entry a linked list of log entries
      can be found and executed.
    DDL_LOG_ENTRY_CODE:
      An entry to be executed in a linked list from an execute log
      entry.
    DDL_IGNORE_LOG_ENTRY_CODE:
      An entry that is to be ignored
  */
  DDL_LOG_EXECUTE_CODE = 'e',
  DDL_LOG_ENTRY_CODE = 'l',
  DDL_IGNORE_LOG_ENTRY_CODE = 'i'
};

enum ddl_log_action_code
{
  /*
    The type of action that a DDL_LOG_ENTRY_CODE entry is to
    perform.
    DDL_LOG_DELETE_ACTION:
      Delete an entity
    DDL_LOG_RENAME_ACTION:
      Rename an entity
    DDL_LOG_REPLACE_ACTION:
      Rename an entity after removing the previous entry with the
      new name, that is replace this entry.
    DDL_LOG_EXCHANGE_ACTION:
      Exchange two entities by renaming them a -> tmp, b -> a, tmp -> b.
  */
  DDL_LOG_DELETE_ACTION = 'd',
  DDL_LOG_RENAME_ACTION = 'r',
  DDL_LOG_REPLACE_ACTION = 's',
  DDL_LOG_EXCHANGE_ACTION = 'e'
};

enum enum_ddl_log_exchange_phase {
  EXCH_PHASE_NAME_TO_TEMP= 0,
  EXCH_PHASE_FROM_TO_NAME= 1,
  EXCH_PHASE_TEMP_TO_FROM= 2
};


typedef struct st_ddl_log_entry
{
  const char *name;
  const char *from_name;
  const char *handler_name;
  const char *tmp_name;
  uint next_entry;
  uint entry_pos;
  enum ddl_log_entry_code entry_type;
  enum ddl_log_action_code action_type;
  /*
    Most actions have only one phase. REPLACE does however have two
    phases. The first phase removes the file with the new name if
    there was one there before and the second phase renames the
    old name to the new name. EXCHANGE have three phases.
  */
  char phase;
} DDL_LOG_ENTRY;

typedef struct st_ddl_log_memory_entry
{
  uint entry_pos;
  struct st_ddl_log_memory_entry *next_log_entry;
  struct st_ddl_log_memory_entry *prev_log_entry;
  struct st_ddl_log_memory_entry *next_active_log_entry;
} DDL_LOG_MEMORY_ENTRY;


enum enum_explain_filename_mode
{
  EXPLAIN_ALL_VERBOSE= 0,
  EXPLAIN_PARTITIONS_VERBOSE,
  EXPLAIN_PARTITIONS_AS_COMMENT
};

/* Maximum length of GEOM_POINT Field */
#define MAX_LEN_GEOM_POINT_FIELD   25

#define WSDI_WRITE_SHADOW 1

/* Flags for conversion functions. */
static const uint FN_FROM_IS_TMP=  1 << 0;
static const uint FN_TO_IS_TMP=    1 << 1;
static const uint FN_IS_TMP=       FN_FROM_IS_TMP | FN_TO_IS_TMP;
/** Don't remove table in engine. Remove only .FRM and maybe .PAR files. */
static const uint NO_HA_TABLE=     1 << 2;
/** Don't check foreign key constraints while renaming table */
static const uint NO_FK_CHECKS=    1 << 3;
/**
  Don't commit transaction after updating data-dictionary while renaming
  the table.
*/
static const uint NO_DD_COMMIT=    1 << 4;


size_t filename_to_tablename(const char *from, char *to, size_t to_length
#ifndef DBUG_OFF
                           , bool stay_quiet = false
#endif /* DBUG_OFF */
                           );
size_t tablename_to_filename(const char *from, char *to, size_t to_length);
size_t build_table_filename(char *buff, size_t bufflen, const char *db,
                            const char *table, const char *ext,
                            uint flags, bool *was_truncated);
// For caller's who are mostly sure that path do not truncate
size_t inline build_table_filename(char *buff, size_t bufflen, const char *db,
                                   const char *table, const char *ext, uint flags)
{
    bool truncated_not_used;
    return build_table_filename(buff, bufflen, db, table, ext, flags,
                                &truncated_not_used);
}
size_t build_table_shadow_filename(char *buff, size_t bufflen,
                                   ALTER_PARTITION_PARAM_TYPE *lpt);
size_t build_tmptable_filename(THD* thd, char *buff, size_t bufflen);
bool mysql_create_table(THD *thd, TABLE_LIST *create_table,
                        HA_CREATE_INFO *create_info,
                        Alter_info *alter_info);
bool mysql_create_table_no_lock(THD *thd, const char *db,
                                const char *table_name,
                                HA_CREATE_INFO *create_info,
                                Alter_info *alter_info,
                                uint select_field_count,
                                bool *is_trans,
                                handlerton **post_ddl_ht);
bool mysql_discard_or_import_tablespace(THD *thd,
                                        TABLE_LIST *table_list);

/**
  Prepare Create_field and Key_spec objects for ALTER and upgrade.
  @param[in,out]  thd          thread handle. Used as a memory pool
                               and source of environment information.
  @param[in]      table        the source table, open and locked
                               Used as an interface to the storage engine
                               to acquire additional information about
                               the original table.
  @param[in,out]  create_info  A blob with CREATE/ALTER TABLE
                               parameters
  @param[in,out]  alter_info   Another blob with ALTER/CREATE parameters.
                               Originally create_info was used only in
                               CREATE TABLE and alter_info only in ALTER TABLE.
                               But since ALTER might end-up doing CREATE,
                               this distinction is gone and we just carry
                               around two structures.
  @param[in,out]  alter_ctx    Runtime context for ALTER TABLE.
  @param[in]      used_fields  used_fields from HA_CREATE_INFO.
  @param[in]      upgrade_flag True if upgrading data directory.

  @retval TRUE   error, out of memory or a semantical error in ALTER
                 TABLE instructions
  @retval FALSE  success

*/
bool prepare_fields_and_keys(THD *thd, TABLE *table,
                             HA_CREATE_INFO *create_info,
                             Alter_info *alter_info,
                             Alter_table_ctx *alter_ctx,
                             const uint &used_fields,
                             bool upgrade_flag);

bool mysql_prepare_alter_table(THD *thd, TABLE *table,
                               HA_CREATE_INFO *create_info,
                               Alter_info *alter_info,
                               Alter_table_ctx *alter_ctx);
bool mysql_trans_prepare_alter_copy_data(THD *thd);
bool mysql_trans_commit_alter_copy_data(THD *thd);
bool mysql_alter_table(THD *thd, const char *new_db, const char *new_name,
                       HA_CREATE_INFO *create_info,
                       TABLE_LIST *table_list,
                       Alter_info *alter_info);
bool mysql_compare_tables(TABLE *table,
                          Alter_info *alter_info,
                          HA_CREATE_INFO *create_info,
                          bool *metadata_equal);
bool mysql_recreate_table(THD *thd, TABLE_LIST *table_list, bool table_copy);
bool mysql_create_like_table(THD *thd, TABLE_LIST *table,
                             TABLE_LIST *src_table,
                             HA_CREATE_INFO *create_info);
bool mysql_rename_table(THD *thd, handlerton *base, const char *old_db,
                        const char * old_name, const char *new_db,
                        const char * new_name, uint flags);

bool mysql_checksum_table(THD* thd, TABLE_LIST* table_list,
                          HA_CHECK_OPT* check_opt);
bool mysql_rm_table(THD *thd,TABLE_LIST *tables, my_bool if_exists,
                    my_bool drop_temporary);
bool mysql_rm_table_no_locks(THD *thd, TABLE_LIST *tables, bool if_exists,
                             bool drop_temporary, bool drop_database,
                             bool *dropped_non_atomic_flag,
                             std::set<handlerton*> *post_ddl_htons
#ifndef WORKAROUND_TO_BE_REMOVED_ONCE_WL7016_IS_READY
                             , Prealloced_array<TABLE_LIST*, 1> *dropped_atomic
#endif
                             );
bool quick_rm_table(THD *thd, handlerton *base, const char *db,
                    const char *table_name, uint flags);
bool prepare_sp_create_field(THD *thd,
                             Create_field *field_def);
bool prepare_pack_create_field(THD *thd, Create_field *sql_field,
                               longlong table_flags);

const CHARSET_INFO* get_sql_field_charset(const Create_field *sql_field,
                                          const HA_CREATE_INFO *create_info);
bool validate_comment_length(THD *thd, const char *comment_str,
                             size_t *comment_len, uint max_len,
                             uint err_code, const char *comment_name);
bool mysql_update_dd(ALTER_PARTITION_PARAM_TYPE *lpt, uint flags);
int write_bin_log(THD *thd, bool clear_error,
                  const char *query, size_t query_length,
                  bool is_trans= FALSE);
bool write_ddl_log_entry(DDL_LOG_ENTRY *ddl_log_entry,
                           DDL_LOG_MEMORY_ENTRY **active_entry);
bool write_execute_ddl_log_entry(uint first_entry,
                                   bool complete,
                                   DDL_LOG_MEMORY_ENTRY **active_entry);
bool deactivate_ddl_log_entry(uint entry_no);
void release_ddl_log_memory_entry(DDL_LOG_MEMORY_ENTRY *log_entry);
void release_ddl_log();
void execute_ddl_log_recovery();
bool execute_ddl_log_entry(THD *thd, uint first_entry);

void promote_first_timestamp_column(List<Create_field> *column_definitions);


/**
  Prepares the column definitions for table creation.

  @param thd                       Thread object.
  @param create_info               Create information.
  @param[in,out] create_list       List of columns to create.
  @param[in,out] select_field_pos  Position where the SELECT columns start
                                   for CREATE TABLE ... SELECT.
  @param file                      The handler for the new table.
  @param[in,out] sql_field         Create_field to populate.
  @param field_no                  Column number.

  @retval false   OK
  @retval true    error
*/

bool prepare_create_field(THD *thd, HA_CREATE_INFO *create_info,
                          List<Create_field> *create_list,
                          int *select_field_pos, handler *file,
                          Create_field *sql_field, int field_no);


/**
  Prepares the table and key structures for table creation.

  @param thd                       Thread object.
  @param error_schema_name         Schema name of the table to create/alter, only
                                   error reporting.
  @param error_table_name          Name of table to create/alter, only used for
                                   error reporting.
  @param create_info               Create information (like MAX_ROWS).
  @param alter_info                List of columns and indexes to create
  @param file                      The handler for the new table.
  @param[out] key_info_buffer      An array of KEY structs for the indexes.
  @param[out] key_count            The number of elements in the array.
  @param[out] fk_key_info_buffer   An array of FOREIGN_KEY structs for the
                                   foreign keys.
  @param[out] fk_key_count         The number of elements in the array.
  @param[in] existing_fks          An array of pre-existing FOREIGN KEYS
                                   (in case of ALTER).
  @param[in] existing_fks_count    The number of pre-existing foreign keys.
  @param select_field_count        The number of fields coming from a select table.

  @retval false   OK
  @retval true    error
*/

bool mysql_prepare_create_table(THD *thd,
                                const char* error_schema_name,
                                const char* error_table_name,
                                HA_CREATE_INFO *create_info,
                                Alter_info *alter_info,
                                handler *file, KEY **key_info_buffer,
                                uint *key_count,
                                FOREIGN_KEY **fk_key_info_buffer,
                                uint *fk_key_count,
                                FOREIGN_KEY *existing_fks,
                                uint existing_fks_count,
                                int select_field_count);


size_t explain_filename(THD* thd, const char *from, char *to, size_t to_length,
                        enum_explain_filename_mode explain_mode);

void parse_filename(const char *filename, size_t filename_length,
                    const char ** schema_name, size_t *schema_name_length,
                    const char ** table_name, size_t *table_name_length,
                    const char ** partition_name, size_t *partition_name_length,
                    const char ** subpartition_name, size_t *subpartition_name_length);

extern MYSQL_PLUGIN_IMPORT const char *primary_key_name;
extern mysql_mutex_t LOCK_gdl;


/**
  Acquire metadata lock on triggers associated with a list of tables.

  @param[in] thd     Current thread context
  @param[in] tables  Tables for that associated triggers have to locked.

  @return Operation status.
    @retval false Success
    @retval true  Failure
*/

bool lock_trigger_names(THD *thd, TABLE_LIST *tables);

#endif /* SQL_TABLE_INCLUDED */
