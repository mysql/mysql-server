/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.

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

#ifndef Transporter_H
#define Transporter_H

#include <ndb_global.h>

#include <EventLogger.hpp>
#include <SocketClient.hpp>

#include <TransporterRegistry.hpp>
#include <TransporterCallback.hpp>
#include "TransporterDefinitions.hpp"
#include "Packer.hpp"

#include <NdbMutex.h>
#include <NdbThread.h>

#include "portlib/ndb_socket.h"
#include "util/NdbSocket.h"

#define DISCONNECT_ERRNO(e, sz) ( \
                (sz == 0) || \
                 (!((sz == -1) && \
                  ((e == SOCKET_EAGAIN) || \
                   (e == SOCKET_EWOULDBLOCK) || \
                   (e == SOCKET_EINTR)))))

class Transporter {
  friend class TransporterRegistry;
  friend class Multi_Transporter;
  friend class Qmgr;
public:
  virtual bool initTransporter() = 0;

  /**
   * Destructor
   */
  virtual ~Transporter();


  /**
   * Disconnect node/socket
   */
  bool do_disconnect(int err, bool send_source);

  /**
   * Clear any data buffered in the transporter.
   * Should only be called in a disconnected state.
   */
  virtual void resetBuffers() {}

  /**
   * Is this transporter part of a multi transporter.
   * It is a real transporter, but can be connected
   * when the node is in the state connected.
   */
  virtual bool isPartOfMultiTransporter()
  {
    return (m_multi_transporter_instance != 0);
  }

  Uint32 get_multi_transporter_instance()
  {
    return m_multi_transporter_instance;
  }
  virtual bool isMultiTransporter()
  {
    return false;
  }

  void set_multi_transporter_instance(Uint32 val)
  {
    m_multi_transporter_instance = val;
  }

  virtual Uint64 get_bytes_sent() const
  {
    return m_bytes_sent;
  }

  virtual Uint64 get_bytes_received() const
  {
    return m_bytes_received;
  }

  /**
   * In most cases we only use transporter per node connection.
   * But in cases where the transporter is heavily loaded we can
   * have multiple transporters to send for one node connection.
   * In this case theNodeIdTransporters points to a Multi_Transporter
   * object that has implemented a hash algorithm for
   * get_send_transporter based on sending thread and receiving
   * thread.
   */
  virtual Transporter* get_send_transporter(Uint32 recBlock, Uint32 sendBlock)
  {
    (void)recBlock;
    (void)sendBlock;
    return this;
  }

  /**
   * None blocking
   *    Use isConnected() to check status
   */
  virtual bool connect_client();
  bool connect_client(NdbSocket &);
  bool connect_client(ndb_socket_t fd) {
    NdbSocket socket(fd, NdbSocket::From::Existing);
    return connect_client(socket);
  }
  bool connect_server(NdbSocket & socket, BaseString& errormsg);

  /**
   * Returns socket used (sockets are used for all transporters to ensure
   * we can wake up also shared memory transporters and other types of
   * transporters in consistent manner.
   */
  ndb_socket_t getSocket() const;

  /**
   * Blocking
   */
  void doDisconnect();

  /**
   * Are we currently connected
   */
  bool isConnected() const;
  
  /**
   * Remote Node Id
   */
  NodeId getRemoteNodeId() const;

  /**
   * Index into allTransporters array.
   */
  TrpId getTransporterIndex() const;
  void setTransporterIndex(TrpId);
  /**
   * Local (own) Node Id
   */
  NodeId getLocalNodeId() const;

  /**
   * Get port we're connecting to (signed)
   */
  int get_s_port() const {
    return m_s_port;
  }
  
  /**
   * Set port to connect to (signed)
   */
  void set_s_port(int port) {
    m_s_port = port;
  }

  void update_status_overloaded(Uint32 used)
  {
    m_transporter_registry.set_status_overloaded(remoteNodeId,
                                                 used >= m_overload_limit);
    m_transporter_registry.set_status_slowdown(remoteNodeId,
                                               used >= m_slowdown_limit);
  }

  virtual bool doSend(bool need_wakeup = true) = 0;

  /* Get the configured maximum send buffer usage. */
  Uint32 get_max_send_buffer() { return m_max_send_buffer; }

  Uint32 get_connect_count() { return m_connect_count; }

  void inc_overload_count() { m_overload_count++; }
  Uint32 get_overload_count() { return m_overload_count; }
  void inc_slowdown_count() { m_slowdown_count++; }
  Uint32 get_slowdown_count() { return m_slowdown_count; }
  void set_recv_thread_idx (Uint32 recv_thread_idx)
  {
    m_recv_thread_idx = recv_thread_idx;
  }
  void set_transporter_active(bool active)
  {
    m_is_active = active;
  }
  Uint32 get_recv_thread_idx() { return m_recv_thread_idx; }

