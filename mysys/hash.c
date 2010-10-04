/* Copyright 2000-2008 MySQL AB, 2008 Sun Microsystems, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

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

typedef struct st_hash_info {
  uint next;					/* index to next key */
  uchar *data;					/* data for current entry */
} HASH_LINK;

static uint my_hash_mask(my_hash_value_type hashnr,
                         size_t buffmax, size_t maxlength);
static void movelink(HASH_LINK *array,uint pos,uint next_link,uint newlink);
static int hashcmp(const HASH *hash, HASH_LINK *pos, const uchar *key,
                   size_t length);

static my_hash_value_type calc_hash(const HASH *hash,
                                    const uchar *key, size_t length)
{
  ulong nr1=1, nr2=4;
  hash->charset->coll->hash_sort(hash->charset,(uchar*) key,length,&nr1,&nr2);
  return (my_hash_value_type)nr1;
}

/**
  @brief Initialize the hash
  
  @details

  Initialize the hash, by defining and giving valid values for
  its elements. The failure to allocate memory for the
  hash->array element will not result in a fatal failure. The
  dynamic array that is part of the hash will allocate memory
  as required during insertion.

  @param[in,out] hash         The hash that is initialized
  @param[in]     charset      The charater set information
  @param[in]     size         The hash size
  @param[in]     key_offest   The key offset for the hash
  @param[in]     key_length   The length of the key used in
                              the hash
  @param[in]     get_key      get the key for the hash
  @param[in]     free_element pointer to the function that
                              does cleanup
  @return        inidicates success or failure of initialization
    @retval 0 success
    @retval 1 failure
*/
my_bool
_my_hash_init(HASH *hash, uint growth_size, CHARSET_INFO *charset,
              ulong size, size_t key_offset, size_t key_length,
              my_hash_get_key get_key,
              void (*free_element)(void*), uint flags)
{
  DBUG_ENTER("my_hash_init");
  DBUG_PRINT("enter",("hash: 0x%lx  size: %u", (long) hash, (uint) size));

  hash->records=0;
  hash->key_offset=key_offset;
  hash->key_length=key_length;
  hash->blength=1;
  hash->get_key=get_key;
  hash->free=free_element;
  hash->flags=flags;
  hash->charset=charset;
  DBUG_RETURN(my_init_dynamic_array_ci(&hash->array, 
                                       sizeof(HASH_LINK), size, growth_size));
}


/*
  Call hash->free on all elements in hash.

  SYNOPSIS
    my_hash_free_elements()
    hash   hash table

  NOTES:
    Sets records to 0
*/

static inline void my_hash_free_elements(HASH *hash)
{
  if (hash->free)
  {
    HASH_LINK *data=dynamic_element(&hash->array,0,HASH_LINK*);
    HASH_LINK *end= data + hash->records;
    while (data < end)
      (*hash->free)((data++)->data);
  }
  hash->records=0;
}


/*
  Free memory used by hash.

  SYNOPSIS
    my_hash_free()
    hash   the hash to delete elements of

  NOTES: Hash can't be reused without calling my_hash_init again.
*/

void my_hash_free(HASH *hash)
{
  DBUG_ENTER("my_hash_free");
  DBUG_PRINT("enter",("hash: 0x%lx", (long) hash));

  my_hash_free_elements(hash);
  hash->free= 0;
  delete_dynamic(&hash->array);
  hash->blength= 0;
  DBUG_VOID_RETURN;
}


/*
  Delete all elements from the hash (the hash itself is to be reused).

  SYNOPSIS
    my_hash_reset()
    hash   the hash to delete elements of
*/

void my_hash_reset(HASH *hash)
{
  DBUG_ENTER("my_hash_reset");
  DBUG_PRINT("enter",("hash: 0x%lxd", (long) hash));

  my_hash_free_elements(hash);
  reset_dynamic(&hash->array);
  /* Set row pointers so that the hash can be reused at once */
  hash->blength= 1;
  DBUG_VOID_RETURN;
}

