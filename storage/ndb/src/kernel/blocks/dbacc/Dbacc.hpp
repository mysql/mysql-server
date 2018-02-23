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

#ifndef DBACC_H
#define DBACC_H

#if defined (VM_TRACE) && !defined(ACC_SAFE_QUEUE)
#define ACC_SAFE_QUEUE
#endif

#include <pc.hpp>
#include "Bitmask.hpp"
#include <DynArr256.hpp>
#include <SimulatedBlock.hpp>
#include <LHLevel.hpp>
#include <IntrusiveList.hpp>
#include "Container.hpp"
#include "signaldata/AccKeyReq.hpp"

#define JAM_FILE_ID 344


#ifdef DBACC_C

// Constants
/** ------------------------------------------------------------------------ 
 *   THESE ARE CONSTANTS THAT ARE USED FOR DEFINING THE SIZE OF BUFFERS, THE
 *   SIZE OF PAGE HEADERS, THE NUMBER OF BUFFERS IN A PAGE AND A NUMBER OF 
 *   OTHER CONSTANTS WHICH ARE CHANGED WHEN THE BUFFER SIZE IS CHANGED. 
 * ----------------------------------------------------------------------- */
#define ZHEAD_SIZE 32
#define ZBUF_SIZE 28
#define ZFREE_LIMIT 65
#define ZNO_CONTAINERS 64
#define ZELEM_HEAD_SIZE 1
/* ------------------------------------------------------------------------- */
/*  THESE CONSTANTS DEFINE THE USE OF THE PAGE HEADER IN THE INDEX PAGES.    */
/* ------------------------------------------------------------------------- */
#define ZPOS_PAGE_TYPE_BIT 14
#define ZNORMAL_PAGE_TYPE 0
#define ZOVERFLOW_PAGE_TYPE 1
#define ZADDFRAG 0
#define ZFRAGMENTSIZE 64
#define ZLEFT 1
#define ZOPRECSIZE 740
#define ZPAGESIZE 128
#define ZPARALLEL_QUEUE 1
#define ZSCAN_MAX_LOCK 4
#define ZSERIAL_QUEUE 2
#define ZSPH1 1
#define ZSPH2 2
#define ZSPH3 3
#define ZSPH6 6
#define ZREADLOCK 0
#define ZRIGHT 2
/**
 * Check kernel_types for other operation types
 */
#define ZSCAN_OP 8
#define ZSCAN_REC_SIZE 256
#define ZTABLESIZE 16

/* --------------------------------------------------------------------------------- */
/* CONTINUEB CODES                                                                   */
/* --------------------------------------------------------------------------------- */
#define ZINITIALISE_RECORDS 1
#define ZREL_ROOT_FRAG 5
#define ZREL_FRAG 6
#define ZREL_DIR 7
/* ------------------------------------------------------------------------- */
/* ERROR CODES                                                               */
/* ------------------------------------------------------------------------- */
#define ZLIMIT_OF_ERROR 600 // Limit check for error codes
#define ZCHECKROOT_ERROR 601 // Delete fragment error code
#define ZCONNECT_SIZE_ERROR 602 // ACC_SEIZEREF
#define ZDIR_RANGE_ERROR 603 // Add fragment error code
#define ZFULL_FRAGRECORD_ERROR 604 // Add fragment error code
#define ZFULL_ROOTFRAGRECORD_ERROR 605 // Add fragment error code
#define ZROOTFRAG_STATE_ERROR 606 // Add fragment
#define ZOVERTAB_REC_ERROR 607 // Add fragment

#define ZSCAN_REFACC_CONNECT_ERROR 608 // ACC_SCANREF
#define ZFOUR_ACTIVE_SCAN_ERROR 609 // ACC_SCANREF
#define ZNULL_SCAN_REC_ERROR 610 // ACC_SCANREF
#define ZDIRSIZE_ERROR 623
#define ZOVER_REC_ERROR 624 // Insufficient Space
#define ZPAGESIZE_ERROR 625
#define ZTUPLE_DELETED_ERROR 626
#define ZREAD_ERROR 626
#define ZWRITE_ERROR 630
#define ZTO_OP_STATE_ERROR 631
#define ZTOO_EARLY_ACCESS_ERROR 632
#define ZDIR_RANGE_FULL_ERROR 633 // on fragment

#define ZLOCAL_KEY_LENGTH_ERROR 634 // From Dbdict via Dblqh

#endif

class ElementHeader {
  /**
   * 
   * l = Locked    -- If true contains operation else scan bits + hash value
   * i = page index in dbtup fix page
   * h = Reduced hash value. The lower bits used for address is shifted away
   * o = Operation ptr I
   *
   *           1111111111222222222233
   * 01234567890123456789012345678901
   * liiiiiiiiiiiii  hhhhhhhhhhhhhhhh
   *  ooooooooooooooooooooooooooooooo
   */
public:
  static bool getLocked(Uint32 data);
  static bool getUnlocked(Uint32 data);
  static Uint32 getOpPtrI(Uint32 data);
  static LHBits16 getReducedHashValue(Uint32 data);
  static Uint16 getPageIdx(Uint32 data);

  static Uint32 setLocked(Uint32 opPtrI);
  static Uint32 setUnlocked(Uint16 page_idx, LHBits16 const& reducedHashValue);
  static Uint32 setReducedHashValue(Uint32 header, LHBits16 const& reducedHashValue);

  static Uint32 setInvalid();
  static bool isValid(Uint32 header);
};

inline 
bool
ElementHeader::getLocked(Uint32 data){
  assert(isValid(data));
  return (data & 1) == 1;
}

inline 
bool
ElementHeader::getUnlocked(Uint32 data){
  assert(isValid(data));
  return (data & 1) == 0;
}

inline
LHBits16
ElementHeader::getReducedHashValue(Uint32 data){
  assert(isValid(data));
  assert(getUnlocked(data));
  return LHBits16::unpack(data >> 16);
}

inline
Uint16
ElementHeader::getPageIdx(Uint32 data)
{
  /* Bits 1-13 is reserved for page index */
  NDB_STATIC_ASSERT(MAX_TUPLES_BITS <= 13);
  return (data >> 1) & MAX_TUPLES_PER_PAGE;
}

inline
Uint32 
ElementHeader::getOpPtrI(Uint32 data){
  assert(isValid(data));
  assert(getLocked(data));
  return data >> 1;
}

inline 
Uint32 
ElementHeader::setLocked(Uint32 opPtrI){
  assert(opPtrI < 0x8000000);
  return (opPtrI << 1) + 1;
}
inline
Uint32 
ElementHeader::setUnlocked(Uint16 page_idx, LHBits16 const& reducedHashValue)
{
  assert(page_idx <= MAX_TUPLES_PER_PAGE);
  return (Uint32(reducedHashValue.pack()) << 16) | (page_idx << 1) | 0;
}

inline
Uint32
ElementHeader::setReducedHashValue(Uint32 header, LHBits16 const& reducedHashValue)
{
  assert(getUnlocked(header));
  return (Uint32(reducedHashValue.pack()) << 16) | (header & 0xffff);
}

inline
Uint32
ElementHeader::setInvalid()
{
  /* unlocked, unscanned, bad reduced hash value */
  return 0;
}

inline
bool
ElementHeader::isValid(Uint32 header)
{
  return header != 0;
}

class Element
{
  Uint32 m_header;
  Uint32 m_data;
public:
  Element(Uint32 header, Uint32 data): m_header(header), m_data(data) {}
  Uint32 getHeader() const { return m_header; }
  Uint32 getData() const { return m_data; }
};

typedef Container::Header ContainerHeader;

