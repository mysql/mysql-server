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
#include "mysys_err.h"
#include <my_dir.h>
#include <errno.h>
#if defined(__WIN__)
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
#if defined(__WIN__)
  /* 
    Check that we don't try to open or create a file name that may
    cause problems for us in the future (like PRN)
  */  
  if (check_if_legal_filename(FileName))
  {
    errno= EACCES;
    DBUG_RETURN(my_register_filename(-1, FileName, FILE_BY_OPEN,
                                     EE_FILENOTFOUND, MyFlags));
  }
#ifndef __WIN__
  if (Flags & O_SHARE)
    fd = sopen((char *) FileName, (Flags & ~O_SHARE) | O_BINARY, SH_DENYNO,
	       MY_S_IREAD | MY_S_IWRITE);
  else
    fd = open((char *) FileName, Flags | O_BINARY,
	      MY_S_IREAD | MY_S_IWRITE);
#else
  fd= my_sopen((char *) FileName, (Flags & ~O_SHARE) | O_BINARY, SH_DENYNO,
	       MY_S_IREAD | MY_S_IWRITE);
#endif

#elif !defined(NO_OPEN_3)
  fd = open(FileName, Flags, my_umask);	/* Normal unix */
#else
  fd = open((char *) FileName, Flags);
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
  do
  {
    err= close(fd);
  } while (err == -1 && errno == EINTR);

  if (err)
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
  DBUG_ENTER("my_register_filename");
  if ((int) fd >= 0)
  {
    if ((uint) fd >= my_file_limit)
    {
#if defined(THREAD) && !defined(HAVE_PREAD)
      my_errno= EMFILE;
#else
      thread_safe_increment(my_file_opened,&THR_LOCK_open);
      DBUG_RETURN(fd);				/* safeguard */
#endif
    }
    else
    {
      pthread_mutex_lock(&THR_LOCK_open);
      if ((my_file_info[fd].name = (char*) my_strdup(FileName,MyFlags)))
      {
        my_file_opened++;
        my_file_total_opened++;
        my_file_info[fd].type = type_of_file;
#if defined(THREAD) && !defined(HAVE_PREAD)
        pthread_mutex_init(&my_file_info[fd].mutex,MY_MUTEX_INIT_FAST);
#endif
        pthread_mutex_unlock(&THR_LOCK_open);
        DBUG_PRINT("exit",("fd: %d",fd));
        DBUG_RETURN(fd);
      }
      pthread_mutex_unlock(&THR_LOCK_open);
      my_errno= ENOMEM;
    }
    (void) my_close(fd, MyFlags);
  }
  else
    my_errno= errno;

  DBUG_PRINT("error",("Got error %d on open", my_errno));
  if (MyFlags & (MY_FFNF | MY_FAE | MY_WME))
  {
    if (my_errno == EMFILE)
      error_message_number= EE_OUT_OF_FILERESOURCES;
    DBUG_PRINT("error",("print err: %d",error_message_number));
    my_error(error_message_number, MYF(ME_BELL+ME_WAITTANG),
             FileName, my_errno);
  }
  DBUG_RETURN(-1);
}

#ifdef __WIN__

extern void __cdecl _dosmaperr(unsigned long);

/*
  Open a file with sharing. Similar to _sopen() from libc, but allows managing
  share delete on win32

  SYNOPSIS
    my_sopen()
      path    fully qualified file name
      oflag   operation flags
      shflag	share flag
      pmode   permission flags

  RETURN VALUE
    File descriptor of opened file if success
    -1 and sets errno if fails.
*/

