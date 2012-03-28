/* Copyright (c) 2008, 2012, Oracle and/or its affiliates. All rights reserved.

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
  @file storage/perfschema/pfs_instr.cc
  Performance schema instruments (implementation).
*/

#include <my_global.h>
#include <string.h>

#include "my_sys.h"
#include "pfs.h"
#include "pfs_stat.h"
#include "pfs_instr.h"
#include "pfs_global.h"

/**
  @addtogroup Performance_schema_buffers
  @{
*/

/** Size of the mutex instances array. @sa mutex_array */
ulong mutex_max;
/** Number of mutexes instance lost. @sa mutex_array */
ulong mutex_lost;
/** Size of the rwlock instances array. @sa rwlock_array */
ulong rwlock_max;
/** Number or rwlock instances lost. @sa rwlock_array */
ulong rwlock_lost;
/** Size of the conditions instances array. @sa cond_array */
ulong cond_max;
/** Number of conditions instances lost. @sa cond_array */
ulong cond_lost;
/** Size of the thread instances array. @sa thread_array */
ulong thread_max;
/** Number or thread instances lost. @sa thread_array */
ulong thread_lost;
/** Size of the file instances array. @sa file_array */
ulong file_max;
/** Number of file instances lost. @sa file_array */
ulong file_lost;
/**
  Size of the file handle array. @sa file_handle_array.
  Signed value, for easier comparisons with a file descriptor number.
*/
long file_handle_max;
/** Number of file handle lost. @sa file_handle_array */
ulong file_handle_lost;
/** Size of the table instances array. @sa table_array */
ulong table_max;
/** Number of table instances lost. @sa table_array */
ulong table_lost;
/** Number of EVENTS_WAITS_HISTORY records per thread. */
ulong events_waits_history_per_thread;
/** Number of instruments class per thread. */
ulong instr_class_per_thread;
/** Number of locker lost. @sa LOCKER_STACK_SIZE. */
ulong locker_lost= 0;

/**
  Mutex instrumentation instances array.
  @sa mutex_max
  @sa mutex_lost
*/
PFS_mutex *mutex_array= NULL;

/**
  RWLock instrumentation instances array.
  @sa rwlock_max
  @sa rwlock_lost
*/
PFS_rwlock *rwlock_array= NULL;

/**
  Condition instrumentation instances array.
  @sa cond_max
  @sa cond_lost
*/
PFS_cond *cond_array= NULL;

/**
  Thread instrumentation instances array.
  @sa thread_max
  @sa thread_lost
*/
PFS_thread *thread_array= NULL;

/**
  File instrumentation instances array.
  @sa file_max
  @sa file_lost
  @sa filename_hash
*/
PFS_file *file_array= NULL;

/**
  File instrumentation handle array.
  @sa file_handle_max
  @sa file_handle_lost
*/
PFS_file **file_handle_array= NULL;

/**
  Table instrumentation instances array.
  @sa table_max
  @sa table_lost
*/
PFS_table *table_array= NULL;

static volatile uint32 thread_internal_id_counter= 0;

static uint per_thread_rwlock_class_start;
static uint per_thread_cond_class_start;
static uint per_thread_file_class_start;
static uint thread_instr_class_waits_sizing;
static PFS_single_stat_chain *thread_instr_class_waits_array= NULL;

static PFS_events_waits *thread_history_array= NULL;

/** Hash table for instrumented files. */
static LF_HASH filename_hash;
/** True if filename_hash is initialized. */
static bool filename_hash_inited= false;
C_MODE_START
/** Get hash table key for instrumented files. */
static uchar *filename_hash_get_key(const uchar *, size_t *, my_bool);
C_MODE_END

