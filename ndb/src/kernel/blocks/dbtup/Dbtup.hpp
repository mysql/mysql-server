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

#ifndef DBTUP_H
#define DBTUP_H

#include <pc.hpp>
#include <SimulatedBlock.hpp>
#include <ndb_limits.h>
#include <trigger_definitions.h>
#include <ArrayList.hpp>
#include <AttributeHeader.hpp>
#include <Bitmask.hpp>
#include <signaldata/TupKey.hpp>
#include <signaldata/CreateTrig.hpp>
#include <signaldata/DropTrig.hpp>
#include <signaldata/TrigAttrInfo.hpp>
#include <signaldata/BuildIndx.hpp>

#define ZWORDS_ON_PAGE 8192          /* NUMBER OF WORDS ON A PAGE.      */
#define ZATTRBUF_SIZE 32             /* SIZE OF ATTRIBUTE RECORD BUFFER */
#define ZMIN_PAGE_LIMIT_TUPKEYREQ 5
#define ZTUP_VERSION_BITS 15

typedef bool (Dbtup::* ReadFunction)(Uint32*,
                                     AttributeHeader*,
                                     Uint32,
                                     Uint32);
typedef bool (Dbtup::* UpdateFunction)(Uint32*,
                                       Uint32,
                                       Uint32);

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
// DbtupBuffer.cpp         2000
// DbtupRoutines.cpp       3000
// DbtupCommit.cpp         5000
// DbtupFixAlloc.cpp       6000
// DbtupTrigger.cpp        7000
// DbtupAbort.cpp          9000
// DbtupLCP.cpp           10000
// DbtupUndoLog.cpp       12000
// DbtupPageMap.cpp       14000
// DbtupPagMan.cpp        16000
// DbtupStoredProcDef.cpp 18000
// DbtupMeta.cpp          20000
// DbtupTabDesMan.cpp     22000
// DbtupGen.cpp           24000
// DbtupSystemRestart.cpp 26000
// DbtupIndex.cpp         28000
// DbtupDebug.cpp         30000
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
#define ZNO_OF_LCP_REC 10                   /* NUMBER OF CONCURRENT CHECKPOINTS*/
#define TOT_PAGE_RECORD_SPACE 262144        /* SIZE OF PAGE RECORD FILE.       */
#define ZNO_OF_PAGE TOT_PAGE_RECORD_SPACE/ZWORDS_ON_PAGE   
#define ZNO_OF_PAGE_RANGE_REC 128           /* SIZE OF PAGE RANGE FILE         */
#define ZNO_OF_PARALLELL_UNDO_FILES 16      /* NUMBER OF PARALLEL UNDO FILES   */
#define ZNO_OF_RESTART_INFO_REC 10          /* MAXIMUM PARALLELL RESTART INFOS */
		    /* 24 SEGMENTS WITH 8 PAGES IN EACH*/
                    /* PLUS ONE UNDO BUFFER CACHE      */
// Undo record identifiers are 32-bits with page index 13-bits
#define ZUNDO_RECORD_ID_PAGE_INDEX 13	   /* 13 BITS = 8192 WORDS/PAGE	      */
#define ZUNDO_RECORD_ID_PAGE_INDEX_MASK (ZWORDS_ON_PAGE - 1) /* 1111111111111 */

// Trigger constants
#define ZDEFAULT_MAX_NO_TRIGGERS_PER_TABLE 16

/* ---------------------------------------------------------------- */
// VARIABLE NUMBERS OF PAGE_WORD, UNDO_WORD AND LOGIC_WORD FOR
// COMMUNICATION WITH FILE SYSTEM
/* ---------------------------------------------------------------- */
#define ZBASE_ADDR_PAGE_WORD 1              /* BASE ADDRESS OF PAGE_WORD VAR   */
#define ZBASE_ADDR_UNDO_WORD 2              /* BASE ADDRESS OF UNDO_WORD VAR   */
#define ZBASE_ADDR_LOGIC_WORD 3             /* BASE ADDRESS OF LOGIC_WORD VAR  */

/* ---------------------------------------------------------------- */
// NUMBER OF PAGES SENT TO DISK IN DATA BUFFER AND UNDO BUFFER WHEN
// OPTIMUM PERFORMANCE IS ACHIEVED.
/* ---------------------------------------------------------------- */
#define ZUB_SEGMENT_SIZE 8                  /* SEGMENT SIZE OF UNDO BUFFER     */
#define ZDB_SEGMENT_SIZE 8                  /* SEGMENT SIZE OF DATA BUFFER     */

/* ---------------------------------------------------------------- */
/* A ATTRIBUTE MAY BE NULL, DYNAMIC OR NORMAL. A NORMAL ATTRIBUTE   */
/* IS A ATTRIBUTE THAT IS NOT NULL OR DYNAMIC. A NULL ATTRIBUTE     */
/* MAY HAVE NO VALUE. A DYNAMIC ATTRIBUTE IS A NULL ATTRIBUTE THAT  */
/* DOES NOT HAVE TO BE A MEMBER OF EVERY TUPLE I A CERTAIN TABLE.   */
/* ---------------------------------------------------------------- */
/**
 * #defines moved into include/kernel/Interpreter.hpp
 */
#define ZMAX_REGISTER 21
#define ZINSERT_DELETE 0
/* ---------------------------------------------------------------- */
/* THE MINIMUM SIZE OF AN 'EMPTY' TUPLE HEADER IN R-WORDS           */
/* ---------------------------------------------------------------- */
#define ZTUP_HEAD_MINIMUM_SIZE 2
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

#define ZTH_TYPE3 2            /* TUPLE HEADER THAT MAY HAVE A POINTER TO   */
                               /* A DYNAMIC ATTRIBUTE HEADER. IT MAY ALSO   */
                               /* CONTAIN SHORT ATTRIBUTES AND POINTERS     */
                               /* TO LONG ATTRIBUTE HEADERS.                */

          /* DATA STRUCTURE TYPES */
          /* WHEN ATTRIBUTE INFO IS SENT WITH A ATTRINFO-SIGNAL THE         */
          /* VARIABLE TYPE IS SPECIFYED. THIS MUST BE DONE TO BE ABLE TO    */
          /* NOW HOW MUCH DATA OF A ATTRIBUTE TO READ FROM ATTRINFO.        */
#define ZFIXED_ARRAY 2                             /* ZFIXED  ARRAY FIELD.                  */
#define ZNON_ARRAY 1                               /* NORMAL FIELD.                         */
#define ZVAR_ARRAY 0                               /* VARIABLE ARRAY FIELD                  */
#define ZNOT_STORE 3                               /* THE ATTR IS STORED IN THE INDEX BLOCK */
#define ZMAX_SMALL_VAR_ARRAY 256

          /* PLEASE OBSERVE THAT THEESE CONSTANTS CORRESPONDS TO THE NUMBER */
          /* OF BITS NEEDED TO REPRESENT THEM    D O    N O T   C H A N G E */
#define Z1BIT_VAR 0                                /* 1 BIT VARIABLE.                        */
#define Z2BIT_VAR 1                                /* 2 BIT VARIABLE.                        */
#define Z4BIT_VAR 2                                /* 4 BIT VARIABLE.                        */
#define Z8BIT_VAR 3                                /* 8 BIT VARIABLE.                        */
#define Z16BIT_VAR 4                               /* 16 BIT VARIABLE.                       */
#define Z32BIT_VAR 5                               /* 32 BIT VARIABLE.                       */
#define Z64BIT_VAR 6                               /* 64 BIT VARIABLE.                       */
#define Z128BIT_VAR 7                              /* 128 BIT VARIABLE.                      */

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

#define ZSTORED_SEIZE_ATTRINBUFREC_ERROR 873 // Part of Scan

#define ZREAD_ONLY_CONSTRAINT_VIOLATION 893
#define ZVAR_SIZED_NOT_SUPPORTED 894
#define ZINCONSISTENT_NULL_ATTRIBUTE_COUNT 895
#define ZTUPLE_CORRUPTED_ERROR 896
#define ZTRY_UPDATE_PRIMARY_KEY 897
#define ZMUST_BE_ABORTED_ERROR 898
#define ZTUPLE_DELETED_ERROR 626
#define ZINSERT_ERROR 630


          /* SOME WORD POSITIONS OF FIELDS IN SOME HEADERS */
#define ZPAGE_STATE_POS 0                 /* POSITION OF PAGE STATE            */
#define ZPAGE_NEXT_POS 1                  /* POSITION OF THE NEXT POINTER WHEN IN FREELIST     */
#define ZPAGE_PREV_POS 2                  /* POSITION OF THE PREVIOUS POINTER WHEN IN FREELIST */
#define ZFREELIST_HEADER_POS 3            /* POSITION OF THE FIRST FREELIST        */
#define ZPAGE_FRAG_PAGE_ID_POS 4          /* POSITION OF FRAG PAGE ID WHEN USED*/
#define ZPAGE_NEXT_CLUST_POS 5            /* POSITION OF NEXT FREE SET OF PAGES    */
#define ZPAGE_FIRST_CLUST_POS 2           /* POSITION OF THE POINTER TO THE FIRST PAGE IN A CLUSTER */
#define ZPAGE_LAST_CLUST_POS 6            /* POSITION OF THE POINTER TO THE LAST PAGE IN A CLUSTER */
#define ZPAGE_PREV_CLUST_POS 7            /* POSITION OF THE PREVIOUS POINTER  */
#define ZPAGE_HEADER_SIZE 32              /* NUMBER OF WORDS IN MEM  PAGEHEADER        */
#define ZDISK_PAGE_HEADER_SIZE 32         /* NUMBER OF WORDS IN DISK PAGEHEADER        */
#define ZNO_OF_FREE_BLOCKS 3              /* NO OF FREE BLOCK IN THE DISK PAGE         */
#define ZDISK_PAGE_ID 8                   /*  ID OF THE PAGE ON THE DISK               */
#define ZBLOCK_LIST 9
#define ZCOPY_OF_PAGE 10
#define ZPAGE_PHYSICAL_INDEX 11
#define ZNEXT_IN_PAGE_USED_LIST 12
#define ZPREV_IN_PAGE_USED_LIST 13
#define ZDISK_USED_TYPE 14
#define ZFREE_COMMON 1                    /* PAGE STATE, PAGE IN COMMON AREA                   */
#define ZEMPTY_MM 2                       /* PAGE STATE, PAGE IN EMPTY LIST                    */
#define ZTH_MM_FREE 3                     /* PAGE STATE, TUPLE HEADER PAGE WITH FREE AREA      */
#define ZTH_MM_FULL 4                     /* PAGE STATE, TUPLE HEADER PAGE WHICH IS FULL       */
#define ZAC_MM_FREE 5                     /* PAGE STATE, ATTRIBUTE CLUSTER PAGE WITH FREE AREA */
#define ZTH_MM_FREE_COPY 7                /* PAGE STATE, TH COPY PAGE WITH FREE AREA           */
#define ZTH_MM_FULL_COPY 8                /* PAGE STATE, TH COPY PAGE WHICH IS FULL            */
#define ZAC_MM_FREE_COPY 9                /* PAGE STATE, AC COPY PAGE WITH FREE AREA           */
#define ZMAX_NO_COPY_PAGES 4              /* THE MAXIMUM NUMBER OF COPY PAGES ALLOWED PER FRAGMENT */

          /* CONSTANTS USED TO HANDLE TABLE DESCRIPTOR RECORDS                   */
          /* ALL POSITIONS AND SIZES IS BASED ON R-WORDS (32-BIT ON APZ 212)     */
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

          /* CHECKPOINT RECORD TYPES */
