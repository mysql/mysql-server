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

#ifndef TSMAN_H
#define TSMAN_H

#include <SimulatedBlock.hpp>
#include <ndb_limits.h>
#include <IntrusiveList.hpp>
#include <NodeBitmask.hpp>
#include <signaldata/GetTabInfo.hpp>
#include <signaldata/Extent.hpp>
#include <SafeMutex.hpp>

#include "lgman.hpp"
#include "pgman.hpp"

#define JAM_FILE_ID 456

class FsReadWriteReq;

class Tsman : public SimulatedBlock
{
public:
  Tsman(Block_context&);
  ~Tsman() override;
  BLOCK_DEFINES(Tsman);
  
public:
  void execFSWRITEREQ(const FsReadWriteReq* req) const /* called direct cross threads from Ndbfs */;

protected:
  void execSTTOR(Signal* signal);
  void sendSTTORRY(Signal*);
  void execREAD_CONFIG_REQ(Signal* signal);
  void execDUMP_STATE_ORD(Signal* signal);
  void execCONTINUEB(Signal* signal);
  void execNODE_FAILREP(Signal* signal);

  void execCREATE_FILE_IMPL_REQ(Signal* signal);
  void execCREATE_FILEGROUP_IMPL_REQ(Signal* signal);
  void execDROP_FILE_IMPL_REQ(Signal* signal);
  void execDROP_FILEGROUP_IMPL_REQ(Signal* signal);

  void execSTART_RECREQ(Signal*);
  
  void execFSOPENREF(Signal*);
  void execFSOPENCONF(Signal*);
  void execFSREADREF(Signal*);
  void execFSREADCONF(Signal*);

  void execFSCLOSEREF(Signal*);
  void fscloseconf(Signal*);
  void execFSCLOSECONF(Signal*);

  void execALLOC_EXTENT_REQ(Signal*);
  void execFREE_EXTENT_REQ(Signal*);

  void execALLOC_PAGE_REQ(Signal* signal);

  void execLCP_FRAG_ORD(Signal*);
  void execEND_LCPREQ(Signal*);
  void end_lcp(Signal*, Uint32 tablespace, Uint32 list, Uint32 file);

  void execGET_TABINFOREQ(Signal*);

  void sendGET_TABINFOREF(Signal* signal,
			  GetTabInfoReq * req,
			  GetTabInfoRef::ErrorCode errorCode);

public:
  struct Datafile 
  {
    Datafile(){}
    Datafile(const struct CreateFileImplReq*);

    /**
     * m_file_no
     * - Unique among datafiles on this node
     * - Part of local key
     * - Set by pgman
     */
    Uint32 m_magic;
    Uint32 m_file_no;
    Uint32 m_file_id;        // Used when talking to DICT
    Uint32 m_ndb_version;    // Version of data file
    Uint32 m_fd; // NDBFS
    Uint64 m_file_size;

    Uint32 m_tablespace_ptr_i;
    Uint32 m_extent_size;   
    Uint16 m_state;
    Uint16 m_ref_count;

    enum FileState 
    {
      FS_CREATING = 0x1,
      FS_ONLINE   = 0x2,
      FS_DROPPING = 0x4,
      FS_ERROR_CLOSE = 0x8
    };
    
    union {
      struct {
	Uint32 m_first_free_extent;
	Uint32 m_lcp_free_extent_head; // extents freed but not LCP
	Uint32 m_lcp_free_extent_tail;
        Uint32 m_lcp_free_extent_count;
	Uint32 m_offset_data_pages;    // 1(zero) + extent header pages
	Uint32 m_data_pages;
	Uint32 m_used_extent_cnt;
	Uint32 m_extent_headers_per_extent_page;
      } m_online;
      struct {
	Uint32 m_senderData;
	Uint32 m_senderRef;
	Uint32 m_data_pages;
	Uint32 m_extent_pages;
	Uint32 m_requestInfo;
	union {
	  Uint32 m_page_ptr_i;
	  Uint32 m_loading_extent_page;
	};
        Uint32 m_error_code;
      } m_create;
    };
    
    Uint32 nextHash;
    Uint32 prevHash;
    Uint32 nextList;
    union {
      Uint32 prevList;
      Uint32 nextPool;
    };

#define NUM_EXTENT_PAGE_MUTEXES 32
    NdbMutex m_extent_page_mutex[NUM_EXTENT_PAGE_MUTEXES];

    Uint32 hashValue() const {
      return m_file_no;
    }
    bool equal(const Datafile& rec) const {
      return m_file_no == rec.m_file_no;
    }
  };

  typedef RecordPool<RWPool<Datafile> > Datafile_pool;
  typedef DLFifoList<Datafile_pool> Datafile_list;
  typedef LocalDLFifoList<Datafile_pool> Local_datafile_list;
  typedef DLHashTable<Datafile_pool> Datafile_hash;

