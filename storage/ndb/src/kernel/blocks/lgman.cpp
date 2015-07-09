/* Copyright (c) 2005, 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; version 2 of
   the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "lgman.hpp"
#include "diskpage.hpp"
#include <signaldata/FsRef.hpp>
#include <signaldata/FsConf.hpp>
#include <signaldata/FsOpenReq.hpp>
#include <signaldata/FsCloseReq.hpp>
#include <signaldata/CreateFilegroupImpl.hpp>
#include <signaldata/DropFilegroupImpl.hpp>
#include <signaldata/FsReadWriteReq.hpp>
#include <signaldata/LCP.hpp>
#include <signaldata/SumaImpl.hpp>
#include <signaldata/LgmanContinueB.hpp>
#include <signaldata/GetTabInfo.hpp>
#include <signaldata/NodeFailRep.hpp>
#include <signaldata/DbinfoScan.hpp>
#include <signaldata/CallbackSignal.hpp>
#include "dbtup/Dbtup.hpp"

#include <EventLogger.hpp>
extern EventLogger * g_eventLogger;

#include <record_types.hpp>

#define JAM_FILE_ID 441


/**
 *
 * IMPORTANT NOTE:
 * ---------------
 * Although the code in many aspects is prepared to handle multiple logfile
 * groups, it can at the moment not handle any more than one logfile group.
 * There is lacking functionality for multiple logfile groups e.g. when
 * executing the UNDO log records.
 *
 * ---<a>-----<b>-----<c>-----<d>---> (time)
 *
 * <a> = start of lcp 1
 * <b> = stop of lcp 1
 * <c> = start of lcp 2
 * <d> = stop of lcp 2
 *
 * If ndb crashes before <d>
 *   the entire undo log from crash point until <a> has to be applied
 *
 * at <d> the undo log can be cut til <c> 
 *
 * LSN = Log Sequence Number, an increasing number which is the id of each
 * UNDO log record. Each page is marked with the last LSN it has been
 * updated with. Thus it is easy to check if a log record should be applied
 * to a page, it should be applied if pageLSN > logLSN. After applying this
 * log record the pageLSN should be set to logLSN.
 *
 * UNDO log entry layout
 * ---------------------
 *
 * There are two types of UNDO log entry types:
 *
 * Type 1 variant:
 * --------------------------------------
 * | data1 | data2 ... | dataN | header |
 * --------------------------------------
 * Type 2 variant:
 * ---------------------------------------------------------------------
 * | next_LSN_low | next_LSN_high | data1 | data2 ... | dataN | header |
 * ---------------------------------------------------------------------
 *
 * Header has 3 fields:
 * Bit 0-15: Length of UNDO log entry
 * Bit 16-30: Type of UNDO log entry
 * Bit 31: Set to to 1 for Type 1 of UNDO log entry and 0 for Type 2
 * (Bit 31 is called UNDO_NEXT_LSN when set to 1)
 *
 * In type 1 the previous LSN record have the LSN of the current record
 * minus one. So we save the 2 words used here. This is the only type
 * used in production software since we currently only support one
 * logfile group.
 *
 * Type 2 is used when we have introduce support for more than one logfile
 * group. Then the previous LSN is not necessarily simply minus one. So
 * here we need to have a back pointer of the LSN value of the previous
 * record.
 *
 * In Type 2 record the length of the UNDO log record doesn't include
 * the extra LSN words. These words are implied by the bit 31 in the
 * header not being set.
 *
 * The current types of UNDO log records are:
 * TUP_ALLOC (UNDO allocate in a page)
 * TUP_UPDATE (UNDO update in a page)
 * TUP_FREE (UNDO free in a page)
 * TUP_CREATE (UNDO allocate a page)
 * TUP_DROP (UNDO deallocate a page)
 * TUP_ALLOC_EXTENT (UNDO allocate an extent of pages)
 * TUP_FREE_EXTENT (UNDO deallocate of an extent of pages)
 *
 * UNDO Page layout
 * ----------------
 * -------------------------------------
 * |   Page Header                     |
 * |------------------------------------
 * |   UNDO log record                 |
 * |------------------------------------
 * |   UNDO log record                 |
 * |------------------------------------
 * | ........... more UNDO log records |
 * |------------------------------------
 *
 * Page Header Layout
 * ------------------
 *
 * -------------------------------------
 * |   LSN High Word                   |
 * -------------------------------------
 * |   LSN Low Word                    |
 * -------------------------------------
 * |   Page type                       |
 * -------------------------------------
 * |   Page position                   |
 * -------------------------------------
 *
 * alloc_log_space, get_log_buffer, add_entry, free_log_space
 * ----------------------------------------------------------
 * LGMAN receives commands to log an UNDO entry from DBTUP.
 * During preparation of a transaction we allocate space in the UNDO log
 * file through the call alloc_log_space in the Logfile_client.
 * 
 * At a later time when we are preparing to actually write this entry into
 * the UNDO log we need a two-step approach, we first ensure there is space
 * in the log buffer for the UNDO log and then finally we send the UNDO log
 * entry.
 * 
 * So writing an entry into the UNDO log normally requires 3 calls through the
 * Logfile_client. First at prepare time we allocate space in UNDO log file,
 * Then at commit time we first allocate space into the UNDO log buffer
 * whereafter we finally send the UNDO log entry.
 *
 * All of this interaction happens between DBTUP in any of the LDM threads
 * and LGMAN which normally executes in the main thread. LGMAN can however
 * execute in the LDM threads through the concept of a Logfile_client which
 * uses a mutex to ensure that access to LGMAN is serialised.
 *
 * During allocation of space in the UNDO log buffer we can discover that no
 * space is available, in this case LGMAN will insert the request into a queue
 * and service it once space is available. This queueing service is handled by
 * the CONTINUEB process using PROCESS_LOG_BUFFER_WAITERS. Since this happens
 * as part of a commit we must continue waiting until we get space, there is
 * no option to quit waiting here since that would break the commit protocol.
 * This means that large commits can take quite long time.
 *
 * TODO: We should ensure that TC is informed of the delay to ensure that TC
 * knows that we have a valid reason for waiting. This makes it easier for
 * TC to track progress on the transaction. It also makes it easier to
 * implement human supportable tracking through ndbinfo tables and so forth.
 *
 * In case we abort the transaction we give back the allocated space in the
 * transaction through calls to free_log_space using the Logfile_client.
 *
 * sync_lsn
 * --------
 * LGMAN participates as the Log manager in the WAL protocol. The WAL
 * (Write Ahead Log) protocol uses the following algorithm. Each time a page
 * is changed in PGMAN an UNDO log entry has been sent to the UNDO log first.
 * This UNDO log entry receives an LSN. The page in PGMAN is updated
 * with the latest LSN. Before writing a page in PGMAN to disk we must
 * ensure that all UNDO log entries up until the page LSN have been written
 * to disk.
 *
 * This is performed using the call sync_lsn using the Logfile_client. Most
 * commonly this occurs from PGMAN before writing a page. It can also occur
 * in relation to LCPs where it is used at end of an LCP to ensure that an LCP
 * can be fully restored (which requires that all UNDO log entries generated
 * as part of LCP is flushed to the disk.
 *
 * Signal END_LCP_REQ
 * ------------------
 * As mentioned in the above section we need to call sync_lsn at end of an LCP.
 * We are informed of LCP end through the signal END_LCP_REQ and will respond
 * with END_LCP_CONF when all logfile groups have completed their sync_lsn
 * calls.
 *
 * exec_lcp_frag_ord
 * -----------------
 * exec_lcp_frag_ord is called from DBLQH each time a new LCP is discovered
 * in DBLQH. So effectively this is called at the start of an LCP.
 *
 * In order to be able to write the UNDO log continously we need to cut the
 * log tail every now and then. We enable cutting of the log tail for both
 * REDO and UNDO logs by running checkpoints. We call our checkpoints, LCPs.
 * At start of a new LCP we can cut away the log tail back until we reach the
 * the start of the previous LCP. As explained above we could cut this already
 * at end of the LCP, but we have no knowledge in LGMAN about when a
 * distributed LCP is completed, so we cut it away instead at the next start
 * of an LCP.
 *
 * So in short LGMAN receives log entries from DBTUP, before PGMAN can write
 * any page it ensures that all log entries of the page have been flushed to
 * disk. To ensure that the log doesn't grow to infinity, we use LCPs to
 * cut the log tail every now and then.
 *
 * Signal START_RECREQ
 * -------------------
 * At restart (system restart or node restart) we need to execute the UNDO log
 * back to the start of an LCP. Given that the UNDO log is used for disk data
 * we don't need to restore the disk pages at first, we simply execute the UNDO
 * log records from the end of the UNDO log until we reach the starting LSN
 * of the LCP. There is an UNDO log entry indicating this.
 *
 * This is implemented in a number of steps where we start to find the head and
 * tail of the UNDO log, then we start reading the UNDO log pages in one
 * parallel process which runs until it runs out of free pages to read more.
 * As soon as new free pages becomes available (through the UNDO execution
 * process) the read undo page process can continue. It sends CONTINUEB
 * signals to itself to continue the process until it completes. When no
 * free pages are around it sends CONTINUEB with a 100msec delay.
 *
 * The UNDO execution process happens in parallel to the read undo pages.
 * As soon as there are UNDO pages to execute it will execute those through
 * use of a DBTUP client. In many cases this will actually send a signal
 * to the proper LDM thread to apply the UNDO log.
 * 
 * TODO:
 *   There is a fairly apparent possibility to improve parallelism in executing
 *   the UNDO log by having multiple outstanding UNDO log records. As long as
 *   they are not directed towards the same page in PGMAN it is safe to send
 *   another UNDO log record. So even with just one instance of LGMAN it is
 *   possible to quite easily keep a number of LDM threads busy applying UNDO
 *   log records.
 *
 * Complications in LGMAN
 * ----------------------
 * There are some practical problems related to implementing the above. We are
 * implementing the above on top of a file system. The only guarantees we get
 * from the file system (and hardly even that) is that all writes issued before
 * an fsync call of a specific file is safe on disk when the fsync call
 * completes. For files in LGMAN all FSWRITEREQ calls are done with fsync
 * integrated as the file is opened with the sync flag set.
 * 
 * This gives us at least the following problems to solve in LGMAN:
 * 1) LGMAN logfile groups can consist of multiple files, we need to ensure
 * that sync_lsn means that before we report any LSN's as written in a new
 * file that all LSN entries of the previous file have been sync:ed.
 * We solve this by special file change logic where we don't report any
 * LSNs as completed in a new file until the previous file have had all its
 * writes completed.
 *
 * 2) We can have a set of pages in LGMAN in unknown state after the last
 * sync in a file. This could be a mix of pages not written at all, pages
 * fully written and even pages that are half-written. It is possible to
 * start executing UNDO log entries all the way from the first written
 * page in the log. If we do must however handle two things, we must ignore
 * all records on unwritten log pages and also on half-written log pages.
 *
 * TODO: For us to discover half-written pages we need a checksum on each
 * page which is currently not supported.
 *
 * We must also ensure that we don't allow any unwritten and half-written
 * log pages once we found the first UNDO log entry that actually needed to
 * be applied. The reason is that when we have found such an entry we know
 * that a sync_lsn up until this log entry was performed since the page
 * had been written only after the sync_lsn returned with success.
 *
 * 3) Finding the last written log pages in this lap is not trivial. At first
 * we can insert new log files at any time. We handle this by performing a
 * sort of the log files at restart based on the first LSN they have recorded
 * in the file. To simplify restart logic we always start by writing only the
 * first page in the file before writing anything more in the file. This means
 * that the file sort only need to look at the first page of a file to be able
 * to sort it in the list of files. Second we ensure that at file change we
 * write the last in the old file and the first page in the new file. This
 * simplifies the restart logic.
 *
 * The above complication is actually only efficiently solvable if we also have
 * a finite limit on how much UNDO log pages we can write before we synch the
 * file. If we synch at least once every e.g. 1 MByte then we're certain that
 * we need not search any further than 1 MByte from the first unwritten page.
 * However if no such limit exists, than we have no way of knowing when to stop
 * searching since e.g. the Linux file system could save as much writes in the
 * file system cache as there is memory in the system (which could be quite
 * substantial amounts in modern machines).
 *
 * To handle this we put a cap on the amount written to the UNDO log per
 * FSWRITEREQ (that includes both an OS file write and an fsync for LGMAN
 * files). We also ensure that at restart we continue scanning ahead
 * as much as the size of this cap, only when we have found a segment of
 * unwritten pages this long after the first unwritten page will we stop
 * the search and point to the page before the first unwritten page as the
 * last written page in the UNDO log.
 *
 * We can also have a set of unwritten/half-written pages in the previous
 * file, but these require no special handling other than skipping them
 * when applying the UNDO log which is part of the normal check for each
 * UNDO page before applying.
 *
 * Unwritten pages are treated in exactly the same fashion as half-written
 * pages (we discover half-written pages through the page checksum being
 * wrong).
 *
 * 4) After we found the last written log page in the UNDO log page we will
 * have to start applying the UNDO log records backwards from this position.
 * If we reach an unwritten/half-written page while proceeding backwards,
 * we need to ignore this page. This is safe since it is part of a set of
 * UNDO log pages which didn't have its synch of the file completed. Thus
 * none of the LSNs can have been applied to any page yet according to the
 * WAL protocol that we follow for the UNDO log.
 *
 * As an extra security effort we keep track of the first UNDO log entry
 * that is actually applied on a page, after we found such a record we cannot
 * find any more unwritten/half-written pages while progressing backwards. If
 * we found an unwritten page/half-written page after this, then there is a
 * real issue and the log is corrupted. We check this condition.
 *
 * Implementation details
 * ----------------------
 *
 * 1) Maintaining free log file space
 * ----------------------------------
 * The amount of free space for an UNDO log file group is maintained by the
 * variable m_free_file_words on the struct Logfile_group.
 *
 * It gets its initial value from either creation of a new log file group
 * in which case this is calculated in create_file_commit. Otherwise it is
 * calculated after executing the UNDO log in stop_run_undo_log.
 *
 * The free space is decremented each time we call alloc_log_space, at creation
 * of special LCP UNDO log records in execEND_LCP_CONF, and exec_lcp_frag_ord.
 * It is also decremented due to changing to a new page where we add a number
 * of NOOP entries to fill all pages, this happens in get_log_buffer when
 * page is full and also when we sync pages in flush_log and force_log_sync.
 *
 * The free space is incremented each time we call free_log_space, in
 * add_entry if we discover that we're not changing file group since last
 * LSN. Most importantly it is incremented in cut_log_tail where we add
 * free space for each page that we move ahead the tail.
 * 
 * So in situations where we get error 1501 (out of UNDO log) we can only
 * get back to a normal situation after completing an LCP and starting a
 * new again immediately.
 *
 * The error 1501 is immediately reported when the log file is full. Log being
 * full here means that all the free space of the file have already been
 * allocated by various transactions. At that point we will report back error
 * 1501 (Out of UNDO log space) to the application. This error will mean that
 * any disk data transactions will be blocked for an extended period of time
 * until a new LCP can free up space again. This is obviously a highly
 * undesirable state and should be avoided by ensuring that the UNDO log
 * is sufficiently big and also by ensuring that LCP write speed is high
 * enough to create new LCPs quicker. This variable is not needed during
 * restart, so it's only used during normal operation.
 *
 * 2) Maintaining free log buffer space
 * ------------------------------------
 * The amount of free UNDO log buffer space is maintained by the variable
 * m_free_buffer_words in the struct Logfile_group.
 *
 * This variable is maintained both at restart and during normal operation.
 * It gets the initial value from the size of the UNDO log buffer which is
 * specified when creating the log file group.
 *
 * During restart we decrement the free space when we allocate a page as
 * initial page, when we allocate to read the UNDO log. We increase the
 * free space when we have completed reading the UNDO log page during
 * UNDO log execution.
 *
 * During normal operation we check the amount of free space in the call
 * get_log_buffer in Logfile_client. If there isn't enough free space the
 * caller must wait for a callback when receiving the return value 1.
 * When a wait is started we check that we have CONTINUEB messages sent
 * with the id PROCESS_LOG_BUFFER_WAITERS, this CONTINUEB calls
 * regularly process_log_buffer_waiters until there are no more waiters.
 *
 * When sending a callback we allocate a list entry that contains the
 * request information needed to send the callback. This list is using
 * memory allocated from the GlobalSharedMemory. This is memory that
 * can be used to some extent by other allocation regions at
 * overallocation. It is used for all data structures describing log buffer
 * requests, all data structures describing page requests when needing to read
 * from disk and finally also for the UNDO log buffer itself.
 *
 * If we have set the config parameter InitialLogFileGroup then the size of the
 * Undo buffer in this specification will be added to the size of
 * GlobalSharedMemory. So in this case the request lists will get the entire
 * GlobalSharedMemory except for its use for overallocation. It's likely over
 * time that more and more resources will be sharing this GlobalSharedMemory,
 * care then needs to be taken that we have sufficient memory to handle these
 * lists. Currently we crash when we run out of this resource.
 *
 * TODO: Ensure that we handle these kind of resource problems in an
 * appropriate manner.
 *
 * In normal operations we decrement the free space when calling flush_log
 * to account for NOOP space, same for force_log_sync, we also decrement
 * it when calling the internal get_log_buffer which is called from
 * add_entry and also other places where we create special UNDO log records.
 * Finally we add back space to the UNDO log buffer when FSWRITECONF returns
 * with the log pages being written and the pages are free to be used for
 * other log pages.
 *
 * Maintaining LSN numbers
 * -----------------------
 * We have a number of LSN variables that are used to maintain the LSNs and
 * their current state:
 * 1) m_next_lsn
 * This is the next LSN that we will write into the UNDO log.
 * This variable exists in two instances. It exists for each logfile group
 * where it represents the last LSN written in this logfile group. It also
 * exists as a global variable for the LGMAN block.
 * Actually LGMAN only supports one log file group, so these numbers will
 * always be equal.
 * 2) m_last_sync_req_lsn
 * This is the highest LSN which is currently in the process of being
 * written to the UNDO log file. The file write of this LSN isn't completed
 * yet.
 * 3) m_last_synced_lsn
 * This is the highest LSN which have been written safely to disk.
 * 4) m_max_sync_req_lsn
 * This is the highest LSN which have been requested for sync to disk by a call
 * to sync_lsn.
 *
 * The condition:
 * m_next_lsn > m_max_sync_req_lsn >= m_last_sync_req_lsn >= m_last_synced_lsn
 * will always be true.
 *
 * Performing a restart (system restart or node restart)
 * -----------------------------------------------------
 * At restart we get started on recovery by receiving the START_RECREQ signal.
 * This signal contains the LCP id that we will restore. Disk data gets its
 * data from only one set of pages since the base information is on disk. The
 * information in LGMAN is used to play the tape backwards figuratively
 * speakin (UNDO) until we reach an UNDO log record that represents this LCP.
 * When we reach this log record we have ensured that all data in the disk
 * data parts are as they were at the time of the LCP. Before completing the
 * UNDO execution we also ensure that all pages in PGMAN are flushed to disk
 * to ensure that the UNDO log we have executed is no longer used. Once we
 * have flushed the PGMAN pages to disk we are done and we can write a new LCP
 * record for the same LCP id. Once this record reaches the disk we will never
 * need to replay the UNDO log already executed. There is no specific write of
 * this record, it will be written as soon as some write of the UNDO log is
 * performed. However if it isn't written before the next crash it isn't a
 * problem since we will simply run through a lot of UNDO logs that have
 * already been applied.
 *
 * The place in the code where we flush the pages in PGMAN is marked by:
 * START_FLUSH_PGMAN_CACHE.
 * The place where we return from flushing PGMAN cache is marked by:
 * END_FLUSH_PGMAN_CACHE.
 *
 * At this point we're done with our part of the restart and we're ready
 * to start generating new UNDO log records which will also happen as
 * part of the processing of REDO log records.
 *
 * At restart we need to discover the following things:
 *
 * 1) We need to sort the UNDO log files in the correct order.
 * This is actually the first step in the restart processing.
 *
 * 2) We need to find the end of the UNDO log.
 * Marked by END_OF_UNDO_LOG_FOUND in code below. We reach this code once for
 * each logfile group defined in the cluster. In most cases we only use one
 * logfile group per cluster which can even be defined in the config file.
 *
 * 3) We need to set up the new head and tail of the UNDO log.
 * The new head is set up to the first non-written UNDO log page and the tail
 * is set up to be the page preceding this (could be in previous file). As we
 * proceed to execute the UNDO log records the tail position will be moved
 * back to its final position.
 *
 * 4) We need to set up the UNDO log such that it starts adding the new
 *    log records at the new end of the UNDO log.
 * 
 * 5) We need to find the next LSN to use for the log records we start to
 *    produce after the restart.
 *
 * 3), 4) and 5) happens when finding the end of the UNDO log.
 *
 * 6) We need to initialise the free space in the buffer and in the files.
 *
 * The most problematic part here is 2).
 * The problem is that we can be in the middle of a file change when we
 * crashed, we can also have large writes to the file system that are in
 * a half-written state. This means that e.g. if the last write to the
 * file system was a write of 128 pages, all of those pages can be in one
 * of 3 states. They can be written, they can be unwritten and they can be
 * half-written. We detect first if they are half-written by using a checksum
 * on the log page. If the checksum was ok we look at the starting LSN to
 * detect whether it was written or not written.
 *
 * Finding the end of the UNDO log means finding the very last of the pages
 * that have been written. If we don't find this log page, then we can end
 * up in a situation where the written pages ahead of the end we found, are
 * put together with new log entries generated after the restart. We could
 * have very complicated bugs in that case which would be more or less
 * impossible to ever detect and find.
 *
 * Also 1) is somewhat tied into 2) and we want this to fairly simple. The
 * sorting happens by reading page 1. To make this searching easier and
 * avoiding that we have to look at more than just page 1, we use a special
 * order of writing at file change.
 * 
 * 1) Write the last pages in file X.
 * 2) Next write page 1 in file X+1.
 * 3) Continue writing as usual in file X+1 as soon as 2) is done.
 * 4) sync_lsn for new file cannot move synced_lsn forward until 1) is done.
 * (Write here means both filesystem write and the fsync to ensure the write
 * saved on disk, or using O_DIRECT flag in file system).
 * The UNDO log file is fixed size after creating it since we start by
 * writing the entire file to ensure that it is allocated on disk. In this
 * case O_DIRECT means that writes behaves as write + fsynch when O_DIRECT
 * flag is set. This is explained in detail in:
 * https://lwn.net/Articles/348739.
 *
 * A special problem to handle here is if we get a half-written page 1. In
 * this case we know that the page must be the next file after the file which
 * has the most recent change. Page 1 will get a correctly written soon after
 * completion of restart since it will soon be the next file to use. We need
 * to crash however if we discover more than one half-written page as page 1.
 * This should never happen unless we have a corrupted file system. This is
 * the case since we will never proceed writing in a file until we have
 * completed writing of page 1, so we can't reach the next file to write
 * before we have completed writing of page 1 in the previous file.
 *
 * Additionally to avoid that we have to search extensive distances for the end
 * of the UNDO log we will set a limit of file writes to 16 MByte as a constant.
 * This means that we need to search 128 pages forward in the file before we
 * can be sure that we have found the end of the UNDO log. We never need to
 * bother searching beyond end of file into the next file due to the file
 * change protocol we are using.
 *
 * While executing the UNDO log backwards we need to look out for unwritten
 * pages and half-written pages. No UNDO records from these pages should be
 * applied. Also we need to get a flag from PGMAN when we reach the first
 * UNDO log record which is actually applied. This UNDO log record represents
 * a point in the UNDO log where we are sure that the LSN must have been
 * sync:ed to this point since the page had been forced to disk which only
 * happens after the log have been sync:ed to disk according to the WAL
 * protocol. So when continuing backwards in the UNDO log file we should not
 * encounter any more unwritten pages or half-written pages. Encountering such
 * a page is an indication of a corrupt file system and thus we cannot proceed
 * with the restart.
 *
 * When looking for end of UNDO log we use the following state variables what
 * we are currently doing:
 *
 * FS_SEARCHING : Binary search
 * FS_SEARCHING_END : Forward linear search bounded by 16MB 'rule'
 * FS_SEARCHING_FINAL_READ : Search completed, re-reading 'final' page before
 *                           applying UNDO log.
 *
 * LSNs are only recorded per page and this represents the last LSN written in
 * this page. So the page_lsn is the highest LSN represented in the file, then
 * the UNDO log before the last record has its LSN implied unless the UNDO log
 * records is a special log record that also stores the LSN.
 */

