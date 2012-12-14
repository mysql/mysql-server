/* Copyright (C) 2007 Google Inc.
   Copyright (c) 2008 MySQL AB, 2009 Sun Microsystems, Inc.
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


#ifndef SEMISYNC_MASTER_H
#define SEMISYNC_MASTER_H

#include "semisync.h"

#ifdef HAVE_PSI_INTERFACE
extern PSI_mutex_key key_ss_mutex_LOCK_binlog_;
extern PSI_cond_key key_ss_cond_COND_binlog_send_;
#endif

extern PSI_stage_info stage_waiting_for_semi_sync_ack_from_slave;

struct TranxNode {
  char             log_name_[FN_REFLEN];
  my_off_t          log_pos_;
  struct TranxNode *next_;            /* the next node in the sorted list */
  struct TranxNode *hash_next_;    /* the next node during hash collision */
};

/**
  @class TranxNodeAllocator

  This class provides memory allocating and freeing methods for
  TranxNode. The main target is performance.

  @section ALLOCATE How to allocate a node
    The pointer of the first node after 'last_node' in current_block is
    returned. current_block will move to the next free Block when all nodes of
    it are in use. A new Block is allocated and is put into the rear of the
    Block link table if no Block is free.

    The list starts up empty (ie, there is no allocated Block).

    After some nodes are freed, there probably are some free nodes before
    the sequence of the allocated nodes, but we do not reuse it. It is better
    to keep the allocated nodes are in the sequence, for it is more efficient
    for allocating and freeing TranxNode.

  @section FREENODE How to free nodes
    There are two methods for freeing nodes. They are free_all_nodes and
    free_nodes_before.

    'A Block is free' means all of its nodes are free.
    @subsection free_nodes_before
    As all allocated nodes are in the sequence, 'Before one node' means all
    nodes before given node in the same Block and all Blocks before the Block
    which containing the given node. As such, all Blocks before the given one
    ('node') are free Block and moved into the rear of the Block link table.
    The Block containing the given 'node', however, is not. For at least the
    given 'node' is still in use. This will waste at most one Block, but it is
    more efficient.
 */
#define BLOCK_TRANX_NODES 16
class TranxNodeAllocator
{
public:
  /**
    @param reserved_nodes
      The number of reserved TranxNodes. It is used to set 'reserved_blocks'
      which can contain at least 'reserved_nodes' number of TranxNodes.  When
      freeing memory, we will reserve at least reserved_blocks of Blocks not
      freed.
   */
  TranxNodeAllocator(uint reserved_nodes) :
    reserved_blocks(reserved_nodes/BLOCK_TRANX_NODES +
                  (reserved_nodes%BLOCK_TRANX_NODES > 1 ? 2 : 1)),
    first_block(NULL), last_block(NULL),
    current_block(NULL), last_node(-1), block_num(0) {}

  ~TranxNodeAllocator()
  {
    Block *block= first_block;
    while (block != NULL)
    {
      Block *next= block->next;
      free_block(block);
      block= next;
    }
  }

  /**
    The pointer of the first node after 'last_node' in current_block is
    returned. current_block will move to the next free Block when all nodes of
    it are in use. A new Block is allocated and is put into the rear of the
    Block link table if no Block is free.

    @return Return a TranxNode *, or NULL if an error occured.
   */
  TranxNode *allocate_node()
  {
    TranxNode *trx_node;
    Block *block= current_block;

    if (last_node == BLOCK_TRANX_NODES-1)
    {
      current_block= current_block->next;
      last_node= -1;
    }

    if (current_block == NULL && allocate_block())
    {
      current_block= block;
      if (current_block)
        last_node= BLOCK_TRANX_NODES-1;
      return NULL;
    }

    trx_node= &(current_block->nodes[++last_node]);
    trx_node->log_name_[0] = '\0';
    trx_node->log_pos_= 0;
    trx_node->next_= 0;
    trx_node->hash_next_= 0;
    return trx_node;
  }

  /**
    All nodes are freed.

    @return Return 0, or 1 if an error occured.
   */
  int free_all_nodes()
  {
    current_block= first_block;
    last_node= -1;
    free_blocks();
    return 0;
  }

