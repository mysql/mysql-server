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

/* Check that heap-structure is ok */

#include "heapdef.h"

static int check_one_key(HP_KEYDEF *keydef, uint keynr, ulong records,
			 ulong blength, my_bool print_status);
static int check_one_rb_key(HP_INFO *info, uint keynr, ulong records,
			    my_bool print_status);

/* Returns 0 if the HEAP is ok */

int heap_check_heap(HP_INFO *info, my_bool print_status)
{
  int error;
  uint key;
  HP_SHARE *share=info->s;
  DBUG_ENTER("heap_check_keys");

  for (error=key=0 ; key < share->keys ; key++)
  {
    if (share->keydef[key].algorithm == HA_KEY_ALG_BTREE)
      error|= check_one_rb_key(info, key, share->records, print_status);
    else
      error|= check_one_key(share->keydef + key, key, share->records,
			    share->blength, print_status);
  }

  DBUG_RETURN(error);
}


static int check_one_key(HP_KEYDEF *keydef, uint keynr, ulong records,
			 ulong blength, my_bool print_status)
{
  int error;
  uint i,found,max_links,seek,links;
  uint rec_link;				/* Only used with debugging */
  HASH_INFO *hash_info;

  error=0;
  for (i=found=max_links=seek=0 ; i < records ; i++)
  {
    hash_info=hp_find_hash(&keydef->block,i);
    if (hp_mask(hp_rec_hashnr(keydef, hash_info->ptr_to_rec),
		blength,records) == i)
    {
      found++;
      seek++;
      links=1;
      while ((hash_info=hash_info->next_key) && found < records + 1)
      {
	seek+= ++links;
	if ((rec_link = hp_mask(hp_rec_hashnr(keydef, hash_info->ptr_to_rec),
			        blength, records))
	    != i)
	{
	  DBUG_PRINT("error",("Record in wrong link: Link %d  Record: %lx  Record-link %d", i,hash_info->ptr_to_rec,rec_link));
	  error=1;
	}
	else
	  found++;
      }
      if (links > max_links) max_links=links;
    }
  }
  if (found != records)
  {
    DBUG_PRINT("error",("Found %ld of %ld records"));
    error=1;
  }
  DBUG_PRINT("info",
	     ("records: %ld   seeks: %d   max links: %d   hitrate: %.2f",
	      records,seek,max_links,
	      (float) seek / (float) (records ? records : 1)));
  if (print_status)
    printf("Key: %d  records: %ld   seeks: %d   max links: %d   hitrate: %.2f\n",
	   keynr, records, seek, max_links,
	   (float) seek / (float) (records ? records : 1));
  return error;
}

static int check_one_rb_key(HP_INFO *info, uint keynr, ulong records,
			    my_bool print_status)
{
  HP_KEYDEF *keydef= info->s->keydef + keynr;
  int error= 0;
  ulong found= 0;
  byte *key, *recpos;
  uint key_length;
  uint not_used;
  
  if ((key= tree_search_edge(&keydef->rb_tree, info->parents,
			     &info->last_pos, offsetof(TREE_ELEMENT, left))))
  {
    do
    {
      memcpy(&recpos, key + (*keydef->get_key_length)(keydef,key), sizeof(byte*));
      key_length= hp_rb_make_key(keydef, info->recbuf, recpos, 0);
      if (ha_key_cmp(keydef->seg, info->recbuf, key, key_length, 
		     SEARCH_FIND | SEARCH_SAME, &not_used))
      {
	error= 1;
	DBUG_PRINT("error",("Record in wrong link: Link %d  Record: %lx\n", 
		   link, recpos));
      }
      else
      {
	found++;
      }
      key= tree_search_next(&keydef->rb_tree, &info->last_pos,
			    offsetof(TREE_ELEMENT, left), 
			    offsetof(TREE_ELEMENT, right));
    } while (key);
  }
  if (found != records)
  {
    DBUG_PRINT("error",("Found %ld of %ld records"));
    error= 1;
  }
  if (print_status)
    printf("Key: %d  records: %ld\n", keynr, records);
  return error;
}
