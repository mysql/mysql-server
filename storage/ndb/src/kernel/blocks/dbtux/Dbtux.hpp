/*
   Copyright (c) 2003, 2018, Oracle and/or its affiliates. All rights reserved.

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
  friend class DbtuxProxy;
  friend struct mt_BuildIndxCtx;
  friend Uint32 Dbtux_mt_buildIndexFragment_wrapper_C(void*);
public:
  Dbtux(Block_context& ctx, Uint32 instanceNumber = 0);
  virtual ~Dbtux();

  void prepare_scan_ctx(Uint32 scanPtrI);
  void prepare_scan_bounds();
  // pointer to TUP and LQH instance in this thread
  Dbtup* c_tup;
  Dblqh* c_lqh;
  void execTUX_BOUND_INFO(Signal* signal);
  void execREAD_PSEUDO_REQ(Signal* signal);

private:
  // sizes are in words (Uint32)
  STATIC_CONST( MaxIndexFragments = MAX_FRAG_PER_LQH );
  STATIC_CONST( MaxIndexAttributes = MAX_ATTRIBUTES_IN_INDEX );
  STATIC_CONST( MaxAttrDataSize = 2 * MAX_ATTRIBUTES_IN_INDEX + MAX_KEY_SIZE_IN_WORDS );
  STATIC_CONST( MaxXfrmDataSize = MaxAttrDataSize * MAX_XFRM_MULTIPLY);
public:
  STATIC_CONST( DescPageSize = 512 );
private:
  STATIC_CONST( MaxTreeNodeSize = MAX_TTREE_NODE_SIZE );
  STATIC_CONST( MaxPrefSize = MAX_TTREE_PREF_SIZE );
  STATIC_CONST( ScanBoundSegmentSize = 7 );
  STATIC_CONST( MaxAccLockOps = MAX_PARALLEL_OP_PER_SCAN );
  STATIC_CONST( MaxTreeDepth = 32 );    // strict
#ifdef VM_TRACE
  // for TuxCtx::c_debugBuffer
  STATIC_CONST( DebugBufferBytes = (MaxAttrDataSize << 2) );
#endif
  BLOCK_DEFINES(Dbtux);

  // forward declarations
  struct TuxCtx;

  // AttributeHeader size is assumed to be 1 word
  STATIC_CONST( AttributeHeaderSize = 1 );

  /*
   * Logical tuple address, "local key".  Identifies table tuples.
   */
  typedef Uint32 TupAddr;
  STATIC_CONST( NullTupAddr = (Uint32)-1 );

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
  STATIC_CONST( TreeEntSize = sizeof(TreeEnt) >> 2 );
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
    Uint32 m_nodeScan;          // list of scans at this node
    TreeNode();
  };
  STATIC_CONST( NodeHeadSize = sizeof(TreeNode) >> 2 );

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
    Uint16 m_pos;               // position 0 to m_occup
    Uint8 m_dir;                // see scanNext
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

  DescPage_pool c_descPagePool;
  Uint32 c_descPageList;

  struct DescHead {
    Uint32 m_indexId;
    Uint16 m_numAttrs;
    Uint16 m_magic;
    enum { Magic = 0xDE5C };
  };
  STATIC_CONST( DescHeadSize = sizeof(DescHead) >> 2 );

  typedef NdbPack::Type KeyType;
  typedef NdbPack::Spec KeySpec;
  STATIC_CONST( KeyTypeSize = sizeof(KeyType) >> 2 );

  typedef NdbPack::DataC KeyDataC;
  typedef NdbPack::Data KeyData;
  typedef NdbPack::BoundC KeyBoundC;
  typedef NdbPack::Bound KeyBound;

  // range scan

  /*
   * ScanBound instances are members of ScanOp.  Bound data is stored in
   * a separate segmented buffer pool.
   */
  typedef ArrayPool<DataBufferSegment<ScanBoundSegmentSize> > ScanBoundSegment_pool;
  typedef DataBuffer<ScanBoundSegmentSize,ScanBoundSegment_pool> ScanBoundBuffer;
  typedef LocalDataBuffer<ScanBoundSegmentSize,ScanBoundSegment_pool> LocalScanBoundBuffer;
  struct ScanBound {
    ScanBoundBuffer::Head m_head;
    Uint16 m_cnt;       // number of attributes
    Int16 m_side;
    ScanBound();
  };
  ScanBoundSegment_pool c_scanBoundPool;

  // ScanLock
  struct ScanLock {
    ScanLock() {}
    Uint32 m_accLockOp;
    union {
    Uint32 nextPool;
    Uint32 nextList;
    };
    Uint32 prevList;
  };
  typedef Ptr<ScanLock> ScanLockPtr;
  typedef ArrayPool<ScanLock> ScanLock_pool;
  typedef DLFifoList<ScanLock_pool> ScanLock_fifo;
  typedef LocalDLFifoList<ScanLock_pool> Local_ScanLock_fifo;
  typedef ConstLocalDLFifoList<ScanLock_pool> ConstLocal_ScanLock_fifo;

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
  struct ScanOp;
  friend struct ScanOp;
  struct ScanOp {
    enum {
      Undef = 0,
      First = 1,                // before first entry
      Current = 2,              // at some entry
      Found = 3,                // return current as next scan result
      Blocked = 4,              // found and waiting for ACC lock
      Locked = 5,               // found and locked or no lock needed
      Next = 6,                 // looking for next extry
      Last = 7,                 // after last entry
      Aborting = 8
    };
    Uint8 m_state;
    Uint8 m_lockwait;
    Uint16 m_errorCode;
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
    TreePos m_scanPos;          // position
    TreeEnt m_scanEnt;          // latest entry found
    Uint32 m_nodeScan;          // next scan at node (single-linked)
    Uint32 m_statOpPtrI;        // RNIL unless this is a statistics scan
    union {
    Uint32 nextPool;
    Uint32 nextList;
    };
    Uint32 prevList;
    ScanOp();
  };
  typedef Ptr<ScanOp> ScanOpPtr;
  typedef ArrayPool<ScanOp> ScanOp_pool;
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

  Index_pool c_indexPool;
  RSS_AP_SNAPSHOT(c_indexPool);

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
    ScanOp_list m_scanList;  // current scans on this fragment
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

  Frag_pool c_fragPool;
  RSS_AP_SNAPSHOT(c_fragPool);

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
    Uint32 getNodeScan();
    // setters
    void setLink(unsigned i, TupLoc loc);
    void setSide(unsigned i);
    void setOccup(unsigned n);
    void setBalance(int b);
    void setNodeScan(Uint32 scanPtrI);
    // access other parts of the node
    Uint32* getPref();
    TreeEnt getEnt(unsigned pos);
    // for ndbrequire and ndbassert
    void progError(int line, int cause, const char* file, const char* check);
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
  void readKeyAttrsCurr(TuxCtx&,
                        const Frag& frag,
                        TreeEnt ent,
                        KeyData& keyData,
                        Uint32 count);
  void readTablePk(const Frag& frag, TreeEnt ent, Uint32* pkData, unsigned& pkSize);
  void unpackBound(TuxCtx&, const ScanBound& bound, KeyBoundC& searchBound);
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

  /*
   * DbtuxMaint.cpp
   */
  void execTUX_MAINT_REQ(Signal* signal);
  
  /*
   * DbtuxNode.cpp
   */
  int allocNode(TuxCtx&, NodeHandle& node);
  void freeNode(NodeHandle& node);
  void selectNode(NodeHandle& node, TupLoc loc);
  void insertNode(NodeHandle& node);
  void deleteNode(NodeHandle& node);
  void freePreallocatedNode(Frag& frag);
  void setNodePref(struct TuxCtx &, NodeHandle& node);
  // node operations
  void nodePushUp(TuxCtx&, NodeHandle& node, unsigned pos, const TreeEnt& ent, Uint32 scanList);
  void nodePushUpScans(NodeHandle& node, unsigned pos);
  void nodePopDown(TuxCtx&, NodeHandle& node, unsigned pos, TreeEnt& en, Uint32* scanList);
  void nodePopDownScans(NodeHandle& node, unsigned pos);
  void nodePushDown(TuxCtx&, NodeHandle& node, unsigned pos, TreeEnt& ent, Uint32& scanList);
  void nodePushDownScans(NodeHandle& node, unsigned pos);
  void nodePopUp(TuxCtx&, NodeHandle& node, unsigned pos, TreeEnt& ent, Uint32 scanList);
  void nodePopUpScans(NodeHandle& node, unsigned pos);
  void nodeSlide(TuxCtx&, NodeHandle& dstNode, NodeHandle& srcNode, unsigned cnt, unsigned i);
  // scans linked to node
  void addScanList(NodeHandle& node, unsigned pos, Uint32 scanList);
  void removeScanList(NodeHandle& node, unsigned pos, Uint32& scanList);
  void moveScanList(NodeHandle& node, unsigned pos);
  void linkScan(NodeHandle& node, ScanOpPtr scanPtr);
  void unlinkScan(NodeHandle& node, ScanOpPtr scanPtr);
  bool islinkScan(NodeHandle& node, ScanOpPtr scanPtr);

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
  void continue_scan(Signal *signal, ScanOpPtr scanPtr, Frag& frag);
  void scanFind(ScanOpPtr scanPtr, Frag& frag);
  void scanNext(ScanOpPtr scanPtr, bool fromMaintReq, Frag& frag);
  bool scanCheck(ScanOpPtr scanPtr, TreeEnt ent, Frag& frag);
  bool scanVisible(ScanOpPtr scanPtr, TreeEnt ent);
  void scanClose(Signal* signal, ScanOpPtr scanPtr);
  void abortAccLockOps(Signal* signal, ScanOpPtr scanPtr);
  void addAccLockOp(ScanOpPtr scanPtr, Uint32 accLockOp);
  void removeAccLockOp(ScanOpPtr scanPtr, Uint32 accLockOp);
  void releaseScanOp(ScanOpPtr& scanPtr);

  /*
   * DbtuxSearch.cpp
   */
  void findNodeToUpdate(TuxCtx&, Frag& frag, const KeyDataC& searchKey, TreeEnt searchEnt, NodeHandle& currNode);
  bool findPosToAdd(TuxCtx&, Frag& frag, const KeyDataC& searchKey, TreeEnt searchEnt, NodeHandle& currNode, TreePos& treePos);
  bool findPosToRemove(TuxCtx&, Frag& frag, const KeyDataC& searchKey, TreeEnt searchEnt, NodeHandle& currNode, TreePos& treePos);
  bool searchToAdd(TuxCtx&, Frag& frag, const KeyDataC& searchKey, TreeEnt searchEnt, TreePos& treePos);
  bool searchToRemove(TuxCtx&, Frag& frag, const KeyDataC& searchKey, TreeEnt searchEnt, TreePos& treePos);
  void findNodeToScan(Frag& frag, unsigned dir, const KeyBoundC& searchBound, NodeHandle& currNode);
  void findPosToScan(Frag& frag, unsigned idir, const KeyBoundC& searchBound, NodeHandle& currNode, Uint16* pos);
  void searchToScan(Frag& frag, unsigned idir, const KeyBoundC& searchBound, TreePos& treePos);

  /*
   * DbtuxCmp.cpp
   */
  int cmpSearchKey(TuxCtx&, const KeyDataC& searchKey, const KeyDataC& entryKey, Uint32 cnt);
  int cmpSearchBound(TuxCtx&, const KeyBoundC& searchBound, const KeyDataC& entryKey, Uint32 cnt);

  /*
   * DbtuxStat.cpp
   */
  // one-round-trip tree-dive records in range
  void statRecordsInRange(ScanOpPtr scanPtr, Uint32* out);
  Uint32 getEntriesBeforeOrAfter(Frag& frag, TreePos pos, unsigned idir);
  unsigned getPathToNode(NodeHandle node, Uint16* path);
  // stats scan
  int statScanInit(StatOpPtr, const Uint32* data, Uint32 len, Uint32* usedLen);
  int statScanAddRow(StatOpPtr, TreeEnt ent);
  void statScanReadKey(StatOpPtr, Uint32* out);
  void statScanReadValue(StatOpPtr, Uint32* out);
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
  NdbOut debugOut;
  unsigned debugFlags;
  enum {
    DebugMeta = 1,              // log create and drop index
    DebugMaint = 2,             // log maintenance ops
    DebugTree = 4,              // log and check tree after each op
    DebugScan = 8,              // log scans
    DebugLock = 16,             // log ACC locks
    DebugStat = 32              // log stats collection
  };
  STATIC_CONST( DataFillByte = 0xa2 );
  STATIC_CONST( NodeFillByte = 0xa4 );
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

    char searchBoundData_c[sizeof(KeyDataC)];
    char searchBound_c[sizeof(KeyBoundC)];
    char entryKey_c[sizeof(KeyData)];

    KeyDataC *searchBoundData;
    KeyBoundC *searchBound;
    KeyData *entryKey;
    Uint32 *keyAttrs;

    Uint32 numAttrs;
    Uint32 boundCnt;

    // buffer for scan bound and search key data
    Uint32* c_searchKey;

    // buffer for current entry key data
    Uint32* c_entryKey;

    // buffer for xfrm-ed PK and for temporary use
    Uint32* c_dataBuffer;