class Dbacc: public SimulatedBlock {
  friend class DbaccProxy;

public:
// State values
enum State {
  FREEFRAG = 0,
  ACTIVEFRAG = 1,
  //SEND_QUE_OP = 2,
  WAIT_NOTHING = 10,
  WAIT_ONE_CONF = 26,
  FREE_OP = 30,
  WAIT_EXE_OP = 32,
  WAIT_IN_QUEUE = 34,
  EXE_OP = 35,
  SCAN_ACTIVE = 36,
  SCAN_WAIT_IN_QUEUE = 37,
  IDLE = 39,
  ACTIVE = 40,
  WAIT_COMMIT_ABORT = 41,
  ABORT = 42,
  ABORTADDFRAG = 43,
  REFUSEADDFRAG = 44,
  DELETEFRAG = 45,
  DELETETABLE = 46,
  UNDEFINEDROOT = 47,
  ADDFIRSTFRAG = 48,
  ADDSECONDFRAG = 49,
  DELETEFIRSTFRAG = 50,
  DELETESECONDFRAG = 51,
  ACTIVEROOT = 52
};

// Records

/* --------------------------------------------------------------------------------- */
/* PAGE8                                                                             */
/* --------------------------------------------------------------------------------- */
struct Page8 {
  Uint32 word32[2048];
  enum Page_variables {
    /**
     * First words are for the 32KiB page and must patch with header in Page32.
     * Words should be zeroed out for second to fourth 8KiB page on 32KiB page
     */
    P32_MAGIC = 0,
    P32_LIST_ID = 1,
    P32_NEXT_PAGE = 2,
    P32_PREV_PAGE = 3,
    P32_WORD_COUNT = 4, // Not an variable index, but count of P32 variables
    /**
     * Following words are used for each 8KiB page
     */
    PAGE_ID = 4,
    EMPTY_LIST = 5,
    ALLOC_CONTAINERS = 6,
    CHECKSUM = 7,
    NEXT_PAGE = 8,
    PREV_PAGE = 9,
    SCAN_CON_0_3 = 10,
    SCAN_CON_4_7 = 11,
    SCAN_CON_8_11 = 12,
  };
  Uint8 getContainerShortIndex(Uint32 pointer) const;
  void setScanContainer(Uint16 scanbit, Uint32 conptr);
  void clearScanContainer(Uint16 scanbit, Uint32 conptr);
  bool checkScanContainer(Uint32 conptr) const;
  Uint16 checkScans(Uint16 scanmask, Uint32 conptr) const;
}; /* p2c: size = 8192 bytes */
  typedef Ptr<Page8> Page8Ptr;

struct Page8SLinkMethods
{
  static Uint32 getNext(Page8 const& item) { return item.word32[Page8::NEXT_PAGE]; }
  static void setNext(Page8& item, Uint32 next) { item.word32[Page8::NEXT_PAGE] = next; }
  static void setPrev(Page8& /* item */, Uint32 /* prev */) { /* no op for single linked list */ }
};

struct ContainerPageLinkMethods
{
  static Uint32 getNext(Page8 const& item) { return item.word32[Page8::NEXT_PAGE]; }
  static void setNext(Page8& item, Uint32 next) { item.word32[Page8::NEXT_PAGE] = next; }
  static Uint32 getPrev(Page8 const& item) { return item.word32[Page8::PREV_PAGE]; }
  static void setPrev(Page8& item, Uint32 prev) { item.word32[Page8::PREV_PAGE] = prev; }
};

struct Page32
{
  enum { MAGIC = 0x17283482 };
  union {
    struct {
      /* fields must match P32-values in Page_variables */
      Uint32 magic;
      Uint32 list_id;
      Uint32 nextList;
      Uint32 prevList;
    };
    Page8 page8[4];
  };
};

typedef Ptr<Page32> Page32Ptr;
typedef ArrayPool<Page32> Page32_pool;
typedef DLCFifoList<Page32_pool> Page32_list;
typedef LocalDLCFifoList<Page32_pool> LocalPage32_list;

  class Page32Lists {
    Page32_list::Head lists[16];
    Uint32 sub_page_id_count[4];
    Uint16 nonempty_lists;

    static Uint16 sub_page_id_to_list_id_set(int sub_page_id);
    static Uint8 list_id_to_sub_page_id_set(int list_id);
    static Uint8 sub_page_id_set_to_list_id(int sub_page_id_set);

    Uint8 least_free_list(Uint16 list_id_set);
public:
    enum { ANY_SUB_PAGE = -1, LEAST_COMMON_SUB_PAGE = -2 };
    Page32Lists();

