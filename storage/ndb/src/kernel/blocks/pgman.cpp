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

#include "pgman.hpp"
#include <signaldata/FsRef.hpp>
#include <signaldata/FsConf.hpp>
#include <signaldata/FsReadWriteReq.hpp>
#include <signaldata/PgmanContinueB.hpp>
#include <signaldata/LCP.hpp>

#include <dbtup/Dbtup.hpp>

#include <DebuggerNames.hpp>

/**
 * Requests that make page dirty
 */
#define DIRTY_FLAGS (Page_request::COMMIT_REQ | \
                     Page_request::DIRTY_REQ | \
                     Page_request::ALLOC_REQ)

// todo use this
#ifdef VM_TRACE
#define dbg(x) \
  do { if (! debugFlag) break; debugOut << "PGMAN: " << x << endl; } while (0)
#else
#define dbg(x)
#endif

static bool g_dbg_lcp = false;
#if 1
#define DBG_LCP(x)
#else
#define DBG_LCP(x) if(g_dbg_lcp) ndbout << x
#endif

Pgman::Pgman(Block_context& ctx) :
  SimulatedBlock(PGMAN, ctx),
  m_file_map(m_data_buffer_pool),
  m_page_hashlist(m_page_entry_pool),
  m_page_stack(m_page_entry_pool),
  m_page_queue(m_page_entry_pool)
#ifdef VM_TRACE
  ,debugOut(* new NullOutputStream())
  ,debugFlag(false)
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
  addRecSignal(GSN_END_LCP_REQ, &Pgman::execEND_LCP_REQ);
  
  // loop status
  m_stats_loop_on = false;
  m_busy_loop_on = false;
  m_cleanup_loop_on = false;
  m_lcp_loop_on = false;

  // LCP variables
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
    page_buffer /= GLOBAL_PAGE_SIZE; // in pages
    m_page_entry_pool.setSize(100*page_buffer);
    m_param.m_max_pages = page_buffer;
    m_param.m_max_hot_pages = (page_buffer * 9) / 10;
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
  m_max_hot_pages(56),
  m_max_loop_count(256),
  m_max_io_waits(64),
  m_stats_loop_delay(1000),
  m_cleanup_loop_delay(200),
  m_lcp_loop_delay(0)
{
}

Pgman::Stats::Stats() :
  m_num_pages(0),
  m_page_hits(0),
  m_page_faults(0),
  m_current_io_waits(0)
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
      Lgman* lgman = (Lgman*)globalData.getBlock(LGMAN);
      new (&m_lgman) Logfile_client(this, lgman, 0);
      c_tup = (Dbtup*)globalData.getBlock(DBTUP);
    }
    break;
  case 3:
    {
      // start forever loops
      do_stats_loop(signal);
      do_cleanup_loop(signal);
      m_stats_loop_on = true;
      m_cleanup_loop_on = true;
    }
    break;
  case 7:
    break;
  default:
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
  signal->theData[5] = 7;
  signal->theData[6] = 255; // No more start phases from missra
  sendSignal(NDBCNTR_REF, GSN_STTORRY, signal, 7, JBB);
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
    do_busy_loop(signal);
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
      pl.getPtr(ptr, data1);
      process_lcp_locked(signal, ptr);
    }
    else
    {
      signal->theData[0] = m_end_lcp_req.senderData;
      sendSignal(m_end_lcp_req.senderRef, GSN_END_LCP_CONF, signal, 1, JBB);
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
  m_state(0),
  m_file_no(file_no),
  m_page_no(page_no),
  m_real_page_i(RNIL),
  m_copy_page_i(RNIL),
  m_lsn(0),
  m_last_lcp(0),
  m_dirty_count(0),
  m_busy_count(0),
  m_requests()
{
}

// page lists

Uint32
Pgman::get_sublist_no(Page_state state)
{
  if (state == 0)
  {
    return ZNIL;
  }
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
  return Page_entry::SL_OTHER;
}

void
Pgman::set_page_state(Ptr<Page_entry> ptr, Page_state new_state)
{
#ifdef VM_TRACE
  debugOut << "PGMAN: >set_page_state: state=" << hex << new_state << endl;
  debugOut << "PGMAN: " << ptr << ": before" << endl;
#endif

  Page_state old_state = ptr.p->m_state;
  if (old_state != new_state)
  {
    Uint32 old_list_no = get_sublist_no(old_state);
    Uint32 new_list_no = get_sublist_no(new_state);
    if (old_state != 0)
    {
      ndbrequire(old_list_no != ZNIL);
      if (old_list_no != new_list_no)
      {
        Page_sublist& old_list = *m_page_sublist[old_list_no];
        old_list.remove(ptr);
      }
    }
    if (new_state != 0)
    {
      ndbrequire(new_list_no != ZNIL);
      if (old_list_no != new_list_no)
      {
        Page_sublist& new_list = *m_page_sublist[new_list_no];
        new_list.add(ptr);
      }
    }
    ptr.p->m_state = new_state;
  }

#ifdef VM_TRACE
  debugOut << "PGMAN: " << ptr << ": after" << endl;
  debugOut << "PGMAN: <set_page_state" << endl;
#endif
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
#ifdef VM_TRACE
    debugOut << "PGMAN: find_page_entry" << endl;
    debugOut << "PGMAN: " << ptr << endl;
#endif
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
    debugOut << "PGMAN: seize_page_entry" << endl;
    debugOut << "PGMAN: " << ptr << endl;
#endif

    return true;
  }
  return false;
}

