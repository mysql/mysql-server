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

#ifndef PFS_INSTR_H
#define PFS_INSTR_H

/**
  @file storage/perfschema/pfs_instr.h
  Performance schema instruments (declarations).
*/

struct PFS_mutex_class;
struct PFS_rwlock_class;
struct PFS_cond_class;
struct PFS_file_class;
struct PFS_table_share;
struct PFS_thread_class;
struct PFS_socket_class;
class PFS_opaque_container_page;

class THD;

#include "my_config.h"

#include <sys/types.h>
#include <time.h>
#include <atomic>

#include "my_inttypes.h"
#include "my_io.h"
#include "my_thread_os_id.h"
#ifdef _WIN32
#include <winsock2.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#include "lf.h"
#include "my_compiler.h"
#include "sql/mdl.h"
#include "storage/perfschema/pfs_column_types.h"
#include "storage/perfschema/pfs_con_slice.h"
#include "storage/perfschema/pfs_events_stages.h"
#include "storage/perfschema/pfs_events_statements.h"
#include "storage/perfschema/pfs_events_transactions.h"
#include "storage/perfschema/pfs_events_waits.h"
#include "storage/perfschema/pfs_instr_class.h"
#include "storage/perfschema/pfs_lock.h"
#include "storage/perfschema/pfs_server.h"
#include "storage/perfschema/pfs_stat.h"
#include "violite.h" /* enum_vio_type */

extern PFS_single_stat *thread_instr_class_waits_array_start;
extern PFS_single_stat *thread_instr_class_waits_array_end;

/**
  @addtogroup performance_schema_buffers
  @{
*/

struct PFS_thread;
struct PFS_host;
struct PFS_user;
struct PFS_account;

/** Base structure for wait instruments. */
struct PFS_instr {
  /** Internal lock. */
  pfs_lock m_lock;
  /** Enabled flag. */
  bool m_enabled;
  /** Timed flag. */
  bool m_timed;
  /** Container page. */
  PFS_opaque_container_page *m_page;
};

/** Instrumented mutex implementation. @see PSI_mutex. */
struct PFS_ALIGNED PFS_mutex : public PFS_instr {
  /** Mutex identity, typically a @c pthread_mutex_t. */
  const void *m_identity;
  /** Mutex class. */
  PFS_mutex_class *m_class;
  /** Instrument statistics. */
  PFS_mutex_stat m_mutex_stat;
  /** Current owner. */
  PFS_thread *m_owner;
  /**
    Time stamp of the last lock.
    This statistic is not exposed in user visible tables yet.
  */
  ulonglong m_last_locked;
};

/** Instrumented rwlock implementation. @see PSI_rwlock. */
struct PFS_ALIGNED PFS_rwlock : public PFS_instr {
  /** RWLock identity, typically a @c pthread_rwlock_t. */
  const void *m_identity;
  /** RWLock class. */
  PFS_rwlock_class *m_class;
  /** Instrument statistics. */
  PFS_rwlock_stat m_rwlock_stat;
  /** Current writer thread. */
  PFS_thread *m_writer;
  /** Current count of readers. */
  uint m_readers;
  /**
    Time stamp of the last write.
    This statistic is not exposed in user visible tables yet.
  */
  ulonglong m_last_written;
  /**
    Time stamp of the last read.
    This statistic is not exposed in user visible tables yet.
  */
  ulonglong m_last_read;
};

/** Instrumented condition implementation. @see PSI_cond. */
struct PFS_ALIGNED PFS_cond : public PFS_instr {
  /** Condition identity, typically a @c pthread_cond_t. */
  const void *m_identity;
  /** Condition class. */
  PFS_cond_class *m_class;
  /** Condition instance usage statistics. */
  PFS_cond_stat m_cond_stat;
};

/** Instrumented File and FILE implementation. @see PSI_file. */
struct PFS_ALIGNED PFS_file : public PFS_instr {
  uint32 get_version() { return m_lock.get_version(); }

  /** File identity */
  const void *m_identity;
  /** File name. */
  char m_filename[FN_REFLEN];
  /** File name length in bytes. */
  uint m_filename_length;
  /** File class. */
  PFS_file_class *m_class;
  /** File usage statistics. */
  PFS_file_stat m_file_stat;
  /** True if a temporary file. */
  bool m_temporary;
};

