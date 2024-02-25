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

#include "plugin/semisync/semisync_replica.h"

#include <assert.h>
#include <sys/types.h>

#include "my_byteorder.h"
#include "my_dbug.h"
#include "mysql.h"
#include "sql/current_thd.h"
#include "sql/debug_sync.h"

bool rpl_semi_sync_replica_enabled;
char rpl_semi_sync_replica_status = 0;
unsigned long rpl_semi_sync_replica_trace_level;

int ReplSemiSyncSlave::initObject() {
  int result = 0;
  const char *kWho = "ReplSemiSyncSlave::initObject";

  if (init_done_) {
    LogErr(WARNING_LEVEL, ER_SEMISYNC_FUNCTION_CALLED_TWICE, kWho);
    return 1;
  }
  init_done_ = true;

  /* References to the parameter works after set_options(). */
  setSlaveEnabled(rpl_semi_sync_replica_enabled);
  setTraceLevel(rpl_semi_sync_replica_trace_level);

  return result;
}

int ReplSemiSyncSlave::slaveReadSyncHeader(const char *header,
                                           unsigned long total_len,
                                           bool *need_reply,
                                           const char **payload,
                                           unsigned long *payload_len) {
  const char *kWho = "ReplSemiSyncSlave::slaveReadSyncHeader";
  int read_res = 0;
  function_enter(kWho);

  if ((unsigned char)(header[0]) == kPacketMagicNum) {
    *need_reply = (header[1] & kPacketFlagSync);
    *payload_len = total_len - 2;
    *payload = header + 2;

    if (trace_level_ & kTraceDetail)
      LogErr(INFORMATION_LEVEL, ER_SEMISYNC_REPLICA_REPLY, kWho, *need_reply);
  } else {
    LogErr(ERROR_LEVEL, ER_SEMISYNC_MISSING_MAGIC_NO_FOR_SEMISYNC_PKT,
           total_len);
    read_res = -1;
  }

  return function_exit(kWho, read_res);
}

int ReplSemiSyncSlave::slaveStart(Binlog_relay_IO_param *param) {
  bool semi_sync = getSlaveEnabled();

  LogErr(INFORMATION_LEVEL, ER_SEMISYNC_REPLICA_START,
         semi_sync ? "semi-sync" : "asynchronous", param->user, param->host,
         param->port,
         param->master_log_name[0] ? param->master_log_name : "FIRST",
         (unsigned long)param->master_log_pos);

  if (semi_sync && !rpl_semi_sync_replica_status)
    rpl_semi_sync_replica_status = 1;
  return 0;
}

int ReplSemiSyncSlave::slaveStop(Binlog_relay_IO_param *) {
  if (rpl_semi_sync_replica_status) rpl_semi_sync_replica_status = 0;
  if (mysql_reply) mysql_close(mysql_reply);
  mysql_reply = nullptr;
  return 0;
}

int ReplSemiSyncSlave::slaveReply(MYSQL *mysql, const char *binlog_filename,
                                  my_off_t binlog_filepos) {
  const char *kWho = "ReplSemiSyncSlave::slaveReply";
  NET *net = &mysql->net;
  uchar reply_buffer[REPLY_MAGIC_NUM_LEN + REPLY_BINLOG_POS_LEN +
                     REPLY_BINLOG_NAME_LEN];
  int reply_res;
  size_t name_len = strlen(binlog_filename);

  function_enter(kWho);

  DBUG_EXECUTE_IF("rpl_semisync_before_send_ack", {
    const char act[] = "now SIGNAL sending_ack WAIT_FOR continue";
    assert(opt_debug_sync_timeout > 0);
    assert(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
  };);

  /* Prepare the buffer of the reply. */
  reply_buffer[REPLY_MAGIC_NUM_OFFSET] = kPacketMagicNum;
  int8store(reply_buffer + REPLY_BINLOG_POS_OFFSET, binlog_filepos);
  memcpy(reply_buffer + REPLY_BINLOG_NAME_OFFSET, binlog_filename,
         name_len + 1 /* including trailing '\0' */);

  if (trace_level_ & kTraceDetail)
    LogErr(INFORMATION_LEVEL, ER_SEMISYNC_REPLICA_REPLY_WITH_BINLOG_INFO, kWho,
           binlog_filename, (ulong)binlog_filepos);

  net_clear(net, false);
  /* Send the reply. */
  reply_res =
      my_net_write(net, reply_buffer, name_len + REPLY_BINLOG_NAME_OFFSET);
  if (!reply_res) {
    reply_res = net_flush(net);
    if (reply_res)
      LogErr(ERROR_LEVEL, ER_SEMISYNC_REPLICA_NET_FLUSH_REPLY_FAILED);
  } else {
    LogErr(ERROR_LEVEL, ER_SEMISYNC_REPLICA_SEND_REPLY_FAILED, net->last_error,
           net->last_errno);
  }

  /*
    The progress of the internal state of the NET object differs a bit between
    compressed and non-compressed protocol. For compressed protocol, it is
    necessary to call net_clear when switching between reading and writing.
    For non-compressed protocol, it does not work when we call net_clear here
  */
  if (net->compress) net_clear(net, false);
  return function_exit(kWho, reply_res);
}
