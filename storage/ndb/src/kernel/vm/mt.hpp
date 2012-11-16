/* Copyright (C) 2008 MySQL AB, 2008, 2009 Sun Microsystems, Inc.
    All rights reserved. Use is subject to license terms.

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

#ifndef ndb_mt_hpp
#define ndb_mt_hpp

#include <kernel_types.h>
#include <TransporterDefinitions.hpp>

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

#endif
