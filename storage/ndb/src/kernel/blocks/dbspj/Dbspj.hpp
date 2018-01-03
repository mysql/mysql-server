/*
   Copyright (c) 2012, 2017, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef DBSPJ_H
#define DBSPJ_H

#include <SimulatedBlock.hpp>
#include <signaldata/LqhKey.hpp>
#include <signaldata/ScanFrag.hpp>
#include <AttributeHeader.hpp>
#include <IntrusiveList.hpp>
#include <ArenaPool.hpp>
#include <DataBuffer.hpp>
#include <Bitmask.hpp>
#include <signaldata/DbspjErr.hpp>
#include "../dbtup/tuppage.hpp"

#define JAM_FILE_ID 481


class SectionReader;
struct QueryNode;
struct QueryNodeParameters;

//#define SPJ_TRACE_TIME

class Dbspj: public SimulatedBlock {
public:
  Dbspj(Block_context& ctx, Uint32 instanceNumber = 0);
  virtual ~Dbspj();

  struct Request;
  struct TreeNode;
private:
  BLOCK_DEFINES(Dbspj);

  /**
   * Signals from DICT
   */
  void execTC_SCHVERREQ(Signal* signal);
  void execTAB_COMMITREQ(Signal* signal);
  void execPREP_DROP_TAB_REQ(Signal* signal);
  void execDROP_TAB_REQ(Signal* signal);
  void execALTER_TAB_REQ(Signal* signal);

  /**
   * Signals from TC
   */
  void execLQHKEYREQ(Signal* signal);
  void execSCAN_FRAGREQ(Signal* signal);
  void execSCAN_NEXTREQ(Signal* signal);

  void execDIH_SCAN_TAB_REF(Signal*);
  void execDIH_SCAN_TAB_CONF(Signal*);

  void execSIGNAL_DROPPED_REP(Signal*);

  /**
   * Signals from LQH
   */
  void execLQHKEYREF(Signal* signal);
  void execLQHKEYCONF(Signal* signal);
  void execSCAN_FRAGREF(Signal* signal);
  void execSCAN_FRAGCONF(Signal* signal);
  void execSCAN_HBREP(Signal* signal);
  void execTRANSID_AI(Signal*signal);

  /**
   * General signals
   */
  void execDUMP_STATE_ORD(Signal* signal){}
  void execREAD_NODESCONF(Signal*);
  void execREAD_CONFIG_REQ(Signal* signal);
  void execSTTOR(Signal* signal);
  void execDBINFO_SCANREQ(Signal* signal);
  void execCONTINUEB(Signal*);
  void execNODE_FAILREP(Signal*);
  void execINCL_NODEREQ(Signal*);
  void execAPI_FAILREQ(Signal*);

  void sendSTTORRY(Signal* signal);

protected:
  //virtual bool getParam(const char* name, Uint32* count);