#define DEBUG_UNDO_EXECUTION 0
#define DEBUG_SEARCH_LOG_HEAD 0

#define FREE_BUFFER_MARGIN (2 * File_formats::UNDO_PAGE_WORDS)

Lgman::Lgman(Block_context & ctx) :
  SimulatedBlock(LGMAN, ctx),
  m_tup(0),
  m_logfile_group_list(m_logfile_group_pool),
  m_logfile_group_hash(m_logfile_group_pool),
  m_client_mutex("lgman-client", 2, true)
{
  BLOCK_CONSTRUCTOR(Lgman);
  
  // Add received signals
  addRecSignal(GSN_STTOR, &Lgman::execSTTOR);
  addRecSignal(GSN_READ_CONFIG_REQ, &Lgman::execREAD_CONFIG_REQ);
  addRecSignal(GSN_DUMP_STATE_ORD, &Lgman::execDUMP_STATE_ORD);
  addRecSignal(GSN_DBINFO_SCANREQ, &Lgman::execDBINFO_SCANREQ);
  addRecSignal(GSN_CONTINUEB, &Lgman::execCONTINUEB);
  addRecSignal(GSN_NODE_FAILREP, &Lgman::execNODE_FAILREP);

  addRecSignal(GSN_CREATE_FILE_IMPL_REQ, &Lgman::execCREATE_FILE_IMPL_REQ);
  addRecSignal(GSN_CREATE_FILEGROUP_IMPL_REQ,
               &Lgman::execCREATE_FILEGROUP_IMPL_REQ);

  addRecSignal(GSN_DROP_FILE_IMPL_REQ, &Lgman::execDROP_FILE_IMPL_REQ);
  addRecSignal(GSN_DROP_FILEGROUP_IMPL_REQ,
               &Lgman::execDROP_FILEGROUP_IMPL_REQ);

  addRecSignal(GSN_FSWRITEREQ, &Lgman::execFSWRITEREQ);
  addRecSignal(GSN_FSWRITEREF, &Lgman::execFSWRITEREF, true);
  addRecSignal(GSN_FSWRITECONF, &Lgman::execFSWRITECONF);

  addRecSignal(GSN_FSOPENREF, &Lgman::execFSOPENREF, true);
  addRecSignal(GSN_FSOPENCONF, &Lgman::execFSOPENCONF);

  addRecSignal(GSN_FSCLOSECONF, &Lgman::execFSCLOSECONF);
  
  addRecSignal(GSN_FSREADREF, &Lgman::execFSREADREF, true);
  addRecSignal(GSN_FSREADCONF, &Lgman::execFSREADCONF);

  addRecSignal(GSN_END_LCPREQ, &Lgman::execEND_LCPREQ);
  addRecSignal(GSN_SUB_GCP_COMPLETE_REP, &Lgman::execSUB_GCP_COMPLETE_REP);
  addRecSignal(GSN_START_RECREQ, &Lgman::execSTART_RECREQ);
  
  addRecSignal(GSN_END_LCPCONF, &Lgman::execEND_LCPCONF);

  addRecSignal(GSN_GET_TABINFOREQ, &Lgman::execGET_TABINFOREQ);
  addRecSignal(GSN_CALLBACK_ACK, &Lgman::execCALLBACK_ACK);

  m_next_lsn = 1;
  m_logfile_group_hash.setSize(10);

  if (isNdbMtLqh()) {
    jam();
    int ret = m_client_mutex.create();
    ndbrequire(ret == 0);
  }

  {
    CallbackEntry& ce = m_callbackEntry[THE_NULL_CALLBACK];
    ce.m_function = TheNULLCallback.m_callbackFunction;
    ce.m_flags = 0;
  }
  {
    CallbackEntry& ce = m_callbackEntry[ENDLCP_CALLBACK];
    ce.m_function = safe_cast(&Lgman::endlcp_callback);
    ce.m_flags = 0;
  }
  {
    CallbackTable& ct = m_callbackTable;
    ct.m_count = COUNT_CALLBACKS;
    ct.m_entry = m_callbackEntry;
    m_callbackTableAddr = &ct;
  }
}
  
Lgman::~Lgman()
{
  if (isNdbMtLqh()) {
    (void)m_client_mutex.destroy();
  }
}

void
Lgman::client_lock(BlockNumber block, int line)
{
  if (isNdbMtLqh()) {
#ifdef VM_TRACE
    Uint32 bno = blockToMain(block);
    Uint32 ino = blockToInstance(block);
#endif
    D("try lock " << bno << "/" << ino << V(line));
    int ret = m_client_mutex.lock();
    ndbrequire(ret == 0);
    D("got lock " << bno << "/" << ino << V(line));
  }
}

void
Lgman::client_unlock(BlockNumber block, int line)
{
  if (isNdbMtLqh()) {
#ifdef VM_TRACE
    Uint32 bno = blockToMain(block);
    Uint32 ino = blockToInstance(block);
#endif
    D("unlock " << bno << "/" << ino << V(line));
    int ret = m_client_mutex.unlock();
    ndbrequire(ret == 0);
  }
}

BLOCK_FUNCTIONS(Lgman)

void 
Lgman::execREAD_CONFIG_REQ(Signal* signal)
{
  jamEntry();

  const ReadConfigReq * req = (ReadConfigReq*)signal->getDataPtr();

  Uint32 ref = req->senderRef;
  Uint32 senderData = req->senderData;

  const ndb_mgm_configuration_iterator * p = 
    m_ctx.m_config.getOwnConfigIterator();
  ndbrequire(p != 0);

  Pool_context pc;
  pc.m_block = this;
  m_log_waiter_pool.wo_pool_init(RT_LGMAN_LOG_WAITER, pc);
  m_file_pool.init(RT_LGMAN_FILE, pc);
  m_logfile_group_pool.init(RT_LGMAN_FILEGROUP, pc);
  // 10 -> 150M
  m_data_buffer_pool.setSize(40);

  ReadConfigConf * conf = (ReadConfigConf*)signal->getDataPtrSend();
  conf->senderRef = reference();
  conf->senderData = senderData;
  sendSignal(ref, GSN_READ_CONFIG_CONF, signal, 
	     ReadConfigConf::SignalLength, JBB);
}

void
Lgman::execSTTOR(Signal* signal) 
{
  jamEntry();                            
  Uint32 startPhase = signal->theData[1];
  switch (startPhase) {
  case 1:
    jam();
    m_tup = globalData.getBlock(DBTUP);
    ndbrequire(m_tup != 0);
    break;
  }
  sendSTTORRY(signal);
}

void
Lgman::sendSTTORRY(Signal* signal)
{
  signal->theData[0] = 0;
  signal->theData[3] = 1;
  signal->theData[4] = 255; // No more start phases from missra
  sendSignal(NDBCNTR_REF, GSN_STTORRY, signal, 5, JBB);
}

void
Lgman::execCONTINUEB(Signal* signal){
  jamEntry();

  Uint32 type= signal->theData[0];
  Uint32 ptrI = signal->theData[1];
  client_lock(number(), __LINE__);
  switch(type){
  case LgmanContinueB::FILTER_LOG:
    jam();
    break;
  case LgmanContinueB::CUT_LOG_TAIL:
  {
    jam();
    Ptr<Logfile_group> ptr;
    m_logfile_group_pool.getPtr(ptr, ptrI);
    cut_log_tail(signal, ptr);
    break;
  }
  case LgmanContinueB::FLUSH_LOG:
  {
    jam();
    Ptr<Logfile_group> ptr;
    m_logfile_group_pool.getPtr(ptr, ptrI);
    flush_log(signal, ptr, signal->theData[2]);
    break;
  }
  case LgmanContinueB::PROCESS_LOG_BUFFER_WAITERS:
  {
    jam();
    Ptr<Logfile_group> ptr;
    m_logfile_group_pool.getPtr(ptr, ptrI);
    process_log_buffer_waiters(signal, ptr);
    break;
  }
  case LgmanContinueB::FIND_LOG_HEAD:
  {
    jam();
    Ptr<Logfile_group> ptr;
    if(ptrI != RNIL)
    {
      jam();
      m_logfile_group_pool.getPtr(ptr, ptrI);
      find_log_head(signal, ptr);
    }
    else
    {
      jam();
      init_run_undo_log(signal);
    }
    break;
  }
  case LgmanContinueB::EXECUTE_UNDO_RECORD:
    jam();
    {
      Ptr<Logfile_group> ptr;
      m_logfile_group_list.first(ptr);
      if (signal->theData[1] == 1 && !ptr.p->m_applied)
      {
        /**
         * The variable m_applied is set the first UNDO log record which is
         * applied, we signal if an UNDO log record was applied in the
         * CONTINUEB signal to execute the next UNDO log record.
         */
        jam();
        ptr.p->m_applied = true;
      }
    }
    execute_undo_record(signal);
    break;
  case LgmanContinueB::STOP_UNDO_LOG:
    jam();
    stop_run_undo_log(signal);
    break;
  case LgmanContinueB::READ_UNDO_LOG:
  {
    jam();
    Ptr<Logfile_group> ptr;
    m_logfile_group_pool.getPtr(ptr, ptrI);
    read_undo_log(signal, ptr);
    break;
  }
  case LgmanContinueB::PROCESS_LOG_SYNC_WAITERS:
  {
    jam();
    Ptr<Logfile_group> ptr;
    m_logfile_group_pool.getPtr(ptr, ptrI);
    process_log_sync_waiters(signal, ptr);
    break;
  }
  case LgmanContinueB::FORCE_LOG_SYNC:
  {
    jam();
    Ptr<Logfile_group> ptr;
    m_logfile_group_pool.getPtr(ptr, ptrI);
    force_log_sync(signal, ptr, signal->theData[2], signal->theData[3]);
    break;
  }
  case LgmanContinueB::DROP_FILEGROUP:
  {
    jam();
    Ptr<Logfile_group> ptr;
    m_logfile_group_pool.getPtr(ptr, ptrI);
    if ((ptr.p->m_state & Logfile_group::LG_THREAD_MASK) ||
        ptr.p->m_outstanding_fs > 0)
    {
      jam();
      sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 100, 
			  signal->length());
      break;
    }
    Uint32 ref = signal->theData[2];
    Uint32 data = signal->theData[3];
    drop_filegroup_drop_files(signal, ptr, ref, data);
    break;
  }
  }
  client_unlock(number(), __LINE__);
}

void
Lgman::execNODE_FAILREP(Signal* signal)
{
  jamEntry();
  const NodeFailRep * rep = (NodeFailRep*)signal->getDataPtr();
  NdbNodeBitmask failed; 
  failed.assign(NdbNodeBitmask::Size, rep->theNodes);

  /* Block level cleanup */
  for(unsigned i = 1; i < MAX_NDB_NODES; i++) {
    jam();
    if(failed.get(i)) {
      jam();
      Uint32 elementsCleaned = simBlockNodeFailure(signal, i); // No callback
      ndbassert(elementsCleaned == 0); // No distributed fragmented signals
      (void) elementsCleaned; // Remove compiler warning
    }//if
  }//for
}

void
Lgman::execDUMP_STATE_ORD(Signal* signal){
  jamNoBlock();  /* Due to bug#20135976 */
  if (signal->theData[0] == 12001 || signal->theData[0] == 12002)
  {
    char tmp[1024];
    Ptr<Logfile_group> ptr;
    m_logfile_group_list.first(ptr);
    while(!ptr.isNull())
    {
      BaseString::snprintf(tmp, sizeof(tmp),
                           "lfg %u state: %x fs: %u lsn "
                           " [ next: %llu s(req): %llu s:ed: %llu lcp: %llu ] "
                           " waiters: %d %d",
                           ptr.p->m_logfile_group_id, ptr.p->m_state,
                           ptr.p->m_outstanding_fs,
                           ptr.p->m_next_lsn, ptr.p->m_last_sync_req_lsn,
                           ptr.p->m_last_synced_lsn, ptr.p->m_last_lcp_lsn,
                           !ptr.p->m_log_buffer_waiters.isEmpty(),
                           !ptr.p->m_log_sync_waiters.isEmpty());
      if (signal->theData[0] == 12001)
        infoEvent("%s", tmp);
      ndbout_c("%s", tmp);

      BaseString::snprintf(tmp, sizeof(tmp),
                           "   callback_buffer_words: %u"
                           " free_buffer_words: %u free_file_words: %llu",
                           ptr.p->m_callback_buffer_words,
                           ptr.p->m_free_buffer_words,
                           ptr.p->m_free_file_words);
      if (signal->theData[0] == 12001)
        infoEvent("%s", tmp);
      ndbout_c("%s", tmp);
      if (!ptr.p->m_log_buffer_waiters.isEmpty())
      {
	Ptr<Log_waiter> waiter;
	Local_log_waiter_list 
	  list(m_log_waiter_pool, ptr.p->m_log_buffer_waiters);
	list.first(waiter);
        BaseString::snprintf(tmp, sizeof(tmp),
                             "  head(waiters).sz: %u %u",
                             waiter.p->m_size,
                             FREE_BUFFER_MARGIN);
        if (signal->theData[0] == 12001)
          infoEvent("%s", tmp);
        ndbout_c("%s", tmp);
      }
      if (!ptr.p->m_log_sync_waiters.isEmpty())
      {
	Ptr<Log_waiter> waiter;
	Local_log_waiter_list 
	  list(m_log_waiter_pool, ptr.p->m_log_sync_waiters);
	list.first(waiter);
        BaseString::snprintf(tmp, sizeof(tmp),
                             "  m_last_synced_lsn: %llu head(waiters %x).m_sync_lsn: %llu",
                             ptr.p->m_last_synced_lsn,
                             waiter.i,
                             waiter.p->m_sync_lsn);
        if (signal->theData[0] == 12001)
          infoEvent("%s", tmp);
        ndbout_c("%s", tmp);
	
	while(!waiter.isNull())
	{
	  ndbout_c("ptr: %x %p lsn: %llu next: %x",
		   waiter.i, waiter.p, waiter.p->m_sync_lsn, waiter.p->nextList);
	  list.next(waiter);
	}
      }
      m_logfile_group_list.next(ptr);
    }
  }
  if (signal->theData[0] == 12003)
  {
    bool crash = false;
    Ptr<Logfile_group> ptr;
    for (m_logfile_group_list.first(ptr); !ptr.isNull();
         m_logfile_group_list.next(ptr))
    {
      if (ptr.p->m_callback_buffer_words != 0)
      {
        crash = true;
        break;
      }
    }

    if (crash)
    {
      ndbout_c("Detected logfile-group with non zero m_callback_buffer_words");
      signal->theData[0] = 12002;
      execDUMP_STATE_ORD(signal);
      ndbrequire(false);
    }
#ifdef VM_TRACE
    else
    {
      ndbout_c("Check for non zero m_callback_buffer_words OK!");
    }
#endif
  }
}

void
Lgman::execDBINFO_SCANREQ(Signal *signal)
{
  DbinfoScanReq req= *(DbinfoScanReq*)signal->theData;
  const Ndbinfo::ScanCursor* cursor = 
    CAST_CONSTPTR(Ndbinfo::ScanCursor, DbinfoScan::getCursorPtr(&req));
  Ndbinfo::Ratelimit rl;

  jamEntry();

  switch(req.tableId) {
  case Ndbinfo::LOGSPACES_TABLEID:
  {
    jam();
    Uint32 startBucket = cursor->data[0];
    Logfile_group_hash_iterator iter;
    m_logfile_group_hash.next(startBucket, iter);

    while (!iter.curr.isNull())
    {
      jam();

      Uint32 currentBucket = iter.bucket;
      Ptr<Logfile_group> ptr = iter.curr;

      Uint64 free = ptr.p->m_free_file_words*4;

      Uint64 total = 0;
      Local_undofile_list list(m_file_pool, ptr.p->m_files);
      Ptr<Undofile> filePtr;
      for (list.first(filePtr); !filePtr.isNull(); list.next(filePtr))
      {
        jam();
        total += (Uint64)filePtr.p->m_file_size *
          (Uint64)File_formats::NDB_PAGE_SIZE;
      }

      Uint64 high = 0; // TODO

      Ndbinfo::Row row(signal, req);
      row.write_uint32(getOwnNodeId());
      row.write_uint32(1); // log type, 1 = DD-UNDO
      row.write_uint32(ptr.p->m_logfile_group_id); // log id
      row.write_uint32(0); // log part

      row.write_uint64(total);          // total allocated
      row.write_uint64((total-free));   // currently in use
      row.write_uint64(high);           // in use high water mark
      ndbinfo_send_row(signal, req, row, rl);

      // move to next
      if (m_logfile_group_hash.next(iter) == false)
      {
        jam(); // no more...
        break;
      }
      else if (iter.bucket == currentBucket)
      {
        jam();
        continue; // we need to iterate an entire bucket
      }
      else if (rl.need_break(req))
      {
        jam();
        ndbinfo_send_scan_break(signal, req, rl, iter.bucket);
        return;
      }
    }
    break;
  }

  case Ndbinfo::LOGBUFFERS_TABLEID:
  {
    jam();
    Uint32 startBucket = cursor->data[0];
    Logfile_group_hash_iterator iter;
    m_logfile_group_hash.next(startBucket, iter);

    while (!iter.curr.isNull())
    {
      jam();

      Uint32 currentBucket = iter.bucket;
      Ptr<Logfile_group> ptr = iter.curr;

      Uint64 free = ptr.p->m_free_buffer_words*4;
      Uint64 total = ptr.p->m_total_buffer_words*4;
      Uint64 high = 0; // TODO

      Ndbinfo::Row row(signal, req);
      row.write_uint32(getOwnNodeId());
      row.write_uint32(1); // log type, 1 = DD-UNDO
      row.write_uint32(ptr.p->m_logfile_group_id); // log id
      row.write_uint32(0); // log part

      row.write_uint64(total);          // total allocated
      row.write_uint64((total-free));   // currently in use
      row.write_uint64(high);           // in use high water mark
      ndbinfo_send_row(signal, req, row, rl);

      // move to next
      if (m_logfile_group_hash.next(iter) == false)
      {
        jam(); // no more...
        break;
      }
      else if (iter.bucket == currentBucket)
      {
        jam();
        continue; // we need to iterate an entire bucket
      }
      else if (rl.need_break(req))
      {
        jam();
        ndbinfo_send_scan_break(signal, req, rl, iter.bucket);
        return;
      }
    }
    break;
  }

  default:
    break;
  }

  ndbinfo_send_scan_conf(signal, req, rl);
}

/**
 * Create a new LOGFILE GROUP. This can either happen as part of DICT creating
 * object for the first time or it could happen as part of a restart. We
 * set the state to LG_ONLINE when it is created for the first time and set
 * it to LG_STARTING in case where we are restarting and will need the
 * logfile group to execute the UNDO log on the disk data parts.
 *
 * We currently have a limit on only creating one log file group, so the
 * list of log file groups is always empty or contains one element in the
 * list.
 */
