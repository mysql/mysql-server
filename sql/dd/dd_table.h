/* Copyright (c) 2015, 2017 Oracle and/or its affiliates. All rights reserved.

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

#ifndef DD_TABLE_INCLUDED
#define DD_TABLE_INCLUDED

#include <sys/types.h>
#include <string>

#include "binary_log_types.h"        // enum_field_types
#include "dd/types/column.h"         // dd::enum_column_types
#include "handler.h"                 // legacy_db_type
#include "my_global.h"
#include "prealloced_array.h"        // Prealloced_array
#include "sql_alter.h"               // Alter_info::enum_enable_or_disable
#include "system_variables.h"
#include "table.h"                   // ST_FIELD_INFO

class Create_field;
class FOREIGN_KEY;
class THD;
namespace dd {
class Abstract_table;
}  // namespace dd
struct TABLE_LIST;
struct TABLE_SHARE;

typedef struct st_ha_create_information HA_CREATE_INFO;
class KEY;
template <class T> class List;

namespace dd {
  class Trigger;
  class Table;

  enum class enum_table_type;
  namespace cache {
    class Dictionary_client;
  }

static const char FIELD_NAME_SEPARATOR_CHAR = ';';

/**
  Prepares a dd::Table object from mysql_prepare_create_table() output
  and updates DD tables. This function creates a user table, as opposed
  to create_table() which can handle system tables as well.

  @param thd            Thread handle
  @param schema_name    Schema name.
  @param table_name     Table name.
  @param create_info    HA_CREATE_INFO describing the table to be created.
  @param create_fields  List of fields for the table.
  @param keyinfo        Array with descriptions of keys for the table.
  @param keys           Number of keys.
  @param keys_onoff     keys ON or OFF
  @param fk_keyinfo     Array with descriptions of foreign keys for the table.
  @param fk_keys        Number of foreign keys.
  @param file           handler instance for the table.

  @retval 0 on success.
  @retval 1 on failure.
*/
bool create_dd_user_table(THD *thd,
                          const dd::String_type &schema_name,
                          const dd::String_type &table_name,
                          HA_CREATE_INFO *create_info,
                          const List<Create_field> &create_fields,
                          const KEY *keyinfo,
                          uint keys,
                          Alter_info::enum_enable_or_disable keys_onoff,
                          const FOREIGN_KEY *fk_keyinfo,
                          uint fk_keys,
                          handler *file);

/**
  Prepares a dd::Table object from mysql_prepare_create_table() output
  and updates DD tables accordingly.

  @param thd            Thread handle
  @param schema_name    Schema name.
  @param table_name     Table name.
  @param create_info    HA_CREATE_INFO describing the table to be created.
  @param create_fields  List of fields for the table.
  @param keyinfo        Array with descriptions of keys for the table.
  @param keys           Number of keys.
  @param keys_onoff     Enable or disable keys.
  @param fk_keyinfo     Array with descriptions of foreign keys for the table.
  @param fk_keys        Number of foreign keys.
  @param file           handler instance for the table.

  @retval 0 on success.
  @retval ER_CANT_CREATE_TABLE, ER_TABLE_EXISTS_ERROR on failure.
*/
bool create_table(THD *thd,
                  const dd::String_type &schema_name,
                  const dd::String_type &table_name,
                  HA_CREATE_INFO *create_info,
                  const List<Create_field> &create_fields,
                  const KEY *keyinfo, uint keys,
                  Alter_info::enum_enable_or_disable keys_onoff,
                  const FOREIGN_KEY *fk_keyinfo, uint fk_keys,
                  handler *file);

