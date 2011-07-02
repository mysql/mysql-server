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
  @file storage/perfschema/pfs_instr_class.cc
  Performance schema instruments meta data (implementation).
*/

#include "my_global.h"
#include "my_sys.h"
#include "pfs_instr_class.h"
#include "pfs_instr.h"
#include "pfs_global.h"
#include "pfs_events_waits.h"
#include "pfs_atomic.h"
#include "mysql/psi/mysql_thread.h"
#include "lf.h"

#include <string.h>

/**
  @defgroup Performance_schema_buffers Performance Schema Buffers
  @ingroup Performance_schema_implementation
  @{
*/

/**
  Global performance schema flag.
  Indicate if the performance schema is enabled.
  This flag is set at startup, and never changes.
*/
my_bool pfs_enabled= TRUE;

/**
  Current number of elements in mutex_class_array.
  This global variable is written to during:
  - the performance schema initialization
  - a plugin initialization
*/
static volatile uint32 mutex_class_dirty_count= 0;
static volatile uint32 mutex_class_allocated_count= 0;
static volatile uint32 rwlock_class_dirty_count= 0;
static volatile uint32 rwlock_class_allocated_count= 0;
static volatile uint32 cond_class_dirty_count= 0;
static volatile uint32 cond_class_allocated_count= 0;

/** Size of the mutex class array. @sa mutex_class_array */
ulong mutex_class_max= 0;
/** Number of mutex class lost. @sa mutex_class_array */
ulong mutex_class_lost= 0;
/** Size of the rwlock class array. @sa rwlock_class_array */
ulong rwlock_class_max= 0;
/** Number of rwlock class lost. @sa rwlock_class_array */
ulong rwlock_class_lost= 0;
/** Size of the condition class array. @sa cond_class_array */
ulong cond_class_max= 0;
/** Number of condition class lost. @sa cond_class_array */
ulong cond_class_lost= 0;
/** Size of the thread class array. @sa thread_class_array */
ulong thread_class_max= 0;
/** Number of thread class lost. @sa thread_class_array */
ulong thread_class_lost= 0;
/** Size of the file class array. @sa file_class_array */
ulong file_class_max= 0;
/** Number of file class lost. @sa file_class_array */
ulong file_class_lost= 0;
/** Size of the table share array. @sa table_share_array */
ulong table_share_max= 0;
/** Number of table share lost. @sa table_share_array */
ulong table_share_lost= 0;

static PFS_mutex_class *mutex_class_array= NULL;
static PFS_rwlock_class *rwlock_class_array= NULL;
static PFS_cond_class *cond_class_array= NULL;

/**
  Current number or elements in thread_class_array.
  This global variable is written to during:
  - the performance schema initialization
  - a plugin initialization
*/
static volatile uint32 thread_class_dirty_count= 0;
static volatile uint32 thread_class_allocated_count= 0;

static PFS_thread_class *thread_class_array= NULL;

/**
  Table instance array.
  @sa table_share_max
  @sa table_share_lost
  @sa table_share_hash
*/
PFS_table_share *table_share_array= NULL;

PFS_instr_class global_table_class=
{
  "wait/table", /* name */
  10, /* name length */
  0, /* flags */
  true, /* enabled */
  true, /* timed */
  { &flag_events_waits_current, NULL, 0, 0, 0, 0} /* wait stat chain */
};

/** Hash table for instrumented tables.  */
static LF_HASH table_share_hash;
/** True if table_share_hash is initialized. */
static bool table_share_hash_inited= false;
C_MODE_START
/** Get hash table key for instrumented tables. */
static uchar *table_share_hash_get_key(const uchar *, size_t *, my_bool);
C_MODE_END

static volatile uint32 file_class_dirty_count= 0;
static volatile uint32 file_class_allocated_count= 0;

static PFS_file_class *file_class_array= NULL;