#define ZLCPR_TYPE_INSERT_TH 0             /* INSERT TUPLE HEADER                             */
#define ZLCPR_TYPE_DELETE_TH 1             /* DELETE TUPLE HEADER                             */
#define ZLCPR_TYPE_UPDATE_TH 2             /* DON'T CREATE IT, JUST UPDETE                    */
#define ZLCPR_TYPE_INSERT_TH_NO_DATA 3     /* INSERT TUPLE HEADER                             */
#define ZLCPR_ABORT_UPDATE 4               /* UNDO AN UPDATE OPERATION THAT WAS ACTIVE IN LCP */
#define ZLCPR_ABORT_INSERT 5               /* UNDO AN INSERT OPERATION THAT WAS ACTIVE IN LCP */
#define ZTABLE_DESCRIPTOR 6                /* TABLE DESCRIPTOR                                */
#define ZINDICATE_NO_OP_ACTIVE 7           /* ENSURE THAT NO OPERATION ACTIVE AFTER RESTART   */
#define ZLCPR_UNDO_LOG_PAGE_HEADER 8       /* CHANGE IN PAGE HEADER IS UNDO LOGGED            */
#define ZLCPR_TYPE_UPDATE_GCI 9            /* Update GCI at commit time                       */
#define ZNO_CHECKPOINT_RECORDS 10          /* NUMBER OF CHECKPOINTRECORD TYPES                */

          /* RESULT CODES            */
          /* ELEMENT POSITIONS IN SYSTEM RESTART INFO PAGE OF THE DATA FILE */
#define ZSRI_NO_OF_FRAG_PAGES_POS 10       /* NUMBER OF FRAGMENT PAGES WHEN CHECKPOINT STARTED   */
#define ZSRI_TUP_RESERVED_SIZE_POS 11      /* RESERVED SIZE OF THE TUPLE WHEN CP STARTED         */
#define ZSRI_TUP_FIXED_AREA_POS 12         /* SIZE OF THE TUPLE FIXED AREA WHEN CP STARTED       */
#define ZSRI_TAB_DESCR_SIZE 13             /* SIZE OF THE TABLE DESCRIPTOR WHEN CP STARTED       */
#define ZSRI_NO_OF_ATTRIBUTES_POS 14       /* NUMBER OF ATTRIBUTES                               */
#define ZSRI_UNDO_LOG_END_REC_ID 15        /* LAST UNDO LOG RECORD ID FOR THIS CHECKPOINT        */
#define ZSRI_UNDO_LOG_END_PAGE_ID 16       /* LAST USED LOG PAGE ID FOR THIS CHECKPOINT          */
#define ZSRI_TH_FREE_FIRST 17              /* FIRST FREE PAGE OF TUPLE HEADERS                   */
#define ZSRI_TH_FREE_COPY_FIRST 18         /* FIRST FREE PAGE OF TUPLE HEADER COPIES             */
#define ZSRI_EMPTY_PRIM_PAGE 27            /* FIRST EMPTY PAGE                                   */
#define ZSRI_NO_COPY_PAGES_ALLOC 28        /* NO COPY PAGES IN FRAGMENT AT LOCAL CHECKPOINT      */
#define ZSRI_UNDO_FILE_VER 29              /* CHECK POINT ID OF THE UNDO FILE                    */
#define ZSRI_NO_OF_INDEX_ATTR 30           /* No of index attributes                             */
#define ZNO_OF_PAGES_CLUSTER_REC 0

//------------------------------------------------------------
// TUP_CONTINUEB codes
//------------------------------------------------------------
#define ZSTART_EXEC_UNDO_LOG 0
#define ZCONT_START_SAVE_CL 1
#define ZCONT_SAVE_DP 2
#define ZCONT_EXECUTE_LC 3
#define ZCONT_LOAD_DP 4
#define ZLOAD_BAL_LCP_TIMER 5
#define ZINITIALISE_RECORDS 6
#define ZREL_FRAG 7
#define ZREPORT_MEMORY_USAGE 8
#define ZBUILD_INDEX 9

#define ZINDEX_STORAGE 0
#define ZDATA_WORD_AT_DISK_PAGE 2030
#define ZALLOC_DISK_PAGE_LAST_INDEX 2047
#define ZWORD_IN_BLOCK 127                 /* NO OF WORD IN A BLOCK */
#define ZNO_DISK_PAGES_FILE_REC 100
#define ZMASK_PAGE_INDEX 0x7ff
#define ZBIT_PAGE_INDEX 11                 /* 8 KBYT PAGE = 2048 WORDS */
#define ZSCAN_PROCEDURE 0
#define ZCOPY_PROCEDURE 2
#define ZSTORED_PROCEDURE_DELETE 3
#define ZSTORED_PROCEDURE_FREE 0xffff
#define ZMIN_PAGE_LIMIT_TUP_COMMITREQ 2
#define ZUNDO_PAGE_HEADER_SIZE 2           /* SIZE OF UNDO PAGE HEADER     */
#endif

