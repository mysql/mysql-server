/* Copyright (C) 2007 Google Inc.
   Copyright (c) 2008, 2023, Oracle and/or its affiliates.

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

#include "plugin/semisync/semisync_source.h"

#include <assert.h>
#include <time.h>

#include "my_byteorder.h"
#include "my_compiler.h"
#include "my_systime.h"
#include "sql/mysqld.h"  // max_connections
#if defined(ENABLED_DEBUG_SYNC)
#include "sql/current_thd.h"
#include "sql/debug_sync.h"
#include "sql/sql_class.h"
#endif

#define TIME_THOUSAND 1000
#define TIME_MILLION 1000000
#define TIME_BILLION 1000000000

/* This indicates whether semi-synchronous replication is enabled. */
bool rpl_semi_sync_source_enabled;
unsigned long rpl_semi_sync_source_timeout;
unsigned long rpl_semi_sync_source_trace_level;
char rpl_semi_sync_source_status = 0;
unsigned long rpl_semi_sync_source_yes_transactions = 0;
unsigned long rpl_semi_sync_source_no_transactions = 0;
unsigned long rpl_semi_sync_source_off_times = 0;
unsigned long rpl_semi_sync_source_timefunc_fails = 0;
unsigned long rpl_semi_sync_source_wait_timeouts = 0;
unsigned long rpl_semi_sync_source_wait_sessions = 0;
unsigned long rpl_semi_sync_source_wait_pos_backtraverse = 0;
unsigned long rpl_semi_sync_source_avg_trx_wait_time = 0;
unsigned long long rpl_semi_sync_source_trx_wait_num = 0;
unsigned long rpl_semi_sync_source_avg_net_wait_time = 0;
unsigned long long rpl_semi_sync_source_net_wait_num = 0;
unsigned long rpl_semi_sync_source_clients = 0;
unsigned long long rpl_semi_sync_source_net_wait_time = 0;
unsigned long long rpl_semi_sync_source_trx_wait_time = 0;
bool rpl_semi_sync_source_wait_no_replica = true;
unsigned int rpl_semi_sync_source_wait_for_replica_count = 1;

static int getWaitTime(const struct timespec &start_ts);

static unsigned long long timespec_to_usec(const struct timespec *ts) {
  return (unsigned long long)ts->tv_sec * TIME_MILLION +
         ts->tv_nsec / TIME_THOUSAND;
}

/*******************************************************************************
 *
 * <ActiveTranx> class : manage all active transaction nodes
 *
 ******************************************************************************/

ActiveTranx::ActiveTranx(mysql_mutex_t *lock, unsigned long trace_level)
    : Trace(trace_level),
      allocator_(max_connections),
      num_entries_(max_connections << 1), /* Transaction hash table size
                                           * is set to double the size
                                           * of max_connections */
      lock_(lock) {
  /* No transactions are in the list initially. */
  trx_front_ = nullptr;
  trx_rear_ = nullptr;

  /* Create the hash table to find a transaction's ending event. */
  trx_htb_ = new TranxNode *[num_entries_];
  for (int idx = 0; idx < num_entries_; ++idx) trx_htb_[idx] = nullptr;

  LogErr(INFORMATION_LEVEL, ER_SEMISYNC_RPL_INIT_FOR_TRX);
}

ActiveTranx::~ActiveTranx() {
  delete[] trx_htb_;
  trx_htb_ = nullptr;
  num_entries_ = 0;
}

unsigned int ActiveTranx::calc_hash(const unsigned char *key,
                                    unsigned int length) {
  unsigned int nr = 1, nr2 = 4;

  /* The hash implementation comes from calc_hashnr() in mysys/hash.c. */
  while (length--) {
    nr ^=
        (((nr & 63) + nr2) * ((unsigned int)(unsigned char)*key++)) + (nr << 8);
    nr2 += 3;
  }
  return ((unsigned int)nr);
}

unsigned int ActiveTranx::get_hash_value(const char *log_file_name,
                                         my_off_t log_file_pos) {
  unsigned int hash1 =
      calc_hash((const unsigned char *)log_file_name, strlen(log_file_name));
  unsigned int hash2 =
      calc_hash((const unsigned char *)(&log_file_pos), sizeof(log_file_pos));

  return (hash1 + hash2) % num_entries_;
}

int ActiveTranx::compare(const char *log_file_name1, my_off_t log_file_pos1,
                         const char *log_file_name2, my_off_t log_file_pos2) {
  int cmp = strcmp(log_file_name1, log_file_name2);

  if (cmp != 0) return cmp;

  if (log_file_pos1 > log_file_pos2)
    return 1;
  else if (log_file_pos1 < log_file_pos2)
    return -1;
  return 0;
}

int ActiveTranx::insert_tranx_node(const char *log_file_name,
                                   my_off_t log_file_pos) {
  const char *kWho = "ActiveTranx:insert_tranx_node";
  TranxNode *ins_node;
  int result = 0;
  unsigned int hash_val;

  function_enter(kWho);

  ins_node = allocator_.allocate_node();
  if (!ins_node) {
    LogErr(ERROR_LEVEL, ER_SEMISYNC_FAILED_TO_ALLOCATE_TRX_NODE, kWho,
           log_file_name, (unsigned long)log_file_pos);
    result = -1;
    goto l_end;
  }

  /* insert the binlog position in the active transaction list. */
  strncpy(ins_node->log_name_, log_file_name, FN_REFLEN - 1);
  ins_node->log_name_[FN_REFLEN - 1] = 0; /* make sure it ends properly */
  ins_node->log_pos_ = log_file_pos;

  if (!trx_front_) {
    /* The list is empty. */
    trx_front_ = trx_rear_ = ins_node;
  } else {
    int cmp = compare(ins_node, trx_rear_);
    if (cmp > 0) {
      /* Compare with the tail first.  If the transaction happens later in
       * binlog, then make it the new tail.
       */
      trx_rear_->next_ = ins_node;
      trx_rear_ = ins_node;
    } else {
      /* Otherwise, it is an error because the transaction should hold the
       * mysql_bin_log.LOCK_log when appending events.
       */
      LogErr(ERROR_LEVEL, ER_SEMISYNC_BINLOG_WRITE_OUT_OF_ORDER, kWho,
             trx_rear_->log_name_, (unsigned long)trx_rear_->log_pos_,
             ins_node->log_name_, (unsigned long)ins_node->log_pos_);
      result = -1;
      goto l_end;
    }
  }

  hash_val = get_hash_value(ins_node->log_name_, ins_node->log_pos_);
  ins_node->hash_next_ = trx_htb_[hash_val];
  trx_htb_[hash_val] = ins_node;

  if (trace_level_ & kTraceDetail)
    LogErr(INFORMATION_LEVEL, ER_SEMISYNC_INSERT_LOG_INFO_IN_ENTRY, kWho,
           ins_node->log_name_, (unsigned long)ins_node->log_pos_, hash_val);

l_end:
  return function_exit(kWho, result);
}

