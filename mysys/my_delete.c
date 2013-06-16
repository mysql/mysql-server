/* Copyright (c) 2000, 2010, Oracle and/or its affiliates. All rights reserved.

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
#include "mysys_err.h"
#include <my_sys.h>

#ifdef _WIN32
static int my_win_unlink(const char *name);
#endif

int my_delete(const char *name, myf MyFlags)
{
  int err;
  DBUG_ENTER("my_delete");
  DBUG_PRINT("my",("name %s MyFlags %d", name, MyFlags));

#ifdef _WIN32
  err = my_win_unlink(name);
#else
  err = unlink(name);
#endif

  if(err)
  {
    my_errno=errno;
    if (MyFlags & (MY_FAE+MY_WME))
      my_error(EE_DELETE,MYF(ME_BELL+ME_WAITTANG+(MyFlags & ME_NOINPUT)),
	       name,errno);
  }
  else if ((MyFlags & MY_SYNC_DIR) &&
           my_sync_dir_by_file(name, MyFlags))
    err= -1;
  DBUG_RETURN(err);
} /* my_delete */


#if defined (_WIN32)
/* 
  Delete file.

  The function also makes best effort to minimize number of errors, 
  where another program (or thread in the current program) has the the same file
  open.

  We're using 2 tricks to prevent the errors.

  1. A usual Win32's DeleteFile() can with ERROR_SHARED_VIOLATION,
  because the file is opened in another application (often, antivirus or backup)
  
  We avoid the error by using CreateFile() with FILE_FLAG_DELETE_ON_CLOSE, instead
  of DeleteFile()

  2. If file which is deleted (delete on close) but has not entirely gone,
  because it is still opened by some app, an attempt to trcreate file with the 
  same name would  result in yet another error. The workaround here is renaming 
  a file to unique name.

  Symbolic link are deleted without renaming. Directories are not deleted.
 */
static int my_win_unlink(const char *name)
{
  HANDLE handle= INVALID_HANDLE_VALUE;
  DWORD attributes;
  DWORD last_error;
  char unique_filename[MAX_PATH + 35];
  unsigned long long tsc; /* time stamp counter, for unique filename*/

  DBUG_ENTER("my_win_unlink");
  attributes= GetFileAttributes(name);
  if (attributes == INVALID_FILE_ATTRIBUTES)
  {
    last_error= GetLastError();
    DBUG_PRINT("error",("GetFileAttributes(%s) failed with %u\n", name, last_error));
    goto error;
  }

  if (attributes & FILE_ATTRIBUTE_DIRECTORY)
  {
    DBUG_PRINT("error",("can't remove %s - it is a directory\n", name));
    errno= EINVAL;
    DBUG_RETURN(-1);
  }
 
  if (attributes & FILE_ATTRIBUTE_REPARSE_POINT)
  {
    /* Symbolic link. Delete link, the not target */
    if (!DeleteFile(name))
    {
       last_error= GetLastError();
       DBUG_PRINT("error",("DeleteFile(%s) failed with %u\n", name,last_error));
       goto error;
    }
    DBUG_RETURN(0);
  }

  handle= CreateFile(name, DELETE, 0,  NULL, OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE, NULL);
  if (handle != INVALID_HANDLE_VALUE)
  {
    /*
      We opened file without sharing flags (exclusive), noone else has this file
      opened, thus it is save to close handle to remove it. No renaming is 
      necessary.
    */
    CloseHandle(handle);
    DBUG_RETURN(0);
  }

  /*
     Can't open file exclusively, hence the file must be already opened by 
     someone else. Open it for delete (with all FILE_SHARE flags set), 
     rename to unique name, close.
  */
  handle= CreateFile(name, DELETE, FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
    NULL, OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE, NULL);
  if (handle == INVALID_HANDLE_VALUE)
  {
     last_error= GetLastError();
     DBUG_PRINT("error",
       ("CreateFile(%s) with FILE_FLAG_DELETE_ON_CLOSE failed with %u\n",
        name,last_error));
     goto error;
  }

  tsc= __rdtsc();
  my_snprintf(unique_filename,sizeof(unique_filename),"%s.%llx.deleted", 
    name, tsc);
  if (!MoveFile(name, unique_filename)) 
  {
    DBUG_PRINT("warning",  ("moving %s to unique filename failed, error %u\n",
    name,GetLastError()));
  }

  CloseHandle(handle);
  DBUG_RETURN(0);
 
error:
  my_osmaperr(last_error);
  DBUG_RETURN(-1);
}
#endif