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

#include "heapdef.h"

static int keys_compare(heap_rb_param *param, uchar *key1, uchar *key2);
static void init_block(HP_BLOCK *block,uint reclength,ulong min_records,
		       ulong max_records);

int heap_create(const char *name, uint keys, HP_KEYDEF *keydef,
		uint reclength, ulong max_records, ulong min_records,
		HP_CREATE_INFO *create_info)
{
  uint i, j, key_segs, max_length, length;
  HP_SHARE *share;
  HA_KEYSEG *keyseg;
  
  DBUG_ENTER("heap_create");
  pthread_mutex_lock(&THR_LOCK_heap);

  if ((share= hp_find_named_heap(name)) && share->open_count == 0)
  {
    hp_free(share);
    share= NULL;
  }
  
  if (!share)
  {
    HP_KEYDEF *keyinfo;
    DBUG_PRINT("info",("Initializing new table"));
    
    /*
      We have to store sometimes byte* del_link in records,
      so the record length should be at least sizeof(byte*)
    */
    set_if_bigger(reclength, sizeof (byte*));
    
    for (i= key_segs= max_length= 0, keyinfo= keydef; i < keys; i++, keyinfo++)
    {
      bzero((char*) &keyinfo->block,sizeof(keyinfo->block));
      bzero((char*) &keyinfo->rb_tree ,sizeof(keyinfo->rb_tree));
      for (j= length= 0; j < keyinfo->keysegs; j++)
      {
	length+= keyinfo->seg[j].length;
	if (keyinfo->seg[j].null_bit)
	{
	  length++;
	  if (!(keyinfo->flag & HA_NULL_ARE_EQUAL))
	    keyinfo->flag|= HA_NULL_PART_KEY;
	  if (keyinfo->algorithm == HA_KEY_ALG_BTREE)
	    keyinfo->rb_tree.size_of_element++;
	}
	switch (keyinfo->seg[j].type) {
	case HA_KEYTYPE_SHORT_INT:
	case HA_KEYTYPE_LONG_INT:
	case HA_KEYTYPE_FLOAT:
	case HA_KEYTYPE_DOUBLE:
	case HA_KEYTYPE_USHORT_INT:
	case HA_KEYTYPE_ULONG_INT:
	case HA_KEYTYPE_LONGLONG:
	case HA_KEYTYPE_ULONGLONG:
	case HA_KEYTYPE_INT24:
	case HA_KEYTYPE_UINT24:
	case HA_KEYTYPE_INT8:
	  keyinfo->seg[j].flag|= HA_SWAP_KEY;
	default:
	  break;
	}
      }
      keyinfo->length= length;
      length+= keyinfo->rb_tree.size_of_element + 
	       ((keyinfo->algorithm == HA_KEY_ALG_BTREE) ? sizeof(byte*) : 0);
      if (length > max_length)
	max_length= length;
      key_segs+= keyinfo->keysegs;
      if (keyinfo->algorithm == HA_KEY_ALG_BTREE)
      {
        key_segs++; /* additional HA_KEYTYPE_END segment */
        if (keyinfo->flag & HA_NULL_PART_KEY)
          keyinfo->get_key_length= hp_rb_null_key_length;
        else
          keyinfo->get_key_length= hp_rb_key_length;
      }
    }
    if (!(share= (HP_SHARE*) my_malloc((uint) sizeof(HP_SHARE)+
				       keys*sizeof(HP_KEYDEF)+
				       key_segs*sizeof(HA_KEYSEG),
				       MYF(MY_ZEROFILL))))
    {
      pthread_mutex_unlock(&THR_LOCK_heap);
      DBUG_RETURN(1);
    }
    share->keydef= (HP_KEYDEF*) (share + 1);
    keyseg= (HA_KEYSEG*) (share->keydef + keys);
    init_block(&share->block, reclength + 1, min_records, max_records);
	/* Fix keys */
    memcpy(share->keydef, keydef, (size_t) (sizeof(keydef[0]) * keys));
    for (i= 0, keyinfo= share->keydef; i < keys; i++, keyinfo++)
    {
      keyinfo->seg= keyseg;
      memcpy(keyseg, keydef[i].seg,
	     (size_t) (sizeof(keyseg[0]) * keydef[i].keysegs));
      keyseg+= keydef[i].keysegs;

      if (keydef[i].algorithm == HA_KEY_ALG_BTREE)
      {
	/* additional HA_KEYTYPE_END keyseg */
	keyseg->type=     HA_KEYTYPE_END;
	keyseg->length=   sizeof(byte*);
	keyseg->flag=     0;
	keyseg->null_bit= 0;
	keyseg++;
	
	init_tree(&keyinfo->rb_tree, 0, 0, sizeof(byte*), 
		  (qsort_cmp2)keys_compare, 1, NULL, NULL);
	keyinfo->delete_key= hp_rb_delete_key;
	keyinfo->write_key= hp_rb_write_key;
      }
      else
      {
	init_block(&keyinfo->block, sizeof(HASH_INFO), min_records, 
		   max_records);
	keyinfo->delete_key= hp_delete_key;
	keyinfo->write_key= hp_write_key;
        keyinfo->hash_buckets= 0;
      }
    }
    share->min_records= min_records;
    share->max_records= max_records;
    share->data_length= share->index_length= 0;
    share->reclength= reclength;
    share->blength= 1;
    share->keys= keys;
    share->max_key_length= max_length;
    share->changed= 0;
    share->auto_key= create_info->auto_key;
    share->auto_key_type= create_info->auto_key_type;
    share->auto_increment= create_info->auto_increment;
    /* Must be allocated separately for rename to work */
    if (!(share->name= my_strdup(name,MYF(0))))
    {
      my_free((gptr) share,MYF(0));
      pthread_mutex_unlock(&THR_LOCK_heap);
      DBUG_RETURN(1);
    }
#ifdef THREAD
    thr_lock_init(&share->lock);
    VOID(pthread_mutex_init(&share->intern_lock,MY_MUTEX_INIT_FAST));
#endif
    share->open_list.data= (void*) share;
    heap_share_list= list_add(heap_share_list,&share->open_list);
  }
  pthread_mutex_unlock(&THR_LOCK_heap);
  DBUG_RETURN(0);
} /* heap_create */

