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

#ifndef SIMBLOCKASYNCFILESYSTEM_H
#define SIMBLOCKASYNCFILESYSTEM_H

#include <pc.hpp>
#include <SimulatedBlock.hpp>
#include "Pool.hpp"
#include "AsyncFile.hpp"
#include "OpenFiles.hpp"
#include <signaldata/FsOpenReq.hpp>

class AsyncIoThread;

// Because one NDB Signal request can result in multiple requests to
// AsyncFile one class must be made responsible to keep track
// of all out standing request and when all are finished the result
// must be reported to the sending block.

class Ndbfs : public SimulatedBlock
{
  friend class AsyncIoThread;
public:
  Ndbfs(Block_context&);
  virtual ~Ndbfs();
  virtual const char* get_filename(Uint32 fd) const;

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
  
  // Used for uniqe number generation
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

  Vector<AsyncIoThread*> theThreads;// List of all created threads
  Vector<AsyncFile*> theFiles;      // List all created AsyncFiles
  Vector<AsyncFile*> theIdleFiles;  // List of idle AsyncFiles
  OpenFiles theOpenFiles;           // List of open AsyncFiles

  BaseString m_base_path[FsOpenReq::BP_MAX];
  
  // Statistics variables
  Uint32 m_maxOpenedFiles;
  
  // Limit for max number of AsyncFiles created
  Uint32 m_maxFiles;

  void readWriteRequest(  int action, Signal * signal );

  static Uint32 translateErrno(int aErrno);

  Uint32 m_bound_threads_cnt;
  Uint32 m_unbounds_threads_cnt;
  Uint32 m_active_bound_threads_cnt;
  void cnt_active_bound(int val);
public:
  const BaseString& get_base_path(Uint32 no) const;
};

class VoidFs : public Ndbfs
{
public:
  VoidFs(Block_context&);
  virtual ~VoidFs();

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
  
  // Used for uniqe number generation
  Uint32 c_maxFileNo;
};

#endif


