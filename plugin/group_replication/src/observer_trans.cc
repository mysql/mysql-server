/* Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved.

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

#include "observer_trans.h"
#include "plugin_log.h"
#include <mysql/service_rpl_transaction_ctx.h>
#include <mysql/service_rpl_transaction_write_set.h>

/*
  Buffer to read the write_set value as a string. It will not exceed length
  of 22, since the length of uint32 is less than than 22 characters.
*/
#define BUFFER_READ_PKE 22

/*
  Internal auxiliary functions signatures.
*/
static bool reinit_cache(IO_CACHE *cache,
                         enum cache_type type,
                         my_off_t position);

void cleanup_transaction_write_set(Transaction_write_set *transaction_write_set)
{
  DBUG_ENTER("cleanup_write_set");
  if (transaction_write_set != NULL)
  {
    my_free (transaction_write_set->write_set);
    my_free (transaction_write_set);
  }
  DBUG_VOID_RETURN;
}

int add_write_set(Transaction_context_log_event *tcle,
                  Transaction_write_set *set)
{
  DBUG_ENTER("enter_write_set");
  int iterator= set->write_set_size;
  for (int i = 0; i < iterator; i++)
  {
    char buff[BUFFER_READ_PKE];
    const char *pke_field_value= my_safe_itoa(10, (longlong)set->write_set[i],
                                              &buff[sizeof(buff)-1]);
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

int gcs_trans_before_dml(Trans_param *param, int& out)
{
  DBUG_ENTER("gcs_trans_before_dml");

  out= 0;

  //If group replication has not started, then moving along...
  if (!is_gcs_rpl_running())
  {
    DBUG_RETURN(0);
  }

  /*
   The first check to be made is if the session binlog is active
   If it is not active, this query is not relevant for the plugin.
   */
  if(!param->trans_ctx_info.binlog_enabled)
  {
    DBUG_RETURN(0);
  }

  /*
   In runtime, check the global variables that can change.
   */
  if( (out+= (param->trans_ctx_info.binlog_format != BINLOG_FORMAT_ROW)) )
  {
    log_message(MY_ERROR_LEVEL, "Binlog format should be ROW for Group Replication");

    DBUG_RETURN(0);
  }

  if( (out+= (param->trans_ctx_info.binlog_checksum_options !=
                                                   binary_log::BINLOG_CHECKSUM_ALG_OFF)) )
  {
    log_message(MY_ERROR_LEVEL, "Binlog checksum should be OFF for Group Replication");

    DBUG_RETURN(0);
  }

  if ((out+= (param->trans_ctx_info.transaction_write_set_extraction !=
              HASH_ALGORITHM_MURMUR32)))
  {
    log_message(MY_ERROR_LEVEL, "transaction_write_set_extraction should be"
                " MURMUR32 for Group Replication");

    DBUG_RETURN(0);
  }
  /*
    Cycle through all involved tables to assess if they all
    comply with the runtime GCS requirements. For now:
    - The table must be from a transactional engine
    - It must contain at least one primary key
   */
  for(uint table=0; out == 0 && table < param->number_of_tables; table++)
  {
    if(!(param->tables_info[table].transactional_table))
    {
      log_message(MY_ERROR_LEVEL, "Table %s is not transactional. This is not compatible with Group Replication",
                  param->tables_info[table].table_name);
      out++;
    }

    if(param->tables_info[table].number_of_primary_keys == 0)
    {
      log_message(MY_ERROR_LEVEL, "Table %s does not have any PRIMARY KEY. This is not compatible with Group Replication",
                  param->tables_info[table].table_name);
      out++;
    }
  }

  DBUG_RETURN(0);
}

int gcs_trans_before_commit(Trans_param *param)
{
  DBUG_ENTER("gcs_trans_before_commit");
  int error= 0;

  DBUG_EXECUTE_IF("gcs_force_error_on_before_commit_listener",
                  DBUG_RETURN(1););

  if (!is_gcs_rpl_running())
    DBUG_RETURN(0);

  /*If the originating id belongs to a thread in the plugin, the transaction was already certified*/
  if (applier_module->is_own_event_channel(param->thread_id)
        || recovery_module->is_own_event_channel(param->thread_id))
    DBUG_RETURN(0);

  Cluster_member_info* for_local_status=
      cluster_member_mgr->get_cluster_member_info(*local_member_info->get_uuid());
  Cluster_member_info::Cluster_member_status node_status=
      for_local_status->get_recovery_status();
  delete for_local_status;

  if (node_status == Cluster_member_info::MEMBER_IN_RECOVERY)
  {
    log_message(MY_ERROR_LEVEL,
                "Transaction cannot be executed while Group Replication is recovering."
                " Try again when the server is ONLINE.");
    DBUG_RETURN(1);
  }

  if (node_status == Cluster_member_info::MEMBER_OFFLINE)
  {
    log_message(MY_ERROR_LEVEL,
                "Transaction cannot be executed while Group Replication is OFFLINE."
                " Check for errors and restart the plugin");
    DBUG_RETURN(1);
  }

  // Transaction information.
  const bool is_gtid_specified= param->gtid_info.type == GTID_GROUP;
  Gtid gtid= { param->gtid_info.sidno, param->gtid_info.gno };
  if (!is_gtid_specified)
  {
    // Dummy values that will be replaced after certification.
    gtid.sidno= 1;
    gtid.gno= 1;
  }

  const Gtid_specification gtid_specification= { GTID_GROUP, gtid };
  Gtid_log_event *gle= NULL;

  Transaction_context_log_event *tcle= NULL;
  // GCS cache.
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
    log_message(MY_ERROR_LEVEL, "We can only use one cache type at a "
                                "time on session %u", param->thread_id);
    error= 1;
    goto err;
  }

  DBUG_ASSERT(cache_log->type == WRITE_CACHE);
  DBUG_PRINT("cache_log", ("thread_id: %u, trx_cache_log_position: %llu,"
                           " stmt_cache_log_position: %llu",
                           param->thread_id, trx_cache_log_position,
                           stmt_cache_log_position));

  // Open GCS cache.
  if (open_cached_file(&cache, mysql_tmpdir, "gcs_trans_before_commit_cache",
                       param->cache_log_max_size, MYF(MY_WME)))
  {
    log_message(MY_ERROR_LEVEL, "Failed to create gcs commit cache "
                                "on session %u", param->thread_id);
    error= 1;
    goto err;
  }

  // Reinit binlog cache to read.
  if (reinit_cache(cache_log, READ_CACHE, 0))
  {
    log_message(MY_ERROR_LEVEL, "Failed to reinit binlog cache log for read "
                                "on session %u", param->thread_id);
    error= 1;
    goto err;
  }

  // Create transaction context.
  tcle= new Transaction_context_log_event(param->server_uuid,
                                          is_dml,
                                          param->thread_id,
                                          is_gtid_specified);

  if (is_dml)
  {
    Transaction_write_set* write_set= get_transaction_write_set(param->thread_id);
    /*
      When GTID is specified we may have empty transactions, that is,
      a transaction may have not write set at all because it didn't
      change any data, it will just persist that GTID as applied.
    */
    if ((write_set == NULL) && (!is_gtid_specified))
    {
      log_message(MY_ERROR_LEVEL, "Failed to extract the set of items written "
                                  "during the execution of the current "
                                  "transaction on session %u", param->thread_id);
      error= 1;
      goto err;
    }

    if (write_set != NULL)
    {
      if (add_write_set(tcle, write_set))
      {
        cleanup_transaction_write_set(write_set);
        log_message(MY_ERROR_LEVEL, "Failed to gather the set of items written "
                                    "during the execution of the current "
                                    "transaction on session %u", param->thread_id);
        error= 1;
        goto err;
      }
      cleanup_transaction_write_set(write_set);
      DBUG_ASSERT(is_gtid_specified || (tcle->get_write_set()->size() > 0));
    }
  }

  // Write transaction context to GCS cache.
  tcle->write(&cache);

  // Write Gtid log event to GCS cache.
  gle= new Gtid_log_event(param->server_id, is_dml, 0, 1, gtid_specification);
  gle->write(&cache);

  // Reinit GCS cache to read.
  if (reinit_cache(&cache, READ_CACHE, 0))
  {
    log_message(MY_ERROR_LEVEL, "Error while re-initializing an internal "
                                "cache, for read operations, on session %u",
                                param->thread_id);
    error= 1;
    goto err;
  }

  // Copy GCS cache to buffer.
  if (transaction_msg.append_cache(&cache))
  {
    log_message(MY_ERROR_LEVEL, "Error while appending data to an internal "
                                "cache on session %u", param->thread_id);
    error= 1;
    goto err;
  }

  // Copy binlog cache content to buffer.
  if (transaction_msg.append_cache(cache_log))
  {
    log_message(MY_ERROR_LEVEL, "Error while writing binary log cache on "
                                "session %u", param->thread_id);
    error= 1;
    goto err;
  }

  // Reinit binlog cache to write (revert what we did).
  if (reinit_cache(cache_log, WRITE_CACHE, cache_log_position))
  {
    log_message(MY_ERROR_LEVEL, "Error while re-initializing an internal "
                                "cache, for write operations, on session %u",
                                param->thread_id);
    error= 1;
    goto err;
  }

  if (certification_latch->registerTicket(param->thread_id))
  {
    log_message(MY_ERROR_LEVEL, "Unable to register for getting notifications "
                                "regarding the outcome of the transaction on "
                                "session %u", param->thread_id);
    error= 1;
    goto err;
  }

  DEBUG_SYNC_C("gcs_before_message_broadcast");

  //Broadcast the Transaction Message
  if (send_transaction_message(&transaction_msg))
  {
    log_message(MY_ERROR_LEVEL, "Error while broadcasting the transaction to "
                                "the group on session %u", param->thread_id);
    error= 1;
    goto err;
  }

  if (certification_latch->waitTicket(param->thread_id))
  {
    log_message(MY_ERROR_LEVEL, "Error while waiting for conflict detection "
                                "procedure to finish on session %u",
                                param->thread_id);
    error= 1;
    goto err;
  }

err:
  delete gle;
  delete tcle;
  close_cached_file(&cache);

  if (error)
  {
    // Release and remove certification latch ticket.
    certification_latch->releaseTicket(param->thread_id);
    certification_latch->waitTicket(param->thread_id);
  }

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

  gcs_trans_before_dml,
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
