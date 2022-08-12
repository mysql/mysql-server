/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NdbEventOperationImpl_H
#define NdbEventOperationImpl_H

#include <cstring>
#include <vector>

#include <NdbMutex.h>
#include <NdbTick.h>
#include <AttributeHeader.hpp>
#include <NdbEventOperation.hpp>
#include <NdbRecAttr.hpp>
#include <UtilBuffer.hpp>
#include <Vector.hpp>
#include <signaldata/SumaImpl.hpp>

#include "my_pointer_arithmetic.h"

#define NDB_EVENT_OP_MAGIC_NUMBER 0xA9F301B4
//#define EVENT_DEBUG
#ifdef EVENT_DEBUG
#define DBUG_ENTER_EVENT(A) DBUG_ENTER(A)
#define DBUG_RETURN_EVENT(A) DBUG_RETURN(A)
#define DBUG_VOID_RETURN_EVENT DBUG_VOID_RETURN
#define DBUG_PRINT_EVENT(A, B) DBUG_PRINT(A, B)
#define DBUG_DUMP_EVENT(A, B, C) DBUG_DUMP(A, B, C)
#else
#define DBUG_ENTER_EVENT(A)
#define DBUG_RETURN_EVENT(A) return (A)
#define DBUG_VOID_RETURN_EVENT return
#define DBUG_PRINT_EVENT(A, B)
#define DBUG_DUMP_EVENT(A, B, C)
#endif

#include <ndb_logevent.h>
typedef enum ndb_logevent_event_buffer_status_report_reason ReportReason;

class NdbEventOperationImpl;
class EpochData;
class EventBufDataHead;

/////////////////////////////////

/**
 * EventBufAllocator is a C++ STL memory allocator.
 *
 * It can be used to construct STL container objects which allocate
 * its memory in the NdbEventBuffer
 */
template <class T>
class EventBufAllocator {
 public:
  typedef T value_type;

  EventBufAllocator(NdbEventBuffer *e) : m_eventBuffer(e) {}

  template <class U>
  constexpr EventBufAllocator(const EventBufAllocator<U> &other) noexcept
      : m_eventBuffer(other.m_eventBuffer) {}

  [[nodiscard]] T *allocate(std::size_t n);
  void deallocate(T *p, std::size_t n) noexcept;

  NdbEventBuffer *const m_eventBuffer;
};

/////////////////////////////////

class EventBufData {
 public:
  union {
    SubTableData *sdata;
    Uint32 *memory;
  };
  LinearSectionPtr ptr[3];
  NdbEventOperationImpl *m_event_op;

  /*
   * Blobs are stored in blob list (m_next_blob) where each entry
   * is list of parts (m_next).  TODO order by part number
   *
   * Data item lists keep track of item count and sum(sz) and
   * these include both main items and blob parts.
   */
  union {  // Next wrt to global order or Next blob part
    EventBufData *m_next;
    EventBufDataHead *m_next_main;
  };
  EventBufData *m_next_blob;  // First part in next blob
  EventBufDataHead *m_main;   // Head of set of events

  EventBufData()
      : memory(nullptr),
        m_event_op(nullptr),
        m_next(nullptr),
        m_next_blob(nullptr) {}

  size_t get_this_size() const;

  // Debug/assert only, else prefer size/count in EventBufDataHead
  Uint32 get_count() const;
  size_t get_size() const;
  Uint64 getGCI() const;
};

/**
 * The 'main' EventBufData aggregates the total volume blob-parts
 * available through the m_next_blob chains.
 */
class EventBufDataHead : public EventBufData {
 public:
  EventBufDataHead() : m_event_count(0), m_data_size(0) {}

  Uint32 m_event_count;
  size_t m_data_size;
};

/**
 * The MonotonicEpoch class provides a monotonic increasing epoch
 * identifier - Even across an initial restart which may start a
 * new sequence of GCIs from 0/0.
 * Several garbage collection mechanism in the EventBuffer relies
 * on the monotonicity of the GCI being used as an 'expiry stamp'
 * for when the object can be permanently deleted.
 */
class MonotonicEpoch {
 public:
  static const MonotonicEpoch min;
  static const MonotonicEpoch max;

  MonotonicEpoch() : m_seq(0), m_epoch(0) {}

  MonotonicEpoch(Uint32 seq, Uint64 epoch) : m_seq(seq), m_epoch(epoch) {}