    Uint32 getCount() const;
    void addPage32(Page32_pool& pool, Page32Ptr p);
    void dropLastPage32(Page32_pool& pool, Page32Ptr& p, Uint32 keep);
    void dropPage32(Page32_pool& pool, Page32Ptr p);
    void seizePage8(Page32_pool& pool, Page8Ptr& /* out */ p, int sub_page_id);
    void releasePage8(Page32_pool& pool, Page8Ptr p);
    bool haveFreePage8(int sub_page_id) const;
  };

class Page8_pool
{
public:
  typedef Page8 Type;
  explicit Page8_pool(Page32_pool& pool): m_page_pool(pool) { }
  void getPtr(Ptr<Page8>& page) const;
  void getPtrForce(Ptr<Page8>& page) const;
private:
  Page32_pool& m_page_pool;
};

typedef SLCFifoList<Page8_pool,Page8,Page8SLinkMethods> Page8List;
typedef LocalSLCFifoList<Page8_pool,Page8,Page8SLinkMethods> LocalPage8List;
typedef DLCFifoList<Page8_pool,Page8,ContainerPageLinkMethods> ContainerPageList;
typedef LocalDLCFifoList<Page8_pool,Page8,ContainerPageLinkMethods> LocalContainerPageList;

/* --------------------------------------------------------------------------------- */
/* FRAGMENTREC. ALL INFORMATION ABOUT FRAMENT AND HASH TABLE IS SAVED IN FRAGMENT    */
/*         REC  A POINTER TO FRAGMENT RECORD IS SAVED IN ROOTFRAGMENTREC FRAGMENT    */
/* --------------------------------------------------------------------------------- */
struct Fragmentrec {
  Uint32 scan[MAX_PARALLEL_SCANS_PER_FRAG];
  Uint16 activeScanMask;
  union {
    Uint32 mytabptr;
    Uint32 myTableId;
  };
  union {
    Uint32 fragmentid;
    Uint32 myfid;
  };
  Uint32 tupFragptr;
  Uint32 roothashcheck;
  Uint32 m_commit_count;
  State rootState;
  
//-----------------------------------------------------------------------------
// Temporary variables used during shrink and expand process.
//-----------------------------------------------------------------------------
  Uint32 expReceivePageptr;
  Uint32 expReceiveIndex;
  bool expReceiveIsforward;
  Uint32 expSenderDirIndex;
  Uint32 expSenderIndex;
  Uint32 expSenderPageptr;

//-----------------------------------------------------------------------------
// List of lock owners currently used only for self-check
//-----------------------------------------------------------------------------
  Uint32 lockOwnersList;

//-----------------------------------------------------------------------------
// References to Directory Ranges (which in turn references directories, which
// in its turn references the pages) for the bucket pages and the overflow
// bucket pages.
//-----------------------------------------------------------------------------
  DynArr256::Head directory;

//-----------------------------------------------------------------------------
// We have a list of overflow pages with free areas. We have a special record,
// the overflow record representing these pages. The reason is that the
// same record is also used to represent pages in the directory array that have
// been released since they were empty (there were however higher indexes with
// data in them). These are put in the firstFreeDirIndexRec-list.
// An overflow record representing a page can only be in one of these lists.
//-----------------------------------------------------------------------------
  ContainerPageList::Head fullpages; // For pages where only containers on page are allowed to overflow (word32[ZPOS_ALLOC_CONTAINERS] > ZFREE_LIMIT)
  ContainerPageList::Head sparsepages; // For pages that other pages are still allowed to overflow into (0 < word32[ZPOS_ALLOC_CONTAINERS] <= ZFREE_LIMIT)

//-----------------------------------------------------------------------------
// Counter keeping track of how many times we have expanded. We need to ensure
// that we do not shrink so many times that this variable becomes negative.
//-----------------------------------------------------------------------------
  Uint32 expandCounter;

//-----------------------------------------------------------------------------
// These variables are important for the linear hashing algorithm.
// localkeylen is the size of the local key (1 and 2 is currently supported)
// maxloadfactor is the factor specifying when to expand
// minloadfactor is the factor specifying when to shrink (hysteresis model)
// maxp and p
// maxp and p is the variables most central to linear hashing. p + maxp + 1 is the
// current number of buckets. maxp is the largest value of the type 2**n - 1
// which is smaller than the number of buckets. These values are used to find
// correct bucket with the aid of the hash value.
//
// slack is the variable keeping track of whether we have inserted more than
// the current size is suitable for or less. Slack together with the boundaries
// set by maxloadfactor and minloadfactor decides when to expand/shrink
// slackCheck When slack goes over this value it is time to expand.
// slackCheck = (maxp + p + 1)*(maxloadfactor - minloadfactor) or 
// bucketSize * hysteresis
// Since at most RNIL 8KiB-pages can be used for a fragment, the extreme values
// for slack will be within -2^43 and +2^43 words.
//-----------------------------------------------------------------------------
  LHLevelRH level;
  Uint32 localkeylen; // Currently only 1 is supported
  Uint32 maxloadfactor;
  Uint32 minloadfactor;
  Int64 slack;
  Int64 slackCheck;

//-----------------------------------------------------------------------------
// nextfreefrag is the next free fragment if linked into a free list
//-----------------------------------------------------------------------------
  Uint32 nextfreefrag;

//-----------------------------------------------------------------------------
// Fragment State, mostly applicable during LCP and restore
//-----------------------------------------------------------------------------
  State fragState;

//-----------------------------------------------------------------------------
// elementLength: Length of element in bucket and overflow pages
// keyLength: Length of key
//-----------------------------------------------------------------------------
  STATIC_CONST( elementLength = 2 );
  Uint16 keyLength;

//-----------------------------------------------------------------------------
// Only allow one expand or shrink signal in queue at the time.
//-----------------------------------------------------------------------------
  bool expandOrShrinkQueued;

//-----------------------------------------------------------------------------
// hashcheckbit is the bit to check whether to send element to split bucket or not
// k (== 6) is the number of buckets per page
//-----------------------------------------------------------------------------
  STATIC_CONST( k = 6 );
  STATIC_CONST( MIN_HASH_COMPARE_BITS = 7 );
  STATIC_CONST( MAX_HASH_VALUE_BITS = 31 );

//-----------------------------------------------------------------------------
// nodetype can only be STORED in this release. Is currently only set, never read
//-----------------------------------------------------------------------------
  Uint8 nodetype;

//-----------------------------------------------------------------------------
// flag to avoid accessing table record if no char attributes
//-----------------------------------------------------------------------------
  Uint8 hasCharAttr;

//-----------------------------------------------------------------------------
// flag to mark that execEXPANDCHECK2 has failed due to DirRange full
//-----------------------------------------------------------------------------
  Uint8 dirRangeFull;

  // Number of Page8 pages allocated for the hash index.
  Int32 m_noOfAllocatedPages;

//-----------------------------------------------------------------------------
// Lock stats
//-----------------------------------------------------------------------------
// Used to track row lock activity on this fragment
  struct LockStats 
  {
    /* Exclusive row lock counts */

    /*   Total requests received */
    Uint64 m_ex_req_count;

    /*   Total requests immediately granted */
    Uint64 m_ex_imm_ok_count;

    /*   Total requests granted after a wait */
    Uint64 m_ex_wait_ok_count;

    /*   Total requests failed after a wait */
    Uint64 m_ex_wait_fail_count;
    

    /* Shared row lock counts */

    /*   Total requests received */
    Uint64 m_sh_req_count;

    /*   Total requests immediately granted */
    Uint64 m_sh_imm_ok_count;

    /*   Total requests granted after a wait */
    Uint64 m_sh_wait_ok_count;

    /*   Total requests failed after a wait */
    Uint64 m_sh_wait_fail_count;

    /* Wait times */


    /*   Total time spent waiting for a lock
     *   which was eventually granted
     */
    Uint64 m_wait_ok_millis;

    /*   Total time spent waiting for a lock
     *   which was not eventually granted
     */
    Uint64 m_wait_fail_millis;
    
    void init()
    {
      m_ex_req_count       = 0;
      m_ex_imm_ok_count    = 0;
      m_ex_wait_ok_count   = 0;
      m_ex_wait_fail_count = 0;
    
      m_sh_req_count       = 0;
      m_sh_imm_ok_count    = 0;
      m_sh_wait_ok_count   = 0;
      m_sh_wait_fail_count = 0;

      m_wait_ok_millis     = 0;
      m_wait_fail_millis   = 0;
    };

    // req_start_imm_ok
    // A request was immediately granted (No contention)
    void req_start_imm_ok(bool ex,
                          NDB_TICKS& op_timestamp,
                          const NDB_TICKS now)
    {
      if (ex)
      {
        m_ex_req_count++;
        m_ex_imm_ok_count++;
      }
      else
      {
        m_sh_req_count++;
        m_sh_imm_ok_count++;
      }

      /* Hold-time starts */
      op_timestamp = now;
    }

    // req_start
    // A request was not granted immediately
    void req_start(bool ex, 
                   NDB_TICKS& op_timestamp, 
                   const NDB_TICKS now)
    {
      if (ex)
      {
        m_ex_req_count++;
      }
      else
      {
        m_sh_req_count++;
      }

      /* Wait-time starts */
      op_timestamp = now;
    }

    // wait_ok
    // A request that had to wait is now granted
    void wait_ok(bool ex, NDB_TICKS& op_timestamp, const NDB_TICKS now)
    {
      assert(NdbTick_IsValid(op_timestamp)); /* Set when starting to wait */
      if (ex)
      {
        m_ex_wait_ok_count++;
      }
      else
      {
        m_sh_wait_ok_count++;
      }

      const Uint64 wait_millis = NdbTick_Elapsed(op_timestamp,
                                                 now).milliSec();
      m_wait_ok_millis += wait_millis;
      
      /* Hold-time starts */
      op_timestamp = now;
    }
    
    // wait_fail
    // A request that had to wait has now been
    // aborted.  May or may not be due to TC
    // timeout
    void wait_fail(bool ex, NDB_TICKS& wait_start, const NDB_TICKS now)
    {
      assert(NdbTick_IsValid(wait_start));
      if (ex)
      {
        m_ex_wait_fail_count++;
      }
      else
      {
        m_sh_wait_fail_count++;
      }
      
      const Uint64 wait_millis = NdbTick_Elapsed(wait_start,
                                                 now).milliSec();
      m_wait_fail_millis += wait_millis;
      /* Debugging */
      NdbTick_Invalidate(&wait_start);
    }
  };

  LockStats m_lockStats;

public:
  Uint32 getPageNumber(Uint32 bucket_number) const;
  Uint32 getPageIndex(Uint32 bucket_number) const;
  bool enough_valid_bits(LHBits16 const& reduced_hash_value) const;
};