class Dbtup: public SimulatedBlock {
public:
// State values
enum State {
  NOT_INITIALIZED = 0,
  COMMON_AREA_PAGES = 1,
  UNDO_RESTART_PAGES = 2,
  UNDO_PAGES = 3,
  READ_ONE_PAGE = 4,
  CHECKPOINT_DATA_READ = 7,
  CHECKPOINT_DATA_READ_PAGE_ZERO = 8,
  CHECKPOINT_DATA_WRITE = 9,
  CHECKPOINT_DATA_WRITE_LAST = 10,
  CHECKPOINT_DATA_WRITE_FLUSH = 11,
  CHECKPOINT_UNDO_READ = 12,
  CHECKPOINT_UNDO_READ_FIRST = 13,
  CHECKPOINT_UNDO_WRITE = 14,
  CHECKPOINT_UNDO_WRITE_FLUSH = 15,
  CHECKPOINT_TD_READ = 16,
  IDLE = 17,
  ACTIVE = 18,
  SYSTEM_RESTART = 19,
  NO_OTHER_OP = 20,
  COMMIT_DELETE = 21,
  TO_BE_COMMITTED = 22,
  ABORTED = 23,
  ALREADY_ABORTED_INSERT = 24,
  ALREADY_ABORTED = 25,
  ABORT_INSERT = 26,
  ABORT_UPDATE = 27,
  INIT = 28,
  INITIAL_READ = 29,
  INTERPRETED_EXECUTION = 30,
  FINAL_READ = 31,
  FINAL_UPDATE = 32,
  DISCONNECTED = 33,
  DEFINED = 34,
  ERROR_WAIT_TUPKEYREQ = 35,
  STARTED = 36,
  NOT_DEFINED = 37,
  COMPLETED = 38,
  WAIT_ABORT = 39,
  NORMAL_PAGE = 40,
  COPY_PAGE = 41,
  DELETE_BLOCK = 42,
  WAIT_STORED_PROCEDURE_ATTR_INFO = 43,
  DATA_FILE_READ = 45,
  DATA_FILE_WRITE = 46,
  LCP_DATA_FILE_READ = 47,
  LCP_DATA_FILE_WRITE = 48,
  LCP_DATA_FILE_WRITE_WITH_UNDO = 49,
  LCP_DATA_FILE_CLOSE = 50,
  LCP_UNDO_FILE_READ = 51,
  LCP_UNDO_FILE_CLOSE = 52,
  LCP_UNDO_FILE_WRITE = 53,
  OPENING_DATA_FILE = 54,
  INITIATING_RESTART_INFO = 55,
  INITIATING_FRAGMENT = 56,
  OPENING_UNDO_FILE = 57,
  READING_RESTART_INFO = 58,
  INIT_UNDO_SEGMENTS = 59,
  READING_TAB_DESCR = 60,
  READING_DATA_PAGES = 61,
  WAIT_COPY_PROCEDURE = 62,
  TOO_MUCH_AI = 63,
  SAME_PAGE = 64,
  DEFINING = 65,
  TUPLE_BLOCKED = 66,
  ERROR_WAIT_STORED_PROCREQ = 67
};

// Records
/* ************** ATTRIBUTE INFO BUFFER RECORD ****************** */
/* THIS RECORD IS USED AS A BUFFER FOR INCOMING AND OUTGOING DATA */
/* ************************************************************** */
struct Attrbufrec {
  Uint32 attrbuf[ZATTRBUF_SIZE];
}; /* p2c: size = 128 bytes */

typedef Ptr<Attrbufrec> AttrbufrecPtr;

/* ********** CHECKPOINT INFORMATION ************ */
/* THIS RECORD HOLDS INFORMATION NEEDED TO        */
/* PERFORM A CHECKPOINT. IT'S POSSIBLE TO RUN     */
/* MULTIPLE CHECKPOINTS AT A TIME. THIS RECORD    */
/* MAKES IT POSSIBLE TO DISTINGER BETWEEN THE     */
/* DIFFERENT CHECKPOINTS.                         */
/* ********************************************** */
struct CheckpointInfo {
  Uint32 lcpNextRec;                           /* NEXT RECORD IN FREELIST                          */
  Uint32 lcpCheckpointVersion;                 /* VERSION OF THE CHECKPOINT                        */
  Uint32 lcpLocalLogInfoP;                     /* POINTER TO A LOCAL LOG INFO RECORD               */
  Uint32 lcpUserptr;                           /* USERPOINTER TO THE BLOCK REQUESTING THE CP       */
  Uint32 lcpFragmentP;                         /* FRAGMENT POINTER TO WHICH THE CHECKPOINT APPLIES */
  Uint32 lcpFragmentId;                        /* FRAGMENT ID                                      */
  Uint32 lcpTabPtr;                            /* TABLE POINTER                                    */
  Uint32 lcpDataBufferSegmentP;                /* POINTER TO A DISK BUFFER SEGMENT POINTER (DATA)  */
  Uint32 lcpDataFileHandle;   /* FILE HANDLES FOR DATA FILE. LOG FILE HANDLE IN LOCAL_LOG_INFO_RECORD */
                                              /* FILE HANDLE TO THE OPEN DATA FILE                */
  Uint32 lcpNoOfPages;
  Uint32 lcpThFreeFirst;
  Uint32 lcpThFreeCopyFirst;
  Uint32 lcpEmptyPrimPage;
  Uint32 lcpNoCopyPagesAlloc;
  Uint32 lcpTmpOperPtr;                        /* TEMPORARY STORAGE OF OPER_PTR DURING SAVE        */
  BlockReference lcpBlockref;                       /* BLOCKREFERENCE TO THE BLOCK REQUESTING THE CP    */
};
typedef Ptr<CheckpointInfo> CheckpointInfoPtr;

/* *********** DISK BUFFER SEGMENT INFO ********* */
/* THIS RECORD HOLDS INFORMATION NEEDED DURING    */
/* A WRITE OF THE DATA BUFFER TO DISK. WHEN THE   */
/* WRITE SIGNAL IS SENT A POINTER TO THIS RECORD  */
/* IS INCLUDED. WHEN THE WRITE IS COMPLETED AND   */
/* CONFIRMED THE PTR TO THIS RECORD IS RETURNED   */
/* AND THE BUFFER PAGES COULD EASILY BE LOCATED   */
/* AND DEALLOCATED. THE CHECKPOINT_INFO_VERSION   */
/* KEEPS TRACK OF THE CHECPOINT_INFO_RECORD THAT  */
/* INITIATED THE WRITE AND THE CP_PAGE_TO_DISK    */
/* ELEMENT COULD BE INCREASED BY THE NUMBER OF    */
/* PAGES WRITTEN.                                 */
/* ********************************************** */
struct DiskBufferSegmentInfo {
  Uint32 pdxDataPage[16];                     /* ARRAY OF DATA BUFFER PAGES */
  Uint32 pdxUndoBufferSet[2];
  Uint32 pdxNextRec;
  State pdxBuffertype;
  State pdxOperation;
             /*---------------------------------------------------------------------------*/
             /* PDX_FLAGS BITS AND THEIR USAGE:                                           */
             /* BIT    0                    1                      COMMENT                */
             /*---------------------------------------------------------------------------*/
             /* 0      SEGMENT INVALID      SEGMENT VALID          USED DURING READS      */
             /* 1-15                                               NOT USED               */
             /*---------------------------------------------------------------------------*/
  Uint32 pdxCheckpointInfoP;                  /* USED DURING LOCAL CHKP     */
  Uint32 pdxRestartInfoP;                     /* USED DURING RESTART        */
  Uint32 pdxLocalLogInfoP;                    /* POINTS TO A LOCAL LOG INFO */
  Uint32 pdxFilePage;                         /* START PAGE IN FILE         */
  Uint32 pdxNumDataPages;                     /* NUMBER OF DATA PAGES       */
};
typedef Ptr<DiskBufferSegmentInfo> DiskBufferSegmentInfoPtr;

struct Fragoperrec {
  bool   definingFragment;
  Uint32 nextFragoprec;
  Uint32 lqhPtrFrag;
  Uint32 fragidFrag;
  Uint32 tableidFrag;
  Uint32 fragPointer;
  Uint32 attributeCount;
  Uint32 freeNullBit;
  Uint32 noOfNewAttrCount;
  Uint32 charsetIndex;
  BlockReference lqhBlockrefFrag;
};
typedef Ptr<Fragoperrec> FragoperrecPtr;

struct Fragrecord {
  Uint32 nextStartRange;
  Uint32 currentPageRange;
  Uint32 rootPageRange;
  Uint32 noOfPages;
  Uint32 emptyPrimPage;

  Uint32 firstusedOprec;

  Uint32 thFreeFirst;
  Uint32 thFreeCopyFirst;
  Uint32 noCopyPagesAlloc;

  Uint32 checkpointVersion;
  Uint32 minPageNotWrittenInCheckpoint;
  Uint32 maxPageWrittenInCheckpoint;
  State fragStatus;
  Uint32 fragTableId;
  Uint32 fragmentId;
  Uint32 nextfreefrag;
};
typedef Ptr<Fragrecord> FragrecordPtr;

          /* ************ LOCAL LOG FILE INFO ************* */
          /* THIS RECORD HOLDS INFORMATION NEEDED DURING    */
          /* CHECKPOINT AND RESTART. THERE ARE FOUR         */
          /* PARALLELL UNDO LOG FILES, EACH ONE REPRESENTED */
          /* BY AN ENTITY OF THIS RECORD.                   */
          /* BECAUSE EACH FILE IS SHARED BETWEEN FOUR       */
          /* TABLES AND HAS ITS OWN PAGEPOINTERS AND        */
          /* WORDPOINTERS.                                  */
          /* ********************************************** */
struct LocalLogInfo {
  Uint32 lliActiveLcp;                                   /* NUMBER OF ACTIVE LOCAL CHECKPOINTS ON THIS FILE */
  Uint32 lliEndPageId;                                   /* PAGE IDENTIFIER OF LAST PAGE WITH LOG DATA      */
  Uint32 lliPrevRecordId;                                /* PREVIOUS RECORD IN THIS LOGFILE                 */
  Uint32 lliLogFilePage;                                 /* PAGE IN LOGFILE                                 */
  Uint32 lliNumFragments;                                /* NO OF FRAGMENTS RESTARTING FROM THIS LOCAL LOG  */
  Uint32 lliUndoBufferSegmentP;                          /* POINTER TO A DISK BUFFER SEGMENT POINTER (UNDO) */
  Uint32 lliUndoFileHandle;                              /* FILE HANDLE OF UNDO LOG FILE                    */
  Uint32 lliUndoPage;                                    /* UNDO PAGE IN BUFFER                             */
  Uint32 lliUndoWord;
  Uint32 lliUndoPagesToDiskWithoutSynch;
};
typedef Ptr<LocalLogInfo> LocalLogInfoPtr;

struct Operationrec {
// Easy to remove (2 words)
  Uint32 attroutbufLen;
  Uint32 logSize;

// Needed (20 words)
  State tupleState;
  Uint32 prevActiveOp;
  Uint32 nextActiveOp;
  Uint32 nextOprecInList;
  Uint32 prevOprecInList;
  Uint32 tableRef;
  Uint32 fragId;
  Uint32 fragmentPtr;
  Uint32 fragPageId;
  Uint32 realPageId;
  bool undoLogged;
  Uint32 realPageIdC;
  Uint32 fragPageIdC;
  Uint32 firstAttrinbufrec;
  Uint32 lastAttrinbufrec;
  Uint32 attrinbufLen;
  Uint32 currentAttrinbufLen;
  Uint32 userpointer;
  State transstate;
  Uint32 savePointId;

// Easy to remove (3 words)
  Uint32 tcOperationPtr;
  Uint32 transid1;
  Uint32 transid2;

// Needed (2 words)
  Uint16 pageIndex;
  Uint16 pageOffset;
  Uint16 pageOffsetC;
  Uint16 pageIndexC;
// Hard to remove
  Uint16 tupVersion;

// Easy to remove (1.5 word)
  BlockReference recBlockref;
  BlockReference userblockref;
  Uint16 storedProcedureId;

  Uint8 inFragList;
  Uint8 inActiveOpList;
  Uint8 deleteInsertFlag;

// Needed (1 word)
  Uint8 dirtyOp;
  Uint8 interpretedExec;
  Uint8 optype;
  Uint8 opSimple;

// Used by triggers
  Uint32 primaryReplica;
  BlockReference coordinatorTC;
  Uint32 tcOpIndex;
  Uint32 gci;
  Uint32 noFiredTriggers;
  union {
    Uint32 hashValue; // only used in TUP_COMMITREQ
    Uint32 lastRow;
  };
  Bitmask<MAXNROFATTRIBUTESINWORDS> changeMask;
};
typedef Ptr<Operationrec> OperationrecPtr;

struct Page {
  Uint32 pageWord[ZWORDS_ON_PAGE];
};
typedef Ptr<Page> PagePtr;