/** Instrumented table implementation. @see PSI_table. */
struct PFS_ALIGNED PFS_table {
  /**
    True if table I/O instrumentation is enabled.
    This flag is computed.
  */
  bool m_io_enabled;
  /**
    True if table lock instrumentation is enabled.
    This flag is computed.
  */
  bool m_lock_enabled;
  /**
    True if table I/O instrumentation is timed.
    This flag is computed.
  */
  bool m_io_timed;
  /**
    True if table lock instrumentation is timed.
    This flag is computed.
  */
  bool m_lock_timed;

  /** True if table I/O statistics have been collected. */
  bool m_has_io_stats;

  /** True if table lock statistics have been collected. */
  bool m_has_lock_stats;

 public:
  /**
    Aggregate this table handle statistics to the parents.
    Only use this method for handles owned by the calling code.
    @sa sanitized_aggregate.
  */
  void aggregate(const TABLE_SHARE *server_share) {
    if (m_has_io_stats) {
      safe_aggregate_io(server_share, &m_table_stat, m_share);
      m_has_io_stats = false;
    }
    if (m_has_lock_stats) {
      safe_aggregate_lock(&m_table_stat, m_share);
      m_has_lock_stats = false;
    }
  }

  /**
    Aggregate this table handle statistics to the parents.
    This method is safe to call on handles not owned by the calling code.
    @sa aggregate
    @sa sanitized_aggregate_io
    @sa sanitized_aggregate_lock
  */
  void sanitized_aggregate(void);

  /**
    Aggregate this table handle I/O statistics to the parents.
    This method is safe to call on handles not owned by the calling code.
  */
  void sanitized_aggregate_io(void);

  /**
    Aggregate this table handle lock statistics to the parents.
    This method is safe to call on handles not owned by the calling code.
  */
  void sanitized_aggregate_lock(void);

  /** Internal lock. */
  pfs_lock m_lock;
  /** Thread Owner. */
  PFS_thread *m_thread_owner;
  /** Event Owner. */
  ulonglong m_owner_event_id;
  /** Table share. */
  PFS_table_share *m_share;
  /** Table identity, typically a handler. */
  const void *m_identity;
  /** Table statistics. */
  PFS_table_stat m_table_stat;
  /** Current internal lock. */
  PFS_TL_LOCK_TYPE m_internal_lock;
  /** Current external lock. */
  PFS_TL_LOCK_TYPE m_external_lock;
  /** Container page. */
  PFS_opaque_container_page *m_page;

 private:
  static void safe_aggregate_io(const TABLE_SHARE *optional_server_share,
                                PFS_table_stat *stat,
                                PFS_table_share *safe_share);
  static void safe_aggregate_lock(PFS_table_stat *stat,
                                  PFS_table_share *safe_share);
};

/** Instrumented socket implementation. @see PSI_socket. */
struct PFS_ALIGNED PFS_socket : public PFS_instr {
  uint32 get_version() { return m_lock.get_version(); }

  /** Socket identity, typically int */
  const void *m_identity;
  /** Owning thread, if applicable */
  PFS_thread *m_thread_owner;
  /** Socket file descriptor */
  uint m_fd;
  /** Raw socket address */
  struct sockaddr_storage m_sock_addr;
  /** Length of address */
  socklen_t m_addr_len;
  /** Idle flag. */
  bool m_idle;
  /** Socket class. */
  PFS_socket_class *m_class;
  /** Socket usage statistics. */
  PFS_socket_stat m_socket_stat;
};

/** Instrumented metadata lock implementation. @see PSI_metadata_lock. */
struct PFS_ALIGNED PFS_metadata_lock : public PFS_instr {
  uint32 get_version() { return m_lock.get_version(); }

  /** Lock identity. */
  const void *m_identity;
  MDL_key m_mdl_key;
  opaque_mdl_type m_mdl_type;
  opaque_mdl_duration m_mdl_duration;
  opaque_mdl_status m_mdl_status;
  const char *m_src_file;
  uint m_src_line;
  ulonglong m_owner_thread_id;
  ulonglong m_owner_event_id;
};

