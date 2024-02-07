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

#ifndef LGMAN_H
#define LGMAN_H

#include <SimulatedBlock.hpp>

#include <DLHashTable.hpp>
#include <IntrusiveList.hpp>
#include <KeyTable.hpp>
#include <NodeBitmask.hpp>
#include <signaldata/GetTabInfo.hpp>
#include "diskpage.hpp"

#include <SafeMutex.hpp>
#include <WOPool.hpp>

#define JAM_FILE_ID 339

class FsReadWriteReq;

class Lgman : public SimulatedBlock {
 public:
  Lgman(Block_context &ctx);
  ~Lgman() override;
  BLOCK_DEFINES(Lgman);

 public:
  void execFSWRITEREQ(const FsReadWriteReq *req) const
      /* called direct cross threads from Ndbfs */;

 protected:
  void execSTTOR(Signal *signal);
  void sendSTTORRY(Signal *);
  void execREAD_CONFIG_REQ(Signal *signal);
  void execDUMP_STATE_ORD(Signal *signal);
  void execDBINFO_SCANREQ(Signal *signal);
  void execCONTINUEB(Signal *signal);
  void execNODE_FAILREP(Signal *signal);

  void execCREATE_FILE_IMPL_REQ(Signal *signal);
  void execCREATE_FILEGROUP_IMPL_REQ(Signal *signal);
  void execDROP_FILE_IMPL_REQ(Signal *signal);
  void execDROP_FILEGROUP_IMPL_REQ(Signal *signal);

  void execFSWRITEREF(Signal *);
  void execFSWRITECONF(Signal *);

  void execFSOPENREF(Signal *);
  void execFSOPENCONF(Signal *);

  void execFSCLOSEREF(Signal *);
  void execFSCLOSECONF(Signal *);

  void execFSREADREF(Signal *);
  void execFSREADCONF(Signal *);

  void execSUB_GCP_COMPLETE_REP(Signal *);

  void execSTART_RECREQ(Signal *);
  void execEND_LCPCONF(Signal *);

  void execGET_TABINFOREQ(Signal *);
  void execCALLBACK_ACK(Signal *);

  void sendGET_TABINFOREF(Signal *signal, GetTabInfoReq *req,
                          GetTabInfoRef::ErrorCode errorCode);

  Uint64 exec_lcp_frag_ord(Signal *, Uint32, SimulatedBlock *client_block);

 public:
  struct Log_waiter {
    CallbackPtr m_callback;
    union {
      Uint32 m_size;
      Uint64 m_sync_lsn;
    };
    Uint32 m_block;  // includes instance
    Uint32 nextList;
    Uint32 m_magic;
  };

  typedef RecordPool<WOPool<Log_waiter>> Log_waiter_pool;
  typedef SLFifoList<Log_waiter_pool> Log_waiter_list;
  typedef LocalSLFifoList<Log_waiter_pool> Local_log_waiter_list;

  struct Undofile {
    Undofile() {}
    Undofile(const struct CreateFileImplReq *, Uint32 lg_ptr_i);

    Uint32 m_magic;
    Uint32 m_file_id;  // Dict obj id
    Uint32 m_logfile_group_ptr_i;

    Uint32 m_file_size;
    Uint32 m_state;
    Uint32 m_fd;  // When speaking to NDBFS
    Uint64 m_start_lsn;
    Uint32 m_zero_page_i;  // Page to read zero page
    Uint32 m_requestInfo;

    enum FileState {
      FS_CREATING = 0x1  // File is being created
      ,
      FS_DROPPING = 0x2  // File is being dropped
      ,
      FS_ONLINE = 0x4  // File is online
      ,
      FS_OPENING = 0x8  // File is being opened during SR
      ,
      FS_SORTING = 0x10  // Files in group are being sorted
      ,
      FS_SEARCHING = 0x20  // File is being binary searched for end of log
      ,
      FS_EXECUTING = 0x40  // File is used for executing UNDO log
      ,
      FS_EMPTY = 0x80  // File is empty (used when online)
      ,
      FS_OUTSTANDING = 0x100  // File has outstanding request
      ,
      FS_MOVE_NEXT = 0x200  // When receiving reply move to next file
      ,
      FS_SEARCHING_END = 0x400  // Searched for end of log, scan
      ,
      FS_SEARCHING_FINAL_READ = 0x800  // Searched for log end, read last page
      ,
      FS_READ_ZERO_PAGE = 0x1000  // Reading zero page
    };

