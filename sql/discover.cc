/* Copyright (c) 2004, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */


/**
  @file

  @brief
  Functions for discover of frm file from handler
*/

#include "sql_priv.h"
#include "unireg.h"
#include "discover.h"
#include <my_dir.h>

/**
  Read the contents of a .frm file.

  frmdata and len are set to 0 on error.

  @param name           path to table-file "db/name"
  @param frmdata        frm data
  @param len            length of the read frmdata

  @retval
    0	ok
  @retval
    1	Could not open file
  @retval
    2    Could not stat file
  @retval
    3    Could not allocate data for read.  Could not read file
*/

int readfrm(const char *name, uchar **frmdata, size_t *len)
{
  int    error;
  char	 index_file[FN_REFLEN];
  File	 file;
  size_t read_len;
  uchar *read_data;
  MY_STAT state;  
  DBUG_ENTER("readfrm");
  DBUG_PRINT("enter",("name: '%s'",name));
  
  *frmdata= NULL;      // In case of errors
  *len= 0;
  error= 1;
  if ((file= mysql_file_open(key_file_frm,
                             fn_format(index_file, name, "", reg_ext,
                               MY_UNPACK_FILENAME|MY_APPEND_EXT),
                             O_RDONLY | O_SHARE,
                             MYF(0))) < 0)
    goto err_end; 
  
  // Get length of file
  error= 2;
  if (mysql_file_fstat(file, &state, MYF(0)))
    goto err;
  read_len= state.st_size;  

  // Read whole frm file
  error= 3;
  read_data= 0;                                 // Nothing to free
  if (read_string(file, &read_data, read_len))
    goto err;

  // Setup return data
  *frmdata= (uchar*) read_data;
  *len= read_len;
  error= 0;
  
 err:
  if (file > 0)
    (void) mysql_file_close(file, MYF(MY_WME));
  
 err_end:		      /* Here when no file */
  DBUG_RETURN (error);
} /* readfrm */


/*
  Write the content of a frm data pointer 
  to a frm file.

  @param name           path to table-file "db/name"
  @param frmdata        frm data
  @param len            length of the frmdata

  @retval
    0	ok
  @retval
    2    Could not write file
*/

int writefrm(const char *name, const uchar *frmdata, size_t len)
{
  File file;
  char	 index_file[FN_REFLEN];
  int error;
  DBUG_ENTER("writefrm");
  DBUG_PRINT("enter",("name: '%s' len: %lu ",name, (ulong) len));

  error= 0;
  if ((file= mysql_file_create(key_file_frm,
                               fn_format(index_file, name, "", reg_ext,
                                         MY_UNPACK_FILENAME | MY_APPEND_EXT),
                               CREATE_MODE, O_RDWR | O_TRUNC,
                               MYF(MY_WME))) >= 0)
  {
    if (mysql_file_write(file, frmdata, len, MYF(MY_WME | MY_NABP)))
      error= 2;
    (void) mysql_file_close(file, MYF(0));
  }
  DBUG_RETURN(error);
} /* writefrm */