bool ActiveTranx::is_tranx_end_pos(const char *log_file_name,
                                   my_off_t log_file_pos) {
  const char *kWho = "ActiveTranx::is_tranx_end_pos";
  function_enter(kWho);

  unsigned int hash_val = get_hash_value(log_file_name, log_file_pos);
  TranxNode *entry = trx_htb_[hash_val];

  while (entry != nullptr) {
    if (compare(entry, log_file_name, log_file_pos) == 0) break;

    entry = entry->hash_next_;
  }

  if (trace_level_ & kTraceDetail)
    LogErr(INFORMATION_LEVEL, ER_SEMISYNC_PROBE_LOG_INFO_IN_ENTRY, kWho,
           log_file_name, (unsigned long)log_file_pos, hash_val);

  function_exit(kWho, (entry != nullptr));
  return (entry != nullptr);
}

int ActiveTranx::signal_waiting_sessions_all() {
  const char *kWho = "ActiveTranx::signal_waiting_sessions_all";
  function_enter(kWho);
  for (TranxNode *entry = trx_front_; entry; entry = entry->next_)
    mysql_cond_broadcast(&entry->cond);

  return function_exit(kWho, 0);
}

int ActiveTranx::signal_waiting_sessions_up_to(const char *log_file_name,
                                               my_off_t log_file_pos) {
  const char *kWho = "ActiveTranx::signal_waiting_sessions_up_to";
  function_enter(kWho);

  TranxNode *entry = trx_front_;
  int cmp = ActiveTranx::compare(entry->log_name_, entry->log_pos_,
                                 log_file_name, log_file_pos);
  while (entry && cmp <= 0) {
    mysql_cond_broadcast(&entry->cond);
    entry = entry->next_;
    if (entry)
      cmp = ActiveTranx::compare(entry->log_name_, entry->log_pos_,
                                 log_file_name, log_file_pos);
  }

  return function_exit(kWho, (entry != nullptr));
}

TranxNode *ActiveTranx::find_active_tranx_node(const char *log_file_name,
                                               my_off_t log_file_pos) {
  const char *kWho = "ActiveTranx::find_active_tranx_node";
  function_enter(kWho);

  TranxNode *entry = trx_front_;

  while (entry) {
    if (ActiveTranx::compare(log_file_name, log_file_pos, entry->log_name_,
                             entry->log_pos_) <= 0)
      break;
    entry = entry->next_;
  }
  function_exit(kWho, 0);
  return entry;
}

int ActiveTranx::clear_active_tranx_nodes(const char *log_file_name,
                                          my_off_t log_file_pos) {
  const char *kWho = "ActiveTranx::::clear_active_tranx_nodes";
  TranxNode *new_front;

  function_enter(kWho);

  if (log_file_name != nullptr) {
    new_front = trx_front_;

    while (new_front) {
      if (compare(new_front, log_file_name, log_file_pos) > 0 ||
          new_front->n_waiters > 0)
        break;
      new_front = new_front->next_;
    }
  } else {
    /* If log_file_name is NULL, clear everything. */
    new_front = nullptr;
  }

  if (new_front == nullptr) {
    /* No active transaction nodes after the call. */

    /* Clear the hash table. */
    memset(trx_htb_, 0, num_entries_ * sizeof(TranxNode *));
    allocator_.free_all_nodes();

    /* Clear the active transaction list. */
    if (trx_front_ != nullptr) {
      trx_front_ = nullptr;
      trx_rear_ = nullptr;
    }

    if (trace_level_ & kTraceDetail)
      LogErr(INFORMATION_LEVEL,
             ER_SEMISYNC_CLEARED_ALL_ACTIVE_TRANSACTION_NODES, kWho);
  } else if (new_front != trx_front_) {
    TranxNode *curr_node, *next_node;

    /* Delete all transaction nodes before the confirmation point. */
    int n_frees = 0;
    curr_node = trx_front_;
    while (curr_node != new_front) {
      next_node = curr_node->next_;
      n_frees++;

      /* Remove the node from the hash table. */
      unsigned int hash_val =
          get_hash_value(curr_node->log_name_, curr_node->log_pos_);
      TranxNode **hash_ptr = &(trx_htb_[hash_val]);
      while ((*hash_ptr) != nullptr) {
        if ((*hash_ptr) == curr_node) {
          (*hash_ptr) = curr_node->hash_next_;
          break;
        }
        hash_ptr = &((*hash_ptr)->hash_next_);
      }

      curr_node = next_node;
    }

    trx_front_ = new_front;
    allocator_.free_nodes_before(trx_front_);

    if (trace_level_ & kTraceDetail)
      LogErr(INFORMATION_LEVEL, ER_SEMISYNC_CLEARED_ACTIVE_TRANSACTION_TILL_POS,
             kWho, n_frees, trx_front_->log_name_,
             (unsigned long)trx_front_->log_pos_);
  }

  return function_exit(kWho, 0);
}

