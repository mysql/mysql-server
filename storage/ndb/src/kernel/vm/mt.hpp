#include <kernel_types.h>
#include <TransporterDefinitions.hpp>

#ifndef ndb_mt_hpp
#define ndb_mt_hpp


/*
  For now, we use locks to only have one thread at the time running in the
  transporter as sender, and only one as receiver.

  Thus, we can use a global variable to record the id of the current
  transporter threads. Only valid while holding the transporter receive lock.
*/
extern Uint32 receiverThreadId;

/* Assign block instances to thread */
void add_thr_map(Uint32 block, Uint32 instance, Uint32 thr_no);
void add_main_thr_map();
void add_lqh_worker_thr_map(Uint32 block, Uint32 instance);

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

#endif