bool
Pgman::get_page_entry(Ptr<Page_entry>& ptr, Uint32 file_no, Uint32 page_no)
{
  if (find_page_entry(ptr, file_no, page_no))
  {
    ndbrequire(ptr.p->m_state != 0);
    m_stats.m_page_hits++;
    return true;
  }

  if (seize_page_entry(ptr, file_no, page_no))
  {
    ndbrequire(ptr.p->m_state == 0);
    m_stats.m_page_faults++;
    return true;
  }

  ndbrequire(false);
  
  return false;
}

void
Pgman::release_page_entry(Ptr<Page_entry>& ptr)
{
#ifdef VM_TRACE
  debugOut << "PGMAN: release_page_entry" << endl;
  debugOut << "PGMAN: " << ptr << endl;
#endif
  Page_state state = ptr.p->m_state;

  ndbrequire(ptr.p->m_requests.isEmpty());

  ndbrequire(! (state & Page_entry::ONSTACK));
  ndbrequire(! (state & Page_entry::ONQUEUE));
  ndbrequire(ptr.p->m_real_page_i == RNIL);

  if (! (state & Page_entry::LOCKED))
    ndbrequire(! (state & Page_entry::REQUEST));
  
  set_page_state(ptr, 0);
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
#ifdef VM_TRACE
  debugOut << "PGMAN: >lirs_stack_prune" << endl;
#endif
  Page_stack& pl_stack = m_page_stack;
  Page_queue& pl_queue = m_page_queue;
  Ptr<Page_entry> ptr;

  while (pl_stack.first(ptr))      // first is stack bottom
  {
    Page_state state = ptr.p->m_state;
    if (state & Page_entry::HOT)
    {
      jam();
      break;
    }

#ifdef VM_TRACE
  debugOut << "PGMAN: " << ptr << ": prune from stack" << endl;
#endif

    pl_stack.remove(ptr);
    state &= ~ Page_entry::ONSTACK;
    set_page_state(ptr, state);

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
#ifdef VM_TRACE
  debugOut << "PGMAN: <lirs_stack_prune" << endl;
#endif
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
#ifdef VM_TRACE
  debugOut << "PGMAN: lirs_stack_pop" << endl;
#endif
  Page_stack& pl_stack = m_page_stack;
  Page_queue& pl_queue = m_page_queue;

  Ptr<Page_entry> ptr;
  bool ok = pl_stack.first(ptr);
  ndbrequire(ok);
  Page_state state = ptr.p->m_state;

#ifdef VM_TRACE
  debugOut << "PGMAN: " << ptr << ": pop from stack" << endl;
#endif

  ndbrequire(state & Page_entry::HOT);
  ndbrequire(state & Page_entry::ONSTACK);
  pl_stack.remove(ptr);
  state &= ~ Page_entry::HOT;
  state &= ~ Page_entry::ONSTACK;
  ndbrequire(! (state & Page_entry::ONQUEUE));

  if (state & Page_entry::BOUND)
  {
    jam();
    pl_queue.add(ptr);
    state |= Page_entry::ONQUEUE;
  }
  else
  {
    // enters queue at bind
    jam();
    ndbrequire(state & Page_entry::REQUEST);
  }

  set_page_state(ptr, state);
  lirs_stack_prune();
}

/*
 * Update LIRS lists when page is referenced.
 */
void
Pgman::lirs_reference(Ptr<Page_entry> ptr)
{
#ifdef VM_TRACE
  debugOut << "PGMAN: >lirs_reference" << endl;
  debugOut << "PGMAN: " << ptr << endl;
#endif
  Page_stack& pl_stack = m_page_stack;
  Page_queue& pl_queue = m_page_queue;

  Page_state state = ptr.p->m_state;
  ndbrequire(! (state & Page_entry::LOCKED));

  // even non-LIRS cache pages are counted on l.h.s.
  if (m_stats.m_num_pages >= m_param.m_max_hot_pages)
  {
    if (state & Page_entry::HOT)
    {
      // case 1
      jam();
      ndbrequire(state & Page_entry::ONSTACK);
      bool at_bottom = ! pl_stack.hasPrev(ptr);
      pl_stack.remove(ptr);
      pl_stack.add(ptr);
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
      pl_stack.add(ptr);
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
      pl_stack.add(ptr);
      state |= Page_entry::ONSTACK;
      if (state & Page_entry::ONQUEUE)
      {
        jam();
        move_cleanup_ptr(ptr);
        pl_queue.remove(ptr);
      }
      if (state & Page_entry::BOUND)
      {
        jam();
        pl_queue.add(ptr);
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
#ifdef VM_TRACE
    debugOut << "PGMAN: filling up initial hot pages: "
             << m_stats.m_num_pages << " of "
             << m_param.m_max_hot_pages << endl;
#endif
    jam();
    if (state & Page_entry::ONSTACK)
    {
      jam();
      pl_stack.remove(ptr);
    }
    pl_stack.add(ptr);
    state |= Page_entry::ONSTACK;
    state |= Page_entry::HOT;
  }

  set_page_state(ptr, state);
#ifdef VM_TRACE
  debugOut << "PGMAN: <lirs_reference" << endl;
#endif
}

// continueB loops

void
Pgman::do_stats_loop(Signal* signal)
{
#ifdef VM_TRACE
  debugOut << "PGMAN: do_stats_loop" << endl;
  verify_all();
#endif
  Uint32 delay = m_param.m_stats_loop_delay;
  signal->theData[0] = PgmanContinueB::STATS_LOOP;
  sendSignalWithDelay(PGMAN_REF, GSN_CONTINUEB, signal, delay, 1);
}

void
Pgman::do_busy_loop(Signal* signal, bool direct)
{
#ifdef VM_TRACE
  debugOut << "PGMAN: >do_busy_loop on=" << m_busy_loop_on
           << " direct=" << direct << endl;
#endif
  Uint32 restart = false;
  if (direct)
  {
    // may not cover the calling entry
    (void)process_bind(signal);
    (void)process_map(signal);
    // callback must be queued
    if (! m_busy_loop_on)
    {
      restart = true;
      m_busy_loop_on = true;
    }
  }
  else
  {
    ndbrequire(m_busy_loop_on);
    restart += process_bind(signal);
    restart += process_map(signal);
    restart += process_callback(signal);
    if (! restart)
    {
      m_busy_loop_on = false;
    }
  }
  if (restart)
  {
    signal->theData[0] = PgmanContinueB::BUSY_LOOP;
    sendSignal(PGMAN_REF, GSN_CONTINUEB, signal, 1, JBB);
  }
#ifdef VM_TRACE
  debugOut << "PGMAN: <do_busy_loop on=" << m_busy_loop_on
           << " restart=" << restart << endl;
#endif
}

void
Pgman::do_cleanup_loop(Signal* signal)
{
#ifdef VM_TRACE
  debugOut << "PGMAN: do_cleanup_loop" << endl;
#endif
  process_cleanup(signal);

  Uint32 delay = m_param.m_cleanup_loop_delay;
  signal->theData[0] = PgmanContinueB::CLEANUP_LOOP;
  sendSignalWithDelay(PGMAN_REF, GSN_CONTINUEB, signal, delay, 1);
}

void
Pgman::do_lcp_loop(Signal* signal, bool direct)
{
#ifdef VM_TRACE
  debugOut << "PGMAN: >do_lcp_loop on=" << m_lcp_loop_on
           << " direct=" << direct << endl;
#endif
  Uint32 restart = false;
  if (direct)
  {
    ndbrequire(! m_lcp_loop_on);
    restart = true;
    m_lcp_loop_on = true;
  }
  else
  {
    ndbrequire(m_lcp_loop_on);
    restart += process_lcp(signal);
    if (! restart)
    {
      m_lcp_loop_on = false;
    }
  }
  if (restart)
  {
    Uint32 delay = m_param.m_lcp_loop_delay;
    signal->theData[0] = PgmanContinueB::LCP_LOOP;
    if (delay)
      sendSignalWithDelay(PGMAN_REF, GSN_CONTINUEB, signal, delay, 1);
    else
      sendSignal(PGMAN_REF, GSN_CONTINUEB, signal, 1, JBB);
  }
#ifdef VM_TRACE
  debugOut << "PGMAN: <do_lcp_loop on=" << m_lcp_loop_on
           << " restart=" << restart << endl;
#endif
}

// busy loop

bool
Pgman::process_bind(Signal* signal)
{
#ifdef VM_TRACE
  debugOut << "PGMAN: >process_bind" << endl;
#endif
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
#ifdef VM_TRACE
  debugOut << "PGMAN: <process_bind" << endl;
#endif
  return ! pl_bind.isEmpty();
}

bool
Pgman::process_bind(Signal* signal, Ptr<Page_entry> ptr)
{
#ifdef VM_TRACE
  debugOut << "PGMAN: " << ptr << " : process_bind" << endl;
#endif
  Page_sublist& pl_bind = *m_page_sublist[Page_entry::SL_BIND];
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
#ifdef VM_TRACE
      debugOut << "PGMAN: bind failed: queue empty" << endl;
#endif
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
#ifdef VM_TRACE
      debugOut << "PGMAN: bind failed: queue front not evictable" << endl;
      debugOut << "PGMAN: " << clean_ptr << endl;
#endif
      // XXX busy loop
      return false;
    }

#ifdef VM_TRACE
    debugOut << "PGMAN: " << clean_ptr << " : evict" << endl;
#endif

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

    set_page_state(clean_ptr, clean_state);

    if (! (clean_state & Page_entry::ONSTACK))
      release_page_entry(clean_ptr);

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

#ifdef VM_TRACE
    debugOut << "PGMAN: " << ptr << " : add to queue at bind" << endl;
#endif

    pl_queue.add(ptr);
    state |= Page_entry::ONQUEUE;
  }

  set_page_state(ptr, state);
  return true;
}

bool
Pgman::process_map(Signal* signal)
{
#ifdef VM_TRACE
  debugOut << "PGMAN: >process_map" << endl;
#endif
  int max_count = m_param.m_max_io_waits - m_stats.m_current_io_waits;
  if (max_count > 0)
    max_count = max_count / 2 + 1;
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
#ifdef VM_TRACE
  debugOut << "PGMAN: <process_map" << endl;
#endif
  return ! pl_map.isEmpty();
}

bool
Pgman::process_map(Signal* signal, Ptr<Page_entry> ptr)
{
#ifdef VM_TRACE
  debugOut << "PGMAN: " << ptr << " : process_map" << endl;
#endif
  pagein(signal, ptr);
  return true;
}

bool
Pgman::process_callback(Signal* signal)
{
#ifdef VM_TRACE
  debugOut << "PGMAN: >process_callback" << endl;
#endif
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
#ifdef VM_TRACE
  debugOut << "PGMAN: <process_callback" << endl;
#endif
  return ! pl_callback.isEmpty();
}

bool
Pgman::process_callback(Signal* signal, Ptr<Page_entry> ptr)
{
#ifdef VM_TRACE
  debugOut << "PGMAN: " << ptr << " : process_callback" << endl;
#endif
  int max_count = 1;
  Page_state state = ptr.p->m_state;

  while (! ptr.p->m_requests.isEmpty() && --max_count >= 0)
  {
    jam();
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
#ifdef VM_TRACE
      debugOut << "PGMAN: " << req_ptr << " : process_callback" << endl;
#endif

#ifdef ERROR_INSERT
      if (req_ptr.p->m_flags & Page_request::DELAY_REQ)
      {
	Uint64 now = NdbTick_CurrentMillisecond();
	if (now < req_ptr.p->m_delay_until_time)
	{
	  break;
	}
      }
#endif
      
      b = globalData.getBlock(req_ptr.p->m_block);
      callback = req_ptr.p->m_callback;
      
      if (req_ptr.p->m_flags & DIRTY_FLAGS)
      {
        jam();
        state |= Page_entry::DIRTY;
	ndbassert(ptr.p->m_dirty_count);
	ptr.p->m_dirty_count --;
      }

      req_list.releaseFirst(req_ptr);
    }
    ndbrequire(state & Page_entry::BOUND);
    ndbrequire(state & Page_entry::MAPPED);
    
    // callback may re-enter PGMAN and change page state
    set_page_state(ptr, state);
    b->execute(signal, callback, ptr.p->m_real_page_i);
    state = ptr.p->m_state;
  }
  
  if (ptr.p->m_requests.isEmpty())
  {
    jam();
    state &= ~ Page_entry::REQUEST;
  }
  set_page_state(ptr, state);
  return true;
}

