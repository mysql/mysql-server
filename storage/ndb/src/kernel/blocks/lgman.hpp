/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef LGMAN_H
#define LGMAN_H

#include <SimulatedBlock.hpp>

#include <SLList.hpp>
#include <DLList.hpp>
#include <DLFifoList.hpp>
#include <KeyTable.hpp>
#include <DLHashTable.hpp>
#include <NodeBitmask.hpp>
#include "diskpage.hpp"
#include <signaldata/GetTabInfo.hpp>

#include <WOPool.hpp>
#include <SLFifoList.hpp>

class Lgman : public SimulatedBlock
{
public:
  Lgman(Block_context& ctx);
  virtual ~Lgman();
  BLOCK_DEFINES(Lgman);
  
protected:
  
  void execSTTOR(Signal* signal);
  void sendSTTORRY(Signal*);
  void execREAD_CONFIG_REQ(Signal* signal);
  void execDUMP_STATE_ORD(Signal* signal);
  void execCONTINUEB(Signal* signal);
  
  void execCREATE_FILE_REQ(Signal* signal);
  void execCREATE_FILEGROUP_REQ(Signal* signal);
  void execDROP_FILE_REQ(Signal* signal);
  void execDROP_FILEGROUP_REQ(Signal* signal);
  
  void execFSWRITEREQ(Signal*);
  void execFSWRITEREF(Signal*);
  void execFSWRITECONF(Signal*);

  void execFSOPENREF(Signal*);
  void execFSOPENCONF(Signal*);

  void execFSCLOSEREF(Signal*);
  void execFSCLOSECONF(Signal*);

  void execFSREADREF(Signal*);
  void execFSREADCONF(Signal*);

  void execLCP_FRAG_ORD(Signal*);
  void execEND_LCP_REQ(Signal*);
  void execSUB_GCP_COMPLETE_REP(Signal*);
  
  void execSTART_RECREQ(Signal*);
  void execEND_LCP_CONF(Signal*);

  void execGET_TABINFOREQ(Signal*);

  void sendGET_TABINFOREF(Signal* signal,
			  GetTabInfoReq * req,
			  GetTabInfoRef::ErrorCode errorCode);

public:
  struct Log_waiter
  {
    Callback m_callback;
    union {
      Uint32 m_size;
      Uint64 m_sync_lsn;
    };
    Uint32 m_block;
    Uint32 nextList;
    Uint32 m_magic;
  };

  typedef RecordPool<Log_waiter, WOPool> Log_waiter_pool;
  typedef SLFifoListImpl<Log_waiter_pool, Log_waiter> Log_waiter_list;
  typedef LocalSLFifoListImpl<Log_waiter_pool, Log_waiter> Local_log_waiter_list;
  
  struct Undofile
  {
    Undofile(){}
    Undofile(const struct CreateFileImplReq*, Uint32 lg_ptr_i);
    
    Uint32 m_magic;
    Uint32 m_file_id; // Dict obj id
    Uint32 m_logfile_group_ptr_i;

    Uint32 m_file_size;
    Uint32 m_state;
    Uint32 m_fd; // When speaking to NDBFS
    
    enum FileState 
    {
      FS_CREATING     = 0x1   // File is being created
      ,FS_DROPPING    = 0x2   // File is being dropped
      ,FS_ONLINE      = 0x4   // File is online
      ,FS_OPENING     = 0x8   // File is being opened during SR
      ,FS_SORTING     = 0x10  // Files in group are being sorted
      ,FS_SEARCHING   = 0x20  // File is being searched for end of log
      ,FS_EXECUTING   = 0x40  // File is used for executing UNDO log
      ,FS_EMPTY       = 0x80  // File is empty (used when online)
      ,FS_OUTSTANDING = 0x100 // File has outstanding request
      ,FS_MOVE_NEXT   = 0x200 // When receiving reply move to next file
    };
    
    union {
      struct {
	Uint32 m_outstanding; // Outstaning pages
	Uint64 m_lsn;         // Used when finding log head
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

  typedef RecordPool<Undofile, RWPool> Undofile_pool;
  typedef DLFifoListImpl<Undofile_pool, Undofile> Undofile_list;
  typedef LocalDLFifoListImpl<Undofile_pool, Undofile> Local_undofile_list;
  typedef LocalDataBuffer<15> Page_map;

