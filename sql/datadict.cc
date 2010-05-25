/* Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

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

#include "datadict.h"
#include "sql_priv.h"
#include "sql_class.h"
#include "sql_table.h"


/**
  Check type of .frm if we are not going to parse it.

  @param  path  path to FRM file

  @retval  FRMTYPE_ERROR        error
  @retval  FRMTYPE_TABLE        table
  @retval  FRMTYPE_VIEW         view
*/

frm_type_enum dd_frm_type(THD *thd, char *path, enum legacy_db_type *dbt)
{
  File file;
  uchar header[10];     //"TYPE=VIEW\n" it is 10 characters
  size_t error;
  DBUG_ENTER("dd_frm_type");

  *dbt= DB_TYPE_UNKNOWN;

  if ((file= mysql_file_open(key_file_frm, path, O_RDONLY | O_SHARE, MYF(0))) < 0)
    DBUG_RETURN(FRMTYPE_ERROR);
  error= mysql_file_read(file, (uchar*) header, sizeof(header), MYF(MY_NABP));
  mysql_file_close(file, MYF(MY_WME));

  if (error)
    DBUG_RETURN(FRMTYPE_ERROR);
  if (!strncmp((char*) header, "TYPE=VIEW\n", sizeof(header)))
    DBUG_RETURN(FRMTYPE_VIEW);

  /*
    This is just a check for DB_TYPE. We'll return default unknown type
    if the following test is true (arg #3). This should not have effect
    on return value from this function (default FRMTYPE_TABLE)
  */
  if (header[0] != (uchar) 254 || header[1] != 1 ||
      (header[2] != FRM_VER && header[2] != FRM_VER+1 &&
       (header[2] < FRM_VER+3 || header[2] > FRM_VER+4)))
    DBUG_RETURN(FRMTYPE_TABLE);

  *dbt= (enum legacy_db_type) (uint) *(header + 3);

  /* Probably a table. */
  DBUG_RETURN(FRMTYPE_TABLE);
}


/**
  Given a table name, check if the storage engine for the
  table referred by this name supports an option 'flag'.
  Return an error if the table does not exist or is not a
  base table.

  @pre Any metadata lock on the table.

  @param[in]    thd         The current session.
  @param[in]    db          Table schema.
  @param[in]    table_name  Table database.
  @param[in]    flag        The option to check.
  @param[out]   yes_no      The result. Undefined if error.
*/

bool dd_check_storage_engine_flag(THD *thd,
                                  const char *db, const char *table_name,
                                  uint32 flag, bool *yes_no)
{
  char path[FN_REFLEN + 1];
  enum legacy_db_type db_type;
  handlerton *table_type;
  LEX_STRING db_name = {(char *) db, strlen(db)};

  if (check_db_name(&db_name))
  {
    my_error(ER_WRONG_DB_NAME, MYF(0), db_name.str);
    return TRUE;
  }

  if (check_table_name(table_name, strlen(table_name)))
  {
    my_error(ER_WRONG_TABLE_NAME, MYF(0), table_name);
    return TRUE;
  }

  /* There should be at least some lock on the table.  */
  DBUG_ASSERT(thd->mdl_context.is_lock_owner(MDL_key::TABLE, db,
                                             table_name, MDL_SHARED));

  (void) build_table_filename(path, sizeof(path) - 1, db,
                              table_name, reg_ext, 0);

  dd_frm_type(thd, path, &db_type);

  /* Type is unknown if the object is not found or is not a table. */
  if (db_type == DB_TYPE_UNKNOWN)
  {
    my_error(ER_NO_SUCH_TABLE, MYF(0), db, table_name);
    return TRUE;
  }

  table_type= ha_resolve_by_legacy_type(thd, db_type);
  *yes_no= ha_check_storage_engine_flag(table_type, flag);

  return FALSE;
}


/*
  Regenerate a metadata locked table.

  @param  thd   Thread context.
  @param  db    Name of the database to which the table belongs to.
  @param  name  Table name.

  @retval  FALSE  Success.
  @retval  TRUE   Error.
*/

bool dd_recreate_table(THD *thd, const char *db, const char *table_name)
{
  bool error= TRUE;
  HA_CREATE_INFO create_info;
  char path[FN_REFLEN + 1];
  DBUG_ENTER("dd_recreate_table");

  /* There should be a exclusive metadata lock on the table. */
  DBUG_ASSERT(thd->mdl_context.is_lock_owner(MDL_key::TABLE, db, table_name,
                                             MDL_EXCLUSIVE));

  memset(&create_info, 0, sizeof(create_info));

  /* Create a path to the table, but without a extension. */
  build_table_filename(path, sizeof(path) - 1, db, table_name, "", 0);

  /* Attempt to reconstruct the table. */
  mysql_mutex_lock(&LOCK_open);
  error= ha_create_table(thd, path, db, table_name, &create_info, TRUE);
  mysql_mutex_unlock(&LOCK_open);

  DBUG_RETURN(error);
}

