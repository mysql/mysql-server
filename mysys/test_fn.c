/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA */

#include "mysys_priv.h"

const char *test_names[]=
{
  "/usr/my/include/srclib/myfunc/dbug/test",
  "test",
  "dbug/test",
  "/usr/my/srclib/myfunc/dbug/test",
  "/usr/monty/oldcopy/jazz/setupp.frm",
  "~/monty.tst",
  "~/dbug/monty.tst",
  "./hejsan",
  "./dbug/test",
  "../dbug/test",
  "../myfunc/test",
  "../../monty/rutedit",
  "/usr/monty//usr/monty/rutedit",
  "/usr/./monty/rutedit",
  "/usr/my/../monty/rutedit",
  "/usr/my/~/rutedit",
  "~/../my",
  "~/../my/srclib/myfunc/test",
  "~/../my/srclib/myfunc/./dbug/test",
  "/../usr/my/srclib/dbug",
  "c/../my",
  "/c/../my",
  NullS,
};

int main(int argc __attribute__((unused)), char **argv)
{
  const char **pos;
  char buff[FN_REFLEN],buff2[FN_REFLEN];
  DBUG_ENTER ("main");
  DBUG_PROCESS (argv[0]);
  MY_INIT(argv[0]);

  if (argv[1] && argv[1][1] == '#')
    DBUG_PUSH(argv[1]+2);

  for (pos=test_names; *pos ; pos++)
  {
    printf("org :   '%s'\n",*pos);
    printf("pack:   '%s'\n",fn_format(buff,*pos,"","",8));
    printf("unpack: '%s'\n",fn_format(buff2,*pos,"","",4));
    if (strcmp(unpack_filename(buff,buff),buff2) != 0)
    {
      printf("error on cmp: '%s' != '%s'\n",buff,buff2);
    }
    puts("");
  }
  DBUG_RETURN(0);
}
