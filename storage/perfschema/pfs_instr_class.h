/* Copyright (c) 2008, 2022, Oracle and/or its affiliates.

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

#ifndef PFS_INSTR_CLASS_H
#define PFS_INSTR_CLASS_H

#include "my_config.h"

#include <assert.h>
#include <sys/types.h>
#include <atomic>

#include "lf.h"
#include "my_compiler.h"

#include "my_inttypes.h"
#include "mysql_com.h" /* NAME_LEN */
#include "mysqld_error.h"
#include "prealloced_array.h"
#include "storage/perfschema/pfs_column_types.h"
#include "storage/perfschema/pfs_global.h"
#include "storage/perfschema/pfs_lock.h"
#include "storage/perfschema/pfs_name.h"
#include "storage/perfschema/pfs_stat.h"
#include "storage/perfschema/terminology_use_previous_enum.h"

struct TABLE_SHARE;

/**
  @file storage/perfschema/pfs_instr_class.h
  Performance schema instruments metadata (declarations).
*/

/**
  Maximum length of an instrument name.
  For example, 'wait/sync/mutex/sql/LOCK_open' is an instrument name.
*/
#define PFS_MAX_INFO_NAME_LENGTH 128

/**
  Maximum length of the thread os name.
  Must include a terminating NUL character.
  Length is 16 because of linux pthread_setname_np(3)
  @see my_thread_self_setname()
*/
#define PFS_MAX_OS_NAME_LENGTH 16

/**
  Maximum length of the 'full' prefix of an instrument name.
  For example, for the instrument name 'wait/sync/mutex/sql/LOCK_open',
  the full prefix is 'wait/sync/mutex/sql/', which in turn derives from
  a prefix 'wait/sync/mutex' for mutexes, and a category of 'sql' for mutexes
  of the sql layer in the server.
*/
#define PFS_MAX_FULL_PREFIX_NAME_LENGTH 32

struct PFS_global_param;
struct PFS_table_share;
class PFS_opaque_container_page;

/**
  @addtogroup performance_schema_buffers
  @{
*/

extern bool pfs_enabled;

/** Global ref count for plugin and component events. */
extern std::atomic<uint32> pfs_unload_plugin_ref_count;

/** Key, naming a synch instrument (mutex, rwlock, cond). */
typedef unsigned int PFS_sync_key;
/** Key, naming a thread instrument. */
typedef unsigned int PFS_thread_key;
/** Key, naming a file instrument. */
typedef unsigned int PFS_file_key;
/** Key, naming a stage instrument. */
typedef unsigned int PFS_stage_key;
/** Key, naming a statement instrument. */
typedef unsigned int PFS_statement_key;
/** Key, naming a transaction instrument. */
typedef unsigned int PFS_transaction_key;
/** Key, naming a socket instrument. */
typedef unsigned int PFS_socket_key;
/** Key, naming a memory instrument. */
typedef unsigned int PFS_memory_key;

enum PFS_class_type {
  PFS_CLASS_NONE = 0,
  PFS_CLASS_MUTEX = 1,
  PFS_CLASS_RWLOCK = 2,
  PFS_CLASS_COND = 3,
  PFS_CLASS_FILE = 4,
  PFS_CLASS_TABLE = 5,
  PFS_CLASS_STAGE = 6,
  PFS_CLASS_STATEMENT = 7,
  PFS_CLASS_TRANSACTION = 8,
  PFS_CLASS_SOCKET = 9,
  PFS_CLASS_TABLE_IO = 10,
  PFS_CLASS_TABLE_LOCK = 11,
  PFS_CLASS_IDLE = 12,
  PFS_CLASS_MEMORY = 13,
  PFS_CLASS_METADATA = 14,
  PFS_CLASS_ERROR = 15,
  PFS_CLASS_THREAD = 16,
  PFS_CLASS_LAST = PFS_CLASS_THREAD,
  PFS_CLASS_MAX = PFS_CLASS_LAST + 1
};