          /* ****************************** PAGE RANGE RECORD ************************** */
          /* PAGE RANGES AND BASE PAGE ID. EACH RANGE HAS A  CORRESPONDING BASE PAGE ID  */
          /* THAT IS USED TO  CALCULATE REAL PAGE ID FROM A FRAGMENT PAGE ID AND A TABLE */
          /* REFERENCE.                                                                  */
          /* THE PAGE RANGES ARE ORGANISED IN A B-TREE FASHION WHERE THE VARIABLE TYPE   */
          /* SPECIFIES IF A LEAF NODE HAS BEEN REACHED. IF A LEAF NODE HAS BEEN REACHED  */
          /* THEN BASE_PAGE_ID IS THE BASE_PAGE_ID OF THE SET OF PAGES THAT WAS          */
          /* ALLOCATED IN THAT RANGE. OTHERWISE BASE_PAGE_ID IS THE POINTER TO THE NEXT  */
          /* PAGE_RANGE RECORD.                                                          */
          /* *************************************************************************** */
struct PageRange {
  Uint32 startRange[4];                                  /* START OF RANGE                                   */
  Uint32 endRange[4];                                    /* END OF THIS RANGE                                */
  Uint32 basePageId[4];                                  /* BASE PAGE ID.                                    */
/*----               VARIABLE BASE_PAGE_ID2 (4) 8 DS NEEDED WHEN SUPPORTING 40 BIT PAGE ID           -------*/
  Uint8 type[4];                                        /* TYPE OF BASE PAGE ID                             */
  Uint32 nextFree;                                       /* NEXT FREE PAGE RANGE RECORD                      */
  Uint32 parentPtr;                                      /* THE PARENT TO THE PAGE RANGE REC IN THE B-TREE   */
  Uint8 currentIndexPos;
};
typedef Ptr<PageRange> PageRangePtr;

          /* *********** PENDING UNDO WRITE INFO ********** */
          /* THIS RECORD HOLDS INFORMATION NEEDED DURING    */
          /* A FILE OPEN OPERATION                          */
          /* IF THE FILE OPEN IS A PART OF A CHECKPOINT THE */
          /* CHECKPOINT_INFO_P WILL HOLD A POINTER TO THE   */
          /* CHECKPOINT_INFOR_PTR RECORD                    */
          /* IF IT IS A PART OF RESTART THE PFO_RESTART_INFO*/
          /* ELEMENT WILL POINT TO A RESTART INFO RECORD    */
          /* ********************************************** */
struct PendingFileOpenInfo {
  Uint32 pfoNextRec;
  State pfoOpenType;
  Uint32 pfoCheckpointInfoP;
  Uint32 pfoRestartInfoP;
};
typedef Ptr<PendingFileOpenInfo> PendingFileOpenInfoPtr;

struct RestartInfoRecord {
  Uint32 sriNextRec;
  State sriState;                                       /* BLOCKREFERENCE TO THE REQUESTING BLOCK           */
  Uint32 sriUserptr;                                     /* USERPOINTER TO THE REQUESTING BLOCK              */
  Uint32 sriDataBufferSegmentP;                          /* POINTER TO A DISK BUFFER SEGMENT POINTER (DATA)  */
  Uint32 sriDataFileHandle;                              /* FILE HANDLE TO THE OPEN DATA FILE                */
  Uint32 sriCheckpointVersion;                           /* CHECKPOINT VERSION TO RESTART FROM               */
  Uint32 sriFragid;                                      /* FRAGMENT ID                                      */
  Uint32 sriFragP;                                       /* FRAGMENT POINTER                                 */
  Uint32 sriTableId;                                     /* TABLE ID                                         */
  Uint32 sriLocalLogInfoP;                               /* POINTER TO A LOCAL LOG INFO RECORD               */
  Uint32 sriNumDataPages;                                /* NUMBER OF DATA PAGES TO READ                     */
  Uint32 sriCurDataPageFromBuffer;                       /* THE CHECKPOINT IS COMPLETED                      */
  BlockReference sriBlockref;
};
typedef Ptr<RestartInfoRecord> RestartInfoRecordPtr;

  /* ************* TRIGGER DATA ************* */
  /* THIS RECORD FORMS LISTS OF ACTIVE       */
  /* TRIGGERS FOR EACH TABLE.                 */
  /* THE RECORDS ARE MANAGED BY A TRIGGER     */
  /* POOL wHERE A TRIGGER RECORD IS SEIZED    */
  /* WHEN A TRIGGER IS ACTIVATED AND RELEASED */
  /* WHEN THE TRIGGER IS DEACTIVATED.         */
  /* **************************************** */
struct TupTriggerData {
  
  /**
   * Trigger id, used by DICT/TRIX to identify the trigger
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

  ReadFunction* readFunctionArray;
  UpdateFunction* updateFunctionArray;
  CHARSET_INFO** charsetArray;

  Uint32 readKeyArray;
  Uint32 tabDescriptor;
  Uint32 attributeGroupDescriptor;

  bool   GCPIndicator;
  bool   checksumIndicator;

  Uint16 tupheadsize;
  Uint16 noOfAttr;
  Uint16 noOfKeyAttr;
  Uint16 noOfCharsets;
  Uint16 noOfNewAttr;
  Uint16 noOfNullAttr;
  Uint16 noOfAttributeGroups;

  Uint8  tupChecksumIndex;
  Uint8  tupNullIndex;
  Uint8  tupNullWords;
  Uint8  tupGCPIndex;

  // Lists of trigger data for active triggers
  ArrayList<TupTriggerData> afterInsertTriggers;
  ArrayList<TupTriggerData> afterDeleteTriggers;
  ArrayList<TupTriggerData> afterUpdateTriggers;
  ArrayList<TupTriggerData> subscriptionInsertTriggers;
  ArrayList<TupTriggerData> subscriptionDeleteTriggers;
  ArrayList<TupTriggerData> subscriptionUpdateTriggers;
  ArrayList<TupTriggerData> constraintUpdateTriggers;

  // List of ordered indexes
  ArrayList<TupTriggerData> tuxCustomTriggers;

  Uint32 fragid[2 * MAX_FRAG_PER_NODE];
  Uint32 fragrec[2 * MAX_FRAG_PER_NODE];

  struct {
    Uint32 tabUserPtr;
    Uint32 tabUserRef;
  } m_dropTable;
  State tableStatus;
};

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

          /* **************** UNDO PAGE RECORD ******************* */
          /* THIS RECORD FORMS AN UNDO PAGE CONTAINING A NUMBER OF */
          /* DATA WORDS. CURRENTLY THERE ARE 2048 WORDS ON A PAGE  */
          /* EACH OF 32 BITS (4 BYTES) WHICH FORMS AN UNDO PAGE    */
          /* WITH A TOTAL OF 8192 BYTES                            */
          /* ***************************************************** */
struct UndoPage {
  Uint32 undoPageWord[ZWORDS_ON_PAGE]; /* 32 KB */
};
typedef Ptr<UndoPage> UndoPagePtr;

  /*
   * Build index operation record.
   */
  struct BuildIndexRec {
    // request cannot use signal class due to extra members
    Uint32 m_request[BuildIndxReq::SignalLength];
    Uint32 m_triggerPtrI;       // the index trigger
    Uint32 m_fragNo;            // fragment number under Tablerec
    Uint32 m_pageId;            // logical fragment page id
    Uint32 m_tupleNo;           // tuple number on page (pageIndex >> 1)
    BuildIndxRef::ErrorCode m_errorCode;
    union {
      Uint32 nextPool;
      Uint32 nextList;
    };
    Uint32 prevList;
  };
  typedef Ptr<BuildIndexRec> BuildIndexPtr;
  ArrayPool<BuildIndexRec> c_buildIndexPool;
  ArrayList<BuildIndexRec> c_buildIndexList;
  Uint32 c_noOfBuildIndexRec;

public:
  Dbtup(const class Configuration &);
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
  int tuxReadPk(Uint32 fragPtrI, Uint32 pageId, Uint32 pageOffset, Uint32* dataOut);

  /*
   * TUX checks if tuple is visible to scan.
   */
  bool tuxQueryTh(Uint32 fragPtrI, Uint32 tupAddr, Uint32 tupVersion, Uint32 transId1, Uint32 transId2, Uint32 savePointId);

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
  void execSTORED_PROCREQ(Signal* signal);
  void execTUPFRAGREQ(Signal* signal);
  void execTUP_ADD_ATTRREQ(Signal* signal);
  void execTUP_COMMITREQ(Signal* signal);
  void execTUP_ABORTREQ(Signal* signal);
  void execTUP_SRREQ(Signal* signal);
  void execTUP_PREPLCPREQ(Signal* signal);
  void execFSOPENCONF(Signal* signal);
  void execFSOPENREF(Signal* signal);
  void execFSCLOSECONF(Signal* signal);
  void execFSCLOSEREF(Signal* signal);
  void execFSWRITECONF(Signal* signal);
  void execFSWRITEREF(Signal* signal);
  void execFSREADCONF(Signal* signal);
  void execFSREADREF(Signal* signal);
  void execNDB_STTOR(Signal* signal);
  void execREAD_CONFIG_REQ(Signal* signal);
  void execSET_VAR_REQ(Signal* signal);
  void execDROP_TAB_REQ(Signal* signal);
  void execALTER_TAB_REQ(Signal* signal);
  void execFSREMOVECONF(Signal* signal);
  void execFSREMOVEREF(Signal* signal);
  void execTUP_ALLOCREQ(Signal* signal);
  void execTUP_DEALLOCREQ(Signal* signal);
  void execTUP_WRITELOG_REQ(Signal* signal);

