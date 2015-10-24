/* Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

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

#ifndef NDBINFO_SCAN_VIRTUAL_HPP
#define NDBINFO_SCAN_VIRTUAL_HPP

#include "NdbInfo.hpp"
#include "NdbInfoScanOperation.hpp"

/*
  Scan implementation for retrieving rows from a virtual
  table (also known as "hardcoded").
*/
class NdbInfoScanVirtual : public NdbInfoScanOperation {
public:
  virtual int readTuples();

  virtual const class NdbInfoRecAttr* getValue(const char * anAttrName);
  virtual const class NdbInfoRecAttr* getValue(Uint32 anAttrId);
  virtual int execute();
  virtual int nextResult();
  virtual  ~NdbInfoScanVirtual();

  NdbInfoScanVirtual(const NdbInfo::Table* table,
                     const class VirtualTable* virt);
  int init();

  static bool create_virtual_tables(Vector<NdbInfo::Table*>& list);
  static void delete_virtual_tables(Vector<NdbInfo::Table*>& list);
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
};

#endif
