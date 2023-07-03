/*
   Copyright (c) 2003, 2022, Oracle and/or its affiliates.

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

#ifndef DBTUX_H
#define DBTUX_H

#include <ndb_limits.h>
#include <SimulatedBlock.hpp>
#include <AttributeDescriptor.hpp>
#include <AttributeHeader.hpp>
#include <ArrayPool.hpp>
#include <DataBuffer.hpp>
#include <IntrusiveList.hpp>
#include <md5_hash.hpp>

// big brother
#include <dbtup/Dbtup.hpp>
#include <dblqh/Dblqh.hpp>
#include <dbacc/Dbacc.hpp>

// packed index keys and bounds
#include <NdbPack.hpp>

// signal classes
#include <signaldata/DictTabInfo.hpp>
#include <signaldata/TuxContinueB.hpp>
#include <signaldata/TupFrag.hpp>
#include <signaldata/AlterIndxImpl.hpp>
#include <signaldata/DropTab.hpp>
#include <signaldata/TuxMaint.hpp>
#include <signaldata/AccScan.hpp>
#include <signaldata/TuxBound.hpp>
#include <signaldata/NextScan.hpp>
#include <signaldata/AccLock.hpp>
#include <signaldata/DumpStateOrd.hpp>
#include <signaldata/IndexStatSignal.hpp>

// debug
#ifdef VM_TRACE
#include <NdbOut.hpp>
#include <OutputStream.hpp>
#endif


#define JAM_FILE_ID 374


#undef max
#undef min

class Configuration;
struct mt_BuildIndxCtx;

class Dbtux : public SimulatedBlock {
  friend class Dbqtux;
  friend class DbtuxProxy;
  friend struct mt_BuildIndxCtx;
  friend Uint32 Dbtux_mt_buildIndexFragment_wrapper_C(void*);

  Uint32 m_acc_block;
  Uint32 m_lqh_block;
  Uint32 m_tux_block;
  bool m_is_query_block;
  Uint32 m_my_scan_instance;
 public:
  Dbtux(Block_context& ctx,
        Uint32 instanceNumber = 0,
        Uint32 blockNo = DBTUX);
  ~Dbtux() override;

  void prepare_scan_ctx(Uint32 scanPtrI) override;
  // pointer to TUP and LQH instance in this thread
  Dbtup* c_tup;
  Dblqh* c_lqh;
  Dbacc* c_acc;
  void execTUX_BOUND_INFO(Signal* signal);
  void execREAD_PSEUDO_REQ(Uint32 scanPtrI, Uint32 attrId, Uint32* out, Uint32 out_words);

private:
  // sizes are in words (Uint32)
  static constexpr Uint32 MaxIndexFragments = MAX_FRAG_PER_LQH;
  static constexpr Uint32 MaxIndexAttributes = MAX_ATTRIBUTES_IN_INDEX;
  static constexpr Uint32 MaxAttrDataSize = 2 * MAX_ATTRIBUTES_IN_INDEX + MAX_KEY_SIZE_IN_WORDS;
  static constexpr Uint32 MaxXfrmDataSize = MaxAttrDataSize * MAX_XFRM_MULTIPLY;
public:
  static constexpr Uint32 DescPageSize = 512;
private:
  static constexpr Uint32 MaxTreeNodeSize = MAX_TTREE_NODE_SIZE;
  static constexpr Uint32 MaxPrefSize = MAX_TTREE_PREF_SIZE;
  static constexpr Uint32 ScanBoundSegmentSize = 7;
  static constexpr Uint32 MaxAccLockOps = MAX_PARALLEL_OP_PER_SCAN;
  static constexpr Uint32 MaxTreeDepth = 32;    // strict
#ifdef VM_TRACE
  // for TuxCtx::c_debugBuffer
  static constexpr Uint32 DebugBufferBytes = (MaxAttrDataSize << 2);
#endif
  BLOCK_DEFINES(Dbtux);

  // forward declarations
  struct TuxCtx;

  // AttributeHeader size is assumed to be 1 word
  static constexpr Uint32 AttributeHeaderSize = 1;

  /*
   * Logical tuple address, "local key".  Identifies table tuples.
   */
  typedef Uint32 TupAddr;
  static constexpr Uint32 NullTupAddr = (Uint32)-1;

  /*
   * Physical tuple address in TUP.  Provides fast access to table tuple
   * or index node.  Valid within the db node and across timeslices.
   * Not valid between db nodes or across restarts.
   *
   * To avoid wasting an Uint16 the pageid is split in two.
   */
  struct TupLoc {
  private:
    Uint16 m_pageId1;           // page i-value (big-endian)
    Uint16 m_pageId2;
    Uint16 m_pageOffset;        // page offset in words
  public:
    TupLoc();
    TupLoc(Uint32 pageId, Uint16 pageOffset);
    Uint32 getPageId() const;
    void setPageId(Uint32 pageId);
    Uint32 getPageOffset() const;
    void setPageOffset(Uint32 pageOffset);
    bool operator==(const TupLoc& loc) const;
    bool operator!=(const TupLoc& loc) const;
  };

  /*
   * There is no const member NullTupLoc since the compiler may not be
   * able to optimize it to TupLoc() constants.  Instead null values are
   * constructed on the stack with TupLoc().
   */
#define NullTupLoc TupLoc()

  // tree definitions

  /*
   * Tree entry.  Points to a tuple in primary table via physical
   * address of "original" tuple and tuple version.
   *
   * ZTUP_VERSION_BITS must be 15 (or less).
   */
  struct TreeEnt;
  friend struct TreeEnt;
  struct TreeEnt {
    TupLoc m_tupLoc;            // address of original tuple
    unsigned m_tupVersion : 15; // version
    TreeEnt();
    // methods
    bool eqtuple(const TreeEnt ent) const;
    bool eq(const TreeEnt ent) const;
    int cmp(const TreeEnt ent) const;
  };
  static constexpr Uint32 TreeEntSize = sizeof(TreeEnt) >> 2;
  static const TreeEnt NullTreeEnt;

  /*
   * Tree node has 3 parts:
   *
   * 1) struct TreeNode - the header (6 words)
   * 2) some key values for min entry - the min prefix
   * 3) list of TreeEnt (each 2 words)
   *
   * There are 3 links to other nodes: left child, right child, parent.
   * Occupancy (number of entries) is at least 1 except temporarily when
   * a node is about to be removed.
   */
  struct TreeNode;
  friend struct TreeNode;
  struct TreeNode {
    TupLoc m_link[3];           // link to 0-left child 1-right child 2-parent
    unsigned m_side : 2;        // we are 0-left child 1-right child 2-root
    unsigned m_balance : 2;     // balance -1, 0, +1 plus 1 for Solaris CC
    unsigned pad1 : 4;
    Uint8 m_occup;              // current number of entries
    Uint32 m_nodeScanPtrI;      // list of scans at this node
    Uint32 m_nodeScanInstance;
    TreeNode();
  };
  static constexpr Uint32 NodeHeadSize = sizeof(TreeNode) >> 2;

  /*
   * Tree header.  There is one in each fragment.  Contains tree
   * parameters and address of root node.
   */
  struct TreeHead;
  friend struct TreeHead;
  struct TreeHead {
    Uint8 m_nodeSize;           // words in tree node
    Uint8 m_prefSize;           // words in min prefix
    Uint8 m_minOccup;           // min entries in internal node
    Uint8 m_maxOccup;           // max entries in node
    TupLoc m_root;              // root node
    TreeHead();
    // methods
    Uint32* getPref(TreeNode* node) const;
    TreeEnt* getEntList(TreeNode* node) const;
  };

  /*
   * Tree position.  Specifies node, position within node (from 0 to
   * m_occup), and whether the position is at an existing entry or
   * before one (if any).  Position m_occup points past the node and is
   * also represented by position 0 of next node.  Includes direction
   * used by scan.
   */
  struct TreePos;
  friend struct TreePos;
  struct TreePos {
    TupLoc m_loc;               // physical node address
    Uint32 m_pos;               // position 0 to m_occup
    Uint32 m_dir;                // see scanNext
    TreePos();
  };

  // packed metadata

  /*
   * Descriptor page.  The "hot" metadata for an index is stored as
   * contiguous array of words on some page.  It has 3 parts:
   * 1) DescHead
   * 2) array of NdbPack::Type used by NdbPack::Spec of index key
   * 3) array of attr headers for reading index key values from TUP
   */
  struct DescPage;
  friend struct DescPage;
  struct DescPage {
    Uint32 m_nextPage;
    Uint32 m_numFree;           // number of free words
    union {
    Uint32 m_data[DescPageSize];
    Uint32 nextPool;
    };
    DescPage();
  };
  typedef Ptr<DescPage> DescPagePtr;
  typedef ArrayPool<DescPage> DescPage_pool;

