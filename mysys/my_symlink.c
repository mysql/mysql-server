/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA */

#include "mysys_priv.h"
#include "mysys_err.h"
#include <m_string.h>
#ifdef HAVE_REALPATH
#include <sys/param.h>
#include <sys/stat.h>
#endif

/*
  Reads the content of a symbolic link
  If the file is not a symbolic link, return the original file name in to.
*/

int my_readlink(char *to, const char *filename, myf MyFlags)
{
#ifndef HAVE_READLINK
  strmov(to,filename);
  return 0;
#else
  int result=0;
  int length;
  DBUG_ENTER("my_readlink");

  if ((length=readlink(filename, to, FN_REFLEN-1)) < 0)
  {
    /* Don't give an error if this wasn't a symlink */
    if ((my_errno=errno) == EINVAL)
    {
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
  Create a file and a symbolic link that points to this file
  If linkname is a null pointer or equal to filename, we don't
  create a link.
 */


File my_create_with_symlink(const char *linkname, const char *filename,
			    int createflags, int access_flags, myf MyFlags)
{
  File file;
  int tmp_errno;
  DBUG_ENTER("my_create_with_symlink");
  if ((file=my_create(filename, createflags, access_flags, MyFlags)) >= 0)
  {
    /* Test if we should create a link */
    if (linkname && strcmp(linkname,filename))
    {
      /* Delete old link/file */
      if (MyFlags & MY_DELETE_OLD)
	my_delete(linkname, MYF(0));
      /* Create link */
      if (my_symlink(filename, linkname, MyFlags))
      {
	/* Fail, remove everything we have done */
	tmp_errno=my_errno;
	my_close(file,MYF(0));
	my_delete(filename, MYF(0));
	file= -1;
	my_errno=tmp_errno;
      }
    }
  }
  DBUG_RETURN(file);
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

int my_realpath(char *to, const char *filename, myf MyFlags)
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
    if ((ptr=realpath(filename,buff)))
      strmake(to,ptr,FN_REFLEN-1);
    else
    {
      /* Realpath didn't work;  Use original name */
      my_errno=errno;
      if (MyFlags & MY_WME)
	my_error(EE_REALPATH, MYF(0), filename, my_errno);
      if (to != filename)
	strmov(to,filename);
      result= -1;
    }
  }
  return result;
#else
  if (to != filename)
    strmov(to,filename);
  return 0;
#endif
}
