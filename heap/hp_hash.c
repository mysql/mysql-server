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

/* The hash functions used for saveing keys */

#include "heapdef.h"
#include <m_ctype.h>

	/* Search after a record based on a key */
	/* Sets info->current_ptr to found record */
	/* next_flag:  Search=0, next=1, prev =2, same =3 */

byte *_hp_search(HP_INFO *info, HP_KEYDEF *keyinfo, const byte *key,
		 uint nextflag)
{
  reg1 HASH_INFO *pos,*prev_ptr;
  int flag;
  uint old_nextflag;
  HP_SHARE *share=info->s;
  DBUG_ENTER("_hp_search");

  old_nextflag=nextflag;
  flag=1;
  prev_ptr=0;

  if (share->records)
  {
    pos=hp_find_hash(&keyinfo->block,_hp_mask(_hp_hashnr(keyinfo,key),
					      share->blength,share->records));
    do
    {
      if (!_hp_key_cmp(keyinfo,pos->ptr_to_rec,key))
      {
	switch (nextflag) {
	case 0:					/* Search after key */
	  DBUG_PRINT("exit",("found key at %d",pos->ptr_to_rec));
	  info->current_hash_ptr=pos;
	  DBUG_RETURN(info->current_ptr= pos->ptr_to_rec);
	case 1:					/* Search next */
	  if (pos->ptr_to_rec == info->current_ptr)
	    nextflag=0;
	  break;
	case 2:					/* Search previous */
	  if (pos->ptr_to_rec == info->current_ptr)
	  {
	    my_errno=HA_ERR_KEY_NOT_FOUND;	/* If gpos == 0 */
	    info->current_hash_ptr=prev_ptr;
	    DBUG_RETURN(info->current_ptr=prev_ptr ? prev_ptr->ptr_to_rec : 0);
	  }
	  prev_ptr=pos;				/* Prev. record found */
	  break;
	case 3:					/* Search same */
	  if (pos->ptr_to_rec == info->current_ptr)
	  {
	    info->current_hash_ptr=pos;
	    DBUG_RETURN(info->current_ptr);
	  }
	}
      }
      if (flag)
      {
	flag=0;					/* Reset flag */
	if (hp_find_hash(&keyinfo->block,
			 _hp_mask(_hp_rec_hashnr(keyinfo,pos->ptr_to_rec),
				  share->blength,share->records)) != pos)
	  break;				/* Wrong link */
      }
    }
    while ((pos=pos->next_key));
  }
  my_errno=HA_ERR_KEY_NOT_FOUND;
  if (nextflag == 2 && ! info->current_ptr)
  {
    /* Do a previous from end */
    info->current_hash_ptr=prev_ptr;
    DBUG_RETURN(info->current_ptr=prev_ptr ? prev_ptr->ptr_to_rec : 0);
  }

  if (old_nextflag && nextflag)
    my_errno=HA_ERR_RECORD_CHANGED;		/* Didn't find old record */
  DBUG_PRINT("exit",("Error: %d",my_errno));
  info->current_hash_ptr=0;  
  DBUG_RETURN((info->current_ptr= 0));
}


/*
  Search next after last read;  Assumes that the table hasn't changed
  since last read !
*/

byte *_hp_search_next(HP_INFO *info, HP_KEYDEF *keyinfo, const byte *key,
		      HASH_INFO *pos)
{
  DBUG_ENTER("_hp_search_next");

  while ((pos= pos->next_key))
  {
    if (!_hp_key_cmp(keyinfo,pos->ptr_to_rec,key))
    {
      info->current_hash_ptr=pos;
      DBUG_RETURN (info->current_ptr= pos->ptr_to_rec);
    }
  }
  my_errno=HA_ERR_KEY_NOT_FOUND;
  DBUG_PRINT("exit",("Error: %d",my_errno));
  info->current_hash_ptr=0;
  DBUG_RETURN ((info->current_ptr= 0));
}


	/* Calculate pos according to keys */

ulong _hp_mask(ulong hashnr, ulong buffmax, ulong maxlength)
{
  if ((hashnr & (buffmax-1)) < maxlength) return (hashnr & (buffmax-1));
  return (hashnr & ((buffmax >> 1) -1));
}


	/* Change link from pos to new_link */

void _hp_movelink(HASH_INFO *pos, HASH_INFO *next_link, HASH_INFO *newlink)
{
  HASH_INFO *old_link;
  do
  {
    old_link=next_link;
  }
  while ((next_link=next_link->next_key) != pos);
  old_link->next_key=newlink;
  return;
}

#ifndef NEW_HASH_FUNCTION

	/* Calc hashvalue for a key */

ulong _hp_hashnr(register HP_KEYDEF *keydef, register const byte *key)
{
  register ulong nr=1, nr2=4;
  HP_KEYSEG *seg,*endseg;

  for (seg=keydef->seg,endseg=seg+keydef->keysegs ; seg < endseg ; seg++)
  {
    uchar *pos=(uchar*) key;
    key+=seg->length;
    if (seg->type == HA_KEYTYPE_TEXT)
    {
      for (; pos < (uchar*) key ; pos++)
      {
	nr^=(ulong) ((((uint) nr & 63)+nr2)*((uint) my_sort_order[(uint) *pos]))+ (nr << 8);
	nr2+=3;
      }
    }
    else
    {
      for (; pos < (uchar*) key ; pos++)
      {
	nr^=(ulong) ((((uint) nr & 63)+nr2)*((uint) *pos))+ (nr << 8);
	nr2+=3;
      }
    }
  }
  return((ulong) nr);
}

	/* Calc hashvalue for a key in a record */

