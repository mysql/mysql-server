/* Copyright (c) 2008, 2017, Oracle and/or its affiliates. All rights reserved.

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

/**
  @file storage/perfschema/pfs_digest.cc
  Statement Digest data structures (implementation).
*/

/*
  This code needs extra visibility in the lexer structures
*/

#include "storage/perfschema/pfs_digest.h"

#include <string.h>

#include "my_compiler.h"
#include "my_dbug.h"
#include "my_sys.h"
#include "sql/sql_get_diagnostics.h"
#include "sql/sql_lex.h"
#include "sql/sql_signal.h"
#include "sql_string.h"
#include "storage/perfschema/pfs_builtin_memory.h"
#include "storage/perfschema/pfs_global.h"
#include "storage/perfschema/pfs_instr.h"
#include "storage/perfschema/table_helper.h"

size_t digest_max = 0;
ulong digest_lost = 0;

/** EVENTS_STATEMENTS_SUMMARY_BY_DIGEST buffer. */
PFS_statements_digest_stat *statements_digest_stat_array = NULL;
static unsigned char *statements_digest_token_array = NULL;
static char *statements_digest_query_sample_text_array = NULL;
/** Consumer flag for table EVENTS_STATEMENTS_SUMMARY_BY_DIGEST. */
bool flag_statements_digest = true;
/**
  Current index in Stat array where new record is to be inserted.
  index 0 is reserved for "all else" case when entire array is full.
*/
static PFS_ALIGNED PFS_cacheline_atomic_uint32 digest_monotonic_index;
bool digest_full = false;

LF_HASH digest_hash;
static bool digest_hash_inited = false;

/**
  Initialize table EVENTS_STATEMENTS_SUMMARY_BY_DIGEST.
  @param param performance schema sizing
*/
int
init_digest(const PFS_global_param *param)
{
  /*
    Allocate memory for statements_digest_stat_array based on
    performance_schema_digests_size values
  */
  digest_max = param->m_digest_sizing;
  digest_lost = 0;
  digest_monotonic_index.m_u32.store(1);
  digest_full = false;

  if (digest_max == 0)
  {
    return 0;
  }

  statements_digest_stat_array =
    PFS_MALLOC_ARRAY(&builtin_memory_digest,
                     digest_max,
                     sizeof(PFS_statements_digest_stat),
                     PFS_statements_digest_stat,
                     MYF(MY_ZEROFILL));

  if (unlikely(statements_digest_stat_array == NULL))
  {
    cleanup_digest();
    return 1;
  }

  if (pfs_max_digest_length > 0)
  {
    /* Size of each digest array. */
    size_t digest_memory_size = pfs_max_digest_length * sizeof(unsigned char);

    statements_digest_token_array =
      PFS_MALLOC_ARRAY(&builtin_memory_digest_tokens,
                       digest_max,
                       digest_memory_size,
                       unsigned char,
                       MYF(MY_ZEROFILL));

    if (unlikely(statements_digest_token_array == NULL))
    {
      cleanup_digest();
      return 1;
    }
  }

  if (pfs_max_sqltext > 0)
  {
    /* Size of each query sample text array. */
    size_t sqltext_size = pfs_max_sqltext * sizeof(char);

    statements_digest_query_sample_text_array =
      PFS_MALLOC_ARRAY(&builtin_memory_digest_sample_sqltext,
                       digest_max,
                       sqltext_size,
                       char,
                       MYF(MY_ZEROFILL));

    if (unlikely(statements_digest_query_sample_text_array == NULL))
    {
      cleanup_digest();
      return 1;
    }
  }

  for (size_t index = 0; index < digest_max; index++)
  {
    statements_digest_stat_array[index].reset_data(
      statements_digest_token_array + index * pfs_max_digest_length,
      pfs_max_digest_length,
      statements_digest_query_sample_text_array + index * pfs_max_sqltext);
  }

  /* Set record[0] as allocated. */
  statements_digest_stat_array[0].m_lock.set_allocated();

  return 0;
}

/** Cleanup table EVENTS_STATEMENTS_SUMMARY_BY_DIGEST. */
void
cleanup_digest(void)
{
  PFS_FREE_ARRAY(&builtin_memory_digest,
                 digest_max,
                 sizeof(PFS_statements_digest_stat),
                 statements_digest_stat_array);

  PFS_FREE_ARRAY(&builtin_memory_digest_tokens,
                 digest_max,
                 (pfs_max_digest_length * sizeof(unsigned char)),
                 statements_digest_token_array);

  PFS_FREE_ARRAY(&builtin_memory_digest_sample_sqltext,
                 digest_max,
                 (pfs_max_sqltext * sizeof(char)),
                 statements_digest_query_sample_text_array);

  statements_digest_stat_array = NULL;
  statements_digest_token_array = NULL;
  statements_digest_query_sample_text_array = NULL;
}

