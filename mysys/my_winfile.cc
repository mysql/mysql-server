/* Copyright (c) 2008, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file mysys/my_winfile.cc
  The purpose of this file is to provide implementation of file IO routines on 
  Windows that can be thought as drop-in replacement for corresponding C runtime
  functionality.

  Compared to Windows CRT, this one 
  - does not have the same file descriptor 
  limitation (default is 16384 and can  be increased further, whereas CRT poses
  a hard limit of 2048 file descriptors)
  - the file operations are not serialized
  - positional IO pread/pwrite is ported here.
  - no text mode for files, all IO is "binary"

  Naming convention:
  All routines are prefixed with my_win_, e.g Posix open() is implemented with 
  my_win_open()

  Implemented are
  - POSIX routines(e.g open, read, lseek ...)
  - Some ANSI C stream routines (fopen, fdopen, fileno, fclose)
  - Windows CRT equivalents (my_get_osfhandle, open_osfhandle)

  Worth to note:
  - File descriptors used here are located in a range that is not compatible 
  with CRT on purpose. Attempt to use a file descriptor from Windows CRT library
  range in my_win_* function will be punished with DBUG_ASSERT()

  - File streams (FILE *) are actually from the C runtime. The routines provided
  here are useful only in scenarios that use low-level IO with my_win_fileno()
*/

#ifdef _WIN32

#include <errno.h>
#include <share.h>
#include <sys/stat.h>

#include "my_dbug.h"
#include "my_thread_local.h"
#include "mysys_priv.h"

/* Associates a file descriptor with an existing operating-system file handle.*/
File my_open_osfhandle(HANDLE handle, int oflag)
{
  int offset= -1;
  uint i;
  DBUG_ENTER("my_open_osfhandle");

  mysql_mutex_lock(&THR_LOCK_open);
  for(i= MY_FILE_MIN; i < my_file_limit;i++)
  {
    if(my_file_info[i].fhandle == 0)
    {
      struct st_my_file_info *finfo= &(my_file_info[i]);
      finfo->type=    FILE_BY_OPEN;
      finfo->fhandle= handle;
      finfo->oflag=   oflag;
      offset= i;
      break;
    }
  }
  mysql_mutex_unlock(&THR_LOCK_open);
  if(offset == -1)
    errno= EMFILE; /* to many file handles open */
  DBUG_RETURN(offset);
}


static void invalidate_fd(File fd)
{
  DBUG_ENTER("invalidate_fd");
  DBUG_ASSERT(fd >= MY_FILE_MIN && fd < (int)my_file_limit);
  my_file_info[fd].fhandle= 0;
  DBUG_VOID_RETURN;
}


/* Get Windows handle for a file descriptor */
HANDLE my_get_osfhandle(File fd)
{
  DBUG_ENTER("my_get_osfhandle");
  DBUG_ASSERT(fd >= MY_FILE_MIN && fd < (int)my_file_limit);
  DBUG_RETURN(my_file_info[fd].fhandle);
}


static int my_get_open_flags(File fd)
{
  DBUG_ENTER("my_get_open_flags");
  DBUG_ASSERT(fd >= MY_FILE_MIN && fd < (int)my_file_limit);
  DBUG_RETURN(my_file_info[fd].oflag);
}


/*
  Open a file with sharing. Similar to _sopen() from libc, but allows managing
  share delete on win32

  SYNOPSIS
  my_win_sopen()
  path    file name
  oflag   operation flags
  shflag  share flag
  pmode   permission flags

  RETURN VALUE
  File descriptor of opened file if success
  -1 and sets errno if fails.
*/

