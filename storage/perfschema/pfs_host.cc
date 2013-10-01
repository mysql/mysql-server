/* Copyright (c) 2010, 2013, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

/**
  @file storage/perfschema/pfs_host.cc
  Performance schema host (implementation).
*/

#include "my_global.h"
#include "my_sys.h"
#include "pfs.h"
#include "pfs_stat.h"
#include "pfs_instr.h"
#include "pfs_setup_actor.h"
#include "pfs_host.h"
#include "pfs_global.h"
#include "pfs_instr_class.h"

/**
  @addtogroup Performance_schema_buffers
  @{
*/

ulong host_max;
ulong host_lost;

PFS_host *host_array= NULL;

static PFS_single_stat *host_instr_class_waits_array= NULL;
static PFS_stage_stat *host_instr_class_stages_array= NULL;
static PFS_statement_stat *host_instr_class_statements_array= NULL;

LF_HASH host_hash;
static bool host_hash_inited= false;

/**
  Initialize the host buffers.
  @param param                        sizing parameters
  @return 0 on success
*/
int init_host(const PFS_global_param *param)
{
  uint index;

  host_max= param->m_host_sizing;

  host_array= NULL;
  host_instr_class_waits_array= NULL;
  host_instr_class_stages_array= NULL;
  host_instr_class_statements_array= NULL;
  uint waits_sizing= host_max * wait_class_max;
  uint stages_sizing= host_max * stage_class_max;
  uint statements_sizing= host_max * statement_class_max;

  if (host_max > 0)
  {
    host_array= PFS_MALLOC_ARRAY(host_max, PFS_host,
                                 MYF(MY_ZEROFILL));
    if (unlikely(host_array == NULL))
      return 1;
  }

  if (waits_sizing > 0)
  {
    host_instr_class_waits_array=
      PFS_connection_slice::alloc_waits_slice(waits_sizing);
    if (unlikely(host_instr_class_waits_array == NULL))
      return 1;
  }

  if (stages_sizing > 0)
  {
    host_instr_class_stages_array=
      PFS_connection_slice::alloc_stages_slice(stages_sizing);
    if (unlikely(host_instr_class_stages_array == NULL))
      return 1;
  }

  if (statements_sizing > 0)
  {
    host_instr_class_statements_array=
      PFS_connection_slice::alloc_statements_slice(statements_sizing);
    if (unlikely(host_instr_class_statements_array == NULL))
      return 1;
  }

  for (index= 0; index < host_max; index++)
  {
    host_array[index].m_instr_class_waits_stats=
      &host_instr_class_waits_array[index * wait_class_max];
    host_array[index].m_instr_class_stages_stats=
      &host_instr_class_stages_array[index * stage_class_max];
    host_array[index].m_instr_class_statements_stats=
      &host_instr_class_statements_array[index * statement_class_max];
  }

  return 0;
}

/** Cleanup all the host buffers. */
void cleanup_host(void)
{
  pfs_free(host_array);
  host_array= NULL;
  pfs_free(host_instr_class_waits_array);
  host_instr_class_waits_array= NULL;
  pfs_free(host_instr_class_stages_array);
  host_instr_class_stages_array= NULL;
  pfs_free(host_instr_class_statements_array);
  host_instr_class_statements_array= NULL;
  host_max= 0;
}

C_MODE_START
static uchar *host_hash_get_key(const uchar *entry, size_t *length,
                                my_bool)
{
  const PFS_host * const *typed_entry;
  const PFS_host *host;
  const void *result;
  typed_entry= reinterpret_cast<const PFS_host* const *> (entry);
  DBUG_ASSERT(typed_entry != NULL);
  host= *typed_entry;
  DBUG_ASSERT(host != NULL);
  *length= host->m_key.m_key_length;
  result= host->m_key.m_hash_key;
  return const_cast<uchar*> (reinterpret_cast<const uchar*> (result));
}
C_MODE_END

/**
  Initialize the host hash.
  @return 0 on success
*/
int init_host_hash(void)
{
  if ((! host_hash_inited) && (host_max > 0))
  {
    lf_hash_init(&host_hash, sizeof(PFS_host*), LF_HASH_UNIQUE,
                 0, 0, host_hash_get_key, &my_charset_bin);
    host_hash.size= host_max;
    host_hash_inited= true;
  }
  return 0;
}

/** Cleanup the host hash. */
void cleanup_host_hash(void)
{
  if (host_hash_inited)
  {
    lf_hash_destroy(&host_hash);
    host_hash_inited= false;
  }
}

static LF_PINS* get_host_hash_pins(PFS_thread *thread)
{
  if (unlikely(thread->m_host_hash_pins == NULL))
  {
    if (! host_hash_inited)
      return NULL;
    thread->m_host_hash_pins= lf_hash_get_pins(&host_hash);
  }
  return thread->m_host_hash_pins;
}

static void set_host_key(PFS_host_key *key,
                         const char *host, uint host_length)
{
  DBUG_ASSERT(host_length <= HOSTNAME_LENGTH);

  char *ptr= &key->m_hash_key[0];
  if (host_length > 0)
  {
    memcpy(ptr, host, host_length);
    ptr+= host_length;
  }
  ptr[0]= 0;
  ptr++;
  key->m_key_length= ptr - &key->m_hash_key[0];
}

