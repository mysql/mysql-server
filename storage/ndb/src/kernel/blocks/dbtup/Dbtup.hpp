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
#include <signaldata/DropTrig.hpp>
#include <signaldata/TrigAttrInfo.hpp>
#include <signaldata/BuildIndx.hpp>
#include <signaldata/AlterTab.hpp>
#include <AttributeDescriptor.hpp>
#include "AttributeOffset.hpp"
#include "Undo_buffer.hpp"
#include "tuppage.hpp"
#include <DynArr256.hpp>
#include <../pgman.hpp>
#include <../tsman.hpp>

// jams
#undef jam
#undef jamEntry
#ifdef DBTUP_BUFFER_CPP
#define jam()	 	jamLine(10000 + __LINE__)
#define jamEntry() 	jamEntryLine(10000 + __LINE__)
#endif
#ifdef DBTUP_ROUTINES_CPP
#define jam()           jamLine(15000 + __LINE__)
#define jamEntry()      jamEntryLine(15000 + __LINE__)
#endif
#ifdef DBTUP_COMMIT_CPP
#define jam()           jamLine(20000 + __LINE__)
#define jamEntry()      jamEntryLine(20000 + __LINE__)
#endif
#ifdef DBTUP_FIXALLOC_CPP
#define jam()           jamLine(25000 + __LINE__)
#define jamEntry()      jamEntryLine(25000 + __LINE__)
#endif
#ifdef DBTUP_TRIGGER_CPP
#define jam()           jamLine(30000 + __LINE__)
#define jamEntry()      jamEntryLine(30000 + __LINE__)
#endif
#ifdef DBTUP_ABORT_CPP
#define jam()           jamLine(35000 + __LINE__)
#define jamEntry()      jamEntryLine(35000 + __LINE__)
#endif
#ifdef DBTUP_PAGE_MAP_CPP
#define jam()           jamLine(40000 + __LINE__)
#define jamEntry()      jamEntryLine(40000 + __LINE__)
#endif
#ifdef DBTUP_PAG_MAN_CPP
#define jam()           jamLine(45000 + __LINE__)
#define jamEntry()      jamEntryLine(45000 + __LINE__)
#endif
#ifdef DBTUP_STORE_PROC_DEF_CPP
#define jam()           jamLine(50000 + __LINE__)
#define jamEntry()      jamEntryLine(50000 + __LINE__)
#endif
#ifdef DBTUP_META_CPP
#define jam()           jamLine(55000 + __LINE__)
#define jamEntry()      jamEntryLine(55000 + __LINE__)
#endif
#ifdef DBTUP_TAB_DES_MAN_CPP
#define jam()           jamLine(60000 + __LINE__)
#define jamEntry()      jamEntryLine(60000 + __LINE__)
#endif
#ifdef DBTUP_GEN_CPP
#define jam()           jamLine(65000 + __LINE__)
#define jamEntry()      jamEntryLine(65000 + __LINE__)
#endif
#ifdef DBTUP_INDEX_CPP
#define jam()           jamLine(70000 + __LINE__)
#define jamEntry()      jamEntryLine(70000 + __LINE__)
#endif
#ifdef DBTUP_DEBUG_CPP
#define jam()           jamLine(75000 + __LINE__)
#define jamEntry()      jamEntryLine(75000 + __LINE__)
#endif
#ifdef DBTUP_VAR_ALLOC_CPP
#define jam()           jamLine(80000 + __LINE__)
#define jamEntry()      jamEntryLine(80000 + __LINE__)
#endif
#ifdef DBTUP_SCAN_CPP
#define jam()           jamLine(85000 + __LINE__)
#define jamEntry()      jamEntryLine(85000 + __LINE__)
#endif
#ifdef DBTUP_DISK_ALLOC_CPP
#define jam()           jamLine(90000 + __LINE__)
#define jamEntry()      jamEntryLine(90000 + __LINE__)
#endif
#ifndef jam
#define jam()           jamLine(__LINE__)
#define jamEntry()      jamEntryLine(__LINE__)
#endif

#ifdef VM_TRACE
inline const char* dbgmask(const Bitmask<MAXNROFATTRIBUTESINWORDS>& bm) {
  static int i=0; static char buf[5][200];
  bm.getText(buf[i%5]); return buf[i++%5]; }
inline const char* dbgmask(const Uint32 bm[2]) {
  static int i=0; static char buf[5][200];
  sprintf(buf[i%5],"%08x%08x",bm[1],bm[0]); return buf[i++%5]; }
#endif

#define ZWORDS_ON_PAGE 8192          /* NUMBER OF WORDS ON A PAGE.      */
#define ZATTRBUF_SIZE 32             /* SIZE OF ATTRIBUTE RECORD BUFFER */
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
//------------------------------------------------------------------
// Jam Handling:
//
// When DBTUP reports lines through jam in the trace files it has to
// be interpreted. 4024 means as an example line 24 in DbtupCommit.cpp
// Thus 4000 is added to the line number beacuse it is located in the
// file DbtupCommit.cpp. The following is the exhaustive list of the
// added value in the various files. ndbrequire, ptrCheckGuard still
// only reports the line number in the file it currently is located in.
// 
// DbtupExecQuery.cpp         0
// DbtupBuffer.cpp         10000
// DbtupRoutines.cpp       15000
// DbtupCommit.cpp         20000
// DbtupFixAlloc.cpp       25000
// DbtupTrigger.cpp        30000
// DbtupAbort.cpp          35000
// DbtupPageMap.cpp        40000
// DbtupPagMan.cpp         45000
// DbtupStoredProcDef.cpp  50000
// DbtupMeta.cpp           55000
// DbtupTabDesMan.cpp      60000
// DbtupGen.cpp            65000
// DbtupIndex.cpp          70000
// DbtupDebug.cpp          75000
// DbtupVarAlloc.cpp       80000
// DbtupScan.cpp           85000
// DbtupDiskAlloc.cpp      90000
//------------------------------------------------------------------

/*
2.2 LOCAL SYMBOLS
-----------------
*/
/* ---------------------------------------------------------------- */
/*       S I Z E              O F               R E C O R D S       */
/* ---------------------------------------------------------------- */
#define ZNO_OF_ATTRBUFREC 10000             /* SIZE   OF ATTRIBUTE INFO FILE   */
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

#define ZINVALID_CHAR_FORMAT 744
#define ZROWID_ALLOCATED 899
#define ZINVALID_ALTER_TAB 741

          /* SOME WORD POSITIONS OF FIELDS IN SOME HEADERS */

#define ZFREE_COMMON 1                    /* PAGE STATE, PAGE IN COMMON AREA                   */
#define ZEMPTY_MM 2                       /* PAGE STATE, PAGE IN EMPTY LIST                    */
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

          /* ATTRINBUFREC VARIABLE POSITIONS. */
#define ZBUF_PREV 29                      /* POSITION OF 'PREV'-VARIABLE (USED BY INTERPRETED EXEC) */
#define ZBUF_DATA_LEN 30                  /* POSITION OF 'DATA LENGTH'-VARIABLE. */
#define ZBUF_NEXT 31                      /* POSITION OF 'NEXT'-VARIABLE.        */
#define ZSAVE_BUF_NEXT 28
#define ZSAVE_BUF_DATA_LEN 27

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