/**
  @def WAIT_STACK_LOGICAL_SIZE
  Maximum number of nested waits.
  Some waits, such as:
  - "wait/io/table/sql/handler"
  - "wait/lock/table/sql/handler"
  are implemented by calling code in a storage engine,
  that can cause nested waits (file I/O, mutex, ...)
  Because of partitioned tables, a table I/O event (on the whole table)
  can contain a nested table I/O event (on a partition).
  Because of additional debug instrumentation,
  waiting on what looks like a "mutex" (safe_mutex, innodb sync0sync, ...)
  can cause nested waits to be recorded.
  For example, a wait on innodb mutexes can lead to:
  - wait/sync/mutex/innobase/some_mutex
    - wait/sync/mutex/innobase/sync0sync
      - wait/sync/mutex/innobase/os0sync
  The max depth of the event stack must be sufficient
  for these low level details to be visible.
*/
#define WAIT_STACK_LOGICAL_SIZE 5
/**
  @def WAIT_STACK_BOTTOM
  Maximum number dummy waits records.
  One dummy record is reserved for the parent stage / statement / transaction,
  at the bottom of the wait stack.
*/
#define WAIT_STACK_BOTTOM 1
/**
  @def WAIT_STACK_SIZE
  Physical size of the waits stack
*/
#define WAIT_STACK_SIZE (WAIT_STACK_BOTTOM + WAIT_STACK_LOGICAL_SIZE)

/** Max size of the statements stack. */
extern uint statement_stack_max;
/** Max size of the digests token array. */
extern size_t pfs_max_digest_length;
/** Max size of SQL TEXT. */
extern size_t pfs_max_sqltext;

/** Instrumented thread implementation. @see PSI_thread. */
struct PFS_ALIGNED PFS_thread : PFS_connection_slice {
  static PFS_thread *get_current_thread(void);

  /** Thread instrumentation flag. */
  bool m_enabled;
  /** Thread history instrumentation flag. */
  bool m_history;

  /**
    Derived flag flag_events_waits_history, per thread.
    Cached computation of
      TABLE SETUP_CONSUMERS[EVENTS_WAITS_HISTORY].ENABLED == 'YES'
    AND
      TABLE THREADS[THREAD_ID].HISTORY == 'YES'
  */
  bool m_flag_events_waits_history;
  /**
    Derived flag flag_events_waits_history_long, per thread.
    Cached computation of
      TABLE SETUP_CONSUMERS[EVENTS_WAITS_HISTORY_LONG].ENABLED == 'YES'
    AND
      TABLE THREADS[THREAD_ID].HISTORY == 'YES'
  */
  bool m_flag_events_waits_history_long;
  /**
    Derived flag flag_events_stages_history, per thread.
    Cached computation of
      TABLE SETUP_CONSUMERS[EVENTS_STAGES_HISTORY].ENABLED == 'YES'
    AND
      TABLE THREADS[THREAD_ID].HISTORY == 'YES'
  */
  bool m_flag_events_stages_history;
  /**
    Derived flag flag_events_stages_history_long, per thread.
    Cached computation of
      TABLE SETUP_CONSUMERS[EVENTS_STAGES_HISTORY_LONG].ENABLED == 'YES'
    AND
      TABLE THREADS[THREAD_ID].HISTORY == 'YES'
  */
  bool m_flag_events_stages_history_long;
  /**
    Derived flag flag_events_statements_history, per thread.
    Cached computation of
      TABLE SETUP_CONSUMERS[EVENTS_STATEMENTS_HISTORY].ENABLED == 'YES'
    AND
      TABLE THREADS[THREAD_ID].HISTORY == 'YES'
  */
  bool m_flag_events_statements_history;
  /**
    Derived flag flag_events_statements_history_long, per thread.
    Cached computation of
      TABLE SETUP_CONSUMERS[EVENTS_STATEMENTS_HISTORY_LONG].ENABLED == 'YES'
    AND
      TABLE THREADS[THREAD_ID].HISTORY == 'YES'
  */
  bool m_flag_events_statements_history_long;
  /**
    Derived flag flag_events_transactions_history, per thread.
    Cached computation of
      TABLE SETUP_CONSUMERS[EVENTS_TRANSACTIONS_HISTORY].ENABLED == 'YES'
    AND
      TABLE THREADS[THREAD_ID].HISTORY == 'YES'
  */
  bool m_flag_events_transactions_history;
  /**
    Derived flag flag_events_transactions_history_long, per thread.
    Cached computation of
      TABLE SETUP_CONSUMERS[EVENTS_TRANSACTIONS_HISTORY_LONG].ENABLED == 'YES'
    AND
      TABLE THREADS[THREAD_ID].HISTORY == 'YES'
  */
  bool m_flag_events_transactions_history_long;

