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
#include <NdbMutex.h>
#include <ndb_localtime.h>

#include <InputStream.hpp>
#include <OutputStream.hpp>

#include <EventLogger.hpp>
extern EventLogger * g_eventLogger;

#if 0
#define DEBUG_FPRINTF(arglist) do { fprintf arglist ; } while (0)
#else
#define DEBUG_FPRINTF(a)
#endif

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
				 Uint32 _shmSize,
				 bool preSendChecksum,
                                 Uint32 _spintime,
                                 Uint32 _send_buffer_size) :
  Transporter(t_reg, tt_SHM_TRANSPORTER,
	      lHostName, rHostName, r_port, isMgmConnection_arg,
	      lNodeId, rNodeId, serverNodeId,
	      0, false, checksum, signalId,
              _send_buffer_size,
              preSendChecksum),
  m_spintime(_spintime),
  shmKey(_shmKey),
  shmSize(_shmSize)
{
#ifndef _WIN32
  shmId= 0;
#endif
  _shmSegCreated = false;
  _attached = false;

  shmBuf = 0;
  reader = 0;
  writer = 0;
  
  setupBuffersDone = false;
  m_server_locked = false;
  m_client_locked = false;
#ifdef DEBUG_TRANSPORTER
  printf("shm key (%d - %d) = %d\n", lNodeId, rNodeId, shmKey);
#endif
  m_signal_threshold = 262144;
}


bool
SHM_Transporter::configure_derived(const TransporterConfiguration* conf)
{
  if ((key_t)conf->shm.shmKey == shmKey &&
      (int)conf->shm.shmSize == shmSize)
    return true; // No change
  return false; // Can't reconfigure
}


SHM_Transporter::~SHM_Transporter()
{
  DEBUG_FPRINTF((stderr, "(%u)doDisconnect(%u), line: %d\n",
                localNodeId, remoteNodeId, __LINE__));
  doDisconnect();
}

void
SHM_Transporter::resetBuffers()
{
  assert(!isConnected());
  DEBUG_FPRINTF((stderr, "(%u)resetBuffers(%u), line: %d\n",
                localNodeId, remoteNodeId, __LINE__));
  detach_shm(true);
  send_checksum_state.init();
}

bool 
SHM_Transporter::initTransporter()
{
  return true;
}
    