/* some helper functions */

/*
  This function is char* instead of uchar* as HPUX11 compiler can't
  handle inline functions that are not defined as native types
*/

static inline char*
my_hash_key(const HASH *hash, const uchar *record, size_t *length,
            my_bool first)
{
  if (hash->get_key)
    return (char*) (*hash->get_key)(record,length,first);
  *length=hash->key_length;
  return (char*) record+hash->key_offset;
}

	/* Calculate pos according to keys */

static uint my_hash_mask(my_hash_value_type hashnr, size_t buffmax,
                         size_t maxlength)
{
  if ((hashnr & (buffmax-1)) < maxlength) return (hashnr & (buffmax-1));
  return (hashnr & ((buffmax >> 1) -1));
}

static uint my_hash_rec_mask(const HASH *hash, HASH_LINK *pos,
                             size_t buffmax, size_t maxlength)
{
  size_t length;
  uchar *key= (uchar*) my_hash_key(hash, pos->data, &length, 0);
  return my_hash_mask(calc_hash(hash, key, length), buffmax, maxlength);
}



/* for compilers which can not handle inline */
static
#if !defined(__USLC__) && !defined(__sgi)
inline
#endif
my_hash_value_type rec_hashnr(HASH *hash,const uchar *record)
{
  size_t length;
  uchar *key= (uchar*) my_hash_key(hash, record, &length, 0);
  return calc_hash(hash,key,length);
}


uchar* my_hash_search(const HASH *hash, const uchar *key, size_t length)
{
  HASH_SEARCH_STATE state;
  return my_hash_first(hash, key, length, &state);
}

uchar* my_hash_search_using_hash_value(const HASH *hash, 
                                       my_hash_value_type hash_value,
                                       const uchar *key,
                                       size_t length)
{
  HASH_SEARCH_STATE state;
  return my_hash_first_from_hash_value(hash, hash_value,
                                       key, length, &state);
}

my_hash_value_type my_calc_hash(const HASH *hash,
                                const uchar *key, size_t length)
{
  return calc_hash(hash, key, length ? length : hash->key_length);
}


/*
  Search after a record based on a key

  NOTE
   Assigns the number of the found record to HASH_SEARCH_STATE state
*/

uchar* my_hash_first(const HASH *hash, const uchar *key, size_t length,
                     HASH_SEARCH_STATE *current_record)
{
  uchar *res;
  if (my_hash_inited(hash))
    res= my_hash_first_from_hash_value(hash,
                   calc_hash(hash, key, length ? length : hash->key_length),
                   key, length, current_record);
  else
    res= 0;
  return res;
}


uchar* my_hash_first_from_hash_value(const HASH *hash,
                                     my_hash_value_type hash_value,
                                     const uchar *key,
                                     size_t length,
                                     HASH_SEARCH_STATE *current_record)
{
  HASH_LINK *pos;
  uint flag,idx;
  DBUG_ENTER("my_hash_first_from_hash_value");

  flag=1;
  if (hash->records)
  {
    idx= my_hash_mask(hash_value,
                      hash->blength, hash->records);
    do
    {
      pos= dynamic_element(&hash->array,idx,HASH_LINK*);
      if (!hashcmp(hash,pos,key,length))
      {
	DBUG_PRINT("exit",("found key at %d",idx));
	*current_record= idx;
	DBUG_RETURN (pos->data);
      }
      if (flag)
      {
	flag=0;					/* Reset flag */
	if (my_hash_rec_mask(hash, pos, hash->blength, hash->records) != idx)
	  break;				/* Wrong link */
      }
    }
    while ((idx=pos->next) != NO_RECORD);
  }
  *current_record= NO_RECORD;
  DBUG_RETURN(0);
}

	/* Get next record with identical key */
	/* Can only be called if previous calls was my_hash_search */

