/* Copyright (c) 2008, 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

#define DBTUP_C
#include "Dbtup.hpp"
#include "DbtupProxy.hpp"

#define JAM_FILE_ID 417


Dbtup_client::Dbtup_client(SimulatedBlock* block,
                           SimulatedBlock* dbtup)
  :m_jamBuf(block->jamBuffer())
{
  assert(m_jamBuf == getThrJamBuf());
  if (dbtup->isNdbMtLqh() && dbtup->instance() == 0) {
    m_dbtup_proxy = (DbtupProxy*)dbtup;
    m_dbtup = 0;
  } else {
    m_dbtup_proxy = 0;
    m_dbtup = (Dbtup*)dbtup;
  }
}

// LGMAN

void
Dbtup_client::disk_restart_undo(Signal* signal, Uint64 lsn,
                                Uint32 type, const Uint32 * ptr, Uint32 len)
{
  if (m_dbtup_proxy != 0) {
    m_dbtup_proxy->disk_restart_undo(signal, lsn, type, ptr, len);
    return;
  }
  m_dbtup->disk_restart_undo(signal, lsn, type, ptr, len);
}

// TSMAN

int
Dbtup_client::disk_restart_alloc_extent(Uint32 tableId, Uint32 fragId, 
                                        const Local_key* key, Uint32 pages)
{
  if (m_dbtup_proxy != 0) {
    return
      m_dbtup_proxy->disk_restart_alloc_extent(tableId, fragId, key, pages);
  }
  return m_dbtup->disk_restart_alloc_extent(m_jamBuf, tableId, 
                                            fragId, key, pages);
}

void
Dbtup_client::disk_restart_page_bits(Uint32 tableId, Uint32 fragId,
                                     const Local_key* key, Uint32 bits)
{
  if (m_dbtup_proxy != 0) {
    m_dbtup_proxy->disk_restart_page_bits(tableId, fragId, key, bits);
    return;
  }
  m_dbtup->disk_restart_page_bits(m_jamBuf, tableId, fragId, key, 
                                  bits);
}