// cleanup loop

bool
Pgman::process_cleanup(Signal* signal)
{
#ifdef VM_TRACE
  debugOut << "PGMAN: >process_cleanup" << endl;
#endif
  Page_queue& pl_queue = m_page_queue;

  // XXX for now start always from beginning
  m_cleanup_ptr.i = RNIL;

  if (m_cleanup_ptr.i == RNIL && ! pl_queue.first(m_cleanup_ptr))
  {
    jam();
#ifdef VM_TRACE
    debugOut << "PGMAN: <process_cleanup: empty queue" << endl;
#endif
    return false;
  }

  int max_loop_count = m_param.m_max_loop_count;
  int max_count = m_param.m_max_io_waits - m_stats.m_current_io_waits;

  if (max_count > 0)
  {
    max_count = max_count / 2 + 1;
    /*
     * Possibly add code here to avoid writing too rapidly.  May be
     * unnecessary since only cold pages are cleaned.
     */
  }

  Ptr<Page_entry> ptr = m_cleanup_ptr;
  while (max_loop_count != 0 && max_count != 0)
  {
    Page_state state = ptr.p->m_state;
    ndbrequire(! (state & Page_entry::LOCKED));
    if (state & Page_entry::BUSY)
    {
#ifdef VM_TRACE
      debugOut << "PGMAN: process_cleanup: break on busy page" << endl;
      debugOut << "PGMAN: " << ptr << endl;
#endif
      break;
    }
    if (state & Page_entry::DIRTY &&
        ! (state & Page_entry::PAGEIN) &&
        ! (state & Page_entry::PAGEOUT))
    {
#ifdef VM_TRACE
      debugOut << "PGMAN: " << ptr << " : process_cleanup" << endl;
#endif
      c_tup->disk_page_unmap_callback(ptr.p->m_real_page_i, 
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
#ifdef VM_TRACE
  debugOut << "PGMAN: <process_cleanup" << endl;
#endif
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
  LcpFragOrd* ord = (LcpFragOrd*)signal->getDataPtr();
  ndbrequire(ord->lcpId >= m_last_lcp_complete + 1 || m_last_lcp_complete == 0);
  m_last_lcp = ord->lcpId;
  DBG_LCP("Pgman::execLCP_FRAG_ORD lcp: " << m_last_lcp << endl);
  
#ifdef VM_TRACE
  debugOut
    << "PGMAN: execLCP_FRAG_ORD"
    << " this=" << m_last_lcp << " last_complete=" << m_last_lcp_complete
    << " bucket=" << m_lcp_curr_bucket << endl;
#endif
}

void
Pgman::execEND_LCP_REQ(Signal* signal)
{
  EndLcpReq* req = (EndLcpReq*)signal->getDataPtr();
  m_end_lcp_req = *req;

  DBG_LCP("execEND_LCP_REQ" << endl);

  ndbrequire(!m_lcp_outstanding);
  m_lcp_curr_bucket = 0;
  
#ifdef VM_TRACE
  debugOut
    << "PGMAN: execEND_LCP_REQ"
    << " this=" << m_last_lcp << " last_complete=" << m_last_lcp_complete
    << " bucket=" << m_lcp_curr_bucket
    << " outstanding=" << m_lcp_outstanding << endl;
#endif

  m_last_lcp_complete = m_last_lcp;
  
  do_lcp_loop(signal, true);
}

bool
Pgman::process_lcp(Signal* signal)
{
  Page_hashlist& pl_hash = m_page_hashlist;
  int max_count = m_param.m_max_io_waits - m_stats.m_current_io_waits;
  if (max_count > 0)
    max_count = max_count / 2 + 1;

#ifdef VM_TRACE
  debugOut
    << "PGMAN: process_lcp"
    << " this=" << m_last_lcp << " last_complete=" << m_last_lcp_complete
    << " bucket=" << m_lcp_curr_bucket
    << " outstanding=" << m_lcp_outstanding << endl;
#endif

  // start or re-start from beginning of current hash bucket
  if (m_lcp_curr_bucket != ~(Uint32)0)
  {
    Page_hashlist::Iterator iter;
    pl_hash.next(m_lcp_curr_bucket, iter);
    Uint32 loop = 0;
    while (iter.curr.i != RNIL && 
	   m_lcp_outstanding < max_count &&
	   (loop ++ < 32 || iter.bucket == m_lcp_curr_bucket))
    {
      Ptr<Page_entry>& ptr = iter.curr;
      Page_state state = ptr.p->m_state;
      
      DBG_LCP("LCP " << ptr << " - ");
      
      if (ptr.p->m_last_lcp < m_last_lcp &&
          (state & Page_entry::DIRTY) &&
	  (! (state & Page_entry::LOCKED)))
      {
        if(! (state & Page_entry::BOUND))
        {
          ndbout << ptr << endl;
          ndbrequire(false);
        }
        if (state & Page_entry::BUSY)
        {
	  DBG_LCP(" BUSY" << endl);
          break;  // wait for it
        } 
	else if (state & Page_entry::PAGEOUT)
        {
	  DBG_LCP(" PAGEOUT -> state |= LCP" << endl);
          set_page_state(ptr, state | Page_entry::LCP);
        }
        else
        {
	  DBG_LCP(" pageout()" << endl);
          ptr.p->m_state |= Page_entry::LCP;
	  c_tup->disk_page_unmap_callback(ptr.p->m_real_page_i, 
					  ptr.p->m_dirty_count);
          pageout(signal, ptr);
        }
        ptr.p->m_last_lcp = m_last_lcp;
        m_lcp_outstanding++;
      }
      else
      {
	DBG_LCP(" NOT DIRTY" << endl);
      }	
      pl_hash.next(iter);
    }
    
    m_lcp_curr_bucket = (iter.curr.i != RNIL ? iter.bucket : ~(Uint32)0);
  }

  if (m_lcp_curr_bucket == ~(Uint32)0  && !m_lcp_outstanding)
  {
    Ptr<Page_entry> ptr;
    Page_sublist& pl = *m_page_sublist[Page_entry::SL_LOCKED];
    if (pl.first(ptr))
    {
      process_lcp_locked(signal, ptr);
    }
    else
    {
      signal->theData[0] = m_end_lcp_req.senderData;
      sendSignal(m_end_lcp_req.senderRef, GSN_END_LCP_CONF, signal, 1, JBB);
    }
    return false;
  }
  
  return true;
}

void
Pgman::process_lcp_locked(Signal* signal, Ptr<Page_entry> ptr)
{
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
#ifdef VM_TRACE
  debugOut << "PGMAN: pagein" << endl;
  debugOut << "PGMAN: " << ptr << endl;
#endif

  ndbrequire(! (ptr.p->m_state & Page_entry::PAGEIN));
  set_page_state(ptr, ptr.p->m_state | Page_entry::PAGEIN);

  fsreadreq(signal, ptr);
  m_stats.m_current_io_waits++;
}

void
Pgman::fsreadconf(Signal* signal, Ptr<Page_entry> ptr)
{
#ifdef VM_TRACE
  debugOut << "PGMAN: fsreadconf" << endl;
  debugOut << "PGMAN: " << ptr << endl;
#endif
  ndbrequire(ptr.p->m_state & Page_entry::PAGEIN);
  Page_state state = ptr.p->m_state;

  state &= ~ Page_entry::PAGEIN;
  state &= ~ Page_entry::EMPTY;
  state |= Page_entry::MAPPED;
  set_page_state(ptr, state);

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

  ptr.p->m_last_lcp = m_last_lcp_complete;
  do_busy_loop(signal, true);
}

void
Pgman::pageout(Signal* signal, Ptr<Page_entry> ptr)
{
#ifdef VM_TRACE
  debugOut << "PGMAN: pageout" << endl;
  debugOut << "PGMAN: " << ptr << endl;
#endif
  
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
  page->m_page_header.m_page_lsn_hi = ptr.p->m_lsn >> 32;
  page->m_page_header.m_page_lsn_lo = ptr.p->m_lsn & 0xFFFFFFFF;

  // undo WAL
  Logfile_client::Request req;
  req.m_callback.m_callbackData = ptr.i;
  req.m_callback.m_callbackFunction = safe_cast(&Pgman::logsync_callback);
  int ret = m_lgman.sync_lsn(signal, ptr.p->m_lsn, &req, 0);
  if (ret > 0)
  {
    fswritereq(signal, ptr);
    m_stats.m_current_io_waits++;
  }
  else
  {
    ndbrequire(ret == 0);
    state |= Page_entry::LOGSYNC;
  }
  set_page_state(ptr, state);
}

void
Pgman::logsync_callback(Signal* signal, Uint32 ptrI, Uint32 res)
{
  Ptr<Page_entry> ptr;
  m_page_entry_pool.getPtr(ptr, ptrI);

#ifdef VM_TRACE
  debugOut << "PGMAN: logsync_callback" << endl;
  debugOut << "PGMAN: " << ptr << endl;
#endif

  // it is OK to be "busy" at this point (the commit is queued)
  Page_state state = ptr.p->m_state;
  ndbrequire(state & Page_entry::PAGEOUT);
  ndbrequire(state & Page_entry::LOGSYNC);
  state &= ~ Page_entry::LOGSYNC;
  set_page_state(ptr, state);

  fswritereq(signal, ptr);
  m_stats.m_current_io_waits++;
}

void
Pgman::fswriteconf(Signal* signal, Ptr<Page_entry> ptr)
{
#ifdef VM_TRACE
  debugOut << "PGMAN: fswriteconf" << endl;
  debugOut << "PGMAN: " << ptr << endl;
#endif

  Page_state state = ptr.p->m_state;
  ndbrequire(state & Page_entry::PAGEOUT);

  state &= ~ Page_entry::PAGEOUT;
  state &= ~ Page_entry::EMPTY;
  state &= ~ Page_entry::DIRTY;

  ndbrequire(m_stats.m_current_io_waits > 0);
  m_stats.m_current_io_waits--;

  if (state & Page_entry::LCP)
  {
    ndbrequire(m_lcp_outstanding);
    m_lcp_outstanding--;
    state &= ~ Page_entry::LCP;
    
    if (ptr.p->m_copy_page_i != RNIL)
    {
      process_lcp_locked_fswriteconf(signal, ptr);
    }
  }
  
  set_page_state(ptr, state);
  do_busy_loop(signal, true);
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
  
  sendSignal(NDBFS_REF, GSN_FSWRITEREQ, signal,
	     FsReadWriteReq::FixedLength + 1, JBA);
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
Pgman::get_page(Signal* signal, Ptr<Page_entry> ptr, Page_request page_req)
{
#ifdef VM_TRACE
  Ptr<Page_request> tmp = { &page_req, RNIL};
  debugOut << "PGMAN: >get_page" << endl;
  debugOut << "PGMAN: " << ptr << endl;
  debugOut << "PGMAN: " << tmp << endl;
#endif
  Uint32 req_flags = page_req.m_flags;

  if (req_flags & Page_request::EMPTY_PAGE)
  {
    // Only one can "init" a page at a time
    //ndbrequire(ptr.p->m_requests.isEmpty());
  }

  Page_state state = ptr.p->m_state;
  bool is_new = (state == 0);
  bool busy_count = false;

  if (req_flags & Page_request::LOCK_PAGE)
  {
    jam();
    state |= Page_entry::LOCKED;
  }
  
  if (req_flags & Page_request::ALLOC_REQ)
  {
    jam();
  }
  else if (req_flags & Page_request::COMMIT_REQ)
  {
    busy_count = true;
    state |= Page_entry::BUSY;
  }
  else if ((req_flags & Page_request::OP_MASK) != ZREAD)
  {
    jam();
  }

  // update LIRS
  if (! (state & Page_entry::LOCKED) &&
      ! (req_flags & Page_request::CORR_REQ))
  {
    jam();
    set_page_state(ptr, state);
    lirs_reference(ptr);
    state = ptr.p->m_state;
  }

  const Page_state LOCKED = Page_entry::LOCKED | Page_entry::MAPPED;
  if ((state & LOCKED) == LOCKED && 
      ! (req_flags & Page_request::UNLOCK_PAGE))
  {
    ptr.p->m_state |= (req_flags & DIRTY_FLAGS ? Page_entry::DIRTY : 0);
    if (ptr.p->m_copy_page_i != RNIL)
    {
      return ptr.p->m_copy_page_i;
    }
    
    return ptr.p->m_real_page_i;
  }
  
  bool only_request = ptr.p->m_requests.isEmpty();
#ifdef ERROR_INSERT
  if (req_flags & Page_request::DELAY_REQ)
  {
    jam();
    only_request = false;
  }
#endif  
  if (only_request &&
      state & Page_entry::MAPPED)
  {
    if (! (state & Page_entry::PAGEOUT))
    {
      if (req_flags & DIRTY_FLAGS)
	state |= Page_entry::DIRTY;
      
      ptr.p->m_busy_count += busy_count;
      set_page_state(ptr, state);
      
#ifdef VM_TRACE
      debugOut << "PGMAN: <get_page: immediate" << endl;
#endif

      ndbrequire(ptr.p->m_real_page_i != RNIL);
      return ptr.p->m_real_page_i;
    }
  }

  if (! (req_flags & (Page_request::LOCK_PAGE | Page_request::UNLOCK_PAGE)))
  {
    ndbrequire(! (state & Page_entry::LOCKED));
  }

  // queue the request
  Ptr<Pgman::Page_request> req_ptr;
  {
    Local_page_request_list req_list(m_page_request_pool, ptr.p->m_requests);
    if (! (req_flags & Page_request::ALLOC_REQ))
      req_list.seizeLast(req_ptr);
    else
      req_list.seizeFirst(req_ptr);
  }
  
  if (req_ptr.i == RNIL)
  {
    if (is_new)
    {
      release_page_entry(ptr);
    }
    return -1;
  }

  req_ptr.p->m_block = page_req.m_block;
  req_ptr.p->m_flags = page_req.m_flags;
  req_ptr.p->m_callback = page_req.m_callback;
#ifdef ERROR_INSERT
  req_ptr.p->m_delay_until_time = page_req.m_delay_until_time;
#endif
  
  state |= Page_entry::REQUEST;
  if (only_request && req_flags & Page_request::EMPTY_PAGE)
  {
    state |= Page_entry::EMPTY;
  }

  if (req_flags & Page_request::UNLOCK_PAGE)
  {
    // keep it locked
  }
  
  ptr.p->m_busy_count += busy_count;
  ptr.p->m_dirty_count += !!(req_flags & DIRTY_FLAGS);
  set_page_state(ptr, state);
  
  do_busy_loop(signal, true);

#ifdef VM_TRACE
  debugOut << "PGMAN: " << req_ptr << endl;
  debugOut << "PGMAN: <get_page: queued" << endl;
#endif
  return 0;
}

void
Pgman::update_lsn(Ptr<Page_entry> ptr, Uint32 block, Uint64 lsn)
{
#ifdef VM_TRACE
  const char* bname = getBlockName(block, "?");
  debugOut << "PGMAN: >update_lsn: block=" << bname << " lsn=" << lsn << endl;
  debugOut << "PGMAN: " << ptr << endl;
#endif

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
  set_page_state(ptr, state);
  
#ifdef VM_TRACE
  debugOut << "PGMAN: " << ptr << endl;
  debugOut << "PGMAN: <update_lsn" << endl;
#endif
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
	return it.pos;
      }
    } while(m_file_map.next(it));
  }

  Uint32 file_no = m_file_map.getSize();
  Uint32 fd = (1u << 31) | file_no;

  if (m_file_map.append(&fd, 1))
  {
    return file_no;
  }
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
	return RNIL;
    }
  }

  File_map::DataBufferIterator it;
  m_file_map.first(it);
  m_file_map.next(it, file_no);
  if (* it.data != RNIL)
    return RNIL;

  *it.data = (1u << 31) | file_no;
  return file_no;
}

