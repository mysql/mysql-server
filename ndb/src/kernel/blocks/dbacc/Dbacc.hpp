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

#ifndef DBACC_H
#define DBACC_H



#include <pc.hpp>
#include <SimulatedBlock.hpp>

// primary key is stored in TUP
#include <Dbtup.hpp>

#ifdef DBACC_C
// Debug Macros
#define dbgWord32(ptr, ind, val) 

/*
#define dbgWord32(ptr, ind, val) \
if(debug_jan){ \
tmp_val = val; \
switch(ind){ \
case 1: strcpy(tmp_string, "ZPOS_PAGE_TYPE   "); \
break; \
case 2: strcpy(tmp_string, "ZPOS_NO_ELEM_IN_PAGE"); \
break; \
case 3: strcpy(tmp_string, "ZPOS_CHECKSUM    "); \
break; \
case 4: strcpy(tmp_string, "ZPOS_OVERFLOWREC  "); \
break; \
case 5: strcpy(tmp_string, "ZPOS_FREE_AREA_IN_PAGE"); \
break; \
case 6: strcpy(tmp_string, "ZPOS_LAST_INDEX   "); \
break; \
case 7: strcpy(tmp_string, "ZPOS_INSERT_INDEX  "); \
break; \
case 8: strcpy(tmp_string, "ZPOS_ARRAY_POS    "); \
break; \
case 9: strcpy(tmp_string, "ZPOS_NEXT_FREE_INDEX"); \
break; \
case 10: strcpy(tmp_string, "ZPOS_NEXT_PAGE   "); \
break; \
case 11: strcpy(tmp_string, "ZPOS_PREV_PAGE   "); \
break; \
default: sprintf(tmp_string, "%-20d", ind);\
} \
ndbout << "Ptr: " << ptr.p->word32 << " \tIndex: " << tmp_string << " \tValue: " << tmp_val << " \tLINE: " << __LINE__ << endl; \
}\
*/

#define dbgUndoword(ptr, ind, val)

// Constants
/** ------------------------------------------------------------------------ 
 *   THESE ARE CONSTANTS THAT ARE USED FOR DEFINING THE SIZE OF BUFFERS, THE
 *   SIZE OF PAGE HEADERS, THE NUMBER OF BUFFERS IN A PAGE AND A NUMBER OF 
 *   OTHER CONSTANTS WHICH ARE CHANGED WHEN THE BUFFER SIZE IS CHANGED. 
 * ----------------------------------------------------------------------- */
#define ZHEAD_SIZE 32
#define ZCON_HEAD_SIZE 2
#define ZBUF_SIZE 28
#define ZEMPTYLIST 72
#define ZUP_LIMIT 14
#define ZDOWN_LIMIT 12
#define ZSHIFT_PLUS 5
#define ZSHIFT_MINUS 2
#define ZFREE_LIMIT 65
#define ZNO_CONTAINERS 64
#define ZELEM_HEAD_SIZE 1
/* ------------------------------------------------------------------------- */
/*  THESE CONSTANTS DEFINE THE USE OF THE PAGE HEADER IN THE INDEX PAGES.    */
/* ------------------------------------------------------------------------- */
#define ZPOS_PAGE_ID 0
#define ZPOS_PAGE_TYPE 1
#define ZPOS_PAGE_TYPE_BIT 14
#define ZPOS_EMPTY_LIST 1
#define ZPOS_ALLOC_CONTAINERS 2
#define ZPOS_CHECKSUM 3
#define ZPOS_OVERFLOWREC 4
#define ZPOS_NO_ELEM_IN_PAGE 2
#define ZPOS_FREE_AREA_IN_PAGE 5
#define ZPOS_LAST_INDEX 6
#define ZPOS_INSERT_INDEX 7
#define ZPOS_ARRAY_POS 8
#define ZPOS_NEXT_FREE_INDEX 9
#define ZPOS_NEXT_PAGE 10
#define ZPOS_PREV_PAGE 11
#define ZNORMAL_PAGE_TYPE 0
#define ZOVERFLOW_PAGE_TYPE 1
#define ZDEFAULT_LIST 3
#define ZWORDS_IN_PAGE 2048
/* --------------------------------------------------------------------------------- */
/*       CONSTANTS FOR THE ZERO PAGES                                                */
/* --------------------------------------------------------------------------------- */
#define ZPAGEZERO_PREV_UNDOP 8
#define ZPAGEZERO_NO_OVER_PAGE 9
#define ZPAGEZERO_TABID 10
#define ZPAGEZERO_FRAGID0 11
#define ZPAGEZERO_FRAGID1 12
#define ZPAGEZERO_HASH_CHECK 13
#define ZPAGEZERO_DIRSIZE 14
#define ZPAGEZERO_EXPCOUNTER 15
#define ZPAGEZERO_NEXT_UNDO_FILE 16
#define ZPAGEZERO_SLACK 17
#define ZPAGEZERO_NO_PAGES 18
#define ZPAGEZERO_HASHCHECKBIT 19
#define ZPAGEZERO_K 20
#define ZPAGEZERO_LHFRAGBITS 21
#define ZPAGEZERO_LHDIRBITS 22
#define ZPAGEZERO_LOCALKEYLEN 23
#define ZPAGEZERO_MAXP 24
#define ZPAGEZERO_MAXLOADFACTOR 25
#define ZPAGEZERO_MINLOADFACTOR 26
#define ZPAGEZERO_MYFID 27
#define ZPAGEZERO_LAST_OVER_INDEX 28
#define ZPAGEZERO_P 29
#define ZPAGEZERO_NO_OF_ELEMENTS 30
#define ZPAGEZERO_ELEMENT_LENGTH 31
#define ZPAGEZERO_KEY_LENGTH 32
#define ZPAGEZERO_NODETYPE 33
#define ZPAGEZERO_SLACK_CHECK 34
/* --------------------------------------------------------------------------------- */
/*       CONSTANTS IN ALPHABETICAL ORDER                                             */
/* --------------------------------------------------------------------------------- */
#define ZADDFRAG 0
#define ZCOPY_NEXT 1
#define ZCOPY_NEXT_COMMIT 2
#define ZCOPY_COMMIT 3
#define ZCOPY_REPEAT 4
#define ZCOPY_ABORT 5
#define ZCOPY_CLOSE 6
#define ZDIRARRAY 68
#define ZDIRRANGESIZE 65
//#define ZEMPTY_FRAGMENT 0
#define ZFRAGMENTSIZE 64
#define ZFIRSTTIME 1
#define ZFS_CONNECTSIZE 300
#define ZFS_OPSIZE 100
#define ZKEYINKEYREQ 4
#define ZLCP_CONNECTSIZE 30
#define ZLEFT 1
#define ZLOCALLOGFILE 2
#define ZLOCKED 0
#define ZMAXSCANSIGNALLEN 20
#define ZMAINKEYLEN 8
#define ZMAX_UNDO_VERSION 4
#define ZNO_OF_DISK_VERSION 3
#define ZNO_OF_OP_PER_SIGNAL 20
//#define ZNOT_EMPTY_FRAGMENT 1
#define ZNR_OF_UNDO_PAGE_GROUP 16
#define ZOP_HEAD_INFO_LN 3
#define ZOPRECSIZE 740
#define ZOVERFLOWRECSIZE 5
#define ZPAGE8_BASE_ADD 1
#define ZPAGESIZE 128
#define ZPARALLEL_QUEUE 1
#define ZPDIRECTORY 1
#define ZSCAN_MAX_LOCK 4
#define ZSERIAL_QUEUE 2
#define ZSPH1 1
#define ZSPH2 2
#define ZSPH3 3
#define ZSPH6 6
#define ZREADLOCK 0
#define ZRIGHT 2
#define ZROOTFRAGMENTSIZE 32
#define ZSCAN_LOCK_ALL 3
#define ZSCAN_OP 5
#define ZSCAN_REC_SIZE 256
#define ZSR_VERSION_REC_SIZE 16
#define ZSTAND_BY 2
#define ZTABLESIZE 16
#define ZTABMAXINDEX 3
#define ZUNDEFINED_OP 6
#define ZUNDOHEADSIZE 7
#define ZUNLOCKED 1
#define ZUNDOPAGE_BASE_ADD 2
#define ZUNDOPAGEINDEXBITS 13
#define ZUNDOPAGEINDEX_MASK 0x1fff
#define ZWRITEPAGESIZE 8
#define ZWRITE_UNDOPAGESIZE 2
#define ZMIN_UNDO_PAGES_AT_COMMIT 4
#define ZMIN_UNDO_PAGES_AT_OPERATION 10
#define ZMIN_UNDO_PAGES_AT_EXPAND 16

