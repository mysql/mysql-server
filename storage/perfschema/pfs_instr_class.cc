/* Copyright (c) 2008, 2013, Oracle and/or its affiliates. All rights reserved.

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
#include "structs.h"
#include "table.h"
#include "pfs_instr_class.h"
#include "pfs_instr.h"
#include "pfs_global.h"
#include "pfs_timer.h"
#include "pfs_events_waits.h"
#include "pfs_setup_object.h"
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
  PFS_INSTRUMENT option settings array and associated state variable to
  serialize access during shutdown.
 */
DYNAMIC_ARRAY pfs_instr_config_array;
int pfs_instr_config_state= PFS_INSTR_CONFIG_NOT_INITIALIZED;

static void configure_instr_class(PFS_instr_class *entry);

static void init_instr_class(PFS_instr_class *klass,
                             const char *name,
                             uint name_length,
                             int flags,
                             PFS_class_type class_type);

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
/** Size of the stage class array. @sa stage_class_array */
ulong stage_class_max= 0;
/** Number of stage class lost. @sa stage_class_array */
ulong stage_class_lost= 0;
/** Size of the statement class array. @sa statement_class_array */
ulong statement_class_max= 0;
/** Number of statement class lost. @sa statement_class_array */
ulong statement_class_lost= 0;
/** Size of the table share array. @sa table_share_array */
ulong table_share_max= 0;
/** Number of table share lost. @sa table_share_array */
ulong table_share_lost= 0;
/** Size of the socket class array. @sa socket_class_array */
ulong socket_class_max= 0;
/** Number of socket class lost. @sa socket_class_array */
ulong socket_class_lost= 0;

PFS_mutex_class *mutex_class_array= NULL;
PFS_rwlock_class *rwlock_class_array= NULL;
PFS_cond_class *cond_class_array= NULL;

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

PFS_ALIGNED PFS_single_stat global_idle_stat;
PFS_ALIGNED PFS_table_io_stat global_table_io_stat;
PFS_ALIGNED PFS_table_lock_stat global_table_lock_stat;
PFS_ALIGNED PFS_instr_class global_table_io_class;
PFS_ALIGNED PFS_instr_class global_table_lock_class;
PFS_ALIGNED PFS_instr_class global_idle_class;

/** Class-timer map */
enum_timer_name *class_timers[] =
{&wait_timer,      /* PFS_CLASS_NONE */
 &wait_timer,      /* PFS_CLASS_MUTEX */
 &wait_timer,      /* PFS_CLASS_RWLOCK */
 &wait_timer,      /* PFS_CLASS_COND */
 &wait_timer,      /* PFS_CLASS_FILE */
 &wait_timer,      /* PFS_CLASS_TABLE */
 &stage_timer,     /* PFS_CLASS_STAGE */
 &statement_timer, /* PFS_CLASS_STATEMENT */
 &wait_timer,      /* PFS_CLASS_SOCKET */
 &wait_timer,      /* PFS_CLASS_TABLE_IO */
 &wait_timer,      /* PFS_CLASS_TABLE_LOCK */
 &idle_timer       /* PFS_CLASS_IDLE */
};

/**
  Hash index for instrumented table shares.
  This index is searched by table fully qualified name (@c PFS_table_share_key),
  and points to instrumented table shares (@c PFS_table_share).
  @sa table_share_array
  @sa PFS_table_share_key
  @sa PFS_table_share
  @sa table_share_hash_get_key
  @sa get_table_share_hash_pins
*/
LF_HASH table_share_hash;
/** True if table_share_hash is initialized. */
static bool table_share_hash_inited= false;

static volatile uint32 file_class_dirty_count= 0;
static volatile uint32 file_class_allocated_count= 0;

PFS_file_class *file_class_array= NULL;

static volatile uint32 stage_class_dirty_count= 0;
static volatile uint32 stage_class_allocated_count= 0;

static PFS_stage_class *stage_class_array= NULL;

static volatile uint32 statement_class_dirty_count= 0;
static volatile uint32 statement_class_allocated_count= 0;

static PFS_statement_class *statement_class_array= NULL;

static volatile uint32 socket_class_dirty_count= 0;
static volatile uint32 socket_class_allocated_count= 0;