  /**
    All Blocks before the given 'node' are free Block and moved into the rear
    of the Block link table.

    @param node All nodes before 'node' will be freed

    @return Return 0, or 1 if an error occured.
   */
  int free_nodes_before(TranxNode* node)
  {
    Block *block;
    Block *prev_block= NULL;

    block= first_block;
    while (block != current_block->next)
    {
      /* Find the Block containing the given node */
      if (&(block->nodes[0]) <= node && &(block->nodes[BLOCK_TRANX_NODES]) >= node)
      {
        /* All Blocks before the given node are put into the rear */
        if (first_block != block)
        {
          last_block->next= first_block;
          first_block= block;
          last_block= prev_block;
          last_block->next= NULL;
          free_blocks();
        }
        return 0;
      }
      prev_block= block;
      block= block->next;
    }

    /* Node does not find should never happen */
    DBUG_ASSERT(0);
    return 1;
  }

private:
  uint reserved_blocks;

 /**
   A sequence memory which contains BLOCK_TRANX_NODES TranxNodes.

   BLOCK_TRANX_NODES The number of TranxNodes which are in a Block.

   next Every Block has a 'next' pointer which points to the next Block.
        These linking Blocks constitute a Block link table.
  */
  struct Block {
    Block *next;
    TranxNode nodes[BLOCK_TRANX_NODES];
  };

  /**
    The 'first_block' is the head of the Block link table;
   */
  Block *first_block;
  /**
    The 'last_block' is the rear of the Block link table;
   */
  Block *last_block;

  /**
    current_block always points the Block in the Block link table in
    which the last allocated node is. The Blocks before it are all in use
    and the Blocks after it are all free.
   */
  Block *current_block;

  /**
    It always points to the last node which has been allocated in the
    current_block.
   */
  int last_node;

  /**
    How many Blocks are in the Block link table.
   */
  uint block_num;

  /**
    Allocate a block and then assign it to current_block.
  */
  int allocate_block()
  {
    Block *block= (Block *)my_malloc(sizeof(Block), MYF(0));
    if (block)
    {
      block->next= NULL;

      if (first_block == NULL)
        first_block= block;
      else
        last_block->next= block;

      /* New Block is always put into the rear */
      last_block= block;
      /* New Block is always the current_block */
      current_block= block;
      ++block_num;
      return 0;
    }
    return 1;
  }

  /**
    Free a given Block.
    @param block The Block will be freed.
   */
  void free_block(Block *block)
  {
    my_free(block);
    --block_num;
  }


  /**
    If there are some free Blocks and the total number of the Blocks in the
    Block link table is larger than the 'reserved_blocks', Some free Blocks
    will be freed until the total number of the Blocks is equal to the
    'reserved_blocks' or there is only one free Block behind the
    'current_block'.
   */
  void free_blocks()
  {
    if (current_block == NULL || current_block->next == NULL)
      return;

    /* One free Block is always kept behind the current block */
    Block *block= current_block->next->next;
    while (block_num > reserved_blocks && block != NULL)
    {
      Block *next= block->next;
      free_block(block);
      block= next;
    }
    current_block->next->next= block;
    if (block == NULL)
      last_block= current_block->next;
  }
};