/**
  Initialize all the instruments instance buffers.
  @param param                        sizing parameters
  @return 0 on success
*/
int init_instruments(const PFS_global_param *param)
{
  uint thread_history_sizing;
  uint index;

  mutex_max= param->m_mutex_sizing;
  mutex_lost= 0;
  rwlock_max= param->m_rwlock_sizing;
  rwlock_lost= 0;
  cond_max= param->m_cond_sizing;
  cond_lost= 0;
  file_max= param->m_file_sizing;
  file_lost= 0;
  file_handle_max= param->m_file_handle_sizing;
  file_handle_lost= 0;
  table_max= param->m_table_sizing;
  table_lost= 0;
  thread_max= param->m_thread_sizing;
  thread_lost= 0;

  events_waits_history_per_thread= param->m_events_waits_history_sizing;
  thread_history_sizing= param->m_thread_sizing
    * events_waits_history_per_thread;

  per_thread_rwlock_class_start= param->m_mutex_class_sizing;
  per_thread_cond_class_start= per_thread_rwlock_class_start
    + param->m_rwlock_class_sizing;
  per_thread_file_class_start= per_thread_cond_class_start
    + param->m_cond_class_sizing;
  instr_class_per_thread= per_thread_file_class_start
    + param->m_file_class_sizing;

  thread_instr_class_waits_sizing= param->m_thread_sizing
    * instr_class_per_thread;

  mutex_array= NULL;
  rwlock_array= NULL;
  cond_array= NULL;
  file_array= NULL;
  file_handle_array= NULL;
  table_array= NULL;
  thread_array= NULL;
  thread_history_array= NULL;
  thread_instr_class_waits_array= NULL;
  thread_internal_id_counter= 0;

  if (mutex_max > 0)
  {
    mutex_array= PFS_MALLOC_ARRAY(mutex_max, PFS_mutex, MYF(MY_ZEROFILL));
    if (unlikely(mutex_array == NULL))
      return 1;
  }

  if (rwlock_max > 0)
  {
    rwlock_array= PFS_MALLOC_ARRAY(rwlock_max, PFS_rwlock, MYF(MY_ZEROFILL));
    if (unlikely(rwlock_array == NULL))
      return 1;
  }

  if (cond_max > 0)
  {
    cond_array= PFS_MALLOC_ARRAY(cond_max, PFS_cond, MYF(MY_ZEROFILL));
    if (unlikely(cond_array == NULL))
      return 1;
  }

  if (file_max > 0)
  {
    file_array= PFS_MALLOC_ARRAY(file_max, PFS_file, MYF(MY_ZEROFILL));
    if (unlikely(file_array == NULL))
      return 1;
  }

  if (file_handle_max > 0)
  {
    file_handle_array= PFS_MALLOC_ARRAY(file_handle_max, PFS_file*, MYF(MY_ZEROFILL));
    if (unlikely(file_handle_array == NULL))
      return 1;
  }

  if (table_max > 0)
  {
    table_array= PFS_MALLOC_ARRAY(table_max, PFS_table, MYF(MY_ZEROFILL));
    if (unlikely(table_array == NULL))
      return 1;
  }

  if (thread_max > 0)
  {
    thread_array= PFS_MALLOC_ARRAY(thread_max, PFS_thread, MYF(MY_ZEROFILL));
    if (unlikely(thread_array == NULL))
      return 1;
  }

  if (thread_history_sizing > 0)
  {
    thread_history_array=
      PFS_MALLOC_ARRAY(thread_history_sizing, PFS_events_waits,
                       MYF(MY_ZEROFILL));
    if (unlikely(thread_history_array == NULL))
      return 1;
  }

  if (thread_instr_class_waits_sizing > 0)
  {
    thread_instr_class_waits_array=
      PFS_MALLOC_ARRAY(thread_instr_class_waits_sizing,
                       PFS_single_stat_chain, MYF(MY_ZEROFILL));
    if (unlikely(thread_instr_class_waits_array == NULL))
      return 1;
  }

  for (index= 0; index < thread_instr_class_waits_sizing; index++)
  {
    /*
      Currently, this chain is of length 1,
      but it's still implemented as a stat chain,
      since more aggregations are planned to be implemented in m_parent.
    */
    thread_instr_class_waits_array[index].m_control_flag=
      &flag_events_waits_summary_by_thread_by_event_name;
    thread_instr_class_waits_array[index].m_parent= NULL;
  }

  for (index= 0; index < thread_max; index++)
  {
    thread_array[index].m_waits_history=
      &thread_history_array[index * events_waits_history_per_thread];
    thread_array[index].m_instr_class_wait_stats=
      &thread_instr_class_waits_array[index * instr_class_per_thread];
  }

  return 0;
}