/* --------------------------------------------------------------------------------- */
/* CONTINUEB CODES                                                                   */
/* --------------------------------------------------------------------------------- */
#define ZLOAD_BAL_LCP_TIMER 0
#define ZINITIALISE_RECORDS 1
#define ZSR_READ_PAGES_ALLOC 2
#define ZSTART_UNDO 3
#define ZSEND_SCAN_HBREP 4
#define ZREL_ROOT_FRAG 5
#define ZREL_FRAG 6
#define ZREL_DIR 7
#define ZREPORT_MEMORY_USAGE 8
#define ZLCP_OP_WRITE_RT_BREAK 9

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
#define ZTEMPORARY_ACC_UNDO_FAILURE 677
#endif

class ElementHeader {
  /**
   * 
   * l = Locked    -- If true contains operation else scan bits + hash value
   * s = Scan bits
   * h = Hash value
   * o = Operation ptr I
   *
   *           1111111111222222222233
   * 01234567890123456789012345678901
   * lssssssssssss   hhhhhhhhhhhhhhhh
   *  ooooooooooooooooooooooooooooooo
   */
public:
  STATIC_CONST( HASH_VALUE_PART_MASK = 0xFFFF );
  
  static bool getLocked(Uint32 data);
  static bool getUnlocked(Uint32 data);
  static Uint32 getScanBits(Uint32 data);
  static Uint32 getHashValuePart(Uint32 data);
  static Uint32 getOpPtrI(Uint32 data);

  static Uint32 setLocked(Uint32 opPtrI);
  static Uint32 setUnlocked(Uint32 hashValuePart, Uint32 scanBits);
  static Uint32 setScanBit(Uint32 header, Uint32 scanBit);
  static Uint32 clearScanBit(Uint32 header, Uint32 scanBit);
};

inline 
bool
ElementHeader::getLocked(Uint32 data){
  return (data & 1) == 0;
}

inline 
bool
ElementHeader::getUnlocked(Uint32 data){
  return (data & 1) == 1;
}

inline 
Uint32 
ElementHeader::getScanBits(Uint32 data){
  assert(getUnlocked(data));
  return (data >> 1) & ((1 << MAX_PARALLEL_SCANS_PER_FRAG) - 1);
}

inline 
Uint32 
ElementHeader::getHashValuePart(Uint32 data){
  assert(getUnlocked(data));
  return data >> 16;
}

inline
Uint32 
ElementHeader::getOpPtrI(Uint32 data){
  assert(getLocked(data));
  return data >> 1;
}

inline 
Uint32 
ElementHeader::setLocked(Uint32 opPtrI){
  return (opPtrI << 1) + 0;
}
inline
Uint32 
ElementHeader::setUnlocked(Uint32 hashValue, Uint32 scanBits){
  return (hashValue << 16) + (scanBits << 1) + 1;
}

inline
Uint32 
ElementHeader::setScanBit(Uint32 header, Uint32 scanBit){
  assert(getUnlocked(header));
  return header | (scanBit << 1);
}

inline
Uint32 
ElementHeader::clearScanBit(Uint32 header, Uint32 scanBit){
  assert(getUnlocked(header));
  return header & (~(scanBit << 1));
}


class Dbacc: public SimulatedBlock {
public:
// State values
enum State {
  FREEFRAG = 0,
  ACTIVEFRAG = 1,
  SEND_QUE_OP = 2,
  WAIT_ACC_LCPREQ = 3,
  LCP_SEND_PAGES = 4,
  LCP_SEND_OVER_PAGES = 5,
  LCP_SEND_ZERO_PAGE = 6,
  SR_READ_PAGES = 7,
  SR_READ_OVER_PAGES = 8,
  WAIT_ZERO_PAGE_STORED = 9,
  WAIT_NOTHING = 10,
  WAIT_OPEN_UNDO_LCP = 11,
  WAIT_OPEN_UNDO_LCP_NEXT = 12,
  WAIT_OPEN_DATA_FILE_FOR_READ = 13,
  WAIT_OPEN_DATA_FILE_FOR_WRITE = 14,
  OPEN_UNDO_FILE_SR = 15,
  READ_UNDO_PAGE = 16,
  READ_UNDO_PAGE_AND_CLOSE = 17,
  WAIT_READ_DATA = 18,
  WAIT_READ_PAGE_ZERO = 19,
  WAIT_WRITE_DATA = 20,
  WAIT_WRITE_UNDO = 21,
  WAIT_WRITE_UNDO_EXIT = 22,
  WAIT_CLOSE_UNDO = 23,
  LCP_CLOSE_DATA = 24,
  SR_CLOSE_DATA = 25,
  WAIT_ONE_CONF = 26,
  WAIT_TWO_CONF = 27,
  LCP_FREE = 28,
  LCP_ACTIVE = 29,
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
  ACTIVEROOT = 52,
  LCP_CREATION = 53
};

// Records

/* --------------------------------------------------------------------------------- */
/* UNDO HEADER RECORD                                                                */
/* --------------------------------------------------------------------------------- */

  struct UndoHeader {
    enum UndoHeaderType{
      ZPAGE_INFO = 0,
      ZOVER_PAGE_INFO = 1,
      ZOP_INFO = 2,
      ZNO_UNDORECORD_TYPES = 3
    };
    UintR tableId;
    UintR rootFragId;
    UintR localFragId;
    UintR variousInfo;
    UintR logicalPageId;
    UintR prevUndoAddressForThisFrag;
    UintR prevUndoAddress;
  };

/* --------------------------------------------------------------------------------- */
/* DIRECTORY RANGE                                                                   */
/* --------------------------------------------------------------------------------- */
  struct DirRange {
    Uint32 dirArray[256];
  }; /* p2c: size = 1024 bytes */
  
