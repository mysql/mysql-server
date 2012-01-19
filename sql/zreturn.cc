/* Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

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


#include "zgtids.h"


#ifndef DBUG_OFF


#ifndef MYSQL_CLIENT
#include "sql_class.h"
#endif // ifndef MYSQL_CLIENT


void check_return_status(enum_return_status status, const char *action,
                         const char *status_name, int allow_unreported)
{
  if (status != RETURN_STATUS_OK)
  {
    DBUG_ASSERT(allow_unreported || status == RETURN_STATUS_REPORTED_ERROR);
    if (status == RETURN_STATUS_REPORTED_ERROR)
    {
#if !defined(MYSQL_CLIENT) && !defined(DBUG_OFF)
      THD *thd= current_thd;
      DBUG_ASSERT(thd == NULL ||
                  thd->get_stmt_da()->status() == Diagnostics_area::DA_ERROR);
#endif
    }
    DBUG_PRINT("info", ("%s error %d (%s)", action, status, status_name));
  }
}


#endif // ! DBUG_OFF
