/* Copyright (c) 2000, 2015, Oracle and/or its affiliates. All rights reserved.

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

/*
  Rename a table
*/

#include "fulltext.h"

int mi_rename(const char *old_name, const char *new_name)
{
  char from[FN_REFLEN],to[FN_REFLEN];
  DBUG_ENTER("mi_rename");

#ifdef EXTRA_DEBUG
  check_table_is_closed(old_name,"rename old_table");
  check_table_is_closed(new_name,"rename new table2");
#endif

  fn_format(from,old_name,"",MI_NAME_IEXT,MY_UNPACK_FILENAME|MY_APPEND_EXT);
  fn_format(to,new_name,"",MI_NAME_IEXT,MY_UNPACK_FILENAME|MY_APPEND_EXT);
  if (mysql_file_rename_with_symlink(mi_key_file_kfile, from, to, MYF(MY_WME)))
    DBUG_RETURN(my_errno());
  fn_format(from,old_name,"",MI_NAME_DEXT,MY_UNPACK_FILENAME|MY_APPEND_EXT);
  fn_format(to,new_name,"",MI_NAME_DEXT,MY_UNPACK_FILENAME|MY_APPEND_EXT);
  DBUG_RETURN(mysql_file_rename_with_symlink(mi_key_file_dfile,
                                             from, to,
                                             MYF(MY_WME)) ? my_errno() : 0);
}