PFS_host *find_or_create_host(PFS_thread *thread,
                              const char *hostname, uint hostname_length)
{
  if (host_max == 0)
  {
    host_lost++;
    return NULL;
  }

  LF_PINS *pins= get_host_hash_pins(thread);
  if (unlikely(pins == NULL))
  {
    host_lost++;
    return NULL;
  }

  PFS_host_key key;
  set_host_key(&key, hostname, hostname_length);

  PFS_host **entry;
  uint retry_count= 0;
  const uint retry_max= 3;

search:
  entry= reinterpret_cast<PFS_host**>
    (lf_hash_search(&host_hash, pins,
                    key.m_hash_key, key.m_key_length));
  if (entry && (entry != MY_ERRPTR))
  {
    PFS_host *pfs;
    pfs= *entry;
    pfs->inc_refcount();
    lf_hash_search_unpin(pins);
    return pfs;
  }

  lf_hash_search_unpin(pins);

  PFS_scan scan;
  uint random= randomized_index(hostname, host_max);

  for (scan.init(random, host_max);
       scan.has_pass();
       scan.next_pass())
  {
    PFS_host *pfs= host_array + scan.first();
    PFS_host *pfs_last= host_array + scan.last();
    for ( ; pfs < pfs_last; pfs++)
    {
      if (pfs->m_lock.is_free())
      {
        if (pfs->m_lock.free_to_dirty())
        {
          pfs->m_key= key;
          if (hostname_length > 0)
            pfs->m_hostname= &pfs->m_key.m_hash_key[0];
          else
            pfs->m_hostname= NULL;
          pfs->m_hostname_length= hostname_length;

          pfs->init_refcount();
          pfs->reset_stats();
          pfs->m_disconnected_count= 0;

          int res;
          res= lf_hash_insert(&host_hash, pins, &pfs);
          if (likely(res == 0))
          {
            pfs->m_lock.dirty_to_allocated();
            return pfs;
          }

          pfs->m_lock.dirty_to_free();

          if (res > 0)
          {
            if (++retry_count > retry_max)
            {
              host_lost++;
              return NULL;
            }
            goto search;
          }

          host_lost++;
          return NULL;
        }
      }
    }
  }

  host_lost++;
  return NULL;
}

void PFS_host::aggregate()
{
  aggregate_waits();
  aggregate_stages();
  aggregate_statements();
  aggregate_stats();
}

void PFS_host::aggregate_waits()
{
  /* No parent to aggregate to, clean the stats */
  reset_waits_stats();
}

void PFS_host::aggregate_stages()
{
  /*
    Aggregate EVENTS_STAGES_SUMMARY_BY_HOST_BY_EVENT_NAME to:
    -  EVENTS_STAGES_SUMMARY_GLOBAL_BY_EVENT_NAME
  */
  aggregate_all_stages(m_instr_class_stages_stats,
                       global_instr_class_stages_array);
}

void PFS_host::aggregate_statements()
{
  /*
    Aggregate EVENTS_STATEMENTS_SUMMARY_BY_HOST_BY_EVENT_NAME to:
    -  EVENTS_STATEMENTS_SUMMARY_GLOBAL_BY_EVENT_NAME
  */
  aggregate_all_statements(m_instr_class_statements_stats,
                           global_instr_class_statements_array);
}

void PFS_host::aggregate_stats()
{
  /* No parent to aggregate to, clean the stats */
  m_disconnected_count= 0;
}

void PFS_host::release()
{
  dec_refcount();
}

PFS_host *sanitize_host(PFS_host *unsafe)
{
  if ((&host_array[0] <= unsafe) &&
      (unsafe < &host_array[host_max]))
    return unsafe;
  return NULL;
}

void purge_host(PFS_thread *thread, PFS_host *host)
{
  LF_PINS *pins= get_host_hash_pins(thread);
  if (unlikely(pins == NULL))
    return;

  PFS_host **entry;
  entry= reinterpret_cast<PFS_host**>
    (lf_hash_search(&host_hash, pins,
                    host->m_key.m_hash_key, host->m_key.m_key_length));
  if (entry && (entry != MY_ERRPTR))
  {
    PFS_host *pfs;
    pfs= *entry;
    DBUG_ASSERT(pfs == host);
    if (host->get_refcount() == 0)
    {
      lf_hash_delete(&host_hash, pins,
                     host->m_key.m_hash_key, host->m_key.m_key_length);
      host->m_lock.allocated_to_free();
    }
  }

  lf_hash_search_unpin(pins);
}

/** Purge non connected hosts, reset stats of connected hosts. */
void purge_all_host(void)
{
  PFS_thread *thread= PFS_thread::get_current_thread();
  if (unlikely(thread == NULL))
    return;

  PFS_host *pfs= host_array;
  PFS_host *pfs_last= host_array + host_max;

  for ( ; pfs < pfs_last; pfs++)
  {
    if (pfs->m_lock.is_populated())
    {
      pfs->aggregate();
      if (pfs->get_refcount() == 0)
        purge_host(thread, pfs);
    }
  }
}

/** @} */
