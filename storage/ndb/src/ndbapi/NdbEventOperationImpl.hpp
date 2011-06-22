/*
   Copyright (c) 2003, 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NdbEventOperationImpl_H
#define NdbEventOperationImpl_H

#include <NdbEventOperation.hpp>
#include <signaldata/SumaImpl.hpp>
#include <transporter/TransporterDefinitions.hpp>
#include <NdbRecAttr.hpp>
#include <AttributeHeader.hpp>
#include <UtilBuffer.hpp>

#define NDB_EVENT_OP_MAGIC_NUMBER 0xA9F301B4
//#define EVENT_DEBUG
#ifdef EVENT_DEBUG
#define DBUG_ENTER_EVENT(A) DBUG_ENTER(A)
#define DBUG_RETURN_EVENT(A) DBUG_RETURN(A)
#define DBUG_VOID_RETURN_EVENT DBUG_VOID_RETURN
#define DBUG_PRINT_EVENT(A,B) DBUG_PRINT(A,B)
#define DBUG_DUMP_EVENT(A,B,C) DBUG_DUMP(A,B,C)
#else
#define DBUG_ENTER_EVENT(A)
#define DBUG_RETURN_EVENT(A) return(A)
#define DBUG_VOID_RETURN_EVENT return
#define DBUG_PRINT_EVENT(A,B)
#define DBUG_DUMP_EVENT(A,B,C)
#endif

#undef NDB_EVENT_VERIFY_SIZE
#ifdef VM_TRACE
// not much effect on performance, leave on
#define NDB_EVENT_VERIFY_SIZE
#endif

class NdbEventOperationImpl;

struct EventBufData
{
  union {
    SubTableData *sdata;
    Uint32 *memory;
  };
  LinearSectionPtr ptr[3];
  unsigned sz;
  NdbEventOperationImpl *m_event_op;

  /*
   * Blobs are stored in blob list (m_next_blob) where each entry
   * is list of parts (m_next).  TODO order by part number
   *
   * Processed data (m_used_data, m_free_data) keeps the old blob
   * list intact.  It is reconsumed when new data items are needed.
   *
   * Data item lists keep track of item count and sum(sz) and
   * these include both main items and blob parts.
   */

  EventBufData *m_next; // Next wrt to global order or Next blob part
  EventBufData *m_next_blob; // First part in next blob

  EventBufData *m_next_hash; // Next in per-GCI hash
  Uint32 m_pkhash; // PK hash (without op) for fast compare

  EventBufData() {}

  /*
   * Main item does not include summary of parts (space / performance
   * tradeoff).  The summary is needed when moving single data item.
   * It is not needed when moving entire list.
   */
  void get_full_size(Uint32 & full_count, Uint32 & full_sz) const {
    full_count = 1;
    full_sz = sz;
    if (m_next_blob != 0)
      add_part_size(full_count, full_sz);
  }
  void add_part_size(Uint32 & full_count, Uint32 & full_sz) const;
};

class EventBufData_list
{
public:
  EventBufData_list();
  ~EventBufData_list();

  // remove first and return its size
  void remove_first(Uint32 & full_count, Uint32 & full_sz);
  // for remove+append avoid double call to get_full_size()
  void append_used_data(EventBufData *data, Uint32 full_count, Uint32 full_sz);
  // append data and insert data but ignore Gci_op list
  void append_used_data(EventBufData *data);
  // append data and insert data into Gci_op list with add_gci_op
  void append_data(EventBufData *data);
  // append list to another, will call move_gci_ops
  void append_list(EventBufData_list *list, Uint64 gci);

  int is_empty();

  EventBufData *m_head, *m_tail;
  Uint32 m_count;
  Uint32 m_sz;