int ReplSemiSyncMaster::reportReplyPacket(uint32 server_id, const uchar *packet,
                                          ulong packet_len) {
  const char *kWho = "ReplSemiSyncMaster::reportReplyPacket";
  int result = -1;
  char log_file_name[FN_REFLEN + 1];
  my_off_t log_file_pos;
  ulong log_file_len = 0;

  function_enter(kWho);

  if (unlikely(packet[REPLY_MAGIC_NUM_OFFSET] !=
               ReplSemiSyncMaster::kPacketMagicNum)) {
    LogErr(ERROR_LEVEL, ER_SEMISYNC_REPLY_MAGIC_NO_ERROR);
    goto l_end;
  }

  if (unlikely(packet_len < REPLY_BINLOG_NAME_OFFSET)) {
    LogErr(ERROR_LEVEL, ER_SEMISYNC_REPLY_PKT_LENGTH_TOO_SMALL);
    goto l_end;
  }

  log_file_pos = uint8korr(packet + REPLY_BINLOG_POS_OFFSET);
  log_file_len = packet_len - REPLY_BINLOG_NAME_OFFSET;
  if (unlikely(log_file_len >= FN_REFLEN)) {
    LogErr(ERROR_LEVEL, ER_SEMISYNC_REPLY_BINLOG_FILE_TOO_LARGE);
    goto l_end;
  }
  strncpy(log_file_name, (const char *)packet + REPLY_BINLOG_NAME_OFFSET,
          log_file_len);
  log_file_name[log_file_len] = 0;

  if (trace_level_ & kTraceDetail)
    LogErr(INFORMATION_LEVEL, ER_SEMISYNC_SERVER_REPLY, kWho, log_file_name,
           (ulong)log_file_pos, server_id);

  handleAck(server_id, log_file_name, log_file_pos);

l_end:
  return function_exit(kWho, result);
}

/*******************************************************************************
 *
 * <ReplSemiSyncMaster> class: the basic code layer for sync-replication master.
 * <ReplSemiSyncSlave>  class: the basic code layer for sync-replication slave.
 *
 * The most important functions during semi-syn replication listed:
 *
 * Master:
 *  . reportReplyBinlog():  called by the binlog dump thread when it receives
 *                          the slave's status information.
 *  . updateSyncHeader():   based on transaction waiting information, decide
 *                          whether to request the slave to reply.
 *  . writeTranxInBinlog(): called by the transaction thread when it finishes
 *                          writing all transaction events in binlog.
 *  . commitTrx():          transaction thread wait for the slave reply.
 *
 * Slave:
 *  . slaveReadSyncHeader(): read the semi-sync header from the master, get the
 *                           sync status and get the payload for events.
 *  . slaveReply():          reply to the master about the replication progress.
 *
 ******************************************************************************/

ReplSemiSyncMaster::ReplSemiSyncMaster() {
  reply_file_name_[0] = '\0';
  wait_file_name_[0] = '\0';
  commit_file_name_[0] = '\0';
}

int ReplSemiSyncMaster::initObject() {
  int result;
  const char *kWho = "ReplSemiSyncMaster::initObject";

  if (init_done_) {
    LogErr(WARNING_LEVEL, ER_SEMISYNC_FUNCTION_CALLED_TWICE, kWho);
    return 1;
  }
  init_done_ = true;

  /* References to the parameter works after set_options(). */
  setWaitTimeout(rpl_semi_sync_source_timeout);
  setTraceLevel(rpl_semi_sync_source_trace_level);

  /* Mutex initialization can only be done after MY_INIT(). */
  mysql_mutex_init(key_ss_mutex_LOCK_binlog_, &LOCK_binlog_,
                   MY_MUTEX_INIT_FAST);

  /*
    rpl_semi_sync_source_wait_for_replica_count may be set through mysqld
    option. So call setWaitSlaveCount to initialize the internal ack container.
  */
  if (setWaitSlaveCount(rpl_semi_sync_source_wait_for_replica_count)) return 1;

  if (rpl_semi_sync_source_enabled)
    result = enableMaster();
  else
    result = disableMaster();

  return result;
}

int ReplSemiSyncMaster::enableMaster() {
  int result = 0;

  /* Must have the lock when we do enable of disable. */
  lock();

  if (!getMasterEnabled()) {
    if (active_tranxs_ == nullptr)
      active_tranxs_ = new ActiveTranx(&LOCK_binlog_, trace_level_);

    if (active_tranxs_ != nullptr) {
      commit_file_name_inited_ = false;
      reply_file_name_inited_ = false;
      wait_file_name_inited_ = false;

      set_master_enabled(true);
      /*
        state_ will be set off when users don't want to wait(
        rpl_semi_sync_source_wait_no_replica == 0) if there is no enough active
        semisync clients
      */
      state_ = (rpl_semi_sync_source_wait_no_replica != 0 ||
                (rpl_semi_sync_source_clients >=
                 rpl_semi_sync_source_wait_for_replica_count));
      LogErr(INFORMATION_LEVEL, ER_SEMISYNC_RPL_ENABLED_ON_SOURCE);
    } else {
      LogErr(ERROR_LEVEL, ER_SEMISYNC_SOURCE_OOM);
      result = -1;
    }
  }

  unlock();

  return result;
}

int ReplSemiSyncMaster::disableMaster() {
  /* Must have the lock when we do enable of disable. */
  lock();

  if (getMasterEnabled()) {
    /* Switch off the semi-sync first so that waiting transaction will be
     * waken up.
     */
    switch_off();

    if (active_tranxs_ && active_tranxs_->is_empty()) {
      delete active_tranxs_;
      active_tranxs_ = nullptr;
    }

    reply_file_name_inited_ = false;
    wait_file_name_inited_ = false;
    commit_file_name_inited_ = false;

    ack_container_.clear();

    set_master_enabled(false);
    LogErr(INFORMATION_LEVEL, ER_SEMISYNC_DISABLED_ON_SOURCE);
  }

  unlock();

  return 0;
}