/** User-defined instrument configuration. */
struct PFS_instr_config {
  /* Instrument name. */
  char *m_name;
  /* Name length. */
  uint m_name_length;
  /** Enabled flag. */
  bool m_enabled;
  /** Timed flag. */
  bool m_timed;
};

typedef Prealloced_array<PFS_instr_config *, 10> Pfs_instr_config_array;
extern Pfs_instr_config_array *pfs_instr_config_array;

struct PFS_thread;

extern uint mutex_class_start;
extern uint rwlock_class_start;
extern uint cond_class_start;
extern uint file_class_start;
extern uint socket_class_start;
extern uint wait_class_max;

/**
  Encapsulates the name of an instrumented entity.
*/
class PFS_instr_name {
 public:
  static constexpr uint max_length = PFS_MAX_INFO_NAME_LENGTH - 1;
  /*
    DO NOT ACCESS THE DATA MEMBERS DIRECTLY.  USE THE GETTERS AND
    SETTERS INSTEAD.

    The data members should really have been private, but having both
    private and public members would make the class a non-POD.  We
    need to call memset on PFS_instr_class (in init_instr_class), and
    the behavior of memset is undefined on non-POD objects.  Therefore
    we keep the data members public, with an underscore prefix, and
    this warning text.
  */
 public /*private*/:
  /** Instrument name. */
  char m_private_name[max_length + 1];
  /** Length in bytes of @c m_name. */
  uint m_private_name_length;
  /** Old instrument name, if any. */
  const char *m_private_old_name;
  /** Length in bytes of old instrument name len, if any. */
  uint m_private_old_name_length;
  /** The oldest version that uses the new name. */
  terminology_use_previous::enum_compatibility_version m_private_version;

 public:
  /** Return the name as a string. */
  const char *str() const;
  /** Return the length of the string. */
  uint length() const;
  /**
    Copy the specified name to this name.

    @param class_type The class type of this name, i.e., whether it is
    the name of a mutex, thread, etc.

    @param name The buffer to read from.

    @param max_length_arg If is given, at most that many chars are
    copied, plus the terminating '\0'. Otherwise, up to buffer_size-1
    characters are copied, plus the terminating '\0'.
  */
  void set(PFS_class_type class_type, const char *name,
           uint max_length_arg = max_length);
};

/** Information for all instrumentation. */
struct PFS_instr_class {
  /** Class type */
  PFS_class_type m_type;
  /** True if this instrument is enabled. */
  bool m_enabled;
  /** True if this instrument is timed. */
  bool m_timed;
  /** Instrument flags. */
  uint m_flags;
  /** Instrument enforced flags. */
  uint m_enforced_flags;
  /** Volatility index. */
  int m_volatility;
  /**
    Instrument name index.
    Self index in:
    - EVENTS_WAITS_SUMMARY_*_BY_EVENT_NAME for waits
    - EVENTS_STAGES_SUMMARY_*_BY_EVENT_NAME for stages
    - EVENTS_STATEMENTS_SUMMARY_*_BY_EVENT_NAME for statements
    - EVENTS_TRANSACTIONS_SUMMARY_*_BY_EVENT_NAME for transactions
  */
  uint m_event_name_index;
  /** Instrument name. */
  PFS_instr_name m_name;
  /** Documentation. */
  char *m_documentation;

  bool is_singleton() const { return m_flags & PSI_FLAG_SINGLETON; }

  bool is_mutable() const { return m_flags & PSI_FLAG_MUTABLE; }

  bool is_progress() const { return m_flags & PSI_FLAG_STAGE_PROGRESS; }

  bool is_shared_exclusive() const { return m_flags & PSI_FLAG_RWLOCK_SX; }

  bool is_priority() const { return m_flags & PSI_FLAG_RWLOCK_PR; }

  bool is_transferable() const { return m_flags & PSI_FLAG_TRANSFER; }

  bool is_user() const { return m_flags & PSI_FLAG_USER; }

  bool is_system_thread() const { return m_flags & PSI_FLAG_THREAD_SYSTEM; }

  bool is_global() const { return m_flags & PSI_FLAG_ONLY_GLOBAL_STAT; }

