/* Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef DD_TABLE_INCLUDED
#define DD_TABLE_INCLUDED

#include <sys/types.h>
#include <memory>                    // std:unique_ptr
#include <string>

#include "binary_log_types.h"        // enum_field_types
#include "my_inttypes.h"
#include "sql/dd/string_type.h"
#include "sql/dd/types/column.h"     // dd::enum_column_types
#include "sql/handler.h"             // legacy_db_type
#include "sql/sql_alter.h"           // Alter_info::enum_enable_or_disable
#include "sql/system_variables.h"

class Create_field;
class FOREIGN_KEY;
class KEY;
class THD;
namespace dd {
class Schema;
}  // namespace dd
struct TABLE;

typedef struct st_ha_create_information HA_CREATE_INFO;
template <class T> class List;

namespace dd {
  class Abstract_table;
  class Table;

  namespace cache {
    class Dictionary_client;
  }

static const char FIELD_NAME_SEPARATOR_CHAR = ';';
static const char FOREIGN_KEY_NAME_SUBSTR[]= "_ibfk_";

/**
  Prepares a dd::Table object from mysql_prepare_create_table() output
  and updates DD tables. This function creates a user table, as opposed
  to create_table() which can handle system tables as well.

  @param thd            Thread handle
  @param sch_obj        Schema.
  @param table_name     Table name.
  @param create_info    HA_CREATE_INFO describing the table to be created.
  @param create_fields  List of fields for the table.
  @param keyinfo        Array with descriptions of keys for the table.
  @param keys           Number of keys.
  @param keys_onoff     keys ON or OFF
  @param fk_keyinfo     Array with descriptions of foreign keys for the table.
  @param fk_keys        Number of foreign keys.
  @param file           handler instance for the table.

  @note The caller must rollback both statement and transaction on failure,
        before any further accesses to DD. This is because such a failure
        might be caused by a deadlock, which requires rollback before any
        other operations on SE (including reads using attachable transactions)
        can be done.

  @retval False - Success.
  @retval True  - Error.
*/
bool create_dd_user_table(THD *thd,
                          const dd::Schema &sch_obj,
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

  @param thd                Thread handle
  @param sch_obj            Schema.
  @param table_name         Table name.
  @param create_info        HA_CREATE_INFO describing the table to be created.
  @param create_fields      List of fields for the table.
  @param keyinfo            Array with descriptions of keys for the table.
  @param keys               Number of keys.
  @param keys_onoff         Enable or disable keys.
  @param fk_keyinfo         Array with descriptions of foreign keys for the table.
  @param fk_keys            Number of foreign keys.
  @param file               handler instance for the table.

  @note The caller must rollback both statement and transaction on failure,
        before any further accesses to DD. This is because such a failure
        might be caused by a deadlock, which requires rollback before any
        other operations on SE (including reads using attachable transactions)
        can be done.

  @retval False - Success.
  @retval True  - Error.
*/
bool create_table(THD *thd,
                  const dd::Schema &sch_obj,
                  const dd::String_type &table_name,
                  HA_CREATE_INFO *create_info,
                  const List<Create_field> &create_fields,
                  const KEY *keyinfo, uint keys,
                  Alter_info::enum_enable_or_disable keys_onoff,
                  const FOREIGN_KEY *fk_keyinfo,
                  uint fk_keys,
                  handler *file);

/**
  Prepares a dd::Table object for a temporary table from
  mysql_prepare_create_table() output. Doesn't update DD tables,
  instead returns dd::Table object to caller.

  @param thd            Thread handle.
  @param sch_obj        Schema.
  @param table_name     Table name.
  @param create_info    HA_CREATE_INFO describing the table to be created.
  @param create_fields  List of fields for the table.
  @param keyinfo        Array with descriptions of keys for the table.
  @param keys           Number of keys.
  @param keys_onoff     Enable or disable keys.
  @param file           handler instance for the table.

  @returns Constructed dd::Table object, or nullptr in case of an error.
*/
std::unique_ptr<dd::Table> create_tmp_table(THD *thd,
                             const dd::Schema &sch_obj,
                             const dd::String_type &table_name,
                             HA_CREATE_INFO *create_info,
                             const List<Create_field> &create_fields,
                             const KEY *keyinfo, uint keys,
                             Alter_info::enum_enable_or_disable keys_onoff,
                             handler *file);

//////////////////////////////////////////////////////////////////////////
// Function common to 'table' and 'view' objects
//////////////////////////////////////////////////////////////////////////

/*
  Remove table metadata from the data dictionary.

  @param thd                Thread context.
  @param schema_name        Schema of the table to be removed.
  @param name               Name of the table to be removed.
  @param table_def          dd::Table object for the table to be removed.

  @note The caller must rollback both statement and transaction on failure,
        before any further accesses to DD. This is because such a failure
        might be caused by a deadlock, which requires rollback before any
        other operations on SE (including reads using attachable transactions)
        can be done.

  @retval false on success
  @retval true on failure
*/
bool drop_table(THD *thd, const char *schema_name, const char *name,
                const dd::Table &table_def);
/**
  Check if a table or view exists

  table_exists() sets exists=true if such a table or a view exists.

  @param       client       The dictionary client.
  @param       schema_name  The schema in which the object should be defined.
  @param       name         The object name to search for.
  @param [out] exists       A boolean which is set to true if the object
                            is found.

  @retval      true         Failure (error has been reported).
  @retval      false        Success.
*/
bool table_exists(dd::cache::Dictionary_client *client,
                  const char *schema_name,
                  const char *name,
                  bool *exists);

/**
  Rename foreign keys which have generated names to
  match the new name of the table.

  @param old_table_name  Table name before rename.
  @param new_tab         New version of the table with new name set.

  @todo Implement new naming scheme (or move responsibility of
        naming to the SE layer).

  @returns true if error, false otherwise.
*/

bool rename_foreign_keys(const char *old_table_name,
                         dd::Table *new_tab);

//////////////////////////////////////////////////////////////////////////
// Functions for retrieving, inspecting and manipulating instances of
// dd::Abstract_table, dd::Table and dd::View
//////////////////////////////////////////////////////////////////////////

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
  Get the storage engine handlerton for the given table.

  This function sets explicit error codes if:
  - The SE is invalid:      ER_UNKNOWN_STORAGE_ENGINE

  @param[in]  thd             Thread context
  @param[in]  table           dd::Table object describing the table.
  @param[out] hton            Handlerton for the table's storage engine

  @retval     true            Error
  @retval     false           Success
*/
bool table_storage_engine(THD *thd, const dd::Table *table,
                          handlerton **hton);

/**
  Regenerate a metadata locked table.

  This function does not set error codes beyond what is set by the
  functions it calls.

  @pre There must be an exclusive MDL lock on the table.

  @param[in]    thd                 Thread context
  @param[in]    schema_name         Name of the schema
  @param[in]    table_name          Name of the table

  @retval       false       Success
  @retval       true        Error
*/
bool recreate_table(THD *thd, const char *schema_name,
                    const char *table_name);

/**
  Function prepares string representing columns data type.
  This is required for IS implementation which uses views on DD tables
*/
String_type get_sql_type_by_field_info(THD *thd,
                                       enum_field_types field_type,
                                       uint32 field_length,
                                       uint32 decimals,
                                       bool maybe_null,
                                       bool is_unsigned,
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
  Update row format for the table with the value
  value supplied by caller function.

  @pre There must be an exclusive MDL lock on the table.

  @param[in]    thd              Thread context.
  @param[in]    table            Table object for the table.
  @param[in]    correct_row_type row_type to be set.

  @retval       false       Success
  @retval       true        Error
*/
bool fix_row_type(THD *thd, dd::Table *table, row_type correct_row_type);

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
