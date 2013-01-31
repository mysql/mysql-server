/* Copyright (c) 2008, 2010, 2012 Oracle and/or its affiliates. All rights reserved.

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

#include "sql_priv.h"
#include "rpl_gtid.h"
#include "rpl_mi.h"
#include "rpl_rli.h"
#include "sql_plugin.h"
#include "replication.h"

class Observer_info {
public:
  void *observer;
  st_plugin_int *plugin_int;
  plugin_ref plugin;

  Observer_info(void *ob, st_plugin_int *p)
    :observer(ob), plugin_int(p)
  {
    plugin= plugin_int_to_ref(plugin_int);
  }
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
    return rw_rdlock(&lock);
  }

  inline int write_lock()
  {
    if (!inited)
      return TRUE;
    return rw_wrlock(&lock);
  }

  inline int unlock()
  {
    if (!inited)
      return TRUE;
    return rw_unlock(&lock);
  }

  inline bool is_inited()
  {
    return inited;
  }
  
  Delegate()
  {
    inited= FALSE;
    if (my_rwlock_init(&lock, NULL))
      return;
    init_sql_alloc(&memroot, 1024, 0);
    inited= TRUE;
  }
  ~Delegate()
  {
    inited= FALSE;
    rwlock_destroy(&lock);
    free_root(&memroot, MYF(0));
  }

private:
  Observer_info_list observer_info_list;
  rw_lock_t lock;
  MEM_ROOT memroot;
  bool inited;
};

class Trans_delegate
  :public Delegate {
public:
  typedef Trans_observer Observer;
  int before_commit(THD *thd, bool all);
  int before_rollback(THD *thd, bool all);
  int after_commit(THD *thd, bool all);
  int after_rollback(THD *thd, bool all);
};

class Binlog_storage_delegate
  :public Delegate {
public:
  typedef Binlog_storage_observer Observer;
  int after_flush(THD *thd, const char *log_file,
                  my_off_t log_pos);
};

#ifdef HAVE_REPLICATION
class Binlog_transmit_delegate
  :public Delegate {
public:
  typedef Binlog_transmit_observer Observer;
  int transmit_start(THD *thd, ushort flags,
                     const char *log_file, my_off_t log_pos);
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

class Binlog_relay_IO_delegate
  :public Delegate {
public:
  typedef Binlog_relay_IO_observer Observer;
  int thread_start(THD *thd, Master_info *mi);
  int thread_stop(THD *thd, Master_info *mi);
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

#endif /* RPL_HANDLER_H */