/**
  Prepares a dd::Table object for a temporary table from
  mysql_prepare_create_table() output. Doesn't update DD tables,
  instead returns dd::Table object to caller.

  @param thd            Thread handle.
  @param schema_name    Database name.
  @param table_name     Table name.
  @param create_info    HA_CREATE_INFO describing the table to be created.
  @param create_fields  List of fields for the table.
  @param keyinfo        Array with descriptions of keys for the table.
  @param keys           Number of keys.
  @param keys_onoff     Enable or disable keys.
  @param file           handler instance for the table.

  @returns constructed dd::Table object, or NULL in case of an error.
*/
dd::Table *create_tmp_table(THD *thd,
                            const dd::String_type &schema_name,
                            const dd::String_type &table_name,
                            HA_CREATE_INFO *create_info,
                            const List<Create_field> &create_fields,
                            const KEY *keyinfo, uint keys,
                            Alter_info::enum_enable_or_disable keys_onoff,
                            handler *file);

/**
  Add foreign keys and triggers to a given table. This is used by
  ALTER TABLE to restore existing foreign keys and triggers towards
  the end of the statement. This is needed to avoid problems with
  duplicate foreign key and trigger names while we have two definitions
  of the same table.

  @param thd            Thread handle.
  @param schema_name    Database name.
  @param table_name     Table name.
  @param fk_keyinfo     Array of foreign key information.
  @param fk_keys        Number of foreign keys to add.
  @param trg_info       Array of triggers to be added to the table.

  @retval false on success
  @retval true on failure
*/
bool add_foreign_keys_and_triggers(THD *thd,
                                   const dd::String_type &schema_name,
                                   const dd::String_type &table_name,
                                   const FOREIGN_KEY *fk_keyinfo, uint fk_keys,
                                   Prealloced_array<dd::Trigger*, 1> *trg_info);

//////////////////////////////////////////////////////////////////////////
// Function common to 'table' and 'view' objects
//////////////////////////////////////////////////////////////////////////

/* Remove table metadata from dd.tables */
template <typename T>
bool drop_table(THD *thd,
                const char *schema_name,
                const char *table_name);

/**
  Check if a table or view exists

  dd_table_exists < dd::Abstract_table > () sets exists=true if such a
  table or a view exists.

  When checking whether the table is in the new DD, we distinguish
  between DD and non-DD tables. A DD table is created during boot-
  strapping to generate the structures required for opening it. Thus,
  we have to pretend it does not exist to allow the CREATE TABLE
  statement to execute.

  dd_table_exists < dd::Table > () sets exists=true if such a table exists.

  dd_table_exists < dd::View > () sets exists=true if such a view exists.

  @param       client       The dictionary client.
  @param       schema_name  The schema in which the object should be defined.
  @param       name         The object name to search for.
  @param [out] exists       A boolean which is set to true if the object
                            is found.

  @retval      true         Failure (error has been reported).
  @retval      false        Success.
*/
template <typename T>
bool table_exists(dd::cache::Dictionary_client *client,
                  const char *schema_name,
                  const char *name,
                  bool *exists);

/**
  Rename a table or view in dd.tables.

  @param  thd                  The dictionary client.
  @param  from_schema_name     Schema of table/view to rename.
  @param  from_name            Table/view name to rename.
  @param  to_schema_name       New schema name.
  @param  to_name              New table/view name.
  @param  no_foreign_key_check Ignore foreign key check while rename, if true.
  @param  mark_as_hidden       Mark the new table as hidden, if true.

  @retval      true         Failure (error has been reported).
  @retval      false        Success.
*/
template <typename T>
bool rename_table(THD *thd,
                  const char *from_schema_name,
                  const char *from_name,
                  const char *to_schema_name,
                  const char *to_name,
                  bool no_foreign_key_check,
                  bool mark_as_hidden);

template <typename T>
inline bool rename_table(THD *thd,
                         const char *from_schema_name,
                         const char *from_name,
                         const char *to_schema_name,
                         const char *to_name)
{
  return dd::rename_table<T>(thd,
                             from_schema_name, from_name,
                             to_schema_name, to_name,
                             false, false);
}


//////////////////////////////////////////////////////////////////////////
// Functions for retrieving, inspecting and manipulating instances of
// dd::Abstract_table, dd::Table and dd::View
//////////////////////////////////////////////////////////////////////////

