/*
   Copyright (c) 2005, 2024, Oracle and/or its affiliates.

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

#include "restore.hpp"
#include <AttributeHeader.hpp>
#include <KeyDescriptor.hpp>
#include <backup/Backup.hpp>
#include <dblqh/Dblqh.hpp>
#include <dbtup/Dbtup.hpp>
#include <md5_hash.hpp>
#include <signaldata/AttrInfo.hpp>
#include <signaldata/DictTabInfo.hpp>
#include <signaldata/DumpStateOrd.hpp>
#include <signaldata/FsCloseReq.hpp>
#include <signaldata/FsConf.hpp>
#include <signaldata/FsOpenReq.hpp>
#include <signaldata/FsReadWriteReq.hpp>
#include <signaldata/FsRef.hpp>
#include <signaldata/FsRemoveReq.hpp>
#include <signaldata/KeyInfo.hpp>
#include <signaldata/LqhKey.hpp>
#include <signaldata/RestoreImpl.hpp>

#include <NdbTick.h>
#include <EventLogger.hpp>

#define JAM_FILE_ID 453

#if (defined(VM_TRACE) || defined(ERROR_INSERT))
// #define DEBUG_START_RES 1
// #define DEBUG_RES 1
// #define DEBUG_RES_OPEN 1
// #define DEBUG_RES_PARTS 1
// #define DEBUG_RES_STAT 1
// #define DEBUG_RES_STAT_EXTRA 1
// #define DEBUG_RES_DEL 1
// #define DEBUG_HIGH_RES 1
#endif

#ifdef DEBUG_START_RES
#define DEB_START_RES(arglist)   \
  do {                           \
    g_eventLogger->info arglist; \
  } while (0)
#else
#define DEB_START_RES(arglist) \
  do {                         \
  } while (0)
#endif

#ifdef DEBUG_RES
#define DEB_RES(arglist)         \
  do {                           \
    g_eventLogger->info arglist; \
  } while (0)
#else
#define DEB_RES(arglist) \
  do {                   \
  } while (0)
#endif

#ifdef DEBUG_RES_OPEN
#define DEB_RES_OPEN(arglist)    \
  do {                           \
    g_eventLogger->info arglist; \
  } while (0)
#else
#define DEB_RES_OPEN(arglist) \
  do {                        \
  } while (0)
#endif

#ifdef DEBUG_RES_PARTS
#define DEB_RES_PARTS(arglist)   \
  do {                           \
    g_eventLogger->info arglist; \
  } while (0)
#else
#define DEB_RES_PARTS(arglist) \
  do {                         \
  } while (0)
#endif

#ifdef DEBUG_RES_STAT
#define DEB_RES_STAT(arglist)    \
  do {                           \
    g_eventLogger->info arglist; \
  } while (0)
#else
#define DEB_RES_STAT(arglist) \
  do {                        \
  } while (0)
#endif

#ifdef DEBUG_RES_STAT_EXTRA
#define DEB_RES_STAT_EXTRA(arglist) \
  do {                              \
    g_eventLogger->info arglist;    \
  } while (0)
#else
#define DEB_RES_STAT_EXTRA(arglist) \
  do {                              \
  } while (0)
#endif

#ifdef DEBUG_RES_DEL
#define DEB_RES_DEL(arglist)     \
  do {                           \
    g_eventLogger->info arglist; \
  } while (0)
#else
#define DEB_RES_DEL(arglist) \
  do {                       \
  } while (0)
#endif

#ifdef DEBUG_HIGH_RES
#define DEB_HIGH_RES(arglist)    \
  do {                           \
    g_eventLogger->info arglist; \
  } while (0)
#else
#define DEB_HIGH_RES(arglist) \
  do {                        \
  } while (0)
#endif

/**
 * Same error codes used by both DBLQH and DBTC.
 * See Dblqh.hpp and Dbtc.hpp.
 */
#define ZGET_DATAREC_ERROR 418
#define ZGET_ATTRINBUF_ERROR 419

#define PAGES LCP_RESTORE_BUFFER

Restore::Restore(Block_context &ctx, Uint32 instanceNumber, Uint32 blockNo)
    : SimulatedBlock(blockNo, ctx, instanceNumber),
      m_file_list(m_file_pool),
      m_file_hash(m_file_pool),
      m_rows_restored(0),
      m_millis_spent(0),
      m_frags_restored(0) {
  BLOCK_CONSTRUCTOR(Restore);

  // Add received signals
  if (blockNo == RESTORE) {
    addRecSignal(GSN_STTOR, &Restore::execSTTOR);
    addRecSignal(GSN_DUMP_STATE_ORD, &Restore::execDUMP_STATE_ORD);
    addRecSignal(GSN_CONTINUEB, &Restore::execCONTINUEB);
    addRecSignal(GSN_READ_CONFIG_REQ, &Restore::execREAD_CONFIG_REQ, true);

    addRecSignal(GSN_RESTORE_LCP_REQ, &Restore::execRESTORE_LCP_REQ);

    addRecSignal(GSN_FSOPENREF, &Restore::execFSOPENREF, true);
    addRecSignal(GSN_FSOPENCONF, &Restore::execFSOPENCONF);
    addRecSignal(GSN_FSREADREF, &Restore::execFSREADREF, true);
    addRecSignal(GSN_FSREADCONF, &Restore::execFSREADCONF);
    addRecSignal(GSN_FSCLOSEREF, &Restore::execFSCLOSEREF, true);
    addRecSignal(GSN_FSCLOSECONF, &Restore::execFSCLOSECONF);
    addRecSignal(GSN_FSREMOVEREF, &Restore::execFSREMOVEREF, true);
    addRecSignal(GSN_FSREMOVECONF, &Restore::execFSREMOVECONF);
    addRecSignal(GSN_FSWRITECONF, &Restore::execFSWRITECONF);

    addRecSignal(GSN_LQHKEYREF, &Restore::execLQHKEYREF);
    addRecSignal(GSN_LQHKEYCONF, &Restore::execLQHKEYCONF);
    m_is_query_block = false;
    m_lqh_block = DBLQH;
  } else {
    ndbrequire(blockNo == QRESTORE);
    m_is_query_block = true;
    m_lqh_block = DBQLQH;
    addRecSignal(GSN_STTOR, &Restore::execSTTOR);
    addRecSignal(GSN_DUMP_STATE_ORD, &Restore::execDUMP_STATE_ORD);
    addRecSignal(GSN_CONTINUEB, &Restore::execCONTINUEB);
    addRecSignal(GSN_READ_CONFIG_REQ, &Restore::execREAD_CONFIG_REQ, true);

    addRecSignal(GSN_RESTORE_LCP_REQ, &Restore::execRESTORE_LCP_REQ);
    addRecSignal(GSN_LQHKEYREF, &Restore::execLQHKEYREF);
    addRecSignal(GSN_LQHKEYCONF, &Restore::execLQHKEYCONF);

    addRecSignal(GSN_FSOPENREF, &Restore::execFSOPENREF, true);
    addRecSignal(GSN_FSOPENCONF, &Restore::execFSOPENCONF);
    addRecSignal(GSN_FSREADREF, &Restore::execFSREADREF, true);
    addRecSignal(GSN_FSREADCONF, &Restore::execFSREADCONF);
    addRecSignal(GSN_FSCLOSEREF, &Restore::execFSCLOSEREF, true);
    addRecSignal(GSN_FSCLOSECONF, &Restore::execFSCLOSECONF);
    addRecSignal(GSN_FSREMOVEREF, &Restore::execFSREMOVEREF, true);
    addRecSignal(GSN_FSREMOVECONF, &Restore::execFSREMOVECONF);
    addRecSignal(GSN_FSWRITECONF, &Restore::execFSWRITECONF);
  }
  ndbrequire(sizeof(Column) == 8);
}

Restore::~Restore() {}

BLOCK_FUNCTIONS(Restore)

void Restore::execSTTOR(Signal *signal) {
  jamEntry();

  if (m_is_query_block) {
    c_lqh = (Dblqh *)globalData.getBlock(DBQLQH, instance());
    c_tup = (Dbtup *)globalData.getBlock(DBQTUP, instance());
    c_backup = (Backup *)globalData.getBlock(QBACKUP, instance());
    ndbrequire(c_lqh != 0 && c_tup != 0 && c_backup != 0);
  } else {
    c_lqh = (Dblqh *)globalData.getBlock(DBLQH, instance());
    c_tup = (Dbtup *)globalData.getBlock(DBTUP, instance());
    c_backup = (Backup *)globalData.getBlock(BACKUP, instance());
    ndbrequire(c_lqh != 0 && c_tup != 0 && c_backup != 0);
  }
  sendSTTORRY(signal);
  return;
}  // Restore::execNDB_STTOR()

void Restore::execREAD_CONFIG_REQ(Signal *signal) {
  jamEntry();
  const ReadConfigReq *req = (ReadConfigReq *)signal->getDataPtr();
  Uint32 ref = req->senderRef;
  Uint32 senderData = req->senderData;
  ndbrequire(req->noOfParameters == 0);

  const ndb_mgm_configuration_iterator *p =
      m_ctx.m_config.getOwnConfigIterator();
  ndbrequire(p != 0);

  Uint32 encrypted_filesystem = 0;
  ndb_mgm_get_int_parameter(p, CFG_DB_ENCRYPTED_FILE_SYSTEM,
                            &encrypted_filesystem);
  c_encrypted_filesystem = encrypted_filesystem;

  m_file_pool.setSize(1);
  Uint32 cnt = 2 * MAX_ATTRIBUTES_IN_TABLE;
  cnt += PAGES;
  cnt += List::getSegmentSize() - 1;
  cnt /= List::getSegmentSize();
  cnt += 2;
  m_databuffer_pool.setSize(cnt);

  /**
   * Set up read and write buffer for LCP control files.
   * We use 1 buffer of 4k in size. So currently no
   * parallel reads or writes are supported.
   */
  NewVARIABLE *bat = allocateBat(1);
  bat[0].WA = &m_lcp_ctl_file_data[0][0];
  bat[0].nrr = 2 * (4 * BackupFormat::LCP_CTL_FILE_BUFFER_SIZE_IN_WORDS);

  ReadConfigConf *conf = (ReadConfigConf *)signal->getDataPtrSend();
  conf->senderRef = reference();
  conf->senderData = senderData;
  sendSignal(ref, GSN_READ_CONFIG_CONF, signal, ReadConfigConf::SignalLength,
             JBB);
}

void Restore::sendSTTORRY(Signal *signal) {
  signal->theData[0] = 0;
  signal->theData[3] = 1;
  signal->theData[4] = 255;  // No more start phases from missra
  BlockReference cntrRef = !isNdbMtLqh()      ? NDBCNTR_REF
                           : m_is_query_block ? QRESTORE_REF
                                              : RESTORE_REF;
  sendSignal(cntrRef, GSN_STTORRY, signal, 5, JBB);
}

void Restore::execCONTINUEB(Signal *signal) {
  jamEntry();

  switch (signal->theData[0]) {
    case RestoreContinueB::RESTORE_NEXT: {
      FilePtr file_ptr;
      ndbrequire(m_file_pool.getPtr(file_ptr, signal->theData[1]));
      restore_next(signal, file_ptr);
      return;
    }
    case RestoreContinueB::READ_FILE: {
      FilePtr file_ptr;
      ndbrequire(m_file_pool.getPtr(file_ptr, signal->theData[1]));
      read_data_file(signal, file_ptr);
      return;
    }
    case RestoreContinueB::CHECK_EXPAND_SHRINK: {
      FilePtr file_ptr;
      ndbrequire(m_file_pool.getPtr(file_ptr, signal->theData[1]));
      restore_lcp_conf(signal, file_ptr);
      return;
    }
    default:
      ndbabort();
  }
}

void Restore::execDUMP_STATE_ORD(Signal *signal) {
  jamEntry();

  if (signal->theData[0] == DumpStateOrd::RestoreRates) {
    jam();
    Uint64 rate =
        m_rows_restored * 1000 / (m_millis_spent == 0 ? 1 : m_millis_spent);
    const char *thread_type = "LDM";
    if (m_is_query_block) {
      thread_type = "Recover";
    }

    g_eventLogger->info(
        "%s instance %u: Restored LCP : %u fragments,"
        " %llu rows, "
        "%llu millis, %llu rows/s",
        thread_type, instance(), m_frags_restored, m_rows_restored,
        m_millis_spent, rate);
    infoEvent(
        "LDM instance %u: Restored LCP : %u fragments, %llu rows, "
        "%llu millis, %llu rows/s",
        instance(), m_frags_restored, m_rows_restored, m_millis_spent, rate);
  }
}

