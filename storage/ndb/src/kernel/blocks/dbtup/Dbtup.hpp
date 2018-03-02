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

#ifndef DBTUP_H
#define DBTUP_H

#include <pc.hpp>
#include <SimulatedBlock.hpp>
#include <ndb_limits.h>
#include <trigger_definitions.h>
#include <AttributeHeader.hpp>
#include <Bitmask.hpp>
#include <signaldata/TupKey.hpp>
#include <signaldata/CreateTrig.hpp>
#include <signaldata/CreateTrigImpl.hpp>
#include <signaldata/DropTrig.hpp>
#include <signaldata/DropTrigImpl.hpp>
#include <signaldata/TrigAttrInfo.hpp>
#include <signaldata/BuildIndxImpl.hpp>
#include <signaldata/AlterTab.hpp>
#include <AttributeDescriptor.hpp>
#include "AttributeOffset.hpp"
#include "Undo_buffer.hpp"
#include "tuppage.hpp"
#include <DynArr256.hpp>
#include "../pgman.hpp"
#include "../tsman.hpp"
#include <EventLogger.hpp>
#include "../backup/BackupFormat.hpp"

#define JAM_FILE_ID 414


extern EventLogger* g_eventLogger;

#ifdef VM_TRACE
inline const char* dbgmask(const Bitmask<MAXNROFATTRIBUTESINWORDS>& bm) {
  static int i=0; static char buf[5][200];
  bm.getText(buf[i%5]); return buf[i++%5]; }
inline const char* dbgmask(const Uint32 bm[2]) {
  static int i=0; static char buf[5][200];
  sprintf(buf[i%5],"%08x%08x",bm[1],bm[0]); return buf[i++%5]; }
#endif

#define ZWORDS_ON_PAGE 8192          /* NUMBER OF WORDS ON A PAGE.      */
#define ZMIN_PAGE_LIMIT_TUPKEYREQ 5
#define ZTUP_VERSION_BITS 15
#define ZTUP_VERSION_MASK ((1 << ZTUP_VERSION_BITS) - 1)
#define MAX_FREE_LIST 4

inline Uint32* ALIGN_WORD(void * ptr)
{
  return (Uint32*)(((UintPtr(ptr) + 3) >> 2) << 2);
}

inline const Uint32* ALIGN_WORD(const void* ptr)
{
  return (Uint32*)(((UintPtr(ptr) + 3) >> 2) << 2);
}

#ifdef DBTUP_C

/*
2.2 LOCAL SYMBOLS
-----------------
*/
/* ---------------------------------------------------------------- */
/*       S I Z E              O F               R E C O R D S       */
/* ---------------------------------------------------------------- */
#define ZNO_OF_CONCURRENT_OPEN_OP 40        /* NUMBER OF CONCURRENT OPENS      */
#define ZNO_OF_CONCURRENT_WRITE_OP 80       /* NUMBER OF CONCURRENT DISK WRITES*/
#define ZNO_OF_FRAGOPREC 20                 /* NUMBER OF CONCURRENT ADD FRAG.  */
#define TOT_PAGE_RECORD_SPACE 262144        /* SIZE OF PAGE RECORD FILE.       */
#define ZNO_OF_PAGE TOT_PAGE_RECORD_SPACE/ZWORDS_ON_PAGE   
#define ZNO_OF_PAGE_RANGE_REC 128           /* SIZE OF PAGE RANGE FILE         */
// Trigger constants
#define ZDEFAULT_MAX_NO_TRIGGERS_PER_TABLE 16

/* ---------------------------------------------------------------- */
/* A ATTRIBUTE MAY BE NULL, DYNAMIC OR NORMAL. A NORMAL ATTRIBUTE   */
/* IS A ATTRIBUTE THAT IS NOT NULL OR DYNAMIC. A NULL ATTRIBUTE     */
/* MAY HAVE NO VALUE. A DYNAMIC ATTRIBUTE IS A NULL ATTRIBUTE THAT  */
/* DOES NOT HAVE TO BE A MEMBER OF EVERY TUPLE I A CERTAIN TABLE.   */
/* ---------------------------------------------------------------- */
/**
 * #defines moved into include/kernel/Interpreter.hpp
 */
#define ZINSERT_DELETE 0
#define ZUPDATE_ALL 8
/* ---------------------------------------------------------------- */
/* THE MINIMUM SIZE OF AN 'EMPTY' TUPLE HEADER IN R-WORDS           */
/* ---------------------------------------------------------------- */
          /* THE TUPLE HEADER FIELD 'SIZE OF NULL ATTR. FIELD' SPECIFYES    */
          /* THE SIZE OF THE TUPLE HEADER FIELD 'NULL ATTR. FIELD'.         */
          /* THE TUPLE HEADER FIELD 'TYPE' SPECIFYES THE TYPE OF THE TUPLE  */
          /* HEADER.                                                        */
                               /* TUPLE ATTRIBUTE INDEX CLUSTERS, ATTRIBUTE */
                               /* CLUSTERS AND A DYNAMIC ATTRIBUTE HEADER.  */
                               /* IT MAY ALSO CONTAIN SHORT ATTRIBUTES AND  */
                               /* POINTERS TO LONG ATTRIBUTE HEADERS.       */
                               /* TUPLE ATTRIBUTE INDEX CLUSTERS, ATTRIBUTE */
                               /* CLUSTERS AND A DYNAMIC ATTRIBUTE HEADER.  */

          /* DATA STRUCTURE TYPES */
          /* WHEN ATTRIBUTE INFO IS SENT WITH A ATTRINFO-SIGNAL THE         */
          /* VARIABLE TYPE IS SPECIFYED. THIS MUST BE DONE TO BE ABLE TO    */
          /* NOW HOW MUCH DATA OF A ATTRIBUTE TO READ FROM ATTRINFO.        */

          /* WHEN A REQUEST CAN NOT BE EXECUTED BECAUSE OF A ERROR THE      */
          /* ERROR MUST BE IDENTIFYED BY MEANS OF A ERROR CODE AND SENT TO  */
          /* THE REQUESTER.                                                 */
#define ZGET_OPREC_ERROR 804            // TUP_SEIZEREF

#define ZEXIST_FRAG_ERROR 816           // Add fragment
#define ZFULL_FRAGRECORD_ERROR 817      // Add fragment
#define ZNO_FREE_PAGE_RANGE_ERROR 818   // Add fragment
#define ZNOFREE_FRAGOP_ERROR 830        // Add fragment
#define ZTOO_LARGE_TUPLE_ERROR 851      // Add fragment
#define ZNO_FREE_TAB_ENTRY_ERROR 852    // Add fragment
#define ZNO_PAGES_ALLOCATED_ERROR 881   // Add fragment

#define ZGET_REALPID_ERROR 809
#define ZNOT_IMPLEMENTED_ERROR 812
#define ZSEIZE_ATTRINBUFREC_ERROR 805
#define ZTOO_MUCH_ATTRINFO_ERROR 823
#define ZMEM_NOTABDESCR_ERROR 826
#define ZMEM_NOMEM_ERROR 827
#define ZAI_INCONSISTENCY_ERROR 829
#define ZNO_ILLEGAL_NULL_ATTR 839
#define ZNOT_NULL_ATTR 840
#define ZBAD_DEFAULT_VALUE_LEN 850
#define ZNO_INSTRUCTION_ERROR 871
#define ZOUTSIDE_OF_PROGRAM_ERROR 876
#define ZSTORED_PROC_ID_ERROR 877
#define ZREGISTER_INIT_ERROR 878
#define ZATTRIBUTE_ID_ERROR 879
#define ZTRY_TO_READ_TOO_MUCH_ERROR 880
#define ZTOTAL_LEN_ERROR 882
#define ZATTR_INTERPRETER_ERROR 883
#define ZSTACK_OVERFLOW_ERROR 884
#define ZSTACK_UNDERFLOW_ERROR 885
#define ZTOO_MANY_INSTRUCTIONS_ERROR 886
#define ZTRY_TO_UPDATE_ERROR 888
#define ZCALL_ERROR 890
#define ZTEMPORARY_RESOURCE_FAILURE 891
#define ZUNSUPPORTED_BRANCH 892

#define ZSTORED_SEIZE_ATTRINBUFREC_ERROR 873 // Part of Scan
#define ZSTORED_TOO_MUCH_ATTRINFO_ERROR 874

#define ZREAD_ONLY_CONSTRAINT_VIOLATION 893
#define ZVAR_SIZED_NOT_SUPPORTED 894
#define ZINCONSISTENT_NULL_ATTRIBUTE_COUNT 895
#define ZTUPLE_CORRUPTED_ERROR 896
#define ZTRY_UPDATE_PRIMARY_KEY 897
#define ZMUST_BE_ABORTED_ERROR 898
#define ZTUPLE_DELETED_ERROR 626
#define ZINSERT_ERROR 630
#define ZOP_AFTER_REFRESH_ERROR 920
#define ZNO_COPY_TUPLE_MEMORY_ERROR 921

#define ZINVALID_CHAR_FORMAT 744
#define ZROWID_ALLOCATED 899
#define ZINVALID_ALTER_TAB 741

#define ZTOO_MANY_BITS_ERROR 791

          /* SOME WORD POSITIONS OF FIELDS IN SOME HEADERS */

#define ZTH_MM_FREE 3                     /* PAGE STATE, TUPLE HEADER PAGE WITH FREE AREA      */
#define ZTH_MM_FULL 4                     /* PAGE STATE, TUPLE HEADER PAGE WHICH IS FULL       */

#define ZTD_HEADER 0                      /* HEADER POSITION                   */
#define ZTD_DATASIZE 1                    /* SIZE OF THE DATA IN THIS CHUNK    */
#define ZTD_SIZE 2                        /* TOTAL SIZE OF TABLE DESCRIPTOR    */

          /* TRAILER POSITIONS FROM END OF TABLE DESCRIPTOR RECORD               */
#define ZTD_TR_SIZE 1                     /* SIZE DESCRIPTOR POS FROM END+1    */
#define ZTD_TR_TYPE 2
#define ZTD_TRAILER_SIZE 2                /* TOTAL SIZE OF TABLE TRAILER       */
#define ZAD_SIZE 2                        /* TOTAL SIZE OF ATTR DESCRIPTOR     */
#define ZAD_LOG_SIZE 1                    /* TWO LOG OF TOTAL SIZE OF ATTR DESCRIPTOR     */

          /* CONSTANTS USED TO HANDLE TABLE DESCRIPTOR AS A FREELIST             */
#define ZTD_FL_HEADER 0                   /* HEADER POSITION                   */
#define ZTD_FL_SIZE 1                     /* TOTAL SIZE OF THIS FREELIST ENTRY */
#define ZTD_FL_PREV 2                     /* PREVIOUS RECORD IN FREELIST       */
#define ZTD_FL_NEXT 3                     /* NEXT RECORD IN FREELIST           */
#define ZTD_FREE_SIZE 16                  /* SIZE NEEDED TO HOLD ONE FL ENTRY  */

          /* CONSTANTS USED IN LSB OF TABLE DESCRIPTOR HEADER DESCRIBING USAGE   */
#define ZTD_TYPE_FREE 0                   /* RECORD LINKED INTO FREELIST       */
#define ZTD_TYPE_NORMAL 1                 /* RECORD USED AS TABLE DESCRIPTOR   */
          /* ATTRIBUTE OPERATION CONSTANTS */
#define ZLEAF 1
#define ZNON_LEAF 2

          /* RETURN POINTS. */
          /* RESTART PHASES */
#define ZSTARTPHASE1 1
#define ZSTARTPHASE2 2
#define ZSTARTPHASE3 3
#define ZSTARTPHASE4 4
#define ZSTARTPHASE6 6

#define ZADDFRAG 0

//------------------------------------------------------------
// TUP_CONTINUEB codes
//------------------------------------------------------------
#define ZINITIALISE_RECORDS 6
#define ZREL_FRAG 7
#define ZREPORT_MEMORY_USAGE 8
#define ZBUILD_INDEX 9
#define ZTUP_SCAN 10
#define ZFREE_EXTENT 11
#define ZUNMAP_PAGES 12
#define ZFREE_VAR_PAGES 13
#define ZFREE_PAGES 14
#define ZREBUILD_FREE_PAGE_LIST 15
#define ZDISK_RESTART_UNDO 16

#define ZSCAN_PROCEDURE 0
#define ZCOPY_PROCEDURE 2
#define ZSTORED_PROCEDURE_DELETE 3
#define ZSTORED_PROCEDURE_FREE 0xffff
#define ZMIN_PAGE_LIMIT_TUP_COMMITREQ 2

#define ZSKIP_TUX_TRIGGERS 0x1 // flag for TUP_ABORTREQ
#define ZABORT_DEALLOC     0x2 // flag for TUP_ABORTREQ

#endif

class Dbtup: public SimulatedBlock {
friend class DbtupProxy;
friend class Suma;
public:
struct KeyReqStruct;
friend struct KeyReqStruct; // CC
typedef bool (Dbtup::* ReadFunction)(Uint8*,
                                     KeyReqStruct*,
                                     AttributeHeader*,
                                     Uint32);
typedef bool (Dbtup::* UpdateFunction)(Uint32*,
                                       KeyReqStruct*,
                                       Uint32);
  void prepare_scan_ctx(Uint32 scanPtrI);
private:
  
  typedef Tup_fixsize_page Fix_page;
  typedef Tup_varsize_page Var_page;

public:
  class Dblqh *c_lqh;
  class Backup *c_backup;
  Tsman* c_tsman;
  Lgman* c_lgman;
  Pgman* c_pgman;

  enum CallbackIndex {
    // lgman
    DROP_TABLE_LOG_BUFFER_CALLBACK = 1,
    DROP_FRAGMENT_FREE_EXTENT_LOG_BUFFER_CALLBACK = 2,
    NR_DELETE_LOG_BUFFER_CALLBACK = 3,
    DISK_PAGE_LOG_BUFFER_CALLBACK = 4,
    COUNT_CALLBACKS = 5
  };
  CallbackEntry m_callbackEntry[COUNT_CALLBACKS];
  CallbackTable m_callbackTable;

enum TransState {
  TRANS_IDLE = 0,
  TRANS_STARTED = 1,
  TRANS_NOT_USED_STATE = 2, // No longer used.
  TRANS_ERROR_WAIT_STORED_PROCREQ = 3,
  TRANS_ERROR_WAIT_TUPKEYREQ = 4,
  TRANS_TOO_MUCH_AI = 5,
  TRANS_DISCONNECTED = 6
};

enum TupleState {
  TUPLE_PREPARED = 1,
  TUPLE_ALREADY_ABORTED = 2,
  TUPLE_TO_BE_COMMITTED = 3
};
  
enum State {
  NOT_INITIALIZED = 0,
  IDLE = 17,
  ACTIVE = 18,
  SYSTEM_RESTART = 19,
  DEFINED = 34,
  NOT_DEFINED = 37,
  NORMAL_PAGE = 40,
  DEFINING = 65,
  DROPPING = 68
};


struct Fragoperrec {
  Uint64 minRows;
  Uint64 maxRows;
  Uint32 nextFragoprec;
  Uint32 lqhPtrFrag;
  Uint32 fragidFrag;
  Uint32 tableidFrag;
  Uint32 fragPointer;
  Uint32 attributeCount;
  Uint32 charsetIndex;
  Uint32 m_null_bits[2];
  Uint32 m_extra_row_gci_bits;
  Uint32 m_extra_row_author_bits;
  union {
    BlockReference lqhBlockrefFrag;
    Uint32 m_senderRef;
  };
  Uint32 m_senderData;
  Uint32 m_restoredLcpId;
  Uint32 m_restoredLocalLcpId;
  Uint32 m_maxGciCompleted;
  bool inUse;
  bool definingFragment;
};
typedef Ptr<Fragoperrec> FragoperrecPtr;

  /* Operation record used during alter table. */
  struct AlterTabOperation {
    AlterTabOperation() { memset(this, 0, sizeof(AlterTabOperation)); };
    Uint32 nextAlterTabOp;
    Uint32 newNoOfAttrs;
    Uint32 newNoOfCharsets;
    Uint32 newNoOfKeyAttrs;
    Uint32 noOfDynNullBits;
    Uint32 noOfDynVar;
    Uint32 noOfDynFix;
    Uint32 noOfDynamic;
    Uint32 tabDesOffset[7];
    Uint32 tableDescriptor;
    Uint32 dynTabDesOffset[3];
    Uint32 dynTableDescriptor;
  };
  typedef Ptr<AlterTabOperation> AlterTabOperationPtr;

  typedef Tup_page Page;
  typedef Ptr<Page> PagePtr;
  typedef ArrayPool<Page> Page_pool;
  typedef DLList<Page_pool> Page_list;
  typedef LocalDLList<Page_pool> Local_Page_list;
  typedef DLFifoList<Page_pool> Page_fifo;
  typedef LocalDLFifoList<Page_pool> Local_Page_fifo;

  // Scan position
  struct ScanPos {
    enum Get {
      Get_undef = 0,
      Get_next_page,
      Get_page,
      Get_next_page_mm,
      Get_page_mm,
      Get_next_page_dd,
      Get_page_dd,
      Get_next_tuple,
      Get_tuple
    };
    Get m_get;                  // entry point in scanNext
    Local_key m_key;            // scan position pointer MM or DD
    Page* m_page;               // scanned MM or DD (cache) page
    Local_key m_key_mm;         // MM local key returned
    Uint32 m_realpid_mm;        // MM real page id
    Uint32 m_extent_info_ptr_i;
    Uint32 m_next_small_area_check_idx;
    Uint32 m_next_large_area_check_idx;
    bool m_all_rows;
    bool m_lcp_scan_changed_rows_page;
    bool m_is_last_lcp_state_D;
    ScanPos() {
      /*
       * Position is Null until scanFirst().  In particular in LCP scan
       * it is Null between LCP_FRAG_ORD and ACC_SCANREQ.
       */
      m_key.setNull();
    }
  };

  // Scan Lock
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

  ScanLock_pool c_scanLockPool;

  // Tup scan, similar to Tux scan.  Later some of this could
  // be moved to common superclass.
  struct ScanOp {
    ScanOp() :
      m_state(Undef),
      m_bits(0),
      m_last_seen(0),
      m_userPtr(RNIL),
      m_userRef(RNIL),
      m_tableId(RNIL),
      m_fragId(~(Uint32)0),
      m_fragPtrI(RNIL),
      m_transId1(0),
      m_transId2(0),
      m_savePointId(0),
      m_accLockOp(RNIL)
    {}

    enum State {
      Undef = 0,
      First = 1,                // before first entry
      Current = 2,              // at current before locking
      Blocked = 3,              // at current waiting for ACC lock
      Locked = 4,               // at current and locked or no lock needed
      Next = 5,                 // looking for next extry
      Last = 6,                 // after last entry
      Aborting = 7,             // lock wait at scan close
      Invalid = 9               // cannot return REF to LQH currently
    };
    Uint16 m_state;

    enum Bits {
      SCAN_DD        = 0x01,        // scan disk pages
      SCAN_VS        = 0x02,        // page format is var size
      SCAN_LCP       = 0x04,        // LCP mem page scan
      SCAN_LOCK_SH   = 0x10,        // lock mode shared
      SCAN_LOCK_EX   = 0x20,        // lock mode exclusive
      SCAN_LOCK_WAIT = 0x40,        // lock wait
      // any lock mode
      SCAN_LOCK      = SCAN_LOCK_SH | SCAN_LOCK_EX,
      SCAN_NR        = 0x80        // Node recovery scan
    };
    Uint16 m_bits;
    Uint16 m_last_seen;
    
    Uint32 m_userPtr;           // scanptr.i in LQH
    Uint32 m_userRef;
    Uint32 m_tableId;
    Uint32 m_fragId;
    Uint32 m_fragPtrI;
    Uint32 m_transId1;
    Uint32 m_transId2;
    union {
      Uint32 m_savePointId;
      Uint32 m_scanGCI;
    };
    Uint32 m_endPage;
    // lock waited for or obtained and not yet passed to LQH
    Uint32 m_accLockOp;

    ScanPos m_scanPos;

    ScanLock_fifo::Head m_accLockOps;

