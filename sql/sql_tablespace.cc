/* Copyright (c) 2000, 2012, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

/* drop and alter of tablespaces */

#include "sql_priv.h"
#include "unireg.h"
#include "sql_tablespace.h"
#include "sql_table.h"                          // write_bin_log
#include "sql_class.h"                          // THD

int mysql_alter_tablespace(THD *thd, st_alter_tablespace *ts_info)
{
  int error= HA_ADMIN_NOT_IMPLEMENTED;
  handlerton *hton= ts_info->storage_engine;

  DBUG_ENTER("mysql_alter_tablespace");
  /*
    If the user haven't defined an engine, this will fallback to using the
    default storage engine.
  */
  if (hton == NULL || hton->state != SHOW_OPTION_YES)
  {
    hton= ha_default_handlerton(thd);
    if (ts_info->storage_engine != 0)
      push_warning_printf(thd, Sql_condition::SL_WARNING,
                          ER_WARN_USING_OTHER_HANDLER,
                          ER(ER_WARN_USING_OTHER_HANDLER),
                          ha_resolve_storage_engine_name(hton),
                          ts_info->tablespace_name ? ts_info->tablespace_name
                                                : ts_info->logfile_group_name);
  }

  if (hton->alter_tablespace)
  {
    if ((error= hton->alter_tablespace(hton, thd, ts_info)))
    {
      if (error == HA_ADMIN_NOT_IMPLEMENTED)
      {
        my_error(ER_CHECK_NOT_IMPLEMENTED, MYF(0), "");
      }
      else if (error == 1)
      {
        DBUG_RETURN(1);
      }
      else
      {
        my_error(error, MYF(0));
      }
      DBUG_RETURN(error);
    }
  }
  else
  {
    my_error(ER_ILLEGAL_HA_CREATE_OPTION, MYF(0),
             ha_resolve_storage_engine_name(hton),
             "TABLESPACE or LOGFILE GROUP");
    DBUG_RETURN(HA_ADMIN_NOT_IMPLEMENTED);
  }
  error= write_bin_log(thd, FALSE, thd->query(), thd->query_length());
  DBUG_RETURN(error);
}
