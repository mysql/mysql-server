/*
   Copyright (c) 2004, 2022, Oracle and/or its affiliates.

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

#ifndef NDB_WAITER_HPP
#define NDB_WAITER_HPP

#include <ndb_global.h>
#include "trp_client.hpp"

enum WaitSignalType { 
  NO_WAIT           = 0,
  WAIT_NODE_FAILURE = 1,  // Node failure during wait
  WST_WAIT_TIMEOUT  = 2,  // Timeout during wait

  WAIT_TC_SEIZE     = 3,
  WAIT_TC_RELEASE   = 4,
  WAIT_NDB_TAMPER   = 5,
  WAIT_SCAN         = 6,
  WAIT_TRANS        = 7,
  WAIT_EVENT        = 8,

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
  explicit NdbWaiter(trp_client* clnt)
    : m_clnt(clnt), m_node(0), m_state(NO_WAIT)
  {}

  void signal(Uint32 state);
  void nodeFail(Uint32 node);

  void set_state(Uint32 state) { m_state= state; }
  Uint32 get_state() const { return m_state; }

  void set_node(Uint32 node) { m_node= node; }

private:
  trp_client* const m_clnt;
  Uint32 m_node;
  Uint32 m_state;
};


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
