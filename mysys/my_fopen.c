/* Copyright (C) 2000 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "mysys_priv.h"
#include "my_static.h"
#include <errno.h>
#include "mysys_err.h"

static void make_ftype(char * to,int flag);

/*
  Open a file as stream

  SYNOPSIS
    my_fopen()
    FileName	Path-name of file
    Flags	Read | write | append | trunc (like for open())
    MyFlags	Flags for handling errors

  RETURN
    0	Error
    #	File handler
*/

FILE *my_fopen(const char *filename, int flags, myf MyFlags)
{
  FILE *fd;
  char type[5];
  DBUG_ENTER("my_fopen");
  DBUG_PRINT("my",("Name: '%s'  flags: %d  MyFlags: %d",
		   filename, flags, MyFlags));
  /* 
    if we are not creating, then we need to use my_access to make sure  
    the file exists since Windows doesn't handle files like "com1.sym" 
    very  well 
  */
#ifdef __WIN__
  if (check_if_legal_filename(filename))
  {
    errno= EACCES;
    fd= 0;
  }
  else
#endif
  {
    make_ftype(type,flags);
    fd = fopen(filename, type);
  }
  
  if (fd != 0)
  {
    /*
      The test works if MY_NFILE < 128. The problem is that fileno() is char
      on some OS (SUNOS). Actually the filename save isn't that important
      so we can ignore if this doesn't work.
    */
    if ((uint) fileno(fd) >= my_file_limit)
    {
      thread_safe_increment(my_stream_opened,&THR_LOCK_open);
      DBUG_RETURN(fd);				/* safeguard */
    }
    pthread_mutex_lock(&THR_LOCK_open);
    if ((my_file_info[fileno(fd)].name = (char*)
	 my_strdup(filename,MyFlags)))
    {
      my_stream_opened++;
      my_file_total_opened++;
      my_file_info[fileno(fd)].type = STREAM_BY_FOPEN;
      pthread_mutex_unlock(&THR_LOCK_open);
      DBUG_PRINT("exit",("stream: 0x%lx", (long) fd));
      DBUG_RETURN(fd);
    }
    pthread_mutex_unlock(&THR_LOCK_open);
    (void) my_fclose(fd,MyFlags);
    my_errno=ENOMEM;
  }
  else
    my_errno=errno;
  DBUG_PRINT("error",("Got error %d on open",my_errno));
  if (MyFlags & (MY_FFNF | MY_FAE | MY_WME))
    my_error((flags & O_RDONLY) || (flags == O_RDONLY ) ? EE_FILENOTFOUND :
	     EE_CANTCREATEFILE,
	     MYF(ME_BELL+ME_WAITTANG), filename, my_errno);
  DBUG_RETURN((FILE*) 0);
} /* my_fopen */


	/* Close a stream */

int my_fclose(FILE *fd, myf MyFlags)
{
  int err,file;
  DBUG_ENTER("my_fclose");
  DBUG_PRINT("my",("stream: 0x%lx  MyFlags: %d", (long) fd, MyFlags));

  pthread_mutex_lock(&THR_LOCK_open);
  file=fileno(fd);
  if ((err = fclose(fd)) < 0)
  {
    my_errno=errno;
    if (MyFlags & (MY_FAE | MY_WME))
      my_error(EE_BADCLOSE, MYF(ME_BELL+ME_WAITTANG),
	       my_filename(file),errno);
  }
  else
    my_stream_opened--;
  if ((uint) file < my_file_limit && my_file_info[file].type != UNOPEN)
  {
    my_file_info[file].type = UNOPEN;
    my_free(my_file_info[file].name, MYF(MY_ALLOW_ZERO_PTR));
  }
  pthread_mutex_unlock(&THR_LOCK_open);
  DBUG_RETURN(err);
} /* my_fclose */


	/* Make a stream out of a file handle */
	/* Name may be 0 */

FILE *my_fdopen(File Filedes, const char *name, int Flags, myf MyFlags)
{
  FILE *fd;
  char type[5];
  DBUG_ENTER("my_fdopen");
  DBUG_PRINT("my",("Fd: %d  Flags: %d  MyFlags: %d",
		   Filedes, Flags, MyFlags));

  make_ftype(type,Flags);
  if ((fd = fdopen(Filedes, type)) == 0)
  {
    my_errno=errno;
    if (MyFlags & (MY_FAE | MY_WME))
      my_error(EE_CANT_OPEN_STREAM, MYF(ME_BELL+ME_WAITTANG),errno);
  }
  else
  {
    pthread_mutex_lock(&THR_LOCK_open);
    my_stream_opened++;
    if ((uint) Filedes < (uint) my_file_limit)
    {
      if (my_file_info[Filedes].type != UNOPEN)
      {
        my_file_opened--;		/* File is opened with my_open ! */
      }
      else
      {
        my_file_info[Filedes].name=  my_strdup(name,MyFlags);
      }
      my_file_info[Filedes].type = STREAM_BY_FDOPEN;
    }
    pthread_mutex_unlock(&THR_LOCK_open);
  }

  DBUG_PRINT("exit",("stream: 0x%lx", (long) fd));
  DBUG_RETURN(fd);
} /* my_fdopen */


/*   
  Make a fopen() typestring from a open() type bitmap

  SYNOPSIS
    make_ftype()
    to		String for fopen() is stored here
    flag	Flag used by open()

  IMPLEMENTATION
    This routine attempts to find the best possible match 
    between  a numeric option and a string option that could be 
    fed to fopen.  There is not a 1 to 1 mapping between the two.  
  
  NOTE
    On Unix, O_RDONLY is usually 0

  MAPPING
    r  == O_RDONLY   
    w  == O_WRONLY|O_TRUNC|O_CREAT  
    a  == O_WRONLY|O_APPEND|O_CREAT  
    r+ == O_RDWR  
    w+ == O_RDWR|O_TRUNC|O_CREAT  
    a+ == O_RDWR|O_APPEND|O_CREAT
*/

static void make_ftype(register char * to, register int flag)
{
  /* check some possible invalid combinations */  
  DBUG_ASSERT((flag & (O_TRUNC | O_APPEND)) != (O_TRUNC | O_APPEND));
  DBUG_ASSERT((flag & (O_WRONLY | O_RDWR)) != (O_WRONLY | O_RDWR));

  if ((flag & (O_RDONLY|O_WRONLY)) == O_WRONLY)    
    *to++= (flag & O_APPEND) ? 'a' : 'w';  
  else if (flag & O_RDWR)          
  {
    /* Add '+' after theese */    
    if (flag & (O_TRUNC | O_CREAT))      
      *to++= 'w';    
    else if (flag & O_APPEND)      
      *to++= 'a';    
    else      
      *to++= 'r';
    *to++= '+';  
  }  
  else    
    *to++= 'r';

#if FILE_BINARY            /* If we have binary-files */  
  if (flag & FILE_BINARY)    
    *to++='b';
#endif  
  *to='\0';
} /* make_ftype */
