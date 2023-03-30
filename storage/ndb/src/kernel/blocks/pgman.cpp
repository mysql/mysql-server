/*
   Copyright (c) 2005, 2023, Oracle and/or its affiliates.

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

#include "util/require.h"
#include "pgman.hpp"
#include <signaldata/FsRef.hpp>
#include <signaldata/FsConf.hpp>
#include <signaldata/FsReadWriteReq.hpp>
#include <signaldata/PgmanContinueB.hpp>
#include <signaldata/LCP.hpp>
#include <signaldata/DataFileOrd.hpp>
#include <signaldata/ReleasePages.hpp>

#include <dbtup/Dbtup.hpp>
#include <dbtup/tuppage.hpp>

#include <DebuggerNames.hpp>
#include <md5_hash.hpp>

#include <PgmanProxy.hpp>

#include <EventLogger.hpp>

#define JAM_FILE_ID 335


/**
 * Requests that make page dirty
 */
#define DIRTY_FLAGS (Page_request::COMMIT_REQ | \
                     Page_request::ABORT_REQ | \
                     Page_request::DIRTY_REQ | \
                     Page_request::ALLOC_REQ)

static bool g_dbg_lcp = false;

#if (defined(VM_TRACE) || defined(ERROR_INSERT))
//#define DEBUG_PAGE_ENTRY 1
//#define DEBUG_PGMAN_IO 1
//#define DEBUG_PGMAN_WRITE 1
//#define DEBUG_GET_PAGE 1
//#define DEBUG_PGMAN_PAGE 1
//#define DEBUG_PGMAN_EXTRA 1
//#define DEBUG_PGMAN_LCP_TIME_STAT 1
//#define DEBUG_PGMAN 1
//#define DEBUG_PGMAN_LCP_EXTRA 1
//#define DEBUG_PGMAN_LCP 1
//#define DEBUG_PGMAN_LCP_STAT 1
//#define DEBUG_PGMAN_PREP_PAGE 1
#endif

#ifdef DEBUG_PAGE_ENTRY
#define DEB_PAGE_ENTRY(arglist) do { g_eventLogger->info arglist ; } while (0)
#else
#define DEB_PAGE_ENTRY(arglist) do { } while (0)
#endif

#ifdef DEBUG_PGMAN_WRITE
#define DEB_PGMAN_WRITE(arglist) do { g_eventLogger->info arglist ; } while (0)
#else
#define DEB_PGMAN_WRITE(arglist) do { } while (0)
#endif

#ifdef DEBUG_PGMAN
#define DEB_PGMAN(arglist) do { g_eventLogger->info arglist ; } while (0)
#else
#define DEB_PGMAN(arglist) do { } while (0)
#endif

#ifdef DEBUG_GET_PAGE
#define DEB_GET_PAGE(arglist) do { g_eventLogger->info arglist ; } while (0)
#else
#define DEB_GET_PAGE(arglist) do { } while (0)
#endif

#ifdef DEBUG_PGMAN_EXTRA
#define DEB_PGMAN_EXTRA(arglist) do { g_eventLogger->info arglist ; } while (0)
#else
#define DEB_PGMAN_EXTRA(arglist) do { } while (0)
#endif

#ifdef DEBUG_PGMAN_PAGE
#define DEB_PGMAN_PAGE(arglist) do { g_eventLogger->info arglist ; } while (0)
#else
#define DEB_PGMAN_PAGE(arglist) do { } while (0)
#endif

#ifdef DEBUG_PGMAN_PREP_PAGE
#define DEB_PGMAN_PREP_PAGE(arglist) do { g_eventLogger->info arglist ; } while (0)
#else
#define DEB_PGMAN_PREP_PAGE(arglist) do { } while (0)
#endif

#ifdef DEBUG_PGMAN_IO
#define DEB_PGMAN_IO(arglist) do { g_eventLogger->info arglist ; } while (0)
#else
#define DEB_PGMAN_IO(arglist) do { } while (0)
#endif

#ifdef DEBUG_PGMAN_LCP
#define DEB_PGMAN_LCP(arglist) do { g_eventLogger->info arglist ; } while (0)
#else
#define DEB_PGMAN_LCP(arglist) do { } while (0)
#endif

#ifdef DEBUG_PGMAN_LCP_EXTRA
#define DEB_PGMAN_LCP_EXTRA(arglist) do { g_eventLogger->info arglist ; } while (0)
#else
#define DEB_PGMAN_LCP_EXTRA(arglist) do { } while (0)
#endif

#ifdef DEBUG_PGMAN_LCP_STAT
#define DEB_PGMAN_LCP_STAT(arglist) do { g_eventLogger->info arglist ; } while (0)
#else
#define DEB_PGMAN_LCP_STAT(arglist) do { } while (0)
#endif

#ifdef DEBUG_PGMAN_LCP_TIME_STAT
#define DEB_PGMAN_LCP_TIME_STAT(arglist) do { g_eventLogger->info arglist ; } while (0)
#else
#define DEB_PGMAN_LCP_TIME_STAT(arglist) do { } while (0)
#endif

Pgman::Pgman(Block_context& ctx, Uint32 instanceNumber) :
  SimulatedBlock(PGMAN, ctx, instanceNumber),
  m_fragmentRecordHash(m_fragmentRecordPool),
  m_dirty_list_lcp(m_page_entry_pool),
  m_dirty_list_lcp_out(m_page_entry_pool),
  m_file_map(m_data_buffer_pool),
  m_page_hashlist(m_page_entry_pool),
  m_page_stack(m_page_entry_pool),
  m_page_queue(m_page_entry_pool)
#ifdef VM_TRACE
  ,debugFlag(false)
  ,debugSummaryFlag(false)
#endif
{
  BLOCK_CONSTRUCTOR(Pgman);

  for (Uint32 i = 0; i < NUM_ORDERED_LISTS; i++)
  {
    m_fragmentRecordList[i].init();
  }

  m_access_extent_page_mutex = NdbMutex_Create();
  ndbrequire(m_access_extent_page_mutex != 0);

  // Add received signals
  addRecSignal(GSN_STTOR, &Pgman::execSTTOR);
  addRecSignal(GSN_READ_CONFIG_REQ, &Pgman::execREAD_CONFIG_REQ);
  addRecSignal(GSN_DUMP_STATE_ORD, &Pgman::execDUMP_STATE_ORD);
  addRecSignal(GSN_CONTINUEB, &Pgman::execCONTINUEB);
  addRecSignal(GSN_FSREADREF, &Pgman::execFSREADREF, true);
  addRecSignal(GSN_FSREADCONF, &Pgman::execFSREADCONF);
  addRecSignal(GSN_FSWRITEREF, &Pgman::execFSWRITEREF, true);
  addRecSignal(GSN_FSWRITECONF, &Pgman::execFSWRITECONF);

  addRecSignal(GSN_END_LCPREQ, &Pgman::execEND_LCPREQ);
  addRecSignal(GSN_SYNC_PAGE_CACHE_REQ, &Pgman::execSYNC_PAGE_CACHE_REQ);
  addRecSignal(GSN_SYNC_PAGE_CACHE_CONF, &Pgman::execSYNC_PAGE_CACHE_CONF);
  addRecSignal(GSN_SYNC_EXTENT_PAGES_REQ, &Pgman::execSYNC_EXTENT_PAGES_REQ);
  addRecSignal(GSN_SYNC_EXTENT_PAGES_CONF, &Pgman::execSYNC_EXTENT_PAGES_CONF);

  addRecSignal(GSN_DATA_FILE_ORD, &Pgman::execDATA_FILE_ORD);
  addRecSignal(GSN_RELEASE_PAGES_REQ, &Pgman::execRELEASE_PAGES_REQ);
  addRecSignal(GSN_DBINFO_SCANREQ, &Pgman::execDBINFO_SCANREQ);
  
  // loop status
  m_stats_loop_on = false;
  m_busy_loop_on = false;
  m_cleanup_loop_on = false;

  // LCP variables
  m_sync_extent_pages_ongoing = false;
  m_lcp_loop_ongoing = false;
  m_lcp_outstanding = 0;
  m_prep_lcp_outstanding = 0;
  m_locked_pages_written = 0;
  m_lcp_table_id = RNIL;
  m_lcp_fragment_id = 0;
  m_prev_lcp_table_id = RNIL;
  m_prev_lcp_fragment_id = 0;

  // clean-up variables
  m_cleanup_ptr.i = RNIL;

  // Indicator of extra PGMAN worker block
  m_extra_pgman = false;
  m_extra_pgman_reserve_pages = 0;

  // should be a factor larger than number of pool pages
  m_data_buffer_pool.setSize(16);
  
  for (Uint32 k = 0; k < Page_entry::SUBLIST_COUNT; k++)
    m_page_sublist[k] = new Page_sublist(m_page_entry_pool);

  {
    CallbackEntry& ce = m_callbackEntry[THE_NULL_CALLBACK];
    ce.m_function = TheNULLCallback.m_callbackFunction;
    ce.m_flags = 0;
  }
  {
    CallbackEntry& ce = m_callbackEntry[LOGSYNC_CALLBACK];
    ce.m_function = safe_cast(&Pgman::logsync_callback);
    ce.m_flags = 0;
  }
  {
    CallbackTable& ct = m_callbackTable;
    ct.m_count = COUNT_CALLBACKS;
    ct.m_entry = m_callbackEntry;
    m_callbackTableAddr = &ct;
  }
  m_time_track_histogram_upper_bound[0] = 0;
  m_time_track_histogram_upper_bound[1] = 16;
  for (Uint32 i = 2; i < PGMAN_TIME_TRACK_NUM_RANGES; i++)
  {
    m_time_track_histogram_upper_bound[i] =
      2 * m_time_track_histogram_upper_bound[i - 1];
  }
  m_time_track_histogram_upper_bound[PGMAN_TIME_TRACK_NUM_RANGES - 1] =
    Uint64(~0);

  for (Uint32 i = 0; i < PGMAN_TIME_TRACK_NUM_RANGES; i++)
  {
    m_time_track_reads[i] = 0;
    m_time_track_writes[i] = 0;
    m_time_track_log_waits[i] = 0;
    m_time_track_get_page[i] = 0;
  }
  m_pages_made_dirty = Uint64(0);
  m_tot_pages_made_dirty = Uint64(0);
  m_reads_completed = Uint64(0);
  m_reads_issued = Uint64(0);
  m_writes_issued = Uint64(0);
  m_writes_completed = Uint64(0);
  m_tot_writes_completed = Uint64(0);
  m_get_page_calls_issued = Uint64(0);
  m_get_page_reqs_issued = Uint64(0);
  m_get_page_reqs_completed = Uint64(0);
  m_last_stat_index = NUM_STAT_HISTORY - 1;
  memset(m_pages_made_dirty_history, 0, sizeof(m_pages_made_dirty_history));
  memset(m_reads_completed_history, 0, sizeof(m_reads_completed_history));
  memset(m_reads_issued_history, 0, sizeof(m_reads_issued_history));
  memset(m_writes_completed_history, 0, sizeof(m_writes_completed_history));
  memset(m_writes_issued_history, 0, sizeof(m_writes_issued_history));
  memset(m_get_page_calls_issued_history,
         0,
         sizeof(m_get_page_calls_issued_history));
  memset(m_get_page_reqs_issued_history,
         0,
         sizeof(m_get_page_reqs_issued_history));
  memset(m_get_page_reqs_completed_history,
         0,
         sizeof(m_get_page_reqs_completed_history));
  memset(m_stat_time_delay, 0, sizeof(m_stat_time_delay));
  m_num_dd_accesses = Uint64(0);
  m_total_dd_latency_us = Uint64(0);
  m_outstanding_dd_requests = Uint64(0);
  m_abort_counter = 0;
  m_abort_level = 0;
  m_lcp_dd_percentage = Uint64(0);
  m_num_dirty_pages = Uint64(0);
  m_track_lcp_speed_loop_ongoing = false;
  m_dirty_page_rate_per_sec = Uint64(0);
  m_current_lcp_pageouts = Uint64(0);
  m_start_lcp_made_dirty = Uint64(0);
  m_last_lcp_made_dirty = Uint64(0);
  m_last_pageouts = Uint64(0);
  m_last_made_dirty = Uint64(0);
  m_current_lcp_flushes = Uint64(0);
  m_last_flushes = Uint64(0);
  m_max_lcp_pages_outstanding = Uint64(4);
  m_prep_max_lcp_pages_outstanding = Uint64(4);
  m_redo_alert_state = RedoStateRep::NO_REDO_ALERT;
  m_redo_alert_state_last_lcp = RedoStateRep::NO_REDO_ALERT;
  m_raise_redo_alert_state = 0;
  m_available_lcp_pageouts = Uint64(100);
  m_prep_available_lcp_pageouts = Uint64(100);
  m_available_lcp_pageouts_used = Uint64(0);
  m_redo_alert_factor = Uint64(100);
  m_total_write_latency_us = Uint64(0);
  m_last_lcp_writes_completed = Uint64(0);
  m_last_lcp_total_write_latency_us = Uint64(0);
  /* 1 ms is the default estimate for latency to disk drives */
  m_last_lcp_write_latency_us = Uint64(1000);
  m_mm_curr_disk_write_speed = Uint64(0);
  m_percent_spent_in_checkpointing = Uint64(100);
  m_lcp_time_in_ms = Uint64(0);
  m_lcp_ongoing = false;
  m_num_ldm_completed_lcp = 0;
  m_max_pageout_rate = Uint64(0);
  m_sync_extent_next_page_entry = RNIL;
  m_sync_extent_pages_ongoing = false;
  m_sync_extent_continueb_ongoing = false;
  memset(&m_sync_page_cache_req, 0, sizeof(m_sync_page_cache_req));
  memset(&m_sync_extent_pages_req, 0, sizeof(m_sync_extent_pages_req));
}

Pgman::~Pgman()
{
  NdbMutex_Destroy(m_access_extent_page_mutex);
  m_access_extent_page_mutex = 0;
  for (Uint32 k = 0; k < Page_entry::SUBLIST_COUNT; k++)
    delete m_page_sublist[k];
}

BLOCK_FUNCTIONS(Pgman)

void 
Pgman::execREAD_CONFIG_REQ(Signal* signal)
{
  jamEntry();

  const ReadConfigReq * req = (ReadConfigReq*)signal->getDataPtr();

  Uint32 ref = req->senderRef;
  Uint32 senderData = req->senderData;

  const ndb_mgm_configuration_iterator * p = 
    m_ctx.m_config.getOwnConfigIterator();
  ndbrequire(p != 0);

  Uint32 max_dd_latency = 0;
  ndb_mgm_get_int_parameter(p, CFG_DB_MAX_DD_LATENCY, &max_dd_latency);
  m_max_dd_latency_ms = max_dd_latency;

  Uint32 dd_using_same_disk = 1;
  ndb_mgm_get_int_parameter(p,
                            CFG_DB_DD_USING_SAME_DISK,
                            &dd_using_same_disk);
  m_dd_using_same_disk = dd_using_same_disk;

  Uint64 page_buffer = 64*1024*1024;
  ndb_mgm_get_int64_parameter(p, CFG_DB_DISK_PAGE_BUFFER_MEMORY, &page_buffer);
  
  if (page_buffer > 0)
  {
    jam();
    if (isNdbMtLqh())
    {
      jam();
      // divide between workers - wl4391_todo give extra worker less
      /**
       * The disk page buffer memory is equally divided between the instances.
       * This is ok for all except the extra worker. The extra worker needs
       * pages for permanent allocation of extent pages and it also needs some
       * extra pages during UNDO log execution to read the page header and get
       * table and fragment id of the page to UNDO information on. The table
       * and fragment id is needed to map to the correct LDM instance for this
       * UNDO log record. One potential solution to this mapping problem is
       * to get this information from the extent header instead. This would
       * avoid any need to read page in the extra PGMAN worker thread.
       */
      Uint32 workers = getLqhWorkers() + 1;
      page_buffer = page_buffer / workers;
      Uint32 min_buffer = 4*1024*1024;
      if (page_buffer < min_buffer)
        page_buffer = min_buffer;
    }
    // convert to pages
    Uint32 page_cnt = Uint32((page_buffer + GLOBAL_PAGE_SIZE - 1) / GLOBAL_PAGE_SIZE);

    if (ERROR_INSERTED(11009))
    {
      page_cnt = 25;
      g_eventLogger->info("Setting page_cnt = %u", page_cnt);
    }

    m_param.m_max_pages = page_cnt;

    // how many page entries per buffer pages
    Uint32 entries = 0;
    ndb_mgm_get_int_parameter(p, CFG_DB_DISK_PAGE_BUFFER_ENTRIES, &entries);
    g_eventLogger->info("pgman: page buffer entries = %u", entries);
    if (entries > 0) // should be
    {
      // param name refers to unbound entries ending up on stack
      m_param.m_lirs_stack_mult = entries;
    }
    Uint32 pool_size = m_param.m_lirs_stack_mult * page_cnt;
    m_page_entry_pool.setSize(pool_size);
    m_page_hashlist.setSize(pool_size);

    m_param.m_max_hot_pages = (page_cnt * 9) / 10;
    ndbrequire(m_param.m_max_hot_pages >= 1);
  }

  Pool_context pc;
  pc.m_block = this;
  m_page_request_pool.wo_pool_init(RT_PGMAN_PAGE_REQUEST, pc);
  m_file_entry_pool.init(RT_PGMAN_FILE, pc);
  
  Uint32 noFragments = 0;
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_LQH_FRAG, &noFragments));
  m_fragmentRecordPool.setSize(noFragments);
  m_fragmentRecordHash.setSize(noFragments);

  Uint32 noTables = 0;
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_LQH_TABLE, &noTables));
  m_tableRecordPool.setSize(noTables);

  for (Uint32 i = 0; i < noTables; i++)
  {
    TableRecordPtr tabPtr;
    ndbrequire(m_tableRecordPool.seizeId(tabPtr, i));
    tabPtr.p->m_is_table_ready_for_prep_lcp_writes = false;
    tabPtr.p->m_num_prepare_lcp_outstanding = 0;
  }

  ReadConfigConf * conf = (ReadConfigConf*)signal->getDataPtrSend();
  conf->senderRef = reference();
  conf->senderData = senderData;
  sendSignal(ref, GSN_READ_CONFIG_CONF, signal, 
	     ReadConfigConf::SignalLength, JBB);
}

Pgman::Param::Param() :
  m_max_pages(64),      // smallish for testing
  m_lirs_stack_mult(10),
  m_max_hot_pages(56),
  m_max_loop_count(256),
  m_max_io_waits(256),
  m_stats_loop_delay(1000),
  m_cleanup_loop_delay(200)
{
}

void
Pgman::execSTTOR(Signal* signal)
{
  jamEntry();

  const Uint32 startPhase  = signal->theData[1];

  switch (startPhase) {
  case 1:
    {
      jam();
      if (!isNdbMtLqh())
      {
        c_tup = (Dbtup*)globalData.getBlock(DBTUP);
        c_backup = (Backup*)globalData.getBlock(BACKUP);
      }
      else if (instance() <= getLqhWorkers())
      {
        c_tup = (Dbtup*)globalData.getBlock(DBTUP, instance());
        c_backup = (Backup*)globalData.getBlock(BACKUP, instance());
        ndbrequire(c_tup != 0);
        ndbrequire(c_backup != 0);
      }
      else
      {
        // extra worker
        c_tup = 0;
        c_backup = 0;
      }
      c_lgman = (Lgman*)globalData.getBlock(LGMAN);
      c_tsman = (Tsman*)globalData.getBlock(TSMAN);
    }
    break;
  case 3:
    {
      jam();
      // start forever loops
      do_stats_loop(signal);
      do_cleanup_loop(signal);
      m_stats_loop_on = true;
      m_cleanup_loop_on = true;
      NDB_TICKS now = NdbTick_getCurrentTicks();
      m_last_time_calc_stats_loop = now.getUint64();
      signal->theData[0] = PgmanContinueB::CALC_STATS_LOOP;
      sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 1000, 1);
    }
    break;
  default:
    jam();
    break;
  }

  sendSTTORRY(signal);
}

void
Pgman::init_extra_pgman()
{
  m_extra_pgman = true;

  // Reserve 1MB of extra pgman's disk page buffer memory for
  // undo log execution (in number of pages)
  m_extra_pgman_reserve_pages =
    Uint32((1*1024*1024+ GLOBAL_PAGE_SIZE - 1)/GLOBAL_PAGE_SIZE);
}

void
Pgman::sendSTTORRY(Signal* signal)
{
  signal->theData[0] = 0;
  signal->theData[3] = 1;
  signal->theData[4] = 3;
  signal->theData[5] = 255; // No more start phases from missra
  BlockReference cntrRef = !isNdbMtLqh() ? NDBCNTR_REF : PGMAN_REF;
  sendSignal(cntrRef, GSN_STTORRY, signal, 6, JBB);
}

void
Pgman::execCONTINUEB(Signal* signal)
{
  jamEntry();
  switch (signal->theData[0]) {
  case PgmanContinueB::STATS_LOOP:
    jam();
    do_stats_loop(signal);
    break;
  case PgmanContinueB::BUSY_LOOP:
    jam();
    do_busy_loop(signal, false, jamBuffer());
    break;
  case PgmanContinueB::CLEANUP_LOOP:
    jam();
    do_cleanup_loop(signal);
    break;
  case PgmanContinueB::LCP_LOOP:
  {
    jam();
    ndbrequire(m_lcp_loop_ongoing);
    m_lcp_loop_ongoing = false;
    check_restart_lcp(signal, true);
    return;
  }
  case PgmanContinueB::CALC_STATS_LOOP:
  {
    do_calc_stats_loop(signal);
    break;
  }
  case PgmanContinueB::TRACK_LCP_SPEED_LOOP:
  {
    do_track_handle_lcp_speed_loop(signal);
    break;
  }
  default:
    ndbabort();
  }
}

// page entry

Pgman::Page_entry::Page_entry(Uint32 file_no,
                              Uint32 page_no,
                              Uint32 tableId,
                              Uint32 fragmentId) :
  m_file_no(file_no),
  m_dirty_state(Pgman::IN_NO_DIRTY_LIST),
  m_dirty_during_pageout(false),
  m_state(0),
  m_page_no(page_no),
  m_real_page_i(RNIL),
  m_lsn(0),
  m_table_id(tableId),
  m_fragment_id(fragmentId),
  m_dirty_count(0),
  m_copy_page_i(RNIL),
  m_busy_count(0),
  m_requests()
{
}

// page lists

Uint32
Pgman::get_sublist_no(Page_state state)
{
  if (state & Page_entry::REQUEST)
  {
    if (! (state & Page_entry::BOUND))
    {
      return Page_entry::SL_BIND;
    }
    if (! (state & Page_entry::MAPPED))
    {
      if (! (state & Page_entry::PAGEIN))
      {
        return Page_entry::SL_MAP;
      }
      return Page_entry::SL_MAP_IO;
    }
    if (! (state & Page_entry::PAGEOUT))
    {
      return Page_entry::SL_CALLBACK;
    }
    return Page_entry::SL_CALLBACK_IO;
  }
  if (state & Page_entry::BUSY)
  {
    return Page_entry::SL_BUSY;
  }
  if (state & Page_entry::LOCKED)
  {
    return Page_entry::SL_LOCKED;
  }
  if (state == Page_entry::ONSTACK) {
    return Page_entry::SL_IDLE;
  }
  if (state != 0)
  {
    return Page_entry::SL_OTHER;
  }
  return ZNIL;
}

void
Pgman::set_page_state(EmulatedJamBuffer* jamBuf, Ptr<Page_entry> ptr, 
                      Page_state new_state)
{
  D("> [" << ptr.i << "]->set_page_state: state=" << hex << new_state);
  D(ptr << ": before");

  Page_state old_state = ptr.p->m_state;
  if (old_state != new_state)
  {
    Uint32 old_list_no = get_sublist_no(old_state);
    thrjam(jamBuf);
    Uint32 new_list_no = get_sublist_no(new_state);
    if (old_state != 0)
    {
      thrjam(jamBuf);
      thrjamLineDebug(jamBuf, Uint16(old_list_no));
      ndbrequire(old_list_no != ZNIL);
      if (old_list_no != new_list_no)
      {
        thrjam(jamBuf);
        thrjamLineDebug(jamBuf, Uint16(new_list_no));
        Page_sublist& old_list = *m_page_sublist[old_list_no];
        old_list.remove(ptr);
      }
    }
    if (new_state != 0)
    {
      thrjam(jamBuf);
      thrjamLineDebug(jamBuf, Uint16(new_list_no));
      ndbrequire(new_list_no != ZNIL);
      if (old_list_no != new_list_no)
      {
        thrjam(jamBuf);
        thrjamLineDebug(jamBuf, Uint16(old_list_no));
        Page_sublist& new_list = *m_page_sublist[new_list_no];
        new_list.addLast(ptr);
      }
    }
    ptr.p->m_state = new_state;

    bool old_hot = (old_state & Page_entry::HOT);
    bool new_hot = (new_state & Page_entry::HOT);
    if (! old_hot && new_hot)
    {
      thrjam(jamBuf);
      m_stats.m_num_hot_pages++;
    }
    if (old_hot && ! new_hot)
    {
      thrjam(jamBuf);
      ndbrequire(m_stats.m_num_hot_pages != 0);
      m_stats.m_num_hot_pages--;
    }

    {
      const bool old_locked = (old_state & Page_entry::LOCKED);
      const bool new_locked = (new_state & Page_entry::LOCKED);
      if (!old_locked && new_locked)
      {
        thrjam(jamBuf);
        m_stats.m_num_locked_pages++;
      }
      if (old_locked && !new_locked)
      {
        thrjam(jamBuf);
        m_stats.m_num_locked_pages--;
      }
    }
  }

  D(ptr << ": after");
#ifdef VM_TRACE
  verify_page_entry(ptr);
#endif
  D("<set_page_state");
}

// seize/release pages and entries

bool
Pgman::seize_cache_page(Ptr<GlobalPage>& gptr)
{
  // page cache has no own pool yet
  bool ok = m_global_page_pool.seize(gptr);

  // zero is reserved as return value for queued request
  if (ok && gptr.i == 0)
    ok = m_global_page_pool.seize(gptr);

  if (ok)
  {
    ndbrequire(m_stats.m_num_pages < m_param.m_max_pages);
    m_stats.m_num_pages++;
  }
  return ok;
}

void
Pgman::release_cache_page(Uint32 i)
{
  m_global_page_pool.release(i);

  ndbrequire(m_stats.m_num_pages != 0);
  m_stats.m_num_pages--;
}

bool
Pgman::find_page_entry(Ptr<Page_entry>& ptr, Uint32 file_no, Uint32 page_no)
{
  Page_entry key;
  key.m_file_no = file_no;
  key.m_page_no = page_no;
  
  if (m_page_hashlist.find(ptr, key))
  {
    ndbassert(ptr.p->m_page_no == page_no);
    ndbassert(ptr.p->m_file_no == file_no);
    D("find_page_entry");
    D(ptr);
    return true;
  }
  return false;
}

Uint32
Pgman::seize_page_entry(Ptr<Page_entry>& ptr,
                        Uint32 file_no,
                        Uint32 page_no,
                        Uint32 tableId,
                        Uint32 fragmentId,
                        EmulatedJamBuffer *jamBuf)
{
  if (m_page_entry_pool.seize(ptr))
  {
    thrjam(jamBuf);
    new (ptr.p) Page_entry(file_no,
                           page_no,
                           tableId,
                           fragmentId);
    m_page_hashlist.add(ptr);
    DEB_PAGE_ENTRY(("(%u) seize_page_entry: tab(%u,%u), page(%u,%u),"
                    " ptr.i: %u",
                    instance(),
                    tableId,
                    fragmentId,
                    file_no,
                    page_no,
                    ptr.i));
#ifdef VM_TRACE
    ptr.p->m_this = this;
#endif
    D("seize_page_entry");
    D(ptr);

    if (m_stats.m_entries_high < m_page_entry_pool.getUsed())
    {
      thrjam(jamBuf);
      m_stats.m_entries_high = m_page_entry_pool.getUsed();
    }

    return true;
  }
  thrjam(jamBuf);
  return false;
}

bool
Pgman::get_page_entry(EmulatedJamBuffer* jamBuf,
                      Ptr<Page_entry>& ptr, 
                      Uint32 file_no,
                      Uint32 page_no,
                      Uint32 tableId,
                      Uint32 fragmentId,
                      Uint32 flags)
{
  if (m_extra_pgman && tableId != RNIL)
  {
    ndbabort();
  }
  else if (!m_extra_pgman && isNdbMtLqh() && tableId == RNIL)
  {
    ndbabort();
  }

  if (find_page_entry(ptr, file_no, page_no))
  {
    thrjam(jamBuf);
    ndbrequire(ptr.p->m_state != 0);
    m_stats.m_page_hits++;

    D("get_page_entry: found");
    D(ptr);
    if (!(flags & Page_request::UNDO_REQ))
    {
      thrjam(jamBuf);
      /**
       * We skip this part for retrieving page as part of UNDO log
       * applier. We will handle this in the callback function for
       * UNDO entries.
       */
      if (ptr.p->m_table_id != tableId ||
          ptr.p->m_fragment_id != fragmentId)
      {
        thrjam(jamBuf);
        /**
         * The Page Manager drops dirty pages during drop fragment. It does
         * however not release page entries that are either unmapped or mapped
         * but not dirty.
         * This means that when allocating a previously dropped page we can
         * come here and find that the page entry is belonging to another table
         * id and fragment id.
         *
         * This should only happen when allocating a page which was previously
         * an empty page, this means that we have recently allocated this page
         * from an extent. Thus a dropped fragment could potentially have
         * released this page and its extent as part of a drop fragment.
         *
         * We check that the request is to allocate a new page and that it is
         * an EMPTY page. We also verify that the page isn't in any dirty list
         * at this time.
         *
         * TUP doesn't keep information about all pages it has in the page
         * cache for a specific fragment. If it had this information we
         * could avoid this problem by ensuring that drop_page is called for
         * all pages in the page cache.
         *
         * We can also encounter when we perform disk scan, in this case we
         * read pages in disk order without knowing if it is actually been
         * written to yet.
         */
        DEB_PGMAN(("(%u)func: %s, flags: %x", instance(), __func__, flags));
        if (!(
              ((flags & Page_request::ALLOC_REQ) &&
               (flags & Page_request::EMPTY_PAGE)) ||
              (flags & Page_request::DISK_SCAN)))
        {
          g_eventLogger->info("(%u)tab(%u,%u) page(%u,%u) on page:tab(%u,%u)",
                              instance(),
                              tableId,
                              fragmentId,
                              file_no,
                              page_no,
                              ptr.p->m_table_id,
                              ptr.p->m_fragment_id);

        }
        ndbrequire((flags & Page_request::ALLOC_REQ &&
                    flags & Page_request::EMPTY_PAGE) ||
                    flags & Page_request::DISK_SCAN);
        ndbrequire(ptr.p->m_dirty_state == Pgman::IN_NO_DIRTY_LIST);
        ptr.p->m_table_id = tableId;
        ptr.p->m_fragment_id = fragmentId;
      }
      ndbrequire(ptr.p->m_table_id == tableId);
      ndbrequire(ptr.p->m_fragment_id == fragmentId);
    }
    return true;
  }

  if (m_page_entry_pool.getNoOfFree() == 0)
  {
    thrjam(jamBuf);
    Page_sublist& pl_idle = *m_page_sublist[Page_entry::SL_IDLE];
    Ptr<Page_entry> idle_ptr;
    if (pl_idle.first(idle_ptr))
    {
      thrjam(jamBuf);

      D("get_page_entry: re-use idle entry");
      D(idle_ptr);

      Page_state state = idle_ptr.p->m_state;
      ndbrequire(state == Page_entry::ONSTACK);

      Page_stack& pl_stack = m_page_stack;
      ndbrequire(pl_stack.hasPrev(idle_ptr));
      pl_stack.remove(idle_ptr);
      state &= ~ Page_entry::ONSTACK;
      set_page_state(jamBuf, idle_ptr, state);
      ndbrequire(idle_ptr.p->m_state == 0);

      release_page_entry(idle_ptr, jamBuf);
    }
  }

  if (seize_page_entry(ptr,
                       file_no,
                       page_no,
                       tableId,
                       fragmentId,
                       jamBuf))
  {
    thrjam(jamBuf);
    ndbrequire(ptr.p->m_state == 0);
    m_stats.m_page_faults++;

    D("get_page_entry: seize");
    D(ptr);
    return true;
  }

  ndbabort();
  
  return false;
}