public:
  DescPage_pool c_descPagePool;
private:
  Uint32 c_descPageList;

  struct DescHead {
    Uint32 m_indexId;
    Uint16 m_numAttrs;
    Uint16 m_magic;
    enum { Magic = 0xDE5C };
  };
  static constexpr Uint32 DescHeadSize = sizeof(DescHead) >> 2;

  typedef NdbPack::Type KeyType;
  typedef NdbPack::Spec KeySpec;
  static constexpr Uint32 KeyTypeSize = sizeof(KeyType) >> 2;

  typedef NdbPack::DataC KeyDataC;
  typedef NdbPack::Data KeyData;
  typedef NdbPack::BoundC KeyBoundC;
  typedef NdbPack::Bound KeyBound;
  typedef NdbPack::DataArray KeyDataArray;
  typedef NdbPack::BoundArray KeyBoundArray;

  // range scan

  /*
   * ScanBound instances are members of ScanOp.  Bound data is stored in
   * a separate segmented buffer pool.
   */
  typedef DataBufferSegment<ScanBoundSegmentSize, RT_DBTUX_SCAN_BOUND>
            ScanBoundSegment;
  typedef TransientPool<ScanBoundSegment> ScanBoundBuffer_pool;
  static constexpr Uint32 DBTUX_SCAN_BOUND_TRANSIENT_POOL_INDEX = 2;
  typedef DataBuffer<ScanBoundSegmentSize,
                     ScanBoundBuffer_pool,
                     RT_DBTUX_SCAN_BOUND> ScanBoundBuffer;
  typedef LocalDataBuffer<ScanBoundSegmentSize,
                          ScanBoundBuffer_pool,
                          RT_DBTUX_SCAN_BOUND> LocalScanBoundBuffer;
  struct ScanBound {
    ScanBoundBuffer::Head m_head;
    Uint16 m_cnt;       // number of attributes
    Int16 m_side;
    ScanBound();
  };
  ScanBoundBuffer_pool c_scanBoundPool;

  // ScanLock
  struct ScanLock {
    static constexpr Uint32 TYPE_ID = RT_DBTUX_SCAN_LOCK;
    Uint32 m_magic;

    ScanLock() :
      m_magic(Magic::make(TYPE_ID))
    {
    }
    ~ScanLock()
    {
    }
    Uint32 m_accLockOp;
    union {
    Uint32 nextPool;
    Uint32 nextList;
    };
    Uint32 prevList;
  };
  static constexpr Uint32 DBTUX_SCAN_LOCK_TRANSIENT_POOL_INDEX = 1;
  typedef Ptr<ScanLock> ScanLockPtr;
  typedef TransientPool<ScanLock> ScanLock_pool;
  typedef DLFifoList<ScanLock_pool> ScanLock_fifo;
  typedef LocalDLFifoList<ScanLock_pool> Local_ScanLock_fifo;
  typedef ConstLocalDLFifoList<ScanLock_pool> ConstLocal_ScanLock_fifo;
  Uint32 c_freeScanLock;
  ScanLock_pool c_scanLockPool;
 
  /*
   * Scan operation.
   *
   * Tuples are locked one at a time.  The current lock op is set to
   * RNIL as soon as the lock is obtained and passed to LQH.  We must
   * however remember all locks which LQH has not returned for unlocking
   * since they must be aborted by us when the scan is closed.
   *
   * Scan state describes the entry we are interested in.  There is
   * a separate lock wait flag.  It may be for current entry or it may
   * be for an entry we were moved away from.  In any case nothing
   * happens with current entry before lock wait flag is cleared.
   *
   * An unfinished scan is always linked to some tree node, and has
   * current position and direction (see comments at scanNext).  There
   * is also a copy of latest entry found.
   *
   * Error handling:  An error code (independent of scan state) is set
   * and returned to LQH.  No more result rows are returned but normal
   * protocol is still followed until scan close.
   */
  struct ScanOp {
    static constexpr Uint32 TYPE_ID = RT_DBTUX_SCAN_OPERATION;
    Uint32 m_magic;

    ~ScanOp()
    {
    }

    enum {
      Undef = 0,
      First = 1,                // before first entry
      Current = 2,              // at some entry
      Found = 3,                // return current as next scan result
      Blocked = 4,              // found and waiting for ACC lock
      Locked = 5,               // found and locked or no lock needed
      Next = 6,                 // looking for next entry
      Last = 7,                 // after last entry
      Aborting = 8
    };
    Uint32 m_errorCode;
    Uint32 m_lockwait;
    Uint32 m_state;
    Uint32 m_userPtr;           // scanptr.i in LQH
    Uint32 m_userRef;
    Uint32 m_tableId;
    Uint32 m_indexId;
    Uint32 m_fragId;
    Uint32 m_fragPtrI;
    Uint32 m_transId1;
    Uint32 m_transId2;
    Uint32 m_savePointId;
    // lock waited for or obtained and not yet passed to LQH
    Uint32 m_accLockOp;
    // locks obtained and passed to LQH but not yet returned by LQH
    ScanLock_fifo::Head m_accLockOps;
    Uint8 m_readCommitted;      // no locking
    Uint8 m_lockMode;
    Uint8 m_descending;
    ScanBound m_scanBound[2];
    bool m_is_linked_scan;
    TreePos m_scanPos;          // position
    TupLoc  m_scanLinkedPos;    // Location of next scan entry now
    TreeEnt m_scanEnt;          // latest entry found
    Uint32 m_nodeScanPtrI;      // next scan at node (single-linked)
    Uint32 m_nodeScanInstance;  // next scan instance at node
    Uint32 m_statOpPtrI;        // RNIL unless this is a statistics scan
    union {
    Uint32 nextPool;
    Uint32 nextList;
    };
    Uint32 prevList;
    ScanOp();
  };
  static constexpr Uint32 DBTUX_SCAN_OPERATION_TRANSIENT_POOL_INDEX = 0;
  typedef Ptr<ScanOp> ScanOpPtr;
  typedef TransientPool<ScanOp> ScanOp_pool;
  typedef DLList<ScanOp_pool> ScanOp_list;
  ScanOp_pool c_scanOpPool;

  // indexes and fragments

  /*
   * Ordered index.  Top level data structure.  The primary table (table
   * being indexed) lives in TUP.
   */
  struct Index;
  friend struct Index;
  struct Index {
    enum State {
      NotDefined = 0,
      Defining = 1,
      Building = 3,             // triggers activated, building
      Online = 2,               // triggers activated and build done
      Dropping = 9
    };
    State m_state;
    DictTabInfo::TableType m_tableType;
    Uint32 m_tableId;
    Uint16 unused;
    Uint16 m_numFrags;
    Uint32 m_fragId[MaxIndexFragments];
    Uint32 m_fragPtrI[MaxIndexFragments];
    Uint32 m_descPage;          // descriptor page
    Uint16 m_descOff;           // offset within the page
    Uint16 m_numAttrs;
    Uint16 m_prefAttrs;         // attributes in min prefix
    Uint16 m_prefBytes;         // max bytes in min prefix
    KeySpec m_keySpec;
    Uint32 m_statFragPtrI;      // fragment to monitor if not RNIL
    Uint32 m_statLoadTime;      // load time of index stats
    union {
    bool m_storeNullKey;
    Uint32 nextPool;
    };
    Index();
  };
  typedef Ptr<Index> IndexPtr;
  typedef ArrayPool<Index> Index_pool;