  bool operator==(const MonotonicEpoch &other) const {
    return m_epoch == other.m_epoch && m_seq == other.m_seq;
  }
  bool operator!=(const MonotonicEpoch &other) const {
    return m_epoch != other.m_epoch || m_seq != other.m_seq;
  }
  bool operator<(const MonotonicEpoch &other) const {
    return m_seq < other.m_seq ||
           (m_seq == other.m_seq && m_epoch < other.m_epoch);
  }
  bool operator<=(const MonotonicEpoch &other) const {
    return m_seq < other.m_seq ||
           (m_seq == other.m_seq && m_epoch <= other.m_epoch);
  }
  bool operator>(const MonotonicEpoch &other) const {
    return m_seq > other.m_seq ||
           (m_seq == other.m_seq && m_epoch > other.m_epoch);
  }
  bool operator>=(const MonotonicEpoch &other) const {
    return m_seq > other.m_seq ||
           (m_seq == other.m_seq && m_epoch >= other.m_epoch);
  }

  Uint64 getGCI() const { return m_epoch; }

  // 'operator <<' is allowed to access privat members
  friend NdbOut &operator<<(NdbOut &out, const MonotonicEpoch &gci);

 private:
  Uint32 m_seq;
  Uint64 m_epoch;
};

/**
 * All memory allocation for events are done from memory blocks.
 * Each memory block is tagged with an 'expiry-epoch', which holds
 * the highest epoch known up to the point where the block got full.
 *
 * No freeing of objects allocated from the memory block is required.
 * Instead we free the entire block when the client has consumed the
 * last event with an epoch >= the 'expiry-epoch' of the memory block.
 */
class EventMemoryBlock {
 public:
  EventMemoryBlock(Uint32 size) : m_size(data_size(size)) { init(); }

  void init() {
    /**
     * Alloc must start from an aligned memory addr, add padding if required.
     * Assumes that EventMemoryBlock itself is correctly aligned.
     */
    const Uint32 data_offs = my_offsetof(EventMemoryBlock, m_data);
    const Uint32 pad = ALIGN_SIZE(data_offs) - data_offs;
    m_used = pad;
    m_expiry_epoch = MonotonicEpoch::max;
    m_next = nullptr;
  }

  void destruct() {
#ifndef NDEBUG
    // Shredd the memory if debugging
    std::memset(m_data, 0x11, m_size);
    m_used = 0;
    m_expiry_epoch = MonotonicEpoch::min;
#endif
  }

  // Allocate a chunk of memory from this MemoryBlock
  void *alloc(unsigned size) {
    if (unlikely(m_used + size > m_size)) return nullptr;

    char *mem = m_data + m_used;
    m_used += ALIGN_SIZE(size);  // Keep alignment for next object
    return (void *)mem;
  }

  // Get remaining free memory from block
  Uint32 get_free() const { return (m_size - m_used); }

  // Get total usable memory size from block (if empty)
  Uint32 get_size() const { return m_size; }

  // Get total size of block as once allocated
  Uint32 alloced_size() const {
    return m_size + my_offsetof(EventMemoryBlock, m_data);
  }

  const Uint32 m_size;  // Number of bytes available to allocate from m_data
  Uint32 m_used;        // Offset of next free position

  /**
   * Highest epoch of any object allocated memory from this block.
   * Entire block expires when all epoch <= expiry_epoch are consumed.
   */
  MonotonicEpoch m_expiry_epoch;

  EventMemoryBlock *m_next;  // Next memory block

 private:
  char m_data[1];

  // Calculates usable size of m_data given total size 'full_sz'
  Uint32 data_size(Uint32 full_sz) {
    return full_sz - my_offsetof(EventMemoryBlock, m_data);
  }
};

// GCI bucket has also a hash over data, with key event op, table PK.
// It can only be appended to and is invalid after remove_first().
class EventBufData_hash {
 public:
  EventBufData_hash(NdbEventBuffer *event_buffer);

  void clear();

  struct Pos {             // Hash head, and search result
    Uint32 pkhash;         // PK hash
    Uint32 event_id;       // Id of event operation
    union {                // hash either blob_data or main_data
      EventBufData *data;  // non-null if found
      EventBufDataHead *main_data;
    };
  };

  void append(const Pos hpos);
  EventBufData *search(Pos &hpos, NdbEventOperationImpl *op,
                       const LinearSectionPtr ptr[3]);

 private:
  // Allocate and move into a larger m_hash[]
  void expand();

  static Uint32 getpkhash(NdbEventOperationImpl *op,
                          const LinearSectionPtr ptr[3]);

  static bool getpkequal(NdbEventOperationImpl *op,
                         const LinearSectionPtr ptr1[3],
                         const LinearSectionPtr ptr2[3]);

  NdbEventBuffer *m_event_buffer;