  struct Tablespace
  {
    Tablespace(){}
    Tablespace(Tsman*, const struct CreateFilegroupImplReq*);
    
    Uint32 m_magic;
    union {
      Uint32 key;
      Uint32 m_tablespace_id;
    };
    Uint32 m_version;
    Uint16 m_state;
    Uint16 m_ref_count; // Can't release when m_ref_count > 0

    enum TablespaceState 
    {
      TS_CREATING = 0x1,
      TS_ONLINE = 0x2,
      TS_DROPPING = 0x4
    };

    Uint32 m_extent_size;       // In pages
    Datafile_list::Head m_free_files; // Files w/ free space
    Tsman* m_tsman;
    Uint32 m_logfile_group_id;

    Datafile_list::Head m_full_files; // Files wo/ free space
    Datafile_list::Head m_meta_files; // Files being created/dropped

    // Total extents of a tablespace (sum of data page extents of all files)
    Uint64 m_total_extents;
    Uint64 m_total_used_extents;  // Total extents used from a tablespace
    
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
    bool equal(const Tablespace& rec) const {
      return key == rec.key;
    }
  };

  typedef RecordPool<RWPool<Tablespace> > Tablespace_pool;
  typedef DLList<Tablespace_pool> Tablespace_list;
  typedef LocalDLList<Tablespace_pool> Local_tablespace_list;
  typedef KeyTable<Tablespace_pool> Tablespace_hash;

private:
  friend class Tablespace_client;
  Datafile_pool m_file_pool;
  Tablespace_pool m_tablespace_pool;

  bool c_encrypted_filesystem;
  bool m_lcp_ongoing;
  BlockReference m_end_lcp_ref;
  Datafile_hash m_file_hash;
  Tablespace_list m_tablespace_list;
  Tablespace_hash m_tablespace_hash;
  SimulatedBlock * m_pgman;
  Lgman * m_lgman;
  SimulatedBlock * m_tup;

  mutable NdbMutex *m_client_mutex[MAX_NDBMT_LQH_THREADS + 1];
  NdbMutex *m_alloc_extent_mutex;
  void client_lock() const;
  void client_unlock() const;
  void client_lock(Uint32 instance) const;
  void client_unlock(Uint32 instance) const;
  bool is_datafile_ready(Uint32 file_no);
  void lock_extent_page(Uint32 file_no, Uint32 page_no);
  void unlock_extent_page(Uint32 file_no, Uint32 page_no);
  void lock_extent_page(Datafile*, Uint32 page_no);
  void unlock_extent_page(Datafile*, Uint32 page_no);
  void lock_alloc_extent();
  void unlock_alloc_extent();
  
  int open_file(Signal*, Ptr<Tablespace>, Ptr<Datafile>, CreateFileImplReq*,
		SectionHandle* handle);
  void load_extent_pages(Signal* signal, Ptr<Datafile> ptr);
  void load_extent_page_callback(Signal*, Uint32, Uint32);
  void load_extent_page_callback_direct(Signal*, Uint32, Uint32);
  void create_file_ref(Signal*, Ptr<Tablespace>, Ptr<Datafile>, 
		       Uint32,Uint32,Uint32);
  int update_page_free_bits(Signal*, Local_key*, unsigned committed_bits);

  int get_page_free_bits(Signal*, Local_key*, unsigned*, unsigned*);
  int unmap_page(Signal*, Local_key*, unsigned uncommitted_bits);
  int restart_undo_page_free_bits(Signal*,Uint32,Uint32,Uint32,Local_key*, 
				  unsigned committed_bits);
  
  int alloc_extent(Signal* signal, Uint32 tablespace, Local_key* key);
  int alloc_page_from_extent(Signal*, Uint32, Local_key*, Uint32 bits);
  
  void scan_tablespace(Signal*, Uint32 ptrI);
  void scan_datafile(Signal*, Uint32, Uint32);
  void scan_extent_headers(Signal*, Ptr<Datafile>);

  bool find_file_by_id(Ptr<Datafile>&, Datafile_list::Head&, Uint32 id);
  void create_file_abort(Signal* signal, Ptr<Datafile>);

  void release_extent_pages(Signal* signal, Ptr<Datafile> ptr);
  void release_extent_pages_callback(Signal*, Uint32, Uint32);
  void release_extent_pages_callback_direct(Signal*, Uint32, Uint32);

  struct req
  {
    Uint32 m_extent_pages;
    Uint32 m_extent_size;
    Uint32 m_extent_no;      // on extent page
    Uint32 m_extent_page_no;
  };
  