void
Pgman::map_file_no(Uint32 file_no, Uint32 fd)
{
  File_map::DataBufferIterator it;
  m_file_map.first(it);
  m_file_map.next(it, file_no);

  assert(*it.data == ((1u << 31) | file_no));
  *it.data = fd;
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
}

int
Pgman::drop_page(Ptr<Page_entry> ptr)
{
  Page_stack& pl_stack = m_page_stack;
  Page_queue& pl_queue = m_page_queue;

  Page_state state = ptr.p->m_state;
  if (! (state & (Page_entry::PAGEIN | Page_entry::PAGEOUT)))
  {
    ndbrequire(state & Page_entry::BOUND);
    ndbrequire(state & Page_entry::MAPPED);

    if (state & Page_entry::ONSTACK)
    {
      jam();
      pl_stack.remove(ptr);
      state &= ~ Page_entry::ONSTACK;
    }

    if (state & Page_entry::ONQUEUE)
    {
      jam();
      pl_queue.remove(ptr);
      state &= ~ Page_entry::ONQUEUE;
    }

    if (ptr.p->m_real_page_i != RNIL)
    {
      jam();
      release_cache_page(ptr.p->m_real_page_i);
      ptr.p->m_real_page_i = RNIL;
    }

    set_page_state(ptr, state);
    release_page_entry(ptr);
    return 1;
  }
  
  ndbrequire(false);
  return -1;
}

