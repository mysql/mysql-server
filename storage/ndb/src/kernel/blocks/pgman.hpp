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

#ifndef PGMAN_H
#define PGMAN_H

#include <SimulatedBlock.hpp>

#include <DLCHashTable.hpp>
#include <IntrusiveList.hpp>
#include <NodeBitmask.hpp>
#include <signaldata/LCP.hpp>
#include "lgman.hpp"

#include <NdbOut.hpp>
#include <OutputStream.hpp>

#define JAM_FILE_ID 462


/*
 * PGMAN
 *
 * PAGE ENTRIES AND REQUESTS
 *
 * Central structure is "page entry".  It corresponds to a disk page
 * identified by file and page number (file_no, page_no).
 *
 * A page entry is created by first request for the disk page.
 * Subsequent requests are queued under the same page entry.
 *
 * There is a limited number of in-memory "cache pages", also called
 * "buffer pages" or "real pages".  These are used by the more numerous
 * page entries to buffer the disk pages.
 *
 * A new or non-resident page entry must first be "bound" to an
 * available cache page.  Next the disk page must be "mapped" to the
 * cache page.  If the page is empty (never written) it is considered
 * mapped trivially.  Otherwise the cache page must be updated via
 * "pagein" from disk.  A bound and mapped page is called "resident".
 *
 * Updating a resident cache page makes it "dirty".  A background
 * clean-up process makes dirty pages "clean" via "pageout" to disk.
 * Write ahead logging (WAL) of the page is done first i.e. UNDO log is
 * flushed up to the page log sequence number (LSN) by calling a LGMAN
 * method.  The reason for this is obvious but not relevant to PGMAN.
 *
 * A local check point (LCP) periodically performs a complete pageout of
 * dirty pages.  It must iterate over a list which will cover all pages
 * which had been dirty since LCP start.
 *
 * A clean page is a candidate ("victim") for being "unmapped" and
 * "evicted" from the cache, to allow another page to become resident.
 * This process is called "page replacement".
 *
 * PAGE REPLACEMENT
 *
 * Page replacement uses the LIRS algorithm (Jiang-Zhang).
 * 
 * The "recency" of a page is the time between now and the last request
 * for the page.  The "inter-reference recency" (IRR) of a page is the
 * time between the last 2 requests for the page.  "Time" is advanced by
 * request for any page.
 *
 * Page entries are divided into "hot" ("lir") and "cold" ("hir").  Here
 * lir/hir refers to low/high IRR.  Hot pages are always resident but
 * cold pages need not be.
 *
 * Number of hot pages is limited to slightly less than number of cache
 * pages.  Until this number is reached, all used cache pages are hot.
 * Then the algorithm described next is applied.  The algorithm avoids
 * storing any of the actual recency values.
 *
 * Primary data structure is the "stack".  It contains all hot entries
 * and recently referenced cold entries (resident or not).  The stack is
 * in recency order with most recent (lowest recency) entry on top.
 * Entries which are less recent than the least recent hot page are
 * removed ("stack pruning").  So the bottom page is always hot.
 *
 * The cold entries on the stack are undergoing a "trial period".  If
 * they are referenced soon again (see IRR), they become hot.  Otherwise
 * they fall off the bottom of the stack.
 *
 * Secondary data structure is the "queue".  It contains all resident
 * cold pages (on stack or not).  When a hot page is removed from the
 * stack it is added to the end of the queue.  When page replacement
 * needs a page it removes it from the front of the queue.
 *
 * Page requests cause the input entry to be inserted and updated in
 * LIRS lists.  Remember that an entry can be present on both stack and
 * queue.  The rules per type of input entry are:
 *
 * 1. Hot.  Move input entry to stack top.  If input entry was at stack
 * bottom, do stack pruning.
 *
 * 2. Cold resident.  Move input entry to stack top.  Then:
 *
 * 2a. If input entry was on stack, change it to hot, remove it from
 * queue, change stack bottom entry to cold and move the bottom entry to
 * queue end, and do stack pruning.
 *
 * 2b. If input entry was on queue only, leave it cold but move it to
 * end of queue.
 *
 * 3. Cold non-resident.  Remove entry at queue front and evict it from
 * the cache.  If the evicted entry was on stack, it remains as unbound
 * entry on stack, to continue its trial period.  Map input entry to the
 * freed cache page.  Move input entry to stack top.  Then:
 *
 * 3a. If input entry was on stack, change it to hot, change stack
 * bottom entry to cold and move the bottom entry to queue end, and do
 * stack pruning.
 *
 * 3b. If input entry was new, leave it cold but move it to end of
 * queue.
 *
 * LIRS CHANGES
 *
 * In LIRS the 'resident' requirement is changed as follows:
 *
 * Stack entries, including hot ones, can have any state.  Unbound stack
 * entries are created by new requests and by pages evicted from queue
 * front which are still on stack.
 *
 * Queue entries must be bound.  They become resident and evictable
 * within a finite time.  A page is "evictable" if it is mapped, clean,
 * and has no requests.
 *
 * An unbound entry which should be on queue is added there at bind
 * time.  Such entries are created when an unbound entry with open
 * requests is popped (hot) or pruned (cold) from the stack.  This can
 * happen if the cache is too small.
 *
 * CLEANUP PROCESS
 *
 * LIRS (and related algorithms) do not address dirty pages.  From above
 * it is obvious that the clean-up process should process dirty queue
 * entries proceeding from front to end.  This also favors pages with
 * lower LSN numbers which minimizes amount of WAL to write.
 *
 * In fact the clean-up process holds a permanent pointer into the queue
 * where all entries strictly towards the front are clean.  For such an
 * entry to become dirty it must be referenced again which moves it to
 * queue end and past the clean-up pointer.  (In practice, until this
 * works, cleanup recycles back to queue front).
 *
 * PAGE LISTS
 *
 * Page entries are put on a number of lists.
 *
 * 1. Hash table on (file_no, page_no).  Used for fast lookup and for
 * LCP to iterate over.
 *
 * The other lists are doubly-linked FIFOs.  In general entries are
 * added to the end (last entry) and processed from the front (first
 * entry).  When used as stack, end is top and front is bottom.
 *
 * 2. The LIRS stack and queue.  These control page replacement.
 *
 * 3. Page entries are divided into disjoint "sublists" based on page
 * "state" i.e. the set of page properties.  Some sublists drive page
 * processing and have next entry to process at the front.
 *
 * Current sublists are as follows.  Those that drive processing are
 * marked with a plus (+).
 *
 * SL_BIND          + waiting for available buffer page
 * SL_MAP           + waiting to start pagein from disk
 * SL_MAP_IO        - above in i/o wait (the pagein)
 * SL_CALLBACK      + request done, waiting to invoke callbacks
 * SL_CALLBACK_IO   - above in i/o wait (pageout by cleanup)
 * SL_BUSY          - being written to by PGMAN client
 * SL_LOCKED        - permanently locked to cache
 * SL_OTHER         - default sublist
 *
 * PAGE PROCESSING
 *
 * Page processing uses a number independent continueB loops.
 *
 * 1. The "stats loop".  Started at node start.  Checks lists in debug
 * mode.  In the future could gather statistics and adjust parameters
 * based on load.  Continues via delay signal.
 *
 * 2. The "busy loop".  Started by page request.  Each loop does bind,
 * map, and callback of a number of entries.  Continues via no-delay
 * signal until nothing to do.
 *
 * 3. The "cleanup loop".  Started at node start.  Each loop starts
 * pageout of a number of dirty queue entries.  Continues via delay
 * signal.
 *
 * 4. The "LCP loop".  Started periodically by NDB.  Each loop starts
 * pageout of a number of hash list entries.  Continues via delay signal
 * until done.
 *
 * SPECIAL CASES
 *
 * LOCKED pages are not put on stack or queue.  They are flushed to disk
 * by LCP but not by clean-up.
 *
 * A TUP scan is likely to access a page repeatedly within a short time.
 * This can make the page hot when it should not be.  Such "correlated
 * requests" are handled by a request flag which modifies default LIRS
 * processing.  [fill in details later]
 *
 * Also PK operations make 2 rapid page references.  The 2nd one is for
 * commit.  This too should be handled as a correlated request.
 *
 * CLIENT TSMAN
 *
 * TSMAN reads "meta" pages such as extent headers.  There are currently
 * "locked" forever in PGMAN cache.
 *
 * CLIENT DBTUP
 * 
 * DBTUP works with copy pages (or UNDO buffers) in memory.  The real
 * page is updated only between page request with COMMIT_REQ flag and
 * a subsequent LSN update.  These need not occur in same timeslice
 * since DBTUP may need to flush UNDO log in-between.
 *
 * The page is "busy" if any transaction is between COMMIT_REQ and LSN
 * update.  A busy page must be locked in buffer cache.  No pageout of
 * a busy page can be started by clean-up or LCP.
 */

