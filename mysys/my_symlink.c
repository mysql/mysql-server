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
#include "mysys_err.h"
#include <m_string.h>
#include <errno.h>
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
  strmov(to,filename);
  return 1;
#else
  int result=0;
  int length;
  DBUG_ENTER("my_readlink");

  if ((length=readlink(filename, to, FN_REFLEN-1)) < 0)
  {
    /* Don't give an error if this wasn't a symlink */
    if ((my_errno=errno) == EINVAL)
    {
      result= 1;
      strmov(to,filename);
    }
    else
    {
      if (MyFlags & MY_WME)
	my_error(EE_CANT_READLINK, MYF(0), filename, errno);
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
    my_errno=errno;
    if (MyFlags & MY_WME)
      my_error(EE_CANT_SYMLINK, MYF(0), linkname, content, errno);
  }
  DBUG_RETURN(result);
#endif /* HAVE_READLINK */
}

/*
  Resolve all symbolic links in path
  'to' may be equal to 'filename'

  Because purify gives a lot of UMR errors when using realpath(),
  this code is disabled when using purify.

  If MY_RESOLVE_LINK is given, only do realpath if the file is a link.
*/

#if defined(SCO)
#define BUFF_LEN 4097
#elif defined(MAXPATHLEN)
#define BUFF_LEN MAXPATHLEN
#else
#define BUFF_LEN FN_LEN
#endif

int my_realpath(char *to, const char *filename,
		myf MyFlags __attribute__((unused)))
{
#if defined(HAVE_REALPATH) && !defined(HAVE_purify) && !defined(HAVE_BROKEN_REALPATH)
  int result=0;
  char buff[BUFF_LEN];
  struct stat stat_buff;
  DBUG_ENTER("my_realpath");

  if (!(MyFlags & MY_RESOLVE_LINK) ||
      (!lstat(filename,&stat_buff) && S_ISLNK(stat_buff.st_mode)))
  {
    char *ptr;
    DBUG_PRINT("info",("executing realpath"));
    if ((ptr=realpath(filename,buff)))
    {
      strmake(to,ptr,FN_REFLEN-1);
    }
    else
    {
      /*
	Realpath didn't work;  Use my_load_path() which is a poor substitute
	original name but will at least be able to resolve paths that starts
	with '.'.
      */
      DBUG_PRINT("error",("realpath failed with errno: %d", errno));
      my_errno=errno;
      if (MyFlags & MY_WME)
	my_error(EE_REALPATH, MYF(0), filename, my_errno);
      my_load_path(to, filename, NullS);
      result= -1;
    }
  }
  DBUG_RETURN(result);
#else
  my_load_path(to, filename, NullS);
  return 0;
#endif
}