  /*
    distinct ops per gci (assume no hash needed)

    list may be in 2 versions

    1. single list with on gci only
    - one linear array
    Gci_op  *m_gci_op_list;
    Uint32 m_gci_op_count;
    Uint32 m_gci_op_alloc != 0;

    2. multi list with several gcis
    - linked list of gci's
    - one linear array per gci
    Gci_ops *m_gci_ops_list;
    Gci_ops *m_gci_ops_list_tail;
    Uint32 m_is_not_multi_list == 0;

  */
  struct Gci_op                 // 1 + 2
  {
    NdbEventOperationImpl* op;
    Uint32 event_types;
  };
  struct Gci_ops                // 2
  {
    Gci_ops()
      : m_gci(0),
        m_consistent(true),
        m_gci_op_list(NULL),
        m_next(NULL),
        m_gci_op_count(0)
      {};
    ~Gci_ops() {};

    Uint64 m_gci;
    bool m_consistent;
    Gci_op *m_gci_op_list;
    Gci_ops *m_next;
    Uint32 m_gci_op_count;
  };
  union
  {
    Gci_op  *m_gci_op_list;      // 1
    Gci_ops *m_gci_ops_list;     // 2
  };
  union
  {
    Uint32 m_gci_op_count;       // 1
    Gci_ops *m_gci_ops_list_tail;// 2
  };
  union
  {
    Uint32 m_gci_op_alloc;       // 1
    Uint32 m_is_not_multi_list;  // 2
  };
  Gci_ops *first_gci_ops();
  Gci_ops *delete_next_gci_ops();
  // case 1 above; add Gci_op to single list
  void add_gci_op(Gci_op g);
private:
  // case 2 above; move single list or multi list from
  // one list to another
  void move_gci_ops(EventBufData_list *list, Uint64 gci);
};

inline
EventBufData_list::EventBufData_list()
  : m_head(0), m_tail(0),
    m_count(0),
    m_sz(0),
    m_gci_op_list(NULL),
    m_gci_ops_list_tail(0),
    m_gci_op_alloc(0)
{
  DBUG_ENTER_EVENT("EventBufData_list::EventBufData_list");
  DBUG_PRINT_EVENT("info", ("this: %p", this));
  DBUG_VOID_RETURN_EVENT;
}

inline
EventBufData_list::~EventBufData_list()
{
  DBUG_ENTER_EVENT("EventBufData_list::~EventBufData_list");
  DBUG_PRINT_EVENT("info", ("this: %p  m_is_not_multi_list: %u",
                            this, m_is_not_multi_list));
  if (m_is_not_multi_list)
  {
    DBUG_PRINT_EVENT("info", ("delete m_gci_op_list: %p", m_gci_op_list));
    delete [] m_gci_op_list;
  }
  else
  {
    Gci_ops *op = first_gci_ops();
    while (op)
      op = delete_next_gci_ops();
  }
  DBUG_VOID_RETURN_EVENT;
}

inline
int EventBufData_list::is_empty()
{
  return m_head == 0;
}

inline
void EventBufData_list::remove_first(Uint32 & full_count, Uint32 & full_sz)
{
  m_head->get_full_size(full_count, full_sz);
#ifdef VM_TRACE
  assert(m_count >= full_count);
  assert(m_sz >= full_sz);
#endif
  m_count -= full_count;
  m_sz -= full_sz;
  m_head = m_head->m_next;
  if (m_head == 0)
    m_tail = 0;
}

inline
void EventBufData_list::append_used_data(EventBufData *data, Uint32 full_count, Uint32 full_sz)
{
  data->m_next = 0;
  if (m_tail)
    m_tail->m_next = data;
  else
  {
#ifdef VM_TRACE
    assert(m_head == 0);
    assert(m_count == 0);
    assert(m_sz == 0);
#endif
    m_head = data;
  }
  m_tail = data;

  m_count += full_count;
  m_sz += full_sz;
}

inline
void EventBufData_list::append_used_data(EventBufData *data)
{
  Uint32 full_count, full_sz;
  data->get_full_size(full_count, full_sz);
  append_used_data(data, full_count, full_sz);
}

inline
void EventBufData_list::append_data(EventBufData *data)
{
  Gci_op g = { data->m_event_op, 
	       1 << SubTableData::getOperation(data->sdata->requestInfo) };
  add_gci_op(g);

  append_used_data(data);
}

inline EventBufData_list::Gci_ops *
EventBufData_list::first_gci_ops()
{
  assert(!m_is_not_multi_list);
  return m_gci_ops_list;
}

