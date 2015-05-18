/*
   Copyright (c) 2005, 2015, Oracle and/or its affiliates. All rights reserved.

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

#include "pgman.hpp"
#include <signaldata/FsRef.hpp>
#include <signaldata/FsConf.hpp>
#include <signaldata/FsReadWriteReq.hpp>
#include <signaldata/PgmanContinueB.hpp>
#include <signaldata/LCP.hpp>
#include <signaldata/DataFileOrd.hpp>
#include <signaldata/ReleasePages.hpp>

#include <dbtup/Dbtup.hpp>

#include <DebuggerNames.hpp>
#include <md5_hash.hpp>

#include <PgmanProxy.hpp>

#define JAM_FILE_ID 335


/**
 * Requests that make page dirty
 */
#define DIRTY_FLAGS (Page_request::COMMIT_REQ | \
                     Page_request::DIRTY_REQ | \
                     Page_request::ALLOC_REQ)

static bool g_dbg_lcp = false;
#if 1
#define DBG_LCP(x)
#else
#define DBG_LCP(x) if(g_dbg_lcp) ndbout << x
#endif

Pgman::Pgman(Block_context& ctx, Uint32 instanceNumber) :
  SimulatedBlock(PGMAN, ctx, instanceNumber),
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

  addRecSignal(GSN_LCP_FRAG_ORD, &Pgman::execLCP_FRAG_ORD);
  addRecSignal(GSN_END_LCPREQ, &Pgman::execEND_LCPREQ);

  addRecSignal(GSN_DATA_FILE_ORD, &Pgman::execDATA_FILE_ORD);
  addRecSignal(GSN_RELEASE_PAGES_REQ, &Pgman::execRELEASE_PAGES_REQ);
  addRecSignal(GSN_DBINFO_SCANREQ, &Pgman::execDBINFO_SCANREQ);
  
  // loop status
  m_stats_loop_on = false;
  m_busy_loop_on = false;
  m_cleanup_loop_on = false;

  // LCP variables
  m_lcp_state = LS_LCP_OFF;
  m_last_lcp = 0;
  m_last_lcp_complete = 0;
  m_lcp_curr_bucket = ~(Uint32)0;
  m_lcp_outstanding = 0;

  // clean-up variables
  m_cleanup_ptr.i = RNIL;

  // should be a factor larger than number of pool pages
  m_data_buffer_pool.setSize(16);
  m_page_hashlist.setSize(512);
  
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
    m_page_entry_pool.setSize(m_param.m_lirs_stack_mult * page_cnt);

    m_param.m_max_hot_pages = (page_cnt * 9) / 10;
    ndbrequire(m_param.m_max_hot_pages >= 1);
  }

  Pool_context pc;
  pc.m_block = this;
  m_page_request_pool.wo_pool_init(RT_PGMAN_PAGE_REQUEST, pc);
  
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
  m_cleanup_loop_delay(200),
  m_lcp_loop_delay(0)
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
  Uint32 data1 = signal->theData[1];

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
    jam();
    do_lcp_loop(signal);
    break;
  case PgmanContinueB::LCP_LOCKED:
  {
    jam();
    Ptr<Page_entry> ptr;
    Page_sublist& pl = *m_page_sublist[Page_entry::SL_LOCKED];
    if (data1 != RNIL)
    {
      jam();
      pl.getPtr(ptr, data1);
      process_lcp_locked(signal, ptr);
    }
    else
    {
      jam();
      if (ERROR_INSERTED(11007))
      {
        ndbout << "No more writes..." << endl;
        SET_ERROR_INSERT_VALUE(11008);
        signal->theData[0] = 9999;
        sendSignalWithDelay(CMVMI_REF, GSN_NDB_TAMPER, signal, 10000, 1);
      }
      EndLcpConf* conf = (EndLcpConf*)signal->getDataPtrSend();
      conf->senderData = m_end_lcp_req.senderData;
      conf->senderRef = reference();
      sendSignal(m_end_lcp_req.senderRef, GSN_END_LCPCONF,
                 signal, EndLcpConf::SignalLength, JBB);
      m_lcp_state = LS_LCP_OFF;
    }
    return;
  }
  default:
    ndbrequire(false);
    break;
  }
}

// page entry

Pgman::Page_entry::Page_entry(Uint32 file_no, Uint32 page_no) :
  m_file_no(file_no),
  m_state(0),
  m_page_no(page_no),
  m_real_page_i(RNIL),
  m_lsn(0),
  m_last_lcp(0),
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
  D(">set_page_state: state=" << hex << new_state);
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
    D("find_page_entry");
    D(ptr);
    return true;
  }
  return false;
}

