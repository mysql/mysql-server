/* Copyright (c) 2015, 2023, Oracle and/or its affiliates.

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

#ifndef NDBINFO_SCAN_VIRTUAL_HPP
#define NDBINFO_SCAN_VIRTUAL_HPP

#include <map>

#include "NdbInfo.hpp"
#include "NdbInfoScanOperation.hpp"

/*
  Scan implementation for retrieving rows from a virtual table. The table does
  not exist in the data nodes, instead it return hardcoded information or
  retrieves information from the cluster using NdbApi.
*/
class NdbInfoScanVirtual : public NdbInfoScanOperation {
public:
  int readTuples() override;

  const class NdbInfoRecAttr* getValue(const char * anAttrName) override;
  const class NdbInfoRecAttr* getValue(Uint32 anAttrId) override;
  int execute() override;
  int nextResult() override;
   ~NdbInfoScanVirtual() override;

  NdbInfoScanVirtual(Ndb_cluster_connection *connection,
                     const NdbInfo::Table *table,
                     const class VirtualTable *virt);
  int init();
  void initIndex(Uint32) override;
  bool seek(NdbInfoScanOperation::Seek, int) override;

  static bool create_virtual_tables(Vector<NdbInfo::Table*> &list);
  static void delete_virtual_tables(Vector<NdbInfo::Table*> &list);

private:
  enum State { Undefined, Initial, Prepared,
               MoreData, End } m_state;

  const NdbInfo::Table* const m_table;
  const class VirtualTable* const m_virt;

  friend class VirtualTable;
  NdbInfoRecAttrCollection m_recAttrs;

  char* m_buffer;
  size_t m_buffer_size;
  Uint32 m_row_counter; // Current row

  class VirtualScanContext* m_ctx;
  std::map<int, int>::const_iterator m_index_pos;
};

#endif
