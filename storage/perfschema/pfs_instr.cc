/* Copyright (c) 2008, 2014, Oracle and/or its affiliates. All rights reserved.

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
#include "pfs_host.h"
#include "pfs_user.h"
#include "pfs_account.h"
#include "pfs_global.h"
#include "pfs_instr_class.h"

ulong nested_statement_lost= 0;

/**
  @addtogroup Performance_schema_buffers
  @{
*/

/** Size of the mutex instances array. @sa mutex_array */
ulong mutex_max= 0;
/** True when @c mutex_array is full. */
bool mutex_full;
/** Number of mutexes instance lost. @sa mutex_array */
ulong mutex_lost= 0;
/** Size of the rwlock instances array. @sa rwlock_array */
ulong rwlock_max= 0;
/** True when @c rwlock_array is full. */
bool rwlock_full;
/** Number or rwlock instances lost. @sa rwlock_array */
ulong rwlock_lost= 0;
/** Size of the conditions instances array. @sa cond_array */
ulong cond_max= 0;
/** True when @c cond_array is full. */
bool cond_full;
/** Number of conditions instances lost. @sa cond_array */
ulong cond_lost= 0;
/** Size of the thread instances array. @sa thread_array */
ulong thread_max= 0;
/** True when @c thread_array is full. */
bool thread_full;
/** Number or thread instances lost. @sa thread_array */
ulong thread_lost= 0;
/** Size of the file instances array. @sa file_array */
ulong file_max= 0;
/** True when @c file_array is full. */
bool file_full;
/** Number of file instances lost. @sa file_array */
ulong file_lost= 0;
/**
  Size of the file handle array. @sa file_handle_array.
  Signed value, for easier comparisons with a file descriptor number.
*/
long file_handle_max= 0;
/** True when @c file_handle_array is full. */
bool file_handle_full;
/** Number of file handle lost. @sa file_handle_array */
ulong file_handle_lost= 0;
/** Size of the table instances array. @sa table_array */
ulong table_max= 0;
/** True when @c table_array is full. */
bool table_full;
/** Number of table instances lost. @sa table_array */
ulong table_lost= 0;
/** Size of the socket instances array. @sa socket_array */
ulong socket_max= 0;
/** True when @c socket_array is full. */
bool socket_full;
/** Number of socket instances lost. @sa socket_array */
ulong socket_lost= 0;
/** Number of EVENTS_WAITS_HISTORY records per thread. */
ulong events_waits_history_per_thread= 0;
/** Number of EVENTS_STAGES_HISTORY records per thread. */
ulong events_stages_history_per_thread= 0;
/** Number of EVENTS_STATEMENTS_HISTORY records per thread. */
ulong events_statements_history_per_thread= 0;
uint statement_stack_max= 0;
/** Number of locker lost. @sa LOCKER_STACK_SIZE. */
ulong locker_lost= 0;
/** Number of statements lost. @sa STATEMENT_STACK_SIZE. */
ulong statement_lost= 0;
/** Size of connection attribute storage per thread */
ulong session_connect_attrs_size_per_thread;
/** Number of connection attributes lost */
ulong session_connect_attrs_lost= 0;

ulong metadata_lock_max;
ulong metadata_lock_lost;
/** True when @c metadata_lock_array is full. */
bool metadata_lock_full;
/** Number of EVENTS_TRANSACTIONS_HISTORY records per thread. */
ulong events_transactions_history_per_thread= 0;

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

/**
  Socket instrumentation instances array.
  @sa socket_max
  @sa socket_lost
*/
PFS_socket *socket_array= NULL;

PFS_metadata_lock *metadata_lock_array= NULL;

PFS_stage_stat *global_instr_class_stages_array= NULL;
PFS_statement_stat *global_instr_class_statements_array= NULL;
PFS_memory_stat *global_instr_class_memory_array= NULL;

static PFS_ALIGNED PFS_cacheline_uint64 thread_internal_id_counter;

static uint thread_instr_class_waits_sizing= 0;
static uint thread_instr_class_stages_sizing= 0;
static uint thread_instr_class_statements_sizing= 0;
static uint thread_instr_class_transactions_sizing= 0;
static uint thread_instr_class_memory_sizing= 0;
static PFS_single_stat *thread_instr_class_waits_array= NULL;
static PFS_stage_stat *thread_instr_class_stages_array= NULL;
static PFS_statement_stat *thread_instr_class_statements_array= NULL;
static PFS_transaction_stat *thread_instr_class_transactions_array= NULL;
static PFS_memory_stat *thread_instr_class_memory_array= NULL;

static PFS_events_waits *thread_waits_history_array= NULL;
static PFS_events_stages *thread_stages_history_array= NULL;
static PFS_events_statements *thread_statements_history_array= NULL;
static PFS_events_statements *thread_statements_stack_array= NULL;
static PFS_events_transactions *thread_transactions_history_array= NULL;
static char *thread_session_connect_attrs_array= NULL;

PFS_single_stat *thread_instr_class_waits_array_start= NULL;
PFS_single_stat *thread_instr_class_waits_array_end= NULL;

/** Hash table for instrumented files. */
LF_HASH filename_hash;
/** True if filename_hash is initialized. */
static bool filename_hash_inited= false;