  // We start out with a m_hash[] of SIZE_MIN.
  // It will expand on demand, being allocated from m_event_buffer.
  static constexpr int GCI_EVENT_HASH_SIZE_MIN = 37;
  static constexpr int GCI_EVENT_HASH_SIZE_MAX = 4711;

  typedef std::vector<Pos, EventBufAllocator<Pos>> HashBucket;
  HashBucket *m_hash;
  size_t m_hash_size;
  size_t m_element_count;
};

/**
 * The Gci_container creates a collection of EventBufData and
 * the NdbEventOperationImpl receiving an event within this
 * specific epoch. Once 'completed', an 'EpochData' is created from
 * the Gci_container, representing a more static view of the
 * epoch ready to be consumed by the client.
 */
struct Gci_op  // A helper
{
  NdbEventOperationImpl *op;
  Uint32 event_types;
  Uint32 cumulative_any_value;  // Merged for table/epoch events
  Uint32 filtered_any_value;    // Filtered union for table/epoch events
};

class Gci_container {
 public:
  Gci_container(NdbEventBuffer *event_buffer = nullptr)
      : m_event_buffer(event_buffer),
        m_state(0),
        m_gcp_complete_rep_count(0),
        m_gcp_complete_rep_sub_data_streams(),
        m_gci(0),
        m_head(nullptr),
        m_tail(nullptr),
        m_data_hash(event_buffer),
        m_gci_op_list(nullptr),
        m_gci_op_count(0),
        m_gci_op_alloc(0) {}

  void clear() {
    assert(m_event_buffer != nullptr);
    m_state = 0;
    m_gcp_complete_rep_count = 0;
    m_gcp_complete_rep_sub_data_streams.clear();
    m_gci = 0;
    m_head = m_tail = nullptr;
    m_data_hash.clear();

    m_gci_op_list = nullptr;
    m_gci_op_count = 0;
    m_gci_op_alloc = 0;
  }

  bool is_empty() const { return (m_head == nullptr); }

  enum State {
    GC_COMPLETE = 0x1  // GCI is complete, but waiting for out of order
    ,
    GC_INCONSISTENT = 0x2  // GCI might be missing event data
    ,
    GC_CHANGE_CNT = 0x4  // Change m_total_buckets
    ,
    GC_OUT_OF_MEMORY = 0x8  // Not enough event buffer memory to buffer data
  };

  NdbEventBuffer *m_event_buffer;  // Owner

  Uint16 m_state;
  Uint16 m_gcp_complete_rep_count;  // Remaining SUB_GCP_COMPLETE_REP until done
  Bitmask<(MAX_SUB_DATA_STREAMS + 31) / 32> m_gcp_complete_rep_sub_data_streams;
  Uint64 m_gci;  // GCI

  EventBufDataHead *m_head, *m_tail;
  EventBufData_hash m_data_hash;

  Gci_op *m_gci_op_list;
  Uint32 m_gci_op_count;  // Current size of gci_op_list[]
  Uint32 m_gci_op_alloc;  // Items allocated in gci_op_list[]

  bool hasError() const {
    return (m_state & (GC_OUT_OF_MEMORY | GC_INCONSISTENT));
  }

  // get number of EventBufData in this Gci_container (For debug)
  Uint32 count_event_data() const;

  // add Gci_op to container for this Gci
  void add_gci_op(Gci_op g);

  // append data and insert data into Gci_op list with add_gci_op
  void append_data(EventBufDataHead *data);

  // Create an EpochData containing the Gci_op and event data added above.
  // This effectively 'completes' the epoch represented by this Gci_container
  EpochData *createEpochData(Uint64 gci);
};

/**
 * An EpochData is created from a Gci_container when it contains a complete
 * epoch. It contains all EventBufData received within this epoch, and
 * a list of all NdbEventOperationImpl which received an event.
 * (Except exceptional events)
 *
 * m_error shows the error identified when receiving an epoch:
 *  a buffer overflow at the sender (ndb suma) or receiver (event buffer).
 *  This error information is a duplicate, same info is available in
 *  the dummy EventBufData. The reason to store the duplicate is to remove
 *  the need to search the EventBufData by isConsistent(Uint64 &) to find
 *  whether an inconsistency has occurred in the epoch stream.
 *  This method is kept for backward compatibility.
 */
class EpochData {
 public:
  EpochData(MonotonicEpoch gci, Gci_op *gci_op_list, Uint32 count,
            EventBufDataHead *data)
      : m_gci(gci),
        m_error(0),
        m_gci_op_count(count),
        m_gci_op_list(gci_op_list),
        m_data(data),
        m_next(nullptr) {}
  ~EpochData() {}

