/*
   Copyright (c) 2003, 2018, Oracle and/or its affiliates. All rights reserved.

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
SHM_Transporter::connectServer(Uint32 timeOutMillis)
{
  if (!_shmSegCreated)
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

  if (!_attached)
  {
    shmBuf = (char*)MapViewOfFile(hFileMapping, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (shmBuf == 0)
    {
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

  if (*sharedCountAttached == 2 && !setupBuffersDone)
  {
    setupBuffers();
    setupBuffersDone=true;
  }
  if (*sharedCountAttached > 2)
  {
    reportThreadError(remoteNodeId, TE_SHM_DISCONNECT); 
    return false;
  }
  
  if (setupBuffersDone)
  {
    NdbSleep_MilliSleep(timeOutMillis);
    if (*serverStatusFlag==1 && *clientStatusFlag==1)
      return true;
  }

  NdbSleep_MilliSleep(timeOutMillis);
  return false;
}

bool
SHM_Transporter::connectClient(Uint32 timeOutMillis)
{
  if (!_shmSegCreated)
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

  if (!_attached)
  {
    shmBuf = (char*)MapViewOfFile(hFileMapping, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (shmBuf == 0)
    {
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

  if (*sharedCountAttached == 2 && !setupBuffersDone)
  {
    setupBuffers();
    setupBuffersDone=true;
  }

  if (setupBuffersDone)
  {
    if (*serverStatusFlag==1 && *clientStatusFlag==1)
      return true;
  }
  NdbSleep_MilliSleep(timeOutMillis);
  return false;
}

void SHM_Transpporter::ndb_shm_destroy()
{
}

bool
SHM_Transporter::checkConnected()
{
  volatile Uint32 * sharedCountAttached = 
    (volatile Uint32*)(shmBuf + 6*sizeof(Uint32*));
  if (*sharedCountAttached != 2)
  {
    report_error(TE_SHM_DISCONNECT);
    return false;
  }
  return true;
}

void
SHM_Transporter::disconnectImpl()
{
  disconnect_socket();
  if (_attached)
  {
    volatile Uint32 * sharedCountAttached = 
      (volatile Uint32*)(shmBuf + 6*sizeof(Uint32*));
    
    --*sharedCountAttached;

    if (!UnmapViewOfFile(shmBuf))
    {
      report_error(TE_SHM_UNABLE_TO_REMOVE_SEGMENT);
      return;
    }
    
    _attached = false;
    if(!isServer && _shmSegCreated)
      _shmSegCreated = false;
  }
  
  if (_shmSegCreated)
  {
    if (!CloseHandle(hFileMapping))
    {
      report_error(TE_SHM_UNABLE_TO_REMOVE_SEGMENT);
      return;
    }
    _shmSegCreated = false;
  }
  setupBuffersDone=false;
}
