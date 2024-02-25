/* Copyright (c) 2008, 2023, Oracle and/or its affiliates.

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

#ifndef RPL_HANDLER_H
#define RPL_HANDLER_H

#include <sys/types.h>
#include <atomic>
#include <map>

#include "my_alloc.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_psi_config.h"
#include "my_sys.h"  // free_root
#include "mysql/components/services/bits/mysql_rwlock_bits.h"
#include "mysql/components/services/bits/psi_rwlock_bits.h"
#include "mysql/psi/mysql_rwlock.h"
#include "sql/locks/shared_spin_lock.h"  // Shared_spin_lock
#include "sql/psi_memory_key.h"
#include "sql/sql_list.h"        // List
#include "sql/sql_plugin.h"      // my_plugin_(un)lock
#include "sql/sql_plugin_ref.h"  // plugin_ref

class Master_info;
class String;
class THD;
struct Binlog_relay_IO_observer;
struct Binlog_relay_IO_param;
struct Binlog_storage_observer;
struct Binlog_transmit_observer;
struct Server_state_observer;
struct Trans_observer;
class Table_ref;

/**
  Variable to keep the value set for the
  `replication_optimize_for_static_plugin_config` global.

  When this global variable value is set to `1`, we prevent all plugins that
  register replication observers to be unloaded until the variable is set to
  `0`, again. While the value of the variable is `1`, we are also exchanging the
  `Delegate` class read-write lock by an atomic-based shared spin-lock.

  This behaviour is useful for increasing the throughtput of the master when a
  large number of slaves is connected, by preventing the acquisition of the
  `LOCK_plugin` mutex and using a more read-friendly lock in the `Delegate`
  class, when invoking the observer's hooks.

  Note that a large number of slaves means a large number of dump threads, which
  means a large number of threads calling the registered observers hooks.

  If `UNINSTALL` is executed on a replication observer plugin while the variable
  is set to `1`, the unload of the plugin will be deferred until the variable's
  value is set to `0`.
*/
extern bool opt_replication_optimize_for_static_plugin_config;
/**
  Variable to keep the value set for the
  `replication_sender_observe_commit_only` global.

  When this global variable is set to `1`, only the replication observer's
  commit hook will be called, every other registered hook invocation is skipped.
*/
extern std::atomic<bool> opt_replication_sender_observe_commit_only;

class Observer_info {
 public:
  void *observer;
  st_plugin_int *plugin_int;
  plugin_ref plugin;

  Observer_info(void *ob, st_plugin_int *p);
};

/**
  Base class for adding replication event observer infra-structure. It's
  meant to be sub-classed by classes that will provide the support for the
  specific event types. This class is meant just to provide basic support
  for managing observers and managing resource access and lock acquisition.
 */
class Delegate {
 public:
  typedef List<Observer_info> Observer_info_list;
  typedef List_iterator<Observer_info> Observer_info_iterator;

  /**
    Class constructor

    @param key the PFS key for instrumenting the class lock
   */
  explicit Delegate(
#ifdef HAVE_PSI_RWLOCK_INTERFACE
      PSI_rwlock_key key
#endif
  );
  /**
    Class destructor
   */
  virtual ~Delegate();

  /**
    Adds an observer to the observer list.

    @param observer The observer object to be added to the list
    @param plugin The plugin the observer is being loaded from

    @return 0 upon success, 1 otherwise
   */
  int add_observer(void *observer, st_plugin_int *plugin);
  /**
    Removes an observer from the observer list.

    @param observer The observer object to be added to the list

    @return 0 upon success, 1 otherwise
   */
  int remove_observer(void *observer);
  /**
    Retrieves an iterator for the observer list.

    @return the iterator for the observer list
   */
  Observer_info_iterator observer_info_iter();
  /**
    Returns whether or not there are registered observers.

    @return whether or not there are registered observers
   */
  bool is_empty();
  /**
    Acquires this Delegate class instance lock in read/shared mode.

    @return 0 upon success, 1 otherwise
   */
  int read_lock();
  /**
    Acquires this Delegate class instance lock in write/exclusive mode.

    @return 0 upon success, 1 otherwise
   */
  int write_lock();
  /**
    Releases this Delegate class instance lock.

    @return 0 upon success, 1 otherwise
   */
  int unlock();
  /**
    Returns whether or not this instance was initialized.

    @return whether or not this instance was initialized
   */
  bool is_inited();
  /**
    Toggles the type of lock between a classical read-write lock and a
    shared-exclusive spin-lock.
   */
  void update_lock_type();
  /**
    Increases the `info->plugin` usage reference counting if
    `replication_optimize_for_static_plugin_config` is being enabled, in order
    to prevent plugin removal.

    Decreases the `info->plugin` usage reference counting if
    `replication_optimize_for_static_plugin_config` is being disabled, in order
    to allow plugin removal.
   */
  void update_plugin_ref_count();
  /**
    Returns whether or not to use the classic read-write lock.

    The read-write lock should be used if that type of lock is already
    acquired by some thread or if the server is not optimized for static
    plugin configuration.

    @returns true if one should use the classic read-write lock, false otherwise
   */
  bool use_rw_lock_type();
  /**
    Returns whether or not to use the shared spin-lock.

    The shared spin-lock should be used if that type of lock is already
    acquired by some thread or if the server is optimized for static plugin
    configuration.

    @returns true if one should use the shared spin-lock, false otherwise
   */
  bool use_spin_lock_type();

