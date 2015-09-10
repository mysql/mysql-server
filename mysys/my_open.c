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

#include "mysys_priv.h"
#include "my_sys.h"
#include "mysys_err.h"
#include <my_dir.h>
#include <errno.h>
#include "my_thread_local.h"


/*
  Open a file

  SYNOPSIS
    my_open()
      FileName	Fully qualified file name
      Flags	Read | write 
      MyFlags	Special flags

  RETURN VALUE
    File descriptor
*/

File my_open(const char *FileName, int Flags, myf MyFlags)
				/* Path-name of file */
				/* Read | write .. */
				/* Special flags */
{
  File fd;
  DBUG_ENTER("my_open");
  DBUG_PRINT("my",("Name: '%s'  Flags: %d  MyFlags: %d",
		   FileName, Flags, MyFlags));
#if defined(_WIN32)
  fd= my_win_open(FileName, Flags);
#else
  fd = open(FileName, Flags, my_umask);	/* Normal unix */
#endif

  fd= my_register_filename(fd, FileName, FILE_BY_OPEN, EE_FILENOTFOUND, MyFlags);
  DBUG_RETURN(fd);
} /* my_open */


/*
  Close a file

  SYNOPSIS
    my_close()
      fd	File sescriptor
      myf	Special Flags

*/

int my_close(File fd, myf MyFlags)
{
  int err;
  DBUG_ENTER("my_close");
  DBUG_PRINT("my",("fd: %d  MyFlags: %d",fd, MyFlags));

  mysql_mutex_lock(&THR_LOCK_open);
#ifndef _WIN32
  do
  {
    err= close(fd);
  } while (err == -1 && errno == EINTR);
#else
  err= my_win_close(fd);
#endif
  if (err)
  {
    DBUG_PRINT("error",("Got error %d on close",err));
    set_my_errno(errno);
    if (MyFlags & (MY_FAE | MY_WME))
    {
      char errbuf[MYSYS_STRERROR_SIZE];
      my_error(EE_BADCLOSE, MYF(0), my_filename(fd),
               my_errno(), my_strerror(errbuf, sizeof(errbuf), my_errno()));
    }
  }
  if ((uint) fd < my_file_limit && my_file_info[fd].type != UNOPEN)
  {
    my_free(my_file_info[fd].name);
    my_file_info[fd].type = UNOPEN;
  }
  my_file_opened--;
  mysql_mutex_unlock(&THR_LOCK_open);
  DBUG_RETURN(err);
} /* my_close */


/*
  Register file in my_file_info[]
   
  SYNOPSIS
    my_register_filename()
    fd			   File number opened, -1 if error on open
    FileName		   File name
    type_file_type	   How file was created
    error_message_number   Error message number if caller got error (fd == -1)
    MyFlags		   Flags for my_close()

  RETURN
    -1   error
     #   Filenumber

*/

File my_register_filename(File fd, const char *FileName, enum file_type
			  type_of_file, uint error_message_number, myf MyFlags)
{
  char *dup_filename= NULL;
  DBUG_ENTER("my_register_filename");
  if ((int) fd >= MY_FILE_MIN)
  {
    if ((uint) fd >= my_file_limit)
    {
#if defined(_WIN32)
      set_my_errno(EMFILE);
#else
      mysql_mutex_lock(&THR_LOCK_open);
      my_file_opened++;
      mysql_mutex_unlock(&THR_LOCK_open);
      DBUG_RETURN(fd);				/* safeguard */
#endif
    }
    else
    {
      dup_filename= my_strdup(key_memory_my_file_info, FileName, MyFlags);
      if (dup_filename != NULL)
      {
        mysql_mutex_lock(&THR_LOCK_open);
        my_file_info[fd].name= dup_filename;
        my_file_opened++;
        my_file_total_opened++;
        my_file_info[fd].type = type_of_file;
        mysql_mutex_unlock(&THR_LOCK_open);
        DBUG_PRINT("exit",("fd: %d",fd));
        DBUG_RETURN(fd);
      }
      set_my_errno(ENOMEM);
    }
    (void) my_close(fd, MyFlags);
  }
  else
    set_my_errno(errno);

  DBUG_PRINT("error",("Got error %d on open", my_errno()));
  if (MyFlags & (MY_FFNF | MY_FAE | MY_WME))
  {
    char errbuf[MYSYS_STRERROR_SIZE];
    if (my_errno() == EMFILE)
      error_message_number= EE_OUT_OF_FILERESOURCES;
    DBUG_PRINT("error",("print err: %d",error_message_number));
    my_error(error_message_number, MYF(0), FileName,
             my_errno(), my_strerror(errbuf, sizeof(errbuf), my_errno()));
  }
  DBUG_RETURN(-1);
}




#ifdef EXTRA_DEBUG

void my_print_open_files(void)
{
  if (my_file_opened | my_stream_opened)
  {
    uint i;
    for (i= 0 ; i < my_file_limit ; i++)
    {
      if (my_file_info[i].type != UNOPEN)
      {
        my_message_local(INFORMATION_LEVEL,
                         EE(EE_FILE_NOT_CLOSED), my_file_info[i].name, i);
      }
    }
  }
}

#endif
