/*
   Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.

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

#include <ndb_global.h>

#include "Ndbfs.hpp"
#include "AsyncFile.hpp"

#ifdef NDB_WIN
#include "Win32AsyncFile.hpp"
#else
#include "PosixAsyncFile.hpp"
#endif

#include <signaldata/FsOpenReq.hpp>
#include <signaldata/FsCloseReq.hpp>
#include <signaldata/FsReadWriteReq.hpp>
#include <signaldata/FsAppendReq.hpp>
#include <signaldata/FsRemoveReq.hpp>
#include <signaldata/FsConf.hpp>
#include <signaldata/FsRef.hpp>
#include <signaldata/NdbfsContinueB.hpp>
#include <signaldata/DumpStateOrd.hpp>
#include <signaldata/AllocMem.hpp>

#include <RefConvert.hpp>
#include <portlib/NdbDir.hpp>
#include <NdbOut.hpp>
#include <Configuration.hpp>

#include <EventLogger.hpp>
extern EventLogger * g_eventLogger;

NdbMutex g_active_bound_threads_mutex;

inline
int pageSize( const NewVARIABLE* baseAddrRef )
{
   int log_psize;
   int log_qsize = baseAddrRef->bits.q;
   int log_vsize = baseAddrRef->bits.v;
   if (log_vsize < 3)
      log_vsize = 3;
   log_psize = log_qsize + log_vsize - 3;
   return (1 << log_psize);
}

Ndbfs::Ndbfs(Block_context& ctx) :
  SimulatedBlock(NDBFS, ctx),
  scanningInProgress(false),
  theLastId(0),
  theRequestPool(0),
  m_maxOpenedFiles(0),
  m_bound_threads_cnt(0),
  m_unbounds_threads_cnt(0),
  m_active_bound_threads_cnt(0)
{
  BLOCK_CONSTRUCTOR(Ndbfs);

  NdbMutex_Init(&g_active_bound_threads_mutex);

  // Set received signals
  addRecSignal(GSN_READ_CONFIG_REQ, &Ndbfs::execREAD_CONFIG_REQ);
  addRecSignal(GSN_DUMP_STATE_ORD,  &Ndbfs::execDUMP_STATE_ORD);
  addRecSignal(GSN_STTOR,  &Ndbfs::execSTTOR);
  addRecSignal(GSN_FSOPENREQ, &Ndbfs::execFSOPENREQ);
  addRecSignal(GSN_FSCLOSEREQ, &Ndbfs::execFSCLOSEREQ);
  addRecSignal(GSN_FSWRITEREQ, &Ndbfs::execFSWRITEREQ);
  addRecSignal(GSN_FSREADREQ, &Ndbfs::execFSREADREQ);
  addRecSignal(GSN_FSSYNCREQ, &Ndbfs::execFSSYNCREQ);
  addRecSignal(GSN_CONTINUEB, &Ndbfs::execCONTINUEB);
  addRecSignal(GSN_FSAPPENDREQ, &Ndbfs::execFSAPPENDREQ);
  addRecSignal(GSN_FSREMOVEREQ, &Ndbfs::execFSREMOVEREQ);
  addRecSignal(GSN_ALLOC_MEM_REQ, &Ndbfs::execALLOC_MEM_REQ);
  addRecSignal(GSN_SEND_PACKED, &Ndbfs::execSEND_PACKED, true);
  addRecSignal(GSN_BUILD_INDX_IMPL_REQ, &Ndbfs::execBUILD_INDX_IMPL_REQ);
   // Set send signals
  addRecSignal(GSN_FSSUSPENDORD, &Ndbfs::execFSSUSPENDORD);

  theRequestPool = new Pool<Request>;
}

Ndbfs::~Ndbfs()
{
  /**
   * Stop all unbound threads
   */

  /**
   * Post enought Request::end to saturate all unbound threads
   */
  Request request;
  request.action = Request::end;
  for (unsigned i = 0; i < theThreads.size(); i++)
  {
    theToBoundThreads.writeChannel(&request);
    theToUnboundThreads.writeChannel(&request);
  }

  for (unsigned i = 0; i < theThreads.size(); i++)
  {
    AsyncIoThread * thr = theThreads[i];
    thr->shutdown();
  }

  /**
   * delete all threads
   */
  for (unsigned i = 0; i < theThreads.size(); i++)
  {
    AsyncIoThread * thr = theThreads[i];
    delete thr;
    theThreads[i] = 0;
  }
  theThreads.clear();

  /**
   * Delete all files
   */
  for (unsigned i = 0; i < theFiles.size(); i++){
    AsyncFile* file = theFiles[i];
    delete file;
    theFiles[i] = NULL;
  }//for
  theFiles.clear();

  if (theRequestPool)
    delete theRequestPool;
}

static
bool
do_mkdir(const char * path)
{
  return NdbDir::create(path,
                        NdbDir::u_rwx() | NdbDir::g_r() | NdbDir::g_x(),
                        true /* ignore_existing */);
}

static
void
add_path(BaseString& dst, const char * add)
{
  const char * tmp = dst.c_str();
  unsigned len = dst.length();
  unsigned dslen = (unsigned)strlen(DIR_SEPARATOR);

  if (len > dslen && strcmp(tmp+(len - dslen), DIR_SEPARATOR) != 0)
    dst.append(DIR_SEPARATOR);
  dst.append(add);
}

static
bool
validate_path(BaseString & dst,
              const char * path)
{
  char buf2[PATH_MAX];
  memset(buf2, 0,sizeof(buf2));
#ifdef NDB_WIN32
  CreateDirectory(path, 0);
  char* szFilePart;
  if(!GetFullPathName(path, sizeof(buf2), buf2, &szFilePart) ||
     (GetFileAttributes(buf2) & FILE_ATTRIBUTE_READONLY))
    return false;
#else
  if (::realpath(path, buf2) == NULL ||
      ::access(buf2, W_OK) != 0)
    return false;
#endif
  dst.assign(buf2);
  add_path(dst, "");
  return true;
}

const BaseString&
Ndbfs::get_base_path(Uint32 no) const
{
  if (no < NDB_ARRAY_SIZE(m_base_path) &&
      strlen(m_base_path[no].c_str()) > 0)
  {
    jam();
    return m_base_path[no];
  }
  
  return m_base_path[FsOpenReq::BP_FS];
}

