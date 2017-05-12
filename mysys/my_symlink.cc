/* Copyright (c) 2001, 2016, Oracle and/or its affiliates. All rights reserved.

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

/**
  @file mysys/my_symlink.cc
*/

#include "my_config.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "m_string.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_io.h"
#include "my_sys.h"
#include "my_thread_local.h"
#include "mysys_err.h"
#ifndef _WIN32
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
#ifdef _WIN32
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
#endif /* !_WIN32 */
}


/* Create a symbolic link */

#ifndef _WIN32
int my_symlink(const char *content, const char *linkname, myf MyFlags)
{
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
}
#endif /* !_WIN32 */


int my_is_symlink(const char *filename)
{
#ifndef _WIN32
  struct stat stat_buff;
  return !lstat(filename, &stat_buff) && S_ISLNK(stat_buff.st_mode);
#else
  DWORD dwAttr = GetFileAttributes(filename);
  return (dwAttr != INVALID_FILE_ATTRIBUTES) &&
    (dwAttr & FILE_ATTRIBUTE_REPARSE_POINT);
#endif
}


/*
  Resolve all symbolic links in path
  'to' may be equal to 'filename'
*/

int my_realpath(char *to, const char *filename, myf MyFlags)
{
#ifndef _WIN32
  int result=0;
  char buff[PATH_MAX];
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
#else
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
  return 0;
#endif
}
