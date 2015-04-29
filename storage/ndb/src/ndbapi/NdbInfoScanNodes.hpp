/*
   Copyright (c) 2009, 2015, Oracle and/or its affiliates. All rights reserved.

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

#ifndef NDBINFO_SCAN_NODES_H
#define NDBINFO_SCAN_NODES_H

#include "NdbInfoScanOperation.hpp"

#include "kernel/NodeBitmask.hpp"

/*
  Scan implementation for retrieving rows from the NDB
  data nodes.
*/
class NdbInfoScanNodes : public NdbInfoScanOperation {
public:
  virtual int readTuples();
  virtual const class NdbInfoRecAttr* getValue(const char * anAttrName);
  virtual const class NdbInfoRecAttr* getValue(Uint32 anAttrId);
  virtual int execute();
  virtual int nextResult();

  NdbInfoScanNodes(const NdbInfo&,
                   class Ndb_cluster_connection*,
                   const NdbInfo::Table*,
                   Uint32 max_rows, Uint32 max_bytes,
                   Uint32 max_nodes);
  int init(Uint32 id);

  virtual ~NdbInfoScanNodes();
private:
  bool execDBINFO_TRANSID_AI(const struct SimpleSignal * signal);
  bool execDBINFO_SCANCONF(const struct SimpleSignal * signal);
  bool execDBINFO_SCANREF(const struct SimpleSignal * signal, int& error_code);
  int sendDBINFO_SCANREQ();

  int receive(void);
  bool find_next_node();

  const NdbInfo& m_info;
  enum State { Undefined, Initial, Prepared,
               MoreData, End, Error } m_state;
  class Ndb_cluster_connection* m_connection;
  class SignalSender*           m_signal_sender;
  const NdbInfo::Table*     m_table;
  NdbInfoRecAttrCollection m_recAttrs;
  Vector<Uint32>                m_cursor;
  Uint32 m_node_id;
  Uint32 m_transid0;
  Uint32 m_transid1;
  Uint32 m_result_ref;
  Uint32 m_max_rows;
  Uint32 m_max_bytes;
  Uint32 m_result_data;
  Uint32 m_rows_received;
  Uint32 m_rows_confirmed;
  Uint32 m_nodes; // Number of nodes scanned
  const Uint32 m_max_nodes; // Max number of nodes to scan
  NodeBitmask m_nodes_to_scan;
};


#endif
