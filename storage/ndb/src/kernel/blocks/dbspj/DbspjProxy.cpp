/*
  Copyright (c) 2000, 2012, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "DbspjProxy.hpp"
#include "Dbspj.hpp"

DbspjProxy::DbspjProxy(Block_context& ctx) :
  DbgdmProxy(DBSPJ, ctx)
{}

DbspjProxy::~DbspjProxy()
{
}

SimulatedBlock*
DbspjProxy::newWorker(Uint32 instanceNo)
{
  ndbout << "Instantiating Dbspj worker no " << instanceNo << endl;
  return new Dbspj(m_ctx, instanceNo);
}


BLOCK_FUNCTIONS(DbspjProxy)