Uint32
Pgman::seize_page_entry(Ptr<Page_entry>& ptr, Uint32 file_no, Uint32 page_no)
{
  if (m_page_entry_pool.seize(ptr))
  {
    new (ptr.p) Page_entry(file_no, page_no);
    m_page_hashlist.add(ptr);
#ifdef VM_TRACE
    ptr.p->m_this = this;
#endif
    D("seize_page_entry");
    D(ptr);

    if (m_stats.m_entries_high < m_page_entry_pool.getUsed())
      m_stats.m_entries_high = m_page_entry_pool.getUsed();

    return true;
  }
  return false;
}

bool
Pgman::get_page_entry(EmulatedJamBuffer* jamBuf, Ptr<Page_entry>& ptr, 
                      Uint32 file_no, Uint32 page_no)
{
  if (find_page_entry(ptr, file_no, page_no))
  {
    thrjam(jamBuf);
    ndbrequire(ptr.p->m_state != 0);
    m_stats.m_page_hits++;

    D("get_page_entry: found");
    D(ptr);
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

      release_page_entry(idle_ptr);
    }
  }

  if (seize_page_entry(ptr, file_no, page_no))
  {
    thrjam(jamBuf);
    ndbrequire(ptr.p->m_state == 0);
    m_stats.m_page_faults++;

    D("get_page_entry: seize");
    D(ptr);
    return true;
  }

  ndbrequire(false);
  
  return false;
}

void
Pgman::release_page_entry(Ptr<Page_entry>& ptr)
{
  EmulatedJamBuffer *jamBuf = getThrJamBuf();

  D("release_page_entry");
  D(ptr);
  Page_state state = ptr.p->m_state;

  ndbrequire(ptr.p->m_requests.isEmpty());

  ndbrequire(! (state & Page_entry::ONSTACK));
  ndbrequire(! (state & Page_entry::ONQUEUE));
  ndbrequire(ptr.p->m_real_page_i == RNIL);

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
  
  set_page_state(jamBuffer(), ptr, 0);
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
Pgman::lirs_stack_prune()
{
  D(">lirs_stack_prune");
  Page_stack& pl_stack = m_page_stack;
  Ptr<Page_entry> ptr;

  while (pl_stack.first(ptr))      // first is stack bottom
  {
    Page_state state = ptr.p->m_state;
    if (state & Page_entry::HOT)
    {
      jam();
      break;
    }

    D(ptr << ": prune from stack");

    pl_stack.remove(ptr);
    state &= ~ Page_entry::ONSTACK;
    set_page_state(jamBuffer(), ptr, state);

    if (state & Page_entry::BOUND)
    {
      jam();
      ndbrequire(state & Page_entry::ONQUEUE);
    }
    else if (state & Page_entry::REQUEST)
    {
      // enters queue at bind
      jam();
      ndbrequire(! (state & Page_entry::ONQUEUE));
    }
    else
    {
      jam();
      release_page_entry(ptr);
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
Pgman::lirs_stack_pop()
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
    jam();
    pl_queue.addLast(ptr);
    state |= Page_entry::ONQUEUE;
  }
  else
  {
    // enters queue at bind
    jam();
    ndbrequire(state & Page_entry::REQUEST);
  }

  set_page_state(jamBuffer(), ptr, state);
  lirs_stack_prune();
}

/*
 * Update LIRS lists when page is referenced.
 */
void
Pgman::lirs_reference(Ptr<Page_entry> ptr)
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
      jam();
      ndbrequire(state & Page_entry::ONSTACK);
      bool at_bottom = ! pl_stack.hasPrev(ptr);
      pl_stack.remove(ptr);
      pl_stack.addLast(ptr);
      if (at_bottom)
      {
        jam();
        lirs_stack_prune();
      }
    }
    else if (state & Page_entry::ONSTACK)
    {
      // case 2a 3a
      jam();
      pl_stack.remove(ptr);
      if (! pl_stack.isEmpty())
      {
        jam();
        lirs_stack_pop();
      }
      pl_stack.addLast(ptr);
      state |= Page_entry::HOT;
      if (state & Page_entry::ONQUEUE)
      {
        jam();
        move_cleanup_ptr(ptr);
        pl_queue.remove(ptr);
        state &= ~ Page_entry::ONQUEUE;
      }
    }
    else
    {
      // case 2b 3b
      jam();
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
        jam();
        move_cleanup_ptr(ptr);
        pl_queue.remove(ptr);
        state &= ~ Page_entry::ONQUEUE;
      }
      if (state & Page_entry::BOUND)
      {
        jam();
        pl_queue.addLast(ptr);
        state |= Page_entry::ONQUEUE;
      }
      else
      {
        // enters queue at bind
        jam();
      }
    }
  }
  else
  {
    D("filling up hot pages: " << m_stats.m_num_hot_pages << "/"
                               << m_param.m_max_hot_pages);
    jam();
    if (state & Page_entry::ONSTACK)
    {
      jam();
      bool at_bottom = ! pl_stack.hasPrev(ptr);
      pl_stack.remove(ptr);
      if (at_bottom)
      {
        jam();
        ndbassert(state & Page_entry::HOT);
        lirs_stack_prune();
      }
    }
    pl_stack.addLast(ptr);
    state |= Page_entry::ONSTACK;
    state |= Page_entry::HOT;
    // it could be on queue already
    if (state & Page_entry::ONQUEUE) {
      jam();
      pl_queue.remove(ptr);
      state &= ~Page_entry::ONQUEUE;
    }
  }

  set_page_state(jamBuffer(), ptr, state);
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
    (void)process_bind(signal);
    (void)process_map(signal);
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
    (void)process_bind(signal);
    (void)process_map(signal);
    (void)process_callback(signal);
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