#ifdef VM_TRACE
    char* c_debugBuffer;
#endif
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
private:
  Uint32 mt_buildIndexFragment(struct mt_BuildIndxCtx*);

  Signal* c_signal_bug32040;
};

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
  m_nodeScan(RNIL)
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
  m_pos(ZNIL),
  m_dir(255)
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

// Dbtux::ScanOp

inline
Dbtux::ScanOp::ScanOp() :
  m_state(Undef),
  m_lockwait(false),
  m_errorCode(0),
  m_userPtr(RNIL),
  m_userRef(RNIL),
  m_tableId(RNIL),
  m_indexId(RNIL),
  m_fragPtrI(RNIL),
  m_transId1(0),
  m_transId2(0),
  m_savePointId(0),
  m_accLockOp(RNIL),
  m_accLockOps(),
  m_readCommitted(0),
  m_lockMode(0),
  m_descending(0),
  m_scanBound(),
  m_scanPos(),
  m_scanEnt(),
  m_nodeScan(RNIL),
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
  m_scanList(scanOpPool),
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

inline Uint32
Dbtux::NodeHandle::getNodeScan()
{
  return m_node->m_nodeScan;
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
    ndbrequire(false);
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
    ndbrequire(false);
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
Dbtux::NodeHandle::setNodeScan(Uint32 scanPtrI)
{
  m_node->m_nodeScan = scanPtrI;
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

// DbtuxCmp.cpp

inline int
Dbtux::cmpSearchKey(TuxCtx& ctx, const KeyDataC& searchKey, const KeyDataC& entryKey, Uint32 cnt)
{
  // compare cnt attributes from each
  Uint32 num_eq;
  int ret = searchKey.cmp(entryKey, cnt, num_eq);
#ifdef VM_TRACE
  if (debugFlags & DebugMaint) {
    debugOut << "cmpSearchKey: ret:" << ret;
    debugOut << " search:" << searchKey.print(ctx.c_debugBuffer, DebugBufferBytes);
    debugOut << " entry:" << entryKey.print(ctx.c_debugBuffer, DebugBufferBytes);
    debugOut << endl;
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
    debugOut << "cmpSearchBound: res:" << ret;
    debugOut << " search:" << searchBound.print(ctx.c_debugBuffer, DebugBufferBytes);
    debugOut << " entry:" << entryKey.print(ctx.c_debugBuffer, DebugBufferBytes);
    debugOut << endl;
  }
#endif
  return ret;
}


#undef JAM_FILE_ID

#endif