  TransporterType getTransporterType() const;

  /**
   * Only applies to TCP transporter, abort on any other object.
   * Used as part of shutting down transporter when switching to
   * multi socket setup.
   * Shut down only for writes when all data have been sent.
   */
  virtual void shutdown() { abort();}

protected:
  Transporter(TransporterRegistry &,
              TrpId transporter_index,
	      TransporterType,
	      const char *lHostName,
	      const char *rHostName, 
	      int s_port,
	      bool isMgmConnection,
	      NodeId lNodeId,
	      NodeId rNodeId,
	      NodeId serverNodeId,
	      int byteorder, 
	      bool compression, 
	      bool checksum, 
	      bool signalId,
              Uint32 max_send_buffer,
              bool _presend_checksum,
              Uint32 spintime);

  virtual bool configure(const TransporterConfiguration* conf);
  virtual bool configure_derived(const TransporterConfiguration* conf) = 0;

  /**
   * Blocking, for max timeOut milli seconds
   *   Returns true if connect succeeded
   */
  virtual bool connect_server_impl(NdbSocket &) = 0;
  virtual bool connect_client_impl(NdbSocket &) = 0;
  virtual int pre_connect_options(ndb_socket_t) { return 0;}
  
  /**
   * Blocking
   */
  virtual void disconnectImpl() = 0;
  
  /**
   * Remote host name/and address
   */
  char remoteHostName[256];
  char localHostName[256];

  int m_s_port;

  Uint32 m_spintime;
  Uint32 get_spintime()
  {
    return m_spintime;
  }
  const NodeId remoteNodeId;
  const NodeId localNodeId;

  TrpId m_transporter_index;
  const bool isServer;

  int byteOrder;
  bool compressionUsed;
  bool checksumUsed;
  bool check_send_checksum;
  bool signalIdUsed;
  Packer m_packer;  
  Uint32 m_max_send_buffer;
  /* Overload limit, as configured with the OverloadLimit config parameter. */
  Uint32 m_overload_limit;
  Uint32 m_slowdown_limit;
  void resetCounters();
  Uint64 m_bytes_sent;
  Uint64 m_bytes_received;
  Uint32 m_connect_count;
  Uint32 m_overload_count;
  Uint32 m_slowdown_count;

  // Sending/Receiving socket used by both client and server
  NdbSocket theSocket;
private:
  SocketClient *m_socket_client;
  struct in6_addr m_connect_address;

  virtual bool send_is_possible(int timeout_millisec) const = 0;
  virtual bool send_limit_reached(int bufsize) = 0;

  void update_connect_state(bool connected);

protected:
  /**
   * means that we transform an MGM connection into
   * a transporter connection
   */
  bool isMgmConnection;

  Uint32 m_multi_transporter_instance;
  Uint32 m_recv_thread_idx;
  bool m_is_active;

  Uint32 m_os_max_iovec;
  Uint32 m_timeOutMillis;
  bool m_connected;     // Are we connected
  TransporterType m_type;

  /**
   * Statistics
   */
  Uint32 reportFreq;
  Uint32 receiveCount;
  Uint64 receiveSize;
  Uint32 sendCount;
  Uint64 sendSize;

  TransporterRegistry &m_transporter_registry;
  TransporterCallback *get_callback_obj() { return m_transporter_registry.callbackObj; }
  void report_error(enum TransporterError err, const char *info = nullptr)
    { m_transporter_registry.report_error(remoteNodeId, err, info); }

  Uint32 fetch_send_iovec_data(struct iovec dst[], Uint32 cnt);
  void iovec_data_sent(int nBytesSent);

  void set_get(ndb_socket_t fd,
               int level,
               int optval,
               const char *optname, 
               int val);
  /*
   * Keep checksum state for Protocol6 messages over a byte stream.
   */
  class checksum_state {
    enum cs_states
    {
      CS_INIT,
      CS_MSG_CHECK,
      CS_MSG_NOCHECK
    };
    cs_states state;
    Uint32 chksum; // of already sent bytes, rotated so next byte to process matches first byte of chksum
    Uint16 pending; // remaining bytes before state change
  public:
    bool computev(const struct iovec *iov, int iovcnt, size_t bytecnt = SIZE_T_MAX);
    checksum_state(): state(CS_INIT), chksum(0), pending(4) {}
    void init() { state = CS_INIT; chksum = 0; pending = 4; }
  private:
    bool compute(const void* bytes, size_t len);
    void dumpBadChecksumInfo(Uint32 inputSum,
                             Uint32 badSum,
                             size_t offset,
                             Uint32 remaining,
                             const void* buf,
                             size_t len) const;

    static_assert(MAX_SEND_MESSAGE_BYTESIZE == (Uint16)MAX_SEND_MESSAGE_BYTESIZE);
    static_assert(SIZE_T_MAX == (size_t)SIZE_T_MAX);
  };
  checksum_state send_checksum_state;
};

