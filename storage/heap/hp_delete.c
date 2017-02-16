/* Copyright (c) 2000, 2012, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA */

/* remove current record in heap-database */

#include "heapdef.h"

int heap_delete(HP_INFO *info, const uchar *record)
{
  uchar *pos;
  HP_SHARE *share=info->s;
  HP_KEYDEF *keydef, *end, *p_lastinx;
  DBUG_ENTER("heap_delete");
  DBUG_PRINT("enter",("info: 0x%lx  record: 0x%lx", (long) info, (long) record));

  test_active(info);

  if (info->opt_flag & READ_CHECK_USED && hp_rectest(info,record))
    DBUG_RETURN(my_errno);			/* Record changed */
  share->changed=1;

  if ( --(share->records) < share->blength >> 1) share->blength>>=1;
  pos=info->current_ptr;

  p_lastinx = share->keydef + info->lastinx;
  for (keydef = share->keydef, end = keydef + share->keys; keydef < end; 
       keydef++)
  {
    if ((*keydef->delete_key)(info, keydef, record, pos, keydef == p_lastinx))
      goto err;
  }

  info->update=HA_STATE_DELETED;
  *((uchar**) pos)=share->del_link;
  share->del_link=pos;
  pos[share->reclength]=0;		/* Record deleted */
  share->deleted++;
  share->key_version++;
#if !defined(DBUG_OFF) && defined(EXTRA_HEAP_DEBUG)
  DBUG_EXECUTE("check_heap",heap_check_heap(info, 0););
#endif

  DBUG_RETURN(0);
err:
  if (++(share->records) == share->blength)
    share->blength+= share->blength;
  DBUG_RETURN(my_errno);
}


/*
  Remove one key from rb-tree
*/

int hp_rb_delete_key(HP_INFO *info, register HP_KEYDEF *keyinfo,
		   const uchar *record, uchar *recpos, int flag)
{
  heap_rb_param custom_arg;
  ulong old_allocated;
  int res;

  if (flag) 
    info->last_pos= NULL; /* For heap_rnext/heap_rprev */

  custom_arg.keyseg= keyinfo->seg;
  custom_arg.key_length= hp_rb_make_key(keyinfo, info->recbuf, record, recpos);
  custom_arg.search_flag= SEARCH_SAME;
  old_allocated= keyinfo->rb_tree.allocated;
  res= tree_delete(&keyinfo->rb_tree, info->recbuf, custom_arg.key_length,
                   &custom_arg);
  info->s->index_length-= (old_allocated - keyinfo->rb_tree.allocated);
  return res;
}


/*
  Remove one key from hash-table

  SYNPOSIS
    hp_delete_key()
    info		Hash handler
    keyinfo		key definition of key that we want to delete
    record		row data to be deleted
    recpos		Pointer to heap record in memory
    flag		Is set if we want's to correct info->current_ptr

  RETURN
    0      Ok
    other  Error code
*/

int hp_delete_key(HP_INFO *info, register HP_KEYDEF *keyinfo,
		  const uchar *record, uchar *recpos, int flag)
{
  ulong blength, pos2, pos_hashnr, lastpos_hashnr, key_pos;
  HASH_INFO *lastpos,*gpos,*pos,*pos3,*empty,*last_ptr;
  HP_SHARE *share=info->s;
  DBUG_ENTER("hp_delete_key");

  blength=share->blength;
  if (share->records+1 == blength)
    blength+= blength;
  lastpos=hp_find_hash(&keyinfo->block,share->records);
  last_ptr=0;

  /* Search after record with key */
  key_pos= hp_mask(hp_rec_hashnr(keyinfo, record), blength, share->records + 1);
  pos= hp_find_hash(&keyinfo->block, key_pos);

  gpos = pos3 = 0;

  while (pos->ptr_to_rec != recpos)
  {
    if (flag && !hp_rec_key_cmp(keyinfo, record, pos->ptr_to_rec, 0))
      last_ptr=pos;				/* Previous same key */
    gpos=pos;
    if (!(pos=pos->next_key))
    {
      DBUG_RETURN(my_errno=HA_ERR_CRASHED);	/* This shouldn't happend */
    }
  }

  /* Remove link to record */

  if (flag)
  {
    /* Save for heap_rnext/heap_rprev */
    info->current_hash_ptr=last_ptr;
    info->current_ptr = last_ptr ? last_ptr->ptr_to_rec : 0;
    DBUG_PRINT("info",("Corrected current_ptr to point at: 0x%lx",
		       (long) info->current_ptr));
  }
  empty=pos;
  if (gpos)
    gpos->next_key=pos->next_key;	/* unlink current ptr */
  else if (pos->next_key)
  {
    empty=pos->next_key;
    pos->ptr_to_rec=  empty->ptr_to_rec;
    pos->next_key=    empty->next_key;
    pos->hash_of_key= empty->hash_of_key;
  }
  else
    keyinfo->hash_buckets--;

  if (empty == lastpos)			/* deleted last hash key */
    DBUG_RETURN (0);

  /* Move the last key (lastpos) */
  lastpos_hashnr= lastpos->hash_of_key;
  /* pos is where lastpos should be */
  pos=hp_find_hash(&keyinfo->block, hp_mask(lastpos_hashnr, share->blength,
					    share->records));
  if (pos == empty)			/* Move to empty position. */
  {
    empty[0]=lastpos[0];
    DBUG_RETURN(0);
  }
  pos_hashnr= pos->hash_of_key;
  /* pos3 is where the pos should be */
  pos3= hp_find_hash(&keyinfo->block,
		     hp_mask(pos_hashnr, share->blength, share->records));
  if (pos != pos3)
  {					/* pos is on wrong posit */
    empty[0]=pos[0];			/* Save it here */
    pos[0]=lastpos[0];			/* This shold be here */
    hp_movelink(pos, pos3, empty);	/* Fix link to pos */
    DBUG_RETURN(0);
  }
  pos2= hp_mask(lastpos_hashnr, blength, share->records + 1);
  if (pos2 == hp_mask(pos_hashnr, blength, share->records + 1))
  {
    /* lastpos and the row in the main bucket entry (pos) has the same hash */ 
    if (pos2 != share->records)
    {
      /*
        The bucket entry was not deleted. Copy lastpos over the
        deleted entry and update previous link to point to it.
      */
      empty[0]= lastpos[0];
      hp_movelink(lastpos, pos, empty);
      if (last_ptr == lastpos)
      {
        /*
          We moved the row that info->current_hash_ptr points to.
          Update info->current_hash_ptr to point to the new position.
        */
        info->current_hash_ptr= empty;
      }
      DBUG_RETURN(0);
    }
    /*
      Shrinking the hash table deleted the main bucket entry for this hash.
      In this case the last entry was the first key in the key chain.
      We move things around so that we keep the original key order to ensure
      that heap_rnext() works.
      
      - Move the row at the main bucket entry to the empty spot.
      - Move the last entry first in the new chain.
      - Link in the first element of the hash.
    */
    empty[0]= pos[0];
    pos[0]= lastpos[0];
    hp_movelink(pos, pos, empty);

    /* Update current_hash_ptr if the entry moved */
    if (last_ptr == lastpos)
      info->current_hash_ptr= pos;
    else if (last_ptr == pos)
      info->current_hash_ptr= empty;
    DBUG_RETURN(0);
  }

  pos3= 0;				/* Different positions merge */
  keyinfo->hash_buckets--;
  empty[0]=lastpos[0];
  hp_movelink(pos3, empty, pos->next_key);
  pos->next_key=empty;
  DBUG_RETURN(0);
}
