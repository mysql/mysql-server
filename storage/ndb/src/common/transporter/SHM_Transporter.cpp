/*
   Copyright (c) 2003, 2014, Oracle and/or its affiliates. All rights reserved.

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

#include <InputStream.hpp>
#include <OutputStream.hpp>

extern int g_ndb_shm_signum;

SHM_Transporter::SHM_Transporter(TransporterRegistry &t_reg,
				 const char *lHostName,
				 const char *rHostName, 
				 int r_port,
				 bool isMgmConnection_arg,
				 NodeId lNodeId,
				 NodeId rNodeId,
				 NodeId serverNodeId,
				 bool checksum, 
				 bool signalId,
				 key_t _shmKey,
				 Uint32 _shmSize) :
  Transporter(t_reg, tt_SHM_TRANSPORTER,
	      lHostName, rHostName, r_port, isMgmConnection_arg,
	      lNodeId, rNodeId, serverNodeId,
	      0, false, checksum, signalId, 
              4096 + MAX_SEND_MESSAGE_BYTESIZE),
  shmKey(_shmKey),
  shmSize(_shmSize)
{
#ifndef NDB_WIN32
  shmId= 0;
#endif
  _shmSegCreated = false;
  _attached = false;

  shmBuf = 0;
  reader = 0;
  writer = 0;
  
  setupBuffersDone=false;
#ifdef DEBUG_TRANSPORTER
  printf("shm key (%d - %d) = %d\n", lNodeId, rNodeId, shmKey);
#endif
  m_signal_threshold = 4096;
}


bool
SHM_Transporter::configure_derived(const TransporterConfiguration* conf)
{
  if ((key_t)conf->shm.shmKey == shmKey &&
      (int)conf->shm.shmSize == shmSize &&
      conf->shm.signum == g_ndb_shm_signum)
    return true; // No change
  return false; // Can't reconfigure
}


SHM_Transporter::~SHM_Transporter(){
  doDisconnect();
}

bool 
SHM_Transporter::initTransporter(){
  if (g_ndb_shm_signum)
    return true;
  return false;
}
    
void
SHM_Transporter::setupBuffers(){
  Uint32 sharedSize = 0;
  sharedSize += 28; //SHM_Reader::getSharedSize();
  sharedSize += 28; //SHM_Writer::getSharedSize();

  const Uint32 slack = MAX(MAX_RECV_MESSAGE_BYTESIZE,
                           MAX_SEND_MESSAGE_BYTESIZE);

  /**
   *  NOTE: There is 7th shared variable in Win2k (sharedCountAttached).
   */
  Uint32 sizeOfBuffer = shmSize;
  sizeOfBuffer -= 2*sharedSize;
  sizeOfBuffer /= 2;

  Uint32 * base1 = (Uint32*)shmBuf;

  Uint32 * sharedReadIndex1 = base1;
  Uint32 * sharedWriteIndex1 = base1 + 1;
  serverStatusFlag = base1 + 4;
  char * startOfBuf1 = shmBuf+sharedSize;

  Uint32 * base2 = (Uint32*)(shmBuf + sizeOfBuffer + sharedSize);
  Uint32 * sharedReadIndex2 = base2;
  Uint32 * sharedWriteIndex2 = base2 + 1;
  clientStatusFlag = base2 + 4;
  char * startOfBuf2 = ((char *)base2)+sharedSize;
  
  if(isServer){
    * serverStatusFlag = 0;
    reader = new SHM_Reader(startOfBuf1, 
			    sizeOfBuffer,
			    slack,
			    sharedReadIndex1,
			    sharedWriteIndex1);

    writer = new SHM_Writer(startOfBuf2, 
			    sizeOfBuffer,
			    slack,
			    sharedReadIndex2,
			    sharedWriteIndex2);

    * sharedReadIndex1 = 0;
    * sharedWriteIndex1 = 0;

    * sharedReadIndex2 = 0;
    * sharedWriteIndex2 = 0;
    
    reader->clear();
    writer->clear();
    
    * serverStatusFlag = 1;

#ifdef DEBUG_TRANSPORTER 
    printf("-- (%d - %d) - Server -\n", localNodeId, remoteNodeId);
    printf("Reader at: %ld (%p)\n", startOfBuf1 - shmBuf, startOfBuf1);
    printf("sharedReadIndex1 at %ld (%p) = %d\n", 
	   (char*)sharedReadIndex1-shmBuf, 
	   sharedReadIndex1, *sharedReadIndex1);
    printf("sharedWriteIndex1 at %ld (%p) = %d\n", 
	   (char*)sharedWriteIndex1-shmBuf, 
	   sharedWriteIndex1, *sharedWriteIndex1);

    printf("Writer at: %ld (%p)\n", startOfBuf2 - shmBuf, startOfBuf2);
    printf("sharedReadIndex2 at %ld (%p) = %d\n", 
	   (char*)sharedReadIndex2-shmBuf, 
	   sharedReadIndex2, *sharedReadIndex2);
    printf("sharedWriteIndex2 at %ld (%p) = %d\n", 
	   (char*)sharedWriteIndex2-shmBuf, 
	   sharedWriteIndex2, *sharedWriteIndex2);

    printf("sizeOfBuffer = %d\n", sizeOfBuffer);
#endif
  } else {
    * clientStatusFlag = 0;
    reader = new SHM_Reader(startOfBuf2, 
			    sizeOfBuffer,
			    slack,
			    sharedReadIndex2,
			    sharedWriteIndex2);
    
    writer = new SHM_Writer(startOfBuf1, 
			    sizeOfBuffer,
			    slack,
			    sharedReadIndex1,
			    sharedWriteIndex1);
    
    * sharedReadIndex2 = 0;
    * sharedWriteIndex1 = 0;
    
    reader->clear();
    writer->clear();
    * clientStatusFlag = 1;
#ifdef DEBUG_TRANSPORTER
    printf("-- (%d - %d) - Client -\n", localNodeId, remoteNodeId);
    printf("Reader at: %ld (%p)\n", startOfBuf2 - shmBuf, startOfBuf2);
    printf("sharedReadIndex2 at %ld (%p) = %d\n", 
	   (char*)sharedReadIndex2-shmBuf, 
	   sharedReadIndex2, *sharedReadIndex2);
    printf("sharedWriteIndex2 at %ld (%p) = %d\n", 
	   (char*)sharedWriteIndex2-shmBuf, 
	   sharedWriteIndex2, *sharedWriteIndex2);

    printf("Writer at: %ld (%p)\n", startOfBuf1 - shmBuf, startOfBuf1);
    printf("sharedReadIndex1 at %ld (%p) = %d\n", 
	   (char*)sharedReadIndex1-shmBuf, 
	   sharedReadIndex1, *sharedReadIndex1);
    printf("sharedWriteIndex1 at %ld (%p) = %d\n", 
	   (char*)sharedWriteIndex1-shmBuf, 
	   sharedWriteIndex1, *sharedWriteIndex1);
    
    printf("sizeOfBuffer = %d\n", sizeOfBuffer);
#endif
  }
