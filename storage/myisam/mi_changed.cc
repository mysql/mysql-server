/* Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/* Check if somebody has changed table since last check. */

#include "my_dbug.h"
#include "storage/myisam/myisamdef.h"

/* Return 0 if table isn't changed */

int mi_is_changed(MI_INFO *info) {
  int result;
  DBUG_ENTER("mi_is_changed");
  if (fast_mi_readinfo(info)) DBUG_RETURN(-1);
  (void)_mi_writeinfo(info, 0);
  result = (int)info->data_changed;
  info->data_changed = 0;
  DBUG_PRINT("exit", ("result: %d", result));
  DBUG_RETURN(result);
}
