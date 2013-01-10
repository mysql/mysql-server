/*
   Copyright (c) 2000, 2010, Oracle and/or its affiliates

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "mysys_priv.h"
#include "mysys_err.h"
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

int my_redel(const char *org_name, const char *tmp_name,
             time_t backup_time_stamp, myf MyFlags)
{
  int error=1;
  DBUG_ENTER("my_redel");
  DBUG_PRINT("my",("org_name: '%s' tmp_name: '%s'  MyFlags: %d",
		   org_name,tmp_name,MyFlags));

  if (my_copystat(org_name,tmp_name,MyFlags) < 0)
    goto end;
  if (MyFlags & MY_REDEL_MAKE_BACKUP)
  {
    char name_buff[FN_REFLEN + MY_BACKUP_NAME_EXTRA_LENGTH];    
    my_create_backup_name(name_buff, org_name, backup_time_stamp);
    if (my_rename(org_name, name_buff, MyFlags))
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


/**
   Copy stat from one file to another
   @fn     my_copystat()
   @param  from		Copy stat from this file
   @param  to           Copy stat to this file
   @param  MyFlags      Flags:
		        MY_WME    Give error if something goes wrong
		        MY_FAE    Abort operation if something goes wrong
                        If MY_FAE is not given, we don't return -1 for
                        errors from chown (which normally require root
                        privilege)

  @return  0 ok
          -1 if can't get stat,
           1 if wrong type of file
*/

int my_copystat(const char *from, const char *to, int MyFlags)
{
  MY_STAT statbuf;

  if (my_stat(from, &statbuf, MyFlags) == NULL)
    return -1;				/* Can't get stat on input file */

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

#if !defined(__WIN__) && !defined(__NETWARE__)
  if (statbuf.st_nlink > 1 && MyFlags & MY_LINK_WARNING)
  {
    if (MyFlags & MY_LINK_WARNING)
      my_error(EE_LINK_WARNING,MYF(ME_BELL+ME_WAITTANG),from,statbuf.st_nlink);
  }
  /* Copy ownership */
  if (chown(to, statbuf.st_uid, statbuf.st_gid))
  {
    my_errno= errno;
    if (MyFlags & MY_WME)
      my_error(EE_CHANGE_OWNERSHIP, MYF(ME_BELL+ME_WAITTANG), from, errno);
    if (MyFlags & MY_FAE)
      return -1;
  }
#endif /* !__WIN__ && !__NETWARE__ */

#ifndef VMS
#ifndef __ZTC__
  if (MyFlags & MY_COPYTIME)
  {
    struct utimbuf timep;
    timep.actime  = statbuf.st_atime;
    timep.modtime = statbuf.st_mtime;
    VOID(utime((char*) to, &timep));/* Update last accessed and modified times */
  }
#else
  if (MyFlags & MY_COPYTIME)
  {
    time_t time[2];
    time[0]= statbuf.st_atime;
    time[1]= statbuf.st_mtime;
    VOID(utime((char*) to, time));/* Update last accessed and modified times */
  }
#endif
#endif
  return 0;
} /* my_copystat */


/**
   Create a backup file name.
   @fn my_create_backup_name()
   @param to	Store new file name here
   @param from  Original name

   @info
   The backup name is made by adding -YYMMDDHHMMSS.BAK to the file name
*/

void my_create_backup_name(char *to, const char *from, time_t backup_start)
{
  char ext[MY_BACKUP_NAME_EXTRA_LENGTH+1];
  ext[0]='-';
  get_date(ext+1, GETDATE_SHORT_DATE | GETDATE_HHMMSSTIME, backup_start);
  strmov(strend(ext),REDEL_EXT);
  strmov(strmov(to, from), ext);
}