  /** Current wait event in the event stack. */
  PFS_events_waits *m_events_waits_current;
  /** Event ID counter */
  ulonglong m_event_id;
  /**
    Internal lock.
    This lock is exclusively used to protect against races
    when creating and destroying PFS_thread.
    Do not use this lock to protect thread attributes,
    use one of @c m_stmt_lock or @c m_session_lock instead.
  */
  pfs_lock m_lock;
  /** Pins for filename_hash. */
  LF_PINS *m_filename_hash_pins;
  /** Pins for table_share_hash. */
  LF_PINS *m_table_share_hash_pins;
  /** Pins for setup_actor_hash. */
  LF_PINS *m_setup_actor_hash_pins;
  /** Pins for setup_object_hash. */
  LF_PINS *m_setup_object_hash_pins;
  /** Pins for host_hash. */
  LF_PINS *m_host_hash_pins;
  /** Pins for user_hash. */
  LF_PINS *m_user_hash_pins;
  /** Pins for account_hash. */
  LF_PINS *m_account_hash_pins;
  /** Pins for digest_hash. */
  LF_PINS *m_digest_hash_pins;
  /** Pins for routine_hash. */
  LF_PINS *m_program_hash_pins;
  /** Internal thread identifier, unique. */
  ulonglong m_thread_internal_id;
  /** Parent internal thread identifier. */
  ulonglong m_parent_thread_internal_id;
  /** External (@code SHOW PROCESSLIST @endcode) thread identifier, not unique.
   */
  ulong m_processlist_id;
  /** External (Operating system) thread identifier, if any. */
  my_thread_os_id_t m_thread_os_id;
  /** Thread class. */
  PFS_thread_class *m_class;
  /** True if a system thread. */
  bool m_system_thread;
  /**
    Stack of events waits.
    This member holds the data for the table
    PERFORMANCE_SCHEMA.EVENTS_WAITS_CURRENT.
    Note that stack[0] is a dummy record that represents the parent
    stage/statement/transaction.
    For example, assuming the following tree:
    - STAGE ID 100
      - WAIT ID 101, parent STAGE 100
        - WAIT ID 102, parent wait 101
    the data in the stack will be:
    stack[0].m_event_id= 100, set by the stage instrumentation
    stack[0].m_event_type= STAGE, set by the stage instrumentation
    stack[0].m_nesting_event_id= unused
    stack[0].m_nesting_event_type= unused
    stack[1].m_event_id= 101
    stack[1].m_event_type= WAIT
    stack[1].m_nesting_event_id= stack[0].m_event_id= 100
    stack[1].m_nesting_event_type= stack[0].m_event_type= STAGE
    stack[2].m_event_id= 102
    stack[2].m_event_type= WAIT
    stack[2].m_nesting_event_id= stack[1].m_event_id= 101
    stack[2].m_nesting_event_type= stack[1].m_event_type= WAIT

    The whole point of the stack[0] record is to allow this optimization
    in the code, in the instrumentation for wait events:
      wait->m_nesting_event_id= (wait-1)->m_event_id;
      wait->m_nesting_event_type= (wait-1)->m_event_type;
    This code works for both the top level wait, and nested waits,
    and works without if conditions, which helps performances.
  */
  PFS_events_waits m_events_waits_stack[WAIT_STACK_SIZE];
  /** True if the circular buffer @c m_waits_history is full. */
  bool m_waits_history_full;
  /** Current index in the circular buffer @c m_waits_history. */
  uint m_waits_history_index;
  /**
    Waits history circular buffer.
    This member holds the data for the table
    PERFORMANCE_SCHEMA.EVENTS_WAITS_HISTORY.
  */
  PFS_events_waits *m_waits_history;

  /** True if the circular buffer @c m_stages_history is full. */
  bool m_stages_history_full;
  /** Current index in the circular buffer @c m_stages_history. */
  uint m_stages_history_index;
  /**
    Stages history circular buffer.
    This member holds the data for the table
    PERFORMANCE_SCHEMA.EVENTS_STAGES_HISTORY.
  */
  PFS_events_stages *m_stages_history;