ReplSemiSyncMaster::~ReplSemiSyncMaster() {
  if (init_done_) {
    mysql_mutex_destroy(&LOCK_binlog_);
  }

  delete active_tranxs_;
}

void ReplSemiSyncMaster::lock() { mysql_mutex_lock(&LOCK_binlog_); }

void ReplSemiSyncMaster::unlock() { mysql_mutex_unlock(&LOCK_binlog_); }

void ReplSemiSyncMaster::add_slave() {
  lock();
  rpl_semi_sync_source_clients++;
  unlock();
}

void ReplSemiSyncMaster::remove_slave() {
  lock();
  rpl_semi_sync_source_clients--;

  /* Only switch off if semi-sync is enabled and is on */
  if (getMasterEnabled() && is_on()) {
    /*
      If user has chosen not to wait if no enough semi-sync slave available
      and after a slave exists, turn off semi-semi master immediately if active
      slaves are less then required slave numbers.
    */
    if ((rpl_semi_sync_source_clients ==
         rpl_semi_sync_source_wait_for_replica_count - 1) &&
        (!rpl_semi_sync_source_wait_no_replica ||
         connection_events_loop_aborted())) {
      if (connection_events_loop_aborted()) {
        if (commit_file_name_inited_ && reply_file_name_inited_) {
          int cmp = ActiveTranx::compare(reply_file_name_, reply_file_pos_,
                                         commit_file_name_, commit_file_pos_);
          if (cmp < 0) LogErr(WARNING_LEVEL, ER_SEMISYNC_FORCED_SHUTDOWN);
        }
      }
      switch_off();
    }
  }
  unlock();
}

bool ReplSemiSyncMaster::is_semi_sync_slave() {
  int null_value;
  long long val = 0;
  get_user_var_int("rpl_semi_sync_replica", &val, &null_value);
  return val;
}

void ReplSemiSyncMaster::reportReplyBinlog(const char *log_file_name,
                                           my_off_t log_file_pos) {
  const char *kWho = "ReplSemiSyncMaster::reportReplyBinlog";
  int cmp;
  bool can_release_threads = false;
  bool need_copy_send_pos = true;

  function_enter(kWho);
  mysql_mutex_assert_owner(&LOCK_binlog_);

  if (!getMasterEnabled()) goto l_end;

  if (!is_on()) /* We check to see whether we can switch semi-sync ON. */
    try_switch_on(log_file_name, log_file_pos);

  /* The position should increase monotonically, if there is only one
   * thread sending the binlog to the slave.
   * In reality, to improve the transaction availability, we allow multiple
   * sync replication slaves.  So, if any one of them get the transaction,
   * the transaction session in the primary can move forward.
   */
  if (reply_file_name_inited_) {
    cmp = ActiveTranx::compare(log_file_name, log_file_pos, reply_file_name_,
                               reply_file_pos_);

    /* If the requested position is behind the sending binlog position,
     * would not adjust sending binlog position.
     * We based on the assumption that there are multiple semi-sync slave,
     * and at least one of them shou/ld be up to date.
     * If all semi-sync slaves are behind, at least initially, the primary
     * can find the situation after the waiting timeout.  After that, some
     * slaves should catch up quickly.
     */
    if (cmp < 0) {
      /* If the position is behind, do not copy it. */
      need_copy_send_pos = false;
    }
  }

  if (need_copy_send_pos) {
    strncpy(reply_file_name_, log_file_name, sizeof(reply_file_name_) - 1);
    reply_file_name_[sizeof(reply_file_name_) - 1] = '\0';
    reply_file_pos_ = log_file_pos;
    reply_file_name_inited_ = true;

    if (trace_level_ & kTraceDetail)
      LogErr(INFORMATION_LEVEL, ER_SEMISYNC_SOURCE_GOT_REPLY_AT_POS, kWho,
             log_file_name, (unsigned long)log_file_pos);
  }

  if (rpl_semi_sync_source_wait_sessions > 0) {
    /* Let us check if some of the waiting threads doing a trx
     * commit can now proceed.
     */
    cmp = ActiveTranx::compare(reply_file_name_, reply_file_pos_,
                               wait_file_name_, wait_file_pos_);
    if (cmp >= 0) {
      /* Yes, at least one waiting thread can now proceed:
       * let us release all waiting threads with a broadcast
       */
      can_release_threads = true;
      wait_file_name_inited_ = false;
    }
  }

l_end:

  if (can_release_threads) {
    if (trace_level_ & kTraceDetail)
      LogErr(INFORMATION_LEVEL, ER_SEMISYNC_SOURCE_SIGNAL_ALL_WAITING_THREADS,
             kWho);
    active_tranxs_->signal_waiting_sessions_up_to(reply_file_name_,
                                                  reply_file_pos_);
  }

  function_exit(kWho, 0);
}