#define ZSCAN_PROCEDURE 0
#define ZCOPY_PROCEDURE 2
#define ZSTORED_PROCEDURE_DELETE 3
#define ZSTORED_PROCEDURE_FREE 0xffff
#define ZMIN_PAGE_LIMIT_TUP_COMMITREQ 2

#define ZSKIP_TUX_TRIGGERS 0x1 // flag for TUP_ABORTREQ

#endif

class Dbtup: public SimulatedBlock {
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
private:
  
  typedef Tup_fixsize_page Fix_page;
  typedef Tup_varsize_page Var_page;

public:
  class Dblqh *c_lqh;
  Tsman* c_tsman;
  Lgman* c_lgman;
  Page_cache_client m_pgman;

// State values
enum ChangeMaskState {
  DELETE_CHANGES = 0,
  SET_ALL_MASK = 1,
  USE_SAVED_CHANGE_MASK = 2,
  RECALCULATE_CHANGE_MASK = 3
};

enum TransState {
  TRANS_IDLE = 0,
  TRANS_STARTED = 1,
  TRANS_WAIT_STORED_PROCEDURE_ATTR_INFO = 2,
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

// Records
/* ************** ATTRIBUTE INFO BUFFER RECORD ****************** */
/* THIS RECORD IS USED AS A BUFFER FOR INCOMING AND OUTGOING DATA */
/* ************************************************************** */
struct Attrbufrec {
  Uint32 attrbuf[ZATTRBUF_SIZE];
}; /* p2c: size = 128 bytes */

typedef Ptr<Attrbufrec> AttrbufrecPtr;



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
  BlockReference lqhBlockrefFrag;
  bool inUse;
  bool definingFragment;
};
typedef Ptr<Fragoperrec> FragoperrecPtr;

  /* Operation record used during alter table. */
  struct AlterTabOperation {
    Uint32 nextAlterTabOp;
    Uint32 newNoOfAttrs;
    Uint32 newNoOfCharsets;
    Uint32 newNoOfKeyAttrs;
    Uint32 noOfDynNullBits;
    Uint32 noOfDynVar;
    Uint32 noOfDynFix;
    Uint32 noOfDynamic;
    Uint32 tabDesOffset[7];
    Uint32 desAllocSize;
    Uint32 tableDescriptor;
    Uint32 dynTabDesOffset[3];
    Uint32 dynDesAllocSize;
    Uint32 dynTableDescriptor;
  };
  typedef Ptr<AlterTabOperation> AlterTabOperationPtr;

  typedef Tup_page Page;
  typedef Ptr<Page> PagePtr;

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
      Get_tuple,
      Get_next_tuple_fs,
      Get_tuple_fs
    };
    Get m_get;                  // entry point in scanNext
    Local_key m_key;            // scan position pointer MM or DD
    Page* m_page;               // scanned MM or DD (cache) page
    Local_key m_key_mm;         // MM local key returned
    Uint32 m_realpid_mm;        // MM real page id
    Uint32 m_extent_info_ptr_i;
  };

  // Scan Lock
  struct ScanLock {
    Uint32 m_accLockOp;
    union {
      Uint32 nextPool;
      Uint32 nextList;
    };
    Uint32 prevList;
  };
  typedef Ptr<ScanLock> ScanLockPtr;
  ArrayPool<ScanLock> c_scanLockPool;

  // Tup scan, similar to Tux scan.  Later some of this could
  // be moved to common superclass.
  struct ScanOp {
    ScanOp() :
      m_state(Undef),
      m_bits(0),
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

    DLFifoList<ScanLock>::Head m_accLockOps;

    union {
    Uint32 nextPool;
    Uint32 nextList;
    };
    Uint32 prevList;
  };
  typedef Ptr<ScanOp> ScanOpPtr;
  ArrayPool<ScanOp> c_scanOpPool;

  void scanReply(Signal*, ScanOpPtr scanPtr);
  void scanFirst(Signal*, ScanOpPtr scanPtr);
  bool scanNext(Signal*, ScanOpPtr scanPtr);
  void scanCont(Signal*, ScanOpPtr scanPtr);
  void disk_page_tup_scan_callback(Signal*, Uint32 scanPtrI, Uint32 page_i);
  void scanClose(Signal*, ScanOpPtr scanPtr);
  void addAccLockOp(ScanOp& scan, Uint32 accLockOp);
  void removeAccLockOp(ScanOp& scan, Uint32 accLockOp);
  void releaseScanOp(ScanOpPtr& scanPtr);

  // for md5 of key (could maybe reuse existing temp buffer)
  Uint64 c_dataBuffer[ZWORDS_ON_PAGE/2 + 1];

  struct Page_request 
  {
    Local_key m_key;
    Uint32 m_frag_ptr_i;
    Uint32 m_extent_info_ptr;
    Uint16 m_estimated_free_space; // in bytes/records
    Uint16 m_list_index;           // in Disk_alloc_info.m_page_requests
    Uint16 m_ref_count;            // Waiters for page
    Uint16 m_uncommitted_used_space;
    Uint32 nextList;
    Uint32 prevList;
    Uint32 m_magic;
  }; // 32 bytes
  
  typedef RecordPool<Page_request, WOPool> Page_request_pool;
  typedef DLFifoListImpl<Page_request_pool, Page_request> Page_request_list;
  typedef LocalDLFifoListImpl<Page_request_pool, Page_request> Local_page_request_list;

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

  typedef RecordPool<Extent_info, RWPool> Extent_info_pool;
  typedef DLListImpl<Extent_info_pool, Extent_info> Extent_info_list;
  typedef LocalDLListImpl<Extent_info_pool, Extent_info> Local_extent_info_list;
  typedef DLHashTableImpl<Extent_info_pool, Extent_info> Extent_info_hash;
  typedef SLListImpl<Extent_info_pool, Extent_info, Extent_list_t> Fragment_extent_list;
  typedef LocalSLListImpl<Extent_info_pool, Extent_info, Extent_list_t> Local_fragment_extent_list;
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
     * 2) Allocate space on pages waiting to maped that will be dirty
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
    DLList<Page>::Head m_dirty_pages[MAX_FREE_LIST];   // In real page id's

    /**
     * Requests (for update) that have sufficient space left after request
     *   these are currently being "mapped"
     */
    Page_request_list::Head m_page_requests[MAX_FREE_LIST];

    DLList<Page>::Head m_unmap_pages;

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

struct Fragrecord {
  Uint32 noOfPages;
  Uint32 noOfVarPages;

  DynArr256::Head m_page_map;
  DLList<Page>::Head emptyPrimPage; // allocated pages (not init)
  DLFifoList<Page>::Head thFreeFirst;   // pages with atleast 1 free record
  SLList<Page>::Head m_empty_pages; // Empty pages not in logical/physical map
  
  Uint32 m_lcp_scan_op;
  Uint32 m_lcp_keep_list;

  State fragStatus;
  Uint32 fragTableId;
  Uint32 fragmentId;
  Uint32 nextfreefrag;
  DLList<Page>::Head free_var_page_array[MAX_FREE_LIST];
  
  DLList<ScanOp>::Head m_scanList;

  enum { UC_LCP = 1, UC_CREATE = 2, UC_SET_LCP = 3 };
  Uint32 m_restore_lcp_id;
  Uint32 m_undo_complete;
  Uint32 m_tablespace_id;
  Uint32 m_logfile_group_id;
  Disk_alloc_info m_disk_alloc_info;
  Uint32 m_var_page_chunks;
};
typedef Ptr<Fragrecord> FragrecordPtr;