public:
  struct ScanFragHandle;
  typedef DataBuffer<14, LocalArenaPool<DataBufferSegment<14> > > Correlation_list;
  typedef LocalDataBuffer<14, LocalArenaPool<DataBufferSegment<14> > > Local_correlation_list;
  typedef DataBuffer<14, LocalArenaPool<DataBufferSegment<14> > > Dependency_map;
  typedef LocalDataBuffer<14, LocalArenaPool<DataBufferSegment<14> > > Local_dependency_map;
  typedef DataBuffer<14, LocalArenaPool<DataBufferSegment<14> > > PatternStore;
  typedef LocalDataBuffer<14, LocalArenaPool<DataBufferSegment<14> > > Local_pattern_store;
  typedef Bitmask<(NDB_SPJ_MAX_TREE_NODES+31)/32> TreeNodeBitMask;

  /* *********** TABLE RECORD ********************************************* */

  /********************************************************/
  /* THIS RECORD CONTAINS THE CURRENT SCHEMA VERSION OF   */
  /* ALL TABLES IN THE SYSTEM.                            */
  /********************************************************/
  struct TableRecord {
    TableRecord() 
    : m_currentSchemaVersion(0), m_flags(0)
    {};

    TableRecord(Uint32 schemaVersion)
    : m_currentSchemaVersion(schemaVersion), m_flags(TR_PREPARED)
    {};

    Uint32 m_currentSchemaVersion;
    Uint16 m_flags;

    enum {
      TR_ENABLED      = 1 << 0,
      TR_DROPPING     = 1 << 1,
      TR_PREPARED     = 1 << 2
      ,TR_READ_BACKUP = (1 << 5)
      ,TR_FULLY_REPLICATED = (1 << 6)
    };
    Uint8 get_enabled()     const { return (m_flags & TR_ENABLED)      != 0; }
    Uint8 get_dropping()    const { return (m_flags & TR_DROPPING)     != 0; }
    Uint8 get_prepared()    const { return (m_flags & TR_PREPARED)     != 0; }
    void set_enabled(Uint8 f)     { f ? m_flags |= (Uint16)TR_ENABLED      : m_flags &= ~(Uint16)TR_ENABLED; }
    void set_dropping(Uint8 f)    { f ? m_flags |= (Uint16)TR_DROPPING     : m_flags &= ~(Uint16)TR_DROPPING; }
    void set_prepared(Uint8 f)    { f ? m_flags |= (Uint16)TR_PREPARED : m_flags &= ~(Uint16)TR_PREPARED; }

    Uint32 checkTableError(Uint32 schemaVersion) const;
  };
  typedef Ptr<TableRecord> TableRecordPtr;

  enum Buffer_type {
    BUFFER_VOID  = 0,
    BUFFER_STACK = 1,
    BUFFER_VAR   = 2
  };

  struct RowRef
  {
    Uint32 m_page_id;
    Uint16 m_page_pos;
    union
    {
      Uint16 unused;
      enum Buffer_type m_alloc_type:16;
    };

    void copyto_link(Uint32 * dst) const {
      dst[0] = m_page_id; dst[1] = m_page_pos;
    }
    void assign_from_link(const Uint32 * src) {
      m_page_id = src[0];
      m_page_pos = src[1];
    }

    void copyto_map(Uint16 * dst) const {
      dst[0] = Uint16(m_page_id);
      dst[1] = Uint16(m_page_id >> 16);
      dst[2] = m_page_pos;
    }

    void assign_from_map(const Uint16 * src) {
      m_page_id = src[0];
      m_page_id += Uint32(src[1]) << 16;
      m_page_pos = src[2];
    }

    static bool map_is_null(const Uint16 * src) {
      return src[2] == 0xFFFF;
    }

    void setNull() { m_page_id = RNIL;}
    bool isNull() const { return m_page_id == RNIL;}
  };

  static const RowRef NullRowRef;

  /**
   * This struct represent a row being passed to a child
   */
  struct RowPtr
  {
    Uint32 m_type;
    Uint32 m_src_node_ptrI;
    Uint32 m_src_correlation;

    struct Header
    {
      Uint32 m_len;
      Uint32 m_offset[1];
    };

    struct Section
    {
      const Header * m_header;
      SegmentedSectionPtrPOD m_dataPtr;
    };

    struct Linear
    {
      RowRef m_row_ref;
      const Header * m_header;
      const Uint32 * m_data;
    };
    union
    {
      struct Section m_section;
      struct Linear m_linear;
    } m_row_data;

    enum RowType
    {
      RT_SECTION = 1,
      RT_LINEAR = 2,
      RT_END = 0
    };
  };

  struct RowBuffer;  // forward decl.

  /**
   * Define overlayed 'base class' for SLFifoRowList and RowMap.
   * As we want these to be POD struct, we does not use 
   * inheritance, but have to take care that first part
   * of these struct are correctly overlayed.
   */
  struct RowCollectionBase
  {
    RowBuffer* m_rowBuffer;
  };

  struct SLFifoRowList //: public RowCollectionBase
  {
    /**
     * BEWARE: Overlayed 'struct RowCollectionBase'
     */
    RowBuffer* m_rowBuffer;

    /**
     * Data used for a single linked list of rows
     */
    Uint32 m_first_row_page_id;
    Uint32 m_last_row_page_id;
    Uint16 m_first_row_page_pos;
    Uint16 m_last_row_page_pos;

    void construct(RowBuffer& rowBuffer) {
      m_rowBuffer = &rowBuffer;
      init();
    }
    void init() { m_first_row_page_id = RNIL;}
    bool isNull() const { return m_first_row_page_id == RNIL; }
  };

  struct RowMap //: public RowCollectionBase
  {
    /**
     * BEWARE: Overlayed 'struct RowCollectionBase'
     */
    RowBuffer* m_rowBuffer;

    /**
     * Data used for a map with rows (key is correlation id)
     *   currently a single array is used to store row references
     *   (size == batch size)
     */
    RowRef m_map_ref;
    Uint16 m_size;                // size of array
    Uint16 m_elements;            // #elements in array

    void construct(RowBuffer& rowBuffer,
                   Uint32 capacity)
    {
      m_rowBuffer = &rowBuffer;
      m_size = capacity;
      init();
    }
    void init() {
      m_map_ref.setNull();
      m_elements = 0;
    }

    bool isNull() const { return m_map_ref.isNull(); }

    void assign (RowRef ref) {
      m_map_ref = ref;
    }

    void copyto(RowRef& ref) const {
      ref = m_map_ref;
    }

    /**
     * functions for manipulating *content* of map
     */
    void clear(Uint32 * ptr)  {
      memset(ptr, 0xFF, MAP_SIZE_PER_REF_16 * m_size * sizeof(Uint16));
    }
    void store(Uint32 * _ptr, Uint32 pos, RowRef ref) {
      Uint16 * ptr = (Uint16*)_ptr;
      ptr += MAP_SIZE_PER_REF_16 * pos;
      ref.copyto_map(ptr);
      m_elements++;
    }
    static void load(const Uint32 * _ptr, Uint32 pos, RowRef & ref) {
      const Uint16 * ptr = (const Uint16*)_ptr;
      ptr += MAP_SIZE_PER_REF_16 * pos;
      ref.assign_from_map(ptr);
    }
    static bool isNull(const Uint32 * _ptr, Uint32 pos) {
      const Uint16 * ptr = (const Uint16*)_ptr;
      ptr += MAP_SIZE_PER_REF_16 * pos;
      return RowRef::map_is_null(ptr);
    }

    STATIC_CONST( MAP_SIZE_PER_REF_16 = 3 );
  };

  /**
   * Define overlayed 'base class' for SLFifoRowListIterator
   * and RowMapIterator.
   * As we want these to be POD struct, we does not use 
   * inheritance, but have to take care that first part
   * of these struct are correctly overlayed.
   */
  struct RowIteratorBase
  {
    RowRef m_ref;
    Uint32 * m_row_ptr;

    bool isNull() const { return m_ref.isNull(); }
    void setNull() { m_ref.setNull(); }
  };

  struct SLFifoRowListIterator //: public RowIteratorBase
  {
    /**
     * BEWARE: Overlayed 'struct RowIteratorBase'
     */
    RowRef m_ref;
    Uint32 * m_row_ptr;

    bool isNull() const { return m_ref.isNull(); }
    void setNull() { m_ref.setNull(); }
    // END: RowIteratorBase
  };

  struct RowMapIterator //: public RowIteratorBase
  {
    /**
     * BEWARE: Overlayed 'struct RowIteratorBase'
     */
    RowRef m_ref;
    Uint32 * m_row_ptr;

    bool isNull() const { return m_ref.isNull(); }
    void setNull() { m_ref.setNull(); }
    // END: RowIteratorBase

    Uint32 * m_map_ptr;
    Uint16 m_size;
    Uint16 m_element_no;
  };

  /**
   * Abstraction of SLFifoRowList & RowMap
   */
  struct RowCollection
  {
    enum collection_type
    {
      COLLECTION_VOID,
      COLLECTION_MAP,
      COLLECTION_LIST
    };
    union
    {
      RowCollectionBase m_base;  // Common part for map & list
      SLFifoRowList m_list;
      RowMap m_map;
    };

    RowCollection() : m_type(COLLECTION_VOID) {}

    void construct(collection_type type,
                   RowBuffer& rowBuffer,
                   Uint32 capacity)
    {
      m_type = type;
      if (m_type == COLLECTION_MAP)
        m_map.construct(rowBuffer,capacity);
      else if (m_type == COLLECTION_LIST)
        m_list.construct(rowBuffer);
    }

    void init() {
      if (m_type == COLLECTION_MAP)
        m_map.init();
      else if (m_type == COLLECTION_LIST)
        m_list.init();
    }

    Uint32 rowOffset() const {
      return (m_type == COLLECTION_MAP) ? 0 : 2;
    }

    collection_type m_type;
  };

  struct RowIterator
  {
    union
    {
      RowIteratorBase m_base;  // Common part for map & list
      SLFifoRowListIterator m_list;
      RowMapIterator m_map;
    };
    RowCollection::collection_type m_type;

    RowIterator() { init(); }
    void init() { m_base.setNull(); }
    bool isNull() const { return m_base.isNull(); }
  };
 

  /**
   * A struct used when building an TreeNode
   */
  struct Build_context
  {
    Uint32 m_cnt;
    Uint32 m_scanPrio;
    Uint32 m_savepointId;
    Uint32 m_batch_size_rows;
    Uint32 m_resultRef;  // API
    Uint32 m_resultData; // API
    Uint32 m_senderRef;  // TC (used for routing)
    Uint32 m_scan_cnt;
    Signal* m_start_signal; // Argument to first node in tree

    TreeNodeBitMask m_scans; // TreeNodes doing scans

    // Used for resolving dependencies
    Ptr<TreeNode> m_node_list[NDB_SPJ_MAX_TREE_NODES];
  };

  struct RowPage
  {
    /**
     * NOTE: This contains various padding to be binary aligned with Tup_page
     *       (for storing into DLFifoList<RowPage>
     */
    RowPage() {}
    struct File_formats::Page_header m_page_header;
    Uint32 unused0;
    Uint32 unused1;
    Uint32 nextList;
    Uint32 prevList;
    Uint32 m_data[GLOBAL_PAGE_SIZE_WORDS - 7];
    STATIC_CONST( SIZE = GLOBAL_PAGE_SIZE_WORDS - 7 );
  };
  typedef ArrayPool<RowPage> RowPage_pool;
  typedef SLList<RowPage_pool> RowPage_list;
  typedef LocalSLList<RowPage_pool> Local_RowPage_list;
  typedef DLFifoList<RowPage_pool> RowPage_fifo;
  typedef LocalDLFifoList<RowPage_pool> Local_RowPage_fifo;

  typedef Tup_varsize_page Var_page;

  struct RowBuffer
  {
    enum Buffer_type m_type;

    RowBuffer() : m_type(BUFFER_VOID) {}
    RowPage_fifo::Head m_page_list;

    void init(enum Buffer_type type)
    {
      new (&m_page_list) RowPage_fifo::Head();
      m_type = type;
      reset();
    }
    void reset()
    {
      if (m_type == BUFFER_STACK)
        m_stack.m_pos = 0xFFFF;
      else if (m_type == BUFFER_VAR)
        m_var.m_free = 0;
    }

    struct Stack
    {
      Uint32 m_pos; // position on head-page
    };

    struct Var
    {
      Uint32 m_free; // Free on last page in list
    };

    union {
      struct Stack m_stack;
      struct Var m_var;
    };
  };

  /**
   * A struct for building DA-part
   *   that is shared between QN_LookupNode & QN_ScanFragNode
   */
  struct DABuffer
  {
    const Uint32 * ptr;
    const Uint32 * end;
  };

  /**
   * A struct with "virtual" functions for different operations
   */
  struct OpInfo
  {
    /**
     * This function create a operation suitable
     *   for execution
     */
    Uint32 (Dbspj::*m_build)(Build_context&ctx, Ptr<Request>,
                             const QueryNode*, const QueryNodeParameters*);

    /**
     * This function is called after build, but before start
     *   it's allowed to block (i.e send signals)
     *   and should if so increase request::m_outstanding
     */
    void (Dbspj::*m_prepare)(Signal*, Ptr<Request>, Ptr<TreeNode>);

    /**
     * This function is used for starting a request
     */
    void (Dbspj::*m_start)(Signal*, Ptr<Request>, Ptr<TreeNode>);

    /**
     * This function is called when a waited for signal arrives.
     * Return 'true' if this completes the wait for this treeNode
     */
    bool (Dbspj::*m_countSignal)(const Signal*,Ptr<Request>,Ptr<TreeNode>);

    /**
     * This function is used when getting a LQHKEYREF
     */
    void (Dbspj::*m_execLQHKEYREF)(Signal*, Ptr<Request>, Ptr<TreeNode>);

    /**
     * This function is used when getting a LQHKEYCONF
     */
    void (Dbspj::*m_execLQHKEYCONF)(Signal*, Ptr<Request>, Ptr<TreeNode>);

    /**
     * This function is used when getting a SCAN_FRAGREF
     */
    void (Dbspj::*m_execSCAN_FRAGREF)(Signal*, Ptr<Request>, Ptr<TreeNode>, Ptr<ScanFragHandle>);

    /**
     * This function is used when getting a SCAN_FRAGCONF
     */
    void (Dbspj::*m_execSCAN_FRAGCONF)(Signal*, Ptr<Request>, Ptr<TreeNode>, Ptr<ScanFragHandle>);

    /**
     * This function is called on the *child* by the *parent* when passing rows
     */
    void (Dbspj::*m_parent_row)(Signal*,Ptr<Request>,Ptr<TreeNode>,
                                const RowPtr&);

    /**
     * This function is called on the *child* by the *parent* when *parent*
     *   has completed a batch
     */
    void (Dbspj::*m_parent_batch_complete)(Signal*,Ptr<Request>,Ptr<TreeNode>);

    /**
     * This function is called on the *child* by the *parent* when this
     *   child should prepare to resend results related to parents current batch
     */
    void (Dbspj::*m_parent_batch_repeat)(Signal*,Ptr<Request>,Ptr<TreeNode>);

    /**
     * This function is called on the *child* by the *parent* when
     *   child should release buffers related to parents current batch
     */
    void (Dbspj::*m_parent_batch_cleanup)(Ptr<Request>,Ptr<TreeNode>);

    /**
     * This function is called when getting a SCAN_NEXTREQ
     */
    void (Dbspj::*m_execSCAN_NEXTREQ)(Signal*, Ptr<Request>,Ptr<TreeNode>);

    /**
     * This function is called when all nodes in tree are finished
     *   it's allowed to "block" (by increaseing requestPtr.p->m_outstanding)
     */
    void (Dbspj::*m_complete)(Signal*, Ptr<Request>,Ptr<TreeNode>);

    /**
     * This function is called when a tree is aborted
     *   it's allowed to "block" (by increaseing requestPtr.p->m_outstanding)
     */
    void (Dbspj::*m_abort)(Signal*, Ptr<Request>, Ptr<TreeNode>);

    /**
     * This function is called on node-failure
     */
    Uint32 (Dbspj::*m_execNODE_FAILREP)(Signal*, Ptr<Request>, Ptr<TreeNode>,
                                        NdbNodeBitmask);
    /**
     * This function is called when request/node(s) is/are removed
     *  should only do local cleanup(s)
     */
    void (Dbspj::*m_cleanup)(Ptr<Request>, Ptr<TreeNode>);

    /**
     * This function is called to check the node validity within
     * the request during debug execution
     */
    bool (Dbspj::*m_checkNode)(const Ptr<Request>, const Ptr<TreeNode>);

    /**
     * This function is called to dump request info to the node
     * log for debugging purposes.  It should be used for
     * treenode type-specific data, the generic treenode info
     * is handled by dumpNodeCommon()
     */
    void (Dbspj::*m_dumpNode)(const Ptr<Request>, const Ptr<TreeNode>);
  };  //struct OpInfo

  struct LookupData
  {
    Uint32 m_api_resultRef;
    Uint32 m_api_resultData;
    /**
     * This is the number of outstanding messages. When this number is zero
     * and m_parent_batch_complete is true, we know that we have received
     * all rows for this operation in this batch.
     */
    Uint32 m_outstanding;
    Uint32 m_lqhKeyReq[LqhKeyReq::FixedSignalLength + 4];
  };

  struct ScanFragHandle
  {
    enum SFH_State
    {
      SFH_NOT_STARTED  = 0,
      SFH_SCANNING     = 1, // in LQH
      SFH_WAIT_NEXTREQ = 2,
      SFH_COMPLETE     = 3,
      SFH_WAIT_CLOSE   = 4
    };

    void init(Uint32 fid, bool readBackup)
    {
      m_ref = 0;
      m_fragId = fid;
      m_state = SFH_NOT_STARTED;
      m_rangePtrI = RNIL;
      m_readBackup = readBackup;
      reset_ranges();
    }

    Uint32 m_magic;
    Uint32 m_treeNodePtrI;
    Uint16 m_fragId;
    Uint8 m_state;
    Uint8 m_readBackup;
    Uint32 m_ref;

    void reset_ranges() {
      // m_rangePtrI is explicitly managed...in code
      m_range_builder.m_range_cnt = m_range_builder.m_range_size = 0;
    }
    struct RangeBuilder
    {
      Uint32 m_range_size;
      Uint16 m_range_cnt; // too set bounds info correctly
    } m_range_builder;
    Uint32 m_rangePtrI;
    union {
      Uint32 nextList;
      Uint32 nextPool;
    };
  };

  typedef RecordPool<ArenaPool<ScanFragHandle> > ScanFragHandle_pool;
  typedef SLFifoList<ScanFragHandle_pool> ScanFragHandle_list;
  typedef LocalSLFifoList<ScanFragHandle_pool> Local_ScanFragHandle_list;

  /**
   * This class computes mean and standard deviation incrementally for a series
   * of samples.
   */
  class IncrementalStatistics
  {
  public:
    /**
     * We cannot have a (non-trivial) constructor, since this class is used in
     * unions.
     */
    void init()
    {
      m_mean = m_sumSquare = 0.0;
      m_noOfSamples = 0;
    }

    // Add another sample.
    void sample(double observation);

    double getMean() const {
      return m_mean;
    }

    double getStdDev() const { 
      return m_noOfSamples < 2 ? 0.0 : sqrt(m_sumSquare/(m_noOfSamples - 1));
    }

    bool isValid() const {
      return (m_noOfSamples > 0);
    }

  private:
    // Mean of all samples
    double m_mean;
    //Sum of square of differences from the current mean.
    double m_sumSquare;
    Uint32 m_noOfSamples;
  }; // IncrementalStatistics

  struct ScanFragData
  {
    Uint16 m_frags_complete;
    Uint16 m_frags_outstanding;
    /**
     * The number of fragment for which we have not yet sent SCAN_FRAGREQ but
     * will eventually do so.
     */
    Uint16 m_frags_not_started;
    Uint32 m_rows_received;  // #execTRANSID_AI
    Uint32 m_rows_expecting; // Sum(ScanFragConf)
    Uint32 m_batch_chunks;   // #SCAN_FRAGREQ + #SCAN_NEXTREQ to retrieve batch
    Uint32 m_scanCookie;
    Uint32 m_fragCount;
    // The number of fragments that we scan in parallel.
    Uint32 m_parallelism;
    // True if we are still receiving the first batch for this operation.
    bool   m_firstBatch;
    /**
     * Mean and standard deviation for the optimal parallelism for earlier
     * executions of this operation.
     */
    IncrementalStatistics m_parallelismStat;
    // Total number of rows for the current execution of this operation.
    Uint32 m_totalRows;
    // Total number of bytes for the current execution of this operation.
    Uint32 m_totalBytes;

    ScanFragHandle_list::HeadPOD m_fragments; // ScanFrag states
    union
    {
      PatternStore::HeadPOD m_prunePattern;
      Uint32 m_constPrunePtrI;
    };
    /**
     * Max number of rows seen in a batch. Used for calculating the number of
     * rows per fragment in the next next batch when using adaptive batch size.
     */
    Uint32 m_largestBatchRows;
    /**
     * Max number of bytes seen in a batch. Used for calculating the number of
     * rows per fragment in the next next batch when using adaptive batch size.
     */
    Uint32 m_largestBatchBytes;
    Uint32 m_scanFragReq[ScanFragReq::SignalLength + 2];
  };

  struct DeferredParentOps
  {
    /**
     * m_correlations contains a list of Correlation Values (Uint32)
     * which identifies parent rows which has been deferred. 
     * m_pos are index into this array, identifying the next parent row
     * for which to resume operation.
     */
    Correlation_list::Head m_correlations;
    Uint16 m_pos; // Next row operation to resume 

    DeferredParentOps() : m_correlations(), m_pos(0) {}

    void init()  {
      m_correlations.init();
      m_pos = 0;
    }
    bool isEmpty() const {
      return (m_pos == m_correlations.getSize());
    }
  };

  struct TreeNode_cursor_ptr
  {
    Uint32 nextList;
  };

  /**
   * A node in a Query
   *   (This is an instantiated version of QueryNode in
   *    include/kernel/signal/QueryTree.hpp)
   */
  struct TreeNode : TreeNode_cursor_ptr
  {
    STATIC_CONST ( MAGIC = ~RT_SPJ_TREENODE );

    TreeNode()
    : m_magic(MAGIC), m_state(TN_END),
      m_parentPtrI(RNIL), m_requestPtrI(RNIL),
      m_ancestors(),
      m_resumeEvents(0), m_resumePtrI(RNIL)
    {
    }

    TreeNode(Uint32 request)
    : m_magic(MAGIC),
      m_info(0), m_bits(T_LEAF), m_state(TN_BUILDING),
      m_parentPtrI(RNIL), m_requestPtrI(request),
      m_ancestors(),
      m_resumeEvents(0), m_resumePtrI(RNIL),
      nextList(RNIL), prevList(RNIL)
    {
//    m_send.m_ref = 0;
      m_send.m_correlation = 0;
      m_send.m_keyInfoPtrI = RNIL;
      m_send.m_attrInfoPtrI = RNIL;
    }

    const Uint32 m_magic;
    const struct OpInfo* m_info;

    enum TreeNodeState
    {
      /**
       * Initial
       */
      TN_BUILDING = 1,

      /**
       * Tree node is preparing
       */
      TN_PREPARING = 2,

      /**
       * Tree node is build and prepared, but not active
       */
      TN_INACTIVE = 3,

      /**
       * Tree node is active (i.e has outstanding request(s))
       */
      TN_ACTIVE = 4,

      /**
       * Tree node is "finishing" (after TN_INACTIVE)
       */
      TN_COMPLETING = 5,

      /**
       * end-marker, not a valid state
       */
      TN_END = 0
    };

    enum TreeNodeBits
    {
      T_ATTR_INTERPRETED = 0x1,

      /**
       * Will node be executed only once (::parent_row())
       *   implies key/attr-info will be disowned (by send-signal)
       */
      T_ONE_SHOT = 0x2,

      /**
       * Is keyinfo "constructed"
       *   (implies key info will be disowned (by send-signal)
       */
      T_KEYINFO_CONSTRUCTED = 0x4,

      /**
       * Is attrinfo "constructed"
       *   (implies attr info will be disowned (by send-signal)
       */
      T_ATTRINFO_CONSTRUCTED = 0x8,

      /**
       * Is this node a leaf-node
       */
      T_LEAF = 0x10,

      /**
       * Does this node have a user projection. (The index access part of
       * an index lookup operation has no user projection, since only the
       * base table tuple is sent to the API.)
       */
      T_USER_PROJECTION = 0x20,

      /**
       * Is this a unique index lookup (on index table)
       *   (implies some extra error handling code)
       */
      T_UNIQUE_INDEX_LOOKUP = 0x40,

      /*
       * Should this node buffers its rows
       */
      T_ROW_BUFFER = 0x80,

      /**
       * Should rows have dictionary (i.e random access capability)
       *  This is typically used when having nodes depending on multiple parents
       *  so that when row gets availble from "last" parent, a key can be
       *  constructed using correlation value from parents
       */
      T_ROW_BUFFER_MAP = 0x100,

      /**
       * Does any child need to know when all its ancestors are complete
       */
      T_REPORT_BATCH_COMPLETE  = 0x200,

      /**
       * Do *I need* to know when all ancestors has completed this batch
       */
      T_NEED_REPORT_BATCH_COMPLETED = 0x400,

      /**
       * Constant prune pattern
       */
      T_CONST_PRUNE = 0x800,

      /**
       * Prune pattern
       */
      T_PRUNE_PATTERN = 0x1000,

      /**
       * Should fragment scan be parallel
       */
      T_SCAN_PARALLEL = 0x2000,

      /**
       * Possible requesting resultset for this fragment scan to be repeated
       */
      T_SCAN_REPEATABLE = 0x4000,

      /**
       * Exec of a previous REQ must complete before we can proceed.
       * A ResumeEvent will later resume exec. of this operation
       */
      T_EXEC_SEQUENTIAL = 0x8000,

      /**
       * Does this node need the m_prepare() method to be called.
       *  (Also implies RT_NEED_PREPARE is set)
       */
      T_NEED_PREPARE = 0x10000,

      /**
       * Does this node need the m_complete() method to be called.
       *  (Also implies RT_NEED_COMPLETE is set)
       *
       */
      T_NEED_COMPLETE = 0x20000,

      // End marker...
      T_END = 0
    };

    /**
     * Describe whether a LQHKEY-REF and/or CONF whould trigger a 
     * exec resume of another TreeNode having T_EXEC_SEQUENTIAL.
     * (Used as a bitmask)
     */
    enum TreeNodeResumeEvents
    {
      TN_RESUME_REF   = 0x01,
      TN_RESUME_CONF  = 0x02
    };

    bool isLeaf() const { return (m_bits & T_LEAF) != 0;}

    // table or index this TreeNode operates on, and its schemaVersion
    Uint32 m_tableOrIndexId;
    Uint32 m_schemaVersion;

    // TableId if 'm_tableOrIndexId' is an index, else equal 
    Uint32 m_primaryTableId; 

    Uint32 m_bits;
    Uint32 m_state;
    Uint32 m_node_no;
    Uint32 m_batch_size;
    Uint32 m_parentPtrI;
    const Uint32 m_requestPtrI;
    TreeNodeBitMask m_ancestors;
    Dependency_map::Head m_dependent_nodes;
    PatternStore::Head m_keyPattern;
    PatternStore::Head m_attrParamPattern;

    /**
     * Rows buffered by this node
     */
    RowCollection m_rows;

    /**
     * T_EXEC_SEQUENTIAL cause execution of child operations to
     * be deferred.  These operations are queued in the 'struct DeferredParentOps'
     * Currently only Lookup operation might be deferred.
     * Could later be extended to also cover index scans.
     */
    DeferredParentOps m_deferred;

    /**
     * Set of TreeNodeResumeEvents, possibly or'ed.
     * Specify whether a REF or CONF will cause a resume
     * of the TreeNode referred by 'm_resumePtrI'.
     */
    Uint32 m_resumeEvents;
    Uint32 m_resumePtrI;

    union
    {
      LookupData m_lookup_data;
      ScanFragData m_scanFrag_data;
    };

    struct {
      Uint32 m_ref;              // dst for signal
      /** Each tuple has a 16-bit id that is unique within that operation,
       * batch and SPJ block instance. The upper half word of m_correlation
       * is the id of the parent tuple, and the lower half word is the
       * id of the current tuple.*/
      Uint32 m_correlation;
      Uint32 m_keyInfoPtrI;      // keyInfoSection
      Uint32 m_attrInfoPtrI;     // attrInfoSection
    } m_send;

    union {
      Uint32 nextList;
      Uint32 nextPool;
    };
    Uint32 prevList;
  };  //struct TreeNode

  static const Ptr<TreeNode> NullTreeNodePtr;

  typedef RecordPool<ArenaPool<TreeNode> > TreeNode_pool;
  typedef DLFifoList<TreeNode_pool> TreeNode_list;
  typedef LocalDLFifoList<TreeNode_pool> Local_TreeNode_list;

  typedef SLList<TreeNode_pool, TreeNode_cursor_ptr>
  TreeNodeCursor_list;
  typedef LocalSLList<TreeNode_pool, TreeNode_cursor_ptr>
  Local_TreeNodeCursor_list;

  /**
   * A request (i.e a query + parameters)
   */
  struct Request
  {
    enum RequestBits
    {
      RT_SCAN                = 0x1  // unbounded result set, scan interface
      ,RT_ROW_BUFFERS        = 0x2  // Do any of the node use row-buffering
      ,RT_MULTI_SCAN         = 0x4  // Is there several scans in request
//    ,RT_VAR_ALLOC          = 0x8  // DEPRECATED
      ,RT_NEED_PREPARE       = 0x10 // Does any node need m_prepare hook
      ,RT_NEED_COMPLETE      = 0x20 // Does any node need m_complete hook
      ,RT_REPEAT_SCAN_RESULT = 0x40 // Repeat bushy scan result when required
    };

    enum RequestState
    {
      RS_BUILDING   = 0x1,
      RS_PREPARING  = 0x2,
      RS_RUNNING    = 0x3,
      RS_COMPLETING = 0x4,

      RS_ABORTING   = 0x1000, // Or:ed together with other states
      RS_WAITING    = 0x2000, // Waiting for SCAN_NEXTREQ

      RS_ABORTED    = 0x2008, // Aborted and waiting for SCAN_NEXTREQ
      RS_END = 0
    };

    Request() {}
    Request(const ArenaHead & arena) : m_arena(arena) {}
    Uint32 m_magic;
    Uint32 m_bits;
    Uint32 m_state;
    Uint32 m_errCode;
    Uint32 m_node_cnt;
    Uint32 m_senderRef;
    Uint32 m_senderData;
    Uint32 m_rootResultData;
    Uint32 m_rootFragId;
    Uint32 m_rootFragCnt;
    Uint32 m_transId[2];
    TreeNode_list::Head m_nodes;
    TreeNodeCursor_list::Head m_cursor_nodes;
    Uint32 m_cnt_active;       // No of "running" nodes
    TreeNodeBitMask
           m_active_nodes;     // Nodes which will return more data in NEXTREQ
    TreeNodeBitMask
           m_completed_nodes;  // Nodes wo/ any 'outstanding' signals
    Uint32 m_rows;             // Rows accumulated in current batch
    Uint32 m_outstanding;      // Outstanding signals, when 0, batch is done
    Uint16 m_lookup_node_data[MAX_NDB_NODES];
    ArenaHead m_arena;
    RowBuffer m_rowBuffer;

#ifdef SPJ_TRACE_TIME
    Uint32 m_cnt_batches;
    Uint32 m_sum_rows;
    Uint32 m_sum_running;
    Uint32 m_sum_waiting;
    NDB_TICKS m_save_time;
#endif

    bool isScan() const { return (m_bits & RT_SCAN) != 0;}
    bool isLookup() const { return (m_bits & RT_SCAN) == 0;}

    bool equal(const Request & key) const {
      return
	m_senderData == key.m_senderData &&
	m_transId[0] == key.m_transId[0] &&
	m_transId[1] == key.m_transId[1];
    }

    Uint32 hashValue() const {
      return m_transId[0] ^ m_senderData;
    }

    union {
      Uint32 nextHash;
      Uint32 nextPool;
    };
    Uint32 prevHash;
  }; //struct Request

