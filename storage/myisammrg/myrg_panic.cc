/* Copyright (c) 2000, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "my_dbug.h"
#include "my_thread_local.h"
#include "storage/myisammrg/myrg_def.h"

/* if flag == HA_PANIC_CLOSE then all misam files are closed */
/* if flag == HA_PANIC_WRITE then all misam files are unlocked and
   all changed data in single user misam is written to file */
/* if flag == HA_PANIC_READ then all misam files that was locked when
   mi_panic(HA_PANIC_WRITE) was done is locked. A mi_readinfo() is
   done for all single user files to get changes in database */

int myrg_panic(enum ha_panic_function flag) {
  int error = 0;
  LIST *list_element, *next_open;
  MYRG_INFO *info;
  DBUG_TRACE;

  for (list_element = myrg_open_list; list_element; list_element = next_open) {
    next_open = list_element->next; /* Save if close */
    info = (MYRG_INFO *)list_element->data;
    if (flag == HA_PANIC_CLOSE && myrg_close(info)) error = my_errno();
  }
  if (myrg_open_list && flag != HA_PANIC_CLOSE) return mi_panic(flag);
  if (error) set_my_errno(error);
  return error;
}
