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

#include "tsman.hpp"
#include "pgman.hpp"
#include "diskpage.hpp"
#include <signaldata/FsRef.hpp>
#include <signaldata/FsConf.hpp>
#include <signaldata/FsOpenReq.hpp>
#include <signaldata/FsCloseReq.hpp>
#include <signaldata/CreateFilegroupImpl.hpp>
#include <signaldata/DropFilegroupImpl.hpp>
#include <signaldata/FsReadWriteReq.hpp>
#include <signaldata/Extent.hpp>
#include <signaldata/DumpStateOrd.hpp>
#include <signaldata/TsmanContinueB.hpp>
#include <signaldata/GetTabInfo.hpp>
#include <signaldata/NodeFailRep.hpp>
#include <dbtup/Dbtup.hpp>

#define JAM_FILE_ID 359


#define JONAS 0

#define COMMITTED_MASK   ((1 << 0) | (1 << 1))
#define UNCOMMITTED_MASK ((1 << 2) | (1 << 3))
#define UNCOMMITTED_SHIFT 2

#define DBG_UNDO 0

Tsman::Tsman(Block_context& ctx) :
  SimulatedBlock(TSMAN, ctx),
  m_file_hash(m_file_pool),
  m_tablespace_list(m_tablespace_pool),
  m_tablespace_hash(m_tablespace_pool),
  m_pgman(0),
  m_lgman(0),
  m_tup(0),
  m_client_mutex("tsman-client", 2, true)
{
  BLOCK_CONSTRUCTOR(Tsman);

  Uint32 SZ = File_formats::Datafile::EXTENT_HEADER_BITMASK_BITS_PER_PAGE;  
  ndbrequire((COMMITTED_MASK & UNCOMMITTED_MASK) == 0);
  ndbrequire((COMMITTED_MASK | UNCOMMITTED_MASK) == ((1 << SZ) - 1));
  
  // Add received signals
  addRecSignal(GSN_STTOR, &Tsman::execSTTOR);
  addRecSignal(GSN_READ_CONFIG_REQ, &Tsman::execREAD_CONFIG_REQ);
  addRecSignal(GSN_DUMP_STATE_ORD, &Tsman::execDUMP_STATE_ORD);
  addRecSignal(GSN_CONTINUEB, &Tsman::execCONTINUEB);
  addRecSignal(GSN_NODE_FAILREP, &Tsman::execNODE_FAILREP);

  addRecSignal(GSN_CREATE_FILE_IMPL_REQ, &Tsman::execCREATE_FILE_IMPL_REQ);
  addRecSignal(GSN_CREATE_FILEGROUP_IMPL_REQ, &Tsman::execCREATE_FILEGROUP_IMPL_REQ);

  addRecSignal(GSN_DROP_FILE_IMPL_REQ, &Tsman::execDROP_FILE_IMPL_REQ);
  addRecSignal(GSN_DROP_FILEGROUP_IMPL_REQ, &Tsman::execDROP_FILEGROUP_IMPL_REQ);

  addRecSignal(GSN_FSWRITEREQ, &Tsman::execFSWRITEREQ);

  addRecSignal(GSN_FSOPENREF, &Tsman::execFSOPENREF, true);
  addRecSignal(GSN_FSOPENCONF, &Tsman::execFSOPENCONF);

  //addRecSignal(GSN_FSCLOSEREF, &Tsman::execFSCLOSEREF);
  addRecSignal(GSN_FSCLOSECONF, &Tsman::execFSCLOSECONF);
  addRecSignal(GSN_FSREADCONF, &Tsman::execFSREADCONF);

  addRecSignal(GSN_ALLOC_EXTENT_REQ, &Tsman::execALLOC_EXTENT_REQ);
  addRecSignal(GSN_FREE_EXTENT_REQ, &Tsman::execFREE_EXTENT_REQ);
  
  addRecSignal(GSN_START_RECREQ, &Tsman::execSTART_RECREQ);

  addRecSignal(GSN_LCP_FRAG_ORD, &Tsman::execLCP_FRAG_ORD);
  addRecSignal(GSN_END_LCPREQ, &Tsman::execEND_LCPREQ);

  addRecSignal(GSN_GET_TABINFOREQ, &Tsman::execGET_TABINFOREQ);

  m_tablespace_hash.setSize(10);
  m_file_hash.setSize(10);
  m_lcp_ongoing = false;

  if (isNdbMtLqh()) {
    jam();
    int ret = m_client_mutex.create();
    ndbrequire(ret == 0);
  }
}
  
Tsman::~Tsman()
{
  if (isNdbMtLqh()) {
    (void)m_client_mutex.destroy();
  }
}

void
Tsman::client_lock(BlockNumber block, int line)
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
Tsman::client_unlock(BlockNumber block, int line)
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

BLOCK_FUNCTIONS(Tsman)

void 
Tsman::execREAD_CONFIG_REQ(Signal* signal)
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

  m_file_pool.init(RT_TSMAN_FILE, pc);
  m_tablespace_pool.init(RT_TSMAN_FILEGROUP, pc);

  ReadConfigConf * conf = (ReadConfigConf*)signal->getDataPtrSend();
  conf->senderRef = reference();
  conf->senderData = senderData;
  sendSignal(ref, GSN_READ_CONFIG_CONF, signal, 
	     ReadConfigConf::SignalLength, JBB);
}

void
Tsman::execSTTOR(Signal* signal) 
{
  jamEntry();                            
  Uint32 startPhase = signal->theData[1];
  switch (startPhase) {
  case 1:
    jam();
    m_pgman = globalData.getBlock(PGMAN);
    m_lgman = (Lgman*)globalData.getBlock(LGMAN);
    m_tup = globalData.getBlock(DBTUP);
    ndbrequire(m_pgman != 0 && m_lgman != 0 && m_tup != 0);
    break;
  }
  sendSTTORRY(signal);
}

void
Tsman::sendSTTORRY(Signal* signal){
  signal->theData[0] = 0;
  signal->theData[3] = 1;
  signal->theData[4] = 255; // No more start phases from missra
  sendSignal(NDBCNTR_REF, GSN_STTORRY, signal, 5, JBB);
}

void
Tsman::execCONTINUEB(Signal* signal)
{
  jamEntry();
  Uint32 type = signal->theData[0];
  Uint32 ptrI = signal->theData[1];
  client_lock(number(), __LINE__);
  switch(type){
  case TsmanContinueB::SCAN_TABLESPACE_EXTENT_HEADERS:
    jam();
    scan_tablespace(signal, ptrI);
    break;
  case TsmanContinueB::SCAN_DATAFILE_EXTENT_HEADERS:
    jam();
    scan_datafile(signal, ptrI, signal->theData[2]);
    break;
  case TsmanContinueB::END_LCP:
    jam();
    end_lcp(signal, ptrI, signal->theData[2], signal->theData[3]);
    break;
  case TsmanContinueB::RELEASE_EXTENT_PAGES:
  {
    jam();
    Ptr<Datafile> ptr;
    m_file_pool.getPtr(ptr, ptrI);
    release_extent_pages(signal, ptr);
    break;
  }
  case TsmanContinueB::LOAD_EXTENT_PAGES:
  {
    jam();
    Ptr<Datafile> ptr;
    m_file_pool.getPtr(ptr, ptrI);
    load_extent_pages(signal, ptr);
    break;
  }
  default:
    ndbrequire(false);
    break;
  }
  client_unlock(number(), __LINE__);
}

void
Tsman::execNODE_FAILREP(Signal* signal)
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

#ifdef VM_TRACE
struct TsmanChunk
{ 
  Uint32 page_count;
  Local_key start_page;
  Vector<Uint32> bitmask;
};
template class Vector<TsmanChunk>;
#endif

void
Tsman::execDUMP_STATE_ORD(Signal* signal)
{
  jamEntry();

  /**
   * 9000
   */

  if(signal->theData[0] == DumpStateOrd::DumpTsman + 0)
  {
    jam();
    Uint32 id = signal->theData[1];

    AllocExtentReq* req = (AllocExtentReq*)signal->theData;
    req->request.tablespace_id = id;
    req->request.table_id = 0;
    req->request.fragment_id = 0;
    execALLOC_EXTENT_REQ(signal);

    if(req->reply.errorCode == 0){
      jam();
      ndbout_c("Success");
      ndbout_c("page: %d %d count: %d", 
	       req->reply.page_id.m_file_no,
	       req->reply.page_id.m_page_no,
	       req->reply.page_count);
    } else {
      jam();
      ndbout_c("Error: %d", req->reply.errorCode); 
    }
  }

  if(signal->theData[0] == DumpStateOrd::DumpTsman + 1)
  {
    jam();
    Uint32 id = signal->theData[1];
    Uint32 file= signal->theData[2];
    Uint32 page= signal->theData[3];
    Uint32 bits= signal->theData[4];

    AllocPageReq* req = (AllocPageReq*)signal->theData;
    req->request.tablespace_id = id;
    req->request.table_id = 0;
    req->request.fragment_id = 0;
    req->key.m_page_no= page;
    req->key.m_file_no= file;
    req->bits= bits;
    execALLOC_PAGE_REQ(signal);

    if(req->reply.errorCode == 0){
      jam();
      ndbout_c("Success");
      ndbout_c("page: %d %d bits: %d", 
	       req->key.m_file_no,
	       req->key.m_page_no,
	       req->bits);
    } else {
      jam();
      ndbout_c("Error: %d", req->reply.errorCode); 
    }
  }

#ifdef VM_TRACE
  if(signal->theData[0] == DumpStateOrd::DumpTsman + 2)
  {
    jam();
    Uint32 id = signal->theData[1];
    Vector<TsmanChunk> chunks;
    for(size_t i = 0; i<1000; i++)
    {
      /**
       * 0) Alloc extent ok
       * 1) toggle page bits
       * 2) Free extent
       */
      Uint32 sz = chunks.size();
      switch((rand() * sz) % 2){
      case 0:
      {
	ndbout_c("case 0");
	AllocExtentReq* req = (AllocExtentReq*)signal->theData;
	req->request.tablespace_id = id;
	req->request.table_id = 0;
	req->request.fragment_id = 0;
	execALLOC_EXTENT_REQ(signal);
	if(req->reply.errorCode == 0){
	  TsmanChunk c;
	  c.start_page = req->reply.page_id;
	  c.page_count = req->reply.page_count;
	  Uint32 words = File_formats::Datafile::extent_header_words(c.page_count);
	  ndbout_c("execALLOC_EXTENT_REQ - OK - [ %d %d ] count: %d(%d)", 
		   c.start_page.m_file_no,
		   c.start_page.m_page_no,
		   c.page_count,
		   words);
	  Uint32 zero = 0;
	  chunks.push_back(c);
	  chunks.back().bitmask.fill(words, zero);

	  ndbout_c("execALLOC_EXTENT_REQ - OK - [ %d %d ] count: %d", 
		   chunks.back().start_page.m_file_no,
		   chunks.back().start_page.m_page_no,
		   chunks.back().page_count);
	} else {
	  ndbout_c("Error: %d", req->reply.errorCode); 
	}
	break;
      }
      case 1:
      {
	Uint32 chunk = rand() % sz;
	Uint32 count = chunks[chunk].page_count;
	Uint32 page = rand() % count;
	ndbout_c("case 1 - %d %d %d", chunk, count, page);
	
	File_formats::Datafile::Extent_header* header =
	  (File_formats::Datafile::Extent_header*)
	  (chunks[chunk].bitmask.getBase());
	Uint32 curr_bits = header->get_free_bits(page);
	Uint32 new_bits = curr_bits ^ rand();
	Local_key key = chunks[chunk].start_page;
	key.m_page_no += page;
	ndbrequire(update_page_free_bits(signal, &key, new_bits) == 0);
      }
      }
    }
  }
#endif

  if(signal->theData[0] == DumpStateOrd::DumpTsman + 3)
  {
    jam();
    GetTabInfoReq* req = (GetTabInfoReq*)signal->theData;
    req->requestType= GetTabInfoReq::RequestById;
    req->tableId= signal->theData[1];

    execGET_TABINFOREQ(signal);
  }
}

