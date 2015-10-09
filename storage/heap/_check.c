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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

/* Check that heap-structure is ok */

#include "heapdef.h"

static int check_one_key(HP_KEYDEF *keydef, uint keynr, ulong records,
			 ulong blength, my_bool print_status);
static int check_one_rb_key(HP_INFO *info, uint keynr, ulong records,
			    my_bool print_status);


/*
  Check if keys and rows are ok in a heap table

  SYNOPSIS
    heap_check_heap()
    info		Table handler
    print_status	Prints some extra status

  NOTES
    Doesn't change the state of the table handler

  RETURN VALUES
    0	ok
    1 error
*/

int heap_check_heap(HP_INFO *info, my_bool print_status)
{
  int error;
  uint key;
  ulong records=0, deleted=0, pos, next_block;
  HP_SHARE *share=info->s;
  HP_INFO save_info= *info;			/* Needed because scan_init */
  DBUG_ENTER("heap_check_heap");

  for (error=key= 0 ; key < share->keys ; key++)
  {
    if (share->keydef[key].algorithm == HA_KEY_ALG_BTREE)
      error|= check_one_rb_key(info, key, share->records, print_status);
    else
      error|= check_one_key(share->keydef + key, key, share->records,
			    share->blength, print_status);
  }
  /*
    This is basicly the same code as in hp_scan, but we repeat it here to
    get shorter DBUG log file.
  */
  for (pos=next_block= 0 ; ; pos++)
  {
    if (pos < next_block)
    {
      info->current_ptr+= share->block.recbuffer;
    }
    else
    {
      next_block+= share->block.records_in_block;
      if (next_block >= share->records+share->deleted)
      {
	next_block= share->records+share->deleted;
	if (pos >= next_block)
	  break;				/* End of file */
      }
    }
    hp_find_record(info,pos);

    if (!info->current_ptr[share->reclength])
      deleted++;
    else
      records++;
  }

  if (records != share->records || deleted != share->deleted)
  {
    DBUG_PRINT("error",("Found rows: %lu (%lu)  deleted %lu (%lu)",
			records, (ulong) share->records,
                        deleted, (ulong) share->deleted));
    error= 1;
  }
  *info= save_info;
  DBUG_RETURN(error);
}


static int check_one_key(HP_KEYDEF *keydef, uint keynr, ulong records,
			 ulong blength, my_bool print_status)
{
  int error;
  ulong i,found,max_links,seek,links;
  ulong rec_link;				/* Only used with debugging */
  ulong hash_buckets_found;
  HASH_INFO *hash_info;

  error=0;
  hash_buckets_found= 0;
  for (i=found=max_links=seek=0 ; i < records ; i++)
  {
    hash_info=hp_find_hash(&keydef->block,i);
    if (hp_mask(hash_info->hash, blength,records) == i)
    {
      found++;
      seek++;
      links=1;
      while ((hash_info=hash_info->next_key) && found < records + 1)
      {
	seek+= ++links;
	if (i != (rec_link= hp_mask(hash_info->hash, blength, records)))
	{
	  DBUG_PRINT("error",
                     ("Record in wrong link: Link %lu  Record: 0x%lx  Record-link %lu",
                      i, (long) hash_info->ptr_to_rec, rec_link));
	  error=1;
	}
	else
	  found++;
      }
      if (links > max_links) max_links=links;
      hash_buckets_found++;
    }
  }
  if (found != records)
  {
    DBUG_PRINT("error",("Found %ld of %ld records", found, records));
    error=1;
  }
  if (keydef->hash_buckets != hash_buckets_found)
  {
    DBUG_PRINT("error",("Found %ld buckets, stats shows %ld buckets",
                        hash_buckets_found, (long) keydef->hash_buckets));
    error=1;
  }
  DBUG_PRINT("info",
	     ("records: %ld   seeks: %lu   max links: %lu   hitrate: %.2f   "
              "buckets: %lu",
	      records,seek,max_links,
	      (float) seek / (float) (records ? records : 1), 
              hash_buckets_found));
  if (print_status)
    printf("Key: %d  records: %ld   seeks: %lu   max links: %lu   "
           "hitrate: %.2f   buckets: %lu\n",
	   keynr, records, seek, max_links,
	   (float) seek / (float) (records ? records : 1), 
           hash_buckets_found);
  return error;
}

static int check_one_rb_key(HP_INFO *info, uint keynr, ulong records,
			    my_bool print_status)
{
  HP_KEYDEF *keydef= info->s->keydef + keynr;
  int error= 0;
  ulong found= 0;
  uchar *key, *recpos;
  uint key_length;
  uint not_used[2];
  
  if ((key= tree_search_edge(&keydef->rb_tree, info->parents,
			     &info->last_pos, offsetof(TREE_ELEMENT, left))))
  {
    do
    {
      memcpy(&recpos, key + (*keydef->get_key_length)(keydef,key), sizeof(uchar*));
      key_length= hp_rb_make_key(keydef, info->recbuf, recpos, 0);
      if (ha_key_cmp(keydef->seg, (uchar*) info->recbuf, (uchar*) key,
		     key_length, SEARCH_FIND | SEARCH_SAME, not_used))
      {
	error= 1;
	DBUG_PRINT("error",("Record in wrong link:  key: %u  Record: 0x%lx\n", 
			    keynr, (long) recpos));
      }
      else
	found++;
      key= tree_search_next(&keydef->rb_tree, &info->last_pos,
			    offsetof(TREE_ELEMENT, left), 
			    offsetof(TREE_ELEMENT, right));
    } while (key);
  }
  if (found != records)
  {
    DBUG_PRINT("error",("Found %lu of %lu records", found, records));
    error= 1;
  }
  if (print_status)
    printf("Key: %d  records: %ld\n", keynr, records);
  return error;
}