void
Pgman::release_page_entry(Ptr<Page_entry>& ptr, EmulatedJamBuffer *jamBuf)
{
  D("release_page_entry");
  D(ptr);
  Page_state state = ptr.p->m_state;

  ndbrequire(ptr.p->m_requests.isEmpty());

  ndbrequire(! (state & Page_entry::ONSTACK));
  ndbrequire(! (state & Page_entry::ONQUEUE));
  ndbrequire(ptr.p->m_real_page_i == RNIL);
  ndbrequire(ptr.p->m_dirty_state == Pgman::IN_NO_DIRTY_LIST);

  DEB_PAGE_ENTRY(("(%u) release_page_entry: tab(%u,%u), page(%u,%u),"
                  " ptr.i: %u",
                  instance(),
                  ptr.p->m_table_id,
                  ptr.p->m_fragment_id,
                  ptr.p->m_file_no,
                  ptr.p->m_page_no,
                  ptr.i));
  if (! (state & Page_entry::LOCKED))
  {
    thrjam(jamBuf);
    ndbrequire(! (state & Page_entry::REQUEST));
  }

  if (ptr.p->m_copy_page_i != RNIL)
  {
    thrjam(jamBuf);
    m_global_page_pool.release(ptr.p->m_copy_page_i);
  }
  
  set_page_state(jamBuf, ptr, 0);
  m_page_hashlist.remove(ptr);
  m_page_entry_pool.release(ptr);
}

// LIRS

/*
 * After the hot entry at stack bottom is removed, additional entries
 * are removed until next hot entry is found.  There are 3 cases for the
 * removed entry:  1) a bound entry is already on queue 2) an unbound
 * entry with open requests enters queue at bind time 3) an unbound
 * entry without requests is returned to entry pool.
 */
void
Pgman::lirs_stack_prune(EmulatedJamBuffer *jamBuf)
{
  D(">lirs_stack_prune");
  Page_stack& pl_stack = m_page_stack;
  Ptr<Page_entry> ptr;

  while (pl_stack.first(ptr))      // first is stack bottom
  {
    Page_state state = ptr.p->m_state;
    if (state & Page_entry::HOT)
    {
      thrjam(jamBuf);
      break;
    }

    D(ptr << ": prune from stack");

    pl_stack.remove(ptr);
    state &= ~ Page_entry::ONSTACK;
    set_page_state(jamBuf, ptr, state);

    if (state & Page_entry::BOUND)
    {
      thrjam(jamBuf);
      ndbrequire(state & Page_entry::ONQUEUE);
    }
    else if (state & Page_entry::REQUEST)
    {
      // enters queue at bind
      thrjam(jamBuf);
      ndbrequire(! (state & Page_entry::ONQUEUE));
    }
    else
    {
      thrjam(jamBuf);
      release_page_entry(ptr, jamBuf);
    }
  }
  D("<lirs_stack_prune");
}

/*
 * Remove the hot entry at stack bottom and make it cold and do stack
 * pruning.  There are 2 cases for the removed entry:  1) a bound entry
 * is moved to queue 2) an unbound entry must have requests and enters
 * queue at bind time.
 */
void
Pgman::lirs_stack_pop(EmulatedJamBuffer *jamBuf)
{
  D("lirs_stack_pop");
  Page_stack& pl_stack = m_page_stack;
  Page_queue& pl_queue = m_page_queue;

  Ptr<Page_entry> ptr;
  bool ok = pl_stack.first(ptr);
  ndbrequire(ok);
  Page_state state = ptr.p->m_state;

  D(ptr << ": pop from stack");

  ndbrequire(state & Page_entry::HOT);
  ndbrequire(state & Page_entry::ONSTACK);
  pl_stack.remove(ptr);
  state &= ~ Page_entry::HOT;
  state &= ~ Page_entry::ONSTACK;
  ndbrequire(! (state & Page_entry::ONQUEUE));

  if (state & Page_entry::BOUND)
  {
    thrjam(jamBuf);
    pl_queue.addLast(ptr);
    state |= Page_entry::ONQUEUE;
  }
  else
  {
    // enters queue at bind
    thrjam(jamBuf);
    ndbrequire(state & Page_entry::REQUEST);
  }

  set_page_state(jamBuf, ptr, state);
  lirs_stack_prune(jamBuf);
}

/*
 * Update LIRS lists when page is referenced.
 */
void
Pgman::lirs_reference(EmulatedJamBuffer *jamBuf,
                      Ptr<Page_entry> ptr)
{
  D(">lirs_reference");
  D(ptr);
  Page_stack& pl_stack = m_page_stack;
  Page_queue& pl_queue = m_page_queue;

  Page_state state = ptr.p->m_state;
  ndbrequire(! (state & Page_entry::LOCKED));

  ndbrequire(m_stats.m_num_hot_pages <= m_param.m_max_hot_pages);

  // LIRS kicks in when we have max hot pages
  if (m_stats.m_num_hot_pages == m_param.m_max_hot_pages)
  {
    if (state & Page_entry::HOT)
    {
      // case 1
      thrjam(jamBuf);
      ndbrequire(state & Page_entry::ONSTACK);
      bool at_bottom = ! pl_stack.hasPrev(ptr);
      pl_stack.remove(ptr);
      pl_stack.addLast(ptr);
      if (at_bottom)
      {
        thrjam(jamBuf);
        lirs_stack_prune(jamBuf);
      }
    }
    else if (state & Page_entry::ONSTACK)
    {
      // case 2a 3a
      thrjam(jamBuf);
      pl_stack.remove(ptr);
      if (! pl_stack.isEmpty())
      {
        thrjam(jamBuf);
        lirs_stack_pop(jamBuf);
      }
      pl_stack.addLast(ptr);
      state |= Page_entry::HOT;
      if (state & Page_entry::ONQUEUE)
      {
        thrjam(jamBuf);
        move_cleanup_ptr(ptr, jamBuf);
        pl_queue.remove(ptr);
        state &= ~ Page_entry::ONQUEUE;
      }
    }
    else
    {
      // case 2b 3b
      thrjam(jamBuf);
      pl_stack.addLast(ptr);
      state |= Page_entry::ONSTACK;
      /*
       * bug#48910.  Using hot page count (not total page count)
       * guarantees that stack is not empty here.  Therefore the new
       * entry (added to top) is not at bottom and need not be hot.
       */
      ndbrequire(pl_stack.hasPrev(ptr));
      if (state & Page_entry::ONQUEUE)
      { 
        thrjam(jamBuf);
        move_cleanup_ptr(ptr, jamBuf);
        pl_queue.remove(ptr);
        state &= ~ Page_entry::ONQUEUE;
      }
      if (state & Page_entry::BOUND)
      {
        thrjam(jamBuf);
        pl_queue.addLast(ptr);
        state |= Page_entry::ONQUEUE;
      }
      else
      {
        // enters queue at bind
        thrjam(jamBuf);
      }
    }
  }
  else
  {
    D("filling up hot pages: " << m_stats.m_num_hot_pages << "/"
                               << m_param.m_max_hot_pages);
    thrjam(jamBuf);
    if (state & Page_entry::ONSTACK)
    {
      thrjam(jamBuf);
      bool at_bottom = ! pl_stack.hasPrev(ptr);
      pl_stack.remove(ptr);
      if (at_bottom)
      {
        thrjam(jamBuf);
        ndbassert(state & Page_entry::HOT);
        lirs_stack_prune(jamBuf);
      }
    }
    pl_stack.addLast(ptr);
    state |= Page_entry::ONSTACK;
    state |= Page_entry::HOT;
    // it could be on queue already
    if (state & Page_entry::ONQUEUE) {
      thrjam(jamBuf);
      pl_queue.remove(ptr);
      state &= ~Page_entry::ONQUEUE;
    }
  }

  set_page_state(jamBuf, ptr, state);
  D("<lirs_reference");
}

// continueB loops

void
Pgman::do_stats_loop(Signal* signal)
{
  //D("do_stats_loop");
#ifdef VM_TRACE
  verify_all();
#endif
  Uint32 delay = m_param.m_stats_loop_delay;
  signal->theData[0] = PgmanContinueB::STATS_LOOP;
  sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, delay, 1);
}

/**
 * do_busy_loop is called to process bind requests, map requests,
 * and callback requests that have been queued. As part of executing
 * those requests we could end up here again. This means we start in
 * the not direct path and we later end up in the non-direct path.
 * 
 * The consequence of this is that while processing callbacks we can
 * fill up at least the bind queue and possibly even the map queue.
 * Thus we need to check all lists after completing processing all
 * the bind, map and callback lists.
 */
void
Pgman::do_busy_loop(Signal* signal, bool direct, EmulatedJamBuffer *jamBuf)
{
  D(">do_busy_loop on=" << m_busy_loop_on << " direct=" << direct);
  Uint32 restart = false;
  if (direct)
  {
    thrjam(jamBuf);
    // may not cover the calling entry
    (void)process_bind(signal, jamBuf);
    (void)process_map(signal, jamBuf);
    // callback must be queued
    if (! m_busy_loop_on)
    {
      thrjam(jamBuf);
      restart = true;
      m_busy_loop_on = true;
    }
  }
  else
  {
    thrjam(jamBuf);
    ndbrequire(m_busy_loop_on);
    restart = true;
    (void)process_bind(signal, jamBuf);
    (void)process_map(signal, jamBuf);
    (void)process_callback(signal, jamBuf);
    Page_sublist& pl_bind = *m_page_sublist[Page_entry::SL_BIND];
    Page_sublist& pl_map = *m_page_sublist[Page_entry::SL_MAP];
    Page_sublist& pl_callback = *m_page_sublist[Page_entry::SL_CALLBACK];

    if (pl_bind.isEmpty() && pl_map.isEmpty() && pl_callback.isEmpty())
    {
      thrjam(jamBuf);
      restart = false;
      m_busy_loop_on = false;
    }
  }
  if (restart)
  {
    thrjam(jamBuf);
    signal->theData[0] = PgmanContinueB::BUSY_LOOP;
    sendSignal(reference(), GSN_CONTINUEB, signal, 1, JBB);
  }
  D("<do_busy_loop on=" << m_busy_loop_on << " restart=" << restart);
}

void
Pgman::do_cleanup_loop(Signal* signal)
{
  //D("do_cleanup_loop");
  process_cleanup(signal);

  Uint32 delay = m_param.m_cleanup_loop_delay;
  signal->theData[0] = PgmanContinueB::CLEANUP_LOOP;
  sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, delay, 1);
}

// busy loop
bool
Pgman::process_bind(Signal* signal, EmulatedJamBuffer *jamBuf)
{
  D(">process_bind");
  int max_count = 32;
  Page_sublist& pl_bind = *m_page_sublist[Page_entry::SL_BIND];

  while (! pl_bind.isEmpty() && --max_count >= 0)
  {
    thrjam(jamBuf);
    Ptr<Page_entry> ptr;
    pl_bind.first(ptr);
    if (! process_bind(signal, ptr, jamBuf))
    {
      jam();
      thrjam(jamBuf);
      break;
    }
  }
  D("<process_bind");
  return ! pl_bind.isEmpty();
}

bool
Pgman::process_bind(Signal* signal,
                    Ptr<Page_entry> ptr,
                    EmulatedJamBuffer *jamBuf)
{
  D(ptr << " : process_bind");
  Page_queue& pl_queue = m_page_queue;
  Ptr<GlobalPage> gptr;

  if (m_stats.m_num_pages < m_param.m_max_pages)
  {
    thrjam(jamBuf);
    bool ok = seize_cache_page(gptr);
    // to handle failure requires some changes in LIRS
    ndbrequire(ok);
  }
  else
  {
    thrjam(jamBuf);
    Ptr<Page_entry> clean_ptr;
    if (! pl_queue.first(clean_ptr))
    {
      thrjam(jamBuf);
      D("bind failed: queue empty");
      // XXX busy loop
      return false;
    }
    Page_state clean_state = clean_ptr.p->m_state;
    // under unusual circumstances it could still be paging in
    if (! (clean_state & Page_entry::MAPPED) ||
        clean_state & Page_entry::DIRTY ||
        clean_state & Page_entry::REQUEST)
    {
      thrjam(jamBuf);
      D("bind failed: queue front not evictable");
      D(clean_ptr);
      // XXX busy loop
      return false;
    }

    D(clean_ptr << " : evict");

    ndbassert(clean_ptr.p->m_dirty_count == 0);
    ndbrequire(clean_state & Page_entry::ONQUEUE);
    ndbrequire(clean_state & Page_entry::BOUND);
    ndbrequire(clean_state & Page_entry::MAPPED);

    move_cleanup_ptr(clean_ptr, jamBuf);
    pl_queue.remove(clean_ptr);
    clean_state &= ~ Page_entry::ONQUEUE;

    gptr.i = clean_ptr.p->m_real_page_i;

    clean_ptr.p->m_real_page_i = RNIL;
    clean_state &= ~ Page_entry::BOUND;
    clean_state &= ~ Page_entry::MAPPED;

    set_page_state(jamBuf, clean_ptr, clean_state);

    if (! (clean_state & Page_entry::ONSTACK))
    {
      thrjam(jamBuf);
      release_page_entry(clean_ptr, jamBuf);
    }

    m_global_page_pool.getPtr(gptr);
  }

  Page_state state = ptr.p->m_state;

  ptr.p->m_real_page_i = gptr.i;
  state |= Page_entry::BOUND;
  if (state & Page_entry::EMPTY)
  {
    /**
     * When we retrieve an EMPTY page we don't read it from disk.
     * We will immediately overwrite it.
     */
    thrjam(jamBuf);
    state |= Page_entry::MAPPED;
  }

  if (! (state & Page_entry::LOCKED) &&
      ! (state & Page_entry::ONQUEUE) &&
      ! (state & Page_entry::HOT))
  {
    thrjam(jamBuf);

    D(ptr << " : add to queue at bind");
    pl_queue.addLast(ptr);
    state |= Page_entry::ONQUEUE;
  }

  set_page_state(jamBuf, ptr, state);
  return true;
}

bool
Pgman::process_map(Signal* signal, EmulatedJamBuffer *jamBuf)
{
  D(">process_map");
  int max_count = 0;
  if (m_param.m_max_io_waits > m_stats.m_current_io_waits) {
    max_count = m_param.m_max_io_waits - m_stats.m_current_io_waits;
    max_count = max_count / 2 + 1;
  }
  Page_sublist& pl_map = *m_page_sublist[Page_entry::SL_MAP];

  while (! pl_map.isEmpty() && --max_count >= 0)
  {
    thrjam(jamBuf);
    Ptr<Page_entry> ptr;
    pl_map.first(ptr);
    if (! process_map(signal, ptr, jamBuf))
    {
      thrjam(jamBuf);
      break;
    }
  }
  D("<process_map");
  return ! pl_map.isEmpty();
}

bool
Pgman::process_map(Signal* signal,
                   Ptr<Page_entry> ptr,
                   EmulatedJamBuffer *jamBuf)
{
  D(ptr << " : process_map");
  pagein(signal, ptr, jamBuf);
  return true;
}

bool
Pgman::process_callback(Signal* signal, EmulatedJamBuffer *jamBuf)
{
  D(">process_callback");
  int max_count = 1;
  Page_sublist& pl_callback = *m_page_sublist[Page_entry::SL_CALLBACK];

  Ptr<Page_entry> ptr;
  pl_callback.first(ptr);

  while (! ptr.isNull() && --max_count >= 0)
  {
    thrjam(jamBuf);
    Ptr<Page_entry> curr = ptr;
    pl_callback.next(ptr);
    
    if (! process_callback(signal, curr, jamBuf))
    {
      thrjam(jamBuf);
      break;
    }
  }
  D("<process_callback");
  return ! pl_callback.isEmpty();
}

bool
Pgman::process_callback(Signal* signal,
                        Ptr<Page_entry> ptr,
                        EmulatedJamBuffer *jamBuf)
{
  D(ptr << " : process_callback");
  int max_count = 1;

  while (! ptr.p->m_requests.isEmpty() && --max_count >= 0)
  {
    thrjam(jamBuf);
    Page_state state = ptr.p->m_state;
    SimulatedBlock* b;
    Callback callback;
    {
      /**
       * Make sure list is in own scope if callback will access this
       * list again (destructor restores list head).
       */
      Local_page_request_list req_list(m_page_request_pool, ptr.p->m_requests);
      Ptr<Page_request> req_ptr;

      req_list.first(req_ptr);
      D(req_ptr << " : process_callback");

#ifdef ERROR_INSERT
      if (req_ptr.p->m_flags & Page_request::DELAY_REQ)
      {
	const NDB_TICKS now = NdbTick_getCurrentTicks();
	if (NdbTick_Compare(now,req_ptr.p->m_delay_until_time) < 0)
	{
	  break;
	}
      }
#endif
      
      Uint32 blockNo = blockToMain(req_ptr.p->m_block);
      Uint32 instanceNo = blockToInstance(req_ptr.p->m_block);
      b = globalData.getBlock(blockNo, instanceNo);
      callback = req_ptr.p->m_callback;
      
      if (req_ptr.p->m_flags & DIRTY_FLAGS)
      {
        thrjam(jamBuf);
        /**
         * Given that the page entry is in the SL_CALLBACK sublist it cannot
         * be in pageout to disk. So there is no need to check here for
         * PAGEOUT, actually we even put an assert on this here.
         */
        ndbrequire(! (state & Page_entry::PAGEOUT));
        state |= Page_entry::DIRTY;
        insert_fragment_dirty_list(ptr, state, jamBuffer());
        ndbassert(ptr.p->m_dirty_count);
        ptr.p->m_dirty_count--;
      }

      NDB_TICKS now = getHighResTimer();
      NDB_TICKS start = req_ptr.p->m_start_time;
      Uint64 micros = NdbTick_Elapsed(start, now).microSec();
      add_histogram(micros, &m_time_track_get_page[0]);
      m_get_page_reqs_completed++;
      m_total_dd_latency_us += micros;
      m_num_dd_accesses++;
      ndbassert(m_outstanding_dd_requests > 0);
      m_outstanding_dd_requests--;
      DEB_GET_PAGE(("(%u)get_page(%u,%u) resume, flags: %u, state: %u"
                    ", opRec: %u, outstanding IOs: %llu, micros: %llu",
                    instance(),
                    ptr.p->m_file_no,
                    ptr.p->m_page_no,
                    req_ptr.p->m_flags,
                    ptr.p->m_state,
                    req_ptr.p->m_callback.m_callbackData,
                    m_outstanding_dd_requests,
                    micros));
      req_list.releaseFirst(/* req_ptr */);
    }
    ndbrequire(state & Page_entry::BOUND);
    ndbrequire(state & Page_entry::MAPPED);

    // make REQUEST state consistent before set_page_state()
    if (ptr.p->m_requests.isEmpty())
    {
      thrjam(jamBuf);
      state &= ~ Page_entry::REQUEST;
    }
    
    // callback may re-enter PGMAN and change page state
    set_page_state(jamBuf, ptr, state);
    b->execute(signal, callback, ptr.p->m_real_page_i);
  }
  return true;
}

// cleanup loop

bool
Pgman::process_cleanup(Signal* signal)
{
  //D(">process_cleanup");
  Page_queue& pl_queue = m_page_queue;

  // XXX for now start always from beginning
  m_cleanup_ptr.i = RNIL;

  if (m_cleanup_ptr.i == RNIL && ! pl_queue.first(m_cleanup_ptr))
  {
    jam();
    //D("<process_cleanup: empty queue");
    return false;
  }

  int max_loop_count = m_param.m_max_loop_count;
  int max_count = 0;
  if (m_param.m_max_io_waits > m_stats.m_current_io_waits) {
    max_count = m_param.m_max_io_waits - m_stats.m_current_io_waits;
    max_count = max_count / 2 + 1;
  }

  Ptr<Page_entry> ptr = m_cleanup_ptr;
  while (max_loop_count != 0 && max_count != 0)
  {
    Page_state state = ptr.p->m_state;
    ndbrequire(! (state & Page_entry::LOCKED));
    if (state & Page_entry::BUSY)
    {
      D("process_cleanup: break on busy page");
      D(ptr);
      break;
    }
    if (state & Page_entry::DIRTY &&
        ! (state & Page_entry::PAGEIN) &&
        ! (state & Page_entry::PAGEOUT))
    {
      D(ptr << " : process_cleanup");
      if (c_tup != 0)
        c_tup->disk_page_unmap_callback(0, 
                                        ptr.p->m_real_page_i, 
                                        ptr.p->m_dirty_count,
                                        ptr.i);
      DEB_PGMAN_PAGE(("(%u)pageout():cleanup, page(%u,%u):%u:%x",
                      instance(),
                      ptr.p->m_file_no,
                      ptr.p->m_page_no,
                      ptr.i,
                      (unsigned int)state));

      pageout(signal, ptr);
      max_count--;
    }
    if (! pl_queue.hasNext(ptr))
      break;
    pl_queue.next(ptr);
    max_loop_count--;
  }
  m_cleanup_ptr = ptr;
  D("<process_cleanup");
  return true;
}

/*
 * Call this before queue.remove(ptr).  If the removed entry is the
 * clean-up pointer, move it towards front.
 */
void
Pgman::move_cleanup_ptr(Ptr<Page_entry> ptr, EmulatedJamBuffer *jamBuf)
{
  Page_queue& pl_queue = m_page_queue;
  if (ptr.i == m_cleanup_ptr.i)
  {
    thrjam(jamBuf);
    pl_queue.prev(m_cleanup_ptr);
  }
}

/**
 * LCP Module
 * ----------
 */

/**
 * The below methods are only used at restarts to synch the page cache after
 * the UNDO log execution.
 */
void
Pgman::sendSYNC_PAGE_CACHE_REQ(Signal *signal, FragmentRecordPtr fragPtr)
{
  SyncPageCacheReq* req = (SyncPageCacheReq*)signal->getDataPtrSend();
  req->senderData = fragPtr.i;
  req->senderRef = reference();
  req->tableId = fragPtr.p->m_table_id;
  req->fragmentId = fragPtr.p->m_fragment_id;
  sendSignal(reference(), GSN_SYNC_PAGE_CACHE_REQ, signal,
             SyncPageCacheReq::SignalLength, JBA);
}

void
Pgman::sendSYNC_EXTENT_PAGES_REQ(Signal *signal)
{
  SyncExtentPagesReq *req = (SyncExtentPagesReq*)signal->getDataPtrSend();
  req->senderData = 0;
  req->senderRef = reference();
  req->lcpOrder = SyncExtentPagesReq::RESTART_SYNC;
  sendSignal(reference(), GSN_SYNC_EXTENT_PAGES_REQ, signal,
             SyncExtentPagesReq::SignalLength, JBA);
}

void
Pgman::sendEND_LCPCONF(Signal *signal)
{
  DEB_PGMAN_LCP(("(%u)sendEND_LCPCONF", instance()));
  EndLcpConf *conf = (EndLcpConf*)signal->getDataPtrSend();
  conf->senderData = m_end_lcp_req.senderData;
  sendSignal(m_end_lcp_req.senderRef, GSN_END_LCPCONF, signal,
             EndLcpConf::SignalLength, JBA);
}

void
Pgman::execEND_LCPREQ(Signal *signal)
{
  EndLcpReq *req = (EndLcpReq*)signal->getDataPtr();
  /**
   * As part of restart we need to synchronize all data pages to
   * disk. We do this by syncing each fragment, one by one and
   * for the extra PGMAN worker it means that we synchronize the
   * extent pages.
   */
  FragmentRecordPtr fragPtr;
  m_end_lcp_req = *req;
  ndbrequire(!m_lcp_ongoing);
  if (!get_first_ordered_fragment(fragPtr))
  {
    if (m_extra_pgman || !isNdbMtLqh())
    {
      jam();
      sendSYNC_EXTENT_PAGES_REQ(signal);
      return;
    }
    jam();
    sendEND_LCPCONF(signal);
  }
  else
  {
    /**
     * There are no table objects in the proxy block.
     */
    ndbrequire(!m_extra_pgman);
    lcp_start_point(signal, 0, 0);
    sendSYNC_PAGE_CACHE_REQ(signal, fragPtr);
  }
}

void
Pgman::execSYNC_PAGE_CACHE_CONF(Signal *signal)
{
  SyncPageCacheConf* conf = (SyncPageCacheConf*)signal->getDataPtr();
  FragmentRecordPtr fragPtr;

  fragPtr.i = conf->senderData;
  m_fragmentRecordPool.getPtr(fragPtr);
  if (!get_next_ordered_fragment(fragPtr))
  {
    /**
     * We need to create an LCP end point before ending the sync of
     * disk pages. In the case of single threaded ndbd we next proceed
     * with sync of the extent pages, we still need to create an end
     * point of the LCP since the next step will be to create an LCP
     * start point when executing SYNC_EXTENT_PAGES_REQ(RESTART_SYNC).
     */ 
    NDB_TICKS now = getHighResTimer();
    Uint64 lcp_time = NdbTick_Elapsed(m_lcp_start_time,now).milliSec();
    lcp_end_point(Uint32(lcp_time), true, true);
    if (isNdbMtLqh())
    {
      jam();
      sendEND_LCPCONF(signal);
      return;
    }
    jam();
    sendSYNC_EXTENT_PAGES_REQ(signal);
  }
  else
  {
    jam();
    sendSYNC_PAGE_CACHE_REQ(signal, fragPtr);
  }
}

void
Pgman::execSYNC_EXTENT_PAGES_CONF(Signal *signal)
{
  sendEND_LCPCONF(signal);
}

bool
Pgman::idle_fragment_lcp(Uint32 tableId, Uint32 fragmentId)
{
  /**
   * Our handling of disk data requires us to be in synch with
   * the backup block on which fragment has completed the LCP.
   * In addition if we for some reason has outstanding disk
   * writes and/or there are dirty pages. This is possible
   * even when no committed changes have been performed when
   * timing is such that the commit haven't happened yet, but
   * the page have been set to dirty.
   *
   * Since we want to keep consistency to be able to check for
   * various error conditions we report that we need a real
   * LCP to be done in those cases. An idle LCP would endanger
   * our consistency of the count of outstanding Prepare LCP
   * writes. This consistency is guaranteed if we use a normal
   * LCP execution.
   *
   * If idle list is empty we are also certain that no outstanding
   * Prepare LCP requests are around. They are removed from dirty
   * list when the disk IO request is done.
   */
  FragmentRecord key(*this, tableId, fragmentId);
  FragmentRecordPtr fragPtr;
  if (m_fragmentRecordHash.find(fragPtr, key))
  {
    jam();
    if (likely(fragPtr.p->m_dirty_list.isEmpty()))
    {
      jam();
      m_prev_lcp_table_id = tableId;
      m_prev_lcp_fragment_id = fragmentId;
      return true;
    }
    else
    {
      jam();
      return false;
    }
  }
  jam();
  /**
   * m_lcp_table_id and m_lcp_fragment_id points to the
   * last disk data fragment that completed the checkpoint.
   * If this points to a table without disk data it will
   * point to a non-existing record in PGMAN.
   */
  return true;
}
 
/**
 * This is the module that handles LCP, SYNC_PAGE_CACHE_REQ orders
 * LCP on a fragment for the data pages. SYNC_EXTENT_PAGES_REQ orders
 * LCP of all extent pages (but is executed for each fragment).
 */
void Pgman::execSYNC_PAGE_CACHE_REQ(Signal *signal)
{
  /**
   * A fragment of a table has completed its execution of an LCP.
   * We have been requested to write all pages that currently are
   * dirty to disk. We will only write dirty pages that are part
   * of this fragment.
   *
   * We will sync in two PGMAN instances for each fragment. The first
   * one is the PGMAN part of the same thread as the fragment resides
   * on. This means that we write the data pages of the fragment to
   * disk. The second PGMAN instance we write is the PGMAN proxy
   * instance. This instance takes care of all checkpointing all extent
   * pages for a fragment.
   */
  jamEntry();
  SyncPageCacheReq* req = (SyncPageCacheReq*)signal->getDataPtr();
  FragmentRecord key(*this, req->tableId, req->fragmentId);
  FragmentRecordPtr fragPtr;
  m_sync_page_cache_req = *req;
  if (!m_fragmentRecordHash.find(fragPtr, key))
  {
    /**
     * This fragment has no disk data attached to it, finish sync
     * of page cache without doing any work.
     */
    finish_lcp(signal, NULL);
    return;
  }
  ndbrequire(fragPtr.i != RNIL);
  ndbrequire(!m_sync_extent_pages_ongoing);
  ndbrequire(m_lcp_outstanding == 0);
  ndbrequire(!m_extra_pgman);
  ndbrequire(m_lcp_table_id == RNIL);

  DEB_PGMAN_LCP_EXTRA(("(%u)execSYNC_PAGE_CACHE_REQ", instance()));
  /**
   * Switch over active list to the other list. This means that we are
   * ready to send all the dirty pages of the previously active list to
   * disk. When the previously active list is empty, then the LCP of
   * disk pages part of fragment is completed.
   *
   * By switching the current lcp dirty state on the fragment we effectively
   * also change the state of all page entries in the list to ensure that we
   * later bring them out of the correct list.
   *
   * So when ptr.p->m_dirty_state == fragPtr.p->m_current_lcp_dirty_state it
   * means that we are in the fragment dirty list.
   */

  fragPtr.p->m_is_frag_ready_for_prep_lcp_writes = true;
  if (fragPtr.p->m_current_lcp_dirty_state == Pgman::IN_FIRST_FRAG_DIRTY_LIST)
  {
    jam();
    fragPtr.p->m_current_lcp_dirty_state = Pgman::IN_SECOND_FRAG_DIRTY_LIST;
  }
  else
  {
    jam();
    ndbrequire(fragPtr.p->m_current_lcp_dirty_state ==
               Pgman::IN_SECOND_FRAG_DIRTY_LIST);
    fragPtr.p->m_current_lcp_dirty_state = Pgman::IN_FIRST_FRAG_DIRTY_LIST;
  }
  m_lcp_table_id = req->tableId;
  m_lcp_fragment_id = req->fragmentId;
  DEB_PGMAN_LCP_EXTRA(("(%u)Move page_entries from dirty list to lcp list of"
                       " tab(%u,%u), list is %s",
                       instance(),
                       m_lcp_table_id,
                       m_lcp_fragment_id,
                       fragPtr.p->m_dirty_list.isEmpty() ?
                       "empty" : "not empty"));
  ndbrequire(m_dirty_list_lcp.isEmpty());
  ndbrequire(m_dirty_list_lcp_out.isEmpty());
  m_dirty_list_lcp.swapList(fragPtr.p->m_dirty_list);
  start_lcp_loop(signal);
}