  struct Buffer_idx 
  {
    Uint32 m_ptr_i;
    Uint32 m_idx;
    bool operator== (const Buffer_idx& bi) const { 
      return (m_ptr_i == bi.m_ptr_i && m_idx == bi.m_idx); 
    }
  };

  struct Logfile_group
  {
    Logfile_group(){}
    Logfile_group(const struct CreateFilegroupImplReq*);
    
    Uint32 m_magic;
    union {
      Uint32 key;
      Uint32 m_logfile_group_id;
    };
    Uint32 m_version;
    Uint16 m_state;
    Uint16 m_outstanding_fs;
    Uint32 m_next_reply_ptr_i;
    
    enum Logfile_group_state
    {
      LG_ONLINE               = 0x001
      ,LG_SORTING             = 0x002  // Sorting files
      ,LG_SEARCHING           = 0x004  // Searching in last file
      ,LG_EXEC_THREAD         = 0x008  // Execute thread is running
      ,LG_READ_THREAD         = 0x010  // Read thread is running
      ,LG_FORCE_SYNC_THREAD   = 0x020
      ,LG_SYNC_WAITERS_THREAD = 0x040
      ,LG_CUT_LOG_THREAD      = 0x080
      ,LG_WAITERS_THREAD      = 0x100
      ,LG_FLUSH_THREAD        = 0x200
      ,LG_DROPPING            = 0x400
      ,LG_STARTING            = 0x800
    };

    static const Uint32 LG_THREAD_MASK = Logfile_group::LG_FORCE_SYNC_THREAD |
                                  Logfile_group::LG_SYNC_WAITERS_THREAD |
                                  Logfile_group::LG_CUT_LOG_THREAD |
                                  Logfile_group::LG_WAITERS_THREAD |
                                  Logfile_group::LG_FLUSH_THREAD;
    
    Uint64 m_last_lsn;
    Uint64 m_last_sync_req_lsn; // Outstanding
    Uint64 m_last_synced_lsn;   // 
    Uint64 m_max_sync_req_lsn;  // User requested lsn
    union {
      Uint64 m_last_read_lsn;
      Uint64 m_last_lcp_lsn;
    };
    Log_waiter_list::Head m_log_sync_waiters;
    
    Buffer_idx m_tail_pos[3]; // 0 is cut, 1 is saved, 2 is current
    Buffer_idx m_file_pos[2]; // 0 tail, 1 head = { file_ptr_i, page_no }
    Uint64 m_free_file_words; // Free words in logfile group 
    
    Undofile_list::Head m_files;     // Files in log
    Undofile_list::Head m_meta_files;// Files being created or dropped
    
    Uint32 m_free_buffer_words;    // Free buffer page words
    Log_waiter_list::Head m_log_buffer_waiters;
    Page_map::Head m_buffer_pages; // Pairs of { ptr.i, count }
    struct Position {
      Buffer_idx m_current_page;   // { m_buffer_pages.i, left in range }
      Buffer_idx m_current_pos;    // { page ptr.i, m_words_used }
    } m_pos[2]; // 0 is reader (lgman) 1 is writer (tup)

    Uint32 nextHash;
    Uint32 prevHash;
    Uint32 nextList;
    union {
      Uint32 prevList;
      Uint32 nextPool;
    };
    Uint32 hashValue() const {
      return key;
    }
    bool equal(const Logfile_group& rec) const {
      return key == rec.key;
    }
  };

  typedef RecordPool<Logfile_group, RWPool> Logfile_group_pool;
  typedef DLFifoListImpl<Logfile_group_pool, Logfile_group> Logfile_group_list;
  typedef LocalDLFifoListImpl<Logfile_group_pool, Logfile_group> Local_logfile_group_list;
  typedef KeyTableImpl<Logfile_group_pool, Logfile_group> Logfile_group_hash;

  /**
   * Alloc/free space in log
   *   Alloction will be removed at either/or
   *   1) Logfile_client::add_entry
   *   2) free_log_space
   */
  int alloc_log_space(Uint32 logfile_ref, Uint32 words);
  int free_log_space(Uint32 logfile_ref, Uint32 words);
  
private:
  friend class Logfile_client;
  