    union {
    Uint32 nextPool;
    Uint32 nextList;
    };
    Uint32 prevList;
  };
  typedef Ptr<ScanOp> ScanOpPtr;
  typedef ArrayPool<ScanOp> ScanOp_pool;
  typedef DLList<ScanOp_pool> ScanOp_list;
  typedef LocalDLList<ScanOp_pool> Local_ScanOp_list;
  ScanOp_pool c_scanOpPool;

  void scanReply(Signal*, ScanOpPtr scanPtr);
  void scanFirst(Signal*, ScanOpPtr scanPtr);
  bool scanNext(Signal*, ScanOpPtr scanPtr);
  void scanCont(Signal*, ScanOpPtr scanPtr);
  void disk_page_tup_scan_callback(Signal*, Uint32 scanPtrI, Uint32 page_i);
  void scanClose(Signal*, ScanOpPtr scanPtr);
  void addAccLockOp(ScanOp& scan, Uint32 accLockOp);
  void removeAccLockOp(ScanOp& scan, Uint32 accLockOp);
  void releaseScanOp(ScanOpPtr& scanPtr);

  struct Tuple_header;

  Uint32 prepare_lcp_scan_page(ScanOp& scan,
                               Local_key& key,
                               Uint32 *next_ptr,
                               Uint32 *prev_ptr);
  Uint32 handle_lcp_skip_page(ScanOp& scan,
                              Local_key key,
                              Page *page);
  Uint32 handle_scan_change_page_rows(ScanOp& scan,
                                      Fix_page *fix_page,
                                      Tuple_header* tuple_header_ptr,
                                      Uint32 & foundGCI);
  Uint32 setup_change_page_for_scan(ScanOp& scan,
                                    Fix_page *fix_page,
                                    Local_key& key,
                                    Uint32 size);
  Uint32 move_to_next_change_page_row(ScanOp & scan,
                                      Fix_page *fix_page,
                                      Tuple_header **tuple_header_ptr,
                                      Uint32 & loop_count,
                                      Uint32 size);

  // for md5 of key (could maybe reuse existing temp buffer)
  Uint64 c_dataBuffer[ZWORDS_ON_PAGE/2 + 1];

  // Crash the node when a tuple got corrupted
  bool c_crashOnCorruptedTuple;

  struct Page_request 
  {
    Page_request() {}
    Local_key m_key;
    Uint32 m_frag_ptr_i;
    Uint32 m_extent_info_ptr;
    Uint16 m_original_estimated_free_space; // in bytes/records
    Uint16 m_list_index;                  // in Disk_alloc_info.m_page_requests
    Uint16 m_ref_count;                   // Waiters for page
    Uint16 m_uncommitted_used_space;
    Uint32 nextList;
    Uint32 prevList;
    Uint32 m_magic;
  }; // 32 bytes
  
  typedef RecordPool<WOPool<Page_request> > Page_request_pool;
  typedef DLFifoList<Page_request_pool> Page_request_list;
  typedef LocalDLFifoList<Page_request_pool> Local_page_request_list;

  STATIC_CONST( EXTENT_SEARCH_MATRIX_COLS = 4 ); // Guarantee size
  STATIC_CONST( EXTENT_SEARCH_MATRIX_ROWS = 5 ); // Total size
  STATIC_CONST( EXTENT_SEARCH_MATRIX_SIZE = 20 );
  
  struct Extent_list_t
  {
    Uint32 nextList;
  };

  struct Extent_info : public Extent_list_t
  {
    Uint32 m_magic;
    Uint32 m_first_page_no;
    Uint32 m_empty_page_no;
    Local_key m_key;
    Uint32 m_free_space;
    Uint32 m_free_matrix_pos;
    Uint16 m_free_page_count[EXTENT_SEARCH_MATRIX_COLS];
    union {
      Uint32 nextList;
      Uint32 nextPool;
    };
    Uint32 prevList;
    Uint32 nextHash, prevHash;

    Uint32 hashValue() const {
      return (m_key.m_file_no << 16) ^ m_key.m_page_idx;
    }

    Extent_info() {}
    bool equal(const Extent_info & rec) const {
      return m_key.m_file_no == rec.m_key.m_file_no &&
	m_key.m_page_idx == rec.m_key.m_page_idx;
    }
  }; // 40 bytes

  typedef RecordPool<RWPool<Extent_info> > Extent_info_pool;
  typedef DLList<Extent_info_pool> Extent_info_list;
  typedef LocalDLList<Extent_info_pool> Local_extent_info_list;
  typedef DLHashTable<Extent_info_pool> Extent_info_hash;
  typedef SLList<Extent_info_pool, Extent_list_t> Fragment_extent_list;
  typedef LocalSLList<Extent_info_pool, Extent_list_t> Local_fragment_extent_list;
  struct Tablerec;
  struct Disk_alloc_info 
  {
    Disk_alloc_info() {}
    Disk_alloc_info(const Tablerec* tabPtrP, 
		    Uint32 extent_size_in_pages);
    Uint32 m_extent_size;
    
    /**
     * Disk allocation
     *
     * 1) Allocate space on pages that already are dirty
     *    (4 free lists for different requests)
     * 2) Allocate space on pages waiting to be mapped that will be dirty
     *    (4 free lists for different requests)
     * 3) Check if "current" extent can accommodate request
     *    If so, allocate page from there
     *    Else put "current" into free matrix
     * 4) Search free matrix for extent with greatest amount of free space
     *    while still accommodating current request
     *    (20 free lists for different requests)
     */
    
    /**
     * Free list of pages in different size
     *   that are dirty
     */
    Page_list::Head m_dirty_pages[MAX_FREE_LIST];   // In real page id's

    /**
     * Requests (for update) that have sufficient space left after request
     *   these are currently being "mapped"
     */
    Page_request_list::Head m_page_requests[MAX_FREE_LIST];

    Page_list::Head m_unmap_pages;

    /**
     * Current extent
     */
    Uint32 m_curr_extent_info_ptr_i;
    
    /**
     * 
     */
    STATIC_CONST( SZ = EXTENT_SEARCH_MATRIX_SIZE );
    Extent_info_list::Head m_free_extents[SZ];
    Uint32 m_total_extent_free_space_thresholds[EXTENT_SEARCH_MATRIX_ROWS];
    Uint32 m_page_free_bits_map[EXTENT_SEARCH_MATRIX_COLS];

    Uint32 find_extent(Uint32 sz) const;
    Uint32 calc_extent_pos(const Extent_info*) const;

    /**
     * Compute minimum free space on page given bits
     */
    Uint32 calc_page_free_space(Uint32 bits) const {
      return m_page_free_bits_map[bits];
    }
  
    /**
     * Compute page free bits, given free space
     */
    Uint32 calc_page_free_bits(Uint32 free) const {
      for(Uint32 i = 0; i<EXTENT_SEARCH_MATRIX_COLS-1; i++)
	if(free >= m_page_free_bits_map[i])
	  return i;
      return EXTENT_SEARCH_MATRIX_COLS - 1;
    }

    Fragment_extent_list::Head m_extent_list;
  };
  
  void dump_disk_alloc(Disk_alloc_info&);

  STATIC_CONST( FREE_PAGE_BIT =   0x80000000 );
  STATIC_CONST( LCP_SCANNED_BIT = 0x40000000 );
  STATIC_CONST( LAST_LCP_FREE_BIT = 0x40000000 );
  STATIC_CONST( FREE_PAGE_RNIL =  0x3fffffff );
  STATIC_CONST( PAGE_BIT_MASK =   0x3fffffff );
  STATIC_CONST( MAX_PAGES_IN_DYN_ARRAY = (RNIL & PAGE_BIT_MASK));

struct Fragrecord {
  // Number of allocated pages for fixed-sized data.
  Uint32 noOfPages;
  // Number of allocated pages for var-sized data.
  Uint32 noOfVarPages;
  // No of allocated but unused words for var-sized fields.
  Uint64 m_varWordsFree;

  /**
   * m_max_page_cnt contains the next page number to use when allocating
   * a new page and all pages with lower page numbers are filled with
   * rows. At fragment creation it is 0 since no pages are yet allocated.
   * With 1 page allocated it is set to 1. The actual max page number with
   * 1 page is however 0 since we start with page numbers from 0.
   */
  Uint32 m_max_page_cnt;
  Uint32 m_free_page_id_list;
  DynArr256::Head m_page_map;
  Page_fifo::Head thFreeFirst;   // pages with atleast 1 free record

  Uint32 m_lcp_scan_op;
  Local_key m_lcp_keep_list_head;
  Local_key m_lcp_keep_list_tail;

  enum FragState
  { FS_FREE
    ,FS_ONLINE           // Ordinary fragment
    ,FS_REORG_NEW        // A new (not yet "online" fragment)
    ,FS_REORG_COMMIT     // An ordinary fragment which has been split
    ,FS_REORG_COMMIT_NEW // An new fragment which is online
    ,FS_REORG_COMPLETE     // An ordinary fragment which has been split
    ,FS_REORG_COMPLETE_NEW // An new fragment which is online
  } fragStatus;
  Uint32 fragTableId;
  Uint32 fragmentId;
  Uint32 partitionId;
  Uint32 nextfreefrag;
  // +1 is as "full" pages are stored last
  Page_list::Head free_var_page_array[MAX_FREE_LIST+1];
  
  ScanOp_list::Head m_scanList;

  enum
  {
    UC_LCP = 1,
    UC_CREATE = 2,
    UC_SET_LCP = 3,
    UC_DROP = 4
  };
  /* Calculated average row size of the rows in the fragment */
  Uint32 m_average_row_size;
  Uint32 m_restore_lcp_id;
  Uint32 m_restore_local_lcp_id;
  Uint32 m_undo_complete;
  Uint32 m_tablespace_id;
  Uint32 m_logfile_group_id;
  Disk_alloc_info m_disk_alloc_info;
  // GCI at time of start LCP (used to deduce if one should count row changes)
  Uint32 m_lcp_start_gci;
  // Number of changed rows since last LCP (approximative)
  Uint64 m_lcp_changed_rows;
  // Number of fixed-seize tuple parts (which equals the tuple count).
  Uint64 m_fixedElemCount;
  Uint64 m_row_count;
  Uint64 m_prev_row_count;
  Uint64 m_committed_changes;
  /**
    Number of variable-size tuple parts, i.e. the number of tuples that has
    one or more non-NULL varchar/varbinary or blob fields. (The first few bytes
    of a blob is stored like that, the rest in a blob table.)
  */
  Uint64 m_varElemCount;

  // Consistency check.
  bool verifyVarSpace() const
  {
    if ((m_varWordsFree < Uint64(1)<<60) && //Underflow.
        m_varWordsFree * sizeof(Uint32) <=
        Uint64(noOfVarPages) * File_formats::NDB_PAGE_SIZE)
    {
      return true;
    }
    else
    {
      g_eventLogger->info("TUP : T%uF%u verifyVarSpace fails : "
                          "m_varWordsFree : %llu "
                          "noOfVarPages : %u",
                          fragTableId,
                          fragmentId,
                          m_varWordsFree,
                          noOfVarPages);
      return false;
    }
  }

};
typedef Ptr<Fragrecord> FragrecordPtr;

struct Operationrec {
  /*
   * Doubly linked list with anchor on tuple.
   * This is to handle multiple updates on the same tuple
   * by the same transaction.
   */
  Uint32 prevActiveOp;
  Uint32 nextActiveOp;

  Operationrec() {}
  bool is_first_operation() const { return prevActiveOp == RNIL;}
  bool is_last_operation() const { return nextActiveOp == RNIL;}

  Uint32 m_undo_buffer_space; // In words

  Uint32 m_any_value;
  Uint32 nextPool;
  
  /*
   * From fragment i-value we can find fragment and table record
   */
  Uint32 fragmentPtr;

  /*
   * We need references to both the original tuple and the copy tuple.
   * We keep the page's real i-value and its index and from there we
   * can find out about the fragment page id and the page offset.
   */
  Local_key m_tuple_location;
  Local_key m_copy_tuple_location;

  /*
   * We keep the record linked to the operation record in LQH.
   * This is needed due to writing of REDO log must be performed
   * in correct order, which is the same order as the writes
   * occurred. LQH can receive the records in different order.
   */
  Uint32 userpointer;

  /*
   * When responding to queries in the same transaction they will see
   * a result from the save point id the query was started. Again
   * functionality for multi-updates of the same record in one
   * transaction.
   */
  union {
    Uint32 savepointId;
    Uint32 m_commit_disk_callback_page;
  };

  Uint32 op_type;
  Uint32 trans_state;
  Uint32 tuple_state;

  /*
   * State variables on connection.
   * State variable on tuple after multi-updates
   * Is operation undo logged or not
   * Is operation in fragment list
   * Is operation in multi-update list
   * Operation type (READ, UPDATE, etc)
   * Is record primary replica
   * Is delete or insert performed
   */
  struct OpBitFields {
  /*
   * TUX needs to know the tuple version of the tuple since it
   * keeps an entry for both the committed and all versions in
   * a transaction currently. So each update will create a new
   * version even if in the same transaction.
   */
    unsigned int tupVersion : 16;

    unsigned int m_reorg : 2;
    unsigned int in_active_list : 1;
    unsigned int delete_insert_flag : 1;
    unsigned int m_disk_preallocated : 1;
    unsigned int m_load_diskpage_on_commit : 1;
    unsigned int m_wait_log_buffer : 1;
    unsigned int m_gci_written : 1;

    /**
     * @see TupKeyReq
     *
     * 0 = non-primary replica, fire detached triggers
     * 1 = primary replica, fire immediate and detached triggers
     * 2 = no fire triggers
     *     e.g If the op has no logical effect, it should not be
     *         sent as an event. Example op is OPTIMIZE table,
     *         which uses ZUPDATE to move varpart values physically.
     */
    unsigned int m_triggers : 2;

    /*
     * The TupKeyReq requested the after<Op>Triggers to be deferred.
     * Thus, the *constraints* defined in this trigger list should be
     * deferred until FIRE_TRIG_REQ arrives.
     * Note that this does not affect the triggers *declared* as deferred
     * ('no action') which are managed in the deferred<Op>Triggers and
     * always deferred until commit time (FIRE_TRIG_REQ)
     */
    unsigned int m_deferred_constraints : 1;

    /* No foreign keys should be checked for this operation.
     * No fk triggers will be fired.  
     */
    unsigned int m_disable_fk_checks : 1;
    unsigned int m_tuple_existed_at_start : 1;
  };

  union OpStruct {
    OpBitFields bit_field;
    Uint32 op_bit_fields;
  };
  OpStruct op_struct;

  /*
   * When refreshing a row, there are four scenarios
   * The actual scenario is encoded in the 'copy tuple location'
   * to enable special handling at commit time
   */
  enum RefreshScenario
  {
    RF_SINGLE_NOT_EXIST = 1,    /* Refresh op first in trans, no row */
    RF_SINGLE_EXIST     = 2,    /* Refresh op first in trans, row exists */
    RF_MULTI_NOT_EXIST  = 3,    /* Refresh op !first in trans, row deleted */
    RF_MULTI_EXIST      = 4     /* Refresh op !first in trans, row exists */
  };
};
typedef Ptr<Operationrec> OperationrecPtr;
typedef ArrayPool<Operationrec> Operationrec_pool;

  OperationrecPtr prepare_oper_ptr;
  /* ************* TRIGGER DATA ************* */
  /* THIS RECORD FORMS LISTS OF ACTIVE       */
  /* TRIGGERS FOR EACH TABLE.                 */
  /* THE RECORDS ARE MANAGED BY A TRIGGER     */
  /* POOL wHERE A TRIGGER RECORD IS SEIZED    */
  /* WHEN A TRIGGER IS ACTIVATED AND RELEASED */
  /* WHEN THE TRIGGER IS DEACTIVATED.         */
  /* **************************************** */
struct TupTriggerData {
  TupTriggerData() {}
  
  /**
   * Trigger id, used by DICT/TRIX to identify the trigger
   *
   * trigger Ids are unique per block for SUBSCRIPTION triggers.
   * This is so that BACKUP can use TUP triggers directly and delete them
   * properly.
   */
  Uint32 triggerId;

  /**
   * In 6.3 there is one trigger per operation
   */
  Uint32 oldTriggerIds[3]; // INS/UPD/DEL

  /**
   * Index id is needed for ordered index.
   */
  Uint32 indexId;

  /**
   * Trigger type etc, defines what the trigger is used for
   */
  TriggerType::Value triggerType;
  TriggerActionTime::Value triggerActionTime;
  TriggerEvent::Value triggerEvent;
  /**
   * Receiver block reference
   */
  Uint32 m_receiverRef;
  
  /**
   * Monitor all replicas, i.e. trigger will fire on all nodes where tuples
   * are stored
   */
  bool monitorReplicas;

  /**
   * Monitor all attributes, the trigger monitors all changes to attributes 
   * in the table
   */
  bool monitorAllAttributes;

  /**
   * Send only changed attributes at trigger firing time.
   */
  bool sendOnlyChangedAttributes;

  /**
   * Send also before values at trigger firing time.
   */
  bool sendBeforeValues;

  /**
   * Attribute mask, defines what attributes are to be monitored
   * Can be seen as a compact representation of SQL column name list
   */
  Bitmask<MAXNROFATTRIBUTESINWORDS> attributeMask;
  
  /**
   * Next ptr (used in pool/list)
   */
  union {
    Uint32 nextPool;
    Uint32 nextList;
  };
  
  /**
   * Prev pointer (used in list)
   */
  Uint32 prevList;

  inline void print(NdbOut & s) const { s << "[TriggerData = " << triggerId << "]"; };
};

typedef Ptr<TupTriggerData> TriggerPtr;
typedef ArrayPool<TupTriggerData> TupTriggerData_pool;
typedef DLFifoList<TupTriggerData_pool> TupTriggerData_list;

/**
 * Pool of trigger data record
 */
TupTriggerData_pool c_triggerPool;

  /* ************ TABLE RECORD ************ */
  /* THIS RECORD FORMS A LIST OF TABLE      */
  /* REFERENCE INFORMATION. ONE RECORD      */
  /* PER TABLE REFERENCE.                   */
  /* ************************************** */
  STATIC_CONST( MM = 0 );
  STATIC_CONST( DD = 1 );
  STATIC_CONST( DYN_BM_LEN_BITS = 8 );
  STATIC_CONST( DYN_BM_LEN_MASK = ((1 << DYN_BM_LEN_BITS) - 1));

  /* Array length in the data structures like
     dynTabDescriptor, dynVarSizeMask, dynFixSizeMask, etc.
     1 for dynamic main memory data,
     2 for dynamic main memory and dynamic disk data.
  */
  STATIC_CONST( NO_DYNAMICS = 2 );
  
  struct Tablerec {
    Tablerec(TupTriggerData_pool & triggerPool) :
      afterInsertTriggers(triggerPool),
      afterDeleteTriggers(triggerPool),
      afterUpdateTriggers(triggerPool),
      subscriptionInsertTriggers(triggerPool),
      subscriptionDeleteTriggers(triggerPool),
      subscriptionUpdateTriggers(triggerPool),
      constraintUpdateTriggers(triggerPool),
      deferredInsertTriggers(triggerPool),
      deferredUpdateTriggers(triggerPool),
      deferredDeleteTriggers(triggerPool),
      tuxCustomTriggers(triggerPool)
      {}
    
    Bitmask<MAXNROFATTRIBUTESINWORDS> notNullAttributeMask;
    Bitmask<MAXNROFATTRIBUTESINWORDS> blobAttributeMask;
    
    /*
      Extra table descriptor for dynamic attributes, or RNIL if none.
      The size of this depends on actual column definitions, so it is allocated
      _after_ seeing all columns, hence must be separate from the readKeyArray
      et al descriptor, which is allocated before seeing columns.
    */
    Uint32 dynTabDescriptor[2];

    /* Mask of variable-sized dynamic attributes. */
    Uint32* dynVarSizeMask[2];
    /*
      Mask of fixed-sized dynamic attributes. There is one bit set for each
      32-bit word occupied by fixed-size attributes, so fixed-size dynamic
      attributes >32bit have multiple bits here.
    */
    Uint32* dynFixSizeMask[2];

    ReadFunction* readFunctionArray;
    UpdateFunction* updateFunctionArray;
    CHARSET_INFO** charsetArray;
    
