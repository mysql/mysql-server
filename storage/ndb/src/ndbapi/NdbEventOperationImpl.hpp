/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef NdbEventOperationImpl_H
#define NdbEventOperationImpl_H

#include <NdbEventOperation.hpp>
#include <signaldata/SumaImpl.hpp>
#include <transporter/TransporterDefinitions.hpp>
#include <NdbRecAttr.hpp>

#define NDB_EVENT_OP_MAGIC_NUMBER 0xA9F301B4

class NdbEventOperationImpl;
struct EventBufData
{
  union {
    SubTableData *sdata;
    char *memory;
  };
  LinearSectionPtr ptr[3];
  unsigned sz;
  NdbEventOperationImpl *m_event_op;
  EventBufData *m_next; // Next wrt to global order
};

class EventBufData_list
{
public:
  EventBufData_list();
  ~EventBufData_list();

  void remove_first();
  void append(EventBufData *data);
  void append(const EventBufData_list &list);

  int is_empty();

  EventBufData *m_head, *m_tail;
  unsigned m_count;
  unsigned m_sz;
};

inline
EventBufData_list::EventBufData_list()
  : m_head(0), m_tail(0),
    m_count(0),
    m_sz(0)
{
}

inline
EventBufData_list::~EventBufData_list()
{
}


inline
int EventBufData_list::is_empty()
{
  return m_head == 0;
}

inline
void EventBufData_list::remove_first()
{
  m_count--;
  m_sz-= m_head->sz;
  m_head= m_head->m_next;
  if (m_head == 0)
    m_tail= 0;
}

inline
void EventBufData_list::append(EventBufData *data)
{
  data->m_next= 0;
  if (m_tail)
    m_tail->m_next= data;
  else
  {
#ifdef VM_TRACE
    assert(m_count == 0);
    assert(m_sz == 0);
#endif
    m_head= data;
  }
  m_tail= data;

  m_count++;
  m_sz+= data->sz;
}

inline
void EventBufData_list::append(const EventBufData_list &list)
{
  if (m_tail)
    m_tail->m_next= list.m_head;
  else
    m_head= list.m_head;
  m_tail= list.m_tail;
  m_count+= list.m_count;
  m_sz+= list.m_sz;
}

struct Gci_container
{
  enum State 
  {
    GC_COMPLETE = 0x1 // GCI is complete, but waiting for out of order
  };
  
  Uint32 m_state;
  Uint32 m_gcp_complete_rep_count; // Remaining SUB_GCP_COMPLETE_REP until done
  Uint64 m_gci;                    // GCI
  EventBufData_list m_data;
};

class NdbEventOperationImpl : public NdbEventOperation {
public:
  NdbEventOperationImpl(NdbEventOperation &N,
			Ndb *theNdb, 
			const char* eventName);
  NdbEventOperationImpl(NdbEventOperationImpl&); //unimplemented
  NdbEventOperationImpl& operator=(const NdbEventOperationImpl&); //unimplemented
  ~NdbEventOperationImpl();

  NdbEventOperation::State getState();

  int execute();
  int stop();
  NdbRecAttr *getValue(const char *colName, char *aValue, int n);
  NdbRecAttr *getValue(const NdbColumnImpl *, char *aValue, int n);
  int receive_event();
  Uint64 getGCI();
  Uint64 getLatestGCI();

  NdbDictionary::Event::TableEvent getEventType();

  void print();
  void printAll();

  NdbEventOperation *m_facade;
  Uint32 m_magic_number;

  const NdbError & getNdbError() const;
  NdbError m_error;

  Ndb *m_ndb;
  NdbEventImpl *m_eventImpl;

  NdbRecAttr *theFirstPkAttrs[2];
  NdbRecAttr *theCurrentPkAttrs[2];
  NdbRecAttr *theFirstDataAttrs[2];
  NdbRecAttr *theCurrentDataAttrs[2];

  NdbEventOperation::State m_state; /* note connection to mi_type */
  Uint32 mi_type; /* should be == 0 if m_state != EO_EXECUTING
		   * else same as in EventImpl
		   */
  Uint32 m_eventId;
  Uint32 m_oid;
  
