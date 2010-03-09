/* Copyright (C) 2003 MySQL AB

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

#ifndef DBSPJ_H
#define DBSPJ_H

#include <SimulatedBlock.hpp>
#include <signaldata/LqhKey.hpp>
#include <signaldata/ScanFrag.hpp>
#include <AttributeHeader.hpp>
#include <DLFifoList.hpp>
#include <ArenaPool.hpp>
#include <DataBuffer2.hpp>

class SectionReader;
struct QueryNode;
struct QueryNodeParameters;

class Dbspj: public SimulatedBlock {
public:
  Dbspj(Block_context& ctx, Uint32 instanceNumber = 0);
  virtual ~Dbspj();

private:
  BLOCK_DEFINES(Dbspj);

  /**
   * Signals from TC
   */
  void execLQHKEYREQ(Signal* signal);
  void execSCAN_FRAGREQ(Signal* signal);
  void execSCAN_NEXTREQ(Signal* signal);

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
  void execREAD_CONFIG_REQ(Signal* signal);
  void execSTTOR(Signal* signal);
  void execDBINFO_SCANREQ(Signal* signal); 

protected:
  //virtual bool getParam(const char* name, Uint32* count);

public:
  struct Request;
  struct TreeNode;
  typedef DataBuffer2<14, LocalArenaPoolImpl> Dependency_map;
  typedef LocalDataBuffer2<14, LocalArenaPoolImpl> Local_dependency_map;
  typedef DataBuffer2<14, LocalArenaPoolImpl> PatternStore;
  typedef LocalDataBuffer2<14, LocalArenaPoolImpl> Local_pattern_store;

  /**
   * This struct represent a row being passed to a child
   *   currently only the RT_SECTION type is supported
   *   but RT_ROW_BUF is also planned (for buffered rows)
   *     that will be used for equi-join (and increased parallelism on scans)
   */
  struct RowRef
  {
    Uint32 m_type;
    Uint32 m_src_node_no;
    Uint32 m_src_node_ptrI;
    Uint32 m_src_correlation;

    struct Header
    {
      Uint32 m_len;
      AttributeHeader m_headers[1];
    };

    struct Section
    {
      const Header * m_header;
      SegmentedSectionPtrPOD m_dataPtr;
    };

    struct Linear
    {
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

  /**
   * A struct used when building an TreeNode
   */
  struct Build_context
  {
    Uint32 m_cnt;
    Uint32 m_scanPrio;
    Uint32 m_savepointId;
    Uint32 m_resultRef;  // API
    Uint32 m_resultData; // API
    Ptr<TreeNode> m_node_list[63]; // Used for resolving dependencies
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
     * This function is used for starting a request
     */
    void (Dbspj::*m_start)(Signal*, Ptr<Request>, Ptr<TreeNode>,
			   SegmentedSectionPtr);

    /**
     * This function is used when getting a TRANSID_AI
     */
    void (Dbspj::*m_execTRANSID_AI)(Signal*,Ptr<Request>,Ptr<TreeNode>,
				    const RowRef&);

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
    void (Dbspj::*m_execSCAN_FRAGREF)(Signal*, Ptr<Request>, Ptr<TreeNode>);

    /**
     * This function is used when getting a SCAN_FRAGCONF
     */
    void (Dbspj::*m_execSCAN_FRAGCONF)(Signal*, Ptr<Request>, Ptr<TreeNode>);

    /**
     * This function is called on the *child* by the *parent* when passing rows
     */
    void (Dbspj::*m_start_child)(Signal*,Ptr<Request>,Ptr<TreeNode>,
                                 const RowRef&);

    /**
     * This function is called when getting a SCAN_NEXTREQ
     */
    void (Dbspj::*m_execSCAN_NEXTREQ)(Signal*, Ptr<Request>,Ptr<TreeNode>);

    /**
     * This function is called when all nodes in tree are finished
     *   it's allowed to "block" (by increaseing requestPtr.p->m_cnt_active)
     */
    void (Dbspj::*m_complete)(Signal*, Ptr<Request>,Ptr<TreeNode>);

    /**
     * This function is called when a tree is aborted
     *   it's allowed to "block" (by increaseing requestPtr.p->m_cnt_active)
     */
    void (Dbspj::*m_abort)(Signal*, Ptr<Request>,Ptr<TreeNode>);

    /**
     * This function is called when request/node(s) is/are removed
     *  should only do local cleanup(s)
     */
    void (Dbspj::*m_cleanup)(Ptr<Request>, Ptr<TreeNode>);

    /**
     * This function is called on the root operation  when a LQHKEYCONF, 
     * LQKEYREF or LQHKEYREQ signal is sent or received on behalf of a 
     * descendant operation*/
    void (Dbspj::*m_count_descendant_signal)(Signal* signal,
                                             Ptr<Request> requestPtr,
                                             Ptr<TreeNode> treeNodePtr,
                                             Ptr<TreeNode> rootPtr,
                                             Uint32 globalSignalNo);
  };

  struct LookupData
  {
    Uint32 m_api_resultRef;
    Uint32 m_api_resultData;
    Uint32 m_outstanding;
    Uint32 m_lqhKeyReq[LqhKeyReq::FixedSignalLength + 4];
  };

  struct ScanFragData
  {
    enum ScanFragState
    {
      /**
       * Nothing oustanding
       */
      SF_IDLE = 0,

      /**
       * SCAN_FRAGREQ/SCAN_NEXTREQ is sent
       */
      SF_RUNNING = 1,

      /**
       * SCAN_FRAGCONF is received
       */
      SF_STARTED = 2,

      /**
       * SCAN_NEXTREQ(close) has been sent to datanodes
       */
      SF_CLOSING = 3
    };

    Uint32 m_scan_state;     // Only valid if TreeNodeState >= TN_ACTIVE
    Uint32 m_scan_status;    // fragmentCompleted
    bool   m_pending_close;  // SCAN_NEXTREQ(close) pending while SF_RUNNING
    /** True if signal has been received since sending 
     * last SCAN_FRAGREQ/SCAN_NEXTREQ*/
    bool   m_scan_fragconf_received; 
    Uint32 m_rows_received;  // #execTRANSID_AI
    Uint32 m_rows_expecting; // ScanFragConf
    /** Number of receiced LQHKEYCONF messages from descendant lookup 
     * operations which has user projections.*/
    Uint32 m_descendant_keyconfs_received;
    /** Number of receiced LQHKEYCONF messages from descendant lookup 
     * operations which has no user projections.*/
    Uint32 m_descendant_silent_keyconfs_received;
    /** Number of received LQHKEYREF messages from descendant lookup 
     * operations.*/
    Uint32 m_descendant_keyrefs_received;
    /** Number of LQHKEYREQ messages sent for descendant lookup operations.*/
    Uint32 m_descendant_keyreqs_sent;
    /** Number of missing transid AI messages for descendant lookup operations.
     * This is decremented when we receive TRANSID_AI, and incremented when
     * we receive LQHKEYCONF for a non-leaf operation. (For leaf operations,
     * no TRANSID_AI is sent to the SPJ block.)*/
    int m_missing_descendant_rows;
    Uint32 m_scanFragReq[ScanFragReq::SignalLength + 2];
  };

  /**
   * A node in a Query
   *   (This is an instantiated version of QueryNode in
   *    include/kernel/signal/QueryTree.hpp)
   */
  struct TreeNode
  {
    STATIC_CONST ( MAGIC = ~RT_SPJ_TREENODE );

    TreeNode()
    : m_magic(MAGIC), m_state(TN_END),
      m_node_no(0), m_requestPtrI(0)
    {}

    TreeNode(Uint32 node_no, Uint32 request)
    : m_magic(MAGIC),
      m_info(0), m_bits(T_LEAF), m_state(TN_BUILDING),
      m_node_no(node_no), m_requestPtrI(request),
      nextList(RNIL), prevList(RNIL)
    {
//    m_send.m_ref = 0;
      m_send.m_correlation = 0;
      m_send.m_keyInfoPtrI = RNIL;
      m_send.m_attrInfoPtrI = RNIL;
      m_send.m_attrInfoParamPtrI = RNIL;
    };

    const Uint32 m_magic;
    const struct OpInfo* m_info;

    enum TreeNodeState
    {
      /**
       * Initial
       */
      TN_BUILDING = 1,

      /**
       * Tree node is build, but not active
       */
      TN_INACTIVE = 2,

      /**
       * Tree node is active (i.e has outstanding request(s))
       */
      TN_ACTIVE = 3,

      /**
       * Tree node is "finishing" (after TN_INACTIVE)
       */
      TN_COMPLETING = 5,

      /**
       * Tree node is aborting
       */
      TN_ABORTING = 6,

      /**
       * end-marker, not a valid state
       */
      TN_END = 0
    };

    enum TreeNodeBits
    {
      T_ATTR_INTERPRETED = 0x1,

      /**
       * Will this request be executed only once
       *   (implies key/attr-info will be disowned (by send-signal)
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

      // End marker...
      T_END = 0
    };

    bool isLeaf() const { return (m_bits & T_LEAF) != 0;}

    Uint32 m_bits;
    Uint32 m_state;
    const Uint32 m_node_no;
    const Uint32 m_requestPtrI;
    Dependency_map::Head m_dependent_nodes;
    PatternStore::Head m_keyPattern;
    PatternStore::Head m_attrParamPattern;

    union
    {
      LookupData m_lookup_data;
      ScanFragData m_scanfrag_data;
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
      Uint32 m_attrInfoParamPtrI;// attrInfoParamSection
    } m_send;

    union {
      Uint32 nextList;
      Uint32 nextPool;
    };
    Uint32 prevList;
  };

  typedef RecordPool<TreeNode, ArenaPool> TreeNode_pool;
  typedef DLFifoListImpl<TreeNode_pool, TreeNode> TreeNode_list;
  typedef LocalDLFifoListImpl<TreeNode_pool, TreeNode> Local_TreeNode_list;

  /**
   * A request (i.e a query + parameters)
   */
  struct Request
  {
    Request() {}
    Request(const ArenaHead & arena) : m_arena(arena) {}
    Uint32 m_magic;
    Uint32 m_bits;
    Uint32 m_node_cnt;
    Uint32 m_senderRef;
    Uint32 m_senderData;
    Uint32 m_rootResultData;
    Uint32 m_transId[2];
    NdbNodeBitmask m_node_mask; // Dependant data nodes...
    TreeNode_list::Head m_nodes;
    Uint32 m_currentNodePtrI;
    Uint32 m_cnt_active;       // No of "running" nodes
    ArenaHead m_arena;

    enum RequestBits
    {
      RT_SCAN = 0x1  // unbounded result set, scan interface
    };

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
  };

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
     * This is the number of incomming queries where the root operation is a 
     * fragment scan and this is a "direct scan" that does not go via an index.
     */
    CI_TABLE_SCANS_RECEIVED = 3,

    /**
     * This is the number of "direct" fragment scans (i.e. no via an ordered 
     * index)sent to the local LQH block.
     */
    CI_LOCAL_TABLE_SCANS_SENT = 4,

    /**
     * This is the number of incomming queries where the root operation is a 
     * fragment scan which scans the fragment via an ordered index..
     */
    CI_RANGE_SCANS_RECEIVED = 5,

    /**
     * This the number of scans using ordered indexes that have been sent to the
     * local LQH block.
     */
    CI_LOCAL_RANGE_SCANS_SENT = 6,

    CI_END = 7 // End marker - not a valid counter id. 
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

  typedef RecordPool<Request, ArenaPool> Request_pool;
  typedef DLListImpl<Request_pool, Request> Request_list;
  typedef LocalDLListImpl<Request_pool, Request> Local_Request_list;
  typedef DLHashTableImpl<Request_pool, Request> Request_hash;

  ArenaAllocator m_arenaAllocator;
  Request_pool m_request_pool;
  Request_hash m_scan_request_hash;
  Request_hash m_lookup_request_hash;
  ArenaPool m_dependency_map_pool;
  TreeNode_pool m_treenode_pool;

  void do_init(Request*, const LqhKeyReq*, Uint32 senderRef);
  void store_lookup(Ptr<Request>);
  void handle_early_lqhkey_ref(Signal*, const LqhKeyReq *, Uint32 err);
  void sendTCKEYREF(Signal* signal, Uint32 ref, Uint32 routeRef);

  void do_init(Request*, const ScanFragReq*, Uint32 senderRef);
  void store_scan(Ptr<Request>);
  void handle_early_scanfrag_ref(Signal*, const ScanFragReq *, Uint32 err);

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
  Uint32 build(Build_context&, Ptr<Request>, SectionReader&, SectionReader&);
  Uint32 createNode(Build_context&, Ptr<Request>, Ptr<TreeNode> &);
  void start(Signal*, Ptr<Request>, SegmentedSectionPtr);
  void nodeFinished(Signal* signal, Ptr<Request>, Ptr<TreeNode>);
  void cleanup(Ptr<Request>);
  void cleanup_common(Ptr<Request>, Ptr<TreeNode>);

  /**
   * Misc
   */
  Uint32 buildRowHeader(RowRef::Header *, SegmentedSectionPtr);
  Uint32 buildRowHeader(RowRef::Header *, const Uint32 *& src, Uint32 len);
  void getCorrelationData(const RowRef::Section & row, Uint32 col,
                          Uint32& rootStreamId, Uint32& correlationNumber);
  Uint32 appendToPattern(Local_pattern_store &, DABuffer & tree, Uint32);
  Uint32 appendColToPattern(Local_pattern_store&,const RowRef::Linear&, Uint32);

  Uint32 appendTreeToSection(Uint32 & ptrI, SectionReader &, Uint32);
  Uint32 appendColToSection(Uint32 & ptrI, const RowRef::Linear&, Uint32 col);
  Uint32 appendColToSection(Uint32 & ptrI, const RowRef::Section&, Uint32 col);
  Uint32 appendPkColToSection(Uint32 & ptrI, const RowRef::Section&, Uint32 col);
  Uint32 appendDataToSection(Uint32 & ptrI, Local_pattern_store&,
			     Local_pattern_store::ConstDataBufferIterator&,
			     Uint32 len);
  Uint32 expand(Uint32 & ptrI, Local_pattern_store&, const RowRef::Section&);
  Uint32 expand(Uint32 & ptrI, DABuffer& pattern, Uint32 len,
                DABuffer & param, Uint32 cnt);
  Uint32 expand(Local_pattern_store& dst, DABuffer& pattern, Uint32 len,
                DABuffer & param, Uint32 cnt);
  Uint32 parseDA(Build_context&, Ptr<Request>, Ptr<TreeNode>,
                 DABuffer tree, Uint32 treeBits,
                 DABuffer param, Uint32 paramBits);

  Uint32 zeroFill(Uint32 & ptrI, Uint32 cnt);
  Uint32 createEmptySection(Uint32 & ptrI);

  /** Find root operation.*/
  const Ptr<TreeNode> getRoot(TreeNode_list::Head& head);
  
  /**
   * Lookup
   */
  static const OpInfo g_LookupOpInfo;
  Uint32 lookup_build(Build_context&,Ptr<Request>,
		      const QueryNode*, const QueryNodeParameters*);
  void lookup_start(Signal*, Ptr<Request>, Ptr<TreeNode>, SegmentedSectionPtr);
  void lookup_send(Signal*, Ptr<Request>, Ptr<TreeNode>);
  void lookup_execTRANSID_AI(Signal*, Ptr<Request>, Ptr<TreeNode>,
			     const RowRef&);
  void lookup_execLQHKEYREF(Signal*, Ptr<Request>, Ptr<TreeNode>);
  void lookup_execLQHKEYCONF(Signal*, Ptr<Request>, Ptr<TreeNode>);
  void lookup_start_child(Signal*, Ptr<Request>, Ptr<TreeNode>, const RowRef &);
  void lookup_cleanup(Ptr<Request>, Ptr<TreeNode>);
  void lookup_count_descendant_signal(Signal* signal,
                                      Ptr<Request> requestPtr,
                                      Ptr<TreeNode> treeNodePtr,
                                      Ptr<TreeNode> rootPtr,
                                      Uint32 globalSignalNo){};

  Uint32 handle_special_hash(Uint32 tableId, Uint32 dstHash[4],
                             const Uint64* src,
                             Uint32 srcLen,       // Len in #32bit words
                             const struct KeyDescriptor* desc);

  Uint32 computeHash(Signal*, BuildKeyReq&, Uint32 table, Uint32 keyInfoPtrI);
  Uint32 getNodes(Signal*, BuildKeyReq&, Uint32 tableId);

  /**
   * ScanFrag
   */
  static const OpInfo g_ScanFragOpInfo;
  Uint32 scanFrag_build(Build_context&, Ptr<Request>,
                        const QueryNode*, const QueryNodeParameters*);
  void scanFrag_start(Signal*, Ptr<Request>,Ptr<TreeNode>,SegmentedSectionPtr);
  void scanFrag_send(Signal*, Ptr<Request>, Ptr<TreeNode>);
  void scanFrag_execTRANSID_AI(Signal*, Ptr<Request>, Ptr<TreeNode>,
			       const RowRef &);
  void scanFrag_execSCAN_FRAGREF(Signal*, Ptr<Request>, Ptr<TreeNode>);
  void scanFrag_execSCAN_FRAGCONF(Signal*, Ptr<Request>, Ptr<TreeNode>);
  void scanFrag_batch_complete(Signal*, Ptr<Request>, Ptr<TreeNode>);
  void scanFrag_start_child(Signal*,Ptr<Request>,Ptr<TreeNode>, const RowRef &);
  void scanFrag_execSCAN_NEXTREQ(Signal*, Ptr<Request>,Ptr<TreeNode>);
  void scanFrag_cleanup(Ptr<Request>, Ptr<TreeNode>);
  void scanFrag_count_descendant_signal(Signal* signal,
                                        Ptr<Request> requestPtr,
                                        Ptr<TreeNode> treeNodePtr,
                                        Ptr<TreeNode> rootPtr,
                                        Uint32 globalSignalNo);

  /**
   * Scratch buffers...
   */
  Uint32 m_buffer0[8192]; // 32k
  Uint32 m_buffer1[8192]; // 32k
};

#endif