// debug

#ifdef VM_TRACE

void
Pgman::verify_page_entry(Ptr<Page_entry> ptr)
{
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

  bool on_queue = state & Page_entry::ONQUEUE;
  // hot entry is not on queue
  ndbrequire(! is_hot || ! on_queue || dump_page_lists(ptrI));

  bool is_locked = state & Page_entry::LOCKED;
  bool on_queue2 = ! is_locked && ! is_hot && is_bound;
  ndbrequire(on_queue == on_queue2 || dump_page_lists(ptrI));

  // entries waiting to enter queue
  bool to_queue = ! is_locked && ! is_hot && ! is_bound && has_req;

  // page is either LOCKED or under LIRS
  bool is_lirs = on_stack || to_queue || on_queue;
  ndbrequire(is_locked == ! is_lirs || dump_page_lists(ptrI));

  bool pagein = state & Page_entry::PAGEIN;
  bool pageout = state & Page_entry::PAGEOUT;
  // cannot read and write at same time
  ndbrequire(! pagein || ! pageout || dump_page_lists(ptrI));

  Uint32 no = get_sublist_no(state);
  switch (no) {
  case Page_entry::SL_BIND:
    ndbrequire(! pagein && ! pageout || dump_page_lists(ptrI));
    break;
  case Page_entry::SL_MAP:
    ndbrequire(! pagein && ! pageout || dump_page_lists(ptrI));
    break;
  case Page_entry::SL_MAP_IO:
    ndbrequire(pagein && ! pageout || dump_page_lists(ptrI));
    break;
  case Page_entry::SL_CALLBACK:
    ndbrequire(! pagein && ! pageout || dump_page_lists(ptrI));
    break;
  case Page_entry::SL_CALLBACK_IO:
    ndbrequire(! pagein && pageout || dump_page_lists(ptrI));
    break;
  case Page_entry::SL_BUSY:
    break;
  case Page_entry::SL_LOCKED:
    break;
  case Page_entry::SL_OTHER:
    break;
  default:
    ndbrequire(false || dump_page_lists(ptrI));
    break;
  }
}