static int keys_compare(heap_rb_param *param, uchar *key1, uchar *key2)
{
  uint not_used;
  return ha_key_cmp(param->keyseg, key1, key2, param->key_length, 
		    param->search_flag, &not_used);
}

static void init_block(HP_BLOCK *block, uint reclength, ulong min_records,
		       ulong max_records)
{
  uint i,recbuffer,records_in_block;

  max_records= max(min_records,max_records);
  if (!max_records)
    max_records= 1000;			/* As good as quess as anything */
  recbuffer= (uint) (reclength + sizeof(byte**) - 1) & ~(sizeof(byte**) - 1);
  records_in_block= max_records / 10;
  if (records_in_block < 10 && max_records)
    records_in_block= 10;
  if (!records_in_block || records_in_block*recbuffer >
      (my_default_record_cache_size-sizeof(HP_PTRS)*HP_MAX_LEVELS))
    records_in_block= (my_default_record_cache_size - sizeof(HP_PTRS) *
		      HP_MAX_LEVELS) / recbuffer + 1;
  block->records_in_block= records_in_block;
  block->recbuffer= recbuffer;
  block->last_allocated= 0L;

  for (i= 0; i <= HP_MAX_LEVELS; i++)
    block->level_info[i].records_under_level=
      (!i ? 1 : i == 1 ? records_in_block :
       HP_PTRS_IN_NOD * block->level_info[i - 1].records_under_level);
}

int heap_delete_table(const char *name)
{
  int result;
  reg1 HP_SHARE *share;
  DBUG_ENTER("heap_delete_table");

  pthread_mutex_lock(&THR_LOCK_heap);
  if ((share= hp_find_named_heap(name)))
  {
    if (share->open_count == 0)
      hp_free(share);
    else
     share->delete_on_close= 1;
    result= 0;
  }
  else
  {
    result= my_errno=ENOENT;
  }
  pthread_mutex_unlock(&THR_LOCK_heap);
  DBUG_RETURN(result);
}

void hp_free(HP_SHARE *share)
{
  heap_share_list= list_delete(heap_share_list, &share->open_list);
  hp_clear(share);			/* Remove blocks from memory */
#ifdef THREAD
  thr_lock_delete(&share->lock);
  VOID(pthread_mutex_destroy(&share->intern_lock));
#endif
  my_free((gptr) share->name, MYF(0));
  my_free((gptr) share, MYF(0));
  return;
}
