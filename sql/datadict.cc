/* Copyright (c) 2010, 2015, Oracle and/or its affiliates. All rights reserved.

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
#include "sql_class.h"
#include "sql_table.h"                          // build_table_filename
#include "mysqld.h"                             // key_file_frm
#include "psi_memory_key.h"                     // key_memory_frm_extra_segment_buff

#include "pfs_file_provider.h"
#include "mysql/psi/mysql_file.h"

/**
  Check type of .frm if we are not going to parse it.

  @param[in]  thd   The current session.
  @param[in]  path  path to FRM file.

  @retval  FRMTYPE_ERROR        error
  @retval  FRMTYPE_TABLE        table
  @retval  FRMTYPE_VIEW         view
*/

frm_type_enum dd_frm_type(THD *thd, char *path)
{
  File file;
  uchar header[10];     //"TYPE=VIEW\n" it is 10 characters
  size_t error;
  DBUG_ENTER("dd_frm_type");

  if ((file= mysql_file_open(key_file_frm, path, O_RDONLY | O_SHARE, MYF(0))) < 0)
    DBUG_RETURN(FRMTYPE_ERROR);
  error= mysql_file_read(file, (uchar*) header, sizeof(header), MYF(MY_NABP));
  mysql_file_close(file, MYF(MY_WME));

  if (error)
    DBUG_RETURN(FRMTYPE_ERROR);

  if (!strncmp((char*) header, "TYPE=VIEW\n", sizeof(header)))
    DBUG_RETURN(FRMTYPE_VIEW);

  DBUG_RETURN(FRMTYPE_TABLE);
}


/**
  Given a table name, check type of .frm and legacy table type.

  @param[in]   thd          The current session.
  @param[in]   db           Table schema.
  @param[in]   table_name   Table database.
  @param[out]  table_type   handlerton of the table if FRMTYPE_TABLE,
                            otherwise undefined.

  @return FALSE if FRMTYPE_TABLE and storage engine found. TRUE otherwise.
*/

static bool dd_frm_storage_engine(THD *thd, const char *db,
                                  const char *table_name,
                                  handlerton **table_type)
{
  char path[FN_REFLEN + 1];
  LEX_STRING db_name = {(char *) db, strlen(db)};

  /* There should be at least some lock on the table.  */
  DBUG_ASSERT(thd->mdl_context.owns_equal_or_stronger_lock(MDL_key::TABLE,
                                                           db, table_name,
                                                           MDL_SHARED));

  if (check_and_convert_db_name(&db_name, FALSE) != IDENT_NAME_OK)
    return TRUE;

  enum_ident_name_check ident_check_status=
    check_table_name(table_name, strlen(table_name));
  if (ident_check_status == IDENT_NAME_WRONG)
  {
    my_error(ER_WRONG_TABLE_NAME, MYF(0), table_name);
    return TRUE;
  }
  else if (ident_check_status == IDENT_NAME_TOO_LONG)
  {
    my_error(ER_TOO_LONG_IDENT, MYF(0), table_name);
    return TRUE;
  }

  (void) build_table_filename(path, sizeof(path) - 1, db,
                              table_name, reg_ext, 0);

  /* Type is unknown if the object is not found or is not a table. */
  if (dd_frm_type_and_se(thd, path, table_type) != FRMTYPE_TABLE)
  {
    my_error(ER_NO_SUCH_TABLE, MYF(0), db, table_name);
    return TRUE;
  }
  else if (!*table_type)
  {
    my_error(ER_STORAGE_ENGINE_NOT_LOADED, MYF(0), db, table_name);
    return TRUE;
  }

  return FALSE;
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
  handlerton *table_type;

  if (dd_frm_storage_engine(thd, db, table_name, &table_type))
    return TRUE;

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
  DBUG_ASSERT(thd->mdl_context.owns_equal_or_stronger_lock(MDL_key::TABLE,
                                                           db, table_name,
                                                           MDL_EXCLUSIVE));

  memset(&create_info, 0, sizeof(create_info));

  /* Create a path to the table, but without a extension. */
  build_table_filename(path, sizeof(path) - 1, db, table_name, "", 0);

  /* Attempt to reconstruct the table. */
  error= ha_create_table(thd, path, db, table_name, &create_info, TRUE);

  DBUG_RETURN(error);
}


