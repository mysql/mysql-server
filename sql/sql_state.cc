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

#include <m_string.h> // ### MEOW
#include <mysqld_error.h>
#include <sys/types.h>

#include "../storage/perfschema/pfs_error.h" // ### MEOW
#include "my_inttypes.h"  // IWYU pragma: keep
#include "mysql_com.h"  // IWYU pragma: keep
#include "sql/derror.h" // ### MEOW

extern server_error  error_names_array[];
static server_error *sqlstate_map= &error_names_array[1];


extern int mysql_errno_to_builtin(uint mysql_errno);


static const char *builtin_get_sqlstate(int i)
{
  return (i < 0) ? "HY000" : sqlstate_map[i].odbc_state;
}

const char *mysql_errno_to_sqlstate(uint mysql_errno)
{
  return builtin_get_sqlstate(mysql_errno_to_builtin(mysql_errno));
}

static const char *builtin_get_symbol(int i)
{
  return (i < 0) ? nullptr : sqlstate_map[i].name;
}

const char *mysql_errno_to_symbol(int mysql_errno)
{
  return builtin_get_symbol(mysql_errno_to_builtin(mysql_errno));
}

int mysql_symbol_to_errno(const char *error_symbol)
{
  int offset= 0; // Position where the current section starts in the array.
  int i,j;

  for (i= 0; i < NUM_SECTIONS; i++)
  {
    for (j= 0; j < errmsg_section_size[i]; j++)
    {
      if (!native_strcasecmp(error_symbol, sqlstate_map[j + offset].name))
        return sqlstate_map[j + offset].mysql_errno;
    }
    offset+= errmsg_section_size[i];
  }
  return -1; /* General error */
}