struct Operationrec {
  /*
   * To handle Attrinfo signals and buffer them up we need to
   * a simple list with first and last and we also need to keep track
   * of how much we received for security check.
   * Will most likely disappear with introduction of long signals.
   * These variables are used before TUPKEYREQ is received and not
   * thereafter and is disposed with after calling copyAttrinfo
   * which is called before putting the operation into its lists.
   * Thus we can use union declarations for these variables.
   */

  /*
   * Used by scans to find the Attrinfo buffers.
   * This is only until returning from copyAttrinfo and
   * can thus reuse the same memory as needed by the
   * active operation list variables.
   */

  /*
   * Doubly linked list with anchor on tuple.
   * This is to handle multiple updates on the same tuple
   * by the same transaction.
   */
  union {
    Uint32 prevActiveOp;
    Uint32 storedProcedureId; //Used until copyAttrinfo
  };
  union {
    Uint32 nextActiveOp;
    Uint32 currentAttrinbufLen; //Used until copyAttrinfo
  };

  Operationrec() {}
  bool is_first_operation() const { return prevActiveOp == RNIL;}
  bool is_last_operation() const { return nextActiveOp == RNIL;}

  Uint32 m_undo_buffer_space; // In words
  union {
    Uint32 firstAttrinbufrec; //Used until copyAttrinfo
  };
  Uint32 m_any_value;
  union {
    Uint32 lastAttrinbufrec; //Used until copyAttrinfo
    Uint32 nextPool;
  };
  Uint32 attrinbufLen; //only used during STORED_PROCDEF phase
  Uint32 storedProcPtr; //only used during STORED_PROCDEF phase
  
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

  /*
   * We use 64 bits to save change mask for the most common cases.
   */
  Uint32 saved_change_mask[2];

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
    unsigned int trans_state : 3;
    unsigned int tuple_state : 2;
    unsigned int in_active_list : 1;

    unsigned int op_type : 3;
    unsigned int delete_insert_flag : 1;
    unsigned int primary_replica : 1;
    unsigned int change_mask_state : 2;
    unsigned int m_disk_preallocated : 1;
    unsigned int m_load_diskpage_on_commit : 1;
    unsigned int m_wait_log_buffer : 1;
  };
  union {
    OpBitFields op_struct;
    Uint16 op_bit_fields;
  };

  /*
   * TUX needs to know the tuple version of the tuple since it
   * keeps an entry for both the committed and all versions in
   * a transaction currently. So each update will create a new
   * version even if in the same transaction.
   */
  Uint16 tupVersion;
};
typedef Ptr<Operationrec> OperationrecPtr;

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
   * Receiver block
   */
  Uint32 m_receiverBlock;
  
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
  
/**
 * Pool of trigger data record
 */
ArrayPool<TupTriggerData> c_triggerPool;

  /* ************ TABLE RECORD ************ */
  /* THIS RECORD FORMS A LIST OF TABLE      */
  /* REFERENCE INFORMATION. ONE RECORD      */
  /* PER TABLE REFERENCE.                   */
  /* ************************************** */
  STATIC_CONST( MM = 0 );
  STATIC_CONST( DD = 1 );
  STATIC_CONST( DYN_BM_LEN_BITS = 8 );
  STATIC_CONST( DYN_BM_LEN_MASK = ((1 << DYN_BM_LEN_BITS) - 1));
  
  struct Tablerec {
    Tablerec(ArrayPool<TupTriggerData> & triggerPool) : 
      afterInsertTriggers(triggerPool),
      afterDeleteTriggers(triggerPool),
      afterUpdateTriggers(triggerPool),
      subscriptionInsertTriggers(triggerPool),
      subscriptionDeleteTriggers(triggerPool),
      subscriptionUpdateTriggers(triggerPool),
      constraintUpdateTriggers(triggerPool),
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
    Uint32 dynTabDescriptor;

    /* Mask of variable-sized dynamic attributes. */
    Uint32* dynVarSizeMask;
    /*
      Mask of fixed-sized dynamic attributes. There is one bit set for each
      32-bit word occupied by fixed-size attributes, so fixed-size dynamic
      attributes >32bit have multiple bits here.
    */
    Uint32* dynFixSizeMask;

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
      TR_ForceVarPart = 0x4
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
    Uint16 m_dyn_null_bits;

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
    DLList<TupTriggerData> afterInsertTriggers;
    DLList<TupTriggerData> afterDeleteTriggers;
    DLList<TupTriggerData> afterUpdateTriggers;
    DLList<TupTriggerData> subscriptionInsertTriggers;
    DLList<TupTriggerData> subscriptionDeleteTriggers;
    DLList<TupTriggerData> subscriptionUpdateTriggers;
    DLList<TupTriggerData> constraintUpdateTriggers;
    
    // List of ordered indexes
    DLList<TupTriggerData> tuxCustomTriggers;
    
    Uint32 fragid[MAX_FRAG_PER_NODE];
    Uint32 fragrec[MAX_FRAG_PER_NODE];

    struct {
      Uint32 tabUserPtr;
      Uint32 tabUserRef;
      Uint32 m_lcpno;
      Uint32 m_fragPtrI;
    } m_dropTable;
    State tableStatus;
  };  

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
      ,UNDO_CREATE = File_formats::Undofile::UNDO_TUP_CREATE
      ,UNDO_DROP = File_formats::Undofile::UNDO_TUP_DROP
      ,UNDO_ALLOC_EXTENT = File_formats::Undofile::UNDO_TUP_ALLOC_EXTENT
      ,UNDO_FREE_EXTENT = File_formats::Undofile::UNDO_TUP_FREE_EXTENT
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

    struct AllocExtent
    {
      Uint32 m_table;
      Uint32 m_fragment;
      Uint32 m_page_no;
      Uint32 m_file_no;
      Uint32 m_type_length;
    };

    struct FreeExtent
    {
      Uint32 m_table;
      Uint32 m_fragment;
      Uint32 m_page_no;
      Uint32 m_file_no;
      Uint32 m_type_length;
    };
  };
  
  Extent_info_pool c_extent_pool;
  Extent_info_hash c_extent_hash;
  Page_request_pool c_page_request_pool;

  typedef Ptr<Tablerec> TablerecPtr;

  struct storedProc {
    Uint32 storedLinkFirst;
    Uint32 storedLinkLast;
    Uint32 storedCounter;
    Uint32 nextPool;
    Uint16 storedCode;
    Uint16 storedProcLength;
};

typedef Ptr<storedProc> StoredProcPtr;