/**
  Get which table type the given table name refers to.

  This function does not set error codes beyond what is set by the
  functions it calls.

  @param[in]  client      Dictionary client.
  @param[in]  schema_name Name of the schema
  @param[in]  table_name  Name of the table or view
  @param[out] table_type  Table, user view or system view

  @retval     true        Error, e.g. name is not a view nor a table.
                          In this case, the value of table_type is
                          undefined.
  @retval     false       Success
*/
bool abstract_table_type(dd::cache::Dictionary_client *client,
                         const char *schema_name, const char *table_name,
                         dd::enum_table_type *table_type);

/**
  Get the legacy db type from the options of the given table.

  This function does not set error codes beyond what is set by the
  functions it calls.

  @param[in]  thd             Thread context
  @param[in]  schema_name     Name of the schema
  @param[in]  table_name      Name of the table
  @param[out] db_type         Value of the legacy db type option

  @retval     true            Error, e.g. name is not a table, or no
                              legacy_db_type in the table's options.
                              In this case, the value of db_type is
                              undefined.
  @retval     false           Success
*/
bool table_legacy_db_type(THD *thd, const char *schema_name,
                          const char *table_name,
                          enum legacy_db_type *db_type);

/**
  Get the storage engine handlerton for the given table. If the
  table object contains a valid (i.e., not dynamically assigned)
  legacy_db_type as one of the options, use it to get hold of the
  handlerton. Otherwise, use the engine name directly.

  This function sets explicit error codes if:
  - The table is not found: ER_NO_SUCH_TABLE
  - The SE is invalid:      ER_STORAGE_ENGINE_NOT_LOADED

  @pre There must be at least a shared MDL lock on the table.

  @param[in]  thd             Thread context
  @param[in]  table_list      Table to check
  @param[out] hton            Handlerton for the table's storage engine

  @retval     true            Error, e.g. name is not a table, or no
                              valid engine specified. In this case, the
                              value of hton is undefined.
  @retval     false           Success
 */
bool table_storage_engine(THD *thd, const TABLE_LIST *table_list,
                          handlerton **hton);

/**
  For a given table name, check if the storage engine for the
  table supports an option 'flag'. Fail if the table does not
  exist or is not a base table.

  This function does not set error codes beyond what is set by the
  functions it calls.

  @param[in]    thd         Thread context
  @param[in]    table_list  Table to check
  @param[in]    flag        The option to check
  @param[out]   yes_no      Option support, undefined if error

  @retval       true        Error, 'yes_no' is undefined
  @retval       false       Success, 'yes_no' indicates option support
*/
bool check_storage_engine_flag(THD *thd, const TABLE_LIST *table_list,
                               uint32 flag, bool *yes_no);

/**
  Regenerate a metadata locked table.

  This function does not set error codes beyond what is set by the
  functions it calls.

  @pre There must be an exclusive MDL lock on the table.

  @param[in]    thd         Thread context
  @param[in]    schema_name Name of the schema
  @param[in]    table_name  Name of the table

  @retval       false       Success
  @retval       true        Error
*/
bool recreate_table(THD *thd, const char *schema_name,
                    const char *table_name);

/**
  Update dd::Table::options keys_disabled=0/1 based on ALTER TABLE
  ENABLE/DISABLE KEYS. This will be used by INFORMATION_SCHEMA.STATISTICS system
  view.

  @param[in]    thd         Thread context
  @param[in]    schema_name Name of the schema
  @param[in]    table_name  Name of the table
  @param[in]    keys_onoff  Wheather keys are enabled or disabled.

  @retval       false       Success
  @retval       true        Error
*/

bool update_keys_disabled(THD *thd,
                          const char *schema_name,
                          const char *table_name,
                          Alter_info::enum_enable_or_disable keys_onoff);