  bool has_seqnum() const {
    return (m_flags & (PSI_FLAG_SINGLETON | PSI_FLAG_NO_SEQNUM)) == 0;
  }

  bool has_auto_seqnum() const { return m_flags & PSI_FLAG_AUTO_SEQNUM; }

  bool has_default_memory_cnt() const { return m_flags & PSI_FLAG_MEM_COLLECT; }

  bool has_enforced_memory_cnt() const {
    return m_enforced_flags & PSI_FLAG_MEM_COLLECT;
  }

  void set_enforced_flags(uint flags) { m_enforced_flags = flags; }

  void enforce_valid_flags(uint allowed_flags) {
    /* Reserved for future use. */
    allowed_flags |= PSI_FLAG_THREAD | PSI_FLAG_TRANSFER;

    uint valid_flags = m_flags & allowed_flags;
    /*
      This fails when the instrumented code is providing
      flags that are not supported for this instrument.
      To fix it, clean up the instrumented code.
    */
    assert(valid_flags == m_flags);
    m_flags = valid_flags;
  }

  static void set_enabled(PFS_instr_class *pfs, bool enabled);
  static void set_timed(PFS_instr_class *pfs, bool timed);

  bool is_deferred() const {
    switch (m_type) {
      case PFS_CLASS_SOCKET:
        return true;
        break;
      default:
        return false;
        break;
    };
  }

  bool can_be_timed() const {
    switch (m_type) {
      case PFS_CLASS_MEMORY:
      case PFS_CLASS_ERROR:
      case PFS_CLASS_THREAD:
        return false;
      default:
        return true;
    };
  }

  bool can_be_enforced() const {
    switch (m_type) {
      case PFS_CLASS_MEMORY:
        return true;
      default:
        return false;
    };
  }
};

struct PFS_mutex;

#define PFS_MUTEX_PARTITIONS 2

/** Instrumentation metadata for a mutex. */
struct PFS_ALIGNED PFS_mutex_class : public PFS_instr_class {
  /** Mutex usage statistics. */
  PFS_mutex_stat m_mutex_stat;
  /** Singleton instance. */
  PFS_mutex *m_singleton;
};

struct PFS_rwlock;

/** Instrumentation metadata for a read write lock. */
struct PFS_ALIGNED PFS_rwlock_class : public PFS_instr_class {
  /** Rwlock usage statistics. */
  PFS_rwlock_stat m_rwlock_stat;
  /** Singleton instance. */
  PFS_rwlock *m_singleton;
};

struct PFS_cond;

/** Instrumentation metadata for a condition. */
struct PFS_ALIGNED PFS_cond_class : public PFS_instr_class {
  /**
    Condition usage statistics.
    This statistic is not exposed in user visible tables yet.
  */
  PFS_cond_stat m_cond_stat;
  /** Singleton instance. */
  PFS_cond *m_singleton;
};

/** Instrumentation metadata of a thread. */
struct PFS_ALIGNED PFS_thread_class : public PFS_instr_class {
  /** Singleton instance. */
  PFS_thread *m_singleton;
  /** Thread history instrumentation flag. */
  bool m_history{false};
  /** Thread os name. */
  char m_os_name[PFS_MAX_OS_NAME_LENGTH];
  /** Thread instance sequence number counter. */
  std::atomic<unsigned int> m_seqnum;
};

/** Key identifying a table share. */
struct PFS_table_share_key {
  /** Object type. */
  enum_object_type m_type;

  /** Table schema. */
  PFS_schema_name m_schema_name;

  /** Table name. */
  PFS_table_name m_table_name;
};

/** Table index or 'key' */
struct PFS_table_key {
  /** Index name */
  char m_name[NAME_LEN];
  /** Length in bytes of @c m_name. */
  uint m_name_length;
};

/** Index statistics of a table.*/
struct PFS_table_share_index {
  pfs_lock m_lock;
  /** The index name */
  PFS_table_key m_key;
  /** The index stat */
  PFS_table_io_stat m_stat;
  /** Owner table share. To be used later. */
  PFS_table_share *m_owner;
  /** Container page. */
  PFS_opaque_container_page *m_page;
};