void 
Ndbfs::execREAD_CONFIG_REQ(Signal* signal)
{
  const ReadConfigReq * req = (ReadConfigReq*)signal->getDataPtr();

  Uint32 ref = req->senderRef;
  Uint32 senderData = req->senderData;

  const ndb_mgm_configuration_iterator * p = 
    m_ctx.m_config.getOwnConfigIterator();
  ndbrequire(p != 0);
  BaseString tmp;
  tmp.assfmt("ndb_%u_fs%s", getOwnNodeId(), DIR_SEPARATOR);
  m_base_path[FsOpenReq::BP_FS].assfmt("%s%s",
                                       m_ctx.m_config.fileSystemPath(),
                                       tmp.c_str());
  m_base_path[FsOpenReq::BP_BACKUP].assign(m_ctx.m_config.backupFilePath());

  const char * ddpath = 0;
  ndb_mgm_get_string_parameter(p, CFG_DB_DD_FILESYSTEM_PATH, &ddpath);

  {
    const char * datapath = ddpath;
    ndb_mgm_get_string_parameter(p, CFG_DB_DD_DATAFILE_PATH, &datapath);
    if (datapath)
    {
      /**
       * Only set BP_DD_DF if either FileSystemPathDataFiles or FileSystemPathDD
       *   is set...otherwise get_base_path(FsOpenReq::BP_DD_DF) will
       *   return BP_FS (see get_base_path)
       */
      BaseString path;
      add_path(path, datapath);
      do_mkdir(path.c_str());
      add_path(path, tmp.c_str());
      do_mkdir(path.c_str());
      if (!validate_path(m_base_path[FsOpenReq::BP_DD_DF], path.c_str()))
      {
        ERROR_SET(fatal, NDBD_EXIT_AFS_INVALIDPATH,
                  m_base_path[FsOpenReq::BP_DD_DF].c_str(),
                  "FileSystemPathDataFiles");
      }
    }
  }

  {
    const char * undopath = ddpath;
    ndb_mgm_get_string_parameter(p, CFG_DB_DD_UNDOFILE_PATH, &undopath);
    if (undopath)
    {
      /**
       * Only set BP_DD_DF if either FileSystemPathUndoFiles or FileSystemPathDD
       *   is set...otherwise get_base_path(FsOpenReq::BP_DD_UF) will
       *   return BP_FS (see get_base_path)
       */
      BaseString path;
      add_path(path, undopath);
      do_mkdir(path.c_str());
      add_path(path, tmp.c_str());
      do_mkdir(path.c_str());
      
      if (!validate_path(m_base_path[FsOpenReq::BP_DD_UF], path.c_str()))
      {
        ERROR_SET(fatal, NDBD_EXIT_AFS_INVALIDPATH,
                  m_base_path[FsOpenReq::BP_DD_UF].c_str(),
                  "FileSystemPathUndoFiles");
      }
    }
  }

  m_maxFiles = 0;
  ndb_mgm_get_int_parameter(p, CFG_DB_MAX_OPEN_FILES, &m_maxFiles);
  Uint32 noIdleFiles = 27;

  ndb_mgm_get_int_parameter(p, CFG_DB_INITIAL_OPEN_FILES, &noIdleFiles);

  {
    /**
     * each logpart keeps up to 3 logfiles open at any given time...
     *   (bound)
     * make sure noIdleFiles is atleast 4 times #logparts
     */
    Uint32 logParts = NDB_DEFAULT_LOG_PARTS;
    ndb_mgm_get_int_parameter(p, CFG_DB_NO_REDOLOG_PARTS, &logParts);
    Uint32 logfiles = 4 * logParts;
    if (noIdleFiles < logfiles)
    {
      noIdleFiles = logfiles;
    }
  }

  // Make sure at least "noIdleFiles" files can be created
  if (noIdleFiles > m_maxFiles && m_maxFiles != 0)
    m_maxFiles = noIdleFiles;

  // Create idle AsyncFiles
  for (Uint32 i = 0; i < noIdleFiles; i++)
  {
    theIdleFiles.push_back(createAsyncFile());
    AsyncIoThread * thr = createIoThread(/* bound */ true);
    if (thr)
    {
      theThreads.push_back(thr);
    }
  }

  Uint32 threadpool = 2;
  ndb_mgm_get_int_parameter(p, CFG_DB_THREAD_POOL, &threadpool);

  // Create IoThreads
  for (Uint32 i = 0; i < threadpool; i++)
  {
    AsyncIoThread * thr = createIoThread(/* bound */ false);
    if (thr)
    {
      jam();
      theThreads.push_back(thr);
    }
    else
    {
      jam();
      break;
    }
  }

  setup_wakeup();

  ReadConfigConf * conf = (ReadConfigConf*)signal->getDataPtrSend();
  conf->senderRef = reference();
  conf->senderData = senderData;
  sendSignal(ref, GSN_READ_CONFIG_CONF, signal, 
	     ReadConfigConf::SignalLength, JBB);

  // start scanning
  signal->theData[0] = NdbfsContinueB::ZSCAN_MEMORYCHANNEL_10MS_DELAY;
  sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 10, 1);
}

/* Received a restart signal.
 * Answer it like any other block
 * PR0  : StartCase
 * DR0  : StartPhase
 * DR1  : ?
 * DR2  : ?
 * DR3  : ?
 * DR4  : ?
 * DR5  : SignalKey
 */
void
Ndbfs::execSTTOR(Signal* signal)
{
  jamEntry();
  
  if(signal->theData[1] == 0){ // StartPhase 0
    jam();
    
    do_mkdir(m_base_path[FsOpenReq::BP_FS].c_str());
    
    // close all open files
    ndbrequire(theOpenFiles.size() == 0);
    
    signal->theData[3] = 255;
    sendSignal(NDBCNTR_REF, GSN_STTORRY, signal,4, JBB);
    return;
  }
  ndbrequire(0);
}

int
Ndbfs::forward( AsyncFile * file, Request* request)
{
  jam();
  AsyncIoThread* thr = file->getThread();
  if (thr) // bound
  {
    thr->dispatch(request);
  }
  else if (request->m_do_bind)
  {
    theToBoundThreads.writeChannel(request);
  }
  else
  {
    theToUnboundThreads.writeChannel(request);
  }
  return 1;
}