  typedef Ptr<Fragmentrec> FragmentrecPtr;
  void set_tup_fragptr(Uint32 fragptr, Uint32 tup_fragptr);

/* --------------------------------------------------------------------------------- */
/* OPERATIONREC                                                                      */
/* --------------------------------------------------------------------------------- */
struct Operationrec {
  Uint32 m_op_bits;
  Local_key localdata;
  Uint32 elementPage;
  Uint32 elementPointer;
  Uint32 fid;
  Uint32 fragptr;
  LHBits32 hashValue;
  Uint32 nextLockOwnerOp;
  Uint32 nextOp;
  Uint32 nextParallelQue;
  union {
    Uint32 nextSerialQue;      
    Uint32 m_lock_owner_ptr_i; // if nextParallelQue = RNIL, else undefined
  };
  Uint32 prevOp;
  Uint32 prevLockOwnerOp;
  union {
    Uint32 prevParallelQue;
    Uint32 m_lo_last_parallel_op_ptr_i;
  };
  union {
    Uint32 prevSerialQue;
    Uint32 m_lo_last_serial_op_ptr_i;
  };
  Uint32 scanRecPtr;
  Uint32 transId1;
  Uint32 transId2;
  Uint32 userptr;
  Uint16 elementContainer;
  Uint16 tupkeylen;
  Uint32 xfrmtupkeylen;
  Uint32 userblockref;
  enum { ANY_SCANBITS = Uint16(0xffff) };
  LHBits16 reducedHashValue;
  NDB_TICKS m_lockTime;

  enum OpBits {
    OP_MASK                 = 0x0000F // 4 bits for operation type
    ,OP_LOCK_MODE           = 0x00010 // 0 - shared lock, 1 = exclusive lock
    ,OP_ACC_LOCK_MODE       = 0x00020 // Or:de lock mode of all operation
                                      // before me
    ,OP_LOCK_OWNER          = 0x00040
    ,OP_RUN_QUEUE           = 0x00080 // In parallell queue of lock owner
    ,OP_DIRTY_READ          = 0x00100
    ,OP_LOCK_REQ            = 0x00200 // isAccLockReq
    ,OP_COMMIT_DELETE_CHECK = 0x00400
    ,OP_INSERT_IS_DONE      = 0x00800
    ,OP_ELEMENT_DISAPPEARED = 0x01000
    
    ,OP_STATE_MASK          = 0xF0000
    ,OP_STATE_IDLE          = 0xF0000
    ,OP_STATE_WAITING       = 0x00000
    ,OP_STATE_RUNNING       = 0x10000
    ,OP_STATE_EXECUTED      = 0x30000
    
    ,OP_EXECUTED_DIRTY_READ = 0x3050F
    ,OP_INITIAL             = ~(Uint32)0
  };
  
  Operationrec() {}
  bool is_same_trans(const Operationrec* op) const {
    return 
      transId1 == op->transId1 && transId2 == op->transId2;
  }
  
}; /* p2c: size = 168 bytes */

  typedef Ptr<Operationrec> OperationrecPtr;

/* --------------------------------------------------------------------------------- */
/* SCAN_REC                                                                          */
/* --------------------------------------------------------------------------------- */
struct ScanRec {
  enum ScanState {
    WAIT_NEXT = 0,
    SCAN_DISCONNECT = 1
  };
  enum ScanBucketState {
    FIRST_LAP = 0,
    SECOND_LAP = 1,
    SCAN_COMPLETED = 2
  };
  Uint32 activeLocalFrag;
  Uint32 nextBucketIndex;
  Uint32 scanNextfreerec;
  Uint32 scanFirstActiveOp;
  Uint32 scanFirstLockedOp;
  Uint32 scanLastLockedOp;
  Uint32 scanFirstQueuedOp;
  Uint32 scanLastQueuedOp;
  Uint32 scanUserptr;
  Uint32 scanTrid1;
  Uint32 scanTrid2;
  Uint32 startNoOfBuckets;
  Uint32 minBucketIndexToRescan;
  Uint32 maxBucketIndexToRescan;
  Uint32 scanOpsAllocated;
  Uint32 scanLockCount;
  ScanBucketState scanBucketState;
  ScanState scanState;
  Uint16 scanLockHeld;
  Uint16 scan_lastSeen;
  Uint32 scanUserblockref;
  Uint32 scanMask;
  Uint8 scanLockMode;
  Uint8 scanReadCommittedFlag;
private:
  Uint32 inPageI;
  Uint32 inConptr;
  Uint32 elemScanned;
  enum { ELEM_SCANNED_BITS = sizeof(Uint32) * 8 };
public:
  void initContainer();
  bool isInContainer() const;
  bool getContainer(Uint32& pagei, Uint32& conptr) const;
  void enterContainer(Uint32 pagei, Uint32 conptr);
  void leaveContainer(Uint32 pagei, Uint32 conptr);
  bool isScanned(Uint32 elemptr) const;
  void setScanned(Uint32 elemptr);
  void clearScanned(Uint32 elemptr);
  void moveScanBit(Uint32 toptr, Uint32 fromptr);
};

  typedef Ptr<ScanRec> ScanRecPtr;


/* --------------------------------------------------------------------------------- */
/* TABREC                                                                            */
/* --------------------------------------------------------------------------------- */
struct Tabrec {
  Uint32 fragholder[MAX_FRAG_PER_LQH];
  Uint32 fragptrholder[MAX_FRAG_PER_LQH];
  Uint32 tabUserPtr;
  BlockReference tabUserRef;
  Uint32 tabUserGsn;
};
  typedef Ptr<Tabrec> TabrecPtr;

public:
  Dbacc(Block_context&, Uint32 instanceNumber = 0);
  virtual ~Dbacc();

  // pointer to TUP instance in this thread
  class Dbtup* c_tup;
  class Dblqh* c_lqh;

  void execACCMINUPDATE(Signal* signal);
  // Get the size of the logical to physical page map, in bytes.
  Uint32 getL2PMapAllocBytes(Uint32 fragId) const;
  void removerow(Uint32 op, const Local_key*);

  // Get the size of the linear hash map in bytes.
  Uint64 getLinHashByteSize(Uint32 fragId) const;

private:
  BLOCK_DEFINES(Dbacc);

  // Transit signals
  void execDEBUG_SIG(Signal* signal);
  void execCONTINUEB(Signal* signal);
  void execACC_CHECK_SCAN(Signal* signal);
  void execEXPANDCHECK2(Signal* signal);
  void execSHRINKCHECK2(Signal* signal);
  void execACC_OVER_REC(Signal* signal);
  void execNEXTOPERATION(Signal* signal);

  // Received signals
  void execSTTOR(Signal* signal);
  void execACCKEYREQ(Signal* signal);
  void execACCSEIZEREQ(Signal* signal);
  void execACCFRAGREQ(Signal* signal);
  void execNEXT_SCANREQ(Signal* signal);
  void execACC_ABORTREQ(Signal* signal);
  void execACC_SCANREQ(Signal* signal);
  void execACC_COMMITREQ(Signal* signal);
  void execACC_TO_REQ(Signal* signal);
  void execACC_LOCKREQ(Signal* signal);
  void execNDB_STTOR(Signal* signal);
  void execDROP_TAB_REQ(Signal* signal);
  void execREAD_CONFIG_REQ(Signal* signal);
  void execDUMP_STATE_ORD(Signal* signal);

  void execDROP_FRAG_REQ(Signal*);

  void execDBINFO_SCANREQ(Signal *signal);

  // Statement blocks
  void commitDeleteCheck() const;
  void report_dealloc(Signal* signal, const Operationrec* opPtrP);
  