  Undofile_pool m_file_pool;
  Logfile_group_pool m_logfile_group_pool;
  Log_waiter_pool m_log_waiter_pool;

  Page_map::DataBufferPool m_data_buffer_pool;

  Uint64 m_last_lsn;
  Uint32 m_latest_lcp;
  Logfile_group_list m_logfile_group_list;
  Logfile_group_hash m_logfile_group_hash;

  bool alloc_logbuffer_memory(Ptr<Logfile_group>, Uint32 pages);
  void init_logbuffer_pointers(Ptr<Logfile_group>);
  void free_logbuffer_memory(Ptr<Logfile_group>);
  Uint32 compute_free_file_pages(Ptr<Logfile_group>);
  Uint32* get_log_buffer(Ptr<Logfile_group>, Uint32 sz);
  void process_log_buffer_waiters(Signal* signal, Ptr<Logfile_group>);
  Uint32 next_page(Logfile_group* ptrP, Uint32 i);

  void force_log_sync(Signal*, Ptr<Logfile_group>, Uint32 lsnhi, Uint32 lnslo);
  void process_log_sync_waiters(Signal* signal, Ptr<Logfile_group>);
  
  void cut_log_tail(Signal*, Ptr<Logfile_group> ptr);
  void endlcp_callback(Signal*, Uint32, Uint32);
  void open_file(Signal*, Ptr<Undofile>, Uint32 requestInfo);

  void flush_log(Signal*, Ptr<Logfile_group>, Uint32 force);
  Uint32 write_log_pages(Signal*, Ptr<Logfile_group>, 
			 Uint32 pageId, Uint32 pages);

  void find_log_head(Signal* signal, Ptr<Logfile_group> ptr);
  void find_log_head_in_file(Signal*, Ptr<Logfile_group>,Ptr<Undofile>,Uint64);

  void init_run_undo_log(Signal*);
  void read_undo_log(Signal*, Ptr<Logfile_group> ptr);
  Uint32 read_undo_pages(Signal*, Ptr<Logfile_group>, 
			 Uint32 pageId, Uint32 pages);
  
  void execute_undo_record(Signal*);
  const Uint32* get_next_undo_record(Uint64* lsn);
  void stop_run_undo_log(Signal* signal);
  void init_tail_ptr(Signal* signal, Ptr<Logfile_group> ptr);

  bool find_file_by_id(Ptr<Undofile>&, Undofile_list::Head&, Uint32 id);
  void create_file_commit(Signal* signal, Ptr<Logfile_group>, Ptr<Undofile>);
  void create_file_abort(Signal* signal, Ptr<Logfile_group>, Ptr<Undofile>);

#ifdef VM_TRACE
  void validate_logfile_group(Ptr<Logfile_group> ptr, const char * = 0);
#else
  void validate_logfile_group(Ptr<Logfile_group> ptr, const char * = 0) {}
#endif

  void drop_filegroup_drop_files(Signal*, Ptr<Logfile_group>, 
				 Uint32 ref, Uint32 data);
};

class Logfile_client {
  Uint32 m_block;
  Lgman * m_lgman;
public:
  Uint32 m_logfile_group_id;

  Logfile_client() {}
  Logfile_client(SimulatedBlock* block, Lgman*, Uint32 logfile_group_id);

  struct Request
  {
    SimulatedBlock::Callback m_callback;
  };
  
  /**
   * Request flags
   */
  enum RequestFlags 
  {
  };
  
  /**
   * Make sure a lsn is stored
   * @return -1, on error
   *          0, request in queued
   *         >0, done
   */
  int sync_lsn(Signal*, Uint64, Request*, Uint32 flags);

  /**
   * Undolog entries
   */
  struct Change
  { 
    const void * ptr;
    Uint32 len;
  };

  Uint64 add_entry(const void*, Uint32 len);
  Uint64 add_entry(const Change*, Uint32 cnt);

  Uint64 add_entry(Local_key, void * base, Change*);
  Uint64 add_entry(Local_key, Uint32 off, Uint32 change);

  /**
   * Check for space in log buffer
   *
   *   return >0 if available
   *           0 on time slice
   *          -1 on error
   */
  int get_log_buffer(Signal*, Uint32 sz, SimulatedBlock::Callback* m_callback);
  
private:
  Uint32* get_log_buffer(Uint32 sz);
};


#endif