ArrayPool<storedProc> c_storedProcPool;

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
    // request cannot use signal class due to extra members
    Uint32 m_request[BuildIndxReq::SignalLength];
    Uint8  m_build_vs;          // varsize pages
    Uint32 m_indexId;           // the index
    Uint32 m_fragNo;            // fragment number under Tablerec
    Uint32 m_pageId;            // logical fragment page id
    Uint32 m_tupleNo;           // tuple number on page
    Uint32 m_buildRef;          // Where to send tuples
    BuildIndxRef::ErrorCode m_errorCode;
    union {
      Uint32 nextPool;
      Uint32 nextList;
    };
    Uint32 prevList;
  };
  typedef Ptr<BuildIndexRec> BuildIndexPtr;
  ArrayPool<BuildIndexRec> c_buildIndexPool;
  DLList<BuildIndexRec> c_buildIndexList;
  Uint32 c_noOfBuildIndexRec;

  /**
   * Reference to variable part when a tuple is chained
   */
  struct Var_part_ref 
  {
#ifdef NDB_32BIT_VAR_REF
    /*
      In versions prior to ndb 6.1.6, 6.2.1 and mysql 5.1.17
      Running this code limits DataMemory to 16G, also online
      upgrade not possible between versions
     */
    Uint32 m_ref;
    STATIC_CONST( SZ32 = 1 );

    void copyout(Local_key* dst) const {
      dst->m_page_no = m_ref >> MAX_TUPLES_BITS;
      dst->m_page_idx = m_ref & MAX_TUPLES_PER_PAGE;
    }

    void assign(const Local_key* src) {
      m_ref = (src->m_page_no << MAX_TUPLES_BITS) | src->m_page_idx;
    }
#else
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
#endif    
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
      Uint32 m_base_record_ref;  // For disk tuple, ref to MM tuple
    };
    Uint32 m_header_bits;      // Header word
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
    */
    STATIC_CONST( TUP_VERSION_MASK = 0xFFFF );
    STATIC_CONST( COPY_TUPLE  = 0x00010000 ); // Is this a copy tuple
    STATIC_CONST( DISK_PART   = 0x00020000 ); // Is there a disk part
    STATIC_CONST( DISK_ALLOC  = 0x00040000 ); // Is disk part allocated
    STATIC_CONST( DISK_INLINE = 0x00080000 ); // Is disk inline
    STATIC_CONST( ALLOC       = 0x00100000 ); // Is record allocated now
    STATIC_CONST( MM_SHRINK   = 0x00200000 ); // Has MM part shrunk
    STATIC_CONST( MM_GROWN    = 0x00400000 ); // Has MM part grown
    STATIC_CONST( FREED       = 0x00800000 ); // Is freed
    STATIC_CONST( LCP_SKIP    = 0x01000000 ); // Should not be returned in LCP
    STATIC_CONST( LCP_KEEP    = 0x02000000 ); // Should be returned in LCP
    STATIC_CONST( FREE        = 0x02800000 ); // Is free
    STATIC_CONST( VAR_PART    = 0x04000000 ); // Is there a varpart

    Tuple_header() {}
    Uint32 get_tuple_version() const { 
      return m_header_bits & TUP_VERSION_MASK;
    }
    void set_tuple_version(Uint32 version) { 
      m_header_bits= 
	(m_header_bits & ~(Uint32)TUP_VERSION_MASK) | 
	(version & TUP_VERSION_MASK);
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
      return m_data + tabPtrP->m_offsets[MM].m_disk_ref_offset;
    }

    const Uint32* get_disk_ref_ptr(const Tablerec* tabPtrP) const {
      return m_data + tabPtrP->m_offsets[MM].m_disk_ref_offset;
    }

    Uint32 *get_mm_gci(const Tablerec* tabPtrP){
      assert(tabPtrP->m_bits & Tablerec::TR_RowGCI);
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

struct KeyReqStruct {
/**
 * These variables are used as temporary storage during execution of the
 * TUPKEYREQ signal.
 * The first set of variables defines a number of variables needed for
 * the fix part of the tuple.
 *
 * The second part defines a number of commonly used meta data variables.
 *
 * The third set of variables defines a set of variables needed for the
 * variable part.
 *
 * The fourth part is variables needed only for updates and inserts.
 *
 * The fifth part is a long array of real lengths which is is put last
 * for cache memory reasons. This is part of the variable part and
 * contains the real allocated lengths whereas the tuple contains
 * the length of attribute stored.
 */
  Tuple_header *m_tuple_ptr;

  Uint32 check_offset[2];

  TableDescriptor *attr_descr;
  Uint32          max_read;
  Uint32          out_buf_index;
  Uint32          out_buf_bits;
  Uint32          in_buf_index;
  Uint32          in_buf_len;
  Uint32          attr_descriptor;
  bool            xfrm_flag;

  /* Flag: is tuple in expanded or in shrunken/stored format? */
  bool is_expanded;

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

  Tuple_header *m_disk_ptr;
  PagePtr m_page_ptr;
  PagePtr m_varpart_page_ptr;    // could be same as m_page_ptr_p
  PagePtr m_disk_page_ptr;       //
  Local_key m_row_id;
  
  bool            dirty_op;
  bool            interpreted_exec;
  bool            last_row;
  bool            m_use_rowid;

  Signal*         signal;
  Uint32 no_fired_triggers;
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
  Uint32 max_attr_id_updated;
  Uint32 no_changed_attrs;
  BlockReference TC_ref;
  BlockReference rec_blockref;
  bool change_mask_calculated;
  /*
   * A bit mask where a bit set means that the update or insert
   * was updating this record.
   */
  Bitmask<MAXNROFATTRIBUTESINWORDS> changeMask;
  Uint16 var_pos_array[2*MAX_ATTRIBUTES_IN_TABLE + 1];
  OperationrecPtr prevOpPtr;
};

  friend class Undo_buffer;
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

// updateAttributes module
  Uint32          terrorCode;

public:
  Dbtup(Block_context&, Pgman*);
  virtual ~Dbtup();

  /*
   * TUX uses logical tuple address when talking to ACC and LQH.
   */
  void tuxGetTupAddr(Uint32 fragPtrI, Uint32 pageId, Uint32 pageOffset, Uint32& tupAddr);

  /*
   * TUX index in TUP has single Uint32 array attribute which stores an
   * index node.  TUX reads and writes the node directly via pointer.
   */
  int tuxAllocNode(Signal* signal, Uint32 fragPtrI, Uint32& pageId, Uint32& pageOffset, Uint32*& node);
  void tuxFreeNode(Signal* signal, Uint32 fragPtrI, Uint32 pageId, Uint32 pageOffset, Uint32* node);
  void tuxGetNode(Uint32 fragPtrI, Uint32 pageId, Uint32 pageOffset, Uint32*& node);

  /*
   * TUX reads primary table attributes for index keys.  Tuple is
   * specified by location of original tuple and version number.  Input
   * is attribute ids in AttributeHeader format.  Output is attribute
   * data with headers.  Uses readAttributes with xfrm option set.
   * Returns number of words or negative (-terrorCode) on error.
   */
  int tuxReadAttrs(Uint32 fragPtrI, Uint32 pageId, Uint32 pageOffset, Uint32 tupVersion, const Uint32* attrIds, Uint32 numAttrs, Uint32* dataOut);

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

  /*
   * TUX checks if tuple is visible to scan.
   */
  bool tuxQueryTh(Uint32 fragPtrI, Uint32 pageId, Uint32 pageIndex, Uint32 tupVersion, Uint32 transId1, Uint32 transId2, bool dirty, Uint32 savepointId);

  int load_diskpage(Signal*, Uint32 opRec, Uint32 fragPtrI, 
		    Uint32 local_key, Uint32 flags);

  int load_diskpage_scan(Signal*, Uint32 opRec, Uint32 fragPtrI, 
			 Uint32 local_key, Uint32 flags);

  int alloc_page(Tablerec*, Fragrecord*, PagePtr*,Uint32 page_no);
  
  void start_restore_lcp(Uint32 tableId, Uint32 fragmentId);
  void complete_restore_lcp(Uint32 tableId, Uint32 fragmentId);

  int nr_read_pk(Uint32 fragPtr, const Local_key*, Uint32* dataOut, bool&copy);
  int nr_update_gci(Uint32 fragPtr, const Local_key*, Uint32 gci);
  int nr_delete(Signal*, Uint32, Uint32 fragPtr, const Local_key*, Uint32 gci);

  void nr_delete_page_callback(Signal*, Uint32 op, Uint32 page);
  void nr_delete_log_buffer_callback(Signal*, Uint32 op, Uint32 page);

  bool get_frag_info(Uint32 tableId, Uint32 fragId, Uint32* maxPage);
private:
  BLOCK_DEFINES(Dbtup);

  // Transit signals
  void execDEBUG_SIG(Signal* signal);
  void execCONTINUEB(Signal* signal);

  // Received signals
  void execLCP_FRAG_ORD(Signal*signal);
  void execDUMP_STATE_ORD(Signal* signal);
  void execSEND_PACKED(Signal* signal);
  void execSTTOR(Signal* signal);
  void execTUP_LCPREQ(Signal* signal);
  void execEND_LCPREQ(Signal* signal);
  void execSTART_RECREQ(Signal* signal);
  void execMEMCHECKREQ(Signal* signal);
  void execTUPSEIZEREQ(Signal* signal);
  void execTUPRELEASEREQ(Signal* signal);
  void execSTORED_PROCREQ(Signal* signal);
  void execTUPFRAGREQ(Signal* signal);
  void execTUP_ADD_ATTRREQ(Signal* signal);
  void execTUP_COMMITREQ(Signal* signal);
  void execTUP_ABORTREQ(Signal* signal);
  void execNDB_STTOR(Signal* signal);
  void execREAD_CONFIG_REQ(Signal* signal);
  void execDROP_TAB_REQ(Signal* signal);
  void execALTER_TAB_REQ(Signal* signal);
  void execTUP_DEALLOCREQ(Signal* signal);
  void execTUP_WRITELOG_REQ(Signal* signal);

  // Ordered index related
  void execBUILDINDXREQ(Signal* signal);
  void buildIndex(Signal* signal, Uint32 buildPtrI);
  void buildIndexReply(Signal* signal, const BuildIndexRec* buildRec);

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
// Logically there is one request TUPKEYREQ which requests to read/write data
// of one tuple in the database. Since the definition of what to read and write
// can be bigger than the maximum signal size we segment the signal. The definition
// of what to read/write/interpreted program is sent before the TUPKEYREQ signal.
//
// ---> ATTRINFO
// ...
// ---> ATTRINFO
// ---> TUPKEYREQ
// The number of ATTRINFO signals can be anything between 0 and upwards.
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
  void execTUPKEYREQ(Signal* signal);
  void disk_page_load_callback(Signal*, Uint32 op, Uint32 page);
  void disk_page_load_scan_callback(Signal*, Uint32 op, Uint32 page);

//------------------------------------------------------------------
//------------------------------------------------------------------
  void execATTRINFO(Signal* signal);
public:
  void receive_attrinfo(Signal*, Uint32 op, const Uint32* data, Uint32 len);
private:

// Trigger signals
//------------------------------------------------------------------
//------------------------------------------------------------------
  void execCREATE_TRIG_REQ(Signal* signal);

//------------------------------------------------------------------
//------------------------------------------------------------------
  void execDROP_TRIG_REQ(Signal* signal);

// *****************************************************************
// Support methods for ATTRINFO.
// *****************************************************************
//------------------------------------------------------------------
//------------------------------------------------------------------
  void handleATTRINFOforTUPKEYREQ(Signal* signal,
				  const Uint32* data,
                                  Uint32 length,
                                  Operationrec * regOperPtr);

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
                      KeyReqStruct* req_struct);

