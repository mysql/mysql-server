/* Copyright (c) 2000, 2015, Oracle and/or its affiliates. All rights reserved.

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

/* my_setwd() and my_getwd() works with intern_filenames !! */

#include "mysys_priv.h"
#include "my_sys.h"
#include <m_string.h>
#include "mysys_err.h"
#include "my_thread_local.h"
#if defined(_WIN32)
#include <m_ctype.h>
#include <dos.h>
#include <direct.h>
#endif

/* Gets current working directory in buff.

  SYNPOSIS
    my_getwd()
    buf		Buffer to store result. Can be curr_dir[].
    size	Size of buffer
    MyFlags	Flags

  NOTES
    Directory is allways ended with FN_LIBCHAR

  RESULT
    0  ok
    #  error
*/

int my_getwd(char * buf, size_t size, myf MyFlags)
{
  char * pos;
  DBUG_ENTER("my_getwd");
  DBUG_PRINT("my",("buf: 0x%lx  size: %u  MyFlags %d",
                   (long) buf, (uint) size, MyFlags));

  if (size < 1)
    DBUG_RETURN(-1);

  if (curr_dir[0])				/* Current pos is saved here */
    (void) strmake(buf,&curr_dir[0],size-1);
  else
  {
    if (size < 2)
      DBUG_RETURN(-1);
    if (!getcwd(buf,(uint) (size-2)) && MyFlags & MY_WME)
    {
      char errbuf[MYSYS_STRERROR_SIZE];
      set_my_errno(errno);
      my_error(EE_GETWD, MYF(0),
               errno, my_strerror(errbuf, sizeof(errbuf), errno));
      DBUG_RETURN(-1);
    }
    if (*((pos=strend(buf))-1) != FN_LIBCHAR)  /* End with FN_LIBCHAR */
    {
      pos[0]= FN_LIBCHAR;
      pos[1]=0;
    }
    (void) strmake(&curr_dir[0],buf, (size_t) (FN_REFLEN-1));
  }
  DBUG_RETURN(0);
} /* my_getwd */


/* Set new working directory */

int my_setwd(const char *dir, myf MyFlags)
{
  int res;
  size_t length;
  char *start, *pos;
  DBUG_ENTER("my_setwd");
  DBUG_PRINT("my",("dir: '%s'  MyFlags %d", dir, MyFlags));

  start=(char *) dir;
  if (! dir[0] || (dir[0] == FN_LIBCHAR && dir[1] == 0))
    dir=FN_ROOTDIR;
  if ((res=chdir((char*) dir)) != 0)
  {
    set_my_errno(errno);
    if (MyFlags & MY_WME)
    {
      char errbuf[MYSYS_STRERROR_SIZE];
      my_error(EE_SETWD, MYF(0), start,
               errno, my_strerror(errbuf, sizeof(errbuf), errno));
    }
  }
  else
  {
    if (test_if_hard_path(start))
    {						/* Hard pathname */
      pos= strmake(&curr_dir[0],start,(size_t) FN_REFLEN-1);
      if (pos[-1] != FN_LIBCHAR)
      {
	length=(uint) (pos-(char*) curr_dir);
	curr_dir[length]=FN_LIBCHAR;		/* must end with '/' */
	curr_dir[length+1]='\0';
      }
    }
    else
      curr_dir[0]='\0';				/* Don't save name */
  }
  DBUG_RETURN(res);
} /* my_setwd */



	/* Test if hard pathname */
	/* Returns 1 if dirname is a hard path */

int test_if_hard_path(const char *dir_name)
{
  if (dir_name[0] == FN_HOMELIB && dir_name[1] == FN_LIBCHAR)
    return (home_dir != NullS && test_if_hard_path(home_dir));
  if (dir_name[0] == FN_LIBCHAR)
    return (TRUE);
#ifdef FN_DEVCHAR
  return (strchr(dir_name,FN_DEVCHAR) != 0);
#else
  return FALSE;
#endif
} /* test_if_hard_path */


/*
  Test if a name contains an (absolute or relative) path.

  SYNOPSIS
    has_path()
    name                The name to test.

  RETURN
    TRUE        name contains a path.
    FALSE       name does not contain a path.
*/

my_bool has_path(const char *name)
{
  return MY_TEST(strchr(name, FN_LIBCHAR)) 
#if FN_LIBCHAR != '/'
    || MY_TEST(strchr(name,'/'))
#endif
#ifdef FN_DEVCHAR
    || MY_TEST(strchr(name, FN_DEVCHAR))
#endif
    ;
}
