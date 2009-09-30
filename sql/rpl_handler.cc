/* Copyright (C) 2008 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "mysql_priv.h"

#include "rpl_mi.h"
#include "sql_repl.h"
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

static pthread_key(Trans_binlog_info*, RPL_TRANS_BINLOG_INFO);

int get_user_var_int(const char *name,
                     long long int *value, int *null_value)
{
  my_bool null_val;
  user_var_entry *entry= 
    (user_var_entry*) hash_search(&current_thd->user_vars,
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
    (user_var_entry*) hash_search(&current_thd->user_vars,
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
    (user_var_entry*) hash_search(&current_thd->user_vars,
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
  static unsigned long trans_mem[sizeof(Trans_delegate) / sizeof(unsigned long) + 1];
  static unsigned long storage_mem[sizeof(Binlog_storage_delegate) / sizeof(unsigned long) + 1];
#ifdef HAVE_REPLICATION
  static unsigned long transmit_mem[sizeof(Binlog_transmit_delegate) / sizeof(unsigned long) + 1];
  static unsigned long relay_io_mem[sizeof(Binlog_relay_IO_delegate)/ sizeof(unsigned long) + 1];
#endif
  
  if (!(transaction_delegate= new (trans_mem) Trans_delegate)
      || (!transaction_delegate->is_inited())
      || !(binlog_storage_delegate= new (storage_mem) Binlog_storage_delegate)
      || (!binlog_storage_delegate->is_inited())
#ifdef HAVE_REPLICATION
      || !(binlog_transmit_delegate= new (transmit_mem) Binlog_transmit_delegate)
      || (!binlog_transmit_delegate->is_inited())
      || !(binlog_relay_io_delegate= new (relay_io_mem) Binlog_relay_IO_delegate)
      || (!binlog_relay_io_delegate->is_inited())
#endif /* HAVE_REPLICATION */
      )
    return 1;

  if (pthread_key_create(&RPL_TRANS_BINLOG_INFO, NULL))
    return 1;
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
  read_lock();                                                          \
  Observer_info_iterator iter= observer_info_iter();                    \
  Observer_info *info= iter++;                                          \
  for (; info; info= iter++)                                            \
  {                                                                     \
    plugin_ref plugin=                                                  \
      my_plugin_lock(thd, &info->plugin);                               \
    if (!plugin)                                                        \
    {                                                                   \
      r= 1;                                                             \
      break;                                                            \
    }                                                                   \
    if (((Observer *)info->observer)->f                                 \
        && ((Observer *)info->observer)->f args)                        \
    {                                                                   \
      r= 1;                                                             \
      plugin_unlock(thd, plugin);                                       \
      break;                                                            \
    }                                                                   \
    plugin_unlock(thd, plugin);                                         \
  }                                                                     \
  unlock()


int Trans_delegate::after_commit(THD *thd, bool all)
{
  Trans_param param;
  bool is_real_trans= (all || thd->transaction.all.ha_list == 0);
  if (is_real_trans)
    param.flags |= TRANS_IS_REAL_TRANS;

  Trans_binlog_info *log_info=
    my_pthread_getspecific_ptr(Trans_binlog_info*, RPL_TRANS_BINLOG_INFO);

  param.log_file= log_info ? log_info->log_file : 0;
  param.log_pos= log_info ? log_info->log_pos : 0;

  int ret= 0;
  FOREACH_OBSERVER(ret, after_commit, thd, (&param));

  /*
    This is the end of a real transaction or autocommit statement, we
    can free the memory allocated for binlog file and position.
  */
  if (is_real_trans && log_info)
  {
    my_pthread_setspecific_ptr(RPL_TRANS_BINLOG_INFO, NULL);
    my_free(log_info, MYF(0));
  }
  return ret;
}

int Trans_delegate::after_rollback(THD *thd, bool all)
{
  Trans_param param;
  bool is_real_trans= (all || thd->transaction.all.ha_list == 0);
  if (is_real_trans)
    param.flags |= TRANS_IS_REAL_TRANS;

  Trans_binlog_info *log_info=
    my_pthread_getspecific_ptr(Trans_binlog_info*, RPL_TRANS_BINLOG_INFO);
    
  param.log_file= log_info ? log_info->log_file : 0;
  param.log_pos= log_info ? log_info->log_pos : 0;

  int ret= 0;
  FOREACH_OBSERVER(ret, after_commit, thd, (&param));

  /*
    This is the end of a real transaction or autocommit statement, we
    can free the memory allocated for binlog file and position.
  */
  if (is_real_trans && log_info)
  {
    my_pthread_setspecific_ptr(RPL_TRANS_BINLOG_INFO, NULL);
    my_free(log_info, MYF(0));
  }
  return ret;
}

int Binlog_storage_delegate::after_flush(THD *thd,
                                         const char *log_file,
                                         my_off_t log_pos,
                                         bool synced)
{
  Binlog_storage_param param;
  uint32 flags=0;
  if (synced)
    flags |= BINLOG_STORAGE_IS_SYNCED;

  Trans_binlog_info *log_info=
    my_pthread_getspecific_ptr(Trans_binlog_info*, RPL_TRANS_BINLOG_INFO);
    
  if (!log_info)
  {
    if(!(log_info=
         (Trans_binlog_info *)my_malloc(sizeof(Trans_binlog_info), MYF(0))))
      return 1;
    my_pthread_setspecific_ptr(RPL_TRANS_BINLOG_INFO, log_info);
  }
    
  strcpy(log_info->log_file, log_file+dirname_length(log_file));
  log_info->log_pos = log_pos;
  
  int ret= 0;
  FOREACH_OBSERVER(ret, after_flush, thd,
                   (&param, log_info->log_file, log_info->log_pos, flags));
  return ret;
}

#ifdef HAVE_REPLICATION
int Binlog_transmit_delegate::transmit_start(THD *thd, ushort flags,
                                             const char *log_file,
                                             my_off_t log_pos)
{
  Binlog_transmit_param param;
  param.flags= flags;

  int ret= 0;
  FOREACH_OBSERVER(ret, transmit_start, thd, (&param, log_file, log_pos));
  return ret;
}

int Binlog_transmit_delegate::transmit_stop(THD *thd, ushort flags)
{
  Binlog_transmit_param param;
  param.flags= flags;

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

  int ret= 0;
  FOREACH_OBSERVER(ret, before_send_event, thd,
                   (&param, (uchar *)packet->c_ptr(),
                    packet->length(),
                    log_file+dirname_length(log_file), log_pos));
  return ret;
}

int Binlog_transmit_delegate::after_send_event(THD *thd, ushort flags,
                                               String *packet)
{
  Binlog_transmit_param param;
  param.flags= flags;

  int ret= 0;
  FOREACH_OBSERVER(ret, after_send_event, thd,
                   (&param, packet->c_ptr(), packet->length()));
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
  param->user= mi->user;
  param->host= mi->host;
  param->port= mi->port;
  param->master_log_name= mi->master_log_name;
  param->master_log_pos= mi->master_log_pos;
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
  return binlog_storage_delegate->add_observer(observer, (st_plugin_int *)p);
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