public:
  Index_pool c_indexPool;
  RSS_AP_SNAPSHOT(c_indexPool);
private:

  /*
   * Fragment of an index, as known to DIH/TC.  Represents the two
   * duplicate fragments known to LQH/ACC/TUP.  Includes tree header.
   * There are no maintenance operation records yet.
   */
  struct Frag;
  friend struct Frag;
  struct Frag {
    Uint32 m_tableId;           // copy from index level
    Uint32 m_indexId;
    Uint16 unused;
    Uint16 m_fragId;
    TreeHead m_tree;
    TupLoc m_freeLoc;           // one free node for next op
    Uint32 m_tupIndexFragPtrI;
    Uint32 m_tupTableFragPtrI;
    Uint32 m_accTableFragPtrI;
    Uint64 m_entryCount;        // current entries
    Uint64 m_entryBytes;        // sum of index key sizes
    Uint64 m_entryOps;          // ops since last index stats update
    union {
    Uint32 nextPool;
    };
    Frag(ScanOp_pool& scanOpPool);
  };
  typedef Ptr<Frag> FragPtr;
  typedef ArrayPool<Frag> Frag_pool;

public:
  Frag_pool c_fragPool;
  RSS_AP_SNAPSHOT(c_fragPool);
private:
  /*
   * Fragment metadata operation.
   */
  struct FragOp {
    Uint32 m_userPtr;
    Uint32 m_userRef;
    Uint32 m_indexId;
    Uint32 m_fragId;
    Uint32 m_fragPtrI;
    Uint32 m_fragNo;            // fragment number starting at zero
    Uint32 m_numAttrsRecvd;
    union {
    Uint32 nextPool;
    };
    FragOp();
  };
  typedef Ptr<FragOp> FragOpPtr;
  typedef ArrayPool<FragOp> FragOp_pool;

  FragOp_pool c_fragOpPool;
  RSS_AP_SNAPSHOT(c_fragOpPool);

  // node handles

  /*
   * A node handle is a reference to a tree node in TUP.  It is used to
   * operate on the node.  Node handles are allocated on the stack.
   */
  struct NodeHandle;
  friend struct NodeHandle;
  struct NodeHandle {
    Frag& m_frag;               // fragment using the node
    TupLoc m_loc;               // physical node address
    TreeNode* m_node;           // pointer to node storage
    NodeHandle(Frag& frag);
    NodeHandle(const NodeHandle& node);
    NodeHandle& operator=(const NodeHandle& node);
    // check if unassigned
    bool isNull();
    // getters
    TupLoc getLink(unsigned i);
    unsigned getChilds();       // cannot spell
    unsigned getSide();
    unsigned getOccup();
    int getBalance();
    void getNodeScan(Uint32 & scanPtrI, Uint32 &scanInstance);
    bool isNodeScanList();
    // setters
    void setLink(unsigned i, TupLoc loc);
    void setSide(unsigned i);
    void setOccup(unsigned n);
    void setBalance(int b);
    void setNodeScan(Uint32 scanPtrI, Uint32 scanInstance);
    // access other parts of the node
    Uint32* getPref();
    TreeEnt getEnt(unsigned pos);
    // for ndbrequire and ndbassert
    [[noreturn]] void progError(int line,
                                int cause,
                                const char* file,
                                const char* check);
  };

  // stats scan
  struct StatOp;
  friend struct StatOp;
  struct StatOp {
    // the scan
    Uint32 m_scanOpPtrI;
    // parameters
    Uint32 m_saveSize;
    Uint32 m_saveScale;
    Uint32 m_batchSize;
    Uint32 m_estBytes;
   // counters
   Uint32 m_rowCount;
   Uint32 m_batchCurr;
   bool m_haveSample;
   Uint32 m_sampleCount;
   Uint32 m_keyBytes;
   bool m_keyChange;
   bool m_usePrev;
   // metadata
   enum { MaxKeyCount = MAX_INDEX_STAT_KEY_COUNT };
   enum { MaxKeySize = MAX_INDEX_STAT_KEY_SIZE };
   enum { MaxValueCount = MAX_INDEX_STAT_VALUE_COUNT };
   enum { MaxValueSize = MAX_INDEX_STAT_VALUE_SIZE };
   Uint32 m_keyCount;
   Uint32 m_valueCount;
   // pack
   const KeySpec& m_keySpec;
   NdbPack::Spec m_valueSpec;
   NdbPack::Type m_valueSpecBuf[MaxValueCount];
   // data previous current result
   KeyData m_keyData1;
   KeyData m_keyData2;
   KeyData m_keyData;
   NdbPack::Data m_valueData;
   // buffers with one word for length bytes
   Uint32 m_keyDataBuf1[1 + MaxKeySize];
   Uint32 m_keyDataBuf2[1 + MaxKeySize];
   Uint32 m_keyDataBuf[1 + MaxKeySize];
   Uint32 m_valueDataBuf[1 + MaxValueCount];
   // value collection
   struct Value {
     Uint32 m_rir;
     Uint32 m_unq[MaxKeyCount];
     Value();
   };
   Value m_value1;
   Value m_value2;
   union {
   Uint32 nextPool;
   };
   StatOp(const Index&);
  };
  typedef Ptr<StatOp> StatOpPtr;
  typedef ArrayPool<StatOp> StatOp_pool;

  StatOp_pool c_statOpPool;
  RSS_AP_SNAPSHOT(c_statOpPool);

  // stats monitor (shared by req data and continueB loop)
  struct StatMon;
  friend struct StatMon;
  struct StatMon {
    IndexStatImplReq m_req;
    Uint32 m_requestType;
    // continueB loop
    Uint32 m_loopIndexId;
    Uint32 m_loopDelay;
    StatMon();
  };
  StatMon c_statMon;

  // methods

  /*
   * DbtuxGen.cpp
   */
  void execCONTINUEB(Signal* signal);
  void execSTTOR(Signal* signal);
  void execREAD_CONFIG_REQ(Signal* signal);
  void execNODE_STATE_REP(Signal* signal);

  // utils
  void readKeyAttrs(TuxCtx&,
                    const Frag& frag,
                    TreeEnt ent,
                    KeyData& keyData,
                    Uint32 count);
  void readKeyAttrs(TuxCtx&,
                    const Frag& frag,
                    TreeEnt ent,
                    Uint32 count,
                    Uint32 *outputBuffer);
  void readTablePk(TreeEnt ent, Uint32* pkData, unsigned& pkSize);
  void unpackBound(Uint32* const outputBuffer,
                   const ScanBound& bound,
                   KeyBoundC& searchBound);
  void findFrag(EmulatedJamBuffer* jamBuf, const Index& index, 
                Uint32 fragId, FragPtr& fragPtr);

  /*
   * DbtuxMeta.cpp
   */
  void execCREATE_TAB_REQ(Signal*);
  void execTUXFRAGREQ(Signal* signal);
  void execTUX_ADD_ATTRREQ(Signal* signal);
  void execALTER_INDX_IMPL_REQ(Signal* signal);
  void execDROP_TAB_REQ(Signal* signal);
  void execDROP_FRAG_REQ(Signal* signal);
  bool allocDescEnt(IndexPtr indexPtr);
  void freeDescEnt(IndexPtr indexPtr);
  void abortAddFragOp(Signal* signal);
  void dropIndex(Signal* signal, IndexPtr indexPtr, Uint32 senderRef, Uint32 senderData);

