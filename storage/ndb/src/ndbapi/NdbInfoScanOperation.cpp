/*
   Copyright (c) 2009, 2011, Oracle and/or its affiliates. All rights reserved.

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

#include "NdbInfo.hpp"
#include "SignalSender.hpp"
#include <kernel/GlobalSignalNumbers.h>
#include <AttributeHeader.hpp>
#include <signaldata/DbinfoScan.hpp>
#include <signaldata/TransIdAI.hpp>
#include <signaldata/NodeFailRep.hpp>

struct NdbInfoScanOperationImpl
{
  NodeBitmask m_nodes_to_scan;
};

NdbInfoScanOperation::NdbInfoScanOperation(const NdbInfo& info,
                                           Ndb_cluster_connection* connection,
                                           const NdbInfo::Table* table,
                                           Uint32 max_rows, Uint32 max_bytes) :
  m_impl(*new NdbInfoScanOperationImpl),
  m_info(info),
  m_state(Undefined),
  m_connection(connection),
  m_signal_sender(NULL),
  m_table(table),
  m_node_id(0),
  m_max_rows(max_rows),
  m_max_bytes(max_bytes),
  m_result_data(0x37),
  m_rows_received(0),
  m_rows_confirmed(0),
  m_nodes(0),
  m_max_nodes(0)
{
}

bool
NdbInfoScanOperation::init(Uint32 id)
{
  DBUG_ENTER("NdbInfoScanoperation::init");
  if (m_state != Undefined)
    DBUG_RETURN(false);

  m_signal_sender = new SignalSender(m_connection);
  if (!m_signal_sender)
    DBUG_RETURN(false);

  m_transid0 = id;
  m_transid1 = m_table->getTableId();
  m_result_ref = m_signal_sender->getOwnRef();

  for (unsigned i = 0; i < m_table->columns(); i++)
    m_recAttrs.push_back(NULL);

  /*
    Build a bitmask of nodes that will be scanned if
    connected and have been API_REGCONFed. Don't include
    own node since it will always be "connected"
  */
  for (Uint32 i = 1; i < MAX_NDB_NODES; i++)
    m_impl.m_nodes_to_scan.set(i);
  m_impl.m_nodes_to_scan.clear(refToNode(m_result_ref));

  m_state = Initial;
  DBUG_RETURN(true);

}

NdbInfoScanOperation::~NdbInfoScanOperation()
{
  close();
  delete m_signal_sender;
  delete &m_impl;
}

int
NdbInfoScanOperation::readTuples()
{
  if (m_state != Initial)
    return NdbInfo::ERR_WrongState;

  m_state = Prepared;
  return 0;
}

const NdbInfoRecAttr *
NdbInfoScanOperation::getValue(const char * anAttrName)
{
  if (m_state != Prepared)
    return NULL;

  const NdbInfo::Column* column = m_table->getColumn(anAttrName);
  if (!column)
    return NULL;
  return getValue(column->m_column_id);
}

const NdbInfoRecAttr *
NdbInfoScanOperation::getValue(Uint32 anAttrId)
{
  if (m_state != Prepared)
    return NULL;

  if (anAttrId >= m_recAttrs.size())
    return NULL;

  NdbInfoRecAttr *recAttr = new NdbInfoRecAttr;
  m_recAttrs[anAttrId] = recAttr;
  return recAttr;
}


bool
NdbInfoScanOperation::find_next_node()
{
  DBUG_ENTER("NdbInfoScanOperation::find_next_node");

  const NodeId next =
    m_signal_sender->find_confirmed_node(m_impl.m_nodes_to_scan);
  if (next == 0)
  {
    DBUG_PRINT("info", ("no more alive nodes"));
    DBUG_RETURN(false);
  }
  assert(m_node_id != next);
  m_impl.m_nodes_to_scan.clear(next);
  m_node_id = next;
  m_nodes++;

  // Check if number of nodes to scan is limited
  DBUG_PRINT("info", ("nodes: %d, max_nodes: %d", m_nodes, m_max_nodes));
  if (m_max_nodes && m_nodes > m_max_nodes)
  {
    DBUG_PRINT("info", ("Reached max nodes to scan"));
    DBUG_RETURN(false);
  }

  DBUG_PRINT("info", ("switched to node %d", m_node_id));
  DBUG_RETURN(true);
}


