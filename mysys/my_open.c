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

#define USES_TYPES
#include "mysys_priv.h"
#include "mysys_err.h"
#include <my_dir.h>
#include <errno.h>
#if defined(MSDOS) || defined(__WIN__) || defined(__EMX__) || defined(OS2)
#include <share.h>
#endif

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
#if defined(MSDOS) || defined(__WIN__) || defined(__EMX__) || defined(OS2)
  /* if we are not creating, then we need to use my_access to make 
   * sure  the file exists since Windows doesn't handle files like 
   * "com1.sym" very  well 
  */  
  if (! (Flags & O_CREAT) && my_access(FileName, F_OK))    
	  return -1;
  if (Flags & O_SHARE)
    fd = sopen((my_string) FileName, (Flags & ~O_SHARE) | O_BINARY, SH_DENYNO,
	       MY_S_IREAD | MY_S_IWRITE);
  else
    fd = open((my_string) FileName, Flags | O_BINARY,
	      MY_S_IREAD | MY_S_IWRITE);
#elif !defined(NO_OPEN_3)
  fd = open(FileName, Flags, my_umask);	/* Normal unix */
#else
  fd = open((my_string) FileName, Flags);
#endif
  DBUG_RETURN(my_register_filename(fd, FileName, FILE_BY_OPEN,
				   EE_FILENOTFOUND, MyFlags));
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

  pthread_mutex_lock(&THR_LOCK_open);
  if ((err = close(fd)))
  {
    DBUG_PRINT("error",("Got error %d on close",err));
    my_errno=errno;
    if (MyFlags & (MY_FAE | MY_WME))
      my_error(EE_BADCLOSE, MYF(ME_BELL+ME_WAITTANG),my_filename(fd),errno);
  }
  if ((uint) fd < my_file_limit && my_file_info[fd].type != UNOPEN)
  {
    my_free(my_file_info[fd].name, MYF(0));
#if defined(THREAD) && !defined(HAVE_PREAD)
    pthread_mutex_destroy(&my_file_info[fd].mutex);
#endif
    my_file_info[fd].type = UNOPEN;
  }
  my_file_opened--;
  pthread_mutex_unlock(&THR_LOCK_open);
  DBUG_RETURN(err);
} /* my_close */


/*
  Register file in my_file_info[]
   
  SYNOPSIS
    my_register_filename()
      fd
      FileName
      type_file_type
*/

File my_register_filename(File fd, const char *FileName, enum file_type
			  type_of_file, uint error_message_number, myf MyFlags)
{
  if ((int) fd >= 0)
  {
    if ((uint) fd >= my_file_limit)
    {
#if defined(THREAD) && !defined(HAVE_PREAD)
      (void) my_close(fd,MyFlags);
      my_errno=EMFILE;
      if (MyFlags & (MY_FFNF | MY_FAE | MY_WME))
	my_error(EE_OUT_OF_FILERESOURCES, MYF(ME_BELL+ME_WAITTANG),
		 FileName, my_errno);
      return(-1);
#endif
      thread_safe_increment(my_file_opened,&THR_LOCK_open);
      return(fd);				/* safeguard */
    }
    pthread_mutex_lock(&THR_LOCK_open);
    if ((my_file_info[fd].name = (char*) my_strdup(FileName,MyFlags)))
    {
      my_file_opened++;
      my_file_info[fd].type = type_of_file;
#if defined(THREAD) && !defined(HAVE_PREAD)
      pthread_mutex_init(&my_file_info[fd].mutex,MY_MUTEX_INIT_FAST);
#endif
      pthread_mutex_unlock(&THR_LOCK_open);
      DBUG_PRINT("exit",("fd: %d",fd));
      return(fd);
    }
    pthread_mutex_unlock(&THR_LOCK_open);
    (void) my_close(fd, MyFlags);
    my_errno=ENOMEM;
  }
  else
    my_errno=errno;
  DBUG_PRINT("error",("Got error %d on open",my_errno));
  if (MyFlags & (MY_FFNF | MY_FAE | MY_WME))
    my_error(error_message_number, MYF(ME_BELL+ME_WAITTANG),
	     FileName, my_errno);
  return(fd);
}