void
Lgman::execCREATE_FILEGROUP_IMPL_REQ(Signal* signal){
  jamEntry();
  CreateFilegroupImplReq* req= (CreateFilegroupImplReq*)signal->getDataPtr();

  Uint32 senderRef = req->senderRef;
  Uint32 senderData = req->senderData;
  
  Ptr<Logfile_group> ptr;
  CreateFilegroupImplRef::ErrorCode err = CreateFilegroupImplRef::NoError;
  do {
    if (m_logfile_group_hash.find(ptr, req->filegroup_id))
    {
      jam();
      err = CreateFilegroupImplRef::FilegroupAlreadyExists;
      break;
    }
    
    if (!m_logfile_group_list.isEmpty())
    {
      jam();
      err = CreateFilegroupImplRef::OneLogfileGroupLimit;
      break;
    }

    if (!m_logfile_group_pool.seize(ptr))
    {
      jam();
      err = CreateFilegroupImplRef::OutOfFilegroupRecords;
      break;
    }

    new (ptr.p) Logfile_group(req);
    
    if (!alloc_logbuffer_memory(ptr, req->logfile_group.buffer_size))
    {
      jam();
      err= CreateFilegroupImplRef::OutOfLogBufferMemory;
      m_logfile_group_pool.release(ptr);
      break;
    }
    
    m_logfile_group_hash.add(ptr);
    m_logfile_group_list.addLast(ptr);

    if ((getNodeState().getNodeRestartInProgress() &&
         getNodeState().starting.restartType !=
         NodeState::ST_INITIAL_NODE_RESTART)||
        getNodeState().getSystemRestartInProgress())
    {
      jam();
      ptr.p->m_state = Logfile_group::LG_STARTING;
    }
    
    CreateFilegroupImplConf* conf= 
      (CreateFilegroupImplConf*)signal->getDataPtr();
    conf->senderData = senderData;
    conf->senderRef = reference();
    sendSignal(senderRef, GSN_CREATE_FILEGROUP_IMPL_CONF, signal,
	       CreateFilegroupImplConf::SignalLength, JBB);
    
    return;
  } while(0);
  
  CreateFilegroupImplRef* ref= (CreateFilegroupImplRef*)signal->getDataPtr();
  ref->senderData = senderData;
  ref->senderRef = reference();
  ref->errorCode = err;
  sendSignal(senderRef, GSN_CREATE_FILEGROUP_IMPL_REF, signal,
	     CreateFilegroupImplRef::SignalLength, JBB);
}

void
Lgman::execDROP_FILEGROUP_IMPL_REQ(Signal* signal)
{
  jamEntry();

  Uint32 errorCode = 0;
  DropFilegroupImplReq req = *(DropFilegroupImplReq*)signal->getDataPtr();  
  do 
  {
    jam();
    Ptr<Logfile_group> ptr;
    if (!m_logfile_group_hash.find(ptr, req.filegroup_id))
    {
      jam();
      errorCode = DropFilegroupImplRef::NoSuchFilegroup;
      break;
    }
    
    if (ptr.p->m_version != req.filegroup_version)
    {
      jam();
      errorCode = DropFilegroupImplRef::InvalidFilegroupVersion;
      break;
    }
    
    switch(req.requestInfo){
    case DropFilegroupImplReq::Prepare:
      jam();
      break;
    case DropFilegroupImplReq::Commit:
      jam();
      m_logfile_group_list.remove(ptr);
      ptr.p->m_state |= Logfile_group::LG_DROPPING;
      signal->theData[0] = LgmanContinueB::DROP_FILEGROUP;
      signal->theData[1] = ptr.i;
      signal->theData[2] = req.senderRef;
      signal->theData[3] = req.senderData;
      sendSignal(reference(), GSN_CONTINUEB, signal, 4, JBB);
      return;
    case DropFilegroupImplReq::Abort:
      jam();
      break;
    default:
      ndbrequire(false);
    }
  } while(0);
  
  if (errorCode)
  {
    jam();
    DropFilegroupImplRef* ref = 
      (DropFilegroupImplRef*)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->senderData = req.senderData;
    ref->errorCode = errorCode;
    sendSignal(req.senderRef, GSN_DROP_FILEGROUP_IMPL_REF, signal,
	       DropFilegroupImplRef::SignalLength, JBB);
  }
  else
  {
    jam();
    DropFilegroupImplConf* conf = 
      (DropFilegroupImplConf*)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->senderData = req.senderData;
    sendSignal(req.senderRef, GSN_DROP_FILEGROUP_IMPL_CONF, signal,
	       DropFilegroupImplConf::SignalLength, JBB);
  }
}

void
Lgman::drop_filegroup_drop_files(Signal* signal,
				 Ptr<Logfile_group> ptr,
				 Uint32 ref, Uint32 data)
{
  jam();
  ndbrequire(! (ptr.p->m_state & Logfile_group::LG_THREAD_MASK));
  ndbrequire(ptr.p->m_outstanding_fs == 0);

  Local_undofile_list list(m_file_pool, ptr.p->m_files);
  Ptr<Undofile> file_ptr;

  if (list.first(file_ptr))
  {
    jam();
    ndbrequire(! (file_ptr.p->m_state & Undofile::FS_OUTSTANDING));
    file_ptr.p->m_create.m_senderRef = ref;
    file_ptr.p->m_create.m_senderData = data;
    create_file_abort(signal, ptr, file_ptr);
    return;
  }

  Local_undofile_list metalist(m_file_pool, ptr.p->m_meta_files);
  if (metalist.first(file_ptr))
  {
    jam();
    metalist.remove(file_ptr);
    list.addLast(file_ptr);
    file_ptr.p->m_create.m_senderRef = ref;
    file_ptr.p->m_create.m_senderData = data;
    create_file_abort(signal, ptr, file_ptr);
    return;
  }

  free_logbuffer_memory(ptr);
  m_logfile_group_hash.release(ptr);
  DropFilegroupImplConf *conf = (DropFilegroupImplConf*)signal->getDataPtr();  
  conf->senderData = data;
  conf->senderRef = reference();
  sendSignal(ref, GSN_DROP_FILEGROUP_IMPL_CONF, signal,
	     DropFilegroupImplConf::SignalLength, JBB);
}

/**
 * Request to create/open a file as part of a log group. This is performed
 * as part of a metadata transaction. This means that we start by opening
 * or creating the file and then responding back to DBDICT. If DICT decides
 * to commit it sends a new request with commit flag and likewise if it
 * decides to abort it will send a new CREATE_FILE_IMPL_REQ signal, but with
 * a abort flag.
 *
 * If the file is created as part of creating a new log file group or extending
 * an existing log file group, then the file needs to be created. When this
 * happens as part of a restart, it is sufficient to open the file since the
 * file already exists.
 */
void
Lgman::execCREATE_FILE_IMPL_REQ(Signal* signal)
{
  jamEntry();
  CreateFileImplReq* req= (CreateFileImplReq*)signal->getDataPtr();
  
  Uint32 senderRef = req->senderRef;
  Uint32 senderData = req->senderData;
  Uint32 requestInfo = req->requestInfo;
  
  Ptr<Logfile_group> ptr;
  CreateFileImplRef::ErrorCode err = CreateFileImplRef::NoError;
  SectionHandle handle(this, signal);
  do {
    if (!m_logfile_group_hash.find(ptr, req->filegroup_id))
    {
      jam();
      err = CreateFileImplRef::InvalidFilegroup;
      break;
    }

    if (ptr.p->m_version != req->filegroup_version)
    {
      jam();
      err = CreateFileImplRef::InvalidFilegroupVersion;
      break;
    }

    Ptr<Undofile> file_ptr;
    switch(requestInfo){
    case CreateFileImplReq::Commit:
    {
      jam();
      ndbrequire(find_file_by_id(file_ptr, ptr.p->m_meta_files, req->file_id));
      file_ptr.p->m_create.m_senderRef = req->senderRef;
      file_ptr.p->m_create.m_senderData = req->senderData;
      create_file_commit(signal, ptr, file_ptr);
      return;
    }
    case CreateFileImplReq::Abort:
    {
      Uint32 senderRef = req->senderRef;
      Uint32 senderData = req->senderData;
      if (find_file_by_id(file_ptr, ptr.p->m_meta_files, req->file_id))
      {
        jam();
	file_ptr.p->m_create.m_senderRef = senderRef;
	file_ptr.p->m_create.m_senderData = senderData;
	create_file_abort(signal, ptr, file_ptr);
      }
      else
      {
	CreateFileImplConf* conf= (CreateFileImplConf*)signal->getDataPtr();
        jam();
	conf->senderData = senderData;
	conf->senderRef = reference();
	sendSignal(senderRef, GSN_CREATE_FILE_IMPL_CONF, signal,
		   CreateFileImplConf::SignalLength, JBB);
      }
      return;
    }
    default: // prepare
      jam();
      break;
    }
    
    if (!m_file_pool.seize(file_ptr))
    {
      jam();
      err = CreateFileImplRef::OutOfFileRecords;
      break;
    }

    ndbrequire(handle.m_cnt > 0);
    
    if (ERROR_INSERTED(15000) ||
        (sizeof(void*) == 4 && req->file_size_hi & 0xFFFFFFFF))
    {
      jam();
      err = CreateFileImplRef::FileSizeTooLarge;
      break;
    }
    
    Uint64 sz = (Uint64(req->file_size_hi) << 32) + req->file_size_lo;
    if (sz < 1024*1024)
    {
      jam();
      err = CreateFileImplRef::FileSizeTooSmall;
      break;
    }

    new (file_ptr.p) Undofile(req, ptr.i);

    Local_undofile_list tmp(m_file_pool, ptr.p->m_meta_files);
    tmp.addLast(file_ptr);
    
    open_file(signal, file_ptr, req->requestInfo, &handle);
    return;
  } while(0);

  releaseSections(handle);
  CreateFileImplRef* ref= (CreateFileImplRef*)signal->getDataPtr();
  ref->senderData = senderData;
  ref->senderRef = reference();
  ref->errorCode = err;
  sendSignal(senderRef, GSN_CREATE_FILE_IMPL_REF, signal,
	     CreateFileImplRef::SignalLength, JBB);
}

void
Lgman::open_file(Signal* signal, Ptr<Undofile> ptr,
		 Uint32 requestInfo,
		 SectionHandle * handle)
{
  FsOpenReq* req = (FsOpenReq*)signal->getDataPtrSend();
  req->userReference = reference();
  req->userPointer = ptr.i;
  
  memset(req->fileNumber, 0, sizeof(req->fileNumber));
  FsOpenReq::setVersion(req->fileNumber, 4); // Version 4 = specified filename
  FsOpenReq::v4_setBasePath(req->fileNumber, FsOpenReq::BP_DD_UF);

  req->fileFlags = 0;
  req->fileFlags |= FsOpenReq::OM_READWRITE;
  req->fileFlags |= FsOpenReq::OM_DIRECT;
  req->fileFlags |= FsOpenReq::OM_SYNC;
  switch(requestInfo){
  case CreateFileImplReq::Create:
    jam();
    req->fileFlags |= FsOpenReq::OM_CREATE_IF_NONE;
    req->fileFlags |= FsOpenReq::OM_INIT;
    ptr.p->m_state = Undofile::FS_CREATING;
    break;
  case CreateFileImplReq::CreateForce:
    jam();
    req->fileFlags |= FsOpenReq::OM_CREATE;
    req->fileFlags |= FsOpenReq::OM_INIT;
    ptr.p->m_state = Undofile::FS_CREATING;
    break;
  case CreateFileImplReq::Open:
    jam();
    req->fileFlags |= FsOpenReq::OM_CHECK_SIZE;
    ptr.p->m_state = Undofile::FS_OPENING;
    break;
  default:
    ndbrequire(false);
  }

  req->page_size = File_formats::NDB_PAGE_SIZE;
  Uint64 size = (Uint64)ptr.p->m_file_size * (Uint64)File_formats::NDB_PAGE_SIZE;
  req->file_size_hi = (Uint32)(size >> 32);
  req->file_size_lo = (Uint32)(size & 0xFFFFFFFF);

  sendSignal(NDBFS_REF, GSN_FSOPENREQ, signal, FsOpenReq::SignalLength, JBB,
	     handle);
}

/**
 * This code is called during initialisation of the file to ensure that the
 * file content is properly set when the file is created, it is a direct
 * function call via block methods from the file system thread into this
 * block. So this means that we are not allowed to change any block variables
 * and even for reading we have to be careful. The pages are allocated in
 * NDBFS from the DataMemory in DBTUP. So these pages we are allowed to
 * change since they are owned at this moment by the NDB file system thread.
 */
void
Lgman::execFSWRITEREQ(Signal* signal)
{
  jamNoBlock();
  Ptr<Undofile> ptr;
  Ptr<GlobalPage> page_ptr;
  FsReadWriteReq* req= (FsReadWriteReq*)signal->getDataPtr();
  
  m_file_pool.getPtr(ptr, req->userPointer);
  m_shared_page_pool.getPtr(page_ptr, req->data.pageData[0]);

  if (req->varIndex == 0)
  {
    File_formats::Undofile::Zero_page* page = 
      (File_formats::Undofile::Zero_page*)page_ptr.p;
    page->m_page_header.init(File_formats::FT_Undofile, 
			     getOwnNodeId(),
			     ndbGetOwnVersion(),
			     (Uint32)time(0));
    page->m_file_id = ptr.p->m_file_id;
    page->m_logfile_group_id = ptr.p->m_create.m_logfile_group_id;
    page->m_logfile_group_version = ptr.p->m_create.m_logfile_group_version;
    page->m_undo_pages = ptr.p->m_file_size - 1; // minus zero page
  }
  else if (req->varIndex == 1)
  {
    /**
     * We write an UNDO END log record into the very first page. This is to
     * ensure that we don't pass this point if we crash before completing
     * the first synch to the UNDO log. Without this log record we could
     * have written other pages but not this page. In that case we would
     * have no way to distinguish when we find the end of the UNDO log.
     */
    File_formats::Undofile::Undo_page* page = 
      (File_formats::Undofile::Undo_page*)page_ptr.p;
    page->m_page_header.m_page_lsn_hi = 0;
    page->m_page_header.m_page_lsn_lo = 0;
    page->m_words_used = 1;
    page->m_data[0] = (File_formats::Undofile::UNDO_END << 16) | 1 ;
    page->m_page_header.m_page_type = File_formats::PT_Undopage;
  }
  else
  {
    File_formats::Undofile::Undo_page* page = 
      (File_formats::Undofile::Undo_page*)page_ptr.p;
    page->m_page_header.m_page_lsn_hi = 0;
    page->m_page_header.m_page_lsn_lo = 0;
    page->m_page_header.m_page_type = File_formats::PT_Undopage;
    page->m_words_used = 0;
  }
}

void
Lgman::execFSOPENREF(Signal* signal)
{
  jamNoBlock();

  Ptr<Undofile> ptr;  
  Ptr<Logfile_group> lg_ptr;
  FsRef* ref = (FsRef*)signal->getDataPtr();

  Uint32 errCode = ref->errorCode;
  Uint32 osErrCode = ref->osErrorCode;

  m_file_pool.getPtr(ptr, ref->userPointer);
  m_logfile_group_pool.getPtr(lg_ptr, ptr.p->m_logfile_group_ptr_i);

  {
    CreateFileImplRef* ref= (CreateFileImplRef*)signal->getDataPtr();
    ref->senderData = ptr.p->m_create.m_senderData;
    ref->senderRef = reference();
    ref->errorCode = CreateFileImplRef::FileError;
    ref->fsErrCode = errCode;
    ref->osErrCode = osErrCode;

    sendSignal(ptr.p->m_create.m_senderRef, GSN_CREATE_FILE_IMPL_REF, signal,
	       CreateFileImplRef::SignalLength, JBB);
  }

  Local_undofile_list meta(m_file_pool, lg_ptr.p->m_meta_files);
  meta.release(ptr);
}

#define HEAD 0
#define TAIL 1

void
Lgman::execFSOPENCONF(Signal* signal)
{
  jamEntry();
  Ptr<Undofile> ptr;  

  FsConf* conf = (FsConf*)signal->getDataPtr();
  
  Uint32 fd = conf->filePointer;
  m_file_pool.getPtr(ptr, conf->userPointer);

  ptr.p->m_fd = fd;

  {
    Uint32 senderRef = ptr.p->m_create.m_senderRef;
    Uint32 senderData = ptr.p->m_create.m_senderData;
    
    CreateFileImplConf* conf= (CreateFileImplConf*)signal->getDataPtr();
    conf->senderData = senderData;
    conf->senderRef = reference();
    sendSignal(senderRef, GSN_CREATE_FILE_IMPL_CONF, signal,
	       CreateFileImplConf::SignalLength, JBB);
  }
}

bool 
Lgman::find_file_by_id(Ptr<Undofile>& ptr, 
		       Local_undofile_list::Head& head, Uint32 id)
{
  Local_undofile_list list(m_file_pool, head);
  for(list.first(ptr); !ptr.isNull(); list.next(ptr))
  {
    jam();
    if(ptr.p->m_file_id == id)
    {
      jam();
      return true;
    }
  }
  return false;
}

void
Lgman::create_file_commit(Signal* signal, 
			  Ptr<Logfile_group> lg_ptr, 
			  Ptr<Undofile> ptr)
{
  Uint32 senderRef = ptr.p->m_create.m_senderRef;
  Uint32 senderData = ptr.p->m_create.m_senderData;

  bool first= false;
  if(ptr.p->m_state == Undofile::FS_CREATING &&
     (lg_ptr.p->m_state & Logfile_group::LG_ONLINE))
  {
    jam();
    Local_undofile_list free_list(m_file_pool, lg_ptr.p->m_files);
    Local_undofile_list meta(m_file_pool, lg_ptr.p->m_meta_files);
    first= free_list.isEmpty();
    meta.remove(ptr);
    if(!first)
    {
      jam();
      /**
       * Add log file next after current head
       */
      Ptr<Undofile> curr;
      m_file_pool.getPtr(curr, lg_ptr.p->m_file_pos[HEAD].m_ptr_i);
      if(free_list.next(curr))
      {
        jam();
        free_list.insertBefore(ptr, curr);
      }
      else
      {
        jam();
        free_list.addLast(ptr);
      }

      ptr.p->m_state = Undofile::FS_ONLINE | Undofile::FS_EMPTY;
    }
    else
    {
      jam();
      /**
       * First file isn't empty as it can be written to at any time
       */
      free_list.addLast(ptr);
      ptr.p->m_state = Undofile::FS_ONLINE;
      lg_ptr.p->m_state |= Logfile_group::LG_FLUSH_THREAD;
      signal->theData[0] = LgmanContinueB::FLUSH_LOG;
      signal->theData[1] = lg_ptr.i;
      signal->theData[2] = 0;
      sendSignal(reference(), GSN_CONTINUEB, signal, 3, JBB);
    }
  }
  else
  {
    jam();
    ptr.p->m_state = Undofile::FS_SORTING;
  }
  
  ptr.p->m_online.m_lsn = 0;
  ptr.p->m_online.m_outstanding = 0;
  
  Uint64 add= ptr.p->m_file_size - 1;
  lg_ptr.p->m_free_file_words += add * File_formats::UNDO_PAGE_WORDS;

  if(first)
  {
    jam();
    
    Buffer_idx tmp= { ptr.i, 0 };
    lg_ptr.p->m_file_pos[HEAD] = lg_ptr.p->m_file_pos[TAIL] = tmp;
    
    /**
     * Init log tail pointer
     */
    lg_ptr.p->m_tail_pos[0] = tmp;
    lg_ptr.p->m_tail_pos[1] = tmp;
    lg_ptr.p->m_tail_pos[2] = tmp;
    lg_ptr.p->m_next_reply_ptr_i = ptr.i;
  }

  validate_logfile_group(lg_ptr, "create_file_commit", jamBuffer());

  CreateFileImplConf* conf= (CreateFileImplConf*)signal->getDataPtr();
  conf->senderData = senderData;
  conf->senderRef = reference();
  sendSignal(senderRef, GSN_CREATE_FILE_IMPL_CONF, signal,
	     CreateFileImplConf::SignalLength, JBB);
}

void
Lgman::create_file_abort(Signal* signal, 
			 Ptr<Logfile_group> lg_ptr, 
			 Ptr<Undofile> ptr)
{
  if (ptr.p->m_fd == RNIL)
  {
    jam();
    ((FsConf*)signal->getDataPtr())->userPointer = ptr.i;
    execFSCLOSECONF(signal);
    return;
  }

  FsCloseReq *req= (FsCloseReq*)signal->getDataPtrSend();
  req->filePointer = ptr.p->m_fd;
  req->userReference = reference();
  req->userPointer = ptr.i;
  req->fileFlag = 0;
  FsCloseReq::setRemoveFileFlag(req->fileFlag, true);
  
  sendSignal(NDBFS_REF, GSN_FSCLOSEREQ, signal, 
	     FsCloseReq::SignalLength, JBB);
}

void
Lgman::execFSCLOSECONF(Signal* signal)
{
  Ptr<Undofile> ptr;
  Ptr<Logfile_group> lg_ptr;
  Uint32 ptrI = ((FsConf*)signal->getDataPtr())->userPointer;
  m_file_pool.getPtr(ptr, ptrI);
  
  Uint32 senderRef = ptr.p->m_create.m_senderRef;
  Uint32 senderData = ptr.p->m_create.m_senderData;
  
  m_logfile_group_pool.getPtr(lg_ptr, ptr.p->m_logfile_group_ptr_i);

  if (lg_ptr.p->m_state & Logfile_group::LG_DROPPING)
  {
    jam();
    {
      Local_undofile_list list(m_file_pool, lg_ptr.p->m_files);
      list.release(ptr);
    }
    drop_filegroup_drop_files(signal, lg_ptr, senderRef, senderData);
  }
  else
  {
    jam();
    Local_undofile_list list(m_file_pool, lg_ptr.p->m_meta_files);
    list.release(ptr);

    CreateFileImplConf* conf= (CreateFileImplConf*)signal->getDataPtr();
    conf->senderData = senderData;
    conf->senderRef = reference();
    sendSignal(senderRef, GSN_CREATE_FILE_IMPL_CONF, signal,
	       CreateFileImplConf::SignalLength, JBB);
  }
}