int ReplSemiSyncMaster::commitTrx(const char *trx_wait_binlog_name,
                                  my_off_t trx_wait_binlog_pos) {
  const char *kWho = "ReplSemiSyncMaster::commitTrx";

  function_enter(kWho);
  PSI_stage_info old_stage;

#if defined(ENABLED_DEBUG_SYNC)
  /* debug sync may not be initialized for a master */
  if (current_thd->debug_sync_control)
    DEBUG_SYNC(current_thd, "rpl_semisync_source_commit_trx_before_lock");
#endif
  /* Acquire the mutex. */
  lock();

  TranxNode *entry = nullptr;
  mysql_cond_t *thd_cond = nullptr;
  bool is_semi_sync_trans = true;
  if (active_tranxs_ != nullptr && trx_wait_binlog_name) {
    entry = active_tranxs_->find_active_tranx_node(trx_wait_binlog_name,
                                                   trx_wait_binlog_pos);
    if (entry) thd_cond = &entry->cond;
  }
  /* This must be called after acquired the lock */
  THD_ENTER_COND(nullptr, thd_cond, &LOCK_binlog_,
                 &stage_waiting_for_semi_sync_ack_from_replica, &old_stage);

  if (getMasterEnabled() && trx_wait_binlog_name) {
    struct timespec start_ts;
    struct timespec abstime;
    int wait_result;

    set_timespec(&start_ts, 0);
    /* This is the real check inside the mutex. */
    if (!getMasterEnabled() || !is_on()) goto l_end;

    if (trace_level_ & kTraceDetail) {
      LogErr(INFORMATION_LEVEL, ER_SEMISYNC_SOURCE_TRX_WAIT_POS, kWho,
             trx_wait_binlog_name, (unsigned long)trx_wait_binlog_pos,
             (int)is_on());
    }

    /* Calculate the waiting period. */
    abstime.tv_sec = start_ts.tv_sec + wait_timeout_ / TIME_THOUSAND;
    abstime.tv_nsec =
        start_ts.tv_nsec + (wait_timeout_ % TIME_THOUSAND) * TIME_MILLION;
    if (abstime.tv_nsec >= TIME_BILLION) {
      abstime.tv_sec++;
      abstime.tv_nsec -= TIME_BILLION;
    }

    while (is_on()) {
      if (reply_file_name_inited_) {
        int cmp =
            ActiveTranx::compare(reply_file_name_, reply_file_pos_,
                                 trx_wait_binlog_name, trx_wait_binlog_pos);
        if (cmp >= 0) {
          /* We have already sent the relevant binlog to the slave: no need to
           * wait here.
           */
          if (trace_level_ & kTraceDetail)
            LogErr(INFORMATION_LEVEL, ER_SEMISYNC_BINLOG_REPLY_IS_AHEAD, kWho,
                   reply_file_name_, (unsigned long)reply_file_pos_);
          break;
        }
      }
      /*
        When code reaches here an Entry object may not be present in the
        following scenario.

        Semi sync was not enabled when transaction entered into ordered_commit
        process. During flush stage, semi sync was not enabled and there was no
        'Entry' object created for the transaction being committed and at a
        later stage it was enabled. In this case trx_wait_binlog_name and
        trx_wait_binlog_pos are set but the 'Entry' object is not present. Hence
        dump thread will not wait for reply from slave and it will not update
        reply_file_name. In such case the committing transaction should not wait
        for an ack from slave and it should be considered as an async
        transaction.
      */
      if (!entry) {
        is_semi_sync_trans = false;
        goto l_end;
      }

      /* Let us update the info about the minimum binlog position of waiting
       * threads.
       */
      if (wait_file_name_inited_) {
        int cmp =
            ActiveTranx::compare(trx_wait_binlog_name, trx_wait_binlog_pos,
                                 wait_file_name_, wait_file_pos_);
        if (cmp <= 0) {
          /* This thd has a lower position, let's update the minimum info. */
          strncpy(wait_file_name_, trx_wait_binlog_name,
                  sizeof(wait_file_name_) - 1);
          wait_file_name_[sizeof(wait_file_name_) - 1] = '\0';
          wait_file_pos_ = trx_wait_binlog_pos;

          rpl_semi_sync_source_wait_pos_backtraverse++;
          if (trace_level_ & kTraceDetail)
            LogErr(INFORMATION_LEVEL, ER_SEMISYNC_MOVE_BACK_WAIT_POS, kWho,
                   wait_file_name_, (unsigned long)wait_file_pos_);
        }
      } else {
        strncpy(wait_file_name_, trx_wait_binlog_name,
                sizeof(wait_file_name_) - 1);
        wait_file_name_[sizeof(wait_file_name_) - 1] = '\0';
        wait_file_pos_ = trx_wait_binlog_pos;
        wait_file_name_inited_ = true;

        if (trace_level_ & kTraceDetail)
          LogErr(INFORMATION_LEVEL, ER_SEMISYNC_INIT_WAIT_POS, kWho,
                 wait_file_name_, (unsigned long)wait_file_pos_);
      }

      /* In semi-synchronous replication, we wait until the binlog-dump
       * thread has received the reply on the relevant binlog segment from the
       * replication slave.
       *
       * Let us suspend this thread to wait on the condition;
       * when replication has progressed far enough, we will release
       * these waiting threads.
       */
      if (connection_events_loop_aborted() &&
          (rpl_semi_sync_source_clients ==
           rpl_semi_sync_source_wait_for_replica_count - 1) &&
          is_on()) {
        LogErr(WARNING_LEVEL, ER_SEMISYNC_FORCED_SHUTDOWN);
        switch_off();
        break;
      }

      rpl_semi_sync_source_wait_sessions++;

      if (trace_level_ & kTraceDetail)
        LogErr(INFORMATION_LEVEL, ER_SEMISYNC_WAIT_TIME_FOR_BINLOG_SENT, kWho,
               wait_timeout_, wait_file_name_, (unsigned long)wait_file_pos_);

      /* wait for the position to be ACK'ed back */
      assert(entry);
      entry->n_waiters++;
      wait_result = mysql_cond_timedwait(&entry->cond, &LOCK_binlog_, &abstime);
      entry->n_waiters--;
      /*
        After we release LOCK_binlog_ above while waiting for the condition,
        it can happen that some other parallel client session executed
        RESET MASTER. That can set rpl_semi_sync_source_wait_sessions to zero.
        Hence check the value before decrementing it and decrement it only if it
        is non-zero value.
      */
      if (rpl_semi_sync_source_wait_sessions > 0)
        rpl_semi_sync_source_wait_sessions--;

      if (wait_result != 0) {
        /* This is a real wait timeout. */
        LogErr(WARNING_LEVEL, ER_SEMISYNC_WAIT_FOR_BINLOG_TIMEDOUT,
               trx_wait_binlog_name, (unsigned long)trx_wait_binlog_pos,
               reply_file_name_, (unsigned long)reply_file_pos_);
        rpl_semi_sync_source_wait_timeouts++;

        /* switch semi-sync off */
        switch_off();
      } else {
        int wait_time;

        wait_time = getWaitTime(start_ts);
        if (wait_time < 0) {
          if (trace_level_ & kTraceGeneral) {
            LogErr(INFORMATION_LEVEL,
                   ER_SEMISYNC_WAIT_TIME_ASSESSMENT_FOR_COMMIT_TRX_FAILED,
                   trx_wait_binlog_name, (unsigned long)trx_wait_binlog_pos);
          }
          rpl_semi_sync_source_timefunc_fails++;
        } else {
          rpl_semi_sync_source_trx_wait_num++;
          rpl_semi_sync_source_trx_wait_time += wait_time;
        }
      }
    }

  l_end:
    /* Update the status counter. */
    if (is_on() && is_semi_sync_trans)
      rpl_semi_sync_source_yes_transactions++;
    else
      rpl_semi_sync_source_no_transactions++;
  }

  /* Last waiter removes the TranxNode */
  if (trx_wait_binlog_name && active_tranxs_ && entry && entry->n_waiters == 0)
    active_tranxs_->clear_active_tranx_nodes(trx_wait_binlog_name,
                                             trx_wait_binlog_pos);

  unlock();
  THD_EXIT_COND(nullptr, &old_stage);
  return function_exit(kWho, 0);
}
void ReplSemiSyncMaster::set_wait_no_replica(const void *val) {
  lock();
  char set_switch = *static_cast<const char *>(val);
  if (set_switch == 0) {
    if ((rpl_semi_sync_source_clients == 0) && (is_on())) switch_off();
  } else {
    if (!is_on() && getMasterEnabled()) force_switch_on();
  }
  unlock();
}