void
Tsman::execCREATE_FILEGROUP_IMPL_REQ(Signal* signal)
{
  jamEntry();
  CreateFilegroupImplReq* req= (CreateFilegroupImplReq*)signal->getDataPtr();

  Uint32 senderRef = req->senderRef;
  Uint32 senderData = req->senderData;
  
  Ptr<Tablespace> ptr;
  CreateFilegroupImplRef::ErrorCode err = CreateFilegroupImplRef::NoError;
  do
  {
    if (m_tablespace_hash.find(ptr, req->filegroup_id))
    {
      jam();
      err = CreateFilegroupImplRef::FilegroupAlreadyExists;
      break;
    }

    if (!m_tablespace_pool.seize(ptr))
    {
      jam();
      err = CreateFilegroupImplRef::OutOfFilegroupRecords;
      break;
    }

    new (ptr.p) Tablespace(this, req);
    m_tablespace_hash.add(ptr);
    m_tablespace_list.addFirst(ptr);

    ptr.p->m_state = Tablespace::TS_ONLINE;

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

NdbOut&
operator<<(NdbOut& out, const File_formats::Datafile::Extent_header & obj)
{
  out << "table: " << obj.m_table 
      << " fragment: " << obj.m_fragment_id << " ";
  for(Uint32 i = 0; i<32; i++)
  {
    char t[2];
    BaseString::snprintf(t, sizeof(t), "%x", obj.get_free_bits(i));
    out << t;
  }
  return out;
}

void
Tsman::execDROP_FILEGROUP_IMPL_REQ(Signal* signal)
{
  jamEntry();

  Uint32 errorCode = 0;
  DropFilegroupImplReq req = *(DropFilegroupImplReq*)signal->getDataPtr();  
  Ptr<Tablespace> ptr;
  do 
  {
    if (!m_tablespace_hash.find(ptr, req.filegroup_id))
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
    
    if (! (ptr.p->m_meta_files.isEmpty() && ptr.p->m_free_files.isEmpty() &&
	   ptr.p->m_full_files.isEmpty()))
    {
      jam();
      errorCode = DropFilegroupImplRef::FilegroupInUse;
      break;
    }
    
    switch(req.requestInfo){
    case DropFilegroupImplReq::Prepare:
      jam();
      ptr.p->m_state = Tablespace::TS_DROPPING;
      break;
    case DropFilegroupImplReq::Commit:
      jam();
      if (ptr.p->m_ref_count)
      {
        jam();
        sendSignalWithDelay(reference(), GSN_DROP_FILEGROUP_IMPL_REQ, signal,
                            100, signal->getLength());
        return;
      }
      m_tablespace_list.remove(ptr);
      m_tablespace_hash.release(ptr);
      break;
    case DropFilegroupImplReq::Abort:
      jam();
      ptr.p->m_state = Tablespace::TS_ONLINE;
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

bool 
Tsman::find_file_by_id(Ptr<Datafile>& ptr, 
		       Datafile_list::Head& head, 
		       Uint32 id)
{
  Local_datafile_list list(m_file_pool, head);
  for(list.first(ptr); !ptr.isNull(); list.next(ptr))
  {
    if(ptr.p->m_file_id == id)
    {
      return true;
    }
   }
  return false;
}

void
Tsman::execCREATE_FILE_IMPL_REQ(Signal* signal)
{
  jamEntry();
  client_lock(number(), __LINE__);
  CreateFileImplReq* req= (CreateFileImplReq*)signal->getDataPtr();
  
  Uint32 senderRef = req->senderRef;
  Uint32 senderData = req->senderData;
  
  Ptr<Tablespace> ptr;
  CreateFileImplRef::ErrorCode err = CreateFileImplRef::NoError;
  SectionHandle handle(this, signal);
  do
  {
    if (!m_tablespace_hash.find(ptr, req->filegroup_id))
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

    if (ptr.p->m_state != Tablespace::TS_ONLINE)
    {
      jam();
      err = CreateFileImplRef::FilegroupNotOnline;
      break;
    }
    
    Ptr<Datafile> file_ptr;
    switch(req->requestInfo){
    case CreateFileImplReq::Commit:
    {
      jam();
      ndbrequire(find_file_by_id(file_ptr, ptr.p->m_meta_files, req->file_id));
      file_ptr.p->m_create.m_senderRef = req->senderRef;
      file_ptr.p->m_create.m_senderData = req->senderData;
      file_ptr.p->m_create.m_requestInfo = req->requestInfo;
      
      Page_cache_client pgman(this, m_pgman);
      pgman.map_file_no(signal, file_ptr.p->m_file_no, file_ptr.p->m_fd);
      file_ptr.p->m_create.m_loading_extent_page = 1;
      load_extent_pages(signal, file_ptr);
      client_unlock(number(), __LINE__);
      return;
    }
    case CreateFileImplReq::Abort:
    {
      jam();
      Uint32 senderRef = req->senderRef;
      Uint32 senderData = req->senderData;
      if(find_file_by_id(file_ptr, ptr.p->m_meta_files, req->file_id))
      {
        jam();
	file_ptr.p->m_create.m_senderRef = senderRef;
	file_ptr.p->m_create.m_senderData = senderData;
	file_ptr.p->m_create.m_requestInfo = req->requestInfo;
	create_file_abort(signal, file_ptr);
        client_unlock(number(), __LINE__);
	return;
      }
      else
      {
        jam();
	CreateFileImplConf* conf= (CreateFileImplConf*)signal->getDataPtr();
	conf->senderData = senderData;
	conf->senderRef = reference();
	sendSignal(senderRef, GSN_CREATE_FILE_IMPL_CONF, signal,
		   CreateFileImplConf::SignalLength, JBB);
        client_unlock(number(), __LINE__);
	return;
      }
    }
    default:
      // Prepare
      break;
    }

    ndbrequire(handle.m_cnt > 0);
    
    if (!m_file_pool.seize(file_ptr))
    {
      jam();
      err = CreateFileImplRef::OutOfFileRecords;
      break;
    }
    
    if(ERROR_INSERTED(16000) ||
       (sizeof(void*) == 4 && req->file_size_hi & 0xFFFFFFFF))
    {
      jam();
      releaseSections(handle);

      CreateFileImplRef* ref= (CreateFileImplRef*)signal->getDataPtr();
      ref->senderData = senderData;
      ref->senderRef = reference();
      ref->errorCode = CreateFileImplRef::FileSizeTooLarge;
      sendSignal(senderRef, GSN_CREATE_FILE_IMPL_REF, signal,
                 CreateFileImplRef::SignalLength, JBB);
      client_unlock(number(), __LINE__);
      return;
    }
 
    new (file_ptr.p) Datafile(req);
    Local_datafile_list tmp(m_file_pool, ptr.p->m_meta_files);
    tmp.addFirst(file_ptr);

    file_ptr.p->m_state = Datafile::FS_CREATING;
    file_ptr.p->m_tablespace_ptr_i = ptr.i;
    file_ptr.p->m_extent_size = ptr.p->m_extent_size;

    err = (CreateFileImplRef::ErrorCode)open_file(signal, ptr, file_ptr, req,
                                                  &handle);
    if(err)
    {
      jam();
      break;
    }
    client_unlock(number(), __LINE__);
    return;
  } while(0);
  
  releaseSections(handle);
  CreateFileImplRef* ref= (CreateFileImplRef*)signal->getDataPtr();
  ref->senderData = senderData;
  ref->senderRef = reference();
  ref->errorCode = err;
  sendSignal(senderRef, GSN_CREATE_FILE_IMPL_REF, signal,
	     CreateFileImplRef::SignalLength, JBB);
  client_unlock(number(), __LINE__);
}

static inline Uint64 DIV(Uint64 a, Uint64 b){ return (a + b - 1) / b;}

void
Tsman::release_extent_pages(Signal* signal, Ptr<Datafile> ptr)
{
  Uint32 page = ptr.p->m_create.m_extent_pages;
  if (page > 0)
  {
    Page_cache_client::Request preq;
    preq.m_page.m_file_no = ptr.p->m_file_no;
    preq.m_page.m_page_no = page;
    
    preq.m_callback.m_callbackData = ptr.i;
    preq.m_callback.m_callbackFunction = 
      safe_cast(&Tsman::release_extent_pages_callback);
    
    int page_id;
    int flags = Page_cache_client::UNLOCK_PAGE;
    Page_cache_client pgman(this, m_pgman);
    if((page_id = pgman.get_page(signal, preq, flags)) > 0)
    {
      execute(signal, preq.m_callback, page_id);
    } 
    return;
  }
  
  create_file_abort(signal, ptr);
}

void
Tsman::release_extent_pages_callback(Signal* signal, 
				     Uint32 ptrI,
				     Uint32 page_id)
{
  Ptr<Datafile> ptr;
  m_file_pool.getPtr(ptr, ptrI);
  Local_key key;
  key.m_file_no = ptr.p->m_file_no;
  key.m_page_no = ptr.p->m_create.m_extent_pages;
  Page_cache_client pgman(this, m_pgman);
  ndbrequire(pgman.drop_page(key, page_id));
  ptr.p->m_create.m_extent_pages--;
  
  signal->theData[0] = TsmanContinueB::RELEASE_EXTENT_PAGES;
  signal->theData[1] = ptr.i;
  
  sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
}

void
Tsman::create_file_abort(Signal* signal, Ptr<Datafile> ptr)
{
  if (ptr.p->m_fd == RNIL)
  {
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
Tsman::execFSCLOSECONF(Signal* signal)
{
  Ptr<Datafile> ptr;
  Ptr<Tablespace> lg_ptr;
  Uint32 ptrI = ((FsConf*)signal->getDataPtr())->userPointer;
  m_file_pool.getPtr(ptr, ptrI);
  
  Uint32 senderRef = ptr.p->m_create.m_senderRef;
  Uint32 senderData = ptr.p->m_create.m_senderData;
  
  if (ptr.p->m_state == Datafile::FS_CREATING)
  {
    if (ptr.p->m_file_no != RNIL)
    {
      jam();
      Page_cache_client pgman(this, m_pgman);
      pgman.free_data_file(signal, ptr.p->m_file_no);
    }

    CreateFileImplConf* conf= (CreateFileImplConf*)signal->getDataPtr();
    conf->senderData = senderData;
    conf->senderRef = reference();
    sendSignal(senderRef, GSN_CREATE_FILE_IMPL_CONF, signal,
	       CreateFileImplConf::SignalLength, JBB);
  }
  else if(ptr.p->m_state == Datafile::FS_DROPPING)
  {
    m_file_hash.remove(ptr);
    Page_cache_client pgman(this, m_pgman);
    pgman.free_data_file(signal, ptr.p->m_file_no, ptr.p->m_fd);
    DropFileImplConf* conf= (DropFileImplConf*)signal->getDataPtr();
    conf->senderData = senderData;
    conf->senderRef = reference();
    sendSignal(senderRef, GSN_DROP_FILE_IMPL_CONF, signal,
	       DropFileImplConf::SignalLength, JBB);

  }
  else
  {
    ndbrequire(false);
  }
  
  {
    m_tablespace_pool.getPtr(lg_ptr, ptr.p->m_tablespace_ptr_i);
    Local_datafile_list list(m_file_pool, lg_ptr.p->m_meta_files);
    list.release(ptr);
  }
}

int
Tsman::open_file(Signal* signal, 
		 Ptr<Tablespace> ts_ptr, 
		 Ptr<Datafile> ptr,
		 CreateFileImplReq* org,
		 SectionHandle* handle)
{
  Uint32 requestInfo = org->requestInfo;
  Uint32 hi = org->file_size_hi;
  Uint32 lo = org->file_size_lo;
  
  if(requestInfo == CreateFileImplReq::Create || 
     requestInfo == CreateFileImplReq::CreateForce){
    jam();
    Page_cache_client pgman(this, m_pgman);
    Uint32 file_no = pgman.create_data_file(signal);
    if(file_no == RNIL)
    {
      return CreateFileImplRef::OutOfFileRecords;
    }
    ptr.p->m_file_no = file_no;
  }
  
  FsOpenReq* req = (FsOpenReq*)signal->getDataPtrSend();
  req->userReference = reference();
  req->userPointer = ptr.i;
  
  memset(req->fileNumber, 0, sizeof(req->fileNumber));
  FsOpenReq::setVersion(req->fileNumber, 4); // Version 4 = specified filename
  FsOpenReq::v4_setBasePath(req->fileNumber, FsOpenReq::BP_DD_DF);

  req->fileFlags = 0;
  req->fileFlags |= FsOpenReq::OM_READWRITE;
  req->fileFlags |= FsOpenReq::OM_DIRECT;
  req->fileFlags |= FsOpenReq::OM_THREAD_POOL;
  switch(requestInfo){
  case CreateFileImplReq::Create:
    req->fileFlags |= FsOpenReq::OM_CREATE_IF_NONE;
    req->fileFlags |= FsOpenReq::OM_INIT;
    break;
  case CreateFileImplReq::CreateForce:
    req->fileFlags |= FsOpenReq::OM_CREATE;
    req->fileFlags |= FsOpenReq::OM_INIT;
    break;
  case CreateFileImplReq::Open:
    req->fileFlags |= FsOpenReq::OM_CHECK_SIZE;
    break;
  default:
    ndbrequire(false);
  }

  req->page_size = File_formats::NDB_PAGE_SIZE;
  req->file_size_hi = hi;
  req->file_size_lo = lo;

  Uint64 pages = (Uint64(hi) << 32 | lo) / File_formats::NDB_PAGE_SIZE;
  Uint32 extent_size = ts_ptr.p->m_extent_size; // Extent size in #pages
  Uint64 extents = (pages + extent_size - 1) / extent_size;
  extents = extents ? extents : 1;
  Uint64 data_pages = extents * extent_size;

  Uint32 eh_words = File_formats::Datafile::extent_header_words(extent_size);
  ndbrequire(eh_words < File_formats::Datafile::EXTENT_PAGE_WORDS);
  Uint32 extents_per_page = File_formats::Datafile::EXTENT_PAGE_WORDS/eh_words;
  Uint64 extent_pages = (extents + extents_per_page - 1) / extents_per_page;

  // TODO check overflow in cast
  ptr.p->m_create.m_extent_pages = Uint32(extent_pages);
  ptr.p->m_create.m_data_pages = Uint32(data_pages);

  /**
   * Update file size
   */
  pages = 1 + extent_pages + data_pages;
  Uint64 bytes = pages * File_formats::NDB_PAGE_SIZE;
  hi = (Uint32)(bytes >> 32);
  lo = (Uint32)(bytes & 0xFFFFFFFF);
  req->file_size_hi = hi;
  req->file_size_lo = lo;
#if defined VM_TRACE || defined ERROR_INSERT
  ndbout << "DD tsman: file id:" << ptr.p->m_file_id << " datafile pages/bytes:" << data_pages << "/" << data_pages*File_formats::NDB_PAGE_SIZE << " extent pages:" << extent_pages << endl;
#endif

  sendSignal(NDBFS_REF, GSN_FSOPENREQ, signal, FsOpenReq::SignalLength, JBB,
	     handle);

  return 0;
}

void
Tsman::execFSWRITEREQ(Signal* signal)
{
  /**
   * This is currently run in other thread -> no jam
   *
   * We only run this code when initialising a datafile during its creation.
   * This method is called from NDBFS file system thread to initialise the
   * content in the original pages in the datafile when the datafile is
   * first created. The pages used in this creation is allocated from the
   * DataMemory and is owned by the file system thread, so these can be
   * safely written to. Other than that we can only read stable variables
   * that won't change during the execution in the file system thread.
   */
  //jamEntry();
  Ptr<Datafile> ptr;
  Ptr<GlobalPage> page_ptr;
  FsReadWriteReq* req= (FsReadWriteReq*)signal->getDataPtr();
  
  m_file_pool.getPtr(ptr, req->userPointer);
  m_shared_page_pool.getPtr(page_ptr, req->data.pageData[0]);
  memset(page_ptr.p, 0, File_formats::NDB_PAGE_SIZE);
  
  Uint32 page_no = req->varIndex;
  Uint32 size = ptr.p->m_extent_size;
  Uint32 extent_pages = ptr.p->m_create.m_extent_pages;
  Uint32 datapages = ptr.p->m_create.m_data_pages;

  Uint32 header_words = File_formats::Datafile::extent_header_words(size);
  Uint32 per_page = File_formats::Datafile::EXTENT_PAGE_WORDS / header_words;
  Uint32 extents = datapages/size;
  
  if (page_no == 0)
  {
    //jam();
    Ptr<Tablespace> lg_ptr;
    m_tablespace_hash.getPtr(lg_ptr, ptr.p->m_tablespace_ptr_i);

    File_formats::Datafile::Zero_page* page = 
      (File_formats::Datafile::Zero_page*)page_ptr.p;
    page->m_page_header.init(File_formats::FT_Datafile, 
			     getOwnNodeId(),
			     ndbGetOwnVersion(),
			     (Uint32)time(0));
    page->m_file_no = ptr.p->m_file_no;
    page->m_file_id = ptr.p->m_file_id;
    page->m_tablespace_id = lg_ptr.p->m_tablespace_id;
    page->m_tablespace_version = lg_ptr.p->m_version;
    page->m_data_pages = extents * size;
    page->m_extent_pages = extent_pages;
    page->m_extent_size = size;
    page->m_extent_count = extents;
    page->m_extent_headers_per_page = per_page;
    page->m_extent_header_words = header_words;
    page->m_extent_header_bits_per_page = 
      File_formats::Datafile::EXTENT_HEADER_BITMASK_BITS_PER_PAGE;
  } 
  else if ((page_no-1) < extent_pages)
  {
    //jam();
    
    Uint32 curr_extent = page_no*per_page;
    
    File_formats::Datafile::Extent_page* page = 
      (File_formats::Datafile::Extent_page*)page_ptr.p;
    page->m_page_header.m_page_lsn_hi = 0;
    page->m_page_header.m_page_lsn_lo = 0;
    page->m_page_header.m_page_type = File_formats::PT_Unallocated;
    
    for(Uint32 i = 0; i<per_page; i++)
    {
      File_formats::Datafile::Extent_header * head = page->get_header(i, size);
      memset(head, 0, 4*header_words);
      head->m_table = RNIL;
      head->m_next_free_extent = ++curr_extent;
    }
    if (page_no == extent_pages)
    {
      Uint32 last = extents - ((extent_pages - 1) * per_page);
      page->get_header(last - 1, size)->m_next_free_extent = RNIL;
    }
  }
  else 
  {
    //jam();
    File_formats::Datafile::Data_page* page = 
      (File_formats::Datafile::Data_page*)page_ptr.p;
    page->m_page_header.m_page_lsn_hi = 0;
    page->m_page_header.m_page_lsn_lo = 0;
  }
}

void
Tsman::create_file_ref(Signal* signal, 
		       Ptr<Tablespace> lg_ptr,
		       Ptr<Datafile> ptr, 
		       Uint32 error, Uint32 fsError, Uint32 osError)
{
  CreateFileImplRef* ref= (CreateFileImplRef*)signal->getDataPtr();
  ref->senderData = ptr.p->m_create.m_senderData;
  ref->senderRef = reference();
  ref->errorCode = (CreateFileImplRef::ErrorCode)error;
  ref->fsErrCode = fsError;
  ref->osErrCode = osError;
  sendSignal(ptr.p->m_create.m_senderRef, GSN_CREATE_FILE_IMPL_REF, signal,
	     CreateFileImplRef::SignalLength, JBB);
  
  Local_datafile_list meta(m_file_pool, lg_ptr.p->m_meta_files);
  meta.release(ptr);
}

void
Tsman::execFSOPENREF(Signal* signal)
{
  jamEntry();

  Ptr<Datafile> ptr;  
  Ptr<Tablespace> lg_ptr;
  FsRef* ref = (FsRef*)signal->getDataPtr();

  Uint32 errCode = ref->errorCode;
  Uint32 osErrCode = ref->osErrorCode;

  m_file_pool.getPtr(ptr, ref->userPointer);
  m_tablespace_hash.getPtr(lg_ptr, ptr.p->m_tablespace_ptr_i);

  create_file_ref(signal, lg_ptr, ptr, 
		  CreateFileImplRef::FileError, errCode, osErrCode);
}

void
Tsman::execFSOPENCONF(Signal* signal)
{
  jamEntry();
  Ptr<Datafile> ptr;  
  Ptr<Tablespace> lg_ptr;
  FsConf* conf = (FsConf*)signal->getDataPtr();

  m_file_pool.getPtr(ptr, conf->userPointer);
  m_tablespace_hash.getPtr(lg_ptr, ptr.p->m_tablespace_ptr_i);

  Uint32 fd = ptr.p->m_fd = conf->filePointer;
  
  switch(ptr.p->m_create.m_requestInfo){
  case CreateFileImplReq::Create:
  case CreateFileImplReq::CreateForce:
  {
    jam();
    
    CreateFileImplConf* conf= (CreateFileImplConf*)signal->getDataPtr();
    conf->senderData = ptr.p->m_create.m_senderData;
    conf->senderRef = reference();
    sendSignal(ptr.p->m_create.m_senderRef, GSN_CREATE_FILE_IMPL_CONF, signal,
	       CreateFileImplConf::SignalLength, JBB);
    return;
  }
  case CreateFileImplReq::Open:
  {
    jam();
    /**
     * Read zero page and compare values
     *   can't use page cache as file's file_no is not known
     */
    Ptr<GlobalPage> page_ptr;
    if(m_global_page_pool.seize(page_ptr) == false)
    {
      jam();
      create_file_ref(signal, lg_ptr, ptr, 
		      CreateFileImplRef::OutOfMemory, 0, 0);
      return;
    }

    ptr.p->m_create.m_page_ptr_i = page_ptr.i;

    FsReadWriteReq* req= (FsReadWriteReq*)signal->getDataPtrSend();
    req->filePointer = fd;
    req->userReference = reference();
    req->userPointer = ptr.i;
    req->varIndex = 0;
    req->numberOfPages = 1;
    req->operationFlag = 0;
    FsReadWriteReq::setFormatFlag(req->operationFlag, 
				  FsReadWriteReq::fsFormatGlobalPage);
    req->data.pageData[0] = page_ptr.i;
    sendSignal(NDBFS_REF, GSN_FSREADREQ, signal, 
	       FsReadWriteReq::FixedLength + 1, JBB);
    return;
  }
  }
}

void
Tsman::execFSREADCONF(Signal* signal){
  jamEntry();
  Ptr<Datafile> ptr;  
  Ptr<Tablespace> lg_ptr;
  FsConf* conf = (FsConf*)signal->getDataPtr();
  
  /**
   * We currently on read pages here as part of CREATE_FILE
   *  (other read is done using pgman)
   */
  m_file_pool.getPtr(ptr, conf->userPointer);
  m_tablespace_hash.getPtr(lg_ptr, ptr.p->m_tablespace_ptr_i);

  Ptr<GlobalPage> page_ptr;
  m_global_page_pool.getPtr(page_ptr, ptr.p->m_create.m_page_ptr_i);
  
  File_formats::Datafile::Zero_page* page = 
    (File_formats::Datafile::Zero_page*)page_ptr.p;

  CreateFileImplRef::ErrorCode err = CreateFileImplRef::NoError;
  Uint32 fsError = 0;
  Uint32 osError = 0;
  
  do {
    err = CreateFileImplRef::InvalidFileMetadata;
    fsError = page->m_page_header.validate(File_formats::FT_Datafile, 
					   getOwnNodeId(),
					   ndbGetOwnVersion(),
					   (Uint32)time(0));
    if(fsError)
      break;

    osError = 1;
    if(page->m_file_id != ptr.p->m_file_id)
      break;

    osError = 2;
    if(page->m_tablespace_id != lg_ptr.p->m_tablespace_id)
      break;

    osError = 3;
    if(page->m_tablespace_version != lg_ptr.p->m_version)
      break;

    osError = 4;
    if(page->m_data_pages != ptr.p->m_create.m_data_pages)
      break;

    osError = 5;
    if(page->m_extent_pages != ptr.p->m_create.m_extent_pages)
      break;

    osError = 6;
    if(page->m_extent_size != ptr.p->m_extent_size)
      break;

    osError = 7;
    if(page->m_extent_header_bits_per_page != 
       File_formats::Datafile::EXTENT_HEADER_BITMASK_BITS_PER_PAGE)
      break;

    osError = 8;
    Uint32 eh_words = 
      File_formats::Datafile::extent_header_words(ptr.p->m_extent_size);
    if(page->m_extent_header_words != eh_words)
      break;

    osError = 9;
    Uint32 per_page = File_formats::Datafile::EXTENT_PAGE_WORDS/eh_words;
    if(page->m_extent_headers_per_page != per_page)
      break;
    
    osError = 10;    
    Uint32 extents = page->m_data_pages / ptr.p->m_extent_size;
    if(page->m_extent_count != extents)
      break;

    osError = 11;
    ptr.p->m_file_no = page->m_file_no;
    Page_cache_client pgman(this, m_pgman);
    if(pgman.alloc_data_file(signal, ptr.p->m_file_no) == RNIL)
    {
      jam();
      break;
    }

    /**
     *
     */
    m_global_page_pool.release(page_ptr);

    CreateFileImplConf* conf= (CreateFileImplConf*)signal->getDataPtr();
    conf->senderData = ptr.p->m_create.m_senderData;
    conf->senderRef = reference();
    sendSignal(ptr.p->m_create.m_senderRef, GSN_CREATE_FILE_IMPL_CONF, signal,
	       CreateFileImplConf::SignalLength, JBB);    
    return;
  } while(0);

  m_global_page_pool.release(page_ptr);
  create_file_ref(signal, lg_ptr, ptr, err, fsError, osError);
}

void
Tsman::execFSREADREF(Signal* signal)
{
  jamEntry();
  Ptr<Datafile> ptr;  
  Ptr<Tablespace> lg_ptr;
  FsRef* ref = (FsRef*)signal->getDataPtr();

  m_file_pool.getPtr(ptr, ref->userPointer);
  m_tablespace_hash.find(lg_ptr, ptr.p->m_tablespace_ptr_i);

  m_global_page_pool.release(ptr.p->m_create.m_page_ptr_i);
  create_file_ref(signal, lg_ptr, ptr, CreateFileImplRef::FileReadError, 
		  ref->errorCode, ref->osErrorCode);
}

void
Tsman::load_extent_pages(Signal* signal, Ptr<Datafile> ptr)
{
  /**
   * Currently all extent header pages needs to be locked in memory
   */
  Page_cache_client::Request preq;
  preq.m_page.m_file_no = ptr.p->m_file_no;
  preq.m_page.m_page_no = ptr.p->m_create.m_loading_extent_page;

  preq.m_callback.m_callbackData = ptr.i;
  preq.m_callback.m_callbackFunction = 
    safe_cast(&Tsman::load_extent_page_callback);
  
  int page_id;
  int flags = Page_cache_client::LOCK_PAGE;
  Page_cache_client pgman(this, m_pgman);
  if((page_id = pgman.get_page(signal, preq, flags)) > 0)
  {
    load_extent_page_callback(signal, ptr.i, (Uint32)page_id);
  }
  
  if(page_id < 0)
  {
    ndbrequire(false);
  }
}

void
Tsman::load_extent_page_callback(Signal* signal, 
				 Uint32 callback,
				 Uint32 real_page_ptr_i)
{
  jamEntry();
  Ptr<Datafile> ptr;
  m_file_pool.getPtr(ptr, callback);
  
  if(++ptr.p->m_create.m_loading_extent_page <= ptr.p->m_create.m_extent_pages)
  {
    jam();
    signal->theData[0] = TsmanContinueB::LOAD_EXTENT_PAGES;
    signal->theData[1] = ptr.i;
    sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
    return;
  }

  Uint32 senderRef = ptr.p->m_create.m_senderRef;  
  Uint32 senderData = ptr.p->m_create.m_senderData;
  Uint32 extent_pages = ptr.p->m_create.m_extent_pages;
  Uint32 data_pages = ptr.p->m_create.m_data_pages;
  ndbassert(ptr.p->m_create.m_requestInfo == CreateFileImplReq::Commit);

  Uint32 eh= File_formats::Datafile::extent_header_words(ptr.p->m_extent_size);
  Uint32 per_page = File_formats::Datafile::EXTENT_PAGE_WORDS/eh;

  ptr.p->m_state = Datafile::FS_ONLINE;
  ptr.p->m_online.m_offset_data_pages = 1 + extent_pages;
  ptr.p->m_online.m_first_free_extent = per_page;
  ptr.p->m_online.m_lcp_free_extent_head = RNIL;  
  ptr.p->m_online.m_lcp_free_extent_tail = RNIL;  
  ptr.p->m_online.m_data_pages = data_pages;
  ptr.p->m_online.m_used_extent_cnt = 0;
  ptr.p->m_online.m_extent_headers_per_extent_page = per_page;

  Ptr<Tablespace> ts_ptr;
  m_tablespace_pool.getPtr(ts_ptr, ptr.p->m_tablespace_ptr_i);
  if (getNodeState().startLevel >= NodeState::SL_STARTED ||
      (getNodeState().startLevel == NodeState::SL_STARTING &&
       getNodeState().starting.restartType == NodeState::ST_INITIAL_START) ||
      (getNodeState().getNodeRestartInProgress() &&
       getNodeState().starting.restartType == NodeState::ST_INITIAL_NODE_RESTART))
  {
    jam();
    Local_datafile_list free_list(m_file_pool, ts_ptr.p->m_free_files);
    Local_datafile_list meta(m_file_pool, ts_ptr.p->m_meta_files);
    meta.remove(ptr);
    free_list.addFirst(ptr);
  }
  m_file_hash.add(ptr);
  
  CreateFileImplConf* conf= (CreateFileImplConf*)signal->getDataPtr();
  conf->senderData = senderData;
  conf->senderRef = reference();
  sendSignal(senderRef, GSN_CREATE_FILE_IMPL_CONF, signal,
	     CreateFileImplConf::SignalLength, JBB);
}

void
Tsman::execSTART_RECREQ(Signal* signal)
{
  jamEntry();
  Ptr<Tablespace> lg_ptr;
  m_tablespace_list.first(lg_ptr);
  
  signal->theData[0] = TsmanContinueB::SCAN_TABLESPACE_EXTENT_HEADERS;
  signal->theData[1] = lg_ptr.i;
  sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
}

void
Tsman::scan_tablespace(Signal* signal, Uint32 ptrI)
{
  Ptr<Tablespace> lg_ptr;
  if(ptrI == RNIL)
  {
    jam();
    signal->theData[0] = reference();
    sendSignal(DBLQH_REF, GSN_START_RECCONF, signal, 1, JBB);    
    return;
  }
  
  m_tablespace_pool.getPtr(lg_ptr, ptrI);

  Ptr<Datafile> file_ptr;
  {
    Local_datafile_list meta(m_file_pool, lg_ptr.p->m_meta_files);
    meta.first(file_ptr);
  }

  scan_datafile(signal, lg_ptr.i, file_ptr.i);
}

void
Tsman::scan_datafile(Signal* signal, Uint32 ptrI, Uint32 filePtrI)
{
  Ptr<Datafile> file_ptr;
  Ptr<Tablespace> lg_ptr;
  m_tablespace_pool.getPtr(lg_ptr, ptrI);
  if(filePtrI == RNIL)
  {
    jam();
    m_tablespace_list.next(lg_ptr);
    signal->theData[0] = TsmanContinueB::SCAN_TABLESPACE_EXTENT_HEADERS;
    signal->theData[1] = lg_ptr.i;
    sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
  }
  else
  {
    jam();
    m_file_pool.getPtr(file_ptr, filePtrI);
    scan_extent_headers(signal, file_ptr);
  }
}

void
Tsman::scan_extent_headers(Signal* signal, Ptr<Datafile> ptr)
{
  Ptr<Tablespace> lg_ptr;
  m_tablespace_pool.getPtr(lg_ptr, ptr.p->m_tablespace_ptr_i);

  Uint32 firstFree= RNIL;
  Uint32 size = ptr.p->m_extent_size;
  Uint32 per_page = ptr.p->m_online.m_extent_headers_per_extent_page;
  Uint32 pages= ptr.p->m_online.m_offset_data_pages - 1;
  Uint32 datapages= ptr.p->m_online.m_data_pages;
  for(Uint32 i = 0; i < pages; i++)
  {
    jam();
    Uint32 page_no = pages - i;
    Page_cache_client::Request preq;
    preq.m_page.m_page_no = page_no;
    preq.m_page.m_file_no = ptr.p->m_file_no;
    
    int flags = Page_cache_client::DIRTY_REQ;
    Page_cache_client pgman(this, m_pgman);
    int real_page_id = pgman.get_page(signal, preq, flags);
    ndbrequire(real_page_id > 0);
    D("scan_extent_headers" << V(pages) << V(page_no) << V(real_page_id));

    File_formats::Datafile::Extent_page* page = 
      (File_formats::Datafile::Extent_page*)pgman.m_ptr.p;
    
    Uint32 extents= per_page;
    if(page_no == pages)
    {
      jam();
      /**
       * Last extent header page...
       *   set correct no of extent headers
       */
      Uint32 total_extents = datapages / size;
      extents= total_extents - (pages - 1)*per_page;
    }
    for(Uint32 j = 0; j<extents; j++)
    {
      jam();
      Uint32 extent_no = extents - j - 1;
      File_formats::Datafile::Extent_header* header= 
	page->get_header(extent_no, size);
      if (header->m_table == RNIL)
      {
        jam();
        D("extent free" << V(j));
	header->m_next_free_extent = firstFree;
	firstFree = page_no * per_page + extent_no;
      }
      else
      {
        jam();
	Uint32 tableId= header->m_table;
	Uint32 fragmentId= header->m_fragment_id;
        Dbtup_client tup(this, m_tup);
	Local_key key;
	key.m_file_no = ptr.p->m_file_no;
	key.m_page_no = 
	  pages + 1 + size * (page_no * per_page + extent_no - per_page);
	key.m_page_idx = page_no * per_page + extent_no;
	if(!tup.disk_restart_alloc_extent(tableId, fragmentId, &key, size))
	{
          jamEntry();
	  ptr.p->m_online.m_used_extent_cnt++;
	  for(Uint32 i = 0; i<size; i++, key.m_page_no++)
	  {
            jam();
	    Uint32 bits= header->get_free_bits(i) & COMMITTED_MASK;
	    header->update_free_bits(i, bits | (bits << UNCOMMITTED_SHIFT));
	    tup.disk_restart_page_bits(tableId, fragmentId, &key, 
                                       bits);
	  }
          D("extent used" << V(j) << V(tableId) << V(fragmentId) << V(key));
	}
	else
	{
          jam();
	  header->m_table = RNIL;
	  header->m_next_free_extent = firstFree;
	  firstFree = page_no * per_page + extent_no;
          D("extent free" << V(j) << V(tableId) << V(fragmentId) << V(key));
	}
      }
    }
  }
  ptr.p->m_online.m_first_free_extent= firstFree;
  
  Local_datafile_list meta(m_file_pool, lg_ptr.p->m_meta_files);
  Ptr<Datafile> next = ptr;
  meta.next(next);
  if(firstFree != RNIL)
  {
    jam();
    Local_datafile_list free_list(m_file_pool, lg_ptr.p->m_free_files);
    meta.remove(ptr);
    free_list.addFirst(ptr);
  }
  else
  {
    jam();
    Local_datafile_list full(m_file_pool, lg_ptr.p->m_full_files);
    meta.remove(ptr);
    full.addFirst(ptr);
  }
  
  signal->theData[0] = TsmanContinueB::SCAN_DATAFILE_EXTENT_HEADERS;
  signal->theData[1] = lg_ptr.i;
  signal->theData[2] = next.i;
  sendSignal(reference(), GSN_CONTINUEB, signal, 3, JBB);
}

void
Tsman::execDROP_FILE_IMPL_REQ(Signal* signal)
{
  jamEntry();
  client_lock(number(), __LINE__);
  DropFileImplReq req = *(DropFileImplReq*)signal->getDataPtr();
  Ptr<Datafile> file_ptr;
  Ptr<Tablespace> fg_ptr;
  
  Uint32 errorCode = 0;
  do
  {
    if (!m_tablespace_hash.find(fg_ptr, req.filegroup_id))
    {
      jam();
      errorCode = DropFileImplRef::InvalidFilegroup;
      break;
    }
    
    if (fg_ptr.p->m_version != req.filegroup_version)
    {
      jam();
      errorCode = DropFileImplRef::InvalidFilegroupVersion;
      break;
    }
    
    switch(req.requestInfo){
    case DropFileImplReq::Prepare:{
      if (find_file_by_id(file_ptr, fg_ptr.p->m_full_files, req.file_id))
      {
	jam();
	Local_datafile_list full(m_file_pool, fg_ptr.p->m_full_files);
	full.remove(file_ptr);
      }
      else if(find_file_by_id(file_ptr, fg_ptr.p->m_free_files, req.file_id))
      {
	jam();
	Local_datafile_list free_list(m_file_pool, fg_ptr.p->m_free_files);
	free_list.remove(file_ptr);
      }
      else if(find_file_by_id(file_ptr, fg_ptr.p->m_meta_files, req.file_id))
      {
	jam();
	Local_datafile_list meta(m_file_pool, fg_ptr.p->m_meta_files);
	meta.remove(file_ptr);
      }
      else
      {
        jam();
	errorCode = DropFileImplRef::NoSuchFile;
	break;
      }
      
      Local_datafile_list meta(m_file_pool, fg_ptr.p->m_meta_files);
      meta.addFirst(file_ptr);
      
      if (file_ptr.p->m_online.m_used_extent_cnt || 
	  file_ptr.p->m_state != Datafile::FS_ONLINE)
      {
        jam();
	errorCode = DropFileImplRef::FileInUse;
	break;
      }
      
      file_ptr.p->m_state = Datafile::FS_DROPPING;
      break;
    }
    case DropFileImplReq::Commit:
      ndbrequire(find_file_by_id(file_ptr, fg_ptr.p->m_meta_files, req.file_id));
      jam();
      if (file_ptr.p->m_ref_count)
      {
        jam();
        sendSignalWithDelay(reference(), GSN_DROP_FILE_REQ, signal,
                            100, signal->getLength());
        return;
      }
      
      file_ptr.p->m_create.m_extent_pages = 
	file_ptr.p->m_online.m_offset_data_pages - 1;
      file_ptr.p->m_create.m_senderRef = req.senderRef;
      file_ptr.p->m_create.m_senderData = req.senderData;
      release_extent_pages(signal, file_ptr);
      client_unlock(number(), __LINE__);
      return;
    case DropFileImplReq::Abort:{
      ndbrequire(find_file_by_id(file_ptr, fg_ptr.p->m_meta_files, req.file_id));
      file_ptr.p->m_state = Datafile::FS_ONLINE;
      Local_datafile_list meta(m_file_pool, fg_ptr.p->m_meta_files);
      meta.remove(file_ptr);
      if (file_ptr.p->m_online.m_first_free_extent != RNIL)
      {
        jam();
	Local_datafile_list free_list(m_file_pool, fg_ptr.p->m_free_files);
        free_list.addFirst(file_ptr);
      }
      else
      {
        jam();
	Local_datafile_list full(m_file_pool, fg_ptr.p->m_full_files);
        full.addFirst(file_ptr);
      }
      break;
    }
    }
  } while(0);
  
  if (errorCode)
  {
    jam();
    DropFileImplRef* ref = (DropFileImplRef*)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->senderData = req.senderData;
    ref->errorCode = errorCode;
    sendSignal(req.senderRef, GSN_DROP_FILE_IMPL_REF, signal,
	       DropFileImplRef::SignalLength, JBB);
  }
  else
  {
    jam();
    DropFileImplConf* conf = (DropFileImplConf*)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->senderData = req.senderData;
    sendSignal(req.senderRef, GSN_DROP_FILE_IMPL_CONF, signal,
	       DropFileImplConf::SignalLength, JBB);
  }
  client_unlock(number(), __LINE__);
}

Tsman::Tablespace::Tablespace(Tsman* ts, const CreateFilegroupImplReq* req)
{
  m_tsman = ts;
  m_logfile_group_id = req->tablespace.logfile_group_id;
  m_tablespace_id = req->filegroup_id;
  m_version = req->filegroup_version;
  m_ref_count = 0;
  
  m_extent_size = (Uint32)DIV(req->tablespace.extent_size, File_formats::NDB_PAGE_SIZE);
#if defined VM_TRACE || defined ERROR_INSERT
  ndbout << "DD tsman: ts id:" << m_tablespace_id << " extent pages/bytes:" << m_extent_size << "/" << m_extent_size*File_formats::NDB_PAGE_SIZE  << endl;
#endif
}

Tsman::Datafile::Datafile(const struct CreateFileImplReq* req)
{
  m_file_id = req->file_id;
  
  m_file_no = RNIL;
  m_fd = RNIL;
  m_online.m_first_free_extent = RNIL;
  m_ref_count = 0;
    
  m_create.m_senderRef = req->senderRef; // During META
  m_create.m_senderData = req->senderData; // During META
  m_create.m_requestInfo = req->requestInfo;
}

void
Tsman::execALLOC_EXTENT_REQ(Signal* signal)
{
  EmulatedJamBuffer* const jamBuf = getThrJamBuf();

  thrjamEntry(jamBuf);
  Ptr<Tablespace> ts_ptr;
  Ptr<Datafile> file_ptr;
  AllocExtentReq req = *(AllocExtentReq*)signal->getDataPtr();
  AllocExtentReq::ErrorCode err;
  
  ndbrequire(m_tablespace_hash.find(ts_ptr, req.request.tablespace_id));
  Local_datafile_list tmp(m_file_pool, ts_ptr.p->m_free_files);
  
  if (tmp.first(file_ptr))
  {
    thrjam(jamBuf);
    Uint32 size = file_ptr.p->m_extent_size;
    Uint32 extent = file_ptr.p->m_online.m_first_free_extent;
    Uint32 data_off = file_ptr.p->m_online.m_offset_data_pages;
    Uint32 eh_words = File_formats::Datafile::extent_header_words(size);
    Uint32 per_page = File_formats::Datafile::EXTENT_PAGE_WORDS/eh_words;
    Uint32 page_no = extent / per_page;
    Uint32 extent_no = extent % per_page;

    Page_cache_client::Request preq;
    preq.m_page.m_page_no = page_no;
    preq.m_page.m_file_no = file_ptr.p->m_file_no;

    /**
     * Handling of unmapped extent header pages is not implemented
     */
    int flags = Page_cache_client::DIRTY_REQ;
    int real_page_id;
    Page_cache_client pgman(this, m_pgman);
    if ((real_page_id = pgman.get_page(signal, preq, flags)) > 0)
    {
      thrjam(jamBuf);
      GlobalPage* ptr_p = pgman.m_ptr.p;
      
      File_formats::Datafile::Extent_page* page = 
	(File_formats::Datafile::Extent_page*)ptr_p;
      File_formats::Datafile::Extent_header* header = 
	page->get_header(extent_no, size);
      
      ndbrequire(header->m_table == RNIL);
      Uint32 next_free = header->m_next_free_extent;
      
      /**
       * Init header
       */
      memset(header, 0, 4*eh_words);
      header->m_table = req.request.table_id;
      header->m_fragment_id = req.request.fragment_id;
      
      /**
       * Check if file is full
       */
      file_ptr.p->m_online.m_used_extent_cnt++;
      file_ptr.p->m_online.m_first_free_extent = next_free;
      if (next_free == RNIL)
      {
	thrjam(jamBuf);
	Local_datafile_list full(m_file_pool, ts_ptr.p->m_full_files);
	tmp.remove(file_ptr);
        full.addFirst(file_ptr);
      }
      
      /**
       * Pack return values
       */
      ndbassert(extent >= per_page);
      preq.m_page.m_page_no = data_off + size * (extent - /* zero */ per_page);
      preq.m_page.m_page_idx = extent; // extent_no
      
      AllocExtentReq* rep = (AllocExtentReq*)signal->getDataPtr();
      rep->reply.errorCode = 0;
      rep->reply.page_id = preq.m_page;
      rep->reply.page_count = size;
      return;
    }
    else 
    {
      thrjam(jamBuf);
      err = AllocExtentReq::UnmappedExtentPageIsNotImplemented;
    }
  }
  else
  {
    thrjam(jamBuf);
    err = AllocExtentReq::NoExtentAvailable;
    Local_datafile_list full_tmp(m_file_pool, ts_ptr.p->m_full_files);
    if (tmp.isEmpty() && full_tmp.isEmpty())
    { 
      thrjam(jamBuf);
      err = AllocExtentReq::NoDatafile;
    }
  }
  
  /**
   * Pack return values
   */
  AllocExtentReq* rep = (AllocExtentReq*)signal->getDataPtr();
  rep->reply.errorCode = err;
  return;
}

void
Tsman::execFREE_EXTENT_REQ(Signal* signal)
{
  EmulatedJamBuffer* const jamBuf = getThrJamBuf();

  thrjamEntry(jamBuf);
  Ptr<Datafile> file_ptr;
  FreeExtentReq req = *(FreeExtentReq*)signal->getDataPtr();
  FreeExtentReq::ErrorCode err = (FreeExtentReq::ErrorCode)0;
  
  Datafile file_key;
  file_key.m_file_no = req.request.key.m_file_no;
  ndbrequire(m_file_hash.find(file_ptr, file_key));

  struct req val = lookup_extent(req.request.key.m_page_no, file_ptr.p);
  Uint32 extent = 
    (req.request.key.m_page_no - val.m_extent_pages) / val.m_extent_size + 
    file_ptr.p->m_online.m_extent_headers_per_extent_page;
  
  Page_cache_client::Request preq;
  preq.m_page.m_page_no = val.m_extent_page_no;
  preq.m_page.m_file_no = req.request.key.m_file_no;

  ndbout << "Free extent: " << req.request.key << endl;
  
  /**
   * Handling of unmapped extent header pages is not implemented
   */
  int flags = Page_cache_client::DIRTY_REQ;
  int real_page_id;
  Page_cache_client pgman(this, m_pgman);
  if ((real_page_id = pgman.get_page(signal, preq, flags)) > 0)
  {
    thrjam(jamBuf);
    GlobalPage* ptr_p = pgman.m_ptr.p;
    
    File_formats::Datafile::Extent_page* page = 
      (File_formats::Datafile::Extent_page*)ptr_p;
    File_formats::Datafile::Extent_header* header = 
      page->get_header(val.m_extent_no, val.m_extent_size);
    
    ndbrequire(header->m_table == req.request.table_id);
    header->m_table = RNIL;
        
    file_ptr.p->m_online.m_used_extent_cnt--;
    if (m_lcp_ongoing)
    {
      thrjam(jamBuf);
      header->m_next_free_extent= file_ptr.p->m_online.m_lcp_free_extent_head;
      if(file_ptr.p->m_online.m_lcp_free_extent_head == RNIL)
	file_ptr.p->m_online.m_lcp_free_extent_tail= extent;
      file_ptr.p->m_online.m_lcp_free_extent_head= extent;
    }
    else
    {
      thrjam(jamBuf);
      header->m_next_free_extent = file_ptr.p->m_online.m_first_free_extent;
      if (file_ptr.p->m_online.m_first_free_extent == RNIL)
      {
        thrjam(jamBuf);
	/**
	 * Move from full to free
	 */
	Ptr<Tablespace> ptr;
	m_tablespace_pool.getPtr(ptr, file_ptr.p->m_tablespace_ptr_i);
	Local_datafile_list free_list(m_file_pool, ptr.p->m_free_files);
	Local_datafile_list full(m_file_pool, ptr.p->m_full_files);
	full.remove(file_ptr);
        free_list.addFirst(file_ptr);
      }
      file_ptr.p->m_online.m_first_free_extent = extent;
    }
  }
  else
  {
    thrjam(jamBuf);
    err = FreeExtentReq::UnmappedExtentPageIsNotImplemented;
  }
  
  /**
   * Pack return values
   */
  FreeExtentReq* rep = (FreeExtentReq*)signal->getDataPtr();
  rep->reply.errorCode = err;
  return;
}

int
Tsman::update_page_free_bits(Signal* signal, 
			     Local_key *key, 
			     unsigned committed_bits)
{
  EmulatedJamBuffer* const jamBuf = getThrJamBuf();
  
  thrjamEntry(jamBuf);

  /**
   * 1) Compute which extent_no key belongs to
   * 2) Find out which page extent_no belongs to
   * 3) Undo log m_page_bitmask
   * 4) Update m_page_bitmask
   */   
  Ptr<Datafile> file_ptr;
  Datafile file_key;
  file_key.m_file_no = key->m_file_no;
  ndbrequire(m_file_hash.find(file_ptr, file_key));

  struct req val = lookup_extent(key->m_page_no, file_ptr.p);
  
  Page_cache_client::Request preq;
  preq.m_page.m_page_no = val.m_extent_page_no;
  preq.m_page.m_file_no = key->m_file_no;
  
  /**
   * Handling of unmapped extent header pages is not implemented
   */
  int flags = Page_cache_client::COMMIT_REQ;
  int real_page_id;
  Page_cache_client pgman(this, m_pgman);
  if ((real_page_id = pgman.get_page(signal, preq, flags)) > 0)
  {
    thrjam(jamBuf);
    GlobalPage* ptr_p = pgman.m_ptr.p;
    
    File_formats::Datafile::Extent_page* page = 
      (File_formats::Datafile::Extent_page*)ptr_p;
    File_formats::Datafile::Extent_header* header = 
      page->get_header(val.m_extent_no, val.m_extent_size);
    
    if (header->m_table == RNIL)
    {
      thrjam(jamBuf);
      ndbout << "update page free bits page: " << *key 
	     << " " << *header << endl;
    }

    if (0)
    {
      ndbout << "update page free bits page(" << committed_bits << ") " 
	     << *key << " " << *header << endl;
    }

    ndbrequire(header->m_table != RNIL);

    Uint32 page_no_in_extent = calc_page_no_in_extent(key->m_page_no, &val);
    
    /**
     * Toggle word
     */
    ndbassert((committed_bits & ~(COMMITTED_MASK)) == 0);
    Uint32 src = header->get_free_bits(page_no_in_extent) & UNCOMMITTED_MASK;
    header->update_free_bits(page_no_in_extent, src | committed_bits);
    
    pgman.update_lsn(preq.m_page, 0);

    return 0;
  }
  
  return AllocExtentReq::UnmappedExtentPageIsNotImplemented;
}

int
Tsman::get_page_free_bits(Signal* signal, Local_key *key, 
			  unsigned* uncommitted, 
			  unsigned* committed)
{
  EmulatedJamBuffer* const jamBuf = getThrJamBuf();
  
  thrjamEntry(jamBuf);

  Ptr<Datafile> file_ptr;
  Datafile file_key;
  file_key.m_file_no = key->m_file_no;
  ndbrequire(m_file_hash.find(file_ptr, file_key));
  
  struct req val = lookup_extent(key->m_page_no, file_ptr.p);
  
  Page_cache_client::Request preq;
  preq.m_page.m_page_no = val.m_extent_page_no;
  preq.m_page.m_file_no = key->m_file_no;
  
  /**
   * Handling of unmapped extent header pages is not implemented
   */
  int flags = 0;
  int real_page_id;
  Page_cache_client pgman(this, m_pgman);
  if ((real_page_id = pgman.get_page(signal, preq, flags)) > 0)
  {
    thrjam(jamBuf);
    GlobalPage* ptr_p = pgman.m_ptr.p;
    
    File_formats::Datafile::Extent_page* page = 
      (File_formats::Datafile::Extent_page*)ptr_p;
    File_formats::Datafile::Extent_header* header = 
      page->get_header(val.m_extent_no, val.m_extent_size);
    
    ndbrequire(header->m_table != RNIL);

    Uint32 page_no_in_extent = calc_page_no_in_extent(key->m_page_no, &val);
    Uint32 bits = header->get_free_bits(page_no_in_extent);
    *uncommitted = (bits & UNCOMMITTED_MASK) >> UNCOMMITTED_SHIFT;
    *committed = (bits & COMMITTED_MASK);
    return 0;
  }
  
  return AllocExtentReq::UnmappedExtentPageIsNotImplemented;
}

int
Tsman::unmap_page(Signal* signal, Local_key *key, Uint32 uncommitted_bits)
{
  EmulatedJamBuffer* const jamBuf = getThrJamBuf();
  
  thrjamEntry(jamBuf);

  /**
   * 1) Compute which extent_no key belongs to
   * 2) Find out which page extent_no belongs to
   * 3) Undo log m_page_bitmask
   * 4) Update m_page_bitmask
   */   
  Ptr<Datafile> file_ptr;
  Datafile file_key;
  file_key.m_file_no = key->m_file_no;
  ndbrequire(m_file_hash.find(file_ptr, file_key));

  struct req val = lookup_extent(key->m_page_no, file_ptr.p);
  
  Page_cache_client::Request preq;
  preq.m_page.m_page_no = val.m_extent_page_no;
  preq.m_page.m_file_no = key->m_file_no;
  
  /**
   * Handling of unmapped extent header pages is not implemented
   */
  int flags = 0;
  int real_page_id;
  Page_cache_client pgman(this, m_pgman);
  if ((real_page_id = pgman.get_page(signal, preq, flags)) > 0)
  {
    thrjam(jamBuf);
    GlobalPage* ptr_p = pgman.m_ptr.p;
    
    File_formats::Datafile::Extent_page* page = 
      (File_formats::Datafile::Extent_page*)ptr_p;
    File_formats::Datafile::Extent_header* header = 
      page->get_header(val.m_extent_no, val.m_extent_size);
    
    if (header->m_table == RNIL)
    {
      thrjam(jamBuf);
      ndbout << "trying to unmap page: " << *key 
	     << " " << *header << endl;
    }
    ndbrequire(header->m_table != RNIL);

    Uint32 page_no_in_extent = calc_page_no_in_extent(key->m_page_no, &val);
    
    /**
     * Toggle word
     */
    ndbassert(((uncommitted_bits << UNCOMMITTED_SHIFT) & ~UNCOMMITTED_MASK) == 0);
    Uint32 src = header->get_free_bits(page_no_in_extent) & COMMITTED_MASK;
    header->update_free_bits(page_no_in_extent, 
			     src | (uncommitted_bits << UNCOMMITTED_SHIFT));
  }
  
  return AllocExtentReq::UnmappedExtentPageIsNotImplemented;
}

int
Tsman::restart_undo_page_free_bits(Signal* signal, 
				   Uint32 tableId,
				   Uint32 fragId,
				   Local_key *key, 
				   unsigned bits)
{
  EmulatedJamBuffer* const jamBuf = getThrJamBuf();
  
  thrjamEntry(jamBuf);
  
  /**
   * 1) Compute which extent_no key belongs to
   * 2) Find out which page extent_no belongs to
   * 3) Undo log m_page_bitmask
   * 4) Update m_page_bitmask
   */   
  Ptr<Datafile> file_ptr;
  Datafile file_key;
  file_key.m_file_no = key->m_file_no;
  ndbrequire(m_file_hash.find(file_ptr, file_key));

  struct req val = lookup_extent(key->m_page_no, file_ptr.p);

  Page_cache_client::Request preq;
  preq.m_page.m_page_no = val.m_extent_page_no;
  preq.m_page.m_file_no = key->m_file_no;
  
  /**
   * Handling of unmapped extent header pages is not implemented
   */
  int flags = Page_cache_client::DIRTY_REQ;
  int real_page_id;
  Page_cache_client pgman(this, m_pgman);
  if ((real_page_id = pgman.get_page(signal, preq, flags)) > 0)
  {
    thrjam(jamBuf);
    GlobalPage* ptr_p = pgman.m_ptr.p;
    
    File_formats::Datafile::Extent_page* page = 
      (File_formats::Datafile::Extent_page*)ptr_p;
    File_formats::Datafile::Extent_header* header = 
      page->get_header(val.m_extent_no, val.m_extent_size);
        
    if (header->m_table == RNIL)
    {
      thrjam(jamBuf);
      if (DBG_UNDO)
	ndbout_c("tsman: apply undo - skip table == RNIL");
      return 0;
    }

    Uint32 page_no_in_extent = calc_page_no_in_extent(key->m_page_no, &val);
    Uint32 src = header->get_free_bits(page_no_in_extent);
        
    if (! (header->m_table == tableId && header->m_fragment_id == fragId))
    {
      thrjam(jamBuf);
      ndbout_c("%u %u != %u %u", 
               header->m_table, header->m_fragment_id,
               tableId, fragId);
    }

    ndbrequire(header->m_table == tableId);
    ndbrequire(header->m_fragment_id == fragId);
    
    /**
     * Toggle word
     */
    if (DBG_UNDO)
    {
      ndbout << "tsman: apply " 
	     << *key << " " << (src & COMMITTED_MASK) 
	     << " -> " << bits << endl;
    }
    
    ndbassert((bits & ~(COMMITTED_MASK)) == 0);
    header->update_free_bits(page_no_in_extent, 
			     bits | (bits << UNCOMMITTED_SHIFT));
    
    return 0;
  }
  
  return AllocExtentReq::UnmappedExtentPageIsNotImplemented;
}

void
Tsman::execALLOC_PAGE_REQ(Signal* signal)
{
  EmulatedJamBuffer* const jamBuf = getThrJamBuf();
  
  thrjamEntry(jamBuf);
  AllocPageReq *rep= (AllocPageReq*)signal->getDataPtr();
  AllocPageReq req = *rep;
  AllocPageReq::ErrorCode 
    err= AllocPageReq::UnmappedExtentPageIsNotImplemented;
  
  /**
   * 1) Compute which extent_no key belongs to
   * 2) Find out which page extent_no belongs to
   * 3) Undo log m_page_bitmask
   * 4) Update m_page_bitmask
   */   
  Ptr<Datafile> file_ptr;
  Datafile file_key;
  file_key.m_file_no = req.key.m_file_no;
  ndbrequire(m_file_hash.find(file_ptr, file_key));

  struct req val = lookup_extent(req.key.m_page_no, file_ptr.p);
  Uint32 page_no_in_extent = calc_page_no_in_extent(req.key.m_page_no, &val);
  
  Page_cache_client::Request preq;
  preq.m_page.m_page_no = val.m_extent_page_no;
  preq.m_page.m_file_no = req.key.m_file_no;
  
  Uint32 SZ= File_formats::Datafile::EXTENT_HEADER_BITMASK_BITS_PER_PAGE;

  /**
   * Handling of unmapped extent header pages is not implemented
   */
  int flags = Page_cache_client::DIRTY_REQ;
  int real_page_id;
  Uint32 page_no;
  Uint32 src_bits;
  File_formats::Datafile::Extent_header* header; 
  Page_cache_client pgman(this, m_pgman);
  if ((real_page_id = pgman.get_page(signal, preq, flags)) > 0)
  {
    thrjam(jamBuf);
    GlobalPage* ptr_p = pgman.m_ptr.p;
    
    File_formats::Datafile::Extent_page* page = 
      (File_formats::Datafile::Extent_page*)ptr_p;
    header= page->get_header(val.m_extent_no, val.m_extent_size);
    
    ndbrequire(header->m_table == req.request.table_id);
    
    Uint32 word = header->get_free_word_offset(page_no_in_extent);
    Uint32 shift = SZ * (page_no_in_extent & 7);
    
    /**
     * 0 = 00 - free - 100% free
     * 1 = 01 - atleast 70% free, 70= pct_free + 2 * (100 - pct_free) / 3
     * 2 = 10 - atleast 40% free, 40= pct_free + (100 - pct_free) / 3
     * 3 = 11 - full - less than pct_free% free, pct_free=10%
     */

    Uint32 reqbits = req.bits << UNCOMMITTED_SHIFT;

    /**
     * Search
     */
    Uint32 *src= header->m_page_bitmask + word;
    for(page_no= page_no_in_extent; page_no<val.m_extent_size; page_no++)
    {
      thrjam(jamBuf);
      src_bits= (* src >> shift) & ((1 << SZ) - 1);
      if((src_bits & UNCOMMITTED_MASK) <= reqbits)
      {
        thrjam(jamBuf);
	goto found;
      }
      shift += SZ;
      src = src + (shift >> 5);
      shift &= 31;
    }
    
    shift= 0;
    src= header->m_page_bitmask;
    for(page_no= 0; page_no<page_no_in_extent; page_no++)
    {
      thrjam(jamBuf);
      src_bits= (* src >> shift) & ((1 << SZ) - 1);
      if((src_bits & UNCOMMITTED_MASK) <= reqbits)
      {
        thrjam(jamBuf);
	goto found;
      }
      shift += SZ;
      src = src + (shift >> 5);
      shift &= 31;
    }

#if 0
    printf("req.bits: %d bits: ", req.bits);
    for(Uint32 i = 0; i<size; i++)
    {
      printf("%x", header->get_free_bits(i));
    }
    ndbout_c("");
#endif
    err= AllocPageReq::NoPageFree;
  }
  
  rep->reply.errorCode = err;
  return;
  
found:
  header->update_free_bits(page_no, src_bits | UNCOMMITTED_MASK);
  rep->bits= (src_bits & UNCOMMITTED_MASK) >> UNCOMMITTED_SHIFT;
  rep->key.m_page_no = req.key.m_page_no + page_no - page_no_in_extent;
  rep->reply.errorCode= 0;
  return;
}

void
Tsman::execLCP_FRAG_ORD(Signal* signal)
{
  jamEntry();
  ndbrequire(!m_lcp_ongoing);
  m_lcp_ongoing = true;
}

void
Tsman::execEND_LCPREQ(Signal* signal)
{
  jamEntry();
  ndbrequire(m_lcp_ongoing);
  m_lcp_ongoing = false;

  /**
   * Move extents from "lcp" free list to real free list
   */
  Ptr<Tablespace> ptr;
  if (m_tablespace_list.first(ptr))
  {
    jam();
    ptr.p->m_ref_count ++;
    signal->theData[0] = TsmanContinueB::END_LCP;
    signal->theData[1] = ptr.i;
    signal->theData[2] = 0;    // free
    signal->theData[3] = RNIL; // first
    sendSignal(reference(), GSN_CONTINUEB, signal, 4, JBB);
  }
}

void
Tsman::end_lcp(Signal* signal, Uint32 ptrI, Uint32 list, Uint32 filePtrI)
{
  Ptr<Tablespace> ptr;
  m_tablespace_list.getPtr(ptr, ptrI);
  ndbrequire(ptr.p->m_ref_count);
  ptr.p->m_ref_count--;
  
  Ptr<Datafile> file;
  file.i = filePtrI;
  Uint32 nextFile = RNIL;

  switch(list){
  case 0:
  {
    jam();
    Local_datafile_list tmp(m_file_pool, ptr.p->m_free_files);
    if(file.i == RNIL)
    {
      jam();
      if(!tmp.first(file))
      {
        jam();
	list= 1;
	goto next;
      }
    }
    else
    {
      jam();
      tmp.getPtr(file);
      ndbrequire(file.p->m_ref_count);
      file.p->m_ref_count--;
    }
    break;
  }
  case 1:
  {
    jam();
    Local_datafile_list tmp(m_file_pool, ptr.p->m_full_files);
    if(file.i == RNIL)
    {
      jam();
      if(!tmp.first(file))
      {
        jam();
	list= 0;
	if(m_tablespace_list.next(ptr))
        {
          jam();
	  goto next;
        }
	return;
      }
    }
    else
    {
      jam();
      tmp.getPtr(file);
      ndbrequire(file.p->m_ref_count);
      file.p->m_ref_count--;
    }
    break;
  }
  default:
    ndbrequire(false);
  }
  
  nextFile = file.p->nextList;

  /**
   * Move extents...
   */
  if(file.p->m_online.m_lcp_free_extent_head != RNIL)
  {
    jam();
    ndbout_c("moving extents (%d %d) to real free list %d",
	     file.p->m_online.m_lcp_free_extent_head,
	     file.p->m_online.m_lcp_free_extent_tail,
	     file.p->m_online.m_first_free_extent);
    
    if(file.p->m_online.m_first_free_extent == RNIL)
    {
      jam();
      ndbrequire(list == 1);
      file.p->m_online.m_first_free_extent = 
	file.p->m_online.m_lcp_free_extent_head;
      file.p->m_online.m_lcp_free_extent_head = RNIL;
      file.p->m_online.m_lcp_free_extent_tail = RNIL;

      Local_datafile_list free_list(m_file_pool, ptr.p->m_free_files);
      Local_datafile_list full(m_file_pool, ptr.p->m_full_files);
      full.remove(file);
      free_list.addFirst(file);
    }
    else
    {
      jam();
      Uint32 extent = file.p->m_online.m_lcp_free_extent_tail;
      Uint32 size = ptr.p->m_extent_size;
      Uint32 eh_words = File_formats::Datafile::extent_header_words(size);
      Uint32 per_page = File_formats::Datafile::EXTENT_PAGE_WORDS/eh_words;
      
      Uint32 page_no = extent / per_page;
      Uint32 extent_no = extent % per_page;
      
      Page_cache_client::Request preq;
      preq.m_page.m_page_no = page_no;
      preq.m_page.m_file_no = file.p->m_file_no;
      
      int flags = Page_cache_client::DIRTY_REQ;
      int real_page_id;
      Page_cache_client pgman(this, m_pgman);
      ndbrequire((real_page_id = pgman.get_page(signal, preq, flags)) > 0);
      
      GlobalPage* ptr_p = pgman.m_ptr.p;
      
      File_formats::Datafile::Extent_page* page = 
	(File_formats::Datafile::Extent_page*)ptr_p;
      File_formats::Datafile::Extent_header* header = 
	page->get_header(extent_no, size);
      
      header->m_next_free_extent = file.p->m_online.m_first_free_extent;
      file.p->m_online.m_first_free_extent = 
	file.p->m_online.m_lcp_free_extent_head;
      
      file.p->m_online.m_lcp_free_extent_head = RNIL;
      file.p->m_online.m_lcp_free_extent_tail = RNIL;
    }
  }
  
  
  /**
   * next file
   */
  file.i = nextFile;
  if(file.i == RNIL)
  {
    if(list == 0)
    {
      jam();
      list = 1;
    }
    else
    {
      jam();
      list = 0;
      m_tablespace_list.next(ptr);
    }
  }
  else
  {
    jam();
    ndbrequire(ptr.i != RNIL);
    m_file_pool.getPtr(file);
    file.p->m_ref_count++;
  }
  
next:
  if(ptr.i != RNIL)
  {
    jam();
    ptr.p->m_ref_count++;
    
    signal->theData[0] = TsmanContinueB::END_LCP;
    signal->theData[1] = ptr.i;
    signal->theData[2] = list;    
    signal->theData[3] = file.i;  
    sendSignal(reference(), GSN_CONTINUEB, signal, 4, JBB);
  }
}

int
Tablespace_client::get_tablespace_info(CreateFilegroupImplReq* rep)
{
  EmulatedJamBuffer* const jamBuf = getThrJamBuf();

  thrjamEntry(jamBuf);
  Ptr<Tsman::Tablespace> ts_ptr;  
  if(m_tsman->m_tablespace_hash.find(ts_ptr, m_tablespace_id))
  {
    thrjam(jamBuf);
    Uint32 logfile_group_id = ts_ptr.p->m_logfile_group_id;
    // ctor is used here only for logging
    D("Logfile_client - get_tablespace_info");
    Logfile_client lgman(m_tsman, m_tsman->m_lgman, logfile_group_id, false);
    rep->tablespace.extent_size = ts_ptr.p->m_extent_size;
    rep->tablespace.logfile_group_id = lgman.m_logfile_group_id;
    return 0;
  }
  return -1;
}

void Tsman::execGET_TABINFOREQ(Signal* signal)
{
  jamEntry();

  if(!assembleFragments(signal))
  {
    jam();
    return;
  }

  GetTabInfoReq * const req = (GetTabInfoReq *)&signal->theData[0];

  Uint32 tableId= req->tableId;
  const Uint32 reqType = req->requestType & (~GetTabInfoReq::LongSignalConf);
  BlockReference retRef= req->senderRef;
  Uint32 senderData= req->senderData;

  if(reqType == GetTabInfoReq::RequestByName)
  {
    jam();
    SectionHandle handle(this, signal);
    releaseSections(handle);

    sendGET_TABINFOREF(signal, req, GetTabInfoRef::NoFetchByName);
    return;
  }

  Datafile_hash::Iterator iter;
  if (!m_file_hash.first(iter))
  {
    ndbrequire(false);
    return;                                     // Silence compiler warning
  }

  while(iter.curr.p->m_file_id != tableId && m_file_hash.next(iter))
  {
    jam();
  }

  if(iter.curr.p->m_file_id != tableId)
  {
    jam();
    sendGET_TABINFOREF(signal, req, GetTabInfoRef::InvalidTableId);
    return;
  }

  const Ptr<Datafile> &file_ptr= iter.curr;

  jam();

  Uint32 total_free_extents = file_ptr.p->m_online.m_data_pages;
  total_free_extents /= file_ptr.p->m_extent_size;
  total_free_extents -= file_ptr.p->m_online.m_used_extent_cnt;

  GetTabInfoConf *conf = (GetTabInfoConf *)&signal->theData[0];

  conf->senderData= senderData;
  conf->tableId= tableId;
  conf->freeExtents= total_free_extents;
  conf->tableType= DictTabInfo::Datafile;
  conf->senderRef= reference();
  sendSignal(retRef, GSN_GET_TABINFO_CONF, signal,
	     GetTabInfoConf::SignalLength, JBB);
}

void Tsman::sendGET_TABINFOREF(Signal* signal,
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