public:
  /*
   * DbtuxMaint.cpp
   */
  void execTUX_MAINT_REQ(Signal* signal);

private:
  /*
   * DbtuxNode.cpp
   */
  int allocNode(TuxCtx&, NodeHandle& node);
  void freeNode(NodeHandle& node);
  void selectNode(TuxCtx&, NodeHandle& node, TupLoc loc);
  void insertNode(TuxCtx&, NodeHandle& node);
  void deleteNode(NodeHandle& node);
  void freePreallocatedNode(Frag& frag);
  void setNodePref(struct TuxCtx &, NodeHandle& node);
  // node operations
  void nodePushUp(TuxCtx&,
                  NodeHandle& node,
                  unsigned pos,
                  const TreeEnt& ent,
                  Uint32 scanList,
                  Uint32 scanInstance);
  void nodePushUpScans(NodeHandle& node, unsigned pos);
  void nodePopDown(TuxCtx&,
                   NodeHandle& node,
                   unsigned pos,
                   TreeEnt& en,
                   Uint32* scanList,
                   Uint32* scanInstance);
  void nodePopDownScans(NodeHandle& node, unsigned pos);
  void nodePushDown(TuxCtx&,
                    NodeHandle& node,
                    unsigned pos,
                    TreeEnt& ent,
                    Uint32& scanList,
                    Uint32& scanInstance);
  void nodePushDownScans(NodeHandle& node, unsigned pos);
  void nodePopUp(TuxCtx&, NodeHandle& node,
                 unsigned pos,
                 TreeEnt& ent,
                 Uint32 scanList,
                 Uint32 scanInstance);
  void nodePopUpScans(NodeHandle& node, unsigned pos);
  void nodeSlide(TuxCtx&,
                 NodeHandle& dstNode,
                 NodeHandle& srcNode,
                 unsigned cnt,
                 unsigned i);
  // scans linked to node
  void addScanList(NodeHandle& node,
                   unsigned pos,
                   Uint32 scanList,
                   Uint32 scanInstance);
  void removeScanList(NodeHandle& node,
                      unsigned pos,
                      Uint32& scanList,
                      Uint32& scanInstance);
  void moveScanList(NodeHandle& node, unsigned pos);
  void linkScan(NodeHandle& node, ScanOpPtr scanPtr, Uint32 scanInstance);
  void unlinkScan(NodeHandle& node, ScanOpPtr scanPtr, Uint32 scanInstance);
  bool islinkScan(NodeHandle& node, ScanOpPtr scanPtr, Uint32 scanInstance);
  void relinkScan(ScanOp&, Frag&, bool need_lock = true, Uint32 line = 0);

  /*
   * DbtuxTree.cpp
   */
  // add entry
  void treeAdd(TuxCtx&, Frag& frag, TreePos treePos, TreeEnt ent);
  void treeAddFull(TuxCtx&, Frag& frag, NodeHandle lubNode, unsigned pos, TreeEnt ent);
  void treeAddNode(TuxCtx&, Frag& frag, NodeHandle lubNode, unsigned pos, TreeEnt ent, NodeHandle parentNode, unsigned i);
  void treeAddRebalance(TuxCtx&, Frag& frag, NodeHandle node, unsigned i);
  // remove entry
  void treeRemove(Frag& frag, TreePos treePos);
  void treeRemoveInner(Frag& frag, NodeHandle lubNode, unsigned pos);
  void treeRemoveSemi(Frag& frag, NodeHandle node, unsigned i);
  void treeRemoveLeaf(Frag& frag, NodeHandle node);
  void treeRemoveNode(Frag& frag, NodeHandle node);
  void treeRemoveRebalance(Frag& frag, NodeHandle node, unsigned i);
  // rotate
  void treeRotateSingle(TuxCtx&, Frag& frag, NodeHandle& node, unsigned i);
  void treeRotateDouble(TuxCtx&, Frag& frag, NodeHandle& node, unsigned i);

  /*
   * DbtuxScan.cpp
   */
  void execACC_SCANREQ(Signal* signal);
  void execNEXT_SCANREQ(Signal* signal);
  void execACC_CHECK_SCAN(Signal* signal);
  void execACCKEYCONF(Signal* signal);
  void execACCKEYREF(Signal* signal);
  void execACC_ABORTCONF(Signal* signal);
  void scanFirst(ScanOpPtr scanPtr, Frag& frag, const Index& index);
  void continue_scan(Signal *signal, ScanOpPtr scanPtr, Frag& frag, bool);
  void scanFind(ScanOpPtr scanPtr, Frag& frag);
  Uint32 scanNext(ScanOpPtr scanPtr, bool fromMaintReq, Frag& frag);
  bool scanCheck(ScanOp& scan, TreeEnt ent);
  bool scanVisible(ScanOp& scan, TreeEnt ent);
  void scanClose(Signal* signal, ScanOpPtr scanPtr);
  void abortAccLockOps(Signal* signal, ScanOpPtr scanPtr);
  void addAccLockOp(ScanOpPtr scanPtr, Uint32 accLockOp);
  void removeAccLockOp(ScanOpPtr scanPtr, Uint32 accLockOp);
  void releaseScanOp(ScanOpPtr& scanPtr);

  /*
   * DbtuxSearch.cpp
   */
  void findNodeToUpdate(TuxCtx&,
                        Frag& frag,
                        const KeyBoundArray& searchBound,
                        TreeEnt searchEnt,
                        NodeHandle& currNode);
  bool findPosToAdd(TuxCtx&,
                    Frag& frag,
                    const KeyBoundArray& searchBound,
                    TreeEnt searchEnt,
                    NodeHandle& currNode,
                    TreePos& treePos);
  bool findPosToRemove(TuxCtx&,
                       TreeEnt searchEnt,
                       NodeHandle& currNode,
                       TreePos& treePos);
  bool searchToAdd(TuxCtx&,
                   Frag& frag,
                   const KeyBoundArray& searchBound,
                   TreeEnt searchEnt,
                   TreePos& treePos);
  bool searchToRemove(TuxCtx&,
                      Frag& frag,
                      const KeyBoundArray& searchBound,
                      TreeEnt searchEnt,
                      TreePos& treePos);
  void findNodeToScan(Frag& frag,
                      unsigned dir,
                      const KeyBoundArray& searchBound,
                      NodeHandle& currNode);
  void findPosToScan(Frag& frag,
                     unsigned idir,
                     const KeyBoundArray& searchBound,
                     NodeHandle& currNode,
                     Uint32* pos);
  void searchToScan(Frag& frag,
                    unsigned idir,
                    const KeyBoundArray& searchBound,
                    TreePos& treePos);

  /**
   * Prepare methods
   * These methods are setting up variables that are precomputed to avoid having
   * to compute those every time we need them.
   */
  void prepare_scan_bounds(const ScanOp *scanPtrP,
                           const Index *indexPtrP,
                           Dbtux *tux_block);
  void prepare_move_scan_ctx(ScanOpPtr scanPtr, Dbtux *tux_block);

  int cmpSearchKey(TuxCtx&, const KeyDataC& searchKey, const KeyDataC& entryKey, Uint32 cnt);
  int cmpSearchBound(TuxCtx&, const KeyBoundC& searchBound, const KeyDataC& entryKey, Uint32 cnt);

  /*
   * DbtuxStat.cpp
   */
  // one-round-trip tree-dive records in range
  void statRecordsInRange(ScanOpPtr scanPtr, Uint32* out, Uint32 out_words);
  Uint32 getEntriesBeforeOrAfter(Frag& frag, TreePos pos, unsigned idir);
  unsigned getPathToNode(NodeHandle node, Uint16* path);
  // stats scan
  int statScanInit(StatOpPtr, const Uint32* data, Uint32 len, Uint32* usedLen);
  int statScanAddRow(StatOpPtr, TreeEnt ent);
  void statScanReadKey(StatOpPtr, Uint32* out, Uint32 out_words);
  void statScanReadValue(StatOpPtr, Uint32* out, Uint32 out_words);
  void execINDEX_STAT_REP(Signal*); // from TRIX
  // stats monitor request
  void execINDEX_STAT_IMPL_REQ(Signal*);
  void statMonStart(Signal*, StatMon&);
  void statMonStop(Signal*, StatMon&);
  void statMonConf(Signal*, StatMon&);
  // stats monitor continueB loop
  void statMonSendContinueB(Signal*);
  void statMonExecContinueB(Signal*);
  void statMonCheck(Signal*, StatMon&);
  void statMonRep(Signal*, StatMon&);

  /*
   * DbtuxDebug.cpp
   */
  void execDUMP_STATE_ORD(Signal* signal);