/**
  Find the per-thread wait statistics for a mutex class.
  @param thread                       input thread
  @param klass                        mutex class
  @return the per thread per mutex class wait stat
*/
PFS_single_stat_chain *
find_per_thread_mutex_class_wait_stat(PFS_thread *thread,
                                      PFS_mutex_class *klass)
{
  PFS_single_stat_chain *stat;
  uint index;

  DBUG_ASSERT(thread != NULL);
  DBUG_ASSERT(klass != NULL);
  index= klass->m_index;
  DBUG_ASSERT(index < mutex_class_max);

  stat= &(thread->m_instr_class_wait_stats[index]);
  return stat;
}

/**
  Find the per-thread wait statistics for a rwlock class.
  @param thread                       input thread
  @param klass                        rwlock class
  @return the per thread per rwlock class wait stat
*/
PFS_single_stat_chain *
find_per_thread_rwlock_class_wait_stat(PFS_thread *thread,
                                       PFS_rwlock_class *klass)
{
  PFS_single_stat_chain *stat;
  uint index;

  DBUG_ASSERT(thread != NULL);
  DBUG_ASSERT(klass != NULL);
  index= klass->m_index;
  DBUG_ASSERT(index < rwlock_class_max);

  stat= &(thread->m_instr_class_wait_stats
          [per_thread_rwlock_class_start + index]);
  return stat;
}

/**
  Find the per-thread wait statistics for a condition class.
  @param thread                       input thread
  @param klass                        condition class
  @return the per thread per condition class wait stat
*/
PFS_single_stat_chain *
find_per_thread_cond_class_wait_stat(PFS_thread *thread,
                                     PFS_cond_class *klass)
{
  PFS_single_stat_chain *stat;
  uint index;

  DBUG_ASSERT(thread != NULL);
  DBUG_ASSERT(klass != NULL);
  index= klass->m_index;
  DBUG_ASSERT(index < cond_class_max);

  stat= &(thread->m_instr_class_wait_stats
          [per_thread_cond_class_start + index]);
  return stat;
}

/**
  Find the per-thread wait statistics for a file class.
  @param thread                       input thread
  @param klass                        file class
  @return the per thread per file class wait stat
*/
PFS_single_stat_chain *
find_per_thread_file_class_wait_stat(PFS_thread *thread,
                                     PFS_file_class *klass)
{
  PFS_single_stat_chain *stat;
  uint index;

  DBUG_ASSERT(thread != NULL);
  DBUG_ASSERT(klass != NULL);
  index= klass->m_index;
  DBUG_ASSERT(index < file_class_max);

  stat= &(thread->m_instr_class_wait_stats
          [per_thread_file_class_start + index]);
  return stat;
}

/** Reset the wait statistics per thread. */
void reset_per_thread_wait_stat(void)
{
  PFS_single_stat_chain *stat= thread_instr_class_waits_array;
  PFS_single_stat_chain *stat_last= stat + thread_instr_class_waits_sizing;

  for ( ; stat < stat_last; stat++)
    reset_single_stat_link(stat);
}

/** Cleanup all the instruments buffers. */
void cleanup_instruments(void)
{
  pfs_free(mutex_array);
  mutex_array= NULL;
  mutex_max= 0;
  pfs_free(rwlock_array);
  rwlock_array= NULL;
  rwlock_max= 0;
  pfs_free(cond_array);
  cond_array= NULL;
  cond_max= 0;
  pfs_free(file_array);
  file_array= NULL;
  file_max= 0;
  pfs_free(file_handle_array);
  file_handle_array= NULL;
  file_handle_max= 0;
  pfs_free(table_array);
  table_array= NULL;
  table_max= 0;
  pfs_free(thread_array);
  thread_array= NULL;
  thread_max= 0;
  pfs_free(thread_history_array);
  thread_history_array= NULL;
  pfs_free(thread_instr_class_waits_array);
  thread_instr_class_waits_array= NULL;
}

extern "C"
{
static uchar *filename_hash_get_key(const uchar *entry, size_t *length,
                                    my_bool)
{
  const PFS_file * const *typed_entry;
  const PFS_file *file;
  const void *result;
  typed_entry= reinterpret_cast<const PFS_file* const *> (entry);
  DBUG_ASSERT(typed_entry != NULL);
  file= *typed_entry;
  DBUG_ASSERT(file != NULL);
  *length= file->m_filename_length;
  result= file->m_filename;
  return const_cast<uchar*> (reinterpret_cast<const uchar*> (result));
}
}