/**
 * MODULE: Restore LCP
 * -------------------
 * Restore LCP of a fragment
 * Starts by receiving RESTORE_LCP_REQ and later responding by RESTORE_LCP_CONF
 * from DBTUP when done.
 *
 * Here is a flow chart of what we perform here.
 * There are 5 main cases:
 * Case 1) Only valid LCP control file 0 exists
 * Case 2) Only valid LCP control file 1 exists
 *
 *    Perfectly normal cases and common cases. This LCP was completed
 *    and the previous one was both completed and removed from disk.
 *
 * Case 3) Both LCP control file 0 and 1 exists
 *
 *    This case is perfectly normal but unusual. It happens when
 *    we had a crash before completing the removal of the old
 *    LCP control file.
 *
 *    In this case we can either have two valid
 *    LCP control files or one valid and one invalid.
 *
 *    Invalid LCP control files can happen if a crash occurs after opening
 *    the LCP control file for a second LCP on a fragment and not
 *    completing it. It can also happen when the crash occurs in the
 *    middle of writing the LCP control file (should be extremely
 *    rare or even never happening).
 *
 * Case 4) No LCP control file exists (restore of 7.4 and older LCP).
 *
 * This is the normal case for an upgrade case.
 *
 * Case 5) Only LCP control file 0 exists, but it still is empty or contains
 *    invalid data. We could also have two invalid LCP control files here.
 *
 *    This case is also valid and can happen when we crash during running
 *    of the very first LCP on a fragment. It could also happen simply
 *    since we haven't done our first LCP on the fragment yet. In this
 *    case we should definitely have received lcpNo == ZNIL from LQH
 *    since DIH will not know of LCPs that we don't know about ourselves.
 *
 *    This case can also happen if we have 1 completed LCP control file
 *    which is not recoverable. In this case the node crashed just before
 *    completing the GCP that was necessary to make the LCP recoverable.
 *    Even DIH could know about this LCP but also knows to not try to use
 *    it. Either way DIH will send lcpNo equal to ZNIL.
 *
 * Variable descriptions:
 * ----------------------
 * m_ctl_file_no:
 * --------------
 * This represents the number of the CTL file currently being processed.
 * It is set to 0 when opening the first file and 1 when later opening
 * the second CTL file. It is initialised to Uint32(~0). When an empty
 * CTL file is created when no LCP is found it is set to 0.
 *
 * m_status:
 * ---------
 * This variable represents what we are currently doing.
 * It is a bitmap, so more than one state is possible at any time.
 *
 * Initial state is READ_CTL_FILES, this represents reading both CTL
 * files to discover the state of the LCP.
 *
 * FIRST_READ, FILE_THREAD_RUNNING, RESTORE_THREAD_RUNNING, FILE_EOF and
 * READING_RECORDS are states used when reading data files.
 * FIRST_READ is the initial state when starting to open the data file.
 * FILE_THREAD_RUNNING is an indication that a CONTINUEB thread is running
 * that reads the data file.
 * RESTORE_THREAD_RUNNING is an indication that a CONTINUEB thread is
 * running to restore using the data file.
 * READING_RECORDS is an indication that we are now reading records of the
 * data file.
 * FILE_EOF is an indication that the read of the data file is completed.
 * It is set when FILE_THREAD_RUNNING is reset.
 *
 * CREATE_CTL_FILE is a state used when creating a CTL file at times when
 * no LCP files was found.
 *
 * REMOVE_LCP_DATA_FILE is a state used when deleting data files after
 * reading the CTL files.
 * REMOVE_LCP_CTL_FILE is a state used when deleting a CTL file after
 * deleting data files.
 *
 * We start in state READ_CTL_FILES, after that we go CREATE_CTL_FILE
 * if no LCP files were found. If LCP files were found we move to
 * REMOVE_LCP_DATA_FILE if data files to delete was present, next we
 * move to REMOVE_LCP_CTL_FILE if necessary to remove a CTL file.
 *
 * Finally we move to restore using one or more data files. We restore
 * one file at a time using the state variables described above for
 * handling the data file.
 *
 * m_outstanding_reads:
 * --------------------
 * Used during read of data file to keep track of number of outstanding
 * FSREADREQ's.
 *
 * m_outstanding_operations:
 * -------------------------
 * It is used during remove files to keep track of number of outstanding
 * remove data files that are currently outstanding (we can delete multiple
 * files in parallel).
 * It is used during restore to keep track of number of outstanding
 * LQHKEYREQs.
 *
 * m_remove_ctl_file_no:
 * ---------------------
 * It is initialised to Uint32(~0). If set to this we won't delete any
 * CTL files.
 * When we find no CTL files we drop CTL file 0, we also drop all potential
 * data files from 0 to max file number.
 * If a CTL file that isn't restorable is found, then this file number is
 * set in this variable.
 * If we find that the other file is newer and restorable then we set this
 * variable to this file number.
 *
 * m_used_ctl_file_no:
 * -------------------
 * This variable is set to the CTL file we will use for restore. As soon as
 * we find a possible candidate it is set to the candidate, we might then
 * find that the other CTL file is an even better candidate and move the
 * variable to this number. As long as no CTL file have been found it
 * remains set to the initial value Uint32(~0).
 *
 * m_current_page_ptr_i:
 * ---------------------
 * Set to i-value of page we are currently restoring from. We allocate a set
 * of pages at start of restore and use those pages when reading from file
 * into those pages.
 *
 * m_current_page_pos:
 * -------------------
 * Indicates index position on the current page we are restoring.
 *
 * m_current_page_index:
 * ---------------------
 * Indicates which of the allocated pages we are currently restoring, used
 * to find the next page. The allocated pages are in an array. So getting
 * to the next page can be easily accomplished by adding one to this variable.
 * We use modulo page_count always when getting the page ptr, so this variable
 * can be constantly incremented.
 *
 * m_current_file_page:
 * --------------------
 * Used by read file process, keeps track of which page number was the last
 * one we issued a read on.
 *
 * m_bytes_left:
 * -------------
 * Incremented with number of bytes read from disk when FSREADCONF arrives.
 * Decremented by length of record when restoring from file.
 * Thus keeps track of number of bytes left already read from disk.
 *
 * m_rows_restored:
 * ----------------
 * Statistical variable, counts number of rows restored (counts LQHKEYCONF's
 * received). Used to display various stats about the restore.
 *
 * m_restore_start_time:
 * ---------------------
 * Current millisecond when restore starts. Used to print stats on restore
 * performance.
 *
 * m_restored_gcp_id:
 * ------------------
 * This variable keeps track of the GCI we are restoring, no LCP files that
 * have a newer GCP written can be used. This is either retrieved from
 * DIH sysfile or local sysfile (if recovering in a not restorable state).
 * Can be used for upgrade case where we use it to write a CTL file for
 * an existing LCP that had no CTL files.
 *
 * m_restored_lcp_id:
 * m_restored_local_lcp_id:
 * m_max_gci_completed:
 * m_max_gci_written:
 * m_max_page_cnt:
 * ------------------------
 * These five variables are set from the used CTL file. They are initialised
 * from the RESTORE_LCP_REQ to be used in the upgrade case. In the upgrade
 * case we will set MaxPageCnt to Uint32(~0).
 * m_restored_lcp_id and m_restored_local_lcp_id is the id of the LCP used
 * write the LCP.
 * m_max_page_cnt is the number of pages that we have ROW ids for in the file.
 * m_max_gci_written is the maximum GCI written in this LCP.
 * m_max_gci_completed is the maximum GCI completed when writing this LCP.
 * m_max_gci_completed can be bigger than m_max_gci_written.
 *
 * m_create_gci:
 * -------------
 * CreateGCI from RESTORE_LCP_REQ, not used.
 *
 * m_file_id:
 * ----------
 * File id as described in used CTL file. When multiple files are to be restored
 * it starts at first and then moves forward. Is between 0 and
 * BackupFormat::NDB_MAX_LCP_FILES - 1.
 *
 * m_max_parts:
 * ------------
 * Set from used CTL file. Set to 1 when performing upgrade variant.
 *
 * m_max_files:
 * ------------
 * Set from used CTL file, normally set to BackupFormat::NDB_MAX_LCP_FILES but
 * could be set differently when performing downgrade or upgrade. Indicates
 * maximum files that could be used, this is necessary to know what the file
 * name is of the next file.
 *
 * m_num_files:
 * ------------
 * Set from used CTL file. Set to number of files (also number of part pairs)
 * to restore in the LCP.
 *
 * m_current_file_index:
 * ---------------------
 * Number of file currently restored, starts at 0 and goes up to
 * m_num_files - 1 before we're done.
 *
 * m_dih_lcp_no:
 * -------------
 * In pre-7.6 this indicates data file number, in 7.6 it indicates rather
 * which CTL file number that DIH thinks should be restored. If this is set
 * to ZNIL then DIH knows of no LCPs written for this fragment. In this case
 * we don't really know anything about what we will find since we can even
 * have both CTL files restorable in this case if local LCPs was executed
 * as part of restart. However if it is set to 0 or 1, then we should not
 * be able to not find any files at all. So if we find no CTL file in this
 * it is an upgrade case.
 *
 * m_upgrade_case:
 * ---------------
 * Initialised to true, as soon as we find an CTL file whether correct or
 * not we know that it isn't an upgrade from pre-7.6 versions.
 *
 * m_double_lcps_found:
 * --------------------
 * Both CTL files found and both were found to be restorable.
 *
 * m_found_not_restorable:
 * -----------------------
 * We have found one CTL file that wasn't restorable if true.
 *
 * m_old_max_files:
 * ----------------
 * This is the max files read from CTL file NOT used. It is used to
 * delete LCP data from the old data files. It is possible that
 * the new and old CTL files have different max files in an upgrade
 * or downgrade situation.
 *
 * m_num_remove_data_files:
 * ------------------------
 * Number of data files to remove, calculated after finding new and old
 * CTL file. If only one CTL file is found then we cleaned up already during
 * execution of LCP, so no need to clean up. In this case it is set to 0.
 *
 * m_table_id, m_fragment_id, m_table_version:
 * -------------------------------------------
 * Triplet describing the partition we are restoring. m_table_id and
 * m_fragment_id came from RESTORE_LCP_REQ, m_table_version read from
 * data file.
 *
 * The flow chart for Case 1) is here:
 * -----------------------------------
 * Open LCP control 0 -> Success
 * Read LCP control 0 -> Success (read important data into File data)
 * Close LCP control 0 -> Success
 * Open LCP control 1 -> Fail
 * Start restore (starts through open_data_file call)
 *
 * The flow chart for Case 2) is here
 * -----------------------------------
 * Open LCP control 0 -> Fail
 * Open LCP control 1 -> Success
 * Read LCP control 1 -> Success (read important data into File data)
 * Close LCP control 1 -> Success
 * Start restore
 *
 * The flow chart for Case 3) is here
 * -----------------------------------
 * Open LCP control 0 -> Success
 * Read LCP control 0 -> Success (read important data into File data)
 * Close LCP control 0 -> Success
 * Open LCP control 1 -> Success
 * Read LCP control 1 -> Success (calculate which LCP control file to use)
 * Close LCP control 1 -> Success
 * Assume here X is the LCP control file NOT used (0 or 1)
 * Assume here Y is the file number of the file for the NOT used LCP
 * Remove data file Y -> Success
 * Remove control file X -> Success
 * Start restore
 *
 * The flow chart for Case 4) is here
 * ----------------------------------
 * Open LCP control 0 -> Fail
 * Open LCP control 1 -> Fail
 * Create LCP control 0 -> Success
 * Write LCP control 0 -> Success
 * Close LCP control 0 -> Success
 * if (lcpNo == ZNIL) then report Done
 * else
 * Remove not used data file
 * Start restore (this is a certain upgrade)
 *
 * The flow chart for Case 5) is here
 * ----------------------------------
 * Open LCP control 0 -> Success
 * Read LCP control 0 -> Success
 * We discover that the LCP control file is readable but not valid
 * Close LCP control 0 -> Success
 * Open LCP control 1 -> Fail
 * Create LCP control 0 -> Success
 * Write LCP control 0 -> Success
 * Close LCP control 0 -> Success
 * In this case lcpNo must be ZNIL since if there is a CTL file
 * but not completed then this LCP is written using Partial LCP
 * code.
 * ndbrequire(lcpNo == ZNIL) then report Done
 *
 * We will always with the following steps the read and close steps are
 * only needed when open is a success.
 *
 * Open LCP control 0
 * Read LCP control 0
 * Close LCP control 0
 * Open LCP control 1
 * Read LCP control 1
 * Close LCP control 1
 *
 * At this point we know which of the 5 cases we are.
 * 1) and 2) will simply start the restore
 * 4) and 5) will create LCP control file 0 and then conditionally restore
 * 3) needs to remove unneeded LCP control and data file before continuing
 *
 * In 7.5 after development of Partial LCPs the LCP files can be in the
 * following states.
 *
 * 1) No files at all
 *    This state happens immediately after the table has been created and
 *    the first LCP haven't been started yet.
 *    This state is covered by Case 4) above and is handled as if the table
 *    was created in 7.4 or earlier.
 *
 * 2) Two empty control files and possibly a not finished data file 0.
 *    This state happens after the first LCP has started, but not yet
 *    completed. We could also have only 1 invalid empty control file
 *    if the crash occurs in the middle of the start of the first LCP.
 *    In this case there could be a data file 0 which has been created
 *    but not yet completed.
 *    This is covered by state 5) above.
 *
 * 3) One valid LCP control file, in this case the only the data files
 *    present in the control file should exist. There could also be an
 *    invalid LCP control file here after the first LCP have been
 *    completed.
 *    This is Case 1) and 2) above.
 *
 * 4) Two valid control files. In this case all the data files present
 *    in any of the control files can be present. There could however
 *    be ones missing since we could be in the process of deleting an
 *    LCP after completion of an LCP.
 *    This is case 3) above.
 *
 * Execution of partial LCPs at restore
 * ------------------------------------
 * When we are restoring an LCP that consists of multiple data files this
 * is the algorithm used.
 * The LCP control file will cover either all parts or a subset of the parts.
 * We start with the case where it covers all parts.
 *
 * When all parts are covered we could have a case where there is overlap in
 * the parts. Let's use the following example.
 * Last part: All of part 801-35 (801-1023 and 0-35).
 * Last part - 1: All of part 554-800
 * Last part - 2: All of part 287-553
 * Last part - 3: All of part 18-286
 *
 * We need to execute all 4 of those parts (one data file per part). The file
 * number of the last part is given in the control file and also the maximum
 * file number is also given in the control file. This means that we can step
 * backwards and if we step backwards from file number 0 we will step to
 * file number MaxFileNumbers - 1.
 *
 * The above specifies which parts we have all changes for. There will also be
 * changes present for many other parts in the LCP data file. We will ignore
 * parts of those.
 *
 * We will start here with Last Part - 3. We will ignore everything for parts
 * 0-35 and 287-1023. We will insert all data pertaining to parts 36-286.
 * These changes should not contain any deleted rows as these should not be
 * recorded in parts where we record all rows.
 *
 * Next part to restore is Last part - 2. Here we will restore all of parts
 * 287-553. We will also install all changes related to parts 36-286. We
 * will ignore parts 0-35 and 554-1024.
 *
 * Next part to restore is Last part - 1. Here we will restore all of parts
 * 554-800 and all changes related to parts 36-553. We will ignore parts 0-35
 * and parts 801-1023.
 *
 * Finally we will restore Last part. Here we will restore all of parts 0-35
 * and parts 801-1023. We will also restore all changes of rows in parts
 * 36-800.
 *
 * Where we restore all parts we will use INSERT since those rows should not
 * be present yet. We will also reject the restore if we discover a DELETE row
 * in any of those parts.
 *
 * For parts where we restore changes we will use WRITE instead of INSERT since
 * the row might already exist. In addition we will accept DELETE rows by
 * row id.
 *
 * For parts that we ignore we will simply skip to next row.
 *
 * So we effectively divide rows in those parts into 3 separate categories.
 *
 * When we restore an LCP that was not restorable then we will exactly the
 * same scheme, the only difference is that we will only have some parts
 * that are restorable. So this LCP isn't usable in a system restart. It will
 * still be usable in a node restart however.
 */
void Restore::execFSREMOVEREF(Signal *signal) {
  jamEntry();
  FsRef *ref = (FsRef *)signal->getDataPtr();
  const Uint32 ptrI = ref->userPointer;
  FsConf *conf = (FsConf *)signal->getDataPtr();
  conf->userPointer = ptrI;
  execFSREMOVECONF(signal);
}

void Restore::execFSREMOVECONF(Signal *signal) {
  jamEntry();
  FsConf *conf = (FsConf *)signal->getDataPtr();
  FilePtr file_ptr;
  ndbrequire(m_file_pool.getPtr(file_ptr, conf->userPointer));
  lcp_remove_old_file_done(signal, file_ptr);
}

void Restore::execFSWRITECONF(Signal *signal) {
  jamEntry();
  FsConf *conf = (FsConf *)signal->getDataPtr();
  FilePtr file_ptr;
  ndbrequire(m_file_pool.getPtr(file_ptr, conf->userPointer));
  lcp_create_ctl_done_write(signal, file_ptr);
}

void Restore::lcp_create_ctl_open(Signal *signal, FilePtr file_ptr) {
  file_ptr.p->m_ctl_file_no = 0;
  file_ptr.p->m_status = File::CREATE_CTL_FILE;

  FsOpenReq *req = (FsOpenReq *)signal->getDataPtrSend();
  req->userReference = reference();
  req->fileFlags = FsOpenReq::OM_WRITEONLY | FsOpenReq::OM_CREATE;

  req->userPointer = file_ptr.i;

  FsOpenReq::setVersion(req->fileNumber, FsOpenReq::V_LCP);
  FsOpenReq::setSuffix(req->fileNumber, FsOpenReq::S_CTL);
  FsOpenReq::v5_setLcpNo(req->fileNumber, 0);
  FsOpenReq::v5_setTableId(req->fileNumber, file_ptr.p->m_table_id);
  FsOpenReq::v5_setFragmentId(req->fileNumber, file_ptr.p->m_fragment_id);

  req->page_size = 0;
  req->file_size_hi = UINT32_MAX;
  req->file_size_lo = UINT32_MAX;
  req->auto_sync_size = 0;

  sendSignal(NDBFS_REF, GSN_FSOPENREQ, signal, FsOpenReq::SignalLength, JBA);
}

void Restore::lcp_create_ctl_done_open(Signal *signal, FilePtr file_ptr) {
  struct BackupFormat::LCPCtlFile *lcpCtlFilePtr =
      (struct BackupFormat::LCPCtlFile *)&m_lcp_ctl_file_data[0][0];

  memcpy(lcpCtlFilePtr->fileHeader.Magic, BACKUP_MAGIC, 8);

  lcpCtlFilePtr->fileHeader.BackupVersion = NDBD_USE_PARTIAL_LCP_v2;
  const Uint32 sz = sizeof(BackupFormat::FileHeader) >> 2;
  lcpCtlFilePtr->fileHeader.SectionType = BackupFormat::FILE_HEADER;
  lcpCtlFilePtr->fileHeader.SectionLength = sz - 3;
  lcpCtlFilePtr->fileHeader.FileType = BackupFormat::LCP_CTL_FILE;
  lcpCtlFilePtr->fileHeader.BackupId = 0;
  lcpCtlFilePtr->fileHeader.BackupKey_0 = 0;
  lcpCtlFilePtr->fileHeader.BackupKey_1 = 0;
  lcpCtlFilePtr->fileHeader.ByteOrder = 0x12345678;
  lcpCtlFilePtr->fileHeader.NdbVersion = NDB_VERSION_D;
  lcpCtlFilePtr->fileHeader.MySQLVersion = NDB_MYSQL_VERSION_D;

  lcpCtlFilePtr->MaxPartPairs = BackupFormat::NDB_MAX_LCP_PARTS;
  lcpCtlFilePtr->MaxNumberDataFiles = BackupFormat::NDB_MAX_LCP_FILES;
  lcpCtlFilePtr->ValidFlag = 0;
  lcpCtlFilePtr->TableId = file_ptr.p->m_table_id;
  lcpCtlFilePtr->FragmentId = file_ptr.p->m_fragment_id;
  /**
   * There are a couple of possibilities here:
   * 1) DIH knows about the LCP, this is indicated by m_dih_lcp_no set to
   *    0 or 1. In this case if we come here it means we're doing the
   *    upgrade case and we can rely on that there is a correct data file
   *    and we take the opportunity to create a CTL file for this
   *    fragment here as well.
   *
   * 2) DIH knows about no data files, in this case there is no data file
   *    since by coming here we have concluded that we found no correct
   *    CTL file, so thus there is no data file both according to DIH
   *    and according to the non-presence of correct CTL files.
   */
  if (file_ptr.p->m_dih_lcp_no == ZNIL ||
      file_ptr.p->m_used_ctl_file_no == Uint32(~0)) {
    /**
     * We have no checkpointed data file yet, so we will write an initial
     * LCP control file. This could be either upgrade case or not.
     */
    jam();
    lcpCtlFilePtr->CreateGci = file_ptr.p->m_create_gci;
    lcpCtlFilePtr->MaxGciWritten = 0;
    lcpCtlFilePtr->MaxGciCompleted = 0;
    lcpCtlFilePtr->LastDataFileNumber = 0;
    lcpCtlFilePtr->LcpId = 0;
    lcpCtlFilePtr->LocalLcpId = 0;
    lcpCtlFilePtr->MaxPageCount = 0;
  } else {
    jam();
    /**
     * We have the upgrade case where DIH knows about a data file that there
     * is no CTL file defined for. We create a correct data file before
     * proceeding.
     * This is Case 4) above
     */
    ndbrequire(file_ptr.p->m_upgrade_case);
    ndbrequire(file_ptr.p->m_dih_lcp_no == 0 || file_ptr.p->m_dih_lcp_no == 1);
    lcpCtlFilePtr->ValidFlag = 1;
    lcpCtlFilePtr->CreateGci = file_ptr.p->m_create_gci;
    lcpCtlFilePtr->MaxGciWritten = file_ptr.p->m_restored_gcp_id;
    lcpCtlFilePtr->MaxGciCompleted = file_ptr.p->m_max_gci_completed;
    lcpCtlFilePtr->LastDataFileNumber = file_ptr.p->m_dih_lcp_no;
    lcpCtlFilePtr->LcpId = file_ptr.p->m_restored_lcp_id;
    lcpCtlFilePtr->LocalLcpId = 0;
    lcpCtlFilePtr->MaxPageCount = (~0);
  }
  struct BackupFormat::PartPair locPartPair;
  locPartPair.startPart = 0;
  locPartPair.numParts = BackupFormat::NDB_MAX_LCP_PARTS;
  lcpCtlFilePtr->partPairs[0] = locPartPair;
  lcpCtlFilePtr->NumPartPairs = 1;

  /**
   * Since the LCP control file will only contain 1 part we are
   * certain that we will fit in the small LCP control file size.
   */
  c_backup->convert_ctl_page_to_network(
      (Uint32 *)lcpCtlFilePtr, BackupFormat::NDB_LCP_CTL_FILE_SIZE_SMALL);
  FsReadWriteReq *req = (FsReadWriteReq *)signal->getDataPtrSend();

  req->userPointer = file_ptr.i;
  req->filePointer = file_ptr.p->m_fd;
  req->userReference = reference();
  req->varIndex = 0;
  req->numberOfPages = 1;
  req->operationFlag = 0;
  FsReadWriteReq::setFormatFlag(req->operationFlag,
                                FsReadWriteReq::fsFormatMemAddress);
  FsReadWriteReq::setSyncFlag(req->operationFlag, 1);

  /**
   * Data will be written from m_lcp_ctl_file_data as prepared by Bat */
  req->data.memoryAddress.memoryOffset = 0;
  req->data.memoryAddress.fileOffset = 0;
  req->data.memoryAddress.size = BackupFormat::NDB_LCP_CTL_FILE_SIZE_SMALL;

  sendSignal(NDBFS_REF, GSN_FSWRITEREQ, signal, FsReadWriteReq::FixedLength + 3,
             JBA);
}