File my_win_sopen(const char *path, int oflag, int shflag, int pmode)
{
  int  fh;                                /* handle of opened file */
  int mask;
  HANDLE osfh;                            /* OS handle of opened file */
  DWORD fileaccess;                       /* OS file access (requested) */
  DWORD fileshare;                        /* OS file sharing mode */
  DWORD filecreate;                       /* OS method of opening/creating */
  DWORD fileattrib;                       /* OS file attribute flags */
  SECURITY_ATTRIBUTES SecurityAttributes;

  DBUG_ENTER("my_win_sopen");

  if (check_if_legal_filename(path))
  {
    errno= EACCES;
    DBUG_RETURN(-1);
  }
  SecurityAttributes.nLength= sizeof(SecurityAttributes);
  SecurityAttributes.lpSecurityDescriptor= NULL;
  SecurityAttributes.bInheritHandle= !(oflag & _O_NOINHERIT);

  /* decode the access flags  */
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
      DBUG_RETURN(-1);
  }

  /* decode sharing flags */
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
      DBUG_RETURN(-1);
  }

  /* decode open/create method flags  */
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
      DBUG_RETURN(-1);
  }

  /* decode file attribute flags if _O_CREAT was specified */
  fileattrib= FILE_ATTRIBUTE_NORMAL;     /* default */
  if (oflag & _O_CREAT) 
  {
    _umask((mask= _umask(0)));

    if (!((pmode & ~mask) & _S_IWRITE))
      fileattrib= FILE_ATTRIBUTE_READONLY;
  }

  /* Set temporary file (delete-on-close) attribute if requested. */
  if (oflag & _O_TEMPORARY) 
  {
    fileattrib|= FILE_FLAG_DELETE_ON_CLOSE;
    fileaccess|= DELETE;
  }

  /* Set temporary file (delay-flush-to-disk) attribute if requested.*/
  if (oflag & _O_SHORT_LIVED)
    fileattrib|= FILE_ATTRIBUTE_TEMPORARY;

  /* Set sequential or random access attribute if requested. */
  if (oflag & _O_SEQUENTIAL)
    fileattrib|= FILE_FLAG_SEQUENTIAL_SCAN;
  else if (oflag & _O_RANDOM)
    fileattrib|= FILE_FLAG_RANDOM_ACCESS;

  /* try to open/create the file  */
  if ((osfh= CreateFile(path, fileaccess, fileshare, &SecurityAttributes, 
    filecreate, fileattrib, NULL)) == INVALID_HANDLE_VALUE)
  {
    /*
       OS call to open/create file failed! map the error, release
       the lock, and return -1. note that it's not necessary to
       call _free_osfhnd (it hasn't been used yet).
    */
    my_osmaperr(GetLastError());     /* map error */
    DBUG_RETURN(-1);                 /* return error to caller */
  }

  if ((fh= my_open_osfhandle(osfh, 
    oflag & (_O_APPEND | _O_RDONLY | _O_TEXT))) == -1)
  {
    CloseHandle(osfh);
  }

  DBUG_RETURN(fh);                   /* return handle */
}


File my_win_open(const char *path, int flags)
{
  DBUG_ENTER("my_win_open");
  DBUG_RETURN(my_win_sopen((char *) path, flags | _O_BINARY, _SH_DENYNO, 
    _S_IREAD | S_IWRITE));
}


int my_win_close(File fd)
{
  DBUG_ENTER("my_win_close");
  if(CloseHandle(my_get_osfhandle(fd)))
  {
    invalidate_fd(fd);
    DBUG_RETURN(0);
  }
  my_osmaperr(GetLastError());
  DBUG_RETURN(-1);
}


size_t my_win_pread(File Filedes, uchar *Buffer, size_t Count, my_off_t offset)
{
  DWORD         nBytesRead;
  HANDLE        hFile;
  OVERLAPPED    ov= {0};
  LARGE_INTEGER li;

  DBUG_ENTER("my_win_pread");

  if(!Count)
    DBUG_RETURN(0);
  if(Count > UINT_MAX)
    Count= UINT_MAX;

  hFile=         (HANDLE)my_get_osfhandle(Filedes);
  li.QuadPart=   offset;
  ov.Offset=     li.LowPart;
  ov.OffsetHigh= li.HighPart;

  if(!ReadFile(hFile, Buffer, (DWORD)Count, &nBytesRead, &ov))
  {
    DWORD lastError= GetLastError();
    /*
      ERROR_BROKEN_PIPE is returned when no more data coming
      through e.g. a command pipe in windows : see MSDN on ReadFile.
    */
    if(lastError == ERROR_HANDLE_EOF || lastError == ERROR_BROKEN_PIPE)
      DBUG_RETURN(0); /*return 0 at EOF*/
    my_osmaperr(lastError);
    DBUG_RETURN((size_t)-1);
  }
  DBUG_RETURN(nBytesRead);
}


size_t my_win_read(File Filedes, uchar *Buffer, size_t Count)
{
  DWORD         nBytesRead;
  HANDLE        hFile;

  DBUG_ENTER("my_win_read");
  if(!Count)
    DBUG_RETURN(0);
  if(Count > UINT_MAX)
    Count= UINT_MAX;

  hFile= (HANDLE)my_get_osfhandle(Filedes);

  if(!ReadFile(hFile, Buffer, (DWORD)Count, &nBytesRead, NULL))
  {
    DWORD lastError= GetLastError();
    /*
      ERROR_BROKEN_PIPE is returned when no more data coming
      through e.g. a command pipe in windows : see MSDN on ReadFile.
    */
    if(lastError == ERROR_HANDLE_EOF || lastError == ERROR_BROKEN_PIPE)
      DBUG_RETURN(0); /*return 0 at EOF*/
    my_osmaperr(lastError);
    DBUG_RETURN((size_t)-1);
  }
  DBUG_RETURN(nBytesRead);
}


