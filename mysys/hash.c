/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA */

/* The hash functions used for saveing keys */
/* One of key_length or key_length_offset must be given */
/* Key length of 0 isn't allowed */

#include "mysys_priv.h"
#include <m_string.h>
#include <m_ctype.h>
#include "hash.h"

#define NO_RECORD	((uint) -1)
#define LOWFIND 1
#define LOWUSED 2
#define HIGHFIND 4
#define HIGHUSED 8

static uint hash_mask(uint hashnr,uint buffmax,uint maxlength);
static void movelink(HASH_LINK *array,uint pos,uint next_link,uint newlink);
static uint calc_hashnr(const byte *key,uint length);
static uint calc_hashnr_caseup(const byte *key,uint length);
static int hashcmp(HASH *hash,HASH_LINK *pos,const byte *key,uint length);


my_bool hash_init(HASH *hash,uint size,uint key_offset,uint key_length,
		  hash_get_key get_key,
		  void (*free_element)(void*),uint flags)
{
  DBUG_ENTER("hash_init");
  DBUG_PRINT("enter",("hash: %lx  size: %d",hash,size));

  hash->records=0;
  if (init_dynamic_array(&hash->array,sizeof(HASH_LINK),size,0))
  {
    hash->free=0;				/* Allow call to hash_free */
    DBUG_RETURN(TRUE);
  }
  hash->key_offset=key_offset;
  hash->key_length=key_length;
  hash->blength=1;
  hash->current_record= NO_RECORD;		/* For the future */
  hash->get_key=get_key;
  hash->free=free_element;
  hash->flags=flags;
  if (flags & HASH_CASE_INSENSITIVE)
    hash->calc_hashnr=calc_hashnr_caseup;
  else
    hash->calc_hashnr=calc_hashnr;
  DBUG_RETURN(0);
}


void hash_free(HASH *hash)
{
  DBUG_ENTER("hash_free");
  if (hash->free)
  {
    uint i,records;
    HASH_LINK *data=dynamic_element(&hash->array,0,HASH_LINK*);
    for (i=0,records=hash->records ; i < records ; i++)
      (*hash->free)(data[i].data);
    hash->free=0;
  }
  delete_dynamic(&hash->array);
  hash->records=0;
  DBUG_VOID_RETURN;
}

	/* some helper functions */

inline byte*
hash_key(HASH *hash,const byte *record,uint *length,my_bool first)
{
  if (hash->get_key)
    return (*hash->get_key)(record,length,first);
  *length=hash->key_length;
  return (byte*) record+hash->key_offset;
}

	/* Calculate pos according to keys */

static uint hash_mask(uint hashnr,uint buffmax,uint maxlength)
{
  if ((hashnr & (buffmax-1)) < maxlength) return (hashnr & (buffmax-1));
  return (hashnr & ((buffmax >> 1) -1));
}

static uint hash_rec_mask(HASH *hash,HASH_LINK *pos,uint buffmax,
			  uint maxlength)
{
  uint length;
  byte *key=hash_key(hash,pos->data,&length,0);
  return hash_mask((*hash->calc_hashnr)(key,length),buffmax,maxlength);
}

#ifndef NEW_HASH_FUNCTION

	/* Calc hashvalue for a key */

static uint calc_hashnr(const byte *key,uint length)
{
  register uint nr=1, nr2=4;
  while (length--)
  {
    nr^= (((nr & 63)+nr2)*((uint) (uchar) *key++))+ (nr << 8);
    nr2+=3;
  }
  return((uint) nr);
}

	/* Calc hashvalue for a key, case indepenently */