#ifdef VM_TRACE
  struct PrintPar {
    char m_path[100];           // LR prefix
    unsigned m_side;            // expected side
    TupLoc m_parent;            // expected parent address
    int m_depth;                // returned depth
    unsigned m_occup;           // returned occupancy
    TreeEnt m_minmax[2];        // returned subtree min and max
    bool m_ok;                  // returned status
    PrintPar();
  };
  void printTree(Signal* signal, Frag& frag, NdbOut& out);
  void printNode(struct TuxCtx&, Frag&, NdbOut& out, TupLoc loc, PrintPar& par);
  friend class NdbOut& operator<<(NdbOut&, const TupLoc&);
  friend class NdbOut& operator<<(NdbOut&, const TreeEnt&);
  friend class NdbOut& operator<<(NdbOut&, const TreeNode&);
  friend class NdbOut& operator<<(NdbOut&, const TreeHead&);
  friend class NdbOut& operator<<(NdbOut&, const TreePos&);
  friend class NdbOut& operator<<(NdbOut&, const KeyType&);
  friend class NdbOut& operator<<(NdbOut&, const ScanOp&);
  friend class NdbOut& operator<<(NdbOut&, const Index&);
  friend class NdbOut& operator<<(NdbOut&, const Frag&);
  friend class NdbOut& operator<<(NdbOut&, const FragOp&);
  friend class NdbOut& operator<<(NdbOut&, const NodeHandle&);
  friend class NdbOut& operator<<(NdbOut&, const StatOp&);
  friend class NdbOut& operator<<(NdbOut&, const StatMon&);
  FILE* debugFile;
  NdbOut tuxDebugOut;
  unsigned debugFlags;
  enum {
    DebugMeta = 1,              // log create and drop index
    DebugMaint = 2,             // log maintenance ops
    DebugTree = 4,              // log and check tree after each op
    DebugScan = 8,              // log scans
    DebugLock = 16,             // log ACC locks
    DebugStat = 32              // log stats collection
  };
  static constexpr Uint32 DataFillByte = 0xa2;
  static constexpr Uint32 NodeFillByte = 0xa4;
#endif

  void execDBINFO_SCANREQ(Signal* signal);

  // start up info
  Uint32 c_internalStartPhase;
  Uint32 c_typeOfStart;

  /*
   * Global data set at operation start.  Unpacked from index metadata.
   * Not passed as parameter to methods.  Invalid across timeslices.
   *
   * TODO inline all into index metadata
   */
  struct TuxCtx
  {
    EmulatedJamBuffer * jamBuffer;


    ScanOpPtr scanPtr;
    FragPtr fragPtr;
    IndexPtr indexPtr;
    Uint32 *tupIndexFragPtr;
    Uint32 *tupIndexTablePtr;
    Uint32 *tupRealFragPtr;
    Uint32 *tupRealTablePtr;
    Uint32 attrDataOffset;
    Uint32 tuxFixHeaderSize;

    KeyDataArray searchScanDataArray;
    KeyBoundArray searchScanBoundArray;
    Uint32 *keyAttrs;

    KeyDataArray searchKeyDataArray;
    KeyBoundArray searchKeyBoundArray;

    Uint32 scanBoundCnt;
    Uint32 descending;

    TreeEnt m_current_ent;

    // buffer for scan bound and search key data
    Uint32* c_searchKey;

    // buffer for scan bound and search key data for next key
    Uint32* c_nextKey;

    // buffer for current entry key data
    Uint32* c_entryKey;

    // buffer for xfrm-ed PK and for temporary use
    Uint32* c_dataBuffer;

    // buffer for xfrm-ed PK and for temporary use
    Uint32* c_boundBuffer;

#ifdef VM_TRACE
    char* c_debugBuffer;
#endif
    // function for clearing context
    void reset()
    {
      // jamBuffer left
      scanPtr.i = RNIL;
      scanPtr.p = nullptr;
      fragPtr.i = RNIL;
      fragPtr.p = nullptr;
      indexPtr.i = RNIL;
      indexPtr.p = nullptr;
      tupIndexFragPtr = nullptr;
      tupIndexTablePtr = nullptr;
      tupRealFragPtr = nullptr;
      tupRealTablePtr = nullptr;
      attrDataOffset = 0;
      tuxFixHeaderSize = 0;
      // searchScanDataArray left
      // searchScanBoundArray left
      keyAttrs = nullptr;
      // searchKeyDataArray left
      // searchKeyBoundArray left
      scanBoundCnt = 0;
      descending = 0;
      m_current_ent.m_tupLoc = NullTupLoc;
      m_current_ent.m_tupVersion = 0;
      // c_searchKey left
      // c_nextKey left
      // c_entryKey left
      // c_dataBuffer left
      // c_boundBuffer left
      // c_debugBuffer left
    }
  };

  struct TuxCtx c_ctx; // Global Tux context, for everything build MT-index build

  // index stats
  bool c_indexStatAutoUpdate;
  Uint32 c_indexStatSaveSize;
  Uint32 c_indexStatSaveScale;
  Uint32 c_indexStatTriggerPct;
  Uint32 c_indexStatTriggerScale;
  Uint32 c_indexStatUpdateDelay;

  // inlined utils
  Uint32 getDescSize(const Index& index);
  DescHead& getDescHead(const Index& index);
  KeyType* getKeyTypes(DescHead& descHead);
  const KeyType* getKeyTypes(const DescHead& descHead);
  AttributeHeader* getKeyAttrs(DescHead& descHead);
  const AttributeHeader* getKeyAttrs(const DescHead& descHead);
  //
  void getTupAddr(const Frag& frag, TreeEnt ent, Uint32& lkey1, Uint32& lkey2);
  static unsigned min(unsigned x, unsigned y);
  static unsigned max(unsigned x, unsigned y);