  // get number of EventBufData in EpochDataList (For debug)
  Uint32 count_event_data() const;

  const MonotonicEpoch m_gci;
  Uint32 m_error;
  Uint32 const m_gci_op_count;
  Gci_op *const m_gci_op_list;  // All event_op receiving an event
  EventBufDataHead *m_data;     // All event data within epoch
  EpochData *m_next;            // Next completed epoch
};

/**
 * A list of EpochData in increasing GCI order is prepared for the
 * client to consume. Actually it is a 'list of lists'.
 *  - The EpochDataList presents a list of epoch which has completed.
 *  - Within each epoch the client can navigate the EventBufData
 *    valid for this specific Epoch.
 */
class EpochDataList {
 public:
  EpochDataList() : m_head(nullptr), m_tail(nullptr) {}

  // Gci list is cleared to an empty state.
  void clear() { m_head = m_tail = nullptr; }

  bool is_empty() const { return (m_head == nullptr); }

  // append EpochData to list
  void append(EpochData *epoch) {
    if (m_tail)
      m_tail->m_next = epoch;
    else {
      assert(m_head == nullptr);
      m_head = epoch;
    }
    m_tail = epoch;
  }

  // append list to another
  void append_list(EpochDataList *list) {
    if (m_tail)
      m_tail->m_next = list->m_head;
    else
      m_head = list->m_head;
    m_tail = list->m_tail;

    list->m_head = list->m_tail = nullptr;
  }

  EpochData *first_epoch() const { return m_head; }

  // advance list head to next EpochData
  EpochData *next_epoch() {
    m_head = m_head->m_next;
    if (m_head == nullptr) m_tail = nullptr;

    return m_head;
  }

  // find first event data to be delivered.
  EventBufDataHead *get_first_event_data() const {
    EpochData *epoch = m_head;
    while (epoch != nullptr) {
      if (epoch->m_data != nullptr) return epoch->m_data;
      epoch = epoch->m_next;
    }
    return nullptr;
  }

  // get and consume first EventData
  EventBufDataHead *consume_first_event_data() {
    EpochData *epoch = m_head;
    if (epoch != nullptr) {
      EventBufDataHead *data = epoch->m_data;
      if (data != nullptr) m_head->m_data = data->m_next_main;
      return data;
    }
    return nullptr;
  }

  // get number of EventBufData in EpochDataList (For debug)
  Uint32 count_event_data() const;

  // private:
  EpochData *m_head, *m_tail;
};

class NdbEventOperationImpl : public NdbEventOperation {
 public:
  NdbEventOperationImpl(NdbEventOperation &f, Ndb *ndb,
                        const NdbDictionary::Event *event);
  NdbEventOperationImpl(Ndb *theNdb, NdbEventImpl *evnt);
  void init();
  NdbEventOperationImpl(NdbEventOperationImpl &);  // unimplemented
  NdbEventOperationImpl &operator=(
      const NdbEventOperationImpl &);  // unimplemented
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
  int readBlobParts(char *buf, NdbBlob *blob, Uint32 part, Uint32 count,
                    Uint16 *lenLoc);
  int receive_event();
  bool tableNameChanged() const;
  bool tableFrmChanged() const;
  bool tableFragmentationChanged() const;
  bool tableRangeListChanged() const;
  Uint64 getGCI() const;
  Uint32 getAnyValue() const;
  bool isErrorEpoch(NdbDictionary::Event::TableEvent *error_type);
  bool isEmptyEpoch();
  Uint64 getLatestGCI();
  Uint64 getTransId() const;
  bool execSUB_TABLE_DATA(const NdbApiSignal *signal,
                          const LinearSectionPtr ptr[3]);

  NdbDictionary::Event::TableEvent getEventType2();

  void print();

  NdbEventOperation *m_facade;
  Uint32 m_magic_number;

  const NdbError &getNdbError() const;
  // Allow update error from const methods
  mutable NdbError m_error;

  Ndb *const m_ndb;
  // The Event is owned by pointer to NdbEventImpl->m_facade
  NdbEventImpl *const m_eventImpl;

  NdbRecAttr *theFirstPkAttrs[2];
  NdbRecAttr *theCurrentPkAttrs[2];
  NdbRecAttr *theFirstDataAttrs[2];
  NdbRecAttr *theCurrentDataAttrs[2];

  NdbBlob *theBlobList;
  NdbEventOperationImpl *theBlobOpList;  // in main op, list of blob ops
  NdbEventOperationImpl *theMainOp;      // in blob op, the main op
  int theBlobVersion;  // in blob op, NDB_BLOB_V1 or NDB_BLOB_V2