  // Ordered index related
  void execBUILDINDXREQ(Signal* signal);
  void buildIndex(Signal* signal, Uint32 buildPtrI);
  void buildIndexReply(Signal* signal, const BuildIndexRec* buildRec);

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

//------------------------------------------------------------------
//------------------------------------------------------------------
  void execATTRINFO(Signal* signal);

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
                                  Uint32 length,
                                  Operationrec * const regOperPtr);

// *****************************************************************
// Setting up the environment for reads, inserts, updates and deletes.
// *****************************************************************
//------------------------------------------------------------------
//------------------------------------------------------------------
  int handleReadReq(Signal* signal,
                    Operationrec* const regOperPtr,
                    Tablerec* const regTabPtr,
                    Page* pagePtr);

//------------------------------------------------------------------
//------------------------------------------------------------------
  int handleUpdateReq(Signal* signal,
                      Operationrec* const regOperPtr,
                      Fragrecord* const regFragPtr,
                      Tablerec* const regTabPtr,
                      Page* const pagePtr);

//------------------------------------------------------------------
//------------------------------------------------------------------
  int handleInsertReq(Signal* signal,
                      Operationrec* const regOperPtr,
                      Fragrecord* const regFragPtr,
                      Tablerec* const regTabPtr,
                      Page* const pagePtr);

//------------------------------------------------------------------
//------------------------------------------------------------------
  int handleDeleteReq(Signal* signal,
                      Operationrec* const regOperPtr,
                      Fragrecord* const regFragPtr,
                      Tablerec* const regTabPtr,
                      Page* const pagePtr);

//------------------------------------------------------------------
//------------------------------------------------------------------
  int  updateStartLab(Signal* signal,
                      Operationrec* const regOperPtr,
                      Tablerec* const regTabPtr,
                      Page* const pagePtr);

// *****************************************************************
// Interpreter Handling methods.
// *****************************************************************

//------------------------------------------------------------------
//------------------------------------------------------------------
  int interpreterStartLab(Signal* signal,
                          Page* const pagePtr,
                          Uint32 TupHeadOffset);

//------------------------------------------------------------------
//------------------------------------------------------------------
  int interpreterNextLab(Signal* signal,
                         Page* const pagePtr,
                         Uint32 TupHeadOffset,
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
                        Uint32 TnoOfData,
                        const Operationrec * const regOperPtr);

//------------------------------------------------------------------
//------------------------------------------------------------------
  void sendLogAttrinfo(Signal* signal,
                       Uint32 TlogSize,
                       Operationrec * const regOperPtr);

//------------------------------------------------------------------
//------------------------------------------------------------------
  void sendTUPKEYCONF(Signal* signal, Operationrec * 
                      const regOperPtr, 
                      Uint32 TlogSize);

//------------------------------------------------------------------
//------------------------------------------------------------------
// *****************************************************************
// The methods that perform the actual read and update of attributes
// in the tuple.
// *****************************************************************
//------------------------------------------------------------------
//------------------------------------------------------------------
  int readAttributes(Page* const pagePtr,
                     Uint32   TupHeadOffset,
                     const Uint32*  inBuffer,
                     Uint32   inBufLen,
                     Uint32*  outBuffer,
                     Uint32   TmaxRead);

//------------------------------------------------------------------
//------------------------------------------------------------------
  int readAttributesWithoutHeader(Page* const pagePtr,
                                  Uint32   TupHeadOffset,
                                  Uint32*  inBuffer,
                                  Uint32   inBufLen,
                                  Uint32*  outBuffer,
                                  Uint32*  attrBuffer,
                                  Uint32   TmaxRead);

//------------------------------------------------------------------
//------------------------------------------------------------------
  int updateAttributes(Page* const pagePtr,
                       Uint32      TupHeadOffset,
                       Uint32*     inBuffer,
                       Uint32      inBufLen);

//------------------------------------------------------------------
//------------------------------------------------------------------
  bool readFixedSizeTHOneWordNotNULL(Uint32* outBuffer,
                                     AttributeHeader* ahOut,
                                     Uint32  attrDescriptor,
                                     Uint32  attrDes2);

//------------------------------------------------------------------
//------------------------------------------------------------------
  bool updateFixedSizeTHOneWordNotNULL(Uint32* inBuffer,
                                       Uint32  attrDescriptor,
                                       Uint32  attrDes2);

//------------------------------------------------------------------
//------------------------------------------------------------------
  bool readFixedSizeTHTwoWordNotNULL(Uint32* outBuffer,
                                     AttributeHeader* ahOut,
                                     Uint32  attrDescriptor,
                                     Uint32  attrDes2);

//------------------------------------------------------------------
//------------------------------------------------------------------
  bool updateFixedSizeTHTwoWordNotNULL(Uint32* inBuffer,
                                       Uint32  attrDescriptor,
                                       Uint32  attrDes2);

//------------------------------------------------------------------
//------------------------------------------------------------------
  bool readFixedSizeTHManyWordNotNULL(Uint32* outBuffer,
                                      AttributeHeader* ahOut,
                                      Uint32  attrDescriptor,
                                      Uint32  attrDes2);

//------------------------------------------------------------------
//------------------------------------------------------------------
  bool updateFixedSizeTHManyWordNotNULL(Uint32* inBuffer,
                                        Uint32  attrDescriptor,
                                        Uint32  attrDes2);

//------------------------------------------------------------------
//------------------------------------------------------------------
  bool readFixedSizeTHOneWordNULLable(Uint32* outBuffer,
                                      AttributeHeader* ahOut,
                                      Uint32  attrDescriptor,
                                      Uint32  attrDes2);

//------------------------------------------------------------------
//------------------------------------------------------------------
  bool updateFixedSizeTHOneWordNULLable(Uint32* inBuffer,
                                        Uint32  attrDescriptor,
                                        Uint32  attrDes2);

//------------------------------------------------------------------
//------------------------------------------------------------------
  bool readFixedSizeTHTwoWordNULLable(Uint32* outBuffer,
                                      AttributeHeader* ahOut,
                                      Uint32  attrDescriptor,
                                      Uint32  attrDes2);

//------------------------------------------------------------------
//------------------------------------------------------------------
  bool updateFixedSizeTHTwoWordNULLable(Uint32* inBuffer,
                                        Uint32  attrDescriptor,
                                        Uint32  attrDes2);

//------------------------------------------------------------------
//------------------------------------------------------------------
  bool readFixedSizeTHManyWordNULLable(Uint32* outBuffer,
                                       AttributeHeader* ahOut,
                                       Uint32  attrDescriptor,
                                       Uint32  attrDes2);

//------------------------------------------------------------------
//------------------------------------------------------------------
  bool readFixedSizeTHZeroWordNULLable(Uint32* outBuffer,
                                       AttributeHeader* ahOut,
                                       Uint32  attrDescriptor,
                                       Uint32  attrDes2);
//------------------------------------------------------------------
//------------------------------------------------------------------
  bool updateFixedSizeTHManyWordNULLable(Uint32* inBuffer,
                                         Uint32  attrDescriptor,
                                         Uint32  attrDes2);

//------------------------------------------------------------------
//------------------------------------------------------------------
  bool readVariableSizedAttr(Uint32* outBuffer,
                             AttributeHeader* ahOut,
                             Uint32  attrDescriptor,
                             Uint32  attrDes2);

//------------------------------------------------------------------
//------------------------------------------------------------------
  bool updateVariableSizedAttr(Uint32* inBuffer,
                               Uint32  attrDescriptor,
                               Uint32  attrDes2);

//------------------------------------------------------------------
//------------------------------------------------------------------
  bool readVarSizeUnlimitedNotNULL(Uint32* outBuffer,
                                   AttributeHeader* ahOut,
                                   Uint32  attrDescriptor,
                                   Uint32  attrDes2);

//------------------------------------------------------------------
//------------------------------------------------------------------
  bool updateVarSizeUnlimitedNotNULL(Uint32* inBuffer,
                                     Uint32  attrDescriptor,
                                     Uint32  attrDes2);

//------------------------------------------------------------------
//------------------------------------------------------------------
  bool readVarSizeUnlimitedNULLable(Uint32* outBuffer,
                                    AttributeHeader* ahOut,
                                    Uint32  attrDescriptor,
                                    Uint32  attrDes2);

//------------------------------------------------------------------
//------------------------------------------------------------------
  bool updateVarSizeUnlimitedNULLable(Uint32* inBuffer,
                                      Uint32  attrDescriptor,
                                      Uint32  attrDes2);

//------------------------------------------------------------------
//------------------------------------------------------------------
  bool readBigVarSizeNotNULL(Uint32* outBuffer,
                             AttributeHeader* ahOut,
                             Uint32  attrDescriptor,
                             Uint32  attrDes2);

//------------------------------------------------------------------
//------------------------------------------------------------------
  bool updateBigVarSizeNotNULL(Uint32* inBuffer,
                               Uint32  attrDescriptor,
                               Uint32  attrDes2);

//------------------------------------------------------------------
//------------------------------------------------------------------
  bool readBigVarSizeNULLable(Uint32* outBuffer,
                              AttributeHeader* ahOut,
                              Uint32  attrDescriptor,
                              Uint32  attrDes2);

//------------------------------------------------------------------
//------------------------------------------------------------------
  bool updateBigVarSizeNULLable(Uint32* inBuffer,
                                Uint32  attrDescriptor,
                                Uint32  attrDes2);

//------------------------------------------------------------------
//------------------------------------------------------------------
  bool readSmallVarSizeNotNULL(Uint32* outBuffer,
                               AttributeHeader* ahOut,
                               Uint32  attrDescriptor,
                               Uint32  attrDes2);

//------------------------------------------------------------------
//------------------------------------------------------------------
  bool updateSmallVarSizeNotNULL(Uint32* inBuffer,
                                 Uint32  attrDescriptor,
                                 Uint32  attrDes2);

//------------------------------------------------------------------
//------------------------------------------------------------------
  bool readSmallVarSizeNULLable(Uint32* outBuffer,
                                AttributeHeader* ahOut,
                                Uint32  attrDescriptor,
                                Uint32  attrDes2);

//------------------------------------------------------------------
//------------------------------------------------------------------
  bool updateSmallVarSizeNULLable(Uint32* inBuffer,
                                  Uint32  attrDescriptor,
                                  Uint32  attrDes2);

//------------------------------------------------------------------
//------------------------------------------------------------------
  bool readDynFixedSize(Uint32* outBuffer,
                        AttributeHeader* ahOut,
                        Uint32  attrDescriptor,
                        Uint32  attrDes2);

//------------------------------------------------------------------
//------------------------------------------------------------------
  bool updateDynFixedSize(Uint32* inBuffer,
                          Uint32  attrDescriptor,
                          Uint32  attrDes2);

//------------------------------------------------------------------
//------------------------------------------------------------------
  bool readDynVarSizeUnlimited(Uint32* outBuffer,
                               AttributeHeader* ahOut,
                               Uint32  attrDescriptor,
                               Uint32  attrDes2);

//------------------------------------------------------------------
//------------------------------------------------------------------
  bool updateDynVarSizeUnlimited(Uint32* inBuffer,
                                 Uint32  attrDescriptor,
                                 Uint32  attrDes2);

//------------------------------------------------------------------
//------------------------------------------------------------------
  bool readDynBigVarSize(Uint32* outBuffer,
                         AttributeHeader* ahOut,
                         Uint32  attrDescriptor,
                         Uint32  attrDes2);

//------------------------------------------------------------------
//------------------------------------------------------------------
  bool updateDynBigVarSize(Uint32* inBuffer,
                           Uint32  attrDescriptor,
                           Uint32  attrDes2);

//------------------------------------------------------------------
//------------------------------------------------------------------
  bool readDynSmallVarSize(Uint32* outBuffer,
                           AttributeHeader* ahOut,
                           Uint32  attrDescriptor,
                           Uint32  attrDes2);

//------------------------------------------------------------------
//------------------------------------------------------------------
  bool updateDynSmallVarSize(Uint32* inBuffer,
                             Uint32  attrDescriptor,
                             Uint32  attrDes2);

//------------------------------------------------------------------
//------------------------------------------------------------------
  bool nullFlagCheck(Uint32  attrDes2);
  Uint32 read_psuedo(Uint32 attrId, Uint32* outBuffer);

//------------------------------------------------------------------
//------------------------------------------------------------------
  void setUpQueryRoutines(Tablerec* const regTabPtr);

// *****************************************************************
// Service methods.
// *****************************************************************
//------------------------------------------------------------------
//------------------------------------------------------------------
  void copyAttrinfo(Signal* signal, Operationrec * const regOperPtr, Uint32*  inBuffer);

//------------------------------------------------------------------
//------------------------------------------------------------------
  void initOpConnection(Operationrec* const regOperPtr);

//------------------------------------------------------------------
//------------------------------------------------------------------
  void initOperationrec(Signal* signal);

//------------------------------------------------------------------
//------------------------------------------------------------------
  int initStoredOperationrec(Operationrec* const regOperPtr,
                             Uint32 storedId);

//------------------------------------------------------------------
//------------------------------------------------------------------
  void insertActiveOpList(Signal* signal, 
                          OperationrecPtr regOperPtr,
                          Page * const pagePtr,
                          Uint32 pageOffset);

//------------------------------------------------------------------
//------------------------------------------------------------------
  void linkOpIntoFragList(OperationrecPtr regOperPtr,
                          Fragrecord* const regFragPtr);

//------------------------------------------------------------------
//------------------------------------------------------------------
  void bufferTRANSID_AI(Signal* signal, BlockReference aRef, Uint32 Tlen);

//------------------------------------------------------------------
// Trigger handling routines
//------------------------------------------------------------------
  ArrayList<TupTriggerData>* findTriggerList(Tablerec* table,
                                             TriggerType::Value ttype,
                                             TriggerActionTime::Value ttime,
                                             TriggerEvent::Value tevent);

