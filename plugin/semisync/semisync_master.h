/* Copyright (C) 2007 Google Inc.
   Copyright (C) 2008 MySQL AB

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


#ifndef SEMISYNC_MASTER_H
#define SEMISYNC_MASTER_H

#include "semisync.h"

/**
   This class manages memory for active transaction list.

   We record each active transaction with a TranxNode.  Because each
   session can only have only one open transaction, the total active
   transaction nodes can not exceed the maximum sessions.  Currently
   in MySQL, sessions are the same as connections.
*/
class ActiveTranx
  :public Trace {
private:
  struct TranxNode {
    char             *log_name_;
    my_off_t          log_pos_;
    struct TranxNode *next_;            /* the next node in the sorted list */
    struct TranxNode *hash_next_;    /* the next node during hash collision */
  };

  /* The following data structure maintains an active transaction list. */
  TranxNode       *node_array_;
  TranxNode       *free_pool_;

  /* These two record the active transaction list in sort order. */
  TranxNode       *trx_front_, *trx_rear_;

  TranxNode      **trx_htb_;        /* A hash table on active transactions. */

  int              num_transactions_;               /* maximum transactions */
  int              num_entries_;              /* maximum hash table entries */
  pthread_mutex_t *lock_;                                     /* mutex lock */

  inline void assert_lock_owner();

  inline TranxNode* alloc_tranx_node();

  inline unsigned int calc_hash(const unsigned char *key,unsigned int length);
  unsigned int get_hash_value(const char *log_file_name, my_off_t log_file_pos);

  int compare(const char *log_file_name1, my_off_t log_file_pos1,
	      const TranxNode *node2) {
    return compare(log_file_name1, log_file_pos1,
		   node2->log_name_, node2->log_pos_);
  }
  int compare(const TranxNode *node1,
	      const char *log_file_name2, my_off_t log_file_pos2) {
    return compare(node1->log_name_, node1->log_pos_,
		   log_file_name2, log_file_pos2);
  }
  int compare(const TranxNode *node1, const TranxNode *node2) {
    return compare(node1->log_name_, node1->log_pos_,
		   node2->log_name_, node2->log_pos_);
  }

public:
  ActiveTranx(int max_connections, pthread_mutex_t *lock,
	      unsigned long trace_level);
  ~ActiveTranx();

  /* Insert an active transaction node with the specified position.
   *
   * Return:
   *  0: success;  -1 or otherwise: error
   */
  int insert_tranx_node(const char *log_file_name, my_off_t log_file_pos);

  /* Clear the active transaction nodes until(inclusive) the specified
   * position.
   * If log_file_name is NULL, everything will be cleared: the sorted
   * list and the hash table will be reset to empty.
   * 
   * Return:
   *  0: success;  -1 or otherwise: error
   */
  int clear_active_tranx_nodes(const char *log_file_name,
			       my_off_t    log_file_pos);

  /* Given a position, check to see whether the position is an active
   * transaction's ending position by probing the hash table.
   */
  bool is_tranx_end_pos(const char *log_file_name, my_off_t log_file_pos);

  /* Given two binlog positions, compare which one is bigger based on
   * (file_name, file_position).
   */
  static int compare(const char *log_file_name1, my_off_t log_file_pos1,
		     const char *log_file_name2, my_off_t log_file_pos2);

};