  struct req lookup_extent(Uint32 page_no, const Datafile*) const;
  Uint32 calc_page_no_in_extent(Uint32 page_no, const struct req* val) const;
  Uint64 calculate_extent_pages_in_file(Uint64 extents,
                                        Uint32 extent_size,
                                        Uint64 data_pages,
                                        bool v2);
  void sendEND_LCPCONF(Signal*);
  void get_set_extent_info(Signal *signal,
                           Local_key &key,
                           Uint32 &tableId,
                           Uint32 &fragId,
                           Uint32 &create_table_version,
                           bool read);
};

inline
Tsman::req
Tsman::lookup_extent(Uint32 page_no, const Datafile * filePtrP) const
{
  struct req val;
  val.m_extent_size = filePtrP->m_extent_size;
  val.m_extent_pages = filePtrP->m_online.m_offset_data_pages;
  Uint32 per_page = filePtrP->m_online.m_extent_headers_per_extent_page;
  
  Uint32 extent = 
    (page_no - val.m_extent_pages) / val.m_extent_size + per_page;
  
  val.m_extent_page_no = extent / per_page;
  val.m_extent_no = extent % per_page;
  return val;
}

inline
Uint32 
Tsman::calc_page_no_in_extent(Uint32 page_no, const Tsman::req* val) const
{
  return (page_no - val->m_extent_pages) % val->m_extent_size;
}

class Tablespace_client
{
public:
  Uint32 m_block;
  Tsman * m_tsman;
  Signal* m_signal;
  Uint32 m_table_id;
  Uint32 m_fragment_id;
  Uint32 m_create_table_version;
  Uint32 m_tablespace_id;
  DEBUG_OUT_DEFINES(TSMAN);

public:
  Tablespace_client(Signal* signal,
                    SimulatedBlock* block,
                    Tsman* tsman, 
                    Uint32 table,
                    Uint32 fragment,
                    Uint32 create_table_version,
                    Uint32 tablespaceId)
  {
    Uint32 bno = block->number();
    Uint32 ino = block->instance();
    m_block= numberToBlock(bno, ino);
    m_tsman= tsman;
    m_signal= signal;
    m_table_id= table;
    m_fragment_id= fragment;
    m_create_table_version = create_table_version;
    m_tablespace_id= tablespaceId;

    m_tsman->client_lock(ino);
  }

  Tablespace_client(Signal* signal, Tsman* tsman, Local_key* key);//undef

  ~Tablespace_client()
  {
    Uint32 ino = blockToInstance(m_block);
    m_tsman->client_unlock(ino);
  }
  
  /**
   * Return >0 if success, no of pages in extent, sets key
   *        <0 if failure, -error code
   */
  int alloc_extent(Local_key* key);
 
  /**
   * Allocated a page from an extent
   *   performs linear search in extent free bits until it find 
   *   page that has at least <em>bits</em> bits free
   * 
   * Start search from key->m_page_no 
   *   and return found page in key->m_page_no
   *   this make sequential calls find sequential pages
   *
   * If page is found, then the _unlogged_ "page allocated bit" is set
   *   so that page can't be allocated twice unless freed first
   *
   * Note: user of allocated page should use update_page_free_bits
   *       to undo log changes in free space on page
   *
   * Return <0 if none found
   *       >=0 if found, then free bits of page found is returned
   */
  int alloc_page_from_extent(Local_key* key, unsigned bits);

  /**
   * Free extent
   */
  int free_extent(Local_key* key, Uint64 lsn);
  
  /**
   * Update page free bits
   */
  int update_page_free_bits(Local_key*, unsigned bits);
  
  /**
   * Get page free bits
   */
  int get_page_free_bits(Local_key*, 
			 unsigned* uncommitted, unsigned* committed);
  
  /**
   * Update unlogged page free bit
   */
  int unmap_page(Local_key*, Uint32 bits);

  /**
   * Check if datafile is ready for checkpoints.
   */
  bool is_datafile_ready(Uint32 file_no);

  /**
   * Lock/Unlock extent page to ensure that access to this extent
   * page is serialised.
   */
  void lock_extent_page(Uint32 file_no, Uint32 page_no);
  void unlock_extent_page(Uint32 file_no, Uint32 page_no);

  /**
   * Undo handling of page bits
   */
  int restart_undo_page_free_bits(Local_key*, unsigned bits);
  
  /**
   * Get tablespace info
   *
   * Store result in <em>rep</em>
   *
   * Return  0 - on success
   *        <0 - on error
   */
  int get_tablespace_info(CreateFilegroupImplReq* rep);

  /**
   * Update lsn of page corresponding to key
   */
  int update_lsn(Local_key* key, Uint64 lsn);

