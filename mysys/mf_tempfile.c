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
#include <m_string.h>
#include "my_static.h"
#include "mysys_err.h"
#include <errno.h>
#ifdef HAVE_PATHS_H
#include <paths.h>
#endif



/*
  @brief
  Create a temporary file with unique name in a given directory

  @details
  create_temp_file
    to             pointer to buffer where temporary filename will be stored
    dir            directory where to create the file
    prefix         prefix the filename with this
    mode           Flags to use for my_create/my_open
    MyFlags        Magic flags

  @return
    File descriptor of opened file if success
    -1 and sets errno if fails.

  @note
    The behaviour of this function differs a lot between
    implementation, it's main use is to generate a file with
    a name that does not already exist.

    When passing O_TEMPORARY flag in "mode" the file should
    be automatically deleted

    The implementation using mkstemp should be considered the
    reference implementation when adding a new or modifying an
    existing one

*/

File create_temp_file(char *to, const char *dir, const char *prefix,
		      int mode __attribute__((unused)),
		      myf MyFlags __attribute__((unused)))
{
  File file= -1;
#ifdef __WIN__
  TCHAR path_buf[MAX_PATH-14];
#endif

  DBUG_ENTER("create_temp_file");
  DBUG_PRINT("enter", ("dir: %s, prefix: %s", dir, prefix));
#if defined (__WIN__)

   /*
     Use GetTempPath to determine path for temporary files.
     This is because the documentation for GetTempFileName 
     has the following to say about this parameter:
     "If this parameter is NULL, the function fails."
   */
   if (!dir)
   {
     if(GetTempPath(sizeof(path_buf), path_buf) > 0) 
       dir = path_buf;
   }
   /*
     Use GetTempFileName to generate a unique filename, create
     the file and release it's handle
      - uses up to the first three letters from prefix
   */
  if (GetTempFileName(dir, prefix, 0, to) == 0)
    DBUG_RETURN(-1);

  DBUG_PRINT("info", ("name: %s", to));

  /*
    Open the file without the "open only if file doesn't already exist"
    since the file has already been created by GetTempFileName
  */
  if ((file= my_open(to,  (mode & ~O_EXCL), MyFlags)) < 0)
  {
    /* Open failed, remove the file created by GetTempFileName */
    int tmp= my_errno;
    (void) my_delete(to, MYF(0));
    my_errno= tmp;
  }

#elif defined(_ZTC__)
  if (!dir)
    dir=getenv("TMPDIR");
  if ((res=tempnam((char*) dir,(char *) prefix)))
  {
    strmake(to,res,FN_REFLEN-1);
    (*free)(res);
    file=my_create(to, 0, mode | O_EXCL | O_NOFOLLOW, MyFlags);
  }
#elif defined(HAVE_MKSTEMP) && !defined(__NETWARE__)
  {
    char prefix_buff[30];
    uint pfx_len;
    File org_file;

    pfx_len= (uint) (strmov(strnmov(prefix_buff,
				    prefix ? prefix : "tmp.",
				    sizeof(prefix_buff)-7),"XXXXXX") -
		     prefix_buff);
    if (!dir && ! (dir =getenv("TMPDIR")))
      dir=P_tmpdir;
    if (strlen(dir)+ pfx_len > FN_REFLEN-2)
    {
      errno=my_errno= ENAMETOOLONG;
      DBUG_RETURN(file);
    }
    strmov(convert_dirname(to,dir,NullS),prefix_buff);
    org_file=mkstemp(to);
    if (mode & O_TEMPORARY)
      (void) my_delete(to, MYF(MY_WME | ME_NOINPUT));
    file=my_register_filename(org_file, to, FILE_BY_MKSTEMP,
			      EE_CANTCREATEFILE, MyFlags);
    /* If we didn't manage to register the name, remove the temp file */
    if (org_file >= 0 && file < 0)
    {
      int tmp=my_errno;
      close(org_file);
      (void) my_delete(to, MYF(MY_WME | ME_NOINPUT));
      my_errno=tmp;
    }
  }
#elif defined(HAVE_TEMPNAM)
  {
#if !defined(__NETWARE__)
    extern char **environ;
#endif

    char *res,**old_env,*temp_env[1];
    if (dir && !dir[0])
    {				/* Change empty string to current dir */
      to[0]= FN_CURLIB;
      to[1]= 0;
      dir=to;
    }
#if !defined(__NETWARE__)
    old_env= (char**) environ;
    if (dir)
    {				/* Don't use TMPDIR if dir is given */
      environ=(const char**) temp_env;
      temp_env[0]=0;
    }
#endif
    if ((res=tempnam((char*) dir, (char*) prefix)))
    {
      strmake(to,res,FN_REFLEN-1);
      (*free)(res);
      file=my_create(to,0,
		     (int) (O_RDWR | O_BINARY | O_TRUNC | O_EXCL | O_NOFOLLOW |
			    O_TEMPORARY | O_SHORT_LIVED),
		     MYF(MY_WME));

    }
    else
    {
      DBUG_PRINT("error",("Got error: %d from tempnam",errno));
    }
#if !defined(__NETWARE__)
    environ=(const char**) old_env;
#endif
  }
#else
#error No implementation found for create_temp_file
#endif
  if (file >= 0)
    thread_safe_increment(my_tmp_file_created,&THR_LOCK_open);
  DBUG_RETURN(file);
}
