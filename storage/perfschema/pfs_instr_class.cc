/* Copyright (c) 2008, 2019, Oracle and/or its affiliates. All rights reserved.

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
  @file storage/perfschema/pfs_instr_class.cc
  Performance schema instruments metadata (implementation).
*/

#include "storage/perfschema/pfs_instr_class.h"

#include <string.h>
#include <atomic>

#include "lex_string.h"
#include "lf.h"
#include "my_dbug.h"
#include "my_macros.h"
#include "my_sys.h"
#include "my_systime.h"
#include "mysql/psi/mysql_thread.h"
#include "sql/mysqld.h"  // lower_case_table_names
#include "sql/table.h"
#include "storage/perfschema/pfs_buffer_container.h"
#include "storage/perfschema/pfs_builtin_memory.h"
#include "storage/perfschema/pfs_column_values.h"
#include "storage/perfschema/pfs_events_waits.h"
#include "storage/perfschema/pfs_global.h"
#include "storage/perfschema/pfs_instr.h"
#include "storage/perfschema/pfs_program.h"
#include "storage/perfschema/pfs_setup_object.h"
#include "storage/perfschema/pfs_timer.h"

/**
  @defgroup performance_schema_buffers Performance Schema Buffers
  @ingroup performance_schema_implementation
  @{
*/

/**
  Global performance schema flag.
  Indicate if the performance schema is enabled.
  This flag is set at startup, and never changes.
*/
bool pfs_enabled = true;

/**
  Global performance schema reference count for plugin and component events.
  Incremented when a shared library is being unloaded, decremented when
  the performance schema is finished processing the event.
*/
std::atomic<uint32> pfs_unload_plugin_ref_count(0);

/**
  PFS_INSTRUMENT option settings array
 */
Pfs_instr_config_array *pfs_instr_config_array = NULL;

static void configure_instr_class(PFS_instr_class *entry);

static void init_instr_class(PFS_instr_class *klass, const char *name,
                             uint name_length, int flags, int volatility,
                             const char *documentation,
                             PFS_class_type class_type);

/**
  Current number of elements in mutex_class_array.
  This global variable is written to during:
  - the performance schema initialization
  - a plugin initialization
*/
static std::atomic<uint32> mutex_class_dirty_count{0};
static std::atomic<uint32> mutex_class_allocated_count{0};
static std::atomic<uint32> rwlock_class_dirty_count{0};
static std::atomic<uint32> rwlock_class_allocated_count{0};
static std::atomic<uint32> cond_class_dirty_count{0};
static std::atomic<uint32> cond_class_allocated_count{0};

/** Size of the mutex class array. @sa mutex_class_array */
ulong mutex_class_max = 0;
/** Number of mutex class lost. @sa mutex_class_array */
ulong mutex_class_lost = 0;
/** Size of the rwlock class array. @sa rwlock_class_array */
ulong rwlock_class_max = 0;
/** Number of rwlock class lost. @sa rwlock_class_array */
ulong rwlock_class_lost = 0;
/** Size of the condition class array. @sa cond_class_array */
ulong cond_class_max = 0;
/** Number of condition class lost. @sa cond_class_array */
ulong cond_class_lost = 0;
/** Size of the thread class array. @sa thread_class_array */
ulong thread_class_max = 0;
/** Number of thread class lost. @sa thread_class_array */
ulong thread_class_lost = 0;
/** Size of the file class array. @sa file_class_array */
ulong file_class_max = 0;
/** Number of file class lost. @sa file_class_array */
ulong file_class_lost = 0;
/** Size of the stage class array. @sa stage_class_array */
ulong stage_class_max = 0;
/** Number of stage class lost. @sa stage_class_array */
ulong stage_class_lost = 0;
/** Size of the statement class array. @sa statement_class_array */
ulong statement_class_max = 0;
/** Number of statement class lost. @sa statement_class_array */
ulong statement_class_lost = 0;
/** Size of the socket class array. @sa socket_class_array */
ulong socket_class_max = 0;
/** Number of socket class lost. @sa socket_class_array */
ulong socket_class_lost = 0;
/** Size of the memory class array. @sa memory_class_array */
ulong memory_class_max = 0;
/** Number of memory class lost. @sa memory_class_array */
ulong memory_class_lost = 0;

/**
  Number of transaction classes. Although there is only one transaction class,
  this is used for sizing by other event classes.
  @sa global_transaction_class
*/
ulong transaction_class_max = 0;

/**
  Number of error classes. Although there is only one error class,
  this is kept for future use if there is more error classification required.
  @sa global_error_class
*/
ulong error_class_max = 0;

PFS_mutex_class *mutex_class_array = NULL;
PFS_rwlock_class *rwlock_class_array = NULL;
PFS_cond_class *cond_class_array = NULL;

/**
  Current number or elements in thread_class_array.
  This global variable is written to during:
  - the performance schema initialization
  - a plugin initialization
*/
static std::atomic<uint32> thread_class_dirty_count{0};
static std::atomic<uint32> thread_class_allocated_count{0};

static PFS_thread_class *thread_class_array = NULL;

PFS_ALIGNED PFS_single_stat global_idle_stat;
PFS_ALIGNED PFS_table_io_stat global_table_io_stat;
PFS_ALIGNED PFS_table_lock_stat global_table_lock_stat;
PFS_ALIGNED PFS_single_stat global_metadata_stat;
PFS_ALIGNED PFS_transaction_stat global_transaction_stat;
PFS_ALIGNED PFS_error_stat global_error_stat;
PFS_ALIGNED PFS_instr_class global_table_io_class;
PFS_ALIGNED PFS_instr_class global_table_lock_class;
PFS_ALIGNED PFS_instr_class global_idle_class;
PFS_ALIGNED PFS_instr_class global_metadata_class;
PFS_ALIGNED PFS_error_class global_error_class;
PFS_ALIGNED PFS_transaction_class global_transaction_class;

/**
  Hash index for instrumented table shares.
  This index is searched by table fully qualified name (@c PFS_table_share_key),
  and points to instrumented table shares (@c PFS_table_share).
  @sa PFS_table_share_key
  @sa PFS_table_share
  @sa table_share_hash_get_key
  @sa get_table_share_hash_pins
*/
LF_HASH table_share_hash;
/** True if table_share_hash is initialized. */
static bool table_share_hash_inited = false;

static std::atomic<uint32> file_class_dirty_count{0};
static std::atomic<uint32> file_class_allocated_count{0};

PFS_file_class *file_class_array = NULL;

static std::atomic<uint32> stage_class_dirty_count{0};
static std::atomic<uint32> stage_class_allocated_count{0};

static PFS_stage_class *stage_class_array = NULL;

static std::atomic<uint32> statement_class_dirty_count{0};
static std::atomic<uint32> statement_class_allocated_count{0};

static PFS_statement_class *statement_class_array = NULL;

static std::atomic<uint32> socket_class_dirty_count{0};
static std::atomic<uint32> socket_class_allocated_count{0};

static PFS_socket_class *socket_class_array = NULL;

static std::atomic<uint32> memory_class_dirty_count{0};
static std::atomic<uint32> memory_class_allocated_count{0};

static std::atomic<PFS_memory_class *> memory_class_array{nullptr};

uint mutex_class_start = 0;
uint rwlock_class_start = 0;
uint cond_class_start = 0;
uint file_class_start = 0;
uint wait_class_max = 0;
uint socket_class_start = 0;

void init_event_name_sizing(const PFS_global_param *param) {
  /* global table I/O, table lock, idle, metadata */
  mutex_class_start = COUNT_GLOBAL_EVENT_INDEX;
  rwlock_class_start = mutex_class_start + param->m_mutex_class_sizing;
  cond_class_start = rwlock_class_start + param->m_rwlock_class_sizing;
  file_class_start = cond_class_start + param->m_cond_class_sizing;
  socket_class_start = file_class_start + param->m_file_class_sizing;
  wait_class_max = socket_class_start + param->m_socket_class_sizing;
}

