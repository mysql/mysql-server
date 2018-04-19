/*
   Copyright (c) 2003, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include <NdbTCP.h>
#include "TCP_Transporter.hpp"
#include <NdbOut.hpp>
#include <NdbSleep.h>

#include <EventLogger.hpp>
extern EventLogger * g_eventLogger;
// End of stuff to be moved

#ifdef DEBUG_TRANSPORTER
#ifdef _WIN32
class ndbstrerror
{
public:
  ndbstrerror(int iError);
  ~ndbstrerror(void);
  operator char*(void) { return m_szError; };

private:
  int m_iError;
  char* m_szError;
};

ndbstrerror::ndbstrerror(int iError)
: m_iError(iError)
{
  FormatMessage( 
    FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
    0,
    iError,
    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
    (LPTSTR)&m_szError,
    0,
    0);
}

ndbstrerror::~ndbstrerror(void)
{
  LocalFree( m_szError );
  m_szError = 0;
}
#else
#define ndbstrerror strerror
#endif
#endif

static
void
setIf(int& ref, Uint32 val, Uint32 def)
{
  if (val)
    ref = val;
  else
    ref = def;
}


static
Uint32 overload_limit(const TransporterConfiguration* conf)
{
  return (conf->tcp.tcpOverloadLimit ?
          conf->tcp.tcpOverloadLimit :
          conf->tcp.sendBufferSize*4/5);
}


TCP_Transporter::TCP_Transporter(TransporterRegistry &t_reg,
				 const TransporterConfiguration* conf)
  :
  Transporter(t_reg, tt_TCP_TRANSPORTER,
	      conf->localHostName,
	      conf->remoteHostName,
	      conf->s_port,
	      conf->isMgmConnection,
	      conf->localNodeId,
	      conf->remoteNodeId,
	      conf->serverNodeId,
	      0, false, 
	      conf->checksum,
	      conf->signalId,
	      conf->tcp.sendBufferSize,
	      conf->preSendChecksum),
  reportFreq(4096),
  receiveCount(0), receiveSize(0),
  sendCount(0), sendSize(0), 
  receiveBuffer()
{
  maxReceiveSize = conf->tcp.maxReceiveSize;
  
  // Initialize member variables
  ndb_socket_invalidate(&theSocket);

  sockOptNodelay    = 1;
  setIf(sockOptRcvBufSize, conf->tcp.tcpRcvBufSize, 0);
  setIf(sockOptSndBufSize, conf->tcp.tcpSndBufSize, 0);
  setIf(sockOptTcpMaxSeg, conf->tcp.tcpMaxsegSize, 0);

  m_overload_limit = overload_limit(conf);
  /**
   * Always set slowdown limit to 60% of overload limit
   */
  m_slowdown_limit = m_overload_limit * 6 / 10;

  send_checksum_state.init();
}


bool
TCP_Transporter::configure_derived(const TransporterConfiguration* conf)
{
  if (conf->tcp.sendBufferSize == m_max_send_buffer &&
      conf->tcp.maxReceiveSize == maxReceiveSize &&
      (int)conf->tcp.tcpSndBufSize == sockOptSndBufSize &&
      (int)conf->tcp.tcpRcvBufSize == sockOptRcvBufSize &&
      (int)conf->tcp.tcpMaxsegSize == sockOptTcpMaxSeg &&
      overload_limit(conf) == m_overload_limit)
    return true; // No change

  return false; // Can't reconfigure
}


TCP_Transporter::~TCP_Transporter() {
  
  // Disconnect
  if (ndb_socket_valid(theSocket))
    doDisconnect();
  
  // Delete receive buffer!!
  assert(!isConnected());
  receiveBuffer.destroy();
}

void
TCP_Transporter::resetBuffers()
{
  assert(!isConnected());
  receiveBuffer.clear();
  send_checksum_state.init();
}

bool TCP_Transporter::connect_server_impl(NDB_SOCKET_TYPE sockfd)
{
  DBUG_ENTER("TCP_Transpporter::connect_server_impl");
  DBUG_RETURN(connect_common(sockfd));
}