/**
  Initialize the file name hash.
  @return 0 on success
*/
int init_file_hash(void)
{
  if (! filename_hash_inited)
  {
    lf_hash_init(&filename_hash, sizeof(PFS_file*), LF_HASH_UNIQUE,
                 0, 0, filename_hash_get_key, &my_charset_bin);
    filename_hash_inited= true;
  }
  return 0;
}

/** Cleanup the file name hash. */
void cleanup_file_hash(void)
{
  if (filename_hash_inited)
  {
    lf_hash_destroy(&filename_hash);
    filename_hash_inited= false;
  }
}

void PFS_scan::init(uint random, uint max_size)
{
  m_pass= 0;

  if (max_size == 0)
  {
    /* Degenerated case, no buffer */
    m_pass_max= 0;
    return;
  }

  DBUG_ASSERT(random < max_size);

  if (PFS_MAX_ALLOC_RETRY < max_size)
  {
    /*
      The buffer is big compared to PFS_MAX_ALLOC_RETRY,
      scan it only partially.
    */
    if (random + PFS_MAX_ALLOC_RETRY < max_size)
    {
      /*
        Pass 1: [random, random + PFS_MAX_ALLOC_RETRY - 1]
        Pass 2: not used.
      */
      m_pass_max= 1;
      m_first[0]= random;
      m_last[0]= random + PFS_MAX_ALLOC_RETRY;
      m_first[1]= 0;
      m_last[1]= 0;
    }
    else
    {
      /*
        Pass 1: [random, max_size - 1]
        Pass 2: [0, ...]
        The combined length of pass 1 and 2 is PFS_MAX_ALLOC_RETRY.
      */
      m_pass_max= 2;
      m_first[0]= random;
      m_last[0]= max_size;
      m_first[1]= 0;
      m_last[1]= PFS_MAX_ALLOC_RETRY - (max_size - random);
    }
  }
  else
  {
    /*
      The buffer is small compared to PFS_MAX_ALLOC_RETRY,
      scan it in full in two passes.
      Pass 1: [random, max_size - 1]
      Pass 2: [0, random - 1]
    */
    m_pass_max= 2;
    m_first[0]= random;
    m_last[0]= max_size;
    m_first[1]= 0;
    m_last[1]= random;
  }

  DBUG_ASSERT(m_first[0] < max_size);
  DBUG_ASSERT(m_first[1] < max_size);
  DBUG_ASSERT(m_last[1] <= max_size);
  DBUG_ASSERT(m_last[1] <= max_size);
  /* The combined length of all passes should not exceed PFS_MAX_ALLOC_RETRY. */
  DBUG_ASSERT((m_last[0] - m_first[0]) +
              (m_last[1] - m_first[1]) <= PFS_MAX_ALLOC_RETRY);
}

/**
  Create instrumentation for a mutex instance.
  @param klass                        the mutex class
  @param identity                     the mutex address
  @return a mutex instance, or NULL
*/
PFS_mutex* create_mutex(PFS_mutex_class *klass, const void *identity)
{
  PFS_scan scan;
  uint random= randomized_index(identity, mutex_max);

  for (scan.init(random, mutex_max);
       scan.has_pass();
       scan.next_pass())
  {
    PFS_mutex *pfs= mutex_array + scan.first();
    PFS_mutex *pfs_last= mutex_array + scan.last();
    for ( ; pfs < pfs_last; pfs++)
    {
      if (pfs->m_lock.is_free())
      {
        if (pfs->m_lock.free_to_dirty())
        {
          pfs->m_identity= identity;
          pfs->m_class= klass;
          pfs->m_wait_stat.m_control_flag=
            &flag_events_waits_summary_by_instance;
          pfs->m_wait_stat.m_parent= &klass->m_wait_stat;
          reset_single_stat_link(&pfs->m_wait_stat);
          pfs->m_lock_stat.m_control_flag=
            &flag_events_locks_summary_by_instance;
          pfs->m_lock_stat.m_parent= &klass->m_lock_stat;
          reset_single_stat_link(&pfs->m_lock_stat);
          pfs->m_owner= NULL;
          pfs->m_last_locked= 0;
          pfs->m_lock.dirty_to_allocated();
          return pfs;
        }
      }
    }
  }

  mutex_lost++;
  return NULL;
}

/**
  Destroy instrumentation for a mutex instance.
  @param pfs                          the mutex to destroy
*/
void destroy_mutex(PFS_mutex *pfs)
{
  DBUG_ASSERT(pfs != NULL);
  pfs->m_lock.allocated_to_free();
}