 private:
  /** List of registered observers */
  Observer_info_list observer_info_list;
  /**
    A read/write lock to be used when not optimizing for static plugin config
   */
  mysql_rwlock_t lock;
  /**
    A shared-exclusive spin lock to be used when optimizing for static plugin
    config.
  */
  lock::Shared_spin_lock m_spin_lock;
  /** Memory pool to be used to allocate the observers list */
  MEM_ROOT memroot{key_memory_delegate, 1024};
  /** Flag statign whether or not this instance was initialized */
  bool inited;
  /**
    The type of lock configured to be used, either a classic read-write (-1)
    lock or a shared-exclusive spin lock (1).
   */
  std::atomic<int> m_configured_lock_type;
  /**
    The count of locks acquired: -1 will be added for each classic
    read-write lock acquisitions; +1 will be added for each
    shared-exclusive spin lock acquisition.
   */
  std::atomic<int> m_acquired_locks;
  /**
    List of acquired plugin references, to be held while
    `replication_optimize_for_static_plugin_config` option is enabled. If the
    option is disabled, the references in this list will be released.
  */
  std::map<plugin_ref, size_t> m_acquired_references;

  enum enum_delegate_lock_type {
    DELEGATE_OS_LOCK = -1,   // Lock used by this class is an OS RW lock
    DELEGATE_SPIN_LOCK = 1,  // Lock used by this class is a spin lock
  };

  enum enum_delegate_lock_mode {
    DELEGATE_LOCK_MODE_SHARED = 0,     // Lock acquired in shared/read mode
    DELEGATE_LOCK_MODE_EXCLUSIVE = 1,  // Lock acquired in exclusive/write mode
  };

  /**
    Increases the `info->plugin` reference counting and stores that reference
    internally.
   */
  void acquire_plugin_ref_count(Observer_info *info);
  /**
    Locks the active lock (OS read-write lock or shared spin-lock)
    according to the mode passed on as a parameter.

    @param mode The mode to lock in, either DELEGATE_LOCK_MODE_SHARED or
                DELEGATE_LOCK_MODE_EXCLUSIVE.
   */
  void lock_it(enum_delegate_lock_mode mode);
};

#ifdef HAVE_PSI_RWLOCK_INTERFACE
extern PSI_rwlock_key key_rwlock_Trans_delegate_lock;
#endif

class Binlog_cache_storage;

class Trans_delegate : public Delegate {
  std::atomic<bool> m_rollback_transaction_on_begin{false};
  std::atomic<bool> m_rollback_transaction_not_reached_before_commit{false};

 public:
  Trans_delegate()
      : Delegate(
#ifdef HAVE_PSI_RWLOCK_INTERFACE
            key_rwlock_Trans_delegate_lock
#endif
        ) {
  }

  typedef Trans_observer Observer;

  int before_dml(THD *thd, int &result);
  int before_commit(THD *thd, bool all, Binlog_cache_storage *trx_cache_log,
                    Binlog_cache_storage *stmt_cache_log,
                    ulonglong cache_log_max_size, bool is_atomic_ddl);
  int before_rollback(THD *thd, bool all);
  int after_commit(THD *thd, bool all);
  int after_rollback(THD *thd, bool all);
  int trans_begin(THD *thd, int &result);

  /**
    The method sets the flag that will fail the new incoming transactions and
    allows some management queries to run. New incoming transactions are rolled
    back.
  */
  int set_transactions_at_begin_must_fail();

  /**
    The method that removes the restrictions on the transactions which were
    earlier failing due to flag set by the set_transactions_at_begin_must_fail
    method.
  */
  int set_no_restrictions_at_transaction_begin();

  /**
    Method to rollback the transactions that passed the begin state but have yet
    not reached the begin_commit stage. Transactions are not allowed to
    broadcast instead failure is returned from before_commit function.
  */
  int set_transactions_not_reached_before_commit_must_fail();

  /**
    Method that allows the transactions to commit again which were earlier
    stopped by set_transactions_not_reached_before_commit_must_fail method.
  */
  int set_no_restrictions_at_transactions_before_commit();
};

#ifdef HAVE_PSI_RWLOCK_INTERFACE
extern PSI_rwlock_key key_rwlock_Server_state_delegate_lock;
#endif