/**
  Initialize the instrument synch class buffers.
  @param mutex_class_sizing           max number of mutex class
  @param rwlock_class_sizing          max number of rwlock class
  @param cond_class_sizing            max number of condition class
  @return 0 on success
*/
int init_sync_class(uint mutex_class_sizing,
                    uint rwlock_class_sizing,
                    uint cond_class_sizing)
{
  mutex_class_dirty_count= mutex_class_allocated_count= 0;
  rwlock_class_dirty_count= rwlock_class_allocated_count= 0;
  cond_class_dirty_count= cond_class_allocated_count= 0;
  mutex_class_max= mutex_class_sizing;
  rwlock_class_max= rwlock_class_sizing;
  cond_class_max= cond_class_sizing;
  mutex_class_lost= rwlock_class_lost= cond_class_lost= 0;

  mutex_class_array= NULL;
  rwlock_class_array= NULL;
  cond_class_array= NULL;

  if (mutex_class_max > 0)
  {
    mutex_class_array= PFS_MALLOC_ARRAY(mutex_class_max, PFS_mutex_class,
                                        MYF(MY_ZEROFILL));
    if (unlikely(mutex_class_array == NULL))
      return 1;
  }

  if (rwlock_class_max > 0)
  {
    rwlock_class_array= PFS_MALLOC_ARRAY(rwlock_class_max, PFS_rwlock_class,
                                         MYF(MY_ZEROFILL));
    if (unlikely(rwlock_class_array == NULL))
      return 1;
  }

  if (cond_class_max > 0)
  {
    cond_class_array= PFS_MALLOC_ARRAY(cond_class_max, PFS_cond_class,
                                       MYF(MY_ZEROFILL));
    if (unlikely(cond_class_array == NULL))
      return 1;
  }

  return 0;
}

/** Cleanup the instrument synch class buffers. */
void cleanup_sync_class(void)
{
  pfs_free(mutex_class_array);
  mutex_class_array= NULL;
  mutex_class_dirty_count= mutex_class_allocated_count= mutex_class_max= 0;
  pfs_free(rwlock_class_array);
  rwlock_class_array= NULL;
  rwlock_class_dirty_count= rwlock_class_allocated_count= rwlock_class_max= 0;
  pfs_free(cond_class_array);
  cond_class_array= NULL;
  cond_class_dirty_count= cond_class_allocated_count= cond_class_max= 0;
}

/**
  Initialize the thread class buffer.
  @param thread_class_sizing          max number of thread class
  @return 0 on success
*/
int init_thread_class(uint thread_class_sizing)
{
  int result= 0;
  thread_class_dirty_count= thread_class_allocated_count= 0;
  thread_class_max= thread_class_sizing;
  thread_class_lost= 0;

  if (thread_class_max > 0)
  {
    thread_class_array= PFS_MALLOC_ARRAY(thread_class_max, PFS_thread_class,
                                         MYF(MY_ZEROFILL));
    if (unlikely(thread_class_array == NULL))
      result= 1;
  }
  else
    thread_class_array= NULL;

  return result;
}

/** Cleanup the thread class buffers. */
void cleanup_thread_class(void)
{
  pfs_free(thread_class_array);
  thread_class_array= NULL;
  thread_class_dirty_count= thread_class_allocated_count= 0;
  thread_class_max= 0;
}

/**
  Initialize the table share buffer.
  @param table_share_sizing           max number of table share
  @return 0 on success
*/
int init_table_share(uint table_share_sizing)
{
  int result= 0;
  table_share_max= table_share_sizing;
  table_share_lost= 0;

  if (table_share_max > 0)
  {
    table_share_array= PFS_MALLOC_ARRAY(table_share_max, PFS_table_share,
                                        MYF(MY_ZEROFILL));
    if (unlikely(table_share_array == NULL))
      result= 1;
  }
  else
    table_share_array= NULL;

  return result;
}

/** Cleanup the table share buffers. */
void cleanup_table_share(void)
{
  pfs_free(table_share_array);
  table_share_array= NULL;
  table_share_max= 0;
}

static uchar *table_share_hash_get_key(const uchar *entry, size_t *length,
                                       my_bool)
{
  const PFS_table_share * const *typed_entry;
  const PFS_table_share *share;
  const void *result;
  typed_entry= reinterpret_cast<const PFS_table_share* const *> (entry);
  DBUG_ASSERT(typed_entry != NULL);
  share= *typed_entry;
  DBUG_ASSERT(share != NULL);
  *length= share->m_key.m_key_length;
  result= &share->m_key.m_hash_key[0];
  return const_cast<uchar*> (reinterpret_cast<const uchar*> (result));
}