void Restore::lcp_create_ctl_done_write(Signal *signal, FilePtr file_ptr) {
  close_file(signal, file_ptr);
}

void Restore::lcp_create_ctl_done_close(Signal *signal, FilePtr file_ptr) {
  if (file_ptr.p->m_dih_lcp_no == ZNIL ||
      file_ptr.p->m_used_ctl_file_no == Uint32(~0)) {
    /**
     * We have created an LCP control file, DIH knew not about any
     * recoverable LCP for this fragment. We have already removed
     * old LCP files not recoverable, so we're ready to move on
     * from here.
     */
    jam();
    /**
     * Done with Case 4) or 5) without upgrade case
     * --------------------------------------------
     * We are done, there was no data file to restore, but we have
     * created an LCP control file, so things should be fine now.
     * We fake start of restore and end of restore to signal back
     * the RESTORE_LCP_CONF and other reporting properly done.
     * We set LCP id and local LCP id to indicate to LQH that no
     * restorable LCP was found.
     */
    ndbrequire(file_ptr.p->m_outstanding_operations == 0);
    DEB_RES(("(%u)restore_lcp_conf", instance()));
    file_ptr.p->m_restored_lcp_id = 0;
    file_ptr.p->m_restored_local_lcp_id = 0;
    restore_lcp_conf(signal, file_ptr);
    return;
  } else if (file_ptr.p->m_dih_lcp_no == 0 || file_ptr.p->m_dih_lcp_no == 1) {
    /**
     * Case 4) Upgrade case
     * --------------------
     * We will clean away any old LCP data file that was not reported as
     * the one to restore. So if we will use 0 to restore we will
     * remove 1 and vice versa.
     */
    jam();
    ndbrequire(file_ptr.p->m_upgrade_case);
    file_ptr.p->m_status = File::CREATE_CTL_FILE;
    lcp_remove_old_file(signal, file_ptr, file_ptr.p->m_dih_lcp_no == 0 ? 1 : 0,
                        false);
    return;
  } else {
    ndbabort();
  }
}

void Restore::lcp_remove_old_file(Signal *signal, FilePtr file_ptr,
                                  Uint32 file_number, bool is_ctl_file) {
  file_ptr.p->m_outstanding_operations++;
  FsRemoveReq *req = (FsRemoveReq *)signal->getDataPtrSend();
  req->userReference = reference();
  req->userPointer = file_ptr.i;
  req->directory = 0;
  req->ownDirectory = 0;
  FsOpenReq::setVersion(req->fileNumber, FsOpenReq::V_LCP);
  if (is_ctl_file) {
    jam();
    FsOpenReq::setSuffix(req->fileNumber, FsOpenReq::S_CTL);
    DEB_RES(("(%u)tab(%u,%u) Delete control file number: %u", instance(),
             file_ptr.p->m_table_id, file_ptr.p->m_fragment_id, file_number));
  } else {
    jam();
    DEB_RES(("tab(%u,%u) Delete data file number: %u", file_ptr.p->m_table_id,
             file_ptr.p->m_fragment_id, file_number));
    FsOpenReq::setSuffix(req->fileNumber, FsOpenReq::S_DATA);
  }
  FsOpenReq::v5_setLcpNo(req->fileNumber, file_number);
  FsOpenReq::v5_setTableId(req->fileNumber, file_ptr.p->m_table_id);
  FsOpenReq::v5_setFragmentId(req->fileNumber, file_ptr.p->m_fragment_id);
  sendSignal(NDBFS_REF, GSN_FSREMOVEREQ, signal, FsRemoveReq::SignalLength,
             JBA);
}

void Restore::lcp_remove_old_file_done(Signal *signal, FilePtr file_ptr) {
  ndbrequire(file_ptr.p->m_outstanding_operations > 0);
  file_ptr.p->m_outstanding_operations--;
  if (file_ptr.p->m_outstanding_operations > 0) {
    jam();
    return;
  }
  switch (file_ptr.p->m_status) {
    case File::CREATE_CTL_FILE: {
      /**
       * END of UPGRADE PATH
       * -------------------
       * We are done creating a new LCP control file and removing
       * any half-written data files still lingering. It is the
       * normal path for case 4) for upgrades but could also happen
       * in case 5) where a crash occurred in an early phase of the
       * fragments lifetime.
       * Done with Case 4) and 5)
       * ------------------------
       * We are now ready to follow the normal path for restoring
       * a fragment. The information needed to complete the
       * restore is available now in the File object.
       */
      jam();
      DEB_RES(("(%u)start_restore_lcp_upgrade", instance()));
      start_restore_lcp_upgrade(signal, file_ptr);
      return;
    }
    case File::REMOVE_LCP_DATA_FILE: {
      jam();
      /**
       * Case 3) completed data file removal
       * -----------------------------------
       * We are starting up a normal restore, we found 2 LCP control files,
       * this is a normal condition, we will always remove any unneeded
       * LCP files as part of restore. We are now done with data file and
       * will continue with LCP control file.
       */
      DEB_RES(("(%u)Case 3 discovered after remove", instance()));
      ndbrequire(file_ptr.p->m_num_remove_data_files > 0);
      file_ptr.p->m_num_remove_data_files--;
      if (file_ptr.p->m_num_remove_data_files > 0) {
        jam();
        if (file_ptr.p->m_remove_data_file_no ==
            (file_ptr.p->m_old_max_files - 1)) {
          jam();
          file_ptr.p->m_remove_data_file_no = 0;
        } else {
          jam();
          file_ptr.p->m_remove_data_file_no++;
        }
        lcp_remove_old_file(signal, file_ptr, file_ptr.p->m_remove_data_file_no,
                            false);
      } else {
        jam();
        file_ptr.p->m_status = File::REMOVE_LCP_CTL_FILE;
        lcp_remove_old_file(signal, file_ptr, file_ptr.p->m_remove_ctl_file_no,
                            true);
      }
      return;
    }
    case File::REMOVE_LCP_CTL_FILE: {
      jam();
      /**
       * Case 3) is completed or Case 4 or Case 5) completed file removal
       * ----------------------------------------------------------------
       * Done with removal of both data file and control file of LCP
       * not used for restore. We are now ready to start restore for
       * Case 3, for Case 5 we will create an empty LCP control file
       * 0 first.
       */
      DEB_RES(("(%u)start_restore_lcp", instance()));
      if (file_ptr.p->m_used_ctl_file_no == Uint32(~0)) {
        jam();
        lcp_create_ctl_open(signal, file_ptr);
        return;
      }
      start_restore_lcp(signal, file_ptr);
      return;
    }
    default: {
      ndbabort();
      return;
    }
  }
}

void Restore::open_ctl_file(Signal *signal, FilePtr file_ptr, Uint32 lcp_no) {
  /* Keep track of which ctl file we're currently dealing with. */
  file_ptr.p->m_ctl_file_no = lcp_no;

  FsOpenReq *req = (FsOpenReq *)signal->getDataPtrSend();
  req->userReference = reference();
  req->fileFlags = FsOpenReq::OM_READONLY;
  req->userPointer = file_ptr.i;

  FsOpenReq::setVersion(req->fileNumber, FsOpenReq::V_LCP);
  FsOpenReq::setSuffix(req->fileNumber, FsOpenReq::S_CTL);
  FsOpenReq::v5_setLcpNo(req->fileNumber, lcp_no);
  FsOpenReq::v5_setTableId(req->fileNumber, file_ptr.p->m_table_id);
  FsOpenReq::v5_setFragmentId(req->fileNumber, file_ptr.p->m_fragment_id);

  req->page_size = 0;
  req->file_size_hi = UINT32_MAX;
  req->file_size_lo = UINT32_MAX;
  req->auto_sync_size = 0;

  sendSignal(NDBFS_REF, GSN_FSOPENREQ, signal, FsOpenReq::SignalLength, JBA);
}

void Restore::open_ctl_file_done_ref(Signal *signal, FilePtr file_ptr) {
  if (file_ptr.p->m_ctl_file_no == 1) {
    if (file_ptr.p->m_used_ctl_file_no == Uint32(~0)) {
      jam();
      /**
       * Case 4) discovered
       * ------------------
       * UPGRADE PATH when restoring an older MySQL Cluster version
       * ----------------------------------------------------------
       * We are done reading the LCP control files. If no one was found we will
       * assume that this is an LCP produced by an older version without LCP
       * control files.
       *
       * In the new format we always have a control file, even when there is
       * no LCP executed yet. We create this control file indicating an empty
       * set of LCP files before we continue restoring the data.
       *
       * We could come here also with a too new LCP completed and we create
       * an empty one also in this case since it will overwrite the old one.
       *
       * We could also come here when we have completed the LCP, but the LCP
       * control file is still invalid since we haven't ensured that the
       * LCP is safe yet by calling sync_lsn. In this case we can even have
       * a case where DIH thinks we have completed an LCP but we haven't
       * actually done so yet.
       */
      if (file_ptr.p->m_upgrade_case) {
        jam();
        DEB_RES(("(%u)Case 4 with upgrade discovered", instance()));
        lcp_create_ctl_open(signal, file_ptr);
      } else {
        jam();
        DEB_RES(("(%u)Case 4 without upgrade discovered", instance()));
        file_ptr.p->m_remove_ctl_file_no = 0;
        file_ptr.p->m_remove_data_file_no = 0;
        file_ptr.p->m_num_remove_data_files =
            BackupFormat::NDB_MAX_FILES_PER_LCP;
        file_ptr.p->m_status = File::REMOVE_LCP_DATA_FILE;
        lcp_remove_old_file(signal, file_ptr, file_ptr.p->m_remove_data_file_no,
                            false);
      }
      return;
    } else {
      /**
       * Case 1) discovered
       * ------------------
       * Normal behaviour, we had no LCP control file 1, but we had an LCP
       * control file 0, so we will use this to perform the restore. It is
       * already set up and ready to proceed with the restore. In this case
       * when there is only one LCP control file then we trust that there is
       * no LCP data files not needed. We always remove the data files of an
       * LCP before we remove the LCP control file of an LCP. So it is safe
       * to continue restoring now, we have 1 LCP control file and 1 set of
       * LCP data files that all are needed and described by the LCP control
       * file.
       */
      jam();
      DEB_RES(("(%u)Case 1 discovered", instance()));
      DEB_RES(
          ("(%u)Use ctl file: 0, 1 not exist, Lcp(%u,%u), GCI_C: %u,"
           " GCI_W: %u, MPC: %u",
           instance(), file_ptr.p->m_restored_lcp_id,
           file_ptr.p->m_restored_local_lcp_id, file_ptr.p->m_max_gci_completed,
           file_ptr.p->m_max_gci_written, file_ptr.p->m_max_page_cnt));
      ndbrequire(!file_ptr.p->m_found_not_restorable);
      start_restore_lcp(signal, file_ptr);
      return;
    }
  } else {
    jam();
    ndbrequire(file_ptr.p->m_ctl_file_no == 0);
    /**
     * We found no LCP control file 0, this can be normal, so we will now
     * instead open LCP control file 1.
     */
    DEB_RES(("(%u)open_ctl_file( 1 )", instance()));
    open_ctl_file(signal, file_ptr, 1);
    return;
  }
}

void Restore::calculate_remove_new_data_files(FilePtr file_ptr) {
  Uint32 new_ctl_no = file_ptr.p->m_remove_ctl_file_no;
  Uint32 old_ctl_no = new_ctl_no == 0 ? 1 : 0;

  ndbrequire(new_ctl_no < 2);
  BackupFormat::LCPCtlFile *oldLcpCtlFilePtr =
      (BackupFormat::LCPCtlFile *)&m_lcp_ctl_file_data[old_ctl_no][0];
  BackupFormat::LCPCtlFile *newLcpCtlFilePtr =
      (BackupFormat::LCPCtlFile *)&m_lcp_ctl_file_data[new_ctl_no][0];

  Uint32 old_last_file = oldLcpCtlFilePtr->LastDataFileNumber;
  Uint32 new_last_file = newLcpCtlFilePtr->LastDataFileNumber;

  Uint32 new_max_files = newLcpCtlFilePtr->MaxNumberDataFiles;
  Uint32 old_max_files = oldLcpCtlFilePtr->MaxNumberDataFiles;

  ndbrequire(new_max_files == old_max_files);
  ndbrequire(new_max_files == BackupFormat::NDB_MAX_LCP_FILES);

  /**
   * Calculate first file to remove.
   */
  Uint32 first_remove_file = new_last_file;
  Uint32 num_remove_files = 0;
  if (new_last_file == old_last_file) {
    /**
     * We could end up here after a number of unsuccessful restarts.
     * The LCP to remove was possibly changing the GCP written, but it
     * didn't contain any real changes to the data, so the same data
     * file was used again. We simply return and continue the restart.
     */
    jam();
    return;
  }
  while (1) {
    Uint32 next_remove_file = first_remove_file;
    num_remove_files++;
    if (next_remove_file == 0) {
      jam();
      next_remove_file = old_max_files - 1;
    } else {
      jam();
      next_remove_file--;
    }
    if (next_remove_file == old_last_file) {
      jam();
      break;
    }
    first_remove_file = next_remove_file;
  }
  ndbrequire(num_remove_files > 0);
  file_ptr.p->m_remove_data_file_no = first_remove_file;
  file_ptr.p->m_num_remove_data_files = num_remove_files;
  file_ptr.p->m_old_max_files = old_max_files;
}

void Restore::calculate_remove_old_data_files(FilePtr file_ptr) {
  Uint32 old_ctl_no = file_ptr.p->m_remove_ctl_file_no;
  Uint32 new_ctl_no = old_ctl_no == 0 ? 1 : 0;

  ndbrequire(old_ctl_no < 2);
  BackupFormat::LCPCtlFile *oldLcpCtlFilePtr =
      (BackupFormat::LCPCtlFile *)&m_lcp_ctl_file_data[old_ctl_no][0];
  BackupFormat::LCPCtlFile *newLcpCtlFilePtr =
      (BackupFormat::LCPCtlFile *)&m_lcp_ctl_file_data[new_ctl_no][0];

  Uint32 new_parts = newLcpCtlFilePtr->NumPartPairs;
  Uint32 old_parts = oldLcpCtlFilePtr->NumPartPairs;

  Uint32 old_last_file = oldLcpCtlFilePtr->LastDataFileNumber;
  Uint32 new_last_file = newLcpCtlFilePtr->LastDataFileNumber;

  Uint32 new_max_files = newLcpCtlFilePtr->MaxNumberDataFiles;
  Uint32 old_max_files = oldLcpCtlFilePtr->MaxNumberDataFiles;

  ndbrequire(new_max_files == old_max_files);
  ndbrequire(new_max_files == BackupFormat::NDB_MAX_LCP_FILES);
  ndbrequire(new_parts > 0);
  ndbrequire(old_parts > 0);
  /**
   * new_parts can never be bigger than old_parts + 1. This happens
   * when the LCP adds one more data file, but removes no data file
   * from the old LCPs. So when old_parts + 1 = new_parts then we
   * should remove 0 data files. When we have removed parts in new
   * LCP, then new_parts will be smaller and thus
   * old_parts + 1 - new_parts will be the number of parts to remove
   * from old LCP.
   */
  Uint32 new_files = 0;
  Uint32 loop_file = new_last_file;
  while (loop_file != old_last_file) {
    new_files++;
    if (loop_file == 0) {
      jam();
      loop_file = old_max_files - 1;
    } else {
      jam();
      loop_file--;
    }
  }
  /* new_files can be 0 in cases where new_parts == old_parts */
  ndbrequire(new_files != 0 || new_parts == old_parts);
  Uint32 remove_parts = (old_parts + new_files) - new_parts;
  file_ptr.p->m_num_remove_data_files = remove_parts;

  if (remove_parts == 0) {
    jam();
    return;
  }

  /**
   * Calculate first file to remove.
   */
  Uint32 first_remove_file = old_last_file;
  for (Uint32 i = 0; i < (old_parts - 1); i++) {
    if (first_remove_file == 0) {
      jam();
      first_remove_file = old_max_files - 1;
    } else {
      jam();
      first_remove_file--;
    }
  }
  file_ptr.p->m_remove_data_file_no = first_remove_file;
  file_ptr.p->m_old_max_files = old_max_files;
}