//------------------------------------------------------------------
//------------------------------------------------------------------
  int handleDeleteReq(Signal* signal,
                      Operationrec* regOperPtr,
                      Fragrecord* regFragPtr,
                      Tablerec* regTabPtr,
                      KeyReqStruct* req_struct,
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
  int interpreterNextLab(Signal* signal,
                         KeyReqStruct *req_struct,
                         Uint32* logMemory,
                         Uint32* mainProgram,
                         Uint32 TmainProgLen,
                         Uint32* subroutineProg,
                         Uint32 TsubroutineLen,
			 Uint32 * tmpArea,
			 Uint32 tmpAreaSz);

// *****************************************************************
// Signal Sending methods.
// *****************************************************************
//------------------------------------------------------------------
//------------------------------------------------------------------
  void sendReadAttrinfo(Signal* signal,
                        KeyReqStruct *req_struct,
                        Uint32 TnoOfData,
                        const Operationrec * regOperPtr);

//------------------------------------------------------------------
//------------------------------------------------------------------
  void sendLogAttrinfo(Signal* signal,
                       Uint32 TlogSize,
                       Operationrec * regOperPtr);

//------------------------------------------------------------------
//------------------------------------------------------------------
  void sendTUPKEYCONF(Signal* signal,
                      KeyReqStruct *req_struct,
                      Operationrec * regOperPtr); 

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
  bool readDiskVarSizeNULLable(Uint8*, KeyReqStruct*, AttributeHeader*,Uint32);
  bool readDiskVarSizeNotNULL(Uint8*, KeyReqStruct*, AttributeHeader*, Uint32);

  bool updateDiskFixedSizeNULLable(Uint32*, KeyReqStruct*, Uint32);
  bool updateDiskFixedSizeNotNULL(Uint32*, KeyReqStruct*, Uint32);

  bool updateDiskVarSizeNULLable(Uint32*, KeyReqStruct *, Uint32);
  bool updateDiskVarSizeNotNULL(Uint32*, KeyReqStruct *, Uint32);
  
  bool readDiskBitsNULLable(Uint8*, KeyReqStruct*, AttributeHeader*, Uint32);
  bool readDiskBitsNotNULL(Uint8*, KeyReqStruct*, AttributeHeader*, Uint32);
  bool updateDiskBitsNULLable(Uint32*, KeyReqStruct*, Uint32);
  bool updateDiskBitsNotNULL(Uint32*, KeyReqStruct*, Uint32);


  /* Alter table methods. */
  void handleAlterTabPrepare(Signal *signal, const Tablerec *regTabPtr);
  void sendAlterTabRef(Signal *signal, AlterTabReq *req, Uint32 errorCode);
  void sendAlterTabConf(Signal *, AlterTabReq *, Uint32 clientData=RNIL);
  void handleAlterTableCommit(Signal *signal,
                              AlterTabOperationPtr regAlterTabOpPtr,
                              Tablerec *regTabPtr);
  void handleAlterTableAbort(Signal *signal,
                             AlterTabOperationPtr regAlterTabOpPtr,
                             Tablerec *regTabPtr);
  void handleCharsetPos(Uint32 csNumber, CHARSET_INFO** charsetArray,
                        Uint32 noOfCharsets,
                        Uint32 & charsetIndex, Uint32 & attrDes2);
  void computeTableMetaData(Tablerec *regTabPtr);

//------------------------------------------------------------------
//------------------------------------------------------------------
  bool nullFlagCheck(KeyReqStruct *req_struct, Uint32  attrDes2);
  bool disk_nullFlagCheck(KeyReqStruct *req_struct, Uint32 attrDes2);
  Uint32 read_pseudo(const Uint32 *, Uint32, KeyReqStruct*, Uint32*);
  Uint32 read_packed(const Uint32 *, Uint32, KeyReqStruct*, Uint32*);

  /* Fast bit counting (16 instructions on x86_64, gcc -O3). */
  static inline uint32_t count_bits(uint32_t x)
  {
    x= x - ((x>>1) & 0x55555555);
    x= (x & 0x33333333) + ((x>>2) & 0x33333333);
    x= (x + (x>>4)) & 0x0f0f0f0f;
    x= (x*0x01010101) >> 24;
    return x;
  }

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
  Uint32 get_frag_page_id(Uint32 real_page_id);
  Uint32 get_fix_page_offset(Uint32 page_index, Uint32 tuple_size);

  Uint32 decr_tup_version(Uint32 tuple_version);
  void set_change_mask_state(Operationrec * const, ChangeMaskState);
  ChangeMaskState get_change_mask_state(Operationrec * const);
  void update_change_mask_info(KeyReqStruct * const, Operationrec * const);
  void set_change_mask_info(KeyReqStruct * const, Operationrec * const);

//------------------------------------------------------------------
//------------------------------------------------------------------
  void copyAttrinfo(Operationrec * regOperPtr, Uint32*  inBuffer);

//------------------------------------------------------------------
//------------------------------------------------------------------
  void initOpConnection(Operationrec* regOperPtr);

//------------------------------------------------------------------
//------------------------------------------------------------------
  void initOperationrec(Signal* signal);

//------------------------------------------------------------------
//------------------------------------------------------------------
  int initStoredOperationrec(Operationrec* regOperPtr,
                             KeyReqStruct* req_struct,
                             Uint32 storedId);

//------------------------------------------------------------------
//------------------------------------------------------------------
  bool insertActiveOpList(OperationrecPtr, KeyReqStruct* req_struct);

//------------------------------------------------------------------
//------------------------------------------------------------------

//------------------------------------------------------------------
//------------------------------------------------------------------
  void bufferTRANSID_AI(Signal* signal, BlockReference aRef, Uint32 Tlen);

//------------------------------------------------------------------
// Trigger handling routines
//------------------------------------------------------------------
  DLList<TupTriggerData>*
  findTriggerList(Tablerec* table,
                  TriggerType::Value ttype,
                  TriggerActionTime::Value ttime,
                  TriggerEvent::Value tevent);

  bool createTrigger(Tablerec* table, const CreateTrigReq* req);

  Uint32 dropTrigger(Tablerec* table,
		     const DropTrigReq* req,
		     BlockNumber sender);

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

#if 0
  void checkDeferredTriggers(Signal* signal, 
                             Operationrec* regOperPtr,
                             Tablerec* regTablePtr);
#endif
  void checkDetachedTriggers(KeyReqStruct *req_struct,
                             Operationrec* regOperPtr,
                             Tablerec* regTablePtr,
                             bool disk);

  void fireImmediateTriggers(KeyReqStruct *req_struct,
                             DLList<TupTriggerData>& triggerList, 
                             Operationrec* regOperPtr,
                             bool disk);

  void fireDeferredTriggers(KeyReqStruct *req_struct,
                            DLList<TupTriggerData>& triggerList,
                            Operationrec* regOperPtr);

  void fireDetachedTriggers(KeyReqStruct *req_struct,
                            DLList<TupTriggerData>& triggerList,
                            Operationrec* regOperPtr,
                            bool disk);

  void executeTriggers(KeyReqStruct *req_struct,
                       DLList<TupTriggerData>& triggerList,
                       Operationrec* regOperPtr,
                       bool disk);

  void executeTrigger(KeyReqStruct *req_struct,
                      TupTriggerData* trigPtr, 
                      Operationrec* regOperPtr,
                      bool disk);

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

  void sendFireTrigOrd(Signal* signal, 
                       KeyReqStruct *req_struct,
                       Operationrec * regOperPtr,
                       TupTriggerData* trigPtr,
		       Uint32 fragmentId,
                       Uint32 noPrimKeySignals, 
                       Uint32 noBeforeSignals, 
                       Uint32 noAfterSignals);

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

// *****************************************************************
// Error Handling routines.
// *****************************************************************
//------------------------------------------------------------------
//------------------------------------------------------------------
  int TUPKEY_abort(Signal* signal, int error_type);

//------------------------------------------------------------------
//------------------------------------------------------------------
  void tupkeyErrorLab(Signal* signal);
  void do_tup_abortreq(Signal*, Uint32 flags);

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
  void setup_fixed_part(KeyReqStruct* req_struct,
			Operationrec* regOperPtr,
			Tablerec* regTabPtr);
  
  void send_TUPKEYREF(Signal* signal,
                      Operationrec* regOperPtr);
  void early_tupkey_error(Signal* signal);

  void printoutTuplePage(Uint32 fragid, Uint32 pageid, Uint32 printLimit);

  bool checkUpdateOfPrimaryKey(KeyReqStruct *req_struct,
                               Uint32* updateBuffer,
                               Tablerec* regTabPtr);

  void setNullBits(Uint32*, Tablerec* regTabPtr);
  bool checkNullAttributes(KeyReqStruct * const, Tablerec* const);
  bool find_savepoint(OperationrecPtr& loopOpPtr, Uint32 savepointId);
  bool setup_read(KeyReqStruct* req_struct,
		  Operationrec* regOperPtr,
		  Fragrecord* regFragPtr,
		  Tablerec* regTabPtr,
		  bool disk);
  
  Uint32 calculateChecksum(Tuple_header*, Tablerec* regTabPtr);
  void setChecksum(Tuple_header*, Tablerec* regTabPtr);

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

  void calculateChangeMask(Page* PagePtr,
                           Tablerec* regTabPtr,
                           KeyReqStruct * req_struct);

  void updateGcpId(KeyReqStruct *req_struct,
                   Operationrec* regOperPtr,
                   Fragrecord* regFragPtr,
                   Tablerec* regTabPtr);

  void setTupleStateOnPreviousOps(Uint32 prevOpIndex);
  void copyMem(Signal* signal, Uint32 sourceIndex, Uint32 destIndex);

  void freeAllAttrBuffers(Operationrec*  const regOperPtr);
  void freeAttrinbufrec(Uint32 anAttrBufRec);
  void removeActiveOpList(Operationrec*  const regOperPtr, Tuple_header*);

  void updatePackedList(Signal* signal, Uint16 ahostIndex);

  void setUpDescriptorReferences(Uint32 descriptorReference,
                                 Tablerec* regTabPtr,
                                 const Uint32* offset);
  void setupDynDescriptorReferences(Uint32 dynDescr,
                                    Tablerec* const regTabPtr,
                                    const Uint32* offset);
  void setUpKeyArray(Tablerec* regTabPtr);
  bool addfragtotab(Tablerec* regTabPtr, Uint32 fragId, Uint32 fragIndex);
  void deleteFragTab(Tablerec* regTabPtr, Uint32 fragId);
  void abortAddFragOp(Signal* signal);
  void releaseTabDescr(Tablerec* regTabPtr);
  void getFragmentrec(FragrecordPtr& regFragPtr, Uint32 fragId, Tablerec* regTabPtr);

  void initialiseRecordsLab(Signal* signal, Uint32 switchData, Uint32, Uint32);
  void initializeAttrbufrec();
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

  void initTab(Tablerec* regTabPtr);

  void startphase3Lab(Signal* signal, Uint32 config1, Uint32 config2);

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
  void drop_fragment_fsremove(Signal*, TablerecPtr, FragrecordPtr);
  void drop_fragment_fsremove_done(Signal*, TablerecPtr, FragrecordPtr);

  // Initialisation
  void initData();
  void initRecords();

  void deleteScanProcedure(Signal* signal, Operationrec* regOperPtr);
  void copyProcedure(Signal* signal,
                     TablerecPtr regTabPtr,
                     Operationrec* regOperPtr);
  void scanProcedure(Signal* signal,
                     Operationrec* regOperPtr,
                     Uint32 lenAttrInfo);
  void storedSeizeAttrinbufrecErrorLab(Signal* signal,
                                       Operationrec* regOperPtr,
                                       Uint32 errorCode);
  bool storedProcedureAttrInfo(Signal* signal,
                               Operationrec* regOperPtr,
			       const Uint32* data,
                               Uint32 length,
                               bool copyProc);

//-----------------------------------------------------------------------------
// Table Descriptor Memory Manager
//-----------------------------------------------------------------------------

// Public methods
  Uint32 getTabDescrOffsets(Uint32, Uint32, Uint32, Uint32*);
  Uint32 getDynTabDescrOffsets(Uint32 MaskSize, Uint32* offset);
  Uint32 allocTabDescr(Uint32 allocSize);
  void freeTabDescr(Uint32 retRef, Uint32 retNo, bool normal = true);
  Uint32 getTabDescrWord(Uint32 index);
  void setTabDescrWord(Uint32 index, Uint32 word);

// Private methods
  Uint32 sizeOfReadFunction();
  void   removeTdArea(Uint32 tabDesRef, Uint32 list);
  void   insertTdArea(Uint32 tabDesRef, Uint32 list);
  void   itdaMergeTabDescr(Uint32& retRef, Uint32& retNo, bool normal);
#ifdef VM_TRACE
  void verifytabdes();
#endif

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
  void allocConsPages(Uint32 noOfPagesToAllocate,
                      Uint32& noOfPagesAllocated,
                      Uint32& allocPageRef);
  void returnCommonArea(Uint32 retPageRef, Uint32 retNo);
  void initializePage();

  Uint32 nextHigherTwoLog(Uint32 input);


//------------------------------------------------------------------------------------------------------
// Page Mapper, convert logical page id's to physical page id's
// The page mapper also handles the pages allocated to the fragment.
//------------------------------------------------------------------------------------------------------
//
// Public methods
  Uint32 getRealpid(Fragrecord* regFragPtr, Uint32 logicalPageId);
  Uint32 getRealpidCheck(Fragrecord* regFragPtr, Uint32 logicalPageId);
  Uint32 getNoOfPages(Fragrecord* regFragPtr);
  Uint32 getEmptyPage(Fragrecord* regFragPtr);
  Uint32 allocFragPage(Fragrecord* regFragPtr);
  void releaseFragPages(Fragrecord* regFragPtr);
  Uint32 get_empty_var_page(Fragrecord* frag_ptr);
  void init_page(Fragrecord*, PagePtr, Uint32 page_no);
  
// Private methods
  void errorHandler(Uint32 errorCode);

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
  void remove_free_page(Fragrecord*, Var_page*, Uint32);
  void insert_free_page(Fragrecord*, Var_page*, Uint32);

//---------------------------------------------------------------
// Fixed Allocator
// Allocates and deallocates tuples of fixed size on a fragment.
//---------------------------------------------------------------
//
// Public methods
  Uint32* alloc_var_rec(Fragrecord*, Tablerec*, Uint32, Local_key*, Uint32*);
  void free_var_rec(Fragrecord*, Tablerec*, Local_key*, Ptr<Page>);
  Uint32* alloc_var_part(Fragrecord*, Tablerec*, Uint32, Local_key*);
  Uint32 *realloc_var_part(Fragrecord*, Tablerec*, 
                           PagePtr, Var_part_ref*, Uint32, Uint32);
  
  void validate_page(Tablerec*, Var_page* page);
  
  Uint32* alloc_fix_rec(Fragrecord*const, Tablerec*const, Local_key*,
                        Uint32*);
  void free_fix_rec(Fragrecord*, Tablerec*, Local_key*, Fix_page*);
  
  Uint32* alloc_fix_rowid(Fragrecord*, Tablerec*, Local_key*, Uint32 *);
  Uint32* alloc_var_rowid(Fragrecord*, Tablerec*, Uint32, Local_key*, Uint32*);
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
  FragrecordPtr   fragptr;
  OperationrecPtr operPtr;
  TablerecPtr     tabptr;

// readAttributes and updateAttributes module
//------------------------------------------------------------------------------------------------------
// Common stored variables. Variables that have a valid value always.
//------------------------------------------------------------------------------------------------------
  Attrbufrec *attrbufrec;
  Uint32 cfirstfreeAttrbufrec;
  Uint32 cnoOfAttrbufrec;
  Uint32 cnoFreeAttrbufrec;

  Fragoperrec *fragoperrec;
  Uint32 cfirstfreeFragopr;
  Uint32 cnoOfFragoprec;

  Fragrecord *fragrecord;
  Uint32 cfirstfreefrag;
  Uint32 cnoOfFragrec;

  AlterTabOperation *alterTabOperRec;
  Uint32 cfirstfreeAlterTabOp;
  Uint32 cnoOfAlterTabOps;

  HostBuffer *hostBuffer;

  DynArr256Pool c_page_map_pool;
  ArrayPool<Operationrec> c_operation_pool;

  ArrayPool<Page> c_page_pool;
  Uint32 cnoOfAllocatedPages;
  Uint32 c_no_of_pages;

  /* read ahead in pages during disk order scan */
  Uint32 m_max_page_read_ahead;
  
  Tablerec *tablerec;
  Uint32 cnoOfTablerec;

  TableDescriptor *tableDescriptor;
  Uint32 cnoOfTabDescrRec;
  
  Uint32 cdata[32];
  Uint32 cdataPages[16];
  Uint32 cpackedListIndex;
  Uint32 cpackedList[MAX_NODES];
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

 // A little bit bigger to cover overwrites in copy algorithms (16384 real size).
#define ZATTR_BUFFER_SIZE 16384
  Uint32 clogMemBuffer[ZATTR_BUFFER_SIZE + 16];
  Uint32 coutBuffer[ZATTR_BUFFER_SIZE + 16];
  Uint32 cinBuffer[ZATTR_BUFFER_SIZE + 16];
  Uint32 ctemp_page[ZWORDS_ON_PAGE];
  Uint32 ctemp_var_record[ZWORDS_ON_PAGE];
  Uint32 totNoOfPagesAllocated;

  // Trigger variables
  Uint32 c_maxTriggersPerTable;
  Uint32 c_memusage_report_frequency;

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

  void expand_tuple(KeyReqStruct*, Uint32 sizes[4], Tuple_header*org, 
		    const Tablerec*, bool disk);
  void shrink_tuple(KeyReqStruct*, Uint32 sizes[2], const Tablerec*,
		    bool disk);
  
  Uint32* get_ptr(Var_part_ref);
  Uint32* get_ptr(PagePtr*, Var_part_ref);
  Uint32* get_ptr(PagePtr*, const Local_key*, const Tablerec*);
  Uint32* get_dd_ptr(PagePtr*, const Local_key*, const Tablerec*);
  Uint32 get_len(Ptr<Page>* pagePtr, Var_part_ref ref);

  /**
   * prealloc space from disk
   *   key.m_file_no  contains file no
   *   key.m_page_no  contains disk page
   *   key.m_page_idx contains byte preallocated
   */
  int disk_page_prealloc(Signal*, Ptr<Fragrecord>, Local_key*, Uint32);
  void disk_page_prealloc_dirty_page(Disk_alloc_info&, 
				     Ptr<Page>, Uint32, Uint32);
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
		       Tablerec*, Fragrecord*, Local_key*, PagePtr, Uint32);
  void disk_page_free(Signal*, 
		      Tablerec*, Fragrecord*, Local_key*, PagePtr, Uint32);
  
  void disk_page_commit_callback(Signal*, Uint32 opPtrI, Uint32 page_id);  
  
  void disk_page_log_buffer_callback(Signal*, Uint32 opPtrI, Uint32); 

  void disk_page_alloc_extent_log_buffer_callback(Signal*, Uint32, Uint32);
  void disk_page_free_extent_log_buffer_callback(Signal*, Uint32, Uint32);
  
  Uint64 disk_page_undo_alloc(Page*, const Local_key*,
			      Uint32 sz, Uint32 gci, Uint32 logfile_group_id);

  Uint64 disk_page_undo_update(Page*, const Local_key*,
			       const Uint32*, Uint32,
			       Uint32 gci, Uint32 logfile_group_id);
  
  Uint64 disk_page_undo_free(Page*, const Local_key*,
			     const Uint32*, Uint32 sz,
			     Uint32 gci, Uint32 logfile_group_id);

  void undo_createtable_callback(Signal* signal, Uint32 opPtrI, Uint32 unused);
  void undo_createtable_logsync_callback(Signal* signal, Uint32, Uint32);

  void drop_table_log_buffer_callback(Signal*, Uint32, Uint32);
  void drop_table_logsync_callback(Signal*, Uint32, Uint32);

  void disk_page_set_dirty(Ptr<Page>);
  void restart_setup_page(Disk_alloc_info&, Ptr<Page>);
  void update_extent_pos(Disk_alloc_info&, Ptr<Extent_info>);
  
  /**
   * Disk restart code
   */