    Uint32 readKeyArray;
    /*
      Offset into Dbtup::tableDescriptor of the start of the descriptor
      words for each attribute.
      For attribute i, the AttributeDescriptor word is stored at index
      Tablerec::tabDescriptor+i*ZAD_SIZE, and the AttributeOffset word at
      index Tablerec::tabDescriptor+i*ZAD_SIZE+1.
    */
    Uint32 tabDescriptor;
    /*
      Offset into Dbtup::tableDescriptor of memory used as an array of Uint16.

      The values stored are offsets from Tablerec::tabDescriptor first for all
      fixed-sized static attributes, then static varsized attributes, then
      dynamic fixed-size, then dynamic varsized, and finally disk-stored fixed
      size:
              [mm_fix mm_var mm_dynfix mm_dynvar dd_fix]
      This is used to find the AttributeDescriptor and AttributeOffset words
      for an attribute. For example, the offset for the second dynamic
      fixed-size attribute is at index <num fixed> + <num varsize> + 1.
    */
    Uint32 m_real_order_descriptor;
    
    enum Bits
    {
      TR_Checksum = 0x1, // Need to be 1
      TR_RowGCI   = 0x2,
      TR_ForceVarPart = 0x4,
      TR_DiskPart  = 0x8,
      TR_ExtraRowGCIBits = 0x10,
      TR_ExtraRowAuthorBits = 0x20
    };
    Uint16 m_bits;
    Uint16 total_rec_size; // Max total size for entire tuple in words
    
    /**
     * Aggregates
     */
    Uint16 m_no_of_attributes;
    Uint16 m_no_of_disk_attributes;
    Uint16 noOfKeyAttr;
    Uint16 noOfCharsets;
    Uint16 m_dyn_null_bits[2];
    Uint16 m_no_of_extra_columns; // "Hidden" columns

    bool need_expand() const { 
      return m_no_of_attributes > m_attributes[MM].m_no_of_fixsize;
    }

    bool need_expand(bool disk) const { 
      return m_attributes[MM].m_no_of_varsize > 0 ||
        m_attributes[MM].m_no_of_dynamic > 0 ||
	(disk && m_no_of_disk_attributes > 0);
    }
    
    bool need_shrink() const {
      return 
	m_attributes[MM].m_no_of_varsize > 0 ||
        m_attributes[MM].m_no_of_dynamic > 0 ||
	m_attributes[DD].m_no_of_varsize > 0;
    }
    
    bool need_shrink(bool disk) const {
      return 
	m_attributes[MM].m_no_of_varsize > 0 ||
	m_attributes[MM].m_no_of_dynamic > 0 ||
        (disk && m_attributes[DD].m_no_of_varsize > 0);
    }

    template <Uint32 bit> Uint32 getExtraAttrId() const {
      if (bit == TR_ExtraRowGCIBits)
        return 0;
      Uint32 no = 0;
      if (m_bits & TR_ExtraRowGCIBits)
        no++;
      assert(bit == TR_ExtraRowAuthorBits);
      //if (bit == TR_ExtraRowAuthorBits)
      return no;
    }

    /**
     * Descriptors for MM and DD part
     */
    struct Tuple_offsets {
      Uint8 m_null_words;
      Uint8 m_null_offset;
      Uint16 m_disk_ref_offset; // In words relative m_data
      Uint16 m_fix_header_size; // For fix size tuples= total rec size(part)
      Uint16 m_max_var_offset;  // In bytes relative m_var_data.m_data_ptr
      Uint16 m_max_dyn_offset;  // In bytes relative m_var_data.m_dyn_data_ptr
      Uint16 m_dyn_null_words;  // 32-bit words in dynattr bitmap
    } m_offsets[2];
    
    Uint32 get_check_offset(Uint32 mm) const {
      return m_offsets[mm].m_fix_header_size;
    }

    struct {
      Uint16 m_no_of_fixsize;
      Uint16 m_no_of_varsize;
      Uint16 m_no_of_dynamic;                   // Total no. of dynamic attrs
      Uint16 m_no_of_dyn_fix;                   // No. of fixsize dynamic
      Uint16 m_no_of_dyn_var;                   // No. of varsize dynamic
      /*
        Note that due to bit types, we may have
            m_no_of_dynamic > m_no_of_dyn_fix + m_no_of_dyn_var
      */
    } m_attributes[2];
    
    // Lists of trigger data for active triggers
    TupTriggerData_list afterInsertTriggers;
    TupTriggerData_list afterDeleteTriggers;
    TupTriggerData_list afterUpdateTriggers;
    TupTriggerData_list subscriptionInsertTriggers;
    TupTriggerData_list subscriptionDeleteTriggers;
    TupTriggerData_list subscriptionUpdateTriggers;
    TupTriggerData_list constraintUpdateTriggers;
    TupTriggerData_list deferredInsertTriggers;
    TupTriggerData_list deferredUpdateTriggers;
    TupTriggerData_list deferredDeleteTriggers;

    // List of ordered indexes
    TupTriggerData_list tuxCustomTriggers;
    
    Uint32 fragid[MAX_FRAG_PER_LQH];
    Uint32 fragrec[MAX_FRAG_PER_LQH];

    union {
      struct {
        Uint32 tabUserPtr;
        Uint32 tabUserRef;
        Uint32 m_outstanding_ops;
        Uint32 m_fragPtrI;
        Uint32 m_filePointer;
        Uint16 m_firstFileId;
        Uint16 m_lastFileId;
        Uint16 m_numDataFiles;
        Uint8 m_file_type;
        Uint8 m_lcpno;
      } m_dropTable;
      struct {
        Uint32 m_fragOpPtrI;
        Uint32 defValSectionI;
        Local_key defValLocation; 
      } m_createTable;
      struct {
        Uint32 m_gci_hi;
      } m_reorg_suma_filter;
    };

    State tableStatus;
    Local_key m_default_value_location;
  };  
  Uint32 m_read_ctl_file_data[BackupFormat::NDB_LCP_CTL_FILE_SIZE_BIG / 4];
  /*
    It is more space efficient to store dynamic fixed-size attributes
    of more than about 16 words as variable-sized internally.
   */
  STATIC_CONST(InternalMaxDynFix= 16);

  struct Disk_undo 
  {
    enum 
    {
      UNDO_ALLOC = File_formats::Undofile::UNDO_TUP_ALLOC
      ,UNDO_UPDATE = File_formats::Undofile::UNDO_TUP_UPDATE
      ,UNDO_FREE = File_formats::Undofile::UNDO_TUP_FREE
      ,UNDO_DROP = File_formats::Undofile::UNDO_TUP_DROP
      ,UNDO_UPDATE_PART = File_formats::Undofile::UNDO_TUP_UPDATE_PART
      ,UNDO_FIRST_UPDATE_PART =
        File_formats::Undofile::UNDO_TUP_FIRST_UPDATE_PART
      ,UNDO_FREE_PART = File_formats::Undofile::UNDO_TUP_FREE_PART
    };
    
    struct Alloc 
    {
      Uint32 m_file_no_page_idx; // 16 bit file_no, 16 bit page_idx
      Uint32 m_page_no;
      Uint32 m_type_length; // 16 bit type, 16 bit length
    };
    
    struct Update
    {
      Uint32 m_file_no_page_idx; // 16 bit file_no, 16 bit page_idx
      Uint32 m_page_no;
      Uint32 m_gci;
      Uint32 m_data[1];
      Uint32 m_type_length; // 16 bit type, 16 bit length
    };
    
    struct UpdatePart
    {
      Uint32 m_file_no_page_idx; // 16 bit file_no, 16 bit page_idx
      Uint32 m_page_no;
      Uint32 m_gci;
      Uint32 m_offset;
      Uint32 m_data[1];
      Uint32 m_type_length; // 16 bit type, 16 bit length
    };

    struct Free
    {
      Uint32 m_file_no_page_idx; // 16 bit file_no, 16 bit page_idx
      Uint32 m_page_no;
      Uint32 m_gci;
      Uint32 m_data[1];
      Uint32 m_type_length; // 16 bit type, 16 bit length
    };

    struct Create
    {
      Uint32 m_table;
      Uint32 m_type_length; // 16 bit type, 16 bit length
    };

    struct Drop
    {
      Uint32 m_table;
      Uint32 m_type_length; // 16 bit type, 16 bit length
    };
  };
  
  Extent_info_pool c_extent_pool;
  Extent_info_hash c_extent_hash;
  Page_request_pool c_page_request_pool;

  typedef Ptr<Tablerec> TablerecPtr;

  struct storedProc {
    Uint32 storedProcIVal;
    Uint32 nextPool;
    Uint16 storedCode;
  };

typedef Ptr<storedProc> StoredProcPtr;
typedef ArrayPool<storedProc> StoredProc_pool;

StoredProc_pool c_storedProcPool;
RSS_AP_SNAPSHOT(c_storedProcPool);
Uint32 c_storedProcCountNonAPI;
void storedProcCountNonAPI(BlockReference apiBlockref, int add_del);

/* **************************** TABLE_DESCRIPTOR RECORD ******************************** */
/* THIS VARIABLE IS USED TO STORE TABLE DESCRIPTIONS. A TABLE DESCRIPTION IS STORED AS A */
/* CONTIGUOS ARRAY IN THIS VARIABLE. WHEN A NEW TABLE IS ADDED A CHUNK IS ALLOCATED IN   */
/* THIS RECORD. WHEN ATTRIBUTES ARE ADDED TO THE TABLE, A NEW CHUNK OF PROPER SIZE IS    */
/* ALLOCATED AND ALL DATA IS COPIED TO THIS NEW CHUNK AND THEN THE OLD CHUNK IS PUT IN   */
/* THE FREE LIST. EACH TABLE IS DESCRIBED BY A NUMBER OF TABLE DESCRIPTIVE ATTRIBUTES    */
/* AND A NUMBER OF ATTRIBUTE DESCRIPTORS AS SHOWN IN FIGURE BELOW                        */
/*                                                                                       */
/* WHEN ALLOCATING A TABLE DESCRIPTOR THE SIZE IS ALWAYS A MULTIPLE OF 16 WORDS.         */
/*                                                                                       */
/*               ----------------------------------------------                          */
/*               |    TRAILER USED FOR ALLOC/DEALLOC          |                          */
/*               ----------------------------------------------                          */
/*               |    TABLE DESCRIPTIVE ATTRIBUTES            |                          */
/*               ----------------------------------------------                          */
/*               |    ATTRIBUTE DESCRIPTION 1                 |                          */
/*               ----------------------------------------------                          */
/*               |    ATTRIBUTE DESCRIPTION 2                 |                          */
/*               ----------------------------------------------                          */
/*               |                                            |                          */
/*               |                                            |                          */
/*               |                                            |                          */
/*               ----------------------------------------------                          */
/*               |    ATTRIBUTE DESCRIPTION N                 |                          */
/*               ----------------------------------------------                          */
/*                                                                                       */
/* THE TABLE DESCRIPTIVE ATTRIBUTES CONTAINS THE FOLLOWING ATTRIBUTES:                   */
/*                                                                                       */
/*               ----------------------------------------------                          */
/*               |    HEADER (TYPE OF INFO)                   |                          */
/*               ----------------------------------------------                          */
/*               |    SIZE OF WHOLE CHUNK (INCL. TRAILER)     |                          */
/*               ----------------------------------------------                          */
/*               |    TABLE IDENTITY                          |                          */
/*               ----------------------------------------------                          */
/*               |    FRAGMENT IDENTITY                       |                          */
/*               ----------------------------------------------                          */
/*               |    NUMBER OF ATTRIBUTES                    |                          */
/*               ----------------------------------------------                          */
/*               |    SIZE OF FIXED ATTRIBUTES                |                          */
/*               ----------------------------------------------                          */
/*               |    NUMBER OF NULL FIELDS                   |                          */
/*               ----------------------------------------------                          */
/*               |    NOT USED                                |                          */
/*               ----------------------------------------------                          */
/*                                                                                       */
/* THESE ATTRIBUTES ARE ALL ONE R-VARIABLE IN THE RECORD.                                */
/* NORMALLY ONLY ONE TABLE DESCRIPTOR IS USED. DURING SCHEMA CHANGES THERE COULD         */
/* HOWEVER EXIST MORE THAN ONE TABLE DESCRIPTION SINCE THE SCHEMA CHANGE OF VARIOUS      */
/* FRAGMENTS ARE NOT SYNCHRONISED. THIS MEANS THAT ALTHOUGH THE SCHEMA HAS CHANGED       */
/* IN ALL FRAGMENTS, BUT THE FRAGMENTS HAVE NOT REMOVED THE ATTRIBUTES IN THE SAME       */
/* TIME-FRAME. THEREBY SOME ATTRIBUTE INFORMATION MIGHT DIFFER BETWEEN FRAGMENTS.        */
/* EXAMPLES OF ATTRIBUTES THAT MIGHT DIFFER ARE SIZE OF FIXED ATTRIBUTES, NUMBER OF      */
/* ATTRIBUTES, FIELD START WORD, START BIT.                                              */
/*                                                                                       */
/* AN ATTRIBUTE DESCRIPTION CONTAINS THE FOLLOWING ATTRIBUTES:                           */
/*                                                                                       */
/*               ----------------------------------------------                          */
/*               |    Field Type, 4 bits (LSB Bits)           |                          */
/*               ----------------------------------------------                          */
/*               |    Attribute Size, 4 bits                  |                          */
/*               ----------------------------------------------                          */
/*               |    NULL indicator 1 bit                    |                          */
/*               ----------------------------------------------                          */
/*               |    Indicator if TUP stores attr. 1 bit     |                          */
/*               ----------------------------------------------                          */
/*               |    Not used 6 bits                         |                          */
/*               ----------------------------------------------                          */
/*               |    No. of elements in fixed array 16 bits  |                          */
/*               ----------------------------------------------                          */
/*               ----------------------------------------------                          */
/*               |    Field Start Word, 21 bits (LSB Bits)    |                          */
/*               ----------------------------------------------                          */
/*               |    NULL Bit, 11 bits                       |                          */
/*               ----------------------------------------------                          */
/*                                                                                       */
/* THE ATTRIBUTE SIZE CAN BE 1,2,4,8,16,32,64 AND 128 BITS.                              */
/*                                                                                       */
/* THE UNUSED PARTS OF THE RECORDS ARE PUT IN A LINKED LIST OF FREE PARTS. EACH OF       */
/* THOSE FREE PARTS HAVE THREE RECORDS ASSIGNED AS SHOWN IN THIS STRUCTURE               */
/* ALL FREE PARTS ARE SET INTO A CHUNK LIST WHERE EACH CHUNK IS AT LEAST 16 WORDS        */
/*                                                                                       */
/*               ----------------------------------------------                          */
/*               |    HEADER = RNIL                           |                          */
/*               ----------------------------------------------                          */
/*               |    SIZE OF FREE AREA                       |                          */
/*               ----------------------------------------------                          */
/*               |    POINTER TO PREVIOUS FREE AREA           |                          */
/*               ----------------------------------------------                          */
/*               |    POINTER TO NEXT FREE AREA               |                          */
/*               ----------------------------------------------                          */
/*                                                                                       */
/* IF THE POINTER TO THE NEXT AREA IS RNIL THEN THIS IS THE LAST FREE AREA.              */
/*                                                                                       */
/*****************************************************************************************/
struct TableDescriptor {
  Uint32 tabDescr;
};
typedef Ptr<TableDescriptor> TableDescriptorPtr;

struct HostBuffer {
  bool  inPackedList;
  Uint32 packetLenTA;
  Uint32 noOfPacketsTA;
  Uint32 packetBufferTA[30];
};
typedef Ptr<HostBuffer> HostBufferPtr;

  /*
   * Build index operation record.
   */
  struct BuildIndexRec {
    BuildIndexRec() {}

    BuildIndxImplReq m_request;
    Uint8  m_build_vs;          // varsize pages
    Uint32 m_indexId;           // the index
    Uint32 m_fragNo;            // fragment number under Tablerec
    Uint32 m_pageId;            // logical fragment page id
    Uint32 m_tupleNo;           // tuple number on page
    Uint32 m_buildRef;          // Where to send tuples
    Uint32 m_outstanding;       // If mt-build...
    BuildIndxImplRef::ErrorCode m_errorCode;
    union {
      Uint32 nextPool;
      Uint32 nextList;
    };
    Uint32 prevList;
  };
  typedef Ptr<BuildIndexRec> BuildIndexPtr;
  typedef ArrayPool<BuildIndexRec> BuildIndexRec_pool;
  typedef DLList<BuildIndexRec_pool> BuildIndexRec_list;
  BuildIndexRec_pool c_buildIndexPool;
  BuildIndexRec_list c_buildIndexList;
  Uint32 c_noOfBuildIndexRec;

  int mt_scan_init(Uint32 tableId, Uint32 fragId, Local_key * pos, Uint32 * fragPtrI);
  int mt_scan_next(Uint32 tableId, Uint32 fragPtrI, Local_key* pos, bool moveNext);

  /**
   * Reference to variable part when a tuple is chained
   */
  struct Var_part_ref 
  {
    Uint32 m_page_no;
    Uint32 m_page_idx;
    STATIC_CONST( SZ32 = 2 );

    void copyout(Local_key* dst) const {
      dst->m_page_no = m_page_no;
      dst->m_page_idx = m_page_idx;
    }

    void assign(const Local_key* src) {
      m_page_no = src->m_page_no;
      m_page_idx = src->m_page_idx;
    }
  };
  
  struct Disk_part_ref
  {
    STATIC_CONST( SZ32 = 2 );
  };

  struct Tuple_header
  {
    union {
      /**
       * List of prepared operations for this tuple.
       * Points to most recent/last operation, ie. to walk the list must follow
       * regOperPtr->prevActiveOp links.
       */
      Uint32 m_operation_ptr_i;  // OperationPtrI
      Uint32 m_base_record_page_no;  // For disk tuple, ref to MM tuple
      Uint32 m_first_words[1];
    };
    union
    {
      Uint32 m_header_bits;      // Header word
      Uint32 m_base_record_page_idx;  // For disk tuple, ref to MM tuple
    };
    union {
      Uint32 m_checksum;
      Uint32 m_data[1];
      Uint32 m_null_bits[1];
    };

    STATIC_CONST( HeaderSize = 2 );
    
    /*
     Header bits.

     MM_GROWN: When a tuple is updated to a bigger size, the original varpart
     of the tuple is immediately re-allocated to a location with sufficient
     size for the new data (but containing only the original smaller-sized
     data). This is so that commit can be sure to find room for the extra
     data. In the case of abort, the varpart must then be shrunk. For a
     MM_GROWN tuple, the original size is stored in the last word of the
     varpart until commit.

     DELETE_WAIT: When a tuple has been marked to be deleted, the tuple header
     has the DELETE_WAIT bit set. Note that DELETE_WAIT means that the tuple
     hasn't actually been deleted. When a tuple has been deleted, it is marked
     with the FREE flag and DELETE_WAIT is reset.
     The need for DELETE_WAIT arises due to the real-time break between the
     marking of the tuple and the actual deletion of the tuple for disk data
     rows. This information would be useful for reads since they'd know the
     proper state of the row. (Related Bug #27584165)
    */
    STATIC_CONST( TUP_VERSION_MASK = 0xFFFF );
    STATIC_CONST( COPY_TUPLE  = 0x00010000 ); // Is this a copy tuple
    STATIC_CONST( DISK_PART   = 0x00020000 ); // Is there a disk part
    STATIC_CONST( DISK_ALLOC  = 0x00040000 ); // Is disk part allocated
    STATIC_CONST( DISK_INLINE = 0x00080000 ); // Is disk inline
    STATIC_CONST( ALLOC       = 0x00100000 ); // Is record allocated now
    STATIC_CONST( NOT_USED_BIT= 0x00200000 ); //
    STATIC_CONST( MM_GROWN    = 0x00400000 ); // Has MM part grown
    STATIC_CONST( FREE        = 0x00800000 ); // Is free
    STATIC_CONST( LCP_SKIP    = 0x01000000 ); // Should not be returned in LCP
    STATIC_CONST( VAR_PART    = 0x04000000 ); // Is there a varpart
    STATIC_CONST( REORG_MOVE  = 0x08000000 ); // Tuple will be moved in reorg
    STATIC_CONST( LCP_DELETE  = 0x10000000 ); // Tuple deleted at LCP start
    STATIC_CONST( DELETE_WAIT = 0x20000000 ); // Waiting for delete tuple page