static const uchar *
digest_hash_get_key(const uchar *entry, size_t *length)
{
  const PFS_statements_digest_stat *const *typed_entry;
  const PFS_statements_digest_stat *digest;
  const void *result;
  typed_entry =
    reinterpret_cast<const PFS_statements_digest_stat *const *>(entry);
  DBUG_ASSERT(typed_entry != NULL);
  digest = *typed_entry;
  DBUG_ASSERT(digest != NULL);
  *length = sizeof(PFS_digest_key);
  result = &digest->m_digest_key;
  return reinterpret_cast<const uchar *>(result);
}

/**
  Initialize the digest hash.
  @return 0 on success
*/
int
init_digest_hash(const PFS_global_param *param)
{
  if ((!digest_hash_inited) && (param->m_digest_sizing != 0))
  {
    lf_hash_init(&digest_hash,
                 sizeof(PFS_statements_digest_stat *),
                 LF_HASH_UNIQUE,
                 0,
                 0,
                 digest_hash_get_key,
                 &my_charset_bin);
    digest_hash_inited = true;
  }
  return 0;
}

void
cleanup_digest_hash(void)
{
  if (digest_hash_inited)
  {
    lf_hash_destroy(&digest_hash);
    digest_hash_inited = false;
  }
}

static LF_PINS *
get_digest_hash_pins(PFS_thread *thread)
{
  if (unlikely(thread->m_digest_hash_pins == NULL))
  {
    if (!digest_hash_inited)
    {
      return NULL;
    }
    thread->m_digest_hash_pins = lf_hash_get_pins(&digest_hash);
  }
  return thread->m_digest_hash_pins;
}

PFS_statements_digest_stat *
find_or_create_digest(PFS_thread *thread,
                      const sql_digest_storage *digest_storage,
                      const char *schema_name,
                      uint schema_name_length)
{
  DBUG_ASSERT(digest_storage != NULL);

  if (statements_digest_stat_array == NULL)
  {
    return NULL;
  }

  if (digest_storage->m_byte_count <= 0)
  {
    return NULL;
  }

  LF_PINS *pins = get_digest_hash_pins(thread);
  if (unlikely(pins == NULL))
  {
    return NULL;
  }

  /*
    Note: the LF_HASH key is a block of memory,
    make sure to clean unused bytes,
    so that memcmp() can compare keys.
  */
  PFS_digest_key hash_key;
  memset(&hash_key, 0, sizeof(hash_key));
  /* Copy digest hash of the tokens received. */
  memcpy(&hash_key.m_hash, digest_storage->m_hash, DIGEST_HASH_SIZE);
  /* Add the current schema to the key */
  hash_key.m_schema_name_length = schema_name_length;
  if (schema_name_length > 0)
  {
    memcpy(hash_key.m_schema_name, schema_name, schema_name_length);
  }

  int res;
  uint retry_count = 0;
  const uint retry_max = 3;
  size_t safe_index;
  size_t attempts = 0;
  PFS_statements_digest_stat **entry;
  PFS_statements_digest_stat *pfs = NULL;
  pfs_dirty_state dirty_state;

  ulonglong now = my_micro_time();

search:

  /* Lookup LF_HASH using this new key. */
  entry = reinterpret_cast<PFS_statements_digest_stat **>(
    lf_hash_search(&digest_hash, pins, &hash_key, sizeof(PFS_digest_key)));

  if (entry && (entry != MY_LF_ERRPTR))
  {
    /* If digest already exists, update stats and return. */
    pfs = *entry;
    pfs->m_last_seen = now;
    lf_hash_search_unpin(pins);
    return pfs;
  }

  lf_hash_search_unpin(pins);

  if (digest_full)
  {
    /* digest_stat array is full. Add stat at index 0 and return. */
    pfs = &statements_digest_stat_array[0];
    digest_lost++;

    if (pfs->m_first_seen == 0)
    {
      pfs->m_first_seen = now;
    }
    pfs->m_last_seen = now;
    return pfs;
  }

  while (++attempts <= digest_max)
  {
    safe_index = digest_monotonic_index.m_u32++ % digest_max;
    if (safe_index == 0)
    {
      /* Record [0] is reserved. */
      continue;
    }

    /* Add a new record in digest stat array. */
    DBUG_ASSERT(safe_index < digest_max);
    pfs = &statements_digest_stat_array[safe_index];

    if (pfs->m_lock.is_free())
    {
      if (pfs->m_lock.free_to_dirty(&dirty_state))
      {
        /* Copy digest hash/LF Hash search key. */
        memcpy(&pfs->m_digest_key, &hash_key, sizeof(PFS_digest_key));

        /*
          Copy digest storage to statement_digest_stat_array so that it could be
          used later to generate digest text.
        */
        pfs->m_digest_storage.copy(digest_storage);

        pfs->m_first_seen = now;
        pfs->m_last_seen = now;

        pfs->m_query_sample_refs = 0;

        pfs->m_histogram.reset();

        res = lf_hash_insert(&digest_hash, pins, &pfs);
        if (likely(res == 0))
        {
          pfs->m_lock.dirty_to_allocated(&dirty_state);
          return pfs;
        }

        pfs->m_lock.dirty_to_free(&dirty_state);

        if (res > 0)
        {
          /* Duplicate insert by another thread */
          if (++retry_count > retry_max)
          {
            /* Avoid infinite loops */
            digest_lost++;
            return NULL;
          }
          goto search;
        }

        /* OOM in lf_hash_insert */
        digest_lost++;
        return NULL;
      }
    }
  }

  /* The digest array is now full. */
  digest_full = true;
  pfs = &statements_digest_stat_array[0];

  if (pfs->m_first_seen == 0)
  {
    pfs->m_first_seen = now;
  }
  pfs->m_last_seen = now;
  return pfs;
}