/** Lock statistics of a table.*/
struct PFS_table_share_lock {
  pfs_lock m_lock;
  /** Lock stats. */
  PFS_table_lock_stat m_stat;
  /** Owner table share. To be used later. */
  PFS_table_share *m_owner;
  /** Container page. */
  PFS_opaque_container_page *m_page;
};

/** Instrumentation metadata for a table share. */
struct PFS_ALIGNED PFS_table_share {
 public:
  uint32 get_version() { return m_lock.get_version(); }

  enum_object_type get_object_type() { return m_key.m_type; }

  void aggregate_io(void);
  void aggregate_lock(void);

  void sum_io(PFS_single_stat *result, uint key_count);
  void sum_lock(PFS_single_stat *result);
  void sum(PFS_single_stat *result, uint key_count);

  inline void aggregate(void) {
    aggregate_io();
    aggregate_lock();
  }

  inline void init_refcount(void) { m_refcount.store(1); }

  inline int get_refcount(void) { return m_refcount.load(); }

  inline void inc_refcount(void) { ++m_refcount; }

  inline void dec_refcount(void) { --m_refcount; }

  void refresh_setup_object_flags(PFS_thread *thread);

  /** Internal lock. */
  pfs_lock m_lock;
  /**
    True if table instrumentation is enabled.
    This flag is computed from the content of table setup_objects.
  */
  bool m_enabled;
  /**
    True if table instrumentation is timed.
    This flag is computed from the content of table setup_objects.
  */
  bool m_timed;

  /** Search key. */
  PFS_table_share_key m_key;

  /** Number of indexes. */
  uint m_key_count;
  /** Container page. */
  PFS_opaque_container_page *m_page;

  PFS_table_share_lock *find_lock_stat() const;
  PFS_table_share_lock *find_or_create_lock_stat();
  void destroy_lock_stat();

  PFS_table_share_index *find_index_stat(uint index) const;
  PFS_table_share_index *find_or_create_index_stat(
      const TABLE_SHARE *server_share, uint index);
  void destroy_index_stats();

 private:
  /** Number of opened table handles. */
  std::atomic<int> m_refcount;
  /** Table locks statistics. */
  std::atomic<PFS_table_share_lock *> m_race_lock_stat;
  /** Table indexes stats. */
  std::atomic<PFS_table_share_index *> m_race_index_stat[MAX_INDEXES + 1];
};

/** Statistics for the IDLE instrument. */
extern PFS_single_stat global_idle_stat;
/** Statistics for dropped table I/O. */
extern PFS_table_io_stat global_table_io_stat;
/** Statistics for dropped table lock. */
extern PFS_table_lock_stat global_table_lock_stat;
/** Statistics for the METADATA instrument. */
extern PFS_single_stat global_metadata_stat;
/** Statistics for the transaction instrument. */
extern PFS_transaction_stat global_transaction_stat;
/** Statistics for the error instrument. */
extern PFS_error_stat global_error_stat;

inline uint sanitize_index_count(uint count) {
  if (likely(count <= MAX_INDEXES)) {
    return count;
  }
  return 0;
}

#define GLOBAL_TABLE_IO_EVENT_INDEX 0
#define GLOBAL_TABLE_LOCK_EVENT_INDEX 1
#define GLOBAL_IDLE_EVENT_INDEX 2
#define GLOBAL_METADATA_EVENT_INDEX 3
/** Number of global wait events. */
#define COUNT_GLOBAL_EVENT_INDEX 4

/** Transaction events are not wait events .*/
#define GLOBAL_TRANSACTION_INDEX 0

#define GLOBAL_ERROR_INDEX 0

/**
  Instrument controlling all table I/O.
  This instrument is used with table SETUP_OBJECTS.
*/
extern PFS_instr_class global_table_io_class;

/**
  Instrument controlling all table lock.
  This instrument is used with table SETUP_OBJECTS.
*/
extern PFS_instr_class global_table_lock_class;

/**
  Instrument controlling all idle waits.
*/
extern PFS_instr_class global_idle_class;