/**
   This class manages memory for active transaction list.

   We record each active transaction with a TranxNode, each session
   can have only one open transaction. Because of EVENT, the total
   active transaction nodes can exceed the maximum allowed
   connections.
*/
class ActiveTranx
  :public Trace {
private:

  TranxNodeAllocator allocator_;
  /* These two record the active transaction list in sort order. */
  TranxNode       *trx_front_, *trx_rear_;

  TranxNode      **trx_htb_;        /* A hash table on active transactions. */

  int              num_entries_;              /* maximum hash table entries */
  mysql_mutex_t *lock_;                                     /* mutex lock */

  inline void assert_lock_owner();

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
  ActiveTranx(mysql_mutex_t *lock, unsigned long trace_level);
  ~ActiveTranx();

  /* Insert an active transaction node with the specified position.
   *
   * Return:
   *  0: success;  non-zero: error
   */
  int insert_tranx_node(const char *log_file_name, my_off_t log_file_pos);

  /* Clear the active transaction nodes until(inclusive) the specified
   * position.
   * If log_file_name is NULL, everything will be cleared: the sorted
   * list and the hash table will be reset to empty.
   * 
   * Return:
   *  0: success;  non-zero: error
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
  mysql_cond_t  COND_binlog_send_;

  /* Mutex that protects the following state variables and the active
   * transaction list.
   * Under no cirumstances we can acquire mysql_bin_log.LOCK_log if we are
   * already holding LOCK_binlog_ because it can cause deadlocks.
   */
  mysql_mutex_t LOCK_binlog_;

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

  bool            state_;                    /* whether semi-sync is switched */

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

  /* In semi-sync replication, reports up to which binlog position we have
   * received replies from the slave indicating that it already get the events
   * or that was skipped in the master.
   *
   * Input:
   *  server_id     - (IN)  master server id number
   *  log_file_name - (IN)  binlog file name
   *  end_offset    - (IN)  the offset in the binlog file up to which we have
   *                        the replies from the slave or that was skipped
   *  skipped_event - (IN)  if the event was skipped
   *
   * Return:
   *  0: success;  non-zero: error
   */
  int reportReplyBinlog(uint32 server_id,
                        const char* log_file_name,
                        my_off_t end_offset,
                        bool skipped_event= false);

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
   *  0: success;  non-zero: error
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
   *  0: success;  non-zero: error
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
   *  0: success;  non-zero: error
   */
  int writeTranxInBinlog(const char* log_file_name, my_off_t log_file_pos);

  /* Read the slave's reply so that we know how much progress the slave makes
   * on receive replication events.
   * 
   * Input:
   *  net          - (IN)  the connection to master
   *  server_id    - (IN)  master server id number
   *  event_buf    - (IN)  pointer to the event packet
   *
   * Return:
   *  0: success;  non-zero: error
   */
  int readSlaveReply(NET *net, uint32 server_id, const char *event_buf);

  /* In semi-sync replication, this method simulates the reception of
   * an reply and executes reportReplyBinlog directly when a transaction
   * is skipped in the master.
   *
   * Input:
   *  event_buf     - (IN)  pointer to the event packet
   *  server_id     - (IN)  master server id numbe
   *  log_file_name - (IN)  the event ending position's file name
   *  log_file_pos  - (IN)  the event ending position's file offset
   *
   * Return:
   *  0: success;  non-zero: error
   */
  int skipSlaveReply(const char *event_buf, uint32 server_id,
                     const char* log_file_name, my_off_t log_file_pos);

  /* Export internal statistics for semi-sync replication. */
  void setExportStats();

  /* 'reset master' command is issued from the user and semi-sync need to
   * go off for that.
   */
  int resetMaster();
};

/* System and status variables for the master component */
extern char rpl_semi_sync_master_enabled;
extern char rpl_semi_sync_master_status;
extern unsigned long rpl_semi_sync_master_clients;
extern unsigned long rpl_semi_sync_master_timeout;
extern unsigned long rpl_semi_sync_master_trace_level;
extern unsigned long rpl_semi_sync_master_yes_transactions;
extern unsigned long rpl_semi_sync_master_no_transactions;
extern unsigned long rpl_semi_sync_master_off_times;
extern unsigned long rpl_semi_sync_master_wait_timeouts;
extern unsigned long rpl_semi_sync_master_timefunc_fails;
extern unsigned long rpl_semi_sync_master_num_timeouts;
extern unsigned long rpl_semi_sync_master_wait_sessions;
extern unsigned long rpl_semi_sync_master_wait_pos_backtraverse;
extern unsigned long rpl_semi_sync_master_avg_trx_wait_time;
extern unsigned long rpl_semi_sync_master_avg_net_wait_time;
extern unsigned long long rpl_semi_sync_master_net_wait_num;
extern unsigned long long rpl_semi_sync_master_trx_wait_num;
extern unsigned long long rpl_semi_sync_master_net_wait_time;
extern unsigned long long rpl_semi_sync_master_trx_wait_time;

/*
  This indicates whether we should keep waiting if no semi-sync slave
  is available.
     0           : stop waiting if detected no avaialable semi-sync slave.
     1 (default) : keep waiting until timeout even no available semi-sync slave.
*/
extern char rpl_semi_sync_master_wait_no_slave;

#endif /* SEMISYNC_MASTER_H */