/**
  Initialize all the instruments instance buffers.
  @param param                        sizing parameters
  @return 0 on success
*/
int init_instruments(const PFS_global_param *param)
{
  uint thread_waits_history_sizing;
  uint thread_stages_history_sizing;
  uint thread_statements_history_sizing;
  uint thread_statements_stack_sizing;
  uint thread_transactions_history_sizing;
  uint thread_session_connect_attrs_sizing;
  uint index;

  /* Make sure init_event_name_sizing is called */
  DBUG_ASSERT(wait_class_max != 0);

  mutex_max= param->m_mutex_sizing;
  mutex_full= false;
  mutex_lost= 0;
  rwlock_max= param->m_rwlock_sizing;
  rwlock_full= false;
  rwlock_lost= 0;
  cond_max= param->m_cond_sizing;
  cond_full= false;
  cond_lost= 0;
  file_max= param->m_file_sizing;
  file_full= false;
  file_lost= 0;
  file_handle_max= param->m_file_handle_sizing;
  file_handle_full= false;
  file_handle_lost= 0;
  table_max= param->m_table_sizing;
  table_full= false;
  table_lost= 0;
  thread_max= param->m_thread_sizing;
  thread_full= false;
  thread_lost= 0;
  socket_max= param->m_socket_sizing;
  socket_full= false;
  socket_lost= 0;
  metadata_lock_max= param->m_metadata_lock_sizing;
  metadata_lock_full= false;
  metadata_lock_lost= 0;

  events_waits_history_per_thread= param->m_events_waits_history_sizing;
  thread_waits_history_sizing= param->m_thread_sizing
    * events_waits_history_per_thread;

  thread_instr_class_waits_sizing= param->m_thread_sizing
    * wait_class_max;

  events_stages_history_per_thread= param->m_events_stages_history_sizing;
  thread_stages_history_sizing= param->m_thread_sizing
    * events_stages_history_per_thread;

  events_statements_history_per_thread= param->m_events_statements_history_sizing;
  thread_statements_history_sizing= param->m_thread_sizing
    * events_statements_history_per_thread;

  statement_stack_max= param->m_statement_stack_sizing;
  thread_statements_stack_sizing= param->m_thread_sizing * statement_stack_max;

  events_transactions_history_per_thread= param->m_events_transactions_history_sizing;
  thread_transactions_history_sizing= param->m_thread_sizing
    * events_transactions_history_per_thread;

  thread_instr_class_stages_sizing= param->m_thread_sizing
    * param->m_stage_class_sizing;

  thread_instr_class_statements_sizing= param->m_thread_sizing
    * param->m_statement_class_sizing;

  /* Transaction class is global with fixed size, no m_transaction_sizing */
  thread_instr_class_transactions_sizing= param->m_thread_sizing
    * transaction_class_max;

  thread_instr_class_memory_sizing= param->m_thread_sizing
    * param->m_memory_class_sizing;

  session_connect_attrs_size_per_thread= param->m_session_connect_attrs_sizing;
  thread_session_connect_attrs_sizing= param->m_thread_sizing
    * session_connect_attrs_size_per_thread;
  session_connect_attrs_lost= 0;

  mutex_array= NULL;
  rwlock_array= NULL;
  cond_array= NULL;
  file_array= NULL;
  file_handle_array= NULL;
  table_array= NULL;
  socket_array= NULL;
  metadata_lock_array= NULL;
  thread_array= NULL;
  thread_waits_history_array= NULL;
  thread_stages_history_array= NULL;
  thread_statements_history_array= NULL;
  thread_statements_stack_array= NULL;
  thread_transactions_history_array= NULL;
  thread_instr_class_waits_array= NULL;
  thread_instr_class_stages_array= NULL;
  thread_instr_class_statements_array= NULL;
  thread_instr_class_transactions_array= NULL;
  thread_instr_class_memory_array= NULL;
  thread_internal_id_counter.m_u64= 0;

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

  if (socket_max > 0)
  {
    socket_array= PFS_MALLOC_ARRAY(socket_max, PFS_socket, MYF(MY_ZEROFILL));
    if (unlikely(socket_array == NULL))
      return 1;
  }

  if (metadata_lock_max > 0)
  {
    metadata_lock_array= PFS_MALLOC_ARRAY(metadata_lock_max, PFS_metadata_lock, MYF(MY_ZEROFILL));
    if (unlikely(metadata_lock_array == NULL))
      return 1;
  }

  if (thread_max > 0)
  {
    thread_array= PFS_MALLOC_ARRAY(thread_max, PFS_thread, MYF(MY_ZEROFILL));
    if (unlikely(thread_array == NULL))
      return 1;
  }

  if (thread_waits_history_sizing > 0)
  {
    thread_waits_history_array=
      PFS_MALLOC_ARRAY(thread_waits_history_sizing, PFS_events_waits,
                       MYF(MY_ZEROFILL));
    if (unlikely(thread_waits_history_array == NULL))
      return 1;
  }

  if (thread_instr_class_waits_sizing > 0)
  {
    thread_instr_class_waits_array=
      PFS_MALLOC_ARRAY(thread_instr_class_waits_sizing,
                       PFS_single_stat, MYF(MY_ZEROFILL));
    if (unlikely(thread_instr_class_waits_array == NULL))
      return 1;

    thread_instr_class_waits_array_start= &thread_instr_class_waits_array[0];
    thread_instr_class_waits_array_end= &thread_instr_class_waits_array[thread_instr_class_waits_sizing];

    for (index= 0; index < thread_instr_class_waits_sizing; index++)
      thread_instr_class_waits_array[index].reset();
  }

  if (thread_stages_history_sizing > 0)
  {
    thread_stages_history_array=
      PFS_MALLOC_ARRAY(thread_stages_history_sizing, PFS_events_stages,
                       MYF(MY_ZEROFILL));
    if (unlikely(thread_stages_history_array == NULL))
      return 1;
  }

  if (thread_instr_class_stages_sizing > 0)
  {
    thread_instr_class_stages_array=
      PFS_MALLOC_ARRAY(thread_instr_class_stages_sizing,
                       PFS_stage_stat, MYF(MY_ZEROFILL));
    if (unlikely(thread_instr_class_stages_array == NULL))
      return 1;

    for (index= 0; index < thread_instr_class_stages_sizing; index++)
      thread_instr_class_stages_array[index].reset();
  }

  if (thread_statements_history_sizing > 0)
  {
    thread_statements_history_array=
      PFS_MALLOC_ARRAY(thread_statements_history_sizing, PFS_events_statements,
                       MYF(MY_ZEROFILL));
    if (unlikely(thread_statements_history_array == NULL))
      return 1;
  }

  if (thread_statements_stack_sizing > 0)
  {
    thread_statements_stack_array=
      PFS_MALLOC_ARRAY(thread_statements_stack_sizing, PFS_events_statements,
                       MYF(MY_ZEROFILL));
    if (unlikely(thread_statements_stack_array == NULL))
      return 1;
  }

  if (thread_instr_class_statements_sizing > 0)
  {
    thread_instr_class_statements_array=
      PFS_MALLOC_ARRAY(thread_instr_class_statements_sizing,
                       PFS_statement_stat, MYF(MY_ZEROFILL));
    if (unlikely(thread_instr_class_statements_array == NULL))
      return 1;

    for (index= 0; index < thread_instr_class_statements_sizing; index++)
      thread_instr_class_statements_array[index].reset();
  }

  if (thread_transactions_history_sizing > 0)
  {
    thread_transactions_history_array=
      PFS_MALLOC_ARRAY(thread_transactions_history_sizing, PFS_events_transactions,
                       MYF(MY_ZEROFILL));
    if (unlikely(thread_transactions_history_array == NULL))
      return 1;
  }

  if (thread_instr_class_transactions_sizing > 0)
  {
    thread_instr_class_transactions_array=
      PFS_MALLOC_ARRAY(thread_instr_class_transactions_sizing,
                       PFS_transaction_stat, MYF(MY_ZEROFILL));
    if (unlikely(thread_instr_class_transactions_array == NULL))
      return 1;

    for (index= 0; index < thread_instr_class_transactions_sizing; index++)
      thread_instr_class_transactions_array[index].reset();
  }

  if (thread_instr_class_memory_sizing > 0)
  {
    thread_instr_class_memory_array=
      PFS_MALLOC_ARRAY(thread_instr_class_memory_sizing,
                       PFS_memory_stat, MYF(MY_ZEROFILL));
    if (unlikely(thread_instr_class_memory_array == NULL))
      return 1;

    for (index= 0; index < thread_instr_class_memory_sizing; index++)
      thread_instr_class_memory_array[index].reset();
  }

  if (thread_session_connect_attrs_sizing > 0)
  {
    thread_session_connect_attrs_array=
      (char *)pfs_malloc(thread_session_connect_attrs_sizing, MYF(MY_ZEROFILL));
    if (unlikely(thread_session_connect_attrs_array == NULL))
      return 1;
  }

  for (index= 0; index < thread_max; index++)
  {
    thread_array[index].m_waits_history=
      &thread_waits_history_array[index * events_waits_history_per_thread];
    thread_array[index].set_instr_class_waits_stats(
      &thread_instr_class_waits_array[index * wait_class_max]);
    thread_array[index].m_stages_history=
      &thread_stages_history_array[index * events_stages_history_per_thread];
    thread_array[index].set_instr_class_stages_stats(
      &thread_instr_class_stages_array[index * stage_class_max]);
    thread_array[index].m_statements_history=
      &thread_statements_history_array[index * events_statements_history_per_thread];
    thread_array[index].m_statement_stack=
      &thread_statements_stack_array[index * statement_stack_max];
    thread_array[index].set_instr_class_statements_stats(
      &thread_instr_class_statements_array[index * statement_class_max]);
    thread_array[index].m_transactions_history=
      &thread_transactions_history_array[index * events_transactions_history_per_thread];
    thread_array[index].set_instr_class_transactions_stats(
      &thread_instr_class_transactions_array[index * transaction_class_max]);
    thread_array[index].set_instr_class_memory_stats(
      &thread_instr_class_memory_array[index * memory_class_max]);
    thread_array[index].m_session_connect_attrs=
      &thread_session_connect_attrs_array[index * session_connect_attrs_size_per_thread];
  }

  if (stage_class_max > 0)
  {
    global_instr_class_stages_array=
      PFS_MALLOC_ARRAY(stage_class_max,
                       PFS_stage_stat, MYF(MY_ZEROFILL));
    if (unlikely(global_instr_class_stages_array == NULL))
      return 1;

    for (index= 0; index < stage_class_max; index++)
      global_instr_class_stages_array[index].reset();
  }

  if (statement_class_max > 0)
  {
    global_instr_class_statements_array=
      PFS_MALLOC_ARRAY(statement_class_max,
                       PFS_statement_stat, MYF(MY_ZEROFILL));
    if (unlikely(global_instr_class_statements_array == NULL))
      return 1;

    for (index= 0; index < statement_class_max; index++)
      global_instr_class_statements_array[index].reset();
  }

  if (memory_class_max > 0)
  {
    global_instr_class_memory_array=
      PFS_MALLOC_ARRAY(memory_class_max,
                       PFS_memory_stat, MYF(MY_ZEROFILL));
    if (unlikely(global_instr_class_memory_array == NULL))
      return 1;

    for (index= 0; index < memory_class_max; index++)
      global_instr_class_memory_array[index].reset();
  }

  return 0;
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
  pfs_free(socket_array);
  socket_array= NULL;
  socket_max= 0;
  pfs_free(metadata_lock_array);
  metadata_lock_array= NULL;
  metadata_lock_max= 0;
  pfs_free(thread_array);
  thread_array= NULL;
  thread_max= 0;
  pfs_free(thread_waits_history_array);
  thread_waits_history_array= NULL;
  pfs_free(thread_stages_history_array);
  thread_stages_history_array= NULL;
  pfs_free(thread_statements_history_array);
  thread_statements_history_array= NULL;
  pfs_free(thread_statements_stack_array);
  thread_statements_stack_array= NULL;
  pfs_free(thread_transactions_history_array);
  thread_transactions_history_array= NULL;
  pfs_free(thread_instr_class_waits_array);
  thread_instr_class_waits_array= NULL;
  pfs_free(thread_instr_class_stages_array);
  thread_instr_class_stages_array= NULL;
  pfs_free(thread_instr_class_statements_array);
  thread_instr_class_statements_array= NULL;
  pfs_free(thread_instr_class_memory_array);
  thread_instr_class_memory_array= NULL;
  pfs_free(thread_instr_class_transactions_array);
  thread_instr_class_transactions_array= NULL;
  pfs_free(global_instr_class_stages_array);
  global_instr_class_stages_array= NULL;
  pfs_free(global_instr_class_statements_array);
  global_instr_class_statements_array= NULL;
  pfs_free(global_instr_class_memory_array);
  global_instr_class_memory_array= NULL;
  pfs_free(thread_session_connect_attrs_array);
  thread_session_connect_attrs_array=NULL;
}

C_MODE_START
/** Get hash table key for instrumented files. */
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
C_MODE_END