void
Pgman::verify_page_lists()
{
  Page_hashlist& pl_hash = m_page_hashlist;
  Page_stack& pl_stack = m_page_stack;
  Page_queue& pl_queue = m_page_queue;
  Ptr<Page_entry> ptr;

  Uint32 stack_count = 0;
  Uint32 queue_count = 0;
  Uint32 queuewait_count = 0;
  Uint32 locked_bound_count = 0;

  Page_hashlist::Iterator iter;
  pl_hash.next(0, iter);
  while (iter.curr.i != RNIL)
  {
    verify_page_entry(iter.curr);

    Page_state state = iter.curr.p->m_state;
    if (state & Page_entry::ONSTACK)
      stack_count++;
    if (state & Page_entry::ONQUEUE)
      queue_count++;
    if (! (state & Page_entry::LOCKED) &&
        ! (state & Page_entry::HOT) &&
        (state & Page_entry::REQUEST) &&
        ! (state & Page_entry::BOUND))
      queuewait_count++;
    if (state & Page_entry::LOCKED &&
        state & Page_entry::BOUND)
      locked_bound_count++;
    pl_hash.next(iter);
  }

  ndbrequire(stack_count == pl_stack.count() || dump_page_lists());
  ndbrequire(queue_count == pl_queue.count() || dump_page_lists());

  Uint32 hot_bound_count = 0;
  Uint32 cold_bound_count = 0;

  Uint32 i1 = RNIL;
  for (pl_stack.first(ptr); ptr.i != RNIL; pl_stack.next(ptr))
  {
    ndbrequire(i1 != ptr.i);
    i1 = ptr.i;
    Page_state state = ptr.p->m_state;
    ndbrequire(state & Page_entry::ONSTACK || dump_page_lists());
    if (! pl_stack.hasPrev(ptr))
      ndbrequire(state & Page_entry::HOT || dump_page_lists());
    if (state & Page_entry::HOT &&
        state & Page_entry::BOUND)
      hot_bound_count++;
  }

  Uint32 i2 = RNIL;
  for (pl_queue.first(ptr); ptr.i != RNIL; pl_queue.next(ptr))
  {
    ndbrequire(i2 != ptr.i);
    i2 = ptr.i;
    Page_state state = ptr.p->m_state;
    ndbrequire(state & Page_entry::ONQUEUE || dump_page_lists());
    ndbrequire(state & Page_entry::BOUND || dump_page_lists());
    cold_bound_count++;
  }

  Uint32 tot_bound_count =
    locked_bound_count + hot_bound_count + cold_bound_count;
  ndbrequire(m_stats.m_num_pages == tot_bound_count || dump_page_lists());

  Uint32 k;
  Uint32 entry_count = 0;

  for (k = 0; k < Page_entry::SUBLIST_COUNT; k++)
  {
    const Page_sublist& pl = *m_page_sublist[k];
    for (pl.first(ptr); ptr.i != RNIL; pl.next(ptr))
    {
      ndbrequire(get_sublist_no(ptr.p->m_state) == k || dump_page_lists());
      entry_count++;
    }
  }

  ndbrequire(entry_count == pl_hash.count() || dump_page_lists());

  debugOut << "PGMAN: loop"
           << " stats=" << m_stats_loop_on
           << " busy=" << m_busy_loop_on
           << " cleanup=" << m_cleanup_loop_on
           << " lcp=" << m_lcp_loop_on << endl;

  debugOut << "PGMAN:"
           << " entry:" << pl_hash.count()
           << " cache:" << m_stats.m_num_pages
           << "(" << locked_bound_count << "L)"
           << " stack:" << pl_stack.count()
           << " queue:" << pl_queue.count()
           << " queuewait:" << queuewait_count << endl;

  debugOut << "PGMAN:";
  for (k = 0; k < Page_entry::SUBLIST_COUNT; k++)
  {
    const Page_sublist& pl = *m_page_sublist[k];
    debugOut << " " << get_sublist_name(k) << ":" << pl.count();
  }
  debugOut << endl;
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
  if (! debugFlag)
    open_debug_file(1);

  debugOut << "PGMAN: page list dump" << endl;
  if (ptrI != RNIL)
    debugOut << "PGMAN: error on PE [" << ptrI << "]" << endl;

  Page_hashlist& pl_hash = m_page_hashlist;
  Page_stack& pl_stack = m_page_stack;
  Page_queue& pl_queue = m_page_queue;
  Ptr<Page_entry> ptr;
  Uint32 n;
  char buf[40];

  debugOut << "hash:" << endl;
  Page_hashlist::Iterator iter;
  pl_hash.next(0, iter);
  n = 0;
  while (iter.curr.i != RNIL)
  {
    sprintf(buf, "%03d", n++);
    debugOut << buf << " " << iter.curr << endl;
    pl_hash.next(iter);
  }

  debugOut << "stack:" << endl;
  n = 0;
  for (pl_stack.first(ptr); ptr.i != RNIL; pl_stack.next(ptr))
  {
    sprintf(buf, "%03d", n++);
    debugOut << buf << " " << ptr << endl;
  }

  debugOut << "queue:" << endl;
  n = 0;
  for (pl_queue.first(ptr); ptr.i != RNIL; pl_queue.next(ptr))
  {
    sprintf(buf, "%03d", n++);
    debugOut << buf << " " << ptr << endl;
  }

  Uint32 k;
  for (k = 0; k < Page_entry::SUBLIST_COUNT; k++)
  {
    debugOut << get_sublist_name(k) << ":" << endl;
    const Page_sublist& pl = *m_page_sublist[k];
    for (pl.first(ptr); ptr.i != RNIL; pl.next(ptr))
    {
      sprintf(buf, "%03d", n++);
    debugOut << buf << " " << ptr << endl;
    }
  }

  if (! debugFlag)
    open_debug_file(0);

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
    return "callback";
  case Page_entry::SL_CALLBACK_IO:
    return "callback_io";
  case Page_entry::SL_BUSY:
    return "busy";
  case Page_entry::SL_LOCKED:
    return "locked";
  case Page_entry::SL_OTHER:
    return "other";
  }
  return "?";
}

