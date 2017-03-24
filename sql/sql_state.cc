/* Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

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

/* Functions to map mysqld errno to sql_state */

#include <mysqld_error.h>
#include <sys/types.h>

#include "my_inttypes.h"  // IWYU pragma: keep
#include "mysql_com.h"  // IWYU pragma: keep

static const int NUM_SECTIONS=
  sizeof(errmsg_section_start) / sizeof(errmsg_section_start[0]);

struct st_map_errno_to_sqlstate
{
  const char *name;
  unsigned    code;
  const char *text;
  /* SQLSTATE */
  const char *odbc_state;
  const char *jdbc_state;
  unsigned error_index;
};

struct st_map_errno_to_sqlstate sqlstate_map[]=
{
#ifndef IN_DOXYGEN
#include <mysqld_ername.h>
#endif /* IN_DOXYGEN */
};

const char *mysql_errno_to_sqlstate(unsigned mysql_errno)
{
  int offset= 0; // Position where the current section starts in the array.
  int i;
  int temp_errno= (int)mysql_errno;

  for (i= 0; i < NUM_SECTIONS; i++)
  {
    if (temp_errno >= errmsg_section_start[i] &&
        temp_errno < (errmsg_section_start[i] + errmsg_section_size[i]))
      return sqlstate_map[mysql_errno - errmsg_section_start[i] + offset].odbc_state;
    offset+= errmsg_section_size[i];
  }
  return "HY000"; /* General error */
}