class Pgman : public SimulatedBlock
{
public:
  Pgman(Block_context& ctx, Uint32 instanceNumber = 0);
  virtual ~Pgman();
  BLOCK_DEFINES(Pgman);

private:
  friend class Page_cache_client;
  friend class PgmanProxy;

  struct Page_entry; // CC
  friend struct Page_entry;

  struct Page_request {
    enum Flags {
      OP_MASK       = 0x000F // 4 bits for TUP operation
      ,LOCK_PAGE    = 0x0020 // lock page in memory
      ,EMPTY_PAGE   = 0x0040 // empty (new) page
      ,ALLOC_REQ    = 0x0080 // part of alloc
      ,COMMIT_REQ   = 0x0100 // part of commit
      ,DIRTY_REQ    = 0x0200 // make page dirty wo/ update_lsn
      ,UNLOCK_PAGE  = 0x0400
      ,CORR_REQ     = 0x0800 // correlated request (no LIRS update)
#ifdef ERROR_INSERT
      ,DELAY_REQ    = 0x1000 // Force request to be delayed
#endif
    };
    
    Uint16 m_block; // includes instance
    Uint16 m_flags;
    SimulatedBlock::Callback m_callback;

#ifdef ERROR_INSERT
    NDB_TICKS m_delay_until_time;
#endif
    Uint32 nextList;
    Uint32 m_magic;
  };