int NdbInfoScanOperation::execute()
{
  DBUG_ENTER("NdbInfoScanOperation::execute");
  DBUG_PRINT("info", ("name: '%s', id: %d",
             m_table->getName(), m_table->getTableId()));

  if (m_state != Prepared)
    DBUG_RETURN(NdbInfo::ERR_WrongState);

  assert(m_cursor.size() == 0);
  m_state = MoreData;

  m_signal_sender->lock();

  if (!find_next_node())
  {
    m_signal_sender->unlock();
    DBUG_RETURN(NdbInfo::ERR_ClusterFailure);
  }

  int ret = sendDBINFO_SCANREQ();
  m_signal_sender->unlock();

  DBUG_RETURN(ret);
}

int
NdbInfoScanOperation::sendDBINFO_SCANREQ(void)
{
  DBUG_ENTER("NdbInfoScanOperation::sendDBINFO_SCANREQ");

  SimpleSignal ss;
  DbinfoScanReq * req = CAST_PTR(DbinfoScanReq, ss.getDataPtrSend());

  // API Identifiers
  req->resultData = m_result_data;
  req->transId[0] = m_transid0;
  req->transId[1] = m_transid1;
  req->resultRef = m_result_ref;

  // Scan parameters
  req->tableId = m_table->getTableId();
  req->colBitmap[0] = ~0;
  req->colBitmap[1] = ~0;
  req->requestInfo = 0;
  req->maxRows = m_max_rows;
  req->maxBytes = m_max_bytes;
  DBUG_PRINT("info", ("max rows: %d, max bytes: %d", m_max_rows, m_max_bytes));

  // Scan result
  req->returnedRows = 0;

  // Cursor data
  Uint32* cursor_ptr = DbinfoScan::getCursorPtrSend(req);
  for (unsigned i = 0; i < m_cursor.size(); i++)
  {
    *cursor_ptr = m_cursor[i];
    DBUG_PRINT("info", ("cursor[%u]: 0x%x", i, m_cursor[i]));
    cursor_ptr++;
  }
  req->cursor_sz = m_cursor.size();
  m_cursor.clear();

  assert((m_rows_received == 0 && m_rows_confirmed == (Uint32)~0) || // first
         m_rows_received == m_rows_confirmed);                       // subsequent

  // No rows recieved in this batch yet
  m_rows_received = 0;

  // Number of rows returned by batch is not yet known
  m_rows_confirmed = ~0;

  assert(m_node_id);
  Uint32 len = DbinfoScanReq::SignalLength + req->cursor_sz;
  if (m_signal_sender->sendSignal(m_node_id, ss, DBINFO,
                                  GSN_DBINFO_SCANREQ, len) != SEND_OK)
  {
    m_state = Error;
    DBUG_RETURN(NdbInfo::ERR_ClusterFailure);
  }

  DBUG_RETURN(0);
}