    union {
      struct {
        Uint32 m_outstanding;  // Outstanding pages
        Uint32 m_current_scan_index;
        Uint32 m_current_scanned_pages;
        bool m_binary_search_end;
        Uint64 m_lsn;  // Used when finding log head
      } m_online;
      struct {
        Uint32 m_senderData;
        Uint32 m_senderRef;
        Uint32 m_logfile_group_id;
        Uint32 m_logfile_group_version;
      } m_create;
    };

    Uint32 nextList;
    union {
      Uint32 prevList;
      Uint32 nextPool;
    };
  };

  typedef RecordPool<RWPool<Undofile>> Undofile_pool;
  typedef DLFifoList<Undofile_pool> Undofile_list;
  typedef LocalDLFifoList<Undofile_pool> Local_undofile_list;
  typedef LocalDataBuffer<15, ArrayPool<DataBufferSegment<15>>> Page_map;

  struct Buffer_idx {
    Uint32 m_ptr_i;
    Uint32 m_idx;
    bool operator==(const Buffer_idx &bi) const {
      return (m_ptr_i == bi.m_ptr_i && m_idx == bi.m_idx);
    }
  };

  struct Logfile_group {
    Logfile_group() {}
    Logfile_group(const struct CreateFilegroupImplReq *);

    Uint32 m_magic;
    union {
      Uint32 key;
      Uint32 m_logfile_group_id;
    };
    Uint32 m_version;
    Uint32 m_ndb_version;
    Uint16 m_state;
    Uint16 m_outstanding_fs;
    Uint32 m_next_reply_ptr_i;

    enum Logfile_group_state {
      LG_ONLINE = 0x001,
      LG_SORTING = 0x002  // Sorting files
      ,
      LG_SEARCHING = 0x004  // Searching in last file
      ,
      LG_EXEC_THREAD = 0x008  // Execute thread is running
      ,
      LG_READ_THREAD = 0x010  // Read thread is running
      ,
      LG_FORCE_SYNC_THREAD = 0x020,
      LG_SYNC_WAITERS_THREAD = 0x040,
      LG_CUT_LOG_THREAD = 0x080,
      LG_WAITERS_THREAD = 0x100,
      LG_FLUSH_THREAD = 0x200,
      LG_DROPPING = 0x400,
      LG_STARTING = 0x800,
      LG_LEVEL_REPORT_THREAD = 0x1000  // Level reporting to LQH thread active
    };

    static const Uint32 LG_THREAD_MASK =
        Logfile_group::LG_FORCE_SYNC_THREAD |
        Logfile_group::LG_SYNC_WAITERS_THREAD |
        Logfile_group::LG_CUT_LOG_THREAD | Logfile_group::LG_WAITERS_THREAD |
        Logfile_group::LG_FLUSH_THREAD | Logfile_group::LG_LEVEL_REPORT_THREAD;

    Uint32 m_applied;
    Uint32 m_count_since_last_report;

    Uint64 m_space_limit;
    Uint64 m_total_log_space;

    Uint64 m_next_lsn;
    Uint64 m_last_sync_req_lsn;  // Outstanding
    Uint64 m_last_synced_lsn;    //
    Uint64 m_max_sync_req_lsn;   // User requested lsn
    union {
      Uint64 m_last_read_lsn;
      Uint64 m_last_lcp_lsn;
    };
    Log_waiter_list::Head m_log_sync_waiters;

    Buffer_idx m_tail_pos[2];  // 0 is cut point, 1 is current LCP cut point
    Buffer_idx m_file_pos[2];  // 0 tail, 1 head = { file_ptr_i, page_no }
    Buffer_idx m_consumer_file_pos;
    Uint64 m_free_log_words;  // Free log words in logfile group
    Uint32 m_last_log_level_reported;

    Undofile_list::Head m_files;       // Files in log
    Undofile_list::Head m_meta_files;  // Files being created or dropped

