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

int heap_check_heap(HP_INFO *info,my_bool print_status)
{
  int error;
  uint key;
  ulong records=0, deleted=0, pos, next_block;
  HP_SHARE *share=info->s;
  HP_INFO save_info= *info;			/* Needed because scan_init */
  DBUG_ENTER("heap_check_heap");

  for (error=key= 0 ; key < share->keys ; key++)
    error|=check_one_key(share->keydef+key,key, share->records,share->blength,
			 print_status);

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
    _hp_find_record(info,pos);

    if (!info->current_ptr[share->reclength])
      deleted++;
    else
      records++;
  }

  if (records != share->records || deleted != share->deleted)
  {
    DBUG_PRINT("error",("Found rows: %lu (%lu)  deleted %lu (%lu)",
			records, share->records, deleted, share->deleted));
    error= 1;
  }
  *info= save_info;
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
    if (_hp_mask(_hp_rec_hashnr(keydef,hash_info->ptr_to_rec),
		 blength,records) == i)
    {
      found++;
      seek++;
      links=1;
      while ((hash_info=hash_info->next_key) && found < records + 1)
      {
	seek+= ++links;
	if ((rec_link=_hp_mask(_hp_rec_hashnr(keydef,hash_info->ptr_to_rec),
			       blength,records))
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