File my_sopen(const char *path, int oflag, int shflag, int pmode)
{
  int  fh;                                /* handle of opened file */
  int mask;
  HANDLE osfh;                            /* OS handle of opened file */
  DWORD fileaccess;                       /* OS file access (requested) */
  DWORD fileshare;                        /* OS file sharing mode */
  DWORD filecreate;                       /* OS method of opening/creating */
  DWORD fileattrib;                       /* OS file attribute flags */
  SECURITY_ATTRIBUTES SecurityAttributes;

  if (!is_filename_allowed(path, strlen(path)))
  {
    errno= EINVAL;
    _doserrno= 0L;        /* not an OS error */
    return -1;
  }

  SecurityAttributes.nLength= sizeof(SecurityAttributes);
  SecurityAttributes.lpSecurityDescriptor= NULL;
  SecurityAttributes.bInheritHandle= !(oflag & _O_NOINHERIT);

  /*
   * decode the access flags
   */
  switch (oflag & (_O_RDONLY | _O_WRONLY | _O_RDWR)) {
    case _O_RDONLY:         /* read access */
      fileaccess= GENERIC_READ;
      break;
    case _O_WRONLY:         /* write access */
      fileaccess= GENERIC_WRITE;
      break;
    case _O_RDWR:           /* read and write access */
      fileaccess= GENERIC_READ | GENERIC_WRITE;
      break;
    default:                /* error, bad oflag */
      errno= EINVAL;
      _doserrno= 0L;        /* not an OS error */
      return -1;
  }

  /*
   * decode sharing flags
   */
  switch (shflag) {
    case _SH_DENYRW:        /* exclusive access except delete */
      fileshare= FILE_SHARE_DELETE;
      break;
    case _SH_DENYWR:        /* share read and delete access */
      fileshare= FILE_SHARE_READ | FILE_SHARE_DELETE;
      break;
    case _SH_DENYRD:        /* share write and delete access */
      fileshare= FILE_SHARE_WRITE | FILE_SHARE_DELETE;
      break;
    case _SH_DENYNO:        /* share read, write and delete access */
      fileshare= FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
      break;
    case _SH_DENYRWD:       /* exclusive access */
      fileshare= 0L;
      break;
    case _SH_DENYWRD:       /* share read access */
      fileshare= FILE_SHARE_READ;
      break;
    case _SH_DENYRDD:       /* share write access */
      fileshare= FILE_SHARE_WRITE;
      break;
    case _SH_DENYDEL:       /* share read and write access */
      fileshare= FILE_SHARE_READ | FILE_SHARE_WRITE;
      break;
    default:                /* error, bad shflag */
      errno= EINVAL;
      _doserrno= 0L;        /* not an OS error */
      return -1;
  }

  /*
   * decode open/create method flags
   */
  switch (oflag & (_O_CREAT | _O_EXCL | _O_TRUNC)) {
    case 0:
    case _O_EXCL:                   /* ignore EXCL w/o CREAT */
      filecreate= OPEN_EXISTING;
      break;

    case _O_CREAT:
      filecreate= OPEN_ALWAYS;
      break;

    case _O_CREAT | _O_EXCL:
    case _O_CREAT | _O_TRUNC | _O_EXCL:
      filecreate= CREATE_NEW;
      break;

    case _O_TRUNC:
    case _O_TRUNC | _O_EXCL:        /* ignore EXCL w/o CREAT */
      filecreate= TRUNCATE_EXISTING;
      break;

    case _O_CREAT | _O_TRUNC:
      filecreate= CREATE_ALWAYS;
      break;

    default:
      /* this can't happen ... all cases are covered */
      errno= EINVAL;
      _doserrno= 0L;
      return -1;
  }

  /*
   * decode file attribute flags if _O_CREAT was specified
   */
  fileattrib= FILE_ATTRIBUTE_NORMAL;     /* default */
  if (oflag & _O_CREAT) 
  {
    _umask((mask= _umask(0)));
    
    if (!((pmode & ~mask) & _S_IWRITE))
      fileattrib= FILE_ATTRIBUTE_READONLY;
  }

  /*
   * Set temporary file (delete-on-close) attribute if requested.
   */
  if (oflag & _O_TEMPORARY) 
  {
    fileattrib|= FILE_FLAG_DELETE_ON_CLOSE;
    fileaccess|= DELETE;
  }

  /*
   * Set temporary file (delay-flush-to-disk) attribute if requested.
   */
  if (oflag & _O_SHORT_LIVED)
    fileattrib|= FILE_ATTRIBUTE_TEMPORARY;

  /*
   * Set sequential or random access attribute if requested.
   */
  if (oflag & _O_SEQUENTIAL)
    fileattrib|= FILE_FLAG_SEQUENTIAL_SCAN;
  else if (oflag & _O_RANDOM)
    fileattrib|= FILE_FLAG_RANDOM_ACCESS;

  /*
   * try to open/create the file
   */
  if ((osfh= CreateFile(path, fileaccess, fileshare, &SecurityAttributes, 
                        filecreate, fileattrib, NULL)) == INVALID_HANDLE_VALUE)
  {
    /*
     * OS call to open/create file failed! map the error, release
     * the lock, and return -1. note that it's not necessary to
     * call _free_osfhnd (it hasn't been used yet).
     */
    _dosmaperr(GetLastError());     /* map error */
    return -1;                      /* return error to caller */
  }

  if ((fh= _open_osfhandle((intptr_t)osfh, 
                           oflag & (_O_APPEND | _O_RDONLY | _O_TEXT))) == -1)
  {
    _dosmaperr(GetLastError());     /* map error */
    CloseHandle(osfh);
  }

  return fh;                        /* return handle */
}
#endif /* __WIN__ */


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
        fprintf(stderr, EE(EE_FILE_NOT_CLOSED), my_file_info[i].name, i);
        fputc('\n', stderr);
      }
    }
  }
}

#endif
