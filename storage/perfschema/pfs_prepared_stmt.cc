/* Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

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
  @file storage/perfschema/pfs_prepared_stmt.cc
  Prepared Statement data structures (implementation).
*/

/*
  This code needs extra visibility in the lexer structures
*/

#include "my_global.h"
#include "my_sys.h"
#include "pfs_instr.h"
#include "pfs_prepared_stmt.h"
#include "pfs_global.h"
#include "sql_string.h"
#include <string.h>

/** PREPARED_STATEMENTS_INSTANCE. */
PFS_prepared_stmt *prepared_stmt_array= NULL;

/** Max size of the prepared stmt array. */
ulong prepared_stmt_max= 0;
/** Number of prepared statement instances lost. */
ulong prepared_stmt_lost= 0;
/** True when prepared stmt array is full. */
bool prepared_stmt_full;

LF_HASH prepared_stmt_hash;                                                           
static bool prepared_stmt_hash_inited= false;

/**
  Initialize table PREPARED_STATEMENTS_INSTANCE.
  @param param performance schema sizing
*/
int init_prepared_stmt(const PFS_global_param *param)
{
  /*
    Allocate memory for prepared_stmt_array based on
    performance_schema_max_prepared_stmt_instances value.
  */
  prepared_stmt_max= param->m_prepared_stmt_sizing;
  prepared_stmt_lost= 0;
  prepared_stmt_full= false;

  if (prepared_stmt_max == 0)
    return 0;

  prepared_stmt_array=
    PFS_MALLOC_ARRAY(prepared_stmt_max, PFS_prepared_stmt,
                     MYF(MY_ZEROFILL));
  if (unlikely(prepared_stmt_array == NULL))
    return 1;

  PFS_prepared_stmt *pfs= prepared_stmt_array;
  PFS_prepared_stmt *pfs_last= prepared_stmt_array + prepared_stmt_max;
  
  for (; pfs < pfs_last ; pfs++)
  {
    pfs->reset_data();
  }

  return 0;
}

/** Cleanup table PREPARED_STATEMENTS_INSTANCE. */
void cleanup_prepared_stmt(void)
{
  /*  Free memory allocated to prepared_stmt_array. */
  pfs_free(prepared_stmt_array);
  prepared_stmt_array= NULL;
}

C_MODE_START
static uchar *prepared_stmt_hash_get_key(const uchar *entry, size_t *length,
                                         my_bool)
{
  const PFS_prepared_stmt * const *typed_entry;
  const PFS_prepared_stmt *prepared_stmt;
  const void *result;
  typed_entry= reinterpret_cast<const PFS_prepared_stmt* const *> (entry);
  DBUG_ASSERT(typed_entry != NULL);
  prepared_stmt= *typed_entry;
  DBUG_ASSERT(prepared_stmt != NULL);
  *length= prepared_stmt->m_key.m_key_length;
  result= prepared_stmt->m_key.m_hash_key;
  return const_cast<uchar*> (reinterpret_cast<const uchar*> (result));
}
C_MODE_END

/**
  Initialize the prepared statement hash.
  @return 0 on success
*/
int init_prepared_stmt_hash(void)
{
  if ((! prepared_stmt_hash_inited) && (prepared_stmt_max > 0))
  {
    lf_hash_init(&prepared_stmt_hash, sizeof(PFS_prepared_stmt*), LF_HASH_UNIQUE,
                 0, 0, prepared_stmt_hash_get_key, &my_charset_bin);
    prepared_stmt_hash.size= prepared_stmt_max;
    prepared_stmt_hash_inited= true;
  }
  return 0;
}

/** Cleanup the prepared statement hash. */
void cleanup_prepared_stmt_hash(void)
{
  if (prepared_stmt_hash_inited)
  {
    lf_hash_destroy(&prepared_stmt_hash);
    prepared_stmt_hash_inited= false;
  }
}

static void set_prepared_stmt_key(PFS_prepared_stmt_key *key,
                                  PSI_prepared_stmt_data* ps_data)
{
  char *ptr= &key->m_hash_key[0];
  if(ps_data->sql_text_length != 0)
  {
    memcpy(ptr, ps_data->sql_text, ps_data->sql_text_length);
    ptr+= ps_data->sql_text_length;
  }
  ptr[0]= 0;
  ptr++;
 
  /* Mayank TODO: Add more parameters to set key value. */
 
  key->m_key_length= ptr - &key->m_hash_key[0];
  return;
}