inline EventBufData_list::Gci_ops *
EventBufData_list::delete_next_gci_ops()
{
  assert(!m_is_not_multi_list);
  Gci_ops *first = m_gci_ops_list;
  m_gci_ops_list = first->m_next;
  if (first->m_gci_op_list)
  {
    DBUG_PRINT_EVENT("info", ("this: %p  delete m_gci_op_list: %p",
                              this, first->m_gci_op_list));
    delete [] first->m_gci_op_list;
  }
  delete first;
  if (m_gci_ops_list == 0)
    m_gci_ops_list_tail = 0;
  return m_gci_ops_list;
}

// GCI bucket has also a hash over data, with key event op, table PK.
// It can only be appended to and is invalid after remove_first().
class EventBufData_hash
{
public:
  struct Pos { // search result
    Uint32 index;       // index into hash array
    EventBufData* data; // non-zero if found
    Uint32 pkhash;      // PK hash
  };

  static Uint32 getpkhash(NdbEventOperationImpl* op, LinearSectionPtr ptr[3]);
  static bool getpkequal(NdbEventOperationImpl* op, LinearSectionPtr ptr1[3], LinearSectionPtr ptr2[3]);

  void search(Pos& hpos, NdbEventOperationImpl* op, LinearSectionPtr ptr[3]);
  void append(Pos& hpos, EventBufData* data);

  enum { GCI_EVENT_HASH_SIZE = 101 };
  EventBufData* m_hash[GCI_EVENT_HASH_SIZE];
};

inline
void EventBufData_hash::append(Pos& hpos, EventBufData* data)
{
  data->m_next_hash = m_hash[hpos.index];
  m_hash[hpos.index] = data;
}

struct Gci_container
{
  enum State 
  {
    GC_COMPLETE     = 0x1, // GCI is complete, but waiting for out of order
    GC_INCONSISTENT = 0x2  // GCI might be missing event data
    ,GC_CHANGE_CNT  = 0x4  // Change m_total_buckets
  };

  
  Uint16 m_state;
  Uint16 m_gcp_complete_rep_count; // Remaining SUB_GCP_COMPLETE_REP until done
  Uint64 m_gci;                    // GCI
  EventBufData_list m_data;
  EventBufData_hash m_data_hash;
};

struct Gci_container_pod
{
  char data[sizeof(Gci_container)];
};

class NdbEventOperationImpl : public NdbEventOperation {
public:
  NdbEventOperationImpl(NdbEventOperation &f,
			Ndb *theNdb, 
			const char* eventName);
  NdbEventOperationImpl(Ndb *theNdb, 
			NdbEventImpl& evnt);
  void init(NdbEventImpl& evnt);
  NdbEventOperationImpl(NdbEventOperationImpl&); //unimplemented
  NdbEventOperationImpl& operator=(const NdbEventOperationImpl&); //unimplemented
  ~NdbEventOperationImpl();

  NdbEventOperation::State getState();

  int execute();
  int execute_nolock();
  int stop();
  NdbRecAttr *getValue(const char *colName, char *aValue, int n);
  NdbRecAttr *getValue(const NdbColumnImpl *, char *aValue, int n);
  NdbBlob *getBlobHandle(const char *colName, int n);
  NdbBlob *getBlobHandle(const NdbColumnImpl *, int n);
  Uint32 get_blob_part_no(bool hasDist);
  int readBlobParts(char* buf, NdbBlob* blob,
                    Uint32 part, Uint32 count, Uint16* lenLoc);
  int receive_event();
  bool tableNameChanged() const;
  bool tableFrmChanged() const;
  bool tableFragmentationChanged() const;
  bool tableRangeListChanged() const;
  Uint64 getGCI();
  Uint32 getAnyValue() const;
  Uint64 getLatestGCI();
  Uint64 getTransId() const;
  bool execSUB_TABLE_DATA(const NdbApiSignal * signal,
                          const LinearSectionPtr ptr[3]);

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

  NdbBlob* theBlobList;
  NdbEventOperationImpl* theBlobOpList; // in main op, list of blob ops
  NdbEventOperationImpl* theMainOp; // in blob op, the main op
  int theBlobVersion; // in blob op, NDB_BLOB_V1 or NDB_BLOB_V2