class Server_state_delegate : public Delegate {
 public:
  Server_state_delegate()
      : Delegate(
#ifdef HAVE_PSI_RWLOCK_INTERFACE
            key_rwlock_Server_state_delegate_lock
#endif
        ) {
  }

  typedef Server_state_observer Observer;
  int before_handle_connection(THD *thd);
  int before_recovery(THD *thd);
  int after_engine_recovery(THD *thd);
  int after_recovery(THD *thd);
  int before_server_shutdown(THD *thd);
  int after_server_shutdown(THD *thd);
  int after_dd_upgrade_from_57(THD *thd);
};

#ifdef HAVE_PSI_RWLOCK_INTERFACE
extern PSI_rwlock_key key_rwlock_Binlog_storage_delegate_lock;
#endif

class Binlog_storage_delegate : public Delegate {
 public:
  Binlog_storage_delegate()
      : Delegate(
#ifdef HAVE_PSI_RWLOCK_INTERFACE
            key_rwlock_Binlog_storage_delegate_lock
#endif
        ) {
  }

  typedef Binlog_storage_observer Observer;
  int after_flush(THD *thd, const char *log_file, my_off_t log_pos);
  int after_sync(THD *thd, const char *log_file, my_off_t log_pos);
};

#ifdef HAVE_PSI_RWLOCK_INTERFACE
extern PSI_rwlock_key key_rwlock_Binlog_transmit_delegate_lock;
#endif

class Binlog_transmit_delegate : public Delegate {
 public:
  Binlog_transmit_delegate()
      : Delegate(
#ifdef HAVE_PSI_RWLOCK_INTERFACE
            key_rwlock_Binlog_transmit_delegate_lock
#endif
        ) {
  }

  typedef Binlog_transmit_observer Observer;
  int transmit_start(THD *thd, ushort flags, const char *log_file,
                     my_off_t log_pos, bool *observe_transmission);
  int transmit_stop(THD *thd, ushort flags);
  int reserve_header(THD *thd, ushort flags, String *packet);
  int before_send_event(THD *thd, ushort flags, String *packet,
                        const char *log_file, my_off_t log_pos);
  int after_send_event(THD *thd, ushort flags, String *packet,
                       const char *skipped_log_file, my_off_t skipped_log_pos);
  int after_reset_master(THD *thd, ushort flags);
};

#ifdef HAVE_PSI_RWLOCK_INTERFACE
extern PSI_rwlock_key key_rwlock_Binlog_relay_IO_delegate_lock;
#endif

class Binlog_relay_IO_delegate : public Delegate {
 public:
  Binlog_relay_IO_delegate()
      : Delegate(
#ifdef HAVE_PSI_RWLOCK_INTERFACE
            key_rwlock_Binlog_relay_IO_delegate_lock
#endif
        ) {
  }

  typedef Binlog_relay_IO_observer Observer;
  int thread_start(THD *thd, Master_info *mi);
  int thread_stop(THD *thd, Master_info *mi);
  int applier_start(THD *thd, Master_info *mi);
  int applier_stop(THD *thd, Master_info *mi, bool aborted);
  int before_request_transmit(THD *thd, Master_info *mi, ushort flags);
  int after_read_event(THD *thd, Master_info *mi, const char *packet, ulong len,
                       const char **event_buf, ulong *event_len);
  int after_queue_event(THD *thd, Master_info *mi, const char *event_buf,
                        ulong event_len, bool synced);
  int after_reset_slave(THD *thd, Master_info *mi);
  int applier_log_event(THD *thd, int &out);

 private:
  void init_param(Binlog_relay_IO_param *param, Master_info *mi);
};

int delegates_init();
/**
  Verify that the replication plugins are ready and OK to be unloaded.
 */
void delegates_shutdown();
void delegates_destroy();
/**
  Invokes `write_lock()` for all the observer delegate objects.
*/
void delegates_acquire_locks();
/**
  Releases locks for all the observer delegate objects.
*/
void delegates_release_locks();
/**
  Toggles the type of lock between a classical read-write lock and a
  shared-exclusive spin-lock.
*/
void delegates_update_lock_type();

extern Trans_delegate *transaction_delegate;
extern Binlog_storage_delegate *binlog_storage_delegate;
extern Server_state_delegate *server_state_delegate;
extern Binlog_transmit_delegate *binlog_transmit_delegate;
extern Binlog_relay_IO_delegate *binlog_relay_io_delegate;

/*
  if there is no observers in the delegate, we can return 0
  immediately.
*/
#define RUN_HOOK(group, hook, args) \
  (group##_delegate->is_empty() ? 0 : group##_delegate->hook args)

#define NO_HOOK(group) (group##_delegate->is_empty())

int launch_hook_trans_begin(THD *thd, Table_ref *table);

#endif /* RPL_HANDLER_H */