void Restore::open_ctl_file_done_conf(Signal *signal, FilePtr file_ptr) {
  file_ptr.p->m_upgrade_case = false;

  FsReadWriteReq *req = (FsReadWriteReq *)signal->getDataPtrSend();
  req->userPointer = file_ptr.i;
  req->filePointer = file_ptr.p->m_fd;
  req->userReference = reference();
  req->varIndex = 0;
  req->numberOfPages = 1;
  req->operationFlag = 0;
  FsReadWriteReq::setFormatFlag(req->operationFlag,
                                FsReadWriteReq::fsFormatMemAddress);
  FsReadWriteReq::setPartialReadFlag(req->operationFlag, 1);

  /**
   * Data will be written from m_lcp_ctl_file_data as prepared by Bat */
  req->data.memoryAddress.memoryOffset =
      file_ptr.p->m_ctl_file_no *
      (BackupFormat::LCP_CTL_FILE_BUFFER_SIZE_IN_WORDS * 4);
  req->data.memoryAddress.fileOffset = 0;
  req->data.memoryAddress.size = BackupFormat::NDB_LCP_CTL_FILE_SIZE_BIG;

  sendSignal(NDBFS_REF, GSN_FSREADREQ, signal, FsReadWriteReq::FixedLength + 3,
             JBA);
}

void Restore::read_ctl_file_done(Signal *signal, FilePtr file_ptr,
                                 Uint32 bytesRead) {
  /**
   * We read the LCP control file, we really want at this point to know
   * the following things.
   * 1) LCP id of this control file
   * 2) GCI completed, this makes it possible to shorten REDO log execution
   * 3) GCI written, if this is higher than the restored GCI than the LCP
   *    is not useful, in this case we should have an older LCP control file
   *    still there, otherwise the system is not restorable.
   * 4) Data file number to make sure we read the correct data file.
   *
   * The remainder of the information is used to verify that it is a correct
   * LCP control file and which version that have created it. We will only
   * go ahead if the LCP control is correct and we have the ability to
   * read it.
   *
   * We need to read both LCP control files, if one is missing then we use
   * the one we found. If both are present then we decide to use the newest
   * restorable LCP.
   * To handle case 3) we need to record which LCP control file we don't
   * use such that we can remove the LCP control file and LCP data file
   * belonging to this LCP which we will no longer use.
   *
   * When we come here the contents of the LCP control file is stored in
   * the m_lcp_ctl_file_data variable.
   */
  ndbrequire(file_ptr.p->m_ctl_file_no < 2);
  BackupFormat::LCPCtlFile *lcpCtlFilePtr =
      (BackupFormat::LCPCtlFile
           *)&m_lcp_ctl_file_data[file_ptr.p->m_ctl_file_no];

  if (bytesRead != BackupFormat::NDB_LCP_CTL_FILE_SIZE_SMALL &&
      bytesRead != BackupFormat::NDB_LCP_CTL_FILE_SIZE_BIG) {
    /**
     * Invalid file, probably still no data written. We will remove it
     * as we close it.
     */
    jam();
    ndbassert(bytesRead == 0);
    ndbrequire(!file_ptr.p->m_found_not_restorable);
    close_file(signal, file_ptr, true);
    return;
  }
  if (!c_backup->convert_ctl_page_to_host(lcpCtlFilePtr)) {
    /* Invalid file data */
    jam();
    ndbassert(false);
    ndbrequire(!file_ptr.p->m_found_not_restorable);
    close_file(signal, file_ptr, true);
    return;
  }
  if (lcpCtlFilePtr->MaxGciWritten == 0 &&
      lcpCtlFilePtr->MaxGciCompleted == 0 && lcpCtlFilePtr->ValidFlag == 0 &&
      lcpCtlFilePtr->LcpId == 0 && lcpCtlFilePtr->LocalLcpId == 0 &&
      lcpCtlFilePtr->LastDataFileNumber == 0 &&
      lcpCtlFilePtr->MaxPageCount == 0) {
    jam();
    g_eventLogger->debug(
        "Found empty LCP control file, "
        "must have been created by earlier restart,"
        " tab(%u,%u), CTL file: %u",
        file_ptr.p->m_table_id, file_ptr.p->m_fragment_id,
        file_ptr.p->m_ctl_file_no);

    /**
     * An empty initialised LCP control file was found, this must have
     * been created by previous restart attempt. We will ignore it and
     * act as if we didn't see the LCP control file at all.
     */
    ndbrequire(!file_ptr.p->m_found_not_restorable);
    close_file(signal, file_ptr, true);
    return;
  }

  const Uint32 sz = sizeof(BackupFormat::FileHeader) >> 2;
  if ((memcmp(BACKUP_MAGIC, lcpCtlFilePtr->fileHeader.Magic, 8) != 0) ||
      ((lcpCtlFilePtr->fileHeader.BackupVersion != NDBD_USE_PARTIAL_LCP_v1) &&
       (lcpCtlFilePtr->fileHeader.BackupVersion != NDBD_USE_PARTIAL_LCP_v2)) ||
      (lcpCtlFilePtr->fileHeader.SectionType != BackupFormat::FILE_HEADER) ||
      (lcpCtlFilePtr->fileHeader.SectionLength != (sz - 3)) ||
      (lcpCtlFilePtr->fileHeader.FileType != BackupFormat::LCP_CTL_FILE) ||
      (lcpCtlFilePtr->TableId != file_ptr.p->m_table_id) ||
      (lcpCtlFilePtr->FragmentId != file_ptr.p->m_fragment_id)) {
    jam();
    g_eventLogger->debug(
        "LCP Control file inconsistency, tab(%u,%u)"
        ", CTL file: %u",
        file_ptr.p->m_table_id, file_ptr.p->m_fragment_id,
        file_ptr.p->m_ctl_file_no);
    ndbrequire(!file_ptr.p->m_found_not_restorable);
    close_file(signal, file_ptr, true);
    return;
  }

  /**
   * Now we are ready to read the parts of the LCP control file that we need
   * to know to handle the restore correctly.
   */
  Uint32 validFlag = lcpCtlFilePtr->ValidFlag;
  Uint32 createGci = lcpCtlFilePtr->CreateGci;
  Uint32 maxGciCompleted = lcpCtlFilePtr->MaxGciCompleted;
  Uint32 maxGciWritten = lcpCtlFilePtr->MaxGciWritten;
  Uint32 lcpId = lcpCtlFilePtr->LcpId;
  Uint32 localLcpId = lcpCtlFilePtr->LocalLcpId;
  Uint32 maxPageCnt = lcpCtlFilePtr->MaxPageCount;
  Uint32 createTableVersion = lcpCtlFilePtr->CreateTableVersion;
  Uint32 lcpCtlVersion = lcpCtlFilePtr->fileHeader.BackupVersion;
  Uint64 rowCount = Uint64(lcpCtlFilePtr->RowCountLow) +
                    (Uint64(lcpCtlFilePtr->RowCountHigh) << 32);

  if (createTableVersion == 0) {
    jam();
    /**
     * LCP control file was created during table drop, simply set the valid flag
     * to 0 and ignore the LCP control file.
     */
    createTableVersion = c_lqh->getCreateSchemaVersion(file_ptr.p->m_table_id);
    validFlag = 0;
  }

  if (createTableVersion !=
      c_lqh->getCreateSchemaVersion(file_ptr.p->m_table_id)) {
    jam();
    g_eventLogger->debug(
        "(%u)Found LCP control file from old table"
        ", drop table haven't cleaned up properly"
        ", tab(%u,%u).%u (now %u), createGci:%u,"
        " maxGciCompleted: %u"
        ", maxGciWritten: %u, restored createGci: %u",
        instance(), file_ptr.p->m_table_id, file_ptr.p->m_fragment_id,
        createTableVersion,
        c_lqh->getCreateSchemaVersion(file_ptr.p->m_table_id), createGci,
        maxGciCompleted, maxGciWritten, file_ptr.p->m_create_gci);
    file_ptr.p->m_status = File::DROP_OLD_FILES;
    file_ptr.p->m_remove_ctl_file_no = file_ptr.p->m_ctl_file_no == 0 ? 1 : 0;
    file_ptr.p->m_remove_data_file_no = 0;
    file_ptr.p->m_num_remove_data_files = BackupFormat::NDB_MAX_FILES_PER_LCP;
    ndbrequire(file_ptr.p->m_used_ctl_file_no == ~Uint32(0));
    close_file(signal, file_ptr, true);
    return;
  } else if (maxGciWritten > file_ptr.p->m_restored_gcp_id ||
             maxGciCompleted > file_ptr.p->m_restored_gcp_id ||
             validFlag == 0) {
    jam();
    /**
     * This is a fairly normal case, but we will still log it to make sure we
     * have sufficient information logged if things turns for the worse. In a
     * normal restart we should at most have a few of those.
     *
     * The LCP contained records that were committed in GCI = maxGciWritten,
     * we are restoring a GCI which is smaller, this means that the LCP cannot
     * be used for restore since we have no UNDO log for main memory
     * data.
     *
     * This is a perfectly normal case although not so common. The LCP was
     * completed but had writes in it that rendered it useless. If this is
     * the very first LCP for this table it could even be that this is the
     * only LCP control file we have. But this can only happen for file 0.
     * If it happens for file 1 and we have no useful CTL file in file 0
     * then we are smoked since that is not supposed to be possible.
     *
     * It is also a normal case where we have written LCP control file
     * but not yet had time to sync the LSN for the LCP. This is flagged
     * by the validFlag not being set in the LCP control file.
     */
    g_eventLogger->debug(
        "(%u)LCP Control file ok, but not recoverable,"
        " tab(%u,%u), maxGciWritten: %u, restoredGcpId: %u"
        ", CTL file: %u, validFlag: %u",
        instance(), file_ptr.p->m_table_id, file_ptr.p->m_fragment_id,
        maxGciWritten, file_ptr.p->m_restored_gcp_id, file_ptr.p->m_ctl_file_no,
        validFlag);
    ndbrequire((file_ptr.p->m_ctl_file_no == 0 ||
                file_ptr.p->m_used_ctl_file_no != Uint32(~0)) ||
               validFlag == 0);
    ndbrequire(!file_ptr.p->m_found_not_restorable);
    file_ptr.p->m_found_not_restorable = true;
    file_ptr.p->m_remove_ctl_file_no = file_ptr.p->m_ctl_file_no;
    if (file_ptr.p->m_ctl_file_no == 1 &&
        file_ptr.p->m_used_ctl_file_no != Uint32(~0)) {
      jam();
      calculate_remove_new_data_files(file_ptr);
    }
  } else if (file_ptr.p->m_used_ctl_file_no == Uint32(~0)) {
    jam();
    /**
     * First LCP control file that we read, we simply set things up for
     * restore. We want the LCP id to check which LCP to use if there is
     * one more, also to report back to DBLQH.
     */
    file_ptr.p->m_max_gci_completed = maxGciCompleted;
    file_ptr.p->m_restored_lcp_id = lcpId;
    file_ptr.p->m_restored_local_lcp_id = localLcpId;
    file_ptr.p->m_max_page_cnt = maxPageCnt;
    file_ptr.p->m_max_gci_written = maxGciWritten;
    file_ptr.p->m_used_ctl_file_no = file_ptr.p->m_ctl_file_no;
    file_ptr.p->m_lcp_ctl_version = lcpCtlVersion;
    file_ptr.p->m_rows_in_lcp = rowCount;
    if (file_ptr.p->m_ctl_file_no == 1) {
      jam();
      DEB_RES(
          ("(%u)Use ctl file: 1, 0 not exist, Lcp(%u,%u), GCI_C: %u,"
           " GCI_W: %u, MPC: %u",
           instance(), file_ptr.p->m_restored_lcp_id,
           file_ptr.p->m_restored_local_lcp_id, file_ptr.p->m_max_gci_completed,
           file_ptr.p->m_max_gci_written, file_ptr.p->m_max_page_cnt));
    }
    if (file_ptr.p->m_found_not_restorable) {
      jam();
      calculate_remove_new_data_files(file_ptr);
    }
  } else if (file_ptr.p->m_restored_lcp_id > lcpId) {
    /**
     * This file is older than the previous one. We will use the previous
     * one.
     */
    jam();
    ndbrequire(file_ptr.p->m_ctl_file_no == 1);
    file_ptr.p->m_double_lcps_found = true;
    file_ptr.p->m_remove_ctl_file_no = 1;
    calculate_remove_old_data_files(file_ptr);
    DEB_RES(
        ("(%u)Use ctl file: 0, 1 older, Lcp(%u,%u), GCI_C: %u,"
         " GCI_W: %u, MPC: %u",
         instance(), file_ptr.p->m_restored_lcp_id,
         file_ptr.p->m_restored_local_lcp_id, file_ptr.p->m_max_gci_completed,
         file_ptr.p->m_max_gci_written, file_ptr.p->m_max_page_cnt));
  } else if (file_ptr.p->m_restored_lcp_id < lcpId ||
             (file_ptr.p->m_restored_lcp_id == lcpId &&
              file_ptr.p->m_restored_local_lcp_id < localLcpId)) {
    jam();
    DEB_RES(
        ("(%u)Use ctl file: 1, 0 older, Lcp(%u,%u), GCI_C: %u,"
         " GCI_W: %u, MPC: %u",
         instance(), lcpId, localLcpId, maxGciCompleted, maxGciWritten,
         maxPageCnt));
    ndbrequire(file_ptr.p->m_ctl_file_no == 1);
    ndbrequire(file_ptr.p->m_max_gci_completed <= maxGciCompleted);
    file_ptr.p->m_used_ctl_file_no = file_ptr.p->m_ctl_file_no;
    file_ptr.p->m_double_lcps_found = true;
    file_ptr.p->m_max_gci_completed = maxGciCompleted;
    file_ptr.p->m_max_gci_written = maxGciWritten;
    file_ptr.p->m_restored_lcp_id = lcpId;
    file_ptr.p->m_restored_local_lcp_id = localLcpId;
    file_ptr.p->m_max_page_cnt = maxPageCnt;
    file_ptr.p->m_remove_ctl_file_no = 0;
    file_ptr.p->m_lcp_ctl_version = lcpCtlVersion;
    file_ptr.p->m_rows_in_lcp = rowCount;
    calculate_remove_old_data_files(file_ptr);
  } else {
    /**
     * The LCP id of both LCPs were the same, this can happen when the
     * node previously crashed in the middle of an LCP and DIH haven't
     * finished it, so it starts the next LCP with the same ID.
     * In this case we have added one to the Local LCP id to ensure we
     * know which is the most recent one.
     * So here we come when CTL file 0 is newer.
     */
    DEB_RES(
        ("(%u)Use ctl file: 0, 1 older, Lcp(%u,%u), GCI_C: %u,"
         " GCI_W: %u, MPC: %u",
         instance(), file_ptr.p->m_restored_lcp_id,
         file_ptr.p->m_restored_local_lcp_id, file_ptr.p->m_max_gci_completed,
         file_ptr.p->m_max_gci_written, file_ptr.p->m_max_page_cnt));
    ndbrequire(file_ptr.p->m_ctl_file_no == 1);
    ndbrequire(file_ptr.p->m_max_gci_completed >= maxGciCompleted);
    file_ptr.p->m_used_ctl_file_no = 0;
    file_ptr.p->m_double_lcps_found = true;
    file_ptr.p->m_remove_ctl_file_no = 1;
    calculate_remove_old_data_files(file_ptr);
  }
  close_file(signal, file_ptr);
}

void Restore::lcp_drop_old_files(Signal *signal, FilePtr file_ptr) {
  file_ptr.p->m_status = File::REMOVE_LCP_DATA_FILE;
  lcp_remove_old_file(signal, file_ptr, file_ptr.p->m_remove_data_file_no,
                      false);
}

void Restore::close_ctl_file_done(Signal *signal, FilePtr file_ptr) {
  if (file_ptr.p->m_ctl_file_no == 0) {
    /**
     * We are done with LCP control file 0, continue with LCP control
     * file 1 in the same manner.
     */
    jam();
    open_ctl_file(signal, file_ptr, 1);
    return;
  } else {
    ndbrequire(file_ptr.p->m_ctl_file_no == 1);
    jam();
    if (file_ptr.p->m_used_ctl_file_no == Uint32(~0)) {
      /**
       * Case 5) discovered
       * No valid LCP file was found. We create an LCP control file 0
       * which is ok and then continue with the restore if there is
       * anything to restore.
       */
      jam();
      ndbrequire(file_ptr.p->m_dih_lcp_no == ZNIL);
      DEB_RES(("(%u)Case 5 discovered", instance()));
      file_ptr.p->m_remove_data_file_no = 0;
      file_ptr.p->m_num_remove_data_files = BackupFormat::NDB_MAX_FILES_PER_LCP;
      file_ptr.p->m_status = File::REMOVE_LCP_DATA_FILE;
      lcp_remove_old_file(signal, file_ptr, file_ptr.p->m_remove_data_file_no,
                          false);
      return;
    }
    if (file_ptr.p->m_double_lcps_found || file_ptr.p->m_found_not_restorable) {
      jam();
      /**
       * Case 3) discovered
       * ------------------
       * We start by removing potential data and CTL files still there.
       */
      DEB_RES(("(%u)Case 3 discovered after close", instance()));
      if (file_ptr.p->m_num_remove_data_files > 0) {
        jam();
        file_ptr.p->m_status = File::REMOVE_LCP_DATA_FILE;
        lcp_remove_old_file(signal, file_ptr, file_ptr.p->m_remove_data_file_no,
                            false);
      } else {
        file_ptr.p->m_status = File::REMOVE_LCP_CTL_FILE;
        lcp_remove_old_file(signal, file_ptr, file_ptr.p->m_remove_ctl_file_no,
                            true);
      }
      return;
    } else {
      jam();
      /**
       * Case 2) discovered
       * ------------------
       * LCP control file 1 existed alone, we are ready to execute the restore
       * now.
       */
      DEB_RES(("(%u)Case 2 discovered, start_restore_lcp", instance()));
      start_restore_lcp(signal, file_ptr);
      return;
    }
  }
}