public:
  static Uint32 mt_buildIndexFragment_wrapper(void*);
  void prepare_build_ctx(TuxCtx& ctx, FragPtr fragPtr);
  void prepare_tup_ptrs(TuxCtx& ctx);
  void prepare_all_tup_ptrs(TuxCtx& ctx);
  void relinkScan(Uint32 line);
private:
  Uint32 mt_buildIndexFragment(struct mt_BuildIndxCtx*);

  Signal* c_signal_bug32040;

private:
  bool check_freeScanLock(ScanOp& scan);
  void release_c_free_scan_lock();
  void checkPoolShrinkNeed(Uint32 pool_index,
                           const TransientFastSlotPool& pool);
  void sendPoolShrink(Uint32 pool_index);
  void shrinkTransientPools(Uint32 pool_index);

  static const Uint32 c_transient_pool_count = 3;
  TransientFastSlotPool* c_transient_pools[c_transient_pool_count];
  Bitmask<1> c_transient_pools_shrinking;

  Uint32 get_my_scan_instance();
  Uint32 get_block_from_scan_instance(Uint32);
  Uint32 get_instance_from_scan_instance(Uint32);
public:
  static Uint64 getTransactionMemoryNeed(
    const Uint32 ldm_instance_count,
    const ndb_mgm_configuration_iterator *mgm_cfg,
    const bool use_reserved);
  Uint32 getDBACC()
  {
    return m_acc_block;
  }
  Uint32 getDBLQH()
  {
    return m_lqh_block;
  }
  Uint32 getDBTUX()
  {
    return m_tux_block;
  }
  ScanOp* getScanOpPtrP(Uint32 scanPtrI)
  {
    ScanOpPtr scanPtr;
    scanPtr.i = scanPtrI;
    ndbrequire(c_scanOpPool.getValidPtr(scanPtr));
    return scanPtr.p;
  }
  ScanOp* getScanOpPtrP(Uint32 scanPtrI, Uint32 scanInstance)
  {
    Dbtux *tux_block;
    Uint32 blockNo = get_block_from_scan_instance(scanInstance);
    Uint32 instanceNo = get_instance_from_scan_instance(scanInstance);
    tux_block = (Dbtux*) globalData.getBlock(blockNo, instanceNo);
    return tux_block->getScanOpPtrP(scanPtrI);
  }
};

inline bool Dbtux::check_freeScanLock(ScanOp& scan)
{
  if (unlikely((! scan.m_readCommitted) &&
                c_freeScanLock == RNIL))
  {
    ScanLockPtr allocPtr;
    if (c_scanLockPool.seize(allocPtr))
    {
      jam();
      c_freeScanLock = allocPtr.i;
    }
    else
    {
      return true;
    }
  }
  return false;
}

inline void
Dbtux::release_c_free_scan_lock()
{
  if (c_freeScanLock != RNIL)
  {
    jam();
    ScanLockPtr releasePtr;
    releasePtr.i = c_freeScanLock;
    ndbrequire(c_scanLockPool.getValidPtr(releasePtr));
    c_scanLockPool.release(releasePtr);
    c_freeScanLock = RNIL;
    checkPoolShrinkNeed(DBTUX_SCAN_LOCK_TRANSIENT_POOL_INDEX,
                        c_scanLockPool);
  }
}

inline void Dbtux::checkPoolShrinkNeed(const Uint32 pool_index,
                                       const TransientFastSlotPool& pool)
{
#if defined(VM_TRACE) || defined(ERROR_INSERT)
  ndbrequire(pool_index < c_transient_pool_count);
  ndbrequire(c_transient_pools[pool_index] == &pool);
#endif
  if (pool.may_shrink())
  {
    sendPoolShrink(pool_index);
  }
}

// Dbtux::TupLoc

inline
Dbtux::TupLoc::TupLoc() :
  m_pageId1(RNIL >> 16),
  m_pageId2(RNIL & 0xFFFF),
  m_pageOffset(0)
{
}

inline
Dbtux::TupLoc::TupLoc(Uint32 pageId, Uint16 pageOffset) :
  m_pageId1(pageId >> 16),
  m_pageId2(pageId & 0xFFFF),
  m_pageOffset(pageOffset)
{
}

inline Uint32
Dbtux::TupLoc::getPageId() const
{
  return (m_pageId1 << 16) | m_pageId2;
}

inline void
Dbtux::TupLoc::setPageId(Uint32 pageId)
{
  m_pageId1 = (pageId >> 16);
  m_pageId2 = (pageId & 0xFFFF);
}

inline Uint32
Dbtux::TupLoc::getPageOffset() const
{
  return (Uint32)m_pageOffset;
}

inline void
Dbtux::TupLoc::setPageOffset(Uint32 pageOffset)
{
  m_pageOffset = (Uint16)pageOffset;
}

inline bool
Dbtux::TupLoc::operator==(const TupLoc& loc) const
{
  return
    m_pageId1 == loc.m_pageId1 &&
    m_pageId2 == loc.m_pageId2 &&
    m_pageOffset == loc.m_pageOffset;
}

inline bool
Dbtux::TupLoc::operator!=(const TupLoc& loc) const
{
  return ! (*this == loc);
}

// Dbtux::TreeEnt

inline
Dbtux::TreeEnt::TreeEnt() :
  m_tupLoc(),
  m_tupVersion(0)
{
}

inline bool
Dbtux::TreeEnt::eqtuple(const TreeEnt ent) const
{
  return
    m_tupLoc == ent.m_tupLoc;
}

inline bool
Dbtux::TreeEnt::eq(const TreeEnt ent) const
{
  return
    m_tupLoc == ent.m_tupLoc &&
    m_tupVersion == ent.m_tupVersion;
}

inline int
Dbtux::TreeEnt::cmp(const TreeEnt ent) const
{
  if (m_tupLoc.getPageId() < ent.m_tupLoc.getPageId())
    return -1;
  if (m_tupLoc.getPageId() > ent.m_tupLoc.getPageId())
    return +1;
  if (m_tupLoc.getPageOffset() < ent.m_tupLoc.getPageOffset())
    return -1;
  if (m_tupLoc.getPageOffset() > ent.m_tupLoc.getPageOffset())
    return +1;
  /*
   * Guess if one tuple version has wrapped around.  This is well
   * defined ordering on existing versions since versions are assigned
   * consecutively and different versions exists only on uncommitted
   * tuple.  Assuming max 2**14 uncommitted ops on same tuple.
   */
  const unsigned version_wrap_limit = (1 << (ZTUP_VERSION_BITS - 1));
  if (m_tupVersion < ent.m_tupVersion) {
    if (unsigned(ent.m_tupVersion - m_tupVersion) < version_wrap_limit)
      return -1;
    else
      return +1;
  }
  if (m_tupVersion > ent.m_tupVersion) {
    if (unsigned(m_tupVersion - ent.m_tupVersion) < version_wrap_limit)
      return +1;
    else
      return -1;
  }
  return 0;
}

