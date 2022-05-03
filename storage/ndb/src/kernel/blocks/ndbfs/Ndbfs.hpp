/*
   Copyright (c) 2003, 2022, Oracle and/or its affiliates.

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

#ifndef SIMBLOCKASYNCFILESYSTEM_H
#define SIMBLOCKASYNCFILESYSTEM_H

#include <pc.hpp>
#include <SimulatedBlock.hpp>
#include "Pool.hpp"
#include "AsyncFile.hpp"
#include "OpenFiles.hpp"
#include <signaldata/FsOpenReq.hpp>

#define JAM_FILE_ID 385

class AsyncIoThread;
class FsReadWriteReq;
struct FsRef;

// Because one NDB Signal request can result in multiple requests to
// AsyncFile one class must be made responsible to keep track
// of all out standing request and when all are finished the result
// must be reported to the sending block.

class Ndbfs : public SimulatedBlock
{
  friend class AsyncIoThread;
public:
  Ndbfs(Block_context&);
  ~Ndbfs() override;
  const char* get_filename(Uint32 fd) const override;

  static Uint32 translateErrno(int aErrno);

  void callFSWRITEREQ(BlockReference ref, FsReadWriteReq* req) const;
protected:
  BLOCK_DEFINES(Ndbfs);

  // The signal processing functions
  void execREAD_CONFIG_REQ(Signal* signal);
  void execDUMP_STATE_ORD(Signal* signal);
  void execFSOPENREQ(Signal* signal);
  void execFSCLOSEREQ(Signal* signal);
  void execFSWRITEREQ(Signal* signal);
  void execFSREADREQ(Signal* signal);
  void execFSSYNCREQ(Signal* signal);
  void execFSAPPENDREQ(Signal* signal);
  void execFSREMOVEREQ(Signal* signal);
  void execSTTOR(Signal* signal);
  void execCONTINUEB(Signal* signal);
  void execALLOC_MEM_REQ(Signal* signal);
  void execSEND_PACKED(Signal*);
  void execBUILD_INDX_IMPL_REQ(Signal* signal);
  void execFSSUSPENDORD(Signal*);

  Uint16 newId();

private:
  int forward(AsyncFile *file, Request* Request);
  void report(Request* request, Signal* signal);
protected:
  bool scanIPC(Signal* signal);
  bool scanningInProgress;

private:
  // Declared but not defined
  Ndbfs(Ndbfs & );
  void operator = (Ndbfs &);
  
  // Used for unique number generation
  Uint16 theLastId;

  // Communication from/to files
  MemoryChannel<Request> theFromThreads;
  MemoryChannel<Request> theToBoundThreads;
  MemoryChannel<Request> theToUnboundThreads;

  Pool<Request>* theRequestPool;

  AsyncIoThread* createIoThread(bool bound);
  AsyncFile* createAsyncFile();
  AsyncFile* getIdleFile(bool bound);
  void pushIdleFile(AsyncFile*);
  void log_file_error(GlobalSignalNumber gsn, AsyncFile* file,
                      Request* request, FsRef* fsRef);

  Vector<AsyncIoThread*> theThreads;// List of all created threads
  Vector<AsyncFile*> theFiles;      // List all created AsyncFiles
  Vector<AsyncFile*> theIdleFiles;  // List of idle AsyncFiles
  OpenFiles theOpenFiles;           // List of open AsyncFiles

  BaseString m_base_path[FsOpenReq::BP_MAX];
  
  // Statistics variables
  Uint32 m_maxOpenedFiles;
  
  // Limit for max number of AsyncFiles created
  Uint32 m_maxFiles;

// Temporary work-around for Bug #18055285 LOTS OF TESTS FAILS IN CLUB MADNESS WITH NEW GCC 4.8.2 -O3
// disabling optimization for readWriteRequest() from gcc 4.8 and up
#if (__GNUC__ * 1000 + __GNUC_MINOR__) >= 4008
  void readWriteRequest(  int action, Signal * signal ) MY_ATTRIBUTE((optimize(0)));
#else
  void readWriteRequest(  int action, Signal * signal );
#endif

  Uint32 m_bound_threads_cnt;
  Uint32 m_unbounds_threads_cnt;

  /**
   * Maintaining active bound threads count in NdbFS from *before* a
   * request is queued (getIdleFile()) until *after* it is
   * completed (pushIdleFile()) means that idle threads which have
   * completed requests are not available for further requests until
   * NdbFS has received and processed their responses.

   * This lag should be fairly short as a wakeup socket is used to
   * wake NdbFs when a response is available.

   * Any existing code relying on threads more 'quickly' becoming
   * available for new requests is timing based and therefore likely
   * to be unreliable.
   */
  Uint32 m_active_bound_threads_cnt;

public:
  const BaseString& get_base_path(Uint32 no) const;
};

class VoidFs : public Ndbfs
{
public:
  VoidFs(Block_context&);
  ~VoidFs() override;

protected:
  BLOCK_DEFINES(VoidFs);

  // The signal processing functions
  void execREAD_CONFIG_REQ(Signal* signal);
  void execDUMP_STATE_ORD(Signal* signal);
  void execFSOPENREQ(Signal* signal);
  void execFSCLOSEREQ(Signal* signal);
  void execFSWRITEREQ(Signal* signal);
  void execFSREADREQ(Signal* signal);
  void execFSSYNCREQ(Signal* signal);
  void execFSAPPENDREQ(Signal* signal);
  void execFSREMOVEREQ(Signal* signal);
  void execSTTOR(Signal* signal);
  void execALLOC_MEM_REQ(Signal*);
  void execSEND_PACKED(Signal*);
  void execFSSUSPENDORD(Signal*);

private:
  // Declared but not defined
  VoidFs(VoidFs & );
  void operator = (VoidFs &);
  
  // Used for unique number generation
  Uint32 c_maxFileNo;
};


#undef JAM_FILE_ID

#endif