void
Pgman::finish_lcp(Signal *signal,
                  FragmentRecord *fragPtrP)
{
  ndbrequire(m_lcp_outstanding == 0);
  /**
   * It is possible that we still have outstanding page writes
   * for Prepare LCP pages since we look ahead more than one
   * fragment. So we can only verify that this is 0 at the
   * end point of LCPs (lcp_end_point).
   */
  m_prev_lcp_table_id = m_lcp_table_id;
  m_prev_lcp_fragment_id = m_lcp_fragment_id;
  m_lcp_table_id = RNIL;
  m_lcp_fragment_id = 0;
  start_lcp_loop(signal);
  ndbrequire(m_dirty_list_lcp.isEmpty());
  ndbrequire(m_dirty_list_lcp_out.isEmpty());
  DEB_PGMAN_LCP(("(%u)finish_lcp tab(%u,%u), ref: %x",
                 instance(),
                 m_sync_page_cache_req.tableId,
                 m_sync_page_cache_req.fragmentId,
                 m_sync_page_cache_req.senderRef));
  SyncPageCacheConf* conf = (SyncPageCacheConf*)signal->getDataPtr();
  conf->senderData = m_sync_page_cache_req.senderData;
  conf->tableId = m_sync_page_cache_req.tableId;
  conf->fragmentId = m_sync_page_cache_req.fragmentId;
  conf->diskDataExistFlag = fragPtrP == NULL ? 0 : 1;
  sendSignal(m_sync_page_cache_req.senderRef,
             GSN_SYNC_PAGE_CACHE_CONF,
             signal,
             SyncPageCacheConf::SignalLength,
             JBA);
}

/**
 * For extent pages we write one page at a time and then send a CONTINUEB
 * signal. The CONTINUEB signal will take us here.
 *
 * LCP writes can be blocked by too many outstanding IOs. In this
 * case we are restarted by calling this function from fsreadconf
 * and fswriteconf.
 *
 * LCP writes can be blocked by too many outstanding writes.
 * In this case we will be restarted by calling this function from
 * fswriteconf.
 *
 * LCP writes can be blocked by a BUSY page. In this case we are
 * restarted by sending a LCP_LOOP CONTINUEB signal to execute
 * this function after unblocking the page.
 */
void
Pgman::start_lcp_loop(Signal *signal)
{
  if (m_lcp_loop_ongoing)
  {
    jam();
#ifdef DEBUG_PGMAN_LCP
    if (m_sync_extent_next_page_entry != RNIL)
    {
      DEB_PGMAN_LCP(("(%u) m_lcp_loop_ongoing true and extent pages left",
                     instance()));
    }
#endif
    return;
  }
  if (!m_lcp_ongoing)
  {
    jam();
#ifdef DEBUG_PGMAN_LCP
    if (m_sync_extent_next_page_entry != RNIL)
    {
      DEB_PGMAN_LCP(("(%u) m_lcp_loop_ongoing false and m_lcp_ongoing"
                     " false and extent pages left",
                     instance()));
    }
#endif
    m_lcp_loop_ongoing = false;
    return;
  }
  jam();
  m_lcp_loop_ongoing = true;
  signal->theData[0] = PgmanContinueB::LCP_LOOP;
  sendSignal(reference(), GSN_CONTINUEB, signal, 1, JBB);
}

void
Pgman::sendSYNC_PAGE_WAIT_REP(Signal *signal, bool normal_pages)
{
  Uint32 count;
  Uint32 senderData;
  BlockReference ref;
  if (normal_pages)
  {
    jam();
    count = m_dirty_list_lcp.getCount();
    count += m_dirty_list_lcp_out.getCount();
    ref = m_sync_page_cache_req.senderRef;
    senderData = m_sync_page_cache_req.senderData;
  }
  else
  {
    count = m_locked_pages_written;
    ref = m_sync_extent_pages_req.senderRef;
    senderData = m_sync_extent_pages_req.senderData;
  }
  if (refToMain(ref) == BACKUP && signal != NULL)
  {
    /**
     * This signal is only needed by Backup block to keep track
     * of progress of the LCP to ensure that LCP watchdog is
     * updated on every progress.
     *
     * When called from drop_page we don't have a signal object.
     * At the same time we focus on IO progress and not on tables
     * being dropped.
     *
     * We send it as direct signal for normal pages to avoid
     * overhead of otherwise sending on A-level. A-level would
     * be needed as SYNC_PAGE_CACHE_CONF is sent on A-level to
     * avoid the signals to come in wrong order.
     *
     * For extent pages it must be a buffered but here it is
     * sufficient to send on B-level since SYNC_EXTENT_PAGES_CONF
     * is sent on B-level.
     */
    jam();
    signal->theData[0] = senderData;
    signal->theData[1] = count;
    if (normal_pages)
    {
      jam();
      EXECUTE_DIRECT(BACKUP, GSN_SYNC_PAGE_WAIT_REP, signal, 2);
    }
    else
    {
      jam();
      sendSignal(ref, GSN_SYNC_PAGE_WAIT_REP, signal, 2, JBB);
    }
  }
}

#define MAX_PREPARE_LCP_SEARCH_DEPTH 4
void
Pgman::check_restart_lcp(Signal *signal, bool check_prepare_lcp)
{
  if (m_lcp_loop_ongoing)
  {
    jam();
    /**
     * CONTINUEB(LCP_LOOP) signal is outstanding, no need to
     * do anything more here. We don't want to complete the
     * LCPs with outstanding CONTINUEB signals.
     */
#ifdef DEBUG_PGMAN_LCP
    if (m_sync_extent_pages_ongoing)
    {
      DEB_PGMAN_LCP(("(%u)check_restart_lcp, m_lcp_loop_ongoing true"
                     " and outstanding extent pages",
                     instance()));
    }
#endif
    return;
  }
  if (m_sync_extent_pages_ongoing)
  {
    jam();
    /**
     * SYNC_EXTENT_PAGES was ongoing, continueb isn't running and
     * we're also not waiting for any outstanding IO. This must mean
     * that we were blocked by too much IO, so we'll start up the
     * process again here.
     */
    ndbrequire(m_lcp_ongoing == true);
    if (m_sync_extent_next_page_entry != RNIL)
    {
      /**
       * We have more pages to write before the Sync of the extent
       * pages is completed.
       */
      jam();
      Ptr<Page_entry> ptr;
      Page_sublist& pl = *m_page_sublist[Page_entry::SL_LOCKED];
      pl.getPtr(ptr, m_sync_extent_next_page_entry);
      process_lcp_locked(signal, ptr);
    }
    else if (m_lcp_outstanding == 0)
    {
      jam();
      /**
       * We had an outstanding CONTINUEB signal when we had the last
       * write of the sync of extent pages completed, we had to wait
       * until here to finish the sync of extent pages.
       */
      finish_sync_extent_pages(signal);
    }
    else
    {
      /**
       * We have written all pages, but we are still waiting for one
       * or more File IO completion (processed by
       * process_lcp_locked_fswriteconf). No need to use CONTINUEB to
       * wait for it, it will arrive in a FSWRITECONF signal.
       */
      DEB_PGMAN_LCP(("(%u)Sync extent completed, but still %u LCP pages out",
                     instance(),
                     m_lcp_outstanding));
      jam();
    }
    return;
  }
  if (m_lcp_table_id != RNIL)
  {
    /**
     * Either we have completed write of a page written for LCP, or we
     * could be ready to send more pages in LCP since there is room for
     * more outstanding LCP pages.
     * Either way we call the function that checks to see if we should
     * send pages for LCP to disk. This function also completes the
     * writing when no more pages remain to be written.
     */
    jam();
    ndbrequire(m_lcp_ongoing == true);
    handle_lcp(signal, m_lcp_table_id, m_lcp_fragment_id);
  }
  else if (m_prev_lcp_table_id != RNIL && check_prepare_lcp)
  {
    jam();
    /**
     * Currently we only do a look ahead a constant number of fragments
     * ahead of the next fragment to LCP. Looking ahead too much can
     * be costly since it could lead to writing pages too
     * early and thus waste disk bandwidth. Not looking ahead
     * at all means that we write the minimum amount, but we
     * tend to be a bit bursty in our writing and thus not
     * use the full bandwidth of the disk subsystem.
     *
     * Striking a balance between those two extremes is
     * important, for now we look ahead up to four fragments.
     */
    ndbrequire(m_lcp_ongoing == true);
    FragmentRecordPtr fragPtr;
    fragPtr.p = 0; //Silence compiler
    if (m_prev_lcp_table_id == 0)
    {
      /**
       * We have started a new LCP, so far we haven't performed
       * any fragment LCP. In this state we will start doing
       * preparation of work for LCPs by starting to write pages
       * from the dirty list of the first fragment to perform
       * an LCP on.
       */
      get_first_ordered_fragment(fragPtr);
      if (fragPtr.i == RNIL)
      {
        jam();
        /* No disk data tables exists */
        return;
      }
      TableRecordPtr tabPtr;
      ndbrequire(m_tableRecordPool.getPtr(tabPtr, fragPtr.p->m_table_id));
      if (tabPtr.p->m_is_table_ready_for_prep_lcp_writes &&
          fragPtr.p->m_is_frag_ready_for_prep_lcp_writes)
      {
        /**
         * We don't care about non-active tables in the Prepare LCP
         * handling, a non-active table that is found in ordered
         * fragment list is being dropped.
         */
        if (!fragPtr.p->m_dirty_list.isEmpty())
        {
          jam();
          handle_prepare_lcp(signal, fragPtr);
          return;
        }
      }
    }
    else
    {
      FragmentRecord key(*this,
                         m_prev_lcp_table_id,
                         m_prev_lcp_fragment_id);
      if (!m_fragmentRecordHash.find(fragPtr, key))
      {
        jam();
        /**
         * The current fragment is part of a dropped table, we
         * will get back on track as soon as the next fragment is
         * performing its LCP for disk data. So no need to do
         * anything advanced for this rare event.
         */
        return;
      }
    }
    Uint32 loop = 0;
    do
    {
      jam();
      if (!get_next_ordered_fragment(fragPtr))
      {
        jam();
        /**
         * We found no easy way to discover a next fragment. We will stop
         * here and return later.
         */
        return;
      }
      TableRecordPtr tabPtr;
      ndbrequire(m_tableRecordPool.getPtr(tabPtr, fragPtr.p->m_table_id));
      if (tabPtr.p->m_is_table_ready_for_prep_lcp_writes &&
          fragPtr.p->m_is_frag_ready_for_prep_lcp_writes)
      {
        if (!fragPtr.p->m_dirty_list.isEmpty())
        {
          jam();
          handle_prepare_lcp(signal, fragPtr);
          return;
        }
      }
      loop++;
    } while (loop < MAX_PREPARE_LCP_SEARCH_DEPTH);
    m_prev_lcp_table_id = RNIL;
  }
}

Uint32
Pgman::get_num_lcp_pages_to_write(bool is_prepare_phase)
{
  Uint64 lcp_outstanding = m_lcp_outstanding + m_prep_lcp_outstanding;
  Uint64 max_count = 0;
  Uint64 max_lcp_pages_outstanding = is_prepare_phase ?
         m_prep_max_lcp_pages_outstanding :
         m_max_lcp_pages_outstanding;
  if (m_param.m_max_io_waits > m_stats.m_current_io_waits &&
      lcp_outstanding < max_lcp_pages_outstanding)
  {
    jam();
    max_count = m_param.m_max_io_waits - m_stats.m_current_io_waits;
    max_count = max_count / 2 + 1;
    if (max_count > (max_lcp_pages_outstanding - lcp_outstanding))
    {
      /**
       * Never more than 1 MByte of outstanding LCP pages at any time.
       * We don't want to use too much of the disk bandwidth for
       * writing out the LCP.
       */
      jam();
      max_count = m_max_lcp_pages_outstanding - lcp_outstanding;
    }
    return max_count;
  }
  else
  {
    jam();
    /**
     * Already used up all room for outstanding disk IO. Continue
     * processing LCP when disk IO bandwidth is available again.
     */
    return 0;
  }
  ndbassert(max_count > 0);
}

void
Pgman::handle_prepare_lcp(Signal *signal, FragmentRecordPtr fragPtr)
{
  Ptr<Page_entry> ptr;
  Uint32 max_count = get_num_lcp_pages_to_write(true);
  if (max_count == 0 ||
      m_available_lcp_pageouts_used >= m_prep_available_lcp_pageouts)
  {
    jam();
    DEB_PGMAN_EXTRA(("(%u)No LCP pages available to write with for Prep LCP"
                     instance()));
    jam();
    return;
  }
  {
    LocalPage_dirty_list list(m_page_entry_pool, fragPtr.p->m_dirty_list);
    list.first(ptr);
  }
  bool break_flag = false;
  Uint64 synced_lsn;
  {
    Logfile_client lgman(this, c_lgman, RNIL);
    synced_lsn = lgman.pre_sync_lsn(ptr.p->m_lsn);
  }
  for (Uint32 i = 0; i < max_count; i++)
  {
    if (ptr.i == RNIL)
    {
      jam();
      return;
    }
    Page_state state = ptr.p->m_state;
    /* See comments in handle_lcp on state handling */ 
    if ((! (state & Page_entry::DIRTY)) ||
        (state & Page_entry::LOCKED) ||
        (! (state & Page_entry::BOUND)))
    {
      print(g_eventLogger, ptr);
      ndbrequire(false);
    }
    if (state & Page_entry::PAGEOUT ||
        state & Page_entry::BUSY)
    {
      jam();
      /* Ignore since we are in prepare LCP state */
    }
    else
    {
      Uint32 no = get_sublist_no(state);
      if (no != Page_entry::SL_CALLBACK &&
          ptr.p->m_lsn < synced_lsn)
      {
        jam();
        DEB_PGMAN_PREP_PAGE((
                        "(%u)pageout():prepare LCP, page(%u,%u):%u:%x"
                        ", m_prep_lcp_outstanding = %u",
                        instance(),
                        ptr.p->m_file_no,
                        ptr.p->m_page_no,
                        ptr.i,
                        (unsigned int)state,
                        m_prep_lcp_outstanding + 1));
        ptr.p->m_state |= Page_entry::PREP_LCP;

        if (c_tup != 0)
        {
          c_tup->disk_page_unmap_callback(0,
                                          ptr.p->m_real_page_i, 
                                          ptr.p->m_dirty_count,
                                          ptr.i);
        }
        TableRecordPtr tabPtr;
        ndbrequire(m_tableRecordPool.getPtr(tabPtr, fragPtr.p->m_table_id));
        tabPtr.p->m_num_prepare_lcp_outstanding++;
        pageout(signal, ptr, false);
        break_flag = true;
        m_current_lcp_pageouts++;
        m_prep_lcp_outstanding++;
        m_available_lcp_pageouts_used++;
      }
      else
      {
        jam();
        /**
         * We will never write anything that is in SL_CALLBACK list.
         * We are only in Prepare LCP phase, so it is not very vital
         * to write the page at this time. It is more important to
         * allow the waiting operation to be able to read the page.
         * We will break and move the page last.
         *
         * We will also not write anything that would generate a wait
         * to force the UNDO log in the prepare LCP phase.
         */
      }
    }
    if (break_flag)
    {
      jam();
      break;
    }
    {
      LocalPage_dirty_list list(m_page_entry_pool, fragPtr.p->m_dirty_list);
      list.next(ptr);
    }
  }
  if (break_flag)
  {
    jam();
    start_lcp_loop(signal);
  }
}

#define MAX_SKIPPED_CALLBACK 32
void
Pgman::handle_lcp(Signal *signal, Uint32 tableId, Uint32 fragmentId)
{
  FragmentRecord key(*this, tableId, fragmentId);
  FragmentRecordPtr fragPtr;
  Ptr<Page_entry> ptr;
  Uint32 max_count = 0;
  ndbrequire(m_fragmentRecordHash.find(fragPtr, key));
  FragmentRecord *fragPtrP = fragPtr.p;

  if (m_dirty_list_lcp.isEmpty() && m_dirty_list_lcp_out.isEmpty())
  {
    jam();
    DEB_PGMAN(("(%u)handle_lcp finished", instance()));
    finish_lcp(signal, fragPtrP);
    return;
  }
  if ((max_count = get_num_lcp_pages_to_write(false)) == 0 ||
       m_available_lcp_pageouts_used >= m_available_lcp_pageouts)
  {
    jam();
    DEB_PGMAN_EXTRA(("No LCP pages available to write with, instance(): %u",
               instance()));
    return;
  }
  bool break_flag = false;
  Uint32 skipped_callbacks = 0;
  bool last_was_callback = false;
  for (Uint32 i = 0; i < max_count; i++)
  {
    m_dirty_list_lcp.first(ptr);
    if (ptr.i == RNIL)
    {
      jam();
      /**
       * No more pages to write out to disk for this LCP.
       * Wait for those outstanding to be completed and then
       * we're done.
       */
      m_dirty_list_lcp_out.first(ptr);
      ndbrequire(ptr.i != RNIL);
      DEB_PGMAN_LCP_EXTRA(("(%u)LCP wait for write out to disk",
                           instance()));
      return;
    }
    Page_state state = ptr.p->m_state;
  
    if ((! (state & Page_entry::DIRTY)) ||
        (state & Page_entry::LOCKED) ||
        (! (state & Page_entry::BOUND)))
    {
      print(g_eventLogger, ptr);
      ndbabort();
    }

    if (state & Page_entry::PAGEOUT)
    {
      jam();

      /**
       * We could be in BUSY state here if PAGEOUT was started before
       * setting the BUSY state. In this case we need not wait for
       * BUSY state to be completed. We simply wait for PAGEOUT to
       * be completed.
       */
      DEB_PGMAN_PAGE(("(%u)PAGEOUT state in LCP, page(%u,%u):%u:%x",
                      instance(),
                      ptr.p->m_file_no,
                      ptr.p->m_page_no,
                      ptr.i,
                      (unsigned int)state));

      ndbrequire(ptr.p->m_dirty_state != fragPtrP->m_current_lcp_dirty_state);
      m_dirty_list_lcp.removeFirst(ptr);
      m_dirty_list_lcp_out.addLast(ptr);
      ptr.p->m_dirty_state = Pgman::IN_LCP_OUT_LIST;
      last_was_callback = false;
      if (!(state & Page_entry::LCP ||
            state & Page_entry::PREP_LCP))
      {
        jam();
        m_lcp_outstanding++;
        m_current_lcp_pageouts++;
        set_page_state(jamBuffer(), ptr, state | Page_entry::LCP);
      }
    }
    else if (state & Page_entry::BUSY)
    {
      jam();
      DEB_PGMAN_EXTRA(("(%u)BUSY state in LCP, page(%u,%u):%u:%x",
                       instance(),
                       ptr.p->m_file_no,
                       ptr.p->m_page_no,
                       ptr.i,
                       (unsigned int)state));

      set_page_state(jamBuffer(), ptr, state | Page_entry::WAIT_LCP);
      /**
       * If there are other pages available to process while we are
       * waiting for the BUSY page then it is ok to do so. However to
       * avoid complex logic around this we simply move the BUSY page
       * to last in the list to have a look at it later. We will restart
       * the search for pages to write out as part of LCP when either of
       * three conditions occur.
       *
       * 1) A BUSY condition on a page is removed
       * 2) A write of a page is completed (fswriteconf)
       * 3) A read of a page is completed (fsreadconf)
       *
       * This move of the page to last will hopefully improve things
       * at least for large fragments. The wait for BUSY to be removed
       * is normally a short wait, but there might be a disk read
       * involved as part of the wait and in the future it might
       * potentially be multiple disk reads that is waited for.
       */
      m_dirty_list_lcp.removeFirst(ptr);
      m_dirty_list_lcp.addLast(ptr);
      return; // wait for it
    }
    else
    {
      Uint32 no = get_sublist_no(state);
      if (no != Page_entry::SL_CALLBACK ||
          !m_dirty_list_lcp.hasNext(ptr) ||
          skipped_callbacks > MAX_SKIPPED_CALLBACK ||
          last_was_callback)
      {
        jam();
        DEB_PGMAN_PAGE(("(%u)pageout():LCP, page(%u,%u):%u:%x",
                        instance(),
                        ptr.p->m_file_no,
                        ptr.p->m_page_no,
                        ptr.i,
                        (unsigned int)state));

        ndbrequire(ptr.p->m_dirty_state !=
                   fragPtrP->m_current_lcp_dirty_state);
        m_dirty_list_lcp.removeFirst(ptr);
        m_dirty_list_lcp_out.addLast(ptr);
        ptr.p->m_dirty_state = Pgman::IN_LCP_OUT_LIST;
        ptr.p->m_state |= Page_entry::LCP;
        if (c_tup != 0)
        {
          c_tup->disk_page_unmap_callback(0,
                                          ptr.p->m_real_page_i, 
                                          ptr.p->m_dirty_count,
                                          ptr.i);
        }
        pageout(signal, ptr);
        break_flag = true;
        m_current_lcp_pageouts++;
        m_lcp_outstanding++;
        m_available_lcp_pageouts_used++;
      }
      else
      {
        /**
         * We try to skip this page for now since it is in SL_CALLBACK
         * list. This means that very soon it will reply to a get_page
         * call. We try to avoid the extra latency from now sending it
         * to the disk. The get_page call has already waited for at
         * least one round already. We do however only move it one
         * step forward to avoid messing up the list that wants the
         * latest dirtied pages at the end. This should in most cases
         * work fine.
         *
         * We don't even attempt to skip if the page is the last in the
         * dirty list to write.
         *
         * We don't skip two pages after each other since this could easily
         * lead to eternal loop where we skip two pages.
         */
        jam();
        skipped_callbacks++;
        max_count++;
        Ptr<Page_entry> move_ptr;
        m_dirty_list_lcp.removeFirst(ptr);
        m_dirty_list_lcp.first(move_ptr);
        m_dirty_list_lcp.insertAfter(ptr, move_ptr);
        last_was_callback = true;
      }
    }
    if (break_flag)
    {
      jam();
      break;
    }
  }
  start_lcp_loop(signal);
}

void
Pgman::set_redo_alert_state(RedoStateRep::RedoAlertState new_state)
{
  if (new_state != m_redo_alert_state)
  {
    jam();
    if (new_state != RedoStateRep::NO_REDO_ALERT)
    {
      jam();
      m_raise_redo_alert_state = 2;
    }
  }
  m_redo_alert_factor = 100;
  m_redo_alert_state = new_state;
  switch (new_state)
  {
    case RedoStateRep::NO_REDO_ALERT:
    {
      if (m_raise_redo_alert_state > 0)
      {
        jam();
        m_raise_redo_alert_state = 1;
        m_redo_alert_factor = 101;
      }
      break;
    }
    case RedoStateRep::REDO_ALERT_LOW:
    {
      m_redo_alert_factor = 120;
      break;
    }
    case RedoStateRep::REDO_ALERT_HIGH:
    {
      m_redo_alert_factor = 140;
      break;
    }
    case RedoStateRep::REDO_ALERT_CRITICAL:
    {
      m_redo_alert_factor = 170;
      break;
    }
    default:
    {
      ndbrequire(false);
      break;
    }
  }
}

void
Pgman::set_lcp_dd_percentage(Uint32 dd_percentage)
{
  m_lcp_dd_percentage = Uint64(dd_percentage);
}

void
Pgman::set_current_disk_write_speed(Uint64 disk_write_speed)
{
  /**
   * Set current speed of checkpointing for in-memory data.
   * The value is in bytes per second in this particular
   * LDM thread.
   */
  m_mm_curr_disk_write_speed = disk_write_speed;
}

Uint64
Pgman::get_current_lcp_made_dirty()
{
  return (m_tot_pages_made_dirty - m_start_lcp_made_dirty);
}

void
Pgman::lcp_start_point(Signal *signal,
                       Uint32 max_undo_log_level,
                       Uint32 max_redo_log_level)
{
  Uint32 max_log_level = MAX(max_undo_log_level, max_redo_log_level);
  ndbrequire(!m_lcp_ongoing);
  m_lcp_ongoing = true;
  if (max_log_level > 0)
  {
    /**
     * max_log_level == 0 means that this is called from inside
     * PGMAN. This happens at restarts to flush pages. We don't
     * want to have any PREP_LCP writes performed in this case.
     * Thus we avoid setting m_prev_lcp_table_id to 0 which will
     * start off the PREP_LCP writes.
     * PREP_LCP writes are used to smooth out the checkpoint writes
     * for disk data pages during LCPs.
     */
    jam();
    m_prev_lcp_table_id = 0;
  }
  NDB_TICKS lcp_start_time = getHighResTimer();
  if (m_lcp_time_in_ms > 0)
  {
    Uint64 tot_millis = NdbTick_Elapsed(m_lcp_start_time,
                                        lcp_start_time).milliSec();
    m_lcp_start_time = lcp_start_time;
    if (m_lcp_time_in_ms > tot_millis)
    {
      jam();
      tot_millis = m_lcp_time_in_ms;
    }
    Uint64 percent_lcp = m_lcp_time_in_ms * Uint64(100);
    percent_lcp /= tot_millis;
    if (percent_lcp < 67)
    {
      /**
       * We never speed up more than 50% due to a long
       * time waiting for a new LCP to start up. Most likely
       * a long wait is simply an indication of an idle period
       * and this can be quickly followed by a busy period and
       * in this case it is not so good to increase the
       * checkpoint speed too much.
       */
      jam();
      m_percent_spent_in_checkpointing = 67;
    }
    else
    {
      jam();
      m_percent_spent_in_checkpointing = percent_lcp;
    }

    lock_access_extent_page();
    m_last_lcp_made_dirty = get_current_lcp_made_dirty();
    m_dirty_page_rate_per_sec = m_last_lcp_made_dirty * Uint64(1000) /
                                tot_millis;
    m_start_lcp_made_dirty = m_tot_pages_made_dirty;
    unlock_access_extent_page();
    Uint64 writes_since_last_lcp_start =
      m_tot_writes_completed - m_last_lcp_writes_completed;
    Uint64 latency_since_last_lcp_start =
      m_total_write_latency_us - m_last_lcp_total_write_latency_us;
    if (writes_since_last_lcp_start < 10)
    {
      /* Too small number to estimate, keep old estimate */
      jam();
    }
    else
    {
      jam();
      m_last_lcp_write_latency_us = latency_since_last_lcp_start /
                                    writes_since_last_lcp_start;
    }

    /**
     * We don't want checkpoint rate to be fast. This causes LCPs to
     * complete in a very short time, doing so means that we write
     * extent pages too quickly and we don't give the application any
     * chance to write the same page more than once. At the same time
     * we don't want the LCPs to take too long time either. Fast
     * checkpoints means fast recovery as well. We try to increase
     * the checkpoint time if it is below 10 seconds, otherwise we
     * don't make any changes to the checkpoint speed.
     *
     * As with all adaptive algorithms it is important to not change
     * the control parameters too fast. Therefore we only give small
     * changes to checkpoint speed to increase length of LCPs.
     *
     * We calculate whether it is ok to increase checkpoint time. If
     * it is we multiply the checkpoint speed by 90%. This means that
     * after a number of checkpoints we will have increased the
     * checkpoint time.
     *
     * We don't want to increase checkpoint speed such that gives any
     * risk of running out of UNDO log. We try to always keep UNDO log
     * below 25%. In addition we ignore any caps from this part if
     * the REDO log reports any type of overload problem.
     *
     * max undo log level set to 0 means it is a local call.
     */
    if (m_lcp_time_in_ms < 10000 &&
        max_log_level < 25 &&
        max_log_level > 0 &&
        m_redo_alert_factor == 100)
    {
      jam();
      if (m_lcp_time_in_ms < 2000 &&
          max_log_level < 20)
      {
        jam();
        m_max_pageout_rate = Uint64(67);
      }
      else if (m_lcp_time_in_ms < 4000 &&
               max_log_level < 22)
      {
        jam();
        m_max_pageout_rate = Uint64(75);
      }
      else if (m_lcp_time_in_ms < 8000 &&
               max_log_level < 23)
      {
        jam();
        m_max_pageout_rate = Uint64(83);
      }
      else
      {
        jam();
        m_max_pageout_rate = Uint64(90);
      }
    }
    else
    {
      jam();
      m_max_pageout_rate = Uint64(100);
    }

    DEB_PGMAN_LCP_STAT(("(%u)LCP Start: dirty rate: %llu pages/sec,"
                        " time since last LCP start: %llu ms, "
                        "total pages made dirty: %llu, "
                        "Writes since last LCP: %llu, "
                        "Write latency last LCP: %llu, "
                        "percent spent in checkpointing: %llu, "
                        "max pageout rate: %llu",
                        instance(),
                        m_dirty_page_rate_per_sec,
                        tot_millis,
                        m_last_lcp_made_dirty,
                        writes_since_last_lcp_start,
                        m_last_lcp_write_latency_us,
                        percent_lcp,
                        m_max_pageout_rate));
    m_last_lcp_total_write_latency_us = m_total_write_latency_us;
    m_last_lcp_writes_completed = m_tot_writes_completed;
  }
  else
  {
    jam();
    m_lcp_start_time = lcp_start_time;
    m_percent_spent_in_checkpointing = Uint64(100);
  }
  start_lcp_loop(signal);
  if (!m_track_lcp_speed_loop_ongoing)
  {
    jam();
    m_track_lcp_speed_loop_ongoing = true;
    m_last_track_lcp_speed_call = getHighResTimer();
    signal->theData[0] = PgmanContinueB::TRACK_LCP_SPEED_LOOP;
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 100, 1);
  }
}

bool
Pgman::lcp_end_point(Uint32 lcp_time_in_ms, bool first, bool internal)
{
  ndbrequire(m_lcp_ongoing == true || !first);
  ndbrequire(m_lcp_table_id == RNIL);
  m_lcp_ongoing = false;
  if (m_prep_lcp_outstanding > 0)
  {
    FragmentRecordPtr tmpFragPtr;
    if (m_prev_lcp_table_id != 0 &&
        m_prev_lcp_table_id != RNIL)
    {
      FragmentRecord key(*this,
                         m_prev_lcp_table_id,
                         m_prev_lcp_fragment_id);
      if (m_fragmentRecordHash.find(tmpFragPtr, key) &&
          get_next_ordered_fragment(tmpFragPtr))
      {
        g_eventLogger->info("(%u) isEmpty: %u, tab(%u,%u)",
                            instance(),
                            tmpFragPtr.p->m_dirty_list.isEmpty(),
                            tmpFragPtr.p->m_table_id,
                            tmpFragPtr.p->m_fragment_id);
      }
      else
      {
        g_eventLogger->info("(%u) not found, prev tab(%u,%u)",
                            instance(),
                            m_prev_lcp_table_id,
                            m_prev_lcp_fragment_id);
      }
    }
    else
    {
      g_eventLogger->info("(%u)m_prev_lcp_table_id = %u",
                          instance(),
                          m_prev_lcp_table_id);
    }
    /**
     * We have started performing PREP_LCP writes on a fragment
     * that was either dropped or it was recently created and
     * performed an early checkpoint. In this case we have to
     * wait until all PREP_LCP writes have finished before
     * we complete the LCP.
     *
     * It should be a very rare event, thus we make a printout to node log
     * here every time it happens.
     */
    ndbrequire(internal == false);
    m_prev_lcp_table_id = RNIL;
    return false;
  }
  m_prev_lcp_table_id = RNIL;
  Uint32 last_lcp_pageouts = m_current_lcp_pageouts;
  m_current_lcp_pageouts = Uint64(0);
  m_last_pageouts = Uint64(0);
  m_current_lcp_flushes = Uint64(0);
  m_last_flushes = Uint64(0);
  m_lcp_time_in_ms = Uint64(lcp_time_in_ms);
  Uint64 page_out_rate = Uint64(0);
  if (lcp_time_in_ms > 0)
  {
    lock_access_extent_page();
    m_dirty_page_rate_per_sec = get_current_lcp_made_dirty() * Uint64(1000) /
                                m_lcp_time_in_ms;
    unlock_access_extent_page();
    page_out_rate = last_lcp_pageouts * Uint64(1000) / m_lcp_time_in_ms;
  }
  else
  {
    m_dirty_page_rate_per_sec = Uint64(0);
  }
  (void)page_out_rate;
  DEB_PGMAN_LCP_STAT(("(%u)LCP End: page out rate: %llu, "
                      "dirty rate: %llu pages/sec,"
                      " LCP time: %llu ms",
                      instance(),
                      page_out_rate,
                      m_dirty_page_rate_per_sec,
                      m_lcp_time_in_ms));
  if (m_redo_alert_state == RedoStateRep::NO_REDO_ALERT)
  {
    jam();
    m_raise_redo_alert_state = 0;
  }
  m_redo_alert_state_last_lcp = m_redo_alert_state;
  return true;
}

