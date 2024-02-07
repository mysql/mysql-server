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

/* Create a MYMERGE_-file */

#include <fcntl.h>
#include <sys/types.h>

#include "m_string.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_io.h"
#include "my_thread_local.h"
#include "mysql/psi/mysql_file.h"
#include "nulls.h"
#include "storage/myisammrg/myrg_def.h"
#include "strxmov.h"
#include "typelib.h"

/* create file named 'name' and save filenames in it
   table_names should be NULL or a vector of string-pointers with
   a NULL-pointer last
   */

int myrg_create(const char *name, const char **table_names, uint insert_method,
                bool fix_names) {
  int save_errno;
  uint errpos;
  File file;
  char buff[FN_REFLEN], *end;
  DBUG_TRACE;

  errpos = 0;
  if ((file = mysql_file_create(rg_key_file_MRG,
                                fn_format(buff, name, "", MYRG_NAME_EXT,
                                          MY_UNPACK_FILENAME | MY_APPEND_EXT),
                                0, O_RDWR | O_EXCL | O_NOFOLLOW, MYF(MY_WME))) <
      0)
    goto err;
  errpos = 1;
  if (table_names) {
    for (; *table_names; table_names++) {
      my_stpcpy(buff, *table_names);
      if (fix_names) fn_same(buff, name, 4);
      *(end = strend(buff)) = '\n';
      end[1] = 0;
      if (mysql_file_write(file, (uchar *)buff, (uint)(end - buff + 1),
                           MYF(MY_WME | MY_NABP)))
        goto err;
    }
  }
  if (insert_method != MERGE_INSERT_DISABLED) {
    end =
        strxmov(buff, "#INSERT_METHOD=",
                get_type(&merge_insert_method, insert_method - 1), "\n", NullS);
    if (mysql_file_write(file, (uchar *)buff, (uint)(end - buff),
                         MYF(MY_WME | MY_NABP)))
      goto err;
  }
  if (mysql_file_close(file, MYF(0))) goto err;
  return 0;

err:
  save_errno = my_errno() ? my_errno() : -1;
  switch (errpos) {
    case 1:
      (void)mysql_file_close(file, MYF(0));
  }
  set_my_errno(save_errno);
  return save_errno;
} /* myrg_create */