void
Lgman::execDROP_FILE_IMPL_REQ(Signal* signal)
{
  jamEntry();
  ndbrequire(false);
}

#define CONSUMER 0
#define PRODUCER 1

Lgman::Logfile_group::Logfile_group(const CreateFilegroupImplReq* req)
{
  m_logfile_group_id = req->filegroup_id;
  m_version = req->filegroup_version;
  m_state = LG_ONLINE;
  m_outstanding_fs = 0;
  m_next_reply_ptr_i = RNIL;
  
  m_applied = false;
  /**
   * We initialise this variable to 0 to indicate that we haven't received
   * any log records yet, this is used in a number of checks, probably better
   * to remove those checks and do the same work even when no LSNs have been
   * produced, but keep it for now as it is easiest.
   */
  m_next_lsn = 0;
  m_last_synced_lsn = 0;
  m_last_sync_req_lsn = 0;
  m_max_sync_req_lsn = 0;
  m_last_read_lsn = 0;
  m_file_pos[0].m_ptr_i= m_file_pos[1].m_ptr_i = RNIL;

  m_free_file_words = 0;
  m_total_buffer_words = 0;
  m_free_buffer_words = 0;
  m_callback_buffer_words = 0;

  m_pos[CONSUMER].m_current_page.m_ptr_i = RNIL;// { m_buffer_pages, idx }
  m_pos[CONSUMER].m_current_pos.m_ptr_i = RNIL; // { page ptr.i, m_words_used}
  m_pos[PRODUCER].m_current_page.m_ptr_i = RNIL;// { m_buffer_pages, idx }
  m_pos[PRODUCER].m_current_pos.m_ptr_i = RNIL; // { page ptr.i, m_words_used}

  m_tail_pos[2].m_ptr_i= RNIL;
  m_tail_pos[2].m_idx= ~0;
  
  m_tail_pos[0] = m_tail_pos[1] = m_tail_pos[2];
}

bool
Lgman::alloc_logbuffer_memory(Ptr<Logfile_group> ptr, Uint32 bytes)
{
  Uint32 pages= (((bytes + 3) >> 2) + File_formats::NDB_PAGE_SIZE_WORDS - 1)
    / File_formats::NDB_PAGE_SIZE_WORDS;
#if defined VM_TRACE || defined ERROR_INSERT
  Uint32 requested= pages;
#endif
  {
    Page_map map(m_data_buffer_pool, ptr.p->m_buffer_pages);
    while(pages)
    {
      Uint32 ptrI;
      Uint32 cnt = pages > 64 ? 64 : pages;
      m_ctx.m_mm.alloc_pages(RG_DISK_OPERATIONS, &ptrI, &cnt, 1);
      if (cnt)
      {
        jam();
	Buffer_idx range;
	range.m_ptr_i= ptrI;
	range.m_idx = cnt;
        
	if (map.append((Uint32*)&range, 2) == false)
        {
          /**
           * Failed to append page-range...
           *   jump out of alloc routine
           */
          jam();
          m_ctx.m_mm.release_pages(RG_DISK_OPERATIONS, 
                                   range.m_ptr_i, range.m_idx);
          break;
        }
	pages -= range.m_idx;
      }
      else
      {
        jam();
	break;
      }
    }
  }

  if(pages)
  {
    jam();
    /* Could not allocate all of the requested memory.
     * So release that already allocated.
     */
    free_logbuffer_memory(ptr);
    return false;
  }
  
#if defined VM_TRACE || defined ERROR_INSERT
  ndbout << "DD lgman: fg id:" << ptr.p->m_logfile_group_id << " undo buffer pages/bytes:" << (requested-pages) << "/" << (requested-pages)*File_formats::NDB_PAGE_SIZE << endl;
#endif
  
  init_logbuffer_pointers(ptr);
  return true;
}

void
Lgman::init_logbuffer_pointers(Ptr<Logfile_group> ptr)
{
  Page_map map(m_data_buffer_pool, ptr.p->m_buffer_pages);
  Page_map::Iterator it;
  union {
    Uint32 tmp[2];
    Buffer_idx range;
  };
  
  map.first(it);
  tmp[0] = *it.data;
  ndbrequire(map.next(it));
  tmp[1] = *it.data;
  
  ptr.p->m_pos[CONSUMER].m_current_page.m_ptr_i = 0;      // Index in page map
  ptr.p->m_pos[CONSUMER].m_current_page.m_idx = range.m_idx - 1;// left range
  ptr.p->m_pos[CONSUMER].m_current_pos.m_ptr_i = range.m_ptr_i; // Which page
  ptr.p->m_pos[CONSUMER].m_current_pos.m_idx = 0;               // Page pos
  
  ptr.p->m_pos[PRODUCER].m_current_page.m_ptr_i = 0;      // Index in page map
  ptr.p->m_pos[PRODUCER].m_current_page.m_idx = range.m_idx - 1;// left range
  ptr.p->m_pos[PRODUCER].m_current_pos.m_ptr_i = range.m_ptr_i; // Which page
  ptr.p->m_pos[PRODUCER].m_current_pos.m_idx = 0;               // Page pos

  Uint32 pages= range.m_idx;
  while(map.next(it))
  {
    jam();
    tmp[0] = *it.data;
    ndbrequire(map.next(it));
    tmp[1] = *it.data;
    pages += range.m_idx;
  }
  
  ptr.p->m_total_buffer_words =
    ptr.p->m_free_buffer_words = pages * File_formats::UNDO_PAGE_WORDS;
}

/**
 * Cannot use jam on this method since it is used before jam buffers
 * have been properly set up.
 */
Uint32
Lgman::compute_free_file_pages(Ptr<Logfile_group> ptr,
                               EmulatedJamBuffer *jamBuf)
{
  Buffer_idx head= ptr.p->m_file_pos[HEAD];
  Buffer_idx tail= ptr.p->m_file_pos[TAIL];
  Uint32 pages = 0;
  if (head.m_ptr_i == tail.m_ptr_i && head.m_idx < tail.m_idx)
  {
    thrjam(jamBuf);
    pages += tail.m_idx - head.m_idx;
  }
  else
  {
    thrjam(jamBuf);
    Ptr<Undofile> file;
    m_file_pool.getPtr(file, head.m_ptr_i);
    Local_undofile_list list(m_file_pool, ptr.p->m_files);
    
    do 
    {
      thrjam(jamBuf);
      pages += (file.p->m_file_size - head.m_idx - 1);
      if(!list.next(file))
      {
        thrjam(jamBuf);
	list.first(file);
      }
      head.m_idx = 0;
    } while(file.i != tail.m_ptr_i);
    
    pages += tail.m_idx - head.m_idx;
  }
  return pages;
}

void
Lgman::free_logbuffer_memory(Ptr<Logfile_group> ptr)
{
  union {
    Uint32 tmp[2];
    Buffer_idx range;
  };

  Page_map map(m_data_buffer_pool, ptr.p->m_buffer_pages);

  Page_map::Iterator it;
  map.first(it);
  while(!it.isNull())
  {
    jam();
    tmp[0] = *it.data;
    ndbrequire(map.next(it));
    tmp[1] = *it.data;
    
    m_ctx.m_mm.release_pages(RG_DISK_OPERATIONS, range.m_ptr_i, range.m_idx);
    map.next(it);
  }
  map.release();
}

Lgman::Undofile::Undofile(const struct CreateFileImplReq* req, Uint32 ptrI)
{
  m_fd = RNIL;
  m_file_id = req->file_id;
  m_logfile_group_ptr_i= ptrI;
  
  Uint64 pages = req->file_size_hi;
  pages = (pages << 32) | req->file_size_lo;
  pages /= GLOBAL_PAGE_SIZE;
  m_file_size = Uint32(pages);
#if defined VM_TRACE || defined ERROR_INSERT
  ndbout << "DD lgman: file id:" << m_file_id << " undofile pages/bytes:" << m_file_size << "/" << m_file_size*GLOBAL_PAGE_SIZE << endl;
#endif

  m_create.m_senderRef = req->senderRef; // During META
  m_create.m_senderData = req->senderData; // During META
  m_create.m_logfile_group_id = req->filegroup_id;
}

Logfile_client::Logfile_client(SimulatedBlock* block, 
			       Lgman* lgman, Uint32 logfile_group_id,
                               bool lock)
{
  Uint32 bno = block->number();
  Uint32 ino = block->instance();
  m_client_block= block;
  m_block= numberToBlock(bno, ino);
  m_lgman= lgman;
  m_lock = lock;
  m_logfile_group_id= logfile_group_id;
  D("client ctor " << bno << "/" << ino);
  if (m_lock)
  {
    jamBlock(block);
    m_lgman->client_lock(m_block, 0);
  }
}

Logfile_client::~Logfile_client()
{
#ifdef VM_TRACE
  Uint32 bno = blockToMain(m_block);
  Uint32 ino = blockToInstance(m_block);
#endif
  D("client dtor " << bno << "/" << ino);
  if (m_lock)
    m_lgman->client_unlock(m_block, 0);
}

int
Logfile_client::sync_lsn(Signal* signal, 
			 Uint64 lsn, Request* req, Uint32 flags)
{
  Ptr<Lgman::Logfile_group> ptr;
  if(m_lgman->m_logfile_group_list.first(ptr))
  {
    jamBlock(m_client_block);
    if(ptr.p->m_last_synced_lsn >= lsn)
    {
      jamBlock(m_client_block);
      return 1;
    }
    
    bool empty= false;
    Ptr<Lgman::Log_waiter> wait;
    {
      Lgman::Local_log_waiter_list
	list(m_lgman->m_log_waiter_pool, ptr.p->m_log_sync_waiters);
      
      empty= list.isEmpty();
      if (!list.seizeLast(wait))
      {
        jamBlock(m_client_block);
	return -1;
      }
      
      wait.p->m_block= m_block;
      wait.p->m_sync_lsn= lsn;
      memcpy(&wait.p->m_callback, &req->m_callback, 
	     sizeof(SimulatedBlock::CallbackPtr));

      ptr.p->m_max_sync_req_lsn = lsn > ptr.p->m_max_sync_req_lsn ?
	lsn : ptr.p->m_max_sync_req_lsn;
    }
    
    if(ptr.p->m_last_sync_req_lsn < lsn && 
       ! (ptr.p->m_state & Lgman::Logfile_group::LG_FORCE_SYNC_THREAD))
    {
      jamBlock(m_client_block);
      ptr.p->m_state |= Lgman::Logfile_group::LG_FORCE_SYNC_THREAD;
      signal->theData[0] = LgmanContinueB::FORCE_LOG_SYNC;
      signal->theData[1] = ptr.i;
      signal->theData[2] = (Uint32)(lsn >> 32);
      signal->theData[3] = (Uint32)(lsn & 0xFFFFFFFF);
      m_client_block->sendSignalWithDelay(m_lgman->reference(), 
                                          GSN_CONTINUEB, signal, 10, 4);
    }
    return 0;
  }
  jamBlock(m_client_block);
  return -1;
}

void
Lgman::force_log_sync(Signal* signal, 
		      Ptr<Logfile_group> ptr, 
		      Uint32 lsn_hi, Uint32 lsn_lo)
{
  Local_log_waiter_list list(m_log_waiter_pool, ptr.p->m_log_sync_waiters);
  Uint64 force_lsn = lsn_hi; force_lsn <<= 32; force_lsn += lsn_lo;

  if(ptr.p->m_last_sync_req_lsn < force_lsn)
  {
    jam();
    /**
     * Do force
     */
    Buffer_idx pos= ptr.p->m_pos[PRODUCER].m_current_pos;
    GlobalPage *page = m_shared_page_pool.getPtr(pos.m_ptr_i);
  
    Uint32 free= File_formats::UNDO_PAGE_WORDS - pos.m_idx;
    if(pos.m_idx) // don't flush empty page...
    {
      jam();
      Uint64 lsn= ptr.p->m_next_lsn - 1;
      
      File_formats::Undofile::Undo_page* undo= 
	(File_formats::Undofile::Undo_page*)page;
      undo->m_page_header.m_page_lsn_lo = (Uint32)(lsn & 0xFFFFFFFF);
      undo->m_page_header.m_page_lsn_hi = (Uint32)(lsn >> 32);
      undo->m_words_used= File_formats::UNDO_PAGE_WORDS - free;
      
      /**
       * Update free space with extra NOOP
       */
      ndbrequire(ptr.p->m_free_file_words >= free);
      ndbrequire(ptr.p->m_free_buffer_words > free);
      ptr.p->m_free_file_words -= free;
      ptr.p->m_free_buffer_words -= free;
      
      validate_logfile_group(ptr, "force_log_sync", jamBuffer());

      next_page(ptr.p, PRODUCER, jamBuffer());
      ptr.p->m_pos[PRODUCER].m_current_pos.m_idx = 0;
    }
  }

  
  
  Uint64 max_req_lsn = ptr.p->m_max_sync_req_lsn;
  if(max_req_lsn > force_lsn && 
     max_req_lsn > ptr.p->m_last_sync_req_lsn)
  {
    jam();
    ndbrequire(ptr.p->m_state & Lgman::Logfile_group::LG_FORCE_SYNC_THREAD);
    signal->theData[0] = LgmanContinueB::FORCE_LOG_SYNC;
    signal->theData[1] = ptr.i;
    signal->theData[2] = (Uint32)(max_req_lsn >> 32);
    signal->theData[3] = (Uint32)(max_req_lsn & 0xFFFFFFFF);
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 10, 4);
  }
  else
  {
    ptr.p->m_state &= ~(Uint32)Lgman::Logfile_group::LG_FORCE_SYNC_THREAD;
  }
}

void
Lgman::process_log_sync_waiters(Signal* signal, Ptr<Logfile_group> ptr)
{
  Local_log_waiter_list 
    list(m_log_waiter_pool, ptr.p->m_log_sync_waiters);

  if(list.isEmpty())
  {
    jam();
    return;
  }

  bool removed= false;
  Ptr<Log_waiter> waiter;
  list.first(waiter);
  Uint32 logfile_group_id = ptr.p->m_logfile_group_id;

  if(waiter.p->m_sync_lsn <= ptr.p->m_last_synced_lsn)
  {
    jam();
    removed= true;
    Uint32 block = waiter.p->m_block;
    CallbackPtr & callback = waiter.p->m_callback;
    sendCallbackConf(signal, block, callback, logfile_group_id,
                     LgmanContinueB::PROCESS_LOG_SYNC_WAITERS, 0);
    
    list.releaseFirst(/* waiter */);
  }
  
  if(removed && !list.isEmpty())
  {
    jam();
    ptr.p->m_state |= Logfile_group::LG_SYNC_WAITERS_THREAD;
    signal->theData[0] = LgmanContinueB::PROCESS_LOG_SYNC_WAITERS;
    signal->theData[1] = ptr.i;
    sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
  }
  else
  {
    jam();
    ptr.p->m_state &= ~(Uint32)Logfile_group::LG_SYNC_WAITERS_THREAD;
  }
}


Uint32*
Lgman::get_log_buffer(Ptr<Logfile_group> ptr,
                      Uint32 sz,
                      EmulatedJamBuffer *jamBuf)
{
  GlobalPage *page;
  page=m_shared_page_pool.getPtr(ptr.p->m_pos[PRODUCER].m_current_pos.m_ptr_i);
  
  Uint32 total_free= ptr.p->m_free_buffer_words;
  ndbrequire(total_free >= sz);
  Uint32 pos= ptr.p->m_pos[PRODUCER].m_current_pos.m_idx;
  Uint32 free= File_formats::UNDO_PAGE_WORDS - pos;

  if(sz <= free)
  {
next:
    thrjam(jamBuf);
    // fits this page wo/ problem
    ndbrequire(total_free >= sz);
    ptr.p->m_free_buffer_words = total_free - sz;
    ptr.p->m_pos[PRODUCER].m_current_pos.m_idx = pos + sz;
    return ((File_formats::Undofile::Undo_page*)page)->m_data + pos;
  }
  thrjam(jamBuf);
  
  /**
   * It didn't fit page...fill page with a NOOP log entry
   */
  Uint64 lsn= ptr.p->m_next_lsn - 1;
  File_formats::Undofile::Undo_page* undo= 
    (File_formats::Undofile::Undo_page*)page;
  undo->m_page_header.m_page_lsn_lo = (Uint32)(lsn & 0xFFFFFFFF);
  undo->m_page_header.m_page_lsn_hi = (Uint32)(lsn >> 32);
  undo->m_words_used= File_formats::UNDO_PAGE_WORDS - free;
  
  /**
   * Update free space with extra NOOP
   */
  ndbrequire(ptr.p->m_free_file_words >= free);
  ptr.p->m_free_file_words -= free;

  validate_logfile_group(ptr, "get_log_buffer", jamBuf);
  
  pos= 0;
  assert(total_free >= free);
  total_free -= free;
  page= m_shared_page_pool.getPtr(next_page(ptr.p, PRODUCER, jamBuf));
  goto next;
}

Uint32 
Lgman::next_page(Logfile_group* ptrP,
                 Uint32 i,
                 EmulatedJamBuffer *jamBuf)
{
  Uint32 page_ptr_i= ptrP->m_pos[i].m_current_pos.m_ptr_i;
  Uint32 left_in_range= ptrP->m_pos[i].m_current_page.m_idx;
  if(left_in_range > 0)
  {
    thrjam(jamBuf);
    ptrP->m_pos[i].m_current_page.m_idx = left_in_range - 1;
    ptrP->m_pos[i].m_current_pos.m_ptr_i = page_ptr_i + 1;
    return page_ptr_i + 1;
  }
  else
  {
    thrjam(jamBuf);
    Lgman::Page_map map(m_data_buffer_pool, ptrP->m_buffer_pages);
    Uint32 pos= (ptrP->m_pos[i].m_current_page.m_ptr_i + 2) % map.getSize();
    Lgman::Page_map::Iterator it;
    map.position(it, pos);

    union {
      Uint32 tmp[2];
      Lgman::Buffer_idx range;
    };
    
    tmp[0] = *it.data; map.next(it);
    tmp[1] = *it.data;
    
    ptrP->m_pos[i].m_current_page.m_ptr_i = pos;           // New index in map
    ptrP->m_pos[i].m_current_page.m_idx = range.m_idx - 1; // Free pages 
    ptrP->m_pos[i].m_current_pos.m_ptr_i = range.m_ptr_i;  // Current page
    // No need to set ptrP->m_current_pos.m_idx, that is set "in higher"-func
    return range.m_ptr_i;
  }
}

int
Logfile_client::get_log_buffer(Signal* signal,
                               Uint32 sz, 
			       SimulatedBlock::CallbackPtr* callback)
{
  sz += 2; // lsn
  Lgman::Logfile_group key;
  key.m_logfile_group_id= m_logfile_group_id;
  Ptr<Lgman::Logfile_group> ptr;
  if(m_lgman->m_logfile_group_hash.find(ptr, key))
  {
    jamBlock(m_client_block);
    Uint32 callback_buffer = ptr.p->m_callback_buffer_words;
    Uint32 free_buffer = ptr.p->m_free_buffer_words;
    if (free_buffer >= (sz + callback_buffer + FREE_BUFFER_MARGIN) &&
        ptr.p->m_log_buffer_waiters.isEmpty())
    {
      jamBlock(m_client_block);
      ptr.p->m_callback_buffer_words = callback_buffer + sz;
      return 1;
    }
    
    bool empty= false;
    {
      Ptr<Lgman::Log_waiter> wait;
      Lgman::Local_log_waiter_list
	list(m_lgman->m_log_waiter_pool, ptr.p->m_log_buffer_waiters);
      
      empty= list.isEmpty();
      if (!list.seizeFirst(wait))
      {
        jamBlock(m_client_block);
	return -1;
      }      

      wait.p->m_size= sz;
      wait.p->m_block= m_block;
      memcpy(&wait.p->m_callback, callback,sizeof(SimulatedBlock::CallbackPtr));
    }
    return 0;
  }
  jamBlock(m_client_block);
  return -1;
}

NdbOut&
operator<<(NdbOut& out, const Lgman::Buffer_idx& pos)
{
  out << "[ " 
      << pos.m_ptr_i << " "
      << pos.m_idx << " ]";
  return out;
}

NdbOut&
operator<<(NdbOut& out, const Lgman::Logfile_group::Position& pos)
{
  out << "[ (" 
      << pos.m_current_page.m_ptr_i << " "
      << pos.m_current_page.m_idx << ") ("
      << pos.m_current_pos.m_ptr_i << " "
      << pos.m_current_pos.m_idx << ") ]";
  return out;
}