public:
  int disk_page_load_hook(Uint32 page_id);
  
  void disk_page_unmap_callback(Uint32 when, Uint32 page, Uint32 dirty_count);
  
  int disk_restart_alloc_extent(Uint32 tableId, Uint32 fragId, 
				const Local_key* key, Uint32 pages);
  void disk_restart_page_bits(Uint32 tableId, Uint32 fragId,
			      const Local_key*, Uint32 bits);
  void disk_restart_undo(Signal* signal, Uint64 lsn,
			 Uint32 type, const Uint32 * ptr, Uint32 len);

  struct Apply_undo 
  {
    Uint32 m_type, m_len;
    const Uint32* m_ptr;
    Uint64 m_lsn;
    Ptr<Tablerec> m_table_ptr;
    Ptr<Fragrecord> m_fragment_ptr;
    Ptr<Page> m_page_ptr;
    Ptr<Extent_info> m_extent_ptr;
    Local_key m_key;
  };

  void disk_restart_lcp_id(Uint32 table, Uint32 frag, Uint32 lcpId);
  
private:
  void disk_restart_undo_next(Signal*);
  void disk_restart_undo_lcp(Uint32, Uint32, Uint32 flag, Uint32 lcpId);
  void disk_restart_undo_callback(Signal* signal, Uint32, Uint32);
  void disk_restart_undo_alloc(Apply_undo*);
  void disk_restart_undo_update(Apply_undo*);
  void disk_restart_undo_free(Apply_undo*);
  void disk_restart_undo_page_bits(Signal*, Apply_undo*);