  bool createTrigger(Tablerec* table, const CreateTrigReq* req);

  Uint32 dropTrigger(Tablerec* table, const DropTrigReq* req);

  void checkImmediateTriggersAfterInsert(Signal* signal, 
                                         Operationrec* const regOperPtr, 
                                         Tablerec* const tablePtr);

  void checkImmediateTriggersAfterUpdate(Signal* signal, 
                                         Operationrec* const regOperPtr, 
                                         Tablerec* const tablePtr);

  void checkImmediateTriggersAfterDelete(Signal* signal, 
                                         Operationrec* const regOperPtr, 
                                         Tablerec* const tablePtr);

#if 0
  void checkDeferredTriggers(Signal* signal, 
                             Operationrec* const regOperPtr,
                             Tablerec* const regTablePtr);
#endif
  void checkDetachedTriggers(Signal* signal, 
                             Operationrec* const regOperPtr,
                             Tablerec* const regTablePtr);

  void fireImmediateTriggers(Signal* signal, 
                             ArrayList<TupTriggerData>& triggerList, 
                             Operationrec* const regOperPtr);

  void fireDeferredTriggers(Signal* signal, 
                            ArrayList<TupTriggerData>& triggerList,
                            Operationrec* const regOperPtr);

  void fireDetachedTriggers(Signal* signal,
                            ArrayList<TupTriggerData>& triggerList,
                            Operationrec* const regOperPtr);

  void executeTriggers(Signal* signal,
                       ArrayList<TupTriggerData>& triggerList,
                       Operationrec* const regOperPtr);

  void executeTrigger(Signal* signal, 
                      TupTriggerData* const trigPtr, 
                      Operationrec* const regOperPtr);

  bool readTriggerInfo(TupTriggerData* const trigPtr,
                       Operationrec* const regOperPtr, 
                       Uint32* const keyBuffer,
                       Uint32& noPrimKey,
                       Uint32* const mainBuffer,
                       Uint32& noMainWords,
                       Uint32* const copyBuffer,
                       Uint32& noCopyWords);

  void sendTrigAttrInfo(Signal*        signal, 
                        Uint32*        data, 
                        Uint32         dataLen,
                        bool           executeDirect,
                        BlockReference receiverReference);

  Uint32 setAttrIds(Bitmask<MAXNROFATTRIBUTESINWORDS>& attributeMask, 
                    Uint32 noOfAttributes, 
                    Uint32* inBuffer);

  void sendFireTrigOrd(Signal* signal, 
                       Operationrec * const regOperPtr, 
                       TupTriggerData* const trigPtr,
                       Uint32 noPrimKeySignals, 
                       Uint32 noBeforeSignals, 
                       Uint32 noAfterSignals);

  bool primaryKey(Tablerec* const, Uint32);

  // these set terrorCode and return non-zero on error

  int executeTuxInsertTriggers(Signal* signal, 
                                Operationrec* const regOperPtr,
                                Tablerec* const regTabPtr);

  int executeTuxUpdateTriggers(Signal* signal, 
                                Operationrec* const regOperPtr,
                                Tablerec* const regTabPtr);

  int executeTuxDeleteTriggers(Signal* signal, 
                                Operationrec* const regOperPtr,
                                Tablerec* const regTabPtr);

  // these crash the node on error

  void executeTuxCommitTriggers(Signal* signal, 
                                Operationrec* regOperPtr,
                                Tablerec* const regTabPtr);

  void executeTuxAbortTriggers(Signal* signal, 
                               Operationrec* regOperPtr,
                               Tablerec* const regTabPtr);

// *****************************************************************
// Error Handling routines.
// *****************************************************************
//------------------------------------------------------------------
//------------------------------------------------------------------
  int TUPKEY_abort(Signal* signal, int error_type);

//------------------------------------------------------------------
//------------------------------------------------------------------
  void tupkeyErrorLab(Signal* signal);

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
// we will set the delete state on the record that becomes the owner
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
  void checkPages(Fragrecord* const regFragPtr);
#endif
  void printoutTuplePage(Uint32 fragid, Uint32 pageid, Uint32 printLimit);

  bool checkUpdateOfPrimaryKey(Uint32* updateBuffer, Tablerec* const regTabPtr);

  void setNullBits(Page* const regPage, Tablerec* const regTabPtr, Uint32 pageOffset);
  bool checkNullAttributes(Operationrec* const, Tablerec* const);
  bool getPage(PagePtr& pagePtr,
               Operationrec* const regOperPtr,
               Fragrecord* const regFragPtr,
               Tablerec* const regTabPtr);

  bool getPageLastCommitted(Operationrec* const regOperPtr,
                            Operationrec* const leaderOpPtr);

  bool getPageThroughSavePoint(Operationrec* const regOperPtr,
                               Operationrec* const leaderOpPtr);

  Uint32 calculateChecksum(Page* const pagePtr, Uint32 tupHeadOffset, Uint32 tupHeadSize);
  void setChecksum(Page* const pagePtr, Uint32 tupHeadOffset, Uint32 tupHeadSize);

  void commitSimple(Signal* signal,
                    Operationrec* const regOperPtr,
                    Fragrecord* const regFragPtr,
                    Tablerec* const regTabPtr);

  void commitRecord(Signal* signal,
                    Operationrec* const regOperPtr,
                    Fragrecord* const regFragPtr,
                    Tablerec* const regTabPtr);

  void setTupleStatesSetOpType(Operationrec* const regOperPtr,
                               Page* const pagePtr,
                               Uint32& opType,
                               OperationrecPtr& firstOpPtr);

  void findBeforeValueOperation(OperationrecPtr& befOpPtr,
                                OperationrecPtr firstOpPtr);

  void calculateChangeMask(Page* const PagePtr,
                           Tablerec* const regTabPtr,
                           Uint32 pageOffset,
                           Bitmask<MAXNROFATTRIBUTESINWORDS>& attributeMask);

  void updateGcpId(Signal* signal,
                   Operationrec* const regOperPtr,
                   Fragrecord* const regFragPtr,
                   Tablerec* const regTabPtr);

  void abortUpdate(Signal* signal,
                   Operationrec*  const regOperPtr,
                   Fragrecord* const regFragPtr,
                   Tablerec* const regTabPtr);
  void commitUpdate(Signal* signal,
                    Operationrec*  const regOperPtr,
                    Fragrecord* const regFragPtr,
                    Tablerec* const regTabPtr);

  void setTupleStateOnPreviousOps(Uint32 prevOpIndex);
  void copyMem(Signal* signal, Uint32 sourceIndex, Uint32 destIndex);

  void freeAllAttrBuffers(Operationrec*  const regOperPtr);
  void freeAttrinbufrec(Uint32 anAttrBufRec);
  void removeActiveOpList(Operationrec*  const regOperPtr);

  void updatePackedList(Signal* signal, Uint16 ahostIndex);

  void setUpDescriptorReferences(Uint32 descriptorReference,
                                 Tablerec* const regTabPtr,
                                 const Uint32* offset);
  void setUpKeyArray(Tablerec* const regTabPtr);
  bool addfragtotab(Tablerec* const regTabPtr, Uint32 fragId, Uint32 fragIndex);
  void deleteFragTab(Tablerec* const regTabPtr, Uint32 fragId);
  void releaseTabDescr(Tablerec* const regTabPtr);
  void getFragmentrec(FragrecordPtr& regFragPtr, Uint32 fragId, Tablerec* const regTabPtr);

