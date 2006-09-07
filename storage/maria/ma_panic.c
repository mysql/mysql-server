/* Copyright (C) 2006 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

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

  pthread_mutex_lock(&THR_LOCK_maria);
  for (list_element=maria_open_list ; list_element ; list_element=next_open)
  {
    next_open=list_element->next;		/* Save if close */
    info=(MARIA_HA*) list_element->data;
    switch (flag) {
    case HA_PANIC_CLOSE:
      pthread_mutex_unlock(&THR_LOCK_maria);	/* Not exactly right... */
      if (maria_close(info))
	error=my_errno;
      pthread_mutex_lock(&THR_LOCK_maria);
      break;
    case HA_PANIC_WRITE:		/* Do this to free databases */
#ifdef CANT_OPEN_FILES_TWICE
      if (info->s->options & HA_OPTION_READ_ONLY_DATA)
	break;
#endif
      if (flush_key_blocks(info->s->key_cache, info->s->kfile, FLUSH_RELEASE))
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
      if (info->s->kfile >= 0 && my_close(info->s->kfile,MYF(0)))
	error = my_errno;
      if (info->dfile >= 0 && my_close(info->dfile,MYF(0)))
	error = my_errno;
      info->s->kfile=info->dfile= -1;	/* Files aren't open anymore */
      break;
#endif
    case HA_PANIC_READ:			/* Restore to before WRITE */
#ifdef CANT_OPEN_FILES_TWICE
      {					/* Open closed files */
	char name_buff[FN_REFLEN];
	if (info->s->kfile < 0)
	  if ((info->s->kfile= my_open(fn_format(name_buff,info->filename,"",
					      N_NAME_IEXT,4),info->mode,
				    MYF(MY_WME))) < 0)
	    error = my_errno;
	if (info->dfile < 0)
	{
	  if ((info->dfile= my_open(fn_format(name_buff,info->filename,"",
					      N_NAME_DEXT,4),info->mode,
				    MYF(MY_WME))) < 0)
	    error = my_errno;
	  info->rec_cache.file=info->dfile;
	}
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
  pthread_mutex_unlock(&THR_LOCK_maria);
  if (flag == HA_PANIC_CLOSE)
    maria_end();
  if (!error)
    DBUG_RETURN(0);
  DBUG_RETURN(my_errno=error);
} /* maria_panic */
