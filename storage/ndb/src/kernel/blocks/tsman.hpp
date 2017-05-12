/*
   Copyright (c) 2005, 2016, Oracle and/or its affiliates. All rights reserved.

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

#ifndef TSMAN_H
#define TSMAN_H

#include <SimulatedBlock.hpp>

#include <IntrusiveList.hpp>
#include <NodeBitmask.hpp>
#include <signaldata/GetTabInfo.hpp>
#include <signaldata/Extent.hpp>
#include <SafeMutex.hpp>

#include "lgman.hpp"
#include "pgman.hpp"

#define JAM_FILE_ID 456


class Tsman : public SimulatedBlock
{
public:
  Tsman(Block_context&);
  virtual ~Tsman();
  BLOCK_DEFINES(Tsman);
  
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
  
  void execFSWRITEREQ(Signal*);
  void execFSOPENREF(Signal*);
  void execFSOPENCONF(Signal*);
  void execFSREADREF(Signal*);
  void execFSREADCONF(Signal*);

  void execFSCLOSEREF(Signal*);
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

    Uint32 hashValue() const {
      return m_file_no;
    }
    bool equal(const Datafile& rec) const {
      return m_file_no == rec.m_file_no;
    }
  };

  typedef RecordPool<Datafile, RWPool<Datafile> > Datafile_pool;
  typedef DLList<Datafile, Datafile_pool> Datafile_list;
  typedef LocalDLList<Datafile, Datafile_pool> Local_datafile_list;
  typedef DLHashTable<Datafile_pool, Datafile> Datafile_hash;

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

  typedef RecordPool<Tablespace, RWPool<Tablespace> > Tablespace_pool;
  typedef DLList<Tablespace, Tablespace_pool> Tablespace_list;
  typedef LocalDLList<Tablespace, Tablespace_pool> Local_tablespace_list;
  typedef KeyTable<Tablespace_pool, Tablespace> Tablespace_hash;

private:
  friend class Tablespace_client;
  Datafile_pool m_file_pool;
  Tablespace_pool m_tablespace_pool;
  
  bool m_lcp_ongoing;
  Datafile_hash m_file_hash;
  Tablespace_list m_tablespace_list;
  Tablespace_hash m_tablespace_hash;
  SimulatedBlock * m_pgman;
  Lgman * m_lgman;
  SimulatedBlock * m_tup;

  SafeMutex m_client_mutex;
  void client_lock(BlockNumber block, int line);
  void client_unlock(BlockNumber block, int line);
  
  int open_file(Signal*, Ptr<Tablespace>, Ptr<Datafile>, CreateFileImplReq*,
		SectionHandle* handle);
  void load_extent_pages(Signal* signal, Ptr<Datafile> ptr);
  void load_extent_page_callback(Signal*, Uint32, Uint32);
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

  struct req
  {
    Uint32 m_extent_pages;
    Uint32 m_extent_size;
    Uint32 m_extent_no;      // on extent page
    Uint32 m_extent_page_no;
  };
  
  struct req lookup_extent(Uint32 page_no, const Datafile*) const;
  Uint32 calc_page_no_in_extent(Uint32 page_no, const struct req* val) const;
  uint64 calculate_extent_pages_in_file(Uint64 extents,
                                        Uint32 extent_size,
                                        Uint64 data_pages,
                                        bool v2);
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
  bool m_lock;
  DEBUG_OUT_DEFINES(TSMAN);

public:
  Tablespace_client(Signal* signal,
                    SimulatedBlock* block,
                    Tsman* tsman, 
                    Uint32 table,
                    Uint32 fragment,
                    Uint32 create_table_version,
                    Uint32 tablespaceId,
                    bool lock = true) {
    Uint32 bno = block->number();
    Uint32 ino = block->instance();
    m_block= numberToBlock(bno, ino);
    m_tsman= tsman;
    m_signal= signal;
    m_table_id= table;
    m_fragment_id= fragment;
    m_create_table_version = create_table_version;
    m_tablespace_id= tablespaceId;
    m_lock = lock;

    D("client ctor " << bno << "/" << ino
      << V(m_table_id) << V(m_fragment_id) << V(m_tablespace_id));
    if (m_lock)
      m_tsman->client_lock(m_block, 0);
  }

  Tablespace_client(Signal* signal, Tsman* tsman, Local_key* key);//undef

  ~Tablespace_client() {
#ifdef VM_TRACE
    Uint32 bno = blockToMain(m_block);
    Uint32 ino = blockToInstance(m_block);
#endif
    D("client dtor " << bno << "/" << ino);
    if (m_lock)
      m_tsman->client_unlock(m_block, 0);
  }
  
  /**
   * Return >0 if success, no of pages in extent, sets key
   *        <0 if failure, -error code
   */
  int alloc_extent(Local_key* key);
  
  /**
   * Allocated a page from an extent
   *   performs linear search in extent free bits until it find 
   *   page that has atleast <em>bits</em> bits free
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
   * Undo handling of page bits
   */
  int restart_undo_page_free_bits(Local_key*, unsigned bits);
  
  /**
   * Get tablespace info
   *
   * Store result in <em>rep</em>
   *
   * Return  0 - on sucess
   *        <0 - on error
   */
  int get_tablespace_info(CreateFilegroupImplReq* rep);

  /**
   * Update lsn of page corresponing to key
   */
  int update_lsn(Local_key* key, Uint64 lsn);
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

  if(req->reply.errorCode == 0){
    *key = req->key;
    D("alloc_page_from_extent" << V(*key) << V(bits) << V(req->bits));
    return req->bits;
  } else {
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


#undef JAM_FILE_ID

#endif