    Uint32 m_total_buffer_words;     // Total buffer page words
    Uint32 m_free_buffer_words;      // Free buffer page words
    Uint32 m_callback_buffer_words;  // buffer words that has been
                                     // returned to user, but not yet consumed
    Log_waiter_list::Head m_log_buffer_waiters;
    /**
     * Each page range consists of up to 64 pages == 2 MByte.
     * m_current_page.m_ptr_i points to position in Page_map (m_buffer_pages)
     * m_current_page.m_idx indicates how many pages are left in range
     * m_current_pos.m_ptr_i points to i-value of current page
     * m_current_pos.m_idx indicates how many words are used in current page
     * For PRODUCER m_current_pos.m_idx is updated when we write the UNDO log
     * record from the get_log_buffer call. get_log_buffer also calls next_page
     * to move to next page in range OR first page in next range.
     * For CONSUMER m_current_pos.m_idx is only used during recovery, it is set
     * to 0 when running in normal operation.
     * 0 is reader == CONSUMER, 1 is writer == PRODUCER
     */
    Page_map::Head m_buffer_pages;  // Pairs of { ptr.i, count }
    struct Position {
      Buffer_idx m_current_page;  // { m_buffer_pages.i, left in range }
      Buffer_idx m_current_pos;   // { page ptr.i, m_words_used }
    } m_pos[2];                   // 0 is reader (lgman) 1 is writer (tup)

    Uint32 nextHash;
    Uint32 prevHash;
    Uint32 nextList;
    union {
      Uint32 prevList;
      Uint32 nextPool;
    };
    Uint32 hashValue() const { return key; }
    bool equal(const Logfile_group &rec) const { return key == rec.key; }
  };

  typedef RecordPool<RWPool<Logfile_group>> Logfile_group_pool;
  typedef DLFifoList<Logfile_group_pool> Logfile_group_list;
  typedef LocalDLFifoList<Logfile_group_pool> Local_logfile_group_list;
  typedef KeyTable<Logfile_group_pool> Logfile_group_hash;
  typedef KeyTable<Logfile_group_pool>::Iterator Logfile_group_hash_iterator;
  enum CallbackIndex {
    // lgman
    COUNT_CALLBACKS = 1
  };
  CallbackEntry m_callbackEntry[COUNT_CALLBACKS];
  CallbackTable m_callbackTable;

 private:
  friend class Logfile_client;
  SimulatedBlock *m_tup;

  /**
   * Alloc/free space in log
   *   Allocation will be removed at either/or
   *   1) Logfile_client::add_entry
   *   2) free_log_space
   */
  int alloc_log_space(Uint32 logfile_ref, Uint32 &words, bool add_extra_words,
                      bool abortable, EmulatedJamBuffer *jamBuf);
  int free_log_space(Uint32 logfile_ref, Uint32 words,
                     EmulatedJamBuffer *jamBuf);

  Undofile_pool m_file_pool;
  Logfile_group_pool m_logfile_group_pool;
  Log_waiter_pool m_log_waiter_pool;

  Page_map::DataBufferPool m_data_buffer_pool;

  Uint32 m_latest_lcp;
  Uint32 m_latest_local_lcp;
  Logfile_group_list m_logfile_group_list;
  Logfile_group_hash m_logfile_group_hash;
  Uint32 m_end_lcp_senderdata;
  bool m_node_restart_ongoing;
  bool m_dropped_undo_log;
  bool c_encrypted_filesystem;

  Uint64 m_records_applied;  // Track number of records applied
  Uint64 m_pages_applied;    // Track number of pages applied
  NdbMutex *m_client_mutex;

  /**
   * Index 0 : Total number of pending undo records (All LDMs combined)
   * Index i(>0) : Number of pending undo records for DBTUP instance i
   *
   * The counts are incremented in LGMAN when a
   * CONTINUEB (ZDISK_RESTART_UNDO) is sent to a single LDM with the required
   * undo record data.
   * The counts are decremented in LGMAN when a CONTINUEB is received from
   * an LDM thread.
   * Note: The numbers are applicable only to records of type UNDO_TUP_ALLOC,
   * UNDO_TUP_UPDATE, UNDO_TUP_UPDATE_PART, UNDO_TUP_UPDATE_PART,
   * UNDO_TUP_FREE and UNDO_TUP_FREE_PART.
   *
   */
  int m_pending_undo_records[MAX_NDBMT_LQH_WORKERS + 1];
  struct serial_record {
    Uint64 lsn;
    Uint32 ptr_array[20 + MAX_TUPLE_SIZE_IN_WORDS];
    Uint32 *ptr;
  } m_serial_record;