    Tuple_header() {}
    Uint32 get_tuple_version() const { 
      return m_header_bits & TUP_VERSION_MASK;
    }
    void set_tuple_version(Uint32 version) { 
      m_header_bits= 
	(m_header_bits & ~(Uint32)TUP_VERSION_MASK) | 
	(version & TUP_VERSION_MASK);
    }
    void get_base_record_ref(Local_key& key)
    {
      require(m_base_record_page_idx <= MAX_TUPLES_PER_PAGE);
      key.m_page_no = m_base_record_page_no;
      key.m_page_idx = m_base_record_page_idx;
    }
    void set_base_record_ref(Local_key key)
    {
      m_base_record_page_no = key.m_page_no;
      m_base_record_page_idx = key.m_page_idx;
    }
    Uint32* get_null_bits(const Tablerec* tabPtrP) {
      return m_null_bits+tabPtrP->m_offsets[MM].m_null_offset;
    }

    Uint32* get_null_bits(const Tablerec* tabPtrP, Uint32 mm) {
      return m_null_bits+tabPtrP->m_offsets[mm].m_null_offset;
    }
    
    Var_part_ref* get_var_part_ref_ptr(const Tablerec* tabPtrP) {
      return (Var_part_ref*)(get_disk_ref_ptr(tabPtrP) + Disk_part_ref::SZ32);
    }

    const Var_part_ref* get_var_part_ref_ptr(const Tablerec* tabPtrP) const {
      return (Var_part_ref*)(get_disk_ref_ptr(tabPtrP) + Disk_part_ref::SZ32);
    }
    
    Uint32* get_end_of_fix_part_ptr(const Tablerec* tabPtrP) {
      return m_data + tabPtrP->m_offsets[MM].m_fix_header_size - 
        Tuple_header::HeaderSize;
    }
    
    const Uint32* get_end_of_fix_part_ptr(const Tablerec* tabPtrP) const {
      return m_data + tabPtrP->m_offsets[MM].m_fix_header_size - 
        Tuple_header::HeaderSize;
    }
    
    Uint32* get_disk_ref_ptr(const Tablerec* tabPtrP) {
      return m_first_words + tabPtrP->m_offsets[MM].m_disk_ref_offset;
    }

    const Uint32* get_disk_ref_ptr(const Tablerec* tabPtrP) const {
      return m_first_words + tabPtrP->m_offsets[MM].m_disk_ref_offset;
    }

    Uint32 *get_mm_gci(const Tablerec* tabPtrP){
      /* Mandatory position even if TR_RowGCI isn't set (happens in restore */
      return m_data + (tabPtrP->m_bits & Tablerec::TR_Checksum);
    }

    Uint32 *get_dd_gci(const Tablerec* tabPtrP, Uint32 mm){
      assert(tabPtrP->m_bits & Tablerec::TR_RowGCI);
      return m_data;
    }
  };

  /**
   * Format of varpart after insert/update
   */
  struct Varpart_copy
  {
    Uint32 m_len;
    Uint32 m_data[1]; // Only used for easy offset handling

    STATIC_CONST( SZ32 = 1 );
  };

  enum When
  {
    KRS_PREPARE = 0,
    KRS_COMMIT = 1,
    KRS_PRE_COMMIT_BASE = 2,
    KRS_UK_PRE_COMMIT0 = KRS_PRE_COMMIT_BASE + TriggerPreCommitPass::UK_PASS_0,
    KRS_UK_PRE_COMMIT1 = KRS_PRE_COMMIT_BASE + TriggerPreCommitPass::UK_PASS_1,
    KRS_FK_PRE_COMMIT  = KRS_PRE_COMMIT_BASE + TriggerPreCommitPass::FK_PASS_0
  };

struct KeyReqStruct {

  KeyReqStruct(EmulatedJamBuffer * _jamBuffer, When when) :
    changeMask()
  {
#if defined VM_TRACE || defined ERROR_INSERT
    memset(this, 0xf3, sizeof(* this));
#endif
    jamBuffer = _jamBuffer;
    m_when = when;
    m_deferred_constraints = true;
    m_disable_fk_checks = false;
    m_tuple_ptr = NULL;
  }

  KeyReqStruct(EmulatedJamBuffer * _jamBuffer) :
    changeMask(false)
  {
#if defined VM_TRACE || defined ERROR_INSERT
    memset(this, 0xf3, sizeof(* this));
#endif
    jamBuffer = _jamBuffer;
    m_when = KRS_PREPARE;
    m_deferred_constraints = true;
    m_disable_fk_checks = false;
  }

  KeyReqStruct(Dbtup* tup) :
    changeMask(false)
  {
#if defined VM_TRACE || defined ERROR_INSERT
    memset(this, 0xf3, sizeof(* this));
#endif
    jamBuffer = tup->jamBuffer();
    m_when = KRS_PREPARE;
    m_deferred_constraints = true;
    m_disable_fk_checks = false;
  }

  KeyReqStruct(Dbtup* tup, When when) :
    changeMask()
  {
#if defined VM_TRACE || defined ERROR_INSERT
    memset(this, 0xf3, sizeof(* this));
#endif
    jamBuffer = tup->jamBuffer();
    m_when = when;
    m_deferred_constraints = true;
    m_disable_fk_checks = false;
    m_tuple_ptr = NULL;
  }
  
/**
 * These variables are used as temporary storage during execution of the
 * TUPKEYREQ signal.
 *
 * The first set of variables defines a number of variables needed for
 * the fix part of the tuple.
 *
 * The second part defines a number of commonly used meta data variables.
 *
 * The third part is variables needed only for updates and inserts.
 *
 * The fourth set of variables defines a set of variables needed for the
 * variable part.
 *
 * The fifth part is a long array of real lengths which is is put last
 * for cache memory reasons. This is part of the variable part and
 * contains the real allocated lengths whereas the tuple contains
 * the length of attribute stored.
 */

  Tablerec* tablePtrP;
  Fragrecord* fragPtrP;
  Operationrec * operPtrP;
  EmulatedJamBuffer * jamBuffer;
  Tuple_header *m_tuple_ptr;

  /**
   * Variables often used in read of columns
   */
  TableDescriptor *attr_descr;
  Uint32 check_offset[2];
  Uint32          max_read;
  Uint32          out_buf_index;

  Uint32          out_buf_bits;
  Uint32          in_buf_index;


  union {
    Uint32 in_buf_len;
    Uint32 m_lcp_varpart_len;
  };
  union {
    Uint32          attr_descriptor;
    Uint32 errorCode; // Used in DbtupRoutines read/update functions
  };
  bool            xfrm_flag;

  /* Flag: is tuple in expanded or in shrunken/stored format? */
  bool is_expanded;
  bool m_is_lcp;
  enum When m_when;

  Tuple_header *m_disk_ptr;
  PagePtr m_page_ptr;
  PagePtr m_varpart_page_ptr;    // could be same as m_page_ptr_p
  PagePtr m_disk_page_ptr;       //
  Local_key m_row_id;
  Uint32 optimize_options;
  
  bool            dirty_op;
  bool            interpreted_exec;
  bool            last_row;
  bool            m_use_rowid;
  bool            m_nr_copy_or_redo;
  Uint8           m_reorg;
  Uint8           m_prio_a_flag;
  bool            m_deferred_constraints;
  bool            m_disable_fk_checks;

  Signal*         signal;
  Uint32 num_fired_triggers;
  Uint32 no_exec_instructions;
  Uint32 frag_page_id;
  Uint32 hash_value;
  Uint32 gci_hi;
  Uint32 gci_lo;
  Uint32 log_size;
  Uint32 read_length;
  Uint32 attrinfo_len;
  Uint32 tc_operation_ptr;
  Uint32 trans_id1;
  Uint32 trans_id2;
  Uint32 TC_index;
  // next 2 apply only to attrids >= 64 (zero otherwise)
  BlockReference TC_ref;
  BlockReference rec_blockref;

  struct Var_data {
    /*
      These are the pointers and offsets to the variable-sized part of the row
      (static part, alwways stored even if NULL). They are used both for
      expanded and shrunken form, with different values to allow using the
      same read/update code for both forms.
    */
    char *m_data_ptr;
    Uint16 *m_offset_array_ptr;
    Uint16 m_var_len_offset;
    Uint16 m_max_var_offset;
    Uint16 m_max_dyn_offset;

    /* These are the pointers and offsets to the dynamic part of the row. */

    /* Pointer to the start of the bitmap for the dynamic part of the row. */
    char *m_dyn_data_ptr;
    /* Number of 32-bit words in dynamic part (stored/shrunken format). */
    Uint32 m_dyn_part_len;
    /*
      Pointer to array with one element for each dynamic attribute (both
      variable and fixed size). Each value is the offset from the end of the
      bitmap to the start of the data for that attribute.
    */
    Uint16 *m_dyn_offset_arr_ptr;
    /*
      Offset from m_dyn_offset_array_ptr of array with one element for each
      dynamic attribute. Each value is the offset to the end of data for that
      attribute, so the difference to m_dyn_offset_array_ptr elements provides
      the data lengths.
    */
    Uint16 m_dyn_len_offset;
  } m_var_data[2];

  /*
   * A bit mask where a bit set means that the update or insert
   * was updating this record.
   */
  Bitmask<MAXNROFATTRIBUTESINWORDS> changeMask;
  Uint16 var_pos_array[2*MAX_ATTRIBUTES_IN_TABLE + 1];
  OperationrecPtr prevOpPtr;
};

  friend struct Undo_buffer;
  Undo_buffer c_undo_buffer;
  
/*
 No longer used:
 Implemented by shift instructions in subroutines instead
 
struct TupHeadInfo {
  struct BitPart {
    unsigned int disk_indicator : 1;
    unsigned int var_part_loc_ind : 1;
    unsigned int initialised : 1;
    unsigned int not_used_yet : 5;
    unsigned int no_var_sized : 8;
    unsigned int tuple_version : 16;
  };
  union {
    Uint32 all;
    BitPart bit_part;
  };
};
*/

  struct ChangeMask
  {
    Uint32 m_cols;
    Uint32 m_mask[1];

    const Uint32 * end_of_mask() const { return end_of_mask(m_cols); }
    const Uint32 * end_of_mask(Uint32 cols) const {
      return m_mask + ((cols + 31) >> 5);
    }

    Uint32 * end_of_mask() { return end_of_mask(m_cols); }
    Uint32 * end_of_mask(Uint32 cols) {
      return m_mask + ((cols + 31) >> 5);
    }
  };

// updateAttributes module
  Uint32          terrorCode;

public:
  Dbtup(Block_context&, Uint32 instanceNumber = 0);
  virtual ~Dbtup();

  /*
   * TUX uses logical tuple address when talking to ACC and LQH.
   */
  void tuxGetTupAddr(Uint32 fragPtrI, Uint32 pageId, Uint32 pageOffset,
                     Uint32& lkey1, Uint32& lkey2);

  /*
   * TUX index in TUP has single Uint32 array attribute which stores an
   * index node.  TUX reads and writes the node directly via pointer.
   */
  int tuxAllocNode(EmulatedJamBuffer*, Uint32 fragPtrI, Uint32& pageId, Uint32& pageOffset, Uint32*& node);
  void tuxFreeNode(Uint32 fragPtrI, Uint32 pageId, Uint32 pageOffset, Uint32* node);
  void tuxGetNode(Uint32 fragPtrI, Uint32 pageId, Uint32 pageOffset, Uint32*& node);

  /*
   * TUX reads primary table attributes for index keys.  Tuple is
   * specified by location of original tuple and version number.  Input
   * is attribute ids in AttributeHeader format.  Output is attribute
   * data with headers.  Uses readAttributes with xfrm option set.
   * After wl4163, xfrm is not set.
   * Returns number of words or negative (-terrorCode) on error.
   */
  int tuxReadAttrs(EmulatedJamBuffer*,
                   Uint32 fragPtrI,
                   Uint32 pageId,
                   Uint32 pageOffset,
                   Uint32 tupVersion,
                   const Uint32* attrIds,
                   Uint32 numAttrs,
                   Uint32* dataOut,
                   bool xfrmFlag);
  int tuxReadAttrsCurr(EmulatedJamBuffer*,
                       const Uint32* attrIds,
                       Uint32 numAttrs,
                       Uint32* dataOut,
                       bool xfrmFlag,
                       Uint32 tupVersion);
  int tuxReadAttrsCommon(KeyReqStruct &req_struct,
                         const Uint32* attrIds,
                         Uint32 numAttrs,
                         Uint32* dataOut,
                         bool xfrmFlag,
                         Uint32 tupVersion);

  /*
   * TUX reads primary key without headers into an array of words.  Used
   * for md5 summing and when returning keyinfo.  Returns number of
   * words or negative (-terrorCode) on error.
   */
  int tuxReadPk(Uint32 fragPtrI, Uint32 pageId, Uint32 pageOffset, Uint32* dataOut, bool xfrmFlag);

  /*
   * ACC reads primary key without headers into an array of words.  At
   * this point in ACC deconstruction, ACC still uses logical references
   * to fragment and tuple.
   */
  int accReadPk(Uint32 tableId, Uint32 fragId, Uint32 fragPageId, Uint32 pageIndex, Uint32* dataOut, bool xfrmFlag);

  inline Uint32 get_tuple_operation_ptr_i()
  {
    Tuple_header *tuple_ptr = (Tuple_header*)prepare_tuple_ptr;
    return tuple_ptr->m_operation_ptr_i;
  }
  /*
   * TUX checks if tuple is visible to scan.
   */
  bool tuxQueryTh(Uint32 opPtrI,
                  Uint32 tupVersion,
                  Uint32 transId1,
                  Uint32 transId2,
                  bool dirty,
                  Uint32 savepointId);

  int load_diskpage(Signal*, Uint32 opRec, Uint32 fragPtrI,
		    Uint32 lkey1, Uint32 lkey2, Uint32 flags);

  int load_diskpage_scan(Signal*, Uint32 opRec, Uint32 fragPtrI,
			 Uint32 lkey1, Uint32 lkey2, Uint32 flags);

  void start_restore_lcp(Uint32 tableId, Uint32 fragmentId);
  void complete_restore_lcp(Signal*,
                            Uint32 ref,
                            Uint32 data,
                            Uint32 restoredLcpId,
                            Uint32 restoredLocalLcpId,
                            Uint32 maxGciCompleted,
                            Uint32 maxGciWritten,
                            Uint32 tableId,
                            Uint32 fragmentId);
  Uint32 get_max_lcp_record_size(Uint32 tableId);
  
  int nr_read_pk(Uint32 fragPtr, const Local_key*, Uint32* dataOut, bool&copy);
  int nr_update_gci(Uint32 fragPtr,
                    const Local_key*,
                    Uint32 gci,
                    bool tuple_exists);
  int nr_delete(Signal*, Uint32, Uint32 fragPtr, const Local_key*, Uint32 gci);

  void nr_delete_page_callback(Signal*, Uint32 op, Uint32 page);
  void nr_delete_log_buffer_callback(Signal*, Uint32 op, Uint32 page);

  bool get_frag_info(Uint32 tableId, Uint32 fragId, Uint32* maxPage);

  void execSTORED_PROCREQ(Signal* signal);

  void start_lcp_scan(Uint32 tableId,
                      Uint32 fragmentId,
                      Uint32 & max_page_cnt);
  void stop_lcp_scan(Uint32 tableId, Uint32 fragmentId);
  void lcp_frag_watchdog_print(Uint32 tableId, Uint32 fragmentId);

  Uint64 get_restore_row_count(Uint32 tableId, Uint32 fragmentId);
  void set_lcp_start_gci(Uint32 fragPtrI, Uint32 startGci);
  void get_lcp_frag_stats(Uint32 fragPtrI,
                          Uint32 startGci,
                          Uint32 & maxPageCount,
                          Uint64 & row_count,
                          Uint64 & prev_row_count,
                          Uint64 & row_change_count,
                          Uint64 & memory_used_in_bytes,
                          bool reset_flag);

  // Statistics about fragment memory usage.
  struct FragStats
  {
    Uint64 committedRowCount;
    Uint64 committedChanges;
    // Size of fixed-size part of record.
    Uint32 fixedRecordBytes;
    // Page size (32k, see File_formats::NDB_PAGE_SIZE).
    Uint32 pageSizeBytes;
    // Number of fixed-size parts that fits in each page.
    Uint32 fixedSlotsPerPage;
    // Number of pages allocated for storing fixed-size parts.
    Uint64 fixedMemoryAllocPages;
    // Number of pages allocated for storing var-size parts.
    Uint64 varMemoryAllocPages;
    /** 
      Number of bytes for storing var-size parts that are allocated but not yet 
      used.
    */
    Uint64 varMemoryFreeBytes;
    // Number of fixed-size elements (i.e. number of rows.)
    Uint64 fixedElemCount;
    /**
      Number of var-size elements. There will be one for each row that has at
      least one non-null var-size field (varchar/varbinary/blob).
     */
    Uint64 varElemCount;
    // Size of the page map (DynArr256) that maps from logical to physical pages.
    Uint64 logToPhysMapAllocBytes;
  };

  const FragStats get_frag_stats(Uint32 fragId) const;

private:
  BLOCK_DEFINES(Dbtup);

  // Transit signals
  void execDEBUG_SIG(Signal* signal);
  void execCONTINUEB(Signal* signal);

  // Received signals
  void execDUMP_STATE_ORD(Signal* signal);
  void execSEND_PACKED(Signal* signal);
  void execSTTOR(Signal* signal);
  void execTUP_LCPREQ(Signal* signal);
  void execEND_LCPREQ(Signal* signal);
  void execSTART_RECREQ(Signal* signal);
  void execMEMCHECKREQ(Signal* signal);
  void execTUPSEIZEREQ(Signal* signal);
  void execTUPRELEASEREQ(Signal* signal);

  void execCREATE_TAB_REQ(Signal*);
  void execTUP_ADD_ATTRREQ(Signal* signal);
  void execTUPFRAGREQ(Signal* signal);
  void execTUP_COMMITREQ(Signal* signal);
  void execTUP_ABORTREQ(Signal* signal);
  void execNDB_STTOR(Signal* signal);
  void execREAD_CONFIG_REQ(Signal* signal);
  void execDROP_TAB_REQ(Signal* signal);
  void execALTER_TAB_REQ(Signal* signal);
  void execTUP_DEALLOCREQ(Signal* signal);
  void execTUP_WRITELOG_REQ(Signal* signal);
  void execNODE_FAILREP(Signal* signal);

  void execDROP_FRAG_REQ(Signal*);

  // Ordered index related
  void execBUILD_INDX_IMPL_REQ(Signal* signal);
  void execBUILD_INDX_IMPL_REF(Signal* signal);
  void execBUILD_INDX_IMPL_CONF(Signal* signal);
  void buildIndex(Signal* signal, Uint32 buildPtrI);
  void buildIndexReply(Signal* signal, const BuildIndexRec* buildRec);
  void buildIndexOffline(Signal* signal, Uint32 buildPtrI);
  void buildIndexOffline_table_readonly(Signal* signal, Uint32 buildPtrI);
  void execALTER_TAB_CONF(Signal*);

  // Tup scan
  void execACC_SCANREQ(Signal* signal);
  void execNEXT_SCANREQ(Signal* signal);
  void execACC_CHECK_SCAN(Signal* signal);
  void execACCKEYCONF(Signal* signal);
  void execACCKEYREF(Signal* signal);
  void execACC_ABORTCONF(Signal* signal);


  // Drop table
  void execFSREMOVEREF(Signal*);
  void execFSREMOVECONF(Signal*);
  void execFSOPENREF(Signal*);
  void execFSOPENCONF(Signal*);
  void execFSREADREF(Signal*);
  void execFSREADCONF(Signal*);
  void execFSCLOSEREF(Signal*);
  void execFSCLOSECONF(Signal*);

