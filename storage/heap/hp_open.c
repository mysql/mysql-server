/* Copyright (C) 2000-2004, 2006 MySQL AB

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

/* open a heap-database */

#include "heapdef.h"
#ifdef VMS
#include "hp_static.c"			/* Stupid vms-linker */
#endif

#include "my_sys.h"

/*
  Open heap table based on HP_SHARE structure
  
  NOTE
    This doesn't register the table in the open table list.
*/

HP_INFO *heap_open_from_share(HP_SHARE *share, int mode)
{
  HP_INFO *info;
  DBUG_ENTER("heap_open_from_share");

  if (!(info= (HP_INFO*) my_malloc((uint) sizeof(HP_INFO) +
				  2 * share->max_key_length,
				  MYF(MY_ZEROFILL))))
  {
    DBUG_RETURN(0);
  }
  share->open_count++; 
#ifdef THREAD
  thr_lock_data_init(&share->lock,&info->lock,NULL);
#endif
  info->s= share;
  info->lastkey= (uchar*) (info + 1);
  info->recbuf= (uchar*) (info->lastkey + share->max_key_length);
  info->mode= mode;
  info->current_record= (ulong) ~0L;		/* No current record */
  info->lastinx= info->errkey= -1;
#ifndef DBUG_OFF
  info->opt_flag= READ_CHECK_USED;		/* Check when changing */
#endif
  DBUG_PRINT("exit",("heap: 0x%lx  reclength: %d  records_in_block: %d",
		     (long) info, share->reclength,
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

  pthread_mutex_lock(&THR_LOCK_heap);
  if ((info= heap_open_from_share(share, mode)))
  {
    info->open_list.data= (void*) info;
    heap_open_list= list_add(heap_open_list,&info->open_list);
  }
  pthread_mutex_unlock(&THR_LOCK_heap);
  DBUG_RETURN(info);
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

  pthread_mutex_lock(&THR_LOCK_heap);
  if (!(share= hp_find_named_heap(name)))
  {
    my_errno= ENOENT;
    pthread_mutex_unlock(&THR_LOCK_heap);
    DBUG_RETURN(0);
  }
  if ((info= heap_open_from_share(share, mode)))
  {
    info->open_list.data= (void*) info;
    heap_open_list= list_add(heap_open_list,&info->open_list);
  }
  pthread_mutex_unlock(&THR_LOCK_heap);
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
      DBUG_PRINT("exit", ("Old heap_database: 0x%lx", (long) info));
      DBUG_RETURN(info);
    }
  }
  DBUG_RETURN((HP_SHARE *) 0);
}