void register_global_classes() {
  /* Table I/O class */
  init_instr_class(&global_table_io_class, table_io_class_name.str,
                   (uint)table_io_class_name.length, 0, 0, PSI_DOCUMENT_ME,
                   PFS_CLASS_TABLE_IO);
  global_table_io_class.m_event_name_index = GLOBAL_TABLE_IO_EVENT_INDEX;
  configure_instr_class(&global_table_io_class);

  /* Table lock class */
  init_instr_class(&global_table_lock_class, table_lock_class_name.str,
                   (uint)table_lock_class_name.length, 0, 0, PSI_DOCUMENT_ME,
                   PFS_CLASS_TABLE_LOCK);
  global_table_lock_class.m_event_name_index = GLOBAL_TABLE_LOCK_EVENT_INDEX;
  configure_instr_class(&global_table_lock_class);

  /* Idle class */
  init_instr_class(&global_idle_class, idle_class_name.str,
                   (uint)idle_class_name.length, PSI_FLAG_USER,
                   0, /* no volatility */
                   PSI_DOCUMENT_ME, PFS_CLASS_IDLE);
  global_idle_class.m_event_name_index = GLOBAL_IDLE_EVENT_INDEX;
  configure_instr_class(&global_idle_class);

  /* Metadata class */
  init_instr_class(&global_metadata_class, metadata_lock_class_name.str,
                   (uint)metadata_lock_class_name.length, 0, 0, PSI_DOCUMENT_ME,
                   PFS_CLASS_METADATA);
  global_metadata_class.m_event_name_index = GLOBAL_METADATA_EVENT_INDEX;
  configure_instr_class(&global_metadata_class);

  /* Error class */
  init_instr_class(&global_error_class, error_class_name.str,
                   (uint)error_class_name.length, 0, 0, PSI_DOCUMENT_ME,
                   PFS_CLASS_ERROR);
  global_error_class.m_event_name_index = GLOBAL_ERROR_INDEX;
  global_error_class.m_enabled = true; /* Enabled by default */
  configure_instr_class(&global_error_class);
  global_error_class.m_timed = false; /* Not applicable */
  error_class_max = 1;                /* only one error class as of now. */

  /* Transaction class */
  init_instr_class(&global_transaction_class, transaction_instrument_prefix.str,
                   (uint)transaction_instrument_prefix.length, 0, 0,
                   PSI_DOCUMENT_ME, PFS_CLASS_TRANSACTION);
  global_transaction_class.m_event_name_index = GLOBAL_TRANSACTION_INDEX;
  configure_instr_class(&global_transaction_class);
  transaction_class_max = 1; /* used for sizing by other event classes */
}

/**
  Initialize the instrument synch class buffers.
  @param mutex_class_sizing           max number of mutex class
  @param rwlock_class_sizing          max number of rwlock class
  @param cond_class_sizing            max number of condition class
  @return 0 on success
*/
int init_sync_class(uint mutex_class_sizing, uint rwlock_class_sizing,
                    uint cond_class_sizing) {
  mutex_class_dirty_count = mutex_class_allocated_count = 0;
  rwlock_class_dirty_count = rwlock_class_allocated_count = 0;
  cond_class_dirty_count = cond_class_allocated_count = 0;
  mutex_class_max = mutex_class_sizing;
  rwlock_class_max = rwlock_class_sizing;
  cond_class_max = cond_class_sizing;
  mutex_class_lost = rwlock_class_lost = cond_class_lost = 0;

  mutex_class_array = NULL;
  rwlock_class_array = NULL;
  cond_class_array = NULL;

  if (mutex_class_max > 0) {
    mutex_class_array = PFS_MALLOC_ARRAY(
        &builtin_memory_mutex_class, mutex_class_max, sizeof(PFS_mutex_class),
        PFS_mutex_class, MYF(MY_ZEROFILL));
    if (unlikely(mutex_class_array == NULL)) {
      return 1;
    }
  }

  if (rwlock_class_max > 0) {
    rwlock_class_array = PFS_MALLOC_ARRAY(
        &builtin_memory_rwlock_class, rwlock_class_max,
        sizeof(PFS_rwlock_class), PFS_rwlock_class, MYF(MY_ZEROFILL));
    if (unlikely(rwlock_class_array == NULL)) {
      return 1;
    }
  }

  if (cond_class_max > 0) {
    cond_class_array = PFS_MALLOC_ARRAY(&builtin_memory_cond_class,
                                        cond_class_max, sizeof(PFS_cond_class),
                                        PFS_cond_class, MYF(MY_ZEROFILL));
    if (unlikely(cond_class_array == NULL)) {
      return 1;
    }
  }

  return 0;
}

/** Cleanup the instrument synch class buffers. */
void cleanup_sync_class(void) {
  unsigned int i;

  if (mutex_class_array != NULL) {
    for (i = 0; i < mutex_class_max; i++) {
      my_free(mutex_class_array[i].m_documentation);
    }
  }

  PFS_FREE_ARRAY(&builtin_memory_mutex_class, mutex_class_max,
                 sizeof(PFS_mutex_class), mutex_class_array);
  mutex_class_array = NULL;
  mutex_class_dirty_count = mutex_class_allocated_count = mutex_class_max = 0;

  if (rwlock_class_array != NULL) {
    for (i = 0; i < rwlock_class_max; i++) {
      my_free(rwlock_class_array[i].m_documentation);
    }
  }

  PFS_FREE_ARRAY(&builtin_memory_rwlock_class, rwlock_class_max,
                 sizeof(PFS_rwlock_class), rwlock_class_array);
  rwlock_class_array = NULL;
  rwlock_class_dirty_count = rwlock_class_allocated_count = rwlock_class_max =
      0;

  if (cond_class_array != NULL) {
    for (i = 0; i < cond_class_max; i++) {
      my_free(cond_class_array[i].m_documentation);
    }
  }

  PFS_FREE_ARRAY(&builtin_memory_cond_class, cond_class_max,
                 sizeof(PFS_cond_class), cond_class_array);
  cond_class_array = NULL;
  cond_class_dirty_count = cond_class_allocated_count = cond_class_max = 0;
}

/**
  Initialize the thread class buffer.
  @param thread_class_sizing          max number of thread class
  @return 0 on success
*/
int init_thread_class(uint thread_class_sizing) {
  int result = 0;
  thread_class_dirty_count = thread_class_allocated_count = 0;
  thread_class_max = thread_class_sizing;
  thread_class_lost = 0;

  if (thread_class_max > 0) {
    thread_class_array = PFS_MALLOC_ARRAY(
        &builtin_memory_thread_class, thread_class_max,
        sizeof(PFS_thread_class), PFS_thread_class, MYF(MY_ZEROFILL));
    if (unlikely(thread_class_array == NULL)) {
      result = 1;
    }
  } else {
    thread_class_array = NULL;
  }

  return result;
}

/** Cleanup the thread class buffers. */
void cleanup_thread_class(void) {
  unsigned int i;

  if (thread_class_array != NULL) {
    for (i = 0; i < thread_class_max; i++) {
      my_free(thread_class_array[i].m_documentation);
    }
  }

  PFS_FREE_ARRAY(&builtin_memory_thread_class, thread_class_max,
                 sizeof(PFS_thread_class), thread_class_array);
  thread_class_array = NULL;
  thread_class_dirty_count = thread_class_allocated_count = 0;
  thread_class_max = 0;
}

/**
  Initialize the table share buffer.
  @param table_share_sizing           max number of table share
  @return 0 on success
*/
int init_table_share(uint table_share_sizing) {
  if (global_table_share_container.init(table_share_sizing)) {
    return 1;
  }

  return 0;
}