/**
  Create instrumentation for a rwlock instance.
  @param klass                        the rwlock class
  @param identity                     the rwlock address
  @return a rwlock instance, or NULL
*/
PFS_rwlock* create_rwlock(PFS_rwlock_class *klass, const void *identity)
{
  PFS_scan scan;
  uint random= randomized_index(identity, rwlock_max);

  for (scan.init(random, rwlock_max);
       scan.has_pass();
       scan.next_pass())
  {
    PFS_rwlock *pfs= rwlock_array + scan.first();
    PFS_rwlock *pfs_last= rwlock_array + scan.last();
    for ( ; pfs < pfs_last; pfs++)
    {
      if (pfs->m_lock.is_free())
      {
        if (pfs->m_lock.free_to_dirty())
        {
          pfs->m_identity= identity;
          pfs->m_class= klass;
          pfs->m_wait_stat.m_control_flag=
            &flag_events_waits_summary_by_instance;
          pfs->m_wait_stat.m_parent= &klass->m_wait_stat;
          reset_single_stat_link(&pfs->m_wait_stat);
          pfs->m_lock.dirty_to_allocated();
          pfs->m_read_lock_stat.m_control_flag=
            &flag_events_locks_summary_by_instance;
          pfs->m_read_lock_stat.m_parent= &klass->m_read_lock_stat;
          reset_single_stat_link(&pfs->m_read_lock_stat);
          pfs->m_write_lock_stat.m_control_flag=
            &flag_events_locks_summary_by_instance;
          pfs->m_write_lock_stat.m_parent= &klass->m_write_lock_stat;
          reset_single_stat_link(&pfs->m_write_lock_stat);
          pfs->m_writer= NULL;
          pfs->m_readers= 0;
          pfs->m_last_written= 0;
          pfs->m_last_read= 0;
          return pfs;
        }
      }
    }
  }

  rwlock_lost++;
  return NULL;
}

/**
  Destroy instrumentation for a rwlock instance.
  @param pfs                          the rwlock to destroy
*/
void destroy_rwlock(PFS_rwlock *pfs)
{
  DBUG_ASSERT(pfs != NULL);
  pfs->m_lock.allocated_to_free();
}

/**
  Create instrumentation for a condition instance.
  @param klass                        the condition class
  @param identity                     the condition address
  @return a condition instance, or NULL
*/
PFS_cond* create_cond(PFS_cond_class *klass, const void *identity)
{
  PFS_scan scan;
  uint random= randomized_index(identity, cond_max);

  for (scan.init(random, cond_max);
       scan.has_pass();
       scan.next_pass())
  {
    PFS_cond *pfs= cond_array + scan.first();
    PFS_cond *pfs_last= cond_array + scan.last();
    for ( ; pfs < pfs_last; pfs++)
    {
      if (pfs->m_lock.is_free())
      {
        if (pfs->m_lock.free_to_dirty())
        {
          pfs->m_identity= identity;
          pfs->m_class= klass;
          pfs->m_cond_stat.m_signal_count= 0;
          pfs->m_cond_stat.m_broadcast_count= 0;
          pfs->m_wait_stat.m_control_flag=
            &flag_events_waits_summary_by_instance;
          pfs->m_wait_stat.m_parent= &klass->m_wait_stat;
          reset_single_stat_link(&pfs->m_wait_stat);
          pfs->m_lock.dirty_to_allocated();
          return pfs;
        }
      }
    }
  }

  cond_lost++;
  return NULL;
}

/**
  Destroy instrumentation for a condition instance.
  @param pfs                          the condition to destroy
*/
void destroy_cond(PFS_cond *pfs)
{
  DBUG_ASSERT(pfs != NULL);
  pfs->m_lock.allocated_to_free();
}

