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


/* Functions for discover of frm file from handler */

#include "mysql_priv.h"
#include <my_dir.h>

/*
  Read the contents of a .frm file

  SYNOPSIS
    readfrm()

    name           path to table-file "db/name"
    frmdata        frm data
    len            length of the read frmdata

  RETURN VALUES
   0	ok
   1	Could not open file
   2    Could not stat file
   3    Could not allocate data for read
        Could not read file

   frmdata and len are set to 0 on error
*/

int readfrm(const char *name,
	    const void **frmdata, uint *len)
{
  int    error;
  char	 index_file[FN_REFLEN];
  File	 file;
  ulong read_len;
  char *read_data;
  MY_STAT state;  
  DBUG_ENTER("readfrm");
  DBUG_PRINT("enter",("name: '%s'",name));
  
  *frmdata= NULL;      // In case of errors
  *len= 0;
  error= 1;
  if ((file=my_open(fn_format(index_file,name,"",reg_ext,4),
		    O_RDONLY | O_SHARE,
		    MYF(0))) < 0)  
    goto err_end; 
  
  // Get length of file
  error= 2;
  if (my_fstat(file, &state, MYF(0)))
    goto err;
  read_len= state.st_size;  

  // Read whole frm file
  error= 3;
  read_data= 0; 
  if (read_string(file, &read_data, read_len))
    goto err;

  // Setup return data
  *frmdata= (void*) read_data;
  *len= read_len;
  error= 0;
  
 err:
  if (file > 0)
    VOID(my_close(file,MYF(MY_WME)));
  
 err_end:		      /* Here when no file */
  DBUG_RETURN (error);
} /* readfrm */


/*
  Write the content of a frm data pointer 
  to a frm file

  SYNOPSIS
    writefrm()

    name           path to table-file "db/name"
    frmdata        frm data
    len            length of the frmdata

  RETURN VALUES
   0	ok
   2    Could not write file
*/

int writefrm(const char *name, const void *frmdata, uint len)
{
  File file;
  char	 index_file[FN_REFLEN];
  int error;
  DBUG_ENTER("writefrm");
  DBUG_PRINT("enter",("name: '%s' len: %d ",name,len));
  //DBUG_DUMP("frmdata", (char*)frmdata, len);

  error= 0;
  if ((file=my_create(fn_format(index_file,name,"",reg_ext,4),
		      CREATE_MODE,O_RDWR | O_TRUNC,MYF(MY_WME))) >= 0)
  {
    if (my_write(file,(byte*)frmdata,len,MYF(MY_WME | MY_NABP)))
      error= 2;
  }
  VOID(my_close(file,MYF(0)));
  DBUG_RETURN(error);
} /* writefrm */





