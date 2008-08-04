/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "DbtupProxy.hpp"
#include "Dbtup.hpp"

DbtupProxy::DbtupProxy(Block_context& ctx) :
  LocalProxy(DBTUP, ctx)
{
  addRecSignal(GSN_SEND_PACKED, &DbtupProxy::execSEND_PACKED);
}

DbtupProxy::~DbtupProxy()
{
}

SimulatedBlock*
DbtupProxy::newWorker(Uint32 instanceNo)
{
  return new Dbtup(m_ctx, 0, instanceNo);
}

// GSN_SEND_PACKED

void
DbtupProxy::execSEND_PACKED(Signal* signal)
{
  Uint32 i;
  for (i = 0; i < c_workers; i++) {
    Dbtup* dbtup = static_cast<Dbtup*>(c_worker[i]);
    dbtup->execSEND_PACKED(signal);
  }
}

BLOCK_FUNCTIONS(DbtupProxy)