  NdbEventOperation::State m_state; /* note connection to mi_type */
  Uint32 mi_type; /* should be == 0 if m_state != EO_EXECUTING
		   * else same as in EventImpl
		   */
  Uint32 m_eventId;
  Uint32 m_oid;

  /*
    when parsed gci > m_stop_gci it is safe to drop operation
    as kernel will not have any more references
  */
  Uint64 m_stop_gci;

  /*
    m_ref_count keeps track of outstanding references to an event
    operation impl object.  To make sure that the object is not
    deleted too early.

    If on dropEventOperation there are still references to an
    object it is queued for delete in NdbEventBuffer::m_dropped_ev_op
  
    the following references exists for a _non_ blob event op:
    * user reference
    - add    - NdbEventBuffer::createEventOperation
    - remove - NdbEventBuffer::dropEventOperation
    * kernel reference
    - add    - execute_nolock
    - remove - TE_STOP, TE_CLUSTER_FAILURE
    * blob reference
    - add    - execute_nolock on blob event
    - remove - TE_STOP, TE_CLUSTER_FAILURE on blob event
    * gci reference
    - add    - insertDataL/add_gci_op
    - remove - NdbEventBuffer::deleteUsedEventOperations

    the following references exists for a blob event op:
    * kernel reference
    - add    - execute_nolock
    - remove - TE_STOP, TE_CLUSTER_FAILURE    
   */

  int m_ref_count;
  bool m_mergeEvents;
  
  EventBufData *m_data_item;

  void *m_custom_data;
  int m_has_error;

  Uint32 m_fragmentId;
  UtilBuffer m_buffer;

  // Bit mask for what has changed in a table (for TE_ALTER event)
  Uint32 m_change_mask;

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

  Uint32 m_total_buckets;
  Uint16 m_min_gci_index;
  Uint16 m_max_gci_index;
  Vector<Uint64> m_known_gci;
  Vector<Gci_container_pod> m_active_gci;
  STATIC_CONST( ACTIVE_GCI_DIRECTORY_SIZE = 4 );
  STATIC_CONST( ACTIVE_GCI_MASK = ACTIVE_GCI_DIRECTORY_SIZE - 1 );

  NdbEventOperation *createEventOperation(const char* eventName,
					  NdbError &);
  NdbEventOperationImpl *createEventOperationImpl(NdbEventImpl& evnt,
                                                  NdbError &);
  void dropEventOperation(NdbEventOperation *);
  static NdbEventOperationImpl* getEventOperationImpl(NdbEventOperation* tOp);

  void add_drop_lock() { NdbMutex_Lock(m_add_drop_mutex); }
  void add_drop_unlock() { NdbMutex_Unlock(m_add_drop_mutex); }
  void lock() { NdbMutex_Lock(m_mutex); }
  void unlock() { NdbMutex_Unlock(m_mutex); }

  void add_op();
  void remove_op();
  void init_gci_containers();

  // accessed from the "receive thread"
  int insertDataL(NdbEventOperationImpl *op,
		  const SubTableData * const sdata, Uint32 len,
		  LinearSectionPtr ptr[3]);
  void execSUB_GCP_COMPLETE_REP(const SubGcpCompleteRep * const, Uint32 len,
                                int complete_cluster_failure= 0);
  void complete_outof_order_gcis();
  
  void report_node_failure_completed(Uint32 node_id);

  // used by user thread 
  Uint64 getLatestGCI();
  Uint32 getEventId(int bufferId);

  int pollEvents(int aMillisecondNumber, Uint64 *latestGCI= 0);
  int flushIncompleteEvents(Uint64 gci);
  NdbEventOperation *nextEvent();
  bool isConsistent(Uint64& gci);
  bool isConsistentGCI(Uint64 gci);

  NdbEventOperationImpl* getGCIEventOperations(Uint32* iter,
                                               Uint32* event_types);
  void deleteUsedEventOperations(Uint64 last_consumed_gci);

  NdbEventOperationImpl *move_data();