/**
  Instrument controlling all metadata locks.
*/
extern PFS_instr_class global_metadata_class;

/** Instrumentation metadata for an error. */
struct PFS_ALIGNED PFS_error_class : public PFS_instr_class {};
/**
  Instrument controlling all server errors.
*/
extern PFS_error_class global_error_class;

struct PFS_file;

/** Instrumentation metadata for a file. */
struct PFS_ALIGNED PFS_file_class : public PFS_instr_class {
  /** File usage statistics. */
  PFS_file_stat m_file_stat;
  /** Singleton instance. */
  PFS_file *m_singleton;
};

/** Instrumentation metadata for a stage. */
struct PFS_ALIGNED PFS_stage_class : public PFS_instr_class {
  /**
    Length of the @c "stage/\<component\>/" prefix.
    This is to extract @c "foo" from @c "stage/sql/foo".
  */
  uint m_prefix_length;
  /** Stage usage statistics. */
  PFS_stage_stat m_stage_stat;
};

/** Instrumentation metadata for a statement. */
struct PFS_ALIGNED PFS_statement_class : public PFS_instr_class {};

/** Instrumentation metadata for a transaction. */
struct PFS_ALIGNED PFS_transaction_class : public PFS_instr_class {};

extern PFS_transaction_class global_transaction_class;

struct PFS_socket;

/** Instrumentation metadata for a socket. */
struct PFS_ALIGNED PFS_socket_class : public PFS_instr_class {
  /** Socket usage statistics. */
  PFS_socket_stat m_socket_stat;
  /** Singleton instance. */
  PFS_socket *m_singleton;
};

/** Instrumentation metadata for a memory. */
struct PFS_ALIGNED PFS_memory_class : public PFS_instr_class {};

void init_event_name_sizing(const PFS_global_param *param);

void register_global_classes();

int init_sync_class(uint mutex_class_sizing, uint rwlock_class_sizing,
                    uint cond_class_sizing);

void cleanup_sync_class();
int init_thread_class(uint thread_class_sizing);
void cleanup_thread_class();
int init_table_share(uint table_share_sizing);
void cleanup_table_share();

int init_table_share_lock_stat(uint table_stat_sizing);
void cleanup_table_share_lock_stat();
PFS_table_share_lock *create_table_share_lock_stat();
void release_table_share_lock_stat(PFS_table_share_lock *pfs);

int init_table_share_index_stat(uint index_stat_sizing);
void cleanup_table_share_index_stat();
PFS_table_share_index *create_table_share_index_stat(const TABLE_SHARE *share,
                                                     uint index);
void release_table_share_index_stat(PFS_table_share_index *pfs);

int init_table_share_hash(const PFS_global_param *param);
void cleanup_table_share_hash();
int init_file_class(uint file_class_sizing);
void cleanup_file_class();
int init_stage_class(uint stage_class_sizing);
void cleanup_stage_class();
int init_statement_class(uint statement_class_sizing);
void cleanup_statement_class();
int init_socket_class(uint socket_class_sizing);
void cleanup_socket_class();
int init_memory_class(uint memory_class_sizing);
void cleanup_memory_class();

PFS_sync_key register_mutex_class(const char *name, uint name_length,
                                  PSI_mutex_info *info);

PFS_sync_key register_rwlock_class(const char *name, uint name_length,
                                   PSI_rwlock_info *info);

PFS_sync_key register_cond_class(const char *name, uint name_length,
                                 PSI_cond_info *info);

PFS_thread_key register_thread_class(const char *name, uint name_length,
                                     PSI_thread_info *info);

PFS_file_key register_file_class(const char *name, uint name_length,
                                 PSI_file_info *info);

PFS_stage_key register_stage_class(const char *name, uint prefix_length,
                                   uint name_length, PSI_stage_info *info);

PFS_statement_key register_statement_class(const char *name, uint name_length,
                                           PSI_statement_info *info);

PFS_socket_key register_socket_class(const char *name, uint name_length,
                                     PSI_socket_info *info);

PFS_memory_key register_memory_class(const char *name, uint name_length,
                                     PSI_memory_info *info);