int NdbInfoScanOperation::receive(void)
{
  DBUG_ENTER("NdbInfoScanOperation::receive");
  while (true)
  {
    const SimpleSignal* sig = m_signal_sender->waitFor();
    if (!sig)
      DBUG_RETURN(-1);
    //sig->print();

    int sig_number = sig->readSignalNumber();
    switch (sig_number) {

    case GSN_DBINFO_TRANSID_AI:
    {
      if (execDBINFO_TRANSID_AI(sig))
        continue;  // Wait for next signal

      if (m_rows_received < m_rows_confirmed)
        DBUG_RETURN(1); // Row available

      // All rows in this batch recieved
      assert(m_rows_received == m_rows_confirmed);

      if (m_cursor.size() == 0 && !find_next_node())
      {
        DBUG_PRINT("info", ("No cursor -> EOF"));
        m_state = End;
        DBUG_RETURN(1); // Row available(will get End on next 'nextResult')
      }

      // Cursor is still set, fetch more rows
      assert(m_state == MoreData);
      int err = sendDBINFO_SCANREQ();
      if (err != 0)
      {
        DBUG_PRINT("error", ("Failed to request more data"));
        assert(m_state == Error);
        // Return error immediately
        DBUG_RETURN(err);
      }

      DBUG_RETURN(1); // Row available
      break;
    }

    case GSN_DBINFO_SCANCONF:
    {
      if (execDBINFO_SCANCONF(sig))
        continue; // Wait for next signal

      if (m_rows_received < m_rows_confirmed)
        continue;  // Continue waiting(for late TRANSID_AI signals)

      // All rows in this batch recieved
      assert(m_rows_received == m_rows_confirmed);

      if (m_cursor.size() == 0 && !find_next_node())
      {
        DBUG_PRINT("info", ("No cursor -> EOF"));
        m_state = End;
        DBUG_RETURN(0); // No more rows
      }

      // Cursor is still set, fetch more rows
      assert(m_state == MoreData);
      int err = sendDBINFO_SCANREQ();
      if (err != 0)
      {
        DBUG_PRINT("error", ("Failed to request more data"));
        assert(m_state == Error);
        DBUG_RETURN(err);
      }

      continue;
    }

    case GSN_DBINFO_SCANREF:
    {
      int error;
      if (execDBINFO_SCANREF(sig, error))
        continue; // Wait for next signal
      assert(m_state == Error);
      DBUG_RETURN(error);
      break;
    }

    case GSN_NODE_FAILREP:
    {
      const NodeFailRep * const rep =
        CAST_CONSTPTR(NodeFailRep, sig->getDataPtr());
      if (NdbNodeBitmask::get(rep->theNodes, m_node_id))
      {
        DBUG_PRINT("info", ("Node %d where scan was runnig failed", m_node_id));
        m_state = Error;
        DBUG_RETURN(NdbInfo::ERR_ClusterFailure);
      }
      break;
    }

    case GSN_NF_COMPLETEREP:
      // Already handled in NODE_FAILREP
      break;

    case GSN_SUB_GCP_COMPLETE_REP:
    case GSN_API_REGCONF:
    case GSN_TAKE_OVERTCCONF:
    case GSN_CONNECT_REP:
      // ignore
      break;

    default:
      DBUG_PRINT("error", ("Got unexpected signal: %d", sig_number));
      assert(false);
      break;
    }
  }
  assert(false); // Should never come here
  DBUG_RETURN(-1);
}

int
NdbInfoScanOperation::nextResult()
{
  DBUG_ENTER("NdbInfoScanOperation::nextResult");

  switch(m_state)
  {
  case MoreData:
  {
    m_signal_sender->lock();
    int ret = receive();
    m_signal_sender->unlock();
    DBUG_RETURN(ret);
    break;
  }
  case End:
    DBUG_RETURN(0); // EOF
    break;
  default:
    break;
  }
  DBUG_RETURN(-1);
}

void
NdbInfoScanOperation::close()
{
  DBUG_ENTER("NdbInfoScanOperation::close");

  for (unsigned i = 0; i < m_recAttrs.size(); i++)
  {
    if (m_recAttrs[i])
    {
      delete m_recAttrs[i];
      m_recAttrs[i] = NULL;
    }
  }

  DBUG_VOID_RETURN;
}

