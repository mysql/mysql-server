/* Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

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

#include "my_global.h"

#include "binary_log_types.h"        // enum_field_types
#include "handler.h"                 // legacy_db_type

#include "dd/types/column.h"         // enum_column_types

class Create_field;
class THD;
typedef struct st_ha_create_information HA_CREATE_INFO;
typedef struct st_key KEY;
template <class T> class List;

namespace dd {
  class Table;
  enum class enum_table_type;
  namespace cache {
    class Dictionary_client;
  }

static const char FIELD_NAME_SEPARATOR_CHAR = ';';

/**
  Convert from old field type to new enum types for fields in DD framework.

  @param type Old field type.

  @retval New field type.
*/
dd::enum_column_types dd_get_new_field_type(enum_field_types type);

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
  @param file           handler instance for the table.

  @retval 0 on success.
  @retval ER_CANT_CREATE_TABLE, ER_TABLE_EXISTS_ERROR on failure.
*/
bool create_table(THD *thd,
                  const std::string &schema_name,
                  const std::string &table_name,
                  HA_CREATE_INFO *create_info,
                  const List<Create_field> &create_fields,
                  const KEY *keyinfo, uint keys,
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
  @param file           handler instance for the table.

  @returns constructed dd::Table object, or NULL in case of an error.
*/
dd::Table *create_tmp_table(THD *thd,
                            const std::string &schema_name,
                            const std::string &table_name,
                            HA_CREATE_INFO *create_info,
                            const List<Create_field> &create_fields,
                            const KEY *keyinfo, uint keys,
                            handler *file);

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

/* Rename table name in dd.tables */
template <typename T>
bool rename_table(THD *thd,
                  const char *from_schema_name,
                  const char *from_name,
                  const char *to_schema_name,
                  const char *to_name,
                  bool no_foreign_key_check);

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
                             false);
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

} // namespace dd
#endif // DD_TABLE_INCLUDED