/**
  Create instrumentation for a thread instance.
  @param klass                        the thread class
  @param identity                     the thread address,
    or a value characteristic of this thread
  @param thread_id                    the PROCESSLIST thread id,
    or 0 if unknown
  @return a thread instance, or NULL
*/
PFS_thread* create_thread(PFS_thread_class *klass, const void *identity,
                          ulong thread_id)
{
  PFS_scan scan;
  uint random= randomized_index(identity, thread_max);

  for (scan.init(random, thread_max);
       scan.has_pass();
       scan.next_pass())
  {
    PFS_thread *pfs= thread_array + scan.first();
    PFS_thread *pfs_last= thread_array + scan.last();
    for ( ; pfs < pfs_last; pfs++)
    {
      if (pfs->m_lock.is_free())
      {
        if (pfs->m_lock.free_to_dirty())
        {
          pfs->m_thread_internal_id=
            PFS_atomic::add_u32(&thread_internal_id_counter, 1);
          pfs->m_thread_id= thread_id;
          pfs->m_event_id= 1;
          pfs->m_enabled= true;
          pfs->m_class= klass;
          pfs->m_wait_locker_count= 0;
          pfs->m_waits_history_full= false;
          pfs->m_waits_history_index= 0;

          PFS_single_stat_chain *stat= pfs->m_instr_class_wait_stats;
          PFS_single_stat_chain *stat_last= stat + instr_class_per_thread;
          for ( ; stat < stat_last; stat++)
            reset_single_stat_link(stat);
          pfs->m_filename_hash_pins= NULL;
          pfs->m_table_share_hash_pins= NULL;
          pfs->m_lock.dirty_to_allocated();
          return pfs;
        }
      }
    }
  }

  thread_lost++;
  return NULL;
}

/**
  Sanitize a PFS_thread pointer.
  Validate that the PFS_thread is part of thread_array.
  Sanitizing data is required when the data can be
  damaged with expected race conditions, for example
  involving EVENTS_WAITS_HISTORY_LONG.
  @param unsafe the pointer to sanitize
  @return a valid pointer, or NULL
*/
PFS_thread *sanitize_thread(PFS_thread *unsafe)
{
  SANITIZE_ARRAY_BODY(PFS_thread, thread_array, thread_max, unsafe);
}

const char *sanitize_file_name(const char *unsafe)
{
  intptr ptr= (intptr) unsafe;
  intptr first= (intptr) &file_array[0];
  intptr last= (intptr) &file_array[file_max];

  /* Check if unsafe points inside file_array[] */
  if (likely((first <= ptr) && (ptr < last)))
  {
    /* Check if unsafe points to PFS_file::m_filename */
    intptr offset= (ptr - first) % sizeof(PFS_file);
    intptr valid_offset= my_offsetof(PFS_file, m_filename[0]);
    if (likely(offset == valid_offset))
    {   
      return unsafe;
    }   
  }
  return NULL;
}

/**
  Destroy instrumentation for a thread instance.
  @param pfs                          the thread to destroy
*/
void destroy_thread(PFS_thread *pfs)
{
  DBUG_ASSERT(pfs != NULL);
  if (pfs->m_filename_hash_pins)
  {
    lf_hash_put_pins(pfs->m_filename_hash_pins);
    pfs->m_filename_hash_pins= NULL;
  }
  if (pfs->m_table_share_hash_pins)
  {
    lf_hash_put_pins(pfs->m_table_share_hash_pins);
    pfs->m_table_share_hash_pins= NULL;
  }
  pfs->m_lock.allocated_to_free();
}

/**
  Get the hash pins for @filename_hash.
  @param thread The running thread.
  @returns The LF_HASH pins for the thread.
*/
LF_PINS* get_filename_hash_pins(PFS_thread *thread)
{
  if (unlikely(thread->m_filename_hash_pins == NULL))
  {
    if (! filename_hash_inited)
      return NULL;
    thread->m_filename_hash_pins= lf_hash_get_pins(&filename_hash);
  }
  return thread->m_filename_hash_pins;
}