  void client_lock(BlockNumber block, int line, SimulatedBlock *);
  void client_unlock(BlockNumber block, int line, SimulatedBlock *);

  bool alloc_logbuffer_memory(Ptr<Logfile_group>, Uint32 pages);
  void init_logbuffer_pointers(Ptr<Logfile_group>);
  void free_logbuffer_memory(Ptr<Logfile_group>);
  Uint32 compute_free_file_pages(Ptr<Logfile_group>, EmulatedJamBuffer *jamBuf);
  void calculate_space_limit(Ptr<Logfile_group> lg_ptr);
  Uint32 get_remaining_page_space(Uint32);
  Uint32 *get_log_buffer(Ptr<Logfile_group>, Uint32 sz,
                         EmulatedJamBuffer *jamBuf);
  void process_log_buffer_waiters(Signal *signal, Ptr<Logfile_group>);
  Uint32 next_page(Logfile_group *ptrP, Uint32 i, EmulatedJamBuffer *jamBuf);

  void force_log_sync(Signal *, Ptr<Logfile_group>, Uint32 lsnhi, Uint32 lnslo);
  void process_log_sync_waiters(Signal *signal, Ptr<Logfile_group>);

  void level_report_thread(Signal *, Ptr<Logfile_group> ptr);
  void send_level_report_thread(Signal *, Ptr<Logfile_group> ptr);
  Uint64 calc_total_log_space(Ptr<Logfile_group> ptr);

  void cut_log_tail(Signal *, Ptr<Logfile_group> ptr);
  void open_file(Signal *, Ptr<Undofile>, Uint32, SectionHandle *);

  void flush_log(Signal *, Ptr<Logfile_group>, Uint32 force,
                 bool issue_continueb);
  Uint32 write_log_pages(Signal *, Ptr<Logfile_group>, Uint32 pageId,
                         Uint32 pages);

  void find_log_head(Signal *signal, Ptr<Logfile_group> ptr);
  void find_log_head_in_file(Signal *, Ptr<Logfile_group>, Ptr<Undofile>,
                             Uint64);
  void find_log_head_end_check(Signal *, Ptr<Logfile_group>, Ptr<Undofile>,
                               Uint64);
  void find_log_head_complete(Signal *, Ptr<Logfile_group>, Ptr<Undofile>);

  void init_run_undo_log(Signal *);
  void read_undo_log(Signal *, Ptr<Logfile_group> ptr);
  Uint32 read_undo_pages(Signal *, Ptr<Logfile_group>, Uint32 pageId,
                         Uint32 pages);

  void execute_undo_record(Signal *);
  const Uint32 *get_next_undo_record(Uint64 *lsn);
  void update_consumer_file_pos(Ptr<Logfile_group> ptr);
  void stop_run_undo_log(Signal *signal);
  void init_tail_ptr(Signal *signal, Ptr<Logfile_group> ptr);

  bool find_file_by_id(Ptr<Undofile> &, Local_undofile_list::Head &, Uint32 id);
  void create_file_commit(Signal *signal, Ptr<Logfile_group>, Ptr<Undofile>);
  void create_file_abort(Signal *signal, Ptr<Logfile_group>, Ptr<Undofile>);

#ifdef VM_TRACE
  void validate_logfile_group(Ptr<Logfile_group> ptr, const char *,
                              EmulatedJamBuffer *jamBuf);
#else
  void validate_logfile_group(Ptr<Logfile_group> ptr, const char * = 0,
                              EmulatedJamBuffer *jamBuf = 0) {}
#endif

  void drop_filegroup_drop_files(Signal *, Ptr<Logfile_group>, Uint32 ref,
                                 Uint32 data);

