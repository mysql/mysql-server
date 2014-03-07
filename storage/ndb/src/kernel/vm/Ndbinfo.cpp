/*
   Copyright (c) 2009, 2013, Oracle and/or its affiliates. All rights reserved.

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

#include "Ndbinfo.hpp"
#include "SimulatedBlock.hpp"
#include <kernel/AttributeHeader.hpp>
#include <signaldata/TransIdAI.hpp>

#define JAM_FILE_ID 326


Ndbinfo::Row::Row(Signal* signal, DbinfoScanReq& req) :
  col_counter(0),
  m_req(req)
{
  // Use the "temporary" part of signal->theData as row buffer
  start = signal->getDataPtrSend() + DbinfoScanReq::SignalLength;
  const Uint32 data_sz = sizeof(signal->theData)/sizeof(signal->theData[0]);
  end = signal->getDataPtrSend() + data_sz;
  assert(start < end);

  curr = start;
}

bool
Ndbinfo::Row::check_buffer_space(AttributeHeader& ah) const
{
  const Uint32 needed =  ah.getHeaderSize() + ah.getDataSize();
  const Uint32 avail = (Uint32)(end - curr);

  if(needed > avail)
  {
    ndbout_c("Warning, too small row buffer for attribute: %d, "
             "needed: %d, avail: %d", ah.getAttributeId(), needed, avail);
    assert(false);
    return false; // Not enough room in row buffer
  }
  return true;
}

void
Ndbinfo::Row::check_attribute_type(AttributeHeader& ah, ColumnType type) const
{
#ifdef VM_TRACE
  const Table& tab = getTable(m_req.tableId);
  const Uint32 colid = ah.getAttributeId();
  assert(colid < (Uint32)tab.m.ncols);
  assert(tab.col[colid].coltype == type);
#endif
}

void
Ndbinfo::Row::write_string(const char* str)
{
  const size_t clen = strlen(str) + 1;
  // Create AttributeHeader
  AttributeHeader ah(col_counter++, (Uint32)clen);
  check_attribute_type(ah, Ndbinfo::String);
  if (!check_buffer_space(ah))
    return;

  // Write AttributeHeader to buffer
  ah.insertHeader(curr);
  curr += ah.getHeaderSize();

  // Write data to buffer
  memcpy(curr, str, clen);
  curr += ah.getDataSize();

  assert(curr <= end);
  return;
}

void
Ndbinfo::Row::write_uint32(Uint32 value)
{
  // Create AttributeHeader
  AttributeHeader ah(col_counter++, sizeof(Uint32));
  check_attribute_type(ah, Ndbinfo::Number);
  if (!check_buffer_space(ah))
    return;

  // Write AttributeHeader to buffer
  ah.insertHeader(curr);
  curr += ah.getHeaderSize();

  // Write data to buffer
  memcpy(curr, &value, sizeof(Uint32));
  curr += ah.getDataSize();

  assert(curr <= end);
  return;
}

void
Ndbinfo::Row::write_uint64(Uint64 value)
{
  // Create AttributeHeader
  AttributeHeader ah(col_counter++, sizeof(Uint64));
  check_attribute_type(ah, Ndbinfo::Number64);
  if (!check_buffer_space(ah))
    return;

  // Write AttributeHeader to buffer
  ah.insertHeader(curr);
  curr += ah.getHeaderSize();

  // Write data to buffer
  memcpy(curr, &value, sizeof(Uint64));
  curr += ah.getDataSize();

  assert(curr <= end);
  return;
}


