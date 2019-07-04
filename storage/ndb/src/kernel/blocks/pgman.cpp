/*
   Copyright (c) 2005, 2019, Oracle and/or its affiliates. All rights reserved.

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

extern EventLogger *g_eventLogger;

/**
 * Requests that make page dirty
 */
#define DIRTY_FLAGS (Page_request::COMMIT_REQ | \
                     Page_request::DIRTY_REQ | \
                     Page_request::ALLOC_REQ)

static bool g_dbg_lcp = false;

#ifdef VM_TRACE
//#define DEBUG_PGMAN 1
//#define DEBUG_PGMAN_IO 1
//#define DEBUG_PGMAN_LCP 1
#endif

#ifdef DEBUG_PGMAN
#define DEB_PGMAN(arglist) do { g_eventLogger->info arglist ; } while (0)
#else
#define DEB_PGMAN(arglist) do { } while (0)
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

Pgman::Pgman(Block_context& ctx, Uint32 instanceNumber) :
  SimulatedBlock(PGMAN, ctx, instanceNumber),
  m_fragmentRecordList(m_fragmentRecordPool),
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
  m_lcp_loop_ongoing = false;

  // LCP variables
  m_sync_extent_pages_ongoing = false;
  m_lcp_loop_ongoing = false;
  m_lcp_outstanding = 0;
  m_locked_pages_written = 0;
  m_lcp_table_id = RNIL;
  m_lcp_fragment_id = 0;

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
}