void
Lgman::flush_log(Signal* signal, Ptr<Logfile_group> ptr, Uint32 force)
{
  Logfile_group::Position consumer= ptr.p->m_pos[CONSUMER];
  Logfile_group::Position producer= ptr.p->m_pos[PRODUCER];
 
  jamEntry();

  if (consumer.m_current_page == producer.m_current_page)
  {
    jam();
    Buffer_idx pos = producer.m_current_pos;

#if 0
    if (force)
    {
      ndbout_c("force: %d ptr.p->m_file_pos[HEAD].m_ptr_i= %x", 
	       force, ptr.p->m_file_pos[HEAD].m_ptr_i);
      ndbout_c("consumer.m_current_page: %d %d producer.m_current_page: %d %d",
	       consumer.m_current_page.m_ptr_i, consumer.m_current_page.m_idx,
	       producer.m_current_page.m_ptr_i, producer.m_current_page.m_idx);
    }
#endif
    if (! (ptr.p->m_state & Logfile_group::LG_DROPPING))
    {
      if (ptr.p->m_log_buffer_waiters.isEmpty() || pos.m_idx == 0)
      {
        jam();
	force =  0;
      }
      else if (ptr.p->m_free_buffer_words < FREE_BUFFER_MARGIN)
      {
        jam();
        force = 2;
      }

      if (force < 2 || ptr.p->m_outstanding_fs)
      {
        jam();
	signal->theData[0] = LgmanContinueB::FLUSH_LOG;
	signal->theData[1] = ptr.i;
	signal->theData[2] = force + 1;
	sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 
			    force ? 10 : 100, 3);
	return;
      }
      else
      {
        jam();
	GlobalPage *page = m_shared_page_pool.getPtr(pos.m_ptr_i);
	
	Uint32 free= File_formats::UNDO_PAGE_WORDS - pos.m_idx;

	g_eventLogger->info("LGMAN: force flush %d %d outstanding: %u"
                            " isEmpty(): %u",
                            pos.m_idx, ptr.p->m_free_buffer_words,
                            ptr.p->m_outstanding_fs,
                            ptr.p->m_log_buffer_waiters.isEmpty());
	
	ndbrequire(pos.m_idx); // don't flush empty page...
	Uint64 lsn= ptr.p->m_next_lsn - 1;
	
	File_formats::Undofile::Undo_page* undo= 
	  (File_formats::Undofile::Undo_page*)page;
	undo->m_page_header.m_page_lsn_lo = (Uint32)(lsn & 0xFFFFFFFF);
	undo->m_page_header.m_page_lsn_hi = (Uint32)(lsn >> 32);
	undo->m_words_used= File_formats::UNDO_PAGE_WORDS - free;
	
	/**
	 * Update free space with extra NOOP
	 */
	ndbrequire(ptr.p->m_free_file_words >= free);
	ndbrequire(ptr.p->m_free_buffer_words > free);
	ptr.p->m_free_file_words -= free;
	ptr.p->m_free_buffer_words -= free;
         
	validate_logfile_group(ptr, "force_log_flush", jamBuffer());
	
	next_page(ptr.p, PRODUCER, jamBuffer());
	ptr.p->m_pos[PRODUCER].m_current_pos.m_idx = 0;
	producer = ptr.p->m_pos[PRODUCER];
	// break through
      }
    }
    else
    {
      jam();
      ptr.p->m_state &= ~(Uint32)Logfile_group::LG_FLUSH_THREAD;
      return;
    }
  }
  
  bool full= false;
  Uint32 tot= 0;
  while(!(consumer.m_current_page == producer.m_current_page) && !full)
  {
    jam();
    validate_logfile_group(ptr, "before flush log", jamBuffer());

    Uint32 cnt; // pages written
    Uint32 page= consumer.m_current_pos.m_ptr_i;
    if(consumer.m_current_page.m_ptr_i == producer.m_current_page.m_ptr_i)
    {
      /**
       * In same range
       */
      if(producer.m_current_pos.m_ptr_i > page)
      {
        /**
         * producer ahead of consumer in same chunk
         */
	jam();
	Uint32 tmp= producer.m_current_pos.m_ptr_i - page;
	cnt= write_log_pages(signal, ptr, page, tmp);
	assert(cnt <= tmp);
	
	consumer.m_current_pos.m_ptr_i += cnt;
	consumer.m_current_page.m_idx -= cnt;
	full= (tmp > cnt);
      }
      else
      {
        /**
         * consumer ahead of producer in same chunk
         */
	Uint32 tmp= consumer.m_current_page.m_idx + 1;
	cnt= write_log_pages(signal, ptr, page, tmp);
	assert(cnt <= tmp);

	if(cnt == tmp)
	{
	  jam();
	  /**
	   * Entire chunk is written
	   *   move to next
	   */
	  ptr.p->m_pos[CONSUMER].m_current_page.m_idx= 0;
	  next_page(ptr.p, CONSUMER, jamBuffer());
	  consumer = ptr.p->m_pos[CONSUMER];
 	}
	else
	{
	  jam();
	  /**
	   * Failed to write entire chunk...
	   */
	  full= true;
	  consumer.m_current_page.m_idx -= cnt;
	  consumer.m_current_pos.m_ptr_i += cnt;
	}
      }
    }
    else
    {
      Uint32 tmp= consumer.m_current_page.m_idx + 1;
      cnt= write_log_pages(signal, ptr, page, tmp);
      assert(cnt <= tmp);

      if(cnt == tmp)
      {
	jam();
	/**
	 * Entire chunk is written
	 *   move to next
	 */
	ptr.p->m_pos[CONSUMER].m_current_page.m_idx= 0;
	next_page(ptr.p, CONSUMER, jamBuffer());
	consumer = ptr.p->m_pos[CONSUMER];
      }
      else
      {
	jam();
	/**
	 * Failed to write entire chunk...
	 */
	full= true;
	consumer.m_current_page.m_idx -= cnt;
	consumer.m_current_pos.m_ptr_i += cnt;
      }
    }

    tot += cnt;
    if(cnt)
      validate_logfile_group(ptr, " after flush_log", jamBuffer());
  }

  ptr.p->m_pos[CONSUMER]= consumer;
  
  if (! (ptr.p->m_state & Logfile_group::LG_DROPPING))
  {
    jam();
    signal->theData[0] = LgmanContinueB::FLUSH_LOG;
    signal->theData[1] = ptr.i;
    signal->theData[2] = 0;
    sendSignal(reference(), GSN_CONTINUEB, signal, 3, JBB);
  }
  else
  {
    jam();
    ptr.p->m_state &= ~(Uint32)Logfile_group::LG_FLUSH_THREAD;
  }
}

/*
 * Overloaded UNDO buffer creates waiters for buffer space.
 * As in direct return from Logfile_client::get_log_buffer()
 * the FREE_BUFFER_MARGIN allows for a possible NOOP entry
 * when the logged entry does not fit on current page.
 *
 * In non-MT case the entry is added in same time-slice.
 * In MT case callback is via signals.  Here the problem is
 * that we cannot account for multiple NOOP entries created
 * in non-deterministic order.  So we serialize processing
 * to one entry at a time via CALLBACK_ACK signals.  This all
 * happens in memory at commit and should not have major impact.
 */
void
Lgman::process_log_buffer_waiters(Signal* signal, Ptr<Logfile_group> ptr)
{
  Uint32 free_buffer= ptr.p->m_free_buffer_words;
  Uint32 callback_buffer = ptr.p->m_callback_buffer_words;
  Local_log_waiter_list 
    list(m_log_waiter_pool, ptr.p->m_log_buffer_waiters);

  if (list.isEmpty())
  {
    jam();
    ptr.p->m_state &= ~(Uint32)Logfile_group::LG_WAITERS_THREAD;
    return;
  }
  
  bool removed= false;
  Ptr<Log_waiter> waiter;
  list.first(waiter);
  Uint32 sz  = waiter.p->m_size;
  Uint32 logfile_group_id = ptr.p->m_logfile_group_id;
  if (sz + callback_buffer + FREE_BUFFER_MARGIN < free_buffer)
  {
    jam();
    removed= true;
    Uint32 block = waiter.p->m_block;
    CallbackPtr & callback = waiter.p->m_callback;
    ptr.p->m_callback_buffer_words += sz;
    sendCallbackConf(signal, block, callback, logfile_group_id,
                     LgmanContinueB::PROCESS_LOG_BUFFER_WAITERS, 0);

    list.releaseFirst(/* waiter */);
  }
  
  if (removed && !list.isEmpty())
  {
    jam();
    ptr.p->m_state |= Logfile_group::LG_WAITERS_THREAD;
    // continue via CALLBACK_ACK
  }
  else
  {
    jam();
    ptr.p->m_state &= ~(Uint32)Logfile_group::LG_WAITERS_THREAD;
  }
}

void
Lgman::execCALLBACK_ACK(Signal* signal)
{
  jamEntry();
  BlockReference senderRef = signal->getSendersBlockRef();
  BlockNumber senderBlock = refToMain(senderRef);

  const CallbackAck* ack = (const CallbackAck*)signal->getDataPtr();
  Uint32 logfile_group_id = ack->senderData;
  Uint32 callbackInfo = ack->callbackInfo;

  Ptr<Logfile_group> ptr;
  ndbrequire(m_logfile_group_hash.find(ptr, logfile_group_id));

  // using ContinueB as convenience

  switch (callbackInfo) {
  case LgmanContinueB::PROCESS_LOG_BUFFER_WAITERS:
    jam();
    ndbrequire(senderBlock == DBTUP || senderBlock == LGMAN);
    break;
  // no PROCESS_LOG_SYNC_WAITERS yet (or ever)
  default:
    ndbrequire(false);
    break;
  }

  signal->theData[0] = callbackInfo;
  signal->theData[1] = ptr.i;
  sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
}

/**
 * 512 => 512 * 32 kByte = 16 MByte
 * This means that we can have at most 16 MBytes of UNDO log pages
 * outstanding at any time. This number represents a compromise
 * between not having to search so far ahead in the UNDO log when
 * restarting and ensuring that we can get good throughput in
 * writing to the UNDO log.
 */
#define MAX_UNDO_PAGES_OUTSTANDING 512

Uint32
Lgman::write_log_pages(Signal* signal, Ptr<Logfile_group> ptr,
		       Uint32 pageId, Uint32 in_pages)
{
  assert(in_pages);
  Ptr<Undofile> filePtr;
  Buffer_idx head= ptr.p->m_file_pos[HEAD];
  Buffer_idx tail= ptr.p->m_file_pos[TAIL];
  m_file_pool.getPtr(filePtr, head.m_ptr_i);
  
  if(filePtr.p->m_online.m_outstanding > 0)
  {
    jam();
    return 0;
  }
  
  Uint32 sz= filePtr.p->m_file_size - 1; // skip zero
  Uint32 max, pages= in_pages;

  if(!(head.m_ptr_i == tail.m_ptr_i && head.m_idx < tail.m_idx))
  {
    jam();
    max= sz - head.m_idx;
  }
  else
  {
    jam();
    max= tail.m_idx - head.m_idx;
  }

  if (head.m_idx == 0)
  {
    /**
     * The first write in a file is always just one page. The reason is that
     * we want the restart logic to sort the UNDO log files to be easy. We
     * need to add a recommendation that the UNDO log should have at least
     * 2 files if possible and also to be of reasonably large size.
     *
     * If we allow larger writes of the first page we also need to adapt the
     * file sort logic during restarts to look further into the file before
     * concluding the file sort order of a file.
     */
    jam();
    pages = 1;
  }
  if (pages > MAX_UNDO_PAGES_OUTSTANDING)
  {
    /**
     * We will never write more than 16 MBytes per write. If more is
     * available it will have to wait until we come back after this
     * write. This is to ensure that we have a limit that can be used
     * to find the last written UNDO log page during restart.
     *
     * LGMAN files are opened with the OM_SYNC flag set, so each
     * FSWRITEREQ is translated into both a file system write and
     * a fsync call (unless using O_DIRECT when no fsync call is
     * needed). See above comment on O_DIRECT and why no fsync calls
     * are needed.
     */
    jam();
    pages = MAX_UNDO_PAGES_OUTSTANDING;
  }

  FsReadWriteReq* req= (FsReadWriteReq*)signal->getDataPtrSend();
  req->filePointer = filePtr.p->m_fd;
  req->userReference = reference();
  req->userPointer = filePtr.i;
  req->varIndex = 1+head.m_idx; // skip zero page
  req->numberOfPages = pages;
  req->data.pageData[0] = pageId;
  req->operationFlag = 0;
  FsReadWriteReq::setFormatFlag(req->operationFlag,
				FsReadWriteReq::fsFormatSharedPage);

  if(max > pages)
  {
    /**
     * The write will be entirely within the current file, just proceed with
     * the write and se states accordingly.
     */
    jam();
    max= pages;
    head.m_idx += max;
    ptr.p->m_file_pos[HEAD] = head;  

    sendSignal(NDBFS_REF, GSN_FSWRITEREQ, signal, 
               FsReadWriteReq::FixedLength + 1, JBA);

    ptr.p->m_outstanding_fs++;
    filePtr.p->m_online.m_outstanding = max;
    filePtr.p->m_state |= Undofile::FS_OUTSTANDING;

    File_formats::Undofile::Undo_page *page= (File_formats::Undofile::Undo_page*)
      m_shared_page_pool.getPtr(pageId + max - 1);
    Uint64 lsn = 0;
    lsn += page->m_page_header.m_page_lsn_hi; lsn <<= 32;
    lsn += page->m_page_header.m_page_lsn_lo;
    
    filePtr.p->m_online.m_lsn = lsn;  // Store last writereq lsn on file
    ptr.p->m_last_sync_req_lsn = lsn; // And logfile_group
  }
  else
  {
    /**
     * We need to write the last part of this UNDO log file, this includes
     * changing into a new file.
     */
    jam();
    req->numberOfPages = max;
    
    sendSignal(NDBFS_REF, GSN_FSWRITEREQ, signal, 
               FsReadWriteReq::FixedLength + 1, JBA);

    ptr.p->m_outstanding_fs++;
    filePtr.p->m_online.m_outstanding = max;
    filePtr.p->m_state |= Undofile::FS_OUTSTANDING;

    File_formats::Undofile::Undo_page *page= (File_formats::Undofile::Undo_page*)
      m_shared_page_pool.getPtr(pageId + max - 1);
    Uint64 lsn = 0;
    lsn += page->m_page_header.m_page_lsn_hi; lsn <<= 32;
    lsn += page->m_page_header.m_page_lsn_lo;
    
    filePtr.p->m_online.m_lsn = lsn;  // Store last writereq lsn on file
    ptr.p->m_last_sync_req_lsn = lsn; // And logfile_group
    
    Ptr<Undofile> next = filePtr;
    Local_undofile_list files(m_file_pool, ptr.p->m_files);
    if(!files.next(next))
    {
      jam();
      files.first(next);
    }
    SimulatedBlock* fs = globalData.getBlock(NDBFS);
    g_eventLogger->info("LGMAN: changing file from %s to %s",
                        fs->get_filename(filePtr.p->m_fd),
                        fs->get_filename(next.p->m_fd));
    filePtr.p->m_state |= Undofile::FS_MOVE_NEXT;
    next.p->m_state &= ~(Uint32)Undofile::FS_EMPTY;

    head.m_idx= 0;
    head.m_ptr_i= next.i;
    ptr.p->m_file_pos[HEAD] = head;
    if (max < pages)
    {
      max += write_log_pages(signal, ptr, pageId + max, pages - max);
    }
  }
  
  assert(max);
  return max;
}

void
Lgman::execFSWRITEREF(Signal* signal)
{
  jamEntry();
  SimulatedBlock::execFSWRITEREF(signal);
  ndbrequire(false);
}

void
Lgman::execFSWRITECONF(Signal* signal)
{
  jamEntry();
  client_lock(number(), __LINE__);
  FsConf * conf = (FsConf*)signal->getDataPtr();
  Ptr<Undofile> ptr;
  m_file_pool.getPtr(ptr, conf->userPointer);

  ndbrequire(ptr.p->m_state & Undofile::FS_OUTSTANDING);
  ptr.p->m_state &= ~(Uint32)Undofile::FS_OUTSTANDING;

  Ptr<Logfile_group> lg_ptr;
  m_logfile_group_pool.getPtr(lg_ptr, ptr.p->m_logfile_group_ptr_i);
  
  Uint32 cnt= lg_ptr.p->m_outstanding_fs;
  ndbrequire(cnt);
  
  if(lg_ptr.p->m_next_reply_ptr_i == ptr.i)
  {
    jam();
    Uint32 tot= 0;
    Uint64 lsn = 0;
    {
      Local_undofile_list files(m_file_pool, lg_ptr.p->m_files);
      while(cnt && ! (ptr.p->m_state & Undofile::FS_OUTSTANDING))
      {
        jam();
	Uint32 state= ptr.p->m_state;
	Uint32 pages= ptr.p->m_online.m_outstanding;
	ndbrequire(pages);
	ptr.p->m_online.m_outstanding= 0;
	ptr.p->m_state &= ~(Uint32)Undofile::FS_MOVE_NEXT;
	tot += pages;
	cnt--;
	
	lsn = ptr.p->m_online.m_lsn;
	
	if((state & Undofile::FS_MOVE_NEXT) && !files.next(ptr))
        {
          jam();
	  files.first(ptr);
        }
      }
    }
    
    ndbassert(tot);
    lg_ptr.p->m_outstanding_fs = cnt;
    lg_ptr.p->m_free_buffer_words += (tot * File_formats::UNDO_PAGE_WORDS);
    lg_ptr.p->m_next_reply_ptr_i = ptr.i;
    lg_ptr.p->m_last_synced_lsn = lsn;

    if(! (lg_ptr.p->m_state & Logfile_group::LG_SYNC_WAITERS_THREAD))
    {
      jam();
      process_log_sync_waiters(signal, lg_ptr);
    }

    if(! (lg_ptr.p->m_state & Logfile_group::LG_WAITERS_THREAD))
    {
      jam();
      process_log_buffer_waiters(signal, lg_ptr);
    }
  }
  else
  {
    jam();
    g_eventLogger->info("LGMAN: miss matched writes");
  }
  client_unlock(number(), __LINE__);
  return;
}

void
Lgman::exec_lcp_frag_ord(Signal* signal, SimulatedBlock* client_block)
{
  jamBlock(client_block);

  LcpFragOrd * ord = (LcpFragOrd *)signal->getDataPtr();
  Uint32 lcp_id= ord->lcpId;
  Uint32 frag_id = ord->fragmentId;
  Uint32 table_id = ord->tableId;

  Ptr<Logfile_group> ptr;
  m_logfile_group_list.first(ptr);
  
  Uint32 entry= lcp_id == m_latest_lcp ? 
    File_formats::Undofile::UNDO_LCP : File_formats::Undofile::UNDO_LCP_FIRST;
  if(!ptr.isNull() && ! (ptr.p->m_state & Logfile_group::LG_CUT_LOG_THREAD))
  {
    jamBlock(client_block);
    ptr.p->m_state |= Logfile_group::LG_CUT_LOG_THREAD;
    signal->theData[0] = LgmanContinueB::CUT_LOG_TAIL;
    signal->theData[1] = ptr.i;
    client_block->sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
  }
  
  if(!ptr.isNull() && ptr.p->m_next_lsn)
  {
    jamBlock(client_block);
    Uint32 undo[3];
    undo[0] = lcp_id;
    undo[1] = (table_id << 16) | frag_id;
    undo[2] = (entry << 16 ) | (sizeof(undo) >> 2);

    Uint64 next_lsn= m_next_lsn;
    
    if(ptr.p->m_next_lsn == next_lsn
#ifdef VM_TRACE
       && ((rand() % 100) > 50)
#endif
       )
    {
      jamBlock(client_block);
      undo[2] |= (File_formats::Undofile::UNDO_NEXT_LSN << 16);
      Uint32 *dst= get_log_buffer(ptr,
                                  sizeof(undo) >> 2,
                                  client_block->jamBuffer());
      memcpy(dst, undo, sizeof(undo));
      ndbrequire(ptr.p->m_free_file_words >= (sizeof(undo) >> 2));
      ptr.p->m_free_file_words -= (sizeof(undo) >> 2);
    }
    else
    {
      jamBlock(client_block);
      Uint32 *dst= get_log_buffer(ptr,
                                  (sizeof(undo) >> 2) + 2,
                                  client_block->jamBuffer());      
      * dst++ = (Uint32)(next_lsn >> 32);
      * dst++ = (Uint32)(next_lsn & 0xFFFFFFFF);
      memcpy(dst, undo, sizeof(undo));
      ndbrequire(ptr.p->m_free_file_words >= (sizeof(undo) >> 2));
      ptr.p->m_free_file_words -= ((sizeof(undo) >> 2) + 2);
    }
    ptr.p->m_last_lcp_lsn = next_lsn;
    m_next_lsn = ptr.p->m_next_lsn = next_lsn + 1;

    validate_logfile_group(ptr, "execLCP_FRAG_ORD", client_block->jamBuffer());
  }
  
  while(!ptr.isNull())
  {
    jamBlock(client_block);
    if (ptr.p->m_next_lsn)
    { 
      jamBlock(client_block);
      /**
       * First LCP_FRAGORD for each LCP, sets tail pos
       */
      if(m_latest_lcp != lcp_id)
      {
        jamBlock(client_block);
	ptr.p->m_tail_pos[0] = ptr.p->m_tail_pos[1];
	ptr.p->m_tail_pos[1] = ptr.p->m_tail_pos[2];
	ptr.p->m_tail_pos[2] = ptr.p->m_file_pos[HEAD];
      }
      
      if(0)
	ndbout_c
	  ("execLCP_FRAG_ORD (%d %d) (%d %d) (%d %d) free pages: %ld", 
	   ptr.p->m_tail_pos[0].m_ptr_i, ptr.p->m_tail_pos[0].m_idx,
	   ptr.p->m_tail_pos[1].m_ptr_i, ptr.p->m_tail_pos[1].m_idx,
	   ptr.p->m_tail_pos[2].m_ptr_i, ptr.p->m_tail_pos[2].m_idx,
	   (long) (ptr.p->m_free_file_words / File_formats::UNDO_PAGE_WORDS));
    }
    m_logfile_group_list.next(ptr);
  }
  m_latest_lcp = lcp_id;
}

void
Lgman::execEND_LCPREQ(Signal* signal)
{
  jamEntry();
  EndLcpReq* req= (EndLcpReq*)signal->getDataPtr();
  ndbrequire(m_latest_lcp == req->backupId);
  m_end_lcp_senderdata = req->senderData;

  Ptr<Logfile_group> ptr;
  m_logfile_group_list.first(ptr);
  bool wait= false;
  while(!ptr.isNull())
  {
    jam();
    Uint64 lcp_lsn = ptr.p->m_last_lcp_lsn;
    if(ptr.p->m_last_synced_lsn < lcp_lsn)
    {
      jam();
      wait= true;
      if(signal->getSendersBlockRef() != reference())
      {
        jam();
        D("Logfile_client - execEND_LCPREQ");
	Logfile_client tmp(this, this, ptr.p->m_logfile_group_id);
	Logfile_client::Request req;
	req.m_callback.m_callbackData = ptr.i;
	req.m_callback.m_callbackIndex = ENDLCP_CALLBACK;
	ndbrequire(tmp.sync_lsn(signal, lcp_lsn, &req, 0) == 0);
      }
    }
    else
    {
      jam();
      ptr.p->m_last_lcp_lsn = 0;
    }
    m_logfile_group_list.next(ptr);
  }
  
  if(wait)
  {
    jam();
    return;
  }

  EndLcpConf* conf = (EndLcpConf*)signal->getDataPtrSend();
  conf->senderData = m_end_lcp_senderdata;
  conf->senderRef = reference();
  sendSignal(DBLQH_REF, GSN_END_LCPCONF,
             signal, EndLcpConf::SignalLength, JBB);
}