static PFS_socket_class *socket_class_array= NULL;

uint mutex_class_start= 0;
uint rwlock_class_start= 0;
uint cond_class_start= 0;
uint file_class_start= 0;
uint wait_class_max= 0;
uint socket_class_start= 0;

void init_event_name_sizing(const PFS_global_param *param)
{
  mutex_class_start= 3; /* global table io, table lock, idle */
  rwlock_class_start= mutex_class_start + param->m_mutex_class_sizing;
  cond_class_start= rwlock_class_start + param->m_rwlock_class_sizing;
  file_class_start= cond_class_start + param->m_cond_class_sizing;
  socket_class_start= file_class_start + param->m_file_class_sizing;
  wait_class_max= socket_class_start + param->m_socket_class_sizing;
}

void register_global_classes()
{
  /* Table IO class */
  init_instr_class(&global_table_io_class, "wait/io/table/sql/handler", 25,
                   0, PFS_CLASS_TABLE_IO);
  global_table_io_class.m_event_name_index= GLOBAL_TABLE_IO_EVENT_INDEX;
  configure_instr_class(&global_table_io_class);

  /* Table lock class */
  init_instr_class(&global_table_lock_class, "wait/lock/table/sql/handler", 27,
                   0, PFS_CLASS_TABLE_LOCK);
  global_table_lock_class.m_event_name_index= GLOBAL_TABLE_LOCK_EVENT_INDEX;
  configure_instr_class(&global_table_lock_class);
  
  /* Idle class */
  init_instr_class(&global_idle_class, "idle", 4,
                   0, PFS_CLASS_IDLE);
  global_idle_class.m_event_name_index= GLOBAL_IDLE_EVENT_INDEX;
  configure_instr_class(&global_idle_class);
}

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

C_MODE_START
/** get_key function for @c table_share_hash. */
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
C_MODE_END