  typedef RecordPool<Page_request, WOPool> Page_request_pool;
  typedef SLFifoListImpl<Page_request_pool, Page_request> Page_request_list;
  typedef LocalSLFifoListImpl<Page_request_pool, Page_request> Local_page_request_list;
  
  struct Page_entry_stack_ptr {
    Uint32 nextList;
    Uint32 prevList;
  };

  struct Page_entry_queue_ptr {
    Uint32 nextList;
    Uint32 prevList;
  };

  struct Page_entry_sublist_ptr {
    Uint32 nextList;
    Uint32 prevList;
  };

  typedef Uint16 Page_state;
  
  struct Page_entry : Page_entry_stack_ptr,
                      Page_entry_queue_ptr,
                      Page_entry_sublist_ptr {
    Page_entry() {}
    Page_entry(Uint32 file_no, Uint32 page_no);

    enum State {
      NO_STATE = 0x0000
      ,REQUEST = 0x0001 // has outstanding request
      ,EMPTY   = 0x0002 // empty (never written) page
      ,BOUND   = 0x0004 // m_real_page_ptr assigned
      ,MAPPED  = 0x0008 // bound, and empty or paged in
      ,DIRTY   = 0x0010 // page is modified
      ,USED    = 0x0020 // used by some tx (not set currently)
      ,BUSY    = 0x0040 // page is being written to
      ,LOCKED  = 0x0080 // locked in cache (forever)
      ,PAGEIN  = 0x0100 // paging in
      ,PAGEOUT = 0x0200 // paging out
      ,LOGSYNC = 0x0400 // undo WAL as part of pageout
      ,LCP     = 0x1000 // page is LCP flushed
      ,HOT     = 0x2000 // page is hot
      ,ONSTACK = 0x4000 // page is on LIRS stack
      ,ONQUEUE = 0x8000 // page is on LIRS queue
    };
    
    enum Sublist {
      SL_BIND = 0
      ,SL_MAP = 1
      ,SL_MAP_IO = 2
      ,SL_CALLBACK = 3
      ,SL_CALLBACK_IO = 4
      ,SL_BUSY = 5
      ,SL_LOCKED = 6
      ,SL_IDLE = 7
      ,SL_OTHER = 8
      ,SUBLIST_COUNT = 9
    };

    Uint16 m_file_no;       // disk page address set at seize
    Page_state m_state;         // flags (0 for new entry)
    
    Uint32 m_page_no;
    Uint32 m_real_page_i;
    Uint64 m_lsn;
    
    Uint32 m_last_lcp;
    Uint32 m_dirty_count;
    Uint32 m_copy_page_i;
    union {
      Uint32 m_busy_count;        // non-zero means BUSY
      Uint32 nextPool;
    };
    
    Page_request_list::Head m_requests;
    
    Uint32 nextHash;
    Uint32 prevHash;
    
    Uint32 hashValue() const { return m_file_no << 16 | m_page_no; }
    bool equal(const Page_entry& obj) const { 
      return 
	m_file_no == obj.m_file_no && m_page_no == obj.m_page_no;
    }

#ifdef VM_TRACE
    Pgman* m_this;
#endif
  };