void Restore::execRESTORE_LCP_REQ(Signal *signal) {
  jamEntry();

  Uint32 err = 0;
  RestoreLcpReq *req = (RestoreLcpReq *)signal->getDataPtr();
  Uint32 senderRef = req->senderRef;
  Uint32 senderData = req->senderData;
  if (m_is_query_block) {
    jam();
    /**
     * Redirect our reference to LQH for the restore of this fragment.
     * This LQH will not manipulate the table object while we are
     * restoring this fragment.
     * Same reasoning around TUP
     *
     * Since we will also trigger ordered index updates during restore
     * we will set up also DBTUX parts as is done in setting it up for
     * scan access.
     */
    Uint32 instance = refToInstance(senderRef);
    c_lqh->setup_query_thread_for_restore_access(instance, req->cnewestGci);
  }
  do {
    FilePtr file_ptr;
    if (!m_file_list.seizeFirst(file_ptr)) {
      jam();
      err = RestoreLcpRef::NoFileRecord;
      break;
    }

    if ((err = init_file(req, file_ptr))) {
      break;
    }

    signal->theData[0] = NDB_LE_StartReadLCP;
    signal->theData[1] = file_ptr.p->m_table_id;
    signal->theData[2] = file_ptr.p->m_fragment_id;
    sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 3, JBB);

    open_ctl_file(signal, file_ptr, 0);
    return;
  } while (0);

  c_lqh->reset_restore_thread_access();
  DEB_RES(("(%u)RESTORE_LCP_REF", instance()));
  RestoreLcpRef *ref = (RestoreLcpRef *)signal->getDataPtrSend();
  ref->senderData = senderData;
  ref->senderRef = reference();
  ref->errorCode = err;
  sendSignal(senderRef, GSN_RESTORE_LCP_REF, signal,
             RestoreLcpRef::SignalLength, JBB);
}

Uint32 Restore::init_file(const RestoreLcpReq *req, FilePtr file_ptr) {
  new (file_ptr.p) File();
  file_ptr.p->m_sender_ref = req->senderRef;
  file_ptr.p->m_sender_data = req->senderData;

  file_ptr.p->m_fd = RNIL;
  file_ptr.p->m_file_type = BackupFormat::LCP_FILE;
  file_ptr.p->m_status = File::READ_CTL_FILES;

  file_ptr.p->m_double_lcps_found = false;
  file_ptr.p->m_found_not_restorable = false;
  file_ptr.p->m_upgrade_case = true;
  file_ptr.p->m_remove_ctl_file_no = Uint32(~0);
  file_ptr.p->m_remove_data_file_no = Uint32(~0);
  file_ptr.p->m_num_remove_data_files = 0;
  file_ptr.p->m_old_max_files = Uint32(~0);

  file_ptr.p->m_dih_lcp_no = req->lcpNo;
  file_ptr.p->m_table_id = req->tableId;
  file_ptr.p->m_fragment_id = req->fragmentId;
  file_ptr.p->m_table_version = RNIL;
  file_ptr.p->m_restored_gcp_id = req->restoreGcpId;
  file_ptr.p->m_restored_lcp_id = req->lcpId;
  file_ptr.p->m_restored_local_lcp_id = 0;
  file_ptr.p->m_max_gci_completed = req->maxGciCompleted;
  file_ptr.p->m_create_gci = req->createGci;
  DEB_START_RES(
      ("(%u)RESTORE_LCP_REQ tab(%u,%u),"
       " GCI: %u, LCP id: %u, LCP no: %u, createGci: %u",
       instance(), req->tableId, req->fragmentId, req->restoreGcpId, req->lcpId,
       req->lcpNo, req->createGci));

  file_ptr.p->m_bytes_left = 0;  // Bytes read from FS
  file_ptr.p->m_current_page_ptr_i = RNIL;
  file_ptr.p->m_current_page_pos = 0;
  file_ptr.p->m_current_page_index = 0;
  file_ptr.p->m_current_file_page = 0;
  file_ptr.p->m_outstanding_reads = 0;
  file_ptr.p->m_outstanding_operations = 0;

  file_ptr.p->m_rows_in_lcp = 0;
  file_ptr.p->m_rows_restored = 0;
  file_ptr.p->m_rows_restored_insert = 0;
  file_ptr.p->m_rows_restored_delete = 0;
  file_ptr.p->m_rows_restored_delete_failed = 0;
  file_ptr.p->m_rows_restored_delete_page = 0;
  file_ptr.p->m_rows_restored_write = 0;
  file_ptr.p->m_ignored_rows = 0;
  file_ptr.p->m_row_operations = 0;

  file_ptr.p->m_file_id = Uint32(~0);
  file_ptr.p->m_ctl_file_no = Uint32(~0);
  file_ptr.p->m_used_ctl_file_no = Uint32(~0);
  file_ptr.p->m_current_file_index = 0;
  file_ptr.p->m_num_files = 0;
  file_ptr.p->m_max_parts = BackupFormat::NDB_MAX_LCP_PARTS;
  file_ptr.p->m_max_files = BackupFormat::NDB_MAX_LCP_FILES;
  file_ptr.p->m_restore_start_time = NdbTick_CurrentMillisecond();
  Uint32 err = seize_file(file_ptr);
  return err;
}

Uint32 Restore::seize_file(FilePtr file_ptr) {
  LocalList pages(m_databuffer_pool, file_ptr.p->m_pages);

  ndbassert(pages.isEmpty());
  pages.release();

  Uint32 buf_size = PAGES * GLOBAL_PAGE_SIZE;
  Uint32 page_count = (buf_size + GLOBAL_PAGE_SIZE - 1) / GLOBAL_PAGE_SIZE;
  if (!pages.seize(page_count)) {
    jam();
    return RestoreLcpRef::OutOfDataBuffer;
  }

  List::Iterator it;
  for (pages.first(it); !it.isNull(); pages.next(it)) {
    *it.data = RNIL;
  }

  Uint32 err = 0;
  for (pages.first(it); !it.isNull(); pages.next(it)) {
    Ptr<GlobalPage> page_ptr;
    if (!m_global_page_pool.seize(page_ptr)) {
      jam();
      err = RestoreLcpRef::OutOfReadBufferPages;
      break;
    }
    *it.data = page_ptr.i;
  }

  if (err) {
    for (pages.first(it); !it.isNull(); pages.next(it)) {
      if (*it.data == RNIL) break;
      m_global_page_pool.release(*it.data);
    }
  } else {
    pages.first(it);
    file_ptr.p->m_current_page_ptr_i = *it.data;
  }
  return err;
}

void Restore::release_file(FilePtr file_ptr, bool statistics) {
  LocalList pages(m_databuffer_pool, file_ptr.p->m_pages);

  List::Iterator it;
  for (pages.first(it); !it.isNull(); pages.next(it)) {
    if (*it.data == RNIL) {
      jam();
      continue;
    }
    m_global_page_pool.release(*it.data);
  }

  if (statistics) {
    Uint64 millis =
        NdbTick_CurrentMillisecond() - file_ptr.p->m_restore_start_time;
    if (millis == 0) millis = 1;
    Uint64 rows_per_sec =
        (file_ptr.p->m_row_operations * Uint64(1000)) / millis;

    g_eventLogger->info(
        "LDM instance %u: Restored T%dF%u LCP %llu rows, "
        "%llu row operations, "
        "%llu millis, %llu row operations/sec)",
        instance(), file_ptr.p->m_table_id, file_ptr.p->m_fragment_id,
        file_ptr.p->m_rows_restored, file_ptr.p->m_row_operations, millis,
        rows_per_sec);

    m_millis_spent += millis;
    m_rows_restored += file_ptr.p->m_rows_restored;
    m_frags_restored++;

    DEB_RES_STAT((
        "(%u)Restore tab(%u,%u): file_index: %u"
        ", inserts: %llu, writes: %llu"
        ", deletes: %llu, delete_pages: %llu"
        ", delete_failed: %llu"
        ", ignored rows: %llu",
        instance(), file_ptr.p->m_table_id, file_ptr.p->m_fragment_id,
        file_ptr.p->m_current_file_index - 1,
        file_ptr.p->m_rows_restored_insert, file_ptr.p->m_rows_restored_write,
        file_ptr.p->m_rows_restored_delete,
        file_ptr.p->m_rows_restored_delete_page,
        file_ptr.p->m_rows_restored_delete_failed, file_ptr.p->m_ignored_rows));
  } else {
    DEB_RES_STAT_EXTRA((
        "(%u)Restore tab(%u,%u): file_index: %u"
        ", inserts: %llu, writes: %llu"
        ", deletes: %llu, delete_pages: %llu"
        ", delete_failed: %llu"
        ", ignored rows: %llu",
        instance(), file_ptr.p->m_table_id, file_ptr.p->m_fragment_id,
        file_ptr.p->m_current_file_index - 1,
        file_ptr.p->m_rows_restored_insert, file_ptr.p->m_rows_restored_write,
        file_ptr.p->m_rows_restored_delete,
        file_ptr.p->m_rows_restored_delete_page,
        file_ptr.p->m_rows_restored_delete_failed, file_ptr.p->m_ignored_rows));
  }

  pages.release();
  if (statistics) {
    jam();
    m_file_list.release(file_ptr);
  }
}

void Restore::prepare_parts_for_execution(Signal *signal, FilePtr file_ptr) {
  ndbrequire(file_ptr.p->m_used_ctl_file_no < 2);
  BackupFormat::LCPCtlFile *lcpCtlFilePtr =
      (BackupFormat::LCPCtlFile
           *)&m_lcp_ctl_file_data[file_ptr.p->m_used_ctl_file_no][0];

  if (file_ptr.p->m_max_parts == 1 && file_ptr.p->m_num_files == 1) {
    /**
     * UPGRADE CASE, everything is in one file.
     */
    jam();
    file_ptr.p->m_part_state[0] = File::PART_ALL_ROWS;
    return;
  }
  /**
   * We set up the part state array in 3 steps.
   * The default state is that all parts receives all changes.
   *
   * For the current file index we have recorded in the LCP control file
   * all the parts where all rows exists, so these parts will all have the
   * state PART_ALL_ROWS.
   *
   * Lastly we will go backwards from the last LCP data file to restore and
   * set all parts that will be fully restored in this LCP data file to be
   * ignored by earlier LCP data files.
   *
   * We ensure that we have consistent data by ensuring that we don't have
   * any files set to PART_IGNORED that was in the array to receive all rows.
   */
  for (Uint32 i = 0; i < file_ptr.p->m_max_parts; i++) {
    file_ptr.p->m_part_state[i] = File::PART_ALL_CHANGES;
  }

  {
    struct BackupFormat::PartPair partPair =
        lcpCtlFilePtr->partPairs[file_ptr.p->m_current_file_index];

    DEB_RES_PARTS(("(%u)Prepare ALL parts[%u] = (%u,%u)", instance(),
                   file_ptr.p->m_current_file_index, partPair.startPart,
                   partPair.numParts));

    Uint32 part_id = partPair.startPart;
    for (Uint32 i = 0; i < partPair.numParts; i++) {
      file_ptr.p->m_part_state[part_id] = File::PART_ALL_ROWS;
      part_id++;
      if (part_id == file_ptr.p->m_max_parts) part_id = 0;
    }
  }

  for (Uint32 i = file_ptr.p->m_current_file_index + 1;
       i < lcpCtlFilePtr->NumPartPairs; i++) {
    jam();
    struct BackupFormat::PartPair partPair = lcpCtlFilePtr->partPairs[i];

    DEB_RES_PARTS(("(%u)Prepare IGNORE parts[%u] = (%u,%u)", instance(), i,
                   partPair.startPart, partPair.numParts));

    Uint32 part_id = partPair.startPart;
    for (Uint32 j = 0; j < partPair.numParts; j++) {
      ndbrequire(file_ptr.p->m_part_state[part_id] == File::PART_ALL_CHANGES);
      file_ptr.p->m_part_state[part_id] = File::PART_IGNORED;
      part_id++;
      if (part_id == file_ptr.p->m_max_parts) part_id = 0;
    }
  }
}

void Restore::start_restore_lcp_upgrade(Signal *signal, FilePtr file_ptr) {
  /**
   * In this an LCP existed, but no valid LCP control file, this can
   * only occur if the LCP was written by older versions of MySQL
   * Cluster.
   */
  file_ptr.p->m_current_file_index = 0;
  file_ptr.p->m_num_files = 1;
  file_ptr.p->m_max_parts = 1;
  file_ptr.p->m_max_files = 1;
  file_ptr.p->m_file_id = file_ptr.p->m_dih_lcp_no;
  open_data_file(signal, file_ptr);
}

void Restore::step_file_number_back(FilePtr file_ptr, Uint32 steps) {
  for (Uint32 i = 0; i < steps; i++) {
    if (file_ptr.p->m_file_id == 0) {
      jam();
      file_ptr.p->m_file_id = file_ptr.p->m_max_files - 1;
    } else {
      jam();
      file_ptr.p->m_file_id--;
    }
  }
}

void Restore::step_file_number_forward(FilePtr file_ptr) {
  file_ptr.p->m_file_id++;
  if (file_ptr.p->m_file_id == file_ptr.p->m_max_files) {
    jam();
    file_ptr.p->m_file_id = 0;
  }
}

void Restore::start_restore_lcp(Signal *signal, FilePtr file_ptr) {
  ndbrequire(file_ptr.p->m_used_ctl_file_no < 2);
  BackupFormat::LCPCtlFile *lcpCtlFilePtr =
      (BackupFormat::LCPCtlFile
           *)&m_lcp_ctl_file_data[file_ptr.p->m_used_ctl_file_no][0];

  /**
   * Initialise a few variables before starting the first data file
   * restore.
   */
  file_ptr.p->m_current_file_index = 0;
  file_ptr.p->m_num_files = lcpCtlFilePtr->NumPartPairs;
  file_ptr.p->m_max_parts = lcpCtlFilePtr->MaxPartPairs;
  file_ptr.p->m_max_files = lcpCtlFilePtr->MaxNumberDataFiles;
  file_ptr.p->m_file_id = lcpCtlFilePtr->LastDataFileNumber;
  file_ptr.p->m_table_version = lcpCtlFilePtr->CreateTableVersion;
  DEB_RES_OPEN(("(%u) tab(%u,%u), num_files: %u, last_file: %u", instance(),
                file_ptr.p->m_table_id, file_ptr.p->m_fragment_id,
                file_ptr.p->m_num_files, file_ptr.p->m_file_id));
  ndbrequire(file_ptr.p->m_num_files > 0);
  ndbrequire(file_ptr.p->m_num_files <= BackupFormat::NDB_MAX_LCP_PARTS);
  ndbrequire(file_ptr.p->m_file_id <= BackupFormat::NDB_MAX_LCP_FILES);
  step_file_number_back(file_ptr, file_ptr.p->m_num_files - 1);
  open_data_file(signal, file_ptr);
}

void Restore::open_data_file(Signal *signal, FilePtr file_ptr) {
  prepare_parts_for_execution(signal, file_ptr);
  file_ptr.p->m_status = File::FIRST_READ;

  FsOpenReq *req = (FsOpenReq *)signal->getDataPtrSend();
  req->userReference = reference();
  req->fileFlags =
      FsOpenReq::OM_READONLY | FsOpenReq::OM_READ_FORWARD | FsOpenReq::OM_GZ;
  req->userPointer = file_ptr.i;

  if (c_encrypted_filesystem) {
    jam();
    req->fileFlags |= FsOpenReq::OM_ENCRYPT_XTS;
  }

  DEB_RES_OPEN(("(%u)tab(%u,%u) open_data_file data file number = %u",
                instance(), file_ptr.p->m_table_id, file_ptr.p->m_fragment_id,
                file_ptr.p->m_file_id));

  FsOpenReq::setVersion(req->fileNumber, FsOpenReq::V_LCP);
  FsOpenReq::setSuffix(req->fileNumber, FsOpenReq::S_DATA);
  FsOpenReq::v5_setLcpNo(req->fileNumber, file_ptr.p->m_file_id);
  FsOpenReq::v5_setTableId(req->fileNumber, file_ptr.p->m_table_id);
  FsOpenReq::v5_setFragmentId(req->fileNumber, file_ptr.p->m_fragment_id);

  req->page_size = 0;
  req->file_size_hi = UINT32_MAX;
  req->file_size_lo = UINT32_MAX;
  req->auto_sync_size = 0;

  if (req->fileFlags & FsOpenReq::OM_ENCRYPT_CIPHER_MASK) {
    LinearSectionPtr lsptr[3];

    // Use a dummy file name
    ndbrequire(FsOpenReq::getVersion(req->fileNumber) != FsOpenReq::V_FILENAME);
    lsptr[FsOpenReq::FILENAME].p = nullptr;
    lsptr[FsOpenReq::FILENAME].sz = 0;

    req->fileFlags |= FsOpenReq::OM_ENCRYPT_KEY;

    EncryptionKeyMaterial nmk;
    nmk.length = globalData.nodeMasterKeyLength;
    memcpy(&nmk.data, globalData.nodeMasterKey, globalData.nodeMasterKeyLength);
    lsptr[FsOpenReq::ENCRYPT_KEY_MATERIAL].p = (const Uint32 *)&nmk;
    lsptr[FsOpenReq::ENCRYPT_KEY_MATERIAL].sz = nmk.get_needed_words();

    sendSignal(NDBFS_REF, GSN_FSOPENREQ, signal, FsOpenReq::SignalLength, JBA,
               lsptr, 2);
  } else {
    sendSignal(NDBFS_REF, GSN_FSOPENREQ, signal, FsOpenReq::SignalLength, JBA);
  }
}

