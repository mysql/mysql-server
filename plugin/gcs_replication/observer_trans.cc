/* Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

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
#include "gcs_utils.h"
#include <gcs_protocol.h>
#include <gcs_protocol_factory.h>
#include <log_event.h>


/*
  Internal auxiliary functions signatures.
*/
static bool reinit_cache(IO_CACHE *cache,
                         enum cache_type type,
                         my_off_t position);

static bool copy_cache(MessageBuffer *dest, IO_CACHE *src);


/*
  Transaction lifecycle events observers.
*/
int gcs_trans_before_commit(Trans_param *param)
{
  DBUG_ENTER("gcs_trans_before_commit");
  int error= 0;

  bool is_real_trans= param->flags & TRANS_IS_REAL_TRANS;
  if (!is_real_trans)
    DBUG_RETURN(0);

  if (!is_gcs_rpl_running())
    DBUG_RETURN(0);

  // GCS cache.
  Transaction_context_log_event *tcle= NULL;
  rpl_gno snapshot_timestamp;
  IO_CACHE cache;
  MessageBuffer *buffer= new MessageBuffer();

  // GCS API.
  GCS::Protocol *protocol= GCS::Protocol_factory::get_instance();
  GCS::Message *message= NULL;

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
    log_message(MY_ERROR_LEVEL, "We can only use one cache type at a time");
    error= 1;
    goto err;
  }

  DBUG_ASSERT(cache_log->type == WRITE_CACHE);
  DBUG_PRINT("cache_log", ("thread_id: %lu, trx_cache_log_position: %llu,"
                           " stmt_cache_log_position: %llu",
                           param->thread_id, trx_cache_log_position,
                           stmt_cache_log_position));

  // Get transaction snapshot timestamp.
  snapshot_timestamp= get_last_gno_without_gaps(gcs_cluster_sidno);
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
    // TODO: WL#6834: add write set
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
  if (copy_cache(buffer, &cache))
  {
    log_message(MY_ERROR_LEVEL, "Failed while writing GCS cache to buffer");
    error= 1;
    goto err;
  }

  // Copy binlog cache content to buffer.
  if (copy_cache(buffer, cache_log))
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

  // Broadcast GCS message.
  message= new GCS::Message(GCS::MSG_REGULAR, GCS::MSGQOS_UNIFORM,
                                GCS::MSGORD_TOTAL_ORDER,
                                buffer->data(), buffer->length());
  if (protocol->broadcast(*message))
  {
    log_message(MY_ERROR_LEVEL, "Failed to broadcast GCS message");
    error= 1;
    goto err;
  }

  // TODO: WL#6826: wait for certification decision

err:
  delete tcle;
  delete buffer;
  delete message;
  close_cached_file(&cache);

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

/*
  Copy one cache content to a buffer.

  @param[in] dest  buffer to where data will be written
  @param[in] src   cache from which data will be read
*/
static bool copy_cache(MessageBuffer *dest, IO_CACHE *src)
{
  DBUG_ENTER("copy_cache");
  size_t length;

  DBUG_ASSERT(src->type == READ_CACHE);

  while ((length= my_b_fill(src)) > 0)
  {
    if (src->error)
      DBUG_RETURN(true);

    dest->append(src->read_pos, length);
  }

  DBUG_RETURN(false);
}
