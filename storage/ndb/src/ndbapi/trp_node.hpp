/*
   Copyright (c) 2010, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef trp_node_hpp
#define trp_node_hpp

#include <ndb_global.h>
#include <kernel/NodeInfo.hpp>
#include <kernel/NodeState.hpp>

class NdbOut;
NdbOut &operator<<(NdbOut &, const struct trp_node &);

struct trp_node {
  NodeInfo m_info;
  NodeState m_state{NodeState::SL_NOTHING};

  Uint32 minDbVersion = 0;
  Uint32 minApiVersion = 0;
  bool defined = false;
  bool compatible = true;        // Version is compatible
  bool nfCompleteRep = true;     // NF Complete Rep has arrived
  bool m_alive = false;          // Node is alive
  bool m_node_fail_rep = false;  // NodeFailRep has arrived
 private:
  bool m_connected = false;     // Transporter connected
  bool m_api_reg_conf = false;  // API_REGCONF has arrived
 public:
  void set_connected(bool connected) {
    assert(defined);
    m_connected = connected;
  }
  bool is_connected(void) const {
    const bool connected = m_connected;
    // Must be defined if connected
    assert(!connected || (connected && defined));
    return connected;
  }

  void set_confirmed(bool confirmed) {
    if (confirmed) assert(is_connected());
    m_api_reg_conf = confirmed;
  }

  bool is_confirmed(void) const {
    const bool confirmed = m_api_reg_conf;
    assert(!confirmed || (confirmed && is_connected()));
    return confirmed;
  }

 private:
  friend NdbOut &operator<<(NdbOut &, const trp_node &);
};

#endif