void
Pgman::do_track_handle_lcp_speed_loop(Signal *signal)
{
  NDB_TICKS now = getHighResTimer();
  Uint64 millis = NdbTick_Elapsed(m_last_track_lcp_speed_call,now).milliSec();
  Uint64 millis_since_lcp_start =
    NdbTick_Elapsed(m_lcp_start_time, now).milliSec();

  if (millis > 90)
  {
    jam();
    lock_access_extent_page();
    Uint64 num_dirty_pages = m_num_dirty_pages;
    (void)num_dirty_pages;
    Uint64 dirty_rate_since_lcp = get_current_lcp_made_dirty();
    Uint64 dirty_rate = m_tot_pages_made_dirty - m_last_made_dirty;
    m_last_made_dirty = m_tot_pages_made_dirty;
    unlock_access_extent_page();
    Uint64 pageout_rate = m_current_lcp_pageouts - m_last_pageouts;
    m_last_pageouts = m_current_lcp_pageouts;
    Uint64 flush_rate = m_current_lcp_flushes - m_last_flushes;
    m_last_flushes = m_current_lcp_flushes;

    dirty_rate_since_lcp *= Uint64(1000);
    dirty_rate *= Uint64(1000);
    pageout_rate *= Uint64(1000);
    flush_rate *= Uint64(1000);
    if (millis_since_lcp_start < 90)
    {
      jam();
      dirty_rate_since_lcp = Uint64(0);
    }
    else
    {
      jam();
      dirty_rate_since_lcp /= millis_since_lcp_start;
    }
    dirty_rate /= millis;
    flush_rate /= millis;
    pageout_rate /= millis;

    /**
     * We will always allow at least 200 pageouts per second.
     * This is handled in the very last step of the calculations.
     *
     * We calculate the average number of pages made dirty per second in
     * previous LCP and multiply this by the factor of how much disk data
     * checkpointing takes of the total LCP time. This is one estimate of
     * required pageout rate.
     *
     * Next we calculate the same thing with the average number of pages
     * made dirty since the last LCP started, also multiplied by the same
     * factor.
     *
     * We also take the last 100 millisecond into account, but here we
     * decrease it to a maximum of 67% of this value. The idea with this
     * is to react quickly to changes in workload, but not too much.
     *
     * Finally we take the maximum of all those to create a desired pageout
     * rate for the next 100 millisecond.
     *
     * Given that we will not always use this pageout rate we increase it
     * by 10%.
     *
     * If dirty rate since last LCP divided by 1.3 is higher than dirty rate
     * in last LCP then use that instead.
     * If dirty rate last second divided by 1.5 is higher than previously
     * calculated pageouts per second we use this instead.
     *
     * The final step takes into account that we need to speed things up
     * if we are close to running out of UNDO or REDO log.
     *
     * Finally we turn the number into maximum number of pageouts for the
     * next 100 milliseconds.
     */
    Uint64 available_lcp_pageouts_per_sec = 0;
    Uint64 prep_available_lcp_pageouts_per_sec = 0;
    available_lcp_pageouts_per_sec = MAX(available_lcp_pageouts_per_sec,
                                         m_dirty_page_rate_per_sec);

    available_lcp_pageouts_per_sec = MAX(available_lcp_pageouts_per_sec,
             (dirty_rate_since_lcp * Uint64(100) / Uint64(130)));

    available_lcp_pageouts_per_sec = MAX(available_lcp_pageouts_per_sec,
                                (dirty_rate * Uint64(100) / Uint64(150)));

    available_lcp_pageouts_per_sec *= Uint64(110);
    available_lcp_pageouts_per_sec /= Uint64(100);

    available_lcp_pageouts_per_sec *= Uint64(m_redo_alert_factor);
    available_lcp_pageouts_per_sec /= Uint64(100);

    /**
     * We have calculated the disk data checkpoint speed required to keep
     * up with the current dirty rate. However a few points have to be
     * taken into account. We don't do any checkpoint between stop of LCP
     * and start of the next LCP. This time is normally around 1-2 seconds
     * only, but this can still be substantial if the total checkpoint
     * time is measured in single digit seconds as well.
     *
     * So we have to multiply the available LCP speed by this factor.
     */
    available_lcp_pageouts_per_sec *= Uint64(100);
    available_lcp_pageouts_per_sec /= m_percent_spent_in_checkpointing;

     /**
     * Calculate how many percent of the disk write bandwidth for LCPs that
     * currently is for disk data checkpointing.
     * We use this during Prepare LCP phase to ensure that we always have
     * about the same disk bandwidth used for checkpoints. This avoids
     * causing unnecessary speed bumps for disk data usage where latency
     * spikes would be seen otherwise when checkpointing has heavier load.
     *
     * We only use this calculation if the in-memory checkpoints and the
     * disk data checkpoints are using the same disks. We have a special
     * configuration parameter that the user can set to specify that the
     * disk data and in-memory checkpoints are using different disks.
     *
     * In addition we have to run a bit slower during in-memory checkpoints
     * and thus a bit faster during disk data checkpoints. Thus we have to
     * calculate one measurement for prepare LCP phase and one for disk
     * data checkpoint phase. Here we use calculations of how large part of
     * the time is spent in disk data checkpoints and how much time is spent
     * performing in-memory checkpoints.
     */
    Uint64 mm_curr_disk_write_speed = m_mm_curr_disk_write_speed;
    if (!m_dd_using_same_disk)
    {
      /**
       * The Disk data is running on different disk drives. Thus no need to
       * decrease speed of disk data checkpointing to avoid disk drive
       * overload. We can use a constant speed both during actual disk data
       * checkpoints and in between those checkpoints.
       */
      jam();
      mm_curr_disk_write_speed = Uint64(0);
    }
    {
      /**
       * We need to decrease the speed during in-memory checkpoints to even
       * out the load on the disk drive. We calculate the total disk speed
       * required in total and assign the full total to the time when we are
       * only performing disk data checkpoints and we share the load between
       * disk data and in-memory checkpoints when in-memory checkpoints are
       * executed.
       *
       * The Backup block informs us of how many percent of the time we are
       * spending in disk data checkpoints, it also informs us of the current
       * disk write speed. The current disk write speed for in-memory is
       * calculated based on how much time is spent in doing in-memory
       * checkpoints, so the average in-memory disk write speed needs to
       * be multiplied by the percentage of time spent in in-memory
       * checkpointing.
       */
      Uint64 dd_disk_write_speed = available_lcp_pageouts_per_sec *
                                   sizeof(Tup_page);
      mm_curr_disk_write_speed *= (Uint64(100) - m_lcp_dd_percentage);

      Uint64 tot_disk_write_speed =
        dd_disk_write_speed + mm_curr_disk_write_speed;

      if (m_mm_curr_disk_write_speed > tot_disk_write_speed)
      {
        /**
         * We write faster than the average disk write speed during
         * in-memory checkpoints. So no bandwidth available for
         * Prepare LCP checkpoint writes. Calculate the speed during
         * disk data checkpoints to handle the load in the time spent
         * on disk data checkpoints.
         */
        if (m_lcp_dd_percentage > 10)
        {
          jam();
          available_lcp_pageouts_per_sec *= Uint64(100);
          available_lcp_pageouts_per_sec /= m_lcp_dd_percentage;
        }
        else
        {
          jam();
          available_lcp_pageouts_per_sec *= Uint64(10);
        }
        prep_available_lcp_pageouts_per_sec = Uint64(0);
      }
      else
      {
        jam();
        available_lcp_pageouts_per_sec = tot_disk_write_speed /
                                         sizeof(Tup_page);
        prep_available_lcp_pageouts_per_sec =
          (tot_disk_write_speed - m_mm_curr_disk_write_speed) /
           sizeof(Tup_page);
      }
    }
    Uint64 available_lcp_pageouts_used = m_available_lcp_pageouts_used;
    (void)available_lcp_pageouts_used;
    m_available_lcp_pageouts_used = Uint64(0);

    /**
     * We will try to ensure LCPs don't run faster than once per 10
     * second if it is safe to do so. We avoid it when LCPs are already
     * longer than 10 seconds, when we have problems in keeping up with
     * LCPs anyways and when dirty rate have more than doubled since
     * last LCP (transient state that is better to handle with
     * calculated speed).
     */
    Uint64 limit = Uint64(2) *
                   MAX(dirty_rate, dirty_rate_since_lcp) / Uint64(3);
    if (m_redo_alert_state == RedoStateRep::NO_REDO_ALERT &&
        limit < m_dirty_page_rate_per_sec)
    {
      jam();
      available_lcp_pageouts_per_sec =
        available_lcp_pageouts_per_sec * m_max_pageout_rate / Uint64(100);
      prep_available_lcp_pageouts_per_sec =
        prep_available_lcp_pageouts_per_sec * m_max_pageout_rate / Uint64(100);
    }

    if (available_lcp_pageouts_per_sec < Uint64(200))
    {
      jam();
      available_lcp_pageouts_per_sec = Uint64(200);
    }

    m_available_lcp_pageouts = (available_lcp_pageouts_per_sec / Uint64(10));
    m_prep_available_lcp_pageouts =
      (prep_available_lcp_pageouts_per_sec / Uint64(10));

    /**
     * Now it is time to calculate the IO parallelism to get a smooth LCP
     * writing. It is not good to allow the LCPs to become bursty. This will
     * create higher latency for operations. We need to set the parallelism
     * sufficiently high to handle the desired speed, but not much higher.
     *
     * First calculate the IO rate with a single thread of writing LCPs.
     * Next multiply by 50% to get a bit of safety level, but not too safe.
     * Finally divide this by the desired pageouts per second due to LCPs.
     * This we will use to set the desired LCP IO parallelism. It can however
     * not be set higher than 192. Add one to the parallelism to ensure that
     * we don't lose anything in integer calculations.
     */
    Uint64 io_rate_single_thread = Uint64(1000) * Uint64(1000) /
                                   m_last_lcp_write_latency_us;
    if (io_rate_single_thread == Uint64(0))
    {
      jam();
      io_rate_single_thread = 1;
    }
    io_rate_single_thread *= Uint64(150);
    io_rate_single_thread /= Uint64(100);
    {
      Uint64 parallelism = available_lcp_pageouts_per_sec /
                           io_rate_single_thread;
      parallelism++;
      if (parallelism > 192)
      {
        jam();
        parallelism = 192;
      }
      m_max_lcp_pages_outstanding = parallelism;
    }
    if (prep_available_lcp_pageouts_per_sec == Uint64(0))
    {
      jam();
      m_prep_max_lcp_pages_outstanding = Uint64(0);
    }
    else
    {
      jam();
      Uint64 parallelism = prep_available_lcp_pageouts_per_sec /
                           io_rate_single_thread;
      parallelism++;
      if (parallelism > 192)
      {
        jam();
        parallelism = 192;
      }
      m_prep_max_lcp_pages_outstanding = parallelism;
    }

    DEB_PGMAN_LCP_TIME_STAT(("(%u)Current pageout rate/sec: %llu,"
                             " dirty rate: %llu, "
                             " dirty_rate_since_lcp: %llu, "
                             "flush_rate: %llu, "
                             "available_lcp_pageouts_used: %llu, "
                             "available_lcp_pageouts: %llu, "
                             "number of dirty pages: %llu, "
                             "max_lcp_pages_outstanding: %llu, "
                             "prep_max_lcp_pages_outstanding: %llu, "
                             "millis since last call: %llu",
                             instance(),
                             pageout_rate,
                             dirty_rate,
                             dirty_rate_since_lcp,
                             flush_rate,
                             available_lcp_pageouts_used,
                             m_available_lcp_pageouts,
                             num_dirty_pages,
                             m_max_lcp_pages_outstanding,
                             m_prep_max_lcp_pages_outstanding,
                             millis));
    m_last_track_lcp_speed_call = now;
    start_lcp_loop(signal);
  }
  signal->theData[0] = PgmanContinueB::TRACK_LCP_SPEED_LOOP;
  sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 100, 1);
}

void
Pgman::execSYNC_EXTENT_PAGES_REQ(Signal *signal)
{
  SyncExtentPagesReq *req = (SyncExtentPagesReq*)signal->getDataPtr();
  jamEntry();
  Ptr<Page_entry> ptr;

  ndbrequire(m_extra_pgman || !isNdbMtLqh());
  ndbrequire(m_lcp_table_id == RNIL);
  if (m_sync_extent_pages_ongoing)
  {
    /**
     * We only handle one sync at a time, we cannot be certain that it is
     * ok to piggy back on an ongoing, we could optimise by grouping more
     * than one request if they queue up. However if two come in very close
     * to each other they will simply scan the extent page entries and not
     * finding any dirty pages, so not a big deal to let each run by itself
     * without any optimisation.
     */
    jam();
    sendSignalWithDelay(reference(), GSN_SYNC_EXTENT_PAGES_REQ, signal,
                        1, SyncExtentPagesReq::SignalLength);
    return;
  }
  DEB_PGMAN_LCP(("(%u)SYNC_EXTENT_PAGES_REQ, order: %u, from instance: %u",
                 instance(),
                 req->lcpOrder,
                 refToInstance(req->senderRef)));
  m_sync_extent_order = req->lcpOrder;
  m_sync_extent_pages_ongoing = true;
  m_sync_extent_pages_req = *req;
  m_locked_pages_written = 0;
  if ((m_sync_extent_order == SyncExtentPagesReq::FIRST_LCP ||
       m_sync_extent_order == SyncExtentPagesReq::FIRST_AND_END_LCP) &&
      !m_lcp_ongoing)
  {
    /**
     * We are the extra PGMAN worker responsible to write extent
     * pages and this is the first SYNC_EXTENT_PAGES_REQ with
     * FIRST_LCP order set. Thus it is a start of a new LCP.
     */
    jam();
    lcp_start_point(signal, 0, 0);
    ndbrequire(m_num_ldm_completed_lcp == 0);
  }
  else if (m_sync_extent_order == SyncExtentPagesReq::RESTART_SYNC)
  {
    jam();
    /**
     * We are synchronising extent pages as part of restart.
     */
    ndbrequire(!m_lcp_ongoing);
    lcp_start_point(signal, 0, 0);
    ndbrequire(m_num_ldm_completed_lcp == 0);
  }
  else if (m_sync_extent_order == SyncExtentPagesReq::FIRST_AND_END_LCP)
  {
    jam();
    /**
     * A completely empty LCP, no need to anything, we can skip
     * both LCP start and LCP end.
     */
  }
  else
  {
    ndbrequire(m_sync_extent_order == SyncExtentPagesReq::END_LCP ||
               ((m_sync_extent_order == SyncExtentPagesReq::FIRST_LCP ||
                 m_sync_extent_order ==
                   SyncExtentPagesReq::FIRST_AND_END_LCP) &&
                m_lcp_ongoing));
  }

  Page_sublist& pl = *m_page_sublist[Page_entry::SL_LOCKED];
  if (pl.first(ptr))
  {
    jam();
    m_sync_extent_next_page_entry = ptr.i;
    ndbrequire(m_lcp_ongoing);
    start_lcp_loop(signal);
    return;
  }
  finish_sync_extent_pages(signal);
}

void
Pgman::finish_sync_extent_pages(Signal *signal)
{
  DEB_PGMAN_LCP(("(%u)SYNC_EXTENT_PAGES_CONF to %u",
                 instance(),
                 refToInstance(m_sync_extent_pages_req.senderRef)));
  SyncExtentPagesConf *conf = (SyncExtentPagesConf*)signal->getDataPtr();
  m_sync_extent_pages_ongoing = false;
  m_sync_extent_next_page_entry = RNIL;
  if (m_sync_extent_order == SyncExtentPagesReq::END_LCP ||
      m_sync_extent_order == SyncExtentPagesReq::FIRST_AND_END_LCP ||
      m_sync_extent_order == SyncExtentPagesReq::RESTART_SYNC)
  {
    jam();
    m_num_ldm_completed_lcp++;
    DEB_PGMAN_LCP(("(%u) %u LDMs out of %u completed sync extent",
                   instance(),
                   m_num_ldm_completed_lcp,
                   getNumLDMInstances()));
    if (m_num_ldm_completed_lcp == getNumLDMInstances() ||
        m_sync_extent_order == SyncExtentPagesReq::RESTART_SYNC)
    {
      jam();
      /**
       * We are the extra PGMAN worker and we have completed the
       * last sync of the extent pages in this LCP. We call
       * lcp_end_point to finish up the LCP.
       */
      NDB_TICKS now = getHighResTimer();
      Uint64 lcp_time = NdbTick_Elapsed(m_lcp_start_time,now).milliSec();
      lcp_end_point(Uint32(lcp_time), true, true);
      m_num_ldm_completed_lcp = 0;
    }
  }

  BlockReference ref = m_sync_extent_pages_req.senderRef;
  conf->senderRef = reference();
  conf->senderData = m_sync_extent_pages_req.senderData;
  sendSignal(ref, GSN_SYNC_EXTENT_PAGES_CONF, signal,
             SyncExtentPagesConf::SignalLength, JBB);
  return;
}

void
Pgman::process_lcp_locked(Signal* signal, Ptr<Page_entry> ptr)
{
  Uint32 loopCount = 0;
  Uint32 max_count;
  CRASH_INSERTION(11006);

  if ((max_count = get_num_lcp_pages_to_write(false)) == 0)
  {
    jam();
    DEB_PGMAN_LCP(("(%u) No room to start more page writes",
                   instance()));
    m_sync_extent_next_page_entry = ptr.i;
    return;
  }
  /**
   * Protect from tsman parallel access.
   * These pages are often updated from any of the LDM
   * threads using the tsman lock as protection mechanism.
   * So by locking tsman we ensure that those accesses
   * doesn't conflict with our write of extent pages.
   */
  do
  {
    jam();
    bool break_flag = false;
    {
      Tablespace_client tsman(signal, this, c_tsman, 0, 0, 0, 0);
      jam();
      bool is_file_ready = tsman.is_datafile_ready(ptr.p->m_file_no);
      if (is_file_ready)
      {
        /**
         * An extent page is placed into SL_LOCKED pages before the
         * data file is ready for use. This means that we haven't even
         * initialised the mutexes yet and also not initialised all
         * the extent pages. Avoid checkpointing those pages until
         * the data file is ready.
         */
        tsman.lock_extent_page(ptr.p->m_file_no, ptr.p->m_page_no);
        if ((ptr.p->m_state & Page_entry::DIRTY) &&
            !(ptr.p->m_state & Page_entry::PAGEOUT))
        {
          jam();
          Ptr<GlobalPage> org, copy;
          ndbrequire(m_global_page_pool.seize(copy));
          ndbrequire(m_global_page_pool.getPtr(org, ptr.p->m_real_page_i));
          memcpy(copy.p, org.p, sizeof(GlobalPage));
          ptr.p->m_copy_page_i = copy.i;

          ptr.p->m_state |= Page_entry::LCP;

          DEB_PGMAN_PAGE(("(%u)pageout():extent, page(%u,%u):%u:%x",
                          instance(),
                          ptr.p->m_file_no,
                          ptr.p->m_page_no,
                          ptr.i,
                          (unsigned int)ptr.p->m_state));

          pageout(signal, ptr);
          m_lcp_outstanding++;
          m_current_lcp_pageouts++;
          m_available_lcp_pageouts_used++;
          break_flag = true;
        }
        tsman.unlock_extent_page(ptr.p->m_file_no, ptr.p->m_page_no);
      }
    }
  
    Page_sublist& pl = *m_page_sublist[Page_entry::SL_LOCKED];
    pl.next(ptr);
    if (ptr.i == RNIL)
    {
      if (m_lcp_outstanding == 0)
      {
        jam();
        finish_sync_extent_pages(signal);
        return;
      }
      DEB_PGMAN_LCP(("(%u) %u LCP pages outstanding and extents are done",
                     instance(),
                     m_lcp_outstanding));
      jam();
      m_sync_extent_next_page_entry = RNIL;
      return;
    }
    if (break_flag)
    {
      jam();
      break;
    }
  } while (loopCount++ < 32);
  jam();
  m_sync_extent_next_page_entry = ptr.i;
  start_lcp_loop(signal);
}

void
Pgman::copy_back_page(Ptr<Page_entry> ptr)
{
  Ptr<GlobalPage> org, copy;
  ndbrequire(m_global_page_pool.getPtr(copy, ptr.p->m_copy_page_i));
  ndbrequire(m_global_page_pool.getPtr(org, ptr.p->m_real_page_i));
  memcpy(org.p, copy.p, sizeof(GlobalPage));
  m_global_page_pool.release(copy);
  ptr.p->m_copy_page_i = RNIL;
}

void
Pgman::process_lcp_locked_fswriteconf(Signal* signal, Ptr<Page_entry> ptr)
{
  jam();
  ndbrequire(m_lcp_ongoing);
  /**
   * We have already checked that m_sync_extent_pages_ongoing is true
   * when arriving here. Extent pages are only written during LCPs since
   * they are locked in memory, so there is no need to write them to
   * make space for other pages, only required to write to maintain
   * recoverability.
   *
   * Ensure that Backup block is notified of any progress we make on
   * completing LCPs.
   * Important that this is sent before we send SYNC_EXTENT_PAGES_CONF
   * to ensure Backup block is prepared for receiving the signal.
   */
  m_locked_pages_written++;
  sendSYNC_PAGE_WAIT_REP(signal, false);
  DEB_PGMAN_LCP_EXTRA(("(%u) Written an extent page to disk, "
                       "m_locked_pages_written: %u",
                       instance(),
                       m_locked_pages_written));
  if (!m_lcp_loop_ongoing)
  {
    /* No CONTINUEB outstanding, we can finish sync if done */
    if (m_sync_extent_next_page_entry == RNIL)
    {
      if (m_lcp_outstanding == 0)
      {
        jam();
        finish_sync_extent_pages(signal);
        return;
      }
      DEB_PGMAN_LCP(("(%u) Written all extent pages, but %u"
                     " pages still outstanding",
                     instance(),
                     m_lcp_outstanding));
      jam();
      return;
    }
    jam();
    /* Restart before busy loop to keep up in busy system. */
    check_restart_lcp(signal, true);
  }
  jam();
  return;
}
/* END LCP Module */

// page read and write

void
Pgman::pagein(Signal* signal, Ptr<Page_entry> ptr, EmulatedJamBuffer *jamBuf)
{
  D("pagein");
  D(ptr);

  DEB_PGMAN_PAGE(("(%u)pagein() start: page(%u,%u):%u:%x",
                  instance(),
                  ptr.p->m_file_no,
                  ptr.p->m_page_no,
                  ptr.i,
                  (unsigned int)ptr.p->m_state));

  ndbrequire(! (ptr.p->m_state & Page_entry::PAGEIN));
  set_page_state(jamBuf, ptr, ptr.p->m_state | Page_entry::PAGEIN);

  NDB_TICKS now = NdbTick_getCurrentTicks();
  ptr.p->m_time_tracking = now.getUint64();

  fsreadreq(signal, ptr);
  m_stats.m_current_io_waits++;
}

void
Pgman::fsreadconf(Signal* signal, Ptr<Page_entry> ptr)
{
  D("fsreadconf");
  D(ptr);

  handle_reads_time_tracking(ptr);

  Page_state state = ptr.p->m_state;

  ndbrequire(ptr.p->m_state & Page_entry::PAGEIN);

  state &= ~ Page_entry::PAGEIN;
  state &= ~ Page_entry::EMPTY;
  state |= Page_entry::MAPPED;
  set_page_state(jamBuffer(), ptr, state);

  {
    /**
     * Update lsn record on page
     *   as it can be modified/flushed wo/ update_lsn has been called
     *   (e.g. prealloc) and it then would get lsn 0, which is bad
     *   when running undo and following SR
     */
    Ptr<GlobalPage> pagePtr;
    ndbrequire(m_global_page_pool.getPtr(pagePtr, ptr.p->m_real_page_i));
    File_formats::Datafile::Data_page* page =
      (File_formats::Datafile::Data_page*)pagePtr.p;
    
    Uint64 lsn = page->m_page_header.m_page_lsn_hi;
    lsn <<= 32;
    lsn += page->m_page_header.m_page_lsn_lo;
    ptr.p->m_lsn = lsn;
    Tup_fixsize_page *fix_page = (Tup_fixsize_page*)page;
    (void)fix_page;
    DEB_PGMAN_IO(("(%u)pagein completed: page(%u,%u):%x, "
                  "on_page(%u,%u), tab(%u,%u) lsn(%u,%u)",
                  instance(),
                  ptr.p->m_file_no,
                  ptr.p->m_page_no,
                  (unsigned int)state,
                  fix_page->m_page_no,
                  fix_page->m_file_no,
                  fix_page->m_table_id,
                  fix_page->m_fragment_id,
                  page->m_page_header.m_page_lsn_hi,
                  page->m_page_header.m_page_lsn_lo));

  }
  ndbrequire(m_stats.m_current_io_waits > 0);
  m_stats.m_current_io_waits--;
  m_stats.m_pages_read++;

  /**
   * Calling check_restart_lcp before do_busy_loop ensures that
   * we make progress on LCP even in systems with very high IO
   * read rates.
   */
  check_restart_lcp(signal, false);
  do_busy_loop(signal, true, jamBuffer());
}

void
Pgman::pageout(Signal* signal, Ptr<Page_entry> ptr, bool check_sync_lsn)
{
  D("pageout");
  D(ptr);
  
  Page_state state = ptr.p->m_state;
  ndbrequire(state & Page_entry::BOUND);
  ndbrequire(state & Page_entry::MAPPED);
  ndbrequire(! (state & Page_entry::BUSY));
  ndbrequire(! (state & Page_entry::PAGEOUT));

  state |= Page_entry::PAGEOUT;

  // update lsn on page prior to write
  Ptr<GlobalPage> pagePtr;
  ndbrequire(m_global_page_pool.getPtr(pagePtr, ptr.p->m_real_page_i));
  File_formats::Datafile::Data_page* page =
    (File_formats::Datafile::Data_page*)pagePtr.p;
  page->m_page_header.m_page_lsn_hi = (Uint32)(ptr.p->m_lsn >> 32);
  page->m_page_header.m_page_lsn_lo = (Uint32)(ptr.p->m_lsn & 0xFFFFFFFF);
  Tup_fixsize_page *fix_page = (Tup_fixsize_page*)page;
  (void)fix_page;
  DEB_PGMAN_WRITE(("(%u)pageout(),page(%u,%u),tab(%u,%u),lsn(%u,%u),state:%x",
                   instance(),
                   ptr.p->m_file_no,
                   ptr.p->m_page_no,
                   fix_page->m_table_id,
                   fix_page->m_fragment_id,
                   page->m_page_header.m_page_lsn_hi,
                   page->m_page_header.m_page_lsn_lo,
                   (unsigned int)state));
  int ret = 1;
  if (check_sync_lsn)
  {
    // undo WAL, release LGMAN lock ASAP
    Logfile_client::Request req;
    req.m_callback.m_callbackData = ptr.i;
    req.m_callback.m_callbackIndex = LOGSYNC_CALLBACK;
    D("Logfile_client - pageout");
    Logfile_client lgman(this, c_lgman, RNIL);
    ret = lgman.sync_lsn(signal, ptr.p->m_lsn, &req, 0);
  }
  NDB_TICKS now = NdbTick_getCurrentTicks();
  ptr.p->m_time_tracking = now.getUint64();
  if (ret > 0)
  {
    fswritereq(signal, ptr);
    m_stats.m_current_io_waits++;
  }
  else
  {
    ndbrequire(ret == 0);
    m_log_writes_issued++;
    m_stats.m_log_waits++;
    state |= Page_entry::LOGSYNC;
  }
  set_page_state(jamBuffer(), ptr, state);
}

void
Pgman::add_histogram(Uint64 elapsed_time, Uint64 *histogram)
{
  for (Uint32 i = 0; i < PGMAN_TIME_TRACK_NUM_RANGES; i++)
  {
    if (elapsed_time <= m_time_track_histogram_upper_bound[i])
    {
      histogram[i]++;
      return;
    }
  }
  ndbrequire(false);
  return;
}

void
Pgman::handle_reads_time_tracking(Ptr<Page_entry> ptr)
{
  NDB_TICKS now = NdbTick_getCurrentTicks();
  NDB_TICKS old(ptr.p->m_time_tracking);
  Uint64 elapsed_time = NdbTick_Elapsed(old, now).microSec();
  add_histogram(elapsed_time, &m_time_track_reads[0]);
  m_reads_completed++;
}

void
Pgman::handle_writes_time_tracking(Ptr<Page_entry> ptr)
{
  NDB_TICKS now = NdbTick_getCurrentTicks();
  NDB_TICKS old(ptr.p->m_time_tracking);
  Uint64 elapsed_time = NdbTick_Elapsed(old, now).microSec();
  m_total_write_latency_us += elapsed_time;
  add_histogram(elapsed_time, &m_time_track_writes[0]);
  m_writes_completed++;
  m_tot_writes_completed++;
}

void
Pgman::handle_log_waits_time_tracking(Ptr<Page_entry> ptr)
{
  NDB_TICKS now = NdbTick_getCurrentTicks();
  NDB_TICKS old(ptr.p->m_time_tracking);
  Uint64 elapsed_time = NdbTick_Elapsed(old, now).microSec();
  add_histogram(elapsed_time, &m_time_track_log_waits[0]);
}

void
Pgman::logsync_callback(Signal* signal, Uint32 ptrI, Uint32 res)
{
  Ptr<Page_entry> ptr;
  ndbrequire(m_page_entry_pool.getPtr(ptr, ptrI));

  D("logsync_callback");
  D(ptr);

  handle_log_waits_time_tracking(ptr);

  // it is OK to be "busy" at this point (the commit is queued)
  Page_state state = ptr.p->m_state;
  ndbrequire(state & Page_entry::PAGEOUT);
  ndbrequire(state & Page_entry::LOGSYNC);
  state &= ~ Page_entry::LOGSYNC;
  set_page_state(jamBuffer(), ptr, state);

  NDB_TICKS now = NdbTick_getCurrentTicks();
  ptr.p->m_time_tracking = now.getUint64();
  fswritereq(signal, ptr);
  m_log_writes_completed++;
  m_stats.m_current_io_waits++;
}