void
Pgman::do_lcp_loop(Signal* signal)
{
  D(">do_lcp_loop m_lcp_state=" << Uint32(m_lcp_state));
  ndbrequire(m_lcp_state != LS_LCP_OFF);
  LCP_STATE newstate = process_lcp(signal);

  switch(newstate) {
  case LS_LCP_OFF:
    jam();
    break;
  case LS_LCP_ON:
    jam();
    signal->theData[0] = PgmanContinueB::LCP_LOOP;
    sendSignal(reference(), GSN_CONTINUEB, signal, 1, JBB);
    break;
  case LS_LCP_MAX_LCP_OUTSTANDING: // wait until io is completed
    jam();
    break;
  case LS_LCP_LOCKED:
    jam();
    break;
  }
  m_lcp_state = newstate;
  D("<do_lcp_loop m_lcp_state=" << Uint32(m_lcp_state));
}

// busy loop

bool
Pgman::process_bind(Signal* signal)
{
  D(">process_bind");
  int max_count = 32;
  Page_sublist& pl_bind = *m_page_sublist[Page_entry::SL_BIND];

  while (! pl_bind.isEmpty() && --max_count >= 0)
  {
    jam();
    Ptr<Page_entry> ptr;
    pl_bind.first(ptr);
    if (! process_bind(signal, ptr))
    {
      jam();
      break;
    }
  }
  D("<process_bind");
  return ! pl_bind.isEmpty();
}

bool
Pgman::process_bind(Signal* signal, Ptr<Page_entry> ptr)
{
  D(ptr << " : process_bind");
  Page_queue& pl_queue = m_page_queue;
  Ptr<GlobalPage> gptr;

  if (m_stats.m_num_pages < m_param.m_max_pages)
  {
    jam();
    bool ok = seize_cache_page(gptr);
    // to handle failure requires some changes in LIRS
    ndbrequire(ok);
  }
  else
  {
    jam();
    Ptr<Page_entry> clean_ptr;
    if (! pl_queue.first(clean_ptr))
    {
      jam();
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
      jam();
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

    move_cleanup_ptr(clean_ptr);
    pl_queue.remove(clean_ptr);
    clean_state &= ~ Page_entry::ONQUEUE;

    gptr.i = clean_ptr.p->m_real_page_i;

    clean_ptr.p->m_real_page_i = RNIL;
    clean_state &= ~ Page_entry::BOUND;
    clean_state &= ~ Page_entry::MAPPED;

    set_page_state(jamBuffer(), clean_ptr, clean_state);

    if (! (clean_state & Page_entry::ONSTACK))
    {
      jam();
      release_page_entry(clean_ptr);
    }

    m_global_page_pool.getPtr(gptr);
  }

  Page_state state = ptr.p->m_state;

  ptr.p->m_real_page_i = gptr.i;
  state |= Page_entry::BOUND;
  if (state & Page_entry::EMPTY)
  {
    jam();
    state |= Page_entry::MAPPED;
  }

  if (! (state & Page_entry::LOCKED) &&
      ! (state & Page_entry::ONQUEUE) &&
      ! (state & Page_entry::HOT))
  {
    jam();

    D(ptr << " : add to queue at bind");
    pl_queue.addLast(ptr);
    state |= Page_entry::ONQUEUE;
  }

  set_page_state(jamBuffer(), ptr, state);
  return true;
}

bool
Pgman::process_map(Signal* signal)
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
    jam();
    Ptr<Page_entry> ptr;
    pl_map.first(ptr);
    if (! process_map(signal, ptr))
    {
      jam();
      break;
    }
  }
  D("<process_map");
  return ! pl_map.isEmpty();
}

bool
Pgman::process_map(Signal* signal, Ptr<Page_entry> ptr)
{
  D(ptr << " : process_map");
  pagein(signal, ptr);
  return true;
}

bool
Pgman::process_callback(Signal* signal)
{
  D(">process_callback");
  int max_count = 1;
  Page_sublist& pl_callback = *m_page_sublist[Page_entry::SL_CALLBACK];

  Ptr<Page_entry> ptr;
  pl_callback.first(ptr);

  while (! ptr.isNull() && --max_count >= 0)
  {
    jam();
    Ptr<Page_entry> curr = ptr;
    pl_callback.next(ptr);
    
    if (! process_callback(signal, curr))
    {
      jam();
      break;
    }
  }
  D("<process_callback");
  return ! pl_callback.isEmpty();
}

