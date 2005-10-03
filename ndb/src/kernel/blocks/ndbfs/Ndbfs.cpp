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

#include <ndb_global.h>

#include "Ndbfs.hpp"
#include "AsyncFile.hpp"
#include "Filename.hpp"

#include <signaldata/FsOpenReq.hpp>
#include <signaldata/FsCloseReq.hpp>
#include <signaldata/FsReadWriteReq.hpp>
#include <signaldata/FsAppendReq.hpp>
#include <signaldata/FsRemoveReq.hpp>
#include <signaldata/FsConf.hpp>
#include <signaldata/FsRef.hpp>
#include <signaldata/NdbfsContinueB.hpp>
#include <signaldata/DumpStateOrd.hpp>

#include <RefConvert.hpp>
#include <NdbSleep.h>
#include <NdbOut.hpp>
#include <Configuration.hpp>

#define DEBUG(x) { ndbout << "FS::" << x << endl; }

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


Ndbfs::Ndbfs(const Configuration & conf) :
  SimulatedBlock(NDBFS, conf),
  scanningInProgress(false),
  theLastId(0),
  m_maxOpenedFiles(0)
{
  theFileSystemPath = conf.fileSystemPath();
  theBackupFilePath = conf.backupFilePath();

  theRequestPool = new Pool<Request>;

  const ndb_mgm_configuration_iterator * p = conf.getOwnConfigIterator();
  ndbrequire(p != 0);

  m_maxFiles = 40;
  ndb_mgm_get_int_parameter(p, CFG_DB_MAX_OPEN_FILES, &m_maxFiles);
  
  // Create idle AsyncFiles
  Uint32 noIdleFiles = m_maxFiles > 27  ? 27 : m_maxFiles ;
  for (Uint32 i = 0; i < noIdleFiles; i++){
    theIdleFiles.push_back(createAsyncFile());
  }

  BLOCK_CONSTRUCTOR(Ndbfs);

  // Set received signals
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
   // Set send signals
}

Ndbfs::~Ndbfs()
{
  // Delete all files
  // AsyncFile destuctor will take care of deleting
  // the thread it has created
  for (unsigned i = 0; i < theFiles.size(); i++){
    AsyncFile* file = theFiles[i];
    delete file; 
    theFiles[i] = NULL;
  }//for
  theFiles.clear();

  delete theRequestPool;
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
    cownref = NDBFS_REF;
    // close all open files
    ndbrequire(theOpenFiles.size() == 0);
    
    scanningInProgress = false;
    
    signal->theData[0] = NdbfsContinueB::ZSCAN_MEMORYCHANNEL_10MS_DELAY;
    sendSignalWithDelay(cownref, GSN_CONTINUEB, signal, 10, 1);

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
  file->execute(request);
  return 1;
}

void 
Ndbfs::execFSOPENREQ(Signal* signal)
{
  jamEntry();
  const FsOpenReq * const fsOpenReq = (FsOpenReq *)&signal->theData[0];
  const BlockReference userRef = fsOpenReq->userReference;
  AsyncFile* file = getIdleFile();
  ndbrequire(file != NULL);
  ndbrequire(signal->getLength() == FsOpenReq::SignalLength)
  file->theFileName.set( userRef, fsOpenReq->fileNumber);
  file->reportTo(&theFromThreads);
  
  Request* request = theRequestPool->get();
  request->action = Request::open;
  request->error = 0;
  request->par.open.flags = fsOpenReq->fileFlags;
  request->set(userRef, fsOpenReq->userPointer, newId() );
  request->file = file;
  request->theTrace = signal->getTrace();
  
  ndbrequire(forward(file, request));
}

void 
Ndbfs::execFSREMOVEREQ(Signal* signal)
{
  jamEntry();
  const FsRemoveReq * const req = (FsRemoveReq *)signal->getDataPtr();
  const BlockReference userRef = req->userReference;
  AsyncFile* file = getIdleFile();
  ndbrequire(file != NULL);

  file->theFileName.set( userRef, req->fileNumber, req->directory);
  file->reportTo(&theFromThreads);
  
  Request* request = theRequestPool->get();
  request->action = Request::rmrf;
  request->par.rmrf.directory = req->directory;
  request->par.rmrf.own_directory = req->ownDirectory;
  request->error = 0;
  request->set(userRef, req->userPointer, newId() );
  request->file = file;
  request->theTrace = signal->getTrace();
  
  ndbrequire(forward(file, request));
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
    return;
  }

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

  ndbrequire(forward(openFile, request));
}

