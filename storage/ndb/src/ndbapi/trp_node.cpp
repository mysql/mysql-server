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

#include "trp_node.hpp"
#include <NdbOut.hpp>

trp_node::trp_node()
{
  compatible = nfCompleteRep = true;
  m_connected = defined = m_alive = m_api_reg_conf = m_node_fail_rep = false;
  bzero(&m_state, sizeof(m_state));
  m_state.init();
  m_state.startLevel = NodeState::SL_NOTHING;
  minDbVersion = 0;
}

bool
trp_node::operator==(const trp_node& other) const
{
  return (compatible == other.compatible &&
          nfCompleteRep == other.nfCompleteRep &&
          m_connected == other.m_connected &&
          defined == other.defined &&
          m_alive == other.m_alive &&
          m_api_reg_conf == other.m_api_reg_conf &&
          m_node_fail_rep == other.m_node_fail_rep &&
          minDbVersion == other.minDbVersion &&
          memcmp(&m_state, &other.m_state, sizeof(m_state)) == 0);
}

NdbOut&
operator<<(NdbOut& out, const trp_node& n)
{
  out << "[ "
      << "defined: " << n.defined
      << ", compatible: " << n.compatible
      << ", connected: " << n.m_connected
      << ", api_reg_conf: " << n.m_api_reg_conf
      << ", alive: " << n.m_alive
      << ", nodefailrep: " << n.m_node_fail_rep
      << ", nfCompleteRep: " << n.nfCompleteRep
      << ", minDbVersion: " << n.minDbVersion
      << ", state: " << n.m_state
      << ", connected: "
      << BaseString::getPrettyTextShort(n.m_state.m_connected_nodes).c_str()
      << "]";

  return out;
}