void
Pgman::fswriteconf(Signal* signal, Ptr<Page_entry> ptr)
{
  D("fswriteconf");
  D(ptr);

  handle_writes_time_tracking(ptr);

  Page_state state = ptr.p->m_state;

  DEB_PGMAN_IO(("(%u)pageout completed, page(%u,%u):%u:%x",
               instance(),
               ptr.p->m_file_no,
               ptr.p->m_page_no,
               ptr.p->m_real_page_i,
               state));

  ndbrequire(state & Page_entry::PAGEOUT);
  ndbrequire(state & Page_entry::DIRTY);

  if (c_tup != 0)
  {
    jam();
    ndbrequire(!m_extra_pgman);
    c_tup->disk_page_unmap_callback(1, 
                                    ptr.p->m_real_page_i, 
                                    ptr.p->m_dirty_count,
                                    ptr.i);
  }

  if (!m_extra_pgman)
  {
    jam();
    m_num_dirty_pages--;
  }
  state &= ~ Page_entry::PAGEOUT;
  state &= ~ Page_entry::EMPTY;
  state &= ~ Page_entry::DIRTY;

  ndbrequire(m_stats.m_current_io_waits > 0);
  m_stats.m_current_io_waits--;
  remove_fragment_dirty_list(signal, ptr, state);

  if (state & Page_entry::LCP)
  {
    jam();
    state &= ~ Page_entry::LCP;
    ndbrequire(m_lcp_outstanding);
    m_lcp_outstanding--;
    m_stats.m_pages_written_lcp++;
    if (ptr.p->m_copy_page_i != RNIL)
    {
      /**
       * For extent pages we need to keep the page also during pageout.
       * We handle this by copying the page to a copy page at start of
       * the pageout. When the pageout is completed we copy the page
       * back to the real page id and release the copy page. During the
       * pageout is ongoing we will update the copy page (we will return
       * the copy page in all get_page calls during the pageout).
       */
      jam();
      ndbrequire(m_sync_extent_pages_ongoing);
      {
        bool made_dirty = false;
        {
          Tablespace_client tsman(signal, this, c_tsman, 0, 0, 0, 0);
          tsman.lock_extent_page(ptr.p->m_file_no, ptr.p->m_page_no);
          copy_back_page(ptr);
          if (ptr.p->m_dirty_during_pageout)
          {
            jam();
            made_dirty = true;
            ptr.p->m_dirty_during_pageout = false;
            state |= Page_entry::DIRTY;
          }
          set_page_state(jamBuffer(), ptr, state);
          tsman.unlock_extent_page(ptr.p->m_file_no, ptr.p->m_page_no);
        }
        lock_access_extent_page();
        if (made_dirty)
        {
          jam();
          m_tot_pages_made_dirty++;
          m_pages_made_dirty++;
        }
        else
        {
          m_num_dirty_pages--;
        }
        unlock_access_extent_page();
      }
      process_lcp_locked_fswriteconf(signal, ptr);
      do_busy_loop(signal, true, jamBuffer());
      return;
    }
    else
    {
      jam();
      ndbrequire(!m_extra_pgman);
      m_current_lcp_flushes++;
    }
  }
  else if (state & Page_entry::PREP_LCP)
  {
    jam();
    ndbrequire(!m_extra_pgman);
    state &= ~ Page_entry::PREP_LCP;
    ndbrequire(m_prep_lcp_outstanding > 0);
    m_prep_lcp_outstanding--;
    TableRecordPtr tabPtr;
    ndbrequire(m_tableRecordPool.getPtr(tabPtr, ptr.p->m_table_id));
    ndbrequire(tabPtr.p->m_num_prepare_lcp_outstanding > 0);
    tabPtr.p->m_num_prepare_lcp_outstanding--;
    DEB_PGMAN_PREP_PAGE((
                    "(%u)fswriteconf():prepare LCP, page(%u,%u):%u:%x"
                    ", m_prep_lcp_outstanding = %u",
                    instance(),
                    ptr.p->m_file_no,
                    ptr.p->m_page_no,
                    ptr.i,
                    (unsigned int)state,
                    m_prep_lcp_outstanding));
    m_stats.m_pages_written_lcp++;
    m_current_lcp_flushes++;
  }
  else
  {
    jam();
    ndbrequire(!m_extra_pgman);
    m_stats.m_pages_written++;
  }
  
  set_page_state(jamBuffer(), ptr, state);
  /**
   * Calling check_restart_lcp before do_busy_loop ensures that
   * we make progress on LCP even in systems with very high IO
   * read rates.
   */
  check_restart_lcp(signal, true);
  do_busy_loop(signal, true, jamBuffer());
}

// file system interface

void
Pgman::fsreadreq(Signal* signal, Ptr<Page_entry> ptr)
{
  Ptr<File_entry> file_ptr;
  File_map::ConstDataBufferIterator it;
  bool ret = m_file_map.first(it) && m_file_map.next(it, ptr.p->m_file_no);
  ndbrequire(ret);
  Uint32 ptrI = * it.data;
  ndbrequire(m_file_entry_pool.getPtr(file_ptr, ptrI));

  Uint32 fd = file_ptr.p->m_fd;

  ndbrequire(ptr.p->m_page_no > 0);

  m_reads_issued++;

  FsReadWriteReq* req = (FsReadWriteReq*)signal->getDataPtrSend();
  req->filePointer = fd;
  req->userReference = reference();
  req->userPointer = ptr.i;
  req->varIndex = ptr.p->m_page_no;
  req->numberOfPages = 1;
  req->operationFlag = 0;
  FsReadWriteReq::setFormatFlag(req->operationFlag,
				FsReadWriteReq::fsFormatGlobalPage);
  req->data.globalPage.pageNumber = ptr.p->m_real_page_i;
  sendSignal(NDBFS_REF, GSN_FSREADREQ, signal,
	     FsReadWriteReq::FixedLength + 1, JBA);
}

void
Pgman::execFSREADCONF(Signal* signal)
{
  jamEntry();
  FsConf* conf = (FsConf*)signal->getDataPtr();
  Ptr<Page_entry> ptr;
  ndbrequire(m_page_entry_pool.getPtr(ptr, conf->userPointer));

  /**
   * Here is a good place to check checksums written.
   */
  fsreadconf(signal, ptr);
}

void
Pgman::execFSREADREF(Signal* signal)
{
  jamEntry();
  SimulatedBlock::execFSREADREF(signal);
  ndbabort();
}

void
Pgman::fswritereq(Signal* signal, Ptr<Page_entry> ptr)
{
  Ptr<File_entry> file_ptr;
  Ptr<GlobalPage> gptr;
  File_map::ConstDataBufferIterator it;
  ndbrequire(m_file_map.first(it));
  ndbrequire(m_file_map.next(it, ptr.p->m_file_no));
  ndbrequire(m_file_entry_pool.getPtr(file_ptr, *it.data));
  Uint32 fd = file_ptr.p->m_fd;

  /**
   * Before writing the page we need to ensure that we write it
   * using the correct version of the header information.
   * We have to ensure that we write using the correct format,
   * we could write both v1 format and v2 format. If it is v2
   * format we need to ensure that we actually write this format
   * and we also need to mark the page as using the v2 format.
   *
   * This is also a good place to introduce writing of checksums
   * of disk data pages.
   */
  if (file_ptr.p->m_ndb_version >= NDB_DISK_V2)
  {
    gptr.i = ptr.p->m_real_page_i;
    m_global_page_pool.getPtr(gptr);
    File_formats::Page_header *page_header =
      (File_formats::Page_header*)gptr.p;
    if (page_header->m_page_type == File_formats::PT_Tup_fixsize_page)
    {
      Tup_page* tup_page_v2 = (Tup_page*)gptr.p;
      tup_page_v2->m_ndb_version = NDB_DISK_V2;
      tup_page_v2->unused_cluster_page[0] = 0;
      tup_page_v2->unused_cluster_page[1] = 0;
      tup_page_v2->unused_cluster_page[2] = 0;
      tup_page_v2->m_change_map[0] = 0;
      tup_page_v2->m_change_map[1] = 0;
      tup_page_v2->m_change_map[2] = 0;
      tup_page_v2->m_change_map[3] = 0;
    }
    else if (page_header->m_page_type == File_formats::PT_Extent_page)
    {
      File_formats::Datafile::Extent_page_v2 *page_v2 =
        (File_formats::Datafile::Extent_page_v2*)gptr.p;
      page_v2->m_ndb_version = NDB_DISK_V2;
      page_v2->m_checksum = 0;
      page_v2->m_unused[0] = 0;
      page_v2->m_unused[1] = 0;
      page_v2->m_unused[2] = 0;
      page_v2->m_unused[3] = 0;
    }
    else
    {
      ndbabort();
    }
  }

  ndbrequire(ptr.p->m_page_no > 0);

  m_writes_issued++;

  FsReadWriteReq* req = (FsReadWriteReq*)signal->getDataPtrSend();
  req->filePointer = fd;
  req->userReference = reference();
  req->userPointer = ptr.i;
  req->varIndex = ptr.p->m_page_no;
  req->numberOfPages = 1;
  req->operationFlag = 0;
  FsReadWriteReq::setFormatFlag(req->operationFlag,
				FsReadWriteReq::fsFormatGlobalPage);
  req->data.globalPage.pageNumber = ptr.p->m_real_page_i;

  
  if (!ERROR_INSERTED(11008))
  {
    sendSignal(NDBFS_REF, GSN_FSWRITEREQ, signal,
               FsReadWriteReq::FixedLength + 1, JBA);
  }
}

void
Pgman::execFSWRITECONF(Signal* signal)
{
  jamEntry();
  FsConf* conf = (FsConf*)signal->getDataPtr();
  Ptr<Page_entry> ptr;
  ndbrequire(m_page_entry_pool.getPtr(ptr, conf->userPointer));

  fswriteconf(signal, ptr);
}


void
Pgman::execFSWRITEREF(Signal* signal)
{
  jamEntry();
  SimulatedBlock::execFSWRITEREF(signal);
  ndbabort();
}

/**
 * When we perform some operations in the extra PGMAN we do it on
 * behalf of the extent pages. This extra PGMAN block resides in
 * the rep thread block, but the extra PGMAN block is also accessed
 * directly from other threads through TSMAN and through the
 * method get_extent_page. This mutex thus protects the variables:
 * m_num_dirty_pages
 * m_tot_pages_made_dirty
 * m_pages_made_dirty
 */
void
Pgman::lock_access_extent_page()
{
  if (m_extra_pgman)
    NdbMutex_Lock(m_access_extent_page_mutex);
}

void
Pgman::unlock_access_extent_page()
{
  if (m_extra_pgman)
    NdbMutex_Unlock(m_access_extent_page_mutex);
}

// client methods

/**
 * This method is called from the TSMAN block, but the calls may happen
 * from any of the LDM threads and from the REP thread. This function
 * keeps track of the number of dirty pages and update the count of
 * dirty pages to make the calculations of pageout speed correct for
 * the extra PGMAN block. We protect this through a mutex.
 */
Uint32
Pgman::get_extent_page(EmulatedJamBuffer* jamBuf,
                       Signal* signal,
                       Ptr<Page_entry> ptr,
                       Page_request page_req)
{
  thrjam(jamBuf);
  Page_state state = ptr.p->m_state;
  Uint32 req_flags = page_req.m_flags;
  const Page_state LOCKED = Page_entry::LOCKED | Page_entry::MAPPED;
  const Page_state DIRTY = Page_entry::DIRTY;
  ndbrequire((state & LOCKED) == LOCKED);
  if (req_flags & Page_request::COMMIT_REQ)
  {
    thrjam(jamBuf);
    thrjamLine(jamBuf, Uint16(ptr.p->m_file_no));
    thrjamLine(jamBuf, Uint16(ptr.p->m_page_no));
    /**
     * We ignore setting the state to BUSY since this call will always
     * be immediately followed by a call to update_lsn that will remove
     * the busy state if set and thus will also have to update the lists.
     * We want to ensure that the SL_LOCKED list always contains a full
     * list of all LOCKED pages. Thus we don't change the state to BUSY
     * here since that would impact calling set_page_state in update_lsn.
     */
  }

  if (req_flags & DIRTY_FLAGS && ((state & DIRTY) != DIRTY))
  {
    thrjam(jamBuf);
    ptr.p->m_state |= Page_entry::DIRTY;
    lock_access_extent_page();
    m_num_dirty_pages++;
    m_tot_pages_made_dirty++;
    m_pages_made_dirty++;
    unlock_access_extent_page();
  }
  if (ptr.p->m_copy_page_i != RNIL)
  {
    thrjam(jamBuf);
    if (req_flags & DIRTY_FLAGS)
    {
      thrjam(jamBuf);
      ptr.p->m_dirty_during_pageout = true;
    }
    return ptr.p->m_copy_page_i;
  }
  else
  {
    thrjam(jamBuf);
    return ptr.p->m_real_page_i;
  }
}

// client methods
int
Pgman::get_page_no_lirs(EmulatedJamBuffer* jamBuf, Signal* signal,
                        Ptr<Page_entry> ptr, Page_request page_req)
{
  thrjam(jamBuf);

#ifdef VM_TRACE
  Ptr<Page_request> tmp(&page_req, RNIL);

  D(">get_page");
  D(ptr);
  D(tmp);
#endif

  m_get_page_calls_issued++;
  Uint32 req_flags = page_req.m_flags;

  if (req_flags & Page_request::EMPTY_PAGE)
  {
    thrjam(jamBuf);
    // Only one can "init" a page at a time
    //ndbrequire(ptr.p->m_requests.isEmpty());
  }

  Page_state state = ptr.p->m_state;
  bool is_new = (state == 0);
  Uint32 busy_count = 0;
  bool check_overload = false;

  if (req_flags & Page_request::LOCK_PAGE)
  {
    /**
     * Request to read a page locked in page cache, no reason
     * to abort this request.
     */
    thrjam(jamBuf);
    state |= Page_entry::LOCKED;
  }
  
  if (req_flags & Page_request::ALLOC_REQ)
  {
    /**
     * Request to allocate a new page in prepare
     * phase, this request is abortable.
     */
    thrjam(jamBuf);
    check_overload = true;
  }
  else if (req_flags & Page_request::UNDO_REQ ||
           req_flags & Page_request::UNDO_GET_REQ)
  {
    /* UNDOs cannot be aborted. */
    thrjam(jamBuf);
  }
  else if (req_flags & Page_request::ABORT_REQ)
  {
    /* Aborts cannot be aborted, but also perform no commit handling. */
    thrjam(jamBuf);
  }
  else if (req_flags & Page_request::COMMIT_REQ)
  {
    /**
     * Request to commit a change to a page, this request isn't
     * abortable.
     */
    thrjam(jamBuf);
    thrjamLine(jamBuf, Uint16(ptr.p->m_file_no));
    thrjamLine(jamBuf, Uint16(ptr.p->m_page_no));
    busy_count = 1;
    state |= Page_entry::BUSY;
  }
  else if (req_flags & Page_request::COPY_FRAG)
  {
    /**
     * Either a backup scan, a copy fragment scan or a write in a starting
     * node generated by a copy fragment scan. Neither of those operations
     * are abortable.
     */
    thrjam(jamBuf);
  }
  else if (req_flags == 0)
  {
    /**
     * Request as part of a scan in TUP or ACC order, this happens
     * in prepare phase and is abortable.
     */
    thrjam(jamBuf);
    check_overload = true;
  }
  else if (req_flags & Page_request::DISK_SCAN)
  {
    /**
     * Request as part of a scan in disk order, this happens
     * in prepare phase and is abortable.
     */
    thrjam(jamBuf);
    check_overload = true;
  }
  else if ((req_flags & Page_request::OP_MASK) != ZREAD &&
           (req_flags & Page_request::OP_MASK) != ZREAD_EX)
  {
    /**
     * Request as part of write key request of some sort, this
     * happens in prepare phase and is abortable.
     */
    thrjam(jamBuf);
    check_overload = true;
  }
  else if ((req_flags & Page_request::OP_MASK) == ZREAD)
  {
    /**
     * Request as part of read key request, this happens in
     * prepare phase and is abortable.
     */
    thrjam(jamBuf);
    check_overload = true;
  }
  else
  {
    ndbrequire(false);
  }
  if (req_flags & DIRTY_FLAGS &&
      !(ptr.p->m_state & Page_entry::DIRTY))
  {
    if (check_overload && (m_abort_level > 0) && check_overload_error())
    {
      jam();
      /**
       * The disk subsystem is overloaded, we will abort the transaction
       * and report IO overload as the error code.
       * Since continuing here will make page dirty, even if in page cache,
       * the request is aborted since it will later force a disk access to
       * clean the page.
       *
       * It is ok to continue if the page is already dirty, this will not
       * create any additional burden on the disk subsystem.
       */
      DEB_GET_PAGE(("(%u)get_page returns error 1518", instance()));
      return -1518;
    }
  }
  const Page_state LOCKED = Page_entry::LOCKED | Page_entry::MAPPED;
  if ((state & LOCKED) == LOCKED && 
      ! (req_flags & Page_request::UNLOCK_PAGE))
  {
    thrjam(jamBuf);
    if (req_flags & DIRTY_FLAGS)
    {
      /**
       * Here we know that the page is an extent page which is locked.
       * Locked pages are handled globally for LCP and belong to many
       * fragments, so these pages need not be inserted in list of
       * dirty pages per fragment.
       */
      thrjam(jamBuf);
      ptr.p->m_state |= Page_entry::DIRTY;
    }
    m_stats.m_page_requests_direct_return++;
    if (ptr.p->m_copy_page_i != RNIL)
    {
      /**
       * During pageout of a locked page the copy page is the page which
       * is updated and the real page is sent to disk. As soon as the
       * write is done the copy page is copied over to the real page and
       * the copy page is released.
       *
       * In this case we have made the copy page dirty, since the
       * return from the write will clear the DIRTY flag we need to
       * set this flag to ensure that we set the DIRTY flag
       * immediately again after returning from the pageout.
       */
      thrjam(jamBuf);
      D("<get_page: immediate copy_page");
      if (req_flags & DIRTY_FLAGS)
      {
        thrjam(jamBuf);
        ptr.p->m_dirty_during_pageout = true;
      }
      ndbrequire(ptr.p->m_copy_page_i != 0);
      return ptr.p->m_copy_page_i;
    }
    
    D("<get_page: immediate locked");
    ndbrequire(ptr.p->m_real_page_i != 0);
    return ptr.p->m_real_page_i;
  }
  
  bool only_request = ptr.p->m_requests.isEmpty();
#ifdef ERROR_INSERT
  if (req_flags & Page_request::DELAY_REQ)
  {
    thrjam(jamBuf);
    only_request = false;
  }
#endif  
  if (only_request &&
      state & Page_entry::MAPPED)
  {
    thrjam(jamBuf);
    if (! (state & Page_entry::PAGEOUT))
    {
      /**
       * This is an important part of the design!
       * We do not allow to return immediately while a page is
       * in pageout to disk. This means that any page that is
       * in pageout will be temporarily unavailable in the page cache.
       * This ensures that no one writes anything to the page while
       * we are in the process of copying it to the file system buffer.
       *
       * We could remove this limitation for reads if we know that those
       * reads will not do anything apart from reading the page, not a single
       * bit is allowed to be changed in the page for those accesses.
       *
       * We could also allow dirty writing also of other pages, but in this
       * case we would have to copy the page before writing it to disk, we
       * would also need to keep track of the dirty page handling.
       * 
       * With the current implementation we know that the pageout isn't
       * ongoing when we reach here.
       *
       * When the pageout is done we will handle the requests one at a time.
       * This happens through sublist handling. So when the pageout is ongoing
       * the page entry is in the SL_CALLBACK_IO sublist. From this list no
       * entry is leaving. When the pageout is done then we enter the
       * SL_CALLBACK sublist, the SL_CALLBACK list is handled by the
       * process_callback in the order they were entered into this list.
       * If the page is paged out again then the page is again moved to the
       * SL_CALLBACK_IO sublist and thus there is no risk for it to be
       * reported until it is done with the new pageout.
       *
       * There is some special implication for BUSY pages (pages that are
       * locked into the page cache to ensure that we can commit a row
       * or drop a page or delete a row during node restart). These pages
       * are not allowed to pageout when they are in the state BUSY. However
       * we can come here when the page is already in PAGEOUT state. In
       * this case we don't treat the page in any special manner for LCPs.
       *
       * This means that when we call handle_lcp we first check the PAGEOUT
       * state and only after that we check the BUSY state. So in this manner
       * we ensure that the BUSY page isn't first put into a wait state where
       * we wait for the page to be released from the BUSY state (through a
       * call to update_lsn) and then released from the dirty list when the
       * pageout completes. This could cause trouble in knowing when we have
       * completed a fragment LCP and could lead to sending of
       * 2 SYNC_PAGE_CACHE_CONF leading to problems in BACKUP. 
       */
      thrjam(jamBuf);
      if (req_flags & DIRTY_FLAGS)
      {
        thrjam(jamBuf);
	state |= Page_entry::DIRTY;
        insert_fragment_dirty_list(ptr, state, jamBuf);
      }
      
      ptr.p->m_busy_count += busy_count;
      set_page_state(jamBuf, ptr, state);
      
      D("<get_page: immediate");

      ndbrequire(ptr.p->m_real_page_i != RNIL);
      m_stats.m_page_requests_direct_return++;
      return ptr.p->m_real_page_i;
    }
  }

  /**
   * A disk access is required to get the page, we will only perform
   * such an action if we can verify that we should not abort due to
   * overload.
   */
  if (check_overload && (m_abort_level > 0) && check_overload_error())
  {
    jam();
    /**
     * The disk subsystem is overloaded, we will abort the transaction
     * and report IO overload as the error code.
     */
    DEB_GET_PAGE(("(%u)get_page returns 1518(2)", instance()));
    return -1518;
  }

  if (! (req_flags & (Page_request::LOCK_PAGE | Page_request::UNLOCK_PAGE)))
  {
    ndbrequire(! (state & Page_entry::LOCKED));
  }

  // queue the request

  if ((state & Page_entry::MAPPED) && ! (state & Page_entry::PAGEOUT))
  {
    thrjam(jamBuf);
    m_stats.m_page_requests_wait_q++;
    DEB_GET_PAGE(("(%u)Queue get_page(%u,%u), opRec: %u, state: %x,"
                  " req_flags: %x",
                  instance(),
                  ptr.p->m_file_no,
                  ptr.p->m_page_no,
                  page_req.m_callback.m_callbackData,
                  state,
                  req_flags));

  }
  else
  {
    thrjam(jamBuf);
    m_stats.m_page_requests_wait_io++;
    DEB_GET_PAGE(("(%u)IO wait get_page(%u,%u), opRec: %u, state: %x,"
                  " req_flags: %x",
                  instance(),
                  ptr.p->m_file_no,
                  ptr.p->m_page_no,
                  page_req.m_callback.m_callbackData,
                  state,
                  req_flags));
  }

  Ptr<Pgman::Page_request> req_ptr;
  if (likely(m_page_request_pool.seize(req_ptr)))
  {
    Local_page_request_list req_list(m_page_request_pool, ptr.p->m_requests);
    if (! (req_flags & Page_request::ALLOC_REQ))
    {
      thrjam(jamBuf);
      req_list.addLast(req_ptr);
    }
    else
    {
      thrjam(jamBuf);
      req_list.addFirst(req_ptr);
    }
  }
  else
  {
    thrjam(jamBuf);
    if (is_new)
    {
      thrjam(jamBuf);
      release_page_entry(ptr, jamBuf);
      DEB_GET_PAGE(("(%u)Failed to seize page_request for new page",
                     instance()));
    }
    else
    {
      DEB_GET_PAGE(("(%u)Failed to seize page_request for old page",
                    instance()));
    }
    D("<get_page: error out of requests");
    return -1;
  }

  m_get_page_reqs_issued++;
  m_outstanding_dd_requests++;
  req_ptr.p->m_start_time = getHighResTimer();
  req_ptr.p->m_block = page_req.m_block;
  req_ptr.p->m_flags = page_req.m_flags;
  req_ptr.p->m_callback = page_req.m_callback;
#ifdef ERROR_INSERT
  req_ptr.p->m_delay_until_time = page_req.m_delay_until_time;
#endif
  
  state |= Page_entry::REQUEST;
  if (only_request && (req_flags & Page_request::EMPTY_PAGE))
  {
    thrjam(jamBuf);
    state |= Page_entry::EMPTY;
  }

  if (req_flags & Page_request::UNLOCK_PAGE)
  {
    thrjam(jamBuf);
    // keep it locked
  }
  
  ptr.p->m_busy_count += busy_count;
  ptr.p->m_dirty_count += !!(req_flags & DIRTY_FLAGS);
  set_page_state(jamBuf, ptr, state);

  D(req_ptr);
  D("<get_page: queued");
  return 0;
}

int
Pgman::get_page(EmulatedJamBuffer* jamBuf,
                Signal* signal,
                Ptr<Page_entry> ptr,
                Page_request page_req)
{
  int i = get_page_no_lirs(jamBuf, signal, ptr, page_req);
  if (unlikely(i <= (int)-1))
  {
    thrjam(jamBuf);
    return i;
  }

  Uint32 req_flags = page_req.m_flags;
  Page_state state = ptr.p->m_state;

  // update LIRS
  if (! (state & Page_entry::LOCKED) &&
      ! (req_flags & Page_request::CORR_REQ))
  {
    thrjam(jamBuf);
    lirs_reference(jamBuf, ptr);
  }

  // start processing if request was queued
  if (i == 0)
  {
    thrjam(jamBuf);
    do_busy_loop(signal, true, jamBuf);
  }

  return i;
}

/**
 * This method can be called from any thread, for normal pages
 * it is always called from the same thread that the PGMAN instance
 * belongs to, so for these pages there is no risk of interaction.
 * For extent pages the pages are owned by the extra PGMAN block
 * and thus this can be accessed in parallel.
 *
 * To protect the pages in the extra PGMAN block every access to an
 * extent page goes through TSMAN and TSMAN must lock the extent page
 * before accessing it here.
 *
 * Currently calls from TSMAN do not access any block variables in
 * this function. If this is added it must be protected in a proper
 * manner to avoid concurrency issues.
 */
void
Pgman::set_lsn(Ptr<Page_entry> ptr,
               Uint64 lsn)
{
  ptr.p->m_lsn = lsn;
}

void
Pgman::update_lsn(Signal *signal,
                  EmulatedJamBuffer* jamBuf,
                  Ptr<Page_entry> ptr,
                  Uint32 block,
                  Uint64 lsn)
{
  bool busy_lcp = false;
  thrjam(jamBuf);
  D(">update_lsn: block=" << hex << block << dec << " lsn=" << lsn);
  D(ptr);

  Page_state state = ptr.p->m_state;
  ptr.p->m_lsn = lsn;
  
  if (state & Page_entry::BUSY)
  {
    thrjam(jamBuf);
    thrjamLine(jamBuf, Uint16(ptr.p->m_file_no));
    thrjamLine(jamBuf, Uint16(ptr.p->m_page_no));
    ndbrequire(ptr.p->m_busy_count != 0);
    if (--ptr.p->m_busy_count == 0)
    {
      thrjam(jamBuf);
      state &= ~ Page_entry::BUSY;
      if (state & Page_entry::WAIT_LCP)
      {
        thrjam(jamBuf);
        busy_lcp = true;
        state &= ~ Page_entry::WAIT_LCP;
      }
    }
    else
    {
      thrjam(jamBuf);
    }
  }
  
  state |= Page_entry::DIRTY;
  if (! (state & Pgman::Page_entry::LOCKED))
  {
    jam();
    insert_fragment_dirty_list(ptr, state, jamBuf);
  }
  set_page_state(jamBuf, ptr, state);
 
  if (busy_lcp)
  {
    jam();
    /**
     * Should only happen in LDM threads, not in proxy since proxy
     * block only handles LOCKED pages. This is signalled through
     * setting signal object to NULL.
     *
     * LCP handling is signalled as being blocked by this busy page.
     * Now that the page is no longer busy we will see if we can
     * continue with the LCP.
     */
     ndbassert(signal != NULL);
     ndbrequire(ptr.p->m_table_id != RNIL);
     ndbrequire((state & Page_entry::LOCKED) == 0);
     start_lcp_loop(signal);
  }
  D(ptr);
  D("<update_lsn");
}

Uint32
Pgman::create_data_file(Uint32 version)
{
  File_map::DataBufferIterator it;
  Ptr<File_entry> file_ptr;
  if (!m_file_entry_pool.seize(file_ptr))
  {
    D("create_data_file: RNIL (lack of File_entry records)");
    return RNIL;
  }
  file_ptr.p->m_fd = 0;
  file_ptr.p->m_ndb_version = version;
  if(m_file_map.first(it))
  {
    do 
    {
      if(*it.data == RNIL)
      {
        *it.data = file_ptr.i;
        file_ptr.p->m_file_no = it.pos;
        D("create_data_file:" << V(it.pos));
	return it.pos;
      }
    } while(m_file_map.next(it));
  }

  file_ptr.p->m_file_no = m_file_map.getSize();

  if (m_file_map.append(&file_ptr.i, 1))
  {
    D("create_data_file:" << V(file_ptr.p->m_file_no));
    return file_ptr.p->m_file_no;
  }
  m_file_entry_pool.release(file_ptr);
  D("create_data_file: RNIL");
  return RNIL;
}

Uint32
Pgman::alloc_data_file(Uint32 file_no, Uint32 version)
{
  Ptr<File_entry> file_ptr;
  if (!m_file_entry_pool.seize(file_ptr))
  {
    D("alloc_data_file: RNIL (lack of File_entry records)");
    return RNIL;
  }
  Uint32 sz = m_file_map.getSize();
  if (file_no >= sz)
  {
    Uint32 len = file_no - sz + 1;
    Uint32 fd = RNIL;
    while (len--)
    {
      if (! m_file_map.append(&fd, 1))
      {
        D("alloc_data_file: RNIL");
        m_file_entry_pool.release(file_ptr);
	return RNIL;
      }
    }
  }

  File_map::DataBufferIterator it;
  ndbrequire(m_file_map.first(it));
  ndbrequire(m_file_map.next(it, file_no));
  if (* it.data != RNIL)
  {
    D("alloc_data_file: RNIL");
    m_file_entry_pool.release(file_ptr);
    return RNIL;
  }

  *it.data = file_ptr.i;
  file_ptr.p->m_ndb_version = version;
  file_ptr.p->m_file_no = file_no;
  file_ptr.p->m_fd = 0;
  D("alloc_data_file:" << V(file_no));
  return file_no;
}

void
Pgman::map_file_no(Uint32 file_no, Uint32 fd)
{
  Ptr<File_entry> file_ptr;
  File_map::DataBufferIterator it;
  ndbrequire(m_file_map.first(it));
  ndbrequire(m_file_map.next(it, file_no));
  D("map_file_no:" << V(file_no) << V(fd));

  ndbrequire(m_file_entry_pool.getPtr(file_ptr, *it.data));
  ndbassert(file_ptr.p->m_fd == 0);
  file_ptr.p->m_fd = fd;
}

void
Pgman::free_data_file(Uint32 file_no, Uint32 fd)
{
  Ptr<File_entry> file_ptr;
  File_map::DataBufferIterator it;
  ndbrequire(m_file_map.first(it));
  ndbrequire(m_file_map.next(it, file_no));
  ndbrequire(m_file_entry_pool.getPtr(file_ptr, *it.data));
  
  if (fd == RNIL)
  {
    ndbrequire(file_ptr.p->m_fd == 0);
  }
  else
  {
    ndbrequire(file_ptr.p->m_fd == fd);
  }
  m_file_entry_pool.release(file_ptr);
  *it.data = RNIL;
  D("free_data_file:" << V(file_no) << V(fd));
}

void
Pgman::execDATA_FILE_ORD(Signal* signal)
{
  const DataFileOrd* ord = (const DataFileOrd*)signal->getDataPtr();
  Uint32 ret;
  switch (ord->cmd) {
  case DataFileOrd::CreateDataFile:
    ret = create_data_file(ord->version);
    ndbrequire(ret == ord->ret);
    break;
  case DataFileOrd::AllocDataFile:
    ret = alloc_data_file(ord->file_no, ord->version);
    ndbrequire(ret == ord->ret);
    break;
  case DataFileOrd::MapFileNo:
    map_file_no(ord->file_no, ord->fd);
    break;
  case DataFileOrd::FreeDataFile:
    free_data_file(ord->file_no, ord->fd);
    break;
  default:
    ndbabort();
  }
}

