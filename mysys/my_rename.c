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
#include <my_dir.h>
#include "mysys_err.h"
#include "m_string.h"
#undef my_rename

	/* On unix rename deletes to file if it exists */

int my_rename(const char *from, const char *to, myf MyFlags)
{
  int error = 0;
  DBUG_ENTER("my_rename");
  DBUG_PRINT("my",("from %s to %s MyFlags %d", from, to, MyFlags));

#if defined(HAVE_FILE_VERSIONS)
  {				/* Check that there isn't a old file */
    int save_errno;
    MY_STAT my_stat_result;
    save_errno=my_errno;
    if (my_stat(to,&my_stat_result,MYF(0)))
    {
      my_errno=EEXIST;
      error= -1;
      if (MyFlags & MY_FAE+MY_WME)
	my_error(EE_LINK, MYF(ME_BELL+ME_WAITTANG),from,to,my_errno);
      DBUG_RETURN(error);
    }
    my_errno=save_errno;
  }
#endif
#if defined(HAVE_RENAME)
#if defined(__WIN__) || defined(__NETWARE__)
  /*
    On windows we can't rename over an existing file:
    Remove any conflicting files:
  */
  (void) my_delete(to, MYF(0));
#endif
  if (rename(from,to))
#else
  if (link(from, to) || unlink(from))
#endif
  {
    my_errno=errno;
    error = -1;
    if (MyFlags & (MY_FAE+MY_WME))
      my_error(EE_LINK, MYF(ME_BELL+ME_WAITTANG),from,to,my_errno);
  }
  else if (MyFlags & MY_SYNC_DIR)
  {
#ifdef NEED_EXPLICIT_SYNC_DIR
    /* do only the needed amount of syncs: */
    char dir_from[FN_REFLEN], dir_to[FN_REFLEN];
    size_t dir_from_length, dir_to_length;
    dirname_part(dir_from, from, &dir_from_length);
    dirname_part(dir_to, to, &dir_to_length);
    if (my_sync_dir(dir_from, MyFlags) ||
        (strcmp(dir_from, dir_to) &&
         my_sync_dir(dir_to, MyFlags)))
      error= -1;
#endif
  }
  DBUG_RETURN(error);
} /* my_rename */