bool TCP_Transporter::connect_client_impl(NDB_SOCKET_TYPE sockfd)
{
  DBUG_ENTER("TCP_Transpporter::connect_client_impl");
  DBUG_RETURN(connect_common(sockfd));
}

bool TCP_Transporter::connect_common(NDB_SOCKET_TYPE sockfd)
{
  setSocketOptions(sockfd);
  setSocketNonBlocking(sockfd);

  get_callback_obj()->lock_transporter(remoteNodeId);
  theSocket = sockfd;
  send_checksum_state.init();
  get_callback_obj()->unlock_transporter(remoteNodeId);

  DBUG_PRINT("info", ("Successfully set-up TCP transporter to node %d",
              remoteNodeId));
  return true;
}

bool
TCP_Transporter::initTransporter() {
  
  // Allocate buffer for receiving
  // Let it be the maximum size we receive plus 8 kB for any earlier received
  // incomplete messages (slack)
  Uint32 recBufSize = maxReceiveSize;
  if(recBufSize < MAX_RECV_MESSAGE_BYTESIZE){
    recBufSize = MAX_RECV_MESSAGE_BYTESIZE;
  }
  
  if(!receiveBuffer.init(recBufSize+MAX_RECV_MESSAGE_BYTESIZE)){
    return false;
  }
  
  send_checksum_state.init();
  return true;
}

static
void
set_get(NDB_SOCKET_TYPE fd, int level, int optval, const char *optname, 
	int val)
{
  int actual = 0, defval = 0;
  ndb_socket_len_t len = sizeof(actual);

  ndb_getsockopt(fd, level, optval, (char*)&defval, &len);

  if (ndb_setsockopt(fd, level, optval,
                    (char*)&val, sizeof(val)) < 0)
  {
#ifdef DEBUG_TRANSPORTER
    g_eventLogger->error("setsockopt(%s, %d) errno: %d %s",
                         optname, val, errno, strerror(errno));
#endif
  }
  
  len = sizeof(actual);
  if ((ndb_getsockopt(fd, level, optval,
                     (char*)&actual, &len) == 0) &&
      actual != val)
  {
#ifdef DEBUG_TRANSPORTER
    g_eventLogger->error("setsockopt(%s, %d) - actual %d default: %d",
                         optname, val, actual, defval);
#endif
  }
}

int
TCP_Transporter::pre_connect_options(NDB_SOCKET_TYPE sockfd)
{
  if (sockOptTcpMaxSeg)
  {
#ifdef TCP_MAXSEG
    set_get(sockfd, IPPROTO_TCP, TCP_MAXSEG, "TCP_MAXSEG", sockOptTcpMaxSeg);
#endif
  }
  return 0;
}

void
TCP_Transporter::setSocketOptions(NDB_SOCKET_TYPE socket)
{
  if (sockOptRcvBufSize)
  {
    set_get(socket, SOL_SOCKET, SO_RCVBUF, "SO_RCVBUF", sockOptRcvBufSize);
  }
  if (sockOptSndBufSize)
  {
    set_get(socket, SOL_SOCKET, SO_SNDBUF, "SO_SNDBUF", sockOptSndBufSize);
  }

  set_get(socket, IPPROTO_TCP, TCP_NODELAY, "TCP_NODELAY", sockOptNodelay);
  set_get(socket, SOL_SOCKET, SO_KEEPALIVE, "SO_KEEPALIVE", 1);

  if (sockOptTcpMaxSeg)
  {
#ifdef TCP_MAXSEG
    set_get(socket, IPPROTO_TCP, TCP_MAXSEG, "TCP_MAXSEG", sockOptTcpMaxSeg);
#endif
  }
}

bool TCP_Transporter::setSocketNonBlocking(NDB_SOCKET_TYPE socket)
{
  if(ndb_socket_nonblock(socket, true)==0)
    return true;
  return false;
}

bool
TCP_Transporter::send_is_possible(int timeout_millisec) const
{
  return send_is_possible(theSocket, timeout_millisec);
}