/**
  Find or create instrumentation for a file instance by file name.
  @param thread                       the executing instrumented thread
  @param klass                        the file class
  @param filename                     the file name
  @param len                          the length in bytes of filename
  @return a file instance, or NULL
*/
PFS_file*
find_or_create_file(PFS_thread *thread, PFS_file_class *klass,
                    const char *filename, uint len)
{
  PFS_file *pfs;
  PFS_scan scan;

  LF_PINS *pins= get_filename_hash_pins(thread);
  if (unlikely(pins == NULL))
  {
    file_lost++;
    return NULL;
  }

  char safe_buffer[FN_REFLEN];
  const char *safe_filename;

  if (len >= FN_REFLEN)
  {
    /*
      The instrumented code uses file names that exceeds FN_REFLEN.
      This could be legal for instrumentation on non mysys APIs,
      so we support it.
      Truncate the file name so that:
      - it fits into pfs->m_filename
      - it is safe to use mysys apis to normalize the file name.
    */
    memcpy(safe_buffer, filename, FN_REFLEN - 1);
    safe_buffer[FN_REFLEN - 1]= 0;
    safe_filename= safe_buffer;
  }
  else
    safe_filename= filename;

  /*
    Normalize the file name to avoid duplicates when using aliases:
    - absolute or relative paths
    - symbolic links
    Names are resolved as follows:
    - /real/path/to/real_file ==> same
    - /path/with/link/to/real_file ==> /real/path/to/real_file
    - real_file ==> /real/path/to/real_file
    - ./real_file ==> /real/path/to/real_file
    - /real/path/to/sym_link ==> same
    - /path/with/link/to/sym_link ==> /real/path/to/sym_link
    - sym_link ==> /real/path/to/sym_link
    - ./sym_link ==> /real/path/to/sym_link
    When the last component of a file is a symbolic link,
    the last component is *not* resolved, so that all file io
    operations on a link (create, read, write, delete) are counted
    against the link itself, not the target file.
    Resolving the name would lead to create counted against the link,
    and read/write/delete counted against the target, leading to
    incoherent results and instrumentation leaks.
    Also note that, when creating files, this name resolution
    works properly for files that do not exist (yet) on the file system.
  */
  char buffer[FN_REFLEN];
  char dirbuffer[FN_REFLEN];
  size_t dirlen;
  const char *normalized_filename;
  int normalized_length;

  dirlen= dirname_length(safe_filename);
  if (dirlen == 0)
  {
    dirbuffer[0]= FN_CURLIB;
    dirbuffer[1]= FN_LIBCHAR;
    dirbuffer[2]= '\0';
  }
  else
  {
    memcpy(dirbuffer, safe_filename, dirlen);
    dirbuffer[dirlen]= '\0';
  }

  if (my_realpath(buffer, dirbuffer, MYF(0)) != 0)
  {
    file_lost++;
    return NULL;
  }

  /* Append the unresolved file name to the resolved path */
  char *ptr= buffer + strlen(buffer);
  char *buf_end= &buffer[sizeof(buffer)-1];
  if ((buf_end > ptr) && (*(ptr-1) != FN_LIBCHAR))
    *ptr++= FN_LIBCHAR;
  if (buf_end > ptr)
    strncpy(ptr, safe_filename + dirlen, buf_end - ptr);
  *buf_end= '\0';

  normalized_filename= buffer;
  normalized_length= strlen(normalized_filename);

  PFS_file **entry;
  uint retry_count= 0;
  const uint retry_max= 3;
search:
  entry= reinterpret_cast<PFS_file**>
    (lf_hash_search(&filename_hash, pins,
                    normalized_filename, normalized_length));
  if (entry && (entry != MY_ERRPTR))
  {
    pfs= *entry;
    pfs->m_file_stat.m_open_count++;
    lf_hash_search_unpin(pins);
    return pfs;
  }

  lf_hash_search_unpin(pins);

  /* filename is not constant, just using it for noise on create */
  uint random= randomized_index(filename, file_max);

  for (scan.init(random, file_max);
       scan.has_pass();
       scan.next_pass())
  {
    pfs= file_array + scan.first();
    PFS_file *pfs_last= file_array + scan.last();
    for ( ; pfs < pfs_last; pfs++)
    {
      if (pfs->m_lock.is_free())
      {
        if (pfs->m_lock.free_to_dirty())
        {
          pfs->m_class= klass;
          strncpy(pfs->m_filename, normalized_filename, normalized_length);
          pfs->m_filename[normalized_length]= '\0';
          pfs->m_filename_length= normalized_length;
          pfs->m_file_stat.m_open_count= 1;
          pfs->m_wait_stat.m_control_flag=
            &flag_events_waits_summary_by_instance;
          pfs->m_wait_stat.m_parent= &klass->m_wait_stat;
          reset_single_stat_link(&pfs->m_wait_stat);

          int res;
          res= lf_hash_insert(&filename_hash, pins,
                              &pfs);
          if (likely(res == 0))
          {
            pfs->m_lock.dirty_to_allocated();
            return pfs;
          }

          pfs->m_lock.dirty_to_free();

          if (res > 0)
          {
            /* Duplicate insert by another thread */
            if (++retry_count > retry_max)
            {
              /* Avoid infinite loops */
              file_lost++;
              return NULL;
            }
            goto search;
          }

          /* OOM in lf_hash_insert */
          file_lost++;
          return NULL;
        }
      }
    }
  }

  file_lost++;
  return NULL;
}

