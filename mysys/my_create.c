/* Copyright (C) 2000 MySQL AB

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

#include "mysys_priv.h"
#include <my_dir.h>
#include "mysys_err.h"
#include <errno.h>
#if defined(__WIN__)
#include <share.h>
#endif

	/*
	** Create a new file
	** Arguments:
	** Path-name of file
	** Read | write on file (umask value)
	** Read & Write on open file
	** Special flags
	*/


File my_create(const char *FileName, int CreateFlags, int access_flags,
	       myf MyFlags)
{
  int fd;
  DBUG_ENTER("my_create");
  DBUG_PRINT("my",("Name: '%s' CreateFlags: %d  AccessFlags: %d  MyFlags: %d",
		   FileName, CreateFlags, access_flags, MyFlags));

#if !defined(NO_OPEN_3)
  fd = open((my_string) FileName, access_flags | O_CREAT,
	    CreateFlags ? CreateFlags : my_umask);
#elif defined(VMS)
  fd = open((my_string) FileName, access_flags | O_CREAT, 0,
	    "ctx=stm","ctx=bin");
#elif defined(__WIN__)
  fd= my_sopen((my_string) FileName, access_flags | O_CREAT | O_BINARY,
	       SH_DENYNO, MY_S_IREAD | MY_S_IWRITE);
#else
  fd = open(FileName, access_flags);
#endif

  DBUG_RETURN(my_register_filename(fd, FileName, FILE_BY_CREATE,
				   EE_CANTCREATEFILE, MyFlags));
} /* my_create */