  /**
   * During UNDO log execution TUP proxy needs a fast method
   * to get the table id and fragment id based on the page id.
   * We get this information from the extent information.
   */
  void get_extent_info(Local_key &key);
  Uint32 get_table_id()
  {
    return m_table_id;
  }
  Uint32 get_fragment_id()
  {
    return m_fragment_id;
  }
  Uint32 get_create_table_version()
  {
    return m_create_table_version;
  }

  /**
   * TUP proxy might discover an extent header that haven't
   * been written. This can happen if the node crashes before
   * completing an LCP before the crash and after the extent
   * was created. In this case we might have pages written
   * but not yet the extent written. These are found since there
   * must be an UNDO log record referring to them since the WAL
   * principle applies here.
   */
  void write_extent_info(Local_key &key);
};

inline
int
Tablespace_client::alloc_extent(Local_key* key)
{
  AllocExtentReq* req = (AllocExtentReq*)m_signal->theData;
  req->request.table_id = m_table_id;
  req->request.fragment_id = m_fragment_id;
  req->request.tablespace_id = m_tablespace_id;
  req->request.create_table_version = m_create_table_version;
  m_tsman->execALLOC_EXTENT_REQ(m_signal);
  
  if(req->reply.errorCode == 0){
    * key = req->reply.page_id;
    D("alloc_extent" << V(*key) << V(req->reply.page_count));
    return req->reply.page_count;
  } else {
    return -(int)req->reply.errorCode;
  }
}

inline
int
Tablespace_client::alloc_page_from_extent(Local_key* key, Uint32 bits)
{
  AllocPageReq* req = (AllocPageReq*)m_signal->theData;
  req->key= *key;
  req->bits= bits;
  req->request.table_id = m_table_id;
  req->request.fragment_id = m_fragment_id;
  req->request.tablespace_id = m_tablespace_id;
  m_tsman->execALLOC_PAGE_REQ(m_signal);

  if(req->reply.errorCode == 0)
  {
    *key = req->key;
    D("alloc_page_from_extent" << V(*key) << V(bits) << V(req->bits));
    return req->bits;
  }
  else
  {
    return -(int)req->reply.errorCode;
  }
}

inline
int
Tablespace_client::free_extent(Local_key* key, Uint64 lsn)
{
  FreeExtentReq* req = (FreeExtentReq*)m_signal->theData;
  req->request.key = *key;
  req->request.table_id = m_table_id;
  req->request.tablespace_id = m_tablespace_id;
  req->request.lsn_hi = (Uint32)(lsn >> 32);
  req->request.lsn_lo = (Uint32)(lsn & 0xFFFFFFFF);
  m_tsman->execFREE_EXTENT_REQ(m_signal);
  
  if(req->reply.errorCode == 0){
    D("free_extent" << V(*key) << V(lsn));
    return 0;
  } else {
    return -(int)req->reply.errorCode;
  }
}

inline
int
Tablespace_client::update_page_free_bits(Local_key *key, 
					 unsigned committed_bits)
{
  D("update_page_free_bits" << V(*key) << V(committed_bits));
  return m_tsman->update_page_free_bits(m_signal, key, committed_bits);
}

inline
int
Tablespace_client::get_page_free_bits(Local_key *key, 
				      unsigned* uncommited, 
				      unsigned* commited)
{
  return m_tsman->get_page_free_bits(m_signal, key, uncommited, commited);
}

inline
bool
Tablespace_client::is_datafile_ready(Uint32 file_no)
{
  return m_tsman->is_datafile_ready(file_no);
}

inline
void
Tablespace_client::lock_extent_page(Uint32 file_no, Uint32 page_no)
{
  m_tsman->lock_extent_page(file_no, page_no);
}

inline
void
Tablespace_client::unlock_extent_page(Uint32 file_no, Uint32 page_no)
{
  m_tsman->unlock_extent_page(file_no, page_no);
}

inline
int
Tablespace_client::unmap_page(Local_key *key, unsigned uncommitted_bits)
{
  return m_tsman->unmap_page(m_signal, key, uncommitted_bits);
}

inline
int 
Tablespace_client::restart_undo_page_free_bits(Local_key* key, 
					       unsigned committed_bits)
{
  return m_tsman->restart_undo_page_free_bits(m_signal,
					      m_table_id,
					      m_fragment_id,
                                              m_create_table_version,
					      key, 
					      committed_bits);
}

inline
void
Tablespace_client::get_extent_info(Local_key &key)
{
  m_tsman->get_set_extent_info(m_signal,
                               key,
                               m_table_id,
                               m_fragment_id,
                               m_create_table_version,
                               true);
}

inline
void
Tablespace_client::write_extent_info(Local_key &key)
{
  m_tsman->get_set_extent_info(m_signal,
                               key,
                               m_table_id,
                               m_fragment_id,
                               m_create_table_version,
                               false);
}
#undef JAM_FILE_ID

#endif