/**
   The extension class for the master of semi-synchronous replication
*/
class ReplSemiSyncMaster
  :public ReplSemiSyncBase {
 private:
  ActiveTranx    *active_tranxs_;  /* active transaction list: the list will
                                      be cleared when semi-sync switches off. */

  /* True when initObject has been called */
  bool init_done_;

  /* This cond variable is signaled when enough binlog has been sent to slave,
   * so that a waiting trx can return the 'ok' to the client for a commit.
   */
  pthread_cond_t  COND_binlog_send_;

  /* Mutex that protects the following state variables and the active
   * transaction list.
   * Under no cirumstances we can acquire mysql_bin_log.LOCK_log if we are
   * already holding LOCK_binlog_ because it can cause deadlocks.
   */
  pthread_mutex_t LOCK_binlog_;

  /* This is set to true when reply_file_name_ contains meaningful data. */
  bool            reply_file_name_inited_;

  /* The binlog name up to which we have received replies from any slaves. */
  char            reply_file_name_[FN_REFLEN];

  /* The position in that file up to which we have the reply from any slaves. */
  my_off_t        reply_file_pos_;

  /* This is set to true when we know the 'smallest' wait position. */
  bool            wait_file_name_inited_;

  /* NULL, or the 'smallest' filename that a transaction is waiting for
   * slave replies.
   */
  char            wait_file_name_[FN_REFLEN];

  /* The smallest position in that file that a trx is waiting for: the trx
   * can proceed and send an 'ok' to the client when the master has got the
   * reply from the slave indicating that it already got the binlog events.
   */
  my_off_t        wait_file_pos_;

  /* This is set to true when we know the 'largest' transaction commit
   * position in the binlog file.
   * We always maintain the position no matter whether semi-sync is switched
   * on switched off.  When a transaction wait timeout occurs, semi-sync will
   * switch off.  Binlog-dump thread can use the three fields to detect when
   * slaves catch up on replication so that semi-sync can switch on again.
   */
  bool            commit_file_name_inited_;

  /* The 'largest' binlog filename that a commit transaction is seeing.       */
  char            commit_file_name_[FN_REFLEN];

  /* The 'largest' position in that file that a commit transaction is seeing. */
  my_off_t        commit_file_pos_;

  /* All global variables which can be set by parameters. */
  volatile bool            master_enabled_;      /* semi-sync is enabled on the master */
  unsigned long           wait_timeout_;      /* timeout period(ms) during tranx wait */

  /* All status variables. */
  bool            state_;                    /* whether semi-sync is switched */
  unsigned long           enabled_transactions_;          /* semi-sync'ed tansactions */
  unsigned long           disabled_transactions_;     /* non-semi-sync'ed tansactions */
  unsigned long           switched_off_times_;    /* how many times are switched off? */
  unsigned long           timefunc_fails_;           /* how many time function fails? */
  unsigned long           total_wait_timeouts_;      /* total number of wait timeouts */
  unsigned long           wait_sessions_;      /* how many sessions wait for replies? */
  unsigned long           wait_backtraverse_;         /* wait position back traverses */
  unsigned long long       total_trx_wait_num_;   /* total trx waits: non-timeout ones */
  unsigned long long       total_trx_wait_time_;         /* total trx wait time: in us */
  unsigned long long       total_net_wait_num_;                 /* total network waits */
  unsigned long long       total_net_wait_time_;            /* total network wait time */

  /* The number of maximum active transactions.  This should be the same as
   * maximum connections because MySQL does not do connection sharing now.
   */
  int             max_transactions_;

  void lock();
  void unlock();
  void cond_broadcast();
  int  cond_timewait(struct timespec *wait_time);

  /* Is semi-sync replication on? */
  bool is_on() {
    return (state_);
  }

  void set_master_enabled(bool enabled) {
    master_enabled_ = enabled;
  }

  /* Switch semi-sync off because of timeout in transaction waiting. */
  int switch_off();

  /* Switch semi-sync on when slaves catch up. */
  int try_switch_on(int server_id,
                    const char *log_file_name, my_off_t log_file_pos);

 public:
  ReplSemiSyncMaster();
  ~ReplSemiSyncMaster();

  bool getMasterEnabled() {
    return master_enabled_;
  }
  void setTraceLevel(unsigned long trace_level) {
    trace_level_ = trace_level;
    if (active_tranxs_)
      active_tranxs_->trace_level_ = trace_level;
  }

  /* Set the transaction wait timeout period, in milliseconds. */
  void setWaitTimeout(unsigned long wait_timeout) {
    wait_timeout_ = wait_timeout;
  }

  /* Initialize this class after MySQL parameters are initialized. this
   * function should be called once at bootstrap time.
   */
  int initObject();

  /* Enable the object to enable semi-sync replication inside the master. */
  int enableMaster();

  /* Enable the object to enable semi-sync replication inside the master. */
  int disableMaster();

  /* Add a semi-sync replication slave */
  void add_slave();
    
  /* Remove a semi-sync replication slave */
  void remove_slave();

  /* Is the slave servered by the thread requested semi-sync */
  bool is_semi_sync_slave();

  int reportReplyBinlog(const char *log_file_pos);
  
  /* In semi-sync replication, reports up to which binlog position we have
   * received replies from the slave indicating that it already get the events.
   *
   * Input:
   *  server_id     - (IN)  master server id number
   *  log_file_name - (IN)  binlog file name
   *  end_offset    - (IN)  the offset in the binlog file up to which we have
   *                        the replies from the slave
   *
   * Return:
   *  0: success;  -1 or otherwise: error
   */
  int reportReplyBinlog(uint32 server_id,
                        const char* log_file_name,
                        my_off_t end_offset);

  /* Commit a transaction in the final step.  This function is called from
   * InnoDB before returning from the low commit.  If semi-sync is switch on,
   * the function will wait to see whether binlog-dump thread get the reply for
   * the events of the transaction.  Remember that this is not a direct wait,
   * instead, it waits to see whether the binlog-dump thread has reached the
   * point.  If the wait times out, semi-sync status will be switched off and
   * all other transaction would not wait either.
   *
   * Input:  (the transaction events' ending binlog position)
   *  trx_wait_binlog_name - (IN)  ending position's file name
   *  trx_wait_binlog_pos  - (IN)  ending position's file offset
   *
   * Return:
   *  0: success;  -1 or otherwise: error
   */
  int commitTrx(const char* trx_wait_binlog_name,
                my_off_t trx_wait_binlog_pos);

  /* Reserve space in the replication event packet header:
   *  . slave semi-sync off: 1 byte - (0)
   *  . slave semi-sync on:  3 byte - (0, 0xef, 0/1}
   * 
   * Input:
   *  header   - (IN)  the header buffer
   *  size     - (IN)  size of the header buffer
   *
   * Return:
   *  size of the bytes reserved for header
   */
  int reserveSyncHeader(unsigned char *header, unsigned long size);

  /* Update the sync bit in the packet header to indicate to the slave whether
   * the master will wait for the reply of the event.  If semi-sync is switched
   * off and we detect that the slave is catching up, we switch semi-sync on.
   * 
   * Input:
   *  packet        - (IN)  the packet containing the replication event
   *  log_file_name - (IN)  the event ending position's file name
   *  log_file_pos  - (IN)  the event ending position's file offset
   *  server_id     - (IN)  master server id number
   *
   * Return:
   *  0: success;  -1 or otherwise: error
   */
  int updateSyncHeader(unsigned char *packet,
                       const char *log_file_name,
		       my_off_t log_file_pos,
		       uint32 server_id);

  /* Called when a transaction finished writing binlog events.
   *  . update the 'largest' transactions' binlog event position
   *  . insert the ending position in the active transaction list if
   *    semi-sync is on
   * 
   * Input:  (the transaction events' ending binlog position)
   *  log_file_name - (IN)  transaction ending position's file name
   *  log_file_pos  - (IN)  transaction ending position's file offset
   *
   * Return:
   *  0: success;  -1 or otherwise: error
   */
  int writeTranxInBinlog(const char* log_file_name, my_off_t log_file_pos);

  /* Export internal statistics for semi-sync replication. */
  void setExportStats();

  /* 'reset master' command is issued from the user and semi-sync need to
   * go off for that.
   */
  int resetMaster();
};

/* System and status variables for the master component */
extern char rpl_semi_sync_master_enabled;
extern unsigned long rpl_semi_sync_master_timeout;
extern unsigned long rpl_semi_sync_master_trace_level;
extern unsigned long rpl_semi_sync_master_status;
extern unsigned long rpl_semi_sync_master_yes_transactions;
extern unsigned long rpl_semi_sync_master_no_transactions;
extern unsigned long rpl_semi_sync_master_off_times;
extern unsigned long rpl_semi_sync_master_timefunc_fails;
extern unsigned long rpl_semi_sync_master_num_timeouts;
extern unsigned long rpl_semi_sync_master_wait_sessions;
extern unsigned long rpl_semi_sync_master_back_wait_pos;
extern unsigned long rpl_semi_sync_master_trx_wait_time;
extern unsigned long rpl_semi_sync_master_net_wait_time;
extern unsigned long long rpl_semi_sync_master_net_wait_num;
extern unsigned long long rpl_semi_sync_master_trx_wait_num;
extern unsigned long long rpl_semi_sync_master_net_wait_total_time;
extern unsigned long long rpl_semi_sync_master_trx_wait_total_time;
extern unsigned long rpl_semi_sync_master_clients;

#endif /* SEMISYNC_MASTER_H */