  typedef Ptr<DirRange> DirRangePtr;

/* --------------------------------------------------------------------------------- */
/* DIRECTORYARRAY                                                                    */
/* --------------------------------------------------------------------------------- */
struct Directoryarray {
  Uint32 pagep[256];
}; /* p2c: size = 1024 bytes */

  typedef Ptr<Directoryarray> DirectoryarrayPtr;

/* --------------------------------------------------------------------------------- */
/* FRAGMENTREC. ALL INFORMATION ABOUT FRAMENT AND HASH TABLE IS SAVED IN FRAGMENT    */
/*         REC  A POINTER TO FRAGMENT RECORD IS SAVED IN ROOTFRAGMENTREC FRAGMENT    */
/* --------------------------------------------------------------------------------- */
struct Fragmentrec {
//-----------------------------------------------------------------------------
// References to long key pages with free area. Some type of buddy structure
// where references in higher index have more free space.
//-----------------------------------------------------------------------------
  Uint32 longKeyPageArray[4];

//-----------------------------------------------------------------------------
// These variables keep track of allocated pages, the number of them and the
// start file page of them. Used during local checkpoints.
//-----------------------------------------------------------------------------
  Uint32 datapages[8];
  Uint32 activeDataPage;
  Uint32 activeDataFilePage;

//-----------------------------------------------------------------------------
// Temporary variables used during shrink and expand process.
//-----------------------------------------------------------------------------
  Uint32 expReceivePageptr;
  Uint32 expReceiveIndex;
  Uint32 expReceiveForward;
  Uint32 expSenderDirIndex;
  Uint32 expSenderDirptr;
  Uint32 expSenderIndex;
  Uint32 expSenderPageptr;

//-----------------------------------------------------------------------------
// List of lock owners and list of lock waiters to support LCP handling
//-----------------------------------------------------------------------------
  Uint32 lockOwnersList;
  Uint32 firstWaitInQueOp;
  Uint32 lastWaitInQueOp;
  Uint32 sentWaitInQueOp;

//-----------------------------------------------------------------------------
// References to Directory Ranges (which in turn references directories, which
// in its turn references the pages) for the bucket pages and the overflow
// bucket pages.
//-----------------------------------------------------------------------------
  Uint32 directory;
  Uint32 dirsize;
  Uint32 overflowdir;
  Uint32 lastOverIndex;

//-----------------------------------------------------------------------------
// These variables are used to support LCP and Restore from disk.
// lcpDirIndex: used during LCP as the frag page id currently stored.
// lcpMaxDirIndex: The dirsize at start of LCP.
// lcpMaxOverDirIndex: The xx at start of LCP
// During a LCP one writes the minimum of the number of pages in the directory
// and the number of pages at the start of the LCP.
// noStoredPages: Number of bucket pages written in LCP used at restore
// noOfOverStoredPages: Number of overflow pages written in LCP used at restore
// This variable is also used during LCP to calculate this number.
//-----------------------------------------------------------------------------
  Uint32 lcpDirIndex;
  Uint32 lcpMaxDirIndex;
  Uint32 lcpMaxOverDirIndex;
  Uint32 noStoredPages;
  Uint32 noOfStoredOverPages;

//-----------------------------------------------------------------------------
// We have a list of overflow pages with free areas. We have a special record,
// the overflow record representing these pages. The reason is that the
// same record is also used to represent pages in the directory array that have
// been released since they were empty (there were however higher indexes with
// data in them). These are put in the firstFreeDirIndexRec-list.
// An overflow record representing a page can only be in one of these lists.
//-----------------------------------------------------------------------------
  Uint32 firstOverflowRec;
  Uint32 lastOverflowRec;
  Uint32 firstFreeDirindexRec;

//-----------------------------------------------------------------------------
// localCheckpId is used during execution of UNDO log to ensure that we only
// apply UNDO log records from the restored LCP of the fragment.
// lcpLqhPtr keeps track of LQH record for this fragment to checkpoint
//-----------------------------------------------------------------------------
  Uint32 localCheckpId;
  Uint32 lcpLqhPtr;

//-----------------------------------------------------------------------------
// Counter keeping track of how many times we have expanded. We need to ensure
// that we do not shrink so many times that this variable becomes negative.
//-----------------------------------------------------------------------------
  Uint32 expandCounter;
//-----------------------------------------------------------------------------
// Reference to record for open file at LCP and restore
//-----------------------------------------------------------------------------
  Uint32 fsConnPtr;

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
//-----------------------------------------------------------------------------
  Uint32 localkeylen;
  Uint32 maxp;
  Uint32 maxloadfactor;
  Uint32 minloadfactor;
  Uint32 p;
  Uint32 slack;
  Uint32 slackCheck;

//-----------------------------------------------------------------------------
// myfid is the fragment id of the fragment
// myroot is the reference to the root fragment record
// nextfreefrag is the next free fragment if linked into a free list
//-----------------------------------------------------------------------------
  Uint32 myfid;
  Uint32 myroot;
  Uint32 myTableId;
  Uint32 nextfreefrag;

//-----------------------------------------------------------------------------
// This variable is used during restore to keep track of page id of read pages.
// During read of bucket pages this is used to calculate the page id and also
// to verify that the page id of the read page is correct. During read of over-
// flow pages it is only used to keep track of the number of pages read.
//-----------------------------------------------------------------------------
  Uint32 nextAllocPage;

//-----------------------------------------------------------------------------
// Keeps track of undo position for fragment during LCP and restore.
//-----------------------------------------------------------------------------
  Uint32 prevUndoposition;

//-----------------------------------------------------------------------------
// Page reference during LCP and restore of page zero where fragment data is
// saved
//-----------------------------------------------------------------------------
  Uint32 zeroPagePtr;

//-----------------------------------------------------------------------------
// Number of pages read from file during restore
//-----------------------------------------------------------------------------
  Uint32 noOfExpectedPages;

//-----------------------------------------------------------------------------
// Fragment State, mostly applicable during LCP and restore
//-----------------------------------------------------------------------------
  State fragState;

//-----------------------------------------------------------------------------
// Keep track of number of outstanding writes of UNDO log records to ensure that
// we have saved all UNDO info before concluding local checkpoint.
//-----------------------------------------------------------------------------
  Uint32 nrWaitWriteUndoExit;

//-----------------------------------------------------------------------------
// lastUndoIsStored is used to handle parallel writes of UNDO log and pages to
// know when LCP is completed
//-----------------------------------------------------------------------------
  Uint8 lastUndoIsStored;

//-----------------------------------------------------------------------------
// Set to ZTRUE when local checkpoint freeze occurs and set to ZFALSE when
// local checkpoint concludes.
//-----------------------------------------------------------------------------
  Uint8 createLcp;

//-----------------------------------------------------------------------------
// Flag indicating whether we are in the load phase of restore still.
//-----------------------------------------------------------------------------
  Uint8 loadingFlag;

//-----------------------------------------------------------------------------
// elementLength: Length of element in bucket and overflow pages
// keyLength: Length of key
//-----------------------------------------------------------------------------
  Uint8 elementLength;
  Uint16 keyLength;

//-----------------------------------------------------------------------------
// This flag is used to avoid sending a big number of expand or shrink signals
// when simultaneously committing many inserts or deletes.
//-----------------------------------------------------------------------------
  Uint8 expandFlag;

//-----------------------------------------------------------------------------
// hashcheckbit is the bit to check whether to send element to split bucket or not
// k (== 6) is the number of buckets per page
// lhfragbits is the number of bits used to calculate the fragment id
// lhdirbits is the number of bits used to calculate the page id
//-----------------------------------------------------------------------------
  Uint8 hashcheckbit;
  Uint8 k;
  Uint8 lhfragbits;
  Uint8 lhdirbits;

//-----------------------------------------------------------------------------
// nodetype can only be STORED in this release. Is currently only set, never read
// stopQueOp is indicator that locked operations will not start until LCP have
// released the lock on the fragment
//-----------------------------------------------------------------------------
  Uint8 nodetype;
  Uint8 stopQueOp;

//-----------------------------------------------------------------------------
// flag to avoid accessing table record if no char attributes
//-----------------------------------------------------------------------------
  Uint8 hasCharAttr;
};

