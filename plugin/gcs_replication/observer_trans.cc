/* Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.

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

#include "gcs_plugin.h"
#include "observer_trans.h"
#include "gcs_plugin_utils.h"
#include <log_event.h>
#include <my_stacktrace.h>
#include "sql_class.h"
#include "gcs_replication.h"

/*
  Internal auxiliary functions signatures.
*/
static bool reinit_cache(IO_CACHE *cache,
                         enum cache_type type,
                         my_off_t position);

int add_write_set(Transaction_context_log_event *tcle,
                   std::list<uint32> *set)
{
  DBUG_ENTER("enter_write_set");
  for (std::list<uint32>::iterator it= set->begin();
       it!=set->end();
       ++it)
  {
    char buff[22];
    const char *pke_field_value= my_safe_itoa(10, *it, &buff[sizeof(buff)-1]);
    // TODO: This will be later moved to transaction memroot to be persisted
    //       as long as the transaction persists.
    char *write_set_value=my_strdup(PSI_NOT_INSTRUMENTED, pke_field_value,
                                    MYF(MY_WME));
    if (write_set_value)
      tcle->add_write_set(write_set_value);
    else
    {
      log_message(MY_ERROR_LEVEL, "Failed during mysql_strdup call");
      DBUG_RETURN(1);
    }
  }
  DBUG_RETURN(0);
}

/*
  Transaction lifecycle events observers.
*/
int gcs_trans_before_commit(Trans_param *param)
{
  DBUG_ENTER("gcs_trans_before_commit");
  int error= 0;

  if (!is_gcs_rpl_running())
    DBUG_RETURN(0);

  /*If the originating id belongs to a thread in the plugin, the transaction was already certified*/
  if (applier_module->is_own_event_channel(param->thread_id)
        || recovery_module->is_own_event_channel(param->thread_id))
    DBUG_RETURN(0);

  bool is_real_trans= param->flags & TRANS_IS_REAL_TRANS;
  if (!is_real_trans)
    DBUG_RETURN(0);

  mysql_cond_t COND_certify_wait;
  mysql_mutex_t LOCK_certify_wait;

  int mutex_init= 0;
  // GCS cache.
  Transaction_context_log_event *tcle= NULL;
  rpl_gno snapshot_timestamp;
  IO_CACHE cache;
  // Todo optimize for memory (IO-cache's buf to start with, if not enough then trans mem-root)
  // to avoid New message create/delete and/or its implicit MessageBuffer.
  Transaction_Message transaction_msg;

  // Binlog cache.
  bool is_dml= true;
  IO_CACHE *cache_log= NULL;
  my_off_t cache_log_position= 0;
  const my_off_t trx_cache_log_position= my_b_tell(param->trx_cache_log);
  const my_off_t stmt_cache_log_position= my_b_tell(param->stmt_cache_log);

  mutex_init= init_cond_mutex(&COND_certify_wait, &LOCK_certify_wait);
  if (mutex_init)
  {
    log_message(MY_ERROR_LEVEL, "Failed to initialize the mutex and condtion");
    error= 1;
    goto err;
  }

  if (trx_cache_log_position > 0 && stmt_cache_log_position == 0)
  {
    cache_log= param->trx_cache_log;
    cache_log_position= trx_cache_log_position;
  }
  else if (trx_cache_log_position == 0 && stmt_cache_log_position > 0)
  {
    cache_log= param->stmt_cache_log;
    cache_log_position= stmt_cache_log_position;
    is_dml= false;
  }
  else
  {
    log_message(MY_ERROR_LEVEL, "We can only use one cache type at a time");
    error= 1;
    goto err;
  }

  DBUG_ASSERT(cache_log->type == WRITE_CACHE);
  DBUG_PRINT("cache_log", ("thread_id: %u, trx_cache_log_position: %llu,"
                           " stmt_cache_log_position: %llu",
                           param->thread_id, trx_cache_log_position,
                           stmt_cache_log_position));

  // Get transaction snapshot timestamp.
  snapshot_timestamp= get_last_executed_gno_without_gaps(gcs_cluster_sidno);
  DBUG_PRINT("snapshot_timestamp", ("snapshot_timestamp: %llu",
                                    snapshot_timestamp));

  // Open GCS cache.
  if (open_cached_file(&cache, mysql_tmpdir, "gcs_trans_before_commit_cache",
                       param->cache_log_max_size, MYF(MY_WME)))
  {
    log_message(MY_ERROR_LEVEL, "Failed to create gcs commit cache");
    error= 1;
    goto err;
  }

  // Reinit binlog cache to read.
  if (reinit_cache(cache_log, READ_CACHE, 0))
  {
    log_message(MY_ERROR_LEVEL, "Failed to reinit binlog cache log for read");
    error= 1;
    goto err;
  }

  // Create transaction context.
  tcle= new Transaction_context_log_event(param->server_uuid,
                                          param->thread_id,
                                          snapshot_timestamp);


  // TODO: For now DDL won't have write-set, it will be added by
  // WL#6823 and WL#6824.
  if (is_dml)
  {
    if (add_write_set(tcle, param->write_set))
    {
      log_message(MY_ERROR_LEVEL, "Failed to add values to tcle write_set");
      error= 1;
      goto err;
    }
    DBUG_ASSERT(tcle->get_write_set()->size() > 0);
  }

  // Write transaction context to GCS cache.
  tcle->write(&cache);

  // Reinit GCS cache to read.
  if (reinit_cache(&cache, READ_CACHE, 0))
  {
    log_message(MY_ERROR_LEVEL, "Failed to reinit GCS cache log for read");
    error= 1;
    goto err;
  }

  // Copy GCS cache to buffer.
  if (transaction_msg.append_cache(&cache))
  {
    log_message(MY_ERROR_LEVEL, "Failed while writing GCS cache to buffer");
    error= 1;
    goto err;
  }

  // Copy binlog cache content to buffer.
  if (transaction_msg.append_cache(cache_log))
  {
    log_message(MY_ERROR_LEVEL, "Failed while writing binlog cache to buffer");
    error= 1;
    goto err;
  }

  // Reinit binlog cache to write (revert what we did).
  if (reinit_cache(cache_log, WRITE_CACHE, cache_log_position))
  {
    log_message(MY_ERROR_LEVEL, "Failed to reinit binlog cache log for write");
    error= 1;
    goto err;
  }

  if ((add_transaction_wait_cond(param->thread_id, &COND_certify_wait,
                                 &LOCK_certify_wait)))
  {
    log_message(MY_ERROR_LEVEL, "Insertion of the condition variables in the map failed.");
    error= 1;
    goto err;
  }

  //Broadcast the Transaction Message
  if (send_transaction_message(&transaction_msg))
  {
    log_message(MY_ERROR_LEVEL, "Failed to broadcast GCS message");
    error= 1;
    goto err;
  }

  mysql_mutex_lock(&LOCK_certify_wait);
  while ((get_transaction_certification_result(param->thread_id)).second == -1)
    mysql_cond_wait(&COND_certify_wait, &LOCK_certify_wait);
  mysql_mutex_unlock(&LOCK_certify_wait);
  delete_transaction_wait_cond(param->thread_id);

err:
  delete tcle;
  if (!mutex_init)
    destroy_cond_mutex(&COND_certify_wait, &LOCK_certify_wait);
  close_cached_file(&cache);
  mutex_init= 0;
  DBUG_RETURN(error);
}

