/* Copyright (c) 2008 MySQL AB, 2009 Sun Microsystems, Inc.
   Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */


#include "semisync_slave.h"

char rpl_semi_sync_slave_enabled;
char rpl_semi_sync_slave_status= 0;
unsigned long rpl_semi_sync_slave_trace_level;

int ReplSemiSyncSlave::initObject()
{
  int result= 0;
  const char *kWho = "ReplSemiSyncSlave::initObject";

  if (init_done_)
  {
    fprintf(stderr, "%s called twice\n", kWho);
    return 1;
  }
  init_done_ = true;

  /* References to the parameter works after set_options(). */
  setSlaveEnabled(rpl_semi_sync_slave_enabled);
  setTraceLevel(rpl_semi_sync_slave_trace_level);

  return result;
}

int ReplSemiSyncSlave::slaveReadSyncHeader(const char *header,
                                      unsigned long total_len,
                                      bool  *need_reply,
                                      const char **payload,
                                      unsigned long *payload_len)
{
  const char *kWho = "ReplSemiSyncSlave::slaveReadSyncHeader";
  int read_res = 0;
  function_enter(kWho);

  if ((unsigned char)(header[0]) == kPacketMagicNum)
  {
    *need_reply  = (header[1] & kPacketFlagSync);
    *payload_len = total_len - 2;
    *payload     = header + 2;

    if (trace_level_ & kTraceDetail)
      sql_print_information("%s: reply - %d", kWho, *need_reply);
  }
  else
  {
    sql_print_error("Missing magic number for semi-sync packet, packet "
                    "len: %lu", total_len);
    read_res = -1;
  }

  return function_exit(kWho, read_res);
}

int ReplSemiSyncSlave::slaveStart(Binlog_relay_IO_param *param)
{
  bool semi_sync= getSlaveEnabled();
  
  sql_print_information("Slave I/O thread: Start %s replication to\
 master '%s@%s:%d' in log '%s' at position %lu",
			semi_sync ? "semi-sync" : "asynchronous",
			param->user, param->host, param->port,
			param->master_log_name[0] ? param->master_log_name : "FIRST",
			(unsigned long)param->master_log_pos);

  if (semi_sync && !rpl_semi_sync_slave_status)
    rpl_semi_sync_slave_status= 1;
  return 0;
}

int ReplSemiSyncSlave::slaveStop(Binlog_relay_IO_param *param)
{
  if (rpl_semi_sync_slave_status)
    rpl_semi_sync_slave_status= 0;
  if (mysql_reply)
    mysql_close(mysql_reply);
  mysql_reply= 0;
  return 0;
}

int ReplSemiSyncSlave::slaveReply(MYSQL *mysql,
                                 const char *binlog_filename,
                                 my_off_t binlog_filepos)
{
  const char *kWho = "ReplSemiSyncSlave::slaveReply";
  NET *net= &mysql->net;
  uchar reply_buffer[REPLY_MAGIC_NUM_LEN
                     + REPLY_BINLOG_POS_LEN
                     + REPLY_BINLOG_NAME_LEN];
  int  reply_res, name_len = strlen(binlog_filename);

  function_enter(kWho);

  /* Prepare the buffer of the reply. */
  reply_buffer[REPLY_MAGIC_NUM_OFFSET] = kPacketMagicNum;
  int8store(reply_buffer + REPLY_BINLOG_POS_OFFSET, binlog_filepos);
  memcpy(reply_buffer + REPLY_BINLOG_NAME_OFFSET,
         binlog_filename,
         name_len + 1 /* including trailing '\0' */);

  if (trace_level_ & kTraceDetail)
    sql_print_information("%s: reply (%s, %lu)", kWho,
                          binlog_filename, (ulong)binlog_filepos);

  net_clear(net, 0);
  /* Send the reply. */
  reply_res = my_net_write(net, reply_buffer,
                           name_len + REPLY_BINLOG_NAME_OFFSET);
  if (!reply_res)
  {
    reply_res = net_flush(net);
    if (reply_res)
      sql_print_error("Semi-sync slave net_flush() reply failed");
  }
  else
  {
    sql_print_error("Semi-sync slave send reply failed: %s (%d)",
                    net->last_error, net->last_errno);
  }

  return function_exit(kWho, reply_res);
}
