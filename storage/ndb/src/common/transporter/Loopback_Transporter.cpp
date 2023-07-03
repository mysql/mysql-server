/*
   Copyright (c) 2010, 2023, Oracle and/or its affiliates.

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

#include "Loopback_Transporter.hpp"
#include <NdbOut.hpp>
#include <NdbSleep.h>

#include <EventLogger.hpp>
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
  ndb_socket_t pair[2];
  if (ndb_socketpair(pair))
  {
    perror("socketpair failed!");
    return false;
  }

  if (!TCP_Transporter::setSocketNonBlocking(pair[0]) ||
      !TCP_Transporter::setSocketNonBlocking(pair[1]))
  {
    goto err;
  }

  theSocket.init_from_new(pair[0]);
  m_send_socket = pair[1];

  m_connected = true;
  return true;

err:
  ndb_socket_close(pair[0]);
  ndb_socket_close(pair[1]);
  return false;
}

void
Loopback_Transporter::disconnectImpl()
{
  ndb_socket_t pair[] = { theSocket.ndb_socket(), m_send_socket };

  get_callback_obj()->lock_transporter(remoteNodeId, m_transporter_index);

  theSocket.invalidate();
  ndb_socket_invalidate(&m_send_socket);

  get_callback_obj()->unlock_transporter(remoteNodeId, m_transporter_index);

  if (ndb_socket_valid(pair[0]))
    ndb_socket_close(pair[0]);

  if (ndb_socket_valid(pair[1]))
    ndb_socket_close(pair[1]);
}

bool
Loopback_Transporter::send_is_possible(int timeout_millisec) const
{
  return TCP_Transporter::send_is_possible(m_send_socket, timeout_millisec);
}

bool
Loopback_Transporter::doSend(bool need_wakeup)
{
  (void)need_wakeup;
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

  Uint32 pos = 0;
  Uint32 sum_sent = 0;
  Uint32 send_cnt = 0;
  Uint32 remain = sum;

  if (cnt == NDB_ARRAY_SIZE(iov))
  {
    // If pulling all iov's make sure that we never return everything
    // flushed
    sum++;
  }

  while (send_cnt < 5)
  {
    send_cnt++;
    Uint32 iovcnt = cnt > m_os_max_iovec ? m_os_max_iovec : cnt;
    int nBytesSent = (int)ndb_socket_writev(m_send_socket, iov+pos, iovcnt);
    assert(nBytesSent <= (int)remain);

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
      if ((DISCONNECT_ERRNO(err, nBytesSent)))
      {
        do_disconnect(err, true); //Initiate pending disconnect
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
  if(sendCount >= reportFreq)
  {
    get_callback_obj()->reportSendLen(remoteNodeId, sendCount, sendSize);
    sendCount = 0;
    sendSize  = 0;
  }

  return (remain>0); // false if nothing remains or disconnected, else true
}
