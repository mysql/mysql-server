/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/* Create a MYMERGE_-file */

#include "mymrgdef.h"

	/* create file named 'name' and save filenames in it
	   table_names should be NULL or a vector of string-pointers with
	   a NULL-pointer last
	   */

int myrg_create(const char *name, const char **table_names,
                uint insert_method, my_bool fix_names)
{
  int save_errno;
  uint errpos;
  File file;
  char buff[FN_REFLEN],*end;
  DBUG_ENTER("myrg_create");

  errpos=0;
  if ((file = my_create(fn_format(buff,name,"",MYRG_NAME_EXT,4),0,
       O_RDWR | O_TRUNC,MYF(MY_WME))) < 0)
    goto err;
  errpos=1;
  if (table_names)
  {
    for ( ; *table_names ; table_names++)
    {
      strmov(buff,*table_names);
      if (fix_names)
	fn_same(buff,name,4);
      *(end=strend(buff))='\n';
      end[1]=0;
      if (my_write(file,buff,(uint) (end-buff+1),
		   MYF(MY_WME | MY_NABP)))
	goto err;
    }
  }
  if (insert_method != MERGE_INSERT_DISABLED)
  {
    end=strxmov(buff,"#INSERT_METHOD=",
		get_type(&merge_insert_method,insert_method-1),"\n",NullS);
    if (my_write(file,buff,(uint) (end-buff),MYF(MY_WME | MY_NABP)))
        goto err;
  }
  if (my_close(file,MYF(0)))
    goto err;
  DBUG_RETURN(0);

err:
  save_errno=my_errno ? my_errno : -1;
  switch (errpos) {
  case 1:
    VOID(my_close(file,MYF(0)));
  }
  DBUG_RETURN(my_errno=save_errno);
} /* myrg_create */