  typedef void * RootfragmentrecPtr;
  void initRootFragPageZero(FragmentrecPtr, Page8Ptr) const;
  void initFragAdd(Signal*, FragmentrecPtr) const;
  void initFragPageZero(FragmentrecPtr, Page8Ptr) const;
  void initFragGeneral(FragmentrecPtr) const;
  void verifyFragCorrect(FragmentrecPtr regFragPtr) const;
  void releaseFragResources(Signal* signal, Uint32 fragIndex);
  void releaseRootFragRecord(Signal* signal, RootfragmentrecPtr rootPtr) const;
  void releaseRootFragResources(Signal* signal, Uint32 tableId);
  void releaseDirResources(Signal* signal);
  void releaseDirectoryResources(Signal* signal,
                                 Uint32 fragIndex,
                                 Uint32 dirIndex,
                                 Uint32 startIndex,
                                 Uint32 directoryIndex) const;
  void releaseFragRecord(FragmentrecPtr regFragPtr);
  void initScanFragmentPart();
  Uint32 checkScanExpand(Uint32 splitBucket);
  Uint32 checkScanShrink(Uint32 sourceBucket, Uint32 destBucket);
  void initialiseFragRec();
  void initialiseFsConnectionRec(Signal* signal) const;
  void initialiseFsOpRec(Signal* signal) const;
  void initialiseOperationRec();
  void initialisePageRec();
  void initialiseRootfragRec(Signal* signal) const;
  void initialiseScanRec();
  void initialiseTableRec();
  bool addfragtotab(Uint32 rootIndex, Uint32 fragId) const;
  void initOpRec(const AccKeyReq* signal, Uint32 siglen) const;
  void sendAcckeyconf(Signal* signal) const;
  Uint32 getNoParallelTransaction(const Operationrec*) const;

#ifdef VM_TRACE
  Uint32 getNoParallelTransactionFull(const Operationrec*) const;
#endif
#ifdef ACC_SAFE_QUEUE
  bool validate_lock_queue(OperationrecPtr opPtr) const;
  Uint32 get_parallel_head(OperationrecPtr opPtr) const;
  void dump_lock_queue(OperationrecPtr loPtr) const;
#else
  bool validate_lock_queue(OperationrecPtr) const { return true;}
#endif
  /**
    Return true if the sum of per fragment pages counts matches the total
    page count (cnoOfAllocatedPages). Used for consistency checks. 
   */
  bool validatePageCount() const;
public:  
  void execACCKEY_ORD(Signal* signal, Uint32 opPtrI);
  void startNext(Signal* signal, OperationrecPtr lastOp);
  
private:
  Uint32 placeReadInLockQueue(OperationrecPtr lockOwnerPtr) const;
  Uint32 placeWriteInLockQueue(OperationrecPtr lockOwnerPtr) const;
  void placeSerialQueue(OperationrecPtr lockOwner, OperationrecPtr op) const;
  void abortSerieQueueOperation(Signal* signal, OperationrecPtr op);  
  void abortParallelQueueOperation(Signal* signal, OperationrecPtr op);
  
  void expandcontainer(Page8Ptr pageptr, Uint32 conidx);
  void shrinkcontainer(Page8Ptr pageptr,
                       Uint32 conptr,
                       bool isforward,
                       Uint32 conlen);
  void releaseAndCommitActiveOps(Signal* signal);
  void releaseAndCommitQueuedOps(Signal* signal);
  void releaseAndAbortLockedOps(Signal* signal);
  void getContainerIndex(Uint32 pointer, Uint32& index, bool& isforward) const;
  Uint32 getContainerPtr(Uint32 index, bool isforward) const;
  Uint32 getForwardContainerPtr(Uint32 index) const;
  Uint32 getBackwardContainerPtr(Uint32 index) const;
  bool getScanElement(Page8Ptr& pageptr,
                      Uint32& conidx,
                      Uint32& conptr,
                      bool& isforward,
                      Uint32& elemptr,
                      Uint32& islocked) const;
  void initScanOpRec(Page8Ptr pageptr,
                     Uint32 conptr,
                     Uint32 elemptr) const;
  void nextcontainerinfo(Page8Ptr& pageptr,
                         Uint32 conptr,
                         ContainerHeader containerhead,
                         Uint32& nextConidx,
                         bool& nextIsforward) const;
  void putActiveScanOp() const;
  void putOpScanLockQue() const;
  void putReadyScanQueue(Uint32 scanRecIndex) const;
  void releaseScanBucket(Page8Ptr pageptr,
                         Uint32 conidx,
                         Uint16 scanMask) const;
  void releaseScanContainer(Page8Ptr pageptr,
                            Uint32 conptr,
                            bool isforward,
                            Uint32 conlen,
                            Uint16 scanMask,
                            Uint16 allScanned) const;
  void releaseScanRec();
  bool searchScanContainer(Page8Ptr pageptr,
                           Uint32 conptr,
                           bool isforward,
                           Uint32 conlen,
                           Uint32& elemptr,
                           Uint32& islocked) const;
  void sendNextScanConf(Signal* signal);
  void setlock(Page8Ptr pageptr, Uint32 elemptr) const;
  void takeOutActiveScanOp() const;
  void takeOutScanLockQueue(Uint32 scanRecIndex) const;
  void takeOutReadyScanQueue() const;
  void insertElement(Element elem,
                     OperationrecPtr oprecptr,
                     Page8Ptr& pageptr,
                     Uint32& conidx,
                     bool& isforward,
                     Uint32& conptr,
                     Uint16 conScanMask,
                     bool newBucket);
  void insertContainer(Element elem,
                       OperationrecPtr  oprecptr,
                       Page8Ptr pageptr,
                       Uint32 conidx,
                       bool isforward,
                       Uint32& conptr,
                       ContainerHeader& containerhead,
                       Uint16 conScanMask,
                       bool newContainer,
                       Uint32& result);
  void addnewcontainer(Page8Ptr pageptr, Uint32 conptr,
    Uint32 nextConidx, Uint32 nextContype, bool nextSamepage,
    Uint32 nextPagei) const;
  void getfreelist(Page8Ptr pageptr, Uint32& pageindex, Uint32& buftype);
  void increaselistcont(Page8Ptr);
  void seizeLeftlist(Page8Ptr slPageptr, Uint32 conidx);
  void seizeRightlist(Page8Ptr slPageptr, Uint32 conidx);
  Uint32 readTablePk(Uint32, Uint32, Uint32, OperationrecPtr, Uint32*);
  Uint32 getElement(const AccKeyReq* signal,
                    OperationrecPtr& lockOwner,
                    Page8Ptr& bucketPageptr,
                    Uint32& bucketConidx,
                    Page8Ptr& elemPageptr,
                    Uint32& elemConptr,
                    Uint32& elemptr);
  LHBits32 getElementHash(OperationrecPtr& oprec);
  LHBits32 getElementHash(Uint32 const* element);
  LHBits32 getElementHash(Uint32 const* element, OperationrecPtr& oprec);
  void shrink_adjust_reduced_hash_value(Uint32 bucket_number);
  Uint32 getPagePtr(DynArr256::Head&, Uint32);
  bool setPagePtr(DynArr256::Head& directory, Uint32 index, Uint32 ptri);
  Uint32 unsetPagePtr(DynArr256::Head& directory, Uint32 index);
  void getdirindex(Page8Ptr& pageptr, Uint32& conidx);
  void commitdelete(Signal* signal);
  void deleteElement(Page8Ptr delPageptr, Uint32 delConptr,
      Uint32 delElemptr, Page8Ptr lastPageptr, Uint32 lastElemptr) const;
  void getLastAndRemove(Page8Ptr tlastPrevpageptr, Uint32 tlastPrevconptr,
     Page8Ptr& lastPageptr, Uint32& tlastPageindex, Uint32& tlastContainerptr,
     bool& tlastIsforward, Uint32& tlastElementptr);
  void releaseLeftlist(Page8Ptr rlPageptr, Uint32 conidx, Uint32 conptr);
  void releaseRightlist(Page8Ptr rlPageptr, Uint32 conidx, Uint32 conptr);
  void checkoverfreelist(Page8Ptr colPageptr);
  void abortOperation(Signal* signal);
  void commitOperation(Signal* signal);
  void copyOpInfo(OperationrecPtr dst, OperationrecPtr src) const;
  Uint32 executeNextOperation(Signal* signal) const;
  void releaselock(Signal* signal) const;
  void release_lockowner(Signal* signal, OperationrecPtr, bool commit);
  void startNew(Signal* signal, OperationrecPtr newOwner);
  void abortWaitingOperation(Signal*, OperationrecPtr) const;
  void abortExecutedOperation(Signal*, OperationrecPtr) const;
  