bool
Pgman::process_callback(Signal* signal, Ptr<Page_entry> ptr)
{
  D(ptr << " : process_callback");
  int max_count = 1;

  while (! ptr.p->m_requests.isEmpty() && --max_count >= 0)
  {
    jam();
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
        jam();
        state |= Page_entry::DIRTY;
	ndbassert(ptr.p->m_dirty_count);
	ptr.p->m_dirty_count --;
      }

      req_list.releaseFirst(/* req_ptr */);
    }
    ndbrequire(state & Page_entry::BOUND);
    ndbrequire(state & Page_entry::MAPPED);

    // make REQUEST state consistent before set_page_state()
    if (ptr.p->m_requests.isEmpty())
    {
      jam();
      state &= ~ Page_entry::REQUEST;
    }
    
    // callback may re-enter PGMAN and change page state
    set_page_state(jamBuffer(), ptr, state);
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
Pgman::move_cleanup_ptr(Ptr<Page_entry> ptr)
{
  Page_queue& pl_queue = m_page_queue;
  if (ptr.i == m_cleanup_ptr.i)
  {
    jam();
    pl_queue.prev(m_cleanup_ptr);
  }
}

// LCP


void
Pgman::execLCP_FRAG_ORD(Signal* signal)
{
  if (ERROR_INSERTED(11008))
  {
    ndbout_c("Ignore LCP_FRAG_ORD");
    return;
  }
  LcpFragOrd* ord = (LcpFragOrd*)signal->getDataPtr();
  ndbrequire(ord->lcpId >= m_last_lcp_complete + 1 || m_last_lcp_complete == 0);
  m_last_lcp = ord->lcpId;
  DBG_LCP("Pgman::execLCP_FRAG_ORD lcp: " << m_last_lcp << endl);
  
  D("execLCP_FRAG_ORD"
    << " this=" << m_last_lcp
    << " last_complete=" << m_last_lcp_complete
    << " bucket=" << m_lcp_curr_bucket);
}

void
Pgman::execEND_LCPREQ(Signal* signal)
{
  if (ERROR_INSERTED(11008))
  {
    ndbout_c("Ignore END_LCP");
    return;
  }

  EndLcpReq* req = (EndLcpReq*)signal->getDataPtr();
  m_end_lcp_req = *req;

  DBG_LCP("execEND_LCPREQ" << endl);

  ndbrequire(!m_lcp_outstanding);
  m_lcp_curr_bucket = 0;
  
  D("execEND_LCPREQ"
    << " this=" << m_last_lcp
    << " last_complete=" << m_last_lcp_complete
    << " bucket=" << m_lcp_curr_bucket
    << " outstanding=" << m_lcp_outstanding);

  m_last_lcp_complete = m_last_lcp;
  ndbrequire(m_lcp_state == LS_LCP_OFF);
  m_lcp_state = LS_LCP_ON;
  do_lcp_loop(signal);
}