void Restore::execFSOPENREF(Signal *signal) {
  FsRef *ref = (FsRef *)signal->getDataPtr();
  FilePtr file_ptr;
  jamEntry();
  ndbrequire(m_file_pool.getPtr(file_ptr, ref->userPointer));

  if (file_ptr.p->m_status == File::READ_CTL_FILES) {
    jam();
    open_ctl_file_done_ref(signal, file_ptr);
    return;
  } else if (file_ptr.p->m_status == File::CREATE_CTL_FILE) {
    ndbabort();
  }
  ndbrequire(file_ptr.p->m_status == File::FIRST_READ);

  Uint32 errCode = ref->errorCode;
  Uint32 osError = ref->osErrorCode;

  c_lqh->reset_restore_thread_access();
  RestoreLcpRef *rep = (RestoreLcpRef *)signal->getDataPtrSend();
  rep->senderData = file_ptr.p->m_sender_data;
  rep->errorCode = errCode;
  rep->extra[0] = osError;
  sendSignal(file_ptr.p->m_sender_ref, GSN_RESTORE_LCP_REF, signal,
             RestoreLcpRef::SignalLength + 1, JBB);
  release_file(file_ptr, true);
}

void Restore::execFSOPENCONF(Signal *signal) {
  jamEntry();
  FilePtr file_ptr;
  FsConf *conf = (FsConf *)signal->getDataPtr();
  ndbrequire(m_file_pool.getPtr(file_ptr, conf->userPointer));

  file_ptr.p->m_fd = conf->filePointer;

  if (file_ptr.p->m_status == File::READ_CTL_FILES) {
    jam();
    open_ctl_file_done_conf(signal, file_ptr);
    return;
  } else if (file_ptr.p->m_status == File::CREATE_CTL_FILE) {
    jam();
    lcp_create_ctl_done_open(signal, file_ptr);
    return;
  }
  ndbrequire(file_ptr.p->m_status == File::FIRST_READ);

  /**
   * Start thread's
   */

  ndbrequire((file_ptr.p->m_status & File::FILE_THREAD_RUNNING) == 0);
  ndbrequire((file_ptr.p->m_status & File::RESTORE_THREAD_RUNNING) == 0);
  file_ptr.p->m_status |= File::FILE_THREAD_RUNNING;
  signal->theData[0] = RestoreContinueB::READ_FILE;
  signal->theData[1] = file_ptr.i;
  sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);

  file_ptr.p->m_status |= File::RESTORE_THREAD_RUNNING;
  signal->theData[0] = RestoreContinueB::RESTORE_NEXT;
  signal->theData[1] = file_ptr.i;
  sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
}

void Restore::restore_next(Signal *signal, FilePtr file_ptr) {
  Uint32 *data, len = 0;
  Uint32 status = file_ptr.p->m_status;
  Uint32 page_count = file_ptr.p->m_pages.getSize();
  BackupFormat::RecordType header_type = BackupFormat::INSERT_TYPE;
  do {
    Uint32 left = file_ptr.p->m_bytes_left;
    if (left < 8) {
      jam();
      /**
       * Not enough bytes to read header
       */
      break;
    }
    Ptr<GlobalPage> page_ptr(0, 0), next_page_ptr(0, 0);
    ndbrequire(
        m_global_page_pool.getPtr(page_ptr, file_ptr.p->m_current_page_ptr_i));
    List::Iterator it;

    Uint32 pos = file_ptr.p->m_current_page_pos;
    if (status & File::READING_RECORDS) {
      jam();
      /**
       * We are reading records
       */
      len = ntohl(*(page_ptr.p->data + pos)) + 1;
      Uint32 type = len >> 16;
      len &= 0xFFFF;
      ndbrequire(len < GLOBAL_PAGE_SIZE_WORDS);
      ndbrequire(header_type < BackupFormat::END_TYPE);
      header_type = (BackupFormat::RecordType)type;
    } else {
      jam();
      /**
       * Section length is in 2 word
       */
      if (pos + 1 == GLOBAL_PAGE_SIZE_WORDS) {
        jam();
        /**
         * But that's stored on next page...
         *   and since we have at least 8 bytes left in buffer
         *   we can be sure that that's in buffer
         */
        LocalList pages(m_databuffer_pool, file_ptr.p->m_pages);
        Uint32 next_page = file_ptr.p->m_current_page_index + 1;
        pages.position(it, next_page % page_count);
        ndbrequire(m_global_page_pool.getPtr(next_page_ptr, *it.data));
        len = ntohl(*next_page_ptr.p->data);
      } else {
        jam();
        len = ntohl(*(page_ptr.p->data + pos + 1));
      }
    }

    if (file_ptr.p->m_status & File::FIRST_READ) {
      jam();
      len = 3;
      file_ptr.p->m_status &= ~(Uint32)File::FIRST_READ;
    }

    if (4 * len > left) {
      jam();

      /**
       * Not enough bytes to read "record"
       */
      if (unlikely((status & File::FILE_THREAD_RUNNING) == 0)) {
        crash_during_restore(file_ptr, __LINE__, 0);
      }
      len = 0;
      break;
    }

    /**
     * Entire record is in buffer
     */

    if (pos + len >= GLOBAL_PAGE_SIZE_WORDS) {
      jam();
      /**
       * But it's split over pages
       */
      if (next_page_ptr.p == 0) {
        LocalList pages(m_databuffer_pool, file_ptr.p->m_pages);
        Uint32 next_page = file_ptr.p->m_current_page_index + 1;
        pages.position(it, next_page % page_count);
        ndbrequire(m_global_page_pool.getPtr(next_page_ptr, *it.data));
      }
      file_ptr.p->m_current_page_ptr_i = next_page_ptr.i;
      file_ptr.p->m_current_page_pos = (pos + len) - GLOBAL_PAGE_SIZE_WORDS;
      file_ptr.p->m_current_page_index =
          (file_ptr.p->m_current_page_index + 1) % page_count;

      if (len <= GLOBAL_PAGE_SIZE_WORDS) {
        jam();
        Uint32 first = (GLOBAL_PAGE_SIZE_WORDS - pos);
        // wl4391_todo removing valgrind overlap warning for now
        memmove(page_ptr.p, page_ptr.p->data + pos, 4 * first);
        memcpy(page_ptr.p->data + first, next_page_ptr.p, 4 * (len - first));
        data = page_ptr.p->data;
      } else {
        jam();
        /**
         * A table definition can be larger than one page...
         * when that happens copy it out to side buffer
         *
         * First copy part belonging to page_ptr
         * Then copy full middle pages (moving forward in page-list)
         * Last copy last part
         */
        Uint32 save = len;
        assert(len <= NDB_ARRAY_SIZE(m_table_buf));
        Uint32 *dst = m_table_buf;

        /**
         * First
         */
        Uint32 first = (GLOBAL_PAGE_SIZE_WORDS - pos);
        memcpy(dst, page_ptr.p->data + pos, 4 * first);
        len -= first;
        dst += first;

        /**
         * Middle
         */
        while (len > GLOBAL_PAGE_SIZE_WORDS) {
          jam();
          memcpy(dst, next_page_ptr.p, 4 * GLOBAL_PAGE_SIZE_WORDS);
          len -= GLOBAL_PAGE_SIZE_WORDS;
          dst += GLOBAL_PAGE_SIZE_WORDS;

          {
            LocalList pages(m_databuffer_pool, file_ptr.p->m_pages);
            Uint32 next_page =
                (file_ptr.p->m_current_page_index + 1) % page_count;
            pages.position(it, next_page % page_count);
            ndbrequire(m_global_page_pool.getPtr(next_page_ptr, *it.data));

            file_ptr.p->m_current_page_ptr_i = next_page_ptr.i;
            file_ptr.p->m_current_page_index = next_page;
          }
        }

        /**
         * last
         */
        memcpy(dst, next_page_ptr.p, 4 * len);
        file_ptr.p->m_current_page_pos = len;

        /**
         * Set pointer and len
         */
        len = save;
        data = m_table_buf;
      }
    } else {
      file_ptr.p->m_current_page_pos = pos + len;
      data = page_ptr.p->data + pos;
    }

    file_ptr.p->m_bytes_left -= 4 * len;

    if (status & File::READING_RECORDS) {
      if (len == 1) {
        file_ptr.p->m_status = status & ~(Uint32)File::READING_RECORDS;
      } else {
        parse_record(signal, file_ptr, data, len, header_type);
      }
    } else {
      switch (ntohl(*data)) {
        case BackupFormat::FILE_HEADER:
          parse_file_header(signal, file_ptr, data - 3, len + 3);
          break;
        case BackupFormat::FRAGMENT_HEADER:
          file_ptr.p->m_status = status | File::READING_RECORDS;
          parse_fragment_header(signal, file_ptr, data, len);
          break;
        case BackupFormat::FRAGMENT_FOOTER:
          parse_fragment_footer(signal, file_ptr, data, len);
          break;
        case BackupFormat::TABLE_LIST:
          parse_table_list(signal, file_ptr, data, len);
          break;
        case BackupFormat::TABLE_DESCRIPTION:
          parse_table_description(signal, file_ptr, data, len);
          break;
        case BackupFormat::GCP_ENTRY:
          parse_gcp_entry(signal, file_ptr, data, len);
          break;
        case BackupFormat::EMPTY_ENTRY:
          // skip
          break;
        case 0x4e444242:  // 'NDBB'
          if (check_file_version(signal, ntohl(*(data + 2))) == 0) {
            break;
          }
          // Fall through - on bad version
          [[fallthrough]];
        default:
          parse_error(signal, file_ptr, __LINE__, ntohl(*data));
      }
    }
  } while (0);

  if (file_ptr.p->m_bytes_left == 0 && status & File::FILE_EOF) {
    file_ptr.p->m_status &= ~(Uint32)File::RESTORE_THREAD_RUNNING;
    /**
     * File is finished...
     */
    close_file(signal, file_ptr);
    return;
  }

  /**
   * We send an immediate signal to continue the restore, at times this
   * could lead to burning some extra CPU since we might still wait for
   * input from the disk reading. This code is however only executed
   * as part of restarts, so it should be ok to spend some extra CPU
   * to ensure that restarts are quick.
   */
  signal->theData[0] = RestoreContinueB::RESTORE_NEXT;
  signal->theData[1] = file_ptr.i;
  sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
}

void Restore::read_data_file(Signal *signal, FilePtr file_ptr) {
  Uint32 left = file_ptr.p->m_bytes_left;
  Uint32 page_count = file_ptr.p->m_pages.getSize();
  Uint32 free = GLOBAL_PAGE_SIZE * page_count - left;
  Uint32 read_count = free / GLOBAL_PAGE_SIZE;

  if (read_count <= file_ptr.p->m_outstanding_reads) {
    signal->theData[0] = RestoreContinueB::READ_FILE;
    signal->theData[1] = file_ptr.i;
    sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
    return;
  }

  read_count -= file_ptr.p->m_outstanding_reads;
  Uint32 curr_page = file_ptr.p->m_current_page_index;
  LocalList pages(m_databuffer_pool, file_ptr.p->m_pages);

  FsReadWriteReq *req = (FsReadWriteReq *)signal->getDataPtrSend();
  req->filePointer = file_ptr.p->m_fd;
  req->userReference = reference();
  req->userPointer = file_ptr.i;
  req->numberOfPages = 1;
  req->operationFlag = 0;
  FsReadWriteReq::setFormatFlag(req->operationFlag,
                                FsReadWriteReq::fsFormatGlobalPage);
  FsReadWriteReq::setPartialReadFlag(req->operationFlag, 1);

  Uint32 start = (curr_page + page_count - read_count) % page_count;

  List::Iterator it;
  pages.position(it, start);
  do {
    file_ptr.p->m_outstanding_reads++;
    req->varIndex = file_ptr.p->m_current_file_page++;
    req->data.globalPage.pageNumber = *it.data;
    sendSignal(NDBFS_REF, GSN_FSREADREQ, signal,
               FsReadWriteReq::FixedLength + 1, JBA);

    start++;
    if (start == page_count) {
      start = 0;
      pages.position(it, start);
    } else {
      pages.next(it);
    }
  } while (start != curr_page);
}

void Restore::execFSREADREF(Signal *signal) {
  jamEntry();
  FilePtr file_ptr;
  FsRef *ref = (FsRef *)signal->getDataPtr();
  ndbrequire(m_file_pool.getPtr(file_ptr, ref->userPointer));
  if (file_ptr.p->m_status == File::READ_CTL_FILES) {
    jam();
    read_ctl_file_done(signal, file_ptr, 0);
    return;
  }
  SimulatedBlock::execFSREADREF(signal);
  ndbabort();
}

void Restore::execFSREADCONF(Signal *signal) {
  jamEntry();
  FilePtr file_ptr;
  FsConf *conf = (FsConf *)signal->getDataPtr();
  ndbrequire(m_file_pool.getPtr(file_ptr, conf->userPointer));

  if (file_ptr.p->m_status == File::READ_CTL_FILES) {
    jam();
    read_ctl_file_done(signal, file_ptr, conf->bytes_read);
    return;
  }
  file_ptr.p->m_bytes_left += conf->bytes_read;

  ndbassert(file_ptr.p->m_outstanding_reads);
  file_ptr.p->m_outstanding_reads--;

  if (file_ptr.p->m_outstanding_reads == 0) {
    ndbassert(conf->bytes_read <= GLOBAL_PAGE_SIZE);
    if (conf->bytes_read == GLOBAL_PAGE_SIZE) {
      jam();
      read_data_file(signal, file_ptr);
    } else {
      jam();
      file_ptr.p->m_status |= File::FILE_EOF;
      file_ptr.p->m_status &= ~(Uint32)File::FILE_THREAD_RUNNING;
    }
  }
}

void Restore::close_file(Signal *signal, FilePtr file_ptr, bool remove_flag) {
  FsCloseReq *req = (FsCloseReq *)signal->getDataPtrSend();
  req->filePointer = file_ptr.p->m_fd;
  req->userPointer = file_ptr.i;
  req->userReference = reference();
  req->fileFlag = 0;
  if (remove_flag) {
    jam();
    FsCloseReq::setRemoveFileFlag(req->fileFlag, 1);
  }
  sendSignal(NDBFS_REF, GSN_FSCLOSEREQ, signal, FsCloseReq::SignalLength, JBA);
}

void Restore::execFSCLOSEREF(Signal *signal) {
  jamEntry();
  SimulatedBlock::execFSCLOSEREF(signal);
  ndbabort();
}

void Restore::execFSCLOSECONF(Signal *signal) {
  jamEntry();
  FilePtr file_ptr;
  FsConf *conf = (FsConf *)signal->getDataPtr();
  ndbrequire(m_file_pool.getPtr(file_ptr, conf->userPointer));

  file_ptr.p->m_fd = RNIL;

  if (file_ptr.p->m_status == File::READ_CTL_FILES) {
    jam();
    close_ctl_file_done(signal, file_ptr);
    return;
  } else if (file_ptr.p->m_status == File::CREATE_CTL_FILE) {
    jam();
    lcp_create_ctl_done_close(signal, file_ptr);
    return;
  } else if (file_ptr.p->m_status == File::DROP_OLD_FILES) {
    jam();
    lcp_drop_old_files(signal, file_ptr);
    return;
  }

  if (file_ptr.p->m_outstanding_operations == 0) {
    jam();
    restore_lcp_conf_after_execute(signal, file_ptr);
    return;
  }
}

void Restore::parse_file_header(Signal *signal, FilePtr file_ptr,
                                const Uint32 *data, Uint32 len) {
  const BackupFormat::FileHeader *fh = (BackupFormat::FileHeader *)data;

  if (memcmp(fh->Magic, "NDBBCKUP", 8) != 0) {
    parse_error(signal, file_ptr, __LINE__, *data);
    return;
  }

  file_ptr.p->m_lcp_version = ntohl(fh->BackupVersion);
  if (check_file_version(signal, ntohl(fh->BackupVersion))) {
    parse_error(signal, file_ptr, __LINE__, ntohl(fh->NdbVersion));
    return;
  }
  ndbassert(ntohl(fh->SectionType) == BackupFormat::FILE_HEADER);

  if (ntohl(fh->SectionLength) != len - 3) {
    parse_error(signal, file_ptr, __LINE__, ntohl(fh->SectionLength));
    return;
  }

  if (ntohl(fh->FileType) != BackupFormat::LCP_FILE) {
    parse_error(signal, file_ptr, __LINE__, ntohl(fh->FileType));
    return;
  }

  if (fh->ByteOrder != 0x12345678) {
    parse_error(signal, file_ptr, __LINE__, fh->ByteOrder);
    return;
  }
}

