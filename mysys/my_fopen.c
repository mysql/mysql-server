/* Copyright (C) 2000 MySQL AB, 2008-2009 Sun Microsystems, Inc

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

#if defined(__FreeBSD__)
extern int getosreldate(void);
#endif

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

  make_ftype(type,flags);

#ifdef _WIN32
  fd= my_win_fopen(filename, type);
#else
  fd= fopen(filename, type);
#endif
  if (fd != 0)
  {
    /*
      The test works if MY_NFILE < 128. The problem is that fileno() is char
      on some OS (SUNOS). Actually the filename save isn't that important
      so we can ignore if this doesn't work.
    */

    int filedesc= my_fileno(fd);
    if ((uint)filedesc >= my_file_limit)
    {
      thread_safe_increment(my_stream_opened,&THR_LOCK_open);
      DBUG_RETURN(fd);				/* safeguard */
    }
    mysql_mutex_lock(&THR_LOCK_open);
    if ((my_file_info[filedesc].name= (char*)
	 my_strdup(filename,MyFlags)))
    {
      my_stream_opened++;
      my_file_total_opened++;
      my_file_info[filedesc].type= STREAM_BY_FOPEN;
      mysql_mutex_unlock(&THR_LOCK_open);
      DBUG_PRINT("exit",("stream: 0x%lx", (long) fd));
      DBUG_RETURN(fd);
    }
    mysql_mutex_unlock(&THR_LOCK_open);
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


#if defined(_WIN32)

static FILE *my_win_freopen(const char *path, const char *mode, FILE *stream)
{
  int handle_fd, fd= _fileno(stream);
  HANDLE osfh;

  DBUG_ASSERT(path && stream);

  /* Services don't have stdout/stderr on Windows, so _fileno returns -1. */
  if (fd < 0)
  {
    if (!freopen(path, mode, stream))
      return NULL;

    fd= _fileno(stream);
  }

  if ((osfh= CreateFile(path, GENERIC_READ | GENERIC_WRITE,
                        FILE_SHARE_READ | FILE_SHARE_WRITE |
                        FILE_SHARE_DELETE, NULL,
                        OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                        NULL)) == INVALID_HANDLE_VALUE)
    return NULL;

  if ((handle_fd= _open_osfhandle((intptr_t)osfh,
                                  _O_APPEND | _O_TEXT)) == -1)
  {
    CloseHandle(osfh);
    return NULL;
  }

  if (_dup2(handle_fd, fd) < 0)
  {
    CloseHandle(osfh);
    return NULL;
  }

  _close(handle_fd);

  return stream;
}

#elif defined(__FreeBSD__)

/* No close operation hook. */

static int no_close(void *cookie __attribute__((unused)))
{
  return 0;
}

/*
  A hack around a race condition in the implementation of freopen.

  The race condition steams from the fact that the current fd of
  the stream is closed before its number is used to duplicate the
  new file descriptor. This defeats the desired atomicity of the
  close and duplicate of dup2().

  See PR number 79887 for reference:
  http://www.freebsd.org/cgi/query-pr.cgi?pr=79887
*/

static FILE *my_freebsd_freopen(const char *path, const char *mode, FILE *stream)
{
  int old_fd;
  FILE *result;

  flockfile(stream);

  old_fd= fileno(stream);

  /* Use a no operation close hook to avoid having the fd closed. */
  stream->_close= no_close;

  /* Relies on the implicit dup2 to close old_fd. */
  result= freopen(path, mode, stream);

  /* If successful, the _close hook was replaced. */

  if (result == NULL)
    close(old_fd);
  else
    funlockfile(result);

  return result;
}

#endif


/**
  Change the file associated with a file stream.

  @param path   Path to file.
  @param mode   Mode of the stream.
  @param stream File stream.

  @note
    This function is used to redirect stdout and stderr to a file and
    subsequently to close and reopen that file for log rotation.

  @retval A FILE pointer on success. Otherwise, NULL.
*/

FILE *my_freopen(const char *path, const char *mode, FILE *stream)
{
  FILE *result;

#if defined(_WIN32)
  result= my_win_freopen(path, mode, stream);
#elif defined(__FreeBSD__)
  /*
    XXX: Once the fix is ported to the stable releases, this should
         be dependent upon the specific FreeBSD versions. Check at:
         http://www.freebsd.org/cgi/query-pr.cgi?pr=79887
  */
  if (getosreldate() > 900027)
    result= freopen(path, mode, stream);
  else
    result= my_freebsd_freopen(path, mode, stream);
#else
  result= freopen(path, mode, stream);
#endif

  return result;
}


/* Close a stream */
int my_fclose(FILE *fd, myf MyFlags)
{
  int err,file;
  DBUG_ENTER("my_fclose");
  DBUG_PRINT("my",("stream: 0x%lx  MyFlags: %d", (long) fd, MyFlags));

  mysql_mutex_lock(&THR_LOCK_open);
  file= my_fileno(fd);
#ifndef _WIN32
  err= fclose(fd);
#else
  err= my_win_fclose(fd);
#endif
  if(err < 0)
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
    my_free(my_file_info[file].name);
  }
  mysql_mutex_unlock(&THR_LOCK_open);
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
#ifdef _WIN32
  fd= my_win_fdopen(Filedes, type);
#else
  fd= fdopen(Filedes, type);
#endif
  if (!fd)
  {
    my_errno=errno;
    if (MyFlags & (MY_FAE | MY_WME))
      my_error(EE_CANT_OPEN_STREAM, MYF(ME_BELL+ME_WAITTANG),errno);
  }
  else
  {
    mysql_mutex_lock(&THR_LOCK_open);
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
    mysql_mutex_unlock(&THR_LOCK_open);
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