  void execDBINFO_SCANREQ(Signal*);
  void execSUB_GCP_COMPLETE_REP(Signal*);

//------------------------------------------------------------------
//------------------------------------------------------------------
// Methods to handle execution of TUPKEYREQ + ATTRINFO.
//
// Module Execution Manager
//
// The TUPKEYREQ signal is central to this block. This signal is used
// by everybody that needs to read data residing in DBTUP. The data is
// read using an interpreter approach.
//
// Operations only needing to read execute a simplified version of the
// interpreter where the only instruction is read Attribute to send.
// Operations only needing to update the record (insert or update)
// execute a simplified version of the interpreter where the only
// instruction is write Attribute.
//
// Currently TUPKEYREQ is used in the following situations.
// 1) Normal transaction execution. Can be any of the types described
//    below.
// 2) Execution of fragment redo log during system restart.
//    In this situation there will only be normal updates, inserts
//    and deletes performed.
// 3) A special type of normal transaction execution is to write the
//    records arriving from the primary replica in the node restart
//    processing. This will always be normal write operations which
//    are translated to inserts or updates before arriving to TUP.
// 4) Scan processing. The scan processing will use normal reads or
//    interpreted reads in their execution. There will be one TUPKEYREQ
//    signal for each record processed.
// 5) Copy fragment processing. This is a special type of scan used in the
//    primary replica at system restart. It reads the entire reads and
//    converts those to writes to the starting node. In this special case
//    LQH acts as an API node and receives also the ATTRINFO sent in the
//    TRANSID_AI signals.
//
// Signal Diagram:
//
// In Signals:
// -----------
//
// ---> TUPKEYREQ
// A single TUPKEYREQ is received.  The TUPKEYREQ can contain an I-value
// for a long section containing AttrInfo words.  Delete requests usually
// contain no AttrInfo, and requests referencing a stored procedure (e.g.
// scan originated requests) do not contain AttrInfo.
// 
// The total size of the ATTRINFO is not allowed to be more than 16384 words.
// There is always one and only one TUPKEYREQ.
//
// Response Signals (successful case):
//
// Simple/Dirty Read Operation
// ---------------------------
//
// <---- TRANSID_AI (to API)
// ...
// <---- TRANSID_AI (to API)
// <---- READCONF   (to API)
// <---- TUPKEYCONF (to LQH)
// There is always exactly one READCONF25 sent last. The number of
// TRANSID_AI is dependent on how much that was read. The maximum size
// of the ATTRINFO sent back is 16384 words. The signals are sent
// directly to the application with an address provided by the
// TUPKEYREQ signal.
// A positive response signal is also sent to LQH.
//
// Normal Read Operation
// ---------------------
//
// <---- TRANSID_AI (to API)
// ...
// <---- TRANSID_AI (to API)
// <---- TUPKEYCONF (to LQH)
// The number of TRANSID_AI is dependent on how much that was read.
// The maximum size of the ATTRINFO sent back is 16384 words. The
// signals are sent directly to the application with an address
// provided by the TUPKEYREQ signal.
// A positive response signal is also sent to LQH.
//
// Normal update/insert/delete operation
// -------------------------------------
//
// <---- TUPKEYCONF
// After successful updating of the tuple LQH is informed of this.
//
// Delete with read
// ----------------
//
// Will behave as a normal read although it also prepares the
// deletion of the tuple.
//
// Interpreted Update
// ------------------
//
// <---- TRANSID_AI (to API)
// ...
// <---- TRANSID_AI (to API)
// <---- TUP_ATTRINFO (to LQH)
// ...
// <---- TUP_ATTRINFO (to LQH)
// <---- TUPKEYCONF (to LQH)
//
// The interpreted Update contains five sections:
// The first section performs read Attribute operations
// that send results back to the API.
//
// The second section executes the interpreted program
// where data from attributes can be updated and it
// can also read attribute values into the registers.
//
// The third section performs unconditional updates of
// attributes.
//
// The fourth section can read the attributes to be sent to the
// API after updating the record.
//
// The fifth section contains subroutines used by the interpreter
// in the second section.
//
// All types of interpreted programs contains the same five sections.
// The only difference is that only interpreted updates can update
// attributes. Interpreted inserts are not allowed.
//
// Interpreted Updates have to send back the information about the
// attributes they have updated. This information will be shipped to
// the log and also to any other replicas. Thus interpreted updates
// are only performed in the primary replica. The fragment redo log
// in LQH will contain information so that normal update/inserts/deletes
// can be performed using TUPKEYREQ.
//
// Interpreted Read
// ----------------
//
// From a signalling point of view the Interpreted Read behaves as
// as a Normal Read. The interpreted Read is often used by Scan's.
//
// Interpreted Delete
// ------------------
//
// <---- TUPKEYCONF
// After successful prepartion to delete the tuple LQH is informed
// of this.
//
// Interpreted Delete with Read
// ----------------------------
//
// From a signalling point of view an interpreted delete with read
// behaves as a normal read.
//
// Continuation after successful case:
//
// After a read of any kind the operation record is ready to be used
// again by a new operation.
//
// Any updates, inserts or deletes waits for either of two messages.
// A commit specifying that the operation is to be performed for real
// or an abort specifying that the operation is to be rolled back and
// the record to be restored in its original format.
// 
// This is handled by the module Transaction Manager.
//
// Response Signals (unsuccessful case):
//
// <---- TUPKEYREF (to LQH)
// A signal is sent back to LQH informing about the unsuccessful
// operation. In this case TUP waits for an abort signal to arrive
// before the operation record is ready for the next operation.
// This is handled by the Transaction Manager.
//------------------------------------------------------------------
//------------------------------------------------------------------

// *****************************************************************
// Signal Reception methods.
// *****************************************************************
//------------------------------------------------------------------
//------------------------------------------------------------------
public:
  bool execTUPKEYREQ(Signal* signal);
  /**
   * Prepare for execTUPKEYREQ by prefetching row and preparing
   * some variables as part of row address calculation.
   */
  void prepareTUPKEYREQ(Uint32 page_id,
                        Uint32 page_idx,
                        Uint32 frag_id);
  void prepare_scanTUPKEYREQ(Uint32 page_id, Uint32 page_idx);
  void prepare_scan_tux_TUPKEYREQ(Uint32 page_id, Uint32 page_idx);
  void prepare_op_pointer(Uint32 opPtrI);
  void prepare_tab_pointers(Uint32 fragPtrI);
  Uint32 get_current_frag_page_id();
private:
  void disk_page_load_callback(Signal*, Uint32 op, Uint32 page);
  void disk_page_load_scan_callback(Signal*, Uint32 op, Uint32 page);

private:

// Trigger signals
//------------------------------------------------------------------
//------------------------------------------------------------------
  void execCREATE_TRIG_IMPL_REQ(Signal* signal);

//------------------------------------------------------------------
//------------------------------------------------------------------
  void execDROP_TRIG_IMPL_REQ(Signal* signal);

  /**
   * Deferred triggers execute when execFIRE_TRIG_REQ
   *   is called
   */
  void execFIRE_TRIG_REQ(Signal* signal);

// *****************************************************************
// Setting up the environment for reads, inserts, updates and deletes.
// *****************************************************************
//------------------------------------------------------------------
//------------------------------------------------------------------
  int handleReadReq(Signal* signal,
                    Operationrec* regOperPtr,
                    Tablerec* regTabPtr,
                    KeyReqStruct* req_struct);

//------------------------------------------------------------------
//------------------------------------------------------------------
  int handleUpdateReq(Signal* signal,
                      Operationrec* regOperPtr,
                      Fragrecord* regFragPtr,
                      Tablerec* regTabPtr,
                      KeyReqStruct* req_struct,
		      bool disk);

//------------------------------------------------------------------
//------------------------------------------------------------------
  int handleInsertReq(Signal* signal,
                      Ptr<Operationrec> regOperPtr,
                      Ptr<Fragrecord>,
                      Tablerec* regTabPtr,
                      KeyReqStruct* req_struct,
                      Local_key ** accminupdateptr);

//------------------------------------------------------------------
//------------------------------------------------------------------
  int handleDeleteReq(Signal* signal,
                      Operationrec* regOperPtr,
                      Fragrecord* regFragPtr,
                      Tablerec* regTabPtr,
                      KeyReqStruct* req_struct,
		      bool disk);

  int handleRefreshReq(Signal* signal,
                       Ptr<Operationrec>,
                       Ptr<Fragrecord>,
                       Tablerec*,
                       KeyReqStruct*,
                       bool disk);

//------------------------------------------------------------------
//------------------------------------------------------------------
  int  updateStartLab(Signal* signal,
                      Operationrec* regOperPtr,
                      Fragrecord* regFragPtr,
                      Tablerec* regTabPtr,
                      KeyReqStruct* req_struct);

// *****************************************************************
// Interpreter Handling methods.
// *****************************************************************

//------------------------------------------------------------------
//------------------------------------------------------------------
  int interpreterStartLab(Signal* signal,
                          KeyReqStruct *req_struct);

//------------------------------------------------------------------
//------------------------------------------------------------------
  Uint32 brancher(Uint32, Uint32);
  int interpreterNextLab(Signal* signal,
                         KeyReqStruct *req_struct,
                         Uint32* logMemory,
                         Uint32* mainProgram,
                         Uint32 TmainProgLen,
                         Uint32* subroutineProg,
                         Uint32 TsubroutineLen,
			 Uint32 * tmpArea,
			 Uint32 tmpAreaSz);

  const Uint32 * lookupInterpreterParameter(Uint32 paramNo,
                                            const Uint32 * subptr,
                                            Uint32 sublen) const;

// *****************************************************************
// Signal Sending methods.
// *****************************************************************
//------------------------------------------------------------------
//------------------------------------------------------------------
  void sendReadAttrinfo(Signal* signal,
                        KeyReqStruct *req_struct,
                        Uint32 TnoOfData);

//------------------------------------------------------------------
//------------------------------------------------------------------
  int sendLogAttrinfo(Signal* signal,
                      KeyReqStruct *req_struct,
                      Uint32 TlogSize,
                      Operationrec * regOperPtr);

//------------------------------------------------------------------
//------------------------------------------------------------------
  void returnTUPKEYCONF(Signal* signal,
                        KeyReqStruct *req_struct,
                        Operationrec * regOperPtr,
                        TransState trans_state);

//------------------------------------------------------------------
//------------------------------------------------------------------
// *****************************************************************
// The methods that perform the actual read and update of attributes
// in the tuple.
// *****************************************************************
//------------------------------------------------------------------
//------------------------------------------------------------------
  int readAttributes(KeyReqStruct* req_struct,
                     const Uint32*  inBuffer,
                     Uint32   inBufLen,
                     Uint32*  outBuffer,
                     Uint32   TmaxRead,
                     bool     xfrmFlag);

//------------------------------------------------------------------
//------------------------------------------------------------------
  int updateAttributes(KeyReqStruct *req_struct,
                       Uint32*     inBuffer,
                       Uint32      inBufLen);

//------------------------------------------------------------------
//------------------------------------------------------------------
  bool readFixedSizeTHOneWordNotNULL(Uint8* outBuffer,
                                     KeyReqStruct *req_struct,
                                     AttributeHeader* ahOut,
                                     Uint32  attrDes2);

//------------------------------------------------------------------
//------------------------------------------------------------------
  bool updateFixedSizeTHOneWordNotNULL(Uint32* inBuffer,
                                       KeyReqStruct *req_struct,
                                       Uint32  attrDes2);

//------------------------------------------------------------------
//------------------------------------------------------------------
  bool readFixedSizeTHTwoWordNotNULL(Uint8* outBuffer,
                                     KeyReqStruct *req_struct,
                                     AttributeHeader* ahOut,
                                     Uint32  attrDes2);

//------------------------------------------------------------------
//------------------------------------------------------------------
  bool updateFixedSizeTHTwoWordNotNULL(Uint32* inBuffer,
                                       KeyReqStruct *req_struct,
                                       Uint32  attrDes2);

//------------------------------------------------------------------
//------------------------------------------------------------------
  bool readFixedSizeTHManyWordNotNULL(Uint8* outBuffer,
                                      KeyReqStruct *req_struct,
                                      AttributeHeader* ahOut,
                                      Uint32  attrDes2);

//------------------------------------------------------------------
//------------------------------------------------------------------
  bool fixsize_updater(Uint32* inBuffer,
                       KeyReqStruct *req_struct,
                       Uint32  attrDes2,
                       Uint32 *dst_ptr,
                       Uint32 updateOffset,
                       Uint32 checkOffset);
  bool updateFixedSizeTHManyWordNotNULL(Uint32* inBuffer,
                                        KeyReqStruct *req_struct,
                                        Uint32  attrDes2);

//------------------------------------------------------------------
//------------------------------------------------------------------
  bool readFixedSizeTHOneWordNULLable(Uint8* outBuffer,
                                      KeyReqStruct *req_struct,
                                      AttributeHeader* ahOut,
                                      Uint32  attrDes2);

//------------------------------------------------------------------
//------------------------------------------------------------------
  bool updateFixedSizeTHOneWordNULLable(Uint32* inBuffer,
                                        KeyReqStruct *req_struct,
                                        Uint32  attrDes2);

//------------------------------------------------------------------
//------------------------------------------------------------------
  bool readFixedSizeTHTwoWordNULLable(Uint8* outBuffer,
                                      KeyReqStruct *req_struct,
                                      AttributeHeader* ahOut,
                                      Uint32  attrDes2);

//------------------------------------------------------------------
//------------------------------------------------------------------
  bool updateFixedSizeTHTwoWordNULLable(Uint32* inBuffer,
                                        KeyReqStruct *req_struct,
                                        Uint32  attrDes2);

//------------------------------------------------------------------
//------------------------------------------------------------------
  bool readFixedSizeTHManyWordNULLable(Uint8* outBuffer,
                                       KeyReqStruct *req_struct,
                                       AttributeHeader* ahOut,
                                       Uint32  attrDes2);

//------------------------------------------------------------------
//------------------------------------------------------------------
  bool readFixedSizeTHZeroWordNULLable(Uint8* outBuffer,
                                       KeyReqStruct *req_struct,
                                       AttributeHeader* ahOut,
                                       Uint32  attrDes2);
//------------------------------------------------------------------
//------------------------------------------------------------------
  bool updateFixedSizeTHManyWordNULLable(Uint32* inBuffer,
                                         KeyReqStruct *req_struct,
                                         Uint32  attrDes2);

//------------------------------------------------------------------
//------------------------------------------------------------------
  bool varsize_reader(Uint8* out_buffer,
                      KeyReqStruct *req_struct,
                      AttributeHeader* ah_out,
                      Uint32  attr_des2,
                      const void* src_ptr,
                      Uint32 vsize_in_bytes);
  
  bool xfrm_reader(Uint8* out_buffer,
                   KeyReqStruct *req_struct,
                   AttributeHeader* ah_out,
                   Uint32  attr_des2,
                   const void* src_ptr,
                   Uint32 srcBytes);

  bool bits_reader(Uint8* out_buffer,
                   KeyReqStruct *req_struct,
                   AttributeHeader* ah_out,
                   const Uint32* bm_ptr, Uint32 bm_len,
                   Uint32 bitPos, Uint32 bitCnt);
  
  bool varsize_updater(Uint32* in_buffer,
                       KeyReqStruct *req_struct,
                       char *var_data_start,
                       Uint32 var_attr_pos,
                       Uint16 *len_offset_ptr,
                       Uint32 check_offset);
//------------------------------------------------------------------
//------------------------------------------------------------------
  bool readVarSizeNotNULL(Uint8* outBuffer,
                          KeyReqStruct *req_struct,
                          AttributeHeader* ahOut,
                          Uint32  attrDes2);

//------------------------------------------------------------------
//------------------------------------------------------------------
  bool updateVarSizeNotNULL(Uint32* inBuffer,
                            KeyReqStruct *req_struct,
                            Uint32  attrDes2);

//------------------------------------------------------------------
//------------------------------------------------------------------
  bool readVarSizeNULLable(Uint8* outBuffer,
                           KeyReqStruct *req_struct,
                           AttributeHeader* ahOut,
                           Uint32  attrDes2);

//------------------------------------------------------------------
//------------------------------------------------------------------
  bool updateVarSizeNULLable(Uint32* inBuffer,
                             KeyReqStruct *req_struct,
                             Uint32  attrDes2);

//------------------------------------------------------------------
//------------------------------------------------------------------
  bool readDynFixedSizeNotNULL(Uint8* outBuffer,
                               KeyReqStruct *req_struct,
                               AttributeHeader* ahOut,
                               Uint32  attrDes2);
  bool readDynFixedSizeNULLable(Uint8* outBuffer,
                                KeyReqStruct *req_struct,
                                AttributeHeader* ahOut,
                                Uint32  attrDes2);
  bool readDynFixedSizeExpandedNotNULL(Uint8* outBuffer,
                                       KeyReqStruct *req_struct,
                                       AttributeHeader* ahOut,
                                       Uint32  attrDes2);
  bool readDynFixedSizeShrunkenNotNULL(Uint8* outBuffer,
                                       KeyReqStruct *req_struct,
                                       AttributeHeader* ahOut,
                                       Uint32  attrDes2);
  bool readDynFixedSizeExpandedNULLable(Uint8* outBuffer,
                                        KeyReqStruct *req_struct,
                                        AttributeHeader* ahOut,
                                        Uint32  attrDes2);
  bool readDynFixedSizeShrunkenNULLable(Uint8* outBuffer,
                                        KeyReqStruct *req_struct,
                                        AttributeHeader* ahOut,
                                        Uint32  attrDes2);

//------------------------------------------------------------------
//------------------------------------------------------------------
  bool updateDynFixedSizeNotNULL(Uint32* inBuffer,
                                 KeyReqStruct *req_struct,
                                 Uint32  attrDes2);
  bool updateDynFixedSizeNULLable(Uint32* inBuffer,
                                  KeyReqStruct *req_struct,
                                  Uint32  attrDes2);

//------------------------------------------------------------------
//------------------------------------------------------------------
  bool readDynBigFixedSizeNotNULL(Uint8* outBuffer,
                                  KeyReqStruct *req_struct,
                                  AttributeHeader* ahOut,
                                  Uint32  attrDes2);
  bool readDynBigFixedSizeNULLable(Uint8* outBuffer,
                                   KeyReqStruct *req_struct,
                                   AttributeHeader* ahOut,
                                   Uint32  attrDes2);
  bool readDynBigFixedSizeExpandedNotNULL(Uint8* outBuffer,
                                          KeyReqStruct *req_struct,
                                          AttributeHeader* ahOut,
                                          Uint32  attrDes2);
  bool readDynBigFixedSizeShrunkenNotNULL(Uint8* outBuffer,
                                          KeyReqStruct *req_struct,
                                          AttributeHeader* ahOut,
                                          Uint32  attrDes2);
  bool readDynBigFixedSizeExpandedNULLable(Uint8* outBuffer,
                                           KeyReqStruct *req_struct,
                                           AttributeHeader* ahOut,
                                           Uint32  attrDes2);
  bool readDynBigFixedSizeShrunkenNULLable(Uint8* outBuffer,
                                           KeyReqStruct *req_struct,
                                           AttributeHeader* ahOut,
                                           Uint32  attrDes2);

//------------------------------------------------------------------
//------------------------------------------------------------------
  bool updateDynBigFixedSizeNotNULL(Uint32* inBuffer,
                                    KeyReqStruct *req_struct,
                                    Uint32  attrDes2);
  bool updateDynBigFixedSizeNULLable(Uint32* inBuffer,
                                     KeyReqStruct *req_struct,
                                     Uint32  attrDes2);

//------------------------------------------------------------------
//------------------------------------------------------------------
  bool readDynBitsNotNULL(Uint8* outBuffer,
                          KeyReqStruct *req_struct,
                          AttributeHeader* ahOut,
                          Uint32  attrDes2);
  bool readDynBitsNULLable(Uint8* outBuffer,
                           KeyReqStruct *req_struct,
                           AttributeHeader* ahOut,
                           Uint32  attrDes2);
  bool readDynBitsExpandedNotNULL(Uint8* outBuffer,
                                  KeyReqStruct *req_struct,
                                  AttributeHeader* ahOut,
                                  Uint32  attrDes2);
  bool readDynBitsShrunkenNotNULL(Uint8* outBuffer,
                                  KeyReqStruct *req_struct,
                                  AttributeHeader* ahOut,
                                  Uint32  attrDes2);
  bool readDynBitsExpandedNULLable(Uint8* outBuffer,
                                   KeyReqStruct *req_struct,
                                   AttributeHeader* ahOut,
                                   Uint32  attrDes2);
  bool readDynBitsShrunkenNULLable(Uint8* outBuffer,
                                   KeyReqStruct *req_struct,
                                   AttributeHeader* ahOut,
                                   Uint32  attrDes2);

//------------------------------------------------------------------
//------------------------------------------------------------------
  bool updateDynBitsNotNULL(Uint32* inBuffer,
                            KeyReqStruct *req_struct,
                            Uint32  attrDes2);
  bool updateDynBitsNULLable(Uint32* inBuffer,
                             KeyReqStruct *req_struct,
                             Uint32  attrDes2);

//------------------------------------------------------------------
//------------------------------------------------------------------
  bool readDynVarSizeNotNULL(Uint8* outBuffer,
                             KeyReqStruct *req_struct,
                             AttributeHeader* ahOut,
                             Uint32  attrDes2);
  bool readDynVarSizeNULLable(Uint8* outBuffer,
                              KeyReqStruct *req_struct,
                              AttributeHeader* ahOut,
                              Uint32  attrDes2);
  bool readDynVarSizeExpandedNotNULL(Uint8* outBuffer,
                                     KeyReqStruct *req_struct,
                                     AttributeHeader* ahOut,
                                     Uint32  attrDes2);
  bool readDynVarSizeShrunkenNotNULL(Uint8* outBuffer,
                                     KeyReqStruct *req_struct,
                                     AttributeHeader* ahOut,
                                     Uint32  attrDes2);
  bool readDynVarSizeExpandedNULLable(Uint8* outBuffer,
                                      KeyReqStruct *req_struct,
                                      AttributeHeader* ahOut,
                                      Uint32  attrDes2);
  bool readDynVarSizeShrunkenNULLable(Uint8* outBuffer,
                                      KeyReqStruct *req_struct,
                                      AttributeHeader* ahOut,
                                      Uint32  attrDes2);

//------------------------------------------------------------------
//------------------------------------------------------------------
  bool updateDynVarSizeNotNULL(Uint32* inBuffer,
                               KeyReqStruct *req_struct,
                               Uint32  attrDes2);
  bool updateDynVarSizeNULLable(Uint32* inBuffer,
                                KeyReqStruct *req_struct,
                                Uint32  attrDes2);