bool
TCP_Transporter::send_is_possible(NDB_SOCKET_TYPE fd,int timeout_millisec) const
{
  ndb_socket_poller poller;

  if (!ndb_socket_valid(fd))
    return false;

  poller.add(fd, false, true, false);

  if (poller.poll_unsafe(timeout_millisec) <= 0)
    return false; // Timeout or error occured

  return true;
}

#define DISCONNECT_ERRNO(e, sz) ((sz == 0) || \
                                 (!((sz == -1) && ((e == SOCKET_EAGAIN) || (e == SOCKET_EWOULDBLOCK) || (e == SOCKET_EINTR)))))


bool
TCP_Transporter::doSend() {
  struct iovec iov[64];
  Uint32 cnt = fetch_send_iovec_data(iov, NDB_ARRAY_SIZE(iov));
  Uint32 init_cnt = cnt;

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

  Uint32 pos = 0;
  Uint32 sum_sent = 0;
  Uint32 send_cnt = 0;
  Uint32 remain = sum;

  if (cnt == NDB_ARRAY_SIZE(iov))
  {
    // If pulling all iov's make sure that we never return everyting
    // flushed
    sum++;
  }

  while (send_cnt < 5)
  {
    send_cnt++;
    Uint32 iovcnt = cnt > m_os_max_iovec ? m_os_max_iovec : cnt;
    if (checksumUsed && check_send_checksum)
    {
      /* Check combination of sent + potential-to-be-sent */
      checksum_state cs = send_checksum_state;
      if (!cs.computev(iov + pos, iovcnt))
      {
        g_eventLogger->error("TCP_Transporter::doSend(%u) computev() failed. "
                             "cnt %u iovcnt %u pos %u send_cnt %u sum_sent %u "
                             "remain %u",
                             remoteNodeId,
                             cnt,
                             iovcnt,
                             pos,
                             send_cnt,
                             sum_sent,
                             remain);
        /* Consider disconnecting remote rather than killing node */
        require(false);
      }
    }
    int nBytesSent = (int)ndb_socket_writev(theSocket, iov+pos, iovcnt);
    assert(nBytesSent <= (int)remain);

    if (checksumUsed && check_send_checksum)
    {
      /* Add + check sent into current state */
      if (nBytesSent > 0)
      {
        if (!send_checksum_state.computev(iov + pos, iovcnt, nBytesSent))
        {
          g_eventLogger->error("TCP_Transporter::doSend(%u) computev() failed. "
                               "nBytesSent %u cnt %u iovcnt %u pos %u send_cnt %u "
                               "sum_sent %u remain %u",
                               remoteNodeId,
                               nBytesSent,
                               cnt,
                               iovcnt,
                               pos,
                               send_cnt,
                               sum_sent,
                               remain);
          /* Consider disconnecting remote rather than killing node */
          require(false);
        }
      }
    }

    if (Uint32(nBytesSent) == remain)  //Completed this send
    {
      sum_sent += nBytesSent;
      assert(sum >= sum_sent);
      remain = sum - sum_sent;
      break;
    }
    else if (nBytesSent > 0)           //Sent some, more pending
    {
      sum_sent += nBytesSent;
      require(remain >= (Uint32)nBytesSent);
      remain -= nBytesSent;

      /**
       * Forward in iovec
       */
      while (Uint32(nBytesSent) >= iov[pos].iov_len)
      {
        assert(iov[pos].iov_len > 0);
        nBytesSent -= iov[pos].iov_len;
        pos++;
        cnt--;
        require(cnt <= init_cnt); //prevent overflow/ wrap around
        require(pos < init_cnt); // avoid seg fault
      }

      if (nBytesSent > 0)
      {
        assert(iov[pos].iov_len > Uint32(nBytesSent));
        iov[pos].iov_len -= nBytesSent;
        iov[pos].iov_base = ((char*)(iov[pos].iov_base))+nBytesSent;
      }
    }
    else                               //Send failed, terminate
    {
      const int err = ndb_socket_errno();

#if defined DEBUG_TRANSPORTER
      g_eventLogger->error("Send Failure(disconnect==%d) to node = %d "
                           "nBytesSent = %d "
                           "errno = %d strerror = %s",
                           DISCONNECT_ERRNO(err, nBytesSent),
                           remoteNodeId, nBytesSent, ndb_socket_errno(),
                           (char*)ndbstrerror(err));
#endif

      if ((DISCONNECT_ERRNO(err, nBytesSent)))
      {
        do_disconnect(err); //Initiate pending disconnect
        remain = 0;
      }
      break;
    }
  }

  if (sum_sent > 0)
  {
    iovec_data_sent(sum_sent);
  }
  sendCount += send_cnt;
  sendSize  += sum_sent;
  m_bytes_sent += sum_sent;
  if(sendCount >= reportFreq)
  {
    get_callback_obj()->reportSendLen(remoteNodeId, sendCount, sendSize);
    sendCount = 0;
    sendSize  = 0;
  }

  return (remain>0); // false if nothing remains or disconnected, else true
}

