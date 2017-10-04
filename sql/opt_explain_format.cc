/* Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "sql/opt_explain_format.h"

#include "m_ctype.h"
#include "my_dbug.h"
#include "sql/current_thd.h"
#include "sql/sql_class.h"

bool qep_row::mem_root_str::is_empty()
{
  if (deferred)
  {
    StringBuffer<128> buff(system_charset_info);
    if (deferred->eval(&buff) || set(buff))
    {
      DBUG_ASSERT(!"OOM!");
      return true; // ignore OOM
    }
    deferred= NULL; // prevent double evaluation, if any
  }
  return str == NULL;
}

bool
qep_row::mem_root_str::set(const char *str_arg, size_t length_arg)
{
  deferred= NULL;
  if (!(str= strndup_root(current_thd->mem_root, str_arg, length_arg)))
    return true; /* purecov: inspected */
  length= length_arg;
  return false;
}