  bool readCharNotNULL(Uint8* outBuffer,
                       KeyReqStruct *req_struct,
                       AttributeHeader* ahOut,
                       Uint32  attrDes2);

  bool readCharNULLable(Uint8* outBuffer,
                        KeyReqStruct *req_struct,
                        AttributeHeader* ahOut,
                        Uint32  attrDes2);

  bool readBitsNULLable(Uint8* outBuffer, KeyReqStruct *req_struct, AttributeHeader*, Uint32);
  bool updateBitsNULLable(Uint32* inBuffer, KeyReqStruct *req_struct, Uint32);
  bool readBitsNotNULL(Uint8* outBuffer, KeyReqStruct *req_struct, AttributeHeader*, Uint32);
  bool updateBitsNotNULL(Uint32* inBuffer, KeyReqStruct *req_struct, Uint32);

  bool updateFixedNULLable(Uint32* inBuffer, KeyReqStruct *req_struct, Uint32);
  bool updateFixedNotNull(Uint32* inBuffer, KeyReqStruct *req_struct, Uint32);

  bool updateVarNULLable(Uint32* inBuffer, KeyReqStruct *req_struct, Uint32);
  bool updateVarNotNull(Uint32* inBuffer, KeyReqStruct *req_struct, Uint32);


  bool readDiskFixedSizeNotNULL(Uint8* outBuffer,
				KeyReqStruct *req_struct,
				AttributeHeader* ahOut,
				Uint32  attrDes2);
  
  bool readDiskFixedSizeNULLable(Uint8* outBuffer,
				 KeyReqStruct *req_struct,
				 AttributeHeader* ahOut,
				 Uint32  attrDes2);

  bool readDiskVarAsFixedSizeNotNULL(Uint8* outBuffer,
				KeyReqStruct *req_struct,
				AttributeHeader* ahOut,
				Uint32  attrDes2);
  
  bool readDiskVarAsFixedSizeNULLable(Uint8* outBuffer,
				 KeyReqStruct *req_struct,
				 AttributeHeader* ahOut,
				 Uint32  attrDes2);
  bool readDiskVarSizeNULLable(Uint8*, KeyReqStruct*, AttributeHeader*,Uint32);
  bool readDiskVarSizeNotNULL(Uint8*, KeyReqStruct*, AttributeHeader*, Uint32);

  bool updateDiskFixedSizeNULLable(Uint32*, KeyReqStruct*, Uint32);
  bool updateDiskFixedSizeNotNULL(Uint32*, KeyReqStruct*, Uint32);

  bool updateDiskVarAsFixedSizeNULLable(Uint32*, KeyReqStruct*, Uint32);
  bool updateDiskVarAsFixedSizeNotNULL(Uint32*, KeyReqStruct*, Uint32);

  bool updateDiskVarSizeNULLable(Uint32*, KeyReqStruct *, Uint32);
  bool updateDiskVarSizeNotNULL(Uint32*, KeyReqStruct *, Uint32);
  
  bool readDiskBitsNULLable(Uint8*, KeyReqStruct*, AttributeHeader*, Uint32);
  bool readDiskBitsNotNULL(Uint8*, KeyReqStruct*, AttributeHeader*, Uint32);
  bool updateDiskBitsNULLable(Uint32*, KeyReqStruct*, Uint32);
  bool updateDiskBitsNotNULL(Uint32*, KeyReqStruct*, Uint32);


  /* Alter table methods. */
  void handleAlterTablePrepare(Signal *, const AlterTabReq *, const Tablerec *);
  void handleAlterTableCommit(Signal *, const AlterTabReq *, Tablerec *);
  void handleAlterTableComplete(Signal *, const AlterTabReq *, Tablerec *);
  void handleAlterTableAbort(Signal *, const AlterTabReq *, const Tablerec *);
  void sendAlterTabRef(Signal *signal, Uint32 errorCode);
  void sendAlterTabConf(Signal *, Uint32 clientData=RNIL);

  void handleCharsetPos(Uint32 csNumber, CHARSET_INFO** charsetArray,
                        Uint32 noOfCharsets,
                        Uint32 & charsetIndex, Uint32 & attrDes2);
  Uint32 computeTableMetaData(Tablerec *regTabPtr);

//------------------------------------------------------------------
//------------------------------------------------------------------
  bool nullFlagCheck(KeyReqStruct *req_struct, Uint32  attrDes2);
  bool disk_nullFlagCheck(KeyReqStruct *req_struct, Uint32 attrDes2);
  int read_pseudo(const Uint32 *, Uint32, KeyReqStruct*, Uint32*);
  Uint32 read_packed(const Uint32 *, Uint32, KeyReqStruct*, Uint32*);
  Uint32 update_packed(KeyReqStruct*, const Uint32* src);

  Uint32 read_lcp(const Uint32 *, Uint32, KeyReqStruct*, Uint32*);
  void update_lcp(KeyReqStruct *req_struct, const Uint32* src, Uint32 len);

  void flush_read_buffer(KeyReqStruct *, const Uint32* outBuf,
			 Uint32 resultRef, Uint32 resultData, Uint32 routeRef);
public:
  Uint32 copyAttrinfo(Uint32 storedProcId);
  void copyAttrinfo(Uint32 expectedLen,
                    Uint32 attrInfoIVal);
  /**
   * Used by Restore...
   */
  Uint32 read_lcp_keys(Uint32, const Uint32 * src, Uint32 len, Uint32 *dst);
private:

//------------------------------------------------------------------
//------------------------------------------------------------------
  void setUpQueryRoutines(Tablerec* regTabPtr);

// *****************************************************************
// Service methods.
// *****************************************************************
  TransState get_trans_state(Operationrec * const);
  void set_trans_state(Operationrec * const, TransState);
  TupleState get_tuple_state(Operationrec * const);
  void set_tuple_state(Operationrec * const, TupleState);
  Uint32 get_fix_page_offset(Uint32 page_index, Uint32 tuple_size);

  Uint32 decr_tup_version(Uint32 tuple_version);
  void update_change_mask_info(const Tablerec*, ChangeMask* dst, const Uint32*src);
  void set_change_mask_info(const Tablerec*, ChangeMask* dst);
  void clear_change_mask_info(const Tablerec*, ChangeMask* dst);
  void copy_change_mask_info(const Tablerec*, ChangeMask* dst, const ChangeMask * src);
  void set_commit_change_mask_info(const Tablerec*,
                                   KeyReqStruct*,
                                   const Operationrec*);

//------------------------------------------------------------------
//------------------------------------------------------------------
  void initOpConnection(Operationrec* regOperPtr);

//------------------------------------------------------------------
//------------------------------------------------------------------
  void initOperationrec(Signal* signal);

//------------------------------------------------------------------
//------------------------------------------------------------------
  void getStoredProcAttrInfo(Uint32 storedId,
                             KeyReqStruct* req_struct,
                             Uint32& attrInfoIVal);

//------------------------------------------------------------------
//------------------------------------------------------------------
  bool insertActiveOpList(OperationrecPtr, KeyReqStruct* req_struct);

//------------------------------------------------------------------
//------------------------------------------------------------------

  int  store_default_record(const TablerecPtr& regTabPtr);
  bool  receive_defvalue(Signal* signal, const TablerecPtr& regTabPtr);
//------------------------------------------------------------------
//------------------------------------------------------------------
  void bufferTRANSID_AI(Signal* signal, BlockReference aRef, 
                        const Uint32 *dataBuf,
                        Uint32 lenOfData);

  void sendAPI_TRANSID_AI(Signal* signal,
                          BlockReference recBlockRef,
                          const Uint32 *dataBuf,
                          Uint32 lenOfData);

//------------------------------------------------------------------
// Trigger handling routines
//------------------------------------------------------------------
  TupTriggerData_list*
  findTriggerList(Tablerec* table,
                  TriggerType::Value ttype,
                  TriggerActionTime::Value ttime,
                  TriggerEvent::Value tevent);

  bool createTrigger(Tablerec*, const CreateTrigImplReq*, const AttributeMask&);

  Uint32 dropTrigger(Tablerec* table,
		     const DropTrigImplReq* req,
		     BlockNumber sender);

  Uint32 getOldTriggerId(const TupTriggerData*, Uint32 op);

  void
  checkImmediateTriggersAfterInsert(KeyReqStruct *req_struct,
                                    Operationrec* regOperPtr, 
                                    Tablerec* tablePtr,
                                    bool disk);

  void
  checkImmediateTriggersAfterUpdate(KeyReqStruct *req_struct,
                                    Operationrec* regOperPtr, 
                                    Tablerec* tablePtr,
                                    bool disk);

  void
  checkImmediateTriggersAfterDelete(KeyReqStruct *req_struct,
                                    Operationrec* regOperPtr, 
                                    Tablerec* tablePtr,
                                    bool disk);

  void checkDeferredTriggers(KeyReqStruct *req_struct,
                             Operationrec* regOperPtr,
                             Tablerec* regTablePtr,
                             bool disk);

  void checkDetachedTriggers(KeyReqStruct *req_struct,
                             Operationrec* regOperPtr,
                             Tablerec* regTablePtr,
                             bool disk,
                             Uint32 diskPagePtrI);

  void fireImmediateTriggers(KeyReqStruct *req_struct,
                             TupTriggerData_list& triggerList,
                             Operationrec* regOperPtr,
                             bool disk);

  void checkDeferredTriggersDuringPrepare(KeyReqStruct *req_struct,
                                          TupTriggerData_list& triggerList,
                                          Operationrec* const regOperPtr,
                                          bool disk);
  void fireDeferredTriggers(KeyReqStruct *req_struct,
                            TupTriggerData_list& triggerList,
                            Operationrec* const regOperPtr,
                            bool disk);

  void fireDeferredConstraints(KeyReqStruct *req_struct,
                               TupTriggerData_list& triggerList,
                               Operationrec* const regOperPtr,
                               bool disk);

  void fireDetachedTriggers(KeyReqStruct *req_struct,
                            TupTriggerData_list& triggerList,
                            Operationrec* regOperPtr,
                            bool disk,
                            Uint32 diskPagePtrI);

  void executeTrigger(KeyReqStruct *req_struct,
                      TupTriggerData* trigPtr, 
                      Operationrec* regOperPtr,
                      bool disk);

  bool check_fire_trigger(const Fragrecord*,
                          const TupTriggerData*,
                          const KeyReqStruct*,
                          const Operationrec*) const;

  bool check_fire_reorg(const KeyReqStruct *, Fragrecord::FragState) const;
  bool check_fire_fully_replicated(const KeyReqStruct *,
                                   Fragrecord::FragState) const;
  bool check_fire_suma(const KeyReqStruct *,
                       const Operationrec*,
                       const Fragrecord*) const;

  bool readTriggerInfo(TupTriggerData* trigPtr,
                       Operationrec* regOperPtr,
                       KeyReqStruct * req_struct,
                       Fragrecord* regFragPtr,
                       Uint32* keyBuffer,
                       Uint32& noPrimKey,
                       Uint32* afterBuffer,
                       Uint32& noAfterWords,
                       Uint32* beforeBuffer,
                       Uint32& noBeforeWords,
                       bool disk);
  
  void sendTrigAttrInfo(Signal*        signal, 
                        Uint32*        data, 
                        Uint32         dataLen,
                        bool           executeDirect,
                        BlockReference receiverReference);

  Uint32 setAttrIds(Bitmask<MAXNROFATTRIBUTESINWORDS>& attributeMask, 
                    Uint32 noOfAttributes, 
                    Uint32* inBuffer);

  bool primaryKey(Tablerec* const, Uint32);

  // these set terrorCode and return non-zero on error

  int executeTuxInsertTriggers(Signal* signal, 
                               Operationrec* regOperPtr,
                               Fragrecord* regFragPtr,
                               Tablerec* regTabPtr);

  int executeTuxUpdateTriggers(Signal* signal, 
                               Operationrec* regOperPtr,
                               Fragrecord* regFragPtr,
                               Tablerec* regTabPtr);

  int executeTuxDeleteTriggers(Signal* signal, 
                               Operationrec* regOperPtr,
                               Fragrecord* regFragPtr,
                               Tablerec* regTabPtr);

  int addTuxEntries(Signal* signal,
                    Operationrec* regOperPtr,
                    Tablerec* regTabPtr);

  // these crash the node on error

  void executeTuxCommitTriggers(Signal* signal, 
                                Operationrec* regOperPtr,
                                Fragrecord* regFragPtr,
                                Tablerec* regTabPtr);

  void executeTuxAbortTriggers(Signal* signal, 
                               Operationrec* regOperPtr,
                               Fragrecord* regFragPtr,
                               Tablerec* regTabPtr);

  void removeTuxEntries(Signal* signal,
                        Tablerec* regTabPtr);

  void ndbmtd_buffer_suma_trigger(Signal* signal, Uint32 len,
                                  LinearSectionPtr ptr[]);
  void flush_ndbmtd_suma_buffer(Signal*);

  struct SumaTriggerBuffer
  {
    SumaTriggerBuffer() { m_out_of_memory = 0;m_pageId = RNIL; m_freeWords = 0;}
    Uint32 m_out_of_memory;
    Uint32 m_pageId;
    Uint32 m_freeWords;
  } m_suma_trigger_buffer;

// *****************************************************************
// Error Handling routines.
// *****************************************************************
//------------------------------------------------------------------
//------------------------------------------------------------------
  int TUPKEY_abort(KeyReqStruct*, int error_type);

//------------------------------------------------------------------
//------------------------------------------------------------------
  void tupkeyErrorLab(KeyReqStruct*);
  void do_tup_abortreq(Signal*, Uint32 flags);
  void do_tup_abort_operation(Signal*, Tuple_header *,
                              Operationrec*,
                              Fragrecord*,
                              Tablerec*);

//------------------------------------------------------------------
//------------------------------------------------------------------
// Methods to handle execution of TUP_COMMITREQ + TUP_ABORTREQ.
//
// Module Transaction Manager
//
// The Transaction Manager module is responsible for the commit
// and abort of operations started by the Execution Manager.
//
// Commit Operation:
// ----------------
//
// Failures in commit processing is not allowed since that would
// leave the database in an unreliable state. Thus the only way
// to handle failures in commit processing is to crash the node.
//
// TUP_COMMITREQ can only be received in the wait state after a
// successful TUPKEYREQ which was not a read operation.
// 
// Commit of Delete:
// -----------------
//
// This will actually perform the deletion of the record unless
// other operations also are connected to the record. In this case
// we will set the delete state on the record that becomes the ownerd
// of the record.
//
// Commit of Update:
// ----------------
//
// We will release the copy record where the original record was kept.
// Also here we will take special care if more operations are updating
// the record simultaneously.
//
// Commit of Insert:
// -----------------
//
// Will simply reset the state of the operation record.
//
// Signal Diagram:
// --->  TUP_COMMITREQ (from LQH)
// <---- TUP_COMMITCONF (to LQH)
//
//
// Abort Operation:
// ----------------
//
// Signal Diagram:
// --->  TUP_ABORTREQ (from LQH)
// <---- TUP_ABORTCONF (to LQH)
//
// Failures in abort processing is not allowed since that would
// leave the database in an unreliable state. Thus the only way
// to handle failures in abort processing is to crash the node.
//
// Abort messages can arrive at any time. It can arrive even before
// anything at all have arrived of the operation. It can arrive after
// receiving a number of ATTRINFO but before TUPKEYREQ has been received.
// It must arrive after that we sent TUPKEYREF in response to TUPKEYREQ
// and finally it can arrive after successfully performing the TUPKEYREQ
// in all cases including the read case.
//------------------------------------------------------------------
//------------------------------------------------------------------

#if 0 
  void checkPages(Fragrecord* regFragPtr);
#endif
  Uint32 convert_byte_to_word_size(Uint32 byte_size)
  {
    return ((byte_size + 3) >> 2);
  }
  Uint32 convert_bit_to_word_size(Uint32 bit_size)
  {
    return ((bit_size + 31) >> 5);
  }

  void prepare_initial_insert(KeyReqStruct*, Operationrec*, Tablerec*);
  void fix_disk_insert_no_mem_insert(KeyReqStruct*, Operationrec*, Tablerec*);
  void setup_fixed_tuple_ref_opt(KeyReqStruct* req_struct);
  void setup_fixed_tuple_ref(KeyReqStruct* req_struct,
			     Operationrec* regOperPtr,
			     Tablerec* regTabPtr);
  void setup_fixed_part(KeyReqStruct* req_struct,
			Operationrec* regOperPtr,
			Tablerec* regTabPtr);
  
  void send_TUPKEYREF(const KeyReqStruct* req_struct);
  void early_tupkey_error(KeyReqStruct*);

  void printoutTuplePage(Uint32 fragid, Uint32 pageid, Uint32 printLimit);

  bool checkUpdateOfPrimaryKey(KeyReqStruct *req_struct,
                               Uint32* updateBuffer,
                               Tablerec* regTabPtr);

  void setNullBits(Uint32*, Tablerec* regTabPtr);
  bool checkNullAttributes(KeyReqStruct * const, Tablerec* const);
  bool find_savepoint(OperationrecPtr& loopOpPtr, Uint32 savepointId);
  bool setup_read(KeyReqStruct* req_struct,
		  Operationrec* regOperPtr,
		  Tablerec* regTabPtr,
		  bool disk);
  
  Uint32 calculateChecksum(Tuple_header*, const Tablerec* regTabPtr);
  void setChecksum(Tuple_header*, const Tablerec* regTabPtr);
  void setInvalidChecksum(Tuple_header*, const Tablerec* regTabPtr);
  void updateChecksum(Tuple_header *,
                      const Tablerec *,
                      Uint32 old_header,
                      Uint32 new_header);
  int corruptedTupleDetected(KeyReqStruct*, Tablerec*);

  void complexTrigger(Signal* signal,
                      KeyReqStruct *req_struct,
                      Operationrec* regOperPtr,
                      Fragrecord* regFragPtr,
                      Tablerec* regTabPtr);

  void setTupleStatesSetOpType(Operationrec* regOperPtr,
                               KeyReqStruct *req_struct,
                               Page* pagePtr,
                               Uint32& opType,
                               OperationrecPtr& firstOpPtr);

  void findBeforeValueOperation(OperationrecPtr& befOpPtr,
                                OperationrecPtr firstOpPtr);

  void updateGcpId(KeyReqStruct *req_struct,
                   Operationrec* regOperPtr,
                   Fragrecord* regFragPtr,
                   Tablerec* regTabPtr);

  void setTupleStateOnPreviousOps(Uint32 prevOpIndex);
  void copyMem(Signal* signal, Uint32 sourceIndex, Uint32 destIndex);

  void removeActiveOpList(Operationrec*  const regOperPtr, Tuple_header*);