int
TCP_Transporter::doReceive(TransporterReceiveHandle& recvdata)
{
  // Select-function must return the socket for read
  // before this method is called
  // It reads the external TCP/IP interface once
  Uint32 size = receiveBuffer.sizeOfBuffer - receiveBuffer.sizeOfData;
  if(size > 0){
    const int nBytesRead = (int)ndb_recv(theSocket,
				receiveBuffer.insertPtr,
				size < maxReceiveSize ? size : maxReceiveSize,
				0);

    if (nBytesRead > 0) {
      receiveBuffer.sizeOfData += nBytesRead;
      receiveBuffer.insertPtr  += nBytesRead;
      require(receiveBuffer.insertPtr <= (char*)(receiveBuffer.startOfBuffer) +
                 receiveBuffer.sizeOfBuffer); // prevent buf overflow
      
      if(receiveBuffer.sizeOfData > receiveBuffer.sizeOfBuffer){
#ifdef DEBUG_TRANSPORTER
        g_eventLogger->error("receiveBuffer.sizeOfData(%d) > receiveBuffer.sizeOfBuffer(%d)",
                             receiveBuffer.sizeOfData, receiveBuffer.sizeOfBuffer);
        g_eventLogger->error("nBytesRead = %d", nBytesRead);
#endif
        g_eventLogger->error("receiveBuffer.sizeOfData(%d) > receiveBuffer.sizeOfBuffer(%d)",
                             receiveBuffer.sizeOfData, receiveBuffer.sizeOfBuffer);
	report_error(TE_INVALID_MESSAGE_LENGTH);
	return 0;
      }
      
      receiveCount ++;
      receiveSize  += nBytesRead;
      m_bytes_received += nBytesRead;
      
      if(receiveCount == reportFreq){
        recvdata.reportReceiveLen(remoteNodeId,
                                  receiveCount, receiveSize);
	receiveCount = 0;
	receiveSize  = 0;
      }
      return nBytesRead;
    } else {
#if defined DEBUG_TRANSPORTER
      g_eventLogger->error("Receive Failure(disconnect==%d) to node = %d nBytesSent = %d "
                           "errno = %d strerror = %s",
                           DISCONNECT_ERRNO(ndb_socket_errno(), nBytesRead),
                           remoteNodeId, nBytesRead, ndb_socket_errno(),
                           (char*)ndbstrerror(ndb_socket_errno()));
#endif   
      if(DISCONNECT_ERRNO(ndb_socket_errno(), nBytesRead)){
	do_disconnect(ndb_socket_errno());
      } 
    }
    return nBytesRead;
  } else {
    return 0;
  }
}

void
TCP_Transporter::disconnectImpl()
{
  get_callback_obj()->lock_transporter(remoteNodeId);

  NDB_SOCKET_TYPE sock = theSocket;
  ndb_socket_invalidate(&theSocket);

  get_callback_obj()->unlock_transporter(remoteNodeId);

  if(ndb_socket_valid(sock))
  {
    if(ndb_socket_close(sock) < 0){
      report_error(TE_ERROR_CLOSING_SOCKET);
    }
  }
}
