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

ha_rows hp_rb_records_in_range(HP_INFO *info, int inx, const byte *start_key,
			       uint start_key_len,
			       enum ha_rkey_function start_search_flag,
			       const byte *end_key, uint end_key_len,
			       enum ha_rkey_function end_search_flag)
{
  ha_rows start_pos, end_pos;
  HP_KEYDEF *keyinfo= info->s->keydef + inx;
  TREE *rb_tree = &keyinfo->rb_tree;
  heap_rb_param custom_arg;

  info->lastinx= inx;
  custom_arg.keyseg= keyinfo->seg;
  custom_arg.search_flag= SEARCH_FIND | SEARCH_SAME;
  if (start_key)
  {
    custom_arg.key_length= hp_rb_pack_key(keyinfo, (uchar*) info->recbuf,
					  (uchar*) start_key, 
					  start_key_len);
    start_pos= tree_record_pos(rb_tree, info->recbuf, start_search_flag, 
			       &custom_arg);
  }
  else
  {
    start_pos= 0;
  }
  
  if (end_key)
  {
    custom_arg.key_length= hp_rb_pack_key(keyinfo, (uchar*) info->recbuf,
					  (uchar*) end_key, end_key_len);
    end_pos= tree_record_pos(rb_tree, info->recbuf, end_search_flag, 
			     &custom_arg);
  }
  else
  {
    end_pos= rb_tree->elements_in_tree + (ha_rows)1;
  }

  if (start_pos == HA_POS_ERROR || end_pos == HA_POS_ERROR)
    return HA_POS_ERROR;
  return end_pos < start_pos ? (ha_rows) 0 :
	      (end_pos == start_pos ? (ha_rows) 1 : end_pos - start_pos);
}

	/* Search after a record based on a key */
	/* Sets info->current_ptr to found record */
	/* next_flag:  Search=0, next=1, prev =2, same =3 */