// Dbtux::TreeNode

inline
Dbtux::TreeNode::TreeNode() :
  m_side(2),
  m_balance(0 + 1),
  pad1(0),
  m_occup(0),
  m_nodeScanPtrI(RNIL),
  m_nodeScanInstance(0)
{
  m_link[0] = NullTupLoc;
  m_link[1] = NullTupLoc;
  m_link[2] = NullTupLoc;
}

// Dbtux::TreeHead

inline
Dbtux::TreeHead::TreeHead() :
  m_nodeSize(0),
  m_prefSize(0),
  m_minOccup(0),
  m_maxOccup(0),
  m_root()
{
}

inline Uint32*
Dbtux::TreeHead::getPref(TreeNode* node) const
{
  Uint32* ptr = (Uint32*)node + NodeHeadSize;
  return ptr;
}

inline Dbtux::TreeEnt*
Dbtux::TreeHead::getEntList(TreeNode* node) const
{
  Uint32* ptr = (Uint32*)node + NodeHeadSize + m_prefSize;
  return (TreeEnt*)ptr;
}

// Dbtux::TreePos

inline
Dbtux::TreePos::TreePos() :
  m_loc(),
  m_pos(Uint32(~0)),
  m_dir(Uint32(~0))
{
}

// Dbtux::DescPage

inline
Dbtux::DescPage::DescPage() :
  m_nextPage(RNIL),
  m_numFree(ZNIL)
{
  for (unsigned i = 0; i < DescPageSize; i++) {
#ifdef VM_TRACE
    m_data[i] = 0x13571357;
#else
    m_data[i] = 0;
#endif
  }
}

// Dbtux::ScanBound

inline
Dbtux::ScanBound::ScanBound() :
  m_head(),
  m_cnt(0),
  m_side(0)
{
}

inline
Dbtux::ScanOp::ScanOp() :
  m_magic(Magic::make(ScanOp::TYPE_ID)),
  m_errorCode(0),
  m_lockwait(false),
  m_fragPtrI(RNIL),
  m_accLockOp(RNIL),
  m_accLockOps(),
  m_scanBound(),
  m_scanPos(),
  m_scanLinkedPos(),
  m_scanEnt(),
  m_nodeScanPtrI(RNIL),
  m_nodeScanInstance(0),
  m_statOpPtrI(RNIL)
{
}

// Dbtux::Index

inline
Dbtux::Index::Index() :
  m_state(NotDefined),
  m_tableType(DictTabInfo::UndefTableType),
  m_tableId(RNIL),
  m_numFrags(0),
  m_descPage(RNIL),
  m_descOff(0),
  m_numAttrs(0),
  m_prefAttrs(0),
  m_prefBytes(0),
  m_keySpec(),
  m_statFragPtrI(RNIL),
  m_statLoadTime(0),
  m_storeNullKey(false)
{
  for (unsigned i = 0; i < MaxIndexFragments; i++) {
    m_fragId[i] = ZNIL;
    m_fragPtrI[i] = RNIL;
  };
}

// Dbtux::Frag

inline
Dbtux::Frag::Frag(ScanOp_pool& scanOpPool) :
  m_tableId(RNIL),
  m_indexId(RNIL),
  m_fragId(ZNIL),
  m_tree(),
  m_freeLoc(),
  m_tupIndexFragPtrI(RNIL),
  m_tupTableFragPtrI(RNIL),
  m_accTableFragPtrI(RNIL),
  m_entryCount(0),
  m_entryBytes(0),
  m_entryOps(0)
{
}

// Dbtux::FragOp

inline
Dbtux::FragOp::FragOp() :
  m_userPtr(RNIL),
  m_userRef(RNIL),
  m_indexId(RNIL),
  m_fragId(ZNIL),
  m_fragPtrI(RNIL),
  m_fragNo(ZNIL),
  m_numAttrsRecvd(ZNIL)
{
}

// Dbtux::NodeHandle

inline
Dbtux::NodeHandle::NodeHandle(Frag& frag) :
  m_frag(frag),
  m_loc(),
  m_node(0)
{
}

inline
Dbtux::NodeHandle::NodeHandle(const NodeHandle& node) :
  m_frag(node.m_frag),
  m_loc(node.m_loc),
  m_node(node.m_node)
{
}

inline Dbtux::NodeHandle&
Dbtux::NodeHandle::operator=(const NodeHandle& node)
{
  ndbassert(&m_frag == &node.m_frag);
  m_loc = node.m_loc;
  m_node = node.m_node;
  return *this;
}

inline bool
Dbtux::NodeHandle::isNull()
{
  return m_node == 0;
}

inline Dbtux::TupLoc
Dbtux::NodeHandle::getLink(unsigned i)
{
  ndbrequire(i <= 2);
  return m_node->m_link[i];
}

inline unsigned
Dbtux::NodeHandle::getChilds()
{
  return (m_node->m_link[0] != NullTupLoc) + (m_node->m_link[1] != NullTupLoc);
}

inline unsigned
Dbtux::NodeHandle::getSide()
{
  return m_node->m_side;
}

inline unsigned
Dbtux::NodeHandle::getOccup()
{
  return m_node->m_occup;
}

inline int
Dbtux::NodeHandle::getBalance()
{
  return (int)m_node->m_balance - 1;
}

inline bool
Dbtux::NodeHandle::isNodeScanList()
{
  return m_node->m_nodeScanPtrI != RNIL;
}

inline void
Dbtux::NodeHandle::getNodeScan(Uint32 &scanPtrI, Uint32 &instance)
{
  scanPtrI = m_node->m_nodeScanPtrI;
  instance = m_node->m_nodeScanInstance;
}

inline void
Dbtux::NodeHandle::setLink(unsigned i, TupLoc loc)
{
  if (likely(i <= 2))
  {
    m_node->m_link[i] = loc;
  }
  else
  {
    ndbabort();
  }
}

inline Uint32
Dbtux::get_instance_from_scan_instance(Uint32 scanInstance)
{
  return (scanInstance >> 16);
}

inline Uint32
Dbtux::get_block_from_scan_instance(Uint32 scanInstance)
{
  return (scanInstance & 0xFFFF);
}

inline Uint32
Dbtux::get_my_scan_instance()
{
  if (m_is_query_block)
  {
    return DBQTUX + (instance() << 16);
  }
  else
  {
    return DBTUX + (instance() << 16);
  }
}

inline void
Dbtux::NodeHandle::setSide(unsigned i)
{
  if (likely(i <= 2))
  {
    m_node->m_side = i;
  }
  else
  {
    ndbabort();
  }
}

inline void
Dbtux::NodeHandle::setOccup(unsigned n)
{
  TreeHead& tree = m_frag.m_tree;
  ndbrequire(n <= tree.m_maxOccup);
  m_node->m_occup = n;
}

inline void
Dbtux::NodeHandle::setBalance(int b)
{
  ndbrequire(abs(b) <= 1);
  m_node->m_balance = (unsigned)(b + 1);
}

inline void
Dbtux::NodeHandle::setNodeScan(Uint32 scanPtrI,
                               Uint32 instance)
{
  m_node->m_nodeScanPtrI = scanPtrI;
  m_node->m_nodeScanInstance = instance;
}

inline Uint32*
Dbtux::NodeHandle::getPref()
{
  TreeHead& tree = m_frag.m_tree;
  return tree.getPref(m_node);
}