  void takeOutFragWaitQue(Signal* signal) const;
  void check_lock_upgrade(Signal* signal,
                          OperationrecPtr release_op,
                          bool lo) const;
  void check_lock_upgrade(Signal* signal, OperationrecPtr lock_owner,
			  OperationrecPtr release_op) const;
  Uint32 allocOverflowPage();
  bool getfragmentrec(FragmentrecPtr&, Uint32 fragId);
  void insertLockOwnersList(const OperationrecPtr&) const;
  void takeOutLockOwnersList(const OperationrecPtr&) const;

  void initFsOpRec(Signal* signal) const;
  void initOverpage(Page8Ptr);
  void initPage(Page8Ptr, Uint32);
  void initRootfragrec(Signal* signal) const;
  void putOpInFragWaitQue(Signal* signal) const;
  void releaseFsConnRec(Signal* signal) const;
  void releaseFsOpRec(Signal* signal) const;
  void releaseOpRec();
  void releaseOverpage(Page8Ptr ropPageptr);
  void releasePage(Page8Ptr rpPageptr);
  void seizeDirectory(Signal* signal) const;
  void seizeFragrec();
  void seizeFsConnectRec(Signal* signal) const;
  void seizeFsOpRec(Signal* signal) const;
  void seizeOpRec();
  Uint32 seizePage(Page8Ptr& spPageptr, int sub_page_id);
  void seizeRootfragrec(Signal* signal) const;
  void seizeScanRec();
  void sendSystemerror(int line) const;

  void addFragRefuse(Signal* signal, Uint32 errorCode) const;
  void acckeyref1Lab(Signal* signal, Uint32 result_code) const;
  void insertelementLab(Signal* signal,
                        Page8Ptr bucketPageptr,
                        Uint32 bucketConidx);
  void checkNextFragmentLab(Signal* signal);
  void endofexpLab(Signal* signal);
  void endofshrinkbucketLab(Signal* signal);
  void sttorrysignalLab(Signal* signal, Uint32 signalkey) const;
  void sendholdconfsignalLab(Signal* signal) const;
  void accIsLockedLab(Signal* signal, OperationrecPtr lockOwnerPtr) const;
  void insertExistElemLab(Signal* signal, OperationrecPtr lockOwnerPtr) const;
  void releaseScanLab(Signal* signal);
  void initialiseRecordsLab(Signal* signal, Uint32, Uint32, Uint32);
  void checkNextBucketLab(Signal* signal);
  void storeDataPageInDirectoryLab(Signal* signal) const;

  void zpagesize_error(const char* where);

  // charsets
  void xfrmKeyData(AccKeyReq* signal) const;

  // Initialisation
  void initData();
  void initRecords();

#ifdef VM_TRACE
  void debug_lh_vars(const char* where) const;
#else
  void debug_lh_vars(const char* where) const {}
#endif

private:
  // Variables
/* --------------------------------------------------------------------------------- */
/* DIRECTORY                                                                         */
/* --------------------------------------------------------------------------------- */
  DynArr256Pool   directoryPool;
/* --------------------------------------------------------------------------------- */
/* FRAGMENTREC. ALL INFORMATION ABOUT FRAMENT AND HASH TABLE IS SAVED IN FRAGMENT    */
/*         REC  A POINTER TO FRAGMENT RECORD IS SAVED IN ROOTFRAGMENTREC FRAGMENT    */
/* --------------------------------------------------------------------------------- */
  Fragmentrec *fragmentrec;
  FragmentrecPtr fragrecptr;
  Uint32 cfirstfreefrag;
  Uint32 cfragmentsize;
  RSS_OP_COUNTER(cnoOfFreeFragrec);
  RSS_OP_SNAPSHOT(cnoOfFreeFragrec);


/* --------------------------------------------------------------------------------- */
/* FS_CONNECTREC                                                                     */
/* --------------------------------------------------------------------------------- */
/* OPERATIONREC                                                                      */
/* --------------------------------------------------------------------------------- */
  Operationrec *operationrec;
  OperationrecPtr operationRecPtr;
  OperationrecPtr queOperPtr;
  Uint32 cfreeopRec;
  Uint32 coprecsize;

/* --------------------------------------------------------------------------------- */
/* PAGE8                                                                             */
/* --------------------------------------------------------------------------------- */
  /* 8 KB PAGE                       */
  Page32Lists pages;
  Page8List::Head cfreepages;
  Uint32 cpageCount;
  Uint32 cnoOfAllocatedPages;
  Uint32 cnoOfAllocatedPagesMax;

  Page32_pool c_page_pool;
  Page8_pool c_page8_pool;
  bool c_allow_use_of_spare_pages;
/* --------------------------------------------------------------------------------- */
/* ROOTFRAGMENTREC                                                                   */
/*          DURING EXPAND FRAGMENT PROCESS, EACH FRAGMEND WILL BE EXPAND INTO TWO    */
/*          NEW FRAGMENTS.TO MAKE THIS PROCESS EASIER, DURING ADD FRAGMENT PROCESS   */
/*          NEXT FRAGMENT IDENTIIES WILL BE CALCULATED, AND TWO FRAGMENTS WILL BE    */
/*          ADDED IN (NDBACC). THEREBY EXPAND OF FRAGMENT CAN BE PERFORMED QUICK AND */
/*          EASY.THE NEW FRAGMENT ID SENDS TO TUP MANAGER FOR ALL OPERATION PROCESS. */
/* --------------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------------- */
/* SCAN_REC                                                                          */
/* --------------------------------------------------------------------------------- */
  ScanRec *scanRec;
  ScanRecPtr scanPtr;
  Uint32 cscanRecSize;
  Uint32 cfirstFreeScanRec;
/* --------------------------------------------------------------------------------- */
/* TABREC                                                                            */
/* --------------------------------------------------------------------------------- */
  Tabrec *tabrec;
  TabrecPtr tabptr;
  Uint32 ctablesize;
};

#ifdef DBACC_C

/**
 * Container short index is a third(!) numbering of containers on a Page8.
 *
 * pointer - is the container headers offset within the page.
 * index number with end indicator - index of buffer plus left or right.
 * short index - enumerates the containers with increasing pointer.
 *
 * Below formulas for valid values.
 * 32 is ZHEAD_SIZE the words in beginning of page reserved for page header.
 * 28 is ZBUF_SIZE buffer size, container grows either from left or right
 * end of buffer.
 * The left end header is on offset 0 in a buffer, the right end at offset 26,
 * since container header is 2 word big.
 * There are 72 container buffers on a page.
 *
 * Valid values for left containers are:
 * pointer: 32 + 28 * i
 * index number: i (end == left)
 * short index: 1 + 2 * i
 *
 * Valid values for right containers are:
 * pointer: 32 + 28 * i + 26
 * index number: i (end == right)
 * short index: 2 + 2 * i
 *
 * index number, i, goes from 0 to 71
 * short index, 0 means no container, valid values for container are 1 - 144
 *
 */