Pgman::~Pgman()
{
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
      ndbout_c("Setting page_cnt = %u", page_cnt);
    }

    m_param.m_max_pages = page_cnt;

    // how many page entries per buffer pages
    Uint32 entries = 0;
    ndb_mgm_get_int_parameter(p, CFG_DB_DISK_PAGE_BUFFER_ENTRIES, &entries);
    ndbout << "pgman: page buffer entries = " << entries << endl;
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
      if (!isNdbMtLqh()) {
        c_tup = (Dbtup*)globalData.getBlock(DBTUP);
      } else if (instance() <= getLqhWorkers()) {
        c_tup = (Dbtup*)globalData.getBlock(DBTUP, instance());
        ndbrequire(c_tup != 0);
      } else {
        // extra worker
        c_tup = 0;
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
    check_restart_lcp(signal);
    return;
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
      ndbrequire(old_list_no != ZNIL);
      if (old_list_no != new_list_no)
      {
        thrjam(jamBuf);
        Page_sublist& old_list = *m_page_sublist[old_list_no];
        old_list.remove(ptr);
      }
    }
    if (new_state != 0)
    {
      thrjam(jamBuf);
      ndbrequire(new_list_no != ZNIL);
      if (old_list_no != new_list_no)
      {
        thrjam(jamBuf);
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
  D("do_stats_loop");
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
  D("do_cleanup_loop");
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
  D(">process_cleanup");
  Page_queue& pl_queue = m_page_queue;

  // XXX for now start always from beginning
  m_cleanup_ptr.i = RNIL;

  if (m_cleanup_ptr.i == RNIL && ! pl_queue.first(m_cleanup_ptr))
  {
    jam();
    D("<process_cleanup: empty queue");
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
                                        ptr.p->m_dirty_count);
      DEB_PGMAN(("(%u)pageout():cleanup, page(%u,%u):%u:%x",
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
  req->lcpOrder = SyncExtentPagesReq::FIRST_LCP;
  sendSignal(reference(), GSN_SYNC_EXTENT_PAGES_REQ, signal,
             SyncExtentPagesReq::SignalLength, JBA);
}

void
Pgman::sendEND_LCPCONF(Signal *signal)
{
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
   * disk. We do this by synching each fragment, one by one and
   * for the extra PGMAN worker it means that we synchronize the
   * extent pages.
   */
  FragmentRecordPtr fragPtr;
  m_end_lcp_req = *req;
  m_fragmentRecordList.first(fragPtr);
  if (fragPtr.i == RNIL)
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
  m_fragmentRecordList.next(fragPtr);
  if (fragPtr.i == RNIL)
  {
    if (isNdbMtLqh())
    {
      jam();
      DEB_PGMAN_LCP(("sendEND_LCPCONF: instance(): %u", instance()));
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
  DEB_PGMAN_LCP(("sendEND_LCPCONF: instance(): %u", instance()));
  sendEND_LCPCONF(signal);
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
  ndbrequire(!m_lcp_loop_ongoing);
  ndbrequire(m_lcp_outstanding == 0);
  ndbrequire(!m_extra_pgman);
  ndbrequire(m_lcp_table_id == RNIL);

  DEB_PGMAN_LCP(("execSYNC_PAGE_CACHE_REQ: instance(): %u", instance()));
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
  DEB_PGMAN(("(%u)Move page_entries from dirty list to lcp list of tab(%u,%u)"
             ", list is %s",
             instance(),
             m_lcp_table_id,
             m_lcp_fragment_id,
             fragPtr.p->m_dirty_list.isEmpty() ?
               "empty" : "not empty"));
  ndbrequire(m_dirty_list_lcp.isEmpty());
  m_dirty_list_lcp.swapList(fragPtr.p->m_dirty_list);
  check_restart_lcp(signal);
}

void
Pgman::finish_lcp(Signal *signal,
                  FragmentRecord *fragPtrP)
{
  SyncPageCacheConf* conf = (SyncPageCacheConf*)signal->getDataPtr();
  conf->senderData = m_sync_page_cache_req.senderData;
  conf->tableId = m_sync_page_cache_req.tableId;
  conf->fragmentId = m_sync_page_cache_req.fragmentId;
  conf->diskDataExistFlag = fragPtrP == NULL ? 0 : 1;
  ndbrequire(m_lcp_outstanding == 0);
  ndbrequire(!m_lcp_loop_ongoing);
  m_lcp_table_id = RNIL;
  m_lcp_fragment_id = 0;
  ndbrequire(m_dirty_list_lcp.isEmpty());
  ndbrequire(m_dirty_list_lcp_out.isEmpty());
  DEB_PGMAN(("(%u)finish_lcp tab(%u,%u), ref: %x",
             instance(),
             m_sync_page_cache_req.tableId,
             m_sync_page_cache_req.fragmentId,
             m_sync_page_cache_req.senderRef));
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

void
Pgman::check_restart_lcp(Signal *signal)
{
  if (m_lcp_loop_ongoing)
  {
    jam();
    /**
     * CONTINUEB(LCP_LOOP) signal is outstanding, no need to
     * do anything more here. We don't want to complete the
     * LCPs with outstanding CONTINUEB signals.
     */
    return;
  }
  if (m_sync_extent_pages_ongoing &&
      m_sync_extent_next_page_entry != RNIL)
  {
    jam();
    /**
     * SYNC_EXTENT_PAGES was ongoing, continueb isn't running and
     * we're also not waiting for any outstanding IO. This must mean
     * that we were blocked by too much IO, so we'll start up the
     * process again here.
     */
    Ptr<Page_entry> ptr;
    Page_sublist& pl = *m_page_sublist[Page_entry::SL_LOCKED];
    pl.getPtr(ptr, m_sync_extent_next_page_entry);
    process_lcp_locked(signal, ptr);
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
    handle_lcp(signal, m_lcp_table_id, m_lcp_fragment_id);
  }
}

#define MAX_LCP_PAGES_OUTSTANDING 32
Uint32
Pgman::get_num_lcp_pages_to_write(void)
{
  Uint32 max_count = 0;
  if (m_param.m_max_io_waits > m_stats.m_current_io_waits &&
      m_lcp_outstanding < MAX_LCP_PAGES_OUTSTANDING)
  {
    jam();
    max_count = m_param.m_max_io_waits - m_stats.m_current_io_waits;
    max_count = max_count / 2 + 1;
    if (max_count > (MAX_LCP_PAGES_OUTSTANDING - m_lcp_outstanding))
    {
      /**
       * Never more than 1 MByte of outstanding LCP pages at any time.
       * We don't want to use too much of the disk bandwidth for
       * writing out the LCP.
       */
      jam();
      max_count = MAX_LCP_PAGES_OUTSTANDING - m_lcp_outstanding;
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
Pgman::handle_lcp(Signal *signal, Uint32 tableId, Uint32 fragmentId)
{
  FragmentRecord key(*this, tableId, fragmentId);
  FragmentRecordPtr fragPtr;
  Ptr<Page_entry> ptr;
  Uint32 max_count = 0;
  ndbrequire(m_fragmentRecordHash.find(fragPtr, key));
  FragmentRecord *fragPtrP = fragPtr.p;

  if ((max_count = get_num_lcp_pages_to_write()) == 0)
  {
    jam();
    DEB_PGMAN(("No LCP pages available to write with, instance(): %u",
               instance()));
    return;
  }
  if (m_dirty_list_lcp.isEmpty() && m_dirty_list_lcp_out.isEmpty())
  {
    jam();
    DEB_PGMAN(("instance(): %u, handle_lcp finished", instance()));
    finish_lcp(signal, fragPtrP);
    return;
  }
  bool break_flag = false;
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
      DEB_PGMAN(("instance(): %u, LCP wait for write out to disk",
                 instance()));
      return;
    }
    Page_state state = ptr.p->m_state;
  
    if ((! (state & Page_entry::DIRTY)) ||
        (state & Page_entry::LOCKED) ||
        (! (state & Page_entry::BOUND)))
    {
      ndbout << ptr << endl;
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
      DEB_PGMAN(("(%u)PAGEOUT state in LCP, page(%u,%u):%u:%x",
                 instance(),
                 ptr.p->m_file_no,
                 ptr.p->m_page_no,
                 ptr.i,
                 (unsigned int)state));

      ndbrequire(ptr.p->m_dirty_state != fragPtrP->m_current_lcp_dirty_state);
      ndbrequire((state & Page_entry::DIRTY) == Page_entry::DIRTY);
      m_dirty_list_lcp.removeFirst(ptr);
      m_dirty_list_lcp_out.addLast(ptr);
      ptr.p->m_dirty_state = Pgman::IN_LCP_OUT_LIST;
      set_page_state(jamBuffer(), ptr, state | Page_entry::LCP);
    }
    else if (state & Page_entry::BUSY)
    {
      jam();

      DEB_PGMAN(("(%u)BUSY state in LCP, page(%u,%u):%u:%x",
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
      jam();

      DEB_PGMAN(("(%u)pageout():LCP, page(%u,%u):%u:%x",
                 instance(),
                 ptr.p->m_file_no,
                 ptr.p->m_page_no,
                 ptr.i,
                 (unsigned int)state));

      ndbrequire(ptr.p->m_dirty_state != fragPtrP->m_current_lcp_dirty_state);
      m_dirty_list_lcp.removeFirst(ptr);
      m_dirty_list_lcp_out.addLast(ptr);
      ptr.p->m_dirty_state = Pgman::IN_LCP_OUT_LIST;
      ptr.p->m_state |= Page_entry::LCP;
      if (c_tup != 0)
      {
        c_tup->disk_page_unmap_callback(0,
                                        ptr.p->m_real_page_i, 
                                        ptr.p->m_dirty_count);
      }
      pageout(signal, ptr);
      break_flag = true;
    }
    m_lcp_outstanding++;
    if (break_flag)
    {
      jam();
      break;
    }
  }
  start_lcp_loop(signal);
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
  DEB_PGMAN_LCP(("SYNC_EXTENT_PAGES_REQ: instance(): %u", instance()));
  ndbrequire(!m_lcp_loop_ongoing);
  m_sync_extent_order = req->lcpOrder;
  m_sync_extent_pages_ongoing = true;
  m_sync_extent_pages_req = *req;
  m_locked_pages_written = 0;
  Page_sublist& pl = *m_page_sublist[Page_entry::SL_LOCKED];
  if (pl.first(ptr))
  {
    jam();
    m_sync_extent_next_page_entry = ptr.i;
    check_restart_lcp(signal);
    return;
  }
  finish_sync_extent_pages(signal);
}

void
Pgman::finish_sync_extent_pages(Signal *signal)
{
  DEB_PGMAN_LCP(("SYNC_EXTENT_PAGES_CONF: instance(): %u", instance()));
  SyncExtentPagesConf *conf = (SyncExtentPagesConf*)signal->getDataPtr();
  m_sync_extent_pages_ongoing = false;
  ndbrequire(!m_lcp_loop_ongoing);
  m_sync_extent_next_page_entry = RNIL;
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

  if ((max_count = get_num_lcp_pages_to_write()) == 0)
  {
    jam();
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
  Tablespace_client tsman(signal, this, c_tsman, 0, 0, 0, 0);
  do
  {
    bool break_flag = false;
    if ((ptr.p->m_state & Page_entry::DIRTY) &&
        !(ptr.p->m_state & Page_entry::PAGEOUT))
    {
      jam();
      Ptr<GlobalPage> org, copy;
      ndbrequire(m_global_page_pool.seize(copy));
      m_global_page_pool.getPtr(org, ptr.p->m_real_page_i);
      memcpy(copy.p, org.p, sizeof(GlobalPage));
      ptr.p->m_copy_page_i = copy.i;

      m_lcp_outstanding++;
      ptr.p->m_state |= Page_entry::LCP;

      DEB_PGMAN(("(%u)pageout():extent, page(%u,%u):%u:%x",
                 instance(),
                 ptr.p->m_file_no,
                 ptr.p->m_page_no,
                 ptr.i,
                 (unsigned int)ptr.p->m_state));

      pageout(signal, ptr);
      break_flag = true;
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
  m_sync_extent_next_page_entry = ptr.i;
  start_lcp_loop(signal);
}

void
Pgman::process_lcp_locked_fswriteconf(Signal* signal, Ptr<Page_entry> ptr)
{
  Ptr<GlobalPage> org, copy;
  m_global_page_pool.getPtr(copy, ptr.p->m_copy_page_i);
  m_global_page_pool.getPtr(org, ptr.p->m_real_page_i);
  memcpy(org.p, copy.p, sizeof(GlobalPage));
  m_global_page_pool.release(copy);
  ptr.p->m_copy_page_i = RNIL;

  if (m_sync_extent_pages_ongoing)
  {
    jam();
    /**
     * Ensure that Backup block is notified of any progress we make on
     * completing LCPs.
     * Important that this is sent before we send SYNC_EXTENT_PAGES_CONF
     * to ensure Backup block is prepared for receiving the signal.
     */
    m_locked_pages_written++;
    sendSYNC_PAGE_WAIT_REP(signal, false);
  }
  if (!m_lcp_loop_ongoing)
  {
    if (m_sync_extent_next_page_entry == RNIL)
    {
      if (m_lcp_outstanding == 0)
      {
        jam();
        finish_sync_extent_pages(signal);
        return;
      }
      jam();
      return;
    }
    jam();
    check_restart_lcp(signal);
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

  DEB_PGMAN(("(%u)pagein() start: page(%u,%u):%u:%x",
            instance(),
            ptr.p->m_file_no,
            ptr.p->m_page_no,
            ptr.i,
            (unsigned int)ptr.p->m_state));

  ndbrequire(! (ptr.p->m_state & Page_entry::PAGEIN));
  set_page_state(jamBuf, ptr, ptr.p->m_state | Page_entry::PAGEIN);

  fsreadreq(signal, ptr);
  m_stats.m_current_io_waits++;
}

void
Pgman::fsreadconf(Signal* signal, Ptr<Page_entry> ptr)
{
  D("fsreadconf");
  D(ptr);

  Page_state state = ptr.p->m_state;

  DEB_PGMAN_IO(("(%u)pagein completed: page(%u,%u):%x",
               instance(),
               ptr.p->m_file_no,
               ptr.p->m_page_no,
               (unsigned int)state));

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
    m_global_page_pool.getPtr(pagePtr, ptr.p->m_real_page_i);
    File_formats::Datafile::Data_page* page =
      (File_formats::Datafile::Data_page*)pagePtr.p;
    
    Uint64 lsn = 0;
    lsn += page->m_page_header.m_page_lsn_hi; lsn <<= 32;
    lsn += page->m_page_header.m_page_lsn_lo;
    ptr.p->m_lsn = lsn;
  }
  
  ndbrequire(m_stats.m_current_io_waits > 0);
  m_stats.m_current_io_waits--;
  m_stats.m_pages_read++;

  /**
   * Calling check_restart_lcp before do_busy_loop ensures that
   * we make progress on LCP even in systems with very high IO
   * read rates.
   */
  check_restart_lcp(signal);
  do_busy_loop(signal, true, jamBuffer());
}

void
Pgman::pageout(Signal* signal, Ptr<Page_entry> ptr)
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
  m_global_page_pool.getPtr(pagePtr, ptr.p->m_real_page_i);
  File_formats::Datafile::Data_page* page =
    (File_formats::Datafile::Data_page*)pagePtr.p;
  page->m_page_header.m_page_lsn_hi = (Uint32)(ptr.p->m_lsn >> 32);
  page->m_page_header.m_page_lsn_lo = (Uint32)(ptr.p->m_lsn & 0xFFFFFFFF);

  int ret;
  {
    // undo WAL, release LGMAN lock ASAP
    Logfile_client::Request req;
    req.m_callback.m_callbackData = ptr.i;
    req.m_callback.m_callbackIndex = LOGSYNC_CALLBACK;
    D("Logfile_client - pageout");
    Logfile_client lgman(this, c_lgman, RNIL);
    ret = lgman.sync_lsn(signal, ptr.p->m_lsn, &req, 0);
  }
  if (ret > 0)
  {
    fswritereq(signal, ptr);
    m_stats.m_current_io_waits++;
  }
  else
  {
    ndbrequire(ret == 0);
    m_stats.m_log_waits++;
    state |= Page_entry::LOGSYNC;
  }
  set_page_state(jamBuffer(), ptr, state);
}

void
Pgman::logsync_callback(Signal* signal, Uint32 ptrI, Uint32 res)
{
  Ptr<Page_entry> ptr;
  m_page_entry_pool.getPtr(ptr, ptrI);

  D("logsync_callback");
  D(ptr);

  // it is OK to be "busy" at this point (the commit is queued)
  Page_state state = ptr.p->m_state;
  ndbrequire(state & Page_entry::PAGEOUT);
  ndbrequire(state & Page_entry::LOGSYNC);
  state &= ~ Page_entry::LOGSYNC;
  set_page_state(jamBuffer(), ptr, state);

  fswritereq(signal, ptr);
  m_stats.m_current_io_waits++;
}

void
Pgman::fswriteconf(Signal* signal, Ptr<Page_entry> ptr)
{
  D("fswriteconf");
  D(ptr);

  Page_state state = ptr.p->m_state;

  DEB_PGMAN_IO(("(%u)pageout completed, page(%u,%u):%u:%x",
               instance(),
               ptr.p->m_file_no,
               ptr.p->m_page_no,
               ptr.p->m_real_page_i,
               state));

  ndbrequire(state & Page_entry::PAGEOUT);

  if (c_tup != 0)
  {
    jam();
    c_tup->disk_page_unmap_callback(1, 
                                    ptr.p->m_real_page_i, 
                                    ptr.p->m_dirty_count);
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
      Tablespace_client tsman(signal, this, c_tsman, 0, 0, 0, 0);
      process_lcp_locked_fswriteconf(signal, ptr);
      if (ptr.p->m_dirty_during_pageout)
      {
        jam();
        ptr.p->m_dirty_during_pageout = false;
        state |= Page_entry::DIRTY;
      }
      set_page_state(jamBuffer(), ptr, state);
      do_busy_loop(signal, true, jamBuffer());
      return;
    }
  }
  else
  {
    jam();
    m_stats.m_pages_written++;
  }
  
  set_page_state(jamBuffer(), ptr, state);
  /**
   * Calling check_restart_lcp before do_busy_loop ensures that
   * we make progress on LCP even in systems with very high IO
   * read rates.
   */
  check_restart_lcp(signal);
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
  m_file_entry_pool.getPtr(file_ptr, ptrI);

  Uint32 fd = file_ptr.p->m_fd;

  ndbrequire(ptr.p->m_page_no > 0);

  FsReadWriteReq* req = (FsReadWriteReq*)signal->getDataPtrSend();
  req->filePointer = fd;
  req->userReference = reference();
  req->userPointer = ptr.i;
  req->varIndex = ptr.p->m_page_no;
  req->numberOfPages = 1;
  req->operationFlag = 0;
  FsReadWriteReq::setFormatFlag(req->operationFlag,
				FsReadWriteReq::fsFormatGlobalPage);
  req->data.pageData[0] = ptr.p->m_real_page_i;
  sendSignal(NDBFS_REF, GSN_FSREADREQ, signal,
	     FsReadWriteReq::FixedLength + 1, JBA);
}

void
Pgman::execFSREADCONF(Signal* signal)
{
  jamEntry();
  FsConf* conf = (FsConf*)signal->getDataPtr();
  Ptr<Page_entry> ptr;
  m_page_entry_pool.getPtr(ptr, conf->userPointer);

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
  m_file_entry_pool.getPtr(file_ptr, *it.data);
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

  FsReadWriteReq* req = (FsReadWriteReq*)signal->getDataPtrSend();
  req->filePointer = fd;
  req->userReference = reference();
  req->userPointer = ptr.i;
  req->varIndex = ptr.p->m_page_no;
  req->numberOfPages = 1;
  req->operationFlag = 0;
  FsReadWriteReq::setFormatFlag(req->operationFlag,
				FsReadWriteReq::fsFormatGlobalPage);
  req->data.pageData[0] = ptr.p->m_real_page_i;
  
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
  m_page_entry_pool.getPtr(ptr, conf->userPointer);

  fswriteconf(signal, ptr);
}


void
Pgman::execFSWRITEREF(Signal* signal)
{
  jamEntry();
  SimulatedBlock::execFSWRITEREF(signal);
  ndbabort();
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

  if (req_flags & Page_request::LOCK_PAGE)
  {
    thrjam(jamBuf);
    state |= Page_entry::LOCKED;
  }
  
  if (req_flags & Page_request::ALLOC_REQ)
  {
    thrjam(jamBuf);
  }
  else if (req_flags & Page_request::COMMIT_REQ)
  {
    thrjam(jamBuf);
    busy_count = 1;
    state |= Page_entry::BUSY;
  }
  else if ((req_flags & Page_request::OP_MASK) != ZREAD)
  {
    thrjam(jamBuf);
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
      ptr.p->m_dirty_during_pageout = true;
      return ptr.p->m_copy_page_i;
    }
    
    D("<get_page: immediate locked");
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

  if (! (req_flags & (Page_request::LOCK_PAGE | Page_request::UNLOCK_PAGE)))
  {
    ndbrequire(! (state & Page_entry::LOCKED));
  }

  // queue the request

  if ((state & Page_entry::MAPPED) && ! (state & Page_entry::PAGEOUT))
  {
    thrjam(jamBuf);
    m_stats.m_page_requests_wait_q++;
  }
  else
  {
    thrjam(jamBuf);
    m_stats.m_page_requests_wait_io++;
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
    }
    D("<get_page: error out of requests");
    return -1;
  }

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
  if (unlikely(i == -1))
  {
    thrjam(jamBuf);
    return -1;
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
     ndbassert((state & Page_entry::LOCKED) == 0);
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

  m_file_entry_pool.getPtr(file_ptr, *it.data);
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
  m_file_entry_pool.getPtr(file_ptr, *it.data);
  
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
 * amongst those requests and wil be served one at a time.
 *
 * When a page is requested with either COMMIT_REQ/DIRTY_REQ/ALLOC_REQ then the
 * page will be put into the dirty state after completing the request. We will
 * put it into the fragement dirty list only when we call the callback from the
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
 * a restart then we know that it will be kept consistent by continously
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
 * We might optimise things by only synching the page free bits always after
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
    m_pgman->m_global_page_pool.getPtr(m_ptr, (Uint32)i);
  }
  return i;
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
  m_fragmentRecordPool.seize(fragPtr);
  if (fragPtr.i == RNIL)
  {
    jam();
    return 1;
  }
  /* Initialise head objects by calling constructor in-place */
  new (fragPtr.p) FragmentRecord(*this, tableId, fragmentId);
  ndbrequire(!m_fragmentRecordHash.find(check, *fragPtr.p));
  m_fragmentRecordHash.add(fragPtr);
  m_fragmentRecordList.addFirst(fragPtr);
  return 0;
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
  if (fragPtr.i != RNIL)
  {
    jam();
    m_fragmentRecordList.remove(fragPtr);
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
     */
    thrjam(jamBuf);
    return;
  }

  DEB_PGMAN(("(%u)Insert page(%u,%u):%u:%x into dirty list of tab(%u,%u)"
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
   * Add the page entry as first item in the dirty list.
   */
  ptr.p->m_dirty_state = fragPtr.p->m_current_lcp_dirty_state;
  {
    LocalPage_dirty_list list(m_page_entry_pool, fragPtr.p->m_dirty_list);
    list.addFirst(ptr);
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
    DEB_PGMAN(("(%u)remove_fragment_dirty_list not in any list: "
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

      DEB_PGMAN(("(%u)Remove page page(%u,%u):%u:%x from dirty list"
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

      DEB_PGMAN(("(%u)Remove page(%u,%u):%u:%x from dirty lcp"
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
    DEB_PGMAN(("(%u)Remove page(%u,%u):%u:%x from dirty out"
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
    if (pr.m_flags & Pgman::Page_request::DIRTY_REQ)
      out << ",dirty_req";
    if (pr.m_flags & Pgman::Page_request::CORR_REQ)
      out << ",corr_req";
    if (pr.m_flags & Pgman::Page_request::DISK_SCAN)
      out << ",disk_scan";
  }
  return out;
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
      pe.m_this->m_global_page_pool.getPtr(gptr, pe.m_real_page_i);
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

    ndbout_c("pgman(%u)\n"
             " page_entry_pool: size %u used: %u (%u %%)\n"
             " high: %u (%u %%)\n"
             " locked pages: %u\n"
             " \t related to entries %u (%u %%)\n"
             " \t related to available pages for extent pages %u (%u %%)\n"
             " \t related to Total pages in disk page buffer memory %u (%u %%)\n",
             instance(),
             size, used, usedpct,
             high, highpct,
             locked,
             size, lockedpct,
             avail_for_extent_pages, lockedpct2,
             max_pages, lockedpct3);
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

void
Pgman::execDBINFO_SCANREQ(Signal *signal)
{
  DbinfoScanReq req= *(DbinfoScanReq*)signal->theData;
  Ndbinfo::Ratelimit rl;

  jamEntry();
  switch(req.tableId) {
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
  }
  default:
    break;
  }
  ndbinfo_send_scan_conf(signal, req, rl);
}