  typedef Ptr<Fragmentrec> FragmentrecPtr;

/* --------------------------------------------------------------------------------- */
/* FS_CONNECTREC                                                                     */
/* --------------------------------------------------------------------------------- */
struct FsConnectrec {
  Uint32 fsNext;
  Uint32 fsPrev;
  Uint32 fragrecPtr;
  Uint32 fsPtr;
  State fsState;
  Uint8 activeFragId;
  Uint8 fsPart;
}; /* p2c: size = 24 bytes */

  typedef Ptr<FsConnectrec> FsConnectrecPtr;

/* --------------------------------------------------------------------------------- */
/* FS_OPREC                                                                          */
/* --------------------------------------------------------------------------------- */
struct FsOprec {
  Uint32 fsOpnext;
  Uint32 fsOpfragrecPtr;
  Uint32 fsConptr;
  State fsOpstate;
  Uint16 fsOpMemPage;
}; /* p2c: size = 20 bytes */

  typedef Ptr<FsOprec> FsOprecPtr;

/* --------------------------------------------------------------------------------- */
/* LCP_CONNECTREC                                                                    */
/* --------------------------------------------------------------------------------- */
struct LcpConnectrec {
  Uint32 nextLcpConn;
  Uint32 lcpUserptr;
  Uint32 rootrecptr;
  State syncUndopageState;
  State lcpstate;
  Uint32 lcpUserblockref;
  Uint16 localCheckPid;
  Uint8 noOfLcpConf;
};
  typedef Ptr<LcpConnectrec> LcpConnectrecPtr;

/* --------------------------------------------------------------------------------- */
/* OPERATIONREC                                                                      */
/* --------------------------------------------------------------------------------- */
struct Operationrec {
  Uint32 keydata[8];
  Uint32 localdata[2];
  Uint32 elementIsforward;
  Uint32 elementPage;
  Uint32 elementPointer;
  Uint32 fid;
  Uint32 fragptr;
  Uint32 hashvaluePart;
  Uint32 hashValue;
  Uint32 insertDeleteLen;
  Uint32 keyinfoPage;
  Uint32 nextLockOwnerOp;
  Uint32 nextOp;
  Uint32 nextParallelQue;
  Uint32 nextQueOp;
  Uint32 nextSerialQue;
  Uint32 prevOp;
  Uint32 prevLockOwnerOp;
  Uint32 prevParallelQue;
  Uint32 prevQueOp;
  Uint32 prevSerialQue;
  Uint32 scanRecPtr;
  Uint32 transId1;
  Uint32 transId2;
  Uint32 longPagePtr;
  Uint32 longKeyPageIndex;
  State opState;
  Uint32 userptr;
  State transactionstate;
  Uint16 elementContainer;
  Uint16 tupkeylen;
  Uint32 xfrmtupkeylen;
  Uint32 userblockref;
  Uint32 scanBits;
  Uint8 elementIsDisappeared;
  Uint8 insertIsDone;
  Uint8 lockMode;
  Uint8 lockOwner;
  Uint8 nodeType;
  Uint8 operation;
  Uint8 opSimple;
  Uint8 dirtyRead;
  Uint8 commitDeleteCheckFlag;
  Uint8 isAccLockReq;
  Uint8 isUndoLogReq;
}; /* p2c: size = 168 bytes */

  typedef Ptr<Operationrec> OperationrecPtr;

/* --------------------------------------------------------------------------------- */
/* OVERFLOW_RECORD                                                                   */
/* --------------------------------------------------------------------------------- */
struct OverflowRecord {
  Uint32 dirindex;
  Uint32 nextOverRec;
  Uint32 nextOverList;
  Uint32 prevOverRec;
  Uint32 prevOverList;
  Uint32 overpage;
  Uint32 nextfreeoverrec;
};

  typedef Ptr<OverflowRecord> OverflowRecordPtr;

/* --------------------------------------------------------------------------------- */
/* PAGE8                                                                             */
/* --------------------------------------------------------------------------------- */
struct Page8 {
  Uint32 word32[2048];
}; /* p2c: size = 8192 bytes */

  typedef Ptr<Page8> Page8Ptr;

/* --------------------------------------------------------------------------------- */
/* ROOTFRAGMENTREC                                                                   */
/*          DURING EXPAND FRAGMENT PROCESS, EACH FRAGMEND WILL BE EXPAND INTO TWO    */
/*          NEW FRAGMENTS.TO MAKE THIS PROCESS EASIER, DURING ADD FRAGMENT PROCESS   */
/*          NEXT FRAGMENT IDENTIIES WILL BE CALCULATED, AND TWO FRAGMENTS WILL BE    */
/*          ADDED IN (NDBACC). THEREBY EXPAND OF FRAGMENT CAN BE PERFORMED QUICK AND */
/*          EASY.THE NEW FRAGMENT ID SENDS TO TUP MANAGER FOR ALL OPERATION PROCESS. */
/* --------------------------------------------------------------------------------- */
struct Rootfragmentrec {
  Uint32 scan[MAX_PARALLEL_SCANS_PER_FRAG];
  Uint32 fragmentptr[2];
  Uint32 fragmentid[2];
  Uint32 lcpPtr;
  Uint32 mytabptr;
  Uint32 nextroot;
  Uint32 roothashcheck;
  Uint32 noOfElements;
  Uint32 m_commit_count;
  State rootState;
}; /* p2c: size = 72 bytes */

  typedef Ptr<Rootfragmentrec> RootfragmentrecPtr;

/* --------------------------------------------------------------------------------- */
/* SCAN_REC                                                                          */
/* --------------------------------------------------------------------------------- */
struct ScanRec {
  enum ScanState {
    WAIT_NEXT,  
    SCAN_DISCONNECT
  };
  enum ScanBucketState {
    FIRST_LAP,
    SECOND_LAP,
    SCAN_COMPLETED
  };
  Uint32 activeLocalFrag;
  Uint32 rootPtr;
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
  ScanBucketState scanBucketState;
  ScanState scanState;
  Uint16 scanLockHeld;
  Uint32 scanUserblockref;
  Uint32 scanMask;
  Uint8 scanLockMode;
  Uint8 scanKeyinfoFlag;
  Uint8 scanTimer;
  Uint8 scanContinuebCounter;
  Uint8 scanReadCommittedFlag;
}; 

