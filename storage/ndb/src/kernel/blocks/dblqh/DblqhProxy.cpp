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

#include "DblqhProxy.hpp"
#include "Dblqh.hpp"

DblqhProxy::DblqhProxy(Block_context& ctx) :
  LocalProxy(DBLQH, ctx)
{
  // GSN_SEND_PACKED
  addRecSignal(GSN_SEND_PACKED, &DblqhProxy::execSEND_PACKED);
}

DblqhProxy::~DblqhProxy()
{
}

SimulatedBlock*
DblqhProxy::newWorker(Uint32 instanceNo)
{
  return new Dblqh(m_ctx, instanceNo);
}

// GSN_SEND_PACKED

void
DblqhProxy::execSEND_PACKED(Signal* signal)
{
  Uint32 i;
  for (i = 0; i < c_workers; i++) {
    ndbrequire(c_worker[i] != 0);
    Dblqh* dblqh = static_cast<Dblqh*>(c_worker[i]);
    dblqh->execSEND_PACKED(signal);
  }
}

BLOCK_FUNCTIONS(DblqhProxy)