Pgman::LCP_STATE
Pgman::process_lcp(Signal* signal)
{
  Page_hashlist& pl_hash = m_page_hashlist;

  int max_count = 0;
  if (m_param.m_max_io_waits > m_stats.m_current_io_waits)
  {
    jam();
    max_count = m_param.m_max_io_waits - m_stats.m_current_io_waits;
    max_count = max_count / 2 + 1;
  }

  D("process_lcp"
    << " this=" << m_last_lcp
    << " last_complete=" << m_last_lcp_complete
    << " bucket=" << m_lcp_curr_bucket
    << " outstanding=" << m_lcp_outstanding);

  // start or re-start from beginning of current hash bucket
  if (m_lcp_curr_bucket != ~(Uint32)0)
  {
    jam();
    Page_hashlist::Iterator iter;
    pl_hash.next(m_lcp_curr_bucket, iter);
    Uint32 loop = 0;
    while (iter.curr.i != RNIL && 
	   m_lcp_outstanding < (Uint32) max_count &&
	   (loop ++ < 32 || iter.bucket == m_lcp_curr_bucket))
    {
      jam();
      Ptr<Page_entry>& ptr = iter.curr;
      Page_state state = ptr.p->m_state;
      
      DBG_LCP("LCP " << ptr << " - ");
      
      if (ptr.p->m_last_lcp < m_last_lcp &&
          (state & Page_entry::DIRTY) &&
	  (! (state & Page_entry::LOCKED)))
      {
        jam();
        if(! (state & Page_entry::BOUND))
        {
          ndbout << ptr << endl;
          ndbrequire(false);
        }
        if (state & Page_entry::BUSY)
        {
          jam();
          DBG_LCP(" BUSY" << endl);
          break;  // wait for it
        } 
	else if (state & Page_entry::PAGEOUT)
        {
          jam();
          DBG_LCP(" PAGEOUT -> state |= LCP" << endl);
          set_page_state(jamBuffer(), ptr, state | Page_entry::LCP);
        }
        else
        {
          jam();
          DBG_LCP(" pageout()" << endl);
          ptr.p->m_state |= Page_entry::LCP;
          if (c_tup != 0)
            c_tup->disk_page_unmap_callback(0,
                                            ptr.p->m_real_page_i, 
                                            ptr.p->m_dirty_count);
          pageout(signal, ptr);
        }
        ptr.p->m_last_lcp = m_last_lcp;
        m_lcp_outstanding++;
      }
      else
      {
        jam();
        DBG_LCP(" NOT DIRTY" << endl);
      }	
      pl_hash.next(iter);
    }
    
    m_lcp_curr_bucket = (iter.curr.i != RNIL ? iter.bucket : ~(Uint32)0);
  }

  if (m_lcp_curr_bucket == ~(Uint32)0  && !m_lcp_outstanding)
  {
    jam();
    Ptr<Page_entry> ptr;
    Page_sublist& pl = *m_page_sublist[Page_entry::SL_LOCKED];
    if (pl.first(ptr))
    {
      jam();
      process_lcp_locked(signal, ptr);
      return LS_LCP_LOCKED;
    }
    else
    {
      jam();
      if (ERROR_INSERTED(11007))
      {
        ndbout << "No more writes..." << endl;
        signal->theData[0] = 9999;
        sendSignalWithDelay(CMVMI_REF, GSN_NDB_TAMPER, signal, 10000, 1);
        SET_ERROR_INSERT_VALUE(11008);
      }
      EndLcpConf* conf = (EndLcpConf*)signal->getDataPtrSend();
      conf->senderData = m_end_lcp_req.senderData;
      conf->senderRef = reference();
      sendSignal(m_end_lcp_req.senderRef, GSN_END_LCPCONF,
                 signal, EndLcpConf::SignalLength, JBB);
      return LS_LCP_OFF;
    }
  }

  if (m_lcp_outstanding >= (Uint32) max_count)
  {
    jam();
    return LS_LCP_MAX_LCP_OUTSTANDING;
  }
  
  return LS_LCP_ON;
}

void
Pgman::process_lcp_locked(Signal* signal, Ptr<Page_entry> ptr)
{
  CRASH_INSERTION(11006);

  // protect from tsman parallel access
  Tablespace_client tsman(signal, this, c_tsman, 0, 0, 0);
  ptr.p->m_last_lcp = m_last_lcp;
  if (ptr.p->m_state & Page_entry::DIRTY)
  {
    Ptr<GlobalPage> org, copy;
    ndbrequire(m_global_page_pool.seize(copy));
    m_global_page_pool.getPtr(org, ptr.p->m_real_page_i);
    memcpy(copy.p, org.p, sizeof(GlobalPage));
    ptr.p->m_copy_page_i = copy.i;

    m_lcp_outstanding++;
    ptr.p->m_state |= Page_entry::LCP;
    pageout(signal, ptr);
    return;
  }
  
  Page_sublist& pl = *m_page_sublist[Page_entry::SL_LOCKED];
  pl.next(ptr);
  
  signal->theData[0] = PgmanContinueB::LCP_LOCKED;
  signal->theData[1] = ptr.i;
  sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
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

  Page_sublist& pl = *m_page_sublist[Page_entry::SL_LOCKED];
  pl.next(ptr);
  
  signal->theData[0] = PgmanContinueB::LCP_LOCKED;
  signal->theData[1] = ptr.i;
  sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
}

// page read and write

void
Pgman::pagein(Signal* signal, Ptr<Page_entry> ptr)
{
  D("pagein");
  D(ptr);

  ndbrequire(! (ptr.p->m_state & Page_entry::PAGEIN));
  set_page_state(jamBuffer(), ptr, ptr.p->m_state | Page_entry::PAGEIN);

  fsreadreq(signal, ptr);
  m_stats.m_current_io_waits++;
}

