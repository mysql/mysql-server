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

#include "mysys_priv.h"
#include <m_string.h>
#include "my_static.h"
#include "mysys_err.h"
#include <errno.h>
#include "my_thread_local.h"


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
		      int mode, myf MyFlags)
{
  File file= -1;
#ifdef _WIN32
  TCHAR path_buf[MAX_PATH-14];
#endif

  DBUG_ENTER("create_temp_file");
  DBUG_PRINT("enter", ("dir: %s, prefix: %s", dir, prefix));
#if defined(_WIN32)

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
    int tmp= my_errno();
    (void) my_delete(to, MYF(0));
    set_my_errno(tmp);
  }

#else /* mkstemp() is available on all non-Windows supported platforms. */
  {
    char prefix_buff[30];
    uint pfx_len;
    File org_file;

    pfx_len= (uint) (my_stpcpy(my_stpnmov(prefix_buff,
				    prefix ? prefix : "tmp.",
				    sizeof(prefix_buff)-7),"XXXXXX") -
		     prefix_buff);
    if (!dir && ! (dir =getenv("TMPDIR")))
      dir= DEFAULT_TMPDIR;
    if (strlen(dir)+ pfx_len > FN_REFLEN-2)
    {
      errno=ENAMETOOLONG;
      set_my_errno(ENAMETOOLONG);
      DBUG_RETURN(file);
    }
    my_stpcpy(convert_dirname(to,dir,NullS),prefix_buff);
    org_file=mkstemp(to);
    if (mode & O_TEMPORARY)
      (void) my_delete(to, MYF(MY_WME));
    file=my_register_filename(org_file, to, FILE_BY_MKSTEMP,
			      EE_CANTCREATEFILE, MyFlags);
    /* If we didn't manage to register the name, remove the temp file */
    if (org_file >= 0 && file < 0)
    {
      int tmp=my_errno();
      close(org_file);
      (void) my_delete(to, MYF(MY_WME));
      set_my_errno(tmp);
    }
  }
#endif
  if (file >= 0)
  {
    mysql_mutex_lock(&THR_LOCK_open);
    my_tmp_file_created++;
    mysql_mutex_unlock(&THR_LOCK_open);
  }
  DBUG_RETURN(file);
}