#ifdef VM_TRACE
  void verify_page_lists(Disk_alloc_info&);
#else
  void verify_page_lists(Disk_alloc_info&) {}
#endif
  
  void findFirstOp(OperationrecPtr&);
  void commit_operation(Signal*, Uint32, Tuple_header*, PagePtr,
			Operationrec*, Fragrecord*, Tablerec*);
  
  void dealloc_tuple(Signal* signal, Uint32, Page*, Tuple_header*, 
		     Operationrec*, Fragrecord*, Tablerec*);
  
  int handle_size_change_after_update(KeyReqStruct* req_struct,
				      Tuple_header* org,
				      Operationrec*,
				      Fragrecord* regFragPtr,
				      Tablerec* regTabPtr,
				      Uint32 sizes[4]);

  /**
   * Setup all pointer on keyreqstruct to prepare for read
   *   req_struct->m_tuple_ptr is set to tuple to read
   */
  void prepare_read(KeyReqStruct*, Tablerec* const, bool disk);

  /* For debugging, dump the contents of a tuple. */
  void dump_tuple(const KeyReqStruct* req_struct, const Tablerec* tabPtrP);
};

#if 0
inline
Uint32
Dbtup::get_frag_page_id(Uint32 real_page_id)
{
  PagePtr real_page_ptr;
  real_page_ptr.i= real_page_id;
  ptrCheckGuard(real_page_ptr, cnoOfPage, cpage);
  return real_page_ptr.p->frag_page_id;
}
#endif