  NdbEventOperation::State m_state; /* note connection to mi_type */
  Uint32 mi_type;                   /* should be == 0 if m_state != EO_EXECUTING
                                     * else same as in EventImpl
                                     */
  Uint32 m_oid;

  /*
    when parsed gci > m_stop_gci it is safe to drop operation
    as kernel will not have any more references
  */
  MonotonicEpoch m_stop_gci;

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

  // Used for allowing empty updates be passed to the user
  bool m_allow_empty_update;

  // Placeholder for the requestinfo which will be sent in SubStartReq
  Uint32 m_requestInfo;

  // Default implementation that performs no filtering
  static Uint32 default_anyvalue_filter(Uint32 any_value) { return any_value; }
  // Callback for filtering any_value in received row changes
  AnyValueFilterFn m_any_value_filter{default_anyvalue_filter};

 private:
  void receive_data(NdbRecAttr *r, const Uint32 *data, Uint32 sz);
  void print_blob_part_bufs(const NdbBlob *blob, const EventBufData *data,
                            bool hasDist, Uint32 part, Uint32 count) const;
};

class EventBufferManager {
 public:
  EventBufferManager(const Ndb *const m_ndb);
  ~EventBufferManager() {}

 private:
  const Ndb *const m_ndb;
  /* Last epoch that will be buffered completely before
   * the beginning of a gap.
   */
  Uint64 m_pre_gap_epoch;

  /* The epoch where the gap begins. The received event data for this epoch
   * will be thrown. Gcp-completion of this epoch will add a dummy event
   * data and a dummy gci-ops list denoting the problem causing the gap.
   */
  Uint64 m_begin_gap_epoch;

  /* This is the last epoch that will NOT be buffered during the gap period.
   * From the next epoch (post-gap epoch), all event data will be
   * completely buffered.
   */
  Uint64 m_end_gap_epoch;

  // Epochs newer than this will be discarded when event buffer
  // is used up.
  Uint64 m_max_buffered_epoch;

  /* Since no buffering will take place during a gap, m_max_buffered_epoch
   * will not be updated. Therefore, use m_max_received_epoch to
   * find end_gap_epoch when memory becomes available again.
   * Epochs newer than this will be buffered.
   */
  Uint64 m_max_received_epoch;

  /* After the max_alloc limit is hit, the % of event buffer memory
   * that should be available before resuming buffering:
   * min 1, max 99, default 20.
   */
  unsigned m_free_percent;

  enum {
    EBM_COMPLETELY_BUFFERING,
    EBM_PARTIALLY_DISCARDING,
    EBM_COMPLETELY_DISCARDING,
    EBM_PARTIALLY_BUFFERING
  } m_event_buffer_manager_state;

  /**
   * Event buffer manager has 4 states :
   * COMPLETELY_BUFFERING :
   *  all received event data are buffered.
   * Entry condition:
   *  m_pre_gap_epoch = 0 && m_begin_gap_epoch = 0 && m_end_gap_epoch = 0.
   *
   * PARTIALLY_DISCARDING :
   *  event data up to epochs m_pre_gap_epoch are buffered,
   *  others are discarded.
   *  Entry condition:
   *   m_pre_gap_epoch > 0 && m_begin_gap = 0 && m_end_gap_epoch = 0.
   *
   * COMPLETELY_DISCARDING :
   *  all received epoch event data are discarded.
   *  Entry condition:
   *   m_pre_gap_epoch > 0 && m_begin_gap_epoch > 0 && m_end_gap_epoch = 0.
   *
   * PARTIALLY_BUFFERING :
   *  all received event data <= m_end_gap are discarded, others are buffered.
   *  Entry condition:
   *   m_pre_gap_epoch > 0 && m_begin_gap_epoch > 0 && m_end_gap_epoch > 0.
   *
   * Transitions :
   * COMPLETELY_BUFFERING -> PARTIALLY_DISCARDING :
   *  memory is completely used up at the reception of SUB_TABLE_DATA,
   *  Action: m_pre_gap_epoch is set with m_max_buffered_epoch.
   *   ==> An incoming new epoch, which is larger than the
   *       m_max_buffered_epoch can NOT be an m_pre_gap_epoch.
   *
   * PARTIALLY_DISCARDING -> COMPLETELY_DISCARDING :
   *  epoch next to m_pre_gap_epoch, has gcp-completed,
   * Action: set m_begin_gap_epoch with the gcp_completing epoch
   * (marking the beginning of a gap).
   * The reason to have an m_begin_gap in addition to m_pre_gap is:
   * The gci of the epoch next to m_pre_gap is needed for creating the
   * exceptional epoch. We reuse the code in complete_bucket that will
   * create the exceptional epoch. Complete_bucket is called only when
   * an epoch is gcp-completing.
   *
   * COMPLETELY_DISCARDING -> PARTIALLY_BUFFERING :
   *  m_free_percent of the event buffer  becomes available at the
   *  reception of SUB_TABLE_DATA.
   * Action : set m_end_gap_epoch with max_received_epoch
   * (cannot use m_max_buffered_epoch since it has not been updated
   * since PARTIALLY_DISCARDING).
   *
   * PARTIALLY_BUFFERING -> COMPLETELY_BUFFERING :
   *  epoch next to m_end_gap_epoch (post-gap epoch) has buffered
   *  completely and gcp_completed.
   * Action : reset m_pre_gap_epoch, m_begin_gap_epoch and m_end_gap_epoch.
   */