size_t my_win_pwrite(File Filedes, const uchar *Buffer, size_t Count, 
                     my_off_t offset)
{
  DWORD         nBytesWritten;
  HANDLE        hFile;
  OVERLAPPED    ov= {0};
  LARGE_INTEGER li;

  DBUG_ENTER("my_win_pwrite");
  DBUG_PRINT("my",("Filedes: %d, Buffer: %p, Count: %llu, offset: %llu", 
    Filedes, Buffer, (ulonglong)Count, (ulonglong)offset));

  if(!Count)
    DBUG_RETURN(0);

  if(Count > UINT_MAX)
    Count= UINT_MAX;

  hFile=         (HANDLE)my_get_osfhandle(Filedes);
  li.QuadPart=   offset;
  ov.Offset=     li.LowPart;
  ov.OffsetHigh= li.HighPart;

  if(!WriteFile(hFile, Buffer, (DWORD)Count, &nBytesWritten, &ov))
  {
    my_osmaperr(GetLastError());
    DBUG_RETURN((size_t)-1);
  }
  else
    DBUG_RETURN(nBytesWritten);
}


my_off_t my_win_lseek(File fd, my_off_t pos, int whence)
{
  LARGE_INTEGER offset;
  LARGE_INTEGER newpos;

  DBUG_ENTER("my_win_lseek");

  static_assert(FILE_BEGIN == SEEK_SET && FILE_CURRENT == SEEK_CUR
                  && FILE_END == SEEK_END,
                "Windows and POSIX seek constants must be compatible.");

  offset.QuadPart= pos;
  if(!SetFilePointerEx(my_get_osfhandle(fd), offset, &newpos, whence))
  {
    my_osmaperr(GetLastError());
    newpos.QuadPart= -1;
  }
  DBUG_RETURN(newpos.QuadPart);
}


#ifndef FILE_WRITE_TO_END_OF_FILE
#define FILE_WRITE_TO_END_OF_FILE       0xffffffff
#endif
size_t my_win_write(File fd, const uchar *Buffer, size_t Count)
{
  DWORD nWritten;
  OVERLAPPED ov;
  OVERLAPPED *pov= NULL;
  HANDLE hFile;

  DBUG_ENTER("my_win_write");
  DBUG_PRINT("my",("Filedes: %d, Buffer: %p, Count %llu", fd, Buffer, 
      (ulonglong)Count));

  if(!Count)
    DBUG_RETURN(0);

  if(Count > UINT_MAX)
    Count= UINT_MAX;

  if(my_get_open_flags(fd) & _O_APPEND)
  {
    /*
       Atomic append to the end of file is is done by special initialization of 
       the OVERLAPPED structure. See MSDN WriteFile documentation for more info.
    */
    memset(&ov, 0, sizeof(ov));
    ov.Offset= FILE_WRITE_TO_END_OF_FILE; 
    ov.OffsetHigh= -1;
    pov= &ov;
  }

  hFile= my_get_osfhandle(fd);
  if(!WriteFile(hFile, Buffer, (DWORD)Count, &nWritten, pov))
  {
    my_osmaperr(GetLastError());
    DBUG_RETURN((size_t)-1);
  }
  DBUG_RETURN(nWritten);
}


int my_win_chsize(File fd,  my_off_t newlength)
{
  HANDLE hFile;
  LARGE_INTEGER length;
  DBUG_ENTER("my_win_chsize");

  hFile= (HANDLE) my_get_osfhandle(fd);
  length.QuadPart= newlength;
  if (!SetFilePointerEx(hFile, length , NULL , FILE_BEGIN))
    goto err;
  if (!SetEndOfFile(hFile))
    goto err;
  DBUG_RETURN(0);
err:
  my_osmaperr(GetLastError());
  set_my_errno(errno);
  DBUG_RETURN(-1);
}


/* Get the file descriptor for stdin,stdout or stderr */
static File my_get_stdfile_descriptor(FILE *stream)
{
  HANDLE hFile;
  DWORD nStdHandle;
  DBUG_ENTER("my_get_stdfile_descriptor");

  if(stream == stdin)
    nStdHandle= STD_INPUT_HANDLE;
  else if(stream == stdout)
    nStdHandle= STD_OUTPUT_HANDLE;
  else if(stream == stderr)
    nStdHandle= STD_ERROR_HANDLE;
  else
    DBUG_RETURN(-1);

  hFile= GetStdHandle(nStdHandle);
  if(hFile != INVALID_HANDLE_VALUE)
    DBUG_RETURN(my_open_osfhandle(hFile, 0));
  DBUG_RETURN(-1);
}