int gcs_trans_before_rollback(Trans_param *param)
{
  DBUG_ENTER("gcs_trans_before_rollback");
  DBUG_RETURN(0);
}

int gcs_trans_after_commit(Trans_param *param)
{
  DBUG_ENTER("gcs_trans_after_commit");
  DBUG_RETURN(0);
}

int gcs_trans_after_rollback(Trans_param *param)
{
  DBUG_ENTER("gcs_trans_after_rollback");
  DBUG_RETURN(0);
}

Trans_observer trans_observer = {
  sizeof(Trans_observer),

  gcs_trans_before_commit,
  gcs_trans_before_rollback,
  gcs_trans_after_commit,
  gcs_trans_after_rollback,
};

/*
  Internal auxiliary functions.
*/

/*
  Reinit IO_cache type.

  @param[in] cache     cache
  @param[in] type      type to which cache will change
  @param[in] position  position to which cache will seek
*/
static bool reinit_cache(IO_CACHE *cache,
                         enum cache_type type,
                         my_off_t position)
{
  DBUG_ENTER("reinit_cache");

  if (READ_CACHE == type && flush_io_cache(cache))
    DBUG_RETURN(true);

  if (reinit_io_cache(cache, type, position, 0, 0))
    DBUG_RETURN(true);

  DBUG_RETURN(false);
}

bool send_transaction_message(Transaction_Message* msg)
{
  string gcs_group_name(gcs_group_pointer);
  Gcs_group_identifier group_id(gcs_group_name);

  Gcs_communication_interface *comm_if
                              = gcs_module->get_communication_session(group_id);
  Gcs_control_interface *ctrl_if
                              = gcs_module->get_control_session(group_id);

  Gcs_message to_send(*ctrl_if->get_local_information(),
                      *ctrl_if->get_current_view()->get_group_id(),
                      UNIFORM);

  vector<uchar> transaction_message_data;
  msg->encode(&transaction_message_data);
  to_send.append_to_payload(&transaction_message_data.front(),
                             transaction_message_data.size());

  return comm_if->send_message(&to_send);
}

//Transaction Message implementation

Transaction_Message::Transaction_Message():Gcs_plugin_message(PAYLOAD_TRANSACTION_EVENT)
{
}

Transaction_Message::~Transaction_Message()
{
}

bool
Transaction_Message::append_cache(IO_CACHE *src)
{
  DBUG_ENTER("copy_cache");
  size_t length;

  DBUG_ASSERT(src->type == READ_CACHE);

  while ((length= my_b_fill(src)) > 0)
  {
    if (src->error)
      DBUG_RETURN(true);

    data.insert(data.end(),
                src->read_pos,
                src->read_pos + length);
  }

  DBUG_RETURN(false);
}

void
Transaction_Message::encode_message(vector<uchar>* buf)
{
  buf->insert(buf->end(), data.begin(), data.end());
}

void
Transaction_Message::decode_message(uchar* buf, size_t len)
{
  data.insert(data.end(), buf, buf+len);
}