ulong _hp_rec_hashnr(register HP_KEYDEF *keydef, register const byte *rec)
{
  register ulong nr=1, nr2=4;
  HP_KEYSEG *seg,*endseg;

  for (seg=keydef->seg,endseg=seg+keydef->keysegs ; seg < endseg ; seg++)
  {
    uchar *pos=(uchar*) rec+seg->start,*end=pos+seg->length;
    if (seg->type == HA_KEYTYPE_TEXT)
    {
      for (; pos < end ; pos++)
      {
	nr^=(ulong) ((((uint) nr & 63)+nr2)*((uint) my_sort_order[(uint) *pos]))+ (nr << 8);
	nr2+=3;
      }
    }
    else
    {
      for (; pos < end ; pos++)
      {
	nr^=(ulong) ((((uint) nr & 63)+nr2)*((uint) *pos))+ (nr << 8);
	nr2+=3;
      }
    }
  }
  return((ulong) nr);
}

#else

/*
 * Fowler/Noll/Vo hash
 *
 * The basis of the hash algorithm was taken from an idea sent by email to the
 * IEEE Posix P1003.2 mailing list from Phong Vo (kpv@research.att.com) and
 * Glenn Fowler (gsf@research.att.com).  Landon Curt Noll (chongo@toad.com)
 * later improved on their algorithm.
 *
 * The magic is in the interesting relationship between the special prime
 * 16777619 (2^24 + 403) and 2^32 and 2^8.
 *
 * This hash produces the fewest collisions of any function that we've seen so
 * far, and works well on both numbers and strings.
 */

ulong _hp_hashnr(register HP_KEYDEF *keydef, register const byte *key)
{
  register ulong nr=0;
  HP_KEYSEG *seg,*endseg;

  for (seg=keydef->seg,endseg=seg+keydef->keysegs ; seg < endseg ; seg++)
  {
    uchar *pos=(uchar*) key;
    key+=seg->length;
    if (seg->type == HA_KEYTYPE_TEXT)
    {
      for (; pos < (uchar*) key ; pos++)
      {
	nr *=16777619; 
	nr ^=((uint) my_sort_order[(uint) *pos]);
      }
    }
    else
    {
      for ( ; pos < (uchar*) key ; pos++)
      {
	nr *=16777619; 
	nr ^=(uint) *pos;
      }
    }
  }
  return((ulong) nr);
}

	/* Calc hashvalue for a key in a record */

ulong _hp_rec_hashnr(register HP_KEYDEF *keydef, register const byte *rec)
{
  register ulong nr=0;
  HP_KEYSEG *seg,*endseg;

  for (seg=keydef->seg,endseg=seg+keydef->keysegs ; seg < endseg ; seg++)
  {
    uchar *pos=(uchar*) rec+seg->start,*end=pos+seg->length;
    if (seg->type == HA_KEYTYPE_TEXT)
    {
      for ( ; pos < end ; pos++)
      {
	nr *=16777619; 
	nr ^=(uint) my_sort_order[(uint) *pos];
      }
    }
    else
    {
      for ( ; pos < end ; pos++)
      {
	nr *=16777619; 
	nr ^=(uint) *pos;
      }
    }
  }
  return((ulong) nr);
}

#endif


	/* Compare keys for two records. Returns 0 if they are identical */

int _hp_rec_key_cmp(HP_KEYDEF *keydef, const byte *rec1, const byte *rec2)
{
  HP_KEYSEG *seg,*endseg;

  for (seg=keydef->seg,endseg=seg+keydef->keysegs ; seg < endseg ; seg++)
  {
    if (seg->type == HA_KEYTYPE_TEXT)
    {
      if (my_sortcmp(rec1+seg->start,rec2+seg->start,seg->length))
	return 1;
    }
    else
    {
      if (bcmp(rec1+seg->start,rec2+seg->start,seg->length))
	return 1;
    }
  }
  return 0;
}

	/* Compare a key in a record to a hole key */

int _hp_key_cmp(HP_KEYDEF *keydef, const byte *rec, const byte *key)
{
  HP_KEYSEG *seg,*endseg;

  for (seg=keydef->seg,endseg=seg+keydef->keysegs ; seg < endseg ; seg++)
  {
    if (seg->type == HA_KEYTYPE_TEXT)
    {
      if (my_sortcmp(rec+seg->start,key,seg->length))
	return 1;
    }
    else
    {
      if (bcmp(rec+seg->start,key,seg->length))
	return 1;
    }
    key+=seg->length;
  }
  return 0;
}


	/* Copy a key from a record to a keybuffer */

void _hp_make_key(HP_KEYDEF *keydef, byte *key, const byte *rec)
{
  HP_KEYSEG *seg,*endseg;

  for (seg=keydef->seg,endseg=seg+keydef->keysegs ; seg < endseg ; seg++)
  {
    memcpy(key,rec+seg->start,(size_t) seg->length);
    key+=seg->length;
  }
}