File my_win_fileno(FILE *file)
{
  HANDLE hFile= (HANDLE)_get_osfhandle(fileno(file));
  int retval= -1;
  uint i;

  DBUG_ENTER("my_win_fileno");

  for(i= MY_FILE_MIN; i < my_file_limit; i++)
  {
    if(my_file_info[i].fhandle == hFile)
    {
      retval= i;
      break;
    }
  }
  if(retval == -1)
    /* try std stream */
    DBUG_RETURN(my_get_stdfile_descriptor(file));
  DBUG_RETURN(retval);
}


FILE *my_win_fopen(const char *filename, const char *type)
{
  FILE *file;
  int flags= 0;
  DBUG_ENTER("my_win_open");

  /* 
    If we are not creating, then we need to use my_access to make sure  
    the file exists since Windows doesn't handle files like "com1.sym" 
    very  well
  */
  if (check_if_legal_filename(filename))
  {
    errno= EACCES;
    DBUG_RETURN(NULL);
  }

  file= fopen(filename, type);
  if(!file)
    DBUG_RETURN(NULL);

  if(strchr(type,'a') != NULL)
    flags= O_APPEND;

  /*
     Register file handle in my_table_info.
     Necessary for my_fileno()
   */
  if(my_open_osfhandle((HANDLE)_get_osfhandle(fileno(file)), flags) < 0)
  {
    fclose(file);
    DBUG_RETURN(NULL);
  }
  DBUG_RETURN(file);
}


FILE * my_win_fdopen(File fd, const char *type)
{
  FILE *file;
  int crt_fd;
  int flags= 0;

  DBUG_ENTER("my_win_fdopen");

  if(strchr(type,'a') != NULL)
    flags= O_APPEND;
  /* Convert OS file handle to CRT file descriptor and then call fdopen*/
  crt_fd= _open_osfhandle((intptr_t)my_get_osfhandle(fd), flags);
  if(crt_fd < 0)
    file= NULL;
  else
    file= fdopen(crt_fd, type);
  DBUG_RETURN(file);
}


int my_win_fclose(FILE *file)
{
  File fd;

  DBUG_ENTER("my_win_close");
  fd= my_fileno(file);
  if(fd < 0)
    DBUG_RETURN(-1);
  if(fclose(file) < 0)
    DBUG_RETURN(-1);
  invalidate_fd(fd);
  DBUG_RETURN(0);
}



/*
  Quick and dirty my_fstat() implementation for Windows.
  Use CRT fstat on temporarily allocated file descriptor.
  Patch file size, because size that fstat returns is not 
  reliable (may be outdated)
*/
int my_win_fstat(File fd, struct _stati64 *buf)
{
  int crt_fd;
  int retval;
  HANDLE hFile, hDup;

  DBUG_ENTER("my_win_fstat");

  hFile= my_get_osfhandle(fd);
  if(!DuplicateHandle( GetCurrentProcess(), hFile, GetCurrentProcess(), 
    &hDup ,0,FALSE,DUPLICATE_SAME_ACCESS))
  {
    my_osmaperr(GetLastError());
    DBUG_RETURN(-1);
  }
  if ((crt_fd= _open_osfhandle((intptr_t)hDup,0)) < 0)
    DBUG_RETURN(-1);

  retval= _fstati64(crt_fd, buf);
  if(retval == 0)
  {
    /* File size returned by stat is not accurate (may be outdated), fix it*/
    GetFileSizeEx(hDup, (PLARGE_INTEGER) (&(buf->st_size)));
  }
  _close(crt_fd);
  DBUG_RETURN(retval);
}



int my_win_stat( const char *path, struct _stati64 *buf)
{
  DBUG_ENTER("my_win_stat");
  if(_stati64( path, buf) == 0)
  {
    /* File size returned by stat is not accurate (may be outdated), fix it*/
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (GetFileAttributesEx(path, GetFileExInfoStandard, &data))
    {
      LARGE_INTEGER li;
      li.LowPart=    data.nFileSizeLow;
      li.HighPart=   data.nFileSizeHigh;
      buf->st_size= li.QuadPart;
    }
    DBUG_RETURN(0);
  }
  DBUG_RETURN(-1);
}



int my_win_fsync(File fd)
{
  DBUG_ENTER("my_win_fsync");
  if(FlushFileBuffers(my_get_osfhandle(fd)))
    DBUG_RETURN(0);
  my_osmaperr(GetLastError());
  DBUG_RETURN(-1);
}



int my_win_dup(File fd)
{
  HANDLE hDup;
  DBUG_ENTER("my_win_dup");
  if (DuplicateHandle(GetCurrentProcess(), my_get_osfhandle(fd),
       GetCurrentProcess(), &hDup, 0, FALSE, DUPLICATE_SAME_ACCESS))
  {
     DBUG_RETURN(my_open_osfhandle(hDup, my_get_open_flags(fd)));
  }
  my_osmaperr(GetLastError());
  DBUG_RETURN(-1);
}

#endif /*_WIN32*/