NdbOut&
operator<<(NdbOut& out, Ptr<Pgman::Page_request> ptr)
{
  const Pgman::Page_request& pr = *ptr.p;
  const char* bname = getBlockName(pr.m_block, "?");
  out << "PR";
  if (ptr.i != RNIL)
    out << " [" << dec << ptr.i << "]";
  out << " block=" << bname;
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
  else
    out << " realpage=" << dec << pe.m_real_page_i;
  out << " lsn=" << dec << pe.m_lsn;
  out << " busy_count=" << dec << pe.m_busy_count;
#ifdef VM_TRACE
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

#ifdef VM_TRACE
void
Pgman::open_debug_file(Uint32 flag)
{
  if (flag)
  {
    FILE* f = globalSignalLoggers.getOutputStream();
    debugOut = *new NdbOut(*new FileOutputStream(f));
  }
  else
  {
    debugOut = *new NdbOut(*new NullOutputStream());
  }
}
#endif

void
Pgman::execDUMP_STATE_ORD(Signal* signal)
{
  jamEntry();
  Page_hashlist& pl_hash = m_page_hashlist;
#ifdef VM_TRACE
  if (signal->theData[0] == 11000 && signal->getLength() == 2)
  {
    Uint32 flag = signal->theData[1];
    open_debug_file(flag);
    debugFlag = flag;
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
      c_tup->disk_page_unmap_callback(ptr.p->m_real_page_i, 
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
    ndbout << "Dump LCP bucket m_lcp_outstanding: %d", m_lcp_outstanding;
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
    g_dbg_lcp = ~g_dbg_lcp;
  }
}

// page cache client

Page_cache_client::Page_cache_client(SimulatedBlock* block, Pgman* pgman)
{
  m_block = block->number();
  m_pgman = pgman;
}
