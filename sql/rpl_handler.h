/* Copyright (c) 2008, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef RPL_HANDLER_H
#define RPL_HANDLER_H

#include "my_global.h"
#include "my_sys.h"                        // free_root
#include "mysql/psi/mysql_thread.h"        // mysql_rwlock_t
#include "mysqld.h"                        // key_memory_delegate
#include "sql_list.h"                      // List
#include "sql_plugin_ref.h"                // plugin_ref

#include <list>

class Master_info;
class String;
struct Binlog_relay_IO_observer;
struct Binlog_relay_IO_param;
struct Binlog_storage_observer;
struct Binlog_transmit_observer;
struct Server_state_observer;
struct Trans_observer;
struct Trans_table_info;


class Observer_info {
public:
  void *observer;
  st_plugin_int *plugin_int;
  plugin_ref plugin;

  Observer_info(void *ob, st_plugin_int *p);
};

class Delegate {
public:
  typedef List<Observer_info> Observer_info_list;
  typedef List_iterator<Observer_info> Observer_info_iterator;

  int add_observer(void *observer, st_plugin_int *plugin)
  {
    int ret= FALSE;
    if (!inited)
      return TRUE;
    write_lock();
    Observer_info_iterator iter(observer_info_list);
    Observer_info *info= iter++;
    while (info && info->observer != observer)
      info= iter++;
    if (!info)
    {
      info= new Observer_info(observer, plugin);
      if (!info || observer_info_list.push_back(info, &memroot))
        ret= TRUE;
    }
    else
      ret= TRUE;
    unlock();
    return ret;
  }

  int remove_observer(void *observer, st_plugin_int *plugin)
  {
    int ret= FALSE;
    if (!inited)
      return TRUE;
    write_lock();
    Observer_info_iterator iter(observer_info_list);
    Observer_info *info= iter++;
    while (info && info->observer != observer)
      info= iter++;
    if (info)
    {
      iter.remove();
      delete info;
    }
    else
      ret= TRUE;
    unlock();
    return ret;
  }

  inline Observer_info_iterator observer_info_iter()
  {
    return Observer_info_iterator(observer_info_list);
  }

  inline bool is_empty()
  {
    DBUG_PRINT("debug", ("is_empty: %d", observer_info_list.is_empty()));
    return observer_info_list.is_empty();
  }

  inline int read_lock()
  {
    if (!inited)
      return TRUE;
    return mysql_rwlock_rdlock(&lock);
  }

  inline int write_lock()
  {
    if (!inited)
      return TRUE;
    return mysql_rwlock_wrlock(&lock);
  }

  inline int unlock()
  {
    if (!inited)
      return TRUE;
    return mysql_rwlock_unlock(&lock);
  }

  inline bool is_inited()
  {
    return inited;
  }

  Delegate(
#ifdef HAVE_PSI_INTERFACE
           PSI_rwlock_key key
#endif
           )
  {
    inited= FALSE;
#ifdef HAVE_PSI_INTERFACE
    if (mysql_rwlock_init(key, &lock))
      return;
#else
    if (mysql_rwlock_init(0, &lock))
      return;
#endif
    init_sql_alloc(key_memory_delegate, &memroot, 1024, 0);
    inited= TRUE;
  }
  ~Delegate()
  {
    inited= FALSE;
    mysql_rwlock_destroy(&lock);
    free_root(&memroot, MYF(0));
  }

private:
  Observer_info_list observer_info_list;
  mysql_rwlock_t lock;
  MEM_ROOT memroot;
  bool inited;
};

#ifdef HAVE_PSI_INTERFACE
extern PSI_rwlock_key key_rwlock_Trans_delegate_lock;
#endif

class Trans_delegate
  :public Delegate {
public:

  Trans_delegate()
  : Delegate(
#ifdef HAVE_PSI_INTERFACE
             key_rwlock_Trans_delegate_lock
#endif
             )
  {}

  typedef Trans_observer Observer;

  int before_dml(THD *thd, int& result);
  int before_commit(THD *thd, bool all,
                    IO_CACHE *trx_cache_log,
                    IO_CACHE *stmt_cache_log,
                    ulonglong cache_log_max_size);
  int before_rollback(THD *thd, bool all);
  int after_commit(THD *thd, bool all);
  int after_rollback(THD *thd, bool all);
private:
  void prepare_table_info(THD* thd,
                          Trans_table_info*& table_info_list,
                          uint& number_of_tables);
};

#ifdef HAVE_PSI_INTERFACE
extern PSI_rwlock_key key_rwlock_Server_state_delegate_lock;
#endif

class Server_state_delegate
  :public Delegate {
public:

  Server_state_delegate()
  : Delegate(
#ifdef HAVE_PSI_INTERFACE
             key_rwlock_Server_state_delegate_lock
#endif
             )
  {}

  typedef Server_state_observer Observer;
  int before_handle_connection(THD *thd);
  int before_recovery(THD *thd);
  int after_engine_recovery(THD *thd);
  int after_recovery(THD *thd);
  int before_server_shutdown(THD *thd);
  int after_server_shutdown(THD *thd);
};

#ifdef HAVE_PSI_INTERFACE
extern PSI_rwlock_key key_rwlock_Binlog_storage_delegate_lock;
#endif

class Binlog_storage_delegate
  :public Delegate {
public:

  Binlog_storage_delegate()
  : Delegate(
#ifdef HAVE_PSI_INTERFACE
             key_rwlock_Binlog_storage_delegate_lock
#endif
             )
  {}

  typedef Binlog_storage_observer Observer;
  int after_flush(THD *thd, const char *log_file,
                  my_off_t log_pos);
  int after_sync(THD *thd, const char *log_file,
                 my_off_t log_pos);
};

#ifdef HAVE_REPLICATION
#ifdef HAVE_PSI_INTERFACE
extern PSI_rwlock_key key_rwlock_Binlog_transmit_delegate_lock;
#endif

class Binlog_transmit_delegate
  :public Delegate {
public:

  Binlog_transmit_delegate()
  : Delegate(
#ifdef HAVE_PSI_INTERFACE
             key_rwlock_Binlog_transmit_delegate_lock
#endif
             )
  {}

  typedef Binlog_transmit_observer Observer;
  int transmit_start(THD *thd, ushort flags,
                     const char *log_file, my_off_t log_pos,
                     bool *observe_transmission);
  int transmit_stop(THD *thd, ushort flags);
  int reserve_header(THD *thd, ushort flags, String *packet);
  int before_send_event(THD *thd, ushort flags,
                        String *packet, const
                        char *log_file, my_off_t log_pos );
  int after_send_event(THD *thd, ushort flags,
                       String *packet, const char *skipped_log_file,
                       my_off_t skipped_log_pos);
  int after_reset_master(THD *thd, ushort flags);
};

#ifdef HAVE_PSI_INTERFACE
extern PSI_rwlock_key key_rwlock_Binlog_relay_IO_delegate_lock;
#endif

class Binlog_relay_IO_delegate
  :public Delegate {
public:

  Binlog_relay_IO_delegate()
  : Delegate(
#ifdef HAVE_PSI_INTERFACE
             key_rwlock_Binlog_relay_IO_delegate_lock
#endif
             )
  {}

  typedef Binlog_relay_IO_observer Observer;
  int thread_start(THD *thd, Master_info *mi);
  int thread_stop(THD *thd, Master_info *mi);
  int applier_start(THD *thd, Master_info *mi);
  int applier_stop(THD *thd, Master_info *mi, bool aborted);
  int before_request_transmit(THD *thd, Master_info *mi, ushort flags);
  int after_read_event(THD *thd, Master_info *mi,
                       const char *packet, ulong len,
                       const char **event_buf, ulong *event_len);
  int after_queue_event(THD *thd, Master_info *mi,
                        const char *event_buf, ulong event_len,
                        bool synced);
  int after_reset_slave(THD *thd, Master_info *mi);
private:
  void init_param(Binlog_relay_IO_param *param, Master_info *mi);
};
#endif /* HAVE_REPLICATION */

int delegates_init();
void delegates_destroy();

extern Trans_delegate *transaction_delegate;
extern Binlog_storage_delegate *binlog_storage_delegate;
extern Server_state_delegate *server_state_delegate;
#ifdef HAVE_REPLICATION
extern Binlog_transmit_delegate *binlog_transmit_delegate;
extern Binlog_relay_IO_delegate *binlog_relay_io_delegate;
#endif /* HAVE_REPLICATION */

/*
  if there is no observers in the delegate, we can return 0
  immediately.
*/
#define RUN_HOOK(group, hook, args)             \
  (group ##_delegate->is_empty() ?              \
   0 : group ##_delegate->hook args)

#define NO_HOOK(group) (group ##_delegate->is_empty())

#endif /* RPL_HANDLER_H */