#ifdef DEBUG_TRANSPORTER
  printf("Mapping from %p to %p\n", shmBuf, shmBuf+shmSize);
#endif
}

bool
SHM_Transporter::connect_server_impl(NDB_SOCKET_TYPE sockfd)
{
  DBUG_ENTER("SHM_Transporter::connect_server_impl");
  SocketOutputStream s_output(sockfd);
  SocketInputStream s_input(sockfd);
  char buf[256];

  // Create
  if(!_shmSegCreated){
    if (!ndb_shm_create()) {
      make_error_info(buf, sizeof(buf));
      report_error(TE_SHM_UNABLE_TO_CREATE_SEGMENT, buf);
      NDB_CLOSE_SOCKET(sockfd);
      DBUG_RETURN(false);
    }
    _shmSegCreated = true;
  }

  // Attach
  if(!_attached){
    if (!ndb_shm_attach()) {
      make_error_info(buf, sizeof(buf));
      report_error(TE_SHM_UNABLE_TO_ATTACH_SEGMENT, buf);
      NDB_CLOSE_SOCKET(sockfd);
      DBUG_RETURN(false);
    }
    _attached = true;
  }

  // Send ok to client
  s_output.println("shm server 1 ok: %d", 
		   m_transporter_registry.m_shm_own_pid);
  
  // Wait for ok from client
  DBUG_PRINT("info", ("Wait for ok from client"));
  if (s_input.gets(buf, sizeof(buf)) == 0) 
  {
    NDB_CLOSE_SOCKET(sockfd);
    DBUG_RETURN(false);
  }

  if(sscanf(buf, "shm client 1 ok: %d", &m_remote_pid) != 1)
  {
    NDB_CLOSE_SOCKET(sockfd);
    DBUG_RETURN(false);
  }

  int r= connect_common(sockfd);

  if (r) {
    // Send ok to client
    s_output.println("shm server 2 ok");
    // Wait for ok from client
    if (s_input.gets(buf, 256) == 0) {
      NDB_CLOSE_SOCKET(sockfd);
      DBUG_RETURN(false);
    }
    DBUG_PRINT("info", ("Successfully connected server to node %d",
                remoteNodeId)); 
  }

  NDB_CLOSE_SOCKET(sockfd);
  DBUG_RETURN(r);
}

