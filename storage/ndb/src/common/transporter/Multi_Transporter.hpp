/*
   Copyright (c) 2019, 2022, Oracle and/or its affiliates.

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

#ifndef MULTI_TRANSPORTER_HPP
#define MULTI_TRANSPORTER_HPP

#include "util/require.h"
#include "Transporter.hpp"

class Multi_Transporter : public Transporter {
  friend class TransporterRegistry;
  friend class Qmgr;
private:
  // Initialize member variables
  Multi_Transporter(TransporterRegistry&, const Transporter*);

  ~Multi_Transporter() override;

  /**
   * Clear any data buffered in the transporter.
   * Should only be called in a disconnected state.
   */
  void resetBuffers() override;

  bool configure_derived(const TransporterConfiguration* /*conf*/) override
  {
    return true;
  }

public:
  Uint64 get_bytes_sent() const override
  {
    Uint64 bytes_sent = m_bytes_sent;
    for (Uint32 i = 0; i < m_num_active_transporters; i++)
    {
      bytes_sent += m_active_transporters[i]->m_bytes_sent;
    }
    return bytes_sent;
  }

  Uint64 get_bytes_received() const override
  {
    Uint64 bytes_received = m_bytes_received;
    for (Uint32 i = 0; i < m_num_active_transporters; i++)
    {
      bytes_received += m_active_transporters[i]->m_bytes_received;
    }
    return bytes_received;
  }

  Transporter* get_send_transporter(Uint32 recBlock, Uint32 /*sendBlock*/) override
  {
    /**
     * We hash on receiver instance to avoid any risk of changed signal order
     * compared to today. In addition each receiver thread will act on behalf
     * of a subset of the LDM/TC threads which should minimise the future
     * mutex interactions between receiver threads.
     */
    Uint32 recInstance = recBlock >> NDBMT_BLOCK_BITS;
    Uint32 instanceXOR = recInstance;
    Uint32 index = instanceXOR % m_num_active_transporters;
    return m_active_transporters[index];
  }

  bool isMultiTransporter() override
  {
    return true;
  }

  Uint32 get_num_active_transporters()
  {
    return m_num_active_transporters;
  }

  Uint32 get_num_inactive_transporters()
  {
    return m_num_inactive_transporters;
  }

  Transporter* get_active_transporter(Uint32 index)
  {
    require(index < m_num_active_transporters);
    return m_active_transporters[index];
  }

  Transporter* get_inactive_transporter(Uint32 index)
  {
    require(index < m_num_inactive_transporters);
    return m_inactive_transporters[index];
  }

private:
  /**
   * Allocate buffers for sending and receiving
   */
  bool initTransporter() override;

  /**
   * Retrieves the contents of the send buffers and writes it on
   * the external TCP/IP interface.
   */
  bool doSend(bool /*need_wakeup*/) override
  {
    /* Send only done on real transporters */
    require(false);
    return true;
  }

  bool send_is_possible(int) const override
  {
    require(false);
    return true;
  }

  bool send_limit_reached(int) override
  {
    require(false);
    return true;
  }

  void add_active_trp(Transporter*);
  void add_not_used_trp(Transporter*);
  void switch_active_trp();
  void set_num_inactive_transporters(Uint32);
protected:
  /**
   * Setup client/server and perform connect/accept
   * Is used both by clients and servers
   * A client connects to the remote server
   * A server accepts any new connections
   */
  bool connect_server_impl(ndb_socket_t sockfd) override;
  bool connect_client_impl(ndb_socket_t sockfd) override;
  bool connect_common(ndb_socket_t sockfd);
 
  /**
   * Disconnects a TCP/IP node, possibly blocking.
   */
  void disconnectImpl() override;
 
private:
  Uint32 m_num_active_transporters;
  Uint32 m_num_inactive_transporters;
  Uint32 m_num_not_used_transporters;
  Transporter* m_active_transporters[MAX_NODE_GROUP_TRANSPORTERS];
  Transporter* m_inactive_transporters[MAX_NODE_GROUP_TRANSPORTERS];
  Transporter* m_not_used_transporters[MAX_NODE_GROUP_TRANSPORTERS];
};
#endif // Define of TCP_Transporter_H
