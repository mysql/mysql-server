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

#include "tsman.hpp"
#include <dbtup/Dbtup.hpp>
#include <signaldata/CreateFilegroupImpl.hpp>
#include <signaldata/DropFilegroupImpl.hpp>
#include <signaldata/DumpStateOrd.hpp>
#include <signaldata/Extent.hpp>
#include <signaldata/FsCloseReq.hpp>
#include <signaldata/FsConf.hpp>
#include <signaldata/FsOpenReq.hpp>
#include <signaldata/FsReadWriteReq.hpp>
#include <signaldata/FsRef.hpp>
#include <signaldata/GetTabInfo.hpp>
#include <signaldata/NodeFailRep.hpp>
#include <signaldata/TsmanContinueB.hpp>
#include "diskpage.hpp"
#include "pgman.hpp"

#define JAM_FILE_ID 359

static bool g_use_old_format = false;

#define COMMITTED_MASK ((1 << 0) | (1 << 1))
#define UNCOMMITTED_MASK ((1 << 2) | (1 << 3))
#define UNCOMMITTED_SHIFT 2

#if (defined(VM_TRACE) || defined(ERROR_INSERT))
//#define DEBUG_TSMAN 1
//#define DEBUG_TSMAN_NUM_EXTENTS 1
//#define DEBUG_TSMAN_RESTART 1
//#define DEBUG_TSMAN_IO 1
#endif

#ifdef DEBUG_TSMAN
#define DEB_TSMAN(arglist)       \
  do {                           \
    g_eventLogger->info arglist; \
  } while (0)
#else
#define DEB_TSMAN(arglist) \
  do {                     \
  } while (0)
#endif

#ifdef DEBUG_TSMAN_NUM_EXTENTS
#define DEB_TSMAN_NUM_EXTENTS(arglist) \
  do {                                 \
    g_eventLogger->info arglist;       \
  } while (0)
#else
#define DEB_TSMAN_NUM_EXTENTS(arglist) \
  do {                                 \
  } while (0)
#endif

#ifdef DEBUG_TSMAN_RESTART
#define DEB_TSMAN_RESTART(arglist) \
  do {                             \
    g_eventLogger->info arglist;   \
  } while (0)
#else
#define DEB_TSMAN_RESTART(arglist) \
  do {                             \
  } while (0)
#endif

#ifdef DEBUG_TSMAN_IO
#define DEB_TSMAN_IO(arglist)    \
  do {                           \
    g_eventLogger->info arglist; \
  } while (0)
#else
#define DEB_TSMAN_IO(arglist) \
  do {                        \
  } while (0)
#endif

#define DBG_UNDO 0

Tsman::Tsman(Block_context &ctx)
    : SimulatedBlock(TSMAN, ctx),
      m_file_hash(m_file_pool),
      m_tablespace_list(m_tablespace_pool),
      m_tablespace_hash(m_tablespace_pool),
      m_pgman(0),
      m_lgman(0),
      m_tup(0) {
  BLOCK_CONSTRUCTOR(Tsman);

  Uint32 SZ = File_formats::Datafile::EXTENT_HEADER_BITMASK_BITS_PER_PAGE;
  ndbrequire((COMMITTED_MASK & UNCOMMITTED_MASK) == 0);
  ndbrequire((COMMITTED_MASK | UNCOMMITTED_MASK) == ((1 << SZ) - 1));

  if (isNdbMtLqh()) {
    jam();
    for (Uint32 i = 0; i < MAX_NDBMT_LQH_THREADS + 1; i++) {
      m_client_mutex[i] = NdbMutex_Create();
      ndbrequire(m_client_mutex[i] != 0);
    }
    m_alloc_extent_mutex = NdbMutex_Create();
    ndbrequire(m_alloc_extent_mutex != 0);
  }
  // Add received signals
  addRecSignal(GSN_STTOR, &Tsman::execSTTOR);
  addRecSignal(GSN_READ_CONFIG_REQ, &Tsman::execREAD_CONFIG_REQ);
  addRecSignal(GSN_DUMP_STATE_ORD, &Tsman::execDUMP_STATE_ORD);
  addRecSignal(GSN_CONTINUEB, &Tsman::execCONTINUEB);
  addRecSignal(GSN_NODE_FAILREP, &Tsman::execNODE_FAILREP);

  addRecSignal(GSN_CREATE_FILE_IMPL_REQ, &Tsman::execCREATE_FILE_IMPL_REQ);
  addRecSignal(GSN_CREATE_FILEGROUP_IMPL_REQ,
               &Tsman::execCREATE_FILEGROUP_IMPL_REQ);

  addRecSignal(GSN_DROP_FILE_IMPL_REQ, &Tsman::execDROP_FILE_IMPL_REQ);
  addRecSignal(GSN_DROP_FILEGROUP_IMPL_REQ,
               &Tsman::execDROP_FILEGROUP_IMPL_REQ);

  addRecSignal(GSN_FSOPENREF, &Tsman::execFSOPENREF, true);
  addRecSignal(GSN_FSOPENCONF, &Tsman::execFSOPENCONF);

  // addRecSignal(GSN_FSCLOSEREF, &Tsman::execFSCLOSEREF);
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
}

Tsman::~Tsman() {
  if (isNdbMtLqh()) {
    for (Uint32 i = 0; i < MAX_NDBMT_LQH_THREADS + 1; i++) {
      NdbMutex_Destroy(m_client_mutex[i]);
      m_client_mutex[i] = 0;
    }
    NdbMutex_Destroy(m_alloc_extent_mutex);
    m_alloc_extent_mutex = 0;
  }
}

BLOCK_FUNCTIONS(Tsman)

void Tsman::execREAD_CONFIG_REQ(Signal *signal) {
  jamEntry();

  const ReadConfigReq *req = (ReadConfigReq *)signal->getDataPtr();

  Uint32 ref = req->senderRef;
  Uint32 senderData = req->senderData;

  const ndb_mgm_configuration_iterator *p =
      m_ctx.m_config.getOwnConfigIterator();
  ndbrequire(p != 0);

  Pool_context pc;
  pc.m_block = this;

#ifdef ERROR_INSERT
  Uint32 disk_data_format = 1;
  ndb_mgm_get_int_parameter(p, CFG_DB_DISK_DATA_FORMAT, &disk_data_format);
  g_use_old_format = (disk_data_format == 0);
#endif
  Uint32 encrypted_filesystem = 0;
  ndb_mgm_get_int_parameter(p, CFG_DB_ENCRYPTED_FILE_SYSTEM,
                            &encrypted_filesystem);
  c_encrypted_filesystem = encrypted_filesystem;

  m_file_pool.init(RT_TSMAN_FILE, pc);
  m_tablespace_pool.init(RT_TSMAN_FILEGROUP, pc);

  ReadConfigConf *conf = (ReadConfigConf *)signal->getDataPtrSend();
  conf->senderRef = reference();
  conf->senderData = senderData;
  sendSignal(ref, GSN_READ_CONFIG_CONF, signal, ReadConfigConf::SignalLength,
             JBB);
}

void Tsman::execSTTOR(Signal *signal) {
  jamEntry();
  Uint32 startPhase = signal->theData[1];
  switch (startPhase) {
    case 1:
      jam();
      m_pgman = globalData.getBlock(PGMAN);
      m_lgman = (Lgman *)globalData.getBlock(LGMAN);
      m_tup = globalData.getBlock(DBTUP);
      ndbrequire(m_pgman != 0 && m_lgman != 0 && m_tup != 0);
      break;
  }
  sendSTTORRY(signal);
}

void Tsman::sendSTTORRY(Signal *signal) {
  signal->theData[0] = 0;
  signal->theData[3] = 1;
  signal->theData[4] = 255;  // No more start phases from missra
  sendSignal(NDBCNTR_REF, GSN_STTORRY, signal, 5, JBB);
}

void Tsman::execCONTINUEB(Signal *signal) {
  jamEntry();
  Uint32 type = signal->theData[0];
  Uint32 ptrI = signal->theData[1];

  if (type == TsmanContinueB::END_LCP) {
    jam();
    end_lcp(signal, ptrI, signal->theData[2], signal->theData[3]);
    return;
  }
  client_lock();
  switch (type) {
    case TsmanContinueB::SCAN_TABLESPACE_EXTENT_HEADERS:
      jam();
      scan_tablespace(signal, ptrI);
      break;
    case TsmanContinueB::SCAN_DATAFILE_EXTENT_HEADERS:
      jam();
      scan_datafile(signal, ptrI, signal->theData[2]);
      break;
    case TsmanContinueB::RELEASE_EXTENT_PAGES: {
      jam();
      Ptr<Datafile> ptr;
      ndbrequire(m_file_pool.getPtr(ptr, ptrI));
      release_extent_pages(signal, ptr);
      break;
    }
    case TsmanContinueB::LOAD_EXTENT_PAGES: {
      jam();
      Ptr<Datafile> ptr;
      ndbrequire(m_file_pool.getPtr(ptr, ptrI));
      load_extent_pages(signal, ptr);
      break;
    }
    default:
      ndbabort();
  }
  client_unlock();
}

void Tsman::execNODE_FAILREP(Signal *signal) {
  jamEntry();
  NodeFailRep *rep = (NodeFailRep *)signal->getDataPtr();
  if (signal->getNoOfSections() >= 1) {
    ndbrequire(ndbd_send_node_bitmask_in_section(
        getNodeInfo(refToNode(signal->getSendersBlockRef())).m_version));
    SegmentedSectionPtr ptr;
    SectionHandle handle(this, signal);
    ndbrequire(handle.getSection(ptr, 0));
    memset(rep->theNodes, 0, sizeof(rep->theNodes));
    copy(rep->theNodes, ptr);
    releaseSections(handle);
  } else {
    memset(rep->theNodes + NdbNodeBitmask48::Size, 0, _NDB_NBM_DIFF_BYTES);
  }
  NdbNodeBitmask failed;
  failed.assign(NdbNodeBitmask::Size, rep->theNodes);

  /* Block level cleanup */
  for (unsigned i = 1; i < MAX_NDB_NODES; i++) {
    jam();
    if (failed.get(i)) {
      jam();
      Uint32 elementsCleaned = simBlockNodeFailure(signal, i);  // No callback
      ndbassert(elementsCleaned == 0);  // No distributed fragmented signals
      (void)elementsCleaned;            // Remove compiler warning
    }                                   // if
  }                                     // for
}

#ifdef VM_TRACE
struct TsmanChunk {
  Uint32 page_count;
  Local_key start_page;
  Vector<Uint32> bitmask;
};
template class Vector<TsmanChunk>;
#endif

void Tsman::execDUMP_STATE_ORD(Signal *signal) {
  jamEntry();

  /**
   * 9000
   */

#if 0
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
      g_eventLogger->info("Success page: %d %d count: %d",
	       req->reply.page_id.m_file_no,
	       req->reply.page_id.m_page_no,
	       req->reply.page_count);
    } else {
      jam();
      g_eventLogger->info("Error: %d", req->reply.errorCode);
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
      g_eventLogger->info("Success page: %d %d bits: %d",
	       req->key.m_file_no,
	       req->key.m_page_no,
	       req->bits);
    } else {
      jam();
      g_eventLogger->info("Error: %d", req->reply.errorCode);
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
	g_eventLogger->info("case 0");
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
	  g_eventLogger->info("execALLOC_EXTENT_REQ - OK - [ %d %d ] count: %d(%d)",
		   c.start_page.m_file_no,
		   c.start_page.m_page_no,
		   c.page_count,
		   words);
	  Uint32 zero = 0;
	  chunks.push_back(c);
	  chunks.back().bitmask.fill(words, zero);

	  g_eventLogger->info("execALLOC_EXTENT_REQ - OK - [ %d %d ] count: %d",
		   chunks.back().start_page.m_file_no,
		   chunks.back().start_page.m_page_no,
		   chunks.back().page_count);
	} else {
	  g_eventLogger->info("Error: %d", req->reply.errorCode);
	}
	break;
      }
      case 1:
      {
	Uint32 chunk = rand() % sz;
	Uint32 count = chunks[chunk].page_count;
	Uint32 page = rand() % count;
	g_eventLogger->info("case 1 - %d %d %d", chunk, count, page);
	
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
#endif
}

void Tsman::execCREATE_FILEGROUP_IMPL_REQ(Signal *signal) {
  jamEntry();
  CreateFilegroupImplReq *req = (CreateFilegroupImplReq *)signal->getDataPtr();

  Uint32 senderRef = req->senderRef;
  Uint32 senderData = req->senderData;

  client_lock();
  Ptr<Tablespace> ptr;
  CreateFilegroupImplRef::ErrorCode err = CreateFilegroupImplRef::NoError;
  do {
    if (m_tablespace_hash.find(ptr, req->filegroup_id)) {
      jam();
      err = CreateFilegroupImplRef::FilegroupAlreadyExists;
      break;
    }

    if (unlikely(ERROR_INSERTED(16001)) || !m_tablespace_pool.seize(ptr)) {
      jam();
      err = CreateFilegroupImplRef::OutOfFilegroupRecords;
      break;
    }

    new (ptr.p) Tablespace(this, req);
    m_tablespace_hash.add(ptr);
    m_tablespace_list.addFirst(ptr);

    ptr.p->m_state = Tablespace::TS_ONLINE;

    client_unlock();

    CreateFilegroupImplConf *conf =
        (CreateFilegroupImplConf *)signal->getDataPtr();
    conf->senderData = senderData;
    conf->senderRef = reference();
    sendSignal(senderRef, GSN_CREATE_FILEGROUP_IMPL_CONF, signal,
               CreateFilegroupImplConf::SignalLength, JBB);
    return;
  } while (0);
  client_unlock();

  CreateFilegroupImplRef *ref = (CreateFilegroupImplRef *)signal->getDataPtr();
  ref->senderData = senderData;
  ref->senderRef = reference();
  ref->errorCode = err;
  sendSignal(senderRef, GSN_CREATE_FILEGROUP_IMPL_REF, signal,
             CreateFilegroupImplRef::SignalLength, JBB);
}

static char *print(char buf[], int n,
                   const File_formats::Datafile::Extent_data &obj) {
  buf[0] = '\0';
  for (Uint32 i = 0; i < 32; i++) {
    BaseString::snappend(buf, n, "%x", obj.get_free_bits(i));
  }
  return buf;
}