void ReplSemiSyncMaster::force_switch_on() { state_ = true; }

/* Indicate that semi-sync replication is OFF now.
 *
 * What should we do when it is disabled?  The problem is that we want
 * the semi-sync replication enabled again when the slave catches up
 * later.  But, it is not that easy to detect that the slave has caught
 * up.  This is caused by the fact that MySQL's replication protocol is
 * asynchronous, meaning that if the master does not use the semi-sync
 * protocol, the slave would not send anything to the master.
 * Still, if the master is sending (N+1)-th event, we assume that it is
 * an indicator that the slave has received N-th event and earlier ones.
 *
 * If semi-sync is disabled, all transactions still update the wait
 * position with the last position in binlog.  But no transactions will
 * wait for confirmations maintained.  In binlog dump thread,
 * updateSyncHeader() checks whether the current sending event catches
 * up with last wait position.  If it does match, semi-sync will be
 * switched on again.
 */
int ReplSemiSyncMaster::switch_off() {
  const char *kWho = "ReplSemiSyncMaster::switch_off";

  function_enter(kWho);
  state_ = false;

  rpl_semi_sync_source_off_times++;
  wait_file_name_inited_ = false;
  reply_file_name_inited_ = false;
  LogErr(INFORMATION_LEVEL, ER_SEMISYNC_RPL_SWITCHED_OFF);

  /* signal waiting sessions */
  active_tranxs_->signal_waiting_sessions_all();

  return function_exit(kWho, 0);
}

int ReplSemiSyncMaster::try_switch_on(const char *log_file_name,
                                      my_off_t log_file_pos) {
  const char *kWho = "ReplSemiSyncMaster::try_switch_on";
  bool semi_sync_on = false;

  function_enter(kWho);

  /* If the current sending event's position is larger than or equal to the
   * 'largest' commit transaction binlog position, the slave is already
   * catching up now and we can switch semi-sync on here.
   * If commit_file_name_inited_ indicates there are no recent transactions,
   * we can enable semi-sync immediately.
   */
  if (commit_file_name_inited_) {
    int cmp = ActiveTranx::compare(log_file_name, log_file_pos,
                                   commit_file_name_, commit_file_pos_);
    semi_sync_on = (cmp >= 0);
  } else {
    semi_sync_on = true;
  }

  if (semi_sync_on) {
    /* Switch semi-sync replication on. */
    state_ = true;

    LogErr(INFORMATION_LEVEL, ER_SEMISYNC_RPL_SWITCHED_ON, log_file_name,
           (unsigned long)log_file_pos);
  }

  return function_exit(kWho, 0);
}

int ReplSemiSyncMaster::reserveSyncHeader(unsigned char *header,
                                          unsigned long size) {
  const char *kWho = "ReplSemiSyncMaster::reserveSyncHeader";
  function_enter(kWho);

  int hlen = 0;
  {
    /* No enough space for the extra header, disable semi-sync master */
    if (sizeof(kSyncHeader) > size) {
      LogErr(WARNING_LEVEL, ER_SEMISYNC_NO_SPACE_IN_THE_PKT);
      disableMaster();
      return 0;
    }

    /* Set the magic number and the sync status.  By default, no sync
     * is required.
     */
    memcpy(header, kSyncHeader, sizeof(kSyncHeader));
    hlen = sizeof(kSyncHeader);
  }
  return function_exit(kWho, hlen);
}