void
Lgman::endlcp_callback(Signal* signal, Uint32 ptr, Uint32 res)
{
  EndLcpReq* req= (EndLcpReq*)signal->getDataPtr();
  req->backupId = m_latest_lcp;
  req->senderData = m_end_lcp_senderdata;
  execEND_LCPREQ(signal);
}

void
Lgman::cut_log_tail(Signal* signal, Ptr<Logfile_group> ptr)
{
  bool done= true;
  if (likely(ptr.p->m_next_lsn))
  {
    jam();
    Buffer_idx tmp= ptr.p->m_tail_pos[0];
    Buffer_idx tail= ptr.p->m_file_pos[TAIL];
    
    Ptr<Undofile> filePtr;
    m_file_pool.getPtr(filePtr, tail.m_ptr_i);
    
    if(!(tmp == tail))
    {
      Uint32 free;
      if(tmp.m_ptr_i == tail.m_ptr_i && tail.m_idx < tmp.m_idx)
      {
        jam();
	free= tmp.m_idx - tail.m_idx; 
	ptr.p->m_free_file_words += free * File_formats::UNDO_PAGE_WORDS;
	ptr.p->m_file_pos[TAIL] = tmp;
      }
      else
      {
        jam();
	free= filePtr.p->m_file_size - tail.m_idx - 1;
	ptr.p->m_free_file_words += free * File_formats::UNDO_PAGE_WORDS;
	
	Ptr<Undofile> next = filePtr;
	Local_undofile_list files(m_file_pool, ptr.p->m_files);
	while(files.next(next) && (next.p->m_state & Undofile::FS_EMPTY))
        {
	  ndbrequire(next.i != filePtr.i);
        }
	if(next.isNull())
	{
	  jam();
	  files.first(next);
	  while((next.p->m_state & Undofile::FS_EMPTY) && files.next(next))
          {
	    ndbrequire(next.i != filePtr.i);
          }
	}
	
	tmp.m_idx= 0;
	tmp.m_ptr_i= next.i;
	ptr.p->m_file_pos[TAIL] = tmp;
	done= false;      
      }
    } 
    validate_logfile_group(ptr, "cut log", jamBuffer());
  }
  
  if (done)
  {
    jam();
    ptr.p->m_state &= ~(Uint32)Logfile_group::LG_CUT_LOG_THREAD;
    m_logfile_group_list.next(ptr); 
  }
  
  if(!done || !ptr.isNull())
  {
    jam();
    ptr.p->m_state |= Logfile_group::LG_CUT_LOG_THREAD;
    signal->theData[0] = LgmanContinueB::CUT_LOG_TAIL;
    signal->theData[1] = ptr.i;
    sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
  }
}

void
Lgman::execSUB_GCP_COMPLETE_REP(Signal* signal)
{
  jamEntry();

  Ptr<Logfile_group> ptr;
  m_logfile_group_list.first(ptr);

  /**
   * Filter all logfile groups in parallell
   */
  return; // NOT IMPLEMENTED YET
  
  signal->theData[0] = LgmanContinueB::FILTER_LOG;
  while(!ptr.isNull())
  {
    jam();
    signal->theData[1] = ptr.i;
    sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
    m_logfile_group_list.next(ptr);
  }
}

int
Lgman::alloc_log_space(Uint32 ref,
                       Uint32 words,
                       EmulatedJamBuffer *jamBuf)
{
  thrjamEntry(jamBuf);
  ndbrequire(words);
  words += 2; // lsn
  Logfile_group key;
  key.m_logfile_group_id= ref;
  Ptr<Logfile_group> ptr;
  if(m_logfile_group_hash.find(ptr, key) && 
     ptr.p->m_free_file_words >= (words + (4 * File_formats::UNDO_PAGE_WORDS)))
  {
    thrjam(jamBuf);
    ptr.p->m_free_file_words -= words;
    validate_logfile_group(ptr, "alloc_log_space", jamBuf);
    return 0;
  }
  
  if(ptr.isNull())
  {
    thrjam(jamBuf);
    return -1;
  }
  thrjam(jamBuf);
  return 1501;
}

int
Lgman::free_log_space(Uint32 ref,
                      Uint32 words,
                      EmulatedJamBuffer *jamBuf)
{
  thrjamEntry(jamBuf);
  ndbrequire(words);
  Logfile_group key;
  key.m_logfile_group_id= ref;
  Ptr<Logfile_group> ptr;
  if(m_logfile_group_hash.find(ptr, key))
  {
    thrjam(jamBuf);
    ptr.p->m_free_file_words += (words + 2);
    validate_logfile_group(ptr, "free_log_space", jamBuf);
    return 0;
  }
  ndbrequire(false);
  return -1;
}

Uint64
Logfile_client::add_entry(const Change* src, Uint32 cnt)
{
  Uint32 i, tot= 0;
  jamBlock(m_client_block);
  jamBlockLine(m_client_block, cnt);
  for(i= 0; i<cnt; i++)
  {
    tot += src[i].len;
  }
  
  Uint32 *dst;
  Uint64 next_lsn= m_lgman->m_next_lsn;
  {
    Lgman::Logfile_group key;
    key.m_logfile_group_id= m_logfile_group_id;
    Ptr<Lgman::Logfile_group> ptr;
    if(m_lgman->m_logfile_group_hash.find(ptr, key))
    {
      jamBlock(m_client_block);
      Uint32 callback_buffer = ptr.p->m_callback_buffer_words;
      Uint64 next_lsn_filegroup= ptr.p->m_next_lsn;
      if(next_lsn_filegroup == next_lsn
#ifdef VM_TRACE
	 && ((rand() % 100) > 50)
#endif
	 )
      {
        jamBlock(m_client_block);
	dst= m_lgman->get_log_buffer(ptr, tot, m_client_block->jamBuffer());
	for(i= 0; i<cnt; i++)
	{
	  memcpy(dst, src[i].ptr, 4*src[i].len);
	  dst += src[i].len;
	}
	* (dst - 1) |= (File_formats::Undofile::UNDO_NEXT_LSN << 16);
	ptr.p->m_free_file_words += 2;
	m_lgman->validate_logfile_group(ptr,
                                        (const char*)0,
                                        m_client_block->jamBuffer());
      }
      else
      {
        jamBlock(m_client_block);
	dst= m_lgman->get_log_buffer(ptr,
                                     tot + 2,
                                     m_client_block->jamBuffer());
	* dst++ = (Uint32)(next_lsn >> 32);
	* dst++ = (Uint32)(next_lsn & 0xFFFFFFFF);
	for(i= 0; i<cnt; i++)
	{
	  memcpy(dst, src[i].ptr, 4*src[i].len);
	  dst += src[i].len;
	}
      }
      /**
       * for callback_buffer, always allocats 2 extra...
       *   not knowing if LSN must be added or not
       */
      tot += 2;

      if (unlikely(! (tot <= callback_buffer)))
      {
        jamBlock(m_client_block);
        abort();
      }
      ptr.p->m_callback_buffer_words = callback_buffer - tot;
    }
    m_lgman->m_next_lsn = ptr.p->m_next_lsn = next_lsn + 1;
    return next_lsn;
  }
}

/**
 * Start Recovery in LGMAN
 * -----------------------
 * Recovery in LGMAN means running the UNDO log for disk data tables.
 * This is performed after receiving information about all tables to
 * restore from DBDIH. It happens before executing the REDO log but
 * after restoring the LCP that we want to use for restoring. So we
 * have restored an old variant of the main memory data when arriving
 * here. The disk data is at least current since the last completed
 * LCP. It resides on disk, so no special preparatory action is needed
 * here.
 *
 * We need to restore the page state of the tables as they were at the
 * time of the start of the last completed LCP. We receive the latest
 * completed LCP id in this signal. We do this by executing UNDO log
 * records that roll back the state of pages by running log backwards
 * unto the point where the LCP id was started.
 *
 * After this is completed we will run the REDO log to get the table
 * into a fairly recent but consistent state.
 *
 * The steps preceding this in a restart is:
 * 1) Create the log file group (if any)
 *   This happens through the signal CREATE_FILEGROUP_IMPL_REQ
 * 2) Create the log file(s) (if any)
 *   This happens through the signal CREATE_FILE_IMPL_REQ
 *   In a restart the file is only opened since it already exists.
 *   These signals initiate the data about log file groups and
 *   log files.
 * So as we arrive here the files are opened and can immediately be used
 * to read and write to. We also have set up the necessary data structures
 * around log file groups and log files.
 */
void
Lgman::execSTART_RECREQ(Signal* signal)
{
  jamEntry();
  m_latest_lcp = signal->theData[0];
  
  Ptr<Logfile_group> ptr;
  m_logfile_group_list.first(ptr);

  if(ptr.i != RNIL)
  {
    infoEvent("LGMAN: Applying undo to LCP: %d", m_latest_lcp);
    g_eventLogger->info("LGMAN: Applying undo to LCP: %d", m_latest_lcp);
    find_log_head(signal, ptr);
    return;
  }
  /**
   * No log file groups available in the data node. This means we're
   * not using disk data in this data node. So we can immediately respond
   * the execution of UNDO log for disk data is completed.
   */
  signal->theData[0] = reference();
  sendSignal(DBLQH_REF, GSN_START_RECCONF, signal, 1, JBB);
}

void
Lgman::find_log_head(Signal* signal, Ptr<Logfile_group> ptr)
{
  ndbrequire(ptr.p->m_state & 
             (Logfile_group::LG_STARTING | Logfile_group::LG_SORTING));

  if(ptr.p->m_meta_files.isEmpty() && ptr.p->m_files.isEmpty())
  {
    jam();
    /**
     * Logfile_group wo/ any files 
     * This means we're done obviously
     */
    ptr.p->m_state &= ~(Uint32)Logfile_group::LG_STARTING;
    ptr.p->m_state |= Logfile_group::LG_ONLINE;
    m_logfile_group_list.next(ptr);
    signal->theData[0] = LgmanContinueB::FIND_LOG_HEAD;
    signal->theData[1] = ptr.i;
    sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
    return;
  }

  ptr.p->m_state = Logfile_group::LG_SORTING;
  
  /**
   * Read first page from each undofile (1 file at a time...)
   */
  Local_undofile_list files(m_file_pool, ptr.p->m_meta_files);
  Ptr<Undofile> file_ptr;
  files.first(file_ptr);
  
  if(!file_ptr.isNull())
  {
    jam();
    /**
     * Use log buffer memory when reading
     */
    Uint32 page_id = ptr.p->m_pos[CONSUMER].m_current_pos.m_ptr_i;
    file_ptr.p->m_online.m_outstanding= page_id;
    
    FsReadWriteReq* req= (FsReadWriteReq*)signal->getDataPtrSend();
    req->filePointer = file_ptr.p->m_fd;
    req->userReference = reference();
    req->userPointer = file_ptr.i;
    req->varIndex = 1; // skip zero page
    req->numberOfPages = 1;
    req->data.pageData[0] = page_id;
    req->operationFlag = 0;
    FsReadWriteReq::setFormatFlag(req->operationFlag,
				  FsReadWriteReq::fsFormatSharedPage);
    
    sendSignal(NDBFS_REF, GSN_FSREADREQ, signal,
	       FsReadWriteReq::FixedLength + 1, JBA);

    ptr.p->m_outstanding_fs++;
    file_ptr.p->m_state |= Undofile::FS_OUTSTANDING;
    return;
  }
  else
  {
    jam();
    /**
     * All files have read first page
     *   and m_files is sorted acording to lsn
     */
    ndbrequire(!ptr.p->m_files.isEmpty());
    Local_undofile_list read_files(m_file_pool, ptr.p->m_files);
    read_files.last(file_ptr);

    /**
     * Init binary search
     */
    ptr.p->m_state = Logfile_group::LG_SEARCHING;
    file_ptr.p->m_state = Undofile::FS_SEARCHING;
    ptr.p->m_file_pos[TAIL].m_idx = 1;                   // left page
    ptr.p->m_file_pos[HEAD].m_idx = file_ptr.p->m_file_size;
    ptr.p->m_file_pos[HEAD].m_ptr_i = ((file_ptr.p->m_file_size - 1) >> 1) + 1;
    
    Uint32 page_id = ptr.p->m_pos[CONSUMER].m_current_pos.m_ptr_i;
    file_ptr.p->m_online.m_outstanding= page_id;

    FsReadWriteReq* req= (FsReadWriteReq*)signal->getDataPtrSend();
    req->filePointer = file_ptr.p->m_fd;
    req->userReference = reference();
    req->userPointer = file_ptr.i;
    req->varIndex = ptr.p->m_file_pos[HEAD].m_ptr_i;
    req->numberOfPages = 1;
    req->data.pageData[0] = page_id;
    req->operationFlag = 0;
    FsReadWriteReq::setFormatFlag(req->operationFlag,
				  FsReadWriteReq::fsFormatSharedPage);
    
    sendSignal(NDBFS_REF, GSN_FSREADREQ, signal, 
	       FsReadWriteReq::FixedLength + 1, JBA);
    
    ptr.p->m_outstanding_fs++;
    file_ptr.p->m_state |= Undofile::FS_OUTSTANDING;
    return;
  }
}

void
Lgman::execFSREADCONF(Signal* signal)
{
  jamEntry();
  client_lock(number(), __LINE__);

  Ptr<Undofile> file_ptr;
  Ptr<Logfile_group> lg_ptr;
  FsConf* conf = (FsConf*)signal->getDataPtr();
  
  m_file_pool.getPtr(file_ptr, conf->userPointer);
  m_logfile_group_pool.getPtr(lg_ptr, file_ptr.p->m_logfile_group_ptr_i);

  ndbrequire(file_ptr.p->m_state & Undofile::FS_OUTSTANDING);
  file_ptr.p->m_state &= ~(Uint32)Undofile::FS_OUTSTANDING;
  
  Uint32 cnt= lg_ptr.p->m_outstanding_fs;
  ndbrequire(cnt);
  
  if((file_ptr.p->m_state & Undofile::FS_EXECUTING)== Undofile::FS_EXECUTING)
  {
    jam();
    
    if(lg_ptr.p->m_next_reply_ptr_i == file_ptr.i)
    {
      jam();
      Uint32 tot= 0;
      Local_undofile_list files(m_file_pool, lg_ptr.p->m_files);
      while(cnt && ! (file_ptr.p->m_state & Undofile::FS_OUTSTANDING))
      {
        jam();
	Uint32 state= file_ptr.p->m_state;
	Uint32 pages= file_ptr.p->m_online.m_outstanding;
	ndbrequire(pages);
	file_ptr.p->m_online.m_outstanding= 0;
	file_ptr.p->m_state &= ~(Uint32)Undofile::FS_MOVE_NEXT;
	tot += pages;
	cnt--;
	
	if((state & Undofile::FS_MOVE_NEXT) && !files.prev(file_ptr))
        {
          jam();
	  files.last(file_ptr);
        }
      }
      
      lg_ptr.p->m_outstanding_fs = cnt;
      lg_ptr.p->m_pos[PRODUCER].m_current_pos.m_idx += tot;
      lg_ptr.p->m_next_reply_ptr_i = file_ptr.i;
    }
    client_unlock(number(), __LINE__);
    return;
  }
  
  lg_ptr.p->m_outstanding_fs = cnt - 1;

  Ptr<GlobalPage> page_ptr;
  m_shared_page_pool.getPtr(page_ptr, file_ptr.p->m_online.m_outstanding);
  file_ptr.p->m_online.m_outstanding= 0;
  
  File_formats::Undofile::Undo_page* page = 
    (File_formats::Undofile::Undo_page*)page_ptr.p;
  
  Uint64 lsn = 0;
  lsn += page->m_page_header.m_page_lsn_hi; lsn <<= 32;
  lsn += page->m_page_header.m_page_lsn_lo;

  switch(file_ptr.p->m_state){
  case Undofile::FS_SORTING:
    jam();
    break;
  case Undofile::FS_SEARCHING:
    jam();
    find_log_head_in_file(signal, lg_ptr, file_ptr, lsn);
    client_unlock(number(), __LINE__);
    return;
  case Undofile::FS_SEARCHING_END:
    jam();
    find_log_head_end_check(signal, lg_ptr, file_ptr, lsn);
    client_unlock(number(), __LINE__);
    return;
  case Undofile::FS_SEARCHING_FINAL_READ:
    jam();
    find_log_head_complete(signal, lg_ptr, file_ptr);
    client_unlock(number(), __LINE__);
    return;
  default:
  case Undofile::FS_EXECUTING:
  case Undofile::FS_CREATING:
  case Undofile::FS_DROPPING:
  case Undofile::FS_ONLINE:
  case Undofile::FS_OPENING:
  case Undofile::FS_EMPTY:
    jam();
    jamLine(file_ptr.p->m_state);
    ndbrequire(false);
  }

  /**
   * Prepare for execution
   */
  file_ptr.p->m_state = Undofile::FS_EXECUTING;
  file_ptr.p->m_online.m_lsn = lsn;
  file_ptr.p->m_start_lsn = lsn;
  
  /**
   * Insert into m_files
   */
  {
    Local_undofile_list meta(m_file_pool, lg_ptr.p->m_meta_files);  
    Local_undofile_list files(m_file_pool, lg_ptr.p->m_files);
    meta.remove(file_ptr);

    Ptr<Undofile> loop;  
    files.first(loop);
    while(!loop.isNull() && loop.p->m_online.m_lsn <= lsn)
    {
      jam();
      files.next(loop);
    }
    
    if(loop.isNull())
    {
      /**
       * File has highest lsn, add last
       */
      jam();
      files.addLast(file_ptr);
    }
    else
    {
      jam();
      /**
       * Insert file in correct position in file list
       */
      files.insertBefore(file_ptr, loop);
    }
  }
  find_log_head(signal, lg_ptr);
  client_unlock(number(), __LINE__);
}
  
void
Lgman::execFSREADREF(Signal* signal)
{
  jamEntry();
  SimulatedBlock::execFSREADREF(signal);
  ndbrequire(false);
}

/**
 * We're performing a binary search to find the end of the UNDO log.
 * We're comparing with the LSN number found so far in the file.
 * At start we know the LSN of the first page, so any pages with
 * larger LSN have been written after this page, so when the page
 * LSN is greater than the highest found so far, then we move forward
 * in the binary search, otherwise we will move backwards.
 */
void
Lgman::find_log_head_in_file(Signal* signal, 
                             Ptr<Logfile_group> ptr, 
                             Ptr<Undofile> file_ptr,
                             Uint64 last_lsn)
{ 
  Uint32 curr= ptr.p->m_file_pos[HEAD].m_ptr_i;
  Uint32 head= ptr.p->m_file_pos[HEAD].m_idx;
  Uint32 tail= ptr.p->m_file_pos[TAIL].m_idx;

  ndbrequire(head > tail);
  Uint32 diff = head - tail;
  
  if(DEBUG_SEARCH_LOG_HEAD)
    printf("tail: %d(%lld) head: %d last: %d(%lld) -> ", 
	   tail, file_ptr.p->m_online.m_lsn,
	   head, curr, last_lsn);
  if(last_lsn > file_ptr.p->m_online.m_lsn)
  {
    /**
     * Move forward in binary search since page LSN is higher than the largest
     * LSN found so far.
     */
    jam();
    if(DEBUG_SEARCH_LOG_HEAD)
      printf("moving tail ");
    
    file_ptr.p->m_online.m_lsn = last_lsn;
    ptr.p->m_file_pos[TAIL].m_idx = tail = curr;
  }
  else
  {
    /**
     * A page with lower LSN than the highest is found, this means that the
     * page wasn't written in this log lap. This means that we're now close
     * to the end of the UNDO log. We'll continue the binary search to the
     * left in the log file.
     */
    jam();
    if(DEBUG_SEARCH_LOG_HEAD)
      printf("moving head ");

    ptr.p->m_file_pos[HEAD].m_idx = head = curr;
  }
  
  if(diff > 1)
  {
    jam();
    // We need to find more pages to be sure...
    ptr.p->m_file_pos[HEAD].m_ptr_i = curr = ((head + tail) >> 1);

    if(DEBUG_SEARCH_LOG_HEAD)    
      ndbout_c("-> new search tail: %d(%lld) head: %d -> %d", 
	       tail, file_ptr.p->m_online.m_lsn,
	       head, curr);

    Uint32 page_id = ptr.p->m_pos[CONSUMER].m_current_pos.m_ptr_i;
    file_ptr.p->m_online.m_outstanding= page_id;
    
    FsReadWriteReq* req= (FsReadWriteReq*)signal->getDataPtrSend();
    req->filePointer = file_ptr.p->m_fd;
    req->userReference = reference();
    req->userPointer = file_ptr.i;
    req->varIndex = curr;
    req->numberOfPages = 1;
    req->data.pageData[0] = page_id;
    req->operationFlag = 0;
    FsReadWriteReq::setFormatFlag(req->operationFlag,
				  FsReadWriteReq::fsFormatSharedPage);
    
    sendSignal(NDBFS_REF, GSN_FSREADREQ, signal, 
	       FsReadWriteReq::FixedLength + 1, JBA);
    
    ptr.p->m_outstanding_fs++;
    file_ptr.p->m_state |= Undofile::FS_OUTSTANDING;
    return;
  }
  
  ndbrequire(diff == 1);
  /**
   * We have found the end of the UNDO log through a binary search of the
   * UNDO log pages. There can still be pages up to 16 MByte ahead of us.
   * We will look for more pages up to 16 MBytes ahead or until we find the
   * end of the file.
   *
   * The reason is that we synch up to 16 MByte at a time to the
   * UNDO log, so this means that we must take into account the
   * case that the UNDO log is not written consecutively at the
   * end.
   *
   * We want to find the last page which have been written to
   * avoid weirdness later when applying the UNDO log. We will
   * keep track that no records are applied until we have executed
   * backwards until we have found this end since it is impossible
   * that a log record is applied if it lies in this region of
   * written/unwritten log pages. This is because of that we
   * use the WAL protocol to write pages to disk.
   */

  if(DEBUG_SEARCH_LOG_HEAD)    
    ndbout_c("-> found last page in binary search: %d", tail);

  /**
   * m_next_lsn indicates next LSN to write, so we step this forward one
   * step to ensure that we don't write one more record with the same
   * LSN number.
   */
  m_next_lsn = ptr.p->m_next_lsn = (file_ptr.p->m_online.m_lsn + 1);
  ptr.p->m_last_read_lsn = file_ptr.p->m_online.m_lsn;
  ptr.p->m_last_synced_lsn = file_ptr.p->m_online.m_lsn;
  
  /**
   * Set HEAD and TAIL position to use when we start logging again.
   * We might have to change those during the check of the end of
   * the log file search check. But we won't change the file, only
   * end page index.
   */
  ptr.p->m_file_pos[HEAD].m_ptr_i = file_ptr.i;
  ptr.p->m_file_pos[HEAD].m_idx = tail;
  
  ptr.p->m_file_pos[TAIL].m_ptr_i = file_ptr.i;
  ptr.p->m_file_pos[TAIL].m_idx = tail - 1;
  ptr.p->m_next_reply_ptr_i = file_ptr.i;


  file_ptr.p->m_state = Undofile::FS_SEARCHING_END;

  file_ptr.p->m_online.m_current_scan_index = curr;
  file_ptr.p->m_online.m_current_scanned_pages = 0;
  file_ptr.p->m_online.m_binary_search_end = true;
  find_log_head_end_check(signal, ptr, file_ptr, last_lsn);
}