inline Dbtux::TreeEnt
Dbtux::NodeHandle::getEnt(unsigned pos)
{
  TreeHead& tree = m_frag.m_tree;
  TreeEnt* entList = tree.getEntList(m_node);
  const unsigned occup = m_node->m_occup;
  ndbrequire(pos < occup);
  return entList[pos];
}

// stats

inline
Dbtux::StatOp::Value::Value()
{
  m_rir = 0;
  Uint32 i;
  for (i = 0; i < MaxKeyCount; i++)
    m_unq[i] = 0;
}

inline
Dbtux::StatOp::StatOp(const Index& index) :
  m_scanOpPtrI(RNIL),
  m_saveSize(0),
  m_saveScale(0),
  m_batchSize(0),
  m_estBytes(0),
  m_rowCount(0),
  m_batchCurr(0),
  m_haveSample(false),
  m_sampleCount(0),
  m_keyBytes(0),
  m_keyChange(false),
  m_usePrev(false),
  m_keyCount(0),
  m_valueCount(0),
  m_keySpec(index.m_keySpec),
  m_keyData1(m_keySpec, false, 2),
  m_keyData2(m_keySpec, false, 2),
  m_keyData(m_keySpec, false, 2),
  m_valueData(m_valueSpec, false, 2),
  m_value1(),
  m_value2()
{
  m_valueSpec.set_buf(m_valueSpecBuf, MaxValueCount);
  m_keyData1.set_buf(m_keyDataBuf1, sizeof(m_keyDataBuf1));
  m_keyData2.set_buf(m_keyDataBuf2, sizeof(m_keyDataBuf2));
  m_keyData.set_buf(m_keyDataBuf, sizeof(m_keyDataBuf));
  m_valueData.set_buf(m_valueDataBuf, sizeof(m_valueDataBuf));
}

// Dbtux::StatMon

inline
Dbtux::StatMon::StatMon() :
  m_requestType(0),
  m_loopIndexId(0),
  m_loopDelay(1000)
{
  memset(&m_req, 0, sizeof(m_req));
}

// parameters for methods

#ifdef VM_TRACE
inline
Dbtux::PrintPar::PrintPar() :
  // caller fills in
  m_path(),
  m_side(255),
  m_parent(),
  // default return values
  m_depth(0),
  m_occup(0),
  m_ok(true)
{
}
#endif

// utils

inline Uint32
Dbtux::getDescSize(const Index& index)
{
  return
    DescHeadSize +
    index.m_numAttrs * KeyTypeSize +
    index.m_numAttrs * AttributeHeaderSize;
}

inline Dbtux::DescHead&
Dbtux::getDescHead(const Index& index)
{
  DescPagePtr pagePtr;
  pagePtr.i = index.m_descPage;
  c_descPagePool.getPtr(pagePtr);
  ndbrequire(index.m_descOff < DescPageSize);
  Uint32* ptr = &pagePtr.p->m_data[index.m_descOff];
  DescHead* descHead = reinterpret_cast<DescHead*>(ptr);
  ndbrequire(descHead->m_magic == DescHead::Magic);
  return *descHead;
}

inline Dbtux::KeyType*
Dbtux::getKeyTypes(DescHead& descHead)
{
  Uint32* ptr = reinterpret_cast<Uint32*>(&descHead);
  ptr += DescHeadSize;
  return reinterpret_cast<KeyType*>(ptr);
}

inline const Dbtux::KeyType*
Dbtux::getKeyTypes(const DescHead& descHead)
{
  const Uint32* ptr = reinterpret_cast<const Uint32*>(&descHead);
  ptr += DescHeadSize;
  return reinterpret_cast<const KeyType*>(ptr);
}

inline AttributeHeader*
Dbtux::getKeyAttrs(DescHead& descHead)
{
  Uint32* ptr = reinterpret_cast<Uint32*>(&descHead);
  ptr += DescHeadSize;
  ptr += descHead.m_numAttrs * KeyTypeSize;
  return reinterpret_cast<AttributeHeader*>(ptr);
}

inline const AttributeHeader*
Dbtux::getKeyAttrs(const DescHead& descHead)
{
  const Uint32* ptr = reinterpret_cast<const Uint32*>(&descHead);
  ptr += DescHeadSize;
  ptr += descHead.m_numAttrs * KeyTypeSize;
  return reinterpret_cast<const AttributeHeader*>(ptr);
}

inline
void
Dbtux::getTupAddr(const Frag& frag, TreeEnt ent, Uint32& lkey1, Uint32& lkey2)
{
  const Uint32 tableFragPtrI = frag.m_tupTableFragPtrI;
  const TupLoc tupLoc = ent.m_tupLoc;
  c_tup->tuxGetTupAddr(tableFragPtrI, tupLoc.getPageId(),tupLoc.getPageOffset(),
                       lkey1, lkey2);
  jamEntryDebug();
}

inline unsigned
Dbtux::min(unsigned x, unsigned y)
{
  return x < y ? x : y;
}

inline unsigned
Dbtux::max(unsigned x, unsigned y)
{
  return x > y ? x : y;
}

/**
 * Can be called from MT-build of ordered indexes,
 * but it doesn't make use of the MT-context other
 * than for debug printouts.
 */
inline int
Dbtux::cmpSearchKey(TuxCtx& ctx,
                    const KeyDataC& searchKey,
                    const KeyDataC& entryKey,
                    Uint32 cnt)
{
  // compare cnt attributes from each
  Uint32 num_eq;
  int ret = searchKey.cmp(entryKey, cnt, num_eq);
#ifdef VM_TRACE
  if (debugFlags & DebugMaint) {
    tuxDebugOut << "cmpSearchKey: ret:" << ret;
    tuxDebugOut << " search:" << searchKey.print(ctx.c_debugBuffer, DebugBufferBytes);
    tuxDebugOut << " entry:" << entryKey.print(ctx.c_debugBuffer, DebugBufferBytes);
    tuxDebugOut << endl;
  }
#endif
  return ret;
}

inline int
Dbtux::cmpSearchBound(TuxCtx& ctx, const KeyBoundC& searchBound, const KeyDataC& entryKey, Uint32 cnt)
{
  // compare cnt attributes from each
  Uint32 num_eq;
  int ret = searchBound.cmp(entryKey, cnt, num_eq);
#ifdef VM_TRACE
  if (debugFlags & DebugScan) {
    tuxDebugOut << "cmpSearchBound: res:" << ret;
    tuxDebugOut << " search:" << searchBound.print(ctx.c_debugBuffer, DebugBufferBytes);
    tuxDebugOut << " entry:" << entryKey.print(ctx.c_debugBuffer, DebugBufferBytes);
    tuxDebugOut << endl;
  }
#endif
  return ret;
}

inline
void
Dbtux::prepare_all_tup_ptrs(TuxCtx& ctx)
{
  c_tup->get_all_tup_ptrs(ctx.fragPtr.p->m_tupIndexFragPtrI,
                          ctx.fragPtr.p->m_tupTableFragPtrI,
                          &ctx.tupIndexFragPtr,
                          &ctx.tupIndexTablePtr,
                          &ctx.tupRealFragPtr,
                          &ctx.tupRealTablePtr,
                          ctx.attrDataOffset,
                          ctx.tuxFixHeaderSize);
}

inline
void
Dbtux::relinkScan(Uint32 line)
{
  ndbrequire(c_ctx.scanPtr.p != nullptr);
  ScanOp& scan = *c_ctx.scanPtr.p;
  Frag& frag = *c_ctx.fragPtr.p;
  relinkScan(scan, frag, true, line);
}
#undef JAM_FILE_ID

#endif