inline
Dbtup::TransState
Dbtup::get_trans_state(Operationrec * regOperPtr)
{
  return (Dbtup::TransState)regOperPtr->op_struct.trans_state;
}

inline
void
Dbtup::set_trans_state(Operationrec* regOperPtr,
                       Dbtup::TransState trans_state)
{
  regOperPtr->op_struct.trans_state= (Uint32)trans_state;
}

inline
Dbtup::TupleState
Dbtup::get_tuple_state(Operationrec * regOperPtr)
{
  return (Dbtup::TupleState)regOperPtr->op_struct.tuple_state;
}

inline
void
Dbtup::set_tuple_state(Operationrec* regOperPtr,
                       Dbtup::TupleState tuple_state)
{
  regOperPtr->op_struct.tuple_state= (Uint32)tuple_state;
}


inline
Uint32
Dbtup::decr_tup_version(Uint32 tup_version)
{
  return (tup_version - 1) & ZTUP_VERSION_MASK;
}

inline
Dbtup::ChangeMaskState
Dbtup::get_change_mask_state(Operationrec * regOperPtr)
{
  return (Dbtup::ChangeMaskState)regOperPtr->op_struct.change_mask_state;
}

inline
void
Dbtup::set_change_mask_state(Operationrec * regOperPtr,
                             ChangeMaskState new_state)
{
  regOperPtr->op_struct.change_mask_state= (Uint32)new_state;
}

inline
void
Dbtup::update_change_mask_info(KeyReqStruct * req_struct,
                               Operationrec * regOperPtr)
{
  if (req_struct->max_attr_id_updated == 0) {
    if (get_change_mask_state(regOperPtr) == USE_SAVED_CHANGE_MASK) {
      // add new changes
      regOperPtr->saved_change_mask[0] |= req_struct->changeMask.getWord(0);
      regOperPtr->saved_change_mask[1] |= req_struct->changeMask.getWord(1);
    }
  } else {
    if (req_struct->no_changed_attrs < 16) {
      set_change_mask_state(regOperPtr, RECALCULATE_CHANGE_MASK);
    } else {
      set_change_mask_state(regOperPtr, SET_ALL_MASK);
    }
  }
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

#endif