void
Lgman::find_log_head_end_check(Signal* signal,
                               Ptr<Logfile_group> ptr,
                               Ptr<Undofile> file_ptr,
                               Uint64 last_lsn)
{
  Uint32 curr = file_ptr.p->m_online.m_current_scan_index;
  Uint32 scanned_pages = file_ptr.p->m_online.m_current_scanned_pages;

  if(last_lsn > file_ptr.p->m_online.m_lsn)
  {
    /**
     * We did actually find a written page after the end which the binary
     * search found. We need to record this as the new end of the UNDO
     * log.
     */
    jam();
    if (file_ptr.p->m_online.m_binary_search_end)
    {
      jam();
      file_ptr.p->m_online.m_binary_search_end = false;
      g_eventLogger->info("LGMAN: Found written page after end found by binary"
                          " search, binary search head found: %u",
                          ptr.p->m_file_pos[HEAD].m_idx);
    }
    ptr.p->m_file_pos[HEAD].m_idx = curr;
    ptr.p->m_file_pos[TAIL].m_idx = curr - 1;

    /**
     * m_next_lsn indicates next LSN to write, so we step this forward one
     * step to ensure that we don't write one more record with the same
     * LSN number.
     */
    m_next_lsn = ptr.p->m_next_lsn = (file_ptr.p->m_online.m_lsn + 1);
    ptr.p->m_last_read_lsn = file_ptr.p->m_online.m_lsn;
    ptr.p->m_last_synced_lsn = file_ptr.p->m_online.m_lsn;
  }

  curr++;
  scanned_pages++;
  file_ptr.p->m_online.m_current_scan_index = curr;
  file_ptr.p->m_online.m_current_scanned_pages = scanned_pages;
  if ((curr < file_ptr.p->m_file_size) &&
      (scanned_pages <= MAX_UNDO_PAGES_OUTSTANDING))
  {
    jam();
    Uint32 page_id = ptr.p->m_pos[CONSUMER].m_current_pos.m_ptr_i;
    file_ptr.p->m_online.m_outstanding= page_id;

    FsReadWriteReq* req= (FsReadWriteReq*)signal->getDataPtrSend();
    req->filePointer = file_ptr.p->m_fd;
    req->userReference = reference();
    req->userPointer = file_ptr.i;
    req->varIndex = curr;
    req->numberOfPages = 1;
    req->data.pageData[0] = page_id;
    req->operationFlag = 0;
    FsReadWriteReq::setFormatFlag(req->operationFlag,
                                  FsReadWriteReq::fsFormatSharedPage);

    sendSignal(NDBFS_REF, GSN_FSREADREQ, signal,
               FsReadWriteReq::FixedLength + 1, JBA);

    ptr.p->m_outstanding_fs++;
    file_ptr.p->m_state |= Undofile::FS_OUTSTANDING;
    return;
  }
  jam();

  /**
   * Now we are done with the search for the end. However when starting to
   * execute the UNDO log we expect the first page to be already read. So
   * we reread the last page in the UNDO log that we found.
   */
  Uint32 page_id = ptr.p->m_pos[CONSUMER].m_current_pos.m_ptr_i;
  file_ptr.p->m_online.m_outstanding= page_id;
  curr = ptr.p->m_file_pos[HEAD].m_idx;

  FsReadWriteReq* req= (FsReadWriteReq*)signal->getDataPtrSend();
  req->filePointer = file_ptr.p->m_fd;
  req->userReference = reference();
  req->userPointer = file_ptr.i;
  req->varIndex = curr;
  req->numberOfPages = 1;
  req->data.pageData[0] = page_id;
  req->operationFlag = 0;
  FsReadWriteReq::setFormatFlag(req->operationFlag,
                                FsReadWriteReq::fsFormatSharedPage);

  sendSignal(NDBFS_REF, GSN_FSREADREQ, signal,
             FsReadWriteReq::FixedLength + 1, JBA);

  ptr.p->m_outstanding_fs++;
  file_ptr.p->m_state = Undofile::FS_SEARCHING_FINAL_READ;
  file_ptr.p->m_state |= Undofile::FS_OUTSTANDING;
  return;
}

void
Lgman::find_log_head_complete(Signal *signal,
                              Ptr<Logfile_group> ptr,
                              Ptr<Undofile> file_ptr)
{
  Uint32 head = ptr.p->m_file_pos[HEAD].m_idx;
  /**
   * END_OF_UNDO_LOG_FOUND
   *
   * We have found the end of the UNDO log. This is the position
   * from which we will start the UNDO execution and the position
   * from which we later will start adding new UNDO log records.
   * We have just reread the last UNDO log page, so we are ready
   * to start executing the UNDO log.
   */
  ptr.p->m_state = 0;
  file_ptr.p->m_state = Undofile::FS_EXECUTING;

  {
    Local_undofile_list files(m_file_pool, ptr.p->m_files);
    if(head == 1)
    {
      jam();
      /**
       * HEAD is first page in a file...
       *   -> TAIL should be last page in previous file
       */
      Ptr<Undofile> prev = file_ptr;
      if(!files.prev(prev))
      {
        jam();
	files.last(prev);
      }
      ptr.p->m_file_pos[TAIL].m_ptr_i = prev.i;
      ptr.p->m_file_pos[TAIL].m_idx = prev.p->m_file_size - 1;
      ptr.p->m_next_reply_ptr_i = prev.i;
    }
    
    SimulatedBlock* fs = globalData.getBlock(NDBFS);
    infoEvent("LGMAN: Undo head - %s page: %d lsn: %lld",
	      fs->get_filename(file_ptr.p->m_fd), 
	      head, file_ptr.p->m_online.m_lsn);
    g_eventLogger->info("LGMAN: Undo head - %s page: %d lsn: %lld",
                        fs->get_filename(file_ptr.p->m_fd),
                        head, file_ptr.p->m_online.m_lsn);
    
    for(files.prev(file_ptr); !file_ptr.isNull(); files.prev(file_ptr))
    {
      infoEvent("   - next - %s(%lld)", 
		fs->get_filename(file_ptr.p->m_fd), 
		file_ptr.p->m_online.m_lsn);

      g_eventLogger->info("   - next - %s(%lld)", 
                          fs->get_filename(file_ptr.p->m_fd),
                          file_ptr.p->m_online.m_lsn);
    }
  }
  
  /**
   * Start next logfile group
   */
  m_logfile_group_list.next(ptr);
  signal->theData[0] = LgmanContinueB::FIND_LOG_HEAD;
  signal->theData[1] = ptr.i;
  sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
}

void
Lgman::init_run_undo_log(Signal* signal)
{
  /**
   * Perform initial sorting of logfile groups
   */
  Ptr<Logfile_group> group;
  Logfile_group_list& list= m_logfile_group_list;
  Logfile_group_list::Head tmpHead;
  bool found_any = false;
  {
    Logfile_group_list::Local tmp(m_logfile_group_pool, tmpHead);

    list.first(group);
    while (!group.isNull())
    {
      jam();
      Ptr<Logfile_group> ptr= group;
      list.next(group);
      list.remove(ptr);

      if (ptr.p->m_state & Logfile_group::LG_ONLINE)
      {
        /**
         * No logfiles in group
         */
        jam();
        tmp.addLast(ptr);
        continue;
      }

      found_any = true;

      {
        /**
         * Init buffer pointers
         */
        ptr.p->m_free_buffer_words -= File_formats::UNDO_PAGE_WORDS;
        ptr.p->m_pos[CONSUMER].m_current_page.m_idx = 0; // 0 more pages read
        ptr.p->m_pos[PRODUCER].m_current_page.m_idx = 0; // 0 more pages read

        Uint32 page = ptr.p->m_pos[CONSUMER].m_current_pos.m_ptr_i;
        File_formats::Undofile::Undo_page* pageP =
          (File_formats::Undofile::Undo_page*)m_shared_page_pool.getPtr(page);

        ptr.p->m_pos[CONSUMER].m_current_pos.m_idx = pageP->m_words_used;
        ptr.p->m_pos[PRODUCER].m_current_pos.m_idx = 1;
        ptr.p->m_last_read_lsn++;

        ptr.p->m_consumer_file_pos = ptr.p->m_file_pos[HEAD];
      }

      /**
       * Start producer thread
       */
      signal->theData[0] = LgmanContinueB::READ_UNDO_LOG;
      signal->theData[1] = ptr.i;
      sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
    
      /**
       * Insert in correct position in list of logfile_group's
       */
      Ptr<Logfile_group> pos;
      for (tmp.first(pos); !pos.isNull(); tmp.next(pos))
      {
        jam();
        if (ptr.p->m_last_read_lsn >= pos.p->m_last_read_lsn)
        {
          break;
        }
      }
    
      if (pos.isNull())
      {
        jam();
        tmp.addLast(ptr);
      }
      else
      {
        jam();
        tmp.insertBefore(ptr, pos);
      }
    
      ptr.p->m_state =
        Logfile_group::LG_EXEC_THREAD | Logfile_group::LG_READ_THREAD;
    }
  }
  ndbassert(list.isEmpty());
  list.appendList(tmpHead);

  if (found_any == false)
  {
    /**
     * No logfilegroup had any logfiles
     */
    jam();
    signal->theData[0] = reference();
    sendSignal(DBLQH_REF, GSN_START_RECCONF, signal, 1, JBB);
    return;
  }
  
  execute_undo_record(signal);
}

void
Lgman::read_undo_log(Signal* signal, Ptr<Logfile_group> ptr)
{
  Uint32 cnt, free= ptr.p->m_free_buffer_words;

  if(! (ptr.p->m_state & Logfile_group::LG_EXEC_THREAD))
  {
    jam();
    /**
     * Logfile_group is done...
     */
    ptr.p->m_state &= ~(Uint32)Logfile_group::LG_READ_THREAD;
    stop_run_undo_log(signal);
    return;
  }
  
  if(free <= File_formats::UNDO_PAGE_WORDS)
  {
    signal->theData[0] = LgmanContinueB::READ_UNDO_LOG;
    signal->theData[1] = ptr.i;
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 100, 2);
    return;
  }

  Logfile_group::Position producer= ptr.p->m_pos[PRODUCER];
  Logfile_group::Position consumer= ptr.p->m_pos[CONSUMER];

  if(producer.m_current_page.m_idx == 0)
  {
    jam();
    /**
     * zero pages left in range -> switch range
     */
    Lgman::Page_map::Iterator it;
    Page_map map(m_data_buffer_pool, ptr.p->m_buffer_pages);
    Uint32 sz = map.getSize();
    Uint32 pos= (producer.m_current_page.m_ptr_i + sz - 2) % sz;
    map.position(it, pos);
    union {
      Uint32 _tmp[2];
      Lgman::Buffer_idx range;
    };
    _tmp[0] = *it.data; map.next(it); _tmp[1] = *it.data;
    producer.m_current_page.m_ptr_i = pos;
    producer.m_current_page.m_idx = range.m_idx;
    producer.m_current_pos.m_ptr_i = range.m_ptr_i + range.m_idx;
  }
  
  if(producer.m_current_page.m_ptr_i == consumer.m_current_page.m_ptr_i &&
     producer.m_current_pos.m_ptr_i > consumer.m_current_pos.m_ptr_i)
  {
    jam();
    Uint32 max= 
      producer.m_current_pos.m_ptr_i - consumer.m_current_pos.m_ptr_i - 1;
    ndbrequire(free >= max * File_formats::UNDO_PAGE_WORDS);
    cnt= read_undo_pages(signal, ptr, producer.m_current_pos.m_ptr_i, max);
    ndbrequire(cnt <= max);    
    producer.m_current_pos.m_ptr_i -= cnt;
    producer.m_current_page.m_idx -= cnt;
  } 
  else
  {
    jam();
    Uint32 max= producer.m_current_page.m_idx;
    ndbrequire(free >= max * File_formats::UNDO_PAGE_WORDS);
    cnt= read_undo_pages(signal, ptr, producer.m_current_pos.m_ptr_i, max);
    ndbrequire(cnt <= max);
    producer.m_current_pos.m_ptr_i -= cnt;
    producer.m_current_page.m_idx -= cnt;
  } 
  
  ndbrequire(free >= cnt * File_formats::UNDO_PAGE_WORDS);
  free -= (cnt * File_formats::UNDO_PAGE_WORDS);
  ptr.p->m_free_buffer_words = free;
  ptr.p->m_pos[PRODUCER] = producer;  

  signal->theData[0] = LgmanContinueB::READ_UNDO_LOG;
  signal->theData[1] = ptr.i;

  if(free > File_formats::UNDO_PAGE_WORDS)
  {
    jam();
    sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
  }
  else
  {
    jam();
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 100, 2);
  }
}

Uint32
Lgman::read_undo_pages(Signal* signal, Ptr<Logfile_group> ptr, 
		       Uint32 pageId, Uint32 pages)
{
  ndbrequire(pages);
  Ptr<Undofile> filePtr;
  Buffer_idx tail= ptr.p->m_file_pos[TAIL];
  m_file_pool.getPtr(filePtr, tail.m_ptr_i);
  
  if(filePtr.p->m_online.m_outstanding > 0)
  {
    jam();
    return 0;
  }

  Uint32 max= tail.m_idx;

  FsReadWriteReq* req= (FsReadWriteReq*)signal->getDataPtrSend();
  req->filePointer = filePtr.p->m_fd;
  req->userReference = reference();
  req->userPointer = filePtr.i;
  req->operationFlag = 0;
  FsReadWriteReq::setFormatFlag(req->operationFlag, 
				FsReadWriteReq::fsFormatSharedPage);


  if(max > pages)
  {
    jam();
    tail.m_idx -= pages;

    req->varIndex = 1 + tail.m_idx;
    req->numberOfPages = pages;
    req->data.pageData[0] = pageId - pages;
    ptr.p->m_file_pos[TAIL] = tail;
    
    if(DEBUG_UNDO_EXECUTION)
      ndbout_c("a reading from file: %d page(%d-%d) into (%d-%d)",
	       ptr.i, 1 + tail.m_idx, 1+tail.m_idx+pages-1,
	       pageId - pages, pageId - 1);

    sendSignal(NDBFS_REF, GSN_FSREADREQ, signal, 
	       FsReadWriteReq::FixedLength + 1, JBA);
    
    ptr.p->m_outstanding_fs++;
    filePtr.p->m_state |= Undofile::FS_OUTSTANDING;
    filePtr.p->m_online.m_outstanding = pages;
    max = pages;
  }
  else
  {
    jam();

    ndbrequire(tail.m_idx - max == 0);
    req->varIndex = 1;
    req->numberOfPages = max;
    req->data.pageData[0] = pageId - max;
    
    if(DEBUG_UNDO_EXECUTION)
      ndbout_c("b reading from file: %d page(%d-%d) into (%d-%d)",
	       ptr.i, 1 , 1+max-1,
	       pageId - max, pageId - 1);
    
    sendSignal(NDBFS_REF, GSN_FSREADREQ, signal, 
	       FsReadWriteReq::FixedLength + 1, JBA);
    
    ptr.p->m_outstanding_fs++;
    filePtr.p->m_online.m_outstanding = max;
    filePtr.p->m_state |= Undofile::FS_OUTSTANDING | Undofile::FS_MOVE_NEXT;
    
    Ptr<Undofile> prev = filePtr;
    {
      Local_undofile_list files(m_file_pool, ptr.p->m_files);
      if(!files.prev(prev))
      {
	jam();
	files.last(prev);
      }
    }
    if(DEBUG_UNDO_EXECUTION)
      ndbout_c("changing file from %d to %d", filePtr.i, prev.i);

    tail.m_idx= prev.p->m_file_size - 1;
    tail.m_ptr_i= prev.i;
    ptr.p->m_file_pos[TAIL] = tail;
    if(max < pages && filePtr.i != prev.i)
    {
      jam();
      max += read_undo_pages(signal, ptr, pageId - max, pages - max);
    }
  }
  return max;
}

void
Lgman::execute_undo_record(Signal* signal)
{
  /**
   * This code isn't prepared to handle more than one logfile group.
   * To support multiple logfile groups one needs to adapt this code
   * for this requirement.
   */

  Uint64 lsn;
  const Uint32* ptr;
  if((ptr = get_next_undo_record(&lsn)))
  {
    Uint32 len= (* ptr) & 0xFFFF;
    Uint32 type= (* ptr) >> 16;
    Uint32 mask= type & (~((Uint32)File_formats::Undofile::UNDO_NEXT_LSN));
    switch(mask){
    case File_formats::Undofile::UNDO_END:
      jam();
      g_eventLogger->info("LGMAN: Stop UNDO log execution at LSN %llu,"
                          " found END record",
                          lsn);
      stop_run_undo_log(signal);
      return;
    case File_formats::Undofile::UNDO_LCP:
    case File_formats::Undofile::UNDO_LCP_FIRST:
    {
      jam();
      Uint32 lcp = * (ptr - len + 1);
      if(m_latest_lcp && lcp > m_latest_lcp)
      {
        jam();
        if (0)
        {
	  const Uint32 * base = ptr - len + 1;
          Uint32 lcp = base[0];
          Uint32 tableId = base[1] >> 16;
          Uint32 fragId = base[1] & 0xFFFF;

	  ndbout_c("NOT! ignoring lcp: %u tab: %u frag: %u", 
		   lcp, tableId, fragId);
	}
      }

      if(m_latest_lcp == 0 || 
	 lcp < m_latest_lcp || 
	 (lcp == m_latest_lcp && 
	  mask == File_formats::Undofile::UNDO_LCP_FIRST))
      {
        jam();
        g_eventLogger->info("LGMAN: Stop UNDO log execution at LSN %llu,"
                            " found LCP record",
                            lsn);
	stop_run_undo_log(signal);
	return;
      }
      // Fallthrough
    }
    case File_formats::Undofile::UNDO_TUP_ALLOC:
    case File_formats::Undofile::UNDO_TUP_UPDATE:
    case File_formats::Undofile::UNDO_TUP_FREE:
    case File_formats::Undofile::UNDO_TUP_CREATE:
    case File_formats::Undofile::UNDO_TUP_DROP:
    case File_formats::Undofile::UNDO_TUP_ALLOC_EXTENT:
    case File_formats::Undofile::UNDO_TUP_FREE_EXTENT:
      {
        jam();
        jamLine(mask);
        Dbtup_client tup(this, m_tup);
        tup.disk_restart_undo(signal, lsn, mask, ptr - len + 1, len);
        jamEntry();
      }
      return;
    default:
      ndbrequire(false);
    }
  }
  signal->theData[0] = LgmanContinueB::EXECUTE_UNDO_RECORD;
  signal->theData[1] = 0; /* Not applied flag */
  sendSignal(LGMAN_REF, GSN_CONTINUEB, signal, 2, JBB);
  return;
}

/**
 * Move back one page in the file position of the currently
 * executing UNDO log. Change to previous file if needed.
 */
void Lgman::update_consumer_file_pos(Ptr<Logfile_group> lg_ptr)
{
  Buffer_idx consumer_file_pos = lg_ptr.p->m_consumer_file_pos;
  if (consumer_file_pos.m_idx == 1)
  {
    /* We switch to a new file now */
    jam();
    Ptr<Undofile> filePtr;
    m_file_pool.getPtr(filePtr, consumer_file_pos.m_ptr_i);
    Ptr<Undofile> prev = filePtr;
    Local_undofile_list files(m_file_pool, lg_ptr.p->m_files);
    if(!files.prev(prev))
    {
      jam();
      files.last(prev);
    }
    consumer_file_pos.m_ptr_i = prev.i;
    consumer_file_pos.m_idx = prev.p->m_file_size - 1;
  }
  else
  {
    jam();
    consumer_file_pos.m_idx--;
  }
  lg_ptr.p->m_consumer_file_pos = consumer_file_pos;
}