void 
Ndbfs::execFSOPENREQ(Signal* signal)
{
  jamEntry();
  const FsOpenReq * const fsOpenReq = (FsOpenReq *)&signal->theData[0];
  const BlockReference userRef = fsOpenReq->userReference;

  bool bound = (fsOpenReq->fileFlags & FsOpenReq::OM_THREAD_POOL) == 0;
  AsyncFile* file = getIdleFile(bound);
  ndbrequire(file != NULL);

  Uint32 userPointer = fsOpenReq->userPointer;
  
  
  SectionHandle handle(this, signal);
  SegmentedSectionPtr ptr; ptr.setNull();
  if (handle.m_cnt)
  {
    jam();
    handle.getSection(ptr, FsOpenReq::FILENAME);
  }
  file->theFileName.set(this, userRef, fsOpenReq->fileNumber, false, ptr);
  releaseSections(handle);
  
  if (fsOpenReq->fileFlags & FsOpenReq::OM_INIT)
  {
    jam();
    Uint32 cnt = 16; // 512k
    Ptr<GlobalPage> page_ptr;
    m_ctx.m_mm.alloc_pages(RT_DBTUP_PAGE, &page_ptr.i, &cnt, 1);
    if(cnt == 0)
    {
      file->m_page_ptr.setNull();
      file->m_page_cnt = 0;
      
      FsRef * const fsRef = (FsRef *)&signal->theData[0];
      fsRef->userPointer  = userPointer; 
      fsRef->setErrorCode(fsRef->errorCode, FsRef::fsErrOutOfMemory);
      fsRef->osErrorCode  = ~0; // Indicate local error
      sendSignal(userRef, GSN_FSOPENREF, signal, 3, JBB);
      return;
    }
    m_shared_page_pool.getPtr(page_ptr);
    file->set_buffer(RT_DBTUP_PAGE, page_ptr, cnt);
  } 
  else if (fsOpenReq->fileFlags & FsOpenReq::OM_WRITE_BUFFER)
  {
    jam();
    Uint32 cnt = NDB_FILE_BUFFER_SIZE / GLOBAL_PAGE_SIZE; // 256k
    Ptr<GlobalPage> page_ptr;
    m_ctx.m_mm.alloc_pages(RT_FILE_BUFFER, &page_ptr.i, &cnt, 1);
    if (cnt == 0)
    {
      jam();
      file->m_page_ptr.setNull();
      file->m_page_cnt = 0;

      FsRef * const fsRef = (FsRef *)&signal->theData[0];
      fsRef->userPointer  = userPointer;
      fsRef->setErrorCode(fsRef->errorCode, FsRef::fsErrOutOfMemory);
      fsRef->osErrorCode  = ~0; // Indicate local error
      sendSignal(userRef, GSN_FSOPENREF, signal, 3, JBB);
      return;
    }
    m_shared_page_pool.getPtr(page_ptr);
    file->set_buffer(RT_FILE_BUFFER, page_ptr, cnt);
  }
  else
  {
    ndbassert(file->m_page_ptr.isNull());
    file->m_page_ptr.setNull();
    file->m_page_cnt = 0;
  }
  
  if (getenv("NDB_TRACE_OPEN"))
    ndbout_c("open(%s) bound: %u", file->theFileName.c_str(), bound);
  
  Request* request = theRequestPool->get();
  request->action = Request::open;
  request->error = 0;
  request->set(userRef, userPointer, newId() );
  request->file = file;
  request->theTrace = signal->getTrace();
  request->par.open.flags = fsOpenReq->fileFlags;
  request->par.open.page_size = fsOpenReq->page_size;
  request->par.open.file_size = fsOpenReq->file_size_hi;
  request->par.open.file_size <<= 32;
  request->par.open.file_size |= fsOpenReq->file_size_lo;
  request->par.open.auto_sync_size = fsOpenReq->auto_sync_size;
  request->m_do_bind = bound;

  ndbrequire(forward(file, request));
}

void 
Ndbfs::execFSREMOVEREQ(Signal* signal)
{
  jamEntry();
  const FsRemoveReq * const req = (FsRemoveReq *)signal->getDataPtr();
  const BlockReference userRef = req->userReference;
  bool bound = true;
  AsyncFile* file = getIdleFile(bound);
  ndbrequire(file != NULL);

  SectionHandle handle(this, signal);
  SegmentedSectionPtr ptr; ptr.setNull();
  if(handle.m_cnt)
  {
    jam();
    handle.getSection(ptr, FsOpenReq::FILENAME);
  }

  file->theFileName.set(this, userRef, req->fileNumber, req->directory, ptr);
  releaseSections(handle);

  Uint32 version = FsOpenReq::getVersion(req->fileNumber);
  Uint32 bp = FsOpenReq::v5_getLcpNo(req->fileNumber);

  Request* request = theRequestPool->get();
  request->action = Request::rmrf;
  request->par.rmrf.directory = req->directory;
  request->par.rmrf.own_directory = req->ownDirectory;
  request->error = 0;
  request->set(userRef, req->userPointer, newId() );
  request->file = file;
  request->theTrace = signal->getTrace();
  request->m_do_bind = bound;

  if (version == 6)
  {
    ndbrequire(bp < NDB_ARRAY_SIZE(m_base_path));
    if (strlen(m_base_path[bp].c_str()) == 0)
    {
      goto ignore;
    }
  }
  
  ndbrequire(forward(file, request));
  return;
ignore:
  report(request, signal);
}

/*
 * PR0: File Pointer DR0: User reference DR1: User Pointer DR2: Flag bit 0= 1
 * remove file
 */
void 
Ndbfs::execFSCLOSEREQ(Signal * signal)
{
  jamEntry();
  const FsCloseReq * const fsCloseReq = (FsCloseReq *)&signal->theData[0];
  const BlockReference userRef = fsCloseReq->userReference;
  const Uint16 filePointer = (Uint16)fsCloseReq->filePointer;
  const UintR userPointer = fsCloseReq->userPointer; 

  AsyncFile* openFile = theOpenFiles.find(filePointer);
  if (openFile == NULL) {
    // The file was not open, send error back to sender
    jam();    
    // Initialise FsRef signal
    FsRef * const fsRef = (FsRef *)&signal->theData[0];
    fsRef->userPointer  = userPointer; 
    fsRef->setErrorCode(fsRef->errorCode, FsRef::fsErrFileDoesNotExist);
    fsRef->osErrorCode  = ~0; // Indicate local error
    sendSignal(userRef, GSN_FSCLOSEREF, signal, 3, JBB);

    g_eventLogger->warning("Trying to close unknown file!! %u", userPointer);
    g_eventLogger->warning("Dumping files");
    signal->theData[0] = 405;
    execDUMP_STATE_ORD(signal);
    return;
  }

  if (getenv("NDB_TRACE_OPEN"))
    ndbout_c("close(%s)", openFile->theFileName.c_str());

  Request *request = theRequestPool->get();
  if( fsCloseReq->getRemoveFileFlag(fsCloseReq->fileFlag) == true ) {
     jam();
     request->action = Request::closeRemove;
  } else {
     jam();
     request->action = Request::close;
  }
  request->set(userRef, fsCloseReq->userPointer, filePointer);
  request->file = openFile;
  request->error = 0;
  request->theTrace = signal->getTrace();
  request->m_do_bind = false;

  ndbrequire(forward(openFile, request));
}