  typedef Ptr<ScanRec> ScanRecPtr;

/* --------------------------------------------------------------------------------- */
/* SR_VERSION_REC                                                                    */
/* --------------------------------------------------------------------------------- */
struct SrVersionRec {
  Uint32 nextFreeSr;
  Uint32 checkPointId;
  Uint32 prevAddress;
  Uint32 srUnused;	/* p2c: Not used */
}; /* p2c: size = 16 bytes */

  typedef Ptr<SrVersionRec> SrVersionRecPtr;

/* --------------------------------------------------------------------------------- */
/* TABREC                                                                            */
/* --------------------------------------------------------------------------------- */
struct Tabrec {
  Uint32 fragholder[MAX_FRAG_PER_NODE];
  Uint32 fragptrholder[MAX_FRAG_PER_NODE];
  Uint32 tabUserPtr;
  BlockReference tabUserRef;
};
  typedef Ptr<Tabrec> TabrecPtr;

/* --------------------------------------------------------------------------------- */
/* UNDOPAGE                                                                          */
/* --------------------------------------------------------------------------------- */
struct Undopage {
  Uint32 undoword[8192];
}; /* p2c: size = 32768 bytes */

  typedef Ptr<Undopage> UndopagePtr;

public:
  Dbacc(const class Configuration &);
  virtual ~Dbacc();

  // pointer to TUP instance in this thread
  Dbtup* c_tup;

private:
  BLOCK_DEFINES(Dbacc);

  // Transit signals
  void execDEBUG_SIG(Signal* signal);
  void execCONTINUEB(Signal* signal);
  void execACC_CHECK_SCAN(Signal* signal);
  void execEXPANDCHECK2(Signal* signal);
  void execSHRINKCHECK2(Signal* signal);
  void execACC_OVER_REC(Signal* signal);
  void execACC_SAVE_PAGES(Signal* signal);
  void execNEXTOPERATION(Signal* signal);
  void execREAD_PSUEDO_REQ(Signal* signal);

  // Received signals
  void execSTTOR(Signal* signal);
  void execSR_FRAGIDREQ(Signal* signal);
  void execLCP_FRAGIDREQ(Signal* signal);
  void execLCP_HOLDOPREQ(Signal* signal);
  void execEND_LCPREQ(Signal* signal);
  void execACC_LCPREQ(Signal* signal);
  void execSTART_RECREQ(Signal* signal);
  void execACC_CONTOPREQ(Signal* signal);
  void execACCKEYREQ(Signal* signal);
  void execACCSEIZEREQ(Signal* signal);
  void execACCFRAGREQ(Signal* signal);
  void execACC_SRREQ(Signal* signal);
  void execNEXT_SCANREQ(Signal* signal);
  void execACC_ABORTREQ(Signal* signal);
  void execACC_SCANREQ(Signal* signal);
  void execACCMINUPDATE(Signal* signal);
  void execACC_COMMITREQ(Signal* signal);
  void execACC_TO_REQ(Signal* signal);
  void execACC_LOCKREQ(Signal* signal);
  void execFSOPENCONF(Signal* signal);
  void execFSCLOSECONF(Signal* signal);
  void execFSWRITECONF(Signal* signal);
  void execFSREADCONF(Signal* signal);
  void execNDB_STTOR(Signal* signal);
  void execDROP_TAB_REQ(Signal* signal);
  void execFSREMOVECONF(Signal* signal);
  void execREAD_CONFIG_REQ(Signal* signal);
  void execSET_VAR_REQ(Signal* signal);
  void execDUMP_STATE_ORD(Signal* signal);

  // Statement blocks
  void ACCKEY_error(Uint32 fromWhere);

  void commitDeleteCheck();