void Tsman::execDROP_FILEGROUP_IMPL_REQ(Signal *signal) {
  jamEntry();

  Uint32 errorCode = 0;
  DropFilegroupImplReq req = *(DropFilegroupImplReq *)signal->getDataPtr();
  Ptr<Tablespace> ptr;
  client_lock();
  do {
    if (!m_tablespace_hash.find(ptr, req.filegroup_id)) {
      jam();
      errorCode = DropFilegroupImplRef::NoSuchFilegroup;
      break;
    }

    if (ptr.p->m_version != req.filegroup_version) {
      jam();
      errorCode = DropFilegroupImplRef::InvalidFilegroupVersion;
      break;
    }

    if (!(ptr.p->m_meta_files.isEmpty() && ptr.p->m_free_files.isEmpty() &&
          ptr.p->m_full_files.isEmpty())) {
      jam();
      errorCode = DropFilegroupImplRef::FilegroupInUse;
      break;
    }

    switch (req.requestInfo) {
      case DropFilegroupImplReq::Prepare:
        jam();
        ptr.p->m_state = Tablespace::TS_DROPPING;
        break;
      case DropFilegroupImplReq::Commit:
        jam();
        /** Change the state for the case where CREATE_FILEGROUP_IMPL_REQ
         * aborts (due to another participant fail creating FG)
         * by sending DropFilegroupImplReq::Commit to cleanup this
         * participant without sending DropFilegroupImplReq::Prepare first.
         */
        ptr.p->m_state = Tablespace::TS_DROPPING;

        if (ptr.p->m_ref_count) {
          jam();
          client_unlock();
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
        ndbabort();
    }
  } while (0);

  if (errorCode) {
    jam();
    DropFilegroupImplRef *ref =
        (DropFilegroupImplRef *)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->senderData = req.senderData;
    ref->errorCode = errorCode;
    sendSignal(req.senderRef, GSN_DROP_FILEGROUP_IMPL_REF, signal,
               DropFilegroupImplRef::SignalLength, JBB);
  } else {
    jam();
    DropFilegroupImplConf *conf =
        (DropFilegroupImplConf *)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->senderData = req.senderData;
    sendSignal(req.senderRef, GSN_DROP_FILEGROUP_IMPL_CONF, signal,
               DropFilegroupImplConf::SignalLength, JBB);
  }
  client_unlock();
}

bool Tsman::find_file_by_id(Ptr<Datafile> &ptr, Datafile_list::Head &head,
                            Uint32 id) {
  Local_datafile_list list(m_file_pool, head);
  for (list.first(ptr); !ptr.isNull(); list.next(ptr)) {
    if (ptr.p->m_file_id == id) {
      return true;
    }
  }
  return false;
}

void Tsman::execCREATE_FILE_IMPL_REQ(Signal *signal) {
  jamEntry();
  client_lock();
  CreateFileImplReq *req = (CreateFileImplReq *)signal->getDataPtr();

  Uint32 senderRef = req->senderRef;
  Uint32 senderData = req->senderData;

  Ptr<Tablespace> ptr;
  CreateFileImplRef::ErrorCode err = CreateFileImplRef::NoError;
  SectionHandle handle(this, signal);
  do {
    if (!m_tablespace_hash.find(ptr, req->filegroup_id)) {
      jam();
      err = CreateFileImplRef::InvalidFilegroup;
      break;
    }

    if (ptr.p->m_version != req->filegroup_version) {
      jam();
      err = CreateFileImplRef::InvalidFilegroupVersion;
      break;
    }

    if (ptr.p->m_state != Tablespace::TS_ONLINE) {
      jam();
      err = CreateFileImplRef::FilegroupNotOnline;
      break;
    }

    Ptr<Datafile> file_ptr;
    switch (req->requestInfo) {
      case CreateFileImplReq::Commit: {
        jam();
        ndbrequire(
            find_file_by_id(file_ptr, ptr.p->m_meta_files, req->file_id));
        file_ptr.p->m_create.m_senderRef = req->senderRef;
        file_ptr.p->m_create.m_senderData = req->senderData;
        file_ptr.p->m_create.m_requestInfo = req->requestInfo;

        Page_cache_client pgman(this, m_pgman);
        pgman.map_file_no(signal, file_ptr.p->m_file_no, file_ptr.p->m_fd);
        file_ptr.p->m_create.m_loading_extent_page = 1;
        m_file_hash.add(file_ptr);
        load_extent_pages(signal, file_ptr);
        client_unlock();
        return;
      }
      case CreateFileImplReq::Abort: {
        jam();
        Uint32 senderRef = req->senderRef;
        Uint32 senderData = req->senderData;
        if (find_file_by_id(file_ptr, ptr.p->m_meta_files, req->file_id)) {
          jam();
          file_ptr.p->m_create.m_senderRef = senderRef;
          file_ptr.p->m_create.m_senderData = senderData;
          file_ptr.p->m_create.m_requestInfo = req->requestInfo;
          create_file_abort(signal, file_ptr);
          client_unlock();
          return;
        } else {
          jam();
          CreateFileImplConf *conf = (CreateFileImplConf *)signal->getDataPtr();
          conf->senderData = senderData;
          conf->senderRef = reference();
          sendSignal(senderRef, GSN_CREATE_FILE_IMPL_CONF, signal,
                     CreateFileImplConf::SignalLength, JBB);
          client_unlock();
          return;
        }
      }
      default:
        // Prepare
        break;
    }

    ndbrequire(handle.m_cnt > 0);

    if (!m_file_pool.seize(file_ptr)) {
      jam();
      err = CreateFileImplRef::OutOfFileRecords;
      break;
    }

    if (ERROR_INSERTED(16000) ||
        (sizeof(void *) == 4 && req->file_size_hi & 0xFFFFFFFF)) {
      jam();
      releaseSections(handle);

      CreateFileImplRef *ref = (CreateFileImplRef *)signal->getDataPtr();
      ref->senderData = senderData;
      ref->senderRef = reference();
      ref->errorCode = CreateFileImplRef::FileSizeTooLarge;
      sendSignal(senderRef, GSN_CREATE_FILE_IMPL_REF, signal,
                 CreateFileImplRef::SignalLength, JBB);
      client_unlock();
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
    if (err) {
      jam();
      break;
    }
    client_unlock();
    return;
  } while (0);

  releaseSections(handle);
  CreateFileImplRef *ref = (CreateFileImplRef *)signal->getDataPtr();
  ref->senderData = senderData;
  ref->senderRef = reference();
  ref->errorCode = err;
  sendSignal(senderRef, GSN_CREATE_FILE_IMPL_REF, signal,
             CreateFileImplRef::SignalLength, JBB);
  client_unlock();
}

static inline Uint64 DIV(Uint64 a, Uint64 b) { return (a + b - Uint64(1)) / b; }

void Tsman::release_extent_pages(Signal *signal, Ptr<Datafile> ptr) {
  Uint32 page = ptr.p->m_create.m_extent_pages;
  if (page > 0) {
    Page_cache_client::Request preq;
    preq.m_page.m_file_no = ptr.p->m_file_no;
    preq.m_page.m_page_no = page;
    preq.m_table_id = RNIL;
    preq.m_fragment_id = 0;

    preq.m_callback.m_callbackData = ptr.i;
    preq.m_callback.m_callbackFunction =
        safe_cast(&Tsman::release_extent_pages_callback);

    int page_id;
    int flags = Page_cache_client::UNLOCK_PAGE;
    Page_cache_client pgman(this, m_pgman);
    if ((page_id = pgman.get_page(signal, preq, flags)) > 0) {
      release_extent_pages_callback_direct(signal, ptr.i, page_id);
    }
    return;
  }

  create_file_abort(signal, ptr);
}

void Tsman::release_extent_pages_callback(Signal *signal, Uint32 ptrI,
                                          Uint32 page_id) {
  client_lock();
  release_extent_pages_callback_direct(signal, ptrI, page_id);
  client_unlock();
}

void Tsman::release_extent_pages_callback_direct(Signal *signal, Uint32 ptrI,
                                                 Uint32 page_id) {
  Ptr<Datafile> ptr;
  ndbrequire(m_file_pool.getPtr(ptr, ptrI));
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

void Tsman::create_file_abort(Signal *signal, Ptr<Datafile> ptr) {
  if (ptr.p->m_fd == RNIL) {
    ((FsConf *)signal->getDataPtr())->userPointer = ptr.i;
    fscloseconf(signal);
    return;
  }

  FsCloseReq *req = (FsCloseReq *)signal->getDataPtrSend();
  req->filePointer = ptr.p->m_fd;
  req->userReference = reference();
  req->userPointer = ptr.i;
  req->fileFlag = 0;
  FsCloseReq::setRemoveFileFlag(req->fileFlag, true);

  sendSignal(NDBFS_REF, GSN_FSCLOSEREQ, signal, FsCloseReq::SignalLength, JBB);
}

void Tsman::execFSCLOSECONF(Signal *signal) {
  client_lock();
  fscloseconf(signal);
  client_unlock();
}

void Tsman::fscloseconf(Signal *signal) {
  Ptr<Datafile> ptr;
  Ptr<Tablespace> ts_ptr;
  Uint32 ptrI = ((FsConf *)signal->getDataPtr())->userPointer;

  ndbrequire(m_file_pool.getPtr(ptr, ptrI));

  Uint32 senderRef = ptr.p->m_create.m_senderRef;
  Uint32 senderData = ptr.p->m_create.m_senderData;
  ndbrequire(m_tablespace_pool.getPtr(ts_ptr, ptr.p->m_tablespace_ptr_i));

  if (ptr.p->m_state == Datafile::FS_CREATING) {
    jam();
    if (ptr.p->m_file_no != RNIL) {
      jam();
      Page_cache_client pgman(this, m_pgman);
      pgman.free_data_file(signal, ptr.p->m_file_no);
    }

    CreateFileImplConf *conf = (CreateFileImplConf *)signal->getDataPtr();
    conf->senderData = senderData;
    conf->senderRef = reference();
    sendSignal(senderRef, GSN_CREATE_FILE_IMPL_CONF, signal,
               CreateFileImplConf::SignalLength, JBB);
  } else if (ptr.p->m_state == Datafile::FS_DROPPING) {
    jam();
    for (Uint32 i = 0; i < 32; i++) {
      int ret = NdbMutex_Deinit(&ptr.p->m_extent_page_mutex[i]);
      ndbrequire(ret == 0);
    }
    m_file_hash.remove(ptr);
    Page_cache_client pgman(this, m_pgman);
    pgman.free_data_file(signal, ptr.p->m_file_no, ptr.p->m_fd);
    DropFileImplConf *conf = (DropFileImplConf *)signal->getDataPtr();
    conf->senderData = senderData;
    conf->senderRef = reference();
    sendSignal(senderRef, GSN_DROP_FILE_IMPL_CONF, signal,
               DropFileImplConf::SignalLength, JBB);

  } else if (ptr.p->m_state == Datafile::FS_ERROR_CLOSE) {
    jam();
    create_file_ref(signal, ts_ptr, ptr, ptr.p->m_create.m_error_code, 0, 0);
    return;
  } else {
    ndbabort();
  }

  {
    Local_datafile_list list(m_file_pool, ts_ptr.p->m_meta_files);
    list.release(ptr);
  }
}

Uint64 Tsman::calculate_extent_pages_in_file(Uint64 extents, Uint32 extent_size,
                                             Uint64 data_pages, bool v2) {
  Uint64 eh_words =
      (Uint64)File_formats::Datafile::extent_header_words(extent_size, v2);
  ndbrequire(eh_words < File_formats::Datafile::extent_page_words(v2));
  Uint64 extents_per_page =
      (Uint64)File_formats::Datafile::extent_page_words(v2) / eh_words;
  return (extents + extents_per_page - Uint64(1)) / extents_per_page;
}

int Tsman::open_file(Signal *signal, Ptr<Tablespace> ts_ptr, Ptr<Datafile> ptr,
                     CreateFileImplReq *org, SectionHandle *handle) {
  Uint32 requestInfo = org->requestInfo;
  Uint32 hi = org->file_size_hi;
  Uint32 lo = org->file_size_lo;
  bool v2 = true;

  if (requestInfo == CreateFileImplReq::Create ||
      requestInfo == CreateFileImplReq::CreateForce) {
    jam();
    if (g_use_old_format) {
      v2 = false;
    }

    Page_cache_client pgman(this, m_pgman);
    Uint32 file_no = pgman.create_data_file(signal, v2 ? NDB_DISK_V2 : 0);
    if (file_no == RNIL) {
      return CreateFileImplRef::OutOfFileRecords;
    }
    ptr.p->m_file_no = file_no;
    if (v2) {
      jam();
      ptr.p->m_ndb_version = NDB_DISK_V2;
    } else {
      jam();
      ptr.p->m_ndb_version = 0;
    }
  } else {
    /**
     * We don't know at this point what type of format the file has.
     * We assume v2 format to start with.
     */
    ptr.p->m_ndb_version = NDB_DISK_V2;
    v2 = true;
  }

  FsOpenReq *req = (FsOpenReq *)signal->getDataPtrSend();
  req->userReference = reference();
  req->userPointer = ptr.i;

  memset(req->fileNumber, 0, sizeof(req->fileNumber));
  FsOpenReq::setVersion(req->fileNumber, 4);  // Version 4 = specified filename
  FsOpenReq::v4_setBasePath(req->fileNumber, FsOpenReq::BP_DD_DF);

  req->fileFlags = 0;
  req->fileFlags |= FsOpenReq::OM_READWRITE;
  req->fileFlags |= FsOpenReq::OM_DIRECT;
  req->fileFlags |= FsOpenReq::OM_THREAD_POOL;
  switch (requestInfo) {
    case CreateFileImplReq::Create:
      DEB_TSMAN_RESTART(("File::Create"));
      req->fileFlags |= FsOpenReq::OM_CREATE_IF_NONE;
      req->fileFlags |= FsOpenReq::OM_INIT;
      break;
    case CreateFileImplReq::CreateForce:
      DEB_TSMAN_RESTART(("File::CreateForce"));
      req->fileFlags |= FsOpenReq::OM_CREATE;
      req->fileFlags |= FsOpenReq::OM_INIT;
      break;
    case CreateFileImplReq::Open:
      DEB_TSMAN_RESTART(("File::Open"));
      req->fileFlags |= FsOpenReq::OM_READ_SIZE;
      break;
    default:
      ndbabort();
  }
  if (c_encrypted_filesystem) {
    jam();
    req->fileFlags |= FsOpenReq::OM_ENCRYPT_XTS;
  }

  req->page_size = File_formats::NDB_PAGE_SIZE;
  req->file_size_hi = hi;
  req->file_size_lo = lo;
  req->auto_sync_size = 0;

  Uint64 pages =
      (Uint64(hi) << 32 | Uint64(lo)) / Uint64(File_formats::NDB_PAGE_SIZE);
  Uint32 extent_size = ts_ptr.p->m_extent_size;  // Extent size in #pages
  Uint64 extents =
      (pages + Uint64(extent_size) - Uint64(1)) / Uint64(extent_size);
  extents = extents ? extents : Uint64(1);
  Uint64 data_pages = extents * Uint64(extent_size);

  /**
   * We always calculate the file size by using the v1 format to ensure
   * that we can always open the file with size check.
   */
  Uint64 extent_pages =
      calculate_extent_pages_in_file(extents, extent_size, data_pages, v2);
  Uint64 tot_pages = Uint64(1) + extent_pages + data_pages;

  // TODO check overflow in cast
  ptr.p->m_create.m_extent_pages = Uint32(extent_pages);
  ptr.p->m_create.m_data_pages = Uint32(data_pages);

  /**
   * Check whether there are enough free slots in the disk page buffer
   * for extent pages, which will be locked in the buffer.
   */
  {
    Page_cache_client pgman(this, m_pgman);
    if (!pgman.extent_pages_available(extent_pages)) {
      return CreateFileImplRef::OutOfDiskPageBufferMemory;

      // CreateFileImplReq::Abort from DBDICT will free the
      // PGMAN datafile already created
    }
  }

  /**
   * Update file size
   */
  Uint64 bytes = tot_pages * Uint64(File_formats::NDB_PAGE_SIZE);
  hi = (Uint32)(bytes >> 32);
  lo = (Uint32)(bytes & 0xFFFFFFFF);
  req->file_size_hi = hi;
  req->file_size_lo = lo;
#if defined VM_TRACE || defined ERROR_INSERT
  g_eventLogger->info(
      "DD tsman: file id: %u datafile pages/bytes: %llu/%llu"
      " extent pages: %llu",
      ptr.p->m_file_id, data_pages, data_pages * File_formats::NDB_PAGE_SIZE,
      extent_pages);
#endif

  if ((req->fileFlags & FsOpenReq::OM_ENCRYPT_CIPHER_MASK) != 0) {
    ndbrequire(handle->m_cnt == 1);

    EncryptionKeyMaterial nmk;
    nmk.length = globalData.nodeMasterKeyLength;
    memcpy(&nmk.data, globalData.nodeMasterKey, globalData.nodeMasterKeyLength);

    ndbrequire(import(handle->m_ptr[FsOpenReq::ENCRYPT_KEY_MATERIAL],
                      (const Uint32 *)&nmk, nmk.get_needed_words()));
    handle->m_cnt++;
    req->fileFlags |= FsOpenReq::OM_ENCRYPT_KEY;
  }
  sendSignal(NDBFS_REF, GSN_FSOPENREQ, signal, FsOpenReq::SignalLength, JBB,
             handle);

  return 0;
}

void Tsman::execFSWRITEREQ(const FsReadWriteReq *req)
    const /* called direct cross threads from Ndbfs */
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
   *
   * We will always initialise new data files to the new format even if
   * other data files of the same tablespace use the old format. The
   * format of the data files is a file property and not a tablespace
   * property.
   *
   * For testing purposes we make it possible to still test with the old
   * format using either an error insert or a by changing false to true
   * below before compiling.
   */
  // jamEntry();
  Ptr<Datafile> ptr;
  Ptr<GlobalPage> page_ptr;

  ndbrequire(m_file_pool.getPtr(ptr, req->userPointer));
  ndbrequire(req->getFormatFlag(req->operationFlag) == req->fsFormatSharedPage);
  ndbrequire(
      m_shared_page_pool.getPtr(page_ptr, req->data.sharedPage.pageNumber));

  Uint32 page_no = req->varIndex;
  Uint32 size = ptr.p->m_extent_size;
  Uint32 extent_pages = ptr.p->m_create.m_extent_pages;
  Uint32 datapages = ptr.p->m_create.m_data_pages;

  bool v2 = (ptr.p->m_ndb_version >= NDB_DISK_V2);
  Uint32 header_words = File_formats::Datafile::extent_header_words(size, v2);
  Uint32 per_page =
      File_formats::Datafile::extent_page_words(v2) / header_words;
  Uint32 extents = datapages / size;

  client_lock(0);
  if (page_no == 0) {
    // jam();
    memset(page_ptr.p, 0, File_formats::NDB_PAGE_SIZE);
    Ptr<Tablespace> ts_ptr;
    m_tablespace_hash.getPtr(ts_ptr, ptr.p->m_tablespace_ptr_i);

    File_formats::Datafile::Zero_page *page =
        (File_formats::Datafile::Zero_page *)page_ptr.p;
    page->m_page_header.init(File_formats::FT_Datafile, getOwnNodeId(),
                             v2 ? NDB_DISK_V2 : 0, (Uint32)time(0));
    page->m_file_no = ptr.p->m_file_no;
    page->m_file_id = ptr.p->m_file_id;
    page->m_tablespace_id = ts_ptr.p->m_tablespace_id;
    page->m_tablespace_version = ts_ptr.p->m_version;
    page->m_data_pages = extents * size;
    page->m_extent_pages = extent_pages;
    page->m_extent_size = size;
    page->m_extent_count = extents;
    page->m_extent_headers_per_page = per_page;
    page->m_extent_header_words = header_words;
    page->m_extent_header_bits_per_page =
        File_formats::Datafile::EXTENT_HEADER_BITMASK_BITS_PER_PAGE;
    if (v2) {
      File_formats::Datafile::Zero_page_v2 *page_v2 =
          (File_formats::Datafile::Zero_page_v2 *)page;
      page_v2->m_checksum = 0;
    }
  } else if ((page_no - 1) < extent_pages) {
    // jam();
    memset(page_ptr.p, 0, File_formats::NDB_PAGE_SIZE);

    Uint32 curr_extent = page_no * per_page;

    File_formats::Datafile::Extent_page *page =
        (File_formats::Datafile::Extent_page *)page_ptr.p;
    page->m_page_header.m_page_lsn_hi = 0;
    page->m_page_header.m_page_lsn_lo = 0;
    page->m_page_header.m_page_type = File_formats::PT_Extent_page;
    if (v2) {
      File_formats::Datafile::Extent_page_v2 *page_v2 =
          (File_formats::Datafile::Extent_page_v2 *)page;
      page_v2->m_checksum = 0;
      page_v2->m_ndb_version = NDB_DISK_V2;
    }

    for (Uint32 i = 0; i < per_page; i++) {
      if (v2) {
        File_formats::Datafile::Extent_page_v2 *page_v2 =
            (File_formats::Datafile::Extent_page_v2 *)page;
        File_formats::Datafile::Extent_header_v2 *head =
            page_v2->get_header_v2(i, size);
        memset(head, 0, 4 * header_words);
        head->m_table = RNIL;
        head->m_next_free_extent = ++curr_extent;
      } else {
        File_formats::Datafile::Extent_header *head =
            page->get_header(i, size, v2);
        memset(head, 0, 4 * header_words);
        head->m_table = RNIL;
        head->m_next_free_extent = ++curr_extent;
      }
    }
    if (page_no == extent_pages) {
      Uint32 last = extents - ((extent_pages - 1) * per_page);
      if (v2) {
        File_formats::Datafile::Extent_page_v2 *page_v2 =
            (File_formats::Datafile::Extent_page_v2 *)page;
        page_v2->get_header_v2(last - 1, size)->m_next_free_extent = RNIL;
      } else {
        page->get_header(last - 1, size, v2)->m_next_free_extent = RNIL;
      }
    }
  } else {
    // jam();
    /* Should be sufficient to clear header. */
    memset(page_ptr.p, 0, File_formats::NDB_PAGE_SIZE);
    File_formats::Page_header *page_header =
        (File_formats::Page_header *)page_ptr.p;
    page_header->m_page_type = File_formats::PT_Unallocated;
  }
  client_unlock(0);
}

void Tsman::create_file_ref(Signal *signal, Ptr<Tablespace> ts_ptr,
                            Ptr<Datafile> ptr, Uint32 error, Uint32 fsError,
                            Uint32 osError) {
  CreateFileImplRef *ref = (CreateFileImplRef *)signal->getDataPtr();
  ref->senderData = ptr.p->m_create.m_senderData;
  ref->senderRef = reference();
  ref->errorCode = (CreateFileImplRef::ErrorCode)error;
  ref->fsErrCode = fsError;
  ref->osErrCode = osError;
  sendSignal(ptr.p->m_create.m_senderRef, GSN_CREATE_FILE_IMPL_REF, signal,
             CreateFileImplRef::SignalLength, JBB);

  Local_datafile_list meta(m_file_pool, ts_ptr.p->m_meta_files);
  meta.release(ptr);
}

void Tsman::execFSOPENREF(Signal *signal) {
  jamEntry();

  Ptr<Datafile> ptr;
  Ptr<Tablespace> ts_ptr;
  FsRef *ref = (FsRef *)signal->getDataPtr();

  Uint32 errCode = ref->errorCode;
  Uint32 osErrCode = ref->osErrorCode;

  ndbrequire(m_file_pool.getPtr(ptr, ref->userPointer));
  m_tablespace_hash.getPtr(ts_ptr, ptr.p->m_tablespace_ptr_i);

  create_file_ref(signal, ts_ptr, ptr, CreateFileImplRef::FileError, errCode,
                  osErrCode);
}

void Tsman::execFSOPENCONF(Signal *signal) {
  jamEntry();
  Ptr<Datafile> ptr;
  Ptr<Tablespace> ts_ptr;
  FsConf *conf = (FsConf *)signal->getDataPtr();

  ndbrequire(m_file_pool.getPtr(ptr, conf->userPointer));
  m_tablespace_hash.getPtr(ts_ptr, ptr.p->m_tablespace_ptr_i);

  Uint32 fd = ptr.p->m_fd = conf->filePointer;

  switch (ptr.p->m_create.m_requestInfo) {
    case CreateFileImplReq::Create:
    case CreateFileImplReq::CreateForce: {
      jam();
      lock_alloc_extent();
      const Uint32 extents =
          ptr.p->m_create.m_data_pages / ts_ptr.p->m_extent_size;
      ts_ptr.p->m_total_extents += Uint64(extents);  // At initial start
      unlock_alloc_extent();
      DEB_TSMAN_NUM_EXTENTS(
          ("Total num_extents: %llu", ts_ptr.p->m_total_extents));

      CreateFileImplConf *conf = (CreateFileImplConf *)signal->getDataPtr();
      conf->senderData = ptr.p->m_create.m_senderData;
      conf->senderRef = reference();
      sendSignal(ptr.p->m_create.m_senderRef, GSN_CREATE_FILE_IMPL_CONF, signal,
                 CreateFileImplConf::SignalLength, JBB);
      return;
    }
    case CreateFileImplReq::Open: {
      jam();
      /**
       * Read zero page and compare values
       *   can't use page cache as file's file_no is not known
       *
       * We need to verify length of file here. We need to allocate
       * a page to read page zero. Any failure requires us to also
       * close the file before returning.
       */
      Uint64 file_size_hi = conf->file_size_hi;
      Uint64 file_size_lo = conf->file_size_lo;
      Uint64 file_size = (file_size_hi << 32) + file_size_lo;
      Uint64 calc_file_size =
          (Uint64(1) + Uint64(ptr.p->m_create.m_data_pages) +
           Uint64(ptr.p->m_create.m_extent_pages)) *
          Uint64(File_formats::NDB_PAGE_SIZE);
      if (file_size != calc_file_size) {
        /**
         * Using v2 format to calculate the file size didn't work, try with
         * v1 format instead and see if this is successful.
         */
        Uint64 extent_size = ts_ptr.p->m_extent_size;
        Uint64 data_pages = ptr.p->m_create.m_data_pages;
        Uint64 num_extents = data_pages / extent_size;
        Uint64 extent_pages = calculate_extent_pages_in_file(
            num_extents, Uint32(extent_size), data_pages, false);
        Uint64 calc_file_size_v1 = (Uint64(1) + data_pages + extent_pages) *
                                   Uint64(File_formats::NDB_PAGE_SIZE);
        if (file_size == calc_file_size_v1) {
          jam();
          ptr.p->m_ndb_version = 0;
          ptr.p->m_create.m_extent_pages = Uint32(extent_pages);
        } else if (file_size > calc_file_size_v1) {
          jam();
          g_eventLogger->info(
              "file_size = %llu, calc_file_size: %llu, calc_file_size_v1: %llu"
              "num data_pages: %llu, num extent_pages: %llu"
              "extent_size: %llu, num_extents: %llu",
              file_size, calc_file_size, calc_file_size_v1, data_pages,
              extent_pages, extent_size, num_extents);
          ptr.p->m_create.m_error_code = CreateFileImplRef::FileSizeTooLarge;
        } else {
          jam();
          ptr.p->m_create.m_error_code = CreateFileImplRef::FileSizeTooSmall;
        }
      }

      Ptr<GlobalPage> page_ptr;
      if (m_global_page_pool.seize(page_ptr) == false) {
        jam();
        ptr.p->m_create.m_error_code = CreateFileImplRef::OutOfMemory;
      }
      if (ptr.p->m_create.m_error_code != 0) {
        jam();
        ptr.p->m_state = Datafile::FS_ERROR_CLOSE;
        FsCloseReq *req = (FsCloseReq *)signal->getDataPtrSend();
        req->filePointer = ptr.p->m_fd;
        req->userReference = reference();
        req->userPointer = ptr.i;
        sendSignal(NDBFS_REF, GSN_FSCLOSEREQ, signal, FsCloseReq::SignalLength,
                   JBB);
        return;
      }

      ptr.p->m_create.m_page_ptr_i = page_ptr.i;

      FsReadWriteReq *req = (FsReadWriteReq *)signal->getDataPtrSend();
      req->filePointer = fd;
      req->userReference = reference();
      req->userPointer = ptr.i;
      req->varIndex = 0;
      req->numberOfPages = 1;
      req->operationFlag = 0;
      req->setFormatFlag(req->operationFlag,
                         FsReadWriteReq::fsFormatGlobalPage);
      req->data.globalPage.pageNumber = page_ptr.i;
      sendSignal(NDBFS_REF, GSN_FSREADREQ, signal,
                 FsReadWriteReq::FixedLength + 1, JBB);
      return;
    }
  }
}

void Tsman::execFSREADCONF(Signal *signal) {
  jamEntry();
  Ptr<Datafile> ptr;
  Ptr<Tablespace> ts_ptr;
  FsConf *conf = (FsConf *)signal->getDataPtr();

  /**
   * We currently only read pages here as part of CREATE_FILE
   *  (other read is done using pgman)
   */
  ndbrequire(m_file_pool.getPtr(ptr, conf->userPointer));
  m_tablespace_hash.getPtr(ts_ptr, ptr.p->m_tablespace_ptr_i);

  Ptr<GlobalPage> page_ptr;
  ndbrequire(m_global_page_pool.getPtr(page_ptr, ptr.p->m_create.m_page_ptr_i));

  File_formats::Datafile::Zero_page *page =
      (File_formats::Datafile::Zero_page *)page_ptr.p;

  Uint32 fsError = 0;
  bool assumed_v2 = (ptr.p->m_ndb_version >= NDB_DISK_V2);
  ptr.p->m_ndb_version = page->m_page_header.m_ndb_version;
  bool v2 = (page->m_page_header.m_ndb_version >= NDB_DISK_V2);

  if (assumed_v2 != v2) {
    /**
     * We assumed that the file used v2 format when we opened it.
     * It turned out that our assumption was wrong. This means that
     * our calculation of extent pages is wrong. We need to correct
     * the setting of extent_pages.
     */
    jam();
    assert(v2 && !assumed_v2);
    ptr.p->m_create.m_extent_pages = page->m_extent_pages;
  }
  if (v2) {
    File_formats::Datafile::Zero_page_v2 *page_v2 =
        (File_formats::Datafile::Zero_page_v2 *)page_ptr.p;
    fsError = page_v2->m_page_header.validate(File_formats::FT_Datafile,
                                              getOwnNodeId(), NDB_DISK_V2,
                                              (Uint32)time(0));
  } else {
    fsError = page->m_page_header.validate(File_formats::FT_Datafile,
                                           getOwnNodeId(), 0, (Uint32)time(0));
  }
  CreateFileImplRef::ErrorCode err = CreateFileImplRef::NoError;
  Uint32 osError = 0;

  do {
    err = CreateFileImplRef::InvalidFileMetadata;
    if (fsError) break;

    osError = 1;
    if (page->m_file_id != ptr.p->m_file_id) break;

    osError = 2;
    if (page->m_tablespace_id != ts_ptr.p->m_tablespace_id) break;

    osError = 3;
    if (page->m_tablespace_version != ts_ptr.p->m_version) break;

    osError = 4;
    if (page->m_data_pages != ptr.p->m_create.m_data_pages) break;

    osError = 5;
    if (page->m_extent_pages != ptr.p->m_create.m_extent_pages) break;

    osError = 6;
    if (page->m_extent_size != ptr.p->m_extent_size) break;

    osError = 7;
    if (page->m_extent_header_bits_per_page !=
        File_formats::Datafile::EXTENT_HEADER_BITMASK_BITS_PER_PAGE)
      break;

    osError = 8;
    Uint32 eh_words =
        File_formats::Datafile::extent_header_words(ptr.p->m_extent_size, v2);
    if (page->m_extent_header_words != eh_words) break;

    osError = 9;
    Uint32 per_page = File_formats::Datafile::extent_page_words(v2) / eh_words;
    if (page->m_extent_headers_per_page != per_page) break;

    osError = 10;
    Uint32 extents = page->m_data_pages / ptr.p->m_extent_size;
    if (page->m_extent_count != extents) break;

    osError = 11;
    ptr.p->m_file_no = page->m_file_no;
    Page_cache_client pgman(this, m_pgman);
    if (pgman.alloc_data_file(signal, ptr.p->m_file_no, v2 ? NDB_DISK_V2 : 0) ==
        RNIL) {
      jam();
      break;
    }

    /**
     *
     */
    m_global_page_pool.release(page_ptr);

    lock_alloc_extent();
    ts_ptr.p->m_total_extents += Uint64(extents);  // At node restart
    DEB_TSMAN_NUM_EXTENTS(
        ("Total num_extents: %llu", ts_ptr.p->m_total_extents));
    unlock_alloc_extent();

    CreateFileImplConf *conf = (CreateFileImplConf *)signal->getDataPtr();
    conf->senderData = ptr.p->m_create.m_senderData;
    conf->senderRef = reference();
    sendSignal(ptr.p->m_create.m_senderRef, GSN_CREATE_FILE_IMPL_CONF, signal,
               CreateFileImplConf::SignalLength, JBB);
    return;
  } while (0);

  m_global_page_pool.release(page_ptr);
  create_file_ref(signal, ts_ptr, ptr, err, fsError, osError);
}

void Tsman::execFSREADREF(Signal *signal) {
  jamEntry();
  Ptr<Datafile> ptr;
  Ptr<Tablespace> ts_ptr;
  FsRef *ref = (FsRef *)signal->getDataPtr();

  ndbrequire(m_file_pool.getPtr(ptr, ref->userPointer));
  m_tablespace_hash.find(ts_ptr, ptr.p->m_tablespace_ptr_i);

  m_global_page_pool.release(ptr.p->m_create.m_page_ptr_i);
  create_file_ref(signal, ts_ptr, ptr, CreateFileImplRef::FileReadError,
                  ref->errorCode, ref->osErrorCode);
}

void Tsman::load_extent_pages(Signal *signal, Ptr<Datafile> ptr) {
  /**
   * Currently all extent header pages needs to be locked in memory
   */
  Page_cache_client::Request preq;
  preq.m_page.m_file_no = ptr.p->m_file_no;
  preq.m_page.m_page_no = ptr.p->m_create.m_loading_extent_page;
  preq.m_table_id = RNIL;
  preq.m_fragment_id = 0;

  preq.m_callback.m_callbackData = ptr.i;
  preq.m_callback.m_callbackFunction =
      safe_cast(&Tsman::load_extent_page_callback);

  int page_id;
  int flags = Page_cache_client::LOCK_PAGE;
  Page_cache_client pgman(this, m_pgman);
  if ((page_id = pgman.get_page(signal, preq, flags)) > 0) {
    load_extent_page_callback_direct(signal, ptr.i, (Uint32)page_id);
  }

  if (page_id < 0) {
    ndbabort();
  }
}

void Tsman::load_extent_page_callback(Signal *signal, Uint32 callback,
                                      Uint32 real_page_ptr_i) {
  jamEntry();
  client_lock();
  load_extent_page_callback_direct(signal, callback, real_page_ptr_i);
  client_unlock();
}

void Tsman::load_extent_page_callback_direct(Signal *signal, Uint32 callback,
                                             Uint32 real_page_ptr_i) {
  jamEntry();
  Ptr<Datafile> ptr;
  Ptr<GlobalPage> page_ptr;
  ndbrequire(m_file_pool.getPtr(ptr, callback));

  /**
   * Ensure that all extent pages are marked as extent pages before being
   * written by PGMAN (PGMAN needs this information to know how to interpret
   * the page layout and where to find version number and other entries in
   * the page.
   *
   * LSN number for extent pages isn't used.
   */
  ndbrequire(m_global_page_pool.getPtr(page_ptr, real_page_ptr_i));
  File_formats::Page_header *extent_page_header =
      (File_formats::Page_header *)page_ptr.p;
  extent_page_header->m_page_lsn_hi = 0;
  extent_page_header->m_page_lsn_lo = 0;
  extent_page_header->m_page_type = File_formats::PT_Extent_page;

  if (++ptr.p->m_create.m_loading_extent_page <=
      ptr.p->m_create.m_extent_pages) {
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

  bool v2 = (ptr.p->m_ndb_version >= NDB_DISK_V2);
  Uint32 eh =
      File_formats::Datafile::extent_header_words(ptr.p->m_extent_size, v2);
  Uint32 per_page = File_formats::Datafile::extent_page_words(v2) / eh;

  ptr.p->m_state = Datafile::FS_ONLINE;
  ptr.p->m_online.m_offset_data_pages = 1 + extent_pages;
  ptr.p->m_online.m_first_free_extent = per_page;
  ptr.p->m_online.m_lcp_free_extent_head = RNIL;
  ptr.p->m_online.m_lcp_free_extent_tail = RNIL;
  ptr.p->m_online.m_lcp_free_extent_count = 0;
  ptr.p->m_online.m_data_pages = data_pages;
  ptr.p->m_online.m_used_extent_cnt = 0;
  ptr.p->m_online.m_extent_headers_per_extent_page = per_page;

  for (Uint32 i = 0; i < NUM_EXTENT_PAGE_MUTEXES; i++) {
    int ret = NdbMutex_Init(&ptr.p->m_extent_page_mutex[i]);
    ndbrequire(ret == 0);
  }
  Ptr<Tablespace> ts_ptr;
  ndbrequire(m_tablespace_pool.getPtr(ts_ptr, ptr.p->m_tablespace_ptr_i));
  if (getNodeState().startLevel >= NodeState::SL_STARTED ||
      (getNodeState().startLevel == NodeState::SL_STARTING &&
       getNodeState().starting.restartType == NodeState::ST_INITIAL_START) ||
      (getNodeState().getNodeRestartInProgress() &&
       getNodeState().starting.restartType ==
           NodeState::ST_INITIAL_NODE_RESTART)) {
    jam();
    Local_datafile_list free_list(m_file_pool, ts_ptr.p->m_free_files);
    Local_datafile_list meta(m_file_pool, ts_ptr.p->m_meta_files);
    meta.remove(ptr);
    free_list.addFirst(ptr);
  }

  CreateFileImplConf *conf = (CreateFileImplConf *)signal->getDataPtr();
  conf->senderData = senderData;
  conf->senderRef = reference();
  sendSignal(senderRef, GSN_CREATE_FILE_IMPL_CONF, signal,
             CreateFileImplConf::SignalLength, JBB);
}

void Tsman::execSTART_RECREQ(Signal *signal) {
  jamEntry();
  Ptr<Tablespace> ts_ptr;
  m_tablespace_list.first(ts_ptr);

  signal->theData[0] = TsmanContinueB::SCAN_TABLESPACE_EXTENT_HEADERS;
  signal->theData[1] = ts_ptr.i;
  sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
}

void Tsman::scan_tablespace(Signal *signal, Uint32 ptrI) {
  Ptr<Tablespace> ts_ptr;
  if (ptrI == RNIL) {
    jam();
    signal->theData[0] = reference();
    sendSignal(DBLQH_REF, GSN_START_RECCONF, signal, 1, JBB);
    return;
  }

  ndbrequire(m_tablespace_pool.getPtr(ts_ptr, ptrI));

  Ptr<Datafile> file_ptr;
  {
    Local_datafile_list meta(m_file_pool, ts_ptr.p->m_meta_files);
    meta.first(file_ptr);
  }

  scan_datafile(signal, ts_ptr.i, file_ptr.i);
}

void Tsman::scan_datafile(Signal *signal, Uint32 ptrI, Uint32 filePtrI) {
  Ptr<Datafile> file_ptr;
  Ptr<Tablespace> ts_ptr;
  ndbrequire(m_tablespace_pool.getPtr(ts_ptr, ptrI));
  if (filePtrI == RNIL) {
    jam();
    m_tablespace_list.next(ts_ptr);
    signal->theData[0] = TsmanContinueB::SCAN_TABLESPACE_EXTENT_HEADERS;
    signal->theData[1] = ts_ptr.i;
    sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
  } else {
    jam();
    ndbrequire(m_file_pool.getPtr(file_ptr, filePtrI));
    scan_extent_headers(signal, file_ptr);
  }
}

/**
 * This function is used during restarts to ensure that DBTUP and TSMAN
 * gets its in-memory representations of free pages in extents up to date.
 */
void Tsman::scan_extent_headers(Signal *signal, Ptr<Datafile> ptr) {
  Ptr<Tablespace> ts_ptr;
  ndbrequire(m_tablespace_pool.getPtr(ts_ptr, ptr.p->m_tablespace_ptr_i));

  Uint32 firstFree = RNIL;
  Uint32 size = ptr.p->m_extent_size;
  Uint32 per_page = ptr.p->m_online.m_extent_headers_per_extent_page;
  Uint32 pages = ptr.p->m_online.m_offset_data_pages - 1;
  Uint32 datapages = ptr.p->m_online.m_data_pages;
  for (Uint32 i = 0; i < pages; i++) {
    jam();
    Uint32 page_no = pages - i;
    Page_cache_client::Request preq;
    preq.m_page.m_page_no = page_no;
    preq.m_page.m_file_no = ptr.p->m_file_no;
    preq.m_table_id = RNIL;
    preq.m_fragment_id = 0;

    int flags = Page_cache_client::DIRTY_REQ;
    Page_cache_client pgman(this, m_pgman);
    pgman.get_extent_page(signal, preq, flags);

    bool v2 = (ptr.p->m_ndb_version >= NDB_DISK_V2);
    File_formats::Datafile::Extent_page *page =
        (File_formats::Datafile::Extent_page *)pgman.m_ptr.p;

    Uint32 extents = per_page;
    if (page_no == pages) {
      jam();
      /**
       * Last extent header page...
       * This extent page might not be used fully, so
       * set correct no of extent headers on this page.
       */
      Uint32 total_extents = datapages / size;
      extents = total_extents - (pages - 1) * per_page;
    }
    for (Uint32 j = 0; j < extents; j++) {
      jam();
      Uint32 extent_no = extents - j - 1;

      File_formats::Datafile::Extent_data *ext_data =
          page->get_extent_data(extent_no, size, v2);
      Uint32 *ext_table_id = page->get_table_id(extent_no, size, v2);
      Uint32 *ext_next_free_extent =
          page->get_next_free_extent(extent_no, size, v2);
      Uint32 *ext_fragment_id = page->get_fragment_id(extent_no, size, v2);
      Uint32 *ext_create_table_version =
          page->get_create_table_version(extent_no, size, v2);

      if ((*ext_table_id) == RNIL) {
        jam();
        /* This extent was free still, so no need to do anything. */
        D("extent free" << V(j));
        DEB_TSMAN_RESTART(("extent(%u,%u) free", ptr.p->m_file_no,
                           ((page_no * per_page) + extent_no)));
        (*ext_table_id) = RNIL;
        (*ext_next_free_extent) = firstFree;
        firstFree = page_no * per_page + extent_no;
      } else {
        jam();
        /**
         * This extent was used, we do however need to step with care here
         * since it is perfectly possible that the table id and fragment id
         * might be an old dropped page. If the table doesn't exist then
         * this is clear, it could also have the wrong create table version.
         * We can only detect if it is the wrong create table version if we
         * are using the new version of the extent page layout (v2).
         *
         * With v1 we might in this manner get extents that have unused pages
         * attached to them. These pages will be allocated to the table, so it
         * is a form of memory leak, but the pages in this extent was not
         * used at all, so the content on them is garbage. The extent bits
         * indicate that a page is full, in this case the page is lost until
         * the table is dropped, so a form of memory leak. The extent bits
         * might indicate that the page is empty in which case the page might
         * get used if the table grows, but could also remain unused. Finally
         * if the extent bits indicate that the page is not full and also not
         * empty then we can have a problem if the page is claiming it is full.
         * In this case we assume the page comes from this problematic case
         * and initial the page as it was an empty page.
         */
        Dbtup_client tup(this, m_tup);
        Local_key key;
        key.m_file_no = ptr.p->m_file_no;
        key.m_page_no =
            pages + 1 + size * (page_no * per_page + extent_no - per_page);
        key.m_page_idx = page_no * per_page + extent_no;
        int res = tup.disk_restart_alloc_extent(
            (*ext_table_id), (*ext_fragment_id),
            v2 ? (*ext_create_table_version) : 0, &key, size);
        if (res == 0) {
          jamEntry();
          ptr.p->m_online.m_used_extent_cnt++;
          ts_ptr.p->m_total_used_extents++;
          DEB_TSMAN_NUM_EXTENTS(
              ("Allocated extent during restart"
               " tab(%u,%u), num_extents: %llu",
               (*ext_table_id), (*ext_fragment_id),
               ts_ptr.p->m_total_used_extents));
          for (Uint32 i = 0; i < size; i++, key.m_page_no++) {
            jam();
            Uint32 bits = ext_data->get_free_bits(i) & COMMITTED_MASK;
            /**
             * No need to make page dirty since only UNCOMMITTED bits
             * are changed
             */
            ext_data->update_free_bits(i, bits | (bits << UNCOMMITTED_SHIFT));
            tup.disk_restart_page_bits((*ext_table_id), (*ext_fragment_id),
                                       v2 ? (*ext_create_table_version) : 0,
                                       &key, bits);
          }
          D("extent used" << V(j) << V((*ext_table_id)) << V((*ext_fragment_id))
                          << V(key));
        } else {
          /**
           * Either table has been deleted, it could be a table with a table
           * id that have been reused. The table id, fragment id and
           * create table version all three have to be found for things to
           * be correct and we can use the extent for this fragment.
           */
          jam();
          DEB_TSMAN_RESTART(("tab(%u,%u):%u not used, deleted", (*ext_table_id),
                             (*ext_fragment_id), (*ext_create_table_version)));
          (*ext_table_id) = RNIL;
          (*ext_next_free_extent) = firstFree;
          firstFree = page_no * per_page + extent_no;
          D("extent free" << V(j) << V((*ext_table_id)) << V((*ext_fragment_id))
                          << V(key));
        }
      }
    }
  }
  ptr.p->m_online.m_first_free_extent = firstFree;
  Local_datafile_list meta(m_file_pool, ts_ptr.p->m_meta_files);
  Ptr<Datafile> next = ptr;
  meta.next(next);
  if (firstFree != RNIL) {
    jam();
    Local_datafile_list free_list(m_file_pool, ts_ptr.p->m_free_files);
    meta.remove(ptr);
    free_list.addFirst(ptr);
  } else {
    jam();
    Local_datafile_list full(m_file_pool, ts_ptr.p->m_full_files);
    meta.remove(ptr);
    full.addFirst(ptr);
  }

  signal->theData[0] = TsmanContinueB::SCAN_DATAFILE_EXTENT_HEADERS;
  signal->theData[1] = ts_ptr.i;
  signal->theData[2] = next.i;
  sendSignal(reference(), GSN_CONTINUEB, signal, 3, JBB);
}

void Tsman::execDROP_FILE_IMPL_REQ(Signal *signal) {
  jamEntry();
  client_lock();
  DropFileImplReq req = *(DropFileImplReq *)signal->getDataPtr();
  Ptr<Datafile> file_ptr;
  Ptr<Tablespace> fg_ptr;

  Uint32 errorCode = 0;
  do {
    if (!m_tablespace_hash.find(fg_ptr, req.filegroup_id)) {
      jam();
      errorCode = DropFileImplRef::InvalidFilegroup;
      break;
    }

    if (fg_ptr.p->m_version != req.filegroup_version) {
      jam();
      errorCode = DropFileImplRef::InvalidFilegroupVersion;
      break;
    }

    switch (req.requestInfo) {
      case DropFileImplReq::Prepare: {
        if (find_file_by_id(file_ptr, fg_ptr.p->m_full_files, req.file_id)) {
          jam();
          Local_datafile_list full(m_file_pool, fg_ptr.p->m_full_files);
          full.remove(file_ptr);
        } else if (find_file_by_id(file_ptr, fg_ptr.p->m_free_files,
                                   req.file_id)) {
          jam();
          Local_datafile_list free_list(m_file_pool, fg_ptr.p->m_free_files);
          free_list.remove(file_ptr);
        } else if (find_file_by_id(file_ptr, fg_ptr.p->m_meta_files,
                                   req.file_id)) {
          jam();
          Local_datafile_list meta(m_file_pool, fg_ptr.p->m_meta_files);
          meta.remove(file_ptr);
        } else {
          jam();
          errorCode = DropFileImplRef::NoSuchFile;
          break;
        }

        Local_datafile_list meta(m_file_pool, fg_ptr.p->m_meta_files);
        meta.addFirst(file_ptr);

        if (file_ptr.p->m_online.m_used_extent_cnt ||
            file_ptr.p->m_state != Datafile::FS_ONLINE) {
          jam();
          errorCode = DropFileImplRef::FileInUse;
          break;
        }

        file_ptr.p->m_state = Datafile::FS_DROPPING;
        break;
      }
      case DropFileImplReq::Commit:
        ndbrequire(
            find_file_by_id(file_ptr, fg_ptr.p->m_meta_files, req.file_id));
        jam();
        if (file_ptr.p->m_ref_count) {
          jam();
          client_unlock();
          sendSignalWithDelay(reference(), GSN_DROP_FILE_REQ, signal, 100,
                              signal->getLength());
          return;
        }

        file_ptr.p->m_create.m_extent_pages =
            file_ptr.p->m_online.m_offset_data_pages - 1;
        file_ptr.p->m_create.m_senderRef = req.senderRef;
        file_ptr.p->m_create.m_senderData = req.senderData;
        release_extent_pages(signal, file_ptr);
        client_unlock();
        return;
      case DropFileImplReq::Abort: {
        ndbrequire(
            find_file_by_id(file_ptr, fg_ptr.p->m_meta_files, req.file_id));
        file_ptr.p->m_state = Datafile::FS_ONLINE;
        Local_datafile_list meta(m_file_pool, fg_ptr.p->m_meta_files);
        meta.remove(file_ptr);
        if (file_ptr.p->m_online.m_first_free_extent != RNIL) {
          jam();
          Local_datafile_list free_list(m_file_pool, fg_ptr.p->m_free_files);
          free_list.addLast(file_ptr);
        } else {
          jam();
          Local_datafile_list full(m_file_pool, fg_ptr.p->m_full_files);
          full.addFirst(file_ptr);
        }
        break;
      }
    }
  } while (0);

  if (errorCode) {
    jam();
    DropFileImplRef *ref = (DropFileImplRef *)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->senderData = req.senderData;
    ref->errorCode = errorCode;
    sendSignal(req.senderRef, GSN_DROP_FILE_IMPL_REF, signal,
               DropFileImplRef::SignalLength, JBB);
  } else {
    jam();
    DropFileImplConf *conf = (DropFileImplConf *)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->senderData = req.senderData;
    sendSignal(req.senderRef, GSN_DROP_FILE_IMPL_CONF, signal,
               DropFileImplConf::SignalLength, JBB);
  }
  client_unlock();
}

Tsman::Tablespace::Tablespace(Tsman *ts, const CreateFilegroupImplReq *req) {
  m_tsman = ts;
  m_logfile_group_id = req->tablespace.logfile_group_id;
  m_tablespace_id = req->filegroup_id;
  m_version = req->filegroup_version;
  m_ref_count = 0;
  m_total_extents = Uint64(0);
  m_total_used_extents = Uint64(0);

  m_extent_size =
      (Uint32)DIV(req->tablespace.extent_size, File_formats::NDB_PAGE_SIZE);
#if defined VM_TRACE || defined ERROR_INSERT
  g_eventLogger->info("DD tsman: ts id: %u extent pages/bytes: %u/%u",
                      m_tablespace_id, m_extent_size,
                      m_extent_size * File_formats::NDB_PAGE_SIZE);
#endif
}

Tsman::Datafile::Datafile(const struct CreateFileImplReq *req) {
  m_file_id = req->file_id;

  m_file_no = RNIL;
  m_fd = RNIL;
  m_online.m_first_free_extent = RNIL;
  m_ref_count = 0;
  m_ndb_version = NDB_DISK_V2;

  m_create.m_senderRef = req->senderRef;    // During META
  m_create.m_senderData = req->senderData;  // During META
  m_create.m_requestInfo = req->requestInfo;
  m_create.m_error_code = 0;
}

void Tsman::execALLOC_EXTENT_REQ(Signal *signal) {
  EmulatedJamBuffer *const jamBuf = getThrJamBuf();

  thrjamEntry(jamBuf);
  Ptr<Tablespace> ts_ptr;
  Ptr<Datafile> file_ptr;
  AllocExtentReq req = *(AllocExtentReq *)signal->getDataPtr();
  AllocExtentReq::ErrorCode err;

  lock_alloc_extent();
  ndbrequire(m_tablespace_hash.find(ts_ptr, req.request.tablespace_id));

  Local_datafile_list tmp(m_file_pool, ts_ptr.p->m_free_files);

  const bool starting = (getNodeState().startLevel <= NodeState::SL_STARTING);

  // Reserve 4% of total data extents of a tablespace from normal usage.
  // This will be used during node starts.
  bool extent_available = false;

  if (tmp.first(file_ptr)) {
    if (unlikely(starting)) {
      thrjam(jamBuf);
      extent_available = true;
    } else {
      thrjam(jamBuf);
      extent_available = (Uint64(100) * (ts_ptr.p->m_total_used_extents + 1) <
                          Uint64(96) * ts_ptr.p->m_total_extents);
    }
  }
  if (extent_available) {
    thrjam(jamBuf);
    Uint32 size = file_ptr.p->m_extent_size;
    Uint32 extent = file_ptr.p->m_online.m_first_free_extent;
    Uint32 data_off = file_ptr.p->m_online.m_offset_data_pages;
    Uint32 eh_words;
    Uint32 per_page;
    bool v2 = (file_ptr.p->m_ndb_version >= NDB_DISK_V2);
    eh_words = File_formats::Datafile::extent_header_words(size, v2);
    per_page = File_formats::Datafile::extent_page_words(v2) / eh_words;
    Uint32 page_no = extent / per_page;
    Uint32 extent_no = extent % per_page;
    Page_cache_client::Request preq;
    preq.m_page.m_page_no = page_no;
    preq.m_page.m_file_no = file_ptr.p->m_file_no;
    preq.m_table_id = RNIL;
    preq.m_fragment_id = 0;

    /**
     * Handling of unmapped extent header pages is not implemented
     */
    lock_extent_page(file_ptr.p, page_no);
    int flags = Page_cache_client::DIRTY_REQ;
    Page_cache_client pgman(this, m_pgman);
    pgman.get_extent_page(signal, preq, flags);
    {
      thrjam(jamBuf);
      GlobalPage *ptr_p = pgman.m_ptr.p;

      File_formats::Datafile::Extent_page *page =
          (File_formats::Datafile::Extent_page *)ptr_p;

      File_formats::Datafile::Extent_header *header =
          page->get_header(extent_no, size, v2);

      Uint32 *ext_table_id = page->get_table_id(extent_no, size, v2);
      ndbassert((Uint32 *)header == (Uint32 *)ext_table_id);
      Uint32 *ext_fragment_id = page->get_fragment_id(extent_no, size, v2);
      Uint32 *ext_next_free_extent =
          page->get_next_free_extent(extent_no, size, v2);
      Uint32 *ext_create_table_version =
          page->get_create_table_version(extent_no, size, v2);
      ndbrequire((*ext_table_id) == RNIL);
      Uint32 next_free = *ext_next_free_extent;
      /**
       * Init header
       */
      memset(header, 0, 4 * eh_words);
      (*ext_table_id) = req.request.table_id;
      (*ext_fragment_id) = req.request.fragment_id;
      if (v2) {
        thrjam(jamBuf);
        (*ext_create_table_version) = req.request.create_table_version;
      }

      /**
       * Check if file is full
       */
      file_ptr.p->m_online.m_used_extent_cnt++;
      ts_ptr.p->m_total_used_extents++;
      DEB_TSMAN_NUM_EXTENTS((
          "ALLOC_EXTENT_REQ: tab(%u,%u)"
          " num_extents: %llu",
          (*ext_table_id), (*ext_fragment_id), ts_ptr.p->m_total_used_extents));
      file_ptr.p->m_online.m_first_free_extent = next_free;
      tmp.remove(file_ptr);
      if (next_free == RNIL) {
        thrjam(jamBuf);
        Local_datafile_list full(m_file_pool, ts_ptr.p->m_full_files);
        full.addFirst(file_ptr);
      } else {
        /**
         * Ensure that we round robin allocation of extents on all available
         * data files. This ensures that we get a sort of RAID on the defined
         * data files in the tablespace.
         */
        thrjam(jamBuf);
        tmp.addLast(file_ptr);
      }

      /**
       * Pack return values
       */
      ndbassert(extent >= per_page);
      preq.m_page.m_page_no = data_off + size * (extent - /* zero */ per_page);
      preq.m_page.m_page_idx = extent;  // extent_no

      AllocExtentReq *rep = (AllocExtentReq *)signal->getDataPtr();
      rep->reply.errorCode = 0;
      rep->reply.page_id = preq.m_page;
      rep->reply.page_count = size;
    }
    unlock_extent_page(file_ptr.p, page_no);
  } else {
    thrjam(jamBuf);
    err = AllocExtentReq::NoExtentAvailable;
    Local_datafile_list full_tmp(m_file_pool, ts_ptr.p->m_full_files);
    if (tmp.isEmpty() && full_tmp.isEmpty()) {
      thrjam(jamBuf);
      err = AllocExtentReq::NoDatafile;
    }

    /**
     * Pack return values
     */
    AllocExtentReq *rep = (AllocExtentReq *)signal->getDataPtr();
    rep->reply.errorCode = err;
  }
  unlock_alloc_extent();
  return;
}

void Tsman::execFREE_EXTENT_REQ(Signal *signal) {
  EmulatedJamBuffer *const jamBuf = getThrJamBuf();

  thrjamEntry(jamBuf);
  Ptr<Datafile> file_ptr;
  FreeExtentReq req = *(FreeExtentReq *)signal->getDataPtr();
  FreeExtentReq::ErrorCode err = (FreeExtentReq::ErrorCode)0;

  char logbuf[MAX_LOG_MESSAGE_SIZE];
  printLocal_Key(logbuf, MAX_LOG_MESSAGE_SIZE, req.request.key);
  g_eventLogger->info("Free extent: %s", logbuf);

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
  preq.m_table_id = RNIL;
  preq.m_fragment_id = 0;

  lock_alloc_extent();
  lock_extent_page(file_ptr.p, val.m_extent_page_no);

  /**
   * Handling of unmapped extent header pages is not implemented
   */
  int flags = Page_cache_client::DIRTY_REQ;
  Page_cache_client pgman(this, m_pgman);
  pgman.get_extent_page(signal, preq, flags);
  {
    thrjam(jamBuf);
    GlobalPage *ptr_p = pgman.m_ptr.p;

    bool v2 = (file_ptr.p->m_ndb_version >= NDB_DISK_V2);
    File_formats::Datafile::Extent_page *page =
        (File_formats::Datafile::Extent_page *)ptr_p;
    Uint32 *ext_table_id =
        page->get_table_id(val.m_extent_no, val.m_extent_size, v2);
#ifdef DEBUG_TSMAN_NUM_EXTENTS
    Uint32 *ext_fragment_id =
        page->get_fragment_id(val.m_extent_no, val.m_extent_size, v2);
#endif
    Uint32 *ext_next_free_extent =
        page->get_next_free_extent(val.m_extent_no, val.m_extent_size, v2);

    ndbrequire((*ext_table_id) == req.request.table_id);
    (*ext_table_id) = RNIL;

    file_ptr.p->m_online.m_used_extent_cnt--;
    if (m_lcp_ongoing) {
      thrjam(jamBuf);
      *ext_next_free_extent = file_ptr.p->m_online.m_lcp_free_extent_head;
      if (file_ptr.p->m_online.m_lcp_free_extent_head == RNIL)
        file_ptr.p->m_online.m_lcp_free_extent_tail = extent;
      file_ptr.p->m_online.m_lcp_free_extent_head = extent;
      file_ptr.p->m_online.m_lcp_free_extent_count++;
      DEB_TSMAN_NUM_EXTENTS(("FREE_EXTENT_REQ(waitLCP): tab(%u,%u)",
                             req.request.table_id, (*ext_fragment_id)));
    } else {
      thrjam(jamBuf);
      *ext_next_free_extent = file_ptr.p->m_online.m_first_free_extent;
      if (file_ptr.p->m_online.m_first_free_extent == RNIL) {
        thrjam(jamBuf);
        /**
         * Move from full to free
         */
        Ptr<Tablespace> ptr;
        ndbrequire(
            m_tablespace_pool.getPtr(ptr, file_ptr.p->m_tablespace_ptr_i));
        Local_datafile_list free_list(m_file_pool, ptr.p->m_free_files);
        Local_datafile_list full(m_file_pool, ptr.p->m_full_files);
        full.remove(file_ptr);
        free_list.addLast(file_ptr);
      }
      file_ptr.p->m_online.m_first_free_extent = extent;

      Ptr<Tablespace> ts_ptr;
      ndbrequire(
          m_tablespace_pool.getPtr(ts_ptr, file_ptr.p->m_tablespace_ptr_i));
      ts_ptr.p->m_total_used_extents--;
      DEB_TSMAN_NUM_EXTENTS(
          ("FREE_EXTENT_REQ: tab(%u,%u)"
           " num_extents: %llu",
           req.request.table_id, (*ext_fragment_id),
           ts_ptr.p->m_total_used_extents));
    }
  }

  /**
   * Pack return values
   */
  FreeExtentReq *rep = (FreeExtentReq *)signal->getDataPtr();
  rep->reply.errorCode = err;
  unlock_extent_page(file_ptr.p, val.m_extent_page_no);
  unlock_alloc_extent();
  return;
}

void Tsman::get_set_extent_info(Signal *signal, Local_key &key, Uint32 &tableId,
                                Uint32 &fragId, Uint32 &create_table_version,
                                bool read) {
  EmulatedJamBuffer *const jamBuf = getThrJamBuf();
  thrjamEntry(jamBuf);
  Ptr<Datafile> file_ptr;
  Datafile file_key;
  file_key.m_file_no = key.m_file_no;
  ndbrequire(m_file_hash.find(file_ptr, file_key));

  // Get extent page info
  struct req val = lookup_extent(key.m_page_no, file_ptr.p);

  Page_cache_client::Request preq;
  preq.m_page.m_page_no = val.m_extent_page_no;
  preq.m_page.m_file_no = key.m_file_no;
  preq.m_table_id = RNIL;
  preq.m_fragment_id = 0;

  lock_extent_page(file_ptr.p, val.m_extent_page_no);
  int flags = 0;
  Page_cache_client pgman(this, m_pgman);

  /**
   * Extent pages are locked into the page cache.
   * This means that it is bound in the page cache until
   * the node goes down. Hence, get_extent_page always return
   * for extent pages successfully. Otherwise it will crash.
   */
  pgman.get_extent_page(signal, preq, flags);
  thrjam(jamBuf);
  GlobalPage *ptr_p = pgman.m_ptr.p;
  bool v2 = (file_ptr.p->m_ndb_version >= NDB_DISK_V2);
  File_formats::Datafile::Extent_page *page =
      (File_formats::Datafile::Extent_page *)ptr_p;

  Uint32 *ext_table_id =
      page->get_table_id(val.m_extent_no, val.m_extent_size, v2);
  Uint32 *ext_fragment_id =
      page->get_fragment_id(val.m_extent_no, val.m_extent_size, v2);
  Uint32 *ext_create_table_version =
      page->get_create_table_version(val.m_extent_no, val.m_extent_size, v2);

  if (read) {
    thrjam(jamBuf);
    tableId = *ext_table_id;
    fragId = *ext_fragment_id;
    create_table_version = *ext_create_table_version;
  } else {
    Uint32 eh_words;
    thrjam(jamBuf);
    File_formats::Datafile::Extent_header *header =
        page->get_header(val.m_extent_no, val.m_extent_size, v2);
    eh_words =
        File_formats::Datafile::extent_header_words(val.m_extent_size, v2);
    memset(header, 0, 4 * eh_words);
    *ext_table_id = tableId;
    *ext_fragment_id = fragId;
    if (v2) {
      thrjam(jamBuf);
      *ext_create_table_version = create_table_version;
    }
  }
  unlock_extent_page(file_ptr.p, val.m_extent_page_no);
}

int Tsman::update_page_free_bits(Signal *signal, Local_key *key,
                                 unsigned new_committed_bits) {
  EmulatedJamBuffer *const jamBuf = getThrJamBuf();

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
  preq.m_table_id = RNIL;
  preq.m_fragment_id = 0;

  /**
   * Handling of unmapped extent header pages is not implemented
   */
  lock_extent_page(file_ptr.p, val.m_extent_page_no);
  int flags = 0;
  Page_cache_client pgman(this, m_pgman);
  pgman.get_extent_page(signal, preq, flags);
  {
    thrjam(jamBuf);
    GlobalPage *ptr_p = pgman.m_ptr.p;

    bool v2 = (file_ptr.p->m_ndb_version >= NDB_DISK_V2);
    File_formats::Datafile::Extent_page *page =
        (File_formats::Datafile::Extent_page *)ptr_p;
    File_formats::Datafile::Extent_data *ext_data =
        page->get_extent_data(val.m_extent_no, val.m_extent_size, v2);
    Uint32 *ext_table_id =
        page->get_table_id(val.m_extent_no, val.m_extent_size, v2);
    if ((*ext_table_id) == RNIL) {
      Uint32 *ext_fragment_id =
          page->get_fragment_id(val.m_extent_no, val.m_extent_size, v2);
      thrjam(jamBuf);

      char key_str[MAX_LOG_MESSAGE_SIZE];
      printLocal_Key(key_str, MAX_LOG_MESSAGE_SIZE, *key);
      char data_str[MAX_LOG_MESSAGE_SIZE];
      print(data_str, MAX_LOG_MESSAGE_SIZE, *ext_data);
      g_eventLogger->info(
          "table: %u fragment: %u update page free bits page: %s %s",
          *ext_table_id, *ext_fragment_id, key_str, data_str);
    }
    ndbrequire((*ext_table_id) != RNIL);
    Uint32 page_no_in_extent = calc_page_no_in_extent(key->m_page_no, &val);
    /**
     * Toggle word
     */
    ndbassert((new_committed_bits & ~(COMMITTED_MASK)) == 0);
    Uint32 old_free_bits = ext_data->get_free_bits(page_no_in_extent);
    Uint32 old_uncommitted_bits = old_free_bits & UNCOMMITTED_MASK;
    Uint32 new_free_bits = old_uncommitted_bits | new_committed_bits;
    DEB_TSMAN((
        "(%u), page:(%u,%u), extent_page: %u, page_no_in_extent: %u,"
        " old_free_bits: %u, old_uncommitted_bits: %u,"
        " new_free_bits: %u",
        instance(), key->m_file_no, key->m_page_no, preq.m_page.m_page_no,
        page_no_in_extent, old_free_bits, old_uncommitted_bits, new_free_bits));

    /**
     * We have now read the free bits in the page. If these are the same as we
     * are going to set then there is no reason to update the page on disk.
     * If they are different we will update the disk page as well.
     * We do this by calling get_extent_page with COMMIT_REQ set followed by
     * calling update_lsn. This will ensure that the page is dirty and will be
     * written in the next fragment LCP.
     *
     * There are calls to update_free_bits where we update the free bits but
     * we don't checkpoint those changes to disk. The reason is that we only
     * care about changes to the COMMITTED state bits. The uncommitted state
     * bits are only valid as long as the node is up. During recovery we will
     * simply copy the COMMITTED bits to the UNCOMMITTED bits. Thus we need
     * only mark a page as dirty after updating its COMMITTED bits and only
     * if we actually change those. new_free_bits and old_free_bits will only
     * differ if we change the COMMITTED bits here.
     */
    if (new_free_bits != old_free_bits) {
      Uint32 old_committed_bits = old_free_bits & COMMITTED_MASK;
      if (old_committed_bits == new_committed_bits) {
        thrjam(jamBuf);
        ext_data->update_free_bits(page_no_in_extent, new_free_bits);
      } else {
        DEB_TSMAN_IO(
            ("(%u), page:(%u,%u), extent_page: (%u,%u) "
             "page_no_in_extent: %u,"
             " old_committed_bits: %u,"
             " new_committed_bits: %u",
             instance(), key->m_file_no, key->m_page_no, key->m_file_no,
             preq.m_page.m_page_no, page_no_in_extent, old_committed_bits,
             new_committed_bits));
        thrjam(jamBuf);
        flags = Page_cache_client::COMMIT_REQ;
        pgman.get_extent_page(signal, preq, flags);
        thrjam(jamBuf);
        ext_data->update_free_bits(page_no_in_extent, new_free_bits);
        pgman.update_lsn(signal, preq.m_page, 0);
      }
    }
  }
  unlock_extent_page(file_ptr.p, val.m_extent_page_no);
  return 0;
}

int Tsman::get_page_free_bits(Signal *signal, Local_key *key,
                              unsigned *uncommitted, unsigned *committed) {
  EmulatedJamBuffer *const jamBuf = getThrJamBuf();

  thrjamEntry(jamBuf);

  Ptr<Datafile> file_ptr;
  Datafile file_key;
  file_key.m_file_no = key->m_file_no;
  ndbrequire(m_file_hash.find(file_ptr, file_key));

  struct req val = lookup_extent(key->m_page_no, file_ptr.p);

  Page_cache_client::Request preq;
  preq.m_page.m_page_no = val.m_extent_page_no;
  preq.m_page.m_file_no = key->m_file_no;
  preq.m_table_id = RNIL;
  preq.m_fragment_id = 0;

  /**
   * Handling of unmapped extent header pages is not implemented
   */
  lock_extent_page(file_ptr.p, val.m_extent_page_no);
  int flags = 0;
  Page_cache_client pgman(this, m_pgman);
  pgman.get_extent_page(signal, preq, flags);
  {
    thrjam(jamBuf);
    GlobalPage *ptr_p = pgman.m_ptr.p;

    Uint32 bits;
    bool v2 = (file_ptr.p->m_ndb_version >= NDB_DISK_V2);
    File_formats::Datafile::Extent_page *page =
        (File_formats::Datafile::Extent_page *)ptr_p;
    File_formats::Datafile::Extent_data *ext_data =
        page->get_extent_data(val.m_extent_no, val.m_extent_size, v2);
    Uint32 *ext_table_id =
        page->get_table_id(val.m_extent_no, val.m_extent_size, v2);

    ndbrequire((*ext_table_id) != RNIL);
    Uint32 page_no_in_extent = calc_page_no_in_extent(key->m_page_no, &val);
    bits = ext_data->get_free_bits(page_no_in_extent);
    *uncommitted = (bits & UNCOMMITTED_MASK) >> UNCOMMITTED_SHIFT;
    *committed = (bits & COMMITTED_MASK);
  }
  unlock_extent_page(file_ptr.p, val.m_extent_page_no);
  return 0;
}

int Tsman::unmap_page(Signal *signal, Local_key *key, Uint32 uncommitted_bits) {
  EmulatedJamBuffer *const jamBuf = getThrJamBuf();

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
  preq.m_table_id = RNIL;
  preq.m_fragment_id = 0;

  /**
   * Handling of unmapped extent header pages is not implemented
   */
  lock_extent_page(file_ptr.p, val.m_extent_page_no);
  int flags = 0;
  Page_cache_client pgman(this, m_pgman);
  pgman.get_extent_page(signal, preq, flags);
  {
    thrjam(jamBuf);
    GlobalPage *ptr_p = pgman.m_ptr.p;

    ndbassert(((uncommitted_bits << UNCOMMITTED_SHIFT) & ~UNCOMMITTED_MASK) ==
              0);
    bool v2 = (file_ptr.p->m_ndb_version >= NDB_DISK_V2);
    File_formats::Datafile::Extent_page *page =
        (File_formats::Datafile::Extent_page *)ptr_p;
    File_formats::Datafile::Extent_data *ext_data =
        page->get_extent_data(val.m_extent_no, val.m_extent_size, v2);
    Uint32 *ext_table_id =
        page->get_table_id(val.m_extent_no, val.m_extent_size, v2);
    if ((*ext_table_id) == RNIL) {
      Uint32 *ext_fragment_id =
          page->get_fragment_id(val.m_extent_no, val.m_extent_size, v2);
      thrjam(jamBuf);

      char key_str[MAX_LOG_MESSAGE_SIZE];
      printLocal_Key(key_str, MAX_LOG_MESSAGE_SIZE, *key);
      char data_str[MAX_LOG_MESSAGE_SIZE];
      print(data_str, MAX_LOG_MESSAGE_SIZE, *ext_data);

      g_eventLogger->info("table: %u fragment: %u trying to unmap page: %s %s",
                          *ext_table_id, *ext_fragment_id, key_str, data_str);
      ndbabort();
    }
    Uint32 page_no_in_extent = calc_page_no_in_extent(key->m_page_no, &val);
    /**
     * Toggle word
     * No need to make page dirty since only UNCOMMITTED bits are changed.
     */
    Uint32 src = ext_data->get_free_bits(page_no_in_extent) & COMMITTED_MASK;
    ext_data->update_free_bits(page_no_in_extent,
                               src | (uncommitted_bits << UNCOMMITTED_SHIFT));
  }
  unlock_extent_page(file_ptr.p, val.m_extent_page_no);
  return 0;
}

int Tsman::restart_undo_page_free_bits(Signal *signal, Uint32 tableId,
                                       Uint32 fragId,
                                       Uint32 create_table_version,
                                       Local_key *key, unsigned bits) {
  EmulatedJamBuffer *const jamBuf = getThrJamBuf();

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
  preq.m_table_id = RNIL;
  preq.m_fragment_id = 0;

  /**
   * Handling of unmapped extent header pages is not implemented
   */
  lock_extent_page(file_ptr.p, val.m_extent_page_no);
  int flags = Page_cache_client::DIRTY_REQ;
  Page_cache_client pgman(this, m_pgman);
  pgman.get_extent_page(signal, preq, flags);
  {
    thrjam(jamBuf);
    GlobalPage *ptr_p = pgman.m_ptr.p;

    ndbassert((bits & ~(COMMITTED_MASK)) == 0);
    bool v2 = (file_ptr.p->m_ndb_version >= NDB_DISK_V2);
    File_formats::Datafile::Extent_page *page =
        (File_formats::Datafile::Extent_page *)ptr_p;
    File_formats::Datafile::Extent_data *ext_data =
        page->get_extent_data(val.m_extent_no, val.m_extent_size, v2);
    Uint32 *ext_table_id =
        page->get_table_id(val.m_extent_no, val.m_extent_size, v2);
    Uint32 *ext_fragment_id =
        page->get_fragment_id(val.m_extent_no, val.m_extent_size, v2);
    Uint32 *ext_create_table_version =
        page->get_create_table_version(val.m_extent_no, val.m_extent_size, v2);

    if ((*ext_table_id) != tableId || (*ext_fragment_id) != fragId ||
        (ext_create_table_version != NULL &&
         (*ext_create_table_version) != create_table_version)) {
      thrjam(jamBuf);
      /**
       * This is a special situation. We want to UNDO log a page that
       * belongs to an extent that hasn't yet been written to disk.
       * This is a possible situation. The following must have happened.
       * 1) A new extent was allocated to the table
       * 2) A page was allocated to the extent and a record written to it
       * 3) The page was flushed to disk.
       * 4) The extent page was obviously written as part of this, but it
       *    the node was stopped before the extent page was flushed to disk.
       *
       * Extent pages are flushed to disk at start of an LCP to ensure that
       * we start the LCP with all extents written to disk. Next we ensure
       * that the extent pages are written as part of the last fragment LCP
       * in the LCP. This means that we could have written pages during LCP
       * that never got its extent page flushed to disk. The WAL principle
       * does however guarantee that we for each such write there is also a
       * corresponding UNDO log written before the write. So we can trust
       * that UNDO log execution will pass all those pages that are missing
       * their extent definition on disk. This is the place in the code
       * where we discover such a missing extent definition. We will create
       * the extent here and now. Later in the restart process TUP will
       * build the memory structures for this and all the other extents
       * based on this information written here after completing the
       * UNDO log execution.
       *
       * Also no need to update free lists of extent in TSMAN since these
       * lists are also built while scanning extent headers.
       */
      Uint32 size = val.m_extent_size;
      File_formats::Datafile::Extent_header *header =
          page->get_header(val.m_extent_no, size, v2);
      Uint32 eh_words = File_formats::Datafile::extent_header_words(size, v2);
      memset(header, 0, 4 * eh_words);
      *ext_table_id = tableId;
      *ext_fragment_id = fragId;
      if (v2) {
        thrjam(jamBuf);
        *ext_create_table_version = create_table_version;
      }
      g_eventLogger->info(
          "Wrote extent that wasn't written before node stop"
          " for tab(%u,%u):%u, extent: %u",
          tableId, fragId, create_table_version, val.m_extent_no);
    }

    Uint32 page_no_in_extent = calc_page_no_in_extent(key->m_page_no, &val);
    Uint32 src = ext_data->get_free_bits(page_no_in_extent);

    if (DBG_UNDO) {
      ndbout << "tsman: apply " << *key << " " << (src & COMMITTED_MASK)
             << " -> " << bits << endl;
    }
#ifdef DEBUG_TSMAN_RESTART
    Uint32 per_page = file_ptr.p->m_online.m_extent_headers_per_extent_page;
    Uint32 extent_page_no = val.m_extent_page_no;
    Uint32 extent_no = (per_page * extent_page_no) + val.m_extent_no;
    DEB_TSMAN_RESTART(
        ("page(%u,%u) in tab(%u,%u):%u, bits = %u, extent: %u"
         ", src: %u, page_no_in_extent: %u",
         key->m_file_no, key->m_page_no, tableId, fragId, create_table_version,
         bits, extent_no, src, page_no_in_extent));
#endif
    /* Toggle word */
    ext_data->update_free_bits(page_no_in_extent,
                               bits | (bits << UNCOMMITTED_SHIFT));
  }
  unlock_extent_page(file_ptr.p, val.m_extent_page_no);
  return 0;
}

void Tsman::execALLOC_PAGE_REQ(Signal *signal) {
  EmulatedJamBuffer *const jamBuf = getThrJamBuf();

  thrjamEntry(jamBuf);
  AllocPageReq *rep = (AllocPageReq *)signal->getDataPtr();
  AllocPageReq req = *rep;
  AllocPageReq::ErrorCode err =
      AllocPageReq::UnmappedExtentPageIsNotImplemented;

  /**
   * 1) Get file the extent belongs to
   * 2) Compute which extent_no key belongs to
   *    key.m_page_no is the page number of a page in the extent
   * 3) Find out which page extent_no belongs to
   * 4) Undo log m_page_bitmask
   * 5) Update m_page_bitmask
   */
  Ptr<Datafile> file_ptr;
  Datafile file_key;
  file_key.m_file_no = req.key.m_file_no;
  thrjamLine(jamBuf, Uint16(req.key.m_file_no));
  thrjamLine(jamBuf, Uint16(req.key.m_page_no));
  ndbrequire(m_file_hash.find(file_ptr, file_key));

  struct req val = lookup_extent(req.key.m_page_no, file_ptr.p);
  Uint32 page_no_in_extent = calc_page_no_in_extent(req.key.m_page_no, &val);

  Page_cache_client::Request preq;
  preq.m_page.m_page_no = val.m_extent_page_no;
  preq.m_page.m_file_no = req.key.m_file_no;
  preq.m_table_id = RNIL;
  preq.m_fragment_id = 0;

  Uint32 SZ = File_formats::Datafile::EXTENT_HEADER_BITMASK_BITS_PER_PAGE;

  /**
   * Handling of unmapped extent header pages is not implemented
   *
   * There is no need to make the extent page dirty here. The reason is that
   * it will only update the uncommitted bits and those don't matter at
   * restarts and thus it doesn't really matter for recovery that we make the
   * page the dirty here.
   */
  int flags = 0;
  Uint32 page_no;
  Uint32 src_bits;
  File_formats::Datafile::Extent_data *ext_data = NULL;
  lock_extent_page(file_ptr.p, val.m_extent_page_no);
  Page_cache_client pgman(this, m_pgman);
  pgman.get_extent_page(signal, preq, flags);
  {
    thrjam(jamBuf);
    GlobalPage *ptr_p = pgman.m_ptr.p;
    bool v2 = (file_ptr.p->m_ndb_version >= NDB_DISK_V2);

    File_formats::Datafile::Extent_page *page =
        (File_formats::Datafile::Extent_page *)ptr_p;
    ext_data = page->get_extent_data(val.m_extent_no, val.m_extent_size, v2);

    Uint32 word = ext_data->get_free_word_offset(page_no_in_extent);
    Uint32 shift = SZ * (page_no_in_extent & 7);

    /**
     * 0 = 00 - free - 100% free
     * 1 = 01 - at least some row free
     * 2 = 10 - full
     * 3 = 11 - full, special state set when in uncommitted state
     */

    Uint32 reqbits = req.bits << UNCOMMITTED_SHIFT;

    /**
     * Search
     */
    Uint32 *src = ((Uint32 *)ext_data) + word;
    for (page_no = page_no_in_extent; page_no < val.m_extent_size; page_no++) {
      thrjam(jamBuf);
      src_bits = (*src >> shift) & ((1 << SZ) - 1);
      if ((src_bits & UNCOMMITTED_MASK) <= reqbits) {
        thrjam(jamBuf);
        goto found;
      }
      shift += SZ;
      src = src + (shift >> 5);
      shift &= 31;
    }

    shift = 0;
    src = (Uint32 *)ext_data;
    for (page_no = 0; page_no < page_no_in_extent; page_no++) {
      thrjam(jamBuf);
      src_bits = (*src >> shift) & ((1 << SZ) - 1);
      if ((src_bits & UNCOMMITTED_MASK) <= reqbits) {
        thrjam(jamBuf);
        goto found;
      }
      shift += SZ;
      src = src + (shift >> 5);
      shift &= 31;
    }
    err = AllocPageReq::NoPageFree;
  }

  rep->reply.errorCode = err;
  unlock_extent_page(file_ptr.p, val.m_extent_page_no);
  return;

found:
  ext_data->update_free_bits(page_no, src_bits | UNCOMMITTED_MASK);

  rep->bits = (src_bits & UNCOMMITTED_MASK) >> UNCOMMITTED_SHIFT;
  rep->key.m_page_no = req.key.m_page_no + page_no - page_no_in_extent;
  thrjamLine(jamBuf, Uint16(rep->key.m_page_no));
  rep->reply.errorCode = 0;
  unlock_extent_page(file_ptr.p, val.m_extent_page_no);
  return;
}

void Tsman::execLCP_FRAG_ORD(Signal *signal) {
  jamEntry();
  ndbrequire(!m_lcp_ongoing);
  m_lcp_ongoing = true;
}

void Tsman::execEND_LCPREQ(Signal *signal) {
  EndLcpReq *req = (EndLcpReq *)signal->getDataPtr();
  jamEntry();
  ndbrequire(m_lcp_ongoing);
  m_lcp_ongoing = false;
  m_end_lcp_ref = req->senderRef;

  /**
   * Move extents from "lcp" free list to real free list
   */
  Ptr<Tablespace> ptr;
  if (m_tablespace_list.first(ptr)) {
    jam();
    ptr.p->m_ref_count++;
    signal->theData[0] = TsmanContinueB::END_LCP;
    signal->theData[1] = ptr.i;
    signal->theData[2] = 0;     // free
    signal->theData[3] = RNIL;  // first
    sendSignal(reference(), GSN_CONTINUEB, signal, 4, JBB);
    return;
  }
  sendEND_LCPCONF(signal);
}

void Tsman::end_lcp(Signal *signal, Uint32 ptrI, Uint32 list, Uint32 filePtrI) {
  Ptr<Tablespace> ts_ptr;

  lock_alloc_extent();
  m_tablespace_list.getPtr(ts_ptr, ptrI);
  ndbrequire(ts_ptr.p->m_ref_count);
  ts_ptr.p->m_ref_count--;

  Ptr<Datafile> file;
  file.i = filePtrI;
  Uint32 nextFile = RNIL;

  switch (list) {
    case 0: {
      jam();
      Local_datafile_list tmp(m_file_pool, ts_ptr.p->m_free_files);
      if (file.i == RNIL) {
        jam();
        if (!tmp.first(file)) {
          jam();
          list = 1;
          goto next;
        }
      } else {
        jam();
        tmp.getPtr(file);
        ndbrequire(file.p->m_ref_count);
        file.p->m_ref_count--;
      }
      break;
    }
    case 1: {
      jam();
      Local_datafile_list tmp(m_file_pool, ts_ptr.p->m_full_files);
      if (file.i == RNIL) {
        jam();
        if (!tmp.first(file)) {
          jam();
          list = 0;
          if (m_tablespace_list.next(ts_ptr)) {
            jam();
          }
          goto next;
        }
      } else {
        jam();
        tmp.getPtr(file);
        ndbrequire(file.p->m_ref_count);
        file.p->m_ref_count--;
      }
      break;
    }
    default:
      ndbabort();
  }

  nextFile = file.p->nextList;

  /**
   * Move extents...
   */
  if (file.p->m_online.m_lcp_free_extent_head != RNIL) {
    jam();
    g_eventLogger->info("moving extents (%d %d) to real free list %d",
                        file.p->m_online.m_lcp_free_extent_head,
                        file.p->m_online.m_lcp_free_extent_tail,
                        file.p->m_online.m_first_free_extent);

    // Update the used extents of the tablespace
    ts_ptr.p->m_total_used_extents -= file.p->m_online.m_lcp_free_extent_count;
    DEB_TSMAN_NUM_EXTENTS(
        ("FREE_EXTENT_REQ(LCP):"
         " num_extents: %llu",
         ts_ptr.p->m_total_used_extents));
    file.p->m_online.m_lcp_free_extent_count = 0;

    if (file.p->m_online.m_first_free_extent == RNIL) {
      jam();
      ndbrequire(list == 1);
      file.p->m_online.m_first_free_extent =
          file.p->m_online.m_lcp_free_extent_head;
      file.p->m_online.m_lcp_free_extent_head = RNIL;
      file.p->m_online.m_lcp_free_extent_tail = RNIL;

      Local_datafile_list free_list(m_file_pool, ts_ptr.p->m_free_files);
      Local_datafile_list full(m_file_pool, ts_ptr.p->m_full_files);
      full.remove(file);
      free_list.addLast(file);
    } else {
      jam();
      bool v2 = (file.p->m_ndb_version >= NDB_DISK_V2);
      Uint32 extent = file.p->m_online.m_lcp_free_extent_tail;
      Uint32 size = ts_ptr.p->m_extent_size;
      Uint32 eh_words = File_formats::Datafile::extent_header_words(size, v2);
      Uint32 per_page =
          File_formats::Datafile::extent_page_words(v2) / eh_words;

      Uint32 page_no = extent / per_page;
      Uint32 extent_no = extent % per_page;

      Page_cache_client::Request preq;
      preq.m_page.m_page_no = page_no;
      preq.m_page.m_file_no = file.p->m_file_no;
      preq.m_table_id = RNIL;
      preq.m_fragment_id = 0;

      lock_extent_page(file.p, page_no);
      int flags = Page_cache_client::DIRTY_REQ;
      Page_cache_client pgman(this, m_pgman);
      pgman.get_extent_page(signal, preq, flags);

      GlobalPage *ptr_p = pgman.m_ptr.p;

      File_formats::Datafile::Extent_page *page =
          (File_formats::Datafile::Extent_page *)ptr_p;
      Uint32 *next_free_extent =
          page->get_next_free_extent(extent_no, size, v2);

      *next_free_extent = file.p->m_online.m_first_free_extent;
      unlock_extent_page(file.p, page_no);
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
  if (file.i == RNIL) {
    if (list == 0) {
      jam();
      list = 1;
    } else {
      jam();
      list = 0;
      m_tablespace_list.next(ts_ptr);
    }
  } else {
    jam();
    ndbrequire(ts_ptr.i != RNIL);
    m_file_pool.getPtr(file);
    file.p->m_ref_count++;
  }

next:
  if (ts_ptr.i != RNIL) {
    jam();
    ts_ptr.p->m_ref_count++;
    unlock_alloc_extent();

    signal->theData[0] = TsmanContinueB::END_LCP;
    signal->theData[1] = ts_ptr.i;
    signal->theData[2] = list;
    signal->theData[3] = file.i;
    sendSignal(reference(), GSN_CONTINUEB, signal, 4, JBB);
    return;
  }
  unlock_alloc_extent();
  sendEND_LCPCONF(signal);
}

void Tsman::sendEND_LCPCONF(Signal *signal) {
  BlockReference ref = m_end_lcp_ref;
  EndLcpConf *conf = (EndLcpConf *)signal->getDataPtr();
  conf->senderData = 0; /* Ignored */
  conf->senderRef = reference();
  sendSignal(ref, GSN_END_LCPCONF, signal, EndLcpConf::SignalLength, JBB);
}

int Tablespace_client::get_tablespace_info(CreateFilegroupImplReq *rep) {
  EmulatedJamBuffer *const jamBuf = getThrJamBuf();

  thrjamEntry(jamBuf);
  Ptr<Tsman::Tablespace> ts_ptr;
  if (m_tsman->m_tablespace_hash.find(ts_ptr, m_tablespace_id)) {
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

void Tsman::execGET_TABINFOREQ(Signal *signal) {
  jamEntry();

  if (!assembleFragments(signal)) {
    jam();
    return;
  }

  GetTabInfoReq *const req = (GetTabInfoReq *)&signal->theData[0];

  Uint32 tableId = req->tableId;
  const Uint32 reqType = req->requestType & (~GetTabInfoReq::LongSignalConf);
  BlockReference retRef = req->senderRef;
  Uint32 senderData = req->senderData;

  if (reqType == GetTabInfoReq::RequestByName) {
    jam();
    SectionHandle handle(this, signal);
    releaseSections(handle);

    sendGET_TABINFOREF(signal, req, GetTabInfoRef::NoFetchByName);
    return;
  }

  Datafile_hash::Iterator iter;
  if (!m_file_hash.first(iter)) {
    ndbabort();
    return;  // Silence compiler warning
  }

  while (iter.curr.p->m_file_id != tableId && m_file_hash.next(iter)) {
    jam();
  }

  if (iter.curr.p->m_file_id != tableId) {
    jam();
    sendGET_TABINFOREF(signal, req, GetTabInfoRef::InvalidTableId);
    return;
  }

  const Ptr<Datafile> &file_ptr = iter.curr;

  jam();

  lock_alloc_extent();
  Uint32 total_free_extents = file_ptr.p->m_online.m_data_pages;
  total_free_extents /= file_ptr.p->m_extent_size;
  total_free_extents -= file_ptr.p->m_online.m_used_extent_cnt;
  unlock_alloc_extent();

  GetTabInfoConf *conf = (GetTabInfoConf *)&signal->theData[0];

  conf->senderData = senderData;
  conf->tableId = tableId;
  conf->freeExtents = total_free_extents;
  conf->tableType = DictTabInfo::Datafile;
  conf->senderRef = reference();
  sendSignal(retRef, GSN_GET_TABINFO_CONF, signal, GetTabInfoConf::SignalLength,
             JBB);
}

void Tsman::sendGET_TABINFOREF(Signal *signal, GetTabInfoReq *req,
                               GetTabInfoRef::ErrorCode errorCode) {
  jamEntry();
  GetTabInfoRef *const ref = (GetTabInfoRef *)&signal->theData[0];
  /**
   * The format of GetTabInfo Req/Ref is the same
   */
  BlockReference retRef = req->senderRef;
  ref->errorCode = errorCode;

  sendSignal(retRef, GSN_GET_TABINFOREF, signal, signal->length(), JBB);
}

/**
 * The concurrency model for TSMAN works in the following manner.
 * In ndbd there is only one thread running the blocks, so no interaction
 * is possible.
 *
 * There are number of important things that we need to control usage of in
 * TSMAN. These are:
 *
 * 1) File hash and extent parameters per tablespace.
 * 2) The extent pages
 * 3) The hash table in the extra PGMAN block.
 * 4) The list of free extents
 * 5) Other internal block variables
 *
 * Each thread instance (Proxy instance + all LDM threads) that interacts
 * with TSMAN has its own mutex. By locking this it ensures that no one can
 * own the entire blocks data structures until we're done with our work.
 *
 * There are essentially two types of interactions with TSMAN that don't come
 * from signals to the block. The first is various calls to manipulate one of
 * the extent pages. There is a number of such calls.
 * The second type is allocating and freeing extents.
 *
 * The first type starts by using the m_file_hash to find the file record.
 * Thus all changes of this hash table must be well protected from these
 * external calls. Next they use PGMAN to get an extent page from the
 * page cache. This call uses a hash table in the extra PGMAN worker to
 * find the page entry. We need to protect this hash table and we need to
 * protect the page entry and the page itself. The hash table in the extra
 * PGMAN worker is only changed by calls using LOCK_PAGE and UNLOCK_PAGE,
 * thus these calls must be well protected from these external calls.
 * The extent pages must also be protected from being used from more than
 * one place.
 *
 * By locking the mutex for all instances we ensure that all accesses to
 * extent pages and their page entries, the hash table of extent page entries
 * in the extra PGMAN instance, internal block variables is locked.
 *
 * Now many instances can access extent pages in parallel. They can also
 * access the same extent pages. Thus we need to control accesses to those
 * extent pages coming from the various thread instances. We keep a set of
 * mutexes in each tablespace file for this purpose, we use 32 different
 * mutexes and use a simple AND function as hash table to use the proper mutex
 * for each extent page.
 *
 * The calls to allocate and free extents touch a few more things. At first
 * they use the m_tablespace_hash to find the tablespace record. Thus any
 * changes of this must either lock all client locks or lock the allocate
 * extent lock. Second they use a number of variables on the tablespace
 * record (m_total_used_extents, m_total_extents) and they manipulate the
 * m_full_files and m_free_files lists on the tablespace record. Finally they
 * manipulate the list of free extents by updating the file record and one
 * extent page. The first pointer of the list is stored on the file record
 * and the next pointer is stored on an extent page.
 *
 * The free extent moves the free to a special list later managed by the
 * end_lcp call, this list has a head and tail on the file record. In
 * addition they manipulate most of what allocate extent did as well.
 *
 * The signals executed within the block need not be protected from each
 * other since the block is as usual single threaded, so only one thread
 * can execute inside the block at a time. Thus we need only protect against
 * the above external calls. A heavy way of protecting is to lock ALL
 * client locks. This ensures that no external calls can execute in parallel
 * with our execution. However for the end_lcp call that happens frequently
 * we try to avoid this by only locking the extent allocation and locking
 * the extent page that is manipulated as part of updating the free list
 * of extents. end_lcp is executed as a signal, so it doesn't need
 * protection against other list manipulations happening as part of
 * signal executions.
 *
 * We also use the client lock when initialising a page, this protects
 * the tablespace record. It should not be touched in this phase, but
 * we protect for any future changes.
 */
void Tsman::client_lock(Uint32 instance) const {
  (void)instance;
  if (isNdbMtLqh()) {
    int ret = NdbMutex_Lock(m_client_mutex[instance]);
    ndbrequire(ret == 0);
  }
}

void Tsman::client_unlock(Uint32 instance) const {
  (void)instance;
  if (isNdbMtLqh()) {
    int ret = NdbMutex_Unlock(m_client_mutex[instance]);
    ndbrequire(ret == 0);
  }
}

void Tsman::client_lock() const {
  if (isNdbMtLqh()) {
    for (Uint32 i = 0; i < MAX_NDBMT_LQH_THREADS + 1; i++) {
      int ret = NdbMutex_Lock(m_client_mutex[i]);
      ndbrequire(ret == 0);
    }
  }
}

void Tsman::client_unlock() const {
  if (isNdbMtLqh()) {
    for (Uint32 i = 0; i < MAX_NDBMT_LQH_THREADS + 1; i++) {
      int ret = NdbMutex_Unlock(m_client_mutex[i]);
      ndbrequire(ret == 0);
    }
  }
}

bool Tsman::is_datafile_ready(Uint32 file_no) {
  Ptr<Datafile> file_ptr;
  Datafile file_key;
  file_key.m_file_no = file_no;
  if (m_file_hash.find(file_ptr, file_key)) {
    if (file_ptr.p->m_state == Datafile::FS_CREATING) return false;
    return true;
  }
  return false;
}

void Tsman::lock_extent_page(Uint32 file_no, Uint32 page_no) {
  if (isNdbMtLqh()) {
    Ptr<Datafile> file_ptr;
    Datafile file_key;
    file_key.m_file_no = file_no;
    ndbrequire(m_file_hash.find(file_ptr, file_key));
    lock_extent_page(file_ptr.p, page_no);
  }
}

void Tsman::unlock_extent_page(Uint32 file_no, Uint32 page_no) {
  if (isNdbMtLqh()) {
    Ptr<Datafile> file_ptr;
    Datafile file_key;
    file_key.m_file_no = file_no;
    ndbrequire(m_file_hash.find(file_ptr, file_key));
    unlock_extent_page(file_ptr.p, page_no);
  }
}

void Tsman::lock_extent_page(Datafile *filePtrP, Uint32 page_no) {
  if (isNdbMtLqh()) {
    Uint32 mutex_id = page_no & (NUM_EXTENT_PAGE_MUTEXES - 1);
    NdbMutex_Lock(&filePtrP->m_extent_page_mutex[mutex_id]);
  }
}

void Tsman::unlock_extent_page(Datafile *filePtrP, Uint32 page_no) {
  if (isNdbMtLqh()) {
    Uint32 mutex_id = page_no & (NUM_EXTENT_PAGE_MUTEXES - 1);
    NdbMutex_Unlock(&filePtrP->m_extent_page_mutex[mutex_id]);
  }
}

void Tsman::lock_alloc_extent() {
  if (isNdbMtLqh()) {
    int ret = NdbMutex_Lock(m_alloc_extent_mutex);
    ndbrequire(ret == 0);
  }
}

void Tsman::unlock_alloc_extent() {
  if (isNdbMtLqh()) {
    int ret = NdbMutex_Unlock(m_alloc_extent_mutex);
    ndbrequire(ret == 0);
  }
}
