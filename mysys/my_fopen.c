/* Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

#include "mysys_priv.h"
#include "my_static.h"
#include <errno.h>
#include "mysys_err.h"
#include "my_thread_local.h"


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
  char *dup_filename= NULL;
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
      mysql_mutex_lock(&THR_LOCK_open);
      my_stream_opened++;
      mysql_mutex_unlock(&THR_LOCK_open);
      DBUG_RETURN(fd);				/* safeguard */
    }
    dup_filename= my_strdup(key_memory_my_file_info, filename, MyFlags);
    if (dup_filename != NULL)
    {
      mysql_mutex_lock(&THR_LOCK_open);
      my_file_info[filedesc].name= dup_filename;
      my_stream_opened++;
      my_file_total_opened++;
      my_file_info[filedesc].type= STREAM_BY_FOPEN;
      mysql_mutex_unlock(&THR_LOCK_open);
      DBUG_PRINT("exit",("stream: 0x%lx", (long) fd));
      DBUG_RETURN(fd);
    }
    (void) my_fclose(fd,MyFlags);
    set_my_errno(ENOMEM);
  }
  else
    set_my_errno(errno);
  DBUG_PRINT("error",("Got error %d on open",my_errno()));
  if (MyFlags & (MY_FFNF | MY_FAE | MY_WME))
  {
    char errbuf[MYSYS_STRERROR_SIZE];
    my_error((flags & O_RDONLY) || (flags == O_RDONLY ) ? EE_FILENOTFOUND :
             EE_CANTCREATEFILE,
             MYF(0), filename,
             my_errno(), my_strerror(errbuf, sizeof(errbuf), my_errno()));
  }
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
  {
    _close(fd);
    return NULL;
  }

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
#else

/**
  Replacement for freopen() which will not close the stream until the
  new file has been successfully opened. This gives the ability log
  the reason for reopen's failure to the stream, and also to flush any
  buffered messages already written to the stream, before terminating
  the process.

  After a new stream to path has been opened, its file descriptor is
  obtained, and dup2() is used to associate the original stream with
  the new file. The newly opened stream and associated file descriptor
  is then closed with fclose().

  @param path file which the stream should be opened to
  @param mode FILE mode to use when opening new file
  @param stream stream to reopen

  @retval FILE stream being passed in if successful,
  @retval nullptr otherwise
 */
static FILE *my_safe_freopen(const char *path, const char *mode, FILE *stream)
{
  int streamfd= -1;
  FILE *pathstream= NULL;
  int pathfd= -1;
  int ds= -1;

  DBUG_ASSERT(path != NULL && stream != NULL);
  streamfd= fileno(stream);
  if (streamfd == -1)
  {
    /* We have not done anything to the stream, but if we cannot
       get its fd, it is probably in a bad state anyway... */
    return NULL;
  }
  pathstream= fopen(path, mode);
  if (pathstream == NULL)
  {
    /* Failed to open file for some reason. stream should still be
       usable. */
    return NULL;
  }

  pathfd= fileno(pathstream);
  if (pathfd == -1)
  {
    fclose(pathstream);
    return NULL;
  }
  do
  {
    ds= fflush(stream);
    if (ds == 0)
    {
      ds= dup2(pathfd, streamfd);
    }
  }
  while (ds == -1 && errno == EINTR);
  fclose(pathstream);
  return (ds == -1 ? NULL : stream);
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
#else
  result= my_safe_freopen(path, mode, stream);
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
    set_my_errno(errno);
    if (MyFlags & (MY_FAE | MY_WME))
    {
      char errbuf[MYSYS_STRERROR_SIZE];
      my_error(EE_BADCLOSE, MYF(0), my_filename(file),
               my_errno(), my_strerror(errbuf, sizeof(errbuf), my_errno()));
    }
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
    set_my_errno(errno);
    if (MyFlags & (MY_FAE | MY_WME))
    {
      char errbuf[MYSYS_STRERROR_SIZE];
      my_error(EE_CANT_OPEN_STREAM, MYF(0),
               my_errno(), my_strerror(errbuf, sizeof(errbuf), my_errno()));
    }
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
        my_file_info[Filedes].name= my_strdup(key_memory_my_file_info,
                                              name,MyFlags);
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

static void make_ftype(char * to, int flag)
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