void Restore::parse_table_list(Signal *signal, FilePtr file_ptr,
                               const Uint32 *data, Uint32 len) {
  const BackupFormat::CtlFile::TableList *fh =
      (BackupFormat::CtlFile::TableList *)data;

  if (ntohl(fh->TableIds[0]) != file_ptr.p->m_table_id) {
    parse_error(signal, file_ptr, __LINE__, ntohl(fh->TableIds[0]));
    return;
  }
}

void Restore::parse_table_description(Signal *signal, FilePtr file_ptr,
                                      const Uint32 *data, Uint32 len) {
  const BackupFormat::CtlFile::TableDescription *fh =
      (BackupFormat::CtlFile::TableDescription *)data;

  SimplePropertiesLinearReader it(fh->DictTabInfo, len);
  it.first();

  DictTabInfo::Table tmpTab;
  tmpTab.init();
  SimpleProperties::UnpackStatus stat;
  stat = SimpleProperties::unpack(it, &tmpTab, DictTabInfo::TableMapping,
                                  DictTabInfo::TableMappingSize);
  ndbrequire(stat == SimpleProperties::Break);

  if (tmpTab.TableId != file_ptr.p->m_table_id) {
    parse_error(signal, file_ptr, __LINE__, tmpTab.TableId);
    return;
  }

  file_ptr.p->m_table_version = tmpTab.TableVersion;
}

void Restore::parse_fragment_header(Signal *signal, FilePtr file_ptr,
                                    const Uint32 *data, Uint32 len) {
  const BackupFormat::DataFile::FragmentHeader *fh =
      (BackupFormat::DataFile::FragmentHeader *)data;
  if (ntohl(fh->TableId) != file_ptr.p->m_table_id) {
    parse_error(signal, file_ptr, __LINE__, ntohl(fh->TableId));
    return;
  }

  if (ntohl(fh->ChecksumType) != 0) {
    parse_error(signal, file_ptr, __LINE__, ntohl(fh->SectionLength));
    return;
  }

  file_ptr.p->m_fragment_id = ntohl(fh->FragmentNo);
}

const char *Restore::get_state_string(Uint32 part_state) {
  switch (part_state) {
    case File::PART_IGNORED:
      return "IGNORED";
    case File::PART_ALL_ROWS:
      return "ALL ROWS";
    case File::PART_ALL_CHANGES:
      return "CHANGED ROWS";
    default:
      return "Unknown";
  }
  return NULL;
}

const char *Restore::get_header_string(Uint32 header_type) {
  switch (header_type) {
    case BackupFormat::INSERT_TYPE:
      return "INSERT_TYPE";
    case BackupFormat::WRITE_TYPE:
      return "WRITE_TYPE";
    case BackupFormat::DELETE_BY_PAGEID_TYPE:
      return "DELETE_BY_PAGEID_TYPE";
    case BackupFormat::DELETE_BY_ROWID_TYPE:
      return "DELETE_BY_ROWID_TYPE";
    default:
      ndbabort();
      return NULL;
  }
}

void Restore::parse_record(Signal *signal, FilePtr file_ptr, const Uint32 *data,
                           Uint32 len, BackupFormat::RecordType header_type) {
  Uint32 page_no = data[1];
  data += 1;
  file_ptr.p->m_error_code = 0;
  ndbrequire(file_ptr.p->m_lcp_version >= NDBD_RAW_LCP);
  if (page_no >= file_ptr.p->m_max_page_cnt) {
    /**
     * Page ignored since it is not part of this LCP.
     * Can happen with multiple files used to restore coming
     * from different LCPs.
     */
    jam();
    return;
  }
  Uint32 part_id = c_backup->hash_lcp_part(page_no);
  ndbrequire(part_id < MAX_LCP_PARTS_SUPPORTED);
  /*
  DEB_HIGH_RES(("(%u)parse_record, page_no: %u, part: %u,"
                " state: %s, header_type: %s",
                instance(),
                page_no,
                part_id,
                get_state_string(Uint32(file_ptr.p->m_part_state[part_id])),
                get_header_string(Uint32(header_type))));
  */
  switch (file_ptr.p->m_part_state[part_id]) {
    case File::PART_IGNORED: {
      jam();
      /**
       * The row is a perfectly ok row, but we will ignore since
       * this part is handled by a later LCP data file.
       */
      file_ptr.p->m_ignored_rows++;
      return;
    }
    case File::PART_ALL_ROWS: {
      jam();
      /**
       * The data file contains all rows for this part, it contains no
       * DELETE BY ROWID. This part will be ignored in earlier LCP data
       * files restored, so we can safely use ZINSERT here as op_type.
       */
      ndbrequire(header_type == BackupFormat::INSERT_TYPE);
      break;
    }
    case File::PART_ALL_CHANGES: {
      jam();
      /**
       * This is a row that changed during the LCP this data file records.
       * The row could either exist or not dependent on if the operation
       * that changed it was an INSERT or an UPDATE. It could also be a
       * DELETE, in this case we only record the rowid and nothing more
       * to indicate this rowid was deleted. We will discover this below.
       */
      ndbrequire(header_type != BackupFormat::INSERT_TYPE);
      break;
    }
    default: {
      jam();
      ndbabort();
      return; /* Silence compiler warnings */
    }
  }
  Uint32 outstanding = file_ptr.p->m_outstanding_operations;
  if (header_type == BackupFormat::INSERT_TYPE) {
    /**
     * This is a normal INSERT as part of our restore process.
     * We install using a binary image saved in LCP file.
     */
    Uint32 *const key_start = signal->getDataPtrSend() + 24;
    Uint32 *const attr_start = key_start + MAX_KEY_SIZE_IN_WORDS;
    Local_key rowid_val;
    jam();
    rowid_val.m_page_no = data[0];
    rowid_val.m_page_idx = data[1];
    file_ptr.p->m_rowid_page_no = rowid_val.m_page_no;
    file_ptr.p->m_rowid_page_idx = rowid_val.m_page_idx;
    Uint32 keyLen = c_tup->read_lcp_keys(file_ptr.p->m_table_id, data + 2,
                                         len - 3, key_start);
    AttributeHeader::init(attr_start, AttributeHeader::READ_LCP, 4 * (len - 3));
    Uint32 attrLen = 1 + len - 3;
    file_ptr.p->m_rows_restored_insert++;
    memcpy(attr_start + 1, data + 2, 4 * (len - 3));
    DEB_HIGH_RES(
        ("(%u)INSERT_TYPE tab(%u,%u), row(%u,%u),"
         " keyLen: %u, key[0]: %x",
         instance(), file_ptr.p->m_table_id, file_ptr.p->m_fragment_id,
         rowid_val.m_page_no, rowid_val.m_page_idx, keyLen, key_start[0]));

    execute_operation(signal, file_ptr, keyLen, attrLen, ZINSERT, 0,
                      Uint32(BackupFormat::INSERT_TYPE), &rowid_val);
    handle_return_execute_operation(signal, file_ptr, data, len, outstanding);
  } else {
    if (header_type == BackupFormat::DELETE_BY_ROWID_TYPE ||
        header_type == BackupFormat::WRITE_TYPE) {
      Local_key rowid_val;
      rowid_val.m_page_no = data[0];
      rowid_val.m_page_idx = data[1];
      file_ptr.p->m_rowid_page_no = rowid_val.m_page_no;
      file_ptr.p->m_rowid_page_idx = rowid_val.m_page_idx;
      jam();
      Uint32 gci_id = 0;
      Uint32 sent_header_type;
      if (header_type == BackupFormat::DELETE_BY_ROWID_TYPE) {
        gci_id = data[2];
        if (gci_id == 0) {
          jam();
          /**
           * We didn't have access to the GCI at LCP time, row
           * was in a new page and we didn't know about the GCI of the
           * old row in a previous page incarnation.
           * The DELETE BY ROWID could also have come through a
           * LCP keep list where the GCI isn't transported.
           *
           * The row is deleted at end of this restore and the
           * restore will have at least restored everything up to
           * Max GCI completed, if any changes happened after this
           * they will be in REDO log or need to be fetched from
           * live node.
           *
           * It is important to ensure that it is set to at least
           * this value to ensure that this node can properly
           * delete this row for a node that have been dead for an
           * extended amount of time.
           */
          gci_id = file_ptr.p->m_max_gci_completed;
        }
        sent_header_type = (Uint32)BackupFormat::DELETE_BY_ROWID_TYPE;
        file_ptr.p->m_rows_restored_delete++;
        DEB_HIGH_RES(
            ("(%u)1:DELETE_BY_ROWID tab(%u,%u), row(%u,%u),"
             " gci=%u",
             instance(), file_ptr.p->m_table_id, file_ptr.p->m_fragment_id,
             rowid_val.m_page_no, rowid_val.m_page_idx, gci_id));
      } else {
        sent_header_type = (Uint32)BackupFormat::DELETE_BY_ROWID_WRITE_TYPE;
        file_ptr.p->m_rows_restored_write++;
        DEB_HIGH_RES(
            ("(%u)2:DELETE_BY_ROWID tab(%u,%u), row(%u,%u),"
             " gci=%u",
             instance(), file_ptr.p->m_table_id, file_ptr.p->m_fragment_id,
             rowid_val.m_page_no, rowid_val.m_page_idx, gci_id));
      }
      execute_operation(signal, file_ptr, 0, 0, ZDELETE, gci_id,
                        sent_header_type, &rowid_val);
      if (header_type == BackupFormat::WRITE_TYPE) {
        /**
         * We found a CHANGE record. This is written into the LCP file
         * as part of an LCP where the part only records changes. In
         * this case we might have already inserted the row in a previous
         * LCP file. To simplify code we use a DELETE followed by a
         * normal LCP insert. Otherwise we will have to complicate the
         * TUP code to handle writes of LCP data.
         *
         * Normally there should be a smaller amount of those
         * records, so the performance impact should not be
         * very high.
         */
        DEB_HIGH_RES(("(%u)WRITE_TYPE tab(%u,%u), row(%u,%u), gci=%u",
                      instance(), file_ptr.p->m_table_id,
                      file_ptr.p->m_fragment_id, rowid_val.m_page_no,
                      rowid_val.m_page_idx, gci_id));
        Uint32 *const key_start = signal->getDataPtrSend() + 24;
        Uint32 *const attr_start = key_start + MAX_KEY_SIZE_IN_WORDS;
        Uint32 keyLen = c_tup->read_lcp_keys(file_ptr.p->m_table_id, data + 2,
                                             len - 3, key_start);
        AttributeHeader::init(attr_start, AttributeHeader::READ_LCP,
                              4 * (len - 3));
        Uint32 attrLen = 1 + len - 3;
        memcpy(attr_start + 1, data + 2, 4 * (len - 3));
        execute_operation(signal, file_ptr, keyLen, attrLen, ZINSERT, gci_id,
                          header_type, &rowid_val);
        handle_return_execute_operation(signal, file_ptr, data, len,
                                        outstanding);
      } else {
        /**
         * We found a DELETE BY ROWID, this deletes the row in the rowid
         * position, This can happen in parts where we record changes, we might
         * have inserted the row in an earlier LCP data file, so we need to
         * attempt to remove it here.
         *
         * For DELETE by ROWID there is no key and no ATTRINFO to send.
         * The key is instead the rowid which is sent when the row id flag is
         * set.
         */
        DEB_HIGH_RES(("(%u)3:DELETE_BY_ROWID tab(%u,%u), row(%u,%u), gci=%u",
                      instance(), file_ptr.p->m_table_id,
                      file_ptr.p->m_fragment_id, rowid_val.m_page_no,
                      rowid_val.m_page_idx, gci_id));
        ndbrequire(len == (3 + 1));
        ndbrequire(outstanding == file_ptr.p->m_outstanding_operations);
      }
    } else {
      jam();
      Local_key rowid_val;
      DEB_HIGH_RES(("(%u)DELETE_BY_PAGEID tab(%u,%u), page=%u, record_size=%u",
                    instance(), file_ptr.p->m_table_id,
                    file_ptr.p->m_fragment_id, data[0], data[1]));
      ndbrequire(header_type == BackupFormat::DELETE_BY_PAGEID_TYPE);
      ndbrequire(len == (2 + 1));
      /* DELETE by PAGEID, a loop of DELETE by ROWID */
      rowid_val.m_page_no = data[0];
      rowid_val.m_page_idx = 0;
      Uint32 record_size = data[1];
      file_ptr.p->m_outstanding_operations++;
      file_ptr.p->m_rows_restored_delete_page++;
      while ((rowid_val.m_page_idx + record_size) <=
             Tup_fixsize_page::DATA_WORDS) {
        jam();
        execute_operation(signal, file_ptr, 0, 0, ZDELETE, 0, header_type,
                          &rowid_val);
        rowid_val.m_page_idx += record_size;
      }
      ndbrequire(file_ptr.p->m_outstanding_operations > 0);
      file_ptr.p->m_outstanding_operations--;
      ndbrequire(outstanding == file_ptr.p->m_outstanding_operations);
      check_restore_ready(signal, file_ptr);
    }
  }
}

void Restore::handle_return_execute_operation(Signal *signal, FilePtr file_ptr,
                                              const Uint32 *data, Uint32 len,
                                              Uint32 outstanding) {
  ndbrequire(outstanding == file_ptr.p->m_outstanding_operations);
  if (file_ptr.p->m_error_code == 0) {
    return; /* Normal path, return */
  }
  Uint32 *const key_start = signal->getDataPtrSend() + 24;
  Uint32 *const attr_start = key_start + MAX_KEY_SIZE_IN_WORDS;
  Local_key rowid_val;
  Uint32 keyLen;
  Uint32 attrLen = 1 + len - 3;

  if (file_ptr.p->m_error_code != 630 || file_ptr.p->m_num_files == 1 ||
      file_ptr.p->m_current_file_index == 0)
    goto error;

  jam();
  /**
   * 630 means that key already exists. When inserting a row during
   * restore it is normal that the key we're inserting can exist. This
   * key can have been inserted by a previous insert into a different
   * rowid.
   *
   * The rowid where this key previously existed can have a DELETE BY
   * ROWID operation in the LCP files, it could have a WRITE with a
   * different key as well.
   * In both those cases it is possible that the INSERT comes before
   * this DELETE BY ROWID or WRITE operation since these happen in
   * rowid order and not in key order. They can even happen in a
   * different LCP file since one LCP can span multiple LCP files.
   *
   * To ensure consistency we track exactly how many rows we restored
   * during the restore of the LCP files.
   *
   * We need to reinitialise key data and attribute data from data
   * array since signal object isn't safe after executing the
   * LQHKEYREQ signal.
   *
   * This cannot happen with only 1 LCP file and it cannot happen in
   * the first LCP file.
   */

  DEB_RES(
      ("(%u)tab(%u,%u) row(%u,%u) key already existed,"
       " num_files: %u, current_file: %u",
       instance(), file_ptr.p->m_table_id, file_ptr.p->m_fragment_id,
       file_ptr.p->m_rowid_page_no, file_ptr.p->m_rowid_page_idx,
       file_ptr.p->m_num_files, file_ptr.p->m_current_file_index));

  keyLen = c_tup->read_lcp_keys(file_ptr.p->m_table_id, data + 2, len - 3,
                                key_start);
  execute_operation(signal, file_ptr, keyLen, 0, ZDELETE, 0,
                    BackupFormat::NORMAL_DELETE_TYPE, NULL);

  ndbrequire(outstanding == file_ptr.p->m_outstanding_operations);
  if (file_ptr.p->m_error_code != 0) goto error;

  /**
   * Setup key data and attribute data again, since the signal
   * object cannot be regarded as safe, we need to reinitialise
   * this data.
   */
  keyLen = c_tup->read_lcp_keys(file_ptr.p->m_table_id, data + 2, len - 3,
                                key_start);
  AttributeHeader::init(attr_start, AttributeHeader::READ_LCP, 4 * (len - 3));
  memcpy(attr_start + 1, data + 2, 4 * (len - 3));
  rowid_val.m_page_no = data[0];
  rowid_val.m_page_idx = data[1];
  execute_operation(signal, file_ptr, keyLen, attrLen, ZINSERT, 0,
                    Uint32(BackupFormat::INSERT_TYPE), &rowid_val);
  ndbrequire(outstanding == file_ptr.p->m_outstanding_operations);
  ndbrequire(file_ptr.p->m_error_code == 0);
  return;

error:
  g_eventLogger->info("(%u)tab(%u,%u),row(%u,%u) crash, error: %u", instance(),
                      file_ptr.p->m_table_id, file_ptr.p->m_fragment_id,
                      file_ptr.p->m_rowid_page_no, file_ptr.p->m_rowid_page_idx,
                      file_ptr.p->m_error_code);
  ndbrequire(file_ptr.p->m_error_code == 0);
}