/** Initialize the table share hash table. */
int init_table_share_hash(void)
{
  if ((! table_share_hash_inited) && (table_share_max > 0))
  {
    lf_hash_init(&table_share_hash, sizeof(PFS_table_share*), LF_HASH_UNIQUE,
                 0, 0, table_share_hash_get_key, &my_charset_bin);
    table_share_hash_inited= true;
  }
  return 0;
}

/** Cleanup the table share hash table. */
void cleanup_table_share_hash(void)
{
  if (table_share_hash_inited)
  {
    lf_hash_destroy(&table_share_hash);
    table_share_hash_inited= false;
  }
}

/**
  Initialize the file class buffer.
  @param file_class_sizing            max number of file class
  @return 0 on success
*/
int init_file_class(uint file_class_sizing)
{
  int result= 0;
  file_class_dirty_count= file_class_allocated_count= 0;
  file_class_max= file_class_sizing;
  file_class_lost= 0;

  if (file_class_max > 0)
  {
    file_class_array= PFS_MALLOC_ARRAY(file_class_max, PFS_file_class,
                                       MYF(MY_ZEROFILL));
    if (unlikely(file_class_array == NULL))
      return 1;
  }
  else
    file_class_array= NULL;

  return result;
}

/** Cleanup the file class buffers. */
void cleanup_file_class(void)
{
  pfs_free(file_class_array);
  file_class_array= NULL;
  file_class_dirty_count= file_class_allocated_count= 0;
  file_class_max= 0;
}

static void init_instr_class(PFS_instr_class *klass,
                             const char *name,
                             uint name_length,
                             int flags)
{
  DBUG_ASSERT(name_length <= PFS_MAX_INFO_NAME_LENGTH);
  memset(klass, 0, sizeof(PFS_instr_class));
  strncpy(klass->m_name, name, name_length);
  klass->m_name_length= name_length;
  klass->m_flags= flags;
  klass->m_enabled= true;
  klass->m_timed= true;
}

#define REGISTER_CLASS_BODY_PART(INDEX, ARRAY, MAX, NAME, NAME_LENGTH) \
  for (INDEX= 0; INDEX < MAX; INDEX++)                                 \
  {                                                                    \
    entry= &ARRAY[INDEX];                                              \
    if ((entry->m_name_length == NAME_LENGTH) &&                       \
        (strncmp(entry->m_name, NAME, NAME_LENGTH) == 0))              \
    {                                                                  \
      DBUG_ASSERT(entry->m_flags == flags);                            \
      return (INDEX + 1);                                              \
    }                                                                  \
  }

/**
  Register a mutex instrumentation metadata.
  @param name                         the instrumented name
  @param name_length                  length in bytes of name
  @param flags                        the instrumentation flags
  @return a mutex instrumentation key
*/
PFS_sync_key register_mutex_class(const char *name, uint name_length,
                                  int flags)
{
  uint32 index;
  PFS_mutex_class *entry;

  /*
    This is a full array scan, which is not optimal.
    This is acceptable since this code is only used at startup,
    or when a plugin is loaded.
  */
  REGISTER_CLASS_BODY_PART(index, mutex_class_array, mutex_class_max,
                           name, name_length)
  /*
    Note that:
    mutex_class_dirty_count is incremented *before* an entry is added
    mutex_class_allocated_count is incremented *after* an entry is added
  */
  index= PFS_atomic::add_u32(&mutex_class_dirty_count, 1);

  if (index < mutex_class_max)
  {
    /*
      The instrument was not found (from a possible previous
      load / unload of a plugin), allocate it.
      This code is safe when 2 threads execute in parallel
      for different mutex classes:
      - thread 1 registering class A
      - thread 2 registering class B
      will not collide in the same mutex_class_array[index] entry.
      This code does not protect against 2 threads registering
      in parallel the same class:
      - thread 1 registering class A
      - thread 2 registering class A
      could lead to a duplicate class A entry.
      This is ok, since this case can not happen in the caller:
      - classes names are derived from a plugin name
        ('wait/synch/mutex/<plugin>/xxx')
      - 2 threads can not register concurrently the same plugin
        in INSTALL PLUGIN.
    */
    entry= &mutex_class_array[index];
    init_instr_class(entry, name, name_length, flags);
    entry->m_wait_stat.m_control_flag=
      &flag_events_waits_summary_by_event_name;
    entry->m_wait_stat.m_parent= NULL;
    reset_single_stat_link(&entry->m_wait_stat);
    entry->m_lock_stat.m_control_flag=
      &flag_events_locks_summary_by_event_name;
    entry->m_lock_stat.m_parent= NULL;
    reset_single_stat_link(&entry->m_lock_stat);
    entry->m_index= index;
    /*
      Now that this entry is populated, advertise it

      Technically, there is a small race condition here:
      T0:
      mutex_class_dirty_count= 10
      mutex_class_allocated_count= 10
      T1: Thread A increment mutex_class_dirty_count to 11
      T2: Thread B increment mutex_class_dirty_count to 12
      T3: Thread A populate entry 11
      T4: Thread B populate entry 12
      T5: Thread B increment mutex_class_allocated_count to 11,
          advertise thread A incomplete record 11,
          but does not advertise thread B complete record 12
      T6: Thread A increment mutex_class_allocated_count to 12
      This has no impact, and is acceptable.
      A reader will not see record 12 for a short time.
      A reader will see an incomplete record 11 for a short time,
      which is ok: the mutex name / statistics will be temporarily
      empty/NULL/zero, but this won't cause a crash
      (mutex_class_array is initialized with MY_ZEROFILL).
    */
    PFS_atomic::add_u32(&mutex_class_allocated_count, 1);
    return (index + 1);
  }

  /*
    Out of space, report to SHOW STATUS that
    the allocated memory was too small.
  */
  mutex_class_lost++;
  return 0;
}

