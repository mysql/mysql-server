/*
   Copyright (C) 2003-2006, 2008 MySQL AB
    All rights reserved. Use is subject to license terms.

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

#include "SHM_Transporter.hpp"
#include "TransporterInternalDefinitions.hpp"
#include <TransporterCallback.hpp>
#include <NdbSleep.h>
#include <NdbOut.hpp>

#include <windows.h>


void SHM_Transporter::make_error_info(char info[], int sz)
{
  snprintf(info,sz,"Shm key=%d sz=%d",
	   shmKey, shmSize);
}

bool
SHM_Transporter::connectServer(Uint32 timeOutMillis){
  if(!_shmSegCreated)
  {
    char szName[32];
    sprintf(szName, "ndb%lu", shmKey);
    hFileMapping = CreateFileMapping(INVALID_HANDLE_VALUE, 
				     0, 
				     PAGE_READWRITE, 
				     0, 
				     shmSize, 
				     szName);

    if(!hFileMapping)
    {
      reportThreadError(remoteNodeId, TE_SHM_UNABLE_TO_CREATE_SEGMENT);
      NdbSleep_MilliSleep(timeOutMillis);
      return false;
    }
    _shmSegCreated = true;
  }

  if(!_attached){
    shmBuf = (char*)MapViewOfFile(hFileMapping, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if(shmBuf == 0){
      reportThreadError(remoteNodeId, TE_SHM_UNABLE_TO_ATTACH_SEGMENT);
      NdbSleep_MilliSleep(timeOutMillis);
      return false;
    }
    volatile Uint32 * sharedCountAttached = 
      (volatile Uint32*)(shmBuf + 6*sizeof(Uint32*));
    ++*sharedCountAttached;
    _attached = true;
  }

  volatile Uint32 * sharedCountAttached = 
    (volatile Uint32*)(shmBuf + 6*sizeof(Uint32*));

  if(*sharedCountAttached == 2 && !setupBuffersDone) {
    setupBuffers();
    setupBuffersDone=true;
  }
  if(*sharedCountAttached > 2) {
    reportThreadError(remoteNodeId, TE_SHM_DISCONNECT); 
    return false;
  }
  
  if(setupBuffersDone) {
    NdbSleep_MilliSleep(timeOutMillis);
    if(*serverStatusFlag==1 && *clientStatusFlag==1)
      return true;
  }

  NdbSleep_MilliSleep(timeOutMillis);
  return false;
}

bool
SHM_Transporter::connectClient(Uint32 timeOutMillis){
  if(!_shmSegCreated)
  {
    char szName[32];
    sprintf(szName, "ndb%lu", shmKey);
    hFileMapping = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, szName);

    if(!hFileMapping)
    {
      NdbSleep_MilliSleep(timeOutMillis);
      return false;
    }
    _shmSegCreated = true;
  }

  if(!_attached){
    shmBuf = (char*)MapViewOfFile(hFileMapping, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if(shmBuf == 0){
      reportThreadError(remoteNodeId, TE_SHM_UNABLE_TO_ATTACH_SEGMENT);
      NdbSleep_MilliSleep(timeOutMillis);
      return false;
    }
    volatile Uint32 * sharedCountAttached = 
      (volatile Uint32*)(shmBuf + 6*sizeof(Uint32*));
    ++*sharedCountAttached;
    _attached = true;
  }
  
  volatile Uint32 * sharedCountAttached = 
    (volatile Uint32*)(shmBuf + 6*sizeof(Uint32*));

  if(*sharedCountAttached == 2 && !setupBuffersDone) {
    setupBuffers();
    setupBuffersDone=true;
  }

  if(setupBuffersDone) {
    if(*serverStatusFlag==1 && *clientStatusFlag==1)
      return true;
  }
  NdbSleep_MilliSleep(timeOutMillis);
  return false;

}


bool
SHM_Transporter::checkConnected(){
  volatile Uint32 * sharedCountAttached = 
    (volatile Uint32*)(shmBuf + 6*sizeof(Uint32*));
  if(*sharedCountAttached != 2) {
    report_error(TE_SHM_DISCONNECT);
    return false;
  }
  return true;
}

void
SHM_Transporter::disconnectImpl(){
  if(_attached) {
    volatile Uint32 * sharedCountAttached = 
      (volatile Uint32*)(shmBuf + 6*sizeof(Uint32*));
    
    --*sharedCountAttached;

    if(!UnmapViewOfFile(shmBuf)) {
      report_error(TE_SHM_UNABLE_TO_REMOVE_SEGMENT);
      return;
    }
    
    _attached = false;
    if(!isServer && _shmSegCreated)
      _shmSegCreated = false;
  }
  
  if(_shmSegCreated){
    if(!CloseHandle(hFileMapping)) {
      report_error(TE_SHM_UNABLE_TO_REMOVE_SEGMENT);
      return;
    }
    _shmSegCreated = false;
  }
  setupBuffersDone=false;

}