inline
ndb_socket_t
Transporter::getSocket() const {
  return theSocket.ndb_socket();
}

inline
TransporterType
Transporter::getTransporterType() const
{
  return m_type;
}

inline
bool
Transporter::isConnected() const {
  return m_connected;
}

inline
NodeId
Transporter::getRemoteNodeId() const {
  return remoteNodeId;
}

inline
TrpId
Transporter::getTransporterIndex() const {
  return m_transporter_index;
}

inline
void
Transporter::setTransporterIndex(TrpId val)
{
  m_transporter_index = val;
}

inline
NodeId
Transporter::getLocalNodeId() const {
  return localNodeId;
}

/**
 * Get data to send (in addition to data possibly remaining from previous
 * partial send).
 */
inline
Uint32
Transporter::fetch_send_iovec_data(struct iovec dst[], Uint32 cnt)
{
  return get_callback_obj()->get_bytes_to_send_iovec(remoteNodeId,
                                                     m_transporter_index,
                                                     dst,
                                                     cnt);
}

inline
void
Transporter::iovec_data_sent(int nBytesSent)
{
  Uint32 used_bytes = get_callback_obj()->bytes_sent(remoteNodeId,
                                                     m_transporter_index,
                                                     nBytesSent);
  update_status_overloaded(used_bytes);
}

inline
bool
Transporter::checksum_state::compute(const void* buf, size_t len)
{
  const Uint32 inputSum = chksum;
  Uint32 off = 0;
  unsigned char* psum = static_cast<unsigned char*>(static_cast<void*>(&chksum));
  const unsigned char* bytes = static_cast<const unsigned char*>(buf);

  while (off < len)
  {
    const Uint32 available = len - off;
    switch (state)
    {
    case CS_INIT:
    {
      assert(pending <= 4);
      assert(chksum == 0 || pending < 4);
      const Uint32 nb = MIN(pending, available);
      memcpy(psum + (4-pending), bytes + off, nb);
      off+= nb;
      pending-= nb;

      if (pending == 0)
      {
        /* Msg header word 0 complete, parse to determine msg length */
        assert(Protocol6::getMessageLength(chksum) <= (MAX_SEND_MESSAGE_BYTESIZE >> 2));
        assert(Protocol6::getMessageLength(chksum) >= 2);
        pending = (Protocol6::getMessageLength(chksum) * 4) - 4; /* Word 0 eaten */
        state = (Protocol6::getCheckSumIncluded(chksum)? CS_MSG_CHECK : CS_MSG_NOCHECK);
      }
      break;
    }
    case CS_MSG_CHECK:
    case CS_MSG_NOCHECK:
    {
      if (available < pending)
      {
        /* Only part of current msg body present */
        if (state == CS_MSG_CHECK)
        {
          /* Add available content to the checksum */
          chksum = computeXorChecksumBytes(bytes + off, available, chksum);
        }
        off += available;
        pending -= available;
      }
      else
      {
        /* All of current msg body present, consume and check it */
        if (state == CS_MSG_CHECK)
        {
          chksum = computeXorChecksumBytes(bytes + off, pending, chksum);
          if (chksum != 0)
          {
            dumpBadChecksumInfo(inputSum,
                                chksum,
                                off,
                                pending,
                                buf,
                                len);
            return false;
          }
        }
        off += pending;

        /* Now we are ready for the next msg */
        pending = 4;
        state = CS_INIT;
      }
      break;
    }
    }
  } // while (off < len)

  return true;
}


inline
bool
Transporter::checksum_state::computev(const struct iovec *iov, int iovcnt, size_t bytecnt)
{
  // bytecnt is SIZE_T_MAX implies use all iovec
  size_t off = 0;
  bool ok = true;
  for(int iovi = 0; ok && bytecnt > off && iovi < iovcnt; iovi ++)
  {
    int nb = iov[iovi].iov_len;
    if (bytecnt < off + nb)
    {
      nb = bytecnt - off;
    }
    if (!compute(iov[iovi].iov_base, nb))
    {
      g_eventLogger->info(
          "Transporter::checksum_state::computev() failed on IOV %u/%u "
          "byteCount %llu off %llu nb %u",
          iovi, iovcnt, Uint64(bytecnt), Uint64(off), nb);
      /* TODO : Dump more IOV + bytecnt details */
      return false;
    }
    off += nb;
  }
  if (bytecnt != SIZE_T_MAX && bytecnt != off)
  {
    g_eventLogger->info(
        "Transporter::checksum_state::computev() failed : "
        "bytecnt %llu off %llu",
        Uint64(bytecnt), Uint64(off));
    ok = false;
  }
  return ok;
}

#endif // Define of Transporter_H