/**
 * getContainerShortIndex converts container pointer (p) to short index (s).
 *
 * short index = floor((page offset - page header size) / half-buf-size) + 1
 *
 * For left end containers odd numbers from 1 to 143 will be used
 * short index = floor((pointer - 32)/14) + 1 =
 *             = floor((32 + 28 * i - 32)/14) + 1 =
 *             = 2 * i + 1
 *
 * For right end containers even numbers from 2 to 144 will be used
 * short index = floor((pointer - 32)/14) + 1 =
 *             = floor((32 + 28 * i + 26 - 32)/14) + 1 =
 *             = 2 * i + floor(26/14) + 1 = 2 * i + 2
 *
 * In the implementation the +1 at the end are moved in to the dividend so
 * that only one addition and one division is needed.
 */

inline Uint8 Dbacc::Page8::getContainerShortIndex(Uint32 pointer) const
{
  return ((pointer - ZHEAD_SIZE) + (ZBUF_SIZE / 2)) / (ZBUF_SIZE / 2);
}

inline void Dbacc::Page8::setScanContainer(Uint16 scanbit, Uint32 conptr)
{
  assert(scanbit != 0);
  assert(scanbit < (1U << MAX_PARALLEL_SCANS_PER_FRAG));
  Uint8* p = reinterpret_cast<Uint8*>(&word32[SCAN_CON_0_3]);
  int i = BitmaskImpl::ffs(scanbit);
  assert(p[i] == 0);
  p[i] = getContainerShortIndex(conptr);
}

#ifdef NDEBUG
inline void Dbacc::Page8::clearScanContainer(Uint16 scanbit, Uint32)
#else
inline void Dbacc::Page8::clearScanContainer(Uint16 scanbit, Uint32 conptr)
#endif
{
  assert(scanbit != 0);
  assert(scanbit < (1U << MAX_PARALLEL_SCANS_PER_FRAG));
  Uint8* p = reinterpret_cast<Uint8*>(&word32[SCAN_CON_0_3]);
  int i = BitmaskImpl::ffs(scanbit);
  assert(p[i] == getContainerShortIndex(conptr));
  p[i] = 0;
}

inline bool Dbacc::Page8::checkScanContainer(Uint32 conptr) const
{
  const Uint8* p = reinterpret_cast<const Uint8*>(&word32[SCAN_CON_0_3]);
  return memchr(p, getContainerShortIndex(conptr), MAX_PARALLEL_SCANS_PER_FRAG);
}

inline Uint16 Dbacc::Page8::checkScans(Uint16 scanmask, Uint32 conptr) const
{
  const Uint8* p = reinterpret_cast<const Uint8*>(&word32[SCAN_CON_0_3]);
  Uint16 scanbit = 1U;
  Uint8 i = getContainerShortIndex(conptr);
  for(int j = 0; scanbit <= scanmask; ++j, scanbit <<= 1U)
  {
    if((scanbit & scanmask) && p[j] != i)
    {
      scanmask &= ~scanbit;
    }
  }
  return scanmask;
}

inline Uint32 Dbacc::Fragmentrec::getPageNumber(Uint32 bucket_number) const
{
  assert(bucket_number < RNIL);
  return bucket_number >> k;
}

inline Uint32 Dbacc::Fragmentrec::getPageIndex(Uint32 bucket_number) const
{
  assert(bucket_number < RNIL);
  return bucket_number & ((1 << k) - 1);
}

inline bool Dbacc::Fragmentrec::enough_valid_bits(LHBits16 const& reduced_hash_value) const
{
  // Forte C 5.0 needs use of intermediate constant
  int const bits = MIN_HASH_COMPARE_BITS;
  return level.getNeededValidBits(bits) <= reduced_hash_value.valid_bits();
}

inline void Dbacc::ScanRec::initContainer()
{
  inPageI = RNIL;
  inConptr = 0;
  elemScanned = 0;
}

inline bool Dbacc::ScanRec::isInContainer() const
{
  if (inPageI == RNIL)
  {
    assert(inConptr == 0);
    assert(elemScanned == 0);
    return false;
  }
  else
  {
    assert(inConptr != 0);
    return true;
  }
}

inline bool Dbacc::ScanRec::getContainer(Uint32& pagei, Uint32& conptr) const
{
  if (inPageI == RNIL)
  {
    assert(inConptr == 0);
    assert(elemScanned == 0);
    return false;
  }
  else
  {
    assert(inConptr!=0);
    pagei = inPageI;
    conptr = inConptr;
    return true;
  }
}

inline void Dbacc::ScanRec::enterContainer(Uint32 pagei, Uint32 conptr)
{
  assert(elemScanned == 0);
  assert(inPageI == RNIL);
  assert(inConptr == 0);
  inPageI = pagei;
  inConptr = conptr;
}

inline void Dbacc::ScanRec::leaveContainer(Uint32 pagei, Uint32 conptr)
{
  assert(inPageI == pagei);
  assert(inConptr == conptr);
  inPageI = RNIL;
  inConptr = 0;
  elemScanned = 0;
}

inline bool Dbacc::ScanRec::isScanned(Uint32 elemptr) const
{
  /**
   * Since element pointers within a container can not differ with more than
   * the buffer size (ZBUF_SIZE) we can use the pointer value modulo the
   * number of available bits in elemScanned to get an unique bit index for
   * each element.
   */
  NDB_STATIC_ASSERT(ZBUF_SIZE <= ELEM_SCANNED_BITS);
  return (elemScanned >> (elemptr % ELEM_SCANNED_BITS)) & 1;
}

inline void Dbacc::ScanRec::setScanned(Uint32 elemptr)
{
  assert(((elemScanned >> (elemptr % ELEM_SCANNED_BITS)) & 1) == 0);
  elemScanned |= (1 << (elemptr % ELEM_SCANNED_BITS));
}

inline void Dbacc::ScanRec::clearScanned(Uint32 elemptr)
{
  assert(((elemScanned >> (elemptr % ELEM_SCANNED_BITS)) & 1) == 1);
  elemScanned &= ~(1 << (elemptr % ELEM_SCANNED_BITS));
}

/**
 * moveScanBit are used when one moves an element within a container.
 *
 * This is done on delete there it can happen that the last element
 * in container is moved into the deleted elements place, this method
 * moves the elements scan bit accordingly.
 *
 * In case it is the last element in container that is deleted the
 * toptr and fromptr will be same, in that case the elements scan bit
 * must be cleared.
 */
inline void Dbacc::ScanRec::moveScanBit(Uint32 toptr, Uint32 fromptr)
{
  if (likely(toptr != fromptr))
  {
    /**
     * Move last elements scan bit to deleted elements place.
     * The scan bit at last elements place are cleared.
     */
    elemScanned = (elemScanned &
                   ~((1 << (toptr % ELEM_SCANNED_BITS)) |
                     (1 << (fromptr % ELEM_SCANNED_BITS)))) |
                  (isScanned(fromptr) << (toptr % ELEM_SCANNED_BITS));
  }
  else
  {
    /**
     * Clear the deleted elements scan bit since it is the last element
     * that is deleted.
     */
    elemScanned = (elemScanned &
                   ~(1 << (toptr % ELEM_SCANNED_BITS)));
  }
}

inline void Dbacc::Page8_pool::getPtr(Ptr<Page8>& page) const
{
  require(page.i != RNIL);
  Page32Ptr ptr;
  ptr.i = page.i >> 2;
  m_page_pool.getPtr(ptr);
  page.p = &ptr.p->page8[page.i & 3];
}

inline void Dbacc::Page8_pool::getPtrForce(Ptr<Page8>& page) const
{
  if (page.i == RNIL)
  {
    page.p = NULL;
    return;
  }
  Page32Ptr ptr;
  ptr.i = page.i >> 2;
  m_page_pool.getPtr(ptr);
  page.p = &ptr.p->page8[page.i & 3];
}