bool
SHM_Transporter::setupBuffers()
{
  Uint32 sharedSize = 0;
  sharedSize += 64;
  sharedSize += sizeof(NdbMutex);

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
  serverAwakenedFlag = base1 + 5;
  serverUpFlag = base1 + 6;
  serverMutex = (NdbMutex*)(base1 + 16);
  char * startOfBuf1 = shmBuf+sharedSize;

  Uint32 * base2 = (Uint32*)(shmBuf + sizeOfBuffer + sharedSize);
  Uint32 * sharedReadIndex2 = base2;
  Uint32 * sharedWriteIndex2 = base2 + 1;
  clientStatusFlag = base2 + 4;
  clientAwakenedFlag = base2 + 5;
  clientUpFlag = base2 + 6;
  clientMutex = (NdbMutex*)(base2 + 16);
  char * startOfBuf2 = ((char *)base2)+sharedSize;

  if (isServer)
  {
    if ((NdbMutex_Init_Shared(serverMutex) != 0) ||
        (NdbMutex_Init_Shared(clientMutex) != 0))
    {
      return true;
    }
    * serverAwakenedFlag = 0;
    * clientAwakenedFlag = 0;
    * serverUpFlag = 1;
    * clientUpFlag = 0;
  }
  else
  {
    NdbMutex_Lock(serverMutex);
    * clientUpFlag = 1;
    NdbMutex_Unlock(serverMutex);
  }

  if (reader != 0)
  {
    DEBUG_FPRINTF((stderr, "(%u)reader = %p, m_shm_reader: %p (%u) LINE:%d",
                   localNodeId, reader, &m_shm_reader, remoteNodeId, __LINE__));
  }
  assert(reader == 0);
  assert(writer == 0);
  if(isServer)
  {
    * serverStatusFlag = 0;
    reader = new (&m_shm_reader)
                 SHM_Reader(startOfBuf1, 
			    sizeOfBuffer,
			    slack,
			    sharedReadIndex1,
			    sharedWriteIndex1);

    writer = new (&m_shm_writer)
                            SHM_Writer(startOfBuf2, 
			    sizeOfBuffer,
			    slack,
			    sharedReadIndex2,
			    sharedWriteIndex2);

    * sharedReadIndex1 = 0;
    * sharedWriteIndex1 = 0;

    * sharedReadIndex2 = 0;
    * sharedWriteIndex2 = 0;
    
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
  }
  else
  {
    * clientStatusFlag = 0;
    reader = new (&m_shm_reader)
                 SHM_Reader(startOfBuf2, 
			    sizeOfBuffer,
			    slack,
			    sharedReadIndex2,
			    sharedWriteIndex2);
    
    writer = new (&m_shm_writer)
                 SHM_Writer(startOfBuf1, 
			    sizeOfBuffer,
			    slack,
			    sharedReadIndex1,
			    sharedWriteIndex1);
    
    * sharedReadIndex2 = 0;
    * sharedWriteIndex1 = 0;
    
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
  return false;
}

bool
SHM_Transporter::connect_server_impl(NDB_SOCKET_TYPE sockfd)
{
  DBUG_ENTER("SHM_Transporter::connect_server_impl");
  DEBUG_FPRINTF((stderr, "(%u)connect_server_impl(%u)\n",
                 localNodeId, remoteNodeId));
  SocketOutputStream s_output(sockfd);
  SocketInputStream s_input(sockfd);

  // Create
  if (!_shmSegCreated)
  {
    if (!ndb_shm_create())
    {
      DEBUG_FPRINTF((stderr, "(%u)connect_server_impl failed LINE:%d,"
                             " to remote node %d\n",
                     localNodeId, __LINE__, remoteNodeId));
      DBUG_RETURN(false);
    }
    _shmSegCreated = true;
    DEBUG_FPRINTF((stderr, "(%u)ndb_shm_create()(%u)\n",
                   localNodeId, remoteNodeId));
  }

  // Attach
  if (!_attached)
  {
    if (!ndb_shm_attach())
    {
      DEBUG_FPRINTF((stderr, "(%u)connect_server_impl failed LINE:%d,"
                             " to remote node %d\n",
                     localNodeId, __LINE__, remoteNodeId));
      DBUG_RETURN(false);
    }
    _attached = true;
    DEBUG_FPRINTF((stderr, "(%u)ndb_shm_attach()(%u)\n",
                   localNodeId, remoteNodeId));
  }

  require(!setupBuffersDone);
  {
    DEBUG_FPRINTF((stderr, "(%u)setupBuffers(%u) Line:%d\n",
                   localNodeId, remoteNodeId, __LINE__));
    if (setupBuffers())
    {
      fprintf(stderr, "Shared memory not supported on this platform\n");
      detach_shm(false);
      DBUG_RETURN(false);
    }
    setupBuffersDone=true;
  }

  // Send ok to client
  s_output.println("shm server 1 ok: %d", 
		   m_transporter_registry.m_shm_own_pid);
  
  // Wait for ok from client
  char buf[256];
  DBUG_PRINT("info", ("Wait for ok from client"));
  if (s_input.gets(buf, sizeof(buf)) == 0) 
  {
    DEBUG_FPRINTF((stderr, "(%u)connect_server_impl failed LINE:%d,"
                           " to remote node %d\n",
                   localNodeId, __LINE__, remoteNodeId));
    detach_shm(false);
    DBUG_RETURN(false);
  }

  if (sscanf(buf, "shm client 1 ok: %d", &m_remote_pid) != 1)
  {
    DEBUG_FPRINTF((stderr, "(%u)connect_server_impl failed LINE:%d,"
                           " to remote node %d\n",
                   localNodeId, __LINE__, remoteNodeId));
    detach_shm(false);
    DBUG_RETURN(false);
  }

  DEBUG_FPRINTF((stderr, "(%u)connect_common()(%u)\n",
                 localNodeId, remoteNodeId));
  int r= connect_common(sockfd);

  if (r)
  {
    // Send ok to client
    s_output.println("shm server 2 ok");
    // Wait for ok from client
    if (s_input.gets(buf, 256) == 0)
    {
      DEBUG_FPRINTF((stderr, "(%u)connect_server_impl failed LINE:%d,"
                             " to remote node %d\n",
                     localNodeId, __LINE__, remoteNodeId));
      detach_shm(false);
      DBUG_RETURN(false);
    }
    DBUG_PRINT("info", ("Successfully connected server to node %d",
                remoteNodeId)); 
  }
  DEBUG_FPRINTF((stderr, "(%u)set_socket()(%u)\n",
                 localNodeId, remoteNodeId));
  set_socket(sockfd);
  DBUG_RETURN(r);
}

void
SHM_Transporter::set_socket(NDB_SOCKET_TYPE sockfd)
{
  set_get(sockfd, IPPROTO_TCP, TCP_NODELAY, "TCP_NODELAY", 1);
  set_get(sockfd, SOL_SOCKET, SO_KEEPALIVE, "SO_KEEPALIVE", 1);
  ndb_socket_nonblock(sockfd, true);
  get_callback_obj()->lock_transporter(remoteNodeId);
  theSocket = sockfd;
  send_checksum_state.init();
  get_callback_obj()->unlock_transporter(remoteNodeId);
}

bool
SHM_Transporter::connect_client_impl(NDB_SOCKET_TYPE sockfd)
{
  DBUG_ENTER("SHM_Transporter::connect_client_impl");
  DEBUG_FPRINTF((stderr, "(%u)connect_client_impl(%u)\n",
                 localNodeId, remoteNodeId));
  SocketInputStream s_input(sockfd);
  SocketOutputStream s_output(sockfd);
  char buf[256];

  // Wait for server to create and attach
  DBUG_PRINT("info", ("Wait for server to create and attach"));
  if (s_input.gets(buf, 256) == 0)
  {
    DEBUG_FPRINTF((stderr, "(%u)connect_client_impl failed LINE:%d,"
                           " to remote node %d\n",
                           localNodeId, __LINE__, remoteNodeId));

    DBUG_PRINT("error", ("Server id %d did not attach",
                remoteNodeId));
    DBUG_RETURN(false);
  }

  if(sscanf(buf, "shm server 1 ok: %d", &m_remote_pid) != 1)
  {
    DEBUG_FPRINTF((stderr, "(%u)connect_client_impl failed LINE:%d,"
                           " to remote node %d\n",
                           localNodeId, __LINE__, remoteNodeId));
    DBUG_RETURN(false);
  }
  
  // Create
  if(!_shmSegCreated)
  {
    if (!ndb_shm_get())
    {
      DEBUG_FPRINTF((stderr, "(%u)connect_client_impl failed LINE:%d,"
                             " to remote node %d\n",
                             localNodeId, __LINE__, remoteNodeId));
      DBUG_PRINT("error", ("Failed create of shm seg to node %d",
                  remoteNodeId));
      DBUG_RETURN(false);
    }
    _shmSegCreated = true;
    DEBUG_FPRINTF((stderr, "(%u)ndb_shm_get(%u)\n",
                   localNodeId, remoteNodeId));
  }

  // Attach
  if (!_attached)
  {
    if (!ndb_shm_attach())
    {
      DEBUG_FPRINTF((stderr, "(%u)connect_client_impl failed LINE:%d,"
                             " to remote node %d\n",
                             localNodeId, __LINE__, remoteNodeId));
      DBUG_PRINT("error", ("Failed attach of shm seg to node %d",
                  remoteNodeId));
      DBUG_RETURN(false);
    }
    _attached = true;
    DEBUG_FPRINTF((stderr, "(%u)ndb_shm_attach(%u)\n",
                   localNodeId, remoteNodeId));
  }

  require(!setupBuffersDone);
  {
    DEBUG_FPRINTF((stderr, "(%u)setupBuffers(%u) Line:%d\n",
                   localNodeId, remoteNodeId, __LINE__));
    if (setupBuffers())
    {
      fprintf(stderr, "Shared memory not supported on this platform\n");
      detach_shm(false);
      DBUG_RETURN(false);
    }
    else
    {
      setupBuffersDone=true;
    }
  }

  // Send ok to server
  s_output.println("shm client 1 ok: %d", 
		   m_transporter_registry.m_shm_own_pid);
  
  DEBUG_FPRINTF((stderr, "(%u)connect_common(%u)\n",
                 localNodeId, remoteNodeId));
  int r = connect_common(sockfd);
  
  if (r)
  {
    // Wait for ok from server
    DBUG_PRINT("info", ("Wait for ok from server"));
    if (s_input.gets(buf, 256) == 0)
    {
      DEBUG_FPRINTF((stderr, "(%u)connect_client_impl failed LINE:%d,"
                             " to remote node %d\n",
                             localNodeId, __LINE__, remoteNodeId));
      DBUG_PRINT("error", ("No ok from server node %d",
                  remoteNodeId));
      detach_shm(false);
      DBUG_RETURN(false);
    }
    // Send ok to server
    s_output.println("shm client 2 ok");
    DBUG_PRINT("info", ("Successfully connected client to node %d",
                remoteNodeId)); 
  }
  else
  {
    DEBUG_FPRINTF((stderr, "(%u)connect_client_impl failed LINE:%d,"
                           " to remote node %d\n",
                           localNodeId, __LINE__, remoteNodeId));
    detach_shm(false);
  }
  set_socket(sockfd);
  DEBUG_FPRINTF((stderr, "(%u)set_socket(%u)\n",
                 localNodeId, remoteNodeId));
  DBUG_RETURN(r);
}

bool
SHM_Transporter::connect_common(NDB_SOCKET_TYPE sockfd)
{
  if (!checkConnected())
  {
    DEBUG_FPRINTF((stderr, "(%u)checkConnected failed(%u)\n",
                   localNodeId, remoteNodeId));
    return false;
  }
  if (isServer)
  {
    DEBUG_FPRINTF((stderr, "(%u)ndb_shm_destroy(%u)\n",
                   localNodeId, remoteNodeId));
    ndb_shm_destroy();
  }

  require(setupBuffersDone);
  Uint32 waited = 0;
  while (waited < m_timeOutMillis)
  {
    if (*serverStatusFlag == 1 && *clientStatusFlag == 1)
      return true;
    NdbSleep_MilliSleep(10);
    waited += 10;
  }
  DEBUG_FPRINTF((stderr, "(%u)setupBuffers(%u) failed\n",
                 localNodeId, remoteNodeId));
  DBUG_PRINT("error", ("Failed to set up buffers to node %d",
              remoteNodeId));
  return false;
}

void
SHM_Transporter::remove_mutexes()
{
  if (ndb_socket_valid(theSocket))
  {
    NdbMutex_Deinit(serverMutex);
    NdbMutex_Deinit(clientMutex);
  }
}

void SHM_Transporter::setupBuffersUndone()
{
  if (setupBuffersDone)
  {
    NdbMutex_Lock(serverMutex);
    NdbMutex_Lock(clientMutex);
    setupBuffersDone = false;
    DEBUG_FPRINTF((stderr, "(%u)setupBuffersUndone(%u)\n",
                   localNodeId, remoteNodeId));
    NdbMutex_Unlock(serverMutex);
    NdbMutex_Unlock(clientMutex);
  }
}

void
SHM_Transporter::disconnect_socket()
{
  get_callback_obj()->lock_transporter(remoteNodeId);

  NDB_SOCKET_TYPE sock = theSocket;
  ndb_socket_invalidate(&theSocket);


  if(ndb_socket_valid(sock))
  {
    if(ndb_socket_close(sock) < 0){
      report_error(TE_ERROR_CLOSING_SOCKET);
    }
  }
  setupBuffersUndone();
  get_callback_obj()->unlock_transporter(remoteNodeId);
}

/**
 * This method is used when we need to wake up other side to
 * ensure that the messages we transported in shared memory
 * transporter is quickly handled.
 *
 * The first step is to grab a mutex from the shared memory segment,
 * next we check the status of the transporter on the other side. If
 * this transporter is asleep we will simply send 1 byte, it doesn't
 * matter what the byte value is. We set it to 0 just to ensure it
 * has defined value for potential future use.
 *
 * If we discover that the other side is awake there is no need to
 * do anything, the other side will check the shared memory before
 * it goes to sleep.
 */
void
SHM_Transporter::wakeup()
{
  Uint32 one_more_try = 5;
  char buf[1];
  int iovcnt = 1;
  struct iovec iov[1];

  lock_reverse_mutex();
  bool awake_state = handle_reverse_awake_state();
  unlock_reverse_mutex();
  if (awake_state)
  {
    return;
  }
  iov[0].iov_len = 1;
  iov[0].iov_base = &buf[0];
  buf[0] = 0;
  do
  {
    one_more_try--;
    int nBytesSent = (int)ndb_socket_writev(theSocket, iov, iovcnt);
    if (nBytesSent != 1)
    {
      if (DISCONNECT_ERRNO(ndb_socket_errno(), nBytesSent))
      {
        do_disconnect(ndb_socket_errno());
      }
    }
    else
    {
      return;
    }
  } while (one_more_try);
}

void
SHM_Transporter::doReceive()
{
  bool one_more_try;
  char buf[128];
  do
  {
    one_more_try = false;
    const int nBytesRead = (int)ndb_recv(theSocket, buf, sizeof(buf), 0);
    if (unlikely(nBytesRead <= 0))
    {
      if (DISCONNECT_ERRNO(ndb_socket_errno(), nBytesRead))
      {
        do_disconnect(ndb_socket_errno());
      }
      else
      {
        one_more_try = false;
      }
    }
    else if (unlikely(nBytesRead == sizeof(buf)))
    {
      one_more_try = true;
    }
  } while (one_more_try);
}

/**
 * The need_wakeup flag is always set except when called from
 * forceSend in mt.cpp, in this case we only send to try to
 * free up some send buffers. So there is no need to ensure
 * that the other side is awakened in this special case.
 */
bool
SHM_Transporter::doSend(bool need_wakeup)
{
  struct iovec iov[64];
  Uint32 cnt = fetch_send_iovec_data(iov, NDB_ARRAY_SIZE(iov));

  if (!setupBuffersDone)
  {
    DEBUG_FPRINTF((stderr, "(%u)doSend(%u)\n", localNodeId, remoteNodeId));
    return false;
  }
  if (cnt == 0)
  {
    /**
     * Need to handle the wakeup flag, even when there is nothing to
     * send. We can call doSend in an attempt to do an emergency send.
     * In this case we could register a pending send even with an
     * empty send buffer. So this could lead to a later doSend call
     * that have no data to send. So the idea is to delay the wakeup
     * until end of execution even if the send buffer is full in the
     * middle of executing signals.
     */
    if (need_wakeup)
    {
      wakeup();
    }
    return false;
  }

  Uint32 sum = 0;
  for(Uint32 i = 0; i<cnt; i++)
  {
    assert(iov[i].iov_len);
    sum += iov[i].iov_len;
  }

  int nBytesSent = writer->writev(iov, cnt);
#if 0
  time_t curr_time;
  tm tm_buf;
  curr_time = ::time((time_t*)NULL);
  ndb_localtime_r(&curr_time, &tm_buf);
  Uint32 minute = tm_buf.tm_min;
  Uint32 second = tm_buf.tm_sec;
  Uint64 millis = NdbTick_CurrentMillisecond();
  DEBUG_FPRINTF((stderr, "%u.%u.%llu (%u)W:writev(%u),"
                         " sent: %d, free: %u"
                         ", w_inx: %u, r_inx: %u\n",
                minute, second,
                millis % Uint64(1000),
                localNodeId, remoteNodeId, nBytesSent,
                writer->get_free_buffer(),
                writer->getWriteIndex(),
                writer->getReadIndex()));
#endif
  if (nBytesSent > 0)
  {
    iovec_data_sent(nBytesSent);
    m_bytes_sent += nBytesSent;
    sendCount++;
    sendSize += nBytesSent;
    if (sendCount >= reportFreq)
    {
      get_callback_obj()->reportSendLen(remoteNodeId, sendCount, sendSize);
      sendCount = 0;
      sendSize  = 0;
    }

    if (need_wakeup)
    {
      wakeup();
    }
    if (Uint32(nBytesSent) == sum &&
        (cnt != NDB_ARRAY_SIZE(iov)) &&
        need_wakeup)
    {
      return false;
    }
    return true;
  }
  return true;
}

/*
 * We need the extra m_client_locked and m_server_locked
 * variables to ensure that we don't unlock something
 * that was never locked. The timing of the setting up
 * of buffers and locking of mutexes isn't perfect,
 * therefore we protect those calls through these variables.
 */
void
SHM_Transporter::lock_mutex()
{
  if (setupBuffersDone)
  {
    if (isServer)
    {
      NdbMutex_Lock(serverMutex);
      m_server_locked = true;
    }
    else
    {
      NdbMutex_Lock(clientMutex);
      m_client_locked = true;
    }
  }
}

void
SHM_Transporter::unlock_mutex()
{
  if (setupBuffersDone)
  {
    if (isServer)
    {
      if (m_server_locked)
        NdbMutex_Unlock(serverMutex);
    }
    else
    {
      if (m_client_locked)
        NdbMutex_Unlock(clientMutex);
    }
  }
}

void
SHM_Transporter::lock_reverse_mutex()
{
  if (setupBuffersDone)
  {
    if (isServer)
    {
      NdbMutex_Lock(clientMutex);
      m_client_locked = true;
    }
    else
    {
      NdbMutex_Lock(serverMutex);
      m_server_locked = true;
    }
  }
}

void
SHM_Transporter::unlock_reverse_mutex()
{
  if (setupBuffersDone)
  {
    if (isServer)
    {
      if (m_client_locked)
        NdbMutex_Unlock(clientMutex);
    }
    else
    {
      if (m_server_locked)
        NdbMutex_Unlock(serverMutex);
    }
  }
}

void
SHM_Transporter::set_awake_state(Uint32 awake_state)
{
  if (setupBuffersDone)
  {
    if (isServer)
    {
      *serverStatusFlag = awake_state;
      *serverAwakenedFlag = 0;
    }
    else
    {
      *clientStatusFlag = awake_state;
      *clientAwakenedFlag = 0;
    }
  }
}

bool
SHM_Transporter::handle_reverse_awake_state()
{
  /**
   * We are sending to the other side. We need to understand if we
   * should send a wakeup byte to the other side. If we already did
   * so and the other side still hasn't woke up, we need not do it
   * again. If the other side is awake we also need not send any
   * wakeup byte.
   */
  if (setupBuffersDone)
  {
    if (isServer)
    {
      if (*clientStatusFlag == 1 || *clientAwakenedFlag == 1)
      {
        return true;
      }
      else
      {
        *clientAwakenedFlag = 1;
        return false;
      }
    }
    else
    {
      if (*serverStatusFlag == 1 || *serverAwakenedFlag == 1)
      {
        return true;
      }
      else
      {
        *serverAwakenedFlag = 1;
        return false;
      }
    }
  }
  else
  {
    return true;
  }
}

void
SHM_Transporter::updateReceivePtr(TransporterReceiveHandle& recvdata,
                                  Uint32 *ptr)
{
  Uint32 size_read = reader->updateReadPtr(ptr);
#if 0
  time_t curr_time;
  tm tm_buf;
  curr_time = ::time((time_t*)NULL);
  ndb_localtime_r(&curr_time, &tm_buf);
  Uint32 minute = tm_buf.tm_min;
  Uint32 second = tm_buf.tm_sec;
  Uint64 millis = NdbTick_CurrentMillisecond();
  DEBUG_FPRINTF((stderr, "%u.%u.%llu (%u)updateReadPtr(%u),"
                         " sz_read: %u, r_inx: %u"
                         ", w_inx: %u\n",
                 minute, second,
                 millis % Uint64(1000),
                 localNodeId, remoteNodeId,
                 size_read,
                 reader->getReadIndex(),
                 reader->getWriteIndex()));
#endif
  receiveCount++;
  receiveSize += size_read;
  m_bytes_received += size_read;
  if (receiveCount == reportFreq)
  {
    recvdata.reportReceiveLen(remoteNodeId,
                              receiveCount,
                              receiveSize);
    receiveCount = 0;
    receiveSize = 0;
  }
}

/**
 * send_is_possible is only called in situations with high load.
 * So it is not critical to use the mutex protection here.
 */
bool
SHM_Transporter::send_is_possible(int timeout_millisec) const
{
  do
  {
    if (setupBuffersDone)
    {
      if (writer->get_free_buffer() > MAX_SEND_MESSAGE_BYTESIZE)
      {
        return true;
      }
      if (timeout_millisec > 0)
      {
        DEBUG_FPRINTF((stderr, "send_is_possible, wait 10ms\n"));
        NdbSleep_MilliSleep(timeout_millisec);
        timeout_millisec = 0;
      }
      DEBUG_FPRINTF((stderr, "send_is_possible, timed out\n"));
      return false;
    }
    else
    {
      break;
    }
  } while (1);
  return true;
}
