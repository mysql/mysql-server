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
#include <my_dir.h>
#include <m_string.h>
#include "mysys_err.h"
#if defined(HAVE_UTIME_H)
#include <utime.h>
#elif defined(HAVE_SYS_UTIME_H)
#include <sys/utime.h>
#elif !defined(HPUX10)
struct utimbuf {
  time_t actime;
  time_t modtime;
};
#endif

	/*
	  Rename with copy stat form old file
	  Copy stats from old file to new file, deletes orginal and
	  changes new file name to old file name

	  if MY_REDEL_MAKE_COPY is given, then the orginal file
	  is renamed to org_name-'current_time'.BAK
	*/

#define REDEL_EXT ".BAK"

int my_redel(const char *org_name, const char *tmp_name, myf MyFlags)
{
  int error=1;
  DBUG_ENTER("my_redel");
  DBUG_PRINT("my",("org_name: '%s' tmp_name: '%s'  MyFlags: %d",
		   org_name,tmp_name,MyFlags));

  if (my_copystat(org_name,tmp_name,MyFlags) < 0)
    goto end;
  if (MyFlags & MY_REDEL_MAKE_BACKUP)
  {
    char name_buff[FN_REFLEN+20];    
    char ext[20];
    ext[0]='-';
    get_date(ext+1,2+4,(time_t) 0);
    strmov(strend(ext),REDEL_EXT);
    if (my_rename(org_name, fn_format(name_buff, org_name, "", ext, 2),
		  MyFlags))
      goto end;
  }
  else if (my_delete_allow_opened(org_name, MyFlags))
      goto end;
  if (my_rename(tmp_name,org_name,MyFlags))
    goto end;

  error=0;
end:
  DBUG_RETURN(error);
} /* my_redel */


	/* Copy stat from one file to another */
	/* Return -1 if can't get stat, 1 if wrong type of file */

int my_copystat(const char *from, const char *to, int MyFlags)
{
  struct stat statbuf;

  if (stat(from, &statbuf))
  {
    my_errno=errno;
    if (MyFlags & (MY_FAE+MY_WME))
      my_error(EE_STAT, MYF(ME_BELL+ME_WAITTANG),from,errno);
    return -1;				/* Can't get stat on input file */
  }
  if ((statbuf.st_mode & S_IFMT) != S_IFREG)
    return 1;

  /* Copy modes */
  if (chmod(to, statbuf.st_mode & 07777))
  {
    my_errno= errno;
    if (MyFlags & (MY_FAE+MY_WME))
      my_error(EE_CHANGE_PERMISSIONS, MYF(ME_BELL+ME_WAITTANG), from, errno);
    return -1;
  }

#if !defined(__WIN__)
  if (statbuf.st_nlink > 1 && MyFlags & MY_LINK_WARNING)
  {
    if (MyFlags & MY_LINK_WARNING)
      my_error(EE_LINK_WARNING,MYF(ME_BELL+ME_WAITTANG),from,statbuf.st_nlink);
  }
  /* Copy ownership */
  if (chown(to, statbuf.st_uid, statbuf.st_gid))
  {
    my_errno= errno;
    if (MyFlags & (MY_FAE+MY_WME))
      my_error(EE_CHANGE_OWNERSHIP, MYF(ME_BELL+ME_WAITTANG), from, errno);
    return -1;
  }
#endif /* !__WIN__ */

  if (MyFlags & MY_COPYTIME)
  {
    struct utimbuf timep;
    timep.actime  = statbuf.st_atime;
    timep.modtime = statbuf.st_mtime;
    (void) utime((char*) to, &timep);/* Update last accessed and modified times */
  }

  return 0;
} /* my_copystat */
