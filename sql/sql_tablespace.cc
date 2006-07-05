/* Copyright (C) 2000-2004 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/* drop and alter of tablespaces */

#include "mysql_priv.h"

int mysql_alter_tablespace(THD *thd, st_alter_tablespace *ts_info)
{
  int error= HA_ADMIN_NOT_IMPLEMENTED;
  const handlerton *hton= ts_info->storage_engine;

  DBUG_ENTER("mysql_alter_tablespace");
  /*
    If the user haven't defined an engine, this will fallback to using the
    default storage engine.
  */
  if (hton == NULL || hton->state != SHOW_OPTION_YES)
  {
    hton= ha_default_handlerton(thd);
    if (ts_info->storage_engine != 0)
      push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_ERROR,
                          ER_WARN_USING_OTHER_HANDLER,
                          ER(ER_WARN_USING_OTHER_HANDLER),
                          hton2plugin[hton->slot]->name.str,
                          ts_info->tablespace_name ? ts_info->tablespace_name
                                                : ts_info->logfile_group_name);
  }

  if (hton->alter_tablespace)
  {
    if ((error= hton->alter_tablespace(thd, ts_info)))
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
    push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_ERROR,
                        ER_ILLEGAL_HA_CREATE_OPTION,
                        ER(ER_ILLEGAL_HA_CREATE_OPTION),
                        hton2plugin[hton->slot]->name.str,
                        "TABLESPACE or LOGFILE GROUP");
  }
  if (mysql_bin_log.is_open())
  {
    thd->binlog_query(THD::STMT_QUERY_TYPE,
                      thd->query, thd->query_length, FALSE, TRUE);
  }
  DBUG_RETURN(FALSE);
}
