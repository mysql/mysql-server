/* Copyright (c) 2008, 2016, Oracle and/or its affiliates. All rights reserved.

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

#ifndef ndb_mt_hpp
#define ndb_mt_hpp

#include <kernel_types.h>
#include <TransporterDefinitions.hpp>
#include <portlib/NdbTick.h>
#include <SimulatedBlock.hpp>

#define JAM_FILE_ID 275


#define NUM_MAIN_THREADS 2 // except receiver
/*
  MAX_BLOCK_THREADS need not include the send threads since it's
  used to set size of arrays used by all threads that contains a
  job buffer and executes signals. The send threads only sends
  messages directed to other nodes and contains no blocks and
  executes thus no signals.
*/
#define MAX_BLOCK_THREADS (NUM_MAIN_THREADS +       \
                           MAX_NDBMT_LQH_THREADS +  \
                           MAX_NDBMT_TC_THREADS +   \
                           MAX_NDBMT_RECEIVE_THREADS)

Uint32 mt_get_instance_count(Uint32 block);

/* Assign block instances to thread */
void mt_init_thr_map();
void mt_add_thr_map(Uint32 block, Uint32 instance);
void mt_finalize_thr_map();

void sendlocal(Uint32 self, const struct SignalHeader *s,
               const Uint32 *data, const Uint32 secPtr[3]);
void sendprioa(Uint32 self, const struct SignalHeader *s,
               const Uint32 *data, const Uint32 secPtr[3]);
void senddelay(Uint32 thr_no, const struct SignalHeader*, Uint32 delay);
void mt_execSTOP_FOR_CRASH();

/**
 * Interface methods to SimulatedBlock for ndbtmd.
 */
void mt_getSendBufferLevel(Uint32 self, NodeId node, SB_LevelType &level);
Uint32 mt_getSignalsInJBB(Uint32 self);
NDB_TICKS mt_getHighResTimer(Uint32 self);
void mt_setNeighbourNode(NodeId node);
void mt_setWakeupThread(Uint32 self, Uint32 wakeup_instance);
void mt_setOverloadStatus(Uint32 self,
                         OverloadStatus new_status);
void mt_setNodeOverloadStatus(Uint32 self,
                             OverloadStatus new_status);
void mt_setSendNodeOverloadStatus(OverloadStatus new_status);
void mt_getPerformanceTimers(Uint32 self,
                             Uint64 & micros_sleep,
                             Uint64 & spin_time,
                             Uint64 & buffer_full_sleep,
                             Uint64 & micros_send);
Uint32 mt_getSpintime(Uint32 self);
const char *mt_getThreadName(Uint32 self);
const char *mt_getThreadDescription(Uint32 self);
void mt_getSendPerformanceTimers(Uint32 send_instance,
                                 Uint64 & exec_time,
                                 Uint64 & sleep_time,
                                 Uint64 & spin_time,
                                 Uint64 & user_time_os,
                                 Uint64 & kernel_time_os,
                                 Uint64 & elapsed_time_os);
Uint32 mt_getNumSendThreads();
Uint32 mt_getNumThreads();

SendStatus mt_send_remote(Uint32 self, const SignalHeader *sh, Uint8 prio,
                          const Uint32 *data, NodeId nodeId,
                          const LinearSectionPtr ptr[3]);
SendStatus mt_send_remote(Uint32 self, const SignalHeader *sh, Uint8 prio,
                          const Uint32 *data, NodeId nodeId,
                          class SectionSegmentPool *thePool,
                          const SegmentedSectionPtr ptr[3]);

/**
 * Lock/unlock pools for long signal section(s)
 */
void mt_section_lock();
void mt_section_unlock();

int mt_checkDoJob(Uint32 receiver_thread_idx);

/**
 * Are we (not) multi threaded
 */
bool NdbIsMultiThreaded();

/**
 * Get list of BlockReferences so that
 *   each thread holding an instance of any block in blocks[] get "covered"
 *   (excluding ownThreadId
 *
 * eg. calling it with DBLQH, will return a block-reference to *a* block
 *     in each of the threads that has an DBLQH instance
 */
Uint32 mt_get_thread_references_for_blocks(const Uint32 blocks[],
                                           Uint32 ownThreadId,
                                           Uint32 dst[], Uint32 len);

/**
 * wakeup thread running block
 */
void mt_wakeup(class SimulatedBlock*);

#ifdef VM_TRACE
/**
 * Assert that thread calling this function is "owner" of block instance
 */
void mt_assert_own_thread(class SimulatedBlock*);
#endif

/**
 * return list of references running in this thread
 */
Uint32
mt_get_blocklist(class SimulatedBlock*, Uint32 dst[], Uint32 len);


struct ndb_thr_stat
{
  Uint32 thr_no;
  Uint64 os_tid;
  const char * name;
  Uint64 loop_cnt;
  Uint64 exec_cnt;
  Uint64 wait_cnt;
  Uint64 local_sent_prioa;
  Uint64 local_sent_priob;
  Uint64 remote_sent_prioa;
  Uint64 remote_sent_priob;
};

void
mt_get_thr_stat(class SimulatedBlock *, ndb_thr_stat* dst);

/**
 * Get TransporterReceiveHandle for a specific trpman instance
 *   Currently used for error insert that block/unblock traffic
 */
class TransporterReceiveHandle *
mt_get_trp_receive_handle(unsigned instance);

/**
 * return receiver thread handling a particular node
 *   returned number is indexed from 0 and upwards to #receiver threads
 *   (or MAX_NODES is none)
 */
Uint32
mt_get_recv_thread_idx(NodeId nodeId);


#undef JAM_FILE_ID

#endif