int
Pgman::drop_page(Ptr<Page_entry> ptr, EmulatedJamBuffer *jamBuf)
{
  /**
   * When this occurs we have already ensured that there is no activity
   * ongoing on table before arriving here, this includes ensuring that no
   * LCP is ongoing. So we don't need to protect against ongoing LCPs where
   * the LCP is currently waiting for this BUSY page. We do however
   * ensure that the page is removed from the dirty list as part of dropping
   * pages.
   */
  D("drop_page");
  D(ptr);

  Page_stack& pl_stack = m_page_stack;
  Page_queue& pl_queue = m_page_queue;

  Page_state state = ptr.p->m_state;
  Page_state orig_state = state;
  if (! (state & (Page_entry::PAGEIN | Page_entry::PAGEOUT)))
  {
    if (state & Page_entry::ONSTACK)
    {
      thrjam(jamBuf);
      bool at_bottom = ! pl_stack.hasPrev(ptr);
      pl_stack.remove(ptr);
      state &= ~ Page_entry::ONSTACK;
      if (at_bottom)
      {
        thrjam(jamBuf);
        lirs_stack_prune(jamBuf);
      }
      if (state & Page_entry::HOT)
      {
        thrjam(jamBuf);
        state &= ~ Page_entry::HOT;
      }
    }

    if (state & Page_entry::ONQUEUE)
    {
      thrjam(jamBuf);
      pl_queue.remove(ptr);
      state &= ~ Page_entry::ONQUEUE;
    }

    if (state & Page_entry::BUSY)
    {
      thrjam(jamBuf);
      state &= ~ Page_entry::BUSY;
    }

    if (state & Page_entry::DIRTY)
    {
      thrjam(jamBuf);
      lock_access_extent_page();
      m_num_dirty_pages--;
      unlock_access_extent_page();
      state &= ~ Page_entry::DIRTY;
    }

    if (state & Page_entry::EMPTY)
    {
      thrjam(jamBuf);
      state &= ~ Page_entry::EMPTY;
    }

    if (state & Page_entry::MAPPED)
    {
      thrjam(jamBuf);
      state &= ~ Page_entry::MAPPED;
    }

    if (state & Page_entry::BOUND)
    {
      thrjam(jamBuf);
      if (state & Page_entry::LOCKED &&
          m_sync_extent_next_page_entry == ptr.i)
      {
        /**
         * We are dropping a page that is the next page to be handled
         * SYNC_EXTENT_PAGES processing. We need to move the
         * m_sync_extent_next_page_entry reference to the next page
         * in this list.
         */
        thrjam(jamBuf);
        Ptr<Page_entry> drop_page_ptr;
        Page_sublist& pl = *m_page_sublist[Page_entry::SL_LOCKED];
        pl.getPtr(drop_page_ptr, m_sync_extent_next_page_entry);
        pl.next(drop_page_ptr);
        m_sync_extent_next_page_entry = drop_page_ptr.i;
      }
      ndbrequire(ptr.p->m_copy_page_i == RNIL);
      ndbrequire(ptr.p->m_real_page_i != RNIL);
      release_cache_page(ptr.p->m_real_page_i);
      ptr.p->m_real_page_i = RNIL;
      state &= ~ Page_entry::BOUND;
    }

    set_page_state(jamBuf, ptr, state);
    if (ptr.p->m_table_id != RNIL)
    {
      jam();
      /**
       * Ensure we maintain dirty lists until also during drop fragment.
       * This ensures that our checks in various places remains valid.
       */
      remove_fragment_dirty_list(NULL, ptr, orig_state);
    }
    release_page_entry(ptr, jamBuf);
    return 1;
  }
  
  ndbabort();
  return -1;
}

bool
Pgman::extent_pages_available(Uint32 pages_needed)
{
  Uint32 locked_pages = m_stats.m_num_locked_pages;
  Uint32 max_pages = m_param.m_max_pages;

  Uint32 reserved = m_extra_pgman_reserve_pages;
  if (ERROR_INSERTED(11009))
  {
    // 11009 sets max_pages to 25 which is less than reserved 32
    reserved = 0;
  }

  if (m_extra_pgman)
  {
    /**
     * ndbmtd :
     * Extra pgman uses disk page buffer primarily for extent pages.
     * Extent pages are locked in the buffer during a data file's
     * lifetime.
     * In addition, it reserves 'm_extra_pgman_reserve_pages' slots
     * for undo log execution during restart.
     */
    ndbrequire(max_pages > reserved);
    max_pages -= reserved; // Don't use pages reserved for restart
  }
  else
  {
    // ndbd
    max_pages = (Uint32)((NDBD_EXTENT_PAGE_PERCENT * (Uint64)max_pages)/100);
  }

  if ((locked_pages + pages_needed) > max_pages)
  {
    char tmp[90];
    if (m_extra_pgman)
    {
      BaseString::snprintf(tmp, sizeof(tmp),
                           "Reserved pages for restart %u. "
                           "Pages that can be allocated for extent pages %u.",
                           reserved,
                           m_param.m_max_pages - reserved);
    }
    else
    {
      BaseString::snprintf(tmp, sizeof(tmp),
                           "Pages that can be allocated for extent"
                           "pages (25 percent of total pages) %u.",
                           max_pages);
    }

    g_eventLogger->warning("pgman(%u): Cannot allocate %u "
                           "extent pages requested by the "
                           "data file being created. "
                           "Total pages in disk page buffer %u. "
                           "%s "
                           "Already locked pages %u. ",
                           instance(),
                           pages_needed,
                           m_param.m_max_pages,
                           tmp,
                           m_stats.m_num_locked_pages);
    return false;
  }

  return true;
}

void
Pgman::execRELEASE_PAGES_REQ(Signal* signal)
{
  const ReleasePagesReq* req = (const ReleasePagesReq*)signal->getDataPtr();
  const Uint32 senderData = req->senderData;
  const Uint32 senderRef = req->senderRef;
  const Uint32 requestType = req->requestType;
  const Uint32 bucket = req->requestData;
  ndbrequire(req->requestType == ReleasePagesReq::RT_RELEASE_UNLOCKED);

  Page_hashlist& pl_hash = m_page_hashlist;
  Page_hashlist::Iterator iter;
  pl_hash.next(bucket, iter);

  Uint32 loop = 0;
  while (iter.curr.i != RNIL && (loop++ < 8 || iter.bucket == bucket))
  {
    jam();
    Ptr<Page_entry> ptr = iter.curr;
    if (!(ptr.p->m_state & Page_entry::LOCKED) &&
        (ptr.p->m_state & Page_entry::BOUND) &&
        (ptr.p->m_state & Page_entry::MAPPED)) // should be
    {
      jam();
      D(ptr << ": release");
      ndbrequire(!(ptr.p->m_state & Page_entry::REQUEST));
      ndbrequire(!(ptr.p->m_state & Page_entry::EMPTY));
      ndbrequire(!(ptr.p->m_state & Page_entry::DIRTY));
      ndbrequire(!(ptr.p->m_state & Page_entry::BUSY));
      ndbrequire(!(ptr.p->m_state & Page_entry::PAGEIN));
      ndbrequire(!(ptr.p->m_state & Page_entry::PAGEOUT));
      ndbrequire(!(ptr.p->m_state & Page_entry::LOGSYNC));
      drop_page(ptr, jamBuffer());
    }
    pl_hash.next(iter);
  }

  if (iter.curr.i != RNIL) {
    jam();
    ndbassert(iter.bucket > bucket);
    ReleasePagesReq* req = (ReleasePagesReq*)signal->getDataPtrSend();
    req->senderData = senderData;
    req->senderRef = senderRef;
    req->requestType = requestType;
    req->requestData = iter.bucket;
    sendSignal(reference(), GSN_RELEASE_PAGES_REQ,
               signal, ReleasePagesReq::SignalLength, JBB);
    return;
  }
  jam();

  ReleasePagesConf* conf = (ReleasePagesConf*)signal->getDataPtrSend();
  conf->senderData = senderData;
  conf->senderRef = reference();
  sendSignal(senderRef, GSN_RELEASE_PAGES_CONF,
             signal, ReleasePagesConf::SignalLength, JBB);
}

// page cache client

Page_cache_client::Page_cache_client(SimulatedBlock* block,
                                     SimulatedBlock* pgman)
  :m_jamBuf(getThrJamBuf())
{
  m_block = numberToBlock(block->number(), block->instance());

  if (pgman->isNdbMtLqh() && pgman->instance() == 0) {
    m_pgman_proxy = (PgmanProxy*)pgman;
    m_pgman = 0;
  } else {
    m_pgman_proxy = 0;
    m_pgman = (Pgman*)pgman;
  }
}

bool
Page_cache_client::init_page_entry(Request& req)
{
  Ptr<Pgman::Page_entry> ptr;
  bool ok = m_pgman->find_page_entry(ptr,
                                     req.m_page.m_file_no,
                                     req.m_page.m_page_no);
  if (!ok)
    return ok;

  ptr.p->m_table_id = req.m_table_id;
  ptr.p->m_fragment_id = req.m_fragment_id;
  return ok;
}

/**
 * get_page
 * --------
 * get_page is the driving interface to PGMAN. It is the essential
 * interface that drives most of the handling in PGMAN.
 * There is almost nothing happening in the restart handling.
 * The only things startup starts up are the stats loop and the
 * cleanup loop and obviously there aren't that much to get
 * statistics on and not too much to cleanup before pages have
 * started to be updated.
 *
 *
 * Life of a page entry:
 * A page entry starts it life in the pool of page entries.
 * When requesting a new page it is identified by its file number and
 * page number. At start there will be no page entry for this page.
 *
 * So the page entry is seized and the state is 0 at this time.
 * So in get_page_no_lirs in this case the page will be called in
 * set_page_state where it will be put into the SL_BIND sublist.
 *
 * Next step is that the busy loop picks up the entry in the SL_BIND list.
 * This means process_bind will pick it up and bind the page entry to a
 * page in the page cache. This will set the state to BOUND and this means
 * that the entry will be put into the SL_MAP sublist.
 *
 * Next step is that the busy loop picks it up the entry in the SL_MAP list.
 * This means that process_map will pick it up and start a pagein.
 * As part of this it will be moved to the SL_MAP_IO list. In this list it
 * will not create any action.
 *
 * When the pagein completes (fsreadconf) the page entry will have the state
 * BOUND and MAPPED and no PAGEIN or PAGEOUT state. There is still a REQUEST
 * state for the page entry. This means that when the pagein is completed
 * the page entry will be put in the SL_CALLBACK queue.
 *
 * If someone decides to pageout the page before the page is served to the
 * requester, then the page entry will be put into the SL_CALLBACK_IO sublist.
 * In this list it will wait until the pageout is completed.
 *
 * Next step is that the busy loop will pick it up from the SL_CALLBACK list.
 * This means that the process_callback will pick it up and will call the
 * callback, this means that PGMAN will start the code execution in the
 * requester. To ensure this is done properly we will only take one page
 * entry from the SL_CALLBACK queue per signal execution.
 *
 * When the get_page is issued and the page is already in the page cache then
 * we can serve it immediately as long it isn't in pageout at the moment. Also
 * if there are queued requests to the page entry then it will be queued up
 * amongst those requests and will be served one at a time.
 *
 * When a page is requested with either COMMIT_REQ/DIRTY_REQ/ALLOC_REQ then the
 * page will be put into the dirty state after completing the request. We will
 * put it into the fragment dirty list only when we call the callback from the
 * requester.
 *
 * Extent pages are handled in a special manner. They are locked into the page
 * cache. This means that after going through the BOUND and MAPPED state they
 * will never be evicted from the page cache. Once they are paged in they will
 * get the state LOCKED. This means that it is bound in the page cache until
 * the node goes down.
 *
 * There are 4 reasons for starting a pageout:
 * 1) Cleanup loop
 *    Pages that are put into the "queue" (m_page_queue) are part of the cold
 *    pages. See description of those concepts in pgman.hpp. These pages can
 *    be evicted at any time. A cleanup loop goes through the pages in this
 *    queue starting from the oldest entries and moving forward. When it finds
 *    an entry that is DIRTY and not in pagein or pageout handling at the
 *    moment it will pick the page and do a pageout of the page. This will
 *    make the page clean.
 *
 *    An obvious problem that any page cache has is that the oldest part of
 *    the "queue" becomes very clean. So one might have to scan for fairly
 *    long distances before finding a victim for cleanup. This is not a
 *    concern since the only reason for the cleanup loop is to ensure that
 *    we have sufficient amount of clean pages easily available in the
 *    "queue". So therefore we have a configurable amount of pages we check
 *    before we stop (currently hardcoded to 256). We should make this
 *    parameter and the cleanup loop delay configurable to ensure that it
 *    is always possible to control those important parameters of the page
 *    cache.
 *
 * 2) SYNC_PAGE_CACHE_REQ
 *    A specific fragment is required to "clean" all dirty pages by performing
 *    pageout. We keep a list for all dirty pages of a fragment such that it
 *    is easy to go through them and perform a pageout of those pages.
 *    This will ensure that the disk data pages of a fragment can be restored
 *    to the start point of a fragment LCP using the disk page together with
 *    any UNDO log records produced since the start of the LCP.
 *
 * 3) SYNC_EXTENT_PAGES_REQ
 *    As part of each fragment LCP we will also call SYNC_EXTENT_PAGES_REQ.
 *    This will write out all extent pages that are dirty as part of the LCP
 *    processing. Since we don't want to handle request queues for LOCKED
 *    pages we will copy the page data to a copy page and copy this data back
 *    to the page after the pageout is completed.
 *
 *    It is important to also properly handle pages in the PAGEOUT state to
 *    ensure that we don't drop any DIRTY state that happened during the
 *    pageout.
 *
 * 4) DUMP_STATE_ORD
 *    Finally we have the possibility to request a page to be paged out using
 *    a DUMP_STATE_ORD signal. This is intended for testing purposes.
 *
 * At any time that we perform a pageout of a page we will always ensure that
 * the UNDO log is sync up to the LSN of the page. This is called the WAL
 * principle (Write Ahead Logging) and is a fundamental principle used in
 * most database engines dealing with disk pages. Pageout is only performed
 * on dirty pages.
 *
 * A page can also be temporarily locked into memory as part of a COMMIT
 * operation. This happens through a get_page using COMMIT_REQ. This means
 * that the page is locked in the page cache until we have called
 * update_lsn on the page (or we have called drop_page when it is part of
 * drop fragment handling or a delete row during node restart).
 * This temporary locking happens using the BUSY
 * state. In this state the page cannot be paged out. Obviously during the
 * pagein the page will not become dirty, so the only period it stays in
 * the BUSY state for a DIRTY page is from the time it is paged in until
 * the page has its LSN updated which happens after the sync_lsn call. So
 * it can be in the BUSY state waiting for the UNDO log. Actually there is
 * no reason to stop all pageout activity because of one page being in the
 * BUSY state, one can simply move on to the next page in the queue for
 * pageout and ensure that we don't stop the loop.
 * When setting BUSY state we can be in PAGEOUT already, this is treated
 * such that PAGEOUT state is checked first.
 *
 * The following flags are used in get_page:
 * 1) LOCK_PAGE
 *    Used to lock an extent page into the page cache.
 * 2) UNLOCK_PAGE
 *    Used to unlock an extent page from the page cache. We actually keep
 *    it locked even after this request.
 * 3) EMPTY_PAGE
 *    The request is for a new empty page. The page entry might already
 *    exist and might even be part of a different fragment that has been
 *    dropped. This flag is always used in combination with ALLOC_REQ.
 * 4) ALLOC_REQ
 *    We request a page that is not a new page, but it is a new page to the
 *    requester and the page will be written to. So it will make the page
 *    dirty.
 * 5) DIRTY_REQ
 *    The page will be made dirty as part of the request.
 * 6) CORR_REQ
 *    The request is a correlated request, so no LIRS update is done.
 * 7) DELAY_REQ
 *    Only used for testing, ensures that get_page is delayed and no immediate
 *    response is provided.
 * 8) UNDO_REQ
 *    This is a flag that the get_page comes from UNDO log execution. This
 *    means that the table id and fragment id on the page entry isn't yet
 *    correct. It will become correct as part of the execution of the UNDO
 *    log entry.
 * 9) DISK_SCAN
 *    This is a flag used when we are scanning a table in disk data order.
 *    In this case the page might not be initialised when we arrive here.
 *    Thus we ensure that this is an ok condition.
 *
 * The description of the page replacement algorithm is provided in pgman.hpp.
 * The amount of hot pages is 90% of the page cache. Thus the number of cold
 * pages are 10% of the page cache size. The number of unbound
 * entries we can have in the page cache is provided by the config parameter
 * DiskPageBufferEntries. This is given as a multiplier. By default this is
 * set to 10. This means that if we e.g. have 64 MByte of page cache this
 * means we have 2000 pages. For this we will have up to 2000 bound page
 * entries but in addition we will also have 18000 page entries that are
 * unbound. So effectively for a 64 MByte cache we actually maintain a
 * list of the most recent events for 640 Mbyte of page cache by keeping
 * a lot of extra unbound page entries around for a longer time. This is
 * the essence of the LIRS algorithm. Currently 1 page entry uses 88 bytes.
 * So page entry size by default is about 3% of the page cache size.
 * The size of the page cache is provided by the config parameter
 * DiskPageBufferMemory. It is 64 MByte by default.
 *
 * Tablespace objects
 * ------------------
 * Most of the information described here is implemented in the TSMAN block.
 * It is documented here though since it is so closely connected to the
 * get_page interface and the workings of the PGMAN block.
 *
 * Disk data pages are stored in tablespaces. Tablespaces contain one or more
 * data files. Data files contains one or more extents. A fragment allocates
 * pages from a tablespace in chunks called extents.
 *
 * A data file contains a set of data pages as a multiple of the extent size.
 * So if the data file size is 2 GByte and the extent size is 16 MByte then
 * we have 64 extents per file. Each data file has a zero page at the start
 * and then one or more extent pages and then a number of data pages of the
 * extent size. Each extent stores 2 fixed words plus 4 bits per page in the
 * extent (rounded up to a word). So in this example we have 2048 pages and
 * 1026 words of extent information and in total we need to store 65664 words
 * in the extent pages which means that we need 3 extent pages. Thus the
 * true data file size for this page will be 2 GByte + 1 zero page + 3
 * extent pages.
 *
 * Actually all extent information for one extent is always residing in one
 * data page, there can be multiple extents per extent page, but one extent
 * cannot span many pages.
 *
 * When a tablespace is created very little happens, the tablespace is stored
 * in a hash table, it has an id and a version number. The only really
 * interesting information stored about a tablespace is the extent size.
 *
 * When a data file belonging to a tablespace is created a lot more happens.
 * We ask the file system to preallocate the entire data file size to ensure
 * that the disk storage is truly allocated and not just a fake storage
 * is allocated. This means that all pages are also getting a predefined
 * data consisting of all zeroes. This initialisation is handled by NDBFS.
 *
 * At restart the tablespace and the data files are created in a similar
 * fashion with signals arriving from DICT. The only difference is that
 * we now only open the files and need not initialise the data files.
 * We also read the zero page to find out about the file number of the
 * data file as part of a restart.
 *
 * As part of both creating a data file at create time and restart time
 * we will load all extent pages into the page cache. This is done using
 * the LOCK_PAGE flag described above.
 *
 * As part of this create of tablespace and data files all files and all
 * extents are put into the free lists of the tablespace. This applies to
 * initial restarts as well but not to system restart and node restart.
 *
 * For system restart and node restart we will scan all extent pages to
 * reconstruct the free space information of the tablespaces and their
 * extents and even down to the page level.
 *
 * Data file layout
 * (where k is number of extent pages, m is number of extents in file
 *  and n is number of data pages per extent).
 * -----------------------------
 * |    Zero page              |
 * -----------------------------
 * |    Extent page 0          |
 * -----------------------------
 * ...
 * -----------------------------
 * |     Extent page k - 1     |
 * -----------------------------
 * |     Data extent 0         |
 * -----------------------------
 * |     Data extent 1         |
 * -----------------------------
 * ....
 * -----------------------------
 * |     Data extent m - 1     |
 * -----------------------------
 *
 * Data extent layout
 * -----------------------------
 * |     Data page 0           |
 * -----------------------------
 * |     Data page 1           |
 * -----------------------------
 * ......
 * -----------------------------
 * |     Data page n - 1       |
 * -----------------------------
 *
 * A tablespace contains one or more data files that can be of different sizes,
 * but the extent size is always the same. New data files can be added in
 * ALTER commands from the MySQL Server. There can be many tablespaces in a
 * cluster, but a table can only use one tablespace.
 *
 * Free extent handling
 * --------------------
 * We keep two lists of free extents for each data file. We keep a single
 * linked list of extents that are directly available. We also keep a single
 * linked list of extents that have been free'd, but no LCP is yet complete
 * and thus we cannot yet use this extent in any other fragment.
 *
 * Initialisation of data files
 * ----------------------------
 * During data file creation we get a callback into Tsman::execFSWRITEREQ
 * where for each page to write we initialise the page. This means that
 * we will ensure that the zero page gets the proper content, we will
 * ensure that all extent pages are initialised with table id set to
 * RNIL and fragment id set to next free extent in the data file.
 *
 * Scan of extent pages at node/system restart
 * -------------------------------------------
 * During restart we scan the extent pages. For each extent we find that is
 * free we put it into the immediately available free list of extents.
 * For each extent which is not free we get the committed space bits and
 * copy those two bits over to the uncommitted bits. The content of the
 * uncommitted bits of the extent pages is only valid during the time the
 * node is up and running. The content of those bits on disk is not of any
 * interest, only the committed bits are. The reason is that we only write
 * committed information onto the disk pages, the uncommitted bits are used
 * to ensure that we keep track of resources that have been preallocated as
 * part of the PREPARE phase of a transaction.
 *
 * For extents that are allocated it is important to inform also DBTUP about
 * those extents. It is DBTUP that decides where to place the next tuple
 * inserted (updates are always in-place) and it needs at start up to get
 * this information from the extent pages to initialise all of its data
 * structures to maintain knowledge of which extents are available and
 * also the current resource state of each of the data pages.
 *
 * In DBTUP at restart when the extent is to be kept we initialise an
 * extent data structure and we initialise all pages as not free. After that
 * we will loop through all pages in the extent and call
 * Dbtup::disk_restart_page_bits that will get the committed resource state.
 * There are 4 states with higher levels to indicate a more full page.
 * We don't store any specific information about the page here, we only place
 * the extent in the proper place in the matrix of free spaces for extents as
 * described in the VLDB paper from 2005 on Recovery Principles in
 * MySQL Cluster 5.1.
 *
 * We also write the extent pages during restart which will make the extent
 * pages dirty.
 *
 * The scanning of extent pages happens after the UNDO log execution phase.
 *
 * Allocate an extent handling
 * ---------------------------
 * When we allocate an extent we don't UNDO log this, this means that if the
 * node restarts then this extent will still be mapped to the fragment even
 * though it wasn't allocated to the fragment at the LCP this fragment is
 * restored to. This is actually of no concern at all since the only
 * consequence of this is that we will have more extents at the LCP restore
 * point than what we need. Given that the replay of the REDO log and other
 * synchronisation efforts with other nodes is likely to need this extra
 * extent the loss is not necessarily seen at all.
 *
 * Optimisation possibilities for SYNC_EXTENT_PAGES_REQ
 * ----------------------------------------------------
 * However it is important that all changes up to the LCP start point are
 * not lost. Given that we currently only have an UNDO log, this means that
 * we need to synchronize all changes of extent pages as part of each
 * fragment LCP. We can avoid this by introducing some type of REDO log
 * for extent pages. If this is the case we only need to ensure that the
 * REDO log is synched to disk as part of a fragment LCP. So one manner
 * to handle this would be to perform a sync of extent pages at the start
 * of an LCP and then only insert REDO log entries during LCP execution.
 * We also need a synchronisation at the end of the REDO log to ensure
 * that any drop tables performed during the LCP is synchronised to disk
 * before we start reusing the deallocated extents.
 *
 * The really important thing here is that SYNC_EXTENT_PAGES_REQ does
 * ensure that the extent pages as they are at that time are synched to
 * disk. If we employ a REDO log it is essentially a part of restoring
 * the extent pages. Then after that we apply the UNDO log to bring also
 * the page bits in the extent pages back to their correct state.
 * The REDO log needs not be very big at all, it is most likely sufficient
 * with a REDO log of a few pages, something like 512 kByte is quite
 * sufficient. If there is an overflow of this log such that we no longer
 * can write more into it, then we simply convert the SYNC_EXTENT_PAGES_REQ
 * into a write of all extent pages.
 *
 * We can add a flag so that we know if it is the first SYNC_EXTENT_PAGES_REQ
 * which will always write all dirty extent pages. Then there is the last
 * SYNC_EXTENT_PAGES_REQ which also writes all pages and that one will also
 * write the first page of the REDO log to ensure that it is empty. After
 * last and until the next first we need not use the REDO log at all. The
 * REDO log is started from the point where we start the first the execution
 * of the SYNC_EXTENT_PAGES_REQ. So we need a flag to SYNC_EXTENT_PAGES_REQ
 * that specifies if it is the first or if it is the last or if it is an
 * intermediate one. We need not do anything except activate the REDO log
 * in the first, we need only synch the REDO log in an intermediate one.
 * In the last one we first synch all pages and then we finish by writing
 * the empty first REDO log page.
 *
 * If we overflow the REDO log before starting an intermediate
 * SYNC_EXTENT_PAGES_REQ then we empty the REDO log in memory. Then when
 * the SYNC_EXTENT_PAGES_REQ arrive we start by synchronizing all pages
 * to disk, then finally we write the REDO first page which should be
 * empty. We also start writing the REDO log buffer preparing for the
 * next SYNC_EXTENT_PAGES_REQ.
 *
 * In this manner we avoid doing up to thousands of writes of very minor
 * changes to extent pages and instead we write usually just one page
 * to the REDO log. The next SYNC_EXTENT_PAGES_REQ could always write
 * a new page to avoid the risk of destroying the previous LCP. We do
 * however not at all handle disk writes which aren't atomic. This is
 * in general an area for improvement.
 *
 * Free extent handling
 * --------------------
 * We currently don't free any extent even if they get empty.
 * So the only reason to free an extent is drop table. When we drop
 * a table we have already committed the drop table and thus we will
 * complete the drop table even if a crash happens in the middle of the
 * drop table.
 *
 * Reuse of the freed extents from a drop table
 * --------------------------------------------
 * In principle there is nothing stopping reuse of an extent immediately.
 * However to ensure that we have written the extent pages to disk
 * before we reuse it. So we have kept this little deoptimisation where
 * extents are not provided to be allocated until a LCP have completed.
 * Since we now synchronize the information at every fragment LCP we could
 * speed this up and it is even very likely that we should be able to
 * make those extents immediately available.
 *
 * At end of LCP handling we ensure that the free'd extents are put into
 * a linked list of free extents also in the extent pages on disk, these
 * writes dirty the extent pages.
 *
 * Extent page handling
 * --------------------
 * get_page is used to get data pages used by TUP to store rows of data in
 * disk data tables. It is also used to get pages used to store allocation
 * information for those data pages. These pages are called extent pages.
 * When a tablespace is created it is created with a certain extent size.
 * The default extent size is 16 MByte. A table allocates pages from the
 * tablespace one extent at a time. When an extent has been allocated to
 * a table (actually even to a fragment) then no other fragment can get
 * data from this extent.
 *
 * An extent contains the data pages, each extent also contains one or more
 * extent pages that contain allocation information. Each page in the extent
 * have 4 bits of metadata about its free space status. There is also 2 words
 * of fixed information which stores the table id and and fragment id for an
 * allocated extent and the fragment id is a next page pointer within the
 * tablespace that addresses the next free extent in the tablespace. So for a
 * default extent size we have 2048 pages and thus we have 1026 words of
 * extent information which fits nicely in a 32 kByte page.
 *
 * Each time an insert into a disk table is performed we end up calling
 * ALLOC_PAGE_REQ in TSMAN. This finds the first page in the extent that
 * has sufficient space for the new row. If we find a page then we update
 * the uncommitted bits in the extent pages and thus need no write to the
 * extent page on disk yet. If we don't find any page with free space in
 * the extent, then we have to select a new extent and we use an algorithm
 * that attempts to find an extent with as much as free space as possible.
 *
 * Dirty writes of extent pages
 * ----------------------------
 * The following times we make extent pages dirty:
 * 1) In allocating an extent we initialise the extent page information with
 *    table id, fragment id and 0's for all free space information since at
 *    this time all pages are completely free.
 * 2) Scan extent pages during restart, this updates both extent header info
 *    and also all page bits of the extent.
 * 3) Handling free'd extents at end of LCP
 * 4) Free extents during drop table
 * 5) Page bits are updated after a pageout
 *
 * 2) only happens in restart handling and thus have no effect on LCP
 *    execution.
 * 3) happens after the LCP has ended and also doesn't affect the LCP
 *    execution.
 * 4) happens at any time and will affect the LCP execution. It can however
 *    not affect the fragment LCP currently ongoing. The free'd extents are
 *    for sure not belonging to the fragment currently being checkpointed.
 *    Thus it is not necessary to REDO log any writes due to 4). It is
 *    sufficient to make the page dirty and write it out at the end of the
 *    the LCP or write at the beginning of the next LCP.
 *
 * 1) happens during a LCP and it does have an effect on the LCP execution
 *    and it can definitely also affect the currently running fragment LCP.
 *    So this one needs to be REDO logged if that optimisation is used.
 *
 * 5) happens during an LCP and is by far the most common reason to update
 *    the extent pages. So this one is also necessary to reflect in the
 *    possible REDO log for the extent pages.
 *
 * A simple optimisation for 5) is to only make the page dirty and write to
 * the REDO log when the committed bits are changing. We need to still write
 * the uncommitted bits since those are used as long as the node is alive.
 * But for recovery we only need to care about the committed bits.
 *
 * So this means that we only need to update the page bits when moving from
 * one page committed state to another.
 *
 * The following states are possible:
 * 0: The page is 100% free
 * 1: The page has at least 1 free row
 * 2: The page is full
 * 3: Special state also saying page full, mostly used by uncommitted bits
 *
 * We update this every time a data page has completed its write to disk.
 * Since a fragment LCP contains a lot of writes to disk of data pages
 * this means that this is the essential part we write when it comes to
 * extent pages as part of an LCP.
 *
 * So the conclusion is that we need to REDO log an occasional allocation
 * of an extent to a fragment. But by far the most important to REDO log
 * is the changes coming from every time we have written the data pages
 * to disk.
 *
 * Analysis of extent page synchronisation at restart
 * --------------------------------------------------
 * At a restart we will restore a fragment from an LCP that we know have
 * written out all data pages in the page cache at the time of the start of
 * the LCP, a lot of writes have also appeared after the start of the LCP.
 * Given that we UNDO log everything in the data pages before we write them
 * we know that we can still restore the exact state of the data pages at the
 * time of start of the LCP we are restoring.
 *
 * For modifications to the extent pages there are essentially two things
 * we want to ensure. We need to ensure that extents are not lost after
 * being allocated to a fragment. As shown above we know that any extent
 * allocated before the LCP will certainly be part of the recovery since its
 * extent page was written as part of the LCP. We might however have also
 * allocated extents after the start of the LCP, these will remain part of
 * the fragment even after the restart since there is no UNDO of those
 * extent page writes. This is however of no consequence. For extents that
 * are released we can be certain that the table that owned those extents
 * will not try to regain since they were free'd at a time when the drop
 * table was already committed and thus they won't appear in any restarts.
 *
 * We trust that the LCP handling ensures that we don't attempt to use old
 * tables to restore new tables with the same table id and fragment id. It
 * will check that this doesn't happen by verifying that the GCI of the
 * LCP didn't happen before the createGCI of the table.
 *
 * So finally we come to the page free bits in the extent pages.
 * 
 * If a data page wasn't in the page cache at start of the LCP and not
 * thereafter then we know that the page free bits are correct. This is
 * so since we did write them immediately after paging out the data page.
 * This page state information was at the latest written out as part of
 * the LCP we are attempting to restore.
 *
 * So what about pages that were in the page cache at the time of the
 * start of the LCP or pages that were brought into the page cache
 * after the start of the LCP. We know that the page state at the time
 * of the start of the LCP is definitely written to the extent pages
 * since all data pages at start of LCP were written to disk (pageout)
 * before completing the LCP and after that the extent page information
 * was updated and also this was written before completing the LCP.
 *
 * So the only problem we have with those page free bits is that they
 * might have been updated also after completing the LCP. There is no
 * UNDO log information about those changes.
 *
 * So this means that at startup we might have inconsistency between the
 * state of the data page and the page free bits in the extent pages.
 * It is very important to keep those consistent with each other since
 * the entire allocation of rows depend on this information to be
 * correct.
 *
 * So how do we ensure that after a restart we have ensured that this
 * information is consistent. If we can prove that it is correct after
 * a restart then we know that it will be kept consistent by continuously
 * updating this information.
 *
 * OBSERVATION 1:
 * --------------
 * During restart any page that changed its page free bits in the extent
 * pages will also have updated the data page.
 *
 * This means that all of the pages that are updated after the completion of
 * the LCP will also have an UNDO log created before the page was updated.
 * This UNDO log record will always be passed before we have completed the
 * restart.
 *
 * This means that by always calculating the page free bits as part of
 * UNDO log execution we are sure that the page free bits are kept up
 * to date.
 * 
 * OBSERVATION 2:
 * --------------
 * The page free bits are not necessarily up-to-date even if the LSN is.
 *
 * We can as part of recovery execute the UNDO log records, each time we
 * update a page we will also update the page free bits and we will also
 * update the LSN of the data page. During recovery we will then ensure
 * that all data page changes are written to disk whereafter we will ensure
 * that all extent page changes are written to disk.
 *
 * OBSERVATION 3:
 * --------------
 * Given Observation 2, we can conclude that any pages that have changed since
 * the start of the LCP will have an UNDO log record for the corresponding
 * change. So any page that haven't changed since start of this LCP will have
 * the same extent page information as at the start of the LCP. Thus as long
 * as we have checkpointed all dirty extent pages at some point after the
 * start of the LCP we are safe that we can use the UNDO log to synchronize
 * the extent page information with the page information at a restart.
 *
 * OBSERVATION 4:
 * --------------
 * If we write the extent pages after starting the LCP of a specific fragment
 * then it is sufficient to synchronize the extent page information for those
 * pages that have their UNDO log actually executed.
 * If the write of extent pages happened after start of LCP, but before the
 * start of a specific LCP of a fragment, then it is necessary to synchronize
 * also UNDO pages going backwards to the start of the LCP. It is not necessary
 * to perform UNDO action for those pages, it is only needed to pagein the page
 * followed by a check that the extent page information is the same as the
 * information on the page. We currently don't make use of this optimisation,
 * we will rather avoid writing extent pages more than once per LCP by using
 * all UNDO log records to synch the page state with the extent page state.
 *
 * OBSERVATION 5:
 * --------------
 * At LCP of a fragment we still need to synchronize the extent pages where a
 * new extent have been allocated. This is necessary to ensure that all
 * pages used at time of LCP is still allocated to this fragment. If we didn't
 * synch those pages then we're not sure that the extent is allocated to our
 * fragment at recovery. To handle this we will introduce a new flag called
 * DIRTY_EXTENT_HEADER. This is used whenever we allocate an extent as well
 * as when we free an extent at drop fragment.
 *
 * Lemma 1:
 * --------
 * As part of UNDO log execution we need to update the page free bits for
 * every UNDO log record, even when the LSN numbers indicate that they
 * need not be applied.
 *
 * Lemma 2:
 * --------
 * We might optimise things by only syncing the page free bits always after
 * a pagein operation and after applying an UNDO log record. When the
 * page is brought into the page cache as part of UNDO log execution we will
 * synch it, obviously there is no need to do it again and again unless there
 * is a change to the page which only happens when an UNDO log record is
 * performed.
 *
 * Lemma 3:
 * --------
 * Since we use all UNDO log record back to the start of the UNDO log to
 * synhronize the state of the extent pages, this means that it is sufficient
 * to write the extent pages as part of the first fragment LCP, it is not
 * necessary for subsequent fragment LCPs.
 *
 * Lemma 4:
 * --------
 * If we have a crash between flushing the data pages and flushing the
 * extent pages then the extent pages will not be in synch with the
 * data pages. In the next restart those UNDO log records will not be
 * applied towards the data page, so unless we also use this opportunity
 * to write the page free bits we will fail in this case to get the
 * page free bits of the extent pages in synch with the state of the
 * data pages.
 *
 * m_table_id and m_fragment_id has a few special settings:
 * 1) m_table_id == ~0 and m_fragment_id = 0
 *    This setting is used by UNDO requests to fetch page.
 *    It is simply there to avoid being hit by various asserts
 * 2) m_table_id == RNIL and m_fragment_id
 *    This setting is used when accessing extent pages
 * 3) m_table_id == tableid and m_fragment_id == fragmentid
 *    This is the setting used by most normal page access where tableid
 *    and fragmentid is the real table and fragment ids that own the page.
 *
 */