  typedef DLCHashTable<Page_entry> Page_hashlist;
  typedef DLCFifoList<Page_entry, Page_entry_stack_ptr> Page_stack;
  typedef DLCFifoList<Page_entry, Page_entry_queue_ptr> Page_queue;
  typedef DLCFifoList<Page_entry, Page_entry_sublist_ptr> Page_sublist;

  class Dbtup *c_tup;
  class Lgman *c_lgman;
  class Tsman *c_tsman;

  // loop status
  bool m_stats_loop_on;
  bool m_busy_loop_on;
  bool m_cleanup_loop_on;

  // LCP variables
  enum LCP_STATE
  {
    LS_LCP_OFF = 0
    ,LS_LCP_ON = 1
    ,LS_LCP_MAX_LCP_OUTSTANDING = 2
    ,LS_LCP_LOCKED = 3
  } m_lcp_state;
  Uint32 m_last_lcp;
  Uint32 m_last_lcp_complete;
  Uint32 m_lcp_curr_bucket;
  Uint32 m_lcp_outstanding;     // remaining i/o waits
  EndLcpReq m_end_lcp_req;
  
  // clean-up variables
  Ptr<Page_entry> m_cleanup_ptr;
 
  // file map
  typedef DataBuffer<15> File_map;
  File_map m_file_map;
  File_map::DataBufferPool m_data_buffer_pool;

  // page entries and requests
  Page_request_pool m_page_request_pool;
  ArrayPool<Page_entry> m_page_entry_pool;
  Page_hashlist m_page_hashlist;
  Page_stack m_page_stack;
  Page_queue m_page_queue;
  Page_sublist* m_page_sublist[Page_entry::SUBLIST_COUNT];

  // configuration
  struct Param {
    Param();
    Uint32 m_max_pages;         // max number of cache pages
    Uint32 m_lirs_stack_mult;   // in m_max_pages (around 3-10)
    Uint32 m_max_hot_pages;     // max hot cache pages (up to 99%)
    Uint32 m_max_loop_count;    // limit purely local loops
    Uint32 m_max_io_waits;
    Uint32 m_stats_loop_delay;
    Uint32 m_cleanup_loop_delay;
    Uint32 m_lcp_loop_delay;
  } m_param;