  void initialiseRecordsLab(Signal* signal, Uint32 switchData, Uint32, Uint32);
  void initializeAttrbufrec();
  void initializeCheckpointInfoRec();
  void initializeDiskBufferSegmentRecord();
  void initializeFragoperrec();
  void initializeFragrecord();
  void initializeHostBuffer();
  void initializeLocalLogInfo();
  void initializeOperationrec();
  void initializePendingFileOpenInfoRecord();
  void initializeRestartInfoRec();
  void initializeTablerec();
  void initializeTabDescr();
  void initializeUndoPage();

  void initTab(Tablerec* const regTabPtr);

  void startphase3Lab(Signal* signal, Uint32 config1, Uint32 config2);

  void fragrefuseLab(Signal* signal, FragoperrecPtr fragOperPtr);
  void fragrefuse1Lab(Signal* signal, FragoperrecPtr fragOperPtr);
  void fragrefuse2Lab(Signal* signal, FragoperrecPtr fragOperPtr, FragrecordPtr regFragPtr);
  void fragrefuse3Lab(Signal* signal,
                      FragoperrecPtr fragOperPtr,
                      FragrecordPtr regFragPtr,
                      Tablerec* const regTabPtr,
                      Uint32 fragId);
  void fragrefuse4Lab(Signal* signal,
                      FragoperrecPtr fragOperPtr,
                      FragrecordPtr regFragPtr,
                      Tablerec* const regTabPtr,
                      Uint32 fragId);
  void addattrrefuseLab(Signal* signal,
                        FragrecordPtr regFragPtr,
                        FragoperrecPtr fragOperPtr,
                        Tablerec* const regTabPtr,
                        Uint32 fragId);


  void checkLcpActiveBufferPage(Uint32 minPageNotWrittenInCheckpoint, DiskBufferSegmentInfoPtr dbsiPtr);
  void lcpWriteListDataPageSegment(Signal* signal,
                                   DiskBufferSegmentInfoPtr dbsiPtr,
                                   CheckpointInfoPtr ciPtr,
                                   bool flushFlag);
  void lcpFlushLogLab(Signal* signal, CheckpointInfoPtr ciPtr);
  void lcpClosedDataFileLab(Signal* signal, CheckpointInfoPtr ciPtr);
  void lcpEndconfLab(Signal* signal);
  void lcpSaveDataPageLab(Signal* signal, Uint32 ciIndex);
  void lcpCompletedLab(Signal* signal, Uint32 ciIndex);
  void lcpFlushRestartInfoLab(Signal* signal, Uint32 ciIndex);
  void lcpSaveCopyListLab(Signal* signal, CheckpointInfoPtr ciPtr);

  void sendFSREMOVEREQ(Signal* signal, TablerecPtr tabPtr);
  void releaseFragment(Signal* signal, Uint32 tableId);

  void allocDataBufferSegment(Signal* signal, DiskBufferSegmentInfoPtr& dbsiPtr);
  void allocRestartUndoBufferSegment(Signal* signal, DiskBufferSegmentInfoPtr& dbsiPtr, LocalLogInfoPtr lliPtr);
  void freeDiskBufferSegmentRecord(Signal* signal, DiskBufferSegmentInfoPtr dbsiPtr);
  void freeUndoBufferPages(Signal* signal, DiskBufferSegmentInfoPtr dbsiPtr);

  void releaseCheckpointInfoRecord(CheckpointInfoPtr ciPtr);
  void releaseDiskBufferSegmentRecord(DiskBufferSegmentInfoPtr dbsiPtr);
  void releaseFragoperrec(FragoperrecPtr fragOperPtr);
  void releaseFragrec(FragrecordPtr regFragPtr);
  void releasePendingFileOpenInfoRecord(PendingFileOpenInfoPtr pfoPtr);
  void releaseRestartInfoRecord(RestartInfoRecordPtr riPtr);

  void seizeDiskBufferSegmentRecord(DiskBufferSegmentInfoPtr& dbsiPtr);
  void seizeCheckpointInfoRecord(CheckpointInfoPtr& ciPtr);
  void seizeFragoperrec(FragoperrecPtr& fragOperPtr);
  void seizeFragrecord(FragrecordPtr& regFragPtr);
  void seizeOpRec(OperationrecPtr& regOperPtr);
  void seizePendingFileOpenInfoRecord(PendingFileOpenInfoPtr& pfoiPtr);
  void seizeRestartInfoRecord(RestartInfoRecordPtr& riPtr);

  // Initialisation
  void initData();
  void initRecords();

  void rfrClosedDataFileLab(Signal* signal, Uint32 restartIndex);
  void rfrCompletedLab(Signal* signal, RestartInfoRecordPtr riPtr);
  void rfrInitRestartInfoLab(Signal* signal, DiskBufferSegmentInfoPtr dbsiPtr);
  void rfrLoadDataPagesLab(Signal* signal, RestartInfoRecordPtr riPtr, DiskBufferSegmentInfoPtr dbsiPtr);
  void rfrReadFirstUndoSegment(Signal* signal, DiskBufferSegmentInfoPtr dbsiPtr, LocalLogInfoPtr lliPtr);
  void rfrReadNextDataSegment(Signal* signal, RestartInfoRecordPtr riPtr, DiskBufferSegmentInfoPtr dbsiPtr);
  void rfrReadNextUndoSegment(Signal* signal, DiskBufferSegmentInfoPtr dbsiPtr, LocalLogInfoPtr lliPtr);
  void rfrReadRestartInfoLab(Signal* signal, RestartInfoRecordPtr riPtr);
  void rfrReadSecondUndoLogLab(Signal* signal, DiskBufferSegmentInfoPtr dbsiPtr);

  void startExecUndoLogLab(Signal* signal, Uint32 lliIndex);
  void readExecUndoLogLab(Signal* signal, DiskBufferSegmentInfoPtr dbsiPtr, LocalLogInfoPtr lliPtr);
  void closeExecUndoLogLab(Signal* signal, LocalLogInfoPtr lliPtr);
  void endExecUndoLogLab(Signal* signal, Uint32 lliIndex);

  struct XlcStruct {
    Uint32 PageId;
    Uint32 PageIndex;
    Uint32 LogRecordType;
    Uint32 FragId;
    FragrecordPtr FragPtr;
    LocalLogInfoPtr LliPtr;
    DiskBufferSegmentInfoPtr DbsiPtr;
    UndoPagePtr UPPtr;
    TablerecPtr TabPtr;
  };

  void xlcGetNextRecordLab(Signal* signal, DiskBufferSegmentInfoPtr dbsiPtr, LocalLogInfoPtr lliPtr);
  void xlcRestartCompletedLab(Signal* signal);

  void xlcCopyData(XlcStruct& xlcStruct, Uint32 pageOffset, Uint32 noOfWords, PagePtr pagePtr);
  void xlcGetLogHeader(XlcStruct& xlcStruct);
  Uint32 xlcGetLogWord(XlcStruct& xlcStruct);

  void xlcAbortInsert(Signal* signal, XlcStruct& xlcStruct);
  void xlcAbortUpdate(Signal* signal, XlcStruct& xlcStruct);
  void xlcDeleteTh(XlcStruct& xlcStruct);
  void xlcIndicateNoOpActive(XlcStruct& xlcStruct);
  void xlcInsertTh(XlcStruct& xlcStruct);
  void xlcTableDescriptor(XlcStruct& xlcStruct);
  void xlcUndoLogPageHeader(XlcStruct& xlcStruct);
  void xlcUpdateTh(XlcStruct& xlcStruct);
  void xlcUpdateGCI(XlcStruct& xlcStruct);


  void cprAddData(Signal* signal,
                  Fragrecord* const regFragPtr,
                  Uint32 pageIndex,
                  Uint32 noOfWords,
                  Uint32 startOffset);
  void cprAddGCIUpdate(Signal* signal,
                       Uint32 prevGCI,
                       Fragrecord* const regFragPtr);
  void cprAddLogHeader(Signal* signal,
                       LocalLogInfo* const lliPtr,
                       Uint32 recordType,
                       Uint32 tableId,
                       Uint32 fragId);
  void cprAddUndoLogPageHeader(Signal* signal,
                               Page* const regPagePtr,
                               Fragrecord* const regFragPtr);
  void cprAddUndoLogRecord(Signal* signal,
                           Uint32 recordType,
                           Uint32 pageId,
                           Uint32 pageIndex,
                           Uint32 tableId,
                           Uint32 fragId,
                           Uint32 localLogIndex);
  void cprAddAbortUpdate(Signal* signal,
                         LocalLogInfo* const lliPtr,
                         Operationrec* const regOperPtr);
  void cprAddUndoLogWord(Signal* signal,
                         LocalLogInfo* const lliPtr,
                         Uint32 undoWord);
  bool isUndoLoggingNeeded(Fragrecord* const regFragPtr, Uint32 pageId);
  bool isUndoLoggingActive(Fragrecord* const regFragPtr);
  bool isUndoLoggingBlocked(Fragrecord* const regFragPtr);
  bool isPageUndoLogged(Fragrecord* const regFragPtr, Uint32 pageId);

  void seizeUndoBufferSegment(Signal* signal, UndoPagePtr& regUndoPagePtr);
  void lcpWriteUndoSegment(Signal* signal, LocalLogInfo* const lliPtr, bool flushFlag);


