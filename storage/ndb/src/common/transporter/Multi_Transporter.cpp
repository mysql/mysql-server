/*
   Copyright (c) 2019, 2023, Oracle and/or its affiliates.

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

#include "util/require.h"
#include <ndb_global.h>

#include "Multi_Transporter.hpp"
#include <NdbOut.hpp>
#include <NdbSleep.h>

#include <EventLogger.hpp>
// End of stuff to be moved

Multi_Transporter::Multi_Transporter(TransporterRegistry &t_reg,
                                     const Transporter* t)
  :
  Transporter(t_reg,
              0,
              tt_Multi_TRANSPORTER,
	      t->localHostName,
	      t->remoteHostName,
	      t->m_s_port,
	      t->isMgmConnection,
	      t->localNodeId,
	      t->remoteNodeId,
	      t->isServer ? t->localNodeId : t->remoteNodeId,
	      0,
              false, 
	      t->checksumUsed,
	      t->signalIdUsed,
	      t->m_max_send_buffer,
	      t->check_send_checksum,
              t->m_spintime)
{
  send_checksum_state.init();
  m_num_active_transporters = 0;
  m_num_inactive_transporters = 0;
  m_num_not_used_transporters = 0;
  for (Uint32 i = 0; i < MAX_NODE_GROUP_TRANSPORTERS; i++)
  {
    m_active_transporters[i] = nullptr;
    m_inactive_transporters[i] = nullptr;
    m_not_used_transporters[i] = nullptr;
  }
}

Multi_Transporter::~Multi_Transporter()
{
}

void
Multi_Transporter::resetBuffers()
{
  send_checksum_state.init();
}

bool Multi_Transporter::connect_server_impl(NdbSocket &)
{
  return true;
}

bool Multi_Transporter::connect_client_impl(NdbSocket &)
{
  return true;
}

bool
Multi_Transporter::initTransporter()
{
  m_num_active_transporters = 0;
  m_num_not_used_transporters = 0;
  m_num_inactive_transporters = 0;
  send_checksum_state.init();
  return true;
}

void
Multi_Transporter::disconnectImpl()
{
  require(false);
}

void
Multi_Transporter::add_not_used_trp(Transporter *t)
{
  require(m_num_not_used_transporters < MAX_NODE_GROUP_TRANSPORTERS);
  m_not_used_transporters[m_num_not_used_transporters++] = t;
  t->set_transporter_active(false);
}

void
Multi_Transporter::add_active_trp(Transporter *t)
{
  require(m_num_active_transporters < MAX_NODE_GROUP_TRANSPORTERS);
  m_active_transporters[m_num_active_transporters++] = t;
  t->set_transporter_active(true);
}

void
Multi_Transporter::set_num_inactive_transporters(Uint32 num_used)
{
  require(num_used <= m_num_not_used_transporters +
                      m_num_inactive_transporters);
  require(m_num_active_transporters == 1);
  if (m_num_inactive_transporters != num_used)
  {
    if (m_num_inactive_transporters > num_used)
    {
      /* Decrease the number of inactive transporters */
      Uint32 num_inactive_transporters = m_num_inactive_transporters;
      for (Uint32 i = num_used; i < num_inactive_transporters; i++)
      {
        Transporter *t = m_inactive_transporters[i];
        m_not_used_transporters[i] = t;
        require(t);
        m_inactive_transporters[i] = nullptr;
        m_num_inactive_transporters--;
        m_num_not_used_transporters++;
      }
    }
    else
    {
      /* Increase the number of inactive transporters */
      for (Uint32 i = m_num_inactive_transporters;
           i < num_used;
           i++)
      {
        Transporter *t = m_not_used_transporters[i];
        m_inactive_transporters[i] = t;
        m_not_used_transporters[i] = nullptr;
        require(t);
        m_num_inactive_transporters++;
        m_num_not_used_transporters--;
      }
    }
  }
  else
  {
    /* Already set to the value assigned, no need to do anything. */
  }
  require(num_used == m_num_inactive_transporters);
}

void
Multi_Transporter::switch_active_trp()
{
  Uint64 bytes_sent = 0;
  Uint64 bytes_received = 0;
  for (Uint32 i = 0; i < m_num_active_transporters; i++)
  {
    bytes_sent += m_active_transporters[i]->m_bytes_sent;
    m_active_transporters[i]->m_bytes_sent = 0;
    bytes_received += m_active_transporters[i]->m_bytes_received;
    m_active_transporters[i]->m_bytes_received = 0;
    m_active_transporters[i]->set_transporter_active(false);
  }
  m_bytes_sent += bytes_sent;
  m_bytes_received += bytes_received;

  for (Uint32 i = 0; i < m_num_inactive_transporters; i++)
  {
    m_inactive_transporters[i]->m_bytes_sent = 0;
    m_inactive_transporters[i]->m_bytes_received = 0;
    m_inactive_transporters[i]->set_transporter_active(true);
  }

  /* Save transporters and number of them before copying inactive to active */
  Transporter *save_active_transporters[MAX_NODE_GROUP_TRANSPORTERS];
  Uint32 save_num_active_transporters = m_num_active_transporters;
  for (Uint32 i = 0; i < m_num_active_transporters; i++)
  {
    save_active_transporters[i] = m_active_transporters[i];
  }

  /* Copy inactive to active, ensure not used entries are 0 */
  memset(m_active_transporters, 0, sizeof(m_active_transporters));
  for (Uint32 i = 0; i < m_num_inactive_transporters; i++)
  {
    m_active_transporters[i] = m_inactive_transporters[i];
  }
  m_num_active_transporters = m_num_inactive_transporters;

  /* Copy saved to inactive, ensure not used entries are 0 */
  memset(m_inactive_transporters, 0, sizeof(m_inactive_transporters));
  for (Uint32 i = 0; i < save_num_active_transporters; i++)
  {
    m_inactive_transporters[i] = save_active_transporters[i];
  }
  m_num_inactive_transporters = save_num_active_transporters;
}