void 
Ndbfs::readWriteRequest(int action, Signal * signal)
{
  Uint32 theData[25 + 2 * NDB_FS_RW_PAGES];
  memcpy(theData, signal->theData, 4 * signal->getLength());
  SectionHandle handle(this, signal);
  if (handle.m_cnt > 0)
  {
    SegmentedSectionPtr secPtr;
    ndbrequire(handle.getSection(secPtr, 0));
    ndbrequire(signal->getLength() + secPtr.sz < NDB_ARRAY_SIZE(theData));
    copy(theData + signal->getLength(), secPtr);
    releaseSections(handle);
  }

  const FsReadWriteReq * const fsRWReq = (FsReadWriteReq *)theData;
  Uint16 filePointer =  (Uint16)fsRWReq->filePointer;
  const UintR userPointer = fsRWReq->userPointer; 
  const BlockReference userRef = fsRWReq->userReference;
  const BlockNumber blockNumber = refToMain(userRef);
  const Uint32 instanceNumber = refToInstance(userRef);

  AsyncFile* openFile = theOpenFiles.find(filePointer);

  const NewVARIABLE *myBaseAddrRef =
    &getBat(blockNumber, instanceNumber)[fsRWReq->varIndex];
  UintPtr tPageSize;
  UintPtr tClusterSize;
  UintPtr tNRR;
  UintPtr tPageOffset;
  char*        tWA;
  FsRef::NdbfsErrorCodeType errorCode;

  Request *request = theRequestPool->get();
  request->error = 0;
  request->set(userRef, userPointer, filePointer);
  request->file = openFile;
  request->action = (Request::Action) action;
  request->theTrace = signal->getTrace();
  request->m_do_bind = false;

  Uint32 format = fsRWReq->getFormatFlag(fsRWReq->operationFlag);

  if (fsRWReq->numberOfPages == 0) { //Zero pages not allowed
    jam();
    errorCode = FsRef::fsErrInvalidParameters;
    goto error;
  }

  if(format != FsReadWriteReq::fsFormatGlobalPage &&
     format != FsReadWriteReq::fsFormatSharedPage)
  {     
    if (fsRWReq->varIndex >= getBatSize(blockNumber, instanceNumber)) {
      jam();// Ensure that a valid variable is used    
      errorCode = FsRef::fsErrInvalidParameters;
      goto error;
    }
    if (myBaseAddrRef == NULL) {
      jam(); // Ensure that a valid variable is used
      errorCode = FsRef::fsErrInvalidParameters;
      goto error;
    }
    if (openFile == NULL) {
      jam(); //file not open
      errorCode = FsRef::fsErrFileDoesNotExist;
      goto error;
    }
    tPageSize = pageSize(myBaseAddrRef);
    tClusterSize = myBaseAddrRef->ClusterSize;
    tNRR = myBaseAddrRef->nrr;
    tWA = (char*)myBaseAddrRef->WA;
    
    switch (format) {
      
      // List of memory and file pages pairs
    case FsReadWriteReq::fsFormatListOfPairs: { 
      jam();
      for (unsigned int i = 0; i < fsRWReq->numberOfPages; i++) {
	jam();
	const UintPtr varIndex = fsRWReq->data.listOfPair[i].varIndex;
	const UintPtr fileOffset = fsRWReq->data.listOfPair[i].fileOffset;
	if (varIndex >= tNRR) {
	  jam();
	  errorCode = FsRef::fsErrInvalidParameters;
	  goto error;
	}//if
	request->par.readWrite.pages[i].buf = &tWA[varIndex * tClusterSize];
	request->par.readWrite.pages[i].size = tPageSize;
	request->par.readWrite.pages[i].offset = (off_t)(fileOffset*tPageSize);
      }//for
      request->par.readWrite.numberOfPages = fsRWReq->numberOfPages;
      break;
    }//case
      
      // Range of memory page with one file page
    case FsReadWriteReq::fsFormatArrayOfPages: { 
      if ((fsRWReq->numberOfPages + fsRWReq->data.arrayOfPages.varIndex) > tNRR) {
        jam();
        errorCode = FsRef::fsErrInvalidParameters;
        goto error;
      }//if
      const UintPtr varIndex = fsRWReq->data.arrayOfPages.varIndex;
      const UintPtr fileOffset = fsRWReq->data.arrayOfPages.fileOffset;
      
      request->par.readWrite.pages[0].offset = (off_t)(fileOffset * tPageSize);
      request->par.readWrite.pages[0].size = tPageSize * fsRWReq->numberOfPages;
      request->par.readWrite.numberOfPages = 1;
      request->par.readWrite.pages[0].buf = &tWA[varIndex * tPageSize];
      break;
    }//case
      
      // List of memory pages followed by one file page
    case FsReadWriteReq::fsFormatListOfMemPages: { 
      
      tPageOffset = fsRWReq->data.listOfMemPages.varIndex[fsRWReq->numberOfPages];
      tPageOffset *= tPageSize;
      
      for (unsigned int i = 0; i < fsRWReq->numberOfPages; i++) {
	jam();
	UintPtr varIndex = fsRWReq->data.listOfMemPages.varIndex[i];
	
	if (varIndex >= tNRR) {
	  jam();
	  errorCode = FsRef::fsErrInvalidParameters;
	  goto error;
	}//if
	request->par.readWrite.pages[i].buf = &tWA[varIndex * tClusterSize];
	request->par.readWrite.pages[i].size = tPageSize;
	request->par.readWrite.pages[i].offset = (off_t)
          (tPageOffset + (i*tPageSize));
      }//for
      request->par.readWrite.numberOfPages = fsRWReq->numberOfPages;
      break;
      // make it a writev or readv
    }//case
      
    default: {
      jam();
      errorCode = FsRef::fsErrInvalidParameters;
      goto error;
    }//default
    }//switch
  } 
  else if (format == FsReadWriteReq::fsFormatGlobalPage)
  {
    Ptr<GlobalPage> ptr;
    m_global_page_pool.getPtr(ptr, fsRWReq->data.pageData[0]);
    request->par.readWrite.pages[0].buf = (char*)ptr.p;
    request->par.readWrite.pages[0].size = ((UintPtr)GLOBAL_PAGE_SIZE)*fsRWReq->numberOfPages;
    request->par.readWrite.pages[0].offset= ((UintPtr)GLOBAL_PAGE_SIZE)*fsRWReq->varIndex;
    request->par.readWrite.numberOfPages = 1;
  }
  else
  {
    ndbrequire(format == FsReadWriteReq::fsFormatSharedPage);
    Ptr<GlobalPage> ptr;
    m_shared_page_pool.getPtr(ptr, fsRWReq->data.pageData[0]);
    request->par.readWrite.pages[0].buf = (char*)ptr.p;
    request->par.readWrite.pages[0].size = ((UintPtr)GLOBAL_PAGE_SIZE)*fsRWReq->numberOfPages;
    request->par.readWrite.pages[0].offset= ((UintPtr)GLOBAL_PAGE_SIZE)*fsRWReq->varIndex;
    request->par.readWrite.numberOfPages = 1;
  }
  
  ndbrequire(forward(openFile, request));
  return;
  
error:
  theRequestPool->put(request);
  FsRef * const fsRef = (FsRef *)&signal->theData[0];
  fsRef->userPointer = userPointer;
  fsRef->setErrorCode(fsRef->errorCode, errorCode);
  fsRef->osErrorCode = ~0; // Indicate local error
  switch (action) {
  case Request:: write:
  case Request:: writeSync: {
    jam();
    sendSignal(userRef, GSN_FSWRITEREF, signal, 3, JBB);
    break;
  }//case
  case Request:: readPartial: 
  case Request:: read: {
    jam();
    sendSignal(userRef, GSN_FSREADREF, signal, 3, JBB);
  }//case
  }//switch
  return;
}