  EventBufData *m_data_item;

  void *m_custom_data;
  int m_has_error;

#ifdef VM_TRACE
  Uint32 m_data_done_count;
  Uint32 m_data_count;
#endif

  // managed by the ndb object
  NdbEventOperationImpl *m_next;
  NdbEventOperationImpl *m_prev;
private:
  void receive_data(NdbRecAttr *r, const Uint32 *data, Uint32 sz);
};


class NdbEventBuffer {
public:
  NdbEventBuffer(Ndb*);
  ~NdbEventBuffer();

  const Uint32 &m_system_nodes;
  Vector<Gci_container> m_active_gci;
  NdbEventOperation *createEventOperation(const char* eventName,
					  NdbError &);
  void dropEventOperation(NdbEventOperation *);
  static NdbEventOperationImpl* getEventOperationImpl(NdbEventOperation* tOp);

  void add_drop_lock();
  void add_drop_unlock();
  void lock();
  void unlock();

  void add_op();
  void remove_op();
  void init_gci_containers();
  Uint32 m_active_op_count;

  // accessed from the "receive thread"
  int insertDataL(NdbEventOperationImpl *op,
		  const SubTableData * const sdata,
		  LinearSectionPtr ptr[3]);
  void execSUB_GCP_COMPLETE_REP(const SubGcpCompleteRep * const rep);
  void complete_outof_order_gcis();
  
  void reportClusterFailed(NdbEventOperationImpl *op);
  void completeClusterFailed();

  // used by user thread 
  Uint64 getLatestGCI();
  Uint32 getEventId(int bufferId);

  int pollEvents(int aMillisecondNumber, Uint64 *latestGCI= 0);
  NdbEventOperation *nextEvent();

  NdbEventOperationImpl *move_data();

  // used by both user thread and receive thread
  int copy_data_alloc(const SubTableData * const f_sdata,
		      LinearSectionPtr f_ptr[3],
		      EventBufData *ev_buf);

  void free_list(EventBufData_list &list);

  void reportStatus();

  // Global Mutex used for some things
  static NdbMutex *p_add_drop_mutex;

#ifdef VM_TRACE
  const char *m_latest_command;
#endif

  Ndb *m_ndb;
  Uint64 m_latestGCI;           // latest "handover" GCI
  Uint64 m_latest_complete_GCI; // latest complete GCI (in case of outof order)

  NdbMutex *m_mutex;
  struct NdbCondition *p_cond;

  // receive thread
  Gci_container m_complete_data;
  EventBufData *m_free_data;
#ifdef VM_TRACE
  unsigned m_free_data_count;
#endif
  unsigned m_free_data_sz;

  // user thread
  EventBufData_list m_available_data;
  EventBufData_list m_used_data;

  unsigned m_total_alloc; // total allocated memory

  // threshholds to report status
  unsigned m_free_thresh;
  unsigned m_gci_slip_thresh;

  NdbError m_error;
private:
  int expand(unsigned sz);

  // all allocated data
  struct EventBufData_chunk
  {
    unsigned sz;
    EventBufData data[1];
  };
  Vector<EventBufData_chunk *> m_allocated_data;
  unsigned m_sz;

  // dropped event operations that have not yet
  // been deleted
  NdbEventOperationImpl *m_dropped_ev_op;
};

inline
NdbEventOperationImpl*
NdbEventBuffer::getEventOperationImpl(NdbEventOperation* tOp)
{
  return &tOp->m_impl;
}

inline void
NdbEventOperationImpl::receive_data(NdbRecAttr *r,
				    const Uint32 *data,
				    Uint32 sz)
{
  r->receive_data(data,sz);
#if 0
  if (sz)
  {
    assert((r->attrSize() * r->arraySize() + 3) >> 2 == sz);
    r->theNULLind= 0;
    memcpy(r->aRef(), data, 4 * sz);
    return;
  }
  r->theNULLind= 1;
#endif
}

#endif