  /** True if the circular buffer @c m_statements_history is full. */
  bool m_statements_history_full;
  /** Current index in the circular buffer @c m_statements_history. */
  uint m_statements_history_index;
  /**
    Statements history circular buffer.
    This member holds the data for the table
    PERFORMANCE_SCHEMA.EVENTS_STATEMENTS_HISTORY.
  */
  PFS_events_statements *m_statements_history;

  /** True if the circular buffer @c m_transactions_history is full. */
  bool m_transactions_history_full;
  /** Current index in the circular buffer @c m_transactions_history. */
  uint m_transactions_history_index;
  /**
    Statements history circular buffer.
    This member holds the data for the table
    PERFORMANCE_SCHEMA.EVENTS_TRANSACTIONS_HISTORY.
  */
  PFS_events_transactions *m_transactions_history;

  /**
    Internal lock, for session attributes.
    Statement attributes are expected to be updated in frequently,
    typically per session execution.
  */
  pfs_lock m_session_lock;

  /**
    User name.
    Protected by @c m_session_lock.
  */
  char m_username[USERNAME_LENGTH];
  /**
    Length of @c m_username.
    Protected by @c m_session_lock.
  */
  uint m_username_length;
  /**
    Host name.
    Protected by @c m_session_lock.
  */
  char m_hostname[HOSTNAME_LENGTH];
  /**
    Length of @c m_hostname.
    Protected by @c m_session_lock.
  */
  uint m_hostname_length;
  /**
    Database name.
    Protected by @c m_stmt_lock.
  */
  char m_dbname[NAME_LEN];
  /**
    Length of @c m_dbname.
    Protected by @c m_stmt_lock.
  */
  uint m_dbname_length;
  /**
    Resource group name.
    Protected by @c m_session_lock.
  */
  char m_groupname[NAME_LEN];
  /**
    Length of @c m_groupname.
    Protected by @c m_session_lock.
  */
  uint m_groupname_length;
  /** User-defined data. */
  void *m_user_data;
  /** Current command. */
  int m_command;
  /** Connection type. */
  enum_vio_type m_connection_type;
  /** Start time. */
  time_t m_start_time;
  /**
    Internal lock, for statement attributes.
    Statement attributes are expected to be updated frequently,
    typically per statement execution.
  */
  pfs_lock m_stmt_lock;
  /** Processlist state (derived from stage). */
  PFS_stage_key m_stage;
  /** Current stage progress. */
  PSI_stage_progress *m_stage_progress;
  /**
    Processlist info.
    Protected by @c m_stmt_lock.
  */
  char m_processlist_info[COL_INFO_SIZE];
  /**
    Length of @c m_processlist_info_length.
    Protected by @c m_stmt_lock.
  */
  uint m_processlist_info_length;

  PFS_events_stages m_stage_current;

  /** Size of @c m_events_statements_stack. */
  uint m_events_statements_count;
  PFS_events_statements *m_statement_stack;

  PFS_events_transactions m_transaction_current;

  THD *m_thd;
  PFS_host *m_host;
  PFS_user *m_user;
  PFS_account *m_account;

  /** Raw socket address */
  struct sockaddr_storage m_sock_addr;
  /** Length of address */
  socklen_t m_sock_addr_len;

  /** Reset session connect attributes */
  void reset_session_connect_attrs();

  /**
    Buffer for the connection attributes.
    Protected by @c m_session_lock.
  */
  char *m_session_connect_attrs;
  /**
    Length used by @c m_connect_attrs.
    Protected by @c m_session_lock.
  */
  uint m_session_connect_attrs_length;
  /**
    Character set in which @c m_connect_attrs are encoded.
    Protected by @c m_session_lock.
  */
  uint m_session_connect_attrs_cs_number;

  /** Reset all memory statistics. */
  void rebase_memory_stats();

  void carry_memory_stat_delta(PFS_memory_stat_delta *delta, uint index);

  void set_enabled(bool enabled) { m_enabled = enabled; }

  void set_history(bool history) {
    m_history = history;
    set_history_derived_flags();
  }

  void set_history_derived_flags();

