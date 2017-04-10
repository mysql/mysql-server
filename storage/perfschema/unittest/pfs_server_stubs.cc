/* Copyright (c) 2010, 2017, Oracle and/or its affiliates. All rights reserved.

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

/*
  Minimal code to be able to link a unit test.
*/

#include <stddef.h>
#include <sys/types.h>

#include "m_ctype.h"
#include "mysqld.h"
#include "sql_class.h"
#include "sql_show.h"

struct System_status_var global_status_var;

struct sql_digest_storage;
volatile bool ready_to_exit= false;

uint lower_case_table_names= 0;
CHARSET_INFO *files_charset_info= NULL;
CHARSET_INFO *system_charset_info= NULL;

void compute_digest_md5(const sql_digest_storage *, unsigned char *)
{
}

void reset_status_vars()
{
}

struct System_status_var* get_thd_status_var(THD*)
{
  return NULL;
}

unsigned int mysql_errno_to_sqlstate_index(unsigned int)
{
  return 0;
}