/**
  Register a rwlock instrumentation metadata.
  @param name                         the instrumented name
  @param name_length                  length in bytes of name
  @param flags                        the instrumentation flags
  @return a rwlock instrumentation key
*/
PFS_sync_key register_rwlock_class(const char *name, uint name_length,
                                   int flags)
{
  /* See comments in register_mutex_class */
  uint32 index;
  PFS_rwlock_class *entry;

  REGISTER_CLASS_BODY_PART(index, rwlock_class_array, rwlock_class_max,
                           name, name_length)

  index= PFS_atomic::add_u32(&rwlock_class_dirty_count, 1);

  if (index < rwlock_class_max)
  {
    entry= &rwlock_class_array[index];
    init_instr_class(entry, name, name_length, flags);
    entry->m_wait_stat.m_control_flag=
      &flag_events_waits_summary_by_event_name;
    entry->m_wait_stat.m_parent= NULL;
    reset_single_stat_link(&entry->m_wait_stat);
    entry->m_read_lock_stat.m_control_flag=
      &flag_events_locks_summary_by_event_name;
    entry->m_read_lock_stat.m_parent= NULL;
    reset_single_stat_link(&entry->m_read_lock_stat);
    entry->m_write_lock_stat.m_control_flag=
      &flag_events_locks_summary_by_event_name;
    entry->m_write_lock_stat.m_parent= NULL;
    reset_single_stat_link(&entry->m_write_lock_stat);
    entry->m_index= index;
    PFS_atomic::add_u32(&rwlock_class_allocated_count, 1);
    return (index + 1);
  }

  rwlock_class_lost++;
  return 0;
}

/**
  Register a condition instrumentation metadata.
  @param name                         the instrumented name
  @param name_length                  length in bytes of name
  @param flags                        the instrumentation flags
  @return a condition instrumentation key
*/
PFS_sync_key register_cond_class(const char *name, uint name_length,
                                 int flags)
{
  /* See comments in register_mutex_class */
  uint32 index;
  PFS_cond_class *entry;

  REGISTER_CLASS_BODY_PART(index, cond_class_array, cond_class_max,
                           name, name_length)

  index= PFS_atomic::add_u32(&cond_class_dirty_count, 1);

  if (index < cond_class_max)
  {
    entry= &cond_class_array[index];
    init_instr_class(entry, name, name_length, flags);
    entry->m_wait_stat.m_control_flag=
      &flag_events_waits_summary_by_event_name;
    entry->m_wait_stat.m_parent= NULL;
    reset_single_stat_link(&entry->m_wait_stat);
    entry->m_index= index;
    PFS_atomic::add_u32(&cond_class_allocated_count, 1);
    return (index + 1);
  }

  cond_class_lost++;
  return 0;
}

