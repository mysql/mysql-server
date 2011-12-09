/*
   Copyright (c) 2004, 2010, Oracle and/or its affiliates. All rights reserved.

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

#ifndef NDB_WAITER_HPP
#define NDB_WAITER_HPP

#include <ndb_global.h>
#include <NdbTick.h>
#include <NdbOut.hpp>

enum WaitSignalType { 
  NO_WAIT           = 0,
  WAIT_NODE_FAILURE = 1,  // Node failure during wait
  WST_WAIT_TIMEOUT  = 2,  // Timeout during wait

  WAIT_TC_SEIZE     = 3,
  WAIT_TC_RELEASE   = 4,
  WAIT_NDB_TAMPER   = 5,
  WAIT_SCAN         = 6,

  WAIT_TRANS        = 7,

  // DICT stuff
  WAIT_GET_TAB_INFO_REQ = 11,
  WAIT_CREATE_TAB_REQ = 12,
  WAIT_DROP_TAB_REQ = 13,
  WAIT_ALTER_TAB_REQ = 14,
  WAIT_CREATE_INDX_REQ = 15,
  WAIT_DROP_INDX_REQ = 16,
  WAIT_LIST_TABLES_CONF = 17,
  WAIT_SCHEMA_TRANS = 18
};

class NdbWaiter {
public:
  NdbWaiter(class trp_client*);
  ~NdbWaiter();

  void signal(Uint32 state);
  void nodeFail(Uint32 node);

  void clear_wait_state() { m_state = NO_WAIT; }
  Uint32 get_wait_state() { return m_state; }
  void set_wait_state(Uint32 s) { m_state = s;}

  void set_state(Uint32 state) { m_state= state; }
  void set_node(Uint32 node) { m_node= node; }
  Uint32 get_state() { return m_state; }
private:

  Uint32 m_node;
  Uint32 m_state;
  class trp_client* m_clnt;
};


#include "trp_client.hpp"

inline
void
NdbWaiter::nodeFail(Uint32 aNodeId)
{
  if (m_state != NO_WAIT && m_node == aNodeId)
  {
    m_state = WAIT_NODE_FAILURE;
    m_clnt->wakeup();
  }
}

inline
void 
NdbWaiter::signal(Uint32 state)
{
  m_state = state;
  m_clnt->wakeup();
}

#endif
