/* Copyright (C) 2000-2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

/* Functions to map mysqld errno to sql_state */

#include <my_global.h>
#include <mysqld_error.h>

struct st_map_errno_to_sqlstate
{
  uint mysql_errno;
  const char *odbc_state;
  const char *jdbc_state;
};

struct st_map_errno_to_sqlstate sqlstate_map[]=
{
#include <sql_state.h>
};

const char *mysql_errno_to_sqlstate(uint mysql_errno)
{
  uint first=0, end= array_elements(sqlstate_map)-1;
  struct st_map_errno_to_sqlstate *map;

  /* Do binary search in the sorted array */
  while (first != end)
  {
    uint mid= (first+end)/2;
    map= sqlstate_map+mid;
    if (map->mysql_errno < mysql_errno)
      first= mid+1;
    else
      end= mid;
  }
  map= sqlstate_map+first;
  if (map->mysql_errno == mysql_errno)
    return map->odbc_state;
  return "HY000";				/* General error */
}