void PFS_prepared_stmt::reset_data()
{
  m_prepared_stmt_stat.reset();
}

void reset_prepared_stmt_instances()
{
  if (prepared_stmt_array == NULL)
    return;

  PFS_prepared_stmt *pfs= prepared_stmt_array;
  PFS_prepared_stmt *pfs_last= prepared_stmt_array + prepared_stmt_max;
  
  /* Reset statistics in prepared_stmt_array. */
  for (; pfs < pfs_last ; pfs++)
  {
    pfs->reset_data();
  }
}

static LF_PINS* get_prepared_stmt_hash_pins(PFS_thread *thread)
{
  if (unlikely(thread->m_prepared_stmt_hash_pins == NULL))
  {
    if (! prepared_stmt_hash_inited)
      return NULL;
    thread->m_prepared_stmt_hash_pins= lf_hash_get_pins(&prepared_stmt_hash);
  }
  return thread->m_prepared_stmt_hash_pins;
}

PFS_prepared_stmt*
find_or_create_prepared_stmt(PFS_thread *thread,
                             PSI_prepared_stmt_data* ps_data,
                             my_bool is_create)
{
  if (prepared_stmt_array == NULL || prepared_stmt_max == 0)
    return NULL;

  LF_PINS *pins= get_prepared_stmt_hash_pins(thread);
  if (unlikely(pins == NULL))
    return NULL;
 
  /* Prepare prepared statement key */
  PFS_prepared_stmt_key key;
  set_prepared_stmt_key(&key, ps_data);
  
  PFS_prepared_stmt **entry;
  PFS_prepared_stmt *pfs= NULL;
  uint retry_count= 0;
  const uint retry_max= 3; 
  static uint PFS_ALIGNED prepared_stmt_monotonic_index= 0;
  ulong index= 0;
  ulong attempts= 0;
  pfs_dirty_state dirty_state;

search:
  entry= reinterpret_cast<PFS_prepared_stmt**>
    (lf_hash_search(&prepared_stmt_hash, pins,
                    key.m_hash_key, key.m_key_length));

  if (entry && (entry != MY_ERRPTR))
  {
    /* If record already exists then return its pointer. */
    pfs= *entry;
    lf_hash_search_unpin(pins);
    return pfs;
  }
  
  lf_hash_search_unpin(pins);
 
  if (!is_create)
    return NULL;

  if(prepared_stmt_full)
  {
    prepared_stmt_lost++;
    return NULL;
  }

  /* Else create a new record in prepared stmt stat array. */
  while (++attempts <= prepared_stmt_max)
  {
    index= PFS_atomic::add_u32(& prepared_stmt_monotonic_index, 1) % prepared_stmt_max;
    pfs= prepared_stmt_array + index;
    
    if (pfs->m_lock.is_free())
    {
      if (pfs->m_lock.free_to_dirty(& dirty_state))
      {
        /* Do the assignments. */
        strncpy(pfs->m_sqltext, ps_data->sql_text, ps_data->sql_text_length);
        pfs->m_sqltext_length= ps_data->sql_text_length;
        /* Mayank TODO: Add code to do more assignment. */
      
        /* Insert this record. */
        pfs->m_lock.dirty_to_allocated(& dirty_state);
        int res= lf_hash_insert(&prepared_stmt_hash, pins, &pfs);
       
        if (likely(res == 0))
        {
          return pfs;
        }
       
        pfs->m_lock.allocated_to_free();
      
        if (res > 0)
        {
          /* Duplicate insert by another thread */
          if (++retry_count > retry_max)
          {
            /* Avoid infinite loops */
            prepared_stmt_lost++;
            return NULL;
          }
          goto search;
        }
        /* OOM in lf_hash_insert */
        prepared_stmt_lost++;
        return NULL;
      }
    }
  }
  prepared_stmt_lost++;
  prepared_stmt_full= true;
  return NULL;
}
