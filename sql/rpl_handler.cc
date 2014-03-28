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

#include "sql_priv.h"
#include "unireg.h"

#include "rpl_mi.h"
#include "log_event.h"
#include "rpl_filter.h"
#include <my_dir.h>
#include "rpl_handler.h"

Trans_delegate *transaction_delegate;
Binlog_storage_delegate *binlog_storage_delegate;
#ifdef HAVE_REPLICATION
Binlog_transmit_delegate *binlog_transmit_delegate;
Binlog_relay_IO_delegate *binlog_relay_io_delegate;
#endif /* HAVE_REPLICATION */

/*
  structure to save transaction log filename and position
*/
typedef struct Trans_binlog_info {
  my_off_t log_pos;
  char log_file[FN_REFLEN];
} Trans_binlog_info;

int get_user_var_int(const char *name,
                     long long int *value, int *null_value)
{
  my_bool null_val;
  user_var_entry *entry= 
    (user_var_entry*) my_hash_search(&current_thd->user_vars,
                                  (uchar*) name, strlen(name));
  if (!entry)
    return 1;
  *value= entry->val_int(&null_val);
  if (null_value)
    *null_value= null_val;
  return 0;
}

int get_user_var_real(const char *name,
                      double *value, int *null_value)
{
  my_bool null_val;
  user_var_entry *entry= 
    (user_var_entry*) my_hash_search(&current_thd->user_vars,
                                  (uchar*) name, strlen(name));
  if (!entry)
    return 1;
  *value= entry->val_real(&null_val);
  if (null_value)
    *null_value= null_val;
  return 0;
}

int get_user_var_str(const char *name, char *value,
                     size_t len, unsigned int precision, int *null_value)
{
  String str;
  my_bool null_val;
  user_var_entry *entry= 
    (user_var_entry*) my_hash_search(&current_thd->user_vars,
                                  (uchar*) name, strlen(name));
  if (!entry)
    return 1;
  entry->val_str(&null_val, &str, precision);
  strncpy(value, str.c_ptr(), len);
  if (null_value)
    *null_value= null_val;
  return 0;
}

int delegates_init()
{
  static my_aligned_storage<sizeof(Trans_delegate), MY_ALIGNOF(long)> trans_mem;
  static my_aligned_storage<sizeof(Binlog_storage_delegate),
                            MY_ALIGNOF(long)> storage_mem;
#ifdef HAVE_REPLICATION
  static my_aligned_storage<sizeof(Binlog_transmit_delegate),
                            MY_ALIGNOF(long)> transmit_mem;
  static my_aligned_storage<sizeof(Binlog_relay_IO_delegate),
                            MY_ALIGNOF(long)> relay_io_mem;
#endif

  void *place_trans_mem= trans_mem.data;
  void *place_storage_mem= storage_mem.data;

  transaction_delegate= new (place_trans_mem) Trans_delegate;

  if (!transaction_delegate->is_inited())
  {
    sql_print_error("Initialization of transaction delegates failed. "
                    "Please report a bug.");
    return 1;
  }

  binlog_storage_delegate= new (place_storage_mem) Binlog_storage_delegate;

  if (!binlog_storage_delegate->is_inited())
  {
    sql_print_error("Initialization binlog storage delegates failed. "
                    "Please report a bug.");
    return 1;
  }

#ifdef HAVE_REPLICATION
  void *place_transmit_mem= transmit_mem.data;
  void *place_relay_io_mem= relay_io_mem.data;

  binlog_transmit_delegate= new (place_transmit_mem) Binlog_transmit_delegate;

  if (!binlog_transmit_delegate->is_inited())
  {
    sql_print_error("Initialization of binlog transmit delegates failed. "
                    "Please report a bug.");
    return 1;
  }

  binlog_relay_io_delegate= new (place_relay_io_mem) Binlog_relay_IO_delegate;

  if (!binlog_relay_io_delegate->is_inited())
  {
    sql_print_error("Initialization binlog relay IO delegates failed. "
                    "Please report a bug.");
    return 1;
  }
#endif

  return 0;
}

