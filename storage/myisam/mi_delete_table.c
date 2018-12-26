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

/*
  deletes a table
*/

#include "fulltext.h"

int mi_delete_table(const char *name)
{
  char from[FN_REFLEN];
  DBUG_ENTER("mi_delete_table");

#ifdef EXTRA_DEBUG
  check_table_is_closed(name,"delete");
#endif

  fn_format(from,name,"",MI_NAME_IEXT,MY_UNPACK_FILENAME|MY_APPEND_EXT);
  if (my_is_symlink(from, NULL) && (*myisam_test_invalid_symlink)(from))
  {
    /*
      Symlink is pointing to file in data directory.
      Remove symlink, keep file.
    */
    if (mysql_file_delete(mi_key_file_kfile, from, MYF(MY_WME)))
      DBUG_RETURN(my_errno());
  }
  else
  {
    if (mysql_file_delete_with_symlink(mi_key_file_kfile, from, MYF(MY_WME)))
      DBUG_RETURN(my_errno());
  }
  fn_format(from,name,"",MI_NAME_DEXT,MY_UNPACK_FILENAME|MY_APPEND_EXT);
  if (my_is_symlink(from, NULL) && (*myisam_test_invalid_symlink)(from))
  {
    /*
      Symlink is pointing to file in data directory.
      Remove symlink, keep file.
    */
    if (mysql_file_delete(mi_key_file_dfile, from, MYF(MY_WME)))
      DBUG_RETURN(my_errno());
  }
  else
  {
    if (mysql_file_delete_with_symlink(mi_key_file_dfile, from, MYF(MY_WME)))
      DBUG_RETURN(my_errno());
  }
  DBUG_RETURN(0);
}