inline Uint32 Dbacc::getForwardContainerPtr(Uint32 index) const
{
  ndbassert(index <= Container::MAX_CONTAINER_INDEX);
  return ZHEAD_SIZE + index * Container::CONTAINER_SIZE;
}

inline Uint32 Dbacc::getBackwardContainerPtr(Uint32 index) const
{
  ndbassert(index <= Container::MAX_CONTAINER_INDEX);
  return ZHEAD_SIZE + index * Container::CONTAINER_SIZE +
         Container::CONTAINER_SIZE - Container::HEADER_SIZE;
}

inline void Dbacc::getContainerIndex(const Uint32 pointer,
                                     Uint32& index,
                                     bool& isforward) const
{
  index = (pointer - ZHEAD_SIZE) / ZBUF_SIZE;
  /**
   * All forward container pointers are distanced with a multiple of
   * ZBUF_SIZE to the first forward containers pointer (ZHEAD_SIZE).
   */
  isforward = (pointer % ZBUF_SIZE) == (ZHEAD_SIZE % ZBUF_SIZE);
}

inline Uint32 Dbacc::getContainerPtr(Uint32 index, bool isforward) const
{
  if (isforward)
  {
    return getForwardContainerPtr(index);
  }
  else
  {
    return getBackwardContainerPtr(index);
  }
}

/**
 * Implementation of Dbacc::Page32Lists
 */

inline Dbacc::Page32Lists::Page32Lists()
: nonempty_lists(0)
{
  for (unsigned i = 0; i < NDB_ARRAY_SIZE(lists); i++)
  {
    lists[i].init();
  }
  for (unsigned i = 0; i < NDB_ARRAY_SIZE(sub_page_id_count); i++)
  {
    sub_page_id_count[i] = 0;
  }
}

/**
 * The Dbacc 32KiB pages are arranged in 16 lists depending on which 8KiB
 * pages are in in use on 32KiB page.
 *
 * list#0 - no 8KiB page is in use.
 *        - all sub pages are free.
 *
 * list#1-#4 - one 8KiB page is in use (sub page id 0 - sub page id 3)
 * list#1 - sub page 0, 1, 2, are free.
 * list#2 - sub page 0, 1, 3, are free.
 * list#3 - sub page 0, 2, 3, are free.
 * list#4 - sub page 1, 2, 3, are free.
 *
 * list#5-#10 - two 8KiB pages are in use.
 * list#5  - sub page 0, 1, are free.
 * list#6  - sub page 0, 2, are free.
 * list#7  - sub page 0, 3, are free.
 * list#8  - sub page 1, 2, are free.
 * list#9  - sub page 1, 3, are free.
 * list#10 - sub page 2, 3, are free.
 *
 * list#11-14 - three 8KiB pages are in use.
 * list#11 - sub page 0 is free
 * list#12 - sub page 1 is free
 * list#13 - sub page 2 is free
 * list#14 - sub page 3 is free
 *
 * list#15 - all four 8KiB pages are in use.
 *         - no sub page is free.
 *
 * In list_id_set a set bit indicates that the corresponding list is
 * included.
 *
 * List with fewer 8KiB pages free than an other list have higher id.
 */

/**
 * sub_page_id_to_list_id
 *
 * Find lists of 32KiB pages with requested 8KiB page free, or if
 * ANY_SUB_PAGE are passed all lists with at least one 8KiB page free.
 *
 * @param[in] sub_page_id Index (0-3) of 8KiB page, or ANY_SUB_PAGE.
 *
 * @returns A bitmask with one bit set for each matching list.
 *          For list numbering see comment above.
 */
inline Uint16 Dbacc::Page32Lists::sub_page_id_to_list_id_set(int sub_page_id)
{
  switch (sub_page_id)
  {
  case ANY_SUB_PAGE: /* lists of 32KiB pages with at least one free 8KiB page */
    return 0x7fff;
  case 0: /* lists of 32KiB pages with 8KiB page with sub-id 0 free */
    return 0x08ef; // 0b0'0001'000111'0111'1
  case 1: /* lists of 32KiB pages with 8KiB page with sub-id 1 free */
    return 0x1337; // 0b0'0010'011001'1011'1
  case 2: /* lists of 32KiB pages with 8KiB page with sub-id 2 free */
    return 0x255b; // 0b0'0100'101010'1101'1
  case 3: /* lists of 32KiB pages with 8KiB page with sub-id 3 free */
    return 0x469d; // 0b0'1000'110100'1110'1
  }
  require(false);
  return 0;
}

/**
 * least_free_list
 *
 * Return one of the lists of 32KiB pages that have least number of 8KiB
 * pages free.
 *
 * Note that the list numbering is such (see comment above) that a list
 * with fewer free 8KiB pages have a higher id number than one with more
 * free 8KiB pages.
 *
 * @param[in] list_id_set A bitmask with one bit set for each list to
 *                        consider.
 *                        Note that at least one list must be given.
 *
 * @returns A list id (0-15).
 */
inline Uint8 Dbacc::Page32Lists::least_free_list(Uint16 list_id_set)
{
  require(list_id_set != 0);
  return BitmaskImpl::fls(list_id_set);
}

/**
 * list_id_to_sub_page_id_set
 *
 * Return the 8KiB sub pages that are free for 32KiB pages in a given
 * list.
 *
 * @param[in] list_id
 *
 * @returns A bitmask of four bits, with bit set for 8KiB page free.
 */
inline Uint8 Dbacc::Page32Lists::list_id_to_sub_page_id_set(int list_id)
{
  require(0 <= list_id && list_id <= 15);
  /**
   * The 64 bit word below should be viewed as an array of 16 entries
   * with 4 bits each.
   *
   * Index is the list_id, and a set bit in the 4 bits indicates that
   * corresponding 8KiB page is free.
   *
   * What 8KiB page that are free for pages in the different lists is
   * described in comment above.
   *
   * Example, list#0 have all 8KiB pages free so all 4 bits set, and
   * accordingly the least four bits in lid_to_pidset is set, in hex 0xf.
   */
  const Uint64 lid_to_pidset = 0x08421ca6953edb7fULL;
  return (lid_to_pidset >> (list_id * 4)) & 0xf;
}

/**
 * sub_page_id_set_to_list_id
 *
 * Get the list id for a page with a specific pattern of 8KiB sub pages
 * free.
 *
 * @param[in] sub_page_id_set A four bit bitmask, a bit is set for sub
 *                            page requested to be free.
 *
 * @returns A list id (0-15).
 */
inline Uint8 Dbacc::Page32Lists::sub_page_id_set_to_list_id(int sub_page_id_set)
{
  require(0 <= sub_page_id_set && sub_page_id_set <= 15);
  /**
   * The 64bit value below should be viewed as an array of 16 entries
   * with a 4 bit unsigned list id.
   *
   * There are 16 combinations of free sub pages, use the 4bit bitmask of
   * sub pages as an 4 bit unsigned int as index into the "array".
   *
   * The list numbering is described in comment above.
   */
  const Uint64 pidset_to_lid = 0x043a297e186d5cbfULL; // sub-page-id-set -> list-id
  return (pidset_to_lid >> (sub_page_id_set * 4)) & 0xf;
}

inline Uint32 Dbacc::Page32Lists::getCount() const
{
  Uint32 sum = 0;
  for (unsigned i = 0; i < NDB_ARRAY_SIZE(sub_page_id_count); i++)
    sum += sub_page_id_count[i];
  return sum;
}

inline bool Dbacc::Page32Lists::haveFreePage8(int sub_page_id) const
{
  Uint16 list_id_set = sub_page_id_to_list_id_set(sub_page_id);
  return (list_id_set & nonempty_lists) != 0;
}

#endif

#endif
#undef JAM_FILE_ID