#define FIND_CLASS_BODY(KEY, COUNT, ARRAY) \
  if ((KEY == 0) || (KEY > COUNT))         \
    return NULL;                           \
  return &ARRAY[KEY - 1]

/**
  Find a mutex instrumentation class by key.
  @param key                          the instrument key
  @return the instrument class, or NULL
*/
PFS_mutex_class *find_mutex_class(PFS_sync_key key)
{
  FIND_CLASS_BODY(key, mutex_class_allocated_count, mutex_class_array);
}

PFS_mutex_class *sanitize_mutex_class(PFS_mutex_class *unsafe)
{
  SANITIZE_ARRAY_BODY(PFS_mutex_class, mutex_class_array, mutex_class_max, unsafe);
}

/**
  Find a rwlock instrumentation class by key.
  @param key                          the instrument key
  @return the instrument class, or NULL
*/
PFS_rwlock_class *find_rwlock_class(PFS_sync_key key)
{
  FIND_CLASS_BODY(key, rwlock_class_allocated_count, rwlock_class_array);
}

PFS_rwlock_class *sanitize_rwlock_class(PFS_rwlock_class *unsafe)
{
  SANITIZE_ARRAY_BODY(PFS_rwlock_class, rwlock_class_array, rwlock_class_max, unsafe);
}

/**
  Find a condition instrumentation class by key.
  @param key                          the instrument key
  @return the instrument class, or NULL
*/
PFS_cond_class *find_cond_class(PFS_sync_key key)
{
  FIND_CLASS_BODY(key, cond_class_allocated_count, cond_class_array);
}

PFS_cond_class *sanitize_cond_class(PFS_cond_class *unsafe)
{
  SANITIZE_ARRAY_BODY(PFS_cond_class, cond_class_array, cond_class_max, unsafe);
}

/**
  Register a thread instrumentation metadata.
  @param name                         the instrumented name
  @param name_length                  length in bytes of name
  @param flags                        the instrumentation flags
  @return a thread instrumentation key
*/
PFS_thread_key register_thread_class(const char *name, uint name_length,
                                     int flags)
{
  /* See comments in register_mutex_class */
  uint32 index;
  PFS_thread_class *entry;

  for (index= 0; index < thread_class_max; index++)
  {
    entry= &thread_class_array[index];

    if ((entry->m_name_length == name_length) &&
        (strncmp(entry->m_name, name, name_length) == 0))
      return (index + 1);
  }

  index= PFS_atomic::add_u32(&thread_class_dirty_count, 1);

  if (index < thread_class_max)
  {
    entry= &thread_class_array[index];
    DBUG_ASSERT(name_length <= PFS_MAX_INFO_NAME_LENGTH);
    strncpy(entry->m_name, name, name_length);
    entry->m_name_length= name_length;
    entry->m_enabled= true;
    PFS_atomic::add_u32(&thread_class_allocated_count, 1);
    return (index + 1);
  }

  thread_class_lost++;
  return 0;
}

/**
  Find a thread instrumentation class by key.
  @param key                          the instrument key
  @return the instrument class, or NULL
*/
PFS_thread_class *find_thread_class(PFS_sync_key key)
{
  FIND_CLASS_BODY(key, thread_class_allocated_count, thread_class_array);
}

PFS_thread_class *sanitize_thread_class(PFS_thread_class *unsafe)
{
  SANITIZE_ARRAY_BODY(PFS_thread_class, thread_class_array, thread_class_max, unsafe);
}

/**
  Register a file instrumentation metadata.
  @param name                         the instrumented name
  @param name_length                  length in bytes of name
  @param flags                        the instrumentation flags
  @return a file instrumentation key
*/
PFS_file_key register_file_class(const char *name, uint name_length,
                                 int flags)
{
  /* See comments in register_mutex_class */
  uint32 index;
  PFS_file_class *entry;

  REGISTER_CLASS_BODY_PART(index, file_class_array, file_class_max,
                           name, name_length)

  index= PFS_atomic::add_u32(&file_class_dirty_count, 1);

  if (index < file_class_max)
  {
    entry= &file_class_array[index];
    init_instr_class(entry, name, name_length, flags);
    entry->m_wait_stat.m_control_flag=
      &flag_events_waits_summary_by_event_name;
    entry->m_wait_stat.m_parent= NULL;
    reset_single_stat_link(&entry->m_wait_stat);
    entry->m_index= index;
    PFS_atomic::add_u32(&file_class_allocated_count, 1);
    return (index + 1);
  }

  file_class_lost++;
  return 0;
}