byte *hp_search(HP_INFO *info, HP_KEYDEF *keyinfo, const byte *key,
		 uint nextflag)
{
  reg1 HASH_INFO *pos,*prev_ptr;
  int flag;
  uint old_nextflag;
  HP_SHARE *share=info->s;
  DBUG_ENTER("hp_search");
  old_nextflag=nextflag;
  flag=1;
  prev_ptr=0;

  if (share->records)
  {
    pos=hp_find_hash(&keyinfo->block, hp_mask(hp_hashnr(keyinfo, key),
					      share->blength, share->records));
    do
    {
      if (!hp_key_cmp(keyinfo, pos->ptr_to_rec, key))
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
			 hp_mask(hp_rec_hashnr(keyinfo, pos->ptr_to_rec),
				  share->blength, share->records)) != pos)
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

byte *hp_search_next(HP_INFO *info, HP_KEYDEF *keyinfo, const byte *key,
		      HASH_INFO *pos)
{
  DBUG_ENTER("hp_search_next");

  while ((pos= pos->next_key))
  {
    if (! hp_key_cmp(keyinfo, pos->ptr_to_rec, key))
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

ulong hp_mask(ulong hashnr, ulong buffmax, ulong maxlength)
{
  if ((hashnr & (buffmax-1)) < maxlength) return (hashnr & (buffmax-1));
  return (hashnr & ((buffmax >> 1) -1));
}


	/* Change link from pos to new_link */

void hp_movelink(HASH_INFO *pos, HASH_INFO *next_link, HASH_INFO *newlink)
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

ulong hp_hashnr(register HP_KEYDEF *keydef, register const byte *key)
{
  /*register*/ 
  ulong nr=1, nr2=4;
  HA_KEYSEG *seg,*endseg;

  for (seg=keydef->seg,endseg=seg+keydef->keysegs ; seg < endseg ; seg++)
  {
    uchar *pos=(uchar*) key;
    key+=seg->length;
    if (seg->null_bit)
    {
      key++;					/* Skipp null byte */
      if (*pos)					/* Found null */
      {
	nr^= (nr << 1) | 1;
	continue;
      }
      pos++;
    }
    if (seg->type == HA_KEYTYPE_TEXT)
    {
       seg->charset->coll->hash_sort(seg->charset,pos,((uchar*)key)-pos,&nr,&nr2);
    }
    else
    {
      for (; pos < (uchar*) key ; pos++)
      {
	nr^=(ulong) ((((uint) nr & 63)+nr2)*((uint) *pos)) + (nr << 8);
	nr2+=3;
      }
    }
  }
  return((ulong) nr);
}

	/* Calc hashvalue for a key in a record */

ulong hp_rec_hashnr(register HP_KEYDEF *keydef, register const byte *rec)
{
  /*register*/
  ulong nr=1, nr2=4;
  HA_KEYSEG *seg,*endseg;

  for (seg=keydef->seg,endseg=seg+keydef->keysegs ; seg < endseg ; seg++)
  {
    uchar *pos=(uchar*) rec+seg->start,*end=pos+seg->length;
    if (seg->null_bit)
    {
      if (rec[seg->null_pos] & seg->null_bit)
      {
	nr^= (nr << 1) | 1;
	continue;
      }
    }
    if (seg->type == HA_KEYTYPE_TEXT)
    {
      seg->charset->coll->hash_sort(seg->charset,pos,end-pos,&nr,&nr2);
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

ulong hp_hashnr(register HP_KEYDEF *keydef, register const byte *key)
{
  register ulong nr=0;
  HA_KEYSEG *seg,*endseg;

  for (seg=keydef->seg,endseg=seg+keydef->keysegs ; seg < endseg ; seg++)
  {
    uchar *pos=(uchar*) key;
    key+=seg->length;
    if (seg->null_bit)
    {
      key++;
      if (*pos)
      {
	nr^= (nr << 1) | 1;
	continue;
      }
      pos++;
    }
    if (seg->type == HA_KEYTYPE_TEXT)
    {
      seg->charset->hash_sort(seg->charset,pos,((uchar*)key)-pos,&nr,NULL);
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

ulong hp_rec_hashnr(register HP_KEYDEF *keydef, register const byte *rec)
{
  register ulong nr=0;
  HA_KEYSEG *seg,*endseg;

  for (seg=keydef->seg,endseg=seg+keydef->keysegs ; seg < endseg ; seg++)
  {
    uchar *pos=(uchar*) rec+seg->start,*end=pos+seg->length;
    if (seg->null_bit)
    {
      if (rec[seg->null_pos] & seg->null_bit)
      {
	nr^= (nr << 1) | 1;
	continue;
      }
    }
    if (seg->type == HA_KEYTYPE_TEXT)
    {
      seg->charset->hash_sort(seg->charset,pos,((uchar*)key)-pos,&nr,NULL);
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

int hp_rec_key_cmp(HP_KEYDEF *keydef, const byte *rec1, const byte *rec2)
{
  HA_KEYSEG *seg,*endseg;

  for (seg=keydef->seg,endseg=seg+keydef->keysegs ; seg < endseg ; seg++)
  {
    if (seg->null_bit)
    {
      if ((rec1[seg->null_pos] & seg->null_bit) !=
	  (rec2[seg->null_pos] & seg->null_bit))
	return 1;
      if (rec1[seg->null_pos] & seg->null_bit)
	continue;
    }
    if (seg->type == HA_KEYTYPE_TEXT)
    {
      if (seg->charset->coll->strnncollsp(seg->charset,
      					  (uchar*) rec1+seg->start,seg->length,
					  (uchar*) rec2+seg->start,seg->length))
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

	/* Compare a key in a record to a whole key */

int hp_key_cmp(HP_KEYDEF *keydef, const byte *rec, const byte *key)
{
  HA_KEYSEG *seg,*endseg;

  for (seg=keydef->seg,endseg=seg+keydef->keysegs ;
       seg < endseg ;
       key+= (seg++)->length)
  {
    if (seg->null_bit)
    {
      int found_null=test(rec[seg->null_pos] & seg->null_bit);
      if (found_null != (int) *key++)
	return 1;
      if (found_null)
	continue;
    }
    if (seg->type == HA_KEYTYPE_TEXT)
    {
      if (seg->charset->coll->strnncollsp(seg->charset,
					  (uchar*) rec+seg->start, seg->length,
					  (uchar*) key, seg->length))
	return 1;
    }
    else
    {
      if (bcmp(rec+seg->start,key,seg->length))
	return 1;
    }
  }
  return 0;
}


	/* Copy a key from a record to a keybuffer */

void hp_make_key(HP_KEYDEF *keydef, byte *key, const byte *rec)
{
  HA_KEYSEG *seg,*endseg;

  for (seg=keydef->seg,endseg=seg+keydef->keysegs ; seg < endseg ; seg++)
  {
    if (seg->null_bit)
      *key++= test(rec[seg->null_pos] & seg->null_bit);
    memcpy(key,rec+seg->start,(size_t) seg->length);
    key+=seg->length;
  }
}

uint hp_rb_make_key(HP_KEYDEF *keydef, byte *key, 
		    const byte *rec, byte *recpos)
{
  byte *start_key= key;
  HA_KEYSEG *seg, *endseg;

  for (seg= keydef->seg, endseg= seg + keydef->keysegs; seg < endseg; seg++)
  {
    if (seg->null_bit)
    {
      if (!(*key++= 1 - test(rec[seg->null_pos] & seg->null_bit)))
        continue;
    }
    if (seg->flag & HA_SWAP_KEY)
    {
      uint length= seg->length;
      byte *pos= (byte*) rec + seg->start;
      
#ifdef HAVE_ISNAN
      if (seg->type == HA_KEYTYPE_FLOAT)
      {
	float nr;
	float4get(nr, pos);
	if (isnan(nr))
	{
	  /* Replace NAN with zero */
 	  bzero(key, length);
	  key+= length;
	  continue;
	}
      }
      else if (seg->type == HA_KEYTYPE_DOUBLE)
      {
	double nr;
	float8get(nr, pos);
	if (isnan(nr))
	{
 	  bzero(key, length);
	  key+= length;
	  continue;
	}
      }
#endif
      pos+= length;
      while (length--)
      {
	*key++= *--pos;
      }
      continue;
    }
    memcpy(key, rec + seg->start, (size_t) seg->length);
    key+= seg->length;
  }
  memcpy(key, &recpos, sizeof(byte*));
  return key - start_key;
}

uint hp_rb_pack_key(HP_KEYDEF *keydef, uchar *key, const uchar *old, uint k_len)
{
  HA_KEYSEG *seg, *endseg;
  uchar *start_key= key;
  
  for (seg= keydef->seg, endseg= seg + keydef->keysegs;
       seg < endseg && (int) k_len > 0; old+= seg->length, seg++)
  {
    if (seg->null_bit)
    {
      k_len--;
      if (!(*key++= (char) 1 - *old++))
      {
        k_len-= seg->length;
        continue;
      }
    }
    if (seg->flag & HA_SWAP_KEY)
    {
      uint length= seg->length;
      byte *pos= (byte*) old + length;
      
      k_len-= length;
      while (length--)
      {
	*key++= *--pos;
      }
      continue;
    }
    memcpy((byte*) key, old, seg->length);
    key+= seg->length;
    k_len-= seg->length;
  }
  return key - start_key;
}

uint hp_rb_key_length(HP_KEYDEF *keydef, 
		      const byte *key __attribute__((unused)))
{
  return keydef->length;
}

uint hp_rb_null_key_length(HP_KEYDEF *keydef, const byte *key)
{
  const byte *start_key= key;
  HA_KEYSEG *seg, *endseg;
  
  for (seg= keydef->seg, endseg= seg + keydef->keysegs; seg < endseg; seg++)
  {
    if (seg->null_bit && !*key++)
      continue;
    key+= seg->length;
  }
  return key - start_key;
}
                  
/*
  Test if any of the key parts are NULL.
  Return:
    1 if any of the key parts was NULL
    0 otherwise
*/

my_bool hp_if_null_in_key(HP_KEYDEF *keydef, const byte *record)
{
  HA_KEYSEG *seg,*endseg;
  for (seg=keydef->seg,endseg=seg+keydef->keysegs ; seg < endseg ; seg++)
  {
    if (seg->null_bit && (record[seg->null_pos] & seg->null_bit))
      return 1;
  }
  return 0;
}

void heap_update_auto_increment(HP_INFO *info, const byte *record)
{
  ulonglong value;
  HA_KEYSEG *keyseg= info->s->keydef[info->s->auto_key - 1].seg;
  const uchar *key=  (uchar*) record + keyseg->start;

  switch (info->s->auto_key_type) {
  case HA_KEYTYPE_INT8:
  case HA_KEYTYPE_BINARY:
    value= (ulonglong) *(uchar*) key;
    break;
  case HA_KEYTYPE_SHORT_INT:
  case HA_KEYTYPE_USHORT_INT:
    value= (ulonglong) uint2korr(key);
    break;
  case HA_KEYTYPE_LONG_INT:
  case HA_KEYTYPE_ULONG_INT:
    value= (ulonglong) uint4korr(key);
    break;
  case HA_KEYTYPE_INT24:
  case HA_KEYTYPE_UINT24:
    value= (ulonglong) uint3korr(key);
    break;
  case HA_KEYTYPE_FLOAT:			/* This shouldn't be used */
  {
    float f_1;
    float4get(f_1, key);
    value= (ulonglong) f_1;
    break;
  }
  case HA_KEYTYPE_DOUBLE:			/* This shouldn't be used */
  {
    double f_1;
    float8get(f_1, key);
    value= (ulonglong) f_1;
    break;
  }
  case HA_KEYTYPE_LONGLONG:
  case HA_KEYTYPE_ULONGLONG:
    value= uint8korr(key);
    break;
  default:
    value= 0;					/* Error */
    break;
  }
  set_if_bigger(info->s->auto_increment, value);
}