void Restore::execute_operation(Signal *signal, FilePtr file_ptr, Uint32 keyLen,
                                Uint32 attrLen, Uint32 op_type, Uint32 gci_id,
                                Uint32 header_type, Local_key *rowid_val) {
  LqhKeyReq *req = (LqhKeyReq *)signal->getDataPtrSend();
  /**
   * attrLen is not used for long lqhkeyreq, and should be zero for short
   * lqhkeyreq.
   */
  req->attrLen = 0;

  Uint32 tmp = 0;
  const bool short_lqhkeyreq = (keyLen == 0);
  /**
   * With partital LCP also other operations like delete by rowid will be used.
   * In these cases no data is passed, and receiver will interpret signal as a
   * short signal, but no KEYINFO or ATTRINFO will be sent or expected.
   */
  Uint32 *const key_start = signal->getDataPtrSend() + 24;
  if (short_lqhkeyreq) {
    ndbrequire(attrLen == 0);
    ndbassert(keyLen == 0);
    LqhKeyReq::setKeyLen(tmp, keyLen);
  }
  if (!short_lqhkeyreq) {
    LqhKeyReq::setDisableFkConstraints(tmp, 0);
    LqhKeyReq::setNoTriggersFlag(tmp, 0);
    LqhKeyReq::setUtilFlag(tmp, 0);
  }
  LqhKeyReq::setLastReplicaNo(tmp, 0);
  /* ---------------------------------------------------------------------- */
  // Indicate Application Reference is present in bit 15
  /* ---------------------------------------------------------------------- */
  LqhKeyReq::setApplicationAddressFlag(tmp, 0);
  LqhKeyReq::setDirtyFlag(tmp, 1);
  LqhKeyReq::setSimpleFlag(tmp, 1);
  LqhKeyReq::setOperation(tmp, op_type);
  LqhKeyReq::setSameClientAndTcFlag(tmp, 0);
  if (short_lqhkeyreq) {
    LqhKeyReq::setAIInLqhKeyReq(tmp, 0);
    req->hashValue = 0;
  } else {
    Uint32 tableId = file_ptr.p->m_table_id;
    LqhKeyReq::setCorrFactorFlag(tmp, 0);
    LqhKeyReq::setNormalProtocolFlag(tmp, 0);
    LqhKeyReq::setDeferredConstraints(tmp, 0);

    if (g_key_descriptor_pool.getPtr(tableId)->hasCharAttr) {
      req->hashValue = calculate_hash(tableId, key_start);
    } else {
      req->hashValue = md5_hash(key_start, keyLen);
    }
  }
  LqhKeyReq::setNoDiskFlag(tmp, 1);
  LqhKeyReq::setRowidFlag(tmp, (rowid_val != 0));
  req->clientConnectPtr = (file_ptr.i + (header_type << 28));
  req->tcBlockref = reference();
  req->savePointId = 0;
  req->tableSchemaVersion =
      file_ptr.p->m_table_id + (file_ptr.p->m_table_version << 16);
  req->fragmentData = file_ptr.p->m_fragment_id;
  req->transId1 = 0;
  req->transId2 = 0;
  req->scanInfo = 0;
  Uint32 pos = 0;
  if (op_type != ZDELETE) {
    /**
     * Need not set GCI flag here since we restore also the header part of
     * the row in this case.
     */
    req->variableData[pos++] = rowid_val->m_page_no;
    req->variableData[pos++] = rowid_val->m_page_idx;
    LqhKeyReq::setGCIFlag(tmp, 0);
  } else {
    /**
     * We reuse the Node Restart Copy handling to perform
     * DELETE by ROWID. In this case we need to set the GCI of the record.
     */
    if (rowid_val) {
      req->variableData[pos++] = rowid_val->m_page_no;
      req->variableData[pos++] = rowid_val->m_page_idx;
      LqhKeyReq::setGCIFlag(tmp, 1);
      LqhKeyReq::setNrCopyFlag(tmp, 1);
      req->variableData[pos++] = gci_id;
    }
  }
  req->requestInfo = tmp;
  if (short_lqhkeyreq) {
    file_ptr.p->m_outstanding_operations++;
    EXECUTE_DIRECT(getDBLQH(), GSN_LQHKEYREQ, signal,
                   LqhKeyReq::FixedSignalLength + pos);
  } else {
    bool ok = true;
    SectionHandle sections(this);
    sections.clear();

    sections.m_ptr[LqhKeyReq::KeyInfoSectionNum].i = RNIL;
    ok = appendToSection(sections.m_ptr[LqhKeyReq::KeyInfoSectionNum].i,
                         key_start, keyLen);
    if (unlikely(!ok)) {
      jam();
      crash_during_restore(file_ptr, __LINE__, ZGET_DATAREC_ERROR);
      ndbabort();
    }
    sections.m_cnt++;

    if (attrLen > 0) {
      Uint32 *const attr_start = key_start + MAX_KEY_SIZE_IN_WORDS;
      sections.m_ptr[LqhKeyReq::AttrInfoSectionNum].i = RNIL;
      ok = appendToSection(sections.m_ptr[LqhKeyReq::AttrInfoSectionNum].i,
                           attr_start, attrLen);

      if (unlikely(!ok)) {
        jam();
        crash_during_restore(file_ptr, __LINE__, ZGET_ATTRINBUF_ERROR);
        ndbabort();
      }
      sections.m_cnt++;
    }
    file_ptr.p->m_outstanding_operations++;
    EXECUTE_DIRECT_WITH_SECTIONS(getDBLQH(), GSN_LQHKEYREQ, signal,
                                 LqhKeyReq::FixedSignalLength + pos, &sections);
  }
}

Uint32 Restore::calculate_hash(Uint32 tableId, const Uint32 *src) {
  jam();
  Uint32 tmp[MAX_KEY_SIZE_IN_WORDS * MAX_XFRM_MULTIPLY];
  Uint32 keyPartLen[MAX_ATTRIBUTES_IN_INDEX];
  Uint32 keyLen =
      xfrm_key_hash(tableId, src, tmp, sizeof(tmp) >> 2, keyPartLen);
  ndbrequire(keyLen);

  return md5_hash(tmp, keyLen);
}

void Restore::execLQHKEYREF(Signal *signal) {
  FilePtr file_ptr;
  LqhKeyRef *ref = (LqhKeyRef *)signal->getDataPtr();
  BackupFormat::RecordType header_type =
      (BackupFormat::RecordType)(ref->connectPtr >> 28);
  ndbrequire(m_file_pool.getPtr(file_ptr, (ref->connectPtr & 0x0FFFFFFF)));

  ndbrequire(file_ptr.p->m_outstanding_operations > 0);
  file_ptr.p->m_outstanding_operations--;
  file_ptr.p->m_error_code = 0;
  switch (header_type) {
    case BackupFormat::DELETE_BY_ROWID_TYPE: {
      jam();
      break;
    }
    case BackupFormat::DELETE_BY_PAGEID_TYPE: {
      jam();
      break;
    }
    case BackupFormat::DELETE_BY_ROWID_WRITE_TYPE: {
      jam();
      break;
    }
    case BackupFormat::INSERT_TYPE:
    case BackupFormat::WRITE_TYPE:
    case BackupFormat::NORMAL_DELETE_TYPE:
    default: {
      jam();
      file_ptr.p->m_error_code = ref->errorCode;
      return;
    }
  }
  file_ptr.p->m_rows_restored_delete_failed++;
  file_ptr.p->m_row_operations++;
  check_restore_ready(signal, file_ptr);
}

void Restore::crash_during_restore(FilePtr file_ptr, Uint32 line,
                                   Uint32 errCode) {
  char buf[255], name[100];
  BaseString::snprintf(name, sizeof(name), "%u/T%dF%d", file_ptr.p->m_file_id,
                       file_ptr.p->m_table_id, file_ptr.p->m_fragment_id);

  if (errCode) {
    BaseString::snprintf(buf, sizeof(buf),
                         "Error %d (line: %u) during restore of  %s", errCode,
                         line, name);
  } else {
    BaseString::snprintf(buf, sizeof(buf),
                         "Error (line %u) during restore of  %s", line, name);
  }
  progError(__LINE__, NDBD_EXIT_INVALID_LCP_FILE, buf);
}

void Restore::delete_by_rowid_fail(Uint32 op_ptr) {
  FilePtr file_ptr;
  ndbrequire(m_file_pool.getPtr(file_ptr, (op_ptr & 0x0FFFFFFF)));
  DEB_RES_DEL(("(%u)DELETE fail:tab(%u,%u), m_rows_restored = %llu", instance(),
               file_ptr.p->m_table_id, file_ptr.p->m_fragment_id,
               file_ptr.p->m_rows_restored));
}

void Restore::delete_by_rowid_succ(Uint32 op_ptr) {
  FilePtr file_ptr;
  ndbrequire(m_file_pool.getPtr(file_ptr, (op_ptr & 0x0FFFFFFF)));
  ndbrequire(file_ptr.p->m_rows_restored > 0);
  file_ptr.p->m_rows_restored--;
  DEB_RES_DEL(("(%u)DELETE success:tab(%u,%u), m_rows_restored = %llu",
               instance(), file_ptr.p->m_table_id, file_ptr.p->m_fragment_id,
               file_ptr.p->m_rows_restored));
}

void Restore::execLQHKEYCONF(Signal *signal) {
  FilePtr file_ptr;
  LqhKeyConf *conf = (LqhKeyConf *)signal->getDataPtr();
  BackupFormat::RecordType header_type =
      (BackupFormat::RecordType)(conf->opPtr >> 28);
  ndbrequire(m_file_pool.getPtr(file_ptr, (conf->opPtr & 0x0FFFFFFF)));

  ndbassert(file_ptr.p->m_outstanding_operations);
  file_ptr.p->m_outstanding_operations--;
  file_ptr.p->m_error_code = 0;
  switch (header_type) {
    case BackupFormat::INSERT_TYPE:
      jam();
      file_ptr.p->m_rows_restored++;
      file_ptr.p->m_row_operations++;
      break;
    case BackupFormat::WRITE_TYPE:
      jam();
      file_ptr.p->m_rows_restored++;
      file_ptr.p->m_row_operations++;
      break;
    case BackupFormat::NORMAL_DELETE_TYPE:
      jam();
      file_ptr.p->m_rows_restored--;
      file_ptr.p->m_row_operations++;
      break;
    case BackupFormat::DELETE_BY_ROWID_TYPE:
    case BackupFormat::DELETE_BY_PAGEID_TYPE:
    case BackupFormat::DELETE_BY_ROWID_WRITE_TYPE:
      jam();
      file_ptr.p->m_row_operations++;
      break;
    default:
      ndbabort();
  }
  check_restore_ready(signal, file_ptr);
}

void Restore::check_restore_ready(Signal *signal, FilePtr file_ptr) {
  if (file_ptr.p->m_outstanding_operations == 0 && file_ptr.p->m_fd == RNIL) {
    jam();
    restore_lcp_conf_after_execute(signal, file_ptr);
    return;
  }
}

void Restore::restore_lcp_conf_after_execute(Signal *signal, FilePtr file_ptr) {
  file_ptr.p->m_current_file_index++;
  if (file_ptr.p->m_current_file_index < file_ptr.p->m_num_files) {
    /**
     * There are still more data files to apply before restore is complete.
     * Handle next file now.
     */
    jam();
    DEB_RES(("(%u)Step forward to next data file", instance()));
    step_file_number_forward(file_ptr);
    file_ptr.p->m_current_page_pos = 0;
    file_ptr.p->m_current_page_index = 0;
    file_ptr.p->m_current_file_page = 0;
    ndbrequire(file_ptr.p->m_outstanding_reads == 0);
    ndbrequire(file_ptr.p->m_outstanding_operations == 0);
    ndbrequire(file_ptr.p->m_bytes_left == 0);
    release_file(file_ptr, false);
    ndbrequire(seize_file(file_ptr) == 0);
    open_data_file(signal, file_ptr);
    return;
  }
  restore_lcp_conf(signal, file_ptr);
}

void Restore::restore_lcp_conf(Signal *signal, FilePtr file_ptr) {
  /**
   * All LCP data files that are part of restore have been applied
   * successfully, this fragment has completed its restore and we're
   * ready to continue with the next step.
   */

  /**
   * For Recover threads we have to ensure that any expand or shrink
   * of the fragment in DBACC has completed its work before we move
   * on. We don't support a recover thread working in parallel with
   * the LDM thread once the restore is done.
   */

  if (m_is_query_block) {
    jam();
    if (c_lqh->check_expand_shrink_ongoing(file_ptr.p->m_table_id,
                                           file_ptr.p->m_fragment_id)) {
      jam();
      /* Expand is ongoing still, we need to wait until it is done */
      signal->theData[0] = RestoreContinueB::CHECK_EXPAND_SHRINK;
      signal->theData[1] = file_ptr.i;
      sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
      return;
    }
    /* No expand/shrink ongoing, we can safely move on. */
  }

  /**
   * Temporary reset DBTUP's #disk attributes on table
   *
   * TUP will send RESTORE_LCP_CONF
   */
  DEB_RES(("(%u)Complete restore", instance()));

  if (file_ptr.p->m_lcp_ctl_version == NDBD_USE_PARTIAL_LCP_v2) {
    /**
     * Important to verify that number of rows is what we expect.
     * Otherwise we could go on with inconsistent database without
     * knowing it. So better to crash and specify error.
     */
    if (file_ptr.p->m_rows_in_lcp != file_ptr.p->m_rows_restored) {
      char buf[512];
      BaseString::snprintf(buf, sizeof(buf),
                           "Inconsistency in restoring T%uF%u, restored"
                           " %llu rows, expected to restore %llu rows"
                           "\nInitial node restart is required to recover",
                           file_ptr.p->m_table_id, file_ptr.p->m_fragment_id,
                           file_ptr.p->m_rows_restored,
                           file_ptr.p->m_rows_in_lcp);
      progError(__LINE__, NDBD_EXIT_INVALID_LCP_FILE, buf);
    }
  }
  if (c_tup->get_restore_row_count(file_ptr.p->m_table_id,
                                   file_ptr.p->m_fragment_id) !=
      file_ptr.p->m_rows_restored) {
    char buf[512];
    BaseString::snprintf(
        buf, sizeof(buf),
        "Inconsistency in restoring T%uF%u, restored"
        " %llu rows, TUP claims %llu rows"
        "\nInitial node restart is required to recover",
        file_ptr.p->m_table_id, file_ptr.p->m_fragment_id,
        file_ptr.p->m_rows_restored,
        c_tup->get_restore_row_count(file_ptr.p->m_table_id,
                                     file_ptr.p->m_fragment_id));
    progError(__LINE__, NDBD_EXIT_INVALID_LCP_FILE, buf);
  }
  c_tup->complete_restore_fragment(
      signal, file_ptr.p->m_sender_ref, file_ptr.p->m_sender_data,
      file_ptr.p->m_restored_lcp_id, file_ptr.p->m_restored_local_lcp_id,
      file_ptr.p->m_max_gci_completed, file_ptr.p->m_max_gci_written,
      file_ptr.p->m_table_id, file_ptr.p->m_fragment_id);
  jamEntry();
  signal->theData[0] = NDB_LE_ReadLCPComplete;
  signal->theData[1] = file_ptr.p->m_table_id;
  signal->theData[2] = file_ptr.p->m_fragment_id;
  signal->theData[3] = Uint32(file_ptr.p->m_rows_restored >> 32);
  signal->theData[4] = Uint32(file_ptr.p->m_rows_restored);
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 5, JBB);

  release_file(file_ptr, true);
}

void Restore::parse_fragment_footer(Signal *signal, FilePtr file_ptr,
                                    const Uint32 *data, Uint32 len) {
  const BackupFormat::DataFile::FragmentFooter *fh =
      (BackupFormat::DataFile::FragmentFooter *)data;
  if (ntohl(fh->TableId) != file_ptr.p->m_table_id) {
    parse_error(signal, file_ptr, __LINE__, ntohl(fh->TableId));
    return;
  }

  if (ntohl(fh->Checksum) != 0) {
    parse_error(signal, file_ptr, __LINE__, ntohl(fh->SectionLength));
    return;
  }
}

void Restore::parse_gcp_entry(Signal *signal, FilePtr file_ptr,
                              const Uint32 *data, Uint32 len) {}

void Restore::parse_error(Signal *signal, FilePtr file_ptr, Uint32 line,
                          Uint32 extra) {
  char buf[255], name[100];
  BaseString::snprintf(name, sizeof(name), "%u/T%dF%d", file_ptr.p->m_file_id,
                       file_ptr.p->m_table_id, file_ptr.p->m_fragment_id);

  BaseString::snprintf(buf, sizeof(buf), "Parse error in file: %s, extra: %d",
                       name, extra);

  progError(line, NDBD_EXIT_INVALID_LCP_FILE, buf);
  ndbabort();
}

NdbOut &operator<<(NdbOut &ndbout, const Restore::Column &col) {
  ndbout << "[ Col: id: " << col.m_id << " size: " << col.m_size
         << " key: " << (Uint32)(col.m_flags & Restore::Column::COL_KEY)
         << " variable: " << (Uint32)(col.m_flags & Restore::Column::COL_VAR)
         << " null: " << (Uint32)(col.m_flags & Restore::Column::COL_NULL)
         << " disk: " << (Uint32)(col.m_flags & Restore::Column::COL_DISK)
         << "]";

  return ndbout;
}

int Restore::check_file_version(Signal *signal, Uint32 file_version) {
  if (file_version < MAKE_VERSION(5, 1, 6)) {
    char buf[255];
    char verbuf[255];
    ndbGetVersionString(file_version, 0, 0, verbuf, sizeof(verbuf));
    BaseString::snprintf(buf, sizeof(buf),
                         "Unsupported version of LCP files found on disk, "
                         " found: %s",
                         verbuf);

    progError(__LINE__, NDBD_EXIT_SR_RESTARTCONFLICT, buf);
    return -1;
  }
  return 0;
}
