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
#include "my_sys.h"
#include <my_dir.h> /* for stat */
#include <m_string.h>
#include "mysys_err.h"
#include "my_thread_local.h"

#ifndef _WIN32
#include <utime.h>
#else
#include <sys/utime.h>
#endif

/*
  int my_copy(const char *from, const char *to, myf MyFlags)

  NOTES
    Ordinary ownership and accesstimes are copied from 'from-file'
    If MyFlags & MY_HOLD_ORIGINAL_MODES is set and to-file exists then
    the modes of to-file isn't changed
    If MyFlags & MY_DONT_OVERWRITE_FILE is set, we will give an error
    if the file existed.

  WARNING
    Don't set MY_FNABP or MY_NABP bits on when calling this function !

  RETURN
    0	ok
    #	Error

*/

int my_copy(const char *from, const char *to, myf MyFlags)
{
  size_t Count;
  my_bool new_file_stat= 0; /* 1 if we could stat "to" */
  int create_flag;
  File from_file,to_file;
  uchar buff[IO_SIZE];
  MY_STAT stat_buff,new_stat_buff;
  DBUG_ENTER("my_copy");
  DBUG_PRINT("my",("from %s to %s MyFlags %d", from, to, MyFlags));

  from_file=to_file= -1;
  memset(&new_stat_buff, 0, sizeof(MY_STAT));
  DBUG_ASSERT(!(MyFlags & (MY_FNABP | MY_NABP))); /* for my_read/my_write */
  if (MyFlags & MY_HOLD_ORIGINAL_MODES)		/* Copy stat if possible */
    new_file_stat= MY_TEST(my_stat((char*) to, &new_stat_buff, MYF(0)));

  if ((from_file=my_open(from,O_RDONLY | O_SHARE,MyFlags)) >= 0)
  {
    if (!my_stat(from, &stat_buff, MyFlags))
    {
      set_my_errno(errno);
      goto err;
    }
    if (MyFlags & MY_HOLD_ORIGINAL_MODES && new_file_stat)
      stat_buff=new_stat_buff;
    create_flag= (MyFlags & MY_DONT_OVERWRITE_FILE) ? O_EXCL : O_TRUNC;

    if ((to_file=  my_create(to,(int) stat_buff.st_mode,
			     O_WRONLY | create_flag | O_BINARY | O_SHARE,
			     MyFlags)) < 0)
      goto err;

    while ((Count=my_read(from_file, buff, sizeof(buff), MyFlags)) != 0)
    {
	if (Count == (uint) -1 ||
	    my_write(to_file,buff,Count,MYF(MyFlags | MY_NABP)))
	goto err;
    }

    /* sync the destination file */
    if (MyFlags & MY_SYNC)
    {
      if (my_sync(to_file, MyFlags))
        goto err;
    }

    if (my_close(from_file,MyFlags) | my_close(to_file,MyFlags))
      DBUG_RETURN(-1);				/* Error on close */

    /* Reinitialize closed fd, so they won't be closed again. */
    from_file= -1;
    to_file= -1;

    /* Copy modes if possible */

    if (MyFlags & MY_HOLD_ORIGINAL_MODES && !new_file_stat)
	DBUG_RETURN(0);			/* File copyed but not stat */
    /* Copy modes */
    if (chmod(to, stat_buff.st_mode & 07777))
    {
      set_my_errno(errno);
      if (MyFlags & (MY_FAE+MY_WME))
      {
        char  errbuf[MYSYS_STRERROR_SIZE];
        my_error(EE_CHANGE_PERMISSIONS, MYF(0), from,
                 errno, my_strerror(errbuf, sizeof(errbuf), errno));
      }
      goto err;
    }
#if !defined(_WIN32)
    /* Copy ownership */
    if (chown(to, stat_buff.st_uid, stat_buff.st_gid))
    {
      set_my_errno(errno);
      if (MyFlags & (MY_FAE+MY_WME))
      {
        char  errbuf[MYSYS_STRERROR_SIZE];
        my_error(EE_CHANGE_OWNERSHIP, MYF(0), from,
                 errno, my_strerror(errbuf, sizeof(errbuf), errno));
      }
      goto err;
    }
#endif

    if (MyFlags & MY_COPYTIME)
    {
      struct utimbuf timep;
      timep.actime  = stat_buff.st_atime;
      timep.modtime = stat_buff.st_mtime;
      (void) utime((char*) to, &timep); /* last accessed and modified times */
    }

    DBUG_RETURN(0);
  }

err:
  if (from_file >= 0) (void) my_close(from_file,MyFlags);
  if (to_file >= 0)
  {
    (void) my_close(to_file, MyFlags);
    /* attempt to delete the to-file we've partially written */
    (void) my_delete(to, MyFlags);
  }
  DBUG_RETURN(-1);
} /* my_copy */