void
Page_cache_client::get_extent_page(Signal* signal,
                                   Request& req,
                                   Uint32 flags)
{
  if (m_pgman_proxy != 0)
  {
    thrjam(m_jamBuf);
    assert(req.m_table_id == RNIL);
    m_pgman_proxy->get_extent_page(*this, signal, req, flags);
    return;
  }
  Ptr<Pgman::Page_entry> entry_ptr;
  Uint32 file_no = req.m_page.m_file_no;
  Uint32 page_no = req.m_page.m_page_no;

  thrjam(m_jamBuf);
  // make sure TUP does not peek at obsolete data
  m_ptr.i = RNIL;
  m_ptr.p = 0;

  // find page entry
  require(m_pgman->find_page_entry(entry_ptr, file_no, page_no));
  require(entry_ptr.p->m_state != 0);
  require(entry_ptr.p->m_table_id == req.m_table_id);
  require(entry_ptr.p->m_fragment_id == req.m_fragment_id);

  Pgman::Page_request page_req;
  page_req.m_block = m_block;
  page_req.m_flags = flags;
  Uint32 page = m_pgman->get_extent_page(m_jamBuf,
                                         signal,
                                         entry_ptr,
                                         page_req);
  require(m_pgman->m_global_page_pool.getPtr(m_ptr, page));
}

int
Page_cache_client::get_page(Signal* signal, Request& req, Uint32 flags)
{
  if (m_pgman_proxy != 0) {
    thrjam(m_jamBuf);
    assert(req.m_table_id == RNIL);
    return m_pgman_proxy->get_page(*this, signal, req, flags);
  }

  Ptr<Pgman::Page_entry> entry_ptr;
  Uint32 file_no = req.m_page.m_file_no;
  Uint32 page_no = req.m_page.m_page_no;

  thrjam(m_jamBuf);
  D("get_page" << V(file_no) << V(page_no) << hex << V(flags));

  // make sure TUP does not peek at obsolete data
  m_ptr.i = RNIL;
  m_ptr.p = 0;

  // find or seize
  bool ok = m_pgman->get_page_entry(m_jamBuf,
                                    entry_ptr,
                                    file_no,
                                    page_no,
                                    req.m_table_id,
                                    req.m_fragment_id,
                                    flags);
  if (! ok)
  {
    thrjam(m_jamBuf);
    return -1;
  }

  Pgman::Page_request page_req;
  page_req.m_block = m_block;
  page_req.m_flags = flags;
  page_req.m_callback = req.m_callback;
#ifdef ERROR_INSERT
  page_req.m_delay_until_time = req.m_delay_until_time;
#endif
  
  int i = m_pgman->get_page(m_jamBuf, signal, entry_ptr, page_req);
  if (i > 0)
  {
    thrjam(m_jamBuf);
    // TODO remove
    require(m_pgman->m_global_page_pool.getPtr(m_ptr, (Uint32)i));
  }
  return i;
}

void
Page_cache_client::set_lsn(Local_key key, Uint64 lsn)
{
  if (m_pgman_proxy != 0) {
    thrjam(m_jamBuf);
    m_pgman_proxy->set_lsn(*this, key, lsn);
    return;
  }
  thrjam(m_jamBuf);

  Ptr<Pgman::Page_entry> entry_ptr;
  Uint32 file_no = key.m_file_no;
  Uint32 page_no = key.m_page_no;

  D("set_lsn" << V(file_no) << V(page_no) << V(lsn));

  bool found = m_pgman->find_page_entry(entry_ptr, file_no, page_no);
  require(found);

  m_pgman->set_lsn(entry_ptr, lsn);
}

void
Page_cache_client::update_lsn(Signal *signal, Local_key key, Uint64 lsn)
{
  if (m_pgman_proxy != 0) {
    thrjam(m_jamBuf);
    m_pgman_proxy->update_lsn(NULL, *this, key, lsn);
    return;
  }
  thrjam(m_jamBuf);

  Ptr<Pgman::Page_entry> entry_ptr;
  Uint32 file_no = key.m_file_no;
  Uint32 page_no = key.m_page_no;

  D("update_lsn" << V(file_no) << V(page_no) << V(lsn));

  bool found = m_pgman->find_page_entry(entry_ptr, file_no, page_no);
  require(found);

  m_pgman->update_lsn(signal, m_jamBuf, entry_ptr, m_block, lsn);
}

int
Page_cache_client::drop_page(Local_key key, Uint32 page_id)
{
  if (m_pgman_proxy != 0) {
    thrjam(m_jamBuf);
    return m_pgman_proxy->drop_page(*this, key, page_id);
  }

  Ptr<Pgman::Page_entry> entry_ptr;
  Uint32 file_no = key.m_file_no;
  Uint32 page_no = key.m_page_no;

  D("drop_page" << V(file_no) << V(page_no));

  bool found = m_pgman->find_page_entry(entry_ptr, file_no, page_no);
  require(found && entry_ptr.p->m_real_page_i == page_id);

  return m_pgman->drop_page(entry_ptr, m_jamBuf);
}

Uint32
Page_cache_client::create_data_file(Signal* signal, Uint32 version)
{
  if (m_pgman_proxy != 0) {
    thrjam(m_jamBuf);
    return m_pgman_proxy->create_data_file(signal, version);
  }
  return m_pgman->create_data_file(version);
}

bool
Page_cache_client::extent_pages_available(Uint32 pages_needed)
{
  if (m_pgman_proxy != 0)
  {
    return m_pgman_proxy->extent_pages_available(pages_needed, *this);
  }
  return m_pgman->extent_pages_available(pages_needed);
}

Uint32
Page_cache_client::alloc_data_file(Signal* signal,
                                   Uint32 file_no,
                                   Uint32 version)
{
  if (m_pgman_proxy != 0) {
    thrjam(m_jamBuf);
    return m_pgman_proxy->alloc_data_file(signal, file_no, version);
  }
  thrjam(m_jamBuf);
  return m_pgman->alloc_data_file(file_no, version);
}

void
Page_cache_client::map_file_no(Signal* signal,
                               Uint32 file_no,
                               Uint32 fd)
{
  if (m_pgman_proxy != 0) {
    thrjam(m_jamBuf);
    m_pgman_proxy->map_file_no(signal, file_no, fd);
    return;
  }
  thrjam(m_jamBuf);
  m_pgman->map_file_no(file_no, fd);
}

void
Page_cache_client::free_data_file(Signal* signal, Uint32 file_no, Uint32 fd)
{
  if (m_pgman_proxy != 0) {
    thrjam(m_jamBuf);
    m_pgman_proxy->free_data_file(signal, file_no, fd);
    return;
  }
  thrjam(m_jamBuf);
  m_pgman->free_data_file(file_no, fd);
}

int
Page_cache_client::add_fragment(Uint32 tableId, Uint32 fragmentId)
{
  assert(m_pgman_proxy == 0);
  return m_pgman->add_fragment(tableId, fragmentId);
}

Pgman::FragmentRecord::FragmentRecord(Pgman &pgman,
                                      Uint32 tableId,
                                      Uint32 fragmentId) :
  m_table_id(tableId),
  m_fragment_id(fragmentId),
  m_current_lcp_dirty_state(Pgman::IN_FIRST_FRAG_DIRTY_LIST)
{
}

int
Pgman::add_fragment(Uint32 tableId, Uint32 fragmentId)
{
  FragmentRecordPtr fragPtr;
  FragmentRecordPtr check;
  if (!m_fragmentRecordPool.seize(fragPtr))
  {
    jam();
    return 1;
  }
  /* Initialise head objects by calling constructor in-place */
  new (fragPtr.p) FragmentRecord(*this, tableId, fragmentId);
  ndbrequire(!m_fragmentRecordHash.find(check, *fragPtr.p));
  m_fragmentRecordHash.add(fragPtr);
  insert_ordered_fragment_list(fragPtr);
  fragPtr.p->m_is_frag_ready_for_prep_lcp_writes = false;
  return 0;
}

void
Pgman::set_table_ready_for_prep_lcp_writes(Uint32 tabPtrI,
                                           bool ready)
{
  TableRecordPtr tabPtr;
  ndbrequire(m_tableRecordPool.getPtr(tabPtr, tabPtrI));
  tabPtr.p->m_is_table_ready_for_prep_lcp_writes = ready;
}

bool
Pgman::is_prep_lcp_writes_outstanding(Uint32 tabPtrI)
{
  TableRecordPtr tabPtr;
  ndbrequire(m_tableRecordPool.getPtr(tabPtr, tabPtrI));
  return tabPtr.p->m_num_prepare_lcp_outstanding != 0;
}

void
Pgman::insert_ordered_fragment_list(FragmentRecordPtr fragPtr)
{
  /**
   * To enable us to know the order of LCPs we keep the fragments
   * in sorted order based on table and fragment id. This insert is a
   * rather heavy operation since we could potentially have 20.000
   * tables and each such table could have up to 8 fragments in the
   * absolute worst case.
   *
   * To avoid serious issues with this we divide the list based on
   * table id and have thus a two-level ordered list, we keep 16 lists
   * with current max of 20320 tables, thus about 1280 tables per list
   * and normally we should not have more than about 2500 fragments per
   * list thus. A list with 2500 fragments can be searched within about
   * 250 microseconds which should be ok since it is a rare event.
   *
   * Splitting the list too much introduces too many gaps that affect
   * Prepare LCP handling negatively, so it is a trade off how many lists
   * to keep.
   */
  Uint32 table_id = fragPtr.p->m_table_id;
  Uint32 fragment_id = fragPtr.p->m_fragment_id;
  Uint32 list = get_ordered_list_from_table_id(table_id);
  FragmentRecordPtr searchFragPtr;
  Local_FragmentRecord_list fragList(m_fragmentRecordPool,
                                     m_fragmentRecordList[list]);
  if (fragList.last(searchFragPtr))
  {
    jam();
    bool found = false;
    while (searchFragPtr.p->m_table_id > table_id ||
           (searchFragPtr.p->m_table_id == table_id &&
           searchFragPtr.p->m_fragment_id > fragment_id))
    {
      jam();
      if (!fragList.prev(searchFragPtr))
      {
        jam();
        found = true;
        fragList.addFirst(fragPtr);
      }
    }
    if (!found)
    {
      jam();
      fragList.insertAfter(fragPtr, searchFragPtr);
    }
  }
  else
  {
    jam();
    fragList.addFirst(fragPtr);
  }
  return;
}

/**
 * The ordered list of fragments is used to process some dirty writes
 * before the actual LCP of the fragments is performed. This will enable
 * a more smooth load on the disk subsystem. This means that the fragment
 * selected is not important for correctness, it is only important for
 * getting the proper load on the disk subsystem.
 */
bool
Pgman::get_next_ordered_fragment(FragmentRecordPtr & fragPtr)
{
  Uint32 table_id = fragPtr.p->m_table_id;
  Uint32 list = get_ordered_list_from_table_id(table_id);
  {
    Local_FragmentRecord_list fragList(m_fragmentRecordPool,
                                       m_fragmentRecordList[list]);
    if (fragList.next(fragPtr))
    {
      jam();
      ndbrequire(fragPtr.p->m_table_id >= table_id);
      return true;
    }
  }
  for (Uint32 i = list + 1; i < NUM_ORDERED_LISTS; i++)
  {
    Local_FragmentRecord_list fragList(m_fragmentRecordPool,
                                       m_fragmentRecordList[i]);
    if (fragList.isEmpty())
    {
      continue;
    }
    jamLine(Uint16(i));
    jam();
    fragList.first(fragPtr);
    if (fragPtr.p->m_table_id < table_id)
    {
      jam();
      /**
       * We skipped to the next list and found a table with a lower
       * table id, this makes it take too much computational power
       * to find the next fragment, so we will skip it for now.
       * It is only used for prepare LCP handling.
       */
      fragPtr.p = 0;
      fragPtr.i = RNIL;
      return false;
    }
    return true;
  }
  jam();
  fragPtr.p = 0;
  fragPtr.i = RNIL;
  return false;
}

bool
Pgman::get_first_ordered_fragment(FragmentRecordPtr & fragPtr)
{
  for (Uint32 i = 0; i < NUM_ORDERED_LISTS; i++)
  {
    Local_FragmentRecord_list fragList(m_fragmentRecordPool,
                                       m_fragmentRecordList[i]);
    if (fragList.isEmpty())
    {
      continue;
    }
    jamLine(Uint16(i));
    jam();
    fragList.first(fragPtr);
    return true;
  }
  jam();
  fragPtr.p = 0;
  fragPtr.i = RNIL;
  return false;
}

Uint32
Pgman::get_ordered_list_from_table_id(Uint32 table_id)
{
  Uint32 divisor = NDB_MAX_TABLES / NUM_ORDERED_LISTS;
  Uint32 list = table_id / divisor;
  return list;
}

void
Page_cache_client::drop_fragment(Uint32 tableId, Uint32 fragmentId)
{
  assert(m_pgman_proxy == 0);
  m_pgman->drop_fragment(tableId, fragmentId);
}

void
Pgman::drop_fragment(Uint32 tableId, Uint32 fragmentId)
{
  FragmentRecord key(*this, tableId, fragmentId);
  FragmentRecordPtr fragPtr;
  m_fragmentRecordHash.find(fragPtr, key);
  TableRecordPtr tabPtr;
  ndbrequire(m_tableRecordPool.getPtr(tabPtr, tableId));
  if (fragPtr.i != RNIL)
  {
    jam();
    Uint32 list = get_ordered_list_from_table_id(tableId);
    Local_FragmentRecord_list fragList(m_fragmentRecordPool,
                                       m_fragmentRecordList[list]);
    fragList.remove(fragPtr);
    m_fragmentRecordHash.remove(fragPtr);
    m_fragmentRecordPool.release(fragPtr);
  }
}

void
Pgman::insert_fragment_dirty_list(Ptr<Page_entry> ptr,
                                  Page_state state,
                                  EmulatedJamBuffer *jamBuf)
{
  /**
   * Locked pages need never be in a fragment dirty list, they are
   * handled separately.
   */
  ndbrequire(! (state & Pgman::Page_entry::LOCKED));

  if (ptr.p->m_dirty_state != Pgman::IN_NO_DIRTY_LIST)
  {
    /**
     * We are already in a dirty list, so no need to insert ourselves
     * into the list again. If we are not in the currently active list
     * it is because we are in the LCP list. We should remain in the
     * LCP list until we have been made not dirty and thus also
     * removed from the dirty list altogether.
     *
     * To ensure that we minimise the risk of having to apply the
     * WAL rule and invoke an extra wait for the page before it is
     * written we always move the page to be the last in the dirty
     * list it is currently residing in. This ensures that all
     * newly written pages are at the end and thus as far away from
     * being written as is possible.
     *
     * Using this scheme we avoid to skip pages due to the WAL rule
     * in handle_lcp. It invokes an extra cost of reorganising the lists.
     * The reason to take this cost is to minimise the latency in accessing
     * pages in the page cache. Adding a wait for a log wait call can have
     * substantial negative effect on the latency of disk operations.
     *
     * We should not be able to come here when the page is in the dirty
     * list pageout list.
     */
    ndbrequire(ptr.p->m_dirty_state == Pgman::IN_FIRST_FRAG_DIRTY_LIST ||
               ptr.p->m_dirty_state == Pgman::IN_SECOND_FRAG_DIRTY_LIST);
    FragmentRecordPtr fragPtr;
    FragmentRecord key(*this, ptr.p->m_table_id, ptr.p->m_fragment_id);
    ndbrequire(m_fragmentRecordHash.find(fragPtr, key));
    if (ptr.p->m_dirty_state == fragPtr.p->m_current_lcp_dirty_state)
    {
      thrjam(jamBuf);
      /* Page is in fragment dirty list */
      LocalPage_dirty_list list(m_page_entry_pool, fragPtr.p->m_dirty_list);
      list.remove(ptr);
      list.addLast(ptr);
    }
    else
    {
      thrjam(jamBuf);
      /* Page is in dirty list currently being written in LCP */
      m_dirty_list_lcp.remove(ptr);
      m_dirty_list_lcp.addLast(ptr);
    }
    return;
  }

  ndbrequire(!m_extra_pgman);
  m_tot_pages_made_dirty++;
  m_pages_made_dirty++;
  m_num_dirty_pages++;

  DEB_PGMAN_EXTRA(("(%u)Insert page(%u,%u):%u:%x into dirty list of tab(%u,%u)"
                   ", dirty_state: %u",
                   instance(),
                   ptr.p->m_file_no,
                   ptr.p->m_page_no,
                   ptr.i,
                   (unsigned int)state,
                   ptr.p->m_table_id,
                   ptr.p->m_fragment_id,
                   ptr.p->m_dirty_state));

  Ptr<GlobalPage> gpage;
  Dbtup::PagePtr pagePtr;
  FragmentRecordPtr fragPtr;
  FragmentRecord key(*this, ptr.p->m_table_id, ptr.p->m_fragment_id);
  ndbrequire(m_fragmentRecordHash.find(fragPtr, key));
  /**
   * Add the page entry as last item in the dirty list.
   * We write starting at first and write towards the last.
   * So by putting it last we ensure that the page will
   * be written not shortly. Writing it shortly would
   * increase the risk of having to apply the WAL rule
   * to force the UNDO log.
   */
  ptr.p->m_dirty_state = fragPtr.p->m_current_lcp_dirty_state;
  {
    LocalPage_dirty_list list(m_page_entry_pool, fragPtr.p->m_dirty_list);
    list.addLast(ptr);
  }
}

void
Pgman::remove_fragment_dirty_list(Signal *signal,
                                  Ptr<Page_entry> ptr,
                                  Page_state state)
{
  if (state & Page_entry::LOCKED)
  {
    /**
     * Locked pages are never in fragment dirty list since they belong to
     * a global pool of extent pages shared by many fragments.
     */
    jam();
    return;
  }
  if (ptr.p->m_dirty_state == Pgman::IN_NO_DIRTY_LIST)
  {
    /**
     * Not in any dirty list, so we need not remove it.
     */
    jam();
    DEB_PGMAN_EXTRA(("(%u)remove_fragment_dirty_list not in any list: "
                     "page:(%u,%u):%u:%x, tab(%u,%u)",
                     instance(),
                     ptr.p->m_file_no,
                     ptr.p->m_page_no,
                     ptr.i,
                     (unsigned int)state,
                     ptr.p->m_table_id,
                     ptr.p->m_fragment_id));
    return;
  }

  FragmentRecordPtr fragPtr;
  FragmentRecord key(*this, ptr.p->m_table_id, ptr.p->m_fragment_id);
  ndbrequire(m_fragmentRecordHash.find(fragPtr, key));

  if (ptr.p->m_dirty_state == Pgman::IN_FIRST_FRAG_DIRTY_LIST ||
      ptr.p->m_dirty_state == Pgman::IN_SECOND_FRAG_DIRTY_LIST)
  {
    /**
     * We are either in Dirty LCP list or in fragment dirty list depent on the
     * state of the fragment. We toggle the state on the fragment for each
     * LCP. We always insert the pages into the current dirty state on the
     * fragment, so when we move the entire list we can effectively change
     * the state of all page entries in the fragment list by writing a new
     * dirty list state on the fragment.
     */
    if (ptr.p->m_dirty_state == fragPtr.p->m_current_lcp_dirty_state)
    {
      jam();

      DEB_PGMAN_EXTRA(("(%u)Remove page page(%u,%u):%u:%x from dirty list"
                       " of tab(%u,%u)",
                       instance(),
                       ptr.p->m_file_no,
                       ptr.p->m_page_no,
                       ptr.i,
                       (unsigned int)state,
                       ptr.p->m_table_id,
                       ptr.p->m_fragment_id));

      LocalPage_dirty_list list(m_page_entry_pool, fragPtr.p->m_dirty_list);
      list.remove(ptr);
    }
    else
    {
      jam();

      DEB_PGMAN_EXTRA(("(%u)Remove page(%u,%u):%u:%x from dirty lcp"
                       " list of tab(%u,%u)",
                       instance(),
                       ptr.p->m_file_no,
                       ptr.p->m_page_no,
                       ptr.i,
                       (unsigned int)state,
                       ptr.p->m_table_id,
                       ptr.p->m_fragment_id));

      m_dirty_list_lcp.remove(ptr);
      sendSYNC_PAGE_WAIT_REP(signal, true);
    }
  }
  else if (ptr.p->m_dirty_state == Pgman::IN_LCP_OUT_LIST)
  {
    jam();
    DEB_PGMAN_EXTRA(("(%u)Remove page(%u,%u):%u:%x from dirty out"
                     " list of tab(%u,%u)",
                     instance(),
                     ptr.p->m_file_no,
                     ptr.p->m_page_no,
                     ptr.i,
                     (unsigned int)state,
                     ptr.p->m_table_id,
                     ptr.p->m_fragment_id));
    m_dirty_list_lcp_out.remove(ptr);
    sendSYNC_PAGE_WAIT_REP(signal, true);
  }
  else
  {
    ndbabort();
    return; /* Silence compiler warning */
  }
  ptr.p->m_dirty_state = Pgman::IN_NO_DIRTY_LIST;
}

// debug

#ifdef VM_TRACE

void
Pgman::verify_page_entry(Ptr<Page_entry> ptr)
{
  Page_stack& pl_stack = m_page_stack;

  Uint32 ptrI = ptr.i;
  Page_state state = ptr.p->m_state;

  bool has_req = state & Page_entry::REQUEST;
  bool has_req2 = ! ptr.p->m_requests.isEmpty();
  ndbrequire(has_req == has_req2 || dump_page_lists(ptrI));

  bool is_bound = state & Page_entry::BOUND;
  bool is_bound2 = ptr.p->m_real_page_i != RNIL;
  ndbrequire(is_bound == is_bound2 || dump_page_lists(ptrI));

  bool is_mapped = state & Page_entry::MAPPED;
  // mapped implies bound
  ndbrequire(! is_mapped || is_bound || dump_page_lists(ptrI));
  // bound is mapped or has open requests
  ndbrequire(! is_bound || is_mapped || has_req || dump_page_lists(ptrI));

  bool on_stack = state & Page_entry::ONSTACK;
  bool is_hot = state & Page_entry::HOT;
  // hot entry must be on stack
  ndbrequire(! is_hot || on_stack || dump_page_lists(ptrI));

  // stack bottom is hot
  bool at_bottom = on_stack && ! pl_stack.hasPrev(ptr);
  ndbrequire(! at_bottom || is_hot || dump_page_lists(ptrI));

  bool on_queue = state & Page_entry::ONQUEUE;
  // hot entry is not on queue
  ndbrequire(! is_hot || ! on_queue || dump_page_lists(ptrI));

  bool is_locked = state & Page_entry::LOCKED;
  bool on_queue2 = ! is_locked && ! is_hot && is_bound;
  ndbrequire(on_queue == on_queue2 || dump_page_lists(ptrI));

  // entries waiting to enter queue
  bool to_queue = ! is_locked && ! is_hot && ! is_bound && has_req;

  // page is about to be released
  bool to_release = (state == 0);

  // page is either LOCKED or under LIRS or about to be released
  bool is_lirs = on_stack || to_queue || on_queue;
  ndbrequire(to_release || is_locked == ! is_lirs || dump_page_lists(ptrI));

  bool pagein = state & Page_entry::PAGEIN;
  bool pageout = state & Page_entry::PAGEOUT;
  // cannot read and write at same time
  ndbrequire(! pagein || ! pageout || dump_page_lists(ptrI));

  Uint32 no = get_sublist_no(state);
  switch (no) {
  case Page_entry::SL_BIND:
    ndbrequire((! pagein && ! pageout) || dump_page_lists(ptrI));
    break;
  case Page_entry::SL_MAP:
    ndbrequire((! pagein && ! pageout) || dump_page_lists(ptrI));
    break;
  case Page_entry::SL_MAP_IO:
    ndbrequire((pagein && ! pageout) || dump_page_lists(ptrI));
    break;
  case Page_entry::SL_CALLBACK:
    ndbrequire((! pagein && ! pageout) || dump_page_lists(ptrI));
    break;
  case Page_entry::SL_CALLBACK_IO:
    ndbrequire((! pagein && pageout) || dump_page_lists(ptrI));
    break;
  case Page_entry::SL_BUSY:
    break;
  case Page_entry::SL_LOCKED:
    break;
  case Page_entry::SL_IDLE:
    break;
  case Page_entry::SL_OTHER:
    break;
  case ZNIL:
    ndbrequire(to_release || dump_page_lists(ptrI));
    break;
  default:
    ndbrequire(false || dump_page_lists(ptrI));
    break;
  }
}