  void deleteScanProcedure(Signal* signal, Operationrec* regOperPtr);
  void copyProcedure(Signal* signal,
                     TablerecPtr regTabPtr,
                     Operationrec* regOperPtr);
  void scanProcedure(Signal* signal,
                     Operationrec* regOperPtr,
                     Uint32 lenAttrInfo);
  void storedSeizeAttrinbufrecErrorLab(Signal* signal,
                                       Operationrec* regOperPtr);
  bool storedProcedureAttrInfo(Signal* signal,
                               Operationrec* regOperPtr,
                               Uint32 length,
                               Uint32 firstWord,
                               bool copyProc);

//-----------------------------------------------------------------------------
// Table Descriptor Memory Manager
//-----------------------------------------------------------------------------

// Public methods
  Uint32 getTabDescrOffsets(const Tablerec* regTabPtr, Uint32* offset);
  Uint32 allocTabDescr(const Tablerec* regTabPtr, Uint32* offset);
  void freeTabDescr(Uint32 retRef, Uint32 retNo);
  Uint32 getTabDescrWord(Uint32 index);
  void setTabDescrWord(Uint32 index, Uint32 word);

// Private methods
  Uint32 sizeOfReadFunction();
  void   removeTdArea(Uint32 tabDesRef, Uint32 list);
  void   insertTdArea(Uint32 sizeOfChunk, Uint32 tabDesRef, Uint32 list);
  Uint32 itdaMergeTabDescr(Uint32 retRef, Uint32 retNo);

//------------------------------------------------------------------------------------------------------
// Page Memory Manager
//------------------------------------------------------------------------------------------------------

// Public methods
  void allocConsPages(Uint32 noOfPagesToAllocate,
                      Uint32& noOfPagesAllocated,
                      Uint32& allocPageRef);
  void returnCommonArea(Uint32 retPageRef, Uint32 retNo);
  void initializePage();

// Private methods
  void removeCommonArea(Uint32 remPageRef, Uint32 list);
  void insertCommonArea(Uint32 insPageRef, Uint32 list);
  void findFreeLeftNeighbours(Uint32& allocPageRef, Uint32& noPagesAllocated, Uint32 noPagesToAllocate);
  void findFreeRightNeighbours(Uint32& allocPageRef, Uint32& noPagesAllocated, Uint32 noPagesToAllocate);
  Uint32 nextHigherTwoLog(Uint32 input);

// Private data
  Uint32 cfreepageList[16];

//------------------------------------------------------------------------------------------------------
// Page Mapper, convert logical page id's to physical page id's
// The page mapper also handles the pages allocated to the fragment.
//------------------------------------------------------------------------------------------------------
//
// Public methods
  Uint32 getRealpid(Fragrecord* const regFragPtr, Uint32 logicalPageId);
  Uint32 getNoOfPages(Fragrecord* const regFragPtr);
  void initPageRangeSize(Uint32 size);
  bool insertPageRangeTab(Fragrecord* const regFragPtr,
                          Uint32 startPageId,
                          Uint32 noPages);
  void releaseFragPages(Fragrecord* const regFragPtr);
  void initFragRange(Fragrecord* const regFragPtr);
  void initializePageRange();
  Uint32 getEmptyPage(Fragrecord* const regFragPtr);
  Uint32 allocFragPages(Fragrecord* const regFragPtr, Uint32 noOfPagesAllocated);

// Private methods
  Uint32 leafPageRangeFull(Fragrecord* const regFragPtr, PageRangePtr currPageRangePtr);
  void releasePagerange(PageRangePtr regPRPtr);
  void seizePagerange(PageRangePtr& regPageRangePtr);
  void errorHandler(Uint32 errorCode);
  void allocMoreFragPages(Fragrecord* const regFragPtr);

// Private data
  Uint32 cfirstfreerange;
  PageRange *pageRange;
  Uint32 c_noOfFreePageRanges;
  Uint32 cnoOfPageRangeRec;

//------------------------------------------------------------------------------------------------------
// Fixed Allocator
// Allocates and deallocates tuples of fixed size on a fragment.
//------------------------------------------------------------------------------------------------------
//
// Public methods
  bool allocTh(Fragrecord* const regFragPtr,
               Tablerec* const regTabPtr,
               Uint32 pageType,
               Signal* signal,
               Uint32& pageOffset,
               PagePtr& pagePtr);

  void freeThSr(Tablerec*  const regTabPtr,
                Page*  const regPagePtr,
                Uint32 freePageOffset);

  void freeTh(Fragrecord*  const regFragPtr,
              Tablerec* const regTabPtr,
              Signal* signal,
              Page*  const regPagePtr,
              Uint32 freePageOffset);

  void getThAtPageSr(Page* const regPagePtr,
                     Uint32& pageOffset);

// Private methods
  void convertThPage(Uint32 Tupheadsize,
                     Page*  const regPagePtr);

  void getThAtPage(Fragrecord* const regFragPtr,
                   Page* const regPagePtr,
                   Signal* signal,
                   Uint32& pageOffset);

  void getEmptyPageThCopy(Fragrecord* const regFragPtr,
                          Signal* signal,
                          Page* const regPagePtr);

  void getEmptyPageTh(Fragrecord* const regFragPtr,
                      Signal* signal,
                      Page* const regPagePtr);

//------------------------------------------------------------------------------------------------------
// Temporary variables used for storing commonly used variables in certain modules
//------------------------------------------------------------------------------------------------------

  FragrecordPtr   fragptr;
  OperationrecPtr operPtr;
  TablerecPtr     tabptr;

// readAttributes and updateAttributes module
  Uint32          tCheckOffset;
  Uint32          tMaxRead;
  Uint32          tOutBufIndex;
  Uint32*         tTupleHeader;

// updateAttributes module
  Uint32          tInBufIndex;
  Uint32          tInBufLen;

  Uint32          terrorCode;

//------------------------------------------------------------------------------------------------------
// Common stored variables. Variables that have a valid value always.
//------------------------------------------------------------------------------------------------------
  Uint32 cnoOfLcpRec;
  Uint32 cnoOfParallellUndoFiles;
  Uint32 cnoOfUndoPage;

  Attrbufrec *attrbufrec;
  Uint32 cfirstfreeAttrbufrec;
  Uint32 cnoOfAttrbufrec;
  Uint32 cnoFreeAttrbufrec;

  CheckpointInfo *checkpointInfo;
  Uint32 cfirstfreeLcp;

  DiskBufferSegmentInfo *diskBufferSegmentInfo;
  Uint32 cfirstfreePdx;
  Uint32 cnoOfConcurrentWriteOp;

  Fragoperrec *fragoperrec;
  Uint32 cfirstfreeFragopr;
  Uint32 cnoOfFragoprec;

  Fragrecord *fragrecord;
  Uint32 cfirstfreefrag;
  Uint32 cnoOfFragrec;

  HostBuffer *hostBuffer;

  LocalLogInfo *localLogInfo;
  Uint32 cnoOfLocalLogInfo;

  Uint32 cfirstfreeOprec;
  Operationrec *operationrec;
  Uint32 cnoOfOprec;

  Page *page;
  Uint32 cnoOfPage;
  Uint32 cnoOfAllocatedPages;
  
  PendingFileOpenInfo *pendingFileOpenInfo;
  Uint32 cfirstfreePfo;
  Uint32 cnoOfConcurrentOpenOp;

  RestartInfoRecord *restartInfoRecord;
  Uint32 cfirstfreeSri;
  Uint32 cnoOfRestartInfoRec;

  Tablerec *tablerec;
  Uint32 cnoOfTablerec;

  TableDescriptor *tableDescriptor;
  Uint32 cnoOfTabDescrRec;

  UndoPage *undoPage;
  Uint32 cfirstfreeUndoSeg;
  Int32 cnoFreeUndoSeg;



  Uint32 cnoOfDataPagesToDiskWithoutSynch;

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
  Uint32 cundoFileVersion;
  BlockReference cownref;
  Uint32 cownNodeId;
  Uint32 czero;

 // A little bit bigger to cover overwrites in copy algorithms (16384 real size).
#define ZATTR_BUFFER_SIZE 16384
  Uint32 clogMemBuffer[ZATTR_BUFFER_SIZE + 16];
  Uint32 coutBuffer[ZATTR_BUFFER_SIZE + 16];
  Uint32 cinBuffer[ZATTR_BUFFER_SIZE + 16];
  Uint32 totNoOfPagesAllocated;

  // Trigger variables
  Uint32 c_maxTriggersPerTable;

  // Counters for num UNDO log records executed
  Uint32 cSrUndoRecords[9];

  STATIC_CONST(MAX_PARALLELL_TUP_SRREQ = 2); 
  Uint32 c_sr_free_page_0;

  Uint32 c_errorInsert4000TableId;

  void initGlobalTemporaryVars();
  void reportMemoryUsage(Signal* signal, int incDec);

  
#ifdef VM_TRACE
  struct Th {
    Uint32 data[1];
  };
  friend class NdbOut& operator<<(NdbOut&, const Operationrec&);
  friend class NdbOut& operator<<(NdbOut&, const Th&);
#endif
};

inline
bool Dbtup::isUndoLoggingNeeded(Fragrecord* const regFragPtr,
                                Uint32 pageId)
{
  if ((regFragPtr->checkpointVersion != RNIL) &&
      (pageId >= regFragPtr->minPageNotWrittenInCheckpoint) &&
      (pageId < regFragPtr->maxPageWrittenInCheckpoint)) {
    return true;
  }//if
  return false;
}//Dbtup::isUndoLoggingNeeded()

inline
bool Dbtup::isUndoLoggingActive(Fragrecord* const regFragPtr)
{
  if (regFragPtr->checkpointVersion != RNIL) {
    return true;
  }//if
  return false;
}//Dbtup::isUndoLoggingNeeded()

inline
bool Dbtup::isUndoLoggingBlocked(Fragrecord* const regFragPtr)
{
  if ((regFragPtr->checkpointVersion != RNIL) &&
      (cnoFreeUndoSeg < ZMIN_PAGE_LIMIT_TUPKEYREQ)) {
    return true;
  }//if
  return false;
}//Dbtup::isUndoLoggingNeeded()

inline
bool Dbtup::isPageUndoLogged(Fragrecord* const regFragPtr,
                             Uint32 pageId)
{
  if ((pageId >= regFragPtr->minPageNotWrittenInCheckpoint) &&
      (pageId < regFragPtr->maxPageWrittenInCheckpoint)) {
    return true;
  }//if
  return false;
}//Dbtup::isUndoLoggingNeeded()

#endif