void 
Ndbfs::readWriteRequest(int action, Signal * signal)
{
  const FsReadWriteReq * const fsRWReq = (FsReadWriteReq *)&signal->theData[0];
  Uint16 filePointer =  (Uint16)fsRWReq->filePointer;
  const UintR userPointer = fsRWReq->userPointer; 
  const BlockReference userRef = fsRWReq->userReference;
  const BlockNumber blockNumber = refToBlock(userRef);

  AsyncFile* openFile = theOpenFiles.find(filePointer);

  const NewVARIABLE *myBaseAddrRef = &getBat(blockNumber)[fsRWReq->varIndex];
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

  if (fsRWReq->numberOfPages == 0) { //Zero pages not allowed
    jam();
    errorCode = FsRef::fsErrInvalidParameters;
    goto error;
  }

  if (fsRWReq->varIndex >= getBatSize(blockNumber)) {
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
 
  switch (fsRWReq->getFormatFlag(fsRWReq->operationFlag)) {

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
      request->par.readWrite.pages[i].offset = fileOffset * tPageSize;
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
    
    request->par.readWrite.pages[0].offset = fileOffset * tPageSize;
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
      request->par.readWrite.pages[i].offset = tPageOffset + (i*tPageSize);
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
  
  ndbrequire(forward(openFile,request));
}

void 
Ndbfs::execFSAPPENDREQ(Signal * signal)
{
  const FsAppendReq * const fsReq = (FsAppendReq *)&signal->theData[0];
  const Uint16 filePointer =  (Uint16)fsReq->filePointer;
  const UintR userPointer = fsReq->userPointer; 
  const BlockReference userRef = fsReq->userReference;
  const BlockNumber blockNumber = refToBlock(userRef);

  FsRef::NdbfsErrorCodeType errorCode;

  AsyncFile* openFile = theOpenFiles.find(filePointer);
  const NewVARIABLE *myBaseAddrRef = &getBat(blockNumber)[fsReq->varIndex];

  const Uint32* tWA   = (const Uint32*)myBaseAddrRef->WA;
  const Uint32  tSz   = myBaseAddrRef->nrr;
  const Uint32 offset = fsReq->offset;
  const Uint32 size   = fsReq->size;
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
  
  if (fsReq->varIndex >= getBatSize(blockNumber)) {
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
  request->action = Request::append;
  request->theTrace = signal->getTrace();
  
  request->par.append.buf = (const char *)(tWA + offset);
  request->par.append.size = size << 2;
  
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
Ndbfs::createAsyncFile(){

  // Check limit of open files
  if (theFiles.size()+1 ==  m_maxFiles) {
    // Print info about all open files
    for (unsigned i = 0; i < theFiles.size(); i++){
      AsyncFile* file = theFiles[i];
      ndbout_c("%2d (0x%x): %s", i, file, file->isOpen()?"OPEN":"CLOSED");
    }
    ERROR_SET(fatal, NDBD_EXIT_AFS_MAXOPEN,""," Ndbfs::createAsyncFile");
  }

  AsyncFile* file = new AsyncFile;
  file->doStart(getOwnNodeId(), theFileSystemPath, theBackupFilePath);

  // Put the file in list of all files
  theFiles.push_back(file);

#ifdef VM_TRACE
  infoEvent("NDBFS: Created new file thread %d", theFiles.size());
#endif
  
  return file;
}

AsyncFile*
Ndbfs::getIdleFile(){
  AsyncFile* file;
  if (theIdleFiles.size() > 0){
    file = theIdleFiles[0];
    theIdleFiles.erase(0);
  } else {
    file = createAsyncFile();
  } 
  return file;
}



void
Ndbfs::report(Request * request, Signal* signal)
{
  const Uint32 orgTrace = signal->getTrace();
  signal->setTrace(request->theTrace);
  const BlockReference ref = request->theUserReference;
  if (request->error) {
    jam();
    // Initialise FsRef signal
    FsRef * const fsRef = (FsRef *)&signal->theData[0];
    fsRef->userPointer = request->theUserPointer;
    fsRef->setErrorCode(fsRef->errorCode, translateErrno(request->error));
    fsRef->osErrorCode = request->error; 

    switch (request->action) {
    case Request:: open: {
      jam();
      // Put the file back in idle files list
      theIdleFiles.push_back(request->file);  
      sendSignal(ref, GSN_FSOPENREF, signal, FsRef::SignalLength, JBB);
      break;
    }
    case Request:: closeRemove:
    case Request:: close: {
      jam();
      sendSignal(ref, GSN_FSCLOSEREF, signal, FsRef::SignalLength, JBB);
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
    case Request::append: {
      jam();
      sendSignal(ref, GSN_FSAPPENDREF, signal, FsRef::SignalLength, JBB);
      break;
    }
    case Request::rmrf: {
      jam();
      // Put the file back in idle files list
      theIdleFiles.push_back(request->file);  
      sendSignal(ref, GSN_FSREMOVEREF, signal, FsRef::SignalLength, JBB);
      break;
    }
    
    case Request:: end: {
      // Report nothing
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
      sendSignal(ref, GSN_FSOPENCONF, signal, 3, JBB);
      break;
    }
    case Request:: closeRemove:
    case Request:: close: {
      jam();
      // removes the file from OpenFiles list
      theOpenFiles.erase(request->theFilePointer); 
      // Put the file in idle files list
      theIdleFiles.push_back(request->file); 
      sendSignal(ref, GSN_FSCLOSECONF, signal, 1, JBB);
      break;
    }
    case Request:: writeSync:
    case Request:: writevSync:
    case Request:: write:
    case Request:: writev: {
      jam();
      sendSignal(ref, GSN_FSWRITECONF, signal, 1, JBB);
      break;
    }
    case Request:: read:
    case Request:: readv: {
      jam();
      sendSignal(ref, GSN_FSREADCONF, signal, 1, JBB);
      break;
    }
    case Request:: sync: {
      jam();
      sendSignal(ref, GSN_FSSYNCCONF, signal, 1, JBB);
      break;
    }//case
    case Request::append: {
      jam();
      signal->theData[1] = request->par.append.size;
      sendSignal(ref, GSN_FSAPPENDCONF, signal, 2, JBB);
      break;
    }
    case Request::rmrf: {
      jam();
      // Put the file in idle files list
      theIdleFiles.push_back(request->file);            
      sendSignal(ref, GSN_FSREMOVECONF, signal, 1, JBB);
      break;
    }
    case Request:: end: {
      // Report nothing
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
int Ndbfs::translateErrno(int aErrno)
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
#elif defined NDB_OSE || defined NDB_SOFTOSE
int Ndbfs::translateErrno(int aErrno)
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
      return FsRef::fsErrTemporaryNotAccessible;
      //no space left on device
    case ENFILE:
    case EDQUOT:
    case ENOSPC:
      return FsRef::fsErrNoSpaceLeftOnDevice;
      //none valid parameters
    case EINVAL:
    case EFBIG:
    case EBADF:
    case ENAMETOOLONG:
    case EFAULT:
    case EISDIR:
      return FsRef::fsErrInvalidParameters;
      //environment error
    case EMLINK:
    case ELOOP:
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
#else
int Ndbfs::translateErrno(int aErrno)
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

bool Global_useO_SYNC = false;
bool Global_useO_DIRECT = false;
bool Global_unlinkO_CREAT = false;
Uint32 Global_syncFreq = 1024 * 1024;

void
Ndbfs::execDUMP_STATE_ORD(Signal* signal)
{
  if(signal->theData[0] == 19){
    if(signal->length() > 1){
      Global_useO_SYNC = signal->theData[1];
    }
    if(signal->length() > 2){
      Global_syncFreq = signal->theData[2] * 1024 * 1024;
    }
    if(signal->length() > 3){
      Global_unlinkO_CREAT = signal->theData[3];
    }
    if(signal->length() > 4){
      Global_useO_DIRECT = signal->theData[4];
    }
    ndbout_c("useO_SYNC = %d syncFreq = %d unlinkO_CREATE = %d O_DIRECT = %d",
	     Global_useO_SYNC,
	     Global_syncFreq,
	     Global_unlinkO_CREAT,
	     Global_useO_DIRECT);
    return;
  }
  if(signal->theData[0] == DumpStateOrd::NdbfsDumpFileStat){
    infoEvent("NDBFS: Files: %d Open files: %d",
	      theFiles.size(),
	      theOpenFiles.size());
    infoEvent(" Idle files: %d Max opened files: %d",
	       theIdleFiles.size(),
	       m_maxOpenedFiles);
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
      infoEvent("%2d (0x%x): %s", i,file, file->theFileName.c_str());
    }
    return;
  }
  if(signal->theData[0] == DumpStateOrd::NdbfsDumpAllFiles){
    infoEvent("NDBFS: Dump all files: %d", theFiles.size());
    
    for (unsigned i = 0; i < theFiles.size(); i++){
      AsyncFile* file = theFiles[i];
      infoEvent("%2d (0x%x): %s", i,file, file->isOpen()?"OPEN":"CLOSED");
    }
    return;
  }
  if(signal->theData[0] == DumpStateOrd::NdbfsDumpIdleFiles){
    infoEvent("NDBFS: Dump idle files: %d", theIdleFiles.size());
    
    for (unsigned i = 0; i < theIdleFiles.size(); i++){
      AsyncFile* file = theIdleFiles[i];
      infoEvent("%2d (0x%x): %s", i,file, file->isOpen()?"OPEN":"CLOSED");
    }
    return;
  }

  if(signal->theData[0] == 404)
  {
    ndbrequire(signal->getLength() == 2);
    Uint32 file= signal->theData[1];
    AsyncFile* openFile = theOpenFiles.find(file);
    ndbrequire(openFile);
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
      ndbout_c("%2d (0x%x): %s", i,file, file->isOpen()?"OPEN":"CLOSED");
    }
  }
}//Ndbfs::execDUMP_STATE_ORD()



BLOCK_FUNCTIONS(Ndbfs)

template class Vector<AsyncFile*>;
template class Vector<OpenFiles::OpenFileItem>;
template class MemoryChannel<Request>;
template class Pool<Request>;
template NdbOut& operator<<(NdbOut&, const MemoryChannel<Request>&);