void
Pgman::verify_page_lists()
{
#ifdef VERIFY_PAGE_LISTS
  EmulatedJamBuffer *jamBuf = getThrJamBuf();
  const Stats& stats = m_stats;
  const Param& param = m_param;
  Page_hashlist& pl_hash = m_page_hashlist;
  Page_stack& pl_stack = m_page_stack;
  Page_queue& pl_queue = m_page_queue;
  Ptr<Page_entry> ptr;

  Uint32 is_locked = 0;
  Uint32 is_bound = 0;
  Uint32 is_mapped = 0;
  Uint32 is_hot = 0;
  Uint32 on_stack = 0;
  Uint32 on_queue = 0;
  Uint32 to_queue = 0;

  Page_hashlist::Iterator iter;
  pl_hash.next(0, iter);
  while (iter.curr.i != RNIL)
  {
    thrjam(jamBuf);
    ptr = iter.curr;
    Page_state state = ptr.p->m_state;
    // (state == 0) occurs only within a time-slice
    ndbrequire(state != 0);
    verify_page_entry(ptr);

    if (state & Page_entry::LOCKED)
    {
      thrjam(jamBuf);
      is_locked++;
    }
    if (state & Page_entry::BOUND)
    {
      thrjam(jamBuf);
      is_bound++;
    }
    if (state & Page_entry::MAPPED)
    {
      thrjam(jamBuf);
      is_mapped++;
    }
    if (state & Page_entry::HOT)
    {
      thrjam(jamBuf);
      is_hot++;
    }
    if (state & Page_entry::ONSTACK)
    {
      thrjam(jamBuf);
      on_stack++;
    }
    if (state & Page_entry::ONQUEUE)
    {
      thrjam(jamBuf);
      on_queue++;
    }
    if (! (state & Page_entry::LOCKED) &&
        ! (state & Page_entry::HOT) &&
        (state & Page_entry::REQUEST) &&
        ! (state & Page_entry::BOUND))
    {
      thrjam(jamBuf);
      to_queue++;
    }
    pl_hash.next(iter);
  }

  for (pl_stack.first(ptr); ptr.i != RNIL; pl_stack.next(ptr))
  {
    thrjam(jamBuf);
    Page_state state = ptr.p->m_state;
    ndbrequire(state & Page_entry::ONSTACK || dump_page_lists(ptr.i));
    if (! pl_stack.hasPrev(ptr))
    {
      thrjam(jamBuf);
      ndbrequire(state & Page_entry::HOT || dump_page_lists(ptr.i));
    }
  }

  for (pl_queue.first(ptr); ptr.i != RNIL; pl_queue.next(ptr))
  {
    thrjam(jamBuf);
    Page_state state = ptr.p->m_state;
    ndbrequire(state & Page_entry::ONQUEUE || dump_page_lists(ptr.i));
    ndbrequire(state & Page_entry::BOUND || dump_page_lists(ptr.i));
    ndbrequire(! (state & Page_entry::HOT) || dump_page_lists(ptr.i));
  }

  ndbrequire(is_bound == stats.m_num_pages || dump_page_lists());
  ndbrequire(is_hot == stats.m_num_hot_pages || dump_page_lists());
  ndbrequire(on_stack == pl_stack.getCount() || dump_page_lists());
  ndbrequire(on_queue == pl_queue.getCount() || dump_page_lists());

  Uint32 k;
  Uint32 entry_count = 0;
  char sublist_info[200] = "";
  for (k = 0; k < Page_entry::SUBLIST_COUNT; k++)
  {
    thrjam(jamBuf);
    const Page_sublist& pl = *m_page_sublist[k];
    for (pl.first(ptr); ptr.i != RNIL; pl.next(ptr))
      ndbrequire(get_sublist_no(ptr.p->m_state) == k || dump_page_lists(ptr.i));
    entry_count += pl.getCount();
    sprintf(sublist_info + strlen(sublist_info),
            " %s:%u", get_sublist_name(k), pl.getCount());
  }
  ndbrequire(entry_count == pl_hash.getCount() || dump_page_lists());
  Uint32 hit_pct = 0;
  char hit_pct_str[20];
  if (stats.m_page_hits + stats.m_page_faults != 0)
    hit_pct = 10000 * stats.m_page_hits /
              (stats.m_page_hits + stats.m_page_faults);
  sprintf(hit_pct_str, "%u.%02u", hit_pct / 100, hit_pct % 100);

  D("loop"
    << " stats:" << m_stats_loop_on
    << " busy:" << m_busy_loop_on
    << " cleanup:" << m_cleanup_loop_on);

  D("page"
    << " entries:" << pl_hash.getCount()
    << " pages:" << stats.m_num_pages << "/" << param.m_max_pages
    << " mapped:" << is_mapped
    << " hot:" << is_hot
    << " io:" << stats.m_current_io_waits << "/" << param.m_max_io_waits
    << " hit pct:" << hit_pct_str);

  D("list"
    << " locked:" << is_locked
    << " stack:" << pl_stack.getCount()
    << " queue:" << pl_queue.getCount()
    << " to queue:" << to_queue);

  D(sublist_info);
#endif
}

void
Pgman::verify_all()
{
  Page_sublist& pl_bind = *m_page_sublist[Page_entry::SL_BIND];
  Page_sublist& pl_map = *m_page_sublist[Page_entry::SL_MAP];
  Page_sublist& pl_callback = *m_page_sublist[Page_entry::SL_CALLBACK];

  if (! pl_bind.isEmpty() || ! pl_map.isEmpty() || ! pl_callback.isEmpty())
  {
    ndbrequire(m_busy_loop_on || dump_page_lists());
  }
  verify_page_lists();
}

bool
Pgman::dump_page_lists(Uint32 ptrI)
{
  // use debugOut directly
  debugOut << "PGMAN: page list dump" << endl;
  if (ptrI != RNIL)
    debugOut << "PGMAN: error on PE [" << ptrI << "]" << "\n";

  Page_stack& pl_stack = m_page_stack;
  Page_queue& pl_queue = m_page_queue;
  Ptr<Page_entry> ptr;
  Uint32 n;

  debugOut << "stack:" << "\n";
  n = 0;
  for (pl_stack.first(ptr); ptr.i != RNIL; pl_stack.next(ptr))
    debugOut << n++ << " " << ptr << "\n";

  debugOut << "queue:" << "\n";
  n = 0;
  for (pl_queue.first(ptr); ptr.i != RNIL; pl_queue.next(ptr))
    debugOut << n++ << " " << ptr << "\n";

  Uint32 k;
  for (k = 0; k < Page_entry::SUBLIST_COUNT; k++)
  {
    debugOut << get_sublist_name(k) << ":" << "\n";
    const Page_sublist& pl = *m_page_sublist[k];
    n = 0;
    for (pl.first(ptr); ptr.i != RNIL; pl.next(ptr))
      debugOut << n++ << " " << ptr << "\n";
  }

  debugOut.flushline();
  return false;
}

#endif

const char*
Pgman::get_sublist_name(Uint32 list_no)
{
  switch (list_no) {
  case Page_entry::SL_BIND:
    return "bind";
  case Page_entry::SL_MAP:
    return "map";
  case Page_entry::SL_MAP_IO:
    return "map_io";
  case Page_entry::SL_CALLBACK:
    return "cb";
  case Page_entry::SL_CALLBACK_IO:
    return "cb_io";
  case Page_entry::SL_BUSY:
    return "busy";
  case Page_entry::SL_LOCKED:
    return "locked";
  case Page_entry::SL_IDLE:
    return "idle";
  case Page_entry::SL_OTHER:
    return "other";
  }
  return "?";
}

NdbOut&
operator<<(NdbOut& out, Ptr<Pgman::Page_request> ptr)
{
  const Pgman::Page_request& pr = *ptr.p;
  out << "PR";
  if (ptr.i != RNIL)
    out << " [" << dec << ptr.i << "]";
  out << " block=" << hex << pr.m_block;
  out << " flags=" << hex << pr.m_flags;
  out << "," << dec << (pr.m_flags & Pgman::Page_request::OP_MASK);
  {
    if (pr.m_flags & Pgman::Page_request::LOCK_PAGE)
      out << ",lock_page";
    if (pr.m_flags & Pgman::Page_request::EMPTY_PAGE)
      out << ",empty_page";
    if (pr.m_flags & Pgman::Page_request::ALLOC_REQ)
      out << ",alloc_req";
    if (pr.m_flags & Pgman::Page_request::COMMIT_REQ)
      out << ",commit_req";
    if (pr.m_flags & Pgman::Page_request::ABORT_REQ)
      out << ",abort_req";
    if (pr.m_flags & Pgman::Page_request::UNDO_REQ)
      out << ",undo_req";
    if (pr.m_flags & Pgman::Page_request::UNDO_GET_REQ)
      out << ",undo_get_req";
    if (pr.m_flags & Pgman::Page_request::DIRTY_REQ)
      out << ",dirty_req";
    if (pr.m_flags & Pgman::Page_request::CORR_REQ)
      out << ",corr_req";
    if (pr.m_flags & Pgman::Page_request::DISK_SCAN)
      out << ",disk_scan";
  }
  return out;
}

void
print(EventLogger *logger, Ptr<Pgman::Page_request> ptr)
{
  char logbuf[MAX_LOG_MESSAGE_SIZE];
  logbuf[0] = '\0';
  const Pgman::Page_request &pr = *ptr.p;
  BaseString::snappend(logbuf, MAX_LOG_MESSAGE_SIZE, "PR");
  if (ptr.i != RNIL)
    BaseString::snappend(logbuf, MAX_LOG_MESSAGE_SIZE, " [%u]", ptr.i);

  BaseString::snappend(logbuf, MAX_LOG_MESSAGE_SIZE, " block=%X", pr.m_block);
  BaseString::snappend(logbuf, MAX_LOG_MESSAGE_SIZE, " flags=%X", pr.m_flags);
  BaseString::snappend(logbuf, MAX_LOG_MESSAGE_SIZE,
                       " flags=%d"
                       ",",
                       pr.m_flags & Pgman::Page_request::OP_MASK);
  {
    if (pr.m_flags & Pgman::Page_request::LOCK_PAGE)
      BaseString::snappend(logbuf, MAX_LOG_MESSAGE_SIZE, "lock_page");
    if (pr.m_flags & Pgman::Page_request::EMPTY_PAGE)
      BaseString::snappend(logbuf, MAX_LOG_MESSAGE_SIZE, "empty_page");
    if (pr.m_flags & Pgman::Page_request::ALLOC_REQ)
      BaseString::snappend(logbuf, MAX_LOG_MESSAGE_SIZE, "alloc_req");
    if (pr.m_flags & Pgman::Page_request::COMMIT_REQ)
      BaseString::snappend(logbuf, MAX_LOG_MESSAGE_SIZE, "commit_req");
    if (pr.m_flags & Pgman::Page_request::ABORT_REQ)
      BaseString::snappend(logbuf, MAX_LOG_MESSAGE_SIZE, "abort_req");
    if (pr.m_flags & Pgman::Page_request::UNDO_REQ)
      BaseString::snappend(logbuf, MAX_LOG_MESSAGE_SIZE, "undo_req");
    if (pr.m_flags & Pgman::Page_request::UNDO_GET_REQ)
      BaseString::snappend(logbuf, MAX_LOG_MESSAGE_SIZE, "undo_get_req");
    if (pr.m_flags & Pgman::Page_request::DIRTY_REQ)
      BaseString::snappend(logbuf, MAX_LOG_MESSAGE_SIZE, "dirty_req");
    if (pr.m_flags & Pgman::Page_request::CORR_REQ)
      BaseString::snappend(logbuf, MAX_LOG_MESSAGE_SIZE, "corr_req");
    if (pr.m_flags & Pgman::Page_request::DISK_SCAN)
      BaseString::snappend(logbuf, MAX_LOG_MESSAGE_SIZE, "disk_scan");
  }

  logger->info("%s", logbuf);
}

NdbOut&
operator<<(NdbOut& out, Ptr<Pgman::Page_entry> ptr)
{
  const Pgman::Page_entry& pe = *ptr.p;
  Uint32 list_no = Pgman::get_sublist_no(pe.m_state);
  out << "PE [" << dec << ptr.i << "]";
  out << " state=" << hex << pe.m_state;
  {
    if (pe.m_state & Pgman::Page_entry::REQUEST)
      out << ",request";
    if (pe.m_state & Pgman::Page_entry::EMPTY)
      out << ",empty";
    if (pe.m_state & Pgman::Page_entry::BOUND)
      out << ",bound";
    if (pe.m_state & Pgman::Page_entry::MAPPED)
      out << ",mapped";
    if (pe.m_state & Pgman::Page_entry::DIRTY)
      out << ",dirty";
    if (pe.m_state & Pgman::Page_entry::USED)
      out << ",used";
    if (pe.m_state & Pgman::Page_entry::BUSY)
      out << ",busy";
    if (pe.m_state & Pgman::Page_entry::LOCKED)
      out << ",locked";
    if (pe.m_state & Pgman::Page_entry::PAGEIN)
      out << ",pagein";
    if (pe.m_state & Pgman::Page_entry::PAGEOUT)
      out << ",pageout";
    if (pe.m_state & Pgman::Page_entry::LOGSYNC)
      out << ",logsync";
    if (pe.m_state & Pgman::Page_entry::LCP)
      out << ",lcp";
    if (pe.m_state & Pgman::Page_entry::WAIT_LCP)
      out << ",wait_lcp";
    if (pe.m_state & Pgman::Page_entry::HOT)
      out << ",hot";
    if (pe.m_state & Pgman::Page_entry::ONSTACK)
      out << ",onstack";
    if (pe.m_state & Pgman::Page_entry::ONQUEUE)
      out << ",onqueue";
  }
  out << " list=";
  if (list_no == ZNIL)
    out << "NONE";
  else
  {
    out << dec << list_no;
    out << "," << Pgman::get_sublist_name(list_no);
  }
  out << " diskpage=" << dec << pe.m_file_no << "," << pe.m_page_no;
  if (pe.m_real_page_i == RNIL)
    out << " realpage=RNIL";
  else {
    out << " realpage=" << dec << pe.m_real_page_i;
#ifdef VM_TRACE
    if (pe.m_state & Pgman::Page_entry::MAPPED) {
      Ptr<GlobalPage> gptr;
      require(pe.m_this->m_global_page_pool.getPtr(gptr, pe.m_real_page_i));
      Uint32 hash_result[4];      
      /* NOTE: Assuming "data" is 64 bit aligned as required by 'md5_hash' */
      md5_hash(hash_result,
               (Uint64*)gptr.p->data, sizeof(gptr.p->data)/sizeof(Uint32));
      out.print(" md5=%08x%08x%08x%08x",
                hash_result[0], hash_result[1],
                hash_result[2], hash_result[3]);
    }
#endif
  }
  out << " lsn=" << dec << pe.m_lsn;
  out << " busy_count=" << dec << pe.m_busy_count;
#ifdef VM_TRACE
  {
    Pgman::Page_stack& pl_stack = pe.m_this->m_page_stack;
    if (! pl_stack.hasNext(ptr))
      out << " top";
    if (! pl_stack.hasPrev(ptr))
      out << " bottom";
  }
  {
    Pgman::Local_page_request_list 
      req_list(ptr.p->m_this->m_page_request_pool, ptr.p->m_requests);
    if (! req_list.isEmpty())
    {
      Ptr<Pgman::Page_request> req_ptr;
      out << " req:";
      for (req_list.first(req_ptr); req_ptr.i != RNIL; req_list.next(req_ptr))
      {
        out << " " << req_ptr;
      }
    }
  }
#endif
  return out;
}

void
print(EventLogger *logger, Ptr<Pgman::Page_entry> ptr) {
  const Pgman::Page_entry &pe = *ptr.p;
  char logbuf[MAX_LOG_MESSAGE_SIZE];
  logbuf[0] = '\0';
  Uint32 list_no = Pgman::get_sublist_no(pe.m_state);
  BaseString::snappend(logbuf, MAX_LOG_MESSAGE_SIZE, "PE [%u] state=%X", ptr.i,
                       pe.m_state);
  {
    if (pe.m_state & Pgman::Page_entry::REQUEST)
      BaseString::snappend(logbuf, MAX_LOG_MESSAGE_SIZE, ",request");
    if (pe.m_state & Pgman::Page_entry::EMPTY)
      BaseString::snappend(logbuf, MAX_LOG_MESSAGE_SIZE, ",empty");
    if (pe.m_state & Pgman::Page_entry::BOUND)
      BaseString::snappend(logbuf, MAX_LOG_MESSAGE_SIZE, ",bound");
    if (pe.m_state & Pgman::Page_entry::MAPPED)
      BaseString::snappend(logbuf, MAX_LOG_MESSAGE_SIZE, ",mapped");
    if (pe.m_state & Pgman::Page_entry::DIRTY)
      BaseString::snappend(logbuf, MAX_LOG_MESSAGE_SIZE, ",dirty");
    if (pe.m_state & Pgman::Page_entry::USED)
      BaseString::snappend(logbuf, MAX_LOG_MESSAGE_SIZE, ",used");
    if (pe.m_state & Pgman::Page_entry::BUSY)
      BaseString::snappend(logbuf, MAX_LOG_MESSAGE_SIZE, ",busy");
    if (pe.m_state & Pgman::Page_entry::LOCKED)
      BaseString::snappend(logbuf, MAX_LOG_MESSAGE_SIZE, ",locked");
    if (pe.m_state & Pgman::Page_entry::PAGEIN)
      BaseString::snappend(logbuf, MAX_LOG_MESSAGE_SIZE, ",pagein");
    if (pe.m_state & Pgman::Page_entry::PAGEOUT)
      BaseString::snappend(logbuf, MAX_LOG_MESSAGE_SIZE, ",pageout");
    if (pe.m_state & Pgman::Page_entry::LOGSYNC)
      BaseString::snappend(logbuf, MAX_LOG_MESSAGE_SIZE, ",logsync");
    if (pe.m_state & Pgman::Page_entry::LCP)
      BaseString::snappend(logbuf, MAX_LOG_MESSAGE_SIZE, ",lcp");
    if (pe.m_state & Pgman::Page_entry::WAIT_LCP)
      BaseString::snappend(logbuf, MAX_LOG_MESSAGE_SIZE, ",wait_lcp");
    if (pe.m_state & Pgman::Page_entry::HOT)
      BaseString::snappend(logbuf, MAX_LOG_MESSAGE_SIZE, ",hot");
    if (pe.m_state & Pgman::Page_entry::ONSTACK)
      BaseString::snappend(logbuf, MAX_LOG_MESSAGE_SIZE, ",onstack");
    if (pe.m_state & Pgman::Page_entry::ONQUEUE)
      BaseString::snappend(logbuf, MAX_LOG_MESSAGE_SIZE, ",onqueue");
  }
  BaseString::snappend(logbuf, MAX_LOG_MESSAGE_SIZE, " list=");
  if (list_no == ZNIL)
    BaseString::snappend(logbuf, MAX_LOG_MESSAGE_SIZE, "NONE");
  else {
    BaseString::snappend(logbuf, MAX_LOG_MESSAGE_SIZE, "%u,%s", list_no,
                         Pgman::get_sublist_name(list_no));
  }
  BaseString::snappend(logbuf, MAX_LOG_MESSAGE_SIZE, " diskpage=%u,%u",
                       pe.m_file_no, pe.m_page_no);
  if (pe.m_real_page_i == RNIL)
    BaseString::snappend(logbuf, MAX_LOG_MESSAGE_SIZE, "realpage=RNIL");
  else {
    BaseString::snappend(logbuf, MAX_LOG_MESSAGE_SIZE, " realpage=%u",
                         pe.m_real_page_i);
#ifdef VM_TRACE
    if (pe.m_state & Pgman::Page_entry::MAPPED) {
      Ptr<GlobalPage> gptr;
      require(pe.m_this->m_global_page_pool.getPtr(gptr, pe.m_real_page_i));
      Uint32 hash_result[4];
      /* NOTE: Assuming "data" is 64 bit aligned as required by 'md5_hash' */
      md5_hash(hash_result, (Uint64 *)gptr.p->data,
               sizeof(gptr.p->data) / sizeof(Uint32));
      BaseString::snappend(logbuf, MAX_LOG_MESSAGE_SIZE,
                           " md5=%08x%08x%08x%08x", hash_result[0],
                           hash_result[1], hash_result[2], hash_result[3]);
    }
#endif
  }
  BaseString::snappend(logbuf, MAX_LOG_MESSAGE_SIZE, " lsn=%llu busy_count=%u",
                       pe.m_lsn, pe.m_busy_count);
#ifdef VM_TRACE
  {
    Pgman::Page_stack &pl_stack = pe.m_this->m_page_stack;
    if (!pl_stack.hasNext(ptr))
      BaseString::snappend(logbuf, MAX_LOG_MESSAGE_SIZE, " top");
    if (!pl_stack.hasPrev(ptr))
      BaseString::snappend(logbuf, MAX_LOG_MESSAGE_SIZE, " bottom");
  }
  logger->info("%s", logbuf);
  {
    Pgman::Local_page_request_list req_list(ptr.p->m_this->m_page_request_pool,
                                            ptr.p->m_requests);
    if (!req_list.isEmpty()) {
      Ptr<Pgman::Page_request> req_ptr;
      for (req_list.first(req_ptr); req_ptr.i != RNIL; req_list.next(req_ptr)) {
        print(logger, req_ptr);
      }
    }
  }
#else
  logger->info("%s", logbuf);
#endif
}

void
Pgman::execDUMP_STATE_ORD(Signal* signal)
{
  jamEntry();
#ifdef VM_TRACE
  if (signal->theData[0] == 11000 && signal->getLength() == 2)
  {
    // has no effect currently
    Uint32 flag = signal->theData[1];
    debugFlag = flag & 1;
    debugSummaryFlag = flag & 2;
  }
#endif

  if (signal->theData[0] == 11001)
  {
    // XXX print hash list if no sublist
    Uint32 list = 0;
    if (signal->getLength() > 1)
      list = signal->theData[1];

    if (list >= Page_entry::SUBLIST_COUNT)
    {
      return;
    }

    Page_sublist& pl = *m_page_sublist[list];
    Ptr<Page_entry> ptr;
    
    for (pl.first(ptr); ptr.i != RNIL; pl.next(ptr))
    {
      ndbout << ptr << endl;
      infoEvent(" PE [ file: %d page: %d ] state: %x lsn: %lld busy: %d req-list: %d",
		ptr.p->m_file_no, ptr.p->m_page_no,
		ptr.p->m_state, ptr.p->m_lsn,
		ptr.p->m_busy_count,
		!ptr.p->m_requests.isEmpty());
    }
  }

  if (signal->theData[0] == 11003)
  {
#ifdef VM_TRACE
    verify_page_lists();
    dump_page_lists();
#else
    ndbout << "Only in VM_TRACE builds" << endl;
#endif
  }

  if (signal->theData[0] == 11005)
  {
    g_dbg_lcp = !g_dbg_lcp;
  }

  if (signal->theData[0] == 11006)
  {
    SET_ERROR_INSERT_VALUE(11006);
  }

  if (signal->theData[0] == 11007)
  {
    SET_ERROR_INSERT_VALUE(11007);
  }

  if (signal->theData[0] == 11008)
  {
    SET_ERROR_INSERT_VALUE(11008);
  }

  if (signal->theData[0] == 11009)
  {
    SET_ERROR_INSERT_VALUE(11009);
  }

  if (signal->theData[0] == 11100)
  {
    Uint32 max_pages = m_param.m_max_pages;
    Uint32 size = m_page_entry_pool.getSize();
    Uint32 used = m_page_entry_pool.getUsed();
    Uint32 usedpct = size ? ((100 * used) / size) : 0;
    Uint32 high = m_stats.m_entries_high;
    Uint32 highpct = size ? ((100 * high) / size) : 0;
    Uint32 locked = m_stats.m_num_locked_pages;
    Uint32 reserved = m_extra_pgman_reserve_pages;
    Uint32 lockedpct = size ? ((100 * locked) / size) : 0;
    Uint32 avail_for_extent_pages = (m_extra_pgman) ?
      max_pages - reserved :
      (Uint32)((NDBD_EXTENT_PAGE_PERCENT * (Uint64)max_pages)/100);
    Uint32 lockedpct2 =
      (avail_for_extent_pages > 0) ?
      ((100 * locked) / avail_for_extent_pages) : 0;
    Uint32 lockedpct3 = (max_pages > 0) ? ((100 * locked) / max_pages) : 0;

    g_eventLogger->info(
        "pgman(%u)"
        " page_entry_pool: size %u used: %u (%u %%)"
        " high: %u (%u %%)"
        " locked pages: %u"
        " related to entries %u (%u %%)"
        " related to available pages for extent pages %u (%u %%)"
        " related to Total pages in disk page buffer memory %u (%u %%)",
        instance(), size, used, usedpct, high, highpct, locked, size, lockedpct,
        avail_for_extent_pages, lockedpct2, max_pages, lockedpct3);
  }

  if (signal->theData[0] == 11101)
  {
    int used = m_page_entry_pool.getUsed();
    int high = m_stats.m_entries_high;
    ndbout << "pgman(" << instance() << ")";
    ndbout << " reset entries high: " << high;
    ndbout << " to used: " << used << endl;
    m_stats.m_entries_high = used;
  }
}

bool
Pgman::check_overload_error()
{
  if (m_abort_level > 5)
  {
    jam();
    return true;
  }
  m_abort_counter++;
  if (m_abort_counter % (m_abort_level + 1) == 0)
  {
    jam();
    return false;
  }
  jam();
  return true;

}

void
Pgman::do_calc_stats_loop(Signal *signal)
{
  NDB_TICKS now = NdbTick_getCurrentTicks();
  NDB_TICKS old(m_last_time_calc_stats_loop);
  Uint64 elapsed_ms = NdbTick_Elapsed(old, now).milliSec();
  if (elapsed_ms < 10)
  {
    jam();
    signal->theData[0] = PgmanContinueB::CALC_STATS_LOOP;
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 1000, 1);
    return;
  }
  m_last_time_calc_stats_loop = now.getUint64();

  Uint32 index = m_last_stat_index;
  index++;

  if (index == NUM_STAT_HISTORY)
  {
    jam();
    index = 0;
  }
  m_last_stat_index = index;

  lock_access_extent_page();
  m_pages_made_dirty *= Uint64(1000);
  m_pages_made_dirty /= elapsed_ms;
  m_pages_made_dirty_history[index] = Uint32(m_pages_made_dirty);
  m_pages_made_dirty = Uint64(0);
  unlock_access_extent_page();

  m_reads_completed *= Uint64(1000);
  m_reads_completed /= elapsed_ms;
  m_reads_completed_history[index] = Uint32(m_reads_completed);
  m_reads_completed = Uint64(0);

  m_reads_issued *= Uint64(1000);
  m_reads_issued /= elapsed_ms;
  m_reads_issued_history[index] = Uint32(m_reads_issued);
  m_reads_issued = Uint64(0);

  m_writes_issued *= Uint64(1000);
  m_writes_issued /= elapsed_ms;
  m_writes_issued_history[index] = Uint32(m_writes_issued);
  m_writes_issued = Uint64(0);

  m_writes_completed *= Uint64(1000);
  m_writes_completed /= elapsed_ms;
  m_writes_completed_history[index] = Uint32(m_writes_completed);
  m_writes_completed = Uint64(0);

  m_log_writes_issued *= Uint64(1000);
  m_log_writes_issued /= elapsed_ms;
  m_log_writes_issued_history[index] = Uint32(m_log_writes_issued);
  m_log_writes_issued = Uint64(0);

  m_log_writes_completed *= Uint64(1000);
  m_log_writes_completed /= elapsed_ms;
  m_log_writes_completed_history[index] = Uint32(m_log_writes_completed);
  m_log_writes_completed = Uint64(0);

  m_get_page_calls_issued *= Uint64(1000);
  m_get_page_calls_issued /= elapsed_ms;
  m_get_page_calls_issued_history[index] = Uint32(m_get_page_calls_issued);
  m_get_page_calls_issued = Uint64(0);

  m_get_page_reqs_issued *= Uint64(1000);
  m_get_page_reqs_issued /= elapsed_ms;
  m_get_page_reqs_issued_history[index] = Uint32(m_get_page_reqs_issued);
  m_get_page_reqs_issued = Uint64(0);

  m_get_page_reqs_completed *= Uint64(1000);
  m_get_page_reqs_completed /= elapsed_ms;
  m_get_page_reqs_completed_history[index] = Uint32(m_get_page_reqs_completed);
  m_get_page_reqs_completed = Uint64(0);

  m_stat_time_delay[index] = elapsed_ms;

  m_abort_level = 0;
  m_abort_counter = 0;
  Uint64 dd_latency = 0;
  if (m_num_dd_accesses > Uint64(0))
  {
    jam();
    m_total_dd_latency_us /= Uint64(1000); // Convert to milliseconds
    dd_latency = m_total_dd_latency_us / m_num_dd_accesses;
    m_num_dd_accesses = Uint64(0);
    m_total_dd_latency_us = Uint64(0);
    if (dd_latency >= m_max_dd_latency_ms &&
        m_max_dd_latency_ms > 0)
    {
      jam();
      Uint64 abort_level = dd_latency / m_max_dd_latency_ms;
      m_abort_level = abort_level;
      g_eventLogger->info("Setting DD abort level to %u, dd_latency: %llu",
                          m_abort_level,
                          dd_latency);
    }
  }
  else
  {
    if (m_outstanding_dd_requests > 0)
    {
      DEB_GET_PAGE(("(%u)No outstanding get_page_requests completed this second"
                    ", outstanding: %llu",
                    instance(),
                    m_outstanding_dd_requests));
      if (m_max_dd_latency_ms > 0)
      {
        jam();
        g_eventLogger->info("Setting DD abort level to 1, no completed req");
        m_abort_level = 1;
      }
    }
  }
  signal->theData[0] = PgmanContinueB::CALC_STATS_LOOP;
  sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 1000, 1);
}

void
Pgman::execDBINFO_SCANREQ(Signal *signal)
{
  DbinfoScanReq req= *(DbinfoScanReq*)signal->theData;
  const Ndbinfo::ScanCursor* cursor =
    CAST_CONSTPTR(Ndbinfo::ScanCursor, DbinfoScan::getCursorPtr(&req));
  Ndbinfo::Ratelimit rl;

  jamEntry();
  switch(req.tableId) {
  case Ndbinfo::PGMAN_TIME_TRACK_STATS_TABLEID:
  {
    jam();
    Uint32 start_i = cursor->data[0];
    for (Uint32 i = start_i; i < PGMAN_TIME_TRACK_NUM_RANGES; i++)
    {
      Ndbinfo::Row row(signal, req);
      row.write_uint32(getOwnNodeId());
      row.write_uint32(NDBFS);
      row.write_uint32(instance());   // block instance
      row.write_uint32(m_time_track_histogram_upper_bound[i]);
      row.write_uint64(m_time_track_reads[i]);
      row.write_uint64(m_time_track_writes[i]);
      row.write_uint64(m_time_track_log_waits[i]);
      row.write_uint64(m_time_track_get_page[i]);
      ndbinfo_send_row(signal, req, row, rl);
      if (rl.need_break(req))
      {
        Uint32 save = i + 1;
        jam();
        ndbinfo_send_scan_break(signal, req, rl, save);
        return;
      }
    }
    break;
  }
  case Ndbinfo::DISKPAGEBUFFER_TABLEID:
  {
    jam();
    Ndbinfo::Row row(signal, req);
    row.write_uint32(getOwnNodeId());
    row.write_uint32(instance());   // block instance
    row.write_uint64(m_stats.m_pages_written);
    row.write_uint64(m_stats.m_pages_written_lcp);
    row.write_uint64(m_stats.m_pages_read);
    row.write_uint64(m_stats.m_log_waits);
    row.write_uint64(m_stats.m_page_requests_direct_return);
    row.write_uint64(m_stats.m_page_requests_wait_q);
    row.write_uint64(m_stats.m_page_requests_wait_io);

    ndbinfo_send_row(signal, req, row, rl);
    break;
  }
  case Ndbinfo::DISKSTAT_TABLEID:
  {
    jam();
    Uint32 index = m_last_stat_index;
    Ndbinfo::Row row(signal, req);
    row.write_uint32(getOwnNodeId());
    row.write_uint32(instance());   // block instance
    row.write_uint32(m_pages_made_dirty_history[index]);
    row.write_uint32(m_reads_issued_history[index]);
    row.write_uint32(m_reads_completed_history[index]);
    row.write_uint32(m_writes_issued_history[index]);
    row.write_uint32(m_writes_completed_history[index]);
    row.write_uint32(m_log_writes_issued_history[index]);
    row.write_uint32(m_log_writes_completed_history[index]);
    row.write_uint32(m_get_page_calls_issued_history[index]);
    row.write_uint32(m_get_page_reqs_issued_history[index]);
    row.write_uint32(m_get_page_reqs_completed_history[index]);
    ndbinfo_send_row(signal, req, row, rl);
    break;
  }
  case Ndbinfo::DISKSTATS_1SEC_TABLEID:
  {
    jam();
    Uint32 index = m_last_stat_index;
    for (Uint32 i = 0; i < NUM_STAT_HISTORY; i++)
    {
      Ndbinfo::Row row(signal, req);
      row.write_uint32(getOwnNodeId());
      row.write_uint32(instance());   // block instance
      row.write_uint32(m_pages_made_dirty_history[index]);
      row.write_uint32(m_reads_issued_history[index]);
      row.write_uint32(m_reads_completed_history[index]);
      row.write_uint32(m_writes_issued_history[index]);
      row.write_uint32(m_writes_completed_history[index]);
      row.write_uint32(m_log_writes_issued_history[index]);
      row.write_uint32(m_log_writes_completed_history[index]);
      row.write_uint32(m_get_page_calls_issued_history[index]);
      row.write_uint32(m_get_page_reqs_issued_history[index]);
      row.write_uint32(m_get_page_reqs_completed_history[index]);
      row.write_uint32(i);
      ndbinfo_send_row(signal, req, row, rl);
      index++;
      if (index == NUM_STAT_HISTORY)
        index = 0;
    }
    break;
  }
  default:
    break;
  }
  ndbinfo_send_scan_conf(signal, req, rl);
}
