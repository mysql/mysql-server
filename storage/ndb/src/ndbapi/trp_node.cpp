/*
   Copyright (c) 2010, 2022, Oracle and/or its affiliates.

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

#include <cstring>
#include "trp_node.hpp"
#include <NdbOut.hpp>

NdbOut&
operator<<(NdbOut& out, const trp_node& n)
{
  out << "[ "
      << "defined: " << n.defined << ", compatible: " << n.compatible
      << ", connected: " << n.m_connected
      << ", api_reg_conf: " << n.m_api_reg_conf << ", alive: " << n.m_alive
      << ", nodefailrep: " << n.m_node_fail_rep
      << ", nfCompleteRep: " << n.nfCompleteRep
      << ", minDbVersion: " << n.minDbVersion
      << ", minApiVersion: " << n.minApiVersion << ", state: " << n.m_state
      << ", connected: "
      << BaseString::getPrettyTextShort(n.m_state.m_connected_nodes).c_str()
      << ", info: " << n.m_info << "]";

  return out;
}