/**
  Find a file instrumentation class by key.
  @param key                          the instrument key
  @return the instrument class, or NULL
*/
PFS_file_class *find_file_class(PFS_file_key key)
{
  FIND_CLASS_BODY(key, file_class_allocated_count, file_class_array);
}

PFS_file_class *sanitize_file_class(PFS_file_class *unsafe)
{
  SANITIZE_ARRAY_BODY(PFS_file_class, file_class_array, file_class_max, unsafe);
}

/**
  Find or create a table instance by name.
  @param thread                       the executing instrumented thread
  @param schema_name                  the table schema name
  @param schema_name_length           the table schema name length
  @param table_name                   the table name
  @param table_name_length            the table name length
  @return a table instance, or NULL
*/
PFS_table_share* find_or_create_table_share(PFS_thread *thread,
                                            const char *schema_name,
                                            uint schema_name_length,
                                            const char *table_name,
                                            uint table_name_length)
{
  /* See comments in register_mutex_class */
  int pass;
  PFS_table_share_key key;

  if (! table_share_hash_inited)
  {
    /* Table instrumentation can be turned off. */
    table_share_lost++;
    return NULL;
  }

  if (unlikely(thread->m_table_share_hash_pins == NULL))
  {
    thread->m_table_share_hash_pins= lf_hash_get_pins(&table_share_hash);
    if (unlikely(thread->m_table_share_hash_pins == NULL))
    {
      table_share_lost++;
      return NULL;
    }
  }

  DBUG_ASSERT(schema_name_length <= NAME_LEN);
  DBUG_ASSERT(table_name_length <= NAME_LEN);

  char *ptr= &key.m_hash_key[0];
  memcpy(ptr, schema_name, schema_name_length);
  ptr+= schema_name_length;
  ptr[0]= 0; ptr++;
  memcpy(ptr, table_name, table_name_length);
  ptr+= table_name_length;
  ptr[0]= 0; ptr++;
  key.m_key_length= ptr - &key.m_hash_key[0];

  PFS_table_share **entry;
  uint retry_count= 0;
  const uint retry_max= 3;
search:
  entry= reinterpret_cast<PFS_table_share**>
    (lf_hash_search(&table_share_hash, thread->m_table_share_hash_pins,
                    &key.m_hash_key[0], key.m_key_length));
  if (entry && (entry != MY_ERRPTR))
  {
    PFS_table_share *pfs;
    pfs= *entry;
    lf_hash_search_unpin(thread->m_table_share_hash_pins);
    return pfs;
  }

  /* table_name is not constant, just using it for noise on create */
  uint i= randomized_index(table_name, table_share_max);

  /*
    Pass 1: [random, table_share_max - 1]
    Pass 2: [0, table_share_max - 1]
  */
  for (pass= 1; pass <= 2; i=0, pass++)
  {
    PFS_table_share *pfs= table_share_array + i;
    PFS_table_share *pfs_last= table_share_array + table_share_max;
    for ( ; pfs < pfs_last; pfs++)
    {
      if (pfs->m_lock.is_free())
      {
        if (pfs->m_lock.free_to_dirty())
        {
          pfs->m_key= key;
          pfs->m_schema_name= &pfs->m_key.m_hash_key[0];
          pfs->m_schema_name_length= schema_name_length;
          pfs->m_table_name= &pfs->m_key.m_hash_key[schema_name_length + 1];
          pfs->m_table_name_length= table_name_length;
          pfs->m_wait_stat.m_control_flag=
            &flag_events_waits_summary_by_instance;
          pfs->m_wait_stat.m_parent= NULL;
          reset_single_stat_link(&pfs->m_wait_stat);
          pfs->m_enabled= true;
          pfs->m_timed= true;
          pfs->m_aggregated= false;

          int res;
          res= lf_hash_insert(&table_share_hash,
                              thread->m_table_share_hash_pins, &pfs);
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
              table_share_lost++;
              return NULL;
            }
            goto search;
          }

          /* OOM in lf_hash_insert */
          table_share_lost++;
          return NULL;
        }
      }
    }
  }

  table_share_lost++;
  return NULL;
}