int ReplSemiSyncMaster::updateSyncHeader(unsigned char *packet,
                                         const char *log_file_name,
                                         my_off_t log_file_pos,
                                         uint32 server_id) {
  const char *kWho = "ReplSemiSyncMaster::updateSyncHeader";
  int cmp = 0;
  bool sync = false;

  /* If the semi-sync master is not enabled, do not request replies from the
     slave.
   */
  if (!getMasterEnabled()) return 0;

  function_enter(kWho);

  lock();

  /* This is the real check inside the mutex. */
  if (!getMasterEnabled()) goto l_end;  // sync= false at this point in time

  if (is_on()) {
    /* semi-sync is ON */
    /* sync= false; No sync unless a transaction is involved. */

    if (reply_file_name_inited_) {
      cmp = ActiveTranx::compare(log_file_name, log_file_pos, reply_file_name_,
                                 reply_file_pos_);
      if (cmp <= 0) {
        /* If we have already got the reply for the event, then we do
         * not need to sync the transaction again.
         */
        goto l_end;
      }
    }

    if (wait_file_name_inited_) {
      cmp = ActiveTranx::compare(log_file_name, log_file_pos, wait_file_name_,
                                 wait_file_pos_);
    } else {
      cmp = 1;
    }

    /* If we are already waiting for some transaction replies which
     * are later in binlog, do not wait for this one event.
     */
    if (cmp >= 0) {
      /*
       * We only wait if the event is a transaction's ending event.
       */
      assert(active_tranxs_ != nullptr);
      sync = active_tranxs_->is_tranx_end_pos(log_file_name, log_file_pos);
    }
  } else {
    if (commit_file_name_inited_) {
      int cmp = ActiveTranx::compare(log_file_name, log_file_pos,
                                     commit_file_name_, commit_file_pos_);
      sync = (cmp >= 0);
    } else {
      sync = true;
    }
  }

  if (trace_level_ & kTraceDetail)
    LogErr(INFORMATION_LEVEL, ER_SEMISYNC_SYNC_HEADER_UPDATE_INFO, kWho,
           server_id, log_file_name, (unsigned long)log_file_pos, sync,
           (int)is_on());

l_end:
  unlock();

  /* We do not need to clear sync flag because we set it to 0 when we
   * reserve the packet header.
   */
  if (sync) {
    (packet)[2] = kPacketFlagSync;
  }

  return function_exit(kWho, 0);
}

int ReplSemiSyncMaster::writeTranxInBinlog(const char *log_file_name,
                                           my_off_t log_file_pos) {
  const char *kWho = "ReplSemiSyncMaster::writeTranxInBinlog";
  int result = 0;

  function_enter(kWho);

  lock();

  /* This is the real check inside the mutex. */
  if (!getMasterEnabled()) goto l_end;

  /* Update the 'largest' transaction commit position seen so far even
   * though semi-sync is switched off.
   * It is much better that we update commit_file_* here, instead of
   * inside commitTrx().  This is mostly because updateSyncHeader()
   * will watch for commit_file_* to decide whether to switch semi-sync
   * on. The detailed reason is explained in function updateSyncHeader().
   */
  if (commit_file_name_inited_) {
    int cmp = ActiveTranx::compare(log_file_name, log_file_pos,
                                   commit_file_name_, commit_file_pos_);
    if (cmp > 0) {
      /* This is a larger position, let's update the maximum info. */
      strncpy(commit_file_name_, log_file_name, FN_REFLEN - 1);
      commit_file_name_[FN_REFLEN - 1] = 0; /* make sure it ends properly */
      commit_file_pos_ = log_file_pos;
    }
  } else {
    strncpy(commit_file_name_, log_file_name, FN_REFLEN - 1);
    commit_file_name_[FN_REFLEN - 1] = 0; /* make sure it ends properly */
    commit_file_pos_ = log_file_pos;
    commit_file_name_inited_ = true;
  }

  if (is_on()) {
    assert(active_tranxs_ != nullptr);
    if (active_tranxs_->insert_tranx_node(log_file_name, log_file_pos)) {
      /*
        if insert tranx_node failed, print a warning message
        and turn off semi-sync
      */
      LogErr(WARNING_LEVEL, ER_SEMISYNC_FAILED_TO_INSERT_TRX_NODE,
             log_file_name, (ulong)log_file_pos);
      switch_off();
    }
  }

l_end:
  unlock();

  return function_exit(kWho, result);
}

int ReplSemiSyncMaster::skipSlaveReply(const char *event_buf, uint32 server_id,
                                       const char *skipped_log_file,
                                       my_off_t skipped_log_pos) {
  const char *kWho = "ReplSemiSyncMaster::skipSlaveReply";

  function_enter(kWho);

  assert((unsigned char)event_buf[1] == kPacketMagicNum);
  if ((unsigned char)event_buf[2] != kPacketFlagSync) {
    /* current event would not require a reply anyway */
    goto l_end;
  }

  if (trace_level_ & kTraceDetail)
    LogErr(INFORMATION_LEVEL, ER_SEMISYNC_TRX_SKIPPED_AT_POS, kWho,
           skipped_log_file, (unsigned long)skipped_log_pos);

  /* Treat skipped event as a received ack */
  handleAck(server_id, skipped_log_file, skipped_log_pos);

l_end:
  return function_exit(kWho, 0);
}

int ReplSemiSyncMaster::readSlaveReply(NET *net, const char *event_buf) {
  const char *kWho = "ReplSemiSyncMaster::readSlaveReply";
  int result = -1;

  function_enter(kWho);

  assert((unsigned char)event_buf[1] == kPacketMagicNum);
  if ((unsigned char)event_buf[2] != kPacketFlagSync) {
    /* current event does not require reply */
    result = 0;
    goto l_end;
  }

  /* We flush to make sure that the current event is sent to the network,
   * instead of being buffered in the TCP/IP stack.
   */
  if (net_flush(net)) {
    LogErr(ERROR_LEVEL, ER_SEMISYNC_SOURCE_FAILED_ON_NET_FLUSH);
    goto l_end;
  }

  net_clear(net, false);
  net->pkt_nr++;
  result = 0;
  rpl_semi_sync_source_net_wait_num++;

l_end:
  return function_exit(kWho, result);
}