void delegates_destroy()
{
  if (transaction_delegate)
    transaction_delegate->~Trans_delegate();
  if (binlog_storage_delegate)
    binlog_storage_delegate->~Binlog_storage_delegate();
#ifdef HAVE_REPLICATION
  if (binlog_transmit_delegate)
    binlog_transmit_delegate->~Binlog_transmit_delegate();
  if (binlog_relay_io_delegate)
    binlog_relay_io_delegate->~Binlog_relay_IO_delegate();
#endif /* HAVE_REPLICATION */
}

/*
  This macro is used by almost all the Delegate methods to iterate
  over all the observers running given callback function of the
  delegate .
  
  Add observer plugins to the thd->lex list, after each statement, all
  plugins add to thd->lex will be automatically unlocked.
 */
#define FOREACH_OBSERVER(r, f, thd, args)                               \
  param.server_id= thd->server_id;                                      \
  /*
     Use a struct to make sure that they are allocated adjacent, check
     delete_dynamic().
  */                                                                    \
  struct {                                                              \
    DYNAMIC_ARRAY plugins;                                              \
    /* preallocate 8 slots */                                           \
    plugin_ref plugins_buffer[8];                                       \
  } s;                                                                  \
  DYNAMIC_ARRAY *plugins= &s.plugins;                                   \
  plugin_ref *plugins_buffer= s.plugins_buffer;                         \
  my_init_dynamic_array2(plugins, sizeof(plugin_ref),                   \
                         plugins_buffer, 8, 8);                         \
  read_lock();                                                          \
  Observer_info_iterator iter= observer_info_iter();                    \
  Observer_info *info= iter++;                                          \
  for (; info; info= iter++)                                            \
  {                                                                     \
    plugin_ref plugin=                                                  \
      my_plugin_lock(0, &info->plugin);                                 \
    if (!plugin)                                                        \
    {                                                                   \
      /* plugin is not intialized or deleted, this is not an error */   \
      r= 0;                                                             \
      break;                                                            \
    }                                                                   \
    insert_dynamic(plugins, &plugin);                                   \
    if (((Observer *)info->observer)->f                                 \
        && ((Observer *)info->observer)->f args)                        \
    {                                                                   \
      r= 1;                                                             \
      sql_print_error("Run function '" #f "' in plugin '%s' failed",    \
                      info->plugin_int->name.str);                      \
      break;                                                            \
    }                                                                   \
  }                                                                     \
  unlock();                                                             \
  /* 
     Unlock plugins should be done after we released the Delegate lock
     to avoid possible deadlock when this is the last user of the
     plugin, and when we unlock the plugin, it will try to
     deinitialize the plugin, which will try to lock the Delegate in
     order to remove the observers.
  */                                                                    \
  plugin_unlock_list(0, (plugin_ref*)plugins->buffer,                   \
                     plugins->elements);                                \
  delete_dynamic(plugins)


int Trans_delegate::after_commit(THD *thd, bool all)
{
  DBUG_ENTER("Trans_delegate::after_commit");
  Trans_param param = { 0, 0, 0, 0 };
  bool is_real_trans= (all || thd->transaction.all.ha_list == 0);

  if (is_real_trans)
    param.flags = true;

  thd->get_trans_fixed_pos(&param.log_file, &param.log_pos);

  DBUG_PRINT("enter", ("log_file: %s, log_pos: %llu", param.log_file, param.log_pos));

  int ret= 0;
  FOREACH_OBSERVER(ret, after_commit, thd, (&param));
  DBUG_RETURN(ret);
}

int Trans_delegate::after_rollback(THD *thd, bool all)
{
  Trans_param param = { 0, 0, 0, 0 };
  bool is_real_trans= (all || thd->transaction.all.ha_list == 0);

  if (is_real_trans)
    param.flags|= TRANS_IS_REAL_TRANS;
  thd->get_trans_fixed_pos(&param.log_file, &param.log_pos);
  int ret= 0;
  FOREACH_OBSERVER(ret, after_rollback, thd, (&param));
  return ret;
}

int Binlog_storage_delegate::after_flush(THD *thd,
                                         const char *log_file,
                                         my_off_t log_pos)
{
  DBUG_ENTER("Binlog_storage_delegate::after_flush");
  DBUG_PRINT("enter", ("log_file: %s, log_pos: %llu",
                       log_file, (ulonglong) log_pos));
  Binlog_storage_param param;

  int ret= 0;
  FOREACH_OBSERVER(ret, after_flush, thd, (&param, log_file, log_pos));
  DBUG_RETURN(ret);
}