/**
  Given a .frm file path, check its type and resolve SE.

  @param[in]  thd   The current session.
  @param[in]  path  path to FRM file.
  @param[out] hton  table handlerton if FRMTYPE_TABLE, otherwise NULL.

  @retval  FRMTYPE_ERROR        error
  @retval  FRMTYPE_TABLE        table
  @retval  FRMTYPE_VIEW         view

  Note: hton is set to NULL if SE is not loaded or unknown.
*/

frm_type_enum dd_frm_type_and_se(THD *thd, char *path, handlerton **hton)
{
  File file;
  enum legacy_db_type legacy_type= DB_TYPE_UNKNOWN;
  frm_type_enum frm_type= FRMTYPE_ERROR;
  uchar header[64];
  const char *view_header= "TYPE=VIEW\n";
  uchar *extra_segment= NULL;
  DBUG_ENTER("dd_frm_type_and_se");

  *hton= NULL;

  if ((file= mysql_file_open(key_file_frm, path, O_RDONLY | O_SHARE, MYF(0))) < 0)
    DBUG_RETURN(FRMTYPE_ERROR);

  /* Read header. */
  if (mysql_file_read(file, (uchar *) header, sizeof(header), MYF(MY_NABP)))
    goto err;

  /* Check if it's a VIEW. */
  if (!strncmp((char *) header, view_header, strlen(view_header)))
  {
    frm_type= FRMTYPE_VIEW;
    goto err;
  }

  frm_type= FRMTYPE_TABLE;

  /*
    In unlikely case of wrong .FRM magic number or unknown .FRM version
    we simply report FRMTYPE_TABLE and unknown storage engine.
  */
  if (header[0] != (uchar) 254 || header[1] != 1 ||
      (header[2] != FRM_VER && header[2] != FRM_VER + 1 &&
       (header[2] < FRM_VER + 3 || header[2] > FRM_VER + 4)))
    goto err;

  legacy_type= (enum legacy_db_type) (uint) header[3];

  /*
    There's no point in resolving dynamic SE by its legacy_type.
    Let's resolve it by SE name.
  */
  if (legacy_type > DB_TYPE_UNKNOWN &&
      legacy_type < DB_TYPE_FIRST_DYNAMIC)
  {
    /* Static SE. */
    *hton= ha_resolve_by_legacy_type(thd, legacy_type);
  }
  else if (legacy_type >= DB_TYPE_FIRST_DYNAMIC &&
           legacy_type < DB_TYPE_DEFAULT)
  {
    /* Dynamic SE. Read its name from .frm file then resolve. */
    ulong extra_segment_length= uint4korr(header + 55);
    if (extra_segment_length)
    {
      /* Read extra data segment. */
      if (!(extra_segment = (uchar *) my_malloc(key_memory_frm_extra_segment_buff,
                                                extra_segment_length, MYF(MY_WME))))
        goto err;

      /* Copied from sql/table.cc:open_binary_frm(). */
      ulong record_offset= 
        (ulong) (uint2korr(header + 6) +
                 ((uint2korr(header + 14) == 0xffff ?
                   uint4korr(header + 47) : uint2korr(header + 14))));
      uint reclength = uint2korr((header + 16));
      if (mysql_file_pread(file, extra_segment, extra_segment_length,
                           record_offset + reclength,
                           MYF(MY_NABP)))
        goto err;

      uint connect_string_length= uint2korr(extra_segment);
      uchar *extra_segment_end= extra_segment + extra_segment_length;
      uchar *pos= extra_segment + 2 + connect_string_length;

      if (pos + 2 < extra_segment_end)
      {
        uint se_name_length= uint2korr(pos);
        LEX_CSTRING se_name;
        plugin_ref plugin;
        
        se_name.str= (char *) pos + 2;
        se_name.length= se_name_length;

        if ((plugin= ha_resolve_by_name_raw(thd, se_name)))
          *hton= plugin_data<handlerton*>(plugin);
      }
    }
  }

err:
  my_free(extra_segment);
  mysql_file_close(file, MYF(MY_WME));
  DBUG_RETURN(frm_type);
}