PFS_table_share *sanitize_table_share(PFS_table_share *unsafe)
{
  SANITIZE_ARRAY_BODY(PFS_table_share, table_share_array, table_share_max, unsafe);
}

const char *sanitize_table_schema_name(const char *unsafe)
{
  intptr ptr= (intptr) unsafe;
  intptr first= (intptr) &table_share_array[0];
  intptr last= (intptr) &table_share_array[table_share_max];

  PFS_table_share dummy;

  /* Check if unsafe points inside table_share_array[] */
  if (likely((first <= ptr) && (ptr < last)))
  {
    intptr offset= (ptr - first) % sizeof(PFS_table_share);
    intptr from= my_offsetof(PFS_table_share, m_key.m_hash_key);
    intptr len= sizeof(dummy.m_key.m_hash_key);
    /* Check if unsafe points inside PFS_table_share::m_key::m_hash_key */
    if (likely((from <= offset) && (offset < from + len)))
    {
      PFS_table_share *base= (PFS_table_share*) (ptr - offset);
      /* Check if unsafe really is the schema name */
      if (likely(base->m_schema_name == unsafe))
        return unsafe;
    }
  }
  return NULL;
}

const char *sanitize_table_object_name(const char *unsafe)
{
  intptr ptr= (intptr) unsafe;
  intptr first= (intptr) &table_share_array[0];
  intptr last= (intptr) &table_share_array[table_share_max];

  PFS_table_share dummy;

  /* Check if unsafe points inside table_share_array[] */
  if (likely((first <= ptr) && (ptr < last)))
  {
    intptr offset= (ptr - first) % sizeof(PFS_table_share);
    intptr from= my_offsetof(PFS_table_share, m_key.m_hash_key);
    intptr len= sizeof(dummy.m_key.m_hash_key);
    /* Check if unsafe points inside PFS_table_share::m_key::m_hash_key */
    if (likely((from <= offset) && (offset < from + len)))
    {
      PFS_table_share *base= (PFS_table_share*) (ptr - offset);
      /* Check if unsafe really is the table name */
      if (likely(base->m_table_name == unsafe))
        return unsafe;
    }
  }
  return NULL;
}

static void reset_mutex_class_waits(void)
{
  PFS_mutex_class *pfs= mutex_class_array;
  PFS_mutex_class *pfs_last= mutex_class_array + mutex_class_max;

  for ( ; pfs < pfs_last; pfs++)
    reset_single_stat_link(&pfs->m_wait_stat);
}

static void reset_rwlock_class_waits(void)
{
  PFS_rwlock_class *pfs= rwlock_class_array;
  PFS_rwlock_class *pfs_last= rwlock_class_array + rwlock_class_max;

  for ( ; pfs < pfs_last; pfs++)
    reset_single_stat_link(&pfs->m_wait_stat);
}

static void reset_cond_class_waits(void)
{
  PFS_cond_class *pfs= cond_class_array;
  PFS_cond_class *pfs_last= cond_class_array + cond_class_max;

  for ( ; pfs < pfs_last; pfs++)
    reset_single_stat_link(&pfs->m_wait_stat);
}

static void reset_file_class_waits(void)
{
  PFS_file_class *pfs= file_class_array;
  PFS_file_class *pfs_last= file_class_array + file_class_max;

  for ( ; pfs < pfs_last; pfs++)
    reset_single_stat_link(&pfs->m_wait_stat);
}

/** Reset the wait statistics for every instrument class. */
void reset_instrument_class_waits(void)
{
  reset_mutex_class_waits();
  reset_rwlock_class_waits();
  reset_cond_class_waits();
  reset_file_class_waits();
}

/** Reset the io statistics per file class. */
void reset_file_class_io(void)
{
  PFS_file_class *pfs= file_class_array;
  PFS_file_class *pfs_last= file_class_array + file_class_max;

  for ( ; pfs < pfs_last; pfs++)
    reset_file_stat(&pfs->m_file_stat);
}

/** @} */

