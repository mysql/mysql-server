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

#ifndef SIMBLOCKASYNCFILESYSTEM_H
#define SIMBLOCKASYNCFILESYSTEM_H

#include <pc.hpp>
#include <SimulatedBlock.hpp>
#include "Pool.hpp"
#include "AsyncFile.hpp"
#include "OpenFiles.hpp"



// Because one NDB Signal request can result in multiple requests to
// AsyncFile one class must be made responsible to keep track
// of all out standing request and when all are finished the result
// must be reported to the sending block.


class Ndbfs : public SimulatedBlock
{
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

  bool scanningInProgress;
  Uint16 newId();

private:
  int forward(AsyncFile *file, Request* Request);
  void report(Request* request, Signal* signal);
  bool scanIPC(Signal* signal);

  // Declared but not defined
  Ndbfs(Ndbfs & );
  void operator = (Ndbfs &);
  
  // Used for uniqe number generation
  Uint16 theLastId;
  BlockReference cownref;

  // Communication from files 
  MemoryChannel<Request> theFromThreads;

  Pool<Request>* theRequestPool;

  AsyncFile* createAsyncFile();
  AsyncFile* getIdleFile();

  Vector<AsyncFile*> theFiles;     // List all created AsyncFiles
  Vector<AsyncFile*> theIdleFiles; // List of idle AsyncFiles
  OpenFiles theOpenFiles;          // List of open AsyncFiles

  BaseString theFileSystemPath;
  BaseString theBackupFilePath;
  
  // Statistics variables
  Uint32 m_maxOpenedFiles;
  
  // Limit for max number of AsyncFiles created
  Uint32 m_maxFiles;

  void readWriteRequest(  int action, Signal * signal );

  static Uint32 translateErrno(int aErrno);
};

class VoidFs : public SimulatedBlock
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

private:
  // Declared but not defined
  VoidFs(VoidFs & );
  void operator = (VoidFs &);
  
  // Used for uniqe number generation
  Uint32 c_maxFileNo;
};

#endif


