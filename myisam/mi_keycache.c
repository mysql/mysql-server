/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

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

/*
  Key cache assignments
*/

#include "myisamdef.h"


/*
  Assign pages of the index file for a table to a key cache

  SYNOPSIS
    mi_assign_to_keycache()
      info          open table
      map           map of indexes to assign to the key cache 
      key_cache_ptr pointer to the key cache handle

  RETURN VALUE
    0 if a success. error code - otherwise.

  NOTES.
    At present pages for all indexes must be assigned to the same key cache.
    In future only pages for indexes specified in the key_map parameter
    of the table will be assigned to the specified key cache.
*/

typedef struct st_assign_extra_info
{
  pthread_mutex_t *lock; 
  struct st_my_thread_var *waiting_thread;
} ASSIGN_EXTRA_INFO;

static void remove_key_cache_assign(void *arg)
{
  KEY_CACHE_VAR *key_cache= (KEY_CACHE_VAR *) arg;
  ASSIGN_EXTRA_INFO *extra_info= (ASSIGN_EXTRA_INFO *) key_cache->extra_info;
  struct st_my_thread_var *waiting_thread;
  pthread_mutex_t *lock= extra_info->lock;
  pthread_mutex_lock(lock);
  if (!(--key_cache->assignments) && 
      (waiting_thread = extra_info->waiting_thread))
  {
    my_free(extra_info, MYF(0));
    key_cache->extra_info= 0;
    if (waiting_thread != my_thread_var)
      pthread_cond_signal(&waiting_thread->suspend);
  }
  pthread_mutex_unlock(lock);
}

int mi_assign_to_keycache(MI_INFO *info, ulonglong key_map, 
                          KEY_CACHE_VAR *key_cache, 
                          pthread_mutex_t *assign_lock)
{
  ASSIGN_EXTRA_INFO *extra_info;
  int error= 0;
  MYISAM_SHARE* share= info->s;

  DBUG_ENTER("mi_assign_to_keycache");

  share->reg_keycache= &key_cache->cache;
  pthread_mutex_lock(assign_lock);
  if (!(extra_info= (ASSIGN_EXTRA_INFO *) key_cache->extra_info))
  {
    if (!(extra_info= (ASSIGN_EXTRA_INFO*) my_malloc(sizeof(ASSIGN_EXTRA_INFO),
					           MYF(MY_WME | MY_ZEROFILL))))
      DBUG_RETURN(HA_ERR_OUT_OF_MEM);
    key_cache->extra_info= extra_info;
    key_cache->action= remove_key_cache_assign;
    extra_info->lock= assign_lock;
  }
  key_cache->assignments++;
  pthread_mutex_unlock(assign_lock);
  
  if (!(info->lock_type == F_WRLCK && share->w_locks))
  {
    if (flush_key_blocks(*share->keycache, share->kfile, FLUSH_REMOVE))
    {
      error=my_errno;
      mi_mark_crashed(info);		/* Mark that table must be checked */
    }
    share->keycache= &key_cache->cache;
  }
  else
  {
    extra_info->waiting_thread= my_thread_var;
  }

    
  DBUG_RETURN(error);
}