  // runtime sizes and statistics
  struct Stats {
    Stats() :
      m_num_pages(0),
      m_num_hot_pages(0),
      m_current_io_waits(0),
      m_page_hits(0),
      m_page_faults(0),
      m_pages_written(0),
      m_pages_written_lcp(0),
      m_pages_read(0),
      m_log_waits(0),
      m_page_requests_direct_return(0),
      m_page_requests_wait_q(0),
      m_page_requests_wait_io(0),
      m_entries_high(0)
    {}
    Uint32 m_num_pages;         // current number of cache pages
    Uint32 m_num_hot_pages;
    Uint32 m_current_io_waits;
    Uint64 m_page_hits;
    Uint64 m_page_faults;
    Uint64 m_pages_written;
    Uint64 m_pages_written_lcp;
    Uint64 m_pages_read;
    Uint64 m_log_waits; // wait for undo WAL to flush the log recs
    Uint64 m_page_requests_direct_return;
    Uint64 m_page_requests_wait_q;
    Uint64 m_page_requests_wait_io;
    Uint32 m_entries_high;
  } m_stats;

  enum CallbackIndex {
    // lgman
    LOGSYNC_CALLBACK = 1,
    COUNT_CALLBACKS = 2
  };
  CallbackEntry m_callbackEntry[COUNT_CALLBACKS];
  CallbackTable m_callbackTable;

protected:
  void execSTTOR(Signal* signal);
  void sendSTTORRY(Signal*);
  void execREAD_CONFIG_REQ(Signal* signal);
  void execCONTINUEB(Signal* signal);

  void execLCP_FRAG_ORD(Signal*);
  void execEND_LCPREQ(Signal*);
  void execRELEASE_PAGES_REQ(Signal*);
  
  void execFSREADCONF(Signal*);
  void execFSREADREF(Signal*);
  void execFSWRITECONF(Signal*);
  void execFSWRITEREF(Signal*);

  void execDUMP_STATE_ORD(Signal* signal);

  void execDATA_FILE_ORD(Signal*);

  void execDBINFO_SCANREQ(Signal*);

private:
  static Uint32 get_sublist_no(Page_state state);
  void set_page_state(EmulatedJamBuffer* jamBuf, Ptr<Page_entry> ptr,
                      Page_state new_state);

  bool seize_cache_page(Ptr<GlobalPage>& gptr);
  void release_cache_page(Uint32 i);

  bool find_page_entry(Ptr<Page_entry>&, Uint32 file_no, Uint32 page_no);
  Uint32 seize_page_entry(Ptr<Page_entry>&, Uint32 file_no, Uint32 page_no);
  bool get_page_entry(EmulatedJamBuffer* jamBuf, Ptr<Page_entry>&, 
                      Uint32 file_no, Uint32 page_no);
  void release_page_entry(Ptr<Page_entry>&);

  void lirs_stack_prune();
  void lirs_stack_pop();
  void lirs_reference(Ptr<Page_entry> ptr);

  void do_stats_loop(Signal*);
  void do_busy_loop(Signal*, bool direct, EmulatedJamBuffer *jamBuf);
  void do_cleanup_loop(Signal*);
  void do_lcp_loop(Signal*);

  bool process_bind(Signal*);
  bool process_bind(Signal*, Ptr<Page_entry> ptr);
  bool process_map(Signal*);
  bool process_map(Signal*, Ptr<Page_entry> ptr);
  bool process_callback(Signal*);
  bool process_callback(Signal*, Ptr<Page_entry> ptr);

  bool process_cleanup(Signal*);
  void move_cleanup_ptr(Ptr<Page_entry> ptr);

  LCP_STATE process_lcp(Signal*);
  void process_lcp_locked(Signal* signal, Ptr<Page_entry> ptr);
  void process_lcp_locked_fswriteconf(Signal* signal, Ptr<Page_entry> ptr);

  void pagein(Signal*, Ptr<Page_entry>);
  void fsreadreq(Signal*, Ptr<Page_entry>);
  void fsreadconf(Signal*, Ptr<Page_entry>);
  void pageout(Signal*, Ptr<Page_entry>);
  void logsync_callback(Signal*, Uint32 ptrI, Uint32 res);
  void fswritereq(Signal*, Ptr<Page_entry>);
  void fswriteconf(Signal*, Ptr<Page_entry>);