  /**
    Per thread memory aggregated statistics.
    This member holds the data for the table
    PERFORMANCE_SCHEMA.MEMORY_SUMMARY_BY_THREAD_BY_EVENT_NAME.
    Immutable, safe to use without internal lock.
  */
  PFS_memory_safe_stat *m_instr_class_memory_stats;

  void set_instr_class_memory_stats(PFS_memory_safe_stat *array) {
    m_has_memory_stats = false;
    m_instr_class_memory_stats = array;
  }

  const PFS_memory_safe_stat *read_instr_class_memory_stats() const {
    if (!m_has_memory_stats) {
      return NULL;
    }
    return m_instr_class_memory_stats;
  }

  PFS_memory_safe_stat *write_instr_class_memory_stats() {
    if (!m_has_memory_stats) {
      rebase_memory_stats();
      m_has_memory_stats = true;
    }
    return m_instr_class_memory_stats;
  }
};

void carry_global_memory_stat_delta(PFS_memory_stat_delta *delta, uint index);

extern PFS_stage_stat *global_instr_class_stages_array;
extern PFS_statement_stat *global_instr_class_statements_array;
extern PFS_histogram global_statements_histogram;
extern std::atomic<PFS_memory_shared_stat *> global_instr_class_memory_array;

PFS_mutex *sanitize_mutex(PFS_mutex *unsafe);
PFS_rwlock *sanitize_rwlock(PFS_rwlock *unsafe);
PFS_cond *sanitize_cond(PFS_cond *unsafe);
PFS_thread *sanitize_thread(PFS_thread *unsafe);
PFS_file *sanitize_file(PFS_file *unsafe);
PFS_socket *sanitize_socket(PFS_socket *unsafe);
PFS_metadata_lock *sanitize_metadata_lock(PFS_metadata_lock *unsafe);

int init_instruments(const PFS_global_param *param);
void cleanup_instruments();
int init_file_hash(const PFS_global_param *param);
void cleanup_file_hash();
PFS_mutex *create_mutex(PFS_mutex_class *mutex_class, const void *identity);
void destroy_mutex(PFS_mutex *pfs);
PFS_rwlock *create_rwlock(PFS_rwlock_class *klass, const void *identity);
void destroy_rwlock(PFS_rwlock *pfs);
PFS_cond *create_cond(PFS_cond_class *klass, const void *identity);
void destroy_cond(PFS_cond *pfs);

PFS_thread *create_thread(PFS_thread_class *klass, const void *identity,
                          ulonglong processlist_id);

PFS_thread *find_thread_by_processlist_id(ulonglong processlist_id);
PFS_thread *find_thread_by_internal_id(ulonglong thread_id);

void destroy_thread(PFS_thread *pfs);

PFS_file *find_or_create_file(PFS_thread *thread, PFS_file_class *klass,
                              const char *filename, uint len, bool create);

void find_and_rename_file(PFS_thread *thread, const char *old_filename,
                          uint old_len, const char *new_filename, uint new_len);

void release_file(PFS_file *pfs);
void destroy_file(PFS_thread *thread, PFS_file *pfs);
PFS_table *create_table(PFS_table_share *share, PFS_thread *opening_thread,
                        const void *identity);
void destroy_table(PFS_table *pfs);

PFS_socket *create_socket(PFS_socket_class *socket_class, const my_socket *fd,
                          const struct sockaddr *addr, socklen_t addr_len);
void destroy_socket(PFS_socket *pfs);

PFS_metadata_lock *create_metadata_lock(void *identity, const MDL_key *mdl_key,
                                        opaque_mdl_type mdl_type,
                                        opaque_mdl_duration mdl_duration,
                                        opaque_mdl_status mdl_status,
                                        const char *src_file, uint src_line);
void destroy_metadata_lock(PFS_metadata_lock *pfs);

/* For iterators and show status. */

extern long file_handle_max;
extern ulong file_handle_lost;
extern ulong events_waits_history_per_thread;
extern ulong events_stages_history_per_thread;
extern ulong events_statements_history_per_thread;
extern ulong events_transactions_history_per_thread;
extern ulong locker_lost;
extern ulong statement_lost;
extern ulong session_connect_attrs_lost;
extern ulong session_connect_attrs_longest_seen;
extern ulong session_connect_attrs_size_per_thread;

/* Exposing the data directly, for iterators. */

extern PFS_file **file_handle_array;