  void initRootFragPageZero(RootfragmentrecPtr, Page8Ptr);
  void initRootFragSr(RootfragmentrecPtr, Page8Ptr);
  void initFragAdd(Signal*, Uint32 rootFragIndex, Uint32 rootIndex, FragmentrecPtr);
  void initFragPageZero(FragmentrecPtr, Page8Ptr);
  void initFragSr(FragmentrecPtr, Page8Ptr);
  void initFragGeneral(FragmentrecPtr);
  void verifyFragCorrect(FragmentrecPtr regFragPtr);
  void sendFSREMOVEREQ(Signal* signal, Uint32 tableId);
  void releaseFragResources(Signal* signal, Uint32 fragIndex);
  void releaseRootFragRecord(Signal* signal, RootfragmentrecPtr rootPtr);
  void releaseRootFragResources(Signal* signal, Uint32 tableId);
  void releaseDirResources(Signal* signal,
                           Uint32 fragIndex,
                           Uint32 dirIndex,
                           Uint32 startIndex);
  void releaseDirectoryResources(Signal* signal,
                                 Uint32 fragIndex,
                                 Uint32 dirIndex,
                                 Uint32 startIndex,
                                 Uint32 directoryIndex);
  void releaseOverflowResources(Signal* signal, FragmentrecPtr regFragPtr);
  void releaseDirIndexResources(Signal* signal, FragmentrecPtr regFragPtr);
  void releaseFragRecord(Signal* signal, FragmentrecPtr regFragPtr);
  Uint32 remainingUndoPages();
  void updateLastUndoPageIdWritten(Signal* signal, Uint32 aNewValue);
  void updateUndoPositionPage(Signal* signal, Uint32 aNewValue);
  void srCheckPage(Signal* signal);
  void srCheckContainer(Signal* signal);
  void initScanFragmentPart(Signal* signal);
  Uint32 checkScanExpand(Signal* signal);
  Uint32 checkScanShrink(Signal* signal);
  void initialiseDirRec(Signal* signal);
  void initialiseDirRangeRec(Signal* signal);
  void initialiseFragRec(Signal* signal);
  void initialiseFsConnectionRec(Signal* signal);
  void initialiseFsOpRec(Signal* signal);
  void initialiseLcpConnectionRec(Signal* signal);
  void initialiseOperationRec(Signal* signal);
  void initialiseOverflowRec(Signal* signal);
  void initialisePageRec(Signal* signal);
  void initialiseLcpPages(Signal* signal);
  void initialiseRootfragRec(Signal* signal);
  void initialiseScanRec(Signal* signal);
  void initialiseSrVerRec(Signal* signal);
  void initialiseTableRec(Signal* signal);
  bool addfragtotab(Signal* signal, Uint32 rootIndex, Uint32 fragId);
  void initOpRec(Signal* signal);
  void sendAcckeyconf(Signal* signal);
  Uint32 placeReadInLockQueue(Signal* signal);
  void placeSerialQueueRead(Signal* signal);
  void checkOnlyReadEntry(Signal* signal);
  Uint32 getNoParallelTransaction(const Operationrec*);
  void moveLastParallelQueue(Signal* signal);
  void moveLastParallelQueueWrite(Signal* signal);
  Uint32 placeWriteInLockQueue(Signal* signal);
  void placeSerialQueueWrite(Signal* signal);
  void expandcontainer(Signal* signal);
  void shrinkcontainer(Signal* signal);
  void nextcontainerinfoExp(Signal* signal);
  void lcpCopyPage(Signal* signal);
  void lcpUpdatePage(Signal* signal);
  void checkUndoPages(Signal* signal);
  void undoWritingProcess(Signal* signal);
  void writeUndoDataInfo(Signal* signal);
  void writeUndoHeader(Signal* signal, 
                       Uint32 logicalPageId, 
                       UndoHeader::UndoHeaderType pageType);
  void writeUndoOpInfo(Signal* signal);
  void checksumControl(Signal* signal, Uint32 checkPage);
  void startActiveUndo(Signal* signal);
  void releaseAndCommitActiveOps(Signal* signal);
  void releaseAndCommitQueuedOps(Signal* signal);
  void releaseAndAbortLockedOps(Signal* signal);
  void containerinfo(Signal* signal);
  bool getScanElement(Signal* signal);
  void initScanOpRec(Signal* signal);
  void nextcontainerinfo(Signal* signal);
  void putActiveScanOp(Signal* signal);
  void putOpScanLockQue();
  void putReadyScanQueue(Signal* signal, Uint32 scanRecIndex);
  void releaseScanBucket(Signal* signal);
  void releaseScanContainer(Signal* signal);
  void releaseScanRec(Signal* signal);
  bool searchScanContainer(Signal* signal);
  void sendNextScanConf(Signal* signal);
  void setlock(Signal* signal);
  void takeOutActiveScanOp(Signal* signal);
  void takeOutScanLockQueue(Uint32 scanRecIndex);
  void takeOutReadyScanQueue(Signal* signal);
  void insertElement(Signal* signal);
  void insertContainer(Signal* signal);
  void addnewcontainer(Signal* signal);
  void getfreelist(Signal* signal);
  void increaselistcont(Signal* signal);
  void seizeLeftlist(Signal* signal);
  void seizeRightlist(Signal* signal);
  Uint32 readTablePk(Uint32 localkey1);
  void getElement(Signal* signal);
  void getdirindex(Signal* signal);
  void commitdelete(Signal* signal, bool systemRestart);
  void deleteElement(Signal* signal);
  void getLastAndRemove(Signal* signal);
  void releaseLeftlist(Signal* signal);
  void releaseRightlist(Signal* signal);
  void checkoverfreelist(Signal* signal);
  void abortOperation(Signal* signal);
  void accAbortReqLab(Signal* signal, bool sendConf);
  void commitOperation(Signal* signal);
  void copyOpInfo(Signal* signal);
  Uint32 executeNextOperation(Signal* signal);
  void releaselock(Signal* signal);
  void takeOutFragWaitQue(Signal* signal);
  void check_lock_upgrade(Signal* signal, OperationrecPtr lock_owner,
			  OperationrecPtr release_op);
  void allocOverflowPage(Signal* signal);
  bool getrootfragmentrec(Signal* signal, RootfragmentrecPtr&, Uint32 fragId);
  void insertLockOwnersList(Signal* signal, const OperationrecPtr&);
  void takeOutLockOwnersList(Signal* signal, const OperationrecPtr&);
  void initFsOpRec(Signal* signal);
  void initLcpConnRec(Signal* signal);
  void initOverpage(Signal* signal);
  void initPage(Signal* signal);
  void initRootfragrec(Signal* signal);
  void putOpInFragWaitQue(Signal* signal);
  void putOverflowRecInFrag(Signal* signal);
  void putRecInFreeOverdir(Signal* signal);
  void releaseDirectory(Signal* signal);
  void releaseDirrange(Signal* signal);
  void releaseFsConnRec(Signal* signal);
  void releaseFsOpRec(Signal* signal);
  void releaseLcpConnectRec(Signal* signal);
  void releaseOpRec(Signal* signal);
  void releaseOverflowRec(Signal* signal);
  void releaseOverpage(Signal* signal);
  void releasePage(Signal* signal);
  void releaseLcpPage(Signal* signal);
  void releaseSrRec(Signal* signal);
  void releaseLogicalPage(Fragmentrec * fragP, Uint32 logicalPageId);
  void seizeDirectory(Signal* signal);
  void seizeDirrange(Signal* signal);
  void seizeFragrec(Signal* signal);
  void seizeFsConnectRec(Signal* signal);
  void seizeFsOpRec(Signal* signal);
  void seizeLcpConnectRec(Signal* signal);
  void seizeOpRec(Signal* signal);
  void seizeOverRec(Signal* signal);
  void seizePage(Signal* signal);
  void seizeLcpPage(Page8Ptr&);
  void seizeRootfragrec(Signal* signal);
  void seizeScanRec(Signal* signal);
  void seizeSrVerRec(Signal* signal);
  void sendSystemerror(Signal* signal, int line);
  void takeRecOutOfFreeOverdir(Signal* signal);
  void takeRecOutOfFreeOverpage(Signal* signal);
  void sendScanHbRep(Signal* signal, Uint32);

  void addFragRefuse(Signal* signal, Uint32 errorCode);
  void ndbsttorryLab(Signal* signal);
  void srCloseDataFileLab(Signal* signal);
  void acckeyref1Lab(Signal* signal, Uint32 result_code);
  void insertelementLab(Signal* signal);
  void startUndoLab(Signal* signal);
  void checkNextFragmentLab(Signal* signal);
  void endofexpLab(Signal* signal);
  void endofshrinkbucketLab(Signal* signal);
  void srStartUndoLab(Signal* signal);
  void senddatapagesLab(Signal* signal);
  void undoNext2Lab(Signal* signal);
  void sttorrysignalLab(Signal* signal);
  void sendholdconfsignalLab(Signal* signal);
  void accIsLockedLab(Signal* signal);
  void insertExistElemLab(Signal* signal);
  void refaccConnectLab(Signal* signal);
  void srReadOverPagesLab(Signal* signal);
  void releaseScanLab(Signal* signal);
  void lcpOpenUndofileConfLab(Signal* signal);
  void srFsOpenConfLab(Signal* signal);
  void checkSyncUndoPagesLab(Signal* signal);
  void sendaccSrconfLab(Signal* signal);
  void checkSendLcpConfLab(Signal* signal);
  void endsaveoverpageLab(Signal* signal);
  void lcpCloseDataFileLab(Signal* signal);
  void srOpenDataFileLoopLab(Signal* signal);
  void srReadPagesLab(Signal* signal);
  void srDoUndoLab(Signal* signal);
  void ndbrestart1Lab(Signal* signal);
  void initialiseRecordsLab(Signal* signal, Uint32 ref, Uint32 data);
  void srReadPagesAllocLab(Signal* signal);
  void checkNextBucketLab(Signal* signal);
  void endsavepageLab(Signal* signal);
  void saveZeroPageLab(Signal* signal);
  void srAllocPage0011Lab(Signal* signal);
  void sendLcpFragidconfLab(Signal* signal);
  void savepagesLab(Signal* signal);
  void saveOverPagesLab(Signal* signal);
  void srReadPageZeroLab(Signal* signal);
  void storeDataPageInDirectoryLab(Signal* signal);
  void lcpFsOpenConfLab(Signal* signal);

