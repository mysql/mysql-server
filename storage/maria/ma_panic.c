/* Copyright (C) 2006 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

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

#include "ma_fulltext.h"

/*
  Stop usage of Maria

  SYNOPSIS
     maria_panic()
     flag	HA_PANIC_CLOSE:  All maria files (tables and log) are closed.
				 maria_end() is called.
                HA_PANIC_WRITE:  All misam files are unlocked and
                                 all changed data in single user maria is
                                 written to file
                HA_PANIC_READ    All maria files that was locked when
			         maria_panic(HA_PANIC_WRITE) was done is
                                 locked. A maria_readinfo() is done for
                                 all single user files to get changes
                                 in database

  RETURN
    0  ok
    #  error number in case of error
*/

int maria_panic(enum ha_panic_function flag)
{
  int error=0;
  LIST *list_element,*next_open;
  MARIA_HA *info;
  DBUG_ENTER("maria_panic");

  if (!maria_inited)
    DBUG_RETURN(0);
  mysql_mutex_lock(&THR_LOCK_maria);
  for (list_element=maria_open_list ; list_element ; list_element=next_open)
  {
    next_open=list_element->next;		/* Save if close */
    info=(MARIA_HA*) list_element->data;
    switch (flag) {
    case HA_PANIC_CLOSE:
      /*
        If bad luck (if some tables would be used now, which normally does not
        happen in MySQL), as we release the mutex, the list may change and so
        we may crash.
      */
      mysql_mutex_unlock(&THR_LOCK_maria);
      if (maria_close(info))
	error=my_errno;
      mysql_mutex_lock(&THR_LOCK_maria);
      break;
    case HA_PANIC_WRITE:		/* Do this to free databases */
#ifdef CANT_OPEN_FILES_TWICE
      if (info->s->options & HA_OPTION_READ_ONLY_DATA)
	break;
#endif
      if (_ma_flush_table_files(info, MARIA_FLUSH_DATA | MARIA_FLUSH_INDEX,
                                FLUSH_RELEASE, FLUSH_RELEASE))
	error=my_errno;
      if (info->opt_flag & WRITE_CACHE_USED)
	if (flush_io_cache(&info->rec_cache))
	  error=my_errno;
      if (info->opt_flag & READ_CACHE_USED)
      {
	if (flush_io_cache(&info->rec_cache))
	  error=my_errno;
	reinit_io_cache(&info->rec_cache,READ_CACHE,0,
		       (pbool) (info->lock_type != F_UNLCK),1);
      }
      if (info->lock_type != F_UNLCK && ! info->was_locked)
      {
	info->was_locked=info->lock_type;
	if (maria_lock_database(info,F_UNLCK))
	  error=my_errno;
      }
#ifdef CANT_OPEN_FILES_TWICE
      if (info->s->kfile.file >= 0 && mysql_file_close(info->s->kfile.file, MYF(0)))
	error = my_errno;
      if (info->dfile.file >= 0 && mysql_file_close(info->dfile.file, MYF(0)))
	error = my_errno;
      info->s->kfile.file= info->dfile.file= -1;/* Files aren't open anymore */
#endif
      break;
    case HA_PANIC_READ:			/* Restore to before WRITE */
#ifdef CANT_OPEN_FILES_TWICE
      {					/* Open closed files */
	char name_buff[FN_REFLEN];
        MARIA_SHARE *share= info->s;
	if (share->kfile.file < 0)
        {

	  if ((share->kfile.file= mysql_file_open(key_file_kfile,
                                       fn_format(name_buff, info->filename, "",
                                                 N_NAME_IEXT,4),
                                       info->mode, MYF(MY_WME))) < 0)
	    error = my_errno;  
        }
	if (info->dfile.file < 0)
	{
	  if ((info->dfile.file= mysql_file_open(key_file_dfile,
                                     fn_format(name_buff, info->filename,
                                               "", N_NAME_DEXT, 4),
                                      info->mode, MYF(MY_WME))) < 0)
	    error = my_errno;
	  info->rec_cache.file= info->dfile.file;
	}
	if (share->bitmap.file.file < 0)
	  share->bitmap.file.file= info->dfile.file;
      }
#endif
      if (info->was_locked)
      {
	if (maria_lock_database(info, info->was_locked))
	  error=my_errno;
	info->was_locked=0;
      }
      break;
    }
  }
  mysql_mutex_unlock(&THR_LOCK_maria);
  if (flag == HA_PANIC_CLOSE)
    maria_end();
  if (!error)
    DBUG_RETURN(0);
  DBUG_RETURN(my_errno=error);
} /* maria_panic */