PFS_mutex_class *find_mutex_class(PSI_mutex_key key);
PFS_mutex_class *sanitize_mutex_class(PFS_mutex_class *unsafe);
PFS_rwlock_class *find_rwlock_class(PSI_rwlock_key key);
PFS_rwlock_class *sanitize_rwlock_class(PFS_rwlock_class *unsafe);
PFS_cond_class *find_cond_class(PSI_cond_key key);
PFS_cond_class *sanitize_cond_class(PFS_cond_class *unsafe);
PFS_thread_class *find_thread_class(PSI_thread_key key);
PFS_thread_class *sanitize_thread_class(PFS_thread_class *unsafe);
PFS_file_class *find_file_class(PSI_file_key key);
PFS_file_class *sanitize_file_class(PFS_file_class *unsafe);
PFS_stage_class *find_stage_class(PSI_stage_key key);
PFS_stage_class *sanitize_stage_class(PFS_stage_class *unsafe);
PFS_statement_class *find_statement_class(PSI_statement_key key);
PFS_statement_class *sanitize_statement_class(PFS_statement_class *unsafe);
PFS_instr_class *find_table_class(uint index);
PFS_instr_class *sanitize_table_class(PFS_instr_class *unsafe);
PFS_socket_class *find_socket_class(PSI_socket_key key);
PFS_socket_class *sanitize_socket_class(PFS_socket_class *unsafe);
PFS_memory_class *find_memory_class(PSI_memory_key key);
PFS_memory_class *sanitize_memory_class(PFS_memory_class *unsafe);
PFS_instr_class *find_idle_class(uint index);
PFS_instr_class *sanitize_idle_class(PFS_instr_class *unsafe);
PFS_instr_class *find_metadata_class(uint index);
PFS_instr_class *sanitize_metadata_class(PFS_instr_class *unsafe);
PFS_error_class *find_error_class(uint index);
PFS_error_class *sanitize_error_class(PFS_instr_class *unsafe);
PFS_transaction_class *find_transaction_class(uint index);
PFS_transaction_class *sanitize_transaction_class(
    PFS_transaction_class *unsafe);

PFS_table_share *find_or_create_table_share(PFS_thread *thread, bool temporary,
                                            const TABLE_SHARE *share);
void release_table_share(PFS_table_share *pfs);
void drop_table_share(PFS_thread *thread, bool temporary,
                      const char *schema_name, uint schema_name_length,
                      const char *table_name, uint table_name_length);

PFS_table_share *sanitize_table_share(PFS_table_share *unsafe);

extern ulong mutex_class_max;
extern ulong mutex_class_lost;
extern ulong rwlock_class_max;
extern ulong rwlock_class_lost;
extern ulong cond_class_max;
extern ulong cond_class_lost;
extern ulong thread_class_max;
extern ulong thread_class_lost;
extern ulong file_class_max;
extern ulong file_class_lost;
extern ulong stage_class_max;
extern ulong stage_class_lost;
extern ulong statement_class_max;
extern ulong statement_class_lost;
extern ulong transaction_class_max;
extern ulong socket_class_max;
extern ulong socket_class_lost;
extern ulong memory_class_max;
extern ulong memory_class_lost;
extern ulong error_class_max;

/* Exposing the data directly, for iterators. */

extern PFS_mutex_class *mutex_class_array;
extern PFS_rwlock_class *rwlock_class_array;
extern PFS_cond_class *cond_class_array;
extern PFS_file_class *file_class_array;

void reset_events_waits_by_class();
void reset_file_class_io();
void reset_socket_class_io();

/** Update derived flags for all table shares. */
void update_table_share_derived_flags(PFS_thread *thread);

/** Update derived flags for all stored procedure shares. */
void update_program_share_derived_flags(PFS_thread *thread);

extern LF_HASH table_share_hash;

/**
  Get current time for GTID monitoring.

  @return my_getsystime() when PFS monitoring is enabled.
  @return 0 when PFS monitoring is disabled.
*/
ulonglong gtid_monitoring_getsystime();

/** @} */
#endif