#ifdef HAVE_REPLICATION
int Binlog_transmit_delegate::transmit_start(THD *thd, ushort flags,
                                             const char *log_file,
                                             my_off_t log_pos,
                                             bool *observe_transmission)
{
  Binlog_transmit_param param;
  param.flags= flags;

  int ret= 0;
  FOREACH_OBSERVER(ret, transmit_start, thd, (&param, log_file, log_pos));
  *observe_transmission= param.should_observe();
  return ret;
}

int Binlog_transmit_delegate::transmit_stop(THD *thd, ushort flags)
{
  Binlog_transmit_param param;
  param.flags= flags;

  DBUG_EXECUTE_IF("crash_binlog_transmit_hook", DBUG_SUICIDE(););

  int ret= 0;
  FOREACH_OBSERVER(ret, transmit_stop, thd, (&param));
  return ret;
}

int Binlog_transmit_delegate::reserve_header(THD *thd, ushort flags,
                                             String *packet)
{
  /* NOTE2ME: Maximum extra header size for each observer, I hope 32
     bytes should be enough for each Observer to reserve their extra
     header. If later found this is not enough, we can increase this
     /HEZX
  */
#define RESERVE_HEADER_SIZE 32
  unsigned char header[RESERVE_HEADER_SIZE];
  ulong hlen;
  Binlog_transmit_param param;
  param.flags= flags;
  param.server_id= thd->server_id;

  DBUG_EXECUTE_IF("crash_binlog_transmit_hook", DBUG_SUICIDE(););

  int ret= 0;
  read_lock();
  Observer_info_iterator iter= observer_info_iter();
  Observer_info *info= iter++;
  for (; info; info= iter++)
  {
    plugin_ref plugin=
      my_plugin_lock(thd, &info->plugin);
    if (!plugin)
    {
      ret= 1;
      break;
    }
    hlen= 0;
    if (((Observer *)info->observer)->reserve_header
        && ((Observer *)info->observer)->reserve_header(&param,
                                                        header,
                                                        RESERVE_HEADER_SIZE,
                                                        &hlen))
    {
      ret= 1;
      plugin_unlock(thd, plugin);
      break;
    }
    plugin_unlock(thd, plugin);
    if (hlen == 0)
      continue;
    if (hlen > RESERVE_HEADER_SIZE || packet->append((char *)header, hlen))
    {
      ret= 1;
      break;
    }
  }
  unlock();
  return ret;
}

int Binlog_transmit_delegate::before_send_event(THD *thd, ushort flags,
                                                String *packet,
                                                const char *log_file,
                                                my_off_t log_pos)
{
  Binlog_transmit_param param;
  param.flags= flags;

  DBUG_EXECUTE_IF("crash_binlog_transmit_hook", DBUG_SUICIDE(););

  int ret= 0;
  FOREACH_OBSERVER(ret, before_send_event, thd,
                   (&param, (uchar *)packet->c_ptr(),
                    packet->length(),
                    log_file+dirname_length(log_file), log_pos));
  return ret;
}

int Binlog_transmit_delegate::after_send_event(THD *thd, ushort flags,
                                               String *packet,
                                               const char *skipped_log_file,
                                               my_off_t skipped_log_pos)
{
  Binlog_transmit_param param;
  param.flags= flags;

  DBUG_EXECUTE_IF("crash_binlog_transmit_hook", DBUG_SUICIDE(););

  int ret= 0;
  FOREACH_OBSERVER(ret, after_send_event, thd,
                   (&param, packet->c_ptr(), packet->length(),
                   skipped_log_file+dirname_length(skipped_log_file),
                    skipped_log_pos));
  return ret;
}

int Binlog_transmit_delegate::after_reset_master(THD *thd, ushort flags)

{
  Binlog_transmit_param param;
  param.flags= flags;

  int ret= 0;
  FOREACH_OBSERVER(ret, after_reset_master, thd, (&param));
  return ret;
}