  bool isCompletelyBuffering();
  bool isPartiallyDiscarding();
  bool isCompletelyDiscarding();
  bool isPartiallyBuffering();
  bool isInDiscardingState();

 public:
  unsigned get_eventbuffer_free_percent();
  void set_eventbuffer_free_percent(unsigned free);

  void onBufferingEpoch(Uint64 received_epoch);  // update m_max_buffered_epoch

  /* Execute the state machine by checking the buffer manager state
   * and performing the correct transition according to buffer availability:
   * Returned value indicates whether reportStatus() is necessary.
   * Transitions CB -> PD and CD -> PB and updating m_max_received epoc
   * are performed here.
   */
  ReportReason onEventDataReceived(Uint32 memory_usage_percent,
                                   Uint64 received_epoch);

  // Check whether the received event data can be discarded.
  // Discard-criteria : m_pre_gap_epoch < received_epoch <= m_end_gap_epoch.
  bool isEventDataToBeDiscarded(Uint64 received_epoch);

  /* Execute the state machine by checking the buffer manager state
   * and performing the correct transition according to gcp_completion.
   * Transitions PD -> CD and PB -> CB are performed here.
   * Check whether the received gcp_completing epoch can mark the beginning
   * of a gap (qualifies as m_begin_gap_epoch) or
   * the gap is ended and the transition to COMPLETE_BUFFERING can be performed.
   * The former case sets gap_begins to true.
   */
  ReportReason onEpochCompleted(Uint64 completed_epoch, bool &gap_begins);

  /* Check whether the epoch is in an out-of-memory gap where event
   * data is being discarded
   */
  bool isEpochInOOMGap(Uint64 completed_epoch);
};

class NdbEventBuffer {
 public:
  NdbEventBuffer(Ndb *);
  ~NdbEventBuffer();

  Uint32 m_total_buckets;
  Uint16 m_min_gci_index;
  Uint16 m_max_gci_index;
  Vector<Uint64> m_known_gci;
  Vector<Gci_container> m_active_gci;
  static constexpr Uint32 ACTIVE_GCI_DIRECTORY_SIZE = 4;
  static constexpr Uint32 ACTIVE_GCI_MASK = ACTIVE_GCI_DIRECTORY_SIZE - 1;

  NdbEventOperation *createEventOperation(const char *eventName, NdbError &);
  NdbEventOperationImpl *createEventOperationImpl(NdbEventImpl *evnt,
                                                  NdbError &);
  void dropEventOperation(NdbEventOperation *);
  static NdbEventOperationImpl *getEventOperationImpl(NdbEventOperation *tOp);

  void add_drop_lock() { NdbMutex_Lock(m_add_drop_mutex); }
  void add_drop_unlock() { NdbMutex_Unlock(m_add_drop_mutex); }
  void lock() { NdbMutex_Lock(m_mutex); }
  bool trylock() { return NdbMutex_Trylock(m_mutex) == 0; }
  void unlock() { NdbMutex_Unlock(m_mutex); }

  void add_op();
  void remove_op();
  void init_gci_containers();

  // accessed from the "receive thread"
  int insertDataL(NdbEventOperationImpl *op, const SubTableData *const sdata,
                  Uint32 len, const LinearSectionPtr ptr[3]);
  void execSUB_GCP_COMPLETE_REP(const SubGcpCompleteRep *const, Uint32 len,
                                int complete_cluster_failure = 0);
  void execSUB_START_CONF(const SubStartConf *const, Uint32 len);
  void execSUB_STOP_CONF(const SubStopConf *const, Uint32 len);
  void execSUB_STOP_REF(const SubStopRef *const, Uint32 len);

