/*
   Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

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

#include "Loopback_Transporter.hpp"
#include <NdbOut.hpp>
#include <NdbSleep.h>

#include <EventLogger.hpp>
extern EventLogger * g_eventLogger;
// End of stuff to be moved


Loopback_Transporter::Loopback_Transporter(TransporterRegistry &t_reg,
                                           const TransporterConfiguration* conf)
  : TCP_Transporter(t_reg, conf)
{
  assert(isServer == false);
}

Loopback_Transporter::~Loopback_Transporter()
{
}

bool
Loopback_Transporter::connect_client()
{
  NDB_SOCKET_TYPE pair[2];
  if (my_socketpair(pair))
  {
    perror("socketpair failed!");
    return false;
  }

  if (!TCP_Transporter::setSocketNonBlocking(pair[0]) ||
      !TCP_Transporter::setSocketNonBlocking(pair[1]))
  {
    goto err;
  }

  theSocket = pair[0];
  m_send_socket = pair[1];
  m_connected = true;
  return true;

err:
  my_socket_close(pair[0]);
  my_socket_close(pair[1]);
  return false;
}

void
Loopback_Transporter::disconnectImpl()
{
  NDB_SOCKET_TYPE pair[] = { theSocket, m_send_socket };

  get_callback_obj()->lock_transporter(remoteNodeId);

  receiveBuffer.clear();
  my_socket_invalidate(&theSocket);
  my_socket_invalidate(&m_send_socket);

  get_callback_obj()->unlock_transporter(remoteNodeId);

  if (my_socket_valid(pair[0]))
    my_socket_close(pair[0]);

  if (my_socket_valid(pair[1]))
    my_socket_close(pair[1]);
}

bool
Loopback_Transporter::send_is_possible(int timeout_millisec) const
{
  return TCP_Transporter::send_is_possible(m_send_socket, timeout_millisec);
}

#define DISCONNECT_ERRNO(e, sz) ((sz == 0) || \
                                 (!((sz == -1) && ((e == SOCKET_EAGAIN) || (e == SOCKET_EWOULDBLOCK) || (e == SOCKET_EINTR)))))

int
Loopback_Transporter::doSend() {
  struct iovec iov[64];
  Uint32 cnt = fetch_send_iovec_data(iov, NDB_ARRAY_SIZE(iov));

  if (cnt == 0)
  {
    return 0;
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
    int nBytesSent = (int)my_socket_writev(m_send_socket, iov+pos, iovcnt);
    assert(nBytesSent <= (int)remain);

    if (Uint32(nBytesSent) == remain)
    {
      sum_sent += nBytesSent;
      goto ok;
    }
    else if (nBytesSent > 0)
    {
      sum_sent += nBytesSent;
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
      }

      if (nBytesSent)
      {
        assert(iov[pos].iov_len > Uint32(nBytesSent));
        iov[pos].iov_len -= nBytesSent;
        iov[pos].iov_base = ((char*)(iov[pos].iov_base))+nBytesSent;
      }
      continue;
    }
    else
    {
      int err = my_socket_errno();
      if (!(DISCONNECT_ERRNO(err, nBytesSent)))
      {
        if (sum_sent)
        {
          goto ok;
        }
        else
        {
          return remain;
        }
      }

      do_disconnect(err);
      return 0;
    }
  }

ok:
  assert(sum >= sum_sent);
  iovec_data_sent(sum_sent);
  sendCount += send_cnt;
  sendSize  += sum_sent;
  if(sendCount >= reportFreq)
  {
    get_callback_obj()->reportSendLen(remoteNodeId, sendCount, sendSize);
    sendCount = 0;
    sendSize  = 0;
  }

  return sum - sum_sent; // 0 if every thing flushed else >0
}
