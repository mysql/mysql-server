/* Copyright (c) 2008, 2010, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software Foundation,
  51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

/**
  @file storage/perfschema/pfs_digest.h
  Statement Digest data structures (implementation).
*/

#include "pfs_digest.h"
#include "my_sys.h"
#include "pfs_global.h"
#include <string.h>

unsigned int statements_digest_size= 0;
/** EVENTS_STATEMENTS_HISTORY_LONG circular buffer. */
PFS_statements_digest_stat *statements_digest_stat_array= NULL;
/** Consumer flag for table EVENTS_STATEMENTS_SUMMARY_BY_DIGEST. */
bool flag_statements_digest= true;
/** 
  Current index in Stat array where new record is to be inserted.
  index 0 is reserved for "all else" case when entire array is full.
*/
int digest_index= 1;

static LF_HASH digest_hash;
static bool digest_hash_inited= false;

/**
  Initialize table EVENTS_STATEMENTS_SUMMARY_BY_DIGEST.
  @param digest_sizing      
*/
int init_digest(unsigned int statements_digest_sizing)
{
  /* 
    TBD. Allocate memory for statements_digest_stat_array based on 
    performance_schema_digests_size values
  */
  statements_digest_size= statements_digest_sizing;
 
  if (statements_digest_size == 0)
    return 0;

  statements_digest_stat_array=
    PFS_MALLOC_ARRAY(statements_digest_size, PFS_statements_digest_stat,
                     MYF(MY_ZEROFILL));

  return (statements_digest_stat_array ? 0 : 1);
}

/** Cleanup table EVENTS_STATEMENTS_SUMMARY_BY_DIGEST. */
void cleanup_digest(void)
{
  /* 
    TBD. Free memory allocated to statements_digest_stat_array. 
  */
  pfs_free(statements_digest_stat_array);
  statements_digest_stat_array= NULL;
}

C_MODE_START
static uchar *digest_hash_get_key(const uchar *entry, size_t *length,
                                my_bool)
{
  const PFS_statements_digest_stat * const *typed_entry;
  const PFS_statements_digest_stat *digest;
  const void *result;
  typed_entry= reinterpret_cast<const PFS_statements_digest_stat*const*>(entry);
  DBUG_ASSERT(typed_entry != NULL);
  digest= *typed_entry;
  DBUG_ASSERT(digest != NULL);
  *length= 16; 
  result= digest->m_md5_hash.m_md5;
  return const_cast<uchar*> (reinterpret_cast<const uchar*> (result));
}
C_MODE_END


/**
  Initialize the digest hash.
  @return 0 on success
*/
int init_digest_hash(void)
{
  if (! digest_hash_inited)
  {
    lf_hash_init(&digest_hash, sizeof(PFS_statements_digest_stat*),
                 LF_HASH_UNIQUE, 0, 0, digest_hash_get_key,
                 &my_charset_bin);
    digest_hash_inited= true;
  }
  return 0;
}

/** Cleanup the digest hash. */
void cleanup_digest_hash(void)
{
  if (digest_hash_inited)
  {
    lf_hash_destroy(&digest_hash);
    digest_hash_inited= false;
  }
}

static LF_PINS* get_digest_hash_pins(PFS_thread *thread)
{
  if (unlikely(thread->m_digest_hash_pins == NULL))
  {
    if (!digest_hash_inited)
      return NULL;
    thread->m_digest_hash_pins= lf_hash_get_pins(&digest_hash);
  }
  return thread->m_digest_hash_pins;
}

PFS_statements_digest_stat* 
search_insert_statement_digest(PFS_thread* thread,
                               unsigned char* hash_key,
                               char* digest_text,
                               unsigned int digest_text_length)
{
  /* get digest pin. */
  LF_PINS *pins= get_digest_hash_pins(thread);
  if (unlikely(pins == NULL))
  {
    return NULL;
  }
 
  PFS_statements_digest_stat **entry;
  PFS_statements_digest_stat *pfs;

  /* Lookup LF_HASH using this new key. */
  entry= reinterpret_cast<PFS_statements_digest_stat**>
    (lf_hash_search(&digest_hash, pins,
                    hash_key, 16));

  if(!entry)
  {
    /* 
      If statement digest entry doesn't exist.
    */
    //printf("\n Doesn't Exist. Adding new entry. \n");

    if(digest_index==0)
    {
      /*
        digest_stat array is full. Add stat at index 0 and return.
      */
      pfs= &statements_digest_stat_array[0];
      //TODO
      return pfs;
    }

    /* 
      Add a new record in digest stat array. 
    */
    pfs= &statements_digest_stat_array[digest_index];
    
    /* Set digest text. */
    memcpy(pfs->m_digest_text, digest_text, digest_text_length);
    pfs->m_digest_text_length= digest_text_length;

    /* Set digest hash/LF Hash search key. */
    memcpy(pfs->m_md5_hash.m_md5, hash_key, 16);
    
    /* Increment index. */
    digest_index++;
    
    if(digest_index%statements_digest_size == 0)
    {
      /* 
        Digest stat array is full. Now all stat for all further 
        entries will go into index 0.
      */
      digest_index= 0;
    }
    
    /* Add this new digest into LF_HASH */
    int res;
    res= lf_hash_insert(&digest_hash, pins, &pfs);
    lf_hash_search_unpin(pins);
    if (res > 0)
    {
      /* ERROR CONDITION */
      return NULL;
    }
    return pfs;
  }
  else if (entry && (entry != MY_ERRPTR))
  {
    /* 
      If stmt digest already exists, update stat and return 
    */
    //printf("\n Already Exists \n");
    pfs= *entry;
    lf_hash_search_unpin(pins);
    return pfs;
  }

  return NULL;
}
 
void reset_esms_by_digest()
{
  /*TBD*/ 
}