  Uint32 *get_undo_data_ptr(Uint32 *page, Ptr<Logfile_group> lg_ptr,
                            EmulatedJamBuffer *jamBuf) {
    if (lg_ptr.p->m_ndb_version >= NDB_DISK_V2) {
      thrjam(jamBuf);
      const File_formats::Undofile::Undo_page_v2 *page_v2 =
          (const File_formats::Undofile::Undo_page_v2 *)page;
      return (Uint32 *)(&page_v2->m_data);
    } else {
      thrjam(jamBuf);
      const File_formats::Undofile::Undo_page *page_v1 =
          (const File_formats::Undofile::Undo_page *)page;
      return (Uint32 *)(&page_v1->m_data);
    }
  }
  Uint32 get_undo_page_words(Ptr<Logfile_group> lg_ptr) {
    if (lg_ptr.p->m_ndb_version >= NDB_DISK_V2) {
      return File_formats::UNDO_PAGE_WORDS_v2;
    } else {
      return File_formats::UNDO_PAGE_WORDS;
    }
  }
  void reinit_logbuffer_words(Ptr<Logfile_group> lg_ptr);
  void completed_zero_page_read(Signal *signal, Ptr<Undofile> lg_ptr);
  void sendCREATE_FILE_IMPL_CONF(Signal *signal, Ptr<Undofile> file_ptr);
  void sendCUT_UNDO_LOG_TAIL_CONF(Signal *);
  void execCUT_UNDO_LOG_TAIL_REQ(Signal *);

  /**
   * Checks if it's needed to wait for the pending records to complete.
   * If waiting is required, it saves "ptr" in a member variable.
   * @param ptr The undo log record that requires all the pending records to
   * complete execution
   * @return true if wait required, false otherwise.
   */
  bool wait_pending(Uint64 lsn, const Uint32 *ptr, Uint32 len);
};

class Logfile_client {
  SimulatedBlock *m_client_block;
  Uint32 m_block;  // includes instance
  Lgman *m_lgman;
  bool m_lock;
  DEBUG_OUT_DEFINES(LGMAN);

 public:
  Uint32 m_logfile_group_id;

  Logfile_client(SimulatedBlock *block, Lgman *, Uint32 logfile_group_id,
                 bool lock = true);
  ~Logfile_client();

  struct Request {
    SimulatedBlock::CallbackPtr m_callback;
  };

  /**
   * Request flags
   */
  enum RequestFlags {};

  /**
   * Check if a logfile group exists
   * @return true if a logfile group exists
   *         false otherwise
   */
  bool exists_logfile_group();
  /**
   * Make sure a lsn is stored
   * @return -1, on error
   *          0, request in queued
   *         >0, done
   */
  int sync_lsn(Signal *, Uint64, Request *, Uint32 flags);

  /**
   * Get the last lsn stored, also ensure that this lsn is
   * stored next time we call sync_lsn from LCP code.
   */
  Uint64 pre_sync_lsn(Uint64 lsn);

  /**
   * Undolog entries
   */
  struct Change {
    const void *ptr;
    Uint32 len;
  };

  Uint64 get_latest_lsn();
  Uint64 add_entry_simple(const Change *, Uint32 cnt, Uint32 alloc_size,
                          bool update_callback_buffer_words = true);
  Uint64 add_entry_complex(const Change *, Uint32 cnt, bool, Uint32 alloc_size);

  /**
   * Check for space in log buffer
   *
   *   return >0 if available
   *           0 on time slice
   *          -1 on error
   */
  int get_log_buffer(Signal *, Uint32 sz, SimulatedBlock::CallbackPtr *);

  int alloc_log_space(Uint32 &words, bool add_extra_words, bool abortable,
                      EmulatedJamBuffer *jamBuf) {
    return m_lgman->alloc_log_space(m_logfile_group_id, words, add_extra_words,
                                    abortable, jamBuf);
  }

  int free_log_space(Uint32 words, EmulatedJamBuffer *jamBuf) {
    return m_lgman->free_log_space(m_logfile_group_id, words, jamBuf);
  }

  Uint64 exec_lcp_frag_ord(Signal *signal, Uint32 local_lcp_id) {
    return m_lgman->exec_lcp_frag_ord(signal, local_lcp_id, m_client_block);
  }

 private:
  Uint32 *get_log_buffer(Uint32 sz);
};

#undef JAM_FILE_ID

#endif