/**
  Release instrumentation for a file instance.
  @param pfs                          the file to release
*/
void release_file(PFS_file *pfs)
{
  DBUG_ASSERT(pfs != NULL);
  pfs->m_file_stat.m_open_count--;
}

/**
  Destroy instrumentation for a file instance.
  @param thread                       the executing thread instrumentation
  @param pfs                          the file to destroy
*/
void destroy_file(PFS_thread *thread, PFS_file *pfs)
{
  DBUG_ASSERT(thread != NULL);
  DBUG_ASSERT(pfs != NULL);

  LF_PINS *pins= get_filename_hash_pins(thread);
  DBUG_ASSERT(pins != NULL);

  lf_hash_delete(&filename_hash, pins,
                 pfs->m_filename, pfs->m_filename_length);
  pfs->m_lock.allocated_to_free();
}

/**
  Create instrumentation for a table instance.
  @param share                        the table share
  @param identity                     the table address
  @return a table instance, or NULL
*/
PFS_table* create_table(PFS_table_share *share, const void *identity)
{
  PFS_scan scan;
  uint random= randomized_index(identity, table_max);

  for (scan.init(random, table_max);
       scan.has_pass();
       scan.next_pass())
  {
    PFS_table *pfs= table_array + scan.first();
    PFS_table *pfs_last= table_array + scan.last();
    for ( ; pfs < pfs_last; pfs++)
    {
      if (pfs->m_lock.is_free())
      {
        if (pfs->m_lock.free_to_dirty())
        {
          pfs->m_identity= identity;
          pfs->m_share= share;
          pfs->m_wait_stat.m_control_flag=
            &flag_events_waits_summary_by_instance;
          pfs->m_wait_stat.m_parent= &share->m_wait_stat;
          reset_single_stat_link(&pfs->m_wait_stat);
          pfs->m_lock.dirty_to_allocated();
          return pfs;
        }
      }
    }
  }

  table_lost++;
  return NULL;
}

/**
  Destroy instrumentation for a table instance.
  @param pfs                          the table to destroy
*/
void destroy_table(PFS_table *pfs)
{
  DBUG_ASSERT(pfs != NULL);
  pfs->m_lock.allocated_to_free();
}

static void reset_mutex_waits_by_instance(void)
{
  PFS_mutex *pfs= mutex_array;
  PFS_mutex *pfs_last= mutex_array + mutex_max;

  for ( ; pfs < pfs_last; pfs++)
    reset_single_stat_link(&pfs->m_wait_stat);
}

static void reset_rwlock_waits_by_instance(void)
{
  PFS_rwlock *pfs= rwlock_array;
  PFS_rwlock *pfs_last= rwlock_array + rwlock_max;

  for ( ; pfs < pfs_last; pfs++)
    reset_single_stat_link(&pfs->m_wait_stat);
}

static void reset_cond_waits_by_instance(void)
{
  PFS_cond *pfs= cond_array;
  PFS_cond *pfs_last= cond_array + cond_max;

  for ( ; pfs < pfs_last; pfs++)
    reset_single_stat_link(&pfs->m_wait_stat);
}

static void reset_file_waits_by_instance(void)
{
  PFS_file *pfs= file_array;
  PFS_file *pfs_last= file_array + file_max;

  for ( ; pfs < pfs_last; pfs++)
    reset_single_stat_link(&pfs->m_wait_stat);
}

/** Reset the wait statistics per object instance. */
void reset_events_waits_by_instance(void)
{
  reset_mutex_waits_by_instance();
  reset_rwlock_waits_by_instance();
  reset_cond_waits_by_instance();
  reset_file_waits_by_instance();
}

/** Reset the io statistics per file instance. */
void reset_file_instance_io(void)
{
  PFS_file *pfs= file_array;
  PFS_file *pfs_last= file_array + file_max;

  for ( ; pfs < pfs_last; pfs++)
    reset_file_stat(&pfs->m_file_stat);
}

/** @} */