/*
    PR0: File Pointer , theData[0]
    DR0: User reference, theData[1]
    DR1: User Pointer,   etc.
    DR2: Flag
    DR3: Var number
    DR4: amount of pages
    DR5->: Memory Page id and File page id according to Flag
*/
void 
Ndbfs::execFSWRITEREQ(Signal* signal)
{
  jamEntry();
  const FsReadWriteReq * const fsWriteReq = (FsReadWriteReq *)&signal->theData[0];
  
  if (fsWriteReq->getSyncFlag(fsWriteReq->operationFlag) == true){
    jam();
    readWriteRequest( Request::writeSync, signal );
  } else {
    jam();
    readWriteRequest( Request::write, signal );
  }
}

/*
    PR0: File Pointer
    DR0: User reference
    DR1: User Pointer
    DR2: Flag
    DR3: Var number
    DR4: amount of pages
    DR5->: Memory Page id and File page id according to Flag
*/
void 
Ndbfs::execFSREADREQ(Signal* signal)
{
  jamEntry();
  FsReadWriteReq * req = (FsReadWriteReq *)signal->getDataPtr();
  if (FsReadWriteReq::getPartialReadFlag(req->operationFlag))
    readWriteRequest( Request::readPartial, signal );
  else
    readWriteRequest( Request::read, signal );
}

/*
 * PR0: File Pointer DR0: User reference DR1: User Pointer
 */
void
Ndbfs::execFSSYNCREQ(Signal * signal)
{
  jamEntry();
  Uint16 filePointer =  (Uint16)signal->theData[0];
  BlockReference userRef = signal->theData[1];
  const UintR userPointer = signal->theData[2]; 
  AsyncFile* openFile = theOpenFiles.find(filePointer);

  if (openFile == NULL) {
     jam(); //file not open
     FsRef * const fsRef = (FsRef *)&signal->theData[0];
     fsRef->userPointer = userPointer;
     fsRef->setErrorCode(fsRef->errorCode, FsRef::fsErrFileDoesNotExist);
     fsRef->osErrorCode = ~0; // Indicate local error
     sendSignal(userRef, GSN_FSSYNCREF, signal, 3, JBB);
     return;
  }
  
  Request *request = theRequestPool->get();
  request->error = 0;
  request->action = Request::sync;
  request->set(userRef, userPointer, filePointer);
  request->file = openFile;
  request->theTrace = signal->getTrace();
  request->m_do_bind = false;

  ndbrequire(forward(openFile,request));
}

/*
 * PR0: File Pointer DR0: User reference DR1: User Pointer
 */
void
Ndbfs::execFSSUSPENDORD(Signal * signal)
{
  jamEntry();
  Uint16 filePointer =  (Uint16)signal->theData[0];
  Uint32 millis = signal->theData[1];
  AsyncFile* openFile = theOpenFiles.find(filePointer);

  if (openFile == NULL)
  {
    jam(); //file not open
    return;
  }

  Request *request = theRequestPool->get();
  request->error = 0;
  request->action = Request::suspend;
  request->set(0, 0, filePointer);
  request->file = openFile;
  request->theTrace = signal->getTrace();
  request->par.suspend.milliseconds = millis;
  request->m_do_bind = false;

  ndbrequire(forward(openFile,request));
}

void 
Ndbfs::execFSAPPENDREQ(Signal * signal)
{
  const FsAppendReq * const fsReq = (FsAppendReq *)&signal->theData[0];
  const Uint16 filePointer =  (Uint16)fsReq->filePointer;
  const UintR userPointer = fsReq->userPointer; 
  const BlockReference userRef = fsReq->userReference;
  const BlockNumber blockNumber = refToMain(userRef);
  const Uint32 instanceNumber = refToInstance(userRef);

  FsRef::NdbfsErrorCodeType errorCode;

  AsyncFile* openFile = theOpenFiles.find(filePointer);
  const NewVARIABLE *myBaseAddrRef =
    &getBat(blockNumber, instanceNumber)[fsReq->varIndex];

  const Uint32* tWA   = (const Uint32*)myBaseAddrRef->WA;
  const Uint32  tSz   = myBaseAddrRef->nrr;
  const Uint32 offset = fsReq->offset;
  const Uint32 size   = fsReq->size;
  const Uint32 synch_flag = fsReq->synch_flag;
  Request *request = theRequestPool->get();

  if (openFile == NULL) {
    jam();
    errorCode = FsRef::fsErrFileDoesNotExist;
    goto error;
  }

  if (myBaseAddrRef == NULL) {
    jam(); // Ensure that a valid variable is used
    errorCode = FsRef::fsErrInvalidParameters;
    goto error;
  }
  
  if (fsReq->varIndex >= getBatSize(blockNumber, instanceNumber)) {
    jam();// Ensure that a valid variable is used    
    errorCode = FsRef::fsErrInvalidParameters;
    goto error;
  }
  
  if(offset + size > tSz){
    jam(); // Ensure that a valid variable is used
    errorCode = FsRef::fsErrInvalidParameters;
    goto error;
  }

  request->error = 0;
  request->set(userRef, userPointer, filePointer);
  request->file = openFile;
  request->theTrace = signal->getTrace();
  
  request->par.append.buf = (const char *)(tWA + offset);
  request->par.append.size = size << 2;

  if (!synch_flag)
    request->action = Request::append;
  else
    request->action = Request::append_synch;
  request->m_do_bind = false;
  ndbrequire(forward(openFile, request));
  return;
  
error:
  jam();
  theRequestPool->put(request);
  FsRef * const fsRef = (FsRef *)&signal->theData[0];
  fsRef->userPointer = userPointer;
  fsRef->setErrorCode(fsRef->errorCode, errorCode);
  fsRef->osErrorCode = ~0; // Indicate local error

  jam();
  sendSignal(userRef, GSN_FSAPPENDREF, signal, 3, JBB);
  return;
}

void
Ndbfs::execALLOC_MEM_REQ(Signal* signal)
{
  jamEntry();

  AllocMemReq* req = (AllocMemReq*)signal->getDataPtr();

  bool bound = true;
  AsyncFile* file = getIdleFile(bound);
  ndbrequire(file != NULL);

  Request *request = theRequestPool->get();

  request->error = 0;
  request->set(req->senderRef, req->senderData, 0);
  request->file = file;
  request->theTrace = signal->getTrace();

  request->par.alloc.ctx = &m_ctx;
  request->par.alloc.requestInfo = req->requestInfo;
  request->par.alloc.bytes = (Uint64(req->bytes_hi) << 32) + req->bytes_lo;
  request->action = Request::allocmem;
  request->m_do_bind = bound;
  ndbrequire(forward(file, request));
}

#include <signaldata/BuildIndxImpl.hpp>