  // routines to copy/merge events
  EventBufData* alloc_data();
  int alloc_mem(EventBufData* data,
                LinearSectionPtr ptr[3],
                Uint32 * change_sz);
  void dealloc_mem(EventBufData* data,
                   Uint32 * change_sz);
  int copy_data(const SubTableData * const sdata, Uint32 len,
                LinearSectionPtr ptr[3],
                EventBufData* data,
                Uint32 * change_sz);
  int merge_data(const SubTableData * const sdata, Uint32 len,
                 LinearSectionPtr ptr[3],
                 EventBufData* data,
                 Uint32 * change_sz);
  int get_main_data(Gci_container* bucket,
                    EventBufData_hash::Pos& hpos,
                    EventBufData* blob_data);
  void add_blob_data(Gci_container* bucket,
                     EventBufData* main_data,
                     EventBufData* blob_data);

  void free_list(EventBufData_list &list);

  void reportStatus();

  // Global Mutex used for some things
  static NdbMutex *p_add_drop_mutex;

#ifdef VM_TRACE
  const char *m_latest_command;
  Uint64 m_flush_gci;
#endif

  Ndb *m_ndb;

  // "latest gci" variables updated in receiver thread
  Uint64 m_latestGCI;           // latest GCI completed in order
  Uint64 m_latest_complete_GCI; // latest complete GCI (in case of outof order)
  Uint64 m_highest_sub_gcp_complete_GCI; // highest gci seen in api
  // "latest gci" variables updated in user thread
  Uint64 m_latest_poll_GCI; // latest gci handed over to user thread

  bool m_startup_hack;

  NdbMutex *m_mutex;
  struct NdbCondition *p_cond;

  // receive thread
  Gci_container m_complete_data;
  EventBufData *m_free_data;
#ifdef VM_TRACE
  Uint32 m_free_data_count;
#endif
  Uint32 m_free_data_sz;

  // user thread
  EventBufData_list m_available_data;
  EventBufData_list m_used_data;

  unsigned m_total_alloc; // total allocated memory

  // threshholds to report status
  unsigned m_free_thresh, m_min_free_thresh, m_max_free_thresh;
  unsigned m_gci_slip_thresh;

  NdbError m_error;

#ifdef VM_TRACE
  static void verify_size(const EventBufData* data, Uint32 count, Uint32 sz);
  static void verify_size(const EventBufData_list & list);
#endif

private:
  void insert_event(NdbEventOperationImpl* impl,
                    SubTableData &data,
                    LinearSectionPtr *ptr,
                    Uint32 &oid_ref);
  
  int expand(unsigned sz);

  // all allocated data
  struct EventBufData_chunk
  {
    unsigned sz;
    EventBufData data[1];
  };
  Vector<EventBufData_chunk *> m_allocated_data;
  unsigned m_sz;

  /*
    dropped event operations (dropEventOperation) that have not yet
    been deleted because of outstanding m_ref_count

    check for delete is done on occations when the ref_count may have
    changed by calling deleteUsedEventOperations:
    - nextEvent - each time the user has completed processing a gci
  */
  NdbEventOperationImpl *m_dropped_ev_op;

  Uint32 m_active_op_count;
  NdbMutex *m_add_drop_mutex;

  inline Gci_container* find_bucket(Uint64 gci){
    Uint32 pos = (Uint32)(gci & ACTIVE_GCI_MASK);
    Gci_container *bucket= ((Gci_container*)(m_active_gci.getBase())) + pos;
    if(likely(gci == bucket->m_gci))
      return bucket;

    return find_bucket_chained(gci);
  }

#ifdef VM_TRACE
  void verify_known_gci(bool allowempty);
#endif
  Gci_container* find_bucket_chained(Uint64 gci);
  void complete_bucket(Gci_container*);
  bool find_max_known_gci(Uint64 * res) const;
  void resize_known_gci();

  Bitmask<(unsigned int)_NDB_NODE_BITMASK_SIZE> m_alive_node_bit_mask;

  void handle_change_nodegroup(const SubGcpCompleteRep*);

public:
  void set_total_buckets(Uint32);
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