static void
purge_digest(PFS_thread *thread, PFS_digest_key *hash_key)
{
  LF_PINS *pins = get_digest_hash_pins(thread);
  if (unlikely(pins == NULL))
  {
    return;
  }

  PFS_statements_digest_stat **entry;

  /* Lookup LF_HASH using this new key. */
  entry = reinterpret_cast<PFS_statements_digest_stat **>(
    lf_hash_search(&digest_hash, pins, hash_key, sizeof(PFS_digest_key)));

  if (entry && (entry != MY_LF_ERRPTR))
  {
    lf_hash_delete(&digest_hash, pins, hash_key, sizeof(PFS_digest_key));
  }
  lf_hash_search_unpin(pins);
  return;
}

void
PFS_statements_digest_stat::reset_data(unsigned char *token_array,
                                       size_t token_array_length,
                                       char *query_sample_array)
{
  pfs_dirty_state dirty_state;
  m_lock.set_dirty(&dirty_state);
  m_digest_storage.reset(token_array, token_array_length);
  m_stat.reset();
  m_first_seen = 0;
  m_last_seen = 0;
  m_query_sample = query_sample_array;
  m_query_sample_length = 0;
  m_query_sample_truncated = false;
  m_query_sample_seen = 0;
  m_query_sample_timer_wait = 0;
  m_query_sample_cs_number = system_charset_info->number;
  m_lock.dirty_to_free(&dirty_state);
}

void
PFS_statements_digest_stat::reset_index(PFS_thread *thread)
{
  /* Only remove entries that exists in the HASH index. */
  if (m_digest_storage.m_byte_count > 0)
  {
    purge_digest(thread, &m_digest_key);
  }
}

void
reset_esms_by_digest()
{
  uint index;

  if (statements_digest_stat_array == NULL)
  {
    return;
  }

  PFS_thread *thread = PFS_thread::get_current_thread();
  if (unlikely(thread == NULL))
  {
    return;
  }

  /* Reset statements_digest_stat_array. */
  for (index = 0; index < digest_max; index++)
  {
    statements_digest_stat_array[index].reset_index(thread);
    statements_digest_stat_array[index].reset_data(
      statements_digest_token_array + index * pfs_max_digest_length,
      pfs_max_digest_length,
      statements_digest_query_sample_text_array + index * pfs_max_sqltext);
  }

  /* Mark record[0] as allocated again. */
  statements_digest_stat_array[0].m_lock.set_allocated();

  /*
    Reset index which indicates where the next calculated digest information
    to be inserted in statements_digest_stat_array.
  */
  digest_monotonic_index.m_u32.store(1);
  digest_full = false;
}

void
reset_histogram_by_digest()
{
  uint index;

  if (statements_digest_stat_array == NULL)
  {
    return;
  }

  for (index = 0; index < digest_max; index++)
  {
    statements_digest_stat_array[index].m_histogram.reset();
  }
}