void
Ndbfs::execBUILD_INDX_IMPL_REQ(Signal* signal)
{
  jamEntry();
  mt_BuildIndxReq * req = (mt_BuildIndxReq*)signal->getDataPtr();

  bool bound = true;
  AsyncFile* file = getIdleFile(bound);
  ndbrequire(file != NULL);

  Request *request = theRequestPool->get();
  request->error = 0;
  request->set(req->senderRef, req->senderData, 0);
  request->file = file;
  request->theTrace = signal->getTrace();

  Uint32 cnt = (req->buffer_size + 32768 - 1) / 32768;
  Uint32 save = cnt;
  Ptr<GlobalPage> page_ptr;
  m_ctx.m_mm.alloc_pages(RT_DBTUP_PAGE, &page_ptr.i, &cnt, cnt);
  if(cnt == 0)
  {
    file->m_page_ptr.setNull();
    file->m_page_cnt = 0;

    ndbrequire(false); // TODO
    return;
  }

  ndbrequire(cnt == save);

  m_shared_page_pool.getPtr(page_ptr);
  file->set_buffer(RT_DBTUP_PAGE, page_ptr, cnt);

  memcpy(&request->par.build.m_req, req, sizeof(* req));
  request->action = Request::buildindx;
  request->m_do_bind = bound;
  ndbrequire(forward(file, request));
}

Uint16
Ndbfs::newId()
{
  // finds a new key, eg a new filepointer
  for (int i = 1; i < SHRT_MAX; i++) 
  {
    if (theLastId == SHRT_MAX) {
      jam();
      theLastId = 1;
    } else {
      jam();
      theLastId++;
    }
      
    if(theOpenFiles.find(theLastId) == NULL) {
      jam();
      return theLastId;
    }
  }  
  ndbrequire(1 == 0);
  // The program will not reach this point
  return 0;
}

AsyncFile*
Ndbfs::createAsyncFile()
{
  // Check limit of open files
  if (m_maxFiles !=0 && theFiles.size() ==  m_maxFiles)
  {
    // Print info about all open files
    for (unsigned i = 0; i < theFiles.size(); i++){
      AsyncFile* file = theFiles[i];
      ndbout_c("%2d (0x%lx): %s", i, (long) file, file->isOpen()?"OPEN":"CLOSED");
    }
    ERROR_SET(fatal, NDBD_EXIT_AFS_MAXOPEN,""," Ndbfs::createAsyncFile");
  }

#ifdef NDB_WIN
  AsyncFile* file = new Win32AsyncFile(* this);
#else
  AsyncFile* file = new PosixAsyncFile(* this);
#endif

  if (file->init())
  {
    ERROR_SET(fatal, NDBD_EXIT_AFS_MAXOPEN,""," Ndbfs::createAsyncFile");
  }

  theFiles.push_back(file);
  return file;
}

void
Ndbfs::pushIdleFile(AsyncFile* file)
{
  assert(file->getThread() == 0);
  theIdleFiles.push_back(file);
}

AsyncIoThread*
Ndbfs::createIoThread(bool bound)
{
  AsyncIoThread* thr = new AsyncIoThread(*this, bound);
  if (thr)
  {
#ifdef VM_TRACE
    ndbout_c("NDBFS: Created new file thread %d", theThreads.size());
#endif

    struct NdbThread* thrptr = thr->doStart();
    globalEmulatorData.theConfiguration->addThread(thrptr, NdbfsThread);

    if (bound)
      m_bound_threads_cnt++;
    else
      m_unbounds_threads_cnt++;
  }

  return thr;
}

AsyncFile*
Ndbfs::getIdleFile(bool bound)
{
  AsyncFile* file = 0;
  Uint32 sz = theIdleFiles.size();
  if (sz)
  {
    file = theIdleFiles[sz - 1];
    theIdleFiles.erase(sz - 1);
  }
  else
  {
    file = createAsyncFile();
  }

  if (bound)
  {
    /**
     * Check if we should create thread
     */
    if (m_active_bound_threads_cnt == m_bound_threads_cnt)
    {
      AsyncIoThread * thr = createIoThread(true);
      if (thr)
      {
        theThreads.push_back(thr);
      }
    }
  }
  return file;
}

void
Ndbfs::cnt_active_bound(int val)
{
  Guard g(&g_active_bound_threads_mutex);
  if (val < 0)
  {
    val = -val;
    assert(m_active_bound_threads_cnt >= (Uint32)val);
    m_active_bound_threads_cnt -= val;
  }
  else
  {
    m_active_bound_threads_cnt += val;
  }
}