/** Initialize the table share hash table. */
int init_table_share_hash(void)
{
  if ((! table_share_hash_inited) && (table_share_max > 0))
  {
    lf_hash_init(&table_share_hash, sizeof(PFS_table_share*), LF_HASH_UNIQUE,
                 0, 0, table_share_hash_get_key, &my_charset_bin);
    table_share_hash.size= table_share_max;
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
  Get the hash pins for @sa table_share_hash.
  @param thread The running thread.
  @returns The LF_HASH pins for the thread.
*/
LF_PINS* get_table_share_hash_pins(PFS_thread *thread)
{
  if (unlikely(thread->m_table_share_hash_pins == NULL))
  {
    if (! table_share_hash_inited)
      return NULL;
    thread->m_table_share_hash_pins= lf_hash_get_pins(&table_share_hash);
  }
  return thread->m_table_share_hash_pins;
}

/**
  Set a table share hash key.
  @param [out] key The key to populate.
  @param temporary True for TEMPORARY TABLE.
  @param schema_name The table schema name.
  @param schema_name_length The table schema name length.
  @param table_name The table name.
  @param table_name_length The table name length.
*/
static void set_table_share_key(PFS_table_share_key *key,
                                bool temporary,
                                const char *schema_name, uint schema_name_length,
                                const char *table_name, uint table_name_length)
{
  DBUG_ASSERT(schema_name_length <= NAME_LEN);
  DBUG_ASSERT(table_name_length <= NAME_LEN);
  char *saved_schema_name;
  char *saved_table_name;

  char *ptr= &key->m_hash_key[0];
  ptr[0]= (temporary ? OBJECT_TYPE_TEMPORARY_TABLE : OBJECT_TYPE_TABLE);
  ptr++;
  saved_schema_name= ptr;
  memcpy(ptr, schema_name, schema_name_length);
  ptr+= schema_name_length;
  ptr[0]= 0;
  ptr++;
  saved_table_name= ptr;
  memcpy(ptr, table_name, table_name_length);
  ptr+= table_name_length;
  ptr[0]= 0;
  ptr++;
  key->m_key_length= ptr - &key->m_hash_key[0];

  if (lower_case_table_names)
  {
    my_casedn_str(files_charset_info, saved_schema_name);
    my_casedn_str(files_charset_info, saved_table_name);
  }
}

void PFS_table_share::refresh_setup_object_flags(PFS_thread *thread)
{
  lookup_setup_object(thread,
                      OBJECT_TYPE_TABLE,
                      m_schema_name, m_schema_name_length,
                      m_table_name, m_table_name_length,
                      &m_enabled, &m_timed);
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

/**
  Initialize the stage class buffer.
  @param stage_class_sizing            max number of stage class
  @return 0 on success
*/
int init_stage_class(uint stage_class_sizing)
{
  int result= 0;
  stage_class_dirty_count= stage_class_allocated_count= 0;
  stage_class_max= stage_class_sizing;
  stage_class_lost= 0;

  if (stage_class_max > 0)
  {
    stage_class_array= PFS_MALLOC_ARRAY(stage_class_max, PFS_stage_class,
                                        MYF(MY_ZEROFILL));
    if (unlikely(stage_class_array == NULL))
      return 1;
  }
  else
    stage_class_array= NULL;

  return result;
}

/** Cleanup the stage class buffers. */
void cleanup_stage_class(void)
{
  pfs_free(stage_class_array);
  stage_class_array= NULL;
  stage_class_dirty_count= stage_class_allocated_count= 0;
  stage_class_max= 0;
}

/**
  Initialize the statement class buffer.
  @param statement_class_sizing            max number of statement class
  @return 0 on success
*/
int init_statement_class(uint statement_class_sizing)
{
  int result= 0;
  statement_class_dirty_count= statement_class_allocated_count= 0;
  statement_class_max= statement_class_sizing;
  statement_class_lost= 0;

  if (statement_class_max > 0)
  {
    statement_class_array= PFS_MALLOC_ARRAY(statement_class_max, PFS_statement_class,
                                            MYF(MY_ZEROFILL));
    if (unlikely(statement_class_array == NULL))
      return 1;
  }
  else
    statement_class_array= NULL;

  return result;
}

/** Cleanup the statement class buffers. */
void cleanup_statement_class(void)
{
  pfs_free(statement_class_array);
  statement_class_array= NULL;
  statement_class_dirty_count= statement_class_allocated_count= 0;
  statement_class_max= 0;
}

/**
  Initialize the socket class buffer.
  @param socket_class_sizing            max number of socket class
  @return 0 on success
*/
int init_socket_class(uint socket_class_sizing)
{
  int result= 0;
  socket_class_dirty_count= socket_class_allocated_count= 0;
  socket_class_max= socket_class_sizing;
  socket_class_lost= 0;

  if (socket_class_max > 0)
  {
    socket_class_array= PFS_MALLOC_ARRAY(socket_class_max, PFS_socket_class,
                                         MYF(MY_ZEROFILL));
    if (unlikely(socket_class_array == NULL))
      return 1;
  }
  else
    socket_class_array= NULL;

  return result;
}

/** Cleanup the socket class buffers. */
void cleanup_socket_class(void)
{
  pfs_free(socket_class_array);
  socket_class_array= NULL;
  socket_class_dirty_count= socket_class_allocated_count= 0;
  socket_class_max= 0;
}

static void init_instr_class(PFS_instr_class *klass,
                             const char *name,
                             uint name_length,
                             int flags,
                             PFS_class_type class_type)
{
  DBUG_ASSERT(name_length <= PFS_MAX_INFO_NAME_LENGTH);
  memset(klass, 0, sizeof(PFS_instr_class));
  strncpy(klass->m_name, name, name_length);
  klass->m_name_length= name_length;
  klass->m_flags= flags;
  klass->m_enabled= true;
  klass->m_timed= true;
  klass->m_type= class_type;
  klass->m_timer= class_timers[class_type];
}

/**
  Set user-defined configuration values for an instrument.
*/
static void configure_instr_class(PFS_instr_class *entry)
{
  uint match_length= 0; /* length of matching pattern */

  for (uint i= 0; i < pfs_instr_config_array.elements; i++)
  {
    PFS_instr_config* e;
    get_dynamic(&pfs_instr_config_array, (uchar*)&e, i);

    /**
      Compare class name to all configuration entries. In case of multiple
      matches, the longer specification wins. For example, the pattern
      'ABC/DEF/GHI=ON' has precedence over 'ABC/DEF/%=OFF' regardless of
      position within the configuration file or command line.

      Consecutive wildcards affect the count.
    */
    if (!my_wildcmp(&my_charset_latin1,
                    entry->m_name, entry->m_name+entry->m_name_length,
                    e->m_name, e->m_name+e->m_name_length,
                    '\\', '?','%'))
    {
        if (e->m_name_length >= match_length)
        {
           entry->m_enabled= e->m_enabled;
           entry->m_timed= e->m_timed;
           match_length= MY_MAX(e->m_name_length, match_length);
        }
    }
  }
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
    init_instr_class(entry, name, name_length, flags, PFS_CLASS_MUTEX);
    entry->m_mutex_stat.reset();
    entry->m_event_name_index= mutex_class_start + index;
    entry->m_singleton= NULL;
    entry->m_enabled= false; /* disabled by default */
    entry->m_timed= false;

    /* Set user-defined configuration options for this instrument */
    configure_instr_class(entry);

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
    init_instr_class(entry, name, name_length, flags, PFS_CLASS_RWLOCK);
    entry->m_rwlock_stat.reset();
    entry->m_event_name_index= rwlock_class_start + index;
    entry->m_singleton= NULL;
    entry->m_enabled= false; /* disabled by default */
    entry->m_timed= false;
    /* Set user-defined configuration options for this instrument */
    configure_instr_class(entry);
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
    init_instr_class(entry, name, name_length, flags, PFS_CLASS_COND);
    entry->m_event_name_index= cond_class_start + index;
    entry->m_singleton= NULL;
    entry->m_enabled= false; /* disabled by default */
    entry->m_timed= false;
    /* Set user-defined configuration options for this instrument */
    configure_instr_class(entry);
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
    init_instr_class(entry, name, name_length, flags, PFS_CLASS_FILE);
    entry->m_event_name_index= file_class_start + index;
    entry->m_singleton= NULL;
    entry->m_enabled= true; /* enabled by default */
    entry->m_timed= true;
    /* Set user-defined configuration options for this instrument */
    configure_instr_class(entry);
    PFS_atomic::add_u32(&file_class_allocated_count, 1);
    return (index + 1);
  }

  file_class_lost++;
  return 0;
}

/**
  Register a stage instrumentation metadata.
  @param name                         the instrumented name
  @param prefix_length                length in bytes of the name prefix
  @param name_length                  length in bytes of name
  @param flags                        the instrumentation flags
  @return a stage instrumentation key
*/
PFS_stage_key register_stage_class(const char *name,
                                   uint prefix_length,
                                   uint name_length,
                                   int flags)
{
  /* See comments in register_mutex_class */
  uint32 index;
  PFS_stage_class *entry;

  REGISTER_CLASS_BODY_PART(index, stage_class_array, stage_class_max,
                           name, name_length)

  index= PFS_atomic::add_u32(&stage_class_dirty_count, 1);

  if (index < stage_class_max)
  {
    entry= &stage_class_array[index];
    init_instr_class(entry, name, name_length, flags, PFS_CLASS_STAGE);
    entry->m_prefix_length= prefix_length;
    entry->m_event_name_index= index;
    entry->m_enabled= false; /* disabled by default */
    entry->m_timed= false;
    /* Set user-defined configuration options for this instrument */
    configure_instr_class(entry);
    PFS_atomic::add_u32(&stage_class_allocated_count, 1);

    return (index + 1);
  }

  stage_class_lost++;
  return 0;
}

/**
  Register a statement instrumentation metadata.
  @param name                         the instrumented name
  @param name_length                  length in bytes of name
  @param flags                        the instrumentation flags
  @return a statement instrumentation key
*/
PFS_statement_key register_statement_class(const char *name, uint name_length,
                                           int flags)
{
  /* See comments in register_mutex_class */
  uint32 index;
  PFS_statement_class *entry;

  REGISTER_CLASS_BODY_PART(index, statement_class_array, statement_class_max,
                           name, name_length)

  index= PFS_atomic::add_u32(&statement_class_dirty_count, 1);

  if (index < statement_class_max)
  {
    entry= &statement_class_array[index];
    init_instr_class(entry, name, name_length, flags, PFS_CLASS_STATEMENT);
    entry->m_event_name_index= index;
    entry->m_enabled= true; /* enabled by default */
    entry->m_timed= true;
    /* Set user-defined configuration options for this instrument */
    configure_instr_class(entry);
    PFS_atomic::add_u32(&statement_class_allocated_count, 1);

    return (index + 1);
  }

  statement_class_lost++;
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
  Find a stage instrumentation class by key.
  @param key                          the instrument key
  @return the instrument class, or NULL
*/
PFS_stage_class *find_stage_class(PFS_stage_key key)
{
  FIND_CLASS_BODY(key, stage_class_allocated_count, stage_class_array);
}

PFS_stage_class *sanitize_stage_class(PFS_stage_class *unsafe)
{
  SANITIZE_ARRAY_BODY(PFS_stage_class, stage_class_array, stage_class_max, unsafe);
}

/**
  Find a statement instrumentation class by key.
  @param key                          the instrument key
  @return the instrument class, or NULL
*/
PFS_statement_class *find_statement_class(PFS_stage_key key)
{
  FIND_CLASS_BODY(key, statement_class_allocated_count, statement_class_array);
}

PFS_statement_class *sanitize_statement_class(PFS_statement_class *unsafe)
{
  SANITIZE_ARRAY_BODY(PFS_statement_class, statement_class_array, statement_class_max, unsafe);
}

/**
  Register a socket instrumentation metadata.
  @param name                         the instrumented name
  @param name_length                  length in bytes of name
  @param flags                        the instrumentation flags
  @return a socket instrumentation key
*/
PFS_socket_key register_socket_class(const char *name, uint name_length,
                                     int flags)
{
  /* See comments in register_mutex_class */
  uint32 index;
  PFS_socket_class *entry;

  REGISTER_CLASS_BODY_PART(index, socket_class_array, socket_class_max,
                           name, name_length)

  index= PFS_atomic::add_u32(&socket_class_dirty_count, 1);

  if (index < socket_class_max)
  {
    entry= &socket_class_array[index];
    init_instr_class(entry, name, name_length, flags, PFS_CLASS_SOCKET);
    entry->m_event_name_index= socket_class_start + index;
    entry->m_singleton= NULL;
    entry->m_enabled= false; /* disabled by default */
    entry->m_timed= false;
    /* Set user-defined configuration options for this instrument */
    configure_instr_class(entry);
    PFS_atomic::add_u32(&socket_class_allocated_count, 1);
    return (index + 1);
  }

  socket_class_lost++;
  return 0;
}

/**
  Find a socket instrumentation class by key.
  @param key                          the instrument key
  @return the instrument class, or NULL
*/
PFS_socket_class *find_socket_class(PFS_socket_key key)
{
  FIND_CLASS_BODY(key, socket_class_allocated_count, socket_class_array);
}

PFS_socket_class *sanitize_socket_class(PFS_socket_class *unsafe)
{
  SANITIZE_ARRAY_BODY(PFS_socket_class, socket_class_array, socket_class_max, unsafe);
}

PFS_instr_class *find_table_class(uint index)
{
  if (index == 1)
    return & global_table_io_class;
  if (index == 2)
    return & global_table_lock_class;
  return NULL;
}

PFS_instr_class *sanitize_table_class(PFS_instr_class *unsafe)
{
  if (likely((& global_table_io_class == unsafe) ||
             (& global_table_lock_class == unsafe)))
    return unsafe;
  return NULL;
}

PFS_instr_class *find_idle_class(uint index)
{
  if (index == 1)
    return & global_idle_class;
  return NULL;
}

PFS_instr_class *sanitize_idle_class(PFS_instr_class *unsafe)
{
  if (likely(& global_idle_class == unsafe))
    return unsafe;
  return NULL;
}

static void set_keys(PFS_table_share *pfs, const TABLE_SHARE *share)
{
  int len;
  KEY *key_info= share->key_info;
  PFS_table_key *pfs_key= pfs->m_keys;
  PFS_table_key *pfs_key_last= pfs->m_keys + share->keys;
  pfs->m_key_count= share->keys;

  for ( ; pfs_key < pfs_key_last; pfs_key++, key_info++)
  {
    len= strlen(key_info->name);
    memcpy(pfs_key->m_name, key_info->name, len);
    pfs_key->m_name_length= len;
  }

  pfs_key_last= pfs->m_keys + MAX_INDEXES;
  for ( ; pfs_key < pfs_key_last; pfs_key++)
    pfs_key->m_name_length= 0;
}

static int compare_keys(PFS_table_share *pfs, const TABLE_SHARE *share)
{
  uint len;
  KEY *key_info= share->key_info;
  PFS_table_key *pfs_key= pfs->m_keys;
  PFS_table_key *pfs_key_last= pfs->m_keys + share->keys;

  if (pfs->m_key_count != share->keys)
    return 1;

  for ( ; pfs_key < pfs_key_last; pfs_key++, key_info++)
  {
    len= strlen(key_info->name);
    if (len != pfs_key->m_name_length)
      return 1;

    if (memcmp(pfs_key->m_name, key_info->name, len) != 0)
      return 1;
  }

  return 0;
}

/**
  Find or create a table share instrumentation.
  @param thread                       the executing instrumented thread
  @param temporary                    true for TEMPORARY TABLE
  @param share                        table share
  @return a table share, or NULL
*/
PFS_table_share* find_or_create_table_share(PFS_thread *thread,
                                            bool temporary,
                                            const TABLE_SHARE *share)
{
  /* See comments in register_mutex_class */
  PFS_table_share_key key;

  LF_PINS *pins= get_table_share_hash_pins(thread);
  if (unlikely(pins == NULL))
  {
    table_share_lost++;
    return NULL;
  }

  const char *schema_name= share->db.str;
  uint schema_name_length= share->db.length;
  const char *table_name= share->table_name.str;
  uint table_name_length= share->table_name.length;

  set_table_share_key(&key, temporary,
                      schema_name, schema_name_length,
                      table_name, table_name_length);

  PFS_table_share **entry;
  uint retry_count= 0;
  const uint retry_max= 3;
  bool enabled= true;
  bool timed= true;
  static uint PFS_ALIGNED table_share_monotonic_index= 0;
  uint index;
  uint attempts= 0;
  PFS_table_share *pfs;

search:
  entry= reinterpret_cast<PFS_table_share**>
    (lf_hash_search(&table_share_hash, pins,
                    key.m_hash_key, key.m_key_length));
  if (entry && (entry != MY_ERRPTR))
  {
    pfs= *entry;
    pfs->inc_refcount() ;
    if (compare_keys(pfs, share) != 0)
    {
      set_keys(pfs, share);
      /* FIXME: aggregate to table_share sink ? */
      pfs->m_table_stat.fast_reset();
    }
    lf_hash_search_unpin(pins);
    return pfs;
  }

  lf_hash_search_unpin(pins);

  if (retry_count == 0)
  {
    lookup_setup_object(thread,
                        OBJECT_TYPE_TABLE,
                        schema_name, schema_name_length,
                        table_name, table_name_length,
                        &enabled, &timed);
    /*
      Even when enabled is false, a record is added in the dictionary:
      - It makes enabling a table already in the table cache possible,
      - It improves performances for the next time a TABLE_SHARE is reloaded
        in the table cache.
    */
  }

  while (++attempts <= table_share_max)
  {
    /* See create_mutex() */
    index= PFS_atomic::add_u32(& table_share_monotonic_index, 1) % table_share_max;
    pfs= table_share_array + index;

    if (pfs->m_lock.is_free())
    {
      if (pfs->m_lock.free_to_dirty())
      {
        pfs->m_key= key;
        pfs->m_schema_name= &pfs->m_key.m_hash_key[1];
        pfs->m_schema_name_length= schema_name_length;
        pfs->m_table_name= &pfs->m_key.m_hash_key[schema_name_length + 2];
        pfs->m_table_name_length= table_name_length;
        pfs->m_enabled= enabled;
        pfs->m_timed= timed;
        pfs->init_refcount();
        pfs->m_table_stat.fast_reset();
        set_keys(pfs, share);

        int res;
        res= lf_hash_insert(&table_share_hash, pins, &pfs);
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

  table_share_lost++;
  return NULL;
}

void PFS_table_share::aggregate_io(void)
{
  uint safe_key_count= sanitize_index_count(m_key_count);
  PFS_table_io_stat *from_stat;
  PFS_table_io_stat *from_stat_last;
  PFS_table_io_stat sum_io;

  /* Aggregate stats for each index, if any */
  from_stat= & m_table_stat.m_index_stat[0];
  from_stat_last= from_stat + safe_key_count;
  for ( ; from_stat < from_stat_last ; from_stat++)
    sum_io.aggregate(from_stat);

  /* Aggregate stats for the table */
  sum_io.aggregate(& m_table_stat.m_index_stat[MAX_INDEXES]);

  /* Add this table stats to the global sink. */
  global_table_io_stat.aggregate(& sum_io);
  m_table_stat.fast_reset_io();
}

void PFS_table_share::aggregate_lock(void)
{
  global_table_lock_stat.aggregate(& m_table_stat.m_lock_stat);
  m_table_stat.fast_reset_lock();
}

void release_table_share(PFS_table_share *pfs)
{
  DBUG_ASSERT(pfs->get_refcount() > 0);
  pfs->dec_refcount();
}

/**
  Drop the instrumented table share associated with a table.
  @param thread The running thread
  @param temporary True for TEMPORARY TABLE
  @param schema_name The table schema name
  @param schema_name_length The table schema name length
  @param table_name The table name
  @param table_name_length The table name length
*/
void drop_table_share(PFS_thread *thread,
                      bool temporary,
                      const char *schema_name, uint schema_name_length,
                      const char *table_name, uint table_name_length)
{
  PFS_table_share_key key;
  LF_PINS* pins= get_table_share_hash_pins(thread);
  if (unlikely(pins == NULL))
    return;
  set_table_share_key(&key, temporary, schema_name, schema_name_length,
                      table_name, table_name_length);
  PFS_table_share **entry;
  entry= reinterpret_cast<PFS_table_share**>
    (lf_hash_search(&table_share_hash, pins,
                    key.m_hash_key, key.m_key_length));
  if (entry && (entry != MY_ERRPTR))
  {
    PFS_table_share *pfs= *entry;
    lf_hash_delete(&table_share_hash, pins,
                   pfs->m_key.m_hash_key, pfs->m_key.m_key_length);
    pfs->m_lock.allocated_to_free();
  }

  lf_hash_search_unpin(pins);
}

/**
  Sanitize an unsafe table_share pointer.
  @param unsafe The possibly corrupt pointer.
  @return A valid table_safe_pointer, or NULL.
*/
PFS_table_share *sanitize_table_share(PFS_table_share *unsafe)
{
  SANITIZE_ARRAY_BODY(PFS_table_share, table_share_array, table_share_max, unsafe);
}

/** Reset the wait statistics per instrument class. */
void reset_events_waits_by_class()
{
  reset_file_class_io();
  reset_socket_class_io();
  global_idle_stat.reset();
  global_table_io_stat.reset();
  global_table_lock_stat.reset();
}

/** Reset the io statistics per file class. */
void reset_file_class_io(void)
{
  PFS_file_class *pfs= file_class_array;
  PFS_file_class *pfs_last= file_class_array + file_class_max;

  for ( ; pfs < pfs_last; pfs++)
    pfs->m_file_stat.m_io_stat.reset();
}

/** Reset the io statistics per socket class. */
void reset_socket_class_io(void)
{
  PFS_socket_class *pfs= socket_class_array;
  PFS_socket_class *pfs_last= socket_class_array + socket_class_max;

  for ( ; pfs < pfs_last; pfs++)
    pfs->m_socket_stat.m_io_stat.reset();
}

void update_table_share_derived_flags(PFS_thread *thread)
{
  PFS_table_share *pfs= table_share_array;
  PFS_table_share *pfs_last= table_share_array + table_share_max;

  for ( ; pfs < pfs_last; pfs++)
  {
    if (pfs->m_lock.is_populated())
      pfs->refresh_setup_object_flags(thread);
  }
}

/** @} */

