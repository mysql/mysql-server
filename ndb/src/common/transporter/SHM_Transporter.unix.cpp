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

#include "SHM_Transporter.hpp"
#include "TransporterInternalDefinitions.hpp"
#include <TransporterCallback.hpp>
#include <NdbSleep.h>
#include <NdbOut.hpp>

#include <InputStream.hpp>
#include <OutputStream.hpp>

#include <sys/ipc.h>
#include <sys/shm.h>

bool
SHM_Transporter::connect_server_impl(NDB_SOCKET_TYPE sockfd)
{
  SocketOutputStream s_output(sockfd);
  SocketInputStream s_input(sockfd);

  if(!_shmSegCreated){
    shmId = shmget(shmKey, shmSize, IPC_CREAT | 960);
    if(shmId == -1){
      perror("shmget: ");
      report_error(TE_SHM_UNABLE_TO_CREATE_SEGMENT);
      NdbSleep_MilliSleep(m_timeOutMillis);
      NDB_CLOSE_SOCKET(sockfd);
      return false;
    }
    _shmSegCreated = true;
  }

  s_output.println("shm server 1 ok");

  char buf[256];
  if (s_input.gets(buf, 256) == 0) {
    NDB_CLOSE_SOCKET(sockfd);
    return false;
  }

  int r= connect_common(sockfd);

  if (r) {
    s_output.println("shm server 2 ok");
    if (s_input.gets(buf, 256) == 0) {
      NDB_CLOSE_SOCKET(sockfd);
      return false;
    }
  }

  NDB_CLOSE_SOCKET(sockfd);
  return r;
}

bool
SHM_Transporter::connect_client_impl(NDB_SOCKET_TYPE sockfd)
{
  SocketInputStream s_input(sockfd);
  SocketOutputStream s_output(sockfd);

  char buf[256];
  if (s_input.gets(buf, 256) == 0) {
    NDB_CLOSE_SOCKET(sockfd);
    return false;
  }

  if(!_shmSegCreated){
    shmId = shmget(shmKey, shmSize, 0);
    if(shmId == -1){
      NdbSleep_MilliSleep(m_timeOutMillis);
      NDB_CLOSE_SOCKET(sockfd);
      return false;
    }
    _shmSegCreated = true;
  }

  s_output.println("shm client 1 ok");

  int r= connect_common(sockfd);

  if (r) {
    if (s_input.gets(buf, 256) == 0) {
      NDB_CLOSE_SOCKET(sockfd);
      return false;
    }
    s_output.println("shm client 2 ok");
  }

  NDB_CLOSE_SOCKET(sockfd);
  return r;
}

bool
SHM_Transporter::connect_common(NDB_SOCKET_TYPE sockfd)
{
  if(!_attached){
    shmBuf = (char *)shmat(shmId, 0, 0);
    if(shmBuf == 0){
      report_error(TE_SHM_UNABLE_TO_ATTACH_SEGMENT);
      NdbSleep_MilliSleep(m_timeOutMillis);
      return false;
    }
    _attached = true;
  }

  struct shmid_ds info;

  const int res = shmctl(shmId, IPC_STAT, &info);
  if(res == -1){
    report_error(TE_SHM_IPC_STAT);
    NdbSleep_MilliSleep(m_timeOutMillis);
    return false;
  }
  

  if(info.shm_nattch == 2 && !setupBuffersDone) {
    setupBuffers();
    setupBuffersDone=true;
  }

  if(setupBuffersDone) {
    NdbSleep_MilliSleep(m_timeOutMillis);
    if(*serverStatusFlag==1 && *clientStatusFlag==1)
      return true;
  }

  if(info.shm_nattch > 2){
    report_error(TE_SHM_DISCONNECT);
    NdbSleep_MilliSleep(m_timeOutMillis);
    return false;
  }

  NdbSleep_MilliSleep(m_timeOutMillis);
  return false;
}

bool
SHM_Transporter::checkConnected(){
  struct shmid_ds info;
  const int res = shmctl(shmId, IPC_STAT, &info);
  if(res == -1){
    report_error(TE_SHM_IPC_STAT);
    return false;
  }
 
  if(info.shm_nattch != 2){
    report_error(TE_SHM_DISCONNECT);
    return false;
  }
  return true;
}

void
SHM_Transporter::disconnectImpl(){
  if(_attached){
    const int res = shmdt(shmBuf);
    if(res == -1){
      perror("shmdelete: ");
      return;   
    }
    _attached = false;
    if(!isServer && _shmSegCreated)
      _shmSegCreated = false;
  }
  
  if(isServer && _shmSegCreated){
    const int res = shmctl(shmId, IPC_RMID, 0);
    if(res == -1){
      report_error(TE_SHM_UNABLE_TO_REMOVE_SEGMENT);
      return;
    }
    _shmSegCreated = false;
  }
  setupBuffersDone=false;
}