private:
  /**
   * These are the rows in ndbinfo.counters that concerns the SPJ block.
   * @see Ndbinfo::counter_id.
   */
  enum CounterId
  {
    /**
     * This is the number of incomming LQHKEYREQ messages (i.e queries with a
     * lookup as root).
     */
    CI_READS_RECEIVED = 0,

    /**
     * This is the number of lookup operations (LQHKEYREQ) sent to a local
     * LQH block.
     */
    CI_LOCAL_READS_SENT = 1,

    /**
     * This is the number of lookup operations (LQHKEYREQ) sent to a remote
     * LQH block.
     */
    CI_REMOTE_READS_SENT = 2,

    /**
     * No of lookup operations which did not return a row (LQHKEYREF).
     * (Most likely due to non matching key, or predicate
     * filter which evalueted  to 'false').
     */
    CI_READS_NOT_FOUND = 3,

    /**
     * This is the number of incomming queries where the root operation is a
     * fragment scan and this is a "direct scan" that does not go via an index.
     */
    CI_TABLE_SCANS_RECEIVED = 4,

    /**
     * This is the number of "direct" fragment scans (i.e. no via an ordered
     * index)sent to the local LQH block.
     */
    CI_LOCAL_TABLE_SCANS_SENT = 5,

    /**
     * This is the number of incomming queries where the root operation is a
     * fragment scan which scans the fragment via an ordered index..
     */
    CI_RANGE_SCANS_RECEIVED = 6,

    /**
     * This the number of scans using ordered indexes that have been sent to the
     * local LQH block.
     */
    CI_LOCAL_RANGE_SCANS_SENT = 7,

    /**
     * This the number of scans using ordered indexes that have been sent to a
     * remote LQH block.
     */
    CI_REMOTE_RANGE_SCANS_SENT = 8,

    /**
     * No of scan batches (on range or full table) returned to ndbapi
     */
    CI_SCAN_BATCHES_RETURNED = 9,

    /**
     * Total no of rows returned from scans.
     */
    CI_SCAN_ROWS_RETURNED = 10,

    /**
     * No of prunable indexscans that has been received
     */
    CI_PRUNED_RANGE_SCANS_RECEIVED = 11,

    /**
     * No of "const" prunable index scans that has been received
     * i.e index-scan only access 1 partition
     */
    CI_CONST_PRUNED_RANGE_SCANS_RECEIVED = 12,

    CI_END = 13 // End marker - not a valid counter id.
  };

  /**
   * This is a set of counters for monitoring the behavior of the SPJ block.
   * They may be read through the ndbinfo.counters SQL table.
   */
  class MonotonicCounters {
  public:

    MonotonicCounters()
    {
      for(int i = 0; i < CI_END; i++)
      {
        m_counters[i] = 0;
      }
    }

    Uint64 get_counter(CounterId id) const
    {
      return m_counters[id];
    }

    void incr_counter(CounterId id, Uint64 delta)
    {
      m_counters[id] += delta;
    }

  private:
    Uint64 m_counters[CI_END];

  } c_Counters;

  typedef RecordPool<ArenaPool<Request> > Request_pool;
  typedef DLList<Request_pool> Request_list;
  typedef LocalDLList<Request_pool> Local_Request_list;
  typedef DLHashTable<Request_pool> Request_hash;
  typedef DLHashTable<Request_pool>::Iterator Request_iterator;

  ArenaAllocator m_arenaAllocator;
  Request_pool m_request_pool;
  Request_hash m_scan_request_hash;
  Request_hash m_lookup_request_hash;
  ArenaPool<DataBufferSegment<14> > m_dependency_map_pool;
  TreeNode_pool m_treenode_pool;
  ScanFragHandle_pool m_scanfraghandle_pool;

  TableRecord *m_tableRecord;
  UintR c_tabrecFilesize;

  NdbNodeBitmask c_alive_nodes;

  void do_init(Request*, const LqhKeyReq*, Uint32 senderRef);
  void store_lookup(Ptr<Request>);
  void handle_early_lqhkey_ref(Signal*, const LqhKeyReq *, Uint32 err);
  void sendTCKEYREF(Signal* signal, Uint32 ref, Uint32 routeRef);
  void sendTCKEYCONF(Signal* signal, Uint32 len, Uint32 ref, Uint32 routeRef);

  void do_init(Request*, const ScanFragReq*, Uint32 senderRef);
  void store_scan(Ptr<Request>);
  void handle_early_scanfrag_ref(Signal*, const ScanFragReq *, Uint32 err);

  bool checkRequest(const Ptr<Request>);

  struct BuildKeyReq
  {
    Uint32 hashInfo[4]; // Used for hashing
    Uint32 fragId;
    Uint32 fragDistKey;
    Uint32 receiverRef; // NodeId + InstanceNo
  };

  /**
   * Build
   */
  const OpInfo* getOpInfo(Uint32 op);
  Uint32 build(Build_context&,Ptr<Request>,SectionReader&,SectionReader&);
  Uint32 initRowBuffers(Ptr<Request>);
  void buildExecPlan(Ptr<Request>, Ptr<TreeNode> node, Ptr<TreeNode> next);
  void prepare(Signal*, Ptr<Request>);
  void checkPrepareComplete(Signal*, Ptr<Request>);
  void checkBatchComplete(Signal*, Ptr<Request>);
  void batchComplete(Signal*, Ptr<Request>);
  void prepareNextBatch(Signal*, Ptr<Request>);
  void sendConf(Signal*, Ptr<Request>, bool is_complete);
  void complete(Signal*, Ptr<Request>);
  void cleanup(Ptr<Request>);
  void cleanupBatch(Ptr<Request>);
  void abort(Signal*, Ptr<Request>, Uint32 errCode);
  Uint32 nodeFail(Signal*, Ptr<Request>, NdbNodeBitmask mask);

  Uint32 createNode(Build_context&, Ptr<Request>, Ptr<TreeNode> &);
  void handleTreeNodeComplete(Signal*, Ptr<Request>, Ptr<TreeNode>);
  void reportAncestorsComplete(Signal*, Ptr<Request>, Ptr<TreeNode>);
  void releaseScanBuffers(Ptr<Request> requestPtr);
  void releaseRequestBuffers(Ptr<Request> requestPtr);
  void releaseNodeRows(Ptr<Request> requestPtr, Ptr<TreeNode>);
  void registerActiveCursor(Ptr<Request>, Ptr<TreeNode>);
  void nodeFail_checkRequests(Signal*);
  void cleanup_common(Ptr<Request>, Ptr<TreeNode>);

  /**
   * Row buffering
   */
  Uint32 storeRow(RowCollection& collection, RowPtr &row);
  void releaseRow(RowCollection& collection, RowRef ref);
  Uint32* stackAlloc(RowBuffer& dst, RowRef&, Uint32 len);
  Uint32* varAlloc(RowBuffer& dst, RowRef&, Uint32 len);
  Uint32* rowAlloc(RowBuffer& dst, RowRef&, Uint32 len);

  void add_to_list(SLFifoRowList & list, RowRef);
  Uint32 add_to_map(RowMap& map, Uint32, RowRef);

  void setupRowPtr(const RowCollection& collection,
                   RowPtr& dst, RowRef, const Uint32 * src);
  Uint32 * get_row_ptr(RowRef pos);

  /**
   * SLFifoRowListIterator
   */
  bool first(const SLFifoRowList& list, SLFifoRowListIterator&);
  bool next(SLFifoRowListIterator&);

  /**
   * RowMapIterator
   */
  bool first(const RowMap& map, RowMapIterator&);
  bool next(RowMapIterator&);

  /**
   * RowIterator:
   * Abstraction which may iterate either a RowList or Map
   */
  bool first(const RowCollection&, RowIterator&);
  bool next(RowIterator&);

  /**
   * Misc
   */
  Uint32 buildRowHeader(RowPtr::Header *, SegmentedSectionPtr);
  Uint32 buildRowHeader(RowPtr::Header *, const Uint32 *& src, Uint32 len);
  void getCorrelationData(const RowPtr::Section & row, Uint32 col,
                          Uint32& correlationNumber);
  void getCorrelationData(const RowPtr::Linear & row, Uint32 col,
                          Uint32& correlationNumber);
  Uint32 appendToPattern(Local_pattern_store &, DABuffer & tree, Uint32);
  Uint32 appendParamToPattern(Local_pattern_store&,const RowPtr::Linear&,
                              Uint32);
  Uint32 appendParamHeadToPattern(Local_pattern_store&,const RowPtr::Linear&,
                                  Uint32);

  Uint32 appendTreeToSection(Uint32 & ptrI, SectionReader &, Uint32);
  Uint32 appendColToSection(Uint32 & ptrI, const RowPtr::Linear&, Uint32 col, bool& hasNull);
  Uint32 appendColToSection(Uint32 & ptrI, const RowPtr::Section&, Uint32 col, bool& hasNull);
  Uint32 appendPkColToSection(Uint32 & ptrI, const RowPtr::Section&,Uint32 col);
  Uint32 appendPkColToSection(Uint32 & ptrI, const RowPtr::Linear&, Uint32 col);
  Uint32 appendAttrinfoToSection(Uint32 &, const RowPtr::Linear&, Uint32 col, bool& hasNull);
  Uint32 appendAttrinfoToSection(Uint32 &, const RowPtr::Section&, Uint32 col, bool& hasNull);
  Uint32 appendDataToSection(Uint32 & ptrI, Local_pattern_store&,
			     Local_pattern_store::ConstDataBufferIterator&,
			     Uint32 len, bool& hasNull);
  Uint32 appendFromParent(Uint32 & ptrI, Local_pattern_store&,
                          Local_pattern_store::ConstDataBufferIterator&,
                          Uint32 level, const RowPtr&, bool& hasNull);
  Uint32 expand(Uint32 & ptrI, Local_pattern_store& p, const RowPtr& r, bool& hasNull){
    switch(r.m_type){
    case RowPtr::RT_SECTION:
      return expandS(ptrI, p, r, hasNull);
    case RowPtr::RT_LINEAR:
      return expandL(ptrI, p, r, hasNull);
    }
    return DbspjErr::InternalError;
  }
  Uint32 expandS(Uint32 & ptrI, Local_pattern_store&, const RowPtr&, bool& hasNull);
  Uint32 expandL(Uint32 & ptrI, Local_pattern_store&, const RowPtr&, bool& hasNull);
  Uint32 expand(Uint32 & ptrI, DABuffer& pattern, Uint32 len,
                DABuffer & param, Uint32 cnt, bool& hasNull);
  Uint32 expand(Local_pattern_store& dst, Ptr<TreeNode> treeNodePtr,
                DABuffer & pattern, Uint32 len,
                DABuffer & param, Uint32 cnt);
  Uint32 parseDA(Build_context&, Ptr<Request>, Ptr<TreeNode>,
                 DABuffer & tree, Uint32 treeBits,
                 DABuffer & param, Uint32 paramBits);

  Uint32 getResultRef(Ptr<Request> requestPtr);

  Uint32 checkTableError(Ptr<TreeNode> treeNodePtr) const;
  Uint32 getNodes(Signal*, BuildKeyReq&, Uint32 tableId);

  void common_execTRANSID_AI(Signal*, Ptr<Request>, Ptr<TreeNode>,
			     const RowPtr&);

  void dumpScanFragHandle(const Ptr<ScanFragHandle> fragPtr) const;
  void dumpNodeCommon(const Ptr<TreeNode>) const;
  void dumpRequest(const char* reason,
                   const Ptr<Request> requestPtr);

  /**
   * Lookup
   */
  static const OpInfo g_LookupOpInfo;
  Uint32 lookup_build(Build_context&,Ptr<Request>,
		      const QueryNode*, const QueryNodeParameters*);
  void lookup_start(Signal*, Ptr<Request>, Ptr<TreeNode>);
  void lookup_resume(Signal*, Ptr<Request>, Ptr<TreeNode>);
  void lookup_send(Signal*, Ptr<Request>, Ptr<TreeNode>);
  bool lookup_countSignal(const Signal*, Ptr<Request>, Ptr<TreeNode>);
  void lookup_execLQHKEYREF(Signal*, Ptr<Request>, Ptr<TreeNode>);
  void lookup_execLQHKEYCONF(Signal*, Ptr<Request>, Ptr<TreeNode>);
  void lookup_stop_branch(Signal*, Ptr<Request>, Ptr<TreeNode>, Uint32 err);
  void lookup_parent_row(Signal*, Ptr<Request>, Ptr<TreeNode>, const RowPtr &);
  void lookup_row(Signal*, Ptr<Request>, Ptr<TreeNode>, const RowPtr &);
  void lookup_abort(Signal*, Ptr<Request>, Ptr<TreeNode>);
  Uint32 lookup_execNODE_FAILREP(Signal*signal, Ptr<Request>, Ptr<TreeNode>,
                               NdbNodeBitmask);

  void lookup_sendLeafCONF(Signal*, Ptr<Request>, Ptr<TreeNode>,
                           Uint32 node);
  void lookup_cleanup(Ptr<Request>, Ptr<TreeNode>);

  Uint32 handle_special_hash(Uint32 tableId, Uint32 dstHash[4],
                             const Uint64* src,
                             Uint32 srcLen,       // Len in #32bit words
                             const struct KeyDescriptor* desc);

  Uint32 computeHash(Signal*, BuildKeyReq&, Uint32 table, Uint32 keyInfoPtrI);
  Uint32 computePartitionHash(Signal*, BuildKeyReq&, Uint32 table, Uint32 keyInfoPtrI);
  bool lookup_checkNode(const Ptr<Request> requestPtr, 
                        const Ptr<TreeNode> treeNodePtr);
  void lookup_dumpNode(const Ptr<Request> requestPtr,
                       const Ptr<TreeNode> treeNodePtr);

  /**
   * ScanFrag
   */
  static const OpInfo g_ScanFragOpInfo;
  Uint32 scanFrag_build(Build_context&, Ptr<Request>,
                        const QueryNode*, const QueryNodeParameters*);
  Uint32 parseScanFrag(Build_context&, Ptr<Request>, Ptr<TreeNode>,
                       DABuffer tree, Uint32 treeBits,
                       DABuffer param, Uint32 paramBits);
  void scanFrag_start(Signal*, Ptr<Request>,Ptr<TreeNode>);
  void scanFrag_prepare(Signal*, Ptr<Request>, Ptr<TreeNode>);
  bool scanFrag_countSignal(const Signal*, Ptr<Request>, Ptr<TreeNode>);
  void scanFrag_execSCAN_FRAGREF(Signal*, Ptr<Request>, Ptr<TreeNode>,
                                 Ptr<ScanFragHandle>);
  void scanFrag_execSCAN_FRAGCONF(Signal*, Ptr<Request>, Ptr<TreeNode>,
                                  Ptr<ScanFragHandle>);
  void scanFrag_parent_row(Signal*,Ptr<Request>,Ptr<TreeNode>, const RowPtr&);
  void scanFrag_fixupBound(Ptr<ScanFragHandle> fragPtr, Uint32 ptrI, Uint32);
  void scanFrag_send(Signal*, Ptr<Request>, Ptr<TreeNode>);
  Uint32 scanFrag_send(Signal* signal,
                       Ptr<Request> requestPtr,
                       Ptr<TreeNode> treeNodePtr,
                       Uint32 noOfFrags,
                       Uint32 bs_bytes,
                       Uint32 bs_rows,
                       Uint32& batchRange);
  void scanFrag_batchComplete(Signal* signal);
  Uint32 scanFrag_findFrag(Local_ScanFragHandle_list &, Ptr<ScanFragHandle>&,
                           Uint32 fragId);
  void scanFrag_parent_batch_complete(Signal*, Ptr<Request>, Ptr<TreeNode>);
  void scanFrag_parent_batch_repeat(Signal*, Ptr<Request>, Ptr<TreeNode>);
  void scanFrag_execSCAN_NEXTREQ(Signal*, Ptr<Request>,Ptr<TreeNode>);
  void scanFrag_complete(Signal*, Ptr<Request>, Ptr<TreeNode>);
  void scanFrag_abort(Signal*, Ptr<Request>, Ptr<TreeNode>);
  Uint32 scanFrag_execNODE_FAILREP(Signal*signal, Ptr<Request>, Ptr<TreeNode>,
                                   NdbNodeBitmask);
  void scanFrag_parent_batch_cleanup(Ptr<Request>, Ptr<TreeNode>);
  void scanFrag_cleanup(Ptr<Request>, Ptr<TreeNode>);

  void scanFrag_release_rangekeys(Ptr<Request>, Ptr<TreeNode>);

  Uint32 scanFrag_sendDihGetNodesReq(Signal* signal,
                                     Ptr<Request> requestPtr,
                                     Ptr<TreeNode> treeNodePtr);

  bool scanFrag_checkNode(const Ptr<Request> requestPtr,
                          const Ptr<TreeNode> treeNodePtr);

  void scanFrag_dumpNode(const Ptr<Request> requestPtr,
                         const Ptr<TreeNode> treeNodePtr);

  /**
   * Page manager
   */
  bool allocPage(Ptr<RowPage> &);
  void releasePage(Ptr<RowPage>);
  void releasePages(Uint32 first, Ptr<RowPage> last);
  void releaseGlobal(Signal*);
  RowPage_list::Head m_free_page_list;
  RowPage_pool m_page_pool;

  /* Random fault injection */

#ifdef ERROR_INSERT
  bool appendToSection(Uint32& firstSegmentIVal,
                       const Uint32* src, Uint32 len);
#endif

  /**
   * Scratch buffers...
   */
  Uint32 m_buffer0[16*1024]; // 64k
  Uint32 m_buffer1[16*1024]; // 64k
};


#undef JAM_FILE_ID

#endif
