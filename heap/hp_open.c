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

/* open a heap-database */

#include "heapdef.h"
#ifdef VMS
#include "hp_static.c"			/* Stupid vms-linker */
#endif

static void init_block(HP_BLOCK *block,uint reclength,ulong min_records,
		       ulong max_records);

	/* open a heap database. */

HP_INFO *heap_open(const char *name, int mode, uint keys, HP_KEYDEF *keydef,
		   uint reclength, ulong max_records, ulong min_records)
{
  uint i,j,key_segs,max_length,length;
  HP_INFO *info;
  HP_SHARE *share;
  HP_KEYSEG *keyseg;
  DBUG_ENTER("heap_open");

  pthread_mutex_lock(&THR_LOCK_heap);
  if (!(share=_hp_find_named_heap(name)))
  {
    for (i=key_segs=max_length=0 ; i < keys ; i++)
    {
      key_segs+= keydef[i].keysegs;
      bzero((char*) &keydef[i].block,sizeof(keydef[i].block));
      for (j=length=0 ; j < keydef[i].keysegs; j++)
	length+=keydef[i].seg[j].length;
      keydef[i].length=length;
      if (length > max_length)
	max_length=length;
    }
    if (!(share = (HP_SHARE*) my_malloc((uint) sizeof(HP_SHARE)+
				       keys*sizeof(HP_KEYDEF)+
				       key_segs*sizeof(HP_KEYSEG),
				       MYF(MY_ZEROFILL))))
    {
      pthread_mutex_unlock(&THR_LOCK_heap);
      DBUG_RETURN(0);
    }
    share->keydef=(HP_KEYDEF*) (share+1);
    keyseg=(HP_KEYSEG*) (share->keydef+keys);
    init_block(&share->block,reclength+1,min_records,max_records);
	/* Fix keys */
    memcpy(share->keydef,keydef,(size_t) (sizeof(keydef[0])*keys));
    for (i=0 ; i < keys ; i++)
    {
      share->keydef[i].seg=keyseg;
      memcpy(keyseg,keydef[i].seg,
	     (size_t) (sizeof(keyseg[0])*keydef[i].keysegs));
      keyseg+=keydef[i].keysegs;
      init_block(&share->keydef[i].block,sizeof(HASH_INFO),min_records,
		 max_records);
    }

    share->min_records=min_records;
    share->max_records=max_records;
    share->data_length=share->index_length=0;
    share->reclength=reclength;
    share->blength=1;
    share->keys=keys;
    share->max_key_length=max_length;
    share->changed=0;
    if (!(share->name=my_strdup(name,MYF(0))))
    {
      my_free((gptr) share,MYF(0));
      pthread_mutex_unlock(&THR_LOCK_heap);
      DBUG_RETURN(0);
    }
#ifdef THREAD
    thr_lock_init(&share->lock);
    VOID(pthread_mutex_init(&share->intern_lock,NULL));
#endif
    share->open_list.data=(void*) share;
    heap_share_list=list_add(heap_share_list,&share->open_list);
  }
  if (!(info= (HP_INFO*) my_malloc((uint) sizeof(HP_INFO)+
				  share->max_key_length,
				  MYF(MY_ZEROFILL))))
  {
    pthread_mutex_unlock(&THR_LOCK_heap);
    DBUG_RETURN(0);
  }
  share->open_count++;
#ifdef THREAD
  thr_lock_data_init(&share->lock,&info->lock,NULL);
#endif
  info->open_list.data=(void*) info;
  heap_open_list=list_add(heap_open_list,&info->open_list);
  pthread_mutex_unlock(&THR_LOCK_heap);

  info->s=share;
  info->lastkey=(byte*) (info+1);
  info->mode=mode;
  info->current_record= (ulong) ~0L;		/* No current record */
  info->current_ptr=0;
  info->current_hash_ptr=0;
  info->lastinx=  info->errkey= -1;
  info->update=0;
#ifndef DBUG_OFF
  info->opt_flag=READ_CHECK_USED;		/* Check when changing */
#endif
  DBUG_PRINT("exit",("heap: %lx  reclength: %d  records_in_block: %d",
		     info,share->reclength,share->block.records_in_block));
  DBUG_RETURN(info);
} /* heap_open */


	/* map name to a heap-nr. If name isn't found return 0 */

HP_SHARE *_hp_find_named_heap(const char *name)
{
  LIST *pos;
  HP_SHARE *info;
  DBUG_ENTER("heap_find");
  DBUG_PRINT("enter",("name: %s",name));

  for (pos=heap_share_list ; pos ; pos=pos->next)
  {
    info=(HP_SHARE*) pos->data;
    if (!strcmp(name,info->name))
    {
      DBUG_PRINT("exit",("Old heap_database: %lx",info));
      DBUG_RETURN(info);
    }
  }
  DBUG_RETURN((HP_SHARE *)0);
}


static void init_block(HP_BLOCK *block, uint reclength, ulong min_records,
		       ulong max_records)
{
  uint i,recbuffer,records_in_block;

  max_records=max(min_records,max_records);
  if (!max_records)
    max_records=1000;			/* As good as quess as anything */
  recbuffer=(uint) (reclength+sizeof(byte**)-1) & ~(sizeof(byte**)-1);
  records_in_block=max_records/10;
  if (records_in_block < 10 && max_records)
    records_in_block=10;
  if (!records_in_block || records_in_block*recbuffer >
      (my_default_record_cache_size-sizeof(HP_PTRS)*HP_MAX_LEVELS))
    records_in_block=(my_default_record_cache_size-sizeof(HP_PTRS)*
		      HP_MAX_LEVELS)/recbuffer+1;
  block->records_in_block=records_in_block;
  block->recbuffer=recbuffer;
  block->last_allocated= 0L;

  for (i=0 ; i <= HP_MAX_LEVELS ; i++)
    block->level_info[i].records_under_level=
      (!i ? 1 : i == 1 ? records_in_block :
       HP_PTRS_IN_NOD * block->level_info[i-1].records_under_level);
}