void
Pgman::fsreadconf(Signal* signal, Ptr<Page_entry> ptr)
{
  D("fsreadconf");
  D(ptr);

  ndbrequire(ptr.p->m_state & Page_entry::PAGEIN);
  Page_state state = ptr.p->m_state;

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

  ptr.p->m_last_lcp = m_last_lcp_complete;
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

  // undo WAL
  Logfile_client::Request req;
  req.m_callback.m_callbackData = ptr.i;
  req.m_callback.m_callbackIndex = LOGSYNC_CALLBACK;
  D("Logfile_client - pageout");
  Logfile_client lgman(this, c_lgman, RNIL);
  int ret = lgman.sync_lsn(signal, ptr.p->m_lsn, &req, 0);
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

  if (state & Page_entry::LCP)
  {
    jam();
    state &= ~ Page_entry::LCP;
    ndbrequire(m_lcp_outstanding);
    m_lcp_outstanding--;
    m_stats.m_pages_written_lcp++;
    if (ptr.p->m_copy_page_i != RNIL)
    {
      jam();
      Tablespace_client tsman(signal, this, c_tsman, 0, 0, 0);
      process_lcp_locked_fswriteconf(signal, ptr);
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
  do_busy_loop(signal, true, jamBuffer());

  if (m_lcp_state == LS_LCP_MAX_LCP_OUTSTANDING)
  {
    jam();
    do_lcp_loop(signal);
  }
}

// file system interface

void
Pgman::fsreadreq(Signal* signal, Ptr<Page_entry> ptr)
{
  File_map::ConstDataBufferIterator it;
  bool ret = m_file_map.first(it) && m_file_map.next(it, ptr.p->m_file_no);
  ndbrequire(ret);
  Uint32 fd = * it.data;

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

  fsreadconf(signal, ptr);
}

void
Pgman::execFSREADREF(Signal* signal)
{
  jamEntry();
  SimulatedBlock::execFSREADREF(signal);
  ndbrequire(false);
}

void
Pgman::fswritereq(Signal* signal, Ptr<Page_entry> ptr)
{
  File_map::ConstDataBufferIterator it;
  m_file_map.first(it);
  m_file_map.next(it, ptr.p->m_file_no);
  Uint32 fd = * it.data;

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

#if ERROR_INSERT_CODE
  if (ptr.p->m_state & Page_entry::LOCKED)
  {
    sendSignalWithDelay(NDBFS_REF, GSN_FSWRITEREQ, signal,
			3000, FsReadWriteReq::FixedLength + 1);
    ndbout_c("pageout locked (3s)");
    return;
  }
#endif
  
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
  ndbrequire(false);
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
  bool busy_count = false;

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
    busy_count = true;
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
    ptr.p->m_state |= (req_flags & DIRTY_FLAGS ? Page_entry::DIRTY : 0);
    m_stats.m_page_requests_direct_return++;
    if (ptr.p->m_copy_page_i != RNIL)
    {
      thrjam(jamBuf);
      D("<get_page: immediate copy_page");
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
      thrjam(jamBuf);
      if (req_flags & DIRTY_FLAGS)
      {
        thrjam(jamBuf);
	state |= Page_entry::DIRTY;
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
  {
    Local_page_request_list req_list(m_page_request_pool, ptr.p->m_requests);
    if (! (req_flags & Page_request::ALLOC_REQ))
    {
      thrjam(jamBuf);
      req_list.seizeLast(req_ptr);
    }
    else
    {
      thrjam(jamBuf);
      req_list.seizeFirst(req_ptr);
    }
  }
  
  if (req_ptr.i == RNIL)
  {
    thrjam(jamBuf);
    if (is_new)
    {
      thrjam(jamBuf);
      release_page_entry(ptr);
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
Pgman::get_page(EmulatedJamBuffer* jamBuf, Signal* signal, Ptr<Page_entry> ptr,
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
    lirs_reference(ptr);
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
Pgman::update_lsn(EmulatedJamBuffer* jamBuf, Ptr<Page_entry> ptr, Uint32 block,
                  Uint64 lsn)
{
  thrjam(jamBuf);
  D(">update_lsn: block=" << hex << block << dec << " lsn=" << lsn);
  D(ptr);

  Page_state state = ptr.p->m_state;
  ptr.p->m_lsn = lsn;
  
  if (state & Page_entry::BUSY)
  {
    ndbrequire(ptr.p->m_busy_count != 0);
    if (--ptr.p->m_busy_count == 0)
    {
      state &= ~ Page_entry::BUSY;
    }
  }
  
  state |= Page_entry::DIRTY;
  set_page_state(jamBuf, ptr, state);
  
  D(ptr);
  D("<update_lsn");
}

Uint32
Pgman::create_data_file()
{
  File_map::DataBufferIterator it;
  if(m_file_map.first(it))
  {
    do 
    {
      if(*it.data == RNIL)
      {
	*it.data = (1u << 31) | it.pos;
        D("create_data_file:" << V(it.pos));
	return it.pos;
      }
    } while(m_file_map.next(it));
  }

  Uint32 file_no = m_file_map.getSize();
  Uint32 fd = (1u << 31) | file_no;

  if (m_file_map.append(&fd, 1))
  {
    D("create_data_file:" << V(file_no));
    return file_no;
  }
  D("create_data_file: RNIL");
  return RNIL;
}

Uint32
Pgman::alloc_data_file(Uint32 file_no)
{
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
	return RNIL;
      }
    }
  }

  File_map::DataBufferIterator it;
  m_file_map.first(it);
  m_file_map.next(it, file_no);
  if (* it.data != RNIL)
  {
    D("alloc_data_file: RNIL");
    return RNIL;
  }

  *it.data = (1u << 31) | file_no;
  D("alloc_data_file:" << V(file_no));
  return file_no;
}

void
Pgman::map_file_no(Uint32 file_no, Uint32 fd)
{
  File_map::DataBufferIterator it;
  m_file_map.first(it);
  m_file_map.next(it, file_no);

  ndbassert(*it.data == ((1u << 31) | file_no));
  *it.data = fd;
  D("map_file_no:" << V(file_no) << V(fd));
}

void
Pgman::free_data_file(Uint32 file_no, Uint32 fd)
{
  File_map::DataBufferIterator it;
  m_file_map.first(it);
  m_file_map.next(it, file_no);
  
  if (fd == RNIL)
  {
    ndbrequire(*it.data == ((1u << 31) | file_no));
  }
  else
  {
    ndbrequire(*it.data == fd);
  }
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
    ret = create_data_file();
    ndbrequire(ret == ord->ret);
    break;
  case DataFileOrd::AllocDataFile:
    ret = alloc_data_file(ord->file_no);
    ndbrequire(ret == ord->ret);
    break;
  case DataFileOrd::MapFileNo:
    map_file_no(ord->file_no, ord->fd);
    break;
  case DataFileOrd::FreeDataFile:
    free_data_file(ord->file_no, ord->fd);
    break;
  default:
    ndbrequire(false);
    break;
  }
}

int
Pgman::drop_page(Ptr<Page_entry> ptr)
{
  D("drop_page");
  D(ptr);

  Page_stack& pl_stack = m_page_stack;
  Page_queue& pl_queue = m_page_queue;

  Page_state state = ptr.p->m_state;
  if (! (state & (Page_entry::PAGEIN | Page_entry::PAGEOUT)))
  {
    if (state & Page_entry::ONSTACK)
    {
      jam();
      bool at_bottom = ! pl_stack.hasPrev(ptr);
      pl_stack.remove(ptr);
      state &= ~ Page_entry::ONSTACK;
      if (at_bottom)
      {
        jam();
        lirs_stack_prune();
      }
      if (state & Page_entry::HOT)
      {
        jam();
        state &= ~ Page_entry::HOT;
      }
    }

    if (state & Page_entry::ONQUEUE)
    {
      jam();
      pl_queue.remove(ptr);
      state &= ~ Page_entry::ONQUEUE;
    }

    if (state & Page_entry::BUSY)
    {
      jam();
      state &= ~ Page_entry::BUSY;
    }

    if (state & Page_entry::DIRTY)
    {
      jam();
      state &= ~ Page_entry::DIRTY;
    }

    if (state & Page_entry::EMPTY)
    {
      jam();
      state &= ~ Page_entry::EMPTY;
    }

    if (state & Page_entry::MAPPED)
    {
      jam();
      state &= ~ Page_entry::MAPPED;
    }

    if (state & Page_entry::BOUND)
    {
      jam();
      ndbrequire(ptr.p->m_real_page_i != RNIL);
      release_cache_page(ptr.p->m_real_page_i);
      ptr.p->m_real_page_i = RNIL;
      state &= ~ Page_entry::BOUND;
    }

    set_page_state(jamBuffer(), ptr, state);
    release_page_entry(ptr);
    return 1;
  }
  
  ndbrequire(false);
  return -1;
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
      drop_page(ptr);
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

int
Page_cache_client::get_page(Signal* signal, Request& req, Uint32 flags)
{
  if (m_pgman_proxy != 0) {
    thrjam(m_jamBuf);
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
  bool ok = m_pgman->get_page_entry(m_jamBuf, entry_ptr, file_no, page_no);
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
Page_cache_client::update_lsn(Local_key key, Uint64 lsn)
{
  if (m_pgman_proxy != 0) {
    thrjam(m_jamBuf);
    m_pgman_proxy->update_lsn(*this, key, lsn);
    return;
  }
  thrjam(m_jamBuf);

  Ptr<Pgman::Page_entry> entry_ptr;
  Uint32 file_no = key.m_file_no;
  Uint32 page_no = key.m_page_no;

  D("update_lsn" << V(file_no) << V(page_no) << V(lsn));

  bool found = m_pgman->find_page_entry(entry_ptr, file_no, page_no);
  require(found);

  m_pgman->update_lsn(m_jamBuf, entry_ptr, m_block, lsn);
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

  return m_pgman->drop_page(entry_ptr);
}

Uint32
Page_cache_client::create_data_file(Signal* signal)
{
  if (m_pgman_proxy != 0) {
    thrjam(m_jamBuf);
    return m_pgman_proxy->create_data_file(signal);
  }
  return m_pgman->create_data_file();
}

Uint32
Page_cache_client::alloc_data_file(Signal* signal, Uint32 file_no)
{
  if (m_pgman_proxy != 0) {
    thrjam(m_jamBuf);
    return m_pgman_proxy->alloc_data_file(signal, file_no);
  }
  thrjam(m_jamBuf);
  return m_pgman->alloc_data_file(file_no);
}

void
Page_cache_client::map_file_no(Signal* signal, Uint32 file_no, Uint32 fd)
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
  ndbrequire(on_stack == pl_stack.count() || dump_page_lists());
  ndbrequire(on_queue == pl_queue.count() || dump_page_lists());

  Uint32 k;
  Uint32 entry_count = 0;
  char sublist_info[200] = "";
  for (k = 0; k < Page_entry::SUBLIST_COUNT; k++)
  {
    thrjam(jamBuf);
    const Page_sublist& pl = *m_page_sublist[k];
    for (pl.first(ptr); ptr.i != RNIL; pl.next(ptr))
      ndbrequire(get_sublist_no(ptr.p->m_state) == k || dump_page_lists(ptr.i));
    entry_count += pl.count();
    sprintf(sublist_info + strlen(sublist_info),
            " %s:%u", get_sublist_name(k), pl.count());
  }
  ndbrequire(entry_count == pl_hash.count() || dump_page_lists());

  Uint32 hit_pct = 0;
  char hit_pct_str[20];
  if (stats.m_page_hits + stats.m_page_faults != 0)
    hit_pct = 10000 * stats.m_page_hits /
              (stats.m_page_hits + stats.m_page_faults);
  sprintf(hit_pct_str, "%u.%02u", hit_pct / 100, hit_pct % 100);

  D("loop"
    << " stats:" << m_stats_loop_on
    << " busy:" << m_busy_loop_on
    << " cleanup:" << m_cleanup_loop_on
    << " lcp:" << Uint32(m_lcp_state));

  D("page"
    << " entries:" << pl_hash.count()
    << " pages:" << stats.m_num_pages << "/" << param.m_max_pages
    << " mapped:" << is_mapped
    << " hot:" << is_hot
    << " io:" << stats.m_current_io_waits << "/" << param.m_max_io_waits
    << " hit pct:" << hit_pct_str);

  D("list"
    << " locked:" << is_locked
    << " stack:" << pl_stack.count()
    << " queue:" << pl_queue.count()
    << " to queue:" << to_queue);

  D(sublist_info);
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
  }
  return out;
}

NdbOut&
operator<<(NdbOut& out, Ptr<Pgman::Page_entry> ptr)
{
  const Pgman::Page_entry pe = *ptr.p;
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
  Page_hashlist& pl_hash = m_page_hashlist;
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
      infoEvent(" PE [ file: %d page: %d ] state: %x lsn: %lld lcp: %d busy: %d req-list: %d",
		ptr.p->m_file_no, ptr.p->m_page_no,
		ptr.p->m_state, ptr.p->m_lsn, ptr.p->m_last_lcp,
		ptr.p->m_busy_count,
		!ptr.p->m_requests.isEmpty());
    }
  }

  if (signal->theData[0] == 11002 && signal->getLength() == 3)
  {
    Page_entry key;
    key.m_file_no = signal->theData[1];
    key.m_page_no = signal->theData[2];
    
    Ptr<Page_entry> ptr;
    if (pl_hash.find(ptr, key))
    {
      ndbout << "pageout " << ptr << endl;
      if (c_tup != 0)
        c_tup->disk_page_unmap_callback(0,
                                        ptr.p->m_real_page_i, 
                                        ptr.p->m_dirty_count);
      pageout(signal, ptr);
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

  if (signal->theData[0] == 11004)
  {
    ndbout << "Dump LCP bucket m_lcp_outstanding: " << m_lcp_outstanding;
    if (m_lcp_curr_bucket != ~(Uint32)0)
    {
      Page_hashlist::Iterator iter;
      pl_hash.next(m_lcp_curr_bucket, iter);
      
      ndbout_c(" %d", m_lcp_curr_bucket);

      while (iter.curr.i != RNIL && iter.bucket == m_lcp_curr_bucket)
      {
	Ptr<Page_entry>& ptr = iter.curr;
	ndbout << ptr << endl;
	pl_hash.next(iter);
      }

      ndbout_c("-- done");
    }
    else
    {
      ndbout_c(" == ~0");
    }
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
    int pages = m_param.m_max_pages;
    int size = m_page_entry_pool.getSize();
    int used = m_page_entry_pool.getUsed();
    int usedpct = size ? ((100 * used) / size) : 0;
    int high = m_stats.m_entries_high;
    int highpct = size ? ((100 * high) / size) : 0;
    ndbout << "pgman(" << instance() << ")";
    ndbout << " pages: " << pages << " entries: " << size;
    ndbout << " used: " << used << " (" << usedpct << "%)";
    ndbout << " high: " << high << " (" << highpct << "%)";
    ndbout << endl;
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
