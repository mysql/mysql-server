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

/* remove current record in heap-database */

#include <stddef.h>
#include <sys/types.h>

#include "my_dbug.h"
#include "my_inttypes.h"
#include "storage/heap/heapdef.h"

int heap_delete(HP_INFO *info, const uchar *record)
{
  uchar *pos;
  HP_SHARE *share=info->s;
  HP_KEYDEF *keydef, *end, *p_lastinx;
  DBUG_ENTER("heap_delete");
  DBUG_PRINT("enter",("info: %p  record: %p", info, record));

  test_active(info);

  if (info->opt_flag & READ_CHECK_USED && hp_rectest(info,record))
    DBUG_RETURN(my_errno());			/* Record changed */
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
  info->current_hash_ptr=0;
#if !defined(DBUG_OFF) && defined(EXTRA_HEAP_DEBUG)
  DBUG_EXECUTE("check_heap",heap_check_heap(info, 0););
#endif

  DBUG_RETURN(0);
err:
  if (++(share->records) == share->blength)
    share->blength+= share->blength;
  DBUG_RETURN(my_errno());
}


/*
  Remove one key from rb-tree
*/

int hp_rb_delete_key(HP_INFO *info, HP_KEYDEF *keyinfo,
		   const uchar *record, uchar *recpos, int flag)
{
  heap_rb_param custom_arg;
  uint old_allocated;
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

int hp_delete_key(HP_INFO *info, HP_KEYDEF *keyinfo,
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
    if (flag && !hp_rec_key_cmp(keyinfo, record, pos->ptr_to_rec))
      last_ptr=pos;				/* Previous same key */
    gpos=pos;
    if (!(pos=pos->next_key))
    {
      set_my_errno(HA_ERR_CRASHED);
      DBUG_RETURN(HA_ERR_CRASHED);	/* This shouldn't happend */
    }
  }

  /* Remove link to record */

  if (flag)
  {
    /* Save for heap_rnext/heap_rprev */
    info->current_hash_ptr=last_ptr;
    info->current_ptr = last_ptr ? last_ptr->ptr_to_rec : 0;
    DBUG_PRINT("info",("Corrected current_ptr to point at: %p",
		       info->current_ptr));
  }
  empty=pos;
  if (gpos)
    gpos->next_key=pos->next_key;	/* unlink current ptr */
  else if (pos->next_key)
  {
    empty=pos->next_key;
    *pos= *empty;
  }
  else
    keyinfo->hash_buckets--;

  if (empty == lastpos)			/* deleted last hash key */
    DBUG_RETURN (0);

  /* Move the last key (lastpos) */
  lastpos_hashnr= lastpos->hash;
  /* pos is where lastpos should be */
  pos=hp_find_hash(&keyinfo->block, hp_mask(lastpos_hashnr, share->blength,
					    share->records));
  if (pos == empty)			/* Move to empty position. */
  {
    empty[0]=lastpos[0];
    DBUG_RETURN(0);
  }
  pos_hashnr= pos->hash;
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
  {					/* Identical key-positions */
    if (pos2 != share->records)
    {
      empty[0]=lastpos[0];
      hp_movelink(lastpos, pos, empty);
      DBUG_RETURN(0);
    }
    pos3= pos;				/* Link pos->next after lastpos */
    /* 
      One of elements from the bucket we're scanning is moved to the
      beginning of the list. Reset search since this element may not have
      been processed yet. 
    */
    if (flag && pos2 == key_pos)
    {
      info->current_ptr= 0;
      info->current_hash_ptr= 0;
    }
  }
  else
  {
    pos3= 0;				/* Different positions merge */
    keyinfo->hash_buckets--;
  }

  empty[0]=lastpos[0];
  hp_movelink(pos3, empty, pos->next_key);
  pos->next_key=empty;
  DBUG_RETURN(0);
}