/**
  Function prepares string representing columns data type.
  This is required for IS implementation which uses views on DD tables
*/
String_type get_sql_type_by_field_info(THD *thd,
                                       enum_field_types field_type,
                                       uint32 field_length,
                                       const CHARSET_INFO *field_charset);

/**
  Convert field type from MySQL server type to new enum types in DD.
  We have plans to retain both old and new enum values in DD tables so as
  to handle client compatibility and information schema requirements.

  @param[in]    type   MySQL server field type.

  @retval  field type used by DD framework.
*/

enum_column_types get_new_field_type(enum_field_types type);

/**
  Update real row format for the table in the data-dictionary with
  value from the storage engine.

  @pre There must be an exclusive MDL lock on the table.

  @param[in]    thd         Thread context.
  @param[in]    share       TABLE_SHARE for the table.

  @retval       false       Success
  @retval       true        Error
*/
bool fix_row_type(THD *thd, TABLE_SHARE *share);

/**
  Update row format for the table with the value
  value supplied by caller function.

  @pre There must be an exclusive MDL lock on the table.

  @param[in]    thd              Thread context.
  @param[in]    share            TABLE_SHARE for the table.
  @param[in]    correct_row_type row_type to be set.

  @retval       false       Success
  @retval       true        Error
*/
bool fix_row_type(THD *thd, TABLE_SHARE *share, row_type correct_row_type);

/**
  Move all triggers from a table to another.

  @param[in]    thd              Thread context
  @param[in]    from_schema_name Name of the schema
  @param[in]    from_name        Name of the table
  @param[in]    to_schema_name   Name of the schema
  @param[in]    to_name          Name of the table

  Triggers from from_schema_name.from_table_name will be moved
  into to_schema_name.to_table_name. And the transaction will be
  committed.

  @retval       false       Success
  @retval       true        Error
*/

bool move_triggers(THD *thd,
                   const char *from_schema_name,
                   const char *from_name,
                   const char *to_schema_name,
                   const char *to_name);

/**
  Add column objects to dd::Abstract_table objects according to the
  list of Create_field objects.

  @param   thd              Thread handle.
  @param   tab_obj          dd::Table or dd::View's instance.
  @param   create_fields    List of Create_field objects to fill
                            dd::Column object(s).
  @param   file             handler instance for the table.

  @retval  false            On Success
  @retval  true             On error.
*/

bool
fill_dd_columns_from_create_fields(THD *thd,
                                   Abstract_table *tab_obj,
                                   const List<Create_field> &create_fields,
                                   handler *file);

/**
  @brief Function returns string representing column type by Create_field.
         This is required for the IS implementation which uses views on DD
         tables

  @param[in]   table           TABLE object.
  @param[in]   field           Column information.

  @return dd::String_type representing column type.
*/

dd::String_type get_sql_type_by_create_field(TABLE *table,
                                             Create_field *field);


/**
  Helper method to get numeric scale for types using Create_field type
  object.

  @param[in]  field      Field object.
  @param[out] scale      numeric scale value for types.

  @retval false  If numeric scale is calculated.
  @retval true   If numeric scale is not calculated;
*/

bool get_field_numeric_scale(Create_field *field, uint *scale);


/**
  Helper method to get numeric precision for types using Create_field type
  object.

  @param[in]  field             Field object.
  @param[out] numeric_precision numeric precision value for types.

  @retval false  If numeric precision is calculated.
  @retval true   If numeric precision is not calculated;
*/

bool get_field_numeric_precision(Create_field *field,
                                 uint *numeric_precision);


/**
  Helper method to get datetime precision for types using Create_field type
  object.

  @param[in]  field              Field object.
  @param[out] datetime_precision datetime precision value for types.

  @retval false  If datetime precision is calculated.
  @retval true   If datetime precision is not calculated;
*/

bool get_field_datetime_precision(Create_field *field,
                                  uint *datetime_precision);
} // namespace dd
#endif // DD_TABLE_INCLUDED
