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

#define USES_TYPES
#include "mysys_priv.h"
#include "mysys_err.h"
#include <my_dir.h>
#include <errno.h>
#if defined(MSDOS) || defined(__WIN__) || defined(__EMX__)
#include <share.h>
#endif

	/* Open a file */

File my_dup(File file, myf MyFlags)
{
  File fd;
  char *filename;
  DBUG_ENTER("my_dup");
  DBUG_PRINT("my",("file: %d  MyFlags: %d", MyFlags));
  fd = dup(file);
  filename= (((int) file < MY_NFILE) ?
	     my_file_info[(int) file].name : "Unknown");
  DBUG_RETURN(my_register_filename(fd, filename, FILE_BY_DUP,
				   EE_FILENOTFOUND, MyFlags));
} /* my_open */