  void zpagesize_error(const char* where);

  void reportMemoryUsage(Signal* signal, int gth);
  void lcp_write_op_to_undolog(Signal* signal);
  void reenable_expand_after_redo_log_exection_complete(Signal*);

  // charsets
  void xfrmKeyData(Signal* signal);

  // Initialisation
  void initData();
  void initRecords();

  // Variables
/* --------------------------------------------------------------------------------- */
/* DIRECTORY RANGE                                                                   */
/* --------------------------------------------------------------------------------- */
  DirRange *dirRange;
  DirRangePtr expDirRangePtr;
  DirRangePtr gnsDirRangePtr;
  DirRangePtr newDirRangePtr;
  DirRangePtr rdDirRangePtr;
  DirRangePtr nciOverflowrangeptr;
  Uint32 cdirrangesize;
  Uint32 cfirstfreeDirrange;
/* --------------------------------------------------------------------------------- */
/* DIRECTORYARRAY                                                                    */
/* --------------------------------------------------------------------------------- */
  Directoryarray *directoryarray;
  DirectoryarrayPtr expDirptr;
  DirectoryarrayPtr rdDirptr;
  DirectoryarrayPtr sdDirptr;
  DirectoryarrayPtr nciOverflowDirptr;
  Uint32 cdirarraysize;
  Uint32 cdirmemory;
  Uint32 cfirstfreedir;
/* --------------------------------------------------------------------------------- */
/* FRAGMENTREC. ALL INFORMATION ABOUT FRAMENT AND HASH TABLE IS SAVED IN FRAGMENT    */
/*         REC  A POINTER TO FRAGMENT RECORD IS SAVED IN ROOTFRAGMENTREC FRAGMENT    */
/* --------------------------------------------------------------------------------- */
  Fragmentrec *fragmentrec;
  FragmentrecPtr fragrecptr;
  Uint32 cfirstfreefrag;
  Uint32 cfragmentsize;
/* --------------------------------------------------------------------------------- */
/* FS_CONNECTREC                                                                     */
/* --------------------------------------------------------------------------------- */
  FsConnectrec *fsConnectrec;
  FsConnectrecPtr fsConnectptr;
  Uint32 cfsConnectsize;
  Uint32 cfsFirstfreeconnect;
/* --------------------------------------------------------------------------------- */
/* FS_OPREC                                                                          */
/* --------------------------------------------------------------------------------- */
  FsOprec *fsOprec;
  FsOprecPtr fsOpptr;
  Uint32 cfsOpsize;
  Uint32 cfsFirstfreeop;
/* --------------------------------------------------------------------------------- */
/* LCP_CONNECTREC                                                                    */
/* --------------------------------------------------------------------------------- */
  LcpConnectrec *lcpConnectrec;
  LcpConnectrecPtr lcpConnectptr;
  Uint32 clcpConnectsize;
  Uint32 cfirstfreelcpConnect;
/* --------------------------------------------------------------------------------- */
/* OPERATIONREC                                                                      */
/* --------------------------------------------------------------------------------- */
  Operationrec *operationrec;
  OperationrecPtr operationRecPtr;
  OperationrecPtr idrOperationRecPtr;
  OperationrecPtr copyInOperPtr;
  OperationrecPtr copyOperPtr;
  OperationrecPtr mlpqOperPtr;
  OperationrecPtr queOperPtr;
  OperationrecPtr readWriteOpPtr;
  Uint32 cfreeopRec;
  Uint32 coprecsize;
/* --------------------------------------------------------------------------------- */
/* OVERFLOW_RECORD                                                                   */
/* --------------------------------------------------------------------------------- */
  OverflowRecord *overflowRecord;
  OverflowRecordPtr iopOverflowRecPtr;
  OverflowRecordPtr tfoOverflowRecPtr;
  OverflowRecordPtr porOverflowRecPtr;
  OverflowRecordPtr priOverflowRecPtr;
  OverflowRecordPtr rorOverflowRecPtr;
  OverflowRecordPtr sorOverflowRecPtr;
  OverflowRecordPtr troOverflowRecPtr;
  Uint32 cfirstfreeoverrec;
  Uint32 coverflowrecsize;

/* --------------------------------------------------------------------------------- */
/* PAGE8                                                                             */
/* --------------------------------------------------------------------------------- */
  Page8 *page8;
  /* 8 KB PAGE                       */
  Page8Ptr ancPageptr;
  Page8Ptr colPageptr;
  Page8Ptr ccoPageptr;
  Page8Ptr datapageptr;
  Page8Ptr delPageptr;
  Page8Ptr excPageptr;
  Page8Ptr expPageptr;
  Page8Ptr gdiPageptr;
  Page8Ptr gePageptr;
  Page8Ptr gflPageptr;
  Page8Ptr idrPageptr;
  Page8Ptr ilcPageptr;
  Page8Ptr inpPageptr;
  Page8Ptr iopPageptr;
  Page8Ptr lastPageptr;
  Page8Ptr lastPrevpageptr;
  Page8Ptr lcnPageptr;
  Page8Ptr lcnCopyPageptr;
  Page8Ptr lupPageptr;
  Page8Ptr priPageptr;
  Page8Ptr pwiPageptr;
  Page8Ptr ciPageidptr;
  Page8Ptr gsePageidptr;
  Page8Ptr isoPageptr;
  Page8Ptr nciPageidptr;
  Page8Ptr rsbPageidptr;
  Page8Ptr rscPageidptr;
  Page8Ptr slPageidptr;
  Page8Ptr sscPageidptr;
  Page8Ptr rlPageptr;
  Page8Ptr rlpPageptr;
  Page8Ptr ropPageptr;
  Page8Ptr rpPageptr;
  Page8Ptr slPageptr;
  Page8Ptr spPageptr;
  Uint32 cfirstfreepage;
  Uint32 cfreepage;
  Uint32 cpagesize;
  Uint32 cfirstfreeLcpPage;
  Uint32 cnoOfAllocatedPages;
  Uint32 cnoLcpPages;
/* --------------------------------------------------------------------------------- */
/* ROOTFRAGMENTREC                                                                   */
/*          DURING EXPAND FRAGMENT PROCESS, EACH FRAGMEND WILL BE EXPAND INTO TWO    */
/*          NEW FRAGMENTS.TO MAKE THIS PROCESS EASIER, DURING ADD FRAGMENT PROCESS   */
/*          NEXT FRAGMENT IDENTIIES WILL BE CALCULATED, AND TWO FRAGMENTS WILL BE    */
/*          ADDED IN (NDBACC). THEREBY EXPAND OF FRAGMENT CAN BE PERFORMED QUICK AND */
/*          EASY.THE NEW FRAGMENT ID SENDS TO TUP MANAGER FOR ALL OPERATION PROCESS. */
/* --------------------------------------------------------------------------------- */
  Rootfragmentrec *rootfragmentrec;
  RootfragmentrecPtr rootfragrecptr;
  Uint32 crootfragmentsize;
  Uint32 cfirstfreerootfrag;
/* --------------------------------------------------------------------------------- */
/* SCAN_REC                                                                          */
/* --------------------------------------------------------------------------------- */
  ScanRec *scanRec;
  ScanRecPtr scanPtr;
  Uint32 cscanRecSize;
  Uint32 cfirstFreeScanRec;
/* --------------------------------------------------------------------------------- */
/* SR_VERSION_REC                                                                    */
/* --------------------------------------------------------------------------------- */
  SrVersionRec *srVersionRec;
  SrVersionRecPtr srVersionPtr;
  Uint32 csrVersionRecSize;
  Uint32 cfirstFreeSrVersionRec;
/* --------------------------------------------------------------------------------- */
/* TABREC                                                                            */
/* --------------------------------------------------------------------------------- */
  Tabrec *tabrec;
  TabrecPtr tabptr;
  Uint32 ctablesize;
/* --------------------------------------------------------------------------------- */
/* UNDOPAGE                                                                          */
/* --------------------------------------------------------------------------------- */
  Undopage *undopage;
                                                   /* 32 KB PAGE                      */
  UndopagePtr undopageptr;
  Uint32 tpwiElementptr;
  Uint32 tpriElementptr;
  Uint32 tgseElementptr;
  Uint32 tgseContainerptr;
  Uint32 trlHead;
  Uint32 trlRelCon;
  Uint32 trlNextused;
  Uint32 trlPrevused;
  Uint32 tlcnChecksum;
  Uint32 tlupElemIndex;
  Uint32 tlupIndex;
  Uint32 tlupForward;
  Uint32 tancNext;
  Uint32 tancBufType;
  Uint32 tancContainerptr;
  Uint32 tancPageindex;
  Uint32 tancPageid;
  Uint32 tidrResult;
  Uint32 tidrElemhead;
  Uint32 tidrForward;
  Uint32 tidrPageindex;
  Uint32 tidrContainerptr;
  Uint32 tidrContainerhead;
  Uint32 tlastForward;
  Uint32 tlastPageindex;
  Uint32 tlastContainerlen;
  Uint32 tlastElementptr;
  Uint32 tlastContainerptr;
  Uint32 tlastContainerhead;
  Uint32 trlPageindex;
  Uint32 tdelContainerptr;
  Uint32 tdelElementptr;
  Uint32 tdelForward;
  Uint32 tiopPageId;
  Uint32 tipPageId;
  Uint32 tgeLocked;
  Uint32 tgeResult;
  Uint32 tgeContainerptr;
  Uint32 tgeElementptr;
  Uint32 tgeForward;
  Uint32 tundoElemIndex;
  Uint32 texpReceivedBucket;
  Uint32 texpDirInd;
  Uint32 texpDirRangeIndex;
  Uint32 texpDirPageIndex;
  Uint32 tdata0;
  Uint32 tcheckpointid;
  Uint32 tciContainerptr;
  Uint32 tnciContainerptr;
  Uint32 tisoContainerptr;
  Uint32 trscContainerptr;
  Uint32 tsscContainerptr;
  Uint32 tciContainerlen;
  Uint32 trscContainerlen;
  Uint32 tsscContainerlen;
  Uint32 tciContainerhead;
  Uint32 tnciContainerhead;
  Uint32 tslElementptr;
  Uint32 tisoElementptr;
  Uint32 tsscElementptr;
  Uint32 tfid;
  Uint32 tscanFlag;
  Uint32 theadundoindex;
  Uint32 tgflBufType;
  Uint32 tgseIsforward;
  Uint32 tsscIsforward;
  Uint32 trscIsforward;
  Uint32 tciIsforward;
  Uint32 tnciIsforward;
  Uint32 tisoIsforward;
  Uint32 tgseIsLocked;
  Uint32 tsscIsLocked;
  Uint32 tkeylen;
  Uint32 tmp;
  Uint32 tmpP;
  Uint32 tmpP2;
  Uint32 tmp1;
  Uint32 tmp2;
  Uint32 tgflPageindex;
  Uint32 tmpindex;
  Uint32 tslNextfree;
  Uint32 tslPageindex;
  Uint32 tgsePageindex;
  Uint32 tnciNextSamePage;
  Uint32 tslPrevfree;
  Uint32 tciPageindex;
  Uint32 trsbPageindex;
  Uint32 tnciPageindex;
  Uint32 tlastPrevconptr;
  Uint32 tresult;
  Uint32 tslUpdateHeader;
  Uint32 tuserptr;
  BlockReference tuserblockref;
  Uint32 tundoindex;
  Uint32 tlqhPointer;
  Uint32 tholdSentOp;
  Uint32 tholdMore;
  Uint32 tlcpLqhCheckV;
  Uint32 tgdiPageindex;
  Uint32 tiopIndex;
  Uint32 tnciTmp;
  Uint32 tullIndex;
  Uint32 turlIndex;
  Uint32 tlfrTmp1;
  Uint32 tlfrTmp2;
  Uint32 tscanTrid1;
  Uint32 tscanTrid2;