  int get_page_no_lirs(EmulatedJamBuffer* jamBuf, Signal*, Ptr<Page_entry>, 
                       Page_request page_req);
  int get_page(EmulatedJamBuffer* jamBuf, Signal*, Ptr<Page_entry>, 
               Page_request page_req);
  void update_lsn(EmulatedJamBuffer* jamBuf, Ptr<Page_entry>, Uint32 block, 
                  Uint64 lsn);
  Uint32 create_data_file();
  Uint32 alloc_data_file(Uint32 file_no);
  void map_file_no(Uint32 file_no, Uint32 fd);
  void free_data_file(Uint32 file_no, Uint32 fd = RNIL);
  int drop_page(Ptr<Page_entry>);
  
#ifdef VM_TRACE
  bool debugFlag;        // not yet in use in 7.0
  bool debugSummaryFlag; // loop summary to signal log even if ! debugFlag
  void verify_page_entry(Ptr<Page_entry> ptr);
  void verify_page_lists();
  void verify_all();
  bool dump_page_lists(Uint32 ptrI = RNIL);
#endif
  static const char* get_sublist_name(Uint32 list_no);
  friend class NdbOut& operator<<(NdbOut&, Ptr<Page_request>);
  friend class NdbOut& operator<<(NdbOut&, Ptr<Page_entry>);
};

class NdbOut& operator<<(NdbOut&, Ptr<Pgman::Page_request>);
class NdbOut& operator<<(NdbOut&, Ptr<Pgman::Page_entry>);

class Page_cache_client
{
  friend class PgmanProxy;
  Uint32 m_block; // includes instance
  class PgmanProxy* m_pgman_proxy; // set if we go via proxy
  Pgman* m_pgman;
  EmulatedJamBuffer* const m_jamBuf;
  DEBUG_OUT_DEFINES(PGMAN);

public:
  Page_cache_client(SimulatedBlock* block, SimulatedBlock* pgman);

  struct Request {
    Local_key m_page;
    SimulatedBlock::Callback m_callback;
    
#ifdef ERROR_INSERT
    NDB_TICKS m_delay_until_time;
#endif
  };

  Ptr<GlobalPage> m_ptr;        // TODO remove

  enum RequestFlags {
    LOCK_PAGE = Pgman::Page_request::LOCK_PAGE
    ,EMPTY_PAGE = Pgman::Page_request::EMPTY_PAGE
    ,ALLOC_REQ = Pgman::Page_request::ALLOC_REQ
    ,COMMIT_REQ = Pgman::Page_request::COMMIT_REQ
    ,DIRTY_REQ = Pgman::Page_request::DIRTY_REQ
    ,UNLOCK_PAGE = Pgman::Page_request::UNLOCK_PAGE
    ,CORR_REQ = Pgman::Page_request::CORR_REQ
#ifdef ERROR_INSERT
    ,DELAY_REQ = Pgman::Page_request::DELAY_REQ
#endif
  };
  
  /**
   * Get a page
   * @note This request may return true even if previous request
   *       for same page return false, and it's callback has not been called
   * @return -1, on error
   *          0, request is queued
   *         >0, real_page_id
   */
  int get_page(Signal*, Request&, Uint32 flags);

  void update_lsn(Local_key, Uint64 lsn);

  /**
   * Drop page
   *
   * @return -1 on error
   *          0 is request is queued
   *         >0 is ok
   */
  int drop_page(Local_key, Uint32 page_id);
  
  /**
   * Create file record
   */
  Uint32 create_data_file(Signal*);

  /**
   * Alloc datafile record
   */
  Uint32 alloc_data_file(Signal*, Uint32 file_no);

  /**
   * Map file_no to m_fd
   */
  void map_file_no(Signal*, Uint32 m_file_no, Uint32 m_fd);

  /**
   * Free file
   */
  void free_data_file(Signal*, Uint32 file_no, Uint32 fd = RNIL);
};


#undef JAM_FILE_ID

#endif