void
Ndbfs::report(Request * request, Signal* signal)
{
  const Uint32 orgTrace = signal->getTrace();
  signal->setTrace(request->theTrace);
  const BlockReference ref = request->theUserReference;

  if (request->file->has_buffer())
  {
    if ((request->action == Request::open && request->error) ||
        request->action == Request::close ||
        request->action == Request::closeRemove ||
        request->action == Request::buildindx)
    {
      Uint32 rg;
      Uint32 cnt;
      Ptr<GlobalPage> ptr;
      request->file->clear_buffer(rg, ptr, cnt);
      m_ctx.m_mm.release_pages(rg, ptr.i, cnt);
    }
  }
  
  if (request->error) {
    jam();
    // Initialise FsRef signal
    FsRef * const fsRef = (FsRef *)&signal->theData[0];
    fsRef->userPointer = request->theUserPointer;
    if(request->error & FsRef::FS_ERR_BIT)
    {
      fsRef->errorCode = request->error;
      fsRef->osErrorCode = 0;
    }
    else 
    {
      fsRef->setErrorCode(fsRef->errorCode, translateErrno(request->error));
      fsRef->osErrorCode = request->error; 
    }
    switch (request->action) {
    case Request:: open: {
      jam();
      // Put the file back in idle files list
      pushIdleFile(request->file);
      sendSignal(ref, GSN_FSOPENREF, signal, FsRef::SignalLength, JBB);
      break;
    }
    case Request:: closeRemove:
    case Request:: close: {
      jam();
      sendSignal(ref, GSN_FSCLOSEREF, signal, FsRef::SignalLength, JBB);

      g_eventLogger->warning("Error closing file: %s %u/%u",
                             request->file->theFileName.c_str(),
                             fsRef->errorCode,
                             fsRef->osErrorCode);
      g_eventLogger->warning("Dumping files");
      signal->theData[0] = 405;
      execDUMP_STATE_ORD(signal);
      break;
    }
    case Request:: writeSync:
    case Request:: writevSync:
    case Request:: write:
    case Request:: writev: {
      jam();
      sendSignal(ref, GSN_FSWRITEREF, signal, FsRef::SignalLength, JBB);
      break;
    }
    case Request:: read: 
    case Request:: readPartial:
    case Request:: readv: {
      jam();
      sendSignal(ref, GSN_FSREADREF, signal, FsRef::SignalLength, JBB);
      break;
    }
    case Request:: sync: {
      jam();
      sendSignal(ref, GSN_FSSYNCREF, signal, FsRef::SignalLength, JBB);
      break;
    }
    case Request::append:
    case Request::append_synch:
    {
      jam();
      sendSignal(ref, GSN_FSAPPENDREF, signal, FsRef::SignalLength, JBB);
      break;
    }
    case Request::rmrf: {
      jam();
      // Put the file back in idle files list
      pushIdleFile(request->file);
      sendSignal(ref, GSN_FSREMOVEREF, signal, FsRef::SignalLength, JBB);
      break;
    }
    
    case Request:: end: {
    case Request:: suspend:
      // Report nothing
      break;
    }
    case Request::allocmem: {
      jam();
      AllocMemRef* rep = (AllocMemRef*)signal->getDataPtrSend();
      rep->senderRef = reference();
      rep->senderData = request->theUserPointer;
      rep->errorCode = request->error;
      sendSignal(ref, GSN_ALLOC_MEM_REF, signal,
                 AllocMemRef::SignalLength, JBB);
      pushIdleFile(request->file);
      break;
    }
    case Request::buildindx: {
      jam();
      BuildIndxImplRef* rep = (BuildIndxImplRef*)signal->getDataPtrSend();
      rep->senderRef = reference();
      rep->senderData = request->theUserPointer;
      rep->errorCode = (BuildIndxImplRef::ErrorCode)request->error;
      sendSignal(ref, GSN_BUILD_INDX_IMPL_REF, signal,
                 BuildIndxImplRef::SignalLength, JBB);
      pushIdleFile(request->file);
      break;
    }
    }//switch
  } else {
    jam();
    FsConf * const fsConf = (FsConf *)&signal->theData[0];
    fsConf->userPointer = request->theUserPointer;
    switch (request->action) {
    case Request:: open: {
      jam();
      theOpenFiles.insert(request->file, request->theFilePointer);

      // Keep track on max number of opened files
      if (theOpenFiles.size() > m_maxOpenedFiles)
	m_maxOpenedFiles = theOpenFiles.size();

      fsConf->filePointer = request->theFilePointer;
      sendSignal(ref, GSN_FSOPENCONF, signal, 3, JBA);
      break;
    }
    case Request:: closeRemove:
    case Request:: close: {
      jam();
      // removes the file from OpenFiles list
      theOpenFiles.erase(request->theFilePointer); 
      // Put the file in idle files list
      pushIdleFile(request->file);
      sendSignal(ref, GSN_FSCLOSECONF, signal, 1, JBA);
      break;
    }
    case Request:: writeSync:
    case Request:: writevSync:
    case Request:: write:
    case Request:: writev: {
      jam();
      sendSignal(ref, GSN_FSWRITECONF, signal, 1, JBA);
      break;
    }
    case Request:: read:
    case Request:: readv: {
      jam();
      sendSignal(ref, GSN_FSREADCONF, signal, 1, JBA);
      break;
    }
    case Request:: readPartial: {
      jam();
      fsConf->bytes_read = Uint32(request->par.readWrite.pages[0].size);
      sendSignal(ref, GSN_FSREADCONF, signal, 2, JBA);
      break;
    }
    case Request:: sync: {
      jam();
      sendSignal(ref, GSN_FSSYNCCONF, signal, 1, JBA);
      break;
    }//case
    case Request::append:
    case Request::append_synch:
    {
      jam();
      signal->theData[1] = Uint32(request->par.append.size);
      sendSignal(ref, GSN_FSAPPENDCONF, signal, 2, JBA);
      break;
    }
    case Request::rmrf: {
      jam();
      // Put the file in idle files list
      pushIdleFile(request->file);
      sendSignal(ref, GSN_FSREMOVECONF, signal, 1, JBA);
      break;
    }
    case Request:: end: {
    case Request:: suspend:
      // Report nothing
      break;
    }
    case Request::allocmem: {
      jam();
      AllocMemConf* conf = (AllocMemConf*)signal->getDataPtrSend();
      conf->senderRef = reference();
      conf->senderData = request->theUserPointer;
      conf->bytes_hi = Uint32(request->par.alloc.bytes >> 32);
      conf->bytes_lo = Uint32(request->par.alloc.bytes);
      sendSignal(ref, GSN_ALLOC_MEM_CONF, signal,
                 AllocMemConf::SignalLength, JBB);
      pushIdleFile(request->file);
      break;
    }
    case Request::buildindx: {
      jam();
      BuildIndxImplConf* rep = (BuildIndxImplConf*)signal->getDataPtrSend();
      rep->senderRef = reference();
      rep->senderData = request->theUserPointer;
      sendSignal(ref, GSN_BUILD_INDX_IMPL_CONF, signal,
                 BuildIndxImplConf::SignalLength, JBB);
      pushIdleFile(request->file);
      break;
    }
    }    
  }//if
  signal->setTrace(orgTrace);
}


bool
Ndbfs::scanIPC(Signal* signal)
{
   Request* request = theFromThreads.tryReadChannel();
   jam();
   if (request) {
      jam();
      report(request, signal);
      theRequestPool->put(request);
      return true;
   }
   return false;
}

#if defined NDB_WIN32
Uint32 Ndbfs::translateErrno(int aErrno)
{
  switch (aErrno)
    {
      //permission denied
    case ERROR_ACCESS_DENIED:

      return FsRef::fsErrPermissionDenied;
      //temporary not accessible
    case ERROR_PATH_BUSY:
    case ERROR_NO_MORE_SEARCH_HANDLES:

      return FsRef::fsErrTemporaryNotAccessible;
      //no space left on device
    case ERROR_HANDLE_DISK_FULL:
    case ERROR_DISK_FULL:

      return FsRef::fsErrNoSpaceLeftOnDevice;
      //none valid parameters
    case ERROR_INVALID_HANDLE:
    case ERROR_INVALID_DRIVE:
    case ERROR_INVALID_ACCESS:
    case ERROR_HANDLE_EOF:
    case ERROR_BUFFER_OVERFLOW:

      return FsRef::fsErrInvalidParameters;
      //environment error
    case ERROR_CRC:
    case ERROR_ARENA_TRASHED:
    case ERROR_BAD_ENVIRONMENT:
    case ERROR_INVALID_BLOCK:
    case ERROR_WRITE_FAULT:
    case ERROR_READ_FAULT:
    case ERROR_OPEN_FAILED:

      return FsRef::fsErrEnvironmentError;

      //no more process resources
    case ERROR_TOO_MANY_OPEN_FILES:
    case ERROR_NOT_ENOUGH_MEMORY:
    case ERROR_OUTOFMEMORY:
      return FsRef::fsErrNoMoreResources;
      //no file
    case ERROR_FILE_NOT_FOUND:
      return FsRef::fsErrFileDoesNotExist;

    case ERR_ReadUnderflow:
      return FsRef::fsErrReadUnderflow;

    default:
      return FsRef::fsErrUnknown;
    }
}
#else
Uint32 Ndbfs::translateErrno(int aErrno)
{
  switch (aErrno)
    {
      //permission denied
    case EACCES:
    case EROFS:
    case ENXIO:
      return FsRef::fsErrPermissionDenied;
      //temporary not accessible
    case EAGAIN:
    case ETIMEDOUT:
    case ENOLCK:
    case EINTR:
    case EIO:
      return FsRef::fsErrTemporaryNotAccessible;
      //no space left on device
    case ENFILE:
    case EDQUOT:
#ifdef ENOSR
    case ENOSR:
#endif
    case ENOSPC:
    case EFBIG:
      return FsRef::fsErrNoSpaceLeftOnDevice;
      //none valid parameters
    case EINVAL:
    case EBADF:
    case ENAMETOOLONG:
    case EFAULT:
    case EISDIR:
    case ENOTDIR:
    case EEXIST:
    case ETXTBSY:
      return FsRef::fsErrInvalidParameters;
      //environment error
    case ELOOP:
#ifdef ENOLINK
    case ENOLINK:
#endif
#ifdef EMULTIHOP
    case EMULTIHOP:
#endif
#ifdef EOPNOTSUPP
    case EOPNOTSUPP:
#endif
#ifdef ESPIPE
    case ESPIPE:
#endif
    case EPIPE:
      return FsRef::fsErrEnvironmentError;

      //no more process resources
    case EMFILE:
    case ENOMEM:
      return FsRef::fsErrNoMoreResources;
      //no file
    case ENOENT:
      return FsRef::fsErrFileDoesNotExist;

    case ERR_ReadUnderflow:
      return FsRef::fsErrReadUnderflow;
      
    default:
      return FsRef::fsErrUnknown;
    }
}
#endif