  Uint16 clastUndoPageIdWritten;
  Uint32 cactiveCheckpId;
  Uint32 cactiveRootfrag;
  Uint32 cactiveSrFsPtr;
  Uint32 cactiveUndoFilePage;
  Uint32 cactiveOpenUndoFsPtr;
  Uint32 cactiveSrUndoPage;
  Uint32 cprevUndoaddress;
  Uint32 creadyUndoaddress;
  Uint32 ctest;
  Uint32 cundoLogActive;
  Uint32 clqhPtr;
  BlockReference clqhBlockRef;
  Uint32 cminusOne;
  NodeId cmynodeid;
  Uint32 cactiveUndoFileVersion;
  BlockReference cownBlockref;
  BlockReference cndbcntrRef;
  Uint16 csignalkey;
  Uint32 cundopagesize;
  Uint32 cundoposition;
  Uint32 cundoElemIndex;
  Uint32 cundoinfolength;
  Uint32 czero;
  Uint32 csrVersList[16];
  Uint32 clblPageCounter;
  Uint32 clblPageOver;
  Uint32 clblPagesPerTick;
  Uint32 clblPagesPerTickAfterSr;
  Uint32 csystemRestart;
  Uint32 cexcForward;
  Uint32 cexcPageindex;
  Uint32 cexcContainerptr;
  Uint32 cexcContainerhead;
  Uint32 cexcContainerlen;
  Uint32 cexcElementptr;
  Uint32 cexcPrevconptr;
  Uint32 cexcMovedLen;
  Uint32 cexcPrevpageptr;
  Uint32 cexcPrevpageindex;
  Uint32 cexcPrevforward;
  Uint32 clocalkey[32];
  union {
  Uint32 ckeys[2048];
  Uint64 ckeys_align;
  };
  
  Uint32 c_errorInsert3000_TableId;
  Uint32 cSrUndoRecords[UndoHeader::ZNO_UNDORECORD_TYPES];
};

#endif
