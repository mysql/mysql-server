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


#include "dba_process.hpp"

NewtonBatchProcess::NewtonBatchProcess(Ndb & ndb, NdbMutex & mutex) : 
  theNdb(ndb),
  theMutex(mutex)
{
  theThread = 0;
  startStopMutex = NdbMutex_Create();

  _running = false;
  _stop = false;
} 

NewtonBatchProcess::~NewtonBatchProcess(){
  doStop(true);

  if(theThread != 0)
    NdbThread_Destroy(&theThread);
  
  if(startStopMutex != 0)
    NdbMutex_Destroy(startStopMutex);
  startStopMutex = 0;
}

extern "C" 
void* 
runNDB_C(void * _nbp){
  NewtonBatchProcess * nbp = (NewtonBatchProcess*)_nbp;
  nbp->_running = true;
  nbp->run();  
  nbp->_running = false;
  
  /** 
   *  This sleep is to make sure that the transporter 
   *  send thread will come in and send any
   *  signal buffers that this thread may have allocated.
   *  If that doesn't happen an error will occur in OSE
   *  when trying to restore a signal buffer allocated by a thread
   *  that have been killed.
   */
  NdbSleep_MilliSleep(50);
  NdbThread_Exit(0);
  return 0;
}

void
NewtonBatchProcess::doStart(){
  NdbMutex_Lock(startStopMutex);
  if(_running && !_stop){
    NdbMutex_Unlock(startStopMutex);
    return ;
  }
  
  while(_running){
    NdbMutex_Unlock(startStopMutex);
    NdbSleep_MilliSleep(200);
    NdbMutex_Lock(startStopMutex);
  }
  
  require(!_running);
  _stop = false;
  
  if(theThread != 0)
    NdbThread_Destroy(&theThread);
  
  theThread = NdbThread_Create(runNDB_C,
			       (void**)this,
			       65535,
			       "Newton_BP",
			       NDB_THREAD_PRIO_LOWEST);
  
  NdbMutex_Unlock(startStopMutex);
}

void
NewtonBatchProcess::doStop(bool wait){
  NdbMutex_Lock(startStopMutex);
  _stop = true;

  if(wait){
    while(_running){
      NdbSleep_MilliSleep(200);
    }
  }
  NdbMutex_Unlock(startStopMutex);
}

bool
NewtonBatchProcess::isRunning() const {
  return _running;
}

bool
NewtonBatchProcess::isStopping() const {
  return _stop;
}

void
NewtonBatchProcess::run(){
  while(!_stop){
    NdbMutex_Lock(&theMutex);
    theNdb.sendPollNdb(0, 1, DBA__NBP_Force);
    NdbMutex_Unlock(&theMutex);
    NdbSleep_MilliSleep(DBA__NBP_Intervall);
  }
}