void Binlog_relay_IO_delegate::init_param(Binlog_relay_IO_param *param,
                                          Master_info *mi)
{
  param->mysql= mi->mysql;
  param->user= const_cast<char *>(mi->get_user());
  param->host= mi->host;
  param->port= mi->port;
  param->master_log_name= const_cast<char *>(mi->get_master_log_name());
  param->master_log_pos= mi->get_master_log_pos();
}

int Binlog_relay_IO_delegate::thread_start(THD *thd, Master_info *mi)
{
  Binlog_relay_IO_param param;
  init_param(&param, mi);

  int ret= 0;
  FOREACH_OBSERVER(ret, thread_start, thd, (&param));
  return ret;
}


int Binlog_relay_IO_delegate::thread_stop(THD *thd, Master_info *mi)
{

  Binlog_relay_IO_param param;
  init_param(&param, mi);

  int ret= 0;
  FOREACH_OBSERVER(ret, thread_stop, thd, (&param));
  return ret;
}

int Binlog_relay_IO_delegate::before_request_transmit(THD *thd,
                                                      Master_info *mi,
                                                      ushort flags)
{
  Binlog_relay_IO_param param;
  init_param(&param, mi);

  int ret= 0;
  FOREACH_OBSERVER(ret, before_request_transmit, thd, (&param, (uint32)flags));
  return ret;
}

int Binlog_relay_IO_delegate::after_read_event(THD *thd, Master_info *mi,
                                               const char *packet, ulong len,
                                               const char **event_buf,
                                               ulong *event_len)
{
  Binlog_relay_IO_param param;
  init_param(&param, mi);

  int ret= 0;
  FOREACH_OBSERVER(ret, after_read_event, thd,
                   (&param, packet, len, event_buf, event_len));
  return ret;
}

int Binlog_relay_IO_delegate::after_queue_event(THD *thd, Master_info *mi,
                                                const char *event_buf,
                                                ulong event_len,
                                                bool synced)
{
  Binlog_relay_IO_param param;
  init_param(&param, mi);

  uint32 flags=0;
  if (synced)
    flags |= BINLOG_STORAGE_IS_SYNCED;

  int ret= 0;
  FOREACH_OBSERVER(ret, after_queue_event, thd,
                   (&param, event_buf, event_len, flags));
  return ret;
}

int Binlog_relay_IO_delegate::after_reset_slave(THD *thd, Master_info *mi)

{
  Binlog_relay_IO_param param;
  init_param(&param, mi);

  int ret= 0;
  FOREACH_OBSERVER(ret, after_reset_slave, thd, (&param));
  return ret;
}
#endif /* HAVE_REPLICATION */

int register_trans_observer(Trans_observer *observer, void *p)
{
  return transaction_delegate->add_observer(observer, (st_plugin_int *)p);
}

int unregister_trans_observer(Trans_observer *observer, void *p)
{
  return transaction_delegate->remove_observer(observer, (st_plugin_int *)p);
}

int register_binlog_storage_observer(Binlog_storage_observer *observer, void *p)
{
  DBUG_ENTER("register_binlog_storage_observer");
  int result= binlog_storage_delegate->add_observer(observer, (st_plugin_int *)p);
  DBUG_RETURN(result);
}

int unregister_binlog_storage_observer(Binlog_storage_observer *observer, void *p)
{
  return binlog_storage_delegate->remove_observer(observer, (st_plugin_int *)p);
}

#ifdef HAVE_REPLICATION
int register_binlog_transmit_observer(Binlog_transmit_observer *observer, void *p)
{
  return binlog_transmit_delegate->add_observer(observer, (st_plugin_int *)p);
}

int unregister_binlog_transmit_observer(Binlog_transmit_observer *observer, void *p)
{
  return binlog_transmit_delegate->remove_observer(observer, (st_plugin_int *)p);
}

int register_binlog_relay_io_observer(Binlog_relay_IO_observer *observer, void *p)
{
  return binlog_relay_io_delegate->add_observer(observer, (st_plugin_int *)p);
}

int unregister_binlog_relay_io_observer(Binlog_relay_IO_observer *observer, void *p)
{
  return binlog_relay_io_delegate->remove_observer(observer, (st_plugin_int *)p);
}
#endif /* HAVE_REPLICATION */
