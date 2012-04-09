/*
   Copyright 2009, 2010 Sun Microsystems, Inc.
   All rights reserved. Use is subject to license terms.

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

#ifndef NDBINFOSCANOPERATION_H
#define NDBINFOSCANOPERATION_H

class NdbInfoScanOperation {
public:
  int readTuples();
  const class NdbInfoRecAttr* getValue(const char * anAttrName);
  const class NdbInfoRecAttr* getValue(Uint32 anAttrId);
  int execute();
  int nextResult();
protected:
  friend class NdbInfo;
  NdbInfoScanOperation(const NdbInfo&,
                       class Ndb_cluster_connection*,
                       const NdbInfo::Table*,
                       Uint32 max_rows, Uint32 max_bytes);
  bool init(Uint32 id);
  ~NdbInfoScanOperation();
  void close();
private:
  bool execDBINFO_TRANSID_AI(const struct SimpleSignal * signal);
  bool execDBINFO_SCANCONF(const struct SimpleSignal * signal);
  bool execDBINFO_SCANREF(const struct SimpleSignal * signal, int& error_code);
  int sendDBINFO_SCANREQ();

  int receive(void);
  bool find_next_node();

  struct NdbInfoScanOperationImpl& m_impl;
  const NdbInfo& m_info;
  enum State { Undefined, Initial, Prepared,
               MoreData, End, Error } m_state;
  class Ndb_cluster_connection* m_connection;
  class SignalSender*           m_signal_sender;
  const NdbInfo::Table*     m_table;
  Vector<NdbInfoRecAttr*>       m_recAttrs;
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
  Uint32 m_max_nodes; // Max number of nodes to scan
};


#endif