bool
NdbInfoScanOperation::execDBINFO_TRANSID_AI(const SimpleSignal * signal)
{
  DBUG_ENTER("NdbInfoScanOperation::execDBINFO_TRANSID_AI");
  const TransIdAI* transid =
          CAST_CONSTPTR(TransIdAI, signal->getDataPtr());
  if (transid->connectPtr != m_result_data ||
      transid->transId[0] != m_transid0 ||
      transid->transId[1] != m_transid1)
  {
    // Drop signal that belongs to previous scan
    DBUG_RETURN(true); // Continue waiting
  }

  m_rows_received++;
  DBUG_PRINT("info", ("rows received: %d", m_rows_received));

  // Reset all recattr values before reading the new row
  for (unsigned i = 0; i < m_recAttrs.size(); i++)
  {
    if (m_recAttrs[i])
      m_recAttrs[i]->m_defined = false;
  }

  // Read attributes from long signal section
  AttributeHeader* attr = (AttributeHeader*)signal->ptr[0].p;
  AttributeHeader* last = (AttributeHeader*)(signal->ptr[0].p +
                                            signal->ptr[0].sz);
  while (attr < last)
  {
    const Uint32 col = attr->getAttributeId();
    const Uint32 len = attr->getByteSize();
    DBUG_PRINT("info", ("col: %u, len: %u", col, len));
    if (col < m_recAttrs.size())
    {
      NdbInfoRecAttr* rec_attr = m_recAttrs[col];
      if (rec_attr)
      {
        // Update NdbInfoRecAttr pointer, length and defined flag
        rec_attr->m_data = (const char*)attr->getDataPtr();
        rec_attr->m_len = len;
        rec_attr->m_defined = true;
      }
    }

    attr = attr->getNext();
  }

  DBUG_RETURN(false); // Don't wait more, process this row
}

bool
NdbInfoScanOperation::execDBINFO_SCANCONF(const SimpleSignal * sig)
{
  DBUG_ENTER("NdbInfoScanOperation::execDBINFO_SCANCONF");
  const DbinfoScanConf* conf =
          CAST_CONSTPTR(DbinfoScanConf, sig->getDataPtr());
  if (conf->resultData != m_result_data ||
      conf->transId[0] != m_transid0 ||
      conf->transId[1] != m_transid1 ||
      conf->resultRef != m_result_ref)
  {
    // Drop signal that belongs to previous scan
    DBUG_RETURN(true); // Continue waiting
  }
  const Uint32 tableId = conf->tableId;
  assert(tableId == m_table->getTableId());

  // Assert all scan settings is unchanged
  assert(conf->colBitmap[0] == (Uint32)~0);
  assert(conf->colBitmap[1] == (Uint32)~0);
  assert(conf->requestInfo == 0);
  assert(conf->maxRows == m_max_rows);
  assert(conf->maxBytes == m_max_bytes);

  DBUG_PRINT("info", ("returnedRows : %d", conf->returnedRows));

  // Save cursor data
  DBUG_PRINT("info", ("cursor size: %d", conf->cursor_sz));
  assert(m_cursor.size() == 0);
  const Uint32* cursor_ptr = DbinfoScan::getCursorPtr(conf);
  for (unsigned i = 0; i < conf->cursor_sz; i++)
  {
    m_cursor.push_back(*cursor_ptr);
    //DBUG_PRINT("info", ("cursor[%u]: 0x%x", i, m_cursor[i]));
    cursor_ptr++;
  }
  assert(conf->cursor_sz == m_cursor.size());

  assert(m_rows_confirmed == (Uint32)~0); // Should've been unknown until now
  m_rows_confirmed = conf->returnedRows;

  // Don't allow confirmation of less rows than already been received
  DBUG_PRINT("info", ("received: %d, confirmed: %d", m_rows_received, m_rows_confirmed));
  assert(m_rows_received <= m_rows_confirmed);

  DBUG_RETURN(false);
}

bool
NdbInfoScanOperation::execDBINFO_SCANREF(const SimpleSignal * signal,
                                         int& error_code)
{
  DBUG_ENTER("NdbInfoScanOperation::execDBINFO_SCANREF");
  const DbinfoScanRef* ref =
          CAST_CONSTPTR(DbinfoScanRef, signal->getDataPtr());

  if (ref->resultData != m_result_data ||
      ref->transId[0] != m_transid0 ||
      ref->transId[1] != m_transid1 ||
      ref->resultRef != m_result_ref)
  {
    // Drop signal that belongs to previous scan
    DBUG_RETURN(true); // Continue waiting
  }

  error_code = ref->errorCode;

  m_state = Error;
  DBUG_RETURN(false);
}


template class Vector<NdbInfoRecAttr*>;