const Uint32*
Lgman::get_next_undo_record(Uint64 * this_lsn)
{
  Ptr<Logfile_group> ptr;
  m_logfile_group_list.first(ptr);

  Logfile_group::Position consumer= ptr.p->m_pos[CONSUMER];
  if(ptr.p->m_pos[PRODUCER].m_current_pos.m_idx < 2)
  {
    jam();
    /**
     * Wait for fetching pages...
     */
    return 0;
  }
  
  Uint32 pos = consumer.m_current_pos.m_idx;
  Uint32 page = consumer.m_current_pos.m_ptr_i;
  
  File_formats::Undofile::Undo_page* pageP=(File_formats::Undofile::Undo_page*)
    m_shared_page_pool.getPtr(page);


  if (ptr.p->m_last_read_lsn == (Uint64)1)
  {
    /**
     * End of log, we hadn't concluded any LCPs before the crash.
     * So we find the end of the log by noting that we expect this LSN
     * number to be 0 which doesn't exist.
     *
     * When initialising we have also written an UNDO_END log record
     * into the first page, so we should not be able to run past that
     * point even when the first pages in the file are unwritten and
     * others ahead are written which is a difficult corner case to
     * handle.
     */
    jam();
    pageP->m_data[0] = (File_formats::Undofile::UNDO_END << 16) | 1 ;
    pageP->m_page_header.m_page_lsn_hi = 0;
    pageP->m_page_header.m_page_lsn_lo = 0;
    ptr.p->m_pos[CONSUMER].m_current_pos.m_idx= pageP->m_words_used = 1;
    this_lsn = 0;
    return pageP->m_data;
  }

  /**
   * Before we start using the page we need to verify that the page has a
   * LSN which is what we expect it to be. It needs to be bigger than the
   * LSN of the first LSN in the file and it needs to be smaller than the
   * LSN of the previous UNDO log record applied since we are applying the
   * UNDO log backwards.
   *
   * If we discover a page which has an invalid LSN it means this page was
   * unwritten and should be ignored. Such pages are ok to encounter at the
   * end of the UNDO log. If we encounter such a page after we already
   * applied an UNDO log then the WAL protocol says that this is wrong since
   * we should have sync:ed everything before that LSN and thus no unwritten
   * pages are ok to encounter anymore.
   */

  Uint32 page_position = pageP->m_words_used;
  bool ignore_page = false;
  bool new_page;

  if (page_position == pos)
  {
    jam();
    /**
     * This is the first log entry in a new page, we need to
     * verify this page before we start using it.
     */
    Uint64 page_lsn = pageP->m_page_header.m_page_lsn_hi;
    page_lsn <<= 32;
    page_lsn += pageP->m_page_header.m_page_lsn_lo;
    if (page_lsn != (ptr.p->m_last_read_lsn - 1))
    {
      jam();
      /**
       * The page LSN wasn't the expected one. We need to verify that
       * it is ok that this page is here.
       */
      Ptr<Undofile> filePtr;
      m_file_pool.getPtr(filePtr, ptr.p->m_consumer_file_pos.m_ptr_i);
      /**
       * Due to an old bug we can have rewrite an LSN number, so we can
       * only assert on that we don't write a second LSN with the same
       * number, but we require that it isn't more than two LSNs with
       * the same number.
       *
       * We can upgrade this assert to a require when we are sure that
       * the log wasn't produced by these older versions.
       */
      ndbassert(page_lsn < (ptr.p->m_last_read_lsn - 1));
      ndbrequire(page_lsn < (ptr.p->m_last_read_lsn - 1) ||
                 page_lsn == ptr.p->m_last_read_lsn);
      if (filePtr.p->m_start_lsn <= page_lsn)
      {
        /**
         * A normal page, continue as usual.
         * However given that we now have skipped over a few pages we need to
         * set back the last read LSN to be the last LSN of the previous page
         * that was never written, or in other words this pageLSN + 1.
         */
        jam();
        ptr.p->m_last_read_lsn = page_lsn + 1;
        SimulatedBlock* fs = globalData.getBlock(NDBFS);
        g_eventLogger->info("LGMAN: Continue applying log records in written"
                            "page: %u in the file %s",
                            ptr.p->m_consumer_file_pos.m_idx,
                            fs->get_filename(filePtr.p->m_fd));
      }
      else
      {
        jam();
        if (ptr.p->m_applied)
        {
          /**
           * We need to crash since we found a not OK page after an UNDO log
           * record have already been applied.
           */
          SimulatedBlock* fs = globalData.getBlock(NDBFS);
          g_eventLogger->info("LGMAN: File %s have wrong pageLSN in page: %u",
                              fs->get_filename(filePtr.p->m_fd),
                              ptr.p->m_consumer_file_pos.m_idx);
          progError(__LINE__, NDBD_EXIT_SR_UNDOLOG);
        }
        SimulatedBlock* fs = globalData.getBlock(NDBFS);
        g_eventLogger->info("LGMAN: Ignoring log records in unwritten page: "
                            "%u in the file %s",
                            ptr.p->m_consumer_file_pos.m_idx,
                            fs->get_filename(filePtr.p->m_fd));
        ignore_page = true;
        new_page = true;
      }
    }
  }
  /**
   * Read the UNDO record
   */
  Uint32 *record = NULL;
  if (!ignore_page)
  {
    jam();
    record= pageP->m_data + pos - 1;
    Uint32 len= (* record) & 0xFFFF;
    ndbrequire(len);
    Uint32 *prev= record - len;
    Uint64 lsn = 0;

    if (((* record) >> 16) & (File_formats::Undofile::UNDO_NEXT_LSN))
    {
      /* This was a Type 1 record, previous LSN is -1 of current */
      jam();
      lsn = ptr.p->m_last_read_lsn - 1;
      ndbrequire((Int64)lsn >= 0);
    }
    else
    {
      /**
       * This was a Type 2 record, previous LSN given by LSNs in the UNDO
       * log record, see UNDO log record layout in beginning of file.
       */
      ndbrequire(pos >= 3);
      jam();
      lsn += * (prev - 1); lsn <<= 32;
      lsn += * (prev - 0);
      len += 2;
      ndbrequire((Int64)lsn >= 0);
    }
    *this_lsn = ptr.p->m_last_read_lsn = lsn;
    ndbrequire(pos >= len);
    new_page = (pos == len);
    consumer.m_current_pos.m_idx -= len;
  }

  /**
   * Now step back to previous UNDO log record. Also change to new page
   * if necessary.
   */

  if (new_page)
  {
    /**
     * Switching to next page in our backwards scan of UNDO log pages.
     *
     * The length of the UNDO record is the same as the position of the
     * header of the UNDO log record which means that this was the first
     * UNDO log record in the page and thus the last UNDO log record to
     * apply in the page. We prepare moving to the next page.
     */
    jam();
    ndbrequire(ptr.p->m_pos[PRODUCER].m_current_pos.m_idx);
    ptr.p->m_pos[PRODUCER].m_current_pos.m_idx--;

    if(consumer.m_current_page.m_idx)
    {
      jam();
      consumer.m_current_page.m_idx--;   // left in range
      consumer.m_current_pos.m_ptr_i--; // page
    }
    else
    {
      jam();
      // 0 pages left in range...switch range
      Lgman::Page_map::Iterator it;
      Page_map map(m_data_buffer_pool, ptr.p->m_buffer_pages);
      Uint32 sz = map.getSize();
      Uint32 tmp = (consumer.m_current_page.m_ptr_i + sz - 2) % sz;
      
      map.position(it, tmp);
      union {
	Uint32 _tmp[2];
	Lgman::Buffer_idx range;
      };
      
      _tmp[0] = *it.data;
      map.next(it);
      _tmp[1] = *it.data;
      
      consumer.m_current_page.m_idx = range.m_idx - 1; // left in range
      consumer.m_current_page.m_ptr_i = tmp;           // pos in map

      consumer.m_current_pos.m_ptr_i = range.m_ptr_i + range.m_idx - 1; // page
    }

    if(DEBUG_UNDO_EXECUTION)
      ndbout_c("reading from %d", consumer.m_current_pos.m_ptr_i);

    ptr.p->m_free_buffer_words += File_formats::UNDO_PAGE_WORDS;

    /**
     * We have switched to a new page. Before starting to apply this page
     * we need to ensure that this page is containing valid entries to
     * apply. We might come to pages that don't have an accurate set of
     * LSNs. This should however never happen after we reach an UNDO log
     * record that have been applied due to the WAL protocol. If this
     * happens the UNDO log is inconsistent and we need to crash with
     * a report about this.
     *
     * We will verify the LSN of the page by checking that the LSN of the
     * page is bigger than or equal to the LSN of the first page in the
     * file. To do this we need to keep track of the current file and to
     * keep track of this we simply keep track of the file position of the
     * UNDO logs we execute.
     *
     * We will then verify that the page LSN is smaller than the highest
     * we reached so far, but still bigger or equal to the page LSN of the
     * first page in the file we are currently executing the UNDO log in.
     */
    update_consumer_file_pos(ptr);

    pageP=(File_formats::Undofile::Undo_page*)
      m_shared_page_pool.getPtr(consumer.m_current_pos.m_ptr_i);

    consumer.m_current_pos.m_idx = pageP->m_words_used;
  }
  ptr.p->m_pos[CONSUMER] = consumer;

  /**
   * Re-sort log file groups
   * This is code prepared for future use of multiple logfile groups.
   * We comment it out for now.
   */
#if 0
  Ptr<Logfile_group> sort = ptr;
  if(m_logfile_group_list.next(sort))
  {
    jam();
    while(!sort.isNull() && sort.p->m_last_read_lsn > lsn)
    {
      jam();
      m_logfile_group_list.next(sort);
    }
    
    if(sort.i != ptr.p->nextList)
    {
      m_logfile_group_list.remove(ptr);
      if(sort.isNull())
      {
        jam();
        m_logfile_group_list.addLast(ptr);
      }
      else
      {
        jam();
        m_logfile_group_list.insertBefore(ptr, sort);
      }
    }
  }
#endif
  return record;
}

void
Lgman::stop_run_undo_log(Signal* signal)
{
  bool running = false, outstanding = false;
  Ptr<Logfile_group> ptr;
  m_logfile_group_list.first(ptr);
  while(!ptr.isNull())
  {
    jam();
    /**
     * Mark exec thread as completed
     */
    ptr.p->m_state &= ~(Uint32)Logfile_group::LG_EXEC_THREAD;

    if(ptr.p->m_state & Logfile_group::LG_READ_THREAD)
    {
      jam();
      /**
       * Thread is still running...wait for it to complete
       */
      running = true;
    }
    else if(ptr.p->m_outstanding_fs)
    {
      jam();
      outstanding = true; // a FSREADREQ is outstanding...wait for it
    }
    else if(ptr.p->m_state != Logfile_group::LG_ONLINE)
    {
      jam();
      /**
       * Fix log TAIL
       */
      ndbrequire(ptr.p->m_state == 0);
      ptr.p->m_state = Logfile_group::LG_ONLINE;
      Buffer_idx tail= ptr.p->m_file_pos[TAIL];
      Uint32 pages= ptr.p->m_pos[PRODUCER].m_current_pos.m_idx;
      
      while(pages)
      {
	Ptr<Undofile> file;
	m_file_pool.getPtr(file, tail.m_ptr_i);
	Uint32 page= tail.m_idx;
	Uint32 size= file.p->m_file_size;
	ndbrequire(size >= page);
	Uint32 diff= size - page;
	
	if(pages >= diff)
	{
          jam();
	  pages -= diff;
	  Local_undofile_list files(m_file_pool, ptr.p->m_files);
	  if(!files.next(file))
	    files.first(file);
	  tail.m_idx = 1;
	  tail.m_ptr_i= file.i;
	}
	else
	{
          jam();
	  tail.m_idx += pages;
	  pages= 0;
	}
      }
      ptr.p->m_tail_pos[0] = tail;
      ptr.p->m_tail_pos[1] = tail;
      ptr.p->m_tail_pos[2] = tail;
      ptr.p->m_file_pos[TAIL] = tail;

      init_logbuffer_pointers(ptr);

      {
	Buffer_idx head= ptr.p->m_file_pos[HEAD];
	Ptr<Undofile> file;
	m_file_pool.getPtr(file, head.m_ptr_i);
	if (head.m_idx == file.p->m_file_size - 1)
	{
          jam();
	  Local_undofile_list files(m_file_pool, ptr.p->m_files);
	  if(!files.next(file))
	  {
	    jam();
	    files.first(file);
	  }
	  head.m_idx = 0;
	  head.m_ptr_i = file.i;
	  ptr.p->m_file_pos[HEAD] = head;
	}
      }
      
      client_lock(number(), __LINE__);
      ptr.p->m_free_file_words = (Uint64)File_formats::UNDO_PAGE_WORDS * 
	(Uint64)compute_free_file_pages(ptr, jamBuffer());
      client_unlock(number(), __LINE__);
      ptr.p->m_next_reply_ptr_i = ptr.p->m_file_pos[HEAD].m_ptr_i;
      
      ptr.p->m_state |= Logfile_group::LG_FLUSH_THREAD;
      signal->theData[0] = LgmanContinueB::FLUSH_LOG;
      signal->theData[1] = ptr.i;
      signal->theData[2] = 0;
      sendSignal(reference(), GSN_CONTINUEB, signal, 3, JBB);

      if(1)
      {
	SimulatedBlock* fs = globalData.getBlock(NDBFS);
	Ptr<Undofile> hf, tf;
	m_file_pool.getPtr(tf, tail.m_ptr_i);
	m_file_pool.getPtr(hf,  ptr.p->m_file_pos[HEAD].m_ptr_i);
	infoEvent("LGMAN: Logfile group: %d ", ptr.p->m_logfile_group_id);
        g_eventLogger->info("LGMAN: Logfile group: %d ",
                            ptr.p->m_logfile_group_id);
	infoEvent("  head: %s page: %d",
                  fs->get_filename(hf.p->m_fd),
                  ptr.p->m_file_pos[HEAD].m_idx);
        g_eventLogger->info("  head: %s page: %d",
                            fs->get_filename(hf.p->m_fd),
                            ptr.p->m_file_pos[HEAD].m_idx);
	infoEvent("  tail: %s page: %d",
		  fs->get_filename(tf.p->m_fd), tail.m_idx);
        g_eventLogger->info("  tail: %s page: %d",
                            fs->get_filename(tf.p->m_fd), tail.m_idx);
      }
    }
    
    m_logfile_group_list.next(ptr);
  }
  
  if(running)
  {
    jam();
    return;
  }
  
  if(outstanding)
  {
    jam();
    signal->theData[0] = LgmanContinueB::STOP_UNDO_LOG;
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 100, 1);
    return;
  }
  
  infoEvent("LGMAN: Flushing page cache after undo completion");
  g_eventLogger->info("LGMAN: Flushing page cache after undo completion");

  /**
   * START_FLUSH_PGMAN_CACHE
   * 
   * Start flushing pages (a form of a local LCP)
   *
   * As part of a restart we want to ensure that we don't need to replay
   * the UNDO log again. This is done by ensuring that all pages
   * currently in PGMAN cache is flushed to disk. We do this by faking
   * a LCP to PGMAN by a first LCP_FRAG_ORD followed by END_LCP_REQ.
   */

  LcpFragOrd * ord = (LcpFragOrd *)signal->getDataPtr();
  ord->lcpId = m_latest_lcp;
  sendSignal(PGMAN_REF, GSN_LCP_FRAG_ORD, signal, 
	     LcpFragOrd::SignalLength, JBB);
  
  EndLcpReq* req= (EndLcpReq*)signal->getDataPtr();
  req->senderData = 0;
  req->senderRef = reference();
  req->backupId = m_latest_lcp;
  sendSignal(PGMAN_REF, GSN_END_LCPREQ, signal, 
	     EndLcpReq::SignalLength, JBB);
}

void
Lgman::execEND_LCPCONF(Signal* signal)
{
  {
    Dbtup_client tup(this, m_tup);
    tup.disk_restart_undo(signal, 0, File_formats::Undofile::UNDO_END, 0, 0);
    jamEntry();
  }
  
  /**
   * END_FLUSH_PGMAN_CACHE
   *
   * PGMAN has completed flushing all pages
   *
   * Insert "fake" LCP record preventing undo to be "rerun"
   */

  Uint32 undo[3];
  undo[0] = m_latest_lcp;
  undo[1] = (0 << 16) | 0;
  undo[2] = (File_formats::Undofile::UNDO_LCP_FIRST << 16 ) 
    | (sizeof(undo) >> 2);
  
  Ptr<Logfile_group> ptr;
  ndbrequire(m_logfile_group_list.first(ptr));

  Uint64 next_lsn= m_next_lsn;
  if(ptr.p->m_next_lsn == next_lsn
#ifdef VM_TRACE
     && ((rand() % 100) > 50)
#endif
     )
  {
    jam();
    undo[2] |= File_formats::Undofile::UNDO_NEXT_LSN << 16;
    Uint32 *dst= get_log_buffer(ptr,
                                sizeof(undo) >> 2,
                                jamBuffer());
    memcpy(dst, undo, sizeof(undo));
    ndbrequire(ptr.p->m_free_file_words >= (sizeof(undo) >> 2));
    ptr.p->m_free_file_words -= (sizeof(undo) >> 2);
  }
  else
  {
    jam();
    Uint32 *dst= get_log_buffer(ptr,
                                (sizeof(undo) >> 2) + 2,
                                jamBuffer());      
    * dst++ = (Uint32)(next_lsn >> 32);
    * dst++ = (Uint32)(next_lsn & 0xFFFFFFFF);
    memcpy(dst, undo, sizeof(undo));
    ndbrequire(ptr.p->m_free_file_words >= ((sizeof(undo) >> 2) + 2));
    ptr.p->m_free_file_words -= ((sizeof(undo) >> 2) + 2);
  }
  m_next_lsn = ptr.p->m_next_lsn = next_lsn + 1;

  ptr.p->m_last_synced_lsn = next_lsn;
  while(m_logfile_group_list.next(ptr))
  {
    jam();
    ptr.p->m_last_synced_lsn = next_lsn;
  }
  
  infoEvent("LGMAN: Flushing complete");
  g_eventLogger->info("LGMAN: Flushing complete");

  signal->theData[0] = reference();
  sendSignal(DBLQH_REF, GSN_START_RECCONF, signal, 1, JBB);
}

#ifdef VM_TRACE
void 
Lgman::validate_logfile_group(Ptr<Logfile_group> ptr,
                              const char * heading,
                              EmulatedJamBuffer *jamBuf)
{
  do 
  {
    if (ptr.p->m_file_pos[HEAD].m_ptr_i == RNIL)
    {
      thrjam(jamBuf);
      break;
    }
    
    Uint32 pages = compute_free_file_pages(ptr, jamBuf);
    
    Uint32 group_pages = 
      ((ptr.p->m_free_file_words + File_formats::UNDO_PAGE_WORDS - 1) /
        File_formats::UNDO_PAGE_WORDS) ;
    Uint32 last = ptr.p->m_free_file_words % File_formats::UNDO_PAGE_WORDS;
    
    if(! (pages >= group_pages))
    {
      ndbout << heading << " Tail: " << ptr.p->m_file_pos[TAIL] 
	     << " Head: " << ptr.p->m_file_pos[HEAD] 
	     << " free: " << group_pages << "(" << last << ")" 
	     << " found: " << pages;
      for(Uint32 i = 0; i<3; i++)
      {
	ndbout << " - " << ptr.p->m_tail_pos[i];
      }
      ndbout << endl;
      
      ndbrequire(pages >= group_pages);
    }
  } while(0);
}
#endif

void Lgman::execGET_TABINFOREQ(Signal* signal)
{
  jamEntry();

  if(!assembleFragments(signal))
  {
    return;
  }

  GetTabInfoReq * const req = (GetTabInfoReq *)&signal->theData[0];

  const Uint32 reqType = req->requestType & (~GetTabInfoReq::LongSignalConf);
  BlockReference retRef= req->senderRef;
  Uint32 senderData= req->senderData;
  Uint32 tableId= req->tableId;

  if(reqType == GetTabInfoReq::RequestByName)
  {
    jam();
    SectionHandle handle(this, signal);
    releaseSections(handle);

    sendGET_TABINFOREF(signal, req, GetTabInfoRef::NoFetchByName);
    return;
  }

  Logfile_group key;
  key.m_logfile_group_id= tableId;
  Ptr<Logfile_group> ptr;
  m_logfile_group_hash.find(ptr, key);

  if(ptr.p->m_logfile_group_id != tableId)
  {
    jam();

    sendGET_TABINFOREF(signal, req, GetTabInfoRef::InvalidTableId);
    return;
  }


  GetTabInfoConf *conf = (GetTabInfoConf *)&signal->theData[0];

  conf->senderData= senderData;
  conf->tableId= tableId;
  conf->freeWordsHi= (Uint32)(ptr.p->m_free_file_words >> 32);
  conf->freeWordsLo= (Uint32)(ptr.p->m_free_file_words & 0xFFFFFFFF);
  conf->tableType= DictTabInfo::LogfileGroup;
  conf->senderRef= reference();
  sendSignal(retRef, GSN_GET_TABINFO_CONF, signal,
	     GetTabInfoConf::SignalLength, JBB);
}

void Lgman::sendGET_TABINFOREF(Signal* signal,
			       GetTabInfoReq * req,
			       GetTabInfoRef::ErrorCode errorCode)
{
  jamEntry();
  GetTabInfoRef * const ref = (GetTabInfoRef *)&signal->theData[0];
  /**
   * The format of GetTabInfo Req/Ref is the same
   */
  BlockReference retRef = req->senderRef;
  ref->errorCode = errorCode;

  sendSignal(retRef, GSN_GET_TABINFOREF, signal, signal->length(), JBB);
}
