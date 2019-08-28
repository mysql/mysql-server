/*
   Copyright (c) 2000, 2015, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
   02110-1301  USA */

/* TODO: Test all functions  */

#include "mysys_priv.h"
#include "my_dir.h"

int main(int argc, char *argv[])
{
  MY_DIR	*a;
  uint		f;
  DBUG_ENTER ("main");
  DBUG_PROCESS (argv[0]);

  if (--argc > 0 && (*(++argv))[0] == '-' && (*argv)[1] == '#' )
    DBUG_PUSH (*argv+2);

  a = my_dir("./", 0);
  for (f = 0; f < a->number_off_files; f++)
  {
    printf("%s\n", a->dir_entry[f].name);
  }

  a = my_dir("./", MY_WANT_STAT);
  for (f = 0; f < a->number_off_files; f++)
  {
    printf("%s %d %d %d %s\n", a->dir_entry[f].name,
	   (int) a->dir_entry[f].mystat.st_size,
	   (int) a->dir_entry[f].mystat.st_uid,
	   (int) a->dir_entry[f].mystat.st_gid,
	   S_ISDIR(a->dir_entry[f].mystat.st_mode) ? "dir" : "");
  }
  DBUG_RETURN(0);
}