  void complete_outof_order_gcis();

  void report_node_failure_completed(Uint32 node_id);

  // used by user thread
  Uint64 getLatestGCI();
  Uint32 getEventId(int bufferId);
  Uint64 getHighestQueuedEpoch();
  void setEventBufferQueueEmptyEpoch(bool queue_empty_epoch);

  int pollEvents(Uint64 *HighestQueuedEpoch = nullptr);
  int flushIncompleteEvents(Uint64 gci);

  void remove_consumed_memory(MonotonicEpoch consumedGci);
  void remove_consumed_epoch_data(MonotonicEpoch consumedGci);

  /* Remove all resources related to specified epoch
   * after it has been completely consumed.
   */
  void remove_consumed(MonotonicEpoch consumedGci);

  // Count the buffered epochs (in event queue and completed list).
  Uint32 count_buffered_epochs() const;

  /* Consume and discard all completed events.
   * Memory related to discarded events are released.
   */
  void consume_all();

  // Check if event data belongs to an exceptional epoch, such as,
  // an inconsistent, out-of-memory or empty epoch.
  bool is_exceptional_epoch(EventBufData *data);

  // Consume current EventData and dequeue next for consumption
  EventBufDataHead *nextEventData();

  // Dequeue event data from event queue and give it for consumption.
  NdbEventOperation *nextEvent2();
  bool isConsistent(Uint64 &gci);
  bool isConsistentGCI(Uint64 gci);

  NdbEventOperationImpl *getEpochEventOperations(
      Uint32 *iter, Uint32 &event_types, Uint32 &cumulative_any_value,
      Uint32 &filtered_any_value) const;
  void deleteUsedEventOperations(MonotonicEpoch last_consumed_gci);

  EventBufDataHead *move_data();

  // routines to copy/merge events
  EventBufData *alloc_data();
  EventBufDataHead *alloc_data_main();
  int alloc_mem(EventBufData *data, const LinearSectionPtr ptr[3]);
  int copy_data(const SubTableData *const sdata, Uint32 len,
                const LinearSectionPtr ptr[3], EventBufData *data);
  int merge_data(const SubTableData *const sdata, Uint32 len,
                 const LinearSectionPtr ptr[3], EventBufData *data);
  int get_main_data(Gci_container *bucket, EventBufData_hash::Pos &hpos,
                    EventBufData *blob_data);
  void add_blob_data(EventBufDataHead *main_data, EventBufData *blob_data);

  void *alloc(Uint32 sz);
  Uint64 get_free_data_sz() const;
  Uint64 get_used_data_sz() const;

  // Must report status if buffer manager state is changed
  void reportStatus(ReportReason reason = NO_REPORT);

  // Get event buffer memory usage statistics
  void get_event_buffer_memory_usage(Ndb::EventBufferMemoryUsage &usage);

  // Global Mutex used for some things
  static NdbMutex *p_add_drop_mutex;

#ifdef VM_TRACE
  const char *m_latest_command;
  Uint64 m_flush_gci;
#endif

  Ndb *m_ndb;

  // Gci are monotonic increasing while the cluster is not restarted.
  // A restart will start a new generation of epochs which also inc:
  Uint32 m_epoch_generation;

  // "latest gci" variables updated in receiver thread
  Uint64 m_latestGCI;            // latest GCI completed in order
  Uint64 m_latest_complete_GCI;  // latest complete GCI (in case of outof order)
  Uint64 m_highest_sub_gcp_complete_GCI;  // highest gci seen in api
  // "latest gci" variables updated in user thread
  MonotonicEpoch m_latest_poll_GCI;  // latest gci handed over to user thread
  Uint64 m_latest_consumed_epoch;    // latest epoch consumed by user thread

  /**
   * m_buffered_epochs = #completed epochs - #completely consumed epochs.
   * Updated in receiver thread when an epoch completes.
   * User thread updates it when an epoch is completely consumed.
   * Owned by receiver thread and user thread update needs mutex.
   */
  Uint32 m_buffered_epochs;

  bool m_failure_detected;  // marker that event operations have failure events

  bool m_startup_hack;
  bool m_prevent_nodegroup_change;

  NdbMutex *m_mutex;

  // receive thread
  EpochDataList m_complete_data;

  // user thread
  EpochDataList m_event_queue;
  const EventBufData *m_current_data;

  Uint64 m_total_alloc;  // total allocated memory

  // ceiling for total allocated memory, 0 means unlimited
  Uint64 m_max_alloc;

  // Crash when OS memory allocation for event buffer fails
  void crashMemAllocError(const char *error_text);

