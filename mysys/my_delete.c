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

int my_delete(const char *name, myf MyFlags)
{
  int err;
  DBUG_ENTER("my_delete");
  DBUG_PRINT("my",("name %s MyFlags %d", name, MyFlags));

  if ((err = unlink(name)) == -1)
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

#if defined(__WIN__)
/**
  Delete file which is possibly not closed.

  This function is intended to be used exclusively as a temporal solution
  for Win NT in case when it is needed to delete a not closed file (note
  that the file must be opened everywhere with FILE_SHARE_DELETE mode).
  Deleting not-closed files can not be supported on Win 98|ME (and because
  of that is considered harmful).
  
  The function deletes the file with its preliminary renaming. This is
  because when not-closed share-delete file is deleted it still lives on
  a disk until it will not be closed everwhere. This may conflict with an
  attempt to create a new file with the same name. The deleted file is
  renamed to <name>.<num>.deleted where <name> - the initial name of the
  file, <num> - a hexadecimal number chosen to make the temporal name to
  be unique.

  @param the name of the being deleted file
  @param the flags instructing how to react on an error internally in
         the function

  @note The per-thread @c my_errno holds additional info for a caller to
        decide how critical the error can be.

  @retval
    0	ok
  @retval
    1   error


*/
int nt_share_delete(const char *name, myf MyFlags)
{
  char buf[MAX_PATH + 20];
  ulong cnt;
  DBUG_ENTER("nt_share_delete");
  DBUG_PRINT("my",("name %s MyFlags %d", name, MyFlags));

  for (cnt= GetTickCount(); cnt; cnt--)
  {
    errno= 0;
    sprintf(buf, "%s.%08X.deleted", name, cnt);
    if (MoveFile(name, buf))
      break;

    if ((errno= GetLastError()) == ERROR_ALREADY_EXISTS)
      continue;

    /* This happened during tests with MERGE tables. */
    if (errno == ERROR_ACCESS_DENIED)
      continue;

    DBUG_PRINT("warning", ("Failed to rename %s to %s, errno: %d",
                           name, buf, errno));
    break;
  }

  if (errno == ERROR_FILE_NOT_FOUND)
  {
    my_errno= ENOENT;    // marking, that `name' doesn't exist 
  }
  else if (errno == 0)
  {
    if (DeleteFile(buf))
      DBUG_RETURN(0);
    /*
      The below is more complicated than necessary. For some reason, the
      assignment to my_errno clears the error number, which is retrieved
      by GetLastError() (VC2005EE). Assigning to errno first, allows to
      retrieve the correct value.
    */
    errno= GetLastError();
    if (errno == 0)
      my_errno= ENOENT; // marking, that `buf' doesn't exist
    else
      my_errno= errno;
  }
  else
    my_errno= errno;

  if (MyFlags & (MY_FAE+MY_WME))
    my_error(EE_DELETE, MYF(ME_BELL + ME_WAITTANG + (MyFlags & ME_NOINPUT)),
             name, my_errno);
  DBUG_RETURN(-1);
}
#endif