/** Cleanup the table share buffers. */
void cleanup_table_share(void) { global_table_share_container.cleanup(); }

/** get_key function for @c table_share_hash. */
static const uchar *table_share_hash_get_key(const uchar *entry,
                                             size_t *length) {
  const PFS_table_share *const *typed_entry;
  const PFS_table_share *share;
  const void *result;
  typed_entry = reinterpret_cast<const PFS_table_share *const *>(entry);
  DBUG_ASSERT(typed_entry != NULL);
  share = *typed_entry;
  DBUG_ASSERT(share != NULL);
  *length = share->m_key.m_key_length;
  result = &share->m_key.m_hash_key[0];
  return reinterpret_cast<const uchar *>(result);
}

/** Initialize the table share hash table. */
int init_table_share_hash(const PFS_global_param *param) {
  if ((!table_share_hash_inited) && (param->m_table_share_sizing != 0)) {
    lf_hash_init(&table_share_hash, sizeof(PFS_table_share *), LF_HASH_UNIQUE,
                 0, 0, table_share_hash_get_key, &my_charset_bin);
    table_share_hash_inited = true;
  }
  return 0;
}

/** Cleanup the table share hash table. */
void cleanup_table_share_hash(void) {
  if (table_share_hash_inited) {
    lf_hash_destroy(&table_share_hash);
    table_share_hash_inited = false;
  }
}