void 
Ndbfs::execCONTINUEB(Signal* signal)
{
  jamEntry();
  if (signal->theData[0] == NdbfsContinueB::ZSCAN_MEMORYCHANNEL_10MS_DELAY) {
    jam();

    // Also send CONTINUEB to ourself in order to scan for 
    // incoming answers from AsyncFile on MemoryChannel theFromThreads
    signal->theData[0] = NdbfsContinueB::ZSCAN_MEMORYCHANNEL_10MS_DELAY;
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 10, 1);
    if (scanningInProgress == true) {
      jam();
      return;
    }
  }
  if (scanIPC(signal)) {
    jam();
    scanningInProgress = true;
    signal->theData[0] = NdbfsContinueB::ZSCAN_MEMORYCHANNEL_NO_DELAY;    
    sendSignal(reference(), GSN_CONTINUEB, signal, 1, JBB);
   } else {
    jam();
    scanningInProgress = false;
   }
   return;
}

void
Ndbfs::execSEND_PACKED(Signal* signal)
{
  jamEntry();
  if (scanningInProgress == false && scanIPC(signal))
  {
    jam();
    scanningInProgress = true;
    signal->theData[0] = NdbfsContinueB::ZSCAN_MEMORYCHANNEL_NO_DELAY;
    sendSignal(reference(), GSN_CONTINUEB, signal, 1, JBB);
  }
}

void
Ndbfs::execDUMP_STATE_ORD(Signal* signal)
{
  if(signal->theData[0] == 19){
    return;
  }
  if(signal->theData[0] == DumpStateOrd::NdbfsDumpFileStat){
    infoEvent("NDBFS: Files: %d Open files: %d",
	      theFiles.size(),
	      theOpenFiles.size());
    infoEvent(" Idle files: %u Max opened files: %d",
              theIdleFiles.size(),
              m_maxOpenedFiles);
    infoEvent(" Bound Threads: %u (active %u) Unbound threads: %u",
              m_bound_threads_cnt,
              m_active_bound_threads_cnt,
              m_unbounds_threads_cnt);
    infoEvent(" Max files: %d",
	      m_maxFiles);
    infoEvent(" Requests: %d",
	      theRequestPool->size());

    return;
  }
  if(signal->theData[0] == DumpStateOrd::NdbfsDumpOpenFiles){
    infoEvent("NDBFS: Dump open files: %d", theOpenFiles.size());
    
    for (unsigned i = 0; i < theOpenFiles.size(); i++){
      AsyncFile* file = theOpenFiles.getFile(i);
      infoEvent("%2d (0x%lx): %s thr: %lx", i,
                (long)file,
                file->theFileName.c_str(),
                (long)file->getThread());
    }
    return;
  }
  if(signal->theData[0] == DumpStateOrd::NdbfsDumpAllFiles){
    infoEvent("NDBFS: Dump all files: %d", theFiles.size());
    
    for (unsigned i = 0; i < theFiles.size(); i++){
      AsyncFile* file = theFiles[i];
      infoEvent("%2d (0x%lx): %s", i, (long)file, file->isOpen()?"OPEN":"CLOSED");
    }
    return;
  }
  if(signal->theData[0] == DumpStateOrd::NdbfsDumpIdleFiles){
    infoEvent("NDBFS: Dump idle files: %u",
              theIdleFiles.size());

    for (unsigned i = 0; i < theIdleFiles.size(); i++){
      AsyncFile* file = theIdleFiles[i];
      infoEvent("%2d (0x%lx): %s", i, (long)file, file->isOpen()?"OPEN":"CLOSED");
    }

    return;
  }

  if(signal->theData[0] == 404)
  {
#if 0
    ndbrequire(signal->getLength() == 2);
    Uint32 file= signal->theData[1];
    AsyncFile* openFile = theOpenFiles.find(file);
    ndbrequire(openFile != 0);
    ndbout_c("File: %s %p", openFile->theFileName.c_str(), openFile);
    Request* curr = openFile->m_current_request;
    Request* last = openFile->m_last_request;
    if(curr)
      ndbout << "Current request: " << *curr << endl;
    if(last)
       ndbout << "Last request: " << *last << endl;

    ndbout << "theReportTo " << *openFile->theReportTo << endl;
    ndbout << "theMemoryChannelPtr" << *openFile->theMemoryChannelPtr << endl;

    ndbout << "All files: " << endl;
    for (unsigned i = 0; i < theFiles.size(); i++){
      AsyncFile* file = theFiles[i];
      ndbout_c("%2d (0x%lx): %s", i, (long) file, file->isOpen()?"OPEN":"CLOSED");
    }
#endif
  }

  if(signal->theData[0] == 405)
  {
    for (unsigned i = 0; i < theFiles.size(); i++)
    {
      AsyncFile* file = theFiles[i];
      if (file == 0)
        continue;
      ndbout_c("%u : %s %s", i,
               file->theFileName.c_str() ? file->theFileName.c_str() : "",
               file->isOpen() ? "OPEN" : "CLOSED");
    }
  }
}//Ndbfs::execDUMP_STATE_ORD()

const char*
Ndbfs::get_filename(Uint32 fd) const
{
  jamEntry();
  const AsyncFile* openFile = theOpenFiles.find(fd);
  if(openFile)
    return openFile->theFileName.get_base_name();
  return "";
}


BLOCK_FUNCTIONS(Ndbfs)

template class Vector<AsyncFile*>;
template class Vector<AsyncIoThread*>;
template class Vector<OpenFiles::OpenFileItem>;
template class MemoryChannel<Request>;
template class Pool<Request>;
template NdbOut& operator<<(NdbOut&, const MemoryChannel<Request>&);
