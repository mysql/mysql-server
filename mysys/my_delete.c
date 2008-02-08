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

#if defined(__WIN__) && defined(__NT__)
/*
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
*/
int nt_share_delete(const char *name, myf MyFlags)
{
  char buf[MAX_PATH + 20];
  ulong cnt;
  DBUG_ENTER("nt_share_delete");
  DBUG_PRINT("my",("name %s MyFlags %d", name, MyFlags));

  for (cnt= GetTickCount(); cnt; cnt--)
  {
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

  if (DeleteFile(buf))
    DBUG_RETURN(0);

  my_errno= GetLastError();
  if (MyFlags & (MY_FAE+MY_WME))
    my_error(EE_DELETE, MYF(ME_BELL + ME_WAITTANG + (MyFlags & ME_NOINPUT)),
	       name, my_errno);

  DBUG_RETURN(-1);
}
#endif
