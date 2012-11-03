/*
   Copyright (c) 2004, 2011, Oracle and/or its affiliates. All rights reserved.

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

#include <pc.hpp>
#define DBSPJ_C
#include "Dbspj.hpp"
#include <ndb_limits.h>

#define DEBUG(x) { ndbout << "SPJ::" << x << endl; }


Dbspj::Dbspj(Block_context& ctx, Uint32 instanceNumber):
  SimulatedBlock(DBSPJ, ctx, instanceNumber),
  m_scan_request_hash(m_request_pool),
  m_lookup_request_hash(m_request_pool),
  m_tableRecord(NULL), c_tabrecFilesize(0)
{
  BLOCK_CONSTRUCTOR(Dbspj);

  addRecSignal(GSN_DUMP_STATE_ORD, &Dbspj::execDUMP_STATE_ORD);
  addRecSignal(GSN_READ_NODESCONF, &Dbspj::execREAD_NODESCONF);
  addRecSignal(GSN_READ_CONFIG_REQ, &Dbspj::execREAD_CONFIG_REQ);
  addRecSignal(GSN_STTOR, &Dbspj::execSTTOR);
  addRecSignal(GSN_DBINFO_SCANREQ, &Dbspj::execDBINFO_SCANREQ);
  addRecSignal(GSN_CONTINUEB, &Dbspj::execCONTINUEB);
  addRecSignal(GSN_NODE_FAILREP, &Dbspj::execNODE_FAILREP);
  addRecSignal(GSN_INCL_NODEREQ, &Dbspj::execINCL_NODEREQ);
  addRecSignal(GSN_API_FAILREQ, &Dbspj::execAPI_FAILREQ);

  /**
   * Signals from DICT
   */
  addRecSignal(GSN_TC_SCHVERREQ, &Dbspj::execTC_SCHVERREQ);
  addRecSignal(GSN_TAB_COMMITREQ, &Dbspj::execTAB_COMMITREQ);
  addRecSignal(GSN_PREP_DROP_TAB_REQ, &Dbspj::execPREP_DROP_TAB_REQ);
  addRecSignal(GSN_DROP_TAB_REQ, &Dbspj::execDROP_TAB_REQ);
  addRecSignal(GSN_ALTER_TAB_REQ, &Dbspj::execALTER_TAB_REQ);

  /**
   * Signals from DIH
   */
  addRecSignal(GSN_DIH_SCAN_TAB_REF, &Dbspj::execDIH_SCAN_TAB_REF);
  addRecSignal(GSN_DIH_SCAN_TAB_CONF, &Dbspj::execDIH_SCAN_TAB_CONF);
  addRecSignal(GSN_DIH_SCAN_GET_NODES_REF, &Dbspj::execDIH_SCAN_GET_NODES_REF);
  addRecSignal(GSN_DIH_SCAN_GET_NODES_CONF,&Dbspj::execDIH_SCAN_GET_NODES_CONF);

  /**
   * Signals from TC
   */
  addRecSignal(GSN_LQHKEYREQ, &Dbspj::execLQHKEYREQ);
  addRecSignal(GSN_SCAN_FRAGREQ, &Dbspj::execSCAN_FRAGREQ);
  addRecSignal(GSN_SCAN_NEXTREQ, &Dbspj::execSCAN_NEXTREQ);

  /**
   * Signals from LQH
   */
  addRecSignal(GSN_LQHKEYREF, &Dbspj::execLQHKEYREF);
  addRecSignal(GSN_LQHKEYCONF, &Dbspj::execLQHKEYCONF);
  addRecSignal(GSN_SCAN_FRAGREF, &Dbspj::execSCAN_FRAGREF);
  addRecSignal(GSN_SCAN_FRAGCONF, &Dbspj::execSCAN_FRAGCONF);
  addRecSignal(GSN_TRANSID_AI, &Dbspj::execTRANSID_AI);
  addRecSignal(GSN_SCAN_HBREP, &Dbspj::execSCAN_HBREP);

  ndbout << "Instantiating DBSPJ instanceNo=" << instanceNumber << endl;
}//Dbspj::Dbspj()

Dbspj::~Dbspj()
{
  m_page_pool.clear();

  deallocRecord((void**)&m_tableRecord,
		"TableRecord",
		sizeof(TableRecord), 
		c_tabrecFilesize);
}//Dbspj::~Dbspj()


BLOCK_FUNCTIONS(Dbspj)