  void updatePackedList(Uint16 ahostIndex);

  void setUpDescriptorReferences(Uint32 descriptorReference,
                                 Tablerec* regTabPtr,
                                 const Uint32* offset);
  void setupDynDescriptorReferences(Uint32 dynDescr,
                                    Tablerec* const regTabPtr,
                                    const Uint32* offset,
                                    Uint32 ind=0);
  void setUpKeyArray(Tablerec* regTabPtr);
  bool addfragtotab(Tablerec* regTabPtr, Uint32 fragId, Uint32 fragIndex);
  Uint32 get_frag_from_tab(TablerecPtr tabPtr, Uint32 fragId);
  void remove_frag_from_tab(TablerecPtr tabPtr, Uint32 fragId);
  void deleteFragTab(Tablerec* regTabPtr, Uint32 fragId);
  void abortAddFragOp(Signal* signal);
  void releaseTabDescr(Tablerec* regTabPtr);
  void getFragmentrec(FragrecordPtr& regFragPtr, Uint32 fragId, Tablerec* regTabPtr);

  void initialiseRecordsLab(Signal* signal, Uint32 switchData, Uint32, Uint32);
  void initializeCheckpointInfoRec();
  void initializeDiskBufferSegmentRecord();
  void initializeFragoperrec();
  void initializeFragrecord();
  void initializeAlterTabOperation();
  void initializeHostBuffer();
  void initializeLocalLogInfo();
  void initializeOperationrec();
  void initializePendingFileOpenInfoRecord();
  void initializeRestartInfoRec();
  void initializeTablerec();
  void initializeTabDescr();
  void initializeUndoPage();
  void initializeDefaultValuesFrag();

  void initTab(Tablerec* regTabPtr);

  void fragrefuseLab(Signal* signal, FragoperrecPtr fragOperPtr);
  void fragrefuse1Lab(Signal* signal, FragoperrecPtr fragOperPtr);
  void fragrefuse2Lab(Signal* signal, FragoperrecPtr fragOperPtr, FragrecordPtr regFragPtr);
  void fragrefuse3Lab(Signal* signal,
                      FragoperrecPtr fragOperPtr,
                      FragrecordPtr regFragPtr,
                      Tablerec* regTabPtr,
                      Uint32 fragId);
  void fragrefuse4Lab(Signal* signal,
                      FragoperrecPtr fragOperPtr,
                      FragrecordPtr regFragPtr,
                      Tablerec* regTabPtr,
                      Uint32 fragId);
  void addattrrefuseLab(Signal* signal,
                        FragrecordPtr regFragPtr,
                        FragoperrecPtr fragOperPtr,
                        Tablerec* regTabPtr,
                        Uint32 fragId);

  void releaseFragment(Signal*, Uint32, Uint32);
  void drop_fragment_free_var_pages(Signal*);
  void drop_fragment_free_pages(Signal*);
  void drop_fragment_free_extent(Signal*, TablerecPtr, FragrecordPtr, Uint32);
  void drop_fragment_free_extent_log_buffer_callback(Signal*, Uint32, Uint32);
  void drop_fragment_unmap_pages(Signal*, TablerecPtr, FragrecordPtr, Uint32);
  void drop_fragment_unmap_page_callback(Signal* signal, Uint32, Uint32);
  void drop_fragment_fsremove_init(Signal*, TablerecPtr, FragrecordPtr);
  void lcp_open_ctl_file(Signal*, Uint32, Uint32, Uint32, Uint32);
  void lcp_read_ctl_file(Signal*, Uint32, Uint32, Uint32, Uint32, Uint32);
  void lcp_close_ctl_file(Signal*, Uint32, Uint32);
  bool handle_ctl_info(TablerecPtr, FragrecordPtr, Uint32);
  void lcp_read_completed(Signal*, TablerecPtr, FragrecordPtr);
  void drop_fragment_fsremove(Signal*, TablerecPtr, FragrecordPtr);
  void drop_fragment_fsremove_done(Signal*, TablerecPtr, FragrecordPtr);

  // Initialisation
  void initData();
  void initRecords();

  // 2 words for optional GCI64 + AUTHOR info
#define EXTRA_COPY_PROC_WORDS 2
#define MAX_COPY_PROC_LEN (MAX_ATTRIBUTES_IN_TABLE + EXTRA_COPY_PROC_WORDS)


  void deleteScanProcedure(Signal* signal, Operationrec* regOperPtr);
  void allocCopyProcedure();
  void freeCopyProcedure();
  void prepareCopyProcedure(Uint32 numAttrs, Uint16 tableBits);
  void releaseCopyProcedure();
  void copyProcedure(Signal* signal,
                     TablerecPtr regTabPtr,
                     Operationrec* regOperPtr);
  void scanProcedure(Signal* signal,
                     Operationrec* regOperPtr,
                     SectionHandle* handle,
                     bool isCopy);
  void storedProcBufferSeizeErrorLab(Signal* signal,
                                     Operationrec* regOperPtr,
                                     Uint32 storedProcPtr,
                                     Uint32 errorCode);

//-----------------------------------------------------------------------------
// Table Descriptor Memory Manager
//-----------------------------------------------------------------------------

// Public methods
  Uint32 getTabDescrOffsets(Uint32, Uint32, Uint32, Uint32, Uint32*);
  Uint32 getDynTabDescrOffsets(Uint32 MaskSize, Uint32* offset);
  Uint32 allocTabDescr(Uint32 allocSize);
  void releaseTabDescr(Uint32 desc);

  void freeTabDescr(Uint32 retRef, Uint32 retNo, bool normal = true);
  Uint32 getTabDescrWord(Uint32 index);
  void setTabDescrWord(Uint32 index, Uint32 word);

// Private methods
  Uint32 sizeOfReadFunction();
  void   removeTdArea(Uint32 tabDesRef, Uint32 list);
  void   insertTdArea(Uint32 tabDesRef, Uint32 list);
  void   itdaMergeTabDescr(Uint32& retRef, Uint32& retNo, bool normal);
  void   verifytabdes();

  void seizeOpRec(OperationrecPtr& regOperPtr);
  void seizeFragrecord(FragrecordPtr& regFragPtr);
  void seizeFragoperrec(FragoperrecPtr& fragOperPtr);
  void seizeAlterTabOperation(AlterTabOperationPtr& alterTabOpPtr);
  void releaseFragoperrec(FragoperrecPtr fragOperPtr);
  void releaseFragrec(FragrecordPtr);
  void releaseAlterTabOpRec(AlterTabOperationPtr regAlterTabOpPtr);

//----------------------------------------------------------------------------
// Page Memory Manager
//----------------------------------------------------------------------------
  
// Public methods
  void allocConsPages(EmulatedJamBuffer* jamBuf,
                      Uint32 noOfPagesToAllocate,
                      Uint32& noOfPagesAllocated,
                      Uint32& allocPageRef);
  void returnCommonArea(Uint32 retPageRef, Uint32 retNo, bool locked = false);
  void initializePage();

  Uint32 nextHigherTwoLog(Uint32 input);

  Uint32 m_pages_allocated;
  Uint32 m_pages_allocated_max;

//------------------------------------------------------------------------------------------------------
// Page Mapper, convert logical page id's to physical page id's
// The page mapper also handles the pages allocated to the fragment.
//------------------------------------------------------------------------------------------------------
//
// Public methods
  Uint32 getRealpid(Fragrecord* regFragPtr, Uint32 logicalPageId);
  Uint32 getRealpidCheck(Fragrecord* regFragPtr, Uint32 logicalPageId);
  Uint32 getRealpidScan(Fragrecord* regFragPtr,
                        Uint32 logicalPageId,
                        Uint32 **next_ptr,
                        Uint32 **prev_ptr);
  void set_last_lcp_state(Fragrecord*, Uint32, bool);
  void set_last_lcp_state(Uint32*, bool);
  bool get_last_lcp_state(Uint32 *prev_ptr);
  bool get_lcp_scanned_bit(Fragrecord*, Uint32);
  bool get_lcp_scanned_bit(Uint32 *next_ptr);
  //void reset_lcp_scanned_bit(Fragrecord*, Uint32);
  void reset_lcp_scanned_bit(Uint32 *next_ptr);

  Uint32 getNoOfPages(Fragrecord* regFragPtr);
  Uint32 getEmptyPage(Fragrecord* regFragPtr);
  Uint32 allocFragPage(EmulatedJamBuffer* jamBuf,
                       Uint32 * err, 
                       Fragrecord* regFragPtr,
                       Tablerec *regTabPtr);
  Uint32 allocFragPage(Uint32 * err, Tablerec*, Fragrecord*, Uint32 page_no);
  void releaseFragPage(Fragrecord* regFragPtr,
                       Uint32 logicalPageId,
                       PagePtr);
  void rebuild_page_free_list(Signal*);
  Uint32 get_empty_var_page(Fragrecord* frag_ptr);
  void init_page(Fragrecord*, PagePtr, Uint32 page_no);
  
// Private methods
  void errorHandler(Uint32 errorCode);
  Uint32 insert_new_page_into_page_map(EmulatedJamBuffer *jamBuf,
                                       Fragrecord *fragPtrP,
                                       PagePtr pagePtr,
                                       Uint32 noOfPagesAllocated);
  Uint32 remove_first_free_from_page_map(EmulatedJamBuffer *jamBuf,
                                         Fragrecord *fragPtrP,
                                         PagePtr pagePtr);
  void remove_page_id_from_dll(Fragrecord *fragPtrP,
                               Uint32 page_no,
                               Uint32 pagePtrI,
                               Uint32 *ptr);
  void handle_lcp_skip_bit(EmulatedJamBuffer *jamBuf,
                           Fragrecord *fragPtrP,
                           PagePtr pagePtr,
                           Uint32 page_no);
  void handle_new_page(EmulatedJamBuffer *jamBuf,
                       Fragrecord *fragPtrP,
                       Tablerec *tabPtrP,
                       PagePtr pagePtr,
                       Uint32 page_no);

  void record_delete_by_pageid(Signal *signal,
                               Uint32 tableId,
                               Uint32 fragmentId,
                               ScanOp &scan,
                               Uint32 page_no,
                               Uint32 record_size,
                               bool set_scan_state);

  void record_delete_by_rowid(Signal *signal,
                              Uint32 tableId,
                              Uint32 fragmentId,
                              ScanOp &scan,
                              Local_key &key,
                              Uint32 foundGCI,
                              bool set_scan_state);

//---------------------------------------------------------------
// Variable Allocator
// Allocates and deallocates tuples of fixed size on a fragment.
//---------------------------------------------------------------
//
// Public methods

  void init_list_sizes(void);

// Private methods

  Uint32 get_alloc_page(Fragrecord* const, Uint32);
  void update_free_page_list(Fragrecord* const, Ptr<Page>);

#if 0  
  Uint32 calc_free_list(const Tablerec* regTabPtr, Uint32 sz) const {
    return regTabPtr->m_disk_alloc_info.calc_page_free_bits(sz);
  }
#endif

  Uint32 calculate_free_list_impl(Uint32) const ;
  Uint64 calculate_used_var_words(Fragrecord* fragPtr);
  void remove_free_page(Fragrecord*, Var_page*, Uint32);
  void insert_free_page(Fragrecord*, Var_page*, Uint32);

//---------------------------------------------------------------
// Fixed Allocator
// Allocates and deallocates tuples of fixed size on a fragment.
//---------------------------------------------------------------
//
// Public methods
  Uint32* alloc_var_rec(Uint32 * err,
                        Fragrecord*, Tablerec*, Uint32, Local_key*, Uint32*);
  void free_var_rec(Fragrecord*, Tablerec*, Local_key*, Ptr<Page>);
  void free_var_part(Fragrecord*, Tablerec*, Local_key*);
  Uint32* alloc_var_part(Uint32*err,Fragrecord*, Tablerec*, Uint32, Local_key*);
  Uint32 *realloc_var_part(Uint32 * err, Fragrecord*, Tablerec*,
                           PagePtr, Var_part_ref*, Uint32, Uint32);
  
  void move_var_part(Fragrecord* fragPtr, Tablerec* tabPtr, PagePtr pagePtr,
                     Var_part_ref* refptr, Uint32 size);
 
  void free_var_part(Fragrecord* fragPtr, PagePtr pagePtr, Uint32 page_idx);

  void validate_page(Tablerec*, Var_page* page);
  
  Uint32* alloc_fix_rec(EmulatedJamBuffer* jamBuf, Uint32* err,
                        Fragrecord*const, Tablerec*const, Local_key*,
                        Uint32*);
  void free_fix_rec(Fragrecord*, Tablerec*, Local_key*, Fix_page*);
  
  Uint32* alloc_fix_rowid(Uint32 * err,
                          Fragrecord*, Tablerec*, Local_key*, Uint32 *);
  Uint32* alloc_var_rowid(Uint32 * err,
                          Fragrecord*, Tablerec*, Uint32, Local_key*, Uint32*);
// Private methods
  void convertThPage(Fix_page* regPagePtr,
		     Tablerec*,
		     Uint32 mm);

  /**
   * Return offset
   */
  Uint32 alloc_tuple_from_page(Fragrecord* regFragPtr,
			       Fix_page* regPagePtr);
  
//---------------------------------------------------------------
// Temporary variables used for storing commonly used variables
// in certain modules
//---------------------------------------------------------------

  Uint32 c_lcp_scan_op;

// readAttributes and updateAttributes module
//------------------------------------------------------------------------------------------------------
// Common stored variables. Variables that have a valid value always.
//------------------------------------------------------------------------------------------------------
  bool m_immediate_flag; // Temporary variable
  Fragoperrec *fragoperrec;
  Uint32 cfirstfreeFragopr;
  Uint32 cnoOfFragoprec;
  RSS_OP_COUNTER(cnoOfFreeFragoprec);
  RSS_OP_SNAPSHOT(cnoOfFreeFragoprec);

  Fragrecord *fragrecord;
  Uint32 cfirstfreefrag;
  Uint32 cnoOfFragrec;
  RSS_OP_COUNTER(cnoOfFreeFragrec);
  RSS_OP_SNAPSHOT(cnoOfFreeFragrec);
  FragrecordPtr prepare_fragptr;

  /*
   * DefaultValuesFragment is a normal struct Fragrecord.
   * It is TUP block-variable.
   * There is only ONE DefaultValuesFragment shared
   * among all table fragments stored by this TUP block.
  */
  FragrecordPtr DefaultValuesFragment;
  RSS_OP_SNAPSHOT(defaultValueWordsHi);
  RSS_OP_SNAPSHOT(defaultValueWordsLo);

  AlterTabOperation *alterTabOperRec;
  Uint32 cfirstfreeAlterTabOp;
  Uint32 cnoOfAlterTabOps;

  HostBuffer *hostBuffer;

  NdbMutex c_page_map_pool_mutex;
  DynArr256Pool c_page_map_pool;
  Operationrec_pool c_operation_pool;

  bool c_allow_alloc_spare_page;
  Page_pool c_page_pool;

  /* read ahead in pages during disk order scan */
  Uint32 m_max_page_read_ahead;
  
  Tablerec *tablerec;
  Uint32 cnoOfTablerec;

  TableDescriptor *tableDescriptor;
  Uint32 cnoOfTabDescrRec;
  RSS_OP_COUNTER(cnoOfFreeTabDescrRec);
  RSS_OP_SNAPSHOT(cnoOfFreeTabDescrRec);
  TablerecPtr prepare_tabptr;

  TablerecPtr m_curr_tabptr;
  FragrecordPtr m_curr_fragptr;

  PagePtr prepare_pageptr;
  Uint32 *prepare_tuple_ptr;
#ifdef VM_TRACE
  Local_key prepare_orig_local_key;
#endif
  Uint32 prepare_page_no;
  Uint32 prepare_frag_page_id;
  Uint32 prepare_page_idx;
  Uint64 c_debug_count;
  
  Uint32 cdata[32];
  Uint32 cdataPages[16];
  Uint32 cpackedListIndex;
  Uint32 cpackedList[MAX_NODES];
  Uint32 cerrorPackedDelay;
  Uint32 cfreeTdList[16];
  Uint32 clastBitMask;
  Uint32 clblPageCounter;
  Uint32 clblPagesPerTick;
  Uint32 clblPagesPerTickAfterSr;
  BlockReference clqhBlockref;
  Uint32 clqhUserpointer;
  Uint32 cminusOne;
  BlockReference cndbcntrRef;
  BlockReference cownref;
  Uint32 cownNodeId;
  Uint32 czero;
  Uint32 cCopyProcedure;
  Uint32 cCopyLastSeg;
  Uint32 cCopyOverwrite;
  Uint32 cCopyOverwriteLen;

 // A little bit bigger to cover overwrites in copy algorithms (16384 real size).
#define ZATTR_BUFFER_SIZE 16384
  Uint32 clogMemBuffer[ZATTR_BUFFER_SIZE + 16];
  Uint32 coutBuffer[ZATTR_BUFFER_SIZE + 16];
  Uint32 cinBuffer[ZATTR_BUFFER_SIZE + 16];
  Uint32 ctemp_page[ZWORDS_ON_PAGE];
  Uint32 ctemp_var_record[ZWORDS_ON_PAGE];

  // Trigger variables
  Uint32 c_maxTriggersPerTable;
  Uint32 m_max_parallel_index_build;

  Uint32 c_errorInsert4000TableId;
  Uint32 c_min_list_size[MAX_FREE_LIST + 1];
  Uint32 c_max_list_size[MAX_FREE_LIST + 1];

  void initGlobalTemporaryVars();
  void reportMemoryUsage(Signal* signal, int incDec);

  
#ifdef VM_TRACE
  struct Th {
    Uint32 data[1];
  };
  friend class NdbOut& operator<<(NdbOut&, const Operationrec&);
  friend class NdbOut& operator<<(NdbOut&, const Th&);
#endif

  void expand_tuple(KeyReqStruct*,
                    Uint32 sizes[4],
                    Tuple_header *org, 
		    const Tablerec*,
                    bool disk,
                    bool from_lcp_keep = false);
  void shrink_tuple(KeyReqStruct*,
                    Uint32 sizes[2],
                    const Tablerec*,
		    bool disk);
  
  Uint32* get_ptr(Var_part_ref);
  Uint32* get_ptr(PagePtr*, Var_part_ref);
  Uint32* get_ptr(PagePtr*, const Local_key*, const Tablerec*);
  Uint32* get_dd_ptr(PagePtr*, const Local_key*, const Tablerec*);
  Uint32* get_default_ptr(const Tablerec*, Uint32&);
  Uint32 get_len(Ptr<Page>* pagePtr, Var_part_ref ref);

  STATIC_CONST( COPY_TUPLE_HEADER32 = 4 );

  Tuple_header* alloc_copy_tuple(const Tablerec* tabPtrP, Local_key* ptr){
    Uint32 * dst = c_undo_buffer.alloc_copy_tuple(ptr,
                                                  tabPtrP->total_rec_size);
    if (unlikely(dst == 0))
      return 0;
#ifdef HAVE_VALGRIND
    bzero(dst, tabPtrP->total_rec_size);
#endif
    Uint32 count = tabPtrP->m_no_of_attributes;
    ChangeMask * mask = (ChangeMask*)(dst + COPY_TUPLE_HEADER32);
    mask->m_cols = count;
    return (Tuple_header*)(mask->end_of_mask(count));
  }

  Uint32 * get_copy_tuple_raw(const Local_key* ptr) {
    return c_undo_buffer.get_ptr(ptr);
  }

  Tuple_header * get_copy_tuple(Uint32 * rawptr) {
    return (Tuple_header*)
      (get_change_mask_ptr(rawptr)->end_of_mask());
  }

  ChangeMask * get_change_mask_ptr(Uint32 * rawptr) {
    return (ChangeMask*)(rawptr + COPY_TUPLE_HEADER32);
  }

  Tuple_header* get_copy_tuple(const Local_key* ptr){
    return get_copy_tuple(get_copy_tuple_raw(ptr));
  }

  ChangeMask* get_change_mask_ptr(const Tablerec* tabP,Tuple_header* copytuple){
    Uint32 * raw = (Uint32*)copytuple;
    Uint32 * tmp = raw - (1 + ((tabP->m_no_of_attributes + 31) >> 5));
    ChangeMask* mask = (ChangeMask*)tmp;
    assert(mask->end_of_mask() == raw);
    assert(get_copy_tuple(tmp - COPY_TUPLE_HEADER32) == copytuple);
    return mask;
  }