int ReplSemiSyncMaster::resetMaster() {
  const char *kWho = "ReplSemiSyncMaster::resetMaster";
  int result = 0;

  function_enter(kWho);

  lock();

  ack_container_.clear();

  wait_file_name_inited_ = false;
  reply_file_name_inited_ = false;
  commit_file_name_inited_ = false;

  rpl_semi_sync_source_yes_transactions = 0;
  rpl_semi_sync_source_no_transactions = 0;
  rpl_semi_sync_source_off_times = 0;
  rpl_semi_sync_source_timefunc_fails = 0;
  rpl_semi_sync_source_wait_sessions = 0;
  rpl_semi_sync_source_wait_pos_backtraverse = 0;
  rpl_semi_sync_source_trx_wait_num = 0;
  rpl_semi_sync_source_trx_wait_time = 0;
  rpl_semi_sync_source_net_wait_num = 0;
  rpl_semi_sync_source_net_wait_time = 0;

  unlock();

  return function_exit(kWho, result);
}

void ReplSemiSyncMaster::setExportStats() {
  lock();

  rpl_semi_sync_source_status = state_;
  rpl_semi_sync_source_avg_trx_wait_time =
      ((rpl_semi_sync_source_trx_wait_num)
           ? (unsigned long)((double)rpl_semi_sync_source_trx_wait_time /
                             ((double)rpl_semi_sync_source_trx_wait_num))
           : 0);
  rpl_semi_sync_source_avg_net_wait_time =
      ((rpl_semi_sync_source_net_wait_num)
           ? (unsigned long)((double)rpl_semi_sync_source_net_wait_time /
                             ((double)rpl_semi_sync_source_net_wait_num))
           : 0);

  unlock();
}

int ReplSemiSyncMaster::setWaitSlaveCount(unsigned int new_value) {
  const AckInfo *ackinfo = nullptr;
  int result = 0;

  const char *kWho = "ReplSemiSyncMaster::updateWaitSlaves";
  function_enter(kWho);

  lock();

  result = ack_container_.resize(new_value, &ackinfo);
  if (result == 0) {
    rpl_semi_sync_source_wait_for_replica_count = new_value;
    if (ackinfo != nullptr)
      reportReplyBinlog(ackinfo->binlog_name, ackinfo->binlog_pos);
  }

  unlock();
  return function_exit(kWho, result);
}

const AckInfo *AckContainer::insert(int server_id, const char *log_file_name,
                                    my_off_t log_file_pos) {
  const AckInfo *ret_ack = nullptr;

  const char *kWho = "AckContainer::insert";
  function_enter(kWho);

  if (!m_greatest_ack.less_than(log_file_name, log_file_pos)) {
    if (trace_level_ & kTraceDetail)
      LogErr(INFORMATION_LEVEL, ER_SEMISYNC_RECEIVED_ACK_IS_SMALLER);

    goto l_end;
  }

  /* Update the slave's ack position if it is in the ack array */
  if (updateIfExist(server_id, log_file_name, log_file_pos) < m_size)
    goto l_end;

  if (full()) {
    AckInfo *min_ack;

    ret_ack = &m_greatest_ack;

    /* Find the minimum ack which is smaller than the inserted ack. */
    min_ack = minAck(log_file_name, log_file_pos);
    if (likely(min_ack == nullptr)) {
      m_greatest_ack.set(server_id, log_file_name, log_file_pos);

      /* Remove all slaves which have minimum ack position from the ack array */
      remove_all(log_file_name, log_file_pos);

      /* Don't insert current ack into container if it is the minimum ack. */
      goto l_end;
    } else {
      m_greatest_ack = *min_ack;
      remove_all(m_greatest_ack.binlog_name, m_greatest_ack.binlog_pos);
    }
  }

  m_ack_array[m_empty_slot].set(server_id, log_file_name, log_file_pos);

  if (trace_level_ & kTraceDetail)
    LogErr(INFORMATION_LEVEL, ER_SEMISYNC_ADD_ACK_TO_SLOT, m_empty_slot);

l_end:
  function_exit(kWho, 0);
  return ret_ack;
}

int AckContainer::resize(unsigned int size, const AckInfo **ackinfo) {
  AckInfo *old_ack_array = m_ack_array;
  unsigned int old_array_size = m_size;
  unsigned int i;

  if (size - 1 == m_size) return 0;

  m_size = size - 1;
  m_ack_array = nullptr;
  if (m_size) {
    m_ack_array = (AckInfo *)DBUG_EVALUATE_IF(
        "rpl_semisync_simulate_allocate_ack_container_failure", NULL,
        my_malloc(0, sizeof(AckInfo) * (size - 1), MYF(MY_ZEROFILL)));
    if (m_ack_array == nullptr) {
      m_ack_array = old_ack_array;
      m_size = old_array_size;
      return -1;
    }
  }

  if (old_ack_array != nullptr) {
    for (i = 0; i < old_array_size; i++) {
      const AckInfo *ack = insert(old_ack_array[i]);
      if (ack) *ackinfo = ack;
    }
    my_free(old_ack_array);
  }
  return 0;
}

/* Get the waiting time given the wait's staring time.
 *
 * Return:
 *  >= 0: the waiting time in microsecons(us)
 *   < 0: error in get time or time back traverse
 */
static int getWaitTime(const struct timespec &start_ts) {
  unsigned long long start_usecs, end_usecs;
  struct timespec end_ts;

  /* Starting time in microseconds(us). */
  start_usecs = timespec_to_usec(&start_ts);

  /* Get the wait time interval. */
  set_timespec(&end_ts, 0);

  /* Ending time in microseconds(us). */
  end_usecs = timespec_to_usec(&end_ts);

  if (end_usecs < start_usecs) return -1;

  return (int)(end_usecs - start_usecs);
}