/**
  Get the hash pins for @sa table_share_hash.
  @param thread The running thread.
  @returns The LF_HASH pins for the thread.
*/
static LF_PINS *get_table_share_hash_pins(PFS_thread *thread) {
  if (unlikely(thread->m_table_share_hash_pins == NULL)) {
    if (!table_share_hash_inited) {
      return NULL;
    }
    thread->m_table_share_hash_pins = lf_hash_get_pins(&table_share_hash);
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
static void set_table_share_key(PFS_table_share_key *key, bool temporary,
                                const char *schema_name,
                                size_t schema_name_length,
                                const char *table_name,
                                size_t table_name_length) {
  DBUG_ASSERT(schema_name_length <= NAME_LEN);
  DBUG_ASSERT(table_name_length <= NAME_LEN);
  char *saved_schema_name;
  char *saved_table_name;

  char *ptr = &key->m_hash_key[0];
  ptr[0] = (temporary ? OBJECT_TYPE_TEMPORARY_TABLE : OBJECT_TYPE_TABLE);
  ptr++;
  saved_schema_name = ptr;
  memcpy(ptr, schema_name, schema_name_length);
  ptr += schema_name_length;
  ptr[0] = 0;
  ptr++;
  saved_table_name = ptr;
  memcpy(ptr, table_name, table_name_length);
  ptr += table_name_length;
  ptr[0] = 0;
  ptr++;
  key->m_key_length = ptr - &key->m_hash_key[0];

  if (lower_case_table_names) {
    my_casedn_str(files_charset_info, saved_schema_name);
    my_casedn_str(files_charset_info, saved_table_name);
  }
}

/**
  Find an existing table share lock instrumentation.
  @return a table share lock.
*/
PFS_table_share_lock *PFS_table_share::find_lock_stat() const {
  PFS_table_share *that = const_cast<PFS_table_share *>(this);
  return that->m_race_lock_stat.load();
}

/**
  Find or create a table share lock instrumentation.
  @return a table share lock, or NULL.
*/
PFS_table_share_lock *PFS_table_share::find_or_create_lock_stat() {
  PFS_table_share_lock *pfs = this->m_race_lock_stat.load();
  if (pfs != NULL) {
    return pfs;
  }

  /* (2) Create a lock stat */
  PFS_table_share_lock *new_pfs = create_table_share_lock_stat();
  if (new_pfs == NULL) {
    return NULL;
  }
  new_pfs->m_owner = this;

  /* (3) Atomic CAS */
  if (atomic_compare_exchange_strong(&this->m_race_lock_stat, &pfs, new_pfs)) {
    /* Ok. */
    return new_pfs;
  }

  /* Collision with another thread that also executed (2) and (3). */
  release_table_share_lock_stat(new_pfs);

  return pfs;
}

/** Destroy a table share lock instrumentation. */
void PFS_table_share::destroy_lock_stat() {
  PFS_table_share_lock *new_ptr = NULL;
  PFS_table_share_lock *old_ptr = this->m_race_lock_stat.exchange(new_ptr);
  if (old_ptr != NULL) {
    release_table_share_lock_stat(old_ptr);
  }
}

/**
  Find an existing table share index instrumentation.
  @return a table share index
*/
PFS_table_share_index *PFS_table_share::find_index_stat(uint index) const {
  DBUG_ASSERT(index <= MAX_INDEXES);

  return this->m_race_index_stat[index].load();
}

/**
  Find or create a table share index instrumentation.
  @param server_share the server TABLE_SHARE structure
  @param index the index
  @return a table share index, or NULL
*/
PFS_table_share_index *PFS_table_share::find_or_create_index_stat(
    const TABLE_SHARE *server_share, uint index) {
  DBUG_ASSERT(index <= MAX_INDEXES);

  /* (1) Atomic Load */
  PFS_table_share_index *pfs = this->m_race_index_stat[index].load();
  if (pfs != NULL) {
    return pfs;
  }

  /* (2) Create an index stat */
  PFS_table_share_index *new_pfs =
      create_table_share_index_stat(server_share, index);
  if (new_pfs == NULL) {
    return NULL;
  }
  new_pfs->m_owner = this;

  /* (3) Atomic CAS */
  if (atomic_compare_exchange_strong(&this->m_race_index_stat[index], &pfs,
                                     new_pfs)) {
    /* Ok. */
    return new_pfs;
  }

  /* Collision with another thread that also executed (2) and (3). */
  release_table_share_index_stat(new_pfs);

  return pfs;
}

/** Destroy table share index instrumentation. */
void PFS_table_share::destroy_index_stats() {
  for (uint index = 0; index <= MAX_INDEXES; index++) {
    PFS_table_share_index *new_ptr = NULL;
    PFS_table_share_index *old_ptr =
        this->m_race_index_stat[index].exchange(new_ptr);
    if (old_ptr != NULL) {
      release_table_share_index_stat(old_ptr);
    }
  }
}

void PFS_table_share::refresh_setup_object_flags(PFS_thread *thread) {
  lookup_setup_object(thread, OBJECT_TYPE_TABLE, m_schema_name,
                      m_schema_name_length, m_table_name, m_table_name_length,
                      &m_enabled, &m_timed);
}

/**
  Initialize the table lock stat buffer.
  @param table_stat_sizing           max number of table lock statistics
  @return 0 on success
*/
int init_table_share_lock_stat(uint table_stat_sizing) {
  if (global_table_share_lock_container.init(table_stat_sizing)) {
    return 1;
  }

  return 0;
}

/**
  Create a table share lock instrumentation.
  @return table share lock instrumentation, or NULL
*/
PFS_table_share_lock *create_table_share_lock_stat() {
  PFS_table_share_lock *pfs = NULL;
  pfs_dirty_state dirty_state;

  /* Create a new record in table stat array. */
  pfs = global_table_share_lock_container.allocate(&dirty_state);
  if (pfs != NULL) {
    /* Reset the stats. */
    pfs->m_stat.reset();

    /* Use this stat buffer. */
    pfs->m_lock.dirty_to_allocated(&dirty_state);
  }

  return pfs;
}

/** Release a table share lock instrumentation. */
void release_table_share_lock_stat(PFS_table_share_lock *pfs) {
  pfs->m_owner = NULL;
  global_table_share_lock_container.deallocate(pfs);
  return;
}

/** Cleanup the table stat buffers. */
void cleanup_table_share_lock_stat(void) {
  global_table_share_lock_container.cleanup();
}

/**
  Initialize table index stat buffer.
  @param index_stat_sizing           max number of index statistics
  @return 0 on success
*/
int init_table_share_index_stat(uint index_stat_sizing) {
  if (global_table_share_index_container.init(index_stat_sizing)) {
    return 1;
  }

  return 0;
}

/**
  Create a table share index instrumentation.
  @return table share index instrumentation, or NULL
*/
PFS_table_share_index *create_table_share_index_stat(
    const TABLE_SHARE *server_share, uint server_index) {
  DBUG_ASSERT((server_share != NULL) || (server_index == MAX_INDEXES));

  PFS_table_share_index *pfs = NULL;
  pfs_dirty_state dirty_state;

  /* Create a new record in index stat array. */
  pfs = global_table_share_index_container.allocate(&dirty_state);
  if (pfs != NULL) {
    if (server_index == MAX_INDEXES) {
      pfs->m_key.m_name_length = 0;
    } else {
      KEY *key_info = server_share->key_info + server_index;
      size_t len = strlen(key_info->name);

      memcpy(pfs->m_key.m_name, key_info->name, len);
      pfs->m_key.m_name_length = len;
    }

    /* Reset the stats. */
    pfs->m_stat.reset();

    /* Use this stat buffer. */
    pfs->m_lock.dirty_to_allocated(&dirty_state);
  }

  return pfs;
}

/** Release a table share index instrumentation. */
void release_table_share_index_stat(PFS_table_share_index *pfs) {
  pfs->m_owner = NULL;
  global_table_share_index_container.deallocate(pfs);
  return;
}

/** Cleanup the table stat buffers. */
void cleanup_table_share_index_stat(void) {
  global_table_share_index_container.cleanup();
}

/**
  Initialize the file class buffer.
  @param file_class_sizing            max number of file class
  @return 0 on success
*/
int init_file_class(uint file_class_sizing) {
  int result = 0;
  file_class_dirty_count = file_class_allocated_count = 0;
  file_class_max = file_class_sizing;
  file_class_lost = 0;

  if (file_class_max > 0) {
    file_class_array = PFS_MALLOC_ARRAY(&builtin_memory_file_class,
                                        file_class_max, sizeof(PFS_file_class),
                                        PFS_file_class, MYF(MY_ZEROFILL));
    if (unlikely(file_class_array == NULL)) {
      return 1;
    }
  } else {
    file_class_array = NULL;
  }

  return result;
}

/** Cleanup the file class buffers. */
void cleanup_file_class(void) {
  unsigned int i;

  if (file_class_array != NULL) {
    for (i = 0; i < file_class_max; i++) {
      my_free(file_class_array[i].m_documentation);
    }
  }

  PFS_FREE_ARRAY(&builtin_memory_file_class, file_class_max,
                 sizeof(PFS_file_class), file_class_array);
  file_class_array = NULL;
  file_class_dirty_count = file_class_allocated_count = 0;
  file_class_max = 0;
}

/**
  Initialize the stage class buffer.
  @param stage_class_sizing            max number of stage class
  @return 0 on success
*/
int init_stage_class(uint stage_class_sizing) {
  int result = 0;
  stage_class_dirty_count = stage_class_allocated_count = 0;
  stage_class_max = stage_class_sizing;
  stage_class_lost = 0;

  if (stage_class_max > 0) {
    stage_class_array = PFS_MALLOC_ARRAY(
        &builtin_memory_stage_class, stage_class_max, sizeof(PFS_stage_class),
        PFS_stage_class, MYF(MY_ZEROFILL));
    if (unlikely(stage_class_array == NULL)) {
      return 1;
    }
  } else {
    stage_class_array = NULL;
  }

  return result;
}

/** Cleanup the stage class buffers. */
void cleanup_stage_class(void) {
  unsigned int i;

  if (stage_class_array != NULL) {
    for (i = 0; i < stage_class_max; i++) {
      my_free(stage_class_array[i].m_documentation);
    }
  }

  PFS_FREE_ARRAY(&builtin_memory_stage_class, stage_class_max,
                 sizeof(PFS_stage_class), stage_class_array);
  stage_class_array = NULL;
  stage_class_dirty_count = stage_class_allocated_count = 0;
  stage_class_max = 0;
}

/**
  Initialize the statement class buffer.
  @param statement_class_sizing            max number of statement class
  @return 0 on success
*/
int init_statement_class(uint statement_class_sizing) {
  int result = 0;
  statement_class_dirty_count = statement_class_allocated_count = 0;
  statement_class_max = statement_class_sizing;
  statement_class_lost = 0;

  if (statement_class_max > 0) {
    statement_class_array = PFS_MALLOC_ARRAY(
        &builtin_memory_statement_class, statement_class_max,
        sizeof(PFS_statement_class), PFS_statement_class, MYF(MY_ZEROFILL));
    if (unlikely(statement_class_array == NULL)) {
      return 1;
    }
  } else {
    statement_class_array = NULL;
  }

  return result;
}

/** Cleanup the statement class buffers. */
void cleanup_statement_class(void) {
  unsigned int i;

  if (statement_class_array != NULL) {
    for (i = 0; i < statement_class_max; i++) {
      my_free(statement_class_array[i].m_documentation);
    }
  }

  PFS_FREE_ARRAY(&builtin_memory_statement_class, statement_class_max,
                 sizeof(PFS_statement_class), statement_class_array);
  statement_class_array = NULL;
  statement_class_dirty_count = statement_class_allocated_count = 0;
  statement_class_max = 0;
}

/**
  Initialize the socket class buffer.
  @param socket_class_sizing            max number of socket class
  @return 0 on success
*/
int init_socket_class(uint socket_class_sizing) {
  int result = 0;
  socket_class_dirty_count = socket_class_allocated_count = 0;
  socket_class_max = socket_class_sizing;
  socket_class_lost = 0;

  if (socket_class_max > 0) {
    socket_class_array = PFS_MALLOC_ARRAY(
        &builtin_memory_socket_class, socket_class_max,
        sizeof(PFS_socket_class), PFS_socket_class, MYF(MY_ZEROFILL));
    if (unlikely(socket_class_array == NULL)) {
      return 1;
    }
  } else {
    socket_class_array = NULL;
  }

  return result;
}

/** Cleanup the socket class buffers. */
void cleanup_socket_class(void) {
  unsigned int i;

  if (socket_class_array != NULL) {
    for (i = 0; i < socket_class_max; i++) {
      my_free(socket_class_array[i].m_documentation);
    }
  }

  PFS_FREE_ARRAY(&builtin_memory_socket_class, socket_class_max,
                 sizeof(PFS_socket_class), socket_class_array);
  socket_class_array = NULL;
  socket_class_dirty_count = socket_class_allocated_count = 0;
  socket_class_max = 0;
}

/**
  Initialize the memory class buffer.
  @param memory_class_sizing            max number of memory class
  @return 0 on success
*/
int init_memory_class(uint memory_class_sizing) {
  int result = 0;
  memory_class_dirty_count = memory_class_allocated_count = 0;
  memory_class_max = memory_class_sizing;
  memory_class_lost = 0;

  if (memory_class_max > 0) {
    memory_class_array = PFS_MALLOC_ARRAY(
        &builtin_memory_memory_class, memory_class_max,
        sizeof(PFS_memory_class), PFS_memory_class, MYF(MY_ZEROFILL));
    if (unlikely(memory_class_array.load() == nullptr)) {
      return 1;
    }
  } else {
    memory_class_array = nullptr;
  }

  return result;
}

/** Cleanup the memory class buffers. */
void cleanup_memory_class(void) {
  unsigned int i;

  if (memory_class_array.load() != nullptr) {
    for (i = 0; i < memory_class_max; i++) {
      my_free(memory_class_array[i].m_documentation);
    }
  }

  PFS_FREE_ARRAY(&builtin_memory_memory_class, memory_class_max,
                 sizeof(PFS_memory_class), memory_class_array);
  memory_class_array = NULL;
  memory_class_dirty_count = memory_class_allocated_count = 0;
  memory_class_max = 0;
}

static void init_instr_class(PFS_instr_class *klass, const char *name,
                             uint name_length, int flags, int volatility,
                             const char *documentation,
                             PFS_class_type class_type) {
  DBUG_ASSERT(name_length <= PFS_MAX_INFO_NAME_LENGTH);
  memset(klass, 0, sizeof(PFS_instr_class));
  strncpy(klass->m_name, name, name_length);
  klass->m_name_length = name_length;
  klass->m_flags = flags;
  klass->m_volatility = volatility;
  klass->m_enabled = true;
  klass->m_timed = true;
  klass->m_type = class_type;

  klass->m_documentation = NULL;
  if (documentation != NULL) {
    /* PSI_DOCUMENT_ME is an empty string. */
    if (documentation[0] != '\0') {
      klass->m_documentation =
          my_strdup(PSI_NOT_INSTRUMENTED, documentation, 0);
    }
  }
}

/**
  Set user-defined configuration values for an instrument.
*/
static void configure_instr_class(PFS_instr_class *entry) {
  uint match_length = 0; /* length of matching pattern */

  // May be NULL in unit tests
  if (pfs_instr_config_array == NULL) {
    return;
  }
  Pfs_instr_config_array::iterator it = pfs_instr_config_array->begin();
  for (; it != pfs_instr_config_array->end(); it++) {
    PFS_instr_config *e = *it;

    /**
      Compare class name to all configuration entries. In case of multiple
      matches, the longer specification wins. For example, the pattern
      'ABC/DEF/GHI=ON' has precedence over 'ABC/DEF/%=OFF' regardless of
      position within the configuration file or command line.

      Consecutive wildcards affect the count.
    */
    if (!my_wildcmp(&my_charset_latin1, entry->m_name,
                    entry->m_name + entry->m_name_length, e->m_name,
                    e->m_name + e->m_name_length, '\\', '?', '%')) {
      if (e->m_name_length >= match_length) {
        entry->m_enabled = e->m_enabled;
        entry->m_timed = e->m_timed;
        match_length = MY_MAX(e->m_name_length, match_length);
      }
    }
  }
}

#define REGISTER_CLASS_BODY_PART(INDEX, ARRAY, MAX, NAME, NAME_LENGTH) \
  for (INDEX = 0; INDEX < MAX; INDEX++) {                              \
    entry = &ARRAY[INDEX];                                             \
    if ((entry->m_name_length == NAME_LENGTH) &&                       \
        (strncmp(entry->m_name, NAME, NAME_LENGTH) == 0)) {            \
      DBUG_ASSERT(entry->m_flags == info->m_flags);                    \
      return (INDEX + 1);                                              \
    }                                                                  \
  }

/**
  Register a mutex instrumentation metadata.
  @param name                         the instrumented name
  @param name_length                  length in bytes of name
  @param info                         the instrumentation properties
  @return a mutex instrumentation key
*/
PFS_sync_key register_mutex_class(const char *name, uint name_length,
                                  PSI_mutex_info *info) {
  uint32 index;
  PFS_mutex_class *entry;

  /*
    This is a full array scan, which is not optimal.
    This is acceptable since this code is only used at startup,
    or when a plugin is loaded.
  */
  REGISTER_CLASS_BODY_PART(index, mutex_class_array, mutex_class_max, name,
                           name_length)
  /*
    Note that:
    mutex_class_dirty_count is incremented *before* an entry is added
    mutex_class_allocated_count is incremented *after* an entry is added
  */
  index = mutex_class_dirty_count++;

  if (index < mutex_class_max) {
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
    entry = &mutex_class_array[index];
    init_instr_class(entry, name, name_length, info->m_flags,
                     info->m_volatility, info->m_documentation,
                     PFS_CLASS_MUTEX);
    entry->m_mutex_stat.reset();
    entry->m_event_name_index = mutex_class_start + index;
    entry->m_singleton = NULL;
    entry->m_enabled = false; /* disabled by default */
    entry->m_timed = false;

    entry->enforce_valid_flags(PSI_FLAG_SINGLETON);

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
    ++mutex_class_allocated_count;
    return (index + 1);
  }

  /*
    Out of space, report to SHOW STATUS that
    the allocated memory was too small.
  */
  if (pfs_enabled) {
    mutex_class_lost++;
  }
  return 0;
}

/**
  Register a rwlock instrumentation metadata.
  @param name                         the instrumented name
  @param name_length                  length in bytes of name
  @param info                         the instrumentation properties
  @return a rwlock instrumentation key
*/
PFS_sync_key register_rwlock_class(const char *name, uint name_length,
                                   PSI_rwlock_info *info) {
  /* See comments in register_mutex_class */
  uint32 index;
  PFS_rwlock_class *entry;

  REGISTER_CLASS_BODY_PART(index, rwlock_class_array, rwlock_class_max, name,
                           name_length)

  index = rwlock_class_dirty_count++;

  if (index < rwlock_class_max) {
    entry = &rwlock_class_array[index];
    init_instr_class(entry, name, name_length, info->m_flags,
                     info->m_volatility, info->m_documentation,
                     PFS_CLASS_RWLOCK);
    entry->m_rwlock_stat.reset();
    entry->m_event_name_index = rwlock_class_start + index;
    entry->m_singleton = NULL;
    entry->m_enabled = false; /* disabled by default */
    entry->m_timed = false;

    entry->enforce_valid_flags(PSI_FLAG_SINGLETON | PSI_FLAG_RWLOCK_SX);

    /* Set user-defined configuration options for this instrument */
    configure_instr_class(entry);
    ++rwlock_class_allocated_count;
    return (index + 1);
  }

  if (pfs_enabled) {
    rwlock_class_lost++;
  }
  return 0;
}

/**
  Register a condition instrumentation metadata.
  @param name                         the instrumented name
  @param name_length                  length in bytes of name
  @param info                         the instrumentation properties
  @return a condition instrumentation key
*/
PFS_sync_key register_cond_class(const char *name, uint name_length,
                                 PSI_cond_info *info) {
  /* See comments in register_mutex_class */
  uint32 index;
  PFS_cond_class *entry;

  REGISTER_CLASS_BODY_PART(index, cond_class_array, cond_class_max, name,
                           name_length)

  index = cond_class_dirty_count++;

  if (index < cond_class_max) {
    entry = &cond_class_array[index];
    init_instr_class(entry, name, name_length, info->m_flags,
                     info->m_volatility, info->m_documentation, PFS_CLASS_COND);
    entry->m_event_name_index = cond_class_start + index;
    entry->m_singleton = NULL;
    entry->m_enabled = false; /* disabled by default */
    entry->m_timed = false;

    entry->enforce_valid_flags(PSI_FLAG_SINGLETON);

    /* Set user-defined configuration options for this instrument */
    configure_instr_class(entry);
    ++cond_class_allocated_count;
    return (index + 1);
  }

  if (pfs_enabled) {
    cond_class_lost++;
  }
  return 0;
}

#define FIND_CLASS_BODY(KEY, COUNT, ARRAY) \
  if ((KEY == 0) || (KEY > COUNT)) {       \
    return NULL;                           \
  }                                        \
  return &ARRAY[KEY - 1]

/**
  Find a mutex instrumentation class by key.
  @param key                          the instrument key
  @return the instrument class, or NULL
*/
PFS_mutex_class *find_mutex_class(PFS_sync_key key) {
  FIND_CLASS_BODY(key, mutex_class_allocated_count, mutex_class_array);
}

PFS_mutex_class *sanitize_mutex_class(PFS_mutex_class *unsafe) {
  SANITIZE_ARRAY_BODY(PFS_mutex_class, mutex_class_array, mutex_class_max,
                      unsafe);
}

/**
  Find a rwlock instrumentation class by key.
  @param key                          the instrument key
  @return the instrument class, or NULL
*/
PFS_rwlock_class *find_rwlock_class(PFS_sync_key key) {
  FIND_CLASS_BODY(key, rwlock_class_allocated_count, rwlock_class_array);
}

PFS_rwlock_class *sanitize_rwlock_class(PFS_rwlock_class *unsafe) {
  SANITIZE_ARRAY_BODY(PFS_rwlock_class, rwlock_class_array, rwlock_class_max,
                      unsafe);
}

/**
  Find a condition instrumentation class by key.
  @param key                          the instrument key
  @return the instrument class, or NULL
*/
PFS_cond_class *find_cond_class(PFS_sync_key key) {
  FIND_CLASS_BODY(key, cond_class_allocated_count, cond_class_array);
}

PFS_cond_class *sanitize_cond_class(PFS_cond_class *unsafe) {
  SANITIZE_ARRAY_BODY(PFS_cond_class, cond_class_array, cond_class_max, unsafe);
}

/**
  Register a thread instrumentation metadata.
  @param name                         the instrumented name
  @param name_length                  length in bytes of name
  @param info                         the instrumentation properties
  @return a thread instrumentation key
*/
PFS_thread_key register_thread_class(const char *name, uint name_length,
                                     PSI_thread_info *info) {
  /* See comments in register_mutex_class */
  uint32 index;
  PFS_thread_class *entry;

  for (index = 0; index < thread_class_max; index++) {
    entry = &thread_class_array[index];

    if ((entry->m_name_length == name_length) &&
        (strncmp(entry->m_name, name, name_length) == 0)) {
      return (index + 1);
    }
  }

  index = thread_class_dirty_count++;

  if (index < thread_class_max) {
    entry = &thread_class_array[index];

    init_instr_class(entry, name, name_length, info->m_flags,
                     info->m_volatility, info->m_documentation,
                     PFS_CLASS_THREAD);
    entry->m_singleton = NULL;
    entry->m_history = true;

    entry->enforce_valid_flags(PSI_FLAG_SINGLETON | PSI_FLAG_USER);

    configure_instr_class(entry);
    ++thread_class_allocated_count;
    return (index + 1);
  }

  if (pfs_enabled) {
    thread_class_lost++;
  }
  return 0;
}

/**
  Find a thread instrumentation class by key.
  @param key                          the instrument key
  @return the instrument class, or NULL
*/
PFS_thread_class *find_thread_class(PFS_sync_key key) {
  FIND_CLASS_BODY(key, thread_class_allocated_count, thread_class_array);
}

PFS_thread_class *sanitize_thread_class(PFS_thread_class *unsafe) {
  SANITIZE_ARRAY_BODY(PFS_thread_class, thread_class_array, thread_class_max,
                      unsafe);
}

/**
  Register a file instrumentation metadata.
  @param name                         the instrumented name
  @param name_length                  length in bytes of name
  @param info                         the instrumentation properties
  @return a file instrumentation key
*/
PFS_file_key register_file_class(const char *name, uint name_length,
                                 PSI_file_info *info) {
  /* See comments in register_mutex_class */
  uint32 index;
  PFS_file_class *entry;

  REGISTER_CLASS_BODY_PART(index, file_class_array, file_class_max, name,
                           name_length)

  index = file_class_dirty_count++;

  if (index < file_class_max) {
    entry = &file_class_array[index];
    init_instr_class(entry, name, name_length, info->m_flags,
                     info->m_volatility, info->m_documentation, PFS_CLASS_FILE);
    entry->m_event_name_index = file_class_start + index;
    entry->m_singleton = NULL;
    entry->m_enabled = true; /* enabled by default */
    entry->m_timed = true;

    entry->enforce_valid_flags(PSI_FLAG_SINGLETON);

    /* Set user-defined configuration options for this instrument */
    configure_instr_class(entry);
    ++file_class_allocated_count;

    return (index + 1);
  }

  if (pfs_enabled) {
    file_class_lost++;
  }
  return 0;
}

/**
  Register a stage instrumentation metadata.
  @param name                         the instrumented name
  @param prefix_length                length in bytes of the name prefix
  @param name_length                  length in bytes of name
  @param info                         the instrumentation properties
  @return a stage instrumentation key
*/
PFS_stage_key register_stage_class(const char *name, uint prefix_length,
                                   uint name_length, PSI_stage_info *info) {
  /* See comments in register_mutex_class */
  uint32 index;
  PFS_stage_class *entry;

  REGISTER_CLASS_BODY_PART(index, stage_class_array, stage_class_max, name,
                           name_length)

  index = stage_class_dirty_count++;

  if (index < stage_class_max) {
    entry = &stage_class_array[index];
    init_instr_class(entry, name, name_length, info->m_flags,
                     0, /* stages have no volatility */
                     info->m_documentation, PFS_CLASS_STAGE);
    entry->m_prefix_length = prefix_length;
    entry->m_event_name_index = index;

    entry->enforce_valid_flags(PSI_FLAG_STAGE_PROGRESS);

    if (entry->is_progress()) {
      /* Stages with progress information are enabled and timed by default */
      entry->m_enabled = true;
      entry->m_timed = true;
    } else {
      /* Stages without progress information are disabled by default */
      entry->m_enabled = false;
      entry->m_timed = false;
    }

    /* Set user-defined configuration options for this instrument */
    configure_instr_class(entry);
    ++stage_class_allocated_count;

    return (index + 1);
  }

  if (pfs_enabled) {
    stage_class_lost++;
  }
  return 0;
}

/**
  Register a statement instrumentation metadata.
  @param name                         the instrumented name
  @param name_length                  length in bytes of name
  @param info                         the instrumentation properties
  @return a statement instrumentation key
*/
PFS_statement_key register_statement_class(const char *name, uint name_length,
                                           PSI_statement_info *info) {
  /* See comments in register_mutex_class */
  uint32 index;
  PFS_statement_class *entry;

  REGISTER_CLASS_BODY_PART(index, statement_class_array, statement_class_max,
                           name, name_length)

  index = statement_class_dirty_count++;

  if (index < statement_class_max) {
    entry = &statement_class_array[index];
    init_instr_class(entry, name, name_length, info->m_flags,
                     0, /* statements have no volatility */
                     info->m_documentation, PFS_CLASS_STATEMENT);
    entry->m_event_name_index = index;
    entry->m_enabled = true; /* enabled by default */
    entry->m_timed = true;

    entry->enforce_valid_flags(PSI_FLAG_MUTABLE);

    /* Set user-defined configuration options for this instrument */
    configure_instr_class(entry);
    ++statement_class_allocated_count;

    return (index + 1);
  }

  if (pfs_enabled) {
    statement_class_lost++;
  }
  return 0;
}

/**
  Find a file instrumentation class by key.
  @param key                          the instrument key
  @return the instrument class, or NULL
*/
PFS_file_class *find_file_class(PFS_file_key key) {
  FIND_CLASS_BODY(key, file_class_allocated_count, file_class_array);
}

PFS_file_class *sanitize_file_class(PFS_file_class *unsafe) {
  SANITIZE_ARRAY_BODY(PFS_file_class, file_class_array, file_class_max, unsafe);
}

/**
  Find a stage instrumentation class by key.
  @param key                          the instrument key
  @return the instrument class, or NULL
*/
PFS_stage_class *find_stage_class(PFS_stage_key key) {
  FIND_CLASS_BODY(key, stage_class_allocated_count, stage_class_array);
}

PFS_stage_class *sanitize_stage_class(PFS_stage_class *unsafe) {
  SANITIZE_ARRAY_BODY(PFS_stage_class, stage_class_array, stage_class_max,
                      unsafe);
}

/**
  Find a statement instrumentation class by key.
  @param key                          the instrument key
  @return the instrument class, or NULL
*/
PFS_statement_class *find_statement_class(PFS_stage_key key) {
  FIND_CLASS_BODY(key, statement_class_allocated_count, statement_class_array);
}

PFS_statement_class *sanitize_statement_class(PFS_statement_class *unsafe) {
  SANITIZE_ARRAY_BODY(PFS_statement_class, statement_class_array,
                      statement_class_max, unsafe);
}

/**
  Register a socket instrumentation metadata.
  @param name                         the instrumented name
  @param name_length                  length in bytes of name
  @param info                         the instrumentation properties
  @return a socket instrumentation key
*/
PFS_socket_key register_socket_class(const char *name, uint name_length,
                                     PSI_socket_info *info) {
  /* See comments in register_mutex_class */
  uint32 index;
  PFS_socket_class *entry;

  REGISTER_CLASS_BODY_PART(index, socket_class_array, socket_class_max, name,
                           name_length)

  index = socket_class_dirty_count++;

  if (index < socket_class_max) {
    entry = &socket_class_array[index];
    init_instr_class(entry, name, name_length, info->m_flags,
                     info->m_volatility, info->m_documentation,
                     PFS_CLASS_SOCKET);
    entry->m_event_name_index = socket_class_start + index;
    entry->m_singleton = NULL;
    entry->m_enabled = false; /* disabled by default */
    entry->m_timed = false;

    entry->enforce_valid_flags(PSI_FLAG_SINGLETON | PSI_FLAG_USER);

    /* Set user-defined configuration options for this instrument */
    configure_instr_class(entry);
    ++socket_class_allocated_count;
    return (index + 1);
  }

  if (pfs_enabled) {
    socket_class_lost++;
  }
  return 0;
}

/**
  Find a socket instrumentation class by key.
  @param key                          the instrument key
  @return the instrument class, or NULL
*/
PFS_socket_class *find_socket_class(PFS_socket_key key) {
  FIND_CLASS_BODY(key, socket_class_allocated_count, socket_class_array);
}

PFS_socket_class *sanitize_socket_class(PFS_socket_class *unsafe) {
  SANITIZE_ARRAY_BODY(PFS_socket_class, socket_class_array, socket_class_max,
                      unsafe);
}

/**
  Register a memory instrumentation metadata.
  @param name                         the instrumented name
  @param name_length                  length in bytes of name
  @param info                         the instrumentation properties
  @return a memory instrumentation key
*/
PFS_memory_key register_memory_class(const char *name, uint name_length,
                                     PSI_memory_info *info) {
  /* See comments in register_mutex_class */
  uint32 index;
  PFS_memory_class *entry;

  REGISTER_CLASS_BODY_PART(index, memory_class_array, memory_class_max, name,
                           name_length)

  index = memory_class_dirty_count++;

  if (index < memory_class_max) {
    entry = &memory_class_array[index];
    init_instr_class(entry, name, name_length, info->m_flags,
                     info->m_volatility, info->m_documentation,
                     PFS_CLASS_MEMORY);
    entry->m_event_name_index = index;

    entry->enforce_valid_flags(PSI_FLAG_ONLY_GLOBAL_STAT);

    /* Set user-defined configuration options for this instrument */
    configure_instr_class(entry);
    entry->m_timed = false; /* Immutable */
    ++memory_class_allocated_count;
    return (index + 1);
  }

  if (pfs_enabled) {
    memory_class_lost++;
  }
  return 0;
}

/**
  Find a memory instrumentation class by key.
  @param key                          the instrument key
  @return the instrument class, or NULL
*/
PFS_memory_class *find_memory_class(PFS_memory_key key) {
  FIND_CLASS_BODY(key, memory_class_allocated_count, memory_class_array);
}

PFS_memory_class *sanitize_memory_class(PFS_memory_class *unsafe) {
  SANITIZE_ARRAY_BODY(PFS_memory_class, memory_class_array.load(),
                      memory_class_max, unsafe);
}

PFS_instr_class *find_table_class(uint index) {
  if (index == 1) {
    return &global_table_io_class;
  }
  if (index == 2) {
    return &global_table_lock_class;
  }
  return NULL;
}

PFS_instr_class *sanitize_table_class(PFS_instr_class *unsafe) {
  if (likely((&global_table_io_class == unsafe) ||
             (&global_table_lock_class == unsafe))) {
    return unsafe;
  }
  return NULL;
}

PFS_instr_class *find_idle_class(uint index) {
  if (index == 1) {
    return &global_idle_class;
  }
  return NULL;
}

PFS_instr_class *sanitize_idle_class(PFS_instr_class *unsafe) {
  if (likely(&global_idle_class == unsafe)) {
    return unsafe;
  }
  return NULL;
}

PFS_instr_class *find_metadata_class(uint index) {
  if (index == 1) {
    return &global_metadata_class;
  }
  return NULL;
}

PFS_instr_class *sanitize_metadata_class(PFS_instr_class *unsafe) {
  if (likely(&global_metadata_class == unsafe)) {
    return unsafe;
  }
  return NULL;
}

PFS_error_class *find_error_class(uint index) {
  if (index == 1) {
    return &global_error_class;
  }
  return NULL;
}

PFS_error_class *sanitize_error_class(PFS_error_class *unsafe) {
  if (likely(&global_error_class == unsafe)) {
    return unsafe;
  }
  return NULL;
}

PFS_transaction_class *find_transaction_class(uint index) {
  if (index == 1) {
    return &global_transaction_class;
  }
  return NULL;
}

PFS_transaction_class *sanitize_transaction_class(
    PFS_transaction_class *unsafe) {
  if (likely(&global_transaction_class == unsafe)) {
    return unsafe;
  }
  return NULL;
}

static int compare_keys(PFS_table_share *pfs, const TABLE_SHARE *share) {
  if (pfs->m_key_count != share->keys) {
    return 1;
  }

  size_t len;
  uint index = 0;
  uint key_count = share->keys;
  KEY *key_info = share->key_info;
  PFS_table_share_index *index_stat;

  for (; index < key_count; key_info++, index++) {
    index_stat = pfs->find_index_stat(index);
    if (index_stat != NULL) {
      len = strlen(key_info->name);

      if (len != index_stat->m_key.m_name_length) {
        return 1;
      }

      if (memcmp(index_stat->m_key.m_name, key_info->name, len) != 0) {
        return 1;
      }
    }
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
PFS_table_share *find_or_create_table_share(PFS_thread *thread, bool temporary,
                                            const TABLE_SHARE *share) {
  /* See comments in register_mutex_class */
  PFS_table_share_key key;

  LF_PINS *pins = get_table_share_hash_pins(thread);
  if (unlikely(pins == NULL)) {
    global_table_share_container.m_lost++;
    return NULL;
  }

  const char *schema_name = share->db.str;
  size_t schema_name_length = share->db.length;
  const char *table_name = share->table_name.str;
  size_t table_name_length = share->table_name.length;

  set_table_share_key(&key, temporary, schema_name, schema_name_length,
                      table_name, table_name_length);

  PFS_table_share **entry;
  uint retry_count = 0;
  const uint retry_max = 3;
  bool enabled = true;
  bool timed = true;
  PFS_table_share *pfs;
  pfs_dirty_state dirty_state;

search:
  entry = reinterpret_cast<PFS_table_share **>(lf_hash_search(
      &table_share_hash, pins, key.m_hash_key, key.m_key_length));
  if (entry && (entry != MY_LF_ERRPTR)) {
    pfs = *entry;
    pfs->inc_refcount();
    if (compare_keys(pfs, share) != 0) {
      /*
        Some DDL was detected.
        - keep the lock stats, they are unaffected
        - destroy the index stats, indexes changed.
        - adjust the expected key count
        - recreate index stats
      */
      pfs->destroy_index_stats();
      pfs->m_key_count = share->keys;
      for (uint index = 0; index < pfs->m_key_count; index++) {
        (void)pfs->find_or_create_index_stat(share, index);
      }
    }
    lf_hash_search_unpin(pins);
    return pfs;
  }

  lf_hash_search_unpin(pins);

  if (retry_count == 0) {
    lookup_setup_object(thread, OBJECT_TYPE_TABLE, schema_name,
                        schema_name_length, table_name, table_name_length,
                        &enabled, &timed);
    /*
      Even when enabled is false, a record is added in the dictionary:
      - It makes enabling a table already in the table cache possible,
      - It improves performances for the next time a TABLE_SHARE is reloaded
        in the table cache.
    */
  }

  pfs = global_table_share_container.allocate(&dirty_state);
  if (pfs != NULL) {
    pfs->m_key = key;
    pfs->m_schema_name = &pfs->m_key.m_hash_key[1];
    pfs->m_schema_name_length = schema_name_length;
    pfs->m_table_name = &pfs->m_key.m_hash_key[schema_name_length + 2];
    pfs->m_table_name_length = table_name_length;
    pfs->m_enabled = enabled;
    pfs->m_timed = timed;
    pfs->init_refcount();
    pfs->destroy_lock_stat();
    pfs->destroy_index_stats();
    pfs->m_key_count = share->keys;

    int res;
    pfs->m_lock.dirty_to_allocated(&dirty_state);
    res = lf_hash_insert(&table_share_hash, pins, &pfs);

    if (likely(res == 0)) {
      /* Create table share index stats. */
      for (uint index = 0; index < pfs->m_key_count; index++) {
        (void)pfs->find_or_create_index_stat(share, index);
      }
      return pfs;
    }

    global_table_share_container.deallocate(pfs);

    if (res > 0) {
      /* Duplicate insert by another thread */
      if (++retry_count > retry_max) {
        /* Avoid infinite loops */
        global_table_share_container.m_lost++;
        return NULL;
      }
      goto search;
    }

    /* OOM in lf_hash_insert */
    global_table_share_container.m_lost++;
    return NULL;
  }

  return NULL;
}

void PFS_table_share::aggregate_io(void) {
  uint index;
  uint safe_key_count = sanitize_index_count(m_key_count);
  PFS_table_share_index *from_stat;
  PFS_table_io_stat sum_io;

  /* Aggregate stats for each index, if any */
  for (index = 0; index < safe_key_count; index++) {
    from_stat = find_index_stat(index);
    if (from_stat != NULL) {
      sum_io.aggregate(&from_stat->m_stat);
      from_stat->m_stat.reset();
    }
  }

  /* Aggregate stats for the table */
  from_stat = find_index_stat(MAX_INDEXES);
  if (from_stat != NULL) {
    sum_io.aggregate(&from_stat->m_stat);
    from_stat->m_stat.reset();
  }

  /* Add this table stats to the global sink. */
  global_table_io_stat.aggregate(&sum_io);
}

void PFS_table_share::sum_io(PFS_single_stat *result, uint key_count) {
  uint index;
  PFS_table_share_index *stat;

  DBUG_ASSERT(key_count <= MAX_INDEXES);

  /* Sum stats for each index, if any */
  for (index = 0; index < key_count; index++) {
    stat = find_index_stat(index);
    if (stat != NULL) {
      stat->m_stat.sum(result);
    }
  }

  /* Sum stats for the table */
  stat = find_index_stat(MAX_INDEXES);
  if (stat != NULL) {
    stat->m_stat.sum(result);
  }
}

void PFS_table_share::sum_lock(PFS_single_stat *result) {
  PFS_table_share_lock *lock_stat;
  lock_stat = find_lock_stat();
  if (lock_stat != NULL) {
    lock_stat->m_stat.sum(result);
  }
}

void PFS_table_share::sum(PFS_single_stat *result, uint key_count) {
  sum_io(result, key_count);
  sum_lock(result);
}

void PFS_table_share::aggregate_lock(void) {
  PFS_table_share_lock *lock_stat;
  lock_stat = find_lock_stat();
  if (lock_stat != NULL) {
    global_table_lock_stat.aggregate(&lock_stat->m_stat);
    /* Reset lock stat. */
    lock_stat->m_stat.reset();
  }
}

void release_table_share(PFS_table_share *pfs) {
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
void drop_table_share(PFS_thread *thread, bool temporary,
                      const char *schema_name, uint schema_name_length,
                      const char *table_name, uint table_name_length) {
  PFS_table_share_key key;
  LF_PINS *pins = get_table_share_hash_pins(thread);
  if (unlikely(pins == NULL)) {
    return;
  }
  set_table_share_key(&key, temporary, schema_name, schema_name_length,
                      table_name, table_name_length);
  PFS_table_share **entry;
  entry = reinterpret_cast<PFS_table_share **>(lf_hash_search(
      &table_share_hash, pins, key.m_hash_key, key.m_key_length));
  if (entry && (entry != MY_LF_ERRPTR)) {
    PFS_table_share *pfs = *entry;
    lf_hash_delete(&table_share_hash, pins, pfs->m_key.m_hash_key,
                   pfs->m_key.m_key_length);
    pfs->destroy_lock_stat();
    pfs->destroy_index_stats();

    global_table_share_container.deallocate(pfs);
  }

  lf_hash_search_unpin(pins);
}

/**
  Sanitize an unsafe table_share pointer.
  @param unsafe The possibly corrupt pointer.
  @return A valid table_safe_pointer, or NULL.
*/
PFS_table_share *sanitize_table_share(PFS_table_share *unsafe) {
  return global_table_share_container.sanitize(unsafe);
}

/** Reset the wait statistics per instrument class. */
void reset_events_waits_by_class() {
  reset_file_class_io();
  reset_socket_class_io();
  global_idle_stat.reset();
  global_table_io_stat.reset();
  global_table_lock_stat.reset();
  global_metadata_stat.reset();
}

/** Reset the I/O statistics per file class. */
void reset_file_class_io(void) {
  PFS_file_class *pfs = file_class_array;
  PFS_file_class *pfs_last = file_class_array + file_class_max;

  for (; pfs < pfs_last; pfs++) {
    pfs->m_file_stat.m_io_stat.reset();
  }
}

/** Reset the I/O statistics per socket class. */
void reset_socket_class_io(void) {
  PFS_socket_class *pfs = socket_class_array;
  PFS_socket_class *pfs_last = socket_class_array + socket_class_max;

  for (; pfs < pfs_last; pfs++) {
    pfs->m_socket_stat.m_io_stat.reset();
  }
}

class Proc_table_share_derived_flags
    : public PFS_buffer_processor<PFS_table_share> {
 public:
  Proc_table_share_derived_flags(PFS_thread *thread) : m_thread(thread) {}

  virtual void operator()(PFS_table_share *pfs) {
    pfs->refresh_setup_object_flags(m_thread);
  }

 private:
  PFS_thread *m_thread;
};

void update_table_share_derived_flags(PFS_thread *thread) {
  Proc_table_share_derived_flags proc(thread);
  global_table_share_container.apply(proc);
}

class Proc_program_share_derived_flags
    : public PFS_buffer_processor<PFS_program> {
 public:
  Proc_program_share_derived_flags(PFS_thread *thread) : m_thread(thread) {}

  virtual void operator()(PFS_program *pfs) {
    pfs->refresh_setup_object_flags(m_thread);
  }

 private:
  PFS_thread *m_thread;
};

void update_program_share_derived_flags(PFS_thread *thread) {
  Proc_program_share_derived_flags proc(thread);
  global_program_container.apply(proc);
}

ulonglong gtid_monitoring_getsystime() {
  if (pfs_enabled) {
    return my_getsystime();
  }
  return 0;
}

/** @} (end of group performance_schema_buffers) */
