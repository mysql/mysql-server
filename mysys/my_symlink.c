/* Copyright (c) 2001, 2017, Oracle and/or its affiliates. All rights reserved.

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
#include <m_string.h>
#include <errno.h>
#include "my_thread_local.h"
#ifdef HAVE_REALPATH
#include <sys/param.h>
#include <sys/stat.h>
#endif

/*
  Reads the content of a symbolic link
  If the file is not a symbolic link, return the original file name in to.

  RETURN
    0  If filename was a symlink,    (to will be set to value of symlink)
    1  If filename was a normal file (to will be set to filename)
   -1  on error.
*/

int my_readlink(char *to, const char *filename, myf MyFlags)
{
#ifndef HAVE_READLINK
  my_stpcpy(to,filename);
  return 1;
#else
  int result=0;
  int length;
  DBUG_ENTER("my_readlink");

  if ((length=readlink(filename, to, FN_REFLEN-1)) < 0)
  {
    /* Don't give an error if this wasn't a symlink */
    set_my_errno(errno);
    if (my_errno() == EINVAL)
    {
      result= 1;
      my_stpcpy(to,filename);
    }
    else
    {
      if (MyFlags & MY_WME)
      {
        char errbuf[MYSYS_STRERROR_SIZE];
        my_error(EE_CANT_READLINK, MYF(0), filename,
                 errno, my_strerror(errbuf, sizeof(errbuf), errno));
      }
      result= -1;
    }
  }
  else
    to[length]=0;
  DBUG_PRINT("exit" ,("result: %d", result));
  DBUG_RETURN(result);
#endif /* HAVE_READLINK */
}


/* Create a symbolic link */

int my_symlink(const char *content, const char *linkname, myf MyFlags)
{
#ifndef HAVE_READLINK
  return 0;
#else
  int result;
  DBUG_ENTER("my_symlink");
  DBUG_PRINT("enter",("content: %s  linkname: %s", content, linkname));

  result= 0;
  if (symlink(content, linkname))
  {
    result= -1;
    set_my_errno(errno);
    if (MyFlags & MY_WME)
    {
      char errbuf[MYSYS_STRERROR_SIZE];
      my_error(EE_CANT_SYMLINK, MYF(0), linkname, content,
               errno, my_strerror(errbuf, sizeof(errbuf), errno));
    }
  }
  else if ((MyFlags & MY_SYNC_DIR) && my_sync_dir_by_file(linkname, MyFlags))
    result= -1;
  DBUG_RETURN(result);
#endif /* HAVE_READLINK */
}

#if defined(MAXPATHLEN)
#define BUFF_LEN MAXPATHLEN
#else
#define BUFF_LEN FN_LEN
#endif


int my_is_symlink(const char *filename MY_ATTRIBUTE((unused)),
                  ST_FILE_ID *file_id)
{
#if defined (HAVE_LSTAT) && defined (S_ISLNK)
  struct stat stat_buff;
  int result= !lstat(filename, &stat_buff) && S_ISLNK(stat_buff.st_mode);
  if (file_id && !result)
  {
    file_id->st_dev= stat_buff.st_dev;
    file_id->st_ino= stat_buff.st_ino;
  }
  return result;
#elif defined (_WIN32)
  DWORD dwAttr = GetFileAttributes(filename);
  return (dwAttr != INVALID_FILE_ATTRIBUTES) &&
    (dwAttr & FILE_ATTRIBUTE_REPARSE_POINT);
#else  /* No symlinks */
  return 0;
#endif
}

/*
  Resolve all symbolic links in path
  'to' may be equal to 'filename'
*/

int my_realpath(char *to, const char *filename, myf MyFlags)
{
#if defined(HAVE_REALPATH)
  int result=0;
  char buff[BUFF_LEN];
  char *ptr;
  DBUG_ENTER("my_realpath");

  DBUG_PRINT("info",("executing realpath"));
  if ((ptr=realpath(filename,buff)))
      strmake(to,ptr,FN_REFLEN-1);
  else
  {
    /*
      Realpath didn't work;  Use my_load_path() which is a poor substitute
      original name but will at least be able to resolve paths that starts
      with '.'.
    */
    DBUG_PRINT("error",("realpath failed with errno: %d", errno));
    set_my_errno(errno);
    if (MyFlags & MY_WME)
    {
      char errbuf[MYSYS_STRERROR_SIZE];
      my_error(EE_REALPATH, MYF(0), filename,
               my_errno(), my_strerror(errbuf, sizeof(errbuf), my_errno()));
    }
    my_load_path(to, filename, NullS);
    result= -1;
  }
  DBUG_RETURN(result);
#elif defined(_WIN32)
  int ret= GetFullPathName(filename,FN_REFLEN, to, NULL);
  if (ret == 0 || ret > FN_REFLEN)
  {
    set_my_errno((ret > FN_REFLEN) ? ENAMETOOLONG : GetLastError());
    if (MyFlags & MY_WME)
    {
      char errbuf[MYSYS_STRERROR_SIZE];
      my_error(EE_REALPATH, MYF(0), filename,
               my_errno(), my_strerror(errbuf, sizeof(errbuf), my_errno()));
    }
    /* 
      GetFullPathName didn't work : use my_load_path() which is a poor 
      substitute original name but will at least be able to resolve 
      paths that starts with '.'.
    */  
    my_load_path(to, filename, NullS);
    return -1;
  }
#else
  my_load_path(to, filename, NullS);
#endif
  return 0;
}


/**
  Return non-zero if the file descriptor and a previously lstat-ed file
  identified by file_id point to the same file
*/
int my_is_same_file(File file, const ST_FILE_ID *file_id)
{
  MY_STAT stat_buf;
  if (my_fstat(file, &stat_buf, MYF(0)) == -1)
  {
    set_my_errno(errno);
    return 0;
  }
  return (stat_buf.st_dev == file_id->st_dev)
    && (stat_buf.st_ino == file_id->st_ino);
}