static uint calc_hashnr_caseup(const byte *key,uint length)
{
  register uint nr=1, nr2=4;
  while (length--)
  {
    nr^= (((nr & 63)+nr2)*((uint) (uchar) toupper(*key++)))+ (nr << 8);
    nr2+=3;
  }
  return((uint) nr);
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
 * This works well on both numbers and strings.
 */

uint calc_hashnr(const byte *key, uint len)
{
  const byte *end=key+len;
  uint hash;
  for (hash = 0; key < end; key++)
  {
    hash *= 16777619;
    hash ^= (uint) *(uchar*) key;
  }
  return (hash);
}

uint calc_hashnr_caseup(const byte *key, uint len)
{
  const byte *end=key+len;
  uint hash;
  for (hash = 0; key < end; key++)
  {
    hash *= 16777619;
    hash ^= (uint) (uchar) toupper(*key);
  }
  return (hash);
}

#endif


inline uint rec_hashnr(HASH *hash,const byte *record)
{
  uint length;
  byte *key=hash_key(hash,record,&length,0);
  return (*hash->calc_hashnr)(key,length);
}


	/* Search after a record based on a key */
	/* Sets info->current_ptr to found record */

gptr hash_search(HASH *hash,const byte *key,uint length)
{
  HASH_LINK *pos;
  uint flag,idx;
  DBUG_ENTER("hash_search");

  flag=1;
  if (hash->records)
  {
    idx=hash_mask((*hash->calc_hashnr)(key,length ? length :
					 hash->key_length),
		    hash->blength,hash->records);
    do
    {
      pos= dynamic_element(&hash->array,idx,HASH_LINK*);
      if (!hashcmp(hash,pos,key,length))
      {
	DBUG_PRINT("exit",("found key at %d",idx));
	hash->current_record= idx;
	DBUG_RETURN (pos->data);
      }
      if (flag)
      {
	flag=0;					/* Reset flag */
	if (hash_rec_mask(hash,pos,hash->blength,hash->records) != idx)
	  break;				/* Wrong link */
      }
    }
    while ((idx=pos->next) != NO_RECORD);
  }
  hash->current_record= NO_RECORD;
  DBUG_RETURN(0);
}

	/* Get next record with identical key */
	/* Can only be called if previous calls was hash_search */

gptr hash_next(HASH *hash,const byte *key,uint length)
{
  HASH_LINK *pos;
  uint idx;

  if (hash->current_record != NO_RECORD)
  {
    HASH_LINK *data=dynamic_element(&hash->array,0,HASH_LINK*);
    for (idx=data[hash->current_record].next; idx != NO_RECORD ; idx=pos->next)
    {
      pos=data+idx;
      if (!hashcmp(hash,pos,key,length))
      {
	hash->current_record= idx;
	return pos->data;
      }
    }
    hash->current_record=NO_RECORD;
  }
  return 0;
}


	/* Change link from pos to new_link */

static void movelink(HASH_LINK *array,uint find,uint next_link,uint newlink)
{
  HASH_LINK *old_link;
  do
  {
    old_link=array+next_link;
  }
  while ((next_link=old_link->next) != find);
  old_link->next= newlink;
  return;
}

	/* Compare a key in a record to a whole key. Return 0 if identical */

static int hashcmp(HASH *hash,HASH_LINK *pos,const byte *key,uint length)
{
  uint rec_keylength;
  byte *rec_key=hash_key(hash,pos->data,&rec_keylength,1);
  return (length && length != rec_keylength) ||
    (hash->flags & HASH_CASE_INSENSITIVE ?
     my_casecmp(rec_key,key,rec_keylength) :
     memcmp(rec_key,key,rec_keylength));
}


	/* Write a hash-key to the hash-index */

my_bool hash_insert(HASH *info,const byte *record)
{
  int flag;
  uint halfbuff,hash_nr,first_index,idx;
  byte *ptr_to_rec,*ptr_to_rec2;
  HASH_LINK *data,*empty,*gpos,*gpos2,*pos;

  LINT_INIT(gpos); LINT_INIT(gpos2);
  LINT_INIT(ptr_to_rec); LINT_INIT(ptr_to_rec2);

  flag=0;
  if (!(empty=(HASH_LINK*) alloc_dynamic(&info->array)))
    return(TRUE);				/* No more memory */

  info->current_record= NO_RECORD;
  data=dynamic_element(&info->array,0,HASH_LINK*);
  halfbuff= info->blength >> 1;

  idx=first_index=info->records-halfbuff;
  if (idx != info->records)				/* If some records */
  {
    do
    {
      pos=data+idx;
      hash_nr=rec_hashnr(info,pos->data);
      if (flag == 0)				/* First loop; Check if ok */
	if (hash_mask(hash_nr,info->blength,info->records) != first_index)
	  break;
      if (!(hash_nr & halfbuff))
      {						/* Key will not move */
	if (!(flag & LOWFIND))
	{
	  if (flag & HIGHFIND)
	  {
	    flag=LOWFIND | HIGHFIND;
	    /* key shall be moved to the current empty position */
	    gpos=empty;
	    ptr_to_rec=pos->data;
	    empty=pos;				/* This place is now free */
	  }
	  else
	  {
	    flag=LOWFIND | LOWUSED;		/* key isn't changed */
	    gpos=pos;
	    ptr_to_rec=pos->data;
	  }
	}
	else
	{
	  if (!(flag & LOWUSED))
	  {
	    /* Change link of previous LOW-key */
	    gpos->data=ptr_to_rec;
	    gpos->next=(uint) (pos-data);
	    flag= (flag & HIGHFIND) | (LOWFIND | LOWUSED);
	  }
	  gpos=pos;
	  ptr_to_rec=pos->data;
	}
      }
      else
      {						/* key will be moved */
	if (!(flag & HIGHFIND))
	{
	  flag= (flag & LOWFIND) | HIGHFIND;
	  /* key shall be moved to the last (empty) position */
	  gpos2 = empty; empty=pos;
	  ptr_to_rec2=pos->data;
	}
	else
	{
	  if (!(flag & HIGHUSED))
	  {
	    /* Change link of previous hash-key and save */
	    gpos2->data=ptr_to_rec2;
	    gpos2->next=(uint) (pos-data);
	    flag= (flag & LOWFIND) | (HIGHFIND | HIGHUSED);
	  }
	  gpos2=pos;
	  ptr_to_rec2=pos->data;
	}
      }
    }
    while ((idx=pos->next) != NO_RECORD);

    if ((flag & (LOWFIND | LOWUSED)) == LOWFIND)
    {
      gpos->data=ptr_to_rec;
      gpos->next=NO_RECORD;
    }
    if ((flag & (HIGHFIND | HIGHUSED)) == HIGHFIND)
    {
      gpos2->data=ptr_to_rec2;
      gpos2->next=NO_RECORD;
    }
  }
  /* Check if we are at the empty position */

  idx=hash_mask(rec_hashnr(info,record),info->blength,info->records+1);
  pos=data+idx;
  if (pos == empty)
  {
    pos->data=(byte*) record;
    pos->next=NO_RECORD;
  }
  else
  {
    /* Check if more records in same hash-nr family */
    empty[0]=pos[0];
    gpos=data+hash_rec_mask(info,pos,info->blength,info->records+1);
    if (pos == gpos)
    {
      pos->data=(byte*) record;
      pos->next=(uint) (empty - data);
    }
    else
    {
      pos->data=(byte*) record;
      pos->next=NO_RECORD;
      movelink(data,(uint) (pos-data),(uint) (gpos-data),(uint) (empty-data));
    }
  }
  if (++info->records == info->blength)
    info->blength+= info->blength;
  return(0);
}


/******************************************************************************
** Remove one record from hash-table. The record with the same record
** ptr is removed.
** if there is a free-function it's called for record if found
******************************************************************************/

my_bool hash_delete(HASH *hash,byte *record)
{
  uint blength,pos2,pos_hashnr,lastpos_hashnr,idx,empty_index;
  HASH_LINK *data,*lastpos,*gpos,*pos,*pos3,*empty;
  DBUG_ENTER("hash_delete");
  if (!hash->records)
    DBUG_RETURN(1);

  blength=hash->blength;
  data=dynamic_element(&hash->array,0,HASH_LINK*);
  /* Search after record with key */
  pos=data+ hash_mask(rec_hashnr(hash,record),blength,hash->records);
  gpos = 0;

  while (pos->data != record)
  {
    gpos=pos;
    if (pos->next == NO_RECORD)
      DBUG_RETURN(1);			/* Key not found */
    pos=data+pos->next;
  }

  if ( --(hash->records) < hash->blength >> 1) hash->blength>>=1;
  hash->current_record= NO_RECORD;
  lastpos=data+hash->records;

  /* Remove link to record */
  empty=pos; empty_index=(uint) (empty-data);
  if (gpos)
    gpos->next=pos->next;		/* unlink current ptr */
  else if (pos->next != NO_RECORD)
  {
    empty=data+(empty_index=pos->next);
    pos->data=empty->data;
    pos->next=empty->next;
  }

  if (empty == lastpos)			/* last key at wrong pos or no next link */
    goto exit;

  /* Move the last key (lastpos) */
  lastpos_hashnr=rec_hashnr(hash,lastpos->data);
  /* pos is where lastpos should be */
  pos=data+hash_mask(lastpos_hashnr,hash->blength,hash->records);
  if (pos == empty)			/* Move to empty position. */
  {
    empty[0]=lastpos[0];
    goto exit;
  }
  pos_hashnr=rec_hashnr(hash,pos->data);
  /* pos3 is where the pos should be */
  pos3= data+hash_mask(pos_hashnr,hash->blength,hash->records);
  if (pos != pos3)
  {					/* pos is on wrong posit */
    empty[0]=pos[0];			/* Save it here */
    pos[0]=lastpos[0];			/* This should be here */
    movelink(data,(uint) (pos-data),(uint) (pos3-data),empty_index);
    goto exit;
  }
  pos2= hash_mask(lastpos_hashnr,blength,hash->records+1);
  if (pos2 == hash_mask(pos_hashnr,blength,hash->records+1))
  {					/* Identical key-positions */
    if (pos2 != hash->records)
    {
      empty[0]=lastpos[0];
      movelink(data,(uint) (lastpos-data),(uint) (pos-data),empty_index);
      goto exit;
    }
    idx= (uint) (pos-data);		/* Link pos->next after lastpos */
  }
  else idx= NO_RECORD;		/* Different positions merge */

  empty[0]=lastpos[0];
  movelink(data,idx,empty_index,pos->next);
  pos->next=empty_index;

exit:
  VOID(pop_dynamic(&hash->array));
  if (hash->free)
    (*hash->free)((byte*) record);
  DBUG_RETURN(0);
}

	/*
	  Update keys when record has changed.
	  This is much more efficent than using a delete & insert.
	  */

my_bool hash_update(HASH *hash,byte *record,byte *old_key,uint old_key_length)
{
  uint idx,new_index,new_pos_index,blength,records,empty;
  HASH_LINK org_link,*data,*previous,*pos;
  DBUG_ENTER("hash_update");

  data=dynamic_element(&hash->array,0,HASH_LINK*);
  blength=hash->blength; records=hash->records;

  /* Search after record with key */

  idx=hash_mask((*hash->calc_hashnr)(old_key,(old_key_length ?
					      old_key_length :
					      hash->key_length)),
		  blength,records);
  new_index=hash_mask(rec_hashnr(hash,record),blength,records);
  if (idx == new_index)
    DBUG_RETURN(0);			/* Nothing to do (No record check) */
  previous=0;
  for (;;)
  {

    if ((pos= data+idx)->data == record)
      break;
    previous=pos;
    if ((idx=pos->next) == NO_RECORD)
      DBUG_RETURN(1);			/* Not found in links */
  }
  hash->current_record= NO_RECORD;
  org_link= *pos;
  empty=idx;

  /* Relink record from current chain */

  if (!previous)
  {
    if (pos->next != NO_RECORD)
    {
      empty=pos->next;
      *pos= data[pos->next];
    }
  }
  else
    previous->next=pos->next;		/* unlink pos */

  /* Move data to correct position */
  pos=data+new_index;
  new_pos_index=hash_rec_mask(hash,pos,blength,records);
  if (new_index != new_pos_index)
  {					/* Other record in wrong position */
    data[empty] = *pos;
    movelink(data,new_index,new_pos_index,empty);
    org_link.next=NO_RECORD;
    data[new_index]= org_link;
  }
  else
  {					/* Link in chain at right position */
    org_link.next=data[new_index].next;
    data[empty]=org_link;
    data[new_index].next=empty;
  }
  DBUG_RETURN(0);
}


byte *hash_element(HASH *hash,uint idx)
{
  if (idx < hash->records)
    return dynamic_element(&hash->array,idx,HASH_LINK*)->data;
  return 0;
}


/*
  Replace old row with new row.  This should only be used when key
  isn't changed
*/

void hash_replace(HASH *hash, uint idx, byte *new_row)
{
  dynamic_element(&hash->array,idx,HASH_LINK*)->data=new_row;
}


#ifndef DBUG_OFF

my_bool hash_check(HASH *hash)
{
  int error;
  uint i,rec_link,found,max_links,seek,links,idx;
  uint records,blength;
  HASH_LINK *data,*hash_info;

  records=hash->records; blength=hash->blength;
  data=dynamic_element(&hash->array,0,HASH_LINK*);
  error=0;

  for (i=found=max_links=seek=0 ; i < records ; i++)
  {
    if (hash_rec_mask(hash,data+i,blength,records) == i)
    {
      found++; seek++; links=1;
      for (idx=data[i].next ;
	   idx != NO_RECORD && found < records + 1;
	   idx=hash_info->next)
      {
	if (idx >= records)
	{
	  DBUG_PRINT("error",
		     ("Found pointer outside array to %d from link starting at %d",
		      idx,i));
	  error=1;
	}
	hash_info=data+idx;
	seek+= ++links;
	if ((rec_link=hash_rec_mask(hash,hash_info,blength,records)) != i)
	{
	  DBUG_PRINT("error",
		     ("Record in wrong link at %d: Start %d  Record: %lx  Record-link %d", idx,i,hash_info->data,rec_link));
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
  if (records)
    DBUG_PRINT("info",
	       ("records: %ld   seeks: %d   max links: %d   hitrate: %.2f",
		records,seek,max_links,(float) seek / (float) records));
  return error;
}
#endif