  /**
   * prealloc space from disk
   *   key.m_file_no  contains file no
   *   key.m_page_no  contains disk page
   *   key.m_page_idx contains byte preallocated
   */
  int disk_page_prealloc(Signal*, Ptr<Fragrecord>, Local_key*, Uint32);
  void disk_page_prealloc_dirty_page(Disk_alloc_info&, 
				     Ptr<Page>,
                                     Uint32,
                                     Uint32,
                                     Fragrecord*);
  void disk_page_prealloc_transit_page(Disk_alloc_info&,
				       Ptr<Page_request>, Uint32, Uint32);
  
  void disk_page_abort_prealloc(Signal*, Fragrecord*,Local_key*, Uint32);
  void disk_page_abort_prealloc_callback(Signal*, Uint32, Uint32);
  void disk_page_abort_prealloc_callback_1(Signal*, Fragrecord*,
					   PagePtr, Uint32);
  
  void disk_page_prealloc_callback(Signal*, Uint32, Uint32);
  void disk_page_prealloc_initial_callback(Signal*, Uint32, Uint32);
  void disk_page_prealloc_callback_common(Signal*, 
					  Ptr<Page_request>, 
					  Ptr<Fragrecord>,
					  Ptr<Page>);
  
  void disk_page_alloc(Signal*,
		       Tablerec*,
                       Fragrecord*,
                       Local_key*,
                       PagePtr,
                       Uint32,
                       const Local_key*,
                       Uint32 alloc_size);
  void disk_page_free(Signal*,
		      Tablerec*,
                      Fragrecord*,
                      Local_key*,
                      PagePtr,
                      Uint32,
                      const Local_key*,
                      Uint32 alloc_size);
  
  void disk_page_commit_callback(Signal*, Uint32 opPtrI, Uint32 page_id);  
  
  void disk_page_log_buffer_callback(Signal*, Uint32 opPtrI, Uint32); 

  void disk_page_alloc_extent_log_buffer_callback(Signal*, Uint32, Uint32);
  void disk_page_free_extent_log_buffer_callback(Signal*, Uint32, Uint32);
  
  Uint64 disk_page_undo_alloc(Signal *signal,
                              Page*,
                              const Local_key*,
			      Uint32 sz,
                              Uint32 gci,
                              Uint32 logfile_group_id,
                              Uint32 alloc_size);

  Uint64 disk_page_undo_update(Signal *signal,
                               Page*,
                               const Local_key*,
			       const Uint32*,
                               Uint32 sz,
			       Uint32 gci,
                               Uint32 logfile_group_id,
                               Uint32 alloc_size);
  
  Uint64 disk_page_undo_free(Signal *signal,
                             Page*,
                             const Local_key*,
			     const Uint32*,
                             Uint32 sz,
			     Uint32 gci,
                             Uint32 logfile_group_id,
                             Uint32 alloc_size);

  void undo_createtable_logsync_callback(Signal* signal, Uint32, Uint32);

  void drop_table_logsync_callback(Signal*, Uint32, Uint32);
  void drop_table_log_buffer_callback(Signal*, Uint32, Uint32);

  void disk_page_set_dirty(Ptr<Page>);
  void restart_setup_page(Ptr<Fragrecord> fragPtr,
                          Disk_alloc_info&,
                          Ptr<Page>,
                          Int32 estimate);
  void update_extent_pos(EmulatedJamBuffer* jamBuf, Disk_alloc_info&, 
                         Ptr<Extent_info>, Int32 delta);

  void disk_page_move_page_request(Disk_alloc_info& alloc,
                                   Ptr<Extent_info>,
                                   Ptr<Page_request> req,
                                   Uint32 old_idx, Uint32 new_idx);

  void disk_page_move_dirty_page(Disk_alloc_info& alloc,
                                 Ptr<Extent_info> extentPtr,
                                 Ptr<Page> pagePtr,
                                 Uint32 old_idx,
                                 Uint32 new_idx,
                                 Fragrecord*);

  void disk_page_get_allocated(const Tablerec*, const Fragrecord*,
                               Uint64 res[2]);
  /**
   * Disk restart code
   */
public:
  int disk_page_load_hook(Uint32 page_id);
  
  void disk_page_unmap_callback(Uint32 when, Uint32 page, Uint32 dirty_count);
  
  int disk_restart_alloc_extent(EmulatedJamBuffer* jamBuf, 
                                Uint32 tableId,
                                Uint32 fragId,
                                Uint32 create_table_version,
				const Local_key* key,
                                Uint32 pages);
  void disk_restart_page_bits(EmulatedJamBuffer* jamBuf,
                              Uint32 tableId,
                              Uint32 fragId,
                              Uint32 create_table_version,
			      const Local_key*,
                              Uint32 bits);
  void disk_restart_undo(Signal* signal,
                         Uint64 lsn,
			 Uint32 type,
                         const Uint32 * ptr,
                         Uint32 len);

  void verify_undo_log_execution();
  struct Apply_undo 
  {
    bool m_in_intermediate_log_record;
    Uint32 m_type;
    Uint32 m_len;
    Uint32 m_offset;
    const Uint32* m_ptr;
    Uint32 m_data[MAX_UNDO_DATA];
    Uint64 m_lsn;
    Ptr<Tablerec> m_table_ptr;
    Ptr<Fragrecord> m_fragment_ptr;
    Ptr<Page> m_page_ptr;
    Ptr<Extent_info> m_extent_ptr;
    Local_key m_key;
    Uint32 nextList;
    union { Uint32 nextPool; Uint32 prevList; };
    Uint32 m_magic;

    Apply_undo();
  };
  typedef RecordPool<RWPool<Apply_undo> > Apply_undo_pool;
  typedef DLCFifoList<Apply_undo_pool> Apply_undo_list;
  typedef LocalDLCFifoList<Apply_undo_pool> LocalApply_undo_list;

  Apply_undo_pool c_apply_undo_pool;

  struct Pending_undo_page
  {
    Pending_undo_page()  {}
    Pending_undo_page(Uint32 file_no, Uint32 page_no)
    {
      m_file_no = file_no;
      m_page_no = page_no;
    }

    Uint16 m_file_no;
    Uint32 m_page_no;
    Apply_undo_list::Head m_apply_undo_head;

    Uint32 nextHash;
    union { Uint32 prevHash; Uint32 nextPool; };
    Uint32 m_magic;

    Uint32 hashValue() const
    {
      return m_file_no << 16 | m_page_no;
    }

    bool equal(const Pending_undo_page& obj) const
    {
      return m_file_no == obj.m_file_no && m_page_no == obj.m_page_no;
    }
  };

  typedef RecordPool<RWPool<Pending_undo_page> >
    Pending_undo_page_pool;
  typedef DLCHashTable<Pending_undo_page_pool>
    Pending_undo_page_hash;

  void disk_restart_lcp_id(Uint32 table,
                           Uint32 frag,
                           Uint32 lcpId,
                           Uint32 localLcpId);
  
  bool is_disk_columns_in_table(Uint32 tableId);

private:
  bool c_started;

  Pending_undo_page_pool c_pending_undo_page_pool;
  Pending_undo_page_hash c_pending_undo_page_hash;

  // these 2 were file-static before mt-lqh
  bool f_undo_done;
  Dbtup::Apply_undo f_undo;

  void disk_restart_undo_next(Signal*,
                              Uint32 applied = 0,
                              Uint32 count_pending = 1);
  void disk_restart_undo_lcp(Uint32,
                             Uint32,
                             Uint32 flag,
                             Uint32 lcpId,
                             Uint32 localLcpId);
  void release_undo_record(Ptr<Apply_undo>&, bool);

  void disk_restart_undo_callback(Signal* signal, Uint32, Uint32);
  void disk_restart_undo_alloc(Apply_undo*);
  void disk_restart_undo_update(Apply_undo*);
  void disk_restart_undo_update_first_part(Apply_undo*);
  void disk_restart_undo_update_part(Apply_undo*);
  void disk_restart_undo_free(Apply_undo*, bool);
  void disk_restart_undo_page_bits(Signal*, Apply_undo*);

#ifdef VM_TRACE
  void verify_page_lists(Disk_alloc_info&);
#else
  void verify_page_lists(Disk_alloc_info&) {}
#endif
  
  void findFirstOp(OperationrecPtr&);
  bool is_rowid_in_remaining_lcp_set(const Page* page,
		                     Fragrecord* regFragPtr, 
                                     const Local_key& key1,
                                     const Dbtup::ScanOp& op,
                           Uint32 check_lcp_scanned_state_reversed);
  void update_gci(Fragrecord*, Tablerec*, Tuple_header*, Uint32);
  void commit_operation(Signal*,
                        Uint32,
                        Uint32,
                        Tuple_header*,
                        PagePtr,
			Operationrec*,
                        Fragrecord*,
                        Tablerec*,
                        Ptr<GlobalPage> diskPagePtr);

  void commit_refresh(Signal*,
                      Uint32,
                      Uint32,
                      Tuple_header*,
                      PagePtr,
                      KeyReqStruct*,
                      Operationrec*,
                      Fragrecord*,
                      Tablerec*,
                      Ptr<GlobalPage> diskPagePtr);

  int retrieve_data_page(Signal*,
                         Page_cache_client::Request,
                         OperationrecPtr,
                         Ptr<GlobalPage> &diskPagePtr,
                         Fragrecord *fragPtrP);
  int retrieve_log_page(Signal*, FragrecordPtr, OperationrecPtr);

  void dealloc_tuple(Signal* signal,
                     Uint32,
                     Uint32,
                     Page*,
                     Tuple_header*,
                     KeyReqStruct*,
                     Operationrec*,
                     Fragrecord*,
                     Tablerec*,
                     Ptr<GlobalPage> diskPagePtr);

  bool store_extra_row_bits(Uint32, const Tablerec*, Tuple_header*, Uint32,
                            bool);
  void read_extra_row_bits(Uint32, const Tablerec*, Tuple_header*, Uint32 *,
                           bool);

  int handle_size_change_after_update(KeyReqStruct* req_struct,
				      Tuple_header* org,
				      Operationrec*,
				      Fragrecord* regFragPtr,
				      Tablerec* regTabPtr,
				      Uint32 sizes[4]);
  int optimize_var_part(KeyReqStruct* req_struct,
                        Tuple_header* org,
                        Operationrec* regOperPtr,
                        Fragrecord* regFragPtr,
                        Tablerec* regTabPtr);

  /**
   * Setup all pointer on keyreqstruct to prepare for read
   *   req_struct->m_tuple_ptr is set to tuple to read
   */
  void prepare_read(KeyReqStruct*, Tablerec* const, bool disk);

  /* For debugging, dump the contents of a tuple. */
  void dump_tuple(const KeyReqStruct* req_struct, const Tablerec* tabPtrP);

#ifdef VM_TRACE
  void check_page_map(Fragrecord*);
  bool find_page_id_in_list(Fragrecord*, Uint32 pid);
#endif
  Uint32* init_page_map_entry(Fragrecord*, Uint32);
  const char* insert_free_page_id_list(Fragrecord* fragPtrP,
                                       Uint32 logicalPageId,
                                       Uint32 *next,
                                       Uint32 *prev,
                                       Uint32 lcp_scanned_bit,
                                       Uint32 last_lcp_state);
  void remove_top_from_lcp_keep_list(Fragrecord*, Uint32*, Local_key);
  void insert_lcp_keep_list(Fragrecord*, Local_key, Uint32*, const Local_key*);
  void handle_lcp_drop_change_page(Fragrecord*, Uint32, PagePtr, bool);
  void handle_lcp_keep(Signal*, FragrecordPtr, ScanOp*);
  void handle_lcp_keep_commit(const Local_key*,
                              KeyReqStruct *,
                              Operationrec*, Fragrecord*, Tablerec*);

  void setup_lcp_read_copy_tuple( KeyReqStruct *,
                                  Operationrec*,
                                  Tablerec*);

  bool isCopyTuple(Uint32 pageid, Uint32 pageidx) const {
    return (pageidx & (Uint16(1) << 15)) != 0;
  }

  void setCopyTuple(Uint32& pageid, Uint16& pageidx) const {
    assert(!isCopyTuple(pageid, pageidx));
    pageidx |= (Uint16(1) << 15);
    assert(isCopyTuple(pageid, pageidx));
  }

  void clearCopyTuple(Uint32& pageid, Uint16& pageidx) const {
    assert(isCopyTuple(pageid, pageidx));
    pageidx &= ~(Uint16(1) << 15);
    assert(!isCopyTuple(pageid, pageidx));
  }
};

inline
Uint32
Dbtup::get_current_frag_page_id()
{
  return prepare_pageptr.p->frag_page_id;
}

inline
void
Dbtup::setup_fixed_tuple_ref_opt(KeyReqStruct* req_struct)
{
  req_struct->m_page_ptr = prepare_pageptr;
  req_struct->m_tuple_ptr = (Tuple_header*)prepare_tuple_ptr;
}

inline
void
Dbtup::setup_fixed_tuple_ref(KeyReqStruct* req_struct,
                             Operationrec* regOperPtr,
                             Tablerec* regTabPtr)
{
  PagePtr page_ptr;
  Uint32* ptr= get_ptr(&page_ptr, &regOperPtr->m_tuple_location, regTabPtr);
  req_struct->m_page_ptr = page_ptr;
  req_struct->m_tuple_ptr = (Tuple_header*)ptr;
}

inline
Dbtup::TransState
Dbtup::get_trans_state(Operationrec * regOperPtr)
{
  return (Dbtup::TransState)regOperPtr->trans_state;
}

inline
void
Dbtup::set_trans_state(Operationrec* regOperPtr,
                       Dbtup::TransState trans_state)
{
  regOperPtr->trans_state= (Uint32)trans_state;
}

inline
Dbtup::TupleState
Dbtup::get_tuple_state(Operationrec * regOperPtr)
{
  return (Dbtup::TupleState)regOperPtr->tuple_state;
}

inline
void
Dbtup::set_tuple_state(Operationrec* regOperPtr,
                       Dbtup::TupleState tuple_state)
{
  regOperPtr->tuple_state= (Uint32)tuple_state;
}


inline
Uint32
Dbtup::decr_tup_version(Uint32 tup_version)
{
  return (tup_version - 1) & ZTUP_VERSION_MASK;
}

inline
Uint32*
Dbtup::get_ptr(Var_part_ref ref)
{
  Ptr<Page> tmp;
  return get_ptr(&tmp, ref);
}

inline
Uint32*
Dbtup::get_ptr(Ptr<Page>* pagePtr, Var_part_ref ref)
{
  PagePtr tmp;
  Local_key key;
  ref.copyout(&key);
  tmp.i = key.m_page_no;
  
  c_page_pool.getPtr(tmp);
  memcpy(pagePtr, &tmp, sizeof(tmp));
  return ((Var_page*)tmp.p)->get_ptr(key.m_page_idx);
}

inline
Uint32*
Dbtup::get_ptr(PagePtr* pagePtr, 
	       const Local_key* key, const Tablerec* regTabPtr)
{
  PagePtr tmp;
  tmp.i= key->m_page_no;
  c_page_pool.getPtr(tmp);
  memcpy(pagePtr, &tmp, sizeof(tmp));

  return ((Fix_page*)tmp.p)->
    get_ptr(key->m_page_idx, regTabPtr->m_offsets[MM].m_fix_header_size);
}

inline
Uint32*
Dbtup::get_default_ptr(const Tablerec* regTabPtr, Uint32& default_len)
{
  Var_part_ref ref;
  ref.assign(&regTabPtr->m_default_value_location);
  Ptr<Page> page;

  Uint32* default_data = get_ptr(&page, ref);
  default_len = get_len(&page, ref);

  return default_data;
}

inline
Uint32*
Dbtup::get_dd_ptr(PagePtr* pagePtr, 
		  const Local_key* key, const Tablerec* regTabPtr)
{
  PagePtr tmp;
  tmp.i= key->m_page_no;
  tmp.p= (Page*)m_global_page_pool.getPtr(tmp.i);
  memcpy(pagePtr, &tmp, sizeof(tmp));
  
  if(regTabPtr->m_attributes[DD].m_no_of_varsize ||
     regTabPtr->m_attributes[DD].m_no_of_dynamic)
    return ((Var_page*)tmp.p)->get_ptr(key->m_page_idx);
  else
    return ((Fix_page*)tmp.p)->
      get_ptr(key->m_page_idx, regTabPtr->m_offsets[DD].m_fix_header_size);
}

/*
  This function assumes that get_ptr() has been called first to
  initialise the pagePtr argument.
*/
inline
Uint32
Dbtup::get_len(Ptr<Page>* pagePtr, Var_part_ref ref)
{
  Uint32 page_idx= ref.m_page_idx;
  return ((Var_page*)pagePtr->p)->get_entry_len(page_idx);
}

NdbOut&
operator<<(NdbOut&, const Dbtup::Tablerec&);

inline
bool Dbtup::find_savepoint(OperationrecPtr& loopOpPtr, Uint32 savepointId)
{
  while (true) {
    if (savepointId > loopOpPtr.p->savepointId) {
      jam();
      return true;
    }
    loopOpPtr.i = loopOpPtr.p->prevActiveOp;
    if (loopOpPtr.i == RNIL) {
      break;
    }
    c_operation_pool.getPtr(loopOpPtr);
  }
  return false;
}

inline
void
Dbtup::update_change_mask_info(const Tablerec* tablePtrP,
                               ChangeMask* dst,
                               const Uint32 * src)
{
  assert(dst->m_cols == tablePtrP->m_no_of_attributes);
  Uint32 * ptr = dst->m_mask;
  Uint32 len = (tablePtrP->m_no_of_attributes + 31) >> 5;
  for (Uint32 i = 0; i<len; i++)
  {
    * ptr |= *src;
    ptr++;
    src++;
  }
}

inline
void
Dbtup::set_change_mask_info(const Tablerec* tablePtrP, ChangeMask* dst)
{
  assert(dst->m_cols == tablePtrP->m_no_of_attributes);
  Uint32 len = (tablePtrP->m_no_of_attributes + 31) >> 5;
  BitmaskImpl::set(len, dst->m_mask);
}

inline
void
Dbtup::clear_change_mask_info(const Tablerec* tablePtrP, ChangeMask* dst)
{
  assert(dst->m_cols == tablePtrP->m_no_of_attributes);
  Uint32 len = (tablePtrP->m_no_of_attributes + 31) >> 5;
  BitmaskImpl::clear(len, dst->m_mask);
}

inline
void
Dbtup::copy_change_mask_info(const Tablerec* tablePtrP,
                             ChangeMask* dst, const ChangeMask* src)
{
  Uint32 dst_cols = tablePtrP->m_no_of_attributes;
  assert(dst->m_cols == dst_cols);
  Uint32 src_cols = src->m_cols;

  if (dst_cols == src_cols)
  {
    memcpy(dst->m_mask, src->m_mask, 4 * ((dst_cols + 31) >> 5));
  }
  else
  {
    ndbassert(dst_cols > src_cols); // drop column not supported
    memcpy(dst->m_mask, src->m_mask, 4 * ((src_cols + 31) >> 5));
    BitmaskImpl::setRange((dst_cols + 31) >> 5, dst->m_mask,
                          src_cols,  (dst_cols - src_cols));
  }
}

// Dbtup_client provides proxying similar to Page_cache_client

class Dbtup_client
{
  friend class DbtupProxy;
  // jam buffer of caller block.
  EmulatedJamBuffer* const m_jamBuf;
  class DbtupProxy* m_dbtup_proxy; // set if we go via proxy
  Dbtup* m_dbtup;
  DEBUG_OUT_DEFINES(DBTUP);

public:
  Dbtup_client(SimulatedBlock* block, SimulatedBlock* dbtup);

  // LGMAN

  void disk_restart_undo(Signal* signal, Uint64 lsn,
                         Uint32 type, const Uint32 * ptr, Uint32 len);

  // TSMAN

  int disk_restart_alloc_extent(Uint32 tableId,
                                Uint32 fragId,
                                Uint32 create_table_version,
				const Local_key* key,
                                Uint32 pages);

  void disk_restart_page_bits(Uint32 tableId,
                              Uint32 fragId,
                              Uint32 create_table_version,
			      const Local_key* key,
                              Uint32 bits);
};


#undef JAM_FILE_ID

#endif