uchar* my_hash_next(const HASH *hash, const uchar *key, size_t length,
                    HASH_SEARCH_STATE *current_record)
{
  HASH_LINK *pos;
  uint idx;

  if (*current_record != NO_RECORD)
  {
    HASH_LINK *data=dynamic_element(&hash->array,0,HASH_LINK*);
    for (idx=data[*current_record].next; idx != NO_RECORD ; idx=pos->next)
    {
      pos=data+idx;
      if (!hashcmp(hash,pos,key,length))
      {
	*current_record= idx;
	return pos->data;
      }
    }
    *current_record= NO_RECORD;
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

/*
  Compare a key in a record to a whole key. Return 0 if identical

  SYNOPSIS
    hashcmp()
    hash   hash table
    pos    position of hash record to use in comparison
    key    key for comparison
    length length of key

  NOTES:
    If length is 0, comparison is done using the length of the
    record being compared against.

  RETURN
    = 0  key of record == key
    != 0 key of record != key
 */

static int hashcmp(const HASH *hash, HASH_LINK *pos, const uchar *key,
                   size_t length)
{
  size_t rec_keylength;
  uchar *rec_key= (uchar*) my_hash_key(hash, pos->data, &rec_keylength, 1);
  return ((length && length != rec_keylength) ||
	  my_strnncoll(hash->charset, (uchar*) rec_key, rec_keylength,
		       (uchar*) key, rec_keylength));
}


	/* Write a hash-key to the hash-index */

my_bool my_hash_insert(HASH *info, const uchar *record)
{
  int flag;
  size_t idx,halfbuff,first_index;
  my_hash_value_type hash_nr;
  uchar *UNINIT_VAR(ptr_to_rec),*UNINIT_VAR(ptr_to_rec2);
  HASH_LINK *data,*empty,*UNINIT_VAR(gpos),*UNINIT_VAR(gpos2),*pos;

  if (HASH_UNIQUE & info->flags)
  {
    uchar *key= (uchar*) my_hash_key(info, record, &idx, 1);
    if (my_hash_search(info, key, idx))
      return(TRUE);				/* Duplicate entry */
  }

  flag=0;
  if (!(empty=(HASH_LINK*) alloc_dynamic(&info->array)))
    return(TRUE);				/* No more memory */

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
	if (my_hash_mask(hash_nr, info->blength, info->records) != first_index)
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
	    gpos->next= (uint) (pos-data);
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

  idx= my_hash_mask(rec_hashnr(info, record), info->blength, info->records + 1);
  pos=data+idx;
  if (pos == empty)
  {
    pos->data=(uchar*) record;
    pos->next=NO_RECORD;
  }
  else
  {
    /* Check if more records in same hash-nr family */
    empty[0]=pos[0];
    gpos= data + my_hash_rec_mask(info, pos, info->blength, info->records + 1);
    if (pos == gpos)
    {
      pos->data=(uchar*) record;
      pos->next=(uint) (empty - data);
    }
    else
    {
      pos->data=(uchar*) record;
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

my_bool my_hash_delete(HASH *hash, uchar *record)
{
  uint blength,pos2,idx,empty_index;
  my_hash_value_type pos_hashnr, lastpos_hashnr;
  HASH_LINK *data,*lastpos,*gpos,*pos,*pos3,*empty;
  DBUG_ENTER("my_hash_delete");
  if (!hash->records)
    DBUG_RETURN(1);

  blength=hash->blength;
  data=dynamic_element(&hash->array,0,HASH_LINK*);
  /* Search after record with key */
  pos= data + my_hash_mask(rec_hashnr(hash, record), blength, hash->records);
  gpos = 0;

  while (pos->data != record)
  {
    gpos=pos;
    if (pos->next == NO_RECORD)
      DBUG_RETURN(1);			/* Key not found */
    pos=data+pos->next;
  }

  if ( --(hash->records) < hash->blength >> 1) hash->blength>>=1;
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
  pos= data + my_hash_mask(lastpos_hashnr, hash->blength, hash->records);
  if (pos == empty)			/* Move to empty position. */
  {
    empty[0]=lastpos[0];
    goto exit;
  }
  pos_hashnr=rec_hashnr(hash,pos->data);
  /* pos3 is where the pos should be */
  pos3= data + my_hash_mask(pos_hashnr, hash->blength, hash->records);
  if (pos != pos3)
  {					/* pos is on wrong posit */
    empty[0]=pos[0];			/* Save it here */
    pos[0]=lastpos[0];			/* This should be here */
    movelink(data,(uint) (pos-data),(uint) (pos3-data),empty_index);
    goto exit;
  }
  pos2= my_hash_mask(lastpos_hashnr, blength, hash->records + 1);
  if (pos2 == my_hash_mask(pos_hashnr, blength, hash->records + 1))
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
  (void) pop_dynamic(&hash->array);
  if (hash->free)
    (*hash->free)((uchar*) record);
  DBUG_RETURN(0);
}

	/*
	  Update keys when record has changed.
	  This is much more efficent than using a delete & insert.
	  */

my_bool my_hash_update(HASH *hash, uchar *record, uchar *old_key,
                       size_t old_key_length)
{
  uint new_index,new_pos_index,blength,records;
  size_t idx,empty;
  HASH_LINK org_link,*data,*previous,*pos;
  DBUG_ENTER("my_hash_update");
  
  if (HASH_UNIQUE & hash->flags)
  {
    HASH_SEARCH_STATE state;
    uchar *found, *new_key= (uchar*) my_hash_key(hash, record, &idx, 1);
    if ((found= my_hash_first(hash, new_key, idx, &state)))
    {
      do 
      {
        if (found != record)
          DBUG_RETURN(1);		/* Duplicate entry */
      } 
      while ((found= my_hash_next(hash, new_key, idx, &state)));
    }
  }

  data=dynamic_element(&hash->array,0,HASH_LINK*);
  blength=hash->blength; records=hash->records;

  /* Search after record with key */

  idx= my_hash_mask(calc_hash(hash, old_key, (old_key_length ?
                                              old_key_length :
                                              hash->key_length)),
                    blength, records);
  new_index= my_hash_mask(rec_hashnr(hash, record), blength, records);
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
  if (new_index == empty)
  {
    /*
      At this point record is unlinked from the old chain, thus it holds
      random position. By the chance this position is equal to position
      for the first element in the new chain. That means updated record
      is the only record in the new chain.
    */
    if (empty != idx)
    {
      /*
        Record was moved while unlinking it from the old chain.
        Copy data to a new position.
      */
      data[empty]= org_link;
    }
    data[empty].next= NO_RECORD;
    DBUG_RETURN(0);
  }
  pos=data+new_index;
  new_pos_index= my_hash_rec_mask(hash, pos, blength, records);
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


uchar *my_hash_element(HASH *hash, ulong idx)
{
  if (idx < hash->records)
    return dynamic_element(&hash->array,idx,HASH_LINK*)->data;
  return 0;
}


/*
  Replace old row with new row.  This should only be used when key
  isn't changed
*/

void my_hash_replace(HASH *hash, HASH_SEARCH_STATE *current_record,
                     uchar *new_row)
{
  if (*current_record != NO_RECORD)            /* Safety */
    dynamic_element(&hash->array, *current_record, HASH_LINK*)->data= new_row;
}


#ifndef DBUG_OFF

my_bool my_hash_check(HASH *hash)
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
    if (my_hash_rec_mask(hash, data + i, blength, records) == i)
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
	if ((rec_link= my_hash_rec_mask(hash, hash_info,
                                        blength, records)) != i)
	{
          DBUG_PRINT("error", ("Record in wrong link at %d: Start %d  "
                               "Record: 0x%lx  Record-link %d",
                               idx, i, (long) hash_info->data, rec_link));
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
    DBUG_PRINT("error",("Found %u of %u records", found, records));
    error=1;
  }
  if (records)
    DBUG_PRINT("info",
	       ("records: %u   seeks: %d   max links: %d   hitrate: %.2f",
		records,seek,max_links,(float) seek / (float) records));
  return error;
}
#endif
