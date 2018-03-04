/* Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/* open a heap-database */

#include <errno.h>
#include <sys/types.h>

#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "mysql/service_mysql_alloc.h"
#include "storage/heap/heapdef.h"

/*
  Open heap table based on HP_SHARE structure
  
  NOTE
    This doesn't register the table in the open table list.
*/

HP_INFO *heap_open_from_share(HP_SHARE *share, int mode)
{
  HP_INFO *info;
  DBUG_ENTER("heap_open_from_share");

  if (!(info= (HP_INFO*) my_malloc(hp_key_memory_HP_INFO,
                                   (uint) sizeof(HP_INFO) +
				  2 * share->max_key_length,
				  MYF(MY_ZEROFILL))))
  {
    DBUG_RETURN(0);
  }
  share->open_count++; 
  /*
    Don't initialize THR_LOCK_DATA for internal temporary tables as it
    is not used for them anyway (and THR_LOCK is not initialized for them
    too).
  */
  if (share->open_list.data != NULL)
    thr_lock_data_init(&share->lock, &info->lock, NULL);
  info->s= share;
  info->lastkey= (uchar*) (info + 1);
  info->recbuf= (uchar*) (info->lastkey + share->max_key_length);
  info->mode= mode;
  info->current_record= (ulong) ~0L;		/* No current record */
  info->lastinx= info->errkey= -1;
#ifndef DBUG_OFF
  info->opt_flag= READ_CHECK_USED;		/* Check when changing */
#endif
  DBUG_PRINT("exit",("heap: %p  reclength: %d  records_in_block: %d",
		     info, share->reclength,
                     share->block.records_in_block));
  DBUG_RETURN(info);
}


/*
  Open heap table based on HP_SHARE structure and register it
*/

HP_INFO *heap_open_from_share_and_register(HP_SHARE *share, int mode)
{
  HP_INFO *info;
  DBUG_ENTER("heap_open_from_share_and_register");

  mysql_mutex_lock(&THR_LOCK_heap);
  if ((info= heap_open_from_share(share, mode)))
  {
    info->open_list.data= (void*) info;
    heap_open_list= list_add(heap_open_list,&info->open_list);
    /* Unpin the share, it is now pinned by the file. */
    share->open_count--;
  }
  mysql_mutex_unlock(&THR_LOCK_heap);
  DBUG_RETURN(info);
}


/**
  Dereference a HEAP share and free it if it's not referenced.
  We needn't check open_count for single instances.
*/
void heap_release_share(HP_SHARE *share, bool single_instance)
{
  /* Couldn't open table; Remove the newly created table */
  if (single_instance)
    hp_free(share);
  else
  {
    mysql_mutex_lock(&THR_LOCK_heap);
    if (--share->open_count == 0)
      hp_free(share);
    mysql_mutex_unlock(&THR_LOCK_heap);
  }
}

/*
  Open heap table based on name

  NOTE
    This register the table in the open table list. so that it can be
    found by future heap_open() calls.
*/

HP_INFO *heap_open(const char *name, int mode)
{
  HP_INFO *info;
  HP_SHARE *share;
  DBUG_ENTER("heap_open");

  mysql_mutex_lock(&THR_LOCK_heap);
  if (!(share= hp_find_named_heap(name)))
  {
    set_my_errno(ENOENT);
    mysql_mutex_unlock(&THR_LOCK_heap);
    DBUG_RETURN(0);
  }
  if ((info= heap_open_from_share(share, mode)))
  {
    info->open_list.data= (void*) info;
    heap_open_list= list_add(heap_open_list,&info->open_list);
  }
  mysql_mutex_unlock(&THR_LOCK_heap);
  DBUG_RETURN(info);
}


/* map name to a heap-nr. If name isn't found return 0 */

HP_SHARE *hp_find_named_heap(const char *name)
{
  LIST *pos;
  HP_SHARE *info;
  DBUG_ENTER("heap_find");
  DBUG_PRINT("enter",("name: %s",name));

  for (pos= heap_share_list; pos; pos= pos->next)
  {
    info= (HP_SHARE*) pos->data;
    if (!strcmp(name, info->name))
    {
      DBUG_PRINT("exit", ("Old heap_database: %p", info));
      DBUG_RETURN(info);
    }
  }
  DBUG_RETURN((HP_SHARE *) 0);
}