bool
SHM_Transporter::connect_client_impl(NDB_SOCKET_TYPE sockfd)
{
  DBUG_ENTER("SHM_Transporter::connect_client_impl");
  SocketInputStream s_input(sockfd);
  SocketOutputStream s_output(sockfd);
  char buf[256];

  // Wait for server to create and attach
  DBUG_PRINT("info", ("Wait for server to create and attach"));
  if (s_input.gets(buf, 256) == 0) {
    NDB_CLOSE_SOCKET(sockfd);
    DBUG_PRINT("error", ("Server id %d did not attach",
                remoteNodeId));
    DBUG_RETURN(false);
  }

  if(sscanf(buf, "shm server 1 ok: %d", &m_remote_pid) != 1)
  {
    NDB_CLOSE_SOCKET(sockfd);
    DBUG_RETURN(false);
  }
  
  // Create
  if(!_shmSegCreated){
    if (!ndb_shm_get()) {
      NDB_CLOSE_SOCKET(sockfd);
      DBUG_PRINT("error", ("Failed create of shm seg to node %d",
                  remoteNodeId));
      DBUG_RETURN(false);
    }
    _shmSegCreated = true;
  }

  // Attach
  if(!_attached){
    if (!ndb_shm_attach()) {
      make_error_info(buf, sizeof(buf));
      report_error(TE_SHM_UNABLE_TO_ATTACH_SEGMENT, buf);
      NDB_CLOSE_SOCKET(sockfd);
      DBUG_PRINT("error", ("Failed attach of shm seg to node %d",
                  remoteNodeId));
      DBUG_RETURN(false);
    }
    _attached = true;
  }

  // Send ok to server
  s_output.println("shm client 1 ok: %d", 
		   m_transporter_registry.m_shm_own_pid);
  
  int r= connect_common(sockfd);
  
  if (r) {
    // Wait for ok from server
    DBUG_PRINT("info", ("Wait for ok from server"));
    if (s_input.gets(buf, 256) == 0) {
      NDB_CLOSE_SOCKET(sockfd);
      DBUG_PRINT("error", ("No ok from server node %d",
                  remoteNodeId));
      DBUG_RETURN(false);
    }
    // Send ok to server
    s_output.println("shm client 2 ok");
    DBUG_PRINT("info", ("Successfully connected client to node %d",
                remoteNodeId)); 
  }

  NDB_CLOSE_SOCKET(sockfd);
  DBUG_RETURN(r);
}

bool
SHM_Transporter::connect_common(NDB_SOCKET_TYPE sockfd)
{
  if (!checkConnected()) {
    return false;
  }
  
  if(!setupBuffersDone) 
  {
    setupBuffers();
    setupBuffersDone=true;
  }

  if(setupBuffersDone) 
  {
    NdbSleep_MilliSleep(m_timeOutMillis);
    if(*serverStatusFlag == 1 && *clientStatusFlag == 1)
      return true;
  }

  DBUG_PRINT("error", ("Failed to set up buffers to node %d",
              remoteNodeId));
  return false;
}

bool
SHM_Transporter::doSend()
{
  struct iovec iov[64];
  Uint32 cnt = fetch_send_iovec_data(iov, NDB_ARRAY_SIZE(iov));

  if (cnt == 0)
  {
    return false;
  }

  Uint32 sum = 0;
  for(Uint32 i = 0; i<cnt; i++)
  {
    assert(iov[i].iov_len);
    sum += iov[i].iov_len;
  }

  int nBytesSent = writer->writev(iov, cnt);

  if (nBytesSent > 0)
  {
    kill(m_remote_pid, g_ndb_shm_signum);
    iovec_data_sent(nBytesSent);

    if (Uint32(nBytesSent) == sum && (cnt != NDB_ARRAY_SIZE(iov)))
    {
      return false;
    }
    return true;
  }

  return true;
}