  EventBufferManager m_event_buffer_manager;  // managing buffer memory usage

  unsigned get_eventbuffer_free_percent();
  void set_eventbuffer_free_percent(unsigned free);

  // thresholds to report status
  unsigned m_free_thresh, m_min_free_thresh, m_max_free_thresh;
  unsigned m_gci_slip_thresh;
  NDB_TICKS m_last_log_time;  // Limit frequency of event buffer status reports

  // Allow update error from const methods
  mutable NdbError m_error;

 private:
  void insert_event(NdbEventOperationImpl *impl, SubTableData &data,
                    const LinearSectionPtr *ptr, Uint32 &oid_ref);

  EventMemoryBlock *expand_memory_blocks();
  void complete_memory_block(MonotonicEpoch highest_epoch);

  /*
    List of Memory blocks in use in increasing 'epoch-expiry' order.
    Thus, allocation is always from 'tail' and we release
    expired blocks from 'head.
  */
  EventMemoryBlock *m_mem_block_head;
  EventMemoryBlock *m_mem_block_tail;

  /*
    List of free memory blocks available for recycle and its size
    (Included in ::get_free_data_sz())
  */
  EventMemoryBlock *m_mem_block_free;
  Uint64 m_mem_block_free_sz;  // Total size of above

  bool m_queue_empty_epoch;

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

  inline Gci_container *find_bucket(Uint64 gci) {
    Uint32 pos = (Uint32)(gci & ACTIVE_GCI_MASK);
    Gci_container *bucket = ((Gci_container *)(m_active_gci.getBase())) + pos;
    if (likely(gci == bucket->m_gci)) return bucket;

    return find_bucket_chained(gci);
  }

#ifdef VM_TRACE
  void verify_known_gci(bool allowempty);
#endif
  Gci_container *find_bucket_chained(Uint64 gci);
  void complete_bucket(Gci_container *);
  bool find_max_known_gci(Uint64 *res) const;
  void resize_known_gci();

  Bitmask<(unsigned int)_NDB_NODE_BITMASK_SIZE> m_alive_node_bit_mask;
  Uint16 m_sub_data_streams[MAX_SUB_DATA_STREAMS];

  void handle_change_nodegroup(const SubGcpCompleteRep *);

  Uint16 find_sub_data_stream_number(Uint16 sub_data_stream);
  void crash_on_invalid_SUB_GCP_COMPLETE_REP(const Gci_container *bucket,
                                             const SubGcpCompleteRep *const rep,
                                             Uint32 replen, Uint32 remcnt,
                                             Uint32 repcnt) const;

 public:
  // Create an epoch with only a exceptional event and an empty gci_op list.
  EpochData *create_empty_exceptional_epoch(Uint64 gci, Uint32 type);

  void set_total_buckets(Uint32);
};

inline NdbEventOperationImpl *NdbEventBuffer::getEventOperationImpl(
    NdbEventOperation *tOp) {
  return &tOp->m_impl;
}

inline void NdbEventOperationImpl::receive_data(NdbRecAttr *r,
                                                const Uint32 *data, Uint32 sz) {
  r->receive_data(data, sz);
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

inline bool EventBufferManager::isCompletelyBuffering() {
  if (m_event_buffer_manager_state == EBM_COMPLETELY_BUFFERING) {
    assert(m_pre_gap_epoch == 0 && m_begin_gap_epoch == 0 &&
           m_end_gap_epoch == 0);
    return true;
  }
  return false;
}

inline bool EventBufferManager::isPartiallyDiscarding() {
  if (m_event_buffer_manager_state == EBM_PARTIALLY_DISCARDING) {
    assert(m_pre_gap_epoch > 0 && m_begin_gap_epoch == 0 &&
           m_end_gap_epoch == 0);
    return true;
  }
  return false;
}

inline bool EventBufferManager::isCompletelyDiscarding() {
  if (m_event_buffer_manager_state == EBM_COMPLETELY_DISCARDING) {
    assert(m_pre_gap_epoch > 0 && m_begin_gap_epoch > 0 &&
           m_end_gap_epoch == 0);
    return true;
  }
  return false;
}

inline bool EventBufferManager::isPartiallyBuffering() {
  if (m_event_buffer_manager_state == EBM_PARTIALLY_BUFFERING) {
    assert(m_pre_gap_epoch > 0 && m_begin_gap_epoch > 0 && m_end_gap_epoch > 0);
    return true;
  }
  return false;
}

inline bool EventBufferManager::isInDiscardingState() {
  return (m_event_buffer_manager_state != EBM_COMPLETELY_BUFFERING);
}

#endif
