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

/* remove current record in heap-database */

#include "heapdef.h"

int heap_delete(HP_INFO *info, const byte *record)
{
  uint key;
  byte *pos;
  HP_SHARE *share=info->s;
  DBUG_ENTER("heap_delete");
  DBUG_PRINT("enter",("info: %lx  record: %lx",info,record));

  test_active(info);

  if (info->opt_flag & READ_CHECK_USED && _hp_rectest(info,record))
    DBUG_RETURN(my_errno);			/* Record changed */
  share->changed=1;

  if ( --(share->records) < share->blength >> 1) share->blength>>=1;
  pos=info->current_ptr;

  for (key=0 ; key < share->keys ; key++)
  {
    if (_hp_delete_key(info,share->keydef+key,record,pos,
		       key == (uint) info->lastinx))
      goto err;
  }

  info->update=HA_STATE_DELETED;
  *((byte**) pos)=share->del_link;
  share->del_link=pos;
  pos[share->reclength]=0;		/* Record deleted */
  share->deleted++;
  info->current_hash_ptr=0;
  DBUG_RETURN(0);
 err:
  if( ++(share->records) == share->blength) share->blength+= share->blength;
  DBUG_RETURN(my_errno);
}

	/* Remove one key from hash-table */
	/* Flag is set if we want's to correct info->current_ptr */

int _hp_delete_key(HP_INFO *info, register HP_KEYDEF *keyinfo,
		   const byte *record, byte *recpos, int flag)
{
  ulong blength,pos2,pos_hashnr,lastpos_hashnr;
  HASH_INFO *lastpos,*gpos,*pos,*pos3,*empty,*last_ptr;
  HP_SHARE *share=info->s;
  DBUG_ENTER("_hp_delete_key");

  blength=share->blength;
  if (share->records+1 == blength) blength+= blength;
  lastpos=hp_find_hash(&keyinfo->block,share->records);
  last_ptr=0;

  /* Search after record with key */
  pos= hp_find_hash(&keyinfo->block,
		    _hp_mask(_hp_rec_hashnr(keyinfo,record),blength,
			     share->records+1));
  gpos = pos3 = 0;

  while (pos->ptr_to_rec != recpos)
  {
    if (flag && !_hp_rec_key_cmp(keyinfo,record,pos->ptr_to_rec))
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
    DBUG_PRINT("info",("Corrected current_ptr to point at: %lx",
		       info->current_ptr));
  }
  empty=pos;
  if (gpos)
    gpos->next_key=pos->next_key;	/* unlink current ptr */
  else if (pos->next_key)
  {
    empty=pos->next_key;
    pos->ptr_to_rec=empty->ptr_to_rec;
    pos->next_key=empty->next_key;
  }

  if (empty == lastpos)			/* deleted last hash key */
    DBUG_RETURN (0);

  /* Move the last key (lastpos) */
  lastpos_hashnr=_hp_rec_hashnr(keyinfo,lastpos->ptr_to_rec);
  /* pos is where lastpos should be */
  pos=hp_find_hash(&keyinfo->block,_hp_mask(lastpos_hashnr,share->blength,
					    share->records));
  if (pos == empty)			/* Move to empty position. */
  {
    empty[0]=lastpos[0];
    DBUG_RETURN(0);
  }
  pos_hashnr=_hp_rec_hashnr(keyinfo,pos->ptr_to_rec);
  /* pos3 is where the pos should be */
  pos3= hp_find_hash(&keyinfo->block,
		     _hp_mask(pos_hashnr,share->blength,share->records));
  if (pos != pos3)
  {					/* pos is on wrong posit */
    empty[0]=pos[0];			/* Save it here */
    pos[0]=lastpos[0];			/* This shold be here */
    _hp_movelink(pos,pos3,empty);	/* Fix link to pos */
    DBUG_RETURN(0);
  }
  pos2= _hp_mask(lastpos_hashnr,blength,share->records+1);
  if (pos2 == _hp_mask(pos_hashnr,blength,share->records+1))
  {					/* Identical key-positions */
    if (pos2 != share->records)
    {
      empty[0]=lastpos[0];
      _hp_movelink(lastpos,pos,empty);
      DBUG_RETURN(0);
    }
    pos3= pos;				/* Link pos->next after lastpos */
  }
  else pos3= 0;				/* Different positions merge */

  empty[0]=lastpos[0];
  _hp_movelink(pos3,empty,pos->next_key);
  pos->next_key=empty;
  DBUG_RETURN(0);
}