/**
  Initialize the file name hash.
  @return 0 on success
*/
int init_file_hash(void)
{
  if ((! filename_hash_inited) && (file_max > 0))
  {
    lf_hash_init(&filename_hash, sizeof(PFS_file*), LF_HASH_UNIQUE,
                 0, 0, filename_hash_get_key, &my_charset_bin);
    filename_hash.size= file_max;
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

/**
  Create instrumentation for a mutex instance.
  @param klass                        the mutex class
  @param identity                     the mutex address
  @return a mutex instance, or NULL
*/
PFS_mutex* create_mutex(PFS_mutex_class *klass, const void *identity)
{
  static PFS_ALIGNED PFS_cacheline_uint32 monotonic;

  uint index;
  uint attempts= 0;
  PFS_mutex *pfs;
  pfs_dirty_state dirty_state;

  if (mutex_full)
  {
    /*
      This is a safety plug.
      When mutex_array is severely undersized,
      do not spin to death for each call.
    */
    mutex_lost++;
    return NULL;
  }

  while (++attempts <= mutex_max)
  {
    /*
      Problem:
      Multiple threads running concurrently may need to create a new
      instrumented mutex, and find an empty slot in mutex_array[].
      With N1 threads running on a N2 core hardware:
      - up to N2 hardware threads can run concurrently,
      causing contention if looking at the same array[i] slot.
      - up to N1 threads can run almost concurrently (with thread scheduling),
      scanning maybe overlapping regions in the [0-mutex_max] array.

      Solution:
      Instead of letting different threads compete on the same array[i] entry,
      this code forces all threads to cooperate with the monotonic_index.
      Only one thread will be allowed to test a given array[i] slot.
      All threads do scan from the same region, starting at monotonic_index.
      Serializing on monotonic_index ensures that when a slot is found occupied
      in a given loop by a given thread, other threads will not attempt this
      slot.
    */
    index= PFS_atomic::add_u32(& monotonic.m_u32, 1) % mutex_max;
    pfs= mutex_array + index;

    if (pfs->m_lock.free_to_dirty(& dirty_state))
    {
      pfs->m_identity= identity;
      pfs->m_class= klass;
      pfs->m_enabled= klass->m_enabled && flag_global_instrumentation;
      pfs->m_timed= klass->m_timed;
      pfs->m_mutex_stat.reset();
      pfs->m_owner= NULL;
      pfs->m_last_locked= 0;
      pfs->m_lock.dirty_to_allocated(& dirty_state);
      if (klass->is_singleton())
        klass->m_singleton= pfs;
      return pfs;
    }
  }

  mutex_lost++;
  /*
    Race condition.
    The mutex_array might not be full if a concurrent thread
    called destroy_mutex() during the scan, leaving one
    empty slot we did not find.
    However, 99.999 percent full tables or 100 percent full tables
    are treated the same here, we declare the array overloaded.
  */
  mutex_full= true;
  return NULL;
}

/**
  Destroy instrumentation for a mutex instance.
  @param pfs                          the mutex to destroy
*/
void destroy_mutex(PFS_mutex *pfs)
{
  DBUG_ASSERT(pfs != NULL);
  PFS_mutex_class *klass= pfs->m_class;
  /* Aggregate to EVENTS_WAITS_SUMMARY_GLOBAL_BY_EVENT_NAME */
  klass->m_mutex_stat.aggregate(& pfs->m_mutex_stat);
  pfs->m_mutex_stat.reset();
  if (klass->is_singleton())
    klass->m_singleton= NULL;
  pfs->m_lock.allocated_to_free();
  mutex_full= false;
}

/**
  Create instrumentation for a rwlock instance.
  @param klass                        the rwlock class
  @param identity                     the rwlock address
  @return a rwlock instance, or NULL
*/
PFS_rwlock* create_rwlock(PFS_rwlock_class *klass, const void *identity)
{
  static PFS_ALIGNED PFS_cacheline_uint32 monotonic;

  uint index;
  uint attempts= 0;
  PFS_rwlock *pfs;
  pfs_dirty_state dirty_state;

  if (rwlock_full)
  {
    rwlock_lost++;
    return NULL;
  }

  while (++attempts <= rwlock_max)
  {
    /* See create_mutex() */
    index= PFS_atomic::add_u32(& monotonic.m_u32, 1) % rwlock_max;
    pfs= rwlock_array + index;

    if (pfs->m_lock.free_to_dirty(& dirty_state))
    {
      pfs->m_identity= identity;
      pfs->m_class= klass;
      pfs->m_enabled= klass->m_enabled && flag_global_instrumentation;
      pfs->m_timed= klass->m_timed;
      pfs->m_rwlock_stat.reset();
      pfs->m_writer= NULL;
      pfs->m_readers= 0;
      pfs->m_last_written= 0;
      pfs->m_last_read= 0;
      pfs->m_lock.dirty_to_allocated(& dirty_state);
      if (klass->is_singleton())
        klass->m_singleton= pfs;
      return pfs;
    }
  }

  rwlock_lost++;
  rwlock_full= true;
  return NULL;
}

/**
  Destroy instrumentation for a rwlock instance.
  @param pfs                          the rwlock to destroy
*/
void destroy_rwlock(PFS_rwlock *pfs)
{
  DBUG_ASSERT(pfs != NULL);
  PFS_rwlock_class *klass= pfs->m_class;
  /* Aggregate to EVENTS_WAITS_SUMMARY_GLOBAL_BY_EVENT_NAME */
  klass->m_rwlock_stat.aggregate(& pfs->m_rwlock_stat);
  pfs->m_rwlock_stat.reset();
  if (klass->is_singleton())
    klass->m_singleton= NULL;
  pfs->m_lock.allocated_to_free();
  rwlock_full= false;
}

/**
  Create instrumentation for a condition instance.
  @param klass                        the condition class
  @param identity                     the condition address
  @return a condition instance, or NULL
*/
PFS_cond* create_cond(PFS_cond_class *klass, const void *identity)
{
  static PFS_ALIGNED PFS_cacheline_uint32 monotonic;

  uint index;
  uint attempts= 0;
  PFS_cond *pfs;
  pfs_dirty_state dirty_state;

  if (cond_full)
  {
    cond_lost++;
    return NULL;
  }

  while (++attempts <= cond_max)
  {
    /* See create_mutex() */
    index= PFS_atomic::add_u32(& monotonic.m_u32, 1) % cond_max;
    pfs= cond_array + index;

    if (pfs->m_lock.free_to_dirty(& dirty_state))
    {
      pfs->m_identity= identity;
      pfs->m_class= klass;
      pfs->m_enabled= klass->m_enabled && flag_global_instrumentation;
      pfs->m_timed= klass->m_timed;
      pfs->m_cond_stat.reset();
      pfs->m_lock.dirty_to_allocated(& dirty_state);
      if (klass->is_singleton())
        klass->m_singleton= pfs;
      return pfs;
    }
  }

  cond_lost++;
  cond_full= true;
  return NULL;
}

/**
  Destroy instrumentation for a condition instance.
  @param pfs                          the condition to destroy
*/
void destroy_cond(PFS_cond *pfs)
{
  DBUG_ASSERT(pfs != NULL);
  PFS_cond_class *klass= pfs->m_class;
  /* Aggregate to EVENTS_WAITS_SUMMARY_GLOBAL_BY_EVENT_NAME */
  klass->m_cond_stat.aggregate(& pfs->m_cond_stat);
  pfs->m_cond_stat.reset();
  if (klass->is_singleton())
    klass->m_singleton= NULL;
  pfs->m_lock.allocated_to_free();
  cond_full= false;
}

PFS_thread* PFS_thread::get_current_thread()
{
  return static_cast<PFS_thread*>(my_get_thread_local(THR_PFS));
}

void PFS_thread::reset_session_connect_attrs()
{
  m_session_connect_attrs_length= 0;
  m_session_connect_attrs_cs_number= 0;

  if ((m_session_connect_attrs != NULL) &&
      (session_connect_attrs_size_per_thread > 0) )
  {
    /* Do not keep user data */
    memset(m_session_connect_attrs, 0, session_connect_attrs_size_per_thread);
  }
}

void PFS_thread::set_enabled(bool enabled)
{
  m_enabled= enabled;
}

void PFS_thread::carry_memory_stat_delta(PFS_memory_stat_delta *delta, uint index)
{
  if (m_account != NULL)
  {
    m_account->carry_memory_stat_delta(delta, index);
    return;
  }

  if (m_user != NULL)
  {
    m_user->carry_memory_stat_delta(delta, index);
    /* do not return, need to process m_host below */
  }

  if (m_host != NULL)
  {
    m_host->carry_memory_stat_delta(delta, index);
    return;
  }

  carry_global_memory_stat_delta(delta, index);
}

void carry_global_memory_stat_delta(PFS_memory_stat_delta *delta, uint index)
{
  PFS_memory_stat *stat;
  PFS_memory_stat_delta delta_buffer;

  stat= & global_instr_class_memory_array[index];
  (void) stat->apply_delta(delta, &delta_buffer);
}

/**
  Create instrumentation for a thread instance.
  @param klass                        the thread class
  @param identity                     the thread address,
    or a value characteristic of this thread
  @param processlist_id               the PROCESSLIST id,
    or 0 if unknown
  @return a thread instance, or NULL
*/
PFS_thread* create_thread(PFS_thread_class *klass, const void *identity,
                          ulonglong processlist_id)
{
  static PFS_ALIGNED PFS_cacheline_uint32 monotonic;

  uint index;
  uint attempts= 0;
  PFS_thread *pfs;
  pfs_dirty_state dirty_state;

  if (thread_full)
  {
    thread_lost++;
    return NULL;
  }

  while (++attempts <= thread_max)
  {
    /* See create_mutex() */
    index= PFS_atomic::add_u32(& monotonic.m_u32, 1) % thread_max;
    pfs= thread_array + index;

    if (pfs->m_lock.free_to_dirty(& dirty_state))
    {
      pfs->m_thread_internal_id=
        PFS_atomic::add_u64(&thread_internal_id_counter.m_u64, 1);
      pfs->m_parent_thread_internal_id= 0;
      pfs->m_processlist_id= static_cast<ulong>(processlist_id);
      pfs->m_event_id= 1;
      pfs->m_stmt_lock.set_allocated();
      pfs->m_session_lock.set_allocated();
      pfs->m_enabled= true;
      pfs->m_class= klass;
      pfs->m_events_waits_current= & pfs->m_events_waits_stack[WAIT_STACK_BOTTOM];
      pfs->m_waits_history_full= false;
      pfs->m_waits_history_index= 0;
      pfs->m_stages_history_full= false;
      pfs->m_stages_history_index= 0;
      pfs->m_statements_history_full= false;
      pfs->m_statements_history_index= 0;
      pfs->m_transactions_history_full= false;
      pfs->m_transactions_history_index= 0;

      pfs->reset_stats();
      pfs->reset_session_connect_attrs();

      pfs->m_filename_hash_pins= NULL;
      pfs->m_table_share_hash_pins= NULL;
      pfs->m_setup_actor_hash_pins= NULL;
      pfs->m_setup_object_hash_pins= NULL;
      pfs->m_user_hash_pins= NULL;
      pfs->m_account_hash_pins= NULL;
      pfs->m_host_hash_pins= NULL;
      pfs->m_digest_hash_pins= NULL;
      pfs->m_program_hash_pins= NULL;

      pfs->m_username_length= 0;
      pfs->m_hostname_length= 0;
      pfs->m_dbname_length= 0;
      pfs->m_command= 0;
      pfs->m_start_time= 0;
      pfs->m_stage= 0;
      pfs->m_stage_progress= NULL;
      pfs->m_processlist_info[0]= '\0';
      pfs->m_processlist_info_length= 0;

      pfs->m_host= NULL;
      pfs->m_user= NULL;
      pfs->m_account= NULL;
      set_thread_account(pfs);

      /*
        For child waits, by default,
        - NESTING_EVENT_ID is NULL
        - NESTING_EVENT_TYPE is NULL
      */
      PFS_events_waits *child_wait= & pfs->m_events_waits_stack[0];
      child_wait->m_event_id= 0;

      /*
        For child stages, by default,
        - NESTING_EVENT_ID is NULL
        - NESTING_EVENT_TYPE is NULL
      */
      PFS_events_stages *child_stage= & pfs->m_stage_current;
      child_stage->m_nesting_event_id= 0;

      pfs->m_events_statements_count= 0;
      pfs->m_transaction_current.m_event_id= 0;

      pfs->m_lock.dirty_to_allocated(& dirty_state);
      return pfs;
    }
  }

  thread_lost++;
  thread_full= true;
  return NULL;
}

PFS_mutex *sanitize_mutex(PFS_mutex *unsafe)
{
  SANITIZE_ARRAY_BODY(PFS_mutex, mutex_array, mutex_max, unsafe);
}

PFS_rwlock *sanitize_rwlock(PFS_rwlock *unsafe)
{
  SANITIZE_ARRAY_BODY(PFS_rwlock, rwlock_array, rwlock_max, unsafe);
}

PFS_cond *sanitize_cond(PFS_cond *unsafe)
{
  SANITIZE_ARRAY_BODY(PFS_cond, cond_array, cond_max, unsafe);
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

PFS_file *sanitize_file(PFS_file *unsafe)
{
  SANITIZE_ARRAY_BODY(PFS_file, file_array, file_max, unsafe);
}

PFS_socket *sanitize_socket(PFS_socket *unsafe)
{
  SANITIZE_ARRAY_BODY(PFS_socket, socket_array, socket_max, unsafe);
}

PFS_metadata_lock *sanitize_metadata_lock(PFS_metadata_lock *unsafe)
{
  SANITIZE_ARRAY_BODY(PFS_metadata_lock, metadata_lock_array, metadata_lock_max, unsafe);
}

/**
  Destroy instrumentation for a thread instance.
  @param pfs                          the thread to destroy
*/
void destroy_thread(PFS_thread *pfs)
{
  DBUG_ASSERT(pfs != NULL);
  pfs->reset_session_connect_attrs();
  if (pfs->m_account != NULL)
  {
    pfs->m_account->release();
    pfs->m_account= NULL;
    DBUG_ASSERT(pfs->m_user == NULL);
    DBUG_ASSERT(pfs->m_host == NULL);
  }
  else
  {
    if (pfs->m_user != NULL)
    {
      pfs->m_user->release();
      pfs->m_user= NULL;
    }
    if (pfs->m_host != NULL)
    {
      pfs->m_host->release();
      pfs->m_host= NULL;
    }
  }
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
  if (pfs->m_setup_actor_hash_pins)
  {
    lf_hash_put_pins(pfs->m_setup_actor_hash_pins);
    pfs->m_setup_actor_hash_pins= NULL;
  }
  if (pfs->m_setup_object_hash_pins)
  {
    lf_hash_put_pins(pfs->m_setup_object_hash_pins);
    pfs->m_setup_object_hash_pins= NULL;
  }
  if (pfs->m_user_hash_pins)
  {
    lf_hash_put_pins(pfs->m_user_hash_pins);
    pfs->m_user_hash_pins= NULL;
  }
  if (pfs->m_account_hash_pins)
  {
    lf_hash_put_pins(pfs->m_account_hash_pins);
    pfs->m_account_hash_pins= NULL;
  }
  if (pfs->m_host_hash_pins)
  {
    lf_hash_put_pins(pfs->m_host_hash_pins);
    pfs->m_host_hash_pins= NULL;
  }
  if (pfs->m_digest_hash_pins)
  {
    lf_hash_put_pins(pfs->m_digest_hash_pins);
    pfs->m_digest_hash_pins= NULL;
  }
  if (pfs->m_program_hash_pins)
  {
    lf_hash_put_pins(pfs->m_program_hash_pins);
    pfs->m_program_hash_pins= NULL;
  }
  pfs->m_lock.allocated_to_free();
  thread_full= false;
}

/**
  Get the hash pins for @c filename_hash.
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
  @param create                       create a file instance if none found
  @return a file instance, or NULL
*/
PFS_file*
find_or_create_file(PFS_thread *thread, PFS_file_class *klass,
                    const char *filename, uint len, bool create)
{
  static PFS_ALIGNED PFS_cacheline_uint32 monotonic;

  PFS_file *pfs;

  DBUG_ASSERT(klass != NULL || ! create);

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
  size_t normalized_length;

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
  uint index;
  uint attempts= 0;
  pfs_dirty_state dirty_state;

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

  if (! create)
  {
    /* No lost counter, just looking for the file existence. */
    return NULL;
  }

  if (file_full)
  {
    file_lost++;
    return NULL;
  }

  while (++attempts <= file_max)
  {
    /* See create_mutex() */
    index= PFS_atomic::add_u32(& monotonic.m_u32, 1) % file_max;
    pfs= file_array + index;

    if (pfs->m_lock.free_to_dirty(& dirty_state))
    {
      pfs->m_class= klass;
      pfs->m_enabled= klass->m_enabled && flag_global_instrumentation;
      pfs->m_timed= klass->m_timed;
      strncpy(pfs->m_filename, normalized_filename, normalized_length);
      pfs->m_filename[normalized_length]= '\0';
      pfs->m_filename_length= normalized_length;
      pfs->m_file_stat.m_open_count= 1;
      pfs->m_file_stat.m_io_stat.reset();
      pfs->m_identity= (const void *)pfs;

      int res;
      pfs->m_lock.dirty_to_allocated(& dirty_state);
      res= lf_hash_insert(&filename_hash, pins,
                          &pfs);
      if (likely(res == 0))
      {
        if (klass->is_singleton())
          klass->m_singleton= pfs;
        return pfs;
      }

      pfs->m_lock.allocated_to_free();

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

  file_lost++;
  file_full= true;
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
  PFS_file_class *klass= pfs->m_class;

  /* Aggregate to FILE_SUMMARY_BY_EVENT_NAME */
  klass->m_file_stat.aggregate(& pfs->m_file_stat);
  pfs->m_file_stat.reset();

  if (klass->is_singleton())
    klass->m_singleton= NULL;

  LF_PINS *pins= get_filename_hash_pins(thread);
  DBUG_ASSERT(pins != NULL);

  lf_hash_delete(&filename_hash, pins,
                 pfs->m_filename, pfs->m_filename_length);
  if (klass->is_singleton())
    klass->m_singleton= NULL;
  pfs->m_lock.allocated_to_free();
  file_full= false;
}

/**
  Create instrumentation for a table instance.
  @param share                        the table share
  @param opening_thread               the opening thread
  @param identity                     the table address
  @return a table instance, or NULL
*/
PFS_table* create_table(PFS_table_share *share, PFS_thread *opening_thread,
                        const void *identity)
{
  static PFS_ALIGNED PFS_cacheline_uint32 monotonic;

  uint index;
  uint attempts= 0;
  PFS_table *pfs;
  pfs_dirty_state dirty_state;

  if (table_full)
  {
    table_lost++;
    return NULL;
  }

  while (++attempts <= table_max)
  {
    /* See create_mutex() */
    index= PFS_atomic::add_u32(& monotonic.m_u32, 1) % table_max;
    pfs= table_array + index;

    if (pfs->m_lock.free_to_dirty(& dirty_state))
    {
      pfs->m_identity= identity;
      pfs->m_share= share;
      pfs->m_io_enabled= share->m_enabled &&
        flag_global_instrumentation && global_table_io_class.m_enabled;
      pfs->m_io_timed= share->m_timed && global_table_io_class.m_timed;
      pfs->m_lock_enabled= share->m_enabled &&
        flag_global_instrumentation && global_table_lock_class.m_enabled;
      pfs->m_lock_timed= share->m_timed && global_table_lock_class.m_timed;
      pfs->m_has_io_stats= false;
      pfs->m_has_lock_stats= false;
      pfs->m_internal_lock= PFS_TL_NONE;
      pfs->m_external_lock= PFS_TL_NONE;
      share->inc_refcount();
      pfs->m_table_stat.fast_reset();
      pfs->m_thread_owner= opening_thread;
      pfs->m_owner_event_id= opening_thread->m_event_id;
      pfs->m_lock.dirty_to_allocated(& dirty_state);
      return pfs;
    }
  }

  table_lost++;
  table_full= true;
  return NULL;
}

void PFS_table::sanitized_aggregate(void)
{
  /*
    This thread could be a TRUNCATE on an aggregated summary table,
    and not own the table handle.
  */
  PFS_table_share *safe_share= sanitize_table_share(m_share);
  if (safe_share != NULL)
  {
    if (m_has_io_stats)
    {
      safe_aggregate_io(NULL, & m_table_stat, safe_share);
      m_has_io_stats= false;
    }
    if (m_has_lock_stats)
    {
      safe_aggregate_lock(& m_table_stat, safe_share);
      m_has_lock_stats= false;
    }
  }
}

void PFS_table::sanitized_aggregate_io(void)
{
  PFS_table_share *safe_share= sanitize_table_share(m_share);
  if (safe_share != NULL && m_has_io_stats)
  {
    safe_aggregate_io(NULL, & m_table_stat, safe_share);
    m_has_io_stats= false;
  }
}

void PFS_table::sanitized_aggregate_lock(void)
{
  PFS_table_share *safe_share= sanitize_table_share(m_share);
  if (safe_share != NULL && m_has_lock_stats)
  {
    safe_aggregate_lock(& m_table_stat, safe_share);
    m_has_lock_stats= false;
  }
}

void PFS_table::safe_aggregate_io(const TABLE_SHARE *optional_server_share,
                                  PFS_table_stat *table_stat,
                                  PFS_table_share *table_share)
{
  DBUG_ASSERT(table_stat != NULL);
  DBUG_ASSERT(table_share != NULL);

  uint key_count= sanitize_index_count(table_share->m_key_count);

  PFS_table_share_index *to_stat;
  PFS_table_io_stat *from_stat;
  uint index;

  DBUG_ASSERT(key_count <= MAX_INDEXES);

  /* Aggregate stats for each index, if any */
  for (index= 0; index < key_count; index++)
  {
    from_stat= & table_stat->m_index_stat[index];
    if (from_stat->m_has_data)
    {
      if (optional_server_share != NULL)
      {
        /*
          An instrumented thread is closing a table,
          and capable of providing index names when
          creating index statistics on the fly.
        */
        to_stat= table_share->find_or_create_index_stat(optional_server_share, index);
      }
      else
      {
        /*
          A monitoring thread, performing TRUNCATE TABLE,
          is asking to flush existing stats from table handles,
          but it does not know about index names used in handles.
          If the index stat already exists, find it and aggregate to it.
          It the index stat does not exist yet, drop the stat and do nothing.
        */
        to_stat= table_share->find_index_stat(index);
      }
      if (to_stat != NULL)
      {
        /* Aggregate to TABLE_IO_SUMMARY */
        to_stat->m_stat.aggregate(from_stat);
      }
    }
  }

  /* Aggregate stats for the table */
  from_stat= & table_stat->m_index_stat[MAX_INDEXES];
  if (from_stat->m_has_data)
  {
    to_stat= table_share->find_or_create_index_stat(NULL, MAX_INDEXES);
    if (to_stat != NULL)
    {
      /* Aggregate to TABLE_IO_SUMMARY */
      to_stat->m_stat.aggregate(from_stat);
    }
  }

  table_stat->fast_reset_io();
}

void PFS_table::safe_aggregate_lock(PFS_table_stat *table_stat,
                                    PFS_table_share *table_share)
{
  DBUG_ASSERT(table_stat != NULL);
  DBUG_ASSERT(table_share != NULL);

  PFS_table_lock_stat *from_stat= & table_stat->m_lock_stat;

  PFS_table_share_lock *to_stat;

  to_stat= table_share->find_or_create_lock_stat();
  if (to_stat != NULL)
  {
    /* Aggregate to TABLE_LOCK_SUMMARY */
    to_stat->m_stat.aggregate(from_stat);
  }

  table_stat->fast_reset_lock();
}

/**
  Destroy instrumentation for a table instance.
  @param pfs                          the table to destroy
*/
void destroy_table(PFS_table *pfs)
{
  DBUG_ASSERT(pfs != NULL);
  pfs->m_share->dec_refcount();
  pfs->m_lock.allocated_to_free();
  table_full= false;
}

/**
  Create instrumentation for a socket instance.
  @param klass                        the socket class
  @param fd                           the socket file descriptor
  @param addr                         the socket address
  @param addr_len                     the socket address length
  @return a socket instance, or NULL
*/
PFS_socket* create_socket(PFS_socket_class *klass, const my_socket *fd,
                          const struct sockaddr *addr, socklen_t addr_len)
{
  static PFS_ALIGNED PFS_cacheline_uint32 monotonic;

  uint index;
  uint attempts= 0;
  PFS_socket *pfs;
  pfs_dirty_state dirty_state;

  if (socket_full)
  {
    socket_lost++;
    return NULL;
  }

  uint fd_used= 0;
  uint addr_len_used= addr_len;

  if (fd != NULL)
    fd_used= *fd;

  if (addr_len_used > sizeof(sockaddr_storage))
    addr_len_used= sizeof(sockaddr_storage);

  while (++attempts <= socket_max)
  {
    index= PFS_atomic::add_u32(& monotonic.m_u32, 1) % socket_max;
    pfs= socket_array + index;

    if (pfs->m_lock.free_to_dirty(& dirty_state))
    {
      pfs->m_fd= fd_used;
      /* There is no socket object, so we use the instrumentation. */
      pfs->m_identity= pfs;
      pfs->m_class= klass;
      pfs->m_enabled= klass->m_enabled && flag_global_instrumentation;
      pfs->m_timed= klass->m_timed;
      pfs->m_idle= false;
      pfs->m_socket_stat.reset();
      pfs->m_thread_owner= NULL;

      pfs->m_addr_len= addr_len_used;
      if ((addr != NULL) && (addr_len_used > 0))
      {
        pfs->m_addr_len= addr_len_used;
        memcpy(&pfs->m_sock_addr, addr, addr_len_used);
      }
      else
      {
        pfs->m_addr_len= 0;
      }

      pfs->m_lock.dirty_to_allocated(& dirty_state);

      if (klass->is_singleton())
        klass->m_singleton= pfs;
      return pfs;
    }
  }

  socket_lost++;
  socket_full= true;
  return NULL;
}

/**
  Destroy instrumentation for a socket instance.
  @param pfs                          the socket to destroy
*/
void destroy_socket(PFS_socket *pfs)
{
  DBUG_ASSERT(pfs != NULL);
  PFS_socket_class *klass= pfs->m_class;

  /* Aggregate to SOCKET_SUMMARY_BY_EVENT_NAME */
  klass->m_socket_stat.m_io_stat.aggregate(&pfs->m_socket_stat.m_io_stat);

  if (klass->is_singleton())
    klass->m_singleton= NULL;

  /* Aggregate to EVENTS_WAITS_SUMMARY_BY_THREAD_BY_EVENT_NAME */
  PFS_thread *thread= pfs->m_thread_owner;
  if (thread != NULL)
  {
    /* Combine stats for all operations */
    PFS_single_stat stat;
    pfs->m_socket_stat.m_io_stat.sum_waits(&stat);
    if (stat.m_count != 0)
    {
      PFS_single_stat *event_name_array;
      event_name_array= thread->write_instr_class_waits_stats();
      uint index= pfs->m_class->m_event_name_index;

      event_name_array[index].aggregate(&stat);
    }
  }

  pfs->m_socket_stat.reset();
  pfs->m_thread_owner= NULL;
  pfs->m_fd= 0;
  pfs->m_addr_len= 0;
  pfs->m_lock.allocated_to_free();
  socket_full= false;
}

PFS_metadata_lock* create_metadata_lock(void *identity,
                                        const MDL_key *mdl_key,
                                        opaque_mdl_type mdl_type,
                                        opaque_mdl_duration mdl_duration,
                                        opaque_mdl_status mdl_status,
                                        const char *src_file,
                                        uint src_line)
{
  static PFS_ALIGNED PFS_cacheline_uint32 monotonic;
  uint index;
  uint attempts= 0;
  PFS_metadata_lock *pfs;
  pfs_dirty_state dirty_state;

  if (metadata_lock_full)
  {
    metadata_lock_lost++;
    return NULL;
  }

  while (++attempts <= metadata_lock_max)
  {
    /* See create_mutex() */
    index= PFS_atomic::add_u32(& monotonic.m_u32, 1) % metadata_lock_max;
    pfs= metadata_lock_array + index;

    if (pfs->m_lock.free_to_dirty(& dirty_state))
    {
      pfs->m_identity= identity;
      pfs->m_enabled= global_metadata_class.m_enabled && flag_global_instrumentation;
      pfs->m_timed= global_metadata_class.m_timed;
      pfs->m_mdl_key.mdl_key_init(mdl_key);
      pfs->m_mdl_type= mdl_type;
      pfs->m_mdl_duration= mdl_duration;
      pfs->m_mdl_status= mdl_status;
      pfs->m_src_file= src_file;
      pfs->m_src_line= src_line;
      pfs->m_owner_thread_id= 0;
      pfs->m_owner_event_id= 0;
      pfs->m_lock.dirty_to_allocated(& dirty_state);
      return pfs;
    }
  }

  metadata_lock_lost++;
  metadata_lock_full= true;
  return NULL;
}

void destroy_metadata_lock(PFS_metadata_lock *pfs)
{
  DBUG_ASSERT(pfs != NULL);
  pfs->m_lock.allocated_to_free();
}

static void reset_mutex_waits_by_instance(void)
{
  PFS_mutex *pfs= mutex_array;
  PFS_mutex *pfs_last= mutex_array + mutex_max;

  for ( ; pfs < pfs_last; pfs++)
    pfs->m_mutex_stat.reset();
}

static void reset_rwlock_waits_by_instance(void)
{
  PFS_rwlock *pfs= rwlock_array;
  PFS_rwlock *pfs_last= rwlock_array + rwlock_max;

  for ( ; pfs < pfs_last; pfs++)
    pfs->m_rwlock_stat.reset();
}

static void reset_cond_waits_by_instance(void)
{
  PFS_cond *pfs= cond_array;
  PFS_cond *pfs_last= cond_array + cond_max;

  for ( ; pfs < pfs_last; pfs++)
    pfs->m_cond_stat.reset();
}

static void reset_file_waits_by_instance(void)
{
  PFS_file *pfs= file_array;
  PFS_file *pfs_last= file_array + file_max;

  for ( ; pfs < pfs_last; pfs++)
    pfs->m_file_stat.reset();
}

static void reset_socket_waits_by_instance(void)
{
  PFS_socket *pfs= socket_array;
  PFS_socket *pfs_last= socket_array + socket_max;

  for ( ; pfs < pfs_last; pfs++)
    pfs->m_socket_stat.reset();
}

/** Reset the wait statistics per object instance. */
void reset_events_waits_by_instance(void)
{
  reset_mutex_waits_by_instance();
  reset_rwlock_waits_by_instance();
  reset_cond_waits_by_instance();
  reset_file_waits_by_instance();
  reset_socket_waits_by_instance();
}

/** Reset the io statistics per file instance. */
void reset_file_instance_io(void)
{
  PFS_file *pfs= file_array;
  PFS_file *pfs_last= file_array + file_max;

  for ( ; pfs < pfs_last; pfs++)
    pfs->m_file_stat.m_io_stat.reset();
}

/** Reset the io statistics per socket instance. */
void reset_socket_instance_io(void)
{
  PFS_socket *pfs= socket_array;
  PFS_socket *pfs_last= socket_array + socket_max;

  for ( ; pfs < pfs_last; pfs++)
    pfs->m_socket_stat.m_io_stat.reset();
}

void aggregate_all_event_names(PFS_single_stat *from_array,
                               PFS_single_stat *to_array)
{
  PFS_single_stat *from;
  PFS_single_stat *from_last;
  PFS_single_stat *to;

  from= from_array;
  from_last= from_array + wait_class_max;
  to= to_array;

  for ( ; from < from_last ; from++, to++)
  {
    if (from->m_count > 0)
    {
      to->aggregate(from);
      from->reset();
    }
  }
}

void aggregate_all_event_names(PFS_single_stat *from_array,
                               PFS_single_stat *to_array_1,
                               PFS_single_stat *to_array_2)
{
  PFS_single_stat *from;
  PFS_single_stat *from_last;
  PFS_single_stat *to_1;
  PFS_single_stat *to_2;

  from= from_array;
  from_last= from_array + wait_class_max;
  to_1= to_array_1;
  to_2= to_array_2;

  for ( ; from < from_last ; from++, to_1++, to_2++)
  {
    if (from->m_count > 0)
    {
      to_1->aggregate(from);
      to_2->aggregate(from);
      from->reset();
    }
  }
}

void aggregate_all_stages(PFS_stage_stat *from_array,
                          PFS_stage_stat *to_array)
{
  PFS_stage_stat *from;
  PFS_stage_stat *from_last;
  PFS_stage_stat *to;

  from= from_array;
  from_last= from_array + stage_class_max;
  to= to_array;

  for ( ; from < from_last ; from++, to++)
  {
    if (from->m_timer1_stat.m_count > 0)
    {
      to->aggregate(from);
      from->reset();
    }
  }
}

void aggregate_all_stages(PFS_stage_stat *from_array,
                          PFS_stage_stat *to_array_1,
                          PFS_stage_stat *to_array_2)
{
  PFS_stage_stat *from;
  PFS_stage_stat *from_last;
  PFS_stage_stat *to_1;
  PFS_stage_stat *to_2;

  from= from_array;
  from_last= from_array + stage_class_max;
  to_1= to_array_1;
  to_2= to_array_2;

  for ( ; from < from_last ; from++, to_1++, to_2++)
  {
    if (from->m_timer1_stat.m_count > 0)
    {
      to_1->aggregate(from);
      to_2->aggregate(from);
      from->reset();
    }
  }
}

void aggregate_all_statements(PFS_statement_stat *from_array,
                              PFS_statement_stat *to_array)
{
  PFS_statement_stat *from;
  PFS_statement_stat *from_last;
  PFS_statement_stat *to;

  from= from_array;
  from_last= from_array + statement_class_max;
  to= to_array;

  for ( ; from < from_last ; from++, to++)
  {
    if (from->m_timer1_stat.m_count > 0)
    {
      to->aggregate(from);
      from->reset();
    }
  }
}

void aggregate_all_statements(PFS_statement_stat *from_array,
                              PFS_statement_stat *to_array_1,
                              PFS_statement_stat *to_array_2)
{
  PFS_statement_stat *from;
  PFS_statement_stat *from_last;
  PFS_statement_stat *to_1;
  PFS_statement_stat *to_2;

  from= from_array;
  from_last= from_array + statement_class_max;
  to_1= to_array_1;
  to_2= to_array_2;

  for ( ; from < from_last ; from++, to_1++, to_2++)
  {
    if (from->m_timer1_stat.m_count > 0)
    {
      to_1->aggregate(from);
      to_2->aggregate(from);
      from->reset();
    }
  }
}

void aggregate_all_transactions(PFS_transaction_stat *from_array,
                                PFS_transaction_stat *to_array)
{
  DBUG_ASSERT(from_array != NULL);
  DBUG_ASSERT(to_array != NULL);

  if (from_array->count() > 0)
  {
    to_array->aggregate(from_array);
    from_array->reset();
  }
}

void aggregate_all_transactions(PFS_transaction_stat *from_array,
                                PFS_transaction_stat *to_array_1,
                                PFS_transaction_stat *to_array_2)
{
  DBUG_ASSERT(from_array != NULL);
  DBUG_ASSERT(to_array_1 != NULL);
  DBUG_ASSERT(to_array_2 != NULL);

  if (from_array->count() > 0)
  {
    to_array_1->aggregate(from_array);
    to_array_2->aggregate(from_array);
    from_array->reset();
  }
}

void aggregate_all_memory(bool alive,
                          PFS_memory_stat *from_array,
                          PFS_memory_stat *to_array)
{
  PFS_memory_stat *from;
  PFS_memory_stat *from_last;
  PFS_memory_stat *to;

  from= from_array;
  from_last= from_array + memory_class_max;
  to= to_array;

  if (alive)
  {
    for ( ; from < from_last ; from++, to++)
    {
      from->partial_aggregate_to(to);
    }
  }
  else
  {
    for ( ; from < from_last ; from++, to++)
    {
      from->full_aggregate_to(to);
      from->reset();
    }
  }
}

void aggregate_all_memory(bool alive,
                          PFS_memory_stat *from_array,
                          PFS_memory_stat *to_array_1,
                          PFS_memory_stat *to_array_2)
{
  PFS_memory_stat *from;
  PFS_memory_stat *from_last;
  PFS_memory_stat *to_1;
  PFS_memory_stat *to_2;

  from= from_array;
  from_last= from_array + memory_class_max;
  to_1= to_array_1;
  to_2= to_array_2;

  if (alive)
  {
    for ( ; from < from_last ; from++, to_1++, to_2++)
    {
      from->partial_aggregate_to(to_1, to_2);
    }
  }
  else
  {
    for ( ; from < from_last ; from++, to_1++, to_2++)
    {
      from->full_aggregate_to(to_1, to_2);
      from->reset();
    }
  }
}

void aggregate_thread_stats(PFS_thread *thread,
                            PFS_account *safe_account,
                            PFS_user *safe_user,
                            PFS_host *safe_host)
{
  if (likely(safe_account != NULL))
  {
    safe_account->m_disconnected_count++;
    return;
  }

  if (safe_user != NULL)
    safe_user->m_disconnected_count++;

  if (safe_host != NULL)
    safe_host->m_disconnected_count++;

  /* There is no global table for connections statistics. */
  return;
}

void aggregate_thread(PFS_thread *thread,
                      PFS_account *safe_account,
                      PFS_user *safe_user,
                      PFS_host *safe_host)
{
  /* No HAVE_PSI_???_INTERFACE flag, waits cover multiple instrumentations */
  aggregate_thread_waits(thread, safe_account, safe_user, safe_host);

#ifdef HAVE_PSI_STAGE_INTERFACE
  aggregate_thread_stages(thread, safe_account, safe_user, safe_host);
#endif

#ifdef HAVE_PSI_STATEMENT_INTERFACE
  aggregate_thread_statements(thread, safe_account, safe_user, safe_host);
#endif

#ifdef HAVE_PSI_TRANSACTION_INTERFACE
  aggregate_thread_transactions(thread, safe_account, safe_user, safe_host);
#endif

#ifdef HAVE_PSI_MEMORY_INTERFACE
  aggregate_thread_memory(false, thread, safe_account, safe_user, safe_host);
#endif

  aggregate_thread_stats(thread, safe_account, safe_user, safe_host);
}

void aggregate_thread_waits(PFS_thread *thread,
                            PFS_account *safe_account,
                            PFS_user *safe_user,
                            PFS_host *safe_host)
{
  if (thread->read_instr_class_waits_stats() == NULL)
    return;

  if (likely(safe_account != NULL))
  {
    /*
      Aggregate EVENTS_WAITS_SUMMARY_BY_THREAD_BY_EVENT_NAME
      to EVENTS_WAITS_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME.
    */
    aggregate_all_event_names(thread->write_instr_class_waits_stats(),
                              safe_account->write_instr_class_waits_stats());

    return;
  }

  if ((safe_user != NULL) && (safe_host != NULL))
  {
    /*
      Aggregate EVENTS_WAITS_SUMMARY_BY_THREAD_BY_EVENT_NAME to:
      -  EVENTS_WAITS_SUMMARY_BY_USER_BY_EVENT_NAME
      -  EVENTS_WAITS_SUMMARY_BY_HOST_BY_EVENT_NAME
      in parallel.
    */
    aggregate_all_event_names(thread->write_instr_class_waits_stats(),
                              safe_user->write_instr_class_waits_stats(),
                              safe_host->write_instr_class_waits_stats());
    return;
  }

  if (safe_user != NULL)
  {
    /*
      Aggregate EVENTS_WAITS_SUMMARY_BY_THREAD_BY_EVENT_NAME
      to EVENTS_WAITS_SUMMARY_BY_USER_BY_EVENT_NAME, directly.
    */
    aggregate_all_event_names(thread->write_instr_class_waits_stats(),
                              safe_user->write_instr_class_waits_stats());
    return;
  }

  if (safe_host != NULL)
  {
    /*
      Aggregate EVENTS_WAITS_SUMMARY_BY_THREAD_BY_EVENT_NAME
      to EVENTS_WAITS_SUMMARY_BY_HOST_BY_EVENT_NAME, directly.
    */
    aggregate_all_event_names(thread->write_instr_class_waits_stats(),
                              safe_host->write_instr_class_waits_stats());
    return;
  }

  /* Orphan thread, clean the waits stats. */
  thread->reset_waits_stats();
}

void aggregate_thread_stages(PFS_thread *thread,
                             PFS_account *safe_account,
                             PFS_user *safe_user,
                             PFS_host *safe_host)
{
  if (thread->read_instr_class_stages_stats() == NULL)
    return;

  if (likely(safe_account != NULL))
  {
    /*
      Aggregate EVENTS_STAGES_SUMMARY_BY_THREAD_BY_EVENT_NAME
      to EVENTS_STAGES_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME.
    */
    aggregate_all_stages(thread->write_instr_class_stages_stats(),
                         safe_account->write_instr_class_stages_stats());

    return;
  }

  if ((safe_user != NULL) && (safe_host != NULL))
  {
    /*
      Aggregate EVENTS_STAGES_SUMMARY_BY_THREAD_BY_EVENT_NAME to:
      -  EVENTS_STAGES_SUMMARY_BY_USER_BY_EVENT_NAME
      -  EVENTS_STAGES_SUMMARY_BY_HOST_BY_EVENT_NAME
      in parallel.
    */
    aggregate_all_stages(thread->write_instr_class_stages_stats(),
                         safe_user->write_instr_class_stages_stats(),
                         safe_host->write_instr_class_stages_stats());
    return;
  }

  if (safe_user != NULL)
  {
    /*
      Aggregate EVENTS_STAGES_SUMMARY_BY_THREAD_BY_EVENT_NAME to:
      -  EVENTS_STAGES_SUMMARY_BY_USER_BY_EVENT_NAME
      -  EVENTS_STAGES_SUMMARY_GLOBAL_BY_EVENT_NAME
      in parallel.
    */
    aggregate_all_stages(thread->write_instr_class_stages_stats(),
                         safe_user->write_instr_class_stages_stats(),
                         global_instr_class_stages_array);
    return;
  }

  if (safe_host != NULL)
  {
    /*
      Aggregate EVENTS_STAGES_SUMMARY_BY_THREAD_BY_EVENT_NAME
      to EVENTS_STAGES_SUMMARY_BY_HOST_BY_EVENT_NAME, directly.
    */
    aggregate_all_stages(thread->write_instr_class_stages_stats(),
                         safe_host->write_instr_class_stages_stats());
    return;
  }

  /*
    Aggregate EVENTS_STAGES_SUMMARY_BY_THREAD_BY_EVENT_NAME
    to EVENTS_STAGES_SUMMARY_GLOBAL_BY_EVENT_NAME.
  */
  aggregate_all_stages(thread->write_instr_class_stages_stats(),
                       global_instr_class_stages_array);
}

void aggregate_thread_statements(PFS_thread *thread,
                                 PFS_account *safe_account,
                                 PFS_user *safe_user,
                                 PFS_host *safe_host)
{
  if (thread->read_instr_class_statements_stats() == NULL)
    return;

  if (likely(safe_account != NULL))
  {
    /*
      Aggregate EVENTS_STATEMENTS_SUMMARY_BY_THREAD_BY_EVENT_NAME
      to EVENTS_STATEMENTS_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME.
    */
    aggregate_all_statements(thread->write_instr_class_statements_stats(),
                             safe_account->write_instr_class_statements_stats());

    return;
  }

  if ((safe_user != NULL) && (safe_host != NULL))
  {
    /*
      Aggregate EVENTS_STATEMENT_SUMMARY_BY_THREAD_BY_EVENT_NAME to:
      -  EVENTS_STATEMENT_SUMMARY_BY_USER_BY_EVENT_NAME
      -  EVENTS_STATEMENT_SUMMARY_BY_HOST_BY_EVENT_NAME
      in parallel.
    */
    aggregate_all_statements(thread->write_instr_class_statements_stats(),
                             safe_user->write_instr_class_statements_stats(),
                             safe_host->write_instr_class_statements_stats());
    return;
  }

  if (safe_user != NULL)
  {
    /*
      Aggregate EVENTS_STATEMENTS_SUMMARY_BY_THREAD_BY_EVENT_NAME to:
      -  EVENTS_STATEMENTS_SUMMARY_BY_USER_BY_EVENT_NAME
      -  EVENTS_STATEMENTS_SUMMARY_GLOBAL_BY_EVENT_NAME
      in parallel.
    */
    aggregate_all_statements(thread->write_instr_class_statements_stats(),
                             safe_user->write_instr_class_statements_stats(),
                             global_instr_class_statements_array);
    return;
  }

  if (safe_host != NULL)
  {
    /*
      Aggregate EVENTS_STATEMENTS_SUMMARY_BY_THREAD_BY_EVENT_NAME
      to EVENTS_STATEMENTS_SUMMARY_BY_HOST_BY_EVENT_NAME, directly.
    */
    aggregate_all_statements(thread->write_instr_class_statements_stats(),
                             safe_host->write_instr_class_statements_stats());
    return;
  }

  /*
    Aggregate EVENTS_STATEMENTS_SUMMARY_BY_THREAD_BY_EVENT_NAME
    to EVENTS_STATEMENTS_SUMMARY_GLOBAL_BY_EVENT_NAME.
  */
  aggregate_all_statements(thread->write_instr_class_statements_stats(),
                           global_instr_class_statements_array);
}

void aggregate_thread_transactions(PFS_thread *thread,
                                   PFS_account *safe_account,
                                   PFS_user *safe_user,
                                   PFS_host *safe_host)
{
  if (thread->read_instr_class_transactions_stats() == NULL)
    return;

  if (likely(safe_account != NULL))
  {
    /*
      Aggregate EVENTS_TRANSACTIONS_SUMMARY_BY_THREAD_BY_EVENT_NAME
      to EVENTS_TRANSACTIONS_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME.
    */
    aggregate_all_transactions(thread->write_instr_class_transactions_stats(),
                               safe_account->write_instr_class_transactions_stats());

    return;
  }

  if ((safe_user != NULL) && (safe_host != NULL))
  {
    /*
      Aggregate EVENTS_TRANSACTION_SUMMARY_BY_THREAD_BY_EVENT_NAME to:
      -  EVENTS_TRANSACTION_SUMMARY_BY_USER_BY_EVENT_NAME
      -  EVENTS_TRANSACTION_SUMMARY_BY_HOST_BY_EVENT_NAME
      in parallel.
    */
    aggregate_all_transactions(thread->write_instr_class_transactions_stats(),
                               safe_user->write_instr_class_transactions_stats(),
                               safe_host->write_instr_class_transactions_stats());
    return;
  }

  if (safe_user != NULL)
  {
    /*
      Aggregate EVENTS_TRANSACTIONS_SUMMARY_BY_THREAD_BY_EVENT_NAME to:
      -  EVENTS_TRANSACTIONS_SUMMARY_BY_USER_BY_EVENT_NAME
      -  EVENTS_TRANSACTIONS_SUMMARY_GLOBAL_BY_EVENT_NAME
      in parallel.
    */
    aggregate_all_transactions(thread->write_instr_class_transactions_stats(),
                               safe_user->write_instr_class_transactions_stats(),
                               &global_transaction_stat);
    return;
  }

  if (safe_host != NULL)
  {
    /*
      Aggregate EVENTS_TRANSACTIONS_SUMMARY_BY_THREAD_BY_EVENT_NAME
      to EVENTS_TRANSACTIONS_SUMMARY_BY_HOST_BY_EVENT_NAME, directly.
    */
    aggregate_all_transactions(thread->write_instr_class_transactions_stats(),
                               safe_host->write_instr_class_transactions_stats());
    return;
  }

  /*
    Aggregate EVENTS_TRANSACTIONS_SUMMARY_BY_THREAD_BY_EVENT_NAME
    to EVENTS_TRANSACTIONS_SUMMARY_GLOBAL_BY_EVENT_NAME.
  */
  aggregate_all_transactions(thread->write_instr_class_transactions_stats(),
                             &global_transaction_stat);
}

void aggregate_thread_memory(bool alive, PFS_thread *thread,
                             PFS_account *safe_account,
                             PFS_user *safe_user,
                             PFS_host *safe_host)
{
  if (thread->read_instr_class_memory_stats() == NULL)
    return;

  if (likely(safe_account != NULL))
  {
    /*
      Aggregate MEMORY_SUMMARY_BY_THREAD_BY_EVENT_NAME
      to MEMORY_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME.
    */
    aggregate_all_memory(alive,
                         thread->write_instr_class_memory_stats(),
                         safe_account->write_instr_class_memory_stats());

    return;
  }

  if ((safe_user != NULL) && (safe_host != NULL))
  {
    /*
      Aggregate MEMORY_SUMMARY_BY_THREAD_BY_EVENT_NAME to:
      -  MEMORY_SUMMARY_BY_USER_BY_EVENT_NAME
      -  MEMORY_SUMMARY_BY_HOST_BY_EVENT_NAME
      in parallel.
    */
    aggregate_all_memory(alive,
                         thread->write_instr_class_memory_stats(),
                         safe_user->write_instr_class_memory_stats(),
                         safe_host->write_instr_class_memory_stats());
    return;
  }

  if (safe_user != NULL)
  {
    /*
      Aggregate MEMORY_SUMMARY_BY_THREAD_BY_EVENT_NAME to:
      -  MEMORY_SUMMARY_BY_USER_BY_EVENT_NAME
      -  MEMORY_SUMMARY_GLOBAL_BY_EVENT_NAME
      in parallel.
    */
    aggregate_all_memory(alive,
                         thread->write_instr_class_memory_stats(),
                         safe_user->write_instr_class_memory_stats(),
                         global_instr_class_memory_array);
    return;
  }

  if (safe_host != NULL)
  {
    /*
      Aggregate MEMORY_SUMMARY_BY_THREAD_BY_EVENT_NAME
      to MEMORY_SUMMARY_BY_HOST_BY_EVENT_NAME, directly.
    */
    aggregate_all_memory(alive,
                         thread->write_instr_class_memory_stats(),
                         safe_host->write_instr_class_memory_stats());
    return;
  }

  /*
    Aggregate MEMORY_SUMMARY_BY_THREAD_BY_EVENT_NAME
    to MEMORY_SUMMARY_GLOBAL_BY_EVENT_NAME.
  */
  aggregate_all_memory(alive,
                       thread->write_instr_class_memory_stats(),
                       global_instr_class_memory_array);
}

void clear_thread_account(PFS_thread *thread)
{
  if (thread->m_account != NULL)
  {
    thread->m_account->release();
    thread->m_account= NULL;
  }

  if (thread->m_user != NULL)
  {
    thread->m_user->release();
    thread->m_user= NULL;
  }

  if (thread->m_host != NULL)
  {
    thread->m_host->release();
    thread->m_host= NULL;
  }
}

void set_thread_account(PFS_thread *thread)
{
  DBUG_ASSERT(thread->m_account == NULL);
  DBUG_ASSERT(thread->m_user == NULL);
  DBUG_ASSERT(thread->m_host == NULL);

  thread->m_account= find_or_create_account(thread,
                                                thread->m_username,
                                                thread->m_username_length,
                                                thread->m_hostname,
                                                thread->m_hostname_length);

  if ((thread->m_account == NULL) && (thread->m_username_length > 0))
    thread->m_user= find_or_create_user(thread,
                                        thread->m_username,
                                        thread->m_username_length);

  if ((thread->m_account == NULL) && (thread->m_hostname_length > 0))
    thread->m_host= find_or_create_host(thread,
                                        thread->m_hostname,
                                        thread->m_hostname_length);
}

void update_mutex_derived_flags()
{
  PFS_mutex *pfs= mutex_array;
  PFS_mutex *pfs_last= mutex_array + mutex_max;
  PFS_mutex_class *klass;

  for ( ; pfs < pfs_last; pfs++)
  {
    klass= sanitize_mutex_class(pfs->m_class);
    if (likely(klass != NULL))
    {
      pfs->m_enabled= klass->m_enabled && flag_global_instrumentation;
      pfs->m_timed= klass->m_timed;
    }
    else
    {
      pfs->m_enabled= false;
      pfs->m_timed= false;
    }
  }
}

void update_rwlock_derived_flags()
{
  PFS_rwlock *pfs= rwlock_array;
  PFS_rwlock *pfs_last= rwlock_array + rwlock_max;
  PFS_rwlock_class *klass;

  for ( ; pfs < pfs_last; pfs++)
  {
    klass= sanitize_rwlock_class(pfs->m_class);
    if (likely(klass != NULL))
    {
      pfs->m_enabled= klass->m_enabled && flag_global_instrumentation;
      pfs->m_timed= klass->m_timed;
    }
    else
    {
      pfs->m_enabled= false;
      pfs->m_timed= false;
    }
  }
}

void update_cond_derived_flags()
{
  PFS_cond *pfs= cond_array;
  PFS_cond *pfs_last= cond_array + cond_max;
  PFS_cond_class *klass;

  for ( ; pfs < pfs_last; pfs++)
  {
    klass= sanitize_cond_class(pfs->m_class);
    if (likely(klass != NULL))
    {
      pfs->m_enabled= klass->m_enabled && flag_global_instrumentation;
      pfs->m_timed= klass->m_timed;
    }
    else
    {
      pfs->m_enabled= false;
      pfs->m_timed= false;
    }
  }
}

void update_file_derived_flags()
{
  PFS_file *pfs= file_array;
  PFS_file *pfs_last= file_array + file_max;
  PFS_file_class *klass;

  for ( ; pfs < pfs_last; pfs++)
  {
    klass= sanitize_file_class(pfs->m_class);
    if (likely(klass != NULL))
    {
      pfs->m_enabled= klass->m_enabled && flag_global_instrumentation;
      pfs->m_timed= klass->m_timed;
    }
    else
    {
      pfs->m_enabled= false;
      pfs->m_timed= false;
    }
  }
}

void update_table_derived_flags()
{
  PFS_table *pfs= table_array;
  PFS_table *pfs_last= table_array + table_max;
  PFS_table_share *share;

  for ( ; pfs < pfs_last; pfs++)
  {
    share= sanitize_table_share(pfs->m_share);
    if (likely(share != NULL))
    {
      pfs->m_io_enabled= share->m_enabled &&
        flag_global_instrumentation && global_table_io_class.m_enabled;
      pfs->m_io_timed= share->m_timed && global_table_io_class.m_timed;
      pfs->m_lock_enabled= share->m_enabled &&
        flag_global_instrumentation && global_table_lock_class.m_enabled;
      pfs->m_lock_timed= share->m_timed && global_table_lock_class.m_timed;
    }
    else
    {
      pfs->m_io_enabled= false;
      pfs->m_io_timed= false;
      pfs->m_lock_enabled= false;
      pfs->m_lock_timed= false;
    }
  }
}

void update_socket_derived_flags()
{
  PFS_socket *pfs= socket_array;
  PFS_socket *pfs_last= socket_array + socket_max;
  PFS_socket_class *klass;

  for ( ; pfs < pfs_last; pfs++)
  {
    klass= sanitize_socket_class(pfs->m_class);
    if (likely(klass != NULL))
    {
      pfs->m_enabled= klass->m_enabled && flag_global_instrumentation;
      pfs->m_timed= klass->m_timed;
    }
    else
    {
      pfs->m_enabled= false;
      pfs->m_timed= false;
    }
  }
}

void update_metadata_derived_flags()
{
  PFS_metadata_lock *pfs= metadata_lock_array;
  PFS_metadata_lock *pfs_last= metadata_lock_array + metadata_lock_max;

  for ( ; pfs < pfs_last; pfs++)
  {
    pfs->m_enabled= global_metadata_class.m_enabled && flag_global_instrumentation;
    pfs->m_timed= global_metadata_class.m_timed;
  }
}

void update_thread_derived_flags()
{
  /* None */
}

void update_instruments_derived_flags()
{
  update_mutex_derived_flags();
  update_rwlock_derived_flags();
  update_cond_derived_flags();
  update_file_derived_flags();
  update_table_derived_flags();
  update_socket_derived_flags();
  update_metadata_derived_flags();
  /* nothing for stages, statements and transactions (no instances) */
}

/** @} */