void reset_events_waits_by_instance();
void reset_file_instance_io();
void reset_socket_instance_io();
void reset_histogram_global();

void aggregate_all_event_names(PFS_single_stat *from_array,
                               PFS_single_stat *to_array);
void aggregate_all_event_names(PFS_single_stat *from_array,
                               PFS_single_stat *to_array_1,
                               PFS_single_stat *to_array_2);

void aggregate_all_stages(PFS_stage_stat *from_array, PFS_stage_stat *to_array);
void aggregate_all_stages(PFS_stage_stat *from_array,
                          PFS_stage_stat *to_array_1,
                          PFS_stage_stat *to_array_2);

void aggregate_all_statements(PFS_statement_stat *from_array,
                              PFS_statement_stat *to_array);
void aggregate_all_statements(PFS_statement_stat *from_array,
                              PFS_statement_stat *to_array_1,
                              PFS_statement_stat *to_array_2);

void aggregate_all_transactions(PFS_transaction_stat *from_array,
                                PFS_transaction_stat *to_array);
void aggregate_all_transactions(PFS_transaction_stat *from_array,
                                PFS_transaction_stat *to_array_1,
                                PFS_transaction_stat *to_array_2);

void aggregate_all_errors(PFS_error_stat *from_array, PFS_error_stat *to_array);
void aggregate_all_errors(PFS_error_stat *from_array,
                          PFS_error_stat *to_array_1,
                          PFS_error_stat *to_array_2);

void aggregate_all_memory(bool alive, PFS_memory_safe_stat *from_array,
                          PFS_memory_shared_stat *to_array);
void aggregate_all_memory(bool alive, PFS_memory_shared_stat *from_array,
                          PFS_memory_shared_stat *to_array);
void aggregate_all_memory(bool alive, PFS_memory_safe_stat *from_array,
                          PFS_memory_shared_stat *to_array_1,
                          PFS_memory_shared_stat *to_array_2);
void aggregate_all_memory(bool alive, PFS_memory_shared_stat *from_array,
                          PFS_memory_shared_stat *to_array_1,
                          PFS_memory_shared_stat *to_array_2);

void aggregate_thread(PFS_thread *thread, PFS_account *safe_account,
                      PFS_user *safe_user, PFS_host *safe_host);
void aggregate_thread_waits(PFS_thread *thread, PFS_account *safe_account,
                            PFS_user *safe_user, PFS_host *safe_host);
void aggregate_thread_stages(PFS_thread *thread, PFS_account *safe_account,
                             PFS_user *safe_user, PFS_host *safe_host);
void aggregate_thread_statements(PFS_thread *thread, PFS_account *safe_account,
                                 PFS_user *safe_user, PFS_host *safe_host);
void aggregate_thread_transactions(PFS_thread *thread,
                                   PFS_account *safe_account,
                                   PFS_user *safe_user, PFS_host *safe_host);
void aggregate_thread_errors(PFS_thread *thread, PFS_account *safe_account,
                             PFS_user *safe_user, PFS_host *safe_host);
void aggregate_thread_memory(bool alive, PFS_thread *thread,
                             PFS_account *safe_account, PFS_user *safe_user,
                             PFS_host *safe_host);

void aggregate_thread_status(PFS_thread *thread, PFS_account *safe_account,
                             PFS_user *safe_user, PFS_host *safe_host);

void clear_thread_account(PFS_thread *thread);
void set_thread_account(PFS_thread *thread);

/** Update derived flags for all mutex instances. */
void update_mutex_derived_flags();
/** Update derived flags for all rwlock instances. */
void update_rwlock_derived_flags();
/** Update derived flags for all condition instances. */
void update_cond_derived_flags();
/** Update derived flags for all file handles. */
void update_file_derived_flags();
/** Update derived flags for all table handles. */
void update_table_derived_flags();
/** Update derived flags for all socket instances. */
void update_socket_derived_flags();
/** Update derived flags for all metadata instances. */
void update_metadata_derived_flags();
/** Update derived flags for all thread instances. */
void update_thread_derived_flags();
/** Update derived flags for all instruments. */
void update_instruments_derived_flags();

/** Clear source file pointers for all statements, stages, waits and
 * transactions. */
void reset_source_file_pointers();

extern LF_HASH filename_hash;

/** @} */
#endif
